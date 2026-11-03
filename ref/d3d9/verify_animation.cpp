#include "dx_local.h"
#include "dx_cof_dual.h"
#include <dbghelp.h>
#include <fstream>
#include <stdexcept>
#include <limits>
static HANDLE process=GetCurrentProcess();
static DWORD64 moduleBase;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class T> static T symbol(const char* name){
 alignas(SYMBOL_INFO) char bytes[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};
 auto info=reinterpret_cast<SYMBOL_INFO*>(bytes);info->SizeOfStruct=sizeof(SYMBOL_INFO);info->MaxNameLen=MAX_SYM_NAME;
 require(SymFromName(process,name,info)&&info->ModBase==moduleBase,name);
 return reinterpret_cast<T>(static_cast<uintptr_t>(info->Address));
}
static ref_client_t client{};static ref_host_t host{};static cvar_t shared{};
static std::vector<char> bytes;
static intptr_t parm(int key,int){if(key==PARM_GET_CLIENT_PTR)return reinterpret_cast<intptr_t>(&client);if(key==PARM_GET_HOST_PTR)return reinterpret_cast<intptr_t>(&host);return 0;}
static void reg(convar_t* value){value->value=static_cast<float>(atof(value->string));}
static cvar_t* cvar(const char*){return &shared;}
static void* extra(int,model_t*){return bytes.data();}
int main(int argc,char** argv)try{
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);require(argc==2||argc==3,"renderer and optional model root arguments");
 const auto dll=LoadLibraryA(argv[1]);require(dll!=nullptr,"renderer load");
 SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_EXACT_SYMBOLS);require(SymInitialize(process,nullptr,FALSE)!=FALSE,"symbols");
 const std::string path=argv[1];const auto folder=path.substr(0,path.find_last_of("/\\"));SymSetSearchPath(process,folder.c_str());
 moduleBase=SymLoadModuleEx(process,nullptr,argv[1],nullptr,reinterpret_cast<DWORD64>(dll),0,nullptr,0);require(moduleBase!=0,"matching PDB");
 using API=int(*)(int,ref_interface_t*,ref_api_t*,ref_globals_t*);auto api=reinterpret_cast<API>(GetProcAddress(dll,"GetRefAPI"));
 ref_api_t engine{};ref_interface_t functions{};ref_globals_t globals{};engine.EngineGetParm=parm;engine.Cvar_RegisterVariable=reg;engine.pfnGetCvarPointer=cvar;engine.Mod_Extradata=extra;
 require(api&&api(REF_API_VERSION,&functions,&engine,&globals)==REF_API_VERSION,"API");
 auto budget=symbol<bool(*)(bool,studio_cache_t&)>("R_StudioPoseBudgetOK");auto counter=symbol<int*>("g_studioPoseUpdatesThisFrame");
 symbol<convar_t*>("dx_studio_update_budget")->value=8;
 std::vector<studio_cache_t> caches(24);for(auto& cache:caches)cache.poseUploaded=true;
 unsigned counts[24]{},misses[24]{},longest=0;bool hasEpoch=true;void(*begin)()=nullptr;
 try{begin=symbol<void(*)()>("R_BeginStudioPoseFrame");}catch(const std::exception&){hasEpoch=false;}
 for(unsigned frame=0;frame<120;++frame){if(begin)begin();else *counter=0;
  for(unsigned i=0;i<caches.size();++i){if(budget(false,caches[i])){++counts[i];misses[i]=0;}else{++misses[i];longest=std::max(longest,misses[i]);}}
 }
 printf("ANIMATION BUDGET: first=%u middle=%u last=%u updates/120 frames; longest denial=%u\n",counts[0],counts[12],counts[23],longest);
 require(counts[23]>0&&longest<=1,"draw-order pose starvation: later models never update");require(hasEpoch,"frame epoch");
 // A repeat pass cannot turn an initial same-frame denial into permission.
 begin();*counter=8;studio_cache_t deferred{};deferred.poseUploaded=true;require(!budget(false,deferred)&&!budget(false,deferred),"same-frame recovery forbidden");
 begin();*counter=8;require(budget(false,deferred),"next-visible-frame recovery despite busy quota");
 symbol<convar_t*>("dx_studio_update_budget")->value=0;require(budget(false,deferred),"unlimited remains unlimited");
 symbol<convar_t*>("dx_studio_update_budget")->value=8;*counter=8;studio_cache_t initial{};require(budget(false,initial),"initial upload exempt");
 initial.poseUploaded=true;require(budget(true,initial),"unthrottled viewmodel exempt");initial.gpuSkinningBufferMode=true;symbol<convar_t*>("dx_studio_gpu_smooth")->value=1;require(budget(false,initial),"smooth GPU exempt");
 if(argc==3){
  using Advance=float(*)(studiohdr_t*,cl_entity_t*,float);auto advance=symbol<Advance>("StudioFrameAdvance");
  using Bones=void(*)(model_t*,studiohdr_t*,int,float,BoneMat34*,int,float,float,const cl_entity_t*,float);auto bones=symbol<Bones>("StudioCalcBoneTransformsInternal");
  const char* names[]={"boat.mdl","cutscene/simon_talk2.mdl","cutscene/sicksimond.mdl"};const int sequences[]={1,27,1};
  for(int modelIndex=0;modelIndex<3;++modelIndex){
   std::ifstream file(std::string(argv[2])+"/"+names[modelIndex],std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(file),{});require(bytes.size()>sizeof(studiohdr_t),"owned animation model");
   auto hdr=reinterpret_cast<studiohdr_t*>(bytes.data());require(hdr->numbones>0&&hdr->numbones<=MAXSTUDIOBONES&&hdr->numseq>sequences[modelIndex],"native sequence/bone count");
   model_t model{};model.type=mod_studio;cl_entity_t entity{};entity.model=&model;entity.curstate.sequence=sequences[modelIndex];entity.curstate.framerate=1;
   studio_cache_t cache{};cache.poseUploaded=true;BoneMat34 previous[MAXSTUDIOBONES]{},current[MAXSTUDIOBONES]{};unsigned changes=0,updates=0;
   for(int frame=0;frame<120;++frame){
    begin();*counter=8;client.time=frame/60.f;
    if(!budget(false,cache))continue;
    const float poseFrame=advance(hdr,&entity,static_cast<float>(client.time));
    bones(&model,hdr,entity.curstate.sequence,poseFrame,current,-1,0,0,&entity,static_cast<float>(client.time));
    if(updates&&memcmp(current,previous,static_cast<size_t>(hdr->numbones)*sizeof(BoneMat34)))++changes;
    memcpy(previous,current,static_cast<size_t>(hdr->numbones)*sizeof(BoneMat34));++updates;
   }
   printf("ANIMATION POSE: %s seq=%d uploads=%u changes=%u despite saturated budget\n",names[modelIndex],entity.curstate.sequence,updates,changes);
   require(updates>=59&&changes>3,"boat/cutscene bones advance through recovery gate");
  }
 }
 puts("PASS: actual compiled pose-budget gate,24 animated caches x120 busy frames, bounded denial, repeated passes, unlimited/initial/viewmodel/GPU exemptions. No GPU/window/game.");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
