#pragma once
#define REMIX_ALLOW_X86
#include "vendor/remix_c.h"
#include <windows.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <string>
namespace cof_smoke {
inline bool legacySprite(const char* name) {
 if(!name)return false;const char* target="sprites/gunsmoke.spr";
 for(;*name&&*target;++name,++target){char c=*name;if(c=='\\')c='/';if(c>='A'&&c<='Z')c+=32;if(c!=*target)return false;}
 return !*name&&!*target;
}
// Immutable atlas quads with explicit alpha materials work through the x86
// bridge without relying on native particle-system extension marshalling.
class System {
 static constexpr unsigned frames=12,levels=16,capacity=256;
 struct Puff {float origin[3]{},velocity[3]{},position[3]{},birth=0,life=1,phase=0,rotation=0;bool active=false;};
 remixapi_Interface api{};HMODULE module=nullptr;bool injected=false;ULONGLONG retryAt=0;
 uint64_t generation=0;unsigned seed=0x62736431;std::wstring texturePath;
 std::array<remixapi_MaterialHandle,levels> materials{};
 std::array<remixapi_MeshHandle,frames*levels> meshes{};std::array<Puff,capacity> puffs{};
 float lastTime=-100,lastDraw=-100;std::array<float,3> lastOrigin{};
 float random(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return float(seed&0xffffff)/float(0x1000000);}
public:
 using Collision=bool(*)(const float*,float*);
 System()=default;explicit System(const remixapi_Interface& sink):api(sink),injected(true){}
 void texture(const std::wstring& path){texturePath=path;}
 bool valid()const{return meshes.back()&&materials.back();}
 unsigned queued()const{unsigned n=0;for(const auto& p:puffs)n+=p.active;return n;}
 bool ready(){
  if(!injected){
   auto loaded=GetModuleHandleA("d3d9.dll");
   if(loaded!=module){api={};module=loaded;meshes.fill(nullptr);materials.fill(nullptr);retryAt=0;for(auto& p:puffs)p.active=false;}
   if(!api.CreateMaterial){
    if(!module||GetTickCount64()<retryAt)return false;retryAt=GetTickCount64()+1000;
    auto init=reinterpret_cast<PFN_remixapi_InitializeLibrary>(GetProcAddress(module,"remixapi_InitializeLibrary"));if(!init)return false;
    remixapi_InitializeLibraryInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
    info.version=REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR,REMIXAPI_VERSION_MINOR,REMIXAPI_VERSION_PATCH);
    if(init(&info,&api)!=REMIXAPI_ERROR_CODE_SUCCESS){api={};return false;}
   }
  }
  if(!api.CreateMaterial||!api.CreateMesh||!api.DrawInstance||!api.DestroyMesh||!api.DestroyMaterial)return false;
  if(valid())return true;if(texturePath.empty())return false;
  const auto normal=texturePath.substr(0,texturePath.size()-4)+L"_normal.dds";++generation;
  for(unsigned l=0;l<levels;++l){
   remixapi_MaterialInfoOpaqueEXT opaque{};opaque.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
   opaque.albedoConstant={.65f,.65f,.65f};opaque.opacityConstant=.2f*float(l+1)/levels;
   opaque.roughnessConstant=1;opaque.blendType_hasvalue=1;opaque.blendType_value=0;opaque.alphaTestType=7;
   remixapi_MaterialInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;
   info.pNext=&opaque;info.hash=0x434f46534d4b0000ull+generation*256+l;
   info.albedoTexture=texturePath.c_str();info.normalTexture=normal.c_str();info.filterMode=1;
   if(api.CreateMaterial(&info,&materials[l])!=REMIXAPI_ERROR_CODE_SUCCESS||!materials[l]){reset();return false;}
   for(unsigned frame=0;frame<frames;++frame){
    remixapi_HardcodedVertex v[4]{};
    for(unsigned i=0;i<4;++i){const unsigned x=i==1||i==2,y=i>=2;
     v[i].position[0]=float(x)-.5f;v[i].position[1]=.5f-float(y);v[i].normal[2]=1;v[i].color=0xffffffff;
     v[i].texcoord[0]=(float(frame%4)+float(x))*.25f;v[i].texcoord[1]=(float(frame/4)+float(y))*.25f;}
    const uint32_t indices[]={0,1,2,0,2,3};remixapi_MeshInfoSurfaceTriangles surface{};
    surface.vertices_values=v;surface.vertices_count=4;surface.indices_values=indices;surface.indices_count=6;surface.material=materials[l];
    remixapi_MeshInfo geometry{};geometry.sType=REMIXAPI_STRUCT_TYPE_MESH_INFO;
    geometry.hash=0x434f46534d450000ull+generation*4096+l*frames+frame;geometry.surfaces_values=&surface;geometry.surfaces_count=1;
    auto& mesh=meshes[l*frames+frame];
    if(api.CreateMesh(&geometry,&mesh)!=REMIXAPI_ERROR_CODE_SUCCESS||!mesh){reset();return false;}
   }
  }return true;
 }
 bool queue(const float* origin,const float* forward,float now){
  if(!origin||!forward||!std::isfinite(now))return false;float length=0,dist=0;
  for(int i=0;i<3;++i){if(!std::isfinite(origin[i])||!std::isfinite(forward[i]))return false;length+=forward[i]*forward[i];dist+=(origin[i]-lastOrigin[i])*(origin[i]-lastOrigin[i]);}
  if(length<1e-8f||!std::isfinite(length))return false;
  if(now<lastTime){for(auto& p:puffs)p.active=false;lastDraw=-100;}
  if(now>=lastTime&&now-lastTime<.035f&&dist<1)return false;
  if(queued()>capacity-8)return false;
  for(int i=0;i<3;++i)lastOrigin[i]=origin[i];lastTime=now;unsigned spawned=0;
  for(auto& p:puffs)if(!p.active){p=Puff{};p.active=true;p.birth=now+float(spawned)*.008f;p.life=.85f+random()*.35f;
   p.phase=random()*6.2831853f;p.rotation=random()*6.2831853f;const float speed=24+random()*14;
   for(int i=0;i<3;++i){p.origin[i]=origin[i]+forward[i]/sqrtf(length)*1.5f+(random()-.5f)*.5f;p.position[i]=p.origin[i];p.velocity[i]=forward[i]/sqrtf(length)*speed+(random()-.5f)*8;}
   if(++spawned==8)break;}
  return true;
 }
 unsigned draw(float now,const float* right=nullptr,const float* up=nullptr,const float* camera=nullptr,Collision collide=nullptr){
  if(!std::isfinite(now)||now==lastDraw)return 0;
  if(!ready()){for(auto& p:puffs)p.active=false;return 0;}lastDraw=now;
  const float defaultRight[]={1,0,0},defaultUp[]={0,1,0};if(!right)right=defaultRight;if(!up)up=defaultUp;unsigned drawn=0;
  for(auto& p:puffs)if(p.active){float age=now-p.birth;
   if(age<-.1f||age>p.life){p.active=false;continue;}if(age<0)continue;
   const float t=age/p.life,travel=(1-expf(-3.5f*age))/3.5f;
   float position[3];for(int i=0;i<3;++i)position[i]=p.origin[i]+p.velocity[i]*travel+(sinf(age*5+p.phase+i*2)-sinf(p.phase+i*2))*age*1.4f;
   position[2]+=3.5f*age*age;
   if(collide&&!collide(p.position,position)){p.active=false;continue;}
   for(int i=0;i<3;++i)p.position[i]=position[i];
   float opacity=.2f*std::min(1.f,age/.045f)*powf(1-t,1.4f);
   if(camera){float d=0;for(int i=0;i<3;++i)d+=(position[i]-camera[i])*(position[i]-camera[i]);opacity*=std::max(0.f,std::min(1.f,(sqrtf(d)-2)/4));}
   const int level=std::min(int(levels)-1,int(opacity*levels/.2f)-1);if(level<0)continue;
   const unsigned frame=std::min(frames-1,unsigned(t*frames));const float size=1.8f+6.2f*t,angle=p.rotation+age*.65f,cs=cosf(angle),sn=sinf(angle);
   remixapi_InstanceInfo instance{};instance.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;instance.mesh=meshes[level*frames+frame];instance.doubleSided=1;
   instance.categoryFlags=REMIXAPI_INSTANCE_CATEGORY_BIT_PARTICLE|REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_ANTI_CULLING|REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_BAKED_LIGHTING;
   for(int i=0;i<3;++i){const float r=right[i]*cs+up[i]*sn,u=up[i]*cs-right[i]*sn;instance.transform.matrix[i][0]=r*size;instance.transform.matrix[i][1]=u*size;instance.transform.matrix[i][3]=position[i];}
   instance.transform.matrix[0][2]=right[1]*up[2]-right[2]*up[1];instance.transform.matrix[1][2]=right[2]*up[0]-right[0]*up[2];instance.transform.matrix[2][2]=right[0]*up[1]-right[1]*up[0];
   if(api.DrawInstance(&instance)==REMIXAPI_ERROR_CODE_SUCCESS)++drawn;
  }return drawn;
 }
 void reset(){
  if(injected||module==GetModuleHandleA("d3d9.dll")){
   if(api.DestroyMesh)for(auto h:meshes)if(h)api.DestroyMesh(h);
   if(api.DestroyMaterial)for(auto h:materials)if(h)api.DestroyMaterial(h);}
  meshes.fill(nullptr);materials.fill(nullptr);for(auto& p:puffs)p.active=false;lastTime=lastDraw=-100;
 }
};
}
void R_CofGunSmokeInit();
void R_CofGunSmokeShutdown();
void R_CofGunSmokeReset();
void R_CofGunSmokeDraw();
bool R_CofGunSmokeReplace(const char* modelName);
void R_CofGunSmokeShot(const float* origin,const float* forward,float now);
