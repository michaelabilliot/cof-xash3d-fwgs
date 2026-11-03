// Production pose caching, CPU upload and GPU draw submission, with a recording
// device and buffers. Lighting/context setup is fixed; no graphics driver starts.
#include "dx_studio_shader.h"
constexpr float DX_STUDIO_SHADE_LAMBERT=1.4953241f;
struct Buffer final : IDirect3DVertexBuffer9 {
    std::vector<byte> data;bool fail=false;int locks=0;
    explicit Buffer(size_t size):data(size){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override{return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override{return 1;}
    ULONG STDMETHODCALLTYPE Release() override{return 0;}
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9**) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID,const void*,DWORD,DWORD) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID,void*,DWORD*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID) override{return E_NOTIMPL;}
    DWORD STDMETHODCALLTYPE SetPriority(DWORD) override{return 0;}
    DWORD STDMETHODCALLTYPE GetPriority() override{return 0;}
    void STDMETHODCALLTYPE PreLoad() override{}
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override{return D3DRTYPE_VERTEXBUFFER;}
    HRESULT STDMETHODCALLTYPE Lock(UINT,UINT,void **out,DWORD) override{++locks;*out=fail?nullptr:data.data();return fail?E_FAIL:S_OK;}
    HRESULT STDMETHODCALLTYPE Unlock() override{return S_OK;}
    HRESULT STDMETHODCALLTYPE GetDesc(D3DVERTEXBUFFER_DESC*) override{return E_NOTIMPL;}
};
struct Device {
    float constants[512][4]{};int draws=0;bool shaders=true;D3DXMATRIX world;
    Device(){D3DXMatrixIdentity(&world);}
    HRESULT GetTransform(D3DTRANSFORMSTATETYPE,D3DMATRIX *out){*out=world;return S_OK;}
    HRESULT SetStreamSource(UINT,IDirect3DVertexBuffer9*,UINT,UINT){return S_OK;}
    HRESULT SetVertexShaderConstantF(UINT start,const float *p,UINT count){require(start+count<=512,"GPU constant bounds");memcpy(constants[start],p,count*4*sizeof(float));return S_OK;}
    HRESULT SetIndices(IDirect3DIndexBuffer9*){return S_OK;}
    HRESULT DrawIndexedPrimitive(D3DPRIMITIVETYPE,INT,UINT,UINT,UINT,UINT){++draws;return S_OK;}
    HRESULT SetVertexShader(IDirect3DVertexShader9*){return S_OK;}
    HRESULT SetVertexDeclaration(IDirect3DVertexDeclaration9*){return S_OK;}
    HRESULT SetFVF(DWORD){return S_OK;}
} recordingDevice;
#define device (&recordingDevice)
int g_texcount=0;
std::deque<dxtexture_t> g_Textures;
vec3_t g_vOrigin{},g_vRight{1,0,0},g_vUp{0,0,1};
convar_t dx_studio_update_budget{},dx_studio_gpu_smooth{},dx_studio_viewmodel_throttle{},dx_studio_anim_debug{};
static uint64_t g_studioPoseFrame=0;
static int g_studioPoseUpdatesThisFrame=0;
static bool normals=false,probeUploadNormals=false,gpu=false;
static bool R_StudioNormalLighting(){return false;}
static bool R_StudioUploadNormals(){return probeUploadNormals;}
static bool R_StudioNormalsEnabled(){return normals;}
static D3DCOLOR R_StudioFullbrightDiffuse(D3DCOLOR c){return c;}
static float R_StudioFlatshadeFactor(float){return 1;}
static D3DCOLOR R_StudioApplyDiffuseFactor(D3DCOLOR c,float){return c;}
static D3DCOLOR R_StudioApplyLightFactor(D3DCOLOR c,float,float){return c;}
static D3DCOLOR R_StudioApplyLocalLights(D3DCOLOR c,const float*,const float*,const studio_local_light_t*,int){return c;}
static void R_StudioBuildChromeUV(const D3DMATRIX&,const BoneMat34*,int,const float*,float,float,float &u,float &v){u=v=0;}
static int R_StudioLocalLights(const cl_entity_s*,const D3DMATRIX*,studio_local_light_t*){return 0;}
static bool R_StudioLocalLightsEqual(const studio_local_light_t*,int,const studio_local_light_t*,int){return true;}
void R_StudioStoreLocalLights(studio_cache_t &c,const studio_local_light_t*,int){c.localLightCount=0;}
static UINT R_StudioVertexStride(bool n,bool u){return u?sizeof(studio_normal_lit_vertex_t):(n?sizeof(studio_fast_lit_vertex_t):sizeof(brush_vertex_t));}
static void R_StudioLightToBone(const float in[3],const BoneMat34,float out[3]){memcpy(out,in,12);}
static bool R_CreateStudioBuffers(studio_cache_t &c){c.gpuSkinningBufferMode=gpu;return true;}
static void R_FreeStudioBuffers(studio_cache_t &c){c.combinedVertexBuffer=nullptr;c.gpuSkinSourceVertexBuffer=nullptr;c.gpuSkinningPoseValid=false;c.poseUploaded=false;}
void DX_ProfileDraw(bool,unsigned){}
void DX_ProfileLock(int,int,unsigned,unsigned){}
void DX_ProfileEvent(int,unsigned){}
bool DX_ThreadStudioSkinning(int){return false;}
void DX_MatrixIdentity(D3DMATRIX &m){D3DXMatrixIdentity(reinterpret_cast<D3DXMATRIX*>(&m));}
static void DX_ToD3DXMatrix(const D3DMATRIX &a,D3DXMATRIX &b){memcpy(&b,&a,sizeof(b));}
bool DX_EnsureStudioSkinningShaders(){return recordingDevice.shaders;}
int DX_StudioGpuSkinningBoneCap(){return 72;}
void DX_BindStudioGpuSkinningShader(bool){}
static int R_StudioGlowShellTextureId(){return 0;}
static void R_StudioGlowShellChromeOrigin(float,float*){}
static float R_StudioGlowShellScale(const cl_entity_s*){return 0;}
static D3DCOLOR R_StudioGlowShellColor(const cl_entity_s*){return 0;}
static void R_BeginStudioGlowShellPass(D3DCOLOR){}
void DX_Bind(int,int){}
void R_ApplyStudioMaterialState(unsigned int,const cl_entity_s*,studio_material_state_t&){}
#include "verify_ragdoll_render.inc"
#undef device
static void renderChecks(const studiohdr_t *hdr,const BoneMat34 *base,const BoneMat34 *reacted,const cl_entity_t &ent) {
    studio_cache_t c{};c.model=ent.model;c.batches.emplace_back();auto &batch=c.batches[0];
    const auto *bytes=reinterpret_cast<const byte*>(hdr);const auto *parts=reinterpret_cast<const mstudiobodyparts_t*>(bytes+hdr->bodypartindex);
    for(int i=0;i<hdr->numbodyparts;++i) {
        const auto &part=parts[i];int selected=part.base>0?(ent.curstate.body/part.base)%part.nummodels:0;
        const auto &model=reinterpret_cast<const mstudiomodel_t*>(bytes+part.modelindex)[selected];
        const auto *v=reinterpret_cast<const vec3_t*>(bytes+model.vertindex);const byte *bones=bytes+model.vertinfoindex;
        for(int j=0;j<model.numverts;++j) {studio_source_vertex_t source{};source.x=v[j][0];source.y=v[j][1];source.z=v[j][2];source.bone=bones[j];source.normalBone=bones[j];source.nz=1;batch.sources.push_back(source);}
    }
    require(!batch.sources.empty(),"production render fixture has native vertices");batch.vertexCount=static_cast<unsigned>(batch.sources.size());batch.primitiveCount=1;
    batch.indexBuffer=reinterpret_cast<IDirect3DIndexBuffer9*>(1);
    Buffer buffer(batch.sources.size()*sizeof(studio_normal_lit_vertex_t));batch.vertexBuffer=&buffer;c.combinedVertexBuffer=&buffer;c.combinedVertexBufferMode=true;c.combinedVertexCount=batch.vertexCount;
    c.gpuSkinSourceVertexBuffer=&buffer;D3DXMATRIX world;D3DXMatrixIdentity(&world);float light[3]={0,0,-1};
    auto update=[&](const BoneMat34 *pose,bool procedural,float frame){return R_UpdateStudioVertices(c,ent.model,const_cast<studiohdr_t*>(hdr),&ent,12,frame,-1,0,0,0,0xffffffff,light,0,&world,false,pose,procedural);};
    float peak=0;
    for(int mode=0;mode<3;++mode) {
        gpu=false;normals=mode!=0;probeUploadNormals=mode==2;c.poseUploaded=false;
        require(update(base,false,80),"production CPU baseline upload");auto before=buffer.data;
        require(update(reacted,true,80),"production CPU reacted upload bypasses pose cache");
        const size_t stride=R_StudioVertexStride(normals,probeUploadNormals);
        for(size_t v=0;v<batch.sources.size();++v) {const float *a=reinterpret_cast<const float*>(before.data()+v*stride),*b=reinterpret_cast<const float*>(buffer.data.data()+v*stride);float delta=0;for(int k=0;k<3;++k)delta+=(a[k]-b[k])*(a[k]-b[k]);peak=std::max(peak,std::sqrt(delta));}
        require(peak>1e-4f,"actual native skinned vertices move");
        require(update(base,false,80) && !c.physicsPoseUploaded,"actual CPU recovery restores animation");
        int locks=buffer.locks;require(update(base,false,80) && locks==buffer.locks,"actual pose cache avoids duplicate CPU upload");
    }
    gpu=true;normals=false;probeUploadNormals=false;require(update(reacted,true,80),"production GPU palette handoff");
    require(R_DrawStudioSkinBatches(c,&ent),"production GPU draw submission");
    for(int b=0;b<hdr->numbones;++b) for(int r=0;r<3;++r) require(!memcmp(recordingDevice.constants[DX_STUDIO_GPU_SKIN_CONST_BONES+b*3+r],reacted[b][r],16),"actual SetVertexShaderConstantF rows match reaction palette");
    require(recordingDevice.draws>0,"production draw emitted");
    require(update(base,false,80) && R_DrawStudioSkinBatches(c,&ent),"GPU recovery submission");
    for(int b=0;b<hdr->numbones;++b) for(int r=0;r<3;++r) require(!memcmp(recordingDevice.constants[DX_STUDIO_GPU_SKIN_CONST_BONES+b*3+r],base[b][r],16),"submitted GPU recovery matches animation");
    gpu=false;dx_studio_update_budget.value=1;dx_studio_viewmodel_throttle.value=1;g_studioPoseUpdatesThisFrame=1;
    int locks=buffer.locks;require(update(base,false,81) && locks==buffer.locks,"exhausted production animation budget defers update");
    require(update(reacted,true,81) && buffer.locks>locks,"reaction bypasses exhausted animation budget");
    buffer.fail=true;require(!update(reacted,true,81),"actual failed vertex upload propagates fallback result");
    recordingDevice.shaders=false;require(!R_DrawStudioSkinBatches(c,&ent),"missing GPU path requests CPU fallback");recordingDevice.shaders=true;
    std::cout<<"PASS production skinning/submission: native_vertices="<<batch.sources.size()<<" peak_vertex_delta_units="<<peak<<" draws="<<recordingDevice.draws<<" CPU layouts=3 GPU constants/cache/budget/recovery/upload-failure\n";
    dx_studio_update_budget.value=0;
}

static void integrationChecks(const std::string &path) {
    std::ifstream file(path,std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    auto *h=reinterpret_cast<studiohdr_t*>(bytes.data());require(R_StudioHeaderValid(h),"integration native model");
    DX_RagdollReset();floorSetup();model_t map{};map.type=mod_brush;map.surfaces=&surface;g_worldmodel=&map;maximum.value=1;
    client.time=0;DX_RagdollFrame();model_t model{};model.type=mod_studio;strcpy_s(model.name,"models/slower.mdl");
    BoneMat34 base[128]{},pose[128]{};D3DXMATRIX world;D3DXMatrixTranslation(&world,0,0,80);
    for(int i=1;i<=2;++i) {entities[i]={};entities[i].index=i;entities[i].model=&model;entities[i].curstate.modelindex=2;entities[i].curstate.sequence=12;states[i]={};states[i].modelindex=2;states[i].serial=2000+i;states[i].monster=1;states[i].health=100;}
    StudioCalcBoneTransformsInternal(nullptr,h,12,80,base,-1,0,0,&entities[1],0);
    std::array<float,128> scales{};std::vector<ragdoll::Bone> rig;require(buildBones(&entities[1],h,world,base,scales,rig),"integration rig");
    int head=-1;for(size_t i=0;i<rig.size();++i)if(rig[i].collision && rig[i].region==ragdoll::Region::Head){head=static_cast<int>(i);break;}require(head>=0,"head target");
    const auto point=rig[head].pose*((rig[head].minimum+rig[head].maximum)*.5f);
    ragdoll::Reactions reaction;reaction.hit(rig,head,point,{1,0,0},2.2f);reaction.step(.05f);std::vector<btTransform> reacted;reaction.apply(rig,reacted);memcpy(pose,base,sizeof(base));writePalette(world,scales,reacted.data(),h->numbones,pose);
    renderChecks(h,base,pose,entities[1]);
    for(int i=1;i<=2;++i) {states[i].health=0;states[i].dead=1;memcpy(pose,base,sizeof(base));DX_RagdollPose(&entities[i],h,world,pose,0);}
    require(entries.size()==2 && entries[0].doll && !entries[1].doll && entries[1].activation==Entry::Activation::Pending,"capacity rejection remains retryable");
    for(int i=0;i<3600 && !entries[0].doll->settled;++i) physics->step(1.0/120,800);
    require(entries[0].doll->settled,"first native corpse settles");physics->freeze(*entries[0].doll);
    auto &hit=states[2].hits[0];states[2].hit_count=1;hit={};hit.id=900;hit.shot=70;hit.lethal=1;hit.bone=head+1;hit.pellets=hit.total_pellets=1;hit.weapon=1;hit.time=0;hit.direction[0]=1;
    hit.local_valid=1;for(int j=0;j<3;++j){hit.position[j]=100000;hit.local_position[j]=(rig[head].minimum[j]+rig[head].maximum[j])*.5f/ragdoll::UnitsToMeters;}
    states[2].server_time=2;memcpy(pose,base,sizeof(base));require(DX_RagdollPose(&entities[2],h,world,pose,2),"queued corpse activates after slot frees");
    require(entries[1].physicalSeen==900 && acknowledgements[2]==900,"delayed local-space lethal impulse physically acknowledged despite stale world position");
    auto *second=entries[1].doll;const auto beforeVelocity=second->parts[second->partForBone[head]].body->getLinearVelocity();
    memcpy(pose,base,sizeof(base));DX_RagdollPose(&entities[2],h,world,pose,2);require((second->parts[second->partForBone[head]].body->getLinearVelocity()-beforeVelocity).length2()==0,"physical lethal impulse delivered exactly once");
    retentionEnabled=true;fixtureHeader=h;gEngfuncs.Mod_Extradata=[](int,model_t*)->void*{return fixtureHeader;};
    std::array<btTransform,128> retiredBefore,activeBefore;physics->pose(*entries[0].doll,retiredBefore);physics->pose(*second,activeBefore);
    // Model allocation lifetime ends before R_Shutdown on the engine path.
    for(auto &e:entries) e.model=reinterpret_cast<model_t*>(1);
    DX_RagdollReset("renderer-shutdown");require(retainedRecords[1].bytes && retainedRecords[2].bytes,"unload exports retired and active corpse values");
    client.time=2;DX_RagdollFrame();
    for(int i=1;i<=2;++i){memcpy(pose,base,sizeof(base));require(DX_RagdollPose(&entities[i],h,world,pose,2),"renderer reconstructs retained corpse");}
    std::array<btTransform,128> after;physics->pose(*entries[0].doll,after);require(!memcmp(after.data(),retiredBefore.data(),h->numbones*sizeof(btTransform)),"retired world palette exact after renderer reload");
    physics->pose(*entries[1].doll,after);require(!memcmp(after.data(),activeBefore.data(),h->numbones*sizeof(btTransform)),"active interpolated palette exact after renderer reload");
    require(entries[1].physicalSeen==900 && (entries[1].doll->parts[entries[1].doll->partForBone[head]].body->getLinearVelocity()-beforeVelocity).length2()==0,"reload preserves velocity and event consumption");
    require(entries[0].doll->frozen && entries[0].doll->parts.size()==11,"retired native collision survives reload");
    // A settled (not retired) native pose must also be byte-identical.
    for(int i=0;i<3600 && !entries[1].doll->settled;++i)physics->step(1.0/120,800);
    require(entries[1].doll->settled,"post-impact corpse settles");physics->pose(*entries[1].doll,activeBefore);
    DX_RagdollReset("renderer-shutdown");DX_RagdollFrame();memcpy(pose,base,sizeof(base));require(DX_RagdollPose(&entities[2],h,world,pose,2),"settled corpse restores");
    auto restored=std::find_if(entries.begin(),entries.end(),[](const Entry &e){return e.index==2;});
    require(restored!=entries.end(),"offscreen islands restored before stepping");
    physics->pose(*restored->doll,after);require(restored->doll->settled && !memcmp(activeBefore.data(),after.data(),h->numbones*sizeof(btTransform)),"settled palette byte-identical across DLL reconstruction");
    for(int i=0;i<240;++i) {physics->step(1.0/120,800);require(restored->doll->settled,"restored island keeps stable support contacts");}physics->pose(*restored->doll,after);require(!memcmp(activeBefore.data(),after.data(),h->numbones*sizeof(btTransform)),"restored settled pose stays motionless");
    DX_RagdollReset();retentionEnabled=false;gEngfuncs.Mod_Extradata=nullptr;maximum.value=12;
    std::cout<<"PASS integration: capacity retry, delayed lethal acknowledgement, once-only momentum, active/retired/settled reload, exact palettes and velocity; payload_bytes="<<sizeof(RetainedDoll)<<"\n";
}
