
#include "dx_local.h"

namespace
{
	constexpr int LIGHTMAP_ATLAS_SIZE = 1024;
	constexpr int LIGHTMAP_LOCAL_PAGE_MIN = 8;
	constexpr int LIGHTMAP_DYNAMIC_PAGE_MAX = 1024;
	constexpr int LIGHTMAP_GUTTER = 2;
	constexpr size_t LIGHTMAP_MAX_DIRTY_RECTS = 32;
}

static void R_UpdateLightmapGammaLut( )
{
	const float gamma = std::max( 0.5f, std::min( 4.0f, r_lightmap_gamma.value ) );
	if( fabsf( gamma - g_lightmapGammaLutValue ) < 0.0001f )
		return;

	for( int channel = 0; channel < 256; ++channel )
	{
		const float linear = static_cast<float>( channel ) / 255.0f;
		g_lightmapGammaLut[channel] = static_cast<byte>( std::min( 255,
			static_cast<int>( powf( linear, 1.0f / gamma ) * 255.0f + 0.5f ) ) );
	}
	g_lightmapGammaLutValue = gamma;
}

int R_GetLightmapAtlasCount( void )
{
	return static_cast<int>( g_mapLightmaps.atlases.size( ) +
		g_mapLightmaps.dynamicOverlayAtlases.size( ) );
}

static const world_lightmap_atlas_t *R_GetDebugLightmapAtlas( int atlasIndex,
	bool *outOverlay = nullptr )
{
	if( outOverlay ) *outOverlay = false;
	if( atlasIndex < 0 )
		return nullptr;

	const int baseCount = static_cast<int>( g_mapLightmaps.atlases.size( ) );
	if( atlasIndex < baseCount )
		return &g_mapLightmaps.atlases[atlasIndex];

	const int overlayIndex = atlasIndex - baseCount;
	if( overlayIndex < 0 || overlayIndex >=
		static_cast<int>( g_mapLightmaps.dynamicOverlayAtlases.size( ) ) )
	{
		return nullptr;
	}
	if( outOverlay ) *outOverlay = true;
	return &g_mapLightmaps.dynamicOverlayAtlases[overlayIndex];
}

IDirect3DTexture9 *R_GetLightmapAtlasTexture( int atlasIndex )
{
	const world_lightmap_atlas_t *atlas = R_GetDebugLightmapAtlas( atlasIndex );
	return atlas ? atlas->texture : nullptr;
}

int R_GetLightmapAtlasSize( int atlasIndex )
{
	const world_lightmap_atlas_t *atlas = R_GetDebugLightmapAtlas( atlasIndex );
	return atlas ? atlas->size : 0;
}

qboolean R_LightmapAtlasDynamic( int atlasIndex )
{
	const world_lightmap_atlas_t *atlas = R_GetDebugLightmapAtlas( atlasIndex );
	return atlas && atlas->dynamic ? true : false;
}

qboolean R_LightmapAtlasOverlay( int atlasIndex )
{
	bool overlay = false;
	const world_lightmap_atlas_t *atlas =
		R_GetDebugLightmapAtlas( atlasIndex, &overlay );
	return atlas && overlay && atlas->dynamicOverlay ? true : false;
}

const char *R_GetLightmapAtlasMaterial( int atlasIndex )
{
	const world_lightmap_atlas_t *atlas = R_GetDebugLightmapAtlas( atlasIndex );
	const texture_t *material = atlas ? atlas->materialGroup : nullptr;
	return material ? material->name : nullptr;
}

int R_GetLightmapAtlasAllocCount( int atlasIndex )
{
	const world_lightmap_atlas_t *atlas = R_GetDebugLightmapAtlas( atlasIndex );
	return atlas ? atlas->allocationCount : 0;
}

float R_GetLightmapAtlasOccupancy( int atlasIndex )
{
	const world_lightmap_atlas_t *atlas = R_GetDebugLightmapAtlas( atlasIndex );
	if( !atlas || atlas->size <= 0 )
		return 0.0f;

	const size_t capacity = static_cast<size_t>( atlas->size ) * atlas->size;
	return capacity ? std::min( 1.0f, static_cast<float>( atlas->usedTexels ) /
		static_cast<float>( capacity ) ) : 0.0f;
}

static bool R_GetSurfaceLightmapSize( const msurface_t *surface, int &width, int &height )
{
	width = height = 0;

	if( !surface || !surface->info || !surface->samples ||
		( surface->flags & SURF_DRAWTILED ) )
	{
		return false;
	}

	const float sampleSize = R_SurfaceSampleSize( surface );
	width = static_cast<int>( surface->info->lightextents[0] / sampleSize ) + 1;
	height = static_cast<int>( surface->info->lightextents[1] / sampleSize ) + 1;
	return width > 0 && height > 0;
}

static bool R_HasStyleLightmap( const msurface_t *surface )
{
	if( !surface || dx_separate_style_lightmaps.value == 0.0f )
		return false;

	constexpr int minimumStyle = 1;

	for( int i = 0; i < DX_LIGHTMAP_STYLE_SLOTS; ++i )
	{
		const byte style = surface->styles[i];
		if( style == 255 )
			break;

		if( style != 0 && style >= minimumStyle )
			return true;
	}
	return false;
}

static void R_BuildSurfaceLightmapVerts( const model_t *mod,
	const msurface_t *surface, lightmap_vertex_t *verts, int &outCount )
{
	outCount = 0;
	if( !mod || !surface || surface->numedges < 3 || surface->numedges > 64 )
		return;

	const mtexinfo_t *texinfo = surface->texinfo;
	const texture_t *texture = texinfo ? texinfo->texture : nullptr;
	const float tw = texture ? static_cast<float>( texture->width ) : 64.0f;
	const float th = texture ? static_cast<float>( texture->height ) : 64.0f;
	const int vertexCount = surface->numedges;
	const mextrasurf_t *info = surface->info;
	const float sampleSize = R_SurfaceSampleSize( surface );

	for( int i = 0; i < vertexCount; ++i )
	{
		const mvertex_t *vertex = R_GetBrushSurfaceVertex( mod, surface, i );
		if( !vertex )
			return;

		lightmap_vertex_t &v = verts[i];
		v.x = vertex->position[0];
		v.y = vertex->position[1];
		v.z = vertex->position[2];
		v.diffuse = D3DCOLOR_ARGB( 255, 255, 255, 255 );

		if( texinfo )
		{
			v.u = ( DotProduct( vertex->position, texinfo->vecs[0] ) + texinfo->vecs[0][3] ) / tw;
			v.v = ( DotProduct( vertex->position, texinfo->vecs[1] ) + texinfo->vecs[1][3] ) / th;
		}
		else
		{
			v.u = 0.0f;
			v.v = 0.0f;
		}

		if( info )
		{
			const int lightmapWidth =
				static_cast<int>( info->lightextents[0] / sampleSize ) + 1;
			const int lightmapHeight =
				static_cast<int>( info->lightextents[1] / sampleSize ) + 1;

			const float localS = ( DotProduct( vertex->position, info->lmvecs[0] ) +
				info->lmvecs[0][3] - info->lightmapmins[0] ) / sampleSize;
			const float localT = ( DotProduct( vertex->position, info->lmvecs[1] ) +
				info->lmvecs[1][3] - info->lightmapmins[1] ) / sampleSize;

			v.lu = std::max( 0.0f,
				std::min( static_cast<float>( lightmapWidth - 1 ), localS ) );
			v.lv = std::max( 0.0f,
				std::min( static_cast<float>( lightmapHeight - 1 ), localT ) );
		}
		else
		{
			v.lu = 0.0f;
			v.lv = 0.0f;
		}
	}

	outCount = vertexCount;
}

static void R_DestroyWorldLightmapStore( world_lightmap_store_t &store )
{
	auto ReleaseAtlases = []( std::vector<world_lightmap_atlas_t> &atlases )
		{
			for( auto &atlas : atlases )
			{
				if( atlas.texture )
				{
					atlas.texture->Release( );
					atlas.texture = nullptr;
				}
			}
			atlases.clear( );
		};

	ReleaseAtlases( store.atlases );
	ReleaseAtlases( store.dynamicOverlayAtlases );
	store.allocations.clear( );
	store.allocationBySurface.clear( );
	store.dynamicPageSizeByMaterial.clear( );
	for( auto &styleAllocations : store.allocationsByStyle )
		styleAllocations.clear( );
}

void R_FreeWaterBatches( world_water_batch_t &batch, bool releaseManagedIndex )
{
	if( batch.vertexBuffer )
	{
		batch.vertexBuffer->Release( );
		batch.vertexBuffer = nullptr;
	}
	batch.vertexCacheValid = false;
	if( releaseManagedIndex && batch.indexBuffer )
	{
		batch.indexBuffer->Release( );
		batch.indexBuffer = nullptr;
	}
}

void R_FreeWorldModel( world_model_draw_t &model )
{
	if( model.sharedVertexBuffer )
	{
		model.sharedVertexBuffer->Release( );
		model.sharedVertexBuffer = nullptr;
	}
	if( model.sharedIndexBuffer )
	{
		model.sharedIndexBuffer->Release( );
		model.sharedIndexBuffer = nullptr;
	}
	if( model.overlaySharedVertexBuffer )
	{
		model.overlaySharedVertexBuffer->Release( );
		model.overlaySharedVertexBuffer = nullptr;
	}
	if( model.overlaySharedIndexBuffer )
	{
		model.overlaySharedIndexBuffer->Release( );
		model.overlaySharedIndexBuffer = nullptr;
	}

	for( auto &batch : model.batches )
	{
		if( batch.vertexBuffer )
		{
			batch.vertexBuffer->Release( );
			batch.vertexBuffer = nullptr;
		}
		if( batch.indexBuffer )
		{
			batch.indexBuffer->Release( );
			batch.indexBuffer = nullptr;
		}
	}
	for( auto &batch : model.waterBatches )
		R_FreeWaterBatches( batch, true );

	model.batches.clear( );
	model.dynamicOverlayBatches.clear( );
	model.waterBatches.clear( );
	model.waterPvsPortals.clear( );
	model.sourceModel = nullptr;
	model.pvsApplicable = false;
}

static int R_NextPowerOfTwo( int value )
{
	int result = 1;
	while( result < value && result < LIGHTMAP_DYNAMIC_PAGE_MAX )
		result <<= 1;
	return result;
}

static int R_LightmapPageLimit( float requestedValue )
{
	int requested = static_cast<int>( requestedValue );
	requested = std::max( LIGHTMAP_LOCAL_PAGE_MIN,
		std::min( LIGHTMAP_DYNAMIC_PAGE_MAX, requested ) );
	return R_NextPowerOfTwo( requested );
}

static bool R_LightmapRectLarger(
	const lightmap_rect_t &a,
	const lightmap_rect_t &b )
{
	const int aLongest = std::max( a.width, a.height );
	const int bLongest = std::max( b.width, b.height );
	if( aLongest != bLongest )
		return aLongest > bLongest;

	const int aArea = a.width * a.height;
	const int bArea = b.width * b.height;
	if( aArea != bArea )
		return aArea > bArea;

	if( a.height != b.height )
		return a.height > b.height;
	return a.width > b.width;
}

static void R_SortLightmapPackRects(
	std::vector<lightmap_rect_t> &rects )
{
	std::sort( rects.begin( ), rects.end( ),
		R_LightmapRectLarger );
}

static bool R_CanPackLightmaps(
	const std::vector<lightmap_rect_t> &rects, int pageSize )
{
	if( pageSize <= 0 )
		return false;

	int nextX = 0;
	int nextY = 0;
	int rowHeight = 0;

	for( const lightmap_rect_t &rect : rects )
	{
		if( rect.width <= 0 || rect.height <= 0 ||
			rect.width > pageSize || rect.height > pageSize )
		{
			return false;
		}

		int x = nextX;
		int y = nextY;

		if( x + rect.width > pageSize )
		{
			x = 0;
			y += rowHeight;
			rowHeight = 0;
		}

		if( y + rect.height > pageSize )
			return false;

		nextX = x + rect.width;
		nextY = y;
		rowHeight = std::max( rowHeight, rect.height );
	}

	return true;
}

static int R_SelectLightmapPageSize(
	const std::vector<lightmap_rect_t> &rects,
	int configuredLimit )
{
	configuredLimit = std::max( LIGHTMAP_LOCAL_PAGE_MIN,
		std::min( LIGHTMAP_DYNAMIC_PAGE_MAX, configuredLimit ) );
	configuredLimit = R_NextPowerOfTwo( configuredLimit );

	int minimumSize = LIGHTMAP_LOCAL_PAGE_MIN;
	for( const lightmap_rect_t &rect : rects )
		minimumSize = std::max( minimumSize,
			std::max( rect.width, rect.height ) );
	minimumSize = R_NextPowerOfTwo( minimumSize );

	const int largestAllowedCandidate =
		std::max( configuredLimit, minimumSize );

	for( int candidate = minimumSize;
		candidate <= largestAllowedCandidate;
		candidate <<= 1 )
	{
		if( R_CanPackLightmaps( rects, candidate ) )
			return candidate;

		if( candidate >= LIGHTMAP_DYNAMIC_PAGE_MAX )
			break;
	}

	return largestAllowedCandidate;
}

static void R_PlanMaterialLightmaps(
	world_lightmap_store_t &store, const model_t *mod )
{
	store.dynamicPageSizeByMaterial.clear( );

	if( !mod || mod->type != mod_brush || !mod->surfaces )
		return;

	dxfast::HashMap<texture_t *, std::vector<lightmap_rect_t>> dynamicRects;

	for( int surfaceIndex = 0; surfaceIndex < mod->numsurfaces; ++surfaceIndex )
	{
		msurface_t *surface = &mod->surfaces[surfaceIndex];
		if( !surface->texinfo || !surface->texinfo->texture ||
			( surface->flags & SURF_DRAWSKY ) || !R_HasStyleLightmap( surface ) )
		{
			continue;
		}

		int width = 0;
		int height = 0;
		if( !R_GetSurfaceLightmapSize( surface, width, height ) )
			continue;

		lightmap_rect_t rect;
		rect.width = width + LIGHTMAP_GUTTER * 2;
		rect.height = height + LIGHTMAP_GUTTER * 2;
		dynamicRects[surface->texinfo->texture].push_back( rect );
	}

	const int dynamicLimit = R_LightmapPageLimit( dx_lightmap_dynamic_page.value );
	store.dynamicPageSizeByMaterial.reserve( dynamicRects.size( ) );
	for( auto &entry : dynamicRects )
	{
		R_SortLightmapPackRects( entry.second );
		store.dynamicPageSizeByMaterial.emplace( entry.first,
			R_SelectLightmapPageSize( entry.second, dynamicLimit ) );
	}
}

static int R_MaterialLightmapPageSize(
	const world_lightmap_store_t &store, texture_t *materialGroup, int minimumSize )
{
	const int minimumPage = R_NextPowerOfTwo(
		std::max( LIGHTMAP_LOCAL_PAGE_MIN, minimumSize ) );

	if( materialGroup )
	{
		const auto found = store.dynamicPageSizeByMaterial.find( materialGroup );
		if( found != store.dynamicPageSizeByMaterial.end( ) )
			return std::max( minimumPage, found->second );
	}

	return minimumPage;
}

static int R_AddLightmapAtlas( world_lightmap_store_t &store, bool dynamic,
	texture_t *materialGroup, int minimumSize, bool materialLocal )
{
	world_lightmap_atlas_t atlas;
	atlas.dynamic = dynamic;
	atlas.materialGroup = materialLocal ? materialGroup : nullptr;
	if( materialLocal )
	{
		atlas.size = R_MaterialLightmapPageSize(
			store, materialGroup, minimumSize );
	}
	else
	{
		atlas.size = LIGHTMAP_ATLAS_SIZE;
	}

	atlas.pixels.assign( static_cast<size_t>( atlas.size ) * atlas.size,
		D3DCOLOR_ARGB( 255, 0, 0, 0 ) );
	store.atlases.push_back( std::move( atlas ) );
	return static_cast<int>( store.atlases.size( ) - 1 );
}

static bool R_AllocateAtlasRect( world_lightmap_store_t &store, int width, int height,
	bool dynamic, texture_t *materialGroup, bool materialLocal,
	int &outAtlasIndex, int &outX, int &outY )
{
	const int packedWidth = width + LIGHTMAP_GUTTER * 2;
	const int packedHeight = height + LIGHTMAP_GUTTER * 2;
	const int minimumSize = std::max( packedWidth, packedHeight );

	if( width <= 0 || height <= 0 ||
		minimumSize > LIGHTMAP_DYNAMIC_PAGE_MAX )
	{
		return false;
	}

	for( int atlasIndex = 0; atlasIndex < static_cast<int>( store.atlases.size( ) ) + 1; ++atlasIndex )
	{
		if( atlasIndex == static_cast<int>( store.atlases.size( ) ) )
		{
			R_AddLightmapAtlas( store, dynamic, materialGroup, minimumSize,
				materialLocal );
		}

		world_lightmap_atlas_t &atlas = store.atlases[atlasIndex];
		if( atlas.dynamic != dynamic ||
			( materialLocal && atlas.materialGroup != materialGroup ) ||
			( !materialLocal && atlas.materialGroup != nullptr ) ||
			atlas.size < minimumSize )
		{
			continue;
		}

		int x = atlas.nextX;
		int y = atlas.nextY;
		int candidateRowHeight = atlas.rowHeight;

		if( x + packedWidth > atlas.size )
		{
			x = 0;
			y += atlas.rowHeight;
			candidateRowHeight = 0;
		}

		if( y + packedHeight > atlas.size )
			continue;

		atlas.nextX = x + packedWidth;
		atlas.nextY = y;
		atlas.rowHeight = std::max( candidateRowHeight, packedHeight );
		atlas.usedTexels += static_cast<size_t>( packedWidth ) *
			static_cast<size_t>( packedHeight );
		++atlas.allocationCount;

		outAtlasIndex = atlasIndex;
		outX = x + LIGHTMAP_GUTTER;
		outY = y + LIGHTMAP_GUTTER;
		return true;
	}

	return false;
}

static bool R_ValidateLightmapAtlases(
	const world_lightmap_store_t &store, bool overlay )
{
	const auto &atlases = overlay ? store.dynamicOverlayAtlases : store.atlases;
	bool valid = true;

	for( size_t a = 0; a < store.allocations.size( ); ++a )
	{
		const world_lightmap_allocation_t &lhs = store.allocations[a];
		const int lhsAtlas = overlay ? lhs.overlayAtlasIndex : lhs.atlasIndex;
		const int lhsX = overlay ? lhs.overlayX : lhs.x;
		const int lhsY = overlay ? lhs.overlayY : lhs.y;
		if( lhsAtlas < 0 || lhsAtlas >= static_cast<int>( atlases.size( ) ) )
			continue;

		const int lhsLeft = lhsX - LIGHTMAP_GUTTER;
		const int lhsTop = lhsY - LIGHTMAP_GUTTER;
		const int lhsRight = lhsX + lhs.width + LIGHTMAP_GUTTER;
		const int lhsBottom = lhsY + lhs.height + LIGHTMAP_GUTTER;

		const world_lightmap_atlas_t &atlas = atlases[lhsAtlas];
		if( lhsLeft < 0 || lhsTop < 0 ||
			lhsRight > atlas.size || lhsBottom > atlas.size )
		{
			gEngfuncs.Con_Printf(
				"DX9 ERROR: %s lightmap allocation %u is outside atlas %d "
				"(%d,%d)-(%d,%d) / %d\n",
				overlay ? "overlay" : "base",
				static_cast<unsigned int>( a ), lhsAtlas,
				lhsLeft, lhsTop, lhsRight, lhsBottom, atlas.size );
			valid = false;
		}

		for( size_t b = a + 1; b < store.allocations.size( ); ++b )
		{
			const world_lightmap_allocation_t &rhs = store.allocations[b];
			const int rhsAtlas = overlay ? rhs.overlayAtlasIndex : rhs.atlasIndex;
			if( rhsAtlas != lhsAtlas )
				continue;

			const int rhsX = overlay ? rhs.overlayX : rhs.x;
			const int rhsY = overlay ? rhs.overlayY : rhs.y;
			const int rhsLeft = rhsX - LIGHTMAP_GUTTER;
			const int rhsTop = rhsY - LIGHTMAP_GUTTER;
			const int rhsRight = rhsX + rhs.width + LIGHTMAP_GUTTER;
			const int rhsBottom = rhsY + rhs.height + LIGHTMAP_GUTTER;

			const bool overlaps =
				lhsLeft < rhsRight && lhsRight > rhsLeft &&
				lhsTop < rhsBottom && lhsBottom > rhsTop;
			if( !overlaps )
				continue;

			gEngfuncs.Con_Printf(
				"DX9 ERROR: %s lightmap allocations %u and %u overlap "
				"in atlas %d\n",
				overlay ? "overlay" : "base",
				static_cast<unsigned int>( a ),
				static_cast<unsigned int>( b ), lhsAtlas );
			valid = false;
		}
	}

	return valid;
}

static int R_DynamicOverlayPageSize( int minimumSize )
{
	int preferred = static_cast<int>( dx_lightmap_overlay_page.value );
	preferred = std::max( 64, std::min( LIGHTMAP_DYNAMIC_PAGE_MAX, preferred ) );
	preferred = R_NextPowerOfTwo( preferred );
	return std::max( preferred, R_NextPowerOfTwo( minimumSize ) );
}

static bool R_AllocateDynamicOverlayRect( world_lightmap_store_t &store,
	int width, int height, int &outAtlasIndex, int &outX, int &outY )
{
	const int packedWidth = width + LIGHTMAP_GUTTER * 2;
	const int packedHeight = height + LIGHTMAP_GUTTER * 2;
	const int minimumSize = std::max( packedWidth, packedHeight );
	if( width <= 0 || height <= 0 || minimumSize > LIGHTMAP_DYNAMIC_PAGE_MAX )
		return false;

	for( int atlasIndex = 0; atlasIndex <
		static_cast<int>( store.dynamicOverlayAtlases.size( ) ); ++atlasIndex )
	{
		world_lightmap_atlas_t &atlas = store.dynamicOverlayAtlases[atlasIndex];
		if( atlas.size < minimumSize )
			continue;

		int x = atlas.nextX;
		int y = atlas.nextY;
		int candidateRowHeight = atlas.rowHeight;
		if( x + packedWidth > atlas.size )
		{
			x = 0;
			y += atlas.rowHeight;
			candidateRowHeight = 0;
		}
		if( y + packedHeight > atlas.size )
			continue;

		atlas.nextX = x + packedWidth;
		atlas.nextY = y;
		atlas.rowHeight = std::max( candidateRowHeight, packedHeight );
		atlas.usedTexels += static_cast<size_t>( packedWidth ) * packedHeight;
		++atlas.allocationCount;
		outAtlasIndex = atlasIndex;
		outX = x + LIGHTMAP_GUTTER;
		outY = y + LIGHTMAP_GUTTER;
		return true;
	}

	world_lightmap_atlas_t atlas;
	atlas.dynamic = true;
	atlas.dynamicOverlay = true;
	atlas.size = R_DynamicOverlayPageSize( minimumSize );
	atlas.pixels.assign( static_cast<size_t>( atlas.size ) * atlas.size,
		D3DCOLOR_ARGB( 255, 0, 0, 0 ) );
	store.dynamicOverlayAtlases.push_back( std::move( atlas ) );

	world_lightmap_atlas_t &created = store.dynamicOverlayAtlases.back( );
	created.nextX = packedWidth;
	created.nextY = 0;
	created.rowHeight = packedHeight;
	created.usedTexels = static_cast<size_t>( packedWidth ) * packedHeight;
	created.allocationCount = 1;
	outAtlasIndex = static_cast<int>( store.dynamicOverlayAtlases.size( ) - 1 );
	outX = LIGHTMAP_GUTTER;
	outY = LIGHTMAP_GUTTER;
	return true;
}

static int R_LightStyleValue( const byte style )
{
	if( style == 255 )
		return 0;
	return g_lightStyleValues[style];
}

static D3DCOLOR R_ReadSurfaceLightmapTexel( const msurface_t *surface,
	int texelIndex, int texelCount )
{
	if( !surface || !surface->samples || texelIndex < 0 || texelIndex >= texelCount )
		return D3DCOLOR_ARGB( 255, 255, 255, 255 );

	const color24 *samplePlane = surface->samples;
	int red = 0;
	int green = 0;
	int blue = 0;
	bool hasActiveStyle = false;

	for( int styleIndex = 0; styleIndex < DX_LIGHTMAP_STYLE_SLOTS; ++styleIndex )
	{
		const byte style = surface->styles[styleIndex];
		if( style == 255 )
			break;

		const int scale = std::max( 0, R_LightStyleValue( style ) );
		const color24 &sample = samplePlane[texelIndex];

		red += static_cast<int>( sample.r ) * scale;
		green += static_cast<int>( sample.g ) * scale;
		blue += static_cast<int>( sample.b ) * scale;
		hasActiveStyle = true;
		samplePlane += texelCount;
	}

	if( !hasActiveStyle )
		return D3DCOLOR_ARGB( 255, 255, 255, 255 );

	red = std::min( 255, ( red + 128 ) >> 8 );
	green = std::min( 255, ( green + 128 ) >> 8 );
	blue = std::min( 255, ( blue + 128 ) >> 8 );

	return D3DCOLOR_ARGB( 255, g_lightmapGammaLut[red],
		g_lightmapGammaLut[green], g_lightmapGammaLut[blue] );
}

static float R_LightmapPixelLuminance( D3DCOLOR pixel )
{
	const float red = static_cast<float>( ( pixel >> 16 ) & 0xFF ) / 255.0f;
	const float green = static_cast<float>( ( pixel >> 8 ) & 0xFF ) / 255.0f;
	const float blue = static_cast<float>( pixel & 0xFF ) / 255.0f;
	return red * 0.2126f + green * 0.7152f + blue * 0.0722f;
}

static bool R_AOSurfaceCanBlock( const msurface_t *surface )
{
	if( !surface || !surface->plane || surface->numedges < 3 )
		return false;

	if( surface->flags & ( SURF_DRAWSKY | SURF_DRAWTURB | SURF_DRAWTILED ) )
		return false;

	return true;
}

static float R_AOPointSegmentDistance( const vec3_t point, const vec3_t a,
	const vec3_t b, vec3_t closest )
{
	const float ab0 = b[0] - a[0];
	const float ab1 = b[1] - a[1];
	const float ab2 = b[2] - a[2];
	const float ap0 = point[0] - a[0];
	const float ap1 = point[1] - a[1];
	const float ap2 = point[2] - a[2];
	const float denom = ab0 * ab0 + ab1 * ab1 + ab2 * ab2;
	float t = denom > 0.000001f ?
		( ap0 * ab0 + ap1 * ab1 + ap2 * ab2 ) / denom : 0.0f;
	t = std::max( 0.0f, std::min( 1.0f, t ) );
	closest[0] = a[0] + ab0 * t;
	closest[1] = a[1] + ab1 * t;
	closest[2] = a[2] + ab2 * t;
	const float dx = point[0] - closest[0];
	const float dy = point[1] - closest[1];
	const float dz = point[2] - closest[2];
	return sqrtf( dx * dx + dy * dy + dz * dz );
}

void R_ClearAOEdgeCache( void )
{
	g_aoEdgeCacheModel = nullptr;
	g_aoEdgeCache.clear( );
}

static void R_BuildAOEdgeCache( void )
{
	g_aoEdgeCacheModel = g_worldmodel;
	g_aoEdgeCacheNeighbour = dx_ao_true_edge_neighbor.value;
	g_aoEdgeCacheCoplanarSuppress = dx_ao_coplanar_suppress.value;
	g_aoEdgeCache.clear( );

	if( !g_worldmodel || g_worldmodel->type != mod_brush ||
		g_worldmodel->numsurfaces <= 0 || !g_worldmodel->surfaces )
	{
		return;
	}

	g_aoEdgeCache.resize( g_worldmodel->numsurfaces );
	std::vector<std::vector<int>> edgeUsers(
		static_cast<size_t>( std::max( 0, g_worldmodel->numedges ) ) );

	for( int surfaceIndex = 0; surfaceIndex < g_worldmodel->numsurfaces; ++surfaceIndex )
	{
		const msurface_t *surface = &g_worldmodel->surfaces[surfaceIndex];
		g_aoEdgeCache[surfaceIndex].surface = surface;
		if( !R_AOSurfaceCanBlock( surface ) )
			continue;

		for( int edgeOrdinal = 0; edgeOrdinal < surface->numedges; ++edgeOrdinal )
		{
			const int absoluteEdge = R_GetSurfaceEdge(
				g_worldmodel, surface, edgeOrdinal );
			if( absoluteEdge >= 0 &&
				static_cast<size_t>( absoluteEdge ) < edgeUsers.size( ) )
			{
				edgeUsers[static_cast<size_t>( absoluteEdge )].push_back( surfaceIndex );
			}
		}
	}

	const bool requireNeighbour = dx_ao_true_edge_neighbor.value != 0.0f;
	const bool suppressCoplanar = dx_ao_coplanar_suppress.value != 0.0f;

	for( int surfaceIndex = 0; surfaceIndex < g_worldmodel->numsurfaces; ++surfaceIndex )
	{
		const msurface_t *surface = &g_worldmodel->surfaces[surfaceIndex];
		if( !R_AOSurfaceCanBlock( surface ) )
			continue;

		ao_surface_cache_t &cache = g_aoEdgeCache[surfaceIndex];
		cache.occludingEdges.reserve( static_cast<size_t>( surface->numedges ) );
		const float *normal = surface->plane->normal;

		for( int edgeOrdinal = 0; edgeOrdinal < surface->numedges; ++edgeOrdinal )
		{
			const int absoluteEdge = R_GetSurfaceEdge(
				g_worldmodel, surface, edgeOrdinal );
			if( absoluteEdge < 0 )
				continue;

			bool hasCoplanarContinuation = false;
			bool hasNonCoplanarNeighbour = false;
			if( static_cast<size_t>( absoluteEdge ) < edgeUsers.size( ) )
			{
				const std::vector<int> &users = edgeUsers[static_cast<size_t>( absoluteEdge )];
				for( int otherIndex : users )
				{
					if( otherIndex == surfaceIndex || otherIndex < 0 ||
						otherIndex >= g_worldmodel->numsurfaces )
					{
						continue;
					}

					const msurface_t *other = &g_worldmodel->surfaces[otherIndex];
					if( !R_AOSurfaceCanBlock( other ) )
						continue;

					const float ndot = fabsf( DotProduct( normal,
						other->plane->normal ) );
					if( ndot > 0.999f )
						hasCoplanarContinuation = true;
					else if( ndot < 0.995f )
						hasNonCoplanarNeighbour = true;
				}
			}

			if( suppressCoplanar && hasCoplanarContinuation )
				continue;
			if( requireNeighbour && !hasNonCoplanarNeighbour )
				continue;

			const mvertex_t *a = R_GetBrushSurfaceVertex( g_worldmodel,
				surface, edgeOrdinal );
			const mvertex_t *b = R_GetBrushSurfaceVertex( g_worldmodel,
				surface, ( edgeOrdinal + 1 ) % surface->numedges );
			if( !a || !b )
				continue;

			ao_edge_t edge;
			VectorCopy( a->position, edge.a );
			VectorCopy( b->position, edge.b );
			cache.occludingEdges.push_back( edge );
		}
	}
}

static void R_EnsureAOEdgeCache( void )
{
	if( g_aoEdgeCacheModel == g_worldmodel &&
		fabsf( g_aoEdgeCacheNeighbour - dx_ao_true_edge_neighbor.value ) <= 0.0001f &&
		fabsf( g_aoEdgeCacheCoplanarSuppress - dx_ao_coplanar_suppress.value ) <= 0.0001f )
	{
		return;
	}

	R_BuildAOEdgeCache( );
}

static float R_ComputeEdgeAO( const msurface_t *surface,
	int sampleX, int sampleY, int width, int height, float radiusWorld )
{
	if( dx_ao_true_edges.value == 0.0f || !g_worldmodel ||
		!R_AOSurfaceCanBlock( surface ) || radiusWorld <= 0.5f )
	{
		return 0.0f;
	}

	const float edgeStrength = std::max( 0.0f,
		std::min( 3.0f, dx_ao_true_edge_strength.value ) );
	if( edgeStrength <= 0.0f )
		return 0.0f;

	R_EnsureAOEdgeCache( );
	if( g_aoEdgeCacheModel != g_worldmodel || !g_worldmodel->surfaces )
		return 0.0f;

	const int surfaceIndex = static_cast<int>( surface - g_worldmodel->surfaces );
	if( surfaceIndex < 0 ||
		static_cast<size_t>( surfaceIndex ) >= g_aoEdgeCache.size( ) )
	{
		return 0.0f;
	}

	const ao_surface_cache_t &cache =
		g_aoEdgeCache[static_cast<size_t>( surfaceIndex )];
	if( cache.occludingEdges.empty( ) )
		return 0.0f;

	const float sampleSize = R_SurfaceSampleSize( surface );
	vec3_t worldPoint;
	if( !R_LightmapTexelWorldPoint( surface, sampleX, sampleY, sampleSize, worldPoint ) )
		return 0.0f;

	float occlusion = 0.0f;
	for( const ao_edge_t &edge : cache.occludingEdges )
	{
		vec3_t edgePoint;
		const float distance = R_AOPointSegmentDistance( worldPoint,
			edge.a, edge.b, edgePoint );
		if( distance >= radiusWorld )
			continue;

		const float fade = 1.0f - std::max( 0.0f,
			std::min( 1.0f, distance / radiusWorld ) );
		occlusion = std::max( occlusion, fade * fade * edgeStrength );
	}

	return std::min( 1.0f, occlusion );
}

static float R_ComputeContrastAO( const msurface_t *surface,
	int sampleX, int sampleY, int width, int height, D3DCOLOR pixel )
{
	if( dx_ao_contrast.value == 0.0f || !surface || width <= 2 || height <= 2 )
		return 0.0f;

	const float contrastStrength = std::max( 0.0f,
		std::min( 2.0f, dx_ao_contrast_strength.value ) );
	if( contrastStrength <= 0.0f )
		return 0.0f;

	const float threshold = std::max( 0.0f,
		std::min( 0.75f, dx_ao_contrast_threshold.value ) );
	const float centerLum = R_LightmapPixelLuminance( pixel );
	float minNeighborLum = centerLum;

	static const int offsets[8][2] =
	{
		{ -1,  0 }, {  1,  0 }, {  0, -1 }, {  0,  1 },
		{ -1, -1 }, {  1, -1 }, { -1,  1 }, {  1,  1 }
	};

	for( const auto &offset : offsets )
	{
		const int nx = sampleX + offset[0];
		const int ny = sampleY + offset[1];
		if( nx < 0 || ny < 0 || nx >= width || ny >= height )
			continue;

		const D3DCOLOR neighbor = R_ReadSurfaceLightmapTexel(
			surface, ny * width + nx, width * height );
		minNeighborLum = std::min( minNeighborLum,
			R_LightmapPixelLuminance( neighbor ) );
	}

	const float contrast = centerLum - minNeighborLum;
	if( contrast <= threshold )
		return 0.0f;

	return std::min( 1.0f, ( contrast - threshold ) * contrastStrength );
}

static float R_ComputeAO( const msurface_t *surface,
	int sampleX, int sampleY, int width, int height )
{
	if( dx_ao.value == 0.0f || !surface || width <= 1 || height <= 1 )
		return 0.0f;

	const float sampleSize = std::max( 1.0f, R_SurfaceSampleSize( surface ) );
	const float radiusSamples = std::max( 0.25f,
		std::min( 64.0f, dx_ao_radius.value / sampleSize ) );
	if( radiusSamples <= 0.25f )
		return 0.0f;

	float occlusion = 0.0f;

	occlusion += R_ComputeEdgeAO( surface, sampleX, sampleY,
		width, height, dx_ao_radius.value );

	if( dx_ao_surface_borders.value != 0.0f )
	{
		const float left = static_cast<float>( sampleX );
		const float right = static_cast<float>( width - 1 - sampleX );
		const float top = static_cast<float>( sampleY );
		const float bottom = static_cast<float>( height - 1 - sampleY );
		const float horizontalEdge = 1.0f - std::max( 0.0f,
			std::min( 1.0f, std::min( left, right ) / radiusSamples ) );
		const float verticalEdge = 1.0f - std::max( 0.0f,
			std::min( 1.0f, std::min( top, bottom ) / radiusSamples ) );

		const float edge = std::max( horizontalEdge, verticalEdge );
		const float corner = std::min( horizontalEdge, verticalEdge );
		const float edgeStrength = std::max( 0.0f,
			std::min( 2.0f, dx_ao_edge_strength.value ) );
		const float cornerStrength = std::max( 0.0f,
			std::min( 2.0f, dx_ao_corner_strength.value ) );
		occlusion += edge * edge * edgeStrength + corner * corner * cornerStrength;
	}

	return std::max( 0.0f, std::min( 1.0f, occlusion ) );
}

static bool R_AOCacheValid( const world_lightmap_allocation_t &allocation )
{
	if( dx_ao_cache.value == 0.0f )
		return false;
	if( allocation.aoCacheWidth != allocation.width ||
		allocation.aoCacheHeight != allocation.height )
	{
		return false;
	}
	if( allocation.aoGeometryOcclusion.size( ) !=
		static_cast<size_t>( allocation.width ) * allocation.height )
	{
		return false;
	}

	return fabsf( allocation.aoCacheRadius - dx_ao_radius.value ) <= 0.0001f &&
		fabsf( allocation.aoCacheEdgeStrength - dx_ao_edge_strength.value ) <= 0.0001f &&
		fabsf( allocation.aoCacheCornerStrength - dx_ao_corner_strength.value ) <= 0.0001f &&
		fabsf( allocation.aoCacheSurfaceBorders - dx_ao_surface_borders.value ) <= 0.0001f &&
		fabsf( allocation.aoCacheTrueEdges - dx_ao_true_edges.value ) <= 0.0001f &&
		fabsf( allocation.aoCacheTrueEdgeStrength - dx_ao_true_edge_strength.value ) <= 0.0001f &&
		fabsf( allocation.aoCacheTrueEdgeNeighbor - dx_ao_true_edge_neighbor.value ) <= 0.0001f &&
		fabsf( allocation.aoCacheCoplanarSuppress - dx_ao_coplanar_suppress.value ) <= 0.0001f;
}

static void R_BuildAOCache( world_lightmap_allocation_t &allocation )
{
	allocation.aoGeometryOcclusion.clear( );
	allocation.aoCacheWidth = allocation.width;
	allocation.aoCacheHeight = allocation.height;
	allocation.aoCacheRadius = dx_ao_radius.value;
	allocation.aoCacheEdgeStrength = dx_ao_edge_strength.value;
	allocation.aoCacheCornerStrength = dx_ao_corner_strength.value;
	allocation.aoCacheSurfaceBorders = dx_ao_surface_borders.value;
	allocation.aoCacheTrueEdges = dx_ao_true_edges.value;
	allocation.aoCacheTrueEdgeStrength = dx_ao_true_edge_strength.value;
	allocation.aoCacheTrueEdgeNeighbor = dx_ao_true_edge_neighbor.value;
	allocation.aoCacheCoplanarSuppress = dx_ao_coplanar_suppress.value;

	if( dx_ao.value == 0.0f || !allocation.surface ||
		allocation.width <= 0 || allocation.height <= 0 )
	{
		return;
	}

	allocation.aoGeometryOcclusion.resize(
		static_cast<size_t>( allocation.width ) * allocation.height, 0 );
	for( int y = 0; y < allocation.height; ++y )
	{
		for( int x = 0; x < allocation.width; ++x )
		{
			const float amount = R_ComputeAO(
				allocation.surface, x, y, allocation.width, allocation.height );
			allocation.aoGeometryOcclusion[static_cast<size_t>( y ) * allocation.width + x] =
				static_cast<unsigned char>( std::max( 0.0f,
					std::min( 255.0f, amount * 255.0f + 0.5f ) ) );
		}
	}
}

static float R_GetCachedGeometryAOAmount( world_lightmap_allocation_t &allocation,
	int sampleX, int sampleY )
{
	if( dx_ao.value == 0.0f || dx_ao_cache.value == 0.0f )
		return 0.0f;

	if( !R_AOCacheValid( allocation ) )
	{
		R_BuildAOCache( allocation );
	}

	if( allocation.aoGeometryOcclusion.empty( ) || sampleX < 0 || sampleY < 0 ||
		sampleX >= allocation.width || sampleY >= allocation.height )
	{
		return 0.0f;
	}

	return static_cast<float>( allocation.aoGeometryOcclusion[
		static_cast<size_t>( sampleY ) * allocation.width + sampleX] ) / 255.0f;
}

static D3DCOLOR R_ApplyCachedAO( world_lightmap_allocation_t &allocation,
	int sampleX, int sampleY, D3DCOLOR pixel )
{
	if( dx_ao.value == 0.0f || !allocation.surface ||
		allocation.width <= 1 || allocation.height <= 1 )
	{
		return pixel;
	}

	const float strength = std::max( 0.0f, std::min( 1.0f, dx_ao_strength.value ) );
	if( strength <= 0.0f )
		return pixel;

	float occlusion = R_GetCachedGeometryAOAmount( allocation, sampleX, sampleY );
	const bool skipContrastForStyleUpdate = g_lightStyleUpdateActive &&
		dx_ao_contrast_style_updates.value == 0.0f;
	if( !skipContrastForStyleUpdate )
	{
		occlusion += R_ComputeContrastAO( allocation.surface, sampleX, sampleY,
			allocation.width, allocation.height, pixel );
	}

	const float minFactor = std::max( 0.10f,
		std::min( 1.0f, dx_ao_min_light.value ) );
	const float factor = std::max( minFactor,
		std::min( 1.0f, 1.0f - strength * occlusion ) );

	const int alpha = static_cast<int>( ( pixel >> 24 ) & 0xFF );
	const int red = static_cast<int>( ( pixel >> 16 ) & 0xFF );
	const int green = static_cast<int>( ( pixel >> 8 ) & 0xFF );
	const int blue = static_cast<int>( pixel & 0xFF );
	return D3DCOLOR_ARGB( alpha,
		static_cast<int>( red * factor + 0.5f ),
		static_cast<int>( green * factor + 0.5f ),
		static_cast<int>( blue * factor + 0.5f ) );
}

static D3DCOLOR R_ApplyLightmapAO( const msurface_t *surface,
	int sampleX, int sampleY, int width, int height, D3DCOLOR pixel )
{
	if( dx_ao.value == 0.0f || !surface || width <= 1 || height <= 1 )
		return pixel;

	const float strength = std::max( 0.0f, std::min( 1.0f, dx_ao_strength.value ) );
	if( strength <= 0.0f )
		return pixel;

	const float sampleSize = std::max( 1.0f, R_SurfaceSampleSize( surface ) );
	const float radiusSamples = std::max( 0.25f,
		std::min( 64.0f, dx_ao_radius.value / sampleSize ) );
	if( radiusSamples <= 0.25f )
		return pixel;

	float occlusion = 0.0f;

	occlusion += R_ComputeEdgeAO( surface, sampleX, sampleY,
		width, height, dx_ao_radius.value );

	if( dx_ao_surface_borders.value != 0.0f )
	{
		const float left = static_cast<float>( sampleX );
		const float right = static_cast<float>( width - 1 - sampleX );
		const float top = static_cast<float>( sampleY );
		const float bottom = static_cast<float>( height - 1 - sampleY );
		const float horizontalEdge = 1.0f - std::max( 0.0f,
			std::min( 1.0f, std::min( left, right ) / radiusSamples ) );
		const float verticalEdge = 1.0f - std::max( 0.0f,
			std::min( 1.0f, std::min( top, bottom ) / radiusSamples ) );

		const float edge = std::max( horizontalEdge, verticalEdge );
		const float corner = std::min( horizontalEdge, verticalEdge );
		const float edgeStrength = std::max( 0.0f,
			std::min( 2.0f, dx_ao_edge_strength.value ) );
		const float cornerStrength = std::max( 0.0f,
			std::min( 2.0f, dx_ao_corner_strength.value ) );
		occlusion += edge * edge * edgeStrength + corner * corner * cornerStrength;
	}

	occlusion += R_ComputeContrastAO(
		surface, sampleX, sampleY, width, height, pixel );

	const float minFactor = std::max( 0.10f,
		std::min( 1.0f, dx_ao_min_light.value ) );
	const float factor = std::max( minFactor,
		std::min( 1.0f, 1.0f - strength * occlusion ) );

	const int alpha = static_cast<int>( ( pixel >> 24 ) & 0xFF );
	const int red = static_cast<int>( ( pixel >> 16 ) & 0xFF );
	const int green = static_cast<int>( ( pixel >> 8 ) & 0xFF );
	const int blue = static_cast<int>( pixel & 0xFF );
	return D3DCOLOR_ARGB( alpha,
		static_cast<int>( red * factor + 0.5f ),
		static_cast<int>( green * factor + 0.5f ),
		static_cast<int>( blue * factor + 0.5f ) );
}

template<typename PixelWriter>
static void R_WriteSurfaceLightmapBlock( const msurface_t *surface, int width, int height,
	PixelWriter writePixel, world_lightmap_allocation_t *allocation = nullptr )
{
	if( !surface || !surface->samples || width <= 0 || height <= 0 )
		return;

	for( int y = -LIGHTMAP_GUTTER; y < height + LIGHTMAP_GUTTER; ++y )
	{
		const int sourceY = std::max( 0, std::min( height - 1, y ) );
		for( int x = -LIGHTMAP_GUTTER; x < width + LIGHTMAP_GUTTER; ++x )
		{
			const int sourceX = std::max( 0, std::min( width - 1, x ) );
			D3DCOLOR pixel = R_ReadSurfaceLightmapTexel(
				surface, sourceY * width + sourceX, width * height );
			if( allocation )
				pixel = R_ApplyCachedAO( *allocation,
					sourceX, sourceY, pixel );
			else
				pixel = R_ApplyLightmapAO( surface,
					sourceX, sourceY, width, height, pixel );
			writePixel( x, y, pixel );
		}
	}
}

static void R_CopySurfaceLightmapToAtlas( const msurface_t *surface, int width, int height,
	world_lightmap_atlas_t &atlas, int atlasX, int atlasY )
{
	if( atlas.pixels.empty( ) )
		return;

	R_UpdateLightmapGammaLut( );
	R_WriteSurfaceLightmapBlock( surface, width, height,
		[&]( int x, int y, D3DCOLOR pixel )
		{
			atlas.pixels[static_cast<size_t>( atlasY + y ) * atlas.size + atlasX + x] = pixel;
		} );
}

static bool R_LightmapDirtyRectsTouch( const RECT &a, const RECT &b )
{

	return a.left <= b.right + 1 && a.right + 1 >= b.left &&
		a.top <= b.bottom + 1 && a.bottom + 1 >= b.top;
}

static void R_MarkLightmapRectDirty( lightmap_dirty_atlas_t &dirty,
	int x, int y, int width, int height, const world_lightmap_atlas_t &atlas )
{
	RECT rect;
	rect.left = std::max( 0, x - LIGHTMAP_GUTTER );
	rect.top = std::max( 0, y - LIGHTMAP_GUTTER );
	rect.right = std::min( atlas.size, x + width + LIGHTMAP_GUTTER );
	rect.bottom = std::min( atlas.size, y + height + LIGHTMAP_GUTTER );

	if( rect.left >= rect.right || rect.top >= rect.bottom )
		return;

	for( size_t i = 0; i < dirty.rects.size( ); )
	{
		if( !R_LightmapDirtyRectsTouch( dirty.rects[i], rect ) )
		{
			++i;
			continue;
		}
		rect.left = std::min( rect.left, dirty.rects[i].left );
		rect.top = std::min( rect.top, dirty.rects[i].top );
		rect.right = std::max( rect.right, dirty.rects[i].right );
		rect.bottom = std::max( rect.bottom, dirty.rects[i].bottom );
		dirty.rects.erase( dirty.rects.begin( ) + i );
		i = 0;
	}

	if( dirty.rects.size( ) < LIGHTMAP_MAX_DIRTY_RECTS )
	{
		dirty.rects.push_back( rect );
		return;
	}

	RECT &fallback = dirty.rects[0];
	fallback.left = std::min( fallback.left, rect.left );
	fallback.top = std::min( fallback.top, rect.top );
	fallback.right = std::max( fallback.right, rect.right );
	fallback.bottom = std::max( fallback.bottom, rect.bottom );
	for( size_t i = 1; i < dirty.rects.size( ); ++i )
	{
		fallback.left = std::min( fallback.left, dirty.rects[i].left );
		fallback.top = std::min( fallback.top, dirty.rects[i].top );
		fallback.right = std::max( fallback.right, dirty.rects[i].right );
		fallback.bottom = std::max( fallback.bottom, dirty.rects[i].bottom );
	}
	dirty.rects.resize( 1 );
}

static void R_MarkLightmapAtlasDirty( lightmap_dirty_atlas_t &dirty,
	const world_lightmap_allocation_t &allocation,
	const world_lightmap_atlas_t &atlas )
{
	R_MarkLightmapRectDirty( dirty, allocation.x, allocation.y,
		allocation.width, allocation.height, atlas );
}

static bool R_RebuildLightmapBlock(
	world_lightmap_allocation_t &allocation, world_lightmap_atlas_t &atlas )
{
	if( !allocation.surface || atlas.pixels.size( ) !=
		static_cast<size_t>( atlas.size ) * atlas.size )
	{
		return false;
	}

	R_WriteSurfaceLightmapBlock( allocation.surface, allocation.width, allocation.height,
		[&]( int x, int y, D3DCOLOR pixel )
		{
			const int dstX = allocation.x + x;
			const int dstY = allocation.y + y;
			if( dstX >= 0 && dstX < atlas.size &&
				dstY >= 0 && dstY < atlas.size )
			{
				atlas.pixels[static_cast<size_t>( dstY ) * atlas.size + dstX] = pixel;
			}
		}, &allocation );
	return true;
}

static bool R_RebuildLightmap( world_lightmap_allocation_t &allocation,
	world_lightmap_atlas_t &atlas )
{
	R_UpdateLightmapGammaLut( );
	return R_RebuildLightmapBlock( allocation, atlas );
}

static bool R_UploadDirtyLightmapAtlas( world_lightmap_atlas_t &atlas,
	const lightmap_dirty_atlas_t &dirty, bool allowRectMerge = true )
{
	if( dirty.rects.empty( ) || !atlas.texture || atlas.pixels.size( ) !=
		static_cast<size_t>( atlas.size ) * atlas.size )
	{
		return false;
	}

	const bool forceRebind = dx_lightmap_rebind.value != 0.0f;
	if( forceRebind )
		DX_BindRawTexture( 1, nullptr );

	RECT merged{};
	const bool mergeRects = allowRectMerge &&
		dx_lightmap_merge_uploads.value != 0.0f && dirty.rects.size( ) > 1;
	if( mergeRects )
	{
		merged.left = static_cast<LONG>( atlas.size );
		merged.top = static_cast<LONG>( atlas.size );
		for( const RECT &rect : dirty.rects )
		{
			const LONG left = std::max<LONG>( 0, rect.left );
			const LONG top = std::max<LONG>( 0, rect.top );
			const LONG right = std::min<LONG>( static_cast<LONG>( atlas.size ), rect.right );
			const LONG bottom = std::min<LONG>( static_cast<LONG>( atlas.size ), rect.bottom );
			if( right <= left || bottom <= top )
				continue;
			merged.left = std::min( merged.left, left );
			merged.top = std::min( merged.top, top );
			merged.right = std::max( merged.right, right );
			merged.bottom = std::max( merged.bottom, bottom );
		}
		if( merged.right <= merged.left || merged.bottom <= merged.top )
			return false;
	}

	D3DLOCKED_RECT locked;
	if( FAILED( atlas.texture->LockRect(
		0, &locked, nullptr, D3DLOCK_NO_DIRTY_UPDATE ) ) )
	{
		return false;
	}

	auto CopyRect = [&]( const RECT &rect )
		{
			const int left = std::max( 0L, rect.left );
			const int top = std::max( 0L, rect.top );
			const int right = std::min( static_cast<LONG>( atlas.size ), rect.right );
			const int bottom = std::min( static_cast<LONG>( atlas.size ), rect.bottom );
			const int rowPixels = right - left;
			if( rowPixels <= 0 || bottom <= top )
				return;
			for( int y = top; y < bottom; ++y )
			{
				auto *dst = reinterpret_cast<D3DCOLOR *>(
					static_cast<byte *>( locked.pBits ) +
					static_cast<size_t>( y ) * locked.Pitch ) + left;
				const D3DCOLOR *src = atlas.pixels.data( ) +
					static_cast<size_t>( y ) * atlas.size + left;
				memcpy( dst, src,
					static_cast<size_t>( rowPixels ) * sizeof( D3DCOLOR ) );
			}
		};

	if( mergeRects )
		CopyRect( merged );
	else
		for( const RECT &rect : dirty.rects ) CopyRect( rect );

	if( FAILED( atlas.texture->UnlockRect( 0 ) ) )
		return false;

	if( mergeRects )
	{
		if( FAILED( atlas.texture->AddDirtyRect( &merged ) ) )
			return false;
	}
	else
	{
		for( const RECT &rect : dirty.rects )
		{
			if( FAILED( atlas.texture->AddDirtyRect( &rect ) ) )
				return false;
		}
	}

	if( forceRebind )
		DX_InvalidateTextureUnit( 1 );
	return true;
}

static bool R_LightmapNeedsUpdate( const world_lightmap_allocation_t &allocation )
{
	for( int i = 0; i < DX_LIGHTMAP_STYLE_SLOTS; ++i )
	{
		const byte style = allocation.styles[i];
		if( style == 255 )
			break;
		if( allocation.lastStyleValues[i] != R_LightStyleValue( style ) )
			return true;
	}
	return false;
}

static void R_StoreLightStyles( world_lightmap_allocation_t &allocation )
{
	for( int i = 0; i < DX_LIGHTMAP_STYLE_SLOTS; ++i )
	{
		const byte style = allocation.styles[i];
		allocation.lastStyleValues[i] = style == 255 ? 0 : R_LightStyleValue( style );
	}
}

static void R_FlushDirtyLightmapAtlases( world_lightmap_store_t &store,
	const std::vector<lightmap_dirty_atlas_t> &dirtyAtlases,
	const std::vector<size_t> &rebuiltAllocations )
{
	if( store.atlases.empty( ) || dirtyAtlases.empty( ) ||
		rebuiltAllocations.empty( ) )
	{
		return;
	}

	std::vector<bool> uploaded( store.atlases.size( ), false );
	const size_t atlasCount = std::min( store.atlases.size( ), dirtyAtlases.size( ) );
	for( size_t atlasIndex = 0; atlasIndex < atlasCount; ++atlasIndex )
	{
		if( dirtyAtlases[atlasIndex].rects.empty( ) )
			continue;

		uploaded[atlasIndex] = R_UploadDirtyLightmapAtlas(
			store.atlases[atlasIndex], dirtyAtlases[atlasIndex] );
		if( uploaded[atlasIndex] )
			DX_ProfileEvent( DX_PROFILE_EVENT_LIGHTMAP_ATLAS_UPLOAD, 1 );
	}

	for( const size_t allocationIndex : rebuiltAllocations )
	{
		if( allocationIndex >= store.allocations.size( ) )
			continue;

		world_lightmap_allocation_t &allocation = store.allocations[allocationIndex];
		const int atlasIndex = allocation.atlasIndex;
		if( atlasIndex >= 0 && atlasIndex < static_cast<int>( uploaded.size( ) ) &&
			uploaded[atlasIndex] )
		{
			R_StoreLightStyles( allocation );
		}
	}
}

static void R_QueueLightmapAllocation( size_t allocationIndex )
{
	if( allocationIndex >= g_mapLightmaps.allocations.size( ) )
		return;

	world_lightmap_allocation_t &allocation =
		g_mapLightmaps.allocations[allocationIndex];
	if( allocation.queuedForUpdate )
		return;

	allocation.queuedForUpdate = true;
	g_pendingLightmapAllocations.push_back( allocationIndex );
}

static size_t R_LightmapTexelCost(
	const world_lightmap_allocation_t &allocation )
{
	const size_t packedWidth = static_cast<size_t>( std::max( 0,
		allocation.width + LIGHTMAP_GUTTER * 2 ) );
	const size_t packedHeight = static_cast<size_t>( std::max( 0,
		allocation.height + LIGHTMAP_GUTTER * 2 ) );
	return packedWidth * packedHeight;
}

static float R_LightmapUpdateFrameScale( )
{
	if( dx_lightmap_update_time_scaled.value == 0.0f )
		return 1.0f;

	float now = -1.0f;
	if( gp_cl )
		now = static_cast<float>( gp_cl->time );

	float frameTime = 0.0f;
	if( g_lastLightmapUpdateClock >= 0.0f && now > g_lastLightmapUpdateClock )
		frameTime = now - g_lastLightmapUpdateClock;
	g_lastLightmapUpdateClock = now;

	if( frameTime <= 0.0f )
		frameTime = std::max( 0.0f, g_frametime );
	if( frameTime <= 0.0f )
		return 1.0f;

	const float referenceFps = std::max( 60.0f,
		std::min( 2000.0f, dx_lightmap_ref_fps.value ) );
	const float maxScale = std::max( 1.0f,
		std::min( 64.0f, dx_lightmap_max_scale.value ) );

	return std::max( 1.0f, std::min( maxScale, frameTime * referenceFps ) );
}

static size_t R_ScaleLightmapUpdateBudget( size_t baseBudget, float scale )
{
	if( baseBudget == 0 )
		return 0;
	const double scaled = static_cast<double>( baseBudget ) *
		static_cast<double>( std::max( 1.0f, scale ) );
	const double maxValue = static_cast<double>( static_cast<size_t>( -1 ) );
	return static_cast<size_t>( std::min( scaled, maxValue ) );
}

void R_SetLightStyles( const int *values, int count )
{
	if( !values || count <= 0 )
		return;

	int changedStyles[DX_LIGHTSTYLE_COUNT];
	int changedCount = 0;
	const int limit = std::min( count, DX_LIGHTSTYLE_COUNT );
	for( int i = 0; i < limit; ++i )
	{
		const int value = std::max( 0, std::min( values[i], 1024 ) );
		if( g_lightStyleValues[i] != value )
		{
			g_lightStyleValues[i] = value;
			changedStyles[changedCount++] = i;
		}
	}

	if( changedCount == 0 )
		return;

	++g_lightStyleRevision;
	if( g_mapLightmaps.allocations.empty( ) )
		return;

	for( int changedIndex = 0; changedIndex < changedCount; ++changedIndex )
	{
		const int style = changedStyles[changedIndex];
		for( const size_t allocationIndex :
		g_mapLightmaps.allocationsByStyle[style] )
		{
			R_QueueLightmapAllocation( allocationIndex );
		}
	}
}

void R_UpdateLightmaps( )
{

	if( r_nolm.value != 0.0f )
		return;

	if( g_pendingLightmapCursor >= g_pendingLightmapAllocations.size( ) )
	{
		g_pendingLightmapAllocations.clear( );
		g_pendingLightmapCursor = 0;
		g_nextLightmapUpdateTime = -99999.0f;
		g_lastLightmapUpdateClock = -1.0f;
		return;
	}

	const float updateHz = std::max( 0.0f, std::min( 240.0f, dx_lightmap_update_hz.value ) );
	if( updateHz > 0.0f && gp_cl )
	{
		const float now = static_cast<float>( gp_cl->time );
		if( now < g_nextLightmapUpdateTime )
			return;
		g_nextLightmapUpdateTime = now + 1.0f / updateHz;
	}

	const float frameBudgetScale = R_LightmapUpdateFrameScale( );
	const size_t baseTexelBudget = static_cast<size_t>( std::max( 1024,
		static_cast<int>( dx_lightmap_update_budget.value ) ) );
	const size_t budget = std::max<size_t>( 1024,
		R_ScaleLightmapUpdateBudget( baseTexelBudget, frameBudgetScale ) );
	const size_t baseAllocationBudget = static_cast<size_t>( std::max( 0,
		static_cast<int>( dx_lightmap_alloc_budget.value ) ) );
	size_t allocationBudget = R_ScaleLightmapUpdateBudget( baseAllocationBudget,
		frameBudgetScale );
	if( allocationBudget > 0 )
	{
		const size_t minAllocationBudget = static_cast<size_t>( std::max( 0,
			static_cast<int>( dx_lightmap_min_alloc_budget.value ) ) );
		allocationBudget = std::max( allocationBudget, minAllocationBudget );
	}

	std::vector<size_t> selectedAllocations;
	selectedAllocations.reserve( std::min<size_t>( 512,
		g_pendingLightmapAllocations.size( ) - g_pendingLightmapCursor ) );

	size_t texelsSelected = 0;
	while( g_pendingLightmapCursor <
		g_pendingLightmapAllocations.size( ) )
	{
		const size_t allocationIndex = g_pendingLightmapAllocations[
			g_pendingLightmapCursor];
		++g_pendingLightmapCursor;

		if( allocationIndex >= g_mapLightmaps.allocations.size( ) )
			continue;

		world_lightmap_allocation_t &allocation =
			g_mapLightmaps.allocations[allocationIndex];
		allocation.queuedForUpdate = false;

		if( !R_LightmapNeedsUpdate( allocation ) ||
			allocation.atlasIndex < 0 || allocation.atlasIndex >=
				static_cast<int>( g_mapLightmaps.atlases.size( ) ) )
		{
			continue;
		}

		const size_t cost = R_LightmapTexelCost( allocation );
		if( !selectedAllocations.empty( ) && texelsSelected + cost > budget )
		{
			R_QueueLightmapAllocation( allocationIndex );
			break;
		}
		if( allocationBudget > 0 && selectedAllocations.size( ) >= allocationBudget )
		{
			R_QueueLightmapAllocation( allocationIndex );
			break;
		}

		selectedAllocations.push_back( allocationIndex );
		texelsSelected += cost;
	}

	if( !selectedAllocations.empty( ) )
	{
		R_UpdateLightmapGammaLut( );

		std::vector<unsigned char> rebuildOk( selectedAllocations.size( ), 0 );
		const bool previousStyleUpdateFlag = g_lightStyleUpdateActive;
		g_lightStyleUpdateActive = true;
		DX_LightmapParallelFor( static_cast<int>( selectedAllocations.size( ) ), 16,
			[&]( int begin, int end )
			{
				for( int i = begin; i < end; ++i )
				{
					const size_t allocationIndex = selectedAllocations[static_cast<size_t>( i )];
					if( allocationIndex >= g_mapLightmaps.allocations.size( ) )
						continue;

					world_lightmap_allocation_t &allocation =
						g_mapLightmaps.allocations[allocationIndex];
					if( allocation.atlasIndex < 0 || allocation.atlasIndex >=
						static_cast<int>( g_mapLightmaps.atlases.size( ) ) )
					{
						continue;
					}

					if( R_RebuildLightmapBlock( allocation,
						g_mapLightmaps.atlases[allocation.atlasIndex] ) )
					{
						rebuildOk[static_cast<size_t>( i )] = 1;
					}
				}
			} );
		g_lightStyleUpdateActive = previousStyleUpdateFlag;

		std::vector<lightmap_dirty_atlas_t> dirtyAtlases(
			g_mapLightmaps.atlases.size( ) );
		std::vector<size_t> rebuiltAllocations;
		rebuiltAllocations.reserve( selectedAllocations.size( ) );

		for( size_t i = 0; i < selectedAllocations.size( ); ++i )
		{
			if( !rebuildOk[i] )
				continue;
			const size_t allocationIndex = selectedAllocations[i];
			if( allocationIndex >= g_mapLightmaps.allocations.size( ) )
				continue;

			world_lightmap_allocation_t &allocation =
				g_mapLightmaps.allocations[allocationIndex];
			if( allocation.atlasIndex < 0 || allocation.atlasIndex >=
				static_cast<int>( dirtyAtlases.size( ) ) )
			{
				continue;
			}

			R_MarkLightmapAtlasDirty( dirtyAtlases[allocation.atlasIndex],
				allocation, g_mapLightmaps.atlases[allocation.atlasIndex] );
			rebuiltAllocations.push_back( allocationIndex );
		}

		if( !rebuiltAllocations.empty( ) )
		{
			DX_ProfileEvent( DX_PROFILE_EVENT_LIGHTMAP_ALLOC_UPDATED,
				static_cast<unsigned int>( rebuiltAllocations.size( ) ) );
			R_FlushDirtyLightmapAtlases( g_mapLightmaps, dirtyAtlases,
				rebuiltAllocations );

			for( const size_t allocationIndex : rebuiltAllocations )
			{
				if( allocationIndex < g_mapLightmaps.allocations.size( ) &&
					R_LightmapNeedsUpdate(
						g_mapLightmaps.allocations[allocationIndex] ) )
				{
					R_QueueLightmapAllocation( allocationIndex );
				}
			}
		}
	}

	if( g_pendingLightmapCursor >=
		g_pendingLightmapAllocations.size( ) )
	{
		g_pendingLightmapAllocations.clear( );
		g_pendingLightmapCursor = 0;
		g_nextLightmapUpdateTime = -99999.0f;
		g_lastLightmapUpdateClock = -1.0f;
	}
}

void R_RefreshLightmaps( )
{
	std::vector<lightmap_dirty_atlas_t> dirtyAtlases( g_mapLightmaps.atlases.size( ) );
	std::vector<size_t> rebuiltAllocations;
	rebuiltAllocations.reserve( g_mapLightmaps.allocations.size( ) );

	for( size_t allocationIndex = 0;
		allocationIndex < g_mapLightmaps.allocations.size( ); ++allocationIndex )
	{
		world_lightmap_allocation_t &allocation = g_mapLightmaps.allocations[allocationIndex];
		if( allocation.atlasIndex < 0 ||
			allocation.atlasIndex >= static_cast<int>( g_mapLightmaps.atlases.size( ) ) )
		{
			continue;
		}

		if( R_RebuildLightmap(
			allocation, g_mapLightmaps.atlases[allocation.atlasIndex] ) )
		{
			R_MarkLightmapAtlasDirty( dirtyAtlases[allocation.atlasIndex], allocation,
				g_mapLightmaps.atlases[allocation.atlasIndex] );
			rebuiltAllocations.push_back( allocationIndex );
		}
	}

	R_FlushDirtyLightmapAtlases( g_mapLightmaps, dirtyAtlases, rebuiltAllocations );
}

int R_WorldBatchCellSize( )
{
	const int requested = static_cast<int>( dx_world_batch_cell.value );
	if( requested <= 0 )
		return 0;

	return std::max( 256, std::min( 4096, requested ) );
}

static bool R_UploadLightmapAtlas( world_lightmap_atlas_t &atlas )
{
	if( atlas.pixels.empty( ) )
		return false;

	HRESULT result = device->CreateTexture(
		atlas.size, atlas.size, 1, 0,
		D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &atlas.texture, nullptr );
	if( FAILED( result ) || !atlas.texture )
		return false;

	D3DLOCKED_RECT locked;
	result = atlas.texture->LockRect( 0, &locked, nullptr, 0 );
	if( FAILED( result ) )
	{
		atlas.texture->Release( );
		atlas.texture = nullptr;
		return false;
	}

	for( int y = 0; y < atlas.size; ++y )
	{
		auto *row = reinterpret_cast<D3DCOLOR *>(
			static_cast<byte *>( locked.pBits ) + y * locked.Pitch );
		memcpy( row, atlas.pixels.data( ) + static_cast<size_t>( y ) * atlas.size,
			static_cast<size_t>( atlas.size ) * sizeof( D3DCOLOR ) );
	}

	atlas.texture->UnlockRect( 0 );

	return true;
}

bool R_GetBrushSurfaceRange( const model_t *mod, int &outFirstSurface,
	int &outSurfaceCount )
{
	outFirstSurface = 0;
	outSurfaceCount = 0;

	if( !mod || mod->type != mod_brush || !mod->surfaces || mod->numsurfaces <= 0 )
		return false;

	int firstSurface = mod->firstmodelsurface;
	int surfaceCount = mod->nummodelsurfaces;

	if( mod == g_worldmodel &&
		( firstSurface < 0 || surfaceCount <= 0 ||
			firstSurface > mod->numsurfaces ||
			surfaceCount > mod->numsurfaces - firstSurface ) )
	{
		firstSurface = 0;
		surfaceCount = mod->numsurfaces;
	}

	if( firstSurface < 0 || surfaceCount <= 0 ||
		firstSurface >= mod->numsurfaces ||
		surfaceCount > mod->numsurfaces - firstSurface )
	{
		gEngfuncs.Con_DPrintf(
			"D3D9: refusing invalid brush surface range for %s (%d + %d, max %d)\n",
			mod->name, firstSurface, surfaceCount, mod->numsurfaces );
		return false;
	}

	outFirstSurface = firstSurface;
	outSurfaceCount = surfaceCount;
	return true;
}

static int R_GetLightmapAlloc( msurface_t *surface,
	bool uploadImmediately, bool allowDynamicOverlay )
{
	if( !surface )
		return -1;

	const auto existing = g_mapLightmaps.allocationBySurface.find( surface );
	if( existing != g_mapLightmaps.allocationBySurface.end( ) )
		return static_cast<int>( existing->second );

	int width = 0;
	int height = 0;
	if( !surface->texinfo || !surface->texinfo->texture ||
		( surface->flags & SURF_DRAWSKY ) ||
		!R_GetSurfaceLightmapSize( surface, width, height ) )
	{
		return -1;
	}

	const bool dynamic = R_HasStyleLightmap( surface );

	const bool materialLocal = dynamic;
	texture_t *materialGroup = surface->texinfo->texture;
	int atlasIndex = -1;
	int atlasX = 0;
	int atlasY = 0;
	if( !R_AllocateAtlasRect( g_mapLightmaps, width, height, dynamic,
		materialGroup, materialLocal, atlasIndex, atlasX, atlasY ) )
	{
		return -1;
	}

	R_CopySurfaceLightmapToAtlas( surface, width, height,
		g_mapLightmaps.atlases[atlasIndex], atlasX, atlasY );

	int overlayAtlasIndex = -1;
	int overlayX = 0;
	int overlayY = 0;
	if( allowDynamicOverlay )
	{
		R_AllocateDynamicOverlayRect( g_mapLightmaps, width, height,
			overlayAtlasIndex, overlayX, overlayY );
	}

	world_lightmap_allocation_t allocation;
	allocation.surface = surface;
	allocation.atlasIndex = atlasIndex;
	allocation.x = atlasX;
	allocation.y = atlasY;
	allocation.width = width;
	allocation.height = height;
	allocation.overlayAtlasIndex = overlayAtlasIndex;
	allocation.overlayX = overlayX;
	allocation.overlayY = overlayY;
	R_BuildSurfaceBounds( g_worldmodel ? g_worldmodel : nullptr, surface,
		allocation.mins, allocation.maxs, allocation.boundsValid );
	for( int i = 0; i < DX_LIGHTMAP_STYLE_SLOTS; ++i )
		allocation.styles[i] = surface->styles[i];
	R_StoreLightStyles( allocation );

	const size_t allocationIndex = g_mapLightmaps.allocations.size( );
	g_mapLightmaps.allocations.push_back( std::move( allocation ) );
	g_mapLightmaps.allocationBySurface.emplace( surface, allocationIndex );

	for( int i = 0; i < DX_LIGHTMAP_STYLE_SLOTS; ++i )
	{
		const byte style = g_mapLightmaps.allocations[allocationIndex].styles[i];
		if( style == 255 )
			break;
		g_mapLightmaps.allocationsByStyle[style].push_back( allocationIndex );
	}

	world_lightmap_allocation_t &stored =
		g_mapLightmaps.allocations[allocationIndex];
	if( uploadImmediately )
	{
		if( overlayAtlasIndex >= 0 && overlayAtlasIndex < static_cast<int>(
			g_mapLightmaps.dynamicOverlayAtlases.size( ) ) )
		{
			world_lightmap_atlas_t &overlayAtlas =
				g_mapLightmaps.dynamicOverlayAtlases[overlayAtlasIndex];
			if( !overlayAtlas.texture )
				R_UploadLightmapAtlas( overlayAtlas );
		}

		world_lightmap_atlas_t &atlas = g_mapLightmaps.atlases[atlasIndex];
		if( !atlas.texture )
		{
			R_UploadLightmapAtlas( atlas );
		}
		else
		{
			std::vector<lightmap_dirty_atlas_t> dirtyAtlases(
				g_mapLightmaps.atlases.size( ) );
			R_MarkLightmapAtlasDirty( dirtyAtlases[atlasIndex], stored, atlas );
			R_UploadDirtyLightmapAtlas( atlas, dirtyAtlases[atlasIndex] );
		}
	}

	return static_cast<int>( allocationIndex );
}

void R_BuildMapLightmaps( const model_t *mod )
{
	R_DestroyWorldLightmapStore( g_mapLightmaps );
	g_pendingLightmapAllocations.clear( );
	g_pendingLightmapCursor = 0;
	g_nextLightmapUpdateTime = -99999.0f;
	g_lastLightmapUpdateClock = -1.0f;
	g_dlightTouchedAllocs.clear( );
	g_dlightOverlayActive.clear( );
	g_dynamicLightmapSignature = 0;

	if( !mod || mod->type != mod_brush || !mod->lightdata )
		return;

	g_mapLightmaps.allocations.reserve( mod->numsurfaces );
	g_mapLightmaps.allocationBySurface.reserve( mod->numsurfaces );

	R_PlanMaterialLightmaps( g_mapLightmaps, mod );

	struct lightmap_surface_t
	{
		msurface_t *surface = nullptr;
		int packedWidth = 0;
		int packedHeight = 0;
	};

	std::vector<lightmap_surface_t> buildSurfaces;
	buildSurfaces.reserve( mod->numsurfaces );

	int overlayFirstSurface = 0;
	int overlaySurfaceCount = mod->numsurfaces;
	if( !R_GetBrushSurfaceRange( mod, overlayFirstSurface, overlaySurfaceCount ) )
	{
		overlayFirstSurface = 0;
		overlaySurfaceCount = mod->numsurfaces;
	}

	for( int surfaceIndex = 0; surfaceIndex < mod->numsurfaces; ++surfaceIndex )
	{
		msurface_t *surface = &mod->surfaces[surfaceIndex];
		if( !surface->texinfo || !surface->texinfo->texture ||
			( surface->flags & SURF_DRAWSKY ) )
		{
			continue;
		}

		int width = 0;
		int height = 0;
		if( !R_GetSurfaceLightmapSize( surface, width, height ) )
			continue;

		lightmap_surface_t entry;
		entry.surface = surface;
		entry.packedWidth = width + LIGHTMAP_GUTTER * 2;
		entry.packedHeight = height + LIGHTMAP_GUTTER * 2;
		buildSurfaces.push_back( entry );
	}

	dxfast::HashMap<texture_t *, size_t> materialReservedArea;
	materialReservedArea.reserve( buildSurfaces.size( ) );
	for( const lightmap_surface_t &entry : buildSurfaces )
	{
		texture_t *material = entry.surface && entry.surface->texinfo
			? entry.surface->texinfo->texture : nullptr;
		materialReservedArea[material] += static_cast<size_t>( entry.packedWidth ) *
			static_cast<size_t>( entry.packedHeight );
	}

	std::sort( buildSurfaces.begin( ), buildSurfaces.end( ),
		[&materialReservedArea]( const lightmap_surface_t &a,
			const lightmap_surface_t &b )
		{
			texture_t *aMaterial = a.surface && a.surface->texinfo
				? a.surface->texinfo->texture : nullptr;
			texture_t *bMaterial = b.surface && b.surface->texinfo
				? b.surface->texinfo->texture : nullptr;
			if( aMaterial != bMaterial )
			{
				const size_t aGroupArea = materialReservedArea[aMaterial];
				const size_t bGroupArea = materialReservedArea[bMaterial];
				if( aGroupArea != bGroupArea )
					return aGroupArea > bGroupArea;
				return std::less<texture_t *>{}( aMaterial, bMaterial );
			}

			const int aLongest = std::max( a.packedWidth, a.packedHeight );
			const int bLongest = std::max( b.packedWidth, b.packedHeight );
			if( aLongest != bLongest )
				return aLongest > bLongest;
			const int aArea = a.packedWidth * a.packedHeight;
			const int bArea = b.packedWidth * b.packedHeight;
			if( aArea != bArea )
				return aArea > bArea;
			if( a.packedHeight != b.packedHeight )
				return a.packedHeight > b.packedHeight;
			return a.packedWidth > b.packedWidth;
		} );

	for( const lightmap_surface_t &entry : buildSurfaces )
	{
		const int surfaceIndex = static_cast<int>( entry.surface - mod->surfaces );
		const bool allowOverlay = surfaceIndex >= overlayFirstSurface &&
			surfaceIndex < overlayFirstSurface + overlaySurfaceCount;
		R_GetLightmapAlloc( entry.surface, false, allowOverlay );
	}

	R_ValidateLightmapAtlases( g_mapLightmaps, false );
	R_ValidateLightmapAtlases( g_mapLightmaps, true );

	for( auto &atlas : g_mapLightmaps.atlases )
		R_UploadLightmapAtlas( atlas );
	for( auto &atlas : g_mapLightmaps.dynamicOverlayAtlases )
		R_UploadLightmapAtlas( atlas );
}

static bool R_LoadLightmappedBrushModel( const model_t *mod,
	world_model_draw_t &out )
{
	R_FreeWorldModel( out );

	int firstSurface = 0;
	int surfaceCount = 0;
	if( !R_GetBrushSurfaceRange( mod, firstSurface, surfaceCount ) )
		return false;

	out.sourceModel = mod;
	out.pvsApplicable = R_WorldPVSAvailable( mod );

	surface_pvs_leafs_t surfaceLeafs;
	if( out.pvsApplicable )
		R_BuildSurfacePVSLeafMap( mod, surfaceLeafs );

	for( int surfaceIndex = firstSurface;
		surfaceIndex < firstSurface + surfaceCount; ++surfaceIndex )
	{
		msurface_t *surface = &mod->surfaces[surfaceIndex];
		lightmap_vertex_t surfaceVertices[64];
		int vertexCount = 0;

		if( !surface->texinfo || !surface->texinfo->texture ||
			( surface->flags & SURF_DRAWSKY ) )
		{
			continue;
		}

		if( surface->flags & SURF_DRAWTURB )
		{
			if( out.pvsApplicable &&
				surfaceIndex >= 0 &&
				surfaceIndex < static_cast<int>( surfaceLeafs.size( ) ) )
			{
				std::vector<int> portalLeaves =
					surfaceLeafs[static_cast<size_t>( surfaceIndex )];

				R_AddWaterLeafSamples(
					mod, surface, portalLeaves );

				std::sort( portalLeaves.begin( ), portalLeaves.end( ) );
				portalLeaves.erase(
					std::unique( portalLeaves.begin( ), portalLeaves.end( ) ),
					portalLeaves.end( ) );

				if( portalLeaves.size( ) >= 2 )
					out.waterPvsPortals.push_back( portalLeaves );
			}

			R_AddWaterSurfaceToBatch(
				mod, surface, out, surfaceIndex, surfaceLeafs );
			continue;
		}

		R_BuildSurfaceLightmapVerts( mod, surface, surfaceVertices, vertexCount );
		if( vertexCount < 3 )
			continue;

		lightmap_vertex_t overlayVertices[64];
		memcpy( overlayVertices, surfaceVertices,
			static_cast<size_t>( vertexCount ) * sizeof( lightmap_vertex_t ) );

		int atlasIndex = -1;
		const int allocationIndex = R_GetLightmapAlloc(
			surface, true, mod == g_worldmodel );
		if( allocationIndex >= 0 &&
			allocationIndex < static_cast<int>( g_mapLightmaps.allocations.size( ) ) )
		{
			const world_lightmap_allocation_t &allocation =
				g_mapLightmaps.allocations[allocationIndex];
			atlasIndex = allocation.atlasIndex;
			if( atlasIndex >= 0 &&
				atlasIndex < static_cast<int>( g_mapLightmaps.atlases.size( ) ) )
			{
				const float atlasSize = static_cast<float>(
					g_mapLightmaps.atlases[atlasIndex].size );
				for( int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex )
				{
					lightmap_vertex_t &vertex = surfaceVertices[vertexIndex];

					vertex.lu = ( static_cast<float>( allocation.x ) +
						vertex.lu + 0.5f ) / atlasSize;
					vertex.lv = ( static_cast<float>( allocation.y ) +
						vertex.lv + 0.5f ) / atlasSize;
				}
			}

			if( allocation.overlayAtlasIndex >= 0 &&
				allocation.overlayAtlasIndex < static_cast<int>(
					g_mapLightmaps.dynamicOverlayAtlases.size( ) ) )
			{
				const float overlaySize = static_cast<float>(
					g_mapLightmaps.dynamicOverlayAtlases[
						allocation.overlayAtlasIndex].size );
				for( int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex )
				{
					lightmap_vertex_t &vertex = overlayVertices[vertexIndex];
					vertex.lu = ( static_cast<float>( allocation.overlayX ) +
						vertex.lu + 0.5f ) / overlaySize;
					vertex.lv = ( static_cast<float>( allocation.overlayY ) +
						vertex.lv + 0.5f ) / overlaySize;
				}
			}
		}
		else
		{

			for( int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex )
			{
				surfaceVertices[vertexIndex].lu = 0.0f;
				surfaceVertices[vertexIndex].lv = 0.0f;
			}
		}

		int cellX = 0, cellY = 0, cellZ = 0;
		if( R_WorldMegabatchEnabled( mod ) )
		{

			cellX = cellY = cellZ = 0;
		}
		else
		{
			R_CellForLightmapVerts( surfaceVertices, vertexCount, cellX, cellY, cellZ );
		}
		const bool conveyorSurface =
			( surface->flags & SURF_CONVEYOR ) != 0;
		world_material_batch_t &batch =
			R_FindWorldBatch( out, surface->texinfo->texture, atlasIndex,
				conveyorSurface, cellX, cellY, cellZ );
		R_AddSurfacePVSLeafs( batch.pvsLeafs, batch.pvsAlwaysVisible, surfaceLeafs, surfaceIndex );
		R_ExpandWorldBatchBounds( batch, surfaceVertices, vertexCount );
		const DWORD baseVertex = static_cast<DWORD>( batch.vertices.size( ) );

		batch.vertices.insert( batch.vertices.end( ),
			surfaceVertices, surfaceVertices + vertexCount );
		for( int vertexIndex = 2; vertexIndex < vertexCount; ++vertexIndex )
		{
			batch.indices.push_back( baseVertex );
			batch.indices.push_back( baseVertex + vertexIndex - 1 );
			batch.indices.push_back( baseVertex + vertexIndex );
		}

		if( mod == g_worldmodel &&
			allocationIndex >= 0 && allocationIndex < static_cast<int>(
				g_mapLightmaps.allocations.size( ) ) )
		{
			const world_lightmap_allocation_t &allocation =
				g_mapLightmaps.allocations[allocationIndex];
			if( allocation.overlayAtlasIndex >= 0 )
			{
				world_dynamic_overlay_batch_t &overlayBatch =
					R_FindDynamicOverlayBatch( out, surface->texinfo->texture,
						allocation.overlayAtlasIndex, conveyorSurface,
						cellX, cellY, cellZ );
				R_AddSurfacePVSLeafs( overlayBatch.pvsLeafs, overlayBatch.pvsAlwaysVisible, surfaceLeafs, surfaceIndex );
				R_ExpandOverlayBounds( overlayBatch,
					overlayVertices, vertexCount );

				const DWORD overlayBaseVertex = static_cast<DWORD>(
					overlayBatch.vertices.size( ) );
				const UINT overlayFirstIndex = static_cast<UINT>(
					overlayBatch.indices.size( ) );

				overlayBatch.vertices.insert( overlayBatch.vertices.end( ),
					overlayVertices, overlayVertices + vertexCount );
				for( int vertexIndex = 2; vertexIndex < vertexCount; ++vertexIndex )
				{
					overlayBatch.indices.push_back( overlayBaseVertex );
					overlayBatch.indices.push_back( overlayBaseVertex + vertexIndex - 1 );
					overlayBatch.indices.push_back( overlayBaseVertex + vertexIndex );
				}

				world_dynamic_overlay_range_t range;
				range.allocationIndex = static_cast<size_t>( allocationIndex );
				range.firstIndex = overlayFirstIndex;
				range.indexCount = static_cast<UINT>(
					overlayBatch.indices.size( ) ) - overlayFirstIndex;
				overlayBatch.ranges.push_back( range );
			}
		}
	}

	if( !out.waterPvsPortals.empty( ) )
	{
		for( std::vector<int> &portal : out.waterPvsPortals )
		{
			std::sort( portal.begin( ), portal.end( ) );
			portal.erase(
				std::unique( portal.begin( ), portal.end( ) ), portal.end( ) );
		}
		std::sort( out.waterPvsPortals.begin( ), out.waterPvsPortals.end( ) );
		out.waterPvsPortals.erase(
			std::unique(
				out.waterPvsPortals.begin( ), out.waterPvsPortals.end( ) ),
			out.waterPvsPortals.end( ) );
	}

	R_MarkWaterBatchesVisible( out );

	for( auto &batch : out.batches )
		R_FinalizePVSLeafs( batch.pvsLeafs, batch.pvsAlwaysVisible );
	for( auto &batch : out.dynamicOverlayBatches )
		R_FinalizePVSLeafs( batch.pvsLeafs, batch.pvsAlwaysVisible );

	if( !R_UploadWorldBatches( out ) )
	{
		for( auto &batch : out.batches )
			R_UploadWorldBatch( batch );
	}

	R_UploadOverlayBatches( out );

	for( auto &batch : out.waterBatches )
	{
		R_FinalizePVSLeafs( batch.pvsLeafs, batch.pvsAlwaysVisible );
		R_UploadWaterBatch( batch );
	}
	return true;
}

world_model_draw_t *R_GetLightmappedBrushModel( const model_t *mod )
{
	if( !mod )
		return nullptr;

	auto &front = g_worldModelFrontCache[
		R_PointerFrontCacheIndex( mod, DX_WORLD_MODEL_FRONT_CACHE_SIZE - 1 )];
	if( front.key == mod )
		return front.value;

	if( world_model_draw_t **found = g_worldLightmappedModels.try_get( mod ) )
	{
		front.key = mod;
		front.value = *found;
		return *found;
	}

	g_worldLightmapStorage.emplace_back( );
	world_model_draw_t *cache = &g_worldLightmapStorage.back( );
	if( !R_LoadLightmappedBrushModel( mod, *cache ) )
	{
		R_FreeWorldModel( *cache );
		g_worldLightmapStorage.pop_back( );
		return nullptr;
	}

	g_worldLightmappedModels.insert_unique( mod, cache );
	front.key = mod;
	front.value = cache;
	return cache;
}

static bool R_IsAnimatedBrushTexture( const texture_t *texture )
{
	return texture && texture->name[0] == '+';
}

static int R_AnimatedTextureIndex( const texture_t *texture )
{
	if( !R_IsAnimatedBrushTexture( texture ) )
		return -1;

	const unsigned char c = static_cast<unsigned char>( texture->name[1] );
	if( c >= '0' && c <= '9' )
		return c - '0';
	if( c >= 'A' && c <= 'J' )
		return 10 + c - 'A';
	if( c >= 'a' && c <= 'j' )
		return 10 + c - 'a';
	return -1;
}

static texture_t *R_SelectBrushTextureFrame( texture_t *texture, int frame )
{
	if( !texture || frame < 0 )
		return texture;

	texture_t *const first = texture;
	texture_t *chain = texture;
	for( int pass = 0; pass < 2 && chain; ++pass )
	{
		texture_t *const chainStart = chain;
		for( int guard = 0; chain && guard < 100; ++guard )
		{
			if( R_AnimatedTextureIndex( chain ) == frame )
				return chain;
			chain = chain->anim_next;
			if( chain == chainStart )
				break;
		}

		chain = first->alternate_anims;
	}

	return first;
}

texture_t *R_SelectAnimatedTexture( texture_t *texture,
	const cl_entity_s *ent, float time )
{
	if( !texture || !R_IsAnimatedBrushTexture( texture ) )
		return texture;

	if( ent )
	{
		const int effects = ent->curstate.effects;
		if( effects & EF_FRAMEANIMTEXTURES )
			return R_SelectBrushTextureFrame( texture,
				std::max( 0, static_cast<int>( ent->curstate.frame ) ) );
		if( effects & EF_NOANIMTEXTURES )
			return texture;

		if( ent->curstate.frame != 0.0f && texture->alternate_anims )
			texture = texture->alternate_anims;
	}

	if( !texture || texture->anim_total == 0 )
		return texture;

	texture_t *firstFrame = texture;
	int relative = 0;

	if( texture->anim_total < 0 )
	{
		const int count = -texture->anim_total;
		const int entityNumber = ent ? ent->curstate.number : 0;
		relative = count > 0 ? entityNumber % count : 0;
		if( relative < 0 )
			relative += count;
	}
	else
	{
		relative = static_cast<int>( time * 10.0f ) % texture->anim_total;
		if( relative < 0 )
			relative += texture->anim_total;
	}

	for( int guard = 0; texture && guard < 100; ++guard )
	{
		if( texture->anim_min <= relative && relative < texture->anim_max )
			return texture;
		texture = texture->anim_next;
	}

	return firstFrame;
}

void R_SetLightmapStageEnabled( qboolean enabled )
{
	if( !enabled )
	{
		DX_BindRawTexture( 1, nullptr );
		DX_SetTextureStageStateCached( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
		DX_SetTextureStageStateCached( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
		return;
	}

	DX_SetTextureStageStateCached( 1, D3DTSS_COLOROP,
		R_OverbrightEnabled( ) ? D3DTOP_MODULATE2X : D3DTOP_MODULATE );
	DX_SetTextureStageStateCached( 1, D3DTSS_COLORARG1, D3DTA_CURRENT );
	DX_SetTextureStageStateCached( 1, D3DTSS_COLORARG2, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
	DX_SetTextureStageStateCached( 1, D3DTSS_ALPHAARG1, D3DTA_CURRENT );
	DX_SetTextureStageStateCached( 1, D3DTSS_ALPHAARG2, D3DTA_TEXTURE );

	DX_SetTextureStageStateCached( 1, D3DTSS_TEXCOORDINDEX, 1 );
	DX_SetTextureStageStateCached( 1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
}

static bool R_DynamicLightsEnabled( )
{
	return dx_dlights.value != 0.0f;
}

static int R_DynamicLightsMax( )
{
	return std::max( 0, std::min( 128, static_cast<int>( dx_dlight_max.value ) ) );
}

float R_DynamicLightIntensityScale( )
{
	return std::max( 0.0f, std::min( 8.0f, dx_dlight_intensity.value ) );
}

static float R_DynamicLightRadiusScale( )
{
	return std::max( 0.01f, std::min( 8.0f, dx_dlight_radius_scale.value ) );
}

void R_BeginDLights( void )
{
	g_dxDLights.clear( );
	g_dxLineLights.clear( );
	g_dxStudioGlobalLightCount = 0;
	g_dxStudioLocalLightCount = 0;
}

static void R_AddDLightInternal( const vec3_t origin, float radius,
	int red, int green, int blue, float dieTime, int key, bool studioLocal )
{
	if( !R_DynamicLightsEnabled( ) || !origin )
		return;

	const int maxLights = R_DynamicLightsMax( );
	if( maxLights <= 0 || static_cast<int>( g_dxDLights.size( ) ) >= maxLights )
		return;

	radius *= R_DynamicLightRadiusScale( );
	if( radius < 4.0f )
		return;

	dx_dlight_t light;
	VectorCopy( origin, light.origin );
	light.radius = std::max( 8.0f, std::min( 2048.0f, radius ) );
	light.r = std::max( 0.0f, std::min( 1.0f, static_cast<float>( red ) / 255.0f ) );
	light.g = std::max( 0.0f, std::min( 1.0f, static_cast<float>( green ) / 255.0f ) );
	light.b = std::max( 0.0f, std::min( 1.0f, static_cast<float>( blue ) / 255.0f ) );
	light.dieTime = dieTime;
	light.key = key;
	const uint16_t lightIndex = static_cast<uint16_t>( g_dxDLights.size( ) );
	g_dxDLights.push_back( light );
	if( studioLocal )
	{
		if( g_dxStudioLocalLightCount < static_cast<int>( g_dxStudioLocalLightIndices.size( ) ) )
			g_dxStudioLocalLightIndices[g_dxStudioLocalLightCount++] = lightIndex;
	}
	else
	{
		if( g_dxStudioGlobalLightCount < static_cast<int>( g_dxStudioGlobalLightIndices.size( ) ) )
			g_dxStudioGlobalLightIndices[g_dxStudioGlobalLightCount++] = lightIndex;
	}
}

void R_AddDLight( const vec3_t origin, float radius, int red, int green, int blue, float dieTime, int key )
{
	R_AddDLightInternal( origin, radius, red, green, blue, dieTime, key, false );
}

void R_AddLineLight( const vec3_t start, const vec3_t end, float radius,
	int red, int green, int blue, float dieTime, int key )
{
	if( !R_DynamicLightsEnabled( ) || dx_dlight_beams.value == 0.0f || !start || !end )
		return;

	vec3_t delta;
	VectorSubtract( end, start, delta );
	if( DotProduct( delta, delta ) < 64.0f )
		return;

	const int maxLights = R_DynamicLightsMax( );
	if( maxLights <= 0 || static_cast<int>( g_dxLineLights.size( ) ) >= maxLights )
		return;

	radius *= R_DynamicLightRadiusScale( );
	if( radius < 4.0f )
		return;

	dx_line_light_t light;
	VectorCopy( start, light.start );
	VectorCopy( end, light.end );
	light.radius = std::max( 8.0f, std::min( 2048.0f, radius ) );
	light.r = std::max( 0.0f, std::min( 1.0f, static_cast<float>( red ) / 255.0f ) );
	light.g = std::max( 0.0f, std::min( 1.0f, static_cast<float>( green ) / 255.0f ) );
	light.b = std::max( 0.0f, std::min( 1.0f, static_cast<float>( blue ) / 255.0f ) );
	light.dieTime = dieTime;
	light.key = key;
	g_dxLineLights.push_back( light );
}

void R_AddBeamLight( const vec3_t start, const vec3_t end, float radius, float spacing,
	int red, int green, int blue, float dieTime, int key, int maxSamples )
{
	if( !R_DynamicLightsEnabled( ) || !start || !end )
		return;

	vec3_t delta;
	VectorSubtract( end, start, delta );
	if( DotProduct( delta, delta ) < 64.0f )
		return;

	beam_light_segment_t segment;
	VectorCopy( start, segment.start );
	VectorCopy( end, segment.end );
	segment.radius = std::max( 16.0f, std::min( 1024.0f, radius ) );
	segment.spacing = std::max( 16.0f, std::min( 512.0f, spacing ) );
	segment.dieTime = dieTime;
	segment.r = std::max( 0, std::min( 255, red ) );
	segment.g = std::max( 0, std::min( 255, green ) );
	segment.b = std::max( 0, std::min( 255, blue ) );
	segment.key = key ? key : static_cast<int>( g_dxBeamLights.size( ) + 1 );
	segment.maxSamples = std::max( 2, std::min( 64, maxSamples ) );

	for( beam_light_segment_t &existing : g_dxBeamLights )
	{
		if( existing.key == segment.key )
		{
			existing = segment;
			return;
		}
	}

	if( g_dxBeamLights.size( ) >= 32 )
		g_dxBeamLights.erase( g_dxBeamLights.begin( ) );
	g_dxBeamLights.push_back( segment );
}

void R_CollectBeamLights( void )
{
	if( !R_DynamicLightsEnabled( ) )
	{
		g_dxBeamLights.clear( );
		return;
	}

	const float now = R_CurrentRenderTime( 0.0f );
	g_dxBeamLights.erase(
		std::remove_if( g_dxBeamLights.begin( ), g_dxBeamLights.end( ),
			[now]( const beam_light_segment_t &segment )
			{
				return segment.dieTime <= now;
			} ),
		g_dxBeamLights.end( ) );

	for( const beam_light_segment_t &segment : g_dxBeamLights )
	{
		vec3_t delta;
		VectorSubtract( segment.end, segment.start, delta );
		const float length = VectorLength( delta );
		if( length < 8.0f )
			continue;

		if( dx_dlight_beams.value != 0.0f )
		{
			R_AddLineLight( segment.start, segment.end, segment.radius,
				segment.r, segment.g, segment.b, now + 0.15f, segment.key );
			continue;
		}

		VectorScale( delta, 1.0f / length, delta );
		const int sampleCount = std::max( 2, std::min( std::max( 2, segment.maxSamples ),
			static_cast<int>( length / std::max( 16.0f, segment.spacing ) ) + 1 ) );
		for( int i = 0; i < sampleCount; ++i )
		{
			const float fraction = sampleCount <= 1 ? 0.0f :
				static_cast<float>( i ) / static_cast<float>( sampleCount - 1 );
			vec3_t origin;
			VectorMA( segment.start, length * fraction, delta, origin );
			R_AddDLight( origin, segment.radius,
				segment.r, segment.g, segment.b, now + 0.15f,
				segment.key + i + 1 );
		}
	}
}

static void R_CollectDynamicLightArray( const dlight_t *lights, int count,
	bool studioLocal = false )
{
	if( !lights || count <= 0 )
		return;

	const float now = R_CurrentRenderTime( 0.0f );
	for( int i = 0; i < count; ++i )
	{
		const dlight_t &light = lights[i];
		if( light.radius <= 1.0f || light.dark )
			continue;
		if( light.die > 0.0f && light.die < now )
			continue;

		R_AddDLightInternal( light.origin, light.radius,
			light.color.r, light.color.g, light.color.b, light.die, light.key, studioLocal );
	}
}

void R_CollectEngineDLights( void )
{
	if( dx_dlight_engine.value == 0.0f || !gEngfuncs.EngineGetParm )
		return;

	const dlight_t *dlights = reinterpret_cast<const dlight_t *>(
		gEngfuncs.EngineGetParm( PARM_GET_DLIGHTS_PTR, 0 ) );
	R_CollectDynamicLightArray( dlights, MAX_DLIGHTS, false );

	if( dx_dlight_elights.value != 0.0f )
	{
		const dlight_t *elights = reinterpret_cast<const dlight_t *>(
			gEngfuncs.EngineGetParm( PARM_GET_ELIGHTS_PTR, 0 ) );
		R_CollectDynamicLightArray( elights, MAX_ELIGHTS, true );
	}
}

void R_CollectMuzzleLights( void )
{
	if( dx_dlight_muzzleflash.value == 0.0f )
		return;

	const float now = R_CurrentRenderTime( 0.0f );
	for( const muzzleflash_t &flash : g_rendererMuzzleFlashes )
	{
		if( flash.dieTime <= now )
			continue;

		const float life = std::max( 0.0f, flash.dieTime - now );
		const float scale = std::max( 0.25f, flash.scale * 4.0f );
		R_AddDLight( flash.origin, 128.0f * scale,
			255, 188, 96, now + life, flash.entity ? flash.entity->curstate.number : 0 );
	}
}

void R_CollectEntityDLight( const cl_entity_s *ent )
{
	if( dx_dlight_entities.value == 0.0f || !ent || !ent->model )
		return;

	const int effects = ent->curstate.effects;
	const float now = R_CurrentRenderTime( 0.0f );
	const int key = ent->curstate.number;

	if( effects & EF_BRIGHTLIGHT )
		R_AddDLight( ent->origin, 400.0f, 255, 255, 255, now + 0.10f, key );
	if( effects & EF_DIMLIGHT )
		R_AddDLight( ent->origin, 220.0f, 255, 255, 220, now + 0.10f, key );
	if( effects & EF_LIGHT )
		R_AddDLight( ent->origin, 180.0f, 255, 176, 64, now + 0.10f, key );
}

float R_DistancePointToSegmentSq( const vec3_t point, const vec3_t start, const vec3_t end, float &outFraction )
{
	vec3_t segment;
	VectorSubtract( end, start, segment );
	const float lenSq = DotProduct( segment, segment );
	if( lenSq <= 0.0001f )
	{
		outFraction = 0.0f;
		const float dx = point[0] - start[0];
		const float dy = point[1] - start[1];
		const float dz = point[2] - start[2];
		return dx * dx + dy * dy + dz * dz;
	}

	vec3_t toPoint;
	VectorSubtract( point, start, toPoint );
	outFraction = std::max( 0.0f, std::min( 1.0f, DotProduct( toPoint, segment ) / lenSq ) );
	vec3_t closest;
	VectorMA( start, outFraction, segment, closest );
	const float dx = point[0] - closest[0];
	const float dy = point[1] - closest[1];
	const float dz = point[2] - closest[2];
	return dx * dx + dy * dy + dz * dz;
}

D3DCOLOR R_AddStudioDLights( D3DCOLOR baseColor, const vec3_t point )
{
	if( dx_dlight_studio.value == 0.0f || !point ||
		( g_dxStudioGlobalLightCount <= 0 && g_dxLineLights.empty( ) ) )
		return baseColor;

	float outR = static_cast<float>( ( baseColor >> 16 ) & 0xFF );
	float outG = static_cast<float>( ( baseColor >> 8 ) & 0xFF );
	float outB = static_cast<float>( baseColor & 0xFF );
	const float intensity = R_DynamicLightIntensityScale( );

	if( g_dxStudioGlobalLightCount > 0 )
	{
		DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_DYNAMIC_LIGHT_CANDIDATE,
			static_cast<unsigned int>( g_dxStudioGlobalLightCount ) );
		for( int i = 0; i < g_dxStudioGlobalLightCount; ++i )
		{
			const dx_dlight_t &light =
				g_dxDLights[g_dxStudioGlobalLightIndices[i]];
			const float dx = point[0] - light.origin[0];
			const float dy = point[1] - light.origin[1];
			const float dz = point[2] - light.origin[2];
			const float distSq = dx * dx + dy * dy + dz * dz;
			const float radiusSq = light.radius * light.radius;
			if( distSq >= radiusSq || radiusSq <= 0.0f )
				continue;

			const float distance = sqrtf( distSq );
			const float t = 1.0f - std::max( 0.0f, std::min( 1.0f, distance / light.radius ) );
			const float add = t * t * intensity * 255.0f;
			outR += light.r * add;
			outG += light.g * add;
			outB += light.b * add;
		}
	}

	if( !g_dxLineLights.empty( ) )
	{
		DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_DYNAMIC_LIGHT_CANDIDATE,
			static_cast<unsigned int>( g_dxLineLights.size( ) ) );
		for( const dx_line_light_t &light : g_dxLineLights )
		{
			float fraction = 0.0f;
			const float distSq = R_DistancePointToSegmentSq( point, light.start, light.end, fraction );
			const float radiusSq = light.radius * light.radius;
			if( distSq >= radiusSq || radiusSq <= 0.0f )
				continue;
			const float distance = sqrtf( distSq );
			const float t = 1.0f - std::max( 0.0f, std::min( 1.0f, distance / light.radius ) );
			const float add = t * t * intensity *
				std::max( 0.0f, std::min( 8.0f, dx_dlight_beam_intensity.value ) ) * 255.0f;
			outR += light.r * add;
			outG += light.g * add;
			outB += light.b * add;
		}
	}

	return D3DCOLOR_ARGB( static_cast<byte>( ( baseColor >> 24 ) & 0xFF ),
		static_cast<int>( std::max( 0.0f, std::min( 255.0f, outR ) ) ),
		static_cast<int>( std::max( 0.0f, std::min( 255.0f, outG ) ) ),
		static_cast<int>( std::max( 0.0f, std::min( 255.0f, outB ) ) ) );
}

void R_ApplyDLights( void )
{
	if( !device )
		return;

	if( !R_DynamicLightsEnabled( ) || dx_dlight_fixed.value == 0.0f )
	{
		for( int i = 0; i < g_dxFixedLightCount; ++i )
			device->LightEnable( i, FALSE );
		g_dxFixedLightCount = 0;
		return;
	}

	const int emitCount = std::min( 8, std::min( R_DynamicLightsMax( ),
		static_cast<int>( g_dxDLights.size( ) ) ) );
	for( int i = 0; i < emitCount; ++i )
	{
		const dx_dlight_t &source = g_dxDLights[static_cast<size_t>( i )];
		D3DLIGHT9 light{};
		light.Type = D3DLIGHT_POINT;
		light.Position.x = source.origin[0];
		light.Position.y = source.origin[1];
		light.Position.z = source.origin[2];
		const float intensity = R_DynamicLightIntensityScale( );
		light.Diffuse.r = source.r * intensity;
		light.Diffuse.g = source.g * intensity;
		light.Diffuse.b = source.b * intensity;
		light.Diffuse.a = 1.0f;
		light.Range = source.radius;
		light.Attenuation0 = 0.0f;
		light.Attenuation1 = 1.0f / std::max( 1.0f, source.radius );
		light.Attenuation2 = 1.0f / std::max( 1.0f, source.radius * source.radius );
		device->SetLight( i, &light );
		device->LightEnable( i, TRUE );
	}

	for( int i = emitCount; i < g_dxFixedLightCount; ++i )
		device->LightEnable( i, FALSE );
	g_dxFixedLightCount = emitCount;
}

static unsigned int R_DynamicLightmapHashFloat( unsigned int hash, float value, float quantize )
{
	const int q = static_cast<int>( floorf( value / std::max( 0.001f, quantize ) + 0.5f ) );
	return ( hash ^ static_cast<unsigned int>( q + 0x9e3779b9u + ( hash << 6 ) + ( hash >> 2 ) ) );
}

static unsigned int R_BuildDLightSignature( )
{
	unsigned int hash = 2166136261u;
	hash = R_DynamicLightmapHashFloat( hash, dx_dlight_intensity.value, 0.01f );
	hash = R_DynamicLightmapHashFloat( hash, dx_dlight_beam_intensity.value, 0.01f );
	hash = R_DynamicLightmapHashFloat( hash, dx_dlight_min_light.value, 0.1f );
	hash = hash * 16777619u ^ static_cast<unsigned int>( g_dxDLights.size( ) );
	for( const dx_dlight_t &light : g_dxDLights )
	{
		hash = R_DynamicLightmapHashFloat( hash, light.origin[0], 2.0f );
		hash = R_DynamicLightmapHashFloat( hash, light.origin[1], 2.0f );
		hash = R_DynamicLightmapHashFloat( hash, light.origin[2], 2.0f );
		hash = R_DynamicLightmapHashFloat( hash, light.radius, 2.0f );
		hash = R_DynamicLightmapHashFloat( hash, light.r, 0.01f );
		hash = R_DynamicLightmapHashFloat( hash, light.g, 0.01f );
		hash = R_DynamicLightmapHashFloat( hash, light.b, 0.01f );
	}
	hash = hash * 16777619u ^ static_cast<unsigned int>( g_dxLineLights.size( ) << 8 );
	for( const dx_line_light_t &light : g_dxLineLights )
	{
		hash = R_DynamicLightmapHashFloat( hash, light.start[0], 2.0f );
		hash = R_DynamicLightmapHashFloat( hash, light.start[1], 2.0f );
		hash = R_DynamicLightmapHashFloat( hash, light.start[2], 2.0f );
		hash = R_DynamicLightmapHashFloat( hash, light.end[0], 2.0f );
		hash = R_DynamicLightmapHashFloat( hash, light.end[1], 2.0f );
		hash = R_DynamicLightmapHashFloat( hash, light.end[2], 2.0f );
		hash = R_DynamicLightmapHashFloat( hash, light.radius, 2.0f );
		hash = R_DynamicLightmapHashFloat( hash, light.r, 0.01f );
		hash = R_DynamicLightmapHashFloat( hash, light.g, 0.01f );
		hash = R_DynamicLightmapHashFloat( hash, light.b, 0.01f );
	}
	return hash;
}

static void R_CrossProduct3( const float a[3], const float b[3], float out[3] )
{
	out[0] = a[1] * b[2] - a[2] * b[1];
	out[1] = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
}

bool R_LightmapTexelWorldPoint( const msurface_t *surface, int sampleX, int sampleY,
	float sampleSize, vec3_t outPoint )
{
	if( !surface || !surface->plane || !surface->texinfo )
		return false;

	float axisS[3];
	float axisT[3];
	float targetS;
	float targetT;

	if( surface->info )
	{
		axisS[0] = surface->info->lmvecs[0][0];
		axisS[1] = surface->info->lmvecs[0][1];
		axisS[2] = surface->info->lmvecs[0][2];
		axisT[0] = surface->info->lmvecs[1][0];
		axisT[1] = surface->info->lmvecs[1][1];
		axisT[2] = surface->info->lmvecs[1][2];
		targetS = surface->info->lightmapmins[0] + static_cast<float>( sampleX ) * sampleSize - surface->info->lmvecs[0][3];
		targetT = surface->info->lightmapmins[1] + static_cast<float>( sampleY ) * sampleSize - surface->info->lmvecs[1][3];
	}
	else
	{
		axisS[0] = surface->texinfo->vecs[0][0];
		axisS[1] = surface->texinfo->vecs[0][1];
		axisS[2] = surface->texinfo->vecs[0][2];
		axisT[0] = surface->texinfo->vecs[1][0];
		axisT[1] = surface->texinfo->vecs[1][1];
		axisT[2] = surface->texinfo->vecs[1][2];
		targetS = surface->texturemins[0] + static_cast<float>( sampleX ) * sampleSize - surface->texinfo->vecs[0][3];
		targetT = surface->texturemins[1] + static_cast<float>( sampleY ) * sampleSize - surface->texinfo->vecs[1][3];
	}

	const float *normal = surface->plane->normal;
	float axisTCrossN[3];
	float nCrossAxisS[3];
	float axisSCrossT[3];
	R_CrossProduct3( axisT, normal, axisTCrossN );
	R_CrossProduct3( normal, axisS, nCrossAxisS );
	R_CrossProduct3( axisS, axisT, axisSCrossT );
	const float det = DotProduct( axisS, axisTCrossN );
	if( fabsf( det ) < 0.000001f )
		return false;

	const float invDet = 1.0f / det;
	const float planeDist = surface->plane->dist;
	outPoint[0] = ( targetS * axisTCrossN[0] + targetT * nCrossAxisS[0] + planeDist * axisSCrossT[0] ) * invDet;
	outPoint[1] = ( targetS * axisTCrossN[1] + targetT * nCrossAxisS[1] + planeDist * axisSCrossT[1] ) * invDet;
	outPoint[2] = ( targetS * axisTCrossN[2] + targetT * nCrossAxisS[2] + planeDist * axisSCrossT[2] ) * invDet;
	return true;
}

static bool R_DLightTouchesAlloc( const world_lightmap_allocation_t &allocation )
{
	if( !allocation.surface || !allocation.surface->samples || !allocation.boundsValid )
		return false;
	return R_DynamicLightTouchesBounds( allocation.mins, allocation.maxs );
}

static void R_ClearOverlayAlloc(
	const world_lightmap_allocation_t &allocation,
	world_lightmap_atlas_t &atlas )
{
	if( atlas.pixels.empty( ) )
		return;
	for( int y = -LIGHTMAP_GUTTER;
		y < allocation.height + LIGHTMAP_GUTTER; ++y )
	{
		const int dstY = allocation.overlayY + y;
		if( dstY < 0 || dstY >= atlas.size )
			continue;
		for( int x = -LIGHTMAP_GUTTER;
			x < allocation.width + LIGHTMAP_GUTTER; ++x )
		{
			const int dstX = allocation.overlayX + x;
			if( dstX < 0 || dstX >= atlas.size )
				continue;
			atlas.pixels[static_cast<size_t>( dstY ) * atlas.size + dstX] =
				D3DCOLOR_ARGB( 255, 0, 0, 0 );
		}
	}
}

static bool R_AddDLightsToOverlay(
	const world_lightmap_allocation_t &allocation,
	world_lightmap_atlas_t &atlas )
{
	const msurface_t *surface = allocation.surface;
	if( !surface || !surface->samples || atlas.pixels.empty( ) )
		return false;

	bool anyContribution = false;
	const float sampleSize = R_SurfaceSampleSize( surface );
	const float intensity = R_DynamicLightIntensityScale( );
	const float minLight = std::max( 0.0f,
		std::min( 64.0f, dx_dlight_min_light.value ) );

	for( int y = -LIGHTMAP_GUTTER;
		y < allocation.height + LIGHTMAP_GUTTER; ++y )
	{
		const int sampleY = std::max( 0,
			std::min( allocation.height - 1, y ) );
		for( int x = -LIGHTMAP_GUTTER;
			x < allocation.width + LIGHTMAP_GUTTER; ++x )
		{
			const int sampleX = std::max( 0,
				std::min( allocation.width - 1, x ) );
			vec3_t worldPoint;
			if( !R_LightmapTexelWorldPoint( surface, sampleX, sampleY,
				sampleSize, worldPoint ) )
				continue;

			float addR = 0.0f, addG = 0.0f, addB = 0.0f;
			for( const dx_dlight_t &light : g_dxDLights )
			{
				const float dx = worldPoint[0] - light.origin[0];
				const float dy = worldPoint[1] - light.origin[1];
				const float dz = worldPoint[2] - light.origin[2];
				const float distSq = dx * dx + dy * dy + dz * dz;
				const float radiusSq = light.radius * light.radius;
				if( distSq >= radiusSq || radiusSq <= 1.0f )
					continue;
				const float distance = sqrtf( distSq );
				const float t = 1.0f - std::max( 0.0f,
					std::min( 1.0f, distance / light.radius ) );
				const float add = t * t * intensity * 255.0f;
				addR += light.r * add;
				addG += light.g * add;
				addB += light.b * add;
			}

			for( const dx_line_light_t &light : g_dxLineLights )
			{
				float fraction = 0.0f;
				const float distSq = R_DistancePointToSegmentSq(
					worldPoint, light.start, light.end, fraction );
				const float radiusSq = light.radius * light.radius;
				if( distSq >= radiusSq || radiusSq <= 1.0f )
					continue;
				const float distance = sqrtf( distSq );
				const float t = 1.0f - std::max( 0.0f,
					std::min( 1.0f, distance / light.radius ) );
				const float add = t * t * intensity *
					std::max( 0.0f, std::min( 8.0f,
						dx_dlight_beam_intensity.value ) ) * 255.0f;
				addR += light.r * add;
				addG += light.g * add;
				addB += light.b * add;
			}

			if( addR < minLight && addG < minLight && addB < minLight )
				continue;
			const int dstX = allocation.overlayX + x;
			const int dstY = allocation.overlayY + y;
			if( dstX < 0 || dstX >= atlas.size ||
				dstY < 0 || dstY >= atlas.size )
				continue;
			atlas.pixels[static_cast<size_t>( dstY ) * atlas.size + dstX] =
				D3DCOLOR_ARGB( 255,
					std::min( 255, static_cast<int>( addR + 0.5f ) ),
					std::min( 255, static_cast<int>( addG + 0.5f ) ),
					std::min( 255, static_cast<int>( addB + 0.5f ) ) );
			anyContribution = true;
		}
	}
	return anyContribution;
}

static void R_PrepareOverlayScratch( )
{
	const size_t count = g_mapLightmaps.dynamicOverlayAtlases.size( );
	if( g_dynamicOverlayDirtyScratch.size( ) != count )
		g_dynamicOverlayDirtyScratch.resize( count );
	for( lightmap_dirty_atlas_t &dirty : g_dynamicOverlayDirtyScratch )
		dirty.rects.clear( );
}

static void R_MarkOverlayDirty(
	const world_lightmap_allocation_t &allocation )
{
	if( allocation.overlayAtlasIndex < 0 ||
		allocation.overlayAtlasIndex >= static_cast<int>(
			g_mapLightmaps.dynamicOverlayAtlases.size( ) ) )
		return;
	world_lightmap_atlas_t &atlas =
		g_mapLightmaps.dynamicOverlayAtlases[allocation.overlayAtlasIndex];
	R_MarkLightmapRectDirty(
		g_dynamicOverlayDirtyScratch[allocation.overlayAtlasIndex],
		allocation.overlayX, allocation.overlayY,
		allocation.width, allocation.height, atlas );
}

static void R_FlushOverlayAtlases( )
{
	const size_t count = std::min( g_mapLightmaps.dynamicOverlayAtlases.size( ),
		g_dynamicOverlayDirtyScratch.size( ) );
	for( size_t atlasIndex = 0; atlasIndex < count; ++atlasIndex )
	{
		if( g_dynamicOverlayDirtyScratch[atlasIndex].rects.empty( ) )
			continue;
		if( R_UploadDirtyLightmapAtlas(
			g_mapLightmaps.dynamicOverlayAtlases[atlasIndex],
			g_dynamicOverlayDirtyScratch[atlasIndex], false ) )
		{
			DX_ProfileEvent( DX_PROFILE_EVENT_LIGHTMAP_ATLAS_UPLOAD, 1 );
		}
	}
}

static void R_ClearStaleOverlays( )
{
	if( g_dlightTouchedAllocs.empty( ) )
		return;
	R_PrepareOverlayScratch( );
	for( const size_t allocationIndex : g_dlightTouchedAllocs )
	{
		if( allocationIndex >= g_mapLightmaps.allocations.size( ) )
			continue;
		world_lightmap_allocation_t &allocation =
			g_mapLightmaps.allocations[allocationIndex];
		if( allocation.overlayAtlasIndex < 0 ||
			allocation.overlayAtlasIndex >= static_cast<int>(
				g_mapLightmaps.dynamicOverlayAtlases.size( ) ) )
			continue;
		R_ClearOverlayAlloc( allocation,
			g_mapLightmaps.dynamicOverlayAtlases[allocation.overlayAtlasIndex] );
		R_MarkOverlayDirty( allocation );
		if( allocationIndex < g_dlightOverlayActive.size( ) )
			g_dlightOverlayActive[allocationIndex] = 0;
	}
	R_FlushOverlayAtlases( );
	g_dlightTouchedAllocs.clear( );
}

void R_UpdateDLightOverlay( void )
{
	const bool usable = R_DynamicLightsEnabled( ) &&
		dx_dlight_world.value != 0.0f &&
		dx_dlight_lightmaps.value != 0.0f &&
		r_nolm.value == 0.0f && !g_mapLightmaps.allocations.empty( ) &&
		!g_mapLightmaps.dynamicOverlayAtlases.empty( );
	const bool haveLights = !g_dxDLights.empty( ) ||
		!g_dxLineLights.empty( );

	if( g_dlightOverlayActive.size( ) !=
		g_mapLightmaps.allocations.size( ) )
	{
		g_dlightOverlayActive.assign(
			g_mapLightmaps.allocations.size( ), 0 );
	}
	if( g_dynamicOverlaySelectedMask.size( ) != g_mapLightmaps.allocations.size( ) )
		g_dynamicOverlaySelectedMask.assign( g_mapLightmaps.allocations.size( ), 0 );

	if( !usable || !haveLights )
	{
		R_ClearStaleOverlays( );
		g_dynamicLightmapSignature = 0;
		return;
	}

	const unsigned int signature = R_BuildDLightSignature( );
	if( signature == g_dynamicLightmapSignature )
		return;

	g_dlightSelected.clear( );
	if( g_dlightSelected.capacity( ) < 64 )
		g_dlightSelected.reserve( 64 );
	for( size_t i = 0; i < g_mapLightmaps.allocations.size( ); ++i )
	{
		const world_lightmap_allocation_t &allocation = g_mapLightmaps.allocations[i];
		if( allocation.overlayAtlasIndex >= 0 &&
			R_DLightTouchesAlloc( allocation ) )
		{
			g_dlightSelected.push_back( i );
			g_dynamicOverlaySelectedMask[i] = 1;
		}
	}

	g_dlightRestore.clear( );
	for( const size_t oldIndex : g_dlightTouchedAllocs )
	{
		if( oldIndex >= g_dynamicOverlaySelectedMask.size( ) ||
			!g_dynamicOverlaySelectedMask[oldIndex] )
		{
			g_dlightRestore.push_back( oldIndex );
		}
	}

	R_PrepareOverlayScratch( );
	for( const size_t allocationIndex : g_dlightRestore )
	{
		if( allocationIndex >= g_mapLightmaps.allocations.size( ) )
			continue;
		world_lightmap_allocation_t &allocation =
			g_mapLightmaps.allocations[allocationIndex];
		if( allocation.overlayAtlasIndex < 0 ||
			allocation.overlayAtlasIndex >= static_cast<int>(
				g_mapLightmaps.dynamicOverlayAtlases.size( ) ) )
			continue;
		R_ClearOverlayAlloc( allocation,
			g_mapLightmaps.dynamicOverlayAtlases[allocation.overlayAtlasIndex] );
		R_MarkOverlayDirty( allocation );
		g_dlightOverlayActive[allocationIndex] = 0;
	}

	for( const size_t allocationIndex : g_dlightSelected )
	{
		if( allocationIndex >= g_mapLightmaps.allocations.size( ) )
			continue;
		world_lightmap_allocation_t &allocation =
			g_mapLightmaps.allocations[allocationIndex];
		if( allocation.overlayAtlasIndex < 0 ||
			allocation.overlayAtlasIndex >= static_cast<int>(
				g_mapLightmaps.dynamicOverlayAtlases.size( ) ) )
			continue;
		world_lightmap_atlas_t &atlas =
			g_mapLightmaps.dynamicOverlayAtlases[allocation.overlayAtlasIndex];
		R_ClearOverlayAlloc( allocation, atlas );
		const bool contributed =
			R_AddDLightsToOverlay( allocation, atlas );
		R_MarkOverlayDirty( allocation );
		g_dlightOverlayActive[allocationIndex] =
			contributed ? 1 : 0;
	}
	R_FlushOverlayAtlases( );

	g_dlightTouchedAllocs.clear( );
	g_dlightTouchedAllocs.reserve(
		g_dlightSelected.size( ) );
	for( const size_t selectedIndex : g_dlightSelected )
	{
		if( selectedIndex < g_dlightOverlayActive.size( ) &&
			g_dlightOverlayActive[selectedIndex] )
		{
			g_dlightTouchedAllocs.push_back( selectedIndex );
		}
	}
	for( const size_t selectedIndex : g_dlightSelected )
		g_dynamicOverlaySelectedMask[selectedIndex] = 0;
	g_dynamicLightmapSignature = signature;
}

void R_SpawnRendererMuzzleFlash( const cl_entity_s *ent,
	const vec3_t origin, int eventOption, bool viewModel, float renderTime )
{
	if( dx_muzzleflash_sprite.value == 0.0f || !origin )
		return;

	int option = eventOption;
	if( option < 0 ) option = -option;
	int spriteType = option % 10;
	float scale = static_cast<float>( option / 10 ) * 0.1f;
	if( scale <= 0.0001f )
		scale = 0.20f;

	scale *= std::max( 0.01f, std::min( 8.0f, dx_muzzleflash_scale.value ) );

	muzzleflash_t flash;
	flash.entity = ent;
	VectorCopy( origin, flash.origin );

	const float forwardOffset = std::max( -16.0f,
		std::min( 16.0f, dx_muzzleflash_forward_offset.value ) );
	VectorMA( flash.origin, forwardOffset, g_vForward, flash.origin );

	flash.textureId = R_FindMuzzleFlashTexture( spriteType );
	flash.scale = scale;
	flash.viewModel = viewModel;
	flash.startTime = R_CurrentRenderTime( renderTime );
	flash.dieTime = flash.startTime + std::max( 0.01f,
		std::min( 0.25f, dx_muzzleflash_duration.value ) );
	flash.rotation = fmodf( flash.startTime * 97.0f +
		static_cast<float>( reinterpret_cast<uintptr_t>( ent ) & 1023u ) * 0.017453292f,
		6.28318530718f );

	g_rendererMuzzleFlashes.push_back( flash );

	if( dx_dlight_muzzleflash.value != 0.0f )
	{
		R_AddDLight( flash.origin, 120.0f * std::max( 0.5f, scale * 4.0f ),
			255, 192, 72, flash.dieTime,
			ent ? ent->curstate.number : 0 );
	}
}

bool R_DynamicLightTouchesBounds( const vec3_t mins, const vec3_t maxs )
{
	for( const dx_dlight_t &light : g_dxDLights )
	{
		float distSq = 0.0f;
		for( int axis = 0; axis < 3; ++axis )
		{
			float delta = 0.0f;
			if( light.origin[axis] < mins[axis] )
				delta = mins[axis] - light.origin[axis];
			else if( light.origin[axis] > maxs[axis] )
				delta = light.origin[axis] - maxs[axis];
			distSq += delta * delta;
		}
		if( distSq <= light.radius * light.radius )
			return true;
	}

	for( const dx_line_light_t &light : g_dxLineLights )
	{
		bool separated = false;
		for( int axis = 0; axis < 3; ++axis )
		{
			const float segMin = std::min( light.start[axis], light.end[axis] ) - light.radius;
			const float segMax = std::max( light.start[axis], light.end[axis] ) + light.radius;
			if( segMax < mins[axis] || segMin > maxs[axis] )
			{
				separated = true;
				break;
			}
		}
		if( !separated )
			return true;
	}
	return false;
}

static D3DCOLOR R_DynamicWorldColorAtPoint( const vec3_t worldPoint )
{
	float outR = 0.0f;
	float outG = 0.0f;
	float outB = 0.0f;
	const float intensity = R_DynamicLightIntensityScale( );
	const float minLight = std::max( 0.0f, std::min( 64.0f, dx_dlight_min_light.value ) );

	for( const dx_dlight_t &light : g_dxDLights )
	{
		const float dx = worldPoint[0] - light.origin[0];
		const float dy = worldPoint[1] - light.origin[1];
		const float dz = worldPoint[2] - light.origin[2];
		const float distSq = dx * dx + dy * dy + dz * dz;
		const float radiusSq = light.radius * light.radius;
		if( distSq >= radiusSq || radiusSq <= 0.0f )
			continue;

		const float distance = sqrtf( distSq );
		const float t = 1.0f - std::max( 0.0f, std::min( 1.0f, distance / light.radius ) );
		const float add = t * t * intensity * 255.0f;
		outR += light.r * add;
		outG += light.g * add;
		outB += light.b * add;
	}

	for( const dx_line_light_t &light : g_dxLineLights )
	{
		float fraction = 0.0f;
		const float distSq = R_DistancePointToSegmentSq( worldPoint, light.start, light.end, fraction );
		const float radiusSq = light.radius * light.radius;
		if( distSq >= radiusSq || radiusSq <= 0.0f )
			continue;
		const float distance = sqrtf( distSq );
		const float t = 1.0f - std::max( 0.0f, std::min( 1.0f, distance / light.radius ) );
		const float add = t * t * intensity *
			std::max( 0.0f, std::min( 8.0f, dx_dlight_beam_intensity.value ) ) * 255.0f;
		outR += light.r * add;
		outG += light.g * add;
		outB += light.b * add;
	}

	if( outR < minLight && outG < minLight && outB < minLight )
		return 0;

	return D3DCOLOR_ARGB( 255,
		static_cast<int>( std::max( 0.0f, std::min( 255.0f, outR ) ) ),
		static_cast<int>( std::max( 0.0f, std::min( 255.0f, outG ) ) ),
		static_cast<int>( std::max( 0.0f, std::min( 255.0f, outB ) ) ) );
}

static dynamic_light_vertex_t R_MakeDynamicLightVertex( const lightmap_vertex_t &source,
	const D3DMATRIX &worldMatrix )
{
	dynamic_light_vertex_t out;
	out.x = source.x;
	out.y = source.y;
	out.z = source.z;
	out.u = source.u;
	out.v = source.v;
	vec3_t worldPoint;
	R_TransformPoint( worldMatrix, source.x, source.y, source.z, worldPoint );
	out.diffuse = R_DynamicWorldColorAtPoint( worldPoint );
	return out;
}

static dynamic_light_vertex_t R_MidDynamicLightVertex( const dynamic_light_vertex_t &a,
	const dynamic_light_vertex_t &b, const D3DMATRIX &worldMatrix )
{
	dynamic_light_vertex_t out;
	out.x = 0.5f * ( a.x + b.x );
	out.y = 0.5f * ( a.y + b.y );
	out.z = 0.5f * ( a.z + b.z );
	out.u = 0.5f * ( a.u + b.u );
	out.v = 0.5f * ( a.v + b.v );
	vec3_t worldPoint;
	R_TransformPoint( worldMatrix, out.x, out.y, out.z, worldPoint );
	out.diffuse = R_DynamicWorldColorAtPoint( worldPoint );
	return out;
}

static float R_DynamicLightEdgeLengthSq( const dynamic_light_vertex_t &a,
	const dynamic_light_vertex_t &b, const D3DMATRIX &worldMatrix )
{
	vec3_t aw, bw;
	R_TransformPoint( worldMatrix, a.x, a.y, a.z, aw );
	R_TransformPoint( worldMatrix, b.x, b.y, b.z, bw );
	const float dx = aw[0] - bw[0];
	const float dy = aw[1] - bw[1];
	const float dz = aw[2] - bw[2];
	return dx * dx + dy * dy + dz * dz;
}

static void R_AppendDynamicLightTriangle( const dynamic_light_vertex_t &a,
	const dynamic_light_vertex_t &b, const dynamic_light_vertex_t &c,
	const D3DMATRIX &worldMatrix, int depth,
	std::vector<dynamic_light_vertex_t> &vertices, std::vector<DWORD> &indices )
{
	const float subdivide = std::max( 0.0f, std::min( 512.0f, dx_dlight_subdivide.value ) );
	if( subdivide > 1.0f && depth < 5 )
	{
		const float maxEdgeSq = std::max( R_DynamicLightEdgeLengthSq( a, b, worldMatrix ),
			std::max( R_DynamicLightEdgeLengthSq( b, c, worldMatrix ),
				R_DynamicLightEdgeLengthSq( c, a, worldMatrix ) ) );
		if( maxEdgeSq > subdivide * subdivide )
		{
			const dynamic_light_vertex_t ab = R_MidDynamicLightVertex( a, b, worldMatrix );
			const dynamic_light_vertex_t bc = R_MidDynamicLightVertex( b, c, worldMatrix );
			const dynamic_light_vertex_t ca = R_MidDynamicLightVertex( c, a, worldMatrix );
			R_AppendDynamicLightTriangle( a, ab, ca, worldMatrix, depth + 1, vertices, indices );
			R_AppendDynamicLightTriangle( ab, b, bc, worldMatrix, depth + 1, vertices, indices );
			R_AppendDynamicLightTriangle( ca, bc, c, worldMatrix, depth + 1, vertices, indices );
			R_AppendDynamicLightTriangle( ab, bc, ca, worldMatrix, depth + 1, vertices, indices );
			return;
		}
	}

	if( a.diffuse == 0 && b.diffuse == 0 && c.diffuse == 0 )
		return;

	const DWORD base = static_cast<DWORD>( vertices.size( ) );
	vertices.push_back( a );
	vertices.push_back( b );
	vertices.push_back( c );
	indices.push_back( base + 0 );
	indices.push_back( base + 1 );
	indices.push_back( base + 2 );
}

static void R_DrawDynamicLightBrushPass( const world_model_draw_t &model,
	const cl_entity_s *ent, float time )
{
	if( dx_dlight_world.value == 0.0f || dx_dlight_geometry.value == 0.0f ||
		g_dxDLights.empty( ) || model.batches.empty( ) || !ent )
		return;

	D3DMATRIX worldMatrix;
	R_SetEntityWorldTransform( ent, time, &worldMatrix );

	device->SetFVF( DYNAMIC_LIGHT_FVF );
	R_SetLightmapStageEnabled( false );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXCOORDINDEX, 0 );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE );
	DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_BLENDOP, D3DBLENDOP_ADD );
	DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_ONE );
	DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_ONE );

	int boundBaseTexture = -1;
	std::vector<dynamic_light_vertex_t> litVertices;
	std::vector<DWORD> litIndices;
	for( size_t batchIndex = 0; batchIndex < model.batches.size( ); ++batchIndex )
	{
		const world_material_batch_t &batch = model.batches[batchIndex];
		if( batch.vertices.empty( ) || batch.indices.empty( ) || !batch.boundsValid )
			continue;

		vec3_t worldMins, worldMaxs;
		R_TransformBounds( batch.mins, batch.maxs, worldMatrix, worldMins, worldMaxs );
		if( !R_DynamicLightTouchesBounds( worldMins, worldMaxs ) )
			continue;

		texture_t *texture = R_SelectAnimatedTexture( batch.baseTexture, ent, time );
		if( !texture || texture->gl_texturenum <= 0 )
			continue;

		litVertices.clear( );
		litIndices.clear( );
		litVertices.reserve( std::min<size_t>( batch.indices.size( ) * 2u, 16384u ) );
		litIndices.reserve( std::min<size_t>( batch.indices.size( ) * 2u, 16384u ) );

		for( size_t index = 0; index + 2 < batch.indices.size( ); index += 3 )
		{
			const DWORD ia = batch.indices[index + 0];
			const DWORD ib = batch.indices[index + 1];
			const DWORD ic = batch.indices[index + 2];
			if( ia >= batch.vertices.size( ) || ib >= batch.vertices.size( ) || ic >= batch.vertices.size( ) )
				continue;

			const dynamic_light_vertex_t a = R_MakeDynamicLightVertex( batch.vertices[ia], worldMatrix );
			const dynamic_light_vertex_t b = R_MakeDynamicLightVertex( batch.vertices[ib], worldMatrix );
			const dynamic_light_vertex_t c = R_MakeDynamicLightVertex( batch.vertices[ic], worldMatrix );
			R_AppendDynamicLightTriangle( a, b, c, worldMatrix, 0, litVertices, litIndices );
		}

		if( litVertices.empty( ) || litIndices.empty( ) )
			continue;

		if( texture->gl_texturenum != boundBaseTexture )
		{
			DX_Bind( 0, texture->gl_texturenum );
			boundBaseTexture = texture->gl_texturenum;
		}

		R_DrawScratchIndexedPrimitive( D3DPT_TRIANGLELIST,
			static_cast<UINT>( litVertices.size( ) ),
			static_cast<UINT>( litIndices.size( ) / 3 ),
			litIndices.data( ), D3DFMT_INDEX32,
			litVertices.data( ), sizeof( dynamic_light_vertex_t ) );
	}

	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
	device->SetFVF( BRUSH_FVF );
}

bool R_ReadSurfaceStaticLight( const msurface_t *surface,
	const vec3_t point, color24 &outLight, float *outLightVec )
{
	if( !surface || !surface->samples || !surface->texinfo )
	{
		if( outLightVec ) VectorClear( outLightVec );
		return false;
	}

	const float sampleSize = R_SurfaceSampleSize( surface );
	int lightmapWidth = 0;
	int lightmapHeight = 0;
	int sampleS = 0;
	int sampleT = 0;

	if( surface->info )
	{
		lightmapWidth = static_cast<int>( surface->info->lightextents[0] / sampleSize ) + 1;
		lightmapHeight = static_cast<int>( surface->info->lightextents[1] / sampleSize ) + 1;

		const float sampleCoordS =
			( DotProduct( point, surface->info->lmvecs[0] ) + surface->info->lmvecs[0][3]
				- surface->info->lightmapmins[0] ) / sampleSize;
		const float sampleCoordT =
			( DotProduct( point, surface->info->lmvecs[1] ) + surface->info->lmvecs[1][3]
				- surface->info->lightmapmins[1] ) / sampleSize;
		sampleS = static_cast<int>( floorf( sampleCoordS + 0.5f ) );
		sampleT = static_cast<int>( floorf( sampleCoordT + 0.5f ) );
	}
	else
	{
		const mtexinfo_t *texinfo = surface->texinfo;
		const float s = DotProduct( point, texinfo->vecs[0] ) + texinfo->vecs[0][3];
		const float t = DotProduct( point, texinfo->vecs[1] ) + texinfo->vecs[1][3];
		lightmapWidth = static_cast<int>( surface->extents[0] / sampleSize ) + 1;
		lightmapHeight = static_cast<int>( surface->extents[1] / sampleSize ) + 1;
		sampleS = static_cast<int>( floorf(
			( s - surface->texturemins[0] ) / sampleSize + 0.5f ) );
		sampleT = static_cast<int>( floorf(
			( t - surface->texturemins[1] ) / sampleSize + 0.5f ) );
	}

	if( lightmapWidth <= 0 || lightmapHeight <= 0 ||
		sampleS < 0 || sampleT < 0 ||
		sampleS >= lightmapWidth || sampleT >= lightmapHeight )
	{
		if( outLightVec ) VectorClear( outLightVec );
		return false;
	}

	const int texelCount = lightmapWidth * lightmapHeight;
	const int texelIndex = sampleT * lightmapWidth + sampleS;
	const color24 *samplePlane = surface->samples;
	const color24 *deluxePlane =
		( outLightVec && dx_studio_extended_lighting.value != 0.0f && surface->info )
		? surface->info->deluxemap : nullptr;
	int red = 0;
	int green = 0;
	int blue = 0;
	bool hasActiveStyle = false;
	float deluxeAccum[3];
	float tangent[3];
	float bitangent[3];
	float faceNormal[3];

	if( deluxePlane )
	{
		VectorClear( deluxeAccum );
		tangent[0] = 1.0f; tangent[1] = 0.0f; tangent[2] = 0.0f;
		bitangent[0] = 0.0f; bitangent[1] = 1.0f; bitangent[2] = 0.0f;
		faceNormal[0] = 0.0f; faceNormal[1] = 0.0f; faceNormal[2] = 1.0f;

		if( surface->plane )
		{
			tangent[0] = surface->info->lmvecs[0][0];
			tangent[1] = surface->info->lmvecs[0][1];
			tangent[2] = surface->info->lmvecs[0][2];
			bitangent[0] = -surface->info->lmvecs[1][0];
			bitangent[1] = -surface->info->lmvecs[1][1];
			bitangent[2] = -surface->info->lmvecs[1][2];
			if( surface->flags & SURF_PLANEBACK )
				VectorNegate( surface->plane->normal, faceNormal );
			else
				VectorCopy( surface->plane->normal, faceNormal );
			VectorNormalize( tangent );
			VectorNormalize( bitangent );
			VectorNormalize( faceNormal );
		}
	}

	for( int styleIndex = 0; styleIndex < MAXLIGHTMAPS; ++styleIndex )
	{
		const byte style = surface->styles[styleIndex];
		if( style == 255 )
			break;

		const int styleValue = std::max( 0, R_LightStyleValue( style ) );
		const color24 &sample = samplePlane[texelIndex];
		red += static_cast<int>( sample.r ) * styleValue;
		green += static_cast<int>( sample.g ) * styleValue;
		blue += static_cast<int>( sample.b ) * styleValue;
		hasActiveStyle = true;
		samplePlane += texelCount;

		if( deluxePlane )
		{
			const color24 &dm = deluxePlane[texelIndex];
			const float sx = ( static_cast<float>( dm.r ) - 128.0f ) * ( 1.0f / 128.0f );
			const float sy = ( static_cast<float>( dm.g ) - 128.0f ) * ( 1.0f / 128.0f );
			const float sz = ( static_cast<float>( dm.b ) - 128.0f ) * ( 1.0f / 128.0f );

			const float weight = -static_cast<float>( styleValue );
			deluxeAccum[0] += ( tangent[0] * sx + bitangent[0] * sy + faceNormal[0] * sz ) * weight;
			deluxeAccum[1] += ( tangent[1] * sx + bitangent[1] * sy + faceNormal[1] * sz ) * weight;
			deluxeAccum[2] += ( tangent[2] * sx + bitangent[2] * sy + faceNormal[2] * sz ) * weight;
			deluxePlane += texelCount;
		}
	}

	if( !hasActiveStyle )
	{
		outLight.r = outLight.g = outLight.b = 255;
		if( outLightVec ) VectorClear( outLightVec );
		return true;
	}

	red = std::min( 255, ( red + 128 ) >> 8 );
	green = std::min( 255, ( green + 128 ) >> 8 );
	blue = std::min( 255, ( blue + 128 ) >> 8 );

	const float gamma = std::max( 0.5f, std::min( 4.0f, r_lightmap_gamma.value ) );
	auto applyGamma = [gamma]( int channel ) -> byte
		{
			const float linear = static_cast<float>( channel ) / 255.0f;
			return static_cast<byte>( std::min( 255, static_cast<int>(
				powf( linear, 1.0f / gamma ) * 255.0f + 0.5f ) ) );
		};

	outLight.r = applyGamma( red );
	outLight.g = applyGamma( green );
	outLight.b = applyGamma( blue );
	if( outLightVec )
	{
		if( deluxePlane )
			VectorCopy( deluxeAccum, outLightVec );
		else
			VectorClear( outLightVec );
	}
	return true;
}

void R_CheckLightmapBakeParams( void )
{

	if( r_nolm.value != 0.0f )
		return;

	static float lastLightmapGamma = -1.0f;
	static float lastAmbientOcclusion = -999.0f;
	static float lastAoStrength = -999.0f;
	static float lastAoRadius = -999.0f;
	static float lastAoEdgeStrength = -999.0f;
	static float lastAoCornerStrength = -999.0f;
	static float lastAoMinLight = -999.0f;
	static float lastAoSurfaceBorders = -999.0f;
	static float lastAoTrueEdges = -999.0f;
	static float lastAoTrueEdgeStrength = -999.0f;
	static float lastAoTrueEdgeNeighbor = -999.0f;
	static float lastAoCoplanarSuppress = -999.0f;
	static float lastAoContrast = -999.0f;
	static float lastAoContrastStrength = -999.0f;
	static float lastAoContrastThreshold = -999.0f;
	static float lastAoCache = -999.0f;

	const bool changed =
		fabsf( lastLightmapGamma - r_lightmap_gamma.value ) > 0.0001f ||
		fabsf( lastAmbientOcclusion - dx_ao.value ) > 0.0001f ||
		fabsf( lastAoStrength - dx_ao_strength.value ) > 0.0001f ||
		fabsf( lastAoRadius - dx_ao_radius.value ) > 0.0001f ||
		fabsf( lastAoEdgeStrength - dx_ao_edge_strength.value ) > 0.0001f ||
		fabsf( lastAoCornerStrength - dx_ao_corner_strength.value ) > 0.0001f ||
		fabsf( lastAoMinLight - dx_ao_min_light.value ) > 0.0001f ||
		fabsf( lastAoSurfaceBorders - dx_ao_surface_borders.value ) > 0.0001f ||
		fabsf( lastAoTrueEdges - dx_ao_true_edges.value ) > 0.0001f ||
		fabsf( lastAoTrueEdgeStrength - dx_ao_true_edge_strength.value ) > 0.0001f ||
		fabsf( lastAoTrueEdgeNeighbor - dx_ao_true_edge_neighbor.value ) > 0.0001f ||
		fabsf( lastAoCoplanarSuppress - dx_ao_coplanar_suppress.value ) > 0.0001f ||
		fabsf( lastAoContrast - dx_ao_contrast.value ) > 0.0001f ||
		fabsf( lastAoContrastStrength - dx_ao_contrast_strength.value ) > 0.0001f ||
		fabsf( lastAoContrastThreshold - dx_ao_contrast_threshold.value ) > 0.0001f ||
		fabsf( lastAoCache - dx_ao_cache.value ) > 0.0001f;

	if( !changed )
		return;

	R_RefreshLightmaps( );
	lastLightmapGamma = r_lightmap_gamma.value;
	lastAmbientOcclusion = dx_ao.value;
	lastAoStrength = dx_ao_strength.value;
	lastAoRadius = dx_ao_radius.value;
	lastAoEdgeStrength = dx_ao_edge_strength.value;
	lastAoCornerStrength = dx_ao_corner_strength.value;
	lastAoMinLight = dx_ao_min_light.value;
	lastAoSurfaceBorders = dx_ao_surface_borders.value;
	lastAoTrueEdges = dx_ao_true_edges.value;
	lastAoTrueEdgeStrength = dx_ao_true_edge_strength.value;
	lastAoTrueEdgeNeighbor = dx_ao_true_edge_neighbor.value;
	lastAoCoplanarSuppress = dx_ao_coplanar_suppress.value;
	lastAoContrast = dx_ao_contrast.value;
	lastAoContrastStrength = dx_ao_contrast_strength.value;
	lastAoContrastThreshold = dx_ao_contrast_threshold.value;
	lastAoCache = dx_ao_cache.value;
}

static bool R_DynamicOverlayRangeActive(
	const world_dynamic_overlay_range_t &range )
{
	return range.indexCount >= 3 &&
		range.allocationIndex < g_dlightOverlayActive.size( ) &&
		g_dlightOverlayActive[range.allocationIndex] != 0;
}

static bool R_OverlayBatchActive(
	const world_dynamic_overlay_batch_t &batch )
{
	for( const world_dynamic_overlay_range_t &range : batch.ranges )
	{
		if( R_DynamicOverlayRangeActive( range ) )
			return true;
	}
	return false;
}

static bool R_BuildConveyorTextureMatrix( const cl_entity_s *ent,
	const texture_t *baseTexture, float time, D3DMATRIX &matrix )
{
	float conveyorSpeed = 0.0f;
	if( ent )
	{
		conveyorSpeed = static_cast<float>(
			( static_cast<int>( ent->curstate.rendercolor.g ) << 8 ) |
			static_cast<int>( ent->curstate.rendercolor.b ) ) * ( 1.0f / 16.0f );
		if( ent->curstate.rendercolor.r != 0 )
			conveyorSpeed = -conveyorSpeed;
	}
	else if( gp_host && FBitSet( gp_host->features, ENGINE_QUAKE_COMPATIBLE ) )
	{
		conveyorSpeed = -35.0f;
	}
	else
	{
		return false;
	}

	if( fabsf( conveyorSpeed ) <= 0.0001f )
		return false;

	const int sourceWidthInt = baseTexture
		? static_cast<int>( baseTexture->width ) : 64;
	const float sourceWidth = static_cast<float>(
		sourceWidthInt > 0 ? sourceWidthInt : 1 );
	const float rate = fabsf( conveyorSpeed ) / sourceWidth;
	const float angle = conveyorSpeed >= 0.0f ? static_cast<float>( M_PI ) : 0.0f;
	float sOffset = time * cosf( angle ) * rate;
	float tOffset = time * sinf( angle ) * rate;

	sOffset -= floorf( sOffset );
	tOffset -= floorf( tOffset );

	memset( &matrix, 0, sizeof( matrix ) );
	matrix._11 = 1.0f;
	matrix._22 = 1.0f;
	matrix._33 = 1.0f;
	matrix._44 = 1.0f;
	matrix._31 = sOffset;
	matrix._32 = tOffset;
	return true;
}

static void R_SetConveyorTextureTransform( bool conveyor,
	const cl_entity_s *ent, const texture_t *baseTexture, float time,
	bool &transformEnabled )
{
	if( conveyor )
	{
		D3DMATRIX textureMatrix;
		if( R_BuildConveyorTextureMatrix( ent, baseTexture, time, textureMatrix ) )
		{
			device->SetTransform( D3DTS_TEXTURE0, &textureMatrix );
			DX_SetTextureStageStateCached( 0, D3DTSS_TEXTURETRANSFORMFLAGS,
				D3DTTFF_COUNT2 );
			transformEnabled = true;
			return;
		}
	}

	if( transformEnabled )
	{
		DX_SetTextureStageStateCached( 0, D3DTSS_TEXTURETRANSFORMFLAGS,
			D3DTTFF_DISABLE );
		transformEnabled = false;
	}
}

static void R_DrawDLightOverlay( const world_model_draw_t &model,
	const cl_entity_s *ent, float time )
{

	if( ent || g_dlightTouchedAllocs.empty( ) ||
		model.dynamicOverlayBatches.empty( ) )
	{
		return;
	}

	const bool useSharedGeometry = model.overlaySharedVertexBuffer != nullptr &&
		model.overlaySharedIndexBuffer != nullptr;
	device->SetFVF( LIGHTMAP_FVF );
	if( useSharedGeometry )
	{
		device->SetStreamSource( 0, model.overlaySharedVertexBuffer, 0,
			sizeof( lightmap_vertex_t ) );
		device->SetIndices( model.overlaySharedIndexBuffer );
	}

	DX_SetTextureStageStateCached( 0, D3DTSS_TEXCOORDINDEX, 0 );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
	R_SetLightmapStageEnabled( true );
	DX_SetSamplerStateCached( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
	DX_SetSamplerStateCached( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
	DX_SetSamplerStateCached( 1, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
	DX_SetSamplerStateCached( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
	DX_SetSamplerStateCached( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

	DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_BLENDOP, D3DBLENDOP_ADD );
	DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_ONE );
	DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_ONE );

	int boundBaseTexture = -1;
	IDirect3DTexture9 *boundOverlayTexture = nullptr;
	bool conveyorTransformEnabled = false;
	for( const world_dynamic_overlay_batch_t &batch : model.dynamicOverlayBatches )
	{
		if( batch.primitiveCount == 0 || !R_OverlayBatchActive( batch ) )
			continue;
		if( !R_PVSLeafsVisible( model, batch.pvsLeafs, batch.pvsAlwaysVisible ) )
			continue;
		if( !R_BoundsVisibleInCurrentView( batch.mins, batch.maxs,
			batch.boundsValid ) )
			continue;
		if( batch.overlayAtlasIndex < 0 || batch.overlayAtlasIndex >=
			static_cast<int>( g_mapLightmaps.dynamicOverlayAtlases.size( ) ) )
			continue;

		texture_t *texture = R_SelectAnimatedTexture( batch.baseTexture, nullptr, time );
		IDirect3DTexture9 *overlayTexture =
			g_mapLightmaps.dynamicOverlayAtlases[batch.overlayAtlasIndex].texture;
		if( !texture || texture->gl_texturenum <= 0 || !overlayTexture )
			continue;

		R_SetConveyorTextureTransform( batch.conveyor, ent,
			batch.baseTexture, time, conveyorTransformEnabled );

		if( texture->gl_texturenum != boundBaseTexture )
		{
			DX_Bind( 0, texture->gl_texturenum );
			boundBaseTexture = texture->gl_texturenum;
		}
		if( overlayTexture != boundOverlayTexture )
		{
			DX_BindRawTexture( 1, overlayTexture );
			boundOverlayTexture = overlayTexture;
		}

		UINT runFirstIndex = 0;
		UINT runIndexCount = 0;

		auto FlushActiveRun = [&]( )
			{
				if( runIndexCount < 3 )
				{
					runIndexCount = 0;
					return;
				}

				const UINT primitiveCount = runIndexCount / 3;
				DX_ProfileEvent( DX_PROFILE_EVENT_WORLD_BATCH_DRAWN, 1 );

				if( useSharedGeometry )
				{
					DX_ProfileDraw( true, primitiveCount );
					device->DrawIndexedPrimitive( D3DPT_TRIANGLELIST,
						static_cast<INT>( batch.sharedBaseVertex ), 0,
						batch.vertexCount,
						batch.sharedStartIndex + runFirstIndex,
						primitiveCount );
				}
				else if( !batch.vertices.empty( ) && !batch.indices.empty( ) &&
					runFirstIndex + runIndexCount <= batch.indices.size( ) )
				{
					R_DrawScratchIndexedPrimitive( D3DPT_TRIANGLELIST,
						static_cast<UINT>( batch.vertices.size( ) ),
						primitiveCount,
						batch.indices.data( ) + runFirstIndex, D3DFMT_INDEX32,
						batch.vertices.data( ), sizeof( lightmap_vertex_t ) );
				}

				runIndexCount = 0;
			};

		for( const world_dynamic_overlay_range_t &range : batch.ranges )
		{
			if( !R_DynamicOverlayRangeActive( range ) )
			{
				FlushActiveRun( );
				continue;
			}

			if( runIndexCount == 0 )
			{
				runFirstIndex = range.firstIndex;
				runIndexCount = range.indexCount;
			}
			else if( runFirstIndex + runIndexCount == range.firstIndex )
			{

				runIndexCount += range.indexCount;
			}
			else
			{
				FlushActiveRun( );
				runFirstIndex = range.firstIndex;
				runIndexCount = range.indexCount;
			}
		}
		FlushActiveRun( );
	}

	if( conveyorTransformEnabled )
		DX_SetTextureStageStateCached( 0, D3DTSS_TEXTURETRANSFORMFLAGS,
			D3DTTFF_DISABLE );
	R_SetLightmapStageEnabled( false );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
}

static bool R_BrushEntityUsesLightmap( const world_model_draw_t &model,
	const cl_entity_s *ent )
{
	if( r_nolm.value != 0.0f )
		return false;
	if( !ent )
		return true;
	if( ( ent->curstate.effects & EF_FULLBRIGHT ) != 0 )
		return false;

	switch( ent->curstate.rendermode )
	{
	case kRenderTransTexture:

		return !model.waterBatches.empty( );
	case kRenderTransColor:
	case kRenderTransAdd:
	case kRenderGlow:
		return false;
	default:
		return true;
	}
}

void R_DrawLightmappedBrushModel( const world_model_draw_t &model,
	const cl_entity_s *ent, float time )
{
	DX_MODEL_PROFILE_SCOPE( brushProfileScope, DX_PROFILE_PHASE_BRUSHMODEL );
	if( model.batches.empty( ) )
		return;

	const bool needsEntityState = ent && ent->curstate.rendermode != kRenderNormal;
	dx_entity_state_t entityState{};
	if( needsEntityState )
		R_BeginEntityRenderMode( ent, &entityState );
	else
		DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );

	device->SetFVF( LIGHTMAP_FVF );

	const bool useSharedGeometry =
		model.sharedVertexBuffer != nullptr && model.sharedIndexBuffer != nullptr;
	if( useSharedGeometry )
	{
		device->SetStreamSource( 0, model.sharedVertexBuffer, 0,
			sizeof( lightmap_vertex_t ) );
		device->SetIndices( model.sharedIndexBuffer );
	}

	R_SetLightmapStageEnabled( false );

	DX_SetTextureStageStateCached( 0, D3DTSS_TEXCOORDINDEX, 0 );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );

	const bool useLightmaps = R_BrushEntityUsesLightmap( model, ent );
	int boundBaseTexture = -1;
	IDirect3DTexture9 *boundLightmapTexture = nullptr;
	bool lightmapStageEnabled = false;
	bool conveyorTransformEnabled = false;

	const std::vector<size_t> *cachedStaticBatches =
		!ent ? R_GetVisibleWorldBatches( model ) : nullptr;
	const size_t batchLoopCount = cachedStaticBatches
		? cachedStaticBatches->size( ) : model.batches.size( );
	for( size_t batchLoopIndex = 0; batchLoopIndex < batchLoopCount; ++batchLoopIndex )
	{
		const size_t batchIndex = cachedStaticBatches
			? ( *cachedStaticBatches )[batchLoopIndex] : batchLoopIndex;
		if( batchIndex >= model.batches.size( ) )
			continue;

		const world_material_batch_t &batch = model.batches[batchIndex];
		texture_t *texture = R_SelectAnimatedTexture( batch.baseTexture, ent, time );
		if( !texture || texture->gl_texturenum <= 0 || batch.primitiveCount == 0 )
			continue;

		if( !cachedStaticBatches )
		{
			if( !ent && !R_PVSLeafsVisible( model, batch.pvsLeafs, batch.pvsAlwaysVisible ) )
			{
				DX_ProfileEvent( DX_PROFILE_EVENT_WORLD_CULLED_PVS, 1 );
				continue;
			}

			if( !ent && !R_BoundsVisibleInCurrentView( batch.mins, batch.maxs, batch.boundsValid ) )
			{
				DX_ProfileEvent( DX_PROFILE_EVENT_WORLD_CULLED_FRUSTUM, 1 );
				continue;
			}
		}

		R_SetConveyorTextureTransform( batch.conveyor, ent,
			batch.baseTexture, time, conveyorTransformEnabled );

		if( texture->gl_texturenum != boundBaseTexture )
		{
			DX_Bind( 0, texture->gl_texturenum );

			if( ent && ent->curstate.rendermode != kRenderNormal )
			{
				const int renderMode = ent->curstate.rendermode;
				const bool masked = R_BrushTextureNeedsAlphaTest( texture );

				if( renderMode == kRenderTransAlpha )
				{
					DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
					DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, TRUE );
					DX_SetRenderStateCached( D3DRS_ALPHAFUNC, D3DCMP_GREATER );
					DX_SetRenderStateCached( D3DRS_ALPHAREF, 127 );
					DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
				}
				else
				{
					R_SetAlphaTestEnabled( masked );
					DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
				}
			}
			else
			{
				R_SetAlphaTestEnabled( false );
			}

			boundBaseTexture = texture->gl_texturenum;
		}

		IDirect3DTexture9 *lightmapTexture = nullptr;
		if( useLightmaps && batch.atlasIndex >= 0 &&
			batch.atlasIndex < static_cast<int>( g_mapLightmaps.atlases.size( ) ) )
		{
			lightmapTexture = g_mapLightmaps.atlases[batch.atlasIndex].texture;
		}

		if( lightmapTexture )
		{
			if( !lightmapStageEnabled )
			{
				R_SetLightmapStageEnabled( true );
				DX_SetSamplerStateCached( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
				DX_SetSamplerStateCached( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
				DX_SetSamplerStateCached( 1, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
				DX_SetSamplerStateCached( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
				DX_SetSamplerStateCached( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
				lightmapStageEnabled = true;
			}

			if( boundLightmapTexture != lightmapTexture )
			{
				DX_BindRawTexture( 1, lightmapTexture );
				boundLightmapTexture = lightmapTexture;
			}
		}
		else if( lightmapStageEnabled )
		{
			R_SetLightmapStageEnabled( false );
			lightmapStageEnabled = false;
			boundLightmapTexture = nullptr;
		}

		if( useSharedGeometry )
		{
			DX_ProfileEvent( DX_PROFILE_EVENT_WORLD_BATCH_DRAWN, 1 );
			DX_ProfileDraw( true, batch.primitiveCount );
			device->DrawIndexedPrimitive( D3DPT_TRIANGLELIST,
				static_cast<INT>( batch.sharedBaseVertex ), 0,
				batch.vertexCount, batch.sharedStartIndex,
				batch.primitiveCount );
		}
		else if( batch.vertexBuffer && batch.indexBuffer )
		{
			DX_ProfileEvent( DX_PROFILE_EVENT_WORLD_BATCH_DRAWN, 1 );
			device->SetStreamSource( 0, batch.vertexBuffer, 0,
				sizeof( lightmap_vertex_t ) );
			device->SetIndices( batch.indexBuffer );
			DX_ProfileDraw( true, batch.primitiveCount );
			device->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0,
				batch.vertexCount, 0, batch.primitiveCount );
		}
		else if( !batch.vertices.empty( ) && !batch.indices.empty( ) )
		{
			DX_ProfileEvent( DX_PROFILE_EVENT_WORLD_BATCH_DRAWN, 1 );

			R_DrawScratchIndexedPrimitive( D3DPT_TRIANGLELIST,
				static_cast<UINT>( batch.vertices.size( ) ),
				static_cast<UINT>( batch.indices.size( ) / 3 ),
				batch.indices.data( ), D3DFMT_INDEX32,
				batch.vertices.data( ), sizeof( lightmap_vertex_t ) );
		}
	}

	if( conveyorTransformEnabled )
	{
		DX_SetTextureStageStateCached( 0, D3DTSS_TEXTURETRANSFORMFLAGS,
			D3DTTFF_DISABLE );
		conveyorTransformEnabled = false;
	}

	if( useLightmaps )
	{
		R_DrawDLightOverlay( model, ent, time );
		R_DrawDynamicLightBrushPass( model, ent, time );
	}

	R_SetLightmapStageEnabled( false );
	device->SetFVF( BRUSH_FVF );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );

	if( needsEntityState )
		R_RestoreEntityRenderMode( &entityState );
}

void R_DrawWaterIndexed( world_water_batch_t &batch )
{
	if( batch.primitiveCount == 0 )
		return;

	if( batch.vertexBuffer && batch.indexBuffer )
	{
		DX_ProfileEvent( DX_PROFILE_EVENT_WATER_BATCH_DRAWN, 1 );
		device->SetStreamSource( 0, batch.vertexBuffer, 0, sizeof( brush_vertex_t ) );
		device->SetIndices( batch.indexBuffer );
		DX_ProfileDraw( true, batch.primitiveCount );
		DX_ProfileEvent( DX_PROFILE_EVENT_STUDIO_BATCH_DRAWN, 1 );
		device->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0,
			batch.vertexCount, 0, batch.primitiveCount );
		return;
	}

	if( !batch.dynamicVertices.empty( ) && !batch.indices.empty( ) )
	{
		DX_ProfileEvent( DX_PROFILE_EVENT_WATER_BATCH_DRAWN, 1 );
		R_DrawScratchIndexedPrimitive( D3DPT_TRIANGLELIST,
			static_cast<UINT>( batch.dynamicVertices.size( ) ),
			static_cast<UINT>( batch.indices.size( ) / 3 ),
			batch.indices.data( ), D3DFMT_INDEX32,
			batch.dynamicVertices.data( ), sizeof( brush_vertex_t ) );
	}
}
