#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace portrait_registry {
constexpr unsigned capacity=4096, nameCapacity=256*1024;
struct Entry {std::uint8_t kind,padding;std::uint16_t reflection;const char* name;};
static_assert(sizeof(Entry)==8);
static_assert(offsetof(Entry,name)==4);
inline std::array<Entry,capacity> entries{};
inline std::array<char,nameCapacity> names{};
using Init=void(__cdecl*)(Entry*,void**,void**,int,int);
inline Init original=reinterpret_cast<Init>(0x579e70);
inline void (*logger)(const char*){};
inline bool copyBaseline(const Entry* source,unsigned maximum,Entry* output,unsigned outputSize) {
    if(!source||!output||!maximum||maximum>750||outputSize<maximum)return false;
    for(unsigned i=0;i<maximum;++i)if(source[i].kind==0xff) {
        memset(output,0,outputSize*sizeof(Entry));memcpy(output,source,(i+1)*sizeof(Entry));return true;
    }
    return false;
}
inline void __cdecl initialize(Entry* source,void** cursor,void** end,int maximum,int nameSize) {
    const auto initialName=entries[0].name; // Preserve an early demo/save-icon override.
    if(!copyBaseline(source,static_cast<unsigned>(maximum),entries.data(),entries.size())) {
        if(logger)logger("Portrait registry: unexpected baseline table; native registry retained");
        original(source,cursor,end,maximum,nameSize);return;
    }
    if(initialName)entries[0].name=initialName;
    names.fill(0);
    *reinterpret_cast<unsigned*>(0x951bbc)=0;
    *reinterpret_cast<unsigned*>(0x951bc4)=0;
    // Both buffers belong to the loader. Do not spend the world's fixed arena
    // or enlarge the original static 750-record table in place.
    original(entries.data(),cursor,end,capacity,0);
    *reinterpret_cast<char**>(0x951bc8)=names.data();
    *reinterpret_cast<unsigned*>(0x951bcc)=names.size();
    *reinterpret_cast<unsigned*>(0x951bd0)=0;
    if(logger)logger("Portrait registry: 4096 object slots and 256 KiB name storage initialized");
}
}
