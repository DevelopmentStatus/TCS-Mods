// Verify the production handoff exits the startup, passes the correct folder
// and PID, and leaves only the manager in control of the next launch.
#define WIN32_LEAN_AND_MEAN
#include "../src/startup_preparation.h"
#include <filesystem>
#include <fstream>
#include <cstdio>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"Failed: %s line %d\n",#x,__LINE__);return 1;} } while(0)
int wmain(int argc,wchar_t** argv) {
    namespace fs=std::filesystem;
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    auto self=fs::path(executable);
    if(self.filename()==L"TCSAssetManager.exe") {
        CHECK(argc==6&&!wcscmp(argv[1],L"--game")&&!wcscmp(argv[3],L"--manage")&&!wcscmp(argv[4],L"--wait-for-game"));
        auto game=fs::canonical(argv[2]);
        const auto parent=OpenProcess(SYNCHRONIZE,FALSE,wcstoul(argv[5],nullptr,10));
        if(parent){CHECK(WaitForSingleObject(parent,15000)==WAIT_OBJECT_0);CloseHandle(parent);}
        std::ofstream marker(game/L"manager-opened.txt");marker<<"startup exited; manager owns launch";return 0;
    }
    if(argc==2&&!wcscmp(argv[1],L"--game-child")) {
        startup_preparation::handoff(self.parent_path().wstring()+L"\\",nullptr);
    }
    const auto folder=self.parent_path()/(L"handoff test "+std::to_wstring(GetCurrentProcessId()));
    CHECK(fs::weakly_canonical(folder).parent_path()==fs::canonical(self.parent_path()));
    fs::create_directories(folder);
    fs::copy_file(self,folder/L"LEGOStarWarsSaga.exe");
    fs::copy_file(self,folder/L"TCSAssetManager.exe");
    const auto game=folder/L"LEGOStarWarsSaga.exe";
    std::wstring command=L"\""+game.wstring()+L"\" --game-child";
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
    CHECK(CreateProcessW(game.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,folder.c_str(),&startup,&child));
    CHECK(WaitForSingleObject(child.hProcess,15000)==WAIT_OBJECT_0);
    DWORD code=1;CHECK(GetExitCodeProcess(child.hProcess,&code));
    CloseHandle(child.hProcess);CloseHandle(child.hThread);CHECK(code==0);
    for(int i=0;i<150&&!fs::exists(folder/L"manager-opened.txt");++i)Sleep(100);
    CHECK(fs::exists(folder/L"manager-opened.txt"));
    // The manager stub closes its marker immediately before exit.
    Sleep(100);
    CHECK(fs::weakly_canonical(folder).parent_path()==fs::canonical(self.parent_path()));
    fs::remove_all(folder);return 0;
}
