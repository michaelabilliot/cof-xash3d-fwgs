// DetailTexture.cpp -- detail textures: a second texture unit modulates the
// world surfaces with a tiled detail map named in maps/<level>_detail.txt

#pragma warning( disable : 4530 )	// the engine is built without exception handling

#include <string>
#include <vector>
#include <map>

using namespace std;

extern "C"
{
#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "GL_MODEL.H"
#include "client.h"
#include "GLQUAKE.H"

extern int		TEXTURE2_SGIS;

int		GL_LoadTexture2( char *identifier, GL_TEXTURETYPE textureType, int width, int height, byte *data, qboolean mipmap, int iType, byte *pPal, int filter );

void	DT_Initialize( void );
void	DT_LoadDetailMapFile( char *levelName );
const char *DT_GetDetailTextureName( char *diffuseName );
void	DT_LoadDetailTexture( char *diffuseName, int diffuseId );
int		DT_SetRenderState( int diffuseId );
void	DT_SetTextureCoordinates( float u, float v );
void	DT_ClearRenderState( void );
}

#define GL_COMBINE_ARB			0x8570
#define GL_COMBINE_RGB_ARB		0x8571
#define GL_RGB_SCALE_ARB		0x8573

class DetailMap
{
public:
	DetailMap( const string &diffuseName, const string &detailName, float sScale, float tScale, GLint oglTextureId, GLint oglDetailId ) :
		_diffuseName( diffuseName ),
		_detailName( detailName ),
		_sScale( sScale ),
		_tScale( tScale ),
		_oglTextureId( oglTextureId ),
		_oglDetailId( oglDetailId )
	{
	}

	~DetailMap()
	{
	}

	string	getDiffuseName( void ) { return _diffuseName; }
	string	getDetailName( void ) { return _detailName; }
	float	getSScale( void ) { return _sScale; }
	float	getTScale( void ) { return _tScale; }
	GLint	getOglTextureId( void ) { return _oglTextureId; }
	GLint	getOglDetailId( void ) { return _oglDetailId; }

	void	setDiffuseName( const string &diffuseName ) { _diffuseName = diffuseName; }
	void	setDetailName( const string &detailName ) { _detailName = detailName; }
	void	setSScale( float sScale ) { _sScale = sScale; }
	void	setTScale( float tScale ) { _tScale = tScale; }
	void	setOglTextureId( GLint oglTextureId ) { _oglTextureId = oglTextureId; }
	void	setOglDetailId( GLint oglDetailId ) { _oglDetailId = oglDetailId; }

private:
	string	_diffuseName;
	string	_detailName;
	float	_sScale;
	float	_tScale;
	GLint	_oglTextureId;
	GLint	_oglDetailId;
};

typedef enum
{
	PS_START,
	PS_DIFFUSE_NAME,
	PS_DETAIL_TEXTURE,
	PS_S_SCALE_FACTOR,
	PS_T_SCALE_FACTOR,
	PS_ENTRY,
	PS_ERROR,
	PS_COMPLETE
} ParseState;

typedef enum
{
	LS_START,
	LS_WHITE_SPACE,
	LS_COMMENT,
	LS_TOKEN,
	LS_ERROR,
	LS_COMPLETE
} LexerState;

static bool g_detailEnable = false;
static vector< DetailMap * > g_detailVector;
static map< int, DetailMap * > g_idMap;
static map< string, int > g_detailNameToIdMap;
static map< string, int > g_detailLoadFailedMap;

static bool g_demandLoad = false;
static bool g_detTexLoaded = false;
static string g_levelName;
static vector< pair< string, int > > g_decalTexIDs;

cvar_t	r_detailtextures = { "r_detailtextures", "0", FCVAR_ARCHIVE };

cvar_t	r_detailtexturessupported = { "r_detailtexturessupported", "1", FCVAR_SPONLY };

static bool detTexSupported = false;
static GLint lastDetTex = -1;
static GLfloat lastDetSScale = 1.0, lastDetTScale = 1.0;

static char whiteSpace[4] =
{
	'\r', '\n', '\t', ' '
};

static char extraTokenCharacters[8] =
{
	'_', '.', '-', '/', '{', '~', '!', '+'
};

static GLint defaultID = -1;

/*
================
DT_Initialize
================
*/
void DT_Initialize( void )
{
	Cvar_RegisterVariable( &r_detailtextures );
	Cvar_RegisterVariable( &r_detailtexturessupported );

	qglSelectTextureSGIS( TEXTURE2_SGIS );
	qglEnable( GL_TEXTURE_2D );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );
	qglTexEnvf( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE );
	qglTexEnvf( GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 2.0 );
	qglDisable( GL_TEXTURE_2D );
	qglSelectTextureSGIS( TEXTURE0_SGIS );

	detTexSupported = true;
}

inline bool DT_IsWhiteSpace( char *cPtr )
{
	for ( int idx = 0; idx < sizeof( whiteSpace ); idx++ )
	{
		if ( *cPtr == whiteSpace[idx] )
			return true;
	}

	return false;
}

inline bool DT_IsEnd( char *cPtr )
{
	return *cPtr == EOF;
}

inline bool DT_IsTokenCharacter( char *cPtr )
{
	if ( *cPtr >= 'A' && *cPtr <= 'Z' )
		return true;

	if ( *cPtr >= 'a' && *cPtr <= 'z' )
		return true;

	if ( *cPtr >= '0' && *cPtr <= '9' )
		return true;

	for ( int idx = 0; idx < sizeof( extraTokenCharacters ); idx++ )
	{
		if ( *cPtr == extraTokenCharacters[idx] )
			return true;
	}

	return false;
}

/*
================
DT_GetToken
================
*/
bool DT_GetToken( char *&cPtr, ParseState &parseState, string &token )
{
	char *sPtr;
	char *ePtr;
	LexerState state;

	state = LS_START;

	while ( state != LS_ERROR && state != LS_COMPLETE )
	{
		switch ( state )
		{
		case LS_START:
			sPtr = ePtr = cPtr;
			state = LS_WHITE_SPACE;
			break;

		case LS_WHITE_SPACE:
			while ( 1 )
			{
				if ( DT_IsWhiteSpace( sPtr ) )
				{
					sPtr++;
				}
				else if ( DT_IsEnd( sPtr ) )
				{
					state = LS_COMPLETE;
					parseState = PS_COMPLETE;
					break;
				}
				else
				{
					state = LS_COMMENT;
					break;
				}
			}
			ePtr = sPtr;
			break;

		case LS_COMMENT:
			if ( *sPtr == '/' && *( sPtr + 1 ) == '/' )
			{
				sPtr += 2;
				while ( *sPtr != '\r' && *sPtr != '\n' && *sPtr != EOF )
					sPtr++;
				state = LS_WHITE_SPACE;
			}
			else
			{
				state = LS_TOKEN;
			}
			break;

		case LS_TOKEN:
			while ( 1 )
			{
				if ( DT_IsTokenCharacter( ePtr ) )
				{
					ePtr++;
				}
				else
				{
					break;
				}
			}

			if ( sPtr != ePtr )
			{
				token = string( sPtr, ePtr - sPtr );
			}
			else
			{
				state = LS_ERROR;
				parseState = PS_ERROR;
			}
			state = LS_COMPLETE;
			break;
		}
	}

	cPtr = ePtr;

	if ( parseState == PS_ERROR || parseState == PS_COMPLETE )
		return false;

	return true;
}

/*
================
DT_Parse
================
*/
void DT_Parse( char *pBuffer )
{
	ParseState state;
	char *cPtr;
	string diffuseName;
	string detailName;
	string scaleFactorString;
	float sScaleFactor;
	float tScaleFactor;

	state = PS_START;

	while ( state != PS_ERROR && state != PS_COMPLETE )
	{
		switch ( state )
		{
		case PS_START:
			cPtr = pBuffer;
			state = PS_DIFFUSE_NAME;
			break;

		case PS_DIFFUSE_NAME:
			if ( DT_GetToken( cPtr, state, diffuseName ) )
				state = PS_DETAIL_TEXTURE;
			break;

		case PS_DETAIL_TEXTURE:
			if ( DT_GetToken( cPtr, state, detailName ) )
				state = PS_S_SCALE_FACTOR;
			break;

		case PS_S_SCALE_FACTOR:
			if ( DT_GetToken( cPtr, state, scaleFactorString ) )
			{
				sScaleFactor = atof( scaleFactorString.c_str() );
				state = PS_T_SCALE_FACTOR;
			}
			break;

		case PS_T_SCALE_FACTOR:
			if ( DT_GetToken( cPtr, state, scaleFactorString ) )
			{
				tScaleFactor = atof( scaleFactorString.c_str() );
				state = PS_ENTRY;
			}
			break;

		case PS_ENTRY:
			g_detailVector.push_back( new DetailMap( diffuseName, detailName, sScaleFactor, tScaleFactor, 0, 0 ) );
			state = PS_DIFFUSE_NAME;
			break;
		}
	}

	if ( state == PS_ERROR )
		Con_Printf( "Error parsing detail texture file.\n" );
}

/*
================
DT_LoadDetailMapFile
================
*/
void DT_LoadDetailMapFile( char *levelName )
{
	if ( !detTexSupported )
		return;

	if ( r_detailtextures.value == 0 )
	{
		g_detTexLoaded = false;
		g_levelName = levelName;
		return;
	}

	g_detTexLoaded = true;

	if ( !g_demandLoad )
		g_decalTexIDs.clear();

	string::size_type idx, length = g_detailVector.size();
	for ( idx = 0; idx < length; idx++ )
	{
		delete g_detailVector[idx];
		g_detailVector[idx] = NULL;
	}

	g_detailVector.clear();
	g_idMap.clear();
	g_detailNameToIdMap.clear();
	g_detailLoadFailedMap.clear();

	string fileName( "maps/" );
	fileName += string( levelName ) + string( "_detail.txt" );

	FileHandle_t fileHandle = FS_Open( fileName.c_str(), "rb" );
	if ( !fileHandle )
	{
		Con_Printf( "No detail texture mapping file: %s\n", fileName.c_str() );
		return;
	}

	unsigned int fileSize = FS_FileSize( fileName.c_str() );
	char *pBuffer = new char[fileSize + 10];
	unsigned int numberBytesRead;
	int readSuccess = FS_Read( pBuffer, fileSize, 1, fileHandle );
	if ( readSuccess <= 0 )
	{
		Con_Printf( "Detail texture mapping file read failed\n" );
		return;
	}

	numberBytesRead = fileSize;
	FS_Close( fileHandle );

	pBuffer[numberBytesRead] = EOF;
	DT_Parse( pBuffer );

	delete [] pBuffer;
}

/*
================
DT_GetDetailTextureName
================
*/
const char *DT_GetDetailTextureName( char *diffuseName )
{
	string detailName;
	static char buffer[128];

	string::size_type idx, length = g_detailVector.size();
	for ( idx = 0; idx < length; idx++ )
	{
		if ( !_stricmp( diffuseName, g_detailVector[idx]->getDiffuseName().c_str() ) )
		{
			detailName = g_detailVector[idx]->getDetailName();
			break;
		}
	}

	if ( detailName.length() )
	{
		strcpy( buffer, detailName.c_str() );
		return buffer;
	}

	return NULL;
}

static GLint loadTextureFile( const char *filename );

/*
================
DT_LoadDetailTexture
================
*/
void DT_LoadDetailTexture( char *diffuseName, int diffuseId )
{
	int detailId;
	string detailName;
	string::size_type idx, length = g_detailVector.size();
	map< string, int >::iterator iter;
	static bool once = true;

	if ( !detTexSupported )
		return;

	if ( r_detailtextures.value == 0 )
	{
		g_decalTexIDs.push_back( pair< string, int >( diffuseName, diffuseId ) );
		return;
	}

	for ( idx = 0; idx < length; idx++ )
	{
		if ( !_stricmp( diffuseName, g_detailVector[idx]->getDiffuseName().c_str() ) )
		{
			detailName = g_detailVector[idx]->getDetailName();

			iter = g_detailLoadFailedMap.find( detailName );
			if ( iter != g_detailLoadFailedMap.end() )
				return;

			iter = g_detailNameToIdMap.find( detailName );
			if ( iter == g_detailNameToIdMap.end() )
			{
				detailId = loadTextureFile( detailName.c_str() );
				if ( detailId != -1 )
				{
					g_detailVector[idx]->setOglTextureId( diffuseId );
					g_detailVector[idx]->setOglDetailId( detailId );
					g_idMap.insert( pair< int, DetailMap * >( diffuseId, g_detailVector[idx] ) );
					g_detailNameToIdMap.insert( pair< string, int >( detailName, detailId ) );
				}
				else
				{
					Con_Printf( "Detail texture map load failed: %s\n", detailName.c_str() );
					g_detailLoadFailedMap.insert( pair< string, int >( detailName, 0 ) );
				}
			}
			else
			{
				g_detailVector[idx]->setOglTextureId( diffuseId );
				g_detailVector[idx]->setOglDetailId( iter->second );
				g_idMap.insert( pair< int, DetailMap * >( diffuseId, g_detailVector[idx] ) );
			}
			break;
		}
	}
}

/*
================
loadTextureFile
================
*/
static GLint loadTextureFile( const char *filename )
{
	int width, height;
	byte *data = (byte *)malloc( 1024 * 1024 );
	GLint txID = -1;
	char newFileName[128];

	sprintf( newFileName, "gfx/%s.tga", filename );

	if ( data && LoadTGA2( newFileName, data, 1024 * 1024, &width, &height, FALSE ) )
		txID = GL_LoadTexture2( (char *)filename, GLT_SPRITE, width, height, data, TRUE, 4, NULL, GL_LINEAR_MIPMAP_NEAREST );

	if ( data )
		free( data );

	return txID;
}

/*
================
DT_SetRenderState
================
*/
int DT_SetRenderState( int diffuseId )
{
	if ( !detTexSupported || r_detailtextures.value == 0 )
		return 0;

	if ( !g_detTexLoaded )
	{
		Con_Printf( "Loading Detail Textures...\n" );

		g_demandLoad = true;
		DT_LoadDetailMapFile( (char *)g_levelName.c_str() );

		for ( int i = 0; i < g_decalTexIDs.size(); i++ )
		{
			pair< string, int > p = g_decalTexIDs[i];
			DT_LoadDetailTexture( (char *)p.first.c_str(), p.second );
		}

		g_detTexLoaded = true;
		g_demandLoad = false;
	}

	DetailMap *pDetail = NULL;
	map< int, DetailMap * >::iterator iter = g_idMap.find( diffuseId );

	if ( iter != g_idMap.end() )
	{
		pDetail = iter->second;
		if ( !pDetail->getOglDetailId() || !pDetail->getOglTextureId() )
		{
			pDetail = NULL;
			return 0;
		}
	}
	else
	{
		return 0;
	}

	qglSelectTextureSGIS( TEXTURE2_SGIS );
	qglEnable( GL_TEXTURE_2D );

	bool bind = true;
	bool scale = true;

	if ( lastDetTex != -1 && pDetail->getOglDetailId() == lastDetTex )
	{
		bind = false;
	}
	else
	{
		lastDetTex = pDetail->getOglDetailId();
	}

	if ( pDetail->getSScale() == lastDetSScale && pDetail->getTScale() == lastDetTScale )
	{
		scale = false;
	}
	else
	{
		lastDetSScale = pDetail->getSScale();
		lastDetTScale = pDetail->getTScale();
	}

	if ( bind )
		qglBindTexture( GL_TEXTURE_2D, pDetail->getOglDetailId() );

	if ( scale )
	{
		qglMatrixMode( GL_TEXTURE );
		qglLoadIdentity();
		qglScalef( pDetail->getSScale(), pDetail->getTScale(), 1.0 );
		qglMatrixMode( GL_MODELVIEW );
	}

	return 1;
}

/*
================
DT_SetTextureCoordinates
================
*/
void DT_SetTextureCoordinates( float u, float v )
{
	qglMTexCoord2fSGIS( TEXTURE2_SGIS, u, v );
}

/*
================
DT_ClearRenderState
================
*/
void DT_ClearRenderState( void )
{
	qglSelectTextureSGIS( TEXTURE2_SGIS );
	qglDisable( GL_TEXTURE_2D );
	qglSelectTextureSGIS( TEXTURE1_SGIS );
}
