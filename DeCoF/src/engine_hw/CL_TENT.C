// cl_tent.c -- client side temporary entities

#include "quakedef.h"
#include <math.h>
#include "mathlib.h"
#include "cvar.h"
#include "host.h"
#include "client.h"
#include "delta.h"
#include "server.h"
#include "GL_MODEL.H"
#include "protocol.h"
#include "vid.h"
#include "render.h"
#include "GLQUAKE.H"
#include "sound.h"
#include "pmove.h"
#include "draw.h"
#include "cdll_int.h"
#include "tmessage.h"
#include "entity_types.h"
#include "r_efx.h"
#include "../pm_shared/pm_movevars.h"

#define MAX_TEMP_ENTITIES	500
#define SHARD_VOLUME		12.0	// on shard ever n^3 units

#define FDECAL_PERMANENT	0x01	// This decal should not be removed in favor of any new decals
#define FDECAL_CUSTOM		0x04	// This is a custom clan logo and should not be saved/restored

// gl_model.c
void	Mod_MarkClient( model_t *pModel );
int		ModelFrameCount( model_t *model );

// r_trans.c
void	AppendTEntity( cl_entity_t *pEnt );

// r_part.c
short	R_LookupColor( unsigned char r, unsigned char g, unsigned char b );

// pmovedbg.c
void	PM_ParticleLine( vec_t *start, vec_t *end, int pcolor, float life, float vert );
void	PM_DrawBBox( vec_t *mins, vec_t *maxs, vec_t *origin, int pcolor, float life );

// pr_cmds.c
mnode_t	*PVSNode( mnode_t *node, vec_t *emins, vec_t *emaxs );

// decals.c, gl_rsurf.c
void	*Draw_CustomCacheGet( cachewad_t *wad, void *raw, int rawsize, int index );
void	R_CustomDecalShoot( texture_t *ptexture, int playernum, int entity, int modelIndex, vec_t *position, int flags );

float	RandomFloat( float flLow, float flHigh );

extern	cvar_t		r_decals;
extern	movevars_t	movevars;
extern	vec3_t		modelorg;

// gl_rmisc.c
extern	int			filterMode;
extern	float		filterColorRed;
extern	float		filterColorGreen;
extern	float		filterColorBlue;

static TEMPENTITY	gTempEnts[ MAX_TEMP_ENTITIES ], *gpTempEntFree, *gpTempEntActive;

sfx_t		*cl_sfx_ric1;
sfx_t		*cl_sfx_ric2;
sfx_t		*cl_sfx_ric3;
sfx_t		*cl_sfx_ric4;
sfx_t		*cl_sfx_ric5;
sfx_t		*cl_sfx_r_exp1;
sfx_t		*cl_sfx_r_exp2;
sfx_t		*cl_sfx_r_exp3;
sfx_t		*cl_sfx_pl_shell1;
sfx_t		*cl_sfx_pl_shell2;
sfx_t		*cl_sfx_pl_shell3;

sfx_t		*cl_sfx_sshell1;
sfx_t		*cl_sfx_sshell2;
sfx_t		*cl_sfx_sshell3;

sfx_t		*cl_sfx_wood1;
sfx_t		*cl_sfx_wood2;
sfx_t		*cl_sfx_wood3;

sfx_t		*cl_sfx_metal1;
sfx_t		*cl_sfx_metal2;
sfx_t		*cl_sfx_metal3;

sfx_t		*cl_sfx_glass1;
sfx_t		*cl_sfx_glass2;
sfx_t		*cl_sfx_glass3;

sfx_t		*cl_sfx_concrete1;
sfx_t		*cl_sfx_concrete2;
sfx_t		*cl_sfx_concrete3;

sfx_t		*cl_sfx_flesh1;
sfx_t		*cl_sfx_flesh2;
sfx_t		*cl_sfx_flesh3;
sfx_t		*cl_sfx_flesh4;
sfx_t		*cl_sfx_flesh5;
sfx_t		*cl_sfx_flesh6;

sfx_t		*cl_sfx_geiger1;
sfx_t		*cl_sfx_geiger2;
sfx_t		*cl_sfx_geiger3;
sfx_t		*cl_sfx_geiger4;
sfx_t		*cl_sfx_geiger5;
sfx_t		*cl_sfx_geiger6;

sfx_t		*cl_sfx_imp;
sfx_t		*cl_sfx_rail;

model_t		*cl_sprite_white;
model_t		*cl_sprite_dot;
model_t		*cl_sprite_lightning;
model_t		*cl_sprite_glow;

model_t		*cl_sprite_muzzleflash[3];
model_t		*cl_sprite_ricochet;
model_t		*cl_sprite_shell;

cvar_t	tracerSpeed = { "tracerspeed", "6000" };
cvar_t	tracerOffset = { "traceroffset", "30" };
cvar_t	tracerLength = { "tracerlength", "0.8" };
cvar_t	tracerRed = { "tracerred", "0.8" };
cvar_t	tracerGreen = { "tracergreen", "0.8" };
cvar_t	tracerBlue = { "tracerblue", "0.4" };
cvar_t	tracerAlpha = { "traceralpha", "0.5" };
cvar_t	egon_amplitude = { "egon_amplitude", "0.0" };

TEMPENTITY	*CL_TempEntAlloc( vec_t *org, model_t *model );
TEMPENTITY	*CL_TempEntAllocNoModel( vec_t *org );
TEMPENTITY	*CL_TempEntAllocHigh( vec_t *org, model_t *model );
TEMPENTITY	*R_DefaultSprite( float *pos, int spriteIndex, float framerate );
void	R_ParticleLine( float *start, float *end, unsigned char r, unsigned char g, unsigned char b, float life );
void	R_ParticleBox( float *mins, float *maxs, unsigned char r, unsigned char g, unsigned char b, float life );
void	CL_ParseTextMessage( void );

/*
=================
CL_ShowTEBlood
=================
*/
qboolean CL_ShowTEBlood( void )
{
	if ( violence_hblood.value == 0 && violence_ablood.value == 0 )
		return false;

	return true;
}

/*
=================
CL_TentModel
=================
*/
model_t *CL_TentModel( const char *pName )
{
	model_t	*pModel;

	pModel = Mod_ForName( pName, true, false );
	Mod_MarkClient( pModel );
	return pModel;
}

/*
=================
CL_InitTEnts
=================
*/
void CL_InitTEnts( void )
{
	Cvar_RegisterVariable( &tracerSpeed );
	Cvar_RegisterVariable( &tracerOffset );
	Cvar_RegisterVariable( &tracerLength );
	Cvar_RegisterVariable( &tracerRed );
	Cvar_RegisterVariable( &tracerGreen );
	Cvar_RegisterVariable( &tracerBlue );
	Cvar_RegisterVariable( &tracerAlpha );

	cl_sfx_ric1 = S_PrecacheSound( "weapons/ric1.wav" );
	cl_sfx_ric2 = S_PrecacheSound( "weapons/ric2.wav" );
	cl_sfx_ric3 = S_PrecacheSound( "weapons/ric3.wav" );
	cl_sfx_ric4 = S_PrecacheSound( "weapons/ric4.wav" );
	cl_sfx_ric5 = S_PrecacheSound( "weapons/ric5.wav" );

	cl_sfx_r_exp1 = S_PrecacheSound( "weapons/explode3.wav" );
	cl_sfx_r_exp2 = S_PrecacheSound( "weapons/explode4.wav" );
	cl_sfx_r_exp3 = S_PrecacheSound( "weapons/explode5.wav" );

	cl_sfx_pl_shell1 = S_PrecacheSound( "player/pl_shell1.wav" );
	cl_sfx_pl_shell2 = S_PrecacheSound( "player/pl_shell2.wav" );
	cl_sfx_pl_shell3 = S_PrecacheSound( "player/pl_shell3.wav" );

	cl_sfx_sshell1 = S_PrecacheSound( "weapons/sshell1.wav" );
	cl_sfx_sshell2 = S_PrecacheSound( "weapons/sshell2.wav" );
	cl_sfx_sshell3 = S_PrecacheSound( "weapons/sshell3.wav" );

	cl_sfx_wood1 = S_PrecacheSound( "debris/wood1.wav" );
	cl_sfx_wood2 = S_PrecacheSound( "debris/wood2.wav" );
	cl_sfx_wood3 = S_PrecacheSound( "debris/wood3.wav" );

	cl_sfx_metal1 = S_PrecacheSound( "debris/metal1.wav" );
	cl_sfx_metal2 = S_PrecacheSound( "debris/metal2.wav" );
	cl_sfx_metal3 = S_PrecacheSound( "debris/metal3.wav" );

	cl_sfx_glass1 = S_PrecacheSound( "debris/glass1.wav" );
	cl_sfx_glass2 = S_PrecacheSound( "debris/glass2.wav" );
	cl_sfx_glass3 = S_PrecacheSound( "debris/glass3.wav" );

	cl_sfx_concrete1 = S_PrecacheSound( "debris/concrete1.wav" );
	cl_sfx_concrete2 = S_PrecacheSound( "debris/concrete2.wav" );
	cl_sfx_concrete3 = S_PrecacheSound( "debris/concrete3.wav" );

	cl_sfx_flesh1 = S_PrecacheSound( "debris/flesh1.wav" );
	cl_sfx_flesh2 = S_PrecacheSound( "debris/flesh2.wav" );
	cl_sfx_flesh3 = S_PrecacheSound( "debris/flesh3.wav" );
	cl_sfx_flesh4 = S_PrecacheSound( "debris/flesh5.wav" );
	cl_sfx_flesh5 = S_PrecacheSound( "debris/flesh6.wav" );
	cl_sfx_flesh6 = S_PrecacheSound( "debris/flesh7.wav" );

	cl_sfx_geiger1 = S_PrecacheSound( "player/geiger1.wav" );
	cl_sfx_geiger2 = S_PrecacheSound( "player/geiger2.wav" );
	cl_sfx_geiger3 = S_PrecacheSound( "player/geiger3.wav" );
	cl_sfx_geiger4 = S_PrecacheSound( "player/geiger4.wav" );
	cl_sfx_geiger5 = S_PrecacheSound( "player/geiger5.wav" );
	cl_sfx_geiger6 = S_PrecacheSound( "player/geiger6.wav" );

	cl_sprite_dot = CL_TentModel( "sprites/dot.spr" );
	cl_sprite_lightning = CL_TentModel( "sprites/lgtning.spr" );
	cl_sprite_white = CL_TentModel( "sprites/white.spr" );
	cl_sprite_glow = CL_TentModel( "sprites/animglow01.spr" );
	cl_sprite_muzzleflash[0] = CL_TentModel( "sprites/muzzleflash1.spr" );
	cl_sprite_muzzleflash[1] = CL_TentModel( "sprites/muzzleflash2.spr" );
	cl_sprite_muzzleflash[2] = CL_TentModel( "sprites/muzzleflash3.spr" );
	cl_sprite_ricochet = CL_TentModel( "sprites/richo1.spr" );
	cl_sprite_shell = CL_TentModel( "sprites/shellchrome.spr" );

	CL_TempEntInit();
}

/*
=================
R_RicochetSound

Make a random ricochet noise
=================
*/
void R_RicochetSound( vec_t *pos )
{
	switch ( RandomLong( 0, 4 ) )
	{
	case 0:
		S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_ric1, pos, VOL_NORM, 1.0, 0, PITCH_NORM );
		break;
	case 1:
		S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_ric2, pos, VOL_NORM, 1.0, 0, PITCH_NORM );
		break;
	case 2:
		S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_ric3, pos, VOL_NORM, 1.0, 0, PITCH_NORM );
		break;
	case 3:
		S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_ric4, pos, VOL_NORM, 1.0, 0, PITCH_NORM );
		break;
	case 4:
		S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_ric5, pos, VOL_NORM, 1.0, 0, PITCH_NORM );
		break;
	}
}

/*
=================
R_TracerEffect
=================
*/
void R_TracerEffect( vec_t *start, vec_t *end )
{
	vec3_t	temp, vel;
	float	len;

	if ( tracerSpeed.value <= 0 )
		tracerSpeed.value = 3;

	VectorSubtract( end, start, temp );
	len = Length( temp );

	VectorScale( temp, 1 / len, temp );
	VectorScale( temp, RandomLong( -10, 9 ) + tracerOffset.value, vel );
	VectorAdd( start, vel, start );
	VectorScale( temp, tracerSpeed.value, vel );

	efx.R_TracerParticles( start, vel, len / tracerSpeed.value );
}

/*
=================
R_FizzEffect

Create a fizz effect
=================
*/
void R_FizzEffect( cl_entity_t *pent, int modelIndex, int density )
{
	TEMPENTITY	*pTemp;
	model_t		*model;
	int			i, width, depth, count, frameCount;
	float		maxHeight, speed, xspeed, yspeed;
	vec3_t		origin;

	if ( !pent->model || !modelIndex )
		return;

	model = CL_GetModelByIndex( modelIndex );
	if ( !model )
		return;

	count = density + 1;
	density = count * 3 + 6;

	maxHeight = pent->model->maxs[2] - pent->model->mins[2];
	width = pent->model->maxs[0] - pent->model->mins[0];
	depth = pent->model->maxs[1] - pent->model->mins[1];
	speed = (float)pent->curstate.rendercolor.r * 256.0 + (float)pent->curstate.rendercolor.g;
	if ( pent->curstate.rendercolor.b )
		speed = -speed;

	xspeed = speed * cos( pent->angles[1] * M_PI / 180 );
	yspeed = speed * sin( pent->angles[1] * M_PI / 180 );

	frameCount = ModelFrameCount( model );

	for ( i = 0; i < count; i++ )
	{
		origin[0] = pent->model->mins[0] + RandomLong( 0, width - 1 );
		origin[1] = pent->model->mins[1] + RandomLong( 0, depth - 1 );
		origin[2] = pent->model->mins[2];
		pTemp = efx.CL_TempEntAlloc( origin, model );
		if ( !pTemp )
			return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];

		pTemp->entity.baseline.origin[0] = xspeed;
		pTemp->entity.baseline.origin[1] = yspeed;
		pTemp->entity.baseline.origin[2] = RandomLong( 80, 140 );

		pTemp->die = maxHeight / pTemp->entity.baseline.origin[2] + cl.time - 0.1;
		pTemp->entity.curstate.frame = RandomLong( 0, frameCount - 1 );
		// Set sprite scale
		pTemp->entity.curstate.scale = 1.0 / RandomFloat( 2, 5 );
		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
		pTemp->entity.curstate.renderamt = 255;
	}
}

/*
=================
R_Bubbles

Create bubbles
=================
*/
void R_Bubbles( vec_t *mins, vec_t *maxs, float height, int modelIndex, int count, float speed )
{
	TEMPENTITY	*pTemp;
	model_t		*model;
	int			i, frameCount;
	float		angle;
	vec3_t		origin;

	if ( !modelIndex )
		return;

	model = CL_GetModelByIndex( modelIndex );
	if ( !model )
		return;

	frameCount = ModelFrameCount( model );

	for ( i = 0; i < count; i++ )
	{
		origin[0] = RandomLong( mins[0], maxs[0] );
		origin[1] = RandomLong( mins[1], maxs[1] );
		origin[2] = RandomLong( mins[2], maxs[2] );
		pTemp = efx.CL_TempEntAlloc( origin, model );
		if ( !pTemp )
			return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];
		angle = RandomLong( -M_PI, M_PI );
		pTemp->entity.baseline.origin[0] = speed * cos( angle );
		pTemp->entity.baseline.origin[1] = speed * sin( angle );
		pTemp->entity.baseline.origin[2] = RandomLong( 80, 140 );
		pTemp->die = ( height - ( origin[2] - mins[2] ) ) / pTemp->entity.baseline.origin[2] + cl.time - 0.1;
		pTemp->entity.curstate.frame = RandomLong( 0, frameCount - 1 );

		// Set sprite scale
		pTemp->entity.curstate.scale = 1.0 / RandomFloat( 2, 5 );
		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
		pTemp->entity.curstate.renderamt = 255;
	}
}

/*
=================
R_BubbleTrail

Create bubble trail
=================
*/
void R_BubbleTrail( vec_t *start, vec_t *end, float height, int modelIndex, int count, float speed )
{
	TEMPENTITY	*pTemp;
	model_t		*model;
	int			i, frameCount;
	float		dist, angle;
	vec3_t		origin;

	if ( !modelIndex )
		return;

	model = CL_GetModelByIndex( modelIndex );
	if ( !model )
		return;

	frameCount = ModelFrameCount( model );

	for ( i = 0; i < count; i++ )
	{
		dist = RandomFloat( 0, 1.0 );
		origin[0] = start[0] + dist * ( end[0] - start[0] );
		origin[1] = start[1] + dist * ( end[1] - start[1] );
		origin[2] = start[2] + dist * ( end[2] - start[2] );
		pTemp = efx.CL_TempEntAlloc( origin, model );
		if ( !pTemp )
			return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];
		angle = RandomLong( -M_PI, M_PI );
		pTemp->entity.baseline.origin[0] = speed * cos( angle );
		pTemp->entity.baseline.origin[1] = speed * sin( angle );
		pTemp->entity.baseline.origin[2] = RandomLong( 80, 140 );
		pTemp->die = ( height - ( origin[2] - start[2] ) ) / pTemp->entity.baseline.origin[2] + cl.time - 0.1;
		pTemp->entity.curstate.frame = RandomLong( 0, frameCount - 1 );
		// Set sprite scale
		pTemp->entity.curstate.scale = 1.0 / RandomFloat( 2, 5 );
		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
		pTemp->entity.curstate.renderamt = 255;
	}
}

/*
=================
R_Projectile

Create a projectile entity
=================
*/
void R_Projectile( vec_t *origin, vec_t *velocity, int modelIndex, int life, int owner, void (*hitcallback)( struct tempent_s *ent, struct pmtrace_s *ptr ) )
{
	TEMPENTITY	*ptemp;
	model_t		*model;
	vec3_t		orientation;
	int			frameCount;

	model = CL_GetModelByIndex( modelIndex );
	if ( !model )
		return;

	ptemp = efx.CL_TempEntAllocHigh( origin, model );
	if ( !ptemp )
	{
		Con_Printf( "Couldn't allocate temp ent in R_Projectile!\n" );
		return;
	}

	if ( owner < 0 || owner > cl.maxclients )
	{
		Con_Printf( "Bad ent in R_Projectile!\n" );
		return;
	}

	ptemp->flags |= ( FTENT_COLLIDEALL | FTENT_PERSIST | FTENT_COLLIDEKILL );
	VectorCopy( velocity, ptemp->entity.baseline.origin );
	ptemp->clientIndex = owner;

	if ( ptemp->entity.model->type == mod_sprite )
	{
		frameCount = ModelFrameCount( model );
		ptemp->frameMax = frameCount;
		ptemp->flags |= FTENT_SPRANIMATE;
		if ( frameCount < 10 )
		{
			ptemp->entity.curstate.framerate = 10;
			ptemp->flags |= FTENT_SPRANIMATELOOP;
		}
		else
		{
			ptemp->entity.curstate.framerate = frameCount / life;
		}
	}
	else
	{
		ptemp->frameMax = 0;
		VectorNormalize( velocity );
		VectorAngles( velocity, orientation );
		VectorCopy( orientation, ptemp->entity.angles );
	}

	ptemp->entity.curstate.rendermode = kRenderNormal;
	ptemp->entity.curstate.renderfx = kRenderFxNone;
	ptemp->entity.curstate.renderamt = 255;
	ptemp->entity.baseline.renderamt = 255;
	ptemp->die = cl.time + life;
	ptemp->entity.curstate.frame = 0;
	ptemp->entity.curstate.body = 0;
	ptemp->entity.curstate.scale = 1.0;
	ptemp->entity.curstate.rendercolor.r = 255;
	ptemp->entity.curstate.rendercolor.g = 255;
	ptemp->entity.curstate.rendercolor.b = 255;
	ptemp->hitcallback = hitcallback;
}

/*
=================
R_Sprite_Trail

Line of moving glow sprites with gravity, fadeout, and collisions
=================
*/
void R_Sprite_Trail( int type, vec_t *start, vec_t *end, int modelIndex, int count, float life, float size, float amplitude, int renderamt, float speed )
{
	TEMPENTITY	*ptemp;
	model_t		*model;
	int			i, frameCount;
	vec3_t		delta, pos, dir;

	model = CL_GetModelByIndex( modelIndex );
	if ( !model )
		return;

	frameCount = ModelFrameCount( model );

	VectorSubtract( end, start, delta );
	VectorCopy( delta, dir );
	VectorNormalize( dir );

	amplitude /= 256.0;

	for ( i = 0; i < count; i++ )
	{
		// Be careful of divide by 0 when using 'count' here...
		if ( i == 0 )
			VectorMA( start, 0, delta, pos );
		else
			VectorMA( start, i / ( count - 1.0 ), delta, pos );

		ptemp = efx.CL_TempEntAlloc( pos, model );
		if ( !ptemp )
			return;

		ptemp->flags |= ( FTENT_COLLIDEWORLD | FTENT_SPRCYCLE | FTENT_FADEOUT | FTENT_SLOWGRAVITY );

		VectorScale( dir, speed, ptemp->entity.baseline.origin );
		ptemp->entity.baseline.origin[0] += RandomLong( -127, 128 ) * amplitude;
		ptemp->entity.baseline.origin[1] += RandomLong( -127, 128 ) * amplitude;
		ptemp->entity.baseline.origin[2] += RandomLong( -127, 128 ) * amplitude;
		ptemp->entity.curstate.scale = size;
		ptemp->entity.curstate.rendermode = kRenderGlow;
		ptemp->entity.curstate.renderfx = kRenderFxNoDissipation;
		ptemp->entity.curstate.renderamt = renderamt;
		ptemp->die = cl.time + life + RandomFloat( 0, 4 );
		ptemp->entity.curstate.frame = RandomLong( 0, frameCount - 1 );
		ptemp->frameMax = frameCount;
		ptemp->entity.baseline.renderamt = renderamt;
		ptemp->entity.curstate.rendercolor.r = 255;
		ptemp->entity.curstate.rendercolor.g = 255;
		ptemp->entity.curstate.rendercolor.b = 255;
	}
}

/*
=================
R_TempSphereModel

Spherical shower of models, picks from set
=================
*/
void R_TempSphereModel( float *pos, float speed, float life, int count, int modelIndex )
{
	int			i, frameCount;
	TEMPENTITY	*pTemp;
	model_t		*model;

	if ( !modelIndex )
		return;

	model = CL_GetModelByIndex( modelIndex );
	if ( !model )
		return;

	frameCount = ModelFrameCount( model );

	for ( i = 0; i < count; i++ )
	{
		pTemp = efx.CL_TempEntAlloc( pos, model );
		if ( !pTemp )
			return;

		pTemp->entity.curstate.body = RandomLong( 0, frameCount - 1 );

		if ( RandomLong( 0, 255 ) < 10 )
			pTemp->flags |= FTENT_SLOWGRAVITY;
		else
			pTemp->flags |= FTENT_GRAVITY;

		if ( RandomLong( 0, 255 ) < 220 )
		{
			pTemp->flags |= FTENT_ROTATE;
			pTemp->entity.baseline.angles[0] = RandomFloat( -256, -255 );
			pTemp->entity.baseline.angles[1] = RandomFloat( -256, -255 );
			pTemp->entity.baseline.angles[2] = RandomFloat( -256, -255 );
		}

		if ( RandomLong( 0, 255 ) < 100 )
			pTemp->flags |= FTENT_SMOKETRAIL;

		pTemp->flags |= FTENT_COLLIDEWORLD | FTENT_FLICKER;
		pTemp->entity.curstate.effects = i & 31;
		pTemp->entity.curstate.rendermode = kRenderNormal;
		pTemp->entity.baseline.origin[0] = RandomFloat( -1, 1 );
		pTemp->entity.baseline.origin[1] = RandomFloat( -1, 1 );
		pTemp->entity.baseline.origin[2] = RandomFloat( -1, 1 );

		VectorNormalize( pTemp->entity.baseline.origin );
		VectorScale( pTemp->entity.baseline.origin, speed, pTemp->entity.baseline.origin );

		pTemp->die = cl.time + life;
	}
}

/*
=================
R_BreakModel

Create model shattering shards
=================
*/
void R_BreakModel( float *pos, float *size, float *dir, float random, float life, int count, int modelIndex, char flags )
{
	int			i, frameCount;
	TEMPENTITY	*pTemp;
	model_t		*pModel;
	char		type;

	if ( !modelIndex )
		return;

	type = flags & BREAK_TYPEMASK;

	pModel = CL_GetModelByIndex( modelIndex );
	if ( !pModel )
		return;

	frameCount = ModelFrameCount( pModel );

	if ( count == 0 )
		// assume surface (not volume)
		count = ( size[0] * size[1] + size[1] * size[2] + size[2] * size[0] ) / ( 3 * SHARD_VOLUME * SHARD_VOLUME );

	// limit to 100 pieces
	if ( count > 100 )
		count = 100;

	for ( i = 0; i < count; i++ )
	{
		vec3_t	vecSpot;

		// fill up the box with stuff
		vecSpot[0] = pos[0] + RandomFloat( -0.5, 0.5 ) * size[0];
		vecSpot[1] = pos[1] + RandomFloat( -0.5, 0.5 ) * size[1];
		vecSpot[2] = pos[2] + RandomFloat( -0.5, 0.5 ) * size[2];

		pTemp = efx.CL_TempEntAlloc( vecSpot, pModel );
		if ( !pTemp )
			return;

		// keep track of break_type, so we know how to play sound on collision
		pTemp->hitSound = type;

		if ( pModel->type == mod_sprite )
			pTemp->entity.curstate.frame = RandomLong( 0, frameCount - 1 );
		else if ( pModel->type == mod_studio )
			pTemp->entity.curstate.body = RandomLong( 0, frameCount - 1 );

		pTemp->flags |= FTENT_COLLIDEWORLD | FTENT_FADEOUT | FTENT_SLOWGRAVITY;

		if ( RandomLong( 0, 255 ) < 200 )
		{
			pTemp->flags |= FTENT_ROTATE;
			pTemp->entity.baseline.angles[0] = RandomFloat( -256, 255 );
			pTemp->entity.baseline.angles[1] = RandomFloat( -256, 255 );
			pTemp->entity.baseline.angles[2] = RandomFloat( -256, 255 );
		}

		if ( RandomLong( 0, 255 ) < 100 && ( flags & BREAK_SMOKE ) )
			pTemp->flags |= FTENT_SMOKETRAIL;

		if ( type == BREAK_GLASS || ( flags & BREAK_TRANS ) )
		{
			pTemp->entity.curstate.rendermode = kRenderTransTexture;
			pTemp->entity.baseline.renderamt = pTemp->entity.curstate.renderamt = 128;
		}
		else
		{
			pTemp->entity.curstate.rendermode = kRenderNormal;
			pTemp->entity.baseline.renderamt = 255;		// Set this for fadeout
		}

		pTemp->entity.baseline.origin[0] = dir[0] + RandomFloat( -random, random );
		pTemp->entity.baseline.origin[1] = dir[1] + RandomFloat( -random, random );
		pTemp->entity.baseline.origin[2] = dir[2] + RandomFloat( 0, random );

		pTemp->die = cl.time + life + RandomFloat( 0, 1 );	// Add an extra 0-1 secs of life
	}
}

/*
=================
R_TempModel

Create some simple physically simulated models
=================
*/
TEMPENTITY *R_TempModel( float *pos, float *dir, float *angles, float life, int modelIndex, int soundtype )
{
	TEMPENTITY	*pTemp;
	model_t		*model;
	int			frameCount;

	if ( !modelIndex )
		return NULL;

	model = CL_GetModelByIndex( modelIndex );
	if ( !model )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return NULL;
	}

	frameCount = ModelFrameCount( model );

	pTemp = efx.CL_TempEntAlloc( pos, model );
	if ( !pTemp )
		return NULL;

	// keep track of shell type
	VectorCopy( angles, pTemp->entity.angles );
	pTemp->frameMax = frameCount;
	pTemp->flags |= ( FTENT_COLLIDEWORLD | FTENT_GRAVITY );

	switch ( soundtype )
	{
	case TE_BOUNCE_NULL:
		pTemp->hitSound = 0;
		break;
	case TE_BOUNCE_SHELL:
		pTemp->hitSound = BOUNCE_SHELL;
		pTemp->entity.baseline.angles[0] = RandomFloat( -512, 511 );
		pTemp->entity.baseline.angles[1] = RandomFloat( -256, 255 );
		pTemp->entity.baseline.angles[2] = RandomFloat( -256, 255 );
		pTemp->flags |= FTENT_ROTATE;
		break;
	case TE_BOUNCE_SHOTSHELL:
		pTemp->hitSound = BOUNCE_SHOTSHELL;
		pTemp->entity.baseline.angles[0] = RandomFloat( -512, 511 );
		pTemp->entity.baseline.angles[1] = RandomFloat( -256, 255 );
		pTemp->entity.baseline.angles[2] = RandomFloat( -256, 255 );
		pTemp->flags |= FTENT_ROTATE | FTENT_SLOWGRAVITY;
		break;
	}

	VectorCopy( dir, pTemp->entity.baseline.origin );
	pTemp->die = cl.time + life;

	// is the model a sprite?
	if ( model->type == mod_sprite )
		pTemp->entity.curstate.frame = RandomLong( 0, frameCount - 1 );
	else
		pTemp->entity.curstate.body = RandomLong( 0, frameCount - 1 );

	return pTemp;
}

/*
=================
R_SparkShower

Create a shower of sparks
=================
*/
void R_SparkShower( float *pos )
{
	TEMPENTITY	*pTemp;

	pTemp = efx.CL_TempEntAllocNoModel( pos );
	if ( !pTemp )
		return;

	pTemp->entity.baseline.origin[0] = RandomFloat( -300, 300 );
	pTemp->entity.baseline.origin[1] = RandomFloat( -300, 300 );
	pTemp->entity.baseline.origin[2] = RandomFloat( -200, 200 );
	pTemp->entity.baseline.angles[0] = 0;
	pTemp->entity.baseline.angles[1] = 0;
	pTemp->entity.baseline.angles[2] = 0;
	pTemp->flags |= FTENT_SLOWGRAVITY | FTENT_COLLIDEWORLD | FTENT_SPARKSHOWER;

	pTemp->die = cl.time + 0.5;
	pTemp->entity.curstate.framerate = RandomFloat( 0.5, 1.5 );
	pTemp->entity.curstate.scale = cl.time;
}

/*
=================
R_TempSprite

Create sprite TE
=================
*/
TEMPENTITY *R_TempSprite( float *pos, float *dir, float scale, int modelIndex, int rendermode, int renderfx, float a, float life, int flags )
{
	TEMPENTITY	*pTemp;
	model_t		*model;
	int			frameCount;

	if ( !modelIndex )
		return NULL;

	model = CL_GetModelByIndex( modelIndex );
	if ( !model )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return NULL;
	}

	frameCount = ModelFrameCount( model );

	pTemp = efx.CL_TempEntAlloc( pos, model );
	if ( !pTemp )
		return NULL;

	pTemp->frameMax = frameCount;
	pTemp->entity.curstate.framerate = 10;
	pTemp->entity.curstate.rendermode = rendermode;
	pTemp->entity.curstate.renderfx = renderfx;
	pTemp->entity.curstate.renderamt = a * 255;
	pTemp->entity.curstate.scale = scale;
	pTemp->entity.baseline.renderamt = a * 255;
	pTemp->entity.curstate.rendercolor.r = 255;
	pTemp->entity.curstate.rendercolor.g = 255;
	pTemp->entity.curstate.rendercolor.b = 255;
	pTemp->flags |= flags;

	VectorCopy( dir, pTemp->entity.baseline.origin );
	VectorCopy( pos, pTemp->entity.origin );
	if ( life )
		pTemp->die = cl.time + life;
	else
		pTemp->die = cl.time + ( frameCount * 0.1 ) + 1;

	pTemp->entity.curstate.frame = 0;
	return pTemp;
}

/*
=================
R_Sprite_Spray

Spray of alpha sprites
=================
*/
void R_Sprite_Spray( vec_t *pos, vec_t *dir, int modelIndex, int count, int speed, int iRand )
{
	TEMPENTITY	*pTemp;
	model_t		*pModel;
	float		noise;
	float		znoise;
	int			frameCount;
	int			i;

	noise = (float)iRand / 100;

	// more vertical displacement
	znoise = noise * 1.5;

	if ( znoise > 1 )
		znoise = 1;

	pModel = CL_GetModelByIndex( modelIndex );
	if ( !pModel )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return;
	}

	frameCount = ModelFrameCount( pModel ) - 1;

	for ( i = 0; i < count; i++ )
	{
		pTemp = efx.CL_TempEntAlloc( pos, pModel );
		if ( !pTemp )
			return;

		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
		pTemp->entity.baseline.renderamt = pTemp->entity.curstate.renderamt = 255;
		pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
		pTemp->entity.curstate.framerate = 0.5;
		pTemp->flags |= FTENT_FADEOUT | FTENT_SLOWGRAVITY;
		pTemp->fadeSpeed = 2.0;

		// make the spittle fly the direction indicated, but mix in some noise.
		pTemp->entity.baseline.origin[0] = dir[0] + RandomFloat( -noise, noise );
		pTemp->entity.baseline.origin[1] = dir[1] + RandomFloat( -noise, noise );
		pTemp->entity.baseline.origin[2] = dir[2] + RandomFloat( 0, znoise );
		VectorScale( pTemp->entity.baseline.origin, RandomFloat( ( speed * 0.8 ), ( speed * 1.2 ) ), pTemp->entity.baseline.origin );

		VectorCopy( pos, pTemp->entity.origin );

		pTemp->die = cl.time + 0.35;

		pTemp->entity.curstate.frame = RandomLong( 0, frameCount );
	}
}

/*
=================
R_Spray

Spray of opaque sprites
=================
*/
void R_Spray( vec_t *pos, vec_t *dir, int modelIndex, int count, int speed, int spread, int rendermode )
{
	TEMPENTITY	*pTemp;
	model_t		*pModel;
	float		noise;
	float		znoise;
	int			frameCount;
	int			i;

	noise = (float)spread / 100;

	// more vertical displacement
	znoise = noise * 1.5;

	if ( znoise > 1 )
		znoise = 1;

	pModel = CL_GetModelByIndex( modelIndex );
	if ( !pModel )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return;
	}

	frameCount = ModelFrameCount( pModel ) - 1;

	for ( i = 0; i < count; i++ )
	{
		pTemp = efx.CL_TempEntAlloc( pos, pModel );
		if ( !pTemp )
			return;

		pTemp->entity.curstate.rendermode = rendermode;
		pTemp->entity.baseline.renderamt = pTemp->entity.curstate.renderamt = 255;
		pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
		pTemp->entity.curstate.framerate = 1;
		pTemp->flags |= FTENT_COLLIDEWORLD | FTENT_SLOWGRAVITY;

		if ( frameCount > 1 )
		{
			pTemp->flags |= FTENT_SPRANIMATE;
			pTemp->entity.curstate.framerate = 10;
			pTemp->die = cl.time + ( frameCount * 0.1 );
		}
		else
		{
			pTemp->die = cl.time + 0.35;
		}

		// make the spittle fly the direction indicated, but mix in some noise.
		pTemp->entity.baseline.origin[0] = dir[0] + RandomFloat( -noise, noise );
		pTemp->entity.baseline.origin[1] = dir[1] + RandomFloat( -noise, noise );
		pTemp->entity.baseline.origin[2] = dir[2] + RandomFloat( 0, znoise );
		VectorScale( pTemp->entity.baseline.origin, RandomFloat( ( speed * 0.8 ), ( speed * 1.2 ) ), pTemp->entity.baseline.origin );

		VectorCopy( pos, pTemp->entity.origin );

		pTemp->entity.curstate.frame = 0;
		pTemp->frameMax = frameCount;
	}
}

/*
=================
R_PlayerSprites

Create a particle smoke around player
=================
*/
void R_PlayerSprites( int client, int modelIndex, int count, int size )
{
	TEMPENTITY	*pTemp;
	model_t		*pModel;
	cl_entity_t	*pent;
	int			frameCount;
	int			i;
	float		flMinScale;
	float		flMaxScale;
	vec3_t		position;
	vec3_t		dir;

	flMinScale = 1.0 - ( size / 100.0 );
	flMaxScale = 1.0;

	if ( client < 1 || client > cl.maxclients )
	{
		Con_Printf( "Bad ent in R_PlayerSprites!\n" );
		return;
	}

	pent = &cl_entities[ client ];

	pModel = CL_GetModelByIndex( modelIndex );
	if ( !pModel )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return;
	}

	frameCount = ModelFrameCount( pModel ) - 1;

	for ( i = 0; i < count; i++ )
	{
		position[0] = pent->origin[0] + RandomFloat( -10, 10 );
		position[1] = pent->origin[1] + RandomFloat( -10, 10 );
		position[2] = pent->origin[2] + RandomFloat( -20, 36 );

		pTemp = efx.CL_TempEntAlloc( position, pModel );
		if ( !pTemp )
			return;

		VectorSubtract( pTemp->entity.origin, pent->origin, pTemp->tentOffset );

		if ( i == 0 )
		{
			VectorSubtract( position, pent->origin, dir );
			VectorNormalize( dir );
			VectorScale( dir, 60, dir );
			pTemp->entity.baseline.origin[0] = dir[0];
			pTemp->entity.baseline.origin[1] = dir[1];
			pTemp->entity.baseline.origin[2] = RandomFloat( 20, 60 );
		}
		else
		{
			pTemp->flags |= FTENT_PLYRATTACHMENT;
			pTemp->clientIndex = client;
		}

		pTemp->entity.curstate.rendermode = kRenderNormal;
		pTemp->entity.baseline.renderamt = pTemp->entity.curstate.renderamt = 255;
		pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
		pTemp->entity.curstate.framerate = RandomFloat( flMinScale, flMaxScale );

		if ( frameCount > 1 )
		{
			pTemp->flags |= FTENT_SPRANIMATE;
			pTemp->entity.curstate.framerate = 20;
			pTemp->die = cl.time + ( frameCount * 0.05 );
		}
		else
		{
			pTemp->die = cl.time + 0.35;
		}

		pTemp->entity.curstate.frame = 0;
		pTemp->frameMax = frameCount;
	}
}

/*
=================
R_FireField

Makes a field of fire
=================
*/
void R_FireField( vec_t *org, int radius, int modelIndex, int count, int flags, float life )
{
	TEMPENTITY	*pTemp;
	model_t		*pModel;
	int			frameCount;
	int			i;
	float		time;
	vec3_t		position;

	pModel = CL_GetModelByIndex( modelIndex );
	if ( !pModel )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return;
	}

	frameCount = ModelFrameCount( pModel ) - 1;

	for ( i = 0; i < count; i++ )
	{
		position[0] = org[0] + RandomFloat( -radius, radius );
		position[1] = org[1] + RandomFloat( -radius, radius );

		if ( flags & TEFIRE_FLAG_PLANAR )
			position[2] = org[2];
		else
			position[2] = org[2] + RandomFloat( -radius, radius );

		pTemp = efx.CL_TempEntAlloc( position, pModel );
		if ( !pTemp )
			return;

		if ( flags & TEFIRE_FLAG_ALPHA )
		{
			pTemp->entity.curstate.rendermode = kRenderTransAlpha;
			pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
			pTemp->entity.baseline.renderamt = pTemp->entity.curstate.renderamt = 128;
		}
		else if ( flags & TEFIRE_FLAG_ADDITIVE )
		{
			pTemp->entity.curstate.rendermode = kRenderTransAdd;
			pTemp->entity.curstate.renderamt = 180;
		}
		else
		{
			pTemp->entity.curstate.rendermode = kRenderNormal;
			pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
			pTemp->entity.baseline.renderamt = pTemp->entity.curstate.renderamt = 255;
		}

		pTemp->entity.curstate.framerate = RandomFloat( 0.75, 1.25 );
		time = life + RandomFloat( -0.25, 0.5 );
		pTemp->die = cl.time + time;

		if ( frameCount > 1 )
		{
			pTemp->flags |= FTENT_SPRANIMATE;

			if ( flags & TEFIRE_FLAG_LOOP )
			{
				pTemp->entity.curstate.framerate = 15;
				pTemp->flags |= FTENT_SPRANIMATELOOP;
			}
			else
			{
				pTemp->entity.curstate.framerate = frameCount / time;
			}
		}

		pTemp->entity.curstate.frame = 0;
		pTemp->frameMax = frameCount;

		pTemp->entity.baseline.origin[0] = 0;
		pTemp->entity.baseline.origin[1] = 0;
		pTemp->entity.baseline.origin[2] = 0;

		if ( ( flags & TEFIRE_FLAG_ALLFLOAT ) || ( ( flags & TEFIRE_FLAG_SOMEFLOAT ) && !RandomLong( 0, 1 ) ) )
		{
			// drift sprite upward
			pTemp->entity.baseline.origin[2] = RandomFloat( 10, 30 );
		}
	}
}

/*
=================
R_AttachTentToPlayer2

Attaches entity to player
=================
*/
void R_AttachTentToPlayer2( int client, struct model_s *pModel, float zoffset, float life )
{
	TEMPENTITY	*pTemp;
	cl_entity_t	*pent;
	vec3_t		position;
	int			frameCount;

	if ( client < 0 || client > cl.maxclients )
	{
		Con_Printf( "Bad client in R_AttachTentToPlayer()!\n" );
		return;
	}

	pent = &cl_entities[ client ];
	if ( pent->curstate.messagenum != cl.parsecount )
		return;

	position[0] = pent->origin[0];
	position[1] = pent->origin[1];
	position[2] = zoffset + pent->origin[2];

	pTemp = efx.CL_TempEntAllocHigh( position, pModel );
	if ( !pTemp )
	{
		Con_Printf( "No temp ent.\n" );
		return;
	}

	pTemp->entity.curstate.rendermode = kRenderNormal;
	pTemp->entity.baseline.renderamt = pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
	pTemp->entity.curstate.framerate = 1;

	pTemp->clientIndex = client;
	pTemp->tentOffset[0] = 0;
	pTemp->tentOffset[1] = 0;
	pTemp->tentOffset[2] = zoffset;
	pTemp->die = cl.time + life;
	pTemp->flags |= FTENT_PLYRATTACHMENT | FTENT_PERSIST;

	// is the model a sprite?
	if ( pTemp->entity.model->type == mod_sprite )
	{
		frameCount = ModelFrameCount( pModel );
		pTemp->frameMax = frameCount;
		pTemp->flags |= FTENT_SPRANIMATE | FTENT_SPRANIMATELOOP;
		pTemp->entity.curstate.framerate = 10;
	}
	else
	{
		// no animation support for attached clientside studio models.
		pTemp->frameMax = 0;
	}

	pTemp->entity.curstate.frame = 0;
}

/*
=================
R_AttachTentToPlayer

Attaches entity to player
=================
*/
void R_AttachTentToPlayer( int client, int modelIndex, float zoffset, float life )
{
	model_t	*pModel;

	pModel = CL_GetModelByIndex( modelIndex );
	if ( !pModel )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return;
	}

	R_AttachTentToPlayer2( client, pModel, zoffset, life );
}

/*
=================
R_KillAttachedTents

Detach entity from player
=================
*/
void R_KillAttachedTents( int client )
{
	TEMPENTITY	*pTemp;

	if ( client < 0 || client > cl.maxclients )
	{
		Con_Printf( "Bad client in R_KillAttachedTents()!\n" );
		return;
	}

	pTemp = gpTempEntActive;
	while ( pTemp )
	{
		if ( pTemp->flags & FTENT_PLYRATTACHMENT )
		{
			// this TENT is player attached.
			// if it is attached to this client, set it to die instantly.
			if ( pTemp->clientIndex == client )
			{
				pTemp->die = cl.time;	// good enough, it will die on next tent update.
			}
		}

		pTemp = pTemp->next;
	}
}

/*
=================
R_SparkEffect

Create a streak of sparks and a ricochet sprite
=================
*/
void R_SparkEffect( float *pos, int count, int velocityMin, int velocityMax )
{
	efx.R_SparkStreaks( pos, count, velocityMin, velocityMax );
	efx.R_RicochetSprite( pos, cl_sprite_ricochet, 0.1, RandomFloat( 0.5, 1.0 ) );
}

/*
=================
R_FunnelSprite

Create a funnel of sprites
=================
*/
void R_FunnelSprite( float *org, int modelIndex, int reverse )
{
	TEMPENTITY	*pTemp;
	int			i, j;
	float		flDist;
	float		vel;
	vec3_t		dir;
	vec3_t		dest;
	model_t		*model;
	int			frameCount;

	if ( !modelIndex )
	{
		Con_Printf( "No modelindex for funnel!!\n" );
		return;
	}

	model = CL_GetModelByIndex( modelIndex );
	if ( !model )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return;
	}

	frameCount = ModelFrameCount( model );

	for ( i = -256; i <= 256; i += 64 )
	{
		for ( j = -256; j <= 256; j += 64 )
		{
			pTemp = efx.CL_TempEntAlloc( org, model );
			if ( !pTemp )
				return;

			if ( reverse )
			{
				VectorCopy( org, pTemp->entity.origin );

				dest[0] = org[0] + i;
				dest[1] = org[1] + j;
				dest[2] = org[2] + RandomFloat( 100, 800 );

				// send particle heading to org at a random speed
				VectorSubtract( dest, pTemp->entity.origin, dir );

				// velocity based on how far particle has to travel away from org
				vel = dest[2] / 8;
			}
			else
			{
				pTemp->entity.origin[0] = org[0] + i;
				pTemp->entity.origin[1] = org[1] + j;
				pTemp->entity.origin[2] = org[2] + RandomFloat( 100, 800 );

				// send particle heading to org at a random speed
				VectorSubtract( org, pTemp->entity.origin, dir );

				// velocity based on how far particle starts from org
				vel = pTemp->entity.origin[2] / 8;
			}

			pTemp->frameMax = frameCount;
			pTemp->entity.curstate.framerate = 10;
			pTemp->entity.curstate.rendermode = kRenderGlow;
			pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
			pTemp->entity.baseline.renderamt = pTemp->entity.curstate.renderamt = 200;
			pTemp->entity.curstate.framerate = RandomFloat( 0.1, 0.4 );
			pTemp->flags = FTENT_ROTATE | FTENT_FADEOUT;

			flDist = VectorNormalize( dir );	// normalize vector

			if ( vel < 64 )
				vel = 64;

			vel += RandomFloat( 64, 128 );
			VectorScale( dir, vel, pTemp->entity.baseline.origin );
			pTemp->fadeSpeed = 2.0;
			pTemp->die = ( flDist / vel ) - 0.5 + cl.time;
		}
	}
}

/*
=================
R_RicochetSprite

Create ricochet sprite
=================
*/
void R_RicochetSprite( float *pos, model_t *pmodel, float duration, float scale )
{
	TEMPENTITY	*pTemp;

	pTemp = efx.CL_TempEntAlloc( pos, pmodel );
	if ( !pTemp )
		return;

	pTemp->entity.curstate.rendermode = kRenderGlow;
	pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
	pTemp->entity.baseline.renderamt = pTemp->entity.curstate.renderamt = 200;
	pTemp->entity.curstate.scale = scale;
	pTemp->flags = FTENT_FADEOUT;

	VectorClear( pTemp->entity.baseline.origin );

	VectorCopy( pos, pTemp->entity.origin );

	pTemp->fadeSpeed = 8;
	pTemp->die = cl.time;

	pTemp->entity.curstate.frame = 0;
	pTemp->entity.angles[2] = 45 * RandomLong( 0, 7 );
}

/*
=================
R_RocketFlare

Create a rocket flare
=================
*/
void R_RocketFlare( float *pos )
{
	TEMPENTITY	*pTemp;
	int			frameCount;

	if ( cl.time == cl.oldtime )
		return;

	frameCount = ModelFrameCount( cl_sprite_glow );
	pTemp = efx.CL_TempEntAlloc( pos, cl_sprite_glow );
	if ( !pTemp )
		return;

	pTemp->frameMax = frameCount;
	pTemp->entity.curstate.rendermode = kRenderGlow;
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
	pTemp->entity.curstate.framerate = 1.0;
	VectorCopy( pos, pTemp->entity.origin );
	pTemp->die = cl.time + 0.01;
	pTemp->entity.curstate.frame = RandomLong( 0, frameCount - 1 );
}

/*
=================
R_BloodSprite

Create a blood sprite
=================
*/
void R_BloodSprite( vec_t *org, int colorindex, int modelIndex, int modelIndex2, float size )
{
	int			i, splatter;
	TEMPENTITY	*pTemp;
	model_t		*model, *model2;
	int			frameCount, frameCount2;
	unsigned int	impactindex, spatterindex;
	color24		impactcolor, splattercolor;

	// Large, single blood sprite is a high-priority tent
	impactindex = colorindex + RandomLong( 1, 3 );
	spatterindex = impactindex - 1;

	impactcolor.r = host_basepal[impactindex * 4 + 2];
	impactcolor.g = host_basepal[impactindex * 4 + 1];
	impactcolor.b = host_basepal[impactindex * 4 + 0];

	splattercolor.r = host_basepal[spatterindex * 4 + 2];
	splattercolor.g = host_basepal[spatterindex * 4 + 1];
	splattercolor.b = host_basepal[spatterindex * 4 + 0];

	// Spray of blood with model2
	if ( modelIndex2 )
	{
		model2 = CL_GetModelByIndex( modelIndex2 );
		if ( model2 )
		{
			frameCount2 = ModelFrameCount( model2 );

			// Create a bunch of droplets of blood
			splatter = size + ( RandomLong( 1, 8 ) + RandomLong( 1, 8 ) );

			for ( i = 0; i < splatter; i++ )
			{
				pTemp = efx.CL_TempEntAlloc( org, model2 );
				if ( pTemp )
				{
					pTemp->entity.curstate.rendermode = kRenderNormal;
					pTemp->entity.curstate.renderfx = kRenderFxClampMinScale;
					pTemp->entity.baseline.renderamt = 250;
					pTemp->entity.curstate.renderamt = 250;
					pTemp->entity.curstate.scale = RandomFloat( size / 25, size / 35 );
					pTemp->flags = FTENT_ROTATE | FTENT_SLOWGRAVITY | FTENT_COLLIDEWORLD;
					pTemp->entity.curstate.rendercolor = splattercolor;

					pTemp->entity.baseline.origin[0] = RandomFloat( -96, 95 );
					pTemp->entity.baseline.origin[1] = RandomFloat( -96, 95 );
					pTemp->entity.baseline.origin[2] = RandomFloat( -32, 95 );

					pTemp->entity.baseline.angles[0] = RandomFloat( -256, -255 );
					pTemp->entity.baseline.angles[1] = RandomFloat( -256, -255 );
					pTemp->entity.baseline.angles[2] = RandomFloat( -256, -255 );

					pTemp->entity.curstate.framerate = 0;
					pTemp->die = cl.time + RandomFloat( 1, 2 );
					pTemp->entity.curstate.frame = RandomLong( 1, frameCount2 - 1 );
					if ( pTemp->entity.curstate.frame > 8 )
						pTemp->entity.curstate.frame = frameCount2 - 1;

					pTemp->entity.angles[2] = RandomLong( 0, 360 );
					pTemp->bounceFactor = 0;
				}
			}
		}
	}

	// Impact sprite with model
	if ( modelIndex )
	{
		model = CL_GetModelByIndex( modelIndex );
		if ( model )
		{
			frameCount = ModelFrameCount( model );

			pTemp = efx.CL_TempEntAllocHigh( org, model );
			if ( pTemp )
			{
				pTemp->entity.curstate.rendermode = kRenderNormal;
				pTemp->entity.curstate.renderfx = kRenderFxClampMinScale;
				pTemp->entity.baseline.renderamt = 250;
				pTemp->entity.curstate.renderamt = 250;
				pTemp->entity.curstate.scale = RandomFloat( size / 35, size / 25 );
				pTemp->flags = FTENT_SPRANIMATE;
				pTemp->entity.curstate.rendercolor = impactcolor;

				pTemp->entity.baseline.origin[0] = 0;
				pTemp->entity.baseline.origin[1] = 0;
				pTemp->entity.baseline.origin[2] = 0;

				// Slow down the animation
				pTemp->entity.curstate.framerate = frameCount * 4;
				pTemp->die = cl.time + ( frameCount / pTemp->entity.curstate.framerate );

				pTemp->entity.curstate.frame = 0;
				pTemp->frameMax = frameCount;
				pTemp->entity.angles[2] = RandomLong( 0, 360 );
				pTemp->bounceFactor = 0;
			}
		}
	}
}

/*
=================
R_DefaultSprite

Create default sprite TE
=================
*/
TEMPENTITY *R_DefaultSprite( float *pos, int spriteIndex, float framerate )
{
	TEMPENTITY	*pTemp;
	int			frameCount;
	model_t		*pSprite;

	// don't spawn while paused
	if ( cl.time == cl.oldtime )
		return NULL;

	pSprite = CL_GetModelByIndex( spriteIndex );
	if ( !spriteIndex || !pSprite || pSprite->type != mod_sprite )
	{
		Con_DPrintf( "No Sprite %d!\n", spriteIndex );
		return NULL;
	}

	frameCount = ModelFrameCount( pSprite );

	pTemp = efx.CL_TempEntAlloc( pos, pSprite );
	if ( !pTemp )
		return NULL;

	pTemp->frameMax = frameCount;
	pTemp->entity.curstate.framerate = 1.0;
	pTemp->flags |= FTENT_SPRANIMATE;
	if ( framerate == 0 )
		framerate = 10;

	pTemp->entity.curstate.framerate = framerate;
	pTemp->die = cl.time + (float)frameCount / framerate;
	pTemp->entity.curstate.frame = 0;
	VectorCopy( pos, pTemp->entity.origin );

	return pTemp;
}

/*
=================
R_Sprite_Smoke

Create sprite smoke
=================
*/
void R_Sprite_Smoke( TEMPENTITY *pTemp, float scale )
{
	if ( !pTemp )
		return;

	pTemp->entity.curstate.rendermode = kRenderTransAlpha;
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.baseline.origin[2] = 30;
	pTemp->entity.curstate.rendercolor.r = RandomLong( 20, 35 );
	pTemp->entity.curstate.rendercolor.g = pTemp->entity.curstate.rendercolor.r;
	pTemp->entity.curstate.rendercolor.b = pTemp->entity.curstate.rendercolor.r;
	pTemp->entity.origin[2] += 20;
	pTemp->entity.curstate.scale = scale;
}

/*
=================
R_Sprite_Explode

Create explosion sprite
=================
*/
void R_Sprite_Explode( TEMPENTITY *pTemp, float scale, int flags )
{
	if ( !pTemp )
		return;

	if ( flags & TE_EXPLFLAG_NOADDITIVE )
	{
		// solid sprite
		pTemp->entity.curstate.rendermode = kRenderNormal;
		pTemp->entity.curstate.renderamt = 255;
	}
	else
	{
		// additive sprite
		pTemp->entity.curstate.rendermode = kRenderTransAdd;
		pTemp->entity.curstate.renderamt = 180;
	}

	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.baseline.origin[2] = 8;
	pTemp->entity.curstate.rendercolor.r = 0;
	pTemp->entity.curstate.rendercolor.g = 0;
	pTemp->entity.curstate.rendercolor.b = 0;
	pTemp->entity.origin[2] += 10;
	pTemp->entity.curstate.scale = scale;
}

/*
=================
R_Sprite_WallPuff

Create a wallpuff
=================
*/
void R_Sprite_WallPuff( TEMPENTITY *pTemp, float scale )
{
	if ( !pTemp )
		return;

	pTemp->entity.curstate.rendermode = kRenderTransAlpha;
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.curstate.rendercolor.r = 0;
	pTemp->entity.curstate.rendercolor.g = 0;
	pTemp->entity.curstate.rendercolor.b = 0;
	pTemp->entity.curstate.scale = scale;
	pTemp->die = cl.time + 0.01;
	pTemp->entity.angles[2] = RandomLong( 0, 359 );
	pTemp->entity.curstate.frame = 0;
}

/*
=================
R_MultiGunshot

Client version of shotgun shot
=================
*/
void R_MultiGunshot( vec_t *org, vec_t *dir, vec_t *noise, int count, int decalCount, int *decalIndices )
{
	int			i;
	float		x, y, z;
	float		temp;
	vec3_t		vecDir, vecEnd, vecSpread1, vecSpread2, vecSpread;
	vec3_t		vecUp, vecRight;
	vec3_t		angles;
	pmtrace_t	pmtrace;
	int			iDecal;

	VectorAngles( dir, angles );
	AngleVectors( angles, NULL, vecRight, vecUp );

	for ( i = 0; i < count; i++ )
	{
		// get circular gaussian spread
		do
		{
			x = RandomFloat( -0.5, 0.5 ) + RandomFloat( -0.5, 0.5 );
			y = RandomFloat( -0.5, 0.5 ) + RandomFloat( -0.5, 0.5 );
			z = x * x + y * y;
		} while ( z > 1 );

		iDecal = RandomLong( 0, decalCount - 1 );

		temp = x * noise[0];
		VectorScale( vecRight, temp, vecSpread1 );
		temp = y * noise[1];
		VectorScale( vecUp, temp, vecSpread2 );
		VectorAdd( vecSpread1, vecSpread2, vecSpread );
		VectorAdd( vecSpread, dir, vecDir );

		VectorScale( vecDir, 4096, vecEnd );
		VectorAdd( vecEnd, org, vecEnd );

		pmove->usehull = 2;
		pmtrace = PM_PlayerTrace( org, vecEnd, PM_STUDIO_IGNORE, -1 );

		// Hit something?
		if ( pmtrace.fraction != 1.0 )
		{
			if ( i & 2 )
				efx.R_RicochetSound( pmtrace.endpos );

			efx.R_BulletImpactParticles( pmtrace.endpos );

			if ( r_decals.value )
				efx.R_DecalShoot( efx.Draw_DecalIndex( decalIndices[ iDecal ] ), pmove->physents[ pmtrace.ent ].info, 0, pmtrace.endpos, 0 );
		}
	}
}

/*
=================
R_MuzzleFlash

Do a muzzleflash
=================
*/
void R_MuzzleFlash( float *pos1, int type )
{
	TEMPENTITY	*pTemp;
	int			index;
	float		scale;
	int			frameCount;

	index = type % 10;
	index = index % 3;
	scale = ( type / 10 ) * 0.1;
	if ( scale == 0 )
		scale = 0.5;

	frameCount = ModelFrameCount( cl_sprite_muzzleflash[ index ] );

	// must set position for right culling on render
	pTemp = efx.CL_TempEntAlloc( pos1, cl_sprite_muzzleflash[ index ] );
	if ( !pTemp )
		return;

	pTemp->entity.curstate.rendermode = kRenderTransAdd;
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.curstate.rendercolor.r = 255;
	pTemp->entity.curstate.rendercolor.g = 255;
	pTemp->entity.curstate.rendercolor.b = 255;
	pTemp->entity.curstate.scale = scale;
	VectorCopy( pos1, pTemp->entity.origin );
	pTemp->die = cl.time + 0.01;
	pTemp->entity.curstate.frame = RandomLong( 0, frameCount - 1 );
	pTemp->frameMax = frameCount;
	VectorClear( pTemp->entity.angles );

	if ( index == 0 )
	{
		// Rifle flash
		pTemp->entity.angles[2] = RandomLong( 0, 20 );
	}
	else
	{
		pTemp->entity.angles[2] = RandomLong( 0, 359 );
	}

	AppendTEntity( &pTemp->entity );
}

/*
=================
R_ParticleLine

Draw a line of particles
=================
*/
void R_ParticleLine( float *start, float *end, unsigned char r, unsigned char g, unsigned char b, float life )
{
	int		pcolor;

	pcolor = R_LookupColor( r, g, b );
	PM_ParticleLine( start, end, pcolor, life, 0 );
}

/*
=================
R_ParticleBox

Draw a box of particles
=================
*/
void R_ParticleBox( float *mins, float *maxs, unsigned char r, unsigned char g, unsigned char b, float life )
{
	int		i;
	int		pcolor;
	vec3_t	mmin, mmax;
	vec3_t	origin;

	pcolor = R_LookupColor( r, g, b );

	for ( i = 0; i < 3; i++ )
	{
		origin[i] = ( maxs[i] + mins[i] ) / 2.0;
		mmax[i] = maxs[i] - origin[i];
		mmin[i] = mins[i] - origin[i];
	}

	PM_DrawBBox( mmin, mmax, origin, pcolor, life );
}

/*
=================
CL_ParseTextMessage
=================
*/
void CL_ParseTextMessage( void )
{
	int		channel;
	client_textmessage_t	*pNetMessage;

	channel = MSG_ReadByte() % MAX_NETMESSAGE;
	pNetMessage = &gNetworkTextMessage[ channel ];

	pNetMessage->x = MSG_ReadShort() * ( 1.0 / 8192 );
	pNetMessage->y = MSG_ReadShort() * ( 1.0 / 8192 );
	pNetMessage->effect = MSG_ReadByte();
	pNetMessage->r1 = MSG_ReadByte();
	pNetMessage->g1 = MSG_ReadByte();
	pNetMessage->b1 = MSG_ReadByte();
	pNetMessage->a1 = MSG_ReadByte();
	pNetMessage->r2 = MSG_ReadByte();
	pNetMessage->g2 = MSG_ReadByte();
	pNetMessage->b2 = MSG_ReadByte();
	pNetMessage->a2 = MSG_ReadByte();
	pNetMessage->fadein = MSG_ReadWord() * ( 1.0 / 256 );
	pNetMessage->fadeout = MSG_ReadWord() * ( 1.0 / 256 );
	pNetMessage->holdtime = MSG_ReadWord() * ( 1.0 / 256 );

	if ( pNetMessage->effect == 2 )
		pNetMessage->fxtime = MSG_ReadWord() * ( 1.0 / 256 );

	pNetMessage->pName = gNetworkMessageNames[ channel ];
	pNetMessage->pMessage = gNetworkTextMessageBuffer[ channel ];

	Q_strncpy( gNetworkTextMessageBuffer[ channel ], MSG_ReadString(), 511 );
	gNetworkTextMessageBuffer[ channel ][ 511 ] = 0;

	CL_HudMessage( pNetMessage->pName );
}

/*
=================
CL_ParseTEnt

Parse a temporary entity message
=================
*/
void CL_ParseTEnt( void )
{
	char		c;
	int			type;
	int			soundtype;
	int			speed;
	int			color;
	int			iRand;
	int			entnumber;
	int			modelIndex, modelIndex2;
	vec3_t		pos, size, dir;
	vec3_t		endpos;
	dlight_t	*dl;
	int			colorStart, colorLength;
	int			startEnt, endEnt, startFrame, count;
	float		life, r, g, b, a, frameRate;
	int			flags;
	int			index_array[1];
	unsigned char	br, bg, bb;

	type = MSG_ReadByte();
	switch ( type )
	{
	case TE_LINE:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		endpos[0] = MSG_ReadCoord( &net_message );
		endpos[1] = MSG_ReadCoord( &net_message );
		endpos[2] = MSG_ReadCoord( &net_message );

		size[0] = MSG_ReadShort() * 0.1;
		br = MSG_ReadByte();
		bg = MSG_ReadByte();
		bb = MSG_ReadByte();
		R_ParticleLine( pos, endpos, br, bg, bb, size[0] );
		break;

	case TE_BOX:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		endpos[0] = MSG_ReadCoord( &net_message );
		endpos[1] = MSG_ReadCoord( &net_message );
		endpos[2] = MSG_ReadCoord( &net_message );

		size[0] = MSG_ReadShort() * 0.1;
		br = MSG_ReadByte();
		bg = MSG_ReadByte();
		bb = MSG_ReadByte();
		R_ParticleBox( pos, endpos, br, bg, bb, size[0] );
		break;

	case TE_ARMOR_RICOCHET:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		size[0] = MSG_ReadByte() * 0.1;
		efx.R_RicochetSprite( pos, cl_sprite_ricochet, 0.1, size[0] );
		efx.R_RicochetSound( pos );
		break;

	case TE_STREAK_SPLASH:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		dir[0] = MSG_ReadCoord( &net_message );
		dir[1] = MSG_ReadCoord( &net_message );
		dir[2] = MSG_ReadCoord( &net_message );

		color = MSG_ReadByte();
		count = MSG_ReadShort();
		speed = MSG_ReadShort();
		type = MSG_ReadShort();
		efx.R_StreakSplash( pos, dir, color, count, speed, -type, type );
		break;

	case TE_SPARKS:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		efx.R_SparkEffect( pos, 8, -200, 200 );
		break;

	case TE_GUNSHOT:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		efx.R_RunParticleEffect( pos, vec3_origin, 0, 20 );
		efx.R_RicochetSound( pos );
		break;

	case TE_BSPDECAL:
	case TE_DECAL:
	case TE_WORLDDECAL:
	case TE_WORLDDECALHIGH:
	case TE_DECALHIGH:
		{
			int		decalTextureIndex;

			pos[0] = MSG_ReadCoord( &net_message );
			pos[1] = MSG_ReadCoord( &net_message );
			pos[2] = MSG_ReadCoord( &net_message );

			modelIndex = 0;
			if ( type == TE_BSPDECAL )
			{
				flags = FDECAL_PERMANENT;
				decalTextureIndex = MSG_ReadShort();
			}
			else
			{
				flags = 0;
				decalTextureIndex = MSG_ReadByte();
			}

			if ( type == TE_DECAL || type == TE_DECALHIGH || type == TE_BSPDECAL )
			{
				entnumber = MSG_ReadShort();
				if ( type == TE_BSPDECAL && entnumber )
					modelIndex = MSG_ReadShort();
			}
			else
			{
				entnumber = 0;
			}

			if ( type == TE_DECALHIGH || type == TE_WORLDDECALHIGH )
				decalTextureIndex += 256;

			if ( entnumber >= cl.max_edicts )
				Sys_Error( "Decal: entity = %i", entnumber );

			if ( r_decals.value )
				efx.R_DecalShoot( efx.Draw_DecalIndex( decalTextureIndex ), entnumber, modelIndex, pos, flags );
		}
		break;

	case TE_PROJECTILE:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		dir[0] = MSG_ReadCoord( &net_message );
		dir[1] = MSG_ReadCoord( &net_message );
		dir[2] = MSG_ReadCoord( &net_message );

		modelIndex = MSG_ReadShort();
		life = MSG_ReadByte();
		entnumber = MSG_ReadByte();
		efx.R_Projectile( pos, dir, modelIndex, life, entnumber, NULL );
		break;

	case TE_SPRAY:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		dir[0] = MSG_ReadCoord( &net_message );
		dir[1] = MSG_ReadCoord( &net_message );
		dir[2] = MSG_ReadCoord( &net_message );

		modelIndex = MSG_ReadShort();
		count = MSG_ReadByte();
		speed = MSG_ReadByte();
		iRand = MSG_ReadByte();
		type = MSG_ReadByte();
		efx.R_Spray( pos, dir, modelIndex, count, speed, iRand, type );
		break;

	case TE_PLAYERSPRITES:
		{
			int		isize;

			entnumber = MSG_ReadShort();
			modelIndex = MSG_ReadShort();
			count = MSG_ReadByte();
			isize = MSG_ReadByte();
			efx.R_PlayerSprites( entnumber, modelIndex, count, isize );
		}
		break;

	case TE_PARTICLEBURST:
		{
			int		radius;

			pos[0] = MSG_ReadCoord( &net_message );
			pos[1] = MSG_ReadCoord( &net_message );
			pos[2] = MSG_ReadCoord( &net_message );

			radius = MSG_ReadShort();
			color = MSG_ReadByte();
			life = MSG_ReadByte();
			efx.R_ParticleBurst( pos, radius, color, life / 10.0 );
		}
		break;

	case TE_FIREFIELD:
		{
			int		radius;

			pos[0] = MSG_ReadCoord( &net_message );
			pos[1] = MSG_ReadCoord( &net_message );
			pos[2] = MSG_ReadCoord( &net_message );

			radius = MSG_ReadShort();
			modelIndex = MSG_ReadShort();
			count = MSG_ReadByte();
			flags = MSG_ReadByte();
			life = MSG_ReadByte();
			efx.R_FireField( pos, radius, modelIndex, count, flags, life / 10.0 );
		}
		break;

	case TE_PLAYERATTACHMENT:
		{
			float	offset;

			entnumber = MSG_ReadByte();
			offset = MSG_ReadCoord( &net_message );
			modelIndex = MSG_ReadShort();
			life = MSG_ReadShort();
			efx.R_AttachTentToPlayer( entnumber, modelIndex, offset, life / 10 );
		}
		break;

	case TE_KILLPLAYERATTACHMENTS:
		entnumber = MSG_ReadByte();
		efx.R_KillAttachedTents( entnumber );
		break;

	case TE_MULTIGUNSHOT:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		dir[0] = MSG_ReadCoord( &net_message ) / 10.0;
		dir[1] = MSG_ReadCoord( &net_message ) / 10.0;
		dir[2] = MSG_ReadCoord( &net_message ) / 10.0;

		size[0] = MSG_ReadCoord( &net_message );
		size[1] = MSG_ReadCoord( &net_message );
		size[2] = 0;
		size[0] = size[0] / 100.0;
		size[1] = size[1] / 100.0;

		count = MSG_ReadByte();
		modelIndex = MSG_ReadByte();
		index_array[0] = modelIndex;
		efx.R_MultiGunshot( pos, dir, size, count, 1, index_array );
		break;

	case TE_USERTRACER:
		{
			float	length;

			pos[0] = MSG_ReadCoord( &net_message );
			pos[1] = MSG_ReadCoord( &net_message );
			pos[2] = MSG_ReadCoord( &net_message );

			dir[0] = MSG_ReadCoord( &net_message );
			dir[1] = MSG_ReadCoord( &net_message );
			dir[2] = MSG_ReadCoord( &net_message );

			life = MSG_ReadByte();
			color = MSG_ReadByte();
			length = MSG_ReadByte();
			efx.R_UserTracerParticle( pos, dir, life / 10.0, color, length / 10.0, 0, NULL );
		}
		break;

	case TE_PLAYERDECAL:
		{
			int				decalTextureIndex;
			int				playernum;
			customization_t	*pCust;
			texture_t		*pTexture;

			pTexture = NULL;
			flags = FDECAL_CUSTOM;

			playernum = MSG_ReadByte();
			playernum--;

			pCust = NULL;
			if ( playernum >= 0 && playernum < MAX_CLIENTS )
				pCust = cl.players[ playernum ].customdata.pNext;

			pos[0] = MSG_ReadCoord( &net_message );
			pos[1] = MSG_ReadCoord( &net_message );
			pos[2] = MSG_ReadCoord( &net_message );

			entnumber = MSG_ReadShort();
			decalTextureIndex = MSG_ReadByte();
			modelIndex = 0;

			if ( entnumber >= cl.max_edicts )
				Sys_Error( "Decal: entity = %i", entnumber );

			if ( pCust && pCust->pBuffer && pCust->pInfo && ( pCust->resource.ucFlags & RES_CUSTOM ) &&
				pCust->resource.type == t_decal && pCust->bTranslated )
			{
				decalTextureIndex = min( decalTextureIndex, ( (cachewad_t *)pCust->pInfo )->lumpCount - 1 );
				pCust->nUserData1 = decalTextureIndex;
				pTexture = (texture_t *)Draw_CustomCacheGet( (cachewad_t *)pCust->pInfo, pCust->pBuffer, pCust->resource.nDownloadSize, decalTextureIndex );
			}

			if ( r_decals.value )
			{
				if ( pCust )
				{
					if ( pTexture )
						R_CustomDecalShoot( pTexture, playernum, entnumber, modelIndex, pos, flags );
				}
				else
				{
					efx.R_DecalShoot( efx.Draw_DecalIndex( decalTextureIndex ), entnumber, modelIndex, pos, flags );
				}
			}
		}
		break;

	case TE_GUNSHOTDECAL:
		{
			int		decalTextureIndex;

			pos[0] = MSG_ReadCoord( &net_message );
			pos[1] = MSG_ReadCoord( &net_message );
			pos[2] = MSG_ReadCoord( &net_message );

			entnumber = MSG_ReadShort();
			decalTextureIndex = MSG_ReadByte();
			efx.R_BulletImpactParticles( pos );

			iRand = RandomLong( 0, 0x7FFF );
			if ( iRand < 0x3FFF )
			{
				switch ( iRand % 5 )
				{
				case 0:
					S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_ric1, pos, VOL_NORM, 1.0, 0, PITCH_NORM );
					break;
				case 1:
					S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_ric2, pos, VOL_NORM, 1.0, 0, PITCH_NORM );
					break;
				case 2:
					S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_ric3, pos, VOL_NORM, 1.0, 0, PITCH_NORM );
					break;
				case 3:
					S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_ric4, pos, VOL_NORM, 1.0, 0, PITCH_NORM );
					break;
				case 4:
					S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_ric5, pos, VOL_NORM, 1.0, 0, PITCH_NORM );
					break;
				}
			}

			if ( entnumber >= cl.max_edicts )
				Sys_Error( "Decal: entity = %i", entnumber );

			if ( r_decals.value )
				efx.R_DecalShoot( efx.Draw_DecalIndex( decalTextureIndex ), entnumber, 0, pos, 0 );
		}
		break;

	case TE_TRACER:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		endpos[0] = MSG_ReadCoord( &net_message );
		endpos[1] = MSG_ReadCoord( &net_message );
		endpos[2] = MSG_ReadCoord( &net_message );

		efx.R_TracerEffect( pos, endpos );
		break;

	case TE_LIGHTNING:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		endpos[0] = MSG_ReadCoord( &net_message );
		endpos[1] = MSG_ReadCoord( &net_message );
		endpos[2] = MSG_ReadCoord( &net_message );

		type = MSG_ReadByte();
		size[0] = MSG_ReadByte();
		size[1] = MSG_ReadByte();
		modelIndex = MSG_ReadShort();
		efx.R_BeamLightning( pos, endpos, modelIndex, type * 0.1, size[0] * 0.1, size[1] * 0.01, 0.6, 3.5 );
		break;

	case TE_BEAMPOINTS:
	case TE_BEAMENTPOINT:
	case TE_BEAMENTS:
		if ( type == TE_BEAMENTS )
		{
			startEnt = MSG_ReadShort();
			endEnt = MSG_ReadShort();
		}
		else if ( type == TE_BEAMENTPOINT )
		{
			startEnt = MSG_ReadShort();
			endpos[0] = MSG_ReadCoord( &net_message );
			endpos[1] = MSG_ReadCoord( &net_message );
			endpos[2] = MSG_ReadCoord( &net_message );
		}
		else
		{
			pos[0] = MSG_ReadCoord( &net_message );
			pos[1] = MSG_ReadCoord( &net_message );
			pos[2] = MSG_ReadCoord( &net_message );

			endpos[0] = MSG_ReadCoord( &net_message );
			endpos[1] = MSG_ReadCoord( &net_message );
			endpos[2] = MSG_ReadCoord( &net_message );
		}

		modelIndex = MSG_ReadShort();
		startFrame = MSG_ReadByte();
		frameRate = (unsigned int)MSG_ReadByte() * 0.1;
		life = (unsigned int)MSG_ReadByte() * 0.1;
		size[0] = (unsigned int)MSG_ReadByte();
		size[1] = MSG_ReadByte();
		r = (unsigned int)MSG_ReadByte() * ( 1.0 / 255.0 );
		g = (unsigned int)MSG_ReadByte() * ( 1.0 / 255.0 );
		b = (unsigned int)MSG_ReadByte() * ( 1.0 / 255.0 );
		a = (unsigned int)MSG_ReadByte() * ( 1.0 / 255.0 );
		speed = MSG_ReadByte() * 0.1;

		if ( type == TE_BEAMENTS )
			efx.R_BeamEnts( startEnt, endEnt, modelIndex, life, size[0] * 0.1, size[1] * 0.01, a, speed, startFrame, frameRate, r, g, b );
		else if ( type == TE_BEAMENTPOINT )
			efx.R_BeamEntPoint( startEnt, endpos, modelIndex, life, size[0] * 0.1, size[1] * 0.01, a, speed, startFrame, frameRate, r, g, b );
		else if ( type == TE_BEAMPOINTS )
			efx.R_BeamPoints( pos, endpos, modelIndex, life, size[0] * 0.1, size[1] * 0.01, a, speed, startFrame, frameRate, r, g, b );
		break;

	case TE_BEAMTORUS:
	case TE_BEAMDISK:
	case TE_BEAMCYLINDER:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		endpos[0] = MSG_ReadCoord( &net_message );
		endpos[1] = MSG_ReadCoord( &net_message );
		endpos[2] = MSG_ReadCoord( &net_message );

		modelIndex = MSG_ReadShort();
		startFrame = MSG_ReadByte();
		frameRate = (unsigned int)MSG_ReadByte() * 0.1;
		life = (unsigned int)MSG_ReadByte() * 0.1;
		size[0] = (unsigned int)MSG_ReadByte();
		size[1] = MSG_ReadByte();
		r = (unsigned int)MSG_ReadByte() * ( 1.0 / 255.0 );
		g = (unsigned int)MSG_ReadByte() * ( 1.0 / 255.0 );
		b = (unsigned int)MSG_ReadByte() * ( 1.0 / 255.0 );
		a = (unsigned int)MSG_ReadByte() * ( 1.0 / 255.0 );
		speed = MSG_ReadByte() * 0.1;

		efx.R_BeamCirclePoints( type, pos, endpos, modelIndex, life, size[0], size[1] * 0.01, a, speed, startFrame, frameRate, r, g, b );
		break;

	case TE_BEAMRING:
		startEnt = MSG_ReadShort();
		endEnt = MSG_ReadShort();
		modelIndex = MSG_ReadShort();
		startFrame = MSG_ReadByte();
		frameRate = (unsigned int)MSG_ReadByte() * 0.1;
		life = (unsigned int)MSG_ReadByte() * 0.1;
		size[0] = (unsigned int)MSG_ReadByte();
		size[1] = MSG_ReadByte();
		r = (unsigned int)MSG_ReadByte() * ( 1.0 / 255.0 );
		g = (unsigned int)MSG_ReadByte() * ( 1.0 / 255.0 );
		b = (unsigned int)MSG_ReadByte() * ( 1.0 / 255.0 );
		a = (unsigned int)MSG_ReadByte() * ( 1.0 / 255.0 );
		speed = MSG_ReadByte() * 0.1;

		efx.R_BeamRing( startEnt, endEnt, modelIndex, life, size[0] * 0.1, size[1] * 0.01, a, speed, startFrame, frameRate, r, g, b );
		break;

	case TE_SMOKE:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		type = MSG_ReadShort();
		size[0] = MSG_ReadByte() * 0.1;
		size[1] = MSG_ReadByte();
		efx.R_Sprite_Smoke( R_DefaultSprite( pos, type, size[1] ), size[0] );
		break;

	case TE_EXPLOSION:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		type = MSG_ReadShort();
		size[0] = MSG_ReadByte() * 0.1;
		size[1] = MSG_ReadByte();
		flags = MSG_ReadByte();

		if ( size[0] != 0 )
		{
			// create explosion sprite
			efx.R_Sprite_Explode( R_DefaultSprite( pos, type, size[1] ), size[0], flags );

			if ( !( flags & TE_EXPLFLAG_NOPARTICLES ) )
				efx.R_FlickerParticles( pos );

			if ( !( flags & TE_EXPLFLAG_NODLIGHTS ) )
			{
				dl = efx.CL_AllocDlight( 0 );
				VectorCopy( pos, dl->origin );
				dl->radius = 200;
				dl->color.g = 250;
				dl->color.r = 250;
				dl->color.b = 150;
				dl->die = cl.time + 0.01;
				dl->decay = 800;

				dl = efx.CL_AllocDlight( 0 );
				VectorCopy( pos, dl->origin );
				dl->radius = 150;
				dl->color.r = 255;
				dl->color.g = 190;
				dl->color.b = 40;
				dl->die = cl.time + 1.0;
				dl->decay = 200;
			}
		}

		if ( !( flags & TE_EXPLFLAG_NOSOUND ) )
		{
			switch ( RandomLong( 0, 2 ) )
			{
			case 0:
				S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_r_exp1, pos, VOL_NORM, 0.3, 0, PITCH_NORM );
				break;
			case 1:
				S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_r_exp2, pos, VOL_NORM, 0.3, 0, PITCH_NORM );
				break;
			case 2:
				S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_r_exp3, pos, VOL_NORM, 0.3, 0, PITCH_NORM );
				break;
			}
		}
		break;

	case TE_SPRITE:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		modelIndex = MSG_ReadShort();
		size[0] = MSG_ReadByte() * 0.1;
		a = (unsigned int)MSG_ReadByte() * ( 1.0 / 255.0 );
		efx.R_TempSprite( pos, vec3_origin, size[0], modelIndex, kRenderTransAdd, kRenderFxNone, a, 0, FTENT_SPRANIMATE );
		break;

	case TE_GLOWSPRITE:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		modelIndex = MSG_ReadShort();
		life = (unsigned int)MSG_ReadByte() * 0.1;
		size[0] = MSG_ReadByte() * 0.1;
		a = (unsigned int)MSG_ReadByte() * ( 1.0 / 255.0 );
		efx.R_TempSprite( pos, vec3_origin, size[0], modelIndex, kRenderGlow, kRenderFxNoDissipation, a, life, FTENT_FADEOUT );
		break;

	case TE_TAREXPLOSION:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		efx.R_BlobExplosion( pos );
		S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_r_exp1, pos, VOL_NORM, 1.0, 0, PITCH_NORM );
		break;

	case TE_LAVASPLASH:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		efx.R_LavaSplash( pos );
		break;

	case TE_TELEPORT:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		efx.R_TeleportSplash( pos );
		break;

	case TE_EXPLOSION2:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		colorStart = MSG_ReadByte();
		colorLength = MSG_ReadByte();
		efx.R_ParticleExplosion2( pos, colorStart, colorLength );

		dl = efx.CL_AllocDlight( 0 );
		VectorCopy( pos, dl->origin );
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;

		S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_r_exp1, pos, VOL_NORM, 0.6, 0, PITCH_NORM );
		break;

	case TE_LARGEFUNNEL:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		modelIndex = MSG_ReadShort();
		type = MSG_ReadShort();
		efx.R_LargeFunnel( pos, type );
		efx.R_FunnelSprite( pos, modelIndex, type );
		break;

	case TE_SHOWLINE:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		endpos[0] = MSG_ReadCoord( &net_message );
		endpos[1] = MSG_ReadCoord( &net_message );
		endpos[2] = MSG_ReadCoord( &net_message );

		efx.R_ShowLine( pos, endpos );
		break;

	case TE_BEAMSPRITE:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		endpos[0] = MSG_ReadCoord( &net_message );
		endpos[1] = MSG_ReadCoord( &net_message );
		endpos[2] = MSG_ReadCoord( &net_message );

		modelIndex = MSG_ReadShort();
		type = MSG_ReadShort();
		efx.R_BeamPoints( pos, endpos, modelIndex, 0.01, 0.4, 0, RandomFloat( 0.5, 0.655 ), 5.0, 0, 0, 1.0, 0, 0 );
		efx.R_TempSprite( endpos, vec3_origin, 0.1, type, kRenderTransAdd, kRenderFxNone, 0.35, 0.01, 0 );
		break;

	case TE_BLOODSTREAM:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		dir[0] = MSG_ReadCoord( &net_message );
		dir[1] = MSG_ReadCoord( &net_message );
		dir[2] = MSG_ReadCoord( &net_message );

		color = MSG_ReadByte();
		speed = MSG_ReadByte();
		if ( CL_ShowTEBlood() )
			efx.R_BloodStream( pos, dir, color, speed );
		break;

	case TE_BLOOD:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		dir[0] = MSG_ReadCoord( &net_message );
		dir[1] = MSG_ReadCoord( &net_message );
		dir[2] = MSG_ReadCoord( &net_message );

		color = MSG_ReadByte();
		speed = MSG_ReadByte();
		if ( CL_ShowTEBlood() )
			efx.R_Blood( pos, dir, color, speed );
		break;

	case TE_BLOODSPRITE:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		modelIndex = MSG_ReadShort();
		modelIndex2 = MSG_ReadShort();
		color = MSG_ReadByte();
		size[0] = MSG_ReadByte();
		if ( CL_ShowTEBlood() )
			efx.R_BloodSprite( pos, color, modelIndex, modelIndex2, size[0] );
		break;

	case TE_FIZZ:
		entnumber = MSG_ReadShort();
		if ( entnumber >= cl.max_edicts )
			Sys_Error( "Bubble: entity = %i", entnumber );

		type = MSG_ReadShort();
		efx.R_FizzEffect( &cl_entities[ entnumber ], type, MSG_ReadByte() );
		break;

	case TE_MODEL:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		dir[0] = MSG_ReadCoord( &net_message );
		dir[1] = MSG_ReadCoord( &net_message );
		dir[2] = MSG_ReadCoord( &net_message );

		endpos[0] = 0;
		endpos[1] = MSG_ReadAngle();
		endpos[2] = 0;
		type = MSG_ReadShort();
		soundtype = MSG_ReadByte();
		efx.R_TempModel( pos, dir, endpos, MSG_ReadByte() * 0.1, type, soundtype );
		break;

	case TE_EXPLODEMODEL:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		dir[0] = MSG_ReadCoord( &net_message );
		type = MSG_ReadShort();
		entnumber = MSG_ReadShort();
		efx.R_TempSphereModel( pos, dir[0], MSG_ReadByte() * 0.1, entnumber, type );
		break;

	case TE_BREAKMODEL:
		{
			float	flTime;

			pos[0] = MSG_ReadCoord( &net_message );
			pos[1] = MSG_ReadCoord( &net_message );
			pos[2] = MSG_ReadCoord( &net_message );

			size[0] = MSG_ReadCoord( &net_message );
			size[1] = MSG_ReadCoord( &net_message );
			size[2] = MSG_ReadCoord( &net_message );

			dir[0] = MSG_ReadCoord( &net_message );
			dir[1] = MSG_ReadCoord( &net_message );
			dir[2] = MSG_ReadCoord( &net_message );

			a = MSG_ReadByte() * 10.0;
			type = MSG_ReadShort();
			entnumber = MSG_ReadByte();
			flTime = MSG_ReadByte() * 0.1;
			c = MSG_ReadByte();
			efx.R_BreakModel( pos, size, dir, a, flTime, entnumber, type, c );
		}
		break;

	case TE_IMPLOSION:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		size[0] = MSG_ReadByte();
		size[1] = MSG_ReadByte();
		size[2] = MSG_ReadByte() / 10.0;
		efx.R_Implosion( pos, size[0], size[1], size[2] );
		break;

	case TE_SPRITETRAIL:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		endpos[0] = MSG_ReadCoord( &net_message );
		endpos[1] = MSG_ReadCoord( &net_message );
		endpos[2] = MSG_ReadCoord( &net_message );

		modelIndex = MSG_ReadShort();
		count = MSG_ReadByte();
		life = (unsigned int)MSG_ReadByte() * 0.1;
		size[0] = (unsigned int)MSG_ReadByte();
		if ( size[0] != 0 )
			size[0] = size[0] * 0.1;
		else
			size[0] = 1.0;
		size[1] = MSG_ReadByte() * 10.0;
		speed = MSG_ReadByte() * 10.0;
		efx.R_Sprite_Trail( type, pos, endpos, modelIndex, count, life, size[0], size[1], 255, speed );
		break;

	case TE_BEAMFOLLOW:
		startEnt = MSG_ReadShort();
		modelIndex = MSG_ReadShort();
		life = (unsigned int)MSG_ReadByte() * 0.1;
		size[0] = (unsigned int)MSG_ReadByte();
		r = (unsigned int)MSG_ReadByte() * ( 1.0 / 255.0 );
		g = (unsigned int)MSG_ReadByte() * ( 1.0 / 255.0 );
		b = (unsigned int)MSG_ReadByte() * ( 1.0 / 255.0 );
		a = (unsigned int)MSG_ReadByte() * ( 1.0 / 255.0 );
		efx.R_BeamFollow( startEnt, modelIndex, life, size[0], r, g, b, a );
		break;

	case TE_DLIGHT:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		dl = efx.CL_AllocDlight( 0 );
		VectorCopy( pos, dl->origin );
		dl->radius = MSG_ReadByte() * 10;
		dl->color.r = MSG_ReadByte();
		dl->color.g = MSG_ReadByte();
		dl->color.b = MSG_ReadByte();
		dl->die = cl.time + MSG_ReadByte() * 0.1;
		dl->decay = MSG_ReadByte() * 10;
		break;

	case TE_ELIGHT:
		startEnt = MSG_ReadShort();
		dl = efx.CL_AllocElight( startEnt );
		dl->origin[0] = MSG_ReadCoord( &net_message );
		dl->origin[1] = MSG_ReadCoord( &net_message );
		dl->origin[2] = MSG_ReadCoord( &net_message );
		dl->radius = MSG_ReadCoord( &net_message );
		dl->color.r = MSG_ReadByte();
		dl->color.g = MSG_ReadByte();
		dl->color.b = MSG_ReadByte();
		life = MSG_ReadByte() * 0.1;
		dl->die = cl.time + life;
		dl->decay = MSG_ReadCoord( &net_message );
		if ( life != 0 )
			dl->decay /= life;
		break;

	case TE_KILLBEAM:
		startEnt = MSG_ReadShort();
		efx.R_BeamKill( startEnt );
		break;

	case TE_SPRITE_SPRAY:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		dir[0] = MSG_ReadCoord( &net_message );
		dir[1] = MSG_ReadCoord( &net_message );
		dir[2] = MSG_ReadCoord( &net_message );

		modelIndex = MSG_ReadShort();
		count = MSG_ReadByte();
		speed = MSG_ReadByte();
		iRand = MSG_ReadByte();
		speed *= 2;
		efx.R_Sprite_Spray( pos, dir, modelIndex, count, speed, iRand );
		break;

	case TE_BUBBLES:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		endpos[0] = MSG_ReadCoord( &net_message );
		endpos[1] = MSG_ReadCoord( &net_message );
		endpos[2] = MSG_ReadCoord( &net_message );

		size[0] = MSG_ReadCoord( &net_message );
		modelIndex = MSG_ReadShort();
		count = MSG_ReadByte();
		size[1] = MSG_ReadCoord( &net_message );
		efx.R_Bubbles( pos, endpos, size[0], modelIndex, count, size[1] );
		break;

	case TE_BUBBLETRAIL:
		pos[0] = MSG_ReadCoord( &net_message );
		pos[1] = MSG_ReadCoord( &net_message );
		pos[2] = MSG_ReadCoord( &net_message );

		endpos[0] = MSG_ReadCoord( &net_message );
		endpos[1] = MSG_ReadCoord( &net_message );
		endpos[2] = MSG_ReadCoord( &net_message );

		size[0] = MSG_ReadCoord( &net_message );
		modelIndex = MSG_ReadShort();
		count = MSG_ReadByte();
		size[1] = MSG_ReadCoord( &net_message );
		efx.R_BubbleTrail( pos, endpos, size[0], modelIndex, count, size[1] );
		break;

	case TE_TEXTMESSAGE:
		CL_ParseTextMessage();
		break;

	default:
		Sys_Error( "CL_ParseTEnt: bad type" );
		break;
	}
}


/*
=================
CL_TempEntInit
=================
*/
void CL_TempEntInit( void )
{
	int		i;

	Q_memset( gTempEnts, 0, sizeof( gTempEnts ) );

	// Fix up pointers
	for ( i = 0; i < MAX_TEMP_ENTITIES - 1; i++ )
	{
		gTempEnts[i].next = &gTempEnts[i + 1];
	}
	gTempEnts[ MAX_TEMP_ENTITIES - 1 ].next = NULL;
	gpTempEntFree = gTempEnts;
	gpTempEntActive = NULL;
}

/*
=================
CL_TempEntPrepare
=================
*/
void CL_TempEntPrepare( TEMPENTITY *pTemp, model_t *model )
{
	// Use these to set per-frame and termination conditions / actions
	Q_memset( &pTemp->entity, 0, sizeof( cl_entity_t ) );

	pTemp->flags = FTENT_NONE;
	pTemp->entity.curstate.colormap = 0;
	pTemp->die = cl.time + 0.75;
	pTemp->entity.model = model;
	pTemp->entity.curstate.rendermode = kRenderNormal;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->fadeSpeed = 0.5;
	pTemp->hitSound = 0;
	pTemp->clientIndex = -1;
	pTemp->bounceFactor = 1;
	pTemp->hitcallback = NULL;
	pTemp->callback = NULL;
}

/*
=================
CL_TempEntAlloc
=================
*/
TEMPENTITY *CL_TempEntAlloc( vec_t *org, model_t *model )
{
	TEMPENTITY	*pTemp;

	if ( !gpTempEntFree )
	{
		Con_DPrintf( "Overflow %d temporary ents!\n", MAX_TEMP_ENTITIES );
		return NULL;
	}

	if ( !model )
	{
		Con_DPrintf( "efx.CL_TempEntAlloc: No model\n" );
		return NULL;
	}

	pTemp = gpTempEntFree;
	gpTempEntFree = pTemp->next;

	CL_TempEntPrepare( pTemp, model );

	pTemp->priority = TENTPRIORITY_LOW;
	VectorCopy( org, pTemp->entity.origin );

	pTemp->next = gpTempEntActive;
	gpTempEntActive = pTemp;

	return pTemp;
}

/*
=================
CL_TempEntAllocNoModel
=================
*/
TEMPENTITY *CL_TempEntAllocNoModel( vec_t *org )
{
	TEMPENTITY	*pTemp;

	if ( !gpTempEntFree )
	{
		Con_DPrintf( "Overflow %d temporary ents!\n", MAX_TEMP_ENTITIES );
		return NULL;
	}

	pTemp = gpTempEntFree;
	gpTempEntFree = pTemp->next;

	CL_TempEntPrepare( pTemp, NULL );

	pTemp->flags |= FTENT_NOMODEL;
	pTemp->priority = TENTPRIORITY_LOW;
	VectorCopy( org, pTemp->entity.origin );

	pTemp->next = gpTempEntActive;
	gpTempEntActive = pTemp;

	return pTemp;
}

/*
=================
CL_TempEntAllocHigh
=================
*/
TEMPENTITY *CL_TempEntAllocHigh( vec_t *org, model_t *model )
{
	TEMPENTITY	*pTemp;
	TEMPENTITY	*pActive;

	if ( !model )
	{
		Con_DPrintf( "temporary ent model invalid\n" );
		return NULL;
	}

	if ( !gpTempEntFree )
	{
		// no temporary ents free, so find the first active low-priority temp ent
		// and overwrite it.
		pActive = gpTempEntActive;
		pTemp = NULL;

		while ( pActive )
		{
			if ( pActive->priority == TENTPRIORITY_LOW )
			{
				// found an active, low-priority tent.
				pTemp = pActive;
				break;
			}

			pActive = pActive->next;
		}

		if ( !pTemp )
		{
			// didn't find anything? The tent list is either full of high-priority tents
			// or all tents in the list are still due to live for > 10 seconds.
			Con_DPrintf( "Couldn't alloc a high priority TENT!\n" );
			return NULL;
		}
	}
	else
	{
		// temp ents are free, so just take the first one
		pTemp = gpTempEntFree;
		gpTempEntFree = pTemp->next;
		pTemp->next = gpTempEntActive;
		gpTempEntActive = pTemp;
	}

	CL_TempEntPrepare( pTemp, model );

	pTemp->priority = TENTPRIORITY_HIGH;
	VectorCopy( org, pTemp->entity.origin );

	return pTemp;
}

/*
=================
CL_TempEntPlaySound

play a sound if the tempent hit something
=================
*/
void CL_TempEntPlaySound( TEMPENTITY *pTemp, float damp )
{
	sfx_t	*rgsfx[6];
	int		i;
	float	fvol;
	int		fShell;
	int		zvel;

	fShell = false;
	i = -1;
	Q_memset( rgsfx, 0, sizeof( rgsfx ) );

	switch ( pTemp->hitSound )
	{
	default:
		return;	// null sound

	case BOUNCE_GLASS:
		rgsfx[0] = cl_sfx_glass1;
		rgsfx[1] = cl_sfx_glass2;
		rgsfx[2] = cl_sfx_glass3;
		i = 3;
		fvol = 0.8;
		break;

	case BOUNCE_METAL:
		rgsfx[0] = cl_sfx_metal1;
		rgsfx[1] = cl_sfx_metal2;
		rgsfx[2] = cl_sfx_metal3;
		i = 3;
		fvol = 0.8;
		break;

	case BOUNCE_FLESH:
		rgsfx[0] = cl_sfx_flesh1;
		rgsfx[1] = cl_sfx_flesh2;
		rgsfx[2] = cl_sfx_flesh3;
		rgsfx[3] = cl_sfx_flesh4;
		rgsfx[4] = cl_sfx_flesh5;
		rgsfx[5] = cl_sfx_flesh6;
		i = 6;
		fvol = 0.8;
		break;

	case BOUNCE_WOOD:
		rgsfx[0] = cl_sfx_wood1;
		rgsfx[1] = cl_sfx_wood2;
		rgsfx[2] = cl_sfx_wood3;
		i = 3;
		fvol = 0.8;
		break;

	case BOUNCE_SHRAP:
		rgsfx[0] = cl_sfx_ric1;
		rgsfx[1] = cl_sfx_ric2;
		rgsfx[2] = cl_sfx_ric3;
		rgsfx[3] = cl_sfx_ric4;
		rgsfx[4] = cl_sfx_ric5;
		i = 5;
		fvol = 0.8;
		break;

	case BOUNCE_SHOTSHELL:
		rgsfx[0] = cl_sfx_sshell1;
		rgsfx[1] = cl_sfx_sshell2;
		rgsfx[2] = cl_sfx_sshell3;
		i = 3;
		fShell = true;	// shell casings have different playback parameters
		fvol = 0.5;
		break;

	case BOUNCE_SHELL:
		rgsfx[0] = cl_sfx_pl_shell1;
		rgsfx[1] = cl_sfx_pl_shell2;
		rgsfx[2] = cl_sfx_pl_shell3;
		i = 3;
		fShell = true;	// shell casings have different playback parameters
		fvol = 0.8;
		break;

	case BOUNCE_CONCRETE:
		rgsfx[0] = cl_sfx_concrete1;
		rgsfx[1] = cl_sfx_concrete2;
		rgsfx[2] = cl_sfx_concrete3;
		i = 3;
		fvol = 0.8;
		break;
	}

	zvel = abs( pTemp->entity.baseline.origin[2] );

	// only play one out of every n
	if ( fShell )
	{
		// play first bounce, then 1 out of 3
		if ( zvel < 200 && RandomLong( 0, 3 ) )
			return;
	}
	else
	{
		if ( RandomLong( 0, 5 ) )
			return;
	}

	if ( i > 0 && damp > 0.0 )
	{
		int		pitch;

		if ( fShell )
			fvol *= min( 1.0, ( (float)zvel ) / 350.0 );
		else
			fvol *= min( 1.0, ( (float)zvel ) / 450.0 );

		if ( RandomLong( 0, 3 ) )
			pitch = PITCH_NORM;
		else if ( fShell )
			pitch = PITCH_NORM;
		else
			pitch = RandomLong( 90, 124 );

		S_StartDynamicSound( -1, CHAN_AUTO, rgsfx[ RandomLong( 0, i - 1 ) ], pTemp->entity.origin, fvol, 1.0, 0, pitch );
	}
}

/*
=================
CL_AddVisibleEntity
=================
*/
int CL_AddVisibleEntity( cl_entity_t *pEntity )
{
	int		i;
	vec3_t	mins, maxs;

	if ( !pEntity->model )
		return 0;

	for ( i = 0; i < 3; i++ )
	{
		mins[i] = pEntity->origin[i] + pEntity->model->mins[i];
		maxs[i] = pEntity->origin[i] + pEntity->model->maxs[i];
	}

	// FIXME: Check in visible list
	if ( PVSNode( cl.worldmodel->nodes, mins, maxs ) )
	{
		VectorCopy( pEntity->angles, pEntity->curstate.angles );
		VectorCopy( pEntity->angles, pEntity->latched.prevangles );

		ClientDLL_AddEntity( ET_TEMPENTITY, pEntity );

		return 1;
	}

	return 0;
}

/*
=================
CL_TempEntUpdate
=================
*/
void CL_TempEntUpdate( void )
{
	double	frametime;

	if ( !cl.worldmodel )
		return;

	frametime = cl.time - cl.oldtime;
	ClientDLL_TempEntUpdate( frametime, cl.time, movevars.gravity, &gpTempEntFree, &gpTempEntActive, CL_AddVisibleEntity, CL_TempEntPlaySound );
}

/*
=================
CL_FxBlend
=================
*/
int CL_FxBlend( cl_entity_t *ent )
{
	int		blend;
	float	offset;

	offset = ( (int)ent->curstate.number ) * 363.0;	// Use ent index to de-sync these fx

	switch ( ent->curstate.renderfx )
	{
	case kRenderFxPulseSlowWide:
		blend = ent->curstate.renderamt + 0x40 * sin( cl.time * 2 + offset );
		break;
	case kRenderFxPulseFastWide:
		blend = ent->curstate.renderamt + 0x40 * sin( cl.time * 8 + offset );
		break;
	case kRenderFxPulseSlow:
		blend = ent->curstate.renderamt + 0x10 * sin( cl.time * 2 + offset );
		break;
	case kRenderFxPulseFast:
		blend = ent->curstate.renderamt + 0x10 * sin( cl.time * 8 + offset );
		break;
	// JAY: HACK for now -- not time based
	case kRenderFxFadeSlow:
		if ( ent->curstate.renderamt > 0 )
			ent->curstate.renderamt -= 1;
		else
			ent->curstate.renderamt = 0;
		blend = ent->curstate.renderamt;
		break;
	case kRenderFxFadeFast:
		if ( ent->curstate.renderamt > 3 )
			ent->curstate.renderamt -= 4;
		else
			ent->curstate.renderamt = 0;
		blend = ent->curstate.renderamt;
		break;
	case kRenderFxSolidSlow:
		if ( ent->curstate.renderamt < 255 )
			ent->curstate.renderamt += 1;
		else
			ent->curstate.renderamt = 255;
		blend = ent->curstate.renderamt;
		break;
	case kRenderFxSolidFast:
		if ( ent->curstate.renderamt < 252 )
			ent->curstate.renderamt += 4;
		else
			ent->curstate.renderamt = 255;
		blend = ent->curstate.renderamt;
		break;
	case kRenderFxStrobeSlow:
		blend = 20 * sin( cl.time * 4 + offset );
		if ( blend < 0 )
			blend = 0;
		else
			blend = ent->curstate.renderamt;
		break;
	case kRenderFxStrobeFast:
		blend = 20 * sin( cl.time * 16 + offset );
		if ( blend < 0 )
			blend = 0;
		else
			blend = ent->curstate.renderamt;
		break;
	case kRenderFxStrobeFaster:
		blend = 20 * sin( cl.time * 36 + offset );
		if ( blend < 0 )
			blend = 0;
		else
			blend = ent->curstate.renderamt;
		break;
	case kRenderFxFlickerSlow:
		blend = 20 * ( sin( cl.time * 2 ) + sin( cl.time * 17 + offset ) );
		if ( blend < 0 )
			blend = 0;
		else
			blend = ent->curstate.renderamt;
		break;
	case kRenderFxFlickerFast:
		blend = 20 * ( sin( cl.time * 16 ) + sin( cl.time * 23 + offset ) );
		if ( blend < 0 )
			blend = 0;
		else
			blend = ent->curstate.renderamt;
		break;
	case kRenderFxHologram:
	case kRenderFxDistort:
		{
			vec3_t	tmp;
			float	dist;

			VectorCopy( ent->origin, tmp );
			VectorSubtract( tmp, r_origin, tmp );
			dist = DotProduct( tmp, vpn );

			// Turn off distance fade
			if ( ent->curstate.renderfx == kRenderFxDistort )
				dist = 1;

			if ( dist <= 0 )
			{
				blend = 0;
			}
			else
			{
				ent->curstate.renderamt = 180;
				if ( dist <= 100 )
					blend = ent->curstate.renderamt;
				else
					blend = (int)( ( 1.0 - ( dist - 100 ) * ( 1.0 / 400.0 ) ) * ent->curstate.renderamt );
				blend += RandomLong( -32, 31 );
			}
		}
		break;
	default:
		blend = ent->curstate.renderamt;
		break;
	}

	if ( blend > 255 )
		blend = 255;
	else if ( blend < 0 )
		blend = 0;

	return blend;
}

/*
=================
CL_FxTransform
=================
*/
void CL_FxTransform( cl_entity_t *ent, float transform[3][4] )
{
	switch ( ent->curstate.renderfx )
	{
	case kRenderFxDistort:
	case kRenderFxHologram:
		if ( RandomLong( 0, 49 ) == 0 )
		{
			int	axis;

			axis = RandomLong( 0, 1 );
			if ( axis == 1 ) // Choose between x & z
				axis = 2;
			VectorScale( transform[axis], RandomFloat( 1, 1.484 ), transform[axis] );
		}
		else if ( RandomLong( 0, 49 ) == 0 )
		{
			float	offset;
			int		axis;

			axis = RandomLong( 0, 1 );
			if ( axis == 1 ) // Choose between x & z
				axis = 2;
			offset = RandomFloat( -10, 10 );
			transform[RandomLong( 0, 2 )][3] += offset;
		}
		break;
	case kRenderFxExplode:
		{
			float	scale;

			scale = 1.0 + ( cl.time - ent->curstate.animtime ) * 10.0;
			if ( scale > 2 )	// Don't blow up more than 200%
				scale = 2;
			transform[0][1] *= scale;
			transform[1][1] *= scale;
			transform[2][1] *= scale;
		}
		break;
	}
}

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame( msprite_t *pSprite, int frame )
{
	mspriteframe_t	*pspriteframe;

	pspriteframe = NULL;

	if ( !pSprite )
	{
		Con_Printf( "Sprite:  no pSprite!!!\n" );
		return NULL;
	}

	if ( !pSprite->numframes )
	{
		Con_Printf( "Sprite:  pSprite has no frames!!!\n" );
		return NULL;
	}

	if ( frame >= pSprite->numframes || frame < 0 )
	{
		Con_DPrintf( "Sprite: no such frame %d on\n", frame );
		frame = 0;
	}

	if ( pSprite->frames[frame].type == SPR_SINGLE )
		pspriteframe = pSprite->frames[frame].frameptr;

	return pspriteframe;
}

/*
================
R_GetSpriteAxes
================
*/
void R_GetSpriteAxes( cl_entity_t *pEntity, int type, vec_t *forward, vec_t *right, vec_t *up )
{
	int		i;
	float	dot, angle, sr, cr;
	vec3_t	tvec;

	// Automatically roll parallel sprites if requested
	if ( pEntity->angles[2] != 0 && type == SPR_VP_PARALLEL )
		type = SPR_VP_PARALLEL_ORIENTED;

	switch ( type )
	{
	case SPR_FACING_UPRIGHT:
		// generate the sprite's axes, with vup straight up in worldspace, and
		// r_spritedesc.vright perpendicular to the view direction.
		// This will not work if the view direction is very close to straight up or
		// down, because the cross product will be between two nearly parallel
		// vectors and starts to approach an undefined state, so we don't draw if
		// the two vectors are less than 1 degree apart
		tvec[0] = -modelorg[0];
		tvec[1] = -modelorg[1];
		tvec[2] = -modelorg[2];
		VectorNormalize( tvec );
		dot = tvec[2];	// same as DotProduct (tvec, r_spritedesc.vup) because
						//  r_spritedesc.vup is 0, 0, 1
		if ( ( dot > 0.999848 ) || ( dot < -0.999848 ) )	// cos(1 degree) = 0.999848
			return;
		up[0] = 0;
		up[1] = 0;
		up[2] = 1;
		right[0] = tvec[1];
		right[1] = -tvec[0];
		right[2] = 0;
		VectorNormalize( right );
		forward[0] = -right[1];
		forward[1] = right[0];
		forward[2] = 0;
		break;

	case SPR_VP_PARALLEL:
		// generate the sprite's axes, completely parallel to the viewplane. There
		// are no problem situations, because the sprite is always in the same
		// position relative to the viewer
		for ( i = 0; i < 3; i++ )
		{
			up[i] = vup[i];
			right[i] = vright[i];
			forward[i] = vpn[i];
		}
		break;

	case SPR_VP_PARALLEL_UPRIGHT:
		// generate the sprite's axes, with vup straight up in worldspace, and
		// r_spritedesc.vright perpendicular to modelorg.
		// This will not work if the view direction is very close to straight up or
		// down, because the cross product will be between two nearly parallel
		// vectors and starts to approach an undefined state, so we don't draw if
		// the two vectors are less than 1 degree apart
		dot = vpn[2];	// same as DotProduct (vpn, r_spritedesc.vup) because
						//  r_spritedesc.vup is 0, 0, 1
		if ( ( dot > 0.999848 ) || ( dot < -0.999848 ) )	// cos(1 degree) = 0.999848
			return;
		up[0] = 0;
		up[1] = 0;
		up[2] = 1;
		right[0] = vpn[1];
		right[1] = -vpn[0];
		right[2] = 0;
		VectorNormalize( right );
		forward[0] = -right[1];
		forward[1] = right[0];
		forward[2] = 0;
		break;

	case SPR_ORIENTED:
		// generate the sprite's axes, according to the sprite's world orientation
		AngleVectors( pEntity->angles, forward, right, up );
		break;

	case SPR_VP_PARALLEL_ORIENTED:
		// generate the sprite's axes, parallel to the viewplane, but rotated in
		// that plane around the center according to the sprite entity's roll
		// angle. So vpn stays the same, but vright and vup rotate
		angle = currententity->angles[ROLL] * ( M_PI * 2 / 360 );
		sr = sin( angle );
		cr = cos( angle );

		for ( i = 0; i < 3; i++ )
		{
			forward[i] = vpn[i];
			right[i] = vright[i] * cr + vup[i] * sr;
			up[i] = vright[i] * -sr + vup[i] * cr;
		}
		break;

	default:
		Sys_Error( "R_DrawSprite: Bad sprite type %d", type );
		break;
	}
}

/*
================
R_SpriteColor
================
*/
void R_SpriteColor( colorVec *pColor, cl_entity_t *pEntity, int alpha )
{
	int		a;

	if ( pEntity->curstate.rendermode == kRenderTransAdd || pEntity->curstate.rendermode == kRenderGlow )
		a = alpha;
	else
		a = 256;

	if ( pEntity->curstate.rendercolor.r == 0 && pEntity->curstate.rendercolor.g == 0 && pEntity->curstate.rendercolor.b == 0 )
	{
		pColor->r = pColor->g = pColor->b = ( 255 * a ) >> 8;
	}
	else
	{
		pColor->r = ( ( pEntity->curstate.rendercolor.r * a ) >> 8 );
		pColor->g = ( ( pEntity->curstate.rendercolor.g * a ) >> 8 );
		pColor->b = ( ( pEntity->curstate.rendercolor.b * a ) >> 8 );

		if ( filterMode )
		{
			pColor->r = pColor->r * filterColorRed;
			pColor->g = pColor->g * filterColorGreen;
			pColor->b = pColor->b * filterColorBlue;
		}
	}
}

/*
================
R_GetAttachmentPoint
================
*/
float *R_GetAttachmentPoint( int entity, int attachment )
{
	cl_entity_t	*pEntity;

	pEntity = &cl_entities[ entity ];
	if ( attachment )
		return pEntity->attachment[ attachment - 1 ];

	return pEntity->origin;
}

/*
================
R_Explosion

Create an explosion (scale is magnitude)
================
*/
void R_Explosion( float *pos, int model, float scale, float framerate, int flags )
{
	dlight_t	*dl;

	if ( scale != 0.0 )
	{
		// create explosion sprite
		efx.R_Sprite_Explode( R_DefaultSprite( pos, model, framerate ), scale, flags );

		if ( !( flags & TE_EXPLFLAG_NOPARTICLES ) )
			efx.R_FlickerParticles( pos );

		if ( !( flags & TE_EXPLFLAG_NODLIGHTS ) )
		{
			dl = efx.CL_AllocDlight( 0 );
			VectorCopy( pos, dl->origin );
			dl->radius = 200;
			dl->color.g = 250;
			dl->color.r = 250;
			dl->color.b = 150;
			dl->die = cl.time + 0.01;
			dl->decay = 800;

			dl = efx.CL_AllocDlight( 0 );
			VectorCopy( pos, dl->origin );
			dl->radius = 150;
			dl->color.r = 255;
			dl->color.g = 190;
			dl->color.b = 40;
			dl->die = cl.time + 1.0;
			dl->decay = 200;
		}
	}

	if ( !( flags & TE_EXPLFLAG_NOSOUND ) )
	{
		switch ( RandomLong( 0, 2 ) )
		{
		case 0:
			S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_r_exp1, pos, VOL_NORM, 0.3, 0, PITCH_NORM );
			break;
		case 1:
			S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_r_exp2, pos, VOL_NORM, 0.3, 0, PITCH_NORM );
			break;
		case 2:
			S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_r_exp3, pos, VOL_NORM, 0.3, 0, PITCH_NORM );
			break;
		}
	}
}

/*
================
CL_AllocCustomTempEntity
================
*/
TEMPENTITY *CL_AllocCustomTempEntity( float *origin, struct model_s *model, int high, void (*callback)( struct tempent_s *ent, float frametime, float currenttime ) )
{
	TEMPENTITY	*p;

	if ( high )
		p = efx.CL_TempEntAllocHigh( origin, model );
	else
		p = efx.CL_TempEntAlloc( origin, model );

	if ( p )
	{
		p->flags |= FTENT_CLIENTCUSTOM;
		p->callback = callback;
		p->die = cl.time;
	}

	return p;
}
