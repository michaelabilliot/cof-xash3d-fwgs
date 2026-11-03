#include "dx_ragdoll.h"
#include "ragdoll_physics.h"

namespace {
convar_t enabled={(char*)"dx_ragdolls",(char*)"1",FCVAR_ARCHIVE};
convar_t gravity={(char*)"dx_ragdoll_gravity",(char*)"800",FCVAR_ARCHIVE};
convar_t lifetime={(char*)"dx_ragdoll_lifetime",(char*)"30",FCVAR_ARCHIVE};
convar_t debug={(char*)"dx_ragdoll_debug",(char*)"0",0};
convar_t maximum={(char*)"dx_ragdoll_max",(char*)"12",FCVAR_ARCHIVE};
convar_t sleepSetting={(char*)"dx_ragdoll_sleep",(char*)"1",FCVAR_ARCHIVE};
convar_t hitReactions={(char*)"dx_ragdoll_hit_reactions",(char*)"1",FCVAR_ARCHIVE};
convar_t impactScale={(char*)"dx_ragdoll_impact_scale",(char*)"1.0",FCVAR_ARCHIVE};
convar_t reactionScale={(char*)"dx_ragdoll_reaction_scale",(char*)"1.0",FCVAR_ARCHIVE};
// Keep authored boss/setpiece deaths on their native path by default. The
// list is configurable because custom maps can repurpose the same models.
convar_t excluded={(char*)"dx_ragdoll_exclude",(char*)
    "models/carcassboss.mdl;models/chainsawguy.mdl;models/chainsawguy2.mdl;models/sewer_boss.mdl;models/doctor_boss.mdl;models/booksimon.mdl",FCVAR_ARCHIVE};
struct Entry {
    int index=0,serial=0,modelindex=0,body=0;
    model_t *model=nullptr;
    char modelName[128]{};
    int modelLength=0;
    ragdoll::Doll *doll=nullptr;
    double seen=0,born=0,historyTime=-1;
    double olderTime=-1,lastDiagnostic=-1;
    bool transitionReported=false;
    int liveSequence=-1,deathSource=0;
    float liveHealth=0;
    unsigned impactSeen=0,physicalSeen=0,reportedCorrections=0;
    unsigned reportedSettles=0;
    int reportedRejection=0;
    double reactionTime=-1,visibleTime=-1;
    bool reactionProofPending=false,submissionProofPending=false;
    float reactionDelta=0;
    ragdoll::Reactions reactions;
    std::vector<ragdoll::Bone> reactionRig;
    std::vector<btTransform> reacted,visible,raw;
    enum class Activation { Pending, Active, Fallback };
    Activation activation=Activation::Pending;
    std::vector<btTransform> history,older;
    btVector3 liveVelocity{0,0,0};
    std::array<std::array<char,32>,ragdoll::MaxBones> boneNames{};
    std::array<int,ragdoll::MaxBones> boneParents{};
    int boneCount=0;
    std::array<float,ragdoll::MaxBones> scales{};
};
struct Brush {int index,serial;model_t *model;ragdoll::Mesh *mesh;};
std::unique_ptr<ragdoll::World> physics;
std::vector<Entry> entries;
std::vector<Brush> brushes;
model_t *worldModel=nullptr;
model_t *failedWorld=nullptr;
double lastTime=-1;

const ref_ragdoll_entity_t *snapshot(int index) {
    return gEngfuncs.EngineGetParm && gEngfuncs.EngineGetParm(PARM_GET_RAGDOLL_ENTITY,-2)==REF_RAGDOLL_SNAPSHOT_VERSION ? reinterpret_cast<const ref_ragdoll_entity_t*>(
        gEngfuncs.EngineGetParm(PARM_GET_RAGDOLL_ENTITY,index)):nullptr;
}
float setting(const convar_t &c,float fallback,float lo,float hi) {
    return std::isfinite(c.value)?std::clamp(c.value,lo,hi):fallback;
}
btTransform fromWorld(const D3DMATRIX &m,float &scale) {
    btMatrix3x3 basis(m._11,m._21,m._31,m._12,m._22,m._32,m._13,m._23,m._33);
    scale=basis.getColumn(0).length();
    if(scale<=0 || !std::isfinite(scale)) return btTransform(btQuaternion(0,0,0,1),btVector3(NAN,0,0));
    basis=basis.scaled(btVector3(1/scale,1/scale,1/scale));
    return btTransform(basis,btVector3(m._41,m._42,m._43)*ragdoll::UnitsToMeters);
}
btTransform boneWorld(const BoneMat34 &m,const D3DMATRIX &world,float &scale) {
    D3DXMATRIX local(m[0][0],m[1][0],m[2][0],0,
        m[0][1],m[1][1],m[2][1],0,m[0][2],m[1][2],m[2][2],0,
        m[0][3],m[1][3],m[2][3],1);
    D3DXMATRIX composed=local*D3DXMATRIX(world);
    return fromWorld(composed,scale);
}
bool selectedBones(const studiohdr_t *hdr,int body,std::array<bool,ragdoll::MaxBones> &used) {
    if(hdr->numbodyparts<=0 || hdr->numbodyparts>64 || !R_StudioRangeValid(hdr,hdr->bodypartindex,static_cast<size_t>(hdr->numbodyparts)*sizeof(mstudiobodyparts_t))) return false;
    const auto *data=reinterpret_cast<const byte*>(hdr);
    const auto *parts=reinterpret_cast<const mstudiobodyparts_t*>(data+hdr->bodypartindex);
    const auto *bones=reinterpret_cast<const mstudiobone_t*>(data+hdr->boneindex);
    for(int i=0;i<hdr->numbones;++i) if(bones[i].parent<-1 || bones[i].parent>=i) return false;
    for(int i=0;i<hdr->numbodyparts;++i) {
        const auto &part=parts[i];
        if(part.nummodels<=0 || part.nummodels>256 || !R_StudioRangeValid(hdr,part.modelindex,static_cast<size_t>(part.nummodels)*sizeof(mstudiomodel_t))) return false;
        int selected=part.base>0?(body/part.base)%part.nummodels:0;if(selected<0) selected+=part.nummodels;
        const auto &model=reinterpret_cast<const mstudiomodel_t*>(data+part.modelindex)[selected];
        if(model.numverts<0 || model.numverts>MAXSTUDIOVERTS || !R_StudioRangeValid(hdr,model.vertinfoindex,static_cast<size_t>(model.numverts))) return false;
        for(int v=0;v<model.numverts;++v) {const int bone=data[model.vertinfoindex+v];if(bone>=hdr->numbones) return false;used[bone]=true;}
    }
    for(int i=hdr->numbones-1;i>=0;--i) if(used[i] && bones[i].parent>=0) used[bones[i].parent]=true;
    return true;
}
bool buildBones(const cl_entity_t *ent,studiohdr_t *hdr,const D3DMATRIX &world,const BoneMat34 *pose,
    std::array<float,ragdoll::MaxBones> &scales,std::vector<ragdoll::Bone> &bones,const std::vector<btTransform> *capture=nullptr) {
    if(!hdr || hdr->numbones<1 || hdr->numbones>ragdoll::MaxBones) return false;
    if(hdr->numhitboxes<0 || hdr->numhitboxes>256 || !R_StudioRangeValid(hdr,hdr->hitboxindex,static_cast<size_t>(hdr->numhitboxes)*sizeof(mstudiobbox_t))) return false;
    bones.assign(static_cast<size_t>(hdr->numbones),ragdoll::Bone{});
        auto *source=reinterpret_cast<const mstudiobone_t*>(reinterpret_cast<const byte*>(hdr)+hdr->boneindex);
        auto *boxes=reinterpret_cast<const mstudiobbox_t*>(reinterpret_cast<const byte*>(hdr)+hdr->hitboxindex);
        std::array<bool,ragdoll::MaxBones> used{},hasBounds{};
        if(!selectedBones(hdr,ent->curstate.body,used)) return false;
        for(int i=0;i<hdr->numbones;++i) {
            bones[i].parent=source[i].parent;
            if(source[i].parent>=i || source[i].parent<-1) {return false;}
            bones[i].pose=boneWorld(pose[i],world,scales[i]);
            bones[i].region=ragdoll::region(source[i].name,sizeof(source[i].name),0);
            if(capture && capture->size()==bones.size()) bones[i].pose=(*capture)[i];
            if(!ragdoll::valid(bones[i].pose)) {return false;}
        }
        for(int i=hdr->numbones-1;i>=0;--i) if(used[i] && source[i].parent>=0) used[source[i].parent]=true;
        for(int i=0;i<hdr->numhitboxes;++i) {
            const auto &box=boxes[i];if(box.bone<0 || box.bone>=hdr->numbones) {return false;}
            auto &b=bones[box.bone];
            const float factor=ragdoll::UnitsToMeters*scales[box.bone];
            btVector3 lo(box.bbmin[0]*factor,box.bbmin[1]*factor,box.bbmin[2]*factor);
            btVector3 hi(box.bbmax[0]*factor,box.bbmax[1]*factor,box.bbmax[2]*factor);
            if(!ragdoll::finite(lo) || !ragdoll::finite(hi) || hi.x()<=lo.x() || hi.y()<=lo.y() || hi.z()<=lo.z()) continue;
            if(hasBounds[box.bone]) {b.minimum.setMin(lo);b.maximum.setMax(hi);} else {b.minimum=lo;b.maximum=hi;}
            hasBounds[box.bone]=true;b.collision=used[box.bone];b.group=box.group;
            b.region=ragdoll::region(source[box.bone].name,sizeof(source[box.bone].name),box.group);
        }
        // Add the lowest common ancestor to connect otherwise separate limbs.
        std::array<int,ragdoll::MaxBones> commonForRoot;commonForRoot.fill(-1);
        for(int i=0;i<hdr->numbones;++i) if(bones[i].collision) {
            int root=i;while(bones[root].parent>=0) root=bones[root].parent;
            if(root<0 || root>=ragdoll::MaxBones) return false;
            int &common=commonForRoot[root];
            if(common<0) {common=i;continue;}
            int candidate=common;
            while(candidate>=0) {
                int p=i;while(p>=0 && p!=candidate) p=bones[p].parent;
                if(p==candidate) break;
                candidate=bones[candidate].parent;
            }
            if(candidate<0) {return false;}common=candidate;
        }
        for(int common:commonForRoot) if(common>=0 && !bones[common].collision) {
            bones[common].collision=true;bones[common].minimum=btVector3(-.04f,-.04f,-.04f);bones[common].maximum=-bones[common].minimum;
        }
    return true;
}
bool writePalette(const D3DMATRIX &world,const std::array<float,ragdoll::MaxBones> &scales,
    const btTransform *result,int count,BoneMat34 *pose) {
    D3DXMATRIX inverse;const D3DXMATRIX matrix(world);
    if(!D3DXMatrixInverse(&inverse,nullptr,&matrix)) return false;
    for(int i=0;i<count;++i) if(!ragdoll::valid(result[i])) return false;
    for(int i=0;i<count;++i) {
        const auto &b=result[i].getBasis();const auto &o=result[i].getOrigin();const float s=scales[i];
        D3DXMATRIX m(b[0][0]*s,b[1][0]*s,b[2][0]*s,0,b[0][1]*s,b[1][1]*s,b[2][1]*s,0,
            b[0][2]*s,b[1][2]*s,b[2][2]*s,0,o.x()/ragdoll::UnitsToMeters,o.y()/ragdoll::UnitsToMeters,o.z()/ragdoll::UnitsToMeters,1);
        m=m*inverse;for(int row=0;row<3;++row) for(int col=0;col<4;++col) pose[i][row][col]=m.m[col][row];
    }
    return true;
}
bool newHit(unsigned id,unsigned previous) {return id && (!previous || static_cast<int32_t>(id-previous)>0);}
void publishOwnership(const Entry &e);
void publishOwnership(const Entry &e,int owner);
#include "dx_ragdoll_retained.h"
template<class Callback> void consumeHits(Entry &entry,const ref_ragdoll_entity_t &state,bool physical,Callback &&callback) {
    unsigned &seen=physical?entry.physicalSeen:entry.impactSeen;
    for(int i=0;i<std::clamp(state.hit_count,0,REF_RAGDOLL_MAX_HITS);++i) {
        const auto &hit=state.hits[i];if(!newHit(hit.id,seen)) continue;
        // A lethal cluster remains owned until physics accepts it. A prior
        // nonlethal reaction may never be replayed just because death follows.
        if(!physical && hit.lethal) continue;
        if(physical && !hit.lethal && !newHit(hit.id,entry.impactSeen)) {seen=hit.id;continue;}
        if(!std::isfinite(hit.time) || (!hit.lethal && (state.server_time<hit.time || state.server_time-hit.time>.4f))) {seen=hit.id;continue;}
        if(callback(hit)) {
            seen=hit.id;
            if(physical) {
                entry.impactSeen=hit.id;
                if(auto *ack=reinterpret_cast<unsigned*>(gEngfuncs.EngineGetParm(PARM_RAGDOLL_ACK,entry.index))) *ack=hit.id;
            }
        } else if(hit.lethal) break;
    }
}
bool hasHit(const Entry &entry,const ref_ragdoll_entity_t &state) {
    for(int i=0;i<std::clamp(state.hit_count,0,REF_RAGDOLL_MAX_HITS);++i) if(!state.hits[i].lethal && newHit(state.hits[i].id,entry.impactSeen)) return true;
    return false;
}
bool specialAnimation(const studiohdr_t *hdr,const cl_entity_t *ent) {
    if(ent->curstate.sequence<0 || ent->curstate.sequence>=hdr->numseq || !R_StudioRangeValid(hdr,hdr->seqindex,static_cast<size_t>(hdr->numseq)*sizeof(mstudioseqdesc_t))) return true;
    const auto *seq=reinterpret_cast<const mstudioseqdesc_t*>(reinterpret_cast<const byte*>(hdr)+hdr->seqindex)+ent->curstate.sequence;
    char label[33]{};memcpy(label,seq->label,32);
    return strstr(label,"appear") || strstr(label,"script") || strstr(label,"suicide") || strstr(label,"locker") || strstr(label,"train");
}
bool meshVertices(model_t *model,std::vector<btVector3> &vertices) {
    int first=0,count=0;
    if(!R_GetBrushSurfaceRange(model,first,count) || count>200000) return false;
    for(int i=0;i<count;++i) {
        const msurface_t *s=&model->surfaces[first+i];
        if(s->flags&(SURF_DRAWSKY|SURF_DRAWTURB)) continue;
        if(s->numedges<3 || s->numedges>4096) continue;
        const mvertex_t *a=R_GetBrushSurfaceVertex(model,s,0);
        if(!a) return false;
        for(int v=1;v+1<s->numedges;++v) {
            const mvertex_t *b=R_GetBrushSurfaceVertex(model,s,v),*c=R_GetBrushSurfaceVertex(model,s,v+1);
            if(!b || !c || vertices.size()>ragdoll::MaxTriangles*3-3) return false;
            for(const auto *p:{a,b,c}) vertices.emplace_back(btVector3(p->position[0],p->position[1],p->position[2])*ragdoll::UnitsToMeters);
        }
    }
    return !vertices.empty();
}
#include "dx_ragdoll_transition.h"
#include "dx_ragdoll_capture.h"
void invalidateRetained(int index);
void eraseEntry(Entry &entry) { for(auto &c:captures)if(c.index)c.complete=false;publishOwnership(entry,REF_RAGDOLL_UNOWNED); if(entry.doll) invalidateRetained(entry.index);if(physics) physics->remove(entry.doll);entry.doll=nullptr; }
bool identity(const Entry &entry,const cl_entity_t *ent,const ref_ragdoll_entity_t *s) {
    return ent && s && ent->model && s->serial==entry.serial && s->modelindex>0 && !(s->effects&EF_NODRAW);
}
int deathState(const ref_ragdoll_entity_t &s,const cl_entity_t &ent,const studiohdr_t *hdr) {
    if(s.native_death>0) return 3;
    for(int i=0;i<std::clamp(s.hit_count,0,REF_RAGDOLL_MAX_HITS);++i) if(s.hits[i].lethal) return 4;
    if(s.dead) return 1;
    if(!std::isfinite(s.health) || s.health>0 || ent.curstate.sequence<0 || ent.curstate.sequence>=hdr->numseq ||
        !R_StudioRangeValid(hdr,hdr->seqindex,static_cast<size_t>(hdr->numseq)*sizeof(mstudioseqdesc_t))) return 0;
    const auto *seq=reinterpret_cast<const mstudioseqdesc_t*>(reinterpret_cast<const byte*>(hdr)+hdr->seqindex)+ent.curstate.sequence;
    char label[33]{};memcpy(label,seq->label,32);
    // Actual CoF Slower/Faster/Croucher sequence labels, gated by server health.
    return !_strnicmp(label,"die",3) || !_strnicmp(label,"death",5) || !_strnicmp(label,"headdeath",9)?2:0;
}
void skip(const cl_entity_t *ent,const ref_ragdoll_entity_t *s,int reason,const char *text) {
    struct Last {int index=0,serial=0,model=0,reason=0;};static std::array<Last,64> last{};
    if(debug.value==0 || !ent || !s || !s->monster || (!s->dead && s->native_death<=0 && s->health>0)) return;
    auto &record=last[static_cast<unsigned>(ent->index)%last.size()];
    if(record.index==ent->index && record.serial==s->serial && record.model==ent->curstate.modelindex && record.reason==reason) return;
    record={ent->index,s->serial,ent->curstate.modelindex,reason};
    gEngfuncs.Con_Printf("RAGDOLL skip: entity=%d class=%s model=%s reason=%s health=%.1f deadflag=%d server_seq=%d render_seq=%d\n",
        ent->index,s->classname,ent->model?ent->model->name:"none",text,s->health,s->deadflag,s->sequence,ent->curstate.sequence);
}
}
void DX_RagdollRegister() {
    for(auto *c:{&enabled,&gravity,&lifetime,&debug,&maximum,&excluded,&sleepSetting,&hitReactions,&impactScale,&reactionScale}) gEngfuncs.Cvar_RegisterVariable(c);
}
void DX_RagdollReset(const char *reason) {
    for(auto &capture:captures)if(capture.index){capture.complete=false;finishCapture(capture);}
    captures.clear();captureSlots=0;
    if(physics && debug.value!=0) gEngfuncs.Con_Printf("RAGDOLL reset: reason=%s instances=%u time=%.6f\n",reason,static_cast<unsigned>(entries.size()),lastTime);
    for(const auto &entry:entries) {
        if(!strcmp(reason,"renderer-shutdown")) {saveEntry(entry);auto *saved=retained(entry.index);if(!saved || !saved->bytes)publishOwnership(entry,REF_RAGDOLL_UNOWNED);}
        else publishOwnership(entry,REF_RAGDOLL_UNOWNED);
    }
    motionReferences.clear();
    entries.clear();brushes.clear();physics.reset();worldModel=nullptr;failedWorld=nullptr;lastTime=-1;
}
bool DX_RagdollCandidate(const cl_entity_t *ent) {
    if(enabled.value==0 || !ent || !ent->model || ent->player || ent->index<=0 ||
        ent->curstate.movetype==MOVETYPE_FOLLOW || (ent->curstate.effects&EF_NODRAW)) return false;
    const char *name=ent->model->name;
    const auto *s=snapshot(ent->index);
    if(!_strnicmp(name,"models/cutscene/",16) || !_strnicmp(name,"models/v_",9)) {skip(ent,s,1,"viewmodel/cinematic");return false;}
    if(excluded.string) for(const char *token=excluded.string;*token;) {
        const char *end=strchr(token,';');if(!end) end=token+strlen(token);
        const size_t length=static_cast<size_t>(end-token);
        if(length && strlen(name)==length && !_strnicmp(name,token,length)) {skip(ent,s,2,"authored-exclusion");return false;}
        token=*end?end+1:end;
    }
    if(!s || s->effects&EF_NODRAW) return false;
    const bool tracked=std::any_of(entries.begin(),entries.end(),[&](const Entry &e){return e.index==ent->index && e.serial==s->serial && (e.doll || (e.historyTime>=0 && e.liveHealth>0));});
    if(tracked) return true; // death ownership survives flag/model notification ordering
    if(s->monster && s->modelindex!=ent->curstate.modelindex) skip(ent,s,3,"awaiting-model-update");
    return s->monster && s->modelindex==ent->curstate.modelindex;
}
void DX_RagdollPaletteProof(const cl_entity_t *ent,const BoneMat34 *input,const BoneMat34 *uploaded,int count,const char *path) {
    if(debug.value==0 || !ent || !input || !uploaded || count<=0 || count>ragdoll::MaxBones) return;
    for(auto &e:entries) if(e.index==ent->index && e.reactionProofPending && e.reactionDelta>1e-5f) {
        const bool exact=!memcmp(input,uploaded,static_cast<size_t>(count)*sizeof(BoneMat34));
        gEngfuncs.Con_Printf("RAGDOLL palette: entity=%d event=%u path=%s bones=%d exact=%d displacement=%.6f m owner=animation+reaction\n",e.index,e.impactSeen,path,count,exact?1:0,e.reactionDelta);
        e.reactionProofPending=false;break;
    }
}
void DX_RagdollSubmitted(const cl_entity_t *ent,const studio_cache_t &cache,const BoneMat34 *bones,int count,const D3DMATRIX *world,const char *path) {
    if(debug.value==0 || !ent || !world || !bones || count<1 || count>ragdoll::MaxBones) return;
    captureSubmitted(ent,cache,bones,count,*world);
    for(auto &e:entries) if(e.index==ent->index && e.submissionProofPending && e.raw.size()==static_cast<size_t>(count)) {
        float peak=0;
        for(const auto &batch:cache.batches) for(const auto &v:batch.sources) {
            if(v.bone>=count) continue;
            float scale;const auto finalBone=boneWorld(bones[v.bone],*world,scale);
            const btVector3 local=btVector3(v.x,v.y,v.z)*(ragdoll::UnitsToMeters*e.scales[v.bone]);
            peak=std::max(peak,(finalBone*local-e.raw[v.bone]*local).length());
        }
        if(peak>1e-5f) {
            gEngfuncs.Con_Printf("RAGDOLL submitted: entity=%d event=%u path=%s bones=%d peak_vertex_delta=%.6f m owner=animation+reaction\n",e.index,e.impactSeen,path,count,peak);
            e.submissionProofPending=false;
        }
        break;
    }
}
void DX_RagdollDeviceReset(const char *reason) {
    if(debug.value!=0 && physics) gEngfuncs.Con_Printf("RAGDOLL device reset: reason=%s instances=%u world=preserved poses=preserved\n",reason,static_cast<unsigned>(entries.size()));
}
void DX_RagdollFrame() {
    if(!gp_cl || !g_worldmodel) {if(physics) DX_RagdollReset("world-unavailable");return;}
    // Input destination is not world lifetime. A console/menu suspends the
    // local server while its entities and every corpse remain owned.
    const double now=gp_cl->time;
    if(!std::isfinite(now)) return;
    if(worldModel && worldModel!=g_worldmodel) DX_RagdollReset("map-model-changed");
    if(enabled.value==0 || !gEngfuncs.EngineGetParm(PARM_CLIENT_ACTIVE,0) ||
        !gEngfuncs.EngineGetParm(PARM_CLIENT_INGAME,0)) {lastTime=now;return;}
    const int available=static_cast<int>(gEngfuncs.EngineGetParm(PARM_GET_RAGDOLL_ENTITY,-1));
    if(available<=0) {lastTime=now;return;}
    // Client interpolation clocks can rewind on resume. R_NewMap owns actual
    // load/map invalidation; a clock correction must not recreate bodies.
    if(lastTime>=0 && now<lastTime) lastTime=now;
    if(failedWorld==g_worldmodel) return;
    if(!physics) {
        physics=std::make_unique<ragdoll::World>();worldModel=g_worldmodel;
        std::vector<btVector3> vertices;
        if(!meshVertices(g_worldmodel,vertices) || !physics->addMesh(vertices,false)) {
            if(debug.value!=0) gEngfuncs.Con_Printf("RAGDOLL: map collision unavailable; animation fallback\n");
            physics.reset();failedWorld=g_worldmodel;return;
        }
        entries.reserve(128);brushes.reserve(128);
        if(debug.value!=0) gEngfuncs.Con_Printf("RAGDOLL: map collision %u triangles\n",static_cast<unsigned>(vertices.size()/3));
    }
    // Restore every retained island before simulation, including offscreen
    // support corpses. A visible body may be resting on an unseen retired one.
    if(gEngfuncs.Mod_Extradata && gEngfuncs.EngineGetParm(PARM_GET_RAGDOLL_RETAINED,-2)==REF_RAGDOLL_RETAIN_VERSION && gEngfuncs.EngineGetParm(PARM_GET_RAGDOLL_RETAINED,-3)>0)
    for(int slot=0;slot<REF_RAGDOLL_RETAIN_COUNT && entries.size()<128;++slot) {
        auto *saved=reinterpret_cast<ref_ragdoll_retained_t*>(gEngfuncs.EngineGetParm(PARM_GET_RAGDOLL_RETAINED,-65536-slot));
        if(!saved || !saved->bytes) continue;
        const int index=saved->index;if(index<=0 || index>=available) continue;
        if(std::any_of(entries.begin(),entries.end(),[&](const Entry &e){return e.index==index;})) continue;
        auto *entity=gEngfuncs.R_BeamGetEntity(index);const auto *state=snapshot(index);
        if(!entity || !entity->model || !state || state->serial!=saved->serial || !DX_RagdollCandidate(entity)) continue;
        auto *hdr=static_cast<studiohdr_t*>(gEngfuncs.Mod_Extradata(mod_studio,entity->model));
        if(!R_StudioHeaderValid(hdr) || hdr->numbones<3 || hdr->numbones>ragdoll::MaxBones) continue;
        Entry restored;restored.index=index;restored.serial=state->serial;restored.model=entity->model;
        restored.modelindex=entity->curstate.modelindex;restored.body=entity->curstate.body;restored.seen=now;
        if(restoreEntry(restored,entity,hdr)) entries.push_back(std::move(restored));
    }
    // Metadata covers the entire local server, so offscreen removal/reuse is
    // handled independently of studio draw order and PVS visibility.
    entries.erase(std::remove_if(entries.begin(),entries.end(),[now](Entry &entry) {
        const auto *ent=gEngfuncs.R_BeamGetEntity(entry.index);
        const auto *s=snapshot(entry.index);
        if(!identity(entry,ent,s) || !DX_RagdollCandidate(ent) || (entry.doll && !s->dead && s->health>0 && (entry.deathSource==3?s->native_death==0:s->native_death<=0)) ||
            (!entry.doll && now-entry.seen>2)) {eraseEntry(entry);return true;}
        publishOwnership(entry);
        if(debug.value!=0 && entry.doll && entry.doll->transition.shot && !entry.doll->transition.active && !entry.transitionReported) {
            const auto &t=entry.doll->transition;gEngfuncs.Con_Printf("RAGDOLL release: entity=%d shot=%u reference_seq=%d elapsed=%.4f muscle_work=%.5f muscle_budget=%.5f J owner=dynamic\n",entry.index,t.shot,t.sequence,t.elapsed,t.work,t.budget);entry.transitionReported=true;
        }
        if(entry.doll && !entry.doll->frozen && lifetime.value>0 &&
            now-entry.born>=setting(lifetime,30,.1f,600) && physics->safeToRetire(*entry.doll)) physics->freeze(*entry.doll);
        if(debug.value!=0 && entry.doll && entry.reportedSettles!=entry.doll->settleCount) {
            gEngfuncs.Con_Printf("RAGDOLL settled: entity=%d supported=1 final_pose=preserved sleep_count=%u\n",entry.index,entry.doll->settleCount);
            entry.reportedSettles=entry.doll->settleCount;
        }
        if(debug.value!=0 && s->hit_rejected && entry.reportedRejection!=s->hit_rejected) {
            gEngfuncs.Con_Printf("RAGDOLL hit rejected: entity=%d reason=%d (1=no-damage-evidence 2=generation 3=not-verified-bullet-source)\n",entry.index,s->hit_rejected);
            entry.reportedRejection=s->hit_rejected;
        }
        if(entry.doll && ((debug.value!=0 && entry.doll->velocityCorrections!=entry.reportedCorrections) || (debug.value>=2 && !entry.doll->settled && entry.doll->age>3)) && now-entry.lastDiagnostic>=2) {
            gEngfuncs.Con_Printf("RAGDOLL velocity: entity=%d peak=%.3f m/s spin_peak=%.3f rad/s corrections=%u linear=%u angular=%u supported=%d energy=%.4f J joint_error=%.4f m quiet=%.2f stable=%.2f speed=%.4f spin=%.4f penetration=%.4f wakes=%u self_contacts=%u self_depth=%.4f blockers=0x%x owner=physics\n",
                entry.index,entry.doll->maximumSpeed,entry.doll->maximumAngularSpeed,entry.doll->velocityCorrections,entry.doll->linearCorrections,entry.doll->angularCorrections,entry.doll->supported?1:0,
                entry.doll->restEnergy,entry.doll->restJointError,entry.doll->quietTime,entry.doll->stationaryTime,entry.doll->currentSpeed,entry.doll->currentSpin,entry.doll->restPenetration,entry.doll->wakeCount,entry.doll->selfContacts,entry.doll->selfPenetration,entry.doll->restBlockers);
            entry.reportedCorrections=entry.doll->velocityCorrections;entry.lastDiagnostic=now;
        }
        return false;
    }),entries.end());
    const int limit=static_cast<int>(setting(maximum,12,1,32));
    int active=0;
    for(auto &entry:entries) if(entry.doll && !entry.doll->frozen) ++active;
    for(auto &entry:entries) if(active>limit && entry.doll && !entry.doll->frozen && physics->safeToRetire(*entry.doll)) {physics->freeze(*entry.doll);--active;}

    // Cache each BSP mesh once. Detached/hidden brushes retain no collision;
    // their cached object is masked until the same model is solid again.
    const int count=std::clamp(std::min(available,static_cast<int>(gEngfuncs.EngineGetParm(PARM_MAX_ENTITIES,0))),0,32768);
    for(auto &brush:brushes) brush.mesh->body->setCollisionFlags(brush.mesh->body->getCollisionFlags()|btCollisionObject::CF_NO_CONTACT_RESPONSE);
    for(int i=1;i<count;++i) {
        auto *ent=gEngfuncs.R_BeamGetEntity(i);
        const auto *s=snapshot(i);
        if(!ent || !s || s->solid!=SOLID_BSP || !ent->model || ent->model->type!=mod_brush ||
            s->modelindex!=ent->curstate.modelindex) continue;
        auto found=std::find_if(brushes.begin(),brushes.end(),[i,ent](const Brush &b){return b.index==i && b.model==ent->model;});
        if(found==brushes.end()) {
            if(brushes.size()>=256) continue;
            std::vector<btVector3> vertices;
            if(!meshVertices(ent->model,vertices)) continue;
            auto *mesh=physics->addMesh(vertices,true);if(!mesh) continue;
            brushes.push_back({i,s->serial,ent->model,mesh});found=brushes.end()-1;
        }
        if(found->serial!=s->serial) {found->serial=s->serial;found->mesh->initialized=false;}
        D3DMATRIX matrix;R_SetEntityWorldTransform(ent,static_cast<float>(now),&matrix);
        float scale;const btTransform transform=fromWorld(matrix,scale);
        if(!ragdoll::valid(transform) || btFabs(scale-1)>.001f) continue;
        found->mesh->body->setCollisionFlags(btCollisionObject::CF_KINEMATIC_OBJECT);
        physics->moveMesh(*found->mesh,transform);
    }
    physics->sleeping=sleepSetting.value!=0;
    if(lastTime>=0 && !gp_cl->paused) {const float g=setting(gravity,800,0,4000);physics->step(now-lastTime,g);captureSteps(now,now-lastTime,g);}
    lastTime=now;
}
bool DX_RagdollPose(const cl_entity_t *ent,studiohdr_t *hdr,const D3DMATRIX &world,BoneMat34 *pose,float time) {
    if(enabled.value==0 || !physics || !DX_RagdollCandidate(ent)) return false;
    if(!R_StudioHeaderValid(hdr) || hdr->numbones<3 || hdr->numbones>ragdoll::MaxBones) {
        skip(ent,snapshot(ent->index),6,"invalid-studio-header");return false;
    }
    const ref_ragdoll_entity_t state=*snapshot(ent->index);
    auto it=std::find_if(entries.begin(),entries.end(),[ent](const Entry &e){return e.index==ent->index;});
    if(it!=entries.end() && !identity(*it,ent,&state)) {eraseEntry(*it);entries.erase(it);it=entries.end();}
    if(it==entries.end()) {
        if(entries.size()>=128) return false;
        entries.emplace_back();it=entries.end()-1;
        it->index=ent->index;it->serial=state.serial;it->model=ent->model;
        it->modelindex=state.modelindex;it->body=ent->curstate.body;
    }
    Entry &entry=*it;entry.seen=time;
    strncpy_s(entry.modelName,ent->model->name,_TRUNCATE);entry.modelLength=hdr->length;
    const auto *skeleton=reinterpret_cast<const mstudiobone_t*>(reinterpret_cast<const byte*>(hdr)+hdr->boneindex);
    if(entry.model!=ent->model || entry.body!=ent->curstate.body || entry.modelindex!=ent->curstate.modelindex) {
        bool compatible=entry.boneCount==hdr->numbones;
        for(int i=0;compatible && i<hdr->numbones;++i) compatible=entry.boneParents[i]==skeleton[i].parent && !memcmp(entry.boneNames[i].data(),skeleton[i].name,32);
        if(entry.doll && !compatible) {eraseEntry(entry);entry.activation=Entry::Activation::Fallback;skip(ent,&state,4,"incompatible-corpse-skeleton");}
        else if(entry.doll) {
            std::array<bool,ragdoll::MaxBones> used{};
            if(selectedBones(hdr,ent->curstate.body,used)) physics->updateVisibility(*entry.doll,used);
            if(debug.value!=0) gEngfuncs.Con_Printf("RAGDOLL ownership: entity=%d compatible model/body swap preserved; hidden-part contacts disabled; no reactivation\n",entry.index);
        }
        if(!entry.doll && entry.activation==Entry::Activation::Pending && !compatible) {entry.history.clear();entry.older.clear();entry.visible.clear();entry.reactions={};entry.historyTime=entry.olderTime=entry.visibleTime=-1;}
        entry.model=ent->model;entry.modelindex=ent->curstate.modelindex;entry.body=ent->curstate.body;
    }
    const int deathSource=deathState(state,*ent,hdr);
    if(!entry.doll && deathSource) restoreEntry(entry,ent,hdr);
    if(!entry.doll) {
        publishOwnership(entry,REF_RAGDOLL_UNOWNED);
        if(entry.activation==Entry::Activation::Fallback) return false;
        if(!deathSource) {
            if(!std::isfinite(state.health) || state.health<=0) {skip(ent,&state,7,"awaiting-death-evidence");return false;}
            const float elapsed=entry.reactionTime>=0?static_cast<float>(std::max(0.0,static_cast<double>(time)-entry.reactionTime)):0;
            if(gEngfuncs.EngineGetParm(PARM_CLIENT_INGAME,0) && !gp_cl->paused) entry.reactions.step(elapsed);
            entry.reactionTime=time;
            entry.raw.resize(static_cast<size_t>(hdr->numbones));
            for(int i=0;i<hdr->numbones;++i) entry.raw[i]=boneWorld(pose[i],world,entry.scales[i]);
            bool overridePose=false;
            if(entry.reactions.active() || hasHit(entry,state)) {
                const bool validRig=buildBones(ent,hdr,world,pose,entry.scales,entry.reactionRig);
                const bool permitted=hitReactions.value!=0 && !specialAnimation(hdr,ent) && validRig;
                consumeHits(entry,state,false,[&](const ref_ragdoll_hit_t &hit) {
                    btVector3 point=btVector3(hit.position[0],hit.position[1],hit.position[2])*ragdoll::UnitsToMeters;
                    if(validRig && hit.local_valid && hit.bone>0 && hit.bone<=hdr->numbones) point=entry.reactionRig[hit.bone-1].pose*(btVector3(hit.local_position[0],hit.local_position[1],hit.local_position[2])*(ragdoll::UnitsToMeters*entry.scales[hit.bone-1]));
                    const btVector3 direction(hit.direction[0],hit.direction[1],hit.direction[2]);
                    const int bone=validRig?(ragdoll::hitBone(entry.reactionRig,point,hit.group,hit.bone-1)):-1;
                    const float impulse=ragdoll::shotBudget(hit.weapon?hit.weapon:REF_RAGDOLL_PISTOL,hit.total_pellets,setting(reactionScale,1,0,3))*
                        static_cast<float>(std::clamp(hit.pellets,1,24))/static_cast<float>(std::clamp(hit.total_pellets,1,24));
                    if(permitted && bone>=0) {entry.reactions.hit(entry.reactionRig,bone,point,direction,impulse);entry.reactionProofPending=true;entry.submissionProofPending=true;}
                    if(debug.value!=0) gEngfuncs.Con_Printf("RAGDOLL living hit: entity=%d event=%u shot=%u weapon=%d pellets=%d/%d group=%d bone=%d strength=%.3f applied=%d owner=animation+reaction\n",
                        entry.index,hit.id,hit.shot,hit.weapon,hit.pellets,hit.total_pellets,hit.group,bone,impulse,permitted && bone>=0?1:0);
                    return true;
                });
                if(validRig && entry.reactions.active()) {
                    entry.reactions.apply(entry.reactionRig,entry.reacted);
                    overridePose=writePalette(world,entry.scales,entry.reacted.data(),hdr->numbones,pose);
                    entry.reactionDelta=0;
                    if(overridePose) for(int i=0;i<hdr->numbones;++i) {
                        float scale;
                        const auto finalBone=boneWorld(pose[i],world,scale);
                        entry.reactionDelta=std::max(entry.reactionDelta,(finalBone.getOrigin()-entry.raw[i].getOrigin()).length());
                    }
                }
            }
            entry.visible.resize(static_cast<size_t>(hdr->numbones));entry.visibleTime=time;
            for(int i=0;i<hdr->numbones;++i) entry.visible[i]=boneWorld(pose[i],world,entry.scales[i]);
            if(entry.historyTime!=time && (entry.historyTime<0 || time-entry.historyTime>=.008)) {
                if(entry.liveSequence==ent->curstate.sequence && (state.health>=entry.liveHealth || entry.historyTime<0)) {entry.older=entry.history;entry.olderTime=entry.historyTime;}
                else {entry.older.clear();entry.olderTime=-1;}
                entry.history.resize(static_cast<size_t>(hdr->numbones));
                for(int i=0;i<hdr->numbones;++i) {
                    entry.history[i]=entry.raw[i];
                    if(!ragdoll::valid(entry.history[i])) {entry.history.clear();return false;}
                }
                entry.boneCount=hdr->numbones;
                for(int i=0;i<hdr->numbones;++i) {memcpy(entry.boneNames[i].data(),skeleton[i].name,32);entry.boneParents[i]=skeleton[i].parent;}
                // Retain pre-hit locomotion. Damage-frame knockback must not be
                // inherited as both whole-body speed and a localized impulse.
                if(state.health>=entry.liveHealth || entry.historyTime<0) {
                    entry.liveVelocity=btVector3(state.velocity[0],state.velocity[1],state.velocity[2])*ragdoll::UnitsToMeters;
                    if(state.grounded && entry.liveVelocity.z()>0) entry.liveVelocity.setZ(0);
                }
                entry.liveSequence=ent->curstate.sequence;entry.liveHealth=state.health;
                entry.historyTime=time;
            }
            return overridePose;
        }
        entry.activation=Entry::Activation::Fallback;
        entry.deathSource=deathSource;
        if(hdr->numhitboxes<3 || hdr->numhitboxes>256 || !R_StudioRangeValid(hdr,hdr->hitboxindex,static_cast<size_t>(hdr->numhitboxes)*sizeof(mstudiobbox_t))) {
            skip(ent,&state,5,"invalid-hitboxes");return false;
        }
        std::vector<ragdoll::Bone> bones;
        const auto *capture=entry.visibleTime>=0 && time-entry.visibleTime>=0 && time-entry.visibleTime<=.15?&entry.visible:nullptr;
        if(!buildBones(ent,hdr,world,pose,entry.scales,bones,capture)) {skip(ent,&state,9,"invalid-bodygroup-or-hierarchy");return false;}
        entry.activation=Entry::Activation::Fallback;
        int active=0,totalBodies=0,requiredBodies=0;
        for(const auto &bone:bones) if(bone.collision) ++requiredBodies;
        for(const auto &other:entries) if(other.doll && !other.doll->frozen) {++active;totalBodies+=static_cast<int>(other.doll->parts.size());}
        const int limit=static_cast<int>(setting(maximum,12,1,32));
        for(auto &other:entries) if((active>=limit || totalBodies+requiredBodies>512) && other.doll && !other.doll->frozen && physics->safeToRetire(*other.doll)) {
            totalBodies-=static_cast<int>(other.doll->parts.size());physics->freeze(*other.doll);--active;
        }
        if(active>=limit || totalBodies+requiredBodies>512) {entry.activation=Entry::Activation::Pending;skip(ent,&state,10,"capacity-active-bodies-retained");return false;}
        const bool recent=entry.historyTime>=0 && time-entry.historyTime>=0 && time-entry.historyTime<=.15;
        std::vector<btVector3> reactionAngular;
        if(recent && entry.raw.size()==bones.size() && entry.reactions.active()) {
            reactionAngular.resize(bones.size());
            for(size_t i=0;i<bones.size();++i) {
                reactionAngular[i]=entry.raw[i].getBasis()*entry.reactions.bones[i].angular;
                if(bones[i].parent>=0) reactionAngular[i]+=reactionAngular[bones[i].parent];
            }
        }
        entry.doll=physics->create(bones,recent?entry.liveVelocity:btVector3(0,0,0),recent?&entry.older:nullptr,
            static_cast<float>(entry.historyTime-entry.olderTime),recent?&entry.history:nullptr,&reactionAngular);entry.born=time;
        if(debug.value!=0) gEngfuncs.Con_Printf("RAGDOLL: entity=%d serial=%d model=%s bodies=%u result=%s\n",
            entry.index,entry.serial,ent->model->name,entry.doll?static_cast<unsigned>(entry.doll->parts.size()):0,entry.doll?"active":"animation fallback");
        if(!entry.doll) {skip(ent,&state,8,"physics-construction-rejected");return false;}
        entry.activation=Entry::Activation::Active;
        if(hitReactions.value!=0 && setting(impactScale,1,0,3)>0)for(int i=0;i<std::clamp(state.hit_count,0,REF_RAGDOLL_MAX_HITS);++i) {
            const auto &hit=state.hits[i];if(!hit.lethal || !hit.weapon || !newHit(hit.id,entry.physicalSeen) || !std::isfinite(hit.time) || state.server_time<hit.time || state.server_time-hit.time>.4f)continue;
            if(const auto *reference=motionReference(ent,hdr,*entry.doll)) {
                const bool active=physics->beginTransition(*entry.doll,hit.shot,reference->sequence,reference->peak,reference->angles);
                if(debug.value!=0)gEngfuncs.Con_Printf("RAGDOLL transition: entity=%d shot=%u reference_seq=%d peak=%.4f duration=%.4f supported=%d active=%d work_budget=%.5f J\n",entry.index,hit.shot,reference->sequence,reference->peak,2*reference->peak,entry.doll->supported,active,entry.doll->transition.budget);
            }
            break;
        }
        publishOwnership(entry);
        entry.boneCount=hdr->numbones;
        for(int i=0;i<hdr->numbones;++i) {memcpy(entry.boneNames[i].data(),skeleton[i].name,32);entry.boneParents[i]=skeleton[i].parent;}
        if(debug.value!=0) gEngfuncs.Con_Printf("RAGDOLL activation: entity=%d class=%s death_source=%d deadflag=%d native_death=%d health=%.1f server_seq=%d render_seq=%d pose=%s velocity=(%.2f %.2f %.2f) m/s penetration=%.4f m owner=physics\n",
            entry.index,state.classname,entry.deathSource,state.deadflag,state.native_death,state.health,state.sequence,ent->curstate.sequence,recent?"last-living":"evaluated-no-history",entry.liveVelocity.x(),entry.liveVelocity.y(),entry.liveVelocity.z(),entry.doll->initialPenetration);
        if(debug.value>=2) for(const auto &part:entry.doll->parts) {
            const auto &inertia=part.body->getInvInertiaDiagLocal();const auto &v=part.body->getLinearVelocity();
            gEngfuncs.Con_Printf("RAGDOLL body: entity=%d bone=%d name=%.32s region=%d mass=%.3f kg inverse_inertia=(%.2f %.2f %.2f) velocity=(%.2f %.2f %.2f) m/s\n",
                entry.index,part.bone,skeleton[part.bone].name,static_cast<int>(part.region),1/part.body->getInvMass(),inertia.x(),inertia.y(),inertia.z(),v.x(),v.y(),v.z());
        }
        beginCapture(entry,state,world,time);
        entry.history.clear();entry.older.clear();entry.visible.clear();
    }
    consumeHits(entry,state,true,[&](const ref_ragdoll_hit_t &event) {
        const float scale=setting(impactScale,1,0,3);
        const float impulse=event.weapon?ragdoll::shotBudget(event.weapon,event.total_pellets,scale)*
            static_cast<float>(std::clamp(event.pellets,1,24))/static_cast<float>(std::clamp(event.total_pellets,1,24)):3.f*scale;
        ragdoll::ImpactResult hit;if(impulse<=0)hit.rejected=ragdoll::ImpactRejection::Disabled;
        btVector3 point=btVector3(event.position[0],event.position[1],event.position[2])*ragdoll::UnitsToMeters;
        if(event.local_valid && event.bone>0 && event.bone<=entry.doll->count) {
            std::array<btTransform,ragdoll::MaxBones> physicalPose;physics->pose(*entry.doll,physicalPose,false);
            point=physicalPose[event.bone-1]*(btVector3(event.local_position[0],event.local_position[1],event.local_position[2])*(ragdoll::UnitsToMeters*entry.scales[event.bone-1]));
        }
        if(impulse>0)captureImpact(entry,event,point,impulse);
        if(impulse>0) hit=physics->impact(*entry.doll,point,
            btVector3(event.direction[0],event.direction[1],event.direction[2]),event.group,impulse,event.bone-1);
        // Projectile momentum is pellet-weighted above. An accepted regional
        // reflex uses its native amplitude, bounded by anatomical joint limits.
        if(hit.bone>=0 && event.shot==entry.doll->transition.shot) {
            physics->transitionHit(*entry.doll,hit,point,btVector3(event.direction[0],event.direction[1],event.direction[2]),1.f);
            if(debug.value>=2)for(size_t j=0;j<entry.doll->joints.size();++j) {
                const auto &muscle=entry.doll->transition.muscles[j];if(muscle.target.length2()<1e-8f)continue;
                gEngfuncs.Con_Printf("RAGDOLL reflex: entity=%d event=%u source=%d receiver=%d target=%.4f rad native=%.4f rad work=%.4f/%.4f J\n",entry.index,event.id,event.bone-1,entry.doll->joints[j]->getRigidBodyB().getUserIndex(),muscle.target.length(),muscle.referenceAngle,entry.doll->transition.work,entry.doll->transition.budget);
            }
        }
        if(debug.value!=0) gEngfuncs.Con_Printf("RAGDOLL impact: entity=%d event=%u shot=%u weapon=%d pellets=%d/%d group=%d source_bone=%d receptor_bone=%d bone=%d source_visible=%d remapped=%d body=%d scale=%.3f requested=%.3f N.s impulse=%.3f N.s lever=%.3f m dv=%.3f m/s dw=%.3f rad/s momentum=(%.3f %.3f %.3f) N.s applied=%d rejected=%d deduplicated=1\n",
            entry.index,event.id,event.shot,event.weapon,event.pellets,event.total_pellets,event.group,event.bone-1,hit.receptorBone,hit.bone,hit.sourceVisible?1:0,hit.remapped?1:0,entry.body,scale,impulse,hit.impulse,hit.lever,hit.deltaSpeed,hit.deltaSpin,hit.momentum.x(),hit.momentum.y(),hit.momentum.z(),hit.bone>=0?1:0,static_cast<int>(hit.rejected));
        return hit.bone>=0 || impulse<=0;
    });
    std::array<btTransform,ragdoll::MaxBones> result;
    physics->pose(*entry.doll,result);
    for(int i=0;i<hdr->numbones;++i) {
        if(!ragdoll::valid(result[i])) {physics->freeze(*entry.doll);return false;}
    }
    return writePalette(world,entry.scales,result.data(),hdr->numbones,pose);
}
