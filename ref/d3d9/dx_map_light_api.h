#pragma once
#define REMIX_ALLOW_X86
#include "vendor/remix_c.h"
#include "dx_map_light_power.h"
#include "dx_light_lifetime.h"

namespace dx_map_light {
struct Instance {
    remixapi_LightHandle handle = nullptr;
    D3DLIGHT9 previous{};
    float radius = 0;
    float outputScale = 1.f;
    ULONGLONG nextDefinition = 0;
};

class Bridge {
    remixapi_Interface api{};
    HMODULE module = nullptr;
    bool attempted = false;
    unsigned failures = 0;
    ULONGLONG retryAt = 0;
    unsigned previousInitialization = ~0u;
    LifetimeIds lifetimes;
    bool updateUnavailable = false;
public:
    bool ready() {
        const auto loaded = GetModuleHandleA("d3d9.dll");
        if (loaded != module) { api = {}; attempted = false; module = loaded; updateUnavailable = false; }
        if (attempted) return api.CreateLight && api.DestroyLight && api.DrawLightInstance;
        if (GetTickCount64() < retryAt) return false;
        if (!module) return false;
        attempted = true;
        const auto initialize = reinterpret_cast<PFN_remixapi_InitializeLibrary>(
            GetProcAddress(module, "remixapi_InitializeLibrary"));
        if (!initialize) return false;
        remixapi_InitializeLibraryInfo info{};
        info.sType = REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
        info.version = REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR,
            REMIXAPI_VERSION_MINOR, REMIXAPI_VERSION_PATCH);
        const auto result = initialize(&info, &api);
        if (result != REMIXAPI_ERROR_CODE_SUCCESS) api = {};
        // The bridge can precede the server's first Present during map load.
        // Continue legacy submission and retry after startup, without stalling.
        if (result == REMIXAPI_ERROR_CODE_NOT_INITIALIZED) {
            attempted = false;
            retryAt = GetTickCount64() + 1000;
        }
        if (unsigned(result) != previousInitialization) {
            gEngfuncs.Con_Printf("D3D9: map emitter API initialization %u (%s)\n",
                unsigned(result), api.CreateLight ? "ready" : "D3D fallback");
            previousInitialization = unsigned(result);
        }
        return api.CreateLight && api.DestroyLight && api.DrawLightInstance;
    }
    bool setConfig(const char* key, const char* value) {
        return ready() && api.SetConfigVariable &&
            api.SetConfigVariable(key, value) == REMIXAPI_ERROR_CODE_SUCCESS;
    }
    void retire(Instance& instance) {
        if (instance.handle && api.DestroyLight) api.DestroyLight(instance.handle);
        instance = {};
    }
    bool emit(Instance& instance, const D3DLIGHT9& light, float radius, uint64_t id, float outputScale = 1.f) {
        if (!ready() || light.Type == D3DLIGHT_DIRECTIONAL) return false;
        if (!std::isfinite(outputScale) || outputScale < 0) { retire(instance); return false; }
        // Zero is deliberately handled, so the caller does not emit a legacy
        // fallback light after the API light has been switched off.
        if (outputScale == 0) { retire(instance); return true; }
        const float brightest = std::max({ light.Diffuse.r, light.Diffuse.g, light.Diffuse.b });
        const float output = radiance(brightest, light.Range, light.Attenuation0,
            light.Attenuation1, light.Attenuation2, radius) * outputScale;
        if (output <= 0 || brightest <= 0) { retire(instance); return false; }
        const bool changed = radius != instance.radius || outputScale != instance.outputScale ||
            std::memcmp(&instance.previous, &light, sizeof(light)) != 0;
        const auto now = GetTickCount64();
        if (!instance.handle || (changed && now >= instance.nextDefinition)) {
            remixapi_LightInfoSphereEXT sphere{};
            sphere.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_SPHERE_EXT;
            sphere.position = { light.Position.x, light.Position.y, light.Position.z };
            sphere.radius = radius;
            sphere.volumetricRadianceScale = 1.f;
            if (light.Type == D3DLIGHT_SPOT) {
                sphere.shaping_hasvalue = 1;
                sphere.shaping_value.direction = { light.Direction.x, light.Direction.y, light.Direction.z };
                sphere.shaping_value.coneAngleDegrees = light.Phi * (90.f / 3.14159265358979323846f);
                sphere.shaping_value.coneSoftness = std::cos(light.Theta * .5f) - std::cos(light.Phi * .5f);
                sphere.shaping_value.focusExponent = light.Falloff;
            }
            remixapi_LightInfo info{};
            info.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO;
            info.pNext = &sphere;
            // Keep updates on their current handle; a new allocation gets a
            // new lifetime even if its source entity has the same stable ID.
            const bool canUpdate = instance.handle && api.UpdateLightDefinition && !updateUnavailable;
            info.hash = canUpdate ? uint64_t(instance.handle) : lifetimes.next(id);
            if (!info.hash) return false;
            info.isDynamic = 1; // No stale static-light retention after off/map change.
            info.radiance = { light.Diffuse.r / brightest * output,
                light.Diffuse.g / brightest * output, light.Diffuse.b / brightest * output };
            remixapi_ErrorCode result;
            if (canUpdate) {
                result = api.UpdateLightDefinition(instance.handle, &info);
                // Remix Plus's x86 bridge exposes a non-null no-op stub.
                // Rebuild this light immediately, then avoid that stub later.
                if (result != REMIXAPI_ERROR_CODE_SUCCESS) {
                    updateUnavailable = true;
                    retire(instance);
                    info.hash = lifetimes.next(id);
                    if (!info.hash) return false;
                    result = api.CreateLight(&info, &instance.handle);
                }
            } else {
                retire(instance);
                result = api.CreateLight(&info, &instance.handle);
            }
            if (result != REMIXAPI_ERROR_CODE_SUCCESS || !instance.handle) {
                retire(instance);
                if (failures++ < 8) gEngfuncs.Con_Printf("D3D9: map emitter definition failed %u; D3D fallback\n", unsigned(result));
                return false;
            }
            instance.previous = light;
            instance.radius = radius;
            instance.outputScale = outputScale;
            // Cap moving-source definition/IPC churn; draw the live instance
            // every frame. Static sources perform no repeated definitions.
            instance.nextDefinition = now + 33;
        }
        const auto result = api.DrawLightInstance(instance.handle);
        if (result != REMIXAPI_ERROR_CODE_SUCCESS) {
            retire(instance);
            if (failures++ < 8) gEngfuncs.Con_Printf("D3D9: map emitter draw failed %u; D3D fallback\n", unsigned(result));
            return false;
        }
        return true;
    }
};
}
