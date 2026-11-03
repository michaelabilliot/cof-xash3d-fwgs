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
struct RecordingDevice{void** methods;};
static HRESULT STDMETHODCALLTYPE transform(RecordingDevice*,D3DTRANSFORMSTATETYPE,D3DMATRIX* out){*out={};out->_11=out->_22=out->_33=out->_44=1;return S_OK;}
static HRESULT STDMETHODCALLTYPE rejectVB(RecordingDevice*,UINT,DWORD,DWORD,D3DPOOL,IDirect3DVertexBuffer9** out,HANDLE*){*out=nullptr;return E_FAIL;}
int main(int argc,char** argv)try{
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);require(argc==3||argc==4,"renderer and model root arguments");
 const bool oldProbe=argc==4;
 auto dll=LoadLibraryA(argv[1]);require(dll!=nullptr,"load renderer");
 SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_EXACT_SYMBOLS);require(SymInitialize(process,nullptr,FALSE)!=FALSE,"initialize symbols");
 const std::string dllPath=argv[1];const auto folder=dllPath.substr(0,dllPath.find_last_of("/\\"));require(SymSetSearchPath(process,folder.c_str())!=FALSE,"matching retained symbol folder");
 moduleBase=SymLoadModuleEx(process,nullptr,argv[1],nullptr,reinterpret_cast<DWORD64>(dll),0,nullptr,0);require(moduleBase!=0,"matching PDB");
 using API=int(*)(int,ref_interface_t*,ref_api_t*,ref_globals_t*);
 auto api=reinterpret_cast<API>(GetProcAddress(dll,"GetRefAPI"));ref_api_t engine{};ref_interface_t functions{};ref_globals_t globals{};
 engine.EngineGetParm=parm;engine.Cvar_RegisterVariable=reg;engine.pfnGetCvarPointer=cvar;engine.Mod_Extradata=extra;
 require(api&&api(REF_API_VERSION,&functions,&engine,&globals)==REF_API_VERSION,"initialize API without device");
 auto publish=reinterpret_cast<int(__cdecl*)(const cl_entity_t*,int,float)>(GetProcAddress(dll,"DX_CofDualViewModelV1"));require(publish!=nullptr,"compiled V1 export");
 auto state=symbol<dx_cof_dual_state_t*>("g_cofDual");
 using World=void(*)(const cl_entity_s*,const float*,const float*,const float*,const float*,const float*,float,float,float,D3DMATRIX&,bool);
 auto world=symbol<World>("R_BuildViewModelWorld");
 using Skin=void(*)(model_t*,studiohdr_t*,int,float,model_drawdata_t&,int,float,float,int,int,const cl_entity_s*,float,D3DCOLOR,const float*,float,const D3DMATRIX*,const BoneMat34*,int);
 auto skin=symbol<Skin>("StudioSkinModelInternal");
 auto advance=symbol<float(*)(studiohdr_t*,cl_entity_s*,float)>("StudioFrameAdvance");
 auto textures=symbol<std::deque<dxtexture_t>*>("g_Textures");auto textureCount=symbol<int*>("g_texcount");
 void* methods[119]{};methods[45]=reinterpret_cast<void*>(transform);methods[26]=reinterpret_cast<void*>(rejectVB);RecordingDevice recorder{methods};*symbol<IDirect3DDevice9**>("device")=reinterpret_cast<IDirect3DDevice9*>(&recorder);
 const char* names[]={"glock/v_glock.mdl","p345/v_p345.mdl","vp70/v_vp70.mdl","mobile/v_mobile.mdl","flashlight/v_flashlight.mdl"};
 unsigned poses=0;size_t triangles=0;uint64_t pass=0;
 for(const char* name:names){
  std::ifstream file(std::string(argv[2])+"/weapons/"+name,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(file),{});require(bytes.size()>sizeof(studiohdr_t),name);
  auto hdr=reinterpret_cast<studiohdr_t*>(bytes.data());require(hdr->length==static_cast<int>(bytes.size())&&hdr->numtextures>0,"owned model header");
  textures->resize(static_cast<size_t>(hdr->numtextures)+1);*textureCount=hdr->numtextures;
  auto nativeTextures=reinterpret_cast<mstudiotexture_t*>(bytes.data()+hdr->textureindex);
  for(int i=0;i<hdr->numtextures;++i){nativeTextures[i].index=i+1;(*textures)[i+1].width=nativeTextures[i].width;(*textures)[i+1].height=nativeTextures[i].height;}
  model_t model{};model.type=mod_studio;strcpy_s(model.name,name);cl_entity_t ent{};ent.model=&model;ent.curstate.framerate=1;ent.origin[0]=40;ent.origin[2]=12;
  float origin[3]{40,0,12},angles[3]{},forward[3]{1,0,0},right[3]{0,-1,0},up[3]{0,0,1};
  D3DMATRIX normal{},mirror{};world(&ent,origin,angles,forward,right,up,0,0,0,normal,false);world(&ent,origin,angles,forward,right,up,0,0,0,mirror,true);
  for(int j=0;j<4;++j)for(int i=0;i<4;++i)require(fabsf(mirror.m[i][j]-(i==1?-normal.m[i][j]:normal.m[i][j]))<.0001f,"local-Y reflection including unchanged translation");
  size_t armCounts[3]{};
  std::vector<std::vector<DWORD>> baselineIndices;
  for(int side=0;side<3;++side){
   model_drawdata_t geometry{};skin(&model,hdr,0,0,geometry,-1,0,0,0,0,&ent,0,0xffffffff,nullptr,0,&normal,nullptr,side);
   for(const auto& batch:geometry.renderdata)armCounts[side]+=batch.indices.size()/3;
   if(oldProbe)continue;
   // Build real cached topology but reject allocation in the recording device.
   // No D3D device or graphics driver exists in this fixture.
   using Topology=bool(*)(studio_cache_t&,model_t*,studiohdr_t*,int,int,int);
   studio_cache_t cache{};symbol<Topology>("R_BuildStudioTopology")(cache,&model,hdr,0,0,side);
   require(cache.hiddenArm==side,"arm visibility is part of topology cache identity");
   size_t gpuTriangles=0;auto bones=reinterpret_cast<mstudiobone_t*>(bytes.data()+hdr->boneindex);
   size_t batchNumber=0;
   for(const auto& batch:cache.batches){
    if(!side)baselineIndices.push_back(batch.indices);
    else{
     require(batchNumber<baselineIndices.size(),"stable material batch ordering");std::vector<DWORD> expected;
     const auto& indices=baselineIndices[batchNumber];
     for(size_t triangle=0;triangle<indices.size();triangle+=3){bool hidden=false;
      for(int corner=0;corner<3;++corner){const auto bone=batch.sources[indices[triangle+corner]].bone;char label[33]{};memcpy(label,bones[bone].name,32);hidden|=strstr(label,side==2?"r_":"l_")!=nullptr;}
      if(!hidden)expected.insert(expected.end(),indices.begin()+triangle,indices.begin()+triangle+3);
     }
     require(expected==batch.indices,"exact ordered triangle subset; unrelated item/gripping geometry unchanged");
    }
    ++batchNumber;
   }
   for(const auto& batch:cache.batches){gpuTriangles+=batch.indices.size()/3;for(auto index:batch.indices){require(index<batch.sources.size(),"cached index bounds");const auto bone=batch.sources[index].bone;require(bone<hdr->numbones,"cached bone bounds");char label[33]{};memcpy(label,bones[bone].name,32);require(!side||!strstr(label,side==2?"r_":"l_"),"hidden-arm vertices excluded from cached indices");}}
   require(gpuTriangles==armCounts[side],"CPU fallback and cached topology agree");
  }
  printf("HANDS: %s both=%zu hide-left=%zu hide-right=%zu triangles\n",name,armCounts[0],armCounts[1],armCounts[2]);
  require(armCounts[1]>0&&armCounts[1]<armCounts[0]&&armCounts[2]>0&&armCounts[2]<armCounts[0],"support-arm filtering must remove geometry while retaining item/gripping hand");
  if(oldProbe){puts("PASS: before-build arm filter probe");return 0;}
  using Hands=void(__cdecl*)(int,int);auto hands=reinterpret_cast<Hands>(GetProcAddress(dll,"DX_CofDualHandsV1"));require(hands!=nullptr,"compiled hand-state bridge");
  require(publish(&ent,1,1)==1,"hand-state publication");hands(1,2);require(state->primaryHiddenArm==1&&state->secondaryHiddenArm==2,"independent primary/secondary hand rules");publish(nullptr,0,1);require(!state->primaryHiddenArm&&!state->secondaryHiddenArm,"off restores both models' hand state");
  for(int seq=0;seq<hdr->numseq;++seq)for(int body=0;body<3;++body)for(int sample=0;sample<3;++sample){
   ent.curstate.sequence=seq;ent.curstate.body=body;ent.curstate.skin=sample%std::max(1,hdr->numskinfamilies);ent.curstate.animtime=10;
   const float now=10+poses*.1f;ent.curstate.animtime=now-sample*.1f;client.time=now;
   require(publish(&ent,1,now)==1,"publish secondary");const auto original=ent;ent.origin[0]=999;
   size_t output=0;
   auto draw=[&](cl_entity_t* copy,bool mirrored){require(copy->origin[0]==40&&mirrored&&copy->curstate.sequence==seq&&copy->curstate.body==body,"immediate copied model state");
    model_drawdata_t geometry{};const float frame=advance(hdr,copy,now);require(std::isfinite(frame),"animation frame");
    skin(&model,hdr,seq,frame,geometry,-1,0,0,body,copy->curstate.skin,copy,now,0xffffffff,nullptr,0,&mirror,nullptr,0);
    for(const auto& batch:geometry.renderdata){require(batch.indices.size()%3==0,"triangle boundaries");for(int index:batch.indices)require(index>=0&&static_cast<size_t>(index)<batch.verts.size(),"mesh index bounds");for(const auto& v:batch.verts)require(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.u)&&std::isfinite(v.v),"finite geometry and UV");output+=batch.indices.size()/3;}
   };
   require(state->submit(now,++pass,draw)&&output>0,"nonempty secondary mesh through recording sink");require(!state->submit(now,pass,draw),"once per view pass");
   triangles+=output;++poses;ent=original;
  }
  require(!state->current(9),"clock rollback clears");require(publish(&ent,0,20)==1&&!state->current(20.25f),"expiry clears");require(publish(&ent,0,21)==1&&publish(nullptr,0,21)==1&&!state->valid,"explicit off clears");
  require(publish(&ent,0,std::numeric_limits<float>::quiet_NaN())==0,"NaN rejected");ent.curstate.effects=EF_NODRAW;require(!publish(&ent,0,22),"hidden rejected");
 }
 *symbol<IDirect3DDevice9**>("device")=nullptr;
 printf("PASS: compiled secondary export, immediate copy, %u native model poses, %zu CPU triangles, body/skin/animation, local-Y reflection, once/pass, off/stale/rollback/NaN/hidden cleanup. Recording sink; no GPU/window/game.\n",poses,triangles);
 return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
