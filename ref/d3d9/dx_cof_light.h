#pragma once
#include <cstdint>
#include <cmath>
// Native FlashFlags: 74910(main/reduced), 74e10(weapon), 75170(handheld).
enum dx_cof_light_flag_t : uint32_t {
 DX_COF_MAIN=1, DX_COF_REDUCED=2, DX_COF_WEAPON=4, DX_COF_HANDHELD=8,
 DX_COF_SUPPORTED_FLAGS=DX_COF_MAIN|DX_COF_REDUCED|DX_COF_WEAPON|DX_COF_HANDHELD,
 DX_COF_NATIVE_FLAGS=0xffff
};
struct dx_cof_light_profile_t {float intensity,innerDegrees,outerDegrees;};
inline dx_cof_light_profile_t DX_CofLightProfile(unsigned flags) {
 if(flags&DX_COF_HANDHELD)return {1.4f,30.f,50.f};
 if(flags&DX_COF_WEAPON)return {.5f,40.f,70.f};
 if(flags&DX_COF_MAIN)return {.46f,30.f,50.f};
 if(flags&DX_COF_REDUCED)return {.15f,30.f,50.f};
 return {0.f,30.f,50.f};
}
enum dx_cof_light_reason_t : uint32_t {
 DX_COF_NO_PLAYER=1, DX_COF_OTHER_VIEW=2, DX_COF_BOAT=4,
 DX_COF_ATTACHED=8, DX_COF_WHEELCHAIR=16, DX_COF_SPECIAL=32,
 DX_COF_PUZZLE=64, DX_COF_NO_VIEWMODEL=128, DX_COF_INVALID=256
};
struct dx_cof_light_state_v2 {
 uint32_t size=sizeof(dx_cof_light_state_v2),version=2,rawFlags=0,eligibleFlags=0,reasons=0;
 float time=0,origin[3]{},direction[3]{};
};
static_assert(sizeof(dx_cof_light_state_v2)==48,"CoF light ABI");
inline unsigned DX_CofEligible(unsigned raw,unsigned reasons) {
 if(reasons & ~DX_COF_NO_VIEWMODEL) return 0;
 return (raw&DX_COF_SUPPORTED_FLAGS) & ((reasons&DX_COF_NO_VIEWMODEL)?(DX_COF_MAIN|DX_COF_REDUCED):DX_COF_SUPPORTED_FLAGS);
}
inline bool DX_CofValid(const dx_cof_light_state_v2& p) {
 if(p.size!=sizeof(p)||p.version!=2||!std::isfinite(p.time))return false;
 float length=0;
 for(int i=0;i<3;++i){if(!std::isfinite(p.origin[i])||!std::isfinite(p.direction[i]))return false;length+=p.direction[i]*p.direction[i];}
 return std::isfinite(length)&&length>1e-8f;
}
inline bool DX_CofFresh(float published,float now) {
 return std::isfinite(published)&&std::isfinite(now)&&now>=published&&now-published<.25f;
}
// Both production and CPU fixtures submit through this checked sink contract.
template<class Light,class Set,class Enable> bool DX_CofSubmit(unsigned slot,const Light& light,Set set,Enable enable) {
 if(set(slot,light)<0){enable(slot,false);return false;}
 if(enable(slot,true)<0){enable(slot,false);return false;}
 return true;
}

struct dx_cof_light_bridge_t {
 dx_cof_light_state_v2 packet{};
 bool published=false;
 bool publish(const dx_cof_light_state_v2* source) {
  if(!source||!DX_CofValid(*source)){*this={};return false;}
  packet=*source;packet.rawFlags&=DX_COF_NATIVE_FLAGS;
  packet.eligibleFlags&=DX_CofEligible(packet.rawFlags,packet.reasons);
  published=true;return true;
 }
 unsigned flags(float now)const{return published&&DX_CofFresh(packet.time,now)?packet.eligibleFlags:0;}
};

inline bool DX_CofEmissionEnabled(bool remix,bool global,bool fixed,bool cof,int maxLights) {
 return remix&&global&&fixed&&cof&&maxLights>0;
}
