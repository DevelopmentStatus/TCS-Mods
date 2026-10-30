#include "character_registry.h"
#include <array>
#include <cstdio>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"Failed %d: %s\n",__LINE__,#x);return 1;}} while(0)
using namespace character_registry;
struct Parser {unsigned char padding[0x510];const char* token;};
static Parser parser{};
static std::vector<std::vector<std::string>> lines;
static size_t lineIndex,wordIndex;
static void* __cdecl open(const char*){lineIndex=0;return &parser;}
static int __cdecl nextLine(void*){if(lineIndex==lines.size())return 0;++lineIndex;wordIndex=0;return 1;}
static int __cdecl nextWord(void*) {
    auto& current=lines[lineIndex-1];if(wordIndex==current.size())return 0;
    parser.token=current[wordIndex++].c_str();return 1;
}
static void __cdecl close(void*){}
int main() {
    create=open;line=nextLine;word=nextWord;destroy=close;
    for(unsigned i=0;i<1001;++i) {
        lines.push_back({"CHAR_START"});lines.push_back({"dir","a_long_directory_name_"+std::to_string(i)});
        lines.push_back({"file","unique_character_name_"+std::to_string(i)});lines.push_back({"char_end"});
    }
    auto entries=parse("fixture");CHECK(entries.size()==1000&&entries.back().file=="unique_character_name_999");
    std::vector<unsigned char> memory(512*1024,0xab);void* cursor=memory.data()+1;void* end=memory.data()+memory.size()-4;
    void* runtime=nullptr;int count=-1;
    auto result=static_cast<unsigned char*>(layout(entries,&cursor,&end,&count,0x120,&runtime));
    CHECK(result&&count==1000&&runtime&&reinterpret_cast<uintptr_t>(result)%4==0);
    CHECK(memory.front()==0xab&&memory.back()==0xab);
    for(int i=0;i<count;++i) {
        auto row=result+i*0x4c;
        CHECK(*reinterpret_cast<int*>(row)==-1&&*reinterpret_cast<short*>(row+0x42)==-1);
        CHECK(*reinterpret_cast<float*>(row+0x2c)==1&&*reinterpret_cast<float*>(row+0x34)==-.5f);
        CHECK(entries[i].directory==*reinterpret_cast<char**>(row+8));
        CHECK(entries[i].file==*reinterpret_cast<char**>(row+12));
        CHECK(*reinterpret_cast<void**>(row+0x24)==static_cast<unsigned char*>(runtime)+i*0x120);
    }
    cursor=memory.data()+1;void* originalCursor=cursor;end=memory.data()+100;
    CHECK(!layout(entries,&cursor,&end,&count,0x120,&runtime)&&cursor==originalCursor);
    entries.push_back({"too_many","overflow"});end=memory.data()+memory.size();
    CHECK(!layout(entries,&cursor,&end,&count,0x120,&runtime));
    return 0;
}
