//
// cof_door_prop_view.cpp - invisible target that answers the camera
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );

class CDoorPropView : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	void EXPORT CameraUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( door_prop_view, CDoorPropView );

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int CDoorPropView::Classify( void )
{
	return CLASS_NONE;
}

//=========================================================
// Spawn
//=========================================================
void CDoorPropView::Spawn( void )
{
	Precache( );

	SET_MODEL( ENT( pev ), "models/null.mdl" );

	UTIL_SetSize( pev, Vector( -16, -16, -36 ), Vector( 16, 16, 36 ) );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;
	m_bloodColor = DONT_BLEED;
	m_MonsterState = MONSTERSTATE_NONE;

	SetUse( &CDoorPropView::CameraUse );
}

void CDoorPropView::Precache( void )
{
	PRECACHE_MODEL( "models/null.mdl" );
}

void CDoorPropView::CameraUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( pActivator->IsPlayer() )
		UTIL_ProMessage( pActivator, "Raging nerd.", 2, false );
}
