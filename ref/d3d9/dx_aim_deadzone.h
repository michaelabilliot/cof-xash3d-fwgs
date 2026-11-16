#pragma once
// Arma-style aiming deadzone. The engine's view angles stay the true aim: the
// server fires along them and the client poses the viewmodel from them. Only
// the rendered camera lags: mouse motion first swings the aim (and so the
// weapon) inside a cone of `radius` degrees around the camera, and the camera
// turns only once the aim pushes against that cone's edge.
#include <algorithm>
#include <cmath>
#include <cstring>
namespace aim_deadzone {
inline float wrap(float a) {
 a=std::fmod(a+180.f,360.f);
 if(a<0.f)a+=360.f;
 return a-180.f;
}
// Iron-sight / scope poses in CoF viewmodels: iron_*, iron-*, *ironsights*,
// scope_to. Transitions back out (iron_from, from_ironsights, scope_from)
// release the camera again.
inline bool sightsSequence(const char* label) {
 if(!label)return false;
 char lower[33]{};
 for(int i=0;i<32&&label[i];++i)lower[i]=static_cast<char>(label[i]>='A'&&label[i]<='Z'?label[i]+32:label[i]);
 return (std::strstr(lower,"iron")||std::strstr(lower,"scope"))&&!std::strstr(lower,"from");
}
struct State {
 bool valid=false;
 float camera[2]{}; // pitch, yaw of the rendered camera
 float time=0.f;
 float blend=1.f;   // 1 = full deadzone, 0 = camera locked to aim (sights)
};
struct Input {
 float radius=0.f;  // degrees; <=0 disables and resets
 bool sights=false; // shrink the deadzone to nothing while aiming down sights
 bool snap=false;   // lock to aim immediately (scope overlay)
 float sightsSpeed=6.f; // deadzone fraction per second when entering/leaving sights
};
// aim: pitch,yaw from the engine. camera receives the rendered pitch,yaw.
inline void update(State& s,const float* aim,float time,const Input& in,float* camera) {
 camera[0]=aim[0];camera[1]=aim[1];
 if(!(in.radius>0.f)||!std::isfinite(in.radius)||!std::isfinite(aim[0])||!std::isfinite(aim[1])||!std::isfinite(time)) {
  s=State{};return;
 }
 // A clock that ran backwards is a new map or a reload: start centred.
 if(s.valid&&time<s.time)s=State{};
 const float dt=s.valid?std::min(time-s.time,.25f):0.f;
 const float target=in.sights||in.snap?0.f:1.f;
 if(in.snap||!s.valid)s.blend=target;
 else {
  const float step=std::max(0.f,in.sightsSpeed)*dt;
  s.blend=s.blend<target?std::min(target,s.blend+step):std::max(target,s.blend-step);
  if(!(in.sightsSpeed>0.f))s.blend=target;
 }
 const float radius=std::min(in.radius,60.f)*s.blend;
 float off[2]={0.f,0.f};
 if(s.valid){off[0]=aim[0]-s.camera[0];off[1]=wrap(aim[1]-s.camera[1]);}
 const float length=std::sqrt(off[0]*off[0]+off[1]*off[1]);
 if(!std::isfinite(length)||radius<=0.f){off[0]=off[1]=0.f;}
 else if(length>radius){off[0]*=radius/length;off[1]*=radius/length;}
 camera[0]=std::clamp(aim[0]-off[0],std::min(-89.f,aim[0]),std::max(89.f,aim[0]));
 camera[1]=aim[1]-off[1];
 s.camera[0]=camera[0];s.camera[1]=camera[1];
 s.time=time;s.valid=true;
}
// The usercmd carries one set of view angles for both shooting and movement,
// so pmove builds wishdir from the aim. To walk where the camera looks,
// re-express the camera-relative forward/side input in the aim's frame.
// yawOffset = aim yaw - camera yaw (degrees).
inline void rotateMove(float yawOffset,float& forward,float& side) {
 if(!std::isfinite(yawOffset)||yawOffset==0.f)return;
 const float r=yawOffset*3.14159265358979f/180.f,c=std::cos(r),s=std::sin(r);
 const float f=forward,sd=side;
 forward=f*c-sd*s;
 side=f*s+sd*c;
}
// CoF's PM_CheckParamters halves forwardmove whenever sidemove is also set
// (and drops sidemove entirely while sprinting). Apply that rule to the
// camera-relative input first, rotate, then pre-double forwardmove when the
// rotated command is diagonal so pmove's halving lands on the intended
// camera-relative direction and speed.
inline void cofMove(float yawOffset,float& forward,float& side) {
 if(!std::isfinite(yawOffset)||yawOffset==0.f)return;
 if(forward!=0.f&&side!=0.f)forward*=.5f;
 rotateMove(yawOffset,forward,side);
 if(forward!=0.f&&side!=0.f)forward*=2.f;
}
// Sprint poses (sprint_to, sprint_idle; not sprint_from). Sprinting zeroes
// sidemove in CoF pmove, so the camera must sit on the aim while sprinting.
inline bool sprintSequence(const char* label) {
 if(!label)return false;
 char lower[33]{};
 for(int i=0;i<32&&label[i];++i)lower[i]=static_cast<char>(label[i]>='A'&&label[i]<='Z'?label[i]+32:label[i]);
 return std::strstr(lower,"sprint")&&!std::strstr(lower,"from");
}
// CoF melee viewmodels: v_switchblade(_doctor), v_nightstick, v_branch,
// v_axe(_doctor), v_e3cc3_pickaxe, v_sledgehammer, v_sledgeshovel.
inline bool meleeModel(const char* path) {
 if(!path)return false;
 const char* base=path;
 for(const char* p=path;*p;++p)if(*p=='/'||*p=='\\')base=p+1;
 char lower[64]{};
 for(int i=0;i<63&&base[i];++i)lower[i]=static_cast<char>(base[i]>='A'&&base[i]<='Z'?base[i]+32:base[i]);
 if(std::strncmp(lower,"v_",2))return false;
 for(const char* key:{"switchblade","nightstick","branch","axe","sledge"})
  if(std::strstr(lower,key))return true;
 return false;
}
} // namespace aim_deadzone
