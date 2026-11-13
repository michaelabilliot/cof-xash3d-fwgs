// Recovered original custom-inventory source, Git blob 8a43e97349398e49196d40eeb41f392d06d89213.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
namespace cof::hud {
struct Weapon { int ammo=-1, capacity=0; bool defined=false; };
struct State {
    std::array<Weapon,256> weapons{};
    bool healthKnown=false, staminaKnown=false;
    int cap=0;
    void reset(bool definitions=false) {
        healthKnown=staminaKnown=false; cap=0;
        if(definitions)weapons={};
    }
    bool consume(const char* name,int size,const void* data) {
        if(!name||size<0||(!data&&size))return false;
        const auto b=static_cast<const unsigned char*>(data);
        if(!strcmp(name,"Health")) { if(size!=3)return false; healthKnown=true;return true; }
        if(!strcmp(name,"Stamina")) { if(size!=3)return false; staminaKnown=true;return true; }
        if(!strcmp(name,"HealthCap")) { if(size!=1||b[0]>2)return false;cap=b[0];return true; }
        if(!strcmp(name,"WpnInfo")) {
            if(size!=4||!b[0]||b[1]>127)return false;
            weapons[b[0]].capacity=b[1];return true;
        }
        if(!strcmp(name,"WeaponList")) {
            if(size<10)return false;
            const void* end=memchr(b,0,size_t(size));if(!end)return false;
            const size_t n=static_cast<const unsigned char*>(end)-b+1;
            if(n+8!=size_t(size))return false;
            const int id=b[n+6];if(id<1||id>127)return false;
            weapons[id].defined=true;weapons[id].ammo=b[n]==255?-1:b[n];return true;
        }
        return true;
    }
};
struct Ammo { int id=0, clip=-1, reserve=-1, capacity=0, type=-1; };
struct View {
    int health=0,stamina=0,cap=0;
    bool healthVisible=false,staminaVisible=false;
    Ammo primary{},secondary{};
    float time=0;
};
// WpnInfo is conditional on reserve changes. Counts do not depend on capacity.
inline bool firearm(const Ammo& a) {return a.id>0&&a.clip>=0&&a.clip<=32768&&a.reserve>=0&&a.reserve<=32768;}
inline int segments(const Ammo& a) {
    if(!firearm(a)||a.clip==0||a.capacity<=0||a.capacity>32768)return 0;
    return std::clamp((a.clip*12+a.capacity-1)/a.capacity,0,12);
}
inline float scaleFor(int width,int height) {
    return std::min(float(width)/1920.f,float(height)/1080.f);
}
}
