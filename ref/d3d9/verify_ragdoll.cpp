// CPU only: never creates a device, loads a renderer DLL or starts the game.
// Include the adapter to exercise its actual world/bone conversion code.
#include "dx_ragdoll.cpp"
#include "dx_studio_palette.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstdarg>
#include "verify_ragdoll_animation.inc"
bool R_StudioSampleRagdollReference(model_t *model,studiohdr_t *hdr,int sequence,float frame,BoneMat34 *out) {
    if(!out || !R_StudioHeaderValid(hdr) || sequence<0 || sequence>=hdr->numseq || !std::isfinite(frame))return false;
    cl_entity_t isolated{};isolated.model=model;isolated.curstate.sequence=sequence;
    StudioCalcBoneTransformsInternal(model,hdr,sequence,frame,out,-1,0,0,&isolated,0);return true;
}


bool DX_ThreadStudioBones(int) {return false;}
void DX_ParallelFor(int count,int,const std::function<void(int,int)> &fn) {fn(0,count);}
bool R_NoInterpolationEnabled() {return false;}
bool R_FixedStringEquals(const char *field,size_t size,const char *text) {return strlen(text)<size && !memcmp(field,text,strlen(text)+1);}

extern "C" {
ref_api_t gEngfuncs{};
ref_client_t *gp_cl=nullptr;
}
model_t *g_worldmodel=nullptr;
static ref_client_t client{};
static std::array<cl_entity_t,8> entities{};
static std::array<ref_ragdoll_entity_t,8> states{};
static std::vector<btVector3> mapTriangles;
static msurface_t surface{};
static std::array<mvertex_t,4> quad{};
static void require(bool value,const char *message) {if(!value) throw std::runtime_error(message);}
static bool inputInGame=true,clientActive=true;
static int availableEntities=8;
static bool retentionEnabled=false;
static std::array<ref_ragdoll_retained_t,8> retainedRecords{};
static std::array<unsigned,8> acknowledgements{};
static std::array<ref_ragdoll_claim_t,8> ownershipClaims{};
static studiohdr_t *fixtureHeader=nullptr;
static intptr_t parm(int key,int arg) {
    if(key==PARM_RAGDOLL_CLAIM && arg==-2)return REF_RAGDOLL_CLAIM_VERSION;
    if(key==PARM_RAGDOLL_CLAIM && arg>0 && arg<8){auto &c=ownershipClaims[arg];if(c.serial!=states[arg].serial || c.modelindex!=states[arg].modelindex)c={REF_RAGDOLL_CLAIM_VERSION,1,arg,states[arg].serial,states[arg].modelindex,0};return reinterpret_cast<intptr_t>(&c);}
    if(key==PARM_RAGDOLL_ACK && arg>0 && arg<8) return reinterpret_cast<intptr_t>(&acknowledgements[arg]);
    if(key==PARM_GET_RAGDOLL_RETAINED && retentionEnabled) {
        if(arg==-2) return REF_RAGDOLL_RETAIN_VERSION;
        if(arg==-1) return 1;
        if(arg==-3) {int count=0;for(const auto &r:retainedRecords)if(r.bytes)++count;return count;}
        if(arg<=-65536) {int slot=-arg-65536;return slot<8 && retainedRecords[slot].bytes?reinterpret_cast<intptr_t>(&retainedRecords[slot]):0;}
        if(arg<=-4) arg=-arg-3;
        if(arg>0 && arg<8) {auto &r=retainedRecords[arg];if(r.serial!=states[arg].serial || !r.index) {r={};r.generation=1;r.index=arg;r.serial=states[arg].serial;r.version=REF_RAGDOLL_RETAIN_VERSION;}return reinterpret_cast<intptr_t>(&r);}
    }
    if(key==PARM_GET_RAGDOLL_ENTITY && arg==-2) return REF_RAGDOLL_SNAPSHOT_VERSION;
    if(key==PARM_GET_RAGDOLL_ENTITY && arg==-1) return availableEntities;
    if(key==PARM_GET_RAGDOLL_ENTITY && arg>0 && arg<8 && states[arg].modelindex) return reinterpret_cast<intptr_t>(&states[arg]);
    if(key==PARM_MAX_ENTITIES) return 8;
    if(key==PARM_CLIENT_INGAME) return inputInGame?1:0;
    if(key==PARM_CLIENT_ACTIVE) return clientActive?1:0;
    return 0;
}
static cl_entity_t *entity(int index) {return index>=0 && index<8?&entities[index]:nullptr;}
static void reg(convar_t *c) {c->value=static_cast<float>(atof(c->string));}
static void print(const char *format,...) {va_list args;va_start(args,format);vprintf(format,args);va_end(args);}
bool R_GetBrushSurfaceRange(const model_t*,int &first,int &count) {first=0;count=1;return true;}
const mvertex_t *R_GetBrushSurfaceVertex(const model_t*,const msurface_t*,int i) {return i>=0 && i<4?&quad[i]:nullptr;}
void R_SetEntityWorldTransform(const cl_entity_s *e,float,D3DMATRIX *m) {D3DXMatrixTranslation(reinterpret_cast<D3DXMATRIX*>(m),e->origin[0],e->origin[1],e->origin[2]);}
bool R_StudioRangeValid(const studiohdr_t *h,int offset,size_t bytes) {return h && offset>=0 && h->length>=0 && static_cast<size_t>(offset)<=static_cast<size_t>(h->length) && bytes<=static_cast<size_t>(h->length-offset);}
bool R_StudioHeaderValid(const studiohdr_t *h) {return h && h->numbones>0 && h->numbones<=128 && R_StudioRangeValid(h,h->boneindex,h->numbones*sizeof(mstudiobone_t));}

static void floorSetup() {
    surface.numedges=4;
    const float points[4][3]={{-4096,-4096,0},{4096,-4096,0},{4096,4096,0},{-4096,4096,0}};
    for(int i=0;i<4;++i) memcpy(quad[i].position,points[i],sizeof(points[i]));
}
static void bindPose(studiohdr_t *h,BoneMat34 *pose) {
    const auto *bones=reinterpret_cast<mstudiobone_t*>(reinterpret_cast<byte*>(h)+h->boneindex);
    std::array<btTransform,128> transforms;
    for(int i=0;i<h->numbones;++i) {
        btQuaternion q;q.setEulerZYX(bones[i].value[5],bones[i].value[4],bones[i].value[3]);
        btTransform t(q,btVector3(bones[i].value[0],bones[i].value[1],bones[i].value[2]));
        require(bones[i].parent<i && bones[i].parent>=-1,"model parent order");
        transforms[i]=bones[i].parent<0?t:transforms[bones[i].parent]*t;
        for(int r=0;r<3;++r) {for(int c=0;c<3;++c) pose[i][r][c]=transforms[i].getBasis()[r][c];pose[i][r][3]=transforms[i].getOrigin()[r];}
    }
}
static float difference(const BoneMat34 *a,const BoneMat34 *b,int count) {
    float error=0;for(int i=0;i<count;++i) for(int r=0;r<3;++r) for(int c=0;c<4;++c) error=std::max(error,std::abs(a[i][r][c]-b[i][r][c]));return error;
}
static void triangle(std::vector<btVector3> &v,const btVector3 &a,const btVector3 &b,const btVector3 &c) {v.push_back(a);v.push_back(b);v.push_back(c);}
static void quadMesh(std::vector<btVector3> &v,btVector3 a,btVector3 b,btVector3 c,btVector3 d) {triangle(v,a,b,c);triangle(v,a,c,d);}
static void coreChecks() {
    ragdoll::World w;
    std::vector<btVector3> geometry;
    quadMesh(geometry,{-10,-10,0},{10,-10,0},{10,10,0},{-10,10,0});
    quadMesh(geometry,{2,-10,0},{2,-10,5},{2,10,5},{2,10,0});
    // A real finite sloped platform, not an infinite plane.
    quadMesh(geometry,{-3,-2,.4f},{-1,-2,.8f},{-1,2,.8f},{-3,2,.4f});
    require(w.addMesh(geometry,false)!=nullptr,"floor wall slope mesh");
    std::vector<ragdoll::Bone> bones(4);
    for(int i=0;i<4;++i) {
        bones[i].parent=i-1;bones[i].pose.setOrigin(btVector3(0,0,3+i*.3f));
        bones[i].minimum=btVector3(-.1f,-.1f,-.12f);bones[i].maximum=-bones[i].minimum;
        bones[i].collision=i<3;bones[i].group=i?4:2;
    }
    {
        ragdoll::World impacts;auto *target=impacts.create(bones,{0,0,0});require(target!=nullptr,"torque-limit fixture");
        auto &part=target->parts[2];const auto start=part.body->getLinearVelocity();
        const auto hit=impacts.impact(*target,part.body->getCenterOfMassPosition()+btVector3(0,.08f,0),{1,0,0},4,2.2f);
        require(hit.bone==part.bone && std::abs(hit.impulse-2.2f)<1e-5f,"angular cap retains full bounded shot budget");
        require(std::abs((part.body->getLinearVelocity()-start).x()/part.body->getInvMass()-2.2f)<1e-4f,"actual body receives directional linear momentum");
        require(part.body->getAngularVelocity().length()<=2.5001f,"localized torque still obeys angular increment guard");
    }
    auto *d=w.create(bones,{2,0,0});require(d && d->parts.size()==3 && d->joints.size()==2,"articulated body creation");
    require(d->joints.back()->getConstraintType()==HINGE_CONSTRAINT_TYPE,"distal hinge");
    std::array<btTransform,128> initial,result;w.pose(*d,initial,false);
    for(int i=0;i<4;++i) require((initial[i].getOrigin()-bones[i].pose.getOrigin()).length()<1e-5f,"initial pose offset");
    for(int i=0;i<2400;++i) {
        w.step(1.0/120,800);w.pose(*d,result,false);
        for(int b=0;b<4;++b) require(ragdoll::valid(result[b]),"finite simulation pose");
        for(auto &p:d->parts) {const auto &o=p.body->getWorldTransform().getOrigin();require(o.z()>-.15f && o.x()<2.2f,"floor/wall contact");}
    }
    bool sleeping=true;for(auto &p:d->parts) sleeping&=!p.body->isActive();
    require(sleeping,"settled bodies sleep");
    const btTransform relative=result[2].inverse()*result[3];
    require((relative.getOrigin()-d->local[3].getOrigin()).length()<1e-4f,"unmatched child follows parent");
    w.freeze(*d);w.pose(*d,initial);w.step(10,800);w.pose(*d,result);
    require(d->parts.size()==3 && d->joints.empty() && (result[0].getOrigin()-initial[0].getOrigin()).length()==0,"retirement releases joints and retains collision and pose");
    for(const auto &p:d->parts) require(p.body->isStaticObject() && p.body->getInvMass()==0,"retired corpse collision is stationary");
    w.remove(d);require(w.dolls.empty(),"doll cleanup");
    bones[1].parent=2;require(!w.create(bones,{0,0,0}),"cyclic/out-of-order parent rejection");bones[1].parent=0;
    bones[1].pose.setOrigin(btVector3(NAN,0,0));require(!w.create(bones,{0,0,0}),"NaN rejection");
    auto *platform=w.addMesh(geometry,true);require(platform!=nullptr,"kinematic mesh creation");
    btTransform transform=btTransform::getIdentity();w.moveMesh(*platform,transform);
    transform.setOrigin(btVector3(.04f,0,0));w.moveMesh(*platform,transform);w.step(1.0/30,800);
    require((platform->body->getWorldTransform().getOrigin()-transform.getOrigin()).length()<1e-5f,"moving brush reaches target");
    require(std::abs(platform->body->getLinearVelocity().x()-1.2f)<.01f,"moving brush substep velocity");
    // Equal elapsed time at different frame rates must run the same fixed steps.
    auto simulate=[&](double frame,int frames) {
        ragdoll::World sim;auto input=bones;input[1].pose.setOrigin(btVector3(0,0,3.3f));
        auto *body=sim.create(input,{1,0,0});require(body!=nullptr,"timestep test construction");
        for(int i=0;i<frames;++i) sim.step(frame,800);
        return body->parts[0].body->getWorldTransform().getOrigin();
    };
    require((simulate(1.0/120,120)-simulate(1.0/60,60)).length()<1e-5f,"fixed timestep independent of frame grouping");
    require((simulate(1.0/120,120)-simulate(1.0/240,240)).length()<1e-5f,"high frame rate accumulator");
    require(simulate(10,1).z()>2.5f,"stalled frame bounded catch-up");
    for(int i=0;i<100;++i) {
        auto input=bones;input[1].pose.setOrigin(btVector3(0,0,3.3f));
        auto *body=w.create(input,{0,0,0});require(body!=nullptr,"repeated ownership construction");w.step(1.0/120,800);w.remove(body);
    }
    require(w.dolls.empty(),"repeated teardown releases all dolls");
    {
        ragdoll::World contactWorld;
        std::vector<btVector3> floor;quadMesh(floor,{-5,-5,0},{5,-5,0},{5,5,0},{-5,5,0});
        require(contactWorld.addMesh(floor,false)!=nullptr,"penetration regression floor");
        auto input=bones;input[1].pose.setOrigin(btVector3(0,0,.35f));
        for(size_t i=0;i<input.size();++i) input[i].pose.setOrigin(btVector3(0,0,.1f+static_cast<float>(i)*.25f));
        auto *body=contactWorld.create(input,{0,0,0});require(body && body->initialPenetration>.005f,"measured shallow initial world penetration");
        float peak=0,initialHeight=0,maximumHeight=0;
        for(int step=0;step<30;++step) {
            float mass=0,height=0;btVector3 momentum(0,0,0);
            for(auto &p:body->parts) {const float m=1/p.body->getInvMass();mass+=m;height+=p.body->getCenterOfMassPosition().z()*m;momentum+=p.body->getLinearVelocity()*m;}
            height/=mass;if(step==0) initialHeight=maximumHeight=height;maximumHeight=std::max(maximumHeight,height);
            peak=std::max(peak,momentum.z()/mass);
            contactWorld.step(1.0/120,800);
        }
        std::cout<<"penetration diagnostics depth="<<body->initialPenetration<<" peak_upward_COM="<<peak<<" rise="<<maximumHeight-initialHeight<<"\n";
        require(peak<1.f && maximumHeight-initialHeight<body->initialPenetration+.02f,"floor recovery bounded to clearance rather than an airborne launch");
        std::cout<<"PASS penetration recovery without launch\n";
    }
    {
        ragdoll::World sim;
        std::vector<ragdoll::Bone> input(3);
        input[0].collision=true;input[0].region=ragdoll::Region::Torso;
        input[0].minimum={-.2f,-.2f,-.2f};input[0].maximum=-input[0].minimum;
        input[0].pose.setOrigin({0,0,100});
        for(int i=1;i<3;++i) {input[i]=input[0];input[i].parent=i-1;input[i].pose.setOrigin({0,0,100+.4f*i});}
        auto *fall=sim.create(input,{0,0,0});require(fall!=nullptr,"unsupported sleep fixture");
        for(int i=0;i<480;++i) sim.step(1.0/120,800);
        require(!fall->settled && !sim.safeToRetire(*fall),"falling corpse never sleeps or expires");
        sim.remove(fall);
        std::vector<btVector3> floor;quadMesh(floor,{-5,-5,0},{5,-5,0},{5,5,0},{-5,5,0});
        auto *support=sim.addMesh(floor,true);require(support!=nullptr,"moving sleep support");
        sim.moveMesh(*support,btTransform::getIdentity());
        for(int i=0;i<3;++i) input[i].pose.setOrigin({0,0,1+.4f*i});auto *rest=sim.create(input,{0,0,0});
        for(int i=0;i<2400 && !rest->settled;++i) sim.step(1.0/120,800);
        require(rest->settled && sim.safeToRetire(*rest),"supported island settles");
        const auto settles=rest->settleCount;
        std::array<btTransform,128> visualBefore,visualAfter;
        sim.pose(*rest,visualBefore);
        // Reproduce passive internal reactivation without an external impact.
        rest->parts[0].body->activate(true);rest->parts[0].body->setAngularVelocity({0,0,.5f});
        sim.step(1.0/120,800);sim.pose(*rest,visualAfter);
        require(rest->settled && rest->settleCount==settles && !memcmp(visualBefore.data(),visualAfter.data(),3*sizeof(btTransform)),"internal solver reactivation cannot restart a supported corpse or change its final palette");
        auto moved=btTransform::getIdentity();moved.setOrigin({0,0,-1});
        sim.moveMesh(*support,moved);sim.step(1.0/60,800);
        require(!rest->settled,"moving support wakes cached resting pose");
        for(int i=0;i<2400 && !rest->settled;++i) sim.step(1.0/120,800);
        require(rest->settled,"corpse resettles after moving support");
        const auto hit=sim.impact(*rest,rest->parts[0].body->getCenterOfMassPosition(),{1,0,0},2,2);
        require(hit.impulse>0 && !rest->settled,"new physical impact wakes whole resting island");
        sim.sleeping=false;for(int i=0;i<600;++i) sim.step(1.0/120,800);
        require(!rest->settled,"sleep cvar disables retirement without removing corpse");
        std::cout<<"PASS support-aware sleep, wake, impact wake, falling lifetime and sleep toggle\n";
    }
    std::cout<<"PASS core: contacts, hinge, sleeping, unmatched bones, freeze, cleanup, invalid input\n";
}
static void mapChecks(const std::string &path) {
    std::ifstream file(path,std::ios::binary);uint32_t count=0;float spawn[3]{};
    file.read(reinterpret_cast<char*>(spawn),sizeof(spawn));file.read(reinterpret_cast<char*>(&count),sizeof(count));
    require(count>0 && count<3000000 && count%3==0,"BSP triangle count");
    std::vector<btVector3> triangles;triangles.reserve(count);
    for(uint32_t i=0;i<count;++i) {float p[3];file.read(reinterpret_cast<char*>(p),sizeof(p));triangles.emplace_back(p[0],p[1],p[2]);}
    require(file.good(),"BSP mesh file complete");
    ragdoll::World w;require(w.addMesh(triangles,false)!=nullptr,"actual BSP BVH");
    std::vector<ragdoll::Bone> bones(3);
    for(int i=0;i<3;++i) {
        bones[i].parent=i-1;bones[i].collision=true;
        bones[i].minimum=btVector3(-.08f,-.08f,-.12f);bones[i].maximum=-bones[i].minimum;
        bones[i].pose.setOrigin(btVector3(spawn[0],spawn[1],spawn[2]+1+i*.25f));
    }
    auto *d=w.create(bones,{0,0,0});require(d!=nullptr,"BSP ragdoll creation");
    for(int i=0;i<1200;++i) w.step(1.0/120,800);
    for(auto &p:d->parts) {require(ragdoll::valid(p.body->getWorldTransform()),"BSP finite state");require(p.body->getWorldTransform().getOrigin().z()>spawn[2]-.3f,"actual BSP floor prevents falling through");}
    std::cout<<"PASS actual BSP: "<<count/3<<" triangles, selected native floor contact for 10 seconds\n";
}
static void modelChecks(const std::string &path,bool expectFallback=false,bool requireSettling=false) {
    std::ifstream file(path,std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    require(bytes.size()>=sizeof(studiohdr_t),"owned model read");
    auto *h=reinterpret_cast<studiohdr_t*>(bytes.data());require(static_cast<size_t>(h->length)<=bytes.size(),"model length");
    DX_RagdollReset();floorSetup();model_t map{};map.type=mod_brush;map.surfaces=&surface;g_worldmodel=&map;
    client.time=0;DX_RagdollFrame();require(physics!=nullptr,"adapter map initialization");
    model_t model{};model.type=mod_studio;
    strcpy_s(model.name,"models/faster.mdl");
    auto &e=entities[1];e={};e.index=1;e.model=&model;e.curstate.modelindex=2;
    states[1]={};states[1].serial=100;states[1].modelindex=2;states[1].monster=1;
    require(DX_RagdollCandidate(&e),"local monster eligible");
    e.player=1;require(!DX_RagdollCandidate(&e),"player exclusion");e.player=0;
    e.curstate.movetype=MOVETYPE_FOLLOW;require(!DX_RagdollCandidate(&e),"attachment exclusion");e.curstate.movetype=0;
    strcpy_s(model.name,"models/chainsawguy2.mdl");require(!DX_RagdollCandidate(&e),"boss default animation preservation");
    strcpy_s(model.name,"models/faster.mdl");
    BoneMat34 source[128]{},out[128]{};bindPose(h,source);memcpy(out,source,sizeof(out));
    D3DXMATRIX rotation,translation,world;
    D3DXMatrixRotationYawPitchRoll(&rotation,.7f,.2f,-.1f);D3DXMatrixTranslation(&translation,120,-64,100);world=rotation*translation;
    require(!DX_RagdollPose(&e,h,world,out,0),"living remains animated");
    states[1].dead=1;
    if(expectFallback) {
        require(!DX_RagdollPose(&e,h,world,out,.01f),"oversize model rejects physics");
        require(difference(source,out,h->numbones)==0 && physics->dolls.empty(),"fallback preserves evaluated pose and owns no physics");
        DX_RagdollReset();std::cout<<"PASS expected animation fallback "<<path<<"\n";return;
    }
    require(DX_RagdollPose(&e,h,world,out,.01f),"native model ragdoll construction");
    const float error=difference(source,out,h->numbones);require(error<.002f,"world/model initial roundtrip without snapping");
    const size_t parts=entries[0].doll->parts.size();
    for(int i=1;i<=1200;++i) {
        client.time=i/120.0;DX_RagdollFrame();memcpy(out,source,sizeof(out));
        require(DX_RagdollPose(&e,h,world,out,static_cast<float>(client.time)),"native pose driven");
        for(int b=0;b<h->numbones;++b) for(auto &row:out[b]) for(float value:row) require(std::isfinite(value),"finite native matrices");
    }
    if(requireSettling) {
        lifetime.value=0;
        for(int i=1201;i<=3600 && !entries[0].doll->settled;++i) {client.time=i/120.0;DX_RagdollFrame();}
        require(entries[0].doll->settled,"native articulated corpse settles on supported floor");
        std::array<btTransform,128> a,b;physics->pose(*entries[0].doll,a);
        for(int i=0;i<120;++i) {client.time+=1.0/120;DX_RagdollFrame();}
        physics->pose(*entries[0].doll,b);
        require(!memcmp(a.data(),b.data(),static_cast<size_t>(h->numbones)*sizeof(btTransform)),"settled native palette remains bit-identical");
        std::cout<<"PASS native settle "<<path<<" at "<<client.time<<" seconds; final pose preserved\n";lifetime.value=30;
    }
    require(difference(source,out,h->numbones)>1,"simulation moves rendered palette");
    require(physics->dolls.size()==1,"repeat draws do not duplicate bodies");
    float separation=0;
    for(const auto &joint:entries[0].doll->joints) {
        btTransform a,b;
        ragdoll::jointFrames(*joint,a,b);
        const btVector3 pa=joint->getRigidBodyA().getWorldTransform()*a.getOrigin();
        const btVector3 pb=joint->getRigidBodyB().getWorldTransform()*b.getOrigin();
        separation=std::max(separation,(pa-pb).length());
    }
    require(separation<.08f,"native limbs remain joined");
    const bool safe=physics->safeToRetire(*entries[0].doll);
    float restingSpeed=0,restingSpin=0;
    for(const auto &p:entries[0].doll->parts) {restingSpeed=std::max(restingSpeed,p.body->getLinearVelocity().length());restingSpin=std::max(restingSpin,p.body->getAngularVelocity().length());}
    std::cout<<"REST diagnostics settled="<<entries[0].doll->settled<<" speed="<<restingSpeed<<" spin="<<restingSpin<<" quiet="<<entries[0].doll->quietTime<<" stable="<<entries[0].doll->stationaryTime<<"\n";
    lifetime.value=1;client.time+=.01;DX_RagdollFrame();
    require(entries[0].doll->frozen==safe,"lifetime only retires demonstrably resting physics");lifetime.value=30;
    ++states[1].serial;client.time+=.01;DX_RagdollFrame();require(entries.empty() && physics->dolls.empty(),"entity reuse cleanup");
    memcpy(out,source,sizeof(out));require(DX_RagdollPose(&e,h,world,out,static_cast<float>(client.time)),"new generation owns new doll");
    states[1].modelindex=0;client.time+=.01;DX_RagdollFrame();require(entries.empty(),"removal cleanup");
    DX_RagdollReset();require(!physics && entries.empty() && brushes.empty(),"map/shutdown cleanup");
    std::cout<<"PASS model "<<path<<" bones="<<h->numbones<<" bodies="<<parts<<" roundtrip="<<error<<" joint_error_m="<<separation<<"\n";
}
static void reactionChecks(const std::string &path) {
    std::ifstream file(path,std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    require(bytes.size()>sizeof(studiohdr_t),"reaction native model");auto *hdr=reinterpret_cast<studiohdr_t*>(bytes.data());
    require(R_StudioHeaderValid(hdr) && static_cast<size_t>(hdr->length)<=bytes.size(),"reaction model header");
    DX_RagdollReset();floorSetup();model_t map{};map.type=mod_brush;map.surfaces=&surface;g_worldmodel=&map;
    client.time=0;DX_RagdollFrame();model_t model{};model.type=mod_studio;strcpy_s(model.name,"models/slower.mdl");
    auto &ent=entities[1];ent={};ent.index=1;ent.model=&model;ent.curstate.modelindex=2;ent.curstate.sequence=12;
    auto &state=states[1];state={};state.serial=909;state.monster=1;state.health=100;state.modelindex=2;state.velocity[0]=160;
    D3DXMATRIX world;D3DXMatrixTranslation(&world,0,0,80);
    BoneMat34 base[128]{},pose[128]{},last[128]{};
    StudioCalcBoneTransformsInternal(nullptr,hdr,12,80,base,-1,0,0,&ent,0);memcpy(pose,base,sizeof(pose));
    require(!DX_RagdollPose(&ent,hdr,world,pose,0),"baseline alive animation has no full ragdoll");
    std::array<float,128> scales{};std::vector<ragdoll::Bone> rig;
    require(buildBones(&ent,hdr,world,base,scales,rig),"shared hitbox skeleton build");
    using R=ragdoll::Region;int shoulder=-1,head=-1;
    for(R role:{R::Head,R::Shoulder,R::Torso,R::UpperArm,R::Forearm,R::Thigh,R::Calf}) {
        int bone=-1;for(size_t i=0;i<rig.size();++i) if(rig[i].collision && rig[i].region==role) {bone=static_cast<int>(i);break;}
        require(bone>=0,"native target region exists");
        const btVector3 point=rig[bone].pose*((rig[bone].minimum+rig[bone].maximum)*.5f);
        require(ragdoll::hitBone(rig,point,0)==bone,"head shoulder chest arm and leg map to actual hit location");
        ragdoll::Reactions reaction;reaction.hit(rig,bone,point,btVector3(1,0,0),ragdoll::shotBudget(1,1));
        reaction.step(1.f/60);std::vector<btTransform> reacted;reaction.apply(rig,reacted);
        require(reaction.active() && (reacted[bone].getOrigin()-rig[bone].pose.getOrigin()).length()>1e-5f,"directional additive reaction");
        const btVector3 direct=rig[bone].pose.getBasis()*reaction.bones[bone].shift;
        require(direct.x()>0,"reaction follows bullet direction rather than camera");
        reaction.step(3);require(!reaction.active(),"spring recovers smoothly to animation");
        reaction.apply(rig,reacted);for(size_t i=0;i<rig.size();++i) require((reacted[i].getOrigin()-rig[i].pose.getOrigin()).length()<1e-4f,"recovered hierarchy retains bone offsets");
        if(role==R::Shoulder) shoulder=bone;if(role==R::Head) head=bone;
    }
    require(ragdoll::shotBudget(2,12)>ragdoll::shotBudget(1,1) && ragdoll::shotBudget(2,1000)<=6.001f,"Glock/shotgun strength differs with bounded pellet aggregation");
    const btVector3 point=rig[shoulder].pose*((rig[shoulder].minimum+rig[shoulder].maximum)*.5f);
    ragdoll::Reactions one,many;one.hit(rig,shoulder,point,{1,0,0},2.2f);many=one;
    one.step(1);for(int i=0;i<120;++i) many.step(1.f/120);
    require((one.bones[shoulder].angle-many.bones[shoulder].angle).length()<1e-5f,"analytic reaction integration independent of frame grouping");
    for(int i=0;i<100;++i) {many.hit(rig,shoulder,point,{1,0,0},8);many.step(.01f);}
    std::vector<btTransform> repeated;many.apply(rig,repeated);for(const auto &t:repeated) require(ragdoll::valid(t),"repeated hits remain bounded and finite");
    state.health=80;state.server_time=.01f;state.hit_count=1;auto &hit=state.hits[0];hit={};
    hit.id=100;hit.shot=50;hit.weapon=REF_RAGDOLL_PISTOL;hit.pellets=hit.total_pellets=1;hit.time=.01f;hit.direction[0]=1;
    for(int c=0;c<3;++c) hit.position[c]=point[c]/ragdoll::UnitsToMeters;
    memcpy(pose,base,sizeof(pose));require(DX_RagdollPose(&ent,hdr,world,pose,.01f),"living event consumed without replacing animation");
    memcpy(pose,base,sizeof(pose));state.server_time=.05f;
    require(DX_RagdollPose(&ent,hdr,world,pose,.05f),"living animated recoil visible");
    require(physics->dolls.empty() && state.health==80 && ent.curstate.sequence==12 && state.velocity[0]==160,"living reaction leaves health AI sequence movement and model ownership untouched");
    require(difference(base,pose,hdr->numbones)>.01f,"shoulder visually recoils while alive");memcpy(last,pose,sizeof(last));
    studio_cache_t cache{};
    for(bool gpu:{false,true}) {
        cache.gpuSkinningBufferMode=gpu;
        R_StudioCopyFinalPalette(cache,pose,hdr->numbones,true);
        require(cache.physicsPoseUploaded && cache.gpuSkinningPoseValid && !memcmp(cache.gpuSkinningBoneMatrices,pose,hdr->numbones*sizeof(BoneMat34)),"actual final palette copy preserves reacted matrices and ownership in CPU/GPU modes");
        R_StudioCopyFinalPalette(cache,base,hdr->numbones,false);
        require(!cache.physicsPoseUploaded && !memcmp(cache.gpuSkinningBoneMatrices,base,hdr->numbones*sizeof(BoneMat34)),"GPU recovery restores animation and clears procedural ownership");
    }

    const auto velocity=entries[0].reactions.bones[shoulder].angular;
    memcpy(pose,base,sizeof(pose));DX_RagdollPose(&ent,hdr,world,pose,.05f);
    require((velocity-entries[0].reactions.bones[shoulder].angular).length()==0 && difference(last,pose,hdr->numbones)==0,"repeated draw deduplicates hit and spring integration");
    state.dead=1;state.health=0;state.hit_count=2;state.server_time=.06f;auto &lethal=state.hits[1];lethal=hit;
    lethal.id=101;lethal.shot=51;lethal.time=.06f;lethal.group=1;
    const btVector3 headPoint=rig[head].pose*((rig[head].minimum+rig[head].maximum)*.5f);for(int c=0;c<3;++c) lethal.position[c]=headPoint[c]/ragdoll::UnitsToMeters;
    ent.curstate.sequence=20;StudioCalcBoneTransformsInternal(nullptr,hdr,20,192,pose,-1,0,0,&ent,.06f);
    require(DX_RagdollPose(&ent,hdr,world,pose,.06f) && physics->dolls.size()==1,"lethal hit transitions once into full ragdoll");
    require(difference(last,pose,hdr->numbones)<.002f,"lethal transition retains visually reacted pose without animation snap");
    require(entries[0].impactSeen==101,"nonfatal hit not replayed at death");
    const auto *worldBefore=physics.get();const auto *dollBefore=entries[0].doll;const auto meshes=physics->meshes.size();
    debug.value=2;DX_RagdollPose(&ent,hdr,world,pose,.06f);debug.value=0;
    require(physics.get()==worldBefore && entries[0].doll==dollBefore && physics->meshes.size()==meshes,"debug changes do not reset worlds or reconstruct bodies");
    // Exercise the actual R_BeginFrame callee under console/input changes.
    std::array<btTransform,128> beforeConsole,afterConsole;
    physics->pose(*entries[0].doll,beforeConsole);
    inputInGame=false;
    for(int i=0;i<20;++i) {client.time=.06+i*.1;DX_RagdollFrame();}
    require(physics.get()==worldBefore && entries[0].doll==dollBefore && physics->meshes.size()==meshes,"console cannot reconstruct world or same-serial corpse");
    physics->pose(*entries[0].doll,afterConsole);
    require(!memcmp(beforeConsole.data(),afterConsole.data(),hdr->numbones*sizeof(btTransform)),"console suspension retains exact palette");
    inputInGame=true;client.time=.06;DX_RagdollFrame();
    require(physics.get()==worldBefore && entries[0].doll==dollBefore,"resume clock rewind preserves ownership");
    DX_RagdollDeviceReset("cpu-notification-test");
    availableEntities=0;client.time=.061;DX_RagdollFrame();availableEntities=8;
    enabled.value=0;client.time=.062;DX_RagdollFrame();enabled.value=1;
    clientActive=false;client.time=.063;DX_RagdollFrame();clientActive=true;
    for(int i=0;i<3;++i) {enabled.value=1;debug.value=0;gravity.value=800;client.time=.063;DX_RagdollFrame();}
    require(physics.get()==worldBefore && entries[0].doll==dollBefore,"temporary metadata gap, inactive client and unchanged settings preserve corpse");
    physics->pose(*entries[0].doll,afterConsole);
    require(!memcmp(beforeConsole.data(),afterConsole.data(),hdr->numbones*sizeof(btTransform)),"suspension does not advance physics or reset interpolation");
    state.modelindex=0;client.time=.07;DX_RagdollFrame();require(entries.empty() && physics->dolls.empty(),"hit reaction and ragdoll ownership cleanup");
    DX_RagdollReset();require(!physics && entries.empty(),"map reset clears springs and hit event ownership");
    std::cout<<"PASS living reaction mapping, direction, weapon scaling, analytic recovery, repeated hits, lethal recoil handoff, debug invariance and cleanup\n";
}
static void transitionChecks(const std::string &path) {
    std::ifstream file(path,std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    require(bytes.size()>=sizeof(studiohdr_t),"transition model read");auto *hdr=reinterpret_cast<studiohdr_t*>(bytes.data());
    require(static_cast<size_t>(hdr->length)<=bytes.size() && R_StudioHeaderValid(hdr),"transition header");
    const auto *seq=reinterpret_cast<const mstudioseqdesc_t*>(bytes.data()+hdr->seqindex);
    int death=-1,walk=0;
    for(int i=0;i<hdr->numseq;++i) {
        if(!_strnicmp(seq[i].label,"die",3) || !_strnicmp(seq[i].label,"death",5)) if(death<0) death=i;
        if(!_strnicmp(seq[i].label,"walk",4) || !_strnicmp(seq[i].label,"run",3)) walk=i;
    }
    require(death>=0,"native death sequence found");
    require(seq[walk].seqgroup==0 && seq[death].seqgroup==0,"native transition streams are embedded and available");
    DX_RagdollReset();floorSetup();model_t map{};map.type=mod_brush;map.surfaces=&surface;g_worldmodel=&map;
    client.time=0;DX_RagdollFrame();model_t model{};model.type=mod_studio;strcpy_s(model.name,"models/regular-enemy.mdl");
    auto &ent=entities[1];ent={};ent.index=1;ent.model=&model;ent.curstate.modelindex=2;ent.curstate.sequence=walk;
    auto &state=states[1];state={};state.modelindex=2;state.serial=123;state.monster=1;state.health=100;state.grounded=1;
    state.velocity[0]=160;
    D3DXMATRIX world;D3DXMatrixTranslation(&world,0,0,120);
    BoneMat34 pose[128]{},last[128]{},deathPose[128]{};
    // Place the actual animated feet/hands against the fixture's real floor.
    // A three-metre drop would test falling impacts rather than a grounded kill.
    StudioCalcBoneTransformsInternal(nullptr,hdr,walk,80,pose,-1,0,0,&ent,0);
    BoneMat34 bind[128]{};bindPose(hdr,bind);require(difference(bind,pose,hdr->numbones)>.01f,"actual native animation differs from bind pose");
    float bottom=FLT_MAX;
    const auto *boxes=reinterpret_cast<const mstudiobbox_t*>(bytes.data()+hdr->hitboxindex);
    for(int i=0;i<hdr->numhitboxes;++i) for(int corner=0;corner<8;++corner) {
        const auto &box=boxes[i];require(box.bone>=0 && box.bone<hdr->numbones,"native hitbox bone");
        float point[3],transformed[3];for(int c=0;c<3;++c) point[c]=corner&(1<<c)?box.bbmax[c]:box.bbmin[c];
        VectorTransform(point,pose[box.bone],transformed);bottom=std::min(bottom,transformed[2]);
    }
    require(std::isfinite(bottom),"native animated floor height");D3DXMatrixTranslation(&world,0,0,-bottom+.5f);
    for(int i=0;i<3;++i) {
        const float time=i*.02f;client.time=time;DX_RagdollFrame();
        StudioCalcBoneTransformsInternal(nullptr,hdr,walk,80.f+i*2,pose,-1,0,0,&ent,time);
        require(!DX_RagdollPose(&ent,hdr,world,pose,time),"native moving animation owns living pose");memcpy(last,pose,sizeof(last));
    }
    require(difference(last,pose,hdr->numbones)==0,"animation capture retained");
    // No deadflag: tests the CoF health+actual death-label fallback. Also feed
    // an extreme game knockback value, which must never become body velocity.
    state.health=0;state.dead=0;state.monster=0;state.velocity[2]=5000;state.sequence=death;ent.curstate.sequence=death;
    StudioCalcBoneTransformsInternal(nullptr,hdr,death,192,deathPose,-1,0,0,&ent,.06f);
    memcpy(pose,deathPose,sizeof(pose));
    require(DX_RagdollPose(&ent,hdr,world,pose,.06f),"native health/death-label activation without deadflag");
    require(difference(last,pose,hdr->numbones)<.002f,"pre-death evaluated walking/crawling pose survives death frame discontinuity");
    auto *doll=entries[0].doll;require(entries[0].deathSource==2,"diagnosed fallback death source");
    float mass=0,dominant=0;int hinges=0;btVector3 momentum(0,0,0);
    ragdoll::Part *head=nullptr;
    for(auto &part:doll->parts) {
        const float m=1/part.body->getInvMass();mass+=m;momentum+=part.body->getLinearVelocity()*m;
        if(part.region==ragdoll::Region::Torso || part.region==ragdoll::Region::Pelvis) dominant+=m;
        if(part.region==ragdoll::Region::Head) head=&part;
    }
    for(const auto &joint:doll->joints) if(joint->getConstraintType()==HINGE_CONSTRAINT_TYPE) ++hinges;
    require(std::abs(mass-65)<.01f && dominant/mass>.45f,"exact total mass and dominant pelvis/torso");
    require(std::abs(momentum.z()/mass)<.001f && std::abs(momentum.x()/mass-160*ragdoll::UnitsToMeters)<.001f,"no upward death kick; preserve horizontal locomotion");
    float animationEnergy=0;const btVector3 common=momentum/mass;
    for(const auto &part:doll->parts) animationEnergy+=.5f/part.body->getInvMass()*(part.body->getLinearVelocity()-common).length2();
    require(animationEnergy<=12.001f,"bounded relative animation energy without cancelling locomotion");
    for(const auto &joint:doll->joints) {
        btTransform a,b;
        if(joint->getConstraintType()==HINGE_CONSTRAINT_TYPE) {
            auto *h=static_cast<const btHingeConstraint*>(joint.get());a=h->getAFrame();b=h->getBFrame();
            const btVector3 axis=h->getRigidBodyA().getWorldTransform().getBasis()*a.getBasis().getColumn(2);
            require((h->getRigidBodyB().getAngularVelocity()-h->getRigidBodyA().getAngularVelocity()).cross(axis).length()<.0001f,"initial hinge angular velocity lies on allowed axis");
        }
        else {auto *c=static_cast<const btConeTwistConstraint*>(joint.get());a=c->getAFrame();b=c->getBFrame();}
        const auto &bodyA=joint->getRigidBodyA();const auto &bodyB=joint->getRigidBodyB();
        const btVector3 va=bodyA.getVelocityInLocalPoint(bodyA.getWorldTransform().getBasis()*a.getOrigin());
        const btVector3 vb=bodyB.getVelocityInLocalPoint(bodyB.getWorldTransform().getBasis()*b.getOrigin());
        require((va-vb).length()<.0001f,"initial joint anchor velocities agree before solving");
    }
    require(hinges>=4,"anatomical elbows and knees including zero/nonstandard hit groups");
    require(head!=nullptr,"localized head body");
    const btVector3 point=head->body->getCenterOfMassPosition()+btVector3(0,.03f,0);
    state.impact_id=42;state.impact_group=1;
    for(int c=0;c<3;++c) state.impact_position[c]=point[c]/ragdoll::UnitsToMeters;
    state.impact_direction[0]=1;
    state.server_time=.06f;state.hit_count=1;state.hits[0]={};
    state.hits[0].id=42;state.hits[0].shot=42;state.hits[0].group=1;state.hits[0].time=.06f;state.hits[0].pellets=state.hits[0].total_pellets=1;state.hits[0].weapon=REF_RAGDOLL_RIFLE;
    memcpy(state.hits[0].position,state.impact_position,sizeof(state.impact_position));state.hits[0].direction[0]=1;
    const btVector3 before=head->body->getLinearVelocity();
    require(DX_RagdollPose(&ent,hdr,world,pose,.06f),"confirmed impact delivered");
    const btVector3 after=head->body->getLinearVelocity();
    require(std::abs((after-before).length()-3.f*head->body->getInvMass())<1e-4f,"localized head velocity reflects the full bounded momentum and actual mass");
    btVector3 afterMomentum(0,0,0);for(const auto &part:doll->parts) afterMomentum+=part.body->getLinearVelocity()/part.body->getInvMass();
    require((afterMomentum-momentum).length()<=3.001f && std::abs((afterMomentum-momentum).z())<.001f,"headshot net momentum bounded in N.s without vertical launch");
    require(DX_RagdollPose(&ent,hdr,world,pose,.06f) && (after-head->body->getLinearVelocity()).length()==0,"duplicate draw cannot apply impact twice");
    require(head->body->getAngularVelocity().length()<=6.51f,"bounded angular response");
    // Keep ownership across flag loss, sequence reset and asynchronous model
    // notification. A compatible new asset must not restart the simulation.
    state.monster=0;ent.curstate.sequence=0;state.modelindex=3;
    client.time=.07;DX_RagdollFrame();require(entries.size()==1 && entries[0].doll==doll,"death ownership latched after flag/sequence changes");
    model_t replacement=model;ent.model=&replacement;ent.curstate.modelindex=3;ent.curstate.body=1;
    require(DX_RagdollPose(&ent,hdr,world,pose,.07f) && entries[0].doll==doll && physics->dolls.size()==1,"compatible corpse/bodygroup swap preserves same doll");
    state.monster=1;
    for(int i=0;i<180;++i) {client.time=.08+i/120.0;DX_RagdollFrame();require(DX_RagdollPose(&ent,hdr,world,pose,static_cast<float>(client.time)),"retained physics pose");}
    std::cout<<"transition diagnostics peak="<<doll->maximumSpeed<<" corrections="<<doll->velocityCorrections<<"\n";
    for(const auto &part:doll->parts) require(part.body->getLinearVelocity().length()<=20.001f,"bounded transition velocities");
    std::cout<<"PASS native transition "<<path<<" walk="<<walk<<" death="<<death<<" hinges="<<hinges<<" mass="<<mass<<" torso_fraction="<<dominant/mass<<" peak="<<doll->maximumSpeed<<" m/s\n";
    DX_RagdollReset();
}
static void handoffChecks(const std::string &path) {
    std::ifstream file(path,std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    auto *hdr=reinterpret_cast<studiohdr_t*>(bytes.data());require(R_StudioHeaderValid(hdr),"handoff native model header");
    DX_RagdollReset();floorSetup();model_t map{};map.type=mod_brush;map.surfaces=&surface;g_worldmodel=&map;
    client.time=0;DX_RagdollFrame();model_t model{};model.type=mod_studio;strcpy_s(model.name,"models/slower.mdl");
    auto &ent=entities[1];ent={};ent.index=1;ent.model=&model;ent.curstate.modelindex=2;ent.curstate.sequence=12;
    auto &state=states[1];state={};state.serial=921;state.monster=1;state.health=100;state.modelindex=2;
    D3DXMATRIX world;D3DXMatrixTranslation(&world,0,0,80);
    BoneMat34 base[128]{},pose[128]{};StudioCalcBoneTransformsInternal(nullptr,hdr,12,80,base,-1,0,0,&ent,0);memcpy(pose,base,sizeof(pose));
    require(!DX_RagdollPose(&ent,hdr,world,pose,0),"pre-shot living animation");
    std::array<float,128> scales{};std::vector<ragdoll::Bone> rig;require(buildBones(&ent,hdr,world,base,scales,rig),"real Slower handoff rig");
    int head=-1,neck=-1;
    for(size_t i=0;i<rig.size();++i) {if(rig[i].region==ragdoll::Region::Head)head=static_cast<int>(i);if(rig[i].region==ragdoll::Region::Neck)neck=static_cast<int>(i);}
    require(head>=0 && neck>=0,"real head and neck bones");
    const auto point=rig[head].pose*((rig[head].minimum+rig[head].maximum)*.5f);
    auto &event=state.hits[0];event={};event.id=501;event.shot=401;event.weapon=REF_RAGDOLL_SHOTGUN;event.pellets=event.total_pellets=8;event.bone=head+1;event.direction[0]=1;event.time=.01f;
    for(int c=0;c<3;++c) event.position[c]=point[c]/ragdoll::UnitsToMeters;
    // Exact gameplay ordering: shot confirmed while deadflag still says alive,
    // native Killed memory is already set and CoF has positive corpse health.
    state.hit_count=1;state.health=10;state.dead=0;state.deadflag=0;state.native_death=1;state.server_time=.01f;
    client.time=.01;DX_RagdollFrame();
    memcpy(pose,base,sizeof(pose));require(DX_RagdollPose(&ent,hdr,world,pose,.01f),"native Killed evidence owns lethal frame before raw deadflag");
    require(entries.size()==1 && entries[0].doll && entries[0].deathSource==3 && !entries[0].reactions.active(),"lethal shotgun cannot be consumed by the living spring branch");
    auto *doll=entries[0].doll;auto &body=*doll->parts[doll->partForBone[head]].body;
    const auto velocity=body.getLinearVelocity(),angular=body.getAngularVelocity();
    require(velocity.x()>.5f && velocity.y()==0 && velocity.z()==0,"shotgun actually delivers directional momentum to the head body on the lethal frame");
    require(entries[0].impactSeen==501 && angular.length()<=2.5001f,"one bounded physical impact recorded");
    client.time=.01;DX_RagdollFrame();require(entries.size()==1 && entries[0].doll==doll,"positive corpse health cannot erase early native death ownership");
    state.native_death=-1;DX_RagdollFrame();require(entries.size()==1 && entries[0].doll==doll,"a temporary unavailable native view cannot revive or erase a latched corpse");state.native_death=1;
    state.dead=1;state.deadflag=1;ent.curstate.sequence=26;
    memcpy(pose,base,sizeof(pose));require(DX_RagdollPose(&ent,hdr,world,pose,.01f),"later raw corpse notification retains physics");
    require((velocity-body.getLinearVelocity()).length()==0 && (angular-body.getAngularVelocity()).length()==0,"later death notification and duplicate draw cannot replay the lethal shot");
    // Group zero and overlapping bounds cannot override a verified studio bone.
    auto *other=physics->create(rig,{0,0,0});require(other!=nullptr,"authoritative mapping fixture");
    const auto overlap=rig[neck].pose*((rig[neck].minimum+rig[neck].maximum)*.5f);
    const auto mapped=physics->impact(*other,overlap,{1,0,0},0,2.2f,head);
    require(mapped.bone==head,"authoritative trace bone survives overlap and source-pose differences");
    require(physics->impact(*other,point,{1,0,0},0,2.2f,hdr->numbones+1).bone<0,"invalid authoritative bone cannot fall back to another limb");
    require(physics->impact(*other,point+btVector3(10,0,0),{1,0,0},0,2.2f,head).bone<0,"source bone cannot apply a remote impulse after a corpse has moved away from the actual trace");
    physics->remove(other);
    state.modelindex=0;client.time=.02;DX_RagdollFrame();DX_RagdollReset();
    std::cout<<"PASS gameplay ordering: native Killed before deadflag, positive corpse health, lethal head momentum, exact once-only handoff and authoritative bone mapping\n";
}
static float angularViolation(const ragdoll::Doll &d) {
    float maximum=0;
    for(const auto &joint:d.joints) {
        if(joint->getConstraintType()==HINGE_CONSTRAINT_TYPE) {
            auto &h=*static_cast<btHingeConstraint*>(joint.get());
            const float a=h.getHingeAngle();maximum=std::max(maximum,std::max(h.getLowerLimit()-a,a-h.getUpperLimit()));
        } else {
            const auto &c=*static_cast<const btConeTwistConstraint*>(joint.get());
            const auto a=c.getRigidBodyA().getWorldTransform()*c.getAFrame(),b=c.getRigidBodyB().getWorldTransform()*c.getBFrame();
            const btQuaternion q=b.getRotation().inverse()*a.getRotation();
            const btQuaternion swing=shortestArcQuat(btVector3(1,0,0),quatRotate(q,btVector3(1,0,0)).normalized());
            const float angle=swing.getAngle();
            if(angle>1e-5f) {
                const auto axis=swing.getAxis();const float inv=axis.z()*axis.z()/(c.getSwingSpan1()*c.getSwingSpan1())+axis.y()*axis.y()/(c.getSwingSpan2()*c.getSwingSpan2());
                if(inv>1e-8f) maximum=std::max(maximum,angle-1/std::sqrt(inv));
            }
            auto twist=swing.inverse()*q;if(twist.w()<0)twist=btQuaternion(-twist.x(),-twist.y(),-twist.z(),-twist.w());
            maximum=std::max(maximum,twist.getAngle()-c.getTwistSpan());
        }
    }
    return maximum;
}
static void stressChecks(const std::string &path) {
    std::ifstream file(path,std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    auto *hdr=reinterpret_cast<studiohdr_t*>(bytes.data());require(R_StudioHeaderValid(hdr),"stress model");
    model_t model{};model.type=mod_studio;strcpy_s(model.name,"models/slower.mdl");
    cl_entity_t ent{};ent.index=1;ent.model=&model;
    int failures=0;
    for(int variant=0;variant<8;++variant) {
        const int sample=variant%8;const int seq=sample<4?12:20;ent.curstate.sequence=seq;
        BoneMat34 pose[128]{};StudioCalcBoneTransformsInternal(nullptr,hdr,seq,(sample%4)*64.f,pose,-1,0,0,&ent,0);
        D3DXMATRIX world;D3DXMatrixTranslation(&world,0,0,80);
        std::array<float,128> scales{};std::vector<ragdoll::Bone> rig;
        require(buildBones(&ent,hdr,world,pose,scales,rig),"native stress rig");
        ragdoll::World sim;std::vector<btVector3> floor;quadMesh(floor,{-10,-10,0},{10,-10,0},{10,10,0},{-10,10,0});sim.addMesh(floor,false);
        auto *d=sim.create(rig,{0,0,0});require(d!=nullptr,"native stress doll");
        require(d->parts.size()==11,"Slower uses anatomical bodies rather than one body per studio hitbox");
        int shapes=0;float totalMass=0;
        for(const auto &part:d->parts) {shapes+=part.childShapes.empty()?1:static_cast<int>(part.childShapes.size());totalMass+=1/part.body->getInvMass();}
        require(shapes==21 && std::abs(totalMass-65)<.001f,"compound anatomy preserves all 21 collision shapes and exactly 65 kg");
        std::array<btTransform,128> seed;sim.pose(*d,seed,false);
        for(size_t i=0;i<rig.size();++i) require((seed[i].getOrigin()-rig[i].pose.getOrigin()).length()<1e-5f,"compound construction retains the exact captured skeletal pose");
        for(auto &joint:d->joints) if(joint->getConstraintType()==CONETWIST_CONSTRAINT_TYPE) {
            const auto &c=*static_cast<btConeTwistConstraint*>(joint.get());
            const auto role=d->parts[d->partForBone[joint->getRigidBodyB().getUserIndex()]].region;
            const bool limb=role==ragdoll::Region::Thigh || role==ragdoll::Region::UpperArm;
            require(std::abs(c.getSwingSpan1()-(limb?1.15f:.45f))<1e-6f && std::abs(c.getSwingSpan2()-(limb?.9f:.45f))<1e-6f && std::abs(c.getTwistSpan()-(limb?.65f:.35f))<1e-6f && !c.isMotorEnabled(),"anatomical cone limits remain unchanged and motors remain disabled");
        }
        for(int i=0;i<3600 && !d->settled;++i) {
            sim.step(1.0/120,800);
            if(i%600==599) {
                int bone=-1;float spin=0;
                for(auto &part:d->parts) if(part.body->getAngularVelocity().length()>spin) {spin=part.body->getAngularVelocity().length();bone=part.bone;}
                std::cout<<"STRESS variant="<<variant<<" t="<<(i+1)/120.f<<" speed="<<d->currentSpeed<<" spin="<<d->currentSpin<<" spinning_bone="<<bone<<" energy="<<d->restEnergy<<" joint="<<d->restJointError<<" quiet="<<d->quietTime<<" stable="<<d->stationaryTime<<" corrections="<<d->velocityCorrections<<" self_contacts="<<d->selfContacts<<" self_depth="<<d->selfPenetration<<" blockers="<<d->restBlockers<<" angular_violation="<<angularViolation(*d)<<"\n";
            }
        }
        {
            std::array<bool,128> visible;visible.fill(true);
            auto it=std::find_if(d->parts.begin(),d->parts.end(),[](const ragdoll::Part &p){return p.region==ragdoll::Region::Forearm && !p.childShapes.empty();});
            require(it!=d->parts.end(),"compound forearm exists");
            auto &compound=*static_cast<btCompoundShape*>(it->shape.get());const int count=compound.getNumChildShapes();
            const int hidden=it->childBones.back();visible[hidden]=false;
            std::array<btTransform,128> before,after;sim.pose(*d,before);
            sim.updateVisibility(*d,visible);require(compound.getNumChildShapes()==count-1 && !d->visibleBones[hidden],"hidden compound child loses collision without deleting its owner");
            visible[hidden]=true;sim.updateVisibility(*d,visible);require(compound.getNumChildShapes()==count,"compatible bodygroup restores the same child collider");
            sim.pose(*d,after);require(!memcmp(before.data(),after.data(),rig.size()*sizeof(btTransform)),"compound bodygroup visibility preserves the exact visual palette");
        }
        const float settledAt=d->age;
        if(!d->settled) ++failures;
        if(d->settled) {
            require(angularViolation(*d)<.02f,"resting joint angles remain inside anatomical limits within solver tolerance");
            std::array<btTransform,128> before,after;sim.pose(*d,before);
            const auto count=d->settleCount;
            for(int frame=0;frame<240;++frame) sim.step(1.0/120,800);
            sim.pose(*d,after);
            require(d->settled && d->settleCount==count && count==1 && !memcmp(before.data(),after.data(),rig.size()*sizeof(btTransform)),"all eight poses retain exact rest palettes without resettling");
            for(const auto &part:d->parts) require(!part.body->isActive() && part.body->getLinearVelocity().length2()==0 && part.body->getAngularVelocity().length2()==0,"settled compound island is motionless and asleep");
        }
        std::cout<<"STRESS result variant="<<variant<<" settled="<<d->settled<<" settled_at="<<settledAt<<" observed_age="<<d->age<<" settles="<<d->settleCount<<" angular_violation="<<angularViolation(*d)<<"\n";
    }
    require(failures==0,"all eight animated Slower poses must settle");
}
#include "verify_ragdoll_render.h"
#include "verify_ragdoll_headshot.h"
#include "verify_ragdoll_capture.h"
#include "verify_ragdoll_handoff.h"
#include "verify_ragdoll_anchors.h"
int main(int argc,char **argv) try {
    gEngfuncs.EngineGetParm=parm;gEngfuncs.R_BeamGetEntity=entity;
    gEngfuncs.Cvar_RegisterVariable=reg;gEngfuncs.Con_Printf=print;gp_cl=&client;
    DX_RagdollRegister();coreChecks();
    for(int i=1;i<argc;++i) {
        const std::string arg=argv[i];
        if(arg.rfind("--anchors=",0)==0) anchorChecks(arg.substr(10));
        else if(arg.rfind("--recoil=",0)==0) recoilAnatomyChecks(arg.substr(9));
        else if(arg.rfind("--reference=",0)==0) referenceModelChecks(arg.substr(12));
        else if(arg.rfind("--capture=",0)==0) captureFileChecks(arg.substr(10));
        else if(arg.rfind("--adapter-handoff=",0)==0) adapterHandoffChecks(arg.substr(18));
        else if(arg.rfind("--motion=",0)==0) motionHandoffChecks(arg.substr(9));
        else if(arg.rfind("--headshot=",0)==0) headshotChecks(arg.substr(11));
        else if(arg.rfind("--handoff=",0)==0) handoffChecks(arg.substr(10));
        else if(arg.rfind("--integration=",0)==0) integrationChecks(arg.substr(14));
        else if(arg.rfind("--stress=",0)==0) stressChecks(arg.substr(9));
        else if(arg.rfind("--map=",0)==0) mapChecks(arg.substr(6));
        else if(arg.rfind("--features=",0)==0) reactionChecks(arg.substr(11));
        else if(arg.rfind("--settle=",0)==0) modelChecks(arg.substr(9),false,true);
        else if(arg.rfind("--transition=",0)==0) transitionChecks(arg.substr(13));
        else if(arg.rfind("--fallback=",0)==0) modelChecks(arg.substr(11),true);else modelChecks(arg);
    }
    std::cout<<"PASS all CPU ragdoll checks; no game or graphics device started\n";return 0;
} catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
