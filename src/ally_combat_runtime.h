#pragma once
#include "ally_combat.h"

namespace {
bool activeCombat = true;
AllyDetour combatHook;
void* combatNative{};
struct CombatEntry {
    std::uint8_t* object{};
    DWORD lastSeen{};
    ally_combat::State state{};
};
CombatEntry combatEntries[64]{};

template<class T> T& field(std::uint8_t* object, unsigned offset) {
    return *reinterpret_cast<T*>(object + offset);
}
bool humanPad(const std::uint8_t* pad) {
    for(auto address: {0x93d810u, 0x93d814u}) {
        if(!readable(reinterpret_cast<void*>(address),4)) continue;
        auto player = *reinterpret_cast<std::uint8_t**>(address);
        if(readable(player,0xca8) && (field<unsigned>(player,0x1fc)&0x80) &&
           field<std::uint8_t*>(player,0xca4)==pad) return true;
    }
    return false;
}
void __cdecl combatThink(std::uint8_t* object) {
    if(!activeCombat || !ally(object) || !readable(object,0x10d8) || object[0x28b] ||
       static_cast<std::int8_t>(object[0x107f])<=0 || !object[0x281] ||
       field<void*>(object,0xeac) || field<void*>(object,0xcd0)) return;
    auto targetAPI = field<std::uint8_t*>(object,0x3ac);
    if(!readable(targetAPI,0x28c)) return;
    auto target = field<std::uint8_t*>(targetAPI,0);
    if(!readable(target,0x10d8) || target==object || target[0x28b] ||
       (field<unsigned>(target,0x1fc)&0x1081)!=0x1001 || target[0x280]<8 ||
       static_cast<std::int8_t>(target[0x107f])<=0) return;
    auto data = field<std::uint8_t*>(object,0x54);
    auto pad = field<std::uint8_t*>(object,0xca4);
    if(!readable(data,8) || !readable(pad,0x60) || humanPad(pad)) return;
    auto modelFlags = field<unsigned>(data,4);
    // The verified model flags used by the native saber and blaster paths.
    if(!(modelFlags&0x88)) return;
    if(!readable(reinterpret_cast<void*>(0x925018),4) || !readable(reinterpret_cast<void*>(0x802820),4)) return;
    float dt = *reinterpret_cast<float*>(0x925018);
    int hold = *reinterpret_cast<int*>(0x802820);
    int context = static_cast<std::int8_t>(object[0x7b5]);
    const bool ready = context==-1 || context==5 || context==10 || (hold>=0 && context==hold);
    if(!ready) return;
    float dx = field<float>(targetAPI,0x80)-field<float>(object,0x80);
    float dz = field<float>(targetAPI,0x88)-field<float>(object,0x88);
    float dy = field<float>(targetAPI,0x84)-field<float>(object,0x84);
    float distance = std::sqrt(dx*dx+dz*dz);
    if(!std::isfinite(distance) || !std::isfinite(dy)) return;
    constexpr float radiansPerTurnUnit = 6.28318530718f/65536.0f;
    float heading = field<std::uint16_t>(object,0x5a)*radiansPerTurnUnit;
    float targetHeading = field<std::uint16_t>(target,0x5a)*radiansPerTurnUnit;
    float inverse = distance>0.0001f ? 1.0f/distance : 0;
    float facing = distance>0.0001f ? (std::sin(heading)*dx+std::cos(heading)*dz)*inverse : 1;
    float enemyFacing = -(std::sin(targetHeading)*dx+std::cos(targetHeading)*dz)*inverse;
    bool ranged = (modelFlags&0x80)!=0 && (modelFlags&8)==0;
    bool canBlock = !ranged && (modelFlags&8)!=0;
    int enemyContext = static_cast<std::int8_t>(target[0x7b5]);
    bool enemyAttacking = enemyContext==5 || enemyContext==10 || enemyContext==16 || enemyContext==38;
    DWORD now = GetTickCount();
    CombatEntry* entry = nullptr;
    for(auto& candidate:combatEntries) if(candidate.object==object) { entry=&candidate; break; }
    if(!entry) for(auto& candidate:combatEntries) if(!candidate.object || now-candidate.lastSeen>1000) { entry=&candidate;break; }
    if(!entry) return;
    if(entry->object!=object || now-entry->lastSeen>1000) *entry=CombatEntry{object};
    entry->lastSeen=now;
    ally_combat::Input input{reinterpret_cast<std::uintptr_t>(target),true,ready,ranged,canBlock,
        context==-1 || (hold>=0 && context==hold), (object[0xe22]&1)!=0,
        enemyAttacking,enemyFacing>0.2f,distance,dy,facing,dt};
    auto action = ally_combat::decide(entry->state,input);
    if(action==ally_combat::Action::Native) return;
    if(!readable(reinterpret_cast<void*>(0x80227c),4) || !readable(reinterpret_cast<void*>(0x802280),4)) return;
    unsigned attack = *reinterpret_cast<unsigned*>(0x80227c);
    unsigned special = *reinterpret_cast<unsigned*>(0x802280);
    if(!attack || attack==special) return;
    // Clear only combat edges while engaged. Native navigation, jumps, Force,
    // level scripting and interactions continue to own their remaining input.
    field<unsigned>(pad,4)&=~attack;
    field<unsigned>(pad,8)&=~attack;
    field<unsigned>(pad,12)|=attack;
    if(action==ally_combat::Action::Defend) {
        if(hold>=0 && hold<=127) {
            if(context==-1 && reinterpret_cast<int(__cdecl*)(std::uint8_t*)>(0x58dfa0)(object)) {
                object[0x7b5]=static_cast<std::uint8_t>(hold); context=hold;
            }
            if(context==hold) {
                field<float>(object,0x77c)=0.20f;
                field<unsigned>(pad,4)|=attack;
                field<unsigned>(pad,12)&=~attack;
            }
        }
        return;
    }
    if(hold>=0 && context==hold) {
        object[0x7b5]=255;
        field<float>(object,0x77c)=0;
    }
    if(action==ally_combat::Action::Recover) return;
    if(distance>0.0001f && (context==-1 || context==hold || ranged)) {
        auto desired = static_cast<std::uint16_t>(static_cast<int>(std::atan2(dx,dz)/radiansPerTurnUnit));
        // Bound turning to 360 degrees/second; do not snap the whole animation.
        int difference = static_cast<std::int16_t>(desired-field<std::uint16_t>(object,0x5a));
        int limit = std::max(1,static_cast<int>(65536.0f*dt));
        auto turned = static_cast<std::uint16_t>(field<std::uint16_t>(object,0x5a)+std::clamp(difference,-limit,limit));
        field<std::uint16_t>(object,0x5a)=turned;field<std::uint16_t>(object,0x58)=turned;
    }
    if(action==ally_combat::Action::Attack || action==ally_combat::Action::DrawWeapon) {
        field<unsigned>(pad,8)|=attack;
        field<unsigned>(pad,12)&=~attack;
        // The same native fire-target pointer written by GameAIProcess.
        field<std::uint8_t*>(object,0xea0)=target;
    }
}
// Preserve the game's complete x87/SSE state across the decision callback:
// this instruction lies inside GameAIProcess, not at a C ABI call boundary.
__declspec(naked) void combatDecision() {
    __asm {
        pushfd
        pushad
        mov ebx, esp
        sub esp, 528
        and esp, -16
        fxsave [esp]
        fninit
        push ebx
        push esi
        call combatThink
        add esp, 4
        pop ebx
        fxrstor [esp]
        mov esp, ebx
        popad
        popfd
        jmp dword ptr [combatNative]
    }
}
void __cdecl combatChanged(TCSSettingValue value,void*) {
    activeCombat=value.integer!=0;
    for(auto& entry:combatEntries) entry={};
}
}
