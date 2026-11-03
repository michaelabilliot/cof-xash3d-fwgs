// Exercise the actual model evaluator and physics palette without a renderer.
static void anchorChecks(const std::string &path) {
    std::ifstream file(path,std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    require(bytes.size()>=sizeof(studiohdr_t),"anchor model file");
    auto *hdr=reinterpret_cast<studiohdr_t*>(bytes.data());
    require(R_StudioHeaderValid(hdr),"anchor model header");
    model_t model{};model.type=mod_studio;
    cl_entity_t ent{};ent.model=&model;
    BoneMat34 pose[128]{};
    StudioCalcBoneTransformsInternal(nullptr,hdr,0,0,pose,-1,0,0,&ent,0);
    D3DXMATRIX world;D3DXMatrixIdentity(&world);
    std::array<float,128> scales{};std::vector<ragdoll::Bone> rig;
    require(buildBones(&ent,hdr,world,pose,scales,rig),"anchor native rig");
    ragdoll::World sim;
    auto *a=sim.create(rig,{0,0,0}),*b=sim.create(rig,{0,0,0});
    require(a && b,"independent corpse anchors");
    std::array<btTransform,128> untouched,out;
    auto freeRoot=[&](int bone) {
        if(a->partForBone[bone]<0) return false;
        int ancestor=a->parents[bone];
        while(ancestor>=0 && a->partForBone[ancestor]<0) ancestor=a->parents[ancestor];
        return ancestor<0;
    };
    sim.pose(*b,untouched,false);
    const btVector3 travel(3,-2,-4);
    for(auto &part:a->parts) {
        auto t=part.body->getWorldTransform();t.getOrigin()+=travel;
        part.body->setWorldTransform(t);part.body->setInterpolationWorldTransform(t);
    }
    sim.pose(*a,out,false);
    for(size_t i=0;i<rig.size();++i) {
        if((out[i].getOrigin()-rig[i].pose.getOrigin()-travel).length()>=1e-5f)
            std::cerr<<"anchor travel bone="<<i<<" parent="<<a->parents[i]<<" physical="<<a->partForBone[i]<<" error="<<(out[i].getOrigin()-rig[i].pose.getOrigin()-travel).length()<<"\n";
        require((out[i].getOrigin()-rig[i].pose.getOrigin()-travel).length()<1e-5f,
            "nonphysical studio roots and helper bones follow corpse travel");
    }
    for(auto &part:a->parts) {
        auto t=part.body->getWorldTransform();
        t.getOrigin()+=btVector3(part.bone*.13f,part.bone*.27f,part.bone*.17f);
        t.setRotation(btQuaternion(btVector3(0,0,1),part.bone*.03f)*t.getRotation());
        part.body->setWorldTransform(t);
        auto previous=t;previous.getOrigin()-=btVector3(.1f,.2f,.3f);
        part.body->setInterpolationWorldTransform(previous);
    }
    auto check=[&]() {
        for(int i=0;i<a->count;++i) {
            require(ragdoll::valid(out[i]),"finite orthonormal repaired palette");
            const int parent=a->parents[i];
            if(parent>=0 && !freeRoot(i)) {
                const auto local=out[parent].inverse()*out[i];
                require((local.getOrigin()-a->local[i].getOrigin()).length()<1e-5f,
                    "solver separation cannot stretch rendered bone anchors");
            }
        }
    };
    for(bool interpolate:{false,true}) {
        sim.pose(*a,out,interpolate);check();
        for(const auto &part:a->parts) {
            const auto raw=part.body->getWorldTransform()*part.offset.inverse();
            require(std::abs(out[part.bone].getRotation().dot(raw.getRotation()))>.99999f,
                "anchor repair preserves simulated rotations");
            if(freeRoot(part.bone) && !interpolate)
                require((out[part.bone].getOrigin()-raw.getOrigin()).length()<1e-6f,"free root translation preserved");
        }
    }
    // Retained/rest palettes can also contain a finite stretched child.
    a->restPose=out;a->settled=true;
    for(int i=0;i<a->count;++i)if(a->parents[i]>=0 && !freeRoot(i))a->restPose[i].getOrigin()+=btVector3(50,0,0);
    sim.pose(*a,out);check();
    a->settled=false;a->frozenPose=a->restPose;a->frozen=true;
    sim.pose(*a,out);check();
    sim.pose(*b,out,false);
    require(!memcmp(out.data(),untouched.data(),rig.size()*sizeof(btTransform)),"repair cannot overwrite another corpse palette");
    std::cout<<"PASS bone anchor separation, interpolation, rest, retirement and corpse isolation: "<<path<<"\n";
}
