// Run the compiled production DLL against an isolated synthetic PC shop layout.
// Never opens, injects into, or modifies the user's game process.
#include "mod_api.h"
#include "shop_ownership.h"
#include <array>
#include <cstring>
#include <cstdio>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"Failed line %d: %s\n",__LINE__,#x);return 1;}} while(0)
static TCSSettingChanged toggle;
static int registered;
static HMODULE WINAPI fixtureModule(LPCWSTR){return reinterpret_cast<HMODULE>(0x400000);}
static bool bindFixture(HMODULE dll) {
    auto base=reinterpret_cast<unsigned char*>(dll);
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS32*>(base+reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    auto imports=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+pe->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for(;imports->Name;++imports) {
        auto names=reinterpret_cast<IMAGE_THUNK_DATA32*>(base+imports->OriginalFirstThunk);
        auto slots=reinterpret_cast<IMAGE_THUNK_DATA32*>(base+imports->FirstThunk);
        for(;names->u1.AddressOfData;++names,++slots) {
            if(IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal))continue;
            auto name=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData);
            if(std::strcmp(reinterpret_cast<char*>(name->Name),"GetModuleHandleW"))continue;
            DWORD old{},unused{};if(!VirtualProtect(slots,sizeof(*slots),PAGE_READWRITE,&old))return false;
            slots->u1.Function=reinterpret_cast<DWORD>(fixtureModule);
            VirtualProtect(slots,sizeof(*slots),old,&unused);return true;
        }
    }
    return false;
}
static TCSModHandle __cdecl regMod(const TCSModInfo* info) {
    if(std::strcmp(info->id,"tcs.shopownership"))return nullptr;
    ++registered;return reinterpret_cast<void*>(1);
}
static BOOL __cdecl regSetting(TCSModHandle,const TCSSettingDescriptor* descriptor) {
    if(std::strcmp(descriptor->key,"Enabled")||!descriptor->changed)return FALSE;
    toggle=descriptor->changed;toggle(descriptor->defaultValue,nullptr);return TRUE;
}
static void __cdecl log(TCSModHandle,const char*){}
int main(int argc,char** argv) {
    CHECK(argc==2);
    for(auto address:{0x400000u,0x4c0000u,0x850000u,0x860000u,0x880000u,0x930000u})
        if(VirtualAlloc(reinterpret_cast<void*>(address),0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)!=reinterpret_cast<void*>(address))return 77;
    auto base=reinterpret_cast<unsigned char*>(0x400000);
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x100;
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS32*>(base+0x100);pe->Signature=IMAGE_NT_SIGNATURE;
    pe->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;pe->FileHeader.TimeDateStamp=0x4ac4a20b;
    pe->OptionalHeader.SizeOfImage=0x268e000;
    auto site=reinterpret_cast<unsigned char*>(shop_ownership::updateAddress);
    std::memcpy(site,shop_ownership::updateSignature,sizeof(shop_ownership::updateSignature));
    // Native fixture preserves the exact 10-byte PC prologue then pops EDI/returns.
    site[10]=0x5f;site[11]=0xc3;DWORD old{};
    CHECK(VirtualProtect(site,0x100,PAGE_EXECUTE_READ,&old));
    shop_ownership::Item items[4]{};
    for(unsigned i=0;i<4;++i){items[i].character=static_cast<std::uint16_t>(i);items[i].type=1;items[i].price=5000+i;}
    items[3].type=2;
    const std::array<unsigned char,4> ownership{3,2,0,3};
    *reinterpret_cast<unsigned short*>(0x855efe)=4;
    *reinterpret_cast<shop_ownership::Item**>(0x880478)=items;
    *reinterpret_cast<const unsigned char**>(0x93b288)=ownership.data();
    *reinterpret_cast<unsigned*>(0x93b278)=4;
    *reinterpret_cast<unsigned*>(0x86e4dc)=605010;
    auto before=std::array<unsigned char,0x100>{};std::memcpy(before.data(),reinterpret_cast<void*>(0x86e480),before.size());
    HMODULE dll=LoadLibraryA(argv[1]);CHECK(dll&&bindFixture(dll));
    auto init=reinterpret_cast<TCSModInitialize>(GetProcAddress(dll,"TCSMod_Initialize"));
    auto stop=reinterpret_cast<TCSModShutdown>(GetProcAddress(dll,"TCSMod_Shutdown"));CHECK(init&&stop);
    TCSModHost api{};api.apiVersion=TCS_MOD_API_VERSION;api.structSize=sizeof(api);
    api.registerMod=regMod;api.registerSetting=regSetting;api.log=log;
    TCSModHandle handle{};auto bad=api;bad.apiVersion=99;CHECK(!init(&bad,&handle)&&!registered);
    CHECK(init(&api,&handle)&&handle&&registered==1&&toggle);
    auto update=reinterpret_cast<void(__cdecl*)()>(site);
    update();CHECK(items[0].purchased==1&&items[1].purchased==0&&items[2].purchased==0&&items[3].purchased==0);
    CHECK(!std::memcmp(before.data(),reinterpret_cast<void*>(0x86e480),before.size()));
    CHECK((ownership==std::array<unsigned char,4>{3,2,0,3}));
    TCSSettingValue off{};toggle(off,nullptr);update();CHECK(items[0].purchased==0);
    TCSSettingValue on{};on.integer=1;toggle(on,nullptr);update();CHECK(items[0].purchased==1);
    stop(handle);CHECK(!std::memcmp(site,shop_ownership::updateSignature,sizeof(shop_ownership::updateSignature)));
    // A conflicting native patch must be rejected before plugin registration.
    CHECK(VirtualProtect(site,12,PAGE_EXECUTE_READWRITE,&old));site[0]=0x90;
    CHECK(!init(&api,&handle)&&handle==nullptr&&registered==1);
    FreeLibrary(dll);return 0;
}
