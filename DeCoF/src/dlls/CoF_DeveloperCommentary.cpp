//=========================================================
// cof_developer_commentary: a node that plays a developer
// commentary mp3 when the player uses it
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

extern int gmsgMP3;
extern int gmsgChapter;

// pev->euser1 is the player, pev->owner the player listening, pev->fuser1 the
// end of the commentary, pev->fuser2 its length, pev->fuser4 the next check
class CCoFDevCommentary : public CBaseAnimating
{
	void Spawn( void );
	void Precache( void );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() | FCAP_IMPULSE_USE; }
	void EXPORT DevUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT DevThink( void );
	void SendInitMessage( CBasePlayer *pPlayer );
};

LINK_ENTITY_TO_CLASS( cof_developer_commentary, CCoFDevCommentary );

void CCoFDevCommentary::SendInitMessage( CBasePlayer *pPlayer )
{
	if ( !( pev->spawnflags & 1 ) )
	{
		if ( CVAR_GET_FLOAT( "dev_commentary" ) == 0 )
		{
			UTIL_Remove( this );
			return;
		}
	}

	if ( pPlayer )
	{
		pev->euser1 = pPlayer->edict();

		if ( !( pev->spawnflags & 1 ) )
		{
			if ( !pPlayer->CheckingEndingOrUnlockables( 0x400000, false, false ) )
				UTIL_Remove( this );
		}
	}
}

void CCoFDevCommentary::Spawn( void )
{
	Precache();

	if ( pev->model )
		SET_MODEL( ENT( pev ), STRING( pev->model ) );
	else
		SET_MODEL( ENT( pev ), "models/developer_commentary.mdl" );

	UTIL_SetOrigin( this, pev->origin );

	pev->solid = SOLID_NOT;
	UTIL_SetSize( pev, Vector( -13, -13, 0 ), Vector( 13, 13, 19 ) );

	SetThink( &CCoFDevCommentary::DevThink );
	SetNextThink( 0 );

	SetUse( &CCoFDevCommentary::DevUse );

	pev->fuser4 = gpGlobals->time;

	pev->sequence = 0;
	pev->frame = 0;
	ResetSequenceInfo();
}

void CCoFDevCommentary::Precache( void )
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING( pev->model ) );
	else
		PRECACHE_MODEL( "models/developer_commentary.mdl" );
}

void CCoFDevCommentary::DevThink( void )
{
	if ( gpGlobals->time > pev->fuser4 )
	{
		if ( !( pev->spawnflags & 1 ) )
		{
			if ( CVAR_GET_FLOAT( "dev_commentary" ) == 0 )
				UTIL_Remove( this );
		}

		if ( pev->euser1 )
		{
			CBaseEntity *pPlayer = CBaseEntity::Instance( pev->euser1 );

			if ( pPlayer && pPlayer->IsPlayer() )
			{
				if ( !( pev->spawnflags & 1 ) )
				{
					if ( !GetClassPtr( (CBasePlayer *)pPlayer->pev )->CheckingEndingOrUnlockables( 0x400000, false, false ) )
						UTIL_Remove( this );
				}
			}
		}
		else
		{
			CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

			if ( pPlayer )
				pev->euser1 = pPlayer->edict();
		}

		pev->fuser4 = gpGlobals->time + 0.5;
	}

	// end of the commentary (or skipped with enter)
	if ( pev->owner )
	{
		if ( ( GetAsyncKeyState( VK_RETURN ) & 0x8000 ) || gpGlobals->time > pev->fuser1 )
		{
			CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pev->owner );

			if ( pPlayer && pPlayer->IsPlayer() )
			{
				if ( pev->iuser2 != 1 )
					pPlayer->m_bInfiniteHealth = false;

				if ( pev->iuser3 != 1 )
					pPlayer->pev->flags &= ~FL_NOTARGET;

				pPlayer->m_iMp3NeedsUpdate = 1;
				CVAR_SET_FLOAT( "volume", pev->vuser1.x );

				pev->owner = NULL;

				pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
				pPlayer->m_DrawBars = 0;

				pPlayer->pev->fuser3 = 0;
			}
		}
	}

	SetNextThink( 0 );
}

void CCoFDevCommentary::DevUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( pActivator->IsPlayer() )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

		if ( pPlayer )
		{
			if ( pev->noise )
			{
				pev->owner = pPlayer->edict();

				MESSAGE_BEGIN( MSG_ONE, gmsgMP3, NULL, pPlayer->pev );
					WRITE_BYTE( 0 );
					WRITE_BYTE( 1 );
					WRITE_COORD( 0 );
					WRITE_STRING( STRING( pev->noise ) );
				MESSAGE_END();

				// turn the music down, DevThink restores it
				pev->vuser1.x = CVAR_GET_FLOAT( "volume" );

				float tempvolume = pev->vuser1.x;
				tempvolume *= 0.25;

				CVAR_SET_FLOAT( "volume", tempvolume );

				pPlayer->m_iHideHUD |= HIDEHUD_HEALTH;
				pPlayer->m_DrawBars = 1;

				if ( pPlayer->m_bInfiniteHealth )
					pev->iuser2 = 1;
				else
					pPlayer->m_bInfiniteHealth = true;

				if ( pPlayer->pev->flags & FL_NOTARGET )
					pev->iuser3 = 1;
				else
					pPlayer->pev->flags |= FL_NOTARGET;

				pev->fuser1 = gpGlobals->time + pev->fuser2;

				if ( !pev->message )
				{
					switch ( pev->iuser1 )
					{
					case 0:
						MESSAGE_BEGIN( MSG_ONE, gmsgChapter, NULL, pPlayer->pev );
							WRITE_STRING( "Minuit - Press Enter to skip" );
							WRITE_STRING( NULL );
						MESSAGE_END();
						break;

					case 1:
						MESSAGE_BEGIN( MSG_ONE, gmsgChapter, NULL, pPlayer->pev );
							WRITE_STRING( "ruMpel - Press Enter to skip" );
							WRITE_STRING( NULL );
						MESSAGE_END();
						break;

					case 2:
						MESSAGE_BEGIN( MSG_ONE, gmsgChapter, NULL, pPlayer->pev );
							WRITE_STRING( "Sporkeh - Press Enter to skip" );
							WRITE_STRING( NULL );
						MESSAGE_END();
						break;

					case 3:
						MESSAGE_BEGIN( MSG_ONE, gmsgChapter, NULL, pPlayer->pev );
							WRITE_STRING( "DragonNOR - Press Enter to skip" );
							WRITE_STRING( NULL );
						MESSAGE_END();
						break;

					case 4:
						MESSAGE_BEGIN( MSG_ONE, gmsgChapter, NULL, pPlayer->pev );
							WRITE_STRING( "BerZerk - Press Enter to skip" );
							WRITE_STRING( NULL );
						MESSAGE_END();
						break;

					case 5:
						MESSAGE_BEGIN( MSG_ONE, gmsgChapter, NULL, pPlayer->pev );
							WRITE_STRING( "Mr. Sunabouzu - On Sporkeh" );
							WRITE_STRING( NULL );
						MESSAGE_END();
						break;
					}
				}
				else
				{
					MESSAGE_BEGIN( MSG_ONE, gmsgChapter, NULL, pPlayer->pev );
						WRITE_STRING( STRING( pev->message ) );
						WRITE_STRING( NULL );
					MESSAGE_END();
				}

				pPlayer->pev->fuser3 = 5;
			}
		}
	}
}
