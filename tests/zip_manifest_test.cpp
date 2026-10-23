#include "zip_manifest.h"
#include <cstdio>
#include <iostream>
int main(int argc,char** argv) {
    if(argc>1) {
        for(int i=1;i<argc;++i)std::cout<<argv[i]<<": "<<(ZipHasRootManifest(argv[i])?"plugin":"asset/non-plugin")<<"\n";
        return 0;
    }
    auto path=std::filesystem::path("zip-manifest-test.tmp");
    auto write=[&](std::string name,std::string comment="") {
        std::vector<unsigned char> data(46,0);data[0]=0x50;data[1]=0x4b;data[2]=1;data[3]=2;
        data[28]=static_cast<unsigned char>(name.size());data.insert(data.end(),name.begin(),name.end());
        unsigned length=static_cast<unsigned>(data.size());
        std::vector<unsigned char> e(22,0);e[0]=0x50;e[1]=0x4b;e[2]=5;e[3]=6;e[8]=e[10]=1;
        for(int i=0;i<4;++i)e[12+i]=static_cast<unsigned char>(length>>(8*i));
        e[20]=static_cast<unsigned char>(comment.size());
        data.insert(data.end(),e.begin(),e.end());data.insert(data.end(),comment.begin(),comment.end());
        std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<char*>(data.data()),data.size());
    };
    write("manifest.ini");bool good=ZipHasRootManifest(path);
    write("MANIFEST.INI","comment");good=good&&ZipHasRootManifest(path);
    write("nested/manifest.ini");good=good&&!ZipHasRootManifest(path);
    write("CHARS/HERO/HERO_PC.GHG");good=good&&!ZipHasRootManifest(path);
    {std::ofstream f(path);f<<"invalid ZIP";}
    good=good&&!ZipHasRootManifest(path);std::filesystem::remove(path);
    if(!good){std::fprintf(stderr,"ZIP root-manifest filtering failed\n");return 1;}
    return 0;
}
