// IGA_Engine_guess.cpp -- the engine side of the in-game advertising system:
// the Radial SDK start-up, the level placeholders of the metadata file
// (iga.dat, ICE encrypted), the downloaded or default ads shown as decals or
// handed to the client DLL, and the impressions reported back.
// The file name is not known; it only describes the object and sorts into
// its link position between IceKey.cpp and IGA_Metadata_guess.cpp.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interface.h"
#include "FileSystem.h"
#include "UtlBuffer.h"
#include "UtlVector.h"
#include "UtlRBTree.h"
#include <vgui/KeyValues.h>
#include "IceKey.H"

extern "C"
{
#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "cvar.h"
#include "cmd.h"
#include "GL_MODEL.H"
#include "client.h"
#include "GLQUAKE.H"
#include "progs.h"
#include "eiface.h"
#include "delta.h"
#include "server.h"
#include "draw.h"
#include "vid.h"
#include "host.h"
#include "console.h"
#include "con_nprint.h"
#include "filesystem_.h"
#include "world.h"
#include "ref_params.h"
}

#include "IGA_Engine_guess.h"
#include "IGA_Quantizer_guess.h"

extern IFileSystem *g_pFileSystem;

extern "C"
{
extern float	gDecalAppliedScale;
int		ScreenTransform( vec_t *point, vec_t *screen );
void	R_BulletImpactParticles( vec_t *pos );
void	R_DecalShootScaled( int textureIndex, int entity, int modelIndex, vec_t *position, int flags, float flScale );
void	R_DecalRemoveWithFlag( int flags );
void	GL_UnloadTexture( char *identifier );
int		GL_LoadTexture( char *identifier, GL_TEXTURETYPE textureType, int width, int height, byte *data, int mipmap, int iType, byte *pPal );
}


// Steamworks (steam_api.dll)
class CGameID
{
public:
	explicit CGameID( uint32 nAppID )
	{
		m_ulGameID = 0;
		m_gameID.m_nAppID = nAppID;
	}

private:
	struct GameID_t
	{
		unsigned int m_nAppID : 24;
		unsigned int m_nType : 8;
		unsigned int m_nModID : 32;
	};

	union
	{
		uint64		m_ulGameID;
		GameID_t	m_gameID;
	};
};

class ISteamUser
{
public:
	virtual int		GetHSteamUser() = 0;
	virtual bool	BLoggedOn() = 0;
	virtual uint64	GetSteamID() = 0;
	virtual int		InitiateGameConnection( void *pAuthBlob, int cbMaxAuthBlob, uint64 steamIDGameServer, uint32 unIPServer, unsigned short usPortServer, bool bSecure ) = 0;
	virtual void	TerminateGameConnection( uint32 unIPServer, unsigned short usPortServer ) = 0;
	virtual void	TrackAppUsageEvent( CGameID gameID, int eAppUsageEvent, const char *pchExtraInfo = "" ) = 0;
};

extern "C" __declspec( dllimport ) ISteamUser * __cdecl SteamUser();

cvar_t	iga_areacheck = { "iga_areacheck", "0" };

// the games that show ads: game directory, Radial game id, the two HUD cells
struct IGAGame_guess
{
	const char	*m_pszGameDir_guess;
	int			m_nGameId_guess;
	int			m_HUDCells_guess[2];
};

IGAGame_guess g_IGAGames_guess[] =
{
	{ "cstrike",		95,		{ 180, 181 } },
	{ "cstrike_beta",	95,		{ 180, 181 } },
	{ "czero",			174,	{ 10, 11 } },
};

static int g_nNotifyLine_guess = 10;

// ICE key of iga.dat and the placeholder images
static unsigned char g_IGAKey_guess[32] =
{
	0x32, 0xA6, 0x21, 0xE0, 0xAB, 0x6B, 0xF4, 0x68, 0x93, 0x70, 0xF1, 0x96, 0xE0, 0x38, 0x75, 0x2C,
	0xBA, 0xC6, 0x07, 0x86, 0xFB, 0xB3, 0x71, 0xF4, 0xE3, 0x9B, 0x13, 0x22, 0x0C, 0xFE, 0x88, 0x3A
};

static bool g_bWarnSize_guess = true;

static CInGameAdvertisingSystem g_InGameAdvertisingSystem;

extern "C"
{

int AdsActive_guess( void )
{
	return g_InGameAdvertisingSystem.IsActive_guess();
}

void AdsStartup_guess( void )
{
	g_InGameAdvertisingSystem.Init_guess();
}

void AdsShutdown_guess( void )
{
	g_InGameAdvertisingSystem.Shutdown_guess();
}

void AdsFrame_guess( struct model_s *worldmodel )
{
	g_InGameAdvertisingSystem.Frame_guess( worldmodel );
}

void LevelLoaded_guess( struct model_s *worldmodel )
{
	g_InGameAdvertisingSystem.LevelLoaded_guess( worldmodel );
}

void AdsRenderView_guess( struct model_s *worldmodel, struct ref_params_s *pparams, float *viewangles )
{
	g_InGameAdvertisingSystem.RenderView_guess( worldmodel, pparams, viewangles );
}

void IGA_ClientFactory_guess( void *factory )
{
	CreateInterfaceFn clientFactory_guess = (CreateInterfaceFn)factory;
	g_InGameAdvertisingSystem.SetClientFactory_guess( clientFactory_guess );
}

void ResetLevelState_guess( void )
{
	g_InGameAdvertisingSystem.ResetLevel_guess();
}

struct texture_s *AdsDecalTexture_guess( int index )
{
	return g_InGameAdvertisingSystem.GetDecalTexture_guess( index );
}

void AdsListContent_guess( void )
{
	g_InGameAdvertisingSystem.ListContent_guess();
}

void AdsShoot_f_guess( void )
{
	g_InGameAdvertisingSystem.Shoot_f_guess();
}

void AdsReload_f_guess( void )
{
	g_InGameAdvertisingSystem.Reload_f_guess();
}

}

CInGameAdvertisingSystem::CInGameAdvertisingSystem() :
	m_bInitialized_guess( false ),
	m_Cells_guess( 0, 0, CellLessFunc_guess ),
	m_Content_guess( 0, 0 ),
	m_pClientAds_guess( NULL ),
	m_Textures_guess( 0, 0 ),
	m_Levels_guess( 0, 0, LevelLessFunc_guess ),
	m_bReload_guess( false ),
	m_CellImages_guess( 0, 0, CellImagesLessFunc_guess ),
	m_bMetadataLoaded_guess( false ),
	m_Images_guess( 0, 0 ),
	m_flLastReload_guess( 0 )
{
	VectorCopy( vec3_origin, m_vecViewOrigin_guess );
	VectorCopy( vec3_origin, m_vecViewForward_guess );
	Q_memset( m_HUDCells_guess, 0, sizeof( m_HUDCells_guess ) );
}

#define RAD_ERROR_CASE( x )	case x: return #x;

const char *CInGameAdvertisingSystem::GetRadErrorString_guess( int nError )
{
	switch ( nError )
	{
	RAD_ERROR_CASE( RAD_IMPRESSION_SENT )
	RAD_ERROR_CASE( RAD_SUCCESS )
	RAD_ERROR_CASE( RAD_ERROR_SOCKETERROR )
	RAD_ERROR_CASE( RAD_ERROR_NOSERVER )
	RAD_ERROR_CASE( RAD_ERROR_UNSUPPORTED )
	RAD_ERROR_CASE( RAD_ERROR_INVALIDPARAMETER )
	RAD_ERROR_CASE( RAD_ERROR_INTERNALERROR )
	RAD_ERROR_CASE( RAD_ERROR_COMMUNICATION )
	RAD_ERROR_CASE( RAD_ERROR_BUFFERSIZE )
	RAD_ERROR_CASE( RAD_ERROR_NOCONTENT )
	RAD_ERROR_CASE( RAD_ERROR_CACHE )
	RAD_ERROR_CASE( RAD_ERROR_SYSTEM )
	RAD_ERROR_CASE( RAD_ERROR_CANCELLED )
	RAD_ERROR_CASE( RAD_ERROR_NOTINITIALIZED )
	RAD_ERROR_CASE( RAD_ERROR_OUTOFMEMORY )
	RAD_ERROR_CASE( RAD_ERROR_DISABLED )
	default:
		return "Unknown Error";
	}
}

int CInGameAdvertisingSystem::Init_guess( void )
{
	int game_id_guess;
	unsigned int i;
	int j;
	IGAGame_guess *game_guess;
	const char *dir2_guess;
	int status_guess;

	if ( !g_bUsingInGameAdvertisements )
		return 0;

	game_id_guess = 0;
	for ( i = 0; i < ARRAYSIZE( g_IGAGames_guess ); i++ )
	{
		game_guess = &g_IGAGames_guess[i];
		dir2_guess = game_guess->m_pszGameDir_guess;
		if ( !Q_stricmp( dir2_guess, com_gamedir ) )
		{
			game_id_guess = game_guess->m_nGameId_guess;
			for ( j = 0; j < 2; j++ )
				m_HUDCells_guess[j] = game_guess->m_HUDCells_guess[j];
			break;
		}
	}

	if ( !game_id_guess )
		return 0;

	LoadMetadata_guess();

	Cvar_RegisterVariable( &iga_areacheck );

	Q_memset( &m_RadInfo_guess, 0, sizeof( m_RadInfo_guess ) );
	m_RadInfo_guess.m_nGameId_guess = game_id_guess;
	m_RadInfo_guess.m_bEnabled_guess = true;
	m_RadInfo_guess.m_nRefreshTime_guess = 900000;
	m_RadInfo_guess.m_nRetryTime_guess = 300000;
	m_RadInfo_guess.m_nMaxRequests_guess = 50;
	_snprintf( m_RadInfo_guess.m_szCachePath_guess, sizeof( m_RadInfo_guess.m_szCachePath_guess ), "%s\\", com_gamedir );

	status_guess = radInit_guess( &m_RadInfo_guess );
	if ( status_guess )
	{
		Con_Printf( "Error starting up RAD [%s]\n", GetRadErrorString_guess( status_guess ) );
		m_bInitialized_guess = false;
	}
	else
	{
		m_bInitialized_guess = true;
	}

	Cmd_AddCommand( "iga_shoot", AdsShoot_f_guess );
	Cmd_AddCommand( "iga_reload", AdsReload_f_guess );

	return m_bInitialized_guess;
}

void CInGameAdvertisingSystem::Shutdown_guess( void )
{
	int i;

	if ( m_pClientAds_guess )
		m_pClientAds_guess->Shutdown_guess();

	if ( m_bInitialized_guess )
		radShutdown_guess( &m_RadInfo_guess );

	FreeContent_guess();
	FreeTextures_guess();
	m_Cells_guess.RemoveAll();
	m_Levels_guess.RemoveAll();

	for ( i = 0; i < m_Images_guess.Count(); i++ )
	{
		delete m_Images_guess[i];
	}
	m_Images_guess.RemoveAll();

	m_bMetadataLoaded_guess = false;
	m_bInitialized_guess = false;
}

void CInGameAdvertisingSystem::SetClientFactory_guess( CreateInterfaceFn clientFactory )
{
	m_pClientAds_guess = (IEngineInGameAdvertising_guess *)clientFactory( ENGINEINGAMEADVERTISING_INTERFACE_VERSION, NULL );
	if ( m_pClientAds_guess )
		m_pClientAds_guess->Init_guess( Sys_GetFactoryThis() );
}

void CInGameAdvertisingSystem::ResetLevel_guess( void )
{
	if ( !IsActive_guess() )
		return;

	radResetLevel_guess( &m_RadInfo_guess );
}

int CInGameAdvertisingSystem::IsActive_guess( void )
{
	return m_bInitialized_guess;
}

void CInGameAdvertisingSystem::LevelLoaded_guess( struct model_s *worldmodel )
{
	if ( !IsActive_guess() )
		return;

	FreeContent_guess();
	FreeTextures_guess();
	m_Cells_guess.RemoveAll();

	if ( m_pClientAds_guess )
	{
		CAdContent_guess *hudContent_guess;

		hudContent_guess = FindOrCreateContent_guess( (uint64)-1 );
		hudContent_guess->m_nCellId_guess = m_HUDCells_guess[0];

		CAdCell_guess entry_guess;
		entry_guess.m_nCellId_guess = hudContent_guess->m_nCellId_guess;
		entry_guess.m_pContent_guess = hudContent_guess;
		entry_guess.m_nType_guess = ADCELL_HUD_GUESS;
		m_Cells_guess.Insert( entry_guess );

		hudContent_guess = FindOrCreateContent_guess( (uint64)-2 );
		hudContent_guess->m_nCellId_guess = m_HUDCells_guess[1];
		entry_guess.m_nCellId_guess = hudContent_guess->m_nCellId_guess;
		entry_guess.m_pContent_guess = hudContent_guess;
		entry_guess.m_nType_guess = ADCELL_HUD_GUESS;
		m_Cells_guess.Insert( entry_guess );
	}

	SetupPlaceholders_guess( worldmodel );
	RequestContent_guess();
	PlaceDecals_guess( worldmodel );
}

// area of the screen quad, as a fraction of the screen
float ComputeScreenArea_guess( vec3_t *pPoints )
{
	float flArea_guess;
	int i;

	flArea_guess = 0;
	for ( i = 2; i < 4; i++ )
	{
		float x0 = pPoints[i - 2][0];
		float x1 = pPoints[i - 1][0];
		float x2 = pPoints[i][0];
		float y0 = pPoints[i - 2][1];
		float y1 = pPoints[i - 1][1];
		float y2 = pPoints[i][1];

		flArea_guess = flArea_guess + fabs( ( x1 * y0 - x0 * y1 ) + ( x2 * y1 - x1 * y2 ) + ( x0 * y2 - x2 * y0 ) ) * 0.5;
	}

	return flArea_guess;
}

bool CInGameAdvertisingSystem::ComputeVisibility_guess( CAdCell_guess *pCell, struct model_s *worldmodel, struct ref_params_s *pparams, float *forward, float *pArea, float *pAngle )
{
	float side_guess;
	float planeD_guess;
	vec3_t toCell_guess;
	bool bCulledCur_guess;
	float flAngle2_guess;
	float facing_guess;
	float len_guess;
	float sa_guess;
	float size_guess;
	float screenRatio_guess;
	bool bOccluded_guess;
	float correctionVal_guess;
	CAdContent_guess *cont_guess;
	float est2_guess;
	const CAdPlaceholder_guess *ph_guess;
	float halfw_guess;
	vec3_t screen_guess[4];

	cont_guess = pCell->m_pContent_guess;
	if ( !cont_guess )
		return false;

	if ( !pCell->m_pPlaceholder_guess )
		return false;

	ph_guess = pCell->m_pPlaceholder_guess;
	size_guess = (float)( ph_guess->m_Size_guess[0] * ph_guess->m_Size_guess[1] ) * ph_guess->m_flScale_guess * ph_guess->m_flScale_guess;

	VectorSubtract( ph_guess->m_vecOrigin_guess, pparams->vieworg, toCell_guess );
	len_guess = Length( toCell_guess );
	est2_guess = ( 36.0f * size_guess ) / ( len_guess * len_guess );

	screenRatio_guess = 4.0f / 3.0f;
	if ( vid.height > 0 )
		screenRatio_guess = (float)vid.width / (float)vid.height;

	correctionVal_guess = 1.0f;
	if ( screenRatio_guess > 1.0f )
		correctionVal_guess = screenRatio_guess / ( 4.0f / 3.0f );
	else
		correctionVal_guess = 1.0f / screenRatio_guess / ( 4.0f / 3.0f );

	est2_guess *= correctionVal_guess;

	planeD_guess = DotProduct( ph_guess->m_vecNormal_guess, ph_guess->m_vecOrigin_guess );
	facing_guess = DotProduct( ph_guess->m_vecNormal_guess, forward );
	side_guess = DotProduct( ph_guess->m_vecNormal_guess, pparams->vieworg ) - planeD_guess;

	sa_guess = 0;
	bOccluded_guess = false;
	bCulledCur_guess = false;

	flAngle2_guess = acos( facing_guess ) * 180.0 / M_PI;

	if ( side_guess > 0 || facing_guess < 0 )
	{
		flAngle2_guess = 91.0f;
	}
	else
	{
		trace_t trace_guess;
		float shrink_guess;
		vec3_t end_guess;
		int i;
		int j;
		float hhVal_guess;
		vec3_t src_guess;
		int inview_guess;

		Q_memset( &trace_guess, 0, sizeof( trace_guess ) );
		trace_guess.fraction = 1.0f;
		trace_guess.allsolid = true;

		VectorCopy( pparams->vieworg, src_guess );

		hhVal_guess = ph_guess->m_Size_guess[1] * 0.5f;
		halfw_guess = ph_guess->m_Size_guess[0] * 0.5f;
		shrink_guess = 0.9f;
		hhVal_guess = shrink_guess * ph_guess->m_flScale_guess * hhVal_guess;
		halfw_guess = shrink_guess * ph_guess->m_flScale_guess * halfw_guess;

		for ( i = 0; i < 4; i++ )
		{
			VectorCopy( ph_guess->m_vecOrigin_guess, end_guess );

			switch ( i )
			{
			default:
				for ( j = 0; j < 3; j++ )
					end_guess[j] = ph_guess->m_vecOrigin_guess[j] + hhVal_guess * ph_guess->m_vecUp_guess[j] + halfw_guess * ph_guess->m_vecRight_guess[j];
				break;
			case 1:
				for ( j = 0; j < 3; j++ )
					end_guess[j] = ph_guess->m_vecOrigin_guess[j] - hhVal_guess * ph_guess->m_vecUp_guess[j] + halfw_guess * ph_guess->m_vecRight_guess[j];
				break;
			case 2:
				for ( j = 0; j < 3; j++ )
					end_guess[j] = ph_guess->m_vecOrigin_guess[j] + hhVal_guess * ph_guess->m_vecUp_guess[j] - halfw_guess * ph_guess->m_vecRight_guess[j];
				break;
			case 3:
				for ( j = 0; j < 3; j++ )
					end_guess[j] = ph_guess->m_vecOrigin_guess[j] - hhVal_guess * ph_guess->m_vecUp_guess[j] - halfw_guess * ph_guess->m_vecRight_guess[j];
				break;
			}

			VectorCopy( end_guess, trace_guess.endpos );

			if ( iga_areacheck.value == 2.0f )
				R_BulletImpactParticles( end_guess );

			SV_RecursiveHullCheck( &worldmodel->hulls[0], 0, 0.0f, 1.0f, src_guess, end_guess, &trace_guess );
			if ( trace_guess.fraction < 0.99f )
			{
				flAngle2_guess = 91.0f;
				bOccluded_guess = true;
			}

			inview_guess = ScreenTransform( end_guess, screen_guess[i] );
			screen_guess[i][0] = ( screen_guess[i][0] + 1.0f ) * 0.5f;
			screen_guess[i][1] = ( screen_guess[i][1] + 1.0f ) * 0.5f;
			if ( inview_guess || screen_guess[i][0] < 0 || screen_guess[i][0] > 1.0f || screen_guess[i][1] < 0 || screen_guess[i][1] > 1.0f )
			{
				flAngle2_guess = 91.0f;
				bCulledCur_guess = true;
			}
		}

		sa_guess = ComputeScreenArea_guess( screen_guess ) * 100.0f;
	}

	if ( iga_areacheck.value )
	{
		char status_guess[32];
		bool def_guess;
		con_nprint_t notify_guess;
		uint64 id_guess;

		notify_guess.time_to_live = 0.01f;
		notify_guess.index = g_nNotifyLine_guess++;
		notify_guess.color[0] = 0.5f;
		notify_guess.color[1] = 1.0f;
		notify_guess.color[2] = 0.5f;
		Q_strncpy( status_guess, "+OK", sizeof( status_guess ) );

		if ( flAngle2_guess > 90.5f )
		{
			notify_guess.color[0] = 1.0f;
			notify_guess.color[1] = 0.1f;
			notify_guess.color[2] = 0.1f;

			if ( bCulledCur_guess && bOccluded_guess )
				Q_strncpy( status_guess, "Occ&Cull", sizeof( status_guess ) );
			else if ( bCulledCur_guess )
				Q_strncpy( status_guess, "Culled", sizeof( status_guess ) );
			else if ( bOccluded_guess )
				Q_strncpy( status_guess, "Occluded", sizeof( status_guess ) );
			else
				Q_strncpy( status_guess, "Angle", sizeof( status_guess ) );
		}

		def_guess = ( pCell->m_pContent_guess->m_ContentId_guess & 0x80000000 ) != 0;
		id_guess = pCell->m_pContent_guess->m_ContentId_guess & 0x7fffffff;
		Con_NXPrintf( &notify_guess, "%8s cell(%3d) cont(%s%I64u) est(%8.1f) sa(%8.1f ) d(%8.1f) ang(%.1f)",
			status_guess, pCell->m_pPlaceholder_guess->m_nCellId_guess, def_guess ? "def" : "iga", id_guess, est2_guess, sa_guess, len_guess, flAngle2_guess );
	}

	*pArea = est2_guess;
	*pAngle = flAngle2_guess;
	return true;
}

void CInGameAdvertisingSystem::RenderView_guess( struct model_s *worldmodel, struct ref_params_s *pparams, float *viewangles )
{
	vec3_t vup_guess;
	vec3_t vright_guess;
	int i;
	vec3_t fwd_guess;

	if ( !IsActive_guess() )
		return;

	AngleVectors( viewangles, fwd_guess, vright_guess, vup_guess );
	VectorCopy( pparams->vieworg, m_vecViewOrigin_guess );
	VectorCopy( fwd_guess, m_vecViewForward_guess );

	g_nNotifyLine_guess = 10;

	for ( i = m_Cells_guess.FirstInorder(); i != m_Cells_guess.InvalidIndex(); i = m_Cells_guess.NextInorder( i ) )
	{
		float angle_guess;
		float area_guess;
		CAdCell_guess *pCell_guess;

		pCell_guess = &m_Cells_guess[i];
		if ( !pCell_guess->m_pContent_guess )
			continue;

		if ( pCell_guess->m_nType_guess != ADCELL_PLACEHOLDER_GUESS )
		{
			if ( iga_areacheck.value )
			{
				bool bDefault_guess;
				char szType_guess[32];
				uint64 contentId_guess;
				con_nprint_t notify_guess;

				notify_guess.time_to_live = 0.01f;
				notify_guess.index = g_nNotifyLine_guess++;
				notify_guess.color[0] = 1.0f;
				notify_guess.color[1] = 1.0f;
				notify_guess.color[2] = 1.0f;
				Q_strncpy( szType_guess, "HUD", sizeof( szType_guess ) );
				bDefault_guess = ( pCell_guess->m_pContent_guess->m_ContentId_guess & 0x80000000 ) != 0;
				contentId_guess = pCell_guess->m_pContent_guess->m_ContentId_guess & 0x7fffffff;
				Con_NXPrintf( &notify_guess, "%8s cell(%3d) id(%s%I64u)", szType_guess, pCell_guess->m_nCellId_guess, bDefault_guess ? "def" : "iga", contentId_guess );
			}
			continue;
		}

		if ( !pCell_guess->m_bVisible_guess )
			continue;

		if ( ComputeVisibility_guess( pCell_guess, worldmodel, pparams, fwd_guess, &area_guess, &angle_guess ) )
		{
			int result_guess;

			if ( pCell_guess->m_pContent_guess->m_ContentId_guess & 0x80000000 )
				continue;

			result_guess = radImpression_guess( &m_RadInfo_guess, pCell_guess->m_nCellId_guess, pCell_guess->m_pContent_guess->m_ContentId_guess, area_guess, angle_guess );
			if ( result_guess == RAD_IMPRESSION_SENT )
				TrackImpression_guess( pCell_guess->m_nCellId_guess, pCell_guess->m_pContent_guess->m_ContentId_guess );
		}
	}
}

void CInGameAdvertisingSystem::FreeContent_guess( void )
{
	int i;
	CAdContent_guess *pFree_guess;

	for ( i = 0; i < m_Content_guess.Count(); i++ )
	{
		pFree_guess = m_Content_guess[i];
		delete pFree_guess;
	}
	m_Content_guess.Purge();
}

CAdContent_guess *CInGameAdvertisingSystem::FindOrCreateContent_guess( uint64 contentId )
{
	int total_guess;
	int i;
	CAdContent_guess *entry_guess;

	total_guess = m_Content_guess.Count();
	for ( i = 0; i < total_guess; i++ )
	{
		if ( m_Content_guess[i]->m_ContentId_guess == contentId )
			return m_Content_guess[i];
	}

	entry_guess = new CAdContent_guess;
	m_Content_guess.AddToTail( entry_guess );
	entry_guess->m_bLoaded_guess = false;
	entry_guess->m_ContentId_guess = contentId;
	return entry_guess;
}

// the colour of the palette nearest to the mean of the samples
static unsigned char MeanPaletteIndex_guess( unsigned char *pPalette, CUtlVector< unsigned char > &samples )
{
	float b_guess;
	float pad2_guess;
	float dist_guess;
	float r_guess;
	float green_guess;
	float deltaB_guess;
	float diffR_guess;
	float nearest_guess;
	int i;
	int c_guess;
	int bestColor_guess;
	float dg2_guess;

	pad2_guess = 0;
	r_guess = green_guess = b_guess = 0;

	for ( i = 0; i < samples.Count(); i++ )
	{
		c_guess = samples[i];
		r_guess += pPalette[c_guess * 3];
		green_guess += pPalette[c_guess * 3 + 1];
		b_guess += pPalette[c_guess * 3 + 2];
	}

	r_guess /= samples.Count();
	green_guess /= samples.Count();
	b_guess /= samples.Count();

	nearest_guess = 196609.0f;
	bestColor_guess = -1;
	for ( i = 0; i < 255; i++ )
	{
		c_guess = i;
		diffR_guess = r_guess - pPalette[i * 3];
		dg2_guess = green_guess - pPalette[i * 3 + 1];
		deltaB_guess = b_guess - pPalette[i * 3 + 2];
		dist_guess = diffR_guess * diffR_guess + dg2_guess * dg2_guess + deltaB_guess * deltaB_guess;
		if ( dist_guess < nearest_guess )
		{
			if ( dist_guess == 0 )
				return c_guess;

			nearest_guess = dist_guess;
			bestColor_guess = c_guess;
		}
	}

	return bestColor_guess;
}

unsigned char *CAdContent_guess::BuildMipLevels_guess( struct texture_s *pTexture, unsigned char *pSource, unsigned char *pDest, unsigned char *pPalette )
{
	int miplevel_guess;
	unsigned int step_guess;
	unsigned int x2_guess;
	unsigned int y_guess;

	for ( miplevel_guess = 1; miplevel_guess < 4; miplevel_guess++ )
	{
		pTexture->offsets[miplevel_guess] = pDest - pSource + sizeof( texture_t );
		step_guess = 1 << miplevel_guess;
		for ( y_guess = 0; y_guess < pTexture->height; y_guess += step_guess )
		{
			for ( x2_guess = 0; x2_guess < pTexture->width; x2_guess += step_guess )
			{
				CUtlVector< unsigned char > samples_guess;
				unsigned int dy_guess;
				unsigned char c_guess;
				unsigned int sx_guess;

				for ( dy_guess = 0; dy_guess < step_guess; dy_guess++ )
				{
					for ( sx_guess = 0; sx_guess < step_guess; sx_guess++ )
					{
						c_guess = pSource[( y_guess + dy_guess ) * pTexture->width + x2_guess + sx_guess];
						samples_guess.AddToTail( c_guess );
					}
				}

				*pDest = MeanPaletteIndex_guess( pPalette, samples_guess );
				pDest++;
			}
		}
	}

	return pDest;
}

void CAdContent_guess::BuildMips_guess( unsigned char *pSource, unsigned char *pDest, unsigned char *pPalette )
{
	texture_t tex_guess;

	tex_guess.width = m_nWidth_guess;
	tex_guess.height = m_nHeight_guess;
	BuildMipLevels_guess( &tex_guess, pSource, pDest, pPalette );
}

void CAdContent_guess::DrawBorder_guess( int nBorder, const Color &color, unsigned char *pData, int nWidth, int nHeight )
{
	unsigned char borderVal_guess;
	int i;
	int j;
	unsigned char *row_guess;

	if ( nBorder <= 0 )
		return;

	borderVal_guess = FindNearestColor_guess( m_Palette_guess, 256, color.r(), color.g(), color.b() );

	for ( i = 0; i < nBorder; i++ )
	{
		Q_memset( pData + i * nWidth, borderVal_guess, nWidth );
		Q_memset( pData + ( nHeight - 1 - i ) * nWidth, borderVal_guess, nWidth );
	}

	for ( i = nBorder; i < nHeight - nBorder; i++ )
	{
		row_guess = pData + i * nWidth;
		for ( j = 0; j < nBorder; j++ )
		{
			row_guess[j] = borderVal_guess;
			row_guess[nWidth - 1 - j] = borderVal_guess;
		}
	}
}

struct texture_s *CAdContent_guess::CreateTexture_guess( const char *pszName, int nWidth, int nHeight, int nBorder, const Color &borderColor )
{
	int pixels_guess;
	int dataSize_guess;
	texture_t *pTex_guess;
	int i;

	nWidth = m_nWidth_guess;
	nHeight = m_nHeight_guess;
	pixels_guess = m_nWidth_guess * m_nHeight_guess;
	dataSize_guess = pixels_guess;

	pTex_guess = (texture_t *)new byte[dataSize_guess + sizeof( texture_t )];
	Q_memset( pTex_guess, 0, sizeof( texture_t ) );
	Q_strncpy( pTex_guess->name, pszName, sizeof( pTex_guess->name ) );
	pTex_guess->width = nWidth;
	pTex_guess->height = nHeight;
	pTex_guess->anim_total = 0;
	pTex_guess->anim_min = 0;
	pTex_guess->anim_max = 0;
	pTex_guess->anim_next = NULL;
	pTex_guess->alternate_anims = NULL;

	for ( i = 0; i < 4; i++ )
		pTex_guess->offsets[i] = m_MipOffsets_guess[i] + sizeof( texture_t );

	Q_memcpy( (byte *)( pTex_guess + 1 ), m_pData_guess, dataSize_guess );

	DrawBorder_guess( nBorder, borderColor, (byte *)( pTex_guess + 1 ), m_nWidth_guess, m_nHeight_guess );

	GL_UnloadTexture( pTex_guess->name );
	pTex_guess->gl_texturenum = GL_LoadTexture( pTex_guess->name, GLT_DECAL, pTex_guess->width, pTex_guess->height, (byte *)( pTex_guess + 1 ), true, 0, m_Palette_guess );
	return pTex_guess;
}

struct texture_s *CAdContent_guess::CreateTexture_guess( const char *pszName )
{
	return CreateTexture_guess( pszName, m_nWidth_guess, m_nHeight_guess, 0, Color( 0, 0, 0 ) );
}

bool CAdContent_guess::LoadMip_guess( struct miptex_s *pMip )
{
	unsigned int pixels_guess;
	unsigned int size_guess;

	m_bLoaded_guess = true;
	m_bTexture_guess = true;
	m_nType_guess = 1;
	m_nWidth_guess = pMip->width;
	m_nHeight_guess = pMip->height;

	pixels_guess = pMip->width * pMip->height * 85 / 64;
	size_guess = pMip->width * pMip->height * 85 / 64 + 2 + 2048;
	m_pData_guess = new unsigned char[size_guess];
	m_nMagic_guess = 0xDEADBEEF;
	m_nDataSize_guess = size_guess;

	Q_memcpy( m_pData_guess, (byte *)( pMip + 1 ), pixels_guess );
	Q_memcpy( m_Palette_guess, (byte *)( pMip + 1 ) + pixels_guess + 2, 768 );

	return m_bLoaded_guess;
}

bool CInGameAdvertisingSystem::LoadDefaultContent_guess( uint64 contentId, CAdCell_guess *pCell )
{
	FileHandle_t file_guess;
	char texName_guess[16];
	struct texture_s *decalTex_guess;
	int size_guess;
	unsigned char *pFileData_guess;
	miptex_t *pMip_guess;

	pCell->m_pContent_guess = FindOrCreateContent_guess( contentId );
	pCell->m_pContent_guess->m_nCellId_guess = pCell->m_nCellId_guess;

	if ( !pCell->m_pPlaceholder_guess )
		return false;

	file_guess = FS_Open( "iga_1x1_1.iga", "rb" );
	if ( file_guess )
	{
		size_guess = FS_Size( file_guess );
		pFileData_guess = new unsigned char[size_guess + 1];
		FS_Read( pFileData_guess, size_guess, 1, file_guess );
		FS_Close( file_guess );

		pMip_guess = (miptex_t *)pFileData_guess;
		pCell->m_pContent_guess->LoadMip_guess( pMip_guess );

		_snprintf( texName_guess, sizeof( texName_guess ), "{^IGA_%04d", m_Textures_guess.Count() );

		decalTex_guess = NULL;
		if ( pCell->m_pPlaceholder_guess )
			decalTex_guess = pCell->m_pContent_guess->CreateTexture_guess( texName_guess, pCell->m_pPlaceholder_guess->m_Size_guess[0], pCell->m_pPlaceholder_guess->m_Size_guess[1], pCell->m_pPlaceholder_guess->m_nBorder_guess, pCell->m_pPlaceholder_guess->m_BorderColor_guess );
		else
			decalTex_guess = pCell->m_pContent_guess->CreateTexture_guess( texName_guess );

		if ( decalTex_guess )
			pCell->m_nDecalTexture_guess = m_Textures_guess.AddToTail( decalTex_guess );

		delete [] pFileData_guess;
	}

	return true;
}

bool CInGameAdvertisingSystem::LoadImage_guess( void *pData, int nSize, CAdContent_guess *pContent, const Color *pBorderColor )
{
	bool paletted_guess = IsPalettedImage_guess( pData, nSize );
	bool bResult_guess = false;

	if ( paletted_guess )
		bResult_guess = LoadPalettedImage_guess( pData, nSize, pContent );
	else
		bResult_guess = LoadTrueColorImage_guess( pData, nSize, pContent, pBorderColor );

	return bResult_guess;
}

void CInGameAdvertisingSystem::RequestContent_guess( void )
{
	int i;
	bool offline_guess;

	if ( !IsActive_guess() )
		return;

	m_flLastReload_guess = realtime;

	offline_guess = COM_CheckParm( "-xyzzy" ) ? true : false;

	for ( i = m_Cells_guess.FirstInorder(); i != m_Cells_guess.InvalidIndex(); i = m_Cells_guess.NextInorder( i ) )
	{
		CAdCell_guess *pcell_guess;
		const CAdPlaceholder_guess *placeholder_guess;
		int nType_guess;
		int cellid_guess;
		const Color *pBorderColor_guess;
		RadContent_guess radContent_guess;
		int error_guess;
		bool request_guess;

		pcell_guess = &m_Cells_guess[i];
		cellid_guess = pcell_guess->m_nCellId_guess;
		nType_guess = pcell_guess->m_nType_guess;

		Q_memset( &radContent_guess, 0, sizeof( radContent_guess ) );
		request_guess = true;
		error_guess = RAD_ERROR_NOCONTENT;

		if ( !offline_guess )
			error_guess = radGetContent_guess( &m_RadInfo_guess, cellid_guess, &radContent_guess );

		if ( error_guess || radContent_guess.m_nSize_guess <= 0 )
		{
			CAdCellImages_guess key_guess;
			int idx_guess;

			key_guess.m_nCellId_guess = cellid_guess;
			idx_guess = m_CellImages_guess.Find( key_guess );
			if ( idx_guess != m_CellImages_guess.InvalidIndex() )
			{
				CAdCellImages_guess *pImages_guess;
				int pick_guess;

				pImages_guess = &m_CellImages_guess[idx_guess];
				pick_guess = RandomLong( 0, pImages_guess->m_Images_guess.Count() - 1 );
				if ( pick_guess >= 0 && pick_guess < pImages_guess->m_Images_guess.Count() )
				{
					int index_guess;
					CAdCellImage_guess *image_guess;

					image_guess = &pImages_guess->m_Images_guess[pick_guess];
					index_guess = image_guess->m_nImage_guess;
					if ( index_guess >= 0 && index_guess < m_Images_guess.Count() )
					{
						CAdImage_guess *pDefault_guess;

						pDefault_guess = m_Images_guess[index_guess];
						if ( pDefault_guess->m_nSize_guess < 0x100000 )
						{
							radContent_guess.m_ContentId_guess = image_guess->m_ContentId_guess;
							radContent_guess.m_nSize_guess = pDefault_guess->m_nSize_guess;
							radContent_guess.m_pBuffer_guess = new unsigned char[radContent_guess.m_nSize_guess];
							Q_memcpy( radContent_guess.m_pBuffer_guess, pDefault_guess->m_pData_guess, radContent_guess.m_nSize_guess );
							request_guess = false;
						}
					}
				}
			}

			if ( radContent_guess.m_nSize_guess <= 0 )
			{
				Con_Printf( "Warning:  %s for cellid %d\n", GetRadErrorString_guess( error_guess ), cellid_guess );
				continue;
			}
		}

		if ( radContent_guess.m_nSize_guess > 0x100000 )
		{
			Con_Printf( "Warning:  bogus content size %u for cellid %d\n", radContent_guess.m_nSize_guess, cellid_guess );
			delete radContent_guess.m_pBuffer_guess;
			continue;
		}

		if ( request_guess )
		{
			int err2_guess;

			radContent_guess.m_pBuffer_guess = new unsigned char[radContent_guess.m_nSize_guess];
			Q_memset( radContent_guess.m_pBuffer_guess, 0, radContent_guess.m_nSize_guess );
			err2_guess = radGetContent_guess( &m_RadInfo_guess, cellid_guess, &radContent_guess );
			if ( err2_guess )
			{
				delete radContent_guess.m_pBuffer_guess;
				radContent_guess.m_pBuffer_guess = NULL;

				CAdCellImages_guess retry_guess;
				int idx2_guess;

				retry_guess.m_nCellId_guess = cellid_guess;
				idx2_guess = m_CellImages_guess.Find( retry_guess );
				if ( idx2_guess != m_CellImages_guess.InvalidIndex() )
				{
					int pick2_guess;
					CAdCellImages_guess *pImages2_guess;

					pImages2_guess = &m_CellImages_guess[idx2_guess];
					pick2_guess = RandomLong( 0, pImages2_guess->m_Images_guess.Count() - 1 );
					if ( pick2_guess >= 0 && pick2_guess < pImages2_guess->m_Images_guess.Count() )
					{
						CAdCellImage_guess *pImage2_guess;
						int index2_guess;

						pImage2_guess = &pImages2_guess->m_Images_guess[pick2_guess];
						index2_guess = pImage2_guess->m_nImage_guess;
						if ( index2_guess >= 0 && index2_guess < m_Images_guess.Count() )
						{
							CAdImage_guess *pDefault2_guess;

							pDefault2_guess = m_Images_guess[index2_guess];
							if ( pDefault2_guess->m_nSize_guess < 0x100000 )
							{
								radContent_guess.m_ContentId_guess = pImage2_guess->m_ContentId_guess;
								radContent_guess.m_nSize_guess = pDefault2_guess->m_nSize_guess;
								radContent_guess.m_pBuffer_guess = new unsigned char[radContent_guess.m_nSize_guess];
								Q_memcpy( radContent_guess.m_pBuffer_guess, pDefault2_guess->m_pData_guess, radContent_guess.m_nSize_guess );
							}
						}
					}
				}
				continue;
			}
		}

		if ( !radContent_guess.m_pBuffer_guess )
			continue;

		pcell_guess->m_pContent_guess = FindOrCreateContent_guess( radContent_guess.m_ContentId_guess );
		pcell_guess->m_pContent_guess->m_nCellId_guess = cellid_guess;
		pcell_guess->m_pContent_guess->FreeData_guess();

		placeholder_guess = pcell_guess->m_pPlaceholder_guess;
		pBorderColor_guess = NULL;
		if ( placeholder_guess && placeholder_guess->m_nBorder_guess > 0 )
			pBorderColor_guess = &placeholder_guess->m_BorderColor_guess;

		switch ( nType_guess )
		{
		default:
			{
				bool bLoaded_guess;

				bLoaded_guess = LoadImage_guess( radContent_guess.m_pBuffer_guess, radContent_guess.m_nSize_guess, pcell_guess->m_pContent_guess, pBorderColor_guess );
				if ( bLoaded_guess )
				{
					char szName_guess[16];
					struct texture_s *tex_guess;

					pcell_guess->m_pContent_guess->m_bLoaded_guess = true;
					pcell_guess->m_pContent_guess->m_bTexture_guess = true;

					_snprintf( szName_guess, sizeof( szName_guess ), "{^IGA_%04d", m_Textures_guess.Count() );

					tex_guess = NULL;
					if ( placeholder_guess )
						tex_guess = pcell_guess->m_pContent_guess->CreateTexture_guess( szName_guess, placeholder_guess->m_Size_guess[0], placeholder_guess->m_Size_guess[1], placeholder_guess->m_nBorder_guess, placeholder_guess->m_BorderColor_guess );
					else
						tex_guess = pcell_guess->m_pContent_guess->CreateTexture_guess( szName_guess );

					if ( tex_guess )
						pcell_guess->m_nDecalTexture_guess = m_Textures_guess.AddToTail( tex_guess );
				}
			}
			break;

		case ADCELL_HUD_GUESS:
			if ( LoadHUDImage_guess( radContent_guess.m_pBuffer_guess, radContent_guess.m_nSize_guess, pcell_guess->m_pContent_guess ) )
			{
				pcell_guess->m_pContent_guess->m_bLoaded_guess = true;
				pcell_guess->m_pContent_guess->m_bTexture_guess = false;

				if ( m_pClientAds_guess )
				{
					if ( pcell_guess->m_pContent_guess->m_nCellId_guess == m_HUDCells_guess[0] )
					{
						int flags_guess;

						flags_guess = 0;
						if ( pcell_guess->m_pContent_guess->m_bHUD_guess )
							flags_guess |= 0x8000;

						m_pClientAds_guess->ContentLoaded_guess( flags_guess, pcell_guess->m_pContent_guess->m_nCellId_guess, pcell_guess->m_pContent_guess->m_ContentId_guess,
							pcell_guess->m_pContent_guess->m_nWidth_guess, pcell_guess->m_pContent_guess->m_nHeight_guess, pcell_guess->m_pContent_guess->m_pData_guess );
					}
					else if ( pcell_guess->m_pContent_guess->m_nCellId_guess == m_HUDCells_guess[1] )
					{
						int flags2_guess;

						flags2_guess = 1;
						if ( pcell_guess->m_pContent_guess->m_bHUD_guess )
							flags2_guess |= 0x8000;

						m_pClientAds_guess->ContentLoaded_guess( flags2_guess, pcell_guess->m_pContent_guess->m_nCellId_guess, pcell_guess->m_pContent_guess->m_ContentId_guess,
							pcell_guess->m_pContent_guess->m_nWidth_guess, pcell_guess->m_pContent_guess->m_nHeight_guess, pcell_guess->m_pContent_guess->m_pData_guess );
					}
				}

				delete [] pcell_guess->m_pContent_guess->m_pData_guess;
				pcell_guess->m_pContent_guess->m_pData_guess = NULL;
			}
			break;
		}

		delete radContent_guess.m_pBuffer_guess;
		radContent_guess.m_pBuffer_guess = NULL;
	}
}

void CInGameAdvertisingSystem::FreeTextures_guess( void )
{
	int total_guess;
	int i;

	total_guess = m_Textures_guess.Count();
	for ( i = 0; i < total_guess; i++ )
	{
		GL_UnloadTexture( m_Textures_guess[i]->name );
		delete m_Textures_guess[i];
	}
	m_Textures_guess.RemoveAll();
}

void CInGameAdvertisingSystem::Reload_f_guess( void )
{
	if ( !allow_cheats )
		return;

	m_bReload_guess = true;
}

void CInGameAdvertisingSystem::Frame_guess( struct model_s *worldmodel )
{
	if ( !IsActive_guess() )
		return;

	if ( m_bReload_guess )
	{
		m_bReload_guess = false;
		R_DecalRemoveWithFlag( 0x80 );
		LoadMetadata_guess();
		LevelLoaded_guess( worldmodel );
	}
}

void CInGameAdvertisingSystem::ListContent_guess( void )
{
	int total_guess;
	int i;
	CAdContent_guess *entry_guess;

	total_guess = m_Content_guess.Count();
	for ( i = 0; i < total_guess; i++ )
	{
		entry_guess = m_Content_guess[i];
		Con_Printf( "%d:  cellid[ %u ] contentid [%I64u] %d x %d\n", i + 1, entry_guess->m_nCellId_guess, entry_guess->m_ContentId_guess, entry_guess->m_nWidth_guess, entry_guess->m_nHeight_guess );
	}
}

void CInGameAdvertisingSystem::ImpressionShown_guess( const ClientAdImpression_guess *pImpression )
{
	float angle_guess;
	int result_guess;

	if ( !IsActive_guess() )
		return;

	angle_guess = 91.0f;
	if ( pImpression->m_bAngleZero_guess )
		angle_guess = 0;

	if ( pImpression->m_ContentId_guess & 0x80000000 )
		return;

	result_guess = radImpression_guess( &m_RadInfo_guess, pImpression->m_nCellId_guess, pImpression->m_ContentId_guess, pImpression->m_flArea_guess, angle_guess );
	if ( result_guess == RAD_IMPRESSION_SENT )
		TrackImpression_guess( pImpression->m_nCellId_guess, pImpression->m_ContentId_guess );
}

void CInGameAdvertisingSystem::RequestReload_guess( void )
{
	if ( !IsActive_guess() )
		return;

	if ( m_flLastReload_guess + 240.0f <= realtime )
		m_bReload_guess = true;
}

void CInGameAdvertisingSystem::ShootDecal_guess( int textureIndex, int entity, int modelIndex, float *position, int flags, float flScale )
{
	R_DecalShootScaled( textureIndex, entity, modelIndex, position, flags, flScale );
}

void CInGameAdvertisingSystem::Shoot_f_guess( void )
{
	trace_t trace_guess;
	vec3_t end_guess;
	vec3_t dir2_guess;
	struct model_s *worldmodel_guess;
	vec3_t normal_guess;
	vec3_t vecPos_guess;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "Usage:  iga_shoot index\n" );
		return;
	}

	worldmodel_guess = CL_GetWorldModel();
	if ( !worldmodel_guess )
	{
		Con_Printf( "Must be in a level\n" );
		return;
	}

	Q_memset( &trace_guess, 0, sizeof( trace_guess ) );
	trace_guess.fraction = 1.0f;
	trace_guess.allsolid = true;

	VectorMA( m_vecViewOrigin_guess, 1024.0f, m_vecViewForward_guess, end_guess );
	VectorCopy( end_guess, trace_guess.endpos );

	SV_RecursiveHullCheck( &worldmodel_guess->hulls[0], 0, 0.0f, 1.0f, m_vecViewOrigin_guess, end_guess, &trace_guess );
	if ( trace_guess.fraction == 1.0f )
	{
		Con_Printf( "Missed\n" );
		return;
	}

	VectorScale( m_vecViewForward_guess, -1.0f, dir2_guess );
	VectorMA( trace_guess.endpos, 2.0f, dir2_guess, vecPos_guess );
	VectorCopy( trace_guess.plane.normal, normal_guess );
	VectorScale( normal_guess, -1.0f, normal_guess );

	Con_Printf( "Hit %.2f %.2f %.2f\n", vecPos_guess[0], vecPos_guess[1], vecPos_guess[2] );
	Con_Printf( "Normal %.2f %.2f %.2f\n", normal_guess[0], normal_guess[1], normal_guess[2] );

	if ( !allow_cheats )
		return;

	if ( Cmd_Argv( 1 )[0] == '{' || Cmd_Argv( 1 )[0] == '}' )
	{
		int decalFlags_guess;
		int modelIndex_guess;
		int entity_guess;
		int textureIndex_guess;

		textureIndex_guess = Draw_DecalIndex( Draw_DecalIndexFromName( Cmd_Argv( 1 ) ) );
		modelIndex_guess = 0;
		decalFlags_guess = 0x80;
		entity_guess = 0;
		ShootDecal_guess( textureIndex_guess, entity_guess, modelIndex_guess, vecPos_guess, decalFlags_guess, 1.0f );
	}
	else
	{
		int flags2_guess;
		int modelIndex2_guess;
		int number_guess;
		int nDecal2_guess;
		int ent2_guess;

		number_guess = (int)Q_atof( Cmd_Argv( 1 ) );
		nDecal2_guess = number_guess | 0x4000;
		modelIndex2_guess = 0;
		flags2_guess = 0;
		ent2_guess = 0;
		ShootDecal_guess( nDecal2_guess, ent2_guess, modelIndex2_guess, vecPos_guess, flags2_guess, 1.0f );
	}
}

void DecryptBlock_guess( IceKey &key, unsigned char *pIn, unsigned char *pOut )
{
	key.decrypt( pIn, pOut );
}

void DecryptBuffer_guess( IceKey &key, unsigned char *pBuffer, int nSize )
{
	unsigned char *out_guess = pBuffer;
	unsigned char *pIn_guess = pBuffer;
	int i = 0;

	while ( i < nSize )
	{
		DecryptBlock_guess( key, pIn_guess, out_guess );
		i += 8;
		out_guess += 8;
		pIn_guess += 8;
	}
}

bool CInGameAdvertisingSystem::LoadEncryptedBuffer_guess( const char *pszFile, CUtlBuffer &buf )
{
	IceKey key_guess( 4 );
	FileHandle_t f_guess;

	key_guess.set( g_IGAKey_guess );

	f_guess = FS_Open( pszFile, "rb" );
	if ( f_guess )
	{
		unsigned char *pSrc_guess;
		int realsize_guess;
		unsigned char *pFile_guess;
		unsigned char *out_guess;
		int left_guess;
		int chunk2_guess;
		unsigned char *work_guess;
		int nSize_guess;
		unsigned char *result_guess;

		nSize_guess = FS_Size( f_guess );
		pFile_guess = new unsigned char[nSize_guess + 1];
		FS_Read( pFile_guess, nSize_guess, 1, f_guess );
		FS_Close( f_guess );
		pFile_guess[nSize_guess] = 0;

		out_guess = new unsigned char[nSize_guess + 1];
		result_guess = out_guess;
		left_guess = nSize_guess;
		realsize_guess = *(int *)pFile_guess;
		left_guess -= 4;
		pSrc_guess = pFile_guess + 4;
		chunk2_guess = 0x40000;
		work_guess = new unsigned char[chunk2_guess];

		while ( left_guess > 0 )
		{
			int len_guess;
			int aligned_guess;

			len_guess = left_guess < chunk2_guess ? left_guess : chunk2_guess;
			memcpy( work_guess, pSrc_guess, len_guess );
			pSrc_guess += len_guess;

			aligned_guess = len_guess;
			while ( aligned_guess % 8 )
			{
				work_guess[aligned_guess] = 0;
				aligned_guess++;
			}

			DecryptBuffer_guess( key_guess, work_guess, aligned_guess );
			memcpy( out_guess, work_guess, len_guess );
			out_guess += len_guess;
			left_guess -= len_guess;
		}

		delete [] work_guess;
		delete [] pFile_guess;

		result_guess[nSize_guess] = 0;
		buf.Put( result_guess, realsize_guess );
		return true;
	}

	return false;
}

char *CInGameAdvertisingSystem::LoadEncryptedFile_guess( void )
{
	bool bUnused_guess;
	unsigned char *filebuf_guess;
	char metafile_guess[128];
	int fileLen_guess;

	filebuf_guess = NULL;
	fileLen_guess = 0;
	bUnused_guess = true;

	Q_strncpy( metafile_guess, "iga", sizeof( metafile_guess ) );
	COM_DefaultExtension( metafile_guess, ".dat" );

	IceKey key_guess( 4 );
	key_guess.set( g_IGAKey_guess );

	FileHandle_t f_guess;
	f_guess = g_pFileSystem->OpenFromCacheForRead( metafile_guess, "rb" );
	if ( f_guess )
	{
		unsigned char *pSrc_guess;
		int realsize_guess;
		unsigned char *out_guess;
		int left_guess;
		int chunk2_guess;
		unsigned char *work_guess;
		unsigned char *result_guess;

		fileLen_guess = FS_Size( f_guess );
		filebuf_guess = new unsigned char[fileLen_guess + 1];
		FS_Read( filebuf_guess, fileLen_guess, 1, f_guess );
		FS_Close( f_guess );
		filebuf_guess[fileLen_guess] = 0;

		out_guess = new unsigned char[fileLen_guess + 1];
		result_guess = out_guess;
		left_guess = fileLen_guess;
		realsize_guess = *(int *)filebuf_guess;
		left_guess -= 4;
		pSrc_guess = filebuf_guess + 4;
		chunk2_guess = 0x40000;
		work_guess = new unsigned char[chunk2_guess];

		while ( left_guess > 0 )
		{
			int len_guess;
			int aligned_guess;

			len_guess = left_guess < chunk2_guess ? left_guess : chunk2_guess;
			memcpy( work_guess, pSrc_guess, len_guess );
			pSrc_guess += len_guess;

			aligned_guess = len_guess;
			while ( aligned_guess % 8 )
			{
				work_guess[aligned_guess] = 0;
				aligned_guess++;
			}

			DecryptBuffer_guess( key_guess, work_guess, aligned_guess );
			memcpy( out_guess, work_guess, len_guess );
			out_guess += len_guess;
			left_guess -= len_guess;
		}

		delete [] work_guess;
		delete [] filebuf_guess;

		result_guess[fileLen_guess] = 0;
		return (char *)result_guess;
	}

	return NULL;
}

bool CInGameAdvertisingSystem::ParsePlaceholders_guess( KeyValues *pKeys )
{
	bool bOK_guess;
	KeyValues *pFile_guess;

	if ( m_bMetadataLoaded_guess )
		return true;

	bOK_guess = true;
	for ( pFile_guess = pKeys->GetFirstSubKey(); pFile_guess; pFile_guess = pFile_guess->GetNextKey() )
	{
		KeyValues *pCell_guess;
		KeyValues *pImages_guess;
		const char *pszFile_guess;

		pszFile_guess = pFile_guess->GetString( "name", "" );
		if ( !pszFile_guess || !*pszFile_guess )
			continue;

		pImages_guess = pFile_guess->FindKey( "images" );
		if ( !pImages_guess )
			continue;

		CUtlBuffer buffer_guess( 0, 0, false );
		if ( !LoadEncryptedBuffer_guess( pszFile_guess, buffer_guess ) )
		{
			bOK_guess = false;
			continue;
		}

		if ( !AddImage_guess( m_Images_guess, pszFile_guess, buffer_guess.Base(), buffer_guess.TellPut() ) )
		{
			bOK_guess = false;
			continue;
		}

		for ( pCell_guess = pImages_guess->GetFirstSubKey(); pCell_guess; pCell_guess = pCell_guess->GetNextKey() )
		{
			KeyValues *image_guess;
			int cellid_guess;

			cellid_guess = Q_atoi( pCell_guess->GetName() );
			for ( image_guess = pCell_guess->GetFirstSubKey(); image_guess; image_guess = image_guess->GetNextKey() )
			{
				uint64 cont_guess;
				const char *imagename_guess;
				int idx_guess;
				CAdCellImages_guess *pImages2_guess;
				int j;

				cont_guess = _atoi64( image_guess->GetName() ) | 0x80000000;
				imagename_guess = image_guess->GetString();

				CAdCellImages_guess lookup_guess;
				lookup_guess.m_nCellId_guess = cellid_guess;
				idx_guess = m_CellImages_guess.Find( lookup_guess );
				if ( idx_guess == m_CellImages_guess.InvalidIndex() )
					idx_guess = m_CellImages_guess.Insert( lookup_guess );

				pImages2_guess = &m_CellImages_guess[idx_guess];
				for ( j = 0; j < m_Images_guess.Count(); j++ )
				{
					if ( !Q_stricmp( imagename_guess, m_Images_guess[j]->m_szName_guess ) )
					{
						CAdCellImage_guess newImage_guess;
						newImage_guess.m_nImage_guess = j;
						newImage_guess.m_ContentId_guess = cont_guess;
						pImages2_guess->m_Images_guess.AddToTail( newImage_guess );
						break;
					}
				}
			}
		}
	}

	return bOK_guess;
}

void CInGameAdvertisingSystem::LoadMetadata_guess( void )
{
	const char *errmsg_guess;
	bool ok_guess;
	KeyValues *pKeys_guess;
	char *pBuffer_guess;
	KeyValues *pLevel_guess;

	m_Levels_guess.RemoveAll();

	errmsg_guess = "Error 5899:  Metadata file missing or damaged [%d]!!!\n";
	ok_guess = true;

	pKeys_guess = new KeyValues( "db" );
	pBuffer_guess = LoadEncryptedFile_guess();
	if ( pBuffer_guess )
	{
		pKeys_guess->LoadFromBuffer( pBuffer_guess );

		for ( pLevel_guess = pKeys_guess->GetFirstSubKey(); pLevel_guess; pLevel_guess = pLevel_guess->GetNextKey() )
		{
			const char *pszName_guess;

			pszName_guess = pLevel_guess->GetName();
			if ( !Q_stricmp( pszName_guess, "placeholders" ) )
			{
				if ( !ParsePlaceholders_guess( pLevel_guess ) )
				{
					ok_guess = false;
					Sys_Error( (char *)errmsg_guess, -4 );
				}
				continue;
			}

			CAdLevel_guess level_guess;
			int count_guess;
			KeyValues *pItem_guess;

			Q_strncpy( level_guess.m_szName_guess, pszName_guess, sizeof( level_guess.m_szName_guess ) );
			count_guess = 1;

			for ( pItem_guess = pLevel_guess->GetFirstSubKey(); pItem_guess; pItem_guess = pItem_guess->GetNextKey() )
			{
				CAdPlaceholder_guess ph_guess;
				bool bRounded_guess;
				vec3_t vForward_guess;
				vec3_t vAngles_guess;
				int i;
				KeyValues *pDecals_guess;

				ph_guess.m_nCellId_guess = pItem_guess->GetInt( "cellid", 0 );
				if ( ph_guess.m_nCellId_guess <= 0 )
				{
					Con_Printf( "Bogus cellid[ %d ] in %s [entry %d]\n", ph_guess.m_nCellId_guess, pszName_guess, count_guess );
					ok_guess = false;
				}

				if ( sscanf( pItem_guess->GetString( "pos", "0 0 0" ), "%f %f %f", &ph_guess.m_vecOrigin_guess[0], &ph_guess.m_vecOrigin_guess[1], &ph_guess.m_vecOrigin_guess[2] ) != 3 )
				{
					Con_Printf( "Bogus position[ %f %f %f ] in %s [entry %d]\n", ph_guess.m_vecOrigin_guess[0], ph_guess.m_vecOrigin_guess[1], ph_guess.m_vecOrigin_guess[2], pszName_guess, count_guess );
					ok_guess = false;
				}

				if ( sscanf( pItem_guess->GetString( "normal", "0 0 0" ), "%f %f %f", &ph_guess.m_vecNormal_guess[0], &ph_guess.m_vecNormal_guess[1], &ph_guess.m_vecNormal_guess[2] ) != 3 )
				{
					Con_Printf( "Bogus normal[ %f %f %f ] in %s [entry %d]\n", ph_guess.m_vecNormal_guess[0], ph_guess.m_vecNormal_guess[1], ph_guess.m_vecNormal_guess[2], pszName_guess, count_guess );
					ok_guess = false;
				}

				if ( sscanf( pItem_guess->GetString( "size", "256 256" ), "%d %d", &ph_guess.m_Size_guess[0], &ph_guess.m_Size_guess[1] ) != 2 )
				{
					Con_Printf( "Bogus size[ %d %d ] in %s [entry %d]\n", ph_guess.m_Size_guess[0], ph_guess.m_Size_guess[1], pszName_guess, count_guess );
					ok_guess = false;
				}

				ph_guess.m_nBorder_guess = pItem_guess->GetInt( "border", 0 );
				ph_guess.m_BorderColor_guess = pItem_guess->GetColor( "bordercolor" );

				bRounded_guess = false;
				for ( i = 0; i < 2; i++ )
				{
					if ( ph_guess.m_Size_guess[i] % 16 )
					{
						ph_guess.m_Size_guess[i] = ( ph_guess.m_Size_guess[i] + 15 ) & ~15;
						bRounded_guess = true;
					}
				}

				pDecals_guess = pItem_guess->FindKey( "decals" );
				if ( pDecals_guess )
				{
					KeyValues *pDecal_guess;

					for ( pDecal_guess = pDecals_guess->GetFirstSubKey(); pDecal_guess; pDecal_guess = pDecal_guess->GetNextKey() )
					{
						CAdDecal_guess decal_guess;

						Q_strncpy( decal_guess.m_szName_guess, pDecal_guess->GetString( "name", "" ), sizeof( decal_guess.m_szName_guess ) );
						if ( !decal_guess.m_szName_guess[0] )
						{
							Con_Printf( " Bogus decal name in %s [entry %d]\n", pszName_guess, count_guess );
							ok_guess = false;
							break;
						}

						if ( sscanf( pDecal_guess->GetString( "pos", "0 0 0" ), "%f %f %f", &decal_guess.m_vecOrigin_guess[0], &decal_guess.m_vecOrigin_guess[1], &decal_guess.m_vecOrigin_guess[2] ) != 3 )
						{
							Con_Printf( " Bogus position[ %f %f %f ] in %s [entry %d] for decal %s\n", decal_guess.m_vecOrigin_guess[0], decal_guess.m_vecOrigin_guess[1], decal_guess.m_vecOrigin_guess[2], pszName_guess, count_guess, decal_guess.m_szName_guess );
							ok_guess = false;
							break;
						}

						ph_guess.m_Decals_guess.AddToTail( decal_guess );
					}
				}

				if ( bRounded_guess && g_bWarnSize_guess )
					Con_Printf( "Forcing cellid [%d] to have dimensions which are multiples of 16 [%d x %d]\n", ph_guess.m_nCellId_guess, ph_guess.m_Size_guess[0], ph_guess.m_Size_guess[1] );

				count_guess++;

				VectorAngles( ph_guess.m_vecNormal_guess, vAngles_guess );
				AngleVectors( vAngles_guess, vForward_guess, ph_guess.m_vecRight_guess, ph_guess.m_vecUp_guess );

				level_guess.m_Placeholders_guess.AddToTail( ph_guess );
			}

			m_Levels_guess.Insert( level_guess );
		}

		delete pBuffer_guess;

		if ( m_Levels_guess.Count() <= 0 )
			Sys_Error( (char *)errmsg_guess, -2 );
	}
	else
	{
		Sys_Error( (char *)errmsg_guess, -1 );
	}

	if ( !ok_guess )
		Sys_Error( (char *)errmsg_guess, -3 );

	pKeys_guess->deleteThis();
	g_bWarnSize_guess = false;
}

void CInGameAdvertisingSystem::SetupPlaceholders_guess( struct model_s *worldmodel )
{
	int index_guess;
	int total_guess;
	char mapname_guess[256];
	const CAdLevel_guess *pLevel_guess;
	int i;

	COM_FileBase( worldmodel->name, mapname_guess );

	CAdLevel_guess tmp_guess;
	Q_strncpy( tmp_guess.m_szName_guess, mapname_guess, sizeof( tmp_guess.m_szName_guess ) );
	index_guess = m_Levels_guess.Find( tmp_guess );
	if ( index_guess == m_Levels_guess.InvalidIndex() )
		return;

	pLevel_guess = &m_Levels_guess[index_guess];
	total_guess = pLevel_guess->m_Placeholders_guess.Count();
	for ( i = 0; i < total_guess; i++ )
	{
		const CAdPlaceholder_guess *placeholder_guess;

		placeholder_guess = &pLevel_guess->m_Placeholders_guess[i];

		CAdCell_guess newcell_guess;
		int cellIndex_guess;

		newcell_guess.m_nCellId_guess = placeholder_guess->m_nCellId_guess;
		cellIndex_guess = m_Cells_guess.Find( newcell_guess );
		if ( cellIndex_guess != m_Cells_guess.InvalidIndex() )
			continue;

		newcell_guess.m_nType_guess = ADCELL_PLACEHOLDER_GUESS;
		newcell_guess.m_pPlaceholder_guess = placeholder_guess;
		m_Cells_guess.Insert( newcell_guess );
	}
}

void CInGameAdvertisingSystem::PlaceDecals_guess( struct model_s *worldmodel )
{
	int index_guess;
	int total_guess;
	char mapname_guess[256];
	CAdLevel_guess *pLevel_guess;
	int i;

	COM_FileBase( worldmodel->name, mapname_guess );

	CAdLevel_guess tmp_guess;
	Q_strncpy( tmp_guess.m_szName_guess, mapname_guess, sizeof( tmp_guess.m_szName_guess ) );
	index_guess = m_Levels_guess.Find( tmp_guess );
	if ( index_guess == m_Levels_guess.InvalidIndex() )
		return;

	pLevel_guess = &m_Levels_guess[index_guess];
	total_guess = pLevel_guess->m_Placeholders_guess.Count();
	for ( i = 0; i < total_guess; i++ )
	{
		CAdPlaceholder_guess *placeholder_guess;
		CAdCell_guess *cellPtr_guess;
		int mdl_guess;
		int decalFlags_guess;
		int textureIndex_guess;
		float scaleVal_guess;
		int ent_guess;
		int nCell_guess;

		placeholder_guess = &pLevel_guess->m_Placeholders_guess[i];

		CAdCell_guess cell2_guess;
		cell2_guess.m_nCellId_guess = placeholder_guess->m_nCellId_guess;
		nCell_guess = m_Cells_guess.Find( cell2_guess );
		if ( nCell_guess == m_Cells_guess.InvalidIndex() )
			continue;

		cellPtr_guess = &m_Cells_guess[nCell_guess];
		if ( cellPtr_guess->m_nDecalTexture_guess == -1 )
			continue;

		m_Cells_guess[nCell_guess].m_bVisible_guess = true;

		textureIndex_guess = cellPtr_guess->m_nDecalTexture_guess | 0x4000;
		mdl_guess = 0;
		decalFlags_guess = 0x81;
		if ( cellPtr_guess->m_pContent_guess->m_bHUD_guess )
			decalFlags_guess |= 2;
		ent_guess = 0;
		placeholder_guess->m_flScale_guess = 1.0f;
		scaleVal_guess = 1.0f;
		scaleVal_guess = (float)placeholder_guess->m_Size_guess[0] / cellPtr_guess->m_pContent_guess->m_nWidth_guess;
		ShootDecal_guess( textureIndex_guess, ent_guess, mdl_guess, placeholder_guess->m_vecOrigin_guess, decalFlags_guess, scaleVal_guess );

		if ( gDecalAppliedScale != -1 )
			placeholder_guess->m_flScale_guess = 1.0f / gDecalAppliedScale;

		if ( !cellPtr_guess->m_pContent_guess->m_bHUD_guess )
		{
			int j;

			for ( j = 0; j < placeholder_guess->m_Decals_guess.Count(); j++ )
			{
				int fl2_guess;
				CAdDecal_guess *d2_guess;
				int nModel2_guess;
				int ent2_guess;

				d2_guess = &placeholder_guess->m_Decals_guess[j];
				textureIndex_guess = Draw_DecalIndex( Draw_DecalIndexFromName( d2_guess->m_szName_guess ) );
				nModel2_guess = 0;
				fl2_guess = 0x81;
				ent2_guess = 0;
				ShootDecal_guess( textureIndex_guess, ent2_guess, nModel2_guess, d2_guess->m_vecOrigin_guess, fl2_guess, 1.0f );
			}
		}
	}

	R_DecalRemoveWithFlag( 2 );
}

struct texture_s *CInGameAdvertisingSystem::GetDecalTexture_guess( int index )
{
	struct texture_s *tex_guess;

	index &= ~0x4000;
	if ( index < 0 || index >= m_Textures_guess.Count() )
		return NULL;

	tex_guess = m_Textures_guess[index];
	return tex_guess;
}

void CInGameAdvertisingSystem::TrackImpression_guess( int nCellId, uint64 contentId )
{
	char szInfo_guess[64];

	if ( SteamUser() )
	{
		_snprintf( szInfo_guess, sizeof( szInfo_guess ), "%d %I64u", nCellId, contentId );
		CGameID gameID_guess( 10 );
		SteamUser()->TrackAppUsageEvent( gameID_guess, 7, szInfo_guess );
	}
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CInGameAdvertisingSystem, IClientDLLInGameAdvertising, CLIENTDLLINGAMEADVERTISING_INTERFACE_VERSION, g_InGameAdvertisingSystem );
