// Cry of Fear texture manager (no build records the name of this file)

#include <string>
#include <list>

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "triangleapi.h"
#include "studio_util.h"
#include "r_studioint.h"
#include "glmanager.h"
#include "TextureManager_guess.h"
#include <io.h>

//=========================================================
// Declarations this file takes from DevIL (IL/il.h)
//=========================================================
typedef unsigned int	ILenum;
typedef unsigned char	ILboolean;
typedef unsigned int	ILuint;
typedef int				ILint;
typedef unsigned char	ILubyte;

#define IL_IMAGE_WIDTH		0x0DE4
#define IL_IMAGE_HEIGHT		0x0DE5
#define IL_IMAGE_BPP		0x0DE8
#define IL_IMAGE_FORMAT		0x0DEA

extern "C"
{
__declspec( dllimport ) void __stdcall ilInit( void );
__declspec( dllimport ) void __stdcall ilBindImage( ILuint Image );
__declspec( dllimport ) ILboolean __stdcall ilLoadImage( const char *FileName );
__declspec( dllimport ) ILubyte * __stdcall ilGetData( void );
__declspec( dllimport ) ILint __stdcall ilGetInteger( ILenum Mode );
}

extern engine_studio_api_t IEngineStudio;

// the image types DevIL loads
char szImageTypes_guess[][5] =
{
	"bmp", "cut", "dds", "gif", "ico", "cur", "jpg", "jpe", "jpeg", "lbm",
	"lif", "lmp", "mdl", "mng", "pcd", "pcx", "pic", "pix", "png", "pbm",
	"pgm", "ppm", "pnm", "psd", "pxr", "sgi", "bw", "rgb", "rgba", "tga",
	"tif", "tiff", "wal", "xpm"
};

CTextureManager_guess g_TextureManager_guess;

CTextureManager_guess::CTextureManager_guess()
{
}

CTextureManager_guess::~CTextureManager_guess()
{
	LevelInit_guess();
}

void CTextureManager_guess::CheckMipmapSupport_guess( void )
{
	const char *extensions = strdup( (const char *)glGetString( GL_EXTENSIONS ) );
	float version = atof( (const char *)glGetString( GL_VERSION ) );

	if ( isExtensionSupported_guess( "GL_SGIS_generate_mipmap", extensions ) || isExtensionSupported_guess( "GL_EXT_generate_mipmap", extensions ) )
	{
		m_iMipmapMode_guess = 2;
	}
	else if ( version >= 1.4 )
	{
		m_iMipmapMode_guess = 1;
	}
	else
	{
		gEngfuncs.Con_Printf( "MipMap extension not found\n" );
		m_iMipmapMode_guess = 0;
	}
}

void CTextureManager_guess::CreateTexture_guess( int width, int height, GLuint *texture, GLenum target, GLenum format )
{
	unsigned char *data = new unsigned char[width * height * 4];

	*texture = m_iNextTexture_guess++;

	glBindTexture( target, *texture );
	glTexParameteri( target, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameteri( target, GL_TEXTURE_WRAP_T, GL_REPEAT );
	glTexParameteri( target, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( target, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glCopyTexImage2D( target, 0, format, 0, 0, width, height, 0 );

	texture_t_guess tex;
	tex.type_guess = TEXTYPE_IMAGE_guess;
	tex.name_guess = m_iNextTexture_guess;
	tex.iFound_guess = 0;
	tex.iFormat_guess = GL_RGBA;
	tex.texture_guess = *texture;
	m_Textures_guess.push_back( tex );

	delete[] data;
}

std::string CTextureManager_guess::GetPath_guess( const std::string &name, int type, int *found )
{
	std::string path;

	if ( type == TEXTYPE_SPRITE_guess )
	{
		path = "sprites/";
		path += name;
		path += ".spr";
	}
	else if ( IsHardware_guess() )
	{
		path = GetTexturePath_guess( name, found );
	}

	return path;
}

std::string CTextureManager_guess::GetTexturePath_guess( const std::string &name, int *found )
{
	std::string dir;
	std::string mapname;
	std::string result;
	std::string file;

	dir = gEngfuncs.pfnGetGameDirectory();
	dir += "/textures";

	mapname = gEngfuncs.pfnGetLevelName();
	mapname.erase( 0, 5 );
	mapname.erase( mapname.size() - 4, mapname.size() );

	result = ImportFrames_guess( name, found );

	if ( *found )
		return result;

	std::string base = name;
	base.erase( base.size() - 1, base.size() );

	file = dir + "/" + base + "/" + name;
	result = ImportImageFile_guess( file );

	if ( result.size() )
	{
		(*found)++;
		m_iNextTexture_guess++;
		return result;
	}

	for ( int i = 3; i < 6; i++ )
	{
		file = dir + "/" + mapname + "/";

		switch ( i )
		{
		case 0:
			file += "highest/";
			break;
		case 1:
			file += "high/";
			break;
		case 2:
			file += "medium/";
			break;
		case 3:
			file += "low/";
			break;
		case 5:
			file = dir + "/";
			break;
		}

		file += name;
		result = ImportImageFile_guess( file );

		if ( result.size() )
		{
			(*found)++;
			m_iNextTexture_guess++;
			return result;
		}
	}

	return "";
}

texture_t_guess *CTextureManager_guess::GetTexture_guess( const std::string &name )
{
	for ( std::list<texture_t_guess>::iterator it = m_Textures_guess.begin(); it != m_Textures_guess.end(); ++it )
	{
		if ( it->name_guess == name )
			return &*it;
	}

	return LoadTex_guess( name );
}

std::string CTextureManager_guess::ImportFrames_guess( const std::string &name, int *count )
{
	std::string path;
	std::string base;
	std::string result;
	char number[8];

	*count = 0;

	path = gEngfuncs.pfnGetGameDirectory();
	path += "/textures/";
	path += name;
	base = path;
	path += "/";

	if ( !_access( path.c_str(), 0 ) )
	{
		path += name;
		base = path;
	}

	path = base;
	sprintf( number, "%i", *count );
	path += number;
	path = ImportImageFile_guess( path );

	while ( path.size() )
	{
		texture_t_guess tex;

		(*count)++;
		tex.texture_guess = m_iNextTexture_guess++;
		tex.path_guess = path;
		result = path;
		LoadFile_guess( &tex );

		sprintf( number, "%i", *count );
		path = base;
		path += number;
		path = ImportImageFile_guess( path );
	}

	return result;
}

std::string CTextureManager_guess::ImportImageFile_guess( const std::string &path )
{
	ilBindImage( m_iNextTexture_guess );

	for ( int i = 0; i < 34; i++ )
	{
		std::string file = path;
		file = file + "." + szImageTypes_guess[i];

		if ( ilLoadImage( file.c_str() ) )
			return file;
	}

	return "";
}

bool CTextureManager_guess::IsHardware_guess( void )
{
	return IEngineStudio.IsHardware() == 1;
}

void CTextureManager_guess::LevelInit_guess( void )
{
	if ( IsHardware_guess() )
	{
		ilInit();
		CheckMipmapSupport_guess();
	}

	m_iNextTexture_guess = 2000;
	m_Textures_guess.clear();
}

void CTextureManager_guess::LoadFile_guess( texture_t_guess *tex )
{
	ilBindImage( tex->texture_guess );
	glBindTexture( GL_TEXTURE_2D, tex->texture_guess );

	if ( m_iMipmapMode_guess == 2 || m_iMipmapMode_guess == 1 )
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE );
	}
	else
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}

	ilLoadImage( tex->path_guess.c_str() );
	glTexImage2D( GL_TEXTURE_2D, 0, ilGetInteger( IL_IMAGE_BPP ), ilGetInteger( IL_IMAGE_WIDTH ), ilGetInteger( IL_IMAGE_HEIGHT ), 0, ilGetInteger( IL_IMAGE_FORMAT ), GL_UNSIGNED_BYTE, ilGetData() );
}

texture_t_guess *CTextureManager_guess::LoadFromName_guess( const char *name )
{
	return GetTexture_guess( std::string( name ) );
}

texture_t_guess *CTextureManager_guess::LoadImage_guess( const std::string &name )
{
	std::string path;
	int found = 0;

	path = GetPath_guess( name, TEXTYPE_IMAGE_guess, &found );

	if ( path.size() )
	{
		texture_t_guess tex;
		tex.type_guess = TEXTYPE_IMAGE_guess;
		tex.name_guess = name;
		tex.path_guess = path;
		tex.iFound_guess = found;
		tex.iFormat_guess = 0;
		tex.texture_guess = m_iNextTexture_guess - found;
		tex.pModel_guess = NULL;

		if ( found == 1 )
			LoadFile_guess( &tex );

		m_Textures_guess.push_back( tex );
		return &*--m_Textures_guess.end();
	}

	return NULL;
}

texture_t_guess *CTextureManager_guess::LoadSprite_guess( const std::string &name )
{
	std::string path = "sprites/";
	path += name;
	path += ".spr";

	texture_t_guess tex;
	tex.name_guess = name;
	tex.path_guess = path;
	tex.type_guess = TEXTYPE_SPRITE_guess;
	tex.iFormat_guess = 0;
	tex.pModel_guess = (struct model_s *)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( path.c_str() ) );

	if ( tex.pModel_guess )
	{
		if ( IsHardware_guess() )
		{
			GLint texture;
			gEngfuncs.pTriAPI->SpriteTexture( tex.pModel_guess, 0 );
			glGetIntegerv( GL_TEXTURE_BINDING_2D, &texture );
			tex.texture_guess = texture;
		}

		m_Textures_guess.push_back( tex );
		return &*--m_Textures_guess.end();
	}

	return NULL;
}

texture_t_guess *CTextureManager_guess::LoadTex_guess( const std::string &name )
{
	texture_t_guess *tex;

	if ( IsHardware_guess() && ( tex = LoadImage_guess( name ) ) != NULL )
		return tex;

	return LoadSprite_guess( name );
}

bool CTextureManager_guess::LoadTexture_guess( int texture, const char *name, int format, bool check )
{
	std::list<texture_t_guess>::iterator it = m_Textures_guess.begin();

	if ( check )
	{
		while ( it != m_Textures_guess.end() )
		{
			if ( (*it).texture_guess == texture )
				return true;
			it++;
		}
	}

	if ( IsHardware_guess() )
	{
		std::string path = name;
		int found = 0;

		path = GetPath_guess( path, TEXTYPE_IMAGE_guess, &found );

		if ( path.size() )
		{
			texture_t_guess tex;
			tex.type_guess = TEXTYPE_IMAGE_guess;
			tex.name_guess = name;
			tex.path_guess = path;
			tex.iFound_guess = found;
			tex.texture_guess = texture;
			tex.iFormat_guess = format;
			LoadFile_guess( &tex );
			m_Textures_guess.push_back( tex );

			return true;
		}
	}

	return false;
}

void CTextureManager_guess::PrecachePlayerShadow_guess( void )
{
	LoadFromName_guess( "playershadow" );
}

byte isExtensionSupported_guess( const char *extension, const char *extensions )
{
	int len = strlen( extensions ) - 1;
	int extlen = strlen( extension );

	for ( int i = 0; i < len; i++ )
	{
		int j = 0;

		while ( 1 )
		{
			int a = tolower( extensions[i] );
			int b = tolower( extension[j] );

			if ( b != a )
				break;

			if ( j == extlen - 1 )
				return true;

			i++;
			j++;
		}
	}

	return false;
}
