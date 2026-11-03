#pragma once
#include "dx_effect_timing.h"
#include <vector>
#include <algorithm>
struct dx_muzzle_cursor_t {
 int sequence=-1;float anim=-99999,frame=-1,time=-99999;
 float from=0,to=0,end=0,window=0;
 bool allow=false,changed=false,restart=false,looping=false;
 std::vector<int> emitted;
 void begin(int nextSequence,float nextAnim,float now,float nextFrame,float lastFrame,bool loops,float recentWindow) {
  const bool gap=sequence<0||!std::isfinite(now)||now<time||now-time>.25f;
  changed=sequence!=nextSequence;const bool newAnim=std::fabs(nextAnim-anim)>.0001f;
  restart=!changed&&newAnim;
  const bool wrap=!changed&&loops&&frame>=lastFrame*.75f&&nextFrame<=lastFrame*.25f&&nextFrame<frame;
  if(gap||changed||restart||wrap)emitted.clear();
  from=frame;to=nextFrame;end=lastFrame;window=recentWindow;looping=loops;
  allow=!gap&&std::isfinite(nextAnim)&&std::isfinite(nextFrame)&&(!changed||newAnim);
  sequence=nextSequence;anim=nextAnim;frame=nextFrame;time=now;
 }
 bool event(int identity,float eventFrame) {
  if(!allow||std::find(emitted.begin(),emitted.end(),identity)!=emitted.end())return false;
  const bool crossed=(restart&&to>=from)?
    (eventFrame>to-window-.001f&&eventFrame<=to+.001f):
    DX_MuzzleCrossed(from,to,eventFrame,end,looping,changed,restart,window);
  if(!crossed)return false;
  emitted.push_back(identity);return true;
 }
};
inline bool DX_CofAuthoredMuzzle(bool cofViewmodel,bool modelHasMuzzle) {
 return cofViewmodel&&modelHasMuzzle;
}

inline bool DX_MuzzleFallback(bool authoredOnly,bool effect,bool hadEffect,bool spawned) {
 return !authoredOnly&&effect&&!hadEffect&&!spawned;
}
