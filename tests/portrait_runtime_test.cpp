#include "../src/portrait_runtime.h"
#include <cstdio>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"Failed: %s line %d\n",#x,__LINE__);return 1;} } while(0)
using namespace portrait_runtime;
static int loadCalls=0,readCalls=0,removed=0,nativeDumpCalls=0;
static bool originalWins=false;
static std::vector<short> loaded;
void __cdecl nativeLoad(const Model* list,int,void**,void**) {
    ++loadCalls;
    if(list)for(;list->id!=-1;++list)loaded.push_back(list->id);
}
void* __cdecl nativeRead(void**,void*,const char*) {++readCalls;return reinterpret_cast<void*>(123);}
void __cdecl nativeRemove(void*) {++removed;}
void __cdecl nativeDump() {++nativeDumpCalls;}
int __cdecl nativeFind(void* scene,void* output,const char*,int) {
    if(originalWins || scene==reinterpret_cast<void*>(123)) {*static_cast<void**>(output)=scene;return 1;}
    return 0;
}
int main() {
    wanted={"revan_icon","grogu_icon"};
    const char* names[]{"base_icon","REVAN_ICON",nullptr,"grogu_icon"};
    auto list=preloadList(4,[&](int id){return names[id];});
    CHECK(list.size()==3 && list[0].id==1 && list[1].id==3 && list[2].id==-1);
    CHECK(preloadList(5000,[&](int){return names[0];}).empty());
    loadHook.bridge=reinterpret_cast<unsigned char*>(nativeLoad);
    dumpHook.bridge=reinterpret_cast<unsigned char*>(nativeDump);
    findHook.bridge=reinterpret_cast<unsigned char*>(nativeFind);
    readScene=nativeRead;removeScene=nativeRemove;
    fallbacks={{"stuff\\icons\\test.gsc",{"lost_icon"},nullptr}};
    void* buffer=reinterpret_cast<void*>(1);void* end=reinterpret_cast<void*>(1000);
    const Model base[]{{0,1},{-1,0}};
    loadExtra(base,1,&buffer,&end,list);
    CHECK(loadCalls==2 && loaded==std::vector<short>({0,1,3}) && readCalls==1);
    loadExtra(base,1,&buffer,&end,list);
    CHECK(readCalls==1); // A second loader call in this area reuses the scene.
    void* output{};
    CHECK(findSpecial(buffer,&output,"LOST_ICON",1)==1 && output==reinterpret_cast<void*>(123));
    CHECK(findSpecial(buffer,&output,"unrelated_object",1)==0);
    originalWins=true;CHECK(findSpecial(buffer,&output,"lost_icon",1)==1 && output==buffer);
    originalWins=false;
    dumpIcons();CHECK(removed==1 && nativeDumpCalls==1 && !fallbacks[0].scene);
    CHECK(findSpecial(buffer,&output,"lost_icon",1)==0);
    loadExtra(base,1,&buffer,&end,list);CHECK(readCalls==2);
    dumpIcons();CHECK(removed==2 && nativeDumpCalls==2);
    return 0;
}
