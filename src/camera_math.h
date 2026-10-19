#pragma once
#include <algorithm>
#include <cmath>
#include <array>
namespace fps {
enum class CameraMode { FirstPerson,ThirdPerson,Orbit,Count };
inline CameraMode NextCameraMode(CameraMode mode) { return static_cast<CameraMode>((static_cast<int>(mode)+1)%static_cast<int>(CameraMode::Count)); }
inline bool IsFirstPersonMode(CameraMode mode) { return mode==CameraMode::FirstPerson; }
inline bool UsesCameraAim(CameraMode mode,bool aimBot) { return IsFirstPersonMode(mode) && !aimBot; }
inline std::array<float,3> TransformPoint(const float* m,const std::array<float,3>& p) {
    return {p[0]*m[0]+p[1]*m[4]+p[2]*m[8]+m[12],p[0]*m[1]+p[1]*m[5]+p[2]*m[9]+m[13],p[0]*m[2]+p[1]*m[6]+p[2]*m[10]+m[14]};
}
inline std::array<float,3> RotateOffset(const std::array<float,3>& p,float yaw) {
    return {std::cos(yaw)*p[0]+std::sin(yaw)*p[2],p[1],-std::sin(yaw)*p[0]+std::cos(yaw)*p[2]};
}
inline float BasisDeterminant(const float* m) {
    return m[0]*(m[5]*m[10]-m[6]*m[9])-m[4]*(m[1]*m[10]-m[2]*m[9])+m[8]*(m[1]*m[6]-m[2]*m[5]);
}
inline std::array<float,16> Multiply(const float* a,const float* b) {
    std::array<float,16> result{};
    for(int row=0;row<4;++row) for(int column=0;column<4;++column)
        for(int k=0;k<4;++k) result[row*4+column]+=a[row*4+k]*b[k*4+column];
    return result;
}
inline bool RotationOnly(const float* input,std::array<float,16>& output) {
    std::array<float,3> right{input[0],input[1],input[2]},up{input[4],input[5],input[6]};
    auto normalize=[](std::array<float,3>& v) {
        float length=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
        if(!std::isfinite(length)||length<0.00001f) return false;
        for(float& value:v) value/=length;
        return true;
    };
    if(!normalize(right)) return false;
    float dot=right[0]*up[0]+right[1]*up[1]+right[2]*up[2];
    for(int i=0;i<3;++i) up[i]-=dot*right[i];
    if(!normalize(up)) return false;
    std::array<float,3> forward{right[1]*up[2]-right[2]*up[1],right[2]*up[0]-right[0]*up[2],right[0]*up[1]-right[1]*up[0]};
    output={right[0],right[1],right[2],0,up[0],up[1],up[2],0,forward[0],forward[1],forward[2],0,0,0,0,1};
    return true;
}
inline float LimitLook(float pitch,float upDegrees,float downDegrees) {
    constexpr float radians=3.14159265358979323846f/180;
    return std::clamp(pitch,-downDegrees*radians,upDegrees*radians);
}
constexpr float pi=3.14159265358979323846f;
inline float LookDownBlend(float pitch) {
    float t=std::clamp((-pitch-25*pi/180)/(50*pi/180),0.0f,1.0f);
    return t*t*(3-2*t);
}
inline float VerticalFov(float horizontalDegrees,float heightOverWidth) {
    return 2*std::atan(std::tan(horizontalDegrees*pi/360)*heightOverWidth);
}
inline void Basis(float yaw,float pitch,float* m) {
    const float sy=std::sin(yaw),cy=std::cos(yaw),sp=std::sin(pitch),cp=std::cos(pitch);
    const float basis[16]={cy,0,-sy,0,-sy*sp,cp,-cy*sp,0,sy*cp,sp,cy*cp,0,0,0,0,1};
    std::copy(basis,basis+16,m);
}
// Keep the viewing direction while rebuilding screen up against world up.
inline void RemoveRoll(float* m) {
    float horizontal=std::hypot(m[8],m[10]);
    float yaw=horizontal>0.0001f ? std::atan2(m[8],m[10]) : std::atan2(-m[2],m[0]);
    float pitch=std::atan2(m[9],horizontal);
    float upright[16]; Basis(yaw,pitch,upright);
    std::copy(upright,upright+12,m);
}
struct StableEyeHeight {
    float value=0;
    bool valid=false;
    void Reset() { valid=false; }
    float Update(float target,float dt,float snapDistance) {
        if(!valid || std::fabs(target-value)>snapDistance) { value=target; valid=true; }
        else value+=(target-value)*(1-std::exp(-18*std::max(dt,0.0f)));
        return value;
    }
};
struct SmoothedAttachment {
    StableEyeHeight axes[3];
    void Reset() { for(auto& axis:axes) axis.Reset(); }
    std::array<float,3> Update(const std::array<float,3>& target,float dt,float snapDistance) {
        return {axes[0].Update(target[0],dt,snapDistance),axes[1].Update(target[1],dt,snapDistance),axes[2].Update(target[2],dt,snapDistance)};
    }
};
inline std::array<float,16> CameraShot(const float* camera) {
    std::array<float,16> shot;
    std::copy(camera,camera+16,shot.begin());
    shot[12]=shot[13]=shot[14]=0; shot[15]=1;
    return shot;
}
inline bool PlausibleHeadAttachment(int joint,int count,unsigned char parent,const std::array<float,3>& position,float height) {
    return joint>=0 && joint<count && parent<count && parent!=joint &&
        std::isfinite(position[0]) && std::isfinite(position[1]) && std::isfinite(position[2]) &&
        position[1]>=height*0.55f && position[1]<=height*1.5f &&
        std::hypot(position[0],position[2])<height*0.65f;
}
inline bool Descendant(int joint,int root,const unsigned char* parents,int count) {
    for(int tries=0;joint>=0 && joint<count && tries<count;++tries) {
        if(joint==root) return true;
        int next=parents[joint]; if(next==joint || next==255) return false; joint=next;
    }
    return false;
}
inline int InferHeadJoint(const std::array<float,3>* positions,const unsigned char* parents,int count,float height) {
    if(count<6 || count>256 || !std::isfinite(height) || height<=0) return -1;
    bool leftFoot=false,rightFoot=false;
    for(int j=0;j<count;++j) if(positions[j][1]<height*0.35f) {
        leftFoot|=positions[j][0]<-height*0.04f;
        rightFoot|=positions[j][0]>height*0.04f;
    }
    if(!leftFoot || !rightFoot) return -1;
    int best=-1; float bestScore=1000;
    for(int j=0;j<count;++j) {
        const auto& p=positions[j];
        if(!PlausibleHeadAttachment(j,count,parents[j],p,height) || p[1]<height*0.65f ||
            p[1]>height*1.15f || std::hypot(p[0],p[2])>height*0.25f) continue;
        int descendants=0;
        for(int k=0;k<count;++k) if(Descendant(k,j,parents,count)) ++descendants;
        if(descendants>std::max(3,count/3)) continue;
        float score=std::fabs(p[1]/height-0.8f)+2*std::hypot(p[0],p[2])/height;
        if(score<bestScore) { best=j; bestScore=score; }
    }
    return best;
}
inline bool InverseAffine(const float* m,std::array<float,16>& inverse) {
    float determinant=BasisDeterminant(m);
    if(!std::isfinite(determinant)||std::fabs(determinant)<0.000001f) return false;
    float d=1/determinant;
    inverse={(m[5]*m[10]-m[6]*m[9])*d,(m[2]*m[9]-m[1]*m[10])*d,(m[1]*m[6]-m[2]*m[5])*d,0,
        (m[6]*m[8]-m[4]*m[10])*d,(m[0]*m[10]-m[2]*m[8])*d,(m[2]*m[4]-m[0]*m[6])*d,0,
        (m[4]*m[9]-m[5]*m[8])*d,(m[1]*m[8]-m[0]*m[9])*d,(m[0]*m[5]-m[1]*m[4])*d,0,0,0,0,1};
    for(int column=0;column<3;++column) inverse[12+column]=-(m[12]*inverse[column]+m[13]*inverse[4+column]+m[14]*inverse[8+column]);
    return true;
}
inline bool SuppressJointAnimation(float* output,const float* input,int count,int joint,const unsigned char* parents,const float* restLocal) {
    std::copy(input,input+count*16,output);
    if(joint<0||joint>=count||parents[joint]==joint) return false;
    std::array<float,16> newHead;
    if(parents[joint]==255) std::copy(restLocal,restLocal+16,newHead.begin());
    else if(parents[joint]<count) newHead=Multiply(restLocal,input+parents[joint]*16);
    else return false;
    std::array<float,16> oldHeadInverse;
    if(!InverseAffine(input+joint*16,oldHeadInverse)) return false;
    auto correction=Multiply(oldHeadInverse.data(),newHead.data());
    for(int j=0;j<count;++j) if(Descendant(j,joint,parents,count)) {
        auto replacement=Multiply(input+j*16,correction.data());
        std::copy(replacement.begin(),replacement.end(),output+j*16);
    }
    return true;
}
}
