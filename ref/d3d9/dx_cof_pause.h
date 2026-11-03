#pragma once
#include "dx_cof_pause_shader.h"
namespace cof_pause {
template<class T> struct LocalCom {
    T* p = nullptr;
    ~LocalCom() { if (p) p->Release(); }
    T* operator->() const { return p; }
};
HRESULT blurBackground(IDirect3DDevice9* device, bool& sceneActive) {
    if (!device || !sceneActive) return S_FALSE;
    HRESULT hr = device->TestCooperativeLevel();
    if (FAILED(hr)) return hr;
    LocalCom<IDirect3DSurface9> target, copy, horizontal, depth;
    LocalCom<IDirect3DTexture9> texture, intermediate;
    LocalCom<IDirect3DPixelShader9> shader;
    LocalCom<IDirect3DStateBlock9> state;
    D3DSURFACE_DESC desc{};
    if (FAILED(hr = device->GetRenderTarget(0, &target.p)) || FAILED(hr = target->GetDesc(&desc))) return hr;
    if (!desc.Width || !desc.Height) return E_FAIL;
    hr = device->GetDepthStencilSurface(&depth.p);
    if (FAILED(hr) && hr != D3DERR_NOTFOUND) return hr;
    if (FAILED(hr = device->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, desc.Format, D3DPOOL_DEFAULT, &texture.p, nullptr)) ||
        FAILED(hr = texture->GetSurfaceLevel(0, &copy.p)) ||
        FAILED(hr = device->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, desc.Format, D3DPOOL_DEFAULT, &intermediate.p, nullptr)) ||
        FAILED(hr = intermediate->GetSurfaceLevel(0, &horizontal.p)) ||
        FAILED(hr = device->CreatePixelShader(CofPauseShader, &shader.p)) ||
        FAILED(hr = device->CreateStateBlock(D3DSBT_ALL, &state.p))) return hr;
    if (FAILED(hr = state->Capture())) return hr;
    // StretchRect occurs outside BeginScene/EndScene. Resume the same native
    // scene even if the copy fails; never Present.
    if (FAILED(hr = device->EndScene())) return hr;
    sceneActive = false;
    HRESULT copied = device->StretchRect(target.p, nullptr, copy.p, nullptr, D3DTEXF_NONE);
    if (FAILED(hr = device->BeginScene())) return hr;
    sceneActive = true;
    if (FAILED(copied)) return copied;
    const float radius = std::max(1.f, 1.f * float(desc.Height) / 1080.f);
    const float horizontalSettings[4] = {radius / desc.Width, 0, 1.f, 0};
    const float verticalSettings[4] = {0, radius / desc.Height, 1.f, 0};
    D3DVIEWPORT9 viewport{0, 0, desc.Width, desc.Height, 0.f, 1.f};
    struct Vertex { float x, y, z, rhw, u, v; };
    const float w = float(desc.Width), h = float(desc.Height);
    Vertex vertices[] = {{-.5f, -.5f, 0, 1, 0, 0}, {w-.5f, -.5f, 0, 1, 1, 0},
                         {-.5f, h-.5f, 0, 1, 0, 1}, {w-.5f, h-.5f, 0, 1, 1, 1}};
    // State blocks do not include render targets/depth surfaces. Restore those
    // explicitly, then Apply restores the viewport reset by SetRenderTarget.
    auto configure = [&]() -> HRESULT {
        HRESULT result;
#define GPU(call) do { if (FAILED(result = (call))) return result; } while (false)
        GPU(device->SetTexture(0, nullptr));
        GPU(device->SetDepthStencilSurface(nullptr));
        GPU(device->SetRenderTarget(0, horizontal.p));
        GPU(device->SetViewport(&viewport));
        GPU(device->SetVertexShader(nullptr));
        GPU(device->SetPixelShader(shader.p));
        GPU(device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1));
        GPU(device->SetPixelShaderConstantF(0, horizontalSettings, 1));
        GPU(device->SetTexture(0, texture.p));
        for (auto setting : {D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV}) GPU(device->SetSamplerState(0, setting, D3DTADDRESS_CLAMP));
        for (auto setting : {D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER}) GPU(device->SetSamplerState(0, setting, D3DTEXF_LINEAR));
        GPU(device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE));
        GPU(device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE));
        for (auto setting : {D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_ALPHABLENDENABLE, D3DRS_ALPHATESTENABLE,
                            D3DRS_FOGENABLE, D3DRS_STENCILENABLE, D3DRS_SCISSORTESTENABLE, D3DRS_SRGBWRITEENABLE,
                            D3DRS_CLIPPLANEENABLE}) GPU(device->SetRenderState(setting, 0));
        GPU(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE));
        GPU(device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID));
        GPU(device->SetRenderState(D3DRS_COLORWRITEENABLE, 15));
        GPU(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex)));
        GPU(device->SetTexture(0, nullptr));
        GPU(device->SetRenderTarget(0, target.p));
        GPU(device->SetViewport(&viewport));
        GPU(device->SetTexture(0, intermediate.p));
        GPU(device->SetPixelShaderConstantF(0, verticalSettings, 1));
        GPU(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex)));
        return S_OK;
#undef GPU
    };
    hr = configure();
    HRESULT unbound = device->SetTexture(0, nullptr);
    HRESULT targetRestored = device->SetRenderTarget(0, target.p);
    HRESULT depthRestored = device->SetDepthStencilSurface(depth.p);
    HRESULT restored = state->Apply();
    // No default-pool textures/state blocks survive this call or obstruct Reset.
    if (FAILED(unbound)) return unbound;
    if (FAILED(targetRestored)) return targetRestored;
    if (FAILED(depthRestored)) return depthRestored;
    return FAILED(restored) ? restored : hr;
}
}
