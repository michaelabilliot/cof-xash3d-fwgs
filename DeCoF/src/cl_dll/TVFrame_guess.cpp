// ======================================
// Cry of Fear TV frame panel
// (class name from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "TVFrame_guess.h"

CTVFrame::CTVFrame() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	setBgColor( 0, 0, 0, 255 );

	Initialize();
}

void CTVFrame::Initialize()
{
	m_iTVFrame_guess = 0;
	setVisible( false );
}

int MsgTVFrame_guess( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	// tests the scope panel, not the TV frame
	if ( gViewPort->m_pScope_guess )
		READ_BYTE();

	return 1;
}

void TVFrameInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "TVFrame", MsgTVFrame_guess );
}

CTVFrame::~CTVFrame( void )
{
}
