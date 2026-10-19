// Temporary integration probe. Not included in the release loader package.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "mod_api.h"

namespace {
const TCSModHost* host{};
TCSModHandle handle{};
DWORD started{};
int phase{};
void __cdecl render(IDirect3DDevice9*,void*) {
    DWORD elapsed=GetTickCount()-started;
    if(phase==0&&elapsed>5000){
        phase=1;host->log(handle,"VIDEO PROBE: requesting 1366x768 windowed");
        host->setVideoMode(1366,768,0,1);
    }else if(phase==1&&elapsed>9000){
        int width=*reinterpret_cast<int*>(0x973900),height=*reinterpret_cast<int*>(0x973904);
        host->log(handle,width==1366&&height==768?"VIDEO PROBE: 1366x768 verified in native renderer":"VIDEO PROBE: FAIL target dimensions");
        phase=2;host->setVideoMode(1280,720,0,1);
    }else if(phase==2&&elapsed>13000){
        int width=*reinterpret_cast<int*>(0x973900),height=*reinterpret_cast<int*>(0x973904);
        host->log(handle,width==1280&&height==720?"VIDEO PROBE: 1280x720 recovery verified; render loop survived":"VIDEO PROBE: FAIL recovery dimensions");
        phase=3;
    }
}
void __cdecl created(HWND,IDirect3DDevice9*,void*){started=GetTickCount();}
}
TCS_MOD_EXPORT BOOL __cdecl TCSMod_Initialize(const TCSModHost* api,TCSModHandle* out){
    host=api;TCSModInfo info{sizeof(TCSModInfo),"tcs.runtimeprobe","Runtime Video Probe","test"};
    handle=api->registerMod(&info);*out=handle;
    api->registerDeviceCallbacks(handle,created,nullptr,render,nullptr);
    return handle!=nullptr;
}
