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
// item_key - a key item the player picks up into the inventory
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );
extern char *COM_Parse( char *data );
extern char com_token[ 1500 ];

// The 1.55 debug build declares this class in cof_importantclassdefs.h.
class CKeyitemModel : public CBaseAnimating
{
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void SendInitMessage( CBasePlayer *player );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() | FCAP_IMPULSE_USE; }
	void EXPORT KeyitemUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void KeyValue( KeyValueData *pkvd );
	void SetItemVariables( const char * );
	char *GetDescriptionFromItem( char *item, char *filename );
	float m_useTime;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

LINK_ENTITY_TO_CLASS( item_key, CKeyitemModel );

TYPEDESCRIPTION CKeyitemModel::m_SaveData[] =
{
	DEFINE_FIELD( CKeyitemModel, m_useTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CKeyitemModel, CBaseAnimating );


char *CKeyitemModel::GetDescriptionFromItem( char *item, char *filename )
{
	int gSCounter = 0;
	char szItemName[512];
	int length;
	char *pFile;
	char *aFile;

	aFile = pFile = (char *)LOAD_FILE_FOR_ME( filename, &length );

	if ( !pFile || !length )
	{
		ALERT( at_console, " *\n" );
		ALERT( at_console, " * GetItemDescription failed to load file %s\n", filename );
		ALERT( at_console, " *\n" );
		return "";
	}

	while ( 1 )
	{
		// item name
		pFile = COM_Parse( pFile );
		if ( strlen( com_token ) <= 0 )
			break;

		strcpy( szItemName, com_token );

		// opening brace
		pFile = COM_Parse( pFile );
		if ( com_token[0] != '{' )
		{
			ALERT( at_console, " *\n" );
			ALERT( at_console, " * CheckItem parsing error: expecting { after key name\n" );
			ALERT( at_console, " *\n" );
			break;
		}

		while ( 1 )
		{
			pFile = COM_Parse( pFile );
			if ( com_token[0] == '}' )
				break;

			if ( !strcmp( "data", com_token ) )
			{
				pFile = COM_Parse( pFile );

				if ( !strcmp( szItemName, item ) )
				{
					FREE_FILE( aFile );
					return com_token;
				}
			}
		}
		gSCounter++;
	}

	FREE_FILE( aFile );

	ALERT( at_console, "KEY ERROR: Couldn't parse token of key at %f %f %f!\n", pev->origin.x, pev->origin.y, pev->origin.z );
	return "";
}

void CKeyitemModel::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "customsound" ) )
	{
		pev->noise = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseAnimating::KeyValue( pkvd );
}

void CKeyitemModel::Spawn( void )
{
	if ( !pev->message )
	{
		ALERT( at_error, "CoF: Key with no item .txt at %f %f %f!\n", pev->origin.x, pev->origin.y, pev->origin.z );
		pev->armorvalue = 0;
		UTIL_Remove( this );
	}

	pev->renderfx = 99;

	while ( !pev->model )
	{
		pev->model = ALLOC_STRING( GetDescriptionFromItem( "3dmdl", (char *)STRING( pev->message ) ) );
	}

	Precache();

	if ( pev->model )
		SET_MODEL( ENT( pev ), STRING( pev->model ) );
	else
	{
		ALERT( at_error, "CoF: Key with no model at %f %f %f!\n", pev->origin.x, pev->origin.y, pev->origin.z );
		pev->armorvalue = 0;
		UTIL_Remove( this );
	}

	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_TOSS;

	SetModelCollisionBox();

	SetUse( &CKeyitemModel :: KeyitemUse );

	if ( ( pev->spawnflags & 1 ) && CVAR_GET_FLOAT( "difficulty" ) != 4 )
	{
		pev->armorvalue = 0;
		UTIL_Remove( this );
	}

	if ( pev->spawnflags & 2 )
		pev->iuser1 = 3;
}

void CKeyitemModel::Precache( void )
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING( pev->model ) );
}

void CKeyitemModel::SendInitMessage( CBasePlayer *player )
{
	if ( player )
	{
		if ( ( pev->spawnflags & 2 ) && !player->CheckingEndingOrUnlockables( 0x800000, false, false ) )
			UTIL_Remove( this );
	}
}

void CKeyitemModel::KeyitemUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( m_useTime > gpGlobals->time )
		return;

	if ( pActivator->IsPlayer() )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

		if ( !pPlayer )
			return;

		if ( pev->target )
			FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );

		if ( ( pev->spawnflags & 2 ) && pev->iuser1 )
		{
			switch ( pev->iuser1 )
			{
			case 1:
				UTIL_ProMessage( pPlayer, "Hmm... Should I take it?", 5, false );
				pev->iuser1--;
				return;

			case 2:
				UTIL_ProMessage( pPlayer, "What the... It's addressed to me...", 5, false );
				pev->iuser1--;
				return;

			case 3:
				UTIL_ProMessage( pPlayer, "Looks like some kind of package. Something is written on it.", 5, false );
				pev->iuser1--;
				return;
			}
		}

		if ( pPlayer->InventoryAddCheck( this ) )
		{
			if ( ( pev->spawnflags & 1 ) && CVAR_GET_FLOAT( "difficulty" ) == 4 )
				pPlayer->m_iTapes = 5;

			pev->armorvalue = 0;
			UTIL_Remove( this );
		}
	}

	m_useTime = gpGlobals->time + 1;
}
