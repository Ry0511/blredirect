#include <libhat.hpp>

#include <cstring>

#include "blredirect/engine.h"
#include "blredirect/logging.h"

namespace blredirect::ue3 {

////////////////////////////////////////////////////////////////////////////////
// | GLOBALS |
////////////////////////////////////////////////////////////////////////////////

namespace {

using IndexToObjectFn = UObject*(__thiscall*)(ULinkerLoad * self, int32_t index);

using GetExportPathNameFn = FString*(__thiscall*)(ULinker * self,
                                                  FString* out,
                                                  int32_t index,
                                                  const wchar_t* fake_root,
                                                  uint32_t resolve_forced_exports);

using StaticLoadObjectFn = UObject*(__cdecl*)(UClass * cls,
                                              UObject* outer,
                                              const wchar_t* name,
                                              const wchar_t* filename,
                                              uint32_t load_flags,
                                              void* sandbox,
                                              uint32_t allow_object_reconciliation);

using FreeFn = void(__thiscall*)(FMalloc* self, void* data);

IndexToObjectFn fn_index_to_object = nullptr;
GetExportPathNameFn fn_get_export_path_name = nullptr;
StaticLoadObjectFn fn_static_load_object = nullptr;

FMalloc** gmalloc = nullptr;

// UObject* ULinkerLoad::IndexToObject(PACKAGE_INDEX Index)
constexpr auto INDEX_TO_OBJECT_SIG = hat::compile_signature<
    " 6A FF"                    // push -1
    " 68 ?? ?? ?? ??"           // push handler
    " 64 A1 00 00 00 00"        // mov eax, fs:[0]
    " 50"                       // push eax
    " 83 EC 10"                 // sub esp, 0x10
    " 55 56 57"                 // push ebp; push esi; push edi
    " A1 ?? ?? ?? ??"           // mov eax, [security_cookie]
    " 33 C4"                    // xor eax, esp
    " 50"                       // push eax
    " 8D 44 24 20"              // lea eax, [esp + 0x20]
    " 64 A3 00 00 00 00"        // mov fs:[0], eax
    " 8B E9"                    // mov ebp, ecx (this)
    " 8B 44 24 30"              // mov eax, [esp + 0x30] (Index)
    " C7 44 24 10 00 00 00 00"  // mov [esp + 0x10], 0
    " 85 C0"                    // test eax, eax
    " 0F 8E"                    // jle imports
    >();

// FString ULinker::GetExportPathName
constexpr auto GET_EXPORT_PATH_NAME_SIG = hat::compile_signature<
    " 6A FF"                 // push -1
    " 68 ?? ?? ?? ??"        // push handler
    " 64 A1 00 00 00 00"     // mov eax, fs:[0]
    " 50"                    // push eax
    " 81 EC E0 00 00 00"     // sub esp, 0xe0
    " 53 55 56 57"           // push ebx; push ebp; push esi; push edi
    " A1 ?? ?? ?? ??"        // mov eax, [security_cookie]
    " 33 C4"                 // xor eax, esp
    " 50"                    // push eax
    " 8D 84 24 F4 00 00 00"  // lea eax, [esp + 0xf4]
    " 64 A3 00 00 00 00"     // mov fs:[0], eax
    " 89 4C 24 24"           // mov [esp + 0x24], ecx (this)
    " 33 FF"                 // xor edi, edi
    " 89 7C 24 14"           // mov [esp + 0x14], edi
    " 89 7C 24 18"           // mov [esp + 0x18], edi
    " 89 7C 24 1C"           // mov [esp + 0x1c], edi
    >();

// UObject* UObject::StaticLoadObject
constexpr auto STATIC_LOAD_OBJECT_SIG = hat::compile_signature<
    " 55"                 // push ebp
    " 8B EC"              // mov ebp, esp
    " 6A FF"              // push -1
    " 68 ?? ?? ?? ??"     // push handler
    " 64 A1 00 00 00 00"  // mov eax, fs:[0]
    " 50"                 // push eax
    " 83 EC 68"           // sub esp, 0x68
    " 53 56 57"           // push ebx; push esi; push edi
    " A1 ?? ?? ?? ??"     // mov eax, [security_cookie]
    " 33 C5"              // xor eax, ebp
    " 50"                 // push eax
    " 8D 45 F4"           // lea eax, [ebp - 0xc]
    " 64 A3 00 00 00 00"  // mov fs:[0], eax
    " 89 65 F0"           // mov [ebp - 0x10], esp
    " 8B 75 10"           // mov esi, [ebp + 0x10] (InName)
    " 33 FF"              // xor edi, edi
    " 89 7D EC"           // mov [ebp - 0x14], edi
    " 3B F7"              // cmp esi, edi
    " 74"                 // je
    >();

constexpr auto GMALLOC_SIG = hat::compile_signature<
    " 89 0D ?? ?? ?? ??"  // mov [GMalloc], ecx
    " 8B 11"              // mov edx, [ecx]
    >();

////////////////////////////////////////////////////////////////////////////////
// | HELPERS |
////////////////////////////////////////////////////////////////////////////////

template <typename T, typename Sig>
T find_function(const char* name, const Sig& signature) {
    const auto result = hat::find_pattern(signature, ".text");

    if (!result.has_result()) {
        BLREDIRECT_LOG("couldn't find {}", name);
        return nullptr;
    }

    return reinterpret_cast<T>(result.get());
}

template <typename T, typename Sig>
T* find_global(const char* name, const Sig& signature) {
    const auto result = hat::find_pattern(signature, ".text");

    if (!result.has_result()) {
        BLREDIRECT_LOG("couldn't find {}", name);
        return nullptr;
    }

    T* global = nullptr;
    std::memcpy(static_cast<void*>(&global), result.get() + 2, sizeof(global));
    return global;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// | PUBLIC API |
////////////////////////////////////////////////////////////////////////////////

bool init() {
    fn_index_to_object = find_function<IndexToObjectFn>("ULinkerLoad::IndexToObject", INDEX_TO_OBJECT_SIG);
    fn_get_export_path_name = find_function<GetExportPathNameFn>("ULinker::GetExportPathName", GET_EXPORT_PATH_NAME_SIG);
    fn_static_load_object = find_function<StaticLoadObjectFn>("UObject::StaticLoadObject", STATIC_LOAD_OBJECT_SIG);
    gmalloc = find_global<FMalloc*>("GMalloc", GMALLOC_SIG);

    return fn_index_to_object != nullptr
           && fn_get_export_path_name != nullptr
           && fn_static_load_object != nullptr
           && gmalloc != nullptr;
}

void free(void* ptr) {
    if (ptr != nullptr && gmalloc != nullptr && *gmalloc != nullptr) {
        FMalloc* malloc = *gmalloc;
        reinterpret_cast<FreeFn>(malloc->VfTable[3])(malloc, ptr);
    }
}

UObject* index_to_object(ULinkerLoad* linker, int32_t index) {
    return fn_index_to_object(linker, index);
}

std::wstring get_export_path_name(ULinker* linker, int32_t index, const wchar_t* fake_root, bool resolve_forced_exports) {
    FString path{};
    fn_get_export_path_name(linker, &path, index, fake_root, resolve_forced_exports ? 1 : 0);
    std::wstring result = path.Data != nullptr ? path.Data : L"";
    free(path.Data);
    return result;
}

UObject* static_load_object(
    UClass* cls,
    UObject* outer,
    const wchar_t* name,
    const wchar_t* filename,
    uint32_t load_flags,
    void* sandbox,
    bool allow_object_reconciliation
) {
    return fn_static_load_object(
        cls,
        outer,
        name,
        filename,
        load_flags,
        sandbox,
        allow_object_reconciliation ? 1 : 0
    );
}

UObject* get_outermost(UObject* obj) {
    while (obj->Outer != nullptr) {
        obj = obj->Outer;
    }
    return obj;
}

}  // namespace blredirect::ue3
