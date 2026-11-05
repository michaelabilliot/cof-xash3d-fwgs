#include "dx_local.h"
#include <dbghelp.h>
#include <fstream>
#include <stdexcept>
static HANDLE process=GetCurrentProcess();static DWORD64 base;
static ref_client_t client{};static ref_host_t host{};static cvar_t shared{};
static std::vector<char> bytes;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class T>static T symbol(const char* name){
 alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};auto info=reinterpret_cast<SYMBOL_INFO*>(storage);
 info->SizeOfStruct=sizeof(SYMBOL_INFO);info->MaxNameLen=MAX_SYM_NAME;
 check(SymFromName(process,name,info)&&info->ModBase==base,name);return reinterpret_cast<T>(static_cast<uintptr_t>(info->Address));
}
static intptr_t parm(int key,int){if(key==PARM_GET_CLIENT_PTR)return reinterpret_cast<intptr_t>(&client);if(key==PARM_GET_HOST_PTR)return reinterpret_cast<intptr_t>(&host);return 0;}
static void reg(convar_t* value){value->value=static_cast<float>(atof(value->string));}
static cvar_t* cvar(const char*){return &shared;}
static void* extra(int,model_t*){return bytes.data();}
int main(int argc,char** argv)try{
 check(argc==3,"renderer DLL and model arguments");auto dll=LoadLibraryA(argv[1]);check(dll!=nullptr,"renderer");
 SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_EXACT_SYMBOLS);check(SymInitialize(process,nullptr,FALSE)!=FALSE,"symbols");
 const std::string path=argv[1];SymSetSearchPath(process,path.substr(0,path.find_last_of("/\\")).c_str());
 base=SymLoadModuleEx(process,nullptr,argv[1],nullptr,reinterpret_cast<DWORD64>(dll),0,nullptr,0);check(base!=0,"matching PDB");
 using API=int(*)(int,ref_interface_t*,ref_api_t*,ref_globals_t*);auto api=reinterpret_cast<API>(GetProcAddress(dll,"GetRefAPI"));
 ref_api_t engine{};ref_interface_t functions{};ref_globals_t globals{};
 engine.EngineGetParm=parm;engine.Cvar_RegisterVariable=reg;engine.pfnGetCvarPointer=cvar;engine.Mod_Extradata=extra;
 check(api&&api(REF_API_VERSION,&functions,&engine,&globals)==REF_API_VERSION,"API");
 std::ifstream file(argv[2],std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(file),{});check(bytes.size()>sizeof(studiohdr_t),"model");
 auto hdr=reinterpret_cast<studiohdr_t*>(bytes.data());check(hdr->numattachments>0&&hdr->numattachments<=4,"model attachments");
 model_t model{};model.type=mod_studio;strcpy_s(model.name,"models/booksimon.mdl");
 cl_entity_t ent{};ent.model=&model;ent.index=100;ent.curstate.number=100;ent.curstate.framerate=1;
 using Bones=void(*)(model_t*,studiohdr_t*,int,float,BoneMat34*,int,float,float,const cl_entity_t*,float);
 using Store=void(*)(cl_entity_t*,studiohdr_t*,const BoneMat34*,const D3DMATRIX&,float);
 using Point=void(*)(studiohdr_t*,const BoneMat34*,const D3DMATRIX&,int,float*);
 auto bones=symbol<Bones>("StudioCalcBoneTransformsInternal");auto store=symbol<Store>("R_StudioStoreAttachments");auto point=symbol<Point>("R_StudioAttachmentPoint");
 // Initialize pose cvars using the production CPU query.
 using Query=int(__cdecl*)(cl_entity_t*,float,float*,int);auto query=reinterpret_cast<Query>(GetProcAddress(dll,"DX_StudioCameraAttachmentsV3"));
 float bootstrap[12];check(query&&query(&ent,0,bootstrap,1),"pose initialization");
 auto seq=reinterpret_cast<mstudioseqdesc_t*>(bytes.data()+hdr->seqindex);unsigned samples=0;bool away=false;
 for(int sequence=0;sequence<hdr->numseq;++sequence)for(float fraction:{0.f,.25f,.5f,.75f,.99f}){
  ent.curstate.sequence=sequence;client.time=20+fraction;BoneMat34 matrices[MAXSTUDIOBONES]{};
  bones(&model,hdr,sequence,std::max(0,seq[sequence].numframes-1)*fraction,matrices,-1,0,0,&ent,static_cast<float>(client.time));
  for(float yaw:{0.f,90.f}){
   D3DXMATRIX world;D3DXMatrixRotationZ(&world,yaw*.0174532925199433f);world._41=ent.origin[0]=123;world._42=ent.origin[1]=-456;world._43=ent.origin[2]=78;
   // Reproduce the engine's per-frame origin reset before rendering.
   for(auto& attachment:ent.attachment)VectorCopy(ent.origin,attachment);
   store(&ent,hdr,matrices,world,static_cast<float>(client.time));
   for(int i=0;i<hdr->numattachments;++i){float expected[3];point(hdr,matrices,world,i,expected);
    for(int axis=0;axis<3;++axis){check(std::isfinite(ent.attachment[i][axis]),"finite attachment");
     check(fabsf(ent.attachment[i][axis]-expected[axis])<.001f,"public client attachment must follow bone instead of entity origin");
     if(fabsf(expected[axis]-ent.origin[axis])>1)away=true;}}
   ++samples;
  }
 }
 check(away,"hand away from entity origin");
 auto attachments=reinterpret_cast<mstudioattachment_t*>(bytes.data()+hdr->attachmentindex);const int bone=attachments[0].bone;attachments[0].bone=hdr->numbones;
 ent.attachment[0][0]=999;BoneMat34 matrices[MAXSTUDIOBONES]{};D3DMATRIX world{};store(&ent,hdr,matrices,world,static_cast<float>(client.time));
 check(ent.attachment[0][0]==999,"invalid bone preserves public position");attachments[0].bone=bone;
 printf("PASS: %u animated poses; public attachments follow bones after origin reset, translation/yaw; invalid bone rejected. No game/device.\n",samples);return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL ATTACHMENTS: %s\n",error.what());return 1;}
