#define NOMINMAX
#include "asset_overlay.h"
#include "ally_detour.h"
#include "startup_wait.h"
#include <filesystem>
#include <unordered_map>
#include <algorithm>
#include <cstdio>
#include <vector>

namespace {
using OpenFn = int (__cdecl*)(const char*, int, void*, int);
using ExistsFn = int (__cdecl*)(const char*);
using SizeFn = long long (__cdecl*)(const char*);
using BufferFn = int (__cdecl*)(const char*, void*, int);
AllyDetour openHook, existsHook, sizeHook, bufferHook;
std::unordered_map<std::string, std::string> overrides;
std::string gamePrefix;
void (*logger)(const char*){};
const unsigned char openSignature[]{0x81,0xec,0x04,0x01,0x00,0x00};
const unsigned char existsSignature[]{0x57,0x8b,0x7c,0x24,0x08};
const unsigned char sizeSignature[]{0x51,0xa1,0x90,0xee,0x91,0x00};
const unsigned char bufferSignature[]{0xa1,0x88,0xee,0x91,0x00};

std::string upper(std::string value) {
    for(auto& c:value) { if(c=='/')c='\\'; else if(c>='a'&&c<='z')c-=32; }
    return value;
}
std::string narrow(const std::wstring& text) {
    if(text.empty()) return {};
    int count=WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    std::string out(count,0);
    WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),out.data(),count,nullptr,nullptr);
    return out;
}
const std::string* replacement(const char* name) {
    if(!name) return nullptr;
    auto key=upper(name);
    if(key.compare(0,gamePrefix.size(),gamePrefix)==0)key.erase(0,gamePrefix.size());
    while(key.compare(0,2,".\\")==0)key.erase(0,2);
    auto found=overrides.find(key);
    return found==overrides.end()?nullptr:&found->second;
}
int __cdecl openAsset(const char* name,int mode,void* header,int quiet) {
    auto native=reinterpret_cast<OpenFn>(openHook.bridge);
    if(mode==0 || mode==3) if(auto path=replacement(name)) {
        // Null header bypasses DAT lookup. Native disk handles retain the game's
        // buffering, seek, read, async status and close implementations.
        int result=native(path->c_str(),mode,nullptr,quiet);
        if(result) return result;
        if(logger)logger("Asset override could not open; falling back to original asset");
    }
    return native(name,mode,header,quiet);
}
int __cdecl existsAsset(const char* name) {
    if(auto path=replacement(name)) {
        DWORD attributes=GetFileAttributesA(path->c_str());
        if(attributes!=INVALID_FILE_ATTRIBUTES && !(attributes&FILE_ATTRIBUTE_DIRECTORY))return 1;
    }
    return reinterpret_cast<ExistsFn>(existsHook.bridge)(name);
}
long long __cdecl sizeAsset(const char* name) {
    if(auto path=replacement(name)) {
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if(GetFileAttributesExA(path->c_str(),GetFileExInfoStandard,&data))
            return (static_cast<long long>(data.nFileSizeHigh)<<32)|data.nFileSizeLow;
    }
    return reinterpret_cast<SizeFn>(sizeHook.bridge)(name);
}
int __cdecl bufferAsset(const char* name,void* buffer,int capacity) {
    auto native=reinterpret_cast<BufferFn>(bufferHook.bridge);
    if(auto path=replacement(name)) {
        // This bulk loader consults DAT directly before NuFileOpenDF. An absolute
        // replacement path misses that tree and retains native buffer/decode rules.
        DWORD attributes=GetFileAttributesA(path->c_str());
        if(attributes!=INVALID_FILE_ATTRIBUTES && !(attributes&FILE_ATTRIBUTE_DIRECTORY))
            return native(path->c_str(),buffer,capacity);
    }
    return native(name,buffer,capacity);
}
bool supported() {
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if(base!=reinterpret_cast<unsigned char*>(0x400000))return false;
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return false;
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS32*>(base+dos->e_lfanew);
    return pe->Signature==IMAGE_NT_SIGNATURE && pe->FileHeader.Machine==IMAGE_FILE_MACHINE_I386 &&
        pe->FileHeader.TimeDateStamp==0x4ac4a20b && pe->OptionalHeader.SizeOfImage==0x268e000 &&
        !std::memcmp(reinterpret_cast<void*>(0x4fa3f0),openSignature,sizeof(openSignature)) &&
        !std::memcmp(reinterpret_cast<void*>(0x4fa610),existsSignature,sizeof(existsSignature)) &&
        !std::memcmp(reinterpret_cast<void*>(0x4fa690),sizeSignature,sizeof(sizeSignature)) &&
        !std::memcmp(reinterpret_cast<void*>(0x4fd5d0),bufferSignature,sizeof(bufferSignature));
}
bool runImporter(const std::wstring& root) {
    auto exe=root+L"TCSAssetManager.exe";
    if(GetFileAttributesW(exe.c_str())==INVALID_FILE_ATTRIBUTES) {
        logger("TCSAssetManager.exe missing; legacy asset mods are disabled");return false;
    }
    CreateDirectoryW((root+L"Mods").c_str(),nullptr);
    CreateDirectoryW((root+L"Mods\\Assets").c_str(),nullptr);
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
    HANDLE output=CreateFileW((root+L"Mods\\Assets\\import.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ,
        &security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(output==INVALID_HANDLE_VALUE)return false;
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;
    startup.hStdOutput=startup.hStdError=output;
    startup.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    auto directory=root;while(!directory.empty()&&(directory.back()==L'\\'||directory.back()==L'/'))directory.pop_back();
    auto eventName=L"Local\\TCSAssetPriority-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    HANDLE uiEvent=CreateEventW(nullptr,TRUE,FALSE,eventName.c_str());
    std::wstring command=L"\""+exe+L"\" --game \""+directory+L"\" --startup-ui --ui-event \""+eventName+L"\"";
    // This exists before Python unpacks, so Steam startup never looks invisible.
    WNDCLASSW statusClass{};
    statusClass.lpfnWndProc=DefWindowProcW;
    statusClass.hInstance=GetModuleHandleW(nullptr);
    statusClass.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    statusClass.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));
    statusClass.lpszClassName=L"TCSAssetPreparation";
    RegisterClassW(&statusClass);
    HWND progress=CreateWindowExW(WS_EX_APPWINDOW,statusClass.lpszClassName,L"TCS Mod Loader 1.6.6 - Preparing mods",
        WS_OVERLAPPED|WS_CAPTION|WS_VISIBLE, CW_USEDEFAULT,CW_USEDEFAULT,540,160,
        nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(progress) {
        CreateWindowW(L"STATIC",L"Preparing game assets...\n\nLarge or changed mod packs can take a few minutes.\nThe game will open automatically when preparation finishes.",
            WS_CHILD|WS_VISIBLE,20,20,490,100,progress,nullptr,GetModuleHandleW(nullptr),nullptr);
        UpdateWindow(progress);
    }
    bool started=CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,
        nullptr,root.c_str(),&startup,&process)!=FALSE;
    if(!started){if(progress)DestroyWindow(progress);if(uiEvent)CloseHandle(uiEvent);CloseHandle(output);return false;}
    // Human review time must not consume the asset extraction watchdog.
    logger("Preparing asset mods; waiting for importer or load-order review");
    DWORD wait=WaitForAssetStartup(process.hProcess,uiEvent,600000,progress);
    if(progress)DestroyWindow(progress);
    if(wait!=WAIT_OBJECT_0){TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,5000);}
    DWORD code=1;if(wait==WAIT_OBJECT_0)GetExitCodeProcess(process.hProcess,&code);
    if(wait!=WAIT_OBJECT_0)logger("Asset startup interrupted or timed out; importer stopped");
    else logger(code==0?"Asset preparation completed":"Asset importer returned an error; see Mods/Assets/import.log");
    CloseHandle(process.hProcess);CloseHandle(process.hThread);CloseHandle(output);if(uiEvent)CloseHandle(uiEvent);
    return wait==WAIT_OBJECT_0&&code==0;
}
void registerPackages(const std::wstring& catalog,const TCSModHost& host) {
    int count=GetPrivateProfileIntW(L"Overlay",L"Count",0,catalog.c_str());
    for(int i=0;i<count && i<512;++i) {
        auto section=L"Package"+std::to_wstring(i);wchar_t id[80]{},name[256]{};
        GetPrivateProfileStringW(section.c_str(),L"Id",L"",id,80,catalog.c_str());
        GetPrivateProfileStringW(section.c_str(),L"Name",L"",name,256,catalog.c_str());
        auto identity=narrow(id),title=narrow(name);
        TCSModInfo info{sizeof(info),identity.c_str(),title.c_str(),"Asset package"};
        auto mod=host.registerMod(&info);if(!mod)continue;
        TCSSettingDescriptor setting{};setting.structSize=sizeof(setting);setting.category="Assets";
        setting.key="Enabled";setting.label="Enabled (restart)";setting.type=TCS_SETTING_BOOL;
        setting.defaultValue.integer=1;setting.maximum=1;setting.step=1;
        host.registerSetting(mod,&setting);
        setting.key="Priority";setting.label="Priority (higher wins, restart)";setting.type=TCS_SETTING_INT;
        setting.defaultValue.integer=100;setting.maximum=1000;
        host.registerSetting(mod,&setting);
        int variants=GetPrivateProfileIntW(section.c_str(),L"VariantCount",0,catalog.c_str());
        for(int v=0;v<variants && v<32;++v) {
            auto prefix=L"Variant"+std::to_wstring(v);
            wchar_t key[80]{},label[180]{};
            GetPrivateProfileStringW(section.c_str(),(prefix+L"Key").c_str(),L"",key,80,catalog.c_str());
            GetPrivateProfileStringW(section.c_str(),(prefix+L"Label").c_str(),L"Variant",label,180,catalog.c_str());
            int count=GetPrivateProfileIntW(section.c_str(),(prefix+L"Count").c_str(),0,catalog.c_str());
            if(count<2 || count>128)continue;
            std::vector<std::string> labels;
            for(int o=0;o<count;++o) {
                wchar_t option[128]{};
                GetPrivateProfileStringW(section.c_str(),(prefix+L"Option"+std::to_wstring(o)).c_str(),L"",option,128,catalog.c_str());
                labels.push_back(narrow(option));
            }
            std::vector<const char*> options;
            for(auto& text:labels)options.push_back(text.c_str());
            auto optionKey=narrow(key),optionLabel=narrow(label)+" (restart)";
            setting.key=optionKey.c_str();setting.label=optionLabel.c_str();setting.type=TCS_SETTING_ENUM;
            setting.defaultValue.integer=0;setting.minimum=0;setting.maximum=static_cast<float>(count-1);
            setting.choices=options.data();setting.choiceCount=static_cast<unsigned int>(options.size());
            host.registerSetting(mod,&setting);
        }
    }
}
}

bool PrepareAssetOverlay(const std::wstring& root,const TCSModHost& host,void (*log)(const char*)) try {
    logger=log;
    if(!supported()){log("Asset overrides: unsupported executable or conflicting file hooks");return false;}
    if(!runImporter(root)){log("Asset import failed; see Mods/Assets/import.log. Native assets retained.");return false;}
    auto catalog=root+L"Mods\\Assets\\catalog.ini";
    registerPackages(catalog,host);
    wchar_t folder[2048]{};
    GetPrivateProfileStringW(L"Overlay",L"Path",L"",folder,2048,catalog.c_str());
    std::error_code error;
    auto active=std::filesystem::weakly_canonical(folder,error);
    auto allowed=std::filesystem::weakly_canonical(root+L"Mods\\Assets",error);
    auto relative=active.lexically_relative(allowed);
    if(error || relative.empty() || *relative.begin()==L".." || active.filename()!=L"Active")return false;
    gamePrefix=upper(narrow(root));
    for(auto& entry:std::filesystem::recursive_directory_iterator(active,error)) {
        if(error)return false;
        if(entry.is_symlink() || (GetFileAttributesW(entry.path().c_str())&FILE_ATTRIBUTE_REPARSE_POINT))return false;
        if(!entry.is_regular_file())continue;
        auto path=entry.path().wstring();
        wchar_t shortPath[32768]{};
        if(GetShortPathNameW(path.c_str(),shortPath,32768))path=shortPath;
        BOOL substituted=FALSE;
        int length=WideCharToMultiByte(CP_ACP,WC_NO_BEST_FIT_CHARS,path.c_str(),-1,nullptr,0,nullptr,&substituted);
        if(length<=0 || length>256 || substituted){log("Asset path exceeds native filename limits");return false;}
        std::string disk(length,0);
        WideCharToMultiByte(CP_ACP,WC_NO_BEST_FIT_CHARS,path.c_str(),-1,disk.data(),length,nullptr,&substituted);
        if(substituted)return false;disk.pop_back();
        overrides.emplace(upper(entry.path().lexically_relative(active).generic_string()),std::move(disk));
    }
    if(overrides.empty()){log("No enabled legacy assets");return true;}
    if(!openHook.install(reinterpret_cast<void*>(0x4fa3f0),openAsset,openSignature,sizeof(openSignature)) ||
       !existsHook.install(reinterpret_cast<void*>(0x4fa610),existsAsset,existsSignature,sizeof(existsSignature)) ||
       !sizeHook.install(reinterpret_cast<void*>(0x4fa690),sizeAsset,sizeSignature,sizeof(sizeSignature)) ||
       !bufferHook.install(reinterpret_cast<void*>(0x4fd5d0),bufferAsset,bufferSignature,sizeof(bufferSignature))) {
        bufferHook.remove();sizeHook.remove();existsHook.remove();openHook.remove();overrides.clear();
        log("Asset hooks failed; native assets retained");return false;
    }
    log("Legacy asset overrides installed; original DAT files unchanged");return true;
} catch(const std::exception& error) {
    bufferHook.remove();sizeHook.remove();existsHook.remove();openHook.remove();overrides.clear();
    log((std::string("Asset overrides disabled: ")+error.what()).c_str());return false;
}
