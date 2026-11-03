#include <map>
struct CaptureReader {
    const byte *data;size_t size,offset=0;
    template<class T> T value(){require(sizeof(T)<=size-offset,"capture chunk bounds");T v;memcpy(&v,data+offset,sizeof(v));offset+=sizeof(v);return v;}
};
static float replayCapture(const std::vector<byte> &bytes) {
    require(!bytes.empty() && bytes.size()<=CaptureLimit,"capture file size bound");
    CaptureReader file{bytes.data(),bytes.size()};ragdoll::World sim;std::map<int,ragdoll::Doll*> dolls;std::map<int,ragdoll::Mesh*> meshes;
    int selected=0,steps=0,submissions=0;bool initial=false,ended=false;float peakError=0,vertexError=0;
    for(int chunk=0;file.offset<file.size && chunk<4096;++chunk) {
        require(!ended,"capture ends once with no trailing records");const auto header=file.value<CaptureChunk>();require(header.bytes<=file.size-file.offset,"capture payload bound");CaptureReader r{file.data+file.offset,header.bytes};file.offset+=header.bytes;
        if(header.type==CaptureInitial) {
            require(!initial,"single capture initial record");auto h=r.value<CaptureInitialData>();require(h.magic==0x33434452 && h.version==3 && h.retainedBytes==sizeof(RetainedDoll),"capture value format/version");
            require(h.olderCount>=0 && h.olderCount<=128 && h.historyCount>=0 && h.historyCount<=128,"capture history bounds");
            for(int i=0;i<h.olderCount+h.historyCount;++i)require(ragdoll::valid(unpack(r.value<RetainedTransform>())),"capture history transforms");selected=h.index;initial=true;
        } else if(header.type==CaptureDoll) {
            require(initial,"capture begins with identity");const auto h=r.value<CaptureDollData>();require(h.bytes==sizeof(RetainedDoll) && !dolls.count(h.index),"capture doll identity/size");
            const auto saved=r.value<RetainedDoll>();require(saved.version==2 && saved.count>0 && saved.count<=128 && saved.parts>0 && saved.parts<=64,"capture rig bounds");
            std::vector<ragdoll::Bone> rig(saved.count);for(int i=0;i<saved.count;++i){auto &b=rig[i];const auto &s=saved.rig[i];b.parent=s.parent;b.group=s.group;b.region=static_cast<ragdoll::Region>(s.region);b.collision=s.collision!=0;b.pose=unpack(s.pose);b.minimum={s.minimum[0],s.minimum[1],s.minimum[2]};b.maximum={s.maximum[0],s.maximum[1],s.maximum[2]};}
            auto *d=sim.create(rig,{0,0,0});require(d && d->parts.size()==static_cast<size_t>(saved.parts),"capture rig reconstructs original graph");
            for(int i=0;i<saved.parts;++i){auto &p=d->parts[i];const auto &s=saved.bodies[i];require(p.bone==s.bone && ragdoll::valid(unpack(s.solved)) && ragdoll::valid(unpack(s.previous)),"capture physical body identity");p.body->setCenterOfMassTransform(unpack(s.solved));p.body->setInterpolationWorldTransform(unpack(s.previous));p.body->setLinearVelocity({s.linear[0],s.linear[1],s.linear[2]});p.body->setAngularVelocity({s.angular[0],s.angular[1],s.angular[2]});d->restBodyPose[p.bone]=unpack(s.solved);}
            d->supported=saved.supported!=0;d->age=saved.age;d->sinceImpact=saved.sinceImpact;d->settled=saved.settled!=0;
            for(int i=0;i<saved.count;++i){d->visibleBones[i]=saved.visible[i]!=0;d->restPose[i]=unpack(saved.visual[i]);}sim.updateVisibility(*d,d->visibleBones);
            auto &t=d->transition;t.active=saved.transitioning!=0;t.shot=saved.transitionShot;t.sequence=saved.referenceSequence;t.peakTime=saved.peakTime;t.elapsed=saved.transitionElapsed;t.work=saved.muscleWork;t.budget=saved.muscleBudget;
            require(!t.active || (std::isfinite(t.peakTime)&&t.peakTime>=1.f/120&&t.peakTime<=.3f&&std::isfinite(t.budget)&&t.budget>0),"capture transition parameters");
            for(size_t i=0;i<d->joints.size();++i){auto &m=t.muscles[i];m.rest={saved.muscleRest[i][0],saved.muscleRest[i][1],saved.muscleRest[i][2],saved.muscleRest[i][3]};m.target={saved.muscleTarget[i][0],saved.muscleTarget[i][1],saved.muscleTarget[i][2]};m.referenceAngle=saved.muscleAngle[i];require(std::isfinite(m.rest.length2())&&btFabs(m.rest.length2()-1)<.01f&&ragdoll::finite(m.target)&&std::isfinite(m.referenceAngle)&&m.referenceAngle>=0&&m.referenceAngle<3.2f,"capture muscle values finite");}
            if(d->settled)for(auto &p:d->parts){p.body->setLinearVelocity({0,0,0});p.body->setAngularVelocity({0,0,0});p.body->forceActivationState(ISLAND_SLEEPING);}
            if(saved.frozen){sim.freeze(*d);for(int i=0;i<saved.count;++i)d->frozenPose[i]=unpack(saved.visual[i]);}
            sim.restorePhase(saved.phase);sim.refresh(*d);dolls[h.index]=d;
        } else if(header.type==CaptureMesh) {
            const auto h=r.value<CaptureMeshData>();require(h.vertices>0 && h.vertices<=32768 && h.vertices%3==0 && !meshes.count(h.id),"capture mesh count/id");
            std::vector<btVector3> vertices(h.vertices);for(auto &v:vertices){for(int j=0;j<3;++j)v[j]=r.value<float>();require(ragdoll::finite(v),"capture mesh vertices finite");}
            auto *m=sim.addMesh(vertices,h.moving!=0);require(m && ragdoll::valid(unpack(h.world)) && ragdoll::valid(unpack(h.target)),"capture collision mesh");m->body->setWorldTransform(unpack(h.world));m->body->setInterpolationWorldTransform(unpack(h.world));m->body->setCollisionFlags(h.flags);m->target=unpack(h.target);m->initialized=true;meshes[h.id]=m;
        } else if(header.type==CaptureImpact) {
            const auto h=r.value<CaptureImpactData>();require(dolls.count(h.index),"capture impact recipient exists");auto &d=*dolls[h.index];sim.refresh(d);
            const btVector3 point(h.point[0],h.point[1],h.point[2]),direction(h.direction[0],h.direction[1],h.direction[2]);const auto hit=sim.impact(d,point,direction,h.group,h.impulse,h.source);
            require(hit.bone>=0,"captured accepted event replays into physics");if(h.shot==d.transition.shot)sim.transitionHit(d,hit,point,direction,h.fraction);
        } else if(header.type==CaptureStep) {
            const auto h=r.value<CaptureStepData>();require(dolls.count(h.index) && h.index==selected && h.parts>0 && h.parts<=64 && h.moving>=0 && h.moving<=256 && h.contacts>=0 && h.contacts<=128 && std::isfinite(h.elapsed) && h.elapsed>0,"capture step bounds/time");
            std::vector<RetainedPart> expected(h.parts);for(auto &p:expected)p=r.value<RetainedPart>();
            for(int i=0;i<h.contacts;++i){const auto c=r.value<ragdoll::ContactObservation>();require(std::isfinite(c.distance)&&std::isfinite(c.impulse)&&ragdoll::finite(btVector3(c.normal[0],c.normal[1],c.normal[2])),"capture contact observations finite");}
            for(int i=0;i<h.moving;++i){const auto m=r.value<CaptureMeshPose>();require(meshes.count(m.id),"captured moving brush exists");meshes[m.id]->body->setCollisionFlags(m.flags);sim.moveMesh(*meshes[m.id],unpack(m.target));}
            sim.step(h.elapsed,h.gravity);auto &d=*dolls[h.index];require(d.parts.size()==expected.size(),"captured step graph stable");
            for(size_t i=0;i<expected.size();++i){const auto &actual=d.parts[i].body->getWorldTransform();const auto reference=unpack(expected[i].solved);require(ragdoll::valid(reference),"capture expected transform valid");peakError=std::max(peakError,(actual.getOrigin()-reference.getOrigin()).length());}
            ++steps;
        } else if(header.type==CaptureSubmit) {
            const auto h=r.value<CaptureSubmitData>();require(dolls.count(h.index) && h.count==dolls[h.index]->count && h.count<=128 && h.vertices>=0 && h.vertices<=16,"capture submission bounds");
            std::array<btTransform,128> physical,submitted;sim.pose(*dolls[h.index],physical);
            for(int i=0;i<h.count;++i){submitted[i]=unpack(r.value<RetainedTransform>());require(ragdoll::valid(submitted[i]),"capture submitted palette finite");peakError=std::max(peakError,(submitted[i].getOrigin()-physical[i].getOrigin()).length());}
            for(int i=0;i<h.vertices;++i){const auto v=r.value<CaptureVertex>();require(v.bone>=0&&v.bone<h.count&&std::isfinite(v.scale)&&v.scale>0,"capture vertex bone/scale valid");const auto point=submitted[v.bone]*(btVector3(v.local[0],v.local[1],v.local[2])*(v.scale*ragdoll::UnitsToMeters));vertexError=std::max(vertexError,(point-btVector3(v.world[0],v.world[1],v.world[2])).length());}++submissions;
        } else if(header.type==CaptureEnd) {const auto h=r.value<CaptureEndData>();require(h.complete && h.frames==steps,"capture scene complete and frame accounting");ended=true;}
        else require(false,"known capture chunk type");
        require(r.offset==r.size,"capture chunk consumed exactly");
    }
    require(initial && ended && steps>0 && submissions>0,"capture contains initial state, steps and actual submitted geometry");
    std::cout<<"CAPTURE replay steps="<<steps<<" submissions="<<submissions<<" peak_position_error="<<peakError<<" vertex_error="<<vertexError<<" m\n";
    require(peakError<.005f && vertexError<1e-4f,"captured motion/submission agrees with reconstructed physical inputs");return peakError;
}
static void captureFileChecks(const std::string &path) {std::ifstream f(path,std::ios::binary|std::ios::ate);require(f.good()&&f.tellg()>0&&f.tellg()<=static_cast<std::streamoff>(CaptureLimit),"capture file bounded before allocation");const auto size=static_cast<size_t>(f.tellg());f.seekg(0);std::vector<byte> bytes(size);require(!!f.read(reinterpret_cast<char*>(bytes.data()),size),"complete capture file read");replayCapture(bytes);}
