#pragma once
#include <cstdint>
#include <cstdlib>
namespace fps {
struct MouseDelta { int x=0,y=0; };
// TCS consumes the low 16 bits of its accumulated DirectInput coordinates.
class MouseMotion {
    uint16_t previousX=0,previousY=0;
    bool tracking=false;
    static int Difference(uint16_t now,uint16_t previous) {
        int difference=(static_cast<int>(now)-static_cast<int>(previous)+32768)&65535;
        return difference-32768;
    }
public:
    unsigned int rejectedSamples=0;
    void Reset() { tracking=false; }
    MouseDelta Sample(int32_t rawX,int32_t rawY,bool capture) {
        if(!capture) { Reset(); return {}; }
        uint16_t x=static_cast<uint16_t>(rawX),y=static_cast<uint16_t>(rawY);
        MouseDelta delta;
        if(tracking) delta={Difference(x,previousX),Difference(y,previousY)};
        previousX=x; previousY=y; tracking=true;
        // Device acquisition/reset can change the coordinate origin discontinuously.
        if(std::abs(delta.x)>2048 || std::abs(delta.y)>2048) { ++rejectedSamples; return {}; }
        return delta;
    }
};
}
