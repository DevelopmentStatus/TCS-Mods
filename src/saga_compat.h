#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <filesystem>
#include <string>
#include <cstring>
#include <array>
#pragma comment(lib,"bcrypt.lib")

// Deliberately supports only the exact Saga Tweaks 1.2.0 supplied for this task.
// This is not a general-purpose ASI loader.
namespace saga_compat {
using PauseUpdate = void(__cdecl*)(void*);
inline HMODULE module{};
inline PauseUpdate pauseUpdate{};
inline int* pauseRow{};
struct VideoBridge { int* mode{}; int* sync{}; int* cap{}; void(__cdecl* window)(){}; void(__cdecl* limit)(){}; };
inline VideoBridge video;
inline bool active=false;
inline bool supportedFile(const std::filesystem::path& path) {
    constexpr unsigned char expected[]{0x1d,0xdc,0x6c,0xb2,0xab,0xac,0x5c,0xcf,
        0x05,0x52,0x1c,0x68,0xbe,0xc7,0xd4,0x07,0x2d,0x00,0xa6,0x96,0x38,0x8c,0xec,0x65,
        0x87,0xa8,0x5d,0x85,0x8a,0xf8,0x2e,0xfd};
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};unsigned char digest[32]{};
    bool ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0 &&
        BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
    std::array<unsigned char,65536> buffer{};DWORD count{};
    while(ok) {
        if(!ReadFile(file,buffer.data(),static_cast<DWORD>(buffer.size()),&count,nullptr)){ok=false;break;}
        if(!count)break;
        ok=BCryptHashData(hash,buffer.data(),count,0)>=0;
    }
    if(ok)ok=BCryptFinishHash(hash,digest,sizeof(digest),0)>=0 && !std::memcmp(digest,expected,sizeof(expected));
    if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
    CloseHandle(file);return ok;
}
inline bool ownedCode(HMODULE owner,const void* code) {
    MEMORY_BASIC_INFORMATION memory{};
    return code && VirtualQuery(code,&memory,sizeof(memory)) &&
        memory.AllocationBase==owner && memory.State==MEM_COMMIT &&
        !(memory.Protect&(PAGE_GUARD|PAGE_NOACCESS));
}
template<class Owned> PauseUpdate resolvePauseBytes(const unsigned char* frame,size_t size,Owned owned) {
    if(!frame||size<40)return nullptr;
    const unsigned char mainPattern[]{0xc7,0x44,0x24,0x18};
    const unsigned char pausePattern[]{0xc7,0x44,0x24,0x1c};
    for(int i=0;i<=24;++i) {
        if(std::memcmp(frame+i,mainPattern,4) || std::memcmp(frame+i+8,pausePattern,4))continue;
        PauseUpdate pause{};std::memcpy(&pause,frame+i+12,sizeof(pause));
        if(!owned(reinterpret_cast<void*>(pause)))return nullptr;
        const unsigned char signature[]{0x83,0xec,0x1c,0x8b,0x15};
        return std::memcmp(reinterpret_cast<const void*>(pause),signature,sizeof(signature))?nullptr:pause;
    }
    return nullptr;
}
inline PauseUpdate resolvePause(HMODULE owner) {
    // MenusFrame contains relocated immediates for the main/pause handlers.
    auto frame=reinterpret_cast<const unsigned char*>(GetProcAddress(owner,"MenusFrame"));
    if(!ownedCode(owner,frame))return nullptr;
    return resolvePauseBytes(frame,40,[&](const void* code){return ownedCode(owner,code);});
}
inline bool suppressedPlugin(const std::wstring& id) {
    return active && id==L"tcs.skipsplash";
}
inline bool videoReady() {return active&&video.mode&&video.sync&&video.cap&&video.window&&video.limit;}
inline int windowMode(int modernMode) {return modernMode==0?1:modernMode==1?2:0;}
inline bool publishVideo(int mode,int sync) {
    if(!videoReady())return false;
    *video.mode=windowMode(mode);*video.sync=sync?1:0;return true;
}
inline void publishCap(int cap) {
    if(videoReady()){*video.cap=cap;video.limit();}
}
inline void persistVideo() {
    if(!videoReady()||!module)return;
    wchar_t filename[32768]{};if(!GetModuleFileNameW(module,filename,32768))return;
    const auto config=std::filesystem::path(filename).parent_path()/L"SagaTweaks.ini";
    for(auto item:{std::pair<const wchar_t*,int>{L"WindowMode",*video.mode},
                  {L"VSync",*video.sync},{L"MaxFPS",*video.cap}}) {
        const auto value=std::to_wstring(item.second);
        WritePrivateProfileStringW(L"SagaTweaks",item.first,value.c_str(),config.c_str());
    }
}
inline void bindCompatibility(HMODULE owner) {
    pauseUpdate=resolvePause(owner);pauseRow=nullptr;
    if(pauseUpdate) {
        int* row{};std::memcpy(&row,reinterpret_cast<const unsigned char*>(pauseUpdate)+5,sizeof(row));
        if(ownedCode(owner,row))pauseRow=row;
        else pauseUpdate=nullptr;
    }
    video.mode=reinterpret_cast<int*>(GetProcAddress(owner,"g_window_mode"));
    video.sync=reinterpret_cast<int*>(GetProcAddress(owner,"g_vsync"));
    video.cap=reinterpret_cast<int*>(GetProcAddress(owner,"g_max_fps"));
    video.window=reinterpret_cast<void(__cdecl*)()>(GetProcAddress(owner,"WindowApply"));
    video.limit=reinterpret_cast<void(__cdecl*)()>(GetProcAddress(owner,"ApplyMaxFps"));
    if(!ownedCode(owner,video.mode)||!ownedCode(owner,video.sync)||!ownedCode(owner,video.cap)||
       !ownedCode(owner,reinterpret_cast<void*>(video.window))||!ownedCode(owner,reinterpret_cast<void*>(video.limit)))video={};
}
inline void loadPrepared(const std::wstring& root,void (*log)(const char*)) try {
    namespace fs=std::filesystem;
    const auto catalog=root+L"Mods\\Assets\\catalog.ini";
    const int count=GetPrivateProfileIntW(L"Overlay",L"NativeCount",0,catalog.c_str());
    if(count==0)return;
    if(count!=1){log("Unexpected Saga Tweaks catalog; module not loaded");return;}
    wchar_t sourceText[32768]{},name[256]{},profile[128]{};
    GetPrivateProfileStringW(L"NativePlugin0",L"Source",L"",sourceText,32768,catalog.c_str());
    GetPrivateProfileStringW(L"NativePlugin0",L"Name",L"",name,256,catalog.c_str());
    GetPrivateProfileStringW(L"NativePlugin0",L"Profile",L"",profile,128,catalog.c_str());
    if(std::wcscmp(name,L"SagaTweaks.asi") || std::wcscmp(profile,L"saga-tweaks-1.2.0")) {
        log("Unsupported native module: only verified Saga Tweaks 1.2.0 is accepted");return;
    }
    const auto allowed=fs::weakly_canonical(fs::path(root)/L"Mods/Assets");
    const auto source=fs::weakly_canonical(sourceText);
    const auto relative=source.lexically_relative(allowed);
    if(relative.empty()||*relative.begin()==L".."||source.filename()!=L"SagaTweaks.asi"||!supportedFile(source)) {
        log("Saga Tweaks source/path/hash validation failed; module not loaded");return;
    }
    module=GetModuleHandleW(L"SagaTweaks.asi");
    if(module) {
        wchar_t existing[32768]{};
        if(!GetModuleFileNameW(module,existing,32768)||!supportedFile(existing)) {
            module=nullptr;log("Another Saga Tweaks build is already loaded; compatibility not enabled");return;
        }
        log("Verified Saga Tweaks already loaded; duplicate initialization skipped");
    } else {
        const auto folder=fs::path(root)/L"Mods/Loaded/ASI/SagaTweaks";
        fs::create_directories(folder);
        const auto loadedRoot=fs::weakly_canonical(fs::path(root)/L"Mods/Loaded");
        const auto destination=fs::weakly_canonical(folder/L"SagaTweaks.asi");
        const auto destinationRelative=destination.lexically_relative(loadedRoot);
        if(destinationRelative.empty()||*destinationRelative.begin()==L"..") {
            log("Saga Tweaks destination outside module cache; module not loaded");return;
        }
        auto pending=destination;pending+=L".pending";
        if(!CopyFileW(source.c_str(),pending.c_str(),FALSE)||!supportedFile(pending)||
           !MoveFileExW(pending.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
            DeleteFileW(pending.c_str());log("Saga Tweaks binary could not be prepared; module not loaded");return;
        }
        // Verify the installed copy immediately before Windows invokes its
        // DLL entry point. Do not call optional/general ASI initialization APIs.
        if(!supportedFile(destination)){log("Saga Tweaks final hash check failed");return;}
        module=LoadLibraryExW(destination.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
        if(!module){log("Verified Saga Tweaks failed to load");return;}
        log("Saga Tweaks loaded directly; Universal ASI Loader is not required");
    }
    active=true;bindCompatibility(module);
    log(pauseUpdate?"Saga Tweaks 1.2.0 pause compatibility ready":"Saga Tweaks pause compatibility signature unavailable");
} catch(const std::exception& error) {
    log((std::string("Saga Tweaks preparation failed: ")+error.what()).c_str());
}
}
