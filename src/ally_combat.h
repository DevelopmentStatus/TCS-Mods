#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ally_combat {
enum class Action { Native, Recover, Turn, Attack, Defend, DrawWeapon };
struct State {
    std::uintptr_t target{};
    float attackWait{}, defendWait{}, defending{};
};
struct Input {
    std::uintptr_t target{};
    bool engaged{}, ready{}, ranged{}, canBlock{}, canDefend{}, weaponReady{};
    bool enemyAttacking{}, enemyFacingUs{};
    float distance{}, height{}, facing{}, dt{};
};
inline Action decide(State& state, const Input& in) {
    if(!in.engaged || !in.target || !std::isfinite(in.distance) || !std::isfinite(in.height) ||
       in.distance < 0 || in.distance > 10 || std::fabs(in.height) > 1.5f) {
        state = {}; return Action::Native;
    }
    if(state.target != in.target) state = State{in.target};
    if(!std::isfinite(in.dt) || in.dt <= 0 || in.dt > 0.25f) return Action::Native;
    state.attackWait = std::max(0.0f, state.attackWait - in.dt);
    state.defendWait = std::max(0.0f, state.defendWait - in.dt);
    state.defending = std::max(0.0f, state.defending - in.dt);
    if(!in.ready) return Action::Native;
    const float reach = in.ranged ? 8.0f : 0.85f;
    if(in.distance > reach || (!in.ranged && std::fabs(in.height) > 0.55f)) return Action::Native;
    if(!in.weaponReady) return Action::DrawWeapon;
    if(in.canBlock && in.canDefend && in.distance < 1.2f && in.enemyAttacking &&
       in.enemyFacingUs && state.defendWait <= 0) {
        state.defending = 0.20f;
        state.defendWait = 1.10f;
    }
    if(state.defending > 0 && in.canDefend) return Action::Defend;
    if(!std::isfinite(in.facing) || in.facing < (in.ranged ? 0.90f : 0.35f)) return Action::Turn;
    if(state.attackWait > 0) return Action::Recover;
    state.attackWait = in.ranged ? 0.48f : 0.38f;
    return Action::Attack;
}
}
