// ======================================
// Cry of Fear subtitle language menu
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "vgui_TeamFortressViewport.h"
#include "Language_guess.h"
#include "..\game_shared\vgui_loadtga.h"
#include "event_api.h"

CLanguage::CLanguage() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int x, y;
	CommandButton *pButton;

	m_pBackgroundImage_guess = vgui_LoadTGA( "gfx/vgui/language_settings.tga" );
	if ( m_pBackgroundImage_guess )
		m_pBackgroundImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/language_settings.tga!\n" );

	x = ScreenWidth / 2 - 320;
	y = ScreenHeight / 2 - 240;

	m_pBackground_guess = new ImagePanel( m_pBackgroundImage_guess );
	m_pBackground_guess->setParent( this );
	m_pBackground_guess->setPos( x, y );
	m_pBackground_guess->setVisible( true );

	pButton = new CommandButton( "", 83, 64, 158, 117, true, true );
	pButton->setParent( m_pBackground_guess );
	pButton->addActionSignal( new CLanguageHandler_Command( this, 1 ) );

	pButton = new CommandButton( "", 241, 64, 158, 117, true, true );
	pButton->setParent( m_pBackground_guess );
	pButton->addActionSignal( new CLanguageHandler_Command( this, 7 ) );

	pButton = new CommandButton( "", 399, 64, 158, 117, true, true );
	pButton->setParent( m_pBackground_guess );
	pButton->addActionSignal( new CLanguageHandler_Command( this, 5 ) );

	pButton = new CommandButton( "", 83, 181, 158, 117, true, true );
	pButton->setParent( m_pBackground_guess );
	pButton->addActionSignal( new CLanguageHandler_Command( this, 2 ) );

	pButton = new CommandButton( "", 241, 181, 158, 117, true, true );
	pButton->setParent( m_pBackground_guess );
	pButton->addActionSignal( new CLanguageHandler_Command( this, 3 ) );

	pButton = new CommandButton( "", 399, 181, 158, 117, true, true );
	pButton->setParent( m_pBackground_guess );
	pButton->addActionSignal( new CLanguageHandler_Command( this, 4 ) );

	pButton = new CommandButton( "", 83, 298, 158, 117, true, true );
	pButton->setParent( m_pBackground_guess );
	pButton->addActionSignal( new CLanguageHandler_Command( this, 6 ) );

	setBgColor( 0, 0, 0, 255 );

	Initialize();
}

void CLanguage::ActionSignal( int cmd )
{
	cl_entity_t *pPlayer = gEngfuncs.GetLocalPlayer();

	if ( !pPlayer )
		return;

	switch ( cmd )
	{
	case 0:
		break;
	case 1:
		gEngfuncs.pfnServerCmd( "subtitleset 1" );
		setVisible( false );
		gViewPort->ShowVGUIMenu( 37 );
		gEngfuncs.pEventAPI->EV_PlaySound( -1, pPlayer->curstate.origin, CHAN_WEAPON, "ui/3d_click.wav", 1.0, 0.8, 0, PITCH_NORM );
		gViewPort->UpdateCursorState();
		break;
	case 2:
		gEngfuncs.pfnServerCmd( "subtitleset 2" );
		setVisible( false );
		gViewPort->ShowVGUIMenu( 37 );
		gEngfuncs.pEventAPI->EV_PlaySound( -1, pPlayer->curstate.origin, CHAN_WEAPON, "ui/3d_click.wav", 1.0, 0.8, 0, PITCH_NORM );
		gViewPort->UpdateCursorState();
		break;
	case 3:
		gEngfuncs.pfnServerCmd( "subtitleset 3" );
		setVisible( false );
		gViewPort->ShowVGUIMenu( 37 );
		gEngfuncs.pEventAPI->EV_PlaySound( -1, pPlayer->curstate.origin, CHAN_WEAPON, "ui/3d_click.wav", 1.0, 0.8, 0, PITCH_NORM );
		gViewPort->UpdateCursorState();
		break;
	case 4:
		gEngfuncs.pfnServerCmd( "subtitleset 4" );
		setVisible( false );
		gViewPort->ShowVGUIMenu( 37 );
		gEngfuncs.pEventAPI->EV_PlaySound( -1, pPlayer->curstate.origin, CHAN_WEAPON, "ui/3d_click.wav", 1.0, 0.8, 0, PITCH_NORM );
		gViewPort->UpdateCursorState();
		break;
	case 5:
		gEngfuncs.pfnServerCmd( "subtitleset 5" );
		setVisible( false );
		gViewPort->ShowVGUIMenu( 37 );
		gEngfuncs.pEventAPI->EV_PlaySound( -1, pPlayer->curstate.origin, CHAN_WEAPON, "ui/3d_click.wav", 1.0, 0.8, 0, PITCH_NORM );
		gViewPort->UpdateCursorState();
		break;
	case 6:
		gEngfuncs.pfnServerCmd( "subtitleset 6" );
		setVisible( false );
		gViewPort->ShowVGUIMenu( 37 );
		gEngfuncs.pEventAPI->EV_PlaySound( -1, pPlayer->curstate.origin, CHAN_WEAPON, "ui/3d_click.wav", 1.0, 0.8, 0, PITCH_NORM );
		gViewPort->UpdateCursorState();
		break;
	case 7:
		gEngfuncs.pfnServerCmd( "subtitleset 7" );
		setVisible( false );
		gViewPort->ShowVGUIMenu( 37 );
		gEngfuncs.pEventAPI->EV_PlaySound( -1, pPlayer->curstate.origin, CHAN_WEAPON, "ui/3d_click.wav", 1.0, 0.8, 0, PITCH_NORM );
		gViewPort->UpdateCursorState();
		break;
	default:
		gEngfuncs.Con_DPrintf( "Unknown Language ActionSignal %i.\n", cmd );
		break;
	}
}

void CLanguage::Initialize()
{
	setVisible( false );
}

CLanguage::~CLanguage( void )
{
	delete m_pBackgroundImage_guess;
}
