// draw.c -- this is the only file outside the refresh that touches the
// vid buffer

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "cmd.h"
#include "zone.h"
#include "mem.h"
#include "GL_MODEL.H"
#include "client.h"
#include "vid.h"
#include "render.h"
#include "GLQUAKE.H"
#include "gl_cvars.h"
#include "wad.h"
#include "draw.h"
#include "view.h"
#include "vgui_int.h"
#include "APIProxy.h"
#include "protocol.h"
#include "cdll_int.h"

int		gl_FilterTexture;

cvar_t	gl_nobind = { "gl_nobind", "0" };
cvar_t	gl_max_size = { "gl_max_size", "256" };
cvar_t	gl_round_down = { "gl_round_down", "3" };
cvar_t	gl_picmip = { "gl_picmip", "0" };
cvar_t	gl_palette_tex = { "gl_palette_tex", "1" };

qpic_t	*draw_disc;
qpic_t	*draw_backtile;

int		translate_texture;

int		gl_lightmap_format = GL_RGBA;

int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_max = GL_LINEAR;

int		texels;

typedef struct
{
	int		tag;
	int		referenceCount;
	byte	colors[768];
} GL_PALETTE;

GL_PALETTE	gGLPalette[350];

typedef struct
{
	int		texnum;
	float	sl, tl, sh, th;
} glpic_t;

typedef struct cachepic_s
{
	char		name[64];
	qpic_t		pic;
	byte		padding[32];	// for appended glpic
} cachepic_t;

#define	MAX_CACHED_PICS		16
cachepic_t	menu_cachepics[MAX_CACHED_PICS];
int			menu_numcachepics;

byte		menuplyr_pixels[4096];

int		pic_texels;
int		pic_count;

cachewad_t	custom_wad;

typedef struct
{
	int		texnum;
	short	servercount;
	short	paletteIndex;
	int		width, height;
	qboolean	mipmap;
	char	identifier[64];
} gltexture_t;

#define	MAX_GLTEXTURES	4800
gltexture_t	gltextures[MAX_GLTEXTURES];
int			numgltextures;

extern	cachewad_t	*menu_wad;
extern	qboolean	m_bDrawInitialized;
extern	cvar_t		con_color;
extern	char		decal_names[512][16];
extern	cvar_t		vid_d3d;
extern	cvar_t		console;
extern	unsigned short	*host_basepal;

void	Draw_CenterPic( qpic_t *pPic );
void	Download4444( void );
void	VideoMode_GetCurrentVideoMode( int *wide, int *tall, int *bpp );
void	GL_PaletteClear( int index );

extern	int		gHostSpawnCount;
extern	int		texture_extension_number_guess;
extern	cvar_t		fs_perf_warnings;
extern	qboolean	gfCustomBuild;
extern	char		szCustName[10];

qpic_t	*LoadTransPic( char *pszName, qpic_t *ppic );
void	GL_UnloadTexture( char *identifier );


void	GL_PaletteSelect( int paletteIndex );
void	GL_PaletteInit( void );
qpic_t	*LoadTransBMP( char *pszName );
int		GL_LoadPicTexture( qpic_t *pic, char *pszName );
void	Draw_MiptexTexture( cachewad_t *wad, byte *data );
void	Draw_CacheWadInit( char *name, int cacheMax, cachewad_t *wad );
void	Draw_CacheWadHandler( cachewad_t *wad, PFNCACHE fn, int extraDataSize );
int		Draw_CacheIndex( cachewad_t *wad, char *path );
void	*Draw_CacheGet( cachewad_t *wad, int index );
void	VGUI2_Draw_Init( void );
void	VGUI2_ResetCurrentTexture( void );
int		VGUI2_Draw_Character( int x, int y, int ch, unsigned int font );
int		VGUI2_Draw_CharacterAdd( int x, int y, int ch, int r, int g, int b, unsigned int font );
int		VGUI2_DrawString( int x, int y, const char *str, unsigned int font );
void	VGUI2_Draw_SetTextColor( int r, int g, int b );
int		VGUI2_Draw_StringLen( const char *psz, unsigned int font );

void	Draw_GetDefaultColor( void );

#define GL_SHARED_TEXTURE_PALETTE_EXT	0x81FB

/*
=============================================================================

  scrap allocation

  Allocate all the little status bar obejcts into a single texture
  to crutch up stupid hardware / drivers

=============================================================================
*/

qboolean IsPowerOfTwo( int value )
{
	int		i;

	for ( i = 0; i < 32; i++ )
	{
		if ( value == ( 1 << i ) )
			return true;
	}

	return false;
}

void GL_Bind( int texnum )
{
	int		paletteIndex;

	paletteIndex = ( texnum >> 16 ) - 1;
	texnum &= 0xFFFF;

	if ( currenttexture == texnum )
		return;
	currenttexture = texnum;

	qglBindTexture( GL_TEXTURE_2D, texnum );

	if ( paletteIndex >= 0 )
		GL_PaletteSelect( paletteIndex );
}

void GL_Texels_f( void )
{
	Con_Printf( "Current uploaded texels: %i\n", texels );
}

GLenum	oldtarget = 0x835E;	// TEXTURE0_SGIS

void GL_SelectTexture( GLenum target )
{
	if ( !gl_mtexable )
		return;

	qglSelectTextureSGIS( target );

	if ( target == oldtarget )
		return;

	cnttextures[oldtarget - TEXTURE0_SGIS] = currenttexture;
	currenttexture = cnttextures[target - TEXTURE0_SGIS];
	oldtarget = target;
}

//=============================================================================
/* Support Routines */

qpic_t *Draw_PicFromWad( char *name )
{
	qpic_t	*p;
	glpic_t	*gl;

	p = W_GetLumpName( 0, name );
	gl = (glpic_t *)p->data;

	gl->texnum = GL_LoadPicTexture( p, name );
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return p;
}

/*
================
Draw_CachePic
================
*/
qpic_t *Draw_CachePic( char *path )
{
	qpic_t	*ret;
	int		idx = 0;

	idx = Draw_CacheIndex( menu_wad, path );
	ret = Draw_CacheGet( menu_wad, idx );

	return ret;
}

int Draw_StringLen( const char *psz, unsigned int font )
{
	return VGUI2_Draw_StringLen( psz, font );
}

int Draw_MessageFontInfo( short *pWidth, unsigned int font )
{
	return VGUI2_GetFontTall( font );
}

int Draw_MessageCharacterAdd( int x, int y, int num, int rr, int gg, int bb, unsigned int font )
{
	return VGUI2_Draw_CharacterAdd( x, y, num, rr, gg, bb, font );
}

void Draw_CharToConback( int num, byte *dest )
{
}

typedef struct
{
	char	*name;
	int		minimize, maximize;
} quake_mode_t;

quake_mode_t modes[] =
{
	{ "GL_NEAREST", GL_NEAREST, GL_NEAREST },
	{ "GL_LINEAR", GL_LINEAR, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR }
};

/*
===============
Draw_TextureMode_f_guess
===============
*/
void Draw_TextureMode_f_guess( void )
{
	int		i;
	gltexture_t	*glt;

	if ( Cmd_Argc() == 1 )
	{
		for ( i = 0; i < 6; i++ )
			if ( gl_filter_min == modes[i].minimize )
			{
				Con_Printf( "%s\n", modes[i].name );
				return;
			}
		Con_Printf( "current filter is unknown???\n" );
		return;
	}

	for ( i = 0; i < 6; i++ )
	{
		if ( !Q_strcasecmp( modes[i].name, Cmd_Argv( 1 ) ) )
			break;
	}
	if ( i == 6 )
	{
		Con_Printf( "bad filter name\n" );
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for ( i = 0, glt = gltextures; i < numgltextures; i++, glt++ )
	{
		if ( glt->mipmap )
		{
			GL_Bind( glt->texnum );
			qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min );
			qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max );
		}
	}
}

/*
===============
Draw_Init
===============
*/
void Draw_Init( void )
{
	int		i;

	m_bDrawInitialized = true;
	VGUI2_Draw_Init();

	Cmd_AddCommand( "gl_texturemode", Draw_TextureMode_f_guess );
	Cmd_AddCommand( "gl_texels", GL_Texels_f );

	Cvar_RegisterVariable( &gl_nobind );
	Cvar_RegisterVariable( &gl_max_size );
	Cvar_RegisterVariable( &gl_round_down );
	Cvar_RegisterVariable( &gl_picmip );
	Cvar_RegisterVariable( &gl_palette_tex );

	GL_PaletteInit();

	menu_wad = Mem_ZeroMalloc( sizeof( cachewad_t ) );
	Draw_CacheWadInit( "cached.wad", 16, menu_wad );
	menu_wad->tempWad = 1;

	Draw_CacheWadHandler( &custom_wad, Draw_MiptexTexture, 32 );

	if ( qglColorTableEXT && gl_palette_tex.value )
		qglEnable( GL_SHARED_TEXTURE_PALETTE_EXT );

	Q_memset( decal_names, 0, sizeof( decal_names ) );

	for ( i = 0; i < 256; i++ )
		texgammatable[i] = i;

	draw_disc = LoadTransBMP( "lambda" );

	Draw_GetDefaultColor();
}

void Draw_ResetTextColor( void )
{
	Draw_GetDefaultColor();
}

void Draw_SetTextColor( float r, float g, float b )
{
	g_engdstAddrs.pfnDrawSetTextColor( &r, &g, &b );
	VGUI2_Draw_SetTextColor( r * 255, g * 255, b * 255 );
}

void Draw_GetDefaultColor( void )
{
	int		ir, ig, ib;
	int		num;

	num = sscanf( con_color.string, "%i %i %i", &ir, &ig, &ib );
	if ( num == 3 )
		VGUI2_Draw_SetTextColor( ir, ig, ib );
}

/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
int Draw_Character( int x, int y, int num, unsigned int font )
{
	return VGUI2_Draw_Character( x, y, num, font );
}

/*
================
Draw_String
================
*/
int Draw_String( int x, int y, char *str )
{
	int		wide;

	wide = VGUI2_DrawString( x, y, str, VGUI2_GetConsoleFont() );
	Draw_ResetTextColor();

	return wide + x;
}

void Draw_DebugChar( char num )
{
}

/*
=============
Draw_Pic
=============
*/
void Draw_Pic( int x, int y, qpic_t *pic )
{
	glpic_t	*gl;

	if ( !pic )
		return;

	VGUI2_ResetCurrentTexture();

	gl = (glpic_t *)pic->data;

	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
	qglDisable( GL_DEPTH_TEST );
	qglEnable( GL_ALPHA_TEST );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	qglColor4f( 1, 1, 1, 1 );
	GL_Bind( gl->texnum );
	qglBegin( GL_QUADS );
	qglTexCoord2f( gl->sl, gl->tl );
	qglVertex2f( x, y );
	qglTexCoord2f( gl->sh, gl->tl );
	qglVertex2f( x + pic->width, y );
	qglTexCoord2f( gl->sh, gl->th );
	qglVertex2f( x + pic->width, y + pic->height );
	qglTexCoord2f( gl->sl, gl->th );
	qglVertex2f( x, y + pic->height );
	qglEnd();
}

void Draw_AlphaSubPic( int xDest, int yDest, int xSrc, int ySrc, int iWidth, int iHeight, qpic_t *pPic, colorVec *pc, int iAlpha )
{
	glpic_t	*gl;
	float	flX, flY, flHeight, flWidth, alpha;

	if ( !pPic )
		return;

	VGUI2_ResetCurrentTexture();

	gl = (glpic_t *)pPic->data;

	qglBlendFunc( GL_ONE, GL_ONE );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	qglEnable( GL_BLEND );
	qglEnable( GL_ALPHA_TEST );

	flHeight = (float)iHeight / pPic->height;
	flWidth = (float)iWidth / pPic->width;
	flX = (float)xSrc / pPic->width;
	flY = (float)ySrc / pPic->height;

	alpha = iAlpha / 255.0;

	qglColor4f( pc->r * alpha / 256.0, pc->g * alpha / 256.0, pc->b * alpha / 256.0, 1.0 );

	GL_Bind( gl->texnum );
	qglBegin( GL_QUADS );
	qglTexCoord2f( flX, flY );
	qglVertex2f( xDest, yDest );
	qglTexCoord2f( flX + flWidth, flY );
	qglVertex2f( xDest + iWidth, yDest );
	qglTexCoord2f( flX + flWidth, flY + flHeight );
	qglVertex2f( xDest + iWidth, yDest + iHeight );
	qglTexCoord2f( flX, flY + flHeight );
	qglVertex2f( xDest, yDest + iHeight );
	qglEnd();

	qglDisable( GL_BLEND );
}

void Draw_AlphaPic( int x, int y, qpic_t *pic, colorVec *pc, int iAlpha )
{
	glpic_t	*gl;
	float	alpha;

	if ( !pic )
		return;

	VGUI2_ResetCurrentTexture();

	gl = (glpic_t *)pic->data;

	qglBlendFunc( GL_ONE, GL_ONE );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	qglEnable( GL_BLEND );
	qglEnable( GL_ALPHA_TEST );

	alpha = iAlpha / 255.0;

	if ( pc )
		qglColor3f( pc->r * alpha / 256.0, pc->g * alpha / 256.0, pc->b * alpha / 256.0 );
	else
		qglColor3f( alpha, alpha, alpha );

	GL_Bind( gl->texnum );
	qglBegin( GL_QUADS );
	qglTexCoord2f( gl->sl, gl->tl );
	qglVertex2f( x, y );
	qglTexCoord2f( gl->sh, gl->tl );
	qglVertex2f( x + pic->width, y );
	qglTexCoord2f( gl->sh, gl->th );
	qglVertex2f( x + pic->width, y + pic->height );
	qglTexCoord2f( gl->sl, gl->th );
	qglVertex2f( x, y + pic->height );
	qglEnd();

	qglDisable( GL_BLEND );
}

void Draw_AlphaAddPic( int x, int y, qpic_t *pic, colorVec *pc, int iAlpha )
{
	glpic_t	*gl;

	if ( !pic )
		return;

	VGUI2_ResetCurrentTexture();

	gl = (glpic_t *)pic->data;

	qglEnable( GL_TEXTURE_2D );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE );
	qglEnable( GL_BLEND );
	qglEnable( GL_ALPHA_TEST );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	if ( pc )
		qglColor4f( pc->r / 255.0, pc->g / 255.0, pc->b / 255.0, iAlpha / 255.0 );
	else
		qglColor4f( 1, 1, 1, iAlpha / 255.0 );

	GL_Bind( gl->texnum );
	qglBegin( GL_QUADS );
	qglTexCoord2f( gl->sl, gl->tl );
	qglVertex2f( x, y );
	qglTexCoord2f( gl->sh, gl->tl );
	qglVertex2f( x + pic->width, y );
	qglTexCoord2f( gl->sh, gl->th );
	qglVertex2f( x + pic->width, y + pic->height );
	qglTexCoord2f( gl->sl, gl->th );
	qglVertex2f( x, y + pic->height );
	qglEnd();

	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max );
}

void Draw_Pic2( int x, int y, int w, int h, qpic_t *pic )
{
	glpic_t	*gl;

	if ( !pic )
		return;

	VGUI2_ResetCurrentTexture();

	h--;

	gl = (glpic_t *)pic->data;

	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
	qglEnable( GL_ALPHA_TEST );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max );
	GL_Bind( gl->texnum );
	qglBegin( GL_QUADS );
	qglTexCoord2f( gl->sl, gl->tl );
	qglVertex2f( x, y );
	qglTexCoord2f( gl->sh, gl->tl );
	qglVertex2f( x + w, y );
	qglTexCoord2f( gl->sh, gl->th );
	qglVertex2f( x + w, y + h );
	qglTexCoord2f( gl->sl, gl->th );
	qglVertex2f( x, y + h );
	qglEnd();
}

/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic( int x, int y, qpic_t *pic )
{
	if ( !pic )
		return;

	if ( x < 0 || x + pic->width > vid.width || y < 0 || y + pic->height > vid.height )
	{
		Sys_Error( "Draw_TransPic: bad coordinates" );
	}

	Draw_Pic( x, y, pic );
}


/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
*/
void Draw_TransPicTranslate( int x, int y, qpic_t *pic, byte *translation )
{
	int				v, u, c;
	unsigned		trans[64*64], *dest;
	byte			*src;
	int				p;

	if ( !pic )
		return;

	VGUI2_ResetCurrentTexture();

	GL_Bind( translate_texture );

	c = pic->width * pic->height;

	dest = trans;
	for ( v = 0; v < 64; v++, dest += 64 )
	{
		src = &menuplyr_pixels[( ( v * pic->height ) >> 6 ) * pic->width];
		for ( u = 0; u < 64; u++ )
		{
			p = src[( u * pic->width ) >> 6];
			if ( p == 255 )
				dest[u] = p;
			else
				dest[u] = 0xff0000ff;
		}
	}

	qglTexImage2D( GL_TEXTURE_2D, 0, 4, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans );

	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	qglColor3f( 1, 1, 1 );
	qglBegin( GL_QUADS );
	qglTexCoord2f( 0, 0 );
	qglVertex2f( x, y );
	qglTexCoord2f( 1, 0 );
	qglVertex2f( x + pic->width, y );
	qglTexCoord2f( 1, 1 );
	qglVertex2f( x + pic->width, y + pic->height );
	qglTexCoord2f( 0, 1 );
	qglVertex2f( x, y + pic->height );
	qglEnd();
}

static int		scissor_x, scissor_y, scissor_width, scissor_height;
static qboolean	giScissorTest;

void EnableScissorTest( int x, int y, int width, int height )
{
	x = ( x > (int)vid.width ) ? vid.width : ( ( x < 0 ) ? 0 : x );
	y = ( y > (int)vid.height ) ? vid.height : ( ( y < 0 ) ? 0 : y );
	width = ( width > (int)( vid.width - x ) ) ? vid.width - x : ( ( width < 0 ) ? 0 : width );
	height = ( height > (int)( vid.height - y ) ) ? vid.height - y : ( ( height < 0 ) ? 0 : height );

	scissor_x = x;
	scissor_width = width;
	scissor_y = vid.height - y - height;
	scissor_height = height;

	giScissorTest = true;
}

void DisableScissorTest( void )
{
	scissor_x = 0;
	scissor_width = 0;
	scissor_y = 0;
	scissor_height = 0;

	giScissorTest = false;
}

int ValidateWRect( const wrect_t *prc )
{
	if ( !prc )
		return false;

	if ( ( prc->left >= prc->right ) || ( prc->top >= prc->bottom ) )
	{
		//!!!UNDONE Dev only warning msg
		return false;
	}

	return true;
}

int IntersectWRect( const wrect_t *prc1, const wrect_t *prc2, wrect_t *prc )
{
	wrect_t	rc;

	if ( !prc )
		prc = &rc;

	prc->left = max( prc1->left, prc2->left );
	prc->right = min( prc1->right, prc2->right );

	if ( prc->left < prc->right )
	{
		prc->top = max( prc1->top, prc2->top );
		prc->bottom = min( prc1->bottom, prc2->bottom );

		if ( prc->top < prc->bottom )
			return 1;
	}

	return 0;
}

void AdjustSubRect( mspriteframe_t *pFrame, float *pfLeft, float *pfRight, float *pfTop, float *pfBottom, int *pw, int *ph, const wrect_t *prcSubRect )
{
	wrect_t	rc;
	float	f;

	if ( !ValidateWRect( prcSubRect ) )
		return;

	// clip sub rect to sprite

	rc.top = rc.left = 0;
	rc.right = *pw;
	rc.bottom = *ph;

	if ( !IntersectWRect( prcSubRect, &rc, &rc ) )
		return;

	*pw = rc.right - rc.left;
	*ph = rc.bottom - rc.top;

	f = 1.0 / (float)pFrame->width;
	*pfLeft = ( (float)rc.left + 0.5 ) * f;
	*pfRight = ( (float)rc.right - 0.5 ) * f;

	f = 1.0 / (float)pFrame->height;
	*pfTop = ( (float)rc.top + 0.5 ) * f;
	*pfBottom = ( (float)rc.bottom - 0.5 ) * f;
}

void Draw_Frame( mspriteframe_t *pFrame, int ix, int iy, const wrect_t *prcSubRect )
{
	float	x = (float)ix;
	float	y = (float)iy;

	float	fLeft = 0;
	float	fRight = 1;
	float	fTop = 0;
	float	fBottom = 1;
	int		iWidth = pFrame->width;
	int		iHeight = pFrame->height;

	if ( !vid_d3d.value )
	{
		x += 0.5f;
		y += 0.5f;
	}

	VGUI2_ResetCurrentTexture();

	if ( giScissorTest )
	{
		qglScissor( scissor_x, scissor_y, scissor_width, scissor_height );
		qglEnable( GL_SCISSOR_TEST );
	}

	if ( prcSubRect )
	{
		AdjustSubRect( pFrame, &fLeft, &fRight, &fTop, &fBottom, &iWidth, &iHeight, prcSubRect );
	}

	qglDepthMask( 0 );

	GL_Bind( pFrame->gl_texturenum );

	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	qglBegin( GL_QUADS );
	qglTexCoord2f( fLeft, fTop );
	qglVertex2f( x, y );
	qglTexCoord2f( fRight, fTop );
	qglVertex2f( iWidth + x, y );
	qglTexCoord2f( fRight, fBottom );
	qglVertex2f( iWidth + x, iHeight + y );
	qglTexCoord2f( fLeft, fBottom );
	qglVertex2f( x, iHeight + y );
	qglEnd();

	qglDepthMask( 1 );
	qglDisable( GL_SCISSOR_TEST );
}

void Draw_SpriteFrame( mspriteframe_t *pFrame, unsigned short *pPalette, int x, int y, const wrect_t *prcSubRect )
{
	Draw_Frame( pFrame, x, y, prcSubRect );
}

void Draw_SpriteFrameHoles( mspriteframe_t *pFrame, unsigned short *pPalette, int x, int y, const wrect_t *prcSubRect )
{
	qglEnable( GL_ALPHA_TEST );

	if ( gl_spriteblend.value )
	{
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		qglEnable( GL_BLEND );
	}

	Draw_Frame( pFrame, x, y, prcSubRect );

	qglDisable( GL_ALPHA_TEST );
	qglDisable( GL_BLEND );
}

void Draw_SpriteFrameAdditive( mspriteframe_t *pFrame, unsigned short *pPalette, int x, int y, const wrect_t *prcSubRect )
{
	qglEnable( GL_BLEND );
	qglBlendFunc( GL_ONE, GL_ONE );

	Draw_Frame( pFrame, x, y, prcSubRect );

	qglDisable( GL_BLEND );
}

void Draw_SpriteFrameGeneric( mspriteframe_t *pFrame, unsigned short *pPalette, int x, int y, const wrect_t *prcSubRect, int src, int dest, int width, int height )
{
	int		oldWidth;
	int		oldHeight;

	oldWidth = pFrame->width;
	oldHeight = pFrame->height;

	pFrame->width = width;
	pFrame->height = height;

	qglEnable( GL_BLEND );
	qglBlendFunc( src, dest );

	Draw_Frame( pFrame, x, y, prcSubRect );

	qglDisable( GL_BLEND );

	pFrame->width = oldWidth;
	pFrame->height = oldHeight;
}

/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground( int lines )
{
	char	ver[100];
	int		x;

	{
		int		x, y, w, h;

		x = 0;
		y = lines - glheight;
		w = glwidth;
		h = glheight + 1;
		h--;

		qglDisable( GL_TEXTURE_2D );
		qglDisable( GL_BLEND );
		qglDisable( GL_ALPHA_TEST );
		qglColor4f( 0, 0, 0, 1 );
		qglBegin( GL_QUADS );
		qglVertex2f( x, y );
		qglVertex2f( x + w, y );
		qglVertex2f( x + w, y + h );
		qglVertex2f( x, y + h );
		qglEnd();
		qglEnable( GL_TEXTURE_2D );
		qglEnable( GL_ALPHA_TEST );
	}

	_snprintf( ver, sizeof( ver ), "Half-Life %i/%s (hw build %d)", PROTOCOL_VERSION, gpszVersionString, build_number() );
	x = vid.conwidth - Draw_StringLen( ver, VGUI2_GetConsoleFont() );

	if ( console.value && !( giSubState & 4 ) )
		Draw_String( x, 0, ver );
}

void Draw_FillRGBA( int x, int y, int w, int h, int r, int g, int b, int a )
{
	g_engdstAddrs.pfnFillRGBA( &x, &y, &w, &h, &r, &g, &b, &a );

	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_BLEND );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE );

	qglColor4f( r / 255.0, g / 255.0, b / 255.0, a / 255.0 );

	qglBegin( GL_QUADS );
	qglVertex2f( x, y );
	qglVertex2f( x + w, y );
	qglVertex2f( x + w, y + h );
	qglVertex2f( x, y + h );
	qglEnd();

	qglColor3f( 1, 1, 1 );
	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
}

void Draw_FillRGBABlend( int x, int y, int w, int h, int r, int g, int b, int a )
{
	g_engdstAddrs.pfnFillRGBA( &x, &y, &w, &h, &r, &g, &b, &a );

	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_BLEND );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	qglColor4f( r / 255.0, g / 255.0, b / 255.0, a / 255.0 );

	qglBegin( GL_QUADS );
	qglVertex2f( x, y );
	qglVertex2f( x + w, y );
	qglVertex2f( x + w, y + h );
	qglVertex2f( x, y + h );
	qglEnd();

	qglColor3f( 1, 1, 1 );
	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear( int x, int y, int w, int h )
{
	byte	unused_guess[88];

	Draw_FillRGBA( x, y, w, h, 0, 0, 0, 255 );
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill( int x, int y, int w, int h, int c )
{
	Draw_FillRGBA( x, y, w, h, host_basepal[c * 4], host_basepal[c * 4 + 1], host_basepal[c * 4 + 2], 255 );
}
//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen( void )
{
	qglEnable( GL_BLEND );
	qglDisable( GL_TEXTURE_2D );
	qglColor4f( 0, 0, 0, 0.8 );
	qglBegin( GL_QUADS );

	qglVertex2f( 0, 0 );
	qglVertex2f( glwidth, 0 );
	qglVertex2f( glwidth, glheight );
	qglVertex2f( 0, glheight );

	qglEnd();
	qglColor4f( 1, 1, 1, 1 );
	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
}

//=============================================================================

/*
================
Draw_BeginDisc

Draws the little blue disc in the corner of the screen.
Call before beginning any disc IO.
================
*/
void Draw_BeginDisc( void )
{
	if ( !draw_disc )
		return;

	Draw_CenterPic( draw_disc );
}


/*
================
Draw_EndDisc

Erases the disc icon.
Call after completing any disc IO
================
*/
void Draw_EndDisc( void )
{
}

void ComputeScaledSize( int *wscale, int *hscale, int width, int height )
{
	int		scaled_width, scaled_height;
	int		max_size;

	max_size = max( gl_max_size.value, 128 );

	for ( scaled_width = 1; scaled_width < width; scaled_width <<= 1 )
		;

	if ( gl_round_down.value > 0 && scaled_width > width &&
		( gl_round_down.value == 1 || ( scaled_width - width ) > ( scaled_width >> (int)gl_round_down.value ) ) )
		scaled_width >>= 1;

	for ( scaled_height = 1; scaled_height < height; scaled_height <<= 1 )
		;

	if ( gl_round_down.value > 0 && scaled_height > height &&
		( gl_round_down.value == 1 || ( scaled_height - height ) > ( scaled_height >> (int)gl_round_down.value ) ) )
		scaled_height >>= 1;

	scaled_width >>= (int)gl_picmip.value;
	scaled_height >>= (int)gl_picmip.value;

	if ( scaled_width > max_size )
		scaled_width = max_size;
	if ( scaled_height > max_size )
		scaled_height = max_size;

	if ( wscale )
		*wscale = scaled_width;
	if ( hscale )
		*hscale = scaled_height;
}

/*
================
GL_FindTexture
================
*/
int GL_FindTexture( char *identifier )
{
	int		i;
	gltexture_t	*glt;

	for ( i = 0, glt = gltextures; i < numgltextures; i++, glt++ )
	{
		if ( !Q_strcmp( identifier, glt->identifier ) )
			return gltextures[i].texnum;
	}

	return -1;
}

/*
================
GL_ResampleTexture
================
*/
void GL_ResampleTexture( unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight )
{
	int		i, j;
	unsigned	*inrow, *inrow2;
	unsigned	frac, fracstep;
	unsigned	p1[1024], p2[1024];
	byte		*pix1, *pix2, *pix3, *pix4;

	fracstep = inwidth * 0x10000 / outwidth;

	frac = fracstep >> 2;
	for ( i = 0; i < outwidth; i++ )
	{
		p1[i] = 4 * ( frac >> 16 );
		frac += fracstep;
	}
	frac = 3 * ( fracstep >> 2 );
	for ( i = 0; i < outwidth; i++ )
	{
		p2[i] = 4 * ( frac >> 16 );
		frac += fracstep;
	}

	for ( i = 0; i < outheight; i++, out += outwidth )
	{
		inrow = in + inwidth * (int)( ( i + 0.25 ) * inheight / outheight );
		inrow2 = in + inwidth * (int)( ( i + 0.75 ) * inheight / outheight );
		frac = fracstep >> 1;
		for ( j = 0; j < outwidth; j++ )
		{
			pix1 = (byte *)inrow + p1[j];
			pix2 = (byte *)inrow + p2[j];
			pix3 = (byte *)inrow2 + p1[j];
			pix4 = (byte *)inrow2 + p2[j];
			( (byte *)( out + j ) )[0] = ( pix1[0] + pix2[0] + pix3[0] + pix4[0] ) >> 2;
			( (byte *)( out + j ) )[1] = ( pix1[1] + pix2[1] + pix3[1] + pix4[1] ) >> 2;
			( (byte *)( out + j ) )[2] = ( pix1[2] + pix2[2] + pix3[2] + pix4[2] ) >> 2;
			( (byte *)( out + j ) )[3] = ( pix1[3] + pix2[3] + pix3[3] + pix4[3] ) >> 2;
		}
	}
}

/*
================
GL_ResampleAlphaTexture
================
*/
void GL_ResampleAlphaTexture( byte *in, int inwidth, int inheight, byte *out, int outwidth, int outheight )
{
	int		i, j;
	byte	*inrow, *inrow2;
	unsigned	frac, fracstep;
	byte	p1[1024], p2[1024];
	byte	*pix1, *pix2, *pix3, *pix4;

	fracstep = inwidth * 0x10000 / outwidth;

	frac = fracstep >> 2;
	for ( i = 0; i < outwidth; i++ )
	{
		p1[i] = frac >> 16;
		frac += fracstep;
	}
	frac = 3 * ( fracstep >> 2 );
	for ( i = 0; i < outwidth; i++ )
	{
		p2[i] = frac >> 16;
		frac += fracstep;
	}

	for ( i = 0; i < outheight; i++, out += outwidth )
	{
		inrow = in + inwidth * (int)( ( i + 0.25 ) * inheight / outheight );
		inrow2 = in + inwidth * (int)( ( i + 0.75 ) * inheight / outheight );
		frac = fracstep >> 1;
		for ( j = 0; j < outwidth; j++ )
		{
			pix1 = inrow + p1[j];
			pix2 = inrow + p2[j];
			pix3 = inrow2 + p1[j];
			pix4 = inrow2 + p2[j];
			( out + j )[0] = ( pix1[0] + pix2[0] + pix3[0] + pix4[0] ) >> 2;
			( out + j )[1] = ( pix1[1] + pix2[1] + pix3[1] + pix4[1] ) >> 2;
			( out + j )[2] = ( pix1[2] + pix2[2] + pix3[2] + pix4[2] ) >> 2;
			( out + j )[3] = ( pix1[3] + pix2[3] + pix3[3] + pix4[3] ) >> 2;
		}
	}
}

void GL_ResampleTexturePoint( byte *in, int inwidth, int inheight, byte *out, int outwidth, int outheight )
{
	int		i, j;
	unsigned	ufrac, vfrac;
	unsigned	ufracstep, vfracstep;
	byte	*src, *dest;

	ufracstep = inwidth * 0x10000 / outwidth;
	vfracstep = inheight * 0x10000 / outheight;

	src = in;
	dest = out;

	vfrac = vfracstep >> 2;
	for ( i = 0; i < outheight; i++, out += outwidth )
	{
		ufrac = ufracstep >> 2;
		for ( j = 0; j < outwidth; j++ )
		{
			*dest = src[ufrac >> 16];
			dest++;
			ufrac += ufracstep;
		}
		vfrac += vfracstep;
		src += inwidth * ( vfrac >> 16 );
		vfrac &= 0xFFFF;
	}
}

/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
void GL_MipMap( byte *in, int width, int height )
{
	int		i, j;
	byte	*out;

	width <<= 2;
	height >>= 1;
	out = in;
	for ( i = 0; i < height; i++, in += width )
	{
		for ( j = 0; j < width; j += 8, out += 4, in += 8 )
		{
			out[0] = ( in[0] + in[4] + in[width+0] + in[width+4] ) >> 2;
			out[1] = ( in[1] + in[5] + in[width+1] + in[width+5] ) >> 2;
			out[2] = ( in[2] + in[6] + in[width+2] + in[width+6] ) >> 2;
			out[3] = ( in[3] + in[7] + in[width+3] + in[width+7] ) >> 2;
		}
	}
}

void BoxFilter3x3( byte *out, byte *in, int w, int h, int x, int y )
{
	int		i, j, a, r, g, b, count, acount, u, v;
	byte	*pixel;

	acount = count = 0;
	a = r = g = b = 0;

	for ( i = -1; i < 2; i++ )
	{
		for ( j = -1; j < 2; j++ )
		{
			u = x + i;
			v = y + j;

			if ( u >= 0 && u < w && v >= 0 && v < h )
			{
				pixel = &in[( v * w + u ) * 4];
				acount++;

				if ( pixel[3] != 0 )
				{
					b += pixel[0];
					g += pixel[1];
					r += pixel[2];
					a += pixel[3];
					count++;
				}
			}
		}
	}

	if ( count == 0 )
		count = 1;
	if ( acount == 0 )
		acount = 1;

	out[0] = b / count;
	out[1] = g / count;
	out[2] = r / count;
	out[3] = 0;
}

int		giTotalTextures;
int		giTotalTexBytes;

/*
===============
GL_Upload32
===============
*/
void GL_Upload32( unsigned *data, int width, int height, qboolean mipmap, int iType, int filter )
{
	int			iComponent, iFormat, i;
	static unsigned	scaled[1024*512];	// [512*256];
	int			scaled_width, scaled_height;
	qboolean	f4444;
	int			w, h, bpp;

	f4444 = false;
	VideoMode_GetCurrentVideoMode( &w, &h, &bpp );

	giTotalTexBytes += width * height;

	if ( vid_d3d.value && ( iType == 1 || iType == 3 || iType == 4 ) && bpp == 16 )
		f4444 = true;

	if ( iType != 2 )
		giTotalTexBytes += width * 2 * height;

	giTotalTextures++;

	if ( gl_spriteblend.value && ( iType == 1 || iType == 3 || iType == 4 ) )
	{
		for ( i = 0; i < width * height; i++ )
		{
			if ( data[i] == 0 )
				BoxFilter3x3( (byte *)&data[i], (byte *)data, width, height, i % width, i / width );
		}
	}

	ComputeScaledSize( &scaled_width, &scaled_height, width, height );

	if ( scaled_width * scaled_height > sizeof( scaled ) / 4 )
		Sys_Error( "GL_LoadTexture: too big" );

	switch ( iType )
	{
	case 0:
		if ( bpp == 16 )
			iComponent = 3;
		else
			iComponent = GL_RGBA8;
		iFormat = GL_RGBA;
		break;
	case 1:
	case 3:
		if ( bpp == 16 )
			iComponent = GL_RGBA4;
		else
			iComponent = GL_RGBA8;
		iFormat = GL_RGBA;
		break;
	case 4:
		if ( bpp == 16 )
			iComponent = GL_RGB5_A1;
		else
			iComponent = GL_RGBA8;
		iFormat = GL_RGBA;
		break;
	case 2:
		iComponent = 1;
		iFormat = GL_LUMINANCE;
		break;
	}

	if ( scaled_width == width && scaled_height == height )
	{
		int		iNum = width * height;

		if ( !mipmap )
		{
			if ( f4444 )
				Download4444();
			qglTexImage2D( GL_TEXTURE_2D, 0, iComponent, scaled_width, scaled_height, 0, iFormat, GL_UNSIGNED_BYTE, data );
			goto done;
		}

		if ( iType != 2 )
			iNum *= 4;

		Q_memcpy( scaled, data, iNum );
	}
	else if ( iType == 2 )
		GL_ResampleAlphaTexture( (byte *)data, width, height, (byte *)scaled, scaled_width, scaled_height );
	else
		GL_ResampleTexture( data, width, height, scaled, scaled_width, scaled_height );

	texels += scaled_width * scaled_height;

	if ( f4444 )
		Download4444();
	qglTexImage2D( GL_TEXTURE_2D, 0, iComponent, scaled_width, scaled_height, 0, iFormat, GL_UNSIGNED_BYTE, scaled );

	if ( mipmap )
	{
		int		miplevel;

		miplevel = 0;
		while ( scaled_width > 1 || scaled_height > 1 )
		{
			GL_MipMap( (byte *)scaled, scaled_width, scaled_height );
			scaled_width >>= 1;
			scaled_height >>= 1;
			if ( scaled_width < 1 )
				scaled_width = 1;
			if ( scaled_height < 1 )
				scaled_height = 1;
			miplevel++;
			texels += scaled_width * scaled_height;
			if ( f4444 )
				Download4444();
			qglTexImage2D( GL_TEXTURE_2D, miplevel, iComponent, scaled_width, scaled_height, 0, iFormat, GL_UNSIGNED_BYTE, scaled );
		}
	}
done:;

	if ( mipmap )
	{
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max );
	}
	else
	{
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter );
	}
}

/*
===============
GL_Upload16
===============
*/
void GL_Upload16( byte *data, int width, int height, qboolean mipmap, int iType, byte *pPal, int filter )
{
	static unsigned	trans[640*480];		// FIXME, temporary
	int			i, s;
	qboolean	noalpha;
	int			p;
	byte		*pb;

	s = width * height;
	if ( s > sizeof( trans ) )
	{
		Con_Printf( "Can't upload (%ix%i) texture, it's > 640*480 bytes\n", width, height );
		return;
	}

	if ( iType != 2 )
	{
		if ( !pPal )
			return;

		pb = pPal;
		for ( i = 0; i < 768; i++ )
			pPal[i] = texgammatable[pPal[i]];
	}

	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if ( iType == 1 || iType == 3 || iType == 4 )
	{
		noalpha = true;
		for ( i = 0; i < s; i++ )
		{
			p = data[i];

			if ( iType == 3 )
			{
				trans[i] = *(unsigned *)&pPal[765] & 0x00ffffff;
				trans[i] |= p << 24;
				noalpha = false;
			}
			else if ( iType == 4 )
			{
				trans[i] = *(unsigned *)&pPal[p * 3] & 0x00ffffff;
				trans[i] |= p << 24;
				noalpha = false;
			}
			else if ( p == 255 )
			{
				noalpha = false;
				trans[i] = 0;
			}
			else
			{
				trans[i] = *(unsigned *)&pPal[p * 3] & 0x00ffffff;
				trans[i] |= 0xff000000;
			}
		}

		if ( noalpha )
			iType = 0;
	}
	else if ( iType == 0 )
	{
		if ( s & 3 )
			Sys_Error( "GL_Upload16: s&3" );

		if ( gl_dither.value )
		{
			for ( i = 0; i < s; i++ )
			{
				byte	r, g, b, *ppix;

				ppix = &pPal[data[i] * 3];

				ppix[0] |= ( ppix[0] & 0xC0 ) >> 6;
				r = ppix[0];
				ppix[1] |= ( ppix[1] & 0xC0 ) >> 6;
				g = ppix[1];
				ppix[2] |= ( ppix[2] & 0xC0 ) >> 6;
				b = ppix[2];

				trans[i] = r | ( g << 8 ) | ( b << 16 ) | 0xff000000;
			}
		}
		else
		{
			for ( i = 0; i < s; i += 4 )
			{
				trans[i] = ( *(unsigned *)&pPal[data[i] * 3] & 0x00ffffff ) | 0xff000000;
				trans[i+1] = ( *(unsigned *)&pPal[data[i+1] * 3] & 0x00ffffff ) | 0xff000000;
				trans[i+2] = ( *(unsigned *)&pPal[data[i+2] * 3] & 0x00ffffff ) | 0xff000000;
				trans[i+3] = ( *(unsigned *)&pPal[data[i+3] * 3] & 0x00ffffff ) | 0xff000000;
			}
		}
	}
	else if ( iType == 2 )
	{
		Q_memcpy( trans, data, width * height );
	}
	else
	{
		Con_Printf( "Upload16:Bogus texture type!/n" );
	}

	GL_Upload32( trans, width, height, mipmap, iType, filter );
}

void GL_FreeGLTex( gltexture_t *glt )
{
	int		texnum;

	texnum = glt->texnum;
	qglDeleteTextures( 1, &glt->texnum );

	if ( glt->paletteIndex >= 0 )
	{
		if ( gGLPalette[glt->paletteIndex].referenceCount > 1 )
			gGLPalette[glt->paletteIndex].referenceCount--;
		else
			GL_PaletteClear( glt->paletteIndex );
	}

	Q_memset( glt, 0, sizeof( *glt ) );
	glt->texnum = texnum;
	glt->servercount = -1;
}

void GL_UnloadTextures( void )
{
	int		i;
	gltexture_t	*glt;

	for ( i = 0, glt = gltextures; i < numgltextures; i++, glt++ )
	{
		if ( glt->servercount > 0 && glt->servercount != gHostSpawnCount )
			GL_FreeGLTex( glt );
	}
}

void GL_UnloadTexture( char *identifier )
{
	int		i;
	gltexture_t	*glt;

	for ( i = 0, glt = gltextures; i < numgltextures; i++, glt++ )
	{
		if ( !Q_strcmp( identifier, glt->identifier ) )
		{
			if ( glt->servercount >= 0 )
				GL_FreeGLTex( glt );
			return;
		}
	}
}

void GL_PaletteInit( void )
{
	int		i;

	for ( i = 0; i < 350; i++ )
		GL_PaletteClear( i );
}

void GL_PaletteClear( int index )
{
	if ( index >= 0 && index < 350 )
	{
		gGLPalette[index].tag = -1;
		gGLPalette[index].referenceCount = 0;
	}
}

int GL_PaletteTag( byte *pPal )
{
	int		tag, i;

	tag = pPal[0];
	for ( i = 1; i < 768; i++ )
	{
		tag += pPal[i];
		tag ^= pPal[i-1];
	}

	if ( tag < 0 )
		tag = -tag;

	return tag;
}

int GL_PaletteEqual( byte *pPal1, int tag1, byte *pPal2, int tag2 )
{
	int		i;

	if ( tag1 != tag2 )
		return false;

	for ( i = 0; i < 768; i++ )
	{
		if ( pPal1[i] != pPal2[i] )
			return false;
	}

	return true;
}

void GL_PaletteClearSky( void )
{
	int		i;

	for ( i = 344; i < 350; i++ )
	{
		gGLPalette[i].tag = -1;
		gGLPalette[i].referenceCount = 0;
	}
}

short GL_PaletteAdd( byte *pPal, qboolean isSky )
{
	if ( qglColorTableEXT )
	{
		int		i, tag, limit;
		byte	gammaPal[768];

		for ( i = 0; i < 768; i++ )
			gammaPal[i] = texgammatable[pPal[i]];

		tag = GL_PaletteTag( gammaPal );

		i = 0;
		limit = 350;
		if ( !isSky )
			limit -= 6;
		else
			i = 344;

		for ( ; i < limit; i++ )
		{
			if ( gGLPalette[i].tag < 0 )
			{
				Q_memcpy( gGLPalette[i].colors, gammaPal, 768 );
				gGLPalette[i].tag = tag;
				gGLPalette[i].referenceCount = 1;
				return i;
			}

			if ( GL_PaletteEqual( gammaPal, tag, gGLPalette[i].colors, gGLPalette[i].tag ) )
			{
				gGLPalette[i].referenceCount++;
				return i;
			}
		}
	}

	return -1;
}

static int	g_currentpalette = -1;

void GL_PaletteSelect( int paletteIndex )
{
	if ( g_currentpalette == paletteIndex || !qglColorTableEXT )
		return;

	g_currentpalette = paletteIndex;
	qglColorTableEXT( GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGB, 256, GL_RGB, GL_UNSIGNED_BYTE, gGLPalette[paletteIndex].colors );
}

GLint GL_GenTexture( void )
{
	return texture_extension_number_guess++;
}

/*
================
GL_LoadTexture
================
*/
int GL_LoadTexture2( char *identifier, GL_TEXTURETYPE textureType, int width, int height, byte *data, qboolean mipmap, int iType, byte *pPal, int filter )
{
	int			i;
	int			scaled_width, scaled_height;
	gltexture_t	*glt, *slot;
	BOOL		mustRescale;

	mustRescale = false;
	slot = NULL;

retry:
	// see if the texture is allready present
	if ( identifier[0] )
	{
		for ( i = 0, glt = gltextures; i < numgltextures; i++, glt++ )
		{
			if ( glt->servercount < 0 )
			{
				if ( !slot )
					slot = glt;
				continue;
			}

			if ( !Q_strcmp( identifier, glt->identifier ) )
			{
				if ( width != glt->width || height != glt->height )
				{
					identifier[3]++;	// texture with the same name but another size, rename it
					goto retry;
				}

				if ( glt->servercount > 0 )
					glt->servercount = gHostSpawnCount;

				return ( glt->paletteIndex >= 0 ) ? ( ( ( glt->paletteIndex + 1 ) << 16 ) | glt->texnum ) : glt->texnum;
			}
		}
	}
	else
	{
		Con_DPrintf( "NULL Texture\n" );
	}

	if ( !slot )
	{
		glt = &gltextures[numgltextures];
		numgltextures++;

		if ( numgltextures >= MAX_GLTEXTURES )
			Sys_Error( "Texture Overflow: MAX_GLTEXTURES" );

		glt->texnum = texture_extension_number_guess++;
	}
	else
	{
		glt = slot;
	}

	Q_strncpy( glt->identifier, identifier, sizeof( glt->identifier ) - 1 );
	glt->identifier[sizeof( glt->identifier ) - 1] = 0;
	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;

	if ( textureType != GLT_WORLD )
		glt->servercount = 0;
	else
		glt->servercount = gHostSpawnCount;

	glt->paletteIndex = -1;

	GL_Bind( glt->texnum );
	currenttexture = -1;

	ComputeScaledSize( &scaled_width, &scaled_height, width, height );

	if ( scaled_width != width || scaled_height != height )
		mustRescale = true;

	if ( !mipmap && ( !mustRescale || ( scaled_width <= 128 && scaled_height <= 128 ) ) )
	{
		byte	*pTexture;
		byte	scaled[128*128];

		if ( mustRescale )
		{
			if ( fs_perf_warnings.value )
				Con_DPrintf( "fs_perf_warnings: resampling non-power-of-2 texture %s (%dx%d)\n", identifier, width, height );

			GL_ResampleTexturePoint( data, width, height, scaled, scaled_width, scaled_height );
			pTexture = scaled;
		}
		else
		{
			pTexture = data;
		}

		if ( qglColorTableEXT && gl_palette_tex.value && !iType )
		{
			glt->paletteIndex = GL_PaletteAdd( pPal, false );

			if ( glt->paletteIndex >= 0 )
			{
				qglTexImage2D( GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, pTexture );
				qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter );
				qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter );

				return ( glt->paletteIndex >= 0 ) ? ( ( ( glt->paletteIndex + 1 ) << 16 ) | glt->texnum ) : glt->texnum;
			}
		}
	}

	if ( g_modfuncs.m_pfnTextureLoad )
		g_modfuncs.m_pfnTextureLoad( identifier, width, height, data );

	if ( textureType == GLT_SPRITE && iType == 4 )
		GL_Upload32( (unsigned *)data, width, height, mipmap, iType, filter );
	else
		GL_Upload16( data, width, height, mipmap, iType, pPal, filter );

	return ( glt->paletteIndex >= 0 ) ? ( ( ( glt->paletteIndex + 1 ) << 16 ) | glt->texnum ) : glt->texnum;
}

int GL_LoadTexture( char *identifier, GL_TEXTURETYPE textureType, int width, int height, byte *data, int mipmap, int iType, byte *pPal )
{
	return GL_LoadTexture2( identifier, textureType, width, height, data, mipmap, iType, pPal, gl_filter_max );
}

int GL_LoadPicTexture( qpic_t *pic, char *pszName )
{
	byte	*pPal;

	pPal = &pic->data[pic->width * pic->height + 2];

	return GL_LoadTexture( pszName, GLT_SYSTEM, pic->width, pic->height, pic->data, false, 1, pPal );
}

qpic_t *LoadTransBMP( char *pszName )
{
	return LoadTransPic( pszName, W_GetLumpName( 0, pszName ) );
}

qpic_t *LoadTransPic( char *pszName, qpic_t *ppic )
{
	int			i, width, height;
	gltexture_t	*glt;
	int			*pbuf;
	byte		*pPal;
	glpic_t		*gl;
	qpic_t		*ppicNew;

	if ( !ppic )
		return NULL;

	ppicNew = Mem_Malloc( sizeof( qpic_t ) + sizeof( glpic_t ) );

	width = ppic->width;
	height = ppic->height;

	ppicNew->width = width;
	ppicNew->height = height;

	gl = (glpic_t *)ppicNew->data;

retry:
	// see if the texture is allready present
	if ( pszName[0] )
	{
		for ( i = 0, glt = gltextures; i < numgltextures; i++, glt++ )
		{
			if ( !Q_strcmp( pszName, glt->identifier ) )
			{
				if ( width != glt->width || height != glt->height )
				{
					pszName[3]++;
					goto retry;
				}

				return ppic;
			}
		}
	}
	else
	{
		glt = &gltextures[numgltextures];
	}

	numgltextures++;

	Q_strncpy( glt->identifier, pszName, sizeof( glt->identifier ) - 1 );
	glt->identifier[sizeof( glt->identifier ) - 1] = 0;
	glt->texnum = texture_extension_number_guess;
	glt->width = width;
	glt->height = height;
	glt->mipmap = false;
	glt->servercount = gHostSpawnCount;

	GL_Bind( texture_extension_number_guess );

	pbuf = Mem_Malloc( ppic->width * ppic->height * 4 );
	pPal = &ppic->data[ppic->width * ppic->height + 2];

	for ( i = 0; i < 768; i++ )
		pPal[i] = texgammatable[pPal[i]];

	for ( i = 0; i < ppic->width * ppic->height; i++ )
	{
		byte	b;

		b = ppic->data[i];

		pbuf[i] = *(int *)&pPal[b * 3];
		pbuf[i] &= 0x00ffffff;

		if ( b < 255 )
			pbuf[i] |= 0xff000000;
	}

	GL_Upload32( (unsigned *)pbuf, ppic->width, ppic->height, false, 1, gl_filter_max );

	gl->texnum = texture_extension_number_guess++;
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	Mem_Free( pbuf );

	return ppicNew;
}

void Draw_MiptexTexture( cachewad_t *wad, byte *data )
{
	texture_t	*tex;
	miptex_t	*mip, tmp;
	int			i, pix, paloffset, palettesize;
	byte		*pal, *bitmap;

	if ( wad->cacheExtra != 32 )
		Sys_Error( "Draw_MiptexTexture: Bad cached wad %s\n", wad->name );

	mip = (miptex_t *)( data + wad->cacheExtra );
	tmp = *mip;
	tex = (texture_t *)data;

	Q_memcpy( tex->name, tmp.name, sizeof( tex->name ) );
	tex->width = LittleLong( tmp.width );
	tex->height = LittleLong( tmp.height );
	tex->anim_max = 0;
	tex->anim_min = 0;
	tex->anim_total = 0;
	tex->alternate_anims = NULL;
	tex->anim_next = NULL;

	for ( i = 0; i < MIPLEVELS; i++ )
		tex->offsets[i] = LittleLong( tmp.offsets[i] ) + wad->cacheExtra;

	pix = tex->width * tex->height;
	pix += ( pix >> 2 ) + ( pix >> 4 ) + ( pix >> 6 );

	paloffset = tex->offsets[0] + pix + 2;
	palettesize = *(unsigned short *)( (byte *)mip + sizeof( miptex_t ) + pix );

	pal = (byte *)tex + paloffset;
	bitmap = (byte *)tex + tex->offsets[0];

	if ( gfCustomBuild )
	{
		Q_strncpy( tex->name, szCustName, 15 );
		tex->name[15] = 0;
	}

	if ( pal[765] == 0 && pal[766] == 0 && pal[767] == 255 )
	{
		tex->name[0] = '{';
		tex->gl_texturenum = GL_LoadTexture( tex->name, GLT_DECAL, tex->width, tex->height, bitmap, true, 1, pal );
	}
	else
	{
		tex->name[0] = '}';
		if ( gfCustomBuild )
			GL_UnloadTexture( tex->name );
		tex->gl_texturenum = GL_LoadTexture( tex->name, GLT_DECAL, tex->width, tex->height, bitmap, true, 3, pal );
	}
}

void GL_LoadFilterTexture( float r, float g, float b, float brightness )
{
	byte	*data;
	int		pixelIndex;

	if ( !gl_FilterTexture )
	{
		gl_FilterTexture = texture_extension_number_guess++;

		data = malloc( 8 * 8 * 3 );
		for ( pixelIndex = 0; pixelIndex < 8 * 8 * 3; pixelIndex += 3 )
		{
			data[pixelIndex] = r * brightness * 255;
			data[pixelIndex+1] = g * brightness * 255;
			data[pixelIndex+2] = b * brightness * 255;
		}

		GL_Bind( gl_FilterTexture );
		qglTexImage2D( GL_TEXTURE_2D, 0, 3, 8, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, data );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );

		free( data );
	}
	else
	{
		data = malloc( 8 * 8 * 3 );
		for ( pixelIndex = 0; pixelIndex < 8 * 8 * 3; pixelIndex += 3 )
		{
			data[pixelIndex] = r * brightness * 255;
			data[pixelIndex+1] = g * brightness * 255;
			data[pixelIndex+2] = b * brightness * 255;
		}

		GL_Bind( gl_FilterTexture );
		qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 8, 8, GL_RGB, GL_UNSIGNED_BYTE, data );

		free( data );
	}
}
