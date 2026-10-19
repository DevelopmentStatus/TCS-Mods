#define NOMINMAX
#include "mod_api.h"
#include <initializer_list>
#include "ally_damage.h"
#include "ally_detour.h"

namespace {
const TCSModHost* host{};
TCSModHandle mod{};
bool enabled = true;
AllyDetour flagsHook, hitHook, boltHook;
void* boltNative{};
std::uintptr_t boltContinue = 0x5815d8;
using FlagsFn = unsigned (__cdecl*)(const std::uint8_t*);
using HitFn = int (__cdecl*)(std::uint8_t*, std::uint8_t*, int, unsigned, int, int);
bool readable(const void* p, std::size_t n) {
    MEMORY_BASIC_INFORMATION memory{};
    if(!p || !VirtualQuery(p, &memory, sizeof(memory))) return false;
    auto start = reinterpret_cast<std::uintptr_t>(p);
    auto end = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
    return memory.State == MEM_COMMIT && !(memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
        start <= end && n <= end - start;
}
bool ally(const std::uint8_t* object) {
    if(!readable(object, 0x28c)) return false;
    unsigned slot = object[ally_damage::slotOffset];
    if(!ally_damage::isAllyAI(*reinterpret_cast<const std::uint32_t*>(object + ally_damage::flagsOffset),slot)) return false;
    auto party = reinterpret_cast<std::uint8_t* const*>(ally_damage::partyAddress);
    // A faction flag or a stale slot is not party ownership. All four hooks
    // require the exact live object registered in this native party slot.
    return readable(party,ally_damage::partySize*sizeof(*party)) && party[slot]==object;
}
int __cdecl allowAllyShot(const std::uint8_t* owner) { return enabled && ally(owner); }
// EAX is the bolt owner at this verified PC instruction. Preserve all live
// registers and flags; bypass only the AI same-faction rejection for allies.
__declspec(naked) void boltFilter() {
    __asm {
        pushfd
        pushad
        push eax
        call allowAllyShot
        add esp, 4
        test eax, eax
        jnz permitted
        popad
        popfd
        jmp dword ptr [boltNative]
    permitted:
        popad
        popfd
        jmp dword ptr [boltContinue]
    }
}
unsigned __cdecl hitFlags(const std::uint8_t* attacker) {
    auto flags = reinterpret_cast<FlagsFn>(flagsHook.bridge)(attacker);
    return enabled && ally(attacker) ? ally_damage::playerHitFlags(flags) : flags;
}
int __cdecl hit(std::uint8_t* attacker, std::uint8_t* target, int damage, unsigned flags, int probe, int context) {
    // Also cover explicit/stored hit flags and bolts created before the toggle.
    if(ally(attacker)) {
        auto native = reinterpret_cast<FlagsFn>(flagsHook.bridge)(attacker);
        if(enabled) {
            if(!flags) flags = native;
            flags = ally_damage::playerHitFlags(flags);
        } else if(flags) {
            // Undo cached allegiance flags on shots already in flight when disabled.
            flags = (flags & ~0x806u) | (native & 0x806u);
        }
    }
    return reinterpret_cast<HitFn>(hitHook.bridge)(attacker, target, damage, flags, probe, context);
}
void __cdecl changed(TCSSettingValue value, void*) { enabled = value.integer != 0; }
void log(const char* message) { if(host && host->log) host->log(mod, message); }
}
#include "ally_combat_runtime.h"

TCS_MOD_EXPORT BOOL __cdecl TCSMod_Initialize(const TCSModHost* api, TCSModHandle* out) {
    if(!api || !out || api->apiVersion != TCS_MOD_API_VERSION || api->structSize < sizeof(TCSModHost) ||
       !api->registerMod || !api->registerSetting) return FALSE;
    *out = nullptr;
    if(host) return FALSE;
    auto base = reinterpret_cast<const std::uint8_t*>(GetModuleHandleW(nullptr));
    if(reinterpret_cast<std::uintptr_t>(base) != 0x400000 || !readable(base, sizeof(IMAGE_DOS_HEADER))) return FALSE;
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x1000) return FALSE;
    auto pe = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if(!readable(pe, sizeof(*pe)) || pe->Signature != IMAGE_NT_SIGNATURE ||
       pe->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 || pe->FileHeader.TimeDateStamp != 0x4ac4a20b ||
       pe->OptionalHeader.SizeOfImage != 0x268e000 || pe->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
       pe->OptionalHeader.SizeOfImage < ally_damage::flagsAddress - 0x400000 + 0x44 ||
       !readable(reinterpret_cast<void*>(ally_damage::combatAddress),sizeof(ally_damage::combatSignature)) ||
       !readable(reinterpret_cast<void*>(0x58dfa0),sizeof(ally_damage::guardSignature)) ||
       std::memcmp(reinterpret_cast<void*>(ally_damage::combatAddress),ally_damage::combatSignature,sizeof(ally_damage::combatSignature)) ||
       std::memcmp(reinterpret_cast<void*>(0x58dfa0),ally_damage::guardSignature,sizeof(ally_damage::guardSignature)) ||
       !readable(reinterpret_cast<void*>(ally_damage::flagsAddress), sizeof(ally_damage::flagsSignature)) ||
       !readable(reinterpret_cast<void*>(ally_damage::hitAddress), sizeof(ally_damage::hitSignature)) ||
       !readable(reinterpret_cast<void*>(ally_damage::boltFilterAddress), sizeof(ally_damage::boltFilterSignature)) ||
       std::memcmp(reinterpret_cast<void*>(ally_damage::boltFilterAddress), ally_damage::boltFilterSignature, sizeof(ally_damage::boltFilterSignature)) ||
       std::memcmp(reinterpret_cast<void*>(ally_damage::flagsAddress), ally_damage::flagsSignature, sizeof(ally_damage::flagsSignature)) ||
       std::memcmp(reinterpret_cast<void*>(ally_damage::hitAddress), ally_damage::hitSignature, sizeof(ally_damage::hitSignature))) return FALSE;
    host = api;
    TCSModInfo info{sizeof(TCSModInfo), "tcs.allydamage", "TCS Ally Damage", "1.1.1"};
    mod = host->registerMod(&info);
    if(!mod) { host = nullptr; return FALSE; }
    TCSSettingDescriptor setting{};
    setting.structSize = sizeof(setting); setting.category = "Gameplay";
    setting.key = "Enabled"; setting.label = "AI allies deal damage";
    setting.type = TCS_SETTING_BOOL; setting.defaultValue.integer = 1;
    setting.maximum = 1; setting.step = 1; setting.changed = changed;
    if(!host->registerSetting(mod, &setting)) { host = nullptr; mod = nullptr; return FALSE; }
    setting.key = "ActiveCombatAI"; setting.label = "Active combat AI";
    setting.changed = combatChanged;
    if(!host->registerSetting(mod,&setting)) { host=nullptr;mod=nullptr;return FALSE; }
    if(!flagsHook.install(reinterpret_cast<void*>(ally_damage::flagsAddress), reinterpret_cast<void*>(hitFlags),
            ally_damage::flagsSignature, sizeof(ally_damage::flagsSignature)) ||
       !hitHook.install(reinterpret_cast<void*>(ally_damage::hitAddress), reinterpret_cast<void*>(hit),
            ally_damage::hitSignature, sizeof(ally_damage::hitSignature)) ||
       !boltHook.install(reinterpret_cast<void*>(ally_damage::boltFilterAddress), reinterpret_cast<void*>(boltFilter),
            ally_damage::boltFilterSignature, sizeof(ally_damage::boltFilterSignature)) ||
       !combatHook.install(reinterpret_cast<void*>(ally_damage::combatAddress),reinterpret_cast<void*>(combatDecision),
            ally_damage::combatSignature,sizeof(ally_damage::combatSignature))) {
        combatHook.remove(); boltHook.remove(); hitHook.remove(); flagsHook.remove();
        log("Ally Damage: hook installation failed; native behavior restored.");
        host = nullptr; mod = nullptr; return FALSE;
    }
    boltNative = boltHook.bridge;
    combatNative = combatHook.bridge;
    log("Ally Damage: player-style hit flags restricted to registered AI party companions; native damage and defenses retained.");
    *out = mod; return TRUE;
}
TCS_MOD_EXPORT void __cdecl TCSMod_Shutdown(TCSModHandle) {
    enabled = false;activeCombat=false;
    for(auto& entry:combatEntries) entry={};
    combatHook.remove(); boltHook.remove(); hitHook.remove(); flagsHook.remove();
    host = nullptr; mod = nullptr;
}
