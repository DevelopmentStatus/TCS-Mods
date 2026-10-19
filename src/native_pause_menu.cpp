#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstring>
#include "native_pause_menu.h"
#include "pause_menu.h"

namespace {
using DrawEntry = void(__cdecl*)(void*, const char*);
using UpdateMenu = void(__cdecl*)(void*);
constexpr uintptr_t drawEffectsCall = 0x495bfb;
constexpr uintptr_t drawEntryAddress = 0x574de0;
constexpr uintptr_t updateAddress = 0x495900;
constexpr uintptr_t sourceDrawSlot = 0x7fad30;
constexpr uintptr_t runtimeDrawSlot = 0x800e4c;
void (*openSettings)(void*){};
bool (*settingsOpen)(){};
void (*drawSettings)(void*){};
void (*updateSettings)(void*){};
void (*writeLog)(const char*){};
bool installed{};
bool firstDraw{}, firstUpdate{};

template<class T> T& field(void* menu, size_t offset) {
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(menu) + offset);
}
void __cdecl drawPause(void* menu) {
    if (settingsOpen()) drawSettings(menu);
    else reinterpret_cast<void(__cdecl*)(void*)>(0x495b50)(menu);
}
void __cdecl drawEffectsAndMods(void* menu, const char* effects) {
    auto draw = reinterpret_cast<DrawEntry>(drawEntryAddress);
    draw(menu, effects);
    draw(menu, "Mod Settings");
    if (!firstDraw) { firstDraw=true; writeLog("Pause hook: native Mod Settings row drawn below Effects Settings"); }
}
void __cdecl updatePause(void* menu) {
    if (!firstUpdate) { firstUpdate=true; writeLog("Pause hook: native pause update active"); }
    // The game stays paused underneath the loader's settings panel.
    if (settingsOpen()) {
        updateSettings(menu);
        return;
    }
    int selected = field<int>(menu, 0x34);
    if (selected==tcs::modSettingsRow && field<int>(menu, 0x64)) {
        field<int>(menu, 0x64)=0;
        writeLog("Pause hook: Mod Settings selected");
        openSettings(menu);
        return;
    }
    // Keep native Extras, Drop Out and Quit dispatch correct after insertion.
    field<int>(menu, 0x34)=tcs::originalPauseRow(selected);
    reinterpret_cast<UpdateMenu>(updateAddress)(menu);
    field<int>(menu, 0x34)=selected;
}
bool writableCopy(uintptr_t address, const void* value, size_t size) {
    DWORD old{};
    if (!VirtualProtect(reinterpret_cast<void*>(address),size,PAGE_EXECUTE_READWRITE,&old)) return false;
    std::memcpy(reinterpret_cast<void*>(address),value,size);
    FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(address),size);
    DWORD unused{};
    VirtualProtect(reinterpret_cast<void*>(address),size,old,&unused);
    return true;
}
}

bool InstallNativePauseMenu(void (*open)(void*), bool (*isOpen)(), void (*draw)(void*),
    void (*updateFn)(void*), void (*logger)(const char*)) {
    if (installed) return true;
    // Fail closed on other builds or another mod's edits. Never patch an unknown image.
    if (reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))!=0x400000) return false;
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(0x400000);
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS*>(0x400000+dos->e_lfanew);
    if (dos->e_magic!=IMAGE_DOS_SIGNATURE || nt->Signature!=IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 || nt->OptionalHeader.SizeOfImage<0x2580000) return false;
    auto call=reinterpret_cast<const unsigned char*>(drawEffectsCall);
    if (call[0]!=0xe8 || drawEffectsCall+5+*reinterpret_cast<const int32_t*>(call+1)!=drawEntryAddress ||
        *reinterpret_cast<const uintptr_t*>(sourceDrawSlot)!=0x495b50 ||
        *reinterpret_cast<const uintptr_t*>(sourceDrawSlot+4)!=updateAddress) {
        logger("Pause hook: unsupported or modified game code; native menu not patched");
        return false;
    }
    openSettings=open; settingsOpen=isOpen; drawSettings=draw; updateSettings=updateFn; writeLog=logger;
    uintptr_t drawWrapper=reinterpret_cast<uintptr_t>(drawPause);
    uintptr_t update=reinterpret_cast<uintptr_t>(updatePause);
    // Both the static template and the initialized copy must use the wrapper.
    if (!writableCopy(sourceDrawSlot+4,&update,sizeof(update))) return false;
    writableCopy(sourceDrawSlot,&drawWrapper,sizeof(drawWrapper));
    if (*reinterpret_cast<const uintptr_t*>(runtimeDrawSlot)==0x495b50 &&
        *reinterpret_cast<const uintptr_t*>(runtimeDrawSlot+4)==updateAddress) {
        writableCopy(runtimeDrawSlot+4,&update,sizeof(update));
        writableCopy(runtimeDrawSlot,&drawWrapper,sizeof(drawWrapper));
    }
    unsigned char replacement[5]{0xe8};
    int32_t relative=static_cast<int32_t>(reinterpret_cast<uintptr_t>(drawEffectsAndMods)-(drawEffectsCall+5));
    std::memcpy(replacement+1,&relative,4);
    if (!writableCopy(drawEffectsCall,replacement,sizeof(replacement))) {
        uintptr_t original=updateAddress;
        writableCopy(sourceDrawSlot+4,&original,sizeof(original));
        writableCopy(runtimeDrawSlot+4,&original,sizeof(original));
        original=0x495b50;
        writableCopy(sourceDrawSlot,&original,sizeof(original));
        writableCopy(runtimeDrawSlot,&original,sizeof(original));
        return false;
    }
    installed=true;
    logger("Pause hook installed: Mod Settings at row 5; native Extras/Drop Out/Quit remapped");
    return true;
}
