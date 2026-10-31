#pragma once
#include <windows.h>
#include <cstddef>
#include <cstring>

// The native hooks use addresses from this exact Steam PE image. Validate the
// host before doing any mod work, rather than relying on individual features
// to reject the host after settings, subprocesses or plugins have been started.
inline bool SupportedGameHeader(const void* data, size_t size) {
    if(!data || size<sizeof(IMAGE_DOS_HEADER))return false;
    IMAGE_DOS_HEADER dos{};std::memcpy(&dos,data,sizeof(dos));
    if(dos.e_magic!=IMAGE_DOS_SIGNATURE || dos.e_lfanew<0)return false;
    const auto offset=static_cast<size_t>(dos.e_lfanew);
    if(offset>size || sizeof(IMAGE_NT_HEADERS32)>size-offset)return false;
    IMAGE_NT_HEADERS32 pe{};
    std::memcpy(&pe,static_cast<const unsigned char*>(data)+offset,sizeof(pe));
    return pe.Signature==IMAGE_NT_SIGNATURE &&
        pe.FileHeader.Machine==IMAGE_FILE_MACHINE_I386 &&
        pe.FileHeader.SizeOfOptionalHeader>=sizeof(IMAGE_OPTIONAL_HEADER32) &&
        pe.FileHeader.TimeDateStamp==0x4ac4a20b &&
        pe.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
        pe.OptionalHeader.ImageBase==0x400000 &&
        pe.OptionalHeader.SizeOfImage==0x268e000;
}

inline bool SupportedGameHost() {
    const auto base=GetModuleHandleW(nullptr);
    if(base!=reinterpret_cast<HMODULE>(0x400000))return false;
    MEMORY_BASIC_INFORMATION memory{};
    if(!VirtualQuery(base,&memory,sizeof(memory)) || memory.State!=MEM_COMMIT ||
       memory.AllocationBase!=base || (memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    return SupportedGameHeader(base,memory.RegionSize<4096?memory.RegionSize:4096);
}
