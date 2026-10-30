#include "shop_capacity.h"
#include <sstream>
#include <cstdio>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"Failed line %d: %s\n",__LINE__,#x);return 1;}} while(0)
using namespace shop_capacity;
#ifdef TCS_NATIVE_SWITCH_FIXTURE
#include "native_switch_code.h"
static LONG WINAPI fixtureException(EXCEPTION_POINTERS* error) {
    std::fprintf(stderr,"Native fixture exception %lx at %p esp %lx\n",error->ExceptionRecord->ExceptionCode,
        error->ExceptionRecord->ExceptionAddress,error->ContextRecord->Esp);
    return EXCEPTION_CONTINUE_SEARCH;
}
static unsigned __cdecl textLength(const char* text){return static_cast<unsigned>(strlen(text));}
static char* __cdecl textCopy(char* target,const char* source){return strcpy(target,source);}
static int nativePortraitRegistryTests() {
    AllyDetour::jump((unsigned char*)0x4f1ff0,textLength);
    AllyDetour::jump((unsigned char*)0x4f1f10,textCopy);
    using Entry=portrait_registry::Entry;
    const Entry baseline[]{{0,0,0,"base"},{3,0,0,"base_icon"},{0xff,0,0,nullptr}};
    Entry before[3];memcpy(before,baseline,sizeof(before));
    std::array<unsigned char,16> arena{};void* cursor=arena.data();void* end=arena.data()+arena.size();
    portrait_registry::entries[0].name="demo_override";
    portrait_registry::initialize(const_cast<Entry*>(baseline),&cursor,&end,3,8000);
    CHECK(cursor==arena.data()&&!memcmp(before,baseline,sizeof(before)));
    CHECK(*reinterpret_cast<Entry**>(0x951bb8)==portrait_registry::entries.data());
    CHECK(*reinterpret_cast<unsigned*>(0x951bbc)==2&&*reinterpret_cast<unsigned*>(0x951bc0)==4096);
    CHECK(*reinterpret_cast<unsigned*>(0x951bcc)==portrait_registry::nameCapacity);
    CHECK(std::string(portrait_registry::entries[0].name)=="demo_override");
    auto add=reinterpret_cast<int(__cdecl*)(const char*,int)>(0x579f60);
    for(unsigned i=2;i<750;++i)CHECK(add(("object_"+std::to_string(i)).c_str(),3)==1);
    // These are the exact names supplied by the screenshots' affected packs.
    CHECK(add("anakin_cnpt_icon",3)==1&&add("anakin_cnpt_icon1",3)==1);
    CHECK(add("tk_commander_icon",3)==1&&add("tk_commander_icon1",3)==1);
    CHECK(std::string(portrait_registry::entries[750].name)=="anakin_cnpt_icon");
    CHECK(std::string(portrait_registry::entries[753].name)=="tk_commander_icon1");
    auto count=*reinterpret_cast<unsigned*>(0x951bbc), used=*reinterpret_cast<unsigned*>(0x951bd0);
    *reinterpret_cast<unsigned*>(0x951bd0)=portrait_registry::nameCapacity-4;
    CHECK(add("long_icon",3)==0&&*reinterpret_cast<unsigned*>(0x951bbc)==count);
    *reinterpret_cast<unsigned*>(0x951bd0)=used;
    for(unsigned i=count;i<4096;++i)CHECK(add(("object_"+std::to_string(i)).c_str(),3)==1);
    CHECK(add("overflow_icon",3)==0&&*reinterpret_cast<unsigned*>(0x951bbc)==4096);
    CHECK(std::string(portrait_registry::entries[750].name)=="anakin_cnpt_icon");
    CHECK(*reinterpret_cast<unsigned*>(0x951bd0)<portrait_registry::nameCapacity);
    return 0;
}
static int nativeSwitchTests() {
    auto handler=AddVectoredExceptionHandler(1,fixtureException);
    std::array<short,1000> slots{};
    std::array<unsigned char,0x40> api{};
    *reinterpret_cast<short**>(api.data()+0x1c)=slots.data();
    *reinterpret_cast<void**>(0x29130e4)=api.data();
    *reinterpret_cast<unsigned*>(0x829570)=0x5a83fe1b;
    *reinterpret_cast<unsigned*>(0x93d500)=0;
    for(auto id:{0x7f18fc,0x7f1910,0x7f1958,0x7f18c4,0x7f18e4,0x7f18e8,
                 0x7f1a74,0x7f1900,0x7f19dc,0x7f17c4,0x7f18d0,0x7f18d4})
        *reinterpret_cast<short*>(id)=-1;
    // Unexpected native GS failure must fail this process, never be suppressed.
    const unsigned char trap[]{0x0f,0x0b};memcpy((void*)0x6eeac7,trap,sizeof(trap));
    const unsigned char zero[]{0x33,0xc0,0xc3},one[]{0xb8,1,0,0,0,0xc3};
    memcpy((void*)0x43bb20,one,sizeof(one));
    for(auto target:{0x5ab050,0x51ac10,0x6e69ef})memcpy((void*)target,zero,sizeof(zero));
    std::array<unsigned char,0x400> player{};std::array<unsigned char,0x2b00> world{};
    for(auto count:{340u,373u,1000u}) {
        *reinterpret_cast<unsigned*>(0x93b278)=count;
        std::fprintf(stderr,"Native switch count %u\n",count);
        std::array<short,1004> output;output.fill(0x1234);
        reinterpret_cast<void(__fastcall*)(short*)>(0x491620)(output.data());
        CHECK(output[count-1]==0&&output[count]==0x1234);
        // With the recorded 373-character roster, the old 340-short boundary
        // is overwritten. The expanded callers below must preserve their GS.
        if(count>340)CHECK(output[340]==0);
        *reinterpret_cast<unsigned*>(0x95e5d4)=((count-1)-0x3c6ef35fu)*0xfee058c5u;
        auto chosen=reinterpret_cast<int(__cdecl*)()>(0x491800)();
        CHECK(chosen==static_cast<int>(count-1));
        // Execute the actual function that failed at 0x491a8c, including both
        // native GS epilogues and the full CHARCOUNT copy into its stack array.
        *reinterpret_cast<void**>(0x93d810)=nullptr;
        reinterpret_cast<void(__cdecl*)(void*)>(0x491840)(world.data());
        *reinterpret_cast<void**>(0x93d810)=player.data();
        *reinterpret_cast<short*>(0x87b460)=-1;
        reinterpret_cast<void(__cdecl*)(void*)>(0x491840)(world.data());
    }
    RemoveVectoredExceptionHandler(handler);return 0;
}
#endif
static void __cdecl copyFixture(void* to,const void* from,unsigned size){memmove(to,from,size);}
static std::string slotPath;
static std::string selectedPath;
static const char* __cdecl pathFixture(int slot){selectedPath=slotPath+std::to_string(slot);return selectedPath.c_str();}
static bool saveFails=false, shortWrite=false;
static int __cdecl saveFixture(int slot,void* buffer,int size,unsigned) {
    if(saveFails)return 0;
    std::ofstream out(pathFixture(slot),std::ios::binary);
    out<<"native header";out.write((const char*)buffer,shortWrite?size/2:size);return out?1:0;
}
static void __cdecl newFixture() {
    memset((void*)0x8668c8,0,gameSize);ownership[0]=3;
}
static void __cdecl updateFixture() {
    auto count=*reinterpret_cast<unsigned short*>(0x855efe);
    auto list=*reinterpret_cast<unsigned char**>(0x855ef8);
    for(unsigned i=0;i<count;++i) {
        memcpy(&items[i].character,list+i*0x1c,2);items[i].type=1;
    }
}
static void jump(uintptr_t from,const void* to) {
    AllyDetour::jump(reinterpret_cast<unsigned char*>(from),to);
    FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(from),5);
}
static int dataTests() {
    Records records;
    for(unsigned i=0;i<1000;++i)records["character_"+std::to_string(i)]=(i%3)?3:2;
    Digest hash{};CHECK(digest("snapshot",8,hash));
    std::ostringstream encoded(std::ios::binary);CHECK(writeRecords(encoded,hash,records));
    auto bytes=encoded.str();Records result;
    std::istringstream input(bytes,std::ios::binary);CHECK(readRecords(input,hash,result)&&result==records);
    auto wrong=hash;wrong[4]^=1;std::istringstream other(bytes,std::ios::binary);
    CHECK(!readRecords(other,wrong,result)&&result==records);
    for(auto length:{size_t(0),size_t(8),bytes.size()-1}) {
        std::istringstream shortInput(bytes.substr(0,length),std::ios::binary);CHECK(!readRecords(shortInput,hash,result));
    }
    auto trailing=bytes+"x";std::istringstream bad(trailing,std::ios::binary);CHECK(!readRecords(bad,hash,result));
    Records invalid{{std::string("bad\0key",7),3}};std::ostringstream output;CHECK(!writeRecords(output,hash,invalid));
    wchar_t temp[MAX_PATH]{};CHECK(GetTempPathW(MAX_PATH,temp));
    auto folder=std::filesystem::path(temp)/(L"tcs-capacity-"+std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(folder);
    auto native=folder/L"SaveGame0";std::ofstream(native)<<"native remains unchanged";
    auto side=native;side+=".tcs-characters";CHECK(store(side,hash,records));
    records["character_999"]=0;CHECK(store(side,hash,records));
    std::ifstream read(side,std::ios::binary);CHECK(readRecords(read,hash,result)&&result==records);read.close();
    std::ifstream original(native);std::string text((std::istreambuf_iterator<char>(original)),{});
    CHECK(text=="native remains unchanged");original.close();
    CHECK(!std::filesystem::exists(side.wstring()+L".tmp"));
    std::filesystem::remove(side);std::filesystem::remove(native);std::filesystem::remove(folder);
    return 0;
}
static int runtimeTests() {
    for(auto address:{0x410000u,0x420000u,0x430000u,0x470000u,0x480000u,0x490000u,0x4c0000u,0x4f0000u,0x500000u,0x510000u,0x540000u,0x570000u,0x5a0000u,0x5b0000u,0x6e0000u,0x7f0000u,0x800000u,0x820000u,0x850000u,0x860000u,0x870000u,0x880000u,0x920000u,0x930000u,0x950000u,0x2910000u})
        if(VirtualAlloc((void*)address,0x10000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE)!=(void*)address) {
            MEMORY_BASIC_INFORMATION m{};VirtualQuery((void*)address,&m,sizeof(m));
            if(m.Type!=MEM_PRIVATE||m.RegionSize<0x10000||m.Protect!=PAGE_EXECUTE_READWRITE||
               *reinterpret_cast<unsigned*>(address)!=0x54435346)return 77;
        }
    // The API-system pointer lies above the mirrored executable range.
#ifdef TCS_NATIVE_SWITCH_FIXTURE
    copyNativeSwitchCode();
#endif
    makePatches();for(const auto& p:patches)memcpy((void*)p.address,p.before.data(),p.before.size());
    const unsigned char reset[]{0x83,0xec,8,0x53,0x8a,0x1d,0xd8,0x68,0x86,0};
    memcpy((void*)0x42abf0,reset,sizeof(reset));
    const unsigned char save[]{0xb8,0x70,0x24,0,0};memcpy((void*)0x5439d0,save,sizeof(save));
    // A single conflicting site rejects the entire patch transaction.
    *reinterpret_cast<unsigned char*>(patches.back().address)^=1;
    CHECK(!installVerifiedPatches());
    CHECK(!memcmp((void*)patches[0].address,patches[0].before.data(),patches[0].before.size()));
    *reinterpret_cast<unsigned char*>(patches.back().address)^=1;
    CHECK(installVerifiedPatches());
    for(const auto& p:patches)CHECK(!memcmp((void*)p.address,p.after.data(),p.after.size()));
    CHECK(*reinterpret_cast<unsigned char**>(0x93b288)==ownership.data());
    jump(0x4c15e0,updateFixture);jump(0x5436e0,pathFixture);
    nativeCopy=copyFixture;
    const unsigned boundaries[]{99,100,127,128,339,340,999};
    std::array<unsigned char,1001*0x1c> collection{};
    for(unsigned i=0;i<=1000;++i){unsigned short id=static_cast<unsigned short>(i);memcpy(collection.data()+i*0x1c,&id,2);}
    *reinterpret_cast<unsigned char**>(0x855ef8)=collection.data();
    *reinterpret_cast<unsigned short*>(0x855efe)=1000;
    for(auto i:boundaries)ownership[i]=3;
    auto allocated=allocateItems(nullptr,nullptr,limit*sizeof(shop_ownership::Item));CHECK(allocated==items.data());
    *reinterpret_cast<shop_ownership::Item**>(0x880478)=items.data();
    updateItems();
    for(auto i:boundaries)CHECK(items[i].character==i&&items[i].purchased==1&&(purchased[i/32]&(1u<<(i%32))));
    CHECK(!items[998].purchased);
    *reinterpret_cast<unsigned short*>(0x855efe)=1001;updateItems();
    CHECK(*reinterpret_cast<unsigned short*>(0x855efe)==1001); // native call was clamped, collection retained
    // Ownership after ID339 must never write the neighboring native global.
    unsigned sentinel=0x1234abcd;memcpy((void*)0x86e714,&sentinel,4);
    syncNative();CHECK(*reinterpret_cast<unsigned*>(0x86e714)==sentinel);
    // Save/accepted-load roundtrip, stable names and reordering at the real globals.
    std::array<unsigned char,1000*0x4c> chars{};std::vector<std::string> names;
    names.reserve(1000);for(unsigned i=0;i<1000;++i)names.push_back("char_"+std::to_string(i));
    for(unsigned i=0;i<1000;++i){auto name=names[i].c_str();memcpy(chars.data()+i*0x4c+0xc,&name,4);}
    *reinterpret_cast<unsigned char**>(0x93b274)=chars.data();*reinterpret_cast<unsigned*>(0x93b278)=1000;
    *reinterpret_cast<int*>(0x921dc0)=0;
    std::array<unsigned char,gameSize+4> buffer{};saveCopy(buffer.data(),(void*)0x8668c8,gameSize);
    CHECK(snapshotReady&&snapshot.at("char_999")==3&&snapshot.at("char_340")==3);
    wchar_t temp[MAX_PATH]{};CHECK(GetTempPathW(MAX_PATH,temp));
    auto folder=std::filesystem::path(temp)/(L"tcs-runtime-capacity-"+std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(folder);slotPath=(folder/L"save0").string();
    nativeSave=saveFixture;
    CHECK(reinterpret_cast<Save>(0x5439d0)(0,buffer.data(),buffer.size(),0)==1);
    CHECK(std::filesystem::exists(sidecar(0)));
    ownership[999]=0;saveCopy(buffer.data(),(void*)0x8668c8,gameSize);saveFails=true;
    CHECK(reinterpret_cast<Save>(0x5439d0)(0,buffer.data(),buffer.size(),0)==0);
    saveFails=false;
    // Even native success must not commit a sidecar after a short disk write.
    *reinterpret_cast<int*>(0x921dc0)=1;saveCopy(buffer.data(),(void*)0x8668c8,gameSize);shortWrite=true;
    CHECK(reinterpret_cast<Save>(0x5439d0)(1,buffer.data(),buffer.size(),0)==1);
    CHECK(!std::filesystem::exists(sidecar(1)));shortWrite=false;
    *reinterpret_cast<int*>(0x921dc0)=0;
    // Reordering extra characters retains ownership by identity, independent of ID.
    auto first=names[999].c_str(), second=names[998].c_str();
    memcpy(chars.data()+998*0x4c+0xc,&first,4);memcpy(chars.data()+999*0x4c+0xc,&second,4);
    ownership.fill(0);loadCopy((void*)0x8668c8,buffer.data(),gameSize);
    CHECK(ownership[998]==3&&ownership[999]==0&&ownership[340]==3);
    // Stale native purchase bits cannot hide an unowned character after reorder.
    identityLoaded=true;*reinterpret_cast<unsigned*>(0x86e4a8)=1;ownership[0]=0;
    allocateItems(nullptr,nullptr,0);CHECK(!(purchased[0]&1));
    items[0].purchased=1;updateItems();CHECK(!items[0].purchased);
    // Another save slot has no companion and must not inherit extra ownership.
    *reinterpret_cast<int*>(0x921dc0)=1;loadCopy((void*)0x8668c8,buffer.data(),gameSize);
    CHECK(!ownership[340]&&!ownership[998]);*reinterpret_cast<int*>(0x921dc0)=0;
    // A different native snapshot rejects the old companion and clears extras.
    buffer[0]^=1;loadCopy((void*)0x8668c8,buffer.data(),gameSize);
    CHECK(ownership[340]==0&&ownership[998]==0);
    CHECK(*reinterpret_cast<unsigned*>(0x86e714)==sentinel);
    auto bridge=newGameHook.bridge;newGameHook.bridge=(unsigned char*)newFixture;
    ownership[999]=3;retained["stale_slot"]=3;
    reinterpret_cast<void(__cdecl*)()>(0x42abf0)();
    CHECK(ownership[0]==3&&!ownership[999]&&retained.empty()&&!snapshotReady);
    newGameHook.bridge=bridge;
    std::filesystem::remove(sidecar(0));std::filesystem::remove(pathFixture(0));
    std::filesystem::remove(pathFixture(1));std::filesystem::remove(folder);
#ifdef TCS_NATIVE_SWITCH_FIXTURE
    CHECK(nativeSwitchTests()==0);
    CHECK(nativePortraitRegistryTests()==0);
#endif
    for(auto p=patches.rbegin();p!=patches.rend();++p)CHECK(writePatch(*p,true));
    CHECK(saveHook.remove()&&newGameHook.remove());
    return 0;
}
extern "C" __declspec(dllexport) int __cdecl RunFixture(){return runtimeTests();}
int main(int argc,char**) {return argc==1?dataTests():runtimeTests();}
