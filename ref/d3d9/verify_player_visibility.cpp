#include "dx_local.h"
#include "dx_cof_scene.h"
#include <dbghelp.h>
#include <stdexcept>
#include <limits>
static HANDLE process=GetCurrentProcess();static DWORD64 base;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>static T symbol(const char* name){
 alignas(SYMBOL_INFO) char bytes[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};
 auto info=reinterpret_cast<SYMBOL_INFO*>(bytes);info->SizeOfStruct=sizeof(SYMBOL_INFO);info->MaxNameLen=MAX_SYM_NAME;
 check(SymFromName(process,name,info)&&info->ModBase==base,name);return reinterpret_cast<T>(static_cast<uintptr_t>(info->Address));
}
static ref_client_t client{};static ref_host_t host{};static cvar_t shared{};static unsigned geometryQueries=0;
static intptr_t parm(int key,int){if(key==PARM_GET_CLIENT_PTR)return reinterpret_cast<intptr_t>(&client);if(key==PARM_GET_HOST_PTR)return reinterpret_cast<intptr_t>(&host);return 0;}
static void reg(convar_t* v){v->value=static_cast<float>(atof(v->string));}
static cvar_t* cvar(const char*){return &shared;}
// Deliberately stop accepted geometry at its model-data query, before any GPU work.
static void* extra(int,model_t*){++geometryQueries;return nullptr;}
int main(int argc,char** argv)try{
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 check(argc==2,"renderer argument");auto dll=LoadLibraryA(argv[1]);check(dll!=nullptr,"renderer");
 SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_EXACT_SYMBOLS);check(SymInitialize(process,nullptr,FALSE)!=FALSE,"symbols");
 const std::string path=argv[1];SymSetSearchPath(process,path.substr(0,path.find_last_of("/\\")).c_str());
 base=SymLoadModuleEx(process,nullptr,argv[1],nullptr,reinterpret_cast<DWORD64>(dll),0,nullptr,0);check(base!=0,"matching PDB");
 using API=int(*)(int,ref_interface_t*,ref_api_t*,ref_globals_t*);
 auto api=reinterpret_cast<API>(GetProcAddress(dll,"GetRefAPI"));ref_api_t engine{};ref_interface_t functions{};ref_globals_t globals{};
 engine.EngineGetParm=parm;engine.Cvar_RegisterVariable=reg;engine.pfnGetCvarPointer=cvar;engine.Mod_Extradata=extra;
 check(api&&api(REF_API_VERSION,&functions,&engine,&globals)==REF_API_VERSION,"API");
 using Publish=int(__cdecl*)(unsigned,float);auto publish=reinterpret_cast<Publish>(GetProcAddress(dll,"DX_CofPlayerVisibilityV1"));
 auto draw=symbol<void(*)(cl_entity_t*)>("R_DrawStudioEntity");
 model_t model{};model.type=mod_studio;strcpy_s(model.name,"models/player/police1/police1.mdl");
 model_drawdata_t data{};data.mod=&model;
 symbol<void(*)(model_t*,model_drawdata_t*)>("R_RegisterStudioModelDrawData")(&model,&data);
 auto deviceSlot=symbol<IDirect3DDevice9**>("device");*deviceSlot=reinterpret_cast<IDirect3DDevice9*>(1);
 client.time=10;client.maxclients=2;client.playernum=0;
 auto reaches=[&](bool player,int fx,unsigned expected){
  cl_entity_t entity{};entity.model=&model;entity.player=player;entity.index=1;entity.curstate.renderfx=fx;
  geometryQueries=0;draw(&entity);draw(&entity);
  check(geometryQueries==expected,"player geometry reached model query during native-hidden scene");
 };
 // Before the correction this deliberately fails: the native-hidden player still
 // reaches geometry twice even though its scene actor is separate.
 if(publish)check(publish(2,10)!=0,"attached-camera publication");
 reaches(true,0,0);
 check(publish!=nullptr,"versioned visibility bridge");
 for(unsigned reasons:{1u,2u,3u}){
  check(publish(reasons,10)!=0,"scene reason publication");
  reaches(true,0,0);reaches(false,kRenderFxDeadPlayer,0);
  reaches(false,0,2); // Identical police model as an NPC must remain visible.
 }
 check(publish(0,10)!=0,"scene exit");reaches(true,0,2);reaches(false,kRenderFxDeadPlayer,2);
 // The engine owns ordinary first/third-person visibility; this bridge adds no
 // health, first-person, model-path or player-index restrictions of its own.
 for(int view:{0,1,42}){client.viewentity=view;publish(0,10);reaches(true,0,2);}
 publish(2,10);client.time=10.3;reaches(true,0,2);
 client.time=9;publish(2,10);reaches(true,0,2);
 client.time=10;publish(2,10);check(!publish(4,10),"unknown flags rejected");reaches(true,0,2);
 publish(2,10);check(!publish(0,std::numeric_limits<float>::quiet_NaN()),"shutdown/invalid clears");reaches(true,0,2);
 publish(2,10);*deviceSlot=nullptr;check(!symbol<bool(*)(bool)>("DX_ResetDevice")(false),"device-less reset");
 *deviceSlot=reinterpret_cast<IDirect3DDevice9*>(1);reaches(true,0,2);
 publish(2,10);symbol<void(*)()>("R_ResetCofPlayerVisibility")();reaches(true,0,2);
 *deviceSlot=nullptr;
 puts("PASS: compiled studio draw boundary rejects native-hidden players/dead-player delegates across repeated passes, preserves identical-model NPC actors, restores ordinary views, expires stale/rollback state, and clears invalid/shutdown/device-reset/map-reset state. Recording model queries; no graphics/window/game.");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL PLAYER VISIBILITY: %s\n",e.what());return 1;}
