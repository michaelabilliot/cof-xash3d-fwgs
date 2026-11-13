// vgui_EngineSurfaceHW.cpp -- the OpenGL drawing of the VGUI engine surface

#include <windows.h>

#include "vgui_EngineSurface.h"
#include "vgui/VGuiVertex.h"

#define Assert( _exp ) ((void)0)
#include "UtlRBTree.h"

extern "C"
{
#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "GL_MODEL.H"
#include "client.h"
#include "GLQUAKE.H"
#include "screen.h"
#include "view.h"
#include "keys.h"
#include "APIProxy.h"
#include "winquake.h"

extern	cvar_t	vid_d3d;

void	VideoMode_GetCurrentVideoMode( int *wide, int *tall, int *bpp );
qboolean IsPowerOfTwo( int value );
int		VGui_LoadBMP( FileHandle_t file, unsigned char *buffer, int bufsize, int *width, int *height );
void	AllowFog( int allowed );
void	ClientDLL_HudRedraw( int intermission );
}

struct VertexBuffer_t
{
	float texcoords[2];
	float vertex[2];
};

static VertexBuffer_t g_VertexBuffer[256];
static int g_iVertexBufferEntriesUsed = 0;

namespace
{
class Texture
{
public:
	int _id;
	int _wide;
	int _tall;
	float _s0;
	float _t0;
	float _s1;
	float _t1;
};
}

static Texture *staticTextureCurrent = NULL;

static bool VguiSurfaceTexturesLessFunc( const Texture &lhs, const Texture &rhs )
{
	return lhs._id < rhs._id;
}

static CUtlRBTree< Texture, int > g_VGuiSurfaceTextures( 0, 128, VguiSurfaceTexturesLessFunc );

static Texture *staticGetTextureById( int id );
static Texture *staticAllocTextureForId( int id );

// the projection matrix pushMakeCurrent replaces and popMakeCurrent restores
static float g_ProjectionMatrix_guess[16];

class TCoordRect
{
public:
	float s0;
	float t0;
	float s1;
	float t1;
};

RECT	g_ScissorRect;
bool	g_bScissor = false;

float InterpTCoord( int val, int min, int max, float tMin, float tMax )
{
	float percent = (float)( val - min ) / ( max - min );
	return tMin + percent * ( tMax - tMin );
}

bool ScissorRect_TCoords( int x0, int y0, int x1, int y1, float s0, float t0, float s1, float t1, RECT *pOut, TCoordRect *pTCoords )
{
	RECT rcChar;
	rcChar.left = x0;
	rcChar.top = y0;
	rcChar.right = x1;
	rcChar.bottom = y1;

	if ( g_bScissor )
	{
		// clip the char rect against the scissor rect
		if ( !IntersectRect( pOut, &g_ScissorRect, &rcChar ) )
			return false;

		if ( pTCoords )
		{
			// interpolate the texture coordinates of the clipped rect
			pTCoords->s0 = InterpTCoord( pOut->left, rcChar.left, rcChar.right, s0, s1 );
			pTCoords->s1 = InterpTCoord( pOut->right, rcChar.left, rcChar.right, s0, s1 );
			pTCoords->t0 = InterpTCoord( pOut->top, rcChar.top, rcChar.bottom, t0, t1 );
			pTCoords->t1 = InterpTCoord( pOut->bottom, rcChar.top, rcChar.bottom, t0, t1 );
		}
	}
	else
	{
		*pOut = rcChar;

		if ( pTCoords )
		{
			pTCoords->s0 = s0;
			pTCoords->s1 = s1;
			pTCoords->t0 = t0;
			pTCoords->t1 = t1;
		}
	}

	return true;
}

bool ScissorRect( int x0, int y0, int x1, int y1, RECT *pOut )
{
	return ScissorRect_TCoords( x0, y0, x1, y1, 0, 0, 0, 0, pOut, NULL );
}

void EngineSurface::createRenderPlat( void )
{
}

void EngineSurface::deleteRenderPlat( void )
{
}

void EngineSurface::drawFilledRect( int x0, int y0, int x1, int y1 )
{
	if ( _drawColor[3] == 255 )
		return;

	RECT rcOut;
	if ( !ScissorRect( x0, y0, x1, y1, &rcOut ) )
		return;

	qglDisable( GL_TEXTURE_2D );
	qglColor4ub( _drawColor[0], _drawColor[1], _drawColor[2], 255 - _drawColor[3] );
	qglBegin( GL_QUADS );
	qglVertex2f( rcOut.left, rcOut.top );
	qglVertex2f( rcOut.right, rcOut.top );
	qglVertex2f( rcOut.right, rcOut.bottom );
	qglVertex2f( rcOut.left, rcOut.bottom );
	qglEnd();
	qglEnable( GL_TEXTURE_2D );
}

void EngineSurface::drawPolyLine( int *px, int *py, int numPoints )
{
	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_BLEND );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE );
	qglColor4ub( _drawTextColor[0], _drawTextColor[1], _drawTextColor[2], 255 - _drawTextColor[3] );
	qglBegin( GL_LINES );

	for ( int i = 0; i < numPoints - 1; i++ )
	{
		int x1, x2, y1, y2;

		x1 = px[i];
		y1 = py[i];
		x2 = px[i + 1];
		y2 = py[i + 1];

		qglVertex2f( x1, y1 );
		qglVertex2f( x2, y2 );
	}

	qglEnd();
	qglColor3f( 1, 1, 1 );
	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
}

void EngineSurface::drawTexturedPolygon( vgui2::VGuiVertex *pVertices, int n )
{
	if ( _drawColor[3] == 255 )
		return;

	int x, y;
	float u, v;

	qglColor4ub( _drawColor[0], _drawColor[1], _drawColor[2], 255 - _drawColor[3] );
	qglEnable( GL_TEXTURE_2D );
	qglBegin( GL_POLYGON );

	for ( int i = 0; i < n; i++ )
	{
		pVertices[i].GetVertex( x, y, u, v );
		qglTexCoord2f( u, v );
		qglVertex2f( x, y );
	}

	qglEnd();
}

void EngineSurface::drawLine( int x1, int y1, int x2, int y2 )
{
	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_BLEND );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE );
	qglColor4ub( _drawTextColor[0], _drawTextColor[1], _drawTextColor[2], 255 - _drawTextColor[3] );
	qglBegin( GL_LINES );
	qglVertex2f( x1, y1 );
	qglVertex2f( x2, y2 );
	qglEnd();
	qglColor3f( 1, 1, 1 );
	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
}

void EngineSurface::drawOutlinedRect( int x0, int y0, int x1, int y1 )
{
	if ( _drawColor[3] == 255 )
		return;

	qglDisable( GL_TEXTURE_2D );
	qglColor4ub( _drawColor[0], _drawColor[1], _drawColor[2], 255 - _drawColor[3] );

	drawFilledRect( x0, y0, x1, y0 + 1 );		// top
	drawFilledRect( x0, y1 - 1, x1, y1 );		// bottom
	drawFilledRect( x0, y0 + 1, x0 + 1, y1 - 1 );	// left
	drawFilledRect( x1 - 1, y0 + 1, x1, y1 - 1 );	// right
}

void EngineSurface::drawPrintChar( int x, int y, int wide, int tall, float s0, float t0, float s1, float t1 )
{
	if ( _drawTextColor[3] == 255 )
		return;

	RECT rcOut;
	TCoordRect tRect;
	if ( !ScissorRect_TCoords( x, y, x + wide, y + tall, s0, t0, s1, t1, &rcOut, &tRect ) )
		return;

	if ( 1 )
	{
		// batch the quad, drawFlushText draws it
		if ( g_iVertexBufferEntriesUsed >= 256 )
			drawFlushText();

		g_VertexBuffer[g_iVertexBufferEntriesUsed].texcoords[0] = tRect.s0;
		g_VertexBuffer[g_iVertexBufferEntriesUsed].texcoords[1] = tRect.t0;
		g_VertexBuffer[g_iVertexBufferEntriesUsed].vertex[0] = rcOut.left;
		g_VertexBuffer[g_iVertexBufferEntriesUsed].vertex[1] = rcOut.top;
		g_iVertexBufferEntriesUsed++;

		g_VertexBuffer[g_iVertexBufferEntriesUsed].texcoords[0] = tRect.s1;
		g_VertexBuffer[g_iVertexBufferEntriesUsed].texcoords[1] = tRect.t0;
		g_VertexBuffer[g_iVertexBufferEntriesUsed].vertex[0] = rcOut.right;
		g_VertexBuffer[g_iVertexBufferEntriesUsed].vertex[1] = rcOut.top;
		g_iVertexBufferEntriesUsed++;

		g_VertexBuffer[g_iVertexBufferEntriesUsed].texcoords[0] = tRect.s1;
		g_VertexBuffer[g_iVertexBufferEntriesUsed].texcoords[1] = tRect.t1;
		g_VertexBuffer[g_iVertexBufferEntriesUsed].vertex[0] = rcOut.right;
		g_VertexBuffer[g_iVertexBufferEntriesUsed].vertex[1] = rcOut.bottom;
		g_iVertexBufferEntriesUsed++;

		g_VertexBuffer[g_iVertexBufferEntriesUsed].texcoords[0] = tRect.s0;
		g_VertexBuffer[g_iVertexBufferEntriesUsed].texcoords[1] = tRect.t1;
		g_VertexBuffer[g_iVertexBufferEntriesUsed].vertex[0] = rcOut.left;
		g_VertexBuffer[g_iVertexBufferEntriesUsed].vertex[1] = rcOut.bottom;
		g_iVertexBufferEntriesUsed++;
	}
	else
	{
		qglEnable( GL_TEXTURE_2D );
		qglColor4ub( _drawTextColor[0], _drawTextColor[1], _drawTextColor[2], 255 - _drawTextColor[3] );
		qglBegin( GL_QUADS );
		qglTexCoord2f( tRect.s0, tRect.t0 );
		qglVertex2f( rcOut.left, rcOut.top );
		qglTexCoord2f( tRect.s1, tRect.t0 );
		qglVertex2f( rcOut.right, rcOut.top );
		qglTexCoord2f( tRect.s1, tRect.t1 );
		qglVertex2f( rcOut.right, rcOut.bottom );
		qglTexCoord2f( tRect.s0, tRect.t1 );
		qglVertex2f( rcOut.left, rcOut.bottom );
		qglEnd();
	}
}

void EngineSurface::drawPrintCharAdd( int x, int y, int wide, int tall, float s0, float t0, float s1, float t1 )
{
	if ( _drawTextColor[3] == 255 )
		return;

	RECT rcOut;
	TCoordRect tRect;
	if ( !ScissorRect_TCoords( x, y, x + wide, y + tall, s0, t0, s1, t1, &rcOut, &tRect ) )
		return;

	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE );
	qglEnable( GL_BLEND );
	qglEnable( GL_ALPHA_TEST );
	qglColor4ub( _drawTextColor[0], _drawTextColor[1], _drawTextColor[2], 255 - _drawTextColor[3] );
	qglBegin( GL_QUADS );
	qglTexCoord2f( tRect.s0, tRect.t0 );
	qglVertex2f( rcOut.left, rcOut.top );
	qglTexCoord2f( tRect.s1, tRect.t0 );
	qglVertex2f( rcOut.right, rcOut.top );
	qglTexCoord2f( tRect.s1, tRect.t1 );
	qglVertex2f( rcOut.right, rcOut.bottom );
	qglTexCoord2f( tRect.s0, tRect.t1 );
	qglVertex2f( rcOut.left, rcOut.bottom );
	qglEnd();
	qglDisable( GL_ALPHA_TEST );
	qglDisable( GL_BLEND );
}

void EngineSurface::drawSetTextureRGBA( int id, const char *rgba, int wide, int tall, int hardwareFilter, int hasAlphaChannel )
{
	Texture *texture = staticGetTextureById( id );
	if ( !texture )
		texture = staticAllocTextureForId( id );

	if ( !texture )
		return;

	texture->_id = id;
	texture->_wide = wide;
	texture->_tall = tall;

	long *pExpanded, *pToUse;

	// the texture is uploaded with power of two sizes
	int wide2;
	int tall2;

	for ( int i = 0; i < 32; i++ )
	{
		wide2 = 1 << i;
		if ( wide2 >= wide )
			break;
	}

	for ( i = 0; i < 32; i++ )
	{
		tall2 = 1 << i;
		if ( tall2 >= tall )
			break;
	}

	if ( wide2 != wide || tall2 != tall )
	{
		pExpanded = new long[ wide2 * tall2 ];
		if ( !pExpanded )
			return;

		memset( pExpanded, 0, wide2 * tall2 * 4 );

		for ( int y = 0; y < tall; y++ )
		{
			for ( int x = 0; x < wide; x++ )
			{
				pExpanded[y * wide2 + x] = ( (long *)rgba )[y * wide + x];
			}
		}

		pToUse = pExpanded;
	}
	else
	{
		pExpanded = NULL;
		pToUse = (long *)rgba;
	}

	texture->_s0 = 0;
	texture->_t0 = 0;
	texture->_s1 = (float)wide / ( (float)wide2 );
	texture->_t1 = (float)tall / ( (float)tall2 );

	currenttexture = id;
	staticTextureCurrent = texture;

	qglBindTexture( GL_TEXTURE_2D, id );

	if ( hardwareFilter )
	{
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}
	else
	{
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	}

	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	int w, h, bpp;
	VideoMode_GetCurrentVideoMode( &w, &h, &bpp );

	if ( bpp == 32 || hasAlphaChannel )
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, wide2, tall2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pToUse );
	else
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGB5_A1, wide2, tall2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pToUse );

	if ( pExpanded )
		delete [] pExpanded;
}

static Texture *staticGetTextureById( int id )
{
	Texture findTex = { id };
	int index = g_VGuiSurfaceTextures.Find( findTex );
	if ( g_VGuiSurfaceTextures.IsValidIndex( index ) )
		return &g_VGuiSurfaceTextures[index];

	return NULL;
}

static Texture *staticAllocTextureForId( int id )
{
	Texture newTex = { id };
	int index = g_VGuiSurfaceTextures.Insert( newTex );
	return &g_VGuiSurfaceTextures[index];
}

void EngineSurface::drawSetTexture( int id )
{
	if ( id != currenttexture )
	{
		drawFlushText();
		currenttexture = id;
	}

	staticTextureCurrent = staticGetTextureById( id );
	qglBindTexture( GL_TEXTURE_2D, id );
}

void EngineSurface::drawFlushText( void )
{
	if ( g_iVertexBufferEntriesUsed < 1 )
		return;

	qglEnableClientState( GL_VERTEX_ARRAY );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	qglEnable( GL_TEXTURE_2D );
	qglEnable( GL_BLEND );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglColor4ub( _drawTextColor[0], _drawTextColor[1], _drawTextColor[2], 255 - _drawTextColor[3] );
	qglTexCoordPointer( 2, GL_FLOAT, sizeof( VertexBuffer_t ), g_VertexBuffer[0].texcoords );
	qglVertexPointer( 2, GL_FLOAT, sizeof( VertexBuffer_t ), g_VertexBuffer[0].vertex );
	qglDrawArrays( GL_QUADS, 0, g_iVertexBufferEntriesUsed );
	qglDisable( GL_BLEND );

	g_iVertexBufferEntriesUsed = 0;
}

void EngineSurface::drawTexturedRect( int x0, int y0, int x1, int y1 )
{
	Texture *tex = staticTextureCurrent;
	if ( !tex )
		return;

	if ( _drawColor[3] == 255 )
		return;

	RECT rcOut;
	TCoordRect tRect;
	if ( !ScissorRect_TCoords( x0, y0, x1, y1, tex->_s0, tex->_t0, tex->_s1, tex->_t1, &rcOut, &tRect ) )
		return;

	qglEnable( GL_TEXTURE_2D );
	qglBindTexture( GL_TEXTURE_2D, tex->_id );
	currenttexture = tex->_id;
	qglColor4ub( _drawColor[0], _drawColor[1], _drawColor[2], 255 - _drawColor[3] );

	if ( vid_d3d.value )
	{
		// sample the texel centers
		tRect.s0 += 0.5 / tex->_wide;
		tRect.s1 -= 0.5 / tex->_wide;
		tRect.t0 += 0.5 / tex->_tall;
		tRect.t1 -= 0.5 / tex->_tall;
	}

	qglBegin( GL_QUADS );
	qglTexCoord2f( tRect.s0, tRect.t0 );
	qglVertex2f( rcOut.left, rcOut.top );
	qglTexCoord2f( tRect.s1, tRect.t0 );
	qglVertex2f( rcOut.right, rcOut.top );
	qglTexCoord2f( tRect.s1, tRect.t1 );
	qglVertex2f( rcOut.right, rcOut.bottom );
	qglTexCoord2f( tRect.s0, tRect.t1 );
	qglVertex2f( rcOut.left, rcOut.bottom );
	qglEnd();
}

void EngineSurface::pushMakeCurrent( int *insets, int *absExtents, int *clipRect, bool translateToScreenSpace )
{
	int surfaceAbsExtents[4];
	int xTranslate, yTranslate;

	xTranslate = yTranslate = 0;

	POINT pnt = { 0, 0 };
	RECT rect;

	if ( translateToScreenSpace )
	{
		ClientToScreen( *pmainwindow, &pnt );
	}

	GetClientRect( *pmainwindow, &rect );

	surfaceAbsExtents[0] = pnt.x;
	surfaceAbsExtents[1] = pnt.y;
	surfaceAbsExtents[2] = rect.right + pnt.x;
	surfaceAbsExtents[3] = rect.bottom + pnt.y;

	int wide = surfaceAbsExtents[2] - surfaceAbsExtents[0];
	int tall = surfaceAbsExtents[3] - surfaceAbsExtents[1];

	qglMatrixMode( GL_PROJECTION );
	qglGetFloatv( GL_PROJECTION_MATRIX, g_ProjectionMatrix_guess );
	qglLoadIdentity();
	qglOrtho( 0, wide, tall, 0, -1, 1 );

	qglMatrixMode( GL_MODELVIEW );
	qglPushMatrix();
	qglLoadIdentity();

	qglTranslatef( insets[0], insets[1], 0 );
	xTranslate += insets[0];
	yTranslate += insets[1];

	int x0, y0, x1, y1;

	x0 = absExtents[0];
	y0 = absExtents[1];
	x1 = absExtents[2];
	y1 = absExtents[3];

	x0 -= surfaceAbsExtents[0];
	y0 -= surfaceAbsExtents[1];
	x1 -= surfaceAbsExtents[0];
	y1 -= surfaceAbsExtents[1];

	qglTranslatef( x0, y0, 0 );
	xTranslate += x0;
	yTranslate += y0;

	x0 = clipRect[0];
	y0 = clipRect[1];
	x1 = clipRect[2];
	y1 = clipRect[3];

	x0 -= surfaceAbsExtents[0];
	y0 -= surfaceAbsExtents[1];
	x1 -= surfaceAbsExtents[0];
	y1 -= surfaceAbsExtents[1];

	g_bScissor = true;
	g_ScissorRect.left = x0;
	g_ScissorRect.top = y0;
	g_ScissorRect.right = g_ScissorRect.left + ( x1 - x0 );
	g_ScissorRect.bottom = g_ScissorRect.top + ( y1 - y0 );

	g_ScissorRect.left -= xTranslate;
	g_ScissorRect.top -= yTranslate;
	g_ScissorRect.right -= xTranslate;
	g_ScissorRect.bottom -= yTranslate;

	qglEnable( GL_BLEND );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
}

void EngineSurface::popMakeCurrent( void )
{
	drawFlushText();

	qglMatrixMode( GL_PROJECTION );
	qglLoadMatrixf( g_ProjectionMatrix_guess );

	qglMatrixMode( GL_MODELVIEW );
	qglPopMatrix();

	g_bScissor = false;

	qglEnable( GL_TEXTURE_2D );
}

void EngineSurface::drawSetTextureFile( int id, const char *filename, int hardwareFilter, bool forceReload )
{
	Texture *texture = staticGetTextureById( id );

	if ( !texture || forceReload )
	{
		unsigned char buf[256 * 1024];
		int width, height;

		char name[512];
		_snprintf( name, sizeof( name ), "%s.tga", filename );

		if ( LoadTGA( name, buf, sizeof( buf ), &width, &height ) )
		{
			drawSetTextureRGBA( id, (const char *)buf, width, height, hardwareFilter, false );
			texture = staticGetTextureById( id );
		}
		else
		{
			_snprintf( name, sizeof( name ), "%s.bmp", filename );

			FileHandle_t f = FS_Open( name, "rb" );
			if ( f )
			{
				VGui_LoadBMP( f, buf, sizeof( buf ), &width, &height );
				drawSetTextureRGBA( id, (const char *)buf, width, height, hardwareFilter, false );
				texture = staticGetTextureById( id );
			}
		}

		if ( fs_perf_warnings.value && ( !IsPowerOfTwo( width ) || !IsPowerOfTwo( height ) ) )
		{
			Con_DPrintf( "fs_perf_warnings: Resampling non-power-of-2 image '%s' (%dx%d)\n", filename, width, height );
		}
	}

	if ( texture )
	{
		drawSetTexture( id );
	}
}

void EngineSurface::drawSetSubTextureRGBA( int textureID, int drawX, int drawY, const unsigned char *rgba, int subTextureWide, int subTextureTall )
{
	qglTexSubImage2D( GL_TEXTURE_2D, 0, drawX, drawY, subTextureWide, subTextureTall, GL_RGBA, GL_UNSIGNED_BYTE, rgba );
}

void EngineSurface::drawGetTextureSize( int id, int &wide, int &tall )
{
	Texture *texture = staticGetTextureById( id );

	if ( texture )
	{
		wide = texture->_wide;
		tall = texture->_tall;
	}
	else
	{
		wide = 0;
		tall = 0;
	}
}

int EngineSurface::isTextureIdValid( int id )
{
	return staticGetTextureById( id ) != NULL;
}

void EngineSurface::resetViewPort( void )
{
}

extern "C" void VGui_ViewportPaintBackground( int *extents )
{
	g_engdstAddrs.VGui_ViewportPaintBackground( &extents );

	GLFinishHud();

	// save the view rect
	int old[4];
	old[0] = glx;
	old[1] = gly;
	old[2] = glwidth;
	old[3] = glheight;

	POINT pnt = { 0, 0 };
	ClientToScreen( *pmainwindow, &pnt );

	RECT rect;
	GetClientRect( *pmainwindow, &rect );

	// render the world into the panel
	glx = extents[0] - pnt.x;
	gly = glheight - ( extents[3] - pnt.y );
	glwidth = extents[2] - extents[0];
	glheight = extents[3] - extents[1];

	SCR_CalcRefdef();
	V_RenderView();

	GLBeginHud();

	if ( cls.state == ca_active && cls.signon == SIGNONS )
	{
		bool bState = false;
		if ( cl.intermission == 1 )
			bState = true;

		AllowFog( false );
		ClientDLL_HudRedraw( bState );
		AllowFog( true );
	}

	if ( !scr_drawloading && ( cl.intermission != 1 || key_dest != key_game ) && ( cl.intermission != 1 || key_dest != key_game ) )
	{
		SCR_DrawConsole();
	}

	SCR_DrawLoading();

	if ( cls.state == ca_active && cls.signon == SIGNONS )
	{
		Sbar_Draw();
	}

	GLFinishHud();

	// restore the view rect
	glx = old[0];
	gly = old[1];
	glwidth = old[2];
	glheight = old[3];

	SCR_CalcRefdef();

	GLBeginHud();
}
