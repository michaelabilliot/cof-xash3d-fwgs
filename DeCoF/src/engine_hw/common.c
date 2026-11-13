// common.c -- misc functions used in client and server

#include "quakedef.h"
#include "mathlib.h"
#include "usercmd.h"
#include "cvardef.h"
#include "bspfile.h"
#include "APIProxy.h"
#include "zone.h"
#include "mem.h"
#include "cvar.h"
#include "delta.h"
#include "net.h"
#include "info.h"
#include "server.h"

#include <ctype.h>
#include <stdarg.h>

typedef struct bf_read_s
{
	int			nMsgReadCount;	// was msg_readcount
	sizebuf_t	*pbuf;
	int			nBitFieldReadStartByte;
	int			nBytesRead;
	int			nCurInputBit;
	byte		*pInByte;
} bf_read_t;

typedef struct bf_write_s
{
	int			nCurOutputBit;
	byte		*pOutByte;
	sizebuf_t	*pbuf;
} bf_write_t;

typedef struct
{
	char			chunkID[4];
	long			chunkSize;
	short			wFormatTag;
	unsigned short	wChannels;
	unsigned long	dwSamplesPerSec;
	unsigned long	dwAvgBytesPerSec;
	unsigned short	wBlockAlign;
	unsigned short	wBitsPerSample;
} FormatChunk;

#define WAVE_HEADER_LENGTH	128

char		*gpszAppName = "Cry of Fear";

char		gpszVersionString[32];
char		gpszProductString[32];

static char	*largv[MAX_NUM_ARGVS + NUM_SAFE_ARGVS + 1];
static char	*argvdummy = " ";

static char	*safeargvs[NUM_SAFE_ARGVS] =
	{ "-stdvid", "-nolan", "-nosound", "-nocdaudio", "-nojoy", "-nomouse", "-dibonly" };

cvar_t		com_filewarning = { "com_filewarning", "0" };

qboolean	com_ignorecolons = false;
static qboolean	s_com_token_unget;

char		com_token[COM_TOKEN_LEN];

int			com_argc;
char		**com_argv;

char		com_cmdline[COM_MAX_CMD_LINE];

char		serverinfo[MAX_INFO_STRING];

char		gszDisconnectReason[256];
char		gszExtendedDisconnectReason[256];
qboolean	gfExtendedError = false;

/*
==============
Info_Serverinfo

==============
*/
char *Info_Serverinfo( void )
{
	return serverinfo;
}

/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

void Q_memset( void *dest, int fill, int count )
{
	int		i;

	if ( ( ( (long)dest | count ) & 3 ) == 0 )
	{
		count >>= 2;
		fill = fill | ( fill << 8 ) | ( fill << 16 ) | ( fill << 24 );
		for ( i = 0; i < count; i++ )
			( (int *)dest )[i] = fill;
	}
	else
		for ( i = 0; i < count; i++ )
			( (byte *)dest )[i] = fill;
}

void Q_memcpy( void *dest, const void *src, int count )
{
	int		i;

	if ( ( ( (long)dest | (long)src | count ) & 3 ) == 0 )
	{
		count >>= 2;
		for ( i = 0; i < count; i++ )
			( (int *)dest )[i] = ( (int *)src )[i];
	}
	else
		for ( i = 0; i < count; i++ )
			( (byte *)dest )[i] = ( (byte *)src )[i];
}

int Q_memcmp( void *m1, void *m2, int count )
{
	int		byteLoops = count & 3;
	int		numLoops = count >> 2;
	int		i;

	for ( i = 0; i < numLoops; i++ )
	{
		if ( ( (int *)m1 )[i] != ( (int *)m2 )[i] )
			return -1;
	}

	// the tail compares the first bytes again, not the last ones
	for ( i = 0; i < byteLoops; i++ )
	{
		if ( ( (char *)m1 )[i] != ( (char *)m2 )[i] )
			return -1;
	}

	return 0;
}

void Q_strcpy( char *dest, const char *src )
{
	while ( dest && src && *src )
	{
		*dest++ = *src++;
	}
	*dest++ = 0;
}

void Q_strncpy( char *dest, const char *src, int count )
{
	while ( src && *src && count-- )
	{
		*dest++ = *src++;
	}
	if ( count > 0 )
		*dest++ = 0;
}

int Q_strlen( const char *str )
{
	int		count;

	if ( !str )
		return 0;

	count = 0;
	while ( str[count] )
		count++;

	return count;
}

char *Q_strrchr( char *s, char c )
{
	int		len = Q_strlen( s );

	s += len;
	while ( len-- )
		if ( *--s == c )
			return s;
	return 0;
}

void Q_strcat( char *dest, char *src )
{
	dest += Q_strlen( dest );
	Q_strcpy( dest, src );
}

int Q_strcmp( const char *s1, const char *s2 )
{
	while ( 1 )
	{
		if ( *s1 != *s2 )
			return -1;		// strings not equal
		if ( !*s1 )
			return 0;		// strings are equal
		s1++;
		s2++;
	}

	return -1;
}

int Q_strncmp( const char *s1, const char *s2, int count )
{
	while ( 1 )
	{
		if ( !count-- )
			return 0;
		if ( *s1 != *s2 )
			return -1;		// strings not equal
		if ( !*s1 )
			return 0;		// strings are equal
		s1++;
		s2++;
	}

	return -1;
}

int Q_strncasecmp( const char *s1, const char *s2, int n )
{
	int		c1, c2;

	while ( 1 )
	{
		c1 = *s1++;
		c2 = *s2++;

		if ( !n-- )
			return 0;		// strings are equal until end point

		if ( c1 != c2 )
		{
			if ( c1 >= 'a' && c1 <= 'z' )
				c1 -= ( 'a' - 'A' );
			if ( c2 >= 'a' && c2 <= 'z' )
				c2 -= ( 'a' - 'A' );
			if ( c1 != c2 )
				return -1;	// strings not equal
		}
		if ( !c1 )
			return 0;		// strings are equal
	}

	return -1;
}

int Q_strcasecmp( const char *s1, const char *s2 )
{
	return Q_strncasecmp( s1, s2, 99999 );
}

int Q_stricmp( const char *s1, const char *s2 )
{
	return Q_strcasecmp( s1, s2 );
}

int Q_strnicmp( const char *s1, const char *s2, int n )
{
	return Q_strncasecmp( s1, s2, n );
}

int Q_atoi( const char *str )
{
	int		val;
	int		sign;
	int		c;

	if ( *str == '-' )
	{
		sign = -1;
		str++;
	}
	else
		sign = 1;

	val = 0;

//
// check for hex
//
	if ( str[0] == '0' && ( str[1] == 'x' || str[1] == 'X' ) )
	{
		str += 2;
		while ( 1 )
		{
			c = *str++;
			if ( c >= '0' && c <= '9' )
				val = ( val << 4 ) + c - '0';
			else if ( c >= 'a' && c <= 'f' )
				val = ( val << 4 ) + c - 'a' + 10;
			else if ( c >= 'A' && c <= 'F' )
				val = ( val << 4 ) + c - 'A' + 10;
			else
				return val * sign;
		}
	}

//
// check for character
//
	if ( str[0] == '\'' )
	{
		return sign * str[1];
	}

//
// assume decimal
//
	while ( 1 )
	{
		c = *str++;
		if ( c < '0' || c > '9' )
			return val * sign;
		val = val * 10 + c - '0';
	}

	return 0;
}

float Q_atof( const char *str )
{
	double	val;
	int		sign;
	int		c;
	int		decimal, total;

	if ( *str == '-' )
	{
		sign = -1;
		str++;
	}
	else
		sign = 1;

	val = 0;

//
// check for hex
//
	if ( str[0] == '0' && ( str[1] == 'x' || str[1] == 'X' ) )
	{
		str += 2;
		while ( 1 )
		{
			c = *str++;
			if ( c >= '0' && c <= '9' )
				val = ( val * 16 ) + c - '0';
			else if ( c >= 'a' && c <= 'f' )
				val = ( val * 16 ) + c - 'a' + 10;
			else if ( c >= 'A' && c <= 'F' )
				val = ( val * 16 ) + c - 'A' + 10;
			else
				return val * sign;
		}
	}

//
// check for character
//
	if ( str[0] == '\'' )
	{
		return sign * str[1];
	}

//
// assume decimal
//
	decimal = -1;
	total = 0;
	while ( 1 )
	{
		c = *str++;
		if ( c == '.' )
		{
			decimal = total;
			continue;
		}
		if ( c < '0' || c > '9' )
			break;
		val = val * 10 + c - '0';
		total++;
	}

	if ( decimal == -1 )
		return val * sign;
	while ( total > decimal )
	{
		val /= 10;
		total--;
	}

	return val * sign;
}

char *Q_strlwr( char *src )
{
	char	*p;

	p = src;
	while ( *p )
	{
		*p = tolower( *p );
		p++;
	}

	return p;
}

int Q_FileNameCmp( char *file1, char *file2 )
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

char *Q_strstr( const char *s1, const char *search )
{
	return strstr( s1, search );
}

/*
==============
COM_Nibble

Returns the 4 bit nibble for a hex character
==============
*/
unsigned char COM_Nibble( char c )
{
	if ( c >= '0' && c <= '9' )
		return (unsigned char)( c - '0' );

	if ( c >= 'A' && c <= 'F' )
		return (unsigned char)( c - 'A' + 0x0a );

	if ( c >= 'a' && c <= 'f' )
		return (unsigned char)( c - 'a' + 0x0a );

	return '0';
}

/*
==============
COM_HexConvert

Converts pszInput Hex string to nInputLength/2 binary
==============
*/
void COM_HexConvert( const char *pszInput, int nInputLength, unsigned char *pOutput )
{
	unsigned char	*p;
	int				i;
	const char		*pIn;

	p = pOutput;
	for ( i = 0; i < nInputLength; i += 2 )
	{
		pIn = &pszInput[i];

		*p = COM_Nibble( pIn[0] ) << 4 | COM_Nibble( pIn[1] );
		p++;
	}
}

/*
==============
COM_BinPrintf

==============
*/
char *COM_BinPrintf( unsigned char *buf, int nLen )
{
	static char		szReturn[4096];
	unsigned char	c;
	char			szChunk[10];
	int				i;

	Q_memset( szReturn, 0, sizeof( szReturn ) );

	for ( i = 0; i < nLen; i++ )
	{
		c = buf[i];
		_snprintf( szChunk, sizeof( szChunk ), "%02x", c );
		strncat( szReturn, szChunk, sizeof( szReturn ) - strlen( szReturn ) - 1 );
	}
	return szReturn;
}

/*
==============
COM_ExplainDisconnection

==============
*/
void COM_ExplainDisconnection( qboolean bPrint, char *fmt, ... )
{
	va_list		argptr;
	static char	string[1024];

	va_start( argptr, fmt );
	_vsnprintf( string, sizeof( string ), fmt, argptr );
	va_end( argptr );

	Q_strncpy( gszDisconnectReason, string, sizeof( gszDisconnectReason ) - 1 );
	gszDisconnectReason[sizeof( gszDisconnectReason ) - 1] = 0;
	gfExtendedError = true;

	if ( bPrint && gszDisconnectReason[0] != '#' )
	{
		Con_Printf( "%s\n", gszDisconnectReason );
	}
}

/*
==============
COM_ExtendedExplainDisconnection

==============
*/
void COM_ExtendedExplainDisconnection( qboolean bPrint, char *fmt, ... )
{
	va_list		argptr;
	static char	string[1024];

	va_start( argptr, fmt );
	_vsnprintf( string, sizeof( string ), fmt, argptr );
	va_end( argptr );

	Q_strncpy( gszExtendedDisconnectReason, string, sizeof( gszExtendedDisconnectReason ) - 1 );
	gszExtendedDisconnectReason[sizeof( gszExtendedDisconnectReason ) - 1] = 0;

	if ( bPrint && gszExtendedDisconnectReason[0] != '#' )
	{
		Con_Printf( "%s\n", gszExtendedDisconnectReason );
	}
}

/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/

qboolean	bigendien;

// can't just use function pointers, or dll linkage can
// mess up when qcommon is included in multiple places
short	(*BigShort)( short l );
short	(*LittleShort)( short l );
int32	(*BigLong)( int32 l );
int32	(*LittleLong)( int32 l );
float	(*BigFloat)( float l );
float	(*LittleFloat)( float l );

short ShortSwap( short l )
{
	byte	b1, b2;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;

	return ( b1 << 8 ) + b2;
}

short ShortNoSwap( short l )
{
	return l;
}

int32 LongSwap( int32 l )
{
	byte	b1, b2, b3, b4;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;
	b3 = ( l >> 16 ) & 255;
	b4 = ( l >> 24 ) & 255;

	return ( (int)b1 << 24 ) + ( (int)b2 << 16 ) + ( (int)b3 << 8 ) + b4;
}

int32 LongNoSwap( int32 l )
{
	return l;
}

float FloatSwap( float f )
{
	union
	{
		float	f;
		byte	b[4];
	} dat1, dat2;

	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

float FloatNoSwap( float f )
{
	return f;
}

/*
==============================================================================

			MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
*/

//
// writing functions
//

void MSG_WriteChar( sizebuf_t *sb, int c )
{
	byte	*buf;

	buf = SZ_GetSpace( sb, 1 );
	buf[0] = c;
}

void MSG_WriteByte( sizebuf_t *sb, int c )
{
	byte	*buf;

	buf = SZ_GetSpace( sb, 1 );
	buf[0] = c;
}

void MSG_WriteShort( sizebuf_t *sb, int c )
{
	byte	*buf;

	buf = SZ_GetSpace( sb, 2 );
	buf[0] = c & 0xff;
	buf[1] = c >> 8;
}

void MSG_WriteWord( sizebuf_t *sb, int c )
{
	byte	*buf;

	buf = SZ_GetSpace( sb, 2 );
	buf[0] = c & 0xff;
	buf[1] = ( c >> 8 ) & 0xff;
}

void MSG_WriteLong( sizebuf_t *sb, int c )
{
	byte	*buf;

	buf = SZ_GetSpace( sb, 4 );
	buf[0] = c & 0xff;
	buf[1] = ( c >> 8 ) & 0xff;
	buf[2] = ( c >> 16 ) & 0xff;
	buf[3] = c >> 24;
}

void MSG_WriteFloat( sizebuf_t *sb, float f )
{
	union
	{
		float	f;
		int		l;
	} dat;

	dat.f = f;
	dat.l = LittleLong( dat.l );

	SZ_Write( sb, &dat.l, 4 );
}

void MSG_WriteString( sizebuf_t *sb, const char *s )
{
	if ( !s )
		SZ_Write( sb, "", 1 );
	else
		SZ_Write( sb, s, Q_strlen( s ) + 1 );
}

void MSG_WriteBuf( sizebuf_t *sb, int iSize, void *buf )
{
	if ( !buf )
		return;

	SZ_Write( sb, buf, iSize );
}

void MSG_WriteAngle( sizebuf_t *sb, float f )
{
	MSG_WriteByte( sb, (int)( f * 256 / 360 ) & 255 );
}

void MSG_WriteHiresAngle( sizebuf_t *sb, float f )
{
	MSG_WriteShort( sb, (int)( f * 65536 / 360 ) & 65535 );
}

void MSG_WriteUsercmd( sizebuf_t *buf, usercmd_t *to, usercmd_t *from )
{
	delta_t		**ppdesc;

	ppdesc = DELTA_LookupRegistration( "usercmd_t" );
	MSG_StartBitWriting( buf );
	DELTA_WriteDelta( (byte *)from, (byte *)to, true, *ppdesc, NULL );
	MSG_EndBitWriting( buf );
}

/*
==============================================================================

			BIT FIELD FUNCTIONS

==============================================================================
*/

static bf_write_t	bfwrite;
static bf_read_t	bfread;

void COM_BitOpsInit( void )
{
	Q_memset( &bfwrite, 0, sizeof( bfwrite ) );
	Q_memset( &bfread, 0, sizeof( bfread ) );
}

const unsigned int BITTABLE[] =
{
	0x00000001, 0x00000002, 0x00000004, 0x00000008,
	0x00000010, 0x00000020, 0x00000040, 0x00000080,
	0x00000100, 0x00000200, 0x00000400, 0x00000800,
	0x00001000, 0x00002000, 0x00004000, 0x00008000,
	0x00010000, 0x00020000, 0x00040000, 0x00080000,
	0x00100000, 0x00200000, 0x00400000, 0x00800000,
	0x01000000, 0x02000000, 0x04000000, 0x08000000,
	0x10000000, 0x20000000, 0x40000000, 0x80000000,
	0x00000000,
};

const unsigned int ROWBITTABLE[] =
{
	0x00000000, 0x00000001, 0x00000003, 0x00000007,
	0x0000000F, 0x0000001F, 0x0000003F, 0x0000007F,
	0x000000FF, 0x000001FF, 0x000003FF, 0x000007FF,
	0x00000FFF, 0x00001FFF, 0x00003FFF, 0x00007FFF,
	0x0000FFFF, 0x0001FFFF, 0x0003FFFF, 0x0007FFFF,
	0x000FFFFF, 0x001FFFFF, 0x003FFFFF, 0x007FFFFF,
	0x00FFFFFF, 0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF,
	0x0FFFFFFF, 0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF,
	0xFFFFFFFF,
};

const unsigned int INVBITTABLE[] =
{
	0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFB, 0xFFFFFFF7,
	0xFFFFFFEF, 0xFFFFFFDF, 0xFFFFFFBF, 0xFFFFFF7F,
	0xFFFFFEFF, 0xFFFFFDFF, 0xFFFFFBFF, 0xFFFFF7FF,
	0xFFFFEFFF, 0xFFFFDFFF, 0xFFFFBFFF, 0xFFFF7FFF,
	0xFFFEFFFF, 0xFFFDFFFF, 0xFFFBFFFF, 0xFFF7FFFF,
	0xFFEFFFFF, 0xFFDFFFFF, 0xFFBFFFFF, 0xFF7FFFFF,
	0xFEFFFFFF, 0xFDFFFFFF, 0xFBFFFFFF, 0xF7FFFFFF,
	0xEFFFFFFF, 0xDFFFFFFF, 0xBFFFFFFF, 0x7FFFFFFF,
	0xFFFFFFFF,
};

void MSG_WriteOneBit( int nValue )
{
	if ( bfwrite.nCurOutputBit >= 8 )
	{
		SZ_GetSpace( bfwrite.pbuf, 1 );
		bfwrite.nCurOutputBit = 0;
		bfwrite.pOutByte++;
	}

	if ( bfwrite.pbuf->flags & FSB_OVERFLOWED )
		return;

	if ( nValue )
	{
		*bfwrite.pOutByte |= BITTABLE[bfwrite.nCurOutputBit];
		bfwrite.nCurOutputBit++;
	}
	else
	{
		*bfwrite.pOutByte &= INVBITTABLE[bfwrite.nCurOutputBit];
		bfwrite.nCurOutputBit++;
	}
}

void MSG_StartBitWriting( sizebuf_t *buf )
{
	bfwrite.nCurOutputBit = 0;
	bfwrite.pOutByte = &buf->data[buf->cursize];
	bfwrite.pbuf = buf;
}

qboolean MSG_IsBitWriting( void )
{
	return bfwrite.pbuf != NULL;
}

void MSG_EndBitWriting( sizebuf_t *buf )
{
	int				nPadBitsNeeded = 0;
	unsigned char	cmask;
	int				nBytesAdded = 0;

	if ( bfwrite.pbuf->flags & FSB_OVERFLOWED )
		return;

	nPadBitsNeeded = 8 - bfwrite.nCurOutputBit;
	cmask = 0xff >> nPadBitsNeeded;
	*bfwrite.pOutByte &= cmask;

	SZ_GetSpace( bfwrite.pbuf, 1 );
	bfwrite.pOutByte++;

	bfwrite.nCurOutputBit = 0;
	bfwrite.pOutByte = NULL;
	bfwrite.pbuf = NULL;
}

void MSG_WriteBits( unsigned int data, int numbits )
{
	unsigned int	d;
	unsigned int	reserveBytes;

	reserveBytes = 0;

	if ( numbits < 32 )
	{
		if ( data >= (unsigned int)( 1 << numbits ) )
			data = ROWBITTABLE[numbits];
	}

	if ( bfwrite.nCurOutputBit >= 8 )
	{
		reserveBytes = 1;
		bfwrite.nCurOutputBit = 0;
		bfwrite.pOutByte++;
	}

	d = bfwrite.nCurOutputBit + numbits;

	if ( d > 32 )
	{
		SZ_GetSpace( bfwrite.pbuf, reserveBytes + 4 );
		if ( bfwrite.pbuf->flags & FSB_OVERFLOWED )
			return;

		*(unsigned int *)bfwrite.pOutByte &= ROWBITTABLE[bfwrite.nCurOutputBit];
		*(unsigned int *)bfwrite.pOutByte |= data << bfwrite.nCurOutputBit;
		data >>= 32 - bfwrite.nCurOutputBit;
		bfwrite.nCurOutputBit = d & 7;
		bfwrite.pOutByte += 4;
		*(unsigned int *)bfwrite.pOutByte = 0;
		*(unsigned int *)bfwrite.pOutByte |= data;
	}
	else
	{
		int		length;

		length = d >> 3;
		d &= 7;
		if ( !d )
			length--;

		SZ_GetSpace( bfwrite.pbuf, reserveBytes + length );
		if ( bfwrite.pbuf->flags & FSB_OVERFLOWED )
			return;

		*(unsigned int *)bfwrite.pOutByte &= ROWBITTABLE[bfwrite.nCurOutputBit];
		*(unsigned int *)bfwrite.pOutByte |= data << bfwrite.nCurOutputBit;

		if ( !d )
			bfwrite.nCurOutputBit = 8;
		else
			bfwrite.nCurOutputBit = d;

		bfwrite.pOutByte += length;
	}
}

void MSG_WriteSBits( int data, int numbits )
{
	unsigned int	uidata;

	if ( numbits < 32 )
	{
		int		sigbits = numbits - 1;
		int		maxnum = ( 1 << sigbits ) - 1;

		if ( data > maxnum )
			data = maxnum;
		else if ( data < -maxnum )
			data = -maxnum;
	}

	if ( data < 0 )
		MSG_WriteOneBit( 1 );
	else
		MSG_WriteOneBit( 0 );

	uidata = abs( data );
	MSG_WriteBits( uidata, numbits - 1 );
}

void MSG_WriteBitString( const char *p )
{
	const char	*pch;

	pch = p;
	while ( *pch )
	{
		MSG_WriteBits( *pch, 8 );
		pch++;
	}

	MSG_WriteBits( 0, 8 );
}

void MSG_WriteBitData( void *src, int length )
{
	int				i;
	unsigned char	*p;

	p = (unsigned char *)src;
	for ( i = 0; i < length; i++ )
	{
		MSG_WriteBits( *p, 8 );
		p++;
	}
}

void MSG_WriteBitAngle( float fAngle, int numbits )
{
	int				d;
	unsigned int	mask;
	unsigned int	shift;

	if ( numbits < 32 )
	{
		shift = 1 << numbits;
		mask = shift - 1;
	}
	else
	{
		Sys_Error( "Can't write bit angle with 32 bits precision\n" );
		return;
	}

	d = (int)( shift * fAngle ) / 360;
	d &= mask;

	MSG_WriteBits( d, numbits );
}

float MSG_ReadBitAngle( int numbits )
{
	float			fReturn;
	unsigned int	ui;
	float			shift;

	shift = (float)( 1 << numbits );

	ui = MSG_ReadBits( numbits );

	fReturn = (float)ui * ( 360.0 / shift );

	return fReturn;
}

int MSG_CurrentBit( void )
{
	int		nbits;

	if ( !bfread.pbuf )
		return 8 * msg_readcount;

	nbits = bfread.nCurInputBit + 8 * bfread.nBytesRead;
	return nbits;
}

qboolean MSG_IsBitReading( void )
{
	return bfread.pbuf != NULL;
}

void MSG_StartBitReading( sizebuf_t *buf )
{
	bfread.nMsgReadCount = msg_readcount;
	bfread.nBitFieldReadStartByte = msg_readcount;
	bfread.nCurInputBit = 0;
	bfread.nBytesRead = 0;
	bfread.pInByte = &buf->data[msg_readcount];
	bfread.pbuf = buf;

	bfread.nMsgReadCount++;
	if ( bfread.nMsgReadCount > buf->cursize )
		msg_badread = true;
}

void MSG_EndBitReading( sizebuf_t *buf )
{
	if ( bfread.nMsgReadCount > buf->cursize )
		msg_badread = true;

	msg_readcount = bfread.nMsgReadCount;
	bfread.nBitFieldReadStartByte = 0;
	bfread.nCurInputBit = 0;
	bfread.nBytesRead = 0;
	bfread.pInByte = NULL;
	bfread.pbuf = NULL;
}

int MSG_ReadOneBit( void )
{
	int		nValue;

	if ( msg_badread )
		return 1;

	if ( bfread.nCurInputBit >= 8 )
	{
		bfread.nMsgReadCount++;
		bfread.nCurInputBit = 0;
		bfread.nBytesRead++;
		bfread.pInByte++;
	}

	if ( bfread.nMsgReadCount > bfread.pbuf->cursize )
	{
		msg_badread = true;
		return 1;
	}

	nValue = *bfread.pInByte & BITTABLE[bfread.nCurInputBit];
	bfread.nCurInputBit++;

	return nValue != 0;
}

unsigned int MSG_ReadBits( int numbits )
{
	unsigned int	r, post, d;

	if ( msg_badread )
		return 1;

	if ( bfread.nCurInputBit >= 8 )
	{
		bfread.nMsgReadCount++;
		bfread.nCurInputBit = 0;
		bfread.nBytesRead++;
		bfread.pInByte++;
	}

	d = numbits + bfread.nCurInputBit;

	if ( d > 32 )
	{
		r = *(unsigned int *)bfread.pInByte >> bfread.nCurInputBit;
		bfread.pInByte += 4;
		post = *(unsigned int *)bfread.pInByte;
		d &= 7;
		r |= ( post & ROWBITTABLE[d] ) << ( 32 - bfread.nCurInputBit );
		bfread.nMsgReadCount += 4;
		bfread.nBytesRead += 4;
		bfread.nCurInputBit = d;
	}
	else
	{
		r = ( *(unsigned int *)bfread.pInByte >> bfread.nCurInputBit ) & ROWBITTABLE[numbits];

		if ( !( d & 7 ) )
		{
			bfread.nCurInputBit = 8;
			d = ( d >> 3 ) - 1;
		}
		else
		{
			bfread.nCurInputBit = d & 7;
			d >>= 3;
		}

		bfread.pInByte += d;
		bfread.nMsgReadCount += d;
		bfread.nBytesRead += d;
	}

	if ( bfread.nMsgReadCount > bfread.pbuf->cursize )
	{
		msg_badread = true;
		return 1;
	}

	return r;
}

unsigned int MSG_PeekBits( int numbits )
{
	unsigned int	r;
	bf_read_t		savebf;

	savebf = bfread;
	r = MSG_ReadBits( numbits );
	bfread = savebf;

	return r;
}

int MSG_ReadSBits( int numbits )
{
	int		r, nSignBit;

	nSignBit = MSG_ReadOneBit();
	r = MSG_ReadBits( numbits - 1 );

	if ( nSignBit )
		r = -r;

	return r;
}

char *MSG_ReadBitString( void )
{
	static char	buf[8192];
	char		c;
	char		*p;

	buf[0] = 0;
	p = buf;

	while ( ( c = MSG_ReadBits( 8 ) ) != 0 )
	{
		*p++ = c;
	}

	*p = 0;
	return buf;
}

int MSG_ReadBitData( void *dest, int length )
{
	int				i;
	int				readlen = 0;
	unsigned char	*p = (unsigned char *)dest;

	for ( i = 0; i < length; i++ )
	{
		*p = MSG_ReadBits( 8 );
		p++;
	}

	return length;
}

float MSG_ReadBitCoord( void )
{
	int		intval = 0, fractval = 0, signbit = 0;
	float	value = 0.0;

	intval = MSG_ReadOneBit();
	fractval = MSG_ReadOneBit();

	if ( intval || fractval )
	{
		signbit = MSG_ReadOneBit();

		if ( intval )
			intval = MSG_ReadBits( 12 );

		if ( fractval )
			fractval = MSG_ReadBits( 3 );

		value = intval + fractval * ( 1.0 / 8 );

		if ( signbit )
			value = -value;
	}

	return value;
}

void MSG_WriteBitCoord( const float f )
{
	int		signbit, intval, fractval;

	signbit = f <= -0.125;
	intval = abs( (int)f );
	fractval = abs( (int)f * 8 ) & 7;

	MSG_WriteOneBit( intval );
	MSG_WriteOneBit( fractval );

	if ( intval || fractval )
	{
		MSG_WriteOneBit( signbit );

		if ( intval )
			MSG_WriteBits( intval, 12 );

		if ( fractval )
			MSG_WriteBits( fractval, 3 );
	}
}

void MSG_ReadBitVec3Coord( vec3_t fa )
{
	int		xflag, yflag, zflag;

	xflag = MSG_ReadOneBit();
	yflag = MSG_ReadOneBit();
	zflag = MSG_ReadOneBit();

	if ( xflag )
		fa[0] = MSG_ReadBitCoord();
	if ( yflag )
		fa[1] = MSG_ReadBitCoord();
	if ( zflag )
		fa[2] = MSG_ReadBitCoord();
}

void MSG_WriteBitVec3Coord( const vec3_t fa )
{
	int		xflag, yflag, zflag;

	xflag = ( fa[0] >= 0.125 ) || ( fa[0] <= -0.125 );
	yflag = ( fa[1] >= 0.125 ) || ( fa[1] <= -0.125 );
	zflag = ( fa[2] >= 0.125 ) || ( fa[2] <= -0.125 );

	MSG_WriteOneBit( xflag );
	MSG_WriteOneBit( yflag );
	MSG_WriteOneBit( zflag );

	if ( xflag )
		MSG_WriteBitCoord( fa[0] );
	if ( yflag )
		MSG_WriteBitCoord( fa[1] );
	if ( zflag )
		MSG_WriteBitCoord( fa[2] );
}

float MSG_ReadCoord( sizebuf_t *sb )
{
	float	coord;

	coord = MSG_ReadShort() / 8.0;
	return coord;
}

void MSG_WriteCoord( sizebuf_t *sb, const float f )
{
	MSG_WriteShort( sb, (int)( f * 8 ) );
}

void MSG_ReadVec3Coord( sizebuf_t *sb, vec3_t fa )
{
	if ( MSG_IsBitReading() )
	{
		MSG_ReadBitVec3Coord( fa );
	}
	else
	{
		MSG_StartBitReading( sb );
		MSG_ReadBitVec3Coord( fa );
		MSG_EndBitReading( sb );
	}
}

void MSG_WriteVec3Coord( sizebuf_t *sb, const vec3_t fa )
{
	if ( MSG_IsBitWriting() )
	{
		MSG_WriteBitVec3Coord( fa );
	}
	else
	{
		MSG_StartBitWriting( sb );
		MSG_WriteBitVec3Coord( fa );
		MSG_EndBitWriting( sb );
	}
}

//
// reading functions
//
int			msg_readcount;
qboolean	msg_badread;

void MSG_BeginReading( void )
{
	msg_readcount = 0;
	msg_badread = false;
}

// returns -1 and sets msg_badread if no more characters are available
int MSG_ReadChar( void )
{
	int		c;

	if ( msg_readcount + 1 > net_message.cursize )
	{
		msg_badread = true;
		return -1;
	}

	c = (signed char)net_message.data[msg_readcount];
	msg_readcount++;

	return c;
}

int MSG_ReadByte( void )
{
	int		c;

	if ( msg_readcount + 1 > net_message.cursize )
	{
		msg_badread = true;
		return -1;
	}

	c = (unsigned char)net_message.data[msg_readcount];
	msg_readcount++;

	return c;
}

int MSG_ReadShort( void )
{
	int		c;

	if ( msg_readcount + 2 > net_message.cursize )
	{
		msg_badread = true;
		return -1;
	}

	c = (short)( net_message.data[msg_readcount]
	+ ( net_message.data[msg_readcount + 1] << 8 ) );

	msg_readcount += 2;

	return c;
}

int MSG_ReadWord( void )
{
	int		c;

	if ( msg_readcount + 2 > net_message.cursize )
	{
		msg_badread = true;
		return -1;
	}

	c = net_message.data[msg_readcount]
	+ ( net_message.data[msg_readcount + 1] << 8 );

	msg_readcount += 2;

	return c;
}

int MSG_ReadLong( void )
{
	int		c;

	if ( msg_readcount + 4 > net_message.cursize )
	{
		msg_badread = true;
		return -1;
	}

	c = net_message.data[msg_readcount]
	+ ( net_message.data[msg_readcount + 1] << 8 )
	+ ( net_message.data[msg_readcount + 2] << 16 )
	+ ( net_message.data[msg_readcount + 3] << 24 );

	msg_readcount += 4;

	return c;
}

float MSG_ReadFloat( void )
{
	union
	{
		byte	b[4];
		float	f;
		int		l;
	} dat;

	dat.b[0] = net_message.data[msg_readcount];
	dat.b[1] = net_message.data[msg_readcount + 1];
	dat.b[2] = net_message.data[msg_readcount + 2];
	dat.b[3] = net_message.data[msg_readcount + 3];
	msg_readcount += 4;

	dat.l = LittleLong( dat.l );

	return dat.f;
}

int MSG_ReadBuf( int iSize, void *pbuf )
{
	if ( msg_readcount + iSize > net_message.cursize )
	{
		msg_badread = true;
		return -1;
	}

	Q_memcpy( pbuf, &net_message.data[msg_readcount], iSize );
	msg_readcount += iSize;

	return 1;
}

char *MSG_ReadString( void )
{
	static char	string[8192];
	int			l, c;

	l = 0;
	do
	{
		c = MSG_ReadChar();
		if ( c == -1 || c == 0 )
			break;
		string[l] = c;
		l++;
	} while ( l < sizeof( string ) - 1 );

	string[l] = 0;

	return string;
}

char *MSG_ReadStringLine( void )
{
	static char	string[2048];
	int			l, c;

	l = 0;
	do
	{
		c = MSG_ReadChar();
		if ( c == -1 || c == 0 || c == '\n' )
			break;
		string[l] = c;
		l++;
	} while ( l < sizeof( string ) - 1 );

	string[l] = 0;

	return string;
}

float MSG_ReadAngle( void )
{
	return MSG_ReadChar() * ( 360.0 / 256 );
}

float MSG_ReadHiresAngle( void )
{
	return MSG_ReadShort() * ( 360.0 / 65536 );
}

void MSG_ReadUsercmd( usercmd_t *to, usercmd_t *from )
{
	delta_t		*pdesc;

	pdesc = SV_LookupDelta( "usercmd_t" );
	MSG_StartBitReading( &net_message );
	DELTA_ParseDelta( (byte *)from, (byte *)to, pdesc );
	MSG_EndBitReading( &net_message );
	COM_NormalizeAngles( to->viewangles );
}

//===========================================================================

void SZ_Alloc( const char *name, sizebuf_t *buf, int startsize )
{
	buf->buffername = name;

	if ( startsize < 256 )
		startsize = 256;

	buf->data = Hunk_AllocName( startsize, name );
	buf->maxsize = startsize;
	buf->cursize = 0;
	buf->flags = 0;
}

void SZ_Clear( sizebuf_t *buf )
{
	buf->cursize = 0;
	buf->flags &= ~FSB_OVERFLOWED;
}

void *SZ_GetSpace( sizebuf_t *buf, int length )
{
	void	*data;

	if ( buf->cursize + length > buf->maxsize )
	{
		if ( !( buf->flags & FSB_ALLOWOVERFLOW ) )
		{
			if ( !buf->maxsize )
				Sys_Error( "SZ_GetSpace:  Tried to write to an uninitialized sizebuf_t: %s", buf->buffername ? buf->buffername : "???" );
			else
				Sys_Error( "SZ_GetSpace: overflow without FSB_ALLOWOVERFLOW set on %s", buf->buffername ? buf->buffername : "???" );
		}

		if ( length > buf->maxsize )
		{
			if ( !( buf->flags & FSB_ALLOWOVERFLOW ) )
				Sys_Error( "SZ_GetSpace: %i is > full buffer size on %s", length, buf->buffername ? buf->buffername : "???" );
			else
				Con_DPrintf( "SZ_GetSpace: %i is > full buffer size on %s, ignoring", length, buf->buffername ? buf->buffername : "???" );
		}

		Con_Printf( "SZ_GetSpace: overflow on %s\n", buf->buffername ? buf->buffername : "???" );
		SZ_Clear( buf );
		buf->flags |= FSB_OVERFLOWED;
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

void SZ_Write( sizebuf_t *buf, const void *data, int length )
{
	byte	*pData;

	pData = SZ_GetSpace( buf, length );

	if ( buf->flags & FSB_OVERFLOWED )
		return;

	Q_memcpy( pData, data, length );
}

void SZ_Print( sizebuf_t *buf, const char *data )
{
	int		len;
	byte	*pData;

	len = Q_strlen( data ) + 1;

	if ( buf->data[buf->cursize - 1] )
		pData = SZ_GetSpace( buf, len );		// no trailing 0
	else
		pData = (byte *)SZ_GetSpace( buf, len - 1 ) - 1;	// write over trailing 0

	if ( buf->flags & FSB_OVERFLOWED )
		return;

	Q_memcpy( pData, data, len );
}

/*
============
SZ_Unused_guess

Never called. Returns 0.
============
*/
int SZ_Unused_guess( void )
{
	return 0;
}

//============================================================================

/*
============
COM_SkipPath
============
*/
char *COM_SkipPath( char *pathname )
{
	char	*last;

	last = pathname;
	while ( *pathname )
	{
		if ( *pathname == '/' || *pathname == '\\' )
			last = pathname + 1;
		pathname++;
	}
	return last;
}

/*
============
COM_StripExtension
============
*/
void COM_StripExtension( char *in, char *out )
{
	char	*in_current = in + Q_strlen( in );
	char	*out_current = out + Q_strlen( in );
	int		found_extension = 0;

	while ( in_current >= in )
	{
		if ( !found_extension && *in_current == '.' )
		{
			*out_current = 0;
			found_extension = 1;
		}
		else
		{
			if ( *in_current == '/' || *in_current == '\\' )
				found_extension = 1;
			*out_current = *in_current;
		}

		in_current--;
		out_current--;
	}
}

/*
============
COM_FileExtension
============
*/
char *COM_FileExtension( char *in )
{
	static char	exten[8];
	int			i;

	while ( *in && *in != '.' )
		in++;
	if ( !*in )
		return "";
	in++;
	for ( i = 0; i < 7 && *in; i++, in++ )
		exten[i] = *in;
	exten[i] = 0;
	return exten;
}

/*
============
COM_FileBase

Extracts the base name of a file (no path, no extension, assumes '/' as path separator)
============
*/
void COM_FileBase( char *in, char *out )
{
	int		len, start, end;

	len = Q_strlen( in );
	if ( len <= 0 )
		return;

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
	Q_strncpy( out, &in[start], len );
	// Terminate it
	out[len] = 0;
}

/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension( char *path, char *extension )
{
	char	*src;

//
// if path doesn't have a .EXT, append extension
// (extension should include the .)
//
	src = path + Q_strlen( path ) - 1;

	while ( *src != '/' && src != path )
	{
		if ( *src == '.' )
			return;			// it has an extension
		src--;
	}

	strncat( path, extension, MAX_PATH - strlen( src ) );
}

/*
==============
COM_UngetToken
==============
*/
void COM_UngetToken( void )
{
	s_com_token_unget = true;
}

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse( char *data )
{
	int		c;
	int		len;

	if ( s_com_token_unget )
	{
		s_com_token_unget = false;
		return data;
	}

	len = 0;
	com_token[0] = 0;

	if ( !data )
		return NULL;

// skip whitespace
skipwhite:
	while ( ( c = *data ) <= ' ' )
	{
		if ( c == 0 )
			return NULL;	// end of file;
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
			if ( c == '\"' || !c || len >= COM_TOKEN_LEN - 1 )
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse single characters
	if ( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || ( !com_ignorecolons && c == ':' ) || c == ',' )
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
		if ( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || ( !com_ignorecolons && c == ':' ) || c == ',' || len >= COM_TOKEN_LEN - 1 )
			break;
	} while ( c > 32 );

	com_token[len] = 0;
	return data;
}

/*
==============
COM_ParseLine

Parse a line out of a string
==============
*/
char *COM_ParseLine( char *data )
{
	int		c;
	int		len;

	if ( s_com_token_unget )
	{
		s_com_token_unget = false;
		return data;
	}

	len = 0;
	com_token[0] = 0;

	if ( !data )
		return NULL;

	c = *data;

// parse a line out of the data
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
	} while ( c >= ' ' && len < COM_TOKEN_LEN - 1 );

	com_token[len] = 0;

	if ( c == 0 )	// end of file
		return NULL;

// eat whitespace (LF,CR,etc.) at the end of this line
	while ( ( c = *data ) < ' ' )
	{
		if ( c == 0 )
			return NULL;	// end of file;
		data++;
	}

	return data;
}

/*
==============
COM_TokenWaiting

Returns 1 if additional data is waiting to be processed on this line
==============
*/
int COM_TokenWaiting( char *buffer )
{
	char	*p;

	p = buffer;
	while ( *p && *p != '\n' )
	{
		if ( !isspace( *p ) || isalnum( *p ) )
			return 1;

		p++;
	}

	return 0;
}

/*
================
COM_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int COM_CheckParm( char *parm )
{
	int		i;

	for ( i = 1; i < com_argc; i++ )
	{
		if ( !com_argv[i] )
			continue;		// NEXTSTEP sometimes clears appkit vars.
		if ( !Q_strcmp( parm, com_argv[i] ) )
			return i;
	}

	return 0;
}

/*
================
COM_InitArgv
================
*/
void COM_InitArgv( int argc, char **argv )
{
	qboolean	safe;
	int			i, j, n;

// reconstitute the command line for the cmdline externally visible cvar
	n = 0;

	for ( j = 0; ( j < MAX_NUM_ARGVS ) && ( j < argc ); j++ )
	{
		i = 0;

		while ( ( n < ( COM_MAX_CMD_LINE - 1 ) ) && argv[j][i] )
		{
			com_cmdline[n++] = argv[j][i++];
		}

		if ( n < ( COM_MAX_CMD_LINE - 1 ) )
			com_cmdline[n++] = ' ';
		else
			break;
	}

	com_cmdline[n] = 0;

	safe = false;

	for ( com_argc = 0; ( com_argc < MAX_NUM_ARGVS ) && ( com_argc < argc );
		 com_argc++ )
	{
		largv[com_argc] = argv[com_argc];
		if ( !Q_strcmp( "-safe", argv[com_argc] ) )
			safe = true;
	}

	if ( safe )
	{
	// force all the safe-mode switches. Note that we reserved extra space in
	// case we need to add these, so we don't need an overflow check
		for ( i = 0; i < NUM_SAFE_ARGVS; i++ )
		{
			largv[com_argc] = safeargvs[i];
			com_argc++;
		}
	}

	largv[com_argc] = argvdummy;
	com_argv = largv;
}

static byte	swaptest[2] = { 1, 0 };

/*
================
COM_Init
================
*/
void COM_Init( char *basedir )
{
// set the byte swapping variables in a portable manner
	if ( *(short *)swaptest == 1 )
	{
		bigendien = false;
		BigShort = ShortSwap;
		LittleShort = ShortNoSwap;
		BigLong = LongSwap;
		LittleLong = LongNoSwap;
		BigFloat = FloatSwap;
		LittleFloat = FloatNoSwap;
	}
	else
	{
		bigendien = true;
		BigShort = ShortNoSwap;
		LittleShort = ShortSwap;
		BigLong = LongNoSwap;
		LittleLong = LongSwap;
		BigFloat = FloatNoSwap;
		LittleFloat = FloatSwap;
	}

	Cvar_RegisterVariable( &com_filewarning );

	COM_BitOpsInit();
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
============
*/
char *va( char *format, ... )
{
	va_list		argptr;
	static int	current;
	static char	string[1024][16];

	current++;
	current &= 0xF;

	va_start( argptr, format );
	_vsnprintf( string[current], 1024, format, argptr );
	va_end( argptr );

	return string[current];
}

/*
============
vstr

prints a vector into a temporary string
bufffer.
============
*/
char *vstr( vec_t *v )
{
	static int	idx;
	static char	string[16][1024];

	idx++;
	idx &= 15;

	_snprintf( string[idx], sizeof( string[idx] ), "%.4f %.4f %.4f", v[0], v[1], v[2] );
	return string[idx];
}

/*
============
memsearch
============
*/
int memsearch( byte *start, int count, int search )
{
	int		i;

	for ( i = 0; i < count; i++ )
		if ( start[i] == search )
			return i;
	return -1;
}

/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/

char	com_gamedir[MAX_PATH];
char	com_clientfallback[MAX_PATH];

/*
============
COM_WriteFile

The filename will be prefixed by the current game directory
============
*/
void COM_WriteFile( char *filename, void *data, int len )
{
	FileHandle_t	handle;
	char			name[MAX_PATH];

	_snprintf( name, sizeof( name ), "%s", filename );

	COM_FixSlashes( name );
	COM_CreatePath( name );

	handle = FS_Open( name, "wb" );
	if ( !handle )
	{
		Sys_Printf( "COM_WriteFile: failed on %s\n", name );
		return;
	}

	Sys_Printf( "COM_WriteFile: %s\n", name );
	FS_Write( data, len, 1, handle );
	FS_Close( handle );
}

/*
============
COM_FixSlashes

Changes all '/' characters into '\' characters, in place.
============
*/
void COM_FixSlashes( char *pname )
{
	while ( *pname )
	{
		if ( *pname == '/' )
			*pname = '\\';
		pname++;
	}
}

/*
============
COM_CreatePath

Only used for CopyFile
============
*/
void COM_CreatePath( char *path )
{
	char	*ofs;
	char	old;

	for ( ofs = path + 1; *ofs; ofs++ )
	{
		if ( *ofs == '/' || *ofs == '\\' )
		{	// create the directory
			old = *ofs;
			*ofs = 0;
			FS_CreateDirHierarchy( path, 0 );
			*ofs = old;
		}
	}
}

/*
===========
COM_CopyFile

Copies a file over from the net to the local cache, creating any directories
needed.  This is for the convenience of developers using ISDN from home.
===========
*/
void COM_CopyFile( char *netpath, char *cachepath )
{
	FileHandle_t	in, out;
	int				remaining, count;
	char			buf[4096];

	in = FS_Open( netpath, "rb" );
	if ( !in )
		return;

	remaining = FS_Size( in );

	COM_CreatePath( cachepath );	// create directories up to the cache file
	out = FS_Open( cachepath, "wb" );

	while ( remaining )
	{
		if ( remaining < sizeof( buf ) )
			count = remaining;
		else
			count = sizeof( buf );
		FS_Read( buf, count, 1, in );
		FS_Write( buf, count, 1, out );
		remaining -= count;
	}

	FS_Close( in );
	FS_Close( out );
}

/*
===========
COM_ExpandFilename

Finds the file in the search path, copies over the name with the full path name.
This doesn't search in the pak file.
===========
*/
int COM_ExpandFilename( char *filename )
{
	char	netpath[MAX_PATH];

	FS_GetLocalPath( filename, netpath, sizeof( netpath ) );
	strcpy( filename, netpath );
	if ( strlen( filename ) > 0 )
		return 1;
	return 0;
}

/*
===========
COM_FileSize

Returns the size of the file only.
===========
*/
int COM_FileSize( char *filename )
{
	FileHandle_t	fp;
	int				iSize;

	iSize = -1;
	fp = FS_Open( filename, "rb" );
	if ( fp )
	{
		iSize = FS_Size( fp );
		FS_Close( fp );
	}

	return iSize;
}

/*
============
COM_LoadFile

Filename are reletive to the quake directory.
Allways appends a 0 byte.
============
*/
cache_user_t	*loadcache;
byte			*loadbuf;
int				loadsize;

byte *COM_LoadFile( char *path, int usehunk, int *pLength )
{
	FileHandle_t	hFile;
	byte			*buf;
	char			base[256];
	int				cbread_guess;	// never used
	int				len;

	g_engdstAddrs.COM_LoadFile( &path, &usehunk, &pLength );

	buf = NULL;	// quiet compiler warning

	if ( pLength )
		*pLength = 0;

// look for it in the filesystem or pack files
	hFile = FS_Open( path, "rb" );
	if ( !hFile )
		return NULL;

	len = FS_Size( hFile );

// extract the filename base name for hunk tag
	COM_FileBase( path, base );
	base[32] = 0;

	if ( usehunk == 1 )
		buf = Hunk_AllocName( len + 1, base );
	else if ( usehunk == 2 )
		buf = Hunk_TempAlloc( len + 1 );
	else if ( usehunk == 0 )
		buf = Z_Malloc( len + 1 );
	else if ( usehunk == 3 )
		buf = Cache_Alloc( loadcache, len + 1, base );
	else if ( usehunk == 4 )
	{
		if ( len + 1 > loadsize )
			buf = Hunk_TempAlloc( len + 1 );
		else
			buf = loadbuf;
	}
	else if ( usehunk == 5 )
		buf = Mem_Malloc( len + 1 );
	else
		Sys_Error( "COM_LoadFile: bad usehunk for %s", path );

	if ( !buf )
	{
		Sys_Error( "COM_LoadFile: not enough space for %s", path );
		FS_Close( hFile );
		return NULL;
	}

	buf[len] = 0;

	FS_Read( buf, len, 1, hFile );
	FS_Close( hFile );

	if ( pLength )
		*pLength = len;

	return buf;
}

/*
============
COM_FreeFile
============
*/
void COM_FreeFile( void *buffer )
{
	g_engdstAddrs.COM_FreeFile( &buffer );

	if ( buffer )
		Mem_Free( buffer );
}

/*
============
COM_CopyFileChunk
============
*/
void COM_CopyFileChunk( FileHandle_t dst, FileHandle_t src, int nSize )
{
	int		copysize;
	char	copybuf[COM_COPY_CHUNK_SIZE];

	copysize = nSize;

	while ( copysize > COM_COPY_CHUNK_SIZE )
	{
		FS_Read( copybuf, COM_COPY_CHUNK_SIZE, 1, src );
		FS_Write( copybuf, COM_COPY_CHUNK_SIZE, 1, dst );
		copysize -= COM_COPY_CHUNK_SIZE;
	}

	FS_Read( copybuf, copysize, 1, src );
	FS_Write( copybuf, copysize, 1, dst );

	FS_Flush( src );
	FS_Flush( dst );
}

/*
============
COM_LoadFileLimit

Filename are reletive to the quake directory.
Allways appends a 0 byte.
============
*/
byte *COM_LoadFileLimit( char *path, int pos, int cbmax, int *pcbread, FileHandle_t *phFile )
{
	FileHandle_t	hFile;
	byte			*buf;
	char			base[32];
	int				len;
	int				cbload;

	buf = NULL;	// quiet compiler warning

	if ( !*phFile )
		hFile = FS_Open( path, "rb" );
	else
		hFile = *phFile;

	if ( !hFile )
		return NULL;

	len = FS_Size( hFile );
	if ( pos > len )
		Sys_Error( "COM_LoadFileLimit: invalid seek position for %s", path );

	FS_Seek( hFile, pos, FILESYSTEM_SEEK_HEAD );

	cbload = min( cbmax, len );
	*pcbread = cbload;

	if ( path )
		COM_FileBase( path, base );

	buf = Hunk_TempAlloc( cbload + 1 );
	if ( !buf )
	{
		if ( path )
			Sys_Error( "COM_LoadFileLimit: not enough space for %s", path );

		FS_Close( hFile );
		return NULL;
	}

	buf[cbload] = 0;
	FS_Read( buf, cbload, 1, hFile );

	*phFile = hFile;

	return buf;
}

byte *COM_LoadHunkFile( char *path )
{
	return COM_LoadFile( path, 1, NULL );
}

byte *COM_LoadTempFile( char *path, int *pLength )
{
	return COM_LoadFile( path, 2, pLength );
}

void COM_LoadCacheFile( char *path, struct cache_user_s *cu )
{
	loadcache = cu;
	COM_LoadFile( path, 3, NULL );
}

// uses temp hunk if larger than bufsize
byte *COM_LoadStackFile( char *path, void *buffer, int bufsize, int *length )
{
	byte	*buf;

	loadbuf = (byte *)buffer;
	loadsize = bufsize;
	buf = COM_LoadFile( path, 4, length );

	return buf;
}

/*
=================
COM_Shutdown

Remove the searchpaths
=================
*/
void COM_Shutdown( void )
{
}

/*
================
COM_AddAppDirectory
================
*/
void COM_AddAppDirectory( char *pszBaseDir, const char *appName )
{
	FS_AddSearchPath( pszBaseDir, "PLATFORM" );
}

/*
================
COM_AddDefaultDir
================
*/
void COM_AddDefaultDir( char *pszDir )
{
	if ( !pszDir || !*pszDir )
		return;

	FileSystem_AddFallbackGameDir( pszDir );
}

/*
================
COM_StripTrailingSlash
================
*/
void COM_StripTrailingSlash( char *ppath )
{
	int		len;

	len = strlen( ppath );

	if ( len > 0 )
	{
		if ( ( ppath[len - 1] == '\\' ) || ( ppath[len - 1] == '/' ) )
		{
			ppath[len - 1] = 0;
		}
	}
}

/*
================
COM_ParseDirectoryFromCmd
================
*/
void COM_ParseDirectoryFromCmd( const char *pCmdName, char *pDirName, const char *pDefault )
{
	const char	*pParameter;
	int			cmdParameterIndex;

	cmdParameterIndex = COM_CheckParm( (char *)pCmdName );
	pParameter = NULL;

	// Found a valid parameter?
	if ( cmdParameterIndex && cmdParameterIndex < com_argc - 1 )
	{
		pParameter = com_argv[cmdParameterIndex + 1];

		// Make sure it's not another flag
		if ( *pParameter == '-' || *pParameter == '+' )
		{
			pParameter = NULL;
		}
	}

	// Found a valid parameter on the cmd line?
	if ( pParameter )
	{
		// Grab it
		strcpy( pDirName, pParameter );
	}
	else if ( pDefault )
	{
		// Ok, then use the default
		strcpy( pDirName, pDefault );
	}
	else
	{
		// If no default either, then just terminate the string
		pDirName[0] = 0;
	}

	COM_StripTrailingSlash( pDirName );
}

/*
================
COM_SetupDirectories
================
*/
qboolean COM_SetupDirectories( void )
{
	char	basedir[512];

	com_clientfallback[0] = 0;
	com_gamedir[0] = 0;

	COM_ParseDirectoryFromCmd( "-basedir", basedir, "cryoffear" );
	COM_ParseDirectoryFromCmd( "-game", com_gamedir, basedir );

	if ( !FileSystem_SetGameDirectory( basedir, com_gamedir[0] ? com_gamedir : NULL ) )
		return false;

	// serverinfo holds MAX_INFO_STRING bytes, the limit passed here is larger
	Info_SetValueForStarKey( Info_Serverinfo(), "*gamedir", com_gamedir, MAX_SERVERINFO_STRING );
	return true;
}

/*
================
COM_CheckPrintMap
================
*/
void COM_CheckPrintMap( dheader_t *header, const char *mapname, qboolean bShowOutdated )
{
	qboolean	valid_guess = false;

	if ( header->version == HLBSP_VERSION )
		valid_guess = true;

	if ( valid_guess )
	{
		if ( !bShowOutdated )
			Con_Printf( "%s\n", mapname );
	}
	else
	{
		if ( bShowOutdated )
			Con_Printf( "OUTDATED:  %s\n", mapname );
	}
}

/*
================
COM_ListMaps
================
*/
void COM_ListMaps( char *pszSubString )
{
	int			nSubStringLen;
	int			showOutdated;
	const char	*findfn;

	nSubStringLen = 0;
	findfn = NULL;

	if ( pszSubString && *pszSubString )
		nSubStringLen = strlen( pszSubString );

	Con_Printf( "-------------\n" );

	for ( showOutdated = 1; showOutdated >= 0; showOutdated-- )
	{
		char	mapwild[64];

		strcpy( mapwild, "maps/*.bsp" );
		findfn = Sys_FindFirst( mapwild, NULL );

		while ( findfn )
		{
			char	curDir[MAX_PATH];

			_snprintf( curDir, sizeof( curDir ), "maps/%s", findfn );
			FS_GetLocalPath( curDir, curDir, sizeof( curDir ) );

			if ( strstr( curDir, com_gamedir ) && ( !nSubStringLen || !_strnicmp( findfn, pszSubString, nSubStringLen ) ) )
			{
				FileHandle_t	fp;
				dheader_t		header;
				char			sz[64];

				memset( &header, 0, sizeof( dheader_t ) );
				sprintf( sz, "maps/%s", findfn );

				fp = FS_Open( sz, "rb" );
				if ( fp )
				{
					FS_Read( &header, sizeof( dheader_t ), 1, fp );
					FS_Close( fp );
				}

				COM_CheckPrintMap( &header, findfn, showOutdated != 0 );
			}

			findfn = Sys_FindNext( NULL );
		}

		Sys_FindClose();
	}
}

/*
================
COM_Log

Log a debug message to specified file ( if pszFile == NULL uses c:\hllog.txt )
================
*/
void COM_Log( char *pszFile, char *fmt, ... )
{
	va_list			argptr;
	char			string[1024];
	FileHandle_t	fp;
	char			*pfilename;

	if ( !pszFile )
	{
		// Why so ugly
		pfilename = "c:\\hllog.txt";
	}
	else
	{
		pfilename = pszFile;
	}

	va_start( argptr, fmt );
	_vsnprintf( string, sizeof( string ), fmt, argptr );
	va_end( argptr );

	fp = FS_Open( pfilename, "a+t" );
	if ( fp )
	{
		FS_FPrintf( fp, "%s", string );
		FS_Close( fp );
	}
}

/*
================
COM_LoadFileForMe
================
*/
byte *COM_LoadFileForMe( char *filename, int *pLength )
{
	return COM_LoadFile( filename, 5, pLength );
}

/*
================
COM_CompareFileTime
================
*/
int COM_CompareFileTime( char *filename1, char *filename2, int *iCompare )
{
	int		bRet;

	bRet = 0;
	*iCompare = 0;

	if ( filename1 && filename2 )
	{
		int32	ft1 = FS_GetFileTime( filename1 );
		int32	ft2 = FS_GetFileTime( filename2 );

		if ( ft1 < ft2 )
		{
			*iCompare = -1;
		}
		else if ( ft1 > ft2 )
		{
			*iCompare = 1;
		}

		bRet = 1;
	}

	return bRet;
}

/*
================
COM_GetGameDir
================
*/
void COM_GetGameDir( char *szGameDir )
{
	if ( !szGameDir )
		return;

	_snprintf( szGameDir, MAX_PATH - 1, "%s", com_gamedir );
}

/*
================
COM_EntsForPlayerSlots
================
*/
int COM_EntsForPlayerSlots( int nPlayers )
{
	int		nMax;
	int		nWeaponSlots;
	int		i;

	nMax = MAX_EDICTS;

	i = COM_CheckParm( "-num_edicts" );
	if ( i )
	{
		nMax = max( nMax, atoi( com_argv[i + 1] ) );
	}

	nWeaponSlots = ( nPlayers - 1 ) * 15;

	return nMax + nWeaponSlots;
}

/*
================
COM_NormalizeAngles
================
*/
void COM_NormalizeAngles( vec_t *angles )
{
	int		i;

	// Normalize angles
	for ( i = 0; i < 3; i++ )
	{
		if ( angles[i] > 180.0 )
		{
			angles[i] -= 360.0;
		}
		else if ( angles[i] < -180.0 )
		{
			angles[i] += 360.0;
		}
	}
}

/*
================
COM_Munge funcs

Anything that is not a multiple of 4 bytes is left alone
================
*/
static unsigned char mungify_table[] =
{
	0x7A, 0x64, 0x05, 0xF1,
	0x1B, 0x9B, 0xA0, 0xB5,
	0xCA, 0xED, 0x61, 0x0D,
	0x4A, 0xDF, 0x8E, 0xC7
};

static unsigned char mungify_table2[] =
{
	0x05, 0x61, 0x7A, 0xED,
	0x1B, 0xCA, 0x0D, 0x9B,
	0x4A, 0xF1, 0x64, 0xC7,
	0xB5, 0x8E, 0xDF, 0xA0
};

static unsigned char mungify_table3[] =
{
	0x20, 0x07, 0x13, 0x61,
	0x03, 0x45, 0x17, 0x72,
	0x0A, 0x2D, 0x48, 0x0C,
	0x4A, 0x12, 0xA9, 0xB5
};

void COM_Munge( unsigned char *data, int len, int seq )
{
	int				i;
	int				mungelen;
	int				c;
	int				*pc;
	unsigned char	*p;
	int				j;

	mungelen = len & ~3;
	mungelen /= 4;

	for ( i = 0; i < mungelen; i++ )
	{
		pc = (int *)&data[i * 4];
		c = *pc;
		c ^= ~seq;
		c = LongSwap( c );

		p = (unsigned char *)&c;
		for ( j = 0; j < 4; j++ )
		{
			*p++ ^= ( 0xa5 | ( j << j ) | j | mungify_table[( i + j ) & 0x0f] );
		}

		c ^= seq;
		*pc = c;
	}
}

void COM_UnMunge( unsigned char *data, int len, int seq )
{
	int				i;
	int				mungelen;
	int				c;
	int				*pc;
	unsigned char	*p;
	int				j;

	mungelen = len & ~3;
	mungelen /= 4;

	for ( i = 0; i < mungelen; i++ )
	{
		pc = (int *)&data[i * 4];
		c = *pc;
		c ^= seq;

		p = (unsigned char *)&c;
		for ( j = 0; j < 4; j++ )
		{
			*p++ ^= ( 0xa5 | ( j << j ) | j | mungify_table[( i + j ) & 0x0f] );
		}

		c = LongSwap( c );
		c ^= ~seq;
		*pc = c;
	}
}

void COM_Munge2( unsigned char *data, int len, int seq )
{
	int				i;
	int				mungelen;
	int				c;
	int				*pc;
	unsigned char	*p;
	int				j;

	mungelen = len & ~3;
	mungelen /= 4;

	for ( i = 0; i < mungelen; i++ )
	{
		pc = (int *)&data[i * 4];
		c = *pc;
		c ^= ~seq;
		c = LongSwap( c );

		p = (unsigned char *)&c;
		for ( j = 0; j < 4; j++ )
		{
			*p++ ^= ( 0xa5 | ( j << j ) | j | mungify_table2[( i + j ) & 0x0f] );
		}

		c ^= seq;
		*pc = c;
	}
}

void COM_UnMunge2( unsigned char *data, int len, int seq )
{
	int				i;
	int				mungelen;
	int				c;
	int				*pc;
	unsigned char	*p;
	int				j;

	mungelen = len & ~3;
	mungelen /= 4;

	for ( i = 0; i < mungelen; i++ )
	{
		pc = (int *)&data[i * 4];
		c = *pc;
		c ^= seq;

		p = (unsigned char *)&c;
		for ( j = 0; j < 4; j++ )
		{
			*p++ ^= ( 0xa5 | ( j << j ) | j | mungify_table2[( i + j ) & 0x0f] );
		}

		c = LongSwap( c );
		c ^= ~seq;
		*pc = c;
	}
}

void COM_Munge3( unsigned char *data, int len, int seq )
{
	int				i;
	int				mungelen;
	int				c;
	int				*pc;
	unsigned char	*p;
	int				j;

	mungelen = len & ~3;
	mungelen /= 4;

	for ( i = 0; i < mungelen; i++ )
	{
		pc = (int *)&data[i * 4];
		c = *pc;
		c ^= ~seq;
		c = LongSwap( c );

		p = (unsigned char *)&c;
		for ( j = 0; j < 4; j++ )
		{
			*p++ ^= ( 0xa5 | ( j << j ) | j | mungify_table3[( i + j ) & 0x0f] );
		}

		c ^= seq;
		*pc = c;
	}
}

void COM_UnMunge3( unsigned char *data, int len, int seq )
{
	int				i;
	int				mungelen;
	int				c;
	int				*pc;
	unsigned char	*p;
	int				j;

	mungelen = len & ~3;
	mungelen /= 4;

	for ( i = 0; i < mungelen; i++ )
	{
		pc = (int *)&data[i * 4];
		c = *pc;
		c ^= seq;

		p = (unsigned char *)&c;
		for ( j = 0; j < 4; j++ )
		{
			*p++ ^= ( 0xa5 | ( j << j ) | j | mungify_table3[( i + j ) & 0x0f] );
		}

		c = LongSwap( c );
		c ^= ~seq;
		*pc = c;
	}
}

/*
================
COM_GetApproxWavePlayLength
================
*/
unsigned int COM_GetApproxWavePlayLength( const char *filepath )
{
	FileHandle_t	hFile;
	unsigned int	filelength = 0;
	int				found = 0;
	FormatChunk		format;
	FormatChunk		*tempFormat = NULL;
	unsigned int	numMilliseconds = 0;
	char			buf[WAVE_HEADER_LENGTH + 1];

	hFile = FS_Open( filepath, "rb" );
	if ( !hFile )
		return 0;

	filelength = FS_Size( hFile );
	FS_Read( buf, WAVE_HEADER_LENGTH, 1, hFile );
	FS_Close( hFile );

	filelength -= WAVE_HEADER_LENGTH;
	buf[WAVE_HEADER_LENGTH] = 0;

	if ( _strnicmp( buf, "RIFF", strlen( "RIFF" ) ) )
		return 0;

	if ( _strnicmp( &buf[8], "WAVE", strlen( "WAVE" ) ) )
		return 0;

	memcpy( &format, &buf[12], sizeof( FormatChunk ) );

	if ( _strnicmp( format.chunkID, "fmt ", strlen( "fmt " ) ) )
		return 0;

	if ( format.dwAvgBytesPerSec / 1000 == 0 )
		numMilliseconds = ( filelength / format.dwAvgBytesPerSec ) * 1000;
	else
		numMilliseconds = filelength / ( format.dwAvgBytesPerSec / 1000 );

	return numMilliseconds;
}
