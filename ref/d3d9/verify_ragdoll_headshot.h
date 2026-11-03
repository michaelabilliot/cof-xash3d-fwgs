// Real Slower models and production adapter/physics/skin submissions. No game.
static btVector3 headshotMomentum(const ragdoll::Doll &doll) {
    btVector3 sum(0,0,0);for(const auto &part:doll.parts) if(part.body->getInvMass()>0)sum+=part.body->getLinearVelocity()/part.body->getInvMass();return sum;
}
static float headshotMass(const ragdoll::Doll &doll) {
    float sum=0;for(const auto &part:doll.parts)if(part.body->getInvMass()>0)sum+=1/part.body->getInvMass();return sum;
}
static void headshotChecks(const std::string &path) {
    std::ifstream file(path,std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    require(bytes.size()>sizeof(studiohdr_t),"headshot native model exists");auto *hdr=reinterpret_cast<studiohdr_t*>(bytes.data());
    require(R_StudioHeaderValid(hdr) && hdr->numbones==22 && hdr->numbodyparts==1,"Slower native skeleton");
    const auto *parts=reinterpret_cast<const mstudiobodyparts_t*>(bytes.data()+hdr->bodypartindex);require(parts[0].nummodels==6,"six actual Slower body choices");
    DX_RagdollReset();model_t model{};model.type=mod_studio;strcpy_s(model.name,"models/slower.mdl");
    cl_entity_t ent{};ent.index=1;ent.model=&model;ent.curstate.modelindex=2;
    constexpr int head=21;int deliveries=0;
    float minimumShotgun=-1,maximumShotgun=0;
    for(int seq:{0,12,15}) for(int body=0;body<6;++body) {
        ent.curstate.sequence=seq;ent.curstate.body=body;
        BoneMat34 base[128]{};StudioCalcBoneTransformsInternal(nullptr,hdr,seq,seq?80.f:0.f,base,-1,0,0,&ent,0);
        D3DXMATRIX world;D3DXMatrixTranslation(&world,0,0,80);
        std::array<float,128> scales{};std::vector<ragdoll::Bone> rig;
        require(buildBones(&ent,hdr,world,base,scales,rig),"native bodygroup hitbox build");
        require(rig[head].collision==(body<3) && (rig[head].maximum-rig[head].minimum).length2()>0,"hidden head retains original hit bounds without collision");
        const btVector3 local=(rig[head].minimum+rig[head].maximum)*.5f;const btVector3 point=rig[head].pose*local;
        for(float scale:{1.f,3.f}) for(int weapon:{1,2}) for(int pellets:{1,8}) {
            if(weapon==1 && pellets!=1)continue;
            const int total=weapon==2?8:1;
            const float requested=ragdoll::shotBudget(weapon,total,scale)*pellets/total;
            for(const auto direction:{btVector3(1,0,0),btVector3(-1,0,0),btVector3(0,1,0)}) {
                ragdoll::World sim;auto *d=sim.create(rig,{0,0,0});require(d && d->parts.size()==static_cast<size_t>(body<3?11:10),"head removal retains humanoid grouping");
                require(std::abs(headshotMass(*d)-65)<.001f && d->joints.size()+1==d->parts.size(),"mass and articulated graph remain valid");
                std::array<btTransform,128> original,after;sim.pose(*d,original,false);const auto before=headshotMomentum(*d);
                const auto hit=sim.impact(*d,point,direction,0,requested,head);const auto delta=headshotMomentum(*d)-before;
                require(hit.bone==(body<3?head:8) && hit.sourceBone==head && hit.remapped==(body>=3) && hit.sourceVisible==(body<3),"original head maps only to itself or its surviving torso ancestor");
                require(hit.rejected==ragdoll::ImpactRejection::None && std::abs(hit.impulse-requested)<1e-5f && (delta-direction*requested).length()<2e-4f,"full bounded weapon momentum reaches physical skeleton with correct direction");
                require(hit.deltaSpin<=2.5001f && (hit.momentum-delta).length()<2e-4f,"angular guard retains independently measured linear momentum");
                sim.pose(*d,after,false);require(!memcmp(original.data(),after.data(),hdr->numbones*sizeof(btTransform)),"impulse changes velocity without pose replacement");
                require(sim.impact(*d,point+btVector3(10,0,0),direction,0,requested,head).bone<0,"remote source location cannot be remapped onto the torso");
                require(sim.impact(*d,point,direction,0,requested,hdr->numbones).rejected==ragdoll::ImpactRejection::InvalidSourceBone,"invalid bone cannot choose another region");
                if(weapon==2 && pellets==8) {if(minimumShotgun<0)minimumShotgun=hit.impulse;minimumShotgun=std::min(minimumShotgun,hit.impulse);maximumShotgun=std::max(maximumShotgun,hit.impulse);}
                ++deliveries;
            }
        }
        // Geometry masks change contacts while preserving an existing corpse.
        ent.curstate.body=body%3;std::vector<ragdoll::Bone> complete;
        require(buildBones(&ent,hdr,world,base,scales,complete),"intact swap rig");ragdoll::World sim;auto *d=sim.create(complete,{0,0,0});require(d!=nullptr,"existing swap corpse");
        std::array<btTransform,128> before,after;sim.pose(*d,before);
        ent.curstate.body=body%3+3;std::array<bool,128> used{};require(selectedBones(hdr,ent.curstate.body,used),"headless mask");sim.updateVisibility(*d,used);sim.pose(*d,after);
        require(d->parts.size()==11 && d->joints.size()==10 && !memcmp(before.data(),after.data(),hdr->numbones*sizeof(btTransform)),"late head removal cannot reconstruct or snap the corpse");
        auto &hidden=*d->parts[d->partForBone[head]].body;const auto hiddenVelocity=hidden.getLinearVelocity();
        const auto p=headshotMomentum(*d);const auto hit=sim.impact(*d,point,{1,0,0},0,8,head);
        require(hit.remapped && hit.bone==8 && std::abs((headshotMomentum(*d)-p).x()-8)<2e-4f && (hidden.getLinearVelocity()-hiddenVelocity).length2()==0,"late removed head routes momentum into torso without kicking invisible head");
        require((hidden.getCollisionFlags()&btCollisionObject::CF_NO_CONTACT_RESPONSE)!=0,"removed head collision disabled");
    }
    std::cout<<"PASS headshot momentum: deliveries="<<deliveries<<" six bodies x three native poses; intact/headless bodies=11/10; shotgun_full_J="<<minimumShotgun<<".."<<maximumShotgun<<"; removed-head routing, late body swaps and torque guard\n";
    // Full event-to-adapter ownership and exactly-once delivery for every mesh.
    for(int body=0;body<6;++body) {
        DX_RagdollReset();floorSetup();model_t map{};map.type=mod_brush;map.surfaces=&surface;g_worldmodel=&map;
        client.time=0;DX_RagdollFrame();entities[1]=ent;entities[1].curstate.body=body%3;entities[1].curstate.sequence=0;
        auto &state=states[1];state={};state.serial=8000+body;state.monster=1;state.modelindex=2;state.health=100;acknowledgements[1]=0;
        BoneMat34 base[128]{},pose[128]{};StudioCalcBoneTransformsInternal(nullptr,hdr,0,0,base,-1,0,0,&entities[1],0);
        D3DXMATRIX world;D3DXMatrixTranslation(&world,0,0,80);std::array<float,128> scales{};std::vector<ragdoll::Bone> rig;
        require(buildBones(&entities[1],hdr,world,base,scales,rig),"adapter head source bounds");memcpy(pose,base,sizeof(base));require(!DX_RagdollPose(&entities[1],hdr,world,pose,0),"living ownership before lethal hit");
        state.health=10;state.native_death=1;state.server_time=.01f;state.hit_count=1;auto &event=state.hits[0];event={};event.id=2000+body;event.shot=100+body;event.lethal=1;event.weapon=2;event.bone=head+1;event.local_valid=1;event.pellets=event.total_pellets=8;event.time=.01f;event.direction[0]=1;
        for(int j=0;j<3;++j)event.local_position[j]=(rig[head].minimum[j]+rig[head].maximum[j])*.5f/ragdoll::UnitsToMeters;
        entities[1].curstate.body=body;impactScale.value=30;debug.value=1;memcpy(pose,base,sizeof(base));require(DX_RagdollPose(&entities[1],hdr,world,pose,.01f),"all native bodygroups activate with positive corpse health");debug.value=0;
        auto *d=entries[0].doll;require(d && entries[0].physicalSeen==event.id && acknowledgements[1]==event.id && std::abs(headshotMomentum(*d).x()-8)<2e-4f,"adapter delivers and acknowledges full effective-scale-three shotgun exactly once");
        const auto before=headshotMomentum(*d);memcpy(pose,base,sizeof(base));DX_RagdollPose(&entities[1],hdr,world,pose,.01f);require((headshotMomentum(*d)-before).length2()==0,"repeated draw cannot replay headshot");
        if(body<3) {
            const auto *instance=d;entities[1].curstate.body=body+3;state.hit_count=2;state.hits[1]=event;state.hits[1].id+=100;state.hits[1].shot+=100;
            memcpy(pose,base,sizeof(base));require(DX_RagdollPose(&entities[1],hdr,world,pose,.01f),"late model notification retains physics ownership");
            require(entries[0].doll==instance && physics->dolls.size()==1 && std::abs((headshotMomentum(*d)-before).x()-8)<2e-4f && acknowledgements[1]==state.hits[1].id,"late head loss applies only the new event to the existing corpse");
        }
        DX_RagdollReset();g_worldmodel=nullptr;
    }
    impactScale.value=1;
    // Paired real-pose controls separate recoil from gravity-driven collapse.
    for(int body=0;body<6;++body) for(float gravity:{0.f,386.0886f,800.f}) {
        ent.curstate.body=body;ent.curstate.sequence=0;BoneMat34 base[128]{};
        StudioCalcBoneTransformsInternal(nullptr,hdr,0,0,base,-1,0,0,&ent,0);D3DXMATRIX world;D3DXMatrixTranslation(&world,0,0,0);
        std::array<float,128> scales{};std::vector<ragdoll::Bone> rig;require(buildBones(&ent,hdr,world,base,scales,rig),"gravity control rig");
        float minimum=FLT_MAX;for(const auto &b:rig)if(b.collision)for(int corner=0;corner<8;++corner){btVector3 local;for(int j=0;j<3;++j)local[j]=corner&(1<<j)?b.maximum[j]:b.minimum[j];minimum=std::min(minimum,(b.pose*local).z());}
        D3DXMatrixTranslation(&world,0,0,(-minimum+.0127f)/ragdoll::UnitsToMeters);buildBones(&ent,hdr,world,base,scales,rig);
        ragdoll::World shot,control;std::vector<btVector3> floor;quadMesh(floor,{-10,-10,0},{10,-10,0},{10,10,0},{-10,10,0});shot.addMesh(floor,false);control.addMesh(floor,false);
        auto *a=shot.create(rig,{0,0,0}),*b=control.create(rig,{0,0,0});require(a && b,"paired gravity worlds");
        const auto point=rig[head].pose*((rig[head].minimum+rig[head].maximum)*.5f);const auto impact=shot.impact(*a,point,{1,0,0},0,6,head);require(std::abs(impact.impulse-6)<1e-5f,"default full shotgun J preserved before gravity");
        for(int i=0;i<12;++i){shot.step(1.0/120,gravity);control.step(1.0/120,gravity);}
        std::array<btTransform,128> shotPose,controlPose;shot.pose(*a,shotPose,false);control.pose(*b,controlPose,false);
        float delta=0;for(int i=0;i<hdr->numbones;++i){require(ragdoll::valid(shotPose[i]),"finite articulated shot response");delta=std::max(delta,(shotPose[i].getOrigin()-controlPose[i].getOrigin()).length());}
        require(delta>1e-5f && !a->settled,"shot changes pose while island stays awake through the first 100 ms");
        std::cout<<"HEADSHOT control body="<<body<<" gravity="<<gravity<<" requested=6 delivered="<<impact.impulse<<" owner="<<impact.bone<<" peak_bone_difference_m="<<delta<<" spin="<<a->maximumAngularSpeed<<" corrections="<<a->velocityCorrections<<"\n";
        if(gravity==800) {
            BoneMat34 unshot[128]{},reacted[128]{};writePalette(world,scales,controlPose.data(),hdr->numbones,unshot);writePalette(world,scales,shotPose.data(),hdr->numbones,reacted);
            renderChecks(hdr,unshot,reacted,ent);
            for(int i=12;i<3600 && !a->settled;++i)shot.step(1.0/120,gravity);
            require(a->settled,"all six shot Slower variants settle without changing solver or damping");
            std::cout<<"HEADSHOT settled body="<<body<<" time="<<a->age<<" joint_error="<<a->restJointError<<" angular_violation="<<angularViolation(*a)<<"\n";
            std::array<btTransform,128> finalPose;shot.pose(*a,finalPose);for(int i=0;i<240;++i)shot.step(1.0/120,gravity);shot.pose(*a,shotPose);
            require(!memcmp(finalPose.data(),shotPose.data(),hdr->numbones*sizeof(btTransform)),"shot variants retain exact settled palette");
        }
    }
    std::cout<<"PASS Slower shotgun regression: six native bodygroups, momentum/direction, event ownership, hidden-part fallback, gravity controls, actual skin submission and settling\n";
}
