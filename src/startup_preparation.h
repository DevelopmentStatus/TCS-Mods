#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <cwchar>
#include <algorithm>

namespace startup_preparation {
inline std::wstring value(const std::wstring& receipt,const std::wstring& section,const wchar_t* key) {
    wchar_t text[32768]{};
    GetPrivateProfileStringW(section.c_str(),key,L"",text,32768,receipt.c_str());
    return text;
}
inline bool matches(const std::wstring& receipt,const std::wstring& section,const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    const bool exists=GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&data)!=FALSE;
    if(exists&&(data.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))return false;
    if(!exists&&GetLastError()!=ERROR_FILE_NOT_FOUND&&GetLastError()!=ERROR_PATH_NOT_FOUND)return false;
    const ULONGLONG size=exists?(static_cast<ULONGLONG>(data.nFileSizeHigh)<<32)|data.nFileSizeLow:0;
    const ULONGLONG time=exists?(static_cast<ULONGLONG>(data.ftLastWriteTime.dwHighDateTime)<<32)|data.ftLastWriteTime.dwLowDateTime:0;
    return value(receipt,section,L"Size")==std::to_wstring(size)&&
           value(receipt,section,L"WriteTime")==std::to_wstring(time);
}
inline bool archiveExtension(const wchar_t* name) {
    const auto ext=wcsrchr(name,L'.');
    return ext&&(!_wcsicmp(ext,L".zip")||!_wcsicmp(ext,L".rar")||!_wcsicmp(ext,L".7z"));
}
inline std::vector<std::wstring> archives(const std::wstring& root) {
    std::vector<std::wstring> result;WIN32_FIND_DATAW file{};
    HANDLE find=FindFirstFileW((root+L"Mods\\*").c_str(),&file);
    if(find==INVALID_HANDLE_VALUE)return result;
    do {
        if(!(file.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&archiveExtension(file.cFileName))result.push_back(file.cFileName);
    }while(FindNextFileW(find,&file));
    FindClose(find);return result;
}
inline bool receiptCurrent(const std::wstring& root) {
    auto inputs=archives(root);
    const auto receipt=root+L"Mods\\prepared.ini";
    const auto catalog=root+L"Mods\\Assets\\catalog.ini";
    if(inputs.empty()&&GetFileAttributesW(catalog.c_str())==INVALID_FILE_ATTRIBUTES)return true;
    if(GetPrivateProfileIntW(L"Preparation",L"Format",0,receipt.c_str())!=1||
       GetPrivateProfileIntW(L"Preparation",L"Count",-1,receipt.c_str())!=inputs.size())return false;
    if(!matches(receipt,L"Catalog",catalog)||
       GetPrivateProfileIntW(L"Overlay",L"Format",0,catalog.c_str())!=2||
       !matches(receipt,L"GameDat",root+L"GAME.DAT"))return false;
    const auto active=value(catalog,L"Overlay",L"Path");
    const auto attributes=GetFileAttributesW(active.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES||!(attributes&FILE_ATTRIBUTE_DIRECTORY)||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;
    const auto count=inputs.size();
    for(size_t i=0;i<count;++i) {
        const auto section=L"Archive"+std::to_wstring(i);
        const auto name=value(receipt,section,L"Name");
        auto found=std::find_if(inputs.begin(),inputs.end(),[&](const auto& input){return !_wcsicmp(input.c_str(),name.c_str());});
        if(found==inputs.end()||!matches(receipt,section,root+L"Mods\\"+*found))return false;
        inputs.erase(found); // Every receipt row must describe a different archive.
        const auto config=value(receipt,section,L"Config");
        if(config.size()!=26||config.substr(0,6)!=L"asset_"||config.substr(22)!=L".ini")return false;
        for(size_t c=6;c<22;++c)if(!((config[c]>=L'0'&&config[c]<=L'9')||(config[c]>=L'a'&&config[c]<=L'f')))return false;
        if(!matches(receipt,L"Config"+std::to_wstring(i),root+L"Mods\\Config\\"+config))return false;
    }
    return inputs.empty();
}
inline bool openManager(const std::wstring& root) {
    const auto helper=root+L"TCSAssetManager.exe";
    std::wstring command=L"\""+helper+L"\" --game \""+root+L".\" --manage --wait-for-game "+std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
    if(!CreateProcessW(helper.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,root.c_str(),&startup,&child))return false;
    CloseHandle(child.hProcess);CloseHandle(child.hThread);return true;
}
// Called only after validating the supported game, outside DllMain and before
// loading any mod DLLs or installing hooks. The manager owns the next launch.
[[noreturn]] inline void handoff(const std::wstring& root,void (*log)(const char*)) {
    if(openManager(root)) {
        if(log)log("Mods need preparation; opened Manage Mod Priorities and stopped this game startup.");
        ExitProcess(0);
    }
    if(log)log("Cannot open TCSAssetManager.exe for mod preparation.");
    MessageBoxW(nullptr,L"TCSAssetManager.exe could not be opened. Extract the complete loader ZIP beside LEGOStarWarsSaga.exe, then open Manage Mod Priorities.cmd.",
                L"TCS Mod Loader - manager unavailable",MB_OK|MB_ICONERROR);
    ExitProcess(1);
}
}
