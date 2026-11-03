// Debug-only value stream. Four rotating files, <=2 MiB each; no native or
// Bullet object bytes/pointers. Incomplete scenes are explicitly marked.
enum { CaptureInitial=1,CaptureDoll=2,CaptureMesh=3,CaptureImpact=4,CaptureStep=5,CaptureSubmit=6,CaptureEnd=7 };
struct CaptureChunk {unsigned type,bytes;};
#pragma pack(push,1)
struct CaptureInitialData {
    unsigned magic=0x33434452,version=3,retainedBytes=sizeof(RetainedDoll);
    int index=0,serial=0,generation=0,olderCount=0,historyCount=0;
    double time=0,olderTime=0,historyTime=0;
    D3DMATRIX world{};
    ref_ragdoll_entity_t native{};
};
struct CaptureDollData {int index,bytes;};
struct CaptureMeshData {int id,moving,flags,vertices;RetainedTransform world,target;};
struct CaptureImpactData {int index,source,group;unsigned event,shot;float point[3],direction[3],impulse,fraction;};
struct CaptureStepData {double time,elapsed;float gravity;int index,parts,moving,contacts,supported;float penetration,selfDepth,jointError,energy,muscleWork,muscleBudget;};
struct CaptureMeshPose {int id,flags;RetainedTransform world,target;};
struct CaptureSubmitData {double time;int index,count,vertices;D3DMATRIX world;};
struct CaptureVertex {int bone;float scale;float local[3],world[3];};
struct CaptureEndData {int complete,frames;float muscleWork,muscleBudget;};
#pragma pack(pop)
struct CaptureRecord {size_t meshTotal=0;int index=0,serial=0,slot=0,frames=0;bool complete=true;double start=0;btVector3 center{0,0,0};std::vector<byte> data;std::vector<int> meshes,dolls;};
std::vector<CaptureRecord> captures;unsigned captureSlots=0;
constexpr size_t CaptureLimit=2u*1024u*1024u;
template<class T> void captureValue(std::vector<byte> &out,const T &v) {const auto *p=reinterpret_cast<const byte*>(&v);out.insert(out.end(),p,p+sizeof(v));}
bool captureChunk(CaptureRecord &c,unsigned type,const std::vector<byte> &bytes) {
    if(c.data.size()+bytes.size()+sizeof(CaptureChunk)>CaptureLimit-128) {c.complete=false;return false;}
    captureValue(c.data,CaptureChunk{type,static_cast<unsigned>(bytes.size())});c.data.insert(c.data.end(),bytes.begin(),bytes.end());return true;
}
void captureDoll(CaptureRecord &c,const Entry &e) {if(std::find(c.dolls.begin(),c.dolls.end(),e.index)!=c.dolls.end()){c.complete=false;return;}auto d=retainedData(e);if(!d)return;std::vector<byte> bytes;captureValue(bytes,CaptureDollData{e.index,sizeof(*d)});captureValue(bytes,*d);if(captureChunk(c,CaptureDoll,bytes))c.dolls.push_back(e.index);}
void finishCapture(CaptureRecord &c,const Entry *entry=nullptr) {
    std::vector<byte> bytes;CaptureEndData end{c.complete?1:0,c.frames,0,0};if(entry&&entry->doll){end.muscleWork=entry->doll->transition.work;end.muscleBudget=entry->doll->transition.budget;}captureValue(bytes,end);
    captureValue(c.data,CaptureChunk{CaptureEnd,static_cast<unsigned>(bytes.size())});c.data.insert(c.data.end(),bytes.begin(),bytes.end());
    char path[80];sprintf_s(path,"ragdoll_capture/slot-%u.rdc",static_cast<unsigned>(c.slot));
    const bool saved=gEngfuncs.fsapi && gEngfuncs.fsapi->WriteFile && gEngfuncs.fsapi->WriteFile(path,c.data.data(),static_cast<fs_offset_t>(c.data.size()));
    if(gEngfuncs.Con_Printf)gEngfuncs.Con_Printf("RAGDOLL capture: entity=%d path=%s bytes=%u complete=%d saved=%d\n",c.index,path,static_cast<unsigned>(c.data.size()),c.complete,saved);
    c.index=0;c.data.clear();
}
void beginCapture(const Entry &e,const ref_ragdoll_entity_t &state,const D3DMATRIX &world,double time) {
    // Other captures must know about a new neighbouring island before its J.
    for(auto &c:captures)if(c.index && e.doll && (e.doll->parts[0].body->getCenterOfMassPosition()-c.center).length2()<64)captureDoll(c,e);
    if(debug.value<3 || captureSlots>=4 || !e.doll || !gEngfuncs.fsapi || !gEngfuncs.fsapi->WriteFile)return;
    captures.emplace_back();auto &c=captures.back();c.index=e.index;c.serial=e.serial;c.slot=captureSlots++;c.start=time;c.center=e.doll->parts[0].body->getCenterOfMassPosition();c.data.reserve(CaptureLimit);c.meshTotal=physics->meshes.size();
    CaptureInitialData initial;initial.index=e.index;initial.serial=e.serial;initial.generation=state.generation;initial.time=time;initial.world=world;initial.native=state;
    initial.olderTime=e.olderTime;initial.historyTime=e.historyTime;initial.olderCount=static_cast<int>(e.older.size());initial.historyCount=static_cast<int>(e.history.size());
    std::vector<byte> bytes;captureValue(bytes,initial);for(const auto &p:e.older)captureValue(bytes,pack(p));for(const auto &p:e.history)captureValue(bytes,pack(p));captureChunk(c,CaptureInitial,bytes);
    struct Triangles final:btTriangleCallback {struct Triangle {int part,index;std::array<btVector3,3> v;};std::vector<Triangle> ordered;std::vector<float> values;bool truncated=false;
        void processTriangle(btVector3 *v,int part,int index) override {if(ordered.size()>=10922){truncated=true;return;}ordered.push_back({part,index,{v[0],v[1],v[2]}});}
        void sort(){std::sort(ordered.begin(),ordered.end(),[](const Triangle &a,const Triangle &b){return a.part!=b.part?a.part<b.part:a.index<b.index;});for(const auto &t:ordered)for(const auto &v:t.v)for(int j=0;j<3;++j)values.push_back(v[j]);}
    };
    for(size_t i=0;i<physics->meshes.size();++i) {
        const auto &m=*physics->meshes[i];if(m.body->getCollisionFlags()&btCollisionObject::CF_NO_CONTACT_RESPONSE)continue;
        const auto inverse=m.body->getWorldTransform().inverse();btVector3 lo(BT_LARGE_FLOAT,BT_LARGE_FLOAT,BT_LARGE_FLOAT),hi=-lo;
        for(int corner=0;corner<8;++corner){btVector3 p=c.center;for(int j=0;j<3;++j)p[j]+=(corner&(1<<j))?8.f:-8.f;p=inverse*p;lo.setMin(p);hi.setMax(p);}
        if(m.moving)m.shape->getAabb(btTransform::getIdentity(),lo,hi);
        Triangles triangles;m.shape->processAllTriangles(&triangles,lo,hi);triangles.sort();if(triangles.values.empty())continue;
        bytes.clear();captureValue(bytes,CaptureMeshData{static_cast<int>(i),m.moving,m.body->getCollisionFlags(),static_cast<int>(triangles.values.size()/3),pack(m.body->getWorldTransform()),pack(m.target)});
        for(float v:triangles.values)captureValue(bytes,v);
        if(c.data.size()+bytes.size()>1024u*1024u || !captureChunk(c,CaptureMesh,bytes)){c.complete=false;break;}
        c.complete&=!triangles.truncated;c.meshes.push_back(static_cast<int>(i));
    }
    for(const auto &other:entries)if(other.index!=e.index && other.doll && (other.doll->parts[0].body->getCenterOfMassPosition()-c.center).length2()<64){captureDoll(c,other);if(!other.doll->settled&&!other.doll->frozen)c.complete=false;}
    captureDoll(c,e);
}
void captureImpact(const Entry &e,const ref_ragdoll_hit_t &hit,const btVector3 &point,float impulse) {
    for(auto &c:captures)if(c.index && std::find(c.dolls.begin(),c.dolls.end(),e.index)!=c.dolls.end()) {CaptureImpactData event{};event.index=e.index;event.source=hit.bone-1;event.group=hit.group;event.event=hit.id;event.shot=hit.shot;event.impulse=impulse;event.fraction=impulse/8.f;
        for(int j=0;j<3;++j){event.point[j]=point[j];event.direction[j]=hit.direction[j];}std::vector<byte> bytes;captureValue(bytes,event);captureChunk(c,CaptureImpact,bytes);}
}
void captureSteps(double time,double elapsed,float gravity) {
    for(auto &c:captures)if(c.index) {
        const auto it=std::find_if(entries.begin(),entries.end(),[&](const Entry &e){return e.index==c.index&&e.serial==c.serial&&e.doll;});
        if(it==entries.end()){c.complete=false;finishCapture(c);continue;}
        if(elapsed<=0)continue;const auto &d=*it->doll;if(c.meshTotal!=physics->meshes.size())c.complete=false;
        for(const auto &other:entries)if(other.doll && (other.doll->parts[0].body->getCenterOfMassPosition()-c.center).length2()<64 && std::find(c.dolls.begin(),c.dolls.end(),other.index)==c.dolls.end())c.complete=false;
        for(size_t id=0;id<physics->meshes.size();++id)if(physics->meshes[id]->moving && !(physics->meshes[id]->body->getCollisionFlags()&btCollisionObject::CF_NO_CONTACT_RESPONSE) && std::find(c.meshes.begin(),c.meshes.end(),static_cast<int>(id))==c.meshes.end())c.complete=false;
        std::vector<byte> bytes;int moving=0;for(int id:c.meshes)if(physics->meshes[id]->moving)++moving;
        const auto contacts=physics->contacts(d);
        captureValue(bytes,CaptureStepData{time,elapsed,gravity,c.index,static_cast<int>(d.parts.size()),moving,static_cast<int>(contacts.size()),d.supported,d.restPenetration,d.selfPenetration,d.restJointError,d.restEnergy,d.transition.work,d.transition.budget});
        for(const auto &p:d.parts){RetainedPart r{};r.bone=p.bone;r.flags=p.body->getCollisionFlags();r.solved=pack(p.body->getWorldTransform());r.previous=pack(p.body->getInterpolationWorldTransform());for(int j=0;j<3;++j){r.linear[j]=p.body->getLinearVelocity()[j];r.angular[j]=p.body->getAngularVelocity()[j];}captureValue(bytes,r);if((p.body->getCenterOfMassPosition()-c.center).length2()>36)c.complete=false;}
        for(const auto &contact:contacts)captureValue(bytes,contact);
        for(int id:c.meshes)if(physics->meshes[id]->moving){const auto &m=*physics->meshes[id];captureValue(bytes,CaptureMeshPose{id,m.body->getCollisionFlags(),pack(m.body->getWorldTransform()),pack(m.target)});}
        if(!captureChunk(c,CaptureStep,bytes)){finishCapture(c,&*it);continue;}++c.frames;
        if(time-c.start>=1 || c.frames>=128)finishCapture(c,&*it);
    }
}
void captureSubmitted(const cl_entity_t *ent,const studio_cache_t &cache,const BoneMat34 *bones,int count,const D3DMATRIX &world) {
    for(auto &c:captures)if(c.index==ent->index) {
        std::vector<CaptureVertex> vertices;for(const auto &batch:cache.batches)for(const auto &v:batch.sources)if(vertices.size()<16 && v.bone<count){CaptureVertex vertex{};vertex.bone=v.bone;vertex.local[0]=v.x;vertex.local[1]=v.y;vertex.local[2]=v.z;float scale;const auto b=boneWorld(bones[v.bone],world,scale);const auto p=b*(btVector3(v.x,v.y,v.z)*(ragdoll::UnitsToMeters*scale));for(int j=0;j<3;++j)vertex.world[j]=p[j];vertices.push_back(vertex);}
        for(auto &v:vertices){float scale;boneWorld(bones[v.bone],world,scale);v.scale=scale;}
        std::vector<byte> bytes;captureValue(bytes,CaptureSubmitData{gp_cl?gp_cl->time:0,ent->index,count,static_cast<int>(vertices.size()),world});for(int i=0;i<count;++i){float scale;captureValue(bytes,pack(boneWorld(bones[i],world,scale)));}for(const auto &v:vertices)captureValue(bytes,v);captureChunk(c,CaptureSubmit,bytes);
    }
}
