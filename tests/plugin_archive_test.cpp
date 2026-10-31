// End-to-end: explicit preparation, then plugin loading with no helper installed.
#include "../src/loader.cpp"
#include <filesystem>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"Failed: %s line %d\n",#x,__LINE__);return 1;} } while(0)
int wmain(int argc,wchar_t** argv) {
    CHECK(argc==3);
    namespace fs=std::filesystem;
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    const auto folder=fs::path(executable).parent_path()/(L"plugin-extraction-test-"+std::to_wstring(GetCurrentProcessId()));
    fs::create_directories(folder/L"Mods");root=folder.wstring()+L"\\";
    fs::copy(fs::path(argv[1]).parent_path(),folder,fs::copy_options::recursive|fs::copy_options::overwrite_existing);
    const auto archive=folder/L"Mods/ModernOptions.zip";
    fs::copy_file(argv[2],archive);
    std::wstring prepared;
    CHECK(!preparedPlugin(archive.wstring(),L"ModernOptions",prepared));
    const auto helper=folder/L"TCSAssetManager.exe";
    std::wstring command=L"\""+helper.wstring()+L"\" --game \""+folder.wstring()+L"\"";
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
    CHECK(CreateProcessW(helper.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,folder.c_str(),&startup,&child));
    CHECK(WaitForSingleObject(child.hProcess,120000)==WAIT_OBJECT_0);
    DWORD code=1;CHECK(GetExitCodeProcess(child.hProcess,&code));
    CloseHandle(child.hProcess);CloseHandle(child.hThread);CHECK(code==0);
    CHECK(preparedPlugin(archive.wstring(),L"ModernOptions",prepared));
    fs::remove(helper);
    CHECK(WritePrivateProfileStringW(L"Mod",L"LoadPriority",L"-100",(prepared+L"\\manifest.ini").c_str()));
    loadPlugins();
    CHECK(plugins.size()==1 && plugins[0]->id=="tcs.modernoptions" && videoOptionsLoaded);
    CHECK(plugins[0]->packageFolder.find(L"ModernOptions\\")!=std::wstring::npos);
    CHECK(pluginAvailable("tcs.modernoptions")&&!pluginAvailable("tcs.character-shop-extender"));
    auto module=plugins[0]->module;plugins.clear();FreeLibrary(module);
    {std::ofstream change(archive,std::ios::binary|std::ios::app);change.put('x');}
    CHECK(!preparedPlugin(archive.wstring(),L"ModernOptions",prepared));
    fs::remove_all(folder);
    return 0;
}
