//=========================================================
// camera: sets the player's view to itself and starts its
// camera_model, camera_model: a model that plays a list of
// animations one after the other
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "animation.h"

#undef SET_VIEW
extern void SET_VIEW( edict_t *pClient, edict_t *pViewent );

extern float GetSequenceTimeNamed( void *pmodel, char *sequence );

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );

class CAnimatedCamera : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify( void );
	void EXPORT CameraUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( camera, CAnimatedCamera );

int CAnimatedCamera::Classify( void )
{
	return CLASS_NONE;
}

void CAnimatedCamera::Spawn( void )
{
	Precache();

	SET_MODEL( ENT( pev ), "models/null.mdl" );

	UTIL_SetSize( pev, Vector( -16, -16, -36 ), Vector( 16, 16, 36 ) );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;
	m_bloodColor = DONT_BLEED;
	m_MonsterState = MONSTERSTATE_NONE;

	SetUse( &CAnimatedCamera::CameraUse );
}

void CAnimatedCamera::Precache( void )
{
	PRECACHE_MODEL( "models/null.mdl" );
}

void CAnimatedCamera::CameraUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( pActivator->IsPlayer() )
	{
		if ( !pev->message )
		{
			UTIL_ProMessage( pActivator, UTIL_VarArgs( "Camera %s has no model entity!", STRING( pev->targetname ) ), 2, false );
			return;
		}

		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

		if ( pPlayer )
		{
			pPlayer->m_hCurrentCamera = this;
			SET_VIEW( pPlayer->edict(), edict() );

			CBaseEntity *pTarget = NULL;
			pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->message ) );

			if ( pTarget )
				pTarget->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );
		}
	}
}


class CAnimatedCameraModel : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify( void );
	void EXPORT ModelCameraUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData *pkvd );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	void EXPORT StartNextAnimation( void );

	static	TYPEDESCRIPTION m_SaveData[];

	string_t m_cAnimation[33];
	int m_iNumAnimations;
	int m_iCurrentAnimation;
};

// CoF: the keys are numbered from 1, m_cAnimation[0] is never set
void CAnimatedCameraModel::KeyValue( KeyValueData *pkvd )
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
	else
		CBaseEntity::KeyValue( pkvd );
}

TYPEDESCRIPTION	CAnimatedCameraModel::m_SaveData[] =
{
	DEFINE_FIELD( CAnimatedCameraModel, m_iNumAnimations, FIELD_INTEGER ),
	DEFINE_ARRAY( CAnimatedCameraModel, m_cAnimation, FIELD_STRING, 33 ),
	DEFINE_FIELD( CAnimatedCameraModel, m_iCurrentAnimation, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CAnimatedCameraModel, CBaseEntity );

LINK_ENTITY_TO_CLASS( camera_model, CAnimatedCameraModel );

int CAnimatedCameraModel::Classify( void )
{
	return CLASS_NONE;
}

void CAnimatedCameraModel::Spawn( void )
{
	Precache();

	if ( pev->model )
		SET_MODEL( ENT( pev ), STRING( pev->model ) );
	else
		SET_MODEL( ENT( pev ), "models/null.mdl" );

	UTIL_SetSize( pev, Vector( -16, -16, -36 ), Vector( 16, 16, 36 ) );

	pev->solid = SOLID_NOT;

	m_bloodColor = DONT_BLEED;
	m_MonsterState = MONSTERSTATE_NONE;

	if ( m_iCurrentAnimation < 1 )
		m_iCurrentAnimation = 1;

	SetUse( &CAnimatedCameraModel::ModelCameraUse );
}

void CAnimatedCameraModel::Precache( void )
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING( pev->model ) );
	else
		PRECACHE_MODEL( "models/null.mdl" );
}

void CAnimatedCameraModel::ModelCameraUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	SetNextThink( 0.05 );
	SetThink( &CAnimatedCameraModel::StartNextAnimation );
}

void CAnimatedCameraModel::StartNextAnimation( void )
{
	SetSequenceByName( (char *)STRING( m_cAnimation[m_iCurrentAnimation] ) );

	if ( m_iCurrentAnimation <= m_iNumAnimations )
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
