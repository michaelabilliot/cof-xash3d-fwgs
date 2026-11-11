#pragma once

#include <d3d9.h>

inline constexpr int DX_STUDIO_GPU_SKIN_CONST_ZERO = 0;
inline constexpr int DX_STUDIO_GPU_SKIN_CONST_DIFFUSE = 1;
inline constexpr int DX_STUDIO_GPU_SKIN_CONST_LIGHTDIR = 2;
inline constexpr int DX_STUDIO_GPU_SKIN_CONST_LIGHTSCALE = 3;
inline constexpr int DX_STUDIO_GPU_SKIN_CONST_WVP = 4;
inline constexpr int DX_STUDIO_GPU_SKIN_CONST_WORLD = 8;
inline constexpr int DX_STUDIO_GPU_SKIN_CONST_VIEWORIGIN = 11;
inline constexpr int DX_STUDIO_GPU_SKIN_CONST_VIEWRIGHT = 12;
inline constexpr int DX_STUDIO_GPU_SKIN_CONST_VIEWUP = 13;
inline constexpr int DX_STUDIO_GPU_SKIN_CONST_CHROMESCALE = 14;
inline constexpr int DX_STUDIO_GPU_SKIN_CONST_SHELL = 15;
inline constexpr int DX_STUDIO_GPU_SKIN_CONST_BONES = 16;
inline constexpr int DX_STUDIO_GPU_SKIN_CONST_LOCAL_LIGHT0 = 232;

// D3DTS_WORLDMATRIX accepts indices 0-255, and blend indices are bytes.
inline constexpr int DX_STUDIO_REMIX_BLEND_MAX_BONES = 256;

bool DX_EnsureStudioSkinningShaders( void );
void DX_FreeStudioSkinningShaders( void );
int DX_StudioGpuSkinningBoneCap( void );
void DX_BindStudioGpuSkinningShader( bool localLights );
bool DX_BindStudioRemixBlendDeclaration( void );
