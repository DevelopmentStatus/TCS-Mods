#pragma once
#include "ally_detour.h"
#include "shop_ownership.h"
#include "character_registry.h"
#include "portrait_registry.h"
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#pragma comment(lib,"bcrypt.lib")

// Verified Steam PC layout only. Sidecars augment, never resize, native saves.
namespace shop_capacity {
constexpr unsigned limit=1000, characterLimit=4096, nativeCharacters=340;
constexpr unsigned gameSize=0x7e4c;
using Records=std::map<std::string,unsigned char>;
using Digest=std::array<unsigned char,32>;
inline std::array<shop_ownership::Item,limit> items{};
inline std::array<unsigned, (limit+31)/32> purchased{};
inline std::array<unsigned,limit+44> codes{};
inline std::array<unsigned char,characterLimit> ownership{};
inline Records retained, snapshot;
inline Digest snapshotDigest{};
inline bool snapshotReady=false, installed=false;
inline bool identityLoaded=false;
inline int snapshotSlot=-1;
inline void (*logger)(const char*){};
inline AllyDetour newGameHook, saveHook;
using Copy=void(__cdecl*)(void*,const void*,unsigned);
using Save=int(__cdecl*)(int,void*,int,unsigned);
inline Copy nativeCopy=reinterpret_cast<Copy>(0x4ef660);
inline Save nativeSave{};
inline void note(const char* message) {if(logger)logger(message);}
inline bool accessible(const void* p,size_t size) {
    if(!p||!size)return false;
    MEMORY_BASIC_INFORMATION m{};
    if(!VirtualQuery(p,&m,sizeof(m))||m.State!=MEM_COMMIT||
       (m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
    const auto a=reinterpret_cast<uintptr_t>(p), b=reinterpret_cast<uintptr_t>(m.BaseAddress);
    return a>=b&&a-b<=m.RegionSize&&size<=m.RegionSize-(a-b);
}
inline bool digest(const void* data,size_t size,Digest& out) {
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};
    bool ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0&&
        BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0&&
        BCryptHashData(hash,(PUCHAR)data,static_cast<ULONG>(size),0)>=0&&
        BCryptFinishHash(hash,out.data(),static_cast<ULONG>(out.size()),0)>=0;
    if(hash)BCryptDestroyHash(hash);
    if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
    return ok;
}
inline bool validKey(const std::string& key) {
    if(key.empty()||key.size()>255)return false;
    return std::all_of(key.begin(),key.end(),[](unsigned char c){return c>=32&&c<127;});
}
template<class T> bool readValue(std::istream& in,T& value) {
    return !!in.read(reinterpret_cast<char*>(&value),sizeof(value));
}
template<class T> void writeValue(std::ostream& out,const T& value) {
    out.write(reinterpret_cast<const char*>(&value),sizeof(value));
}
inline bool readRecords(std::istream& in,const Digest& expected,Records& result) {
    constexpr char magic[]="TCSCHR12";char tag[8]{};Digest saved{};unsigned count{};
    if(!in.read(tag,8)||memcmp(tag,magic,8)||!in.read((char*)saved.data(),saved.size())||
       saved!=expected||!readValue(in,count)||count>characterLimit)return false;
    Records parsed;
    for(unsigned i=0;i<count;++i) {
        unsigned short length{};unsigned char value{};
        if(!readValue(in,length)||!readValue(in,value)||length>255||!length)return false;
        std::string key(length,'\0');
        if(!in.read(key.data(),length)||!validKey(key)||!parsed.emplace(key,value).second)return false;
    }
    if(in.peek()!=std::char_traits<char>::eof())return false;
    result=std::move(parsed);return true;
}
inline bool writeRecords(std::ostream& out,const Digest& fingerprint,const Records& records) {
    if(records.size()>characterLimit)return false;
    for(const auto& entry:records)if(!validKey(entry.first))return false;
    out.write("TCSCHR12",8);out.write((const char*)fingerprint.data(),fingerprint.size());
    const unsigned count=static_cast<unsigned>(records.size());writeValue(out,count);
    for(const auto& [key,flags]:records) {
        const auto length=static_cast<unsigned short>(key.size());
        writeValue(out,length);writeValue(out,flags);out.write(key.data(),length);
    }
    return !!out;
}
inline std::string keyFor(unsigned index) {
    const auto count=*reinterpret_cast<const unsigned*>(0x93b278);
    const auto data=*reinterpret_cast<const unsigned char**>(0x93b274);
    if(index>=count||count>characterLimit||!accessible(data,count*0x4c))return {};
    // CData.file is the config/model identity, not a localized name ID.
    auto name=*reinterpret_cast<const char* const*>(data+index*0x4c+0xc);
    std::string key;
    for(unsigned i=0;i<255;++i) {
        if(!accessible(name+i,1))return {};
        unsigned char c=name[i];if(!c)return validKey(key)?key:std::string{};
        if(c>='A'&&c<='Z')c+=32;key.push_back(c);
    }
    return {};
}
inline Records capture() {
    Records result=retained;
    const auto count=*reinterpret_cast<const unsigned*>(0x93b278);
    if(count<=characterLimit)for(unsigned i=0;i<count;++i) {
        auto key=keyFor(i);if(!key.empty())result[key]=ownership[i];
    }
    return result;
}
inline std::filesystem::path sidecar(int slot) {
    if(slot<0||slot>=6)return {};
    const auto name=reinterpret_cast<const char*(__cdecl*)(int)>(0x5436e0)(slot);
    if(!name)return {};
    return std::filesystem::path(name).concat(".tcs-characters");
}
inline bool store(const std::filesystem::path& path,const Digest& hash,const Records& data) {
    if(path.empty())return false;
    auto temporary=path;temporary+=".tmp";
    std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
    if(!out||!writeRecords(out,hash,data)){out.close();DeleteFileW(temporary.c_str());return false;}
    out.flush();bool ok=!!out;out.close();
    if(!ok||!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());return false;
    }
    return true;
}
inline void syncNative() {
    memcpy(reinterpret_cast<void*>(0x86e5c0),ownership.data(),nativeCharacters);
    memcpy(reinterpret_cast<void*>(0x86e4a8),purchased.data(),16);
}
inline void __cdecl saveCopy(void* target,const void* source,unsigned size) {
    if(source==reinterpret_cast<void*>(0x8668c8)&&size==gameSize) {
        syncNative();snapshotReady=false;
        try {
            snapshot=capture();snapshotSlot=*reinterpret_cast<int*>(0x921dc0);
            snapshotReady=digest(source,size,snapshotDigest);
        } catch(...) {note("Character progress: could not prepare sidecar snapshot");}
    }
    nativeCopy(target,source,size);
}
inline void __cdecl loadCopy(void* target,const void* source,unsigned size) {
    nativeCopy(target,source,size);
    if(target!=reinterpret_cast<void*>(0x8668c8)||size!=gameSize)return;
    ownership.fill(0);memcpy(ownership.data(),reinterpret_cast<void*>(0x86e5c0),nativeCharacters);
    purchased.fill(0);memcpy(purchased.data(),reinterpret_cast<void*>(0x86e4a8),16);
    retained.clear();snapshotReady=false;identityLoaded=false;Digest hash{};
    try {
        const auto path=sidecar(*reinterpret_cast<int*>(0x921dc0));
        std::ifstream input(path,std::ios::binary);
        if(input&&digest(source,size,hash)&&readRecords(input,hash,retained)) {
            identityLoaded=true;
            const auto count=*reinterpret_cast<const unsigned*>(0x93b278);
            if(count<=characterLimit)for(unsigned i=0;i<count;++i) {
                auto saved=retained.find(keyFor(i));if(saved!=retained.end())ownership[i]=saved->second;
            }
            note("Character progress: loaded save-slot sidecar by character identity");
        } else if(input)note("Character progress: sidecar mismatched or invalid; native save retained");
    } catch(...) {note("Character progress: could not read sidecar; native save retained");}
    *reinterpret_cast<unsigned char**>(0x93b288)=ownership.data();
    syncNative();
}
inline int __cdecl saveSlot(int slot,void* buffer,int size,unsigned hash) {
    Digest fingerprint{};
    const bool ours=snapshotReady&&snapshotSlot==slot&&size==gameSize+4&&
        digest(buffer,gameSize,fingerprint)&&fingerprint==snapshotDigest;
    auto result=nativeSave(slot,buffer,size,hash);
    if(ours&&result==1) {
        try {
            auto path=sidecar(slot);auto native=path;
            if(path.empty())throw std::runtime_error("Invalid save slot");
            auto text=native.native();text.resize(text.size()-std::wstring(L".tcs-characters").size());native=text;
            // The native writer reports success even after a short fwrite.
            // Confirm this exact snapshot reached disk before committing its companion.
            std::ifstream check(native,std::ios::binary|std::ios::ate);
            bool written=false;
            if(check&&check.tellg()>0&&check.tellg()<2*1024*1024) {
                std::vector<char> bytes(static_cast<size_t>(check.tellg()));check.seekg(0);
                if(check.read(bytes.data(),bytes.size())) {
                    const char* start=static_cast<const char*>(buffer);
                    written=std::search(bytes.begin(),bytes.end(),start,start+size)!=bytes.end();
                }
            }
            if(written&&store(path,fingerprint,snapshot))retained=snapshot;
            else note("Character progress: sidecar save FAILED; added-character progress was not saved");
        } catch(...) {note("Character progress: sidecar save FAILED; added-character progress was not saved");}
        snapshotReady=false;
    }
    return result;
}
inline void __cdecl newGame() {
    ownership.fill(0);purchased.fill(0);retained.clear();snapshot.clear();snapshotReady=false;identityLoaded=true;
    reinterpret_cast<void(__cdecl*)()>(newGameHook.bridge)();
    *reinterpret_cast<unsigned char**>(0x93b288)=ownership.data();syncNative();
}
inline void* __cdecl allocateItems(void**,void**,unsigned) {
    items.fill({});purchased.fill(0);
    memcpy(purchased.data(),reinterpret_cast<void*>(0x86e4a8),16);
    auto list=*reinterpret_cast<const unsigned char**>(0x855ef8);
    auto count=std::min<unsigned>(*reinterpret_cast<const unsigned short*>(0x855efe),limit);
    if(accessible(list,count*0x1c))for(unsigned i=0;i<count;++i) {
        unsigned short id{};memcpy(&id,list+i*0x1c,2);
        if(i>=100||identityLoaded)purchased[i/32]&=~(1u<<(i%32));
        if(id<ownership.size()&&(ownership[id]&1))purchased[i/32]|=1u<<(i%32);
    }
    return items.data();
}
inline void __cdecl updateItems() {
    auto& count=*reinterpret_cast<unsigned short*>(0x855efe);
    const auto original=count;count=static_cast<unsigned short>(std::min<unsigned>(count,limit));
    // Calling the native entry preserves the optional Shop Ownership Fix detour.
    reinterpret_cast<void(__cdecl*)()>(0x4c15e0)();
    const auto active=*reinterpret_cast<shop_ownership::Item**>(0x880478);
    if(active==items.data())for(unsigned i=0;i<count;++i) {
        const auto id=active[i].character;
        if(id<ownership.size()) {
            const bool owned=(ownership[id]&1)!=0;
            if(owned||identityLoaded)active[i].purchased=owned?1:0;
        }
    }
    count=original;
}
struct Patch {
    uintptr_t address{};std::vector<unsigned char> before,after;
};
inline std::vector<Patch> patches;
inline void operand(uintptr_t address,std::initializer_list<unsigned char> expected,unsigned offset,unsigned value) {
    Patch p{address,expected,expected};memcpy(p.after.data()+offset,&value,4);patches.push_back(std::move(p));
}
inline void call(uintptr_t address,std::initializer_list<unsigned char> expected,const void* function) {
    operand(address,expected,1,static_cast<unsigned>(reinterpret_cast<uintptr_t>(function)-(address+5)));
}
inline bool writePatch(const Patch& p,bool restore=false) {
    DWORD previous{},unused{};
    if(!VirtualProtect(reinterpret_cast<void*>(p.address),p.before.size(),PAGE_EXECUTE_READWRITE,&previous))return false;
    const auto& bytes=restore?p.before:p.after;
    memcpy(reinterpret_cast<void*>(p.address),bytes.data(),bytes.size());
    FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(p.address),bytes.size());
    VirtualProtect(reinterpret_cast<void*>(p.address),p.before.size(),previous,&unused);return true;
}
inline void makePatches() {
    patches.clear();
    call(0x4ca16f,{0xe8,0xfc,0xfc,0xa,0},portrait_registry::initialize);
    operand(0x419f21,{0x80,0x3c,0xf5,0xf8,0x44,0x7f,0,3},3,(unsigned)portrait_registry::entries.data());
    operand(0x419f41,{0x8b,0xc,0xf5,0xfc,0x44,0x7f,0},3,(unsigned)portrait_registry::entries.data()+4);
    operand(0x4c9e2f,{0xa3,0xfc,0x44,0x7f,0},1,(unsigned)portrait_registry::entries.data()+4);
    operand(0x4ca77c,{0xa1,0xfc,0x44,0x7f,0},1,(unsigned)portrait_registry::entries.data()+4);
    // Hub background selection has one local int array and no stack arguments.
    // 1000*4 remains below the x86 stack-probe threshold.
    operand(0x419fb0,{0x81,0xec,0x50,5,0,0},2,character_registry::capacity*4);
    operand(0x41a0d3,{0x81,0xc4,0x50,5,0,0},2,character_registry::capacity*4);
    // Character switching copies CHARCOUNT model IDs, including unloaded -1
    // entries. Both callers need 1000 shorts; the picker needs 1000 ints.
    operand(0x491766,{0x81,0xec,0x50,5,0,0},2,character_registry::capacity*4);
    operand(0x49179b,{0x81,0xc4,0x50,5,0,0},2,character_registry::capacity*4);
    operand(0x4917f4,{0x81,0xc4,0x50,5,0,0},2,character_registry::capacity*4);
    operand(0x491800,{0x81,0xec,0xac,2,0,0},2,0x7d4);
    operand(0x49180d,{0x89,0x84,0x24,0xa8,2,0,0},3,0x7d0);
    operand(0x491823,{0x8b,0x8c,0x24,0xa8,2,0,0},3,0x7d0);
    operand(0x491831,{0x81,0xc4,0xac,2,0,0},2,0x7d4);
    operand(0x491840,{0x81,0xec,0xd0,2,0,0},2,0x7f8);
    operand(0x49184d,{0x89,0x84,0x24,0xcc,2,0,0},3,0x7f4);
    operand(0x491855,{0x8b,0x9c,0x24,0xd8,2,0,0},3,0x800);
    operand(0x4918ab,{0x8b,0x8c,0x24,0xcc,2,0,0},3,0x7f4);
    operand(0x4918b9,{0x81,0xc4,0xd0,2,0,0},2,0x7f8);
    operand(0x491a7c,{0x8b,0x8c,0x24,0xd4,2,0,0},3,0x7fc);
    operand(0x491a8c,{0x81,0xc4,0xd0,2,0,0},2,0x7f8);
    patches.push_back({0x47e9b3,{0x8d,0x4c,0x24,0x34,0x51},{0x6a,0,0x90,0x90,0x90}});
    patches.push_back({0x47e9d9,{0x66,0x8b,0x44,0x24,0x28},{0x66,0x8b,0x44,0x24,0x20}});
    // Free Play's ID list also needs room for the terminator. Move its cookie
    // and incoming arguments with the frame; all array offsets stay unchanged.
    operand(0x489150,{0x81,0xec,0xb4,2,0,0},2,0x7e0);
    operand(0x48915d,{0x89,0x84,0x24,0xb0,2,0,0},3,0x7dc);
    operand(0x4891a2,{0x8b,0x84,0x24,0xc8,2,0,0},3,0x7f4);
    operand(0x4891c6,{0x8b,0x8c,0x24,0xcc,2,0,0},3,0x7f8);
    operand(0x4891d3,{0x8b,0xac,0x24,0xd0,2,0,0},3,0x7fc);
    operand(0x489756,{0x8b,0x8c,0x24,0xc0,2,0,0},3,0x7ec);
    operand(0x489768,{0x81,0xc4,0xb4,2,0,0},2,0x7e0);
    call(0x4ca303,{0xe8,0xb8,0x64,0x16,0},character_registry::configure);
    operand(0x41c7db,{0x81,0xff,0x54,1,0,0},2,character_registry::capacity);
    for(auto address:{0x41f8f5,0x41faa3,0x43d8da})operand(address,{0x81,0xff,0x53,1,0,0},2,character_registry::capacity-1);
    operand(0x41ff18,{0x81,0xfd,0x53,1,0,0},2,character_registry::capacity-1);
    operand(0x4c163a,{0x68,0x50,0x2d,0,0},1,limit*sizeof(shop_ownership::Item));
    call(0x4c164d,{0xe8,0x1e,0x10,9,0},allocateItems);
    operand(0x4c1880,{0x81,0xfa,0xf0,0xa,0,0},2,limit*0x1c);
    operand(0x4c6e07,{0x81,0xfd,0x50,0x2d,0,0},2,limit*sizeof(shop_ownership::Item));
    operand(0x4c1840,{0x68,0x40,2,0,0},1,sizeof(codes));
    operand(0x4c1852,{0x68,0xd0,1,0x88,0},1,(unsigned)codes.data());
    operand(0x4c1983,{0x89,4,0xad,0xd0,1,0x88,0},3,(unsigned)codes.data());
    operand(0x4c19e4,{0x89,4,0xad,0xd0,1,0x88,0},3,(unsigned)codes.data());
    operand(0x4c4720,{0x8b,0xc,0xb5,0xd0,1,0x88,0},3,(unsigned)codes.data());
    for(auto address:{0x4c1965,0x4c19d1})operand(address,{0x81,0xfd,0x90,0,0,0},2,limit+44);
    for(auto address:{0x4c4732,0x4c473c})operand(address,{0x81,0xfe,0x90,0,0,0},2,limit+44);
    operand(0x4c18f4,{0x8b,0x14,0x85,0xa8,0xe4,0x86,0},3,(unsigned)purchased.data());
    operand(0x4c26ea,{0xbe,0xa8,0xe4,0x86,0},1,(unsigned)purchased.data());
    operand(0x4c6e22,{0x8d,0x3c,0x85,0xa8,0xe4,0x86,0},3,(unsigned)purchased.data());
    call(0x4c19c1,{0xe8,0x1a,0xfc,0xff,0xff},updateItems);
    call(0x4c74e1,{0xe8,0xfa,0xa0,0xff,0xff},updateItems);
    operand(0x428c43,{0xc7,5,0x88,0xb2,0x93,0,0xc0,0xe5,0x86,0},6,(unsigned)ownership.data());
    // NewGame's only four direct character-save accesses (all other users use
    // the collection ownership pointer). Verify the full instructions below.
    operand(0x42af0b,{8,0x90,0xc0,0xe5,0x86,0},2,(unsigned)ownership.data());
    operand(0x42af20,{8,0x90,0xc0,0xe5,0x86,0},2,(unsigned)ownership.data());
    operand(0x42af3b,{8,0x90,0xc0,0xe5,0x86,0},2,(unsigned)ownership.data());
    operand(0x42af64,{8,0x90,0xc0,0xe5,0x86,0},2,(unsigned)ownership.data());
    call(0x50bdfb,{0xe8,0x60,0x38,0xfe,0xff},saveCopy);
    call(0x50bcd0,{0xe8,0x8b,0x39,0xfe,0xff},loadCopy);
}
inline bool installVerifiedPatches() {
    makePatches();
    const unsigned char reset[]{0x83,0xec,8,0x53,0x8a,0x1d,0xd8,0x68,0x86,0};
    const unsigned char save[]{0xb8,0x70,0x24,0,0};
    for(const auto& p:patches)if(!accessible((void*)p.address,p.before.size())||
        memcmp((void*)p.address,p.before.data(),p.before.size())) {
        note("Shop capacity: executable patch conflict; 1000-entry support was NOT installed");return false;
    }
    if(!accessible((void*)0x42abf0,sizeof(reset))||memcmp((void*)0x42abf0,reset,sizeof(reset))||
       !accessible((void*)0x5439d0,sizeof(save))||memcmp((void*)0x5439d0,save,sizeof(save)))return false;
    if(!newGameHook.install((void*)0x42abf0,newGame,reset,sizeof(reset)))return false;
    if(!saveHook.install((void*)0x5439d0,saveSlot,save,sizeof(save))) {newGameHook.remove();return false;}
    nativeSave=reinterpret_cast<Save>(saveHook.bridge);
    size_t applied=0;
    for(;applied<patches.size();++applied)if(!writePatch(patches[applied]))break;
    if(applied!=patches.size()) {
        while(applied)writePatch(patches[--applied],true);
        saveHook.remove();newGameHook.remove();return false;
    }
    memcpy(ownership.data(),(void*)0x86e5c0,nativeCharacters);
    *reinterpret_cast<unsigned char**>(0x93b288)=ownership.data();
    installed=true;note("Shop capacity: 1000 entries, 1044 cheat slots, extended character progress and per-save sidecars installed");
    return true;
}
inline bool install(void (*log)(const char*)) {
    if(installed)return true;logger=log;character_registry::logger=log;portrait_registry::logger=log;
    auto base=(unsigned char*)GetModuleHandleW(nullptr);
    if(base!=(void*)0x400000||!accessible(base,4096))return false;
    auto dos=(IMAGE_DOS_HEADER*)base;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>0xf00)return false;
    auto pe=(IMAGE_NT_HEADERS32*)(base+dos->e_lfanew);
    if(pe->Signature!=IMAGE_NT_SIGNATURE||pe->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386||
       pe->FileHeader.TimeDateStamp!=0x4ac4a20b||pe->OptionalHeader.SizeOfImage!=0x268e000)return false;
    return installVerifiedPatches();
}
}
