#include "dx_local.h"
#include "dx_gun_smoke.h"
static convar_t dx_cof_gun_smoke={(char*)"dx_cof_gun_smoke",(char*)"1",0};
static cof_smoke::System g_gunSmoke;
static bool g_smokeReadyLogged=false;
static void R_CofGunSmokePreview(){
 if(!gp_cl || !g_worldmodel)return;
 vec3_t origin;
 for(int i=0;i<3;++i)origin[i]=g_vOrigin[i]+g_vForward[i]*18.f+g_vRight[i]*4.f-g_vUp[i]*4.f;
 R_CofGunSmokeShot(origin,g_vForward,R_CurrentRenderTime(0.f));
}
static bool R_CofGunSmokeReady(){
 if(dx_remix.value==0.f||dx_cof_gun_smoke.value==0.f||!R_CofClientPresent())return false;
 if(!g_gunSmoke.ready())return false;
 if(!g_smokeReadyLogged){g_smokeReadyLogged=true;gEngfuncs.Con_Printf("D3D9: CoF gun smoke active (lit alpha, 3D advection, lifetime atlas, 256 puff cap, x86 bridge compatible)\n");}
 return true;
}
void R_CofGunSmokeInit(){
 gEngfuncs.Cvar_RegisterVariable(&dx_cof_gun_smoke);
 gEngfuncs.Cmd_AddCommand("dx_cof_gun_smoke_preview",R_CofGunSmokePreview,"Preview one smoke puff in front of the world camera");
 wchar_t path[MAX_PATH]{};GetModuleFileNameW(nullptr,path,MAX_PATH);
 std::wstring texture(path);const auto slash=texture.find_last_of(L"/\\");
 texture.resize(slash==std::wstring::npos?0:slash+1);texture+=L"CryOfFear\\materials\\cofr\\gun_smoke.dds";
 if(GetFileAttributesW(texture.c_str())!=INVALID_FILE_ATTRIBUTES)g_gunSmoke.texture(texture);
 else gEngfuncs.Con_Printf("D3D9: CoF GPU gun smoke texture missing; original smoke retained\n");
}
void R_CofGunSmokeReset(){g_gunSmoke.reset();g_smokeReadyLogged=false;}
void R_CofGunSmokeShutdown(){gEngfuncs.Cmd_RemoveCommand("dx_cof_gun_smoke_preview");R_CofGunSmokeReset();}
bool R_CofGunSmokeReplace(const char* name){return cof_smoke::legacySprite(name)&&R_CofGunSmokeReady();}
void R_CofGunSmokeShot(const float* origin,const float* forward,float now){
 if(R_CofGunSmokeReady()&&g_gunSmoke.queue(origin,forward,now))
  R_CofLightDebug("smoke queued t=%.4f origin=(%.2f %.2f %.2f)",now,origin[0],origin[1],origin[2]);
}
static bool R_CofSmokeCollision(const float* start,float* end){
 if(!gEngfuncs.CL_TraceLine)return true;
 vec3_t from,to;VectorCopy(start,from);VectorCopy(end,to);
 const auto trace=gEngfuncs.CL_TraceLine(from,to,PM_WORLD_ONLY);
 if(trace.allsolid)return false;
 if(trace.fraction<1.f)for(int i=0;i<3;++i)end[i]=trace.endpos[i]+trace.plane.normal[i]*.3f;
 return true;
}
void R_CofGunSmokeDraw(){
 if(dx_cof_gun_smoke.value==0.f||dx_remix.value==0.f){g_gunSmoke.reset();return;}
 if(!gp_cl||(g_currentViewFlags&(RF_DRAW_CUBEMAP|RF_DRAW_OVERVIEW)))return;
 if(g_gunSmoke.queued()){
  const unsigned drawn=g_gunSmoke.draw(R_CurrentRenderTime(0.f),g_vRight,g_vUp,g_vOrigin,R_CofSmokeCollision);
  if(drawn)R_CofLightDebug("smoke puffs submitted=%u",drawn);
 }
}
