#pragma once
#define REMIX_ALLOW_X86
#include "vendor/remix_c.h"
#include <cstdio>

// A map-scoped, immutable Remix asset. The native entity still owns gameplay,
// collision and its transform; a failed/unsupported import draws the native car.
namespace audi_replacement {
class Asset {
    friend struct TestAccess;
    remixapi_Interface api{};
    HMODULE module = nullptr;
    remixapi_MeshHandle mesh = nullptr;
    std::vector<remixapi_MaterialHandle> materials;
    ULONGLONG retryAt = 0;
    bool failed = false;
    uint64_t generation = 0;
    bool error(const char* reason) {
        reset(); failed = true;
        gEngfuncs.Con_Printf("D3D9: Audi replacement: %s; using native car\n", reason);
        return false;
    }
public:
    void reset() {
        if (module && module == GetModuleHandleA("d3d9.dll")) {
            if (mesh && api.DestroyMesh) api.DestroyMesh(mesh);
            for (auto m : materials) if (m && api.DestroyMaterial) api.DestroyMaterial(m);
        }
        mesh = nullptr; materials.clear(); api = {}; module = nullptr;
        failed = false; retryAt = 0; ++generation;
    }
    bool load() {
        if (failed) return false;
        if (mesh) return true;
        if (GetTickCount64() < retryAt) return false;
        retryAt = GetTickCount64() + 2000;
        module = GetModuleHandleA("d3d9.dll");
        const auto initialize = module ? reinterpret_cast<PFN_remixapi_InitializeLibrary>(
            GetProcAddress(module, "remixapi_InitializeLibrary")) : nullptr;
        if (!initialize) return false;
        remixapi_InitializeLibraryInfo init{};
        init.sType = REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
        init.version = REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR, REMIXAPI_VERSION_MINOR, REMIXAPI_VERSION_PATCH);
        if (initialize(&init, &api) != REMIXAPI_ERROR_CODE_SUCCESS) return false;
        if (!api.CreateMesh || !api.DrawInstance || !api.DestroyMesh || !api.CreateMaterial || !api.DestroyMaterial)
            return error("mesh/material API unavailable");
        return readAsset("CryOfFear/models/cofr/audi200/audi200.bin");
    }
private:
    bool readAsset(const char* path) {
        FILE* file = fopen(path, "rb");
        if (!file) return error("asset file missing");
        struct Close { FILE* f; ~Close() { fclose(f); } } close{file};
        fseek(file, 0, SEEK_END); const long length = ftell(file); rewind(file);
        if (length < 16 || length > 128*1024*1024) return error("invalid file size");
        size_t remaining = size_t(length);
        auto read = [&](void* dest, size_t n) {
            if (n > remaining || fread(dest, 1, n, file) != n) return false;
            remaining -= n; return true;
        };
        uint32_t header[4];
        if (!read(header, sizeof(header)) || header[0] != 0x44554143 || header[1] != 1 ||
            !header[2] || header[2] > 128 || !header[3] || header[3] > 128)
            return error("invalid asset header");
        materials.resize(header[2]);
        for (uint32_t i=0; i<header[2]; ++i) {
            float values[5]; uint32_t glass; char filename[128];
            if (!read(values, sizeof(values)) || !read(&glass, 4) || !read(filename, sizeof(filename)))
                return error("truncated material");
            for (float v : values) if (!std::isfinite(v) || v < 0 || v > 1) return error("invalid material value");
            if (glass > 2 || !memchr(filename, 0, sizeof(filename))) return error("invalid material flags/path");
            std::wstring texture;
            if (*filename) {
                texture = L"CryOfFear/models/cofr/audi200/";
                for (const char* p=filename; *p; ++p) {
                    if (!isalnum(static_cast<unsigned char>(*p)) && *p!='_' && *p!='.') return error("invalid texture filename");
                    texture += wchar_t(*p);
                }
                wchar_t absolute[MAX_PATH]{};
                if (!GetFullPathNameW(texture.c_str(), MAX_PATH, absolute, nullptr)) return error("texture path resolution");
                if (GetFileAttributesW(absolute) == INVALID_FILE_ATTRIBUTES) return error("texture file missing");
                texture = absolute;
            }
            remixapi_MaterialInfoOpaqueEXT opaque{};
            opaque.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
            opaque.albedoConstant = {values[0], values[1], values[2]};
            opaque.opacityConstant = 1; opaque.roughnessConstant = values[3]; opaque.metallicConstant = values[4];
            opaque.alphaTestType = 7; // VK_COMPARE_OP_ALWAYS; zero discards all opaque ray hits.
            remixapi_MaterialInfoTranslucentEXT transparent{};
            transparent.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_TRANSLUCENT_EXT;
            const bool windowPane = glass == 2;
            transparent.refractiveIndex = windowPane ? 1.52f : 1.5f;
            // Thin-wall tint is applied per unit path length. Keep the window
            // tint light and neutral so the panes read as clear automotive glass.
            transparent.transmittanceColor = windowPane
                ? remixapi_Float3D{.8f, .8f, .8f} : remixapi_Float3D{.55f, .58f, .6f};
            transparent.transmittanceMeasurementDistance = 1.f;
            transparent.thinWallThickness_hasvalue = 1;
            transparent.thinWallThickness_value = windowPane ? .25f : .1f;
            remixapi_MaterialInfo info{};
            info.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;
            info.hash = 0x4155444900000000ull + generation*256 + i;
            info.pNext = glass ? static_cast<void*>(&transparent) : static_cast<void*>(&opaque);
            info.albedoTexture = texture.empty() ? nullptr : texture.c_str();
            if (api.CreateMaterial(&info, &materials[i]) != REMIXAPI_ERROR_CODE_SUCCESS || !materials[i])
                return error("material creation failed");
        }
        std::vector<std::vector<remixapi_HardcodedVertex>> vertices(header[3]);
        std::vector<std::vector<uint32_t>> indices(header[3]);
        std::vector<remixapi_MeshInfoSurfaceTriangles> surfaces(header[3]);
        uint64_t triangles = 0;
        for (uint32_t i=0; i<header[3]; ++i) {
            uint32_t desc[3];
            if (!read(desc, sizeof(desc)) || desc[0] >= materials.size() || !desc[1] || desc[1] > 2000000 ||
                !desc[2] || desc[2] > 6000000 || desc[2]%3 || uint64_t(desc[1])*32 + uint64_t(desc[2])*4 > remaining)
                return error("invalid surface counts");
            auto& v = vertices[i]; auto& ix = indices[i]; v.resize(desc[1]); ix.resize(desc[2]);
            for (auto& vertex : v) {
                float data[8]; if (!read(data, sizeof(data))) return error("truncated vertex");
                for (float value : data) if (!std::isfinite(value)) return error("invalid vertex");
                memcpy(vertex.position, data, 12); memcpy(vertex.normal, data+3, 12);
                memcpy(vertex.texcoord, data+6, 8); vertex.color = 0xffffffff;
            }
            if (!read(ix.data(), ix.size()*4)) return error("truncated indices");
            for (auto index : ix) if (index >= v.size()) return error("invalid vertex index");
            auto& surface = surfaces[i]; surface.vertices_values = v.data(); surface.vertices_count = v.size();
            surface.indices_values = ix.data(); surface.indices_count = ix.size(); surface.material = materials[desc[0]];
            triangles += ix.size()/3;
        }
        if (remaining) return error("unexpected trailing data");
        remixapi_MeshInfo info{}; info.sType = REMIXAPI_STRUCT_TYPE_MESH_INFO;
        info.hash = 0x415544494d000000ull + generation;
        info.surfaces_values = surfaces.data(); info.surfaces_count = uint32_t(surfaces.size());
        if (api.CreateMesh(&info, &mesh) != REMIXAPI_ERROR_CODE_SUCCESS || !mesh) return error("mesh creation failed");
        gEngfuncs.Con_Printf("D3D9: Audi replacement loaded: %u surfaces, %llu triangles\n", info.surfaces_count, triangles);
        return true;
    }
public:
    bool draw(const cl_entity_s* ent) {
        if (dx_cof_audi200.value == 0.f || dx_remix.value == 0.f || !g_worldmodel || Q_stricmp(g_worldmodel->name, "maps/c_start2.bsp") ||
            !ent || !ent->model || Q_stricmp(ent->model->name, "models/Props/Fordon/aud.mdl")) return false;
        if (!load()) return false;
        D3DMATRIX world{}; R_SetEntityWorldTransform(ent, R_CurrentRenderTime(0.f), &world);
        remixapi_InstanceInfo instance{}; instance.sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;
        instance.mesh = mesh; instance.doubleSided = 1;
        for (int row=0;row<3;++row) for (int col=0;col<4;++col) instance.transform.matrix[row][col] = world.m[col][row];
        return api.DrawInstance(&instance) == REMIXAPI_ERROR_CODE_SUCCESS;
    }
};
static Asset asset;
}
