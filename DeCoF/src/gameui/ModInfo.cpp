//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: The mod's liblist.gam data
//
// $NoKeywords: $
//=============================================================================

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "ModInfo.h"
#include "EngineInterface.h"

#include <FileSystem.h>
#include <vgui/KeyValues.h>
#include <vgui_controls/Controls.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CModInfo &ModInfo()
{
	static CModInfo s_ModInfo;
	return s_ModInfo;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CModInfo::CModInfo()
{
	m_pModData = new KeyValues( "ModData" );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CModInfo::~CModInfo()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CModInfo::FreeModInfo()
{
	if ( m_pModData )
	{
		m_pModData->deleteThis();
		m_pModData = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CModInfo::GetStartMap()
{
	return m_pModData->GetString( "startmap", "c0a0" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CModInfo::GetTrainMap()
{
	return m_pModData->GetString( "trainmap", "t0a0" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CModInfo::IsMultiplayerOnly()
{
	return ( stricmp( m_pModData->GetString( "type", "" ), "multiplayer_only" ) == 0 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CModInfo::IsSinglePlayerOnly()
{
	return ( stricmp( m_pModData->GetString( "type", "" ), "singleplayer_only" ) == 0 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CModInfo::NoModels()
{
	return ( stricmp( m_pModData->GetString( "nomodels", "0" ), "1" ) == 0 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CModInfo::NoHiModel()
{
	return ( stricmp( m_pModData->GetString( "nohimodel", "0" ), "1" ) == 0 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CModInfo::GetFallbackDir()
{
	return m_pModData->GetString( "fallback_dir", "" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CModInfo::UseFallbackDirMaps()
{
	return ( stricmp( m_pModData->GetString( "fallback_maps", "1" ), "1" ) == 0 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CModInfo::GetDetailedTexture()
{
	return ( stricmp( m_pModData->GetString( "detailed_textures", "0" ), "1" ) == 0 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CModInfo::GetGameDescription()
{
	return m_pModData->GetString( "game", "Cry of Fear" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CModInfo::GetMPFilter()
{
	return m_pModData->GetString( "mpfilter", "" );
}

//-----------------------------------------------------------------------------
// Purpose: loads liblist.gam of the current mod
//-----------------------------------------------------------------------------
void CModInfo::LoadCurrentGameInfo()
{
	// Load up liblist.gam for the current mod
	const char *filename = "liblist.gam";

	// walk the directory structure looking for it
	FileHandle_t fh = filesystem()->Open( filename, "rb" );
	if ( fh )
	{
		int len = filesystem()->Size( fh );
		if ( len > 0 )
		{
			char *buf = (char *)_alloca( len + 1 );
			filesystem()->Read( buf, len, fh );
			buf[len] = 0;
			LoadGameInfoFromBuffer( buf, len );
		}

		filesystem()->Close( fh );
	}
}

//-----------------------------------------------------------------------------
// Purpose: loads the key/value pairs of a liblist.gam file
//-----------------------------------------------------------------------------
void CModInfo::LoadGameInfoFromBuffer( const char *buffer, int bufferSize )
{
	char token[1024];
	bool done = false;
	char *p = (char *)buffer;

	while ( !done && p )
	{
		char key[256];
		char value[256];

		p = engine->COM_ParseFile( p, token );
		if ( strlen( token ) <= 0 )
			break;
		strcpy( key, token );

		p = engine->COM_ParseFile( p, token );
		strcpy( value, token );

		m_pModData->SetString( key, value );
	}
}
