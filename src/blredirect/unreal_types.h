#ifndef BLREDIRECT_UNREAL_TYPES_H
#define BLREDIRECT_UNREAL_TYPES_H

#include <cstdint>

// TODO: I really do need to just create a generic BL1 types library that I can vendor in. That
//  way I can manage both BL1 and BL1E in the same place.

namespace blredirect::ue3 {

#pragma pack(push, 4)

////////////////////////////////////////////////////////////////////////////////
// | Core / General |
////////////////////////////////////////////////////////////////////////////////

struct FName {
    int32_t Index;
    int32_t Number;
};

struct FGuid {
    uint32_t A;
    uint32_t B;
    uint32_t C;
    uint32_t D;

    bool operator==(const FGuid&) const = default;
};

template <typename T>
struct TArray {
    T* Data;
    int32_t ArrayNum;
    int32_t ArrayMax;
};

using FString = TArray<wchar_t>;

struct FMalloc {
    void** VfTable;
};

////////////////////////////////////////////////////////////////////////////////
// | UObject Framework |
////////////////////////////////////////////////////////////////////////////////

struct ULinkerLoad;

struct UObject {
    void* VfTableObject;
    int32_t ObjectInternalInteger;
    uint64_t ObjectFlags;
    UObject* HashNext;
    UObject* HashOuterNext;
    void* StateFrame;
    ULinkerLoad* Linker;
    int32_t LinkerIndex;
    int32_t NetIndex;
    UObject* Outer;
    FName Name;
    struct UClass* Class;
    UObject* ObjectArchetype;
};

struct UField : UObject {
    UField* SuperField;
    UField* Next;
};

struct UClass : UField {};

struct UPackage : UObject {};

////////////////////////////////////////////////////////////////////////////////
// | Package Exports |
////////////////////////////////////////////////////////////////////////////////

struct FObjectExport {
    FName ObjectName;
    int32_t OuterIndex;
    int32_t ClassIndex;
    int32_t SuperIndex;
    int32_t ArchetypeIndex;
    uint64_t ObjectFlags;
    int32_t SerialSize;
    int32_t SerialOffset;
    int32_t ScriptSerializationStartOffset;
    int32_t ScriptSerializationEndOffset;
    UObject* Object;
    int32_t iHashNext;
    uint32_t ExportFlags;
    TArray<int32_t> GenerationNetObjectCount;
    FGuid PackageGuid;
    uint32_t PackageFlags;
};

//
// this is ass to follow
//
// struct TChunkedArray<FObjectExport,65520,FChunkedArraySystemAllocator,128> ExportMap;
//
// struct TChunkedArray<FObjectExport,65520,FChunkedArraySystemAllocator,128> {
//     struct TArray<TChunkedArray<FObjectExport,65520,FChunkedArraySystemAllocator,128>::FChunk,TInlineAllocator<128,FDefaultAllocator>_> Chunks;
//     int ArrayNum;
//     int ArrayMax;
// };
//
// struct TArray<TChunkedArray<FObjectExport,65520,FChunkedArraySystemAllocator,128>::FChunk,TInlineAllocator<128,FDefaultAllocator>_> {
//     struct ForElementType<TChunkedArray<FObjectExport,65520,FChunkedArraySystemAllocator,128>::FChunk> AllocatorInstance;
//     int ArrayNum;
//     int ArrayMax;
// };
//
// struct ForElementType<TChunkedArray<FObjectExport,65520,FChunkedArraySystemAllocator,128>::FChunk> {
//     struct TTypeCompatibleBytes<TChunkedArray<FObjectExport,65520,FChunkedArraySystemAllocator,128>::FChunk> InlineData[128];
//     struct ForElementType<TChunkedArray<FObjectExport,65520,FChunkedArraySystemAllocator,128>::FChunk> SecondaryData;
// };
//
// typedef enum <unnamed-tag_00023430> {
//     NumElementsPerChunk=712
// } <unnamed-tag_00023430>;

template <typename T>
struct TChunkedArray {
    static constexpr int32_t NumElementsPerChunk = 65536 / sizeof(T);
    static constexpr int32_t NumInlineChunks = 128;

    struct FChunk {
        T* Data;
        int32_t NumElements;
    };

    FChunk InlineChunks[NumInlineChunks];
    FChunk* SecondaryChunks;
    int32_t ChunkNum;
    int32_t ChunkMax;
    int32_t NumElements;
    int32_t MaxElements;

    FChunk* data() {
        return SecondaryChunks != nullptr ? SecondaryChunks : InlineChunks;
    }

    T* at(int32_t flat_index) {
        if (flat_index < 0 || flat_index >= NumElements) {
            return nullptr;
        }

        const int32_t chunk_index = flat_index / NumElementsPerChunk;
        const int32_t index = flat_index % NumElementsPerChunk;

        if (chunk_index >= ChunkNum) {
            return nullptr;
        }

        FChunk& chunk = data()[chunk_index];
        return chunk.Data == nullptr ? nullptr : chunk.Data + index;
    }
};

struct ULinker : UObject {
    UPackage* LinkerRoot;
    uint8_t _0[0x98];
    TChunkedArray<FObjectExport> ExportMap;
};

struct ULinkerLoad : ULinker {};

#pragma pack(pop)

}  // namespace blredirect::ue3

#endif  // BLREDIRECT_UNREAL_TYPES_H
