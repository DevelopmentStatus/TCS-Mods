// Exercise the shipped Modern Options plugin through the native loader host.
#include "../src/loader.cpp"
#include <filesystem>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"Failed: %s line %d\n",#x,__LINE__);return 1;} } while(0)
int wmain(int argc,wchar_t** argv) {
    CHECK(argc==2);
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    auto folder=std::filesystem::path(executable).parent_path()/(L"modern-loader-test-"+std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(folder);root=folder.wstring()+L"\\";
    HMODULE plugin=LoadLibraryW(argv[1]);CHECK(plugin);
    auto init=reinterpret_cast<TCSModInitialize>(GetProcAddress(plugin,"TCSMod_Initialize"));CHECK(init);
    TCSModHandle handle{};CHECK(init(&host,&handle));
    CHECK(plugins.size()==1 && plugins[0]->id=="tcs.modernoptions" && videoOptionsLoaded);
    CHECK(windowMode==0 && vsync==1 && fpsCap==0);
    for(auto& setting:plugins[0]->settings)if(setting.key=="WindowMode") {
        setting.value.integer=1;saveSetting(&setting);CHECK(windowMode==1 && activeWindowMode==1);
        setting.value.integer=0;saveSetting(&setting);CHECK(windowMode==0 && activeWindowMode==0);
    }
    for(auto& setting:plugins[0]->settings)if(setting.key=="FrameCap") {
        setting.value.integer=3;saveSetting(&setting);CHECK(fpsCap==120);
    }
    D3DPRESENT_PARAMETERS parameters{};parameters.BackBufferWidth=1033;parameters.BackBufferHeight=777;
    applyStyle(&parameters);
    CHECK(parameters.BackBufferWidth==static_cast<UINT>(width) && parameters.BackBufferHeight==static_cast<UINT>(height));
    CHECK(parameters.Windowed==TRUE && parameters.BackBufferFormat==D3DFMT_UNKNOWN);
    CHECK(!videoPending && !pendingApply);
    plugins.clear();FreeLibrary(plugin);std::filesystem::remove_all(folder);
    return 0;
}
