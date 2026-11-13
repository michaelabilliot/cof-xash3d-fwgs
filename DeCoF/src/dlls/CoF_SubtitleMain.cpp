//=========================================================
// subtitle_main: prints line CurrentLine of the
// subtitle file of the selected language
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include <fstream>
#include <string>

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );

// CoF: declared in cof_subtitlemain.h in the original source
class CSubtitle : public CBaseEntity
{
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	float CurrentLine;

	void KeyValue( KeyValueData *pkvd );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
};

TYPEDESCRIPTION	CSubtitle::m_SaveData[] =
{
	DEFINE_FIELD( CSubtitle, CurrentLine, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CSubtitle, CBaseEntity );

LINK_ENTITY_TO_CLASS( subtitle_main, CSubtitle );

void CSubtitle::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CSubtitle::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "linenumber" ) )
	{
		CurrentLine = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CSubtitle::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	char sFileName[256];
	GET_GAME_DIR( sFileName );

	int language = 0;
	language = CVAR_GET_FLOAT( "cof_subtitlelanguage" );

	switch ( language )
	{
	case 0:
		strcat( sFileName, "/txtfiles/subtitles.txt" );
		break;
	case 1:
		strcat( sFileName, "/txtfiles/subtitles.txt" );
		break;
	case 2:
		strcat( sFileName, "/txtfiles/languages/dutch/subtitles.txt" );
		break;
	case 3:
		strcat( sFileName, "/txtfiles/languages/french/subtitles.txt" );
		break;
	case 4:
		strcat( sFileName, "/txtfiles/languages/german/subtitles.txt" );
		break;
	case 5:
		strcat( sFileName, "/txtfiles/languages/norwegian/subtitles.txt" );
		break;
	case 6:
		strcat( sFileName, "/txtfiles/languages/spanish/subtitles.txt" );
		break;
	case 7:
		strcat( sFileName, "/txtfiles/languages/swedish/subtitles.txt" );
		break;
	default:
		strcat( sFileName, "/txtfiles/subtitles.txt" );
		break;
	}

	std::ifstream file( sFileName );
	std::string line;
	char DestinationText[256];
	std::string SourceString;

	line = CurrentLine;

	int colour = 5;
	int i = 1;

	while ( getline( file, line ) )
	{
		if ( i == CurrentLine )
		{
			if ( line.length() > 254 )
			{
				UTIL_CenterPrintAll( "Your message is too long. Rewrite it!" );
				continue;
			}

			strcpy( DestinationText, line.c_str() );
			DestinationText[line.length()] = 0;
		}

		i++;
	}

	unsigned int l = 0;

	while ( l < strlen( DestinationText ) )
	{
		l++;

		// a star in the line selects the other colour and ends the text
		if ( !strncmp( &DestinationText[l], "*", 1 ) )
		{
			colour = 3;

			DestinationText[l] = 0;
		}
	}

	if ( strlen( DestinationText ) > 1 )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr )
				UTIL_ProMessage( plr, DestinationText, colour, false );
		}
	}
}
