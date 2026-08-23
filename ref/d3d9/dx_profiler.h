#pragma once

enum dx_profile_phase_t
{
	DX_PROFILE_BEGINFRAME = 0,
	DX_PROFILE_LIGHTMAP_UPDATES,
	DX_PROFILE_CLEAR,
	DX_PROFILE_BEGINSCENE,
	DX_PROFILE_SCENE_SETUP,
	DX_PROFILE_DRAWENTS,
	DX_PROFILE_SKY,
	DX_PROFILE_STATIC_WORLD,
	DX_PROFILE_ENTITY_CLASSIFY,
	DX_PROFILE_OPAQUE_ENTITIES,
	DX_PROFILE_SPRITE_FLUSH,
	DX_PROFILE_WATER_DEPTH,
	DX_PROFILE_WATER_COLOR,
	DX_PROFILE_TRANSLUCENT_ENTITIES,
	DX_PROFILE_BEAMS,
	DX_PROFILE_VIEWMODEL,
	DX_PROFILE_2D,
	DX_PROFILE_ENDSCENE,
	DX_PROFILE_PRESENT,
	DX_PROFILE_ENDFRAME_MISC,
	DX_PROFILE_BRUSHMODEL,
	DX_PROFILE_STUDIOMODEL,
	DX_PROFILE_SPRITEMODEL,
	DX_PROFILE_DECALS,
	DX_PROFILE_WATERMODEL,
	DX_PROFILE_STUDIO_LIGHT_PROBE,
	DX_PROFILE_STUDIO_SKY_PROBE,
	DX_PROFILE_STUDIO_DYNAMIC_LIGHTS,
	DX_PROFILE_STUDIO_LOCAL_LIGHTS,
	DX_PROFILE_COUNT
};

struct dx_speeds_frame_t
{
	unsigned int drawCalls = 0;
	unsigned int indexedDrawCalls = 0;
	unsigned int primitiveCount = 0;
};

double DX_Now( void );
bool DX_ProfileActive( void );
void DX_ProfileTime( int phase, double seconds );
void DX_ProfileDraw( bool indexed, unsigned int primitiveCount );
void DX_ProfileLock( int owner, int bufferKind, unsigned int lockFlags, unsigned int bytes );
void DX_ProfileState( int kind, bool changed );
void DX_ProfileTexture( int kind, bool changed );
void DX_ProfileEvent( int eventId, unsigned int amount );

void DX_ProfilerRegisterCommands( void );
void DX_ProfilerRemoveCommands( void );
void DX_ProfilerBeginFrame( bool collectSpeeds );
void DX_ProfilerSampleFrame( void );
const dx_speeds_frame_t &DX_ProfileSpeeds( void );

struct dx_profile_scope_t
{
	int phase = 0;
	double start = 0.0;
	bool active = false;

	explicit dx_profile_scope_t( int profilePhase )
	{

		phase = profilePhase;
		active = DX_ProfileActive( );
		start = active ? DX_Now( ) : 0.0;
	}

	dx_profile_scope_t( const dx_profile_scope_t & ) = delete;
	dx_profile_scope_t &operator=( const dx_profile_scope_t & ) = delete;

	~dx_profile_scope_t( )
	{
		if( active )
			DX_ProfileTime( phase, DX_Now( ) - start );
	}
};

#define DX_PROFILE_SCOPE( name, phaseId ) dx_profile_scope_t name( phaseId )
