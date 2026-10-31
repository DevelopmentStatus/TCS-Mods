#define NOMINMAX
#include "mod_api.h"
#include "shop_capacity.h"

namespace {
const TCSModHost* host{};
TCSModHandle mod{};
HMODULE retainedModule{};
void note(const char* message) {if(host&&host->log)host->log(mod,message);}
}

TCS_MOD_EXPORT BOOL __cdecl TCSMod_Initialize(const TCSModHost* api,TCSModHandle* out) {
    if(!out)return FALSE;*out=nullptr;
    if(!api||api->apiVersion!=TCS_MOD_API_VERSION||api->structSize<sizeof(*api)||
       !api->registerMod||!api->log||retainedModule||mod||!shop_capacity::supportedHost())return FALSE;
    // Game tables and native callbacks reference this module for the process
    // lifetime. Retain a reference so an accidental FreeLibrary cannot strand them.
    HMODULE reference{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(&TCSMod_Initialize),&reference))return FALSE;
    host=api;
    TCSModInfo info{sizeof(info),"tcs.character-shop-extender","Character & Shop Extender","1.0.0"};
    mod=host->registerMod(&info);
    if(!mod||!shop_capacity::install(note)) {
        note("Character & Shop Extender: unsupported game, registration failure or conflicting patches.");
        host=nullptr;mod=nullptr;FreeLibrary(reference);return FALSE;
    }
    retainedModule=reference;
    note("Character & Shop Extender active: expanded roster/shop, text/portrait tables and compatible companion saves.");
    *out=mod;return TRUE;
}

TCS_MOD_EXPORT void __cdecl TCSMod_Shutdown(TCSModHandle) {
    // Restart-only: live game allocations and callbacks reference these tables.
    // Keep the module resident; removal is supported with the game closed.
}
