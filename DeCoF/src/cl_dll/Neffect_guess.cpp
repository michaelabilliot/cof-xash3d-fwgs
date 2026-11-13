// ======================================
// Cry of Fear screen noise effect
// (class name from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"

extern cvar_t *cl_noiseeffect;
extern cvar_t *cl_noiseamount;

int CNeffect::Init( void )
{
	m_iFlags |= HUD_ACTIVE;
	gHUD.AddHudElem( this );

	return 1;
}

int CNeffect::VidInit( void )
{
	m_HUD_snoise_guess = gHUD.GetSpriteIndex( "snoise" );
	m_HUD_arghnoise_guess = gHUD.GetSpriteIndex( "arghnoise" );
	m_HUD_tv_scanlines_guess = gHUD.GetSpriteIndex( "tv_scanlines" );

	return 1;
}

int CNeffect::Draw( float flTime )
{
 // Original tiled screen grain is permanently disabled; other effects are untouched.
 (void)flTime;
 return 1;
}
