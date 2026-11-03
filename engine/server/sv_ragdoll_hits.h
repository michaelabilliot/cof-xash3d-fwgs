/* One native invocation owns a discharge, including misses and every recipient.
 * Engine edict storage owns the sparse caches; renderer reads never consume hits. */
typedef struct {
    int index,serial,spawncount,deadflag,native_before,rejected;
    float health;
    ref_ragdoll_claim_t claim;
    unsigned int shot,ack;
    int pending_count,history_count,lethal_count;
    ref_ragdoll_hit_t pending[24],history[REF_RAGDOLL_MAX_HITS],lethal[REF_RAGDOLL_MAX_HITS];
} ragdoll_hit_cache_t;
static ragdoll_hit_cache_t **ragdoll_hits;
static int ragdoll_hit_capacity;
static unsigned int ragdoll_hit_id,ragdoll_shot_id;
static struct {
    unsigned int id;
    uintptr_t frame;
    const byte *image;
    int shooter,serial,pellet,total,weapon,spawncount;
    double time;
    int recipients[24],count;
    ragdoll_hit_cache_t *previous;
    int previous_pellet;
    uint32_t before_entity;
    float before_amount;
} ragdoll_shot;
static void SV_RagdollInitHits(int capacity)
{
    ragdoll_hit_capacity=bound(0,capacity,32768);
    ragdoll_hits=(ragdoll_hit_cache_t**)Mem_Calloc(svgame.mempool,sizeof(*ragdoll_hits)*ragdoll_hit_capacity);
    memset(&ragdoll_shot,0,sizeof(ragdoll_shot));
}
static ragdoll_hit_cache_t *SV_RagdollHitRecord(int index,qboolean create)
{
    ragdoll_hit_cache_t *r;
    if(!ragdoll_hits || index<=0 || index>=ragdoll_hit_capacity || index>=svgame.numEntities || svgame.edicts[index].free) return NULL;
    r=ragdoll_hits[index];
    if(!r && create) r=ragdoll_hits[index]=(ragdoll_hit_cache_t*)Mem_Calloc(svgame.mempool,sizeof(*r));
    if(r && (r->serial!=svgame.edicts[index].serialnumber || r->spawncount!=svs.spawncount)) {
        memset(r,0,sizeof(*r));r->index=index;r->serial=svgame.edicts[index].serialnumber;r->spawncount=svs.spawncount;
    }
    return r;
}
ref_ragdoll_claim_t *SV_RagdollClaim(int index)
{
    ragdoll_hit_cache_t *r=SV_RagdollHitRecord(index,true);
    if(!r) return NULL;
    if(r->claim.version!=REF_RAGDOLL_CLAIM_VERSION || r->claim.generation!=svs.spawncount || r->claim.serial!=r->serial || r->claim.modelindex!=svgame.edicts[index].v.modelindex) {
        memset(&r->claim,0,sizeof(r->claim));r->claim.version=REF_RAGDOLL_CLAIM_VERSION;
        r->claim.generation=svs.spawncount;r->claim.index=index;r->claim.serial=r->serial;r->claim.modelindex=svgame.edicts[index].v.modelindex;
    }
    return &r->claim;
}
qboolean SV_RagdollOwnsPlayerCollision(int index,int modelindex)
{
    ragdoll_hit_cache_t *r;const edict_t *e;
    if(sv.state!=ss_active || svs.maxclients!=1 || Cvar_VariableValue("dx_ragdolls")==0) return false;
    r=SV_RagdollHitRecord(index,false);if(!r) return false;e=&svgame.edicts[index];
    if(r->claim.version!=REF_RAGDOLL_CLAIM_VERSION || r->claim.generation!=svs.spawncount || r->claim.serial!=e->serialnumber ||
       r->claim.modelindex!=modelindex || modelindex!=e->v.modelindex || r->claim.owner<REF_RAGDOLL_IMPACT || r->claim.owner>REF_RAGDOLL_RESTING) return false;
    // A renderer claim never changes damage, AI or native corpse state. Only
    // the obsolete standing box is omitted from player movement/prediction.
    if(e->v.deadflag==DEAD_NO && e->v.health>0 && SV_RagdollNativeKilled(e)==0) {r->claim.owner=REF_RAGDOLL_UNOWNED;return false;}
    return true;
}
static void SV_RagdollCheckpoint(qboolean newInvocation)
{
    ragdoll_hit_cache_t *r=ragdoll_shot.previous;
    uint32_t target;float amount,delta;
    if(!r || !ragdoll_shot.image || r->shot!=ragdoll_shot.id || ragdoll_shot.previous_pellet<0 || ragdoll_shot.previous_pellet>=r->pending_count) return;
    memcpy(&target,ragdoll_shot.image+0x22a700,4);memcpy(&amount,ragdoll_shot.image+0x22a704,4);
    if(newInvocation && !target) return; /* ClearMultiDamage already ran. State evidence still applies. */
    delta=0;
    if(target==(uint32_t)(uintptr_t)svgame.edicts[r->index].pvPrivateData) {
        delta=amount-(target==ragdoll_shot.before_entity?ragdoll_shot.before_amount:0);
        if(!isfinite(delta) || delta<0) return;
    }
    r->pending[ragdoll_shot.previous_pellet].damage=delta;
    ragdoll_shot.previous=NULL;
}
static void SV_RagdollAppend(ref_ragdoll_hit_t *queue,int *count,const ref_ragdoll_hit_t *hit)
{
    if(*count==REF_RAGDOLL_MAX_HITS) {memmove(queue,queue+1,sizeof(*queue)*(REF_RAGDOLL_MAX_HITS-1));--*count;}
    queue[(*count)++]=*hit;
}
static void SV_RagdollConfirmOne(ragdoll_hit_cache_t *r)
{
    const edict_t *victim;int native_after,lethal,count=0;ref_ragdoll_hit_t clusters[REF_RAGDOLL_MAX_HITS];
    if(!r || !r->pending_count) return;
    if(r->spawncount!=svs.spawncount || r->index>=svgame.numEntities) {r->pending_count=0;return;}
    victim=&svgame.edicts[r->index];
    if(victim->free || victim->serialnumber!=r->serial) {r->pending_count=0;r->rejected=2;return;}
    native_after=SV_RagdollNativeKilled(victim);
    lethal=(r->deadflag==DEAD_NO && victim->v.deadflag!=DEAD_NO) || (r->native_before==0 && native_after==1) || (r->health>0 && victim->v.health<=0);
    if(!(victim->v.health<r->health) && !lethal) {r->pending_count=0;r->rejected=1;return;}
    for(int i=0;i<r->pending_count;++i) {
        ref_ragdoll_hit_t *p=&r->pending[i];int cluster=-1;
        if(p->damage==0) continue;
        for(int j=0;j<count;++j) if(clusters[j].bone==p->bone && clusters[j].group==p->group) {cluster=j;break;}
        if(cluster<0) {if(count==REF_RAGDOLL_MAX_HITS) {r->rejected=4;continue;}cluster=count++;clusters[cluster]=*p;clusters[cluster].pellets=0;clusters[cluster].pellet_mask=0;clusters[cluster].damage=0;}
        {
            ref_ragdoll_hit_t *c=&clusters[cluster];const float weight=1.f/(float)(c->pellets+1);
            for(int k=0;k<3;++k) {c->position[k]+=(p->position[k]-c->position[k])*weight;c->direction[k]+=(p->direction[k]-c->direction[k])*weight;c->local_position[k]+=(p->local_position[k]-c->local_position[k])*weight;}
            c->pellets++;c->pellet_mask|=p->pellet_mask;
            c->damage=c->damage<0 || p->damage<0?-1:c->damage+p->damage;
        }
    }
    for(int i=0;i<count;++i) {
        ref_ragdoll_hit_t *hit=&clusters[i];if(!++ragdoll_hit_id) ++ragdoll_hit_id;
        hit->id=ragdoll_hit_id;hit->lethal=lethal;VectorNormalize(hit->direction);
        SV_RagdollAppend(r->history,&r->history_count,hit);
        if(lethal) SV_RagdollAppend(r->lethal,&r->lethal_count,hit);
        if(Cvar_VariableValue("dx_ragdoll_debug")>=1) Con_Printf("RAGDOLL delivery: entity=%d event=%u shot=%u pellets=%d/%d mask=%x bone=%d damage=%.3f lethal=%d health=%.3f->%.3f\n",r->index,hit->id,hit->shot,hit->pellets,hit->total_pellets,hit->pellet_mask,hit->bone-1,hit->damage,lethal,r->health,victim->v.health);
    }
    r->pending_count=0;if(count) r->rejected=0;
}
void SV_RagdollConfirmImpacts(void)
{
    SV_RagdollCheckpoint(false);
    for(int i=0;i<ragdoll_shot.count;++i) SV_RagdollConfirmOne(SV_RagdollHitRecord(ragdoll_shot.recipients[i],false));
    ragdoll_shot.id=0;ragdoll_shot.count=0;ragdoll_shot.previous=NULL;
}
/* Unsupported player-to-monster traces are candidates, not proof of a shot.
 * Keep this diagnostic bounded and independent of accepted-event state. */
static void SV_RagdollRejectTrace(const edict_t *shooter,const TraceResult *trace,int reason)
{
    static int generation=-1;static unsigned remaining;
    if(Cvar_VariableValue("dx_ragdoll_debug")<1 || !SV_IsValidEdict(shooter) || !FBitSet(shooter->v.flags,FL_CLIENT) || !SV_IsValidEdict(trace->pHit) || trace->flFraction>=1 ||
        (!FBitSet(trace->pHit->v.flags,FL_MONSTER) && Q_strncmp(SV_GetString(trace->pHit->v.classname),"monster_",8))) return;
    if(generation!=svs.spawncount) {generation=svs.spawncount;remaining=16;}
    if(!remaining) return;--remaining;
    Con_Printf("RAGDOLL trace candidate: entity=%d native_context=unsupported reason=%d (1=not-player 2=module 3=frame); no impact event created\n",NUM_FOR_EDICT(trace->pHit),reason);
}
static void SV_RagdollRecordTrace(const float *start,const float *end,edict_t *shooter,const TraceResult *trace,const ragdoll_native_shot_t *context)
{
    edict_t *victim=trace->pHit;ragdoll_hit_cache_t *r;vec3_t direction;int index;ref_ragdoll_hit_t *hit;
    qboolean fresh;
    if(!context || !SV_IsValidEdict(shooter) || !FBitSet(shooter->v.flags,FL_CLIENT)) return;
    fresh=!ragdoll_shot.id || ragdoll_shot.spawncount!=svs.spawncount || ragdoll_shot.frame!=context->frame || ragdoll_shot.shooter!=NUM_FOR_EDICT(shooter) || ragdoll_shot.serial!=shooter->serialnumber || context->pellet<=ragdoll_shot.pellet;
    SV_RagdollCheckpoint(fresh);
    if(fresh) {
        /* The preceding invocation has returned and its native damage is final. */
        for(int i=0;i<ragdoll_shot.count;++i) SV_RagdollConfirmOne(SV_RagdollHitRecord(ragdoll_shot.recipients[i],false));
        memset(&ragdoll_shot,0,sizeof(ragdoll_shot));
        if(!++ragdoll_shot_id) ++ragdoll_shot_id;
        ragdoll_shot.id=ragdoll_shot_id;ragdoll_shot.frame=context->frame;ragdoll_shot.image=context->image;
        ragdoll_shot.shooter=NUM_FOR_EDICT(shooter);ragdoll_shot.serial=shooter->serialnumber;ragdoll_shot.spawncount=svs.spawncount;
        ragdoll_shot.total=context->total;ragdoll_shot.weapon=context->weapon;ragdoll_shot.time=sv.time;
    }
    ragdoll_shot.pellet=context->pellet;
    if(!SV_IsValidEdict(victim) || trace->flFraction>=1 || trace->iHitgroup==8 || (!FBitSet(victim->v.flags,FL_MONSTER) && Q_strncmp(SV_GetString(victim->v.classname),"monster_",8))) return;
    VectorSubtract(end,start,direction);if(VectorNormalizeLength(direction)<=0) return;
    index=NUM_FOR_EDICT(victim);r=SV_RagdollHitRecord(index,true);if(!r) return;
    if(r->shot!=ragdoll_shot.id) {
        r->shot=ragdoll_shot.id;r->health=victim->v.health;r->deadflag=victim->v.deadflag;r->native_before=SV_RagdollNativeKilled(victim);r->pending_count=0;
        if(ragdoll_shot.count>=24) {r->rejected=4;return;}ragdoll_shot.recipients[ragdoll_shot.count++]=index;
    }
    if(r->pending_count>=24) {r->rejected=4;return;}
    hit=&r->pending[r->pending_count++];memset(hit,0,sizeof(*hit));hit->shot=ragdoll_shot.id;hit->weapon=context->weapon;
    hit->total_pellets=context->total;hit->pellets=1;hit->pellet_mask=1u<<(context->pellet-1);hit->group=trace->iHitgroup;
    hit->bone=SV_RagdollTraceBone(victim,start,end,trace->flFraction,hit->local_position);hit->time=(float)sv.time;hit->damage=-1;hit->local_valid=hit->bone>0;
    VectorCopy(trace->vecEndPos,hit->position);VectorCopy(direction,hit->direction);
    ragdoll_shot.previous=r;ragdoll_shot.previous_pellet=r->pending_count-1;
    ragdoll_shot.before_entity=context->damage_entity;ragdoll_shot.before_amount=context->damage_amount;
}
unsigned int *SV_RagdollAck(int index)
{
    ragdoll_hit_cache_t *r=SV_RagdollHitRecord(index,false);return r?&r->ack:NULL;
}
void SV_RagdollReadImpact(int index,ref_ragdoll_entity_t *snapshot)
{
    ragdoll_hit_cache_t *r=SV_RagdollHitRecord(index,false);
    snapshot->native_death=SV_RagdollNativeKilled(&svgame.edicts[index]);
    if(!r) return;snapshot->hit_rejected=r->rejected;
    /* Retained lethal clusters take precedence over the expiring live history. */
    for(int i=0;i<r->lethal_count;++i) if(!r->ack || (int32_t)(r->lethal[i].id-r->ack)>0) snapshot->hits[snapshot->hit_count++]=r->lethal[i];
    for(int i=0;i<r->history_count && snapshot->hit_count<REF_RAGDOLL_MAX_HITS;++i) {
        const ref_ragdoll_hit_t *hit=&r->history[i];qboolean duplicate=false;
        if(hit->lethal || sv.time<hit->time || sv.time-hit->time>.35) continue;
        for(int j=0;j<snapshot->hit_count;++j) if(snapshot->hits[j].id==hit->id) duplicate=true;
        if(!duplicate) snapshot->hits[snapshot->hit_count++]=*hit;
    }
    /* ID order makes cumulative acknowledgements unambiguous. */
    for(int i=1;i<snapshot->hit_count;++i) {ref_ragdoll_hit_t h=snapshot->hits[i];int j=i;while(j>0 && (int32_t)(h.id-snapshot->hits[j-1].id)<0) {snapshot->hits[j]=snapshot->hits[j-1];--j;}snapshot->hits[j]=h;}
    if(snapshot->hit_count) {const ref_ragdoll_hit_t *h=&snapshot->hits[snapshot->hit_count-1];snapshot->impact_id=h->id;snapshot->impact_group=h->group;VectorCopy(h->position,snapshot->impact_position);VectorCopy(h->direction,snapshot->impact_direction);}
}
