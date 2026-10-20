#include "shop_ownership.h"
#include <cstring>
#include <cstdio>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"Failed line %d: %s\n",__LINE__,#x);return 1;}} while(0)
int main() {
    using namespace shop_ownership;
    Item items[5]{};
    for(unsigned i=0;i<5;++i){items[i].type=1;items[i].character=static_cast<std::uint16_t>(i);items[i].price=1000+i;}
    items[3].type=2;items[4].character=500;
    const std::uint8_t ownership[]{3,2,0,3,3};
    std::uint32_t bits[4]{};auto originalBits=std::array<std::uint32_t,4>{};
    auto originalItems=std::array<Item,5>{};std::memcpy(originalItems.data(),items,sizeof(items));
    Sync sync;
    CHECK(sync.apply(items,5,ownership,5,bits,true)==1);
    CHECK(items[0].purchased==1&&items[1].purchased==0&&items[2].purchased==0&&items[3].purchased==0&&items[4].purchased==0);
    CHECK(sync.apply(items,5,ownership,5,bits,true)==0);
    CHECK(!std::memcmp(bits,originalBits.data(),sizeof(bits)));
    for(unsigned i=0;i<5;++i){auto copy=items[i];copy.purchased=originalItems[i].purchased;CHECK(!std::memcmp(&copy,&originalItems[i],sizeof(Item)));}
    sync.apply(items,5,ownership,5,bits,false);CHECK(items[0].purchased==0);
    sync.apply(items,5,ownership,5,bits,true);bits[0]|=1;
    sync.apply(items,5,ownership,5,bits,false);CHECK(items[0].purchased==1);
    CHECK(sync.apply(items,101,ownership,5,bits,true)==0);
    CHECK(sync.apply(nullptr,5,ownership,5,bits,true)==0);
    // Reused slot now references an unowned character: discard stale corrections.
    Sync replaced;items[0].purchased=0;replaced.apply(items,5,ownership,5,bits,true);
    items[0].character=2;items[0].purchased=0;
    replaced.apply(items,5,ownership,5,bits,false);CHECK(items[0].purchased==0);
    return 0;
}
