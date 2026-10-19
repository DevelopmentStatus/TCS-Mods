#pragma once
#include <cmath>
#include <cstdint>
namespace fps {
enum class SecondaryAction { None,Zoom,Block };
inline SecondaryAction Secondary(uint32_t modelFlags,int weaponModel,bool hasBlockAnimation) {
    bool saber=(weaponModel&0xfffd)==0x65 || (weaponModel&0xfffd)==0x69;
    if(saber || ((modelFlags&0x80)==0 && hasBlockAnimation)) return SecondaryAction::Block;
    return (modelFlags&0x80)!=0?SecondaryAction::Zoom:SecondaryAction::None;
}
struct Movement { float x=0,z=0,magnitude=0; uint16_t angle=0; };
inline Movement PcMovement(bool forward,bool back,bool left,bool right,bool walk) {
    Movement result;
    result.x=float(right)-float(left); result.z=float(forward)-float(back);
    float length=std::sqrt(result.x*result.x+result.z*result.z);
    if(length>0) {
        result.x/=length; result.z/=length; result.magnitude=walk?0.45f:1.0f;
        result.angle=static_cast<uint16_t>(static_cast<int>(std::atan2(result.x,result.z)*10430.378f)&0xffff);
    }
    return result;
}
struct ButtonEdges { uint32_t held,pressed,released; };
inline ButtonEdges Buttons(uint32_t held,uint32_t previous) { return {held,held&~previous,previous&~held}; }
}
