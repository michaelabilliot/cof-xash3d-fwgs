#pragma once
#define REMIX_ALLOW_X86
#include "vendor/remix_c.h"
#include "dx_lake_bed_geometry.h"
#include <vector>
#include <string>

namespace lake_bed {
class Asset {
    remixapi_Interface api{};
    remixapi_MaterialHandle material=nullptr;
    remixapi_MeshHandle mesh=nullptr;
    unsigned generation=0;
    std::string message;
    bool fail(const char* why){clear();message=why;return false;}
public:
    bool ready()const{return mesh!=nullptr;}
    const std::string& error()const{return message;}
    void clear(bool release=true) {
        if(release) {
            if(mesh&&api.DestroyMesh)api.DestroyMesh(mesh);
            if(material&&api.DestroyMaterial)api.DestroyMaterial(material);
        }
        mesh=nullptr;material=nullptr;api={};++generation;
    }
    bool create(const remixapi_Interface& source) {
        clear();api=source;message.clear();
        if(!api.CreateMaterial||!api.DestroyMaterial||!api.CreateMesh||!api.DestroyMesh||!api.DrawInstance)
            return fail("required lake bed API unavailable");
        remixapi_MaterialInfoOpaqueEXT opaque{};opaque.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
        // Wet sediment, not an emissive sky or an artificial black curtain.
        opaque.albedoConstant={.025f,.019f,.012f};opaque.opacityConstant=1.f;
        opaque.roughnessConstant=.98f;opaque.metallicConstant=0;opaque.alphaTestType=7;
        opaque.useDrawCallAlphaState=0;
        remixapi_MaterialInfo mat{};mat.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;mat.pNext=&opaque;
        mat.hash=0x4c414b4542454400ull+generation;
        if(api.CreateMaterial(&mat,&material)!=REMIXAPI_ERROR_CODE_SUCCESS||!material)return fail("lake bed material creation failed");
        std::vector<remixapi_HardcodedVertex> vertices(sizeof(positions)/sizeof(positions[0]));
        for(size_t i=0;i<vertices.size();++i) {
            auto& v=vertices[i];for(int a=0;a<3;++a)v.position[a]=positions[i][a];
            v.normal[2]=1;v.color=0xffffffff;
        }
        remixapi_MeshInfoSurfaceTriangles surface{};surface.vertices_values=vertices.data();surface.vertices_count=vertices.size();
        surface.indices_values=indices;surface.indices_count=sizeof(indices)/sizeof(indices[0]);surface.material=material;
        remixapi_MeshInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MESH_INFO;info.hash=0x4c414b45464c5200ull+generation;
        info.surfaces_values=&surface;info.surfaces_count=1;
        if(api.CreateMesh(&info,&mesh)!=REMIXAPI_ERROR_CODE_SUCCESS||!mesh)return fail("lake bed mesh creation failed");
        return true;
    }
    bool draw()const {
        if(!ready())return false;
        remixapi_InstanceInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;info.mesh=mesh;info.doubleSided=1;
        info.transform.matrix[0][0]=info.transform.matrix[1][1]=info.transform.matrix[2][2]=1;
        return api.DrawInstance(&info)==REMIXAPI_ERROR_CODE_SUCCESS;
    }
};
}
