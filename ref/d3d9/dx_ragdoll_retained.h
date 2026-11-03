// No native/Bullet/D3D pointers cross the engine-owned retention boundary.
struct RetainedTransform {float basis[9],origin[3];};
struct RetainedBone {
    int parent,group,region,collision;
    RetainedTransform pose;
    float minimum[3],maximum[3];
};
struct RetainedPart {
    int bone,flags;
    RetainedTransform solved,previous;
    float linear[3],angular[3];
};
struct RetainedDoll {
    unsigned version=2;
    int count=0,parts=0,body=0,modelLength=0,deathSource=0;
    unsigned impactSeen=0,physicalSeen=0,settleCount=0;
    int frozen=0,settled=0,supported=0;
    int transitioning=0,referenceSequence=-1;
    unsigned transitionShot=0;
    float peakTime=0,transitionElapsed=0,muscleWork=0,muscleBudget=0;
    float muscleRest[ragdoll::MaxBodies][4]{},muscleTarget[ragdoll::MaxBodies][3]{},muscleAngle[ragdoll::MaxBodies]{};
    double born=0,phase=0;
    float age=0,sinceImpact=0;
    char model[128]{};
    char names[ragdoll::MaxBones][32]{};
    float scales[ragdoll::MaxBones]{};
    RetainedBone rig[ragdoll::MaxBones]{};
    RetainedPart bodies[ragdoll::MaxBodies]{};
    RetainedTransform visual[ragdoll::MaxBones]{};
    unsigned char visible[ragdoll::MaxBones]{};
};
static_assert(sizeof(RetainedDoll)<=REF_RAGDOLL_RETAIN_BYTES,"engine retention payload bound");
RetainedTransform pack(const btTransform &t) {
    RetainedTransform out{};
    for(int i=0;i<3;++i) {out.origin[i]=t.getOrigin()[i];for(int j=0;j<3;++j) out.basis[i*3+j]=t.getBasis()[i][j];}
    return out;
}
btTransform unpack(const RetainedTransform &t) {
    return btTransform(btMatrix3x3(t.basis[0],t.basis[1],t.basis[2],t.basis[3],t.basis[4],t.basis[5],t.basis[6],t.basis[7],t.basis[8]),btVector3(t.origin[0],t.origin[1],t.origin[2]));
}
ref_ragdoll_retained_t *retained(int index,bool create=false) {
    if(!gEngfuncs.EngineGetParm || gEngfuncs.EngineGetParm(PARM_GET_RAGDOLL_RETAINED,-2)!=REF_RAGDOLL_RETAIN_VERSION) return nullptr;
    return reinterpret_cast<ref_ragdoll_retained_t*>(gEngfuncs.EngineGetParm(PARM_GET_RAGDOLL_RETAINED,create?index:-index-3));
}
void invalidateRetained(int index) {if(auto *r=retained(index)) {r->bytes=0;r->index=0;}}
std::unique_ptr<RetainedDoll> retainedData(const Entry &e) {
    if(!e.doll || e.boneCount<=0 || e.doll->parts.size()>ragdoll::MaxBodies)return nullptr;
    auto data=std::make_unique<RetainedDoll>();const auto &d=*e.doll;
    data->count=d.count;data->parts=static_cast<int>(d.parts.size());data->body=e.body;
    data->deathSource=e.deathSource;data->impactSeen=e.impactSeen;data->physicalSeen=e.physicalSeen;data->settleCount=d.settleCount;
    data->supported=d.supported;const auto &t=d.transition;
    data->transitioning=t.active;data->transitionShot=t.shot;data->referenceSequence=t.sequence;
    data->peakTime=t.peakTime;data->transitionElapsed=t.elapsed;data->muscleWork=t.work;data->muscleBudget=t.budget;
    for(size_t i=0;i<d.joints.size();++i) {const auto &m=t.muscles[i];for(int j=0;j<4;++j)data->muscleRest[i][j]=m.rest[j];for(int j=0;j<3;++j)data->muscleTarget[i][j]=m.target[j];data->muscleAngle[i]=m.referenceAngle;}
    data->frozen=d.frozen;data->settled=d.settled;data->born=e.born;data->phase=physics->phase();data->age=d.age;data->sinceImpact=d.sinceImpact;
    // Engine R_Shutdown may already have called Mod_FreeAll. Export must
    // reference only owned values; model_t and studio headers may be gone.
    strncpy_s(data->model,e.modelName,_TRUNCATE);data->modelLength=e.modelLength;
    std::array<btTransform,ragdoll::MaxBones> visual;physics->pose(d,visual);
    for(int i=0;i<d.count;++i) {
        const auto &b=d.collisionBones[i];auto &r=data->rig[i];
        r.parent=b.parent;r.group=b.group;r.region=static_cast<int>(b.region);r.collision=b.collision;r.pose=pack(b.pose);
        for(int j=0;j<3;++j) {r.minimum[j]=b.minimum[j];r.maximum[j]=b.maximum[j];}
        data->visual[i]=pack(visual[i]);data->scales[i]=e.scales[i];data->visible[i]=d.visibleBones[i];memcpy(data->names[i],e.boneNames[i].data(),32);
    }
    for(size_t i=0;i<d.parts.size();++i) {
        const auto &p=d.parts[i];auto &r=data->bodies[i];r.bone=p.bone;r.flags=p.body->getCollisionFlags();
        r.solved=pack(d.settled&&!d.frozen?d.restBodyPose[p.bone]:p.body->getWorldTransform());
        r.previous=pack(p.body->getInterpolationWorldTransform());
        for(int j=0;j<3;++j) {r.linear[j]=p.body->getLinearVelocity()[j];r.angular[j]=p.body->getAngularVelocity()[j];}
    }
    return data;
}
void saveEntry(const Entry &e) {
    auto data=retainedData(e);if(!data)return;
    auto *mail=retained(e.index,true);if(!mail || mail->serial!=e.serial)return;
    mail->bytes=0;memcpy(mail->data,data.get(),sizeof(*data));mail->bytes=sizeof(*data);
}
bool restoreEntry(Entry &e,const cl_entity_t *ent,const studiohdr_t *hdr) {
    auto *mail=retained(e.index,true);
    if(!mail || !mail->bytes) return false;
    auto data=std::make_unique<RetainedDoll>();
    if(mail->bytes!=sizeof(*data) || mail->serial!=e.serial) {mail->bytes=0;return false;}
    memcpy(data.get(),mail->data,sizeof(*data));
    const auto *bones=reinterpret_cast<const mstudiobone_t*>(reinterpret_cast<const byte*>(hdr)+hdr->boneindex);
    bool valid=data->version==2 && data->count==hdr->numbones && data->parts>0 && data->parts<=ragdoll::MaxBodies &&
        data->body==ent->curstate.body && !strncmp(data->model,ent->model->name,sizeof(data->model)) &&
        (!data->modelLength || data->modelLength==hdr->length) && std::isfinite(data->born) && std::isfinite(data->phase) && data->phase>=0 && data->phase<1.0/120 &&
        std::isfinite(data->age) && std::isfinite(data->sinceImpact);
    if(data->transitioning)valid=valid && data->transitionShot && data->referenceSequence>=0 && std::isfinite(data->peakTime) && data->peakTime>=1.f/120 && data->peakTime<=.3f &&
        std::isfinite(data->transitionElapsed) && data->transitionElapsed>=0 && data->transitionElapsed<=2*data->peakTime && std::isfinite(data->muscleBudget) && data->muscleBudget>0 &&
        std::isfinite(data->muscleWork) && data->muscleWork>=0 && data->muscleWork<=data->muscleBudget+.001f;
    for(int i=0;valid && data->transitioning && i<data->parts-1;++i) {
        btQuaternion q(data->muscleRest[i][0],data->muscleRest[i][1],data->muscleRest[i][2],data->muscleRest[i][3]);
        const btVector3 target(data->muscleTarget[i][0],data->muscleTarget[i][1],data->muscleTarget[i][2]);
        valid=std::isfinite(q.length2()) && btFabs(q.length2()-1)<.01f && ragdoll::finite(target) && std::isfinite(data->muscleAngle[i]) && data->muscleAngle[i]>=0 && data->muscleAngle[i]<3.2f;
    }
    std::vector<ragdoll::Bone> rig(static_cast<size_t>(hdr->numbones));
    for(int i=0;valid && i<data->count;++i) {
        const auto &r=data->rig[i];auto &b=rig[i];
        valid=r.parent==bones[i].parent && !memcmp(data->names[i],bones[i].name,32) && data->scales[i]>0 && std::isfinite(data->scales[i]);
        b.parent=r.parent;b.group=r.group;b.region=static_cast<ragdoll::Region>(r.region);b.collision=r.collision!=0;b.pose=unpack(r.pose);
        b.minimum=btVector3(r.minimum[0],r.minimum[1],r.minimum[2]);b.maximum=btVector3(r.maximum[0],r.maximum[1],r.maximum[2]);
        valid=valid && ragdoll::valid(unpack(data->visual[i]));
    }
    for(int i=0;valid && i<data->parts;++i) {
        const auto &p=data->bodies[i];valid=p.bone>=0 && p.bone<data->count && ragdoll::valid(unpack(p.solved)) && ragdoll::valid(unpack(p.previous));
        for(int j=0;j<3;++j) valid=valid && std::isfinite(p.linear[j]) && std::isfinite(p.angular[j]);
    }
    if(!valid) {mail->bytes=0;return false;}
    auto *d=physics->create(rig,btVector3(0,0,0));
    if(!d || d->parts.size()!=static_cast<size_t>(data->parts)) {if(d) physics->remove(d);mail->bytes=0;return false;}
    for(int i=0;i<data->parts;++i) if(d->parts[i].bone!=data->bodies[i].bone) {physics->remove(d);mail->bytes=0;return false;}
    physics->restorePhase(data->phase);
    for(int i=0;i<data->parts;++i) {
        auto &p=d->parts[i];const auto &r=data->bodies[i];
        p.body->setCenterOfMassTransform(unpack(r.solved));p.body->setInterpolationWorldTransform(unpack(r.previous));
        p.body->setLinearVelocity(btVector3(r.linear[0],r.linear[1],r.linear[2]));p.body->setAngularVelocity(btVector3(r.angular[0],r.angular[1],r.angular[2]));
        d->restBodyPose[p.bone]=unpack(r.solved);
    }
    for(int i=0;i<data->count;++i) {d->visibleBones[i]=data->visible[i]!=0;d->restPose[i]=unpack(data->visual[i]);}
    physics->updateVisibility(*d,d->visibleBones);physics->refresh(*d);
    d->supported=data->supported!=0;auto &t=d->transition;t.active=data->transitioning!=0;t.shot=data->transitionShot;t.sequence=data->referenceSequence;
    t.peakTime=data->peakTime;t.elapsed=data->transitionElapsed;t.work=data->muscleWork;t.budget=data->muscleBudget;
    for(size_t i=0;i<d->joints.size();++i){auto &m=t.muscles[i];m.rest=btQuaternion(data->muscleRest[i][0],data->muscleRest[i][1],data->muscleRest[i][2],data->muscleRest[i][3]);m.target=btVector3(data->muscleTarget[i][0],data->muscleTarget[i][1],data->muscleTarget[i][2]);m.referenceAngle=data->muscleAngle[i];}
    d->settled=data->settled!=0;d->age=data->age;d->sinceImpact=data->sinceImpact;d->settleCount=data->settleCount;
    if(d->settled) for(auto &p:d->parts) {p.body->setLinearVelocity(btVector3(0,0,0));p.body->setAngularVelocity(btVector3(0,0,0));p.body->forceActivationState(ISLAND_SLEEPING);}
    if(data->frozen) {d->settled=true;physics->freeze(*d);for(int i=0;i<data->count;++i)d->frozenPose[i]=unpack(data->visual[i]);}
    strncpy_s(e.modelName,data->model,_TRUNCATE);e.modelLength=hdr->length;
    e.doll=d;e.born=data->born;e.deathSource=data->deathSource;e.impactSeen=data->impactSeen;e.physicalSeen=data->physicalSeen;e.boneCount=data->count;e.activation=Entry::Activation::Active;
    for(int i=0;i<data->count;++i) {e.scales[i]=data->scales[i];e.boneParents[i]=bones[i].parent;memcpy(e.boneNames[i].data(),bones[i].name,32);}
    mail->bytes=0;publishOwnership(e);
    if(debug.value!=0) gEngfuncs.Con_Printf("RAGDOLL restored: entity=%d serial=%d bones=%d settled=%d retired=%d event=%u\n",e.index,e.serial,e.boneCount,d->settled,d->frozen,e.physicalSeen);
    return true;
}
