// Exercise production lookup and fallback without opening or patching the game.
#include "../src/asset_overlay.cpp"
#include <cstdlib>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"Failed: %s line %d\n",#x,__LINE__);return 1;} } while(0)
static std::string lastName;
static int lastMode,lastQuiet;
static void* lastHeader;
static bool failOverride=false;
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
    return 0;
}
