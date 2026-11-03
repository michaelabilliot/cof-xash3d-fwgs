#include "dx_local.h"
#include <dbghelp.h>
#include <fstream>
#include <stdexcept>
static HANDLE process=GetCurrentProcess();static DWORD64 base;
static ref_client_t client{};static ref_host_t host{};static cvar_t shared{};static void* active;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>static T symbol(const char* name){
 alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};auto info=reinterpret_cast<SYMBOL_INFO*>(storage);
 info->SizeOfStruct=sizeof(SYMBOL_INFO);info->MaxNameLen=MAX_SYM_NAME;
 check(SymFromName(process,name,info)&&info->ModBase==base,name);return reinterpret_cast<T>(static_cast<uintptr_t>(info->Address));
}
static intptr_t parm(int key,int){if(key==PARM_GET_CLIENT_PTR)return reinterpret_cast<intptr_t>(&client);if(key==PARM_GET_HOST_PTR)return reinterpret_cast<intptr_t>(&host);return 0;}
static void reg(convar_t* v){v->value=static_cast<float>(atof(v->string));}
static cvar_t* cvar(const char*){return &shared;}
static void* extra(int,model_t*){return active;}
using Bones=void(*)(model_t*,studiohdr_t*,int,float,BoneMat34*,int,float,float,const cl_entity_t*,float);
struct Bounds{float lo[3]{FLT_MAX,FLT_MAX,FLT_MAX},hi[3]{-FLT_MAX,-FLT_MAX,-FLT_MAX};};
struct Asset{std::vector<char> bytes;model_t model{};cl_entity_t ent{};};
static Bounds posed(Asset& asset,int sequence,float fraction,Bones bones){
 auto hdr=reinterpret_cast<studiohdr_t*>(asset.bytes.data());active=hdr;
 auto seq=reinterpret_cast<mstudioseqdesc_t*>(asset.bytes.data()+hdr->seqindex);BoneMat34 matrices[MAXSTUDIOBONES]{};
 asset.ent.curstate.sequence=sequence;client.time=20+fraction;
 bones(&asset.model,hdr,sequence,std::max(0,seq[sequence].numframes-1)*fraction,matrices,-1,0,0,&asset.ent,static_cast<float>(client.time));
 Bounds out;auto parts=reinterpret_cast<mstudiobodyparts_t*>(asset.bytes.data()+hdr->bodypartindex);
 for(int i=0;i<hdr->numbodyparts;++i){auto models=reinterpret_cast<mstudiomodel_t*>(asset.bytes.data()+parts[i].modelindex);
  for(int j=0;j<parts[i].nummodels;++j){auto& m=models[j];auto vertices=reinterpret_cast<const float(*)[3]>(asset.bytes.data()+m.vertindex);auto indices=reinterpret_cast<const byte*>(asset.bytes.data()+m.vertinfoindex);
   for(int k=0;k<m.numverts;++k){check(indices[k]<hdr->numbones,"vertex bone index");const auto& matrix=matrices[indices[k]];
    for(int axis=0;axis<3;++axis){const float v=matrix[axis][0]*vertices[k][0]+matrix[axis][1]*vertices[k][1]+matrix[axis][2]*vertices[k][2]+matrix[axis][3];check(std::isfinite(v),"finite posed vertex");out.lo[axis]=std::min(out.lo[axis],v);out.hi[axis]=std::max(out.hi[axis],v);}
   }
  }
 }return out;
}
static float extent(const Bounds& b){float n=0;for(int axis=0;axis<3;++axis)n+=std::pow(b.hi[axis]-b.lo[axis],2.f);return sqrtf(n);}
int main(int argc,char** argv)try{
 check(argc==5,"renderer, backup model, original Steam model, patched model arguments");auto dll=LoadLibraryA(argv[1]);check(dll!=nullptr,"renderer");
 SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_EXACT_SYMBOLS);check(SymInitialize(process,nullptr,FALSE)!=FALSE,"symbols");const std::string path=argv[1];SymSetSearchPath(process,path.substr(0,path.find_last_of("/\\")).c_str());
 base=SymLoadModuleEx(process,nullptr,argv[1],nullptr,reinterpret_cast<DWORD64>(dll),0,nullptr,0);check(base!=0,"matching PDB");
 using API=int(*)(int,ref_interface_t*,ref_api_t*,ref_globals_t*);auto api=reinterpret_cast<API>(GetProcAddress(dll,"GetRefAPI"));ref_api_t engine{};ref_interface_t functions{};ref_globals_t globals{};
 engine.EngineGetParm=parm;engine.Cvar_RegisterVariable=reg;engine.pfnGetCvarPointer=cvar;engine.Mod_Extradata=extra;
 check(api&&api(REF_API_VERSION,&functions,&engine,&globals)==REF_API_VERSION,"API");auto bones=symbol<Bones>("StudioCalcBoneTransformsInternal");
 auto world=symbol<void*>("R_BuildViewModelWorld");printf("Current R_BuildViewModelWorld RVA: 0x%llx\n",static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(world)-base));
 Asset assets[3];
 for(int i=0;i<3;++i){std::ifstream file(argv[i+2],std::ios::binary);assets[i].bytes.assign(std::istreambuf_iterator<char>(file),{});check(assets[i].bytes.size()>sizeof(studiohdr_t),"camera model");
  assets[i].model.type=mod_studio;strcpy_s(assets[i].model.name,"models/weapons/camera/v_camera.mdl");assets[i].ent.model=&assets[i].model;assets[i].ent.curstate.framerate=1;
 }
 auto hdr=reinterpret_cast<studiohdr_t*>(assets[0].bytes.data());unsigned poses=0;float minRatio=FLT_MAX,maxRatio=0;
 for(int sequence=0;sequence<hdr->numseq;++sequence)for(float fraction:{0.f,.25f,.5f,.75f,.99f}){
  const auto old=posed(assets[0],sequence,fraction,bones),steam=posed(assets[1],sequence,fraction,bones),patched=posed(assets[2],sequence,fraction,bones);
  const float ratio=extent(patched)/extent(old),broken=extent(steam)/extent(old);minRatio=std::min(minRatio,ratio);maxRatio=std::max(maxRatio,ratio);
  check(broken>8.f&&broken<12.f,"negative control reproduces giant Steam camera");check(ratio>.8f&&ratio<1.2f,"patched camera matches working backup spatial size");
  for(int axis=0;axis<3;++axis){check(fabsf(patched.lo[axis]-.1f*steam.lo[axis])<.002f&&fabsf(patched.hi[axis]-.1f*steam.hi[axis])<.002f,"full posed geometry uniformly converted, including bone translations");}
  ++poses;
 }
 printf("PASS: %u complete skinned camera poses; original Steam model reproduces 10x regression, patched/working-backup extent ratio %.6f..%.6f; all posed bounds equal 0.1x source. No game/window/device.\n",poses,minRatio,maxRatio);return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL VIEWMODEL UNITS: %s\n",error.what());return 1;}
