//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Defines a symbol table
//
// $Header: $
// $NoKeywords: $
//=============================================================================

#pragma warning (disable:4514)

#include <string.h>
#include "utlsymbol.h"

#define INVALID_STRING_INDEX 0xFFFFFFFF

//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------

CUtlSymbolTable* CUtlSymbol::s_pSymbolTable = 0;


//-----------------------------------------------------------------------------
// symbol methods
//-----------------------------------------------------------------------------

void CUtlSymbol::Initialize()
{
	// necessary to allow us to create global symbols
	static bool symbolsInitialized = false;
	if (!symbolsInitialized)
	{
		s_pSymbolTable = new CUtlSymbolTable;
		symbolsInitialized = true;
	}
}

CUtlSymbolTable* CUtlSymbol::CurrTable()
{
	Initialize();
	return s_pSymbolTable;
}

CUtlSymbol::CUtlSymbol( char const* pStr )
{
	m_Id = CurrTable()->AddString( pStr );
}

char const* CUtlSymbol::String( ) const
{
	return CurrTable()->String(m_Id);
}

//-----------------------------------------------------------------------------
// checks if the symbol matches a string
//-----------------------------------------------------------------------------

bool CUtlSymbol::operator==( char const* pStr ) const
{
	if (m_Id == UTL_INVAL_SYMBOL)
		return false;
	return strcmp( String(), pStr ) == 0;
}



//-----------------------------------------------------------------------------
// symbol table stuff
//-----------------------------------------------------------------------------

struct LessCtx_t
{
	char const* m_pUserString;
	CUtlSymbolTable* m_pTable;

	LessCtx_t( ) : m_pUserString(0), m_pTable(0) {}
};

static LessCtx_t g_LessCtx;

bool CUtlSymbolTable::SymLess( unsigned int const& i1, unsigned int const& i2 )
{
	char const* str1 = (i1 == INVALID_STRING_INDEX) ? g_LessCtx.m_pUserString :
										&g_LessCtx.m_pTable->m_Strings[i1];
	char const* str2 = (i2 == INVALID_STRING_INDEX) ? g_LessCtx.m_pUserString :
										&g_LessCtx.m_pTable->m_Strings[i2];

	return strcmp( str1, str2 ) < 0;
}

bool CUtlSymbolTable::SymLessi( unsigned int const& i1, unsigned int const& i2 )
{
	char const* str1 = (i1 == INVALID_STRING_INDEX) ? g_LessCtx.m_pUserString :
										&g_LessCtx.m_pTable->m_Strings[i1];
	char const* str2 = (i2 == INVALID_STRING_INDEX) ? g_LessCtx.m_pUserString :
										&g_LessCtx.m_pTable->m_Strings[i2];

	return _stricmp( str1, str2 ) < 0;
}

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CUtlSymbolTable::CUtlSymbolTable( int growSize, int initSize, bool caseInsensitive ) :
	m_Lookup( growSize, initSize, caseInsensitive ? SymLessi : SymLess ), m_Strings( 256 )
{
}

CUtlSymbolTable::~CUtlSymbolTable()
{
}


CUtlSymbol CUtlSymbolTable::Find( char const* pString )
{
	if (!pString)
		return CUtlSymbol();

	// Store a special context used to help with insertion
	g_LessCtx.m_pUserString = pString;
	g_LessCtx.m_pTable = this;

	// Passing this special invalid symbol makes the comparison function
	// use the string passed in the context
	UtlSymId_t idx = m_Lookup.Find( INVALID_STRING_INDEX );
	return CUtlSymbol( idx );
}


//-----------------------------------------------------------------------------
// Finds and/or creates a symbol based on the string
//-----------------------------------------------------------------------------

CUtlSymbol CUtlSymbolTable::AddString( char const* pString )
{
	if (!pString)
		return CUtlSymbol( UTL_INVAL_SYMBOL );

	CUtlSymbol id = Find( pString );

	if (id.IsValid())
		return id;

	int len = strlen(pString) + 1;
	int stridx = m_Strings.AddMultipleToTail( len );
	memcpy( &m_Strings[stridx], pString, len * sizeof(char) );
	UtlSymId_t idx = m_Lookup.Insert( stridx );
	return CUtlSymbol( idx );
}


//-----------------------------------------------------------------------------
// Look up the string associated with a particular symbol
//-----------------------------------------------------------------------------

char const* CUtlSymbolTable::String( CUtlSymbol id ) const
{
	if (!id.IsValid())
		return "";

	Assert( m_Lookup.IsValidIndex((UtlSymId_t)id) );
	return &m_Strings[m_Lookup[id]];
}


//-----------------------------------------------------------------------------
// Remove all symbols in the table.
//-----------------------------------------------------------------------------

void CUtlSymbolTable::RemoveAll()
{
	m_Lookup.RemoveAll();
	m_Strings.RemoveAll();
}
