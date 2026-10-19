#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <share.h>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <intrin.h>
#include <array>
#include <cstdlib>
#include "camera_math.h"
#include "pc_controls.h"
#include "mouse_motion.h"
#include "mod_api.h"

static HWND gameWindow;
static FILE* logfile;
static const TCSModHost* modHost{};
static TCSModHandle modHandle{};
static wchar_t modConfig[MAX_PATH]{};

struct Vec { float x,y,z; };
struct Camera { float m[16]; float fov,aspect,nearClip,farClip; float extra[10]; };
static_assert(sizeof(Camera)==0x78);
static bool enabled=true, mouseLook=true, active=false, seeded=false, firstPerson=false;
static float yaw=0,pitch=0,fovDegrees=90,sensitivity=0.0025f,orbitDistance=1.2f,bodyBlend=0;
static float lookUpDegrees=80,lookDownDegrees=85;
static bool hideHead=true,modernControls=true;
static bool animatedFirstPerson=false,aimBot=false;
static bool gameplayInput=false;
static DWORD lastGameplayFrame=0;
static float aimBlend=0;
enum Bind { Forward,Back,Left,Right,Attack,Jump,Ability,Interact,Tag,Previous,Next,Walk,CameraCycle,Focus,BindCount };
static int keys[BindCount]={'W','S','A','D',VK_LBUTTON,VK_SPACE,'E','F','Q',VK_TAB,'R',VK_SHIFT,'C',VK_RBUTTON};
static const wchar_t* bindingNames[BindCount]={L"Forward",L"Back",L"Left",L"Right",L"Attack",L"Jump",L"Ability",L"Interact",L"SwitchCharacter",L"PreviousCharacter",L"NextCharacter",L"Walk",L"CycleCamera",L"FocusOrOrbit"};
static int queuedCharacterWheel=0;
using CameraMode=fps::CameraMode;
static CameraMode selectedMode=CameraMode::FirstPerson;
static bool AnimatedFirstPerson() { return selectedMode==CameraMode::FirstPerson && animatedFirstPerson; }
static DWORD lastCameraTime=0;
static LONG mouseX=0,mouseY=0;
static unsigned int mousePolls=0,mouseMotionSamples=0;
static fps::MouseMotion mouseMotion;
static WNDPROC oldWndProc;
static bool cameraHookInstalled=false;
static uint8_t* lastPlayer;
static DWORD lastLog=0;
static DWORD lastWorldFrame=0;
static uint32_t previousMouseButtons=0;
struct CharacterInfo {
    uint8_t* data{}; uint8_t* model{}; uint8_t* hierarchy{};
    int headJoint=-1,jointCount=0;
    std::array<float,3> headAttachment{};
    float height=0.42f;
    fps::SecondaryAction secondary=fps::SecondaryAction::None;
};
static CharacterInfo currentCharacter;
struct HeadPose {
    uint8_t* player{}; uint8_t* model{};
    std::array<float,3> offset{};
    std::array<float,16> rotation{};
    float bodyYaw=0;
    DWORD sampledAt=0;
    bool valid=false;
    bool rotationValid=false;
};
static HeadPose headPose;
static unsigned int headPoseSamples=0;
static unsigned int suppressedHeadSamples=0;
static bool animatedHeadApplied=false;
static fps::StableEyeHeight stableEyeHeight;
static fps::SmoothedAttachment smoothedAttachment;
static std::array<float,16> aimCamera{};
static uint8_t* aimPlayer=nullptr;
static DWORD aimSampleTime=0;
static unsigned int cameraAimedShots=0;
static uint8_t* guardedPlayer=nullptr;
static bool nativeGuardAvailable=false;
static unsigned int guardFrames=0;
using CameraSetFn=void(__cdecl*)(Camera*,int);
static CameraSetFn originalCameraSet;
static void Log(const char* text) { if(logfile) { fprintf(logfile,"%s\n",text); fflush(logfile); } }

static bool Readable(const void* p,size_t n) {
    MEMORY_BASIC_INFORMATION mbi{};
    if(!p || !VirtualQuery(p,&mbi,sizeof(mbi))) return false;
    return mbi.State==MEM_COMMIT && !(mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        reinterpret_cast<uintptr_t>(p)+n<=reinterpret_cast<uintptr_t>(mbi.BaseAddress)+mbi.RegionSize;
}

static CharacterInfo InspectCharacter(uint8_t* player) {
    CharacterInfo info;
    if(!Readable(player,0x10d8)) return info;
    info.model=*reinterpret_cast<uint8_t**>(player+0x50);
    info.data=*reinterpret_cast<uint8_t**>(player+0x54);
    if(!Readable(info.model,0x54)||!Readable(info.data,0x4c)) return info;
    float height=*reinterpret_cast<float*>(info.data+0x38);
    if(std::isfinite(height) && height>0.08f && height<4) info.height=height;
    info.hierarchy=*reinterpret_cast<uint8_t**>(info.model+4);
    auto details=*reinterpret_cast<uint8_t**>(info.data+0x24);
    bool hasBlock=false;
    auto animationData=*reinterpret_cast<uint8_t**>(info.model+0xc);
    auto system=*reinterpret_cast<uint8_t**>(0x29130e4);
    auto actionInfo=*reinterpret_cast<uint8_t**>(0x7fbec8);
    if(Readable(system,12)) {
        int count=*reinterpret_cast<int*>(system+8);
        if(count>0 && count<=512 && Readable(actionInfo,count*8) && Readable(animationData,count*4)) {
            for(int action=0;action<count;++action) if((actionInfo[action*8+4]&8)!=0 && *reinterpret_cast<uintptr_t*>(animationData+4*action)!=0) { hasBlock=true; break; }
        }
    }
    int weapon=Readable(details,0xee)?*reinterpret_cast<int16_t*>(details+0xec):-1;
    info.secondary=fps::Secondary(*reinterpret_cast<uint32_t*>(info.data+4),weapon,hasBlock);
    if(!Readable(info.hierarchy,0x1c4)||!Readable(details,0x120)) return info;
    info.jointCount=*reinterpret_cast<int*>(info.hierarchy+0x168);
    if(info.jointCount<1 || info.jointCount>256) return info;
    auto directory=*reinterpret_cast<const char**>(info.data+8);
    if(Readable(directory,4) && _strnicmp(directory,"gonk",4)==0) return info;
    auto skeleton=*reinterpret_cast<uint8_t**>(info.hierarchy+0x16c);
    auto inverseBind=*reinterpret_cast<float**>(info.hierarchy+0x174);
    if(!Readable(skeleton,info.jointCount*0x60) || !Readable(inverseBind,info.jointCount*64)) return info;
    int headLocator=static_cast<int8_t>(details[0x10e]);
    if(headLocator>=0 && headLocator<16) {
        auto attachment=*reinterpret_cast<uint8_t**>(info.model+0x14+4*headLocator);
        if(Readable(attachment,0x50)) {
            int joint=attachment[0x44];
            std::array<float,16> bind;
            if(joint<info.jointCount && fps::InverseAffine(inverseBind+16*joint,bind)) {
                std::array<float,3> locator;
                std::copy(reinterpret_cast<float*>(attachment+0x30),reinterpret_cast<float*>(attachment+0x30)+3,locator.begin());
                auto restPosition=fps::TransformPoint(bind.data(),locator);
                if(details[0x118]!=255 || fps::PlausibleHeadAttachment(joint,info.jointCount,skeleton[joint*0x60+0x50],restPosition,info.height)) {
                    info.headJoint=joint; info.headAttachment=locator; return info;
                }
            }
        }
    }
    // Missing locators: first try a declared joint, then conservative biped anatomy.
    std::array<std::array<float,3>,256> positions{};
    std::array<unsigned char,256> parents{};
    for(int j=0;j<info.jointCount;++j) {
        std::array<float,16> bind;
        if(!fps::InverseAffine(inverseBind+16*j,bind)) return info;
        positions[j]={bind[12],bind[13],bind[14]}; parents[j]=skeleton[j*0x60+0x50];
    }
    int joint=details[0x118];
    if(joint==255 || joint>=info.jointCount || !fps::PlausibleHeadAttachment(joint,info.jointCount,parents[joint],positions[joint],info.height))
        joint=fps::InferHeadJoint(positions.data(),parents.data(),info.jointCount,info.height);
    if(joint>=0) {
        info.headJoint=joint;
        auto eye=positions[joint]; eye[1]+=info.height*0.08f;
        info.headAttachment=fps::TransformPoint(inverseBind+16*joint,eye);
    }

    return info;
}

using MoveFn=void(__cdecl*)(uint8_t*);
struct MoveEntry { uint8_t* data{}; MoveFn original{}; };
static std::array<MoveEntry,128> moveEntries{};
static bool Down(int key) { return (GetAsyncKeyState(key)&0x8000)!=0; }
static bool Held(Bind bind) { return keys[bind]!=0 && Down(keys[bind]); }
static void ReleaseGuard(uint8_t* object) {
    if(object!=guardedPlayer || !Readable(object,0x10d8)) return;
    int holdContext=*reinterpret_cast<int*>(0x802820);
    if(static_cast<int8_t>(object[0x7b5])==holdContext) {
        object[0x7b5]=255;
        *reinterpret_cast<float*>(object+0x77c)=0;
    }
    guardedPlayer=nullptr;
}
static void HoldGuard(uint8_t* object) {
    if(!nativeGuardAvailable) return;
    int holdContext=*reinterpret_cast<int*>(0x802820);
    if(holdContext<0 || holdContext>127) return;
    int context=static_cast<int8_t>(object[0x7b5]);
    if(context==holdContext) {
        *reinterpret_cast<float*>(object+0x77c)=0.3f;
        guardedPlayer=object; ++guardFrames;
    } else if(context==-1 && object[0x281]!=0) {
        // Select a native blocking animation and enter the native held-guard context.
        // Leave jumping, hits, climbing, Force actions and interactions to the game.
        if(reinterpret_cast<int(__cdecl*)(uint8_t*)>(0x58dfa0)(object)!=0) {
            object[0x7b5]=static_cast<uint8_t>(holdContext);
            *reinterpret_cast<float*>(object+0x77c)=0.3f;
            guardedPlayer=object; ++guardFrames;
        }
    }
}
using ReadKeyboardFn=uint8_t*(__thiscall*)(void*);
static ReadKeyboardFn originalReadKeyboard=reinterpret_cast<ReadKeyboardFn>(0x6d5990);
static uint8_t* __fastcall HookReadKeyboard(void* self,void*) {
    auto state=originalReadKeyboard(self);
    // Native DirectInput scan code 0x25 is K. Translate before native pad
    // mapping and character-switch processing, rather than inside movement.
    if(Readable(state,256) && modernControls && gameplayInput &&
       GetTickCount()-lastGameplayFrame<250 && GetForegroundWindow()==gameWindow) {
        bool switching=Held(Tag);
        if(switching) state[0x25]|=0x80;
        static bool wasSwitching=false;
        if(switching && !wasSwitching) Log("SwitchCharacter mapped to native K keyboard state");
        wasSwitching=switching;
    }
    return state;
}
struct NativeMouseState { LONG x,y,wheel; BYTE buttons[8]; };
static_assert(sizeof(NativeMouseState)==20);
using ReadMouseFn=NativeMouseState*(__thiscall*)(void*);
static ReadMouseFn originalReadMouse=reinterpret_cast<ReadMouseFn>(0x6d57d0);
static NativeMouseState* __fastcall HookReadMouse(void* self,void*) {
    // Preserve the game's thiscall receiver in ECX and return its state unchanged.
    NativeMouseState* state=originalReadMouse(self);
    ++mousePolls;
    bool capture=active && mouseLook && GetTickCount()-lastWorldFrame<250 && GetForegroundWindow()==gameWindow;
    if(state) {
        auto delta=mouseMotion.Sample(state->x,state->y,capture);
        InterlockedExchangeAdd(&mouseX,delta.x);
        InterlockedExchangeAdd(&mouseY,delta.y);
        if(delta.x || delta.y) ++mouseMotionSamples;
    } else mouseMotion.Reset();
    return state;
}
static void __cdecl HookMove(uint8_t* object) {
    auto data=Readable(object,0x58)?*reinterpret_cast<uint8_t**>(object+0x54):nullptr;
    MoveFn original=nullptr;
    for(auto& entry:moveEntries) if(entry.data==data) { original=entry.original; break; }
    if(!original) return;
    bool control=modernControls && gameplayInput && GetTickCount()-lastGameplayFrame<250 &&
        GetForegroundWindow()==gameWindow && object==*reinterpret_cast<uint8_t**>(0x93d810);
    bool blocking=control && Held(Focus) && currentCharacter.secondary==fps::SecondaryAction::Block;
    if(!blocking) ReleaseGuard(object);
    if(control) {
        auto camera=*reinterpret_cast<uint8_t**>(0x8021b4);
        bool customCamera=active && enabled && selectedMode!=CameraMode::ThirdPerson;
        if(customCamera && Readable(camera,0x230)) *reinterpret_cast<int32_t*>(camera+0x1fc)=static_cast<int32_t>(yaw*65536/(2*fps::pi))&0xffff;
        auto pad=*reinterpret_cast<uint8_t**>(object+0xca4);
        if(Readable(pad,0x60)) {
            bool pcInput=pad[0x25]==1 || Held(Forward)||Held(Back)||Held(Left)||Held(Right)||Held(Attack)||Held(Jump)||Held(Ability)||Held(Interact)||Held(Tag)||Held(Previous)||Held(Next)||Held(Focus)||queuedCharacterWheel!=0;
            if(pcInput) {
                auto move=fps::PcMovement(Held(Forward),Held(Back),Held(Left),Held(Right),Held(Walk));
                *reinterpret_cast<uint16_t*>(pad+0x26)=move.angle;
                *reinterpret_cast<float*>(pad+0x28)=move.magnitude;
                *reinterpret_cast<float*>(pad+0x2c)=move.z*move.magnitude;
                *reinterpret_cast<float*>(pad+0x30)=move.x*move.magnitude;
                *reinterpret_cast<float*>(pad+0x34)=move.magnitude;
                uint32_t jump=*reinterpret_cast<uint32_t*>(0x802278),attack=*reinterpret_cast<uint32_t*>(0x80227c);
                uint32_t ability=*reinterpret_cast<uint32_t*>(0x802280),tag=*reinterpret_cast<uint32_t*>(0x802284);
                uint32_t previous=*reinterpret_cast<uint32_t*>(0x802288),next=*reinterpret_cast<uint32_t*>(0x80228c);
                uint32_t buttons=0;
                if(Held(Attack)||blocking) buttons|=attack;
                if(Held(Jump)) buttons|=jump;
                if(Held(Ability)||Held(Interact)) buttons|=ability;
                // Character switching is translated earlier by the native keyboard
                // path, just like physical K; preserve its held/edge state below.
                buttons|=*reinterpret_cast<uint32_t*>(pad+4)&tag;
                if(Held(Previous)||queuedCharacterWheel>0) buttons|=previous;
                if(Held(Next)||queuedCharacterWheel<0) buttons|=next;
                queuedCharacterWheel=0;
                auto edges=fps::Buttons(buttons,previousMouseButtons);
                if(blocking) edges.pressed&=~attack;
                uint32_t mask=jump|attack|ability|previous|next;
                *reinterpret_cast<uint32_t*>(pad+4)=(*reinterpret_cast<uint32_t*>(pad+4)&~mask)|edges.held;
                *reinterpret_cast<uint32_t*>(pad+8)=(*reinterpret_cast<uint32_t*>(pad+8)&~mask)|edges.pressed;
                *reinterpret_cast<uint32_t*>(pad+12)=(*reinterpret_cast<uint32_t*>(pad+12)&~mask)|edges.released;
                previousMouseButtons=buttons;
            } else previousMouseButtons=0;
        }
        if(customCamera && firstPerson) {
            uint16_t angle=static_cast<uint16_t>(static_cast<int>(yaw*65536/(2*fps::pi))&0xffff);
            *reinterpret_cast<uint16_t*>(object+0x58)=angle;
            *reinterpret_cast<uint16_t*>(object+0x5a)=angle;
        }
    }
    if(blocking && !Held(Jump) && !Held(Ability) && !Held(Interact)) HoldGuard(object);
    original(object);
    if(blocking && !Held(Jump) && !Held(Ability) && !Held(Interact)) HoldGuard(object);
    if(control && active && enabled && selectedMode!=CameraMode::ThirdPerson && firstPerson) {
        uint16_t angle=static_cast<uint16_t>(static_cast<int>(yaw*65536/(2*fps::pi))&0xffff);
        *reinterpret_cast<uint16_t*>(object+0x58)=angle;
        *reinterpret_cast<uint16_t*>(object+0x5a)=angle;
    }
}
static void RegisterMovement(const CharacterInfo& info) {
    if(!modernControls || !Readable(info.data,0x4c)) return;
    auto slot=reinterpret_cast<MoveFn*>(info.data+0x18);
    if(*slot==HookMove || !*slot) return;
    for(auto& entry:moveEntries) if(entry.data==info.data || !entry.data) {
        entry={info.data,*slot}; *slot=HookMove; return;
    }
}

using RenderFn=int(__cdecl*)(uint8_t*,float*,int,int16_t*,float*,void**,int);
static RenderFn originalRender=reinterpret_cast<RenderFn>(0x5f4ff0);
static unsigned int hiddenHeadDraws=0;
static int __cdecl HookRender(uint8_t* hierarchy,float* world,int count,int16_t* layers,float* joints,void** blend,int flags) {
    const auto& info=currentCharacter;
    auto player=*reinterpret_cast<uint8_t**>(0x93d810);
    bool ownDraw=active && firstPerson && info.headJoint>=0 && GetTickCount()-lastWorldFrame<250 &&
        hierarchy==info.hierarchy && Readable(player,0x10d8) && Readable(world,64) && Readable(joints,64*info.jointCount);
    if(!ownDraw) return originalRender(hierarchy,world,count,layers,joints,blend,flags);
    const auto& position=*reinterpret_cast<Vec*>(player+0x5c);
    // Match this instance; NPCs can share the same character model.
    if(std::fabs(world[12]-position.x)>0.025f || std::fabs(world[13]-position.y)>0.025f || std::fabs(world[14]-position.z)>0.025f)
        return originalRender(hierarchy,world,count,layers,joints,blend,flags);
    auto skeleton=*reinterpret_cast<uint8_t**>(hierarchy+0x16c);
    if(!Readable(skeleton,0x60*info.jointCount)) return originalRender(hierarchy,world,count,layers,joints,blend,flags);
    std::array<unsigned char,256> parents{};
    for(int j=0;j<info.jointCount;++j) parents[j]=skeleton[j*0x60+0x50];
    alignas(16) std::array<float,256*16> copy{};
    float* attachmentJoints=joints;
    if(AnimatedFirstPerson()) {
        auto bind=*reinterpret_cast<float**>(hierarchy+0x170);
        if(Readable(bind,64*info.jointCount) && fps::SuppressJointAnimation(copy.data(),joints,info.jointCount,info.headJoint,parents.data(),bind+16*info.headJoint)) {
            attachmentJoints=copy.data();
            if(++suppressedHeadSamples==1) Log("Animated head: local head animation disabled; body/parent animation retained");
        }
    }
    // Capture the unhidden, evaluated attachment before altering head geometry.
    auto localHead=fps::TransformPoint(attachmentJoints+16*info.headJoint,info.headAttachment);
    auto worldHead=fps::TransformPoint(world,localHead);
    std::array<float,3> offset{worldHead[0]-position.x,worldHead[1]-position.y,worldHead[2]-position.z};
    float distanceSquared=offset[0]*offset[0]+offset[1]*offset[1]+offset[2]*offset[2];
    if(std::isfinite(distanceSquared) && distanceSquared<info.height*info.height*16 && fps::BasisDeterminant(world)>0) {
        headPose.player=player; headPose.model=info.model; headPose.offset=offset;
        headPose.bodyYaw=*reinterpret_cast<uint16_t*>(player+0x58)*(2*fps::pi/65536);
        headPose.sampledAt=GetTickCount(); headPose.valid=true; headPose.rotationValid=false;
        auto inverseBind=*reinterpret_cast<float**>(hierarchy+0x174);
        if(Readable(inverseBind,64*info.jointCount)) {
            // Remove bind-pose axis conventions; retain the head's animated rotation.
            auto deformation=fps::Multiply(inverseBind+16*info.headJoint,attachmentJoints+16*info.headJoint);
            // Rendered characters face -Z; the first-person camera looks along +Z.
            // Correct the attachment's reference frame before applying animation.
            float cameraFacing[16]; fps::Basis(fps::pi,0,cameraFacing);
            auto cameraDeformation=fps::Multiply(cameraFacing,deformation.data());
            auto animatedWorld=fps::Multiply(cameraDeformation.data(),world);
            headPose.rotationValid=fps::RotationOnly(animatedWorld.data(),headPose.rotation);
        }
        ++headPoseSamples;
    }
    if(!hideHead) return originalRender(hierarchy,world,count,layers,attachmentJoints,blend,flags);
    if(attachmentJoints==joints) std::copy(joints,joints+info.jointCount*16,copy.begin());
    const float* head=attachmentJoints+info.headJoint*16;
    for(int j=0;j<info.jointCount;++j) if(fps::Descendant(j,info.headJoint,parents.data(),info.jointCount)) {
        float* matrix=copy.data()+j*16;
        std::fill(matrix,matrix+12,0.0f);
        matrix[12]=head[12]; matrix[13]=head[13]; matrix[14]=head[14]; matrix[15]=1;
    }
    ++hiddenHeadDraws;
    return originalRender(hierarchy,world,count,layers,copy.data(),blend,flags);
}

static LRESULT __cdecl PluginWindowMessage(HWND h,UINT msg,WPARAM w,LPARAM l,BOOL* handled,void*) {
    if(msg==WM_MOUSEWHEEL && gameplayInput) {
        int wheel=GET_WHEEL_DELTA_WPARAM(w);
        if(active && !firstPerson) orbitDistance=std::clamp(orbitDistance-wheel*0.001f,0.5f,3.0f);
        else if(modernControls) queuedCharacterWheel=wheel>0?1:-1;
    }
    if(msg==WM_SETCURSOR && active && mouseLook && LOWORD(l)==HTCLIENT){SetCursor(nullptr);*handled=TRUE;return TRUE;}
    if(msg==WM_KILLFOCUS){active=false;gameplayInput=false;previousMouseButtons=0;queuedCharacterWheel=0;seeded=false;ClipCursor(nullptr);InterlockedExchange(&mouseX,0);InterlockedExchange(&mouseY,0);}
    return 0;
}

static bool Pressed(int key) {
    static bool previous[256]{}; bool down=(GetAsyncKeyState(key)&0x8000)!=0;
    bool edge=down&&!previous[key]; previous[key]=down; return edge;
}

static void __cdecl HookCameraSet(Camera* incoming,int fast) {
    const bool focused=GetForegroundWindow()==gameWindow;
    if(focused) {
        if(Pressed(VK_F5)) { enabled=!enabled; seeded=false; if(!enabled) ClipCursor(nullptr); Log(enabled?"First person ON":"First person OFF"); }
        if(Pressed(VK_F6)) { mouseLook=!mouseLook; if(!mouseLook) ClipCursor(nullptr); Log(mouseLook?"Mouse look ON":"Mouse look OFF"); }
        if(Pressed(VK_F7)) seeded=false;
        if(Pressed(keys[CameraCycle]) && reinterpret_cast<int(__cdecl*)()>(0x574e20)()==-1) {
            selectedMode=fps::NextCameraMode(selectedMode);
            headPose.valid=false;
            seeded=false; enabled=true; ClipCursor(nullptr);
            const char* modes[]={"C mode 1/3: FIRST PERSON","C mode 2/3: THIRD PERSON (native camera)","C mode 3/3: ORBIT (mouse camera)"};
            Log(modes[static_cast<int>(selectedMode)]);
            if(!AnimatedFirstPerson()) Log("Native head-bone animation restored");
        }
    }
    auto player=*reinterpret_cast<uint8_t**>(0x93d810);
    auto gamecam=*reinterpret_cast<uint8_t**>(0x8021b4);
    bool worldCamera=Readable(gamecam,0x230) && incoming==*reinterpret_cast<Camera**>(0x925164) &&
        std::memcmp(incoming->m,gamecam+0xb8,64)==0;
    bool valid=Readable(player,0x10d8) && Readable(gamecam,0x230);
    bool gameplay=valid && *reinterpret_cast<int*>(0x951be0)==0 && *reinterpret_cast<int*>(0x951c30)==0 &&
        *reinterpret_cast<float*>(0x950678)>=1.0f && reinterpret_cast<int(__cdecl*)()>(0x574e20)()==-1;
    auto previousModel=currentCharacter.model;
    gameplayInput=gameplay && focused;
    if(gameplayInput) {
        lastGameplayFrame=GetTickCount();
        currentCharacter=InspectCharacter(player);
        RegisterMovement(currentCharacter);
    } else { previousMouseButtons=0; queuedCharacterWheel=0; }
    if(!valid || !enabled || !focused || !gameplay) { active=false; stableEyeHeight.Reset(); smoothedAttachment.Reset(); ClipCursor(nullptr); if(!valid || !gameplay) seeded=false; originalCameraSet(incoming,fast); return; }
    if(selectedMode==CameraMode::ThirdPerson) {
        active=false; firstPerson=false; stableEyeHeight.Reset(); smoothedAttachment.Reset(); ClipCursor(nullptr);
        if(worldCamera) {
            Camera nativeCamera=*incoming;
            nativeCamera.fov=fps::VerticalFov(fovDegrees,std::clamp(nativeCamera.aspect,0.2f,2.0f));
            originalCameraSet(&nativeCamera,fast);
        } else originalCameraSet(incoming,fast);
        return;
    }
    if(!worldCamera) { originalCameraSet(incoming,fast); return; }
    Vec position=*reinterpret_cast<Vec*>(player+0x5c);
    if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z) || std::fabs(position.x)>100000 || std::fabs(position.y)>100000 || std::fabs(position.z)>100000) {
        active=false; originalCameraSet(incoming,fast); return;
    }
    auto oldModel=previousModel;
    bool nextFirstPerson=currentCharacter.headJoint>=0 && fps::IsFirstPersonMode(selectedMode);
    if(oldModel!=currentCharacter.model) seeded=false;
    if(firstPerson!=nextFirstPerson) { seeded=false; firstPerson=nextFirstPerson; }
    RegisterMovement(currentCharacter);
    if(!seeded || lastPlayer!=player) {
        yaw=*reinterpret_cast<uint16_t*>(player+0x58)*(6.28318530718f/65536.0f); pitch=firstPerson?0.0f:-0.25f;
        bodyBlend=0; stableEyeHeight.Reset(); smoothedAttachment.Reset();
        seeded=true; lastPlayer=player; InterlockedExchange(&mouseX,0); InterlockedExchange(&mouseY,0);
        mouseMotion.Reset();
        if(logfile) { fprintf(logfile,"Character=%p model=%p height=%.3f headJoint=%d mode=%s\n",player,currentCharacter.model,currentCharacter.height,currentCharacter.headJoint,firstPerson?"FIRST_PERSON":"THIRD_PERSON_MOUSE_ORBIT"); fflush(logfile); }
    }
    bool rotating=mouseLook;
    if(rotating) {
        yaw+=InterlockedExchange(&mouseX,0)*sensitivity;
        pitch-=InterlockedExchange(&mouseY,0)*sensitivity;
        pitch=fps::LimitLook(pitch,lookUpDegrees,lookDownDegrees); yaw=std::remainder(yaw,6.28318530718f);
    } else { InterlockedExchange(&mouseX,0); InterlockedExchange(&mouseY,0); }
    if(!mouseLook && firstPerson) yaw=*reinterpret_cast<uint16_t*>(player+0x58)*(6.28318530718f/65536.0f);
    Camera camera=*incoming;
    float matrix[16]; fps::Basis(yaw,pitch,matrix);
    DWORD now=GetTickCount();
    float dt=lastCameraTime?std::min((now-lastCameraTime)/1000.0f,0.1f):0.016f; lastCameraTime=now;
    aimBlend+=((firstPerson && Held(Focus) && currentCharacter.secondary==fps::SecondaryAction::Zoom?1.0f:0.0f)-aimBlend)*(1-std::exp(-14*dt));
    bodyBlend+=(fps::LookDownBlend(pitch)-bodyBlend)*(1-std::exp(-12*dt));
    float height=currentCharacter.height;
    animatedHeadApplied=false;
    if(firstPerson) {
        float offset=height*(0.015f+bodyBlend*0.08f);
        matrix[12]=position.x+std::sin(yaw)*offset;
        matrix[13]=position.y+height*(0.87f+bodyBlend*0.035f);
        matrix[14]=position.z+std::cos(yaw)*offset;
        if(AnimatedFirstPerson()) {
            // Smooth the skeleton offset relative to the moving root, avoiding
            // horizontal movement lag while retaining animated attachment motion.
            std::array<float,3> attachment{0,height*0.87f,0};
            bool usable=headPose.valid && headPose.player==player &&
                headPose.model==currentCharacter.model && now-headPose.sampledAt<150;
            if(usable) {
                float bodyYaw=*reinterpret_cast<uint16_t*>(player+0x58)*(2*fps::pi/65536);
                auto currentOffset=fps::RotateOffset(headPose.offset,bodyYaw-headPose.bodyYaw);
                // Smooth in character space so turning does not drag the camera behind.
                attachment=fps::RotateOffset(currentOffset,-bodyYaw);
            }
            auto smooth=smoothedAttachment.Update(attachment,dt,height*1.5f);
            float bodyYaw=*reinterpret_cast<uint16_t*>(player+0x58)*(2*fps::pi/65536);
            auto worldOffset=fps::RotateOffset(smooth,bodyYaw);
            matrix[12]=position.x+worldOffset[0]+std::sin(yaw)*offset;
            matrix[13]=stableEyeHeight.Update(position.y,dt,height*0.75f)
                +worldOffset[1]+height*bodyBlend*0.035f;
            matrix[14]=position.z+worldOffset[2]+std::cos(yaw)*offset;
            animatedHeadApplied=usable;
        } else { stableEyeHeight.Reset(); smoothedAttachment.Reset(); }
    } else {
        float distance=orbitDistance;
        matrix[12]=position.x-matrix[8]*distance;
        matrix[13]=position.y+height*0.65f-matrix[9]*distance;
        matrix[14]=position.z-matrix[10]*distance;
    }
    std::copy(matrix,matrix+16,aimCamera.begin());
    aimPlayer=player; aimSampleTime=now;
    std::memcpy(camera.m,matrix,sizeof(matrix));
    camera.fov=fps::VerticalFov(fovDegrees*(1-0.25f*aimBlend),std::clamp(camera.aspect,0.2f,2.0f)); camera.nearClip=0.008f;
    // Native character controls calculate movement relative to this camera yaw.
    *reinterpret_cast<int32_t*>(gamecam+0x1fc)=static_cast<int32_t>(yaw*(65536.0f/6.28318530718f))&0xffff;
    active=true;
    lastWorldFrame=GetTickCount();
    if(rotating) {
        RECT r; GetClientRect(gameWindow,&r); POINT start{r.left,r.top},end{r.right,r.bottom};
        ClientToScreen(gameWindow,&start); ClientToScreen(gameWindow,&end); RECT screen{start.x,start.y,end.x,end.y}; ClipCursor(&screen);
    } else ClipCursor(nullptr);
    originalCameraSet(&camera,fast);
    if(now-lastLog>2000) {
        lastLog=now;
        if(logfile) { fprintf(logfile,"%s player=%p pos=%.3f,%.3f,%.3f eye=%.3f,%.3f,%.3f yaw=%.3f pitch=%.3f bodyBlend=%.3f hiddenHeadDraws=%u mousePolls=%u mouseMotionSamples=%u secondary=%d guardFrames=%u animatedHead=%d headPoseSamples=%u\n",firstPerson?(AnimatedFirstPerson()?"HEAD":"FP"):"ORBIT",player,position.x,position.y,position.z,matrix[12],matrix[13],matrix[14],yaw,pitch,bodyBlend,hiddenHeadDraws,mousePolls,mouseMotionSamples,static_cast<int>(currentCharacter.secondary),guardFrames,animatedHeadApplied?1:0,headPoseSamples); fflush(logfile); }
    }
}

// PC Bolt_Shoot and its final Bolt_Add call verified in the runtime analysis image.
using BoltShootFn=void(__cdecl*)(uint8_t*,int,int);
using BoltAddFn=void*(__cdecl*)(uint8_t*,const Vec*,const float*,int,int);
static BoltShootFn originalBoltShoot=reinterpret_cast<BoltShootFn>(0x583af0);
static BoltAddFn originalBoltAdd=reinterpret_cast<BoltAddFn>(0x582e70);
static bool CameraAimFor(uint8_t* shooter) {
    return enabled && active && firstPerson && fps::UsesCameraAim(selectedMode,aimBot) &&
        gameplayInput && GetForegroundWindow()==gameWindow && shooter==aimPlayer &&
        shooter==*reinterpret_cast<uint8_t**>(0x93d810) && GetTickCount()-aimSampleTime<150 &&
        currentCharacter.secondary==fps::SecondaryAction::Zoom && Readable(shooter,0x10d8);
}
static void __cdecl HookBoltShoot(uint8_t* shooter,int type,int flags) {
    if(!CameraAimFor(shooter)) { originalBoltShoot(shooter,type,flags); return; }
    // Suppress only this shot's target/intercept branches, including the native
    // multi-bolt lifetime shortening based on distance to an auto-selected NPC.
    auto& state=*reinterpret_cast<uint32_t*>(shooter+0xe20);
    auto& targeting=*reinterpret_cast<uint32_t*>(shooter+0xeec);
    auto& scriptTarget=*reinterpret_cast<uint8_t**>(shooter+0xea0);
    uint32_t savedAim=state&0x800,savedTargeting=targeting&0x38000;
    auto savedTarget=scriptTarget;
    state&=~0x800u; targeting&=~0x38000u; scriptTarget=nullptr;
    originalBoltShoot(shooter,type,flags);
    state=(state&~0x800u)|savedAim;
    targeting=(targeting&~0x38000u)|savedTargeting; scriptTarget=savedTarget;
}
static void* __cdecl HookShotBoltAdd(uint8_t* shooter,const Vec* origin,const float* rotation,int type,int flags) {
    if(!CameraAimFor(shooter)) return originalBoltAdd(shooter,origin,rotation,type,flags);
    auto shot=fps::CameraShot(aimCamera.data());
    // Preserve the native animated muzzle locator for each gun/barrel.
    void* bolt=originalBoltAdd(shooter,origin,shot.data(),type,flags);
    if(bolt && ++cameraAimedShots==1) Log("First-person muzzle-fired camera-directed bolt created; native NPC targeting bypassed");
    return bolt;
}
static void InstallAimHooks() {
    const uint8_t shootSignature[]={0x81,0xec,0x6c,0x01,0,0};
    const uint8_t addSignature[]={0x8b,0x44,0x24,0x10,0x8b,0x0d,0xbc,0x40,0x80,0};
    if(std::memcmp(reinterpret_cast<void*>(0x583af0),shootSignature,sizeof(shootSignature)) ||
       std::memcmp(reinterpret_cast<void*>(0x582e70),addSignature,sizeof(addSignature))) {
        Log("Projectile signature differs; camera aiming disabled"); return;
    }
    const uintptr_t sites[]={0x4b1d27,0x5841e0};
    const uintptr_t targets[]={0x583af0,0x582e70};
    const uintptr_t hooks[]={reinterpret_cast<uintptr_t>(HookBoltShoot),reinterpret_cast<uintptr_t>(HookShotBoltAdd)};
    for(int i=0;i<2;++i) {
        auto call=reinterpret_cast<uint8_t*>(sites[i]);
        if(call[0]!=0xe8 || reinterpret_cast<uintptr_t>(call+5)+*reinterpret_cast<int32_t*>(call+1)!=targets[i]) {
            Log("Projectile call differs; camera aiming disabled"); return;
        }
    }
    for(int i=0;i<2;++i) {
        auto call=reinterpret_cast<uint8_t*>(sites[i]); DWORD old;
        if(!VirtualProtect(call,5,PAGE_EXECUTE_READWRITE,&old)) { Log("Projectile hook protection failed"); return; }
        *reinterpret_cast<int32_t*>(call+1)=static_cast<int32_t>(hooks[i]-reinterpret_cast<uintptr_t>(call+5));
        FlushInstructionCache(GetCurrentProcess(),call,5); VirtualProtect(call,5,old,&old);
    }
    Log("First-person projectile aim hooks installed");
}

static void InstallCameraHook() {
    if(cameraHookInstalled) return;
    const uint8_t guardSignature[]={0x8b,0x15,0xe4,0x30,0x91,0x02};
    nativeGuardAvailable=std::memcmp(reinterpret_cast<void*>(0x58dfa0),guardSignature,sizeof(guardSignature))==0;
    Log(nativeGuardAvailable?"Native guard animation entry verified":"Native guard entry differs; blocking disabled");
    const uint8_t signature[]={0x81,0xec,0xc8,0,0,0,0x55,0x56};
    if(std::memcmp(reinterpret_cast<void*>(0x5ed540),signature,sizeof(signature))!=0) { Log("Unsupported executable: camera signature differs; camera disabled"); return; }
    auto call=reinterpret_cast<uint8_t*>(0x603cc9);
    if(call[0]!=0xe8 || reinterpret_cast<uintptr_t>(call+5)+*reinterpret_cast<int32_t*>(call+1)!=0x5ed540) { Log("Unsupported camera call site; camera disabled"); return; }
    originalCameraSet=reinterpret_cast<CameraSetFn>(0x5ed540);
    auto mouseCall=reinterpret_cast<uint8_t*>(0x6d79d3);
    const uint8_t mouseSignature[]={0xf6,0x05,0xa8,0x55,0x97,0x02,0x01};
    if(std::memcmp(reinterpret_cast<void*>(0x6d57d0),mouseSignature,sizeof(mouseSignature))!=0 ||
        mouseCall[0]!=0xe8 || reinterpret_cast<uintptr_t>(mouseCall+5)+*reinterpret_cast<int32_t*>(mouseCall+1)!=0x6d57d0) {
        Log("Unsupported native mouse input; camera disabled"); return;
    }
    const uint8_t renderSignature[]={0x83,0xec,0x6c,0x53,0x55};
    if(std::memcmp(reinterpret_cast<void*>(0x5f4ff0),renderSignature,sizeof(renderSignature))!=0) { Log("Unsupported renderer; camera disabled"); return; }
    constexpr uintptr_t renderCalls[]={0x57dad8,0x57e017,0x5fecfd,0x604c39,0x62fb8f,0x6304ef,0x6305ff};
    for(auto address:renderCalls) {
        auto site=reinterpret_cast<uint8_t*>(address);
        if(site[0]!=0xe8 || reinterpret_cast<uintptr_t>(site+5)+*reinterpret_cast<int32_t*>(site+1)!=0x5f4ff0) { Log("Unsupported renderer call site; camera disabled"); return; }
    }
    constexpr uintptr_t keyboardCalls[]={0x6d7978,0x6d8781,0x6d935d,0x6daaf0};
    const uint8_t keyboardSignature[]={0xf6,0x05,0xa8,0x55,0x97,0x02,0x02};
    if(std::memcmp(reinterpret_cast<void*>(0x6d5990),keyboardSignature,sizeof(keyboardSignature))) {
        Log("Unsupported native keyboard input; camera disabled"); return;
    }
    for(auto address:keyboardCalls) {
        auto keyboardCall=reinterpret_cast<uint8_t*>(address);
        if(keyboardCall[0]!=0xe8 || reinterpret_cast<uintptr_t>(keyboardCall+5)+*reinterpret_cast<int32_t*>(keyboardCall+1)!=0x6d5990) {
            Log("Unsupported native keyboard call site; camera disabled"); return;
        }
    }
    DWORD old;
    if(!VirtualProtect(call,5,PAGE_EXECUTE_READWRITE,&old)) { Log("Camera hook protection failed"); return; }
    *reinterpret_cast<int32_t*>(call+1)=static_cast<int32_t>(reinterpret_cast<uintptr_t>(HookCameraSet)-reinterpret_cast<uintptr_t>(call+5));
    FlushInstructionCache(GetCurrentProcess(),call,5);
    VirtualProtect(call,5,old,&old);
    if(VirtualProtect(mouseCall,5,PAGE_EXECUTE_READWRITE,&old)) {
        *reinterpret_cast<int32_t*>(mouseCall+1)=static_cast<int32_t>(reinterpret_cast<uintptr_t>(HookReadMouse)-reinterpret_cast<uintptr_t>(mouseCall+5));
        FlushInstructionCache(GetCurrentProcess(),mouseCall,5); VirtualProtect(mouseCall,5,old,&old);
        Log("Native DirectInput mouse hook installed");
    } else Log("ERROR: native mouse hook protection failed");
    for(auto address:keyboardCalls) {
        auto keyboardCall=reinterpret_cast<uint8_t*>(address);
        if(VirtualProtect(keyboardCall,5,PAGE_EXECUTE_READWRITE,&old)) {
            *reinterpret_cast<int32_t*>(keyboardCall+1)=static_cast<int32_t>(reinterpret_cast<uintptr_t>(HookReadKeyboard)-reinterpret_cast<uintptr_t>(keyboardCall+5));
            FlushInstructionCache(GetCurrentProcess(),keyboardCall,5); VirtualProtect(keyboardCall,5,old,&old);
        } else Log("ERROR: native keyboard hook protection failed");
    }
    Log("Native Q-to-K mapping installed at all four keyboard poll sites");
    for(auto address:renderCalls) {
        auto site=reinterpret_cast<uint8_t*>(address);
        if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old)) { Log("Head renderer hook protection failed"); continue; }
        *reinterpret_cast<int32_t*>(site+1)=static_cast<int32_t>(reinterpret_cast<uintptr_t>(HookRender)-reinterpret_cast<uintptr_t>(site+5));
        FlushInstructionCache(GetCurrentProcess(),site,5); VirtualProtect(site,5,old,&old);
    }
    InstallAimHooks();
    cameraHookInstalled=true;
    Log("Camera and head-render hooks installed; C cycles first person / third person / orbit");
}

static bool crosshair=true;
static void* PatchSlot(void** table,int slot,void* hook) {
    DWORD old; if(!VirtualProtect(table+slot,sizeof(void*),PAGE_READWRITE,&old)) return nullptr;
    void* previous=table[slot]; table[slot]=hook;
    VirtualProtect(table+slot,sizeof(void*),old,&old); return previous;
}

static void __cdecl PluginEndScene(IDirect3DDevice9* device,void*) {
    if(GetTickCount()-lastWorldFrame>200 && active) { active=false; ClipCursor(nullptr); }
    if(crosshair && active && firstPerson && GetForegroundWindow()==gameWindow) {
        IDirect3DStateBlock9* state=nullptr;
        if(SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL,&state))) {
            D3DVIEWPORT9 viewport{}; device->GetViewport(&viewport);
            float x=viewport.X+viewport.Width*0.5f,y=viewport.Y+viewport.Height*0.5f;
            struct Vertex { float x,y,z,w; DWORD color; };
            constexpr DWORD color=0xc0ffffff;
            Vertex lines[]={{x-8,y,0,1,color},{x-3,y,0,1,color},{x+3,y,0,1,color},{x+8,y,0,1,color},
                {x,y-8,0,1,color},{x,y-3,0,1,color},{x,y+3,0,1,color},{x,y+8,0,1,color}};
            device->SetVertexShader(nullptr); device->SetPixelShader(nullptr); device->SetTexture(0,nullptr);
            device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
            device->SetRenderState(D3DRS_ZENABLE,FALSE); device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
            device->SetRenderState(D3DRS_LIGHTING,FALSE); device->SetRenderState(D3DRS_FOGENABLE,FALSE);
            device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE); device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
            device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA); device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
            device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1); device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
            device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1); device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
            device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
            device->DrawPrimitiveUP(D3DPT_LINELIST,4,lines,sizeof(Vertex));
            state->Apply(); state->Release();
        }
    }
}

static void LoadConfig() {
    wchar_t file[MAX_PATH]{}; if(modHost&&modHost->configPath)modHost->configPath(modHandle,file,MAX_PATH);
    if(!file[0]){GetModuleFileNameW(nullptr,file,MAX_PATH);wchar_t* slash=wcsrchr(file,L'\\');if(!slash)return;slash[1]=0;lstrcatW(file,L"TCSFirstPerson.ini");}
    auto number=[&](const wchar_t* key,float fallback) {
        wchar_t value[64]; GetPrivateProfileStringW(L"Camera",key,L"",value,64,file);
        if(!value[0]) return fallback; float parsed=static_cast<float>(wcstod(value,nullptr)); return std::isfinite(parsed)?parsed:fallback;
    };
    fovDegrees=std::clamp(number(L"HorizontalFOV",90),60.0f,120.0f);
    lookUpDegrees=std::clamp(number(L"LookUpDegrees",80),1.0f,89.0f);
    lookDownDegrees=std::clamp(number(L"LookDownDegrees",85),1.0f,89.0f);
    sensitivity=std::clamp(number(L"MouseSensitivity",0.0025f),0.0001f,0.02f);
    orbitDistance=std::clamp(number(L"OrbitDistance",1.2f),0.5f,3.0f);
    hideHead=GetPrivateProfileIntW(L"Camera",L"HideHead",1,file)!=0;
    modernControls=GetPrivateProfileIntW(L"Controls",L"ModernControls",1,file)!=0;
    auto keyCode=[](const wchar_t* name)->int {
        if(wcslen(name)==1) {
            wchar_t c=towupper(name[0]); if((c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')) return c;
        }
        struct NamedKey { const wchar_t* name; int key; };
        constexpr NamedKey names[]={{L"Mouse1",VK_LBUTTON},{L"Mouse2",VK_RBUTTON},{L"Mouse3",VK_MBUTTON},
            {L"Mouse4",VK_XBUTTON1},{L"Mouse5",VK_XBUTTON2},{L"Space",VK_SPACE},{L"Tab",VK_TAB},
            {L"Shift",VK_SHIFT},{L"Ctrl",VK_CONTROL},{L"Alt",VK_MENU},{L"Enter",VK_RETURN},
            {L"Up",VK_UP},{L"Down",VK_DOWN},{L"Left",VK_LEFT},{L"Right",VK_RIGHT},{L"None",0}};
        for(auto entry:names) if(_wcsicmp(name,entry.name)==0) return entry.key;
        if(towupper(name[0])==L'F') { wchar_t* end=nullptr; long n=wcstol(name+1,&end,10); if(end!=name+1 && *end==0 && n>=1&&n<=12) return VK_F1+static_cast<int>(n)-1; }
        return -1;
    };
    for(int binding=0;binding<BindCount;++binding) {
        wchar_t name[64]; GetPrivateProfileStringW(L"Bindings",bindingNames[binding],L"",name,64,file);
        if(name[0]) { int code=keyCode(name); if(code>=0) keys[binding]=code; else Log("Unrecognized binding name; retaining its PC default"); }
    }
    crosshair=GetPrivateProfileIntW(L"Camera",L"Crosshair",1,file)!=0;
}

static void __cdecl SettingChanged(TCSSettingValue v,void* context) {
    int id=static_cast<int>(reinterpret_cast<intptr_t>(context));
    switch(id) {
    case 0:fovDegrees=v.real;break; case 1:lookUpDegrees=v.real;break;case 2:lookDownDegrees=v.real;break;
    case 3:sensitivity=v.real;break;case 4:orbitDistance=v.real;break;case 5:hideHead=v.integer!=0;break;
    case 6:crosshair=v.integer!=0;break;case 7:modernControls=v.integer!=0;break;
    case 100:
        animatedFirstPerson=v.integer==1;
        headPose.valid=false; stableEyeHeight.Reset(); smoothedAttachment.Reset(); seeded=false;
        break;
    case 101:aimBot=v.integer!=0;break;
    default:if(id>=8&&id<8+BindCount)keys[id-8]=v.integer;break;
    }
}
static bool settingsRegistered=true;
static void AddSetting(const char* category,const char* key,const char* label,TCSSettingType type,TCSSettingValue value,float lo,float hi,float step,int id) {
    TCSSettingDescriptor d{};d.structSize=sizeof(d);d.category=category;d.key=key;d.label=label;d.type=type;d.defaultValue=value;d.minimum=lo;d.maximum=hi;d.step=step;d.changed=SettingChanged;d.context=reinterpret_cast<void*>(static_cast<intptr_t>(id));
    if(!modHost->registerSetting(modHandle,&d)) settingsRegistered=false;
}
static void RegisterSettings() {
    static const char* bindingKeys[]={"Forward","Back","Left","Right","Attack","Jump","Ability","Interact","SwitchCharacter","PreviousCharacter","NextCharacter","Walk","CycleCamera","FocusOrOrbit"};
    AddSetting("Gameplay","AimBot","Aim-bot (native targeting)",TCS_SETTING_BOOL,{aimBot?1:0,0},0,1,1,101);
    static const char* firstPersonChoices[]={"Normal","Animated"};
    TCSSettingDescriptor style{};style.structSize=sizeof(style);style.category="Camera";
    style.key="FirstPersonStyle";style.label="First person style";style.type=TCS_SETTING_ENUM;
    style.defaultValue.integer=animatedFirstPerson?1:0;style.minimum=0;style.maximum=1;style.step=1;
    style.choices=firstPersonChoices;style.choiceCount=2;style.changed=SettingChanged;
    style.context=reinterpret_cast<void*>(static_cast<intptr_t>(100));
    if(!modHost->registerSetting(modHandle,&style)) settingsRegistered=false;
    AddSetting("Camera","HorizontalFOV","Horizontal field of view",TCS_SETTING_FLOAT,{0,fovDegrees},60,120,1,0);
    AddSetting("Camera","LookUpDegrees","Look up limit",TCS_SETTING_FLOAT,{0,lookUpDegrees},1,89,1,1);
    AddSetting("Camera","LookDownDegrees","Look down limit",TCS_SETTING_FLOAT,{0,lookDownDegrees},1,89,1,2);
    AddSetting("Camera","MouseSensitivity","Mouse sensitivity",TCS_SETTING_FLOAT,{0,sensitivity},0.0001f,0.02f,0.0002f,3);
    AddSetting("Camera","OrbitDistance","Orbit distance",TCS_SETTING_FLOAT,{0,orbitDistance},0.5f,3,0.1f,4);
    AddSetting("Camera","HideHead","Hide head in first person",TCS_SETTING_BOOL,{hideHead?1:0,0},0,1,1,5);
    AddSetting("Camera","Crosshair","First person crosshair",TCS_SETTING_BOOL,{crosshair?1:0,0},0,1,1,6);
    AddSetting("Gameplay","ModernControls","Modern keyboard and mouse controls",TCS_SETTING_BOOL,{modernControls?1:0,0},0,1,1,7);
    for(int i=0;i<BindCount;++i){TCSSettingValue v{};v.integer=keys[i];AddSetting("Controls",bindingKeys[i],bindingKeys[i],TCS_SETTING_KEY,v,1,255,1,8+i);}
}
static void __cdecl PluginDeviceCreated(HWND window,IDirect3DDevice9*,void*) {
    gameWindow=window;InstallCameraHook();
    Log("TCS First Person initialized as a TCS Mod Loader plugin");
}
static void __cdecl PluginDeviceReset(IDirect3DDevice9*,int,void*) { }
static BOOL __cdecl PluginInitialize(const TCSModHost* host,const TCSModHandle* outHandle) {
    if(!host||host->apiVersion!=TCS_MOD_API_VERSION||host->structSize<sizeof(TCSModHost)||!outHandle)return FALSE;
    modHost=host;modHandle=*outHandle;
    logfile=_fsopen("TCSFirstPerson.log","w",_SH_DENYNO);
    LoadConfig();RegisterSettings();
    if(!settingsRegistered) return FALSE;
    modHost->registerDeviceCallbacks(modHandle,PluginDeviceCreated,PluginDeviceReset,PluginEndScene,nullptr);
    modHost->registerWindowCallback(modHandle,PluginWindowMessage,nullptr);
    return TRUE;
}
TCS_MOD_EXPORT BOOL __cdecl TCSMod_Initialize(const TCSModHost* host,TCSModHandle* out) {
    if(!host || !out || host->apiVersion!=TCS_MOD_API_VERSION || host->structSize<sizeof(TCSModHost) ||
       !host->registerMod || !host->registerSetting || !host->registerDeviceCallbacks || !host->registerWindowCallback) return FALSE;
    TCSModInfo info{sizeof(TCSModInfo),"tcs.firstperson","TCS First Person","0.4.0"};
    auto handle=host->registerMod(&info);if(!handle)return FALSE;*out=handle;
    return PluginInitialize(host,out);
}
TCS_MOD_EXPORT void __cdecl TCSMod_Shutdown(TCSModHandle) { if(logfile){fclose(logfile);logfile=nullptr;} }
