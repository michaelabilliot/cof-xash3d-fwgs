#include "dx_local.h"
#include "dx_lake_bed_asset.h"

static lake_bed::Asset g_lakeBed;
static HMODULE g_lakeBedModule=nullptr;
static ULONGLONG g_lakeBedRetry=0;
static bool g_lakeBedFailed=false,g_lakeBedDrawn=false;
static bool R_LakeBedActive() {
    return dx_remix.value!=0.f&&g_worldmodel&&(g_currentViewFlags&RF_DRAW_WORLD)&&
        !Q_stricmp(g_worldmodel->name,"maps/c_lake.bsp");
}
void R_LakeBedReset() {
    g_lakeBed.clear(g_lakeBedModule&&g_lakeBedModule==GetModuleHandleA("d3d9.dll"));
    g_lakeBedModule=nullptr;g_lakeBedRetry=0;g_lakeBedFailed=false;g_lakeBedDrawn=false;
}
void R_LakeBedDraw() {
    g_lakeBedDrawn=false;
    if(!R_LakeBedActive()||g_lakeBedFailed)return;
    HMODULE module=GetModuleHandleA("d3d9.dll");
    if(g_lakeBed.ready()&&module!=g_lakeBedModule)R_LakeBedReset();
    if(!g_lakeBed.ready()) {
        if(GetTickCount64()<g_lakeBedRetry)return;g_lakeBedRetry=GetTickCount64()+2000;
        const auto init=module?reinterpret_cast<PFN_remixapi_InitializeLibrary>(GetProcAddress(module,"remixapi_InitializeLibrary")):nullptr;
        if(!init)return;
        remixapi_Interface api{};remixapi_InitializeLibraryInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
        info.version=REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR,REMIXAPI_VERSION_MINOR,REMIXAPI_VERSION_PATCH);
        if(init(&info,&api)!=REMIXAPI_ERROR_CODE_SUCCESS)return;
        g_lakeBedModule=module;
        if(!g_lakeBed.create(api)) {
            g_lakeBedFailed=true;gEngfuncs.Con_Printf("D3D9: c_lake lake bed: %s\n",g_lakeBed.error().c_str());return;
        }
        gEngfuncs.Con_Printf("D3D9: c_lake opaque sediment bed restored over underwater sky floor: 133 triangles, no vegetation barrier\n");
    }
    g_lakeBedDrawn=g_lakeBed.draw();
}
