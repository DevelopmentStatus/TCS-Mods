#define NOMINMAX
#include "mod_api.h"
#include "ally_detour.h"
#include "shop_ownership.h"
#include <cstdio>

namespace {
const TCSModHost* host{};
TCSModHandle mod{};
AllyDetour updateHook;
shop_ownership::Sync state;
bool enabled=true;
bool readable(const void* pointer,std::size_t bytes,bool writable=false) {
    if(!pointer||!bytes)return false;
    MEMORY_BASIC_INFORMATION memory{};
    if(!VirtualQuery(pointer,&memory,sizeof(memory))||memory.State!=MEM_COMMIT||
       (memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    const auto start=reinterpret_cast<std::uintptr_t>(pointer);
    const auto end=reinterpret_cast<std::uintptr_t>(memory.BaseAddress)+memory.RegionSize;
    if(start>end||bytes>end-start)return false;
    return !writable||(memory.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}
void __cdecl changed(TCSSettingValue value,void*) {enabled=value.integer!=0;}
void __cdecl update() {
    reinterpret_cast<void(__cdecl*)()>(updateHook.bridge)();
    // Called only by native shop initialization/update, on the game thread.
    // There is no background scan of potentially freed shop allocations.
    auto count=*reinterpret_cast<const std::uint16_t*>(0x855efe);
    auto items=*reinterpret_cast<shop_ownership::Item**>(0x880478);
    auto ownership=*reinterpret_cast<const std::uint8_t**>(0x93b288);
    auto characters=*reinterpret_cast<const std::uint32_t*>(0x93b278);
    auto bought=reinterpret_cast<const std::uint32_t*>(0x86e4a8);
    if(count==0||count>shop_ownership::capacity||characters==0||characters>4096||
       !readable(items,count*sizeof(*items),true)||!readable(ownership,characters)||!readable(bought,16))return;
    auto fixed=state.apply(items,count,ownership,characters,bought,enabled);
    if(fixed&&host&&host->log) {
        char message[160];sprintf_s(message,"Shop Ownership Fix: corrected %u already-owned character shop entries; save and studs unchanged.",fixed);
        host->log(mod,message);
    }
}
bool supported() {
    auto image=reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if(image!=reinterpret_cast<void*>(0x400000)||!readable(image,0x1000))return false;
    auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>0xf00)return false;
    auto pe=reinterpret_cast<const IMAGE_NT_HEADERS32*>(image+dos->e_lfanew);
    auto site=reinterpret_cast<const void*>(shop_ownership::updateAddress);
    return pe->Signature==IMAGE_NT_SIGNATURE&&pe->FileHeader.Machine==IMAGE_FILE_MACHINE_I386&&
        pe->FileHeader.TimeDateStamp==0x4ac4a20b&&pe->OptionalHeader.SizeOfImage==0x268e000&&
        readable(site,sizeof(shop_ownership::updateSignature))&&
        !std::memcmp(site,shop_ownership::updateSignature,sizeof(shop_ownership::updateSignature));
}
}
TCS_MOD_EXPORT BOOL __cdecl TCSMod_Initialize(const TCSModHost* api,TCSModHandle* out) {
    if(!out)return FALSE;*out=nullptr;
    if(!api||api->apiVersion!=TCS_MOD_API_VERSION||api->structSize<sizeof(*api)||
       !api->registerMod||!api->registerSetting||!api->log||mod||updateHook.site)return FALSE;
    if(!supported())return FALSE;
    host=api;enabled=true;state={};
    TCSModInfo info{sizeof(info),"tcs.shopownership","Shop Ownership Fix","1.0.0"};
    mod=host->registerMod(&info);
    if(!mod){host=nullptr;return FALSE;}
    TCSSettingDescriptor setting{};
    setting.structSize=sizeof(setting);setting.category="Shop";setting.key="Enabled";
    setting.label="Correct owned characters in shop";setting.type=TCS_SETTING_BOOL;
    setting.defaultValue.integer=1;setting.maximum=1;setting.step=1;setting.changed=changed;
    if(!host->registerSetting(mod,&setting)||
       !updateHook.install(reinterpret_cast<void*>(shop_ownership::updateAddress),update,
                           shop_ownership::updateSignature,sizeof(shop_ownership::updateSignature))) {
        host->log(mod,"Shop Ownership Fix: registration/hook failed; native shop retained.");
        mod=nullptr;host=nullptr;return FALSE;
    }
    host->log(mod,"Shop Ownership Fix: native shop-update hook installed; only transient character shop flags are synchronized.");
    *out=mod;return TRUE;
}
TCS_MOD_EXPORT void __cdecl TCSMod_Shutdown(TCSModHandle) {
    // Loader removal requires a restart. Never dereference a stale shop pointer.
    if(!updateHook.remove())return;
    state={};mod=nullptr;host=nullptr;
}
