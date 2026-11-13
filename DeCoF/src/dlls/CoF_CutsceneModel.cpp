//=========================================================
// cutscene_model: a model that plays a list of animations
// one after the other when it is used
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "animation.h"

extern float GetSequenceTimeNamed( void *pmodel, char *sequence );

class CCutsceneModel : public CBaseAnimating
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify( void );
	void EXPORT ModelCameraUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData *pkvd );
	void SetSequenceByName( char *szSequence );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	void EXPORT StartNextAnimation( void );

	int m_iSequenceAdjust;

	static	TYPEDESCRIPTION m_SaveData[];

	string_t m_cAnimation[33];
	int m_iNumAnimations;
	int m_iCurrentAnimation;
};

// CoF: the keys are numbered from 1, m_cAnimation[0] is never set
void CCutsceneModel::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "animation1" ) )
	{
		m_cAnimation[1] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation2" ) )
	{
		m_cAnimation[2] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation3" ) )
	{
		m_cAnimation[3] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation4" ) )
	{
		m_cAnimation[4] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation5" ) )
	{
		m_cAnimation[5] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation6" ) )
	{
		m_cAnimation[6] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation7" ) )
	{
		m_cAnimation[7] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation8" ) )
	{
		m_cAnimation[8] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation9" ) )
	{
		m_cAnimation[9] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation10" ) )
	{
		m_cAnimation[10] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation11" ) )
	{
		m_cAnimation[11] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation12" ) )
	{
		m_cAnimation[12] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation13" ) )
	{
		m_cAnimation[13] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		// CoF: m_iNumAnimations is not counted here (original bug)
	}
	else if ( FStrEq( pkvd->szKeyName, "animation14" ) )
	{
		m_cAnimation[14] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation15" ) )
	{
		m_cAnimation[15] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation16" ) )
	{
		m_cAnimation[16] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation17" ) )
	{
		m_cAnimation[17] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation18" ) )
	{
		m_cAnimation[18] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation19" ) )
	{
		m_cAnimation[19] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation20" ) )
	{
		m_cAnimation[20] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation21" ) )
	{
		m_cAnimation[21] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation22" ) )
	{
		m_cAnimation[22] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation23" ) )
	{
		m_cAnimation[23] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation24" ) )
	{
		m_cAnimation[24] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation25" ) )
	{
		m_cAnimation[25] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation26" ) )
	{
		m_cAnimation[26] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation27" ) )
	{
		m_cAnimation[27] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation28" ) )
	{
		m_cAnimation[28] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation29" ) )
	{
		m_cAnimation[29] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation30" ) )
	{
		m_cAnimation[30] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

TYPEDESCRIPTION	CCutsceneModel::m_SaveData[] =
{
	DEFINE_FIELD( CCutsceneModel, m_iNumAnimations, FIELD_INTEGER ),
	DEFINE_ARRAY( CCutsceneModel, m_cAnimation, FIELD_STRING, 33 ),
	DEFINE_FIELD( CCutsceneModel, m_iCurrentAnimation, FIELD_INTEGER ),
	DEFINE_FIELD( CCutsceneModel, m_iSequenceAdjust, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CCutsceneModel, CBaseAnimating );

LINK_ENTITY_TO_CLASS( cutscene_model, CCutsceneModel );

int CCutsceneModel::Classify( void )
{
	return CLASS_NONE;
}

//=========================================================
// SetSequenceByName (from CBaseMonster)
//=========================================================
void CCutsceneModel::SetSequenceByName( char *szSequence )
{
	int	iSequence;

	iSequence = LookupSequence( szSequence );

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		pev->sequence		= iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
	}
	else
	{
		// Not available try to get default anim
		ALERT ( at_aiconsole, "%s has no sequence named:%f\n", STRING(pev->classname), szSequence );
		pev->sequence		= 0;	// Set to the reset anim (if it's there)
	}
}

void CCutsceneModel::Spawn( void )
{
	Precache();

	if ( pev->model )
		SET_MODEL( ENT( pev ), STRING( pev->model ) );
	else
		SET_MODEL( ENT( pev ), "models/null.mdl" );

	UTIL_SetSize( pev, Vector( -16, -16, -36 ), Vector( 16, 16, 36 ) );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;

	if ( pev->spawnflags & 1 )
		pev->effects |= EF_NOINTERP;

	if ( m_iCurrentAnimation < 1 )
		m_iCurrentAnimation = 1;

	SetUse( &CCutsceneModel::ModelCameraUse );
}

void CCutsceneModel::Precache( void )
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING( pev->model ) );
	else
		PRECACHE_MODEL( "models/null.mdl" );
}

void CCutsceneModel::ModelCameraUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	SetNextThink( 0.05 );
	SetThink( &CCutsceneModel::StartNextAnimation );
}

void CCutsceneModel::StartNextAnimation( void )
{
	// undo the height change of the previous animation
	if ( m_iSequenceAdjust > 0 )
	{
		pev->origin.z += m_iSequenceAdjust;
		m_iSequenceAdjust = 0;
	}

	if ( !strcmp( STRING( m_cAnimation[m_iCurrentAnimation] ), "climb_up_loop" ) )
		m_iSequenceAdjust = 48;

	SetSequenceByName( (char *)STRING( m_cAnimation[m_iCurrentAnimation] ) );

	if ( m_iCurrentAnimation < m_iNumAnimations )
	{
		float delay = GetSequenceTimeNamed( GET_MODEL_PTR( ENT( pev ) ), (char *)STRING( m_cAnimation[m_iCurrentAnimation] ) );

		if ( delay != 0 )
		{
			SetNextThink( delay );
		}
		else
			ALERT( at_console, "Unable to find delay\n" );
	}

	m_iCurrentAnimation++;
}
