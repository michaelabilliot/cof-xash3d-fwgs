#pragma once
#include "dx_cof_light_render.h"
#include "dx_muzzle_policy.h"
#include <string>
static bool verifyCofLighting(const char* modelPath) {
 auto check=[](bool ok,const char* name){if(!ok)std::fprintf(stderr,"FAIL LIGHT V2: %s\n",name);return ok;};
 struct PlayerLight{int player=0;};std::vector<int> order;
 const auto build=[](int player,PlayerLight& light){light.player=player;return true;};
 const auto submit=[&](unsigned slot,const PlayerLight& light){if(slot!=order.size())return false;order.push_back(light.player);return true;};
 if(!check(DX_CofEmitPlayerFlashlights<PlayerLight>(0,3,3,4,false,build,submit)==3&&order==std::vector<int>{3,1,2},"local non-host first, remote priority and exact slot cap"))return false;
 order={3};
 if(!check(DX_CofEmitPlayerFlashlights<PlayerLight>(1,3,3,4,true,build,submit)==3&&order==std::vector<int>{3,1,2},"successful native submission prevents duplicate local cone"))return false;
 order.clear();
 if(!check(DX_CofEmitPlayerFlashlights<PlayerLight>(0,1,3,4,false,build,submit)==1&&order==std::vector<int>{3},"failed native light leaves network fallback available"))return false;
 dx_cof_light_bridge_t bridge;dx_cof_light_state_v2 p;p.time=10;p.direction[0]=1;p.origin[2]=28;
 unsigned submitted=0,disabled=0;D3DLIGHT9 recorded{};
 auto set=[&](unsigned slot,const D3DLIGHT9& light)->long{if(slot!=0)return -1;recorded=light;++submitted;return 0;};
 auto enable=[&](unsigned,bool on)->long{if(!on)++disabled;return 0;};
 for(unsigned flags=1;flags<16;++flags) {
  p.rawFlags=p.eligibleFlags=flags;
  for(unsigned tick=0;tick<3600;++tick) {
   p.time=tick/60.f;D3DLIGHT9 light{};
   if(!check(bridge.publish(&p)&&DX_CofBuildLight(bridge.packet,bridge.flags(p.time),1,light)&&DX_CofSubmit(0,light,set,enable),"60s continuous production publication/build/recording sink"))return false;
   if(!check(recorded.Type==D3DLIGHT_SPOT&&recorded.Range==700&&recorded.Position.z==28&&recorded.Direction.x==1&&recorded.Phi>recorded.Theta&&recorded.Diffuse.r>0,"positive normalized descriptor"))return false;
   const float expectedScale=(flags&8)?1.4f:((flags&4)?.5f:((flags&1)?.46f:.15f));
   const float expectedOuter=((flags&8)||!(flags&4))?50.f:70.f;
   if(!check(std::fabs(light.Diffuse.r-expectedScale)<1e-6f&&std::fabs(light.Phi-expectedOuter*.0174532925199433f)<1e-6f,"handheld8 profile and 8>4>1>2 precedence"))return false;
   // Upstream Remix's quadratic end-distance conversion: discriminant and energy remain positive.
   const float threshold=1.f/255.f,c=light.Attenuation0-light.Diffuse.r/threshold;
   const float discriminant=-4*light.Attenuation2*c;
   if(!check(std::isfinite(discriminant)&&discriminant>0,"Remix quadratic conversion nonzero"))return false;
  }
 }
 D3DLIGHT9 dim{},full{},off{};p.rawFlags=p.eligibleFlags=DX_COF_HANDHELD;
 if(!check(DX_CofBuildLight(p,8,1,full)&&DX_CofBuildLight(p,8,1,dim,.25f,0.f),"handheld brightness builder"))return false;
 if(!check(fabsf(dim.Diffuse.r-.35f)<1e-6f&&fabsf(dim.Diffuse.r/full.Diffuse.r-.25f)<1e-6f&&dim.Position.x==p.origin[0]&&dim.Position.y==p.origin[1],"25percent intensity and exact emitter origin"))return false;
 if(!check(!DX_CofBuildLight(p,8,1,off,0.f),"zero brightness disables handheld"))return false;
 if(!check(DX_CofBuildLight(p,1,1,dim,.25f)&&fabsf(dim.Diffuse.r-.46f)<1e-6f,"other lights unchanged by handheld brightness"))return false;
 if(!check(submitted==15*3600&&disabled==0,"continuous refreshed emissions"))return false;
 if(!check(!DX_CofSubmit(0,recorded,[](unsigned,const D3DLIGHT9&)->long{return -1;},enable)&&disabled==1,"SetLight failure disables"))return false;
 if(!check(!DX_CofSubmit(0,recorded,set,[](unsigned,bool on)->long{return on?-1:0;}),"LightEnable failure"))return false;
 p.rawFlags=p.eligibleFlags=0;bridge.publish(&p);if(!check(!bridge.flags(p.time),"off"))return false;
 p.rawFlags=p.eligibleFlags=15;bridge.publish(&p);if(!check(!bridge.flags(p.time+1)&&!bridge.flags(p.time-1),"stale/rollback"))return false;
 for(unsigned reason:{1u,2u,4u,8u,16u,32u,64u,256u}) {
  p.reasons=reason;p.eligibleFlags=DX_CofEligible(15,reason);bridge.publish(&p);if(!check(!bridge.flags(p.time),"suppression"))return false;
 }
 p.reasons=DX_COF_NO_VIEWMODEL;if(!check(DX_CofEligible(15,p.reasons)==11,"bare hands retain stowed portable light and suppress mounted weapon light"))return false;
 for(unsigned raw:{16u,64u,0x8000u,0x8008u,0xffffu}) {
  p.reasons=0;p.rawFlags=raw;p.eligibleFlags=DX_CofEligible(raw,0);D3DLIGHT9 light{};
  if(!check(bridge.publish(&p)&&bridge.packet.rawFlags==raw&&bridge.flags(p.time)==(raw&15),"raw higher HUD bits retained"))return false;
  if(!check(DX_CofBuildLight(bridge.packet,bridge.flags(p.time),1,light)==bool(raw&15),"unrelated bits do not illuminate"))return false;
 }
 p.reasons=0;p.size=1;if(!check(!bridge.publish(&p)&&!bridge.published,"bad version/size fail closed"))return false;
 p.size=sizeof p;p.direction[0]=std::numeric_limits<float>::quiet_NaN();if(!check(!bridge.publish(&p),"NaN rejection"))return false;
 p.direction[0]=1;bridge.publish(&p);bridge={};if(!check(!bridge.published,"reset"))return false;
 for(int mask=0;mask<16;++mask)if(!check(DX_CofEmissionEnabled(mask&1,mask&2,mask&4,mask&8,8)==(mask==15),"controls gate"))return false;
 if(!check(!DX_CofEmissionEnabled(true,true,true,true,0),"zero slots"))return false;
 // Read actual owned MDLs; no guessed animation labels or event placements.
 const std::string path=modelPath;const auto slash=path.find_last_of("/\\");
 const std::string modelDir=path.substr(0,slash);
 for(const char* relative:{"/weapons/glock/v_glock.mdl","/weapons/rifle/v_rifle_scope.mdl","/weapons/flashlight/v_flashlight.mdl","/weapons/mobile/v_mobile.mdl"}) {
  std::ifstream file(modelDir+relative,std::ios::binary);std::vector<char> bytes(std::istreambuf_iterator<char>(file),{});
  if(!check(bytes.size()>=sizeof(studiohdr_t),"actual model file"))return false;
  auto hdr=reinterpret_cast<const studiohdr_t*>(bytes.data());
  if(!check(hdr->seqindex>=0&&hdr->numseq>0&&size_t(hdr->seqindex)+size_t(hdr->numseq)*sizeof(mstudioseqdesc_t)<=bytes.size(),"sequence bounds"))return false;
  auto seqs=reinterpret_cast<const mstudioseqdesc_t*>(bytes.data()+hdr->seqindex);bool hasMuzzle=false;
  for(int seq=0;seq<hdr->numseq;++seq){const auto& d=seqs[seq];
   if(!check(d.numevents>=0&&d.eventindex>=0&&size_t(d.eventindex)+size_t(d.numevents)*sizeof(mstudioevent_t)<=bytes.size(),"event bounds"))return false;
   const auto events=reinterpret_cast<const mstudioevent_t*>(bytes.data()+d.eventindex);
   for(int i=0;i<d.numevents;++i)if(events[i].event==5001||events[i].event==5011||events[i].event==5021||events[i].event==5031)hasMuzzle=true;
  }
  const bool authored=DX_CofAuthoredMuzzle(true,hasMuzzle);
  for(int seq=0;seq<hdr->numseq;++seq){const auto& d=seqs[seq];const auto events=reinterpret_cast<const mstudioevent_t*>(bytes.data()+d.eventindex);
   dx_muzzle_cursor_t cursor;cursor.begin(9999,0,0,0,100,false,1);
   cursor.begin(seq,.016f,.016f,0,float(d.numframes-1),(d.flags&STUDIO_LOOPING)!=0,d.fps/60.f);
   unsigned count=0,expected=0;
   for(int i=0;i<d.numevents;++i){const bool muzzle=events[i].event==5001||events[i].event==5011||events[i].event==5021||events[i].event==5031;
    if(muzzle&&events[i].frame==0){++expected;if(cursor.event(i,float(events[i].frame)))++count;}}
   if(!check(count==expected,"actual frame-zero firing accepted once"))return false;
   cursor.begin(seq,.016f,.016f,0,float(d.numframes-1),(d.flags&STUDIO_LOOPING)!=0,d.fps/60.f);
   for(int i=0;i<d.numevents;++i)if(events[i].event==5001&&events[i].frame==0)if(!check(!cursor.event(i,0),"duplicate pass"))return false;
   const bool nonFiring=strstr(d.label,"draw")||strstr(d.label,"holster")||strstr(d.label,"reload");
   if(authored&&nonFiring)if(!check(!count&&!DX_MuzzleFallback(authored,true,false,false),"draw/holster/reload EF produces no flash"))return false;
   if(expected){cursor.begin(seq,.032f,.032f,0,float(d.numframes-1),false,d.fps/60.f);
    if(!check(cursor.event(0,0),"same-sequence rapid shot"))return false;
    cursor.begin(seq,.032f,1.f,0,float(d.numframes-1),false,1);if(!check(!cursor.event(0,0),"visibility gap primes"))return false;
    cursor={};cursor.begin(seq,.032f,1.f,0,float(d.numframes-1),false,1);if(!check(!cursor.event(0,0),"model replacement primes"))return false;
   }
  }
 }
 if(!check(DX_MuzzleFallback(false,true,false,false)&&!DX_MuzzleFallback(false,true,true,false),"unmarked/generic fallback retained"))return false;
 dx_muzzle_cursor_t loop;loop.begin(1,0,0,29,30,true,1);loop.begin(1,0,.016f,0,30,true,1);
 if(!check(loop.event(0,0)&&!loop.event(0,0),"loop crossing deduplicated"))return false;
 std::puts("PASS: V2 publication/build/recording light sink (15 flags x 60s), failed submissions, eligibility/expiry/reset, positive Remix conversion; actual Glock/scope/phone/flashlight model event policies.");
 return true;
}
