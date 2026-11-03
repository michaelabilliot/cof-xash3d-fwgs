#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>
#include <locale>

namespace dx_light_position {
using Point = std::array<float,3>;
inline bool valid(const Point& p) {
    for(float x:p) if(!std::isfinite(x)||std::abs(x)>1000000.f)return false;
    return true;
}
inline float distanceSquared(const Point& a,const Point& b) {
    float d=0;for(int i=0;i<3;++i)d+=(a[i]-b[i])*(a[i]-b[i]);return d;
}
inline bool lampMaterial(std::string name) {
    if(name.empty()||name.size()>63||name[0]=='{')return false;
    for(char& c:name)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if(name.find("clip")!=std::string::npos||name.find("lightmap")!=std::string::npos)return false;
    return name.find("lamp")!=std::string::npos||name.find("light")!=std::string::npos||
        name.find("bulb")!=std::string::npos||name.find("fluo")!=std::string::npos;
}
// Closed outdoor globe materials verified in c_start/c_start2. Indoor
// pendants and flat panels must retain the authored source below the shade.
inline bool outdoorGlobeMaterial(std::string name) {
 for(char& c:name)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
 return name=="r_sthlight"||name=="c2_galamps1";
}
struct Face { int owner=0;std::vector<Point> vertices; };
struct Fixture { Point minimum{},maximum{},center{};int owner=0;float radius=0; };

// Connected surfaces, not model-wide bounds: CoF combines many separate lamps
// and entire streets into a single decorative brush entity.
inline std::vector<Fixture> fixtures(const std::vector<Face>& faces) {
    if(faces.size()>32768)return {};
    std::vector<size_t> parent(faces.size());std::iota(parent.begin(),parent.end(),0);
    auto root=[&](size_t i){while(parent[i]!=i){parent[i]=parent[parent[i]];i=parent[i];}return i;};
    std::map<std::array<int,4>,size_t> vertices;
    std::vector<bool> usable(faces.size(),true);
    for(size_t i=0;i<faces.size();++i) {
        if(faces[i].vertices.size()<3||faces[i].vertices.size()>64){usable[i]=false;continue;}
        for(const auto& p:faces[i].vertices)if(!valid(p))usable[i]=false;
        if(!usable[i])continue;
        for(const auto& p:faces[i].vertices) {
            const std::array<int,4> key{faces[i].owner,int(std::lround(p[0]*32)),int(std::lround(p[1]*32)),int(std::lround(p[2]*32))};
            const auto found=vertices.emplace(key,i);
            if(!found.second)parent[root(i)]=root(found.first->second);
        }
    }
    std::map<size_t,Fixture> groups;
    for(size_t i=0;i<faces.size();++i)if(usable[i]) {
        const auto key=root(i);auto found=groups.find(key);
        if(found==groups.end()) {
            Fixture f;f.minimum=faces[i].vertices[0];f.maximum=f.minimum;f.owner=faces[i].owner;
            found=groups.emplace(key,f).first;
        }
        for(const auto& p:faces[i].vertices)for(int axis=0;axis<3;++axis) {
            found->second.minimum[axis]=std::min(found->second.minimum[axis],p[axis]);
            found->second.maximum[axis]=std::max(found->second.maximum[axis],p[axis]);
        }
    }
    // Measure the entire emitting mesh, not the nearest exit from a solid.
    // A globe needs a sphere enclosing its furthest vertex plus a margin.
    for(auto& group:groups)for(int axis=0;axis<3;++axis)
        group.second.center[axis]=(group.second.minimum[axis]+group.second.maximum[axis])*.5f;
    for(size_t i=0;i<faces.size();++i)if(usable[i]) {
        auto& fixture=groups[root(i)];
        for(const auto& point:faces[i].vertices)
            fixture.radius=std::max(fixture.radius,std::sqrt(distanceSquared(point,fixture.center)));
    }
    std::vector<Fixture> output;
    for(auto& group:groups) {
        auto f=group.second;float largest=0;int dimensions=0;
        for(int i=0;i<3;++i){const float size=f.maximum[i]-f.minimum[i];largest=std::max(largest,size);if(size>=1)++dimensions;f.center[i]=(f.minimum[i]+f.maximum[i])*.5f;}
        if(largest>=2&&largest<=64&&dimensions>=2){f.radius+=1.f;output.push_back(f);}
    }
    return output;
}
struct Match { Point position{};int fixture=-1;float movement=0; };
// Follow the BSP splits exactly: fixed-distance sampling can miss thin walls.
// The adapter returns contents < 0 for a leaf, otherwise a plane distance.
template<class Node,class Contents,class Distance,class Child>
bool clearSegment(Node node,const Point& a,const Point& b,Contents contents,Distance distance,Child child,int& budget,int depth=0) {
    if(!node||--budget<0||depth>128)return false;
    const int value=contents(node);
    if(value<0)return value!=-2;
    const float da=distance(node,a),db=distance(node,b);
    if(!std::isfinite(da)||!std::isfinite(db))return false;
    if(da>=0&&db>=0)return clearSegment(child(node,0),a,b,contents,distance,child,budget,depth+1);
    if(da<0&&db<0)return clearSegment(child(node,1),a,b,contents,distance,child,budget,depth+1);
    const float t=da/(da-db);Point midpoint;
    for(int i=0;i<3;++i)midpoint[i]=a[i]+(b[i]-a[i])*t;
    const int side=da<0?1:0;
    return clearSegment(child(node,side),a,midpoint,contents,distance,child,budget,depth+1)&&
        clearSegment(child(node,1-side),midpoint,b,contents,distance,child,budget,depth+1);
}
inline Match locate(const Point& origin,const std::vector<Fixture>& candidates,float maximumMovement=24.f,bool includeCentered=false) {
    Match result;result.position=origin;
    if(!valid(origin)||!std::isfinite(maximumMovement)||maximumMovement<=0)return result;
    maximumMovement=std::min(maximumMovement,32.f);
    float best=std::numeric_limits<float>::max(),second=best;int selected=-1;
    for(size_t i=0;i<candidates.size();++i) {
        if(!valid(candidates[i].center))continue;
        const float distance=std::sqrt(distanceSquared(origin,candidates[i].center));
        if(distance>maximumMovement)continue;
        // Same centre split across owners is not an ambiguous second lamp.
        if(selected>=0&&distanceSquared(candidates[i].center,candidates[size_t(selected)].center)<.25f)continue;
        if(distance<best){second=best;best=distance;selected=int(i);}else second=std::min(second,distance);
    }
    if(selected<0||(!includeCentered&&best<.5f)||second-best<4.f)return result;
    result.position=candidates[size_t(selected)].center;result.fixture=selected;result.movement=best;return result;
}
struct Override { int entity=0;Point authored{},position{};float radius=0;bool hasPosition=true; };
inline bool applicable(const Override& value,int entity,const Point& authored) {
    return value.entity==entity&&valid(value.authored)&&valid(value.position)&&valid(authored)&&
        distanceSquared(value.authored,authored)<=.01f&&distanceSquared(value.authored,value.position)<=256.f*256.f&&
        std::isfinite(value.radius)&&(value.radius==0||(value.radius>=1&&value.radius<=24));
}
inline std::vector<Override> parse(const std::string& text) {
    if(text.size()>65536)return {};
    std::vector<Override> output;std::istringstream stream(text);stream.imbue(std::locale::classic());std::string line;
    while(std::getline(stream,line)) {
        const auto start=line.find_first_not_of(" \t\r");
        if(start==std::string::npos||line[start]=='#'||line.compare(start,2,"//")==0)continue;
        std::istringstream row(line);row.imbue(std::locale::classic());Override value;std::string extra;
        if(line.compare(start,7,"radius ")==0) {
            row>>extra;
            if(!(row>>value.entity>>value.authored[0]>>value.authored[1]>>value.authored[2]>>value.radius)||row>>extra||value.radius<1)continue;
            value.position=value.authored;value.hasPosition=false;
        } else if(!(row>>value.entity>>value.authored[0]>>value.authored[1]>>value.authored[2]>>value.position[0]>>value.position[1]>>value.position[2])||row>>extra)continue;
        if(value.entity<0||!applicable(value,value.entity,value.authored))continue;
        const auto existing=std::find_if(output.begin(),output.end(),[&](const Override& item){return item.entity==value.entity;});
        if(existing==output.end()) {if(output.size()>=256)return {};output.push_back(value);}
        else if(distanceSquared(existing->authored,value.authored)>.01f)*existing=value;
        else {
            if(value.hasPosition){existing->position=value.position;existing->hasPosition=true;}
            if(value.radius>0)existing->radius=value.radius;
        }
    }
    return output;
}
}
