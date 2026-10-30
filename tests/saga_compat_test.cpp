#include "../src/saga_compat.h"
#include <cstdio>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"Failed: %s line %d\n",#x,__LINE__);return 1;} } while(0)
int wmain(int argc,wchar_t** argv) {
    CHECK(!saga_compat::supportedFile(L"does-not-exist.asi"));
    if(argc>1)CHECK(saga_compat::supportedFile(argv[1])); // Hash only; never loads the ASI in this test.
    unsigned char code[]{0x83,0xec,0x1c,0x8b,0x15,0,0,0,0};
    unsigned char frame[40]{};
    const unsigned char pattern[]{0xc7,0x44,0x24,0x18};
    const unsigned char pattern2[]{0xc7,0x44,0x24,0x1c};
    std::memcpy(frame+8,pattern,4);std::memcpy(frame+16,pattern2,4);
    auto pointer=reinterpret_cast<saga_compat::PauseUpdate>(&code[0]);
    std::memcpy(frame+20,&pointer,sizeof(pointer));
    auto owned=[&](const void* p){return p==code;};
    CHECK(saga_compat::resolvePauseBytes(frame,sizeof(frame),owned)==pointer);
    CHECK(!saga_compat::resolvePauseBytes(frame,16,owned));
    CHECK(!saga_compat::resolvePauseBytes(frame,sizeof(frame),[](const void*){return false;}));
    code[0]=0x90;CHECK(!saga_compat::resolvePauseBytes(frame,sizeof(frame),owned));
    saga_compat::active=false;CHECK(!saga_compat::suppressedPlugin(L"tcs.skipsplash"));
    saga_compat::active=true;
    CHECK(saga_compat::suppressedPlugin(L"tcs.skipsplash"));
    CHECK(!saga_compat::suppressedPlugin(L"tcs.modernoptions"));
    CHECK(!saga_compat::suppressedPlugin(L"tcs.firstperson"));
    int mode=0,sync=0,cap=0;
    static int applied=0;
    saga_compat::video={&mode,&sync,&cap,+[]{},+[]{++applied;}};
    CHECK(saga_compat::publishVideo(0,1) && mode==1 && sync==1);
    CHECK(saga_compat::publishVideo(1,0) && mode==2 && sync==0);
    CHECK(saga_compat::publishVideo(2,1) && mode==0);
    saga_compat::publishCap(120);CHECK(cap==120 && applied==1);
    return 0;
}
