#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include "mod_api.h"

static int registeredSettings=0;
static int videoSettings=0,enumSettings=0;
static bool deviceCallbacks=false,windowCallback=false;
static bool aimBotSetting=false,styleSetting=false,fovSetting=false;
static TCSModInfo registeredInfo{};
static TCSModHandle __cdecl RegisterMod(const TCSModInfo* info) {
    if(!info||info->structSize<sizeof(TCSModInfo))return nullptr;
    registeredInfo=*info;return reinterpret_cast<void*>(1);
}
static BOOL __cdecl RegisterSetting(TCSModHandle mod,const TCSSettingDescriptor* setting) {
    if(mod!=reinterpret_cast<void*>(1)||!setting||setting->structSize<sizeof(TCSSettingDescriptor)||!setting->key||!setting->changed)return FALSE;
    if(setting->category&&std::strcmp(setting->category,"Video")==0)++videoSettings;
    if(setting->type==TCS_SETTING_ENUM){if(!setting->choices||setting->choiceCount<2)return FALSE;++enumSettings;}
    if(std::strcmp(setting->key,"AimBot")==0) aimBotSetting=setting->type==TCS_SETTING_BOOL && setting->defaultValue.integer==0;
    if(std::strcmp(setting->key,"FirstPersonStyle")==0) styleSetting=setting->type==TCS_SETTING_ENUM && setting->choiceCount==2 &&
        std::strcmp(setting->choices[0],"Normal")==0 && std::strcmp(setting->choices[1],"Animated")==0;
    if(std::strcmp(setting->key,"HorizontalFOV")==0) fovSetting=setting->type==TCS_SETTING_FLOAT && setting->minimum==60 && setting->maximum==120;
    ++registeredSettings;setting->changed(setting->defaultValue,setting->context);return TRUE;
}
static void __cdecl RegisterDevice(TCSModHandle,TCSDeviceCreated created,TCSDeviceReset reset,TCSEndScene endScene,void*) {
    deviceCallbacks=created&&reset&&endScene;
}
static void __cdecl RegisterWindow(TCSModHandle,TCSWindowMessage message,void*) { windowCallback=message!=nullptr; }
static BOOL __cdecl ConfigPath(TCSModHandle,wchar_t*,unsigned int) { return FALSE; }
static void __cdecl Log(TCSModHandle,const char*) { }
static BOOL __cdecl SetVideoMode(int,int,int,int) { return TRUE; }
static void __cdecl SetFrameCap(int) { }

int wmain(int argc,wchar_t** argv) {
    if(argc<2||argc>3)return 2;
    HMODULE plugin=LoadLibraryW(argv[1]);if(!plugin)return 3;
    auto initialize=reinterpret_cast<TCSModInitialize>(GetProcAddress(plugin,"TCSMod_Initialize"));
    if(!initialize){FreeLibrary(plugin);return 4;}
    TCSModHost host{TCS_MOD_API_VERSION,sizeof(TCSModHost),RegisterMod,RegisterSetting,RegisterDevice,RegisterWindow,ConfigPath,Log,SetVideoMode,SetFrameCap};
    TCSModHandle handle{};
    if(initialize(&host,nullptr))return 5;
    auto incompatible=host;incompatible.apiVersion=TCS_MOD_API_VERSION+1;
    if(initialize(&incompatible,&handle))return 6;
    incompatible=host;incompatible.structSize=8;
    if(initialize(&incompatible,&handle))return 7;
    BOOL ok=initialize(&host,&handle);
    bool first=argc==2;
    bool pass=ok&&handle==reinterpret_cast<void*>(1)&&registeredInfo.id&&
      (first?(std::strcmp(registeredInfo.id,"tcs.firstperson")==0&&registeredSettings==24&&enumSettings==1&&aimBotSetting&&styleSetting&&fovSetting&&deviceCallbacks&&windowCallback):
             (std::strcmp(registeredInfo.id,"tcs.modernoptions")==0&&registeredSettings==4&&videoSettings==4&&enumSettings==3));
    auto shutdown=reinterpret_cast<TCSModShutdown>(GetProcAddress(plugin,"TCSMod_Shutdown"));if(shutdown)shutdown(handle);
    FreeLibrary(plugin);
    if(!pass){std::fprintf(stderr,"plugin registration contract failed: %d settings\n",registeredSettings);return 1;}
    return 0;
}
