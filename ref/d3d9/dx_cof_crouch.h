#pragma once
#include <cmath>
struct dx_cof_crouch_signal {
 float time=0,height=28;bool valid=false,held=false;
 void clear(){*this={};}
 bool publish(int duck,float eyeHeight,float timestamp){
  if(duck==-1){clear();return true;}
  if((duck!=0&&duck!=1)||!std::isfinite(eyeHeight)||eyeHeight<0||eyeHeight>64||!std::isfinite(timestamp)){clear();return false;}
  held=duck!=0;height=eyeHeight;time=timestamp;valid=true;return true;
 }
 bool sample(float now,bool& duck,float& eyeHeight)const {
  if(!valid||!std::isfinite(now)||now<time-.01f||now-time>=.25f)return false;
  duck=held;eyeHeight=height;return true;
 }
};
