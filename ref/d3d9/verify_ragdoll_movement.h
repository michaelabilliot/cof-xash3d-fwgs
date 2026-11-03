// Complete production movement list builders + production ownership predicate.
enum {SOLID_NOT=0,SOLID_TRIGGER=1,SOLID_BBOX=2,SOLID_SLIDEBOX=3,SOLID_BSP=4,CONTENTS_NONE=0,CONTENTS_EMPTY=-1,CONTENTS_LADDER=-16,MOVETYPE_PUSH=7,DEAD_DEAD=2,FL_FAKECLIENT=8192,FL_MONSTERCLIP=0x800000,MAX_PHYSENTS=600,MAX_MOVEENTS=64,mod_studio=3};
struct areanode_t {link_t solid_edicts;int axis=-1;float dist=0;areanode_t *children[2]{};};
struct frame_t {bool valid=true;int num_entities=1,first_entity=0;};
struct model_t {int type=mod_studio;struct {int lastclipnode=0;}hulls[4];};
static playermove_t serverMove{},clientMove{};static model_t studio;
static struct {int maxclients=1,playernum=0;}cl;
static struct {playermove_t *pmove=&clientMove;}clgame;
#define Assert(a) ::check(!!(a),"production movement assertion")
#define EDICT_FROM_AREA(p) reinterpret_cast<edict_t*>(reinterpret_cast<char*>(p)-offsetof(edict_t,area))
static edict_t *SV_EdictNum(int i){return entities+i;}
static bool SV_CheckGroupOp(int,int,int){return true;}
static void SV_GetTrueMinMax(void*,int,vec3_t,vec3_t){}
static bool VectorIsNull(const float *p){return p[0]==0&&p[1]==0&&p[2]==0;}
static bool BoundsIntersect(const float *a,const float *b,const float *c,const float *d){for(int i=0;i<3;++i)if(a[i]>d[i]||b[i]<c[i])return false;return true;}
static bool SV_CopyEdictToPhysEnt(physent_t *p,const edict_t *e){p->info=NUM_FOR_EDICT(e);p->solid=e->v.solid;return true;}
static model_t *CL_ModelHandle(int){return &studio;}
static void CL_CopyEntityToPhysEnt(physent_t *p,const entity_state_t *s,bool){p->info=s->number;p->solid=s->solid;}
#include "verify_ragdoll_server_movement.inc"
#include "verify_ragdoll_client_movement.inc"
static void movementChecks() {
    svgame.pmove=&serverMove;areanode_t node;node.solid_edicts.next=&entities[2].area;entities[2].area.next=&node.solid_edicts;
    const vec3_t lo{-128,-128,-128},hi{128,128,128};frame_t frame;auto *claim=SV_RagdollClaim(2);
    for(int phase=0;phase<4;++phase) {
        serverMove.numphysent=serverMove.numvisent=clientMove.numphysent=clientMove.numvisent=0;
        claim->owner=phase?REF_RAGDOLL_UNOWNED:REF_RAGDOLL_IMPACT;
        entities[2].v.deadflag=phase>=2?DEAD_DEAD:DEAD_NO;cls.packet_entities[0].maxs[2]=phase==3?1.f:72.f;
        SV_AddLinksToPmove(&node,lo,hi);CL_AddLinksToPmove(&frame);
        check(serverMove.numphysent==(phase==1?1:0),"server movement removes only claimed/dead native box");
        check(clientMove.numphysent==(phase==1||phase==2?1:0),"prediction shares successful ownership; fallback retains native gate");
        check(serverMove.numvisent==1 && clientMove.numvisent==1,"corpse remains in visual trace lists");
    }
    cls.packet_entities[0].maxs[2]=72;entities[2].v.deadflag=DEAD_NO;claim->owner=REF_RAGDOLL_IMPACT;
    std::puts("PASS production server/client movement lists: positive-health claimed death, fallback, native completion, visual traces");
}
