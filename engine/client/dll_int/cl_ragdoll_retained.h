/* Engine-owned slots, including envelopes: 128 * 64 KiB = 8 MiB. */
static ref_ragdoll_retained_t cl_ragdoll_records[REF_RAGDOLL_RETAIN_COUNT];
static int cl_ragdoll_generation=-1;
static qboolean CL_RagdollRetainedSync(void)
{
    if(cl_ragdoll_generation!=svs.spawncount) {memset(cl_ragdoll_records,0,sizeof(cl_ragdoll_records));cl_ragdoll_generation=svs.spawncount;}
    if(sv.state!=ss_active || !svgame.edicts || cls.demoplayback) return false;
    for(int i=0;i<REF_RAGDOLL_RETAIN_COUNT;++i) {
        ref_ragdoll_retained_t *r=&cl_ragdoll_records[i];
        if(r->index && (r->index>=svgame.numEntities || svgame.edicts[r->index].free || svgame.edicts[r->index].serialnumber!=r->serial)) memset(r,0,sizeof(*r));
    }
    return true;
}
static int CL_RagdollRetainedCount(void)
{
    int count=0;if(!CL_RagdollRetainedSync()) return 0;
    for(int i=0;i<REF_RAGDOLL_RETAIN_COUNT;++i) if(cl_ragdoll_records[i].bytes) ++count;
    return count;
}
static ref_ragdoll_retained_t *CL_RagdollRetainedSlot(int slot)
{
    if(slot<0 || slot>=REF_RAGDOLL_RETAIN_COUNT || !CL_RagdollRetainedSync()) return NULL;
    return cl_ragdoll_records[slot].bytes?&cl_ragdoll_records[slot]:NULL;
}
static ref_ragdoll_retained_t *CL_RagdollRetained(int index,qboolean create)
{
    ref_ragdoll_retained_t *empty=NULL;
    if(!CL_RagdollRetainedSync() || index<=0 || index>=svgame.numEntities || svgame.edicts[index].free) return NULL;
    for(int i=0;i<REF_RAGDOLL_RETAIN_COUNT;++i) {
        ref_ragdoll_retained_t *r=&cl_ragdoll_records[i];
        if(r->index==index && r->serial==svgame.edicts[index].serialnumber) return r;
        if(!r->index && !empty) empty=r;
    }
    if(empty && create) {empty->generation=cl_ragdoll_generation;empty->index=index;empty->serial=svgame.edicts[index].serialnumber;empty->version=REF_RAGDOLL_RETAIN_VERSION;}
    return create?empty:NULL;
}
