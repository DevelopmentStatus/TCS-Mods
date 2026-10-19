#include "camera_math.h"
#include "pc_controls.h"
#include "mouse_motion.h"
#include <cstdio>
#include <cstdlib>
static void require(bool v,const char* message) { if(!v) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); } }
int main() {
    std::array<float,3> biped[]={{0,.18f,0},{0,.25f,0},{0,.34f,0},{-.04f,.05f,0},{.04f,.05f,0},{0,.37f,0}};
    const unsigned char bipedParents[]={255,0,1,0,0,2};
    require(fps::InferHeadJoint(biped,bipedParents,6,.42f)==2,"missing head locator falls back to upper central head joint");
    auto oldLeft=biped[3]; biped[3][0]=0;
    require(fps::InferHeadJoint(biped,bipedParents,6,.42f)==-1,"non-biped root/body locators are not guessed as a head");
    biped[3]=oldLeft; biped[2][0]=.4f; biped[5][0]=.4f;
    require(fps::InferHeadJoint(biped,bipedParents,6,.42f)==-1,"side limbs are not selected as heads");
    require(fps::PlausibleHeadAttachment(4,31,3,{0,0.334f,-0.05f},0.42f),"Emperor-style undeclared head attachment is accepted");
    require(fps::PlausibleHeadAttachment(5,14,4,{0,0.37f,-0.06f},0.42f),"battle-droid undeclared head attachment is accepted");
    require(!fps::PlausibleHeadAttachment(0,8,255,{0,0.35f,0},0.42f),"generic root locator cannot hide the entire body");
    require(!fps::PlausibleHeadAttachment(2,8,1,{0,0.15f,0},0.42f),"low body locator is rejected");
    require(!fps::PlausibleHeadAttachment(2,8,1,{0.4f,0.35f,0},0.42f),"outlying non-head locator is rejected");
    for(float heading:{-2.0f,0.0f,2.0f}) for(float elevation:{-1.2f,0.0f,1.2f}) {
        float camera[16]; fps::Basis(heading,elevation,camera);
        camera[12]=5; camera[13]=2; camera[14]=8;
        auto shot=fps::CameraShot(camera);
        for(int i=0;i<12;++i) require(shot[i]==camera[i],"projectile orientation matches camera including vertical aim");
        require(shot[12]==0 && shot[13]==0 && shot[14]==0 && shot[15]==1,"projectile position is supplied separately from orientation");
    }
    fps::SmoothedAttachment attachmentSmoothing;
    auto initial=attachmentSmoothing.Update({0,0.4f,0},0.016f,1);
    require(initial[1]==0.4f,"animated attachment seeds without a transition jump");
    auto following=attachmentSmoothing.Update({0.1f,0.5f,-0.1f},0.016f,1);
    require(following[0]>0 && following[0]<0.1f && following[1]>0.4f && following[1]<0.5f && following[2]<0 && following[2]>-0.1f,"all skeleton offset axes follow animation with smoothing");
    attachmentSmoothing.Reset();
    require(attachmentSmoothing.Update({0,0.2f,0},0.016f,1)[1]==0.2f,"character changes discard previous attachment smoothing");
    fps::StableEyeHeight eye;
    require(eye.Update(10,0.016f,1)==10,"initial anchor snaps into place");
    float eased=eye.Update(10.1f,0.016f,1);
    require(eased>10 && eased<10.1f,"small vertical changes are gently smoothed");
    require(eye.Update(10.1f,0,1)==eased,"zero elapsed time does not advance smoothing");
    require(eye.Update(20,0.016f,1)==20,"teleports snap instead of dragging camera across level");
    eye.Reset(); require(eye.Update(5,0.016f,1)==5,"mode and character resets discard old eye height");
    fps::StableEyeHeight full,halves;
    full.Update(0,0,2); halves.Update(0,0,2);
    float one=full.Update(1,0.032f,2);
    halves.Update(1,0.016f,2);
    require(std::fabs(one-halves.Update(1,0.016f,2))<0.00001f,"smoothing remains consistent across frame rates");
    for(float angle:{0.0f,0.7f,1.57f,3.14f}) {
        float base[16]; fps::Basis(0.6f,angle,base);
        float forward[3]={base[8],base[9],base[10]};
        base[12]=42;
        fps::RemoveRoll(base);
        require(std::fabs(base[1])<0.0001f && base[5]>=0,"animated camera has no screen roll or inverted up");
        for(int i=0;i<3;++i) require(std::fabs(base[8+i]-forward[i])<0.0001f,"removing roll preserves viewing direction through flips");
        require(base[12]==42 && fps::BasisDeterminant(base)>0.99f,"roll correction preserves position and orthonormal orientation");
    }
    std::array<float,48> animatedSkeleton{},stabilizedSkeleton{};
    float restHead[16],animatedHead[16],childLocal[16];
    fps::Basis(0,fps::pi,animatedSkeleton.data());
    fps::Basis(0,0,restHead); restHead[13]=0.3f;
    fps::Basis(0.7f,0.4f,animatedHead); animatedHead[13]=0.38f;
    auto headGlobal=fps::Multiply(animatedHead,animatedSkeleton.data());
    std::copy(headGlobal.begin(),headGlobal.end(),animatedSkeleton.begin()+16);
    fps::Basis(0.2f,0,childLocal); childLocal[12]=0.1f;
    auto childGlobal=fps::Multiply(childLocal,headGlobal.data());
    std::copy(childGlobal.begin(),childGlobal.end(),animatedSkeleton.begin()+32);
    auto originalSkeleton=animatedSkeleton;
    const unsigned char headParents[]={255,0,1};
    require(fps::SuppressJointAnimation(stabilizedSkeleton.data(),animatedSkeleton.data(),3,1,headParents,restHead),"head animation can be suppressed on a temporary skeleton");
    auto expectedHead=fps::Multiply(restHead,animatedSkeleton.data());
    auto expectedChild=fps::Multiply(childLocal,expectedHead.data());
    for(int i=0;i<16;++i) {
        require(std::fabs(stabilizedSkeleton[16+i]-expectedHead[i])<0.0001f,"head-local nod, turn and bob are removed");
        require(std::fabs(stabilizedSkeleton[32+i]-expectedChild[i])<0.0001f,"attachments follow corrected head without losing their own animation");
        require(stabilizedSkeleton[i]==animatedSkeleton[i],"body-parent animation is unaffected");
    }
    require(stabilizedSkeleton[21]<-0.99f,"body flip remains inherited by stabilized head");
    require(animatedSkeleton==originalSkeleton,"original animated skeleton is unchanged and remains available outside the mode");
    auto mode=fps::CameraMode::FirstPerson;
    mode=fps::NextCameraMode(mode); require(mode==fps::CameraMode::ThirdPerson,"native third person follows first person");
    mode=fps::NextCameraMode(mode); require(mode==fps::CameraMode::Orbit,"orbit follows third person");
    require(fps::NextCameraMode(mode)==fps::CameraMode::FirstPerson,"three-mode cycle wraps correctly");
    require(fps::UsesCameraAim(fps::CameraMode::FirstPerson,false),"aim-bot off uses camera-directed shots");
    require(!fps::UsesCameraAim(fps::CameraMode::FirstPerson,true),"aim-bot on restores native targeting");
    require(!fps::UsesCameraAim(fps::CameraMode::ThirdPerson,false) && !fps::UsesCameraAim(fps::CameraMode::Orbit,false),"third person and orbit retain native targeting");
    float bone[16],world[16]; fps::Basis(0,0,bone); bone[13]=0.3f;
    fps::Basis(fps::pi/2,0,world); world[12]=10; world[13]=2; world[14]=30;
    auto attachment=fps::TransformPoint(bone,{0.01f,0.06f,0.02f});
    auto anchor=fps::TransformPoint(world,attachment);
    require(std::fabs(anchor[0]-10.02f)<0.0001f && std::fabs(anchor[1]-2.36f)<0.0001f && std::fabs(anchor[2]-29.99f)<0.0001f,"head attachment composes bone animation and character transform");
    bone[13]=0.35f;
    auto movedAnchor=fps::TransformPoint(world,fps::TransformPoint(bone,{0.01f,0.06f,0.02f}));
    require(std::fabs(movedAnchor[1]-anchor[1]-0.05f)<0.0001f,"head animation moves the camera anchor");
    auto turned=fps::RotateOffset({0.0f,0.36f,0.1f},fps::pi/2);
    require(std::fabs(turned[0]-0.1f)<0.0001f && std::fabs(turned[1]-0.36f)<0.0001f,"cached head offset follows current character turn");
    require(fps::BasisDeterminant(world)>0.99f,"normal character pose can supply attachment samples");
    world[5]=-1;
    require(fps::BasisDeterminant(world)<0,"mirrored reflection pose cannot replace the real head sample");
    float flippedHead[16]; fps::Basis(0,fps::pi,flippedHead);
    float localLook[16]; fps::Basis(0,fps::LimitLook(0.5f,80,85),localLook);
    auto flippedView=fps::Multiply(localLook,flippedHead);
    require(flippedView[5]<-0.8f,"head flips invert screen up while mouse look remains usable");
    require(std::fabs(fps::LimitLook(100,80,85)-80*fps::pi/180)<0.0001f,"upward mouse look is limited relative to the head");
    require(std::fabs(fps::LimitLook(-100,80,85)+85*fps::pi/180)<0.0001f,"downward mouse look is limited relative to the head");
    float bind[16],inverseBind[16]; fps::Basis(fps::pi/2,0,bind); fps::Basis(-fps::pi/2,0,inverseBind);
    auto neutralHead=fps::Multiply(inverseBind,bind);
    require(std::fabs(neutralHead[0]-1)<0.0001f && std::fabs(neutralHead[10]-1)<0.0001f,"bind-pose attachment axes do not turn the camera sideways");
    std::array<float,16> cleanRotation;
    auto scaledFlip=flippedView;
    for(int i=0;i<12;++i) scaledFlip[i]*=0.42f;
    require(fps::RotationOnly(scaledFlip.data(),cleanRotation) && cleanRotation[5]<-0.8f && fps::BasisDeterminant(cleanRotation.data())>0.99f,"character scale is removed without removing animation flips");
    float characterFacing[16],cameraFacing[16];
    fps::Basis(fps::pi,0,characterFacing); fps::Basis(fps::pi,0,cameraFacing);
    auto correctedFacing=fps::Multiply(cameraFacing,characterFacing);
    require(correctedFacing[10]>0.99f && correctedFacing[5]>0.99f,"backward render reference becomes forward-facing camera without changing up");
    auto correctedFlip=fps::Multiply(cameraFacing,flippedHead);
    auto flippedCharacter=fps::Multiply(correctedFlip.data(),characterFacing);
    require(flippedCharacter[5]<-0.99f,"facing correction preserves animation flips");
    require(fps::Secondary(0,0x65,true)==fps::SecondaryAction::Block,"lightsaber right mouse blocks");
    require(fps::Secondary(0x80,0x69,true)==fps::SecondaryAction::Block,"lightsaber takes priority over ranged classification");
    require(fps::Secondary(0x80,22,true)==fps::SecondaryAction::Zoom,"blaster right mouse retains zoom");
    require(fps::Secondary(0,22,true)==fps::SecondaryAction::Block,"melee character with guard animations blocks");
    require(fps::Secondary(0,-1,false)==fps::SecondaryAction::None,"noncombat droids do not get a fake guard or zoom");
    fps::MouseMotion mouse;
    auto delta=mouse.Sample(737780502,1385890273,true);
    require(delta.x==0 && delta.y==0,"first accumulated mouse sample only establishes an origin");
    for(int i=0;i<500;++i) {
        delta=mouse.Sample(737780502,1385890273,true);
        require(delta.x==0 && delta.y==0,"stationary accumulated coordinates never spin or pitch camera");
    }
    delta=mouse.Sample(737780507,1385890270,true);
    require(delta.x==5 && delta.y==-3,"camera uses movement differences instead of coordinate values");
    mouse.Reset(); mouse.Sample(65535,32767,true); delta=mouse.Sample(0,32768,true);
    require(delta.x==1 && delta.y==1,"16-bit coordinates wrap without jumps");
    delta=mouse.Sample(65535,32767,true);
    require(delta.x==-1 && delta.y==-1,"reverse wrap preserves direction");
    mouse.Sample(12,34,false); delta=mouse.Sample(28000,42000,true);
    require(delta.x==0 && delta.y==0,"resuming capture discards stale coordinates");
    delta=mouse.Sample(35000,42000,true);
    require(delta.x==0 && delta.y==0 && mouse.rejectedSamples==1,"device-origin jumps are discarded");
    mouse.Reset(); mouse.Sample(0x12340010,0x34560020,true); delta=mouse.Sample(0x22220010,0x55550020,true);
    require(delta.x==0 && delta.y==0,"unused high coordinate bits do not create movement");
    auto move=fps::PcMovement(true,false,false,true,false);
    require(std::fabs(move.x*move.x+move.z*move.z-1)<0.0001f,"diagonal movement does not run faster");
    require(move.angle>8000 && move.angle<8400,"forward-right moves 45 degrees relative to camera");
    move=fps::PcMovement(true,true,true,true,false);
    require(move.magnitude==0,"opposite movement keys cancel");
    move=fps::PcMovement(false,false,false,false,false);
    require(move.magnitude==0,"releasing all movement keys stops keyboard movement");
    move=fps::PcMovement(true,false,false,false,true);
    require(move.magnitude==0.45f,"shift produces walking input");
    auto edges=fps::Buttons(128,0);
    require(edges.held==128 && edges.pressed==128 && edges.released==0,"attack press starts once");
    edges=fps::Buttons(128,128);
    require(edges.pressed==0 && edges.released==0,"held attack does not repeat press edges");
    edges=fps::Buttons(0,128);
    require(edges.released==128 && edges.held==0,"attack releases cleanly");
    require(fps::LookDownBlend(0)==0,"level view has no body offset");
    require(fps::LookDownBlend(-80*fps::pi/180)==1,"steep downward view has full offset");
    require(fps::LookDownBlend(-50*fps::pi/180)>0.4f && fps::LookDownBlend(-50*fps::pi/180)<0.6f,"look-down offset eases in");
    for(int y=-180;y<=180;y+=15) for(int p=-80;p<=80;p+=10) {
        float m[16]; fps::Basis(y*fps::pi/180,p*fps::pi/180,m);
        for(int a=0;a<3;a++) for(int b=0;b<3;b++) {
            float dot=0; for(int j=0;j<3;j++) dot+=m[a*4+j]*m[b*4+j];
            require(std::fabs(dot-(a==b?1.0f:0.0f))<0.0001f,"camera basis stays orthonormal");
        }
    }
    float vertical=fps::VerticalFov(90,9.0f/16);
    require(vertical>1.0f && vertical<1.1f,"90-degree horizontal FOV converts correctly at 16:9");
    // At steep downward look, both the torso and feet remain in the camera frustum.
    float m[16]; fps::Basis(0,-80*fps::pi/180,m);
    for(float bodyHeight:{0.05f,0.23f}) {
        float dy=bodyHeight-0.38f,dz=-0.04f;
        float depth=dy*m[9]+dz*m[10],up=dy*m[5]+dz*m[6];
        require(depth>0.01f && std::fabs(up/depth)<std::tan(vertical/2),"body remains visible looking down");
    }
    const unsigned char parents[]={255,0,1,1,3};
    require(fps::Descendant(4,3,parents,5),"head attachment descendant is hidden");
    require(!fps::Descendant(2,3,parents,5),"torso sibling remains visible");
    const unsigned char cycle[]={1,0};
    require(!fps::Descendant(0,7,cycle,2),"invalid skeleton does not loop forever");
    puts("Camera math and body visibility checks passed.");
}
