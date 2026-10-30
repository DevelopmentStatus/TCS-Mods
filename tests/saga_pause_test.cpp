#include "../src/native_pause_menu.cpp"
#include <cstdio>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"Failed: %s line %d\n",#x,__LINE__);return 1;} } while(0)
static int forwarded=-1,forwardedTweaks=-1,opened=0,tweaksRow=8,entries=0;
void __cdecl fakeEntry(void* menu,const char*) {++entries;++field<short>(menu,0x8c);}
void __cdecl saga(void* menu) {forwarded=field<int>(menu,0x34);forwardedTweaks=tweaksRow;}
int main() {
    alignas(4) unsigned char menu[0xe0]{};
    nativeDrawEntry=fakeEntry;
    settingsOpen=+[]{return false;};drawSettings=+[](void*){};
    updateSettings=+[](void*){};writeLog=+[](const char*){};
    openSettings=+[](void*){++opened;};
    ConfigureSagaPauseMenu(saga,&tweaksRow);
    // The patched Effects call draws Mod Settings even before the runtime
    // menu table has copied the draw wrapper during game initialization.
    field<short>(menu,0x8c)=4;drawEffectsAndMods(menu,"Effects Settings");
    CHECK(entries==2 && field<short>(menu,0x8c)==6 && NativeModSettingsRow()==5);
    field<int>(menu,0x34)=5;field<int>(menu,0x64)=1;
    updatePause(menu);CHECK(opened==1 && forwarded==-1);
    for(int oldTweaks=6;oldTweaks<=8;++oldTweaks) {
        tweaksRow=oldTweaks;
        for(int selected=0;selected<=oldTweaks+1;++selected) {
            if(selected==5)continue;
            field<int>(menu,0x34)=selected;field<short>(menu,0xa)=static_cast<short>(selected);
            updatePause(menu);
            CHECK(forwarded==tcs::originalPauseRow(selected));
            CHECK(forwardedTweaks==tcs::originalPauseRow(oldTweaks));
            CHECK(tweaksRow==oldTweaks && field<int>(menu,0x34)==selected && field<short>(menu,0xa)==selected);
        }
    }
    uintptr_t draw=0x495b50,update=0x495900;
    auto write=[&](uintptr_t slot,uintptr_t value){if(slot==runtimeDrawSlot)draw=value;else update=value;};
    CHECK(RepairPauseSlots(draw,update,write));
    CHECK(draw==reinterpret_cast<uintptr_t>(drawPause) && update==reinterpret_cast<uintptr_t>(updatePause));
    update=reinterpret_cast<uintptr_t>(saga);
    CHECK(RepairPauseSlots(draw,update,write) && update==reinterpret_cast<uintptr_t>(updatePause));
    // Never replace an unrecognized third-party draw/update pair.
    CHECK(!RepairPauseSlots(123,456,write));
    ConfigureSagaPauseMenu(nullptr);
    CHECK(!RepairPauseSlots(0x495b50,0,write));
    return 0;
}
