// Native animation supplies a bounded motion/time reference, never the final
// rendered death pose. Ray direction supplies the anatomical recoil axis.
struct MotionReference {
    std::string model;
    int length=0,body=0,sequence=-1;
    float peak=0;
    std::array<float,ragdoll::MaxBones> angles{};
};
std::vector<MotionReference> motionReferences;
const MotionReference *motionReference(const cl_entity_t *ent,studiohdr_t *hdr,const ragdoll::Doll &doll) {
    for(const auto &r:motionReferences)if(r.model==ent->model->name && r.length==hdr->length && r.body==ent->curstate.body)return r.sequence<0?nullptr:&r;
    if(motionReferences.size()>=64)return nullptr;
    MotionReference ref;ref.model=ent->model->name;ref.length=hdr->length;ref.body=ent->curstate.body;
    if(!R_StudioRangeValid(hdr,hdr->seqindex,static_cast<size_t>(hdr->numseq)*sizeof(mstudioseqdesc_t)))return nullptr;
    const auto *seqs=reinterpret_cast<const mstudioseqdesc_t*>(reinterpret_cast<const byte*>(hdr)+hdr->seqindex);
    int selected=-1;
    for(int i=0;i<hdr->numseq;++i)if(!strncmp(seqs[i].label,"flinchsmall",11)){selected=i;break;}
    if(selected<0)for(int i=0;i<hdr->numseq;++i)if(!strncmp(seqs[i].label,"flinch",6)){selected=i;break;}
    // Faster has no flinch sequence; its native simple-death activity (36)
    // supplies the initial recoil reference without replaying the death.
    if(selected<0)for(int i=0;i<hdr->numseq;++i)if(seqs[i].activity==36){selected=i;break;}
    if(selected>=0 && std::isfinite(seqs[selected].fps) && seqs[selected].fps>=10 && seqs[selected].fps<=120 && seqs[selected].numframes>2) {
        const auto &seq=seqs[selected];BoneMat34 first[ragdoll::MaxBones]{},sample[ragdoll::MaxBones]{};
        D3DXMATRIX identity;D3DXMatrixIdentity(&identity);std::array<btQuaternion,ragdoll::MaxBones> relative{};std::array<int,ragdoll::MaxBones> parent{};
        int anchor=-1;
        if(R_StudioSampleRagdollReference(ent->model,hdr,selected,0,first)) {
            for(int i=0;i<doll.count;++i) {
                parent[i]=doll.parents[i];while(parent[i]>=0 && doll.partForBone[parent[i]]<0)parent[i]=doll.parents[parent[i]];
                float scale;auto pose=boneWorld(first[i],identity,scale);
                if(parent[i]>=0)pose=boneWorld(first[parent[i]],identity,scale).inverse()*pose;
                relative[i]=pose.getRotation();if(doll.collisionBones[i].region==ragdoll::Region::Head)anchor=i;
            }
            if(anchor<0)for(const auto &p:doll.parts)if(p.region==ragdoll::Region::Torso){anchor=p.bone;break;}
            float previous=0;
            const int samples=std::min(seq.numframes-1,static_cast<int>(seq.fps*.3f));
            for(int k=1;anchor>=0 && k<=samples;++k) {
                if(!R_StudioSampleRagdollReference(ent->model,hdr,selected,256.f*k/(seq.numframes-1),sample))break;
                std::array<float,ragdoll::MaxBones> angles{};
                for(int i=0;i<doll.count;++i) {
                    float scale;auto pose=boneWorld(sample[i],identity,scale);if(parent[i]>=0)pose=boneWorld(sample[parent[i]],identity,scale).inverse()*pose;
                    angles[i]=(pose.getRotation()*relative[i].inverse()).getAngleShortestPath();
                }
                if(previous>.01f && angles[anchor]<previous-1e-4f)break;
                ref.angles=angles;ref.peak=k/seq.fps;previous=angles[anchor];ref.sequence=selected;
            }
        }
    }
    motionReferences.push_back(std::move(ref));return motionReferences.back().sequence<0?nullptr:&motionReferences.back();
}
void publishOwnership(const Entry &e,int owner) {
    if(!gEngfuncs.EngineGetParm || gEngfuncs.EngineGetParm(PARM_RAGDOLL_CLAIM,-2)!=REF_RAGDOLL_CLAIM_VERSION)return;
    auto *claim=reinterpret_cast<ref_ragdoll_claim_t*>(gEngfuncs.EngineGetParm(PARM_RAGDOLL_CLAIM,e.index));
    if(claim && claim->version==REF_RAGDOLL_CLAIM_VERSION && claim->serial==e.serial && claim->modelindex==e.modelindex)claim->owner=owner;
}
void publishOwnership(const Entry &e) {if(e.doll)publishOwnership(e,e.doll->settled||e.doll->frozen?REF_RAGDOLL_RESTING:e.doll->transition.active?REF_RAGDOLL_IMPACT:REF_RAGDOLL_DYNAMIC);}
