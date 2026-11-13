// cl_draw.c -- the client dll's HUD sprite services and screen filter

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "mem.h"
#include "GL_MODEL.H"
#include "client.h"
#include "vid.h"
#include "APIProxy.h"
#include "qgl.h"

#define MAX_SPRITES		256

typedef struct
{
	struct model_s	*pSprite;
	char			*pName;
	int				frames;
} SPRITELIST;

// gl_rmisc.c
extern	int			filterMode;
extern	float		filterColorRed;
extern	float		filterColorGreen;
extern	float		filterColorBlue;
extern	float		filterBrightness;

// sv_main.c
int		ModelFrameCount( struct model_s *model );

// cl_tent.c
mspriteframe_t	*R_GetSpriteFrame( msprite_t *pSprite, int frame );

// gl_model.c
void	Mod_UnloadSpriteTextures( model_t *pModel );

// gl_draw.c
void	Draw_SpriteFrame( mspriteframe_t *pFrame, unsigned short *pPalette, int x, int y, const wrect_t *prcSubRect );
void	Draw_SpriteFrameHoles( mspriteframe_t *pFrame, unsigned short *pPalette, int x, int y, const wrect_t *prcSubRect );
void	Draw_SpriteFrameAdditive( mspriteframe_t *pFrame, unsigned short *pPalette, int x, int y, const wrect_t *prcSubRect );
void	Draw_SpriteFrameGeneric( mspriteframe_t *pFrame, unsigned short *pPalette, int x, int y, const wrect_t *prcSubRect, int src, int dest, int width, int height );
void	EnableScissorTest( int x, int y, int width, int height );
void	DisableScissorTest( void );

void	SPR_Set( HSPRITE hSprite, int r, int g, int b );
void	SPR_DrawHoles( int frame, int x, int y, const wrect_t *prc );

static	SPRITELIST	*gSpriteList;
static	int			gSpriteCount;

static	msprite_t	*gpSprite;

unsigned short	gSpritePalette[256];

HSPRITE		ghCrosshair;
wrect_t		gCrosshairRc;
int			gCrosshairR;
int			gCrosshairG;
int			gCrosshairB;

/*
=================
SetFilterMode
=================
*/
void SetFilterMode( int mode )
{
	g_engdstAddrs.pfnSetFilterMode( &mode );

	filterMode = mode;
}

/*
=================
SetFilterColor
=================
*/
void SetFilterColor( float r, float g, float b )
{
	g_engdstAddrs.pfnSetFilterColor( &r, &g, &b );

	filterColorRed = r;
	filterColorGreen = g;
	filterColorBlue = b;
}

/*
=================
SetFilterBrightness
=================
*/
void SetFilterBrightness( float brightness )
{
	g_engdstAddrs.pfnSetFilterBrightness( &brightness );

	filterBrightness = brightness;
}

/*
=================
SetCrosshair
=================
*/
void SetCrosshair( HSPRITE hspr, wrect_t rc, int r, int g, int b )
{
	g_engdstAddrs.pfnSetCrosshair( &hspr, &rc, &r, &g, &b );

	ghCrosshair = hspr;
	gCrosshairRc = rc;
	gCrosshairR = r;
	gCrosshairG = g;
	gCrosshairB = b;
}

/*
=================
DrawCrosshair
=================
*/
void DrawCrosshair( int x, int y )
{
	if ( ghCrosshair )
	{
		SPR_Set( ghCrosshair, gCrosshairR, gCrosshairG, gCrosshairB );
		SPR_DrawHoles( 0, x - ( gCrosshairRc.right - gCrosshairRc.left ) / 2, y - ( gCrosshairRc.bottom - gCrosshairRc.top ) / 2, &gCrosshairRc );
	}
}

qboolean	gSpriteMipMap = true;

/*
=================
SPR_Load
=================
*/
HSPRITE SPR_Load( const char *pTextureName )
{
	int		i;

	g_engdstAddrs.pfnSPR_Load( &pTextureName );

	if ( !pTextureName )
		return 0;

	if ( !gSpriteList || gSpriteCount <= 0 )
		return 0;

	for ( i = 0; i < gSpriteCount; i++ )
	{
		if ( !gSpriteList[i].pSprite )
		{
			gSpriteList[i].pName = Mem_Malloc( Q_strlen( pTextureName ) + 1 );
			Q_strcpy( gSpriteList[i].pName, pTextureName );
		}

		if ( !Q_stricmp( pTextureName, gSpriteList[i].pName ) )
		{
			gSpriteMipMap = false;
			gSpriteList[i].pSprite = Mod_ForName( pTextureName, false, true );
			gSpriteMipMap = true;

			if ( !gSpriteList[i].pSprite )
				return 0;

			gSpriteList[i].frames = ModelFrameCount( gSpriteList[i].pSprite );
			return i + 1;
		}
	}

	Sys_Error( "cannot allocate more than %d HUD sprites\n", MAX_SPRITES );
	return 0;
}

/*
=================
SPR_Shutdown
=================
*/
void SPR_Shutdown( void )
{
	int		i;

	if ( !host_initialized )
		return;

	if ( gSpriteList )
	{
		for ( i = 0; i < gSpriteCount; i++ )
		{
			if ( gSpriteList[i].pSprite )
				Mod_UnloadSpriteTextures( gSpriteList[i].pSprite );

			if ( gSpriteList[i].pName )
				Mem_Free( gSpriteList[i].pName );
		}

		Mem_Free( gSpriteList );
	}

	gSpriteList = NULL;
	gSpriteCount = 0;
	gpSprite = NULL;
	ghCrosshair = 0;
}

/*
=================
SPR_Shutdown_NoModelFree
=================
*/
void SPR_Shutdown_NoModelFree( void )
{
	int		i;

	if ( !host_initialized )
		return;

	if ( gSpriteList )
	{
		for ( i = 0; i < gSpriteCount; i++ )
		{
			if ( gSpriteList[i].pName )
				Mem_Free( gSpriteList[i].pName );
		}

		Mem_Free( gSpriteList );
	}

	gSpriteList = NULL;
	gSpriteCount = 0;
	gpSprite = NULL;
	ghCrosshair = 0;
}

/*
=================
SPR_Get
=================
*/
SPRITELIST *SPR_Get( HSPRITE hSprite )
{
	int		spriteIndex;

	spriteIndex = hSprite - 1;

	if ( spriteIndex < 0 || spriteIndex >= gSpriteCount )
		return NULL;

	return &gSpriteList[spriteIndex];
}

/*
=================
SPR_GetModelPointer
=================
*/
const struct model_s *SPR_GetModelPointer( HSPRITE hSprite )
{
	SPRITELIST	*pList;

	pList = SPR_Get( hSprite );
	if ( pList )
		return pList->pSprite;

	return NULL;
}

/*
=================
SPR_Pointer
=================
*/
msprite_t *SPR_Pointer( SPRITELIST *pList )
{
	return (msprite_t *)pList->pSprite->cache.data;
}

/*
=================
SPR_Init
=================
*/
void SPR_Init( void )
{
	int		listSize;

	if ( gSpriteList )
		return;

	ghCrosshair = 0;
	gSpriteCount = MAX_SPRITES;
	listSize = gSpriteCount * sizeof( SPRITELIST );
	gSpriteList = Mem_ZeroMalloc( listSize );
	gpSprite = NULL;
}

/*
=================
SPR_Frames
=================
*/
int SPR_Frames( HSPRITE hSprite )
{
	SPRITELIST	*pList;

	g_engdstAddrs.pfnSPR_Frames( &hSprite );

	pList = SPR_Get( hSprite );
	if ( pList )
		return pList->frames;

	return 0;
}

/*
=================
SPR_Width
=================
*/
int SPR_Width( HSPRITE hSprite, int frame )
{
	mspriteframe_t	*pFrame;
	SPRITELIST		*pList;
	msprite_t		*pSprite;

	g_engdstAddrs.pfnSPR_Width( &hSprite, &frame );

	pList = SPR_Get( hSprite );
	if ( pList )
	{
		pSprite = SPR_Pointer( pList );
		pFrame = R_GetSpriteFrame( pSprite, frame );
		if ( pFrame )
			return pFrame->width;
	}

	return 0;
}

/*
=================
SPR_Height
=================
*/
int SPR_Height( HSPRITE hSprite, int frame )
{
	mspriteframe_t	*pFrame;
	SPRITELIST		*pList;
	msprite_t		*pSprite;

	g_engdstAddrs.pfnSPR_Height( &hSprite, &frame );

	pList = SPR_Get( hSprite );
	if ( pList )
	{
		pSprite = SPR_Pointer( pList );
		pFrame = R_GetSpriteFrame( pSprite, frame );
		if ( pFrame )
			return pFrame->height;
	}

	return 0;
}

/*
=================
UnpackPalette
=================
*/
void UnpackPalette( unsigned short *pDest, unsigned short *pSource, int r, int g, int b )
{
}

/*
=================
SPR_Set
=================
*/
void SPR_Set( HSPRITE hSprite, int r, int g, int b )
{
	SPRITELIST	*pList;

	g_engdstAddrs.pfnSPR_Set( &hSprite, &r, &g, &b );

	pList = SPR_Get( hSprite );
	if ( !pList )
		return;

	gpSprite = SPR_Pointer( pList );
	if ( !gpSprite )
		return;

	qglColor4f( (float)( r / 255.0 ), (float)( g / 255.0 ), (float)( b / 255.0 ), 1.0 );
}

/*
=================
SPR_Draw
=================
*/
void SPR_Draw( int frame, int x, int y, const wrect_t *prc )
{
	mspriteframe_t	*pFrame;

	g_engdstAddrs.pfnSPR_Draw( &frame, &x, &y, &prc );

	if ( !gpSprite || x >= (int)vid.width || y >= (int)vid.height )
		return;

	pFrame = R_GetSpriteFrame( gpSprite, frame );
	if ( !pFrame )
	{
		Con_DPrintf( "Client.dll SPR_Draw error:  invalid frame\n" );
		return;
	}

	Draw_SpriteFrame( pFrame, gSpritePalette, x, y, prc );
}

/*
=================
SPR_DrawHoles
=================
*/
void SPR_DrawHoles( int frame, int x, int y, const wrect_t *prc )
{
	mspriteframe_t	*pFrame;

	g_engdstAddrs.pfnSPR_DrawHoles( &frame, &x, &y, &prc );

	if ( !gpSprite || x >= (int)vid.width || y >= (int)vid.height )
		return;

	pFrame = R_GetSpriteFrame( gpSprite, frame );
	if ( !pFrame )
	{
		Con_DPrintf( "Client.dll SPR_DrawHoles error:  invalid frame\n" );
		return;
	}

	Draw_SpriteFrameHoles( pFrame, gSpritePalette, x, y, prc );
}

/*
=================
SPR_DrawAdditive
=================
*/
void SPR_DrawAdditive( int frame, int x, int y, const wrect_t *prc )
{
	mspriteframe_t	*pFrame;

	g_engdstAddrs.pfnSPR_DrawAdditive( &frame, &x, &y, &prc );

	if ( !gpSprite || x >= (int)vid.width || y >= (int)vid.height )
		return;

	pFrame = R_GetSpriteFrame( gpSprite, frame );
	if ( !pFrame )
	{
		Con_DPrintf( "Client.dll SPR_DrawAdditive error:  invalid frame\n" );
		return;
	}

	Draw_SpriteFrameAdditive( pFrame, gSpritePalette, x, y, prc );
}

/*
=================
SPR_DrawGeneric
=================
*/
void SPR_DrawGeneric( int frame, int x, int y, const wrect_t *prc, int src, int dest, int width, int height )
{
	mspriteframe_t	*pFrame;

	g_engdstAddrs.pfnSPR_DrawGeneric( &frame, &x, &y, &prc, &src, &dest, &width, &height );

	if ( !gpSprite || x >= (int)vid.width || y >= (int)vid.height )
		return;

	pFrame = R_GetSpriteFrame( gpSprite, frame );
	if ( !pFrame )
	{
		Con_DPrintf( "Client.dll SPR_DrawGeneric error: invalid frame\n" );
		return;
	}

	Draw_SpriteFrameGeneric( pFrame, gSpritePalette, x, y, prc, src, dest, width, height );
}

/*
=================
SPR_EnableScissor
=================
*/
void SPR_EnableScissor( int x, int y, int width, int height )
{
	g_engdstAddrs.pfnSPR_EnableScissor( &x, &y, &width, &height );

	EnableScissorTest( x, y, width, height );
}

/*
=================
SPR_DisableScissor
=================
*/
void SPR_DisableScissor( void )
{
	g_engdstAddrs.pfnSPR_DisableScissor();

	DisableScissorTest();
}
