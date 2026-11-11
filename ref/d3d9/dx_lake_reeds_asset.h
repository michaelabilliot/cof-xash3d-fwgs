#pragma once
#define REMIX_ALLOW_X86
#include "vendor/remix_c.h"
#include <array>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace lake_reeds {
// Immutable shared clumps: geometry is uploaded once; instances only carry
// transforms. Coarse distance LODs retain off-screen ray/shadow geometry.
class Asset {
public:
    struct Placement { unsigned brush,variant; std::array<float,12> matrix; };
    struct Stats { unsigned clumps=0; std::array<unsigned,3> lod{}; };
private:
    remixapi_Interface api{};
    std::array<remixapi_MaterialHandle,4> materials{};
    std::array<remixapi_MeshHandle,12> meshes{};
    std::vector<Placement> instances;
    std::vector<unsigned char> previousLod;
    struct WindPhase { float mainSin,mainCos,rippleSin,rippleCos,amplitude; };
    std::vector<WindPhase> windPhases;
    std::array<bool,64> brushMask{};
    std::string errorText;
    unsigned generation=0;
    bool loaded=false;
    Stats statistics{};
    bool fail(const char* text) { clear(); errorText=text; return false; }
public:
    void clear(bool release=true) {
        if(release) {
            if(api.DestroyMesh)for(auto mesh:meshes)if(mesh)api.DestroyMesh(mesh);
            if(api.DestroyMaterial)for(auto material:materials)if(material)api.DestroyMaterial(material);
        }
        meshes={};materials={};instances.clear();previousLod.clear();brushMask={};
        windPhases.clear();
        api={};loaded=false;statistics={};++generation;
    }
    bool ready() const { return loaded; }
    const std::string& error() const { return errorText; }
    size_t count() const { return instances.size(); }
    const std::vector<Placement>& placements() const { return instances; }
    const Stats& stats() const { return statistics; }
    bool replaces(unsigned brush) const { return loaded&&brush<brushMask.size()&&brushMask[brush]; }
    bool load(const remixapi_Interface& source,const char* filename,const std::wstring& directory) {
        clear();api=source;errorText.clear();
        if(!api.CreateMaterial||!api.DestroyMaterial||!api.CreateMesh||!api.DestroyMesh||!api.DrawInstance)
            return fail("mesh/material API unavailable");
        FILE* file=fopen(filename,"rb");if(!file)return fail("reed asset missing");
        struct Close { FILE* f;~Close(){fclose(f);} } close{file};
        fseek(file,0,SEEK_END);long length=ftell(file);rewind(file);
        if(length<72||length>128*1024*1024)return fail("invalid asset size");
        size_t remaining=size_t(length);
        const auto read=[&](void* buffer,size_t size){
            if(size>remaining||fread(buffer,1,size,file)!=size)return false;
            remaining-=size;return true;
        };
        unsigned header[6]{};
        if(!read(header,sizeof(header))||header[0]!=0x44454552||header[1]!=1||header[2]!=4||header[3]!=3||
           !header[4]||header[4]>16384||header[5]!=55834034)return fail("unsupported map/asset header");
        std::array<std::array<float,3>,4> tints{};
        if(!read(tints.data(),sizeof(tints)))return fail("truncated material tints");
        const std::wstring albedo=directory+L"/reed_albedo.dds",normal=directory+L"/reed_normal.dds",
            roughness=directory+L"/reed_roughness.dds",metallic=directory+L"/reed_metallic.dds";
        for(const auto* path:{&albedo,&normal,&roughness,&metallic})
            if(GetFileAttributesW(path->c_str())==INVALID_FILE_ATTRIBUTES)return fail("PBR texture missing");
        for(unsigned i=0;i<4;++i) {
            for(float value:tints[i])if(!std::isfinite(value)||value<.5f||value>1.1f)return fail("invalid material tint");
            remixapi_MaterialInfoOpaqueEXT opaque{};
            opaque.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
            opaque.albedoConstant={tints[i][0],tints[i][1],tints[i][2]};opaque.opacityConstant=1.f;
            opaque.roughnessConstant=.85f;opaque.metallicConstant=0.f;opaque.alphaTestType=7;
            opaque.roughnessTexture=roughness.c_str();opaque.metallicTexture=metallic.c_str();
            remixapi_MaterialInfo material{};material.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;
            material.hash=0x434c414b45000000ull+uint64_t(generation)*32+i;
            material.pNext=&opaque;material.albedoTexture=albedo.c_str();material.normalTexture=normal.c_str();
            if(api.CreateMaterial(&material,&materials[i])!=REMIXAPI_ERROR_CODE_SUCCESS||!materials[i])return fail("material creation failed");
        }
        for(unsigned i=0;i<12;++i) {
            unsigned counts[2]{};
            if(!read(counts,sizeof(counts))||!counts[0]||counts[0]>1000000||!counts[1]||counts[1]>3000000||
               counts[1]%3||uint64_t(counts[0])*32+uint64_t(counts[1])*4>remaining)return fail("invalid mesh counts");
            std::vector<remixapi_HardcodedVertex> vertices(counts[0]);
            std::vector<unsigned> indices(counts[1]);
            for(auto& vertex:vertices) {
                float values[8]{};if(!read(values,sizeof(values)))return fail("truncated vertex");
                for(float value:values)if(!std::isfinite(value)||std::abs(value)>4096)return fail("invalid vertex");
                const float n=values[3]*values[3]+values[4]*values[4]+values[5]*values[5];
                if(n<.5f||n>1.5f)return fail("invalid vertex normal");
                memcpy(vertex.position,values,12);memcpy(vertex.normal,values+3,12);
                memcpy(vertex.texcoord,values+6,8);vertex.color=0xffffffff;
            }
            if(!read(indices.data(),indices.size()*4))return fail("truncated indices");
            for(unsigned index:indices)if(index>=vertices.size())return fail("invalid mesh index");
            remixapi_MeshInfoSurfaceTriangles surface{};
            surface.vertices_values=vertices.data();surface.vertices_count=vertices.size();
            surface.indices_values=indices.data();surface.indices_count=indices.size();surface.material=materials[i/3];
            remixapi_MeshInfo mesh{};mesh.sType=REMIXAPI_STRUCT_TYPE_MESH_INFO;
            mesh.hash=0x5245454400000000ull+uint64_t(generation)*32+i;
            mesh.surfaces_values=&surface;mesh.surfaces_count=1;
            if(api.CreateMesh(&mesh,&meshes[i])!=REMIXAPI_ERROR_CODE_SUCCESS||!meshes[i])return fail("mesh creation failed");
        }
        instances.resize(header[4]);previousLod.assign(header[4],3);
        for(auto& instance:instances) {
            unsigned keys[2];
            if(!read(keys,sizeof(keys))||keys[0]<1||keys[0]>=64||keys[1]>=4||
               !read(instance.matrix.data(),48))return fail("invalid placement record");
            instance.brush=keys[0];instance.variant=keys[1];
            for(float value:instance.matrix)if(!std::isfinite(value)||std::abs(value)>65536)return fail("invalid placement transform");
            const auto& m=instance.matrix;
            const float determinant=m[0]*(m[5]*m[10]-m[6]*m[9])-m[1]*(m[4]*m[10]-m[6]*m[8])+m[2]*(m[4]*m[9]-m[5]*m[8]);
            if(determinant<.15f||determinant>3.f||m[11]<-16||m[11]>32)return fail("invalid placement scale/root");
            brushMask[instance.brush]=true;
        }
        if(remaining)return fail("unexpected trailing asset bytes");
        windPhases.reserve(instances.size());
        for(const auto& instance:instances) {
            const auto& m=instance.matrix;
            const float main=m[3]*.006f+m[7]*.004f+instance.variant*.27f;
            const float ripple=m[3]*.037f-m[7]*.029f+instance.variant*.83f;
            const float amplitude=.89f+.11f*std::sin(m[3]*.021f+m[7]*.017f+instance.variant);
            windPhases.push_back({std::sin(main),std::cos(main),std::sin(ripple),std::cos(ripple),amplitude});
        }
        loaded=true;return true;
    }
    bool draw(const float* camera,float time=0.f,float windStrength=0.f) {
        if(!loaded||!camera)return false;
        for(int a=0;a<3;++a)if(!std::isfinite(camera[a]))return false;
        const float strength=std::isfinite(windStrength)?std::max(0.f,std::min(2.f,windStrength)):0.f;
        const float t=std::isfinite(time)?time:0.f;
        // Evaluate time harmonics once. Spatial phases are precomputed when
        // loading, so a dense bed needs only a few multiplies per clump/frame.
        const float mainSin=std::sin(t*.82f),mainCos=std::cos(t*.82f);
        const float rippleSin=std::sin(t*1.37f),rippleCos=std::cos(t*1.37f);
        const float gust=.78f+.22f*std::sin(t*.23f);
        statistics={};
        for(size_t i=0;i<instances.size();++i) {
            const auto& p=instances[i];const auto& m=p.matrix;
            const float dx=m[3]-camera[0],dy=m[7]-camera[1],dz=m[11]+50.f-camera[2];
            const float d2=dx*dx+dy*dy+dz*dz;
            unsigned lod=d2<360.f*360.f?0:d2<1200.f*1200.f?1:2;
            const auto previous=previousLod[i];
            if(previous==0&&d2<400.f*400.f)lod=0;
            if(previous==1&&d2>320.f*320.f&&d2<1320.f*1320.f)lod=1;
            if(previous==2&&d2>1080.f*1080.f)lod=2;
            previousLod[i]=static_cast<unsigned char>(lod);
            remixapi_InstanceInfo instance{};instance.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;
            instance.mesh=meshes[p.variant*3+lod];instance.doubleSided=1;
            memcpy(instance.transform.matrix,m.data(),48);
            if(strength>0.f) {
                const auto& phase=windPhases[i];
                const float main=mainSin*phase.mainCos+mainCos*phase.mainSin;
                const float ripple=rippleSin*phase.rippleCos+rippleCos*phase.rippleSin;
                const float along=.014f*strength*phase.amplitude*gust*(main+.17f*ripple);
                const float across=.0035f*strength*phase.amplitude*ripple;
                // Root-anchored horizontal shear: the base does not slide,
                // while the tip moves a few units. Preserve the authored lean,
                // all distance LODs and the immutable shared mesh resources.
                instance.transform.matrix[0][2]+=(.928f*along-.371f*across)*m[10];
                instance.transform.matrix[1][2]+=(.371f*along+.928f*across)*m[10];
            }
            if(api.DrawInstance(&instance)!=REMIXAPI_ERROR_CODE_SUCCESS)return false;
            ++statistics.clumps;++statistics.lod[lod];
        }
        return true;
    }
};
}
