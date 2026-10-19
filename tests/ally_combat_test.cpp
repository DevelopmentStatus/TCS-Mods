#include "ally_combat.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
using namespace ally_combat;
int main() {
    State s{};
    Input in{1,true,true,false,true,true,true,false,true,0.55f,0,1,1.0f/60};
    CHECK(decide(s,in)==Action::Attack);
    CHECK(decide(s,in)==Action::Recover);
    int attacks=1;
    for(int i=0;i<120;++i) if(decide(s,in)==Action::Attack) ++attacks;
    CHECK(attacks>=5 && attacks<=7); // paced real-time attacks, not one per frame
    in.enemyAttacking=true; s={};
    CHECK(decide(s,in)==Action::Defend);
    int blocks=1;attacks=0;
    for(int i=0;i<60;++i) { auto a=decide(s,in); blocks+=a==Action::Defend; attacks+=a==Action::Attack; }
    CHECK(blocks<=14 && attacks>=2); // sustained threat still permits counterattacks
    in.enemyFacingUs=false;s={};CHECK(decide(s,in)==Action::Attack);
    in.ready=false;CHECK(decide(s,in)==Action::Native); // do not interrupt interactions
    in.ready=true;in.weaponReady=false;CHECK(decide(s,in)==Action::DrawWeapon);
    in.weaponReady=true;in.facing=-1;CHECK(decide(s,in)==Action::Turn);
    in.distance=2;CHECK(decide(s,in)==Action::Native); // native pathfinding closes distance
    in.ranged=true;in.canBlock=false;in.facing=1;s={};CHECK(decide(s,in)==Action::Attack);
    in.distance=9;CHECK(decide(s,in)==Action::Native);
    in.distance=3;in.height=2;CHECK(decide(s,in)==Action::Native);
    in.height=0;in.engaged=false;CHECK(decide(s,in)==Action::Native);CHECK(!s.target);
    in.engaged=true;in.target=2;in.dt=0;CHECK(decide(s,in)==Action::Native);
    in.dt=0.016f;in.distance=std::numeric_limits<float>::quiet_NaN();CHECK(decide(s,in)==Action::Native);
    std::puts("Combat decisions: attack pacing, bounded defense/counterattack, range, aim, weapon draw and native context preservation passed.");
}
