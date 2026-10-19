#include "mod_api.h"
#include "ally_damage.h"
#include "ally_detour.h"
#include <cstdio>
#include <cstdlib>
#include <array>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"Failed line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
using Fn = unsigned (__cdecl*)(const std::uint8_t*);
static AllyDetour detour;
static bool enabled = true;
unsigned __cdecl intercepted(const std::uint8_t* object) {
    auto native = reinterpret_cast<Fn>(detour.bridge)(object);
    return object && enabled && ally_damage::isAllyAI(*reinterpret_cast<const unsigned*>(object+0x1fc),
        object[0x280]) ? ally_damage::playerHitFlags(native) : native;
}
static int registrations{};
static TCSModHandle __cdecl registerMod(const TCSModInfo*) { ++registrations; return reinterpret_cast<void*>(1); }
static BOOL __cdecl registerSetting(TCSModHandle, const TCSSettingDescriptor*) { ++registrations; return TRUE; }
int main(int argc, char** argv) {
    CHECK(argc == 2);
    auto plugin = LoadLibraryA(argv[1]); CHECK(plugin);
    auto initialize = reinterpret_cast<TCSModInitialize>(GetProcAddress(plugin,"TCSMod_Initialize")); CHECK(initialize);
    TCSModHost host{}; host.apiVersion=TCS_MOD_API_VERSION; host.structSize=sizeof(host);
    host.registerMod=registerMod; host.registerSetting=registerSetting;
    TCSModHandle handle=reinterpret_cast<void*>(1);
    CHECK(!initialize(nullptr,&handle)); CHECK(!initialize(&host,nullptr));
    host.apiVersion++; CHECK(!initialize(&host,&handle)); host.apiVersion--;
    CHECK(!initialize(&host,&handle)); CHECK(handle==nullptr); CHECK(registrations==0);
    CHECK(FreeLibrary(plugin));
    // Execute the actual PC ObjHitObj_Flags machine code in a private executable
    // allocation; exercise the same trampoline and calling convention as the DLL.
    const unsigned char machine[] = {
0x8b,0x54,0x24,0x04,0x33,0xc0,0x85,0xd2,0x74,0x38,0x8b,0x8a,0xfc,0x01,0x00,0x00,0xc1,0xe9,0x07,0x83,0xe1,0x01,0x75,0x03,0x8d,0x41,0x02,0x85,0xc9,0x74,0x05,0x0d,0x04,0x08,0x00,0x00,0x8b,0x8a,0xf8,0x01,0x00,0x00,0xf7,0xc1,0x01,0x00,0x01,0x00,0x74,0x04,0x83,0xc8,0x10,0xc3,0xf6,0xc1,0x04,0x74,0x04,0x83,0xc8,0x20,0xc3,0x83,0xc8,0x08,0xc3
    };
    auto code = static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    CHECK(code); std::memcpy(code,machine,sizeof(machine)); DWORD old{};
    CHECK(VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old));
    auto fn = reinterpret_cast<Fn>(code);
    std::array<std::uint8_t,0x28c> object{};
    auto& flags = *reinterpret_cast<unsigned*>(object.data()+0x1fc);
    auto& state = *reinterpret_cast<unsigned*>(object.data()+0x1f8);
    auto& slot = object[0x280]; flags=0x1001; slot=1;
    CHECK(fn(object.data())==0x0a); CHECK(fn(nullptr)==0);
    CHECK(detour.install(code,reinterpret_cast<void*>(intercepted),ally_damage::flagsSignature,sizeof(ally_damage::flagsSignature)));
    CHECK(fn(object.data())==0x80c); CHECK(fn(nullptr)==0);
    state=4; CHECK(fn(object.data())==0x824);
    state=1; CHECK(fn(object.data())==0x814);
    // Party characters on either faction receive native player damage flags.
    for(unsigned faction: {0u,1u,4u,0x10000u,0x400u}) {
        state=faction;
        auto original=reinterpret_cast<Fn>(detour.bridge)(object.data());
        flags|=0x80; auto player=fn(object.data()); flags&=~0x80u;
        CHECK(ally_damage::playerHitFlags(original)==player);
    }
    state=0; enabled=false; CHECK(fn(object.data())==0x0a);
    enabled=true; flags|=0x80; CHECK(fn(object.data())==0x80c);
    flags=0x1001; slot=255;
    state=1; CHECK(fn(object.data())==0x12); // enemy unchanged
    state=4; CHECK(fn(object.data())==0x22);
    state=0; CHECK(fn(object.data())==0x0a); // ambient NPC unchanged
    state=0x400; CHECK(fn(object.data())==0x0a); // friendly NPC is not a companion
    flags=0; CHECK(!ally_damage::isAllyAI(flags,0));
    flags=0x1081; CHECK(!ally_damage::isAllyAI(flags,0));
    CHECK(ally_damage::playerHitFlags(0x1ca)==0x9cc); // melee/projectile/shield bits retained
    CHECK(detour.remove()); CHECK(!std::memcmp(code,machine,sizeof(machine)));
    CHECK(!detour.install(code,reinterpret_cast<void*>(intercepted),ally_damage::hitSignature,sizeof(ally_damage::hitSignature)));
    CHECK(detour.site==nullptr); CHECK(fn(nullptr)==0);
    CHECK(VirtualProtect(code,4096,PAGE_READWRITE,&old));
    // Exercise the longer eight-byte prologue and stack argument forwarding.
    const unsigned char stackCode[]={0x83,0xec,0x04,0xb8,0x07,0x00,0x00,0x00,0x83,0xc4,0x04,0xc3};
    std::memcpy(code,stackCode,sizeof(stackCode));
    CHECK(VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old));
    CHECK(detour.install(code,reinterpret_cast<void*>(intercepted),stackCode,8));
    CHECK(fn(nullptr)==7); CHECK(detour.remove()); CHECK(fn(nullptr)==7);
    VirtualFree(code,0,MEM_RELEASE);
    std::puts("Native x86 damage flags, party/faction boundaries, toggle, trampoline and rollback passed.");
}
