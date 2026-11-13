// ======================================
// Cry of Fear cinematic black bars
// (class name from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "HUDControl_guess.h"
#include "Credits_guess.h"

extern int g_iLetterboxHeight_guess;

DECLARE_MESSAGE( m_BlackBar_guess, BlackBars )

int CBlackBar::Init( void )
{
	HOOK_MESSAGE( BlackBars );

	m_iFlags |= HUD_ACTIVE;
	gHUD.AddHudElem( this );

	return 1;
}

int CBlackBar::VidInit( void )
{
	m_HUD_blackbar_guess = gHUD.GetSpriteIndex( "cof_blackbar" );
	int iHeight = gHUD.GetSpriteRect( m_HUD_blackbar_guess ).bottom - gHUD.GetSpriteRect( m_HUD_blackbar_guess ).top;
	m_iBottom_guess = ScreenHeight;
	m_iTop_guess = -iHeight - 1;

	return 1;
}

int CBlackBar::Draw( float flTime )
{
	return 1;
}

int CBlackBar::MsgFunc_BlackBars( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_iOn_guess = READ_BYTE();

	if ( m_iOn_guess )
	{
		int iHeight = gHUD.GetSpriteRect( m_HUD_blackbar_guess ).bottom - gHUD.GetSpriteRect( m_HUD_blackbar_guess ).top;
		m_iBottom_guess = ScreenHeight;
		m_iTop_guess = -iHeight - 1;

		if ( gViewPort )
		{
			if ( gViewPort->m_pHUDControl_guess )
			{
				int x, y, x2, y2;

				gViewPort->m_pHUDControl_guess->m_pLetterboxTop_guess->getPos( x, y );
				gViewPort->m_pHUDControl_guess->m_pLetterboxBottom_guess->getPos( x2, y2 );
				gViewPort->m_pHUDControl_guess->m_pLetterboxTop_guess->setPos( x, -g_iLetterboxHeight_guess );
				gViewPort->m_pHUDControl_guess->m_pLetterboxBottom_guess->setPos( x2, ScreenHeight );
			}

			if ( gViewPort && gViewPort->m_pCredits_guess && gViewPort->m_pCredits_guess->isVisible() )
			{
				int x, y, x2, y2;

				gViewPort->m_pCredits_guess->m_pLetterboxTop_guess->getPos( x, y );
				gViewPort->m_pCredits_guess->m_pLetterboxBottom_guess->getPos( x2, y2 );
				gViewPort->m_pCredits_guess->m_pLetterboxTop_guess->setPos( x, -g_iLetterboxHeight_guess );
				gViewPort->m_pCredits_guess->m_pLetterboxBottom_guess->setPos( x2, ScreenHeight );
			}
		}
	}

	return 1;
}
