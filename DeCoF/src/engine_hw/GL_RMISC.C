// r_misc.c -- renderer initialization, new map setup and debug commands

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "cmd.h"
#include "GL_MODEL.H"
#include "client.h"
#include "vid.h"
#include "render.h"
#include "GLQUAKE.H"
#include "gl_cvars.h"
#include "zone.h"

qboolean	atismoothing;

cvar_t	ati_subdiv = { "ati_subdiv", "2.0", FCVAR_ARCHIVE };
cvar_t	ati_npatch = { "ati_npatch", "1.0", FCVAR_ARCHIVE };

cvar_t	r_bmodelinterp = { "r_bmodelinterp", "1" };
cvar_t	r_bmodelhighfrac = { "r_bmodelhighfrac", "5.0", FCVAR_ARCHIVE };
cvar_t	r_norefresh = { "r_norefresh", "0" };
cvar_t	r_drawentities = { "r_drawentities", "1" };
cvar_t	r_drawviewmodel = { "r_drawviewmodel", "1" };
cvar_t	r_speeds = { "r_speeds", "0" };
cvar_t	r_fullbright = { "r_fullbright", "0" };
cvar_t	r_decals = { "r_decals", "4096" };
cvar_t	mp_decals = { "mp_decals", "300", FCVAR_ARCHIVE };
cvar_t	r_lightmap = { "r_lightmap", "0" };
cvar_t	r_shadows = { "r_shadows", "0" };
cvar_t	r_mirroralpha = { "r_mirroralpha", "1" };
cvar_t	r_wateralpha = { "r_wateralpha", "1" };
cvar_t	r_dynamic = { "r_dynamic", "1" };
cvar_t	r_novis = { "r_novis", "0" };
cvar_t	r_mmx = { "r_mmx", "0" };
cvar_t	r_traceglow = { "r_traceglow", "0" };
cvar_t	r_wadtextures = { "r_wadtextures", "0" };
cvar_t	r_glowshellfreq = { "r_glowshellfreq", "2.2" };
cvar_t	d_spriteskip = { "d_spriteskip", "0" };

cvar_t	gl_clear = { "gl_clear", "0" };
cvar_t	gl_cull = { "gl_cull", "1" };
cvar_t	gl_affinemodels = { "gl_affinemodels", "0" };
cvar_t	gl_flashblend = { "gl_flashblend", "0" };
cvar_t	gl_playermip = { "gl_playermip", "0" };
cvar_t	gl_nocolors = { "gl_nocolors", "0" };
cvar_t	gl_keeptjunctions = { "gl_keeptjunctions", "1" };
cvar_t	gl_reporttjunctions = { "gl_reporttjunctions", "0" };
cvar_t	gl_wateramp = { "gl_wateramp", "0.3" };
cvar_t	gl_dither = { "gl_dither", "1", FCVAR_ARCHIVE };
cvar_t	gl_spriteblend = { "gl_spriteblend", "1" };
cvar_t	gl_polyoffset = { "gl_polyoffset", "4", FCVAR_ARCHIVE };
cvar_t	gl_lightholes = { "gl_lightholes", "1" };
cvar_t	gl_zmax = { "gl_zmax", "4096" };
cvar_t	gl_alphamin = { "gl_alphamin", "0.25" };
cvar_t	gl_overdraw = { "gl_overdraw", "0" };
cvar_t	gl_watersides = { "gl_watersides", "0" };
cvar_t	gl_overbright = { "gl_overbright", "1", FCVAR_ARCHIVE };
cvar_t	gl_envmapsize = { "gl_envmapsize", "256" };
cvar_t	gl_flipmatrix = { "gl_flipmatrix", "0", FCVAR_ARCHIVE };
cvar_t	gl_monolights = { "gl_monolights", "0", FCVAR_ARCHIVE };
cvar_t	gl_fog = { "gl_fog", "1", FCVAR_ARCHIVE };
cvar_t	gl_wireframe = { "gl_wireframe", "0", FCVAR_SPONLY };

qboolean	gl_texsort = 1;

qboolean	filterMode;
float		filterColorRed = 1.0;
float		filterColorGreen = 1.0;
float		filterColorBlue = 1.0;
float		filterBrightness = 1.0;

extern	cvar_t	r_cachestudio;
extern	cvar_t	r_cullsequencebox;
extern	cvar_t	vid_d3d;

extern	int		g_bIsCZeroRitual;

extern	int		texture_extension_number_guess;

void	R_InitParticles( void );
void	R_ClearParticles( void );
void	R_ReadPointFile_f( void );
void	R_DecalInit( void );
void	V_InitLevel( void );
void	GL_BuildLightmaps( void );
void	GL_UnloadTextures( void );
void	GL_Bind( int texnum );
int		GL_LoadTexture( char *identifier, GL_TEXTURETYPE textureType, int width, int height, byte *data, int mipmap, int iType, byte *pPal );
void	VideoMode_GetCurrentVideoMode( int *wide, int *tall, int *bpp );
void	VID_TakeSnapshotRect( const char *pFilename, int x, int y, int w, int h );
void	R_RenderView( void );
double	Sys_FloatTime( void );

void GL_Dump_f( void )
{
	if ( gl_vendor )
		Con_Printf( "GL Vendor: %s\n", gl_vendor );
	if ( gl_renderer )
		Con_Printf( "GL Renderer: %s\n", gl_renderer );
	if ( gl_version )
		Con_Printf( "GL Version: %s\n", gl_version );
	if ( gl_extensions )
		Con_Printf( "GL Extensions: %s\n", gl_extensions );
}

/*
==================
R_InitTextures
==================
*/
void R_InitTextures( void )
{
	int		x, y, m;
	byte	*dest;

// create a simple checkerboard texture for the default
	r_notexture_mip = Hunk_AllocName( sizeof( texture_t ) + 16*16 + 8*8 + 4*4 + 2*2, "notexture" );

	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof( texture_t );
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16*16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8*8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4*4;

	for ( m = 0; m < 4; m++ )
	{
		dest = (byte *)r_notexture_mip + r_notexture_mip->offsets[m];
		for ( y = 0; y < ( 16 >> m ); y++ )
			for ( x = 0; x < ( 16 >> m ); x++ )
			{
				if ( ( y < ( 8 >> m ) ) ^ ( x < ( 8 >> m ) ) )
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}
}

void R_UploadEmptyTex( void )
{
	byte	pPal[768];

	Q_memset( pPal, 0, sizeof( pPal ) );
	pPal[765] = 255;
	pPal[766] = 0;
	pPal[767] = 255;

	r_notexture_mip->gl_texturenum = GL_LoadTexture( "**empty**", GLT_SYSTEM, r_notexture_mip->width, r_notexture_mip->height, (byte *)( r_notexture_mip + 1 ), true, 0, pPal );
}

byte	dottexture[8][8] =
{
	{0,1,1,0,0,0,0,0},
	{1,1,1,1,0,0,0,0},
	{1,1,1,1,0,0,0,0},
	{0,1,1,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
};

void R_InitParticleTexture( void )
{
	int		x, y;
	byte	data[8][8][4];
	int		w, h, bpp;

	//
	// particle texture
	//
	particletexture = texture_extension_number_guess++;
	GL_Bind( particletexture );

	for ( x = 0; x < 8; x++ )
	{
		for ( y = 0; y < 8; y++ )
		{
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = dottexture[x][y] * 255;
		}
	}

	VideoMode_GetCurrentVideoMode( &w, &h, &bpp );
	if ( bpp == 32 )
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
	else
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGB5_A1, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );

	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
}

/*
===============
R_Envmap_f

Grab six views for environment mapping tests
===============
*/
void R_Envmap_f( void )
{
	char	name[1024];
	vrect_t	saveVrect;
	char	base[64];

	if ( !cl.worldmodel )
	{
		Con_Printf( "No map loaded\n" );
		return;
	}

	if ( gl_envmapsize.value <= 0 )
	{
		Con_Printf( "Invalid gl_envmapsize value\n" );
		return;
	}

	if ( cl.num_entities && cl_entities[0].model )
		COM_FileBase( cl_entities[0].model->name, base );
	else
		Q_strcpy( base, "Env" );

	qglDrawBuffer( GL_FRONT );
	qglReadBuffer( GL_FRONT );
	envmap = true;

	Q_memcpy( &saveVrect, &r_refdef.vrect, sizeof( saveVrect ) );

	r_refdef.vrect.x = 0;
	r_refdef.vrect.y = 0;
	r_refdef.vrect.width = gl_envmapsize.value;
	r_refdef.vrect.height = gl_envmapsize.value;

	r_refdef.viewangles[0] = 0;
	r_refdef.viewangles[1] = 0;
	r_refdef.viewangles[2] = 0;
	GL_BeginRendering( &glx, &gly, &glwidth, &glheight );
	R_RenderView();
	_snprintf( name, sizeof( name ), "%srt.bmp", base );
	VID_TakeSnapshotRect( name, 0, 0, gl_envmapsize.value, gl_envmapsize.value );

	r_refdef.viewangles[1] = 90;
	GL_BeginRendering( &glx, &gly, &glwidth, &glheight );
	R_RenderView();
	_snprintf( name, sizeof( name ), "%sbk.bmp", base );
	VID_TakeSnapshotRect( name, 0, 0, gl_envmapsize.value, gl_envmapsize.value );

	r_refdef.viewangles[1] = 180;
	GL_BeginRendering( &glx, &gly, &glwidth, &glheight );
	R_RenderView();
	_snprintf( name, sizeof( name ), "%slf.bmp", base );
	VID_TakeSnapshotRect( name, 0, 0, gl_envmapsize.value, gl_envmapsize.value );

	r_refdef.viewangles[1] = 270;
	GL_BeginRendering( &glx, &gly, &glwidth, &glheight );
	R_RenderView();
	_snprintf( name, sizeof( name ), "%sft.bmp", base );
	VID_TakeSnapshotRect( name, 0, 0, gl_envmapsize.value, gl_envmapsize.value );

	r_refdef.viewangles[0] = -90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering( &glx, &gly, &glwidth, &glheight );
	R_RenderView();
	_snprintf( name, sizeof( name ), "%sup.bmp", base );
	VID_TakeSnapshotRect( name, 0, 0, gl_envmapsize.value, gl_envmapsize.value );

	r_refdef.viewangles[0] = 90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering( &glx, &gly, &glwidth, &glheight );
	R_RenderView();
	_snprintf( name, sizeof( name ), "%sdn.bmp", base );
	VID_TakeSnapshotRect( name, 0, 0, gl_envmapsize.value, gl_envmapsize.value );

	Q_memcpy( &r_refdef.vrect, &saveVrect, sizeof( saveVrect ) );
	envmap = false;
	qglDrawBuffer( GL_BACK );
	qglReadBuffer( GL_BACK );
	GL_EndRendering();
}

/*
===============
R_Init
===============
*/
void R_Init( void )
{
	Cmd_AddCommand( "timerefresh", R_TimeRefresh_f );
	Cmd_AddCommand( "envmap", R_Envmap_f );
	Cmd_AddCommand( "pointfile", R_ReadPointFile_f );
	Cmd_AddCommand( "gl_dump", GL_Dump_f );

	Cvar_RegisterVariable( &ati_subdiv );
	Cvar_RegisterVariable( &ati_npatch );
	Cvar_RegisterVariable( &gl_wireframe );
	Cvar_RegisterVariable( &r_cachestudio );
	Cvar_RegisterVariable( &r_cullsequencebox );
	Cvar_RegisterVariable( &r_bmodelinterp );
	Cvar_RegisterVariable( &r_bmodelhighfrac );
	Cvar_RegisterVariable( &r_norefresh );
	Cvar_RegisterVariable( &r_lightmap );
	Cvar_RegisterVariable( &r_fullbright );
	Cvar_RegisterVariable( &r_decals );
	Cvar_RegisterVariable( &mp_decals );
	Cvar_SetValue( "r_decals", 4096 );
	Cvar_RegisterVariable( &r_drawentities );
	Cvar_RegisterVariable( &r_drawviewmodel );
	Cvar_RegisterVariable( &r_mirroralpha );
	Cvar_RegisterVariable( &r_wateralpha );
	Cvar_RegisterVariable( &r_dynamic );
	Cvar_RegisterVariable( &r_novis );
	Cvar_RegisterVariable( &r_speeds );
	Cvar_RegisterVariable( &d_spriteskip );
	Cvar_RegisterVariable( &r_wadtextures );
	Cvar_RegisterVariable( &r_mmx );
	Cvar_RegisterVariable( &r_traceglow );
	Cvar_RegisterVariable( &r_glowshellfreq );

	Cvar_RegisterVariable( &gl_clear );
	Cvar_RegisterVariable( &gl_cull );
	Cvar_RegisterVariable( &gl_affinemodels );
	Cvar_RegisterVariable( &gl_playermip );
	Cvar_RegisterVariable( &gl_nocolors );
	Cvar_RegisterVariable( &gl_dither );
	Cvar_RegisterVariable( &gl_spriteblend );
	Cvar_RegisterVariable( &gl_polyoffset );
	Cvar_RegisterVariable( &gl_lightholes );

	Cvar_RegisterVariable( &gl_keeptjunctions );
	Cvar_RegisterVariable( &gl_reporttjunctions );
	Cvar_RegisterVariable( &gl_wateramp );
	Cvar_RegisterVariable( &gl_overbright );
	Cvar_RegisterVariable( &gl_zmax );
	Cvar_RegisterVariable( &gl_alphamin );
	Cvar_RegisterVariable( &gl_flipmatrix );
	Cvar_RegisterVariable( &gl_monolights );
	Cvar_RegisterVariable( &gl_fog );

	if ( gl_mtexable )
	{
		gl_texsort = 0;
		Cvar_SetValue( "gl_overbright", 0 );
	}

	R_InitParticles();
	R_InitParticleTexture();
	R_UploadEmptyTex();

	playertextures = texture_extension_number_guess;
	texture_extension_number_guess += 16;
}

/*
===============
R_TranslatePlayerSkin

Translates a skin texture by the per-player color lookup
===============
*/
void R_TranslatePlayerSkin( int playernum )
{
}

/*
===============
R_NewMap
===============
*/
void R_NewMap( void )
{
	int		i;

	for ( i = 0; i < 256; i++ )
		d_lightstylevalue[i] = 264;		// normal light value

	Q_memset( &r_worldentity, 0, sizeof( r_worldentity ) );
	r_worldentity.model = cl.worldmodel;

// clear out efrags in case the level hasn't been reloaded
// FIXME: is this one short?
	for ( i = 0; i < cl.worldmodel->numleafs; i++ )
		cl.worldmodel->leafs[i].efrags = NULL;

	r_viewleaf = NULL;
	R_ClearParticles();
	R_DecalInit();
	V_InitLevel();

	GL_BuildLightmaps();

	// identify sky texture
	skytexturenum = -1;
	mirrortexturenum = -1;
	for ( i = 0; i < cl.worldmodel->numtextures; i++ )
	{
		qboolean	IsCzeroAndIsSkyCullAndIsD3D;

		if ( !cl.worldmodel->textures[i] )
			continue;

		IsCzeroAndIsSkyCullAndIsD3D = g_bIsCZeroRitual && !Q_strcmp( cl.worldmodel->textures[i]->name, "skycull" ) && vid_d3d.value;

		if ( !Q_strncmp( cl.worldmodel->textures[i]->name, "sky", 3 ) && ( !g_bIsCZeroRitual || !IsCzeroAndIsSkyCullAndIsD3D ) )
			skytexturenum = i;
		if ( !Q_strncmp( cl.worldmodel->textures[i]->name, "window02_1", 10 ) )
			mirrortexturenum = i;
		cl.worldmodel->textures[i]->texturechain = NULL;
	}

	R_LoadSkys();

	cl_entities->curstate.scale = gl_wateramp.value;

	GL_UnloadTextures();
}

/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
void R_TimeRefresh_f( void )
{
	int		i;
	float	start, stop, time;

	qglDrawBuffer( GL_FRONT );
	qglFinish();

	start = Sys_FloatTime();
	for ( i = 0; i < 128; i++ )
	{
		r_refdef.viewangles[1] = i / 128.0 * 360.0;
		R_RenderView();
	}

	qglFinish();
	stop = Sys_FloatTime();
	time = stop - start;
	Con_Printf( "%f seconds (%f fps)\n", time, 128 / time );

	qglDrawBuffer( GL_BACK );
	GL_EndRendering();
}

void D_FlushCaches( void )
{
}

void GL_LoadFilterTexture( float r, float g, float b, float brightness );

void R_CheckVariables( void )
{
	static float oldFilterMode, oldFilterColorRed, oldFilterColorGreen, oldFilterColorBlue, oldFilterBrightness;

	if ( filterMode != oldFilterMode || filterColorRed != oldFilterColorRed || filterColorGreen != oldFilterColorGreen ||
		filterColorBlue != oldFilterColorBlue || filterBrightness != oldFilterBrightness )
	{
		oldFilterMode = filterMode;
		oldFilterColorRed = filterColorRed;
		oldFilterColorGreen = filterColorGreen;
		oldFilterColorBlue = filterColorBlue;
		oldFilterBrightness = filterBrightness;

		GL_LoadFilterTexture( filterColorRed, filterColorGreen, filterColorBlue, filterBrightness );
		D_FlushCaches();
	}
}
