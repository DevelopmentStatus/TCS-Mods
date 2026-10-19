// A private synthetic game host, never installed into the game. Execute the
// shipped DLL at the real x86 addresses and exercise all four production hooks.
#include "mod_api.h"
#include "ally_damage.h"
#include "ally_detour.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"Failed line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
static HMODULE WINAPI fixtureModule(LPCWSTR) { return reinterpret_cast<HMODULE>(0x400000); }
static void bindFixture(HMODULE dll) {
    auto base=reinterpret_cast<unsigned char*>(dll);
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS32*>(base+reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    auto imports=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+pe->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    bool bound=false;
    for(;imports->Name;++imports) {
        auto names=reinterpret_cast<IMAGE_THUNK_DATA32*>(base+imports->OriginalFirstThunk);
        auto slots=reinterpret_cast<IMAGE_THUNK_DATA32*>(base+imports->FirstThunk);
        for(;names->u1.AddressOfData;++names,++slots) {
            if(IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal))continue;
            auto name=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData);
            if(std::strcmp(reinterpret_cast<char*>(name->Name),"GetModuleHandleW"))continue;
            DWORD previous{},unused{};CHECK(VirtualProtect(slots,sizeof(*slots),PAGE_READWRITE,&previous));
            slots->u1.Function=reinterpret_cast<DWORD>(fixtureModule);
            VirtualProtect(slots,sizeof(*slots),previous,&unused);bound=true;
        }
    }
    CHECK(bound);
}
static TCSSettingChanged toggle{}, combatToggle{};
static unsigned seenFlags{};
static int seenDamage{}, seenProbe{}, seenContext{}, hp=5;
static unsigned char* seenAttacker{};
static TCSModHandle __cdecl regMod(const TCSModInfo* info) {
    CHECK(std::strcmp(info->id,"tcs.allydamage")==0); return reinterpret_cast<void*>(1);
}
static BOOL __cdecl regSetting(TCSModHandle, const TCSSettingDescriptor* setting) {
    if(!std::strcmp(setting->key,"Enabled")) toggle=setting->changed;
    else if(!std::strcmp(setting->key,"ActiveCombatAI")) combatToggle=setting->changed;
    else CHECK(false);
    setting->changed(setting->defaultValue,nullptr);return TRUE;
}
static int __cdecl nativeHit(unsigned char* attacker, unsigned char*, int damage, unsigned flags, int probe, int context) {
    seenAttacker=attacker;seenFlags=flags;seenDamage=damage;seenProbe=probe;seenContext=context;
    if(!(flags&2) && !probe) hp-=damage;
    return hp<=0 ? 2 : 1;
}
static std::uintptr_t boltSite=ally_damage::boltFilterAddress;
static int __cdecl shoot(unsigned char* owner, unsigned char* target) {
    int result{};
    __asm {
        push esi
        mov eax, owner
        mov esi, target
        call dword ptr [boltSite]
        mov result, eax
        pop esi
    }
    return result;
}
static std::uintptr_t combatSite=ally_damage::combatAddress;
static void __cdecl think(unsigned char* actor) {
    __asm {
        push esi
        mov esi, actor
        call dword ptr [combatSite]
        pop esi
    }
}
int main(int argc,char**argv) {
    CHECK(argc==2);
    auto base=static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(0x400000),0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(base!=reinterpret_cast<void*>(0x400000)) return 77; // OS address collision, no code executed
    for(auto address: {0x430000u,0x580000u,0x800000u,0x820000u,0x920000u,0x930000u,0x2910000u})
        if(VirtualAlloc(reinterpret_cast<void*>(address),0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)!=reinterpret_cast<void*>(address)) return 77;
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x100;
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS32*>(base+0x100);pe->Signature=IMAGE_NT_SIGNATURE;
    pe->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;pe->FileHeader.TimeDateStamp=0x4ac4a20b;
    pe->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;pe->OptionalHeader.SizeOfImage=0x268e000;
    auto put=[](std::uintptr_t address,const unsigned char* bytes,std::size_t count) {
        DWORD previous{};CHECK(VirtualProtect(reinterpret_cast<void*>(address),count,PAGE_EXECUTE_READWRITE,&previous));
        std::memcpy(reinterpret_cast<void*>(address),bytes,count);
        FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(address),count);
    };
    const unsigned char flagsMachine[]={
0x8b,0x54,0x24,0x04,0x33,0xc0,0x85,0xd2,0x74,0x38,0x8b,0x8a,0xfc,0x01,0x00,0x00,0xc1,0xe9,0x07,0x83,0xe1,0x01,0x75,0x03,0x8d,0x41,0x02,0x85,0xc9,0x74,0x05,0x0d,0x04,0x08,0x00,0x00,0x8b,0x8a,0xf8,0x01,0x00,0x00,0xf7,0xc1,0x01,0x00,0x01,0x00,0x74,0x04,0x83,0xc8,0x10,0xc3,0xf6,0xc1,0x04,0x74,0x04,0x83,0xc8,0x20,0xc3,0x83,0xc8,0x08,0xc3
    };
    put(ally_damage::flagsAddress,flagsMachine,sizeof(flagsMachine));
    unsigned char damageMachine[16]{};
    std::memcpy(damageMachine,ally_damage::hitSignature,8);
    damageMachine[8]=0x83;damageMachine[9]=0xc4;damageMachine[10]=0x4c;
    put(ally_damage::hitAddress,damageMachine,16);
    AllyDetour::jump(reinterpret_cast<unsigned char*>(ally_damage::hitAddress+11),reinterpret_cast<void*>(nativeHit));
    const unsigned char boltMachine[]={
0x8b,0x88,0xfc,0x01,0x00,0x00,0xf6,0xc1,0x01,0x74,0x20,0x84,0xc9,0x78,0x1c,0x8b,0x88,0xf8,0x01,0x00,0x00,0x83,0xe1,0x01,0x74,0x11,0x8b,0x96,0xf8,0x01,0x00,0x00,0x83,0xe2,0x01,0x3b,0xca,0x0f,0x84,0xbd,0x00,0x00,0x00
    };
    put(ally_damage::boltFilterAddress,boltMachine,sizeof(boltMachine));
    const unsigned char permit[]={0xb8,1,0,0,0,0xc3};
    const unsigned char reject[]={0x33,0xc0,0xc3};
    put(0x5815d8,permit,sizeof(permit)); put(0x581695,reject,sizeof(reject));
    unsigned char combatMachine[7]{};std::memcpy(combatMachine,ally_damage::combatSignature,6);combatMachine[6]=0xc3;
    put(ally_damage::combatAddress,combatMachine,sizeof(combatMachine));
    unsigned char guardMachine[12]{};std::memcpy(guardMachine,ally_damage::guardSignature,6);
    guardMachine[6]=0xb8;guardMachine[7]=1;guardMachine[11]=0xc3;put(0x58dfa0,guardMachine,sizeof(guardMachine));
    *reinterpret_cast<float*>(0x925018)=1.0f/60;
    *reinterpret_cast<int*>(0x802820)=23;
    *reinterpret_cast<unsigned*>(0x80227c)=0x80;
    *reinterpret_cast<unsigned*>(0x802280)=0x20;
    std::array<unsigned char,0x10d8> owner{},target{};
    std::array<unsigned char,0x40> data{},details{};
    std::array<unsigned char,0x60> pad{};
    *reinterpret_cast<void**>(data.data()+0x24)=details.data();
    *reinterpret_cast<void**>(owner.data()+0x54)=data.data();
    *reinterpret_cast<void**>(owner.data()+0xca4)=pad.data();
    *reinterpret_cast<void**>(owner.data()+0x3ac)=target.data();
    *reinterpret_cast<void**>(target.data())=target.data();
    *reinterpret_cast<unsigned*>(data.data()+4)=8;
    *reinterpret_cast<unsigned*>(target.data()+0x1fc)=0x1001;
    target[0x280]=255;target[0x107f]=5;target[0x7b5]=255;
    owner[0x281]=1;owner[0x107f]=5;owner[0x7b5]=255;owner[0xe22]=1;
    *reinterpret_cast<float*>(target.data()+0x88)=0.5f;
    *reinterpret_cast<unsigned short*>(target.data()+0x5a)=0x8000;
    *reinterpret_cast<unsigned*>(owner.data()+0x1fc)=0x1001;
    *reinterpret_cast<unsigned*>(owner.data()+0x1f8)=1;
    owner[0x280]=1;
    *reinterpret_cast<void**>(ally_damage::partyAddress+4)=owner.data();
    *reinterpret_cast<unsigned*>(target.data()+0x1f8)=1;
    CHECK(shoot(owner.data(),target.data())==0); // original same-faction rejection
    auto dll=LoadLibraryA(argv[1]);CHECK(dll);bindFixture(dll);
    auto init=reinterpret_cast<TCSModInitialize>(GetProcAddress(dll,"TCSMod_Initialize"));CHECK(init);
    auto shutdown=reinterpret_cast<TCSModShutdown>(GetProcAddress(dll,"TCSMod_Shutdown"));CHECK(shutdown);
    TCSModHost host{};host.apiVersion=TCS_MOD_API_VERSION;host.structSize=sizeof(host);
    host.registerMod=regMod;host.registerSetting=regSetting;
    TCSModHandle mod{};CHECK(init(&host,&mod));CHECK(mod==reinterpret_cast<void*>(1));CHECK(toggle);CHECK(combatToggle);
    CHECK(shoot(owner.data(),target.data())==1); // production naked hook preserves native ABI
    using HitFn=int(__cdecl*)(unsigned char*,unsigned char*,int,unsigned,int,int);
    using FlagsFn=unsigned(__cdecl*)(unsigned char*);
    auto hit=reinterpret_cast<HitFn>(ally_damage::hitAddress);
    auto flags=reinterpret_cast<FlagsFn>(ally_damage::flagsAddress);
    CHECK(flags(owner.data())==0x814);
    hit(owner.data(),target.data(),2,0x8a,0,77);
    CHECK(hp==3);CHECK(seenDamage==2 && seenProbe==0 && seenContext==77 && seenAttacker==owner.data());
    CHECK(seenFlags==0x88c);
    hit(owner.data(),target.data(),2,0x4a,1,13);CHECK(hp==3);CHECK(seenFlags==0x84c);
    toggle(TCSSettingValue{0,0},nullptr);
    CHECK(flags(owner.data())==0x12);CHECK(shoot(owner.data(),target.data())==0);
    hit(owner.data(),target.data(),2,0x88c,0,0);CHECK(hp==3);CHECK(seenFlags==0x8a);
    toggle(TCSSettingValue{1,0},nullptr);
    owner[0x280]=255; // enemy of the same faction is never promoted
    CHECK(flags(owner.data())==0x12);CHECK(shoot(owner.data(),target.data())==0);
    hit(owner.data(),target.data(),2,0x8a,0,0);CHECK(hp==3);CHECK(seenFlags==0x8a);
    owner[0x280]=1;*reinterpret_cast<unsigned*>(owner.data()+0x1fc)|=0x80;
    CHECK(flags(owner.data())==0x814); // human takeover uses native flags
    *reinterpret_cast<unsigned*>(owner.data()+0x1fc)=0x1001;
    owner[0x7b5]=23; // a companion stuck in held guard must resume attacking
    float sentinel{};
    __asm { fld1 }
    think(owner.data());
    __asm { fstp sentinel }
    CHECK(sentinel==1.0f);CHECK(owner[0x7b5]==255);
    CHECK((*reinterpret_cast<unsigned*>(pad.data()+8)&0x80)!=0);
    CHECK((*reinterpret_cast<unsigned*>(pad.data()+4)&0x80)==0);
    *reinterpret_cast<unsigned*>(pad.data()+8)=0;
    think(owner.data());CHECK((*reinterpret_cast<unsigned*>(pad.data()+8)&0x80)==0); // cooldown
    combatToggle(TCSSettingValue{0,0},nullptr);
    think(owner.data());CHECK((*reinterpret_cast<unsigned*>(pad.data()+8)&0x80)==0);
    combatToggle(TCSSettingValue{1,0},nullptr);
    target[0x7b5]=5; // close enemy attack -> short defense, then counterattack
    think(owner.data());CHECK(owner[0x7b5]==23);
    bool counterattack=false;
    for(int frame=0;frame<30;++frame) {
        *reinterpret_cast<unsigned*>(pad.data()+8)=0;
        think(owner.data());counterattack|=(*reinterpret_cast<unsigned*>(pad.data()+8)&0x80)!=0;
    }
    CHECK(counterattack);
    owner[0x7b5]=45;*reinterpret_cast<unsigned*>(pad.data()+8)=0;
    think(owner.data());CHECK(owner[0x7b5]==45 && *reinterpret_cast<unsigned*>(pad.data()+8)==0);
    owner[0x7b5]=255;*reinterpret_cast<unsigned*>(owner.data()+0x1fc)|=0x80;
    think(owner.data());CHECK(*reinterpret_cast<unsigned*>(pad.data()+8)==0); // human takeover
    *reinterpret_cast<unsigned*>(owner.data()+0x1fc)=0x1001;owner[0x280]=255;
    think(owner.data());CHECK(*reinterpret_cast<unsigned*>(pad.data()+8)==0); // enemy unaffected
    owner[0x280]=1;*reinterpret_cast<void**>(owner.data()+0x3ac)=nullptr;
    think(owner.data());CHECK(*reinterpret_cast<unsigned*>(pad.data()+8)==0); // no encounter
    *reinterpret_cast<void**>(owner.data()+0x3ac)=target.data();
    target[0x7b5]=255;*reinterpret_cast<unsigned*>(data.data()+4)=0x80;
    *reinterpret_cast<float*>(target.data()+0x88)=3;
    combatToggle(TCSSettingValue{1,0},nullptr);
    think(owner.data());CHECK((*reinterpret_cast<unsigned*>(pad.data()+8)&0x80)!=0); // ranged shot
    *reinterpret_cast<unsigned*>(pad.data()+8)=0;
    target[0x280]=1;think(owner.data());CHECK(*reinterpret_cast<unsigned*>(pad.data()+8)==0); // party excluded
    target[0x280]=255;target[0x107f]=0;think(owner.data());CHECK(*reinterpret_cast<unsigned*>(pad.data()+8)==0);
    target[0x107f]=5;
    std::array<unsigned char,0x10d8> human{};
    *reinterpret_cast<unsigned*>(human.data()+0x1fc)=0x1081;
    *reinterpret_cast<void**>(human.data()+0xca4)=pad.data();
    *reinterpret_cast<void**>(0x93d810)=human.data();
    think(owner.data());CHECK(*reinterpret_cast<unsigned*>(pad.data()+8)==0); // shared human input never altered
    *reinterpret_cast<void**>(0x93d810)=nullptr;
    *reinterpret_cast<float*>(0x925018)=0;think(owner.data());CHECK(*reinterpret_cast<unsigned*>(pad.data()+8)==0);
    // Regression: all four hooks must ignore a friendly nonparty NPC, even
    // when its character record contains a plausible/stale party slot.
    *reinterpret_cast<float*>(0x925018)=1.0f/60;
    *reinterpret_cast<unsigned*>(pad.data()+8)=0;
    target[0x7b5]=255;owner[0x7b5]=255;
    *reinterpret_cast<unsigned*>(owner.data()+0x1f8)=0x400;
    for(unsigned slot: {255u,1u}) {
        owner[0x280]=static_cast<unsigned char>(slot);
        *reinterpret_cast<void**>(ally_damage::partyAddress+4)=nullptr;
        CHECK(flags(owner.data())==0x0a);
        int before=hp;
        hit(owner.data(),target.data(),2,0x8a,0,0);
        CHECK(hp==before && seenFlags==0x8a);
        think(owner.data());CHECK(*reinterpret_cast<unsigned*>(pad.data()+8)==0);
        *reinterpret_cast<unsigned*>(owner.data()+0x1f8)=0x401;
        CHECK(shoot(owner.data(),target.data())==0);
        *reinterpret_cast<unsigned*>(owner.data()+0x1f8)=0x400;
    }
    // Conversely, faction does not disqualify a real companion.
    owner[0x280]=1;
    *reinterpret_cast<void**>(ally_damage::partyAddress+4)=owner.data();
    CHECK(flags(owner.data())==0x80c);
    combatToggle(TCSSettingValue{1,0},nullptr);
    think(owner.data());CHECK((*reinterpret_cast<unsigned*>(pad.data()+8)&0x80)!=0);
    shutdown(mod);
    CHECK(!std::memcmp(reinterpret_cast<void*>(ally_damage::flagsAddress),flagsMachine,sizeof(flagsMachine)));
    CHECK(!std::memcmp(reinterpret_cast<void*>(ally_damage::hitAddress),damageMachine,8));
    CHECK(!std::memcmp(reinterpret_cast<void*>(ally_damage::boltFilterAddress),boltMachine,sizeof(boltMachine)));
    CHECK(!std::memcmp(reinterpret_cast<void*>(ally_damage::combatAddress),combatMachine,sizeof(combatMachine)));
    CHECK(FreeLibrary(dll));
    for(auto address: {0x400000u,0x430000u,0x580000u,0x800000u,0x820000u,0x920000u,0x930000u,0x2910000u}) VirtualFree(reinterpret_cast<void*>(address),0,MEM_RELEASE);
    std::puts("Production DLL hooks: ally HP damage, enemy isolation, in-flight toggle, same-faction bolts, human takeover, combat attacks/defense/counterattacks, x87 preservation and shutdown passed (synthetic host).");
}
