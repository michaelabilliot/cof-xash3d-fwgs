#include "dx_local.h"
#include "dx_lake_bed_asset.h"
#include <stdexcept>
#include <dbghelp.h>

static unsigned materials=0,freedMaterials=0,meshes=0,freedMeshes=0,draws=0;
static bool failMaterial=false,failMesh=false,failDraw=false;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static remixapi_ErrorCode REMIXAPI_CALL material(const remixapi_MaterialInfo* info,remixapi_MaterialHandle* handle) {
    if(failMaterial)return REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
    check(info&&info->pNext&&!info->emissiveTexture&&info->emissiveIntensity==0,"sediment cannot emit sky light");
    const auto* p=static_cast<const remixapi_MaterialInfoOpaqueEXT*>(info->pNext);
    check(p->sType==REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT&&!p->pNext&&p->opacityConstant==1&&p->alphaTestType==7,
        "opaque sediment blocks light coming from below the original sky floor");
    check(p->albedoConstant.x>.01f&&p->albedoConstant.x<.04f&&p->roughnessConstant>.9f&&p->metallicConstant==0,"natural wet sediment material");
    *handle=reinterpret_cast<remixapi_MaterialHandle>(uintptr_t(++materials));return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL freeMaterial(remixapi_MaterialHandle){++freedMaterials;return REMIXAPI_ERROR_CODE_SUCCESS;}
static remixapi_ErrorCode REMIXAPI_CALL mesh(const remixapi_MeshInfo* info,remixapi_MeshHandle* handle) {
    if(failMesh)return REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
    check(info&&!info->pNext&&info->surfaces_count==1,"ordinary shared x86-compatible floor mesh");
    const auto& s=info->surfaces_values[0];check(s.vertices_count==246&&s.indices_count==133*3&&s.material,"original floor footprint");
    for(size_t i=0;i<s.vertices_count;++i)check(s.vertices_values[i].position[2]==-31.75f&&s.vertices_values[i].normal[2]==1,
        "horizontal underwater floor only; no barrier or vertical walls");
    for(size_t i=0;i<s.indices_count;++i)check(s.indices_values[i]<s.vertices_count,"valid floor indices");
    *handle=reinterpret_cast<remixapi_MeshHandle>(uintptr_t(++meshes));return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL freeMesh(remixapi_MeshHandle){++freedMeshes;return REMIXAPI_ERROR_CODE_SUCCESS;}
static remixapi_ErrorCode REMIXAPI_CALL draw(const remixapi_InstanceInfo* info) {
    check(info&&info->mesh&&info->doubleSided&&!info->categoryFlags&&!info->pNext,"opaque floor blocks rays from both sides, including offscreen");
    for(int r=0;r<3;++r)for(int c=0;c<4;++c)check(info->transform.matrix[r][c]==(r==c?1.f:0.f),"original world-space floor");
    ++draws;return failDraw?REMIXAPI_ERROR_CODE_GENERAL_FAILURE:REMIXAPI_ERROR_CODE_SUCCESS;
}
static HANDLE process=GetCurrentProcess();static DWORD64 moduleBase=0;
template<class T>static T symbol(const char* name) {
    alignas(SYMBOL_INFO) char bytes[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};auto* s=reinterpret_cast<SYMBOL_INFO*>(bytes);
    s->SizeOfStruct=sizeof(SYMBOL_INFO);s->MaxNameLen=MAX_SYM_NAME;check(SymFromName(process,name,s)&&s->ModBase==moduleBase,name);
    return reinterpret_cast<T>(uintptr_t(s->Address));
}
int main(int argc,char** argv)try {
    check(argc==2,"compiled renderer argument");remixapi_Interface api{};api.CreateMaterial=material;api.DestroyMaterial=freeMaterial;
    api.CreateMesh=mesh;api.DestroyMesh=freeMesh;api.DrawInstance=draw;
    lake_bed::Asset bed;check(bed.create(api)&&bed.draw()&&bed.draw(),"floor initialized and submitted");
    check(materials==1&&meshes==1&&draws==2,"one immutable floor mesh and one draw per frame");
    failDraw=true;check(!bed.draw(),"failed floor submission reported");failDraw=false;
    bed.clear();check(!bed.ready()&&materials==freedMaterials&&meshes==freedMeshes,"floor cleanup");
    failMaterial=true;check(!bed.create(api),"failed material fallback");failMaterial=false;
    failMesh=true;check(!bed.create(api),"failed mesh fallback");failMesh=false;
    check(materials==freedMaterials&&meshes==freedMeshes,"partial initialization cleanup");
    auto dll=LoadLibraryA(argv[1]);check(dll!=nullptr,"compiled renderer load");
    SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_EXACT_SYMBOLS);
    std::string path=argv[1];check(SymInitialize(process,path.substr(0,path.find_last_of("/\\")).c_str(),FALSE)!=FALSE,"matching renderer PDB");
    moduleBase=SymLoadModuleEx(process,nullptr,argv[1],nullptr,reinterpret_cast<DWORD64>(dll),0,nullptr,0);check(moduleBase!=0,"renderer symbols");
    model_t world{};strcpy_s(world.name,"maps/c_lake.bsp");*symbol<model_t**>("g_worldmodel")=&world;
    symbol<convar_t*>("dx_remix")->value=1;auto* flags=symbol<unsigned*>("g_currentViewFlags");*flags=RF_DRAW_WORLD;
    const auto active=symbol<bool(*)()>("R_LakeBedActive");check(active(),"compiled c_lake world gate");
    symbol<convar_t*>("dx_cof_lake_reeds")->value=0;check(active(),"physical floor independent of vegetation settings");
    *flags=0;check(!active(),"interface-only views unchanged");*flags=RF_DRAW_WORLD;
    strcpy_s(world.name,"maps/c_start2.bsp");check(!active(),"other maps unchanged");strcpy_s(world.name,"maps/c_lake.bsp");
    symbol<convar_t*>("dx_remix")->value=0;check(!active(),"classic rendering unchanged");
    alignas(SYMBOL_INFO) char absentBytes[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};
    auto* absent=reinterpret_cast<SYMBOL_INFO*>(absentBytes);absent->SizeOfStruct=sizeof(SYMBOL_INFO);absent->MaxNameLen=MAX_SYM_NAME;
    check(!SymFromName(process,"g_lakeReedBlocker",absent),"old vegetation barrier completely absent from compiled renderer");
    *symbol<model_t**>("g_worldmodel")=nullptr;
    puts("PASS: opaque nonemissive horizontal sediment bed over the original underwater sky floor, 133 triangles, no walls, two-sided ray visibility, resource cleanup and compiled c_lake-only gates.");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL LAKE BED: %s\n",e.what());return 1;}
