// common.h -- general purpose engine functions (common.c)

#ifndef COMMON_H
#define COMMON_H

#define MAX_NUM_ARGVS		50
#define NUM_SAFE_ARGVS		7

#define COM_MAX_CMD_LINE	256
#define COM_TOKEN_LEN		1024

#define COM_COPY_CHUNK_SIZE	1024

//============================================================================

#define FSB_ALLOWOVERFLOW	( 1 << 0 )	// if false, do a Sys_Error
#define FSB_OVERFLOWED		( 1 << 1 )	// set to true if the buffer size failed

typedef struct sizebuf_s
{
	const char		*buffername;
	unsigned short	flags;
	byte			*data;
	int				maxsize;
	int				cursize;	// size of the data written so far
} sizebuf_t;

void	SZ_Alloc( const char *name, sizebuf_t *buf, int startsize );
void	SZ_Clear( sizebuf_t *buf );
void	*SZ_GetSpace( sizebuf_t *buf, int length );
void	SZ_Write( sizebuf_t *buf, const void *data, int length );
void	SZ_Print( sizebuf_t *buf, const char *data );	// strcats onto the sizebuf

//============================================================================

extern	qboolean	bigendien;

extern	short	(*BigShort)( short l );
extern	short	(*LittleShort)( short l );
extern	int32	(*BigLong)( int32 l );
extern	int32	(*LittleLong)( int32 l );
extern	float	(*BigFloat)( float l );
extern	float	(*LittleFloat)( float l );

short	ShortSwap( short l );
short	ShortNoSwap( short l );
int32	LongSwap( int32 l );
int32	LongNoSwap( int32 l );
float	FloatSwap( float f );
float	FloatNoSwap( float f );

//============================================================================

struct usercmd_s;

void	MSG_WriteChar( sizebuf_t *sb, int c );
void	MSG_WriteByte( sizebuf_t *sb, int c );
void	MSG_WriteShort( sizebuf_t *sb, int c );
void	MSG_WriteWord( sizebuf_t *sb, int c );
void	MSG_WriteLong( sizebuf_t *sb, int c );
void	MSG_WriteFloat( sizebuf_t *sb, float f );
void	MSG_WriteString( sizebuf_t *sb, const char *s );
void	MSG_WriteBuf( sizebuf_t *sb, int iSize, void *buf );
void	MSG_WriteAngle( sizebuf_t *sb, float f );
void	MSG_WriteHiresAngle( sizebuf_t *sb, float f );
void	MSG_WriteUsercmd( sizebuf_t *buf, struct usercmd_s *to, struct usercmd_s *from );
void	MSG_WriteCoord( sizebuf_t *sb, const float f );
void	MSG_WriteVec3Coord( sizebuf_t *sb, const float *fa );

void	COM_BitOpsInit( void );
void	MSG_WriteOneBit( int nValue );
void	MSG_StartBitWriting( sizebuf_t *buf );
qboolean	MSG_IsBitWriting( void );
void	MSG_EndBitWriting( sizebuf_t *buf );
void	MSG_WriteBits( unsigned int data, int numbits );
void	MSG_WriteSBits( int data, int numbits );
void	MSG_WriteBitString( const char *p );
void	MSG_WriteBitData( void *src, int length );
void	MSG_WriteBitAngle( float fAngle, int numbits );
float	MSG_ReadBitAngle( int numbits );
int		MSG_CurrentBit( void );
qboolean	MSG_IsBitReading( void );
void	MSG_StartBitReading( sizebuf_t *buf );
void	MSG_EndBitReading( sizebuf_t *buf );
int		MSG_ReadOneBit( void );
unsigned int	MSG_ReadBits( int numbits );
unsigned int	MSG_PeekBits( int numbits );
int		MSG_ReadSBits( int numbits );
char	*MSG_ReadBitString( void );
int		MSG_ReadBitData( void *dest, int length );
float	MSG_ReadBitCoord( void );
void	MSG_WriteBitCoord( const float f );
void	MSG_ReadBitVec3Coord( float *fa );
void	MSG_WriteBitVec3Coord( const float *fa );

extern	int			msg_readcount;
extern	qboolean	msg_badread;		// set if a read goes beyond end of message

void	MSG_BeginReading( void );
int		MSG_ReadChar( void );
int		MSG_ReadByte( void );
int		MSG_ReadShort( void );
int		MSG_ReadWord( void );
int		MSG_ReadLong( void );
float	MSG_ReadFloat( void );
int		MSG_ReadBuf( int iSize, void *pbuf );
char	*MSG_ReadString( void );
char	*MSG_ReadStringLine( void );
float	MSG_ReadAngle( void );
float	MSG_ReadHiresAngle( void );
void	MSG_ReadUsercmd( struct usercmd_s *to, struct usercmd_s *from );
float	MSG_ReadCoord( sizebuf_t *sb );
void	MSG_ReadVec3Coord( sizebuf_t *sb, float *fa );

//============================================================================

char	*Info_Serverinfo( void );

void	Q_memset( void *dest, int fill, int count );
void	Q_memcpy( void *dest, const void *src, int count );
int		Q_memcmp( void *m1, void *m2, int count );
void	Q_strcpy( char *dest, const char *src );
void	Q_strncpy( char *dest, const char *src, int count );
int		Q_strlen( const char *str );
char	*Q_strrchr( char *s, char c );
void	Q_strcat( char *dest, char *src );
int		Q_strcmp( const char *s1, const char *s2 );
int		Q_strncmp( const char *s1, const char *s2, int count );
int		Q_strncasecmp( const char *s1, const char *s2, int n );
int		Q_strcasecmp( const char *s1, const char *s2 );
int		Q_stricmp( const char *s1, const char *s2 );
int		Q_strnicmp( const char *s1, const char *s2, int n );
int		Q_atoi( const char *str );
float	Q_atof( const char *str );
char	*Q_strlwr( char *src );
int		Q_FileNameCmp( char *file1, char *file2 );
char	*Q_strstr( const char *s1, const char *search );

unsigned char	COM_Nibble( char c );
void	COM_HexConvert( const char *pszInput, int nInputLength, unsigned char *pOutput );
char	*COM_BinPrintf( unsigned char *buf, int nLen );

extern	char	gszDisconnectReason[256];
extern	char	gszExtendedDisconnectReason[256];
extern	qboolean	gfExtendedError;

void	COM_ExplainDisconnection( qboolean bPrint, char *fmt, ... );
void	COM_ExtendedExplainDisconnection( qboolean bPrint, char *fmt, ... );

//============================================================================

extern	char	com_token[COM_TOKEN_LEN];
extern	qboolean	com_ignorecolons;	// treat ':' as a regular character

extern	int		com_argc;
extern	char	**com_argv;

extern	char	com_cmdline[COM_MAX_CMD_LINE];
extern	char	serverinfo[];

extern	char	*gpszAppName;
extern	char	gpszVersionString[32];
extern	char	gpszProductString[32];

char	*COM_SkipPath( char *pathname );
void	COM_StripExtension( char *in, char *out );
char	*COM_FileExtension( char *in );
void	COM_FileBase( char *in, char *out );
void	COM_DefaultExtension( char *path, char *extension );

void	COM_UngetToken( void );
char	*COM_Parse( char *data );
char	*COM_ParseLine( char *data );
int		COM_TokenWaiting( char *buffer );

int		COM_CheckParm( char *parm );
void	COM_InitArgv( int argc, char **argv );
void	COM_Init( char *basedir );

char	*va( char *format, ... );
char	*vstr( float *v );
int		memsearch( byte *start, int count, int search );

//============================================================================

extern	char	com_gamedir[MAX_PATH];
extern	char	com_clientfallback[MAX_PATH];

struct cache_user_s;

void	COM_WriteFile( char *filename, void *data, int len );
void	COM_FixSlashes( char *pname );
void	COM_CreatePath( char *path );
void	COM_CopyFile( char *netpath, char *cachepath );
int		COM_ExpandFilename( char *filename );
int		COM_FileSize( char *filename );
byte	*COM_LoadFile( char *path, int usehunk, int *pLength );
void	COM_FreeFile( void *buffer );
void	COM_CopyFileChunk( FileHandle_t dst, FileHandle_t src, int nSize );
byte	*COM_LoadFileLimit( char *path, int pos, int cbmax, int *pcbread, FileHandle_t *phFile );
byte	*COM_LoadHunkFile( char *path );
byte	*COM_LoadTempFile( char *path, int *pLength );
void	COM_LoadCacheFile( char *path, struct cache_user_s *cu );
byte	*COM_LoadStackFile( char *path, void *buffer, int bufsize, int *length );
void	COM_Shutdown( void );
void	COM_AddAppDirectory( char *pszBaseDir, const char *appName );
void	COM_AddDefaultDir( char *pszDir );
void	COM_StripTrailingSlash( char *ppath );
void	COM_ParseDirectoryFromCmd( const char *pCmdName, char *pDirName, const char *pDefault );
qboolean	COM_SetupDirectories( void );
void	COM_ListMaps( char *pszSubString );
void	COM_Log( char *pszFile, char *fmt, ... );
byte	*COM_LoadFileForMe( char *filename, int *pLength );
int		COM_CompareFileTime( char *filename1, char *filename2, int *iCompare );
void	COM_GetGameDir( char *szGameDir );
int		COM_EntsForPlayerSlots( int nPlayers );
void	COM_NormalizeAngles( float *angles );

void	COM_Munge( unsigned char *data, int len, int seq );
void	COM_UnMunge( unsigned char *data, int len, int seq );
void	COM_Munge2( unsigned char *data, int len, int seq );
void	COM_UnMunge2( unsigned char *data, int len, int seq );
void	COM_Munge3( unsigned char *data, int len, int seq );
void	COM_UnMunge3( unsigned char *data, int len, int seq );

unsigned int	COM_GetApproxWavePlayLength( const char *filepath );

#endif // COMMON_H
