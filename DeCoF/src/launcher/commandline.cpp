//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "icommandline.h"

//-----------------------------------------------------------------------------
// Purpose: Implements ICommandLine
//-----------------------------------------------------------------------------
class CCommandLine : public ICommandLine
{
public:
	// Construction
						CCommandLine( void );
	virtual				~CCommandLine( void );

	// Implements ICommandLine
	virtual void		CreateCmdLine( const char *commandline );
	virtual const char	*GetCmdLine( void );
	virtual char		*CheckParm( const char *psz, char **ppszValue = 0 );

	virtual void		RemoveParm( const char *pszParm );
	virtual void		AppendParm( const char *pszParm, const char *pszValues );

	virtual void		SetParm( const char *pszParm, const char *pszValues );
	virtual void		SetParm( const char *pszParm, int iValue );

private:
	// Copy of actual command line
	char				*m_pszCmdLine;
};

static CCommandLine g_CmdLine;
ICommandLine *cmdline = ( ICommandLine * )&g_CmdLine;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CCommandLine::CCommandLine( void )
{
	m_pszCmdLine = NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CCommandLine::~CCommandLine( void )
{
	delete[] m_pszCmdLine;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *src -
// Output : char
//-----------------------------------------------------------------------------
static char *CopyString_guess( const char *src )
{
	if ( !src )
		return NULL;

	char *out = new char[ strlen( src ) + 1 ];
	strcpy( out, src );
	return out;
}

//-----------------------------------------------------------------------------
// Purpose: Create a command line from the passed in string
//  Note that if you pass in a @filename, then the routine will read settings from a file instead of the command line
//-----------------------------------------------------------------------------
void CCommandLine::CreateCmdLine( const char *commandline )
{
	delete[] m_pszCmdLine;

	char szFull[ 4096 ];

	char *pOrig = CopyString_guess( commandline );
	char *pSrc, *pDst;
	FILE *fp;

	pDst = szFull;
	pSrc = pOrig;

	bool allowAtSign = true;

	while ( *pSrc )
	{
		if ( allowAtSign && *pSrc == '@' )
		{
			char szFileName[ _MAX_PATH ];
			char *pOut;

			pSrc++;
			pOut = szFileName;
			while ( *pSrc && *pSrc != ' ' )
			{
				*pOut++ = *pSrc++;
			}

			*pOut = '\0';

			// Skip the space after the file name
			if ( *pSrc )
				pSrc++;

			fp = fopen( szFileName, "r" );
			if ( fp )
			{
				char c;

				// Copy the contents of the file, turning line ends into spaces
				while ( ( c = (char)fgetc( fp ) ) != EOF )
				{
					if ( c == '\n' )
						c = ' ';

					*pDst++ = c;
				}

				*pDst++ = ' ';

				fclose( fp );
			}
			else
			{
				printf( "Parameter file '%s' not found, skipping...", szFileName );
			}
		}
		else
		{
			allowAtSign = isspace( *pSrc ) != 0;
			*pDst++ = *pSrc++;
		}
	}

	*pDst = '\0';

	delete[] pOrig;

	m_pszCmdLine = CopyString_guess( szFull );
}

//-----------------------------------------------------------------------------
// Purpose: Remove specified string ( and any args attached to it ) from command line
// Input  : *pszParm -
//-----------------------------------------------------------------------------
void CCommandLine::RemoveParm( const char *pszParm )
{
	if ( !m_pszCmdLine )
		return;

	if ( !pszParm || !*pszParm )
		return;

	// Search for first occurrence of pszParm
	char *p, *found;
	char *pnextparam;
	int n;
	int curlen;

	while ( 1 )
	{
		p = m_pszCmdLine;
		curlen = strlen( p );

		found = strstr( p, pszParm );
		if ( !found )
			break;

		pnextparam = found + 1;
		while ( pnextparam && *pnextparam && ( *pnextparam != '-' ) && ( *pnextparam != '+' ) )
			pnextparam++;

		if ( pnextparam && *pnextparam )
		{
			// We are either at the end of the string, or at the next param.  Just chop out the current param.
			n = curlen - ( pnextparam - p ); // # of characters after this param.

			memcpy( found, pnextparam, n );

			found[ n ] = '\0';
		}
		else
		{
			// Clear out rest of string.
			n = pnextparam - found;
			memset( found, 0, n );
		}
	}

	// Strip and trailing ' ' characters left over.
	while ( m_pszCmdLine[ strlen( m_pszCmdLine ) - 1 ] == ' ' )
	{
		m_pszCmdLine[ strlen( m_pszCmdLine ) - 1 ] = '\0';
	}
}

//-----------------------------------------------------------------------------
// Purpose: Append parameter and argument values to command line
// Input  : *pszParm -
//			*pszValues -
//-----------------------------------------------------------------------------
void CCommandLine::AppendParm( const char *pszParm, const char *pszValues )
{
	int nNewLength = 0;
	char *pCmdString;

	nNewLength = strlen( pszParm );            // Parameter.
	if ( pszValues )
		nNewLength += strlen( pszValues ) + 1;  // Values + leading space character.
	nNewLength++; // Terminal 0;

	if ( !m_pszCmdLine )
	{
		m_pszCmdLine = new char[ nNewLength ];
		strcpy( m_pszCmdLine, pszParm );
		if ( pszValues )
		{
			strcat( m_pszCmdLine, " " );
			strcat( m_pszCmdLine, pszValues );
		}

		return;
	}

	// Remove any remnants from the current Cmd Line.
	RemoveParm( pszParm );

	nNewLength += strlen( m_pszCmdLine ) + 1 + 1;

	pCmdString = new char[ nNewLength ];
	memset( pCmdString, 0, nNewLength );

	strcpy( pCmdString, m_pszCmdLine ); // Copy old command line.
	strcat( pCmdString, " " ); // Put in a space
	strcat( pCmdString, pszParm );
	if ( pszValues )
	{
		strcat( pCmdString, " " );
		strcat( pCmdString, pszValues );
	}

	// Kill off the old one
	delete[] m_pszCmdLine;

	// Point at the new command line.
	m_pszCmdLine = pCmdString;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pszParm -
//			*pszValues -
//-----------------------------------------------------------------------------
void CCommandLine::SetParm( const char *pszParm, const char *pszValues )
{
	RemoveParm( pszParm );
	AppendParm( pszParm, pszValues );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pszParm -
//			iValue -
//-----------------------------------------------------------------------------
void CCommandLine::SetParm( const char *pszParm, int iValue )
{
	char buf[ 64 ];
	_snprintf( buf, sizeof( buf ), "%d", iValue );
	SetParm( pszParm, buf );
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : const char
//-----------------------------------------------------------------------------
const char *CCommandLine::GetCmdLine( void )
{
	return m_pszCmdLine;
}

//-----------------------------------------------------------------------------
// Purpose: Search for the parameter in the current commandline
// Input  : *psz -
//			**ppszValue -
// Output : char
//-----------------------------------------------------------------------------
char *CCommandLine::CheckParm( const char *psz, char **ppszValue )
{
	static char sz[ 128 ];
	char *pret;

	if ( !m_pszCmdLine )
		return NULL;

	pret = strstr( m_pszCmdLine, psz );

	// should we return a pointer to the value?
	if ( pret && ppszValue )
	{
		char *p1 = pret;
		*ppszValue = NULL;

		while ( *p1 && ( *p1 != ' ' ) )
			p1++;

		if ( p1 )
		{
			char *p2 = ++p1;
			int i;

			for ( i = 0; i < 128; i++ )
			{
				if ( !*p2 || ( *p2 == ' ' ) )
					break;
				sz[ i ] = *p2++;
			}

			sz[ i ] = '\0';
			*ppszValue = sz;
		}
	}

	return pret;
}
