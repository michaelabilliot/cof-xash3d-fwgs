#include "dx_local.h"
#include "cdll_int.h"
#include <cstdarg>
#include <filesystem>
#include "dx_camera_flash.h"
#include "dx_cof_native_light.h"
#include <dbghelp.h>
#include <fstream>
#include <stdexcept>
#include <limits>
static HANDLE process=GetCurrentProcess();static DWORD64 base;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
#include "verify_camera_area.h"
template<class T>static T symbol(const char* name){
 alignas(SYMBOL_INFO) char bytes[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};
 auto info=reinterpret_cast<SYMBOL_INFO*>(bytes);info->SizeOfStruct=sizeof(SYMBOL_INFO);info->MaxNameLen=MAX_SYM_NAME;
 check(SymFromName(process,name,info)&&info->ModBase==base,name);return reinterpret_cast<T>(static_cast<uintptr_t>(info->Address));
}
static ref_client_t client{};static ref_host_t host{};static cvar_t shared{};
static int health=100,thirdPerson=0;static std::string logs;
static void print(const char* fmt,...){char line[2048];va_list args;va_start(args,fmt);vsnprintf(line,sizeof line,fmt,args);va_end(args);logs+=line;}
static cl_entity_t vm{};static std::vector<char> modelBytes;
static intptr_t parm(int key,int){
 if(key==PARM_GET_CLIENT_PTR)return reinterpret_cast<intptr_t>(&client);
 if(key==PARM_GET_HOST_PTR)return reinterpret_cast<intptr_t>(&host);
 if(key==PARM_GET_VIEWENT_PTR)return reinterpret_cast<intptr_t>(&vm);
 if(key==PARM_LOCAL_HEALTH)return health;if(key==PARM_THIRDPERSON)return thirdPerson;return 0;
}
static void reg(convar_t* v){v->value=static_cast<float>(atof(v->string));}
static cvar_t* cvar(const char*){return &shared;}
static void* extra(int,model_t*){return modelBytes.data();}
struct Device{void** methods;};static unsigned sets=0,on=0,off=0;static bool failSet=false,failEnable=false;static D3DLIGHT9 recorded{};
static bool lightEnabled=false;static unsigned worldStages=0;
static void areaObserved(const area_record::Definition* definition){
 if(!definition){lightEnabled=false;++off;return;}
 ++sets;++on;lightEnabled=true;recorded={};recorded.Type=D3DLIGHT_SPOT;recorded.Range=700;
 recorded.Position={definition->rect.position.x,definition->rect.position.y,definition->rect.position.z};recorded.Direction={definition->rect.direction.x,definition->rect.direction.y,definition->rect.direction.z};
 const double perUnit=dx_map_light::radiance(1.f,700,1,0,3.f/(700.f*700.f),4)*.5625*3.14159265358979323846*16/(128.*128.);
 const float equivalent=float(definition->info.radiance.x*camera_area::irradiance(definition->rect.xSize,definition->rect.ySize,128)/perUnit);
 recorded.Diffuse={equivalent,equivalent,equivalent,1};
}
static void worldStage(){check(lightEnabled,"photo light enabled before normal world-stage callback");++worldStages;}
static HRESULT STDMETHODCALLTYPE transform(Device*,D3DTRANSFORMSTATETYPE,const D3DMATRIX*){return S_OK;}
static unsigned cameraViewports=0;
static HRESULT STDMETHODCALLTYPE viewport(Device*,const D3DVIEWPORT9* value){if(value->MinZ==0&&value->MaxZ==0)++cameraViewports;return S_OK;}
static HRESULT STDMETHODCALLTYPE state(Device*,D3DRENDERSTATETYPE,DWORD){return S_OK;}
static HRESULT STDMETHODCALLTYPE texture(Device*,DWORD,IDirect3DBaseTexture9*){return S_OK;}
static HRESULT STDMETHODCALLTYPE stage(Device*,DWORD,D3DTEXTURESTAGESTATETYPE,DWORD){return S_OK;}
static HRESULT STDMETHODCALLTYPE sampler(Device*,DWORD,D3DSAMPLERSTATETYPE,DWORD){return S_OK;}
static HRESULT STDMETHODCALLTYPE fvf(Device*,DWORD){return S_OK;}
static HRESULT STDMETHODCALLTYPE setLight(Device*,DWORD slot,const D3DLIGHT9* light){check(slot<2&&light->Range!=700,"camera must never submit a legacy spotlight");++sets;recorded=*light;return failSet?E_FAIL:S_OK;}
static HRESULT STDMETHODCALLTYPE enable(Device*,DWORD slot,BOOL value){check(slot<2,"enable slot");lightEnabled=value!=FALSE;if(value)++on;else ++off;return value&&failEnable?E_FAIL:S_OK;}
static pfnUserMsgHook registeredPhoto=nullptr;static float messageTime=0;
static float clientTime(){return messageTime;}
static float debugEnabled(const char*){return 1;}
static int hookMessage(const char* name,pfnUserMsgHook callback){check(!strcmp(name,"TakePhoto"),"expected photo registration");registeredPhoto=callback;return 1;}
int main(int argc,char** argv)try{
 check(argc==3||argc==5,"renderer camera-model [client-wrapper original-client]");
 std::ifstream file(argv[2],std::ios::binary);modelBytes.assign(std::istreambuf_iterator<char>(file),{});check(modelBytes.size()>sizeof(studiohdr_t),"camera model");
 auto dll=LoadLibraryA(argv[1]);check(dll!=nullptr,"renderer");
 SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_EXACT_SYMBOLS);check(SymInitialize(process,nullptr,FALSE)!=FALSE,"symbols");
 const std::string path=argv[1];SymSetSearchPath(process,path.substr(0,path.find_last_of("/\\")).c_str());
 base=SymLoadModuleEx(process,nullptr,argv[1],nullptr,reinterpret_cast<DWORD64>(dll),0,nullptr,0);check(base!=0,"matching PDB");
 using API=int(*)(int,ref_interface_t*,ref_api_t*,ref_globals_t*);
 auto api=reinterpret_cast<API>(GetProcAddress(dll,"GetRefAPI"));ref_api_t engine{};ref_interface_t functions{};ref_globals_t globals{};
 engine.EngineGetParm=parm;engine.Cvar_RegisterVariable=reg;engine.pfnGetCvarPointer=cvar;engine.Mod_Extradata=extra;engine.Con_Printf=print;
 check(api&&api(REF_API_VERSION,&functions,&engine,&globals)==REF_API_VERSION,"API");
 model_t model{};model.type=mod_studio;strcpy_s(model.name,"models/weapons/camera/v_camera.mdl");vm.model=&model;vm.curstate.framerate=1;vm.curstate.sequence=3;
 client.maxclients=1;client.playernum=0;client.viewentity=1;client.time=10;shared.value=1;
 *symbol<bool*>("g_sceneViewportValid")=true;*symbol<D3DVIEWPORT9*>("g_sceneViewport")={0,0,1920,1080,0,1};
 symbol<convar_t*>("r_viewmodel_fov")->value=60;symbol<convar_t*>("r_viewmodel_near")->value=.01f;
 D3DXMatrixPerspectiveFovLH(symbol<D3DXMATRIX*>("g_sceneProjection"),D3DXToRadian(75.f),16.f/9.f,4.f,8192.f);
 using Pose=int(__cdecl*)(cl_entity_t*,float,const float*,const float*,const float*,float*);
 auto pose=reinterpret_cast<Pose>(GetProcAddress(dll,"DX_StudioCameraFlashPointV1"));check(pose!=nullptr,"camera flash pose export");
 float zero[3]{},point[3],moved[3];check(pose(&vm,10,zero,zero,zero,point)!=0,"flash bone pose without attachments");
 for(int i=0;i<3;++i)vm.origin[i]=48;float translated[3]={48,48,48};
 check(pose(&vm,10,translated,zero,zero,moved)!=0,"translated flash");for(int i=0;i<3;++i)check(fabsf(moved[i]-point[i]-48)<.01f,"flash follows model position");
 memset(vm.origin,0,sizeof vm.origin);
 float yaw[3]={0,90,0};vm.angles[1]=90;
 check(pose(&vm,10,zero,yaw,zero,moved)!=0,"rotated flash");
 check(fabsf(moved[0]+point[1])<.01f&&fabsf(moved[1]-point[0])<.01f,"flash follows model yaw");vm.angles[1]=0;
 float shifted[3]={2,3,4};check(pose(&vm,10,zero,zero,shifted,moved)!=0,"offset flash");
 D3DMATRIX correction;check(symbol<bool(*)(const float*,const float*,D3DMATRIX&)>("R_CofCameraViewModelCorrection")(zero,zero,correction),"Remix viewmodel correction");
 D3DXVECTOR3 delta(2,-3,4),corrected;D3DXVec3TransformNormal(&corrected,&delta,reinterpret_cast<D3DXMATRIX*>(&correction));
 check(fabsf(moved[0]-point[0]-corrected.x)<.01f&&fabsf(moved[1]-point[1]-corrected.y)<.01f&&fabsf(moved[2]-point[2]-corrected.z)<.01f,"viewmodel offsets follow Remix scale and FOV");
 auto checkedHeader=reinterpret_cast<studiohdr_t*>(modelBytes.data());const int boneIndex=checkedHeader->boneindex;
 checkedHeader->boneindex=checkedHeader->length;moved[0]=123;
 check(!pose(&vm,10,zero,zero,zero,moved)&&moved[0]==123,"malformed flash skeleton leaves output unchanged");checkedHeader->boneindex=boneIndex;
 check(!pose(&vm,std::numeric_limits<float>::quiet_NaN(),zero,zero,zero,moved),"nonfinite pose rejected");
 unsigned poses=0;auto hdr=reinterpret_cast<studiohdr_t*>(modelBytes.data());
 for(int seq=0;seq<hdr->numseq;++seq)for(float time:{10.f,10.1f,10.2f}){vm.curstate.sequence=seq;check(pose(&vm,time,zero,zero,zero,moved)!=0,"all camera animation poses");++poses;}
 vm.curstate.sequence=3;
 using Publish=int(__cdecl*)(const dx_cof_light_state_v2*);using Trigger=int(__cdecl*)(float);using Describe=int(__cdecl*)(D3DLIGHT9*);
 auto publish=reinterpret_cast<Publish>(GetProcAddress(dll,"DX_CofLightStateV2"));auto trigger=reinterpret_cast<Trigger>(GetProcAddress(dll,"DX_CofCameraFlashV1"));auto describe=reinterpret_cast<Describe>(GetProcAddress(dll,"DX_CofCameraFlashDescriptionV1"));
 check(publish&&trigger&&describe,"photo exports");
 for(const char* name:{"dx_remix","dx_dlights","dx_dlight_fixed","dx_cof_camera_flash"})symbol<convar_t*>(name)->value=1;
 symbol<float*>("g_vForward")[0]=1;
 *symbol<unsigned*>("g_currentViewFlags")=RF_DRAW_WORLD;
 symbol<convar_t*>("dx_cof_light_debug")->value=1;
 CameraFlashAreaLightTestAccess::bind(*symbol<CameraFlashAreaLight*>("g_cameraAreaLight"));area_record::observer=areaObserved;
 auto begin=symbol<void(*)()>("R_BeginCofCameraFlashView"),end=symbol<void(*)()>("R_EndCofCameraFlashView");
 auto reset=symbol<void(*)()>("R_ResetCofLights");
 D3DLIGHT9 light{};check(!describe(&light),"no light from draw/equip alone");
 check(trigger(10)&&!trigger(10),"one event per accepted exposure timestamp");
 check(!describe(&light),"descriptor query cannot start a pending exposure");
 for(unsigned flags:{0u,unsigned(RF_DRAW_WORLD|RF_DRAW_CUBEMAP),unsigned(RF_DRAW_WORLD|RF_DRAW_OVERVIEW)}){
  *symbol<unsigned*>("g_currentViewFlags")=flags;client.time=10.4;begin();check(!describe(&light),"auxiliary/model-preview view cannot consume exposure");end();
 }
 *symbol<unsigned*>("g_currentViewFlags")=RF_DRAW_WORLD;client.time=10.8;begin();
 check(describe(&light)&&light.Type==D3DLIGHT_SPOT&&light.Range==700&&fabsf(light.Theta-DEG2RAD(100.f))<.0001f&&fabsf(light.Phi-DEG2RAD(120.f))<.0001f,"broad camera-only preview profile");
 check(pose(&vm,float(client.time),zero,zero,zero,point),"animated flash-tip pose");
 check(fabsf(light.Diffuse.r-1.12f)<.001f&&light.Direction.x>.99f&&fabsf(light.Position.x-point[0])<.001f&&fabsf(light.Position.y-point[1])<.001f&&fabsf(light.Position.z-point[2])<.001f,"light originates at resolved visible flash face, not the eye");
 void* methods[119]{};methods[51]=reinterpret_cast<void*>(setLight);methods[53]=reinterpret_cast<void*>(enable);methods[44]=reinterpret_cast<void*>(transform);methods[47]=reinterpret_cast<void*>(viewport);methods[57]=reinterpret_cast<void*>(state);methods[65]=reinterpret_cast<void*>(texture);methods[67]=reinterpret_cast<void*>(stage);methods[69]=reinterpret_cast<void*>(sampler);methods[89]=reinterpret_cast<void*>(fvf);
 Device recorder{methods};*symbol<IDirect3DDevice9**>("device")=reinterpret_cast<IDirect3DDevice9*>(&recorder);
 auto apply=symbol<void(*)()>("R_ApplyDLights");
 const D3DLIGHT9 captured=light;const auto savedSequence=vm.curstate.sequence;vm.curstate.sequence=0;
 check(describe(&light)&&memcmp(&captured.Position,&light.Position,sizeof light.Position)==0,"descriptor retains the single resolved socket for this world pass");vm.curstate.sequence=savedSequence;
 apply();check(sets==1&&on==1,"production light submission");check(symbol<bool(*)()>("R_CofCameraFlashShadowActive")(),"successful photo draw activates private arm-shadow suppression");end();
 client.time=10.81;begin();apply();check(fabsf(recorded.Diffuse.r-1.12f)<.001f,"first25ms full brightness");end();
 client.time=10.85;begin();apply();check(recorded.Diffuse.r>.74f&&recorded.Diffuse.r<.76f,"50ms short discharge tail");end();
 client.time=10.899;begin();apply();check(recorded.Diffuse.r>0&&recorded.Diffuse.r<.02f,"light lasts almost100ms");end();
 client.time=10.901;begin();apply();check(!describe(&light)&&!lightEnabled&&!symbol<bool(*)()>("R_CofCameraFlashShadowActive")(),"100ms expiry restores ordinary shadow eligibility");end();
 client.time=20;check(trigger(20),"new photo");
 vm.curstate.effects=EF_NODRAW;shared.value=0;auto header=reinterpret_cast<studiohdr_t*>(modelBytes.data());const auto savedBone=header->boneindex;header->boneindex=header->length;
 begin();check(!describe(&light),"missing flash tip never falls back to face light");end();header->boneindex=savedBone;
 client.time=20.01;begin();check(describe(&light),"valid tip starts deferred pulse despite hidden-mesh preference");end();vm.curstate.effects=0;shared.value=1;
 for(int sequence=0;sequence<hdr->numseq;++sequence){
  vm.curstate.sequence=sequence;client.time=20.02;begin();check(describe(&light),"camera animation transitions retain real light");
  check(pose(&vm,float(client.time),zero,zero,zero,point),"animated tip reference");
  check(fabsf(light.Position.x-point[0])<.001f&&fabsf(light.Position.y-point[1])<.001f&&fabsf(light.Position.z-point[2])<.001f,"source follows cleared emitting face in every animation");end();
 }
 auto eye=symbol<float*>("g_vOrigin"),angles=symbol<float*>("g_vAngles"),forward=symbol<float*>("g_vForward");
 eye[0]=120;eye[1]=45;eye[2]=30;angles[1]=90;forward[0]=0;forward[1]=1;
 symbol<convar_t*>("r_viewmodel_offset_x")->value=2;symbol<convar_t*>("r_viewmodel_offset_y")->value=3;symbol<convar_t*>("r_viewmodel_offset_z")->value=4;
 begin();check(describe(&light)&&pose(&vm,float(client.time),eye,angles,shifted,point),"moved camera tip");
 check(fabsf(light.Position.x-point[0])<.001f&&fabsf(light.Position.y-point[1])<.001f&&fabsf(light.Position.z-point[2])<.001f,"tip source follows view translation, yaw and viewmodel offsets");end();
 memset(eye,0,12);memset(angles,0,12);forward[0]=1;forward[1]=0;
 for(const char* name:{"r_viewmodel_offset_x","r_viewmodel_offset_y","r_viewmodel_offset_z"})symbol<convar_t*>(name)->value=0;
 vm.curstate.sequence=3;
 strcpy_s(model.name,"models/weapons/glock/v_glock.mdl");begin();check(!describe(&light),"weapon change cancels pulse");end();
 const auto priorCameraViewports=cameraViewports;symbol<void(*)()>("R_DrawViewModel")();check(cameraViewports==priorCameraViewports,"ordinary weapon viewport remains unchanged");
 strcpy_s(model.name,"models/weapons/camera/v_camera.mdl");
 client.time=30;check(trigger(30),"scene-lag photo");*symbol<bool*>("g_sceneViewModelHidden")=true;begin();check(!describe(&light),"hidden scene waits");end();
 *symbol<bool*>("g_sceneViewModelHidden")=false;client.time=30.7;begin();check(describe(&light)&&light.Diffuse.r>1.11f,"eligible main view receives a complete pulse after scene delay");end();
 client.time=31;check(trigger(31),"accepted second photo refreshes single source");begin();check(describe(&light)&&light.Diffuse.r>1.11f,"refresh resets one pulse");end();
 client.time=29;begin();check(!describe(&light),"clock rollback clears exposure");end();
 for(int gate=0;gate<4;++gate){client.time=40+gate;check(trigger(float(client.time)),"gated photo");
  if(gate==0)health=0;if(gate==1)thirdPerson=1;if(gate==2)client.viewentity=2;if(gate==3)symbol<convar_t*>("dx_cof_camera_flash")->value=0;
  begin();check(!describe(&light),"dead/external/third-person/off suppress real flash");end();health=100;thirdPerson=0;client.viewentity=1;symbol<convar_t*>("dx_cof_camera_flash")->value=1;
 }
 client.time=50;check(trigger(50),"failure pulse");area_record::failCreate=true;begin();check(!describe(&light)&&area_record::live.empty(),"failed area definition leaves exposure pending without legacy fallback");end();area_record::failCreate=false;
 begin();area_record::failDraw=true;apply();check(area_record::live.empty(),"failed area draw retires handle");end();area_record::failDraw=false;
 client.time=50.5;begin();apply();check(recorded.Diffuse.r>1.11f&&area_record::live.size()==1,"first successful area draw gets full exposure after failure");end();
 client.time=50.55;area_record::failUpdate=true;begin();apply();end();check(area_record::live.size()==1,"unsupported update rebuilds one emitter");area_record::failUpdate=false;
 client.time=50.61;begin();apply();end();check(area_record::live.empty(),"rebuilding definitions does not restart100ms exposure");
 CameraFlashAreaLightTestAccess::bind(*symbol<CameraFlashAreaLight*>("g_cameraAreaLight"),false);
 client.time=52;check(trigger(52),"missing-update photo");const auto previousCreates=area_record::created;begin();apply();end();client.time=52.05;begin();apply();end();
 check(area_record::created==previousCreates+2&&area_record::live.size()==1,"absent update API retires/recreates one emitter per changed frame");
 client.time=52.11;begin();apply();end();check(area_record::live.empty(),"absent update API retains100ms expiry");
 CameraFlashAreaLightTestAccess::bind(*symbol<CameraFlashAreaLight*>("g_cameraAreaLight"));
 reset();check(!describe(&light),"map/device reset clears pulse");
 check(!trigger(std::numeric_limits<float>::quiet_NaN()),"NaN trigger rejected");
 client.time=60;check(trigger(60.8f),"message/view clock skew accepted");client.time=61;begin();check(describe(&light)&&light.Diffuse.r>1.11f,"first world view starts complete exposure despite clock skew");end();reset();
 if(argc==5){
  const auto rendererBase=base;const auto nativePath=std::filesystem::path(argv[4]);SetDllDirectoryW(nativePath.parent_path().parent_path().parent_path().c_str());
  auto native=LoadLibraryExA(argv[4],nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);check(native!=nullptr,"original client loaded without game");
  auto wrapper=LoadLibraryExA(argv[3],nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);check(wrapper!=nullptr,"production client wrapper");
  const std::string wrapperPath=argv[3];SymSetSearchPath(process,wrapperPath.substr(0,wrapperPath.find_last_of("/\\")).c_str());
  base=SymLoadModuleEx(process,nullptr,argv[3],nullptr,reinterpret_cast<DWORD64>(wrapper),0,nullptr,0);check(base!=0,"client matching PDB");
  auto wrapperEngine=symbol<cl_enginefunc_t*>("client!engine");*wrapperEngine={};wrapperEngine->GetClientTime=clientTime;wrapperEngine->pfnHookUserMsg=hookMessage;wrapperEngine->pfnGetCvarFloat=debugEnabled;
  auto original=reinterpret_cast<pfnUserMsgHook>(reinterpret_cast<unsigned char*>(native)+0x28720);
  auto nativeFlag=reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(native)+0x432c74);
  auto registerMessage=symbol<int(*)(const char*,pfnUserMsgHook)>("client!hookMessage");check(registerMessage("TakePhoto",original)==1&&registeredPhoto!=original,"production hook registration wraps native callback");
  base=rendererBase;client.time=100;messageTime=100.6f;*nativeFlag=0;
  *reinterpret_cast<float(__cdecl**)()>(reinterpret_cast<unsigned char*>(native)+0x10dd80)=clientTime;
  unsigned char nativeLight[9]={0,0,0,0,0,0,40,1,30};
  auto mydlight=reinterpret_cast<int(__cdecl*)(const char*,int,void*)>(reinterpret_cast<unsigned char*>(native)+0x18dd0);
  const auto beforeCompanion=sets;const int beforeEffects=vm.curstate.effects;
  check(mydlight("mydlight",sizeof nativeLight,nativeLight)==1&&vm.curstate.effects==beforeEffects&&sets==beforeCompanion,"native companion packet creates no legacy world light or fullbright camera flag");
  const int expected=original("TakePhoto",0,nullptr);*nativeFlag=0;
  check(registeredPhoto("TakePhoto",0,nullptr)==expected&&*nativeFlag==1,"production wrapper preserves real native photo result and side effect");
  check(!describe(&light),"native message queues until world pass");
  auto engineTable=symbol<ref_api_t*>("gEngfuncs");engineTable->pfnDrawNormalTriangles=worldStage;
  auto renderWorld=symbol<void(*)(const float*)>("R_DrawEnts");
  client.time=101;*symbol<unsigned*>("g_currentViewFlags")=RF_DRAW_WORLD;
  renderWorld(zero);check(worldStages==1&&lightEnabled&&recorded.Range==700&&recorded.Direction.x>.99f,"native photo through actual R_DrawEnts lights world stage");
  check(cameraViewports>0,"camera viewport explicitly supports Remix viewmodel receiver exclusion");
  const auto afterShot=sets;client.time=101.05;renderWorld(zero);check(sets==afterShot+1&&recorded.Diffuse.r>0,"held attack frames do not create additional photo events");
  engineTable->pfnDrawNormalTriangles=nullptr;client.time=101.11;renderWorld(zero);check(!lightEnabled,"native photo pulse expires without retrigger");
  *nativeFlag=0;messageTime=103;client.time=103;registeredPhoto("TakePhoto",1,nullptr);check(*nativeFlag==1,"malformed event still preserves native callback");begin();check(!describe(&light),"malformed photo does not create light");end();
  engineTable->pfnDrawNormalTriangles=nullptr;reset();
  puts("PASS: real native mydlight + TakePhoto -> registered production client wrapper -> renderer queue -> actual R_DrawEnts light submission before normal world stage; delayed frames, 100ms illumination, no input polling/retrigger. No game/device/window/input.");
 }
 check(logs.find("camera message accepted")!=std::string::npos&&logs.find("first world frame")!=std::string::npos&&logs.find("blockers=")!=std::string::npos&&logs.find("camera area draw")!=std::string::npos,"bounded diagnostics cover queue, gate, view and submission");
 reset();check(area_record::live.empty()&&area_record::created==area_record::destroyed,"all area lifetimes retired");
 using NativePublish=int(__cdecl*)(const dx_cof_native_light*);
 auto nativePublish=reinterpret_cast<NativePublish>(GetProcAddress(dll,"DX_CofNativeLightV1"));check(nativePublish!=nullptr,"native light bridge export");
 symbol<convar_t*>("dx_dlight_entities")->value=1;
 dx_cof_native_light head;head.key=540;head.time=70;head.range=696;head.cone=90;head.color[0]=.75f;head.color[1]=.7421875f;head.color[2]=.6484375f;head.angles[1]=270;
 client.time=70;check(nativePublish(&head)!=0,"left headlight");head.key=546;head.origin[0]=44;check(nativePublish(&head)!=0,"right headlight");
 const unsigned beforeNative=sets;begin();apply();end();check(sets==beforeNative+2&&recorded.Type==D3DLIGHT_SPOT&&recorded.Position.x==44&&recorded.Direction.y<-.999f,"two actual submitted headlight cones");
 client.time=70.16;begin();apply();end();check(sets==beforeNative+2,"cutscene toggle/removal expires lights");
 *symbol<IDirect3DDevice9**>("device")=nullptr;
 printf("PASS CAMERA AREA: rectangle-only/no legacy camera spotlight; ignoreViewModel=1 with camera-only classification;120-degree soft forward emission; calibrated central brightness; %u definitions/%u retirements; in-place/unsupported/absent update paths and100ms lifetime verified.\n",area_record::created,area_record::destroyed);
 printf("PASS: %u real camera poses; main-world-only pulse across message/view clock skew; animated camera-tip placement and100ms expiry; checked production photo/headlight pair submissions, movement direction, off/suppression/model change/rollback/stale/reset/failures. No device/window/game.\n",poses);
 if(argc==5){
  // Like verify-camera-bridge, this partial native engine fixture must not
  // execute native DLL detach, which expects a fully initialized real engine.
  fflush(stdout);fflush(stderr);TerminateProcess(GetCurrentProcess(),0);
 }
 return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL CAMERA FLASH: %s\n",e.what());return 1;}
