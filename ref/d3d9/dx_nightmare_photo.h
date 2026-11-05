#pragma once
#define REMIX_ALLOW_X86
#include "vendor/remix_c.h"

static bool R_NightmarePhotoMap(){return g_worldmodel&&!Q_stricmp(g_worldmodel->name,"maps/c_nightmare.bsp");}
namespace nightmare_photo {
struct Echo {int entity=0;float photo=-99999;std::vector<D3DXVECTOR3> vertices;std::vector<uint32_t> indices;};
static std::array<Echo,4> echoes;
static bool ventOpened=false;
// These resources belong to one map lifetime. No mesh extensions on x86.
class Assets {
 friend struct NightmareAssetsTest;
 remixapi_Interface api{};HMODULE module=nullptr;ULONGLONG retry=0;
 remixapi_MaterialHandle material=nullptr;remixapi_LightHandle vent=nullptr;
 struct Mesh {const world_material_batch_t* batch;remixapi_MeshHandle handle;};
 std::vector<Mesh> meshes;uint64_t serial=1;
public:
 void reset(){
  if(module&&module==GetModuleHandleA("d3d9.dll")){
   for(auto& m:meshes)if(api.DestroyMesh)api.DestroyMesh(m.handle);
   if(material&&api.DestroyMaterial)api.DestroyMaterial(material);
   if(vent&&api.DestroyLight)api.DestroyLight(vent);
  }
  meshes.clear();material=nullptr;vent=nullptr;++serial;
 }
 bool ready(){
  HMODULE current=GetModuleHandleA("d3d9.dll");
  if(current!=module){meshes.clear();material=nullptr;vent=nullptr;api={};module=current;retry=0;++serial;}
  if(api.CreateMaterial&&api.CreateMesh&&api.DrawInstance&&api.DestroyMesh&&api.DestroyMaterial&&api.CreateLight&&api.DrawLightInstance&&api.DestroyLight)return true;
  if(!module||GetTickCount64()<retry)return false;retry=GetTickCount64()+1000;
  auto initialize=reinterpret_cast<PFN_remixapi_InitializeLibrary>(GetProcAddress(module,"remixapi_InitializeLibrary"));
  if(!initialize)return false;
  remixapi_InitializeLibraryInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
  info.version=REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR,REMIXAPI_VERSION_MINOR,REMIXAPI_VERSION_PATCH);
  if(initialize(&info,&api)!=REMIXAPI_ERROR_CODE_SUCCESS){api={};return false;}
  return api.CreateMaterial&&api.CreateMesh&&api.DrawInstance&&api.DestroyMesh&&api.DestroyMaterial&&api.CreateLight&&api.DrawLightInstance&&api.DestroyLight;
 }
 bool x(const world_material_batch_t& batch,const D3DMATRIX& world){
  if(!ready()||batch.vertices.empty()||batch.vertices.size()>4096||batch.indices.empty()||batch.indices.size()>12288||batch.indices.size()%3)return false;
  for(auto i:batch.indices)if(i>=batch.vertices.size())return false;
  if(!material){
   wchar_t path[MAX_PATH]{};auto renderer=GetModuleHandleA("ref_d3d9.dll");
   if(!renderer||!GetModuleFileNameW(renderer,path,MAX_PATH))return false;
   std::wstring texture(path);auto slash=texture.find_last_of(L"\\/");if(slash==std::wstring::npos)return false;
   texture.resize(slash+1);texture+=L"CryOfFear\\gfx\\cofr\\nightmare_x.dds";
   if(GetFileAttributesW(texture.c_str())==INVALID_FILE_ATTRIBUTES)return false;
   remixapi_MaterialInfoOpaqueEXT opaque{};opaque.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
   opaque.albedoConstant={1,1,1};opaque.opacityConstant=1;opaque.roughnessConstant=1;
   opaque.alphaTestType=4;opaque.alphaReferenceValue=8; // GREATER: black background has zero coverage.
   remixapi_MaterialInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;info.pNext=&opaque;
   info.hash=0x434f464e58000000ull+serial++;info.albedoTexture=texture.c_str();info.emissiveTexture=texture.c_str();
   info.emissiveIntensity=1.5f;info.emissiveColorConstant={1,1,1};
   if(api.CreateMaterial(&info,&material)!=REMIXAPI_ERROR_CODE_SUCCESS||!material)return false;
  }
  remixapi_MeshHandle mesh=nullptr;for(auto& m:meshes)if(m.batch==&batch){mesh=m.handle;break;}
  if(!mesh){
   if(meshes.size()>=64)return false;
   std::vector<remixapi_HardcodedVertex> vertices(batch.vertices.size());
   for(size_t i=0;i<vertices.size();++i){auto& dst=vertices[i];const auto& src=batch.vertices[i];
    dst.position[0]=src.x;dst.position[1]=src.y;dst.position[2]=src.z;dst.texcoord[0]=src.u;dst.texcoord[1]=src.v;dst.color=0xffffffff;
   }
   for(const auto& range:batch.surfaceRanges){
    if(range.firstIndex>batch.indices.size()||range.indexCount>batch.indices.size()-range.firstIndex)return false;
    for(size_t j=range.firstIndex;j<size_t(range.firstIndex)+range.indexCount;++j)
     for(int a=0;a<3;++a)vertices[batch.indices[j]].normal[a]=range.normal[a];
   }
   std::vector<uint32_t> indices(batch.indices.begin(),batch.indices.end());
   remixapi_MeshInfoSurfaceTriangles surface{};surface.vertices_values=vertices.data();surface.vertices_count=vertices.size();
   surface.indices_values=indices.data();surface.indices_count=indices.size();surface.material=material;
   remixapi_MeshInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MESH_INFO;info.hash=0x434f464e4d000000ull+serial++;
   info.surfaces_values=&surface;info.surfaces_count=1;
   if(api.CreateMesh(&info,&mesh)!=REMIXAPI_ERROR_CODE_SUCCESS||!mesh)return false;
   meshes.push_back({&batch,mesh});
  }
  remixapi_InstanceInfo instance{};instance.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;instance.mesh=mesh;instance.doubleSided=1;
  for(int row=0;row<3;++row)for(int col=0;col<4;++col)instance.transform.matrix[row][col]=world.m[col][row];
  return api.DrawInstance(&instance)==REMIXAPI_ERROR_CODE_SUCCESS;
 }
 bool rectangle(){
  if(!ready())return false;
  if(!vent){
   // *11's authored 48 x 42 lid matches the floor aperture. Keep the
   // emitter in the floor, independently of the rotating lid transform.
   remixapi_LightInfoRectEXT rect{};rect.sType=REMIXAPI_STRUCT_TYPE_LIGHT_INFO_RECT_EXT;
   rect.position={-520,683,-3.5f};rect.xAxis={1,0,0};rect.yAxis={0,1,0};rect.direction={0,0,1};rect.xSize=48;rect.ySize=42;
   remixapi_LightInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_LIGHT_INFO;info.pNext=&rect;info.hash=0x434f464e56000000ull+serial++;
   info.radiance={.12f,.12f,.12f};info.isDynamic=1;
   if(api.CreateLight(&info,&vent)!=REMIXAPI_ERROR_CODE_SUCCESS||!vent)return false;
  }
  return api.DrawLightInstance(vent)==REMIXAPI_ERROR_CODE_SUCCESS;
 }
};
static Assets assets;
}
void R_ResetNightmarePhoto(){nightmare_photo::echoes={};nightmare_photo::ventOpened=false;nightmare_photo::assets.reset();}
bool R_DrawNightmareX(const world_material_batch_t& batch,const cl_entity_s* ent,float time){
 if(!R_NightmarePhotoMap()||!ent||!ent->model||ent->model->type!=mod_brush||
    ent->curstate.effects&EF_NODRAW||!batch.baseTexture||Q_stricmp(batch.baseTexture->name,"c_nightmarex")||dx_remix.value==0.f||
    FBitSet(g_currentViewFlags,RF_DRAW_CUBEMAP|RF_DRAW_OVERVIEW))return false;
 D3DMATRIX world{};R_SetEntityWorldTransform(ent,time,&world);
 return nightmare_photo::assets.x(batch,world);
}
static int R_NightmarePhotoSlot(const cl_entity_s* ent){
 if(!R_NightmarePhotoMap()||!ent||!ent->model||ent->curstate.effects&EF_NODRAW)return -1;
 const float photo=R_CofPhotoTime(),age=R_CurrentRenderTime(0.f)-photo;
 if(!std::isfinite(age)||age<0||age>.4f)return -1;
 int slot=-1;
 const char* names[]={"models/nightmare/1.mdl","models/nightmare/2.mdl","models/nightmare/3.mdl","models/nightmare/4.mdl"};
 for(int i=0;i<4;++i)if(!Q_stricmp(ent->model->name,names[i]))slot=i;
 return slot;
}
void R_QueueNightmarePhotoCpu(const cl_entity_s* ent,const model_drawdata_t& data,const D3DMATRIX& world){
 const int slot=R_NightmarePhotoSlot(ent);if(slot<0)return;
 nightmare_photo::Echo echo;echo.entity=ent->index;echo.photo=R_CofPhotoTime();
 for(const auto& batch:data.renderdata){
  if(echo.vertices.size()+batch.verts.size()>65536||echo.indices.size()+batch.indices.size()>98304)return;
  const uint32_t base=uint32_t(echo.vertices.size());
  for(const auto& source:batch.verts){D3DXVECTOR3 local(source.x,source.y,source.z),point;
   D3DXVec3TransformCoord(&point,&local,reinterpret_cast<const D3DXMATRIX*>(&world));
   if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))return;echo.vertices.push_back(point);
  }
  for(auto i:batch.indices){if(i<0||size_t(i)>=batch.verts.size())return;echo.indices.push_back(base+uint32_t(i));}
 }
 nightmare_photo::echoes[slot]=std::move(echo);
}
void R_QueueNightmarePhoto(const cl_entity_s* ent,const studio_cache_t& cache,const D3DMATRIX& world){
 const int slot=R_NightmarePhotoSlot(ent);
 if(slot<0||!cache.gpuSkinningPoseValid||cache.gpuSkinningBoneCount<=0||cache.gpuSkinningBoneCount>MAXSTUDIOBONES)return;
 const float photo=R_CofPhotoTime();
 nightmare_photo::Echo echo;echo.entity=ent->index;echo.photo=photo;
 for(const auto& batch:cache.batches){
  if(echo.vertices.size()+batch.sources.size()>65536||echo.indices.size()+batch.indices.size()>98304)return;
  const uint32_t base=uint32_t(echo.vertices.size());
  for(const auto& source:batch.sources){
   if(source.bone>=cache.gpuSkinningBoneCount)return;
   const auto& bone=cache.gpuSkinningBoneMatrices[source.bone];D3DXVECTOR3 posed;
   const float local[3]={source.x,source.y,source.z};float* values=&posed.x;
   for(int row=0;row<3;++row){values[row]=bone[row][3];for(int a=0;a<3;++a)values[row]+=local[a]*bone[row][a];}
   D3DXVECTOR3 point;D3DXVec3TransformCoord(&point,&posed,reinterpret_cast<const D3DXMATRIX*>(&world));
   if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))return;
   echo.vertices.push_back(point);
  }
  for(auto i:batch.indices){if(i>=batch.sources.size())return;echo.indices.push_back(base+i);}
 }
 nightmare_photo::echoes[slot]=std::move(echo);
}
static std::vector<cof_pickup::Span> R_NightmarePhotoSpans(){
 std::vector<cof_pickup::Span> spans;D3DXMATRIX vp;D3DXMatrixMultiply(&vp,&g_sceneView,&g_sceneProjection);
 const float now=R_CurrentRenderTime(0.f);
 for(auto& echo:nightmare_photo::echoes){
  const float age=now-echo.photo;
  if(!std::isfinite(age)||age<0||age>=1.25f){echo={};continue;}
  if(echo.vertices.empty())continue;
  D3DXVECTOR3 center(0,0,0);for(const auto& p:echo.vertices)center+=p;center/=float(echo.vertices.size());
  const float target[3]={center.x,center.y,center.z};
  if(g_worldmodel&&g_worldmodel->nodes&&!R_MapLightPositionAllowed(g_vOrigin,target))continue;
  std::vector<cof_pickup::Point> points;points.reserve(echo.vertices.size());
  for(const auto& world:echo.vertices){D3DXVECTOR4 input(world.x,world.y,world.z,1),clip;D3DXVec4Transform(&clip,&input,&vp);cof_pickup::Point point;
   if(std::isfinite(clip.w)&&clip.w>.01f&&clip.z>=0&&clip.z<=clip.w){
    point={float(g_sceneViewport.X)+(clip.x/clip.w+1)*g_sceneViewport.Width*.5f,
     float(g_sceneViewport.Y)+(1-clip.y/clip.w)*g_sceneViewport.Height*.5f,true};
   }points.push_back(point);
  }
  const float fade=std::clamp((1.25f-age)/1.f,0.f,1.f);
  auto outline=cof_pickup::outline(points,echo.indices,float(g_sceneViewport.X),float(g_sceneViewport.Y),float(g_sceneViewport.Width),float(g_sceneViewport.Height),fade*fade,true);
  spans.insert(spans.end(),outline.begin(),outline.end());
 }return spans;
}
static void R_UpdateNightmareVent(){
 if(!R_NightmarePhotoMap())return;
 const auto vent=[](const cl_entity_s* ent){
  if(!ent||!ent->model||ent->model->type!=mod_brush||strcmp(ent->model->name,"*11")||ent->curstate.effects&EF_NODRAW)return false;
  if(fabsf(ent->origin[0]+520)>.1f||fabsf(ent->origin[1]-662)>.1f||fabsf(ent->origin[2]+4)>.1f)return false;
  return fabsf(ent->angles[0])+fabsf(ent->angles[1])+fabsf(ent->angles[2])>2.f;
 };
 bool open=false;for(auto ent:g_opaqueBrushEntities)if(vent(ent))open=true;
 for(auto candidate:g_translucentCandidates)if(vent(candidate))open=true;
 // door5 has wait=-1: once opened it stays open, including outside the PVS.
 if(open)nightmare_photo::ventOpened=true;
 if(nightmare_photo::ventOpened&&dx_remix.value!=0.f)nightmare_photo::assets.rectangle();
}
static void R_DrawNightmarePhoto(){
 if(!device||!g_sceneViewportValid||!R_NightmarePhotoMap()||FBitSet(g_currentViewFlags,RF_DRAW_CUBEMAP|RF_DRAW_OVERVIEW))return;
 R_UpdateNightmareVent();
 auto spans=R_NightmarePhotoSpans();
 dx_cof_light_state_v2 pose{};pose.time=R_CurrentRenderTime(0.f);
 const float flash=dx_cof_camera_flash.value!=0.f&&R_CofCameraFlashPose(pose)?R_CofPhotoFlash():0.f;
 if(flash>0)spans.push_back({float(g_sceneViewport.X),float(g_sceneViewport.Y),float(g_sceneViewport.Width),float(g_sceneViewport.Height),unsigned(65.f*flash)});
 if(spans.empty())return;
 R_Set2DMode(true);R_Flush2DBatch();R_Apply2DNeutralState();DX_BindRawTexture(0,nullptr);
 DX_SetTextureStageStateCached(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);DX_SetTextureStageStateCached(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
 DX_SetTextureStageStateCached(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);DX_SetTextureStageStateCached(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
 DX_SetRenderStateCached(D3DRS_ALPHABLENDENABLE,TRUE);DX_SetRenderStateCached(D3DRS_BLENDOP,D3DBLENDOP_ADD);
 DX_SetRenderStateCached(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);DX_SetRenderStateCached(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);DX_SetRenderStateCached(D3DRS_CULLMODE,D3DCULL_NONE);
 std::vector<vertex2d_t> vertices;vertices.reserve(spans.size()*6);
 for(const auto& s:spans){const DWORD color=D3DCOLOR_ARGB(s.alpha,255,255,255);
  const vertex2d_t a={s.x-.5f,s.y-.5f,0,1,color,0,0},b={s.x+s.width-.5f,s.y-.5f,0,1,color,0,0};
  const vertex2d_t c={s.x-.5f,s.y+s.height-.5f,0,1,color,0,0},d={s.x+s.width-.5f,s.y+s.height-.5f,0,1,color,0,0};vertices.insert(vertices.end(),{a,b,c,b,d,c});
 }
 device->SetFVF(D3DFVF_VERTEX2D);R_DrawScratch(D3DPT_TRIANGLELIST,UINT(vertices.size()/3),UINT(vertices.size()),vertices.data(),sizeof(vertex2d_t));
 R_Set2DMode(false);DX_InvalidateD3DStateCache();DX_InvalidateTextureBindCache();
}
