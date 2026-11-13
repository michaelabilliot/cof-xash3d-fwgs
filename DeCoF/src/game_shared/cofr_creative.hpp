#pragma once
#include "cofr_item_script.hpp"
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace cof {
inline constexpr const char* CreativeMessage = "Cof8Create";
inline constexpr int CreativeLimit = 256;
enum class CreativeGroup { Weapon = 0, Item = 1, Entity = 2 };
struct CreativeItem {
    std::string script, classname, name, icon, model;
    CreativeGroup group = CreativeGroup::Item;
};
inline bool creativeEnabled(float cheats, int clients, float deathmatch, float coop) {
    return cheats == 1.f && clients == 1 && deathmatch == 0.f && coop == 0.f;
}
inline bool scriptPath(const std::string& path) {
    if (path.size() < 20 || path.size() > 150 || path.compare(0,15,"inventoryitems/") ||
        path.compare(path.size()-4,4,".txt") || path.find("..") != std::string::npos) return false;
    for (unsigned char c : path) if (!std::isalnum(c) && c!='/' && c!='_' && c!='-' && c!='.') return false;
    return true;
}
inline bool creativeNumber(const char* value, int& number) {
    if (!value || !*value || strlen(value) > 5) return false;
    number = 0;
    for (const char* p=value; *p; ++p) {
        if (*p<'0' || *p>'9') return false;
        number = number*10 + (*p-'0');
    }
    return true;
}
struct CreativePacket {
    int kind=0, id=0, count=0;
    bool notarget=false;bool enabled=false, okay=false;
    CreativeGroup group=CreativeGroup::Item;
    std::string value;
};
inline bool creativePacket(const unsigned char* bytes,size_t size,CreativePacket& packet) {
    if (!bytes || size<1 || size>192) return false;
    CreativePacket result; size_t pos=0; bool good=true;
    auto byte=[&]() { if (pos>=size) {good=false;return 0;} return int(bytes[pos++]); };
    auto word=[&]() { int low=byte(); return low|(byte()<<8); };
    auto string=[&]() {
        std::string value;
        while (pos<size && bytes[pos] && value.size()<150) value+=char(bytes[pos++]);
        if (pos>=size || bytes[pos]) good=false; else ++pos;
        return value;
    };
    result.kind=byte();
    if (result.kind==0) {
        const int enabled=byte(); result.enabled=enabled==1; result.count=word();
        if (enabled>1 || result.count>CreativeLimit || (!result.enabled && result.count)) return false;
    } else if (result.kind==1) {
        result.id=word(); const int group=byte(); result.group=CreativeGroup(group); result.value=string();
        if (result.id<1 || result.id>CreativeLimit || group>2 || (group<2 ? !scriptPath(result.value) : result.value.compare(0,8,"monster_")!=0)) return false;
    } else if (result.kind==2) {
        const int okay=byte(); result.okay=okay==1; result.value=string();
        if (okay>1) return false;
    } else if(result.kind==3){int value=byte();if(value>1)return false;result.notarget=value!=0;} else return false;
    if (!good || pos!=size) return false;
    packet=std::move(result); return true;
}
template<class Load, class Export, class Asset>
std::vector<CreativeItem> creativeCatalogue(const char* const* scripts, size_t count, Load load, Export exported, Asset assetExists) {
    std::vector<CreativeItem> items;
    for (size_t i=0; i<count && items.size()<CreativeLimit; ++i) {
        if (!scriptPath(scripts[i])) continue;
        auto content=load(scripts[i]);
        std::map<std::string,std::string> data;
        if (content.empty() || content.size()>1024*1024 || !ItemScript(content).parse(data)) continue;
        CreativeItem item{scripts[i],data["classname"],data["nice_name"],data["inventory_picture"],data["3dmdl"]};
        bool weapon = item.classname.compare(0,7,"weapon_")==0;
        if (!weapon && item.classname!="ammo_rpg7" && item.classname!="item_key" && item.classname!="item_phonebattery" && item.classname!="item_glocktaclight") continue;
        if (!exported(item.classname.c_str()) || item.name.empty() || item.icon.empty()) continue;
        if (weapon) {
            const auto canonical="inventoryitems/weapons/"+item.classname+".txt";
            if (item.script!=canonical && std::find_if(scripts,scripts+count,[&](const char* p){return canonical==p;})!=scripts+count) continue;
            if (std::any_of(items.begin(),items.end(),[&](const CreativeItem& p){return p.classname==item.classname;})) continue;
        } else if ((item.classname!="ammo_rpg7"&&data["map_name"].empty()) || item.model.empty()) continue;
        // Some shipped legacy definitions refer to deleted plate assets. Do
        // not advertise entries that cannot render or produce a dropped model.
        if (!assetExists(item.icon.c_str()) || (!weapon && !assetExists(item.model.c_str()))) continue;
        item.group = weapon ? CreativeGroup::Weapon : CreativeGroup::Item;
        items.push_back(std::move(item));
    }
    std::sort(items.begin(),items.end(),[](const CreativeItem& a,const CreativeItem& b){
        if (a.group!=b.group) return int(a.group)<int(b.group);
        if (a.name!=b.name) return a.name<b.name;
        return a.script<b.script;
    });
    return items;
}
// Reposition only a newly-created record. Occupied destinations never mutate.
template<class Arrays>
bool placeCreative(Arrays arrays, int created, int destination) {
    if (created<1 || created>8 || destination<1 || destination>8 || !arrays(0)[created]) return false;
    if (created==destination) return true;
    if (arrays(0)[destination]) return false;
    for (int kind=0;kind<4;++kind) std::swap(arrays(kind)[created],arrays(kind)[destination]);
    arrays(3)[created]=arrays(3)[destination]=-1;
    return true;
}
}
