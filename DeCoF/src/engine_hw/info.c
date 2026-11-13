// info.c -- key\value info strings for the user and server info

#include "quakedef.h"
#include "info.h"

#include <ctype.h>

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
const char *Info_ValueForKey( const char *s, const char *key )
{
	char	pkey[MAX_KV_LEN];
	static	char value[MAX_INFO_VALUES][MAX_KV_LEN];	// use two buffers so compares
														// work without stomping on each other
	static	int	valueindex;
	char	*o;

	valueindex = ( valueindex + 1 ) % MAX_INFO_VALUES;
	if ( *s == '\\' )
		s++;

	while ( 1 )
	{
		int nCount = 0;

		o = pkey;
		while ( *s != '\\' && nCount < sizeof( pkey ) - 1 )
		{
			if ( !*s )
				return "";
			*o++ = *s++;
			nCount++;
		}
		*o = 0;
		s++;

		nCount = 0;
		o = value[valueindex];
		while ( *s != '\\' && *s && nCount < sizeof( value[valueindex] ) - 1 )
		{
			if ( !*s )
				return "";
			*o++ = *s++;
			nCount++;
		}
		*o = 0;

		if ( !Q_strcmp( key, pkey ) )
			return value[valueindex];

		if ( !*s )
			return "";
		s++;
	}
}

void Info_RemoveKey( char *s, const char *key )
{
	char	*start;
	char	pkey[MAX_KV_LEN];
	char	value[MAX_KV_LEN];
	char	*o;

	int		cmpsize = min( strlen( key ), MAX_KV_LEN - 1 );

	if ( Q_strstr( key, "\\" ) )
	{
		Con_Printf( "Can't use a key with a \\\n" );
		return;
	}

	while ( 1 )
	{
		int nCount;

		start = s;
		if ( *s == '\\' )
			s++;
		nCount = 0;
		o = pkey;
		while ( *s != '\\' && nCount < sizeof( pkey ) - 1 )
		{
			if ( !*s )
				return;
			*o++ = *s++;
			nCount++;
		}
		*o = 0;
		s++;

		nCount = 0;
		o = value;
		while ( *s != '\\' && *s && nCount < sizeof( value ) - 1 )
		{
			if ( !*s )
				return;
			*o++ = *s++;
			nCount++;
		}
		*o = 0;

		if ( !Q_strncmp( key, pkey, cmpsize ) )
		{
			Q_strcpy( start, s );	// remove this part
			return;
		}

		if ( !*s )
			return;
	}
}

void Info_RemovePrefixedKeys( char *start, char prefix )
{
	char	*s;
	char	pkey[MAX_KV_LEN];
	char	value[MAX_KV_LEN];
	char	*o;

	s = start;

	while ( 1 )
	{
		int nCount;

		if ( *s == '\\' )
			s++;
		nCount = 0;
		o = pkey;
		while ( *s != '\\' && nCount < sizeof( pkey ) - 1 )
		{
			if ( !*s )
				return;
			*o++ = *s++;
			nCount++;
		}
		*o = 0;
		s++;

		nCount = 0;
		o = value;
		while ( *s != '\\' && *s && nCount < sizeof( value ) - 1 )
		{
			if ( !*s )
				return;
			*o++ = *s++;
			nCount++;
		}
		*o = 0;

		if ( pkey[0] == prefix )
		{
			Info_RemoveKey( start, pkey );
			s = start;
		}

		if ( !*s )
			return;
	}
}

qboolean Info_IsKeyImportant( const char *key )
{
	if ( key[0] == '*' )
		return TRUE;
	if ( !Q_strcmp( key, "name" ) )
		return TRUE;
	if ( !Q_strcmp( key, "model" ) )
		return TRUE;
	if ( !Q_strcmp( key, "rate" ) )
		return TRUE;
	if ( !Q_strcmp( key, "topcolor" ) )
		return TRUE;
	if ( !Q_strcmp( key, "bottomcolor" ) )
		return TRUE;
	if ( !Q_strcmp( key, "cl_updaterate" ) )
		return TRUE;
	if ( !Q_strcmp( key, "cl_lw" ) )
		return TRUE;
	if ( !Q_strcmp( key, "cl_lc" ) )
		return TRUE;
	if ( !Q_strcmp( key, "*hltv" ) )
		return TRUE;
	if ( !Q_strcmp( key, "*sid" ) )
		return TRUE;

	return FALSE;
}

char *Info_FindLargestKey( char *s, int maxsize )
{
	char	key[MAX_KV_LEN];
	char	value[MAX_KV_LEN];
	char	*o;
	static	char largest_key[MAX_KV_LEN];
	int		largest_size = 0;

	largest_key[0] = 0;

	if ( *s == '\\' )
		s++;

	while ( *s )
	{
		int nCount;
		int size;

		size = 0;
		nCount = 0;
		o = key;
		while ( *s && *s != '\\' && nCount < sizeof( key ) - 1 )
		{
			*o++ = *s++;
			nCount++;
		}
		*o = 0;

		size = Q_strlen( key );

		if ( !*s )
			return largest_key;

		nCount = 0;
		o = value;
		s++;
		while ( *s && *s != '\\' && nCount < sizeof( value ) - 1 )
		{
			*o++ = *s++;
			nCount++;
		}
		*o = 0;

		if ( *s )
			s++;

		size += Q_strlen( value );

		if ( size > largest_size && !Info_IsKeyImportant( key ) )
		{
			largest_size = size;
			Q_strncpy( largest_key, key, sizeof( largest_key ) - 1 );
			largest_key[sizeof( largest_key ) - 1] = 0;
		}
	}

	return largest_key;
}

void Info_SetValueForStarKey( char *s, const char *key, const char *value, int maxsize )
{
	char	newVal[1024], *v;
	int		c;

	if ( Q_strstr( key, "\\" ) || Q_strstr( value, "\\" ) )
	{
		Con_Printf( "Can't use keys or values with a \\\n" );
		return;
	}

	if ( Q_strstr( key, ".." ) || Q_strstr( value, ".." ) )
		return;

	if ( Q_strstr( key, "\"" ) || Q_strstr( value, "\"" ) )
	{
		Con_Printf( "Can't use keys or values with a \"\n" );
		return;
	}

	if ( Q_strlen( key ) <= 0 || Q_strlen( key ) > MAX_KV_LEN - 1 || Q_strlen( value ) > MAX_KV_LEN - 1 )
	{
		Con_Printf( "Keys and values must be < %i characters and > 0.\n", MAX_KV_LEN );
		return;
	}

	Info_RemoveKey( s, key );

	if ( !value || !Q_strlen( value ) )
		return;

	_snprintf( newVal, sizeof( newVal ), "\\%s\\%s", key, value );

	if ( Q_strlen( newVal ) + Q_strlen( s ) >= maxsize )
	{
		// only an important key may push the others out
		if ( Info_IsKeyImportant( key ) )
		{
			char *largekey;

			// remove unimportant keys until there is room
			do
			{
				largekey = Info_FindLargestKey( s, maxsize );
				Info_RemoveKey( s, largekey );
			} while ( Q_strlen( newVal ) + Q_strlen( s ) >= maxsize && *largekey );

			if ( !*largekey )
			{
				Con_Printf( "Info string length exceeded\n" );
				return;
			}
		}
		else
		{
			Con_Printf( "Info string length exceeded\n" );
			return;
		}
	}

	// only copy ascii values
	s += Q_strlen( s );
	v = newVal;
	while ( *v )
	{
		c = (unsigned char)*v++;
		if ( c < 32 || c > 126 )
			continue;

		if ( !Q_stricmp( key, "team" ) )
			c = tolower( c );
		*s++ = c;
	}
	*s = 0;
}

void Info_SetValueForKey( char *s, const char *key, const char *value, int maxsize )
{
	if ( key[0] == '*' )
	{
		Con_Printf( "Can't set * keys\n" );
		return;
	}

	Info_SetValueForStarKey( s, key, value, maxsize );
}

void Info_Print( const char *s )
{
	char	key[MAX_KV_LEN];
	char	value[MAX_KV_LEN];
	char	*o;
	int		l;

	if ( *s == '\\' )
		s++;

	while ( *s )
	{
		int nCount = 0;

		o = key;
		while ( *s && *s != '\\' && nCount < sizeof( key ) - 1 )
		{
			*o++ = *s++;
			nCount++;
		}

		l = o - key;
		if ( l < 20 )
		{
			Q_memset( o, ' ', 20 - l );
			key[20] = 0;
		}
		else
			*o = 0;
		Con_Printf( "%s", key );

		if ( !*s )
		{
			Con_Printf( "MISSING VALUE\n" );
			return;
		}

		nCount = 0;
		o = value;
		s++;
		while ( *s && *s != '\\' && nCount < sizeof( value ) - 1 )
		{
			*o++ = *s++;
			nCount++;
		}
		*o = 0;

		if ( *s )
			s++;
		Con_Printf( "%s\n", value );
	}
}

int Info_IsValid( const char *s )
{
	char	key[MAX_KV_LEN];
	char	value[MAX_KV_LEN];
	char	*o;

	if ( *s == '\\' )
		s++;

	while ( *s )
	{
		int nCount = 0;

		o = key;
		while ( *s && *s != '\\' )
		{
			if ( nCount >= sizeof( key ) - 1 )
				return FALSE;
			*o++ = *s++;
			nCount++;
		}
		*o = 0;

		if ( !*s )
			return FALSE;

		nCount = 0;
		o = value;
		s++;
		while ( *s && *s != '\\' )
		{
			if ( nCount >= sizeof( value ) - 1 )
				return FALSE;
			*o++ = *s++;
			nCount++;
		}
		*o = 0;

		if ( !Q_strlen( value ) )
			return FALSE;

		if ( *s )
			s++;
	}

	return TRUE;
}
