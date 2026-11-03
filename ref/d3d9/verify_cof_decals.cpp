#include "dx_local.h"
#include "dx_cof_decal.h"
#include "cdll_int.h"
#include <dbghelp.h>
#include <fstream>
#include <stdexcept>

static HANDLE process=GetCurrentProcess();static DWORD64 base;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>static T symbol(const char* name){
 alignas(SYMBOL_INFO) char bytes[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};
 auto info=reinterpret_cast<SYMBOL_INFO*>(bytes);info->SizeOfStruct=sizeof(SYMBOL_INFO);info->MaxNameLen=MAX_SYM_NAME;
 check(SymFromName(process,name,info)&&info->ModBase==base,name);return reinterpret_cast<T>(static_cast<uintptr_t>(info->Address));
}
struct Vertex{float x,y,z;D3DCOLOR diffuse;float u,v;};
struct Decal{
 int textureIndex=0;short entityIndex=0,modelIndex=0;byte flags=0;float scale=1;
 vec3_t position{},normal{};const msurface_t* surface=nullptr;int revision=-1;
 bool dynamic=false,cofCustom=false;std::vector<Vertex> vertices;
};
static ref_client_t client{};static ref_host_t host{};static cvar_t shared{};
static intptr_t parm(int key,int){if(key==PARM_GET_CLIENT_PTR)return reinterpret_cast<intptr_t>(&client);if(key==PARM_GET_HOST_PTR)return reinterpret_cast<intptr_t>(&host);return 0;}
static void reg(convar_t* v){v->value=static_cast<float>(atof(v->string));}
static cvar_t* cvar(const char*){return &shared;}
static void print(const char*,...){}
static pfnUserMsgHook capturedHook;static unsigned nativeCalls;
static int registerMessage(const char* name,pfnUserMsgHook callback){check(!strcmp(name,"customdecal"),"exact native message name");capturedHook=callback;return 91;}
static int nativeMessage(const char* name,int size,void* data){check(!strcmp(name,"customdecal")&&size>13&&data,"native callback arguments");++nativeCalls;return 37;}
static std::vector<unsigned char> packet(const cof_decal::Message& m){
 std::vector<unsigned char> b;
 for(int i=0;i<6;++i){const int value=static_cast<int>(std::round((i<3?m.origin[i]:m.normal[i-3])*8));b.push_back(static_cast<byte>(value));b.push_back(static_cast<byte>(value>>8));}
 b.push_back(m.flags);b.insert(b.end(),m.group.begin(),m.group.end());b.push_back(0);return b;
}
template<class T>static T read(std::ifstream& f){T v{};f.read(reinterpret_cast<char*>(&v),sizeof v);check(bool(f),"fixture read");return v;}
struct Buffer{void** methods;std::vector<byte> bytes;};
struct Texture{void** methods;unsigned width,height;std::vector<byte> bytes;};
struct Device{void** methods;};
static DWORD states[256]{};static void* vbMethods[14]{};static void* textureMethods[22]{};
static std::vector<std::unique_ptr<Buffer>> buffers;static std::vector<std::unique_ptr<Texture>> gpuTextures;
static Buffer* bound;static UINT stride,offset;static unsigned draws;static bool remix;
static ULONG STDMETHODCALLTYPE release(void*){return 0;}
static HRESULT STDMETHODCALLTYPE lockVB(Buffer* b,UINT o,UINT,void** p,DWORD){*p=b->bytes.data()+o;return S_OK;}
static HRESULT STDMETHODCALLTYPE unlockVB(Buffer*){return S_OK;}
static HRESULT STDMETHODCALLTYPE createVB(Device*,UINT n,DWORD,DWORD,D3DPOOL,IDirect3DVertexBuffer9** out,HANDLE*){
 auto b=std::make_unique<Buffer>();b->methods=vbMethods;b->bytes.resize(n);*out=reinterpret_cast<IDirect3DVertexBuffer9*>(b.get());buffers.push_back(std::move(b));return S_OK;
}
static HRESULT STDMETHODCALLTYPE lockTexture(Texture* t,UINT,D3DLOCKED_RECT* lock,const RECT*,DWORD){lock->Pitch=static_cast<INT>(t->width*4);lock->pBits=t->bytes.data();return S_OK;}
static HRESULT STDMETHODCALLTYPE unlockTexture(Texture*,UINT){return S_OK;}
static HRESULT STDMETHODCALLTYPE createTexture(Device*,UINT w,UINT h,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DTexture9** out,HANDLE*){
 auto t=std::make_unique<Texture>();t->methods=textureMethods;t->width=w;t->height=h;t->bytes.resize(size_t(w)*h*4);*out=reinterpret_cast<IDirect3DTexture9*>(t.get());gpuTextures.push_back(std::move(t));return S_OK;
}
static HRESULT STDMETHODCALLTYPE state(Device*,D3DRENDERSTATETYPE s,DWORD v){states[s]=v;return S_OK;}
static HRESULT STDMETHODCALLTYPE stage(Device*,DWORD,D3DTEXTURESTAGESTATETYPE,DWORD){return S_OK;}
static HRESULT STDMETHODCALLTYPE sampler(Device*,DWORD,D3DSAMPLERSTATETYPE,DWORD){return S_OK;}
static HRESULT STDMETHODCALLTYPE texture(Device*,DWORD,IDirect3DBaseTexture9*){return S_OK;}
static HRESULT STDMETHODCALLTYPE fvf(Device*,DWORD){return S_OK;}
static HRESULT STDMETHODCALLTYPE stream(Device*,UINT,IDirect3DVertexBuffer9* vb,UINT o,UINT s){bound=reinterpret_cast<Buffer*>(vb);offset=o;stride=s;return S_OK;}
static HRESULT STDMETHODCALLTYPE draw(Device*,D3DPRIMITIVETYPE type,UINT start,UINT count){
 check(type==D3DPT_TRIANGLELIST&&stride==sizeof(Vertex),"production decal vertex ABI");
 check(bound&&size_t(offset)+(start+count*3)*stride<=bound->bytes.size(),"production draw bounds");
 check(states[D3DRS_ZWRITEENABLE]==FALSE,"decal depth writes disabled");
 if(remix)check(states[D3DRS_ALPHATESTENABLE]&&states[D3DRS_ALPHABLENDENABLE]==FALSE,"Remix cutout submission");
 else check(states[D3DRS_SRCBLEND]==D3DBLEND_DESTCOLOR&&states[D3DRS_DESTBLEND]==D3DBLEND_SRCCOLOR&&states[D3DRS_ALPHABLENDENABLE],"native 2*source*destination blend");
 ++draws;return S_OK;
}
struct SizePlane{
 mplane_t plane{};msurface_t surface{};texture_t wall{};mtexinfo_t info{};
 mvertex_t vertices[4]{};medge16_t edges[5]{};int surfedges[4]{1,2,3,4};model_t model{};
 cof_decal::Vec s{},t{};
 SizePlane(cof_decal::Vec n,float extent){
  cof_decal::axes(n,s,t);
  for(int i=0;i<3;++i){plane.normal[i]=n[i];info.vecs[0][i]=s[i];info.vecs[1][i]=t[i];}
  const float signs[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
  for(int i=0;i<4;++i){for(int j=0;j<3;++j)vertices[i].position[j]=extent*(s[j]*signs[i][0]+t[j]*signs[i][1]);edges[i+1].v[0]=static_cast<word>(i);edges[i+1].v[1]=static_cast<word>((i+1)%4);}
  info.texture=&wall;surface.plane=&plane;surface.texinfo=&info;surface.numedges=4;
  model.type=mod_brush;model.numsurfaces=model.nummodelsurfaces=1;model.surfaces=&surface;
  model.numvertexes=4;model.vertexes=vertices;model.numedges=5;model.edges16=edges;model.numsurfedges=4;model.surfedges=surfedges;
 }
};
static float span(const std::vector<Vertex>& vertices,cof_decal::Vec axis){
 float lo=FLT_MAX,hi=-FLT_MAX;
 for(const auto& v:vertices){const float x=cof_decal::dot({v.x,v.y,v.z},axis);lo=std::min(lo,x);hi=std::max(hi,x);}
 return hi-lo;
}
int main(int argc,char** argv)try{
 check(argc==8,"renderer, decalinfo, fixture, client, vgui, custom-size and WAD-size arguments");
 std::ifstream text(argv[2],std::ios::binary);const std::string definitions{std::istreambuf_iterator<char>(text),{}};
 const auto groups=cof_decal::parse(definitions.data(),definitions.size());check(groups.size()>30,"Steam decal definitions");
 bool graffitiSizeChecked=false;
 for(const auto& g:groups)if(g.name=="klotter6"){
  check(g.variants.size()==1&&g.variants[0].halfWidth==64&&g.variants[0].halfHeight==64,"klotter6 128-unit full extent, matching native parser");graffitiSizeChecked=true;
 }
 check(graffitiSizeChecked,"klotter6 definition present");
 const char* oddDefinition="odd { ink 129 65 0.2 }";
 const auto odd=cof_decal::parse(oddDefinition,strlen(oddDefinition));
 check(odd.size()==1&&odd[0].variants[0].halfWidth==64&&odd[0].variants[0].halfHeight==32,"native half-size integer truncation");
 check(cof_decal::parse("x { tex 1 nan 0.2 }",strlen("x { tex 1 nan 0.2 }")).empty(),"nonfinite definition");
 check(cof_decal::parse("x { ../bad 1 1 0.2 }",strlen("x { ../bad 1 1 0.2 }")).empty(),"texture path rejected");
 for(int c:{127,128})check(cof_decal::inkAlpha(c,c,c)==0,"neutral gray transparent");
 check(cof_decal::inkAlpha(0,0,0)==255&&cof_decal::inkAlpha(60,20,20)>0,"ink opaque");
 for(cof_decal::Vec n: {cof_decal::Vec{1,0,0},cof_decal::Vec{-1,0,0},cof_decal::Vec{0,1,0},cof_decal::Vec{0,-1,0},cof_decal::Vec{0,0,1},cof_decal::Vec{0,0,-1}}){
  cof_decal::Vec s,t;cof_decal::axes(n,s,t);check(fabsf(cof_decal::dot(s,t))<.0001f&&fabsf(cof_decal::dot(s,n))<.0001f&&fabsf(cof_decal::dot(s,s)-1)<.0001f,"projection basis");
 }
 auto dll=LoadLibraryA(argv[1]);check(dll!=nullptr,"renderer");
 SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_EXACT_SYMBOLS);check(SymInitialize(process,nullptr,FALSE)!=FALSE,"symbols");
 const std::string path=argv[1];SymSetSearchPath(process,path.substr(0,path.find_last_of("/\\")).c_str());
 base=SymLoadModuleEx(process,nullptr,argv[1],nullptr,reinterpret_cast<DWORD64>(dll),0,nullptr,0);check(base!=0,"matching PDB");
 using API=int(*)(int,ref_interface_t*,ref_api_t*,ref_globals_t*);
 auto api=reinterpret_cast<API>(GetProcAddress(dll,"GetRefAPI"));ref_api_t engine{};ref_interface_t functions{};ref_globals_t globals{};
 engine.EngineGetParm=parm;engine.Cvar_RegisterVariable=reg;engine.pfnGetCvarPointer=cvar;engine.Con_Printf=print;engine.Con_DPrintf=print;
 check(api&&api(REF_API_VERSION,&functions,&engine,&globals)==REF_API_VERSION,"API");
 auto publish=reinterpret_cast<int(__cdecl*)(const void*,int)>(GetProcAddress(dll,"DX_CofCustomDecalV1"));check(publish!=nullptr,"customdecal bridge export");
 auto clear=symbol<void(*)()>("R_ClearDecals");auto activate=symbol<void(*)()>("R_ActivateDecals");
 auto decals=symbol<std::vector<Decal>*>("g_dxDecals");auto pending=symbol<std::vector<cof_decal::Message>*>("g_cofPendingDecals");
 auto installDefinitions=[&](){*symbol<std::vector<cof_decal::Group>*>("g_cofDecalGroups")=groups;*symbol<bool*>("g_cofDefinitionsLoaded")=true;};
 auto textures=symbol<std::deque<dxtexture_t>*>("g_Textures");auto index=symbol<dxfast::HashMap<std::string,int>*>("g_TextureIndex");
 for(const auto& g:groups)for(const auto& v:g.variants){const std::string name="gfx/decals/"+v.texture+".tga";if(index->find(name)!=index->end())continue;
  dxtexture_t t{};strcpy_s(t.name,name.c_str());t.width=t.height=256;const int id=static_cast<int>(textures->size());textures->push_back(t);(*index)[name]=id;
 }
 *symbol<int*>("g_texcount")=static_cast<int>(textures->size())-1;
 symbol<convar_t*>("dx_decal_limit")->value=8192;
 cof_decal::Message m;m.normal={1,0,0};m.group="klotter6";m.flags=1;auto b=packet(m);
 check(!publish(nullptr,0)&&!publish(b.data(),12),"truncated packet rejected");
 auto bad=b;bad.back()='x';check(!publish(bad.data(),static_cast<int>(bad.size())),"unterminated name rejected");
 m.normal={0,0,0};bad=packet(m);check(!publish(bad.data(),static_cast<int>(bad.size())),"zero direction rejected");
 *symbol<bool*>("g_dxDecalMapReady")=false;
 for(int i=0;i<256;++i)check(publish(b.data(),static_cast<int>(b.size()))!=0,"bounded sign-on queue");
 check(!publish(b.data(),static_cast<int>(b.size()))&&pending->size()==256,"queue cap");clear();check(pending->empty(),"clear pending");
 std::ifstream fixture(argv[3],std::ios::binary);const unsigned maps=read<unsigned>(fixture);unsigned total=0,fragments=0;
 for(unsigned map=0;map<maps;++map){
  clear();installDefinitions();
  const unsigned count=read<unsigned>(fixture);check(count>100&&count<100000,"map face bounds");
  std::vector<mplane_t> planes(count);std::vector<msurface_t> surfaces(count);std::vector<mvertex_t> vertices;
  std::vector<medge16_t> edges(1);std::vector<int> surfedges;texture_t wall{};mtexinfo_t info{};info.texture=&wall;
  for(unsigned i=0;i<count;++i){for(float& x:planes[i].normal)x=read<float>(fixture);planes[i].dist=read<float>(fixture);
   auto& s=surfaces[i];s.flags=read<unsigned>(fixture);s.plane=&planes[i];s.texinfo=&info;s.firstedge=static_cast<int>(surfedges.size());s.numedges=read<int>(fixture);
   check(s.numedges>=3&&s.numedges<128,"face edge bounds");
   for(int j=0;j<s.numedges;++j){mvertex_t v{};for(float& x:v.position)x=read<float>(fixture);check(vertices.size()<65535,"fixture vertex width");medge16_t e{};e.v[0]=e.v[1]=static_cast<word>(vertices.size());vertices.push_back(v);surfedges.push_back(static_cast<int>(edges.size()));edges.push_back(e);}
  }
  model_t model{};model.type=mod_brush;model.numplanes=static_cast<int>(count);model.planes=planes.data();model.numsurfaces=model.nummodelsurfaces=static_cast<int>(count);model.surfaces=surfaces.data();model.numvertexes=static_cast<int>(vertices.size());model.vertexes=vertices.data();model.numedges=static_cast<int>(edges.size());model.edges16=edges.data();model.numsurfedges=static_cast<int>(surfedges.size());model.surfedges=surfedges.data();
  *symbol<model_t**>("g_worldmodel")=&model;*symbol<bool*>("g_dxDecalMapReady")=false;
  const unsigned messages=read<unsigned>(fixture);check(messages>0&&messages<256,"map decal count");
  std::vector<std::vector<unsigned char>> packets;
  for(unsigned i=0;i<messages;++i){const unsigned size=read<unsigned>(fixture);check(size<80,"fixture packet bound");std::vector<unsigned char> wire(size);fixture.read(reinterpret_cast<char*>(wire.data()),size);check(publish(wire.data(),static_cast<int>(size))!=0,"real map sign-on accepted");packets.push_back(wire);}
  activate();check(pending->empty()&&!decals->empty(),"custom-only sign-on replay");
  for(const auto& wire:packets){const auto before=decals->size();check(publish(wire.data(),static_cast<int>(wire.size()))!=0,"real map decal clipped");check(decals->size()==before,"static decal replay idempotent");}
  for(const auto& d:*decals){check(d.cofCustom&&(d.flags&FDECAL_DONTSAVE)&&d.vertices.size()%3==0,"custom fragment flags/triangles");for(const auto& v:d.vertices)check(std::isfinite(v.x)&&v.u>=-.001f&&v.u<=1.001f&&v.v>=-.001f&&v.v<=1.001f&&v.diffuse==0xffffffff,"bounded UV and native unlit ink");}
  void* methods[119]{};methods[23]=reinterpret_cast<void*>(createTexture);methods[26]=reinterpret_cast<void*>(createVB);methods[57]=reinterpret_cast<void*>(state);methods[65]=reinterpret_cast<void*>(texture);methods[67]=reinterpret_cast<void*>(stage);methods[69]=reinterpret_cast<void*>(sampler);methods[89]=reinterpret_cast<void*>(fvf);methods[100]=reinterpret_cast<void*>(stream);methods[81]=reinterpret_cast<void*>(draw);
  vbMethods[2]=reinterpret_cast<void*>(release);vbMethods[11]=reinterpret_cast<void*>(lockVB);vbMethods[12]=reinterpret_cast<void*>(unlockVB);
  textureMethods[2]=reinterpret_cast<void*>(release);textureMethods[19]=reinterpret_cast<void*>(lockTexture);textureMethods[20]=reinterpret_cast<void*>(unlockTexture);
  Device recorder{methods};*symbol<IDirect3DDevice9**>("device")=reinterpret_cast<IDirect3DDevice9*>(&recorder);
  for(auto& t:*textures){IDirect3DTexture9* tex=nullptr;createTexture(&recorder,4,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&tex,nullptr);t.tex=tex;t.width=4;t.height=1;auto pixels=reinterpret_cast<Texture*>(tex);pixels->bytes={128,128,128,255,0,0,0,255,127,127,127,255,20,20,90,255};}
  symbol<convar_t*>("dx_decals")->value=1;symbol<convar_t*>("dx_remix_perf")->value=1;
  const auto render=symbol<void(*)(const cl_entity_s*)>("R_DrawEntityDecals");
  for(bool mode:{false,true}){remix=mode;symbol<convar_t*>("dx_remix")->value=mode?1.f:0.f;symbol<void(*)()>("DX_InvalidateD3DStateCache")();draws=0;render(nullptr);check(draws>0,"production render batches");check(!states[D3DRS_ALPHATESTENABLE]&&!states[D3DRS_ALPHABLENDENABLE]&&states[D3DRS_ZWRITEENABLE],"render states restored");}
  const auto convert=symbol<IDirect3DTexture9*(*)(int,bool)>("R_RemixLegacyDecalTexture");
  auto converted=reinterpret_cast<Texture*>(convert(decals->front().textureIndex,true));check(converted&&converted->bytes[3]==0&&converted->bytes[7]==255&&converted->bytes[11]==0,"production gray-to-alpha conversion");
  *symbol<IDirect3DDevice9**>("device")=nullptr;
  printf("Map %u: %u static decals, %zu clipped surface fragments\n",map,messages,decals->size());total+=messages;fragments+=static_cast<unsigned>(decals->size());
  clear();check(decals->empty()&&pending->empty()&&symbol<std::vector<cof_decal::Message>*>("g_cofStaticDecals")->empty(),"map cleanup");*symbol<model_t**>("g_worldmodel")=nullptr;
 }
 unsigned variantsChecked=0,customProjections=0,wadProjections=0;std::string header;
 std::ifstream expected(argv[6]);std::getline(expected,header);check(bool(expected),"independent native size table");
 std::string groupName,textureName;unsigned variantIndex;float rawW,rawH,halfW,halfH,worldW,worldH;int pixelW,pixelH;
 while(expected>>groupName>>variantIndex>>textureName>>rawW>>rawH>>halfW>>halfH>>worldW>>worldH>>pixelW>>pixelH){
  const cof_decal::Group* group=nullptr;for(const auto& g:groups)if(g.name==groupName)group=&g;
  check(group&&variantIndex<group->variants.size(),"every native group/variant retained");
  const auto& v=group->variants[variantIndex];check(v.texture==textureName&&v.halfWidth==halfW&&v.halfHeight==halfH,"every variant matches native size conversion");
  for(cof_decal::Vec direction:{cof_decal::Vec{1,0,0},cof_decal::Vec{-1,0,0},cof_decal::Vec{0,1,0},cof_decal::Vec{0,-1,0},cof_decal::Vec{0,0,1},cof_decal::Vec{0,0,-1}})
   for(int flag:{0,1}){
    clear();installDefinitions();SizePlane fixturePlane(direction,2048);*symbol<model_t**>("g_worldmodel")=&fixturePlane.model;*symbol<bool*>("g_dxDecalMapReady")=true;*symbol<unsigned*>("g_cofVariantSerial")=variantIndex;
    cof_decal::Message decalMessage;decalMessage.normal=direction;decalMessage.flags=static_cast<byte>(flag);decalMessage.group=groupName;auto wire=packet(decalMessage);
    check(publish(wire.data(),static_cast<int>(wire.size()))!=0&&decals->size()==1,"each variant/static/transient/orientation projected");
    const auto& vertices=decals->front().vertices;check(fabsf(span(vertices,fixturePlane.s)-worldW)<.02f&&fabsf(span(vertices,fixturePlane.t)-worldH)<.02f,"actual custom world dimensions match native independently of texture resolution");++customProjections;
   }
  ++variantsChecked;
 }
 size_t variantTotal=0;for(const auto& g:groups)variantTotal+=g.variants.size();check(variantsChecked==variantTotal,"all aliases and variants covered");
 clear();SizePlane wadPlane({1,0,0},65536);*symbol<model_t**>("g_worldmodel")=&wadPlane.model;
 symbol<convar_t*>("dx_decal_lighting")->value=0;symbol<convar_t*>("dx_decal_vertex_lighting")->value=0;
 using BuildWad=bool(*)(const model_t*,const msurface_t*,int,const float*,float,std::vector<Vertex>&,float*);
 const auto buildWad=symbol<BuildWad>("R_BuildDecalOnSurface");std::ifstream wadExpected(argv[7]);std::getline(wadExpected,header);check(bool(wadExpected),"WAD size table");unsigned wadTextures=0;
 while(wadExpected>>textureName>>pixelW>>pixelH){
  (*textures)[1].width=pixelW;(*textures)[1].height=pixelH;
  for(float scale:{-1.f,.005f,.01f,.25f,.5f,1.f,2.f,4.f,16.f,32.f}){
   std::vector<Vertex> vertices;vec3_t origin{},normal{};const float effective=std::max(.01f,std::min(16.f,scale));
   check(buildWad(&wadPlane.model,&wadPlane.surface,1,origin,scale,vertices,normal),"standard WAD projection");
   check(fabsf(span(vertices,wadPlane.s)-pixelW/effective)<.1f&&fabsf(span(vertices,wadPlane.t)-pixelH/effective)<.1f,"WAD dimensions/incoming scale preserved, without custom half-size conversion");++wadProjections;
  }++wadTextures;
 }
 check(wadTextures==200,"all installed WAD decal dimensions covered");clear();*symbol<model_t**>("g_worldmodel")=nullptr;
 printf("PASS ALL SIZES: %u custom variants including aliases / %u projections across six directions and both flags; %u WAD textures / %u scaled projections.\n",variantsChecked,customProjections,wadTextures,wadProjections);
 check(LoadLibraryA(argv[5])!=nullptr,"retained vgui dependency");auto proxy=LoadLibraryA(argv[4]);check(proxy!=nullptr,"built client proxy");
 const auto rendererBase=base;base=SymLoadModuleEx(process,nullptr,argv[4],nullptr,reinterpret_cast<DWORD64>(proxy),0,nullptr,0);check(base!=0,"client PDB");
 symbol<cl_enginefunc_t*>("engine")->pfnHookUserMsg=registerMessage;
 check(symbol<int(*)(const char*,pfnUserMsgHook)>("hookMessage")("customdecal",nativeMessage)==91&&capturedHook!=nullptr,"production client hook registration");
 base=rendererBase;*symbol<bool*>("g_dxDecalMapReady")=false;m.normal={1,0,0};b=packet(m);
 check(capturedHook("customdecal",static_cast<int>(b.size()),b.data())==37&&nativeCalls==1&&pending->size()==1,"client forwards original once and publishes renderer packet");
 clear();
 printf("PASS: %u Steam map decals / %u fragments; production DLL clipping, packet bounds, queue/replay/deduplication, UVs, native blend, Remix cutout upload, draw bounds, state restoration, cleanup and real client hook forwarding. Recording sinks; no game/window/GPU.\n",total,fragments);return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL COF DECALS: %s\n",e.what());return 1;}

