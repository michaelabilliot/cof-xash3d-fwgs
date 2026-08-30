#include "dx_textures.h"
#include "enginefeatures.h"
#include "dx_local.h"
#include <cstring>
#include <limits>

std::deque<dxtexture_t> g_Textures( 1 );
int g_texcount = 0;
dxfast::HashMap<std::string, int> g_TextureIndex;
dxfast::HashMap<std::string, int> g_StudioTexIndex;

static std::vector<int> g_freeTextureIds;

static int DX_AllocateTextureId( void )
{
	if( !g_freeTextureIds.empty( ) )
	{
		const int textureId = g_freeTextureIds.back( );
		g_freeTextureIds.pop_back( );
		g_Textures[textureId] = {};
		return textureId;
	}

	const size_t textureId = g_Textures.size( );
	if( textureId > static_cast<size_t>( std::numeric_limits<int>::max( ) ) )
	{
		return 0;
	}

	g_Textures.emplace_back( );
	g_texcount = static_cast<int>( textureId );
	return g_texcount;
}

static constexpr unsigned int DX_BOUND_TEX_INVALID = 0xFFFFFFFFu;
static constexpr unsigned int DX_BOUND_TEX_RAW = 0xFFFFFFFEu;
static constexpr int DX_MAX_TEXTURE_STAGES = 8;
static unsigned int g_dxBoundTextureId[DX_MAX_TEXTURE_STAGES] =
{
	DX_BOUND_TEX_INVALID, DX_BOUND_TEX_INVALID, DX_BOUND_TEX_INVALID, DX_BOUND_TEX_INVALID,
	DX_BOUND_TEX_INVALID, DX_BOUND_TEX_INVALID, DX_BOUND_TEX_INVALID, DX_BOUND_TEX_INVALID
};
static IDirect3DBaseTexture9 *g_dxBoundRawTexture[DX_MAX_TEXTURE_STAGES] = {};

static constexpr unsigned int DX_SAMPLER_PROFILE_INVALID = 0xFFFFFFFFu;
static constexpr unsigned int DX_SAMPLER_LINEAR_WRAP = 0u;
static constexpr unsigned int DX_SAMPLER_LINEAR_CLAMP = 1u;
static constexpr unsigned int DX_SAMPLER_NEAREST_WRAP = 2u;
static constexpr unsigned int DX_SAMPLER_NEAREST_CLAMP = 3u;
static unsigned int g_dxSamplerProfile[DX_MAX_TEXTURE_STAGES] =
{
	DX_SAMPLER_PROFILE_INVALID, DX_SAMPLER_PROFILE_INVALID, DX_SAMPLER_PROFILE_INVALID, DX_SAMPLER_PROFILE_INVALID,
	DX_SAMPLER_PROFILE_INVALID, DX_SAMPLER_PROFILE_INVALID, DX_SAMPLER_PROFILE_INVALID, DX_SAMPLER_PROFILE_INVALID
};

static DWORD g_dxGlobalMaxAnisotropy = 1;

void DX_SetGlobalAnisotropy( int maxAnisotropy )
{
	DWORD resolved = 1;
	if( maxAnisotropy > 1 )
		resolved = static_cast<DWORD>( std::min( 16, maxAnisotropy ) );

	if( resolved == g_dxGlobalMaxAnisotropy )
		return;

	g_dxGlobalMaxAnisotropy = resolved;
	for( int i = 0; i < DX_MAX_TEXTURE_STAGES; ++i )
		g_dxSamplerProfile[i] = DX_SAMPLER_PROFILE_INVALID;
}

static inline unsigned int DX_TextureSamplerProfileKey( const dxtexture_t &texture )
{
	const bool nearest = FBitSet( texture.flags, TF_NEAREST );
	const bool clamp = FBitSet( texture.flags, TF_CLAMP );
	const bool anisotropic = !nearest && g_dxGlobalMaxAnisotropy > 1;
	return ( nearest ? 2u : 0u ) | ( clamp ? 1u : 0u ) |
		( anisotropic ? ( g_dxGlobalMaxAnisotropy << 4 ) : 0u );
}

static inline void DX_InvalidateSamplerProfile( DWORD sampler )
{
	if( sampler < DX_MAX_TEXTURE_STAGES )
		g_dxSamplerProfile[sampler] = DX_SAMPLER_PROFILE_INVALID;
}

static void DX_ApplySamplerProfile( DWORD sampler, unsigned int profile )
{
	if( sampler >= DX_MAX_TEXTURE_STAGES || g_dxSamplerProfile[sampler] == profile )
		return;

	const bool nearest = ( profile & 2u ) != 0u;
	const bool clamp = ( profile & 1u ) != 0u;
	const DWORD anisotropy = profile >> 4;
	const bool anisotropic = !nearest && anisotropy > 1;

	DX_SetSamplerStateCached( sampler, D3DSAMP_MAGFILTER, nearest ? D3DTEXF_POINT : D3DTEXF_LINEAR );
	DX_SetSamplerStateCached( sampler, D3DSAMP_MINFILTER, nearest ? D3DTEXF_POINT :
		( anisotropic ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR ) );
	DX_SetSamplerStateCached( sampler, D3DSAMP_MIPFILTER, nearest ? D3DTEXF_NONE : D3DTEXF_LINEAR );
	DX_SetSamplerStateCached( sampler, D3DSAMP_MAXANISOTROPY, anisotropic ? anisotropy : 1 );
	DX_SetSamplerStateCached( sampler, D3DSAMP_ADDRESSU, clamp ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP );
	DX_SetSamplerStateCached( sampler, D3DSAMP_ADDRESSV, clamp ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP );

	g_dxSamplerProfile[sampler] = profile;
}

static constexpr int DX_RENDER_STATE_CACHE_SIZE = 256;
static constexpr int DX_STAGE_STATE_CACHE_SIZE = 64;
static constexpr int DX_SAMPLER_STATE_CACHE_SIZE = 32;
static bool g_dxRenderStateValid[DX_RENDER_STATE_CACHE_SIZE] = {};
static DWORD g_dxRenderStateValue[DX_RENDER_STATE_CACHE_SIZE] = {};
static bool g_dxStageStateValid[DX_MAX_TEXTURE_STAGES][DX_STAGE_STATE_CACHE_SIZE] = {};
static DWORD g_dxStageStateValue[DX_MAX_TEXTURE_STAGES][DX_STAGE_STATE_CACHE_SIZE] = {};
static bool g_dxSamplerStateValid[DX_MAX_TEXTURE_STAGES][DX_SAMPLER_STATE_CACHE_SIZE] = {};
static DWORD g_dxSamplerStateValue[DX_MAX_TEXTURE_STAGES][DX_SAMPLER_STATE_CACHE_SIZE] = {};

void DX_InvalidateD3DStateCache( void )
{
	memset( g_dxRenderStateValid, 0, sizeof( g_dxRenderStateValid ) );
	memset( g_dxStageStateValid, 0, sizeof( g_dxStageStateValid ) );
	memset( g_dxSamplerStateValid, 0, sizeof( g_dxSamplerStateValid ) );
	for( int i = 0; i < DX_MAX_TEXTURE_STAGES; ++i )
		g_dxSamplerProfile[i] = DX_SAMPLER_PROFILE_INVALID;
}

HRESULT DX_SetRenderStateCached( D3DRENDERSTATETYPE state, DWORD value )
{
	if( !device )
		return D3DERR_INVALIDCALL;

	const unsigned int index = static_cast<unsigned int>( state );
	if( index < DX_RENDER_STATE_CACHE_SIZE &&
		g_dxRenderStateValid[index] && g_dxRenderStateValue[index] == value )
	{
		DX_ProfileState( DX_PROFILE_STATE_RENDER, false );
		return D3D_OK;
	}

	DX_ProfileState( DX_PROFILE_STATE_RENDER, true );
	const HRESULT result = device->SetRenderState( state, value );
	if( SUCCEEDED( result ) && index < DX_RENDER_STATE_CACHE_SIZE )
	{
		g_dxRenderStateValid[index] = true;
		g_dxRenderStateValue[index] = value;
	}
	return result;
}

HRESULT DX_SetTextureStageStateCached( DWORD stage,
	D3DTEXTURESTAGESTATETYPE type, DWORD value )
{
	if( !device )
		return D3DERR_INVALIDCALL;

	const unsigned int typeIndex = static_cast<unsigned int>( type );
	if( stage < DX_MAX_TEXTURE_STAGES && typeIndex < DX_STAGE_STATE_CACHE_SIZE &&
		g_dxStageStateValid[stage][typeIndex] &&
		g_dxStageStateValue[stage][typeIndex] == value )
	{
		DX_ProfileState( DX_PROFILE_STATE_STAGE, false );
		return D3D_OK;
	}

	DX_ProfileState( DX_PROFILE_STATE_STAGE, true );
	const HRESULT result = device->SetTextureStageState( stage, type, value );
	if( SUCCEEDED( result ) && stage < DX_MAX_TEXTURE_STAGES &&
		typeIndex < DX_STAGE_STATE_CACHE_SIZE )
	{
		g_dxStageStateValid[stage][typeIndex] = true;
		g_dxStageStateValue[stage][typeIndex] = value;
	}
	return result;
}

HRESULT DX_SetSamplerStateCached( DWORD sampler, D3DSAMPLERSTATETYPE type,
	DWORD value )
{
	if( !device )
		return D3DERR_INVALIDCALL;

	const unsigned int typeIndex = static_cast<unsigned int>( type );
	if( sampler < DX_MAX_TEXTURE_STAGES && typeIndex < DX_SAMPLER_STATE_CACHE_SIZE &&
		g_dxSamplerStateValid[sampler][typeIndex] &&
		g_dxSamplerStateValue[sampler][typeIndex] == value )
	{
		DX_ProfileState( DX_PROFILE_STATE_SAMPLER, false );
		return D3D_OK;
	}

	DX_InvalidateSamplerProfile( sampler );
	DX_ProfileState( DX_PROFILE_STATE_SAMPLER, true );
	const HRESULT result = device->SetSamplerState( sampler, type, value );
	if( SUCCEEDED( result ) && sampler < DX_MAX_TEXTURE_STAGES &&
		typeIndex < DX_SAMPLER_STATE_CACHE_SIZE )
	{
		g_dxSamplerStateValid[sampler][typeIndex] = true;
		g_dxSamplerStateValue[sampler][typeIndex] = value;
	}
	return result;
}

void DX_InvalidateTextureBindCache( void )
{
	for( int i = 0; i < DX_MAX_TEXTURE_STAGES; ++i )
	{
		g_dxBoundTextureId[i] = DX_BOUND_TEX_INVALID;
		g_dxBoundRawTexture[i] = nullptr;
	}
	DX_InvalidateD3DStateCache( );
}

void DX_InvalidateTextureUnit( int tmu )
{
	if( tmu < 0 || tmu >= DX_MAX_TEXTURE_STAGES )
		return;
	g_dxBoundTextureId[tmu] = DX_BOUND_TEX_INVALID;
	g_dxBoundRawTexture[tmu] = nullptr;
}

void DX_BindRawTexture( int tmu, IDirect3DBaseTexture9 *texture )
{
	if( !device || tmu < 0 || tmu >= DX_MAX_TEXTURE_STAGES )
		return;

	if( g_dxBoundTextureId[tmu] == DX_BOUND_TEX_RAW &&
		g_dxBoundRawTexture[tmu] == texture )
	{
		DX_ProfileTexture( DX_PROFILE_TEXTURE_RAW, false );
		return;
	}

	DX_ProfileTexture( DX_PROFILE_TEXTURE_RAW, true );
	device->SetTexture( static_cast<DWORD>( tmu ), texture );
	g_dxBoundTextureId[tmu] = DX_BOUND_TEX_RAW;
	g_dxBoundRawTexture[tmu] = texture;
}

void get_texture_name( const char *path, char *out, size_t out_size )
{
	if( !out || out_size == 0 )
		return;
	out[0] = '\0';
	if( !path )
		return;

	const char *name = strrchr( path, '/' );
	name = name ? name + 1 : path;

	if( *name == '-' )
	{
		name++;
		while( isdigit( static_cast<unsigned char>( *name ) ) )
			name++;
	}

	const char *dot = strrchr( name, '.' );
	size_t len = dot ? static_cast<size_t>( dot - name ) : strlen( name );
	len = std::min( len, out_size - 1 );
	memcpy( out, name, len );
	out[len] = '\0';
}

const char *DX_TextureName( unsigned int texnum )
{
	if( texnum == 0 || static_cast<size_t>( texnum ) >= g_Textures.size( ) )
		return "";
	return g_Textures[texnum].name;
}

int DX_FindTexture( const char *name )
{
	if( !name || !name[0] )
		return 0;

	const auto found = g_TextureIndex.find( name );
	return found != g_TextureIndex.end( ) ? found->second : 0;
}

static bool DX_PrepareImageForD3DUpload( dxtexture_t *texture, rgbdata_t **image )
{
	if( !texture || !image || !( *image ) || !( *image )->buffer )
		return false;

	rgbdata_t *pic = *image;
	uint imageFlags = IMAGE_FORCE_RGBA;

	memcpy( texture->fogParams, pic->fogParams, sizeof( texture->fogParams ) );

	if( FBitSet( texture->flags, TF_FORCE_COLOR ) )
		SetBits( pic->flags, IMAGE_HAS_COLOR );

	texture->encode = pic->encode;

	if( ImageCompressed( pic->type ) )
	{

		ClearBits( texture->flags, TF_KEEP_SOURCE );
	}
	else
	{
		if( FBitSet( pic->flags, IMAGE_HAS_LUMA ) )
			SetBits( texture->flags, TF_HAS_LUMA );
		if( FBitSet( pic->flags, IMAGE_QUAKEPAL ) )
			SetBits( texture->flags, TF_QUAKEPAL );

		if( FBitSet( texture->flags, TF_MAKELUMA ) )
		{
			SetBits( imageFlags, IMAGE_MAKE_LUMA );
			ClearBits( texture->flags, TF_MAKELUMA );
		}

		if( !FBitSet( texture->flags, TF_IMG_UPLOADED ) &&
			FBitSet( texture->flags, TF_KEEP_SOURCE ) && !texture->original )
		{
			texture->original = gEngfuncs.FS_CopyImage( pic );
		}
	}

	gEngfuncs.Image_Process( &pic, 0, 0, imageFlags, 0 );
	if( !pic || !pic->buffer || pic->type != PF_RGBA_32 )
	{
		gEngfuncs.Con_DPrintf( "D3D9: unable to convert texture to RGBA\n" );
		return false;
	}

	if( FBitSet( pic->flags, IMAGE_PLAYERDECAL ) &&
		FBitSet( pic->flags, IMAGE_HAS_ALPHA ) &&
		!FBitSet( pic->flags, IMAGE_PREMULTIPLIED ) )
	{
		const int count = pic->width * pic->height;
		byte *pixel = pic->buffer;
		for( int i = 0; i < count; ++i, pixel += 4 )
		{
			const int alpha = pixel[3];
			pixel[0] = static_cast<byte>( ( pixel[0] * alpha + 127 ) / 255 );
			pixel[1] = static_cast<byte>( ( pixel[1] * alpha + 127 ) / 255 );
			pixel[2] = static_cast<byte>( ( pixel[2] * alpha + 127 ) / 255 );
		}
		SetBits( pic->flags, IMAGE_PREMULTIPLIED );
		SetBits( texture->flags, TF_PREMULTIPLIED );
	}

	if( FBitSet( texture->flags, TF_LUMINANCE ) )
		ClearBits( pic->flags, IMAGE_HAS_COLOR );

	const bool hasAlpha = FBitSet( pic->flags, IMAGE_HAS_ALPHA );
	if( hasAlpha )
		SetBits( texture->flags, TF_HAS_ALPHA );
	else
		ClearBits( texture->flags, TF_HAS_ALPHA );

	texture->has_alpha = hasAlpha ? 1 : 0;
	*image = pic;
	return true;
}

enum class dx_alpha_storage_mode_t
{
	Opaque,
	Source,
	LegacyRgbKey,
};

static dx_alpha_storage_mode_t DX_ChooseAlphaStorageMode( const rgbdata_t *pic,
	bool imageDeclaresAlpha )
{
	if( imageDeclaresAlpha )
		return dx_alpha_storage_mode_t::Source;

	if( !pic || !pic->buffer || pic->type != PF_RGBA_32 ||
		pic->width <= 0 || pic->height <= 0 )
	{
		return dx_alpha_storage_mode_t::Opaque;
	}

	bool sawZero = false;
	bool sawNonZero = false;
	bool sawNon255 = false;
	const size_t pixelCount = static_cast<size_t>( pic->width ) *
		static_cast<size_t>( pic->height );
	const byte *pixel = pic->buffer;

	for( size_t i = 0; i < pixelCount; ++i, pixel += 4 )
	{
		const byte alpha = pixel[3];
		sawZero |= alpha == 0;
		sawNonZero |= alpha != 0;
		sawNon255 |= alpha != 255;

		if( sawZero && sawNonZero )
			return dx_alpha_storage_mode_t::Source;
	}

	if( sawNonZero && sawNon255 )
		return dx_alpha_storage_mode_t::Source;

	if( sawZero )
		return dx_alpha_storage_mode_t::LegacyRgbKey;

	return dx_alpha_storage_mode_t::Opaque;
}

static bool DX_UploadRGBAImage( IDirect3DTexture9 *texture,
	const rgbdata_t *pic, dx_alpha_storage_mode_t alphaMode )
{
	if( !texture || !pic || !pic->buffer || pic->type != PF_RGBA_32 )
		return false;

	D3DLOCKED_RECT locked;
	if( FAILED( texture->LockRect( 0, &locked, nullptr, 0 ) ) )
		return false;

	for( int y = 0; y < pic->height; ++y )
	{
		const byte *source = pic->buffer +
			static_cast<size_t>( y ) * pic->width * 4;
		byte *destination = static_cast<byte *>( locked.pBits ) +
			static_cast<size_t>( y ) * locked.Pitch;

		for( int x = 0; x < pic->width; ++x )
		{
			destination[0] = source[2];
			destination[1] = source[1];
			destination[2] = source[0];

			switch( alphaMode )
			{
			case dx_alpha_storage_mode_t::Source:
				destination[3] = source[3];
				break;
			case dx_alpha_storage_mode_t::LegacyRgbKey:

				destination[3] = std::max( source[0],
					std::max( source[1], source[2] ) );
				break;
			case dx_alpha_storage_mode_t::Opaque:
			default:
				destination[3] = 255;
				break;
			}

			source += 4;
			destination += 4;
		}
	}

	texture->UnlockRect( 0 );
	return true;
}

static int DX_CreateTextureFromImage( const char *cacheName, int existingId,
	rgbdata_t **image, texFlags_t flags )
{
	if( !cacheName || !cacheName[0] || !image || !( *image ) || !device )
		return 0;

	dxtexture_t uploadInfo{};
	uploadInfo.flags = flags;
	if( !DX_PrepareImageForD3DUpload( &uploadInfo, image ) )
	{
		if( uploadInfo.original )
			gEngfuncs.FS_FreeImage( uploadInfo.original );
		return 0;
	}

	rgbdata_t *pic = *image;
	IDirect3DTexture9 *newTexture = nullptr;
	const HRESULT createResult = device->CreateTexture(
		pic->width, pic->height, 1, 0, D3DFMT_A8R8G8B8,
		D3DPOOL_MANAGED, &newTexture, nullptr );
	if( FAILED( createResult ) || !newTexture )
	{
		if( uploadInfo.original )
			gEngfuncs.FS_FreeImage( uploadInfo.original );
		gEngfuncs.Con_DPrintf( "D3D9: failed to create image texture %s\n",
			cacheName );
		return 0;
	}

	const dx_alpha_storage_mode_t alphaMode = DX_ChooseAlphaStorageMode( pic,
		uploadInfo.has_alpha != 0 );

	if( !DX_UploadRGBAImage( newTexture, pic, alphaMode ) )
	{
		newTexture->Release( );
		if( uploadInfo.original )
			gEngfuncs.FS_FreeImage( uploadInfo.original );
		gEngfuncs.Con_DPrintf( "D3D9: failed to upload image texture %s\n",
			cacheName );
		return 0;
	}

	const bool isNew = existingId <= 0;
	int textureId = existingId;
	if( isNew )
	{
		textureId = DX_AllocateTextureId( );
		if( textureId <= 0 )
		{
			newTexture->Release( );
			if( uploadInfo.original )
				gEngfuncs.FS_FreeImage( uploadInfo.original );
			gEngfuncs.Con_DPrintf( "D3D9: texture table is full\n" );
			return 0;
		}
	}

	dxtexture_t &texture = g_Textures[textureId];
	IDirect3DTexture9 *oldTexture = texture.tex;
	rgbdata_t *oldOriginal = texture.original;

	texture.tex = newTexture;
	texture.flags = uploadInfo.flags;
	texture.encode = uploadInfo.encode;
	memcpy( texture.fogParams, uploadInfo.fogParams, sizeof( texture.fogParams ) );
	texture.width = pic->width;
	texture.height = pic->height;
	texture.srcWidth = pic->width;
	texture.srcHeight = pic->height;
	texture.has_alpha = uploadInfo.has_alpha;
	texture.original = uploadInfo.original;

	if( isNew )
	{
		memset( texture.name, 0, sizeof( texture.name ) );
		strncpy( texture.name, cacheName, sizeof( texture.name ) - 1 );
		texture.texnum = textureId;
		auto indexIt = g_TextureIndex.find( cacheName );
		if( indexIt != g_TextureIndex.end( ) )
			indexIt->second = textureId;
		else
			g_TextureIndex.emplace( cacheName, textureId );
	}

	if( oldTexture )
		oldTexture->Release( );
	if( oldOriginal )
		gEngfuncs.FS_FreeImage( oldOriginal );

	return textureId;
}

int DX_LoadTextureFromBuffer( const char *name, rgbdata_t *pic, texFlags_t flags,
	qboolean update )
{
	if( !name || !name[0] || !pic )
		return 0;

	int existingId = DX_FindTexture( name );

	char texName[64]{};
	get_texture_name( name, texName, sizeof( texName ) );
	if( !existingId )
		existingId = DX_FindTexture( texName );

	if( existingId > 0 && !update )
		return existingId;

	rgbdata_t *upload = gEngfuncs.FS_CopyImage( pic );
	if( !upload )
		return 0;

	const char *cacheName = existingId > 0
		? g_Textures[existingId].name : texName;
	const int id = DX_CreateTextureFromImage( cacheName, existingId, &upload,
		flags );
	gEngfuncs.FS_FreeImage( upload );
	return id;
}

int DX_LoadTexture( const char *name, const byte *buf, size_t size, int flags )
{
	if( !name || !name[0] )
		return 0;

	const int existingId = DX_FindTexture( name );
	if( existingId )
		return existingId;

	uint picFlags = 0;
	if( FBitSet( flags, TF_NOFLIP_TGA ) )
		SetBits( picFlags, IL_DONTFLIP_TGA );
	if( FBitSet( flags, TF_KEEP_SOURCE ) &&
		!FBitSet( flags, TF_EXPAND_SOURCE ) )
	{
		SetBits( picFlags, IL_KEEP_8BIT );
	}

	gEngfuncs.Image_SetForceFlags( picFlags );
	rgbdata_t *pic = gEngfuncs.FS_LoadImage( name, buf, size );
	if( !pic )
		return 0;

	const int id = DX_CreateTextureFromImage( name, 0, &pic,
		static_cast<texFlags_t>( flags ) );
	gEngfuncs.FS_FreeImage( pic );
	return id;
}

int DX_CreateTexture( const char *name, int width, int height,
	const void *buffer, texFlags_t flags )
{

	if( !name || !name[0] || !buffer || width <= 0 || height <= 0 || !device )
		return 0;

	int textureId = DX_FindTexture( name );
	if( textureId > 0 && !FBitSet( flags, TF_UPDATE ) )
		return textureId;

	const bool sourceHasAlpha = FBitSet( flags, TF_HAS_ALPHA ) != 0;
	const int sourceBytesPerPixel = sourceHasAlpha ? 4 : 3;

	IDirect3DTexture9 *newTexture = nullptr;
	const HRESULT createResult = device->CreateTexture(
		width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
		&newTexture, nullptr );
	if( FAILED( createResult ) || !newTexture )
	{
		gEngfuncs.Con_DPrintf( "D3D9: failed to create raw texture %s\n", name );
		return 0;
	}

	D3DLOCKED_RECT locked;
	if( FAILED( newTexture->LockRect( 0, &locked, nullptr, 0 ) ) )
	{
		newTexture->Release( );
		return 0;
	}

	const byte *source = static_cast<const byte *>( buffer );
	for( int y = 0; y < height; ++y )
	{
		byte *destination = static_cast<byte *>( locked.pBits ) +
			static_cast<size_t>( y ) * locked.Pitch;

		for( int x = 0; x < width; ++x )
		{

			destination[0] = source[2];
			destination[1] = source[1];
			destination[2] = source[0];
			destination[3] = sourceHasAlpha ? source[3] : 255;
			source += sourceBytesPerPixel;
			destination += 4;
		}
	}
	newTexture->UnlockRect( 0 );

	if( textureId <= 0 )
	{
		textureId = DX_AllocateTextureId( );
		if( textureId <= 0 )
		{
			newTexture->Release( );
			return 0;
		}
	}

	dxtexture_t &texture = g_Textures[textureId];
	IDirect3DTexture9 *oldTexture = texture.tex;
	rgbdata_t *oldOriginal = texture.original;
	texture.tex = newTexture;
	texture.flags = flags;
	texture.original = nullptr;
	texture.encode = 0;
	memset( texture.fogParams, 0, sizeof( texture.fogParams ) );
	texture.width = width;
	texture.height = height;
	texture.srcWidth = width;
	texture.srcHeight = height;
	texture.has_alpha = sourceHasAlpha ? 1 : 0;
	texture.texnum = textureId;
	if( !texture.name[0] )
	{
		strncpy( texture.name, name, sizeof( texture.name ) - 1 );
		texture.name[sizeof( texture.name ) - 1] = '\0';
	}
	auto indexIt = g_TextureIndex.find( name );
	if( indexIt != g_TextureIndex.end( ) )
		indexIt->second = textureId;
	else
		g_TextureIndex.emplace( name, textureId );

	if( oldTexture )
		oldTexture->Release( );
	if( oldOriginal )
		gEngfuncs.FS_FreeImage( oldOriginal );
	return textureId;
}

static bool DX_EnsureTextureStorage( dxtexture_t &texture, int width, int height )
{
	if( width <= 0 || height <= 0 || !device )
		return false;

	if( texture.tex && texture.width == width && texture.height == height )
		return true;

	if( texture.tex )
	{
		texture.tex->Release( );
		texture.tex = nullptr;
	}

	const HRESULT result = device->CreateTexture(
		width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
		&texture.tex, nullptr );
	if( FAILED( result ) || !texture.tex )
	{
		gEngfuncs.Con_DPrintf( "D3D9: unable to allocate dynamic texture %dx%d\\n",
			width, height );
		return false;
	}

	texture.width = width;
	texture.height = height;
	return true;
}

qboolean DX_UpdateTexture( int texnum, int cols, int rows, int width, int height,
	const byte *buffer, pixformat_t fmt )
{
	if( !buffer || texnum <= 0 || texnum > g_texcount )
		return false;

	dxtexture_t &texture = g_Textures[texnum];
	if( width <= 0 ) width = texture.width;
	if( height <= 0 ) height = texture.height;
	if( cols <= 0 ) cols = width;
	if( rows <= 0 ) rows = height;

	if( !DX_EnsureTextureStorage( texture, width, height ) )
		return false;

	cols = std::min( cols, width );
	rows = std::min( rows, height );
	if( cols <= 0 || rows <= 0 )
		return false;

	RECT updateRect;
	updateRect.left = 0;
	updateRect.top = 0;
	updateRect.right = cols;
	updateRect.bottom = rows;

	D3DLOCKED_RECT locked;
	if( FAILED( texture.tex->LockRect( 0, &locked, &updateRect, 0 ) ) )
		return false;

	const int sourceBytesPerPixel = fmt == PF_RGBA_32 ? 4 : 3;
	for( int y = 0; y < rows; ++y )
	{
		const byte *source = buffer + static_cast<size_t>( y ) * cols * sourceBytesPerPixel;
		byte *dest = static_cast<byte *>( locked.pBits ) + y * locked.Pitch;

		for( int x = 0; x < cols; ++x )
		{
			const byte red = source[0];
			const byte green = source[1];
			const byte blue = source[2];
			const byte alpha = sourceBytesPerPixel == 4 ? source[3] : 255;

			dest[0] = blue;
			dest[1] = green;
			dest[2] = red;
			dest[3] = alpha;
			source += sourceBytesPerPixel;
			dest += 4;
		}
	}

	texture.tex->UnlockRect( 0 );
	texture.srcWidth = width;
	texture.srcHeight = height;
	if( fmt == PF_RGBA_32 )
		texture.has_alpha = 1;
	return true;
}

void DX_ProcessTexture( int texnum, float gamma, int topColor, int bottomColor )
{
	if( texnum <= 0 || static_cast<size_t>( texnum ) >= g_Textures.size( ) )
		return;

	dxtexture_t &texture = g_Textures[texnum];
	uint imageFlags = 0;

	if( gamma != -1.0f )
	{
		imageFlags = IMAGE_LIGHTGAMMA;
	}
	else if( topColor != -1 && bottomColor != -1 )
	{
		imageFlags = IMAGE_REMAP;
	}
	else
	{
		gEngfuncs.Con_Printf( "D3D9: bad texture processing operation for %s\n",
			texture.name );
		return;
	}

	if( !texture.original )
	{
		gEngfuncs.Con_Printf( "D3D9: no source data for texture %s\n",
			texture.name );
		return;
	}

	if( ImageCompressed( texture.original->type ) )
	{
		gEngfuncs.Con_Printf( "D3D9: can't process compressed texture %s\n",
			texture.name );
		return;
	}

	rgbdata_t *pic = gEngfuncs.FS_CopyImage( texture.original );
	if( !pic )
		return;

	if( pic->type == PF_INDEXED_24 || pic->type == PF_INDEXED_32 )
		SetBits( imageFlags, IMAGE_FORCE_RGBA );

	gEngfuncs.Image_Process( &pic, topColor, bottomColor, imageFlags, 0.0f );

	if( !pic || !DX_PrepareImageForD3DUpload( &texture, &pic ) )
	{
		if( pic )
			gEngfuncs.FS_FreeImage( pic );
		return;
	}

	if( DX_EnsureTextureStorage( texture, pic->width, pic->height ) )
	{
		const dx_alpha_storage_mode_t alphaMode =
			DX_ChooseAlphaStorageMode( pic, texture.has_alpha != 0 );

		if( DX_UploadRGBAImage( texture.tex, pic, alphaMode ) )
		{
			texture.srcWidth = pic->width;
			texture.srcHeight = pic->height;
		}
	}

	gEngfuncs.FS_FreeImage( pic );
}

static bool DX_CheckedAdd( size_t a, size_t b, size_t &out )
{
	if( a > std::numeric_limits<size_t>::max( ) - b )
		return false;
	out = a + b;
	return true;
}

static bool DX_CheckedMul( size_t a, size_t b, size_t &out )
{
	if( a != 0 && b > std::numeric_limits<size_t>::max( ) / a )
		return false;
	out = a * b;
	return true;
}

static size_t DX_BoundedStringLength( const char *text, size_t capacity )
{
	if( !text )
		return 0;
	const void *end = memchr( text, '\0', capacity );
	return end ? static_cast<const char *>( end ) - text : capacity;
}

void R_StudioLoadTexture( model_t *mod, studiohdr_t *phdr, mstudiotexture_t *ptexture )
{
	if( !mod || !phdr || !ptexture || phdr->length < static_cast<int>( sizeof( studiohdr_t ) ) )
		return;

	const byte *base = reinterpret_cast<const byte *>( phdr );
	const byte *texturePtr = reinterpret_cast<const byte *>( ptexture );
	if( texturePtr < base )
		return;
	const size_t textureOffset = static_cast<size_t>( texturePtr - base );
	if( textureOffset > static_cast<size_t>( phdr->length ) ||
		static_cast<size_t>( phdr->length ) - textureOffset < sizeof( mstudiotexture_t ) )
		return;

	if( ptexture->width <= 0 || ptexture->height <= 0 ||
		ptexture->width > 16384 || ptexture->height > 16384 || ptexture->index < 0 )
		return;

	size_t pixelBytes = 0, imageBytes = 0, imageEnd = 0, loaderBytes = 0;
	if( !DX_CheckedMul( static_cast<size_t>( ptexture->width ),
		static_cast<size_t>( ptexture->height ), pixelBytes ) ||
		!DX_CheckedAdd( pixelBytes, 768u, imageBytes ) ||
		!DX_CheckedAdd( static_cast<size_t>( ptexture->index ), imageBytes, imageEnd ) ||
		imageEnd > static_cast<size_t>( phdr->length ) ||
		!DX_CheckedAdd( sizeof( mstudiotexture_t ), imageBytes, loaderBytes ) )
	{
		return;
	}

	char studioName[sizeof( ptexture->name ) + 1]{};
	const size_t studioNameLen = DX_BoundedStringLength( ptexture->name, sizeof( ptexture->name ) );
	memcpy( studioName, ptexture->name, studioNameLen );
	studioName[studioNameLen] = '\0';

	int flags = 0;
	char texname[128]{}, name[128]{}, mdlname[128]{};
	texture_t *tx = nullptr;

	if( ptexture->flags & STUDIO_NF_NORMALMAP )
		flags |= TF_NORMALMAP;

	const bool dmBase = !Q_strnicmp( studioName, "DM_Base", 7 );
	const bool remap = !Q_strnicmp( studioName, "remap", 5 );
	if( dmBase || remap )
	{
		if( mod->numtextures < 0 || mod->numtextures >= 4096 )
			return;
		const size_t pointerBytes = static_cast<size_t>( mod->numtextures + 1 ) * sizeof( texture_t * );
		texture_t **newTextures = static_cast<texture_t **>(
			Mem_Realloc( mod->mempool, mod->textures, pointerBytes ) );
		if( !newTextures )
			return;
		mod->textures = newTextures;

		size_t allocationBytes = 0;
		if( !DX_CheckedAdd( sizeof( texture_t ), imageBytes, allocationBytes ) )
			return;
		tx = static_cast<texture_t *>( Mem_Calloc( mod->mempool, allocationBytes ) );
		if( !tx )
			return;
		mod->textures[mod->numtextures] = tx;

		if( dmBase )
		{
			Q_strncpy( tx->name, "DM_Base", sizeof( tx->name ) );
			tx->anim_min = PLATE_HUE_START;
			tx->anim_max = PLATE_HUE_END;
			tx->anim_total = SUIT_HUE_END;
		}
		else
		{
			Q_strncpy( tx->name, "DM_User", sizeof( tx->name ) );
			if( studioNameLen >= 19 )
			{
				char val[6]{};
				Q_strncpy( val, studioName + 7, 4 );
				tx->anim_min = bound( 0, Q_atoi( val ), 255 );
				Q_strncpy( val, studioName + 11, 4 );
				tx->anim_max = bound( 0, Q_atoi( val ), 255 );
				Q_strncpy( val, studioName + 15, 4 );
				tx->anim_total = bound( 0, Q_atoi( val ), 255 );
			}
		}
	}

	Q_strncpy( mdlname, mod->name, sizeof( mdlname ) );
	COM_FileBase( studioName, name, sizeof( name ) );
	COM_StripExtension( mdlname );

	if( FBitSet( ptexture->flags, STUDIO_NF_NOMIPS ) )
		SetBits( flags, TF_NOMIPMAP );
	if( FBitSet( gp_host->features, ENGINE_IMPROVED_LINETRACE ) &&
		FBitSet( ptexture->flags, STUDIO_NF_MASKED ) )
		flags |= TF_KEEP_SOURCE;

	gEngfuncs.Image_SetMDLPointer( reinterpret_cast<byte *>( phdr ) + ptexture->index );
	Q_snprintf( texname, sizeof( texname ), "#%s/%s.mdl", mdlname, name );
	const int textureId = DX_LoadTexture( texname, reinterpret_cast<byte *>( ptexture ), loaderBytes, flags );
	if( textureId <= 0 ||
		static_cast<size_t>( textureId ) >= g_Textures.size( ) )
	{
		ptexture->index = 1;
		return;
	}

	ptexture->index = textureId;

	auto studioIt = g_StudioTexIndex.find( studioName );
	if( studioIt != g_StudioTexIndex.end( ) )
		studioIt->second = textureId;
	else
		g_StudioTexIndex.emplace( studioName, textureId );
}

void DX_FreeTexture( unsigned int texnum )
{
	if( texnum == 0 || texnum > static_cast<unsigned int>( g_texcount ) )
		return;

	dxtexture_t &texture = g_Textures[texnum];

	if( !texture.tex && !texture.original && !texture.name[0] &&
		texture.texnum == 0 )
	{
		return;
	}

	for( int tmu = 0; tmu < DX_MAX_TEXTURE_STAGES; ++tmu )
	{
		if( g_dxBoundTextureId[tmu] != texnum )
			continue;

		if( device )
			device->SetTexture( static_cast<DWORD>( tmu ), nullptr );

		g_dxBoundTextureId[tmu] = DX_BOUND_TEX_INVALID;
		g_dxBoundRawTexture[tmu] = nullptr;
		g_dxSamplerProfile[tmu] = DX_SAMPLER_PROFILE_INVALID;
	}

	for( auto it = g_TextureIndex.begin( ); it != g_TextureIndex.end( ); )
	{
		if( it->second == static_cast<int>( texnum ) )
			it = g_TextureIndex.erase( it );
		else
			++it;
	}

	for( auto it = g_StudioTexIndex.begin( ); it != g_StudioTexIndex.end( ); )
	{
		if( it->second == static_cast<int>( texnum ) )
			it = g_StudioTexIndex.erase( it );
		else
			++it;
	}

	if( texture.tex )
		texture.tex->Release( );
	if( texture.original )
		gEngfuncs.FS_FreeImage( texture.original );

	texture = {};
	g_freeTextureIds.push_back( static_cast<int>( texnum ) );
}

void DX_Bind( int tmu, unsigned int texnum )
{
	if( !device || tmu < 0 || tmu >= DX_MAX_TEXTURE_STAGES )
		return;

	if( texnum == 0 || texnum > static_cast<unsigned int>( g_texcount ) )
	{
		DX_BindRawTexture( tmu, nullptr );
		return;
	}

	dxtexture_t &texture = g_Textures[texnum];
	IDirect3DBaseTexture9 *d3dTexture = texture.tex;
	const unsigned int samplerProfile = DX_TextureSamplerProfileKey( texture );

	if( g_dxBoundTextureId[tmu] == texnum &&
		g_dxBoundRawTexture[tmu] == d3dTexture &&
		g_dxSamplerProfile[tmu] == samplerProfile )
	{
		DX_ProfileTexture( DX_PROFILE_TEXTURE_GL, false );
		return;
	}

	if( g_dxBoundTextureId[tmu] != texnum || g_dxBoundRawTexture[tmu] != d3dTexture )
	{
		DX_ProfileTexture( DX_PROFILE_TEXTURE_GL, true );
		device->SetTexture( static_cast<DWORD>( tmu ), d3dTexture );
		g_dxBoundTextureId[tmu] = texnum;
		g_dxBoundRawTexture[tmu] = d3dTexture;
	}
	else
	{
		DX_ProfileTexture( DX_PROFILE_TEXTURE_GL, false );
	}

	DX_ApplySamplerProfile( static_cast<DWORD>( tmu ), samplerProfile );
}
