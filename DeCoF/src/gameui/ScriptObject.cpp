//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Option scripts (user.scr, settings.scr) and the controls built from them
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ScriptObject.h"
#include "EngineInterface.h"

#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>

using namespace vgui2;

static char token[ 1024 ];

#define SCRIPT_VERSION	1.0f

void StripFloatTrailingZeros( char *str )
{
	// scan for a '.'
	char *period = strchr( str, '.' );
	if ( !period )
		return;

	// start at the end and scan back to the period
	char *end;
	for ( end = str + strlen( str ) - 1; end > period; --end )
	{
		if ( *end == '0' )
		{
			*end = '\0';
		}
		else
		{
			// we've hit a real value, stop truncating
			break;
		}
	}

	// if we've made it up to the period, kill that to
	if ( *end == '.' )
	{
		*end = '\0';
	}
}

/////////////////////
objtypedesc_t objtypes[] =
{
	{ O_BOOL  , "BOOL" },
	{ O_NUMBER, "NUMBER" },
	{ O_LIST  , "LIST" },
	{ O_STRING, "STRING" }
};

mpcontrol_t::mpcontrol_t( Panel *parent, char const *panelName )
: Panel( parent, panelName )
{
	type = O_BADTYPE;
	pControl = NULL;
	pPrompt = NULL;
	pScrObj = NULL;

	next = NULL;

	SetPaintBackgroundEnabled( false );
}

void mpcontrol_t::OnSizeChanged( int wide, int tall )
{
	int inset = 4;

	if ( pPrompt )
	{
		int w = wide / 2;

		if ( pControl )
			pControl->SetBounds( w + 20, inset, w - 20, tall - 2 * inset );

		pPrompt->SetBounds( 0, inset, w + 20, tall - 2 * inset );
	}
	else
	{
		if ( pControl )
			pControl->SetBounds( 0, inset, wide, tall - 2 * inset );
	}
}

CScriptListItem::CScriptListItem()
{
	pNext = NULL;
	memset( szItemText, 0, 128 );
	memset( szValue, 0, 256 );
}

CScriptListItem::CScriptListItem( char const *strItem, char const *strValue )
{
	pNext = NULL;
	strncpy( szItemText, strItem, sizeof( szItemText ) );
	szItemText[ sizeof( szItemText ) - 1 ] = '\0';
	strncpy( szValue, strValue, sizeof( szValue ) );
	szValue[ sizeof( szValue ) - 1 ] = '\0';
}

CScriptObject::CScriptObject( void )
{
	type = O_BOOL;
	bSetInfo = false;  // Prepend "Setinfo" to keyvalue pair in config?
	pNext = NULL;
	pListItems = NULL;
}

CScriptObject::~CScriptObject()
{
	CScriptListItem *p, *n;

	p = pListItems;
	while ( p )
	{
		n = p->pNext;
		delete p;
		p = n;
	}
	pListItems = NULL;
}

void CScriptObject::SetCurValue( char const *strValue )
{
	strncpy( curValue, strValue, sizeof( curValue ) );
	curValue[ sizeof( curValue ) - 1 ] = '\0';

	fcurValue = (float)atof( curValue );

	if ( type == O_NUMBER || type == O_BOOL )
	{
		StripFloatTrailingZeros( curValue );
	}
}

void CScriptObject::AddItem( CScriptListItem *pItem )
{
	// Link it into the end of the list;
	CScriptListItem *p;
	p = pListItems;
	if ( !p )
	{
		pListItems = pItem;
		pItem->pNext = NULL;
		return;
	}

	while ( p )
	{
		if ( !p->pNext )
		{
			p->pNext = pItem;
			pItem->pNext = NULL;
			return;
		}
		p = p->pNext;
	}
}

/*
===================
UTIL_StripInvalidCharacters

Removes any possible formatting codes and double quote characters from the input string
===================
*/
void UTIL_StripInvalidCharacters( char *pszInput )
{
	char szOutput[4096];
	char *pIn, *pOut;

	pIn = pszInput;
	pOut = szOutput;

	*pOut = '\0';

	while ( *pIn )
	{
		if ( ( *pIn != '"' ) &&
			 ( *pIn != '%' ) )
		{
			*pOut++ = *pIn;
		}
		pIn++;
	}

	*pOut = '\0';

	// Copy back over, in place
	strcpy( pszInput, szOutput );
}

void FixupString( char *inString )
{
	char szBuffer[ 4096 ];
	strncpy( szBuffer, inString, sizeof( szBuffer ) );
	szBuffer[ sizeof( szBuffer ) - 1 ] = '\0';
	UTIL_StripInvalidCharacters( szBuffer );
	strcpy( inString, szBuffer );
}

void CScriptObject::WriteToScriptFile( FileHandle_t fp )
{
	FixupString( cvarname );
	filesystem()->FPrintf( fp, "\t\"%s\"\r\n", cvarname );

	filesystem()->FPrintf( fp, "\t{\r\n" );

	CScriptListItem *pItem;

	FixupString( prompt );

	switch ( type )
	{
	case O_BOOL:
		filesystem()->FPrintf( fp, "\t\t\"%s\"\r\n", prompt );
		filesystem()->FPrintf( fp, "\t\t{ BOOL }\r\n" );
		filesystem()->FPrintf( fp, "\t\t{ \"%i\" }\r\n", (int)fcurValue ? 1 : 0 );
		break;
	case O_NUMBER:
		filesystem()->FPrintf( fp, "\t\t\"%s\"\r\n", prompt );
		filesystem()->FPrintf( fp, "\t\t{ NUMBER %f %f }\r\n", fMin, fMax );
		filesystem()->FPrintf( fp, "\t\t{ \"%f\" }\r\n", fcurValue );
		break;
	case O_STRING:
		filesystem()->FPrintf( fp, "\t\t\"%s\"\r\n", prompt );
		filesystem()->FPrintf( fp, "\t\t{ STRING }\r\n" );
		FixupString( curValue );
		filesystem()->FPrintf( fp, "\t\t{ \"%s\" }\r\n", curValue );
		break;
	case O_LIST:
		filesystem()->FPrintf( fp, "\t\t\"%s\"\r\n", prompt );
		filesystem()->FPrintf( fp, "\t\t{\r\n\t\t\tLIST\r\n" );

		pItem = pListItems;
		while ( pItem )
		{
			UTIL_StripInvalidCharacters( pItem->szItemText );
			UTIL_StripInvalidCharacters( pItem->szValue );
			filesystem()->FPrintf( fp, "\t\t\t\"%s\" \"%s\"\r\n",
				pItem->szItemText, pItem->szValue );

			pItem = pItem->pNext;
		}

		filesystem()->FPrintf( fp, "\t\t}\r\n" );
		filesystem()->FPrintf( fp, "\t\t{ \"%f\" }\r\n", fcurValue );
		break;
	}

	if ( bSetInfo )
		filesystem()->FPrintf( fp, "\t\tSetInfo\r\n" );

	filesystem()->FPrintf( fp, "\t}\r\n\r\n" );
}

void CScriptObject::WriteToFile( FileHandle_t fp )
{
	FixupString( cvarname );
	filesystem()->FPrintf( fp, "\"%s\"\t\t", cvarname );

	CScriptListItem *pItem;
	int n, i;
	float fVal;

	switch ( type )
	{
	case O_BOOL:
		filesystem()->FPrintf( fp, "\"%s\"\r\n", fcurValue != 0.0 ? "1" : "0" );
		break;
	case O_NUMBER:
		fVal = fcurValue;
		if ( fMin != -1.0 )
			fVal = max( fVal, fMin );
		if ( fMax != -1.0 )
			fVal = min( fVal, fMax );
		filesystem()->FPrintf( fp, "\"%f\"\r\n", fVal );
		break;
	case O_STRING:
		FixupString( curValue );
		filesystem()->FPrintf( fp, "\"%s\"\r\n", curValue );
		break;
	case O_LIST:
		pItem = pListItems;
		n = (int)fcurValue;
		i = 0;
		while ( ( i < n ) && pItem )
		{
			i++;
			pItem = pItem->pNext;
		}

		if ( pItem )
		{
			UTIL_StripInvalidCharacters( pItem->szValue );
			filesystem()->FPrintf( fp, "\"%s\"\r\n", pItem->szValue );
		}
		else  //Couln't find index
		{
			filesystem()->FPrintf( fp, "\"0.0\"\r\n" );
		}
		break;
	}
}

void CScriptObject::WriteToConfig( void )
{
	char *pszKey;
	char szValue[2048];

	pszKey = (char *)cvarname;

	CScriptListItem *pItem;
	int n, i;
	float fVal;

	switch ( type )
	{
	case O_BOOL:
		sprintf( szValue, "%s", fcurValue != 0.0 ? "1" : "0" );
		break;
	case O_NUMBER:
		fVal = fcurValue;
		if ( fMin != -1.0 )
			fVal = max( fVal, fMin );
		if ( fMax != -1.0 )
			fVal = min( fVal, fMax );
		sprintf( szValue, "%f", fVal );
		break;
	case O_STRING:
		sprintf( szValue, "\"%s\"", (char *)curValue );
		UTIL_StripInvalidCharacters( szValue );
		break;
	case O_LIST:
		pItem = pListItems;
		n = (int)fcurValue;
		i = 0;
		while ( ( i < n ) && pItem )
		{
			i++;
			pItem = pItem->pNext;
		}

		if ( pItem )
		{
			sprintf( szValue, "%s", pItem->szValue );
			UTIL_StripInvalidCharacters( szValue );
		}
		else  //Couln't find index
		{
			sprintf( szValue, "0.0" );
		}
		break;
	}

	char command[ 256 ];
	if ( bSetInfo )
	{
		sprintf( command, "setinfo %s \"%s\"\n", pszKey, szValue );
	}
	else
	{
		sprintf( command, "%s \"%s\"\n", pszKey, szValue );
	}

	engine->pfnClientCmd( command );
}

objtype_t CScriptObject::GetType( char *pszType )
{
	int i;
	int nTypes;

	nTypes = sizeof( objtypes ) / sizeof( objtypedesc_t );

	for ( i = 0; i < nTypes; i++ )
	{
		if ( !stricmp( objtypes[i].szDescription, pszType ) )
			return objtypes[i].type;
	}

	return O_BADTYPE;
}

bool CScriptObject::ReadFromBuffer( char **pBuffer )
{
	// Get the first token.
	// The cvar we are setting
	*pBuffer = engine->COM_ParseFile( *pBuffer, token );
	if ( strlen( token ) <= 0 )
		return false;

	strcpy( cvarname, token );

	// Parse the {
	*pBuffer = engine->COM_ParseFile( *pBuffer, token );
	if ( strlen( token ) <= 0 )
		return false;

	if ( strcmp( token, "{" ) )
	{
		engine->Con_Printf( "Expecting '{', got '%s'", token );
		return false;
	}

	// Parse the Prompt
	*pBuffer = engine->COM_ParseFile( *pBuffer, token );
	if ( strlen( token ) <= 0 )
		return false;

	strcpy( prompt, token );

	// Parse the next {
	*pBuffer = engine->COM_ParseFile( *pBuffer, token );
	if ( strlen( token ) <= 0 )
		return false;

	if ( strcmp( token, "{" ) )
	{
		engine->Con_Printf( "Expecting '{', got '%s'", token );
		return false;
	}

	// Now parse the type:
	*pBuffer = engine->COM_ParseFile( *pBuffer, token );
	if ( strlen( token ) <= 0 )
		return false;

	type = GetType( token );
	if ( type == O_BADTYPE )
	{
		engine->Con_Printf( "Type '%s' unknown", token );
		return false;
	}

	switch ( type )
	{
	case O_BOOL:
		// Parse the next {
		*pBuffer = engine->COM_ParseFile( *pBuffer, token );
		if ( strlen( token ) <= 0 )
			return false;

		if ( strcmp( token, "}" ) )
		{
			engine->Con_Printf( "Expecting '{', got '%s'", token );
			return false;
		}
		break;
	case O_NUMBER:
		// Parse the Min
		*pBuffer = engine->COM_ParseFile( *pBuffer, token );
		if ( strlen( token ) <= 0 )
			return false;

		fMin = (float)atof( token );

		// Parse the Min
		*pBuffer = engine->COM_ParseFile( *pBuffer, token );
		if ( strlen( token ) <= 0 )
			return false;

		fMax = (float)atof( token );

		// Parse the next {
		*pBuffer = engine->COM_ParseFile( *pBuffer, token );
		if ( strlen( token ) <= 0 )
			return false;

		if ( strcmp( token, "}" ) )
		{
			engine->Con_Printf( "Expecting '{', got '%s'", token );
			return false;
		}
		break;
	case O_STRING:
		// Parse the next {
		*pBuffer = engine->COM_ParseFile( *pBuffer, token );
		if ( strlen( token ) <= 0 )
			return false;

		if ( strcmp( token, "}" ) )
		{
			engine->Con_Printf( "Expecting '{', got '%s'", token );
			return false;
		}
		break;
	case O_LIST:
		// Parse items until we get the }
		while ( 1 )
		{
			// Parse the next {
			*pBuffer = engine->COM_ParseFile( *pBuffer, token );
			if ( strlen( token ) <= 0 )
				return false;

			// Done?
			if ( !strcmp( token, "}" ) )
				break;

			//
			// Add the item to a list somewhere
			// AddItem( token )
			char strItem[ 128 ];
			char strValue[ 128 ];

			strncpy( strItem, token, sizeof( strItem ) );
			strItem[ sizeof( strItem ) - 1 ] = '\0';

			// Parse the value
			*pBuffer = engine->COM_ParseFile( *pBuffer, token );
			if ( strlen( token ) <= 0 )
				return false;

			strncpy( strValue, token, sizeof( strValue ) );
			strValue[ sizeof( strValue ) - 1 ] = '\0';

			CScriptListItem *pItem;
			pItem = new CScriptListItem( strItem, strValue );

			AddItem( pItem );
		}
		break;
	}

	//
	// Now read in the default value

	// Parse the {
	*pBuffer = engine->COM_ParseFile( *pBuffer, token );
	if ( strlen( token ) <= 0 )
		return false;

	if ( strcmp( token, "{" ) )
	{
		engine->Con_Printf( "Expecting '{', got '%s'", token );
		return false;
	}

	// Parse the default
	*pBuffer = engine->COM_ParseFile( *pBuffer, token );
	//if ( strlen( token ) <= 0 )
	//	return false;

	// Set the values
	strncpy( defValue, token, sizeof( defValue ) );
	defValue[ sizeof( defValue ) - 1 ] = '\0';
	fdefValue = (float)atof( token );

	if ( type == O_NUMBER )
	{
		StripFloatTrailingZeros( defValue );
	}

	SetCurValue( defValue );

	// Parse the }
	*pBuffer = engine->COM_ParseFile( *pBuffer, token );
	if ( strlen( token ) <= 0 )
		return false;

	if ( strcmp( token, "}" ) )
	{
		engine->Con_Printf( "Expecting '{', got '%s'", token );
		return false;
	}

	// Parse the final }
	*pBuffer = engine->COM_ParseFile( *pBuffer, token );
	if ( strlen( token ) <= 0 )
		return false;

	if ( !stricmp( token, "SetInfo" ) )
	{
		bSetInfo = true;
		// Parse the final }
		*pBuffer = engine->COM_ParseFile( *pBuffer, token );
		if ( strlen( token ) <= 0 )
			return false;
	}

	if ( strcmp( token, "}" ) )
	{
		engine->Con_Printf( "Expecting '{', got '%s'", token );
		return false;
	}

	return true;
}

/////////////////////////
CDescription::CDescription( CPanelListPanel *panel )
{
	pObjList = NULL;
	m_pListPanel = panel;
}

CDescription::~CDescription()
{
	CScriptObject *p, *n;

	p = pObjList;
	while ( p )
	{
		n = p->pNext;
		p->pNext = NULL;
		delete p;
		p = n;
	}

	pObjList = NULL;

	if ( m_pszHintText )
		free( m_pszHintText );
	if ( m_pszDescriptionType )
		free( m_pszDescriptionType );
}

void CDescription::AddObject( CScriptObject *pObj )
{
	// Link it into the end of the list;
	CScriptObject *p;
	p = pObjList;
	if ( !p )
	{
		pObjList = pObj;
		pObj->pNext = NULL;
		return;
	}

	while ( p )
	{
		if ( !p->pNext )
		{
			p->pNext = pObj;
			pObj->pNext = NULL;
			return;
		}
		p = p->pNext;
	}
}

bool CDescription::ReadFromBuffer( char **pBuffer )
{
	// Get the first token.
	*pBuffer = engine->COM_ParseFile( *pBuffer, token );
	if ( strlen( token ) <= 0 )
		return false;

	// Read VERSION #
	if ( stricmp( token, "VERSION" ) )
	{
		engine->Con_Printf( "Expecting 'VERSION', got '%s'", token );
		return false;
	}

	// Parse in the version #
	// Get the first token.
	*pBuffer = engine->COM_ParseFile( *pBuffer, token );
	if ( strlen( token ) <= 0 )
	{
		engine->Con_Printf( "Expecting version #" );
		return false;
	}

	float fVer;
	fVer = (float)atof( token );

	if ( fVer != SCRIPT_VERSION )
	{
		engine->Con_Printf( "Version mismatch, expecting %f, got %f", SCRIPT_VERSION, fVer );
		return false;
	}

	// Get the "DESCRIPTION"
	*pBuffer = engine->COM_ParseFile( *pBuffer, token );
	if ( strlen( token ) <= 0 )
		return false;

	// Read DESCRIPTION
	if ( stricmp( token, "DESCRIPTION" ) )
	{
		engine->Con_Printf( "Expecting 'DESCRIPTION', got '%s'", token );
		return false;
	}

	// Parse in the description type
	*pBuffer = engine->COM_ParseFile( *pBuffer, token );
	if ( strlen( token ) <= 0 )
	{
		engine->Con_Printf( "Expecting '%s'", m_pszDescriptionType );
		return false;
	}

	if ( stricmp( token, m_pszDescriptionType ) )
	{
		engine->Con_Printf( "Expecting %s, got %s", m_pszDescriptionType, token );
		return false;
	}

	// Parse the {
	*pBuffer = engine->COM_ParseFile( *pBuffer, token );
	if ( strlen( token ) <= 0 )
		return false;

	if ( strcmp( token, "{" ) )
	{
		engine->Con_Printf( "Expecting '{', got '%s'", token );
		return false;
	}

	char *pStart;
	CScriptObject *pObj;
	// Now read in the objects and link them in
	while ( 1 )
	{
		pStart = *pBuffer;

		// Get the first token.
		*pBuffer = engine->COM_ParseFile( *pBuffer, token );
		if ( strlen( token ) <= 0 )
			return false;

		// Read "cvar name" or  } when done
		if ( !stricmp( token, "}" ) )
			break;

		// Unget the token
		*pBuffer = pStart;

		// Create a new object
		pObj = new CScriptObject();
		if ( !pObj )
		{
			engine->Con_Printf( "Couldn't create script object" );
			return false;
		}

		if ( !pObj->ReadFromBuffer( &pStart ) )
		{
			delete pObj;
			return false;
		}

		*pBuffer = pStart;

		// Add to list
		AddObject( pObj );
	}

	return true;
}

bool CDescription::InitFromFile( char *pszFileName )
{
	// Load file into memory
	FileHandle_t file = filesystem()->Open( pszFileName, "rb" );
	if ( !file )
		return false;

	int len = filesystem()->Size( file );

	// read the file
	byte *buffer = new unsigned char[ len ];
	filesystem()->Read( buffer, len, file );
	filesystem()->Close( file );

	char *pBuffer = (char *)buffer;

	ReadFromBuffer( &pBuffer );

	delete[] buffer;

	return true;
}

void CDescription::WriteToFile( FileHandle_t fp )
{
	CScriptObject *pObj;

	WriteFileHeader( fp );

	pObj = pObjList;
	while ( pObj )
	{
		pObj->WriteToFile( fp );
		pObj = pObj->pNext;
	}
}

void CDescription::WriteToConfig( void )
{
	CScriptObject *pObj;

	pObj = pObjList;
	while ( pObj )
	{
		pObj->WriteToConfig();
		pObj = pObj->pNext;
	}
}

void CDescription::WriteToScriptFile( FileHandle_t fp )
{
	CScriptObject *pObj;

	WriteScriptHeader( fp );

	pObj = pObjList;
	while ( pObj )
	{
		pObj->WriteToScriptFile( fp );
		pObj = pObj->pNext;
	}

	filesystem()->FPrintf( fp, "}\r\n" );
}

void CDescription::TransferCurrentValues( const char *pszConfigFile )
{
	char szValue[ 1024 ];

	CScriptObject *pObj = pObjList;
	while ( pObj )
	{
		const char *value;

		if ( pObj->bSetInfo )
		{
			value = engine->LocalPlayerInfo_ValueForKey( pObj->cvarname );
		}
		else
		{
			value = engine->pfnGetCvarString( pObj->cvarname );
		}

		if ( value && value[ 0 ] )
		{
			strncpy( szValue, value, sizeof( szValue ) );
			szValue[ sizeof( szValue ) - 1 ] = '\0';

			// Fill in values
			strncpy( pObj->curValue, szValue, sizeof( pObj->curValue ) );
			pObj->curValue[ sizeof( pObj->curValue ) - 1 ] = '\0';
			pObj->fcurValue = (float)atof( szValue );

			strncpy( pObj->defValue, szValue, sizeof( pObj->defValue ) );
			pObj->defValue[ sizeof( pObj->defValue ) - 1 ] = '\0';
			pObj->fdefValue = (float)atof( szValue );
		}

		pObj = pObj->pNext;
	}
}

void CDescription::setDescription( const char *pszDesc )
{
	m_pszDescriptionType = strdup( pszDesc );
}

void CDescription::setHint( const char *pszHint )
{
	m_pszHintText = strdup( pszHint );
}
