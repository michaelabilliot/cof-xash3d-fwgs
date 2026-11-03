#pragma once
#include <cmath>
// One accepted native TakePhoto message, independent of held attack/toggle flags.
struct dx_camera_flash_t {
 static constexpr float duration=.10f;
 float started=0;bool active=false;
 void clear(){*this={};}
 bool trigger(float time){
  if(!std::isfinite(time)){clear();return false;}
  if(active&&time==started)return false;
  started=time;active=true;return true;
 }
 float fade(float now){
  if(!active)return 0;
  if(!std::isfinite(now)||now<started||now-started>=duration){clear();return 0;}
  // Bright shutter pulse followed by a short discharge tail.
  const float age=now-started;
  return age<=.025f?1.f:1.f-(age-.025f)/(.075f);
 }
};
