#include "mod_api.h"
#include "splash_patch.h"

namespace {
const TCSModHost* host{};
TCSModHandle mod{};
std::uint8_t* branch{};
bool enabled = true;
bool installed = false;
void log(const char* message) { if(host && host->log) host->log(mod, message); }
void __cdecl changed(TCSSettingValue value, void*) { enabled = value.integer != 0; }
bool writeBranch(const std::uint8_t* expected, const std::uint8_t* bytes) {
    if(!branch || std::memcmp(branch, expected, sizeof(splash::original))) return false;
    DWORD protection{};
    if(!VirtualProtect(branch, sizeof(splash::original), PAGE_EXECUTE_READWRITE, &protection)) return false;
    std::memcpy(branch, bytes, sizeof(splash::original));
    FlushInstructionCache(GetCurrentProcess(), branch, sizeof(splash::original));
    DWORD unused{};
    VirtualProtect(branch, sizeof(splash::original), protection, &unused);
    return true;
}
}

TCS_MOD_EXPORT BOOL __cdecl TCSMod_Initialize(const TCSModHost* api, TCSModHandle* out) {
    if(!api || !out || api->apiVersion != TCS_MOD_API_VERSION ||
       api->structSize < sizeof(TCSModHost) || !api->registerMod || !api->registerSetting) return FALSE;
    *out = nullptr;
    if(installed || mod) return FALSE;
    host = api;
    auto image = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if(dos->e_magic != IMAGE_DOS_SIGNATURE) return FALSE;
    auto pe = reinterpret_cast<const IMAGE_NT_HEADERS*>(image + dos->e_lfanew);
    if(pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
       pe->OptionalHeader.ImageBase != 0x400000 || reinterpret_cast<std::uintptr_t>(image) != 0x400000 ||
       !splash::matches(image, pe->OptionalHeader.SizeOfImage)) {
        log("Skip Splash: unsupported executable or conflicting startup patch; nothing changed.");
        host = nullptr;
        return FALSE;
    }
    TCSModInfo info{sizeof(TCSModInfo), "tcs.skipsplash", "TCS Skip Splash", "1.0.0"};
    mod = host->registerMod(&info);
    if(!mod) { host = nullptr; return FALSE; }
    enabled = true;
    TCSSettingDescriptor setting{};
    setting.structSize = sizeof(setting);
    setting.category = "Startup";
    setting.key = "Enabled";
    setting.label = "Skip splash (restart)";
    setting.type = TCS_SETTING_BOOL;
    setting.defaultValue.integer = 1;
    setting.maximum = 1;
    setting.step = 1;
    setting.changed = changed;
    if(!host->registerSetting(mod, &setting)) { mod = nullptr; host = nullptr; return FALSE; }
    branch = image + splash::branchRva;
    if(enabled && !(installed = writeBranch(splash::original, splash::replacement))) {
        log("Skip Splash: could not apply startup branch; nothing changed.");
        branch = nullptr; mod = nullptr; host = nullptr;
        return FALSE;
    }
    log(enabled ? "Skip Splash: native synchronous startup enabled; splash sequence bypassed, asset loading preserved."
                : "Skip Splash: disabled; normal startup retained.");
    *out = mod;
    return TRUE;
}

TCS_MOD_EXPORT void __cdecl TCSMod_Shutdown(TCSModHandle) {
    if(installed) writeBranch(splash::replacement, splash::original);
    installed = false; branch = nullptr; mod = nullptr; host = nullptr;
}
