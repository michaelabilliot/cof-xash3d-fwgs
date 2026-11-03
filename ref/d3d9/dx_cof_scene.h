#pragma once
#include <cmath>
struct dx_cof_player_visibility_t {
 unsigned reasons=0;float time=0;bool published=false;
 void clear(){*this={};}
 bool publish(unsigned flags,float now){
  if(flags>3||!std::isfinite(now)){clear();return false;}
  reasons=flags;time=now;published=true;return true;
 }
 bool hidden(float now){
  if(!published)return false;
  if(!std::isfinite(now)||now<time||now-time>=.25f){clear();return false;}
  return reasons!=0;
 }
};
