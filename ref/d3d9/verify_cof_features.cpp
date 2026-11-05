#include "dx_local.h"
#include "dx_cof_dual.h"
#include <dbghelp.h>
#include <fstream>
#include <stdexcept>
#include <limits>
#include "dx_cof_features.h"
#include "dx_cof_pickup_outline.h"
#include "dx_cof_light.h"
#include "dx_cof_shadow.h"
#include "dx_cof_shadow_motion.h"
#include "ref_params.h"
#include "q_client.h"
#include "progdefs.h"
#include "eiface.h"
#include "edict.h"
static HANDLE process=GetCurrentProcess();
static DWORD64 moduleBase;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class T> static T symbol(const char* name){
 alignas(SYMBOL_INFO) char bytes[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};
 auto info=reinterpret_cast<SYMBOL_INFO*>(bytes);info->SizeOfStruct=sizeof(SYMBOL_INFO);info->MaxNameLen=MAX_SYM_NAME;
 require(SymFromName(process,name,info)&&info->ModBase==moduleBase,name);
 return reinterpret_cast<T>(static_cast<uintptr_t>(info->Address));
}
static cl_entity_t viewmodel{};static unsigned flareIgnitions=0,flareThrows=0;
static ref_client_t client{};static ref_host_t host{};static cvar_t shared{};
static std::vector<char> bytes,donorBytes,shadowVmBytes;static model_t donorModel{};static model_t* shadowVmModel=nullptr;
static model_t secondaryVmModel{};static std::vector<char> secondaryVmBytes;
static int testHealth=100,testThirdPerson=0,testWater=0;
static intptr_t parm(int key,int){if(key==PARM_GET_CLIENT_PTR)return reinterpret_cast<intptr_t>(&client);if(key==PARM_GET_HOST_PTR)return reinterpret_cast<intptr_t>(&host);if(key==PARM_GET_VIEWENT_PTR)return reinterpret_cast<intptr_t>(&viewmodel);if(key==PARM_LOCAL_HEALTH)return testHealth;if(key==PARM_THIRDPERSON)return testThirdPerson;if(key==PARM_WATER_LEVEL)return testWater;return 0;}
static void reg(convar_t* value){value->value=static_cast<float>(atof(value->string));}
static cvar_t* cvar(const char*){return &shared;}
static void* extra(int,model_t* model){if(model==&secondaryVmModel)return secondaryVmBytes.empty()?nullptr:secondaryVmBytes.data();if(model==shadowVmModel)return shadowVmBytes.empty()?nullptr:shadowVmBytes.data();return model==&donorModel?donorBytes.data():bytes.data();}

struct Buffer {void** methods;std::vector<unsigned char> bytes;};
struct Device {void** methods;};
struct Vertex {float x,y,z,rhw;DWORD color;float u,v;};
struct Draw {DWORD state[210];std::vector<Vertex> vertices;};
static DWORD states[210]{};static void* vbMethods[14]{};static std::vector<std::unique_ptr<Buffer>> buffers;static Buffer* bound;static UINT step,streamOffset;static std::vector<Draw> draws;
static ULONG STDMETHODCALLTYPE release(Buffer*){return 0;}
static HRESULT STDMETHODCALLTYPE lock(Buffer* b,UINT o,UINT n,void** out,DWORD){require(o<=b->bytes.size()&&n<=b->bytes.size()-o,"buffer range");*out=b->bytes.data()+o;return S_OK;}
static HRESULT STDMETHODCALLTYPE unlock(Buffer*){return S_OK;}
static HRESULT STDMETHODCALLTYPE createVB(Device*,UINT n,DWORD,DWORD,D3DPOOL,IDirect3DVertexBuffer9** out,HANDLE*){auto b=std::make_unique<Buffer>();b->methods=vbMethods;b->bytes.resize(n);*out=reinterpret_cast<IDirect3DVertexBuffer9*>(b.get());buffers.push_back(std::move(b));return S_OK;}
static HRESULT STDMETHODCALLTYPE renderState(Device*,D3DRENDERSTATETYPE s,DWORD v){require(s<210,"state range");states[s]=v;return S_OK;}
static HRESULT STDMETHODCALLTYPE stage(Device*,DWORD,D3DTEXTURESTAGESTATETYPE,DWORD){return S_OK;}
static HRESULT STDMETHODCALLTYPE sampler(Device*,DWORD,D3DSAMPLERSTATETYPE,DWORD){return S_OK;}
static HRESULT STDMETHODCALLTYPE texture(Device*,DWORD,IDirect3DBaseTexture9*){return S_OK;}
static HRESULT STDMETHODCALLTYPE transform(Device*,D3DTRANSFORMSTATETYPE,const D3DMATRIX*){return S_OK;}
static HRESULT STDMETHODCALLTYPE fvf(Device*,DWORD){return S_OK;}
static HRESULT STDMETHODCALLTYPE stream(Device*,UINT,IDirect3DVertexBuffer9* vb,UINT offset,UINT stride){bound=reinterpret_cast<Buffer*>(vb);step=stride;streamOffset=offset;return S_OK;}
static HRESULT STDMETHODCALLTYPE draw(Device*,D3DPRIMITIVETYPE type,UINT start,UINT count){
 const UINT n=type==D3DPT_TRIANGLELIST?count*3:count+2;require(bound&&step==sizeof(Vertex),"2D vertex ABI");const size_t o=streamOffset+size_t(start)*step;require(o+n*step<=bound->bytes.size(),"draw bounds");
 Draw d{};memcpy(d.state,states,sizeof states);const auto p=reinterpret_cast<const Vertex*>(bound->bytes.data()+o);d.vertices.assign(p,p+n);draws.push_back(std::move(d));return S_OK;
}
static void studioEvent(const mstudioevent_t* event,const cl_entity_t* ent) {
 for(int i=0;i<3;++i)require(std::isfinite(ent->attachment[0][i]),"native flare attachment");
 if(event->event==5050)++flareIgnitions;else if(event->event==5051)++flareThrows;else require(false,"unrelated event dispatched");
}
static bool occluded=false;
static pmtrace_t traceLine(float*,float*,int){pmtrace_t trace{};trace.fraction=occluded?.1f:1.f;return trace;}
static float triangleAlpha(const Vertex& a,const Vertex& b,const Vertex& c,float x,float y) {
 const float denominator=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);
 if(std::fabs(denominator)<1e-6f)return -1;
 const float u=((b.y-c.y)*(x-c.x)+(c.x-b.x)*(y-c.y))/denominator;
 const float v=((c.y-a.y)*(x-c.x)+(a.x-c.x)*(y-c.y))/denominator;
 const float t=1-u-v;if(u<-.00001f||v<-.00001f||t<-.00001f)return -1;
 return u*float(a.color>>24)+v*float(b.color>>24)+t*float(c.color>>24);
}
static unsigned shadowCreates=0,shadowDraws=0,shadowDestroys=0;static uint64_t shadowHash=0;
static std::vector<remixapi_HardcodedVertex> shadowVertices,shadowSkinnedVertices;
static std::vector<uint32_t> shadowBoneIndices;
static std::vector<uint32_t> shadowSurfaceIndices;
static std::vector<remixapi_Transform> shadowPose;
static remixapi_InstanceInfo shadowInstance{};
static bool rejectShadowMesh=false;static unsigned shadowErrors=0;
static void shadowConsole(const char* text,...){if(strstr(text,"player shadow disabled"))++shadowErrors;}
static remixapi_ErrorCode REMIXAPI_CALL shadowConfig(const char* key,const char* value) {
 require((!strcmp(key,"rtx.playerModel.enableInPrimarySpace")&&!strcmp(value,"False"))||
  (!strcmp(key,"rtx.playerModel.enablePrimaryShadows")&&!strcmp(value,"True")),"shadow visibility defaults");return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL shadowMaterial(const remixapi_MaterialInfo* info,remixapi_MaterialHandle* handle){
 require(info&&info->pNext,"explicit opaque shadow material");const auto& opaque=*static_cast<const remixapi_MaterialInfoOpaqueEXT*>(info->pNext);
 require(opaque.sType==REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT&&opaque.alphaTestType==7,"opaque material must ALWAYS pass alpha, zero NEVER makes all geometry invisible");
 require(!opaque.useDrawCallAlphaState&&!opaque.blendType_hasvalue&&opaque.opacityConstant==1.f,"shadow material is independently fully opaque");
 *handle=reinterpret_cast<remixapi_MaterialHandle>(1);return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL shadowDestroyMaterial(remixapi_MaterialHandle){return REMIXAPI_ERROR_CODE_SUCCESS;}
static remixapi_ErrorCode REMIXAPI_CALL shadowCreate(const remixapi_MeshInfo* info,remixapi_MeshHandle* handle) {
 require(info&&info->surfaces_count==1,"single shadow mesh surface");
 require(info->pNext==nullptr,"installed x86 bridge rejects ALL mesh extensions; refresh would hang");
 if(rejectShadowMesh){++shadowCreates;*handle=nullptr;return REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS;}
 const auto& s=info->surfaces_values[0];
 shadowSurfaceIndices.assign(s.indices_values,s.indices_values+s.indices_count);
 require(s.skinning_hasvalue&&s.skinning_value.bonesPerVertex==1,"single bone influence per MDL vertex");
 require(s.skinning_value.blendIndices_count==s.vertices_count&&s.skinning_value.blendWeights_count==s.vertices_count,"skinning arrays cover all vertices");
 for(size_t i=0;i<s.vertices_count;++i)require(s.skinning_value.blendWeights_values[i]==1.f,"rigid MDL bone weight");
 shadowBoneIndices.assign(s.skinning_value.blendIndices_values,s.skinning_value.blendIndices_values+s.skinning_value.blendIndices_count);
 require(s.vertices_count>100&&s.indices_count>300&&s.indices_count%3==0,"real police geometry");
 for(size_t i=0;i<s.indices_count;++i)require(s.indices_values[i]<s.vertices_count,"shadow index bounds");
 for(size_t i=0;i<s.vertices_count;++i)for(float v:s.vertices_values[i].position)require(std::isfinite(v),"finite shadow geometry");

 shadowVertices.assign(s.vertices_values,s.vertices_values+s.vertices_count);shadowHash=info->hash;++shadowCreates;*handle=reinterpret_cast<remixapi_MeshHandle>(2);return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL shadowDestroy(remixapi_MeshHandle){++shadowDestroys;return REMIXAPI_ERROR_CODE_SUCCESS;}
static remixapi_ErrorCode REMIXAPI_CALL shadowDraw(const remixapi_InstanceInfo* info){
 require(info&&info->pNext,"animated instance carries bones");
 auto pose=static_cast<const remixapi_InstanceInfoBoneTransformsEXT*>(info->pNext);
 require(pose->sType==REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BONE_TRANSFORMS_EXT&&!pose->pNext,"only bridge-supported bone-transform extension");
 require(pose->boneTransforms_values&&pose->boneTransforms_count>0&&pose->boneTransforms_count<=128,"bounded bone palette");
 shadowPose.assign(pose->boneTransforms_values,pose->boneTransforms_values+pose->boneTransforms_count);
 shadowSkinnedVertices=shadowVertices;
 for(size_t i=0;i<shadowVertices.size();++i){require(shadowBoneIndices[i]<pose->boneTransforms_count,"palette covers referenced bone");
  const auto& bone=pose->boneTransforms_values[shadowBoneIndices[i]];
  for(int axis=0;axis<3;++axis){float value=bone.matrix[axis][3];for(int j=0;j<3;++j)value+=bone.matrix[axis][j]*shadowVertices[i].position[j];
   require(std::isfinite(value),"finite animated vertex");shadowSkinnedVertices[i].position[axis]=value;}}
 shadowInstance=*info;++shadowDraws;return REMIXAPI_ERROR_CODE_SUCCESS;
}
struct CofShadowMeshTestAccess {
 static void bind(CofShadowMesh& mesh) {
  mesh.module=GetModuleHandleA("d3d9.dll");mesh.api={};mesh.api.CreateMesh=shadowCreate;mesh.api.DrawInstance=shadowDraw;mesh.api.DestroyMesh=shadowDestroy;
  mesh.api.SetConfigVariable=shadowConfig;mesh.api.CreateMaterial=shadowMaterial;mesh.api.DestroyMaterial=shadowDestroyMaterial;
 }
};
static const char* pickupName="item_syringe";static bool nativePackAccepts=true;
static int nativePack(entity_state_t* state,int,edict_t*,edict_t*,int,int,unsigned char*) {
 memset(state,0x5a,sizeof(*state));return nativePackAccepts?1:0;
}
static entvars_t* varsForPickup(edict_t* e){return &e->v;}
static const char* nameForPickup(int index){return index==2?"models/weapons/flare/w_flare.mdl":pickupName;}
int main(int argc,char** argv)try {
 require(argc>=4&&argc<=6,"renderer, game-folder, server-DLL, optional pose capture and client-DLL arguments");auto dll=LoadLibraryA(argv[1]);require(dll!=nullptr,"load renderer");
 SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_EXACT_SYMBOLS);require(SymInitialize(process,nullptr,FALSE)!=FALSE,"symbols");
 const std::string path=argv[1];SymSetSearchPath(process,path.substr(0,path.find_last_of("/\\")).c_str());
 moduleBase=SymLoadModuleEx(process,nullptr,argv[1],nullptr,reinterpret_cast<DWORD64>(dll),0,nullptr,0);require(moduleBase!=0,"matching PDB");
 using API=int(*)(int,ref_interface_t*,ref_api_t*,ref_globals_t*);auto api=reinterpret_cast<API>(GetProcAddress(dll,"GetRefAPI"));
 ref_api_t engine{};ref_interface_t functions{};ref_globals_t globals{};
 engine.EngineGetParm=parm;engine.Cvar_RegisterVariable=reg;engine.pfnGetCvarPointer=cvar;engine.Mod_Extradata=extra;engine.pfnStudioEvent=studioEvent;engine.CL_TraceLine=traceLine;engine.Con_Printf=shadowConsole;
 require(api&&api(REF_API_VERSION,&functions,&engine,&globals)==REF_API_VERSION,"API");
 // Only publish the existing CoF marker; no engine or native game is started.
 using LightPublish=int(__cdecl*)(const dx_cof_light_state_v2*);
 auto lightPublish=reinterpret_cast<LightPublish>(GetProcAddress(dll,"DX_CofLightStateV2"));
 dx_cof_light_state_v2 packet;packet.direction[0]=1;packet.time=10;client.time=10;client.oldtime=9.98;client.viewentity=1;client.playernum=0;
 require(lightPublish&&lightPublish(&packet),"CoF identity");
 std::ifstream flare(std::string(argv[2])+"/models/weapons/flare/v_flare.mdl",std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(flare),{});
 require(bytes.size()>sizeof(studiohdr_t),"flare model");auto hdr=reinterpret_cast<studiohdr_t*>(bytes.data());
 model_t flareModel{};flareModel.type=mod_studio;strcpy_s(flareModel.name,"models/weapons/flare/v_flare.mdl");
 cl_entity_t flareEntity{};flareEntity.model=&flareModel;flareEntity.curstate.framerate=1;
 auto dispatch=symbol<void(*)(studiohdr_t*,cl_entity_t*,float,const D3DMATRIX&,float)>("R_ProcessCofFlareEvents");
 symbol<void(*)()>("R_RegisterModelCvars")();symbol<convar_t*>("dx_muzzleflash_sprite")->value=0;
 D3DMATRIX world{};world._11=world._22=world._33=world._44=1;
 auto seqs=reinterpret_cast<mstudioseqdesc_t*>(bytes.data()+hdr->seqindex);
 float now=10;
 for(int seq:{2,3,7,2,8}) {
  flareEntity.curstate.sequence=seq;flareEntity.curstate.animtime=now;const auto& desc=seqs[seq];
  const int samples=std::max(2,int(float(desc.numframes)*60.f/desc.fps)+1);
  for(int tick=0;tick<samples;++tick) {
   const float nativeFrame=std::min(float(desc.numframes-1),float(tick)*desc.fps/60.f);
   const float frame=nativeFrame*256.f/float(std::max(1,desc.numframes-1));
   client.oldtime=client.time;client.time=now;
   dispatch(hdr,&flareEntity,frame,world,now);dispatch(hdr,&flareEntity,frame,world,now);now+=1.f/60.f;
  }
 }
 require(flareIgnitions==1&&flareThrows==2,"5050/5051 once each across both throw sequences, despite duplicate passes and disabled muzzle effects");
 const unsigned throws=flareThrows;now+=1;client.oldtime=client.time;client.time=now;dispatch(hdr,&flareEntity,256,world,now);
 require(flareThrows==throws,"late visibility must not replay throw");
 puts("PASS: production flare-event dispatcher, actual MDL ignition and both throws, 60Hz duplicate passes, muzzle effects disabled, attachment updates, visibility-gap replay suppression.");
 // Recording COM sink; no D3D device, window or graphics runtime is created.
 void* methods[119]{};methods[26]=reinterpret_cast<void*>(createVB);methods[44]=reinterpret_cast<void*>(transform);methods[57]=reinterpret_cast<void*>(renderState);methods[65]=reinterpret_cast<void*>(texture);methods[67]=reinterpret_cast<void*>(stage);methods[69]=reinterpret_cast<void*>(sampler);methods[89]=reinterpret_cast<void*>(fvf);methods[100]=reinterpret_cast<void*>(stream);methods[81]=reinterpret_cast<void*>(draw);
 vbMethods[2]=reinterpret_cast<void*>(release);vbMethods[11]=reinterpret_cast<void*>(lock);vbMethods[12]=reinterpret_cast<void*>(unlock);
 Device recorder{methods};*symbol<IDirect3DDevice9**>("device")=reinterpret_cast<IDirect3DDevice9*>(&recorder);
 model_t scope{};scope.type=mod_studio;strcpy_s(scope.name,"models/weapons/rifle/v_rifle_scope.mdl");viewmodel.model=&scope;
 auto overlay=symbol<void(*)()>("R_DrawCofScopeOverlay");auto active=symbol<bool(*)()>("R_CofRifleScopeActive");require(active(),"native scoped model activates overlay");
 *symbol<bool*>("g_sceneViewportValid")=true;
 for(const auto dims:{std::array<int,2>{1920,1080},std::array<int,2>{1280,1024},std::array<int,2>{3440,1440}}) {
  auto vp=symbol<D3DVIEWPORT9*>("g_sceneViewport");*vp={0,0,DWORD(dims[0]),DWORD(dims[1]),0,1};draws.clear();overlay();
  require(draws.size()==1,"scope one batched draw");const auto& d=draws.back();
  require(d.state[D3DRS_ZENABLE]==FALSE&&d.state[D3DRS_SRCBLEND]==D3DBLEND_SRCALPHA&&d.state[D3DRS_DESTBLEND]==D3DBLEND_INVSRCALPHA,"scope neutral 2D state");
  const auto alpha=[&](float x,float y){float result=-1;for(size_t i=0;i+2<d.vertices.size();i+=3)result=std::max(result,triangleAlpha(d.vertices[i],d.vertices[i+1],d.vertices[i+2],x,y));return result;};
  const float w=float(dims[0]),h=float(dims[1]);
  require(alpha(2,2)>254&&alpha(w-2,h-2)>254&&alpha(w*.5f,2)>254,"black scope exterior");
  require(alpha(w*.5f+25,h*.5f+25)<0,"clear untinted scope aperture");require(alpha(w*.5f+.1f,h*.5f+.1f)>254,"black scope reticle");
 }
 strcpy_s(scope.name,"models/weapons/rifle/v_rifle.mdl");require(!active(),"unscoped rifle unchanged");draws.clear();overlay();require(draws.empty(),"scope hidden when unscoped");
 puts("PASS: production scope overlay recording sink at 16:9, 5:4 and ultrawide: opaque black exterior, transparent aperture, black reticle, depth/blend state, one draw, unscoped suppression.");
 using PickupPublish=int(__cdecl*)(int,int,int,float);auto pickup=reinterpret_cast<PickupPublish>(GetProcAddress(dll,"DX_CofPickupV1"));require(pickup!=nullptr,"pickup export");
 auto& emitters=*symbol<std::vector<cl_entity_t*>*>("g_entityLightEmitters");model_t syringe{};syringe.type=mod_studio;
 cl_entity_t item{};item.model=&syringe;item.index=123;item.curstate.modelindex=321;item.origin[0]=64;emitters={&item};
 auto eye=symbol<float*>("g_vOrigin");auto aim=symbol<float*>("g_vForward");eye[0]=eye[1]=eye[2]=0;aim[0]=1;aim[1]=aim[2]=0;
 auto select=symbol<void(*)()>("R_SelectCofPickup");auto strength=symbol<float(*)(const cl_entity_t*)>("R_CofPickupStrength");
 symbol<convar_t*>("dx_dlights")->value=0;client.time=100;require(pickup(123,321,1,100),"pickup publication");
 select();require(strength(&item)>.99f,"aimed nearby eligible pickup highlights");
 auto lights=symbol<std::vector<dx_dlight_t>*>("g_dxDLights");const auto lightCount=lights->size();
 symbol<convar_t*>("dx_dlights")->value=1;symbol<convar_t*>("dx_dlight_entities")->value=1;
 symbol<convar_t*>("dx_dlight_max")->value=32;symbol<convar_t*>("dx_dlight_radius_scale")->value=1;
 symbol<convar_t*>("dx_cof_pickup_outline")->value=1;
 select();require(lights->size()==lightCount+1,"pickup adds one faint fill alongside contour");
 const auto& fill=lights->back();
 require(fill.key==-40&&fill.radius==18.f&&std::fabs(fill.r-4.f/255.f)<.0001f&&fill.g==0&&fill.b==0,"fill is smaller and substantially dimmer than the old red hotspot");
 require(fill.origin[0]==64&&fill.origin[2]==4&&fill.dieTime>100&&fill.dieTime<100.11f,"fill floats above item and expires promptly");
 symbol<convar_t*>("dx_cof_pickup_outline")->value=0;select();require(lights->size()==lightCount+1,"disabling highlight also suppresses its fill");
 symbol<convar_t*>("dx_cof_pickup_outline")->value=1;
 std::vector<cof_pickup::Point> shape={{20,20,true},{40,20,true},{40,40,true},{20,40,true}};
 std::vector<uint32_t> topology={0,1,2,0,2,3};
 auto contour=cof_pickup::outline(shape,topology,0,0,80,80,1);
 require(!contour.empty(),"mesh silhouette produces contour");
 for(const auto& span:contour){require(span.alpha<=120&&span.x>=0&&span.y>=0&&span.x+span.width<=80&&span.y+span.height<=80,"faint bounded outline");
  require(!(span.x<30&&span.x+span.width>30&&span.y<=30&&span.y+span.height>30),"item interior remains transparent");}
 require(cof_pickup::outline(shape,{0,1,99},0,0,80,80,1).empty()&&cof_pickup::outline(shape,topology,0,0,80,80,NAN).empty(),"invalid outline input rejected");
 shape[0].x=NAN;require(cof_pickup::outline(shape,topology,0,0,80,80,1).empty(),"nonfinite contour rejected");
 studio_cache_t outlineCache{};outlineCache.gpuSkinningPoseValid=true;outlineCache.gpuSkinningBoneCount=1;
 for(int i=0;i<3;++i)outlineCache.gpuSkinningBoneMatrices[0][i][i]=1;
 outlineCache.batches.resize(1);auto& outlineBatch=outlineCache.batches[0];outlineBatch.sources.resize(3);outlineBatch.indices={0,1,2};
 outlineBatch.sources[0].x=-.2f;outlineBatch.sources[0].y=-.2f;
 outlineBatch.sources[1].x=.2f;outlineBatch.sources[1].y=-.2f;
 outlineBatch.sources[2].x=0;outlineBatch.sources[2].y=.2f;
 for(auto& sourceVertex:outlineBatch.sources)sourceVertex.z=.5f;
 D3DXMATRIX identity;D3DXMatrixIdentity(&identity);*symbol<D3DXMATRIX*>("g_sceneView")=identity;*symbol<D3DXMATRIX*>("g_sceneProjection")=identity;
 *symbol<D3DVIEWPORT9*>("g_sceneViewport")={0,0,1280,720,0,1};symbol<convar_t*>("dx_cof_pickup_outline")->value=1;
 auto queueOutline=symbol<void(*)(const studio_cache_t&,const D3DMATRIX&,float)>("R_QueueCofPickupOutline");
 auto drawOutline=symbol<void(*)()>("R_DrawCofPickupOutline");queueOutline(outlineCache,identity,1);draws.clear();drawOutline();
 require(draws.size()==1&&draws[0].state[D3DRS_ZENABLE]==FALSE&&draws[0].state[D3DRS_SRCBLEND]==D3DBLEND_SRCALPHA,"projected outline is one non-emissive 2D alpha draw");
 require((draws[0].vertices[0].color&0xffffff)==0xeb2d23,"outline uses red UI color instead of a point light");
 select();draws.clear();drawOutline();require(draws.empty(),"scene selection clears previous contour");
 aim[0]=-1;select();require(strength(&item)==0,"look-away clears");aim[0]=1;
 item.origin[0]=150;select();require(strength(&item)==0,"distant item unchanged");item.origin[0]=64;
 occluded=true;select();require(strength(&item)==0,"wall occlusion clears");occluded=false;
 pickup(123,999,1,100);select();require(strength(&item)==0,"entity/model reuse rejected");
 pickup(123,321,0,100);select();require(strength(&item)==0,"collected/carried item clears");
 pickup(123,321,1,100);client.time=101;select();require(strength(&item)==0,"stale item clears");
 require(!pickup(-1,321,1,100)&&!pickup(8192,321,1,100)&&!pickup(123,321,1,NAN),"invalid publication rejected");
 require(cof_features::pickupClass("item_syringe")&&cof_features::pickupClass("weapon_glock")&&cof_features::pickupClass("ammo_9mm")&&!cof_features::pickupClass("monster_syringe")&&!cof_features::pickupClass("prop_item"),"pickup classname policy");
 puts("PASS: pickup adds only a faint radius-18 red fill four units above the item, with prompt expiry and highlight-toggle suppression; transparent-interior outline, bounded raster and selection rejection preserved.");
 // Actual CPU skinning and pose selection feed a recording Remix API.
 std::ifstream policeFile(std::string(argv[2])+"/models/cutscene/player.mdl",std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(policeFile),{});
 std::ifstream donorFile(std::string(argv[2])+"/models/player/police1/police1.mdl",std::ios::binary);donorBytes.assign(std::istreambuf_iterator<char>(donorFile),{});
 require(donorBytes.size()>sizeof(studiohdr_t),"private donor bytes");donorModel.type=mod_studio;strcpy_s(donorModel.name,"models/cofr/player/police_shadow_anim.mdl");
 require(bytes.size()>sizeof(studiohdr_t),"police model");hdr=reinterpret_cast<studiohdr_t*>(bytes.data());
 auto tx=reinterpret_cast<mstudiotexture_t*>(bytes.data()+hdr->textureindex);
 for(int i=0;i<hdr->numtextures;++i){tx[i].index=1;tx[i].flags=STUDIO_NF_FULLBRIGHT;}
 auto textures=symbol<std::deque<dxtexture_t>*>("g_Textures");textures->resize(2);(*textures)[1].width=(*textures)[1].height=64;*symbol<int*>("g_texcount")=1;
 model_t police{};police.type=mod_studio;strcpy_s(police.name,"models/cofr/player/simon_shadow.mdl");
 model_drawdata_t data{};data.mod=&police;data.loaded=true;symbol<void(*)(model_t*,model_drawdata_t*)>("R_RegisterStudioModelDrawData")(&police,&data);
 auto crouchPose=symbol<bool(*)(int,float,bool)>("R_CofShadowCrouchPose");
 require(crouchPose(1,28,false)&&crouchPose(0,12,false)&&!crouchPose(0,28,true),"native hull duck or CoF camera-only duck; standing recovery");
 require(crouchPose(0,20,true)&&!crouchPose(0,20,false)&&!crouchPose(0,NAN,true),"camera duck hysteresis and invalid-input rejection");
 auto blockers=symbol<unsigned(*)(unsigned)>("R_CofShadowSceneBlockers");symbol<convar_t*>("dx_remix")->value=1;
 require(blockers(0)==0,"actual render-frame shadow gate admits ordinary first person");
 require(blockers(RF_DRAW_CUBEMAP)==256u&&blockers(RF_DRAW_OVERVIEW)==512u,"actual frame gate rejects auxiliary cameras");
 testHealth=0;require(blockers(0)==32u,"actual frame gate rejects dead player");testHealth=100;
 testThirdPerson=1;require(blockers(0)==64u,"actual frame gate rejects third person");testThirdPerson=0;
 client.viewentity=42;require(blockers(0)==16u,"actual frame gate rejects external viewentity");client.viewentity=1;
 auto hidden=reinterpret_cast<void(__cdecl*)(int)>(GetProcAddress(dll,"DX_SceneViewModelHiddenV1"));hidden(1);require(blockers(0)==4u,"actual frame gate rejects hidden scene");hidden(0);
 auto visibility=reinterpret_cast<int(__cdecl*)(unsigned,float)>(GetProcAddress(dll,"DX_CofPlayerVisibilityV1"));visibility(2,float(client.time));require(blockers(0)==128u,"actual frame gate rejects native camera scene");visibility(0,float(client.time));
 symbol<convar_t*>("dx_remix")->value=0;require(blockers(0)==2u,"actual frame gate explains absent Remix");symbol<convar_t*>("dx_remix")->value=1;
 symbol<convar_t*>("dx_cof_player_shadow")->value=1.f;
 auto mesh=symbol<CofShadowMesh*>("g_cofShadowMesh");CofShadowMeshTestAccess::bind(*mesh);
 auto shadow=symbol<void(*)(cl_entity_t*,model_t*,model_t*,cl_entity_t*)>("R_DrawCofPlayerShadow");
 cl_entity_t player{};player.index=1;player.player=true;player.curstate.scale=.01f;client.simorg[0]=10;client.simorg[1]=20;client.simorg[2]=36;client.time=200;
 shadow(&player,&police,&donorModel,&viewmodel);require(shadowCreates==1&&shadowDraws==1,"body submits once");
 require(shadowInstance.categoryFlags==(REMIXAPI_INSTANCE_CATEGORY_BIT_THIRD_PERSON_PLAYER_MODEL|REMIXAPI_INSTANCE_CATEGORY_BIT_THIRD_PERSON_PLAYER_BODY),"shadow-only camera categories");
 require(std::fabs(shadowInstance.transform.matrix[2][2]-1.f)<.001f,"private body ignores native hidden-player model scale");
 require(fabsf(shadowInstance.transform.matrix[0][3]-10)<.01f&&fabsf(shadowInstance.transform.matrix[1][3]-20)<.01f,"predicted player horizontal transform");
 float lo[3]={1e9f,1e9f,1e9f},hi[3]={-1e9f,-1e9f,-1e9f};
 for(const auto& v:shadowSkinnedVertices)for(int axis=0;axis<3;++axis){lo[axis]=std::min(lo[axis],v.position[axis]);hi[axis]=std::max(hi[axis],v.position[axis]);}
 require(std::fabs(lo[2]+shadowInstance.transform.matrix[2][3])<.01f,"standing sole touches hull floor instead of floating above bag light");
 require(hi[2]-lo[2]>55.f&&hi[2]-lo[2]<80.f,"plausible standing body height");
 const auto idleVertices=shadowSkinnedVertices;const auto bindVertices=shadowVertices;
 for(int i=1;i<=12;++i){client.time=200+i*.05;client.simorg[0]=10+i*10.f;shadow(&player,&police,&donorModel,&viewmodel);}
 require(shadowCreates==1&&shadowDraws==13,"one immutable mesh across changing animated bone submissions");
 auto shadowEntity=symbol<cl_entity_t*>("g_cofShadowEntity");auto donorHdr=reinterpret_cast<studiohdr_t*>(donorBytes.data());seqs=reinterpret_cast<mstudioseqdesc_t*>(donorBytes.data()+donorHdr->seqindex);
 require(!strncmp(seqs[shadowEntity->curstate.gaitsequence].label,"run",32),"motion-derived run gait despite empty network velocity");
 require(shadowVertices.size()==idleVertices.size(),"stable topology across animation");bool changed=false;
 for(size_t i=0;i<idleVertices.size();++i)for(int j=0;j<3;++j)if(std::fabs(idleVertices[i].position[j]-shadowSkinnedVertices[i].position[j])>.1f)changed=true;
 require(changed,"animated bone skinning changes silhouette");
 require(!memcmp(bindVertices.data(),shadowVertices.data(),bindVertices.size()*sizeof(bindVertices[0])),"bind geometry stays immutable");
 struct RigProbe {BoneMat34 local[128],bind[128],inverse[128];int map[128],parents[128];int count,mapped;};
 auto rig=symbol<RigProbe*>("g_cofShadowRig");require(rig->count==36&&rig->mapped==32&&shadowPose.size()==36,"32 shared police bones plus four Simon-only bones");
 unsigned extraBones=0;for(int i=0;i<rig->count;++i)if(rig->map[i]<0) {
  ++extraBones;require(rig->parents[i]>=0,"unmapped Simon bone has parent");const auto& parent=shadowPose[rig->parents[i]];
  for(int row=0;row<3;++row)for(int col=0;col<4;++col){float expected=col==3?parent.matrix[row][3]:0;
   for(int k=0;k<3;++k)expected+=parent.matrix[row][k]*rig->local[i][k][col];
   require(std::fabs(expected-shadowPose[i].matrix[row][col])<.001f,"Simon-only bone follows its animated parent");}
 }
 require(extraBones==4,"four extra finger/toe bones");
 auto choose=symbol<int(*)(const studiohdr_t*,const char*)>("R_CofShadowSequence");
 const int idle=choose(donorHdr,"idle");require(idle>=0&&choose(donorHdr,"unsupported_action")==idle,"missing animation safely ignored for idle");
 char oldLabel[32];memcpy(oldLabel,seqs[idle].label,32);strcpy_s(seqs[idle].label,"removed_idle");
 require(choose(donorHdr,"unsupported_action")==-1,"missing idle never substitutes arbitrary index zero");memcpy(seqs[idle].label,oldLabel,32);
 const auto poseCase=[&](bool crouch,float travel,const char* weapon,const char* expected) {
  player.curstate.usehull=crouch?1:0;client.time+=.1;client.simorg[0]+=travel;
  strcpy_s(scope.name,weapon);shadow(&player,&police,&donorModel,&viewmodel);
  const bool expectedGait=!strcmp(expected,"walk")||!strcmp(expected,"run")||!strncmp(expected,"crouch_",7);
  require(!strncmp(seqs[expectedGait?shadowEntity->curstate.gaitsequence:shadowEntity->curstate.sequence].label,expected,32),"supported donor locomotion/weapon pose");
  if(expectedGait&&strstr(weapon,"/rifle/"))require(!strncmp(seqs[shadowEntity->curstate.sequence].label,"rifle_idle",32),"rifle arms stay raised during gait/crouch");
  float foot=1e9f;for(const auto& v:shadowSkinnedVertices)foot=std::min(foot,v.position[2]);
  require(std::fabs(foot+shadowInstance.transform.matrix[2][3]-(client.simorg[2]-(crouch?18.f:36.f)))<.01f,"every donor pose keeps sole at standing/crouched hull floor");
  require(shadowCreates==1&&shadowPose.size()==36,"pose changes reuse Simon mesh and complete skeleton");
 };
 poseCase(false,0,"models/weapons/rifle/v_rifle.mdl","rifle_idle");
 poseCase(true,0,"models/weapons/rifle/v_rifle.mdl","crouch_idle");
 poseCase(true,5,"models/weapons/rifle/v_rifle.mdl","crouch_crouch");
 poseCase(false,5,"models/weapons/rifle/v_rifle.mdl","walk");
 poseCase(false,20,"models/weapons/rifle/v_rifle.mdl","run");
 poseCase(false,0,"models/weapons/syringe/v_syringe.mdl","syringe_idle");
 poseCase(false,0,"models/weapons/unknown/v_unknown.mdl","idle");
 player.curstate.usehull=0;
 auto cameraOrigin=symbol<float*>("g_vOrigin");const float oldEyeZ=cameraOrigin[2];cameraOrigin[2]=client.simorg[2]+12.f;client.time+=.1;
 shadow(&player,&police,&donorModel,&viewmodel);
 require(!strncmp(seqs[shadowEntity->curstate.gaitsequence].label,"crouch_idle",32),"CoF camera-only crouch chooses real donor crouch despite snapshot hull zero");
 float cameraDuckSole=1e9f;for(const auto& v:shadowSkinnedVertices)cameraDuckSole=std::min(cameraDuckSole,v.position[2]);
 require(std::fabs(cameraDuckSole+shadowInstance.transform.matrix[2][3]-(client.simorg[2]-36.f))<.01f,"camera-only duck preserves native standing collision floor");
 cameraOrigin[2]=client.simorg[2]+28.f;client.time+=.1;shadow(&player,&police,&donorModel,&viewmodel);cameraOrigin[2]=oldEyeZ;
 // Exercise real first-person clip labels through the production renderer.
 shadowVmModel=&scope;viewmodel.model=&scope;
 const auto loadVm=[&](const char* relative,const char* clip){
  strcpy_s(scope.name,relative);scope.type=mod_studio;
  std::ifstream f(std::string(argv[2])+"/"+relative,std::ios::binary);shadowVmBytes.assign(std::istreambuf_iterator<char>(f),{});
  require(shadowVmBytes.size()>sizeof(studiohdr_t),"actual action VM bytes");auto h=reinterpret_cast<studiohdr_t*>(shadowVmBytes.data());
  auto q=reinterpret_cast<mstudioseqdesc_t*>(shadowVmBytes.data()+h->seqindex);int found=-1;
  for(int i=0;i<h->numseq;++i)if(!strncmp(q[i].label,clip,32)){found=i;break;}require(found>=0,"actual named VM clip exists");
  viewmodel.curstate.sequence=found;viewmodel.curstate.frame=64;viewmodel.curstate.framerate=0;viewmodel.curstate.animtime=float(client.time);
 };
 const auto actionCase=[&](const char* relative,const char* clip,const char* expected,bool crouch=false,float travel=0){
  loadVm(relative,clip);player.curstate.usehull=crouch?1:0;client.time+=.1;client.simorg[0]+=travel;
  shadow(&player,&police,&donorModel,&viewmodel);
  require(!strncmp(seqs[shadowEntity->curstate.sequence].label,expected,32),"actual VM action maps to expected existing police sequence");
  require(shadowCreates==1,"all action families reuse the immutable Simon mesh");
 };
 actionCase("models/weapons/famas/v_famas.mdl","idle","m16_idle");
 actionCase("models/weapons/famas/v_famas.mdl","shoot1","m16_shoot");
 actionCase("models/weapons/famas/v_famas.mdl","reload_empty","m16_reloadempty");
 actionCase("models/weapons/m16/v_m16.mdl","reload_empty_burst","m16_reloadempty");
 actionCase("models/weapons/shotgun/v_shotgun.mdl","idle1","shotgun_idle");
 actionCase("models/weapons/shotgun/v_shotgun.mdl","rel_start","shotgun_reload_start");
 actionCase("models/weapons/shotgun/v_shotgun.mdl","rel_insert","shotgun_reload_insert");
 actionCase("models/weapons/shotgun/v_shotgun.mdl","rel_end_noshot","shotgun_reload_endempty");
 actionCase("models/weapons/tmp/v_tmp.mdl","reload_empty","tmp_reloadempty");
 actionCase("models/weapons/mp5/v_mp5.mdl","shoot1","tmp_shoot");
 actionCase("models/weapons/glock/v_glock.mdl","reload_noshot","pistol_reloadempty");
 actionCase("models/weapons/glock/v_glock.mdl","melee","pistol_melee");
 actionCase("models/weapons/glock/v_glock.mdl","shoot_noshot","pistol_idle");
 actionCase("models/weapons/p345/v_p345.mdl","reload_noshot","p345_reloadempty");
 actionCase("models/weapons/vp70/v_vp70.mdl","reload_noshot","vp70_reload_empty");
 actionCase("models/weapons/revolver/v_revolver.mdl","reload_insert3","revolver_reload_insert");
 actionCase("models/weapons/rifle/v_rifle.mdl","reload_insert","rifle_reload_insert");
 actionCase("models/weapons/rifle/v_rifle.mdl","melee","rifle_melee");
 actionCase("models/weapons/switchblade/v_switchblade.mdl","p2attack3","switchblade_attack2");
 actionCase("models/weapons/branch/v_branch.mdl","attack1","branch_attack");
 actionCase("models/weapons/sledgehammer/v_sledgehammer.mdl","attack","sledgehammer_attack");
 actionCase("models/weapons/syringe/v_syringe.mdl","use","syringe_use");
 actionCase("models/weapons/syringe/v_syringe.mdl","stab_miss","syringe_idle");
 actionCase("models/weapons/mobile/v_mobile.mdl","idle_flash","radio_idle");
 actionCase("models/weapons/flashlight/v_flashlight.mdl","melee","radio_melee");
 actionCase("models/weapons/famas/v_famas.mdl","sprint_idle","sprint",false,20);
 actionCase("models/weapons/famas/v_famas.mdl","jump_to","m16_idle",false,0);
 require(!strncmp(seqs[shadowEntity->curstate.gaitsequence].label,"jump",32),"existing jump lower-body clip is wired");
 // Compare hand pose relative to the spine: gait must change legs, not the
 // action's upper-body transform. The fixed VM frame makes this independent
 // of elapsed locomotion time and tests real MDL bones, not a state-name mirror.
 int spine=-1,hand=-1;auto bodyBones=reinterpret_cast<mstudiobone_t*>(bytes.data()+hdr->boneindex);
 for(int i=0;i<hdr->numbones;++i){if(!strncmp(bodyBones[i].name,"Bip01 Spine",32))spine=i;if(!strncmp(bodyBones[i].name,"Bip01 R Hand",32))hand=i;}
 require(spine>=0&&hand>=0,"layering comparison bones exist");
 const auto relativeHand=[&](){std::array<float,3> result{};const auto& a=shadowPose[spine].matrix;const auto& h=shadowPose[hand].matrix;
  for(int j=0;j<3;++j)for(int k=0;k<3;++k)result[j]+=a[k][j]*(h[k][3]-a[k][3]);return result;};
 const auto settle=[&](float travel=0.f){for(int i=0;i<12;++i){client.time+=.05;client.simorg[0]+=travel;cameraOrigin[2]=client.simorg[2]+28;shadow(&player,&police,&donorModel,&viewmodel);}};
 actionCase("models/weapons/famas/v_famas.mdl","shoot1","m16_shoot");settle();auto standingHand=relativeHand();
 actionCase("models/weapons/famas/v_famas.mdl","shoot1","m16_shoot",false,10);settle(5);auto walkingHand=relativeHand();
 require(!strncmp(seqs[shadowEntity->curstate.gaitsequence].label,"walk",32),"weapon action layers over walking legs");
 actionCase("models/weapons/famas/v_famas.mdl","shoot1","m16_shoot",true,0);settle();auto crouchedHand=relativeHand();
 require(!strncmp(seqs[shadowEntity->curstate.gaitsequence].label,"crouch_idle",32),"weapon action layers over crouched legs");
 for(int j=0;j<3;++j)require(std::fabs(standingHand[j]-walkingHand[j])<.02f&&std::fabs(standingHand[j]-crouchedHand[j])<.02f,"weapon hand pose relative to spine survives walk and crouch");
 const int shoot=choose(donorHdr,"m16_shoot");char savedShoot[32];memcpy(savedShoot,seqs[shoot].label,32);strcpy_s(seqs[shoot].label,"missing_m16_shoot");
 actionCase("models/weapons/famas/v_famas.mdl","shoot1","m16_idle");memcpy(seqs[shoot].label,savedShoot,32);
 shadowVmModel=nullptr;shadowVmBytes.clear();player.curstate.usehull=0;strcpy_s(scope.name,"models/weapons/unknown/v_unknown.mdl");
 puts("PASS: actual VM-to-police idle/fire/dry-fire/reload/melee/heal aliases, FAMAS/M16, shotgun phases, revolver inserts, sprint/jump and layered weapon hands across walking/crouching; missing actions retain weapon stance. One mesh throughout.");
 #include "verify_cof_shadow_motion.inl"
 const auto beforeDebug=shadowCreates;
 symbol<convar_t*>("dx_cof_player_shadow")->value=2;client.time+=.1;shadow(&player,&police,&donorModel,&viewmodel);
 require(shadowInstance.categoryFlags==0&&shadowCreates==beforeDebug,"visible debug body uses existing animated mesh");
 symbol<convar_t*>("dx_cof_player_shadow")->value=1;client.time+=.1;shadow(&player,&police,&donorModel,&viewmodel);
 require(shadowInstance.categoryFlags==(REMIXAPI_INSTANCE_CATEGORY_BIT_THIRD_PERSON_PLAYER_MODEL|REMIXAPI_INSTANCE_CATEGORY_BIT_THIRD_PERSON_PLAYER_BODY),"normal mode restores primary-hidden shadow categories");
 const auto oldHash=shadowHash;symbol<void(*)()>("R_ResetCofShadow")();require(shadowDestroys==1,"shadow teardown releases mesh");
 client.time=201;shadow(&player,&police,&donorModel,&viewmodel);require(shadowHash!=oldHash&&shadowCreates==2,"map reset creates one replacement with new lifetime");
 symbol<convar_t*>("dx_cof_player_shadow")->value=0;
 const unsigned oldDraws=shadowDraws;shadow(&player,&police,&donorModel,&viewmodel);shadow(&player,&police,&donorModel,&viewmodel);
 require(shadowDraws==oldDraws&&!mesh->available(),"disabled cvar retires mesh and skips submissions");
 symbol<convar_t*>("dx_cof_player_shadow")->value=1;client.time=202;
 shadow(&player,&police,&donorModel,&viewmodel);require(shadowDraws==oldDraws+1&&shadowCreates==3,"cvar re-enable builds one mesh");
 symbol<void(*)()>("R_ResetCofShadow")();rejectShadowMesh=true;
 shadow(&player,&police,&donorModel,&viewmodel);const auto failedCalls=shadowCreates;
 for(int i=0;i<100;++i){client.time=203+i*.05;shadow(&player,&police,&donorModel,&viewmodel);}
 require(shadowCreates==failedCalls&&shadowErrors==1,"unsupported mesh is disabled once with one diagnostic and no retry storm");
 symbol<void(*)()>("R_ResetCofShadow")();
 puts("PASS: private Simon mesh with police animation and bind-pose retargeting, predicted transform, model/body shadow categories, hidden primary-camera configuration, grounded standing/crouched/weapon poses, visible debug mode, isolated model scale, one immutable skinned mesh, supported bone instances, resource reset, cvar off/on, error circuit breaker and fresh mesh lifetime. 32 shared/four parent-following bones and missing-action fallback. Recording Remix API only.");
 // Invoke the compiled server wrapper against a recording native full-pack callback.
 const auto rendererBase=moduleBase;auto server=LoadLibraryA(argv[3]);require(server!=nullptr,"candidate server DLL");
 const std::string serverPath=argv[3];SymSetSearchPath(process,serverPath.substr(0,serverPath.find_last_of("/\\")).c_str());
 moduleBase=SymLoadModuleEx(process,nullptr,argv[3],nullptr,reinterpret_cast<DWORD64>(server),0,nullptr,0);require(moduleBase!=0,"server PDB");
 auto nativeApi=symbol<DLL_FUNCTIONS*>("hl!native");auto serverEngine=symbol<enginefuncs_t*>("hl!engine");
 globalvars_t serverGlobals{};serverGlobals.maxClients=1;serverGlobals.time=300;
 *symbol<globalvars_t**>("hl!globals")=&serverGlobals;nativeApi->pfnAddToFullPack=nativePack;
 serverEngine->pfnGetVarsOfEnt=varsForPickup;serverEngine->pfnSzFromIndex=nameForPickup;
 using Pack=int(*)(entity_state_t*,int,edict_t*,edict_t*,int,int,unsigned char*);
 auto pack=symbol<Pack>("hl!fullPack");edict_t source{},viewer{};source.v.modelindex=321;source.v.solid=SOLID_TRIGGER;
 entity_state_t packed{},expected{};memset(&expected,0x5a,sizeof expected);
 moduleBase=rendererBase;auto table=symbol<cof_features::Pickups*>("g_cofPickups");
 require(pack(&packed,123,&source,&viewer,0,0,nullptr)==1&&memcmp(&packed,&expected,sizeof packed)==0,"native packet bytes preserved");
 require(table->contains(123,321,300),"server confirms real pickup");
 pickupName="weapon_flare";source.v.owner=&viewer;source.v.model=2;source.v.movetype=MOVETYPE_TOSS;source.v.renderfx=99;
 require(pack(&packed,123,&source,&viewer,0,0,nullptr)==1&&table->contains(123,321,300)&&memcmp(&packed,&expected,sizeof packed)==0,"native dropped flare retains owner but remains a pickup; packet unchanged");
 source.v.renderfx=165;pack(&packed,123,&source,&viewer,0,0,nullptr);require(!table->contains(123,321,300),"owned burning flare is not a pickup");
 source.v.renderfx=99;source.v.movetype=MOVETYPE_FOLLOW;pack(&packed,123,&source,&viewer,0,0,nullptr);require(!table->contains(123,321,300),"held flare excluded");
 source.v.owner=nullptr;source.v.renderfx=0;source.v.movetype=0;source.v.model=0;
 pickupName="monster_syringe";pack(&packed,123,&source,&viewer,0,0,nullptr);require(!table->contains(123,321,300),"decorative/NPC model cannot opt into pickup");
 pickupName="item_syringe";source.v.owner=&viewer;pack(&packed,123,&source,&viewer,0,0,nullptr);require(!table->contains(123,321,300),"carried item excluded");source.v.owner=nullptr;
 source.v.effects=EF_NODRAW;pack(&packed,123,&source,&viewer,0,0,nullptr);require(!table->contains(123,321,300),"hidden item excluded");source.v.effects=0;
 source.v.solid=SOLID_NOT;pack(&packed,123,&source,&viewer,0,0,nullptr);require(!table->contains(123,321,300),"removed/non-solid item excluded");source.v.solid=SOLID_TRIGGER;
 nativePackAccepts=false;require(pack(&packed,123,&source,&viewer,0,0,nullptr)==0&&!table->contains(123,321,300),"native rejection preserved");nativePackAccepts=true;
 require(pack(&packed,123,&source,&viewer,0,1,nullptr)==1&&!table->contains(123,321,300),"player entity excluded");
 serverGlobals.maxClients=2;pack(&packed,123,&source,&viewer,0,0,nullptr);require(!table->contains(123,321,300),"local feature does not alter multiplayer packets");
 puts("PASS: actual compiled server full-pack wrapper preserves native packet bytes/results; publishes usable world items and rejects owned/hidden/non-solid/NPC/player/native-rejected entities; multiplayer unchanged.");
 symbol<void(*)()>("R_FreeScratchBuffers")();*symbol<IDirect3DDevice9**>("device")=nullptr;return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL COF FEATURES: %s\n",e.what());return 1;}
