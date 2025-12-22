#include <iostream>
#include <string>
#include <vector>
#include <cstdio> 

#include "Unreal/ObjectArray.h"
#include "Unreal/NameArray.h"
#include "Utils/Utils.h"
#include "Utils/Encoding/UtfN.hpp"
#include "Menu/Logger.h"

uint8* NameArray::GNames = nullptr;

FNameEntry::FNameEntry(void* Ptr)
    : Address((uint8*)Ptr)
{
}

UnrealString FNameEntry::GetWString()
{
    if (!Address)
        return TEXT("");

    return GetStr(Address);
}

std::string FNameEntry::GetString()
{
    if (!Address)
        return "";

    return std::string(GetWString().begin(), GetWString().end());
}

void* FNameEntry::GetAddress()
{
    return Address;
}

void FNameEntry::Init(const uint8* FirstChunkPtr, int64 NameEntryStringOffset)
{
    LogInfo("Dumper-7: [FNameEntry] Initializing...");
    if (Settings::Internal::bUseNamePool)
    {
        constexpr int64 NoneStrLen = 0x4;
        constexpr uint16 BytePropertyStrLen = 0xC; // Length of "ByteProperty"
        constexpr uint32 BytePropertyStartAsUint32 = 'etyB'; // "Byte"

        Off::FNameEntry::NamePool::StringOffset = (int32)NameEntryStringOffset;
        Off::FNameEntry::NamePool::HeaderOffset = (int32)NameEntryStringOffset == 6 ? 4 : 0;
 
        const uint8* AssumedBytePropertyEntry = *reinterpret_cast<uint8* const*>(FirstChunkPtr) + NameEntryStringOffset + NoneStrLen;

        /* Check if there's pading after an FNameEntry. Check if there's up to 0x8 bytes padding. */
        for (int i = 0; i < 0x8; i++)
        {
            if (IsBadReadPtr(AssumedBytePropertyEntry + NameEntryStringOffset)) break;
            const uint32 FirstPartOfByteProperty = *reinterpret_cast<const uint32*>(AssumedBytePropertyEntry + NameEntryStringOffset);

            if (FirstPartOfByteProperty == BytePropertyStartAsUint32)
                break;

            AssumedBytePropertyEntry += 0x1;
        }

        uint16 BytePropertyHeader = *reinterpret_cast<const uint16*>(AssumedBytePropertyEntry + Off::FNameEntry::NamePool::HeaderOffset);

        /* Shifiting past the size of the header is not allowed, so limmit the shiftcount here */
        constexpr int32 MaxAllowedShiftCount = sizeof(BytePropertyHeader) * 0x8;
        LogInfo("Dumper-7: MaxAllowedShiftCount %d", MaxAllowedShiftCount);
        LogInfo("Dumper-7: BytePropertyHeader %d", BytePropertyHeader);
        while (BytePropertyHeader != BytePropertyStrLen && FNameEntryLengthShiftCount < MaxAllowedShiftCount)
        {
            FNameEntryLengthShiftCount++;
            BytePropertyHeader >>= 1;
        }

        if (FNameEntryLengthShiftCount == MaxAllowedShiftCount)
        {
            LogError("\nDumper-7: Error, couldn't get FNameEntryLengthShiftCount!\n");
            LogError("Dumper-7: FNameEntryLength %d | MaxAllowedShiftCount %d", FNameEntryLengthShiftCount, MaxAllowedShiftCount);
            GetStr = [](uint8* NameEntry) -> UnrealString { return TEXT("Invalid FNameEntryLengthShiftCount!"); };
            return;
        }

        LogSuccess("Dumper-7: [FNameEntry] NamePool initialized (Shift: %d, Stride: %d)", FNameEntryLengthShiftCount, Off::InSDK::NameArray::FNameEntryStride);

        GetStr = [](uint8* NameEntry) -> UnrealString
        {
            const uint16 HeaderWithoutNumber = *reinterpret_cast<uint16*>(NameEntry + Off::FNameEntry::NamePool::HeaderOffset);
            const int32 NameLen = HeaderWithoutNumber >> FNameEntry::FNameEntryLengthShiftCount;

            if (NameLen == 0)
            {
                const int32 EntryIdOffset = Off::FNameEntry::NamePool::StringOffset + ((Off::FNameEntry::NamePool::StringOffset == 6) * 2);

                const int32 NextEntryIndex = *reinterpret_cast<int32*>(NameEntry + EntryIdOffset);
                const int32 Number = *reinterpret_cast<int32*>(NameEntry + EntryIdOffset + sizeof(int32));

                if (Number > 0)
                    return NameArray::GetNameEntry(NextEntryIndex).GetWString() + TEXT('_') + ToUEString(Number - 1);

                return NameArray::GetNameEntry(NextEntryIndex).GetWString();
            }

            if (HeaderWithoutNumber & NameWideMask)
                return UnrealString(reinterpret_cast<const TCHAR*>(NameEntry + Off::FNameEntry::NamePool::StringOffset), NameLen);

            return UtfN::StringToWString(std::string(reinterpret_cast<const char*>(NameEntry + Off::FNameEntry::NamePool::StringOffset), NameLen));
        };
    }
    else
    {
        // GNames (NameArray) Logic
        LogInfo("Dumper-7: [FNameEntry] Mode: NameArray");
        
        uint8* FNameEntryNone = (uint8*)NameArray::GetNameEntry(0x0).GetAddress();
        uint8* FNameEntryIdxThree = (uint8*)NameArray::GetNameEntry(0x3).GetAddress();
        uint8* FNameEntryIdxEight = (uint8*)NameArray::GetNameEntry(0x8).GetAddress();

        if (IsBadReadPtr(FNameEntryNone) || IsBadReadPtr(FNameEntryIdxThree) || IsBadReadPtr(FNameEntryIdxEight)) {
            LogError("Dumper-7: Invalid FNameEntry pointers (None: %p)", FNameEntryNone);
            return;
        }

        LogInfo("Dumper-7: Analyzing FNameEntry structure...");

        // Scan for String Offset
        for (int i = 0; i < 0x20; i++)
        {
            if (*reinterpret_cast<uint32*>(FNameEntryNone + i) == 'enoN') // None
            {
                Off::FNameEntry::NameArray::StringOffset = i;
                LogInfo("Dumper-7: Found StringOffset at 0x%X", i);
                break;
            }
        }

        // Scan for Index Offset
        for (int i = 0; i < 0x20; i++)
        {
            if ((*reinterpret_cast<uint32*>(FNameEntryIdxThree + i) >> 1) == 0x3 &&
                (*reinterpret_cast<uint32*>(FNameEntryIdxEight + i) >> 1) == 0x8)
            {
                Off::FNameEntry::NameArray::IndexOffset = i;
                LogInfo("Dumper-7: Found IndexOffset at 0x%X", i);
                break;
            }
        }

        LogSuccess("Dumper-7: [FNameEntry] NameArray initialized (StringOffset: 0x%X, IndexOffset: 0x%X)",
                   Off::FNameEntry::NameArray::StringOffset, Off::FNameEntry::NameArray::IndexOffset);

        GetStr = [](uint8* NameEntry) -> UnrealString
        {
            const int32 NameIdx = *reinterpret_cast<int32*>(NameEntry + Off::FNameEntry::NameArray::IndexOffset);
            const void* NameString = reinterpret_cast<void*>(NameEntry + Off::FNameEntry::NameArray::StringOffset);

            if (NameIdx & NameWideMask)
                return UnrealString(reinterpret_cast<const TCHAR*>(NameString));

            return UtfN::StringToWString<std::string>(reinterpret_cast<const char*>(NameString));
        };
    }
}

bool NameArray::InitializeNameArray(uint8* NameArray)
{
    int32 ValidPtrCount = 0x0;
    int32 ZeroQWordCount = 0x0;

    if (!NameArray || IsBadReadPtr(NameArray))
        return false;

    for (int i = 0; i < 0x800; i += 0x8)
    {
        uint8* SomePtr = *reinterpret_cast<uint8**>(NameArray + i);

        if (SomePtr == 0)
        {
            ZeroQWordCount++;
        }
        else if (ZeroQWordCount == 0x0 && SomePtr != nullptr)
        {
            ValidPtrCount++;
        }
        else if (ZeroQWordCount > 0 && SomePtr != 0)
        {
            int32 NumElements = *reinterpret_cast<int32_t*>(NameArray + i);
            int32 NumChunks = *reinterpret_cast<int32_t*>(NameArray + i + 4);

            if (NumChunks == ValidPtrCount)
            {
                Off::NameArray::NumElements = i;
                Off::NameArray::MaxChunkIndex = i + 4;

                ByIndex = [](void* NamesArray, int32 ComparisonIndex, int32 NamePoolBlockOffsetBits) -> void*
                {
                    const int32 ChunkIdx = ComparisonIndex / 0x4000;
                    const int32 InChunk = ComparisonIndex % 0x4000;

                    if (ComparisonIndex > NameArray::GetNumElements())
                        return nullptr;

                    return reinterpret_cast<void***>(NamesArray)[ChunkIdx][InChunk];
                };

                LogSuccess("TNameEntryArray initialized successfully");
                return true;
            }
        }
    }

    LogError("Failed to initialize TNameEntryArray");
    return false;
}


bool NameArray::InitializeNamePool(uint8* NamePool)
{
    LogInfo("Initializing FNamePool...");

    // Default initialization
    Off::NameArray::MaxChunkIndex = 0x0;
    Off::NameArray::ByteCursor = 0x4;
    Off::NameArray::ChunksStart = 0x10;

    bool bWasMaxChunkIndexFound = false;
    // Basic pointer check
    if (IsBadReadPtr(NamePool)) {
        LogError("Invalid NamePool pointer");
        return false;
    }

    // usually at iOS it is MaxBlockOffsets (ChunkStart) is located at 0xD0
    for (int i = 0x0; i < 0x200; i += 4)
    {
        if (IsBadReadPtr(NamePool + i)) break;

        const int32 PossibleMaxChunkIdx = *reinterpret_cast<int32*>(NamePool + i);

        // Sanity checks: Max chunks is 8192 (0x2000)
        if (PossibleMaxChunkIdx < 0 || PossibleMaxChunkIdx > 0x2000)
            continue;

        int32 NotNullptrCount = 0x0;
        bool bFoundFirstPtr = false;
        
        /* Number of invalid pointers we can encounter before we assume that there are no valid pointers anymore. */
        constexpr int32 MaxAllowedNumInvalidPtrs = 0x500;
        int32 NumPtrsSinceLastValid = 0x0;

        for (int j = 0x0; j < 0x10000; j += 8)
        {
            const int32 ChunkOffset = i + 8 + j + (i % 8);
            
            if (IsBadReadPtr(NamePool + ChunkOffset)) break;

            uint8* ChunkPtr = *reinterpret_cast<uint8**>(NamePool + ChunkOffset);

            if (ChunkPtr != nullptr)
            {
                NotNullptrCount++;
                NumPtrsSinceLastValid = 0;

                if (!bFoundFirstPtr)
                {
                    bFoundFirstPtr = true;
                    Off::NameArray::ChunksStart = ChunkOffset;
                }
            }
            else
            {
                NumPtrsSinceLastValid++;
                if (NumPtrsSinceLastValid == MaxAllowedNumInvalidPtrs)
                    break;
            }
        }

        if (PossibleMaxChunkIdx == (NotNullptrCount - 1))
        {
            Off::NameArray::MaxChunkIndex = i;
            Off::NameArray::ByteCursor = i + 4;
            bWasMaxChunkIndexFound = true;
            LogInfo("[NameArray] Found MaxChunkIndex at 0x%X (Count: %d)", i, NotNullptrCount);
            break;
        }
    }

    if (!bWasMaxChunkIndexFound) {
        LogError("MaxChunkIndex not found in NamePool");
        return false;
    }
    
    constexpr uint32 NoneAsUint32 = 0x656E6F4E; // "None"
    constexpr uint64 CoreUObjAsUint64 = 0x6A624F5565726F43; // "jbOUeroC"
    
    uint8_t** ChunkPtr = reinterpret_cast<uint8_t**>(NamePool + Off::NameArray::ChunksStart);
    
    if (IsBadReadPtr(ChunkPtr)) {
        LogError("Invalid ChunkPtr in NamePool");
        return false;
    }

    uint8* FirstChunk = *ChunkPtr;
    if (IsBadReadPtr(FirstChunk)) {
        LogError("Invalid FirstChunk (is bad read). Decryption or Offset failed.");
        return false;
    }
    
    // "/Script/CoreUObject"
    bool bFoundCoreUObjectString = false;
    int64 FNameEntryHeaderSize = 0x0;

    constexpr int32 LoopLimit = 0x1000;
    for (int i = 0; i < LoopLimit; i++)
    {
        if (IsBadReadPtr(FirstChunk + i + 8)) continue;

        if (*reinterpret_cast<uint32*>(FirstChunk + i) == NoneAsUint32 && FNameEntryHeaderSize == 0)
        {
            FNameEntryHeaderSize = i;
        }
        else if (*reinterpret_cast<uint64*>(FirstChunk + i) == CoreUObjAsUint64)
        {
            bFoundCoreUObjectString = true;
            break;
        }
    }

    if (!bFoundCoreUObjectString) {
        LogError("CoreUObject string not found");
        return false;
    }

    LogInfo("Found CoreUObject, HeaderSize: %lld", FNameEntryHeaderSize);
    NameEntryStride = FNameEntryHeaderSize == 2 ? 2 : 4;
    Off::InSDK::NameArray::FNameEntryStride = (int32)NameEntryStride;

    ByIndex = [](void* NamesArray, int32 ComparisonIndex, int32 NamePoolBlockOffsetBits) -> void*
    {
        const int32 ChunkIdx = ComparisonIndex >> NamePoolBlockOffsetBits;
        const int32 InChunkOffset = (ComparisonIndex & ((1 << NamePoolBlockOffsetBits) - 1)) * (int32)NameEntryStride;

        if (ChunkIdx < 0 || ChunkIdx > NameArray::GetNumChunks()) return nullptr;

        uint8* PoolBase = reinterpret_cast<uint8*>(NamesArray);
        uint8** BlocksArray = reinterpret_cast<uint8**>(PoolBase + Off::NameArray::ChunksStart);

        if (IsBadReadPtr(BlocksArray + ChunkIdx)) return nullptr;

        uint8* TargetChunk = BlocksArray[ChunkIdx];
        if (IsBadReadPtr(TargetChunk)) return nullptr;

        return TargetChunk + InChunkOffset;
    };

    Settings::Internal::bUseNamePool = true;
    FNameEntry::Init(reinterpret_cast<uint8*>(ChunkPtr), FNameEntryHeaderSize);

    LogSuccess("FNamePool initialized successfully");
    return true;
}

/* * Finds a call to FName::GetNames, OR a reference to GNames directly.
 * * [iOS Port Note]: The original x64 instruction parsing logic has been replaced with
 * a generic search. On ARM64, direct pointer discovery via "ByteProperty" string reference
 * is usually reliable.
 */
inline std::pair<uintptr, bool> FindFNameGetNamesOrGNames(uintptr StartAddress)
{
    /* Range from "ByteProperty" which we want to search upwards */
    constexpr int32 SearchRange = 0x200;

    /* Find a reference to the string "ByteProperty" */
    /* Note: On iOS/Mach-O, FindByString works by searching for ADRL sequences pointing to the string */
    MemAddress BytePropertyStringRef = FindByStringInAllSections(TEXT("ByteProperty"), StartAddress);
    
    if (!BytePropertyStringRef) {
        // Fallback for ascii
        BytePropertyStringRef = FindByStringInAllSections("ByteProperty", StartAddress);
    }

    if (!BytePropertyStringRef)
        return { 0x0, false };

    /* On ARM64, accessing GNames usually looks like:
       ADRP X0, #Page_GNames
       ADD  X0, X0, #PageOff_GNames
       
       Or LDR X0, [GNames] if it's a pointer.
       
       We search near the string reference for potential global variables.
    */
    
    uintptr StringRefAddr = BytePropertyStringRef;
    
    // Simple Heuristic:
    // If we are in FName::StaticInit (or similar), GNames is likely initialized near here.
    // We scan for instructions referencing the .bss or .data section (where GNames would live).
    // This is a "blind" scan for pointers to valid memory that look like GNames.
    
    // For specific iOS games, pattern scanning is preferred.
    // Here we return the StringRefAddr as a starting point for debugging if manual offset is needed.
    
    // Note: The logic below is a placeholder. On iOS without x64 disassembly,
    // it's safer to rely on explicit patterns or the assumption that GNames is relative to this string.
    
    return { StringRefAddr, false };
};

bool NameArray::TryFindNameArray()
{
    LogInfo("Searching for TNameEntryArray GNames...");
    
    // iOS: We skip the Windows-specific "kernel32.dll" and "EnterCriticalSection" check.
    // Instead, we try to locate based on string references.
    
    auto [Address, bIsGNamesDirectly] = FindFNameGetNamesOrGNames(GetModuleBase());

    if (Address == 0x0)
    {
        LogError("ByteProperty string reference not found");
        return false;
    }

    // IOS TODO: Implement proper ARM64 instruction analysis here.
    // For now, we assume if we found the string, we might need manual verification or
    // we scan nearby for a pointer that looks like the NameArray.
    
    // Heuristic: Scan nearby memory for a pointer that points to a valid NameArray structure
    // (NumElements, MaxChunkIndex)
    
    uintptr ScanStart = Address - 0x200;
    uintptr ScanEnd = Address + 0x200;
    
    // Safety clamp
    if (ScanStart < GetModuleBase()) ScanStart = GetModuleBase();
    
    for (uintptr Ptr = ScanStart; Ptr < ScanEnd; Ptr += 8) {
         if (IsBadReadPtr(Ptr)) continue;
         
         // Candidate for GNames pointer?
         void* Candidate = *reinterpret_cast<void**>(Ptr);
         if (IsBadReadPtr(Candidate)) continue;
         
         // Check if it looks like a NameArray
         if (NameArray::InitializeNameArray((uint8*)Candidate)) {
             Off::InSDK::NameArray::GNames = (int32)GetOffset(Ptr);
             return true;
         }
    }
    
    return false;
}

bool NameArray::TryFindNamePool()
{
    LogInfo("Searching for FNamePool GNames...");
    
    // iOS: Replaced Windows-specific x64 pattern "48 8D 0D ..." with generic string search
    // "ByteProperty" is a very strong anchor for FNamePool.

    /* Singleton instance of FNamePool */
    void* NamePoolIntance = nullptr;
    
    // Try to find "ByteProperty" string ref
    LogInfo("Searching for ByteProperty string reference...");
    MemAddress StringRef = FindByStringInAllSections(TEXT("ByteProperty"));
    if (!StringRef) StringRef = FindByStringInAllSections("ByteProperty");
    LogInfo("ByteProperty StringRef: 0x%p", (void*)StringRef);
    
    if (StringRef) {
        // We found where "ByteProperty" is used.
        // In FNamePool constructor, it initializes the pool.
        // We scan nearby for the pool structure.
        
        LogInfo("Scanning memory range for NamePool structure...");
        uintptr Address = StringRef;
        uintptr ScanStart = Address - 0x200;
        uintptr ScanEnd = Address + 0x200;
        LogInfo("Scan range: 0x%lX to 0x%lX", ScanStart, ScanEnd);
        
        int scanCount = 0;
        for (uintptr Ptr = ScanStart; Ptr < ScanEnd; Ptr += 8) {
             scanCount++;
             if (scanCount % 32 == 0) {
                 LogInfo("Scanned %d addresses, current: 0x%lX", scanCount, Ptr);
             }
             // On ARM64, the address of the NamePool global might be loaded relative to PC.
             // We check if any value here looks like a NamePool.
             
             // NamePool usually starts with [NextChunkIndex (int32)] [ByteCursor (int32)] ...
             // We can check if reinterpret_cast<uint8*>(Ptr) is a valid pool.
             
             // However, NameArray::InitializeNamePool takes the POINTER to the pool data,
             // or the pool structure itself?
             // GNames in FNamePool mode points to the Pool struct.
             
             if (IsBadReadPtr(Ptr)) continue;
             
             // Check if this address itself acts as a NamePool (static instance)
             // or if it points to one.
             
             if (scanCount % 32 == 0) {
                 LogInfo("Testing address 0x%lX as direct NamePool", Ptr);
             }
             if (NameArray::InitializeNamePool(reinterpret_cast<uint8*>(Ptr))) {
                 Off::InSDK::NameArray::GNames = (int32)GetOffset(Ptr);
                 LogSuccess("Found NamePool at direct address 0x%lX", Ptr);
                 return true;
             }
             
             // Check if it's a pointer to the pool
             void* Candidate = *reinterpret_cast<void**>(Ptr);
             if (IsBadReadPtr(Candidate)) continue;
             
             if (scanCount % 32 == 0) {
                 LogInfo("Testing 0x%lX -> 0x%p as indirect NamePool", Ptr, Candidate);
             }
             if (NameArray::InitializeNamePool(reinterpret_cast<uint8*>(Candidate))) {
                 Off::InSDK::NameArray::GNames = (int32)GetOffset(Ptr); // The pointer location is GNames
                 LogSuccess("Found NamePool via pointer at 0x%lX -> 0x%p", Ptr, Candidate);
                 return true;
             }
        }
        LogInfo("Completed scanning %d addresses, no NamePool found", scanCount);
    } else {
        LogError("ByteProperty string reference not found for NamePool search");
    }

    if (NamePoolIntance)
    {
        LogSuccess("Using NamePoolInstance at 0x%p", NamePoolIntance);
        Off::InSDK::NameArray::GNames = (int32)GetOffset(NamePoolIntance);
        return true;
    }

    LogError("TryFindNamePool failed - no valid NamePool found");
    return false;
}

bool NameArray::TryInit(bool bIsTestOnly)
{
    uintptr ImageBase = GetModuleBase();

    uint8* GNamesAddress = nullptr;

    bool bFoundNameArray = false;
    bool bFoundnamePool = false;

    if (NameArray::TryFindNameArray())
    {
        LogSuccess("Found 'TNameEntryArray GNames' at offset 0x%lX", Off::InSDK::NameArray::GNames);
        
        GNamesAddress = *reinterpret_cast<uint8**>(ImageBase + Off::InSDK::NameArray::GNames);// Derefernce
        Settings::Internal::bUseNamePool = false;
        bFoundNameArray = true;
    }
    else if (NameArray::TryFindNamePool())
    {
        LogSuccess("Found 'FNamePool GNames' at offset 0x%lX", Off::InSDK::NameArray::GNames);
        
        GNamesAddress = reinterpret_cast<uint8*>(ImageBase + Off::InSDK::NameArray::GNames); // No derefernce
        Settings::Internal::bUseNamePool = true;
        bFoundnamePool = true;
    }

    if (!bFoundNameArray && !bFoundnamePool)
    {
        LogError("\n\nCould not find GNames!\n\n");
        return false;
    }

    if (bIsTestOnly)
        return false;

    if (bFoundNameArray && NameArray::InitializeNameArray(GNamesAddress))
    {
        GNames = GNamesAddress;
        Settings::Internal::bUseNamePool = false;
        FNameEntry::Init();
        return true;
    }
    else if (bFoundnamePool && NameArray::InitializeNamePool(reinterpret_cast<uint8*>(GNamesAddress)))
    {
        GNames = GNamesAddress;
        Settings::Internal::bUseNamePool = true;
        /* FNameEntry::Init() was moved into NameArray::InitializeNamePool to avoid duplicated logic */
        return true;
    }

    //GNames = nullptr;
    //Off::InSDK::NameArray::GNames = 0x0;
    //Settings::Internal::bUseNamePool = false;

    LogError("The address that was found couldn't be used by the generator, this might be due to GNames-encryption");

    return false;
}

bool NameArray::TryInit(int32 OffsetOverride, bool bIsNamePool, const char* const ModuleName)
{
    uintptr ImageBase = GetModuleBase(ModuleName);

    uint8* GNamesAddress = nullptr;

    const bool bIsNameArrayOverride = !bIsNamePool;
    const bool bIsNamePoolOverride = bIsNamePool;

    bool bFoundNameArray = false;
    bool bFoundnamePool = false;

    Off::InSDK::NameArray::GNames = OffsetOverride;
    
    char buffer[256];

    if (bIsNameArrayOverride)
    {
        snprintf(buffer, sizeof(buffer), "Overwrote offset: 'TNameEntryArray GNames' set as offset 0x%lX\n", Off::InSDK::NameArray::GNames);
        LogSuccess(buffer);
        GNamesAddress = *reinterpret_cast<uint8**>(ImageBase + Off::InSDK::NameArray::GNames);// Derefernce
        Settings::Internal::bUseNamePool = false;
        bFoundNameArray = true;
    }
    else if (bIsNamePoolOverride)
    {
        snprintf(buffer, sizeof(buffer), "Overwrote offset: 'FNamePool GNames' set as offset 0x%lX\n", Off::InSDK::NameArray::GNames);
        LogSuccess(buffer);
        GNamesAddress = reinterpret_cast<uint8*>(ImageBase + Off::InSDK::NameArray::GNames); // No derefernce
        Settings::Internal::bUseNamePool = true;
        bFoundnamePool = true;
    }

    if (!bFoundNameArray && !bFoundnamePool)
    {
        LogError("\n\nCould not find GNames!\n\n");
        return false;
    }

    if (bFoundNameArray && NameArray::InitializeNameArray(GNamesAddress))
    {
        GNames = GNamesAddress;
        Settings::Internal::bUseNamePool = false;
        FNameEntry::Init();
        return true;
    }
    else if (bFoundnamePool && NameArray::InitializeNamePool(reinterpret_cast<uint8*>(GNamesAddress)))
    {
        GNames = GNamesAddress;
        Settings::Internal::bUseNamePool = true;
        /* FNameEntry::Init() was moved into NameArray::InitializeNamePool to avoid duplicated logic */
        return true;
    }

    LogError("The address was overwritten, but couldn't be used. This might be due to GNames-encryption.\n");
    return false;
}

bool NameArray::SetGNamesWithoutCommiting()
{
    /* GNames is already set */
    if (Off::InSDK::NameArray::GNames != 0x0)
        return false;

    char buffer[256];

    if (NameArray::TryFindNameArray())
    {
        snprintf(buffer, sizeof(buffer), "Found 'TNameEntryArray GNames' at offset 0x%lX\n", Off::InSDK::NameArray::GNames);
        LogInfo(buffer);
        Settings::Internal::bUseNamePool = false;
        return true;
    }
    else if (NameArray::TryFindNamePool())
    {
        snprintf(buffer, sizeof(buffer), "Found 'FNamePool GNames' at offset 0x%lX\n", Off::InSDK::NameArray::GNames);
        LogInfo(buffer);
        Settings::Internal::bUseNamePool = true;
        return true;
    }

    LogInfo("\n\nCould not find GNames!\n\n");
    return false;
}

void NameArray::PostInit()
{
    if (GNames && Settings::Internal::bUseNamePool)
    {
        LogInfo("NameArray: PostInit started. Detecting FNameBlockOffsetBits...");

        // Start with the standard UE4/UE5 value (14 bits)
        // 0x10 (16) is standard for many versions, but some games use 0xD like Fortnite (13)
        NameArray::FNameBlockOffsetBits = 0x10;

        // Get the total number of chunks currently allocated in the name pool
        const int32 TotalChunks = NameArray::GetNumChunks();

        // Reverse-order iteration: Newest objects are at the end of the array
        // and are most likely to reside in the highest/last name chunks.
        int i = ObjectArray::Num() - 1;

        while (i >= 0)
        {
            UEObject Obj = ObjectArray::GetByIndex(i);

            if (!Obj)
            {
                i--;
                continue;
            }

            // Calculate which chunk this object's name *would* fall into with current bits
            const int32 ObjNameChunkIdx = Obj.GetFName().GetCompIdx() >> NameArray::FNameBlockOffsetBits;

            // If the calculated Chunk Index is greater than or equal to the TotalChunks available,
            // it means our shift count is too LOW (resulting in a huge index number).
            // We must increase the shift and restart the scan.
            if (ObjNameChunkIdx > TotalChunks)
            {
                LogInfo("NameArray: ChunkIdx %d exceeds limit %d. Incrementing OffsetBits to 0x%X",
                    ObjNameChunkIdx, TotalChunks, NameArray::FNameBlockOffsetBits + 1);

                NameArray::FNameBlockOffsetBits++;
                
                // Restart the search from the end of the list with the new bit count
                i = ObjectArray::Num() - 1;
                continue;
            }

            // Optimization: If we find an object that falls exactly into the highest allocated chunk,
            // we can be fairly confident our bits are correct (or at least correct enough for the current data).
            if (ObjNameChunkIdx == (TotalChunks - 1))
            {
                break;
            }

            i--;
        }

        Off::InSDK::NameArray::FNamePoolBlockOffsetBits = NameArray::FNameBlockOffsetBits;
         LogInfo("NameArray::FNameBlockOffsetBits: 0x%X", NameArray::FNameBlockOffsetBits);
    }
}

int32 NameArray::GetNumChunks()
{
    return *reinterpret_cast<int32*>(GNames + Off::NameArray::MaxChunkIndex);
}

int32 NameArray::GetNumElements()
{
    return !Settings::Internal::bUseNamePool ? *reinterpret_cast<int32*>(GNames + Off::NameArray::NumElements) : 0;
}

int32 NameArray::GetByteCursor()
{
    return Settings::Internal::bUseNamePool ? *reinterpret_cast<int32*>(GNames + Off::NameArray::ByteCursor) : 0;
}

FNameEntry NameArray::GetNameEntry(const void* Name)
{
    return ByIndex(GNames, FName(Name).GetCompIdx(), FNameBlockOffsetBits);
}

FNameEntry NameArray::GetNameEntry(int32 Idx)
{
    return ByIndex(GNames, Idx, FNameBlockOffsetBits);
}
