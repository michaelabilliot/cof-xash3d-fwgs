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
//
// cof_numeralplate.cpp - the two halves of the numeral plate and the
// telephone number that goes with them
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"

class CNumeralPlateLeft : public CPointEntity
{
	void Spawn( void );
	void Think( void );
	void Precache( void );
};

class CNumeralPlateRight : public CPointEntity
{
	void Spawn( void );
};

void CNumeralPlateLeft::Precache( void )
{
	PRECACHE_MODEL( "models/items/numeral_plate1.mdl" );
	PRECACHE_MODEL( "models/items/numeral_plate2.mdl" );
	PRECACHE_MODEL( "models/items/numeral_plate3.mdl" );

	PRECACHE_MODEL( "models/items/numeral_plate_left1.mdl" );
	PRECACHE_MODEL( "models/items/numeral_plate_left2.mdl" );
	PRECACHE_MODEL( "models/items/numeral_plate_left3.mdl" );

	PRECACHE_MODEL( "models/items/numeral_plate_right1.mdl" );
	PRECACHE_MODEL( "models/items/numeral_plate_right2.mdl" );
	PRECACHE_MODEL( "models/items/numeral_plate_right3.mdl" );
}

void CNumeralPlateLeft::Spawn( void )
{
	Precache( );
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;

	// pick one of the three plates
	pev->skin = RANDOM_LONG( 1, 3 );

	SetNextThink( 2 );
}

void CNumeralPlateLeft::Think( void )
{
	int scriptleft;
	CBaseEntity *pTarget;
	int scriptright;
	CBaseEntity *pPhone;
	int codenumber;

	pTarget = UTIL_FindEntityByClassname( NULL, "cof_numeralplate_right" );
	pPhone = UTIL_FindEntityByClassname( NULL, "cof_telephone" );

	switch ( pev->skin )
	{
	case 1:
		scriptleft = MAKE_STRING( "inventoryitems/leftplate_1.txt" );
		codenumber = MAKE_STRING( "51173896" );
		break;

	case 2:
		scriptleft = MAKE_STRING( "inventoryitems/leftplate_2.txt" );
		codenumber = MAKE_STRING( "51173979" );
		break;

	case 3:
		scriptleft = MAKE_STRING( "inventoryitems/leftplate_3.txt" );
		codenumber = MAKE_STRING( "51177816" );
		break;
	}

	if ( pTarget )
	{
		pTarget->pev->skin = pev->skin;

		switch ( pTarget->pev->skin )
		{
		case 1:
			scriptright = MAKE_STRING( "inventoryitems/rightplate_1.txt" );
			break;

		case 2:
			scriptright = MAKE_STRING( "inventoryitems/rightplate_2.txt" );
			break;

		case 3:
			scriptright = MAKE_STRING( "inventoryitems/rightplate_3.txt" );
			break;
		}

		CBaseEntity *pKeyLeft = CBaseEntity::CreateWithScript( "item_key", scriptleft, pev->origin, pev->angles, edict(), 0 );
		CBaseEntity *pKeyRight = CBaseEntity::CreateWithScript( "item_key", scriptright, pTarget->pev->origin, pTarget->pev->angles, pTarget->edict(), 0 );

		if ( pKeyRight )
			UTIL_Remove( pTarget );
		else
			ALERT( at_console, "cof_numeralplate_left: Unable to create RIGHT key!\n" );

		if ( pKeyLeft )
			UTIL_Remove( this );
		else
			ALERT( at_console, "cof_numeralplate_left: Unable to create LEFT key!\n" );

		if ( pPhone )
			pPhone->pev->message = codenumber;
		else
			ALERT( at_console, "cof_numeralplate_left: Unable to find telephone entity!\n" );
	}
	else
		ALERT( at_console, "cof_numeralplate_left: Unable to find cof_numeralplate_right!\n" );
}

LINK_ENTITY_TO_CLASS( cof_numeralplate_left, CNumeralPlateLeft );

void CNumeralPlateRight::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

LINK_ENTITY_TO_CLASS( cof_numeralplate_right, CNumeralPlateRight );
