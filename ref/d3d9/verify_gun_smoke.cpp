#include "dx_gun_smoke.h"
#include <cstdio>
#include <stdexcept>
#include <vector>
static unsigned created=0,released=0,meshes=0,meshReleased=0;
static bool failMesh=false;static std::vector<remixapi_InstanceInfo> submitted;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static remixapi_ErrorCode REMIXAPI_CALL material(const remixapi_MaterialInfo* info,remixapi_MaterialHandle* out){
 auto p=static_cast<const remixapi_MaterialInfoOpaqueEXT*>(info->pNext);
 check(info->emissiveIntensity==0&&info->emissiveColorConstant.x==0,"non-emissive smoke");
 check(p->blendType_hasvalue&&p->blendType_value==0&&p->alphaTestType==7,"diffuse alpha blending");
 check(p->opacityConstant>0&&p->opacityConstant<=.2f,"bounded optical density");
 check(info->normalTexture&&info->spriteSheetFps==0,"normal map without shared animation clock");
 *out=reinterpret_cast<remixapi_MaterialHandle>(uintptr_t(++created));return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL mesh(const remixapi_MeshInfo* info,remixapi_MeshHandle* out){
 check(!info->pNext&&info->surfaces_count==1,"bridge-compatible immutable mesh");
 const auto& s=info->surfaces_values[0];check(s.vertices_count==4&&s.indices_count==6,"small atlas quad");
 for(unsigned i=0;i<4;++i)for(unsigned a=0;a<2;++a)check(s.vertices_values[i].texcoord[a]>=0&&s.vertices_values[i].texcoord[a]<=1,"bounded atlas UV");
 if(failMesh){*out=nullptr;return REMIXAPI_ERROR_CODE_SUCCESS;}
 *out=reinterpret_cast<remixapi_MeshHandle>(uintptr_t(++meshes));return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL draw(const remixapi_InstanceInfo* info){
 check(!info->pNext&&info->mesh,"no unsupported particle extension");
 check(!(info->categoryFlags&REMIXAPI_INSTANCE_CATEGORY_BIT_PARTICLE_EMITTER),"no accidental global emitter");
 check(info->categoryFlags&REMIXAPI_INSTANCE_CATEGORY_BIT_PARTICLE,"ray-traced smoke category");
 for(auto& row:info->transform.matrix)for(float value:row)check(std::isfinite(value),"finite world transform");
 submitted.push_back(*info);return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL destroyMesh(remixapi_MeshHandle){++meshReleased;return REMIXAPI_ERROR_CODE_SUCCESS;}
static remixapi_ErrorCode REMIXAPI_CALL destroyMaterial(remixapi_MaterialHandle){++released;return REMIXAPI_ERROR_CODE_SUCCESS;}
static bool solid(const float*,float*){return false;}
int main()try{
 check(cof_smoke::legacySprite("SPRITES\\GUNSMOKE.SPR"),"sprite case/path");
 for(auto name:{"sprites/fog2.spr","sprites/blacksmoke.spr","sprites/muzzleflash1.spr","sprites/gunsmoke.spr.extra"})check(!cof_smoke::legacySprite(name),"map haze isolation");
 remixapi_Interface api{};api.CreateMaterial=material;api.CreateMesh=mesh;api.DrawInstance=draw;api.DestroyMesh=destroyMesh;api.DestroyMaterial=destroyMaterial;
 cof_smoke::System system(api);system.texture(L"fixture.dds");check(system.ready(),"resources initialized");
 const float origin[]={10,20,30},forward[]={2,0,0},invalid[]={NAN,0,0},vertical[]={0,0,1},zero[]={0,0,0};
 check(!system.queue(invalid,forward,1)&&!system.queue(origin,zero,1),"invalid input rejected");
 check(system.queue(origin,forward,1)&&!system.queue(origin,forward,1)&&system.queued()==8,"one bounded shot, no duplicates");
 check(system.draw(1)==0&&system.draw(1.07f)==8&&system.draw(1.07f)==0,"fade-in and repeated-view suppression");
 const auto initial=submitted.back().transform;
 check(system.draw(1.3f)==8,"persistent smoke after the shot");
 check(submitted.back().transform.matrix[0][3]>initial.matrix[0][3],"advected forward in world space");
 check(system.draw(2.5f)==0&&system.queued()==0,"complete finite-life dissipation");
 check(system.queue(origin,vertical,3)&&system.draw(3.1f)==8,"vertical aim and low-FPS simulation");
 check(system.draw(3.2f,nullptr,nullptr,nullptr,solid)==0&&system.queued()==0,"solid collision retires puffs");
 check(system.queue(origin,forward,4)&&system.draw(100)==0&&system.queued()==0,"stalled frames cannot make delayed smoke walls");
 check(system.queue(origin,forward,1)&&system.draw(1.1f)==8,"clock rollback recovery");
 system.reset();check(created==released&&meshes==meshReleased,"all resources released on map reset");
 for(int i=0;i<40;++i)system.queue(origin,forward,10+i*.05f);
 check(system.queued()==256,"strict particle capacity");system.reset();
 failMesh=true;check(!system.ready()&&!system.valid(),"resource failure preserves legacy fallback");system.reset();
 check(created==released&&meshes==meshReleased,"failed initialization cleanup");
 puts("PASS: x86-compatible meshes/materials, non-emissive alpha, density/atlas, world advection, collisions, finite fade, duplicate views/shots, low FPS, capacity, stale/invalid input and cleanup.");return 0;
}catch(const std::exception& e){printf("FAIL: %s\n",e.what());return 1;}
