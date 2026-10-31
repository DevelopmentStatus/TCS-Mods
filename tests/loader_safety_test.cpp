#include "../src/loader.cpp"
#include <array>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"Failed: %s line %d\n",#x,__LINE__);return 1;} } while(0)
int main() {
    std::array<unsigned char,4096> header{};
    IMAGE_DOS_HEADER dos{};dos.e_magic=IMAGE_DOS_SIGNATURE;dos.e_lfanew=128;
    std::memcpy(header.data(),&dos,sizeof(dos));
    IMAGE_NT_HEADERS32 pe{};
    pe.Signature=IMAGE_NT_SIGNATURE;pe.FileHeader.Machine=IMAGE_FILE_MACHINE_I386;
    pe.FileHeader.SizeOfOptionalHeader=sizeof(IMAGE_OPTIONAL_HEADER32);
    pe.FileHeader.TimeDateStamp=0x4ac4a20b;
    pe.OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    pe.OptionalHeader.ImageBase=0x400000;pe.OptionalHeader.SizeOfImage=0x268e000;
    std::memcpy(header.data()+128,&pe,sizeof(pe));
    CHECK(SupportedGameHeader(header.data(),header.size()));
    CHECK(!SupportedGameHeader(header.data(),128));
    pe.FileHeader.TimeDateStamp++;
    std::memcpy(header.data()+128,&pe,sizeof(pe));
    CHECK(!SupportedGameHeader(header.data(),header.size()));
    dos.e_lfanew=INT_MAX;std::memcpy(header.data(),&dos,sizeof(dos));
    CHECK(!SupportedGameHeader(header.data(),header.size()));
    CHECK(!SupportedGameHost());
    // Calling the proxy from a different host must not create logs, start the
    // helper, register plugins, or patch the returned native D3D interface.
    auto d3d=ProxyCreate9(D3D_SDK_VERSION);
    CHECK(root.empty() && !logFile && plugins.empty() && !realCreateDevice);
    if(d3d)d3d->Release();
    // A failed protection change must not be followed by a write/read of the
    // inaccessible vtable. A reserve-only allocation reliably exercises it.
    auto reserved=VirtualAlloc(nullptr,4096,MEM_RESERVE,PAGE_NOACCESS);CHECK(reserved);
    void** inaccessible=static_cast<void**>(reserved);
    deviceCreated(nullptr,reinterpret_cast<IDirect3DDevice9*>(&inaccessible));
    CHECK(!hookedDevice && !realReset && !realPresent && !realEndScene);
    VirtualFree(reserved,0,MEM_RELEASE);
    // Two graphics-device notifications for one HWND must retain its original
    // window procedure, instead of making GameProc call itself recursively.
    auto window=CreateWindowExW(0,L"STATIC",L"TCS test",0,0,0,1,1,nullptr,nullptr,nullptr,nullptr);
    CHECK(window);
    void* entries[43]{};void** table=entries;
    auto fake=reinterpret_cast<IDirect3DDevice9*>(&table);
    deviceCreated(window,fake);
    const auto previous=GetPropW(window,L"TCSOriginalProc");CHECK(previous);
    deviceCreated(window,fake);
    CHECK(GetPropW(window,L"TCSOriginalProc")==previous);
    CHECK(previous!=reinterpret_cast<HANDLE>(GameProc));
    SendMessageW(window,WM_APP+100,0,0);
    DestroyWindow(window);
    return 0;
}
