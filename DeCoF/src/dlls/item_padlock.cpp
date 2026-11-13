//=========================================================
// item_padlock - a combination padlock the player dials
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "animation.h"
#include "cdll_dll.h"
#include "shake.h"

#undef SET_VIEW
extern void SET_VIEW( edict_t *pClient, edict_t *pViewent );

extern int gmsgPadlock;

// The 1.55 debug build declares this class in effects.h.
class CItemPadlock : public CBaseAnimating
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	void Rotate( int number );
	void EXPORT Check( void );
	void EXPORT Finished( void );
	virtual STATE GetState( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return CBaseAnimating :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void SetSequenceByName( char *szSequence );
	void Cancel( void );
};

LINK_ENTITY_TO_CLASS( item_padlock, CItemPadlock );

void CItemPadlock::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), "models/items/padlock.mdl" );
	UTIL_SetOrigin( this, pev->origin );

	SetBoneController( 0, 0 );
	SetBoneController( 1, 0 );
	SetBoneController( 2, 0 );
	SetBoneController( 3, 0 );

	// dial positions
	pev->iuser1 = pev->iuser2 = pev->iuser3 = pev->iuser4 = 0;

	SetSequenceByName( "idle" );
}

void CItemPadlock::Finished( void )
{
	CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

	if ( pPlayer )
	{
		FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
		SET_VIEW( pPlayer->edict(), pPlayer->edict() );
		UTIL_ScreenFade( pPlayer, Vector( 0, 0, 0 ), 0.5, 0.1, 255, 0 );

		GetClassPtr( (CBasePlayer *)pPlayer->pev )->EnableControl( TRUE );
		GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_iHideHUD &= ~HIDEHUD_HEALTH;
		GetClassPtr( (CBasePlayer *)pPlayer->pev )->pev->flags &= ~FL_NOTARGET;

		MESSAGE_BEGIN( MSG_ONE, gmsgPadlock, NULL, pPlayer->pev );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}
}

int GetRotationForNumber( int number )
{
	switch ( number )
	{
	case 1:
		return 0;

	case 2:
		return 320;

	case 3:
		return 280;

	case 4:
		return 240;

	case 5:
		return 200;

	case 6:
		return 160;

	case 7:
		return 120;

	case 8:
		return 80;

	case 9:
		return 40;
	}

	return 0;
}

void CItemPadlock::Cancel( void )
{
	CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

	if ( pPlayer )
	{
		GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_pPadlock = NULL;
		SET_VIEW( pPlayer->edict(), pPlayer->edict() );

		GetClassPtr( (CBasePlayer *)pPlayer->pev )->EnableControl( TRUE );
		GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_iHideHUD &= ~HIDEHUD_HEALTH;
		GetClassPtr( (CBasePlayer *)pPlayer->pev )->pev->flags &= ~FL_NOTARGET;

		MESSAGE_BEGIN( MSG_ONE, gmsgPadlock, NULL, pPlayer->pev );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}
}

void CItemPadlock::Check( void )
{
	int number1 = GetRotationForNumber( pev->fuser1 );
	int number2 = GetRotationForNumber( pev->fuser2 );
	int number3 = GetRotationForNumber( pev->fuser3 );
	int number4 = GetRotationForNumber( pev->fuser4 );

	if ( pev->iuser1 == number1 && pev->iuser2 == number2 && pev->iuser3 == number3 && pev->iuser4 == number4 )
	{
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "items/padl_unlock.wav", 1, ATTN_NORM );
		SetSequenceByName( "open" );

		pev->body = 1;

		CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

		if ( pPlayer )
		{
			GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_pPadlock = NULL;
			UTIL_ScreenFade( pPlayer, Vector( 0, 0, 0 ), 2, 10, 255, FFADE_OUT | FFADE_STAYOUT );
		}

		SetNextThink( 2 );
		SetThink( &CItemPadlock::Finished );
	}
}

void CItemPadlock::Rotate( int number )
{
	switch ( number )
	{
	case 1:
		pev->iuser1 += 40;
		if ( pev->iuser1 >= 360 )
			pev->iuser1 = 0;

		SetBoneController( 0, pev->iuser1 );
		break;

	case 2:
		pev->iuser2 += 40;
		if ( pev->iuser2 >= 360 )
			pev->iuser2 = 0;

		SetBoneController( 1, pev->iuser2 );
		break;

	case 3:
		pev->iuser3 += 40;
		if ( pev->iuser3 >= 360 )
			pev->iuser3 = 0;

		SetBoneController( 2, pev->iuser3 );
		break;

	case 4:
		pev->iuser4 += 40;
		if ( pev->iuser4 >= 360 )
			pev->iuser4 = 0;

		SetBoneController( 3, pev->iuser4 );
		break;
	}

	Check();

	EMIT_SOUND( ENT( pev ), CHAN_BODY, "items/padl_turn.wav", 1, ATTN_NORM );
}

void CItemPadlock::Precache( void )
{
	PRECACHE_MODEL( "models/items/padlock.mdl" );
	PRECACHE_SOUND( "items/padl_turn.wav" );
	PRECACHE_SOUND( "items/padl_unlock.wav" );
}

STATE CItemPadlock::GetState( void )
{
	if ( pev->body == 0 )
		return STATE_OFF;
	else
		return STATE_ON;
}

void CItemPadlock::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	if ( GetState() == STATE_ON )
		return;

	if ( !pev->message )
		return;

	CBaseEntity *pEntity = UTIL_FindEntityByTargetname( NULL, STRING( pev->message ) );
	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( !pPlayer )
		return;

	if ( !pEntity )
		return;

	pPlayer->m_pPadlock = this;

	pPlayer->EnableControl( FALSE );
	pPlayer->m_iHideHUD |= HIDEHUD_HEALTH;
	pPlayer->pev->flags |= FL_NOTARGET;

	SetBoneController( 0, 0 );
	SetBoneController( 1, 0 );
	SetBoneController( 2, 0 );
	SetBoneController( 3, 0 );

	pev->iuser1 = pev->iuser2 = pev->iuser3 = pev->iuser4 = 0;

	// the combination of this level
	pev->fuser1 = pPlayer->m_iPadlockNumber[1];
	pev->fuser2 = pPlayer->m_iPadlockNumber[2];
	pev->fuser3 = pPlayer->m_iPadlockNumber[3];
	pev->fuser4 = pPlayer->m_iPadlockNumber[4];

	SET_VIEW( pPlayer->edict(), pEntity->edict() );

	MESSAGE_BEGIN( MSG_ONE, gmsgPadlock, NULL, pPlayer->pev );
		WRITE_BYTE( 1 );
	MESSAGE_END();
}

void CItemPadlock::SetSequenceByName( char *szSequence )
{
	int	iSequence;

	iSequence = LookupSequence ( szSequence );

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		pev->sequence		= iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
		//SetYawSpeed();
	}
	else
	{
		// Not available try to get default anim
		ALERT ( at_aiconsole, "%s has no sequence named:%f\n", STRING(pev->classname), szSequence );
		pev->sequence		= 0;	// Set to the reset anim (if it's there)
	}
}
