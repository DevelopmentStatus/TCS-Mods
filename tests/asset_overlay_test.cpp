// Exercise production lookup and fallback without opening or patching the game.
#include "../src/asset_overlay.cpp"
#include <cstdlib>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"Failed: %s line %d\n",#x,__LINE__);return 1;} } while(0)
static std::string lastName;
static int lastMode,lastQuiet;
static void* lastHeader;
static bool failOverride=false;
static bool variantRegistered=false;
TCSModHandle __cdecl testRegisterMod(const TCSModInfo*) {return reinterpret_cast<TCSModHandle>(1);}
BOOL __cdecl testRegisterSetting(TCSModHandle,const TCSSettingDescriptor* setting) {
    if(std::strcmp(setting->key,"Variant0")==0) {
        variantRegistered=setting->type==TCS_SETTING_ENUM && setting->choiceCount==3 &&
            std::strcmp(setting->choices[0],"plain")==0 &&
            std::strcmp(setting->choices[1],"helmet")==0 &&
            std::strcmp(setting->choices[2],"visor")==0;
    }
    return TRUE;
}
int __cdecl nativeOpen(const char* name,int mode,void* header,int quiet) {
    lastName=name;lastMode=mode;lastHeader=header;lastQuiet=quiet;
    return failOverride&&!header?0:79;
}
int __cdecl nativeExists(const char*) {return 17;}
long long __cdecl nativeSize(const char*) {return 12345;}
int __cdecl nativeBuffer(const char* name,void*,int capacity) {lastName=name;return capacity;}
int main() {
    gamePrefix="C:\\GAME\\";
    openHook.bridge=reinterpret_cast<unsigned char*>(nativeOpen);
    existsHook.bridge=reinterpret_cast<unsigned char*>(nativeExists);
    sizeHook.bridge=reinterpret_cast<unsigned char*>(nativeSize);
    bufferHook.bridge=reinterpret_cast<unsigned char*>(nativeBuffer);
    char filename[MAX_PATH]{};GetModuleFileNameA(nullptr,filename,MAX_PATH);
    overrides["CHARS\\HERO\\HERO_PC.GHG"]=filename;
    CHECK(replacement("chars/hero/hero_pc.ghg"));
    CHECK(replacement("C:\\game\\chars\\hero\\hero_pc.ghg"));
    CHECK(!replacement("..\\CHARS\\HERO\\HERO_PC.GHG"));
    void* header=reinterpret_cast<void*>(4);
    CHECK(openAsset("CHARS/HERO/HERO_PC.GHG",0,header,3)==79);
    CHECK(lastName==filename&&lastHeader==nullptr&&lastMode==0&&lastQuiet==3);
    CHECK(existsAsset("CHARS/HERO/HERO_PC.GHG")==1);
    CHECK(sizeAsset("CHARS/HERO/HERO_PC.GHG")>0);
    CHECK(bufferAsset("CHARS/HERO/HERO_PC.GHG",nullptr,32)==32&&lastName==filename);
    // Write and append calls must retain their original destination/header.
    CHECK(openAsset("CHARS/HERO/HERO_PC.GHG",1,header,2)==79);
    CHECK(lastHeader==header&&lastMode==1&&lastName=="CHARS/HERO/HERO_PC.GHG");
    failOverride=true;
    CHECK(openAsset("CHARS/HERO/HERO_PC.GHG",0,header,0)==79&&lastHeader==header);
    // Removing a mod removes its mapping: all operations reach native lookup.
    overrides.clear();
    CHECK(openAsset("CHARS/HERO/HERO_PC.GHG",0,header,0)==79&&lastHeader==header);
    CHECK(existsAsset("CHARS/HERO/HERO_PC.GHG")==17);
    CHECK(sizeAsset("CHARS/HERO/HERO_PC.GHG")==12345);
    CHECK(bufferAsset("CHARS/HERO/HERO_PC.GHG",nullptr,32)==32&&lastName=="CHARS/HERO/HERO_PC.GHG");
    // The importer catalog exposes complete archive variants as native settings.
    auto catalog=std::filesystem::path(filename).wstring()+L".test-catalog.ini";
    CHECK(WritePrivateProfileStringW(L"Overlay",L"Count",L"1",catalog.c_str()));
    WritePrivateProfileStringW(L"Package0",L"Id",L"asset.test",catalog.c_str());
    WritePrivateProfileStringW(L"Package0",L"Name",L"Test",catalog.c_str());
    WritePrivateProfileStringW(L"Package0",L"VariantCount",L"1",catalog.c_str());
    WritePrivateProfileStringW(L"Package0",L"Variant0Key",L"Variant0",catalog.c_str());
    WritePrivateProfileStringW(L"Package0",L"Variant0Label",L"Model style",catalog.c_str());
    WritePrivateProfileStringW(L"Package0",L"Variant0Count",L"3",catalog.c_str());
    WritePrivateProfileStringW(L"Package0",L"Variant0Option0",L"plain",catalog.c_str());
    WritePrivateProfileStringW(L"Package0",L"Variant0Option1",L"helmet",catalog.c_str());
    WritePrivateProfileStringW(L"Package0",L"Variant0Option2",L"visor",catalog.c_str());
    TCSModHost host{};host.registerMod=testRegisterMod;host.registerSetting=testRegisterSetting;
    registerPackages(catalog,host);
    DeleteFileW(catalog.c_str());
    CHECK(variantRegistered);
    return 0;
}
