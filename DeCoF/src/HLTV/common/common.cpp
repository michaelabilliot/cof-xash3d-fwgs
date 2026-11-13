//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: String and file name helpers shared by the HLTV proxy code
//
// $NoKeywords: $
//=============================================================================

#include <ctype.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "types.h"

char com_token[1024];
bool s_com_token_unget = false;

//-----------------------------------------------------------------------------
// Purpose: malloc that clears the memory
//-----------------------------------------------------------------------------
void *Mem_ZeroMalloc( unsigned int size )
{
	void *p = malloc( size );
	if ( p )
	{
		memset( p, 0, size );
	}
	return p;
}

//-----------------------------------------------------------------------------
// Purpose: Days since Oct 24 1996, from the compile date
//-----------------------------------------------------------------------------
int COM_BuildNumber( void )
{
	int m = 0;
	int d = 0;
	int y = 0;
	static int b = 0;

	if ( b != 0 )
		return b;

	char *date = "Dec  2 2012";	// __DATE__ of the retail build of this file
	char *mon[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	char mond[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	for ( m = 0; m < 11; m++ )
	{
		if ( !strncmp( &date[0], mon[m], 3 ) )
			break;
		d += mond[m];
	}

	d += atoi( &date[4] ) - 1;
	y = atoi( &date[7] ) - 1900;
	b = d + (int)( ( y - 1 ) * 365.25 );

	if ( ( ( y % 4 ) == 0 ) && m > 1 )
	{
		b += 1;
	}

	b -= 34995; // Oct 24 1996
	return b;
}

//-----------------------------------------------------------------------------
// Purpose: Current time as YYMMDDHHMM
//-----------------------------------------------------------------------------
char *COM_TimeString( void )
{
	// output format is 'YYMMDDHHMM' (10 chars)
	static char timedate[16];
	time_t ltime;
	struct tm *now;

	time( &ltime );
	now = localtime( &ltime );
	strftime( timedate, sizeof( timedate ), "%y%m%d%H%M", now );

	return timedate;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the file name part of a path
//-----------------------------------------------------------------------------
char *COM_SkipPath( char *pathname )
{
	char *last = pathname;
	while ( *pathname )
	{
		if ( *pathname == '\\' || *pathname == '/' )
			last = pathname + 1;
		pathname++;
	}
	return last;
}

//-----------------------------------------------------------------------------
// Purpose: Extracts the base name of a file (no path, no extension)
//-----------------------------------------------------------------------------
void COM_FileBase( char *in, char *out )
{
	int len, start, end;

	len = strlen( in );

	// scan backward for '.'
	end = len - 1;
	while ( end && in[end] != '.' && in[end] != '/' && in[end] != '\\' )
		end--;

	if ( in[end] != '.' )	// no '.', copy to end
		end = len - 1;
	else
		end--;				// Found ',', copy to left of '.'

	// Scan backward for '/'
	start = len - 1;
	while ( start >= 0 && in[start] != '/' && in[start] != '\\' )
		start--;

	if ( start < 0 || ( in[start] != '/' && in[start] != '\\' ) )
		start = 0;
	else
		start++;

	// Length of new sting
	len = end - start + 1;

	// Copy partial string
	strncpy( out, &in[start], len );
	// Terminate it
	out[len] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: The next COM_Parse returns the same token again
//-----------------------------------------------------------------------------
void COM_UngetToken( void )
{
	s_com_token_unget = true;
}

//-----------------------------------------------------------------------------
// Purpose: Parse a token out of a string
//-----------------------------------------------------------------------------
char *COM_Parse( char *data )
{
	int c;
	int len;

	if ( s_com_token_unget )
	{
		s_com_token_unget = false;
		return data;
	}

	len = 0;
	com_token[0] = 0;

	if ( !data )
	{
		return NULL;
	}

	// skip whitespace
skipwhite:
	while ( ( c = *data ) <= ' ' )
	{
		if ( c == 0 )
		{
			return NULL;	// end of file;
		}
		data++;
	}

	// skip // comments
	if ( c == '/' && data[1] == '/' )
	{
		while ( *data && *data != '\n' )
			data++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if ( c == '\"' )
	{
		data++;
		while ( 1 )
		{
			c = *data++;
			if ( c == '\"' || !c )
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

	// parse single characters
	if ( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':' || c == ',' )
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data + 1;
	}

	// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if ( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':' || c == ',' )
			break;
	} while ( c > 32 );

	com_token[len] = 0;
	return data;
}

//-----------------------------------------------------------------------------
// Purpose: printf into a static buffer
//-----------------------------------------------------------------------------
char *COM_VarArgs( char *format, ... )
{
	va_list argptr;
	static char string[1024];

	va_start( argptr, format );
	_vsnprintf( string, sizeof( string ), format, argptr );
	va_end( argptr );

	return string;
}

//-----------------------------------------------------------------------------
// Purpose: Value of a hex digit
//-----------------------------------------------------------------------------
unsigned char COM_Nibble( char c )
{
	if ( ( c >= '0' ) && ( c <= '9' ) )
	{
		return (unsigned char)( c - '0' );
	}

	if ( ( c >= 'A' ) && ( c <= 'F' ) )
	{
		return (unsigned char)( c - 'A' + 0x0a );
	}

	if ( ( c >= 'a' ) && ( c <= 'f' ) )
	{
		return (unsigned char)( c - 'a' + 0x0a );
	}

	return '0';
}

//-----------------------------------------------------------------------------
// Purpose: Converts a hex string to bytes
//-----------------------------------------------------------------------------
void COM_HexConvert( const char *pszInput, int nInputLength, unsigned char *pOutput )
{
	unsigned char *p;
	int i;
	const char *pIn;

	p = pOutput;
	for ( i = 0; i < nInputLength; i += 2 )
	{
		pIn = &pszInput[i];

		*p = COM_Nibble( pIn[0] ) << 4 | COM_Nibble( pIn[1] );

		p++;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Prints bytes as a hex string into a static buffer
//-----------------------------------------------------------------------------
char *COM_BinPrintf( unsigned char *buf, int nLen )
{
	static char szReturn[4096];
	unsigned char c;
	char szChunk[10];
	int i;

	memset( szReturn, 0, sizeof( szReturn ) );

	for ( i = 0; i < nLen; i++ )
	{
		c = (unsigned char)buf[i];
		_snprintf( szChunk, sizeof( szChunk ), "%02x", c );
		strncat( szReturn, szChunk, sizeof( szReturn ) - strlen( szReturn ) - 1 );
	}
	return szReturn;
}

//-----------------------------------------------------------------------------
// Purpose: Formats seconds as mm:ss or hh:mm:ss
//-----------------------------------------------------------------------------
char *COM_FormatTime( float seconds )
{
	static char time[32];
	int hours;

	if ( seconds < 3600 )
	{
		_snprintf( time, sizeof( time ), "%02i:%02i", (int)( seconds / 60 ), ( (int)seconds ) % 60 );
	}
	else
	{
		hours = (int)seconds / 3600;
		seconds -= hours * 3600;
		_snprintf( time, sizeof( time ), "%02i:%2i:%02i", hours, (int)( seconds / 60 ), ( (int)seconds ) % 60 );
	}

	return time;
}

//-----------------------------------------------------------------------------
// Purpose: Replaces unprintable and format characters with spaces
//-----------------------------------------------------------------------------
void COM_RemoveEvilChars( char *string )
{
	char *c;

	for ( c = string; c && *c; c++ )
	{
		// Replace it with a space
		if ( *c < ' ' || *c > '~' || *c == '%' || *c == ';' )
			*c = ' ';
	}
}

#ifdef _WIN32
//-----------------------------------------------------------------------------
// Purpose: Returns the directory of the executable
//-----------------------------------------------------------------------------
char *COM_GetBaseDir_guess( void )
{
	static char basedir_guess[ MAX_PATH ];
	char *pBuffer = NULL;

	basedir_guess[0] = 0;

	if ( GetModuleFileName( NULL, basedir_guess, sizeof( basedir_guess ) ) )
	{
		pBuffer = strrchr( basedir_guess, '\\' );
		if ( pBuffer && *pBuffer )
		{
			pBuffer[1] = 0;
		}

		int j = strlen( basedir_guess );
		if ( j > 0 )
		{
			if ( basedir_guess[j - 1] == '\\' || basedir_guess[j - 1] == '/' )
			{
				basedir_guess[j - 1] = 0;
			}
		}
	}

	return basedir_guess;
}

//-----------------------------------------------------------------------------
// Purpose: Turns forward slashes into backslashes
//-----------------------------------------------------------------------------
void COM_FixSlashes_guess( char *pname )
{
	while ( *pname )
	{
		if ( *pname == '/' )
			*pname = '\\';

		pname++;
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Compares file names, ignoring case and the slash direction
//-----------------------------------------------------------------------------
int COM_FileNameCmp( const char *file1, const char *file2 )
{
	do
	{
		if ( *file1 == '/' && *file2 == '\\' )
			continue;

		if ( *file2 == '/' && *file1 == '\\' )
			continue;

		if ( tolower( *file1 ) != tolower( *file2 ) )
			return -1;

		if ( !*file1 )
			return 0;

	} while ( *file1++ && *file2++ );

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool COM_IsWhiteSpace( char space )
{
	return ( space == ' ' || space == '\t' || space == '\r' || space == '\n' );
}

//-----------------------------------------------------------------------------
// Purpose: Copies source to dest without the leading and trailing white space
//-----------------------------------------------------------------------------
void COM_TrimSpace( const char *source, char *dest )
{
	int start, end, length;

	start = 0;
	end = strlen( source );

	while ( source[start] && COM_IsWhiteSpace( source[start] ) )
		start++;

	end--;
	while ( end > 0 && COM_IsWhiteSpace( source[end] ) )
		end--;

	end++;
	length = end - start;

	if ( length > 0 )
		strncpy( dest, &source[start], length );
	else
		length = 0;

	// terminate the dest string
	dest[length] = 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COM_UnpackRGB( int &r, int &g, int &b, uint32 ulRGB )
{
	r = ( ulRGB & 0xFF0000 ) >> 16;
	g = ( ulRGB & 0xFF00 ) >> 8;
	b = ( ulRGB & 0xFF );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
unsigned int COM_PackRGB( int r, int g, int b )
{
	return ( ( b & 0xFF ) | ( g & 0xFF ) << 8 | ( r & 0xFF ) << 16 );
}
