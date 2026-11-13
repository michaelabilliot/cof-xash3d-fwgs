#include "dx_local.h"
#include <dbghelp.h>
#include <stdexcept>
static HANDLE process = GetCurrentProcess();
static DWORD64 base;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class T> static T symbol(const char* name) {
    alignas(SYMBOL_INFO) char bytes[sizeof(SYMBOL_INFO) + MAX_SYM_NAME]{};
    auto info = reinterpret_cast<SYMBOL_INFO*>(bytes);
    info->SizeOfStruct = sizeof(SYMBOL_INFO); info->MaxNameLen = MAX_SYM_NAME;
    check(SymFromName(process, name, info) && info->ModBase == base, name);
    return reinterpret_cast<T>(static_cast<uintptr_t>(info->Address));
}
int main(int argc, char** argv) try {
    check(argc == 2, "renderer argument");
    auto dll = LoadLibraryA(argv[1]); check(dll != nullptr, "renderer load");
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_EXACT_SYMBOLS);
    check(SymInitialize(process, nullptr, FALSE) != FALSE, "symbols");
    const std::string path = argv[1]; SymSetSearchPath(process, path.substr(0, path.find_last_of("/\\")).c_str());
    base = SymLoadModuleEx(process, nullptr, argv[1], nullptr, reinterpret_cast<DWORD64>(dll), 0, nullptr, 0);
    check(base != 0, "matching PDB");
    *symbol<bool*>("g_dxCullValid") = true;
    symbol<convar_t*>("r_nocull")->value = 0;
    const char* axes[] = {"g_dxCullOrigin", "g_dxCullForward", "g_dxCullRight", "g_dxCullUp"};
    for (const auto name : axes) for (int i = 0; i < 3; ++i) symbol<float*>(name)[i] = 0;
    symbol<float*>(axes[1])[0] = 1; symbol<float*>(axes[2])[1] = 1; symbol<float*>(axes[3])[2] = 1;
    *symbol<float*>("g_dxCullNear") = 4; *symbol<float*>("g_dxCullFar") = 1000;
    auto sphere = symbol<bool(*)(const float*, float)>("R_SphereVisible");
    for (float tangent : {0.5f, 1.f, 2.f}) {
        *symbol<float*>("g_dxCullTanX") = tangent;
        *symbol<float*>("g_dxCullTanY") = tangent;
        for (int axis = 1; axis <= 2; ++axis) for (float sign : {-1.f, 1.f}) {
            // Center beyond the side plane by 0.99 radii measured along
            // its unit normal: the sphere must still intersect the view.
            const float extent = 10.f * sqrtf(1.f + tangent*tangent);
            float center[3] = {100, 0, 0};
            center[axis] = sign * (100*tangent + extent*.99f);
            check(sphere(center, 10), "sphere intersecting side plane was culled");
            center[axis] = sign * (100*tangent + extent*1.01f);
            check(!sphere(center, 10), "sphere fully outside side plane was retained");
        }
    }
    float behind[3] = {-100,0,0}, farPoint[3] = {1100,0,0}, front[3] = {100,0,0};
    check(!sphere(behind,10) && !sphere(farPoint,10) && sphere(front,10), "near/far planes");
    symbol<convar_t*>("r_nocull")->value = 1;
    check(sphere(behind,10), "no-cull override");
    symbol<convar_t*>("r_nocull")->value = 0;
    *symbol<bool*>("g_dxCullValid") = false;
    check(sphere(behind,10), "invalid camera must fail open");
    *symbol<bool*>("g_dxCullValid") = true;
    auto visible = symbol<bool(*)(const cl_entity_s*)>("R_EntityMightBeVisible");
    model_t model{}; model.type = mod_studio;
    model.mins[0] = -310; model.maxs[0] = -290;
    model.mins[1] = model.mins[2] = -10; model.maxs[1] = model.maxs[2] = 10;
    cl_entity_t ent{}; ent.model = &model; ent.origin[0] = 100; ent.angles[1] = 180;
    // Rotated, offset bounds may reach the frustum even if the unrotated
    // local center would have been behind it.
    check(visible(&ent), "rotating off-center bounds must remain conservative");
    model_t world{}; world.type = mod_brush;
    *symbol<model_t**>("g_worldmodel") = &world;
    symbol<convar_t*>("dx_remix_perf")->value = 1;
    symbol<convar_t*>("dx_world_megabatch")->value = 1;
    symbol<convar_t*>("r_novis")->value = 1;
    world_model_draw_t draw{}; draw.sourceModel = &world; draw.batches.resize(2);
    for (int b = 0; b < 2; ++b) {
        auto& batch = draw.batches[b]; batch.primitiveCount = 1; batch.boundsValid = true;
        const float x = b ? -1000.f : 100.f;
        for (int i=0;i<3;++i) { batch.mins[i] = (i ? 0.f : x)-1; batch.maxs[i] = (i ? 0.f : x)+1; }
    }
    auto batches = symbol<const std::vector<size_t>*(*)(const world_model_draw_t&)>("R_GetVisibleWorldBatches");
    symbol<convar_t*>("r_nocull")->value = 1;
    check(batches(draw)->size() == 2, "no-cull cache contains both batches");
    symbol<convar_t*>("r_nocull")->value = 0;
    check(batches(draw)->size() == 1 && batches(draw)->front() == 0, "enabling frustum invalidates PVS-only cache");
    symbol<float*>("g_dxCullForward")[0] = -1;
    check(batches(draw)->size() == 1 && batches(draw)->front() == 1, "camera turn invalidates megabatch visibility");
    auto capture = symbol<convar_t*>("r_remixcull");
    auto bounds = symbol<bool(*)(const float*, const float*, bool)>("R_BoundsVisibleInCurrentView");
    check(!bounds(draw.batches[0].mins, draw.batches[0].maxs, true), "normal bounds rejection");
    capture->value = 1;
    check(sphere(front,10) && bounds(draw.batches[0].mins, draw.batches[0].maxs, true),
          "capture bypasses entity and world frustum planes");
    check(batches(draw) == nullptr, "capture uses uncached world submission");
    auto pvs = symbol<bool(*)(const world_model_draw_t&, const std::vector<int>&, bool)>("R_PVSLeafsVisible");
    draw.pvsApplicable = true;
    check(pvs(draw, {12345}, false), "capture accepts batches regardless of PVS leaf");
    capture->value = 0;
    check(batches(draw)->size() == 1 && batches(draw)->front() == 1, "capture toggle restores normal culling");
    puts("PASS: production frustum, capture visibility override, and world cache toggling. No GPU/window/game.");
    return 0;
} catch (const std::exception& error) { fprintf(stderr, "FAIL FRUSTUM: %s\n", error.what()); return 1; }
