//=========================================================
// subtitle_multiple: plays a list of subtitle_main lines,
// each one after its own delay
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

class CSubtitleMultiple : public CBaseEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	void EXPORT ChangeSubtitle( void );

	int m_iSubtitleNumber[32];
	float m_fSubtitleNumberNext[32];
	int m_iCurrentSubtitle;
	float m_flStartTime;

	static	TYPEDESCRIPTION m_SaveData[];
};

// CoF: the arrays have 32 entries but 33 are saved (original bug)
TYPEDESCRIPTION	CSubtitleMultiple::m_SaveData[] =
{
	DEFINE_ARRAY( CSubtitleMultiple, m_iSubtitleNumber, FIELD_INTEGER, 33 ),
	DEFINE_ARRAY( CSubtitleMultiple, m_fSubtitleNumberNext, FIELD_FLOAT, 33 ),
	DEFINE_FIELD( CSubtitleMultiple, m_iCurrentSubtitle, FIELD_INTEGER ),
	DEFINE_FIELD( CSubtitleMultiple, m_flStartTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CSubtitleMultiple, CBaseEntity );

LINK_ENTITY_TO_CLASS( subtitle_multiple, CSubtitleMultiple );

void CSubtitleMultiple::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;

	if ( !m_iCurrentSubtitle )
		m_iCurrentSubtitle = 1;
}

// CoF: the keys are numbered from 1, so "subtitlenumber32" and
// "subtitletime32" write one entry past the end of their arrays
// (original bug)
void CSubtitleMultiple::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "subtitlenumber1" ) )
	{
		m_iSubtitleNumber[1] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber2" ) )
	{
		m_iSubtitleNumber[2] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber3" ) )
	{
		m_iSubtitleNumber[3] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber4" ) )
	{
		m_iSubtitleNumber[4] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber5" ) )
	{
		m_iSubtitleNumber[5] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber6" ) )
	{
		m_iSubtitleNumber[6] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber7" ) )
	{
		m_iSubtitleNumber[7] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber8" ) )
	{
		m_iSubtitleNumber[8] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber9" ) )
	{
		m_iSubtitleNumber[9] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber10" ) )
	{
		m_iSubtitleNumber[10] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber11" ) )
	{
		m_iSubtitleNumber[11] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber12" ) )
	{
		m_iSubtitleNumber[12] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber13" ) )
	{
		m_iSubtitleNumber[13] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber14" ) )
	{
		m_iSubtitleNumber[14] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber15" ) )
	{
		m_iSubtitleNumber[15] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber16" ) )
	{
		m_iSubtitleNumber[16] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber17" ) )
	{
		m_iSubtitleNumber[17] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber18" ) )
	{
		m_iSubtitleNumber[18] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber19" ) )
	{
		m_iSubtitleNumber[19] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber20" ) )
	{
		m_iSubtitleNumber[20] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber21" ) )
	{
		m_iSubtitleNumber[21] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber22" ) )
	{
		m_iSubtitleNumber[22] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber23" ) )
	{
		m_iSubtitleNumber[23] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber24" ) )
	{
		m_iSubtitleNumber[24] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber25" ) )
	{
		m_iSubtitleNumber[25] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber26" ) )
	{
		m_iSubtitleNumber[26] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber27" ) )
	{
		m_iSubtitleNumber[27] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber28" ) )
	{
		m_iSubtitleNumber[28] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber29" ) )
	{
		m_iSubtitleNumber[29] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber30" ) )
	{
		m_iSubtitleNumber[30] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber31" ) )
	{
		m_iSubtitleNumber[31] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitlenumber32" ) )
	{
		m_iSubtitleNumber[32] = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime1" ) )
	{
		m_fSubtitleNumberNext[1] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime2" ) )
	{
		m_fSubtitleNumberNext[2] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime3" ) )
	{
		m_fSubtitleNumberNext[3] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime4" ) )
	{
		m_fSubtitleNumberNext[4] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime5" ) )
	{
		m_fSubtitleNumberNext[5] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime6" ) )
	{
		m_fSubtitleNumberNext[6] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime7" ) )
	{
		m_fSubtitleNumberNext[7] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime8" ) )
	{
		m_fSubtitleNumberNext[8] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime9" ) )
	{
		m_fSubtitleNumberNext[9] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime10" ) )
	{
		m_fSubtitleNumberNext[10] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime11" ) )
	{
		m_fSubtitleNumberNext[11] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime12" ) )
	{
		m_fSubtitleNumberNext[12] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime13" ) )
	{
		m_fSubtitleNumberNext[13] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime14" ) )
	{
		m_fSubtitleNumberNext[14] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime15" ) )
	{
		m_fSubtitleNumberNext[15] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime16" ) )
	{
		m_fSubtitleNumberNext[16] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime17" ) )
	{
		m_fSubtitleNumberNext[17] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime18" ) )
	{
		m_fSubtitleNumberNext[18] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime19" ) )
	{
		m_fSubtitleNumberNext[19] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime20" ) )
	{
		m_fSubtitleNumberNext[20] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime21" ) )
	{
		m_fSubtitleNumberNext[21] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime22" ) )
	{
		m_fSubtitleNumberNext[22] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime23" ) )
	{
		m_fSubtitleNumberNext[23] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime24" ) )
	{
		m_fSubtitleNumberNext[24] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime25" ) )
	{
		m_fSubtitleNumberNext[25] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime26" ) )
	{
		m_fSubtitleNumberNext[26] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime27" ) )
	{
		m_fSubtitleNumberNext[27] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime28" ) )
	{
		m_fSubtitleNumberNext[28] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime29" ) )
	{
		m_fSubtitleNumberNext[29] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime30" ) )
	{
		m_fSubtitleNumberNext[30] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime31" ) )
	{
		m_fSubtitleNumberNext[31] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "subtitletime32" ) )
	{
		m_fSubtitleNumberNext[32] = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CSubtitleMultiple::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_iCurrentSubtitle = 1;

	// remember when the list started, the delays are measured from here
	m_flStartTime = gpGlobals->time;

	if ( m_fSubtitleNumberNext[m_iCurrentSubtitle] != 0 )
	{
		SetNextThink( m_fSubtitleNumberNext[m_iCurrentSubtitle] );
		SetThink( &CSubtitleMultiple::ChangeSubtitle );
	}
	else
	{
		SetNextThink( 0.1 );
		SetThink( &CSubtitleMultiple::ChangeSubtitle );
	}
}

void CSubtitleMultiple::ChangeSubtitle( void )
{
	int nextsubtitle = m_iCurrentSubtitle + 1;

	CBaseEntity *pSubMain = NULL;
	pSubMain = UTIL_FindEntityByClassname( NULL, "subtitle_main" );

	if ( pSubMain )
	{
		CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

		if ( pPlayer )
		{
			if ( m_iSubtitleNumber[m_iCurrentSubtitle] )
			{
				GetClassPtr( (CSubtitle *)pSubMain->pev )->CurrentLine = m_iSubtitleNumber[m_iCurrentSubtitle];
				pSubMain->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );
			}
			else
				ALERT( at_console, "SUBTITLE CHANGE: Could not change number!\n" );
		}
	}

	if ( nextsubtitle <= 32 && m_iSubtitleNumber[nextsubtitle] && m_fSubtitleNumberNext[nextsubtitle] != 0 )
	{
		float currenttime = gpGlobals->time;
		float timeelapsed = currenttime - m_flStartTime;

		float desiredtime = m_fSubtitleNumberNext[nextsubtitle] - timeelapsed;

		m_iCurrentSubtitle++;

		SetNextThink( desiredtime );
	}
}
