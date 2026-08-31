
#include "dx_local.h"

#ifndef D3D_PARTICLE_FVF
#define D3D_PARTICLE_FVF ( D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1 )
#endif

struct particle_vertex_t
{
	float x, y, z;
	D3DCOLOR diffuse;
	float u, v;
};

static std::vector<particle_vertex_t> g_dxParticleVerts;

static const color24 *R_ParticlePalette( )
{
	return reinterpret_cast<const color24 *>( ENGINE_GET_PARM( PARM_GET_PALETTE_PTR ) );
}

static byte R_ClampByte( int value )
{
	return static_cast<byte>( std::max( 0, std::min( 255, value ) ) );
}

static D3DCOLOR R_ParticleColor( const particle_t *particle, bool tracer )
{
	byte r = 255, g = 255, b = 255;

	if( tracer )
	{

		const int cr = static_cast<int>( gEngfuncs.pfnGetCvarFloat ?
			gEngfuncs.pfnGetCvarFloat( "tracerred" ) : 255.0f );
		const int cg = static_cast<int>( gEngfuncs.pfnGetCvarFloat ?
			gEngfuncs.pfnGetCvarFloat( "tracergreen" ) : 200.0f );
		const int cb = static_cast<int>( gEngfuncs.pfnGetCvarFloat ?
			gEngfuncs.pfnGetCvarFloat( "tracerblue" ) : 64.0f );
		r = R_ClampByte( cr );
		g = R_ClampByte( cg );
		b = R_ClampByte( cb );
	}
	else if( particle )
	{
		const int colorIndex = std::max( 0, std::min( 255, static_cast<int>( particle->color ) ) );
		const color24 *palette = R_ParticlePalette( );
		if( palette )
		{
			r = palette[colorIndex].r;
			g = palette[colorIndex].g;
			b = palette[colorIndex].b;
		}
		else
		{

			r = g = b = static_cast<byte>( colorIndex );
		}
	}

	int alpha = 255;
	if( particle && gp_cl && particle->die > gp_cl->time && particle->type != 0 )
	{

		alpha = static_cast<int>( 255.0f * static_cast<float>( particle->die - gp_cl->time ) * 16.0f );
		alpha = std::max( 0, std::min( 255, alpha ) );
	}

	if( tracer && gEngfuncs.pfnGetCvarFloat )
	{
		const float tracerAlpha = gEngfuncs.pfnGetCvarFloat( "traceralpha" );
		if( tracerAlpha > 0.0f )
			alpha = std::max( 0, std::min( 255, static_cast<int>( tracerAlpha ) ) );
	}

	return D3DCOLOR_ARGB( alpha, r, g, b );
}

static void R_BeginParticlePass( bool additive, bool remixParticleTexture )
{
	if( !device )
		return;

	DX_AllowFog( additive ? false : true );

	D3DXMATRIX identity;
	D3DXMatrixIdentity( &identity );
	device->SetTransform( D3DTS_WORLD, &identity );
	device->SetFVF( D3D_PARTICLE_FVF );

	const int particleTexture = remixParticleTexture
		? DX_FindTexture( REF_PARTICLE_TEXTURE ) : 0;
	if( particleTexture > 0 )
	{
		DX_Bind( 0, static_cast<unsigned int>( particleTexture ) );
		DX_SetTextureStageStateCached( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
		DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
	}
	else
	{
		DX_BindRawTexture( 0, nullptr );
		DX_SetTextureStageStateCached( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
		DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
		DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
		DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE );
	}

	DX_BindRawTexture( 1, nullptr );
	DX_SetTextureStageStateCached( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
	DX_SetTextureStageStateCached( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_NONE );
	DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
	if( remixParticleTexture && particleTexture > 0 )
	{
		// Keep particle fading, but reject fully transparent texels for Remix.
		DX_SetRenderStateCached( D3DRS_LIGHTING, FALSE );
		DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_ALPHAFUNC, D3DCMP_GREATER );
		DX_SetRenderStateCached( D3DRS_ALPHAREF, 1 );
	}
	else
	{
		DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
	}
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_BLENDOP, D3DBLENDOP_ADD );
	DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
	DX_SetRenderStateCached( D3DRS_DESTBLEND,
		additive ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA );
}

static void R_EndParticlePass( )
{
	if( !device )
		return;
	DX_BindRawTexture( 0, nullptr );
	device->SetFVF( BRUSH_FVF );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_CCW );
	DX_AllowFog( true );
}

static void R_FlushParticles( )
{
	if( g_dxParticleVerts.empty( ) )
		return;
	R_DrawScratch( D3DPT_TRIANGLELIST,
		static_cast<UINT>( g_dxParticleVerts.size( ) / 3 ),
		static_cast<UINT>( g_dxParticleVerts.size( ) ),
		g_dxParticleVerts.data( ), sizeof( particle_vertex_t ) );
	g_dxParticleVerts.clear( );
}

static void R_AppendParticleQuad( const vec3_t origin, float size,
	D3DCOLOR color )
{
	const float half = std::max( 0.25f, size * 0.5f );
	vec3_t right, up;
	VectorScale( g_vRight, half, right );
	VectorScale( g_vUp, half, up );

	const particle_vertex_t v0 =
	{
		origin[0] - right[0] - up[0], origin[1] - right[1] - up[1], origin[2] - right[2] - up[2],
		color, 0.0f, 1.0f
	};
	const particle_vertex_t v1 =
	{
		origin[0] - right[0] + up[0], origin[1] - right[1] + up[1], origin[2] - right[2] + up[2],
		color, 0.0f, 0.0f
	};
	const particle_vertex_t v2 =
	{
		origin[0] + right[0] + up[0], origin[1] + right[1] + up[1], origin[2] + right[2] + up[2],
		color, 1.0f, 0.0f
	};
	const particle_vertex_t v3 =
	{
		origin[0] + right[0] - up[0], origin[1] + right[1] - up[1], origin[2] + right[2] - up[2],
		color, 1.0f, 1.0f
	};

	g_dxParticleVerts.push_back( v0 );
	g_dxParticleVerts.push_back( v1 );
	g_dxParticleVerts.push_back( v2 );
	g_dxParticleVerts.push_back( v0 );
	g_dxParticleVerts.push_back( v2 );
	g_dxParticleVerts.push_back( v3 );
}

static void R_AppendTracer( const vec3_t start, const vec3_t end,
	float width, D3DCOLOR color )
{
	vec3_t direction, normal;
	VectorSubtract( end, start, direction );
	if( DotProduct( direction, direction ) < 0.0001f )
		return;
	CrossProduct( direction, g_vForward, normal );
	if( DotProduct( normal, normal ) < 0.0001f )
		VectorCopy( g_vRight, normal );
	else
	{
		const float invLen = 1.0f / sqrtf( DotProduct( normal, normal ) );
		VectorScale( normal, invLen, normal );
	}
	VectorScale( normal, std::max( 0.25f, width * 0.5f ), normal );

	const particle_vertex_t v0 = { start[0] - normal[0], start[1] - normal[1], start[2] - normal[2], color, 0.0f, 1.0f };
	const particle_vertex_t v1 = { start[0] + normal[0], start[1] + normal[1], start[2] + normal[2], color, 0.0f, 0.0f };
	const particle_vertex_t v2 = { end[0] + normal[0], end[1] + normal[1], end[2] + normal[2], color, 1.0f, 0.0f };
	const particle_vertex_t v3 = { end[0] - normal[0], end[1] - normal[1], end[2] - normal[2], color, 1.0f, 1.0f };
	g_dxParticleVerts.push_back( v0 );
	g_dxParticleVerts.push_back( v1 );
	g_dxParticleVerts.push_back( v2 );
	g_dxParticleVerts.push_back( v0 );
	g_dxParticleVerts.push_back( v2 );
	g_dxParticleVerts.push_back( v3 );
}

static void R_DrawParticles( particle_t *particles, float partsize,
	bool additive, bool tracerList )
{
	if( !device || !particles )
		return;
	if( tracerList ? dx_tracers.value == 0.0f : dx_particles.value == 0.0f )
		return;

	int maxDraw = static_cast<int>( dx_particle_limit.value );
	maxDraw = std::max( 0, std::min( 65536, maxDraw ) );
	if( maxDraw == 0 )
		return;

	R_BeginParticlePass( additive,
		dx_remix.value != 0.0f && !tracerList );
	g_dxParticleVerts.clear( );
	g_dxParticleVerts.reserve( 2048 * 6 );

	const float sizeScale = std::max( 0.05f, std::min( 16.0f, dx_particle_size_scale.value ) );
	const float baseParticleSize = std::max( 0.25f, partsize <= 0.0f ? 1.0f : partsize ) * sizeScale;
	const float tracerWidth = std::max( 0.25f, std::min( 64.0f, dx_tracer_width.value ) );
	const float tracerLengthScale = std::max( 0.05f, std::min( 64.0f, dx_tracer_length_scale.value ) );
	int drawn = 0;

	for( particle_t *p = particles; p && drawn < maxDraw; p = p->next )
	{
		if( gp_cl && p->die > 0.0f && p->die <= gp_cl->time )
			continue;

		if( tracerList )
		{
			vec3_t end, direction;
			VectorCopy( p->vel, direction );
			float lenSq = DotProduct( direction, direction );
			if( lenSq <= 0.0001f )
				continue;
			const float invLen = 1.0f / sqrtf( lenSq );
			VectorScale( direction, invLen, direction );
			float tracerLength = p->ramp > 0.0f ? p->ramp : 32.0f;
			tracerLength *= tracerLengthScale;
			VectorMA( p->org, -tracerLength, direction, end );
			R_AppendTracer( p->org, end, tracerWidth,
				R_ParticleColor( p, true ) );
		}
		else
		{
			float size = baseParticleSize;
			if( p->ramp > 0.0f && p->ramp < 64.0f )
				size *= std::max( 0.5f, std::min( 3.0f, 1.0f + p->ramp * 0.05f ) );
			R_AppendParticleQuad( p->org, size,
				R_ParticleColor( p, false ) );
		}

		++drawn;
		if( g_dxParticleVerts.size( ) >= 2048u * 6u )
			R_FlushParticles( );
	}

	R_FlushParticles( );
	R_EndParticlePass( );
}

void CL_DrawParticles( double frametime, particle_t *particles, float partsize )
{
	(void)frametime;
	R_DrawParticles( particles, partsize,
		dx_particle_additive.value != 0.0f, false );
}

void CL_DrawTracers( double frametime, particle_t *tracers )
{
	(void)frametime;
	R_DrawParticles( tracers, std::max( 1.0f, dx_tracer_width.value ), true, true );
}

#ifndef D3D_BEAM_MAX_SEGMENTS
#define D3D_BEAM_MAX_SEGMENTS 64
#endif

static constexpr int D3D_BEAM_NOISE_DIVISIONS = 64;
static constexpr int D3D_BEAM_RING_MAX_SEGMENTS = D3D_BEAM_NOISE_DIVISIONS * 8;
static constexpr float D3D_BEAM_PI = 3.14159265358979323846f;
static constexpr float D3D_BEAM_TAU = 6.28318530717958647692f;

static float g_dxBeamNoise[D3D_BEAM_NOISE_DIVISIONS + 1] = {};

static constexpr int DX_BEAM_ENTPOINT = 1;
static constexpr int DX_BEAM_ENTS = 2;
static constexpr int DX_BEAM_HOSE = 3;

struct beam_render_state_t
{
	DWORD fvf = 0;
	DWORD zEnable = FALSE, zWrite = FALSE, alphaBlend = FALSE, alphaTest = FALSE;
	DWORD srcBlend = D3DBLEND_ONE, dstBlend = D3DBLEND_ZERO, cull = D3DCULL_CCW;
	DWORD colorOp = D3DTOP_MODULATE, colorArg1 = D3DTA_TEXTURE, colorArg2 = D3DTA_DIFFUSE;
	DWORD alphaOp = D3DTOP_MODULATE, alphaArg1 = D3DTA_TEXTURE, alphaArg2 = D3DTA_DIFFUSE;
	DWORD stage1ColorOp = D3DTOP_DISABLE, stage1AlphaOp = D3DTOP_DISABLE;
	DWORD texCoordIndex = 0, textureTransformFlags = D3DTTFF_DISABLE;
	DWORD magFilter = D3DTEXF_LINEAR, minFilter = D3DTEXF_LINEAR;
	DWORD mipFilter = D3DTEXF_LINEAR, addressU = D3DTADDRESS_WRAP, addressV = D3DTADDRESS_WRAP;
	IDirect3DBaseTexture9 *texture0 = nullptr;
	IDirect3DBaseTexture9 *texture1 = nullptr;
};

static void R_SaveBeamRenderState( beam_render_state_t &state )
{
	if( dx_remix_perf.value != 0.0f )
		return;

	device->GetFVF( &state.fvf );
	device->GetRenderState( D3DRS_ZENABLE, &state.zEnable );
	device->GetRenderState( D3DRS_ZWRITEENABLE, &state.zWrite );
	device->GetRenderState( D3DRS_ALPHABLENDENABLE, &state.alphaBlend );
	device->GetRenderState( D3DRS_ALPHATESTENABLE, &state.alphaTest );
	device->GetRenderState( D3DRS_SRCBLEND, &state.srcBlend );
	device->GetRenderState( D3DRS_DESTBLEND, &state.dstBlend );
	device->GetRenderState( D3DRS_CULLMODE, &state.cull );
	device->GetTextureStageState( 0, D3DTSS_COLOROP, &state.colorOp );
	device->GetTextureStageState( 0, D3DTSS_COLORARG1, &state.colorArg1 );
	device->GetTextureStageState( 0, D3DTSS_COLORARG2, &state.colorArg2 );
	device->GetTextureStageState( 0, D3DTSS_ALPHAOP, &state.alphaOp );
	device->GetTextureStageState( 0, D3DTSS_ALPHAARG1, &state.alphaArg1 );
	device->GetTextureStageState( 0, D3DTSS_ALPHAARG2, &state.alphaArg2 );
	device->GetTextureStageState( 1, D3DTSS_COLOROP, &state.stage1ColorOp );
	device->GetTextureStageState( 1, D3DTSS_ALPHAOP, &state.stage1AlphaOp );
	device->GetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, &state.texCoordIndex );
	device->GetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, &state.textureTransformFlags );
	device->GetSamplerState( 0, D3DSAMP_MAGFILTER, &state.magFilter );
	device->GetSamplerState( 0, D3DSAMP_MINFILTER, &state.minFilter );
	device->GetSamplerState( 0, D3DSAMP_MIPFILTER, &state.mipFilter );
	device->GetSamplerState( 0, D3DSAMP_ADDRESSU, &state.addressU );
	device->GetSamplerState( 0, D3DSAMP_ADDRESSV, &state.addressV );
	device->GetTexture( 0, &state.texture0 );
	device->GetTexture( 1, &state.texture1 );
}

static void R_RestoreBeamState( beam_render_state_t &state )
{
	if( dx_remix_perf.value != 0.0f )
	{
		DX_AllowFog( true );
		device->SetFVF( BRUSH_FVF );
		DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
		DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
		DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_CCW );
		DX_SetTextureStageStateCached( 0, D3DTSS_TEXCOORDINDEX, 0 );
		DX_SetTextureStageStateCached( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
		DX_SetTextureStageStateCached( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
		DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
		DX_BindRawTexture( 1, nullptr );
		DX_SetTextureStageStateCached( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		DX_SetTextureStageStateCached( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
		return;
	}

	device->SetFVF( state.fvf );
	DX_SetRenderStateCached( D3DRS_ZENABLE, state.zEnable );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, state.zWrite );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, state.alphaBlend );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, state.alphaTest );
	DX_SetRenderStateCached( D3DRS_SRCBLEND, state.srcBlend );
	DX_SetRenderStateCached( D3DRS_DESTBLEND, state.dstBlend );
	DX_SetRenderStateCached( D3DRS_CULLMODE, state.cull );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLOROP, state.colorOp );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG1, state.colorArg1 );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG2, state.colorArg2 );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAOP, state.alphaOp );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG1, state.alphaArg1 );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG2, state.alphaArg2 );
	DX_SetTextureStageStateCached( 1, D3DTSS_COLOROP, state.stage1ColorOp );
	DX_SetTextureStageStateCached( 1, D3DTSS_ALPHAOP, state.stage1AlphaOp );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXCOORDINDEX, state.texCoordIndex );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXTURETRANSFORMFLAGS, state.textureTransformFlags );
	DX_SetSamplerStateCached( 0, D3DSAMP_MAGFILTER, state.magFilter );
	DX_SetSamplerStateCached( 0, D3DSAMP_MINFILTER, state.minFilter );
	DX_SetSamplerStateCached( 0, D3DSAMP_MIPFILTER, state.mipFilter );
	DX_SetSamplerStateCached( 0, D3DSAMP_ADDRESSU, state.addressU );
	DX_SetSamplerStateCached( 0, D3DSAMP_ADDRESSV, state.addressV );
	DX_BindRawTexture( 0, state.texture0 );
	DX_BindRawTexture( 1, state.texture1 );
	if( state.texture0 ) state.texture0->Release( );
	if( state.texture1 ) state.texture1->Release( );
	state.texture0 = state.texture1 = nullptr;
	DX_AllowFog( true );
}

static void R_BeginBeamPass( bool transparent, beam_render_state_t &state )
{
	if( dx_remix_perf.value == 0.0f )
		R_SaveBeamRenderState( state );
	DX_AllowFog( transparent ? false : true );
	device->SetFVF( BRUSH_FVF );
	DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, transparent ? FALSE : TRUE );
	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_NONE );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, transparent ? TRUE : FALSE );
	if( transparent )
	{

		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_ONE );
	}

	DX_BindRawTexture( 1, nullptr );
	DX_SetTextureStageStateCached( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
	DX_SetTextureStageStateCached( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXCOORDINDEX, 0 );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );

	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
}

static float R_BeamClamp01( float value )
{
	return std::max( 0.0f, std::min( 1.0f, value ) );
}

static bool R_BeamUsesSineNoise( const BEAM &beam )
{
	if( dx_beam_sine.value == 0.0f )
		return false;
	return FBitSet( beam.flags, FBEAM_SINENOISE ) || beam.amplitude > 0.001f;
}

static float R_BeamNoiseScale( const BEAM &beam )
{

	const float amplitude = fabsf( beam.amplitude );
	if( FBitSet( beam.flags, FBEAM_SINENOISE ) && amplitude > 0.0f && amplitude <= 1.25f )
		return std::max( 0.0f, std::min( 128.0f, dx_beam_sine_boost.value ) );
	return 1.0f;
}

static D3DCOLOR R_ScaleColorAlpha( D3DCOLOR color, float alphaScale )
{
	const int alpha = static_cast<int>( std::max( 0.0f, std::min( 255.0f,
		static_cast<float>( ( color >> 24 ) & 0xFF ) * alphaScale ) ) );
	return ( color & 0x00FFFFFFu ) | ( static_cast<DWORD>( alpha ) << 24 );
}

static float R_BeamNormalizeVector( vec3_t vector )
{
	const float length = VectorLength( vector );
	if( length > 0.00001f )
		VectorScale( vector, 1.0f / length, vector );
	else
		VectorClear( vector );
	return length;
}

static bool R_BeamPoint( int encodedEntity, vec3_t outPoint )
{
	if( !gEngfuncs.R_BeamGetEntity )
		return false;

	cl_entity_t *entity = gEngfuncs.R_BeamGetEntity( encodedEntity );
	if( !entity )
		return false;

	const int packedEntity = encodedEntity < 0 ? -encodedEntity : encodedEntity;
	const int attachment = BEAMENT_ATTACHMENT( packedEntity );
	if( attachment > 0 && attachment <= 4 )
	{

		if( R_GetAttachmentPoint( entity, attachment - 1, outPoint ) )
		{
			if( gp_cl && entity->index == gp_cl->playernum + 1 &&
				dx_viewmodel_beam_offset.value != 0.0f )
			{
				VectorMA( outPoint, dx_viewmodel_beam_offset.value,
					g_vForward, outPoint );
			}
			return true;
		}
		VectorCopy( entity->attachment[attachment - 1], outPoint );
	}
	else if( gp_cl && entity->index == gp_cl->playernum + 1 )
		VectorCopy( gp_cl->simorg, outPoint );
	else
		VectorCopy( entity->origin, outPoint );
	return true;
}

static bool R_UpdateTempBeam( BEAM *beam, float frameTime )
{
	if( !beam )
		return false;

	SetBits( beam->flags, FBEAM_ISACTIVE );
	beam->freq += std::max( 0.0f, frameTime );

	if( FBitSet( beam->flags, FBEAM_STARTENTITY ) )
	{
		cl_entity_t *startEntity = gEngfuncs.R_BeamGetEntity
			? gEngfuncs.R_BeamGetEntity( beam->startEntity ) : nullptr;
		if( R_BeamPoint( beam->startEntity, beam->source ) )
		{
			if( !beam->pFollowModel && startEntity )
				beam->pFollowModel = startEntity->model;
			SetBits( beam->flags, FBEAM_STARTVISIBLE );
		}
		else if( !FBitSet( beam->flags, FBEAM_FOREVER ) )
		{
			ClearBits( beam->flags, FBEAM_STARTENTITY );
		}
		else
		{
			ClearBits( beam->flags, FBEAM_ISACTIVE );
			return false;
		}
	}

	if( FBitSet( beam->flags, FBEAM_ENDENTITY ) )
	{
		cl_entity_t *endEntity = gEngfuncs.R_BeamGetEntity
			? gEngfuncs.R_BeamGetEntity( beam->endEntity ) : nullptr;
		if( R_BeamPoint( beam->endEntity, beam->target ) )
		{
			if( !beam->pFollowModel && endEntity )
				beam->pFollowModel = endEntity->model;
			SetBits( beam->flags, FBEAM_ENDVISIBLE );
		}
		else if( !FBitSet( beam->flags, FBEAM_FOREVER ) )
		{
			ClearBits( beam->flags, FBEAM_ENDENTITY );
			if( gp_cl )
				beam->die = gp_cl->time;
			ClearBits( beam->flags, FBEAM_ISACTIVE );
			return false;
		}
		else
		{
			ClearBits( beam->flags, FBEAM_ISACTIVE );
			return false;
		}
	}

	if( FBitSet( beam->flags, FBEAM_STARTENTITY ) &&
		!FBitSet( beam->flags, FBEAM_STARTVISIBLE ) )
	{
		ClearBits( beam->flags, FBEAM_ISACTIVE );
		return false;
	}

	if( beam->type == TE_BEAMFOLLOW )
	{
		if( beam->width <= 0.0f )
		{
			ClearBits( beam->flags, FBEAM_ISACTIVE );
			return false;
		}
		return true;
	}

	VectorSubtract( beam->target, beam->source, beam->delta );
	const float length = VectorLength( beam->delta );
	if( length < 0.1f || beam->width <= 0.0f )
	{
		ClearBits( beam->flags, FBEAM_ISACTIVE );
		return false;
	}

	beam->segments = static_cast<int>( length *
		( beam->amplitude >= 0.50f ? 0.25f : 0.075f ) + 3.0f );
	if( FBitSet( beam->flags, FBEAM_FADEIN | FBEAM_FADEOUT ) && gp_cl )
	{
		beam->t = beam->freq + ( beam->die - gp_cl->time );
		if( beam->t != 0.0f )
			beam->t = 1.0f - beam->freq / beam->t;
	}
	return true;
}

static bool R_BeamFrameTexture( const BEAM &beam, model_t *model, int &outTexture )
{
	outTexture = 0;
	if( !model || model->type != mod_sprite || !gEngfuncs.R_GetSpriteFrame )
		return false;

	const int frameCount = std::max( 1, beam.frameCount > 0 ? beam.frameCount : model->numframes );
	int frameNumber = static_cast<int>( floorf( beam.frame +
		beam.frameRate * ( gp_cl ? gp_cl->time : 0.0f ) ) );
	frameNumber %= frameCount;
	if( frameNumber < 0 ) frameNumber += frameCount;

	mspriteframe_t *frame = static_cast<mspriteframe_t *>(
		gEngfuncs.R_GetSpriteFrame( model, static_cast<float>( frameNumber ), 0.0f ) );
	if( !frame || frame->gl_texturenum <= 0 )
		return false;
	outTexture = frame->gl_texturenum;
	return true;
}

static D3DCOLOR R_BeamColor( const BEAM &beam )
{
	float alpha = R_BeamClamp01( beam.brightness );
	if( FBitSet( beam.flags, FBEAM_FADEIN ) ) alpha *= R_BeamClamp01( beam.t );
	if( FBitSet( beam.flags, FBEAM_FADEOUT ) ) alpha *= 1.0f - R_BeamClamp01( beam.t );

	const auto toByte = []( float v ) -> byte
		{
			return static_cast<byte>( R_BeamClamp01( v ) * 255.0f + 0.5f );
		};
	return D3DCOLOR_ARGB( toByte( alpha ), toByte( beam.r ),
		toByte( beam.g ), toByte( beam.b ) );
}

static D3DCOLOR R_BeamColorScaled( const BEAM &beam, float alphaScale )
{
	const auto toByte = []( float v ) -> byte
		{
			return static_cast<byte>( R_BeamClamp01( v ) * 255.0f + 0.5f );
		};
	return D3DCOLOR_ARGB( toByte( R_BeamClamp01( beam.brightness ) * alphaScale ),
		toByte( beam.r ), toByte( beam.g ), toByte( beam.b ) );
}

static void R_BeamFracNoise( float *noise, int divs )
{
	const int div2 = divs >> 1;
	if( divs < 2 )
		return;

	noise[div2] = ( noise[0] + noise[divs] ) * 0.5f +
		static_cast<float>( divs ) *
		gEngfuncs.COM_RandomFloat( -0.125f, 0.125f );

	if( div2 > 1 )
	{
		R_BeamFracNoise( &noise[div2], div2 );
		R_BeamFracNoise( noise, div2 );
	}
}

static void R_BeamSineNoise( float *noise, int divs )
{
	float frequency = 0.0f;
	const float step = D3D_BEAM_PI / static_cast<float>( divs );
	for( int i = 0; i < divs; ++i )
	{
		noise[i] = sinf( frequency );
		frequency += step;
	}
}

static void R_PrepareBeamNoise( const BEAM &beam, float frameTime )
{

	if( frameTime == 0.0f )
		return;

	g_dxBeamNoise[0] = 0.0f;
	g_dxBeamNoise[D3D_BEAM_NOISE_DIVISIONS] = 0.0f;
	if( beam.amplitude == 0.0f )
		return;

	if( FBitSet( beam.flags, FBEAM_SINENOISE ) )
		R_BeamSineNoise( g_dxBeamNoise, D3D_BEAM_NOISE_DIVISIONS );
	else
		R_BeamFracNoise( g_dxBeamNoise, D3D_BEAM_NOISE_DIVISIONS );
}

static void R_BeamPerpendicular( const vec3_t beamDelta, vec3_t outPerp )
{
	vec3_t center;
	VectorCopy( beamDelta, center );
	if( R_BeamNormalizeVector( center ) < 0.00001f )
	{
		VectorCopy( g_vRight, outPerp );
		return;
	}

	CrossProduct( g_vForward, center, outPerp );
	if( R_BeamNormalizeVector( outPerp ) < 0.00001f )
		VectorCopy( g_vRight, outPerp );
}

static void R_BeamNormal( const vec3_t startPos,
	const vec3_t nextPos, vec3_t outNormal )
{
	vec3_t tangent, viewer;
	VectorSubtract( startPos, nextPos, tangent );
	VectorSubtract( startPos, g_vOrigin, viewer );
	CrossProduct( tangent, viewer, outNormal );
	if( R_BeamNormalizeVector( outNormal ) < 0.00001f )
		VectorCopy( g_vRight, outNormal );
}

static D3DCOLOR R_BeamVertexColor( const BEAM &beam,
	float brightness, float extraAlpha = 1.0f )
{

	float intensity = R_BeamClamp01( beam.brightness );
	if( FBitSet( beam.flags, FBEAM_FADEIN ) )
		intensity *= R_BeamClamp01( beam.t );
	if( FBitSet( beam.flags, FBEAM_FADEOUT ) )
		intensity *= 1.0f - R_BeamClamp01( beam.t );
	intensity *= R_BeamClamp01( brightness ) * R_BeamClamp01( extraAlpha );

	const auto toByte = []( float value ) -> byte
		{
			return static_cast<byte>(
				R_BeamClamp01( value ) * 255.0f + 0.5f );
		};
	return D3DCOLOR_ARGB( 255,
		toByte( beam.r * intensity ),
		toByte( beam.g * intensity ),
		toByte( beam.b * intensity ) );
}

struct beam_segment_t
{
	vec3_t pos;
	float texcoord;
	float width;
};

static void R_DrawBeamSegments( const BEAM &beam, int texture,
	float extraAlpha = 1.0f )
{
	int segments = std::max( 2,
		std::min( D3D_BEAM_NOISE_DIVISIONS, beam.segments ) );
	const float width = std::max( 0.001f, beam.width *
		std::max( 0.05f, std::min( 16.0f, dx_beam_width_scale.value ) ) );

	const float worldLength = VectorLength( beam.delta );
	if( worldLength < 0.1f )
		return;

	float div = 1.0f / static_cast<float>( segments - 1 );
	const float maxWidth = width * 0.5f;
	if( maxWidth > 0.0f && worldLength * div < maxWidth * 1.41421356f )
	{
		segments = static_cast<int>(
			worldLength / ( maxWidth * 1.41421356f ) ) + 1;
		segments = std::max( 2, std::min( D3D_BEAM_NOISE_DIVISIONS, segments ) );
	}

	div = 1.0f / static_cast<float>( segments - 1 );
	float textureLength = worldLength * 0.01f;
	float vStep = textureLength * div;
	float vLast = fmodf( beam.freq * beam.speed, 1.0f );

	float noiseScale = beam.amplitude;
	if( FBitSet( beam.flags, FBEAM_SINENOISE ) )
	{
		if( segments < 16 )
		{
			segments = 16;
			div = 1.0f / static_cast<float>( segments - 1 );
		}
		noiseScale *= 100.0f;
		textureLength = static_cast<float>( segments ) * 0.1f;

	}
	else
	{
		noiseScale *= textureLength * 2.0f;
	}

	const int noiseStep = static_cast<int>(
		static_cast<float>( D3D_BEAM_NOISE_DIVISIONS - 1 ) *
		div * 65536.0f );
	int noiseIndex = 0;
	float brightness = FBitSet( beam.flags, FBEAM_SHADEIN ) ? 0.0f : 1.0f;

	vec3_t perpendicular;
	R_BeamPerpendicular( beam.delta, perpendicular );

	brush_vertex_t vertices[D3D_BEAM_NOISE_DIVISIONS * 2];
	int vertexCount = 0;
	int segmentsDrawn = 0;
	beam_segment_t current;
	vec3_t lastNormal;
	bool haveLastNormal = false;

	for( int i = 0; i < segments; ++i )
	{
		const float fraction = static_cast<float>( i ) * div;
		beam_segment_t next;
		VectorMA( beam.source, fraction, beam.delta, next.pos );

		if( noiseScale != 0.0f )
		{
			const int sampleIndex = std::max( 0, std::min(
				D3D_BEAM_NOISE_DIVISIONS - 1, noiseIndex >> 16 ) );
			const float factor = g_dxBeamNoise[sampleIndex] * noiseScale;
			if( FBitSet( beam.flags, FBEAM_SINENOISE ) )
			{
				const float phase =
					fraction * D3D_BEAM_PI * textureLength + beam.freq;
				const float s = sinf( phase );
				const float c = cosf( phase );
				VectorMA( next.pos, factor * s, g_vUp, next.pos );
				VectorMA( next.pos, factor * c, g_vRight, next.pos );
			}
			else
			{
				VectorMA( next.pos, factor, perpendicular, next.pos );
			}
		}

		next.width = width * 2.0f;
		next.texcoord = vLast;

		if( segmentsDrawn > 0 )
		{
			vec3_t normal, averageNormal;
			R_BeamNormal( current.pos, next.pos, normal );
			if( segmentsDrawn > 1 && haveLastNormal )
			{
				VectorAdd( normal, lastNormal, averageNormal );
				VectorScale( averageNormal, 0.5f, averageNormal );
				if( R_BeamNormalizeVector( averageNormal ) < 0.00001f )
					VectorCopy( normal, averageNormal );
			}
			else
			{
				VectorCopy( normal, averageNormal );
			}

			VectorCopy( normal, lastNormal );
			haveLastNormal = true;

			vec3_t point1, point2;
			VectorMA( current.pos, current.width * 0.5f,
				averageNormal, point1 );
			VectorMA( current.pos, -current.width * 0.5f,
				averageNormal, point2 );

			const D3DCOLOR color =
				R_BeamVertexColor( beam, brightness, extraAlpha );
			brush_vertex_t &a = vertices[vertexCount++];
			a.x = point1[0]; a.y = point1[1]; a.z = point1[2];
			a.diffuse = color; a.u = 0.0f; a.v = current.texcoord;
			brush_vertex_t &b = vertices[vertexCount++];
			b.x = point2[0]; b.y = point2[1]; b.z = point2[2];
			b.diffuse = color; b.u = 1.0f; b.v = current.texcoord;
		}

		current = next;
		++segmentsDrawn;

		if( FBitSet( beam.flags, FBEAM_SHADEIN ) &&
			FBitSet( beam.flags, FBEAM_SHADEOUT ) )
		{
			brightness = fraction < 0.5f ? fraction : ( 1.0f - fraction );
		}
		else if( FBitSet( beam.flags, FBEAM_SHADEIN ) )
		{
			brightness = fraction;
		}
		else if( FBitSet( beam.flags, FBEAM_SHADEOUT ) )
		{
			brightness = 1.0f - fraction;
		}

		if( segmentsDrawn == segments && haveLastNormal )
		{
			vec3_t point1, point2;
			VectorMA( current.pos, current.width * 0.5f,
				lastNormal, point1 );
			VectorMA( current.pos, -current.width * 0.5f,
				lastNormal, point2 );
			const D3DCOLOR color =
				R_BeamVertexColor( beam, brightness, extraAlpha );

			brush_vertex_t &a = vertices[vertexCount++];
			a.x = point1[0]; a.y = point1[1]; a.z = point1[2];
			a.diffuse = color; a.u = 0.0f; a.v = current.texcoord;
			brush_vertex_t &b = vertices[vertexCount++];
			b.x = point2[0]; b.y = point2[1]; b.z = point2[2];
			b.diffuse = color; b.u = 1.0f; b.v = current.texcoord;
		}

		vLast += vStep;
		noiseIndex += noiseStep;
	}

	if( vertexCount >= 4 )
	{
		DX_Bind( 0, texture );
		R_DrawScratch( D3DPT_TRIANGLESTRIP,
			static_cast<UINT>( vertexCount - 2 ),
			static_cast<UINT>( vertexCount ),
			vertices, sizeof( brush_vertex_t ) );
	}
}

static float R_BeamHoseFade( const BEAM &beam )
{
	vec3_t direction;
	VectorSubtract( beam.target, beam.source, direction );
	if( R_BeamNormalizeVector( direction ) < 0.00001f )
		return 0.0f;

	float dot = DotProduct( direction, g_vForward );

	if( dot > 0.0f )
		return 0.0f;

	float fade = powf( dot, 10.0f );
	vec3_t localDirection;
	VectorSubtract( g_vOrigin, beam.source, localDirection );
	dot = DotProduct( direction, localDirection );

	vec3_t projection, perpendicular;
	VectorScale( direction, dot, projection );
	VectorSubtract( localDirection, projection, perpendicular );
	const float distance = VectorLength( perpendicular );
	if( distance > 30.0f )
	{
		const float distanceFade = 1.0f - ( ( distance - 30.0f ) / 64.0f );
		if( distanceFade <= 0.0f )
			return 0.0f;
		fade *= powf( distanceFade, 3.0f );
	}

	return fade < ( 1.0f / 255.0f ) ? 0.0f :
		std::max( 0.0f, std::min( 1.0f, fade ) );
}

static void R_AppendBeamRadialPair( brush_vertex_t *vertices, int &vertexCount,
	int maxVertices, const vec3_t point, const vec3_t previousPoint,
	float width, float texV, D3DCOLOR color, bool flipNormal = false )
{
	if( vertexCount + 2 > maxVertices )
		return;

	vec3_t tangent, viewer, normal;
	VectorSubtract( previousPoint, point, tangent );
	VectorSubtract( point, g_vOrigin, viewer );
	CrossProduct( tangent, viewer, normal );
	if( R_BeamNormalizeVector( normal ) < 0.00001f )
		VectorCopy( g_vRight, normal );
	if( flipNormal )
	{
		normal[0] = -normal[0];
		normal[1] = -normal[1];
		normal[2] = -normal[2];
	}

	vec3_t p1, p2;
	VectorMA( point, width, normal, p1 );
	VectorMA( point, -width, normal, p2 );

	brush_vertex_t &a = vertices[vertexCount++];
	a.x = p2[0]; a.y = p2[1]; a.z = p2[2];
	a.diffuse = color; a.u = 1.0f; a.v = texV;
	brush_vertex_t &b = vertices[vertexCount++];
	b.x = p1[0]; b.y = p1[1]; b.z = p1[2];
	b.diffuse = color; b.u = 0.0f; b.v = texV;
}

static void R_DrawBeamTorus( const BEAM &beam, int texture )
{
	int segments = std::max( 2,
		std::min( D3D_BEAM_NOISE_DIVISIONS, beam.segments ) );
	const float width = std::max( 0.001f, beam.width *
		std::max( 0.05f, std::min( 16.0f, dx_beam_width_scale.value ) ) );

	float length = VectorLength( beam.delta ) * 0.01f;
	if( length < 0.5f ) length = 0.5f;
	const float div = 1.0f / static_cast<float>( segments - 1 );
	const float vStep = length * div;
	float vLast = fmodf( beam.freq * beam.speed, 1.0f );
	const float scale = beam.amplitude * length;
	const int noiseStep = static_cast<int>(
		static_cast<float>( D3D_BEAM_NOISE_DIVISIONS - 1 ) *
		div * 65536.0f );
	int noiseIndex = 0;

	brush_vertex_t vertices[D3D_BEAM_NOISE_DIVISIONS * 2];
	int vertexCount = 0;
	vec3_t previousPoint;
	bool havePrevious = false;
	const D3DCOLOR color = R_BeamVertexColor( beam, 1.0f );

	for( int i = 0; i < segments; ++i )
	{
		const float fraction = static_cast<float>( i ) * div;
		const float s = sinf( fraction * D3D_BEAM_TAU );
		const float c = cosf( fraction * D3D_BEAM_TAU );
		vec3_t point =
		{
			s * beam.freq * beam.delta[2] + beam.source[0],
			c * beam.freq * beam.delta[2] + beam.source[1],
			beam.source[2]
		};

		if( scale != 0.0f && ( noiseIndex >> 16 ) < D3D_BEAM_NOISE_DIVISIONS )
		{
			const float factor = g_dxBeamNoise[noiseIndex >> 16] * scale;
			VectorMA( point, factor, g_vUp, point );
			VectorMA( point,
				factor * cosf( fraction * D3D_BEAM_PI * 3.0f + beam.freq ),
				g_vRight, point );
		}

		if( havePrevious )
		{
			vLast += vStep;
			R_AppendBeamRadialPair( vertices, vertexCount,
				D3D_BEAM_NOISE_DIVISIONS * 2, point, previousPoint,
				width, vLast, color );
		}

		VectorCopy( point, previousPoint );
		havePrevious = true;
		noiseIndex += noiseStep;
	}

	if( vertexCount >= 4 )
	{
		DX_Bind( 0, texture );
		R_DrawScratch( D3DPT_TRIANGLESTRIP,
			static_cast<UINT>( vertexCount - 2 ),
			static_cast<UINT>( vertexCount ),
			vertices, sizeof( brush_vertex_t ) );
	}
}

static void R_DrawBeamDisk( const BEAM &beam, int texture )
{
	int segments = std::max( 2,
		std::min( D3D_BEAM_NOISE_DIVISIONS, beam.segments ) );
	const float width = std::max( 0.001f, beam.width *
		std::max( 0.05f, std::min( 16.0f, dx_beam_width_scale.value ) ) );
	float length = VectorLength( beam.delta ) * 0.01f;
	if( length < 0.5f ) length = 0.5f;
	const float div = 1.0f / static_cast<float>( segments - 1 );
	const float vStep = length * div;
	float vLast = fmodf( beam.freq * beam.speed, 1.0f );
	const float radius = fmodf( beam.freq,
		std::max( 0.0001f, width * 0.1f ) ) * beam.delta[2];
	const D3DCOLOR color = R_BeamVertexColor( beam, 1.0f );

	brush_vertex_t vertices[D3D_BEAM_NOISE_DIVISIONS * 2];
	int vertexCount = 0;
	for( int i = 0; i < segments; ++i )
	{
		const float fraction = static_cast<float>( i ) * div;
		const float s = sinf( fraction * D3D_BEAM_TAU );
		const float c = cosf( fraction * D3D_BEAM_TAU );

		brush_vertex_t &center = vertices[vertexCount++];
		center.x = beam.source[0];
		center.y = beam.source[1];
		center.z = beam.source[2];
		center.diffuse = color; center.u = 1.0f; center.v = vLast;

		brush_vertex_t &outer = vertices[vertexCount++];
		outer.x = s * radius + beam.source[0];
		outer.y = c * radius + beam.source[1];
		outer.z = beam.source[2];
		outer.diffuse = color; outer.u = 0.0f; outer.v = vLast;
		vLast += vStep;
	}

	if( vertexCount >= 4 )
	{
		DX_Bind( 0, texture );
		R_DrawScratch( D3DPT_TRIANGLESTRIP,
			static_cast<UINT>( vertexCount - 2 ),
			static_cast<UINT>( vertexCount ),
			vertices, sizeof( brush_vertex_t ) );
	}
}

static void R_DrawBeamCylinder( const BEAM &beam, int texture )
{
	int segments = std::max( 2,
		std::min( D3D_BEAM_NOISE_DIVISIONS, beam.segments ) );
	const float width = std::max( 0.001f, beam.width *
		std::max( 0.05f, std::min( 16.0f, dx_beam_width_scale.value ) ) );
	float length = VectorLength( beam.delta ) * 0.01f;
	if( length < 0.5f ) length = 0.5f;
	const float div = 1.0f / static_cast<float>( segments - 1 );
	const float vStep = length * div;
	float vLast = fmodf( beam.freq * beam.speed, 1.0f );
	const D3DCOLOR color = R_BeamVertexColor( beam, 1.0f );

	brush_vertex_t vertices[D3D_BEAM_NOISE_DIVISIONS * 2];
	int vertexCount = 0;
	for( int i = 0; i < segments; ++i )
	{
		const float fraction = static_cast<float>( i ) * div;
		const float s = sinf( fraction * D3D_BEAM_TAU );
		const float c = cosf( fraction * D3D_BEAM_TAU );

		brush_vertex_t &a = vertices[vertexCount++];
		a.x = s * beam.freq * beam.delta[2] + beam.source[0];
		a.y = c * beam.freq * beam.delta[2] + beam.source[1];
		a.z = beam.source[2] + width;
		a.diffuse = D3DCOLOR_ARGB( 255, 0, 0, 0 );
		a.u = 1.0f; a.v = vLast;

		brush_vertex_t &b = vertices[vertexCount++];
		b.x = s * beam.freq * ( beam.delta[2] + width ) + beam.source[0];
		b.y = c * beam.freq * ( beam.delta[2] + width ) + beam.source[1];
		b.z = beam.source[2] - width;
		b.diffuse = color;
		b.u = 0.0f; b.v = vLast;

		vLast += vStep;
	}

	if( vertexCount >= 4 )
	{
		DX_Bind( 0, texture );
		R_DrawScratch( D3DPT_TRIANGLESTRIP,
			static_cast<UINT>( vertexCount - 2 ),
			static_cast<UINT>( vertexCount ),
			vertices, sizeof( brush_vertex_t ) );
	}
}

static void R_DrawBeamRing( const BEAM &beam, int texture )
{
	int segments = static_cast<int>(
		static_cast<float>( std::max( 2, beam.segments ) ) * D3D_BEAM_PI );
	segments = std::max( 2,
		std::min( D3D_BEAM_RING_MAX_SEGMENTS, segments ) );

	float length = VectorLength( beam.delta ) * 0.01f * D3D_BEAM_PI;
	if( length < 0.5f ) length = 0.5f;
	const float div = 1.0f / static_cast<float>( segments - 1 );
	const float vStep = length * div / 8.0f;
	float vLast = fmodf( beam.freq * beam.speed, 1.0f );
	const float scale = beam.amplitude * length / 8.0f;
	const int noiseStep = static_cast<int>(
		static_cast<float>( D3D_BEAM_NOISE_DIVISIONS - 1 ) *
		div * 65536.0f ) * 8;
	int noiseIndex = 0;

	vec3_t halfDelta;
	VectorScale( beam.delta, 0.5f, halfDelta );
	vec3_t center;
	VectorAdd( beam.source, halfDelta, center );
	vec3_t xAxis;
	VectorCopy( halfDelta, xAxis );
	const float radius = VectorLength( xAxis );
	if( radius < 0.001f )
		return;

	vec3_t yAxis = { xAxis[1], -xAxis[0], 0.0f };
	if( R_BeamNormalizeVector( yAxis ) < 0.00001f )
		VectorCopy( g_vRight, yAxis );
	VectorScale( yAxis, radius, yAxis );

	const float width = std::max( 0.001f, beam.width *
		std::max( 0.05f, std::min( 16.0f, dx_beam_width_scale.value ) ) );
	const D3DCOLOR color = R_BeamVertexColor( beam, 1.0f );

	brush_vertex_t vertices[( D3D_BEAM_RING_MAX_SEGMENTS + 1 ) * 2];
	int vertexCount = 0;
	vec3_t previousPoint;
	bool havePrevious = false;
	int regenerateCountdown = std::max( 1, segments / 8 );

	for( int i = 0; i < segments + 1; ++i )
	{
		const float fraction = static_cast<float>( i ) * div;
		const float x = sinf( fraction * D3D_BEAM_TAU );
		const float y = cosf( fraction * D3D_BEAM_TAU );
		vec3_t point;
		point[0] = x * xAxis[0] + y * yAxis[0] + center[0];
		point[1] = x * xAxis[1] + y * yAxis[1] + center[1];
		point[2] = x * xAxis[2] + y * yAxis[2] + center[2];

		const int sampleIndex =
			( noiseIndex >> 16 ) & ( D3D_BEAM_NOISE_DIVISIONS - 1 );
		float factor = g_dxBeamNoise[sampleIndex] * scale;
		VectorMA( point, factor, g_vUp, point );
		factor *= cosf( fraction * D3D_BEAM_PI * 24.0f + beam.freq );
		VectorMA( point, factor, g_vRight, point );

		if( havePrevious )
		{
			vLast += vStep;
			R_AppendBeamRadialPair( vertices, vertexCount,
				( D3D_BEAM_RING_MAX_SEGMENTS + 1 ) * 2,
				point, previousPoint, width, vLast, color, true );
		}

		VectorCopy( point, previousPoint );
		havePrevious = true;
		noiseIndex += noiseStep;

		if( --regenerateCountdown == 0 && beam.amplitude != 0.0f )
		{
			regenerateCountdown = std::max( 1, segments / 8 );
			g_dxBeamNoise[0] = 0.0f;
			g_dxBeamNoise[D3D_BEAM_NOISE_DIVISIONS] = 0.0f;
			R_BeamFracNoise( g_dxBeamNoise,
				D3D_BEAM_NOISE_DIVISIONS );
		}
	}

	if( vertexCount >= 4 )
	{
		DX_Bind( 0, texture );
		R_DrawScratch( D3DPT_TRIANGLESTRIP,
			static_cast<UINT>( vertexCount - 2 ),
			static_cast<UINT>( vertexCount ),
			vertices, sizeof( brush_vertex_t ) );
	}
}

static bool R_DrawBeamGeometry( BEAM &beam, model_t *model )
{
	int texture = 0;
	if( !R_BeamFrameTexture( beam, model, texture ) )
		return false;

	switch( beam.type )
	{
	case TE_BEAMTORUS:
		R_DrawBeamTorus( beam, texture );
		return true;
	case TE_BEAMDISK:
		R_DrawBeamDisk( beam, texture );
		return true;
	case TE_BEAMCYLINDER:
		R_DrawBeamCylinder( beam, texture );
		return true;
	case TE_BEAMRING:
		R_DrawBeamRing( beam, texture );
		return true;
	case TE_BEAMPOINTS:
		R_DrawBeamSegments( beam, texture, 1.0f );
		return true;
	case TE_BEAMHOSE:
	{
		const float hoseFade = R_BeamHoseFade( beam );
		if( hoseFade > 0.0f )
			R_DrawBeamSegments( beam, texture, hoseFade );
		return true;
	}
	default:
		break;
	}

	return false;
}

static void R_DrawBeamStripLayer( const BEAM &beam, int texture,
	float noiseScale, float widthScale, float alphaScale, float phaseOffset,
	bool coilNoise )
{
	vec3_t beamDir;
	VectorCopy( beam.delta, beamDir );
	const float length = R_BeamNormalizeVector( beamDir );
	const float beamWidth = std::max( 0.05f, beam.width * widthScale *
		std::max( 0.05f, std::min( 16.0f, dx_beam_width_scale.value ) ) );
	if( length < 0.1f || beamWidth <= 0.0f )
		return;

	int segments = std::max( 2, std::min( D3D_BEAM_MAX_SEGMENTS, beam.segments ) );
	if( coilNoise || FBitSet( beam.flags, FBEAM_SINENOISE ) )
	{
		const int minSegments = std::max( 4, std::min( 24,
			static_cast<int>( dx_beam_sine_segments.value ) ) );
		const int closeSegments = static_cast<int>( ceilf( length / std::max( 4.0f, beamWidth * 0.65f ) ) ) + 1;
		segments = std::max( segments, std::max( minSegments, closeSegments ) );
		segments = std::min( D3D_BEAM_MAX_SEGMENTS, segments );
	}

	if( dx_beam_close_precision.value != 0.0f )
	{
		vec3_t toSource, toTarget;
		VectorSubtract( beam.source, g_vOrigin, toSource );
		VectorSubtract( beam.target, g_vOrigin, toTarget );
		const float nearestCameraDistance = std::min( VectorLength( toSource ), VectorLength( toTarget ) );
		const float precisionDistance = std::max( 16.0f,
			std::min( 512.0f, dx_beam_close_precision_distance.value ) );
		if( nearestCameraDistance < precisionDistance )
		{
			const float requestedStep = std::max( 1.0f,
				std::min( 32.0f, dx_beam_close_precision_step.value ) );
			const float adaptiveStep = std::max( 1.0f,
				std::min( requestedStep, std::max( 1.0f, beamWidth * 0.50f ) ) );
			const int precisionSegments = static_cast<int>( ceilf( length / adaptiveStep ) ) + 1;
			segments = std::max( segments,
				std::min( D3D_BEAM_MAX_SEGMENTS, precisionSegments ) );
		}
	}

	vec3_t perpendicular;
	CrossProduct( g_vForward, beamDir, perpendicular );
	if( R_BeamNormalizeVector( perpendicular ) < 0.001f )
		VectorCopy( g_vRight, perpendicular );

	vec3_t sideAxis;
	CrossProduct( beamDir, perpendicular, sideAxis );
	if( R_BeamNormalizeVector( sideAxis ) < 0.001f )
		VectorCopy( g_vUp, sideAxis );

	vec3_t centers[D3D_BEAM_MAX_SEGMENTS];
	const float coilScale = coilNoise ?
		std::max( 0.1f, std::min( 4.0f, dx_beam_sine_scale.value ) ) : 1.0f;
	const float rawAmplitude = beam.amplitude * std::max( 0.0f, noiseScale ) * coilScale;
	const float closeFade = dx_beam_sine_close_fade.value == 0.0f ? 1.0f :
		std::max( 0.0f, std::min( 1.0f,
			length / std::max( 24.0f, fabsf( rawAmplitude ) * 2.2f + beamWidth * 5.0f ) ) );
	const float anchorDistance = std::max( 0.0f, std::min( 96.0f,
		dx_beam_sine_anchor.value ) );
	for( int i = 0; i < segments; ++i )
	{
		const float fraction = static_cast<float>( i ) / static_cast<float>( segments - 1 );
		VectorMA( beam.source, fraction * length, beamDir, centers[i] );
		if( rawAmplitude != 0.0f )
		{

			float endpointFade = 1.0f;
			if( anchorDistance > 0.0f )
			{
				const float edgeDistance = std::min( fraction * length, ( 1.0f - fraction ) * length );
				const float t = std::max( 0.0f, std::min( 1.0f, edgeDistance / anchorDistance ) );
				endpointFade = t * t * ( 3.0f - 2.0f * t );
			}
			const float amplitude = rawAmplitude * endpointFade * closeFade;
			const float basePhase = beam.freq + phaseOffset + fraction *
				( FBitSet( beam.flags, FBEAM_SINENOISE ) ? 18.0f : 31.0f );
			const float waveA = FBitSet( beam.flags, FBEAM_SINENOISE ) || coilNoise
				? sinf( basePhase )
				: sinf( basePhase * 2.173f ) * cosf( basePhase * 0.617f );
			VectorMA( centers[i], waveA * amplitude, perpendicular, centers[i] );
			if( coilNoise )
			{
				const float waveB = cosf( basePhase * 1.371f + phaseOffset * 0.37f );
				VectorMA( centers[i], waveB * amplitude * 0.62f, sideAxis, centers[i] );
			}
		}
	}

	brush_vertex_t vertices[D3D_BEAM_MAX_SEGMENTS * 2];
	const D3DCOLOR color = R_ScaleColorAlpha( R_BeamColor( beam ), alphaScale );
	const float scroll = fmodf( beam.freq * beam.speed + phaseOffset * 0.03125f, 1.0f );
	const float texLength = length * 0.01f;

	for( int i = 0; i < segments; ++i )
	{
		const int previous = std::max( 0, i - 1 );
		const int next = std::min( segments - 1, i + 1 );
		vec3_t tangent, viewer, normal;
		VectorSubtract( centers[previous], centers[next], tangent );
		VectorSubtract( centers[i], g_vOrigin, viewer );
		CrossProduct( tangent, viewer, normal );
		if( R_BeamNormalizeVector( normal ) < 0.001f )
			VectorCopy( perpendicular, normal );

		float pointWidth = beamWidth;
		if( dx_beam_endpoint_taper.value != 0.0f && segments > 2 )
		{
			const float fraction = static_cast<float>( i ) / static_cast<float>( segments - 1 );
			const float edgeDistance = std::min( fraction * length, ( 1.0f - fraction ) * length );
			const float taperDistance = std::max( 1.0f,
				std::min( 48.0f, dx_beam_endpoint_taper_distance.value ) );
			const float t = std::max( 0.0f, std::min( 1.0f, edgeDistance / taperDistance ) );
			const float smooth = t * t * ( 3.0f - 2.0f * t );
			pointWidth *= 0.55f + 0.45f * smooth;
		}

		vec3_t left, right;
		VectorMA( centers[i], pointWidth, normal, left );
		VectorMA( centers[i], -pointWidth, normal, right );
		const float v = scroll + texLength *
			static_cast<float>( i ) / static_cast<float>( segments - 1 );

		brush_vertex_t &a = vertices[i * 2 + 0];
		brush_vertex_t &b = vertices[i * 2 + 1];
		a.x = left[0]; a.y = left[1]; a.z = left[2];
		a.diffuse = color; a.u = 0.0f; a.v = v;
		b.x = right[0]; b.y = right[1]; b.z = right[2];
		b.diffuse = color; b.u = 1.0f; b.v = v;
	}

	DX_Bind( 0, texture );
	R_DrawScratch( D3DPT_TRIANGLESTRIP,
		static_cast<UINT>( segments * 2 - 2 ),
		static_cast<UINT>( segments * 2 ), vertices, sizeof( brush_vertex_t ) );
}

static void R_RegisterEgonLight( const BEAM &beam )
{
	if( dx_egon_beam_light.value == 0.0f )
		return;

	const float now = gp_cl ? static_cast<float>( gp_cl->time ) : 0.0f;
	const float radius = std::max( 32.0f, std::min( 512.0f, dx_egon_beam_light_radius.value ) );
	const float spacing = std::max( 32.0f, std::min( 512.0f, dx_egon_beam_light_spacing.value ) );
	const int maxSamples = std::max( 2, std::min( 64, static_cast<int>( dx_egon_beam_light_max_samples.value ) ) );
	const int red = static_cast<int>( R_BeamClamp01( beam.r ) * 255.0f + 0.5f );
	const int green = static_cast<int>( R_BeamClamp01( beam.g ) * 255.0f + 0.5f );
	const int blue = static_cast<int>( R_BeamClamp01( beam.b ) * 255.0f + 0.5f );
	const int key = 0x45000000 ^ ( ( beam.startEntity & 0x0FFF ) << 8 ) ^
		( beam.modelIndex & 0xFF );
	R_AddBeamLight( beam.source, beam.target, radius, spacing,
		red, green, blue, now + 0.18f, key, maxSamples );
}

static void R_DrawBeamStrip( const BEAM &beam, model_t *model )
{
	int texture = 0;
	if( !R_BeamFrameTexture( beam, model, texture ) )
		return;

	const bool noisyBeam = R_BeamUsesSineNoise( beam );
	if( noisyBeam )
	{

		if( FBitSet( beam.flags, FBEAM_SINENOISE ) )
			R_RegisterEgonLight( beam );

		const float noiseScale = R_BeamNoiseScale( beam );
		R_DrawBeamStripLayer( beam, texture, noiseScale * 0.35f, 1.05f, 0.50f, 0.0f, false );
		if( FBitSet( beam.flags, FBEAM_SINENOISE ) )
		{
			R_DrawBeamStripLayer( beam, texture, noiseScale, 0.58f, 0.75f, 2.0943951f, true );
			R_DrawBeamStripLayer( beam, texture, noiseScale * 0.75f, 0.36f, 0.90f, 4.1887902f, true );
		}
		return;
	}

	R_DrawBeamStripLayer( beam, texture, 1.0f, 1.0f, 1.0f, 0.0f, false );
}

static void R_AppendBeamQuad( std::vector<brush_vertex_t> &vertices,
	const vec3_t start, const vec3_t end, float width,
	D3DCOLOR startColor, D3DCOLOR endColor, float v0, float v1 )
{
	vec3_t direction, normal;
	VectorSubtract( end, start, direction );
	if( DotProduct( direction, direction ) < 0.0001f )
		return;
	CrossProduct( direction, g_vForward, normal );
	if( DotProduct( normal, normal ) < 0.0001f )
		VectorCopy( g_vRight, normal );
	else
	{
		const float invLen = 1.0f / sqrtf( DotProduct( normal, normal ) );
		VectorScale( normal, invLen, normal );
	}
	VectorScale( normal, width, normal );

	brush_vertex_t a{}, b{}, c{}, d{};
	a.x = start[0] - normal[0]; a.y = start[1] - normal[1]; a.z = start[2] - normal[2];
	a.diffuse = startColor; a.u = 0.0f; a.v = v0;
	b.x = start[0] + normal[0]; b.y = start[1] + normal[1]; b.z = start[2] + normal[2];
	b.diffuse = startColor; b.u = 1.0f; b.v = v0;
	c.x = end[0] + normal[0]; c.y = end[1] + normal[1]; c.z = end[2] + normal[2];
	c.diffuse = endColor; c.u = 1.0f; c.v = v1;
	d.x = end[0] - normal[0]; d.y = end[1] - normal[1]; d.z = end[2] - normal[2];
	d.diffuse = endColor; d.u = 0.0f; d.v = v1;

	vertices.push_back( a ); vertices.push_back( b ); vertices.push_back( c );
	vertices.push_back( a ); vertices.push_back( c ); vertices.push_back( d );
}

static void R_DrawBeamFollow( BEAM &beam, model_t *model, float frameTime )
{
	if( dx_beam_trails.value == 0.0f || !gp_cl || !gEngfuncs.R_FreeDeadParticles )
		return;

	int texture = 0;
	if( !R_BeamFrameTexture( beam, model, texture ) )
		return;

	gEngfuncs.R_FreeDeadParticles( &beam.particles );

	particle_t *particles = beam.particles;
	particle_t *newParticle = nullptr;
	float distanceFromHead = 0.0f;
	const float spacing = std::max( 1.0f, std::min( 128.0f, dx_beam_trail_spacing.value ) );
	if( FBitSet( beam.flags, FBEAM_STARTENTITY ) )
	{
		if( particles )
		{
			vec3_t delta;
			VectorSubtract( particles->org, beam.source, delta );
			distanceFromHead = VectorLength( delta );
			if( distanceFromHead >= spacing && gEngfuncs.CL_AllocParticleFast )
				newParticle = gEngfuncs.CL_AllocParticleFast( );
		}
		else if( gEngfuncs.CL_AllocParticleFast )
		{
			newParticle = gEngfuncs.CL_AllocParticleFast( );
		}
	}

	if( newParticle )
	{
		VectorCopy( beam.source, newParticle->org );
		VectorClear( newParticle->vel );
		newParticle->die = gp_cl->time + std::max( 0.01f, beam.amplitude );
		newParticle->next = particles;
		beam.particles = newParticle;
		particles = newParticle;
	}

	if( !particles )
		return;

	const float life = std::max( 0.01f, beam.amplitude );
	const float width = std::max( 0.05f, beam.width *
		std::max( 0.05f, std::min( 16.0f, dx_beam_width_scale.value ) ) );
	std::vector<brush_vertex_t> vertices;
	vertices.reserve( 192 );

	vec3_t previous;
	VectorCopy( beam.source, previous );
	float previousAlpha = 1.0f;
	float texV = 0.0f;
	int trailCount = 0;
	for( particle_t *p = particles; p && trailCount++ < 8192; p = p->next )
	{
		const float alpha = R_BeamClamp01( static_cast<float>( p->die - gp_cl->time ) / life );
		if( alpha <= 0.0f )
			continue;
		vec3_t delta;
		VectorSubtract( p->org, previous, delta );
		if( DotProduct( delta, delta ) >= 0.25f )
		{
			R_AppendBeamQuad( vertices, previous, p->org, width,
				R_BeamColorScaled( beam, previousAlpha ),
				R_BeamColorScaled( beam, alpha ), texV, texV + 1.0f );
			texV += 1.0f;
		}
		VectorCopy( p->org, previous );
		previousAlpha = alpha;
	}

	if( !vertices.empty( ) )
	{
		DX_Bind( 0, texture );
		R_DrawScratch( D3DPT_TRIANGLELIST,
			static_cast<UINT>( vertices.size( ) / 3 ),
			static_cast<UINT>( vertices.size( ) ), vertices.data( ), sizeof( brush_vertex_t ) );
	}

	int driftCount = 0;
	for( particle_t *p = beam.particles; p && driftCount++ < 8192; p = p->next )
		VectorMA( p->org, frameTime, p->vel, p->org );
}

static void R_DrawBeam( BEAM &beam, model_t *model, float frameTime )
{
	if( beam.type == TE_BEAMFOLLOW )
	{
		R_DrawBeamFollow( beam, model, frameTime );
		return;
	}

	R_PrepareBeamNoise( beam, frameTime );

	if( R_DrawBeamGeometry( beam, model ) )
	{
		if( FBitSet( beam.flags, FBEAM_SINENOISE ) )
			R_RegisterEgonLight( beam );
		return;
	}

	R_DrawBeamStrip( beam, model );
}

static int R_BeamFxBlend( cl_entity_s *entity )
{
	if( !entity )
		return 0;

	int blend = 0;
	const float now = gp_cl ? static_cast<float>( gp_cl->time ) : 0.0f;
	const float offset = static_cast<float>( entity->index ) * 363.0f;
	int renderAmt = entity->curstate.renderamt;

	switch( entity->curstate.renderfx )
	{
	case kRenderFxPulseSlowWide:
		blend = renderAmt + static_cast<int>( 64.0f * sinf( now * 2.0f + offset ) );
		break;
	case kRenderFxPulseFastWide:
		blend = renderAmt + static_cast<int>( 64.0f * sinf( now * 8.0f + offset ) );
		break;
	case kRenderFxPulseSlow:
		blend = renderAmt + static_cast<int>( 16.0f * sinf( now * 2.0f + offset ) );
		break;
	case kRenderFxPulseFast:
		blend = renderAmt + static_cast<int>( 16.0f * sinf( now * 8.0f + offset ) );
		break;

	case kRenderFxFadeSlow:
		if( !FBitSet( g_currentViewFlags, RF_DRAW_CUBEMAP ) )
		{
			renderAmt = renderAmt > 0 ? renderAmt - 1 : 0;
			entity->curstate.renderamt = static_cast<byte>( renderAmt );
		}
		blend = renderAmt;
		break;
	case kRenderFxFadeFast:
		if( !FBitSet( g_currentViewFlags, RF_DRAW_CUBEMAP ) )
		{
			renderAmt = renderAmt > 3 ? renderAmt - 4 : 0;
			entity->curstate.renderamt = static_cast<byte>( renderAmt );
		}
		blend = renderAmt;
		break;
	case kRenderFxSolidSlow:
		if( !FBitSet( g_currentViewFlags, RF_DRAW_CUBEMAP ) )
		{
			renderAmt = renderAmt < 255 ? renderAmt + 1 : 255;
			entity->curstate.renderamt = static_cast<byte>( renderAmt );
		}
		blend = renderAmt;
		break;
	case kRenderFxSolidFast:
		if( !FBitSet( g_currentViewFlags, RF_DRAW_CUBEMAP ) )
		{
			renderAmt = renderAmt < 252 ? renderAmt + 4 : 255;
			entity->curstate.renderamt = static_cast<byte>( renderAmt );
		}
		blend = renderAmt;
		break;

	case kRenderFxStrobeSlow:
		blend = static_cast<int>( 20.0f * sinf( now * 4.0f + offset ) );
		blend = blend < 0 ? 0 : renderAmt;
		break;
	case kRenderFxStrobeFast:
		blend = static_cast<int>( 20.0f * sinf( now * 16.0f + offset ) );
		blend = blend < 0 ? 0 : renderAmt;
		break;
	case kRenderFxStrobeFaster:
		blend = static_cast<int>( 20.0f * sinf( now * 36.0f + offset ) );
		blend = blend < 0 ? 0 : renderAmt;
		break;
	case kRenderFxFlickerSlow:
		blend = static_cast<int>( 20.0f *
			( sinf( now * 2.0f ) + sinf( now * 17.0f + offset ) ) );
		blend = blend < 0 ? 0 : renderAmt;
		break;
	case kRenderFxFlickerFast:
		blend = static_cast<int>( 20.0f *
			( sinf( now * 16.0f ) + sinf( now * 23.0f + offset ) ) );
		blend = blend < 0 ? 0 : renderAmt;
		break;

	case kRenderFxHologram:
	case kRenderFxDistort:
	{
		vec3_t toEntity;
		VectorSubtract( entity->origin, g_vOrigin, toEntity );
		float distance = DotProduct( toEntity, g_vForward );
		if( entity->curstate.renderfx == kRenderFxDistort )
			distance = 1.0f;

		if( distance <= 0.0f )
		{
			blend = 0;
		}
		else
		{
			renderAmt = 180;
			entity->curstate.renderamt = static_cast<byte>( renderAmt );
			if( distance <= 100.0f )
				blend = renderAmt;
			else
				blend = static_cast<int>(
					( 1.0f - ( distance - 100.0f ) * ( 1.0f / 400.0f ) ) *
					static_cast<float>( renderAmt ) );

			if( gEngfuncs.COM_RandomFloat )
				blend += static_cast<int>(
					gEngfuncs.COM_RandomFloat( -32.0f, 31.999f ) );
		}
		break;
	}

	default:
		blend = renderAmt;
		break;
	}

	return std::max( 0, std::min( 255, blend ) );
}

static bool R_BuildBeamEntity( const cl_entity_s *entity, BEAM &outBeam, float frameTime )
{
	if( !entity || !entity->model || entity->model->type != mod_sprite )
		return false;

	cl_entity_s *mutableEntity = const_cast<cl_entity_s *>( entity );
	const int fxBlend = R_BeamFxBlend( mutableEntity );
	if( fxBlend <= 0 )
		return false;

	memset( &outBeam, 0, sizeof( outBeam ) );
	outBeam.type = TE_BEAMPOINTS;
	outBeam.modelIndex = entity->curstate.modelindex;
	outBeam.frameCount = std::max( 1, entity->model->numframes );
	outBeam.frame = entity->curstate.frame;
	outBeam.frameRate = entity->curstate.framerate;
	outBeam.width = entity->curstate.scale;
	outBeam.amplitude = entity->curstate.body / 100.0f;
	outBeam.speed = entity->curstate.animtime;

	outBeam.freq = outBeam.speed * ( gp_cl ? gp_cl->time : 0.0f );
	const int form = entity->curstate.rendermode & 0x0F;
	outBeam.brightness = R_BeamClamp01(
		static_cast<float>( fxBlend ) * ( 1.0f / 255.0f ) );
	outBeam.r = entity->curstate.rendercolor.r / 255.0f;
	outBeam.g = entity->curstate.rendercolor.g / 255.0f;
	outBeam.b = entity->curstate.rendercolor.b / 255.0f;
	VectorCopy( entity->origin, outBeam.source );
	VectorCopy( entity->curstate.angles, outBeam.target );

	if( form == DX_BEAM_ENTPOINT )
	{
		if( entity->curstate.sequence )
		{
			SetBits( outBeam.flags, FBEAM_STARTENTITY );
			outBeam.startEntity = entity->curstate.sequence;
		}
		if( entity->curstate.skin )
		{
			SetBits( outBeam.flags, FBEAM_ENDENTITY );
			outBeam.endEntity = entity->curstate.skin;
		}
	}
	else if( form == DX_BEAM_ENTS )
	{
		SetBits( outBeam.flags, FBEAM_STARTENTITY | FBEAM_ENDENTITY );
		outBeam.startEntity = entity->curstate.sequence;
		outBeam.endEntity = entity->curstate.skin;
	}
	else if( form == DX_BEAM_HOSE )
	{
		outBeam.type = TE_BEAMHOSE;
	}

	const int flags = entity->curstate.rendermode & 0xF0;
	if( flags & 0x10 ) SetBits( outBeam.flags, FBEAM_SINENOISE );
	if( flags & 0x20 ) SetBits( outBeam.flags, FBEAM_SOLID );
	if( flags & 0x40 ) SetBits( outBeam.flags, FBEAM_SHADEIN );
	if( flags & 0x80 ) SetBits( outBeam.flags, FBEAM_SHADEOUT );

	return R_UpdateTempBeam( &outBeam, frameTime );
}

void R_DrawBeamEntities( bool transparent )
{
	if( !device || g_beamEntityCount <= 0 || dx_beams.value == 0.0f )
		return;

	beam_render_state_t savedState{};
	R_BeginBeamPass( transparent, savedState );
	const float frameTime =
		std::max( 0.0f, std::min( 0.25f, g_frametime ) );
	for( int i = 0; i < g_beamEntityCount; ++i )
	{
		cl_entity_s *entity = g_beamEntities[i];
		BEAM beam;
		if( !R_BuildBeamEntity( entity, beam, frameTime ) )
			continue;
		const bool solid = FBitSet( beam.flags, FBEAM_SOLID );
		if( solid != !transparent )
			continue;
		R_DrawBeam( beam, entity->model, frameTime );
	}
	R_RestoreBeamState( savedState );
}

void CL_DrawBeams( int fTrans, BEAM *beams )
{
	if( !device || !beams || dx_beams.value == 0.0f )
		return;

	const bool transparent = fTrans != 0;
	beam_render_state_t savedState{};
	R_BeginBeamPass( transparent, savedState );

	const float frameTime = std::max( 0.0f, std::min( 0.25f, g_frametime ) );

	int beamCount = 0;
	for( BEAM *beam = beams; beam && beamCount++ < 8192; beam = beam->next )
	{
		const bool solid = FBitSet( beam->flags, FBEAM_SOLID );
		if( solid != !transparent )
			continue;
		if( !gp_cl || beam->modelIndex <= 0 || beam->modelIndex > gp_cl->nummodels ||
			beam->modelIndex >= MAX_MODELS )
			continue;
		model_t *model = gp_cl->models[beam->modelIndex];
		if( !model || !R_UpdateTempBeam( beam, frameTime ) )
			continue;
		R_DrawBeam( *beam, model, frameTime );
	}

	R_RestoreBeamState( savedState );

	if( transparent && dx_beam_particles.value != 0.0f )
	{
		int particleBeamCount = 0;
		for( BEAM *beam = beams; beam && particleBeamCount++ < 8192; beam = beam->next )
		{
			if( beam->type == TE_BEAMFOLLOW )
				continue;
			if( beam->particles )
				R_DrawParticles( beam->particles,
					std::max( 1.0f, beam->width ), true, false );
		}
	}
}

static void R_SetSpritePipelineBaseline( void )
{
	R_SetLightmapStageEnabled( false );
	DX_SetRenderStateCached( D3DRS_LIGHTING, FALSE );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXCOORDINDEX, 0 );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
	device->SetFVF( BRUSH_FVF );
}

static bool R_SpriteHasLightmap( const cl_entity_s *ent, const msprite_t *sprite )
{
	if( !ent || !sprite )
		return false;
	if( sprite->texFormat != SPR_ALPHTEST )
		return false;
	if( ( ent->curstate.effects & EF_FULLBRIGHT ) != 0 )
		return false;
	if( ent->curstate.renderamt <= 127 )
		return false;

	switch( ent->curstate.rendermode )
	{
	case kRenderNormal:
	case kRenderTransAlpha:
	case kRenderTransTexture:
		return true;
	default:
		return false;
	}
}

static D3DCOLOR R_SpriteLightColorAtPoint( const vec3_t origin )
{
	if( !origin || !g_worldmodel || !g_worldmodel->nodes || !g_worldmodel->lightdata )
		return D3DCOLOR_ARGB( 255, 255, 255, 255 );

	vec3_t end = { origin[0], origin[1], origin[2] - 2048.0f };
	color24 light;
	if( !R_StudioTraceLight( origin, end, light, nullptr, nullptr,
		false, true ) )
	{
		return D3DCOLOR_ARGB( 255, 0, 0, 0 );
	}

	return D3DCOLOR_ARGB( 255, light.r, light.g, light.b );
}

static D3DCOLOR R_SpriteVertexColor( const cl_entity_s *ent )
{
	int r = 255, g = 255, b = 255;
	if( ent )
	{
		r = ent->curstate.rendercolor.r;
		g = ent->curstate.rendercolor.g;
		b = ent->curstate.rendercolor.b;
		if( r == 0 && g == 0 && b == 0 ) r = g = b = 255;
	}
	return D3DCOLOR_RGBA( r, g, b, 255 );
}

static void R_FillSpriteQuadVertices( const mspriteframe_t *frame,
	const vec3_t origin, const vec3_t right, const vec3_t up,
	float scale, D3DCOLOR color, brush_vertex_t verts[4] )
{
	vec3_t p;

	VectorMA( origin, frame->down * scale, up, p );
	VectorMA( p, frame->left * scale, right, p );
	verts[0].x = p[0]; verts[0].y = p[1]; verts[0].z = p[2];
	verts[0].diffuse = color; verts[0].u = 0.0f; verts[0].v = 1.0f;

	VectorMA( origin, frame->up * scale, up, p );
	VectorMA( p, frame->left * scale, right, p );
	verts[1].x = p[0]; verts[1].y = p[1]; verts[1].z = p[2];
	verts[1].diffuse = color; verts[1].u = 0.0f; verts[1].v = 0.0f;

	VectorMA( origin, frame->up * scale, up, p );
	VectorMA( p, frame->right * scale, right, p );
	verts[2].x = p[0]; verts[2].y = p[1]; verts[2].z = p[2];
	verts[2].diffuse = color; verts[2].u = 1.0f; verts[2].v = 0.0f;

	VectorMA( origin, frame->down * scale, up, p );
	VectorMA( p, frame->right * scale, right, p );
	verts[3].x = p[0]; verts[3].y = p[1]; verts[3].z = p[2];
	verts[3].diffuse = color; verts[3].u = 1.0f; verts[3].v = 1.0f;
}

static void R_DrawSpriteQuad( const mspriteframe_t *frame, const vec3_t origin,
	const vec3_t right, const vec3_t up, float scale, D3DCOLOR color )
{
	if( !frame ) return;

	brush_vertex_t verts[4];
	R_FillSpriteQuadVertices( frame, origin, right, up, scale, color, verts );
	R_DrawScratchPrimitive( D3DPT_TRIANGLEFAN, 2, 4,
		verts, sizeof( brush_vertex_t ) );
}

static void R_BuildSpriteAxes( const cl_entity_s *ent, const msprite_t *sprite,
	vec3_t origin, vec3_t right, vec3_t up )
{
	int type = sprite->type;
	if( ent->angles[ROLL] != 0.0f && type == SPR_FWD_PARALLEL )
		type = SPR_FWD_PARALLEL_ORIENTED;

	switch( type )
	{
	case SPR_ORIENTED:
	{
		vec3_t forward;
		AngleVectors( ent->angles, forward, right, up );

		VectorMA( origin, -0.01f, forward, origin );
		break;
	}
	case SPR_FACING_UPRIGHT:
		VectorSet( right, origin[1] - g_vOrigin[1],
			-( origin[0] - g_vOrigin[0] ), 0.0f );
		VectorSet( up, 0.0f, 0.0f, 1.0f );
		VectorNormalize( right );
		break;
	case SPR_FWD_PARALLEL_UPRIGHT:
		VectorSet( up, 0.0f, 0.0f, 1.0f );
		VectorSet( right, g_vForward[1], -g_vForward[0], 0.0f );
		VectorNormalize( right );
		break;
	case SPR_FWD_PARALLEL_ORIENTED:
	{
		const float angle = DEG2RAD( ent->angles[ROLL] );
		const float sr = sinf( angle ), cr = cosf( angle );
		for( int i = 0; i < 3; ++i )
		{
			right[i] = g_vRight[i] * cr + g_vUp[i] * sr;
			up[i] = g_vRight[i] * -sr + g_vUp[i] * cr;
		}
		break;
	}
	case SPR_FWD_PARALLEL:
	default:
		VectorCopy( g_vRight, right );
		VectorCopy( g_vUp, up );
		break;
	}
}

void R_ReleaseGlowOcclusionQueries( )
{
	R_ForEachEntityCache( []( entity_cache_t &entityCache )
	{
		glow_occlusion_query_t &glow = entityCache.glow;
		if( glow.query )
			glow.query->Release( );
		glow = glow_occlusion_query_t{};
	} );
}

static bool R_EnsureGlowOcclusionQuery( glow_occlusion_query_t &cache )
{
	if( cache.query )
		return true;
	if( !device )
		return false;

	HRESULT result = device->CreateQuery( D3DQUERYTYPE_OCCLUSION, &cache.query );
	if( FAILED( result ) || !cache.query )
	{
		cache.query = nullptr;
		cache.issued = false;
		cache.hasResult = false;
		cache.visible = true;
		return false;
	}

	cache.issued = false;
	cache.hasResult = false;
	cache.visible = true;
	return true;
}

static void R_DrawGlowOcclusionMarker( const vec3_t origin, float size )
{
	if( !device || !origin )
		return;

	brush_vertex_t verts[4];
	vec3_t p;
	const D3DCOLOR white = D3DCOLOR_ARGB( 255, 255, 255, 255 );
	const float half = std::max( 1.0f, size ) * 0.5f;

	VectorMA( origin, -half, g_vUp, p );
	VectorMA( p, -half, g_vRight, p );
	verts[0].x = p[0]; verts[0].y = p[1]; verts[0].z = p[2];
	verts[0].diffuse = white; verts[0].u = 0.0f; verts[0].v = 1.0f;

	VectorMA( origin, half, g_vUp, p );
	VectorMA( p, -half, g_vRight, p );
	verts[1].x = p[0]; verts[1].y = p[1]; verts[1].z = p[2];
	verts[1].diffuse = white; verts[1].u = 0.0f; verts[1].v = 0.0f;

	VectorMA( origin, half, g_vUp, p );
	VectorMA( p, half, g_vRight, p );
	verts[2].x = p[0]; verts[2].y = p[1]; verts[2].z = p[2];
	verts[2].diffuse = white; verts[2].u = 1.0f; verts[2].v = 0.0f;

	VectorMA( origin, -half, g_vUp, p );
	VectorMA( p, half, g_vRight, p );
	verts[3].x = p[0]; verts[3].y = p[1]; verts[3].z = p[2];
	verts[3].diffuse = white; verts[3].u = 1.0f; verts[3].v = 1.0f;

	DX_BindRawTexture( 0, nullptr );
	device->SetFVF( BRUSH_FVF );
	R_DrawScratchPrimitive( D3DPT_TRIANGLEFAN, 2, 4,
		verts, sizeof( brush_vertex_t ) );
}

static bool R_GlowQueryMayIssue( const glow_occlusion_query_t &cache )
{
	if( !gp_cl )
		return true;

	const float now = static_cast<float>( gp_cl->time );
	if( fabsf( now - g_glowQueryBudgetTime ) > 0.0001f )
	{
		g_glowQueryBudgetTime = now;
		g_glowQueriesIssuedThisFrame = 0;
	}

	int budget = static_cast<int>( dx_glow_query_budget.value );
	budget = std::max( 0, std::min( 1024, budget ) );
	if( budget <= 0 || g_glowQueriesIssuedThisFrame >= budget )
		return false;

	const float hz = std::max( 0.0f, std::min( 240.0f, dx_glow_query_hz.value ) );
	if( hz <= 0.0f )
		return true;

	if( !cache.hasResult )
		return true;

	return ( now - cache.lastIssueTime ) >= ( 1.0f / hz );
}

static bool R_GlowQueryVisible( const cl_entity_s *ent,
	const vec3_t origin, bool &hasHardwareDecision )
{
	hasHardwareDecision = false;
	if( !ent || !device || ( dx_glow_occlusion.value < 1.0f || dx_glow_occlusion.value >= 2.0f ) )
		return true;

	vec3_t toGlow;
	VectorSubtract( origin, g_vOrigin, toGlow );
	if( DotProduct( toGlow, g_vForward ) <= 1.0f )
	{
		hasHardwareDecision = true;
		return false;
	}

	glow_occlusion_query_t &cache = R_GetEntityCache( ent ).glow;
	if( !R_EnsureGlowOcclusionQuery( cache ) )
		return true;

	if( cache.issued )
	{
		DWORD visiblePixels = 0;
		const HRESULT result = cache.query->GetData( &visiblePixels,
			sizeof( visiblePixels ), 0 );
		if( result == S_OK )
		{
			cache.visible = visiblePixels != 0;
			cache.hasResult = true;
			cache.issued = false;
		}
		else if( result != S_FALSE )
		{
			cache.query->Release( );
			cache.query = nullptr;
			cache.issued = false;
			cache.hasResult = false;
			cache.visible = true;
			return true;
		}
	}

	if( !cache.issued && cache.query && R_GlowQueryMayIssue( cache ) )
	{
		const DWORD colorMask = D3DCOLORWRITEENABLE_RED |
			D3DCOLORWRITEENABLE_GREEN |
			D3DCOLORWRITEENABLE_BLUE |
			D3DCOLORWRITEENABLE_ALPHA;
		const float querySize = std::max( 1.0f,
			std::min( 64.0f, dx_glow_query_size.value ) );

		DX_SetRenderStateCached( D3DRS_COLORWRITEENABLE, 0 );
		DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
		DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
		DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_NONE );
		R_SetTextureStage( D3DTOP_SELECTARG2, D3DTA_TEXTURE, D3DTA_DIFFUSE,
			D3DTOP_SELECTARG2, D3DTA_TEXTURE, D3DTA_DIFFUSE );

		if( SUCCEEDED( cache.query->Issue( D3DISSUE_BEGIN ) ) )
		{
			R_DrawGlowOcclusionMarker( origin, querySize );
			if( SUCCEEDED( cache.query->Issue( D3DISSUE_END ) ) )
			{
				cache.issued = true;
				cache.lastIssueTime = gp_cl ? static_cast<float>( gp_cl->time ) : 0.0f;
				g_glowQueriesIssuedThisFrame++;
			}
		}

		DX_SetRenderStateCached( D3DRS_COLORWRITEENABLE, colorMask );
	}

	hasHardwareDecision = cache.hasResult;
	return cache.hasResult ? cache.visible : true;
}

static bool R_SurfaceTextureBlocksGlow( const msurface_t *surface )
{
	if( !surface || !surface->texinfo || !surface->texinfo->texture )
		return false;

	if( surface->flags & ( SURF_DRAWSKY | SURF_DRAWTURB ) )
		return false;

	return !R_BrushTextureNeedsAlphaTest( surface->texinfo->texture );
}

static bool R_PointInSurfacePolygon2D( const model_t *mod,
	const msurface_t *surface, const vec3_t point )
{
	if( !mod || !surface || surface->numedges < 3 )
		return false;

	vec3_t normal;
	if( surface->plane )
		VectorCopy( surface->plane->normal, normal );
	else
		return false;

	const float ax = fabsf( normal[0] );
	const float ay = fabsf( normal[1] );
	const float az = fabsf( normal[2] );
	const int dropAxis = ( ax > ay && ax > az ) ? 0 : ( ay > az ? 1 : 2 );
	const int axisU = ( dropAxis + 1 ) % 3;
	const int axisV = ( dropAxis + 2 ) % 3;
	const float px = point[axisU];
	const float py = point[axisV];

	bool inside = false;
	const mvertex_t *prev = R_GetBrushSurfaceVertex( mod, surface,
		surface->numedges - 1 );
	if( !prev )
		return false;

	for( int i = 0; i < surface->numedges; ++i )
	{
		const mvertex_t *cur = R_GetBrushSurfaceVertex( mod, surface, i );
		if( !cur )
			return false;

		const float x0 = prev->position[axisU];
		const float y0 = prev->position[axisV];
		const float x1 = cur->position[axisU];
		const float y1 = cur->position[axisV];
		const bool crosses = ( ( y0 > py ) != ( y1 > py ) );
		if( crosses )
		{
			const float atX = x0 + ( py - y0 ) * ( x1 - x0 ) /
				( y1 - y0 );
			if( px < atX )
				inside = !inside;
		}

		prev = cur;
	}

	return inside;
}

static bool R_GlowSpriteOccludedByWorld( const cl_entity_s *ent )
{
	if( !ent || !g_worldmodel || g_worldmodel->type != mod_brush ||
		dx_glow_occlusion.value < 2.0f )
	{
		return false;
	}

	vec3_t start, end, dir;
	VectorCopy( g_vOrigin, start );
	VectorCopy( ent->origin, end );
	VectorSubtract( end, start, dir );
	const float lengthSq = DotProduct( dir, dir );
	if( lengthSq <= 1.0f )
		return false;

	const float startBias = 4.0f / sqrtf( lengthSq );
	const float endBias = 4.0f / sqrtf( lengthSq );

	for( int i = 0; i < g_worldmodel->numsurfaces; ++i )
	{
		const msurface_t *surface = &g_worldmodel->surfaces[i];
		if( !R_SurfaceTextureBlocksGlow( surface ) || !surface->plane )
			continue;

		const float *normal = surface->plane->normal;
		const float denom = DotProduct( normal, dir );
		if( fabsf( denom ) < 0.0001f )
			continue;

		const float t = ( surface->plane->dist - DotProduct( normal, start ) ) / denom;
		if( t <= startBias || t >= 1.0f - endBias )
			continue;

		vec3_t hit;
		VectorMA( start, t, dir, hit );
		if( R_PointInSurfacePolygon2D( g_worldmodel, surface, hit ) )
			return true;
	}

	return false;
}

static D3DCOLOR R_SpriteTextureFactor( const cl_entity_s *ent,
	int renderMode, float alphaScale = 1.0f )
{

	int a = 255;
	if( ent && renderMode != kRenderNormal )
	{
		const float scaledAlpha = static_cast<float>( ent->curstate.renderamt ) *
			std::max( 0.0f, std::min( 1.0f, alphaScale ) );
		a = std::max( 0, std::min( 255,
			static_cast<int>( scaledAlpha + 0.5f ) ) );
	}
	return D3DCOLOR_ARGB( a, 255, 255, 255 );
}

static bool R_SpriteAllowLerping( const cl_entity_s *ent, const msprite_t *sprite )
{
	if( !ent || !sprite )
		return false;
	if( sprite->numframes <= 1 || sprite->texFormat != SPR_ADDITIVE )
		return false;
	if( ent->curstate.rendermode == kRenderNormal ||
		ent->curstate.rendermode == kRenderTransAlpha )
	{
		return false;
	}
	return true;
}

static float R_GetSpriteFrameLerp( cl_entity_s *ent, model_t *mod,
	msprite_t *sprite, mspriteframe_t *&oldFrame, mspriteframe_t *&currentFrame )
{
	oldFrame = currentFrame = nullptr;
	if( !ent || !mod || !sprite || sprite->numframes <= 0 ||
		!gEngfuncs.R_GetSpriteFrame )
	{
		return 1.0f;
	}

	int frame = static_cast<int>( ent->curstate.frame );
	frame = std::max( 0, std::min( sprite->numframes - 1, frame ) );
	const bool doInterp = ( ent->curstate.effects & EF_NOINTERP ) == 0;
	float lerp = 1.0f;
	const float now = gp_cl ? gp_cl->time : 0.0f;
	const int frameType = sprite->frames[frame].type;

	if( frameType == FRAME_SINGLE || frameType == FRAME_ANGLED )
	{
		const bool previousValid =
			ent->latched.prevblending[0] < sprite->numframes &&
			sprite->frames[ent->latched.prevblending[0]].type == frameType;

		if( doInterp )
		{
			if( !previousValid )
			{
				ent->latched.prevblending[0] =
					ent->latched.prevblending[1] = static_cast<byte>( frame );
				ent->latched.sequencetime = now;
				lerp = 1.0f;
			}

			if( ent->latched.sequencetime < now )
			{
				if( frame != ent->latched.prevblending[1] )
				{
					ent->latched.prevblending[0] = ent->latched.prevblending[1];
					ent->latched.prevblending[1] = static_cast<byte>( frame );
					ent->latched.sequencetime = now;
					lerp = 0.0f;
				}
				else
				{
					const float rate = frameType == FRAME_ANGLED
						? std::max( 0.0f, ent->curstate.framerate ) : 11.0f;
					lerp = ( now - ent->latched.sequencetime ) * rate;
				}
			}
			else
			{
				ent->latched.prevblending[0] =
					ent->latched.prevblending[1] = static_cast<byte>( frame );
				ent->latched.sequencetime = now;
				lerp = 0.0f;
			}
		}
		else
		{
			ent->latched.prevblending[0] =
				ent->latched.prevblending[1] = static_cast<byte>( frame );
			lerp = 1.0f;
		}

		if( ent->latched.prevblending[0] >= sprite->numframes )
		{
			ent->latched.prevblending[0] =
				ent->latched.prevblending[1] = static_cast<byte>( frame );
			ent->latched.sequencetime = now;
			lerp = 0.0f;
		}

		oldFrame = static_cast<mspriteframe_t *>(
			gEngfuncs.R_GetSpriteFrame( mod, ent->latched.prevblending[0],
				ent->angles[YAW] ) );
		currentFrame = static_cast<mspriteframe_t *>(
			gEngfuncs.R_GetSpriteFrame( mod, frame, ent->angles[YAW] ) );
	}
	else if( frameType == FRAME_GROUP )
	{
		mspritegroup_t *group = reinterpret_cast<mspritegroup_t *>(
			sprite->frames[frame].frameptr );
		if( !group || group->numframes <= 0 || !group->intervals )
			return 1.0f;

		if( group->numframes == 1 )
		{
			oldFrame = currentFrame = group->frames[0];
			return 1.0f;
		}

		const float fullInterval = group->intervals[group->numframes - 1];
		if( fullInterval <= 0.0f )
		{
			oldFrame = currentFrame = group->frames[0];
			return 1.0f;
		}

		const float targetTime = now - floorf( now / fullInterval ) * fullInterval;
		float previousTime = 0.0f;
		float interval = group->intervals[1] - group->intervals[0];
		int currentIndex = 0;
		int previousIndex = group->numframes - 1;
		for( currentIndex = 0, previousIndex = group->numframes - 1;
			currentIndex < group->numframes - 1; ++currentIndex )
		{
			if( group->intervals[currentIndex] > targetTime )
				break;
			previousIndex = currentIndex;
			interval = group->intervals[currentIndex] - previousTime;
			previousTime = group->intervals[currentIndex];
		}

		if( doInterp && interval > 0.0f )
			lerp = ( targetTime - previousTime ) / interval;
		else
			previousIndex = currentIndex;

		oldFrame = group->frames[previousIndex];
		currentFrame = group->frames[currentIndex];
	}
	else
	{
		oldFrame = currentFrame = static_cast<mspriteframe_t *>(
			gEngfuncs.R_GetSpriteFrame( mod, frame, ent->angles[YAW] ) );
		lerp = 1.0f;
	}

	lerp = std::max( 0.0f, std::min( 1.0f, lerp ) );
	if( !oldFrame ) oldFrame = currentFrame;
	if( !currentFrame ) currentFrame = oldFrame;
	return lerp;
}

static void R_GetSpriteFramesForDraw( cl_entity_s *ent, model_t *mod,
	msprite_t *sprite, mspriteframe_t *&oldFrame,
	mspriteframe_t *&currentFrame, float &lerp )
{
	oldFrame = currentFrame = nullptr;
	lerp = 1.0f;
	if( R_SpriteAllowLerping( ent, sprite ) )
	{
		lerp = R_GetSpriteFrameLerp(
			ent, mod, sprite, oldFrame, currentFrame );
	}
	else if( gEngfuncs.R_GetSpriteFrame )
	{
		currentFrame = static_cast<mspriteframe_t *>(
			gEngfuncs.R_GetSpriteFrame( mod, ent->curstate.frame,
				ent->angles[YAW] ) );
		oldFrame = currentFrame;
	}
}

static bool R_SpriteBatchKeyEquals( const sprite_batch_key_t &a,
	const sprite_batch_key_t &b )
{
	return a.texture == b.texture &&
		a.renderMode == b.renderMode &&
		a.textureFactor == b.textureFactor &&
		a.disableDepth == b.disableDepth;
}

static void R_BeginSpriteBatchRenderMode( const sprite_batch_key_t &key )
{
	R_SetSpritePipelineBaseline( );
	DX_AllowFog( ( key.renderMode == kRenderGlow ||
		key.renderMode == kRenderTransAdd ) ? false : true );

	DX_SetRenderStateCached( D3DRS_BLENDOP, D3DBLENDOP_ADD );
	DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHAFUNC, D3DCMP_GREATER );
	DX_SetRenderStateCached( D3DRS_ALPHAREF, 0 );
	DX_SetRenderStateCached( D3DRS_TEXTUREFACTOR, key.textureFactor );

	switch( key.renderMode )
	{
	default:
	case kRenderNormal:
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
		R_SetTextureStage( D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE,
			D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE );
		break;

	case kRenderTransColor:
	case kRenderTransTexture:
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
		R_SetTextureStage( D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE,
			D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_TFACTOR );
		break;

	case kRenderTransAlpha:
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
		R_SetTextureStage( D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE,
			D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_TFACTOR );
		break;

	case kRenderGlow:
	case kRenderTransAdd:
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_ONE );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
		R_SetTextureStage( D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE,
			D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_TFACTOR );
		break;
	}

	if( key.disableDepth )
	{
		DX_SetRenderStateCached( D3DRS_ZENABLE, FALSE );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
	}
	else if( key.renderMode == kRenderGlow )
	{

		DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
	}
}

void R_FlushSprites( void )
{
	if( !g_spriteBatchActive || g_spriteBatchVertices.empty( ) || !device )
	{
		g_spriteBatchActive = false;
		g_spriteBatchVertices.clear( );
		return;
	}

	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_NONE );
	R_BeginSpriteBatchRenderMode( g_spriteBatchKey );
	DX_Bind( 0, g_spriteBatchKey.texture );

	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ALPHAFUNC, D3DCMP_GREATER );
	DX_SetRenderStateCached( D3DRS_ALPHAREF, 0 );

	DX_ProfileEvent( DX_PROFILE_EVENT_SPRITE_BATCH_DRAWN, 1 );
	R_DrawScratchPrimitive( D3DPT_TRIANGLELIST,
		static_cast<UINT>( g_spriteBatchVertices.size( ) / 3 ),
		static_cast<UINT>( g_spriteBatchVertices.size( ) ),
		g_spriteBatchVertices.data( ), sizeof( brush_vertex_t ) );

	g_spriteBatchVertices.clear( );
	g_spriteBatchActive = false;
	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_CCW );
	DX_SetRenderStateCached( D3DRS_TEXTUREFACTOR, 0xFFFFFFFF );
	R_SetOpaqueEntityBaseline( );
}

static void R_QueueSpriteFrame( cl_entity_s *ent, const mspriteframe_t *frame,
	const vec3_t origin, const vec3_t right, const vec3_t up, float scale,
	bool disableDepth, float alphaScale )
{
	if( !ent || !frame || frame->gl_texturenum <= 0 || alphaScale <= 0.0f )
		return;

	sprite_batch_key_t key{};
	key.texture = frame->gl_texturenum;
	key.renderMode = ent->curstate.rendermode;
	key.textureFactor = R_SpriteTextureFactor( ent, key.renderMode, alphaScale );
	key.disableDepth = disableDepth;

	if( g_spriteBatchActive && !R_SpriteBatchKeyEquals( g_spriteBatchKey, key ) )
		R_FlushSprites( );
	if( !g_spriteBatchActive )
	{
		g_spriteBatchKey = key;
		g_spriteBatchActive = true;
	}

	brush_vertex_t quad[4];
	R_FillSpriteQuadVertices( frame, origin, right, up, scale,
		R_SpriteVertexColor( ent ), quad );
	g_spriteBatchVertices.reserve( g_spriteBatchVertices.size( ) + 6 );
	g_spriteBatchVertices.push_back( quad[0] );
	g_spriteBatchVertices.push_back( quad[1] );
	g_spriteBatchVertices.push_back( quad[2] );
	g_spriteBatchVertices.push_back( quad[0] );
	g_spriteBatchVertices.push_back( quad[2] );
	g_spriteBatchVertices.push_back( quad[3] );
	DX_ProfileEvent( DX_PROFILE_EVENT_SPRITE_QUEUED, 1 );
}

bool R_QueueSprite( cl_entity_s *ent, model_t *mod )
{
	if( dx_remix_perf.value == 0.0f )
		return false;
	if( !ent || !mod || !mod->cache.data )
		return true;

	msprite_t *sprite = (msprite_t *)mod->cache.data;
	if( !sprite )
		return true;

	if( R_SpriteHasLightmap( ent, sprite ) )
		return false;

	mspriteframe_t *oldFrame = nullptr;
	mspriteframe_t *frame = nullptr;
	float frameLerp = 1.0f;
	R_GetSpriteFramesForDraw( ent, mod, sprite, oldFrame, frame, frameLerp );
	if( !frame || frame->gl_texturenum <= 0 )
		return true;

	const bool glowSprite = ent->curstate.rendermode == kRenderGlow;
	const bool useGlowCpuTrace = glowSprite && dx_glow_occlusion.value >= 2.0f;
	if( useGlowCpuTrace && R_GlowSpriteOccludedByWorld( ent ) )
		return true;

	bool glowHasAsyncDecision = false;
	bool glowAsyncVisible = true;
	if( glowSprite &&dx_glow_occlusion.value >= 1.0f && dx_glow_occlusion.value < 2.0f )
	{
		glowAsyncVisible = R_GlowQueryVisible( ent,
			ent->origin, glowHasAsyncDecision );
		if( glowHasAsyncDecision && !glowAsyncVisible )
			return true;
	}

	vec3_t origin, right, up;
	VectorCopy( ent->origin, origin );
	R_BuildSpriteAxes( ent, sprite, origin, right, up );

	const float scale = ent->curstate.scale > 0.0f
		? ent->curstate.scale : 1.0f;

	const bool disableDepth =
		useGlowCpuTrace || ( glowSprite && glowHasAsyncDecision );
	if( oldFrame && oldFrame != frame )
	{
		const float oldAlpha = 1.0f - frameLerp;
		if( oldAlpha > 0.0f )
			R_QueueSpriteFrame( ent, oldFrame, origin, right, up,
				scale, disableDepth, oldAlpha );
		if( frameLerp > 0.0f )
			R_QueueSpriteFrame( ent, frame, origin, right, up,
				scale, disableDepth, frameLerp );
	}
	else
	{
		R_QueueSpriteFrame( ent, frame, origin, right, up,
			scale, disableDepth, 1.0f );
	}
	return true;
}

static void R_DrawSpriteLightmapPass( const mspriteframe_t *frame,
	const vec3_t origin, const vec3_t right, const vec3_t up, float scale,
	D3DCOLOR lightColor )
{
	if( !frame )
		return;

	DX_SetRenderStateCached( D3DRS_ZFUNC, D3DCMP_EQUAL );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_ZERO );
	DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR );
	R_SetTextureStage( D3DTOP_SELECTARG2, D3DTA_TEXTURE, D3DTA_DIFFUSE,
		D3DTOP_SELECTARG2, D3DTA_TEXTURE, D3DTA_DIFFUSE );

	R_DrawSpriteQuad( frame, origin, right, up, scale, lightColor );

	DX_SetRenderStateCached( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
}

static void R_DrawSpriteEntity( cl_entity_s *ent, model_t *mod )
{
	DX_MODEL_PROFILE_SCOPE( spriteProfileScope, DX_PROFILE_PHASE_SPRITEMODEL );
	if( !ent || !mod || !mod->cache.data )
		return;

	R_SetSpritePipelineBaseline( );

	msprite_t *sprite = (msprite_t *)mod->cache.data;
	if( !sprite )
		return;
	mspriteframe_t *oldFrame = nullptr;
	mspriteframe_t *frame = nullptr;
	float frameLerp = 1.0f;
	R_GetSpriteFramesForDraw( ent, mod, sprite, oldFrame, frame, frameLerp );
	if( !frame || frame->gl_texturenum <= 0 )
		return;

	const bool glowSprite = ent->curstate.rendermode == kRenderGlow;
	const bool useGlowCpuTrace = glowSprite &&
		dx_glow_occlusion.value >= 2.0f;
	if( useGlowCpuTrace && R_GlowSpriteOccludedByWorld( ent ) )
		return;

	bool glowHasAsyncDecision = false;
	bool glowAsyncVisible = true;
	if( glowSprite &&dx_glow_occlusion.value >= 1.0f && dx_glow_occlusion.value < 2.0f )
	{
		glowAsyncVisible = R_GlowQueryVisible( ent,
			ent->origin, glowHasAsyncDecision );
		if( glowHasAsyncDecision && !glowAsyncVisible )
			return;
	}

	vec3_t origin, lightOrigin, right, up;
	VectorCopy( ent->origin, origin );
	VectorCopy( origin, lightOrigin );
	R_BuildSpriteAxes( ent, sprite, origin, right, up );

	const float scale = ent->curstate.scale > 0.0f
		? ent->curstate.scale : 1.0f;

	const bool lightmappedSprite = R_SpriteHasLightmap( ent, sprite );
	dx_entity_state_t renderState{};
	if( dx_remix_perf.value == 0.0f )
		R_SaveEntityRenderState( &renderState );

	sprite_batch_key_t immediateKey{};
	immediateKey.renderMode = ent->curstate.rendermode;
	immediateKey.textureFactor =
		R_SpriteTextureFactor( ent, immediateKey.renderMode, 1.0f );
	immediateKey.disableDepth =
		useGlowCpuTrace || ( glowSprite && glowHasAsyncDecision );
	R_BeginSpriteBatchRenderMode( immediateKey );
	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_NONE );

	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ALPHAFUNC, D3DCMP_GREATER );
	DX_SetRenderStateCached( D3DRS_ALPHAREF,
		lightmappedSprite ? 85u : 0u );

	auto DrawSpriteFrameWeighted = [&]( mspriteframe_t *drawFrame, float alphaScale )
		{
			if( !drawFrame || drawFrame->gl_texturenum <= 0 || alphaScale <= 0.0f )
				return;
			DX_Bind( 0, drawFrame->gl_texturenum );
			if( ent->curstate.rendermode != kRenderNormal )
			{
				DX_SetRenderStateCached( D3DRS_TEXTUREFACTOR,
					R_SpriteTextureFactor( ent, ent->curstate.rendermode, alphaScale ) );
			}
			R_DrawSpriteQuad( drawFrame, origin, right, up, scale,
				R_SpriteVertexColor( ent ) );
		};

	if( oldFrame && oldFrame != frame )
	{
		const float oldAlpha = 1.0f - frameLerp;
		DrawSpriteFrameWeighted( oldFrame, oldAlpha );
		DrawSpriteFrameWeighted( frame, frameLerp );
	}
	else
	{
		DrawSpriteFrameWeighted( frame, 1.0f );
	}

	if( lightmappedSprite )
	{

		const D3DCOLOR localLight = R_SpriteLightColorAtPoint( lightOrigin );
		R_DrawSpriteLightmapPass( frame, origin, right, up, scale, localLight );
	}

	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_CCW );
	DX_SetRenderStateCached( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
	R_RestoreEntityRenderMode( &renderState );
}

void R_DrawSpriteImmediate( cl_entity_s *ent )
{
	if( !ent || !ent->model || ent->model->type != mod_sprite )
		return;
	R_DrawSpriteEntity( ent, ent->model );
}
