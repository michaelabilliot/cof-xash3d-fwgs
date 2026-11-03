#pragma once
#include <cmath>
#include <cstdint>
#include <limits>
struct dx_cof_dual_state_t {
    cl_entity_t entity{};float time=0;bool valid=false,mirror=false;
    int primaryHiddenArm=0,secondaryHiddenArm=0;
    uint64_t lastPass=std::numeric_limits<uint64_t>::max();
    void clear(){valid=false;entity={};primaryHiddenArm=secondaryHiddenArm=0;lastPass=std::numeric_limits<uint64_t>::max();}
    bool publish(const cl_entity_t* value,int reflected,float timestamp) {
        if(!value){clear();return true;}
        if(!std::isfinite(timestamp)||(valid && timestamp<time)){clear();return false;}
        if(!value->model||value->model->type!=mod_studio||(value->curstate.effects&EF_NODRAW)||
            value->curstate.sequence<0||value->curstate.body<0||value->curstate.skin<0||
            !std::isfinite(value->curstate.frame)||!std::isfinite(value->curstate.animtime)||!std::isfinite(value->curstate.framerate)) {clear();return false;}
        for(int i=0;i<3;++i)if(!std::isfinite(value->origin[i])||!std::isfinite(value->angles[i])){clear();return false;}
        entity=*value;time=timestamp;mirror=reflected!=0;valid=true;return true;
    }
    cl_entity_t* current(float now) {
        if(!valid)return nullptr;
        const float age=now-time;
        if(!std::isfinite(age)||age<0||age>=.25f){clear();return nullptr;}
        return &entity;
    }
    template<class Submit> bool submit(float now,uint64_t pass,Submit&& draw) {
        auto value=current(now);if(!value||pass==lastPass)return false;
        lastPass=pass;draw(value,mirror);return true;
    }
};
