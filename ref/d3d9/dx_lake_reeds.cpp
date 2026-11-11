#include "dx_local.h"
#include "dx_lake_reeds_asset.h"

static convar_t dx_cof_lake_reeds={(char*)"dx_cof_lake_reeds",(char*)"1",0};
static convar_t dx_cof_lake_reeds_wind={(char*)"dx_cof_lake_reeds_wind",(char*)"1",FCVAR_ARCHIVE};
static lake_reeds::Asset g_lakeReeds;
static HMODULE g_lakeReedsModule=nullptr;
static ULONGLONG g_lakeReedsRetry=0;
static bool g_lakeReedsFailed=false,g_lakeReedsDrawn=false;
static unsigned g_lakeReedsSuppressed=0;

static bool R_LakeReedsMap() {
    return dx_cof_lake_reeds.value!=0.f&&dx_remix.value!=0.f&&g_worldmodel&&
        !Q_stricmp(g_worldmodel->name,"maps/c_lake.bsp");
}
static void R_LakeReedsStats() {
    const auto& s=g_lakeReeds.stats();
    gEngfuncs.Con_Printf("D3D9: c_lake reeds enabled=%g ready=%d drawn=%d clumps=%u LOD=(%u %u %u) nativeBrushesSuppressed=%u wind=%g\n",
        dx_cof_lake_reeds.value,g_lakeReeds.ready(),g_lakeReedsDrawn,s.clumps,s.lod[0],s.lod[1],s.lod[2],g_lakeReedsSuppressed,dx_cof_lake_reeds_wind.value);
}
void R_LakeReedsInit() {
    gEngfuncs.Cvar_RegisterVariable(&dx_cof_lake_reeds);
    gEngfuncs.Cvar_RegisterVariable(&dx_cof_lake_reeds_wind);
    gEngfuncs.Cmd_AddCommand("dx_cof_lake_reeds_stats",R_LakeReedsStats,"Show c_lake 3D reed instances and distance LODs");
}
void R_LakeReedsReset() {
    g_lakeReeds.clear(g_lakeReedsModule&&g_lakeReedsModule==GetModuleHandleA("d3d9.dll"));
    g_lakeReedsModule=nullptr;g_lakeReedsRetry=0;g_lakeReedsFailed=false;g_lakeReedsDrawn=false;
}
void R_LakeReedsShutdown() {
    R_LakeReedsReset();gEngfuncs.Cmd_RemoveCommand("dx_cof_lake_reeds_stats");
}
void R_LakeReedsDraw() {
    g_lakeReedsDrawn=false;g_lakeReedsSuppressed=0;
    if(!R_LakeReedsMap()||!(g_currentViewFlags&RF_DRAW_WORLD)||g_lakeReedsFailed)return;
    HMODULE module=GetModuleHandleA("d3d9.dll");
    if(g_lakeReeds.ready()&&module!=g_lakeReedsModule)R_LakeReedsReset();
    if(!g_lakeReeds.ready()) {
        if(GetTickCount64()<g_lakeReedsRetry)return;
        g_lakeReedsRetry=GetTickCount64()+2000;
        const auto initialize=module?reinterpret_cast<PFN_remixapi_InitializeLibrary>(GetProcAddress(module,"remixapi_InitializeLibrary")):nullptr;
        if(!initialize)return;
        remixapi_Interface api{};remixapi_InitializeLibraryInfo info{};
        info.sType=REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
        info.version=REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR,REMIXAPI_VERSION_MINOR,REMIXAPI_VERSION_PATCH);
        if(initialize(&info,&api)!=REMIXAPI_ERROR_CODE_SUCCESS)return;
        wchar_t directory[MAX_PATH]{};
        if(!GetFullPathNameW(L"CryOfFear/models/cofr/lake_reeds",MAX_PATH,directory,nullptr))return;
        g_lakeReedsModule=module;
        if(!g_lakeReeds.load(api,"CryOfFear/models/cofr/lake_reeds/reeds.bin",directory)) {
            g_lakeReedsFailed=true;
            gEngfuncs.Con_Printf("D3D9: c_lake reed replacement: %s; using original vegetation\n",g_lakeReeds.error().c_str());
            return;
        }
        gEngfuncs.Con_Printf("D3D9: c_lake 3D reeds loaded: %u clumps, 4 variations, PBR, 3 distance LODs\n",unsigned(g_lakeReeds.count()));
    }
    g_lakeReedsDrawn=g_lakeReeds.draw(gpGlobals->vieworg,R_CurrentRenderTime(0.f),dx_cof_lake_reeds_wind.value);
}
bool R_LakeReedsReplacesBrush(const model_t* model) {
    if(!g_lakeReedsDrawn||!R_LakeReedsMap()||!model||model->name[0]!='*')return false;
    char* end=nullptr;const unsigned index=static_cast<unsigned>(strtoul(model->name+1,&end,10));
    if(!end||*end||!g_lakeReeds.replaces(index))return false;
    bool reeds=false;
    for(int i=0;i<model->nummodelsurfaces;++i) {
        const auto& s=model->surfaces[model->firstmodelsurface+i];
        const auto* texture=s.texinfo?s.texinfo->texture:nullptr;
        if(!texture)return false;
        if(!Q_stricmp(texture->name,"{c2_vass"))reeds=true;
        else if(Q_stricmp(texture->name,"{invisible"))return false;
    }
    if(reeds)++g_lakeReedsSuppressed;
    return reeds;
}
