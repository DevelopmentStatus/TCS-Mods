#pragma once
#include <cstddef>
#include <cstdint>
#include <array>

namespace shop_ownership {
constexpr std::uintptr_t updateAddress=0x4c15e0;
constexpr std::uint8_t updateSignature[]{0x57,0x33,0xff,0x66,0x39,0x3d,0xfe,0x5e,0x85,0x00};
constexpr std::size_t capacity=100;
struct Item {
    std::uint8_t presentation[0x60];
    std::uint16_t character;
    std::uint8_t type,purchased;
    std::uint32_t price;
    std::uint8_t special[12];
};
static_assert(sizeof(Item)==0x74);
static_assert(offsetof(Item,purchased)==0x63);
// Remember only the UI flags this plugin raised. Native purchase bits and
// character ownership remain read-only, including when the option is disabled.
struct Sync {
    struct Change { std::uint16_t character{}; std::uint8_t original{}; bool raised{}; };
    std::array<Change,capacity> changes{};
    Item* lastItems{};
    const std::uint8_t* lastOwnership{};
    unsigned apply(Item* items,std::size_t count,const std::uint8_t* ownership,
                   std::size_t characterCount,const std::uint32_t* bought,bool enabled) {
        if(!items||!ownership||!bought||count>capacity||!characterCount)return 0;
        if(items!=lastItems||ownership!=lastOwnership) {
            changes={};lastItems=items;lastOwnership=ownership;
        }
        unsigned corrected=0;
        for(std::size_t i=0;i<count;++i) {
            auto& item=items[i];auto& change=changes[i];
            if(item.type!=1||item.character>=characterCount) {change={};continue;}
            if(change.raised&&change.character!=item.character)change={};
            const bool owned=(ownership[item.character]&1)!=0;
            if(enabled&&owned) {
                if(item.purchased!=1) {
                    change={item.character,item.purchased,true};item.purchased=1;++corrected;
                }
            } else if(change.raised) {
                // A legitimate purchase after the correction takes precedence.
                if(item.purchased==1)item.purchased=(bought[i/32]&(1u<<(i%32)))?1:change.original;
                change={};
            }
        }
        return corrected;
    }
};
}
