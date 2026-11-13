//=========================================================
// subtitle_linechange: sets the line of subtitle_main
// and plays it
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"

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

class CSubtitleChange : public CBaseEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	int LineToChange;

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
};

TYPEDESCRIPTION	CSubtitleChange::m_SaveData[] =
{
	DEFINE_FIELD( CSubtitleChange, LineToChange, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CSubtitleChange, CBaseEntity );

LINK_ENTITY_TO_CLASS( subtitle_linechange, CSubtitleChange );

void CSubtitleChange::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CSubtitleChange::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "linechange" ) )
	{
		LineToChange = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CSubtitleChange::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pSubMain = UTIL_FindEntityByClassname( NULL, "subtitle_main" );

	if ( pSubMain )
	{
		CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

		if ( pPlayer )
		{
			if ( LineToChange )
			{
				GetClassPtr( (CSubtitle *)pSubMain->pev )->CurrentLine = LineToChange;
				pSubMain->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );
			}
			else
				ALERT( at_console, "SUBTITLE CHANGE: Could not change number!\n" );
		}
	}
}
