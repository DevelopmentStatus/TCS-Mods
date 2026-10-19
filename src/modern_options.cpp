#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include "mod_api.h"

namespace {
const TCSModHost* host{};
TCSModHandle mod{};
int width=1280,height=720,mode=0,vsync=1,frameCap=0;
constexpr int resolutions[][2]={{640,480},{800,600},{1024,768},{1280,720},{1366,768},{1600,900},{1920,1080},{2560,1440}};
const char* const resolutionNames[]={"640 x 480","800 x 600","1024 x 768","1280 x 720","1366 x 768","1600 x 900","1920 x 1080","2560 x 1440"};
const char* const windowModes[]={"Windowed","Borderless","Exclusive fullscreen"};
const char* const frameCaps[]={"Unlimited","30 FPS","60 FPS","120 FPS","144 FPS","240 FPS"};
int enumIndex(const int values[],int count,int value){for(int i=0;i<count;++i)if(values[i]==value)return i;return 0;}
void apply(){if(host&&host->setVideoMode)host->setVideoMode(width,height,mode,vsync);if(host&&host->setFrameCap)host->setFrameCap(frameCap);}
void __cdecl changed(TCSSettingValue value,void* context){switch(static_cast<int>(reinterpret_cast<intptr_t>(context))){case 0:{int i=std::clamp(value.integer,0,7);width=resolutions[i][0];height=resolutions[i][1];apply();break;}case 1:mode=std::clamp(value.integer,0,2);apply();break;case 2:vsync=value.integer?1:0;apply();break;case 3:{constexpr int caps[]={0,30,60,120,144,240};frameCap=caps[std::clamp(value.integer,0,5)];apply();break;}}}
void add(const char* key,const char* label,TCSSettingType type,int def,int min,int max,const char* const* choices,unsigned int count,int id){TCSSettingDescriptor d{};d.structSize=sizeof(d);d.category="Video";d.key=key;d.label=label;d.type=type;d.defaultValue.integer=def;d.minimum=static_cast<float>(min);d.maximum=static_cast<float>(max);d.step=1;d.changed=changed;d.context=reinterpret_cast<void*>(static_cast<intptr_t>(id));d.choices=choices;d.choiceCount=count;host->registerSetting(mod,&d);}
}

TCS_MOD_EXPORT BOOL __cdecl TCSMod_Initialize(const TCSModHost* api,TCSModHandle* out){
    if(!api||api->apiVersion!=TCS_MOD_API_VERSION||api->structSize<sizeof(TCSModHost)||!out)return FALSE;
    host=api;TCSModInfo info{sizeof(TCSModInfo),"tcs.modernoptions","TCS Modern Options","0.4.0"};mod=host->registerMod(&info);if(!mod)return FALSE;*out=mod;
    add("Resolution","Resolution",TCS_SETTING_ENUM,3,0,7,resolutionNames,8,0);
    add("WindowMode","Window mode",TCS_SETTING_ENUM,0,0,2,windowModes,3,1);
    add("VSync","Vertical sync",TCS_SETTING_BOOL,1,0,1,nullptr,0,2);
    add("FrameCap","Frame cap",TCS_SETTING_ENUM,0,0,5,frameCaps,6,3);
    return TRUE;
}
TCS_MOD_EXPORT void __cdecl TCSMod_Shutdown(TCSModHandle){host=nullptr;mod=nullptr;}
