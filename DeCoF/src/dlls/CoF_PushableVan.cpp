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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "animation.h"

class CPushableVan : public CBaseAnimating
{
	virtual void Spawn( void );
	void EXPORT VanThink( void );
	virtual void Precache( void );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() | FCAP_CONTINUOUS_USE; }
	void EXPORT VanUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	bool longx;
	bool longy;
	int length;
	int width;
	int graboffset;
	CBasePlayer *pPusher[3];
};

LINK_ENTITY_TO_CLASS( cof_pushablevan, CPushableVan );


void CPushableVan::Spawn( void )
{
	Precache();

	if ( pev->model )
		SET_MODEL( ENT( pev ), STRING( pev->model ) );

	UTIL_SetOrigin( this, pev->origin );

	pev->solid = SOLID_BBOX;

	SetModelCollisionBox();

	SetUse( &CPushableVan :: VanUse );
	SetNextThink( 1 );
	SetThink( &CPushableVan::VanThink );

	if ( pev->mins.x > pev->mins.y )
	{
		longx = true;
		longy = false;

		{
			int max = pev->maxs.x;
			int min = pev->mins.x;

			if ( min < 0 )
				min *= -1;

			if ( max < 0 )
				max *= -1;

			length = min + max;
		}

		width = pev->maxs.y;
	}
	else
	{
		longx = false;
		longy = true;

		{
			int max = pev->maxs.y;
			int min = pev->mins.y;

			if ( min < 0 )
				min *= -1;

			if ( max < 0 )
				max *= -1;

			length = min + max;
		}

		width = pev->maxs.x;
	}
}

void CPushableVan::Precache( void )
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING( pev->model ) );
}

void CPushableVan::VanThink( void )
{
	int i;
	bool alreadypushing = false;
	int count = 0;
	int pushers = 0;

	for ( i = 1; i <= 2; i++ )
	{
		if ( pPusher[i] )
		{
			count++;

			if ( !pPusher[i]->IsAlive() || pPusher[i]->pev->fuser4 != 0 || pPusher[i]->edict()->free || !( ( pPusher[i]->pev->button | pPusher[i]->m_afButtonPressed | pPusher[i]->m_afButtonReleased ) & IN_USE ) )
			{
				if ( pPusher[i] )
					ALERT( at_console, "%s just released use\n", STRING( pPusher[i]->pev->netname ) );

				pPusher[i]->ViewLock( false, 0 );
				pPusher[i]->pev->fuser3 = 0;
				pPusher[i]->SetAnimation( PLAYER_IDLE );
				pPusher[i]->ToFirstPerson();
				pPusher[i] = NULL;
				count--;
			}
			else
			{
				pPusher[i]->pev->angles.x = 0;
				pPusher[i]->pev->angles.y = pev->angles.y + 180;
				pPusher[i]->pev->fixangle = 1;

				if ( ( pPusher[i]->pev->button | pPusher[i]->m_afButtonPressed | pPusher[i]->m_afButtonReleased ) & IN_RELOAD )
				{
					pushers++;

					if ( pushers > 2 )
						pushers = 2;
				}
				else
				{
					pushers--;

					if ( pushers < 0 )
						pushers = 0;
				}

				if ( pushers >= 1 )
				{
					pev->movetype = MOVETYPE_FLY;
					pev->solid = SOLID_SLIDEBOX;

					pev->velocity = pev->velocity + gpGlobals->v_forward * 1.1;

					float vellength = pev->velocity.Length();

					if ( vellength > 5 )
						pev->velocity = pev->velocity.Normalize() * 5;

					pPusher[i]->pev->velocity = pev->velocity * 10;
					pPusher[i]->pev->velocity[2] = 0;

					int animDesired = pPusher[i]->LookupSequence( "push_van" );

					if ( animDesired == -1 )
						animDesired = 0;

					if ( pPusher[i]->pev->sequence != animDesired )
					{
						if ( pPusher[i]->pev->sequence != animDesired || !pPusher[i]->m_fSequenceLoops )
							pPusher[i]->pev->frame = 0;

						pPusher[i]->pev->gaitsequence = 0;
						pPusher[i]->m_flSequenceOverrideTime = GetSequenceTime( GET_MODEL_PTR( ENT( pPusher[i]->pev ) ), animDesired ) + gpGlobals->time;

						if ( pPusher[i]->pev->sequence != animDesired )
						{
							pPusher[i]->pev->sequence = animDesired;
							pPusher[i]->pev->frame = 0;
							pPusher[i]->ResetSequenceInfo();
						}
					}
				}
				else
				{
					pev->velocity = g_vecZero;
					pPusher[i]->pev->velocity = pev->velocity;
					pPusher[i]->PlayerAnimation( "push_van_idle", true );
				}
			}
		}
		else
		{
			if ( count >= 2 )
				ALERT( at_console, "Max nerds are grabbing\n", count );
		}
	}

	if ( !count || !pushers )
		pev->velocity = g_vecZero;

	SetNextThink( 0.05 );
}

void CPushableVan::VanUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer && pPlayer->IsPlayer() && pPlayer->IsAlive() )
	{
		int i;
		int count = 0;
		bool alreadypushing = false;

		for ( i = 1; i <= 2; i++ )
		{
			if ( pPusher[i] && pPusher[i] == pPlayer )
			{
				alreadypushing = true;
				continue;
			}
		}

		if ( alreadypushing )
			return;

		for ( i = 1; i <= 2; i++ )
		{
			Vector2D vec2LOS;

			UTIL_MakeVectors( pev->angles );

			Vector origin = pev->origin + gpGlobals->v_forward * length;

			vec2LOS = ( pPlayer->pev->origin - origin ).Make2D();
			vec2LOS = vec2LOS.Normalize();

			float flDot = DotProduct( vec2LOS, gpGlobals->v_forward.Make2D() );

			if ( flDot >= 0 && !pPusher[i] && ( pPlayer->pev->flags & FL_ONGROUND ) && !( pPlayer->pev->flags & FL_DUCKING ) && !( pPlayer->m_afPhysicsFlags & PFLAG_DUCKING ) )
			{
				count++;
				pPusher[i] = pPlayer;
				ALERT( at_console, "%s grabbed the van\n", STRING( pPlayer->pev->netname ) );

				Vector playerorigin = pPlayer->pev->origin;

				if ( longx )
				{
					playerorigin.x = pev->origin.x;

					if ( i == 1 )
						playerorigin.x -= 36;

					if ( i == 2 )
						playerorigin.x += 36;
				}

				if ( longy )
				{
					playerorigin.y = pev->origin.y;

					if ( i == 1 )
						playerorigin.y -= 16;

					if ( i == 2 )
						playerorigin.y += 16;
				}

				UTIL_SetOrigin( pPlayer, playerorigin );

				pPusher[i]->pev->angles.x = 0;
				pPusher[i]->pev->angles.y = pev->angles.y + 180;
				pPusher[i]->pev->fixangle = 1;

				pPusher[i]->pev->velocity = gpGlobals->v_forward * 20;

				pPusher[i]->ViewLock( true, 666 );

				pPlayer->pev->velocity = g_vecZero;
				pPlayer->PlayerAnimation( "push_van_idle", true );

				pPlayer->pev->fuser3 = 50;
				pPlayer->ToThirdPerson();

				return;
			}
		}
	}
}
