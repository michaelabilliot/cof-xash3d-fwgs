// ======================================
// Cry of Fear book menu
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "com_model.h"
#include "vgui_TeamFortressViewport.h"
#include "BookMenu_guess.h"
#include "Unlockables_guess.h"
#include "..\game_shared\vgui_loadtga.h"

#define SHIELD_ON	0
#define SHIELD_OFF	1

extern cl_entity_t g_HeadShieldEnt;
extern int g_HeadShieldState;
extern int g_iHeadShieldType_guess;

CBookMenu::CBookMenu() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int wide, tall;
	int x, y;
	CommandButton *pButton;

	m_pCancelImage_guess = vgui_LoadTGA( "gfx/vgui/640_notecancel.tga" );
	if ( m_pCancelImage_guess )
		m_pCancelImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/640_notecancel.tga!\n" );

	m_pArrowsImage_guess = vgui_LoadTGA( "gfx/vgui/left_right.tga" );
	if ( m_pArrowsImage_guess )
		m_pArrowsImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/left_right.tga!\n" );

	// the cancel picture and its button are placed at (wide, tall)
	m_pCancelImage_guess->getSize( wide, tall );

	m_pCancel_guess = new ImagePanel( m_pCancelImage_guess );
	m_pCancel_guess->setParent( this );
	m_pCancel_guess->setPos( wide, tall );
	m_pCancel_guess->setVisible( true );

	pButton = new CommandButton( "", wide, tall, wide, tall, true, true );
	pButton->setParent( this );
	pButton->addActionSignal( new CBookMenuHandler_Command( this, 3 ) );

	m_pArrowsImage_guess->getSize( wide, tall );
	x = ScreenWidth / 2 - wide / 2;
	y = ScreenHeight - tall * 2;

	m_pArrows_guess = new ImagePanel( m_pArrowsImage_guess );
	m_pArrows_guess->setParent( this );
	m_pArrows_guess->setPos( x, y );
	m_pArrows_guess->setVisible( true );

	pButton = new CommandButton( "", x, y, 100, 50, true, true );
	pButton->setParent( this );
	pButton->addActionSignal( new CBookMenuHandler_Command( this, 1 ) );

	pButton = new CommandButton( "", x + 100, y, 100, 50, true, true );
	pButton->setParent( this );
	pButton->addActionSignal( new CBookMenuHandler_Command( this, 2 ) );

	setBgColor( 0, 0, 0, 255 );

	Initialize();
}

CBookMenu::~CBookMenu()
{
	delete m_pArrowsImage_guess;
	delete m_pCancelImage_guess;
}

void CBookMenu::ActionSignal( int cmd )
{
	switch ( cmd )
	{
	case 1:
		if ( gHUD.m_bMenuClick_guess && g_iHeadShieldType_guess == 7 && gEngfuncs.GetClientTime() > gHUD.m_flMenuClickTime_guess )
		{
			switch ( g_HeadShieldEnt.curstate.sequence )
			{
			case 2:
				g_HeadShieldEnt.curstate.animtime = gEngfuncs.GetClientTime();
				g_HeadShieldEnt.curstate.sequence = 7;
				gHUD.m_flMenuClickTime_guess = gEngfuncs.GetClientTime() + 1;
				gEngfuncs.pfnPlaySoundByName( "common/page_turn.wav", 1 );
				break;
			case 4:
				g_HeadShieldEnt.curstate.animtime = gEngfuncs.GetClientTime();
				g_HeadShieldEnt.curstate.sequence = 6;
				gHUD.m_flMenuClickTime_guess = gEngfuncs.GetClientTime() + 1;
				gEngfuncs.pfnPlaySoundByName( "common/page_turn.wav", 1 );
				break;
			case 6:
				g_HeadShieldEnt.curstate.animtime = gEngfuncs.GetClientTime();
				g_HeadShieldEnt.curstate.sequence = 7;
				gHUD.m_flMenuClickTime_guess = gEngfuncs.GetClientTime() + 1;
				gEngfuncs.pfnPlaySoundByName( "common/page_turn.wav", 1 );
				break;
			}
		}
		break;
	case 2:
		if ( gHUD.m_bMenuClick_guess && g_iHeadShieldType_guess == 7 && gEngfuncs.GetClientTime() > gHUD.m_flMenuClickTime_guess )
		{
			switch ( g_HeadShieldEnt.curstate.sequence )
			{
			case 0:
				g_HeadShieldEnt.curstate.animtime = gEngfuncs.GetClientTime();
				g_HeadShieldEnt.curstate.sequence = 2;
				gHUD.m_flMenuClickTime_guess = gEngfuncs.GetClientTime() + 1;
				gEngfuncs.pfnPlaySoundByName( "common/page_turn.wav", 1 );
				break;
			case 1:
				g_HeadShieldEnt.curstate.animtime = gEngfuncs.GetClientTime();
				g_HeadShieldEnt.curstate.sequence = 2;
				gHUD.m_flMenuClickTime_guess = gEngfuncs.GetClientTime() + 1;
				gEngfuncs.pfnPlaySoundByName( "common/page_turn.wav", 1 );
				break;
			case 2:
				g_HeadShieldEnt.curstate.animtime = gEngfuncs.GetClientTime();
				g_HeadShieldEnt.curstate.sequence = 4;
				gHUD.m_flMenuClickTime_guess = gEngfuncs.GetClientTime() + 1;
				gEngfuncs.pfnPlaySoundByName( "common/page_turn.wav", 1 );
				break;
			case 6:
				g_HeadShieldEnt.curstate.animtime = gEngfuncs.GetClientTime();
				g_HeadShieldEnt.curstate.sequence = 4;
				gHUD.m_flMenuClickTime_guess = gEngfuncs.GetClientTime() + 1;
				gEngfuncs.pfnPlaySoundByName( "common/page_turn.wav", 1 );
				break;
			case 7:
				g_HeadShieldEnt.curstate.animtime = gEngfuncs.GetClientTime();
				g_HeadShieldEnt.curstate.sequence = 2;
				gHUD.m_flMenuClickTime_guess = gEngfuncs.GetClientTime() + 1;
				gEngfuncs.pfnPlaySoundByName( "common/page_turn.wav", 1 );
				break;
			}
		}
		break;
	case 3:
		if ( gViewPort && gViewPort->m_pUnlockables_guess )
		{
			setVisible( false );
			gViewPort->UpdateCursorState();

			gHUD.m_bMenuClick_guess = false;
			g_iHeadShieldType_guess = 0;
			g_HeadShieldState = SHIELD_OFF;

			gViewPort->m_pUnlockables_guess->setVisible( true );
			gViewPort->UpdateCursorState();
			gEngfuncs.pfnPlaySoundByName( "ui/3d_click.wav", 1 );
		}
		break;
	}
}

void CBookMenu::Initialize()
{
	setVisible( false );
}

void CBookMenu::setVisible( bool state )
{
	if ( state )
	{
		if ( g_iHeadShieldType_guess != 7 )
			g_iHeadShieldType_guess = 7;

		g_HeadShieldState = SHIELD_ON;
		gHUD.m_bMenuClick_guess = true;
		gEngfuncs.pfnPlaySoundByName( "common/page_turn.wav", 1 );
		g_HeadShieldEnt.curstate.animtime = gEngfuncs.GetClientTime();
		g_HeadShieldEnt.curstate.sequence = 0;
	}

	Panel::setVisible( state );
}
