#pragma once
#include <cmath>
#include <cstring>
#include <algorithm>
#include <array>
namespace cof_features {
constexpr int pickupLimit=8192;
inline bool pickupClass(const char* name) {
 return name && (!std::strncmp(name,"weapon_",7)||!std::strncmp(name,"ammo_",5)||
  !std::strncmp(name,"item_",5)||!std::strcmp(name,"weaponbox")||!std::strcmp(name,"inventory_item"));
}
inline bool flareModel(const char* name,bool held) {
 return name&&!std::strcmp(name,held?"models/weapons/flare/v_flare.mdl":"models/weapons/flare/w_flare.mdl");
}
inline bool litFlareSequence(const char* label,float frame) {
 if(!label)return false;
 if(!std::strncmp(label,"flare_ignite",32))return frame>=11.f;
 return (!std::strstr(label,"unlit")&&std::strstr(label,"lit"))||
  !std::strcmp(label,"flare_throw")||!std::strcmp(label,"throw_onehanded");
}
inline bool rifleScope(const char* name) {return name&&!std::strcmp(name,"models/weapons/rifle/v_rifle_scope.mdl");}
struct Pickup {int model=0;float time=-9999.f;bool active=false;};
struct Pickups {
 std::array<Pickup,pickupLimit> entries{};
 void clear(){entries={};}
 bool publish(int index,int model,bool active,float time) {
  if(index<=0||index>=pickupLimit||!std::isfinite(time))return false;
  entries[index]={model,time,active&&model>0};return true;
 }
 bool contains(int index,int model,float time)const {
  if(index<=0||index>=pickupLimit||!std::isfinite(time))return false;
  const auto& e=entries[index];return e.active&&e.model==model&&time>=e.time-.15f&&time-e.time<.3f;
 }
};
inline float pickupStrength(const float* eye,const float* forward,const float* target) {
 if(!eye||!forward||!target)return 0.f;
 float delta[3],distance=0,dot=0,forwardLength=0;
 for(int i=0;i<3;++i){if(!std::isfinite(eye[i])||!std::isfinite(target[i])||!std::isfinite(forward[i]))return 0;
  delta[i]=target[i]-eye[i];distance+=delta[i]*delta[i];dot+=delta[i]*forward[i];forwardLength+=forward[i]*forward[i];}
 if(distance<1.f||distance>128.f*128.f||forwardLength<1e-8f)return 0;
 const float alignment=dot/std::sqrt(distance*forwardLength);
 if(alignment<.94f)return 0;
 return std::clamp((alignment-.94f)/.06f,0.f,1.f)*std::clamp((128.f-std::sqrt(distance))/48.f,0.f,1.f);
}
}
