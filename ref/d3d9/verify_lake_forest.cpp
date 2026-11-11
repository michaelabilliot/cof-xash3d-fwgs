#include "dx_local.h"
#include "dx_lake_forest_asset.h"
#include <dbghelp.h>
#include <stdexcept>
#include <fstream>

static unsigned madeMaterials=0,freedMaterials=0,madeMeshes=0,freedMeshes=0,draws=0;
static unsigned exportedTriangles=0,rejectMeshAfter=0;
static bool rejectMesh=false,rejectDraw=false;
static void* mixedHeader=nullptr;
static void* mixedExtra(int,model_t*) {return mixedHeader;}
static void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
static remixapi_ErrorCode REMIXAPI_CALL material(const remixapi_MaterialInfo* info,remixapi_MaterialHandle* result) {
    check(info&&info->albedoTexture&&info->pNext&&!info->emissiveTexture&&info->emissiveIntensity==0,"textured forest receives scene lighting");
    const auto* opaque=static_cast<const remixapi_MaterialInfoOpaqueEXT*>(info->pNext);
    const bool foliage=wcsstr(info->albedoTexture,L"foliage.dds")||
        wcsstr(info->albedoTexture,L"undergrowth.dds")||wcsstr(info->albedoTexture,L"bush_leaf")||
        wcsstr(info->albedoTexture,L"spruce_needles")||wcsstr(info->albedoTexture,L"spruce_twigs")||
        wcsstr(info->albedoTexture,L"dead_3.dds")||wcsstr(info->albedoTexture,L"dead_4.dds")||
        wcsstr(info->albedoTexture,L"dead_5.dds")||wcsstr(info->albedoTexture,L"dead_6.dds");
    check(opaque->opacityConstant==1.f&&opaque->metallicConstant==0.f&&!opaque->useDrawCallAlphaState,"nonmetal forest with authored opacity");
    check(opaque->alphaTestType==(foliage?4:7),"foliage cutouts and opaque bark/soil");
    if(foliage)check(opaque->alphaReferenceValue==96,"foliage transparency threshold");
    if(!wcsstr(info->albedoTexture,L"foliage.dds"))check(info->normalTexture!=nullptr,"supplied plant/bark/soil PBR normals retained");
    if(wcsstr(info->albedoTexture,L"undergrowth")||wcsstr(info->albedoTexture,L"bush_")||wcsstr(info->albedoTexture,L"spruce_"))
        check(opaque->roughnessTexture!=nullptr&&opaque->roughnessConstant==1.f,"supplied shrub roughness bound without double scaling");
    *result=reinterpret_cast<remixapi_MaterialHandle>(uintptr_t(++madeMaterials));return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL freeMaterial(remixapi_MaterialHandle){++freedMaterials;return REMIXAPI_ERROR_CODE_SUCCESS;}
static remixapi_ErrorCode REMIXAPI_CALL mesh(const remixapi_MeshInfo* info,remixapi_MeshHandle* result) {
    if(rejectMesh||(rejectMeshAfter&&madeMeshes>=rejectMeshAfter))return REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
    check(info&&info->surfaces_count==1,"one bounded immutable surface per bridge mesh");
    unsigned triangles=0;
    for(unsigned i=0;i<info->surfaces_count;++i) {
        const auto& s=info->surfaces_values[i];check(s.material&&s.vertices_count&&s.indices_count,"complete forest surface");
        check(s.vertices_count<=128000&&s.indices_count<=384000,"bounded bridge payload without per-plant API calls");
        const unsigned slot=unsigned((reinterpret_cast<uintptr_t>(s.material)-1)%22);
        triangles+=unsigned(s.indices_count/3);
        for(size_t j=0;j<s.vertices_count;++j) {
            const auto& v=s.vertices_values[j];check(v.color==0xffffffff,"no baked vertex lighting");
            check(v.position[2]>-32&&v.position[2]<1600,"terrain and trees have plausible elevations");
        }
        for(size_t j=0;j<s.indices_count;++j)check(s.indices_values[j]<s.vertices_count,"all forest indices valid");
        for(size_t j=0;j<s.indices_count;j+=3) {
            float minX=FLT_MAX,minY=FLT_MAX,maxX=-FLT_MAX,maxY=-FLT_MAX;
            for(unsigned k=0;k<3;++k) {
                const auto* p=s.vertices_values[s.indices_values[j+k]].position;
                minX=std::min(minX,p[0]);maxX=std::max(maxX,p[0]);
                minY=std::min(minY,p[1]);maxY=std::max(maxY,p[1]);
            }
            if(slot!=2)check(maxX<=465.f||minX>=785.f||maxY<=-4164.f||minY>=-2464.f,
                "no trunks, crowns or understory cross the south entry/pier corridor");
            else check(maxX<=425.f||minX>=825.f||maxY<=-3800.f||minY>=-2946.f,
                "forest floor cannot cover the original entry bank or pier");
        }
        if(slot==2)for(size_t j=0;j<s.indices_count;j+=3) {
            const auto* a=s.vertices_values[s.indices_values[j]].position;
            const auto* b=s.vertices_values[s.indices_values[j+1]].position;
            const auto* c=s.vertices_values[s.indices_values[j+2]].position;
            check((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])>0,"forest floor triangles face upward");
        }
    }
    exportedTriangles+=triangles;
    *result=reinterpret_cast<remixapi_MeshHandle>(uintptr_t(++madeMeshes));return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL freeMesh(remixapi_MeshHandle){++freedMeshes;return REMIXAPI_ERROR_CODE_SUCCESS;}
static remixapi_ErrorCode REMIXAPI_CALL draw(const remixapi_InstanceInfo* info) {
    check(info&&info->mesh&&info->doubleSided&&!info->pNext,"bridge-compatible ordinary forest batch");
    for(int row=0;row<3;++row)for(int column=0;column<4;++column)
        check(info->transform.matrix[row][column]==(row==column?1.f:0.f),"forest world coordinates use identity transform");
    ++draws;return rejectDraw?REMIXAPI_ERROR_CODE_GENERAL_FAILURE:REMIXAPI_ERROR_CODE_SUCCESS;
}
static HANDLE process=GetCurrentProcess();static DWORD64 moduleBase=0;
template<class T>static T symbol(const char* name) {
    alignas(SYMBOL_INFO) char bytes[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};
    auto* s=reinterpret_cast<SYMBOL_INFO*>(bytes);s->SizeOfStruct=sizeof(SYMBOL_INFO);s->MaxNameLen=MAX_SYM_NAME;
    check(SymFromName(process,name,s)&&s->ModBase==moduleBase,name);return reinterpret_cast<T>(uintptr_t(s->Address));
}
int main(int argc,char** argv)try {
    check(argc==3,"asset directory and compiled renderer arguments");
    const std::string folder=argv[1],filename=folder+"/forest.bin";const std::wstring directory(folder.begin(),folder.end());
    remixapi_Interface api{};api.CreateMaterial=material;api.DestroyMaterial=freeMaterial;
    api.CreateMesh=mesh;api.DestroyMesh=freeMesh;api.DrawInstance=draw;
    lake_forest::Asset asset;if(!asset.load(api,filename.c_str(),directory))throw std::runtime_error(asset.error());
    check(asset.treeCount()==1652&&asset.understoryCount()==4974&&asset.nativeBushCount()>0,
        "original trees and supplied undergrowth/native shrub replacements loaded");
    const unsigned batches=asset.batchCount();
    check(batches>10&&batches<128&&exportedTriangles==5844799&&asset.nativeModelMask()==16383,"all geometry and native plant coverage retained across bounded batches");
    check(asset.draw()&&asset.draw()&&draws==2*batches&&madeMeshes==batches&&madeMaterials==22,
        "one draw per batch without rebuilding or reallocating resources");
    rejectDraw=true;check(!asset.draw(),"failed submission retains original backdrop");rejectDraw=false;
    asset.clear();check(madeMaterials==freedMaterials&&madeMeshes==freedMeshes&&!asset.ready()&&!asset.nativeModelMask(),"map reset releases all resources and native suppression masks");
    check(!asset.load(api,"missing-forest.bin",directory),"missing asset fallback");
    rejectMesh=true;check(!asset.load(api,filename.c_str(),directory),"mesh API failure fallback");rejectMesh=false;
    rejectMeshAfter=madeMeshes+2;check(!asset.load(api,filename.c_str(),directory),"partial batch initialization failure fallback");rejectMeshAfter=0;
    check(madeMaterials==freedMaterials&&madeMeshes==freedMeshes,"partial batch failure releases every resource");
    const auto invalid=folder+"/invalid-forest-test.bin";
    {std::ifstream input(filename,std::ios::binary);char b[64]{};input.read(b,64);std::ofstream output(invalid,std::ios::binary);output.write(b,64);}
    check(!asset.load(api,invalid.c_str(),directory),"truncated asset fallback");remove(invalid.c_str());
    check(madeMaterials==freedMaterials&&madeMeshes==freedMeshes,"partial initialization cleanup");
    const auto dll=LoadLibraryA(argv[2]);check(dll!=nullptr,"compiled renderer load");
    SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_EXACT_SYMBOLS);
    const std::string dllPath=argv[2];const auto slash=dllPath.find_last_of("/\\");
    check(SymInitialize(process,dllPath.substr(0,slash).c_str(),FALSE)!=FALSE,"renderer symbols");
    moduleBase=SymLoadModuleEx(process,nullptr,argv[2],nullptr,reinterpret_cast<DWORD64>(dll),0,nullptr,0);
    check(moduleBase!=0,"matching renderer PDB");
    auto* production=symbol<lake_forest::Asset*>("g_lakeForest");check(production->load(api,filename.c_str(),directory),"compiled renderer asset load");
    symbol<convar_t*>("dx_remix")->value=1;symbol<convar_t*>("dx_cof_lake_forest")->value=1;
    auto* submitted=symbol<bool*>("g_lakeForestDrawn");*submitted=true;
    model_t world{},brush{};strcpy_s(world.name,"maps/c_lake.bsp");*symbol<model_t**>("g_worldmodel")=&world;
    texture_t textures[2]{};mtexinfo_t tx[2]{};msurface_t surfaces[2]{};
    for(int i=0;i<2;++i){tx[i].texture=&textures[i];surfaces[i].texinfo=&tx[i];}
    brush.surfaces=surfaces;brush.nummodelsurfaces=2;
    strcpy_s(textures[0].name,"{c2_lwa3");strcpy_s(textures[1].name,"{c2_lwa4");
    const auto replaces=symbol<bool(*)(const model_t*)>("R_LakeForestReplacesBrush");
    const auto replacesStudio=symbol<bool(*)(const model_t*)>("R_LakeForestReplacesStudio");
    model_t plant{};
    for(const char* name:{"models/Props/Vaxter/buske.mdl","models/Props/Vaxter/skogsris.mdl",
        "models/Props/Vaxter/litengran1.mdl","models/Props/Vaxter/smoaspar.mdl",
        "models/Props/Vaxter/granmodel.mdl","models/Props/Vaxter/granorigin.mdl",
        "models/Props/Vaxter/lowpolygran.mdl","models/Props/Vaxter/talltrad.mdl",
        "models/Props/Vaxter/bigtree1.mdl","models/Props/Vaxter/aspar.mdl",
        "models/Props/Vaxter/stortrad2.mdl","models/Props/Vaxter/bjork.mdl"}) {
        strcpy_s(plant.name,name);check(replacesStudio(&plant),"native dry plants replaced by supplied models");
        *submitted=false;check(!replacesStudio(&plant),"native plants retained on failed replacement submission");*submitted=true;
    }
    for(const char* name:{"models/Props/Vaxter/other/buske.mdl","models/Props/Blandat/lakemdl.mdl",
        "models/Props/Vaxter/other/aspar.mdl","models/Props/Vaxter/aspar.mdl.other"}) {
        strcpy_s(plant.name,name);check(!replacesStudio(&plant),"native trees/reeds and unrelated plant models preserved");
    }
    strcpy_s(plant.name,"models/Props/Vaxter/skogsris.mdl");
    model_t mixed{};strcpy_s(mixed.name,"models/Props/Blandat/lakeadd1.mdl");
    check(!replacesStudio(&mixed),"mixed model soil must not be hidden with its trees");
    const auto mixedFilter=symbol<bool(*)(const cl_entity_s*,int)>("R_LakeForestStudioTextureSuppressed");
    std::vector<byte> storage(sizeof(studiohdr_t)+9*sizeof(mstudiotexture_t));
    auto* hdr=reinterpret_cast<studiohdr_t*>(storage.data());hdr->length=int(storage.size());
    hdr->ident=IDSTUDIOHEADER;hdr->version=STUDIO_VERSION;hdr->numtextures=9;hdr->textureindex=sizeof(studiohdr_t);
    auto* materialHeaders=reinterpret_cast<mstudiotexture_t*>(storage.data()+hdr->textureindex);
    const char* mixedNames[]={"c2_sgras1.bmp","c2_sskogswl2.bmp","stammtex.bmp","talltex.bmp",
        "litengrenljus.bmp","stam.bmp","litengrenmork.bmp","storgren.bmp","storgrenigenom.bmp"};
    for(int i=0;i<9;++i){strcpy_s(materialHeaders[i].name,mixedNames[i]);materialHeaders[i].index=i+1;}
    auto* engine=symbol<ref_api_t*>("gEngfuncs");const auto oldExtra=engine->Mod_Extradata;
    mixedHeader=hdr;engine->Mod_Extradata=mixedExtra;cl_entity_s mixedEntity{};mixedEntity.model=&mixed;
    for(int i=0;i<9;++i)check(mixedFilter(&mixedEntity,i+1)==(i>=2),"compiled mixed-model filter replaces pine materials while retaining soil/background");
    *submitted=false;check(!mixedFilter(&mixedEntity,3),"native mixed pines return on submission failure");*submitted=true;
    engine->Mod_Extradata=oldExtra;mixedHeader=nullptr;
    // Exercise the actual compiled studio entry point, before any D3D draw.
    // The fake device is never dereferenced when the native replacement gate works.
    auto** renderDevice=symbol<IDirect3DDevice9**>("device");
    const auto oldDevice=*renderDevice;*renderDevice=reinterpret_cast<IDirect3DDevice9*>(uintptr_t(1));
    cl_entity_s nativeEntity{};nativeEntity.model=&plant;plant.type=mod_studio;
    auto* nativeCounter=symbol<unsigned*>("g_lakeForestStudioSuppressed");const unsigned beforeNative=*nativeCounter;
    symbol<void(*)(cl_entity_s*)>("R_DrawStudioEntity")(&nativeEntity);
    check(*nativeCounter==beforeNative+1,"compiled studio draw entry point suppresses the original low-plant model");
    model_t grass{};grass.type=mod_sprite;strcpy_s(grass.name,"sprites/ruMpel/c2_ris.spr");
    const auto replacesSprite=symbol<bool(*)(const model_t*)>("R_LakeForestReplacesSprite");
    check(replacesSprite(&grass),"procedural grass layer covered by 3D replacement");
    *submitted=false;check(!replacesSprite(&grass),"native procedural grass retained until successful submission");*submitted=true;
    // Exercise both actual sprite draw routes used by generated grass. The
    // fake device would fail immediately if either reached native D3D drawing.
    nativeEntity.model=&grass;
    auto* grassCounter=symbol<unsigned*>("g_lakeForestGrassSuppressed");const unsigned beforeGrass=*grassCounter;
    symbol<void(*)(cl_entity_s*,model_t*)>("R_DrawSpriteEntity")(&nativeEntity,&grass);
    check(*grassCounter==beforeGrass+1,"compiled sprite entity entry suppresses native grass");
    struct TriFlags {bool active,screenSpace,gunSmoke,lakeGrass;};
    auto* flags=symbol<TriFlags*>("g_triApi");const TriFlags oldFlags=*flags;
    symbol<int(*)(model_t*,int)>("R_TriSpriteTexture")(&grass,0);
    check(flags->lakeGrass&&!flags->gunSmoke,"triangle API identifies only native grass");
    flags->active=true;flags->screenSpace=false;
    symbol<void(*)()>("R_TriEnd")();check(!flags->active,"triangle API ends hidden grass without drawing");
    strcpy_s(grass.name,"sprites/fog2.spr");
    symbol<int(*)(model_t*,int)>("R_TriSpriteTexture")(&grass,0);
    check(!flags->lakeGrass&&!replacesSprite(&grass),"map fog and other sprites remain native");
    *flags=oldFlags;
    *renderDevice=oldDevice;
    for(const char* name:{"*1","*17","*18"}){strcpy_s(brush.name,name);check(replaces(&brush),"compiled renderer hides the original forest backplates");}
    for(const char* name:{"*2","*34"}){strcpy_s(brush.name,name);check(!replaces(&brush),"reeds and lake water preserved");}
    strcpy_s(brush.name,"*30");check(!replaces(&brush),"native shrub brush needs exact shrub texture whitelist");
    strcpy_s(textures[0].name,"{c2_skva1");strcpy_s(textures[1].name,"{invisible");
    check(replaces(&brush),"native bush panels hidden after replacement mesh submission");
    *submitted=false;check(!replaces(&brush),"failed submission restores native shrub panels");*submitted=true;
    strcpy_s(textures[1].name,"stone");check(!replaces(&brush),"mixed native shrub brush preserved");
    strcpy_s(textures[0].name,"{c2_lwa3");strcpy_s(textures[1].name,"{c2_lwa4");
    strcpy_s(brush.name,"*1");strcpy_s(textures[1].name,"stone");check(!replaces(&brush),"mixed unrelated brush preserved");strcpy_s(textures[1].name,"{c2_lwa4");
    *submitted=false;check(!replaces(&brush),"original backplates kept until successful forest submission");*submitted=true;
    strcpy_s(world.name,"maps/c_start2.bsp");check(!replaces(&brush)&&!replacesStudio(&plant),"other maps preserved");strcpy_s(world.name,"maps/c_lake.bsp");
    symbol<convar_t*>("dx_cof_lake_forest")->value=0;check(!replaces(&brush)&&!replacesStudio(&plant),"replacement can be disabled");
    production->clear();*symbol<model_t**>("g_worldmodel")=nullptr;
    check(madeMaterials==freedMaterials&&madeMeshes==freedMeshes,"production resource cleanup");
    printf("PASS: supplied low plants/shrubs with PBR/cutouts, 1535 original trees, clear entry/pier and %u bounded immutable batches; no geometry lost.\n",batches);
    puts("PASS: compiled renderer replaces native shrub/low-plant models and the entire c2_ris procedural layer through both sprite paths; preserves trees/reeds/water/fog, other maps and fallback on failures.");
    return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL FOREST: %s\n",e.what());return 1;}
