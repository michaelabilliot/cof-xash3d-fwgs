//=========================================================
// cof_strangle: the strangle cutscene, the player mashes
// the attack buttons to hurt the boss until it dies
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "animation.h"

extern int gmsgHealthCap;
extern int gmsgAttachCam;

extern float GetSequenceTimeNamed( void *pmodel, char *sequence );

class CCoFStrangle : public CPointEntity
{
	void Spawn( void );
	void Precache( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT StrangleThink( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	CBasePlayer *m_pPlayer;
	EHANDLE m_pBossEnt;
};

TYPEDESCRIPTION	CCoFStrangle::m_SaveData[] =
{
	DEFINE_FIELD( CCoFStrangle, m_pPlayer, FIELD_CLASSPTR ),
	DEFINE_FIELD( CCoFStrangle, m_pBossEnt, FIELD_EHANDLE ),
};

IMPLEMENT_SAVERESTORE( CCoFStrangle, CPointEntity );

// pev->iuser1 is the state of the cutscene (1 start, 2 health cap, 3 strangling)
void CCoFStrangle::StrangleThink( void )
{
	if ( gpGlobals->time > pev->teleport_time )
	{
		if ( m_pPlayer && m_pPlayer->pDeathScene && pev->iuser1 == 1 )
		{
			((CBaseMonster *)m_pPlayer->pDeathScene)->SetSequenceByName( "strangle_start2" );
			pev->teleport_time = GetSequenceTimeNamed( GET_MODEL_PTR( ENT( m_pPlayer->pDeathScene->pev ) ), "strangle_start2" ) + gpGlobals->time;

			pev->iuser1 = 2;
			SetNextThink( 0 );
			return;
		}

		if ( m_pPlayer && pev->iuser1 == 2 )
		{
			m_pPlayer->m_iHealthCap = 0;

			MESSAGE_BEGIN( MSG_ONE, gmsgHealthCap, NULL, m_pPlayer->pev );
				WRITE_BYTE( m_pPlayer->m_iHealthCap );
			MESSAGE_END();

			m_pPlayer->m_iMorphineRecover = 100;
			pev->iuser1 = 3;
		}

		if ( pev->iuser1 == 3 && m_pPlayer && m_pPlayer->pDeathScene )
		{
			if ( gpGlobals->time > pev->teleport_time )
				((CBaseMonster *)m_pPlayer->pDeathScene)->SetSequenceByName( "strangle_idle" );

			// CoF: tests the buttons of the cutscene model, not of the player (original bug)
			if ( ( m_pPlayer->pDeathScene->pev->button & IN_ATTACK ) || ( m_pPlayer->pDeathScene->pev->button & IN_ATTACK2 ) )
			{
				switch ( RANDOM_LONG( 0, 2 ) )
				{
				case 0:
					EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_AUTO, "cutscene/strangle_strangle1.wav", 1, ATTN_NONE, 0, 100 );
					((CBaseMonster *)m_pPlayer->pDeathScene)->SetSequenceByName( "strangle_strangle1" );
					pev->teleport_time = GetSequenceTimeNamed( GET_MODEL_PTR( ENT( m_pPlayer->pDeathScene->pev ) ), "strangle_strangle1" ) + gpGlobals->time;
					break;

				case 1:
					EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_AUTO, "cutscene/strangle_strangle2.wav", 1, ATTN_NONE, 0, 100 );
					((CBaseMonster *)m_pPlayer->pDeathScene)->SetSequenceByName( "strangle_strangle2" );
					pev->teleport_time = GetSequenceTimeNamed( GET_MODEL_PTR( ENT( m_pPlayer->pDeathScene->pev ) ), "strangle_strangle2" ) + gpGlobals->time;
					break;

				case 2:
					EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_AUTO, "cutscene/strangle_strangle3.wav", 1, ATTN_NONE, 0, 100 );
					((CBaseMonster *)m_pPlayer->pDeathScene)->SetSequenceByName( "strangle_strangle3" );
					pev->teleport_time = GetSequenceTimeNamed( GET_MODEL_PTR( ENT( m_pPlayer->pDeathScene->pev ) ), "strangle_strangle3" ) + gpGlobals->time;
					break;
				}

				if ( m_pBossEnt )
				{
					m_pBossEnt->TakeDamage( m_pPlayer->pev, m_pPlayer->pev, 25, 0 );
					m_pPlayer->pev->health -= 25;

					if ( m_pPlayer->pev->health <= 1 && pev->target )
					{
						((CBaseMonster *)m_pPlayer->pDeathScene)->SetSequenceByName( "strangle_death" );

						m_pPlayer->pev->health = 1;

						EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_AUTO, "cutscene/strangle_strangle4.wav", 1, ATTN_NONE, 0, 100 );

						FireTargets( STRING( pev->target ), m_pPlayer, m_pPlayer, USE_TOGGLE, 0 );
						SetThink( NULL );
						UTIL_Remove( this );
						return;
					}
				}
			}
		}
	}

	SetNextThink( 0 );
}

void CCoFStrangle::Precache( void )
{
	PRECACHE_MODEL( "models/cutscene/strangle_simon.mdl" );
	PRECACHE_SOUND( "cutscene/stranglesounds.wav" );
	PRECACHE_SOUND( "cutscene/strangle_strangle1.wav" );
	PRECACHE_SOUND( "cutscene/strangle_strangle2.wav" );
	PRECACHE_SOUND( "cutscene/strangle_strangle3.wav" );
	PRECACHE_SOUND( "cutscene/strangle_strangle4.wav" );
}

void CCoFStrangle::Spawn( void )
{
	Precache();
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFStrangle::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );
	CBasePlayer *pPlayer = NULL;

	if ( player )
		pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

	// the boss to hurt
	if ( !m_pBossEnt && pev->message )
	{
		CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->message ) );

		if ( pTarget )
			m_pBossEnt = pTarget;
	}

	if ( pPlayer )
	{
		pPlayer->m_bChoking = true;

		if ( pPlayer->m_bLowHealth )
		{
			pPlayer->m_bLowHealth = false;
			g_engfuncs.pfnSetPhysicsKeyValue( pPlayer->edict(), "low", "0" );
		}

		m_pPlayer = pPlayer;
		pPlayer->EnableControl( FALSE );
		pPlayer->pDeathScene = (CBaseAnimating *)CBaseEntity::Create( "camera_model", pev->origin, pev->angles, pPlayer->edict() );

		if ( pPlayer->pDeathScene )
		{
			UTIL_SetOrigin( pPlayer->pDeathScene, Vector( pev->origin.x, pev->origin.y, pev->origin.z ) );
			SET_MODEL( ENT( pPlayer->pDeathScene->pev ), "models/cutscene/strangle_simon.mdl" );

			pPlayer->m_pAttachedCamera = pPlayer->pDeathScene;

			EMIT_SOUND_DYN( pPlayer->edict(), CHAN_AUTO, "cutscene/stranglesounds.wav", 1, ATTN_NONE, 0, 100 );

			((CBaseMonster *)pPlayer->pDeathScene)->SetSequenceByName( "strangle_start1" );

			MESSAGE_BEGIN( MSG_ONE, gmsgAttachCam, NULL, pPlayer->pev );
				WRITE_SHORT( ENTINDEX( pPlayer->m_pAttachedCamera->edict() ) );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			pev->teleport_time = GetSequenceTimeNamed( GET_MODEL_PTR( ENT( pPlayer->pDeathScene->pev ) ), "strangle_start1" ) + gpGlobals->time;
			pev->iuser1 = 1;

			pev->owner = pPlayer->edict();
		}

		SetThink( &CCoFStrangle::StrangleThink );
		SetNextThink( 0.1 );
	}
}

LINK_ENTITY_TO_CLASS( cof_strangle, CCoFStrangle );
