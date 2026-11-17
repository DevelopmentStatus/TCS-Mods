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
    fs::copy_file(argv[1],folder/L"TCSAssetManager.exe");
    const auto archive=folder/L"Mods/ModernOptions.zip";
    fs::copy_file(argv[2],archive);
    std::wstring prepared;
    CHECK(startupPreparationNeeded());
    CHECK(!preparedPlugin(archive.wstring(),L"ModernOptions",prepared));
    const auto helper=folder/L"TCSAssetManager.exe";
    std::wstring command=L"\""+helper.wstring()+L"\" --game \""+folder.wstring()+L"\"";
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
    auto prepareAgain=[&](){
        if(!CreateProcessW(helper.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,folder.c_str(),&startup,&child))return false;
        const auto waited=WaitForSingleObject(child.hProcess,120000);
        DWORD code=1;const auto exited=GetExitCodeProcess(child.hProcess,&code);
        CloseHandle(child.hProcess);CloseHandle(child.hThread);
        return waited==WAIT_OBJECT_0&&exited&&code==0;
    };
    CHECK(prepareAgain());
    CHECK(preparedPlugin(archive.wstring(),L"ModernOptions",prepared));
    CHECK(!startupPreparationNeeded());
    const auto catalog=folder/L"Mods/Assets/catalog.ini";
    const auto active=fs::path(startup_preparation::value(catalog.wstring(),L"Overlay",L"Path"));
    fs::rename(active,active.parent_path()/L"Active.tmp");
    CHECK(startupPreparationNeeded());
    fs::rename(active.parent_path()/L"Active.tmp",active);
    CHECK(!startupPreparationNeeded());
    fs::rename(fs::path(prepared)/L"TCSModernOptions.dll",fs::path(prepared)/L"Entry.tmp");
    CHECK(startupPreparationNeeded());
    fs::rename(fs::path(prepared)/L"Entry.tmp",fs::path(prepared)/L"TCSModernOptions.dll");
    CHECK(!startupPreparationNeeded());
    // Receipt and native FILETIME checks must agree; only asset order config
    // invalidates preparation, rather than ordinary plugin settings.
    fs::create_directories(folder/L"Mods/Config");
    const auto ordinaryConfig=folder/L"Mods/Config/tcs_modernoptions.ini";
    {std::ofstream config(ordinaryConfig);config<<"[Settings]\nFrameCap=60\n";}
    CHECK(!startupPreparationNeeded());
    const auto receipt=folder/L"Mods/prepared.ini";
    const auto assetConfig=startup_preparation::value(receipt.wstring(),L"Archive0",L"Config");
    const auto configPath=folder/L"Mods/Config"/assetConfig;
    {std::ofstream config(configPath);config<<"[Settings]\nPriority=300\n";}
    CHECK(startupPreparationNeeded());
    fs::remove(configPath);
    CHECK(!startupPreparationNeeded());
    {std::ofstream added(folder/L"Mods/added.7z");added<<"fixture";}
    CHECK(startupPreparationNeeded());fs::remove(folder/L"Mods/added.7z");
    fs::rename(archive,folder/L"Mods/Renamed.zip");
    CHECK(startupPreparationNeeded());fs::rename(folder/L"Mods/Renamed.zip",archive);
    CHECK(!startupPreparationNeeded());
    fs::rename(folder/L"Mods/Assets/catalog.ini",folder/L"Mods/Assets/catalog.tmp");
    CHECK(startupPreparationNeeded());
    fs::rename(folder/L"Mods/Assets/catalog.tmp",folder/L"Mods/Assets/catalog.ini");
    CHECK(!startupPreparationNeeded());
    // More than one receipt row must validate fully, regardless of discovery
    // order. Removing the second input must invalidate the published state.
    fs::copy_file(archive,folder/L"Mods/Second.zip");
    CHECK(prepareAgain());CHECK(!startupPreparationNeeded());
    fs::remove(folder/L"Mods/Second.zip");CHECK(startupPreparationNeeded());
    CHECK(prepareAgain());CHECK(!startupPreparationNeeded());
    fs::remove(helper);
    CHECK(WritePrivateProfileStringW(L"Mod",L"LoadPriority",L"-100",(prepared+L"\\manifest.ini").c_str()));
    loadPlugins();
    CHECK(plugins.size()==1 && plugins[0]->id=="tcs.modernoptions" && videoOptionsLoaded);
    CHECK(plugins[0]->packageFolder.find(L"ModernOptions\\")!=std::wstring::npos);
    CHECK(pluginAvailable("tcs.modernoptions")&&!pluginAvailable("tcs.character-shop-extender"));
    auto module=plugins[0]->module;plugins.clear();FreeLibrary(module);
    {std::ofstream change(archive,std::ios::binary|std::ios::app);change.put('x');}
    CHECK(!preparedPlugin(archive.wstring(),L"ModernOptions",prepared));
    CHECK(startupPreparationNeeded());
    fs::remove_all(folder);
    return 0;
}
