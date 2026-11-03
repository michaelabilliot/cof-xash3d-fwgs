#include "ragdoll_physics.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <cctype>

namespace ragdoll {
Region region(const char *name,size_t length,int group) {
    std::string n;for(size_t i=0;i<length && name[i];++i) n.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(name[i]))));
    auto has=[&](const char *s){return n.find(s)!=std::string::npos;};
    if(has("finger") || has("toe") || has("weapon") || has("chainsaw")) return Region::Accessory;
    if(has("pelvis") || has("hips")) return Region::Pelvis;
    if(has("head")) return Region::Head;
    if(has("neck")) return Region::Neck;
    if(has("spine") || has("chest") || has("torso")) return Region::Torso;
    if(has("foot") || has("ankle")) return Region::Foot;
    if(has("hand") || has("wrist")) return Region::Hand;
    if(has("leg1") || has("calf") || has("shin")) return Region::Calf;
    if(has("leg") || has("thigh")) return Region::Thigh;
    if(has("arm2") || has("forearm") || has("lowerarm")) return Region::Forearm;
    if(has("arm1") || has("upperarm")) return Region::UpperArm;
    if(has("clav") || has("shoulder") || has(" arm")) return Region::Shoulder;
    if(group==1) return Region::Head;
    if(group==2 || group==3) return Region::Torso;
    return Region::Unknown;
}
static btVector3 bounded(btVector3 value,float speed) {
    if(!finite(value)) return btVector3(0,0,0);
    const float length=value.length();return length>speed?value*(speed/length):value;
}
bool finite(const btVector3 &v) {
    return std::isfinite(v.x()) && std::isfinite(v.y()) && std::isfinite(v.z());
}
bool valid(const btTransform &t) {
    const btMatrix3x3 &b = t.getBasis();
    return finite(t.getOrigin()) && finite(b[0]) && finite(b[1]) && finite(b[2]) &&
        btFabs(b.determinant()-1.f) < .01f &&
        btFabs(b[0].length2()-1.f)<.01f && btFabs(b[1].length2()-1.f)<.01f &&
        btFabs(b[2].length2()-1.f)<.01f;
}
World::World() {
    dynamics.getSolverInfo().m_numIterations = 24;
    dynamics.getSolverInfo().m_splitImpulse = true;
    // Route even shallow spawn penetration through positional recovery, rather
    // than turning a few centimetres of overlap into upward physical velocity.
    dynamics.getSolverInfo().m_splitImpulsePenetrationThreshold = 0;
    dynamics.getSolverInfo().m_erp2 = .12f;
    dynamics.getSolverInfo().m_maxErrorReduction = 2;
    dynamics.setGravity(btVector3(0,0,-800*UnitsToMeters));
}
World::~World() {
    while (!dolls.empty()) remove(dolls.back().get());
    for (auto &mesh : meshes) dynamics.removeRigidBody(mesh->body.get());
    meshes.clear();
}
Mesh *World::addMesh(const std::vector<btVector3> &vertices, bool moving) {
    if (vertices.empty() || vertices.size()%3 || vertices.size()/3>MaxTriangles-triangleCount) return nullptr;
    auto mesh = std::make_unique<Mesh>();
    mesh->moving=moving;
    mesh->triangles = std::make_unique<btTriangleMesh>(true, false);
    for (size_t i=0; i<vertices.size(); i+=3) {
        if (!finite(vertices[i]) || !finite(vertices[i+1]) || !finite(vertices[i+2])) return nullptr;
        if ((vertices[i+1]-vertices[i]).cross(vertices[i+2]-vertices[i]).length2()<1e-12f) continue;
        mesh->triangles->addTriangle(vertices[i],vertices[i+1],vertices[i+2],false);
    }
    if (!mesh->triangles->getNumTriangles()) return nullptr;
    mesh->shape = std::make_unique<btBvhTriangleMeshShape>(mesh->triangles.get(),true);
    mesh->shape->setMargin(.008f);
    btRigidBody::btRigidBodyConstructionInfo info(0,nullptr,mesh->shape.get());
    mesh->body = std::make_unique<btRigidBody>(info);
    mesh->body->setFriction(.85f);
    if (moving) {
        mesh->body->setCollisionFlags(btCollisionObject::CF_KINEMATIC_OBJECT);
        mesh->body->setActivationState(DISABLE_DEACTIVATION);
    }
    dynamics.addRigidBody(mesh->body.get(),1,2);
    triangleCount+=static_cast<size_t>(mesh->triangles->getNumTriangles());
    Mesh *result=mesh.get(); meshes.push_back(std::move(mesh)); return result;
}
void World::moveMesh(Mesh &mesh,const btTransform &transform) {
    if (!valid(transform)) return;
    mesh.target=transform;
    if(!mesh.initialized || (transform.getOrigin()-mesh.body->getWorldTransform().getOrigin()).length2()>100) {
        mesh.body->setWorldTransform(transform);
        mesh.body->setInterpolationWorldTransform(transform);
        mesh.initialized=true;
        dynamics.updateSingleAabb(mesh.body.get());
    }
}
Doll *World::create(const std::vector<Bone> &bones,const btVector3 &velocity,
    const std::vector<btTransform> *previous,float elapsed,const std::vector<btTransform> *lastLiving,const std::vector<btVector3> *reactionAngular) {
    if (bones.size()<3 || bones.size()>MaxBones || !finite(velocity)) return nullptr;
    int selected=0;
    for (size_t i=0;i<bones.size();++i) {
        const Bone &b=bones[i];
        if (b.parent<-1 || b.parent>=static_cast<int>(i) || !valid(b.pose) || static_cast<size_t>(b.region)>=static_cast<size_t>(Region::Count)) return nullptr;
        if (b.collision && (!finite(b.minimum) || !finite(b.maximum) ||
            b.maximum.x()<=b.minimum.x() || b.maximum.y()<=b.minimum.y() ||
            b.maximum.z()<=b.minimum.z())) return nullptr;
        if(b.collision) {const btVector3 size=b.maximum-b.minimum;if(!finite(size) || size[size.maxAxis()]>6.f) return nullptr;}
        if (b.collision) ++selected;
    }
    if (selected<3 || selected>MaxBodies) return nullptr;
    auto doll=std::make_unique<Doll>(); doll->count=static_cast<int>(bones.size());
    doll->partForBone.fill(-1);
    doll->parts.reserve(selected);
    std::array<float,static_cast<size_t>(Region::Count)> volumes{};
    std::vector<float> masses(bones.size(),0);
    // Weights are allocated per anatomical region, not per hitbox. Adding four
    // spine boxes must not make the torso four times heavier or each one tiny.
    static constexpr float weights[]={5,18,30,1,5,2,5,3,1,18,9,3,.5f};
    for(const auto &b:bones) if(b.collision) {
        btVector3 size=b.maximum-b.minimum;volumes[static_cast<size_t>(b.region)]+=size.x()*size.y()*size.z();
    }
    float sumWeights=0;for(size_t i=0;i<volumes.size();++i) if(volumes[i]>0) sumWeights+=weights[i];
    if (!std::isfinite(sumWeights) || sumWeights<=0) return nullptr;
    for(size_t i=0;i<bones.size();++i) if(bones[i].collision) {
        const auto &b=bones[i];const auto s=b.maximum-b.minimum;const size_t r=static_cast<size_t>(b.region);
        masses[i]=65.f*weights[r]/sumWeights*(s.x()*s.y()*s.z())/volumes[r];
        masses[i]=std::max(masses[i],.08f);
    }
    float totalMass=0;for(float mass:masses) totalMass+=mass;
    for(float &mass:masses) mass*=65.f/totalMass;
    // Studio damage hitboxes are collision pieces, not independent joints.
    // A humanoid chest/clavicles/neck form one rigid load path; hands and feet
    // follow the forearm/calf. Keep every source shape, its capture-relative
    // transform and allocated mass in a compound body. The head, hips,
    // shoulders (upper arms), elbows and knees retain articulated limits.
    // Generic/non-humanoid and independent-root rigs keep their old graph.
    doll->collisionBones=bones;doll->ownerForBone.fill(-1);doll->visibleBones.fill(true);
    bool head=false,pelvis=false;int thighs=0;
    // Anatomical identity comes from the skeleton, not currently visible
    // damage meshes. Losing the head must not ungroup the torso and wrists.
    for(const auto &bone:bones) {head|=bone.region==Region::Head;pelvis|=bone.region==Region::Pelvis;if(bone.region==Region::Thigh)++thighs;}
    const bool humanoid=head && pelvis && thighs>=2;
    for(int i=0;i<doll->count;++i) if(bones[i].collision) {
        int owner=i;
        if(humanoid && (bones[i].region==Region::Torso || bones[i].region==Region::Neck)) {
            int parent=bones[i].parent;
            while(parent>=0 && !bones[parent].collision) parent=bones[parent].parent;
            if(parent>=0 && bones[parent].region==Region::Torso) owner=doll->ownerForBone[parent];
        }
        if(humanoid && bones[i].region==Region::Shoulder) {
            int parent=bones[i].parent;while(parent>=0 && !bones[parent].collision) parent=bones[parent].parent;
            if(parent>=0 && (bones[parent].region==Region::Torso || bones[parent].region==Region::Neck)) owner=doll->ownerForBone[parent];
        }
        if(humanoid && (bones[i].region==Region::Hand || bones[i].region==Region::Foot)) {
            int parent=bones[i].parent;while(parent>=0 && !bones[parent].collision) parent=bones[parent].parent;
            if(parent>=0 && ((bones[i].region==Region::Hand && bones[parent].region==Region::Forearm) || (bones[i].region==Region::Foot && bones[parent].region==Region::Calf))) owner=doll->ownerForBone[parent];
        }
        doll->ownerForBone[i]=owner;
    }
    const auto colliderMasses=masses;
    for(int i=0;i<doll->count;++i) if(doll->ownerForBone[i]>=0 && doll->ownerForBone[i]!=i) {
        masses[doll->ownerForBone[i]]+=masses[i];masses[i]=0;
    }
    auto collider=[](const Bone &bone) -> std::unique_ptr<btCollisionShape> {
        btVector3 half=(bone.maximum-bone.minimum)*.5f;half.setMax(btVector3(.018f,.018f,.018f));
        const bool rounded=bone.region==Region::Thigh || bone.region==Region::Calf || bone.region==Region::UpperArm || bone.region==Region::Forearm || bone.region==Region::Head;
        std::unique_ptr<btCollisionShape> shape;
        if(rounded) {
            const int axis=half.maxAxis();const float radius=std::min(half[(axis+1)%3],half[(axis+2)%3])*.9f;
            const float height=std::max(0.f,2*(half[axis]-radius));
            if(axis==0) shape=std::make_unique<btCapsuleShapeX>(radius,height);
            else if(axis==1) shape=std::make_unique<btCapsuleShape>(radius,height);
            else shape=std::make_unique<btCapsuleShapeZ>(radius,height);
        } else shape=std::make_unique<btBoxShape>(half);
        shape->setMargin(.008f);return shape;
    };
    for (int i=0;i<doll->count;++i) {
        const Bone &b=bones[i]; doll->parents[i]=b.parent;
        doll->local[i]= b.parent<0 ? b.pose : bones[b.parent].pose.inverse()*b.pose;
        if (!b.collision || doll->ownerForBone[i]!=i) continue;
        Part part; part.bone=i;part.group=b.group;part.region=b.region;part.offset.setIdentity();
        part.offset.setOrigin((b.minimum+b.maximum)*.5f);
        part.minimum=b.minimum;part.maximum=b.maximum;
        btVector3 half=(b.maximum-b.minimum)*.5f;
        half.setMax(btVector3(.018f,.018f,.018f));
        if(half.maxAxis()<0 || half[half.maxAxis()]>3.f) return nullptr;
        // Recenter at the allocated mass centroid without moving any shape.
        int children=0;btVector3 center(0,0,0);
        for(int j=0;j<doll->count;++j) if(doll->ownerForBone[j]==i) {++children;center+=(bones[j].pose*((bones[j].minimum+bones[j].maximum)*.5f))*colliderMasses[j];}
        if(children>1) {
            center/=masses[i];part.offset.setOrigin(b.pose.inverse()*center);
            const btTransform aggregate=b.pose*part.offset;
            auto compound=std::make_unique<btCompoundShape>();
            for(int j=0;j<doll->count;++j) if(doll->ownerForBone[j]==i) {
                auto shape=collider(bones[j]);btTransform local=btTransform::getIdentity();local.setOrigin((bones[j].minimum+bones[j].maximum)*.5f);
                const btTransform child=aggregate.inverse()*bones[j].pose*local;
                compound->addChildShape(child,shape.get());part.childBones.push_back(j);part.childTransforms.push_back(child);part.childEnabled.push_back(true);part.childShapes.push_back(std::move(shape));
            }
            part.shape=std::move(compound);btVector3 lo,hi;part.shape->getAabb(btTransform::getIdentity(),lo,hi);half=(hi-lo)*.5f;
        } else part.shape=collider(b);
        const float mass=masses[i];
        btVector3 inertia;part.shape->calculateLocalInertia(mass,inertia);
        btRigidBody::btRigidBodyConstructionInfo info(mass,nullptr,part.shape.get(),inertia);
        info.m_friction=.8f; info.m_restitution=.02f;
        // Capsules otherwise have effectively lossless rolling/spinning contact.
        // Clothing against BSP surfaces supplies resistance only while touching.
        info.m_rollingFriction=.025f;info.m_spinningFriction=.025f;
        info.m_linearDamping=.12f;info.m_angularDamping=.35f;
        part.body=std::make_unique<btRigidBody>(info);
        part.body->setUserIndex(i);
        const btTransform initial=b.pose*part.offset;
        part.body->setCenterOfMassTransform(initial);
        part.body->setInterpolationWorldTransform(initial);
        btVector3 linear(0,0,0);
        if(previous && lastLiving && previous->size()==bones.size() && lastLiving->size()==bones.size() && elapsed>=.008f && elapsed<=.1f && valid((*previous)[i]) && valid((*lastLiving)[i])) {
            // Both samples must precede death. A death sequence or model swap
            // is a pose discontinuity, never a source of physical momentum.
            btQuaternion delta=(*lastLiving)[i].getRotation()*(*previous)[i].getRotation().inverse();
            if(delta.w()<0) delta=btQuaternion(-delta.x(),-delta.y(),-delta.z(),-delta.w());
            const float angle=delta.getAngle();
            if(angle>.0001f) part.body->setAngularVelocity(delta.getAxis()*std::min(angle/elapsed,4.f));
        }
        if(reactionAngular && reactionAngular->size()==bones.size()) part.body->setAngularVelocity(bounded(part.body->getAngularVelocity()+(*reactionAngular)[i],6.f));
        part.body->setLinearVelocity(linear);
        part.body->setSleepingThresholds(.08f,.15f);
        part.body->setCcdMotionThreshold(half[half.minAxis()]*.5f);
        part.body->setCcdSweptSphereRadius(half[half.minAxis()]*.7f);
        doll->partForBone[i]=static_cast<int>(doll->parts.size());
        doll->parts.push_back(std::move(part));
    }
    // Join branches through their common ancestor even if it has no hitbox.
    // Each selected body's nearest selected ancestor owns its joint.
    int roots=0;
    std::array<int,MaxBones> physicalParents;physicalParents.fill(-1);
    for(const auto &part:doll->parts) {
        int parent=bones[part.bone].parent;
        while(parent>=0 && doll->partForBone[parent]<0) parent=bones[parent].parent;
        // Bip01 clavicles are visually parented through Neck. The physical
        // shoulder load belongs to the rib cage, not the small neck collider.
        if((part.region==Region::Shoulder || part.region==Region::UpperArm) && parent>=0 && bones[parent].region==Region::Neck) {
            int chest=bones[parent].parent;
            while(chest>=0 && (doll->partForBone[chest]<0 || bones[chest].region!=Region::Torso)) chest=bones[chest].parent;
            if(chest>=0) parent=chest;
        }
        physicalParents[part.bone]=parent;
    }
    std::array<btVector3,MaxBones> hingeAxes;for(auto &axis:hingeAxes) axis.setZero();
    for(auto &part:doll->parts) {
        const int parent=physicalParents[part.bone];
        if(parent<0) {++roots;continue;}
        Part &other=doll->parts[doll->partForBone[parent]];
        btTransform joint=bones[part.bone].pose;
        const int group=bones[part.bone].group;
        const Region role=bones[part.bone].region;
        const bool named=role!=Region::Unknown;
        const bool limb=role==Region::Thigh || role==Region::Calf || role==Region::UpperArm || role==Region::Forearm || role==Region::Hand || role==Region::Foot || (!named && group>=4 && group<=7);
        const bool distal=role==Region::Calf || role==Region::Forearm || (!named && limb && bones[parent].group==group);
        int grandparent=bones[parent].parent;
        while(grandparent>=0 && doll->partForBone[grandparent]<0) grandparent=bones[grandparent].parent;
        const bool terminal=role==Region::Hand || role==Region::Foot || (!named && distal && grandparent>=0 && bones[grandparent].group==group);
        if(distal && !terminal) {
            btVector3 x=bones[part.bone].pose*((bones[part.bone].minimum+bones[part.bone].maximum)*.5f)-joint.getOrigin();
            if(x.length2()<1e-8f) x=joint.getBasis().getColumn((bones[part.bone].maximum-bones[part.bone].minimum).maxAxis());
            x.normalize();
            btVector3 upper=joint.getOrigin()-bones[parent].pose.getOrigin();
            if(upper.length2()<1e-8f) upper=x;else upper.normalize();
            const float bend=btAcos(std::clamp(upper.dot(x),-1.f,1.f));
            btVector3 reference=joint.getBasis().getColumn(2);
            if(btFabs(x.dot(reference))>.9f) reference=joint.getBasis().getColumn(1);
            btVector3 z=upper.cross(x);
            if(z.length2()<1e-5f) z=x.cross(reference);
            z.normalize();const btVector3 y=z.cross(x).normalized();
            joint.setBasis(btMatrix3x3(x.x(),y.x(),z.x(),x.y(),y.y(),z.y(),x.z(),y.z(),z.z()));
            auto hinge=std::make_unique<btHingeConstraint>(*other.body,*part.body,
                other.body->getWorldTransform().inverse()*joint,
                part.body->getWorldTransform().inverse()*joint,true);
            const float maximumBend=std::max(role==Region::Calf?2.55f:2.7f,bend+.05f);
            hinge->setLimit(-bend-.08f,maximumBend-bend,.95f,.2f,1.f);
            hinge->setParam(BT_CONSTRAINT_ERP,.35f);
            hingeAxes[part.bone]=z;
            doll->joints.push_back(std::move(hinge));continue;
        }
        // ConeTwist's twist axis is X. Studio bones often run along local Z.
        // Align the frame to the actual segment instead of its naming basis.
        btVector3 axis=bones[part.bone].pose*((bones[part.bone].minimum+bones[part.bone].maximum)*.5f)-joint.getOrigin();
        if(axis.length2()<1e-8f) axis=joint.getOrigin()-bones[parent].pose.getOrigin();
        if(axis.length2()>1e-8f) {
            axis.normalize();btVector3 reference=joint.getBasis().getColumn(2);
            if(btFabs(axis.dot(reference))>.9f) reference=joint.getBasis().getColumn(1);
            const btVector3 z=axis.cross(reference).normalized(),y=z.cross(axis);
            joint.setBasis(btMatrix3x3(axis.x(),y.x(),z.x(),axis.y(),y.y(),z.y(),axis.z(),y.z(),z.z()));
        }
        auto constraint=std::make_unique<btConeTwistConstraint>(*other.body,*part.body,
            other.body->getWorldTransform().inverse()*joint,
            part.body->getWorldTransform().inverse()*joint);
        // Hit groups provide a naming-independent anatomical hint. Limits are
        // relative to the captured pose, never to a guessed universal bind pose.
        constraint->setLimit(terminal?.45f:(limb?1.15f:.45f),
            terminal?.45f:(limb?.9f:.45f),terminal?.25f:(limb?.65f:.35f),.9f,.3f,1.f);
        constraint->setDamping(.15f);
        constraint->setParam(BT_CONSTRAINT_ERP,.35f,0);
        doll->joints.push_back(std::move(constraint));
    }
    // CoF includes compound models with several independent skeleton roots
    // (e.g. a carrier and a victim). Never weld separate characters together.
    if(roots<1 || roots>4 || doll->joints.size()+roots!=doll->parts.size()) return nullptr;
    // A skeletal pose derivative is not necessarily constraint-compatible after
    // clipping individual velocities. Project onto equal pivot-point velocity
    // before stepping: vA + wA x rA == vB + wB x rB. This avoids an artificial
    // first-step constraint impulse while preserving pre-death limb rotation.
    btVector3 common=bounded(velocity,8.f);common.setZ(std::clamp(common.z(),-8.f,1.5f));
    for(auto &part:doll->parts) {
        const int parent=physicalParents[part.bone];
        if(parent<0) part.body->setLinearVelocity(common);
        else {
            const auto &a=*doll->parts[doll->partForBone[parent]].body;
            const auto &b=*part.body;const btVector3 joint=bones[part.bone].pose.getOrigin();
            const auto &axis=hingeAxes[part.bone];
            if(axis.length2()>.5f) part.body->setAngularVelocity(a.getAngularVelocity()+axis*(b.getAngularVelocity()-a.getAngularVelocity()).dot(axis));
            part.body->setLinearVelocity(a.getLinearVelocity()+a.getAngularVelocity().cross(joint-a.getCenterOfMassPosition())-
                b.getAngularVelocity().cross(joint-b.getCenterOfMassPosition()));
        }
    }
    btVector3 mean(0,0,0);float massSum=0;
    for(auto &p:doll->parts) {const float m=1/p.body->getInvMass();mean+=p.body->getLinearVelocity()*m;massSum+=m;}
    mean/=massSum;float energy=0;
    for(auto &p:doll->parts) {
        const btVector3 relative=p.body->getLinearVelocity()-mean,w=p.body->getAngularVelocity();
        energy+=.5f/p.body->getInvMass()*relative.length2()+.5f*w.dot(p.body->getInvInertiaTensorWorld().inverse()*w);
    }
    const float scale=energy>12.f?btSqrt(12.f/energy):1.f;
    for(auto &p:doll->parts) {
        p.body->setLinearVelocity(common+(p.body->getLinearVelocity()-mean)*scale);
        p.body->setAngularVelocity(p.body->getAngularVelocity()*scale);
    }
    // Hitboxes overlap around shoulders and spine segments. Ignore two-hop
    // neighbours too, while retaining hand/torso, foot/torso and cross-limb
    // contacts farther along the skeleton.
    auto parentPart=[&](int bone) {
        return physicalParents[bone];
    };
    struct InitialContact : btCollisionWorld::ContactResultCallback {
        bool overlapping=false;
        btScalar addSingleResult(btManifoldPoint &point,const btCollisionObjectWrapper*,int,int,
            const btCollisionObjectWrapper*,int,int) override {
            if(point.getDistance()<-.005f) overlapping=true;
            return 0;
        }
    };
    auto rootOf=[&](int bone) {while(bones[bone].parent>=0) bone=bones[bone].parent;return bone;};
    for(size_t i=0;i<doll->parts.size();++i) for(size_t j=i+1;j<doll->parts.size();++j) {
        auto &a=doll->parts[i];auto &b=doll->parts[j];
        const int pa=parentPart(a.bone),pb=parentPart(b.bone);
        bool ignore=(pa>=0 && pa==pb) || (pa>=0 && parentPart(pa)==b.bone) || (pb>=0 && parentPart(pb)==a.bone);
        if(!ignore && rootOf(a.bone)==rootOf(b.bone)) {
            btVector3 loA,hiA,loB,hiB;
            a.body->getAabb(loA,hiA);b.body->getAabb(loB,hiB);
            if(loA.x()<=hiB.x() && hiA.x()>=loB.x() && loA.y()<=hiB.y() && hiA.y()>=loB.y() && loA.z()<=hiB.z() && hiA.z()>=loB.z()) {
                InitialContact contact;dynamics.contactPairTest(a.body.get(),b.body.get(),contact);
                ignore=contact.overlapping;
            }
        }
        if(ignore) {
            a.body->setIgnoreCollisionCheck(b.body.get(),true);b.body->setIgnoreCollisionCheck(a.body.get(),true);
        }
    }
    struct SpawnContact : btCollisionWorld::ContactResultCallback {
        float depth=0;
        btScalar addSingleResult(btManifoldPoint &point,const btCollisionObjectWrapper*,int,int,
            const btCollisionObjectWrapper*,int,int) override {depth=std::max(depth,-point.getDistance());return 0;}
    };
    for(auto &p:doll->parts) for(auto &mesh:meshes) {
        if(mesh->body->getCollisionFlags()&btCollisionObject::CF_NO_CONTACT_RESPONSE) continue;
        SpawnContact contact;dynamics.contactPairTest(p.body.get(),mesh->body.get(),contact);
        doll->initialPenetration=std::max(doll->initialPenetration,contact.depth);
    }
    for(auto &p:doll->parts) {p.body->setUserPointer(doll.get());dynamics.addRigidBody(p.body.get(),2,3);}
    for(auto &j:doll->joints) dynamics.addConstraint(j.get(),true);
    Doll *result=doll.get(); dolls.push_back(std::move(doll)); return result;
}
void World::updateVisibility(Doll &doll,const std::array<bool,MaxBones> &visible) {
    doll.visibleBones=visible;
    for(auto &part:doll.parts) {
        bool active=visible[part.bone],changed=false;
        if(!part.childShapes.empty()) {
            active=false;
            for(size_t i=0;i<part.childBones.size();++i) {const bool show=visible[part.childBones[i]];active|=show;changed|=show!=part.childEnabled[i];part.childEnabled[i]=show;}
            if(changed && active) {
                auto &shape=*static_cast<btCompoundShape*>(part.shape.get());
                while(shape.getNumChildShapes()) shape.removeChildShapeByIndex(shape.getNumChildShapes()-1);
                for(size_t i=0;i<part.childBones.size();++i) if(part.childEnabled[i]) shape.addChildShape(part.childTransforms[i],part.childShapes[i].get());
                shape.recalculateLocalAabb();
            }
        }
        const int old=part.body->getCollisionFlags();
        const int flags=active?old&~btCollisionObject::CF_NO_CONTACT_RESPONSE:old|btCollisionObject::CF_NO_CONTACT_RESPONSE;
        part.body->setCollisionFlags(flags);
        if(changed || flags!=old) {dynamics.updateSingleAabb(part.body.get());if(!doll.settled) part.body->activate(true);}
    }
}
ImpactResult World::impact(Doll &doll,const btVector3 &point,const btVector3 &direction,int group,float strength,int sourceBone) {
    ImpactResult result;result.sourceBone=sourceBone;
    if(doll.frozen) {result.rejected=ImpactRejection::Retired;return result;}
    if(!finite(point) || !finite(direction) || direction.length2()<.5f || !std::isfinite(strength) || strength<0) {result.rejected=ImpactRejection::InvalidInput;return result;}
    if(sourceBone<-1 || sourceBone>=doll.count) {result.rejected=ImpactRejection::InvalidSourceBone;return result;}
    Part *selected=nullptr;btVector3 contact;
    std::vector<Bone> rig=doll.collisionBones;
    std::array<btTransform,MaxBones> palette;pose(doll,palette,false);
    for(int i=0;i<doll.count;++i) {
        rig[i].pose=palette[i];const int owner=doll.ownerForBone[i];
        const int part=owner>=0?doll.partForBone[owner]:-1;
        if(!doll.visibleBones[i] || part<0 || (doll.parts[part].body->getCollisionFlags()&btCollisionObject::CF_NO_CONTACT_RESPONSE)) rig[i].collision=false;
    }
    int mapped=-1;
    if(sourceBone>=0) {
        result.sourceVisible=rig[sourceBone].collision;
        if(result.sourceVisible) mapped=hitBone(rig,point,group,sourceBone);
        else {
            // The native trace identified this bone before a gore/body swap.
            // Keep that anatomical provenance; never choose a nearby other limb.
            const auto &source=rig[sourceBone];const btVector3 local=source.pose.inverse()*point;
            const btVector3 nearest(std::clamp(local.x(),source.minimum.x(),source.maximum.x()),std::clamp(local.y(),source.minimum.y(),source.maximum.y()),std::clamp(local.z(),source.minimum.z(),source.maximum.z()));
            if((local-nearest).length2()>.75f*.75f) {result.rejected=ImpactRejection::NoCollider;return result;}
            int ancestor=source.parent;
            for(int depth=0;ancestor>=0 && depth<doll.count;++depth) {
                if(rig[ancestor].collision) {mapped=ancestor;break;}
                ancestor=rig[ancestor].parent;
            }
            result.remapped=mapped>=0;
            if(mapped<0) {result.rejected=ImpactRejection::NoSurvivingAncestor;return result;}
        }
    } else mapped=hitBone(rig,point,group,-1);
    result.receptorBone=mapped;
    // Slower's hit groups are all zero: nearest actual collider remains valid.
    // Never infer a headshot from weapon damage or an arbitrary bone index.
    for(auto &part:doll.parts) {
        if(mapped<0 || part.bone!=doll.ownerForBone[mapped]) continue;
        btVector3 lo,hi;part.shape->getAabb(btTransform::getIdentity(),lo,hi);
        btVector3 local=part.body->getWorldTransform().inverse()*point;
        btVector3 closest(std::clamp(local.x(),lo.x(),hi.x()),std::clamp(local.y(),lo.y(),hi.y()),std::clamp(local.z(),lo.z(),hi.z()));
        const btVector3 world=part.body->getWorldTransform()*closest;
        selected=&part;contact=world;break;
    }
    if(!selected) {result.rejected=ImpactRejection::NoCollider;return result;}
    auto &body=*selected->body;
    wake(doll);doll.sinceImpact=0;
    btVector3 lever=bounded(contact-body.getCenterOfMassPosition(),.2f);
    float magnitude=strength>0?std::clamp(strength,0.f,8.f):(selected->region==Region::Head || group==1?3.f:4.5f);
    result.requested=magnitude;
    // Mass determines acceleration. Keep the bounded shot momentum instead
    // of silently making every light body/weapon hit the same velocity cap.
    btVector3 impulse=direction.normalized()*magnitude;
    const btVector3 angularDelta=body.getInvInertiaTensorWorld()*lever.cross(impulse);
    // Preserve the shot's bounded linear momentum. A small inertia must only
    // limit torque, not silently scale down the entire delivered impulse.
    btVector3 torque=lever.cross(impulse);
    if(angularDelta.length()>2.5f) torque*=2.5f/angularDelta.length();
    const auto beforeLinear=body.getLinearVelocity(),beforeAngular=body.getAngularVelocity();
    body.applyCentralImpulse(impulse);body.applyTorqueImpulse(torque);body.activate(true);
    result.deltaSpeed=(body.getLinearVelocity()-beforeLinear).length();result.deltaSpin=(body.getAngularVelocity()-beforeAngular).length();
    result.momentum=(body.getLinearVelocity()-beforeLinear)/body.getInvMass();
    result.bone=selected->bone;result.impulse=magnitude;result.lever=lever.length();return result;
}
bool World::beginTransition(Doll &doll,unsigned shot,int sequence,float peakTime,const std::array<float,MaxBones> &angles) {
    if(doll.frozen || doll.settled || !shot || sequence<0 || !std::isfinite(peakTime) || peakTime<1.f/120 || peakTime>.3f) return false;
    // A carrier/victim model contains independent characters. Its unrelated
    // roots keep their existing passive ownership, not one shared reflex.
    if(doll.parts.size()!=doll.joints.size()+1)return false;
    // Muscle work is meaningful only while the captured stance has support.
    // No world anchors, kinematic bodies or artificial anti-gravity forces.
    refresh(doll);bool supported=false;
    for(int i=0;i<dispatcher.getNumManifolds();++i) {
        const auto *m=dispatcher.getManifoldByIndexInternal(i);
        const bool a=m->getBody0()->getUserPointer()==&doll,b=m->getBody1()->getUserPointer()==&doll;
        if(a==b) continue;
        const auto *other=a?m->getBody1():m->getBody0();
        if(other->getCollisionFlags()&btCollisionObject::CF_NO_CONTACT_RESPONSE)continue;
        for(int j=0;j<m->getNumContacts();++j) {const auto &p=m->getContactPoint(j);if(p.getDistance()<.025f && (a?p.m_normalWorldOnB.z():-p.m_normalWorldOnB.z())>.45f)supported=true;}
    }
    if(!supported) return false;
    auto &t=doll.transition;t={};t.shot=shot;t.sequence=sequence;t.peakTime=peakTime;
    for(size_t i=0;i<doll.joints.size();++i) {
        const auto &j=*doll.joints[i];const auto &a=j.getRigidBodyA();const auto &b=j.getRigidBodyB();auto &m=t.muscles[i];
        m.rest=a.getWorldTransform().getRotation().inverse()*b.getWorldTransform().getRotation();
        const float native=angles[b.getUserIndex()];
        // Keep the native amplitude. The selected recoil axis determines
        // which joint limit applies; twist must not truncate pure swing.
        m.referenceAngle=std::isfinite(native)?std::clamp(native,0.f,SIMD_PI):0;
        const auto inv=a.getInvInertiaTensorWorld()+b.getInvInertiaTensorWorld();
        const float average=(inv[0][0]+inv[1][1]+inv[2][2])/3.f;
        // Bound work by the largest reachable native excursion. Directional
        // target clamping below can only reduce this conservative envelope.
        float reachable=0;
        if(j.getConstraintType()==HINGE_CONSTRAINT_TYPE) {const auto &h=static_cast<const btHingeConstraint&>(j);reachable=std::max(btFabs(h.getLowerLimit()),btFabs(h.getUpperLimit()));}
        else {const auto &c=static_cast<const btConeTwistConstraint&>(j);reachable=std::max({c.getSwingSpan1(),c.getSwingSpan2(),c.getTwistSpan()});}
        const float excursion=std::min(m.referenceAngle,.8f*reachable);
        if(average>1e-6f)t.budget+=.5f/average*btPow(SIMD_PI*excursion/peakTime,2);
    }
    t.active=std::isfinite(t.budget)&&t.budget>0;doll.supported=true;return t.active;
}
void World::transitionHit(Doll &doll,const ImpactResult &hit,const btVector3 &point,const btVector3 &direction,float fraction) {
    auto &t=doll.transition;
    if(!t.active || hit.bone<0 || !finite(point) || !finite(direction) || direction.length2()<.5f || !std::isfinite(fraction))return;
    // Reflex anatomy follows the traced region, not the compound collider owner.
    // The neck belongs to the torso collider, but its surviving head recoils.
    int receptor=hit.bone;
    if(hit.sourceBone>=0 && hit.sourceBone<doll.count && doll.visibleBones[hit.sourceBone]) {
        const auto region=doll.collisionBones[hit.sourceBone].region;
        const auto wanted=region==Region::Neck?Region::Head:region==Region::Shoulder?Region::UpperArm:Region::Unknown;
        if(wanted!=Region::Unknown)for(const auto &part:doll.parts)if(part.region==wanted && doll.visibleBones[part.bone]) {
            int ancestor=part.bone;
            for(int depth=0;ancestor>=0 && depth<doll.count;++depth) {
                if(ancestor==hit.sourceBone){receptor=part.bone;break;}
                ancestor=doll.parents[ancestor];
            }
            if(receptor==part.bone)break;
        }
    }
    if(fraction<=0)return;
    int proximal=-1;for(const auto &joint:doll.joints)if(joint->getRigidBodyB().getUserIndex()==receptor){proximal=joint->getRigidBodyA().getUserIndex();break;}
    for(size_t i=0;i<doll.joints.size();++i) {
        auto &j=*doll.joints[i];auto &a=j.getRigidBodyA();auto &b=j.getRigidBodyB();
        // Distal impacts also excite the proximal joint, without replaying J.
        float share=b.getUserIndex()==receptor?1.f:(b.getUserIndex()==proximal?.25f:0.f);
        if(share==0)continue;
        btTransform fa,fb;jointFrames(j,fa,fb);const auto pivot=a.getWorldTransform()*fa.getOrigin();
        // The reflex moves the receiving body along the ray. The original
        // contact remains exclusively the lever for the projectile impulse.
        btVector3 axis=(b.getCenterOfMassPosition()-pivot).cross(direction.normalized());
        if(axis.length2()<1e-8f)axis=btVector3(0,0,1).cross(direction);
        if(axis.length2()<1e-8f)continue;axis.normalize();
        if(j.getConstraintType()==HINGE_CONSTRAINT_TYPE) {const auto hinge=(a.getWorldTransform()*fa).getBasis().getColumn(2);axis=hinge*axis.dot(hinge);}
        auto &m=t.muscles[i];m.target+=a.getWorldTransform().getBasis().transpose()*axis*(m.referenceAngle*share);
        m.target=bounded(m.target,m.referenceAngle);
        // Existing safety margin, evaluated along the actual constraint axis.
        const auto local=fa.getBasis().transpose()*m.target;
        if(j.getConstraintType()==HINGE_CONSTRAINT_TYPE) {
            const auto &h=static_cast<const btHingeConstraint&>(j);
            const float sign=h.getUseReferenceFrameA()?1.f:-1.f;
            const float requested=local.z()*sign;
            const float allowed=.8f*(requested>=0?h.getUpperLimit():h.getLowerLimit());
            if(btFabs(requested)>btFabs(allowed) && btFabs(requested)>1e-8f)m.target*=btFabs(allowed/requested);
        } else {
            const auto &c=static_cast<const btConeTwistConstraint&>(j);
            const float x=local.x()/std::max(c.getTwistSpan(),1e-6f);
            const float y=local.y()/std::max(c.getSwingSpan2(),1e-6f);
            const float z=local.z()/std::max(c.getSwingSpan1(),1e-6f);
            const float extent=btSqrt(x*x+y*y+z*z);
            if(extent>.8f)m.target*=.8f/extent;
        }
    }
}
void World::stepTransition(Doll &doll,float dt) {
    auto &t=doll.transition;if(!t.active)return;
    if(t.elapsed>=2*t.peakTime || !doll.supported) {t.active=false;return;}
    t.elapsed=std::min(t.elapsed+dt,2*t.peakTime);
    const float phase=SIMD_PI*t.elapsed/(2*t.peakTime);
    const float envelope=btSin(phase)*btSin(phase);
    const float derivative=SIMD_PI/(2*t.peakTime)*btSin(2*phase);
    const float release=t.elapsed<=t.peakTime?1.f:std::max(0.f,2-t.elapsed/t.peakTime);
    const float response=1-btExp(-4*dt/t.peakTime);
    for(size_t i=0;i<doll.joints.size();++i) {
        auto &j=*doll.joints[i];auto &a=j.getRigidBodyA();auto &b=j.getRigidBodyB();const auto &m=t.muscles[i];
        if((a.getCollisionFlags()|b.getCollisionFlags())&btCollisionObject::CF_NO_CONTACT_RESPONSE)continue;
        const btQuaternion qa=a.getWorldTransform().getRotation(),qb=b.getWorldTransform().getRotation();
        const btVector3 target=m.target*envelope;const float angle=target.length();
        const btQuaternion desired=angle>1e-6f?btQuaternion(target/angle,angle)*m.rest:m.rest;
        btQuaternion error=qa*desired*qb.inverse();if(error.w()<0)error=btQuaternion(-error.x(),-error.y(),-error.z(),-error.w());
        const float theta=error.getAngleShortestPath();
        btVector3 velocity=(theta>1e-6f?error.getAxis()*(theta*4/t.peakTime):btVector3(0,0,0))+a.getWorldTransform().getBasis()*(m.target*derivative);
        const btVector3 relative=b.getAngularVelocity()-a.getAngularVelocity();
        velocity=bounded(velocity,4*std::max(m.referenceAngle,.08f)/t.peakTime);
        btVector3 delta=velocity-relative;
        if(j.getConstraintType()==HINGE_CONSTRAINT_TYPE) {btTransform fa,fb;jointFrames(j,fa,fb);const auto axis=(a.getWorldTransform()*fa).getBasis().getColumn(2);delta=axis*delta.dot(axis);}
        if(delta.length2()<1e-10f)continue;
        const auto inverse=a.getInvInertiaTensorWorld()+b.getInvInertiaTensorWorld();
        const auto axis=delta.normalized();const float inv=axis.dot(inverse*axis);if(inv<1e-8f)continue;
        btVector3 impulse=inverse.inverse()*delta*(response*release);
        const float quadratic=.5f*impulse.dot(inverse*impulse),linear=impulse.dot(relative);
        const float work=quadratic+linear,available=std::max(0.f,t.budget-t.work);
        if(work>available) {
            const float scale=quadratic>1e-12f?(-linear+btSqrt(std::max(0.f,linear*linear+4*quadratic*available)))/(2*quadratic):(linear>0?available/linear:0.f);
            impulse*=std::clamp(scale,0.f,1.f);
        }
        t.work+=std::max(0.f,.5f*impulse.dot(inverse*impulse)+impulse.dot(relative));
        a.applyTorqueImpulse(-impulse);b.applyTorqueImpulse(impulse);
    }
    if(t.elapsed>=2*t.peakTime)t.active=false;
}
void World::pose(const Doll &doll,std::array<btTransform,MaxBones> &out,bool interpolate) const {
    if(doll.frozen) out=doll.frozenPose;
    else if(doll.settled) out=doll.restPose;
    else {
        const float alpha=interpolate?static_cast<float>(accumulator*120):1.f;
        for(int i=0;i<doll.count;++i) {
            const int p=doll.partForBone[i];
            if(p>=0) {
                const Part &part=doll.parts[p];
                btTransform t=part.body->getWorldTransform();
                if(interpolate) {
                    const btTransform &before=part.body->getInterpolationWorldTransform();
                    t.setOrigin(before.getOrigin().lerp(t.getOrigin(),alpha));
                    t.setRotation(before.getRotation().slerp(t.getRotation(),alpha));
                }
                out[i]=t*part.offset.inverse();
            } else out[i]=doll.parents[i]<0?doll.local[i]:out[doll.parents[i]]*doll.local[i];
        }
    }
    // Constraint anchors have solver tolerance, especially in crowded corpse
    // contacts. Passing their independent translations to skinning stretches
    // the mesh. Keep physical rotations and free roots, but weld child origins
    // to their captured skeletal offsets, including retained/rest palettes.
    std::array<bool,MaxBones> rooted{};
    int reference=-1;
    for(int i=0;i<doll.count;++i) if(doll.partForBone[i]>=0) {
        int ancestor=doll.parents[i];
        while(ancestor>=0 && doll.partForBone[ancestor]<0) ancestor=doll.parents[ancestor];
        if(ancestor>=0) continue;
        if(reference<0) reference=i;
        // Studio dummy roots have no rigid body. Recover their frame from
        // the first physical descendant so their weighted/helper vertices
        // travel with the corpse rather than staying at the death position.
        btTransform frame=out[i];
        for(int child=i,parent=doll.parents[child];parent>=0;child=parent,parent=doll.parents[child]) {
            frame=frame*doll.local[child].inverse();
            if(!rooted[parent]) {out[parent]=frame;rooted[parent]=true;}
        }
    }
    if(reference>=0) {
        btTransform captured=doll.local[reference];
        for(int parent=doll.parents[reference];parent>=0;parent=doll.parents[parent])
            captured=doll.local[parent]*captured;
        const btTransform travel=out[reference]*captured.inverse();
        // Some models have separate unphysical prop/weapon trees. Carry
        // those with the body rather than leaving their vertices behind.
        for(int i=0;i<doll.count;++i)
            if(doll.parents[i]<0 && doll.partForBone[i]<0 && !rooted[i]) out[i]=travel*doll.local[i];
    }
    for(int i=0;i<doll.count;++i) {
        const int parent=doll.parents[i];
        if(parent<0) continue;
        if(doll.partForBone[i]<0) {
            out[i]=out[parent]*doll.local[i];
        } else {
            int ancestor=parent;
            while(ancestor>=0 && doll.partForBone[ancestor]<0) ancestor=doll.parents[ancestor];
            // A pelvis below a nonphysical studio root is still a free
            // physics root. Pinning it would suspend the corpse in midair.
            if(ancestor<0) continue;
            out[i].setOrigin(out[parent]*doll.local[i].getOrigin());
        }
    }
}
void World::freeze(Doll &doll) {
    if(doll.frozen) return;
    doll.transition.active=false;
    pose(doll,doll.frozenPose,true);
    for(auto &j:doll.joints) dynamics.removeConstraint(j.get());
    doll.joints.clear();
    // Retirement removes solver work, not the corpse's collision geometry.
    for(auto &p:doll.parts) {
        dynamics.removeRigidBody(p.body.get());
        p.body->setMassProps(0,btVector3(0,0,0));
        p.body->setLinearVelocity(btVector3(0,0,0));p.body->setAngularVelocity(btVector3(0,0,0));p.body->clearForces();
        p.body->setCollisionFlags((p.body->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT)|btCollisionObject::CF_STATIC_OBJECT);
        p.body->forceActivationState(ISLAND_SLEEPING);
        dynamics.addRigidBody(p.body.get(),1,2);
    }
    doll.frozen=true;
}
void World::remove(Doll *doll) {
    if(!doll) return;
    if(!doll->frozen) for(auto &j:doll->joints) dynamics.removeConstraint(j.get());
    for(auto &p:doll->parts) dynamics.removeRigidBody(p.body.get());
    dolls.erase(std::remove_if(dolls.begin(),dolls.end(),[doll](const auto &p){return p.get()==doll;}),dolls.end());
}
void World::wake(Doll &doll) {
    if(doll.frozen) return;
    if(doll.settled) ++doll.wakeCount;
    doll.settled=false;doll.quietTime=doll.stationaryTime=doll.stationaryEnergy=0;
    for(auto &p:doll.parts) {p.body->forceActivationState(ACTIVE_TAG);p.body->activate(true);}
}
std::vector<ContactObservation> World::contacts(const Doll &doll,size_t limit) const {
    std::vector<ContactObservation> out;limit=std::min(limit,size_t(128));
    for(int i=0;i<dispatcher.getNumManifolds() && out.size()<limit;++i) {
        const auto *m=dispatcher.getManifoldByIndexInternal(i);const auto *a=m->getBody0(),*b=m->getBody1();
        const bool oursA=a->getUserPointer()==&doll,oursB=b->getUserPointer()==&doll;if(!oursA&&!oursB)continue;
        const auto *ours=oursA?a:b,*other=oursA?b:a;
        for(int j=0;j<m->getNumContacts() && out.size()<limit;++j) {
            const auto &p=m->getContactPoint(j);ContactObservation c;c.bone=ours->getUserIndex();c.otherBone=other->getUserIndex();c.kind=oursA&&oursB?2:other->isStaticOrKinematicObject()?1:3;c.distance=p.getDistance();c.impulse=p.getAppliedImpulse();
            const auto point=oursA?p.getPositionWorldOnA():p.getPositionWorldOnB(),normal=oursA?p.m_normalWorldOnB:-p.m_normalWorldOnB;
            for(int k=0;k<3;++k){c.point[k]=point[k];c.normal[k]=normal[k];}out.push_back(c);
        }
    }
    return out;
}
void World::updateSleep(float elapsed) {
    for(auto &owned:dolls) {
        Doll &d=*owned;if(d.frozen) continue;
        d.age+=elapsed;d.sinceImpact+=elapsed;
        if(!sleeping) {
            if(d.settled) wake(d);
            for(auto &p:d.parts) p.body->forceActivationState(DISABLE_DEACTIVATION);
            d.quietTime=d.stationaryTime=0;continue;
        }
        float speed=0,spin=0,energy=0,penetration=0,jointError=0;
        bool supported=false,unstableSupport=false,externalImpact=false;
        d.selfPenetration=0;d.selfContacts=0;
        for(auto &p:d.parts) {
            if(p.body->getActivationState()==DISABLE_DEACTIVATION) p.body->forceActivationState(ACTIVE_TAG);
            const auto &v=p.body->getLinearVelocity();const auto &w=p.body->getAngularVelocity();
            speed=std::max(speed,v.length());spin=std::max(spin,w.length());
            energy+=.5f/p.body->getInvMass()*v.length2()+.5f*w.dot(p.body->getInvInertiaTensorWorld().inverse()*w);
        }
        for(int i=0;i<dispatcher.getNumManifolds();++i) {
            const auto *m=dispatcher.getManifoldByIndexInternal(i);
            const auto *a=static_cast<const btCollisionObject*>(m->getBody0());const auto *b=static_cast<const btCollisionObject*>(m->getBody1());
            const bool oursA=a->getUserPointer()==&d,oursB=b->getUserPointer()==&d;
            if(oursA && oursB) for(int j=0;j<m->getNumContacts();++j) {
                const auto depth=-m->getContactPoint(j).getDistance();
                if(depth>0) {d.selfPenetration=std::max(d.selfPenetration,depth);++d.selfContacts;}
            }
            if(oursA==oursB) continue;
            const auto *other=oursA?b:a;
            if(other->getCollisionFlags()&btCollisionObject::CF_NO_CONTACT_RESPONSE) continue;
            const auto *rigid=btRigidBody::upcast(other);
            for(int j=0;j<m->getNumContacts();++j) {
                const auto &p=m->getContactPoint(j);if(p.getDistance()>.025f) continue;
                penetration=std::max(penetration,-p.getDistance());
                // Only an external dynamic collision can wake a supported
                // island. Self/contact/limit solver noise is not a new impact.
                if(rigid && !rigid->isStaticOrKinematicObject() && p.getAppliedImpulse()>.25f &&
                    (rigid->getLinearVelocity().length2()>.0225f || rigid->getAngularVelocity().length2()>.16f)) externalImpact=true;
                const float up=oursA?p.m_normalWorldOnB.z():-p.m_normalWorldOnB.z();
                if(up<.45f) continue;
                supported=true;
                if(rigid && (rigid->getLinearVelocity().length2()>.0009f || rigid->getAngularVelocity().length2()>.0025f)) unstableSupport=true;
            }
        }
        for(const auto &joint:d.joints) {
            btTransform a,b;
            jointFrames(*joint,a,b);
            jointError=std::max(jointError,(joint->getRigidBodyA().getWorldTransform()*a.getOrigin()-joint->getRigidBodyB().getWorldTransform()*b.getOrigin()).length());
        }
        d.currentSpeed=speed;d.currentSpin=spin;d.restPenetration=penetration;
        d.supported=supported && !unstableSupport;d.restEnergy=energy;d.restJointError=jointError;
        if(d.settled) {
            if(!supported || unstableSupport || externalImpact) wake(d);
            else for(auto &p:d.parts) {
                // Restore the solved physical sample, not the independently
                // interpolated visual transforms which violate pivot anchors.
                p.body->setCenterOfMassTransform(d.restBodyPose[p.bone]);
                p.body->setInterpolationWorldTransform(d.restBodyPose[p.bone]);
                p.body->setLinearVelocity(btVector3(0,0,0));p.body->setAngularVelocity(btVector3(0,0,0));
                p.body->clearForces();p.body->forceActivationState(ISLAND_SLEEPING);
                dynamics.updateSingleAabb(p.body.get());
            }
            continue;
        }
        d.restBlockers=(!supported?1u:0u)|(unstableSupport?2u:0u)|(penetration>=.05f?4u:0u)|(jointError>=.04f?8u:0u)|(d.sinceImpact<=.5f?16u:0u)|(speed>=.075f?32u:0u)|(spin>=.22f?64u:0u)|(energy>=.2f?128u:0u)|(d.velocityCorrections!=d.lastCorrections?256u:0u);
        const bool safe=supported && !unstableSupport && penetration<.05f && jointError<.04f && d.sinceImpact>.5f;
        d.quietTime=safe && speed<.075f && spin<.22f?d.quietTime+elapsed:0;
        bool stable=safe && d.velocityCorrections==d.lastCorrections;
        if(d.stationaryTime>0) for(auto &p:d.parts) {
            const auto &before=d.stationaryPose[p.bone];const auto &now=p.body->getWorldTransform();
            const float dot=btFabs(before.getRotation().dot(now.getRotation()));
            if((before.getOrigin()-now.getOrigin()).length2()>.000225f) {stable=false;d.restBlockers|=512u;}
            if(dot<btCos(.035f)) {stable=false;d.restBlockers|=1024u;}
        }
        if(!stable || d.stationaryTime<=0) {d.stationaryTime=0;d.stationaryEnergy=0;for(auto &p:d.parts)d.stationaryPose[p.bone]=p.body->getWorldTransform();}
        if(stable) {d.stationaryTime+=elapsed;d.stationaryEnergy+=energy*elapsed;}
        d.lastCorrections=d.velocityCorrections;
        // Judge kinetic energy over the existing stationary-pose window.
        // One constraint/contact velocity spike must not erase three seconds
        // of supported, geometrically stationary, low-energy evidence.
        if(d.quietTime<.75f && !(d.age>2 && d.stationaryTime>3 && d.stationaryEnergy/d.stationaryTime<.2f)) continue;
        // Capture precisely the interpolated palette the renderer would see;
        // stopping does not replace it with a different simulation sample.
        // Keep physical COMs at the solved sample. Independent interpolated
        // rotations/translations do not satisfy joint anchors and must never
        // be fed back into the articulated island at sleep.
        pose(d,d.restPose,true);d.settled=true;++d.settleCount;
        for(auto &p:d.parts) d.restBodyPose[p.bone]=p.body->getWorldTransform();
        for(auto &p:d.parts) {
            p.body->setLinearVelocity(btVector3(0,0,0));p.body->setAngularVelocity(btVector3(0,0,0));
            p.body->clearForces();p.body->forceActivationState(ISLAND_SLEEPING);dynamics.updateSingleAabb(p.body.get());
        }
    }
}
void World::step(double elapsed,float gravity) {
    if(!std::isfinite(elapsed) || elapsed<0 || !std::isfinite(gravity)) return;
    dynamics.setGravity(btVector3(0,0,-std::clamp(gravity,0.f,4000.f)*UnitsToMeters));
    constexpr double fixed=1.0/120.0;
    accumulator+=std::min(elapsed,.0666666666667);
    int steps=0;
    while(accumulator+1e-10>=fixed && steps++<8) {
        const int remaining=std::max(1,std::min(9-steps,static_cast<int>((accumulator+1e-10)/fixed)));
        for(auto &m:meshes) if(m->moving && m->initialized) {
            const btTransform before=m->body->getWorldTransform();
            btTransform after;
            after.setOrigin(before.getOrigin().lerp(m->target.getOrigin(),1.f/remaining));
            after.setRotation(before.getRotation().slerp(m->target.getRotation(),1.f/remaining));
            m->body->setWorldTransform(after);
            dynamics.updateSingleAabb(m->body.get());
        }
        for(auto &d:dolls) if(!d->settled) for(auto &p:d->parts)
            p.body->setInterpolationWorldTransform(p.body->getWorldTransform());
        // Passive joint resistance, not an animation motor or whole-body brake.
        // Equal/opposite impulses dissipate relative rotation while preserving
        // total angular momentum. Bullet's cone damping parameter is used by
        // its obsolete solver path, so implement this explicitly for this path.
        for(auto &d:dolls) if(!d->settled) for(auto &joint:d->joints) {
            auto &a=joint->getRigidBodyA();auto &b=joint->getRigidBodyB();
            // WANTS_DEACTIVATION bodies still participate in the solver. Removing
            // joint resistance here re-excites an otherwise quiet island.
            if(!a.isActive() || !b.isActive()) continue;
            const btVector3 relative=b.getAngularVelocity()-a.getAngularVelocity();
            const float speed=relative.length();if(speed<.25f) continue;
            const btVector3 axis=relative/speed;
            const float inverse=axis.dot((a.getInvInertiaTensorWorld()+b.getInvInertiaTensorWorld())*axis);
            if(inverse<=1e-8f) continue;
            const int index=d->partForBone[b.getUserIndex()];
            const Region role=d->parts[index].region;
            const bool torso=role==Region::Torso || role==Region::Pelvis;
            const float limit=torso?2.f:(role==Region::Calf || role==Region::Thigh?1.5f:.7f);
            const float impulse=std::min(speed/inverse*(1-btExp(-(torso?5.f:3.f)*static_cast<float>(fixed))),limit*static_cast<float>(fixed));
            a.applyTorqueImpulse(axis*impulse);b.applyTorqueImpulse(-axis*impulse);
        }
        for(auto &d:dolls) if(!d->settled && !d->frozen) stepTransition(*d,static_cast<float>(fixed));
        dynamics.stepSimulation(static_cast<float>(fixed),0);
        for(auto &d:dolls) if(!d->settled) for(auto &p:d->parts) {
            const btVector3 linear=p.body->getLinearVelocity(),angular=p.body->getAngularVelocity();
            if(finite(linear)) d->maximumSpeed=std::max(d->maximumSpeed,linear.length());
            if(finite(angular)) d->maximumAngularSpeed=std::max(d->maximumAngularSpeed,angular.length());
            if(!finite(linear) || linear.length2()>400) {p.body->setLinearVelocity(bounded(linear,20));++d->velocityCorrections;++d->linearCorrections;}
            if(!finite(angular) || angular.length2()>625) {p.body->setAngularVelocity(bounded(angular,25));++d->velocityCorrections;++d->angularCorrections;}
        }
        accumulator-=fixed;
    }
    accumulator=std::clamp(accumulator,0.0,fixed);
    if(steps>0) updateSleep(static_cast<float>(std::min(steps,8)*fixed));
}
}
