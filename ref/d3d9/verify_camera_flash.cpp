#include "dx_local.h"
#include "dx_camera_flash.h"
#include <dbghelp.h>
#include <fstream>
#include <stdexcept>
#include <limits>
static HANDLE process=GetCurrentProcess();static DWORD64 base;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>static T symbol(const char* name){
 alignas(SYMBOL_INFO) char bytes[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};
 auto info=reinterpret_cast<SYMBOL_INFO*>(bytes);info->SizeOfStruct=sizeof(SYMBOL_INFO);info->MaxNameLen=MAX_SYM_NAME;
 check(SymFromName(process,name,info)&&info->ModBase==base,name);return reinterpret_cast<T>(static_cast<uintptr_t>(info->Address));
}
static ref_client_t client{};static ref_host_t host{};static cvar_t shared{};
static cl_entity_t vm{};static std::vector<char> modelBytes;
static intptr_t parm(int key,int){
 if(key==PARM_GET_CLIENT_PTR)return reinterpret_cast<intptr_t>(&client);
 if(key==PARM_GET_HOST_PTR)return reinterpret_cast<intptr_t>(&host);
 if(key==PARM_GET_VIEWENT_PTR)return reinterpret_cast<intptr_t>(&vm);
 if(key==PARM_LOCAL_HEALTH)return 100;return 0;
}
static void reg(convar_t* v){v->value=static_cast<float>(atof(v->string));}
static cvar_t* cvar(const char*){return &shared;}
static void* extra(int,model_t*){return modelBytes.data();}
struct Device{void** methods;};static unsigned sets=0,on=0,off=0;static bool failSet=false,failEnable=false;static D3DLIGHT9 recorded{};
static HRESULT STDMETHODCALLTYPE setLight(Device*,DWORD slot,const D3DLIGHT9* light){check(slot==0,"photo light slot");++sets;recorded=*light;return failSet?E_FAIL:S_OK;}
static HRESULT STDMETHODCALLTYPE enable(Device*,DWORD slot,BOOL value){check(slot==0,"enable slot");if(value)++on;else ++off;return value&&failEnable?E_FAIL:S_OK;}
int main(int argc,char** argv)try{
 check(argc==3,"renderer and camera model arguments");
 std::ifstream file(argv[2],std::ios::binary);modelBytes.assign(std::istreambuf_iterator<char>(file),{});check(modelBytes.size()>sizeof(studiohdr_t),"camera model");
 auto dll=LoadLibraryA(argv[1]);check(dll!=nullptr,"renderer");
 SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_EXACT_SYMBOLS);check(SymInitialize(process,nullptr,FALSE)!=FALSE,"symbols");
 const std::string path=argv[1];SymSetSearchPath(process,path.substr(0,path.find_last_of("/\\")).c_str());
 base=SymLoadModuleEx(process,nullptr,argv[1],nullptr,reinterpret_cast<DWORD64>(dll),0,nullptr,0);check(base!=0,"matching PDB");
 using API=int(*)(int,ref_interface_t*,ref_api_t*,ref_globals_t*);
 auto api=reinterpret_cast<API>(GetProcAddress(dll,"GetRefAPI"));ref_api_t engine{};ref_interface_t functions{};ref_globals_t globals{};
 engine.EngineGetParm=parm;engine.Cvar_RegisterVariable=reg;engine.pfnGetCvarPointer=cvar;engine.Mod_Extradata=extra;
 check(api&&api(REF_API_VERSION,&functions,&engine,&globals)==REF_API_VERSION,"API");
 model_t model{};model.type=mod_studio;strcpy_s(model.name,"models/weapons/camera/v_camera.mdl");vm.model=&model;vm.curstate.framerate=1;vm.curstate.sequence=3;
 client.maxclients=1;client.playernum=0;client.viewentity=1;client.time=10;shared.value=1;
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
 check(fabsf(moved[0]-point[0]-2)<.01f&&fabsf(moved[1]-point[1]+3)<.01f&&fabsf(moved[2]-point[2]-4)<.01f,"viewmodel offsets shared with geometry");
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
 dx_cof_light_state_v2 packet;packet.direction[0]=1;packet.time=10;check(publish(&packet)!=0,"zero-toggle light publication");
 D3DLIGHT9 light{};check(!describe(&light),"no light from draw/equip alone");check(trigger(10)&&!trigger(10),"one trigger per exposure timestamp");
 check(describe(&light)&&light.Type==D3DLIGHT_SPOT&&light.Range==600&&light.Diffuse.r>.79f&&light.Direction.x==1,"white photo spotlight");
 check(pose(&vm,10,zero,zero,zero,point)!=0,"expected emitter");check(fabsf(light.Position.x-point[0])<.01f&&fabsf(light.Position.y-point[1])<.01f&&fabsf(light.Position.z-point[2])<.01f,"light at animated flash bone");
 void* methods[119]{};methods[51]=reinterpret_cast<void*>(setLight);methods[53]=reinterpret_cast<void*>(enable);Device recorder{methods};*symbol<IDirect3DDevice9**>("device")=reinterpret_cast<IDirect3DDevice9*>(&recorder);
 auto apply=symbol<void(*)()>("R_ApplyDLights");apply();check(sets==1&&on==1&&off==0,"production light submission");
 client.time=10.075;packet.time=static_cast<float>(client.time);publish(&packet);apply();check(sets==2&&recorded.Diffuse.r>0&&recorded.Diffuse.r<.3f,"short discharge tail");
 client.time=10.11;apply();check(sets==2&&off==1&&!describe(&light),"expiry disables submitted light");
 for(int i=0;i<6;++i){client.time=20+i;packet.time=static_cast<float>(client.time);packet.reasons=0;publish(&packet);check(trigger(packet.time)!=0,"repeated accepted photos");
  if(i==0)vm.curstate.effects=EF_NODRAW;
  if(i==1)strcpy_s(model.name,"models/weapons/glock/v_glock.mdl");
  if(i==2)packet.reasons=DX_COF_ATTACHED;
  if(i==3)client.time-=1;
  if(i==4)client.time+=.3;
  if(i==5)symbol<convar_t*>("dx_cof_camera_flash")->value=0;
  if(i==2)publish(&packet);check(!describe(&light),"hidden/replaced/suppressed/rollback/stale/off clears pulse");
  vm.curstate.effects=0;strcpy_s(model.name,"models/weapons/camera/v_camera.mdl");symbol<convar_t*>("dx_cof_camera_flash")->value=1;
 }
 packet.reasons=0;client.time=50;packet.time=50;publish(&packet);check(trigger(50)!=0,"failure pulse");
 failSet=true;apply();check(off>=2,"failed SetLight disables slot");failSet=false;failEnable=true;apply();check(off>=3,"failed LightEnable disables slot");failEnable=false;
 symbol<void(*)()>("R_ResetCofLights")();check(!describe(&light),"map/device reset clears pulse");
 check(!trigger(std::numeric_limits<float>::quiet_NaN()),"NaN trigger rejected");
 *symbol<IDirect3DDevice9**>("device")=nullptr;
 printf("PASS: %u real camera poses; native-shot pulse without toggle flags, flash-bone placement, checked production light sink, discharge/expiry/off/suppression/model change/rollback/stale/reset/failures. No device/window/game.\n",poses);return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL CAMERA FLASH: %s\n",e.what());return 1;}
