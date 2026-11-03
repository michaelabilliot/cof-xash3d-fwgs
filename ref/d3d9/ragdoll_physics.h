#pragma once
#include <btBulletDynamicsCommon.h>
#include <array>
#include <memory>
#include <vector>

namespace ragdoll {
constexpr int MaxBones = 128, MaxBodies = 64;
constexpr size_t MaxTriangles = 250000;
constexpr float UnitsToMeters = 0.0254f;
enum class Region { Unknown, Pelvis, Torso, Neck, Head, Shoulder, UpperArm, Forearm, Hand, Thigh, Calf, Foot, Accessory, Count };
Region region(const char *name, size_t length, int group);
float shotBudget(int weapon,int pellets,float scale=1);
struct Bone {
    int parent = -1, group = 0;
    btTransform pose = btTransform::getIdentity(); // world, metres
    btVector3 minimum{0,0,0}, maximum{0,0,0}; // bone local, metres
    bool collision = false;
    Region region = Region::Unknown;
};
struct Part {
    int bone = -1;
    int group=0;
    Region region=Region::Unknown;
    btTransform offset;
    btVector3 minimum{0,0,0},maximum{0,0,0};
    std::vector<std::unique_ptr<btCollisionShape>> childShapes;
    std::vector<int> childBones;
    std::vector<btTransform> childTransforms;
    std::vector<bool> childEnabled;
    std::unique_ptr<btCollisionShape> shape;
    std::unique_ptr<btRigidBody> body;
};
struct ImpactMuscle {
    btQuaternion rest{0,0,0,1};
    btVector3 target{0,0,0};
    float referenceAngle=0;
};
struct ImpactTransition {
    bool active=false;
    unsigned shot=0;
    int sequence=-1;
    float peakTime=0,elapsed=0,work=0,budget=0;
    std::array<ImpactMuscle,MaxBodies> muscles{};
};
struct Doll {
    Doll() {parents.fill(-1);partForBone.fill(-1);for(int i=0;i<MaxBones;++i){local[i].setIdentity();frozenPose[i].setIdentity();restPose[i].setIdentity();stationaryPose[i].setIdentity();restBodyPose[i].setIdentity();}}
    int count = 0;
    std::array<int, MaxBones> parents{}, partForBone{};
    std::array<btTransform, MaxBones> local{}, frozenPose{};
    std::vector<Part> parts;
    std::vector<Bone> collisionBones;
    std::array<int,MaxBones> ownerForBone{};
    std::array<bool,MaxBones> visibleBones{};
    std::vector<std::unique_ptr<btTypedConstraint>> joints;
    ImpactTransition transition;
    bool frozen = false;
    float initialPenetration=0;
    unsigned velocityCorrections=0;
    unsigned linearCorrections=0,angularCorrections=0;
    float currentSpeed=0,currentSpin=0,restPenetration=0,selfPenetration=0;
    unsigned selfContacts=0,restBlockers=0;
    unsigned wakeCount=0;
    float maximumSpeed=0;
    float maximumAngularSpeed=0,restEnergy=0,restJointError=0;
    bool supported=false;
    bool settled=false;
    float quietTime=0,stationaryTime=0,stationaryEnergy=0,age=0,sinceImpact=0;
    unsigned settleCount=0,lastCorrections=0;
    std::array<btTransform,MaxBones> restPose{},stationaryPose{},restBodyPose{};
};
inline void jointFrames(const btTypedConstraint &joint,btTransform &a,btTransform &b) {
    if(joint.getConstraintType()==HINGE_CONSTRAINT_TYPE) {const auto &h=static_cast<const btHingeConstraint&>(joint);a=h.getAFrame();b=h.getBFrame();}
    else {const auto &c=static_cast<const btConeTwistConstraint&>(joint);a=c.getAFrame();b=c.getBFrame();}
}
enum class ImpactRejection { None=0, Retired, InvalidInput, InvalidSourceBone, NoCollider, NoSurvivingAncestor, Disabled };
struct ImpactResult {
    int bone=-1,sourceBone=-1,receptorBone=-1;
    bool sourceVisible=false,remapped=false;
    ImpactRejection rejected=ImpactRejection::None;
    float requested=0,impulse=0,lever=0,deltaSpeed=0,deltaSpin=0;
    btVector3 momentum{0,0,0};
};
struct Mesh {
    std::unique_ptr<btTriangleMesh> triangles;
    std::unique_ptr<btBvhTriangleMeshShape> shape;
    std::unique_ptr<btRigidBody> body;
    btTransform target=btTransform::getIdentity();
    bool moving=false, initialized=false;
};
struct ContactObservation {
    int bone=-1,otherBone=-1,kind=0;
    float distance=0,impulse=0,point[3]{},normal[3]{};
};
class World {
    btDefaultCollisionConfiguration configuration;
    btCollisionDispatcher dispatcher{&configuration};
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;
    btDiscreteDynamicsWorld dynamics{&dispatcher, &broadphase, &solver, &configuration};
    double accumulator = 0;
    size_t triangleCount = 0;
    void updateSleep(float elapsed);
    void stepTransition(Doll &doll,float elapsed);
public:
    World();
    ~World();
    std::vector<std::unique_ptr<Mesh>> meshes;
    std::vector<std::unique_ptr<Doll>> dolls;
    Mesh *addMesh(const std::vector<btVector3> &vertices, bool moving);
    void moveMesh(Mesh &mesh, const btTransform &transform);
    void updateVisibility(Doll &doll,const std::array<bool,MaxBones> &visible);
    Doll *create(const std::vector<Bone> &bones, const btVector3 &velocity,
        const std::vector<btTransform> *previous = nullptr, float elapsed = 0,
        const std::vector<btTransform> *lastLiving = nullptr,const std::vector<btVector3> *reactionAngular = nullptr);
    ImpactResult impact(Doll &doll,const btVector3 &point,const btVector3 &direction,int group,float strength=0,int sourceBone=-1);
    bool beginTransition(Doll &doll,unsigned shot,int sequence,float peakTime,const std::array<float,MaxBones> &angles);
    void transitionHit(Doll &doll,const ImpactResult &hit,const btVector3 &point,const btVector3 &direction,float fraction);
    void pose(const Doll &doll, std::array<btTransform, MaxBones> &out, bool interpolate = true) const;
    void freeze(Doll &doll);
    void remove(Doll *doll);
    void step(double elapsed, float gravity);
    double phase() const {return accumulator;}
    void restorePhase(double value) {if(value>=0 && value<1.0/120) accumulator=value;}
    void refresh(Doll &doll) {for(auto &p:doll.parts) dynamics.updateSingleAabb(p.body.get());dynamics.performDiscreteCollisionDetection();}
    std::vector<ContactObservation> contacts(const Doll &doll,size_t limit=128) const;
    bool sleeping=true;
    void wake(Doll &doll);
    bool safeToRetire(const Doll &doll) const {return doll.settled || doll.frozen;}
};
int hitBone(const std::vector<Bone> &bones,const btVector3 &point,int group,int sourceBone=-1);
struct ReactionBone {btVector3 angle{0,0,0}, angular{0,0,0}, shift{0,0,0}, velocity{0,0,0};};
class Reactions {
public:
    std::array<ReactionBone,MaxBones> bones{};
    void step(float seconds);
    void hit(const std::vector<Bone> &skeleton,int bone,const btVector3 &point,const btVector3 &direction,float impulse);
    bool active() const;
    void apply(const std::vector<Bone> &skeleton,std::vector<btTransform> &out) const;
};
bool finite(const btVector3 &v);
bool valid(const btTransform &t);
}
