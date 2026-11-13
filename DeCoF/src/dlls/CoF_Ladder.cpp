/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//=========================================================
// cof_ladder_manager - climbing a ladder with a camera model
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );
extern float GetSequenceTimeNamed( void *pmodel, char *sequence );

extern int gmsgAttachCam;
extern int gmsgCameraZoom;

bool wasqueuedtoreduceorigin;

class CCoFLadderManager : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT LadderThink( void );
	void StartNextAnimation( void );
	void StartPreviousAnimation( void );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	bool m_bFinishedOnLadder;
	bool m_bEnableControl;
	float m_flControlEnableTime;
	bool m_bDescending;
	string_t m_sPreviousAnimation;
	string_t m_sCustomOffAnimation;
	string_t m_sCustomOnAnimation;
	string_t m_sCustomOffDescendAnimation;
	string_t m_sCustomOnDescendAnimation;
	EHANDLE m_eTopEnt;
	EHANDLE m_eBottomEnt;
	string_t m_sTopEnt;
	string_t m_sBottomEnt;
	CBasePlayer *m_pPlayer;
	int m_iDescendOffset;
	float m_flLowerTime;
	bool m_bDoLower;
	bool m_bOriginReductionQueuedPrevious;
	bool m_bOriginReductionQueuedNext;
	static TYPEDESCRIPTION m_SaveData[];
	int m_cAnimation[33];
	bool m_bGotOnButNotDescended;
	int m_iNumAnimations;
	int m_iCurrentAnimation;
};

TYPEDESCRIPTION CCoFLadderManager::m_SaveData[] =
{
	DEFINE_FIELD( CCoFLadderManager, m_iNumAnimations, FIELD_INTEGER ),
	DEFINE_ARRAY( CCoFLadderManager, m_cAnimation, FIELD_STRING, 33 ),
	DEFINE_FIELD( CCoFLadderManager, m_iCurrentAnimation, FIELD_INTEGER ),
	DEFINE_FIELD( CCoFLadderManager, m_pPlayer, FIELD_CLASSPTR ),
	DEFINE_FIELD( CCoFLadderManager, m_bEnableControl, FIELD_BOOLEAN ),
	DEFINE_FIELD( CCoFLadderManager, m_bDescending, FIELD_BOOLEAN ),
	DEFINE_FIELD( CCoFLadderManager, m_flControlEnableTime, FIELD_TIME ),
	DEFINE_FIELD( CCoFLadderManager, m_sPreviousAnimation, FIELD_STRING ),
	DEFINE_FIELD( CCoFLadderManager, m_sCustomOffAnimation, FIELD_STRING ),
	DEFINE_FIELD( CCoFLadderManager, m_sCustomOnAnimation, FIELD_STRING ),
	DEFINE_FIELD( CCoFLadderManager, m_sCustomOffDescendAnimation, FIELD_STRING ),
	DEFINE_FIELD( CCoFLadderManager, m_sCustomOnDescendAnimation, FIELD_STRING ),
	DEFINE_FIELD( CCoFLadderManager, m_iDescendOffset, FIELD_INTEGER ),
	DEFINE_FIELD( CCoFLadderManager, m_flLowerTime, FIELD_TIME ),
	DEFINE_FIELD( CCoFLadderManager, m_bDoLower, FIELD_BOOLEAN ),
	DEFINE_FIELD( CCoFLadderManager, m_bOriginReductionQueuedPrevious, FIELD_BOOLEAN ),
	DEFINE_FIELD( CCoFLadderManager, m_bOriginReductionQueuedNext, FIELD_BOOLEAN ),
	DEFINE_FIELD( CCoFLadderManager, m_bGotOnButNotDescended, FIELD_BOOLEAN ),
	DEFINE_FIELD( CCoFLadderManager, m_bFinishedOnLadder, FIELD_BOOLEAN ),
	DEFINE_FIELD( CCoFLadderManager, m_eTopEnt, FIELD_EHANDLE ),
	DEFINE_FIELD( CCoFLadderManager, m_eBottomEnt, FIELD_EHANDLE ),
	DEFINE_FIELD( CCoFLadderManager, m_sTopEnt, FIELD_STRING ),
	DEFINE_FIELD( CCoFLadderManager, m_sBottomEnt, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CCoFLadderManager, CPointEntity );

void CCoFLadderManager::KeyValue( KeyValueData *pkvd )
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
	else if ( FStrEq( pkvd->szKeyName, "customoff" ) )
	{
		m_sCustomOffAnimation = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "customon" ) )
	{
		m_sCustomOnAnimation = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "customoffd" ) )
	{
		m_sCustomOffDescendAnimation = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "customond" ) )
	{
		m_sCustomOnDescendAnimation = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "descendoffset" ) )
	{
		m_iDescendOffset = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "topent" ) )
	{
		m_sTopEnt = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "bottoment" ) )
	{
		m_sBottomEnt = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CCoFLadderManager::LadderThink( void )
{
	if ( m_bDoLower && m_flLowerTime < gpGlobals->time && m_pPlayer && m_pPlayer->pDeathScene )
	{
		m_bDoLower = false;
		m_pPlayer->pDeathScene->pev->origin.z -= 48;
	}

	if ( m_flControlEnableTime < gpGlobals->time && m_bEnableControl && m_pPlayer )
	{
		m_pPlayer->EnableControl( TRUE );

		if ( m_pPlayer->pDeathScene )
		{
			UTIL_Remove( m_pPlayer->pDeathScene );
			m_pPlayer->pDeathScene = NULL;
			m_pPlayer->m_pAttachedCamera = NULL;
		}

		MESSAGE_BEGIN( MSG_ONE, gmsgAttachCam, NULL, m_pPlayer->pev );
			WRITE_SHORT( 0 );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		m_pPlayer->pev->flags &= ~FL_GODMODE;
		m_pPlayer->pev->flags &= ~FL_NOTARGET;

		CLIENT_COMMAND( m_pPlayer->edict(), "finishladder\n" );

		if ( m_bDescending )
		{
			if ( !m_eBottomEnt )
			{
				CBaseEntity *pTemp = UTIL_FindEntityByTargetname( NULL, STRING( m_sBottomEnt ) );

				if ( pTemp )
					m_eBottomEnt = pTemp;
			}

			if ( m_eBottomEnt )
			{
				Vector tmp = m_eBottomEnt->pev->origin;
				tmp.z += 36;
				tmp.z += 1;

				UTIL_SetOrigin( m_pPlayer, tmp );

				m_pPlayer->pev->angles = m_eBottomEnt->pev->angles;
				m_pPlayer->pev->velocity = m_pPlayer->pev->basevelocity = g_vecZero;

				if ( m_pPlayer->IsPlayer() )
				{
					m_pPlayer->pev->v_angle.y = m_eBottomEnt->pev->angles.y;
					m_pPlayer->pev->fixangle = 1;
				}

				m_pPlayer->pev->flags &= ~FL_ONGROUND;
				m_pPlayer->pev->fixangle = 1;
			}
		}
		else
		{
			if ( !m_eTopEnt )
			{
				CBaseEntity *pTemp = UTIL_FindEntityByTargetname( NULL, STRING( m_sTopEnt ) );

				if ( pTemp )
					m_eTopEnt = pTemp;
			}

			if ( m_eTopEnt )
			{
				Vector tmp = m_eTopEnt->pev->origin;
				tmp.z += 36;
				tmp.z += 1;

				UTIL_SetOrigin( m_pPlayer, tmp );

				m_pPlayer->pev->angles = m_eTopEnt->pev->angles;
				m_pPlayer->pev->velocity = m_pPlayer->pev->basevelocity = g_vecZero;

				if ( m_pPlayer->IsPlayer() )
				{
					m_pPlayer->pev->v_angle.y = m_eTopEnt->pev->angles.y;
					m_pPlayer->pev->fixangle = 1;
				}

				m_pPlayer->pev->flags &= ~FL_ONGROUND;
				m_pPlayer->pev->fixangle = 1;
			}
		}

		MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, m_pPlayer->pev );
			WRITE_COORD( 50 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		m_pPlayer->pev->vuser2.x = 90;

		if ( pev->target )
			FireTargets( STRING( pev->target ), m_pPlayer, m_pPlayer, USE_TOGGLE, 0 );

		m_bEnableControl = false;
		return;
	}

	if ( pev->teleport_time < gpGlobals->time && !m_bFinishedOnLadder )
	{
		if ( m_pPlayer )
		{
			if ( m_pPlayer->pDeathScene && ( m_pPlayer->pDeathScene->pev->button & IN_FORWARD ) )
			{
				m_bDescending = false;
				StartNextAnimation();
			}

			if ( m_pPlayer->pDeathScene && ( m_pPlayer->pDeathScene->pev->button & IN_BACK ) )
			{
				m_bDescending = true;
				StartPreviousAnimation();
			}
		}
	}

	SetNextThink( 0 );
}

void CCoFLadderManager::Precache( void )
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING( pev->model ) );
}

void CCoFLadderManager::Spawn( void )
{
	Precache();
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;

	if ( m_iCurrentAnimation < 1 )
		m_iCurrentAnimation = 1;
}

void CCoFLadderManager::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pev->model )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{
		if ( pPlayer->m_pActiveItem )
		{
			if ( FClassnameIs( pPlayer->m_pActiveItem->edict(), "weapon_flare" ) )
			{
				CBasePlayerWeapon *pFlare = pPlayer->GetPlayerWeapon( 13 );

				if ( pFlare && pFlare->pev->iuser1 )
				{
					UTIL_ProMessage( pPlayer, "I'll need to drop this flare before I can climb", 5, false );
					return;
				}
			}

			if ( FClassnameIs( pPlayer->m_pActiveItem->edict(), "weapon_dualweapon" ) && ( pPlayer->m_iDualWeaponLeftID == 13 || pPlayer->m_iDualWeaponRightID == 13 ) )
			{
				CBasePlayerWeapon *pFlare = pPlayer->GetPlayerWeapon( 13 );

				if ( pFlare && pFlare->pev->iuser1 )
				{
					UTIL_ProMessage( pPlayer, "I'll need to drop this flare before I can climb", 5, false );
					return;
				}
			}
		}
	}

	if ( value != 0 )
		m_bDescending = true;
	else
		m_bDescending = false;

	if ( pPlayer )
	{
		m_pPlayer = pPlayer;
		pPlayer->EnableControl( FALSE );
		pPlayer->pDeathScene = (CBaseAnimating *)CBaseEntity::Create( "camera_model", pev->origin, pev->angles, pPlayer->edict() );

		if ( m_bDescending )
			m_iCurrentAnimation = m_iNumAnimations;
		else
			m_iCurrentAnimation = 1;

		if ( pPlayer->pDeathScene )
		{
			UTIL_SetOrigin( pPlayer->pDeathScene, Vector( pev->origin.x, pev->origin.y, pev->origin.z ) );
			SET_MODEL( ENT( pPlayer->pDeathScene->pev ), STRING( pev->model ) );

			if ( m_bDescending )
				pPlayer->pDeathScene->pev->origin.z += m_iDescendOffset;

			pPlayer->m_pAttachedCamera = pPlayer->pDeathScene;
			pPlayer->m_pClientAttachedCamera = pPlayer->m_pAttachedCamera;

			MESSAGE_BEGIN( MSG_ONE, gmsgAttachCam, NULL, pPlayer->pev );
				WRITE_SHORT( ENTINDEX( pPlayer->m_pAttachedCamera->edict() ) );
				WRITE_BYTE( 1 );
			MESSAGE_END();

			m_pPlayer->pev->flags |= FL_NOTARGET;
			m_pPlayer->pev->flags |= FL_GODMODE;

			pev->owner = pPlayer->edict();
		}

		SetThink( &CCoFLadderManager::LadderThink );
		SetNextThink( 0.1 );

		StartNextAnimation();
	}
}

void CCoFLadderManager::StartNextAnimation( void )
{
	if ( !pev->owner )
		return;

	CBaseEntity *pEntity = CBaseEntity::Instance( pev->owner );

	if ( !pEntity )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pEntity->pev );

	if ( !pPlayer )
		return;

	if ( !pPlayer->pDeathScene )
		return;

	if ( m_bOriginReductionQueuedNext )
	{
		m_bOriginReductionQueuedNext = false;
		wasqueuedtoreduceorigin = true;
		pPlayer->pDeathScene->pev->origin.z -= 48;
	}

	if ( m_iCurrentAnimation > 1 && !m_bDescending && !strstr( STRING( m_sPreviousAnimation ), "climb_loop_down" ) )
	{
		if ( wasqueuedtoreduceorigin )
			wasqueuedtoreduceorigin = false;
		else if ( !m_bGotOnButNotDescended )
			pPlayer->pDeathScene->pev->origin.z += 48;
	}

	int animationtoplay = m_cAnimation[m_iCurrentAnimation];

	if ( m_bDescending )
	{
		if ( strstr( STRING( m_cAnimation[m_iCurrentAnimation] ), "climb_end" ) && strstr( STRING( m_cAnimation[m_iCurrentAnimation] ), "up" ) && m_sCustomOnAnimation && !m_bDescending )
			animationtoplay = m_sCustomOnAnimation;

		if ( strstr( STRING( m_cAnimation[m_iCurrentAnimation] ), "climb_end" ) && strstr( STRING( m_cAnimation[m_iCurrentAnimation] ), "up" ) && m_sCustomOnDescendAnimation && m_bDescending == true )
			animationtoplay = m_sCustomOnDescendAnimation;
	}
	else
	{
		if ( strstr( STRING( m_cAnimation[m_iCurrentAnimation] ), "climb_end" ) && strstr( STRING( m_cAnimation[m_iCurrentAnimation] ), "up" ) && m_sCustomOffAnimation && !m_bDescending )
			animationtoplay = m_sCustomOffAnimation;
	}

	((CBaseMonster *)pPlayer->pDeathScene)->SetSequenceByName( (char *)STRING( animationtoplay ) );
	m_sPreviousAnimation = animationtoplay;

	bool wasdescending = false;

	if ( m_bDescending )
		wasdescending = true;

	float delay = 0;
	delay = GetSequenceTimeNamed( GET_MODEL_PTR( ENT( pPlayer->pDeathScene->pev ) ), (char *)STRING( animationtoplay ) );

	if ( m_bDescending )
	{
		m_bGotOnButNotDescended = true;
		m_bDescending = false;
		m_bOriginReductionQueuedPrevious = true;
	}

	if ( delay != 0 )
	{
		if ( m_iCurrentAnimation == m_iNumAnimations && !wasdescending )
		{
			m_bEnableControl = true;
			m_flControlEnableTime = delay + gpGlobals->time;
		}

		pev->teleport_time = delay + gpGlobals->time;
	}
	else
		ALERT( at_console, "Unable to find delay\n" );

	if ( !wasdescending )
		m_iCurrentAnimation++;
}

void CCoFLadderManager::StartPreviousAnimation( void )
{
	if ( !pev->owner )
		return;

	CBaseEntity *pEntity = CBaseEntity::Instance( pev->owner );

	if ( !pEntity )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pEntity->pev );

	if ( !pPlayer )
		return;

	if ( !pPlayer->pDeathScene )
		return;

	m_bGotOnButNotDescended = false;

	m_iCurrentAnimation--;

	if ( m_bOriginReductionQueuedPrevious )
	{
		m_bOriginReductionQueuedPrevious = false;
		pPlayer->pDeathScene->pev->origin.z -= 48;
	}

	if ( m_bDescending )
	{
		if ( strstr( STRING( m_cAnimation[m_iCurrentAnimation] ), "climb_end" ) && m_sCustomOffDescendAnimation )
			m_cAnimation[m_iCurrentAnimation] = m_sCustomOffDescendAnimation;
	}
	else
	{
		if ( strstr( STRING( m_cAnimation[m_iCurrentAnimation] ), "climb_end" ) && m_sCustomOffAnimation )
			m_cAnimation[m_iCurrentAnimation] = m_sCustomOffAnimation;
	}

	if ( m_iCurrentAnimation <= 1 )
	{
		if ( strstr( STRING( m_sPreviousAnimation ), "climb_loop_down" ) )
			pPlayer->pDeathScene->pev->origin.z -= 48;

		if ( m_bDescending )
		{
			if ( m_sCustomOffDescendAnimation )
				((CBaseMonster *)pPlayer->pDeathScene)->SetSequenceByName( (char *)STRING( m_sCustomOffDescendAnimation ) );
			else
				((CBaseMonster *)pPlayer->pDeathScene)->SetSequenceByName( "climb_end_down" );
		}
		else
		{
			if ( m_sCustomOffAnimation )
				((CBaseMonster *)pPlayer->pDeathScene)->SetSequenceByName( (char *)STRING( m_sCustomOffAnimation ) );
			else
				((CBaseMonster *)pPlayer->pDeathScene)->SetSequenceByName( "climb_end_down" );
		}

		float delay = 0;

		if ( m_bDescending )
		{
			if ( m_sCustomOffDescendAnimation )
			{
				delay = GetSequenceTimeNamed( GET_MODEL_PTR( ENT( pPlayer->pDeathScene->pev ) ), (char *)STRING( m_sCustomOffDescendAnimation ) );
				m_sPreviousAnimation = m_sCustomOffDescendAnimation;
			}
			else
			{
				delay = GetSequenceTimeNamed( GET_MODEL_PTR( ENT( pPlayer->pDeathScene->pev ) ), "climb_end_down" );
				m_sPreviousAnimation = MAKE_STRING( "climb_end_down" );
			}
		}
		else
		{
			if ( m_sCustomOffAnimation )
			{
				delay = GetSequenceTimeNamed( GET_MODEL_PTR( ENT( pPlayer->pDeathScene->pev ) ), (char *)STRING( m_sCustomOffAnimation ) );
				m_sPreviousAnimation = m_sCustomOffAnimation;
			}
			else
			{
				delay = GetSequenceTimeNamed( GET_MODEL_PTR( ENT( pPlayer->pDeathScene->pev ) ), "climb_end_down" );
				m_sPreviousAnimation = MAKE_STRING( "climb_end_down" );
			}
		}

		if ( delay != 0 )
		{
			if ( m_iCurrentAnimation == m_iNumAnimations )
			{
				m_bEnableControl = true;
				m_flControlEnableTime = delay + gpGlobals->time;
			}

			pev->teleport_time = delay + gpGlobals->time;
		}
		else
			ALERT( at_console, "Unable to find delay\n" );

		m_flControlEnableTime = delay + gpGlobals->time;
		m_bEnableControl = true;
	}
	else
	{
		if ( strstr( STRING( m_sPreviousAnimation ), "climb_loop_down" ) )
			pPlayer->pDeathScene->pev->origin.z -= 48;

		((CBaseMonster *)pPlayer->pDeathScene)->SetSequenceByName( "climb_loop_down" );

		float delay = GetSequenceTimeNamed( GET_MODEL_PTR( ENT( pPlayer->pDeathScene->pev ) ), "climb_loop_down" );
		m_sPreviousAnimation = MAKE_STRING( "climb_loop_down" );

		if ( delay != 0 )
		{
			if ( m_iCurrentAnimation == m_iNumAnimations )
			{
				m_bEnableControl = true;
				m_flControlEnableTime = delay + gpGlobals->time;
			}

			pev->teleport_time = delay + gpGlobals->time;
		}
		else
			ALERT( at_console, "Unable to find delay\n" );
	}
}

LINK_ENTITY_TO_CLASS( cof_ladder_manager, CCoFLadderManager );


class CCoFLadderManagerUse : public CBaseToggle
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() | FCAP_IMPULSE_USE) & ~FCAP_ACROSS_TRANSITION; }
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	float m_flNextUse;
};

TYPEDESCRIPTION CCoFLadderManagerUse::m_SaveData[] =
{
	DEFINE_FIELD( CCoFLadderManagerUse, m_flNextUse, FIELD_TIME ),
	DEFINE_FIELD( CCoFLadderManagerUse, m_sMaster, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CCoFLadderManagerUse, CBaseEntity );

LINK_ENTITY_TO_CLASS( cof_ladder_manager_use, CCoFLadderManagerUse );

void CCoFLadderManagerUse::Spawn( void )
{
	Precache();

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin( this, pev->origin );
	UTIL_SetSize( pev, pev->mins, pev->maxs );
	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	m_flNextUse = 0;

	pev->frame = 0;
}

void CCoFLadderManagerUse::Precache( void )
{
	if ( pev->noise )
		PRECACHE_SOUND( (char *)STRING( pev->noise ) );
}

void CCoFLadderManagerUse::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	if ( !UTIL_IsMasterTriggered( m_sMaster, pActivator ) )
		return;

	if ( m_flNextUse > gpGlobals->time )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( !pPlayer )
		return;

	// only climb from the ground
	if ( !( pPlayer->pev->flags & FL_ONGROUND ) )
		return;

	if ( pev->noise )
		EMIT_SOUND( ENT( pActivator->pev ), CHAN_AUTO, STRING( pev->noise ), 1, ATTN_NORM );

	if ( ( pev->spawnflags & 1 ) && pev->target )
	{
		CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );

		if ( pTarget )
		{
			int distance = pPlayer->pev->origin.z - pTarget->pev->origin.z;

			if ( pev->target )
			{
				if ( distance <= 40 )
					FireTargets( STRING( pev->target ), pPlayer, pPlayer, USE_TOGGLE, 0 );
			}
		}
	}
	else
	{
		if ( pev->target )
			FireTargets( STRING( pev->target ), pPlayer, pPlayer, USE_TOGGLE, pev->iuser1 );
	}

	m_flNextUse = gpGlobals->time + 1;
}

class CCoFLadderEndTop : public CPointEntity
{
	void Spawn( void );
};

void CCoFLadderEndTop::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

LINK_ENTITY_TO_CLASS( cof_ladderend_top, CCoFLadderEndTop );

class CCoFLadderEndBottom : public CPointEntity
{
	void Spawn( void );
};

void CCoFLadderEndBottom::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

LINK_ENTITY_TO_CLASS( cof_ladderend_bottom, CCoFLadderEndBottom );
