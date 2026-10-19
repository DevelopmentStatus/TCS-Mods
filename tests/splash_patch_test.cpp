#include "splash_patch.h"
#include <vector>
#include <cstdio>

int main() {
    std::vector<std::uint8_t> image(splash::synchronousRva + sizeof(splash::synchronous));
    std::memcpy(image.data() + splash::entryRva, splash::entry, sizeof(splash::entry));
    std::memcpy(image.data() + splash::synchronousRva, splash::synchronous, sizeof(splash::synchronous));
    if(!splash::matches(image.data(), image.size())) return 1;
    if(splash::matches(image.data(), image.size() - 1) || splash::matches(nullptr, image.size())) return 2;
    for(std::size_t i = 0; i < sizeof(splash::entry); ++i) {
        image[splash::entryRva + i] ^= 1;
        if(splash::matches(image.data(), image.size())) return 3;
        image[splash::entryRva + i] ^= 1;
    }
    image[splash::synchronousRva] ^= 1;
    if(splash::matches(image.data(), image.size())) return 4;
    std::int32_t displacement{};
    std::memcpy(&displacement, splash::replacement + 1, 4);
    if(splash::branchRva + 5 + displacement != splash::synchronousRva || splash::replacement[5] != 0x90) return 5;
    std::puts("Startup signatures reject incompatible code; branch reaches native asset-loading path.");
    return 0;
}
