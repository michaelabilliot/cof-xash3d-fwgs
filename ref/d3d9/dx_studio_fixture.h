#pragma once
#include "dx_light_position.h"
#include <cstring>
void R_StudioFixtureTransform(const float* origin,const float* angles,float scale,D3DMATRIX* world);
void R_StudioFixtureBone(const float* pose,float (*matrix)[4]);

namespace dx_studio_fixture {
using dx_light_position::Point;
struct Bulb {
    dx_light_position::Fixture bounds;
    std::vector<Point> vertices;
    std::string model;
    int body=0,skin=0,entity=-1;
};
inline int profile(std::string path) {
    for(char& c:path){if(c=='\\')c='/';c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));}
    if(path=="models/props/utomhusd/stadlyktrund4.mdl")return 4;
    if(path=="models/props/utomhusd/stadlyktrund2.mdl")return 2;
    return 0;
}
class Reader {
    const unsigned char* data;size_t length;
public:
    Reader(const std::vector<unsigned char>& bytes):data(bytes.data()),length(bytes.size()){}
    template<class T> bool get(int offset,T& value) const {
        if(offset<0||size_t(offset)>length||sizeof(T)>length-size_t(offset))return false;
        std::memcpy(&value,data+offset,sizeof(T));return true;
    }
};
// These two supported assets have one rigid bone and a constant idle pose.
// Reject different animation layouts instead of silently using the bind pose.
inline bool extract(const std::vector<unsigned char>& bytes,const std::string& path,int body,int skin,
    const D3DMATRIX& world,Bulb& output,std::string& reason) {
    reason="invalid studio data";const int kind=profile(path);
    if(!kind){reason="unsupported model";return false;}
    if(kind==4&&body==1){reason="pole only";return false;}
    if(bytes.size()>16*1024*1024)return false;
    Reader read(bytes);studiohdr_t hdr{};
    if(!read.get(0,hdr)||hdr.ident!=0x54534449||hdr.version!=10||hdr.length!=int(bytes.size())||
        hdr.numbones!=1||hdr.numbonecontrollers!=0||hdr.numbodyparts!=1||hdr.numseq<1||hdr.numseq>256||
        hdr.numtextures<1||hdr.numtextures>256||hdr.numskinref<1||hdr.numskinref>256||hdr.numskinfamilies<1||hdr.numskinfamilies>256)return false;
    mstudiobone_t bone{};mstudioseqdesc_t seq{};mstudiobodyparts_t part{};mstudioanim_t anim{};
    if(!read.get(hdr.boneindex,bone)||!read.get(hdr.seqindex,seq)||!read.get(hdr.bodypartindex,part)||
        bone.parent!=-1||seq.seqgroup!=0||seq.numblends!=1||seq.numframes<1||seq.numframes>255||
        !read.get(seq.animindex,anim)||part.base!=1||part.nummodels!=(kind==4?3:2)||body<0||body>=part.nummodels||skin<0||skin>=hdr.numskinfamilies)return false;
    float pose[6];
    for(int i=0;i<6;++i) {
        pose[i]=bone.value[i];
        if(bone.bonecontroller[i]>=0){reason="unsupported bone controller";return false;}
        if(anim.offset[i]) {
            mstudioanimvalue_t span{},sample{};
            if(!read.get(seq.animindex+anim.offset[i],span)||span.num.valid!=1||span.num.total<seq.numframes||
                !read.get(seq.animindex+anim.offset[i]+int(sizeof(span)),sample)){reason="nonconstant idle pose";return false;}
            pose[i]+=sample.value*bone.scale[i];
        }
        if(!std::isfinite(pose[i]))return false;
    }
    const int selected=kind==4?2:body;
    mstudiomodel_t model{};
    if(part.modelindex<0||part.modelindex>hdr.length-int(sizeof(model))*part.nummodels||
        !read.get(part.modelindex+selected*int(sizeof(model)),model)||model.numverts<3||model.numverts>4096||model.nummesh<1||model.nummesh>256)return false;
    matrix3x4 boneMatrix;R_StudioFixtureBone(pose,boneMatrix);
    Bulb bulb;bulb.model=path;bulb.body=selected;bulb.skin=skin;
    std::vector<bool> used(size_t(model.numverts),false);
    // Skin resolution is validated even though these assets currently have one texture.
    for(int m=0;m<model.nummesh;++m) {
        mstudiomesh_t mesh{};short textureIndex=0;mstudiotexture_t texture{};
        if(model.meshindex<0||model.meshindex>hdr.length-int(sizeof(mesh))*model.nummesh||
            !read.get(model.meshindex+m*int(sizeof(mesh)),mesh)||mesh.skinref<0||mesh.skinref>=hdr.numskinref||
            hdr.skinindex<0||hdr.skinindex>hdr.length-2*hdr.numskinref*hdr.numskinfamilies||
            !read.get(hdr.skinindex+2*(skin*hdr.numskinref+mesh.skinref),textureIndex)||textureIndex<0||textureIndex>=hdr.numtextures||
            hdr.textureindex<0||hdr.textureindex>hdr.length-int(sizeof(texture))*hdr.numtextures||
            !read.get(hdr.textureindex+textureIndex*int(sizeof(texture)),texture))return false;
        int offset=mesh.triindex;bool terminated=false;
        for(int commands=0;commands<8192;++commands) {
            short count=0;if(!read.get(offset,count))return false;offset+=2;
            if(!count){terminated=true;break;}
            const int n=std::abs(int(count));if(n<3||n>256)return false;
            for(int i=0;i<n;++i) {
                mstudiotrivert_t vertex{};if(!read.get(offset,vertex)||vertex.vertindex<0||vertex.vertindex>=model.numverts)return false;
                offset+=int(sizeof(vertex));used[size_t(vertex.vertindex)]=true;
            }
        }
        if(!terminated)return false;
    }
    for(int i=0;i<model.numverts;++i)if(used[size_t(i)]) {
        Point local{},posed{};unsigned char boneIndex=255;
        if(model.vertindex<0||model.vertindex>hdr.length-12*model.numverts||model.vertinfoindex<0||
            model.vertinfoindex>hdr.length-model.numverts||!read.get(model.vertindex+i*12,local)||
            !read.get(model.vertinfoindex+i,boneIndex)||boneIndex!=0||!dx_light_position::valid(local))return false;
        Matrix3x4_VectorTransform(boneMatrix,local.data(),posed.data());
        Point p{};
        for(int a=0;a<3;++a)p[a]=posed[0]*world.m[0][a]+posed[1]*world.m[1][a]+posed[2]*world.m[2][a]+world.m[3][a];
        if(!dx_light_position::valid(p))return false;
        bulb.vertices.push_back(p);
    }
    if(bulb.vertices.size()<3)return false;
    auto& f=bulb.bounds;f.minimum=f.maximum=bulb.vertices[0];
    for(const auto& p:bulb.vertices)for(int a=0;a<3;++a){f.minimum[a]=std::min(f.minimum[a],p[a]);f.maximum[a]=std::max(f.maximum[a],p[a]);}
    for(int a=0;a<3;++a)f.center[a]=(f.minimum[a]+f.maximum[a])*.5f;
    float radius=0;for(const auto& p:bulb.vertices)radius=std::max(radius,std::sqrt(dx_light_position::distanceSquared(p,f.center)));
    if(radius<1||radius+1>24){reason="bulb clearance outside supported radius";return false;}
    output=std::move(bulb);reason="studio bulb";return true;
}
inline float clearance(const Bulb& bulb,const Point& origin) {
    float radius=0;for(const auto& p:bulb.vertices)radius=std::max(radius,std::sqrt(dx_light_position::distanceSquared(p,origin)));
    return radius+1.f;
}
}
