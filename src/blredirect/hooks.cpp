#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <MinHook.h>
#include <libhat.hpp>

#include <atomic>
#include <string>
#include <unordered_map>

#include "blredirect/engine.h"
#include "blredirect/hooks.h"
#include "blredirect/logging.h"
#include "blredirect/unreal_types.h"

namespace blredirect {

namespace {

using ue3::FObjectExport;
using ue3::UClass;
using ue3::ULinkerLoad;
using ue3::UObject;

////////////////////////////////////////////////////////////////////////////////
// | GLOBALS |
////////////////////////////////////////////////////////////////////////////////

// UObject* ULinkerLoad::CreateExport
constexpr auto CREATE_EXPORT_SIG = hat::compile_signature<
    " 6A FF"                 // push -1
    " 68 ?? ?? ?? ??"        // push handler
    " 64 A1 00 00 00 00"     // mov eax, fs:[0]
    " 50"                    // push eax
    " 83 EC 6C"              // sub esp, 0x6c
    " 53 55 56 57"           // push ebx; push ebp; push esi; push edi
    " A1 ?? ?? ?? ??"        // mov eax, [security_cookie]
    " 33 C4"                 // xor eax, esp
    " 50"                    // push eax
    " 8D 84 24 80 00 00 00"  // lea eax, [esp + 0x80]
    " 64 A3 00 00 00 00"     // mov fs:[0], eax
    " 8B F9"                 // mov edi, ecx (this)
    " 8B 84 24 90 00 00 00"  // mov eax, [esp + 0x90] (Index)
    " 33 DB"                 // xor ebx, ebx
    " 89 5C 24 38"           // mov [esp + 0x38], ebx
    " 89 3D ?? ?? ?? ??"     // mov [GSerializedExportLinker], edi
    " A3"                    // mov [GSerializedExportIndex], eax
    >();

using CreateExportFn = UObject*(__thiscall*)(ULinkerLoad * self, int32_t index);

using IsDebuggerPresentFn = BOOL(WINAPI*)();

CreateExportFn fn_create_export = nullptr;
IsDebuggerPresentFn fn_is_debugger_present = nullptr;
std::atomic hooks_installed{false};

// NOLINTNEXTLINE(*-throwing-static-initialization)
std::unordered_map<uint64_t, bool> mod_packages;

////////////////////////////////////////////////////////////////////////////////
// | Package Filter |
////////////////////////////////////////////////////////////////////////////////

// HACK - since if we redirect every load we will hit the memory limit quite quickly and
//  crash. So instead we only redirect an export if the package file itself is not cooked which
//  means it has been modified (saved with the editor).
bool is_mod_package(ULinkerLoad* linker, const FObjectExport& exp) {
    int32_t top_index = exp.OuterIndex - 1;
    const FObjectExport* top = linker->ExportMap.at(top_index);
    while (top != nullptr && top->OuterIndex > 0) {
        top_index = top->OuterIndex - 1;
        top = linker->ExportMap.at(top_index);
    }

    if (top == nullptr || top->OuterIndex != 0) {
        return false;
    }

    const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(top->ObjectName.Index)) << 32)
                         | static_cast<uint32_t>(top->ObjectName.Number);

    auto [it, inserted] = mod_packages.try_emplace(key, false);

    if (inserted) {
        constexpr uint32_t PKG_Cooked = 0x8;
        const std::wstring package = ue3::get_export_path_name(linker, top_index, nullptr, true);
        const std::wstring file = ue3::find_package_file(package.c_str());
        const auto flags = file.empty() ? std::nullopt : ue3::read_package_flags(file);

        it->second = flags.has_value() && (*flags & PKG_Cooked) == 0;
        if (it->second) {
            BLREDIRECT_LOG("{} is a mod package, redirecting to: {}", narrow(package), narrow(file));
        }
    }

    return it->second;
}

////////////////////////////////////////////////////////////////////////////////
// | Load/Find Export |
////////////////////////////////////////////////////////////////////////////////

UObject* load_from_package(ULinkerLoad* linker, int32_t index, const FObjectExport& exp) {
    const FObjectExport* outer_export = linker->ExportMap.at(exp.OuterIndex - 1);
    if (outer_export == nullptr) {
        return nullptr;
    }

    if (outer_export->OuterIndex != 0) {
        UObject* group = ue3::index_to_object(linker, exp.OuterIndex);
        const bool group_is_cooked_copy = group != nullptr
                                          && group->Linker != nullptr
                                          && group->Linker->LinkerRoot != ue3::get_outermost(group);

        if (group == nullptr || group_is_cooked_copy) {
            return nullptr;
        }
    }

    auto* cls = reinterpret_cast<UClass*>(ue3::index_to_object(linker, exp.ClassIndex));
    if (cls == nullptr) {
        return nullptr;
    }

    constexpr uint32_t LOAD_NoWarn = 0x2;
    const std::wstring path = ue3::get_export_path_name(linker, index, nullptr, true);
    return ue3::static_load_object(cls, nullptr, path.c_str(), nullptr, LOAD_NoWarn);
}

////////////////////////////////////////////////////////////////////////////////
// | CreateExport Hook |
////////////////////////////////////////////////////////////////////////////////

UObject* __fastcall create_export_hook(ULinkerLoad* self, void* /*edx*/, int32_t index) {
    constexpr uint32_t EF_ForcedExport = 0x1;
    const FObjectExport* exp = self->ExportMap.at(index);

    // Just as a quick note a Forced Export is an object that has been copied in its entirety to
    // another package. The engine loads it under the same name but loads it from the cooked package
    // instead of from the package it was taken from. The startup file is an example of a package
    // with a lot of forced exports but its any cooked package even script packages can have forced
    // exports.

    // We only care about forced exports everything else is fine to fall through as normal. This
    // also handles the function being called recursively problem. Only forced exports of packages
    // a mod replaced load from their stored package, everything else uses the cooked copy.
    if (
        exp == nullptr
        || exp->Object != nullptr
        || (exp->ExportFlags & EF_ForcedExport) == 0
        || exp->OuterIndex <= 0
        || exp->ClassIndex == 0
        || !is_mod_package(self, *exp)
    ) {
        return fn_create_export(self, index);
    }

    UObject* obj = load_from_package(self, index, *exp);
    if (obj != nullptr) {
        return obj;
    }

    return fn_create_export(self, index);
}

////////////////////////////////////////////////////////////////////////////////
// | ENTRY |
////////////////////////////////////////////////////////////////////////////////

bool try_install() {
    const auto create_export = hat::find_pattern(CREATE_EXPORT_SIG, ".text");
    if (!create_export.has_result()) {
        return false;
    }

    if (!ue3::init()) {
        BLREDIRECT_LOG("failed to initialise - not redirecting anything");
        return true;
    }

    auto* target = static_cast<void*>(create_export.get());
    const auto ret = MH_CreateHook(
        target,
        reinterpret_cast<void*>(&create_export_hook),
        reinterpret_cast<void**>(&fn_create_export)
    );

    if (ret != MH_OK || MH_EnableHook(target) != MH_OK) {
        BLREDIRECT_LOG("failed to hook ULinkerLoad::CreateExport: {}", MH_StatusToString(ret));
        return true;
    }

    BLREDIRECT_LOG("hooked ULinkerLoad::CreateExport at {}", target);
    return true;
}

BOOL WINAPI is_debugger_present_hook() {
    if (!hooks_installed.load() && try_install()) {
        hooks_installed.store(true);
    }
    return fn_is_debugger_present();
}

}  // namespace

void install_hooks() {
    void* target = nullptr;
    const auto ret = MH_CreateHookApiEx(
        L"kernel32",
        "IsDebuggerPresent",
        reinterpret_cast<void*>(&is_debugger_present_hook),
        reinterpret_cast<void**>(&fn_is_debugger_present),
        &target
    );

    if (ret != MH_OK || MH_EnableHook(target) != MH_OK) {
        BLREDIRECT_LOG("failed to hook IsDebuggerPresent: {}", MH_StatusToString(ret));
    }
}

}  // namespace blredirect
