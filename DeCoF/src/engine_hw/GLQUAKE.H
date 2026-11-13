// glquake.h -- declarations shared by the OpenGL renderer files
//
// Include after quakedef.h, mathlib.h, cvar.h, GL_MODEL.H and client.h.

#ifndef GLQUAKE_H
#define GLQUAKE_H

#include "qgl.h"

void GL_BeginRendering( int *x, int *y, int *width, int *height );
void GL_EndRendering( void );
void GL_Bind( int texnum );

// glhud.c
void GLBeginHud( void );
void GLFinishHud( void );

extern	int		texture_mode;

extern	float	gldepthmin, gldepthmax;

typedef enum
{
	GLT_SYSTEM,
	GLT_DECAL,
	GLT_HUDSPRITE,
	GLT_STUDIO,
	GLT_WORLD,
	GLT_SPRITE
} GL_TEXTURETYPE;

extern	int		glx, gly, glwidth, glheight;

#define BACKFACE_EPSILON	0.01

void R_TimeRefresh_f( void );
texture_t *R_TextureAnimation( msurface_t *s );

typedef struct alight_s
{
	int			ambientlight;	// clip at 128
	int			shadelight;		// clip at 192 - ambientlight
	vec3_t		color;
	float		*plightvec;
} alight_t;

typedef struct
{
	float	fv[3];		// viewspace x, y
} auxvert_t;

//====================================================

extern	cl_entity_t	r_worldentity;
extern	qboolean	r_cache_thrash;		// compatability
extern	vec3_t		modelorg, r_entorigin;
extern	cl_entity_t	*currententity;
extern	int			r_visframecount;	// ??? what difs?
extern	int			r_framecount;
extern	mplane_t	frustum[4];
extern	int			c_brush_polys, c_alias_polys;

//
// view origin
//
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

//
// screen size info
//
extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern	texture_t	*r_notexture_mip;
extern	int		d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	qboolean	envmap;
extern	int	currenttexture;
extern	int	particletexture;
extern	int	playertextures;

extern	int	skytexturenum;		// index in cl.loadmodel, not gl texture object

extern	cvar_t	r_norefresh;
extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawviewmodel;
extern	cvar_t	r_speeds;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_decals;
extern	cvar_t	mp_decals;
extern	cvar_t	r_lightmap;
extern	cvar_t	r_shadows;
extern	cvar_t	r_mirroralpha;
extern	cvar_t	r_wateralpha;
extern	cvar_t	r_dynamic;
extern	cvar_t	r_novis;
extern	cvar_t	r_mmx;
extern	cvar_t	r_traceglow;
extern	cvar_t	r_wadtextures;

extern	qboolean	filterMode;
extern	float		filterColorRed;
extern	float		filterColorGreen;
extern	float		filterColorBlue;
extern	float		filterBrightness;

extern	int		gl_lightmap_format;

extern	int		mirrortexturenum;	// quake texturenum, not gltexturenum
extern	qboolean	mirror;
extern	mplane_t	*mirror_plane;

extern	float	r_world_matrix[16];

extern	const char *gl_vendor;
extern	const char *gl_renderer;
extern	const char *gl_version;
extern	const char *gl_extensions;

// Multitexture
extern	int		TEXTURE0_SGIS;
extern	int		TEXTURE1_SGIS;
extern	int		TEXTURE2_SGIS;

typedef void ( APIENTRY *lpMTexFUNC )( GLenum, GLfloat, GLfloat );
typedef void ( APIENTRY *lpSelTexFUNC )( GLenum );

extern	int		cnttextures[2];
extern	int		gl_mtexable;

void GL_DisableMultitexture( void );
void GL_EnableMultitexture( void );

extern	model_t	*loadmodel;

// gl_warp.c
void GL_SubdivideSurface( msurface_t *fa );
void EmitWaterPolys( msurface_t *fa, int direction );
void R_DrawSkyChain( msurface_t *s );
void R_ClearSkyBox( void );
void R_DrawSkyBox( void );
void R_LoadSkys( void );
void R_InitSky( void );
qboolean LoadTGA( char *szFilename, unsigned char *buffer, int bufferSize, int *width, int *height );
qboolean LoadTGA2( char *szFilename, unsigned char *buffer, int bufferSize, int *width, int *height, int errFail );
void D_SetFadeColor( int r, int g, int b, int fog );

#endif // GLQUAKE_H
