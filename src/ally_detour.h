#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>

// Only for verified position-independent prologues ending on instruction
// boundaries, with no relative instructions. Also used by Shop Ownership Fix.
struct AllyDetour {
    std::uint8_t* site{};
    std::uint8_t* bridge{};
    std::uint8_t original[16]{};
    std::uint8_t patched[16]{};
    std::size_t length{};
    static void jump(std::uint8_t* at, const void* to) {
        at[0] = 0xe9;
        auto delta = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(to) -
            reinterpret_cast<std::uintptr_t>(at + 5));
        std::memcpy(at + 1, &delta, 4);
    }
    bool install(void* address, const void* hook, const std::uint8_t* expected, std::size_t size) {
        if(site || size < 5 || size > sizeof(original) || std::memcmp(address, expected, size)) return false;
        auto code = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, size + 5, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if(!code) return false;
        std::memcpy(code, address, size);
        jump(code + size, static_cast<std::uint8_t*>(address) + size);
        DWORD previous{};
        if(!VirtualProtect(code, size + 5, PAGE_EXECUTE_READ, &previous)) {
            VirtualFree(code, 0, MEM_RELEASE); return false;
        }
        FlushInstructionCache(GetCurrentProcess(), code, size + 5);
        std::memcpy(original, address, size);
        std::memset(patched, 0x90, size);
        // Compute the jump relative to its final address, not this buffer.
        patched[0] = 0xe9;
        auto delta = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(hook) -
            (reinterpret_cast<std::uintptr_t>(address) + 5));
        std::memcpy(patched + 1, &delta, 4);
        if(!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &previous)) {
            VirtualFree(code, 0, MEM_RELEASE); return false;
        }
        site = static_cast<std::uint8_t*>(address); bridge = code; length = size;
        std::memcpy(site, patched, size);
        FlushInstructionCache(GetCurrentProcess(), site, size);
        DWORD unused{}; VirtualProtect(site, size, previous, &unused);
        return true;
    }
    bool remove() {
        if(!site) return true;
        if(std::memcmp(site, patched, length)) return false;
        DWORD previous{};
        if(!VirtualProtect(site, length, PAGE_EXECUTE_READWRITE, &previous)) return false;
        std::memcpy(site, original, length);
        FlushInstructionCache(GetCurrentProcess(), site, length);
        DWORD unused{}; VirtualProtect(site, length, previous, &unused);
        VirtualFree(bridge, 0, MEM_RELEASE);
        site = bridge = nullptr; length = 0;
        return true;
    }
};
