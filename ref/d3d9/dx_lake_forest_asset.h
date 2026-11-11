#pragma once
#define REMIX_ALLOW_X86
#include "vendor/remix_c.h"
#include <array>
#include <vector>
#include <string>
#include <cstdio>
#include <cmath>
#include <cstring>

namespace lake_forest {
// Trees, supplied undergrowth/shrubs and floor use bounded immutable batches.
// This avoids the x86 bridge's per-mesh payload limit and per-plant API calls.
class Asset {
    remixapi_Interface api{};
    std::array<remixapi_MaterialHandle,22> materials{};
    std::vector<remixapi_MeshHandle> meshes;
    unsigned generation=0,trees=0,understory=0,triangles=0,nativeBushes=0,nativeModels=0;
    std::string message;
    bool fail(const char* reason) { clear();message=reason;return false; }
public:
    void clear(bool release=true) {
        if(release) {
            if(api.DestroyMesh)for(auto mesh:meshes)if(mesh)api.DestroyMesh(mesh);
            if(api.DestroyMaterial)for(auto m:materials)if(m)api.DestroyMaterial(m);
        }
        meshes.clear();materials={};api={};trees=understory=triangles=nativeBushes=nativeModels=0;++generation;
    }
    bool ready() const { return !meshes.empty(); }
    unsigned batchCount() const { return unsigned(meshes.size()); }
    unsigned treeCount() const { return trees; }
    unsigned understoryCount() const { return understory; }
    unsigned triangleCount() const { return triangles; }
    unsigned nativeBushCount() const { return nativeBushes; }
    unsigned nativeModelMask() const { return nativeModels; }
    const std::string& error() const { return message; }
    bool load(const remixapi_Interface& source,const char* filename,const std::wstring& directory) {
        clear();message.clear();api=source;
        if(!api.CreateMaterial||!api.DestroyMaterial||!api.CreateMesh||!api.DestroyMesh||!api.DrawInstance)
            return fail("mesh/material API unavailable");
        FILE* file=fopen(filename,"rb");if(!file)return fail("forest asset missing");
        struct Close {FILE* file;~Close(){fclose(file);}} close{file};
        fseek(file,0,SEEK_END);const long size=ftell(file);rewind(file);
        if(size<60||size>384*1024*1024)return fail("invalid forest file size");
        size_t remaining=static_cast<size_t>(size);
        const auto read=[&](void* to,size_t bytes) {
            if(bytes>remaining||fread(to,1,bytes,file)!=bytes)return false;
            remaining-=bytes;return true;
        };
        unsigned header[6]{};
        if(!read(header,sizeof(header))||header[0]!=0x54535246||
           !((header[1]==1&&header[2]==3)||((header[1]==2||header[1]==3)&&header[2]==10)||(header[1]==4&&header[2]==13)||(header[1]==5&&header[2]==22))||
           header[3]<100||header[3]>10000||header[4]>20000)return fail("unsupported forest header");
        unsigned shrubs[2]{};
        if(header[1]>=2&&(!read(shrubs,sizeof(shrubs))||shrubs[0]>header[4]||
           shrubs[1]!=(shrubs[0]?(1u<<30):0u)))return fail("invalid native shrub replacement mask");
        unsigned native[2]{};
        if(header[1]>=3&&(!read(native,sizeof(native))||
           !(header[1]==5?(native[0]==16383&&native[1]==154):header[1]==4?(native[0]==1023&&native[1]==108):
             (((native[0]==3||native[0]==7)&&native[1]==19)||(native[0]==31&&native[1]==53)))))
            return fail("invalid native plant model replacement mask");
        const unsigned surfaceCount=header[2];
        struct Maps {const wchar_t* albedo;const wchar_t* normal;const wchar_t* roughness;bool cutout;};
        const std::array<Maps,22> maps={{
            {L"foliage.dds",nullptr,nullptr,true},
            {L"bark.dds",L"bark_normal.dds",nullptr,false},
            {L"ground.dds",L"ground_normal.dds",nullptr,false},
            {L"undergrowth.dds",L"undergrowth_normal.dds",L"undergrowth_roughness.dds",true},
            {L"bush_leaf.dds",L"bush_leaf_normal.dds",L"bush_leaf_roughness.dds",true},
            {L"bush_leaf2.dds",L"bush_leaf2_normal.dds",L"bush_leaf2_roughness.dds",true},
            {L"bush_bark.dds",L"bush_bark_normal.dds",L"bush_bark_roughness.dds",false},
            {L"bush_leaf_v2.dds",L"bush_leaf_normal.dds",L"bush_leaf_roughness.dds",true},
            {L"bush_leaf_v3.dds",L"bush_leaf_normal.dds",L"bush_leaf_roughness.dds",true},
            {L"bush_leaf_v4.dds",L"bush_leaf_normal.dds",L"bush_leaf_roughness.dds",true},
            {L"spruce_bark.dds",L"spruce_bark_normal.dds",L"spruce_bark_roughness.dds",false},
            {L"spruce_needles.dds",L"spruce_needles_normal.dds",L"spruce_needles_roughness.dds",true},
            {L"spruce_twigs.dds",L"spruce_twigs_normal.dds",L"spruce_twigs_roughness.dds",true},
            {L"dead_0.dds",L"dead_0_normal.dds",nullptr,false},
            {L"dead_1.dds",L"dead_1_normal.dds",nullptr,false},
            {L"dead_2.dds",L"dead_2_normal.dds",nullptr,false},
            {L"dead_3.dds",L"dead_3_normal.dds",nullptr,true},
            {L"dead_4.dds",L"dead_4_normal.dds",nullptr,true},
            {L"dead_5.dds",L"dead_5_normal.dds",nullptr,true},
            {L"dead_6.dds",L"dead_6_normal.dds",nullptr,true},
            {L"dead_7.dds",L"dead_7_normal.dds",nullptr,false},
            {L"dead_8.dds",L"dead_8_normal.dds",nullptr,false}
        }};
        for(unsigned i=0;i<surfaceCount;++i) {
            const auto& map=maps[i];
            std::wstring albedo=directory+L"/"+map.albedo;
            std::wstring normal=map.normal?directory+L"/"+map.normal:L"";
            std::wstring roughness=map.roughness?directory+L"/"+map.roughness:L"";
            for(const auto* path:{&albedo,&normal,&roughness})
                if(!path->empty()&&GetFileAttributesW(path->c_str())==INVALID_FILE_ATTRIBUTES)
                    return fail("forest PBR texture missing");
            remixapi_MaterialInfoOpaqueEXT opaque{};opaque.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
            opaque.albedoConstant={1.f,1.f,1.f};opaque.opacityConstant=1.f;
            opaque.roughnessConstant=i<3?(i==0?.85f:.95f):i>=13?.95f:1.f;opaque.metallicConstant=0.f;
            opaque.roughnessTexture=roughness.empty()?nullptr:roughness.c_str();
            // AlphaTestType matches VkCompareOp: Greater=4, Always=7.
            opaque.alphaTestType=map.cutout?4:7;opaque.alphaReferenceValue=map.cutout?96:0;
            remixapi_MaterialInfo material{};material.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;
            material.hash=0x4c414b45464f0000ull+uint64_t(generation)*32+i;
            material.pNext=&opaque;material.albedoTexture=albedo.c_str();
            material.normalTexture=normal.empty()?nullptr:normal.c_str();
            material.filterMode=1;material.wrapModeU=material.wrapModeV=map.cutout?0:1;
            if(api.CreateMaterial(&material,&materials[i])!=REMIXAPI_ERROR_CODE_SUCCESS||!materials[i])
                return fail("forest material creation failed");
        }
        unsigned totalTriangles=0;
        for(unsigned i=0;i<surfaceCount;++i) {
            unsigned counts[3]{};
            if(!read(counts,sizeof(counts))||counts[0]!=i||!counts[1]||counts[1]>2000000||
               !counts[2]||counts[2]>6000000||counts[2]%3||
               uint64_t(counts[1])*32+uint64_t(counts[2])*4>remaining)return fail("invalid forest surface counts");
            std::vector<remixapi_HardcodedVertex> vertices(counts[1]);
            std::vector<unsigned> indices(counts[2]);
            for(auto& v:vertices) {
                float values[8]{};if(!read(values,sizeof(values)))return fail("truncated forest vertices");
                for(float value:values)if(!std::isfinite(value)||std::abs(value)>32768)return fail("invalid forest vertex");
                const float n=values[3]*values[3]+values[4]*values[4]+values[5]*values[5];
                if(n<.9f||n>1.1f)return fail("invalid forest normal");
                memcpy(v.position,values,12);memcpy(v.normal,values+3,12);memcpy(v.texcoord,values+6,8);
                v.color=0xffffffff;
            }
            if(!read(indices.data(),indices.size()*4))return fail("truncated forest indices");
            for(unsigned index:indices)if(index>=vertices.size())return fail("invalid forest index");
            // At most ~6 MiB of indexed vertex data per ordinary CreateMesh.
            // Identical seam vertices are copied, never simplified or moved.
            constexpr unsigned maxVertices=128000,maxIndices=384000;
            std::vector<unsigned> remap(vertices.size(),~0u),used;
            std::vector<remixapi_HardcodedVertex> batchVertices;
            std::vector<unsigned> batchIndices;
            batchVertices.reserve(maxVertices);batchIndices.reserve(maxIndices);used.reserve(maxVertices);
            const auto flush=[&]() {
                if(batchIndices.empty())return true;
                remixapi_MeshInfoSurfaceTriangles surface{};
                surface.vertices_values=batchVertices.data();surface.vertices_count=batchVertices.size();
                surface.indices_values=batchIndices.data();surface.indices_count=batchIndices.size();surface.material=materials[i];
                remixapi_MeshInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MESH_INFO;
                info.hash=0x464f524553540000ull+uint64_t(generation)*256+meshes.size();
                info.surfaces_values=&surface;info.surfaces_count=1;
                remixapi_MeshHandle mesh=nullptr;
                const auto status=api.CreateMesh(&info,&mesh);
                if(status!=REMIXAPI_ERROR_CODE_SUCCESS||!mesh) {
                    if(mesh)api.DestroyMesh(mesh);
                    message="forest batch creation failed (API "+std::to_string(unsigned(status))+")";
                    return false;
                }
                meshes.push_back(mesh);
                for(unsigned index:used)remap[index]=~0u;
                used.clear();batchVertices.clear();batchIndices.clear();return true;
            };
            for(size_t at=0;at<indices.size();at+=3) {
                if(batchVertices.size()+3>maxVertices||batchIndices.size()+3>maxIndices)
                    if(!flush()) {const auto reason=message;return fail(reason.c_str());}
                for(unsigned corner=0;corner<3;++corner) {
                    const unsigned index=indices[at+corner];
                    if(remap[index]==~0u) {
                        remap[index]=unsigned(batchVertices.size());used.push_back(index);
                        batchVertices.push_back(vertices[index]);
                    }
                    batchIndices.push_back(remap[index]);
                }
            }
            if(!flush()) {const auto reason=message;return fail(reason.c_str());}
            totalTriangles+=counts[2]/3;
        }
        if(remaining)return fail("unexpected forest file data");
        trees=header[3];understory=header[4];triangles=totalTriangles;nativeBushes=shrubs[0];nativeModels=native[0];return true;
    }
    bool draw() const {
        if(!ready())return false;
        remixapi_InstanceInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;
        info.doubleSided=1;
        info.transform.matrix[0][0]=info.transform.matrix[1][1]=info.transform.matrix[2][2]=1.f;
        bool success=true;
        for(auto mesh:meshes) {
            info.mesh=mesh;
            if(api.DrawInstance(&info)!=REMIXAPI_ERROR_CODE_SUCCESS)success=false;
        }
        return success;
    }
};
}
