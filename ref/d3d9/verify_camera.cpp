#include "dx_local.h"
#include "dx_effect_timing.h"
#include <fstream>
#include <vector>
#include <cstdio>
#include <limits>
static std::vector<char> modelBytes,lightModelBytes,secondaryLightBytes;
static model_t secondaryLightModel{};
static model_t lightModel{};
static cl_entity_t lightViewmodel{};
static ref_client_t client{};
static ref_host_t host{};
static cvar_t shared{};
static cl_entity_t lightPlayer{};
static cl_entity_t* beamEntity(int index) { return index==1?&lightPlayer:nullptr; }
static intptr_t parm(int key,int) {
    if(key==PARM_GET_CLIENT_PTR) return reinterpret_cast<intptr_t>(&client);
    if(key==PARM_GET_VIEWENT_PTR)return reinterpret_cast<intptr_t>(&lightViewmodel);
    if(key==PARM_GET_HOST_PTR) return reinterpret_cast<intptr_t>(&host);
    return 0;
}
static void reg(convar_t* value) { value->value=static_cast<float>(atof(value->string)); }
static cvar_t* cvar(const char*) { return &shared; }
static void* extra(int,model_t* model) { return model==&secondaryLightModel?secondaryLightBytes.data():(model==&lightModel?lightModelBytes.data():modelBytes.data()); }
#include "verify_cof_lighting.h"
int main(int argc,char** argv) {
    // Reproduce late first observation, model-switch priming, shot/repeat,
    // sequence transition, wrap, stale deadline and clock rollback.
    if(DX_MuzzleCrossed(-1,24,0,30,false,true,false,1)) return 20;
    if(DX_MuzzleCrossed(12,24,2,30,false,true,false,1)) return 21;
    if(!DX_MuzzleCrossed(12,0,0,30,false,true,false,1)) return 22;
    if(!DX_MuzzleCrossed(0,1,1,30,false,false,false,1)) return 23;
    if(DX_MuzzleCrossed(1,1,1,30,false,false,false,1)) return 24;
    if(!DX_MuzzleCrossed(29,0,0,30,true,false,false,1)) return 25;
    if(DX_MuzzleCrossed(12,4,1,30,true,false,false,1) ||
       DX_MuzzleCrossed(28,24,1,30,false,false,true,1)) return 26;
    if(!DX_MuzzleCrossed(12,0,0,30,false,false,true,1)) return 27;
    if(DX_LightAlive(100,0,10) || DX_LightAlive(100,9,10) || !DX_LightAlive(100,11,10)) return 28;
    if(DX_FlashAlive(10,10.05f,10.06f) || DX_FlashAlive(10,10.05f,2) || !DX_FlashAlive(10,10.05f,10.02f)) return 29;
    printf("PASS: muzzle event replay/repeat/wrap/restart and light expiry/clock rollback checks.\n");
    if(argc!=3) return 2;
    if(!verifyCofLighting(argv[2]))return 40;
    std::ifstream file(argv[2],std::ios::binary);
    modelBytes.assign(std::istreambuf_iterator<char>(file),{});
    if(modelBytes.size()<sizeof(studiohdr_t)) return 3;
    const std::string modelPath=argv[2];const auto slash=modelPath.find_last_of("/\\");
    std::ifstream flashlight(modelPath.substr(0,slash)+"/weapons/flashlight/v_flashlight.mdl",std::ios::binary);
    lightModelBytes.assign(std::istreambuf_iterator<char>(flashlight),{});
    if(lightModelBytes.size()<sizeof(studiohdr_t))return 45;
    lightModel.type=mod_studio;strcpy_s(lightModel.name,"models/weapons/flashlight/v_flashlight.mdl");
    lightViewmodel.model=&lightModel;lightViewmodel.curstate.framerate=1;
    auto dll=LoadLibraryA(argv[1]); if(!dll) return 4;
    using API=int(*)(int,ref_interface_t*,ref_api_t*,ref_globals_t*);
    using Query=int(__cdecl*)(cl_entity_t*,float,float*,int);
    auto api=reinterpret_cast<API>(GetProcAddress(dll,"GetRefAPI"));
    auto query=reinterpret_cast<Query>(GetProcAddress(dll,"DX_StudioCameraAttachmentsV3"));
    ref_api_t engine{}; ref_interface_t functions{}; ref_globals_t globals{};
    engine.EngineGetParm=parm; engine.Cvar_RegisterVariable=reg;
    engine.pfnGetCvarPointer=cvar; engine.Mod_Extradata=extra;
    engine.R_BeamGetEntity=beamEntity;
    if(!api || !query || api(REF_API_VERSION,&functions,&engine,&globals)!=REF_API_VERSION) return 5;
    // CPU pose initialization registers the same renderer cvars as normal model setup.
    model_t model{};model.type=mod_studio;
    cl_entity_t ent{};ent.model=&model;ent.curstate.framerate=1;
    auto hdr=reinterpret_cast<studiohdr_t*>(modelBytes.data());
    float bootstrap[12];if(!query(&ent,0,bootstrap,1))return 44;
    using LightPose=int(__cdecl*)(cl_entity_t*,float,const float*,const float*,const float*,float*);
    auto pose=reinterpret_cast<LightPose>(GetProcAddress(dll,"DX_StudioViewModelLightAttachmentsV1"));
    float camera[3]{},angles[3]{},offsets[3]{},points[6];
    if(!pose||!pose(&lightViewmodel,0,camera,angles,offsets,points))return 46;
    const float initial[6]={points[0],points[1],points[2],points[3],points[4],points[5]};
    for(int i=0;i<3;++i){lightViewmodel.origin[i]+=48;camera[i]+=48;}
    if(!pose(&lightViewmodel,0,camera,angles,offsets,points))return 47;
    for(int i=0;i<6;++i)if(fabsf(points[i]-initial[i]-48)>.01f)return 48;
    for(int i=0;i<3;++i){lightViewmodel.origin[i]-=48;camera[i]-=48;}
    lightViewmodel.angles[1]=angles[1]=90;
    if(!pose(&lightViewmodel,0,camera,angles,offsets,points))return 49;
    for(int i=0;i<2;++i)if(fabsf(points[i*3]+initial[i*3+1])>.01f||fabsf(points[i*3+1]-initial[i*3])>.01f)return 50;
    lightViewmodel.angles[1]=angles[1]=0;
    offsets[0]=2;offsets[1]=3;offsets[2]=4;
    if(!pose(&lightViewmodel,0,camera,angles,offsets,points))return 51;
    for(int i=0;i<2;++i)if(fabsf(points[i*3]-initial[i*3]-2)>.01f||fabsf(points[i*3+1]-initial[i*3+1]+3)>.01f||fabsf(points[i*3+2]-initial[i*3+2]-4)>.01f)return 52;
    for(float& value:offsets)value=0;
    auto lightHdr=reinterpret_cast<studiohdr_t*>(lightModelBytes.data());
    bool lensMoved=false;unsigned lightPoses=0;
    for(int seq=0;seq<lightHdr->numseq;++seq){lightViewmodel.curstate.sequence=seq;
     for(float t:{0.f,.1f,.2f}){if(!pose(&lightViewmodel,t,camera,angles,offsets,points))return 53;++lightPoses;
      for(int i=0;i<3;++i)if(fabsf(points[i]-initial[i])>.1f)lensMoved=true;}}
    if(!lensMoved)return 54;
    lightViewmodel.curstate.sequence=0;
    float lightSentinel[6]={123,456,789};
    const int attachmentIndex=lightHdr->attachmentindex;
    lightHdr->attachmentindex=lightHdr->length;
    if(pose(&lightViewmodel,0,camera,angles,offsets,lightSentinel)||lightSentinel[0]!=123)return 55;
    lightHdr->attachmentindex=attachmentIndex;
    lightViewmodel.curstate.effects=EF_NODRAW;
    if(pose(&lightViewmodel,0,camera,angles,offsets,lightSentinel))return 56;
    lightViewmodel.curstate.effects=0;
    if(pose(&lightViewmodel,std::numeric_limits<float>::quiet_NaN(),camera,angles,offsets,lightSentinel))return 57;
    std::printf("PASS: actual flashlight lens/aim attachments, %u poses; translation/yaw/viewmodel offsets, animation motion, malformed/hidden/NaN rejection. Idle lens=(%.3f %.3f %.3f), aim=(%.3f %.3f %.3f). No device.\n",lightPoses,initial[0],initial[1],initial[2],initial[3],initial[4],initial[5]);
    using LightState=void(__cdecl*)(unsigned);
    using LightDescription=int(__cdecl*)(D3DLIGHT9*);
    auto lightState=reinterpret_cast<LightState>(GetProcAddress(dll,"DX_CofLightStateV1"));
    auto describe=reinterpret_cast<LightDescription>(GetProcAddress(dll,"DX_CofLightDescriptionV1"));
    if(!lightState || !describe) return 30;
    client.maxclients=1; client.playernum=0; lightPlayer.index=1;
    D3DLIGHT9 light{};
    for(unsigned flags:{1u,2u,4u,5u,8u,12u,15u}) {
        for(int tick=0;tick<1800;++tick) {
            client.time=tick/60.f; lightState(flags);
            if(!describe(&light) || light.Type!=D3DLIGHT_SPOT || light.Range!=700.f ||
                light.Theta<=0 || light.Phi<=light.Theta || (!std::isfinite(light.Diffuse.r)||light.Diffuse.r<=0)) return 31;
        }
    }
    lightState(0); if(describe(&light)) return 32;
    lightState(1); client.time+=1.f; if(describe(&light)) return 33;
    client.time=100; lightState(1); client.time=0; if(describe(&light)) return 34;
    printf("PASS: actual renderer CoF light bridge, main/phone/Maglite, 30s continuous refresh each, off/stale/clock-reset; D3D spotlight descriptors without device/window.\n");
    using LightStateV2=int(__cdecl*)(const dx_cof_light_state_v2*);
    auto publishV2=reinterpret_cast<LightStateV2>(GetProcAddress(dll,"DX_CofLightStateV2"));
    dx_cof_light_state_v2 publication;publication.rawFlags=publication.eligibleFlags=8;
    publication.time=client.time;publication.origin[0]=123;publication.direction[1]=1;
    lightPlayer.index=0; // A missing/mismatched beam entity must not stop V2.
    if(!publishV2||!publishV2(&publication)||!describe(&light)||std::fabs(light.Diffuse.r-.35f)>1e-6f)return 41;
    if(!pose(&lightViewmodel,client.time,camera,angles,offsets,points))return 58;
    const float distance=std::sqrt((points[3]-points[0])*(points[3]-points[0])+(points[4]-points[1])*(points[4]-points[1])+(points[5]-points[2])*(points[5]-points[2]));
    const float position[3]={light.Position.x,light.Position.y,light.Position.z},direction[3]={light.Direction.x,light.Direction.y,light.Direction.z};
    for(int i=0;i<3;++i)if(fabsf(position[i]-points[i])>.01f||fabsf(direction[i]-(points[3+i]-points[i])/distance)>.001f)return 59;
    publication.eligibleFlags=0;publishV2(&publication);if(describe(&light))return 42;
    publication.size=0;if(publishV2(&publication)||describe(&light))return 43;
    std::puts("PASS: actual compiled renderer V2 export builds dimmed lens-attached spotlight without player entity lookup; off/invalid publication clears it.");
    // A switched-on light remains on when stowed, with a dimmer ground beam.
    const auto flashlightBytes=lightModelBytes;secondaryLightBytes=flashlightBytes;
    secondaryLightModel=lightModel;
    std::ifstream gun(modelPath.substr(0,slash)+"/weapons/glock/v_glock.mdl",std::ios::binary);
    lightModelBytes.assign(std::istreambuf_iterator<char>(gun),{});
    strcpy_s(lightModel.name,"models/weapons/glock/v_glock.mdl");
    lightViewmodel.curstate.sequence=0;
    publication.size=sizeof(publication);publication.rawFlags=publication.eligibleFlags=8;publication.time=client.time;
    if(!publishV2(&publication)||!describe(&light)||fabsf(light.Direction.z+.93969262f)>.001f||fabsf(light.Diffuse.r-.2625f)>.0001f)return 70;
    // Glock attachments exist: merely checking attachment count is insufficient.
    if(!pose(&lightViewmodel,client.time,camera,angles,offsets,points))return 71;
    publication.rawFlags=publication.eligibleFlags=12;
    if(!publishV2(&publication)||!describe(&light)||fabsf(light.Diffuse.r-.5f)>.0001f)return 72;
    using Dual=int(__cdecl*)(const cl_entity_t*,int,float);
    auto dual=reinterpret_cast<Dual>(GetProcAddress(dll,"DX_CofDualViewModelV1"));if(!dual)return 73;
    cl_entity_t secondary=lightViewmodel;secondary.model=&secondaryLightModel;
    publication.rawFlags=publication.eligibleFlags=8;
    if(!dual(&secondary,1,client.time)||!publishV2(&publication)||!describe(&light))return 74;
    if(!pose(&secondary,client.time,camera,angles,offsets,points)||fabsf(light.Position.x-points[0])>.01f||fabsf(light.Position.y+points[1])>.01f||fabsf(light.Position.z-points[2])>.01f)return 75;
    if(!dual(nullptr,0,client.time)||!describe(&light)||fabsf(light.Direction.z+.93969262f)>.001f)return 76;
    if(!dual(&secondary,1,client.time))return 77;
    client.time+=.3f;publication.time=client.time;
    if(!publishV2(&publication)||!describe(&light)||fabsf(light.Direction.z+.93969262f)>.001f)return 78;
    client.time=0;publication.time=0;
    lightModelBytes=flashlightBytes;strcpy_s(lightModel.name,"models/weapons/flashlight/v_flashlight.mdl");
    if(!publishV2(&publication)||!describe(&light))return 79;
    publication.rawFlags=publication.eligibleFlags=0;publishV2(&publication);if(describe(&light))return 80;
    puts("PASS: stowed switched-on flashlight emits a 25%-dimmer ground beam; actual gun attachments are not used as its lens; mounted weapon light, held/mirrored second-hand flashlight, re-equip and true off pass. No game.");
    // Exercise the actual phone model through production publication -> light
    // description, not just the attachment helper. Native uses attachment[1].
    auto savedLightBytes=std::move(lightModelBytes);
    std::ifstream phone(modelPath.substr(0,slash)+"/weapons/mobile/v_mobile.mdl",std::ios::binary);
    lightModelBytes.assign(std::istreambuf_iterator<char>(phone),{});
    if(lightModelBytes.size()<sizeof(studiohdr_t))return 60;
    strcpy_s(lightModel.name,"models/weapons/mobile/v_mobile.mdl");
    auto phoneHdr=reinterpret_cast<studiohdr_t*>(lightModelBytes.data());unsigned phonePoses=0;
    publication.size=sizeof(publication);publication.direction[0]=0;publication.direction[1]=1;publication.direction[2]=0;
    for(unsigned flags:{1u,2u,3u})for(int seq=0;seq<phoneHdr->numseq;++seq)for(float t:{0.f,.1f,.2f}){
        lightViewmodel.curstate.sequence=seq;client.time=publication.time=30+phonePoses*.25f+t;
        publication.rawFlags=publication.eligibleFlags=flags;
        lightViewmodel.origin[0]=camera[0]=48;lightViewmodel.angles[1]=angles[1]=90;
        if(!pose(&lightViewmodel,client.time,camera,angles,offsets,points)||!publishV2(&publication)||!describe(&light))return 61;
        const float positionPhone[3]={light.Position.x,light.Position.y,light.Position.z};
        for(int i=0;i<3;++i)if(fabsf(positionPhone[i]-points[3+i])>.01f){
            printf("FAIL: phone light uses camera instead of attachment[1]: actual=(%.3f %.3f %.3f), expected=(%.3f %.3f %.3f)\n",light.Position.x,light.Position.y,light.Position.z,points[3],points[4],points[5]);return 62;}
        if(fabsf(light.Direction.x)>.001f||fabsf(light.Direction.y-1)>.001f||fabsf(light.Direction.z)>.001f||fabsf(light.Diffuse.r-((flags&1)?.46f:.15f))>.0001f)return 63;
        ++phonePoses;
    }
    // The held phone keeps normal camera aim and brightness.
    lightViewmodel.curstate.sequence=0;
    for(float yaw:{-179.f,-90.f,0.f,90.f,179.f})for(float pitch:{-89.f,-45.f,0.f,45.f,89.f}) {
        const float y=yaw*.0174532925199433f,p=pitch*.0174532925199433f;
        publication.direction[0]=std::cos(p)*std::cos(y);publication.direction[1]=std::cos(p)*std::sin(y);publication.direction[2]=-std::sin(p);
        publication.rawFlags=publication.eligibleFlags=1;publication.time=client.time;
        if(!publishV2(&publication)||!describe(&light))return 81;
        if(fabsf(light.Direction.x-publication.direction[0])>.001f||fabsf(light.Direction.y-publication.direction[1])>.001f||fabsf(light.Direction.z-publication.direction[2])>.001f||fabsf(light.Diffuse.r-.46f)>.0001f)return 82;
    }
    // Stowed phone and torch: ignore vertical camera pitch, preserve heading,
    // and dim exactly 25 percent. Exercise production descriptors, not a mock.
    strcpy_s(lightModel.name,"models/weapons/switchblade/v_switchblade.mdl");
    for(unsigned flags:{1u,2u,8u})for(float yaw:{-179.f,-90.f,0.f,90.f,179.f})for(float pitch:{-89.f,-45.f,0.f,45.f,89.f}) {
        const float y=yaw*.0174532925199433f,p=pitch*.0174532925199433f;
        publication.direction[0]=std::cos(p)*std::cos(y);publication.direction[1]=std::cos(p)*std::sin(y);publication.direction[2]=-std::sin(p);
        publication.rawFlags=publication.eligibleFlags=flags;publication.time=client.time;
        publication.origin[0]=123.f+yaw;publication.origin[1]=-45.f;publication.origin[2]=100.f+pitch;
        if(!publishV2(&publication)||!describe(&light))return 84;
        if(fabsf(light.Position.x-publication.origin[0]-4.f*std::cos(y))>.001f||
           fabsf(light.Position.y-publication.origin[1]-4.f*std::sin(y))>.001f||
           fabsf(light.Position.z-publication.origin[2]+24.f)>.001f)return 87;
        const float brightness=(flags==8?.35f:flags==1?.46f:.15f)*.75f;
        if(fabsf(light.Direction.x-.34202014f*std::cos(y))>.001f||fabsf(light.Direction.y-.34202014f*std::sin(y))>.001f||fabsf(light.Direction.z+.93969262f)>.001f||fabsf(light.Diffuse.r-brightness)>.0001f)return 85;
    }
    lightViewmodel.model=nullptr;publication.rawFlags=publication.eligibleFlags=8;publication.reasons=DX_COF_NO_VIEWMODEL;
    if(!publishV2(&publication)||!describe(&light)||fabsf(light.Direction.z+.93969262f)>.001f)return 86;
    lightViewmodel.model=&lightModel;publication.reasons=0;strcpy_s(lightModel.name,"models/weapons/mobile/v_mobile.mdl");
    publication.rawFlags=publication.eligibleFlags=1;
    for(float yaw:{-179.f,-90.f,0.f,90.f,179.f})for(float vertical:{-1.f,1.f}) {
        float direction[3]={0,0,vertical};const float y=yaw*.0174532925199433f;
        if(!DX_CofGroundPhoneDirection(direction,yaw)||fabsf(direction[0]-.34202014f*std::cos(y))>.001f||fabsf(direction[1]-.34202014f*std::sin(y))>.001f||fabsf(direction[2]+.93969262f)>.001f)return 83;
    }
    publication.direction[0]=0;publication.direction[1]=1;publication.direction[2]=0;
    const int phoneAttachmentOffset=phoneHdr->attachmentindex;phoneHdr->attachmentindex=phoneHdr->length;
    if(!publishV2(&publication)||describe(&light))return 64;phoneHdr->attachmentindex=phoneAttachmentOffset;
    publication.rawFlags=publication.eligibleFlags=4;publishV2(&publication);
    if(!describe(&light)||fabsf(light.Position.x-publication.origin[0])>.001f||fabsf(light.Position.y-publication.origin[1]-4)>.001f)return 65;
    publication.rawFlags=publication.eligibleFlags=0;publishV2(&publication);if(describe(&light))return 66;
    lightModelBytes=std::move(savedLightBytes);strcpy_s(lightModel.name,"models/weapons/flashlight/v_flashlight.mdl");
    printf("PASS: held phone attachment/normal aim across %u poses and 25 pitch/yaw pairs; stowed phone/torch origin 24 units below eyes and 4 forward, always 70 degrees down and 25%% dimmer across 75 pitch/yaw cases, both vertical poles and bare hands; invalid attachments, mounted light and off preserved. No device/window.\n",phonePoses);
    client.time=0;
    const int attachmentCount=std::min(hdr->numattachments,4);
    int samples=0; float first[3]{}; bool moved=false;
    for(int seq=0;seq<hdr->numseq;++seq) {
        ent.curstate.sequence=seq;
        for(int step=0;step<5;++step) {
            float point[12]; const float time=step*.15f;
            if(!query(&ent,time,point,attachmentCount)) return 6;
            if(samples==0) memcpy(first,point,sizeof first);
            for(int axis=0;axis<3;++axis) {
                if(!std::isfinite(point[axis])) return 7;
                if(fabsf(first[axis]-point[axis])>.1f) moved=true;
            }
            // A server origin shift must move camera and rendered model together.
            ent.origin[2]+=48.f;
            float shifted[12]; if(!query(&ent,time,shifted,attachmentCount)) return 8;
            for(int i=0;i<attachmentCount;++i) if(fabsf(shifted[i*3+2]-point[i*3+2]-48.f)>.01f) return 8;
            ent.origin[2]-=48.f; ++samples;
        }
    }
    if(attachmentCount==3) {
        // The seat can stay still while the oars move. Compare within each
        // rowing sequence, not across idle/row sequence changes.
        for(int seq=1;seq<hdr->numseq;++seq) {
            ent.curstate.sequence=seq; float start[12],later[12];
            if(!query(&ent,0,start,3) || !query(&ent,.3f,later,3)) return 12;
            bool oarMoved=false;
            for(int i=3;i<9;++i) if(fabsf(start[i]-later[i])>.1f) oarMoved=true;
            if(!oarMoved) return 13;
        }
        printf("PASS: oar attachment motion sampled within each actual boat rowing sequence.\n");
    }
    float sentinel[12]={123,456,789};
    if(query(nullptr,0,sentinel,attachmentCount) || query(&ent,std::numeric_limits<float>::quiet_NaN(),sentinel,attachmentCount) ||
       query(&ent,0,sentinel,0) || query(&ent,0,sentinel,5)) return 9;
    const int old=hdr->attachmentindex; hdr->attachmentindex=hdr->length;
    if(query(&ent,0,sentinel,attachmentCount) || sentinel[0]!=123) return 10;
    hdr->attachmentindex=old;
    // player_sick's camera anchor stays fixed relative to the chair. Entity
    // translation/yaw must move it, but arm animation need not bob the head.
    const bool wheelchairModel=modelPath.substr(slash+1)=="player_sick.mdl";
    if(!moved&&!wheelchairModel) return 11;
    if(wheelchairModel){
        float base[12],turned[12];ent.curstate.sequence=0;ent.angles[1]=0;
        if(!query(&ent,0,base,1))return 67;
        ent.angles[1]=90;if(!query(&ent,0,turned,1))return 68;
        if(fabsf(turned[0]+base[1])>.01f||fabsf(turned[1]-base[0])>.01f||fabsf(turned[2]-base[2])>.01f)return 69;
        ent.angles[1]=0;
        std::puts("PASS: wheelchair static seat attachment follows entity yaw and +48 translation across all four sequences; arm animation does not move its head anchor.");
    }
    printf("PASS: actual D3D9 CPU pose query, %d samples of %s (%d attachments); animation, +48 origin translation, null/NaN/truncated attachment rejection. No device/window/game.\n",samples,argv[2],attachmentCount);
    return 0;
}
