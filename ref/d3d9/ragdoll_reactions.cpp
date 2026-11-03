#include "ragdoll_physics.h"
#include <algorithm>
#include <cmath>

namespace ragdoll {
float shotBudget(int weapon,int pellets,float scale) {
    if(!std::isfinite(scale)) return 0;
    const float count=static_cast<float>(std::clamp(pellets,1,24));
    const float base=weapon==2?2.5f*std::min(std::sqrt(count),2.4f):(weapon==1?2.2f:3.f);
    return std::clamp(base*std::clamp(scale,0.f,3.f),0.f,8.f);
}
int hitBone(const std::vector<Bone> &bones,const btVector3 &point,int group,int sourceBone) {
    if(!finite(point)) return -1;
    float nearest=.75f*.75f;int selected=-1;
    for(size_t i=0;i<bones.size();++i) {
        if(sourceBone>=0 && static_cast<int>(i)!=sourceBone) continue;
        const auto &bone=bones[i];if(!bone.collision || !valid(bone.pose)) continue;
        const btVector3 local=bone.pose.inverse()*point;
        const btVector3 closest(std::clamp(local.x(),bone.minimum.x(),bone.maximum.x()),std::clamp(local.y(),bone.minimum.y(),bone.maximum.y()),std::clamp(local.z(),bone.minimum.z(),bone.maximum.z()));
        const float separation=(local-closest).length2();
        if(sourceBone>=0 && separation>.75f*.75f) continue;
        float distance=separation+.002f*(local-(bone.minimum+bone.maximum)*.5f).length2();
        if(group>0 && bone.group==group) distance*=.25f;
        if(distance<nearest) {nearest=distance;selected=static_cast<int>(i);}
    }
    return selected;
}
static btVector3 limited(btVector3 v,float limit) {const float length=v.length();return length>limit?v*(limit/length):v;}
static void spring(btVector3 &position,btVector3 &velocity,float dt,float frequency) {
    const btVector3 j=velocity+position*frequency;const float decay=btExp(-frequency*dt);
    velocity=(velocity-j*(frequency*dt))*decay;position=(position+j*dt)*decay;
    if(position.length2()<1e-10f && velocity.length2()<1e-8f) {position.setZero();velocity.setZero();}
}
void Reactions::step(float seconds) {
    if(!std::isfinite(seconds) || seconds<=0) return;
    // Analytic critically damped integration remains stable after a long frame.
    for(auto &bone:bones) {spring(bone.angle,bone.angular,seconds,12.f);spring(bone.shift,bone.velocity,seconds,16.f);}
}
bool Reactions::active() const {
    for(const auto &bone:bones) if(bone.angle.length2()>1e-10f || bone.angular.length2()>1e-8f || bone.shift.length2()>1e-10f || bone.velocity.length2()>1e-8f) return true;
    return false;
}
void Reactions::hit(const std::vector<Bone> &skeleton,int bone,const btVector3 &point,const btVector3 &direction,float impulse) {
    if(bone<0 || bone>=static_cast<int>(skeleton.size()) || skeleton.size()>MaxBones || !finite(point) || !finite(direction) || direction.length2()<.5f || !std::isfinite(impulse) || impulse<=0) return;
    auto kick=[&](int index,float amount) {
        const auto &part=skeleton[index];auto &state=bones[index];
        const btVector3 local=part.pose.getBasis().transpose()*direction.normalized();
        const btVector3 lever=limited(part.pose.getBasis().transpose()*(point-part.pose.getOrigin()),.25f);
        const bool torso=part.region==Region::Torso || part.region==Region::Pelvis;
        const bool leg=part.region==Region::Thigh || part.region==Region::Calf || part.region==Region::Foot;
        const float mass=torso?18.f:(part.region==Region::Head?5.f:(leg?7.f:2.5f));
        const btVector3 size=part.maximum-part.minimum;
        const float inertia=std::max(.004f,mass*size.length2()/12.f);
        state.angular=limited(state.angular+lever.cross(local)*(amount*.45f/inertia),torso?1.f:(leg?1.5f:4.f));
        state.velocity=limited(state.velocity+local*(amount*.45f/mass),torso?.15f:(leg?.12f:.5f));
    };
    const float strength=std::clamp(impulse,0.f,8.f);kick(bone,strength);
    const int parent=skeleton[bone].parent;
    if(parent>=0) kick(parent,strength*.25f);
    int children=0;for(size_t i=0;i<skeleton.size();++i) if(skeleton[i].parent==bone) ++children;
    for(size_t i=0;i<skeleton.size();++i) if(skeleton[i].parent==bone) kick(static_cast<int>(i),strength*.15f/std::max(1,children));
}
void Reactions::apply(const std::vector<Bone> &skeleton,std::vector<btTransform> &out) const {
    if(skeleton.size()>MaxBones) {out.clear();return;}
    out.resize(skeleton.size());
    for(size_t i=0;i<skeleton.size();++i) {
        const auto &bone=skeleton[i];const auto &state=bones[i];
        btTransform local=bone.parent<0?bone.pose:skeleton[bone.parent].pose.inverse()*bone.pose;
        const bool leg=bone.region==Region::Thigh || bone.region==Region::Calf || bone.region==Region::Foot;
        const bool torso=bone.region==Region::Torso || bone.region==Region::Pelvis;
        const btVector3 angle=limited(state.angle,torso?.12f:(leg?.15f:.35f));
        const float length=angle.length();const btQuaternion rotation=length>1e-6f?btQuaternion(angle/length,length):btQuaternion::getIdentity();
        local.setOrigin(local.getOrigin()+local.getBasis()*limited(state.shift,torso?.01f:(leg?.006f:.025f)));
        local.setRotation(local.getRotation()*rotation);
        out[i]=bone.parent<0?local:out[bone.parent]*local;
    }
}
}
