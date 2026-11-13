// r_part.c -- particles, tracers and beams

#include "quakedef.h"
#include <math.h>
#include "mathlib.h"
#include "cvar.h"
#include "host.h"
#include "client.h"
#include "GL_MODEL.H"
#include "vid.h"
#include "render.h"
#include "GLQUAKE.H"
#include "qgl.h"
#include "r_efx.h"
#include "triangleapi.h"
#include "../pm_shared/pm_movevars.h"

#define MAX_PARTICLES			2048	// default max # of particles at one time
#define ABSOLUTE_MIN_PARTICLES	512		// no fewer than this no matter what's on the command line

#define NUMVERTEXNORMALS		162

#define MAX_BEAMS				128
#define NOISE_DIVISIONS			128

#ifndef ARRAYSIZE
#define ARRAYSIZE(p)			( sizeof( p ) / sizeof( p[0] ) )
#endif

#define BEAMENT_ENTITY(x)		( ( x ) & 0xFFF )
#define BEAMENT_ATTACHMENT(x)	( ( ( x ) >> 12 ) & 0xF )

// beam entity types (rendermode & 0x0F of a beam entity)
#define BEAM_POINTS				0
#define BEAM_ENTPOINT			1
#define BEAM_ENTS				2
#define BEAM_HOSE				3

// gl_rmain.c
extern	int			particletexture;
extern	float		r_avertexnormals[NUMVERTEXNORMALS][3];
extern	float		gWorldToScreen[16];
extern	float		gScreenToWorld[16];
qboolean	R_CullBox( vec3_t mins, vec3_t maxs );
void		GL_Bind( int texnum );

// gl_rmisc.c
extern	int			filterMode;
extern	float		filterColorRed;
extern	float		filterColorGreen;
extern	float		filterColorBlue;
extern	float		filterBrightness;

// mathlib.c
void	VectorMatrix( vec3_t forward, vec3_t right, vec3_t up );

// cl_tent.c
extern	cvar_t		tracerLength;
extern	cvar_t		tracerRed;
extern	cvar_t		tracerGreen;
extern	cvar_t		tracerBlue;
extern	cvar_t		tracerAlpha;
extern	cvar_t		egon_amplitude;
extern	model_t		*cl_sprite_dot;
int		CL_FxBlend( cl_entity_t *ent );

// gl_model.c
int		ModelFrameCount( model_t *model );

// pr_cmds.c
mnode_t	*PVSNode( mnode_t *node, vec_t *emins, vec_t *emaxs );

// cdll_int.c
cl_entity_t	*ClientDLL_GetUserEntity( int index );

// r_triangle.c
extern	triangleapi_t	tri;

extern	movevars_t	movevars;

float	RandomFloat( float flLow, float flHigh );

int		ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
int		ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };
int		ramp3[8] = { 0x6d, 0x6b, 6, 5, 4, 3 };

int		gSparkRamp[9] = { 0xfe, 0xfd, 0xfc, 0x6f, 0x6e, 0x6d, 0x6c, 0x67, 0x60 };

color24	gTracerColors[] =
{
	{ 255, 255, 255 },		// White
	{ 255, 0, 0 },			// Red
	{ 0, 255, 0 },			// Green
	{ 0, 0, 255 },			// Blue
	{ 0, 0, 0 },			// Tracer default, filled in from cvars, etc.
	{ 255, 167, 17 },		// Yellow-orange sparks
	{ 255, 130, 90 },		// Yellowish streaks (garg)
	{ 55, 60, 144 },		// Blue egon streak
	{ 255, 130, 90 },		// More Yellowish streaks (garg)
	{ 255, 140, 90 },		// More Yellowish streaks (garg)
	{ 200, 130, 90 },		// More red streaks (garg)
	{ 255, 120, 70 },		// Darker red streaks (garg)
};

particle_t	*active_particles, *free_particles, *gpActiveTracers;

particle_t	*particles;
int			r_numparticles;

int			cl_numbeamentities;
cl_entity_t	*cl_beamentities[64];

vec3_t		r_pright, r_pup, r_ppn;

static BEAM	*gBeams, *gpActiveBeams, *gpFreeBeams;

vec3_t		avelocities[NUMVERTEXNORMALS];
float		beamlength = 16;
vec3_t		avelocity = { 23, 7, 3 };
float		partstep = 0.01;
float		timescale = 0.01;
static const float gTracerSize[] = { 0.1, 0.1, 1, 1, 1, 1, 1, 1, 1, 1 };

void	R_TracerDraw( void );
void	R_BeamDrawList( void );
void	R_BeamDraw( BEAM *pbeam, float frametime );
void	R_DrawBeamFollow( BEAM *pbeam );
int		R_BeamCull( vec_t *start, vec_t *end, int pvsOnly );
void	R_DrawSegs( vec_t *source, vec_t *delta, float width, float scale, float freq, float speed, int segments, int flags );
void	R_DrawTorus( vec_t *source, vec_t *delta, float width, float scale, float freq, float speed, int segments );
void	R_DrawDisk( vec_t *source, vec_t *delta, float width, float scale, float freq, float speed, int segments );
void	R_DrawCylinder( vec_t *source, vec_t *delta, float width, float scale, float freq, float speed, int segments );
void	R_DrawRing( vec_t *source, vec_t *delta, float width, float amplitude, float freq, float speed, int segments );
void	R_BeamSetup( BEAM *pbeam, vec_t *start, vec_t *end, int modelIndex, float life, float width, float amplitude, float brightness, float speed );
void	SetBeamAttributes( BEAM *pbeam, float r, float g, float b, float framerate, int startFrame );

/*
==============
R_BeamInit
==============
*/
void R_BeamInit( void )
{
	gBeams = (BEAM *)Hunk_AllocName( MAX_BEAMS * sizeof( BEAM ), "lightning" );
}

/*
==============
R_BeamClear
==============
*/
void R_BeamClear( void )
{
	int		i;

	gpFreeBeams = gBeams;
	gpActiveBeams = NULL;

	Q_memset( gBeams, 0, MAX_BEAMS * sizeof( BEAM ) );

	for ( i = 0; i < MAX_BEAMS; i++ )
	{
		gBeams[i].next = &gBeams[i + 1];
	}
	gBeams[ MAX_BEAMS - 1 ].next = NULL;
}

/*
===============
R_InitParticles
===============
*/
void R_InitParticles( void )
{
	int		i;

	i = COM_CheckParm( "-particles" );

	if ( i )
	{
		r_numparticles = Q_atoi( com_argv[i + 1] );
		if ( r_numparticles < ABSOLUTE_MIN_PARTICLES )
			r_numparticles = ABSOLUTE_MIN_PARTICLES;
	}
	else
	{
		r_numparticles = MAX_PARTICLES;
	}

	particles = (particle_t *)Hunk_AllocName( r_numparticles * sizeof( particle_t ), "particles" );

	R_BeamInit();
}

/*
===============
R_EntityParticles
===============
*/
void R_EntityParticles( cl_entity_t *ent )
{
	int			count;
	int			i;
	particle_t	*p;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward;
	float		dist;

	dist = 64;
	count = 50;

	if ( !avelocities[0][0] )
	{
		for ( i = 0; i < NUMVERTEXNORMALS * 3; i++ )
			avelocities[0][i] = RandomFloat( 0, 2.55 );
	}

	for ( i = 0; i < NUMVERTEXNORMALS; i++ )
	{
		angle = avelocities[i][0] * cl.time;
		sy = sin( angle );
		cy = cos( angle );
		angle = avelocities[i][1] * cl.time;
		sp = sin( angle );
		cp = cos( angle );
		angle = avelocities[i][2] * cl.time;
		sr = sin( angle );
		cr = cos( angle );

		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		if ( !free_particles )
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 0.01;
		p->color = 0x6f;
		p->packedColor = 0;
		p->type = pt_explode;

		p->org[0] = ent->origin[0] + r_avertexnormals[i][0] * dist + forward[0] * beamlength;
		p->org[1] = ent->origin[1] + r_avertexnormals[i][1] * dist + forward[1] * beamlength;
		p->org[2] = ent->origin[2] + r_avertexnormals[i][2] * dist + forward[2] * beamlength;
	}
}

/*
===============
R_ClearParticles
===============
*/
void R_ClearParticles( void )
{
	int		i;

	free_particles = &particles[0];
	active_particles = NULL;
	gpActiveTracers = NULL;

	for ( i = 0; i < r_numparticles; i++ )
		particles[i].next = &particles[i + 1];
	particles[ r_numparticles - 1 ].next = NULL;

	R_BeamClear();
}

/*
===============
R_FreeDeadParticles
===============
*/
void R_FreeDeadParticles( particle_t **ppparticles )
{
	particle_t	*kill;
	particle_t	*p;

	for ( ;; )
	{
		kill = *ppparticles;
		if ( kill && kill->die < cl.time )
		{
			if ( kill->deathfunc )
				kill->deathfunc( kill );
			kill->deathfunc = NULL;
			*ppparticles = kill->next;
			kill->next = free_particles;
			free_particles = kill;
			continue;
		}
		break;
	}

	for ( p = *ppparticles; p; p = p->next )
	{
		for ( ;; )
		{
			kill = p->next;
			if ( kill && kill->die < cl.time )
			{
				if ( kill->deathfunc )
					kill->deathfunc( kill );
				kill->deathfunc = NULL;
				p->next = kill->next;
				kill->next = free_particles;
				free_particles = kill;
				continue;
			}
			break;
		}
	}
}

/*
===============
R_ReadPointFile_f
===============
*/
void R_ReadPointFile_f( void )
{
	FILE		*f;
	vec3_t		org;
	int			r;
	int			c;
	particle_t	*p;
	char		name[MAX_PATH];
	char		base[256];

	if ( !cl.worldmodel || !cl.worldmodel->name )
		return;

	COM_StripExtension( cl.worldmodel->name, base );
	_snprintf( name, sizeof( name ), "%s.pts", base );
	name[ sizeof( name ) - 1 ] = 0;
	COM_FixSlashes( name );

	f = fopen( name, "rb" );
	if ( !f )
	{
		Con_Printf( "couldn't open %s\n", name );
		return;
	}

	Con_Printf( "Reading %s...\n", name );
	c = 0;
	for ( ;; )
	{
		r = fscanf( f, "%f %f %f\n", &org[0], &org[1], &org[2] );
		if ( r != 3 )
			break;
		c++;

		if ( !free_particles )
		{
			Con_Printf( "Not enough free particles\n" );
			break;
		}
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = 99999;
		p->color = ( -c ) & 15;
		p->packedColor = 0;
		p->type = pt_static;
		VectorCopy( vec3_origin, p->vel );
		VectorCopy( org, p->org );
	}

	fclose( f );
	Con_Printf( "%i points read\n", c );
}

/*
===============
R_RunParticleEffect
===============
*/
void R_RunParticleEffect( vec_t *org, vec_t *dir, int color, int count )
{
	int			i, j;
	particle_t	*p;

	for ( i = 0; i < count; i++ )
	{
		if ( !free_particles )
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		if ( count == 1024 )
		{
			// rocket explosion
			p->die = cl.time + 5;
			p->color = ramp1[0];
			p->packedColor = 0;
			p->ramp = RandomLong( 0, 3 );
			if ( i & 1 )
			{
				p->type = pt_explode;
				for ( j = 0; j < 3; j++ )
				{
					p->org[j] = org[j] + RandomFloat( -16, 16 );
					p->vel[j] = RandomFloat( -256, 256 );
				}
			}
			else
			{
				p->type = pt_explode2;
				for ( j = 0; j < 3; j++ )
				{
					p->org[j] = org[j] + RandomFloat( -16, 16 );
					p->vel[j] = RandomFloat( -256, 256 );
				}
			}
		}
		else
		{
			p->die = cl.time + RandomFloat( 0, 0.4 );
			p->color = ( color & ~7 ) + RandomLong( 0, 7 );
			p->packedColor = 0;
			p->type = pt_slowgrav;
			for ( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + RandomFloat( -8, 8 );
				p->vel[j] = dir[j] * 15;
			}
		}
	}
}

/*
===============
R_ParseParticleEffect

Parse an effect out of the server message
===============
*/
void R_ParseParticleEffect( void )
{
	vec3_t	org, dir;
	int		i, count, msgcount, color;

	for ( i = 0; i < 3; i++ )
		org[i] = MSG_ReadCoord( &net_message );
	for ( i = 0; i < 3; i++ )
		dir[i] = MSG_ReadChar() * ( 1.0 / 16 );
	msgcount = MSG_ReadByte();
	color = MSG_ReadByte();

	if ( msgcount == 255 )
		count = 1024;
	else
		count = msgcount;

	R_RunParticleEffect( org, dir, color, count );
}

/*
===============
R_ParticleExplosion
===============
*/
void R_ParticleExplosion( vec_t *org )
{
	int			i, j;
	particle_t	*p;

	for ( i = 0; i < 1024; i++ )
	{
		if ( !free_particles )
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 5;
		p->color = ramp1[0];
		p->packedColor = 0;
		p->ramp = RandomLong( 0, 3 );
		if ( i & 1 )
			p->type = pt_explode;
		else
			p->type = pt_explode2;

		do
		{
			for ( j = 0; j < 3; j++ )
				p->vel[j] = RandomFloat( -512, 512 );
		} while ( DotProduct( p->vel, p->vel ) > 512 * 512 );

		for ( j = 0; j < 3; j++ )
			p->org[j] = org[j] + p->vel[j] / 4;
	}
}

/*
===============
R_ParticleExplosion2
===============
*/
void R_ParticleExplosion2( vec_t *org, int colorStart, int colorLength )
{
	int			i, j;
	particle_t	*p;
	int			colorMod = 0;

	for ( i = 0; i < 512; i++ )
	{
		if ( !free_particles )
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 0.3;
		p->color = colorStart + ( colorMod % colorLength );
		p->packedColor = 0;
		colorMod++;

		p->type = pt_blob;
		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = org[j] + RandomFloat( -16, 16 );
			p->vel[j] = RandomFloat( -256, 256 );
		}
	}
}

/*
===============
R_BlobExplosion
===============
*/
void R_BlobExplosion( vec_t *org )
{
	int			i, j;
	particle_t	*p;

	for ( i = 0; i < 1024; i++ )
	{
		if ( !free_particles )
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + RandomFloat( 1, 1.4 );

		if ( i & 1 )
		{
			p->type = pt_blob;
			p->color = RandomLong( 66, 71 );
			p->packedColor = 0;
			for ( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + RandomFloat( -16, 16 );
				p->vel[j] = RandomFloat( -256, 256 );
			}
		}
		else
		{
			p->type = pt_blob2;
			p->color = RandomLong( 150, 155 );
			p->packedColor = 0;
			for ( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + RandomFloat( -16, 16 );
				p->vel[j] = RandomFloat( -256, 256 );
			}
		}
	}
}

/*
===============
R_TracerParticles
===============
*/
particle_t *R_TracerParticles( vec_t *org, vec_t *vel, float life )
{
	int			i;
	particle_t	*p;

	if ( !free_particles )
		return NULL;
	p = free_particles;
	free_particles = p->next;
	p->next = gpActiveTracers;
	gpActiveTracers = p;

	p->die = cl.time + life;
	p->color = 4;			// Use "tracer" color
	p->packedColor = 255;
	p->type = pt_static;
	p->ramp = tracerLength.value;
	for ( i = 0; i < 3; i++ )
	{
		p->org[i] = org[i];
		p->vel[i] = vel[i];
	}

	return p;
}

/*
===============
R_UserTracerParticle
===============
*/
void R_UserTracerParticle( vec_t *org, vec_t *vel, float life, int colorIndex, float length, unsigned char context, void (*deathfunc)( struct particle_s *particle ) )
{
	int			i;
	particle_t	*p;

	if ( colorIndex < 0 )
	{
		Con_Printf( "UserTracer with color < 0\n" );
		return;
	}

	if ( colorIndex > ARRAYSIZE( gTracerColors ) )
	{
		Con_Printf( "UserTracer with color > %d\n", ARRAYSIZE( gTracerColors ) );
		return;
	}

	if ( !free_particles )
		return;
	p = free_particles;
	free_particles = p->next;
	p->next = gpActiveTracers;
	gpActiveTracers = p;

	p->die = cl.time + life;
	p->color = colorIndex;
	p->packedColor = 255;
	p->type = pt_static;
	p->ramp = length;
	for ( i = 0; i < 3; i++ )
	{
		p->org[i] = org[i];
		p->vel[i] = vel[i];
	}
	p->deathfunc = deathfunc;
	p->context = context;
}

/*
===============
R_SparkStreaks
===============
*/
void R_SparkStreaks( vec_t *pos, int count, int velocityMin, int velocityMax )
{
	int			i, j;
	particle_t	*p;

	for ( i = 0; i < count; i++ )
	{
		if ( !free_particles )
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = gpActiveTracers;
		gpActiveTracers = p;

		p->die = cl.time + RandomFloat( 0.1, 0.5 );
		p->color = 5;
		p->packedColor = 255;
		p->type = pt_grav;
		p->ramp = 0.5;
		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = pos[j];
			p->vel[j] = RandomFloat( velocityMin, velocityMax );
		}
	}
}

/*
===============
R_BulletImpactParticles
===============
*/
void R_BulletImpactParticles( vec_t *pos )
{
	int			i, quantity;
	int			color;
	float		dist;
	particle_t	*p;
	vec3_t		dir;

	VectorSubtract( pos, r_origin, dir );
	dist = Length( dir );
	if ( dist > 1000 )
		dist = 1000;

	quantity = ( 1000 - dist ) / 100;
	if ( quantity == 0 )
		quantity = 1;

	color = 3 - ( ( 30 * quantity ) / 100 );
	quantity *= 4;

	R_SparkStreaks( pos, 2, -200, 200 );

	for ( i = 0; i < quantity; i++ )
	{
		if ( !free_particles )
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorCopy( pos, p->org );

		dir[0] = RandomFloat( -1, 1 );
		dir[1] = RandomFloat( -1, 1 );
		dir[2] = RandomFloat( -1, 1 );

		VectorCopy( dir, p->vel );
		VectorScale( p->vel, RandomFloat( 50, 100 ), p->vel );

		p->color = 3 - color;
		p->packedColor = 0;
		p->type = pt_grav;
		p->die = cl.time + 0.5;
	}
}

/*
===============
R_FlickerParticles
===============
*/
void R_FlickerParticles( vec_t *org )
{
	int			i, j;
	particle_t	*p;

	for ( i = 0; i < 15; i++ )
	{
		if ( !free_particles )
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = org[j];
			p->vel[j] = RandomFloat( -32, 32 );
		}
		p->vel[2] = RandomFloat( 80, 143 );
		p->die = cl.time + 2;
		p->ramp = 0;
		p->color = 254;
		p->packedColor = 0;
		p->type = pt_blob2;
	}
}

/*
===============
R_StreakSplash
===============
*/
void R_StreakSplash( vec_t *pos, vec_t *dir, int color, int count, float speed, int velocityMin, int velocityMax )
{
	int			i, j;
	particle_t	*p;
	vec3_t		initialVelocity;

	VectorScale( dir, speed, initialVelocity );

	for ( i = 0; i < count; i++ )
	{
		if ( !free_particles )
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = gpActiveTracers;
		gpActiveTracers = p;

		p->die = cl.time + RandomFloat( 0.1, 0.5 );
		p->color = color;
		p->packedColor = 255;
		p->type = pt_grav;
		p->ramp = 1;
		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = pos[j];
			p->vel[j] = initialVelocity[j] + RandomFloat( velocityMin, velocityMax );
		}
	}
}

/*
===============
R_ParticleBurst
===============
*/
void R_ParticleBurst( vec_t *pos, int size, int color, float life )
{
	int			i, j, k;
	particle_t	*p;
	vec3_t		dir, dest;
	float		velocity, dist;

	for ( i = -16; i < 16; i++ )
	{
		for ( j = -16; j < 16; j++ )
		{
			for ( k = 0; k < 1; k++ )
			{
				if ( !free_particles )
					return;
				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->color = color + RandomLong( 0, 10 );
				p->packedColor = 0;
				p->type = pt_static;

				VectorCopy( pos, p->org );

				dest[0] = pos[0] + RandomFloat( -size, size );
				dest[1] = pos[1] + RandomFloat( -size, size );
				dest[2] = pos[2] + RandomFloat( -size, size );

				VectorSubtract( dest, p->org, dir );
				dist = VectorNormalize( dir );
				velocity = dist / life;

				p->die = cl.time + life + RandomFloat( -0.5, 0.5 );
				VectorScale( dir, velocity, p->vel );
			}
		}
	}
}

/*
===============
R_LavaSplash
===============
*/
void R_LavaSplash( vec_t *org )
{
	int			i, j, k;
	particle_t	*p;
	float		vel;
	vec3_t		dir;

	for ( i = -16; i < 16; i++ )
	{
		for ( j = -16; j < 16; j++ )
		{
			for ( k = 0; k < 1; k++ )
			{
				if ( !free_particles )
					return;
				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->die = cl.time + RandomFloat( 2, 2.62 );
				p->color = RandomLong( 224, 231 );
				p->packedColor = 0;
				p->type = pt_slowgrav;

				dir[0] = j * 8 + RandomFloat( 0, 7 );
				dir[1] = i * 8 + RandomFloat( 0, 7 );
				dir[2] = 256;

				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + RandomFloat( 0, 63 );

				VectorNormalize( dir );
				vel = RandomFloat( 50, 113 );
				VectorScale( dir, vel, p->vel );
			}
		}
	}
}

/*
===============
R_LargeFunnel
===============
*/
void R_LargeFunnel( vec_t *org, int reverse )
{
	int			i, j;
	particle_t	*p;
	float		vel;
	vec3_t		dir;
	vec3_t		dest;
	float		flDist;

	for ( i = -256; i <= 256; i += 32 )
	{
		for ( j = -256; j <= 256; j += 32 )
		{
			if ( !free_particles )
				return;
			p = free_particles;
			free_particles = p->next;
			p->next = active_particles;
			active_particles = p;

			if ( reverse )
			{
				VectorCopy( org, p->org );

				dest[0] = org[0] + i;
				dest[1] = org[1] + j;
				dest[2] = org[2] + RandomFloat( 100, 800 );

				// send particle heading to org at a random speed
				VectorSubtract( dest, p->org, dir );

				// velocity based on how far particle has to travel away from org
				vel = dest[2] / 8;
			}
			else
			{
				p->org[0] = org[0] + i;
				p->org[1] = org[1] + j;
				p->org[2] = org[2] + RandomFloat( 100, 800 );

				// send particle heading to org at a random speed
				VectorSubtract( org, p->org, dir );

				// velocity based on how far particle starts from org
				vel = p->org[2] / 8;
			}

			p->color = 244;
			p->packedColor = 0;
			p->type = pt_static;

			flDist = VectorNormalize( dir );	// normalize vector

			if ( vel < 64 )
				vel = 64;

			vel += RandomFloat( 64, 128 );
			VectorScale( dir, vel, p->vel );

			// die right when you get there
			p->die = cl.time + ( flDist / vel );
		}
	}
}

/*
===============
R_TeleportSplash
===============
*/
void R_TeleportSplash( vec_t *org )
{
	int			i, j, k;
	particle_t	*p;
	float		vel;
	vec3_t		dir;

	for ( i = -16; i < 16; i += 4 )
	{
		for ( j = -16; j < 16; j += 4 )
		{
			for ( k = -24; k < 32; k += 4 )
			{
				if ( !free_particles )
					return;
				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->die = cl.time + RandomFloat( 0.2, 0.34 );
				p->color = RandomLong( 7, 14 );
				p->packedColor = 0;
				p->type = pt_slowgrav;

				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;

				p->org[0] = org[0] + i + RandomFloat( 0, 3 );
				p->org[1] = org[1] + j + RandomFloat( 0, 3 );
				p->org[2] = org[2] + k + RandomFloat( 0, 3 );

				VectorNormalize( dir );
				vel = RandomFloat( 50, 113 );
				VectorScale( dir, vel, p->vel );
			}
		}
	}
}

/*
===============
R_ShowLine
===============
*/
void R_ShowLine( vec_t *start, vec_t *end )
{
	vec3_t		vec;
	float		len;
	particle_t	*p;
	int			dec;

	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );
	dec = 5;
	VectorScale( vec, dec, vec );

	while ( len > 0 )
	{
		len -= dec;

		if ( !free_particles )
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorCopy( vec3_origin, p->vel );

		p->die = cl.time + 30;
		p->color = 75;
		p->packedColor = 0;
		p->type = pt_static;

		VectorCopy( start, p->org );
		VectorAdd( start, vec, start );
	}
}

/*
===============
R_BloodStream
===============
*/
void R_BloodStream( vec_t *org, vec_t *dir, int pcolor, int speed )
{
	// Add our particles
	vec3_t		dirCopy;
	float		arc = 0.05;
	int			count;
	int			count2;
	particle_t	*p;
	float		num;
	int			speedCopy = speed;

	VectorNormalize( dir );

	for ( count = 0; count < 100; count++ )
	{
		if ( !free_particles )
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 2.0;
		p->color = pcolor + RandomLong( 0, 9 );
		p->packedColor = 0;
		p->type = pt_vox_grav;

		VectorCopy( org, p->org );
		VectorCopy( dir, dirCopy );

		dirCopy[2] -= arc;
		arc -= 0.005;

		VectorScale( dirCopy, speedCopy, p->vel );

		speedCopy -= 0.00001;	// so last few will drip
	}

	// now a few rogue spurts
	arc = 0.075;
	for ( count = 0; count < ( speed / 5 ); count++ )
	{
		if ( !free_particles )
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 3.0;
		p->color = pcolor + RandomLong( 0, 9 );
		p->packedColor = 0;
		p->type = pt_vox_slowgrav;

		VectorCopy( org, p->org );
		VectorCopy( dir, dirCopy );

		dirCopy[2] -= arc;
		arc -= 0.005;

		num = RandomFloat( 0, 1 );
		speedCopy = speed * num;

		num *= 1.7;

		VectorScale( dirCopy, num, dirCopy );	// randomize a bit
		VectorScale( dirCopy, speedCopy, p->vel );

		for ( count2 = 0; count2 < 2; count2++ )
		{
			if ( !free_particles )
				return;
			p = free_particles;
			free_particles = p->next;
			p->next = active_particles;
			active_particles = p;

			p->die = cl.time + 3.0;
			p->color = pcolor + RandomLong( 0, 9 );
			p->packedColor = 0;
			p->type = pt_vox_slowgrav;

			p->org[0] = org[0] + RandomFloat( -1, 1 );
			p->org[1] = org[1] + RandomFloat( -1, 1 );
			p->org[2] = org[2] + RandomFloat( -1, 1 );

			VectorCopy( dir, dirCopy );

			dirCopy[2] -= arc;

			VectorScale( dirCopy, num, dirCopy );	// randomize a bit
			VectorScale( dirCopy, speedCopy, p->vel );
		}
	}
}

/*
===============
R_Blood
===============
*/
void R_Blood( vec_t *org, vec_t *dir, int pcolor, int speed )
{
	vec3_t		dirCopy;
	vec3_t		orgCopy;
	float		arc = 0.05;
	int			count;
	int			count2;
	particle_t	*p;
	int			pspeed;

	pspeed = speed * 3;

	VectorNormalize( dir );

	for ( count = 0; count < ( speed / 2 ); count++ )
	{
		orgCopy[0] = org[0] + RandomFloat( -3, 3 );
		orgCopy[1] = org[1] + RandomFloat( -3, 3 );
		orgCopy[2] = org[2] + RandomFloat( -3, 3 );

		dirCopy[0] = dir[0] + RandomFloat( -0.06, 0.06 );
		dirCopy[1] = dir[1] + RandomFloat( -0.06, 0.06 );
		dirCopy[2] = dir[2] + RandomFloat( -0.06, 0.06 );

		for ( count2 = 0; count2 < 8; count2++ )
		{
			if ( !free_particles )
				return;
			p = free_particles;
			free_particles = p->next;
			p->next = active_particles;
			active_particles = p;

			p->die = cl.time + 1.5;
			p->color = pcolor + RandomLong( 0, 9 );
			p->packedColor = 0;
			p->type = pt_vox_grav;

			p->org[0] = orgCopy[0] + RandomFloat( -1, 1 );
			p->org[1] = orgCopy[1] + RandomFloat( -1, 1 );
			p->org[2] = orgCopy[2] + RandomFloat( -1, 1 );

			VectorScale( dirCopy, pspeed, p->vel );
		}

		pspeed -= speed;
	}
}

/*
===============
R_RocketTrail
===============
*/
void R_RocketTrail( vec_t *start, vec_t *end, int type )
{
	vec3_t		vec, right, up;
	float		len;
	int			j;
	particle_t	*p;
	int			dec;
	static int	tracercount;

	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );

	if ( type == 7 )
	{
		dec = 1;
		VectorMatrix( vec, right, up );
	}
	else if ( type < 128 )
	{
		dec = 3;
	}
	else
	{
		dec = 1;
		type -= 128;
	}

	VectorScale( vec, dec, vec );

	while ( len > 0 )
	{
		len -= dec;

		if ( !free_particles )
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorCopy( vec3_origin, p->vel );
		p->die = cl.time + 2;

		switch ( type )
		{
		case 0:	// rocket trail
			p->ramp = RandomLong( 0, 3 );
			p->color = ramp3[ (int)p->ramp ];
			p->type = pt_fire;
			for ( j = 0; j < 3; j++ )
				p->org[j] = start[j] + RandomFloat( -3, 3 );
			break;

		case 1:	// smoke smoke
			p->ramp = RandomLong( 2, 5 );
			p->color = ramp3[ (int)p->ramp ];
			p->type = pt_fire;
			for ( j = 0; j < 3; j++ )
				p->org[j] = start[j] + RandomFloat( -3, 3 );
			break;

		case 2:	// blood
			p->type = pt_grav;
			p->color = RandomLong( 67, 74 );
			for ( j = 0; j < 3; j++ )
				p->org[j] = start[j] + RandomFloat( -3, 3 );
			break;

		case 3:
		case 5:	// tracer
			p->die = cl.time + 0.5;
			p->type = pt_static;
			if ( type == 3 )
				p->color = 52 + ( ( tracercount & 4 ) << 1 );
			else
				p->color = 230 + ( ( tracercount & 4 ) << 1 );

			tracercount++;

			VectorCopy( start, p->org );
			if ( tracercount & 1 )
			{
				p->vel[0] = 30 * vec[1];
				p->vel[1] = 30 * -vec[0];
			}
			else
			{
				p->vel[0] = 30 * -vec[1];
				p->vel[1] = 30 * vec[0];
			}
			break;

		case 4:	// slight blood
			p->type = pt_grav;
			p->color = RandomLong( 67, 70 );
			for ( j = 0; j < 3; j++ )
				p->org[j] = start[j] + RandomFloat( -3, 3 );
			len -= 3;
			break;

		case 6:	// voor trail
			p->ramp = RandomLong( 0, 3 );
			p->color = ramp3[ (int)p->ramp ];
			p->type = pt_fire;
			VectorCopy( start, p->org );
			break;

		case 7:	// explosion tracer
			{
				float	s, c, x, y;

				x = RandomLong( 0, 65535 );
				y = RandomLong( 8, 16 );
				s = sin( x ) * y;
				c = cos( x ) * y;

				p->org[0] = start[0] + s * right[0] + c * up[0];
				p->org[1] = start[1] + s * right[1] + c * up[1];
				p->org[2] = start[2] + s * right[2] + c * up[2];
				VectorSubtract( start, p->org, p->vel );
				VectorScale( p->vel, 2.0, p->vel );
				VectorMA( p->vel, RandomFloat( 96, 111 ), vec, p->vel );
				p->die = cl.time + 2.0;
				p->type = pt_explode2;
				p->ramp = RandomLong( 0, 3 );
				p->color = ramp3[ (int)p->ramp ];
			}
			break;
		}

		p->packedColor = 0;
		VectorAdd( start, vec, start );
	}
}

/*
===============
R_DrawParticles
===============
*/
void R_DrawParticles( void )
{
	particle_t	*p;
	float		grav;
	int			i;
	float		time2, time3;
	float		time1;
	float		dvel;
	float		frametime;
	vec3_t		up, right;
	float		scale;

	GL_Bind( particletexture );
	qglEnable( GL_ALPHA_TEST );
	qglEnable( GL_BLEND );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglBegin( GL_TRIANGLES );

	VectorScale( vup, 1.5, up );
	VectorScale( vright, 1.5, right );

	frametime = cl.time - cl.oldtime;
	time3 = frametime * 15;
	time2 = frametime * 10;
	time1 = frametime * 5;
	grav = frametime * movevars.gravity * 0.05;
	dvel = 4 * frametime;

	R_FreeDeadParticles( &active_particles );

	for ( p = active_particles; p; p = p->next )
	{
		if ( p->type != pt_blob )
		{
			// hack a scale up to keep particles from disapearing
			scale = ( p->org[0] - r_origin[0] ) * vpn[0] + ( p->org[1] - r_origin[1] ) * vpn[1] + ( p->org[2] - r_origin[2] ) * vpn[2];
			if ( scale < 20 )
				scale = 1;
			else
				scale = 1 + scale * 0.004;

			{
				word	*pb;
				byte	rgba[4];

				pb = &host_basepal[ p->color * 4 ];
				if ( filterMode )
				{
					rgba[0] = pb[2] * filterColorRed * filterBrightness;
					rgba[1] = pb[1] * filterColorGreen * filterBrightness;
					rgba[2] = pb[0] * filterColorBlue * filterBrightness;
					rgba[3] = 255 * filterBrightness * filterBrightness;
				}
				else
				{
					rgba[0] = pb[2];
					rgba[1] = pb[1];
					rgba[2] = pb[0];
					rgba[3] = 255;
				}
				qglColor3ubv( rgba );
			}

			qglTexCoord2f( 0, 0 );
			qglVertex3fv( p->org );
			qglTexCoord2f( 1, 0 );
			qglVertex3f( p->org[0] + up[0] * scale, p->org[1] + up[1] * scale, p->org[2] + up[2] * scale );
			qglTexCoord2f( 0, 1 );
			qglVertex3f( p->org[0] + right[0] * scale, p->org[1] + right[1] * scale, p->org[2] + right[2] * scale );
		}

		if ( p->type != pt_clientcustom )
		{
			p->org[0] += p->vel[0] * frametime;
			p->org[1] += p->vel[1] * frametime;
			p->org[2] += p->vel[2] * frametime;
		}

		switch ( p->type )
		{
		case pt_fire:
			p->ramp += time1;
			if ( p->ramp >= 6 )
			{
				p->die = -1;
			}
			else
			{
				p->color = ramp3[ (int)p->ramp ];
				p->packedColor = 0;
			}
			p->vel[2] += grav;
			break;

		case pt_explode:
			p->ramp += time2;
			if ( p->ramp >= 8 )
			{
				p->die = -1;
			}
			else
			{
				p->color = ramp1[ (int)p->ramp ];
				p->packedColor = 0;
			}
			for ( i = 0; i < 3; i++ )
				p->vel[i] += p->vel[i] * dvel;
			p->vel[2] -= grav;
			break;

		case pt_explode2:
			p->ramp += time3;
			if ( p->ramp >= 8 )
			{
				p->die = -1;
			}
			else
			{
				p->color = ramp2[ (int)p->ramp ];
				p->packedColor = 0;
			}
			for ( i = 0; i < 3; i++ )
				p->vel[i] -= p->vel[i] * frametime;
			p->vel[2] -= grav;
			break;

		case pt_blob:
		case pt_blob2:
			p->ramp += time2;
			if ( p->ramp >= 9 )
				p->ramp = 0;
			p->color = gSparkRamp[ (int)p->ramp ];
			p->packedColor = 0;
			p->vel[0] = p->vel[0] - p->vel[0] * 0.5 * frametime;
			p->vel[1] = p->vel[1] - p->vel[1] * 0.5 * frametime;
			p->vel[2] -= grav * 5;
			if ( RandomLong( 0, 3 ) )
				p->type = pt_blob;
			else
				p->type = pt_blob2;
			break;

		case pt_grav:
			p->vel[2] -= grav * 20;
			break;

		case pt_slowgrav:
			p->vel[2] = grav;
			break;

		case pt_vox_grav:
			p->vel[2] -= grav * 8;
			break;

		case pt_vox_slowgrav:
			p->vel[2] -= grav * 4;
			break;

		case pt_clientcustom:
			if ( p->callback )
				p->callback( p, frametime );
			break;
		}
	}

	qglEnd();

	R_TracerDraw();
	R_BeamDrawList();

	qglDisable( GL_BLEND );
	qglDisable( GL_ALPHA_TEST );
}

/*
===============
R_TracerDraw
===============
*/
void R_TracerDraw( void )
{
	float		scale, attenuation, frametime, clipDist, gravity, size;
	vec3_t		up, right, start, end;
	particle_t	*p;
	vec3_t		screenLast, screen, tmp, normal;
	int			clip;
	qboolean	draw;

	if ( !gpActiveTracers )
		return;

	gTracerColors[4].r = tracerRed.value * tracerAlpha.value * 255;
	gTracerColors[4].g = tracerGreen.value * tracerAlpha.value * 255;
	gTracerColors[4].b = tracerBlue.value * tracerAlpha.value * 255;

	R_FreeDeadParticles( &gpActiveTracers );

	frametime = cl.time - cl.oldtime;

	VectorScale( vup, 1.5, up );
	VectorScale( vright, 1.5, right );

	if ( !tri.SpriteTexture( cl_sprite_dot, 0 ) )
		return;

	gravity = frametime * movevars.gravity;
	clipDist = DotProduct( r_origin, vpn );

	attenuation = 1.0 - frametime * 0.9;
	if ( attenuation < 0 )
		attenuation = 0;

	tri.RenderMode( kRenderTransAdd );
	tri.CullFace( TRI_NONE );

	for ( p = gpActiveTracers; p; p = p->next )
	{
		color24	*pColor;

		pColor = &gTracerColors[ p->color ];
		draw = true;

		if ( p->die - 0.1 > cl.time )
			scale = p->ramp * 0.1;
		else
			scale = ( p->die - cl.time ) * p->ramp;

		end[0] = p->org[0] + scale * p->vel[0];
		end[1] = p->org[1] + scale * p->vel[1];
		end[2] = p->org[2] + scale * p->vel[2];
		VectorCopy( p->org, start );

		clip = ScreenTransform( start, screenLast );
		if ( !clip )
			clip = ScreenTransform( end, screen );

		if ( clip )
		{
			float	fraction, dist1, dist2;

			dist1 = DotProduct( start, vpn ) - clipDist;
			dist2 = DotProduct( end, vpn ) - clipDist;

			if ( dist1 <= 0 && dist2 <= 0 )
			{
				draw = false;
			}
			else
			{
				fraction = dist2 - dist1;
				if ( fraction < 0.01 )
				{
					draw = false;
				}
				else
				{
					fraction = dist1 / fraction;
					if ( dist1 <= 0 )
					{
						start[0] = start[0] + ( end[0] - start[0] ) * fraction;
						start[1] = start[1] + ( end[1] - start[1] ) * fraction;
						start[2] = start[2] + ( end[2] - start[2] ) * fraction;
					}
					else
					{
						end[0] = start[0] + ( end[0] - start[0] ) * fraction;
						end[1] = start[1] + ( end[1] - start[1] ) * fraction;
						end[2] = start[2] + ( end[2] - start[2] ) * fraction;
					}
					ScreenTransform( start, screenLast );
					ScreenTransform( end, screen );
				}
			}
		}

		if ( draw )
		{
			ScreenTransform( start, screenLast );
			ScreenTransform( end, screen );

			// build world-space normal to screen-space direction vector
			VectorSubtract( screen, screenLast, tmp );
			size = gTracerSize[ p->type ];
			// we don't need Z, we're in screen space
			tmp[2] = 0;
			VectorNormalize( tmp );

			// build point along noraml line (normal is -y, x)
			VectorScale( vup, tmp[0] * size, normal );
			VectorMA( normal, -tmp[1] * size, vright, normal );

			tri.Begin( TRI_QUADS );
			tri.Color4ub( pColor->r, pColor->g, pColor->b, p->packedColor );

			tri.Brightness( 0 );
			tri.TexCoord2f( 0, 0 );
			tri.Vertex3f( start[0] + normal[0], start[1] + normal[1], start[2] + normal[2] );

			tri.Brightness( 1 );
			tri.TexCoord2f( 0, 1 );
			tri.Vertex3f( end[0] + normal[0], end[1] + normal[1], end[2] + normal[2] );

			tri.Brightness( 1 );
			tri.TexCoord2f( 1, 1 );
			tri.Vertex3f( end[0] - normal[0], end[1] - normal[1], end[2] - normal[2] );

			tri.Brightness( 0 );
			tri.TexCoord2f( 1, 0 );
			tri.Vertex3f( start[0] - normal[0], start[1] - normal[1], start[2] - normal[2] );

			tri.End();
		}

		p->org[0] += p->vel[0] * frametime;
		p->org[1] += p->vel[1] * frametime;
		p->org[2] += p->vel[2] * frametime;

		switch ( p->type )
		{
		case pt_grav:
			p->vel[2] -= gravity;
			p->vel[0] *= attenuation;
			p->vel[1] *= attenuation;
			p->packedColor = ( p->die - cl.time ) * 2 * 255;
			if ( p->packedColor > 255 )
				p->packedColor = 255;
			break;

		case pt_slowgrav:
			p->vel[2] = gravity * 0.05;
			break;
		}
	}

	tri.CullFace( TRI_FRONT );
	tri.RenderMode( kRenderNormal );
}

/*
==============
R_BeamAlloc
==============
*/
BEAM *R_BeamAlloc( void )
{
	BEAM	*pbeam;

	if ( !gpFreeBeams )
		return NULL;

	pbeam = gpFreeBeams;
	gpFreeBeams = pbeam->next;
	pbeam->next = gpActiveBeams;
	gpActiveBeams = pbeam;

	return pbeam;
}

/*
==============
R_BeamLightning
==============
*/
BEAM *R_BeamLightning( vec_t *start, vec_t *end, int modelIndex, float life, float width, float amplitude, float brightness, float speed )
{
	BEAM	*pbeam;

	pbeam = R_BeamAlloc();
	if ( !pbeam )
		return NULL;

	pbeam->die = cl.time;

	if ( modelIndex < 0 )
		return NULL;

	R_BeamSetup( pbeam, start, end, modelIndex, life, width, amplitude, brightness, speed );

	return pbeam;
}

/*
==============
R_BeamSetup
==============
*/
void R_BeamSetup( BEAM *pbeam, vec_t *start, vec_t *end, int modelIndex, float life, float width, float amplitude, float brightness, float speed )
{
	model_t	*sprite;

	sprite = CL_GetModelByIndex( modelIndex );
	if ( !sprite )
		return;

	pbeam->type = TE_BEAMPOINTS;
	pbeam->modelIndex = modelIndex;
	pbeam->frame = 0;
	pbeam->frameRate = 0;
	pbeam->frameCount = ModelFrameCount( sprite );

	VectorCopy( start, pbeam->source );
	VectorCopy( end, pbeam->target );
	VectorSubtract( end, start, pbeam->delta );

	pbeam->freq = speed * cl.time;
	pbeam->die = life + cl.time;
	pbeam->width = width;
	pbeam->amplitude = amplitude;
	pbeam->brightness = brightness;
	pbeam->speed = speed;

	if ( pbeam->amplitude >= 0.50 )
		pbeam->segments = Length( pbeam->delta ) * 0.25 + 3;	// one per 4 pixels
	else
		pbeam->segments = Length( pbeam->delta ) * 0.075 + 3;	// one per 16 pixels

	pbeam->flags = 0;
	pbeam->pFollowModel = NULL;
}

/*
==============
SetBeamAttributes
==============
*/
void SetBeamAttributes( BEAM *pbeam, float r, float g, float b, float framerate, int startFrame )
{
	pbeam->frame = startFrame;
	pbeam->frameRate = framerate;
	pbeam->r = r;
	pbeam->g = g;
	pbeam->b = b;
}

/*
==============
R_DrawBeamEntList
==============
*/
void R_DrawBeamEntList( float frametime )
{
	BEAM		beam;
	int			i, beamType;
	cl_entity_t	*ent;

	for ( i = 0; i < cl_numbeamentities; i++ )
	{
		ent = cl_beamentities[i];
		beamType = ent->curstate.rendermode & 0x0F;

		R_BeamSetup( &beam, ent->origin, ent->curstate.angles, ent->curstate.movetype, 0, ent->curstate.scale, ent->curstate.body * 0.01, CL_FxBlend( ent ) * ( 1.0 / 255.0 ), ent->curstate.animtime );
		SetBeamAttributes( &beam, ent->curstate.rendercolor.r * ( 1.0 / 255.0 ), ent->curstate.rendercolor.g * ( 1.0 / 255.0 ), ent->curstate.rendercolor.b * ( 1.0 / 255.0 ), 0, ent->curstate.frame );

		switch ( beamType )
		{
		case BEAM_ENTS:
			beam.type = TE_BEAMPOINTS;
			beam.flags = FBEAM_STARTENTITY | FBEAM_ENDENTITY;
			beam.startEntity = ent->curstate.sequence;
			beam.endEntity = ent->curstate.skin;
			break;

		case BEAM_ENTPOINT:
			beam.type = TE_BEAMPOINTS;
			beam.flags = FBEAM_ENDENTITY;
			beam.startEntity = 0;
			beam.endEntity = ent->curstate.skin;
			break;
		}

		if ( ent->curstate.rendermode & FBEAM_SINENOISE )
			beam.flags |= FBEAM_SINENOISE;
		if ( ent->curstate.rendermode & FBEAM_SOLID )
			beam.flags |= FBEAM_SOLID;
		if ( ent->curstate.rendermode & FBEAM_SHADEIN )
			beam.flags |= FBEAM_SHADEIN;
		if ( ent->curstate.rendermode & FBEAM_SHADEOUT )
			beam.flags |= FBEAM_SHADEOUT;

		beam.pFollowModel = NULL;

		R_BeamDraw( &beam, frametime );
	}
}

/*
==============
R_GetBeamAttachmentEntity
==============
*/
cl_entity_t *R_GetBeamAttachmentEntity( int index )
{
	int		num_guess;

	if ( index >= 0 )
		return &cl_entities[ BEAMENT_ENTITY( index ) ];

	num_guess = -index;
	return ClientDLL_GetUserEntity( BEAMENT_ENTITY( num_guess ) );
}

/*
==============
R_BeamGetAttachmentPoint
==============
*/
float *R_BeamGetAttachmentPoint( cl_entity_t *ent, int attachment )
{
	int		attach;

	if ( attachment >= 0 )
		attach = BEAMENT_ATTACHMENT( attachment );
	else
		attach = BEAMENT_ATTACHMENT( -attachment );

	if ( attach )
		return ent->attachment[ attach - 1 ];

	if ( ent->index == cl.playernum + 1 )
		return cl.simorg;

	return ent->origin;
}

/*
==============
R_BeamEnts
==============
*/
BEAM *R_BeamEnts( int startEnt, int endEnt, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b )
{
	BEAM		*pbeam;
	cl_entity_t	*start, *end;

	start = R_GetBeamAttachmentEntity( startEnt );
	end = R_GetBeamAttachmentEntity( endEnt );
	if ( !start || !end )
		return NULL;

	// Don't start temporary beams out of the PVS
	if ( life != 0 && ( !start->model || !end->model ) )
		return NULL;

	pbeam = R_BeamLightning( vec3_origin, vec3_origin, modelIndex, life, width, amplitude, brightness, speed );
	if ( !pbeam )
		return NULL;

	pbeam->type = TE_BEAMPOINTS;
	pbeam->flags = FBEAM_STARTENTITY | FBEAM_ENDENTITY;
	if ( life == 0 )
		pbeam->flags |= FBEAM_FOREVER;

	pbeam->startEntity = startEnt;
	pbeam->endEntity = endEnt;

	SetBeamAttributes( pbeam, r, g, b, framerate, startFrame );

	return pbeam;
}

/*
==============
R_BeamEntPoint
==============
*/
BEAM *R_BeamEntPoint( int startEnt, vec_t *end, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b )
{
	BEAM		*pbeam;
	cl_entity_t	*start;

	start = R_GetBeamAttachmentEntity( startEnt );
	if ( !start )
		return NULL;

	if ( life != 0 && !start->model )
		return NULL;

	pbeam = R_BeamLightning( vec3_origin, end, modelIndex, life, width, amplitude, brightness, speed );
	if ( !pbeam )
		return NULL;

	pbeam->type = TE_BEAMPOINTS;
	pbeam->flags = FBEAM_STARTENTITY;
	if ( life == 0 )
		pbeam->flags |= FBEAM_FOREVER;

	pbeam->startEntity = startEnt;
	pbeam->endEntity = 0;

	SetBeamAttributes( pbeam, r, g, b, framerate, startFrame );

	return pbeam;
}

/*
==============
R_BeamPoints
==============
*/
BEAM *R_BeamPoints( vec_t *start, vec_t *end, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b )
{
	BEAM	*pbeam;

	if ( life != 0 && !R_BeamCull( start, end, 1 ) )
		return NULL;

	pbeam = R_BeamLightning( start, end, modelIndex, life, width, amplitude, brightness, speed );
	if ( !pbeam )
		return NULL;

	if ( life == 0 )
		pbeam->flags |= FBEAM_FOREVER;

	SetBeamAttributes( pbeam, r, g, b, framerate, startFrame );

	return pbeam;
}

/*
==============
R_BeamCirclePoints
==============
*/
BEAM *R_BeamCirclePoints( int type, vec_t *start, vec_t *end, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b )
{
	BEAM	*pbeam;

	pbeam = R_BeamLightning( start, end, modelIndex, life, width, amplitude, brightness, speed );
	if ( !pbeam )
		return NULL;

	pbeam->type = type;
	if ( life == 0 )
		pbeam->flags |= FBEAM_FOREVER;

	SetBeamAttributes( pbeam, r, g, b, framerate, startFrame );

	return pbeam;
}

/*
==============
R_BeamFollow
==============
*/
BEAM *R_BeamFollow( int startEnt, int modelIndex, float life, float width, float r, float g, float b, float brightness )
{
	BEAM	*pbeam;

	pbeam = R_BeamLightning( vec3_origin, vec3_origin, modelIndex, life, width, life, brightness, 1.0 );
	if ( !pbeam )
		return NULL;

	pbeam->type = TE_BEAMFOLLOW;
	pbeam->flags = FBEAM_STARTENTITY;
	pbeam->startEntity = startEnt;

	SetBeamAttributes( pbeam, r, g, b, 1.0, 0 );

	return pbeam;
}

/*
==============
R_BeamRing
==============
*/
BEAM *R_BeamRing( int startEnt, int endEnt, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b )
{
	BEAM		*pbeam;
	cl_entity_t	*start, *end;

	start = R_GetBeamAttachmentEntity( startEnt );
	end = R_GetBeamAttachmentEntity( endEnt );
	if ( !start || !end )
		return NULL;

	if ( life != 0 && ( !start->model || !end->model ) )
		return NULL;

	pbeam = R_BeamLightning( vec3_origin, vec3_origin, modelIndex, life, width, amplitude, brightness, speed );
	if ( !pbeam )
		return NULL;

	pbeam->type = TE_BEAMRING;
	pbeam->flags = FBEAM_STARTENTITY | FBEAM_ENDENTITY;
	if ( life == 0 )
		pbeam->flags |= FBEAM_FOREVER;

	pbeam->startEntity = startEnt;
	pbeam->endEntity = endEnt;

	SetBeamAttributes( pbeam, r, g, b, framerate, startFrame );

	return pbeam;
}

/*
==============
R_KillDeadBeams
==============
*/
void R_KillDeadBeams( int deadEntity )
{
	BEAM		*pbeam;
	BEAM		*pnewlist;
	BEAM		*pnext;
	particle_t	*pHead;

	pbeam = gpActiveBeams;
	pnewlist = NULL;
	while ( pbeam )
	{
		pnext = pbeam->next;
		if ( pbeam->startEntity != deadEntity )	// Link into new list.
		{
			pbeam->next = pnewlist;
			pnewlist = pbeam;
			pbeam = pnext;
			continue;
		}

		pbeam->flags &= ~( FBEAM_STARTENTITY | FBEAM_ENDENTITY );
		if ( pbeam->type != TE_BEAMFOLLOW )
		{
			// Die Die Die!
			pbeam->die = cl.time - 0.1;

			// Kill off particles
			pHead = pbeam->particles;
			while ( pHead )
			{
				pHead->die = cl.time - 0.1;
				pHead = pHead->next;
			}

			// Free the beam
			R_FreeDeadParticles( &pbeam->particles );

			Q_memset( pbeam, 0, sizeof( BEAM ) );
			pbeam->next = gpFreeBeams;
			gpFreeBeams = pbeam;
		}
		else
		{
			// Stays active
			pbeam->next = pnewlist;
			pnewlist = pbeam;
		}
		pbeam = pnext;
	}

	// We now have a new list with the bogus stuff released.
	gpActiveBeams = pnewlist;
}

/*
==============
R_BeamKill
==============
*/
void R_BeamKill( int deadEntity )
{
	BEAM	*pbeam;

	for ( pbeam = gpActiveBeams; pbeam; pbeam = pbeam->next )
	{
		if ( ( pbeam->flags & FBEAM_STARTENTITY ) && pbeam->startEntity == deadEntity )
		{
			pbeam->flags &= ~( FBEAM_STARTENTITY | FBEAM_FOREVER );
			if ( pbeam->type != TE_BEAMFOLLOW )
				pbeam->die = cl.time;
		}

		if ( ( pbeam->flags & FBEAM_ENDENTITY ) && pbeam->endEntity == deadEntity )
		{
			pbeam->flags &= ~( FBEAM_ENDENTITY | FBEAM_FOREVER );
			pbeam->die = cl.time;
		}
	}
}

/*
==============
R_Implosion
==============
*/
void R_Implosion( vec_t *end, float radius, int count, float life )
{
	int		i;
	vec3_t	start;
	vec3_t	temp;
	vec3_t	vel;

	radius = radius / 100.0;
	for ( i = 0; i < count; i++ )
	{
		temp[0] = RandomFloat( -100, 100 ) * radius;
		temp[1] = RandomFloat( -100, 100 ) * radius;
		temp[2] = RandomFloat( 0, 100 ) * radius;
		VectorAdd( temp, end, start );

		VectorScale( temp, -1.0 / life, vel );
		R_TracerParticles( start, vel, life );
	}
}

float	gNoise[ NOISE_DIVISIONS + 1 ];

/*
==============
Noise

Fractal noise generator, power of 2 wavelength
==============
*/
void Noise( float *noise, int divs )
{
	int		div2;

	div2 = divs >> 1;

	if ( divs < 2 )
		return;

	// Noise is normalized to +/- scale
	noise[div2] = ( noise[0] + noise[divs] ) * 0.5 + divs * RandomFloat( -0.125, 0.125 );
	if ( div2 > 1 )
	{
		Noise( &noise[div2], div2 );
		Noise( noise, div2 );
	}
}

/*
==============
SineNoise
==============
*/
void SineNoise( float *noise, int divs )
{
	int		i;
	float	freq, freq2;
	float	step;

	freq = cl.time * 1.0;
	freq2 = 1.0;
	step = M_PI / divs;
	freq = 0;

	for ( i = 0; i < divs; i++ )
	{
		noise[i] = sin( freq );
		freq = freq + step;
	}
}

/*
==============
ScreenTransform
==============
*/
int ScreenTransform( vec_t *point, vec_t *screen )
{
	float	w;

	screen[0] = gWorldToScreen[0] * point[0] + gWorldToScreen[4] * point[1] + gWorldToScreen[8] * point[2] + gWorldToScreen[12];
	screen[1] = gWorldToScreen[1] * point[0] + gWorldToScreen[5] * point[1] + gWorldToScreen[9] * point[2] + gWorldToScreen[13];
	w = gWorldToScreen[3] * point[0] + gWorldToScreen[7] * point[1] + gWorldToScreen[11] * point[2] + gWorldToScreen[15];

	if ( w != 0 )
	{
		w = 1.0 / w;
		screen[0] *= w;
		screen[1] *= w;
	}

	if ( w <= 0 )
		return 1;

	return 0;
}

/*
==============
WorldTransform
==============
*/
void WorldTransform( vec_t *screen, vec_t *point )
{
	float	w;

	point[0] = gScreenToWorld[0] * screen[0] + gScreenToWorld[4] * screen[1] + gScreenToWorld[8] * screen[2] + gScreenToWorld[12];
	point[1] = gScreenToWorld[1] * screen[0] + gScreenToWorld[5] * screen[1] + gScreenToWorld[9] * screen[2] + gScreenToWorld[13];
	point[2] = gScreenToWorld[2] * screen[0] + gScreenToWorld[6] * screen[1] + gScreenToWorld[10] * screen[2] + gScreenToWorld[14];
	w = gScreenToWorld[3] * screen[0] + gScreenToWorld[7] * screen[1] + gScreenToWorld[11] * screen[2] + gScreenToWorld[15];

	if ( w != 0 )
	{
		w = 1.0 / w;
		point[0] *= w;
		point[1] *= w;
		point[2] *= w;
	}
}

/*
==============
R_DrawSegs
==============
*/
void R_DrawSegs( vec_t *source, vec_t *delta, float width, float scale, float freq, float speed, int segments, int flags )
{
	int		i, noiseIndex, noiseStep;
	float	div, length, fraction, factor, vLast, vStep, brightness;
	vec3_t	last1, last2, point, screen, screenLast, tmp, normal;

	if ( segments < 2 )
		return;

	if ( segments > NOISE_DIVISIONS )	// UNDONE: Allow more segments?
		segments = NOISE_DIVISIONS;

	length = Length( delta ) * 0.01;
	if ( length < 0.5 )	// Don't lose all of the noise/texture on short beams
		length = 0.5;
	div = 1.0 / ( segments - 1 );

	vStep = length * div;	// Texture length texels per space pixel

	// UNDONE: Expose texture length scale factor to control "fuzziness"
	vLast = fmod( freq * speed, 1 );	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)

	if ( flags & FBEAM_SINENOISE )
	{
		if ( segments < 16 )
			segments = 16;
		scale = scale * 100;
		length = segments * 0.1;
	}
	else
	{
		scale = scale * length;
	}

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	ScreenTransform( source, screenLast );
	VectorMA( source, div, delta, point );
	ScreenTransform( point, screen );

	// Calc screen-space normal (for screen-space wide beams)
	VectorSubtract( screen, screenLast, tmp );
	// We don't need Z, we're in screen space
	tmp[2] = 0;
	VectorNormalize( tmp );
	VectorScale( vup, tmp[0], normal );	// Build point along noraml line (normal is -y, x)
	VectorMA( normal, -tmp[1], vright, normal );

	// Make a wide line
	VectorMA( source, width, normal, last1 );
	VectorMA( source, -width, normal, last2 );

	noiseStep = (int)( NOISE_DIVISIONS * div * 65536.0 );
	noiseIndex = noiseStep;

	if ( flags & FBEAM_SINENOISE )
		noiseIndex = 0;

	brightness = 1.0;
	if ( flags & FBEAM_SHADEIN )
		brightness = 0;

	for ( i = 1; i < segments; i++ )
	{
		fraction = i * div;

		tri.Brightness( brightness );
		tri.TexCoord2f( 0, vLast );
		tri.Vertex3fv( last1 );
		tri.Brightness( brightness );
		tri.TexCoord2f( 1, vLast );
		tri.Vertex3fv( last2 );

		if ( flags & FBEAM_SHADEIN )
			brightness = fraction;
		else if ( flags & FBEAM_SHADEOUT )
			brightness = 1.0 - fraction;

		// Distort using noise
		VectorMA( source, fraction, delta, point );
		if ( scale != 0 )
		{
			factor = scale * gNoise[ noiseIndex >> 16 ];
			if ( flags & FBEAM_SINENOISE )
			{
				VectorMA( point, factor * sin( fraction * M_PI * length + freq ), vup, point );
				VectorMA( point, factor * cos( fraction * M_PI * length + freq ), vright, point );
			}
			else
			{
				VectorMA( point, factor, vup, point );
				factor = scale * gNoise[ noiseIndex >> 16 ] * cos( fraction * M_PI * 3 + freq );
				VectorMA( point, factor, vright, point );
			}
		}

		// Transform point into screen space
		ScreenTransform( point, screen );

		// Build world-space normal to screen-space direction vector
		VectorSubtract( screen, screenLast, tmp );
		// We don't need Z, we're in screen space
		tmp[2] = 0;
		VectorNormalize( tmp );
		VectorScale( vup, tmp[0], normal );	// Build point along noraml line (normal is -y, x)
		VectorMA( normal, -tmp[1], vright, normal );

		// Make a wide line
		VectorMA( point, width, normal, last1 );
		VectorMA( point, -width, normal, last2 );

		vLast += vStep;	// Advance texture scroll (v axis only)
		tri.Brightness( brightness );
		tri.TexCoord2f( 1, vLast );
		tri.Vertex3fv( last2 );
		tri.Brightness( brightness );
		tri.TexCoord2f( 0, vLast );
		tri.Vertex3fv( last1 );

		VectorCopy( screen, screenLast );

		vLast = fmod( vLast, 1 );	// Since our texture repeats, keep vLast in [0..1)
		noiseIndex += noiseStep;
	}
}

/*
==============
R_DrawTorus
==============
*/
void R_DrawTorus( vec_t *source, vec_t *delta, float width, float scale, float freq, float speed, int segments )
{
	int		i, noiseIndex, noiseStep;
	float	div, length, fraction, factor, vLast, vStep;
	vec3_t	last1, last2, point, screen, screenLast, tmp, normal;

	if ( segments < 2 )
		return;

	if ( segments > NOISE_DIVISIONS )	// UNDONE: Allow more segments?
		segments = NOISE_DIVISIONS;

	length = Length( delta ) * 0.01;
	if ( length < 0.5 )	// Don't lose all of the noise/texture on short beams
		length = 0.5;
	div = 1.0 / ( segments - 1 );

	vStep = length * div;	// Texture length texels per space pixel

	// UNDONE: Expose texture length scale factor to control "fuzziness"
	vLast = fmod( freq * speed, 1 );	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	scale = scale * length;

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	noiseStep = (int)( NOISE_DIVISIONS * div * 65536.0 );
	noiseIndex = 0;

	for ( i = 0; i <= segments; i++ )
	{
		fraction = i * div;

		point[0] = sin( fraction * 2 * M_PI ) * freq * delta[2] + source[0];
		point[1] = cos( fraction * 2 * M_PI ) * freq * delta[2] + source[1];
		point[2] = source[2];

		// Distort using noise
		factor = scale * gNoise[ noiseIndex >> 16 ];
		VectorMA( point, factor, vup, point );
		factor = scale * gNoise[ noiseIndex >> 16 ] * cos( fraction * M_PI * 3 + freq );
		VectorMA( point, factor, vright, point );

		// Transform point into screen space
		ScreenTransform( point, screen );

		if ( i != 0 )
		{
			// Build world-space normal to screen-space direction vector
			VectorSubtract( screen, screenLast, tmp );
			// We don't need Z, we're in screen space
			tmp[2] = 0;
			VectorNormalize( tmp );
			VectorScale( vup, tmp[0], normal );	// Build point along noraml line (normal is -y, x)
			VectorMA( normal, -tmp[1], vright, normal );

			// Make a wide line
			VectorMA( point, width, normal, last1 );
			VectorMA( point, -width, normal, last2 );

			vLast += vStep;	// Advance texture scroll (v axis only)
			tri.TexCoord2f( 1, vLast );
			tri.Vertex3fv( last2 );
			tri.TexCoord2f( 0, vLast );
			tri.Vertex3fv( last1 );
		}
		VectorCopy( screen, screenLast );
		vLast = fmod( vLast, 1 );	// Since our texture repeats, keep vLast in [0..1)
		noiseIndex += noiseStep;
	}
}

/*
==============
R_DrawDisk
==============
*/
void R_DrawDisk( vec_t *source, vec_t *delta, float width, float scale, float freq, float speed, int segments )
{
	int		i;
	float	div, length, fraction, vLast, vStep;
	vec3_t	point;
	float	w;

	if ( segments < 2 )
		return;

	if ( segments > NOISE_DIVISIONS )	// UNDONE: Allow more segments?
		segments = NOISE_DIVISIONS;

	length = Length( delta ) * 0.01;
	if ( length < 0.5 )	// Don't lose all of the noise/texture on short beams
		length = 0.5;
	div = 1.0 / ( segments - 1 );

	vStep = length * div;	// Texture length texels per space pixel

	// UNDONE: Expose texture length scale factor to control "fuzziness"
	vLast = fmod( freq * speed, 1 );	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	scale = scale * length;

	w = freq * delta[2];

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	for ( i = 0; i < segments; i++ )
	{
		fraction = i * div;
		VectorCopy( source, point );

		tri.Brightness( 1.0 );
		tri.TexCoord2f( 1.0, vLast );
		tri.Vertex3fv( point );

		point[0] = sin( fraction * 2 * M_PI ) * w + source[0];
		point[1] = cos( fraction * 2 * M_PI ) * w + source[1];
		point[2] = source[2];

		tri.Brightness( 1.0 );
		tri.TexCoord2f( 0, vLast );
		tri.Vertex3fv( point );

		vLast += vStep;	// Advance texture scroll (v axis only)
		vLast = fmod( vLast, 1 );	// Since our texture repeats, keep vLast in [0..1)
	}
}

/*
==============
R_DrawCylinder
==============
*/
void R_DrawCylinder( vec_t *source, vec_t *delta, float width, float scale, float freq, float speed, int segments )
{
	int		i;
	float	div, length, fraction, vLast, vStep;
	vec3_t	point;

	if ( segments < 2 )
		return;

	if ( segments > NOISE_DIVISIONS )	// UNDONE: Allow more segments?
		segments = NOISE_DIVISIONS;

	length = Length( delta ) * 0.01;
	if ( length < 0.5 )	// Don't lose all of the noise/texture on short beams
		length = 0.5;
	div = 1.0 / ( segments - 1 );

	vStep = length * div;	// Texture length texels per space pixel

	// UNDONE: Expose texture length scale factor to control "fuzziness"
	vLast = fmod( freq * speed, 1 );	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	scale = scale * length;

	for ( i = 0; i < segments; i++ )
	{
		fraction = i * div;

		point[0] = sin( fraction * 2 * M_PI ) * freq * delta[2] + source[0];
		point[1] = cos( fraction * 2 * M_PI ) * freq * delta[2] + source[1];
		point[2] = source[2] + width;

		tri.Brightness( 0 );
		tri.TexCoord2f( 1, vLast );
		tri.Vertex3fv( point );

		point[0] = sin( fraction * 2 * M_PI ) * freq * ( delta[2] + width ) + source[0];
		point[1] = cos( fraction * 2 * M_PI ) * freq * ( delta[2] + width ) + source[1];
		point[2] = source[2] - width;

		tri.Brightness( 1 );
		tri.TexCoord2f( 0, vLast );
		tri.Vertex3fv( point );

		vLast += vStep;	// Advance texture scroll (v axis only)
		vLast = fmod( vLast, 1 );	// Since our texture repeats, keep vLast in [0..1)
	}
}

/*
==============
R_DrawBeamFollow
==============
*/
void R_DrawBeamFollow( BEAM *pbeam )
{
	particle_t	*pNew, *pHead;
	vec3_t		delta;
	float		fraction, div, vLast, vStep;
	vec3_t		last1, last2, screen, screenLast, tmp, normal;

	R_FreeDeadParticles( &pbeam->particles );

	pHead = pbeam->particles;
	pNew = NULL;

	div = 0;
	if ( pbeam->flags & FBEAM_STARTENTITY )
	{
		if ( pHead )
		{
			VectorSubtract( pHead->org, pbeam->source, delta );
			div = Length( delta );
			if ( div >= 32 && free_particles )
			{
				pNew = free_particles;
				free_particles = pNew->next;
			}
		}
		else
		{
			if ( free_particles )
			{
				pNew = free_particles;
				free_particles = pNew->next;
				div = 0;
			}
		}
	}

	if ( pNew )
	{
		VectorCopy( pbeam->source, pNew->org );
		if ( 1 )
			pNew->die = pbeam->amplitude + cl.time;
		else
			pNew->die = pbeam->freq + cl.time;
		VectorClear( pNew->vel );

		pbeam->die = pbeam->amplitude + cl.time;
		pNew->next = pHead;
		pbeam->particles = pNew;
		pHead = pNew;
	}

	vLast = 0;
	vStep = 1;

	if ( !pHead )
		return;

	// Transform point into screen space
	if ( !pNew && div != 0 )
	{
		VectorCopy( pbeam->source, delta );
		ScreenTransform( pbeam->source, screenLast );
		ScreenTransform( pHead->org, screen );
	}
	else if ( pHead && pHead->next )
	{
		VectorCopy( pHead->org, delta );
		ScreenTransform( pHead->org, screenLast );
		ScreenTransform( pHead->next->org, screen );
		pHead = pHead->next;
	}
	else
	{
		return;
	}

	// Build world-space normal to screen-space direction vector
	VectorSubtract( screen, screenLast, tmp );
	// We don't need Z, we're in screen space
	tmp[2] = 0;
	VectorNormalize( tmp );
	VectorScale( vup, tmp[0], normal );	// Build point along noraml line (normal is -y, x)
	VectorMA( normal, -tmp[1], vright, normal );

	// Make a wide line
	VectorMA( delta, pbeam->width, normal, last1 );
	VectorMA( delta, -pbeam->width, normal, last2 );

	div = 1.0 / pbeam->amplitude;
	fraction = ( pbeam->die - cl.time ) * div;

	while ( pHead )
	{
		tri.Brightness( fraction );
		tri.TexCoord2f( 0, 0 );
		tri.Vertex3fv( last1 );
		tri.Brightness( fraction );
		tri.TexCoord2f( 1, 0 );
		tri.Vertex3fv( last2 );

		// Transform point into screen space
		ScreenTransform( pHead->org, screen );

		// Build world-space normal to screen-space direction vector
		VectorSubtract( screen, screenLast, tmp );
		// We don't need Z, we're in screen space
		tmp[2] = 0;
		VectorNormalize( tmp );
		VectorScale( vup, tmp[0], normal );	// Build point along noraml line (normal is -y, x)
		VectorMA( normal, -tmp[1], vright, normal );

		// Make a wide line
		VectorMA( pHead->org, pbeam->width, normal, last1 );
		VectorMA( pHead->org, -pbeam->width, normal, last2 );

		vLast += vStep;	// Advance texture scroll (v axis only)

		if ( pHead->next )
			fraction = ( pHead->die - cl.time ) * div;
		else
			fraction = 0;

		tri.Brightness( fraction );
		tri.TexCoord2f( 1, 1 );
		tri.Vertex3fv( last2 );
		tri.Brightness( fraction );
		tri.TexCoord2f( 0, 1 );
		tri.Vertex3fv( last1 );

		VectorCopy( screen, screenLast );

		vLast = fmod( vLast, 1 );	// Since our texture repeats, keep vLast in [0..1)
		pHead = pHead->next;
	}

	// Drift popcorn trail if there is a velocity
	pHead = pbeam->particles;
	fraction = cl.time - cl.oldtime;
	while ( pHead )
	{
		VectorMA( pHead->org, fraction, pHead->vel, pHead->org );
		pHead = pHead->next;
	}
}

/*
==============
R_DrawRing
==============
*/
void R_DrawRing( vec_t *source, vec_t *delta, float width, float amplitude, float freq, float speed, int segments )
{
	int		i, j, noiseIndex, noiseStep;
	float	div, length, fraction, factor, vLast, vStep;
	vec3_t	last1, last2, point, screen, screenLast, tmp, normal;
	vec3_t	center, xaxis, yaxis, zaxis;
	float	radius, x, y, scale;

	if ( segments < 2 )
		return;

	segments = segments * M_PI;

	if ( segments > NOISE_DIVISIONS * 8 )	// UNDONE: Allow more segments?
		segments = NOISE_DIVISIONS * 8;

	length = Length( delta ) * 0.01 * M_PI;
	if ( length < 0.5 )	// Don't lose all of the noise/texture on short beams
		length = 0.5;
	div = 1.0 / ( segments - 1 );

	vStep = length * div / 8.0;	// Texture length texels per space pixel

	// UNDONE: Expose texture length scale factor to control "fuzziness"
	vLast = fmod( freq * speed, 1 );	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	scale = amplitude * length / 8.0;

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	noiseStep = (int)( NOISE_DIVISIONS * div * 65536.0 ) * 8;
	noiseIndex = 0;

	VectorScale( delta, 0.5, delta );
	VectorAdd( source, delta, center );

	zaxis[0] = 0;
	zaxis[1] = 0;
	zaxis[2] = 1;

	VectorCopy( delta, xaxis );
	radius = Length( xaxis );

	// cull beamring
	// --------------------------------
	// Compute box center +/- radius
	last1[0] = radius;
	last1[1] = radius;
	last1[2] = scale;
	VectorAdd( center, last1, tmp );	// maxs
	VectorSubtract( center, last1, screen );	// mins

	// Is that box in PVS && frustum?
	if ( !cl.worldmodel || !PVSNode( cl.worldmodel->nodes, screen, tmp ) || R_CullBox( screen, tmp ) )
		return;

	yaxis[0] = xaxis[1];
	yaxis[1] = -xaxis[0];
	yaxis[2] = 0;
	VectorNormalize( yaxis );
	VectorScale( yaxis, radius, yaxis );

	j = segments / 8;

	for ( i = 0; i <= segments; i++ )
	{
		fraction = i * div;
		x = sin( fraction * 2 * M_PI );
		y = cos( fraction * 2 * M_PI );

		point[0] = xaxis[0] * x + yaxis[0] * y + center[0];
		point[1] = xaxis[1] * x + yaxis[1] * y + center[1];
		point[2] = xaxis[2] * x + yaxis[2] * y + center[2];

		// Distort using noise
		factor = gNoise[ ( noiseIndex >> 16 ) & 0x7F ] * scale;
		VectorMA( point, factor, vup, point );
		factor = gNoise[ ( noiseIndex >> 16 ) & 0x7F ] * scale * cos( fraction * M_PI * 3 * 8 + freq );
		VectorMA( point, factor, vright, point );

		// Transform point into screen space
		ScreenTransform( point, screen );

		if ( i != 0 )
		{
			// Build world-space normal to screen-space direction vector
			VectorSubtract( screen, screenLast, tmp );
			// We don't need Z, we're in screen space
			tmp[2] = 0;
			VectorNormalize( tmp );
			VectorScale( vup, tmp[0], normal );	// Build point along noraml line (normal is -y, x)
			VectorMA( normal, -tmp[1], vright, normal );

			// Make a wide line
			VectorMA( point, width, normal, last1 );
			VectorMA( point, -width, normal, last2 );

			vLast += vStep;	// Advance texture scroll (v axis only)
			tri.TexCoord2f( 1, vLast );
			tri.Vertex3fv( last2 );
			tri.TexCoord2f( 0, vLast );
			tri.Vertex3fv( last1 );
		}
		VectorCopy( screen, screenLast );
		vLast = fmod( vLast, 1 );	// Since our texture repeats, keep vLast in [0..1)
		noiseIndex += noiseStep;
		j--;
		if ( j == 0 && amplitude != 0 )
		{
			j = segments / 8;
			Noise( gNoise, NOISE_DIVISIONS );
		}
	}
}

/*
==============
ParticleLine
==============
*/
void ParticleLine( float x1, float y1, float z1, float x2, float y2, float z2 )
{
	vec3_t	start, end;

	start[0] = x1;
	start[1] = y1;
	start[2] = z1;

	end[0] = x2;
	end[1] = y2;
	end[2] = z2;

	R_ShowLine( start, end );
}

/*
==============
ParticleBox
==============
*/
void ParticleBox( vec_t *mins, vec_t *maxs )
{
	// Bottom
	ParticleLine( mins[0], mins[1], mins[2], mins[0], maxs[1], mins[2] );
	ParticleLine( maxs[0], mins[1], mins[2], maxs[0], maxs[1], mins[2] );
	ParticleLine( mins[0], mins[1], mins[2], maxs[0], mins[1], mins[2] );
	ParticleLine( mins[0], maxs[1], mins[2], maxs[0], maxs[1], mins[2] );

	// Top
	ParticleLine( mins[0], mins[1], maxs[2], mins[0], maxs[1], maxs[2] );
	ParticleLine( maxs[0], mins[1], maxs[2], maxs[0], maxs[1], maxs[2] );
	ParticleLine( mins[0], mins[1], maxs[2], maxs[0], mins[1], maxs[2] );
	ParticleLine( mins[0], maxs[1], maxs[2], maxs[0], maxs[1], maxs[2] );

	// Sides
	ParticleLine( mins[0], mins[1], mins[2], mins[0], mins[1], maxs[2] );
	ParticleLine( mins[0], maxs[1], mins[2], mins[0], maxs[1], maxs[2] );
	ParticleLine( maxs[0], maxs[1], mins[2], maxs[0], maxs[1], maxs[2] );
	ParticleLine( maxs[0], mins[1], mins[2], maxs[0], mins[1], maxs[2] );
}

/*
==============
R_BeamCull

Cull the beam by bbox
==============
*/
int R_BeamCull( vec_t *start, vec_t *end, int pvsOnly )
{
	vec3_t	mins, maxs;
	int		i;

	if ( !cl.worldmodel )
		return 0;

	for ( i = 0; i < 3; i++ )
	{
		if ( start[i] < end[i] )
		{
			mins[i] = start[i];
			maxs[i] = end[i];
		}
		else
		{
			mins[i] = end[i];
			maxs[i] = start[i];
		}

		// Don't let it be zero sized
		if ( mins[i] == maxs[i] )
			maxs[i] += 1;
	}

	// Check bbox
	if ( PVSNode( cl.worldmodel->nodes, mins, maxs ) )
	{
		if ( pvsOnly || !R_CullBox( mins, maxs ) )
			return 1;	// Beam is visible
	}

	return 0;	// Beam is not visible
}

/*
==============
R_BeamDraw
==============
*/
void R_BeamDraw( BEAM *pbeam, float frametime )
{
	model_t	*sprite;
	vec3_t	difference;

	if ( pbeam->modelIndex < 0 )
	{
		pbeam->die = cl.time;
		return;
	}

	sprite = CL_GetModelByIndex( pbeam->modelIndex );
	if ( !sprite )
		return;

	if ( pbeam->flags & FBEAM_SOLID )
		tri.RenderMode( kRenderNormal );
	else
		tri.RenderMode( kRenderTransAdd );

	pbeam->freq += frametime;

	// Generate fractal noise
	gNoise[0] = 0;
	gNoise[ NOISE_DIVISIONS ] = 0;

	if ( pbeam->amplitude != 0 )
	{
		if ( pbeam->flags & FBEAM_SINENOISE )
			SineNoise( gNoise, NOISE_DIVISIONS );
		else
			Noise( gNoise, NOISE_DIVISIONS );
	}

	if ( pbeam->flags & ( FBEAM_STARTENTITY | FBEAM_ENDENTITY ) )
	{
		// Get attachment points
		if ( pbeam->flags & FBEAM_STARTENTITY )
		{
			cl_entity_t	*start;

			start = R_GetBeamAttachmentEntity( pbeam->startEntity );
			if ( !start )
				return;

			if ( start->model && ( !pbeam->pFollowModel || pbeam->pFollowModel == start->model ) )
			{
				float	*attachmentPoint;

				attachmentPoint = R_BeamGetAttachmentPoint( start, pbeam->startEntity );
				VectorCopy( attachmentPoint, pbeam->source );
				pbeam->flags |= FBEAM_STARTVISIBLE;

				if ( !pbeam->pFollowModel )
					pbeam->pFollowModel = start->model;
			}
			else
			{
				if ( !( pbeam->flags & FBEAM_FOREVER ) )
					pbeam->flags &= ~FBEAM_STARTENTITY;
			}
		}

		if ( pbeam->flags & FBEAM_ENDENTITY )
		{
			cl_entity_t	*end;

			end = R_GetBeamAttachmentEntity( pbeam->endEntity );
			if ( !end )
				return;

			if ( end->model )
			{
				float	*attachmentPoint;

				attachmentPoint = R_BeamGetAttachmentPoint( end, pbeam->endEntity );
				VectorCopy( attachmentPoint, pbeam->target );
				pbeam->flags |= FBEAM_ENDVISIBLE;
			}
			else if ( !( pbeam->flags & FBEAM_FOREVER ) )
			{
				pbeam->flags &= ~FBEAM_ENDENTITY;
				pbeam->die = cl.time;
				return;
			}
			else
			{
				return;
			}

			// If the end entity isn't visible, don't draw
			if ( !( pbeam->flags & FBEAM_ENDVISIBLE ) )
				return;
		}

		// Don't draw until we've seen the start entity
		if ( ( pbeam->flags & FBEAM_STARTENTITY ) && !( pbeam->flags & FBEAM_STARTVISIBLE ) )
			return;

		// Compute segments from the new endpoints
		VectorSubtract( pbeam->target, pbeam->source, difference );
		if ( Length( difference ) > 0.0000001 )
			VectorCopy( difference, pbeam->delta );

		if ( pbeam->amplitude >= 0.50 )
			pbeam->segments = Length( pbeam->delta ) * 0.25 + 3;	// one per 4 pixels
		else
			pbeam->segments = Length( pbeam->delta ) * 0.075 + 3;	// one per 16 pixels
	}

	// Don't draw really short or inactive beams
	if ( pbeam->type == TE_BEAMPOINTS && !R_BeamCull( pbeam->source, pbeam->target, 0 ) )
		return;

	if ( !tri.SpriteTexture( sprite, (int)( pbeam->frame + pbeam->frameRate * cl.time ) % pbeam->frameCount ) )
		return;

	if ( ( pbeam->flags & FBEAM_SINENOISE ) && egon_amplitude.value > 0 )
	{
		particle_t	*p;
		vec3_t		org, speed;
		float		length;

		VectorMA( pbeam->target, egon_amplitude.value * pbeam->amplitude * sin( pbeam->freq * 10 ), vup, org );
		VectorMA( org, egon_amplitude.value * pbeam->amplitude * cos( pbeam->freq * 10 ), vright, org );
		VectorSubtract( pbeam->source, org, speed );
		length = Length( speed );
		if ( length != 0 )
			VectorScale( speed, 1000.0 / length, speed );

		p = R_TracerParticles( org, speed, length * 0.001 );
		if ( p )
			p->color = 7;
	}

	pbeam->t = pbeam->freq + ( pbeam->die - cl.time );
	if ( pbeam->t != 0 )
		pbeam->t = 1.0 - pbeam->freq / pbeam->t;

	if ( pbeam->flags & FBEAM_FADEIN )
		tri.Color4f( pbeam->r, pbeam->g, pbeam->b, pbeam->brightness * ( pbeam->t ) );
	else if ( pbeam->flags & FBEAM_FADEOUT )
		tri.Color4f( pbeam->r, pbeam->g, pbeam->b, pbeam->brightness * ( 1.0 - pbeam->t ) );
	else
		tri.Color4f( pbeam->r, pbeam->g, pbeam->b, pbeam->brightness );

	switch ( pbeam->type )
	{
	case TE_BEAMTORUS:
		tri.Begin( TRI_QUAD_STRIP );
		R_DrawTorus( pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments );
		tri.End();
		break;

	case TE_BEAMDISK:
		tri.Begin( TRI_QUAD_STRIP );
		R_DrawDisk( pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments );
		tri.End();
		break;

	case TE_BEAMCYLINDER:
		tri.Begin( TRI_QUAD_STRIP );
		R_DrawCylinder( pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments );
		tri.End();
		break;

	case TE_BEAMPOINTS:
		tri.Begin( TRI_QUADS );
		R_DrawSegs( pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments, pbeam->flags );
		tri.End();
		break;

	case TE_BEAMFOLLOW:
		tri.Begin( TRI_QUADS );
		R_DrawBeamFollow( pbeam );
		tri.End();
		break;

	case TE_BEAMRING:
		tri.Begin( TRI_QUAD_STRIP );
		R_DrawRing( pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments );
		tri.End();
		break;
	}
}

/*
==============
R_BeamDrawList
==============
*/
void R_BeamDrawList( void )
{
	float	frametime;
	BEAM	*pbeam, *pkill;

	if ( !gpActiveBeams && !cl_numbeamentities )
		return;

	frametime = cl.time - cl.oldtime;

	qglDisable( GL_ALPHA_TEST );
	qglDepthMask( 0 );
	tri.CullFace( TRI_NONE );

	if ( gpActiveBeams )
	{
		// Free dead beams
		for ( ;; )
		{
			pkill = gpActiveBeams;
			if ( pkill && !( pkill->flags & FBEAM_FOREVER ) && pkill->die < cl.time )
			{
				gpActiveBeams = pkill->next;
				pkill->next = gpFreeBeams;
				gpFreeBeams = pkill;
				continue;
			}
			break;
		}

		for ( pbeam = gpActiveBeams; pbeam; pbeam = pbeam->next )
		{
			for ( ;; )
			{
				pkill = pbeam->next;
				if ( pkill && !( pkill->flags & FBEAM_FOREVER ) && pkill->die <= cl.time )
				{
					pbeam->next = pkill->next;
					pkill->next = gpFreeBeams;
					gpFreeBeams = pkill;
					continue;
				}
				break;
			}

			R_BeamDraw( pbeam, frametime );
		}
	}

	R_DrawBeamEntList( frametime );

	qglDepthMask( 1 );
	tri.CullFace( TRI_FRONT );
	tri.RenderMode( kRenderNormal );
}

/*
==============
R_AllocParticle
==============
*/
particle_t *R_AllocParticle( void (*callback)( struct particle_s *particle, float frametime ) )
{
	particle_t	*p;

	if ( !free_particles )
	{
		Con_Printf( "Not enough free particles\n" );
		return NULL;
	}

	p = free_particles;
	free_particles = p->next;
	p->next = active_particles;
	active_particles = p;

	p->type = pt_clientcustom;
	p->die = cl.time;
	p->callback = callback;
	p->color = 0;
	p->deathfunc = NULL;
	VectorCopy( vec3_origin, p->org );
	VectorCopy( vec3_origin, p->vel );
	p->packedColor = 0;
	p->ramp = 0;

	return p;
}

/*
==============
R_GetPackedColor
==============
*/
void R_GetPackedColor( short *packed, short color )
{
	if ( !packed )
		Con_Printf( "R_GetPackedColor called without packed!\n" );
	else
		*packed = 0;
}

/*
==============
R_LookupColor
==============
*/
short R_LookupColor( unsigned char r, unsigned char g, unsigned char b )
{
	int		i;
	int		best = -1;
	float	diff, bestdiff;
	float	rf = 0.3;
	float	gf = 0.5;
	float	bf = 0.2;

	bestdiff = 999999;

	for ( i = 0; i < 256; i++ )
	{
		diff = ( r - host_basepal[i * 4 + 2] ) * rf * ( r - host_basepal[i * 4 + 2] ) +
			( g - host_basepal[i * 4 + 1] ) * gf * ( g - host_basepal[i * 4 + 1] ) +
			( b - host_basepal[i * 4 + 0] ) * bf * ( b - host_basepal[i * 4 + 0] );

		if ( diff < bestdiff )
		{
			bestdiff = diff;
			best = i;
		}
	}

	return ( i != -1 ) ? best : 0;
}
