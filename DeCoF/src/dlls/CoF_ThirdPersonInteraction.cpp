//=========================================================
// third_person_interaction: plays a kill animation between
// the player and a monster (the chainsaw boss) in third
// person
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "animation.h"

extern int gmsgCameraZoom;


class CThirdPersonInteraction : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void TriggerAction( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void SetSequenceByName( char *szSequence );
	void EXPORT NahThink( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	bool m_bSpawnChainsawBlood;
	EHANDLE m_pPlayer;
	EHANDLE m_pMonster;
};

TYPEDESCRIPTION	CThirdPersonInteraction::m_SaveData[] =
{
	DEFINE_FIELD( CThirdPersonInteraction, m_pPlayer, FIELD_EHANDLE ),
	DEFINE_FIELD( CThirdPersonInteraction, m_pMonster, FIELD_EHANDLE ),
	DEFINE_FIELD( CThirdPersonInteraction, m_bSpawnChainsawBlood, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CThirdPersonInteraction, CBaseMonster );
LINK_ENTITY_TO_CLASS( third_person_interaction, CThirdPersonInteraction );

void CThirdPersonInteraction::Spawn( void )
{
	Precache();

	if ( pev->model )
		SET_MODEL( ENT( pev ), STRING( pev->model ) );

	// the model plays both of them
	if ( m_pPlayer )
		m_pPlayer->pev->effects |= EF_NODRAW;

	if ( m_pMonster )
		m_pMonster->pev->effects |= EF_NODRAW;

	if ( m_pPlayer && m_pMonster )
	{
		Vector vecMidPoint = m_pPlayer->pev->origin + ( m_pMonster->pev->origin - m_pPlayer->pev->origin ) * 0.5;
		UTIL_SetOrigin( this, vecMidPoint );

		pev->origin.z += 1;
		DROP_TO_FLOOR( ENT( pev ) );

		pev->angles = m_pMonster->pev->angles;

		pev->angles.y += 180;

		if ( pev->angles.y > 360 )
			pev->angles.y -= 360;

		if ( pev->angles.y < 0 )
			pev->angles.y += 360;

		if ( m_pPlayer->IsPlayer() )
		{
			GetClassPtr( (CBasePlayer *)m_pPlayer->pev )->ToThirdPerson();

			m_pPlayer->pev->flags |= FL_NOTARGET;

			m_pPlayer->pev->flags |= FL_FROZEN;
			GetClassPtr( (CBasePlayer *)m_pPlayer->pev )->RemoveAllItems( FALSE );
		}
	}

	if ( pev->message )
		SetSequenceByName( (char *)STRING( pev->message ) );

	SetNextThink( 0.1 );
	SetThink( &CThirdPersonInteraction::NahThink );
}

void CThirdPersonInteraction::NahThink( void )
{
	CBaseMonster::MonsterThink();

	if ( m_bSpawnChainsawBlood )
	{
		Vector vecSrc, vecAng;

		GetAttachment( 1, vecSrc, vecAng );
		UTIL_BloodDrips( vecSrc, g_vecZero, BLOOD_COLOR_RED, 80, pev->velocity );

		GetAttachment( 2, vecSrc, vecAng );
		UTIL_BloodDrips( vecSrc, g_vecZero, BLOOD_COLOR_RED, 80, pev->velocity );
	}

	SetNextThink( 0.1 );
}

void CThirdPersonInteraction::Precache( void )
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING( pev->model ) );
}

void CThirdPersonInteraction::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch ( pEvent->event )
	{
	case 1:
		// kill the player
		if ( m_pPlayer )
		{
			GetClassPtr( (CBasePlayer *)m_pPlayer->pev )->m_bGibbed = true;

			SetBits( GetClassPtr( (CBasePlayer *)m_pPlayer->pev )->m_iDeathBits, 1 );
			SetBits( GetClassPtr( (CBasePlayer *)m_pPlayer->pev )->m_iDeathBits, 2 );
			SetBits( GetClassPtr( (CBasePlayer *)m_pPlayer->pev )->m_iDeathBits, 4 );
			SetBits( GetClassPtr( (CBasePlayer *)m_pPlayer->pev )->m_iDeathBits, 32 );

			m_pPlayer->pev->health = 0;
			GetClassPtr( (CBasePlayer *)m_pPlayer->pev )->Killed( pev, GIB_NEVER );

			if ( m_bSpawnChainsawBlood )
			{
				UTIL_ScreenShake( m_pPlayer->pev->origin, 5, 10, 0.1, 10 );

				m_bSpawnChainsawBlood = false;
			}
		}
		break;

	case 2:
		if ( m_pPlayer )
		{
			EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, "boss/sawer/chainsawed_player.wav", 1, 0.8, 0, 100 );
			UTIL_ScreenShake( m_pPlayer->pev->origin, 5, 10, 2, 10 );

			MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, m_pPlayer->pev );
				WRITE_COORD( 70 );
				WRITE_BYTE( 0 );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			m_pPlayer->pev->vuser2.x = 75;
		}
		break;

	case 3:
		m_bSpawnChainsawBlood = true;
		pev->body = 1;
		break;

	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

//=========================================================
// SetSequenceByName (from CBaseMonster)
//=========================================================
void CThirdPersonInteraction::SetSequenceByName( char *szSequence )
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
