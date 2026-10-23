#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Inspect only the standard ZIP central directory. Never decompress asset data
// just to decide whether an archive is a native DLL plugin.
inline bool ZipHasRootManifest(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file)return false;
    auto end=file.tellg();if(end<22)return false;
    uint64_t size=static_cast<uint64_t>(end);
    size_t count=static_cast<size_t>(size<65557?size:65557);
    std::vector<unsigned char> tail(count);file.seekg(static_cast<std::streamoff>(size-count));
    if(!file.read(reinterpret_cast<char*>(tail.data()),count))return false;
    auto u16=[](const unsigned char* p){return unsigned(p[0])|(unsigned(p[1])<<8);};
    auto u32=[](const unsigned char* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);};
    size_t at=count-22;
    for(;;) {
        if(u32(tail.data()+at)==0x06054b50 && at+22+u16(tail.data()+at+20)==count)break;
        if(at==0)return false;
        --at;
    }
    auto e=tail.data()+at;
    if(u16(e+4)||u16(e+6)||u16(e+8)!=u16(e+10))return false;
    auto entries=u16(e+10);uint64_t offset=u32(e+16),length=u32(e+12);
    if(entries==65535 || offset+length>size-count+at)return false;
    file.seekg(static_cast<std::streamoff>(offset));uint64_t read=0;
    for(unsigned i=0;i<entries;++i) {
        unsigned char header[46]{};
        if(read+46>length || !file.read(reinterpret_cast<char*>(header),46) || u32(header)!=0x02014b50)return false;
        unsigned names=u16(header+28),extra=u16(header+30),comment=u16(header+32);
        read+=46+names+extra+comment;
        if(read>length)return false;
        std::string name(names,0);
        if(!file.read(name.data(),names))return false;
        for(auto& c:name)if(c>='A'&&c<='Z')c+=32;
        if(name=="manifest.ini")return true;
        file.seekg(extra+comment,std::ios::cur);
    }
    return false;
}
