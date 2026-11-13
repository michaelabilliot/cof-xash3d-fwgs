// ======================================
// Cry of Fear computer puzzle panel
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include <VGUI_TextEntry.h>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "event_api.h"
#include "vgui_TeamFortressViewport.h"
#include "Computer_guess.h"
#include "..\game_shared\vgui_loadtga.h"

CComputer::CComputer() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int wide, tall;
	int x, y;
	int r, g, b, a;
	Panel *pPanel;
	CommandButton *pButton;

	pPanel = new Panel( 0, 0, ScreenWidth, ScreenHeight );
	pPanel->setParent( this );
	pPanel->setSize( ScreenWidth, ScreenHeight );
	pPanel->setPos( 0, 0 );
	pPanel->setBgColor( 0, 0, 0, 0 );
	pPanel->setVisible( true );

	m_pBackgroundImage_guess = vgui_LoadTGA( "gfx/vgui/backgroundtoall.tga" );
	if ( m_pBackgroundImage_guess )
		m_pBackgroundImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/backgroundtoall.tga!\n" );

	m_pBackgroundImage_guess->getSize( wide, tall );
	x = ScreenWidth / 2 - wide / 2;
	y = ScreenHeight / 2 - tall / 2;

	m_pBackground_guess = new ImagePanel( m_pBackgroundImage_guess );
	m_pBackground_guess->setParent( this );
	m_pBackground_guess->setPos( x, y );
	m_pBackground_guess->setVisible( true );

	m_pScreenImage_guess = vgui_LoadTGA( "gfx/vgui/puzzle/computer/computer_logon.tga" );
	if ( m_pScreenImage_guess )
		m_pScreenImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/puzzle/computer/computer_logon.tga!\n" );

	x = ScreenWidth / 2 - 277;
	y = ScreenHeight / 2 - 240;

	m_pScreen_guess = new ImagePanel( m_pScreenImage_guess );
	m_pScreen_guess->setParent( this );
	m_pScreen_guess->setPos( x, y );
	m_pScreen_guess->setVisible( true );

	pButton = new CommandButton( "", 157, 320, 75, 23, true, true );
	pButton->setParent( m_pScreen_guess );
	pButton->addActionSignal( new CComputerHandler_Command( this, 1 ) );

	pButton = new CommandButton( "", 238, 320, 75, 23, true, true );
	pButton->setParent( m_pScreen_guess );
	pButton->addActionSignal( new CComputerHandler_Command( this, 2 ) );

	pButton = new CommandButton( "", 478, 447, 29, 29, true, true );
	pButton->setParent( m_pScreen_guess );
	pButton->addActionSignal( new CComputerHandler_Command( this, 2 ) );

	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );

	m_pUser_guess = new TextEntry( "", 160, 204, 239, 16 );
	m_pUser_guess->setVisible( true );
	m_pUser_guess->setParent( m_pScreen_guess );
	pSchemes->getFgColor( hTitleScheme, r, g, b, a );
	m_pUser_guess->setFgColor( r, g, b, a );
	m_pUser_guess->setFgColor( Scheme::sc_primary1 );
	pSchemes->getBgColor( hTitleScheme, r, g, b, a );
	m_pUser_guess->setBgColor( r, g, b, a );

	m_pPassword_guess = new TextEntry( "", 160, 231, 239, 16 );
	m_pPassword_guess->setVisible( true );
	m_pPassword_guess->setParent( m_pScreen_guess );
	pSchemes->getFgColor( hTitleScheme, r, g, b, a );
	m_pPassword_guess->setFgColor( r, g, b, a );
	m_pPassword_guess->setFgColor( Scheme::sc_primary1 );
	pSchemes->getBgColor( hTitleScheme, r, g, b, a );
	m_pPassword_guess->setBgColor( r, g, b, a );

	m_pText_guess = new TextPanel( "", 194, 162, 211, 152 );
	m_pText_guess->setParent( m_pScreen_guess );
	m_pText_guess->setVisible( false );

	setBgColor( 0, 0, 0, 255 );

	Initialize();
}

void CComputer::ActionSignal( int cmd )
{
	char szUser[256];
	char szPassword[256];
	char szCommand[128];

	switch ( cmd )
	{
	case 0:
		break;
	case 1:
		if ( m_iLoggedOn_guess )
		{
			setVisible( false );
			gViewPort->UpdateCursorState();
			return;
		}
		m_pUser_guess->getText( 0, szUser, 256 );
		m_pPassword_guess->getText( 0, szPassword, 256 );
		sprintf( szCommand, "computercheck %s %s", szUser, szPassword );
		gEngfuncs.pfnServerCmd( szCommand );
		break;
	case 2:
		setVisible( false );
		gViewPort->UpdateCursorState();
		break;
	default:
		gEngfuncs.Con_DPrintf( "Unknown Computer ActionSignal %i.\n", cmd );
		break;
	}
}

void CComputer::Initialize()
{
	setVisible( false );
	m_iLoggedOn_guess = 0;
	m_flNextKeyTime_guess = gEngfuncs.GetClientTime();
}

int CComputer::KeyInput( int down, int keynum, const char *pszCurrentBinding )
{
	if ( pszCurrentBinding && !strcmp( pszCurrentBinding, "toggleconsole" ) )
		return 1;

	return 0;
}

int MsgComputer_guess( const char *pszName, int iSize, void *pbuf )
{
	char szText[512];

	BEGIN_READ( pbuf, iSize );

	if ( gViewPort && gViewPort->m_pComputer_guess )
	{
		if ( !gViewPort->m_pComputer_guess->isVisible() )
		{
			gViewPort->m_pComputer_guess->setVisible( true );
			gViewPort->UpdateCursorState();
		}

		gViewPort->UpdateCursorState();

		gViewPort->m_pComputer_guess->m_iLoggedOn_guess = READ_BYTE();
		int iWrongUser = READ_BYTE();
		int iWrongPassword = READ_BYTE();
		int iSilent = READ_BYTE();

		sprintf( szText, "Jill - %s! - David\n", READ_STRING() );
		gViewPort->m_pComputer_guess->m_pText_guess->setText( szText );
		gViewPort->m_pComputer_guess->m_pText_guess->setFgColor( 0, 0, 0, 0 );

		if ( gViewPort->m_pComputer_guess->m_iLoggedOn_guess )
		{
			gViewPort->m_pComputer_guess->m_pScreen_guess->setImage( vgui_LoadTGA( "gfx/vgui/puzzle/computer/computer_loggedon.tga" ) );
			gViewPort->m_pComputer_guess->m_pUser_guess->setVisible( false );
			gViewPort->m_pComputer_guess->m_pPassword_guess->setVisible( false );

			cl_entity_t *pPlayer = gEngfuncs.GetLocalPlayer();
			gViewPort->m_pComputer_guess->m_pText_guess->setVisible( true );

			if ( pPlayer && !iSilent )
				gEngfuncs.pEventAPI->EV_PlaySound( -1, pPlayer->curstate.origin, CHAN_AUTO, "buttons/computer_logon.wav", 0.7, 0.8, 0, PITCH_NORM );
		}
		else
		{
			gViewPort->m_pComputer_guess->m_pScreen_guess->setImage( vgui_LoadTGA( "gfx/vgui/puzzle/computer/computer_logon.tga" ) );
			gViewPort->m_pComputer_guess->m_pUser_guess->setVisible( true );
			gViewPort->m_pComputer_guess->m_pPassword_guess->setVisible( true );

			cl_entity_t *pPlayer = gEngfuncs.GetLocalPlayer();
			if ( pPlayer && ( iWrongUser || iWrongPassword ) )
				gEngfuncs.pEventAPI->EV_PlaySound( -1, pPlayer->curstate.origin, CHAN_AUTO, "buttons/computer_deny.wav", 0.7, 0.8, 0, PITCH_NORM );

			gViewPort->m_pComputer_guess->m_pText_guess->setVisible( false );

			if ( iWrongUser || iWrongPassword )
			{
				gViewPort->m_pComputer_guess->m_pUser_guess->setText( "Invalid username or password!", 29 );
				gViewPort->m_pComputer_guess->m_pPassword_guess->setText( "Invalid password or password!", 29 );
			}
		}
	}

	return 1;
}

void VGUI_ComputerInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "Computer", MsgComputer_guess );
}

void CComputer::setVisible( bool state )
{
	if ( state )
	{
		m_flNextKeyTime_guess = gEngfuncs.GetClientTime();
		m_pUser_guess->setText( "Enter a username...", 19 );
		m_pPassword_guess->setText( "Enter a password...", 19 );

		if ( m_iLoggedOn_guess )
		{
			m_pUser_guess->setVisible( false );
			m_pPassword_guess->setVisible( false );
		}
		else
		{
			m_pUser_guess->setVisible( true );
			m_pPassword_guess->setVisible( true );
		}
	}
	else
	{
		m_pUser_guess->setText( "Enter a username...", 19 );
		m_pPassword_guess->setText( "Enter a password...", 19 );

		if ( m_iLoggedOn_guess )
		{
			m_pUser_guess->setVisible( false );
			m_pPassword_guess->setVisible( false );
		}
		else
		{
			m_pUser_guess->setVisible( true );
			m_pPassword_guess->setVisible( true );
		}

		gEngfuncs.pfnServerCmd( "stopcomputer" );
	}

	Panel::setVisible( state );
}

void CComputer::solve()
{
	char szUser[256];
	char szPassword[256];

	if ( isVisible() && m_pUser_guess->isVisible() && m_pPassword_guess->isVisible() && !m_iLoggedOn_guess )
	{
		m_pUser_guess->getText( 0, szUser, 256 );
		m_pPassword_guess->getText( 0, szPassword, 256 );

		if ( ( GetAsyncKeyState( VK_RETURN ) & 0x8000 ) && gEngfuncs.GetClientTime() > m_flNextKeyTime_guess )
		{
			char szUser2[256];
			char szPassword2[256];
			char szCommand[128];

			m_flNextKeyTime_guess = gEngfuncs.GetClientTime() + 1;
			m_pUser_guess->getText( 0, szUser2, 256 );
			m_pPassword_guess->getText( 0, szPassword2, 256 );
			sprintf( szCommand, "computercheck %s %s", szUser2, szPassword2 );
			gEngfuncs.pfnServerCmd( szCommand );
		}

		if ( m_pUser_guess->hasFocus() )
		{
			if ( !strcmp( szUser, "Enter a username..." ) )
				m_pUser_guess->setText( "", 0 );

			if ( ( GetAsyncKeyState( VK_TAB ) & 0x8000 ) && gEngfuncs.GetClientTime() > m_flNextKeyTime_guess )
			{
				m_flNextKeyTime_guess = gEngfuncs.GetClientTime() + 1;
				m_pPassword_guess->requestFocus();
			}
		}

		if ( m_pPassword_guess->hasFocus() )
		{
			if ( !strcmp( szPassword, "Enter a password..." ) )
				m_pPassword_guess->setText( "", 0 );

			if ( ( GetAsyncKeyState( VK_TAB ) & 0x8000 ) && gEngfuncs.GetClientTime() > m_flNextKeyTime_guess )
			{
				m_flNextKeyTime_guess = gEngfuncs.GetClientTime() + 1;
				m_pUser_guess->requestFocus();
			}
		}

		cl_entity_t *pPlayer = gEngfuncs.GetLocalPlayer();
		int iPitch = PITCH_NORM - gEngfuncs.pfnRandomLong( -2, 2 );

		if ( m_iUserLength_guess != strlen( szUser ) )
		{
			if ( pPlayer )
				gEngfuncs.pEventAPI->EV_PlaySound( -1, pPlayer->curstate.origin, CHAN_BODY, "buttons/keyboard_type.wav", 0.7, 0.8, 0, iPitch );

			m_iUserLength_guess = strlen( szUser );
		}

		if ( m_iPasswordLength_guess != strlen( szPassword ) )
		{
			if ( pPlayer )
				gEngfuncs.pEventAPI->EV_PlaySound( -1, pPlayer->curstate.origin, CHAN_BODY, "buttons/keyboard_type.wav", 0.7, 0.8, 0, iPitch );

			m_iPasswordLength_guess = strlen( szPassword );
		}
	}

	Panel::solve();
}

CComputer::~CComputer( void )
{
	delete m_pScreenImage_guess;
}
