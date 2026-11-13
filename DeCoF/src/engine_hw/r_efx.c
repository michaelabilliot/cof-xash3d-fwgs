// r_efx.c -- the effects API the engine gives the client DLL (cl_enginefuncs.pEfxAPI)
//
// The file holds only the table; the effects live in R_PART.C, CL_TENT.C,
// CL_MAIN.C, decals.c and GL_RSURF.C. Its .data lies between qgl.obj and
// R_PART.obj in the retail image.

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "client.h"
#include "GL_MODEL.H"
#include "vid.h"
#include "render.h"
#include "r_efx.h"

// R_PART.C
particle_t *R_AllocParticle( void (*callback)( struct particle_s *particle, float frametime ) );
void R_BlobExplosion( vec_t *org );
void R_Blood( vec_t *org, vec_t *dir, int pcolor, int speed );
void R_BloodStream( vec_t *org, vec_t *dir, int pcolor, int speed );
void R_BulletImpactParticles( vec_t *pos );
void R_EntityParticles( cl_entity_t *ent );
void R_FlickerParticles( vec_t *org );
void R_Implosion( vec_t *end, float radius, int count, float life );
void R_LargeFunnel( vec_t *org, int reverse );
void R_LavaSplash( vec_t *org );
void R_ParticleBurst( vec_t *pos, int size, int color, float life );
void R_ParticleExplosion( vec_t *org );
void R_ParticleExplosion2( vec_t *org, int colorStart, int colorLength );
void R_RocketTrail( vec_t *start, vec_t *end, int type );
void R_RunParticleEffect( vec_t *org, vec_t *dir, int color, int count );
void R_ShowLine( vec_t *start, vec_t *end );
void R_SparkStreaks( vec_t *pos, int count, int velocityMin, int velocityMax );
void R_StreakSplash( vec_t *pos, vec_t *dir, int color, int count, float speed, int velocityMin, int velocityMax );
void R_UserTracerParticle( vec_t *org, vec_t *vel, float life, int colorIndex, float length, unsigned char context, void (*deathfunc)( struct particle_s *particle ) );
particle_t *R_TracerParticles( vec_t *org, vec_t *vel, float life );
void R_TeleportSplash( vec_t *org );
BEAM *R_BeamCirclePoints( int type, vec_t *start, vec_t *end, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b );
BEAM *R_BeamEntPoint( int startEnt, vec_t *end, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b );
BEAM *R_BeamEnts( int startEnt, int endEnt, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b );
BEAM *R_BeamFollow( int startEnt, int modelIndex, float life, float width, float r, float g, float b, float brightness );
void R_BeamKill( int deadEntity );
BEAM *R_BeamLightning( vec_t *start, vec_t *end, int modelIndex, float life, float width, float amplitude, float brightness, float speed );
BEAM *R_BeamPoints( vec_t *start, vec_t *end, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b );
BEAM *R_BeamRing( int startEnt, int endEnt, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b );
void R_GetPackedColor( short *packed, short color );
short R_LookupColor( unsigned char r, unsigned char g, unsigned char b );

// CL_TENT.C
void R_BloodSprite( vec_t *org, int colorindex, int modelIndex, int modelIndex2, float size );
void R_BreakModel( float *pos, float *size, float *dir, float random, float life, int count, int modelIndex, char flags );
void R_Bubbles( vec_t *mins, vec_t *maxs, float height, int modelIndex, int count, float speed );
void R_BubbleTrail( vec_t *start, vec_t *end, float height, int modelIndex, int count, float speed );
void R_Explosion( float *pos, int model, float scale, float framerate, int flags );
void R_FizzEffect( cl_entity_t *pent, int modelIndex, int density );
void R_FireField( vec_t *org, int radius, int modelIndex, int count, int flags, float life );
void R_FunnelSprite( float *org, int modelIndex, int reverse );
void R_MultiGunshot( vec_t *org, vec_t *dir, vec_t *noise, int count, int decalCount, int *decalIndices );
void R_MuzzleFlash( float *pos1, int type );
void R_ParticleBox( float *mins, float *maxs, unsigned char r, unsigned char g, unsigned char b, float life );
void R_ParticleLine( float *start, float *end, unsigned char r, unsigned char g, unsigned char b, float life );
void R_PlayerSprites( int client, int modelIndex, int count, int size );
void R_Projectile( vec_t *origin, vec_t *velocity, int modelIndex, int life, int owner, void (*hitcallback)( struct tempent_s *ent, struct pmtrace_s *ptr ) );
void R_RicochetSound( vec_t *pos );
void R_RicochetSprite( float *pos, model_t *pmodel, float duration, float scale );
void R_RocketFlare( float *pos );
void R_SparkEffect( float *pos, int count, int velocityMin, int velocityMax );
void R_SparkShower( float *pos );
void R_Spray( vec_t *pos, vec_t *dir, int modelIndex, int count, int speed, int spread, int rendermode );
void R_Sprite_Explode( TEMPENTITY *pTemp, float scale, int flags );
void R_Sprite_Smoke( TEMPENTITY *pTemp, float scale );
void R_Sprite_Spray( vec_t *pos, vec_t *dir, int modelIndex, int count, int speed, int iRand );
void R_Sprite_Trail( int type, vec_t *start, vec_t *end, int modelIndex, int count, float life, float size, float amplitude, int renderamt, float speed );
void R_Sprite_WallPuff( TEMPENTITY *pTemp, float scale );
void R_TracerEffect( vec_t *start, vec_t *end );
void R_TempSphereModel( float *pos, float speed, float life, int count, int modelIndex );
TEMPENTITY *R_TempModel( float *pos, float *dir, float *angles, float life, int modelIndex, int soundtype );
TEMPENTITY *R_DefaultSprite( float *pos, int spriteIndex, float framerate );
TEMPENTITY *R_TempSprite( float *pos, float *dir, float scale, int modelIndex, int rendermode, int renderfx, float a, float life, int flags );
void R_AttachTentToPlayer( int client, int modelIndex, float zoffset, float life );
void R_KillAttachedTents( int client );
TEMPENTITY *CL_TempEntAlloc( vec_t *org, model_t *model );
TEMPENTITY *CL_TempEntAllocNoModel( vec_t *org );
TEMPENTITY *CL_TempEntAllocHigh( vec_t *org, model_t *model );
TEMPENTITY *CL_AllocCustomTempEntity( float *origin, struct model_s *model, int high, void (*callback)( struct tempent_s *ent, float frametime, float currenttime ) );

// decals.c
int Draw_DecalIndex( int id );
int Draw_DecalIndexFromName( char *name );

// GL_RSURF.C
void R_DecalShoot( int textureIndex, int entity, int modelIndex, vec_t *position, int flags );
void R_DecalRemoveNonPermanent( int textureIndex );
void R_FireCustomDecal( int textureIndex, int entity, int modelIndex, vec_t *position, int flags, float scale );

// CL_MAIN.C
dlight_t *CL_AllocDlight( int key );
dlight_t *CL_AllocElight( int key );

efx_api_t efx =
{
	R_AllocParticle,
	R_BlobExplosion,
	R_Blood,
	R_BloodSprite,
	R_BloodStream,
	R_BreakModel,
	R_Bubbles,
	R_BubbleTrail,
	R_BulletImpactParticles,
	R_EntityParticles,
	R_Explosion,
	R_FizzEffect,
	R_FireField,
	R_FlickerParticles,
	R_FunnelSprite,
	R_Implosion,
	R_LargeFunnel,
	R_LavaSplash,
	R_MultiGunshot,
	R_MuzzleFlash,
	R_ParticleBox,
	R_ParticleBurst,
	R_ParticleExplosion,
	R_ParticleExplosion2,
	R_ParticleLine,
	R_PlayerSprites,
	R_Projectile,
	R_RicochetSound,
	R_RicochetSprite,
	R_RocketFlare,
	R_RocketTrail,
	R_RunParticleEffect,
	R_ShowLine,
	R_SparkEffect,
	R_SparkShower,
	R_SparkStreaks,
	R_Spray,
	R_Sprite_Explode,
	R_Sprite_Smoke,
	R_Sprite_Spray,
	R_Sprite_Trail,
	R_Sprite_WallPuff,
	R_StreakSplash,
	R_TracerEffect,
	R_UserTracerParticle,
	R_TracerParticles,
	R_TeleportSplash,
	R_TempSphereModel,
	R_TempModel,
	R_DefaultSprite,
	R_TempSprite,
	Draw_DecalIndex,
	Draw_DecalIndexFromName,
	R_DecalShoot,
	R_AttachTentToPlayer,
	R_KillAttachedTents,
	R_BeamCirclePoints,
	R_BeamEntPoint,
	R_BeamEnts,
	R_BeamFollow,
	R_BeamKill,
	R_BeamLightning,
	R_BeamPoints,
	R_BeamRing,
	CL_AllocDlight,
	CL_AllocElight,
	CL_TempEntAlloc,
	CL_TempEntAllocNoModel,
	CL_TempEntAllocHigh,
	CL_AllocCustomTempEntity,
	R_GetPackedColor,
	R_LookupColor,
	R_DecalRemoveNonPermanent,
	R_FireCustomDecal,
};
