#include "mod_api.h"
#include <cstdio>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"Failed line %d: %s\n",__LINE__,#x);return 1;}} while(0)
static unsigned registrations;
static TCSModHandle __cdecl registerMod(const TCSModInfo*){++registrations;return reinterpret_cast<void*>(1);}
static void __cdecl log(TCSModHandle,const char*){}
int wmain(int argc,wchar_t** argv) {
    CHECK(argc==2);
    auto module=LoadLibraryW(argv[1]);CHECK(module);
    auto initialize=reinterpret_cast<TCSModInitialize>(GetProcAddress(module,"TCSMod_Initialize"));
    auto shutdown=reinterpret_cast<TCSModShutdown>(GetProcAddress(module,"TCSMod_Shutdown"));
    CHECK(initialize&&shutdown);
    TCSModHandle handle=reinterpret_cast<void*>(1);
    CHECK(!initialize(nullptr,&handle)&&!handle);
    TCSModHost host{};host.structSize=sizeof(host);host.apiVersion=TCS_MOD_API_VERSION;
    host.registerMod=registerMod;host.log=log;
    // Unsupported hosts must be rejected before registration or native patching.
    CHECK(!initialize(&host,&handle)&&!handle&&!registrations);
    CHECK(!initialize(&host,nullptr));shutdown(nullptr);
    CHECK(FreeLibrary(module));return 0;
}
