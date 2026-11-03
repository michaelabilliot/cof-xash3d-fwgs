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
int main(int argc,char** argv)try{
 require(argc==2,"renderer argument");auto dll=LoadLibraryA(argv[1]);require(dll!=nullptr,"load renderer");
 SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_EXACT_SYMBOLS);require(SymInitialize(process,nullptr,FALSE)!=FALSE,"symbols");const std::string path=argv[1];const auto folder=path.substr(0,path.find_last_of("/\\"));SymSetSearchPath(process,folder.c_str());
 moduleBase=SymLoadModuleEx(process,nullptr,argv[1],nullptr,reinterpret_cast<DWORD64>(dll),0,nullptr,0);require(moduleBase!=0,"matching PDB");
 using API=int(*)(int,ref_interface_t*,ref_api_t*,ref_globals_t*);auto api=reinterpret_cast<API>(GetProcAddress(dll,"GetRefAPI"));ref_api_t engine{};ref_interface_t functions{};ref_globals_t globals{};engine.EngineGetParm=parm;engine.Cvar_RegisterVariable=reg;engine.pfnGetCvarPointer=cvar;engine.Mod_Extradata=extra;require(api&&api(REF_API_VERSION,&functions,&engine,&globals)==REF_API_VERSION,"API");
 void* methods[119]{};methods[26]=reinterpret_cast<void*>(createVB);methods[44]=reinterpret_cast<void*>(transform);methods[57]=reinterpret_cast<void*>(renderState);methods[65]=reinterpret_cast<void*>(texture);methods[67]=reinterpret_cast<void*>(stage);methods[69]=reinterpret_cast<void*>(sampler);methods[89]=reinterpret_cast<void*>(fvf);methods[100]=reinterpret_cast<void*>(stream);methods[81]=reinterpret_cast<void*>(draw);
 vbMethods[2]=reinterpret_cast<void*>(release);vbMethods[11]=reinterpret_cast<void*>(lock);vbMethods[12]=reinterpret_cast<void*>(unlock);Device recorder{methods};*symbol<IDirect3DDevice9**>("device")=reinterpret_cast<IDirect3DDevice9*>(&recorder);
 auto textures=symbol<std::deque<dxtexture_t>*>("g_Textures");textures->resize(2);(*textures)[1].width=(*textures)[1].height=1;*symbol<int*>("g_texcount")=1;
 auto flush=symbol<void(*)()>("R_Flush2DBatch");auto mode=symbol<void(*)(int)>("R_SetRenderMode");auto color=symbol<void(*)(unsigned char,unsigned char,unsigned char,unsigned char)>("Color4ub");auto pic=symbol<void(*)(float,float,float,float,float,float,float,float,int)>("R_DrawStretchPic");auto setup=symbol<void(*)(int)>("VGUI_SetupDrawing");
 for(int value:{0,64,128,192,255}){mode(kRenderTransAdd);mode(kRenderScreenFadeModulate);color(static_cast<byte>(value),static_cast<byte>(value),static_cast<byte>(value),255);pic(0,0,1920,1080,0,0,1,1,1);flush();
  const auto& d=draws.back();require(d.state[D3DRS_ALPHABLENDENABLE]==TRUE&&d.state[D3DRS_SRCBLEND]==D3DBLEND_ZERO&&d.state[D3DRS_DESTBLEND]==D3DBLEND_SRCCOLOR,"screen-fade modulation missing/wrong");require(d.vertices.size()==6&&d.vertices[0].color==D3DCOLOR_ARGB(255,value,value,value),"fade RGB and fullscreen quad");
 }
 for(int alpha:{0,64,128,192,255}){mode(kRenderTransTexture);color(0,0,0,static_cast<byte>(alpha));pic(0,0,1920,1080,0,0,1,1,1);flush();const auto& d=draws.back();require(d.state[D3DRS_SRCBLEND]==D3DBLEND_SRCALPHA&&d.state[D3DRS_DESTBLEND]==D3DBLEND_INVSRCALPHA&&d.vertices[0].color==D3DCOLOR_ARGB(alpha,0,0,0),"ordinary black fade alpha");}
 for(int frame=0;frame<240;++frame){
  // Hint fades to zero, then stops drawing. Native controls remain independent.
  if(frame<120){setup(0);color(255,255,255,static_cast<byte>(255-frame*2));pic(0,0,800,48,0,0,1,1,1);}
  setup(0);color(255,255,255,254);pic(810,980,300,100,0,0,1,1,1);flush();const auto& d=draws.back();require(d.vertices.back().color==D3DCOLOR_ARGB(254,255,255,255)&&d.state[D3DRS_SRCBLEND]==D3DBLEND_SRCALPHA&&d.state[D3DRS_DESTBLEND]==D3DBLEND_INVSRCALPHA&&d.state[D3DRS_ZENABLE]==FALSE,"native texture survives fading hint and mode reset");
 }
 symbol<void(*)()>("R_FreeScratchBuffers")();*symbol<IDirect3DDevice9**>("device")=nullptr;
 puts("PASS: compiled 2D recording sink, modulation/ordinary black fades0..255, full-screen geometry and240 native-control draws across hint expiry; no GPU/window/game.");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
