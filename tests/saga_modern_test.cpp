// Exercise the existing Modern Options DLL through the real loader host, with
// exported Saga state replaced by test storage. No game or Saga code runs.
#include "../src/loader.cpp"
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"Failed: %s line %d\n",#x,__LINE__);return 1;} } while(0)
HRESULT STDMETHODCALLTYPE fakePresent(IDirect3DDevice9*,const RECT*,const RECT*,HWND,const RGNDATA*) {return S_OK;}
int wmain(int argc,wchar_t** argv) {
    CHECK(argc==2);
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    auto folder=std::filesystem::path(executable).parent_path()/(L"saga-modern-test-"+std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(folder);root=folder.wstring()+L"\\";
    int mode=0,sync=0,cap=0;static int capCalls=0;
    saga_compat::active=true;saga_compat::video={&mode,&sync,&cap,+[]{},+[]{++capCalls;}};
    CHECK(!saga_compat::suppressedPlugin(L"tcs.modernoptions"));
    HMODULE plugin=LoadLibraryW(argv[1]);CHECK(plugin);
    auto init=reinterpret_cast<TCSModInitialize>(GetProcAddress(plugin,"TCSMod_Initialize"));CHECK(init);
    TCSModHandle handle{};CHECK(init(&host,&handle));
    CHECK(plugins.size()==1 && plugins[0]->id=="tcs.modernoptions" && videoOptionsLoaded);
    CHECK(mode==1 && sync==1 && cap==0 && capCalls>0); // Saved/default windowed mode applies at startup.
    for(auto& setting:plugins[0]->settings)if(setting.key=="WindowMode") {
        setting.value.integer=1;saveSetting(&setting);CHECK(mode==2);
        setting.value.integer=0;saveSetting(&setting);CHECK(mode==1);
    }
    for(auto& setting:plugins[0]->settings)if(setting.key=="FrameCap") {
        setting.value.integer=3;saveSetting(&setting);CHECK(cap==120);
    }
    D3DPRESENT_PARAMETERS parameters{};parameters.BackBufferWidth=1033;parameters.BackBufferHeight=777;
    applyStyle(&parameters);CHECK(parameters.BackBufferWidth==1033 && parameters.BackBufferHeight==777);
    CHECK(parameters.Windowed==TRUE && parameters.BackBufferFormat==D3DFMT_UNKNOWN);
    // Present may schedule a future change but must not execute Saga's reset.
    static int resetCalls=0;saga_compat::video.window=+[]{++resetCalls;};
    device=reinterpret_cast<IDirect3DDevice9*>(1);realPresent=fakePresent;
    pendingApply=true;
    CHECK(hookPresent(device,nullptr,nullptr,nullptr,nullptr)==S_OK);
    CHECK(resetCalls==0 && pendingApply); // Even with a live-device state, Present must not reset.
    pendingApply=false;device=nullptr;
    CHECK(!videoPending && !pendingApply); // No device yet: don't start a false confirmation timeout.
    return 0;
}
