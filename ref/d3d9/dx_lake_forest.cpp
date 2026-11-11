#include "dx_local.h"
#include "dx_lake_forest_asset.h"

static convar_t dx_cof_lake_forest={(char*)"dx_cof_lake_forest",(char*)"1",0};
static lake_forest::Asset g_lakeForest;
static HMODULE g_lakeForestModule=nullptr;
static ULONGLONG g_lakeForestRetry=0;
static bool g_lakeForestFailed=false,g_lakeForestDrawn=false;
static unsigned g_lakeForestSuppressed=0;
static unsigned g_lakeForestStudioSuppressed=0;
static unsigned g_lakeForestGrassSuppressed=0;
static unsigned g_lakeForestPineMaterialsSuppressed=0;

static bool R_LakeForestMap() {
    return dx_cof_lake_forest.value!=0.f&&dx_remix.value!=0.f&&g_worldmodel&&
        !Q_stricmp(g_worldmodel->name,"maps/c_lake.bsp");
}
static void R_LakeForestStats() {
    gEngfuncs.Con_Printf("D3D9: c_lake forest ready=%d drawn=%d trees=%u understory=%u triangles=%u nativeBushes=%u batches=%u nativeBrushesSuppressed=%u nativePlantsSuppressed=%u\n",
        g_lakeForest.ready(),g_lakeForestDrawn,g_lakeForest.treeCount(),g_lakeForest.understoryCount(),
        g_lakeForest.triangleCount(),g_lakeForest.nativeBushCount(),g_lakeForest.batchCount(),g_lakeForestSuppressed,g_lakeForestStudioSuppressed);
}
void R_LakeForestInit() {
    gEngfuncs.Cvar_RegisterVariable(&dx_cof_lake_forest);
    gEngfuncs.Cmd_AddCommand("dx_cof_lake_forest_stats",R_LakeForestStats,"Show c_lake forest replacement status");
}
void R_LakeForestReset() {
    g_lakeForest.clear(g_lakeForestModule&&g_lakeForestModule==GetModuleHandleA("d3d9.dll"));
    g_lakeForestModule=nullptr;g_lakeForestRetry=0;g_lakeForestFailed=false;g_lakeForestDrawn=false;g_lakeForestSuppressed=0;g_lakeForestStudioSuppressed=0;
}
void R_LakeForestShutdown() {
    R_LakeForestReset();gEngfuncs.Cmd_RemoveCommand("dx_cof_lake_forest_stats");
}
void R_LakeForestDraw() {
    g_lakeForestDrawn=false;g_lakeForestSuppressed=0;g_lakeForestStudioSuppressed=0;g_lakeForestGrassSuppressed=0;g_lakeForestPineMaterialsSuppressed=0;
    if(!R_LakeForestMap()||!(g_currentViewFlags&RF_DRAW_WORLD)||g_lakeForestFailed)return;
    HMODULE module=GetModuleHandleA("d3d9.dll");
    if(g_lakeForest.ready()&&module!=g_lakeForestModule)R_LakeForestReset();
    if(!g_lakeForest.ready()) {
        if(GetTickCount64()<g_lakeForestRetry)return;g_lakeForestRetry=GetTickCount64()+2000;
        const auto initialize=module?reinterpret_cast<PFN_remixapi_InitializeLibrary>(GetProcAddress(module,"remixapi_InitializeLibrary")):nullptr;
        if(!initialize)return;
        remixapi_Interface api{};remixapi_InitializeLibraryInfo info{};
        info.sType=REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
        info.version=REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR,REMIXAPI_VERSION_MINOR,REMIXAPI_VERSION_PATCH);
        if(initialize(&info,&api)!=REMIXAPI_ERROR_CODE_SUCCESS)return;
        wchar_t directory[MAX_PATH]{};
        if(!GetFullPathNameW(L"CryOfFear/models/cofr/lake_forest",MAX_PATH,directory,nullptr))return;
        g_lakeForestModule=module;
        if(!g_lakeForest.load(api,"CryOfFear/models/cofr/lake_forest/forest.bin",directory)) {
            g_lakeForestFailed=true;
            gEngfuncs.Con_Printf("D3D9: c_lake forest: %s; keeping original backdrop\n",g_lakeForest.error().c_str());return;
        }
        gEngfuncs.Con_Printf("D3D9: c_lake forest loaded: %u trees / %u understory / %u native shrubs, %u static batches, PBR alpha foliage\n",
            g_lakeForest.treeCount(),g_lakeForest.understoryCount(),g_lakeForest.nativeBushCount(),g_lakeForest.batchCount());
    }
    g_lakeForestDrawn=g_lakeForest.draw();
}
bool R_LakeForestReplacesBrush(const model_t* model) {
    if(!g_lakeForestDrawn||!R_LakeForestMap()||!model)return false;
    const bool shrub=!strcmp(model->name,"*30");
    if(shrub) {if(!g_lakeForest.nativeBushCount())return false;}
    else if(strcmp(model->name,"*1")&&strcmp(model->name,"*17")&&strcmp(model->name,"*18"))return false;
    if(!model->surfaces||model->firstmodelsurface<0||model->nummodelsurfaces<=0)return false;
    for(int i=0;i<model->nummodelsurfaces;++i) {
        const auto& surface=model->surfaces[model->firstmodelsurface+i];
        const auto* texture=surface.texinfo?surface.texinfo->texture:nullptr;
        if(!texture)return false;
        const char* n=texture->name;
        if(shrub) {
            if(Q_stricmp(n,"{c2_skva1")&&Q_stricmp(n,"{invisible"))return false;
            continue;
        }
        if(Q_stricmp(n,"{c2_lwa1_a")&&Q_stricmp(n,"{c2_lwa2_a")&&
           Q_stricmp(n,"{c2_lwa3")&&Q_stricmp(n,"{c2_lwa4")&&Q_stricmp(n,"{r_sweskog"))return false;
    }
    ++g_lakeForestSuppressed;return true;
}
bool R_LakeForestReplacesStudio(const model_t* model) {
    if(!g_lakeForestDrawn||!R_LakeForestMap()||!model)return false;
    const unsigned mask=g_lakeForest.nativeModelMask();
    if(((mask&1)&&!Q_stricmp(model->name,"models/Props/Vaxter/buske.mdl"))||
       ((mask&2)&&!Q_stricmp(model->name,"models/Props/Vaxter/skogsris.mdl"))||
       ((mask&8)&&!Q_stricmp(model->name,"models/Props/Vaxter/litengran1.mdl"))||
       ((mask&16)&&!Q_stricmp(model->name,"models/Props/Vaxter/smoaspar.mdl"))||
       ((mask&32)&&!Q_stricmp(model->name,"models/Props/Vaxter/granmodel.mdl"))||
       ((mask&64)&&!Q_stricmp(model->name,"models/Props/Vaxter/granorigin.mdl"))||
       ((mask&128)&&!Q_stricmp(model->name,"models/Props/Vaxter/lowpolygran.mdl"))||
       ((mask&256)&&!Q_stricmp(model->name,"models/Props/Vaxter/talltrad.mdl"))||
       ((mask&1024)&&!Q_stricmp(model->name,"models/Props/Vaxter/bigtree1.mdl"))||
       ((mask&2048)&&!Q_stricmp(model->name,"models/Props/Vaxter/aspar.mdl"))||
       ((mask&4096)&&!Q_stricmp(model->name,"models/Props/Vaxter/stortrad2.mdl"))||
       ((mask&8192)&&!Q_stricmp(model->name,"models/Props/Vaxter/bjork.mdl"))) {
        ++g_lakeForestStudioSuppressed;return true;
    }
    return false;
}
bool R_LakeForestReplacesSprite(const model_t* model) {
    if(!g_lakeForestDrawn||!R_LakeForestMap()||!model||!(g_lakeForest.nativeModelMask()&4))return false;
    if(Q_stricmp(model->name,"sprites/ruMpel/c2_ris.spr"))return false;
    ++g_lakeForestGrassSuppressed;return true;
}
bool R_LakeForestReplacesMixedPines(const model_t* model) {
    return g_lakeForestDrawn&&R_LakeForestMap()&&model&&(g_lakeForest.nativeModelMask()&512)&&
        !Q_stricmp(model->name,"models/Props/Blandat/lakeadd1.mdl");
}
bool R_LakeForestReplacesStudioTexture(const model_t* model,const char* texture) {
    if(!R_LakeForestReplacesMixedPines(model)||!texture)return false;
    for(const char* name:{"stammtex.bmp","talltex.bmp","litengrenljus.bmp","stam.bmp",
        "litengrenmork.bmp","storgren.bmp","storgrenigenom.bmp"})
        if(!Q_stricmp(texture,name)) {++g_lakeForestPineMaterialsSuppressed;return true;}
    return false;
}
