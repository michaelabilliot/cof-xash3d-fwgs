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
// cof_userpasswordnote.cpp - note with the user name or the password of the computer
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "player.h"

extern int gmsgUserDoc;

// declared in cof_importantclassdefs.h
class CCofComputer : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	void ComputerCheck( CBaseEntity *pUser, char *username, char *password );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	char m_sUserName[64];
	char m_sPassword[64];
	static const char *pUsernames[];
	static const char *pPasswords[];
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

// declared in cof_importantclassdefs.h
class CCoFDocument : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	static TYPEDESCRIPTION m_SaveData[];
	char page1[32];
	char page2[32];
	char page3[32];
	char page4[32];
	char note1[32];
	char note2[32];
	char note3[32];
	char note4[32];
	int turnsound;
	char label[48];
};

class CPasswordNote : public CBaseAnimating
{
	void Spawn( void );
	void Precache( void );
	int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() | FCAP_IMPULSE_USE; }
	void EXPORT NoteUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData *pkvd );

	bool m_bIssuedNote;

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	CCoFDocument *m_pNoteDocument;
};

TYPEDESCRIPTION CPasswordNote::m_SaveData[] =
{
	DEFINE_FIELD( CPasswordNote, m_pNoteDocument, FIELD_CLASSPTR ),
	DEFINE_FIELD( CPasswordNote, m_bIssuedNote, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CPasswordNote, CBaseEntity );

LINK_ENTITY_TO_CLASS( cof_passwordnote, CPasswordNote );

void CPasswordNote::KeyValue( KeyValueData *pkvd )
{
	// 0 = user name note, 1 = password note
	if ( FStrEq( pkvd->szKeyName, "usertype" ) )
	{
		pev->iuser1 = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CPasswordNote::Spawn( void )
{
	Precache( );

	if ( pev->model )
		SET_MODEL( ENT( pev ), STRING( pev->model ) );
	else
		SET_MODEL( ENT( pev ), "models/items/generic_note.mdl" );

	UTIL_SetOrigin( this, pev->origin );

	pev->solid = SOLID_BBOX;
	UTIL_SetSize( pev, Vector( -6, -5, 0 ), Vector( 6, 5, 1 ) );

	pev->renderfx = 100;

	pev->origin.z += 1;
	DROP_TO_FLOOR( ENT( pev ) );
	SetUse( &CPasswordNote::NoteUse );
}

void CPasswordNote::Precache( void )
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING( pev->model ) );
	else
		PRECACHE_MODEL( "models/items/generic_note.mdl" );
}

void CPasswordNote::NoteUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( pActivator->IsPlayer() )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

		// the computer knows which user name and password this map uses
		CBaseEntity *pComputer = NULL;
		pComputer = UTIL_FindEntityByClassname( NULL, "cof_computer" );

		if ( !pComputer )
		{
			ALERT( at_console, "No computer at all!\n" );
			return;
		}

		int usernumber;
		int passnumber;

		if ( !pComputer->pev->iuser2 || !pComputer->pev->iuser3 )
		{
			ALERT( at_console, "No computer user or 3! %i %i\n", pComputer->pev->iuser2, pComputer->pev->iuser3 );
			return;
		}

		usernumber = pComputer->pev->iuser2;
		passnumber = pComputer->pev->iuser3;

		if ( !m_pNoteDocument && !m_bIssuedNote )
		{
			m_bIssuedNote = true;

			CBaseEntity *pDocument = CBaseEntity::Create( "cof_document", pev->origin, pev->angles, edict() );

			if ( pDocument )
			{
				CCoFDocument *pDoc = GetClassPtr( (CCoFDocument *)pDocument->pev );
				CCofComputer *pPC = GetClassPtr( (CCofComputer *)pComputer->pev );

				if ( pDoc && pPC )
				{
					if ( pev->iuser1 )
					{
						if ( usernumber >= 1 && usernumber <= 8 && passnumber >= 1 && passnumber <= 8 )
						{
							char szNote[32];
							char szPassword[128];

							sprintf( szPassword, "pass%i", passnumber );
							strncpy( pDoc->page1, szPassword, 32 );

							// the note text is in a script file
							sprintf( szNote, "%s.txt", CCofComputer::pPasswords[passnumber] );
							strncpy( pDoc->note1, szNote, 32 );
							strncpy( pDoc->label, "gfx/vgui/notes/hiddennote1.tga", 48 );
						}
					}
					else
					{
						if ( usernumber >= 1 && usernumber <= 8 && passnumber >= 1 && passnumber <= 8 )
						{
							char szNote[32];
							char szUsername[128];

							sprintf( szUsername, "user%i", usernumber );
							strncpy( pDoc->page1, szUsername, 32 );

							// the note text is in a script file
							sprintf( szNote, "%s.txt", CCofComputer::pUsernames[usernumber] );
							strncpy( pDoc->note1, szNote, 32 );
							strncpy( pDoc->label, "gfx/vgui/notes/hiddennote2.tga", 48 );
						}
					}

					pDoc->Spawn();
					m_pNoteDocument = pDoc;

					if ( pPlayer )
						pPlayer->m_pCurrentDocument = m_pNoteDocument;
				}
			}
		}

		if ( pPlayer )
		{
			if ( pev->iuser1 )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgUserDoc, NULL, pPlayer->pev );
					WRITE_BYTE( 2 );
					WRITE_BYTE( passnumber );
				MESSAGE_END();
			}
			else
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgUserDoc, NULL, pPlayer->pev );
					WRITE_BYTE( 1 );
					WRITE_BYTE( usernumber );
				MESSAGE_END();
			}
		}
	}
}
