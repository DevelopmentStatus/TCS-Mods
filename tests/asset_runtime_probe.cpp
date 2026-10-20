// Opt-in integration probe; never included in released packages.
#define NOMINMAX
#include "mod_api.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdio>
namespace {
const TCSModHost* host{};TCSModHandle mod{};HWND window{};DWORD started{};bool done{};
std::wstring root;
using Open=int(__cdecl*)(const char*,int,void*,int);
using Read=int(__cdecl*)(int,void*,int);
using Close=void(__cdecl*)(int);
using Size=long long(__cdecl*)(const char*);
using Buffer=int(__cdecl*)(const char*,void*,int);
void __cdecl created(HWND hwnd,IDirect3DDevice9*,void*){window=hwnd;started=GetTickCount();}
void __cdecl frame(IDirect3DDevice9*,void*) {
    if(done||GetTickCount()-started<3000)return;done=true;
    wchar_t active[2048]{};
    GetPrivateProfileStringW(L"Overlay",L"Path",L"",active,2048,(root+L"Mods\\Assets\\catalog.ini").c_str());
    int count=0,failures=0;
    for(const auto& entry:std::filesystem::recursive_directory_iterator(active)) {
        if(!entry.is_regular_file())continue;
        auto name=entry.path().lexically_relative(active).generic_string();
        std::ifstream input(entry.path(),std::ios::binary);
        std::vector<char> expected((std::istreambuf_iterator<char>(input)),{}),actual(expected.size());
        auto size=reinterpret_cast<Size>(0x4fa690)(name.c_str());
        auto header=*reinterpret_cast<void**>(0x91ee88);
        int file=reinterpret_cast<Open>(0x4fa3f0)(name.c_str(),0,header,0);
        bool good=file&&size==static_cast<long long>(expected.size());
        if(file){int read=reinterpret_cast<Read>(0x4fb550)(file,actual.data(),static_cast<int>(actual.size()));
            reinterpret_cast<Close>(0x4f99e0)(file);good=good&&read==size&&actual==expected;}
        std::fill(actual.begin(),actual.end(),0);
        auto loaded=reinterpret_cast<Buffer>(0x4fd5d0)(name.c_str(),actual.data(),static_cast<int>(actual.size()));
        good=good&&loaded==size&&actual==expected;
        if(!good){++failures;host->log(mod,("ASSET PROBE FAIL: "+name).c_str());}
        ++count;
    }
    // Prove vanilla roster fallback after every archive has been removed.
    if(count==0) {
        int size=static_cast<int>(reinterpret_cast<Size>(0x4fa690)("CHARS/CHARS.TXT"));
        if(size<=0||size>1024*1024)++failures;
        else {std::vector<char> roster(size+1);int loaded=reinterpret_cast<Buffer>(0x4fd5d0)("CHARS/CHARS.TXT",roster.data(),size);
            if(loaded!=size||std::strstr(roster.data(),"DARTHNIHILUS"))++failures;}
    }
    char message[200];sprintf_s(message,"ASSET PROBE: %d files checked, %d failures; native open/read/size/bulk load",count,failures);
    host->log(mod,message);
    PostMessageW(window,WM_CLOSE,0,0);
}
}
TCS_MOD_EXPORT BOOL __cdecl TCSMod_Initialize(const TCSModHost* api,TCSModHandle* out) {
    wchar_t path[MAX_PATH]{};GetModuleFileNameW(nullptr,path,MAX_PATH);
    root=std::filesystem::path(path).parent_path().wstring()+L"\\";
    host=api;TCSModInfo info{sizeof(info),"tcs.assetprobe","Asset Runtime Probe","test"};
    *out=mod=api->registerMod(&info);if(!mod)return FALSE;
    api->registerDeviceCallbacks(mod,created,nullptr,frame,nullptr);return TRUE;
}
