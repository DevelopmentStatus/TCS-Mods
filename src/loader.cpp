#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <d3d9.h>
#include <algorithm>
#include <string>
#include <vector>
#include <memory>
#include <cstdio>
#include <cmath>
#include <climits>
#include <share.h>
#include <mmsystem.h>
#include <shellapi.h>
#include "mod_api.h"
#include "native_pause_menu.h"
#include "pause_menu.h"
#include "asset_overlay.h"

#pragma comment(lib,"winmm.lib")
namespace {
struct Plugin;
struct Setting { Plugin* owner{}; std::string category,key,label; TCSSettingType type{}; TCSSettingValue value{},def{}; float lo{},hi{},step{}; TCSSettingChanged changed{}; void* context{}; std::vector<std::string> choices; };
struct Plugin { HMODULE module{}; std::string id,name,version; std::wstring config,packageFolder; TCSModShutdown shutdown{}; TCSDeviceCreated created{}; TCSDeviceReset reset{}; TCSEndScene endScene{}; TCSWindowMessage message{}; void* context{}; std::vector<Setting> settings; };
std::vector<std::unique_ptr<Plugin>> plugins;
std::wstring root, optionsConfig;
std::wstring loadingPackageFolder;
FILE* logFile{};
HWND gameWindow{};
IDirect3DDevice9* device{};
IDirect3DDevice9* hookedDevice{};
D3DPRESENT_PARAMETERS presentation{};
HMODULE realD3D{};
using CreateDeviceFn=HRESULT(STDMETHODCALLTYPE*)(IDirect3D9*,UINT,D3DDEVTYPE,HWND,DWORD,D3DPRESENT_PARAMETERS*,IDirect3DDevice9**);
using ResetFn=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
using EndSceneFn=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*);
using PresentFn=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,const RECT*,const RECT*,HWND,const RGNDATA*);
CreateDeviceFn realCreateDevice{}; ResetFn realReset{}; EndSceneFn realEndScene{}; PresentFn realPresent{};
int width=1280,height=720,windowMode=0,activeWindowMode=0,vsync=1,fpsCap=0,selectedRow=0,category=0;
bool menuOpen=false, videoPending=false, pendingApply=false, revertingVideo=false, videoOptionsLoaded=false;
bool captureKey=false;
Plugin* selectedPlugin=nullptr;
HRESULT lastResetResult=S_OK;
bool nativeResetInProgress=false;
DWORD confirmDeadline{};
int priorWidth=1280,priorHeight=720,priorMode=0,priorVsync=1,priorFpsCap=0;
DWORD lastFrame{};
std::wstring iniPath(const wchar_t* file) { return root+file; }
void log(const char* s) { if(logFile){ std::fprintf(logFile,"%s\n",s); std::fflush(logFile); } }
void writeOptions() {} // Per-mod setting values are persisted by saveSetting().
int readInt(const wchar_t* sec,const wchar_t* key,int def,int lo,int hi,const std::wstring& path) {
    return std::clamp(static_cast<int>(GetPrivateProfileIntW(sec,key,def,path.c_str())),lo,hi);
}
std::wstring configFor(const std::string& id) {
    std::wstring folder=root+L"Mods\\Config\\"; CreateDirectoryW((root+L"Mods").c_str(),nullptr); CreateDirectoryW(folder.c_str(),nullptr);
    std::wstring name; for(unsigned char c:id) name.push_back((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'?c:L'_');
    return folder+name+L".ini";
}
std::wstring toWide(const char* text) { if(!text)return L""; int n=MultiByteToWideChar(CP_UTF8,0,text,-1,nullptr,0); std::wstring v(n?static_cast<size_t>(n):0,L'\0'); if(n)MultiByteToWideChar(CP_UTF8,0,text,-1,v.data(),n); if(!v.empty())v.pop_back(); return v; }
std::string copyText(const char* s) { return s?std::string(s):std::string(); }
std::wstring keyName(int key) {
    if(!key)return L"Unbound";
    if(key==VK_LBUTTON)return L"Mouse 1";if(key==VK_RBUTTON)return L"Mouse 2";if(key==VK_MBUTTON)return L"Mouse 3";if(key==VK_XBUTTON1)return L"Mouse 4";if(key==VK_XBUTTON2)return L"Mouse 5";
    UINT scan=MapVirtualKeyW(static_cast<UINT>(key),MAPVK_VK_TO_VSC);wchar_t name[64]{};int count=GetKeyNameTextW(static_cast<LONG>(scan<<16),name,64);return count?std::wstring(name,count):L"Key "+std::to_wstring(key);
}
TCSModHandle __cdecl apiRegisterMod(const TCSModInfo* info) {
    if(!info||info->structSize<sizeof(TCSModInfo)||!info->id||!info->name||!info->version)return nullptr;
    for(auto& p:plugins) if(p->id==info->id)return nullptr;
    auto p=std::make_unique<Plugin>(); p->id=info->id;p->name=info->name;p->version=info->version;p->config=configFor(p->id);p->packageFolder=loadingPackageFolder;
    auto raw=p.get(); plugins.push_back(std::move(p));
    log(("Loaded plugin: "+raw->name+" "+raw->version).c_str()); return raw;
}
BOOL __cdecl apiRegisterSetting(TCSModHandle h,const TCSSettingDescriptor* d) {
    auto p=static_cast<Plugin*>(h); if(!p||!d||d->structSize<sizeof(TCSSettingDescriptor)||!d->key||!d->label||!d->category||d->type<TCS_SETTING_BOOL||d->type>TCS_SETTING_ENUM||std::strlen(d->key)>200||std::strlen(d->label)>200||std::strlen(d->category)>100||!std::isfinite(d->minimum)||!std::isfinite(d->maximum)||!std::isfinite(d->step)||d->minimum>d->maximum||(d->type!=TCS_SETTING_FLOAT&&(d->minimum<INT_MIN||d->maximum>INT_MAX))||(d->type==TCS_SETTING_ENUM&&(!d->choices||d->choiceCount==0||d->choiceCount>128)))return FALSE;
    Setting s; s.owner=p;s.category=copyText(d->category);s.key=copyText(d->key);s.label=copyText(d->label);s.type=d->type;s.value=s.def=d->defaultValue;s.lo=d->minimum;s.hi=d->maximum;s.step=d->step;s.changed=d->changed;s.context=d->context;
    for(const auto& old:p->settings)if(old.key==s.key)return FALSE;
    wchar_t key[256]; MultiByteToWideChar(CP_UTF8,0,d->key,-1,key,256);
    if(s.type==TCS_SETTING_FLOAT) { if(!std::isfinite(s.value.real))s.value.real=0;s.value.real=std::clamp(s.value.real,s.lo,s.hi);wchar_t b[64];GetPrivateProfileStringW(L"Settings",key,L"",b,64,p->config.c_str());if(b[0]){float f=static_cast<float>(_wtof(b));if(std::isfinite(f))s.value.real=std::clamp(f,s.lo,s.hi);} }
    else { int val=GetPrivateProfileIntW(L"Settings",key,INT_MIN,p->config.c_str());if(val!=INT_MIN)s.value.integer=val;if(s.type==TCS_SETTING_BOOL)s.value.integer=s.value.integer?1:0;else s.value.integer=std::clamp(s.value.integer,static_cast<int>(s.lo),static_cast<int>(s.hi)); }
    if(s.type==TCS_SETTING_ENUM){for(unsigned int i=0;i<d->choiceCount;++i){if(!d->choices[i]||std::strlen(d->choices[i])>128)return FALSE;}s.choices.assign(d->choices,d->choices+d->choiceCount);s.hi=static_cast<float>(d->choiceCount-1);s.value.integer=std::clamp(s.value.integer,0,static_cast<int>(d->choiceCount-1));}
    p->settings.push_back(s); if(s.category=="Video")videoOptionsLoaded=true;if(s.changed)s.changed(s.value,s.context); return TRUE;
}
void __cdecl apiRegisterDevice(TCSModHandle h,TCSDeviceCreated c,TCSDeviceReset r,TCSEndScene e,void* ctx) { auto p=static_cast<Plugin*>(h);if(p){p->created=c;p->reset=r;p->endScene=e;p->context=ctx;} }
void __cdecl apiRegisterWindow(TCSModHandle h,TCSWindowMessage m,void* ctx) { auto p=static_cast<Plugin*>(h);if(p){p->message=m;p->context=ctx;} }
BOOL __cdecl apiConfigPath(TCSModHandle h,wchar_t* out,unsigned int n) { auto p=static_cast<Plugin*>(h);if(!p||!out||!n)return FALSE;if(GetFileAttributesW(p->config.c_str())==INVALID_FILE_ATTRIBUTES&&!p->packageFolder.empty()){auto defaults=p->packageFolder+L"\\default.ini";if(GetFileAttributesW(defaults.c_str())!=INVALID_FILE_ATTRIBUTES)CopyFileW(defaults.c_str(),p->config.c_str(),TRUE);}return wcscpy_s(out,n,p->config.c_str())==0; }
void __cdecl apiLog(TCSModHandle,const char* s) { log(s?s:""); }

void applyWindow(D3DPRESENT_PARAMETERS* p) {
    if(activeWindowMode==2) { p->Windowed=FALSE;p->BackBufferFormat=D3DFMT_X8R8G8B8;p->FullScreen_RefreshRateInHz=0; }
    else { p->Windowed=TRUE;p->BackBufferFormat=D3DFMT_UNKNOWN;p->FullScreen_RefreshRateInHz=0; }
    p->BackBufferWidth=static_cast<UINT>(width);p->BackBufferHeight=static_cast<UINT>(height);
    p->PresentationInterval=vsync?D3DPRESENT_INTERVAL_ONE:D3DPRESENT_INTERVAL_IMMEDIATE;
}
void setWindowStyle() {
    if(!gameWindow)return;
    if(activeWindowMode==0) { SetWindowLongPtrW(gameWindow,GWL_STYLE,WS_OVERLAPPEDWINDOW|WS_VISIBLE);RECT r{0,0,width,height};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);SetWindowPos(gameWindow,nullptr,80,80,r.right-r.left,r.bottom-r.top,SWP_NOZORDER|SWP_FRAMECHANGED|SWP_SHOWWINDOW); }
    else { MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(gameWindow,MONITOR_DEFAULTTONEAREST),&mi);SetWindowLongPtrW(gameWindow,GWL_STYLE,WS_POPUP|WS_VISIBLE);SetWindowPos(gameWindow,HWND_TOP,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right-mi.rcMonitor.left,mi.rcMonitor.bottom-mi.rcMonitor.top,SWP_FRAMECHANGED|SWP_SHOWWINDOW); }
}
void saveSetting(Setting* s);
void notifyReset(bool before) { for(auto& p:plugins) if(p->reset)p->reset(device,before?1:0,p->context); }
void restoreVideoSettings();
bool nativeVideoAvailable(){
    const unsigned char signature[]={0x83,0xec,0x08,0x53,0x55,0x56,0x8b,0xf1};
    return GetModuleHandleW(nullptr)==reinterpret_cast<HMODULE>(0x400000)&&
        std::memcmp(reinterpret_cast<void*>(0x6e3940),signature,sizeof(signature))==0&&
        *reinterpret_cast<IDirect3DDevice9**>(0x29765cc)==device;
}
void applyPending() {
    if(!pendingApply||!device||nativeResetInProgress)return;
    pendingApply=false;
    if(!nativeVideoAvailable()){
        log("Display change rejected: native renderer identity is not supported");
        width=priorWidth;height=priorHeight;windowMode=priorMode;activeWindowMode=windowMode;
        vsync=priorVsync;fpsCap=priorFpsCap;videoPending=false;restoreVideoSettings();return;
    }
    auto renderer=reinterpret_cast<unsigned char*>(0x29765c8);
    auto count=*reinterpret_cast<unsigned int*>(renderer+0x618);
    auto modes=*reinterpret_cast<const unsigned int**>(renderer+0x61c);
    if(!modes||count==0||count>512){log("Display change rejected: invalid native mode table");videoPending=false;restoreVideoSettings();return;}
    unsigned int modeIndex=*reinterpret_cast<unsigned int*>(renderer+0x660);
    for(unsigned int i=0;i<count;++i)if(modes[i*4]==static_cast<unsigned int>(width)&&modes[i*4+1]==static_cast<unsigned int>(height)){
        modeIndex=i;if(modes[i*4+2]==60)break;
    }
    if(modeIndex>=count){log("Display change rejected: invalid native mode index");videoPending=false;restoreVideoSettings();return;}
    *reinterpret_cast<unsigned int*>(renderer+0x660)=modeIndex;
    renderer[0x65c]=activeWindowMode==2?0:1;
    // This routine releases game-owned default-pool resources, performs Reset,
    // updates the viewport/dimensions, and rebuilds render targets and state.
    // Calling Reset alone leaves those resources alive and the device unusable.
    nativeResetInProgress=true;lastResetResult=E_FAIL;
    setWindowStyle();
    reinterpret_cast<void(__thiscall*)(void*,int)>(0x6e3940)(renderer,0);
    nativeResetInProgress=false;
    if(SUCCEEDED(lastResetResult)){
        if(!videoPending)restoreVideoSettings();
        log("Display change completed through native renderer lifecycle");
        confirmDeadline=GetTickCount()+15000;
    }else{
        log("Native display change failed; settings restored");
        width=priorWidth;height=priorHeight;windowMode=priorMode;activeWindowMode=windowMode;
        vsync=priorVsync;fpsCap=priorFpsCap;videoPending=false;restoreVideoSettings();
    }
}

BOOL requestVideo(int w,int h,int mode,int sync) {
    if(revertingVideo){width=w;height=h;windowMode=mode;vsync=sync;activeWindowMode=windowMode;return TRUE;}
    if(!device){width=std::clamp(w,640,7680);height=std::clamp(h,480,4320);windowMode=std::clamp(mode,0,2);activeWindowMode=windowMode;vsync=sync?1:0;pendingApply=false;return TRUE;}
    if(device&&mode==windowMode&&!pendingApply&&presentation.BackBufferWidth==static_cast<UINT>(w)&&presentation.BackBufferHeight==static_cast<UINT>(h)&&
        presentation.Windowed==(mode==2?FALSE:TRUE)&&presentation.PresentationInterval==(sync?D3DPRESENT_INTERVAL_ONE:D3DPRESENT_INTERVAL_IMMEDIATE))return TRUE;
    if(!videoPending){priorWidth=width;priorHeight=height;priorMode=windowMode;priorVsync=vsync;priorFpsCap=fpsCap;}
    width=std::clamp(w,640,7680);height=std::clamp(h,480,4320);windowMode=std::clamp(mode,0,2);activeWindowMode=windowMode;vsync=sync?1:0;
    pendingApply=true;videoPending=true;confirmDeadline=GetTickCount()+15000;writeOptions();return TRUE;
}
void restoreVideoSettings(){
    revertingVideo=true;
    constexpr int resolutions[][2]={{640,480},{800,600},{1024,768},{1280,720},{1366,768},{1600,900},{1920,1080},{2560,1440}};
    constexpr int caps[]={0,30,60,120,144,240};
    int ri=3,ci=0;for(int i=0;i<8;++i)if(resolutions[i][0]==width&&resolutions[i][1]==height)ri=i;for(int i=0;i<6;++i)if(caps[i]==fpsCap)ci=i;
    for(auto& p:plugins)if(p->id=="tcs.modernoptions")for(auto& s:p->settings){if(s.key=="Resolution")s.value.integer=ri;else if(s.key=="WindowMode")s.value.integer=windowMode;else if(s.key=="VSync")s.value.integer=vsync;else if(s.key=="FrameCap")s.value.integer=ci;else continue;saveSetting(&s);}
    revertingVideo=false;
}
void __cdecl requestFrameCap(int cap){fpsCap=std::clamp(cap,0,1000);}
TCSModHost host{TCS_MOD_API_VERSION,sizeof(TCSModHost),apiRegisterMod,apiRegisterSetting,apiRegisterDevice,apiRegisterWindow,apiConfigPath,apiLog,requestVideo,requestFrameCap};

std::vector<Setting*> visibleSettings() {
    std::vector<Setting*> out;
    if(selectedPlugin)for(auto& setting:selectedPlugin->settings)out.push_back(&setting);
    return out;
}
struct Row { std::wstring label,value; Setting* setting{}; };
std::vector<Row> rows() {
    std::vector<Row> out;
    for(auto* s:visibleSettings()) {
        std::wstring v;
        if(s->type==TCS_SETTING_BOOL)v=s->value.integer?L"On":L"Off";
        else if(s->type==TCS_SETTING_FLOAT){wchar_t b[48];swprintf_s(b,L"%.3g",s->value.real);v=b;}
        else if(s->type==TCS_SETTING_KEY){v=keyName(s->value.integer);}
        else if(s->type==TCS_SETTING_ENUM&&s->value.integer>=0&&s->value.integer<static_cast<int>(s->choices.size()))v=toWide(s->choices[s->value.integer].c_str());
        else v=std::to_wstring(s->value.integer);
        out.push_back({toWide(s->label.c_str()),v,s});
    }
    return out;
}
void saveSetting(Setting* s) {
    wchar_t key[256],v[64];MultiByteToWideChar(CP_UTF8,0,s->key.c_str(),-1,key,256);
    if(s->type==TCS_SETTING_FLOAT)swprintf_s(v,L"%.7g",s->value.real);else swprintf_s(v,L"%d",s->value.integer);
    if(s->changed)s->changed(s->value,s->context);
    if(videoPending&&!revertingVideo&&s->owner->id=="tcs.modernoptions"&&s->key!="FrameCap")return;
    WritePrivateProfileStringW(L"Settings",key,v,s->owner->config.c_str());
}
void changeRow(int direction) {
    auto list=rows();if(list.empty())return;selectedRow=std::clamp(selectedRow,0,static_cast<int>(list.size())-1);auto& r=list[selectedRow];
    if(r.setting) {
        auto& s=*r.setting;
        if(s.type==TCS_SETTING_BOOL)s.value.integer=!s.value.integer;
        else if(s.type==TCS_SETTING_FLOAT)s.value.real=std::clamp(s.value.real+direction*(s.step>0?s.step:0.1f),s.lo,s.hi);
        else if(s.type==TCS_SETTING_KEY)s.value.integer=direction>0?(s.value.integer>=VK_F12?VK_F1:s.value.integer+1):(s.value.integer<=VK_F1?VK_F12:s.value.integer-1);
        else if(s.type==TCS_SETTING_ENUM){int n=static_cast<int>(s.choices.size());s.value.integer=(s.value.integer+direction+n)%n;}
        else s.value.integer=std::clamp(s.value.integer+direction*static_cast<int>(s.step?s.step:1),static_cast<int>(s.lo),static_cast<int>(s.hi));saveSetting(&s);
    }
    
}
template<class T> T& menuField(void* menu,size_t offset){return *reinterpret_cast<T*>(static_cast<unsigned char*>(menu)+offset);}
struct NativeRow { std::wstring label; int action{},settingRow{-1}; };
int settingsPage=0;
short pauseLastRow=7;
constexpr int settingsPerPage=6;
std::vector<NativeRow> nativeRows(){
    std::vector<NativeRow> out;
    if(!selectedPlugin){
        int pages=std::max(1,(static_cast<int>(plugins.size())+settingsPerPage-1)/settingsPerPage);
        settingsPage=std::clamp(settingsPage,0,pages-1);
        if(pages>1)out.push_back({L"Page "+std::to_wstring(settingsPage+1)+L" / "+std::to_wstring(pages),2});
        int first=settingsPage*settingsPerPage;
        for(int i=first;i<static_cast<int>(plugins.size())&&i<first+settingsPerPage;++i)
            out.push_back({toWide(plugins[i]->name.c_str()),6,i});
        if(plugins.empty())out.push_back({L"No mods installed",0});
        out.push_back({L"Back to pause menu",3});
        return out;
    }
    if(videoPending){out.push_back({L"Keep display changes",4});out.push_back({L"Revert display changes",5});}
    auto list=rows();
    int pages=std::max(1,(static_cast<int>(list.size())+settingsPerPage-1)/settingsPerPage);
    settingsPage=std::clamp(settingsPage,0,pages-1);
    if(pages>1)out.push_back({L"Page "+std::to_wstring(settingsPage+1)+L" / "+std::to_wstring(pages),2});
    int first=settingsPage*settingsPerPage;
    for(int i=first;i<static_cast<int>(list.size())&&i<first+settingsPerPage;++i){
        auto& r=list[i];
        std::wstring value=captureKey&&selectedRow==i?L"Press a key (Esc cancels)":r.value;
        out.push_back({r.label+(value.empty()?L"":L": "+value),0,i});
    }
    if(list.empty())out.push_back({L"This mod has no registered settings",0});
    out.push_back({L"Back to mods",7});
    return out;
}
void selectNativeRow(void* menu,int row){
    int last=static_cast<int>(nativeRows().size())-1;
    row=std::clamp(row,0,last);
    menuField<short>(menu,0x12)=static_cast<short>(last);
    menuField<short>(menu,0x0a)=static_cast<short>(row);
    menuField<int>(menu,0x34)=row;
}
void revertDisplay(){
    width=priorWidth;height=priorHeight;windowMode=priorMode;activeWindowMode=windowMode;
    vsync=priorVsync;fpsCap=priorFpsCap;videoPending=false;pendingApply=true;
    restoreVideoSettings();
}
void closeModSettings(void* menu){
    if(videoPending)revertDisplay();
    menuOpen=false;captureKey=false;
    menuField<short>(menu,0x12)=pauseLastRow;
    menuField<short>(menu,0x0a)=tcs::modSettingsRow;
    menuField<int>(menu,0x34)=tcs::modSettingsRow;
    menuField<int>(menu,0x64)=0;menuField<int>(menu,0x68)=0;
    log("Mod Settings closed; returned to native pause menu");
}
void openModSettings(void* menu){
    if(menuOpen)return;
    pauseLastRow=menuField<short>(menu,0x12);
    menuOpen=true;captureKey=false;selectedRow=0;settingsPage=0;selectedPlugin=nullptr;
    selectNativeRow(menu,0);
    log("Mod Settings opened with native game renderer (no window or display switch)");
}
bool modSettingsOpen(){return menuOpen;}
void drawModSettings(void* menu){
    auto list=nativeRows();
    menuField<short>(menu,0x12)=static_cast<short>(list.size()-1);
    // Set both dimensions of the native font and provide explicit row positions.
    // The game's default text was taller than its row pitch in this callback.
    float oldScaleX=menuField<float>(menu,0xd0),oldScaleY=menuField<float>(menu,0x90);
    float oldPitch=menuField<float>(menu,0xd4);
    menuField<float>(menu,0xd0)=0.60f;
    menuField<float>(menu,0x90)=0.60f;
    const float pitch=-0.14f;
    float top=-pitch*static_cast<float>(list.size()-1)*0.5f;
    menuField<float>(menu,0x98)=top;
    for(auto& row:list){
        int n=WideCharToMultiByte(CP_UTF8,0,row.label.c_str(),-1,nullptr,0,nullptr,nullptr);
        std::string label(static_cast<size_t>(n),0);
        WideCharToMultiByte(CP_UTF8,0,row.label.c_str(),-1,label.data(),n,nullptr,nullptr);
        *reinterpret_cast<float*>(0x8009a8)=0.60f; // native vertical text scale
        reinterpret_cast<void(__cdecl*)(void*,const char*)>(0x574de0)(menu,label.c_str());
        top+=pitch;menuField<float>(menu,0x98)=top;
    }
    menuField<float>(menu,0xd0)=oldScaleX;menuField<float>(menu,0x90)=oldScaleY;menuField<float>(menu,0xd4)=oldPitch;
}
void updateModSettings(void* menu){
    int row=menuField<int>(menu,0x34);
    auto list=nativeRows();
    if(row<0||row>=static_cast<int>(list.size())){selectNativeRow(menu,0);return;}
    if(menuField<int>(menu,0x68)){
        if(captureKey)captureKey=false;else if(selectedPlugin){if(videoPending)revertDisplay();selectedPlugin=nullptr;settingsPage=0;selectNativeRow(menu,0);}else closeModSettings(menu);
        menuField<int>(menu,0x68)=0;return;
    }
    if(captureKey)return;
    int direction=menuField<int>(menu,0x5c)?-1:menuField<int>(menu,0x60)?1:0;
    bool confirm=menuField<int>(menu,0x64)!=0;
    if(!direction&&!confirm)return;
    auto& entry=list[row];
    if(entry.action==6&&confirm){selectedPlugin=plugins[entry.settingRow].get();settingsPage=0;selectNativeRow(menu,0);log(("Opened settings for "+selectedPlugin->name).c_str());}
    else if(entry.action==7&&confirm){if(videoPending)revertDisplay();selectedPlugin=nullptr;settingsPage=0;selectNativeRow(menu,0);}
    else if(entry.action==2){int count=static_cast<int>(selectedPlugin?rows().size():plugins.size());int pages=std::max(1,(count+settingsPerPage-1)/settingsPerPage);settingsPage=(settingsPage+(direction<0?pages-1:1))%pages;selectNativeRow(menu,row);}
    else if(entry.action==3&&confirm)closeModSettings(menu);
    else if(entry.action==4&&confirm){videoPending=false;restoreVideoSettings();selectNativeRow(menu,0);log("Display changes confirmed and saved");}
    else if(entry.action==5&&confirm){revertDisplay();selectNativeRow(menu,0);}
    else if(entry.action==0&&entry.settingRow>=0){
        selectedRow=entry.settingRow;auto settings=rows();auto* setting=settings[selectedRow].setting;
        if(setting&&setting->type==TCS_SETTING_KEY)captureKey=confirm;
        else {changeRow(direction?direction:1);selectNativeRow(menu,videoPending?0:row);}
    }
    menuField<int>(menu,0x64)=0;
}
void applyStyle(D3DPRESENT_PARAMETERS* p) { if(videoOptionsLoaded){setWindowStyle();applyWindow(p);} }
LRESULT CALLBACK GameProc(HWND h,UINT m,WPARAM w,LPARAM l) {

    if(menuOpen&&captureKey&&m==WM_KEYDOWN){
        if(w==VK_ESCAPE)captureKey=false;
        else {auto list=rows();if(selectedRow>=0&&selectedRow<static_cast<int>(list.size())&&list[selectedRow].setting){list[selectedRow].setting->value.integer=static_cast<int>(w);saveSetting(list[selectedRow].setting);}captureKey=false;}
        return 0;
    }
    BOOL handled=FALSE;LRESULT result=0;for(auto& p:plugins)if(p->message){result=p->message(h,m,w,l,&handled,p->context);if(handled)return result;}
    return CallWindowProcW(reinterpret_cast<WNDPROC>(GetPropW(h,L"TCSOriginalProc")),h,m,w,l);
}

std::wstring psQuote(const std::wstring& s){std::wstring q=L"'";for(wchar_t c:s){q.push_back(c);if(c==L'\'')q.push_back(c);}q.push_back(L'\'');return q;}
bool extractZip(const std::wstring& zip,const std::wstring& dest){
    CreateDirectoryW((root+L"Mods\\Loaded").c_str(),nullptr);CreateDirectoryW(dest.c_str(),nullptr);
    std::wstring command=L"powershell.exe -NoProfile -NonInteractive -WindowStyle Hidden -Command \"if(Test-Path -LiteralPath "+psQuote(dest)+L"){Remove-Item -LiteralPath "+psQuote(dest)+L" -Recurse -Force}; New-Item -ItemType Directory -Path "+psQuote(dest)+L" -Force | Out-Null; Expand-Archive -LiteralPath "+psQuote(zip)+L" -DestinationPath "+psQuote(dest)+L" -Force\"";
    STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;PROCESS_INFORMATION pi{};
    if(!CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,root.c_str(),&si,&pi))return false;
    DWORD wait=WaitForSingleObject(pi.hProcess,15000);DWORD code=1;if(wait==WAIT_OBJECT_0)GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return wait==WAIT_OBJECT_0&&code==0;
}
void loadZip(const std::wstring& zip,const std::wstring& filename){
    std::wstring stem=filename.substr(0,filename.size()-4);for(auto& c:stem)if(!((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'_'||c==L'-'))c=L'_';
    std::wstring dest=root+L"Mods\\Loaded\\"+stem; if(!extractZip(zip,dest)){log("Could not extract mod archive; skipping it");return;}
    std::wstring manifest=dest+L"\\manifest.ini",idBuf[1];wchar_t id[256]{},entry[260]{};
    if(GetFileAttributesW(manifest.c_str())==INVALID_FILE_ATTRIBUTES)return; // Legacy assets are handled by the importer.
    GetPrivateProfileStringW(L"Mod",L"Id",L"",id,256,manifest.c_str());GetPrivateProfileStringW(L"Mod",L"Entry",L"",entry,260,manifest.c_str());
    if(!id[0]||!entry[0]||wcschr(entry,L'/')||wcschr(entry,L'\\')){log("Invalid mod manifest; skipping archive");return;}
    std::wstring dll=dest+L"\\"+entry;HMODULE m=LoadLibraryW(dll.c_str());if(!m){log("Mod entry DLL failed to load; skipping archive");return;}
    auto init=reinterpret_cast<TCSModInitialize>(GetProcAddress(m,"TCSMod_Initialize"));if(!init)init=reinterpret_cast<TCSModInitialize>(GetProcAddress(m,"_TCSMod_Initialize"));
    if(!init){FreeLibrary(m);log("Mod has no supported initialization export");return;}
    size_t before=plugins.size();TCSModHandle h=nullptr;loadingPackageFolder=dest;BOOL initialized=init(&host,&h);loadingPackageFolder.clear();if(!initialized||!h||plugins.size()!=before+1||toWide(plugins[before]->id.c_str())!=id){plugins.resize(before);FreeLibrary(m);log("Mod initialization or manifest ID validation failed");return;}
    plugins[before]->module=m;plugins[before]->shutdown=reinterpret_cast<TCSModShutdown>(GetProcAddress(m,"TCSMod_Shutdown"));if(!plugins[before]->shutdown)plugins[before]->shutdown=reinterpret_cast<TCSModShutdown>(GetProcAddress(m,"_TCSMod_Shutdown"));
}
void loadPlugins() {
    CreateDirectoryW((root+L"Mods").c_str(),nullptr);WIN32_FIND_DATAW fd{};HANDLE find=FindFirstFileW((root+L"Mods\\*.zip").c_str(),&fd);if(find==INVALID_HANDLE_VALUE)return;
    do {if(!(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))loadZip(root+L"Mods\\"+fd.cFileName,fd.cFileName);}while(FindNextFileW(find,&fd));FindClose(find);
}
HRESULT STDMETHODCALLTYPE hookReset(IDirect3DDevice9* dev,D3DPRESENT_PARAMETERS* p){
    notifyReset(true);
    if(videoOptionsLoaded)applyWindow(p);
    HRESULT hr=realReset(dev,p);
    if(FAILED(hr)&&nativeResetInProgress){
        log("Requested Reset failed; recovering previous display mode before resources are rebuilt");
        width=priorWidth;height=priorHeight;windowMode=priorMode;activeWindowMode=windowMode;
        vsync=priorVsync;fpsCap=priorFpsCap;videoPending=false;
        applyWindow(p);setWindowStyle();hr=realReset(dev,p);
    }
    lastResetResult=hr;
    if(SUCCEEDED(hr))presentation=*p;
    notifyReset(false);return hr;
}
HRESULT STDMETHODCALLTYPE hookPresent(IDirect3DDevice9* dev,const RECT* a,const RECT* b,HWND c,const RGNDATA* e){if(videoPending&& !pendingApply && static_cast<LONG>(GetTickCount()-confirmDeadline)>=0){width=priorWidth;height=priorHeight;windowMode=priorMode;activeWindowMode=windowMode;vsync=priorVsync;fpsCap=priorFpsCap;videoPending=false;pendingApply=true;restoreVideoSettings();writeOptions();}applyPending();if(fpsCap>0){DWORD now=timeGetTime();DWORD frame=1000/static_cast<DWORD>(fpsCap);if(lastFrame&&now-lastFrame<frame)Sleep(frame-(now-lastFrame));lastFrame=timeGetTime();}static bool observed=false;if(!observed){observed=true;log("D3D9 Present hook active");}return realPresent(dev,a,b,c,e);}
HRESULT STDMETHODCALLTYPE hookEndScene(IDirect3DDevice9* dev){for(auto& p:plugins)if(p->endScene)p->endScene(dev,p->context);return realEndScene(dev);}

void deviceCreated(HWND w,IDirect3DDevice9* d) { gameWindow=w;device=d;InstallNativePauseMenu(openModSettings,modSettingsOpen,drawModSettings,updateModSettings,log);if(!hookedDevice){auto vt=*reinterpret_cast<void***>(d);DWORD old;VirtualProtect(vt+16,sizeof(void*)*27,PAGE_READWRITE,&old);realReset=reinterpret_cast<ResetFn>(vt[16]);realPresent=reinterpret_cast<PresentFn>(vt[17]);realEndScene=reinterpret_cast<EndSceneFn>(vt[42]);
        vt[16]=reinterpret_cast<void*>(hookReset);
        vt[17]=reinterpret_cast<void*>(hookPresent);
        vt[42]=reinterpret_cast<void*>(hookEndScene);
VirtualProtect(vt+16,sizeof(void*)*27,old,&old);hookedDevice=d;}
    for(auto& p:plugins)if(p->created)p->created(w,d,p->context);
    SetPropW(w,L"TCSOriginalProc",reinterpret_cast<HANDLE>(SetWindowLongPtrW(w,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(GameProc))));
}
HRESULT STDMETHODCALLTYPE hookCreateDevice(IDirect3D9* self,UINT adapter,D3DDEVTYPE type,HWND window,DWORD flags,D3DPRESENT_PARAMETERS* p,IDirect3DDevice9** out) {
    gameWindow=window;D3DPRESENT_PARAMETERS original=*p;applyStyle(p);auto hr=realCreateDevice(self,adapter,type,window,flags,p,out);
    if(FAILED(hr)){*p=original;if(videoOptionsLoaded){width=1280;height=720;windowMode=0;activeWindowMode=0;vsync=1;fpsCap=0;setWindowStyle();applyWindow(p);writeOptions();}log("Configured device creation failed; retrying the game's original display parameters");hr=realCreateDevice(self,adapter,type,window,flags,p,out);}
    if(SUCCEEDED(hr)&&out&&*out){presentation=*p;deviceCreated(window,*out);}return hr;
}
void loadCoreConfig(){optionsConfig=iniPath(L"TCSModLoader.ini");}
}

extern "C" IDirect3D9* WINAPI ProxyCreate9(UINT version) {
    static bool initialized=false;
    if(!realD3D){wchar_t path[MAX_PATH];GetSystemDirectoryW(path,MAX_PATH);wcscat_s(path,L"\\d3d9.dll");realD3D=LoadLibraryW(path);if(!realD3D)return nullptr;timeBeginPeriod(1);logFile=_fsopen("TCSModernOptions.log","w",_SH_DENYNO);wchar_t exe[MAX_PATH];GetModuleFileNameW(nullptr,exe,MAX_PATH);wchar_t* slash=wcsrchr(exe,L'\\');if(slash){slash[1]=0;root=exe;}if(!initialized){initialized=true;loadCoreConfig();PrepareAssetOverlay(root,host,log);loadPlugins();log("TCS Modern Options loader initialized");}}
    auto fn=reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(realD3D,"Direct3DCreate9"));auto d=fn(version);
    // The loader owns Mod Settings, including when no optional mods are installed.
    if(d){auto vt=*reinterpret_cast<void***>(d);DWORD old;VirtualProtect(vt+16,sizeof(void*),PAGE_READWRITE,&old);if(vt[16]!=reinterpret_cast<void*>(hookCreateDevice)){realCreateDevice=reinterpret_cast<CreateDeviceFn>(vt[16]);vt[16]=reinterpret_cast<void*>(hookCreateDevice);}VirtualProtect(vt+16,sizeof(void*),old,&old);}return d;
}
extern "C" int WINAPI ProxyBeginEvent(D3DCOLOR c,LPCWSTR text){auto fn=realD3D?GetProcAddress(realD3D,"D3DPERF_BeginEvent"):nullptr;return fn?reinterpret_cast<int(WINAPI*)(D3DCOLOR,LPCWSTR)>(fn)(c,text):-1;}
extern "C" int WINAPI ProxyEndEvent(){auto fn=realD3D?GetProcAddress(realD3D,"D3DPERF_EndEvent"):nullptr;return fn?reinterpret_cast<int(WINAPI*)()>(fn)():-1;}
