// delta.h -- delta compression of structures (delta.c)

#ifndef DELTA_H
#define DELTA_H

// field types
#define DT_BYTE				( 1 << 0 )	// A byte
#define DT_SHORT			( 1 << 1 )	// 2 byte field
#define DT_FLOAT			( 1 << 2 )	// A floating point field
#define DT_INTEGER			( 1 << 3 )	// 4 byte integer
#define DT_ANGLE			( 1 << 4 )	// A floating point angle
#define DT_TIMEWINDOW_8		( 1 << 5 )	// A floating point timestamp relative to server time
#define DT_TIMEWINDOW_BIG	( 1 << 6 )	// A floating point timestamp relative to server time (with more precision and custom multiplier)
#define DT_STRING			( 1 << 7 )	// A null terminated string, sent as 8 byte chars
#define DT_SIGNED			( 1 << 31 )	// sign modificator

// delta_description_t flags
#define FDT_MARK			( 1 << 0 )	// Delta mark for sending

typedef struct delta_stats_s
{
	int		sendcount;
	int		receivedcount;
} delta_stats_t;

typedef struct delta_description_s
{
	int				fieldType;
	char			fieldName[32];
	int				fieldOffset;
	short			fieldSize;
	int				significant_bits;
	float			premultiply;
	float			postmultiply;
	short			flags;
	delta_stats_t	stats;
} delta_description_t;

struct delta_s;

typedef void (*encoder_t)( struct delta_s *, const unsigned char *, const unsigned char * );

typedef struct delta_s
{
	int						dynamic;
	int						fieldCount;
	char					conditionalencodename[32];
	encoder_t				conditionalencode;
	delta_description_t		*pdd;
} delta_t;

typedef struct delta_encoder_s
{
	struct delta_encoder_s	*next;
	char					*name;
	encoder_t				conditionalencode;
} delta_encoder_t;

extern	delta_t		g_MetaDelta;

struct delta_link_s;
struct delta_definition_s;

delta_description_t	*DELTA_FindField( delta_t *pFields, const char *pszField );
int		DELTA_FindFieldIndex( struct delta_s *pFields, const char *fieldname );
void	DELTA_SetField( struct delta_s *pFields, const char *fieldname );
void	DELTA_UnsetField( struct delta_s *pFields, const char *fieldname );
void	DELTA_SetFieldByIndex( struct delta_s *pFields, int fieldNumber );
void	DELTA_UnsetFieldByIndex( struct delta_s *pFields, int fieldNumber );
void	DELTA_ClearFlags( delta_t *pFields );
int		DELTA_TestDelta( unsigned char *from, unsigned char *to, delta_t *pFields );
int		DELTA_CountSendFields( delta_t *pFields );
void	DELTA_MarkSendFields( unsigned char *from, unsigned char *to, delta_t *pFields );
void	DELTA_SetSendFlagBits( delta_t *pFields, int *bits, int *bytecount );
void	DELTA_WriteMarkedFields( unsigned char *from, unsigned char *to, delta_t *pFields );
int		DELTA_CheckDelta( unsigned char *from, unsigned char *to, delta_t *pFields );
int		DELTA_WriteDelta( unsigned char *from, unsigned char *to, qboolean force, delta_t *pFields, void ( *callback )( void ) );
int		_DELTA_WriteDelta( unsigned char *from, unsigned char *to, qboolean force, delta_t *pFields, void ( *callback )( void ), int sendfields );
int		DELTA_ParseDelta( unsigned char *from, unsigned char *to, delta_t *pFields );
void	DELTA_AddEncoder( char *name, encoder_t conditionalencode );
void	DELTA_ClearEncoders( void );
encoder_t	DELTA_LookupEncoder( char *name );
int		DELTA_CountLinks( struct delta_link_s *plinks );
void	DELTA_ReverseLinks( struct delta_link_s **plinks );
void	DELTA_ClearLinks( struct delta_link_s **plinks );
delta_t	*DELTA_BuildFromLinks( struct delta_link_s **pplinks );
int		DELTA_FindOffset( int count, struct delta_definition_s *pdef, char *fieldname );
qboolean	DELTA_ParseType( delta_description_t *pdelta, char **pstream );
qboolean	DELTA_ParseField( int count, struct delta_definition_s *pdefinition, struct delta_link_s *pField, char **pstream );
void	DELTA_FreeDescription( delta_t **ppdesc );
void	DELTA_AddDefinition( char *name, struct delta_definition_s *pdef, int numelements );
void	DELTA_ClearDefinitions( void );
struct delta_definition_s	*DELTA_FindDefinition( char *name, int *count );
void	DELTA_SkipDescription( char **pstream );
qboolean	DELTA_ParseOneField( char **ppstream, struct delta_link_s **pplist, int count, struct delta_definition_s *pdefinition );
qboolean	DELTA_ParseDescription( char *name, delta_t **ppdesc, char *pstream );
qboolean	DELTA_Load( char *name, delta_t **ppdesc, char *pszFile );
void	DELTA_RegisterDescription( char *name );
void	DELTA_ClearRegistrations( void );
delta_t	**DELTA_LookupRegistration( char *name );
void	DELTA_ClearStats( delta_t *p );
void	DELTA_ClearStats_f( void );
void	DELTA_PrintStats( char *name, delta_t *p );
void	DELTA_DumpStats_f( void );
void	DELTA_Init( void );
void	DELTA_Shutdown( void );

#endif // DELTA_H
