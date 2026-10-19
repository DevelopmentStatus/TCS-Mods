#pragma once
#include <cstdint>
namespace ally_damage {
constexpr std::uintptr_t combatAddress = 0x43f7f8;
inline constexpr std::uint8_t combatSignature[] = {0x8b,0x56,0x54,0x8b,0x42,0x24};
inline constexpr std::uint8_t guardSignature[] = {0x8b,0x15,0xe4,0x30,0x91,0x02};
constexpr std::uintptr_t flagsAddress = 0x58b720;
constexpr std::uintptr_t hitAddress = 0x438230;
constexpr std::uintptr_t boltFilterAddress = 0x5815ad;
inline constexpr std::uint8_t boltFilterSignature[] = {0x8b,0x88,0xfc,0x01,0x00,0x00};
inline constexpr std::uint8_t flagsSignature[] = {0x8b,0x54,0x24,0x04,0x33,0xc0};
inline constexpr std::uint8_t hitSignature[] = {0x83,0xec,0x4c,0xa1,0x70,0x95,0x82,0x00};
// PC offsets, verified against the runtime analysis image (not Android layout).
constexpr std::uintptr_t partyAddress = 0x93d7f0;
constexpr unsigned partySize = 8;
constexpr unsigned stateOffset = 0x1f8, flagsOffset = 0x1fc, slotOffset = 0x280;
inline bool isAllyAI(std::uint32_t flags, unsigned slot) {
    return (flags & 0x1081) == 0x1001 &&
        slot < partySize;
}
inline unsigned playerHitFlags(unsigned flags) {
    // Preserve weapon, shield, scripted and probe semantics. Bit 2 suppresses
    // damage to NPCs; bit 4 selects the normal player-side allegiance filter.
    return (flags & ~2u) | 0x804u;
}
}
