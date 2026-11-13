// r_studio.c -- studio model rendering and studio hulls

#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "cvar.h"
#include "zone.h"
#include "GL_MODEL.H"
#include "progs.h"
#include "eiface.h"
#include "delta.h"
#include "server.h"
#include "client.h"
#include "sound.h"
#include "vid.h"
#include "render.h"
#include "GLQUAKE.H"
#include "gl_cvars.h"
#include "studio.h"
#include "r_studioint.h"
#include "host.h"
#include "view.h"
#include "qgl.h"
#include "mem.h"
#include "triangleapi.h"
#include "../pm_shared/pm_movevars.h"

#include <math.h>

#define STUDIO_CACHE_SIZE	16
#define STUDIO_CACHEMASK	( STUDIO_CACHE_SIZE - 1 )

#define MAXSTUDIOHULLS		128

#define MAX_KNOWN_MODELS	1536

#define STUDIO_RENDER		1
#define STUDIO_EVENTS		2

#define IDSTUDIOHEADER		( ( 'T' << 24 ) + ( 'S' << 16 ) + ( 'D' << 8 ) + 'I' )	// little-endian "IDST"

// GL_ATI_pn_triangles
#define GL_PN_TRIANGLES_ATI						0x87F0
#define GL_PN_TRIANGLES_POINT_MODE_ATI			0x87F2
#define GL_PN_TRIANGLES_NORMAL_MODE_ATI			0x87F3
#define GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI	0x87F4
#define GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI		0x87F6
#define GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATI	0x87F7

typedef struct
{
	float		frame;
	int			sequence;
	vec3_t		angles;
	vec3_t		origin;
	vec3_t		size;
	unsigned char	controller[4];
	unsigned char	blending[2];
	model_t		*pModel;
	int			nStartHull;
	int			nStartPlane;
	int			numhulls;
} r_studiocache_t;

typedef struct
{
	char		name[260];
	char		modelname[260];
	model_t		*model;
} player_model_t;

typedef struct
{
	int			keynum;
	int			topcolor;
	int			bottomcolor;
	model_t		*model;
	char		name[260];
	int			index;
	int			source;
	int			width;
	int			height;
	int			gl_index;
} skin_t;

cvar_t			*cl_righthand;

static studiohdr_t	*pstudiohdr;

vec3_t			r_colormix;
colorVec		r_icolormix;
vec3_t			r_blightvec[MAXSTUDIOBONES];

float			rotationmatrix[3][4];
float			bonetransform[MAXSTUDIOBONES][3][4];
float			lighttransform[MAXSTUDIOBONES][3][4];
int				cached_numbones;
char			cached_bonename[MAXSTUDIOBONES][32];
float			cached_bonetransform[MAXSTUDIOBONES][3][4];
float			cached_lighttransform[MAXSTUDIOBONES][3][4];

static int		cache_hull_hitgroup[MAXSTUDIOHULLS];
static hull_t	cache_hull[MAXSTUDIOHULLS];
static mplane_t	cache_planes[MAXSTUDIOHULLS * 6];
static int		nCurrentHull;
static int		nCurrentPlane;

static r_studiocache_t	rgStudioCache[STUDIO_CACHE_SIZE];
static int		r_cachecurrent;

int				r_dointerp = 1;

cvar_t			r_cachestudio = { "r_cachestudio", "1" };
cvar_t			r_cullsequencebox = { "r_cullsequencebox", "1" };

static int		studio_hull_hitgroup[MAXSTUDIOHULLS];
static hull_t	studio_hull[MAXSTUDIOHULLS];
static dclipnode_t	studio_clipnodes[6];
static mplane_t	studio_planes[MAXSTUDIOHULLS * 6];

int				g_iBackFaceCull;

static mstudiobodyparts_t	*pbodypart;
static mstudiomodel_t	*psubmodel;

static player_info_t	*r_playerinfo;
static int		r_playerindex;
static model_t	*r_model;
static float	r_gaitmovement;
static int		g_ForcedFaceFlags;
static vec3_t	g_ChromeOrigin;
static int		r_flatshade_guess;

static int		chrome[MAXSTUDIOVERTS][2];
static int		g_NormalIndex[MAXSTUDIOVERTS];
static int		chromeage[MAXSTUDIOBONES];
static vec3_t	r_chromeup[MAXSTUDIOBONES];
static vec3_t	r_chromeright[MAXSTUDIOBONES];

static int		numlights;
static dlight_t	*locallight[3];
static int		locallinearlight[3][3];
static float	locallightR2[3];
static float	lightpos[MAXSTUDIOVERTS][3][4];
static vec3_t	lightbonepos[MAXSTUDIOBONES][3];
static int		lightage[MAXSTUDIOBONES];

auxvert_t		*pauxverts;

static auxvert_t	auxverts[MAXSTUDIOVERTS];
static vec3_t	lightvalues[MAXSTUDIOVERTS];
static vec3_t	*pvlightvalues;
static int		pvlightvalues_size;
static vec3_t	g_txformednormal[MAXSTUDIOVERTS];
static int		g_rendermode;

int				r_ambientlight;
float			r_shadelight;

int				r_amodels_drawn;
int				r_smodels_total;

static int		r_topcolor;
static int		r_bottomcolor;

player_model_t	DM_PlayerState[MAX_CLIENTS];
skin_t			DM_RemapSkin[64][11];
static skin_t	*pDM_RemapSkin[2528][11];
int				r_remapindex;

cache_user_t	model_texture_cache[MAX_KNOWN_MODELS][32];

void R_FlushStudioCache( void );
void *Mod_Extradata( model_t *mod );

extern sv_blending_interface_t	*g_pSvBlendingAPI;
extern r_studio_interface_t	*pStudioAPI;
extern int		g_bIsTerrorStrike;
extern cvar_t	r_glowshellfreq;
extern triangleapi_t	tri;
extern model_t	*cl_sprite_white;
extern vec3_t	r_plightvec;
extern float	v_lambert1;
extern int		lightgammatable[1024];
extern int		lineargammatable[1024];
extern int		screengammatable[1024];
extern cvar_t	gl_dither;
extern model_t	mod_known[];
extern cvar_t	developer;
extern cvar_t	cl_himodels;
extern cvar_t	cl_gaitestimation;
extern cvar_t	r_fullbright;
extern cvar_t	v_direct;
extern movevars_t	movevars;
extern qboolean	filterMode;
extern float	filterColorRed;
extern float	filterColorGreen;
extern float	filterColorBlue;
extern float	filterBrightness;
extern cvar_t	gl_affinemodels;
extern cvar_t	r_drawentities;
extern cvar_t	r_shadows;
extern cvar_t	gl_ztrick;
extern float	gldepthmin;
extern model_t	*cl_sprite_shell;
extern float	r_blend;
extern vec3_t	shadevector;

void	R_StudioRenderFinal( void );
void	R_StudioDrawPoints( void );
void	GLR_StudioDrawShadow( void );
void	GL_DisableMultitexture( void );
int		CL_FxBlend( cl_entity_t *e );
void	CL_FxTransform( cl_entity_t *ent, float transform[3][4] );

extern vec3_t	lightspot;
extern int		c_alias_polys;
extern qboolean	gD3DMode;
extern int		atismoothing;
extern cvar_t	ati_npatch;
extern cvar_t	ati_subdiv;
extern void	( APIENTRY * qglPNTrianglesiATI )( GLenum pname, GLint param );

void	R_GLStudioDrawPoints( void );
void	R_GLStudioDrawPointsATI_guess( void );

msurface_t	*SurfaceAtPoint( model_t *pModel, mnode_t *node, vec_t *start, vec_t *end );
colorVec	R_LightVec( vec_t *start, vec_t *end );

int		R_StudioDrawPlayer( int flags, entity_state_t *pplayer );
void	R_StudioCalcAttachments( void );
void	R_StudioClientEvents( void );
void	R_StudioRenderModel( void );
void	R_StudioDynamicLight( cl_entity_t *ent, alight_t *plight );
void	R_StudioEntityLight( alight_t *plight );

void	GL_Bind( int texnum );
void	GL_UnloadTexture( char *identifier );
int		GL_LoadTexture( char *identifier, GL_TEXTURETYPE textureType, int width, int height, byte *data, int mipmap, int iType, byte *pPal );

int		SignbitsForPlane( mplane_t *out );
qboolean	R_CullBox( vec3_t mins, vec3_t maxs );
void	R_StudioTransformVector( vec_t *in, vec_t *out );
void	R_StudioTransformAuxVert( auxvert_t *av, int bone, vec_t *vert );
void	R_StudioLighting( float *lv, int bone, int flags, vec_t *normal );

/*
================
SV_InitStudioHull
================
*/
void SV_InitStudioHull( void )
{
	int		i;
	int		side;

	if ( studio_hull[0].planes != NULL )
		return;

	for ( i = 0; i < 6; i++ )
	{
		studio_clipnodes[i].planenum = i;

		side = i & 1;

		studio_clipnodes[i].children[side] = CONTENTS_EMPTY;
		if ( i != 5 )
			studio_clipnodes[i].children[side ^ 1] = i + 1;
		else
			studio_clipnodes[i].children[side ^ 1] = CONTENTS_SOLID;
	}

	for ( i = 0; i < MAXSTUDIOHULLS; i++ )
	{
		studio_hull[i].clipnodes = studio_clipnodes;
		studio_hull[i].planes = &studio_planes[i * 6];
		studio_hull[i].firstclipnode = 0;
		studio_hull[i].lastclipnode = 5;
	}
}

/*
================
R_CheckStudioCache
================
*/
r_studiocache_t *R_CheckStudioCache( model_t *pModel, float frame, int sequence, const vec_t *angles, const vec_t *origin, const vec_t *size, const unsigned char *controller, const unsigned char *blending )
{
	int		i;
	r_studiocache_t	*pCached;

	for ( i = 0; i < STUDIO_CACHE_SIZE; i++ )
	{
		pCached = &rgStudioCache[( r_cachecurrent - i ) & STUDIO_CACHEMASK];

		if ( pCached->pModel != pModel )
			continue;

		if ( pCached->frame != frame )
			continue;

		if ( pCached->sequence != sequence )
			continue;

		if ( !VectorCompare( pCached->angles, angles ) )
			continue;

		if ( !VectorCompare( pCached->origin, origin ) )
			continue;

		if ( !VectorCompare( pCached->size, size ) )
			continue;

		if ( Q_memcmp( pCached->controller, (void *)controller, 4 ) != 0 )
			continue;

		if ( Q_memcmp( pCached->blending, (void *)blending, 2 ) != 0 )
			continue;

		return pCached;
	}

	return NULL;
}

/*
================
R_AddToStudioCache
================
*/
void R_AddToStudioCache( float frame, int sequence, const vec_t *angles, const vec_t *origin, const vec_t *size, const unsigned char *controller, const unsigned char *pblending, model_t *pModel, hull_t *pHulls, int numhulls )
{
	r_studiocache_t	*p;

	if ( nCurrentHull + numhulls >= MAXSTUDIOHULLS )
		R_FlushStudioCache();

	r_cachecurrent++;

	p = &rgStudioCache[r_cachecurrent & STUDIO_CACHEMASK];

	p->frame = frame;
	p->sequence = sequence;

	VectorCopy( angles, p->angles );
	VectorCopy( origin, p->origin );
	VectorCopy( size, p->size );

	Q_memcpy( p->controller, (void *)controller, sizeof( p->controller ) );
	Q_memcpy( p->blending, (void *)pblending, sizeof( p->blending ) );

	p->pModel = pModel;
	p->nStartHull = nCurrentHull;
	p->nStartPlane = nCurrentPlane;

	Q_memcpy( &cache_hull[nCurrentHull], pHulls, numhulls * sizeof( hull_t ) );
	Q_memcpy( &cache_planes[nCurrentPlane], studio_planes, numhulls * 6 * sizeof( mplane_t ) );
	Q_memcpy( &cache_hull_hitgroup[nCurrentHull], studio_hull_hitgroup, numhulls * sizeof( int ) );

	nCurrentHull += numhulls;
	nCurrentPlane += numhulls * 6;

	p->numhulls = numhulls;
}

/*
====================
AngleQuaternion

====================
*/
void AngleQuaternion( vec_t *angles, vec_t *quaternion )
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;

	// FIXME: rescale the inputs to 1/2 angle
	angle = angles[2] * 0.5;
	sy = sin( angle );
	cy = cos( angle );
	angle = angles[1] * 0.5;
	sp = sin( angle );
	cp = cos( angle );
	angle = angles[0] * 0.5;
	sr = sin( angle );
	cr = cos( angle );

	quaternion[0] = sr*cp*cy-cr*sp*sy; // X
	quaternion[1] = cr*sp*cy+sr*cp*sy; // Y
	quaternion[2] = cr*cp*sy-sr*sp*cy; // Z
	quaternion[3] = cr*cp*cy+sr*sp*sy; // W
}

/*
====================
QuaternionSlerp

====================
*/
float	omega, cosom, sinom, sclp, sclq;

void QuaternionSlerp( vec_t *p, vec_t *q, float t, vec_t *qt )
{
	int		i;

	// decide if one of the quaternions is backwards
	float	a = 0;
	float	b = 0;

	for ( i = 0; i < 4; i++ )
	{
		a += ( p[i] - q[i] ) * ( p[i] - q[i] );
		b += ( p[i] + q[i] ) * ( p[i] + q[i] );
	}
	if ( a > b )
	{
		for ( i = 0; i < 4; i++ )
		{
			q[i] = -q[i];
		}
	}

	cosom = p[0]*q[0] + p[1]*q[1] + p[2]*q[2] + p[3]*q[3];

	if ( ( 1.0 + cosom ) > 0.000001 )
	{
		if ( ( 1.0 - cosom ) > 0.000001 )
		{
			omega = acos( cosom );
			sinom = sin( omega );
			sclp = sin( ( 1.0 - t ) * omega ) / sinom;
			sclq = sin( t * omega ) / sinom;
		}
		else
		{
			sclp = 1.0 - t;
			sclq = t;
		}
		for ( i = 0; i < 4; i++ )
		{
			qt[i] = sclp * p[i] + sclq * q[i];
		}
	}
	else
	{
		qt[0] = -q[1];
		qt[1] = q[0];
		qt[2] = -q[3];
		qt[3] = q[2];
		sclp = sin( ( 1.0 - t ) * ( 0.5 * M_PI ) );
		sclq = sin( t * ( 0.5 * M_PI ) );
		for ( i = 0; i < 3; i++ )
		{
			qt[i] = sclp * p[i] + sclq * qt[i];
		}
	}
}

/*
====================
QuaternionMatrix

====================
*/
void QuaternionMatrix( vec_t *quaternion, float (*matrix)[4] )
{
	matrix[0][0] = 1.0 - 2.0 * quaternion[1] * quaternion[1] - 2.0 * quaternion[2] * quaternion[2];
	matrix[1][0] = 2.0 * quaternion[0] * quaternion[1] + 2.0 * quaternion[3] * quaternion[2];
	matrix[2][0] = 2.0 * quaternion[0] * quaternion[2] - 2.0 * quaternion[3] * quaternion[1];

	matrix[0][1] = 2.0 * quaternion[0] * quaternion[1] - 2.0 * quaternion[3] * quaternion[2];
	matrix[1][1] = 1.0 - 2.0 * quaternion[0] * quaternion[0] - 2.0 * quaternion[2] * quaternion[2];
	matrix[2][1] = 2.0 * quaternion[1] * quaternion[2] + 2.0 * quaternion[3] * quaternion[0];

	matrix[0][2] = 2.0 * quaternion[0] * quaternion[2] + 2.0 * quaternion[3] * quaternion[1];
	matrix[1][2] = 2.0 * quaternion[1] * quaternion[2] - 2.0 * quaternion[3] * quaternion[0];
	matrix[2][2] = 1.0 - 2.0 * quaternion[0] * quaternion[0] - 2.0 * quaternion[1] * quaternion[1];
}

/*
====================
R_StudioCalcBoneAdj

====================
*/
void R_StudioCalcBoneAdj( float dadt, float *adj, const unsigned char *pcontroller1, const unsigned char *pcontroller2, unsigned char mouthopen )
{
	int					i, j;
	float				value;
	mstudiobonecontroller_t *pbonecontroller;

	pbonecontroller = (mstudiobonecontroller_t *)( (byte *)pstudiohdr + pstudiohdr->bonecontrollerindex );

	for ( j = 0; j < pstudiohdr->numbonecontrollers; j++ )
	{
		i = pbonecontroller[j].index;
		if ( i <= 3 )
		{
			// check for 360% wrapping
			if ( pbonecontroller[j].type & STUDIO_RLOOP )
			{
				if ( abs( pcontroller1[i] - pcontroller2[i] ) > 128 )
				{
					int a, b;
					a = ( pcontroller1[j] + 128 ) % 256;
					b = ( pcontroller2[j] + 128 ) % 256;
					value = ( ( a * dadt ) + ( b * ( 1 - dadt ) ) - 128 ) * ( 360.0 / 256.0 ) + pbonecontroller[j].start;
				}
				else
				{
					value = ( ( pcontroller1[i] * dadt + ( pcontroller2[i] ) * ( 1.0 - dadt ) ) ) * ( 360.0 / 256.0 ) + pbonecontroller[j].start;
				}
			}
			else
			{
				value = ( pcontroller1[i] * dadt + pcontroller2[i] * ( 1.0 - dadt ) ) / 255.0;
				if ( value < 0 ) value = 0;
				if ( value > 1.0 ) value = 1.0;
				value = ( 1.0 - value ) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			}
		}
		else
		{
			value = mouthopen / 64.0;
			if ( value > 1.0 ) value = 1.0;
			value = ( 1.0 - value ) * pbonecontroller[j].start + value * pbonecontroller[j].end;
		}

		switch ( pbonecontroller[j].type & STUDIO_TYPES )
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			adj[j] = value * ( M_PI / 180.0 );
			break;
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			adj[j] = value;
			break;
		}
	}
}

/*
====================
R_StudioCalcBoneQuaterion

====================
*/
void R_StudioCalcBoneQuaterion( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *q )
{
	int					j, k;
	vec4_t				q1, q2;
	vec3_t				angle1, angle2;
	mstudioanimvalue_t	*panimvalue;

	for ( j = 0; j < 3; j++ )
	{
		if ( panim->offset[j+3] == 0 )
		{
			angle2[j] = angle1[j] = pbone->value[j+3]; // default;
		}
		else
		{
			panimvalue = (mstudioanimvalue_t *)( (byte *)panim + panim->offset[j+3] );
			k = frame;
			// DEBUG
			if ( panimvalue->num.total < panimvalue->num.valid )
				k = 0;
			while ( panimvalue->num.total <= k )
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				// DEBUG
				if ( panimvalue->num.total < panimvalue->num.valid )
					k = 0;
			}
			// Bah, missing blend!
			if ( panimvalue->num.valid > k )
			{
				angle1[j] = panimvalue[k+1].value;

				if ( panimvalue->num.valid > k + 1 )
				{
					angle2[j] = panimvalue[k+2].value;
				}
				else
				{
					if ( panimvalue->num.total > k + 1 )
						angle2[j] = angle1[j];
					else
						angle2[j] = panimvalue[panimvalue->num.valid+2].value;
				}
			}
			else
			{
				angle1[j] = panimvalue[panimvalue->num.valid].value;
				if ( panimvalue->num.total > k + 1 )
				{
					angle2[j] = angle1[j];
				}
				else
				{
					angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			angle1[j] = pbone->value[j+3] + angle1[j] * pbone->scale[j+3];
			angle2[j] = pbone->value[j+3] + angle2[j] * pbone->scale[j+3];
		}

		if ( pbone->bonecontroller[j+3] != -1 )
		{
			angle1[j] += adj[pbone->bonecontroller[j+3]];
			angle2[j] += adj[pbone->bonecontroller[j+3]];
		}
	}

	if ( !VectorCompare( angle1, angle2 ) )
	{
		AngleQuaternion( angle1, q1 );
		AngleQuaternion( angle2, q2 );
		QuaternionSlerp( q1, q2, s, q );
	}
	else
	{
		AngleQuaternion( angle1, q );
	}
}

/*
====================
R_StudioCalcBonePosition

====================
*/
void R_StudioCalcBonePosition( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *pos )
{
	int					j, k;
	mstudioanimvalue_t	*panimvalue;

	for ( j = 0; j < 3; j++ )
	{
		pos[j] = pbone->value[j]; // default;
		if ( panim->offset[j] != 0 )
		{
			panimvalue = (mstudioanimvalue_t *)( (byte *)panim + panim->offset[j] );

			k = frame;
			// DEBUG
			if ( panimvalue->num.total < panimvalue->num.valid )
				k = 0;
			// find span of values that includes the frame we want
			while ( panimvalue->num.total <= k )
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				// DEBUG
				if ( panimvalue->num.total < panimvalue->num.valid )
					k = 0;
			}
			// if we're inside the span
			if ( panimvalue->num.valid > k )
			{
				// and there's more data in the span
				if ( panimvalue->num.valid > k + 1 )
				{
					pos[j] += ( panimvalue[k+1].value * ( 1.0 - s ) + s * panimvalue[k+2].value ) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[k+1].value * pbone->scale[j];
				}
			}
			else
			{
				// are we at the end of the repeating values section and there's another section with data?
				if ( panimvalue->num.total <= k + 1 )
				{
					pos[j] += ( panimvalue[panimvalue->num.valid].value * ( 1.0 - s ) + s * panimvalue[panimvalue->num.valid + 2].value ) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
				}
			}
		}
		if ( pbone->bonecontroller[j] != -1 && adj )
		{
			pos[j] += adj[pbone->bonecontroller[j]];
		}
	}
}

/*
====================
R_StudioSlerpBones

====================
*/
void R_StudioSlerpBones( vec4_t q1[], float pos1[][3], vec4_t q2[], float pos2[][3], float s )
{
	int			i;
	vec4_t		q3;
	float		s1;

	if ( s < 0 ) s = 0;
	else if ( s > 1.0 ) s = 1.0;

	s1 = 1.0 - s;

	for ( i = 0; i < pstudiohdr->numbones; i++ )
	{
		QuaternionSlerp( q1[i], q2[i], s, q3 );
		q1[i][0] = q3[0];
		q1[i][1] = q3[1];
		q1[i][2] = q3[2];
		q1[i][3] = q3[3];
		pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s;
		pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s;
		pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s;
	}
}

/*
====================
R_GetAnim

====================
*/
mstudioanim_t *R_GetAnim( model_t *psubmodel, mstudioseqdesc_t *pseqdesc )
{
	mstudioseqgroup_t	*pseqgroup;
	cache_user_t		*paSequences;

	pseqgroup = (mstudioseqgroup_t *)( (byte *)pstudiohdr + pstudiohdr->seqgroupindex ) + pseqdesc->seqgroup;

	if ( pseqdesc->seqgroup == 0 )
	{
		return (mstudioanim_t *)( (byte *)pstudiohdr + pseqdesc->animindex );
	}

	paSequences = (cache_user_t *)psubmodel->submodels;

	if ( paSequences == NULL )
	{
		paSequences = (cache_user_t *)Mem_Calloc( 16, sizeof( cache_user_t ) ); // UNDONE: leak!
		psubmodel->submodels = (dmodel_t *)paSequences;
	}

	if ( !Cache_Check( (struct cache_user_s *)&( paSequences[pseqdesc->seqgroup] ) ) )
	{
		Con_DPrintf( "loading %s\n", pseqgroup->name );
		COM_LoadCacheFile( pseqgroup->name, (struct cache_user_s *)&paSequences[pseqdesc->seqgroup] );
	}
	return (mstudioanim_t *)( (byte *)paSequences[pseqdesc->seqgroup].data + pseqdesc->animindex );
}

/*
====================
SV_StudioSetupBones

====================
*/
void SV_StudioSetupBones( model_t *pModel, float frame, int sequence, const vec_t *angles, const vec_t *origin, const unsigned char *pcontroller, const unsigned char *pblending, int iBone, const edict_t *edict )
{
	int					i, j;
	float				f, s;
	float				adj[MAXSTUDIOCONTROLLERS];
	mstudiobone_t		*pbones;
	mstudioseqdesc_t	*pseqdesc;
	mstudioanim_t		*panim;

	static vec3_t		pos[MAXSTUDIOBONES];
	float				bonematrix[3][4];
	static vec4_t		q[MAXSTUDIOBONES];

	int					chain[MAXSTUDIOBONES];
	int					chainlength;

	chainlength = 0;

	if ( sequence < 0 || sequence >= pstudiohdr->numseq )
	{
		Con_DPrintf( "SV_StudioSetupBones:  sequence %i/%i out of range for model %s\n", sequence, pstudiohdr->numseq, pstudiohdr->name );
		sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + sequence;
	pbones = (mstudiobone_t *)( (byte *)pstudiohdr + pstudiohdr->boneindex );
	panim = R_GetAnim( pModel, pseqdesc );

	if ( iBone < -1 || iBone >= pstudiohdr->numbones )
		iBone = 0;

	if ( iBone == -1 )
	{
		chainlength = pstudiohdr->numbones;
		for ( i = 0; i < pstudiohdr->numbones; i++ )
		{
			chain[chainlength - i - 1] = i;
		}
	}
	else
	{
		// only the parent bones
		i = iBone;
		while ( i != -1 )
		{
			chain[chainlength++] = i;
			i = pbones[i].parent;
		}
	}

	if ( pseqdesc->numframes <= 1 )
		f = 0;
	else
		f = ( pseqdesc->numframes - 1 ) * frame / 256.0;

	s = f - (int)f;

	R_StudioCalcBoneAdj( 0, adj, pcontroller, pcontroller, 0 );

	for ( j = chainlength - 1; j >= 0; j-- )
	{
		i = chain[j];
		R_StudioCalcBoneQuaterion( (int)f, s, &pbones[i], &panim[i], adj, q[i] );
		R_StudioCalcBonePosition( (int)f, s, &pbones[i], &panim[i], adj, pos[i] );
	}

	if ( pseqdesc->numblends > 1 )
	{
		static vec3_t	pos2[MAXSTUDIOBONES];
		static vec4_t	q2[MAXSTUDIOBONES];
		float			b;

		pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + sequence;
		panim = R_GetAnim( pModel, pseqdesc );
		panim += pstudiohdr->numbones;

		for ( j = chainlength - 1; j >= 0; j-- )
		{
			i = chain[j];
			R_StudioCalcBoneQuaterion( (int)f, s, &pbones[i], &panim[i], adj, q2[i] );
			R_StudioCalcBonePosition( (int)f, s, &pbones[i], &panim[i], adj, pos2[i] );
		}

		b = *pblending / 255.0;

		R_StudioSlerpBones( q, pos, q2, pos2, b );
	}

	AngleMatrix( angles, rotationmatrix );

	rotationmatrix[0][3] = origin[0];
	rotationmatrix[1][3] = origin[1];
	rotationmatrix[2][3] = origin[2];

	for ( j = chainlength - 1; j >= 0; j-- )
	{
		i = chain[j];

		QuaternionMatrix( q[i], bonematrix );

		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if ( pbones[i].parent == -1 )
			R_ConcatTransforms( rotationmatrix, bonematrix, bonetransform[i] );
		else
			R_ConcatTransforms( bonetransform[pbones[i].parent], bonematrix, bonetransform[i] );
	}
}

/*
====================
SV_SetStudioHullPlane

====================
*/
void SV_SetStudioHullPlane( mplane_t *pplane, int iBone, int k, float dist )
{
	pplane->type = 5;

	pplane->normal[0] = bonetransform[iBone][0][k];
	pplane->normal[1] = bonetransform[iBone][1][k];
	pplane->normal[2] = bonetransform[iBone][2][k];

	pplane->dist = bonetransform[iBone][0][3] * pplane->normal[0] + bonetransform[iBone][1][3] * pplane->normal[1] + bonetransform[iBone][2][3] * pplane->normal[2] + dist;
}

/*
====================
R_StudioHull

====================
*/
hull_t *R_StudioHull( model_t *pModel, float frame, int sequence, const vec_t *angles, const vec_t *origin, const vec_t *size, const unsigned char *pcontroller, const unsigned char *pblending, int *pNumHulls, const edict_t *pEdict, int bSkipShield )
{
	int					i, j;
	mstudiobbox_t		*pbbox;
	vec3_t				angles2;
	r_studiocache_t		*pCached = NULL;
	int					numHitBoxes;

	SV_InitStudioHull();

	if ( r_cachestudio.value )
	{
		pCached = R_CheckStudioCache( pModel, frame, sequence, angles, origin, size, pcontroller, pblending );

		if ( pCached )
		{
			Q_memcpy( studio_planes, &cache_planes[pCached->nStartPlane], pCached->numhulls * 6 * sizeof( mplane_t ) );
			Q_memcpy( studio_hull, &cache_hull[pCached->nStartHull], pCached->numhulls * sizeof( hull_t ) );
			Q_memcpy( studio_hull_hitgroup, &cache_hull_hitgroup[pCached->nStartHull], pCached->numhulls * sizeof( int ) );

			*pNumHulls = pCached->numhulls;
			return studio_hull;
		}
	}

	pstudiohdr = (studiohdr_t *)Mod_Extradata( pModel );

	angles2[ROLL] = angles[ROLL];
	angles2[PITCH] = -angles[PITCH];
	angles2[YAW] = angles[YAW];

	g_pSvBlendingAPI->SV_StudioSetupBones( pModel, frame, sequence, angles2, origin, pcontroller, pblending, -1, pEdict );

	pbbox = (mstudiobbox_t *)( (byte *)pstudiohdr + pstudiohdr->hitboxindex );

	for ( i = 0, j = 0; i < pstudiohdr->numhitboxes; i++ )
	{
		if ( bSkipShield == 1 && i == 21 )
		{
			j += 6;
			continue;
		}

		studio_hull_hitgroup[i] = pbbox[i].group;

		SV_SetStudioHullPlane( &studio_planes[j + 0], pbbox[i].bone, 0, pbbox[i].bbmax[0] );
		SV_SetStudioHullPlane( &studio_planes[j + 1], pbbox[i].bone, 0, pbbox[i].bbmin[0] );
		SV_SetStudioHullPlane( &studio_planes[j + 2], pbbox[i].bone, 1, pbbox[i].bbmax[1] );
		SV_SetStudioHullPlane( &studio_planes[j + 3], pbbox[i].bone, 1, pbbox[i].bbmin[1] );
		SV_SetStudioHullPlane( &studio_planes[j + 4], pbbox[i].bone, 2, pbbox[i].bbmax[2] );
		SV_SetStudioHullPlane( &studio_planes[j + 5], pbbox[i].bone, 2, pbbox[i].bbmin[2] );

		studio_planes[j + 0].dist += fabs( (double)studio_planes[j + 0].normal[0] * size[0] ) + fabs( (double)studio_planes[j + 0].normal[1] * size[1] ) + fabs( (double)studio_planes[j + 0].normal[2] * size[2] );
		studio_planes[j + 1].dist -= fabs( (double)studio_planes[j + 1].normal[0] * size[0] ) + fabs( (double)studio_planes[j + 1].normal[1] * size[1] ) + fabs( (double)studio_planes[j + 1].normal[2] * size[2] );
		studio_planes[j + 2].dist += fabs( (double)studio_planes[j + 2].normal[0] * size[0] ) + fabs( (double)studio_planes[j + 2].normal[1] * size[1] ) + fabs( (double)studio_planes[j + 2].normal[2] * size[2] );
		studio_planes[j + 3].dist -= fabs( (double)studio_planes[j + 3].normal[0] * size[0] ) + fabs( (double)studio_planes[j + 3].normal[1] * size[1] ) + fabs( (double)studio_planes[j + 3].normal[2] * size[2] );
		studio_planes[j + 4].dist += fabs( (double)studio_planes[j + 4].normal[0] * size[0] ) + fabs( (double)studio_planes[j + 4].normal[1] * size[1] ) + fabs( (double)studio_planes[j + 4].normal[2] * size[2] );
		studio_planes[j + 5].dist -= fabs( (double)studio_planes[j + 5].normal[0] * size[0] ) + fabs( (double)studio_planes[j + 5].normal[1] * size[1] ) + fabs( (double)studio_planes[j + 5].normal[2] * size[2] );

		j += 6;
	}

	if ( bSkipShield == 1 )
	{
		numHitBoxes = pstudiohdr->numhitboxes - 1;
		*pNumHulls = numHitBoxes;
	}
	else
	{
		numHitBoxes = pstudiohdr->numhitboxes;
		*pNumHulls = numHitBoxes;
	}

	if ( r_cachestudio.value )
		R_AddToStudioCache( frame, sequence, angles, origin, size, pcontroller, pblending, pModel, studio_hull, numHitBoxes );

	return studio_hull;
}

/*
====================
SV_HitgroupForStudioHull

====================
*/
int SV_HitgroupForStudioHull( int index )
{
	return studio_hull_hitgroup[index];
}

/*
================
R_InitStudioCache
================
*/
void R_InitStudioCache( void )
{
	Q_memset( rgStudioCache, 0, sizeof( rgStudioCache ) );

	r_cachecurrent = 0;
	nCurrentHull = 0;
	nCurrentPlane = 0;
}

/*
================
R_FlushStudioCache
================
*/
void R_FlushStudioCache( void )
{
	R_InitStudioCache();
}

/*
====================
R_StudioBodyVariations

====================
*/
int R_StudioBodyVariations( model_t *model )
{
	studiohdr_t			*pstudiohdr;
	mstudiobodyparts_t	*pbodypart;
	int					i, count;

	if ( model->type != mod_studio )
		return 0;

	pstudiohdr = (studiohdr_t *)Mod_Extradata( model );
	if ( !pstudiohdr )
		return 0;

	count = 1;
	pbodypart = (mstudiobodyparts_t *)( (byte *)pstudiohdr + pstudiohdr->bodypartindex );
	for ( i = 0; i < pstudiohdr->numbodyparts; i++ )
	{
		count *= pbodypart[i].nummodels;
	}

	return count;
}

/*
====================
R_StudioPlayerBlend

====================
*/
void R_StudioPlayerBlend( mstudioseqdesc_t *pseqdesc, int *pBlend, float *pPitch )
{
	// calc up/down pointing
	*pBlend = ( *pPitch * 3 );
	if ( *pBlend < pseqdesc->blendstart[0] )
	{
		*pPitch -= pseqdesc->blendstart[0] / 3.0;
		*pBlend = 0;
	}
	else if ( *pBlend > pseqdesc->blendend[0] )
	{
		*pPitch -= pseqdesc->blendend[0] / 3.0;
		*pBlend = 255;
	}
	else
	{
		if ( pseqdesc->blendend[0] - pseqdesc->blendstart[0] < 0.1 ) // catch qc error
			*pBlend = 127;
		else
			*pBlend = 255 * ( *pBlend - pseqdesc->blendstart[0] ) / ( pseqdesc->blendend[0] - pseqdesc->blendstart[0] );
		*pPitch = 0;
	}
}

/*
====================
SV_HullForStudioModel

====================
*/
hull_t *SV_HullForStudioModel( const edict_t *pEdict, const vec_t *mins, const vec_t *maxs, vec_t *offset, int *pNumHulls )
{
	qboolean	useComplexHull;
	vec3_t		size;
	float		factor;
	int			bSkipShield;

	useComplexHull = FALSE;
	bSkipShield = 0;
	factor = 0.5;

	VectorSubtract( maxs, mins, size );

	if ( VectorCompare( vec3_origin, size ) && !( gGlobalVariables.trace_flags & FTRACE_SIMPLEBOX ) )
	{
		useComplexHull = TRUE;

		if ( pEdict->v.flags & FL_CLIENT )
		{
			if ( sv_clienttrace.value != 0.0f )
			{
				factor = sv_clienttrace.value * 0.5;
				size[0] = size[1] = size[2] = 1.0;
			}
			else
			{
				useComplexHull = FALSE;
			}
		}
	}

	if ( pEdict->v.gamestate == 1 && ( g_bIsTerrorStrike == 1 || g_bIsCStrike == 1 || g_bIsCZero == 1 ) )
		bSkipShield = 1;

	if ( ( sv.models[pEdict->v.modelindex]->flags & STUDIO_TRACE_HITBOX ) || useComplexHull )
	{
		VectorScale( size, factor, size );

		offset[0] = 0;
		offset[1] = 0;
		offset[2] = 0;

		if ( pEdict->v.flags & FL_CLIENT )
		{
			vec3_t				angles;
			int					iBlend;
			unsigned char		blending[2];
			unsigned char		controller[4];
			mstudioseqdesc_t	*pseqdesc;

			pstudiohdr = (studiohdr_t *)Mod_Extradata( sv.models[pEdict->v.modelindex] );
			pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + pEdict->v.sequence;

			VectorCopy( pEdict->v.angles, angles );
			R_StudioPlayerBlend( pseqdesc, &iBlend, &angles[PITCH] );

			blending[0] = iBlend;
			blending[1] = 0;

			controller[0] = 0x7F;
			controller[1] = 0x7F;
			controller[2] = 0x7F;
			controller[3] = 0x7F;

			return R_StudioHull( sv.models[pEdict->v.modelindex], pEdict->v.frame, pEdict->v.sequence, angles, pEdict->v.origin, size, controller, blending, pNumHulls, pEdict, bSkipShield );
		}
		else
		{
			return R_StudioHull( sv.models[pEdict->v.modelindex], pEdict->v.frame, pEdict->v.sequence, pEdict->v.angles, pEdict->v.origin, size, pEdict->v.controller, pEdict->v.blending, pNumHulls, pEdict, bSkipShield );
		}
	}
	else
	{
		*pNumHulls = 1;
		return SV_HullForEntity( (edict_t *)pEdict, mins, maxs, offset );
	}
}

/*
====================
DoesSphereIntersect

====================
*/
qboolean DoesSphereIntersect( float *vSphereCenter, float fSphereRadiusSquared, float *vLinePt, float *vLineDir )
{
	vec3_t		P;
	float		a, b, c;
	float		insideSqr;

	VectorSubtract( vLinePt, vSphereCenter, P );

	a = DotProduct( vLineDir, vLineDir );
	b = 2.0 * DotProduct( P, vLineDir );
	c = DotProduct( P, P ) - fSphereRadiusSquared;

	insideSqr = b * b - 4.0f * a * c;

	if ( insideSqr <= 0.000001f )
		return FALSE;

	return TRUE;
}

/*
====================
SV_CheckSphereIntersection

====================
*/
qboolean SV_CheckSphereIntersection( edict_t *ent, const vec_t *start, const vec_t *end )
{
	int					i;
	float				radiusSquared;
	vec3_t				maxDim;
	vec3_t				traceOrg, traceDir;
	studiohdr_t			*pstudiohdr;
	mstudioseqdesc_t	*pseqdesc;

	radiusSquared = 0;

	if ( !( ent->v.flags & FL_CLIENT ) )
		return TRUE;

	VectorCopy( start, traceOrg );
	VectorSubtract( end, start, traceDir );

	pstudiohdr = (studiohdr_t *)Mod_Extradata( sv.models[ent->v.modelindex] );
	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + ent->v.sequence;

	for ( i = 0; i < 3; i++ )
	{
		maxDim[i] = max( fabs( pseqdesc->bbmin[i] ), fabs( pseqdesc->bbmax[i] ) );
		radiusSquared += maxDim[i] * maxDim[i];
	}

	if ( DoesSphereIntersect( ent->v.origin, radiusSquared, traceOrg, traceDir ) )
		return TRUE;

	return FALSE;
}

/*
====================
AnimationAutomove

====================
*/
void AnimationAutomove( const edict_t *pEdict, float flTime )
{
}

/*
====================
GetBonePosition

====================
*/
void GetBonePosition( const edict_t *pEdict, int iBone, float *rgflOrigin, float *rgflAngles )
{
	pstudiohdr = (studiohdr_t *)Mod_Extradata( sv.models[pEdict->v.modelindex] );

	g_pSvBlendingAPI->SV_StudioSetupBones( sv.models[pEdict->v.modelindex], pEdict->v.frame, pEdict->v.sequence, pEdict->v.angles, pEdict->v.origin, pEdict->v.controller, pEdict->v.blending, iBone, pEdict );

	if ( rgflOrigin )
	{
		rgflOrigin[0] = bonetransform[iBone][0][3];
		rgflOrigin[1] = bonetransform[iBone][1][3];
		rgflOrigin[2] = bonetransform[iBone][2][3];
	}
}

/*
====================
GetAttachment

====================
*/
void GetAttachment( const edict_t *pEdict, int iAttachment, float *rgflOrigin, float *rgflAngles )
{
	mstudioattachment_t	*pattachment;
	vec3_t				angles;

	pstudiohdr = (studiohdr_t *)Mod_Extradata( sv.models[pEdict->v.modelindex] );
	pattachment = (mstudioattachment_t *)( (byte *)pstudiohdr + pstudiohdr->attachmentindex ) + iAttachment;

	angles[ROLL] = pEdict->v.angles[ROLL];
	angles[PITCH] = -pEdict->v.angles[PITCH];
	angles[YAW] = pEdict->v.angles[YAW];

	g_pSvBlendingAPI->SV_StudioSetupBones( sv.models[pEdict->v.modelindex], pEdict->v.frame, pEdict->v.sequence, angles, pEdict->v.origin, pEdict->v.controller, pEdict->v.blending, pattachment->bone, pEdict );

	if ( rgflOrigin )
		VectorTransform( pattachment->org, bonetransform[pattachment->bone], rgflOrigin );
}

/*
====================
ModelFrameCount

====================
*/
int ModelFrameCount( model_t *model )
{
	int		count;

	count = 1;

	if ( !model )
		return count;

	if ( model->type == mod_sprite )
	{
		msprite_t	*psprite;

		psprite = (msprite_t *)model->cache.data;
		count = psprite->numframes;
	}
	else if ( model->type == mod_studio )
	{
		count = R_StudioBodyVariations( model );
	}

	if ( count < 1 )
		count = 1;

	return count;
}

/*
================
R_StudioCheckBBox
================
*/
qboolean R_StudioCheckBBox( void )
{
	static vec3_t		defaultmins_guess = { -16, -16, -16 };
	static vec3_t		defaultmaxs_guess = { 16, 16, 16 };

	mplane_t			plane;
	vec3_t				mins, maxs;
	int					i;
	mstudioseqdesc_t	*pseqdesc;

	// compute the bounding box
	if ( !VectorCompare( vec3_origin, pstudiohdr->bbmin ) )
	{
		VectorAdd( currententity->origin, pstudiohdr->bbmin, mins );
		VectorAdd( currententity->origin, pstudiohdr->bbmax, maxs );
	}
	else if ( !VectorCompare( vec3_origin, pstudiohdr->min ) )
	{
		VectorAdd( currententity->origin, pstudiohdr->min, mins );
		VectorAdd( currententity->origin, pstudiohdr->max, maxs );
	}
	else
	{
		VectorAdd( defaultmins_guess, currententity->origin, mins );
		VectorAdd( defaultmaxs_guess, currententity->origin, maxs );
	}

	if ( currententity->curstate.sequence >= pstudiohdr->numseq )
		currententity->curstate.sequence = 0;

	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + currententity->curstate.sequence;

	for ( i = 0; i < 8; i++ )
	{
		vec3_t	p1, p2;

		p1[0] = ( i & 1 ) ? pseqdesc->bbmin[0] : pseqdesc->bbmax[0];
		p1[1] = ( i & 2 ) ? pseqdesc->bbmin[1] : pseqdesc->bbmax[1];
		p1[2] = ( i & 4 ) ? pseqdesc->bbmin[2] : pseqdesc->bbmax[2];

		R_StudioTransformVector( p1, p2 );

		if ( p2[0] < mins[0] ) mins[0] = p2[0];
		if ( p2[0] > maxs[0] ) maxs[0] = p2[0];
		if ( p2[1] < mins[1] ) mins[1] = p2[1];
		if ( p2[1] > maxs[1] ) maxs[1] = p2[1];
		if ( p2[2] < mins[2] ) mins[2] = p2[2];
		if ( p2[2] > maxs[2] ) maxs[2] = p2[2];
	}

	if ( !Host_IsSinglePlayerGame() && r_cullsequencebox.value )
	{
		if ( R_CullBox( mins, maxs ) )
			return FALSE;
	}
	else
	{
		VectorCopy( vpn, plane.normal );
		plane.dist = DotProduct( r_origin, vpn );
		plane.type = 5;
		plane.signbits = SignbitsForPlane( &plane );

		if ( BoxOnPlaneSide( mins, maxs, &plane ) == 2 )
			return FALSE;
	}

	return TRUE;
}

/*
================
R_StudioTransformVector
================
*/
void R_StudioTransformVector( vec_t *in, vec_t *out )
{
	out[0] = DotProduct( in, rotationmatrix[0] ) + rotationmatrix[0][3];
	out[1] = DotProduct( in, rotationmatrix[1] ) + rotationmatrix[1][3];
	out[2] = DotProduct( in, rotationmatrix[2] ) + rotationmatrix[2][3];
}

/*
================
BuildNormalIndexTable
================
*/
void BuildNormalIndexTable( void )
{
	int				j;
	int				i;
	mstudiomesh_t	*pmesh;

	// use the first normal of each vertex
	for ( i = 0; i < psubmodel->numverts; i++ )
		g_NormalIndex[i] = -1;

	for ( j = 0; j < psubmodel->nummesh; j++ )
	{
		short	*ptricmds;

		pmesh = (mstudiomesh_t *)( (byte *)pstudiohdr + psubmodel->meshindex ) + j;
		ptricmds = (short *)( (byte *)pstudiohdr + pmesh->triindex );

		while ( i = *( ptricmds++ ) )
		{
			if ( i < 0 )
				i = -i;

			for ( ; i > 0; i--, ptricmds += 4 )
			{
				if ( g_NormalIndex[ptricmds[0]] < 0 )
					g_NormalIndex[ptricmds[0]] = ptricmds[1];
			}
		}
	}
}

/*
================
BuildGlowShellVerts
================
*/
void BuildGlowShellVerts( vec3_t *pstudioverts, auxvert_t *pauxverts )
{
	int			i;
	byte		*pvertbone;
	vec3_t		*pstudionorms;
	auxvert_t	*av;
	float		vscale;

	vscale = currententity->curstate.renderamt * 0.05;

	pvertbone = ( (byte *)pstudiohdr + psubmodel->vertinfoindex );
	pstudionorms = (vec3_t *)( (byte *)pstudiohdr + psubmodel->normindex );

	for ( i = 0; i < pstudiohdr->numbones; i++ )
		chromeage[i] = 0;

	for ( av = pauxverts, i = 0; i < psubmodel->numverts; i++, av++, pstudioverts++ )
	{
		vec3_t	vert;

		VectorMA( pstudioverts[0], vscale, pstudionorms[g_NormalIndex[i]], vert );
		R_StudioTransformAuxVert( av, pvertbone[i], vert );
	}

	g_ChromeOrigin[0] = cos( r_glowshellfreq.value * cl.time ) * 4000.0;
	g_ChromeOrigin[1] = sin( r_glowshellfreq.value * cl.time ) * 4000.0;
	g_ChromeOrigin[2] = cos( r_glowshellfreq.value * cl.time * 0.33 ) * 4000.0;

	tri.Color4ub( currententity->curstate.rendercolor.r, currententity->curstate.rendercolor.g, currententity->curstate.rendercolor.b, 255 );
}

/*
================
IsFlippedViewModel
================
*/
qboolean IsFlippedViewModel( void )
{
	if ( cl_righthand && cl_righthand->value > 0 && &cl.viewent && cl.viewent.index == currententity->index )
		return TRUE;

	return FALSE;
}

float	h_scale = 1.5;

/*
================
R_StudioSetUpTransform
================
*/
void R_StudioSetUpTransform( int trivial_accept )
{
	int				i;
	vec3_t			angles;
	vec3_t			modelpos;

	VectorCopy( currententity->origin, modelpos );

	angles[ROLL] = currententity->curstate.angles[ROLL];
	angles[PITCH] = currententity->curstate.angles[PITCH];
	angles[YAW] = currententity->curstate.angles[YAW];

	if ( currententity->curstate.movetype == MOVETYPE_STEP )
	{
		float			f = 0;
		float			d;

		// don't do it if the goalstarttime hasn't updated in a while.

		// NOTE:  Because we need to interpolate multiplayer characters, the interpolation time limit
		//  was increased to 1.0 s., which is 2x the max lag we are accounting for.

		if ( ( cl.time < currententity->curstate.animtime + 1.0f ) &&
			( currententity->curstate.animtime != currententity->latched.prevanimtime ) )
		{
			f = ( cl.time - currententity->curstate.animtime ) / ( currententity->curstate.animtime - currententity->latched.prevanimtime );
		}

		if ( r_dointerp )
		{
			// ugly hack to interpolate angle, position. current is reached 0.1 seconds after being set
			f = f - 1.0;
		}
		else
		{
			f = 0;
		}

		for ( i = 0; i < 3; i++ )
		{
			modelpos[i] += ( currententity->origin[i] - currententity->latched.prevorigin[i] ) * f;
		}

		for ( i = 0; i < 3; i++ )
		{
			float ang1, ang2;

			ang1 = currententity->angles[i];
			ang2 = currententity->latched.prevangles[i];

			d = ang1 - ang2;
			if ( d > 180 )
			{
				d -= 360;
			}
			else if ( d < -180 )
			{
				d += 360;
			}

			angles[i] += d * f;
		}
	}
	else if ( currententity->curstate.movetype != MOVETYPE_NONE )
	{
		VectorCopy( currententity->angles, angles );
	}

	angles[PITCH] = -angles[PITCH];
	AngleMatrix( angles, rotationmatrix );

	rotationmatrix[0][3] = modelpos[0];
	rotationmatrix[1][3] = modelpos[1];
	rotationmatrix[2][3] = modelpos[2];
}

/*
================
VectorIRotate
================
*/
void VectorIRotate( vec_t *in1, float in2[3][4], vec_t *out )
{
	out[0] = in1[0]*in2[0][0] + in1[1]*in2[1][0] + in1[2]*in2[2][0];
	out[1] = in1[0]*in2[0][1] + in1[1]*in2[1][1] + in1[2]*in2[2][1];
	out[2] = in1[0]*in2[0][2] + in1[1]*in2[1][2] + in1[2]*in2[2][2];
}

/*
====================
CL_StudioEstimateInterpolant

====================
*/
float CL_StudioEstimateInterpolant( void )
{
	float dadt;

	if ( r_dointerp && ( currententity->curstate.animtime >= currententity->latched.prevanimtime + 0.01 ) )
	{
		dadt = ( cl.time - currententity->curstate.animtime ) / 0.1;
		if ( dadt > 2.0 )
		{
			dadt = 2.0;
		}
	}
	else
	{
		dadt = 1.0;
	}
	return dadt;
}

/*
====================
R_StudioCalcRotations

====================
*/
void R_StudioCalcRotations( float pos[][3], vec4_t *q, mstudioseqdesc_t *pseqdesc, mstudioanim_t *panim, float f )
{
	int					i;
	int					frame;
	mstudiobone_t		*pbone;

	float				s;
	float				adj[MAXSTUDIOCONTROLLERS];
	float				dadt;

	if ( f > pseqdesc->numframes - 1 )
	{
		f = 0;	// bah, fix this bug with changing sequences too fast
	}
	// BUG ( somewhere else ) but this code should validate this data.
	// This could cause a crash if the frame # is negative, so we'll go ahead
	//  and clamp it here
	else if ( f < -0.01 )
	{
		f = -0.01;
	}

	frame = (int)f;

	dadt = CL_StudioEstimateInterpolant();
	s = ( f - frame );

	// add in programtic controllers
	pbone = (mstudiobone_t *)( (byte *)pstudiohdr + pstudiohdr->boneindex );

	R_StudioCalcBoneAdj( dadt, adj, currententity->curstate.controller, currententity->latched.prevcontroller, currententity->mouth.mouthopen );

	for ( i = 0; i < pstudiohdr->numbones; i++, pbone++, panim++ )
	{
		R_StudioCalcBoneQuaterion( frame, s, pbone, panim, adj, q[i] );

		R_StudioCalcBonePosition( frame, s, pbone, panim, adj, pos[i] );
	}

	if ( pseqdesc->motiontype & STUDIO_X )
	{
		pos[pseqdesc->motionbone][0] = 0.0;
	}
	if ( pseqdesc->motiontype & STUDIO_Y )
	{
		pos[pseqdesc->motionbone][1] = 0.0;
	}
	if ( pseqdesc->motiontype & STUDIO_Z )
	{
		pos[pseqdesc->motionbone][2] = 0.0;
	}

	s = 0 * ( ( 1.0 - ( f - (int)( f ) ) ) / ( pseqdesc->numframes ) ) * currententity->curstate.framerate;

	if ( pseqdesc->motiontype & STUDIO_LX )
	{
		pos[pseqdesc->motionbone][0] += s * pseqdesc->linearmovement[0];
	}
	if ( pseqdesc->motiontype & STUDIO_LY )
	{
		pos[pseqdesc->motionbone][1] += s * pseqdesc->linearmovement[1];
	}
	if ( pseqdesc->motiontype & STUDIO_LZ )
	{
		pos[pseqdesc->motionbone][2] += s * pseqdesc->linearmovement[2];
	}
}

/*
====================
CL_StudioEstimateFrame

====================
*/
float CL_StudioEstimateFrame( mstudioseqdesc_t *pseqdesc )
{
	double				dfdt, f;

	if ( r_dointerp )
	{
		if ( cl.time < currententity->curstate.animtime )
		{
			dfdt = 0;
		}
		else
		{
			dfdt = ( cl.time - currententity->curstate.animtime ) * currententity->curstate.framerate * pseqdesc->fps;
		}
	}
	else
	{
		dfdt = 0;
	}

	if ( pseqdesc->numframes <= 1 )
	{
		f = 0;
	}
	else
	{
		float	baseframe;

		baseframe = currententity->curstate.frame;
		f = ( baseframe * ( pseqdesc->numframes - 1 ) ) / 256.0;
	}

	f += dfdt;

	if ( pseqdesc->flags & STUDIO_LOOPING )
	{
		if ( pseqdesc->numframes > 1 )
		{
			f -= (int)( f / ( pseqdesc->numframes - 1 ) ) * ( pseqdesc->numframes - 1 );
		}
		if ( f < 0 )
		{
			f += ( pseqdesc->numframes - 1 );
		}
	}
	else
	{
		if ( f >= pseqdesc->numframes - 1.001 )
		{
			f = pseqdesc->numframes - 1.001;
		}
		if ( f < 0.0 )
		{
			f = 0.0;
		}
	}
	return f;
}

/*
====================
CL_GetEstimatedFrame

====================
*/
float CL_GetEstimatedFrame( cl_entity_t *ent )
{
	float				f;
	studiohdr_t			*pstudiohdr;
	mstudioseqdesc_t	*pseqdesc;
	cl_entity_t			*oldce;

	oldce = currententity;

	if ( !ent->model )
		return 0;

	if ( ent->model->type != mod_studio )
		return 0;

	pstudiohdr = (studiohdr_t *)Mod_Extradata( ent->model );
	if ( !pstudiohdr )
		return 0;

	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + ent->curstate.sequence;

	currententity = ent;
	f = CL_StudioEstimateFrame( pseqdesc );
	currententity = oldce;

	return f;
}

/*
====================
R_StudioSetupBones

====================
*/
void R_StudioSetupBones( void )
{
	int					i;
	double				f;

	mstudiobone_t		*pbones;
	mstudioseqdesc_t	*pseqdesc;
	mstudioanim_t		*panim;

	static float		pos[MAXSTUDIOBONES][3];
	static vec4_t		q[MAXSTUDIOBONES];
	float				bonematrix[3][4];

	static float		pos2[MAXSTUDIOBONES][3];
	static vec4_t		q2[MAXSTUDIOBONES];
	static float		pos3[MAXSTUDIOBONES][3];
	static vec4_t		q3[MAXSTUDIOBONES];
	static float		pos4[MAXSTUDIOBONES][3];
	static vec4_t		q4[MAXSTUDIOBONES];

	if ( currententity->curstate.sequence >= pstudiohdr->numseq )
	{
		currententity->curstate.sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + currententity->curstate.sequence;

	f = CL_StudioEstimateFrame( pseqdesc );

	if ( currententity->latched.prevframe > f )
		;	// Con_DPrintf( "%f %f\n", currententity->latched.prevframe, f );

	panim = R_GetAnim( r_model, pseqdesc );
	R_StudioCalcRotations( pos, q, pseqdesc, panim, f );

	if ( pseqdesc->numblends > 1 )
	{
		float				s;
		float				dadt;

		panim += pstudiohdr->numbones;
		R_StudioCalcRotations( pos2, q2, pseqdesc, panim, f );

		dadt = CL_StudioEstimateInterpolant();
		s = ( currententity->curstate.blending[0] * dadt + currententity->latched.prevblending[0] * ( 1.0 - dadt ) ) / 255.0;

		R_StudioSlerpBones( q, pos, q2, pos2, s );

		if ( pseqdesc->numblends == 4 )
		{
			panim += pstudiohdr->numbones;
			R_StudioCalcRotations( pos3, q3, pseqdesc, panim, f );

			panim += pstudiohdr->numbones;
			R_StudioCalcRotations( pos4, q4, pseqdesc, panim, f );

			s = ( currententity->curstate.blending[0] * dadt + currententity->latched.prevblending[0] * ( 1.0 - dadt ) ) / 255.0;
			R_StudioSlerpBones( q3, pos3, q4, pos4, s );

			s = ( currententity->curstate.blending[1] * dadt + currententity->latched.prevblending[1] * ( 1.0 - dadt ) ) / 255.0;
			R_StudioSlerpBones( q, pos, q3, pos3, s );
		}
	}

	if ( r_dointerp &&
		currententity->latched.sequencetime &&
		( currententity->latched.sequencetime + 0.2 > cl.time ) &&
		( currententity->latched.prevsequence < pstudiohdr->numseq ) )
	{
		// blend from last sequence
		static float		pos1b[MAXSTUDIOBONES][3];
		static vec4_t		q1b[MAXSTUDIOBONES];
		float				s;

		pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + currententity->latched.prevsequence;
		panim = R_GetAnim( r_model, pseqdesc );
		// clip prevframe
		R_StudioCalcRotations( pos1b, q1b, pseqdesc, panim, currententity->latched.prevframe );

		if ( pseqdesc->numblends > 1 )
		{
			panim += pstudiohdr->numbones;
			R_StudioCalcRotations( pos2, q2, pseqdesc, panim, currententity->latched.prevframe );

			s = ( currententity->latched.prevseqblending[0] ) / 255.0;
			R_StudioSlerpBones( q1b, pos1b, q2, pos2, s );

			if ( pseqdesc->numblends == 4 )
			{
				panim += pstudiohdr->numbones;
				R_StudioCalcRotations( pos3, q3, pseqdesc, panim, currententity->latched.prevframe );

				panim += pstudiohdr->numbones;
				R_StudioCalcRotations( pos4, q4, pseqdesc, panim, currententity->latched.prevframe );

				s = ( currententity->latched.prevseqblending[0] ) / 255.0;
				R_StudioSlerpBones( q3, pos3, q4, pos4, s );

				s = ( currententity->latched.prevseqblending[1] ) / 255.0;
				R_StudioSlerpBones( q1b, pos1b, q3, pos3, s );
			}
		}

		s = 1.0 - ( cl.time - currententity->latched.sequencetime ) / 0.2;
		R_StudioSlerpBones( q, pos, q1b, pos1b, s );
	}
	else
	{
		currententity->latched.prevframe = f;
	}

	pbones = (mstudiobone_t *)( (byte *)pstudiohdr + pstudiohdr->boneindex );

	// calc gait animation
	if ( r_playerinfo && r_playerinfo->gaitsequence != 0 )
	{
		if ( r_playerinfo->gaitsequence >= pstudiohdr->numseq )
		{
			r_playerinfo->gaitsequence = 0;
		}

		pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + r_playerinfo->gaitsequence;

		panim = R_GetAnim( r_model, pseqdesc );
		R_StudioCalcRotations( pos2, q2, pseqdesc, panim, r_playerinfo->gaitframe );

		for ( i = 0; i < pstudiohdr->numbones; i++ )
		{
			if ( !Q_strcmp( pbones[i].name, "Bip01 Spine" ) )
				break;

			Q_memcpy( pos[i], pos2[i], sizeof( pos[i] ) );
			Q_memcpy( q[i], q2[i], sizeof( q[i] ) );
		}
	}

	for ( i = 0; i < pstudiohdr->numbones; i++ )
	{
		QuaternionMatrix( q[i], bonematrix );

		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if ( pbones[i].parent == -1 )
		{
			R_ConcatTransforms( rotationmatrix, bonematrix, bonetransform[i] );
			R_ConcatTransforms( rotationmatrix, bonematrix, lighttransform[i] );

			// Apply client-side effects to the transformation matrix
			CL_FxTransform( currententity, bonetransform[i] );
		}
		else
		{
			R_ConcatTransforms( bonetransform[pbones[i].parent], bonematrix, bonetransform[i] );
			R_ConcatTransforms( lighttransform[pbones[i].parent], bonematrix, lighttransform[i] );
		}
	}
}

/*
====================
MatrixCopy

====================
*/
void MatrixCopy( float in[3][4], float out[3][4] )
{
	Q_memcpy( out, in, sizeof( float ) * 3 * 4 );
}

/*
====================
R_StudioSaveBones

====================
*/
void R_StudioSaveBones( void )
{
	int					i;

	mstudiobone_t		*pbones;
	pbones = (mstudiobone_t *)( (byte *)pstudiohdr + pstudiohdr->boneindex );

	cached_numbones = pstudiohdr->numbones;

	for ( i = 0; i < pstudiohdr->numbones; i++ )
	{
		Q_strncpy( cached_bonename[i], pbones[i].name, sizeof( cached_bonename[i] ) - 1 );
		cached_bonename[i][sizeof( cached_bonename[i] ) - 1] = 0;
		MatrixCopy( bonetransform[i], cached_bonetransform[i] );
		MatrixCopy( lighttransform[i], cached_lighttransform[i] );
	}
}

/*
====================
R_StudioMergeBones

====================
*/
void R_StudioMergeBones( model_t *psubmodel )
{
	int					i, j;
	double				f;
	int					do_hunt = TRUE;

	mstudiobone_t		*pbones;
	mstudioseqdesc_t	*pseqdesc;
	mstudioanim_t		*panim;

	static float		pos[MAXSTUDIOBONES][3];
	float				bonematrix[3][4];
	static vec4_t		q[MAXSTUDIOBONES];

	if ( currententity->curstate.sequence >= pstudiohdr->numseq )
	{
		currententity->curstate.sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + currententity->curstate.sequence;

	f = 0;
	f = CL_StudioEstimateFrame( pseqdesc );

	if ( currententity->latched.prevframe > f )
		;	// Con_DPrintf( "%f %f\n", currententity->latched.prevframe, f );

	panim = R_GetAnim( psubmodel, pseqdesc );
	R_StudioCalcRotations( pos, q, pseqdesc, panim, f );

	pbones = (mstudiobone_t *)( (byte *)pstudiohdr + pstudiohdr->boneindex );

	for ( i = 0; i < pstudiohdr->numbones; i++ )
	{
		for ( j = 0; j < cached_numbones; j++ )
		{
			if ( Q_stricmp( pbones[i].name, cached_bonename[j] ) == 0 )
			{
				MatrixCopy( cached_bonetransform[j], bonetransform[i] );
				MatrixCopy( cached_lighttransform[j], lighttransform[i] );
				break;
			}
		}
		if ( j >= cached_numbones )
		{
			QuaternionMatrix( q[i], bonematrix );

			bonematrix[0][3] = pos[i][0];
			bonematrix[1][3] = pos[i][1];
			bonematrix[2][3] = pos[i][2];

			if ( pbones[i].parent == -1 )
			{
				R_ConcatTransforms( rotationmatrix, bonematrix, bonetransform[i] );
				R_ConcatTransforms( rotationmatrix, bonematrix, lighttransform[i] );

				// Apply client-side effects to the transformation matrix
				CL_FxTransform( currententity, bonetransform[i] );
			}
			else
			{
				R_ConcatTransforms( bonetransform[pbones[i].parent], bonematrix, bonetransform[i] );
				R_ConcatTransforms( lighttransform[pbones[i].parent], bonematrix, lighttransform[i] );
			}
		}
	}
}

int		boxpnt[6][4] =
{
	{ 0, 4, 6, 2 },	// +X
	{ 0, 1, 5, 4 },	// +Y
	{ 0, 2, 3, 1 },	// +Z
	{ 7, 5, 1, 3 },	// -X
	{ 7, 3, 2, 6 },	// -Y
	{ 7, 6, 4, 5 },	// -Z
};

vec3_t	hullcolor[8] =
{
	{ 1.0, 1.0, 1.0 },
	{ 1.0, 0.5, 0.5 },
	{ 0.5, 1.0, 0.5 },
	{ 1.0, 1.0, 0.5 },
	{ 0.5, 0.5, 1.0 },
	{ 1.0, 0.5, 1.0 },
	{ 0.5, 1.0, 1.0 },
	{ 1.0, 1.0, 1.0 },
};

/*
================
R_StudioDrawHulls
================
*/
void R_StudioDrawHulls( void )
{
	int				i, j;
	float			lv;
	vec3_t			tmp;
	vec3_t			p[8];
	mstudiobbox_t	*pbbox;

	pbbox = (mstudiobbox_t *)( (byte *)pstudiohdr + pstudiohdr->hitboxindex );

	tri.SpriteTexture( cl_sprite_white, 0 );

	for ( i = 0; i < pstudiohdr->numhitboxes; i++ )
	{
		for ( j = 0; j < 8; j++ )
		{
			tmp[0] = ( j & 1 ) ? pbbox[i].bbmin[0] : pbbox[i].bbmax[0];
			tmp[1] = ( j & 2 ) ? pbbox[i].bbmin[1] : pbbox[i].bbmax[1];
			tmp[2] = ( j & 4 ) ? pbbox[i].bbmin[2] : pbbox[i].bbmax[2];

			VectorTransform( tmp, lighttransform[pbbox[i].bone], p[j] );
		}

		tri.Begin( TRI_QUADS );

		j = ( pbbox[i].group % 8 );
		tri.Color4f( hullcolor[j][0], hullcolor[j][1], hullcolor[j][2], 1.0 );
		tri.TexCoord2f( 0, 0 );

		for ( j = 0; j < 6; j++ )
		{
			tmp[0] = tmp[1] = tmp[2] = 0;
			tmp[j % 3] = ( j < 3 ) ? 1.0 : -1.0;

			R_StudioLighting( &lv, pbbox[i].bone, 0, tmp );
			tri.Brightness( lv );

			tri.Vertex3fv( p[boxpnt[j][0]] );
			tri.Vertex3fv( p[boxpnt[j][1]] );
			tri.Vertex3fv( p[boxpnt[j][2]] );
			tri.Vertex3fv( p[boxpnt[j][3]] );
		}

		tri.End();
	}
}

/*
================
R_StudioAbsBB
================
*/
void R_StudioAbsBB( void )
{
	int					j;
	float				lv;
	vec3_t				tmp;
	vec3_t				p[8];
	mstudiobone_t		*pbones;
	mstudioseqdesc_t	*pseqdesc;

	pbones = (mstudiobone_t *)( (byte *)pstudiohdr + pstudiohdr->boneindex );

	// get the sequence bounding box
	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + currententity->curstate.sequence;

	tri.RenderMode( kRenderTransAdd );
	tri.SpriteTexture( cl_sprite_white, 0 );

	for ( j = 0; j < 8; j++ )
	{
		p[j][0] = ( j & 1 ) ? pseqdesc->bbmin[0] : pseqdesc->bbmax[0];
		p[j][1] = ( j & 2 ) ? pseqdesc->bbmin[1] : pseqdesc->bbmax[1];
		p[j][2] = ( j & 4 ) ? pseqdesc->bbmin[2] : pseqdesc->bbmax[2];

		VectorAdd( p[j], currententity->origin, p[j] );
	}

	tri.Begin( TRI_QUADS );
	tri.Color4f( 0.5, 0.5, 1, 1 );

	for ( j = 0; j < 6; j++ )
	{
		tmp[0] = tmp[1] = tmp[2] = 0;
		tmp[j % 3] = ( j < 3 ) ? 1.0 : -1.0;

		R_StudioLighting( &lv, -1, 0, tmp );
		tri.Brightness( lv );

		tri.Vertex3fv( p[boxpnt[j][0]] );
		tri.Vertex3fv( p[boxpnt[j][1]] );
		tri.Vertex3fv( p[boxpnt[j][2]] );
		tri.Vertex3fv( p[boxpnt[j][3]] );
	}

	tri.End();
	tri.RenderMode( kRenderNormal );
}

/*
================
R_StudioDrawBones
================
*/
void R_StudioDrawBones( void )
{
	int				i, j, k;
	float			lv;
	vec3_t			tmp;
	vec3_t			p[8];
	vec3_t			up, right, forward;
	vec3_t			a1;
	mstudiobone_t	*pbones;

	pbones = (mstudiobone_t *)( (byte *)pstudiohdr + pstudiohdr->boneindex );

	tri.SpriteTexture( cl_sprite_white, 0 );

	for ( i = 0; i < pstudiohdr->numbones; i++ )
	{
		if ( pbones[i].parent == -1 )
			continue;

		k = pbones[i].parent;

		a1[0] = a1[1] = a1[2] = 1.0;

		up[0] = lighttransform[i][0][3] - lighttransform[k][0][3];
		up[1] = lighttransform[i][1][3] - lighttransform[k][1][3];
		up[2] = lighttransform[i][2][3] - lighttransform[k][2][3];

		if ( up[0] > up[1] )
		{
			if ( up[0] > up[2] )
				a1[0] = 0;
			else
				a1[2] = 0;
		}
		else
		{
			if ( up[1] > up[2] )
				a1[1] = 0;
			else
				a1[2] = 0;
		}

		CrossProduct( up, a1, right );
		VectorNormalize( right );
		CrossProduct( up, right, forward );
		VectorNormalize( forward );
		VectorScale( right, 2, right );
		VectorScale( forward, 2, forward );

		for ( j = 0; j < 8; j++ )
		{
			p[j][0] = lighttransform[k][0][3];
			p[j][1] = lighttransform[k][1][3];
			p[j][2] = lighttransform[k][2][3];

			if ( j & 1 )
			{
				VectorSubtract( p[j], right, p[j] );
			}
			else
			{
				VectorAdd( p[j], right, p[j] );
			}

			if ( j & 2 )
			{
				VectorSubtract( p[j], forward, p[j] );
			}
			else
			{
				VectorAdd( p[j], forward, p[j] );
			}

			if ( j & 4 )
			{
			}
			else
			{
				VectorAdd( p[j], up, p[j] );
			}
		}

		VectorNormalize( up );
		VectorNormalize( right );
		VectorNormalize( forward );

		tri.Begin( TRI_QUADS );
		tri.Color4f( 1, 1, 1, 1 );
		tri.TexCoord2f( 0, 0 );

		for ( j = 0; j < 6; j++ )
		{
			switch ( j )
			{
			case 0:
				VectorCopy( right, tmp );
				break;
			case 1:
				VectorCopy( forward, tmp );
				break;
			case 2:
				VectorCopy( up, tmp );
				break;
			case 3:
				VectorScale( right, -1, tmp );
				break;
			case 4:
				VectorScale( forward, -1, tmp );
				break;
			case 5:
				VectorScale( up, -1, tmp );
				break;
			}

			R_StudioLighting( &lv, -1, 0, tmp );
			tri.Brightness( lv );

			tri.Vertex3fv( p[boxpnt[j][0]] );
			tri.Vertex3fv( p[boxpnt[j][1]] );
			tri.Vertex3fv( p[boxpnt[j][2]] );
			tri.Vertex3fv( p[boxpnt[j][3]] );
		}

		tri.End();
	}
}

/*
================
R_StudioTransformAuxVert
================
*/
void R_StudioTransformAuxVert( auxvert_t *av, int bone, vec_t *vert )
{
	av->fv[0] = DotProduct( vert, bonetransform[bone][0] ) + bonetransform[bone][0][3];
	av->fv[1] = DotProduct( vert, bonetransform[bone][1] ) + bonetransform[bone][1][3];
	av->fv[2] = DotProduct( vert, bonetransform[bone][2] ) + bonetransform[bone][2][3];
}

/*
================
R_StudioLighting
================
*/
void R_StudioLighting( float *lv, int bone, int flags, vec_t *normal )
{
	float	illum;
	float	lightcos;

	illum = r_ambientlight;

	if ( ( flags & STUDIO_NF_FLATSHADE ) && r_flatshade_guess != 1 )
	{
		illum += r_shadelight * 0.8;
	}
	else
	{
		float	r;

		if ( bone != -1 )
			lightcos = DotProduct( normal, r_blightvec[bone] );	// -1 colinear, 1 opposite
		else
			lightcos = DotProduct( normal, r_plightvec );

		if ( lightcos > 1.0 )
			lightcos = 1;

		r = v_lambert1;

		if ( r >= 1.0 )
		{
			illum += r_shadelight;

			lightcos = ( lightcos + ( r - 1.0 ) ) / r;	// do modified hemispherical lighting
			if ( lightcos > 0.0 )
			{
				illum -= r_shadelight * lightcos;
			}
		}
		else
		{
			lightcos = ( -lightcos + r ) / ( r + 1.0f );
			if ( lightcos > 0.0f )
			{
				illum += r_shadelight * lightcos;
			}
		}

		if ( illum <= 0 )
			illum = 0;
	}

	if ( illum > 255 )
		illum = 255;

	illum = lightgammatable[(int)( illum * 4 )];
	*lv = illum * ( 1.0 / 1023.0 );	// Light from 0 to 1.0
}

/*
================
R_LightStrength
================
*/
void R_LightStrength( int bone, float *vert, float (*light)[4] )
{
	int		i;

	if ( lightage[bone] != r_smodels_total )
	{
		for ( i = 0; i < numlights; i++ )
		{
			vec3_t	lpos;

			lpos[0] = locallight[i]->origin[0] - lighttransform[bone][0][3];
			lpos[1] = locallight[i]->origin[1] - lighttransform[bone][1][3];
			lpos[2] = locallight[i]->origin[2] - lighttransform[bone][2][3];

			VectorIRotate( lpos, lighttransform[bone], lightbonepos[bone][i] );
		}

		lightage[bone] = r_smodels_total;
	}

	for ( i = 0; i < numlights; i++ )
	{
		VectorSubtract( vert, lightbonepos[bone][i], light[i] );
		light[i][3] = 0;
	}
}

/*
================
R_LightLambert
================
*/
void R_LightLambert( float (*light)[4], float *normal, float *src, float *lambert )
{
	int		i;
	float	adjr, adjg, adjb;
	float	c;

	if ( !numlights )
	{
		VectorCopy( src, lambert );
		return;
	}

	adjr = adjg = adjb = 0;

	for ( i = 0; i < numlights; i++ )
	{
		c = -DotProduct( normal, light[i] );

		if ( c > 0 )
		{
			if ( light[i][3] == 0.0 )
			{
				float	lengthsq_guess;
				float	length_guess;

				lengthsq_guess = DotProduct( light[i], light[i] );
				if ( lengthsq_guess > 0.0 )
				{
					length_guess = sqrt( lengthsq_guess );
					light[i][3] = locallightR2[i] / ( lengthsq_guess * length_guess );
				}
				else
				{
					light[i][3] = 1.0;
				}
			}

			c *= light[i][3];

			adjr += locallinearlight[i][0] * c;
			adjg += locallinearlight[i][1] * c;
			adjb += locallinearlight[i][2] * c;
		}
	}

	if ( adjr != 0 || adjg != 0 || adjb != 0 )
	{
		int		value_guess;

		value_guess = lineargammatable[(int)( src[0] * 1023 )] + adjr;
		if ( value_guess > 1023 )
			lambert[0] = 1.0;
		else
			lambert[0] = screengammatable[value_guess] / 1023.0;

		value_guess = lineargammatable[(int)( src[1] * 1023 )] + adjg;
		if ( value_guess > 1023 )
			lambert[1] = 1.0;
		else
			lambert[1] = screengammatable[value_guess] / 1023.0;

		value_guess = lineargammatable[(int)( src[2] * 1023 )] + adjb;
		if ( value_guess > 1023 )
			lambert[2] = 1.0;
		else
			lambert[2] = screengammatable[value_guess] / 1023.0;
	}
	else
	{
		VectorCopy( src, lambert );
	}
}

/*
================
R_StudioChrome
================
*/
void R_StudioChrome( int *pchrome, int bone, vec_t *normal )
{
	float	n;

	if ( chromeage[bone] != r_smodels_total )
	{
		// calculate vectors from the viewer to the bone. This roughly adjusts for position
		vec3_t	chromeupvec;		// chrome t vector in world reference frame
		vec3_t	chromerightvec;		// chrome s vector in world reference frame
		vec3_t	tmp;				// vector pointing at bone in world reference frame

		VectorScale( g_ChromeOrigin, -1, tmp );
		tmp[0] += lighttransform[bone][0][3];
		tmp[1] += lighttransform[bone][1][3];
		tmp[2] += lighttransform[bone][2][3];
		VectorNormalize( tmp );
		CrossProduct( tmp, vright, chromeupvec );
		VectorNormalize( chromeupvec );
		CrossProduct( chromeupvec, tmp, chromerightvec );
		VectorNormalize( chromerightvec );

		VectorIRotate( chromeupvec, lighttransform[bone], r_chromeup[bone] );
		VectorIRotate( chromerightvec, lighttransform[bone], r_chromeright[bone] );

		chromeage[bone] = r_smodels_total;
	}

	// calc s coord
	n = DotProduct( normal, r_chromeright[bone] );
	pchrome[0] = ( n + 1.0 ) * 32 * 1024;

	// calc t coord
	n = DotProduct( normal, r_chromeup[bone] );
	pchrome[1] = ( n + 1.0 ) * 32 * 1024;
}

/*
================
R_LoadTextures
================
*/
studiohdr_t *R_LoadTextures( model_t *psubmodel )
{
	model_t		*texmodel;

	if ( pstudiohdr->textureindex )
		return pstudiohdr;

	texmodel = (model_t *)psubmodel->texinfo;

	if ( !texmodel || !Cache_Check( &texmodel->cache ) )
	{
		studiohdr_t	*ptexturehdr;
		char		modelname[MAX_PATH];

		Q_strncpy( modelname, psubmodel->name, sizeof( modelname ) - 2 );
		modelname[sizeof( modelname ) - 2] = 0;
		Q_strcpy( &modelname[Q_strlen( modelname ) - 4], "T.mdl" );

		texmodel = Mod_ForName( modelname, TRUE, FALSE );
		psubmodel->texinfo = (mtexinfo_t *)texmodel;

		ptexturehdr = (studiohdr_t *)texmodel->cache.data;
		Q_strncpy( ptexturehdr->name, modelname, sizeof( ptexturehdr->name ) - 1 );
		ptexturehdr->name[sizeof( ptexturehdr->name ) - 1] = 0;
	}

	return (studiohdr_t *)texmodel->cache.data;
}

/*
================
PaletteHueReplace
================
*/
void PaletteHueReplace( byte *palette, int newHue, int start, int end )
{
	int		i;
	float	r, b, g;
	float	maxcol, mincol;
	float	hue, val, sat;

	hue = (float)( newHue * ( 360.0 / 255 ) );

	for ( i = start; i <= end; i++ )
	{
		r = palette[i * 3 + 0];
		g = palette[i * 3 + 1];
		b = palette[i * 3 + 2];

		maxcol = max( max( r, g ), b ) / 255.0;
		mincol = min( min( r, g ), b ) / 255.0;

		val = maxcol;
		sat = ( maxcol - mincol ) / maxcol;

		mincol = val * ( 1.0 - sat );

		if ( hue <= 120 )
		{
			b = mincol;
			if ( hue < 60 )
			{
				r = val;
				g = mincol + hue * ( val - mincol ) / ( 120 - hue );
			}
			else
			{
				g = val;
				r = mincol + ( 120 - hue ) * ( val - mincol ) / hue;
			}
		}
		else if ( hue <= 240 )
		{
			r = mincol;
			if ( hue < 180 )
			{
				g = val;
				b = mincol + ( hue - 120 ) * ( val - mincol ) / ( 240 - hue );
			}
			else
			{
				b = val;
				g = mincol + ( 240 - hue ) * ( val - mincol ) / ( hue - 120 );
			}
		}
		else
		{
			g = mincol;
			if ( hue < 300 )
			{
				b = val;
				r = mincol + ( hue - 240 ) * ( val - mincol ) / ( 360 - hue );
			}
			else
			{
				r = val;
				b = mincol + ( 360 - hue ) * ( val - mincol ) / ( hue - 240 );
			}
		}

		palette[i * 3 + 0] = r * 255;
		palette[i * 3 + 1] = g * 255;
		palette[i * 3 + 2] = b * 255;
	}
}

/*
================
R_StudioFlushSkins
================
*/
void R_StudioFlushSkins( int keynum )
{
	int		index;

	for ( index = 0; index < 11; index++ )
	{
		if ( pDM_RemapSkin[keynum][index] )
			pDM_RemapSkin[keynum][index]->model = NULL;
	}
}

/*
================
R_StudioGetSkin
================
*/
skin_t *R_StudioGetSkin( int keynum, int index )
{
	skin_t	*pskin;

	if ( index >= 11 )
		index = 0;

	pskin = pDM_RemapSkin[keynum][index];

	if ( !pskin || pskin->keynum != keynum )
	{
		pskin = &DM_RemapSkin[r_remapindex][index];
		r_remapindex = ( r_remapindex + 1 ) % 64;
		pDM_RemapSkin[keynum][index] = pskin;
		pskin->keynum = keynum;
		pskin->topcolor = -1;
		pskin->bottomcolor = -1;
	}

	return pskin;
}

/*
================
R_IsRemapSkin
================
*/
int R_IsRemapSkin( const char *texture, int *low, int *mid, int *high )
{
	char	sz[32];
	char	*p;
	int		len;
	char	ch;

	// skin name must begin with "Remap"
	if ( Q_strnicmp( (char *)texture, "Remap", 5 ) )
		return 0;

	len = Q_strlen( (char *)texture );
	if ( len != 22 && len != 18 )
		return 0;

	ch = texture[5];
	if ( len == 18 && ch != 'C' && ch != 'c' )
		return 0;

	Q_memset( sz, 0, sizeof( sz ) );

	p = (char *)texture + 7;
	Q_strncpy( sz, p, 3 );
	*low = Q_atoi( sz );

	p += 4;
	Q_strncpy( sz, p, 3 );
	*mid = Q_atoi( sz );

	if ( len == 22 )
	{
		p += 4;
		Q_strncpy( sz, p, 3 );
		*high = Q_atoi( sz );
	}
	else
	{
		*high = 0;
	}

	return 1;
}

/*
================
R_StudioReloadSkin
================
*/
byte *R_StudioReloadSkin( model_t *pModel, int index, skin_t *pskin )
{
	int					modelindex;
	cache_user_t		*pCache;
	int					*pData;
	mstudiotexture_t	*ptexture;
	byte				*pbase;
	int					size;

	modelindex = pModel - mod_known;
	if ( modelindex < 0 || modelindex >= MAX_KNOWN_MODELS )
		return NULL;

	pCache = &model_texture_cache[modelindex][index];

	if ( !Cache_Check( pCache ) )
	{
		pbase = COM_LoadFileForMe( pModel->name, NULL );
		ptexture = (mstudiotexture_t *)( pbase + ( (studiohdr_t *)pbase )->textureindex ) + index;
		size = ptexture->width * ptexture->height + 768 + 8;

		Cache_Alloc( pCache, size, pskin->name );

		pData = (int *)pCache->data;
		pData[0] = ptexture->width;
		pData[1] = ptexture->height;
		Q_memcpy( &pData[2], pbase + ptexture->index, size - 8 );

		Mem_Free( pbase );
	}
	else
	{
		pData = (int *)pCache->data;
	}

	pskin->index = index;
	pskin->width = pData[0];
	pskin->height = pData[1];

	return (byte *)&pData[2];
}

/*
================
R_StudioSetupSkin
================
*/
void R_StudioSetupSkin( studiohdr_t *ptexturehdr, int index )
{
	mstudiotexture_t	*ptexture;
	float				temp = 0;

	if ( g_ForcedFaceFlags & STUDIO_NF_CHROME )
		return;

	ptexture = (mstudiotexture_t *)( (byte *)ptexturehdr + ptexturehdr->textureindex ) + index;

	if ( currententity->index > 0 )
	{
		int		l, m, h;

		l = 160;
		m = 191;
		h = 223;

		if ( !Q_stricmp( ptexture->name, "DM_Base.bmp" ) || R_IsRemapSkin( ptexture->name, &l, &m, &h ) )
		{
			skin_t	*pskin;

			pskin = R_StudioGetSkin( currententity->index, index );

			if ( pskin->model != r_model || pskin->topcolor != r_topcolor || pskin->bottomcolor != r_bottomcolor )
			{
				static byte	tmp_palette[768];
				byte		*orig_palette;
				char		name[256];
				byte		*pData;

				if ( pskin->model )
					R_StudioFlushSkins( currententity->index );

				pData = R_StudioReloadSkin( r_model, index, pskin );
				if ( !pData )
					return;

				_snprintf( name, sizeof( name ), "%s%d", ptexture->name, currententity->index );

				orig_palette = pData + ptexture->width * ptexture->height;
				Q_memcpy( tmp_palette, orig_palette, sizeof( tmp_palette ) );

				pskin->model = r_model;
				pskin->topcolor = r_topcolor;
				pskin->bottomcolor = r_bottomcolor;

				PaletteHueReplace( tmp_palette, pskin->topcolor, l, m );
				if ( h )
					PaletteHueReplace( tmp_palette, pskin->bottomcolor, m + 1, h );

				GL_UnloadTexture( name );

				temp = gl_dither.value;
				gl_dither.value = 0;
				pskin->gl_index = GL_LoadTexture( name, GLT_STUDIO, pskin->width, pskin->height, pData, FALSE, 0, tmp_palette );
				gl_dither.value = temp;
			}

			if ( pskin->gl_index )
			{
				GL_Bind( pskin->gl_index );
				return;
			}
		}
	}

	GL_Bind( ptexture->index );
}

/*
================
R_StudioSetupLighting
================
*/
void R_StudioSetupLighting( alight_t *plighting )
{
	int		i;

	r_ambientlight = plighting->ambientlight;
	r_shadelight = plighting->shadelight;
	VectorCopy( plighting->plightvec, r_plightvec );

	for ( i = 0; i < pstudiohdr->numbones; i++ )
	{
		VectorIRotate( r_plightvec, lighttransform[i], r_blightvec[i] );
	}

	r_icolormix.r = (int)( plighting->color[0] * 49407 ) & 0xFF00;
	r_icolormix.g = (int)( plighting->color[1] * 49407 ) & 0xFF00;
	r_icolormix.b = (int)( plighting->color[2] * 49407 ) & 0xFF00;

	VectorCopy( plighting->color, r_colormix );
}

/*
================
R_StudioSetupModel
================
*/
void R_StudioSetupModel( int bodypart )
{
	int		index;

	if ( bodypart > pstudiohdr->numbodyparts )
	{
		Con_DPrintf( "R_StudioSetupModel: no such bodypart %d\n", bodypart );
		bodypart = 0;
	}

	pbodypart = (mstudiobodyparts_t *)( (byte *)pstudiohdr + pstudiohdr->bodypartindex ) + bodypart;

	index = currententity->curstate.body / pbodypart->base;
	index = index % pbodypart->nummodels;

	psubmodel = (mstudiomodel_t *)( (byte *)pstudiohdr + pbodypart->modelindex ) + index;
}

/*
================
R_StudioDrawModel
================
*/
int R_StudioDrawModel( int flags )
{
	alight_t	lighting;
	vec3_t		dir;

	if ( currententity->curstate.renderfx == kRenderFxDeadPlayer )
	{
		entity_state_t	deadplayer;

		int		result;
		int		save_interp;

		if ( currententity->curstate.renderamt <= 0 || currententity->curstate.renderamt > cl.maxclients )
			return 0;

		// get copy of player
		deadplayer = cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->curstate.renderamt - 1];

		// clear weapon, movement state
		deadplayer.number = currententity->curstate.renderamt;
		deadplayer.weaponmodel = 0;
		deadplayer.gaitsequence = 0;

		deadplayer.movetype = MOVETYPE_NONE;
		VectorCopy( currententity->curstate.angles, deadplayer.angles );
		VectorCopy( currententity->curstate.origin, deadplayer.origin );

		save_interp = r_dointerp;
		r_dointerp = 0;

		// draw as though it were a player
		result = R_StudioDrawPlayer( flags, &deadplayer );

		r_dointerp = save_interp;
		return result;
	}

	r_model = currententity->model;
	pstudiohdr = (studiohdr_t *)Mod_Extradata( r_model );

	R_StudioSetUpTransform( 0 );

	if ( flags & STUDIO_RENDER )
	{
		// see if the bounding box lets us trivially reject, also sets
		if ( !R_StudioCheckBBox() )
			return 0;

		r_amodels_drawn++;
		r_smodels_total++; // render data cache cookie

		if ( pstudiohdr->numbodyparts == 0 )
			return 1;
	}

	if ( currententity->curstate.movetype == MOVETYPE_FOLLOW )
	{
		R_StudioMergeBones( r_model );
	}
	else
	{
		R_StudioSetupBones();
	}
	R_StudioSaveBones();

	if ( flags & STUDIO_EVENTS )
	{
		R_StudioCalcAttachments();
		R_StudioClientEvents();

		// copy attachments into global entity array
		if ( currententity->index > 0 )
		{
			Q_memcpy( cl_entities[currententity->index].attachment, currententity->attachment, sizeof( vec3_t ) * 4 );
		}
	}

	if ( flags & STUDIO_RENDER )
	{
		lighting.plightvec = dir;
		R_StudioDynamicLight( currententity, &lighting );

		R_StudioEntityLight( &lighting );

		// model and frame independant
		R_StudioSetupLighting( &lighting );

		// get remap colors
		r_topcolor = currententity->curstate.colormap & 0xFF;
		r_bottomcolor = ( currententity->curstate.colormap & 0xFF00 ) >> 8;

		R_StudioRenderModel();
	}

	return 1;
}

/*
================
R_StudioEstimateGait
================
*/
void R_StudioEstimateGait( entity_state_t *pplayer )
{
	float	dt;
	vec3_t	est_velocity;

	dt = ( cl.time - cl.oldtime );
	if ( dt < 0 )
		dt = 0;
	else if ( dt > 1.0 )
		dt = 1;

	if ( dt == 0 || r_playerinfo->renderframe == r_framecount )
	{
		r_gaitmovement = 0;
		return;
	}

	if ( cl_gaitestimation.value )
	{
		VectorSubtract( currententity->origin, r_playerinfo->prevgaitorigin, est_velocity );
		VectorCopy( currententity->origin, r_playerinfo->prevgaitorigin );
		r_gaitmovement = Length( est_velocity );
		if ( dt <= 0 || r_gaitmovement / dt < 5 )
		{
			r_gaitmovement = 0;
			est_velocity[0] = 0;
			est_velocity[1] = 0;
		}
	}
	else
	{
		VectorCopy( pplayer->velocity, est_velocity );
		r_gaitmovement = Length( est_velocity ) * dt;
	}

	if ( est_velocity[1] == 0 && est_velocity[0] == 0 )
	{
		float	flYawDiff;

		flYawDiff = currententity->angles[YAW] - r_playerinfo->gaityaw;
		flYawDiff = flYawDiff - (int)( flYawDiff / 360 ) * 360;
		if ( flYawDiff > 180 )
			flYawDiff -= 360;
		if ( flYawDiff < -180 )
			flYawDiff += 360;

		if ( dt < 0.25 )
			flYawDiff *= dt * 4;
		else
			flYawDiff *= dt;

		r_playerinfo->gaityaw += flYawDiff;
		r_playerinfo->gaityaw = r_playerinfo->gaityaw - (int)( r_playerinfo->gaityaw / 360 ) * 360;

		r_gaitmovement = 0;
	}
	else
	{
		r_playerinfo->gaityaw = ( atan2( est_velocity[1], est_velocity[0] ) * 180 / M_PI );
		if ( r_playerinfo->gaityaw > 180 )
			r_playerinfo->gaityaw = 180;
		if ( r_playerinfo->gaityaw < -180 )
			r_playerinfo->gaityaw = -180;
	}
}

/*
================
R_StudioProcessGait
================
*/
void R_StudioProcessGait( entity_state_t *pplayer )
{
	mstudioseqdesc_t	*pseqdesc;
	float				dt;
	int					iBlend;
	float				flYaw;	 // view direction relative to movement

	if ( currententity->curstate.sequence >= pstudiohdr->numseq )
	{
		currententity->curstate.sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + currententity->curstate.sequence;

	R_StudioPlayerBlend( pseqdesc, &iBlend, &currententity->angles[PITCH] );

	currententity->latched.prevangles[PITCH] = currententity->angles[PITCH];
	currententity->curstate.blending[0] = iBlend;
	currententity->latched.prevblending[0] = currententity->curstate.blending[0];
	currententity->latched.prevseqblending[0] = currententity->curstate.blending[0];

	dt = ( cl.time - cl.oldtime );
	if ( dt < 0 )
		dt = 0;
	else if ( dt > 1.0 )
		dt = 1;

	R_StudioEstimateGait( pplayer );

	// calc side to side turning
	flYaw = currententity->angles[YAW] - r_playerinfo->gaityaw;
	flYaw = flYaw - (int)( flYaw / 360 ) * 360;
	if ( flYaw < -180 )
		flYaw = flYaw + 360;
	if ( flYaw > 180 )
		flYaw = flYaw - 360;

	if ( flYaw > 120 )
	{
		r_playerinfo->gaityaw = r_playerinfo->gaityaw - 180;
		r_gaitmovement = -r_gaitmovement;
		flYaw = flYaw - 180;
	}
	else if ( flYaw < -120 )
	{
		r_playerinfo->gaityaw = r_playerinfo->gaityaw + 180;
		r_gaitmovement = -r_gaitmovement;
		flYaw = flYaw + 180;
	}

	// adjust torso
	currententity->curstate.controller[0] = ( ( flYaw / 4.0 ) + 30 ) / ( 60.0 / 255.0 );
	currententity->curstate.controller[1] = ( ( flYaw / 4.0 ) + 30 ) / ( 60.0 / 255.0 );
	currententity->curstate.controller[2] = ( ( flYaw / 4.0 ) + 30 ) / ( 60.0 / 255.0 );
	currententity->curstate.controller[3] = ( ( flYaw / 4.0 ) + 30 ) / ( 60.0 / 255.0 );
	currententity->latched.prevcontroller[0] = currententity->curstate.controller[0];
	currententity->latched.prevcontroller[1] = currententity->curstate.controller[1];
	currententity->latched.prevcontroller[2] = currententity->curstate.controller[2];
	currententity->latched.prevcontroller[3] = currententity->curstate.controller[3];

	currententity->angles[YAW] = r_playerinfo->gaityaw;
	if ( currententity->angles[YAW] < -0 )
		currententity->angles[YAW] += 360;
	currententity->latched.prevangles[YAW] = currententity->angles[YAW];

	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + pplayer->gaitsequence;

	// calc gait frame
	if ( pseqdesc->linearmovement[0] > 0 )
	{
		r_playerinfo->gaitframe += ( r_gaitmovement / pseqdesc->linearmovement[0] ) * pseqdesc->numframes;
	}
	else
	{
		r_playerinfo->gaitframe += pseqdesc->fps * dt;
	}

	// do modulo
	r_playerinfo->gaitframe = r_playerinfo->gaitframe - (int)( r_playerinfo->gaitframe / pseqdesc->numframes ) * pseqdesc->numframes;
	if ( r_playerinfo->gaitframe < 0 )
		r_playerinfo->gaitframe += pseqdesc->numframes;
}

/*
================
R_StudioChangePlayerModel
================
*/
void R_StudioChangePlayerModel( void )
{
	int		i;
	skin_t	*pskin;

	for ( i = 0; i < 11; i++ )
	{
		pskin = R_StudioGetSkin( currententity->index, i );
		pskin->topcolor = -1;
		pskin->bottomcolor = -1;
	}
}

/*
================
WeaponHasAttachments
================
*/
BOOL WeaponHasAttachments( entity_state_t *pplayer )
{
	studiohdr_t	*modelheader = NULL;

	if ( !pplayer )
		return FALSE;

	modelheader = (studiohdr_t *)Mod_Extradata( CL_GetModelByIndex( pplayer->weaponmodel ) );
	return modelheader->numattachments != 0;
}

/*
================
R_StudioDrawPlayer
================
*/
int R_StudioDrawPlayer( int flags, entity_state_t *pplayer )
{
	alight_t	lighting;
	vec3_t		dir;

	r_playerindex = pplayer->number - 1;

	if ( r_playerindex < 0 || r_playerindex >= cl.maxclients )
		return 0;

	if ( ( developer.value == 0 && Host_IsSinglePlayerGame() ) || !cl.players[r_playerindex].model[0] )
	{
		DM_PlayerState[r_playerindex].name[0] = 0;
		if ( DM_PlayerState[r_playerindex].model != currententity->model )
		{
			DM_PlayerState[r_playerindex].model = currententity->model;
			R_StudioChangePlayerModel();
		}
	}
	else
	{
		if ( Q_strcmp( DM_PlayerState[r_playerindex].name, cl.players[r_playerindex].model ) )
		{
			Q_strncpy( DM_PlayerState[r_playerindex].name, cl.players[r_playerindex].model, sizeof( DM_PlayerState[r_playerindex].name ) - 1 );
			DM_PlayerState[r_playerindex].name[sizeof( DM_PlayerState[r_playerindex].name ) - 1] = 0;

			_snprintf( DM_PlayerState[r_playerindex].modelname, sizeof( DM_PlayerState[r_playerindex].modelname ), "models/player/%s/%s.mdl", cl.players[r_playerindex].model, cl.players[r_playerindex].model );

			DM_PlayerState[r_playerindex].model = Mod_ForName( DM_PlayerState[r_playerindex].modelname, FALSE, TRUE );
			if ( !DM_PlayerState[r_playerindex].model )
				DM_PlayerState[r_playerindex].model = currententity->model;

			R_StudioChangePlayerModel();
		}
	}

	r_model = DM_PlayerState[r_playerindex].model;

	if ( r_model == NULL )
		return 0;

	pstudiohdr = (studiohdr_t *)Mod_Extradata( r_model );

	if ( pplayer->gaitsequence )
	{
		vec3_t	orig_angles;

		r_playerinfo = &cl.players[r_playerindex];

		VectorCopy( currententity->angles, orig_angles );

		R_StudioProcessGait( pplayer );

		r_playerinfo->gaitsequence = pplayer->gaitsequence;
		r_playerinfo = NULL;

		R_StudioSetUpTransform( 0 );
		VectorCopy( orig_angles, currententity->angles );
	}
	else
	{
		currententity->curstate.controller[0] = 127;
		currententity->curstate.controller[1] = 127;
		currententity->curstate.controller[2] = 127;
		currententity->curstate.controller[3] = 127;
		currententity->latched.prevcontroller[0] = currententity->curstate.controller[0];
		currententity->latched.prevcontroller[1] = currententity->curstate.controller[1];
		currententity->latched.prevcontroller[2] = currententity->curstate.controller[2];
		currententity->latched.prevcontroller[3] = currententity->curstate.controller[3];

		cl.players[r_playerindex].gaitsequence = 0;

		R_StudioSetUpTransform( 0 );
	}

	if ( flags & STUDIO_RENDER )
	{
		// see if the bounding box lets us trivially reject, also sets
		if ( !R_StudioCheckBBox() )
			return 0;

		r_amodels_drawn++;
		r_smodels_total++; // render data cache cookie

		if ( pstudiohdr->numbodyparts == 0 )
			return 1;
	}

	r_playerinfo = &cl.players[r_playerindex];
	R_StudioSetupBones();
	R_StudioSaveBones();
	r_playerinfo->renderframe = r_framecount;

	r_playerinfo = NULL;

	if ( ( flags & STUDIO_EVENTS ) && ( !( flags & STUDIO_RENDER ) || !pplayer->weaponmodel || !WeaponHasAttachments( pplayer ) ) )
	{
		R_StudioCalcAttachments();
		R_StudioClientEvents();

		// copy attachments into global entity array
		if ( currententity->index > 0 )
		{
			Q_memcpy( cl_entities[currententity->index].attachment, currententity->attachment, sizeof( vec3_t ) * 4 );
		}
	}

	if ( flags & STUDIO_RENDER )
	{
		if ( cl_himodels.value && DM_PlayerState[r_playerindex].model != currententity->model )
		{
			// show highest resolution multiplayer model
			currententity->curstate.body = 255;
		}

		if ( !( developer.value == 0 && Host_IsSinglePlayerGame() ) && DM_PlayerState[r_playerindex].name[0] && DM_PlayerState[r_playerindex].model == currententity->model )
		{
			currententity->curstate.body = 1; // force helmet
		}

		lighting.plightvec = dir;
		R_StudioDynamicLight( currententity, &lighting );

		R_StudioEntityLight( &lighting );

		// model and frame independant
		R_StudioSetupLighting( &lighting );

		r_playerinfo = &cl.players[r_playerindex];

		// get remap colors
		r_topcolor = r_playerinfo->topcolor;
		if ( r_topcolor < 0 )
			r_topcolor = 0;
		if ( r_topcolor > 360 )
			r_topcolor = 360;

		r_bottomcolor = r_playerinfo->bottomcolor;
		if ( r_bottomcolor < 0 )
			r_bottomcolor = 0;
		if ( r_bottomcolor > 360 )
			r_bottomcolor = 360;

		R_StudioRenderModel();
		r_playerinfo = NULL;

		if ( pplayer->weaponmodel )
		{
			studiohdr_t	*saveheader;
			cl_entity_t	saveent;
			model_t		*pweaponmodel;

			saveheader = pstudiohdr;
			saveent = *currententity;

			pweaponmodel = CL_GetModelByIndex( pplayer->weaponmodel );
			pstudiohdr = (studiohdr_t *)Mod_Extradata( pweaponmodel );

			R_StudioMergeBones( pweaponmodel );

			R_StudioSetupLighting( &lighting );

			R_StudioRenderModel();

			R_StudioCalcAttachments();

			if ( currententity->index > 0 )
			{
				Q_memcpy( saveent.attachment, currententity->attachment, pstudiohdr->numattachments * sizeof( vec3_t ) );
			}

			*currententity = saveent;
			pstudiohdr = saveheader;

			if ( flags & STUDIO_EVENTS )
				R_StudioClientEvents();
		}
	}

	return 1;
}

/*
================
R_StudioDynamicLight
================
*/
void R_StudioDynamicLight( cl_entity_t *ent, alight_t *plight )
{
	int			lnum;
	vec3_t		dist;
	colorVec	down;
	vec3_t		light;
	float		total, r;
	float		add;

	vec3_t		uporigin, upend;
	float		floor;
	vec3_t		color;

	if ( filterMode )
	{
		plight->shadelight = 0;
		plight->ambientlight = 192 * filterBrightness;

		plight->plightvec[0] = 0;
		plight->plightvec[1] = 0;
		plight->plightvec[2] = -1;

		plight->color[0] = filterColorRed * filterBrightness;
		plight->color[1] = filterColorGreen * filterBrightness;
		plight->color[2] = filterColorBlue * filterBrightness;
		return;
	}

	if ( r_fullbright.value == 1 )
	{
		plight->shadelight = 0;
		plight->ambientlight = 192;

		plight->plightvec[0] = 0;
		plight->plightvec[1] = 0;
		plight->plightvec[2] = -1;

		plight->color[0] = 1.0;
		plight->color[1] = 1.0;
		plight->color[2] = 1.0;
		return;
	}

	// determine plane to get lightvalues from: ceil or floor
	if ( ent->curstate.effects & EF_INVLIGHT )
	{
		light[0] = 0;
		light[1] = 0;
		light[2] = 1;
	}
	else
	{
		light[0] = 0;
		light[1] = 0;
		light[2] = -1;
	}

	uporigin[0] = ent->origin[0] - light[0] * 8;
	uporigin[1] = ent->origin[1] - light[1] * 8;
	uporigin[2] = ent->origin[2] - light[2] * 8;

	down.r = down.g = down.b = 0;

	if ( movevars.skycolor_r + movevars.skycolor_g + movevars.skycolor_b != 0 )
	{
		vec3_t		end;
		msurface_t	*psurf = NULL;

		end[0] = ent->origin[0] - movevars.skyvec_x * 8192;
		end[1] = ent->origin[1] - movevars.skyvec_y * 8192;
		end[2] = ent->origin[2] - movevars.skyvec_z * 8192;

		psurf = SurfaceAtPoint( cl.worldmodel, cl.worldmodel->nodes, uporigin, end );

		if ( ( ent->model->flags & STUDIO_FORCE_SKYLIGHT ) || ( psurf && ( psurf->flags & SURF_DRAWSKY ) ) )
		{
			down.r = movevars.skycolor_r;
			down.g = movevars.skycolor_g;
			down.b = movevars.skycolor_b;

			light[0] = movevars.skyvec_x;
			light[1] = movevars.skyvec_y;
			light[2] = movevars.skyvec_z;
		}
	}

	if ( down.r + down.g + down.b == 0 )
	{
		colorVec	gcolor;
		float		grad[4];

		VectorScale( light, 2048, upend );
		VectorAdd( uporigin, upend, upend );

		down = R_LightVec( uporigin, upend );

		uporigin[0] -= 16;
		uporigin[1] -= 16;
		upend[0] -= 16;
		upend[1] -= 16;

		gcolor = R_LightVec( uporigin, upend );
		grad[0] = ( gcolor.r + gcolor.g + gcolor.b ) / 768.0;

		uporigin[0] += 32;
		upend[0] += 32;

		gcolor = R_LightVec( uporigin, upend );
		grad[1] = ( gcolor.r + gcolor.g + gcolor.b ) / 768.0;

		uporigin[1] += 32;
		upend[1] += 32;

		gcolor = R_LightVec( uporigin, upend );
		grad[2] = ( gcolor.r + gcolor.g + gcolor.b ) / 768.0;

		uporigin[0] -= 32;
		upend[0] -= 32;

		gcolor = R_LightVec( uporigin, upend );
		grad[3] = ( gcolor.r + gcolor.g + gcolor.b ) / 768.0;

		light[0] = grad[0] - grad[1] - grad[2] + grad[3];
		light[1] = grad[0] + grad[1] - grad[2] - grad[3];
		VectorNormalize( light );
	}

	if ( currententity->curstate.renderfx == kRenderFxLightMultiplier && currententity->curstate.iuser4 != 10 )
	{
		down.r = down.r * ( currententity->curstate.iuser4 / 10.0f );
		down.g = down.g * ( currententity->curstate.iuser4 / 10.0f );
		down.b = down.b * ( currententity->curstate.iuser4 / 10.0f );
	}

	currententity->cvFloorColor = down;

	color[0] = down.r;
	color[1] = down.g;
	color[2] = down.b;

	floor = max( down.r, down.g );
	floor = max( floor, down.b );

	if ( floor == 0 )
		floor = 1.0;

	VectorScale( light, floor, light );
	total = floor;

	for ( lnum = 0; lnum < MAX_DLIGHTS; lnum++ )
	{
		if ( cl_dlights[lnum].die >= cl.time )
		{
			VectorSubtract( ent->origin, cl_dlights[lnum].origin, dist );

			r = Length( dist );
			add = cl_dlights[lnum].radius - r;

			if ( add > 0 )
			{
				if ( r <= 1.0 )
					r = add;
				else
					r = add / r;

				total += add;

				VectorScale( dist, r, dist );
				VectorAdd( light, dist, light );

				color[0] += cl_dlights[lnum].color.r * add / 256.0;
				color[1] += cl_dlights[lnum].color.g * add / 256.0;
				color[2] += cl_dlights[lnum].color.b * add / 256.0;
			}
		}
	}

	if ( ent->model->flags & STUDIO_DYNAMIC_LIGHT )
	{
		r = 0.6;
	}
	else
	{
		r = v_direct.value;
		r = ( r > 1.0 ) ? 1.0 : r;
		r = ( r < 0.75 ) ? 0.75 : r;
	}

	VectorScale( light, r, light );

	plight->shadelight = Length( light );
	plight->ambientlight = total - plight->shadelight;

	floor = max( color[0], color[1] );
	floor = max( floor, color[2] );

	if ( floor == 0 )
	{
		plight->color[0] = 1.0;
		plight->color[1] = 1.0;
		plight->color[2] = 1.0;
	}
	else
	{
		r = 1.0 / floor;
		plight->color[0] = color[0] * r;
		plight->color[1] = color[1] * r;
		plight->color[2] = color[2] * r;
	}

	if ( plight->ambientlight > 128 )
		plight->ambientlight = 128;

	if ( plight->ambientlight + plight->shadelight > 255 )
		plight->shadelight = 255 - plight->ambientlight;

	VectorNormalize( light );
	VectorCopy( light, plight->plightvec );
}

/*
================
R_StudioEntityLight
================
*/
void R_StudioEntityLight( alight_t *plight )
{
	int			i, j, k;
	dlight_t	*el;
	vec3_t		mid, pos;
	float		dist2, f;
	float		lstrength[3];
	float		minstrength;

	VectorCopy( currententity->origin, mid );

	for ( j = 0; j < 3; j++ )
		lstrength[j] = 0;

	minstrength = 1000000;
	numlights = 0;

	el = cl_elights;
	for ( i = 0; i < MAX_ELIGHTS; i++, el++ )
	{
		if ( el->die > cl.time && el->radius > 0 )
		{
			if ( ( el->key & 0xFFF ) == currententity->index )
			{
				int		att;

				att = ( el->key >> 12 ) & 0xF;
				if ( !att )
				{
					VectorCopy( currententity->origin, el->origin );
				}
				else
				{
					VectorCopy( currententity->attachment[att - 1], el->origin );
				}
			}

			VectorSubtract( mid, el->origin, pos );
			dist2 = DotProduct( pos, pos );

			if ( dist2 > el->radius * el->radius )
				f = el->radius * el->radius / dist2;
			else
				f = 1.0;

			if ( f > 0.004 )
			{
				if ( numlights < 3 )
				{
					k = numlights;
				}
				else
				{
					k = -1;
					for ( j = 0; j < numlights; j++ )
					{
						if ( lstrength[j] < minstrength && lstrength[j] < f )
						{
							k = j;
							minstrength = lstrength[j];
						}
					}
				}

				if ( k != -1 )
				{
					lstrength[k] = f;
					locallight[k] = el;
					locallightR2[k] = el->radius * el->radius;
					locallinearlight[k][0] = lineargammatable[el->color.r * 4];
					locallinearlight[k][1] = lineargammatable[el->color.g * 4];
					locallinearlight[k][2] = lineargammatable[el->color.b * 4];

					if ( k >= numlights )
						numlights = k + 1;
				}
			}
		}
	}
}

/*
================
CL_LookupSound
================
*/
struct sfx_s *CL_LookupSound( const char *pName )
{
	int		i;
	sfx_t	*pSound;

	for ( i = 0; i < MAX_SOUNDS; i++ )
	{
		pSound = cl.sound_precache[i];
		if ( pSound && !Q_strcmp( (char *)pName, pSound->name ) )
			return pSound;
	}

	return S_PrecacheSound( (char *)pName );
}

/*
================
R_StudioCalcAttachments
================
*/
void R_StudioCalcAttachments( void )
{
	int					i;
	mstudioattachment_t	*pattachment;

	if ( pstudiohdr->numattachments > 4 )
	{
		Sys_Error( "Too many attachments on %s\n", currententity->model->name );
	}

	// calculate attachment points
	pattachment = (mstudioattachment_t *)( (byte *)pstudiohdr + pstudiohdr->attachmentindex );
	for ( i = 0; i < pstudiohdr->numattachments; i++ )
	{
		VectorTransform( pattachment[i].org, lighttransform[pattachment[i].bone], currententity->attachment[i] );
	}
}

/*
================
R_StudioClientEvents
================
*/
void R_StudioClientEvents( void )
{
	int					i;
	mstudioevent_t		*pevent;
	mstudioseqdesc_t	*pseqdesc;
	float				flStart, flEnd;

	pseqdesc = (mstudioseqdesc_t *)( (byte *)pstudiohdr + pstudiohdr->seqindex ) + currententity->curstate.sequence;

	// no time passed, so no events
	if ( cl.time == cl.oldtime )
		return;

	if ( currententity->curstate.effects & EF_MUZZLEFLASH )
	{
		if ( g_bIsTerrorStrike )
		{
			dlight_t	*el;

			el = CL_AllocDlight( 0 );
			VectorCopy( currententity->attachment[0], el->origin );
			el->radius = 300;
			el->die = cl.time + 0.05;
			el->color.r = 255;
			el->color.g = 192;
			el->color.b = 128;
		}
		else
		{
			dlight_t	*el;

			el = CL_AllocElight( 0 );
			VectorCopy( currententity->attachment[0], el->origin );
			el->radius = 16;
			el->decay = el->radius / 0.05;
			el->die = cl.time + 0.05;
			el->color.r = 255;
			el->color.g = 192;
			el->color.b = 64;
		}

		currententity->curstate.effects &= ~EF_MUZZLEFLASH;
	}

	if ( pseqdesc->numevents == 0 )
		return;

	pevent = (mstudioevent_t *)( (byte *)pstudiohdr + pseqdesc->eventindex );

	flEnd = CL_StudioEstimateFrame( pseqdesc );
	flStart = flEnd - currententity->curstate.framerate * host_frametime * pseqdesc->fps;

	if ( currententity->latched.sequencetime == currententity->curstate.animtime && !( pseqdesc->flags & STUDIO_LOOPING ) )
		flStart = -0.01;

	for ( i = 0; i < pseqdesc->numevents; i++ )
	{
		// client side events only
		if ( pevent[i].event < 5000 )
			continue;

		if ( pevent[i].frame > flStart && pevent[i].frame <= flEnd )
			ClientDLL_StudioEvent( &pevent[i], currententity );
	}
}

/*
================
R_StudioRenderModel
================
*/
void R_StudioRenderModel( void )
{
	VectorCopy( r_origin, g_ChromeOrigin );

	g_ForcedFaceFlags = 0;

	if ( currententity->curstate.renderfx == kRenderFxGlowShell )
	{
		currententity->curstate.renderfx = kRenderFxNone;
		R_StudioRenderFinal();

		g_ForcedFaceFlags = STUDIO_NF_CHROME;

		tri.SpriteTexture( cl_sprite_shell, 0 );
		currententity->curstate.renderfx = kRenderFxGlowShell;

		R_StudioRenderFinal();
	}
	else
	{
		R_StudioRenderFinal();
	}
}

/*
================
R_StudioRenderFinal
================
*/
void R_StudioRenderFinal( void )
{
	int		i;
	float	an;
	int		rendermode;

	pauxverts = auxverts;
	pvlightvalues = lightvalues;

	an = 0;
	shadevector[0] = cos( -an );
	shadevector[1] = sin( -an );
	shadevector[2] = 1;
	VectorNormalize( shadevector );

	GL_DisableMultitexture();
	qglPushMatrix();
	qglShadeModel( GL_SMOOTH );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	if ( gl_affinemodels.value )
		qglHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST );

	if ( g_ForcedFaceFlags )
		rendermode = kRenderTransAdd;
	else
		rendermode = currententity->curstate.rendermode;

	if ( r_drawentities.value == 2 )
	{
		R_StudioDrawBones();
	}
	else if ( r_drawentities.value == 3 )
	{
		R_StudioDrawHulls();
	}
	else
	{
		for ( i = 0; i < pstudiohdr->numbodyparts; i++ )
		{
			R_StudioSetupModel( i );

			if ( r_dointerp )
			{
				// interpolation messes up bounding boxes.
				currententity->trivial_accept = 0;
			}

			if ( rendermode != kRenderNormal )
			{
				if ( rendermode == kRenderTransColor )
				{
					qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
					qglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA );
				}
				else if ( rendermode == kRenderTransAdd )
				{
					qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
					qglBlendFunc( GL_ONE, GL_ONE );
					qglColor4f( r_blend, r_blend, r_blend, 1.0 );
					qglDepthMask( GL_FALSE );
				}
				else
				{
					qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
					qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
					qglColor4f( 1.0, 1.0, 1.0, r_blend );
					qglDepthMask( GL_TRUE );
				}

				qglEnable( GL_BLEND );
			}

			R_StudioDrawPoints();

			qglDepthMask( GL_TRUE );

			if ( r_shadows.value && rendermode != kRenderTransAdd && !( r_model->flags & STUDIO_DYNAMIC_LIGHT ) )
			{
				float	color;

				color = 1.0 - ( r_blend * 0.5 );

				qglDisable( GL_TEXTURE_2D );
				qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
				qglEnable( GL_BLEND );
				qglColor4f( 0, 0, 0, 1 - color );

				if ( gl_ztrick.value == 0 || gldepthmin < 0.5 )
					qglDepthFunc( GL_LESS );
				else
					qglDepthFunc( GL_GREATER );

				GLR_StudioDrawShadow();

				if ( gl_ztrick.value == 0 || gldepthmin < 0.5 )
					qglDepthFunc( GL_LEQUAL );
				else
					qglDepthFunc( GL_GEQUAL );

				qglEnable( GL_TEXTURE_2D );
				qglDisable( GL_BLEND );
				qglColor4f( 1, 1, 1, 1 );
				qglShadeModel( GL_SMOOTH );
			}
		}
	}

	if ( r_drawentities.value == 4 )
	{
		tri.RenderMode( kRenderTransAdd );
		R_StudioDrawHulls();
		tri.RenderMode( kRenderNormal );
	}

	if ( rendermode != kRenderNormal )
		qglDisable( GL_BLEND );

	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	qglShadeModel( GL_FLAT );

	if ( gl_affinemodels.value )
		qglHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

	qglPopMatrix();
}


/*
================
R_GLStudioDrawPoints
================
*/
void R_GLStudioDrawPoints( void )
{
	int					i, j;
	byte				*pvertbone;
	byte				*pnormbone;
	vec3_t				*pstudioverts;
	vec3_t				*pstudionorms;
	studiohdr_t			*ptexturehdr;
	mstudiotexture_t	*ptexture;
	mstudiomesh_t		*pmesh;
	auxvert_t			*av;
	float				*lv;
	vec3_t				fl;
	float				lv_tmp;
	short				*pskinref;
	int					flags;
	int					iFlippedVModel = 0;

	pvertbone = ( (byte *)pstudiohdr + psubmodel->vertinfoindex );
	pnormbone = ( (byte *)pstudiohdr + psubmodel->norminfoindex );

	ptexturehdr = R_LoadTextures( r_model );
	ptexture = (mstudiotexture_t *)( (byte *)ptexturehdr + ptexturehdr->textureindex );

	pmesh = (mstudiomesh_t *)( (byte *)pstudiohdr + psubmodel->meshindex );

	pstudioverts = (vec3_t *)( (byte *)pstudiohdr + psubmodel->vertindex );
	pstudionorms = (vec3_t *)( (byte *)pstudiohdr + psubmodel->normindex );

	pskinref = (short *)( (byte *)ptexturehdr + ptexturehdr->skinindex );
	if ( currententity->curstate.skin != 0 && currententity->curstate.skin < ptexturehdr->numskinfamilies )
		pskinref += ( currententity->curstate.skin * ptexturehdr->numskinref );

	if ( currententity->curstate.renderfx == kRenderFxGlowShell )
	{
		BuildNormalIndexTable();
		BuildGlowShellVerts( pstudioverts, pauxverts );
	}
	else
	{
		for ( av = pauxverts, i = 0; i < psubmodel->numverts; i++, av++, pstudioverts++ )
		{
			R_StudioTransformAuxVert( av, pvertbone[i], pstudioverts[0] );
		}
	}

	pvertbone = ( (byte *)pstudiohdr + psubmodel->vertinfoindex );
	pstudioverts = (vec3_t *)( (byte *)pstudiohdr + psubmodel->vertindex );

	for ( i = 0; i < psubmodel->numverts; i++, pvertbone++, pstudioverts++ )
	{
		R_LightStrength( *pvertbone, pstudioverts[0], lightpos[i] );
	}

	lv = (float *)pvlightvalues;
	for ( j = 0; j < psubmodel->nummesh; j++ )
	{
		flags = ptexture[pskinref[pmesh[j].skinref]].flags | g_ForcedFaceFlags;

		if ( r_fullbright.value >= 2 )
			flags &= ~( STUDIO_NF_FLATSHADE | STUDIO_NF_CHROME );

		if ( currententity->curstate.rendermode == kRenderTransAdd )
		{
			for ( i = 0; i < pmesh[j].numnorms; i++, lv += 3 )
			{
				lv[0] = lv[1] = lv[2] = r_blend;

				if ( flags & STUDIO_NF_CHROME )
					R_StudioChrome( chrome[(vec3_t *)lv - pvlightvalues], *pnormbone, (float *)pstudionorms );
			}
		}
		else
		{
			for ( i = 0; i < pmesh[j].numnorms; i++, lv += 3, pstudionorms++, pnormbone++ )
			{
				int		idx;

				idx = (vec3_t *)lv - pvlightvalues;

				R_StudioLighting( &lv_tmp, *pnormbone, flags, (float *)pstudionorms );

				// FIX: move this check out of the inner loop
				if ( flags & STUDIO_NF_CHROME )
					R_StudioChrome( chrome[(vec3_t *)lv - pvlightvalues], *pnormbone, (float *)pstudionorms );

				lv[0] = lv_tmp * r_colormix[0];
				lv[1] = lv_tmp * r_colormix[1];
				lv[2] = lv_tmp * r_colormix[2];
			}
		}
	}

	qglCullFace( GL_FRONT );

	if ( IsFlippedViewModel() == TRUE )
	{
		qglDisable( GL_CULL_FACE );
		iFlippedVModel = 1;
	}

	pstudionorms = (vec3_t *)( (byte *)pstudiohdr + psubmodel->normindex );

	for ( j = 0; j < psubmodel->nummesh; j++ )
	{
		float	s, t;
		short	*ptricmds;

		pmesh = (mstudiomesh_t *)( (byte *)pstudiohdr + psubmodel->meshindex ) + j;
		ptricmds = (short *)( (byte *)pstudiohdr + pmesh->triindex );

		c_alias_polys += pmesh->numtris;

		flags = ptexture[pskinref[pmesh->skinref]].flags | g_ForcedFaceFlags;

		if ( r_fullbright.value >= 2 )
			flags &= ~( STUDIO_NF_FLATSHADE | STUDIO_NF_CHROME );

		if ( flags & STUDIO_NF_MASKED )
		{
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GREATER, 0.5 );
			qglDepthMask( GL_TRUE );
		}

		if ( ( flags & STUDIO_NF_ADDITIVE ) && currententity->curstate.rendermode == kRenderNormal )
		{
			qglBlendFunc( GL_ONE, GL_ONE );
			qglEnable( GL_BLEND );
			qglDepthMask( GL_FALSE );
			qglShadeModel( GL_SMOOTH );
		}

		if ( r_fullbright.value < 2 )
		{
			s = 1.0 / ptexture[pskinref[pmesh->skinref]].width;
			t = 1.0 / ptexture[pskinref[pmesh->skinref]].height;

			R_StudioSetupSkin( ptexturehdr, pskinref[pmesh->skinref] );
		}
		else
		{
			tri.SpriteTexture( cl_sprite_white, 0 );
			s = 1.0 / 256;
			t = 1.0 / 256;
		}

		if ( flags & STUDIO_NF_CHROME )
		{
			s *= 1.0 / 1024;
			t *= 1.0 / 1024;

			if ( g_ForcedFaceFlags & STUDIO_NF_CHROME )
			{
				while ( i = *( ptricmds++ ) )
				{
					if ( i < 0 )
					{
						qglBegin( GL_TRIANGLE_FAN );
						i = -i;
					}
					else
					{
						qglBegin( GL_TRIANGLE_STRIP );
					}

					for ( ; i > 0; i--, ptricmds += 4 )
					{
						int		normalIndex;

						normalIndex = g_NormalIndex[ptricmds[0]];
						qglTexCoord2f( chrome[normalIndex][0] * s, chrome[normalIndex][1] * t );

						av = &pauxverts[ptricmds[0]];
						qglVertex3f( av->fv[0], av->fv[1], av->fv[2] );
					}

					qglEnd();
				}
			}
			else
			{
				while ( i = *( ptricmds++ ) )
				{
					if ( i < 0 )
					{
						qglBegin( GL_TRIANGLE_FAN );
						i = -i;
					}
					else
					{
						qglBegin( GL_TRIANGLE_STRIP );
					}

					for ( ; i > 0; i--, ptricmds += 4 )
					{
						qglTexCoord2f( chrome[ptricmds[1]][0] * s, chrome[ptricmds[1]][1] * t );

						lv = pvlightvalues[ptricmds[1]];
						R_LightLambert( lightpos[ptricmds[0]], pstudionorms[ptricmds[1]], lv, fl );
						qglColor4f( fl[0], fl[1], fl[2], r_blend );

						av = &pauxverts[ptricmds[0]];
						qglVertex3f( av->fv[0], av->fv[1], av->fv[2] );
					}

					qglEnd();
				}
			}
		}
		else
		{
			while ( i = *( ptricmds++ ) )
			{
				if ( i < 0 )
				{
					qglBegin( GL_TRIANGLE_FAN );
					i = -i;
				}
				else
				{
					qglBegin( GL_TRIANGLE_STRIP );
				}

				for ( ; i > 0; i--, ptricmds += 4 )
				{
					qglTexCoord2f( ptricmds[2] * s, ptricmds[3] * t );

					lv = pvlightvalues[ptricmds[1]];

					if ( iFlippedVModel == 1 )
					{
						vec3_t	vNormal;

						vNormal[0] = (double)pstudionorms[ptricmds[1]][0];
						vNormal[1] = -pstudionorms[ptricmds[1]][1];
						vNormal[2] = (double)pstudionorms[ptricmds[1]][2];

						R_LightLambert( lightpos[ptricmds[0]], vNormal, lv, fl );
					}
					else
					{
						R_LightLambert( lightpos[ptricmds[0]], pstudionorms[ptricmds[1]], lv, fl );
					}

					qglColor4f( fl[0], fl[1], fl[2], r_blend );

					av = &pauxverts[ptricmds[0]];
					qglVertex3f( av->fv[0], av->fv[1], av->fv[2] );
				}

				qglEnd();
			}
		}

		if ( flags & STUDIO_NF_MASKED )
		{
			qglAlphaFunc( GL_NOTEQUAL, 0 );
			qglDisable( GL_ALPHA_TEST );
		}

		if ( ( flags & STUDIO_NF_ADDITIVE ) && currententity->curstate.rendermode == kRenderNormal )
		{
			qglDisable( GL_BLEND );
			qglDepthMask( GL_TRUE );
			qglShadeModel( GL_FLAT );
		}
	}

	qglEnable( GL_CULL_FACE );
}

/*
================
IsATISmoothing
================
*/
qboolean IsATISmoothing( void )
{
	if ( atismoothing && ati_npatch.value )
		return TRUE;

	return FALSE;
}

/*
================
GenerateNormals
================
*/
void GenerateNormals( auxvert_t *pts )
{
	int				ii, jj;
	mstudiomesh_t	*pmesh;
	int				v0, v1, v2;
	vec3_t			e0, e1, norm;

	for ( ii = 0; ii < psubmodel->numverts; ii++ )
	{
		g_txformednormal[ii][0] = 0;
		g_txformednormal[ii][1] = 0;
		g_txformednormal[ii][2] = 0;
	}

	for ( jj = 0; jj < psubmodel->nummesh; jj++ )
	{
		short	*ptricmds;

		pmesh = (mstudiomesh_t *)( (byte *)pstudiohdr + psubmodel->meshindex ) + jj;
		ptricmds = (short *)( (byte *)pstudiohdr + pmesh->triindex );

		while ( ii = *( ptricmds++ ) )
		{
			if ( ii < 0 )
			{
				ii = -ii;

				v0 = ptricmds[0];
				ii--;
				ptricmds += 4;

				v2 = ptricmds[0];
				ii--;
				ptricmds += 4;

				for ( ; ii > 0; ii--, ptricmds += 4 )
				{
					v1 = v2;
					v2 = ptricmds[0];

					VectorSubtract( pts[v1].fv, pts[v0].fv, e0 );
					VectorSubtract( pts[v2].fv, pts[v0].fv, e1 );
					CrossProduct( e1, e0, norm );

					VectorAdd( norm, g_txformednormal[v0], g_txformednormal[v0] );
					VectorAdd( norm, g_txformednormal[v1], g_txformednormal[v1] );
					VectorAdd( norm, g_txformednormal[v2], g_txformednormal[v2] );
				}
			}
			else
			{
				qboolean	odd = FALSE;

				v0 = ptricmds[0];
				ii--;
				ptricmds += 4;

				v1 = ptricmds[0];
				ii--;
				ptricmds += 4;

				for ( ; ii > 0; ii--, ptricmds += 4 )
				{
					v2 = ptricmds[0];

					VectorSubtract( pts[v1].fv, pts[v0].fv, e0 );
					VectorSubtract( pts[v2].fv, pts[v0].fv, e1 );
					CrossProduct( e1, e0, norm );

					VectorAdd( norm, g_txformednormal[v0], g_txformednormal[v0] );
					VectorAdd( norm, g_txformednormal[v1], g_txformednormal[v1] );
					VectorAdd( norm, g_txformednormal[v2], g_txformednormal[v2] );

					if ( !odd )
					{
						v0 = v2;
						odd = !odd;
					}
					else
					{
						v1 = v2;
						odd = !odd;
					}
				}
			}
		}
	}

	for ( ii = 0; ii < psubmodel->numverts; ii++ )
	{
		VectorNormalize( g_txformednormal[ii] );
	}
}
/*
================
R_GLStudioDrawPointsATI_guess
================
*/
void R_GLStudioDrawPointsATI_guess( void )
{
	int					i, j;
	byte				*pvertbone;
	byte				*pnormbone;
	vec3_t				*pstudioverts;
	vec3_t				*pstudionorms;
	studiohdr_t			*ptexturehdr;
	mstudiotexture_t	*ptexture;
	mstudiomesh_t		*pmesh;
	auxvert_t			*av;
	float				*lv;
	vec3_t				fl;
	float				lv_tmp;
	short				*pskinref;
	int					flags;
	int					iFlippedVModel = 0;

	qglEnable( GL_PN_TRIANGLES_ATI );
	qglPNTrianglesiATI( GL_PN_TRIANGLES_POINT_MODE_ATI, GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI );
	qglPNTrianglesiATI( GL_PN_TRIANGLES_NORMAL_MODE_ATI, GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATI );
	qglPNTrianglesiATI( GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI, (int)ati_subdiv.value );

	pvertbone = ( (byte *)pstudiohdr + psubmodel->vertinfoindex );
	pnormbone = ( (byte *)pstudiohdr + psubmodel->norminfoindex );

	ptexturehdr = R_LoadTextures( r_model );
	ptexture = (mstudiotexture_t *)( (byte *)ptexturehdr + ptexturehdr->textureindex );

	pmesh = (mstudiomesh_t *)( (byte *)pstudiohdr + psubmodel->meshindex );

	pstudioverts = (vec3_t *)( (byte *)pstudiohdr + psubmodel->vertindex );
	pstudionorms = (vec3_t *)( (byte *)pstudiohdr + psubmodel->normindex );

	pskinref = (short *)( (byte *)ptexturehdr + ptexturehdr->skinindex );
	if ( currententity->curstate.skin != 0 && currententity->curstate.skin < ptexturehdr->numskinfamilies )
		pskinref += ( currententity->curstate.skin * ptexturehdr->numskinref );

	if ( currententity->curstate.renderfx == kRenderFxGlowShell )
	{
		BuildNormalIndexTable();
		BuildGlowShellVerts( pstudioverts, pauxverts );
	}
	else
	{
		for ( av = pauxverts, i = 0; i < psubmodel->numverts; i++, av++, pstudioverts++ )
		{
			R_StudioTransformAuxVert( av, pvertbone[i], pstudioverts[0] );
		}
	}

	GenerateNormals( pauxverts );

	pvertbone = ( (byte *)pstudiohdr + psubmodel->vertinfoindex );
	pstudioverts = (vec3_t *)( (byte *)pstudiohdr + psubmodel->vertindex );

	for ( i = 0; i < psubmodel->numverts; i++, pvertbone++, pstudioverts++ )
	{
		R_LightStrength( *pvertbone, pstudioverts[0], lightpos[i] );
	}

	lv = (float *)pvlightvalues;
	for ( j = 0; j < psubmodel->nummesh; j++ )
	{
		flags = ptexture[pskinref[pmesh[j].skinref]].flags | g_ForcedFaceFlags;

		if ( r_fullbright.value >= 2 )
			flags &= ~( STUDIO_NF_FLATSHADE | STUDIO_NF_CHROME );

		if ( flags & STUDIO_NF_MASKED )
		{
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GREATER, 0.5 );
			qglDepthMask( GL_TRUE );
		}

		if ( ( flags & STUDIO_NF_ADDITIVE ) && currententity->curstate.rendermode == kRenderNormal )
		{
			qglBlendFunc( GL_ONE, GL_ONE );
			qglEnable( GL_BLEND );
			qglDepthMask( GL_FALSE );
			qglShadeModel( GL_SMOOTH );
		}

		if ( currententity->curstate.rendermode == kRenderTransAdd )
		{
			for ( i = 0; i < pmesh[j].numnorms; i++, lv += 3 )
			{
				lv[0] = lv[1] = lv[2] = r_blend;

				if ( flags & STUDIO_NF_CHROME )
					R_StudioChrome( chrome[(vec3_t *)lv - pvlightvalues], *pnormbone, (float *)pstudionorms );
			}
		}
		else
		{
			for ( i = 0; i < pmesh[j].numnorms; i++, lv += 3, pstudionorms++, pnormbone++ )
			{
				int		idx;

				idx = (vec3_t *)lv - pvlightvalues;

				R_StudioLighting( &lv_tmp, *pnormbone, flags, (float *)pstudionorms );

				// FIX: move this check out of the inner loop
				if ( flags & STUDIO_NF_CHROME )
					R_StudioChrome( chrome[(vec3_t *)lv - pvlightvalues], *pnormbone, (float *)pstudionorms );

				lv[0] = lv_tmp * r_colormix[0];
				lv[1] = lv_tmp * r_colormix[1];
				lv[2] = lv_tmp * r_colormix[2];
			}
		}
	}

	qglCullFace( GL_FRONT );

	if ( IsFlippedViewModel() == TRUE )
	{
		qglDisable( GL_CULL_FACE );
		iFlippedVModel = 1;
	}

	pstudionorms = (vec3_t *)( (byte *)pstudiohdr + psubmodel->normindex );

	for ( j = 0; j < psubmodel->nummesh; j++ )
	{
		float	s, t;
		short	*ptricmds;

		pmesh = (mstudiomesh_t *)( (byte *)pstudiohdr + psubmodel->meshindex ) + j;
		ptricmds = (short *)( (byte *)pstudiohdr + pmesh->triindex );

		c_alias_polys += pmesh->numtris;

		flags = ptexture[pskinref[pmesh->skinref]].flags | g_ForcedFaceFlags;

		if ( r_fullbright.value >= 2 )
			flags &= ~( STUDIO_NF_FLATSHADE | STUDIO_NF_CHROME );

		if ( flags & STUDIO_NF_MASKED )
		{
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GREATER, 0.5 );
			qglDepthMask( GL_TRUE );
		}

		if ( ( flags & STUDIO_NF_ADDITIVE ) && currententity->curstate.rendermode == kRenderNormal )
		{
			qglBlendFunc( GL_ONE, GL_ONE );
			qglEnable( GL_BLEND );
			qglDepthMask( GL_FALSE );
			qglShadeModel( GL_SMOOTH );
		}

		if ( r_fullbright.value < 2 )
		{
			s = 1.0 / ptexture[pskinref[pmesh->skinref]].width;
			t = 1.0 / ptexture[pskinref[pmesh->skinref]].height;

			R_StudioSetupSkin( ptexturehdr, pskinref[pmesh->skinref] );
		}
		else
		{
			tri.SpriteTexture( cl_sprite_white, 0 );
			s = 1.0 / 256;
			t = 1.0 / 256;
		}

		if ( flags & STUDIO_NF_CHROME )
		{
			s *= 1.0 / 1024;
			t *= 1.0 / 1024;

			if ( g_ForcedFaceFlags & STUDIO_NF_CHROME )
			{
				while ( i = *( ptricmds++ ) )
				{
					if ( i < 0 )
					{
						qglBegin( GL_TRIANGLE_FAN );
						i = -i;
					}
					else
					{
						qglBegin( GL_TRIANGLE_STRIP );
					}

					for ( ; i > 0; i--, ptricmds += 4 )
					{
						int		normalIndex;

						normalIndex = g_NormalIndex[ptricmds[0]];
						qglTexCoord2f( chrome[normalIndex][0] * s, chrome[normalIndex][1] * t );

						qglNormal3fv( g_txformednormal[ptricmds[0]] );

						av = &pauxverts[ptricmds[0]];
						qglVertex3f( av->fv[0], av->fv[1], av->fv[2] );
					}

					qglEnd();
				}
			}
			else
			{
				while ( i = *( ptricmds++ ) )
				{
					if ( i < 0 )
					{
						qglBegin( GL_TRIANGLE_FAN );
						i = -i;
					}
					else
					{
						qglBegin( GL_TRIANGLE_STRIP );
					}

					for ( ; i > 0; i--, ptricmds += 4 )
					{
						qglTexCoord2f( chrome[ptricmds[1]][0] * s, chrome[ptricmds[1]][1] * t );

						lv = pvlightvalues[ptricmds[1]];
						R_LightLambert( lightpos[ptricmds[0]], pstudionorms[ptricmds[1]], lv, fl );
						qglColor4f( fl[0], fl[1], fl[2], r_blend );

						qglNormal3fv( g_txformednormal[ptricmds[0]] );

						av = &pauxverts[ptricmds[0]];
						qglVertex3f( av->fv[0], av->fv[1], av->fv[2] );
					}

					qglEnd();
				}
			}
		}
		else
		{
			while ( i = *( ptricmds++ ) )
			{
				if ( i < 0 )
				{
					qglBegin( GL_TRIANGLE_FAN );
					i = -i;
				}
				else
				{
					qglBegin( GL_TRIANGLE_STRIP );
				}

				for ( ; i > 0; i--, ptricmds += 4 )
				{
					qglTexCoord2f( ptricmds[2] * s, ptricmds[3] * t );

					lv = pvlightvalues[ptricmds[1]];

					if ( iFlippedVModel == 1 )
					{
						vec3_t	vNormal;

						vNormal[0] = (double)pstudionorms[ptricmds[1]][0];
						vNormal[1] = -pstudionorms[ptricmds[1]][1];
						vNormal[2] = (double)pstudionorms[ptricmds[1]][2];

						R_LightLambert( lightpos[ptricmds[0]], vNormal, lv, fl );
					}
					else
					{
						R_LightLambert( lightpos[ptricmds[0]], pstudionorms[ptricmds[1]], lv, fl );
					}

					qglColor4f( fl[0], fl[1], fl[2], r_blend );

					qglNormal3fv( g_txformednormal[ptricmds[0]] );

					av = &pauxverts[ptricmds[0]];
					qglVertex3f( av->fv[0], av->fv[1], av->fv[2] );
				}

				qglEnd();
			}
		}

		if ( flags & STUDIO_NF_MASKED )
		{
			qglAlphaFunc( GL_NOTEQUAL, 0 );
			qglDisable( GL_ALPHA_TEST );
		}

		if ( ( flags & STUDIO_NF_ADDITIVE ) && currententity->curstate.rendermode == kRenderNormal )
		{
			qglDisable( GL_BLEND );
			qglDepthMask( GL_TRUE );
			qglShadeModel( GL_FLAT );
		}
	}

	qglDisable( GL_PN_TRIANGLES_ATI );
	qglEnable( GL_CULL_FACE );
}


/*
================
R_StudioDrawPoints
================
*/
void R_StudioDrawPoints( void )
{
	if ( IsATISmoothing() && currententity && !currententity->player )
		R_GLStudioDrawPointsATI_guess();
	else
		R_GLStudioDrawPoints();
}

/*
================
GLR_StudioDrawShadow
================
*/
void GLR_StudioDrawShadow( void )
{
	int				i, k;
	vec3_t			point;
	float			height;
	auxvert_t		*av;
	mstudiomesh_t	*pmesh;

	for ( k = 0; k < psubmodel->nummesh; k++ )
	{
		short	*ptricmds;

		pmesh = (mstudiomesh_t *)( (byte *)pstudiohdr + psubmodel->meshindex ) + k;
		ptricmds = (short *)( (byte *)pstudiohdr + pmesh->triindex );

		c_alias_polys += pmesh->numtris;

		height = lightspot[2] + 1.0;

		while ( i = *( ptricmds++ ) )
		{
			if ( i < 0 )
			{
				qglBegin( GL_TRIANGLE_FAN );
				i = -i;
			}
			else
			{
				qglBegin( GL_TRIANGLE_STRIP );
			}

			for ( ; i > 0; i--, ptricmds += 4 )
			{
				av = &pauxverts[ptricmds[0]];

				VectorCopy( av->fv, point );
				point[0] -= shadevector[0] * ( point[2] - lightspot[2] );
				point[1] -= shadevector[1] * ( point[2] - lightspot[2] );
				point[2] = height;

				qglVertex3fv( point );
			}

			qglEnd();
		}
	}
}

/*
================
R_UpdateLatchedVars
================
*/
void R_UpdateLatchedVars( cl_entity_t *ent )
{
	int		i;

	VectorCopy( ent->prevstate.origin, ent->latched.prevorigin );
	VectorCopy( ent->prevstate.angles, ent->latched.prevangles );

	ent->latched.prevanimtime = ent->prevstate.animtime;

	if ( ent->curstate.sequence != ent->prevstate.sequence )
	{
		ent->latched.sequencetime = ent->curstate.animtime;
		ent->latched.prevsequence = ent->prevstate.sequence;
		ent->latched.prevseqblending[0] = ent->prevstate.blending[0];
		ent->latched.prevseqblending[1] = ent->prevstate.blending[1];
	}

	for ( i = 0; i < 4; i++ )
		ent->latched.prevcontroller[i] = ent->prevstate.controller[i];

	for ( i = 0; i < 2; i++ )
		ent->latched.prevblending[i] = ent->prevstate.blending[i];
}

/*
================
R_ResetLatched
================
*/
void R_ResetLatched( cl_entity_t *ent, qboolean full_reset )
{
	if ( full_reset )
	{
		Q_memcpy( ent->latched.prevcontroller, ent->curstate.controller, sizeof( ent->latched.prevcontroller ) );
		Q_memcpy( ent->latched.prevblending, ent->curstate.blending, sizeof( ent->latched.prevblending ) );
		ent->latched.sequencetime = ent->curstate.animtime;
		ent->latched.prevframe = CL_GetEstimatedFrame( ent );
		ent->prevstate = ent->curstate;
	}

	ent->curstate.animtime = cl.mtime[0];
	ent->latched.prevanimtime = ent->curstate.animtime;
	ent->latched.prevsequence = ent->curstate.sequence;

	VectorCopy( ent->curstate.origin, ent->latched.prevorigin );
	VectorCopy( ent->curstate.angles, ent->latched.prevangles );
}

/*
================
R_StudioBoundVertex
================
*/
void R_StudioBoundVertex( vec_t *mins, vec_t *maxs, int *vertcount, const vec_t *point )
{
	int		i;

	if ( *vertcount == 0 )
	{
		// first time through, just set the mins and maxs
		VectorCopy( point, mins );
		VectorCopy( point, maxs );
	}
	else
	{
		// update the mins and maxs
		for ( i = 0; i < 3; i++ )
		{
			if ( point[i] < mins[i] )
				mins[i] = point[i];

			if ( point[i] > maxs[i] )
				maxs[i] = point[i];
		}
	}

	( *vertcount )++;
}

/*
================
R_StudioBoundBone
================
*/
void R_StudioBoundBone( vec_t *mins, vec_t *maxs, int *bonecount, const vec_t *point )
{
	int		i;

	if ( *bonecount == 0 )
	{
		// first time through, just set the mins and maxs
		VectorCopy( point, mins );
		VectorCopy( point, maxs );
	}
	else
	{
		// update the mins and maxs
		for ( i = 0; i < 3; i++ )
		{
			if ( point[i] < mins[i] )
				mins[i] = point[i];

			if ( point[i] > maxs[i] )
				maxs[i] = point[i];
		}
	}

	( *bonecount )++;
}

/*
================
R_StudioAccumulateBoneVerts
================
*/
void R_StudioAccumulateBoneVerts( vec_t *mins, vec_t *maxs, int *vertcount, vec_t *bone_mins, vec_t *bone_maxs, int *bonecount )
{
	vec3_t	delta;
	vec3_t	point;

	if ( *bonecount <= 0 )
		return;

	{
		// get the bone extents
		VectorSubtract( bone_maxs, bone_mins, delta );

		// put both points into the vert list
		VectorScale( delta, 0.5, point );
		R_StudioBoundVertex( mins, maxs, vertcount, point );
		VectorScale( delta, -0.5, point );
		R_StudioBoundVertex( mins, maxs, vertcount, point );

		// reset the bone accumulators
		VectorClear( bone_mins );
		VectorClear( bone_maxs );
		*bonecount = 0;
	}
}

/*
================
R_StudioComputeBounds
================
*/
int R_StudioComputeBounds( byte *pBuffer, float *mins, float *maxs )
{
	studiohdr_t			*header;
	mstudiobodyparts_t	*bodyparts;
	mstudiomodel_t		*pmodel;

	int					c;
	int					model, nummodels;

	int					num_verts, v;
	vec3_t				*pverts;

	mstudiobone_t		*pbones;
	int					bone;

	mstudioseqdesc_t	*pseq;
	int					sequence;

	mstudioanim_t		*panim;

	int					f, num_frames;

	char				*temp;

	float				bone_position[3];

	vec3_t				bone_mins, bone_maxs;
	vec3_t				vert_mins, vert_maxs;
	int					bone_count = 0;
	int					vert_count = 0;

	VectorClear( bone_mins );
	VectorClear( bone_maxs );
	VectorClear( vert_mins );
	VectorClear( vert_maxs );

	// get the header
	header = (studiohdr_t *)pBuffer;

	// count the models
	temp = (char *)pBuffer + header->bodypartindex;
	bodyparts = (mstudiobodyparts_t *)temp;
	nummodels = 0;
	for ( c = 0; c < header->numbodyparts; c++ )
		nummodels += bodyparts[c].nummodels;

	// accumulate the vertices of every model
	temp += header->numbodyparts * sizeof( mstudiobodyparts_t );
	pmodel = (mstudiomodel_t *)temp;
	for ( model = 0; model < nummodels; model++ )
	{
		num_verts = pmodel[model].numverts;
		pverts = (vec3_t *)( pBuffer + pmodel[model].vertindex );
		for ( v = 0; v < num_verts; v++ )
			R_StudioBoundVertex( vert_mins, vert_maxs, &vert_count, pverts[v] );
	}

	pbones = (mstudiobone_t *)( pBuffer + header->boneindex );

	// accumulate the bone positions of every frame of every sequence
	pseq = (mstudioseqdesc_t *)( pBuffer + header->seqindex );
	for ( sequence = 0; sequence < header->numseq; sequence++ )
	{
		num_frames = pseq[sequence].numframes;
		panim = (mstudioanim_t *)( pBuffer + pseq[sequence].animindex );

		for ( bone = 0; bone < header->numbones; bone++ )
		{
			for ( f = 0; f < num_frames; f++ )
			{
				R_StudioCalcBonePosition( f, 0, &pbones[bone], panim, NULL, bone_position );
				R_StudioBoundBone( bone_mins, bone_maxs, &bone_count, bone_position );
			}
		}

		R_StudioAccumulateBoneVerts( vert_mins, vert_maxs, &vert_count, bone_mins, bone_maxs, &bone_count );
	}

	VectorCopy( vert_mins, mins );
	VectorCopy( vert_maxs, maxs );

	return 1;
}

/*
================
R_GetStudioBounds
================
*/
int R_GetStudioBounds( const char *filename, float *mins, float *maxs )
{
	char			*pBuffer;
	int				iret;
	int				length;
	FileHandle_t	fp;
	qboolean		usingReadBuffer;

	iret = 0;
	usingReadBuffer = FALSE;

	VectorCopy( vec3_origin, mins );
	VectorCopy( vec3_origin, maxs );

	if ( !Q_strstr( (char *)filename, "models" ) )
		return iret;

	if ( !Q_strstr( (char *)filename, ".mdl" ) )
		return iret;

	fp = FS_Open( filename, "rb" );
	if ( !fp )
		return iret;

	pBuffer = (char *)FS_GetReadBuffer( fp, &length );
	if ( pBuffer )
		usingReadBuffer = TRUE;
	else
		pBuffer = (char *)COM_LoadFile( (char *)filename, 5, NULL );

	if ( !pBuffer )
	{
	}
	else if ( LittleLong( *(int *)pBuffer ) != IDSTUDIOHEADER )
	{
		COM_FreeFile( pBuffer );
	}
	else
	{
		iret = R_StudioComputeBounds( (byte *)pBuffer, mins, maxs );
	}

	if ( usingReadBuffer )
		FS_ReleaseReadBuffer( fp, pBuffer );
	else
		COM_FreeFile( pBuffer );

	FS_Close( fp );

	return iret;
}

/*
================
studioapi_GetModelByIndex
================
*/
model_t *studioapi_GetModelByIndex( int index )
{
	return CL_GetModelByIndex( index );
}

/*
================
studioapi_GetCurrentEntity
================
*/
cl_entity_t *studioapi_GetCurrentEntity( void )
{
	return currententity;
}

/*
================
studioapi_PlayerInfo
================
*/
player_info_t *studioapi_PlayerInfo( int index )
{
	if ( index < 0 || index >= cl.maxclients )
		return NULL;

	return &cl.players[index];
}

/*
================
studioapi_GetPlayerState
================
*/
entity_state_t *studioapi_GetPlayerState( int index )
{
	if ( index < 0 || index >= cl.maxclients )
		return NULL;

	return &cl.frames[cl.parsecountmod].playerstate[index];
}

/*
================
studioapi_GetViewEntity
================
*/
cl_entity_t *studioapi_GetViewEntity( void )
{
	return &cl.viewent;
}

/*
================
studioapi_GetTimes
================
*/
void studioapi_GetTimes( int *framecount, double *cl_time, double *cl_oldtime )
{
	*framecount = r_framecount;
	*cl_time = cl.time;
	*cl_oldtime = cl.oldtime;
}

/*
================
studioapi_SetupModel
================
*/
void studioapi_SetupModel( int bodypart, void **ppbodypart, void **ppsubmodel )
{
	R_StudioSetupModel( bodypart );

	*ppbodypart = &pbodypart;
	*ppsubmodel = &psubmodel;
}

/*
================
studioapi_SetBackFaceCull
================
*/
void studioapi_SetBackFaceCull( int iCull )
{
	g_iBackFaceCull = iCull;
}

/*
================
studioapi_SetupRenderer
================
*/
void studioapi_SetupRenderer( int rendermode )
{
	float	an;

	pauxverts = auxverts;
	pvlightvalues = lightvalues;

	an = 0;
	shadevector[0] = cos( -an );
	shadevector[1] = sin( -an );
	shadevector[2] = 1;
	VectorNormalize( shadevector );

	GL_DisableMultitexture();
	qglPushMatrix();
	qglShadeModel( GL_SMOOTH );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	if ( gl_affinemodels.value )
		qglHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST );

	g_rendermode = rendermode;
}

/*
================
studioapi_RestoreRenderer
================
*/
void studioapi_RestoreRenderer( void )
{
	if ( g_rendermode != kRenderNormal )
		qglDisable( GL_BLEND );

	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	qglShadeModel( GL_FLAT );

	if ( gl_affinemodels.value )
		qglHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

	qglPopMatrix();
}

/*
================
studioapi_SetChromeOrigin
================
*/
void studioapi_SetChromeOrigin( void )
{
	VectorCopy( r_origin, g_ChromeOrigin );
}

/*
================
studioapi_SetupPlayerModel
================
*/
model_t *studioapi_SetupPlayerModel( int playerindex )
{
	player_model_t	*state;

	state = &DM_PlayerState[playerindex];

	if ( ( developer.value == 0 && Host_IsSinglePlayerGame() ) || !cl.players[playerindex].model[0] )
	{
		state->name[0] = 0;
		if ( state->model != currententity->model )
		{
			state->model = currententity->model;
			R_StudioChangePlayerModel();
		}
	}
	else
	{
		if ( Q_strcmp( state->name, cl.players[playerindex].model ) )
		{
			Q_strncpy( state->name, cl.players[playerindex].model, sizeof( state->name ) - 1 );
			state->name[sizeof( state->name ) - 1] = 0;

			_snprintf( state->modelname, sizeof( state->modelname ), "models/player/%s/%s.mdl", cl.players[playerindex].model, cl.players[playerindex].model );

			state->model = Mod_ForName( state->modelname, FALSE, TRUE );
			if ( !state->model )
				state->model = currententity->model;

			R_StudioChangePlayerModel();
		}
	}

	return state->model;
}

/*
================
studioapi_StudioGetBoneTransform
================
*/
float ****studioapi_StudioGetBoneTransform( void )
{
	return (float ****)bonetransform;
}

/*
================
studioapi_StudioGetLightTransform
================
*/
float ****studioapi_StudioGetLightTransform( void )
{
	return (float ****)lighttransform;
}

/*
================
studioapi_StudioGetAliasTransform
================
*/
float ***studioapi_StudioGetAliasTransform( void )
{
	return NULL;
}

/*
================
studioapi_StudioGetRotationMatrix
================
*/
float ***studioapi_StudioGetRotationMatrix( void )
{
	return (float ***)rotationmatrix;
}

/*
================
studioapi_GetViewInfo
================
*/
void studioapi_GetViewInfo( float *origin, float *upv, float *rightv, float *vpnv )
{
	VectorCopy( r_origin, origin );
	VectorCopy( vup, upv );
	VectorCopy( vright, rightv );
	VectorCopy( vpn, vpnv );
}

/*
================
studioapi_GetCvar
================
*/
struct cvar_s *studioapi_GetCvar( const char *name )
{
	cvar_t	*var_guess;

	var_guess = Cvar_FindVar( (char *)name );
	return var_guess;
}

/*
================
studioapi_GetChromeSprite
================
*/
model_t *studioapi_GetChromeSprite( void )
{
	return cl_sprite_shell;
}

/*
================
studioapi_GetModelCounters
================
*/
void studioapi_GetModelCounters( int **s, int **a )
{
	*s = &r_smodels_total;
	*a = &r_amodels_drawn;
}

/*
================
studioapi_GetAliasScale
================
*/
void studioapi_GetAliasScale( float *x, float *y )
{
	*x = *y = 1.0;
}

/*
================
studioapi_StudioSetRemapColors
================
*/
void studioapi_StudioSetRemapColors( int top, int bottom )
{
	r_topcolor = top;
	r_bottomcolor = bottom;
}

/*
================
studioapi_StudioSetRenderamt
================
*/
void studioapi_StudioSetRenderamt( int iRenderamt )
{
	currententity->curstate.renderamt = iRenderamt;
	r_blend = CL_FxBlend( currententity );
	r_blend *= ( 1.0 / 255.0 );
}

/*
================
studioapi_GetForceFaceFlags
================
*/
int studioapi_GetForceFaceFlags( void )
{
	return g_ForcedFaceFlags;
}

/*
================
studioapi_SetForceFaceFlags
================
*/
void studioapi_SetForceFaceFlags( int flags )
{
	g_ForcedFaceFlags = flags;
}

/*
================
studioapi_StudioSetHeader
================
*/
void studioapi_StudioSetHeader( void *header )
{
	pstudiohdr = (studiohdr_t *)header;
}

/*
================
studioapi_SetRenderModel
================
*/
void studioapi_SetRenderModel( struct model_s *model )
{
	r_model = model;
}

/*
================
IsHardware
================
*/
int IsHardware( void )
{
	return gD3DMode ? 2 : 1;
}

/*
================
studioapi_GL_SetRenderMode
================
*/
void studioapi_GL_SetRenderMode( int rendermode )
{
	if ( rendermode != kRenderNormal )
	{
		if ( rendermode == kRenderTransColor )
		{
			qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			qglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA );
		}
		else if ( rendermode == kRenderTransAdd )
		{
			qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			qglBlendFunc( GL_ONE, GL_ONE );
			qglColor4f( r_blend, r_blend, r_blend, 1.0 );
			qglDepthMask( GL_FALSE );
		}
		else
		{
			qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			qglColor4f( 1.0, 1.0, 1.0, r_blend );
			qglDepthMask( GL_TRUE );
		}

		qglEnable( GL_BLEND );
	}
}

/*
================
studioapi_GL_StudioDrawShadow
================
*/
void studioapi_GL_StudioDrawShadow( void )
{
	qglDepthMask( GL_TRUE );

	if ( r_shadows.value && g_rendermode != kRenderTransAdd && !( r_model->flags & STUDIO_DYNAMIC_LIGHT ) )
	{
		float	color;

		color = 1.0 - ( r_blend * 0.5 );

		qglDisable( GL_TEXTURE_2D );
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		qglEnable( GL_BLEND );
		qglColor4f( 0, 0, 0, 1 - color );

		if ( gl_ztrick.value == 0 || gldepthmin < 0.5 )
			qglDepthFunc( GL_LESS );
		else
			qglDepthFunc( GL_GREATER );

		GLR_StudioDrawShadow();

		if ( gl_ztrick.value == 0 || gldepthmin < 0.5 )
			qglDepthFunc( GL_LEQUAL );
		else
			qglDepthFunc( GL_GEQUAL );

		qglEnable( GL_TEXTURE_2D );
		qglDisable( GL_BLEND );
		qglColor4f( 1, 1, 1, 1 );
		qglShadeModel( GL_SMOOTH );
	}
}

/*
================
studioapi_RenderShadow
================
*/
void studioapi_RenderShadow( int iSprite, float *p1, float *p2, float *p3, float *p4 )
{
	model_t			*sprite;
	static float	color[4] = { 0, 0, 0, 1.0 };

	if ( !iSprite )
		return;

	if ( iSprite < 0 || iSprite >= cl.model_precache_count )
		return;

	sprite = CL_GetModelByIndex( iSprite );
	if ( !sprite )
		return;

	if ( !tri.SpriteTexture( sprite, 0 ) )
		return;

	tri.RenderMode( kRenderTransAlpha );
	tri.Color4f( color[0], color[1], color[2], color[3] );
	tri.Begin( TRI_QUADS );
	tri.TexCoord2f( 0, 0 );
	tri.Vertex3fv( p1 );
	tri.TexCoord2f( 0, 1 );
	tri.Vertex3fv( p2 );
	tri.TexCoord2f( 1, 1 );
	tri.Vertex3fv( p3 );
	tri.TexCoord2f( 1, 0 );
	tri.Vertex3fv( p4 );
	tri.End();
	tri.RenderMode( kRenderNormal );
}

/*
================
legacy_Mod_ForName
================
*/
struct model_s *legacy_Mod_ForName( const char *model, int crash )
{
	return Mod_ForName( model, crash, FALSE );
}

engine_studio_api_t engine_studio_api =
{
	Mem_Calloc,
	Cache_Check,
	COM_LoadCacheFile,
	legacy_Mod_ForName,
	Mod_Extradata,
	studioapi_GetModelByIndex,
	studioapi_GetCurrentEntity,
	studioapi_PlayerInfo,
	studioapi_GetPlayerState,
	studioapi_GetViewEntity,
	studioapi_GetTimes,
	studioapi_GetCvar,
	studioapi_GetViewInfo,
	studioapi_GetChromeSprite,
	studioapi_GetModelCounters,
	studioapi_GetAliasScale,
	studioapi_StudioGetBoneTransform,
	studioapi_StudioGetLightTransform,
	studioapi_StudioGetAliasTransform,
	studioapi_StudioGetRotationMatrix,
	studioapi_SetupModel,
	R_StudioCheckBBox,
	R_StudioDynamicLight,
	R_StudioEntityLight,
	R_StudioSetupLighting,
	R_StudioDrawPoints,
	R_StudioDrawHulls,
	R_StudioAbsBB,
	R_StudioDrawBones,
	R_StudioSetupSkin,
	studioapi_StudioSetRemapColors,
	studioapi_SetupPlayerModel,
	R_StudioClientEvents,
	studioapi_GetForceFaceFlags,
	studioapi_SetForceFaceFlags,
	studioapi_StudioSetHeader,
	studioapi_SetRenderModel,
	studioapi_SetupRenderer,
	studioapi_RestoreRenderer,
	studioapi_SetChromeOrigin,
	IsHardware,
	studioapi_GL_StudioDrawShadow,
	studioapi_GL_SetRenderMode,
	studioapi_StudioSetRenderamt,
	studioapi_SetBackFaceCull,
	studioapi_RenderShadow,
};

r_studio_interface_t studio =
{
	STUDIO_INTERFACE_VERSION,
	R_StudioDrawModel,
	R_StudioDrawPlayer,
};

r_studio_interface_t	*pStudioAPI = &studio;

/*
================
R_ResetStudio
================
*/
void R_ResetStudio( void )
{
	pStudioAPI = &studio;
}

server_studio_api_t server_studio_api =
{
	Mem_Calloc,
	Cache_Check,
	COM_LoadCacheFile,
	Mod_Extradata,
};

sv_blending_interface_t svBlending =
{
	SV_BLENDING_INTERFACE_VERSION,
	SV_StudioSetupBones,
};

sv_blending_interface_t	*g_pSvBlendingAPI = &svBlending;

/*
================
R_ResetSvBlending
================
*/
void R_ResetSvBlending( void )
{
	g_pSvBlendingAPI = &svBlending;
}
