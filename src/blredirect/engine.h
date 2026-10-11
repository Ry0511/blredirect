#ifndef BLREDIRECT_ENGINE_H
#define BLREDIRECT_ENGINE_H

#include <cstdint>
#include <optional>
#include <string>

#include "blredirect/unreal_types.h"

namespace blredirect::ue3 {

bool init();

// GMalloc->Free
void free(void* ptr);

// ULinkerLoad::IndexToObject
UObject* index_to_object(ULinkerLoad* linker, int32_t index);

// Linker::GetExportPathName
std::wstring get_export_path_name(
    ULinker* linker,
    int32_t index,
    const wchar_t* fake_root = nullptr,
    bool resolve_forced_exports = false
);

// UObject::StaticLoadObject
UObject* static_load_object(
    UClass* cls,
    UObject* outer,
    const wchar_t* name,
    const wchar_t* filename = nullptr,
    uint32_t load_flags = 0,
    void* sandbox = nullptr,
    bool allow_object_reconciliation = true
);

UObject* get_outermost(UObject* obj);

// GPackageFileCache->FindPackageFile, empty if the package has no file
std::wstring find_package_file(const wchar_t* package);

// PackageFlags from a package file's summary
std::optional<uint32_t> read_package_flags(const std::wstring& file);

}  // namespace blredirect::ue3

#endif  // BLREDIRECT_ENGINE_H
