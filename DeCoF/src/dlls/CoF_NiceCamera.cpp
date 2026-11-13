//=========================================================
// real_camera: an animated camera model, view_point: the
// point it looks from
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "animation.h"

class CViewPoint : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
};

LINK_ENTITY_TO_CLASS( view_point, CViewPoint );

void CViewPoint::Spawn( void )
{
	Precache();

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects = 0;
	pev->frame = 0;

	SET_MODEL( ENT( pev ), "models/tape_recorder.mdl" );
}

void CViewPoint::Precache( void )
{
	PRECACHE_MODEL( "models/tape_recorder.mdl" );
}


class CRealCamera : public CBaseAnimating
{
public:
	void Spawn( void );
	void SetSequenceByName( char *szSequence );
	void Precache( void );
	void StartAnimation( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void EXPORT Think( void );
	void EXPORT CameraUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( real_camera, CRealCamera );

//=========================================================
// SetSequenceByName (from CBaseMonster)
//=========================================================
void CRealCamera::SetSequenceByName( char *szSequence )
{
	int	iSequence;

	iSequence = LookupSequence( szSequence );

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		pev->sequence		= iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
	}
	else
	{
		// Not available try to get default anim
		ALERT ( at_aiconsole, "%s has no sequence named:%f\n", STRING(pev->classname), szSequence );
		pev->sequence		= 0;	// Set to the reset anim (if it's there)
	}
}

void CRealCamera::Spawn( void )
{
	Precache();

	SET_MODEL( ENT( pev ), "models/camera_player.mdl" );

	UTIL_SetSize( pev, Vector( -16, -16, -36 ), Vector( 16, 16, 36 ) );

	pev->solid = SOLID_NOT;

	SetSequenceByName( "idle" );

	SetUse( &CRealCamera::CameraUse );
}

void CRealCamera::Precache( void )
{
	PRECACHE_MODEL( "models/camera_player.mdl" );
	UTIL_PrecacheOther( "view_point" );
}

void CRealCamera::HandleAnimEvent( MonsterEvent_t *pEvent )
{
}

void CRealCamera::CameraUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	StartAnimation();
}

void CRealCamera::StartAnimation( void )
{
	if ( !pev->targetname )
	{
		UTIL_CenterPrintAll( "Add a name to this entity or I'll knock you out" );
		return;
	}

	if ( !pev->message )
	{
		UTIL_CenterPrintAll( "Error: Specify an animation!" );
		return;
	}

	SetSequenceByName( (char *)STRING( pev->message ) );

	CBasePlayer *pPlayer = (CBasePlayer *)UTIL_FindEntityByClassname( NULL, "player" );

	if ( pPlayer )
		pPlayer->m_pCamera = this;
}

void CRealCamera::Think( void )
{
	Vector BoneOrigin, BoneAngles;
	GetBonePosition( 2, BoneOrigin, BoneAngles );

	pev->fuser1 = BoneOrigin.x;
	pev->fuser2 = BoneOrigin.y;
	pev->fuser3 = BoneOrigin.z;

	ALERT( at_console, "X: %f - Y: %f - Z: %f\n", pev->fuser1, pev->fuser2, pev->fuser3 );
}
