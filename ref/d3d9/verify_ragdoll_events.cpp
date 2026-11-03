// Standalone data/ABI test. No game code, DLL or graphics device is executed.
#include <windows.h>
#include <intrin.h>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <memory>
#include <vector>
using byte=unsigned char;using qboolean=int;using vec3_t=float[3];
constexpr int FL_CLIENT=8,FL_MONSTER=32,DEAD_NO=0,ss_active=2;
struct link_t {link_t *next=nullptr,*prev=nullptr;};
struct physent_t {int info=0,solid=0;};
struct playermove_t {int player_index=0,numphysent=0,numvisent=0,nummoveent=0;physent_t physents[600],visents[600],moveents[64];};
struct entity_state_t {int number=2,modelindex=2,owner=0,solid=3,skin=0;vec3_t mins{-12,-12,0},maxs{12,12,72};};
struct edict_t {link_t area;bool free=false;int serialnumber=1;void *pvPrivateData=nullptr;struct {int flags=0,deadflag=0,modelindex=2,groupinfo=0,solid=3,skin=0,movetype=4;edict_t *owner=nullptr;vec3_t size{24,24,72},absmin{-12,-12,0},absmax{12,12,72};float health=100;const char *classname="monster_slower",*viewmodel="models/v_shotgun.mdl";}v;};
#include "verify_ragdoll_event_types.inc"
static_assert(sizeof(TraceResult)==56 && offsetof(TraceResult,vecEndPos)==20 && offsetof(TraceResult,pHit)==48 && offsetof(TraceResult,iHitgroup)==52,"actual native x86 ABI");
static edict_t entities[520];
static struct {edict_t *edicts=entities;int numEntities=520;void *mempool=nullptr;playermove_t *pmove=nullptr;}svgame;
static struct {double time=1;int state=ss_active;void *current_client=nullptr;}sv;
static struct {int spawncount=1,groupop=0,maxclients=1;}svs;
static struct {bool demoplayback=false;entity_state_t packet_entities[1];int num_client_entities=1;}cls;
static std::vector<void*> allocations;
static void *allocate(size_t bytes) {void *p=calloc(1,bytes);if(!p) std::abort();allocations.push_back(p);return p;}
#define Mem_Calloc(pool,bytes) allocate(bytes)
#define bound(lo,v,hi) std::clamp(v,lo,hi)
#define FBitSet(a,b) ((a)&(b))
#define NUM_FOR_EDICT(e) static_cast<int>((e)-svgame.edicts)
#define Q_strncmp std::strncmp
#define Q_stristr std::strstr
#define VectorCopy(a,b) std::memcpy(b,a,sizeof(vec3_t))
#define VectorSubtract(a,b,o) do{for(int c=0;c<3;++c)(o)[c]=(a)[c]-(b)[c];}while(0)
static float norm(float *v) {float n=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);if(n)for(int c=0;c<3;++c)v[c]/=n;return n;}
#define VectorNormalizeLength(v) norm(v)
#define VectorNormalize(v) ((void)norm(v))
static bool SV_IsValidEdict(const edict_t *e) {return e && e>=entities && e<entities+520 && !e->free;}
static const char *SV_GetString(const char *s) {return s;}
static float ragdollsEnabled=1;
static float diagnosticLevel=0;static int diagnosticLines=0;
static float Cvar_VariableValue(const char *name) {return !strcmp(name,"dx_ragdolls")?ragdollsEnabled:diagnosticLevel;}
static void Con_Printf(const char*,...) {++diagnosticLines;}
alignas(4096) static byte nativeImage[0x259000];
static BOOL WINAPI moduleFor(DWORD,LPCSTR p,HMODULE *out) {auto a=(uintptr_t)p,b=(uintptr_t)nativeImage;if(a<b || a>=b+sizeof(nativeImage))return FALSE;*out=(HMODULE)nativeImage;return TRUE;}
#define GetModuleHandleExA moduleFor
#include "../../engine/server/sv_ragdoll_source.h"
#undef GetModuleHandleExA
static int SV_RagdollTraceBone(edict_t*,const float*,const float*,float,float *local) {local[0]=1;local[1]=2;local[2]=3;return 22;}
#include "../../engine/server/sv_ragdoll_hits.h"
#include "../../engine/client/dll_int/cl_ragdoll_retained.h"
struct mplane_t {float normal[3]{},dist=0;};
static mplane_t studio_planes[6];
#define DotProduct(a,b) ((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
#include "verify_ragdoll_hull.inc"
static void check(bool v,const char *message) {if(!v){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
static void nativeDamage(void *target,float amount) {uint32_t p=(uint32_t)(uintptr_t)target;memcpy(nativeImage+0x22a700,&p,4);memcpy(nativeImage+0x22a704,&amount,4);}
#include "verify_ragdoll_movement.h"
static ragdoll_native_shot_t ctx(int pellet=1,int total=1) {ragdoll_native_shot_t c{};c.frame=0x1234;c.image=nativeImage;c.weapon=total>1?2:1;c.pellet=pellet;c.total=total;memcpy(&c.damage_entity,nativeImage+0x22a700,4);memcpy(&c.damage_amount,nativeImage+0x22a704,4);return c;}
static void ray(int target,int pellet=1,int total=1,int group=1) {
    TraceResult tr{};tr.pHit=target?&entities[target]:nullptr;tr.flFraction=target?.25f:1;tr.vecEndPos[0]=32;tr.iHitgroup=group;
    const float start[3]={0,0,0},end[3]={128,0,0};auto c=ctx(pellet,total);SV_RagdollRecordTrace(start,end,&entities[1],&tr,&c);
}
static ref_ragdoll_entity_t read(int i) {ref_ragdoll_entity_t out{};SV_RagdollReadImpact(i,&out);return out;}
static void reset() {SV_RagdollInitHits(520);nativeDamage(nullptr,0);for(auto &e:entities){e.v.health=100;e.v.deadflag=0;}++sv.time;}
static void decoderChecks() {
    auto *dos=reinterpret_cast<IMAGE_DOS_HEADER*>(nativeImage);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=128;
    auto *nt=reinterpret_cast<IMAGE_NT_HEADERS*>(nativeImage+128);nt->Signature=IMAGE_NT_SIGNATURE;nt->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;nt->FileHeader.TimeDateStamp=0x5048ddb2;nt->OptionalHeader.SizeOfImage=sizeof(nativeImage);
#include "verify_ragdoll_native_image.inc"
    if(!SV_RagdollShotImage(nativeImage))std::fprintf(stderr,"IMAGE dos=%x pe=%x machine=%x timestamp=%x image_size=%x readable=%d p=%02x g=%02x pc=%02x gc=%02x\n",dos->e_magic,nt->Signature,nt->FileHeader.Machine,nt->FileHeader.TimeDateStamp,nt->OptionalHeader.SizeOfImage,SV_RagdollReadable(nativeImage,4096),nativeImage[0xaa250],nativeImage[0xa99c0],nativeImage[0xaa641],nativeImage[0xa9c25]);
    check(SV_RagdollShotImage(nativeImage),"native module identity and all fingerprints validated");
    nativeImage[0xaa641]^=1;check(!SV_RagdollShotImage(nativeImage),"modified primary call instruction rejected");nativeImage[0xaa641]^=1;

    alignas(16) byte frames[1024]{};byte object[8]{};uint32_t pev=(uint32_t)(uintptr_t)&entities[1].v;memcpy(object+4,&pev,4);
    const uintptr_t low=(uintptr_t)frames,high=low+sizeof(frames),cursor=low+32,frame=low+512;
    auto word=[](uintptr_t p,uint32_t v){memcpy((void*)p,&v,4);};ragdoll_native_shot_t c{};
    for(bool player:{false,true}) {
        word(cursor,(uint32_t)frame);word(cursor+4,(uint32_t)(uintptr_t)nativeImage+(player?0xaa646:0xa9c2a));word(frame-4,(uint32_t)(uintptr_t)object);
        word(frame-(player?0x6c:0x60),3);word(frame+(player?12:8),8);
        check(SV_RagdollDecodeShot(cursor,low,high,nativeImage,pev,&c) && c.pellet==3 && c.total==8 && c.frame==frame,"native player/generic frame and hidden-return ABI");
        check(SV_RagdollBulletContext(&entities[1],cursor,&c) && c.pellet==3 && c.total==8 && c.weapon==REF_RAGDOLL_SHOTGUN,"production stack walk decodes native-like frames within actual thread stack bounds");
        word(frame-(player?0x6c:0x60),25);check(!SV_RagdollDecodeShot(cursor,low,high,nativeImage,pev,&c),"out-of-range native pellet rejected");
        word(frame-(player?0x6c:0x60),3);word(cursor+4,(uint32_t)(uintptr_t)nativeImage+0xaa800);word(frame,0);check(!SV_RagdollDecodeShot(cursor,low,high,nativeImage,pev,&c),"secondary call site rejected");
    }
    word(cursor,(uint32_t)cursor);check(!SV_RagdollDecodeShot(cursor,low,high,nativeImage,pev,&c),"cyclic stack rejected");
    check(!SV_RagdollDecodeShot(cursor+1,low,high,nativeImage,pev,&c),"unaligned stack rejected");
    std::puts("PASS x86 native shot decoder: actual ABI declarations, both frame layouts, secondary sites, bounds and cycles");
}
int main() {
    for(int i=0;i<520;++i) {entities[i].pvPrivateData=&entities[i];entities[i].v.flags=FL_MONSTER;}entities[1].v.flags=FL_CLIENT;
    ragdoll_native_shot_t unsupported{};check(!SV_RagdollBulletContext(&entities[1],0,&unsupported),"unsupported stack has no source");check(unsupported.rejected==2,"unsupported module reason retained");decoderChecks();
    check(!SV_RagdollBulletContext(&entities[1],0,&unsupported) && unsupported.rejected==3,"invalid native frame reason retained");
    check(!SV_RagdollBulletContext(&entities[2],0,&unsupported) && unsupported.rejected==1,"non-player reason retained");
    TraceResult candidate{};candidate.pHit=&entities[2];candidate.flFraction=.25f;
    diagnosticLevel=1;for(int i=0;i<24;++i)SV_RagdollRejectTrace(&entities[1],&candidate,3);
    check(diagnosticLines==16 && read(2).hit_count==0,"unsupported trace diagnostics bounded and cannot manufacture damage events");
    ++svs.spawncount;SV_RagdollRejectTrace(&entities[1],&candidate,2);check(diagnosticLines==17,"map generation renews diagnostic allowance");diagnosticLevel=0;
    const float local[3]={2,3,4},maximum[3]={5,6,7},origin[3]={30,20,10};
    const float axes[3][3]={{0,1,0},{-1,0,0},{0,0,1}};float point[3]={30,20,10},converted[3]{};
    for(int i=0;i<3;++i){for(int j=0;j<3;++j){studio_planes[i*2].normal[j]=axes[i][j];point[j]+=axes[i][j]*local[i];}studio_planes[i*2].dist=DotProduct(axes[i],origin)+maximum[i];}
    Mod_StudioHullLocalPoint(0,point,maximum,converted);for(int i=0;i<3;++i)check(std::abs(converted[i]-local[i])<1e-5f,"cached trace hull retains hit coordinates under rotation and translation");
    reset();ray(2);nativeDamage(entities[2].pvPrivateData,10);entities[2].v.health=90;SV_RagdollConfirmImpacts();
    check(read(2).hit_count==1,"first recipient receives hit");nativeDamage(nullptr,0);ray(258);nativeDamage(entities[258].pvPrivateData,10);entities[258].v.health=90;SV_RagdollConfirmImpacts();check(read(2).hit_count==1 && read(258).hit_count==1,"edict 2/258 never alias");
    reset();ray(2);nativeDamage(entities[2].pvPrivateData,10);entities[2].v.health=90;nativeDamage(nullptr,0);ray(2);nativeDamage(entities[2].pvPrivateData,10);entities[2].v.health=80;SV_RagdollConfirmImpacts();auto o=read(2);check(o.hit_count==2 && o.hits[0].shot!=o.hits[1].shot,"sequential same-tick identical shots retain distinct IDs");
    reset();for(int i=1;i<=8;++i) {const int target=i%2?2:258;ray(target,i,8);nativeDamage(entities[target].pvPrivateData,10);entities[target].v.health-=10;}
    SV_RagdollConfirmImpacts();auto a=read(2),b=read(258);check(a.hit_count==1 && b.hit_count==1 && a.hits[0].shot==b.hits[0].shot && a.hits[0].total_pellets==8 && b.hits[0].total_pellets==8 && a.hits[0].pellets+b.hits[0].pellets==8,"one shotgun ID and budget across recipients");
    reset();for(int i=1;i<=8;++i) {ray(i==1?2:0,i,8);if(i==1){nativeDamage(entities[2].pvPrivateData,10);entities[2].v.health=90;}}
    SV_RagdollConfirmImpacts();o=read(2);check(o.hit_count==1 && o.hits[0].pellets==1 && o.hits[0].total_pellets==8,"misses retain the fired-pellet denominator");
    reset();ray(2);SV_RagdollConfirmImpacts();check(read(2).hit_count==0,"native rejected damage produces no reaction");
    reset();ray(2,1,2,8);ray(2,2,2,1);nativeDamage(entities[2].pvPrivateData,10);entities[2].v.health=90;SV_RagdollConfirmImpacts();o=read(2);check(o.hit_count==1 && o.hits[0].pellets==1,"zero-damage group excluded from mixed discharge");
    // The actual read-only death decoder must handle positive corpse health.
    auto *dos=reinterpret_cast<IMAGE_DOS_HEADER*>(nativeImage);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=128;
    auto *nt=reinterpret_cast<IMAGE_NT_HEADERS*>(nativeImage+128);nt->Signature=IMAGE_NT_SIGNATURE;nt->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;nt->FileHeader.TimeDateStamp=0x5048ddb2;nt->OptionalHeader.SizeOfImage=sizeof(nativeImage);
    const byte memRead[]={0x8b,0x88,0xe8,2,0,0},killedBit[]={0x68,0x80,0,0,0};memcpy(nativeImage+0x42df,memRead,6);memcpy(nativeImage+0x89b4c,killedBit,5);memcpy(nativeImage+0x4695c,killedBit,5);
    byte nativeObject[COF_MONSTER_BYTES]{};uint32_t table=(uint32_t)(uintptr_t)(nativeImage+0x1c0000),killed=(uint32_t)(uintptr_t)(nativeImage+0x89a80),pev=(uint32_t)(uintptr_t)&entities[2].v;
    memcpy(nativeObject,&table,4);memcpy(nativeObject+4,&pev,4);memcpy((void*)(table+COF_KILLED_SLOT),&killed,4);
    entities[2].pvPrivateData=nativeObject;reset();entities[2].v.health=5;ray(2);uint32_t memory=0x80;memcpy(nativeObject+COF_MONSTER_MEMORY,&memory,4);nativeDamage(nativeObject,7);entities[2].v.health=10;SV_RagdollConfirmImpacts();o=read(2);check(o.hit_count==1 && o.hits[0].lethal,"native Killed transition confirms 5-to-10 positive corpse health");auto *claim=SV_RagdollClaim(2);check(claim && claim->owner==REF_RAGDOLL_UNOWNED,"ownership starts unclaimed");
    claim->owner=REF_RAGDOLL_IMPACT;movementChecks();check(SV_RagdollOwnsPlayerCollision(2,2),"positive-health native death accepts successful impact handoff");
    svs.maxclients=2;check(!SV_RagdollOwnsPlayerCollision(2,2),"nonlocal prediction keeps native collision");svs.maxclients=1;
    ragdollsEnabled=0;check(!SV_RagdollOwnsPlayerCollision(2,2),"disabled renderer keeps native collision");ragdollsEnabled=1;
    check(!SV_RagdollOwnsPlayerCollision(2,3),"mismatched prediction model cannot ignore collision");
    claim->owner=REF_RAGDOLL_UNOWNED;check(!SV_RagdollOwnsPlayerCollision(2,2),"fallback/rejection keeps collision");claim->owner=REF_RAGDOLL_DYNAMIC;
    memory=0;memcpy(nativeObject+COF_MONSTER_MEMORY,&memory,4);entities[2].v.health=100;
    check(!SV_RagdollOwnsPlayerCollision(2,2) && !claim->owner,"native revival revokes collision claim");
    memory=0x80;memcpy(nativeObject+COF_MONSTER_MEMORY,&memory,4);claim->owner=REF_RAGDOLL_DYNAMIC;++entities[2].serialnumber;
    check(!SV_RagdollOwnsPlayerCollision(2,2) && SV_RagdollClaim(2)->owner==0,"edict reuse clears ownership");
    SV_RagdollClaim(2)->owner=REF_RAGDOLL_RESTING;++svs.spawncount;check(!SV_RagdollOwnsPlayerCollision(2,2),"map generation clears ownership");
    entities[2].pvPrivateData=&entities[2];
    reset();ray(2);nativeDamage(entities[2].pvPrivateData,100);entities[2].v.health=0;SV_RagdollConfirmImpacts();sv.time+=2;o=read(2);check(o.hit_count==1 && o.hits[0].lethal,"lethal event survives delayed visibility");*SV_RagdollAck(2)=o.hits[0].id;check(read(2).hit_count==0,"physical acknowledgement retires lethal replay");
    reset();ray(2);nativeDamage(entities[2].pvPrivateData,10);entities[2].v.health=90;SV_RagdollConfirmImpacts();++entities[2].serialnumber;check(read(2).hit_count==0,"serial reuse discards old events");
    auto *r=CL_RagdollRetained(2,true);check(r!=nullptr,"retention row acquired");r->bytes=32;r->data[0]=123;check(CL_RagdollRetained(2,true)==r && r->data[0]==123,"renderer-independent retention");++entities[2].serialnumber;r=CL_RagdollRetained(2,true);check(r && r->bytes==0,"serial reuse clears retained payload");r->bytes=32;++svs.spawncount;r=CL_RagdollRetained(2,true);check(r && !r->bytes,"map generation clears retained payload");
    check(CL_RagdollRetainedCount()==0 && !CL_RagdollRetainedSlot(0),"empty retained iteration remains empty");
    check(sizeof(*r)*REF_RAGDOLL_RETAIN_COUNT==8u*1024u*1024u,"retention including envelopes bounded at exactly 8 MiB");
    for(void *p:allocations)free(p);
    std::puts("PASS production event cache and engine retention: aliasing, same-tick shots, shared pellets, misses, rejection, lethal delivery, generations and bounds");
}
