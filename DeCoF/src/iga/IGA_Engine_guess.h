// IGA_Engine_guess.h -- the engine side of the in-game advertising system:
// the CInGameAdvertisingSystem singleton, its cells, contents and images.
// No string names the original file or any type except the two classes of
// the RTTI (CInGameAdvertisingSystem, IClientDLLInGameAdvertising).

#ifndef IGA_ENGINE_GUESS_H
#define IGA_ENGINE_GUESS_H
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"
#include "UtlVector.h"
#include "UtlRBTree.h"
#include "Color.h"

struct model_s;
struct ref_params_s;
struct texture_s;
struct miptex_s;
class KeyValues;

#define CLIENTDLLINGAMEADVERTISING_INTERFACE_VERSION	"CLIENTDLLINGAMEADVERTISINGVERSION001"
#define ENGINEINGAMEADVERTISING_INTERFACE_VERSION		"ENGINEINGAMEADVERTISINGVERSION001"

// cell types of CAdCell_guess::m_nType_guess
#define ADCELL_PLACEHOLDER_GUESS	0x10	// a decal placed in the level from the metadata
#define ADCELL_HUD_GUESS			0x20	// a HUD cell drawn by the client DLL

// what the client DLL reports when it has shown a HUD ad
struct ClientAdImpression_guess
{
	bool	m_bAngleZero_guess;
	uint64	m_ContentId_guess;
	int		m_nCellId_guess;
	float	m_flArea_guess;
};

// implemented by the engine, looked up by the client DLL
class IClientDLLInGameAdvertising : public IBaseInterface
{
public:
	virtual void	ImpressionShown_guess( const ClientAdImpression_guess *pImpression ) = 0;
	virtual void	RequestReload_guess( void ) = 0;
};

// implemented by the client DLL, looked up by the engine
class IEngineInGameAdvertising_guess : public IBaseInterface
{
public:
	virtual void	Init_guess( CreateInterfaceFn engineFactory ) = 0;
	virtual void	Shutdown_guess( void ) = 0;
	virtual void	ContentLoaded_guess( int nFlags, int nCellId, uint64 contentId, int nWidth, int nHeight, unsigned char *pData ) = 0;
};

// results of the Radial SDK calls (names from the engine's error strings)
enum
{
	RAD_ERROR_DISABLED = -100,
	RAD_ERROR_OUTOFMEMORY = -13,
	RAD_ERROR_NOTINITIALIZED,
	RAD_ERROR_CANCELLED,
	RAD_ERROR_SYSTEM,
	RAD_ERROR_CACHE,
	RAD_ERROR_NOCONTENT,
	RAD_ERROR_BUFFERSIZE,
	RAD_ERROR_COMMUNICATION,
	RAD_ERROR_INTERNALERROR,
	RAD_ERROR_INVALIDPARAMETER,
	RAD_ERROR_UNSUPPORTED,
	RAD_ERROR_NOSERVER,
	RAD_ERROR_SOCKETERROR,
	RAD_SUCCESS,
	RAD_IMPRESSION_SENT,
};

// the start-up block of the Radial SDK (prebuilt library)
struct RadInfo_guess
{
	int		m_nGameId_guess;
	char	m_szCachePath_guess[512];
	char	m_Reserved_guess[1028];
	int		m_nRefreshTime_guess;
	int		m_nRetryTime_guess;
	int		m_nMaxRequests_guess;
	bool	m_bEnabled_guess;
	char	m_Pad_guess[19];
};

// one content request to the Radial SDK
struct RadContent_guess
{
	uint64			m_ContentId_guess;
	unsigned int	m_nSize_guess;
	int				m_nReserved1_guess;
	void			*m_pBuffer_guess;
	int				m_nReserved2_guess;
};

extern "C"
{
int		radInit_guess( RadInfo_guess *pInfo );
void	radShutdown_guess( RadInfo_guess *pInfo );
int		radGetContent_guess( RadInfo_guess *pInfo, int nCellId, RadContent_guess *pContent );
void	radResetLevel_guess( RadInfo_guess *pInfo );
int		radImpression_guess( RadInfo_guess *pInfo, int nCellId, uint64 contentId, float flArea, float flAngle );
}

// an image of the metadata placeholders (a default ad)
class CAdImage_guess
{
public:
	CAdImage_guess()
	{
		m_pData_guess = NULL;
		m_nSize_guess = 0;
		m_szName_guess[0] = 0;
	}

	~CAdImage_guess()
	{
		delete [] m_pData_guess;
	}

	char			m_szName_guess[32];
	unsigned char	*m_pData_guess;
	unsigned int	m_nSize_guess;
};

// a decal of a placeholder
struct CAdDecal_guess
{
	CAdDecal_guess()
	{
		m_szName_guess[0] = 0;
		VectorCopy( vec3_origin, m_vecOrigin_guess );
	}

	char	m_szName_guess[32];
	vec3_t	m_vecOrigin_guess;
};

// a placeholder of a level: where a cell is drawn
struct CAdPlaceholder_guess
{
	CAdPlaceholder_guess() : m_nCellId_guess( -1 ), m_flScale_guess( 1.0 ), m_nBorder_guess( 0 ), m_BorderColor_guess( 0, 0, 0 )
	{
		VectorCopy( vec3_origin, m_vecOrigin_guess );
		VectorCopy( vec3_origin, m_vecNormal_guess );
		VectorCopy( vec3_origin, m_vecRight_guess );
		VectorCopy( vec3_origin, m_vecUp_guess );
		m_Size_guess[0] = m_Size_guess[1] = 0;
	}

	CAdPlaceholder_guess( const CAdPlaceholder_guess &src )
	{
		m_nCellId_guess = src.m_nCellId_guess;
		VectorCopy( src.m_vecOrigin_guess, m_vecOrigin_guess );
		VectorCopy( src.m_vecNormal_guess, m_vecNormal_guess );
		VectorCopy( src.m_vecRight_guess, m_vecRight_guess );
		VectorCopy( src.m_vecUp_guess, m_vecUp_guess );
		m_Size_guess[0] = src.m_Size_guess[0];
		m_Size_guess[1] = src.m_Size_guess[1];
		m_flScale_guess = src.m_flScale_guess;
		m_nBorder_guess = src.m_nBorder_guess;
		m_BorderColor_guess = src.m_BorderColor_guess;
		for ( int i = 0; i < src.m_Decals_guess.Count(); i++ )
			m_Decals_guess.AddToTail( src.m_Decals_guess[i] );
	}

	~CAdPlaceholder_guess()
	{
	}

	int							m_nCellId_guess;
	vec3_t						m_vecOrigin_guess;
	vec3_t						m_vecNormal_guess;
	vec3_t						m_vecRight_guess;
	vec3_t						m_vecUp_guess;
	int							m_Size_guess[2];
	float						m_flScale_guess;
	int							m_nBorder_guess;
	Color						m_BorderColor_guess;
	CUtlVector< CAdDecal_guess >	m_Decals_guess;
};

// the placeholders of one level
struct CAdLevel_guess
{
	CAdLevel_guess()
	{
		m_szName_guess[0] = 0;
	}

	CAdLevel_guess( const CAdLevel_guess &src )
	{
		Q_strncpy( m_szName_guess, src.m_szName_guess, sizeof( m_szName_guess ) );
		for ( int i = 0; i < src.m_Placeholders_guess.Count(); i++ )
			m_Placeholders_guess.AddToTail( src.m_Placeholders_guess[i] );
	}

	~CAdLevel_guess()
	{
	}

	char								m_szName_guess[64];
	CUtlVector< CAdPlaceholder_guess >	m_Placeholders_guess;
};

// the default image of a cell and its content id
struct CAdCellImage_guess
{
	CAdCellImage_guess()
	{
		m_nImage_guess = 0;
		m_ContentId_guess = 0;
	}

	int		m_nImage_guess;
	uint64	m_ContentId_guess;
};

// the default images of one cell
struct CAdCellImages_guess
{
	CAdCellImages_guess() : m_nCellId_guess( 0 )
	{
	}

	CAdCellImages_guess( const CAdCellImages_guess &src )
	{
		m_nCellId_guess = src.m_nCellId_guess;
		for ( int i = 0; i < src.m_Images_guess.Count(); i++ )
			m_Images_guess.AddToTail( src.m_Images_guess[i] );
	}

	int									m_nCellId_guess;
	CUtlVector< CAdCellImage_guess >	m_Images_guess;
};

// a downloaded or default ad
class CAdContent_guess
{
public:
	CAdContent_guess()
	{
		m_bLoaded_guess = false;
		m_bTexture_guess = false;
		m_bHUD_guess = false;
		m_nType_guess = 0;
		m_ContentId_guess = 0;
		m_nCellId_guess = 0;
		m_nWidth_guess = 0;
		m_nHeight_guess = 0;
		m_pData_guess = NULL;
		m_nTexture_guess = 0;
		Q_memset( m_Palette_guess, 0, sizeof( m_Palette_guess ) );
		Q_memset( m_MipOffsets_guess, 0, sizeof( m_MipOffsets_guess ) );
		m_pPalette_guess = m_Palette_guess;
	}

	~CAdContent_guess()
	{
		FreeData_guess();
	}

	void FreeData_guess( void )
	{
		delete [] m_pData_guess;
		m_pData_guess = NULL;
	}

	unsigned char	*BuildMipLevels_guess( struct texture_s *pTexture, unsigned char *pSource, unsigned char *pDest, unsigned char *pPalette );
	void			BuildMips_guess( unsigned char *pSource, unsigned char *pDest, unsigned char *pPalette );
	void			DrawBorder_guess( int nBorder, const Color &color, unsigned char *pData, int nWidth, int nHeight );
	struct texture_s	*CreateTexture_guess( const char *pszName, int nWidth, int nHeight, int nBorder, const Color &borderColor );
	struct texture_s	*CreateTexture_guess( const char *pszName );
	bool			LoadMip_guess( struct miptex_s *pMip );

	bool			m_bLoaded_guess : 1;
	bool			m_bTexture_guess : 1;
	bool			m_bHUD_guess : 1;
	int				m_nType_guess;
	uint64			m_ContentId_guess;
	int				m_nCellId_guess;
	int				m_nWidth_guess;
	int				m_nHeight_guess;
	unsigned char	*m_pData_guess;
	unsigned int	m_nMagic_guess;
	unsigned int	m_nDataSize_guess;
	unsigned char	m_Palette_guess[768];
	unsigned char	*m_pPalette_guess;
	unsigned int	m_MipOffsets_guess[4];
	int				m_nTexture_guess;
};

// a cell of the current level
struct CAdCell_guess
{
	CAdCell_guess()
	{
		m_nCellId_guess = 0;
		m_nType_guess = 0;
		m_pContent_guess = NULL;
		m_bVisible_guess = false;
		m_nDecalTexture_guess = -1;
		m_pPlaceholder_guess = NULL;
		m_szName_guess[0] = 0;
	}

	CAdCell_guess( const CAdCell_guess &src )
	{
		m_nCellId_guess = src.m_nCellId_guess;
		m_nType_guess = src.m_nType_guess;
		Q_strncpy( m_szName_guess, src.m_szName_guess, sizeof( m_szName_guess ) );
		m_pContent_guess = src.m_pContent_guess;
		m_bVisible_guess = src.m_bVisible_guess;
		m_nDecalTexture_guess = src.m_nDecalTexture_guess;
		m_pPlaceholder_guess = src.m_pPlaceholder_guess;
	}

	~CAdCell_guess()
	{
	}

	int						m_nCellId_guess;
	int						m_nType_guess;
	CAdContent_guess		*m_pContent_guess;
	char					m_szName_guess[16];
	bool					m_bVisible_guess;
	int						m_nDecalTexture_guess;
	const CAdPlaceholder_guess	*m_pPlaceholder_guess;
};

class CInGameAdvertisingSystem : public IClientDLLInGameAdvertising
{
public:
	CInGameAdvertisingSystem();

	const char	*GetRadErrorString_guess( int nError );
	int			Init_guess( void );
	void		Shutdown_guess( void );
	void		SetClientFactory_guess( CreateInterfaceFn clientFactory );
	void		ResetLevel_guess( void );
	int			IsActive_guess( void );
	void		LevelLoaded_guess( struct model_s *worldmodel );
	bool		ComputeVisibility_guess( CAdCell_guess *pCell, struct model_s *worldmodel, struct ref_params_s *pparams, float *forward, float *pArea, float *pAngle );
	void		RenderView_guess( struct model_s *worldmodel, struct ref_params_s *pparams, float *viewangles );
	void		FreeContent_guess( void );
	CAdContent_guess	*FindOrCreateContent_guess( uint64 contentId );
	bool		LoadDefaultContent_guess( uint64 contentId, CAdCell_guess *pCell );
	bool		LoadImage_guess( void *pData, int nSize, CAdContent_guess *pContent, const Color *pBorderColor );
	void		RequestContent_guess( void );
	void		FreeTextures_guess( void );
	void		Reload_f_guess( void );
	void		Frame_guess( struct model_s *worldmodel );
	void		ListContent_guess( void );
	virtual void	ImpressionShown_guess( const ClientAdImpression_guess *pImpression );
	virtual void	RequestReload_guess( void );
	void		ShootDecal_guess( int textureIndex, int entity, int modelIndex, float *position, int flags, float flScale );
	void		Shoot_f_guess( void );
	bool		LoadEncryptedBuffer_guess( const char *pszFile, class CUtlBuffer &buf );
	char		*LoadEncryptedFile_guess( void );
	bool		ParsePlaceholders_guess( KeyValues *pKeys );
	void		LoadMetadata_guess( void );
	void		SetupPlaceholders_guess( struct model_s *worldmodel );
	void		PlaceDecals_guess( struct model_s *worldmodel );
	struct texture_s	*GetDecalTexture_guess( int index );
	void		TrackImpression_guess( int nCellId, uint64 contentId );

	// IGA_Metadata_guess.cpp
	bool		IsPalettedImage_guess( void *pData, int nSize );
	bool		LoadPalettedImage_guess( void *pData, int nSize, CAdContent_guess *pContent );
	bool		LoadTrueColorImage_guess( void *pData, int nSize, CAdContent_guess *pContent, const Color *pBorderColor );
	bool		LoadHUDImage_guess( void *pData, int nSize, CAdContent_guess *pContent );
	bool		LoadHUDImageFile_guess( const char *pszFile, int nFlags, CAdContent_guess *pContent );
	bool		AddImage_guess( CUtlVector< CAdImage_guess * > &images, const char *pszName, void *pData, int nSize );

	static bool LevelLessFunc_guess( CAdLevel_guess const &a, CAdLevel_guess const &b )
	{
		return _stricmp( a.m_szName_guess, b.m_szName_guess ) < 0;
	}

	static bool CellLessFunc_guess( CAdCell_guess const &a, CAdCell_guess const &b )
	{
		return a.m_nCellId_guess < b.m_nCellId_guess;
	}

	static bool CellImagesLessFunc_guess( CAdCellImages_guess const &a, CAdCellImages_guess const &b )
	{
		return a.m_nCellId_guess < b.m_nCellId_guess;
	}

	bool									m_bInitialized_guess;
	RadInfo_guess							m_RadInfo_guess;
	CUtlRBTree< CAdCell_guess, int >		m_Cells_guess;
	CUtlVector< CAdContent_guess * >		m_Content_guess;
	IEngineInGameAdvertising_guess			*m_pClientAds_guess;
	CUtlVector< struct texture_s * >		m_Textures_guess;
	vec3_t									m_vecViewOrigin_guess;
	vec3_t									m_vecViewForward_guess;
	CUtlRBTree< CAdLevel_guess, int >		m_Levels_guess;
	bool									m_bReload_guess;
	CUtlRBTree< CAdCellImages_guess, int >	m_CellImages_guess;
	bool									m_bMetadataLoaded_guess;
	CUtlVector< CAdImage_guess * >			m_Images_guess;
	float									m_flLastReload_guess;
	int										m_HUDCells_guess[2];
};

#endif // IGA_ENGINE_GUESS_H
