
#include "dx_local.h"
#include "dx_studio_shader.h"

namespace
{
	IDirect3DVertexDeclaration9 *s_studioGpuSkinDeclaration = nullptr;
	IDirect3DVertexShader9 *s_studioGpuSkinShader = nullptr;
	IDirect3DVertexShader9 *s_studioGpuSkinLocalShader = nullptr;
	bool s_studioGpuSkinTriedCreate = false;
	int s_studioGpuSkinMaxConstants = 0;
}

void DX_FreeStudioSkinningShaders( void )
{
	if( s_studioGpuSkinShader )
	{
		s_studioGpuSkinShader->Release( );
		s_studioGpuSkinShader = nullptr;
	}
	if( s_studioGpuSkinLocalShader )
	{
		s_studioGpuSkinLocalShader->Release( );
		s_studioGpuSkinLocalShader = nullptr;
	}
	if( s_studioGpuSkinDeclaration )
	{
		s_studioGpuSkinDeclaration->Release( );
		s_studioGpuSkinDeclaration = nullptr;
	}
	s_studioGpuSkinTriedCreate = false;
	s_studioGpuSkinMaxConstants = 0;
}

bool DX_EnsureStudioSkinningShaders( void )
{
	if( s_studioGpuSkinShader && s_studioGpuSkinLocalShader && s_studioGpuSkinDeclaration )
		return true;
	if( s_studioGpuSkinTriedCreate )
		return false;
	s_studioGpuSkinTriedCreate = true;
	if( !device )
		return false;

	D3DCAPS9 caps;
	if( FAILED( device->GetDeviceCaps( &caps ) ) ||
		caps.VertexShaderVersion < D3DVS_VERSION( 2, 0 ) )
	{
		gEngfuncs.Con_DPrintf( "D3D9: studio GPU skinning unavailable: vertex shader 2.0 required\n" );
		return false;
	}
	s_studioGpuSkinMaxConstants = static_cast<int>( caps.MaxVertexShaderConst );

	static const D3DVERTEXELEMENT9 declaration[] =
	{
		{ 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
		{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		{ 0, 32, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
		{ 0, 36, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2 },
		{ 0, 40, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 3 },
		D3DDECL_END( )
	};
	if( FAILED( device->CreateVertexDeclaration( declaration, &s_studioGpuSkinDeclaration ) ) ||
		!s_studioGpuSkinDeclaration )
	{
		gEngfuncs.Con_DPrintf( "D3D9: failed to create studio GPU skinning declaration\n" );
		DX_FreeStudioSkinningShaders( );
		s_studioGpuSkinTriedCreate = true;
		return false;
	}

	static const char *shaderText =
		"vs_2_0\n"
		"dcl_position v0\n"
		"dcl_normal v1\n"
		"dcl_texcoord v2\n"
		"dcl_texcoord1 v3\n"
		"dcl_texcoord2 v4\n"
		"dcl_texcoord3 v5\n"
		"mova a0.x, v3.x\n"
		"dp4 r0.x, v0, c[a0.x + 0]\n"
		"dp4 r0.y, v0, c[a0.x + 1]\n"
		"dp4 r0.z, v0, c[a0.x + 2]\n"
		"mov r0.w, c0.x\n"
		"mova a0.x, v4.x\n"
		"dp3 r1.x, v1, c[a0.x + 0]\n"
		"dp3 r1.y, v1, c[a0.x + 1]\n"
		"dp3 r1.z, v1, c[a0.x + 2]\n"
		"dp3 r2.x, r1, r1\n"
		"max r2.x, r2.x, c3.w\n"
		"rsq r2.x, r2.x\n"
		"mul r1.xyz, r1.xyz, r2.x\n"

		"mad r0.xyz, r1.xyz, c15.x, r0.xyz\n"
		"m4x4 oPos, r0, c4\n"

		"dp3 r3.x, r1, c2\n"
		"min r3.x, r3.x, c0.x\n"
		"max r3.x, r3.x, -c0.x\n"
		"add r3.x, r3.x, c3.y\n"
		"mul r3.x, r3.x, c3.z\n"
		"max r3.x, r3.x, c0.y\n"
		"mul r3.x, r3.x, c3.x\n"
		"sub r3.x, c0.x, r3.x\n"
		"max r3.x, r3.x, c0.y\n"

		"mul r4, c1, r3.x\n"
		"mov r4.w, c1.w\n"
		"sub r11.xyz, r0.xyz, c232.xyz\n"
		"dp3 r11.w, r11, r11\n"
		"max r11.w, r11.w, c3.w\n"
		"rsq r2.x, r11.w\n"
		"dp3 r3.x, r1, r11\n"
		"mul r3.x, r3.x, -c0.x\n"
		"max r3.x, r3.x, c0.y\n"
		"mul r2.y, r2.x, r2.x\n"
		"mul r2.y, r2.y, r2.x\n"
		"mul r2.y, r2.y, c232.w\n"
		"mul r2.y, r2.y, r3.x\n"
		"mul r2.y, r2.y, c233.w\n"
		"mad r4.xyz, c233.xyz, r2.y, r4.xyz\n"
		"sub r11.xyz, r0.xyz, c234.xyz\n"
		"dp3 r11.w, r11, r11\n"
		"max r11.w, r11.w, c3.w\n"
		"rsq r2.x, r11.w\n"
		"dp3 r3.x, r1, r11\n"
		"mul r3.x, r3.x, -c0.x\n"
		"max r3.x, r3.x, c0.y\n"
		"mul r2.y, r2.x, r2.x\n"
		"mul r2.y, r2.y, r2.x\n"
		"mul r2.y, r2.y, c234.w\n"
		"mul r2.y, r2.y, r3.x\n"
		"mul r2.y, r2.y, c235.w\n"
		"mad r4.xyz, c235.xyz, r2.y, r4.xyz\n"
		"sub r11.xyz, r0.xyz, c236.xyz\n"
		"dp3 r11.w, r11, r11\n"
		"max r11.w, r11.w, c3.w\n"
		"rsq r2.x, r11.w\n"
		"dp3 r3.x, r1, r11\n"
		"mul r3.x, r3.x, -c0.x\n"
		"max r3.x, r3.x, c0.y\n"
		"mul r2.y, r2.x, r2.x\n"
		"mul r2.y, r2.y, r2.x\n"
		"mul r2.y, r2.y, c236.w\n"
		"mul r2.y, r2.y, r3.x\n"
		"mul r2.y, r2.y, c237.w\n"
		"mad r4.xyz, c237.xyz, r2.y, r4.xyz\n"
		"sub r11.xyz, r0.xyz, c238.xyz\n"
		"dp3 r11.w, r11, r11\n"
		"max r11.w, r11.w, c3.w\n"
		"rsq r2.x, r11.w\n"
		"dp3 r3.x, r1, r11\n"
		"mul r3.x, r3.x, -c0.x\n"
		"max r3.x, r3.x, c0.y\n"
		"mul r2.y, r2.x, r2.x\n"
		"mul r2.y, r2.y, r2.x\n"
		"mul r2.y, r2.y, c238.w\n"
		"mul r2.y, r2.y, r3.x\n"
		"mul r2.y, r2.y, c239.w\n"
		"mad r4.xyz, c239.xyz, r2.y, r4.xyz\n"
		"min r4.xyz, r4.xyz, c0.x\n"
		"max r4.xyz, r4.xyz, c0.y\n"
		"mov oD0, r4\n"

		"dp3 r5.x, r1, c8\n"
		"dp3 r5.y, r1, c9\n"
		"dp3 r5.z, r1, c10\n"
		"dp3 r5.w, r5, r5\n"
		"max r5.w, r5.w, c3.w\n"
		"rsq r5.w, r5.w\n"
		"mul r5.xyz, r5.xyz, r5.w\n"

		"mov r6.x, c[a0.x + 0].w\n"
		"mov r6.y, c[a0.x + 1].w\n"
		"mov r6.z, c[a0.x + 2].w\n"
		"mov r6.w, c0.x\n"

		"dp4 r7.x, r6, c8\n"
		"dp4 r7.y, r6, c9\n"
		"dp4 r7.z, r6, c10\n"

		"sub r7.xyz, r7, c11\n"
		"dp3 r7.w, r7, r7\n"
		"max r7.w, r7.w, c3.w\n"
		"rsq r7.w, r7.w\n"
		"mul r7.xyz, r7.xyz, r7.w\n"

		"crs r8.xyz, r7, c12\n"
		"dp3 r8.w, r8, r8\n"
		"max r8.w, r8.w, c3.w\n"
		"rsq r8.w, r8.w\n"
		"mul r8.xyz, r8.xyz, r8.w\n"

		"crs r9.xyz, r8, r7\n"
		"dp3 r9.w, r9, r9\n"
		"max r9.w, r9.w, c3.w\n"
		"rsq r9.w, r9.w\n"
		"mul r9.xyz, r9.xyz, r9.w\n"

		"dp3 r10.x, r5, r9\n"
		"add r10.x, r10.x, c0.x\n"
		"mul r10.x, r10.x, c14.x\n"
		"dp3 r10.y, r5, r8\n"
		"add r10.y, r10.y, c0.x\n"
		"mul r10.y, r10.y, c14.y\n"

		"max r4.x, v5.x, c15.y\n"
		"sub r10.xy, r10.xy, v2.xy\n"
		"mad oT0.xy, r10.xy, r4.x, v2.xy\n";

	const std::string localShaderText( shaderText );
	std::string baseShaderText = localShaderText;
	const char *localBeginMarker = "mul r4, c1, r3.x\n";
	const char *localEndMarker = "dp3 r5.x, r1, c8\n";
	const size_t localBegin = baseShaderText.find( localBeginMarker );
	const size_t localEnd = baseShaderText.find( localEndMarker, localBegin );
	if( localBegin == std::string::npos || localEnd == std::string::npos || localEnd <= localBegin )
	{
		gEngfuncs.Con_DPrintf( "D3D9: failed to build base studio GPU skin shader variant\n" );
		DX_FreeStudioSkinningShaders( );
		s_studioGpuSkinTriedCreate = true;
		return false;
	}
	baseShaderText.replace( localBegin, localEnd - localBegin,
		"mul r4, c1, r3.x\n"
		"mov r4.w, c1.w\n"
		"mov oD0, r4\n" );

	auto assembleStudioShader = [&]( const std::string &source,
		IDirect3DVertexShader9 **outShader, const char *variantName ) -> bool
		{
			ID3DXBuffer *code = nullptr;
			ID3DXBuffer *errors = nullptr;
			HRESULT result = D3DXAssembleShader( source.c_str( ),
				static_cast<UINT>( source.size( ) ), nullptr, nullptr, 0, &code, &errors );
			if( FAILED( result ) || !code )
			{
				if( errors )
				{
					gEngfuncs.Con_DPrintf( "D3D9: studio GPU skin %s shader assemble failed: %s\n",
						variantName, static_cast<const char *>( errors->GetBufferPointer( ) ) );
					errors->Release( );
				}
				if( code )
					code->Release( );
				return false;
			}
			if( errors )
				errors->Release( );

			result = device->CreateVertexShader(
				static_cast<const DWORD *>( code->GetBufferPointer( ) ), outShader );
			code->Release( );
			if( FAILED( result ) || !*outShader )
			{
				gEngfuncs.Con_DPrintf( "D3D9: failed to create studio GPU skin %s shader\n",
					variantName );
				return false;
			}
			return true;
		};

	if( !assembleStudioShader( baseShaderText, &s_studioGpuSkinShader, "base" ) ||
		!assembleStudioShader( localShaderText, &s_studioGpuSkinLocalShader, "local-light" ) )
	{
		DX_FreeStudioSkinningShaders( );
		s_studioGpuSkinTriedCreate = true;
		return false;
	}
	return true;
}

int DX_StudioGpuSkinningBoneCap( void )
{
	const int cvarCap = std::max( 0, std::min( MAXSTUDIOBONES,
		static_cast<int>( dx_studio_gpu_max_bones.value ) ) );
	const int reservedConstantEnd = std::min( s_studioGpuSkinMaxConstants,
		DX_STUDIO_GPU_SKIN_CONST_LOCAL_LIGHT0 );
	const int hwCap = reservedConstantEnd > DX_STUDIO_GPU_SKIN_CONST_BONES
		? ( reservedConstantEnd - DX_STUDIO_GPU_SKIN_CONST_BONES ) / 3
		: 0;
	return std::max( 0, std::min( cvarCap, hwCap ) );
}

void DX_BindStudioGpuSkinningShader( bool localLights )
{
	device->SetVertexDeclaration( s_studioGpuSkinDeclaration );
	device->SetVertexShader( localLights ? s_studioGpuSkinLocalShader : s_studioGpuSkinShader );
}
