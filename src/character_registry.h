#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace character_registry {
constexpr unsigned capacity=1000;
struct Entry {std::string directory,file;};
using Create=void*(__cdecl*)(const char*);
using Step=int(__cdecl*)(void*);
using Destroy=void(__cdecl*)(void*);
using Configure=void*(__cdecl*)(const char*,void**,void**,int,int*,int,void**);
inline Create create=reinterpret_cast<Create>(0x4fc860);
inline Step line=reinterpret_cast<Step>(0x4fc5b0),word=reinterpret_cast<Step>(0x4f7fb0);
inline Destroy destroy=reinterpret_cast<Destroy>(0x4fa300);
inline Configure original=reinterpret_cast<Configure>(0x6307c0);
inline void (*logger)(const char*){};
inline std::string folded(std::string value) {
    for(auto& c:value)if(c>='A'&&c<='Z')c+=32;return value;
}
inline std::vector<Entry> parse(const char* file) {
    // Retain the game's lexer/comment/quoted-word behavior. Only the native
    // fixed 500-offset arrays and 10 KB string pool are replaced.
    auto parser=create(file);if(!parser)return {};
    struct Close {void* p;~Close(){destroy(p);}} close{parser};
    std::vector<Entry> entries;bool inside=false;Entry entry;
    auto token=[&]()->const char* {return *reinterpret_cast<const char**>(static_cast<unsigned char*>(parser)+0x510);};
    while(line(parser)) {
        if(!word(parser))continue;
        auto key=folded(token());
        if(!inside) {
            if(key=="char_start"&&entries.size()<capacity){inside=true;entry={};}
        } else if(key=="char_end") {
            inside=false;
            if(!entry.directory.empty()&&!entry.file.empty())entries.push_back(std::move(entry));
        } else if((key=="dir"||key=="file")&&word(parser)) {
            std::string value=token();
            if(value.size()>255)continue;
            if(key=="dir")entry.directory=std::move(value);else entry.file=std::move(value);
        }
    }
    return entries;
}
inline void* layout(const std::vector<Entry>& entries,void** cursor,void** end,int* count,int stride,void** runtime) {
    if(entries.empty()||entries.size()>capacity||!cursor||!end||!*cursor||!*end||stride<0||stride>4096)return nullptr;
    size_t strings=0;
    for(const auto& entry:entries) {
        if(entry.directory.empty()||entry.file.empty()||entry.directory.size()>255||entry.file.size()>255)return nullptr;
        strings+=entry.directory.size()+entry.file.size()+2;
    }
    auto begin=(reinterpret_cast<uintptr_t>(*cursor)+3)&~uintptr_t(3);
    auto stringStart=begin+entries.size()*0x4c;
    auto runtimeStart=(stringStart+strings+3)&~uintptr_t(3);
    auto finish=(runtimeStart+entries.size()*stride+3)&~uintptr_t(3);
    if(begin<reinterpret_cast<uintptr_t>(*cursor)||finish>reinterpret_cast<uintptr_t>(*end)||finish<begin)return nullptr;
    memset(reinterpret_cast<void*>(begin),0,finish-begin);
    auto text=reinterpret_cast<char*>(stringStart);
    for(size_t i=0;i<entries.size();++i) {
        auto row=reinterpret_cast<unsigned char*>(begin+i*0x4c);
        *reinterpret_cast<int*>(row)=-1;
        *reinterpret_cast<float*>(row+0x2c)=1;
        *reinterpret_cast<float*>(row+0x30)=.5f;
        *reinterpret_cast<float*>(row+0x34)=-.5f;
        *reinterpret_cast<float*>(row+0x38)=.5f;
        *reinterpret_cast<float*>(row+0x3c)=1;
        *reinterpret_cast<short*>(row+0x42)=-1;
        for(auto field:{8,12}) {
            const auto& value=field==8?entries[i].directory:entries[i].file;
            *reinterpret_cast<char**>(row+field)=text;
            memcpy(text,value.c_str(),value.size()+1);text+=value.size()+1;
        }
        if(stride)*reinterpret_cast<void**>(row+0x24)=reinterpret_cast<void*>(runtimeStart+i*stride);
    }
    if(stride&&runtime)*runtime=reinterpret_cast<void*>(runtimeStart);
    if(count)*count=static_cast<int>(entries.size());
    *cursor=reinterpret_cast<void*>(finish);return reinterpret_cast<void*>(begin);
}
inline void* __cdecl configure(const char* file,void** cursor,void** end,int requested,int* count,int stride,void** runtime) {
    try {
        auto entries=parse(file);
        if(auto result=layout(entries,cursor,end,count,stride,runtime)) {
            if(logger)logger("Character registration: expanded parser loaded the complete roster (capacity 1000)");
            return result;
        }
    } catch(...) {}
    if(logger)logger("Character registration: expanded parser unavailable; native bounded parser retained");
    return original(file,cursor,end,requested,count,stride,runtime);
}
}
