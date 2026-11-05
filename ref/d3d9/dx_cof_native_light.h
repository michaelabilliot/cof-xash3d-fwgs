#pragma once
#include <array>
#include <cmath>
#include <algorithm>
// CoF HUD_AddEntity renderfx 71 (10019020): range=renderamt*8,
// RGB=rendercolor/128, nonzero scale supplies the spotlight field of view.
struct dx_cof_native_light {
 int key=0;
 float time=0,origin[3]{},angles[3]{},range=0,color[3]{},cone=0;
};
static_assert(sizeof(dx_cof_native_light)==52,"CoF native-light V1 ABI");
struct dx_cof_native_lights {
 struct Entry {dx_cof_native_light light{};bool active=false;};
 std::array<Entry,128> entries{};
 void clear(){entries={};}
 static bool valid(const dx_cof_native_light& p){
  if(p.key<=0||p.key>=8192||!std::isfinite(p.time)||!std::isfinite(p.range)||p.range<=0||p.range>8192||
   !std::isfinite(p.cone)||p.cone<0||p.cone>179)return false;
  for(int a=0;a<3;++a)if(!std::isfinite(p.origin[a])||std::abs(p.origin[a])>1000000||
   !std::isfinite(p.angles[a])||!std::isfinite(p.color[a])||p.color[a]<0||p.color[a]>2)return false;
  return p.color[0]+p.color[1]+p.color[2]>0;
 }
 bool publish(const dx_cof_native_light& p){
  if(!valid(p))return false;
  Entry* slot=nullptr;
  for(auto& e:entries){if(e.active&&e.light.key==p.key){slot=&e;break;}
   if(!slot&&(!e.active||p.time<e.light.time||p.time-e.light.time>.15f))slot=&e;}
  if(!slot)return false;
  *slot={p,true};return true;
 }
 template<class Sink> void visit(float now,Sink sink){
  for(auto& e:entries)if(e.active){
   if(!std::isfinite(now)||now<e.light.time-.25f||now-e.light.time>.15f){e.active=false;continue;}
   if(now>=e.light.time)sink(e.light);
  }
 }
};
