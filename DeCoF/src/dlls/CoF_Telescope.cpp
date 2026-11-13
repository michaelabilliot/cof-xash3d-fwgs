//=========================================================
// cof_telescope: lets the player look through a fixed
// camera (cof_telescope_camera) until he uses it again
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"

extern int gmsgCameraZoom;
extern int gmsgScope;
extern int gmsgTelescope;

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );

class CTelescope : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT TelescopeThink( void );

	EHANDLE m_pPlayer;
	EHANDLE m_pView;
	Vector m_vOriginalAngles;

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};

TYPEDESCRIPTION	CTelescope::m_SaveData[] =
{
	DEFINE_FIELD( CTelescope, m_pView, FIELD_EHANDLE ),
	DEFINE_FIELD( CTelescope, m_pPlayer, FIELD_EHANDLE ),
	DEFINE_FIELD( CTelescope, m_vOriginalAngles, FIELD_VECTOR ),
};

IMPLEMENT_SAVERESTORE( CTelescope, CBaseEntity );

LINK_ENTITY_TO_CLASS( cof_telescope, CTelescope );

void CTelescope::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CTelescope::Precache( void )
{
}

void CTelescope::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{
		// cancelled by cof_telescope_cancel: only tell the number again
		if ( pev->frags != 0 && pev->health == 0 )
		{
			char szMessage[128];
			sprintf( szMessage, "Hmm... The board said %i...", pPlayer->m_iKeypadNumber );
			UTIL_ProMessage( pPlayer, szMessage, 5, false );
			return;
		}

		m_pPlayer = pPlayer;

		if ( pev->message && !m_pView )
		{
			CBaseEntity *pView = UTIL_FindEntityByTargetname( NULL, STRING( pev->message ) );

			if ( pView )
			{
				m_vOriginalAngles = pView->pev->angles;
				m_pView = pView;
			}
		}

		if ( !pev->body )
		{
			UTIL_ProMessage( pPlayer, "Press USE or MOUSE1 to return, MOUSE to rotate", 5, false );
			pev->body = 1;
		}

		pev->health = !pev->health;

		if ( pev->health != 0 )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, m_pPlayer->pev );
				WRITE_COORD( 350 );
				WRITE_BYTE( 1 );
				WRITE_BYTE( 1 );
			MESSAGE_END();

			MESSAGE_BEGIN( MSG_ONE, gmsgScope, NULL, m_pPlayer->pev );
				WRITE_BYTE( 2 );
			MESSAGE_END();

			m_pPlayer->pev->vuser2.x = 35;

			pPlayer->EnableControl( FALSE );

			GetClassPtr( (CBasePlayer *)m_pPlayer->pev )->m_pTelescope = this;

			if ( m_pView )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgTelescope, NULL, pPlayer->pev );
					WRITE_SHORT( ENTINDEX( m_pView->edict() ) );
					WRITE_SHORT( m_pView->pev->angles.x );
					WRITE_SHORT( m_pView->pev->angles.y );
					WRITE_SHORT( m_pView->pev->angles.z );
				MESSAGE_END();
			}
		}
		else
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, m_pPlayer->pev );
				WRITE_COORD( 350 );
				WRITE_BYTE( 0 );
				WRITE_BYTE( 1 );
			MESSAGE_END();

			MESSAGE_BEGIN( MSG_ONE, gmsgScope, NULL, m_pPlayer->pev );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			m_pPlayer->pev->vuser2.x = 90;

			GetClassPtr( (CBasePlayer *)m_pPlayer->pev )->m_pTelescope = NULL;

			pPlayer->EnableControl( TRUE );

			if ( m_pView )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgTelescope, NULL, pPlayer->pev );
					WRITE_SHORT( -1 );
					WRITE_SHORT( m_pView->pev->angles.x );
					WRITE_SHORT( m_pView->pev->angles.y );
					WRITE_SHORT( m_pView->pev->angles.z );
				MESSAGE_END();
			}
		}
	}
}


class CFixedCamera : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int  Classify( void );
};

LINK_ENTITY_TO_CLASS( cof_telescope_camera, CFixedCamera );

int CFixedCamera::Classify( void )
{
	return CLASS_NONE;
}

void CFixedCamera::Spawn( void )
{
	Precache();

	SET_MODEL( ENT( pev ), "sprites/null.spr" );

	UTIL_SetSize( pev, Vector( -16, -16, -36 ), Vector( 16, 16, 36 ) );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;
	m_bloodColor = DONT_BLEED;
	m_MonsterState = MONSTERSTATE_NONE;
}

void CFixedCamera::Precache( void )
{
	PRECACHE_MODEL( "sprites/null.spr" );
}


class CCofTelescopeCancel : public CPointEntity
{
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_telescope_cancel, CCofTelescopeCancel );

void CCofTelescopeCancel::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofTelescopeCancel::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{
		if ( pev->message )
		{
			CBaseEntity *pTelescope = UTIL_FindEntityByTargetname( NULL, STRING( pev->message ) );

			if ( pTelescope )
				pTelescope->pev->frags = 1;
		}
	}
}
