#pragma once
#include <cmath>
// One accepted native TakePhoto message, independent of held attack/toggle flags.
struct dx_camera_flash_t {
 static constexpr float duration=.10f;
 float started=0,requested=0;bool active=false,pending=false;
 void clear(){*this={};}
 bool trigger(float time){
  if(!std::isfinite(time)){clear();return false;}
  if(active&&time==requested)return false;
  requested=started=time;active=pending=true;return true;
 }
 float fade(float now){
  if(!active)return 0;
  if(!std::isfinite(now)){clear();return 0;}
  if(pending){
   // Network messages arrive before the renderer advances its clock.
   // Wait for the first matching view, then give it a complete exposure.
   if(now<requested){if(requested-now>.25f)clear();return 0;}
   if(now-requested>.25f){clear();return 0;}
   started=now;pending=false;
  }
  if(now<started||now-started>=duration){clear();return 0;}
  // Bright shutter pulse followed by a short discharge tail.
  const float age=now-started;
  return age<=.025f?1.f:1.f-(age-.025f)/(.075f);
 }
};
