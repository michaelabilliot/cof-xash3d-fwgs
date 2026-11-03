#include "dx_local.h"
#include <dbghelp.h>
#include <fstream>
#include <stdexcept>

static HANDLE process=GetCurrentProcess();static DWORD64 base;
static ref_client_t client{};static ref_host_t host{};static cvar_t shared{};
static std::vector<char> bytes;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>static T symbol(const char* name){
 alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};
 auto info=reinterpret_cast<SYMBOL_INFO*>(storage);info->SizeOfStruct=sizeof(SYMBOL_INFO);info->MaxNameLen=MAX_SYM_NAME;
 check(SymFromName(process,name,info)&&info->ModBase==base,name);return reinterpret_cast<T>(static_cast<uintptr_t>(info->Address));
}
static intptr_t parm(int key,int){if(key==PARM_GET_CLIENT_PTR)return reinterpret_cast<intptr_t>(&client);if(key==PARM_GET_HOST_PTR)return reinterpret_cast<intptr_t>(&host);return 0;}
static void reg(convar_t* v){v->value=static_cast<float>(atof(v->string));}
static cvar_t* cvar(const char*){return &shared;}
static void* extra(int,model_t*){return bytes.data();}
int main(int argc,char** argv)try{
 check(argc==3,"renderer and model list arguments");auto dll=LoadLibraryA(argv[1]);check(dll!=nullptr,"renderer load");
 SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_EXACT_SYMBOLS);check(SymInitialize(process,nullptr,FALSE)!=FALSE,"symbols");
 const std::string path=argv[1];SymSetSearchPath(process,path.substr(0,path.find_last_of("/\\")).c_str());
 base=SymLoadModuleEx(process,nullptr,argv[1],nullptr,reinterpret_cast<DWORD64>(dll),0,nullptr,0);check(base!=0,"matching PDB");
 using API=int(*)(int,ref_interface_t*,ref_api_t*,ref_globals_t*);auto api=reinterpret_cast<API>(GetProcAddress(dll,"GetRefAPI"));
 ref_api_t engine{};ref_interface_t functions{};ref_globals_t globals{};engine.EngineGetParm=parm;engine.Cvar_RegisterVariable=reg;engine.pfnGetCvarPointer=cvar;engine.Mod_Extradata=extra;
 check(api&&api(REF_API_VERSION,&functions,&engine,&globals)==REF_API_VERSION,"API");
 using Bones=void(*)(model_t*,studiohdr_t*,int,float,BoneMat34*,int,float,float,const cl_entity_t*,float);
 auto bones=symbol<Bones>("StudioCalcBoneTransformsInternal");
 std::ifstream list(argv[2]);std::string filename;unsigned models=0,poses=0,attachments=0;
 while(std::getline(list,filename)){
  if(!filename.empty()&&filename.back()=='\r')filename.pop_back();if(filename.empty())continue;
  std::ifstream input(filename,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(input),{});
  check(bytes.size()>sizeof(studiohdr_t),filename.c_str());auto hdr=reinterpret_cast<studiohdr_t*>(bytes.data());
  check(hdr->numbones>0&&hdr->numbones<=MAXSTUDIOBONES&&hdr->numseq>0,"bone/sequence contract");
  model_t model{};model.type=mod_studio;strcpy_s(model.name,"overlay_fixture.mdl");cl_entity_t entity{};entity.model=&model;entity.curstate.framerate=1;
  auto sequences=reinterpret_cast<mstudioseqdesc_t*>(bytes.data()+hdr->seqindex);
  for(int sequence=0;sequence<hdr->numseq;++sequence)for(float fraction:{0.f,.25f,.5f,.9f}){
   entity.curstate.sequence=sequence;client.time=20+fraction;entity.curstate.animtime=20;
   BoneMat34 transforms[MAXSTUDIOBONES]{};
   bones(&model,hdr,sequence,std::max(0,sequences[sequence].numframes-1)*fraction,transforms,-1,0,0,&entity,static_cast<float>(client.time));
   for(int bone=0;bone<hdr->numbones;++bone){const float* matrix=reinterpret_cast<const float*>(&transforms[bone]);for(int j=0;j<12;++j)check(std::isfinite(matrix[j]),"finite production bone pose");
    const float length=matrix[0]*matrix[0]+matrix[1]*matrix[1]+matrix[2]*matrix[2];check(length>.5f&&length<1.5f,"nondegenerate production bone transform");}
   auto a=reinterpret_cast<mstudioattachment_t*>(bytes.data()+hdr->attachmentindex);
   for(int i=0;i<hdr->numattachments;++i){check(a[i].bone>=0&&a[i].bone<hdr->numbones,"attachment bone");const float* matrix=reinterpret_cast<const float*>(&transforms[a[i].bone]);
    for(int axis=0;axis<3;++axis){const float v=matrix[axis*4]*a[i].org[0]+matrix[axis*4+1]*a[i].org[1]+matrix[axis*4+2]*a[i].org[2]+matrix[axis*4+3];check(std::isfinite(v),"finite transformed attachment");}++attachments;}
   ++poses;
  }++models;
 }
 check(models>0,"nonempty model list");printf("PASS: %u installed models, %u production animation poses, %u transformed attachments; finite bones/attachments across all sequences. No device/window/game.\n",models,poses,attachments);return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL ASSET OVERLAY: %s\n",error.what());return 1;}
