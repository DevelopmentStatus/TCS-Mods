// Exercise the actual frozen helper with native EXE and Steam stand-ins.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdio>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"Failed: %s line %d\n",#x,__LINE__);return 1;} } while(0)
int wmain(int argc,wchar_t** argv) {
    namespace fs=std::filesystem;
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    auto self=fs::path(executable);
    if(self.filename()!=L"launch_environment_test.exe") {
        wchar_t directory[32768]{};
        auto length=GetDllDirectoryW(32768,directory);
        wchar_t reviewed[128]{};
        GetEnvironmentVariableW(L"TCS_PRIORITY_REVIEWED",reviewed,128);
        std::ofstream marker("launch-environment.txt");
        marker<<"dll="<<length<<"\nreviewed="<<(reviewed[0]?1:0)<<"\nsteam="
              <<(argc==3&&!wcscmp(argv[1],L"-applaunch")&&!wcscmp(argv[2],L"32440"))<<"\n";
        return 0;
    }
    CHECK(argc==2);
    auto folder=self.parent_path()/(L"launch-test-"+std::to_wstring(GetCurrentProcessId()));
    CHECK(fs::weakly_canonical(folder).parent_path()==fs::canonical(self.parent_path()));
    for(bool steam:{false,true}) {
        auto game=steam?folder/L"steamapps/common/Fixture":folder/L"standalone";
        fs::create_directories(game/L"Mods");
        fs::copy_file(self,game/L"LEGOStarWarsSaga.exe");
        auto helper=game/L"TCSAssetManager.exe";fs::copy_file(argv[1],helper);
        if(steam) {
            auto client=folder/L"Steam Client/steam.exe";
            fs::create_directories(client.parent_path());fs::copy_file(self,client);
            std::ofstream manifest(folder/L"steamapps/appmanifest_32440.acf");
            manifest<<"\"appid\" \"32440\"\n\"installdir\" \"Fixture\"\n\"LauncherPath\" \""<<client.string()<<"\"\n";
        }
        std::wstring command=L"\""+helper.wstring()+L"\" --game \""+game.wstring()+L"\" --launch";
        STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
        CHECK(CreateProcessW(helper.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,game.c_str(),&startup,&child));
        CHECK(WaitForSingleObject(child.hProcess,120000)==WAIT_OBJECT_0);
        DWORD code=1;CHECK(GetExitCodeProcess(child.hProcess,&code));
        CloseHandle(child.hProcess);CloseHandle(child.hThread);CHECK(code==0);
        auto marker=game/L"launch-environment.txt";
        for(int i=0;i<150&&!fs::exists(marker);++i)Sleep(100);
        CHECK(fs::exists(marker));
        std::ifstream stream(marker);std::string data((std::istreambuf_iterator<char>(stream)),{});
        CHECK(data.find("dll=0")!=std::string::npos);
        CHECK(data.find(steam?"steam=1":"steam=0")!=std::string::npos);
        if(!steam)CHECK(data.find("reviewed=1")!=std::string::npos);
    }
    CHECK(fs::weakly_canonical(folder).parent_path()==fs::canonical(self.parent_path()));
    fs::remove_all(folder);return 0;
}
