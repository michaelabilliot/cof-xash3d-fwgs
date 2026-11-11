#include "dx_local.h"
#include "dx_lake_reeds_asset.h"
#include <stdexcept>
#include <fstream>
#include <set>
#include <limits>
#include <dbghelp.h>

static unsigned materialsCreated=0,materialsDestroyed=0,meshesCreated=0,meshesDestroyed=0,draws=0;
static bool failMesh=false,failDraw=false;
static std::vector<unsigned> triangleCounts;
static std::vector<std::array<float,12>> drawTransforms;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static HANDLE process=GetCurrentProcess();
static DWORD64 moduleBase=0;
template<class T> static T symbol(const char* name) {
    alignas(SYMBOL_INFO) char bytes[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};
    auto* info=reinterpret_cast<SYMBOL_INFO*>(bytes);info->SizeOfStruct=sizeof(SYMBOL_INFO);info->MaxNameLen=MAX_SYM_NAME;
    check(SymFromName(process,name,info)&&info->ModBase==moduleBase,name);
    return reinterpret_cast<T>(uintptr_t(info->Address));
}
static remixapi_ErrorCode REMIXAPI_CALL material(const remixapi_MaterialInfo* info,remixapi_MaterialHandle* handle) {
    check(info&&info->pNext&&info->albedoTexture&&info->normalTexture,"albedo and normal PBR maps");
    const auto* opaque=static_cast<const remixapi_MaterialInfoOpaqueEXT*>(info->pNext);
    check(opaque->roughnessTexture&&opaque->metallicTexture&&opaque->alphaTestType==7&&opaque->opacityConstant==1.f,"opaque 3D vegetation with full PBR maps");
    check(!info->emissiveTexture&&info->emissiveIntensity==0,"vegetation receives scene lighting");
    *handle=reinterpret_cast<remixapi_MaterialHandle>(uintptr_t(++materialsCreated));return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL destroyMaterial(remixapi_MaterialHandle){++materialsDestroyed;return REMIXAPI_ERROR_CODE_SUCCESS;}
static remixapi_ErrorCode REMIXAPI_CALL mesh(const remixapi_MeshInfo* info,remixapi_MeshHandle* handle) {
    if(failMesh)return REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
    check(info&&info->surfaces_count==1,"one immutable mesh per clump variant/LOD");
    const auto& s=info->surfaces_values[0];check(s.indices_count>0&&s.indices_count%3==0&&s.material,"mesh topology/material");
    triangleCounts.push_back(unsigned(s.indices_count/3));
    for(size_t i=0;i<s.vertices_count;++i){const auto& v=s.vertices_values[i];
        check(v.position[2]>-10&&v.position[2]<140,"root-aligned realistic reed height");
        check(v.color==0xffffffff,"no baked lighting or forced emission");}
    *handle=reinterpret_cast<remixapi_MeshHandle>(uintptr_t(++meshesCreated));return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL destroyMesh(remixapi_MeshHandle){++meshesDestroyed;return REMIXAPI_ERROR_CODE_SUCCESS;}
static remixapi_ErrorCode REMIXAPI_CALL draw(const remixapi_InstanceInfo* info) {
    check(info&&info->mesh&&info->doubleSided,"shared double-sided 3D instance");
    check(std::isfinite(info->transform.matrix[0][3]),"finite placement");
    check(info->pNext==nullptr,"installed x86 bridge uses ordinary instances");
    std::array<float,12> transform{};memcpy(transform.data(),info->transform.matrix,48);
    drawTransforms.push_back(transform);
    ++draws;return failDraw?REMIXAPI_ERROR_CODE_GENERAL_FAILURE:REMIXAPI_ERROR_CODE_SUCCESS;
}
int main(int argc,char** argv) try {
    check(argc==3,"asset directory and compiled renderer arguments");
    const std::string folder=argv[1];const std::wstring directory(folder.begin(),folder.end());
    const std::string filename=folder+"/reeds.bin";
    remixapi_Interface api{};api.CreateMaterial=material;api.DestroyMaterial=destroyMaterial;
    api.CreateMesh=mesh;api.DestroyMesh=destroyMesh;api.DrawInstance=draw;
    lake_reeds::Asset asset;
    check(asset.load(api,filename.c_str(),directory),asset.error().c_str());
    check(asset.count()==5595&&materialsCreated==4&&meshesCreated==12,"complete supplied asset and shared resource budget");
    for(unsigned i=0;i<4;++i)check(triangleCounts[i*3]>triangleCounts[i*3+1]*3&&triangleCounts[i*3+1]>triangleCounts[i*3+2]*3,"geometric distance LOD budget");
    const unsigned stems[4]={7,9,11,8};
    for(unsigned i=0;i<4;++i)check(triangleCounts[i*3]==stems[i]*20000&&triangleCounts[i*3+2]==stems[i]*900,"dense clump geometry retained at each distance LOD");
    unsigned stemCount=0;
    unsigned tall=0;
    std::set<unsigned> patches,variants;std::set<int> headings,heights;
    for(const auto& p:asset.placements()) {
        patches.insert(p.brush);variants.insert(p.variant);stemCount+=stems[p.variant];
        if(p.matrix[10]>=1.18f)++tall;
        headings.insert(int(std::atan2(p.matrix[4],p.matrix[0])*100));heights.insert(int(p.matrix[10]*100));
    }
    check(tall>asset.count()*8/10,"tall overlapping canopy at normal player eye height");
    check(stemCount==48986,"requested density increase: 48986 3D stems");
    check(patches.size()==19&&variants.size()==4&&headings.size()>300&&heights.size()>30,"natural placement variation across every original reed patch");
    check(!asset.replaces(0)&&!asset.replaces(34)&&asset.replaces(2)&&asset.replaces(31),"water/world and unrelated brushes preserved");
    float farCamera[3]={100000,100000,10000};
    check(asset.draw(farCamera)&&draws==5595&&asset.stats().lod[2]==5595,"distant/offscreen shadow geometry retained with far LOD");
    const auto p=asset.placements().front();float camera[3]={p.matrix[3],p.matrix[7],p.matrix[11]+50};
    check(asset.draw(camera)&&asset.stats().lod[0]>0&&asset.stats().clumps==5595,"near LOD and all clumps submitted");
    const auto capture=[&](const float* view,float time,float wind) {
        drawTransforms.clear();check(asset.draw(view,time,wind),"wind instance submission");
        check(drawTransforms.size()==asset.count(),"wind keeps every plant visible");
        return drawTransforms;
    };
    const auto still=capture(camera,20.f,0.f);
    const auto breeze=capture(camera,20.f,1.f);
    const auto later=capture(camera,22.f,1.f);
    const auto paused=capture(camera,20.f,1.f);
    const auto farBreeze=capture(farCamera,20.f,1.f);
    unsigned moving=0;std::set<int> phases;
    for(size_t i=0;i<asset.count();++i) {
        const auto& base=asset.placements()[i].matrix;
        check(still[i]==base,"wind off restores exact authored placement");
        check(paused[i]==breeze[i],"paused game time freezes the wind");
        check(farBreeze[i]==breeze[i],"wind phase stays continuous across geometric LOD changes");
        for(unsigned j=0;j<12;++j) {
            check(std::isfinite(breeze[i][j]),"finite animated transform");
            if(j!=2&&j!=6)check(breeze[i][j]==base[j],"wind anchors roots and preserves height/orientation");
        }
        const float x=breeze[i][2]-base[2],y=breeze[i][6]-base[6];
        check(std::sqrt(x*x+y*y)*100.f<4.f,"gentle breeze limits tip movement to four units at local height 100");
        if(breeze[i]!=later[i])++moving;
        phases.insert(int(x*100000));
    }
    check(moving>asset.count()*9/10&&phases.size()>200,"spatially varied continuous breeze reaches the full reed bed");
    check(capture(camera,20.f,-1.f)==still,"negative wind strength safely disables sway");
    check(capture(camera,20.f,std::numeric_limits<float>::quiet_NaN())==still,"invalid strength keeps plants visible and still");
    const auto strong=capture(camera,20.f,2.f);
    check(capture(camera,20.f,100.f)==strong,"excessive wind strength is bounded");
    check(materialsCreated==4&&meshesCreated==12,"wind reuses all shared meshes and PBR materials");
    failDraw=true;check(!asset.draw(camera),"draw failure allows original-vegetation fallback");failDraw=false;
    asset.clear();check(materialsCreated==materialsDestroyed&&meshesCreated==meshesDestroyed&&!asset.replaces(2),"map reset frees all resources and clears replacement mask");
    check(!asset.load(api,"missing-reed-asset.bin",directory),"missing asset fallback");
    failMesh=true;check(!asset.load(api,filename.c_str(),directory),"mesh API failure fallback");failMesh=false;
    check(materialsCreated==materialsDestroyed&&meshesCreated==meshesDestroyed,"partial initialization cleanup");
    const std::string invalid=folder+"/invalid-reed-test.bin";
    {std::ifstream input(filename,std::ios::binary);char header[72]{};input.read(header,sizeof(header));
     std::ofstream output(invalid,std::ios::binary);output.write(header,sizeof(header));}
    check(!asset.load(api,invalid.c_str(),directory),"truncated mesh fallback");remove(invalid.c_str());
    check(materialsCreated==materialsDestroyed&&meshesCreated==meshesDestroyed,"truncated asset cleanup");
    // Check the actual compiled brush suppression function. Only replace the
    // exact c_lake reed/invisible brushes after successful mesh submission.
    auto dll=LoadLibraryA(argv[2]);check(dll!=nullptr,"compiled renderer load");
    SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_EXACT_SYMBOLS);
    const std::string dllPath=argv[2];const auto slash=dllPath.find_last_of("/\\");
    check(SymInitialize(process,dllPath.substr(0,slash).c_str(),FALSE)!=FALSE,"symbols");
    moduleBase=SymLoadModuleEx(process,nullptr,argv[2],nullptr,reinterpret_cast<DWORD64>(dll),0,nullptr,0);
    check(moduleBase!=0,"matching renderer PDB");
    auto* production=symbol<lake_reeds::Asset*>("g_lakeReeds");
    check(production->load(api,filename.c_str(),directory),"production asset initialized");
    symbol<convar_t*>("dx_remix")->value=1;
    symbol<convar_t*>("dx_cof_lake_reeds")->value=1;
    auto* submitted=symbol<bool*>("g_lakeReedsDrawn");*submitted=true;
    model_t world{},brush{};strcpy_s(world.name,"maps/c_lake.bsp");strcpy_s(brush.name,"*2");
    *symbol<model_t**>("g_worldmodel")=&world;
    const auto waterPeriod=symbol<float(*)(const texture_t*)>("R_WaterTexturePeriod");
    texture_t lakeWater{};strcpy_s(lakeWater.name,"!sewage");
    check(waterPeriod(&lakeWater)==1024.f,"lake water spans 16 times the original texture period");
    strcpy_s(world.name,"maps/c_start2.bsp");check(waterPeriod(&lakeWater)==64.f,"other maps keep original water UV scale");
    strcpy_s(world.name,"maps/c_lake.bsp");strcpy_s(lakeWater.name,"!water");
    check(waterPeriod(&lakeWater)==64.f&&waterPeriod(nullptr)==64.f,"other water materials and null textures preserved");
    strcpy_s(lakeWater.name,"!sewage");symbol<convar_t*>("dx_remix")->value=0;
    check(waterPeriod(&lakeWater)==64.f,"classic raster water keeps original scale");
    symbol<convar_t*>("dx_remix")->value=1;
    msurface_t surfaces[2]{};mtexinfo_t info[2]{};texture_t textures[2]{};
    strcpy_s(textures[0].name,"{c2_vass");strcpy_s(textures[1].name,"{invisible");
    for(int i=0;i<2;++i){info[i].texture=&textures[i];surfaces[i].texinfo=&info[i];}
    brush.surfaces=surfaces;brush.nummodelsurfaces=2;
    const auto replaces=symbol<bool(*)(const model_t*)>("R_LakeReedsReplacesBrush");
    check(replaces(&brush),"compiled renderer replaces original reed ribbons");
    *submitted=false;check(!replaces(&brush),"failed submission preserves native vegetation");*submitted=true;
    strcpy_s(world.name,"maps/c_start2.bsp");check(!replaces(&brush),"other maps preserved");strcpy_s(world.name,"maps/c_lake.bsp");
    strcpy_s(brush.name,"*34");check(!replaces(&brush),"lake water preserved");strcpy_s(brush.name,"*2");
    strcpy_s(textures[1].name,"stone");check(!replaces(&brush),"mixed/unrelated brush geometry preserved");
    symbol<convar_t*>("dx_cof_lake_reeds")->value=0;check(!replaces(&brush),"user can restore native vegetation");
    production->clear();*symbol<model_t**>("g_worldmodel")=nullptr;
    check(materialsCreated==materialsDestroyed&&meshesCreated==meshesDestroyed,"production resource cleanup");
    puts("PASS: compiled c_lake brush replacement rejects other maps, lake water, mixed geometry, disabled settings and failed submissions.");
    puts("PASS: gentle spatially varied wind, fixed roots, paused time, continuous LODs, bounded strength and no per-frame mesh/material creation.");
    puts("PASS: real reed clumps/PBR/LODs, 48986 stems in 5595 placements across 19 patches, natural variations, retained offscreen geometry, native brush mask, resource cleanup and missing/truncated/API failure fallbacks.");
    return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL REEDS: %s\n",e.what());return 1;}
