static void motionHandoffChecks(const std::string &path) {
    std::ifstream f(path,std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(f)),{});
    auto *hdr=reinterpret_cast<studiohdr_t*>(bytes.data());require(R_StudioHeaderValid(hdr),"motion native model");
    model_t model{};model.type=mod_studio;strcpy_s(model.name,"models/slower.mdl");cl_entity_t ent{};ent.index=1;ent.model=&model;ent.curstate.modelindex=2;
    float maximumSettle=0;
    for(int body=0;body<6;++body)for(int yaw:{0,90,180,270})for(int source:{8,14,21}) {
        ent.curstate.body=body;BoneMat34 base[128]{};StudioCalcBoneTransformsInternal(nullptr,hdr,0,80,base,-1,0,0,&ent,0);
        D3DXMATRIX world;D3DXMatrixRotationZ(&world,yaw*D3DX_PI/180.f);
        std::array<float,128> scales{};std::vector<ragdoll::Bone> rig;require(buildBones(&ent,hdr,world,base,scales,rig),"handoff rig");
        float bottom=FLT_MAX;for(const auto &b:rig)if(b.collision)for(int c=0;c<8;++c){btVector3 p;for(int j=0;j<3;++j)p[j]=c&(1<<j)?b.maximum[j]:b.minimum[j];bottom=std::min(bottom,(b.pose*p).z());}
        world._43=(-bottom+.001f)/ragdoll::UnitsToMeters;buildBones(&ent,hdr,world,base,scales,rig);
        std::array<btTransform,128> samples[3];float works[3]{};
        for(int rate:{30,60,120}) {
            ragdoll::World sim,control,passive;std::vector<btVector3> floor;quadMesh(floor,{-10,-10,0},{10,-10,0},{10,10,0},{-10,10,0});sim.addMesh(floor,false);control.addMesh(floor,false);passive.addMesh(floor,false);
            auto *d=sim.create(rig,{0,0,0}),*unshot=control.create(rig,{0,0,0}),*limp=passive.create(rig,{0,0,0});require(d&&unshot,"native paired handoff");
            const auto *ref=motionReference(&ent,hdr,*d);require(ref && ref->sequence==3 && ref->peak>.1f && ref->peak<.25f,"initial native recoil peak measured");
            require(sim.beginTransition(*d,100,ref->sequence,ref->peak,ref->angles),"supported native impact transition starts");
            const auto point=rig[source].pose*((rig[source].minimum+rig[source].maximum)*.5f);const btVector3 direction(1,0,0);
            const auto p=headshotMomentum(*d);const auto hit=sim.impact(*d,point,direction,0,8,source);sim.transitionHit(*d,hit,point,direction,1);passive.impact(*limp,point,direction,0,8,source);
            require((headshotMomentum(*d)-p-direction*8).length()<2e-4f,"muscle target setup adds no projectile momentum");
            std::array<btTransform,128> initial;sim.pose(*d,initial,false);
            for(int tick=0;tick<rate/10;++tick){sim.step(1.0/rate,800);control.step(1.0/rate,800);passive.step(1.0/rate,800);}
            const int slot=rate==30?0:rate==60?1:2;sim.pose(*d,samples[slot],false);works[slot]=d->transition.work;
            std::array<btTransform,128> absent,passivePose;control.pose(*unshot,absent,false);passive.pose(*limp,passivePose,false);float difference=0,activeDifference=0;
            for(int i=0;i<hdr->numbones;++i){require(ragdoll::valid(samples[slot][i]),"finite handoff pose");difference=std::max(difference,(samples[slot][i].getOrigin()-absent[i].getOrigin()).length());activeDifference=std::max(activeDifference,(samples[slot][i].getOrigin()-passivePose[i].getOrigin()).length());}
            require(activeDifference>.002f,"supported transition changes the passive-only result");
            require(difference>.005f,"lethal shot changes the initial visible skeleton beyond submillimetre noise");
            require(d->transition.work<=d->transition.budget+.001f && d->transition.work>=0,"active muscle work stays within native reference envelope");
            if(yaw==0)std::cout<<"HANDOFF body="<<body<<" source="<<source<<" peak="<<ref->peak<<" active="<<d->transition.active<<" rate="<<rate<<" at="<<d->transition.elapsed<<" difference="<<difference<<" vs_passive="<<activeDifference<<" work="<<d->transition.work<<" budget="<<d->transition.budget<<" spin="<<d->maximumAngularSpeed<<" corrections="<<d->velocityCorrections<<"\n";
            if(rate==120 && yaw==0 && body==0 && source==8){BoneMat34 rendered[128]{},baseline[128]{};writePalette(world,scales,samples[slot].data(),hdr->numbones,rendered);writePalette(world,scales,absent.data(),hdr->numbones,baseline);renderChecks(hdr,baseline,rendered,ent);}
            for(int tick=rate/10;tick<rate;++tick)sim.step(1.0/rate,800);
            require(!d->transition.active && d->transition.elapsed<=2*ref->peak+.0001f,"supported impact phase releases without teleport or perpetual motors");
            std::array<int,12> blocked{};
            for(int tick=0;rate==120 && tick<7200 && !d->settled;++tick){sim.step(1.0/120,800);if(d->age>10)for(int bit=0;bit<12;++bit)if(d->restBlockers&(1u<<bit))++blocked[bit];}
            if(rate==120 && !d->settled){std::cout<<"BLOCK_HIST";for(int bit=0;bit<12;++bit)std::cout<<" "<<(1u<<bit)<<":"<<blocked[bit];std::cout<<"\n";}
            if(rate==120 && !d->settled)std::cout<<"SETTLE_FAILURE body="<<body<<" source="<<source<<" yaw="<<yaw<<" rate="<<rate<<" age="<<d->age<<" spin="<<d->currentSpin<<" speed="<<d->currentSpeed<<" energy="<<d->restEnergy<<" joint_error="<<d->restJointError<<" quiet="<<d->quietTime<<" stable="<<d->stationaryTime<<" blockers="<<d->restBlockers<<" corrections="<<d->velocityCorrections<<"\n";
            if(rate==120 && !d->settled)for(const auto &part:d->parts)std::cout<<"PART bone="<<part.bone<<" v="<<part.body->getLinearVelocity().length()<<" w="<<part.body->getAngularVelocity().length()<<" r="<<(part.maximum-part.minimum).length()<<" active="<<part.body->getActivationState()<<"\n";
            require(rate!=120 || d->settled,"transition corpse settles with normal joints and solver");if(rate==120)maximumSettle=std::max(maximumSettle,d->age);
        }
        for(int k=0;k<2;++k)for(int i=0;i<hdr->numbones;++i)require((samples[k][i].getOrigin()-samples[2][i].getOrigin()).length()<.002f,"30/60/120 Hz handoff trajectories agree");
        if(body==0 && yaw==0 && source==8) {
            ragdoll::World unsupported;auto *air=unsupported.create(rig,{0,0,0});const auto *ref=motionReference(&ent,hdr,*air);
            require(!unsupported.beginTransition(*air,200,ref->sequence,ref->peak,ref->angles),"unsupported airborne death cannot acquire a stance controller");
            ragdoll::World support;std::vector<btVector3> floor;quadMesh(floor,{-10,-10,0},{10,-10,0},{10,10,0},{-10,10,0});auto *mesh=support.addMesh(floor,false);auto *d=support.create(rig,{0,0,0});require(support.beginTransition(*d,201,ref->sequence,ref->peak,ref->angles),"support control starts");
            mesh->body->setCollisionFlags(mesh->body->getCollisionFlags()|btCollisionObject::CF_NO_CONTACT_RESPONSE);support.step(1.0/120,800);support.step(1.0/120,800);require(!d->transition.active,"support removal releases controller without an anchor");
        }
    }
    std::cout<<"HANDOFF maximum_settle_seconds="<<maximumSettle<<"\n";
    std::cout<<"PASS native recoil handoff: all bodies, head/arm/torso, rotated worlds, 30/60/120 Hz, bounded muscle work, skin submission and settling\n";
}
static void referenceModelChecks(const std::string &path) {
    std::ifstream f(path,std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(f)),{});auto *hdr=reinterpret_cast<studiohdr_t*>(bytes.data());require(R_StudioHeaderValid(hdr),"reference native model");
    model_t model{};model.type=mod_studio;strcpy_s(model.name,"models/reference-fixture.mdl");cl_entity_t ent{};ent.model=&model;std::vector<int> sequences{0};
    const auto *seq=reinterpret_cast<const mstudioseqdesc_t*>(bytes.data()+hdr->seqindex);for(int i=0;i<hdr->numseq && sequences.size()<3;++i)if(!strncmp(seq[i].label,"walk",4)||!strncmp(seq[i].label,"run",3))sequences.push_back(i);
    motionReferences.clear();
    for(int sequence:sequences) {
        ent.curstate.sequence=sequence;BoneMat34 pose[128]{};StudioCalcBoneTransformsInternal(nullptr,hdr,sequence,80,pose,-1,0,0,&ent,0);D3DXMATRIX world;D3DXMatrixIdentity(&world);
        std::array<float,128> scales{};std::vector<ragdoll::Bone> rig;require(buildBones(&ent,hdr,world,pose,scales,rig),"native reference animated rig");float bottom=FLT_MAX;
        for(const auto &b:rig)if(b.collision)for(int c=0;c<8;++c){btVector3 p;for(int j=0;j<3;++j)p[j]=c&(1<<j)?b.maximum[j]:b.minimum[j];bottom=std::min(bottom,(b.pose*p).z());}
        world._43=(-bottom+.001f)/ragdoll::UnitsToMeters;buildBones(&ent,hdr,world,pose,scales,rig);ragdoll::World sim;std::vector<btVector3> floor;quadMesh(floor,{-10,-10,0},{10,-10,0},{10,10,0},{-10,10,0});sim.addMesh(floor,false);auto *d=sim.create(rig,{0,0,0});require(d,"reference physical graph");
        const auto *ref=motionReference(&ent,hdr,*d);if(!ref){std::cout<<"REFERENCE unsupported model="<<path<<" sequence="<<sequence<<" passive fallback retained\n";continue;}
        bool active=sim.beginTransition(*d,1,ref->sequence,ref->peak,ref->angles);int alignment=0;
        // Studio boxes can exceed their rounded physics shapes. Align this
        // supported-pose fixture by contact, retaining all relative bones.
        while(!active && !d->supported && alignment<8){sim.remove(d);for(auto &b:rig)b.pose.getOrigin().setZ(b.pose.getOrigin().z()-.01f);d=sim.create(rig,{0,0,0});require(d,"contact-aligned native reference");active=sim.beginTransition(*d,1,ref->sequence,ref->peak,ref->angles);++alignment;}if(!active)std::cout<<"REFERENCE_INACTIVE model="<<path<<" animation="<<sequence<<" ref="<<ref->sequence<<" peak="<<ref->peak<<" budget="<<d->transition.budget<<" supported="<<d->supported<<" penetration="<<d->initialPenetration<<"\n";require(active,"native locomotion reference is supported");int source=-1;for(int i=0;i<d->count;++i)if(rig[i].collision && rig[i].region==ragdoll::Region::Head){source=i;break;}require(source>=0,"reference head region");
        const auto point=rig[source].pose*((rig[source].minimum+rig[source].maximum)*.5f);const auto hit=sim.impact(*d,point,{1,0,0},0,6,source);sim.transitionHit(*d,hit,point,{1,0,0},.75f);
        for(int i=0;i<120;++i)sim.step(1.0/120,800);std::array<btTransform,128> output;sim.pose(*d,output);for(int i=0;i<d->count;++i)require(ragdoll::valid(output[i]),"finite reference handoff");require(!d->transition.active && d->transition.work<=d->transition.budget+.001f,"reference release and energy bound");
        std::cout<<"REFERENCE model="<<path<<" animation="<<sequence<<" alignment_mm="<<alignment*10<<" reference="<<ref->sequence<<" peak="<<ref->peak<<" work="<<d->transition.work<<" budget="<<d->transition.budget<<"\n";
    }
    motionReferences.clear();
}
static std::vector<byte> capturedFixture;
static void adapterHandoffChecks(const std::string &path) {
    std::ifstream f(path,std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(f)),{});auto *hdr=reinterpret_cast<studiohdr_t*>(bytes.data());require(R_StudioHeaderValid(hdr),"adapter handoff native model");
    fs_api_t files{};files.WriteFile=[](const char*,const void *data,fs_offset_t size)->qboolean{capturedFixture.assign(static_cast<const byte*>(data),static_cast<const byte*>(data)+size);return true;};
    gEngfuncs.fsapi=&files;fixtureHeader=hdr;retentionEnabled=true;gEngfuncs.Mod_Extradata=[](int,model_t*)->void*{return fixtureHeader;};
    for(int body:{0,3})for(bool neck:{false,true}) {
        DX_RagdollReset();floorSetup();model_t map{};map.type=mod_brush;map.surfaces=&surface;g_worldmodel=&map;client.time=0;DX_RagdollFrame();
        model_t model{};model.type=mod_studio;strcpy_s(model.name,"models/slower.mdl");auto &ent=entities[1];ent={};ent.index=1;ent.model=&model;ent.curstate.modelindex=2;ent.curstate.body=body;
        auto &state=states[1];state={};state.generation=1;state.serial=9500+body;state.modelindex=2;state.monster=1;state.health=100;state.grounded=1;acknowledgements[1]=0;
        BoneMat34 base[128]{},pose[128]{};StudioCalcBoneTransformsInternal(nullptr,hdr,0,80,base,-1,0,0,&ent,0);
        D3DXMATRIX world;D3DXMatrixRotationZ(&world,D3DX_PI/2);std::array<float,128> scales{};std::vector<ragdoll::Bone> rig;buildBones(&ent,hdr,world,base,scales,rig);
        float bottom=FLT_MAX;for(const auto &b:rig)if(b.collision)for(int corner=0;corner<8;++corner){btVector3 p;for(int j=0;j<3;++j)p[j]=corner&(1<<j)?b.maximum[j]:b.minimum[j];bottom=std::min(bottom,(b.pose*p).z());}
        world._43=(-bottom+.001f)/ragdoll::UnitsToMeters;buildBones(&ent,hdr,world,base,scales,rig);
        memcpy(pose,base,sizeof(pose));DX_RagdollPose(&ent,hdr,world,pose,0);client.time=.02;DX_RagdollFrame();memcpy(pose,base,sizeof(pose));DX_RagdollPose(&ent,hdr,world,pose,.02f);
        require(ownershipClaims[1].owner==REF_RAGDOLL_UNOWNED,"living actor never releases movement collision");
        state.native_death=1;state.health=10;state.server_time=.03f;state.hit_count=3;
        for(int i=0;i<3;++i){auto &hit=state.hits[i];hit={};hit.id=3000+i;hit.shot=400;hit.lethal=1;hit.weapon=2;hit.bone=(i==0?8:i==1?10:9)+1;hit.pellets=i==0?6:1;if(neck){hit.bone=(i==0?12:21)+1;hit.pellets=i==0?5:i==1?2:1;}hit.total_pellets=8;hit.local_valid=1;hit.direction[0]=1;hit.time=.03f;for(int j=0;j<3;++j)hit.local_position[j]=(rig[hit.bone-1].minimum[j]+rig[hit.bone-1].maximum[j])*.5f/ragdoll::UnitsToMeters;}
        client.time=.03;DX_RagdollFrame();debug.value=body==0?3.f:1.f;impactScale.value=3;memcpy(pose,base,sizeof(pose));require(DX_RagdollPose(&ent,hdr,world,pose,.03f),"lethal positive-health supported handoff");
        auto *d=entries[0].doll;require(d && d->transition.active && ownershipClaims[1].owner==REF_RAGDOLL_IMPACT,"claim published only after supported physical activation");
        if(neck && body==0) {
            bool headTarget=false;for(size_t j=0;j<d->joints.size();++j)if(d->joints[j]->getRigidBodyB().getUserIndex()==21)headTarget=d->transition.muscles[j].target.length()>.2f;
            require(headTarget,"captured 5-neck/3-head pellet split reaches physical head through full adapter");
        }
        require(acknowledgements[1]==3002 && std::abs(headshotMomentum(*d).x()-8)<2e-4f,"logged torso clusters retain shared 8 Ns budget and acknowledge once");
        const auto momentum=headshotMomentum(*d);DX_RagdollPose(&ent,hdr,world,pose,.03f);require((headshotMomentum(*d)-momentum).length2()==0,"repeated render cannot replay lethal discharge");
        studio_cache_t cache{};cache.model=&model;cache.batches.emplace_back();auto &batch=cache.batches[0];batch.vertexCount=16;batch.primitiveCount=1;batch.indexBuffer=reinterpret_cast<IDirect3DIndexBuffer9*>(1);
        const auto *parts=reinterpret_cast<const mstudiobodyparts_t*>(bytes.data()+hdr->bodypartindex);const auto *sub=reinterpret_cast<const mstudiomodel_t*>(bytes.data()+parts[0].modelindex)+body;
        const auto *v=reinterpret_cast<const vec3_t*>(bytes.data()+sub->vertindex);const byte *bone= reinterpret_cast<const byte*>(bytes.data()+sub->vertinfoindex);
        for(int i=0;i<16;++i){studio_source_vertex_t vertex{};vertex.x=v[i][0];vertex.y=v[i][1];vertex.z=v[i][2];vertex.bone=bone[i];vertex.normalBone=bone[i];batch.sources.push_back(vertex);}
        Buffer buffer(16*sizeof(studio_normal_lit_vertex_t));batch.vertexBuffer=&buffer;cache.gpuSkinSourceVertexBuffer=&buffer;cache.combinedVertexBuffer=&buffer;cache.combinedVertexBufferMode=true;cache.combinedVertexCount=16;
        recordingDevice.world=world;gpu=true;normals=false;probeUploadNormals=false;const float light[3]={0,0,-1};
        auto submit=[&](){require(R_UpdateStudioVertices(cache,&model,hdr,&ent,0,80,-1,0,0,static_cast<float>(client.time),0xffffffff,light,0,&world,false,pose,true),"real GPU handoff update");require(R_DrawStudioSkinBatches(cache,&ent),"real GPU handoff constant submission");};submit();
        for(int tick=1;tick<=61;++tick){client.time=.03+tick/60.0;DX_RagdollFrame();memcpy(pose,base,sizeof(pose));require(DX_RagdollPose(&ent,hdr,world,pose,static_cast<float>(client.time)),"physics owns every transition render");submit();
            if(body==3 && tick==6){const auto saved=retainedData(entries[0]);std::array<btTransform,128> before,after;physics->pose(*d,before);DX_RagdollReset("renderer-shutdown");require(ownershipClaims[1].owner==REF_RAGDOLL_IMPACT,"renderer reconstruction keeps movement claim");DX_RagdollFrame();memcpy(pose,base,sizeof(pose));require(DX_RagdollPose(&ent,hdr,world,pose,static_cast<float>(client.time)),"active transition reconstructed");d=entries[0].doll;physics->pose(*d,after);require(!memcmp(before.data(),after.data(),hdr->numbones*sizeof(btTransform)),"transition visual palette exact across reload");require(d->transition.elapsed==saved->transitionElapsed && d->transition.work==saved->muscleWork && d->transition.shot==400,"reload preserves active reference clock/work/event ownership");}
        }
        require(!d->transition.active && ownershipClaims[1].owner==REF_RAGDOLL_DYNAMIC,"transition releases into normal dynamic ownership");
        if(body==0){require(!capturedFixture.empty() && capturedFixture.size()<=CaptureLimit,"debug capture emitted a bounded value file");replayCapture(capturedFixture);if(const char *output=std::getenv("COF_CAPTURE_FIXTURE")){std::ofstream fixture(output,std::ios::binary);fixture.write(reinterpret_cast<const char*>(capturedFixture.data()),capturedFixture.size());require(fixture.good(),"capture fixture exported");}auto malformed=capturedFixture;malformed.pop_back();bool rejected=false;try{replayCapture(malformed);}catch(const std::exception&){rejected=true;}require(rejected,"truncated diagnostic capture rejected");}
        state.native_death=0;state.health=100;client.time+=.02;DX_RagdollFrame();require(entries.empty() && ownershipClaims[1].owner==REF_RAGDOLL_UNOWNED,"revival revokes renderer movement ownership");
        state.native_death=1;state.health=10;state.hit_count=1;state.hits[0].id+=100;state.hits[0].shot+=1;state.server_time=static_cast<float>(client.time);state.hits[0].time=state.server_time;impactScale.value=0;
        memcpy(pose,base,sizeof(pose));require(DX_RagdollPose(&ent,hdr,world,pose,static_cast<float>(client.time)),"disabled-impact corpse still activates");require(!entries[0].doll->transition.active && acknowledgements[1]==state.hits[0].id && ownershipClaims[1].owner==REF_RAGDOLL_DYNAMIC,"scale zero acknowledges intentionally disabled J without a recoil phase or stale replay");
        DX_RagdollReset();
    }
    D3DXMatrixIdentity(&recordingDevice.world);gEngfuncs.fsapi=nullptr;retentionEnabled=false;debug.value=0;impactScale.value=1;g_worldmodel=nullptr;
    std::cout<<"PASS production lethal adapter/ownership/capture: logged multi-body discharge, living/revival gates, supported phase, GPU submission, active renderer reload and offline replay\n";
}

// Reproduce the captured neck/head routing and split-pellet amplitude failure.
static void recoilAnatomyChecks(const std::string &path) {
    std::ifstream file(path,std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    auto *hdr=reinterpret_cast<studiohdr_t*>(bytes.data());require(R_StudioHeaderValid(hdr),"recoil native model");
    model_t model{};model.type=mod_studio;strcpy_s(model.name,"models/slower.mdl");cl_entity_t ent{};ent.model=&model;
    int cases=0;float minimumHeadAngle=FLT_MAX;
    for(int body=0;body<6;++body)for(int yaw:{0,90,180,270})for(int source:{12,21}) {
        ent.curstate.body=body;BoneMat34 base[128]{};StudioCalcBoneTransformsInternal(nullptr,hdr,0,80,base,-1,0,0,&ent,0);
        D3DXMATRIX world;D3DXMatrixRotationZ(&world,yaw*D3DX_PI/180);std::array<float,128> scales{};std::vector<ragdoll::Bone> rig;buildBones(&ent,hdr,world,base,scales,rig);
        float bottom=FLT_MAX;for(const auto &b:rig)if(b.collision)for(int c=0;c<8;++c){btVector3 v;for(int k=0;k<3;++k)v[k]=c&(1<<k)?b.maximum[k]:b.minimum[k];bottom=std::min(bottom,(b.pose*v).z());}
        world._43=(-bottom+.001f)/ragdoll::UnitsToMeters;buildBones(&ent,hdr,world,base,scales,rig);
        btVector3 expected(0,0,0);bool first=true;
        for(float fraction:{.125f,.375f,.625f,1.f}) {
            ragdoll::World sim;std::vector<btVector3> floor;quadMesh(floor,{-10,-10,0},{10,-10,0},{10,10,0},{-10,10,0});sim.addMesh(floor,false);
            auto *d=sim.create(rig,{0,0,0});require(d,"recoil rig");const auto *ref=motionReference(&ent,hdr,*d);require(ref && sim.beginTransition(*d,800,ref->sequence,ref->peak,ref->angles),"recoil supported reference");
            const btVector3 direction(btCos(yaw*SIMD_PI/180),btSin(yaw*SIMD_PI/180),0);
            const auto point=rig[source].pose*((rig[source].minimum+rig[source].maximum)*.5f);
            const auto hit=sim.impact(*d,point,direction,0,8*fraction,source);require(hit.bone>=0,"original anatomical event accepted");
            const auto momentum=headshotMomentum(*d);sim.transitionHit(*d,hit,point,direction,fraction);
            require((headshotMomentum(*d)-momentum).length2()==0,"reflex adds no projectile momentum");
            const int receptor=body<3?21:8;int index=-1;
            for(size_t j=0;j<d->joints.size();++j)if(d->joints[j]->getRigidBodyB().getUserIndex()==receptor)index=static_cast<int>(j);
            require(index>=0,"surviving anatomical reflex joint");const auto &muscle=d->transition.muscles[index];
            require(muscle.target.length()>.02f,"neck/head hit excites surviving anatomical receptor");
            if(first){expected=muscle.target;first=false;}else require((muscle.target-expected).length()<1e-5f,"regional reflex amplitude independent of pellet share");
            const auto &joint=*d->joints[index];btTransform fa,fb;ragdoll::jointFrames(joint,fa,fb);
            const auto lever=joint.getRigidBodyB().getCenterOfMassPosition()-joint.getRigidBodyA().getWorldTransform()*fa.getOrigin();
            const auto axis=joint.getRigidBodyA().getWorldTransform().getBasis()*muscle.target;
            require(axis.cross(lever).dot(direction)>0,"reflex COM displacement follows ray from either side");
            const auto &cone=static_cast<const btConeTwistConstraint&>(joint);const auto local=fa.getBasis().transpose()*muscle.target;
            const float extent=btSqrt(btPow(local.x()/cone.getTwistSpan(),2)+btPow(local.y()/cone.getSwingSpan2(),2)+btPow(local.z()/cone.getSwingSpan1(),2));
            require(extent<=.80001f,"target respects axis-specific anatomical cone envelope");
            if(body<3 && source==12 && fraction==1) {
                const auto rest=muscle.rest;const auto target=muscle.target;
                for(int tick=0;tick<12;++tick)sim.step(1.0/120,800);
                const auto qa=joint.getRigidBodyA().getWorldTransform().getRotation(),qb=joint.getRigidBodyB().getWorldTransform().getRotation();auto q=qa.inverse()*qb*rest.inverse();if(q.w()<0)q=btQuaternion(-q.x(),-q.y(),-q.z(),-q.w());
                const float angle=(q.getAxis()*q.getAngleShortestPath()).dot(target.normalized());minimumHeadAngle=std::min(minimumHeadAngle,angle);
                require(angle>.04f,"physical head tracks neck reflex within first 100 ms");
            }
            ++cases;
        }
    }
    motionReferences.clear();std::cout<<"RECOIL anatomical_cases="<<cases<<" minimum_head_tracking_radians="<<minimumHeadAngle<<" momentum_unchanged=1\n";
}
