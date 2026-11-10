#include "dx_local.h"
#include "dx_audi_replacement.h"
#include <stdexcept>
#include <fstream>
extern "C" { ref_api_t gEngfuncs{}; }
static unsigned created=0, destroyed=0, meshDestroyed=0;
static uint64_t triangleCount=0;
static uint64_t glazingTriangles=0;
static unsigned windowPaneCount=0;
static remixapi_MaterialHandle windowMaterial=nullptr;
static void check(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
static void print(const char*,...) {}
static remixapi_ErrorCode REMIXAPI_CALL material(const remixapi_MaterialInfo* info,remixapi_MaterialHandle* out) {
    check(info && info->sType==REMIXAPI_STRUCT_TYPE_MATERIAL_INFO && info->pNext,"material descriptor");
    auto type=*static_cast<const remixapi_StructType*>(info->pNext);
    if(type==REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT) {
        auto p=static_cast<const remixapi_MaterialInfoOpaqueEXT*>(info->pNext);
        check(p->alphaTestType==7 && p->opacityConstant==1,"opaque surfaces must not discard ray hits");
    } else check(type==REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_TRANSLUCENT_EXT,"glass descriptor");
    if(info->albedoTexture)check(GetFileAttributesW(info->albedoTexture)!=INVALID_FILE_ATTRIBUTES,"texture exists");
    *out=reinterpret_cast<remixapi_MaterialHandle>(uintptr_t(++created));
    if(type==REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_TRANSLUCENT_EXT &&
       static_cast<const remixapi_MaterialInfoTranslucentEXT*>(info->pNext)->thinWallThickness_value>.2f)
        windowMaterial=*out;
    return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL destroyMaterial(remixapi_MaterialHandle) { ++destroyed; return REMIXAPI_ERROR_CODE_SUCCESS; }
static remixapi_ErrorCode REMIXAPI_CALL destroyMesh(remixapi_MeshHandle) { ++meshDestroyed; return REMIXAPI_ERROR_CODE_SUCCESS; }
static remixapi_ErrorCode REMIXAPI_CALL createMesh(const remixapi_MeshInfo* info,remixapi_MeshHandle* out) {
    check(info && !info->pNext && info->surfaces_count>1,"x86-compatible static mesh");
    float lo[3]={FLT_MAX,FLT_MAX,FLT_MAX},hi[3]={-FLT_MAX,-FLT_MAX,-FLT_MAX};
    for(unsigned s=0;s<info->surfaces_count;++s) {
        const auto& surface=info->surfaces_values[s];check(surface.material && !surface.skinning_hasvalue,"immutable material surface");
        for(uint64_t i=0;i<surface.indices_count;++i)check(surface.indices_values[i]<surface.vertices_count,"index range");
        for(uint64_t i=0;i<surface.vertices_count;++i) {
            const auto& v=surface.vertices_values[i];float len=0;
            for(int a=0;a<3;++a) { check(std::isfinite(v.position[a]),"finite position");lo[a]=std::min(lo[a],v.position[a]);hi[a]=std::max(hi[a],v.position[a]);len+=v.normal[a]*v.normal[a]; }
            check(len>.99f && len<1.01f,"unit imported normals");check(v.color==0xffffffff,"unmodified vertex tint");
        }
        triangleCount+=surface.indices_count/3;
        if(surface.material==windowMaterial) {
            glazingTriangles+=surface.indices_count/3;
            std::vector<uint32_t> parent(size_t(surface.vertices_count));
            for(uint32_t i=0;i<parent.size();++i)parent[i]=i;
            auto root=[&](uint32_t i) { while(parent[i]!=i)i=parent[i];return i; };
            for(uint64_t i=0;i<surface.indices_count;i+=3) {
                const auto a=root(surface.indices_values[i]);
                parent[root(surface.indices_values[i+1])]=a;
                parent[root(surface.indices_values[i+2])]=a;
            }
            for(uint32_t i=0;i<parent.size();++i)if(root(i)==i)++windowPaneCount;
        }
    }
    check(windowPaneCount==6 && glazingTriangles>0,"six connected window panes exist");
    check(triangleCount-glazingTriangles==255283,"all original source triangles preserved");
    check(fabsf((hi[0]-lo[0])-209.1f)<.01f && fabsf(lo[2]+33.55998f)<.01f,"native idle-pose axis, length and ground contact preserved");
    *out=reinterpret_cast<remixapi_MeshHandle>(uintptr_t(1));return REMIXAPI_ERROR_CODE_SUCCESS;
}
namespace audi_replacement {
struct TestAccess {
    static void bind(Asset& value) {
        value.module=LoadLibraryA("d3d9.dll");value.api.CreateMaterial=material;value.api.DestroyMaterial=destroyMaterial;
        value.api.CreateMesh=createMesh;value.api.DestroyMesh=destroyMesh;
    }
    static bool read(Asset& value,const char* filename){return value.readAsset(filename);}
};
}
int main() try {
    gEngfuncs.Con_Printf=print;
    audi_replacement::Asset value; audi_replacement::TestAccess::bind(value);
    check(audi_replacement::TestAccess::read(value,"CryOfFear/models/cofr/audi200/audi200.bin"),"installed asset parsed");
    value.reset();check(created==destroyed && meshDestroyed==1,"mesh and all materials released");
    const char* invalid="audi-invalid-test.bin";
    {std::ofstream out(invalid,std::ios::binary);out.write("CAUD",4);}
    audi_replacement::TestAccess::bind(value);
    const bool rejected=!audi_replacement::TestAccess::read(value,invalid);remove(invalid);
    check(rejected,"truncated asset falls back without mesh submission");
    printf("PASS: 255283 source triangles + %llu glazing triangles (%u panes), %u PBR materials, DDS paths, normals, scale/ground contact, opaque alpha state, cleanup and malformed-file fallback. No GPU/window/game.\n",glazingTriangles,windowPaneCount,created);
    return 0;
} catch(const std::exception& e) { fprintf(stderr,"FAIL AUDI: %s\n",e.what());return 1; }
