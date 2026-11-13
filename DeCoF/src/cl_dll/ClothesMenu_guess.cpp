// ======================================
// Cry of Fear clothes menu
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "ClothesMenu_guess.h"
#include "HUDControl_guess.h"

// the custom costume textures are loaded again when these are cleared (studio renderer)
extern bool g_bHoodieLoaded_guess;
extern bool g_bSleeveLoaded_guess;
extern bool g_bJeansLoaded_guess;
extern bool g_bFaceLoaded_guess;

void UseCustomHoodie_guess();

CClothesMenu::CClothesMenu() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	ColorButton *pButton;
	int y;
	int i;
	int tall = ScreenHeight / 12;
	int wide = ScreenWidth / 2;

	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();

	for ( i = 0, y = 0; i <= 12; i++, y += tall )
	{
		m_pClothesButton_guess[i] = new ColorButton( "", ScreenWidth / 2, y, wide, tall, false, false );
		m_pClothesButton_guess[i]->setParent( this );
		m_pClothesButton_guess[i]->addActionSignal( new CClothesMenuHandler_Command( this, i + 1 ) );
		m_pClothesButton_guess[i]->setFont( pSchemes->getFont( pSchemes->getSchemeHandle( "credits" ) ) );
		m_pClothesButton_guess[i]->setUnArmedBorderColor( 255, 255, 255, 200 );
		m_pClothesButton_guess[i]->setUnArmedColor( 255, 255, 255, 0 );
		m_pClothesButton_guess[i]->setArmedBorderColor( 255, 255, 255, 150 );
		m_pClothesButton_guess[i]->setArmedColor( 255, 0, 0, 32 );
		m_pClothesButton_guess[i]->setBgColor( 0, 0, 0, 128 );
		m_pClothesButton_guess[i]->setFgColor( 255, 255, 255, 128 );
	}

	pButton = new ColorButton( "Close", 0, ScreenHeight - tall, wide / 2, tall, false, false );
	pButton->setParent( this );
	pButton->addActionSignal( new CClothesMenuHandler_Command( this, 50 ) );
	pButton->setFont( pSchemes->getFont( pSchemes->getSchemeHandle( "credits" ) ) );
	pButton->setUnArmedBorderColor( 255, 255, 255, 200 );
	pButton->setUnArmedColor( 255, 255, 255, 0 );
	pButton->setArmedBorderColor( 255, 255, 255, 150 );
	pButton->setArmedColor( 255, 0, 0, 32 );
	pButton->setBgColor( 0, 0, 0, 128 );
	pButton->setFgColor( 255, 255, 255, 128 );

	pButton = new ColorButton( "Reset", wide / 2, ScreenHeight - tall, wide / 2, tall, false, false );
	pButton->setParent( this );
	pButton->addActionSignal( new CClothesMenuHandler_Command( this, 66 ) );
	pButton->setFont( pSchemes->getFont( pSchemes->getSchemeHandle( "credits" ) ) );
	pButton->setUnArmedBorderColor( 255, 255, 255, 200 );
	pButton->setUnArmedColor( 255, 255, 255, 0 );
	pButton->setArmedBorderColor( 255, 255, 255, 150 );
	pButton->setArmedColor( 255, 0, 0, 32 );
	pButton->setBgColor( 0, 0, 0, 128 );
	pButton->setFgColor( 255, 255, 255, 128 );

	setBgColor( 0, 0, 0, 255 );

	Initialize();
}

void CClothesMenu::ActionSignal( int cmd )
{
	if ( cmd != 50 && gEngfuncs.GetClientTime() < m_flNextClick_guess )
		return;

	switch ( cmd )
	{
	case 0:
		break;
	case 1:
		if ( m_iUnlocked_guess & 1 )
		{
			gHUD.m_iClothesState_guess = 1;
			g_bHoodieLoaded_guess = false;
			g_bJeansLoaded_guess = false;
			g_bSleeveLoaded_guess = false;
			g_bFaceLoaded_guess = false;
			gEngfuncs.pfnServerCmd( "setclothes 1" );
		}
		break;
	case 2:
		if ( m_iUnlocked_guess & 2 )
		{
			gHUD.m_iClothesState_guess = 2;
			g_bHoodieLoaded_guess = false;
			g_bJeansLoaded_guess = false;
			g_bSleeveLoaded_guess = false;
			g_bFaceLoaded_guess = false;
			gEngfuncs.pfnServerCmd( "setclothes 2" );
		}
		break;
	case 3:
		if ( m_iUnlocked_guess & 4 )
		{
			gHUD.m_iClothesState_guess = 3;
			g_bHoodieLoaded_guess = false;
			g_bSleeveLoaded_guess = false;
			g_bJeansLoaded_guess = false;
			g_bFaceLoaded_guess = false;
			gEngfuncs.pfnServerCmd( "setclothes 3" );
		}
		break;
	case 4:
		if ( m_iUnlocked_guess & 8 )
		{
			gHUD.m_iClothesState_guess = 4;
			g_bHoodieLoaded_guess = false;
			g_bSleeveLoaded_guess = false;
			g_bJeansLoaded_guess = false;
			g_bFaceLoaded_guess = false;
			gEngfuncs.pfnServerCmd( "setclothes 4" );
		}
		break;
	case 5:
		if ( m_iUnlocked_guess & 0x10 )
		{
			gHUD.m_iClothesState_guess = 5;
			g_bHoodieLoaded_guess = false;
			g_bSleeveLoaded_guess = false;
			g_bJeansLoaded_guess = false;
			g_bFaceLoaded_guess = false;
			gEngfuncs.pfnServerCmd( "setclothes 5" );
		}
		break;
	case 6:
		if ( m_iUnlocked_guess & 0x20 )
		{
			gHUD.m_iClothesState_guess = 6;
			g_bHoodieLoaded_guess = false;
			g_bSleeveLoaded_guess = false;
			g_bJeansLoaded_guess = false;
			g_bFaceLoaded_guess = false;
			gEngfuncs.pfnServerCmd( "setclothes 6" );
		}
		break;
	case 7:
		if ( m_iUnlocked_guess & 0x40 )
		{
			gHUD.m_iClothesState_guess = 7;
			g_bHoodieLoaded_guess = false;
			g_bSleeveLoaded_guess = false;
			g_bJeansLoaded_guess = false;
			g_bFaceLoaded_guess = false;
			gEngfuncs.pfnServerCmd( "setclothes 7" );
		}
		break;
	case 8:
		if ( m_iUnlocked_guess & 0x80 )
		{
			gHUD.m_iClothesState_guess = 8;
			g_bHoodieLoaded_guess = false;
			g_bSleeveLoaded_guess = false;
			g_bJeansLoaded_guess = false;
			g_bFaceLoaded_guess = false;
			gEngfuncs.pfnServerCmd( "setclothes 8" );
		}
		break;
	case 9:
		if ( m_iUnlocked_guess & 0x100 )
		{
			gHUD.m_iClothesState_guess = 9;
			g_bHoodieLoaded_guess = false;
			g_bSleeveLoaded_guess = false;
			g_bJeansLoaded_guess = false;
			g_bFaceLoaded_guess = false;
			gEngfuncs.pfnServerCmd( "setclothes 9" );
		}
		break;
	case 10:
		if ( m_iUnlocked_guess & 0x200 )
		{
			gHUD.m_iClothesState_guess = 10;
			g_bHoodieLoaded_guess = false;
			g_bSleeveLoaded_guess = false;
			g_bJeansLoaded_guess = false;
			g_bFaceLoaded_guess = false;
			gEngfuncs.pfnServerCmd( "setclothes 10" );
		}
		break;
	case 11:
		if ( m_iUnlocked_guess & 0x400 )
		{
			gHUD.m_iClothesState_guess = 11;
			g_bHoodieLoaded_guess = false;
			g_bSleeveLoaded_guess = false;
			g_bFaceLoaded_guess = false;
			g_bJeansLoaded_guess = false;
			gEngfuncs.pfnServerCmd( "setclothes 11" );
		}
		break;
	case 12:
		if ( m_iUnlocked_guess & 0x800 )
		{
			UseCustomHoodie_guess();
			m_flNextClick_guess = gEngfuncs.GetClientTime() + 0.5;
		}
		break;
	case 50:
		setVisible( false );
		gViewPort->UpdateCursorState();
		gEngfuncs.pfnServerCmd( "closeclothes" );
		break;
	case 66:
		gHUD.m_iClothesState_guess = 0;
		g_bHoodieLoaded_guess = false;
		g_bSleeveLoaded_guess = false;
		g_bJeansLoaded_guess = false;
		g_bFaceLoaded_guess = false;
		gEngfuncs.pfnServerCmd( "setclothes 0" );
		break;
	default:
		gEngfuncs.Con_DPrintf( "Unknown clothesmenu ActionSignal %i.\n", cmd );
		break;
	}
}

void CClothesMenu::Initialize()
{
	m_iUnlocked_guess = 0;
	setVisible( false );
}

int CClothesMenu::KeyInput_guess( int down, int keynum, const char *pszCurrentBinding )
{
	if ( pszCurrentBinding && !strcmp( pszCurrentBinding, "toggleconsole" ) )
		return 1;

	return 0;
}

int MsgClothes_guess( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	if ( gViewPort->m_pClothesMenu_guess )
	{
		gViewPort->m_pClothesMenu_guess->m_iUnlocked_guess = READ_LONG();
		gHUD.m_iClothes_guess = READ_BYTE();
		gHUD.m_iClothesState_guess = READ_BYTE();
		int iShow = READ_BYTE();

		gViewPort->m_pClothesMenu_guess->UpdateButtons_guess();

		if ( iShow )
		{
			gViewPort->m_pClothesMenu_guess->setVisible( true );
			gViewPort->UpdateCursorState();
			gViewPort->m_pClothesMenu_guess->m_flNextClick_guess = gEngfuncs.GetClientTime() + 0.5;
		}
	}

	return 1;
}

void CClothesMenu::UpdateButtons_guess()
{
	if ( m_iUnlocked_guess & 1 )
		m_pClothesButton_guess[0]->setText( "David Leatherhoff suit" );
	else
		m_pClothesButton_guess[0]->setText( "Locked" );
	m_pClothesButton_guess[0]->setContentAlignment( Label::a_center );

	if ( m_iUnlocked_guess & 2 )
		m_pClothesButton_guess[1]->setText( "ModDB hoodie" );
	else
		m_pClothesButton_guess[1]->setText( "Locked" );
	m_pClothesButton_guess[1]->setContentAlignment( Label::a_center );

	if ( m_iUnlocked_guess & 4 )
		m_pClothesButton_guess[2]->setText( "Hello Kitty suit" );
	else
		m_pClothesButton_guess[2]->setText( "Locked" );
	m_pClothesButton_guess[2]->setContentAlignment( Label::a_center );

	if ( m_iUnlocked_guess & 8 )
		m_pClothesButton_guess[3]->setText( "Afraid of Monsters suit" );
	else
		m_pClothesButton_guess[3]->setText( "Locked" );
	m_pClothesButton_guess[3]->setContentAlignment( Label::a_center );

	if ( m_iUnlocked_guess & 0x10 )
		m_pClothesButton_guess[4]->setText( "Camouflage hoodie" );
	else
		m_pClothesButton_guess[4]->setText( "Locked" );
	m_pClothesButton_guess[4]->setContentAlignment( Label::a_center );

	if ( m_iUnlocked_guess & 0x20 )
		m_pClothesButton_guess[5]->setText( "Half-Life Creations hoodie" );
	else
		m_pClothesButton_guess[5]->setText( "Locked" );
	m_pClothesButton_guess[5]->setContentAlignment( Label::a_center );

	if ( m_iUnlocked_guess & 0x40 )
		m_pClothesButton_guess[6]->setText( "Black Metal suit" );
	else
		m_pClothesButton_guess[6]->setText( "Locked" );
	m_pClothesButton_guess[6]->setContentAlignment( Label::a_center );

	if ( m_iUnlocked_guess & 0x80 )
		m_pClothesButton_guess[7]->setText( "Team Psykskallar hoodie" );
	else
		m_pClothesButton_guess[7]->setText( "Locked" );
	m_pClothesButton_guess[7]->setContentAlignment( Label::a_center );

	if ( m_iUnlocked_guess & 0x100 )
		m_pClothesButton_guess[8]->setText( "Awesome suit" );
	else
		m_pClothesButton_guess[8]->setText( "Locked" );
	m_pClothesButton_guess[8]->setContentAlignment( Label::a_center );

	if ( m_iUnlocked_guess & 0x200 )
		m_pClothesButton_guess[9]->setText( "Sick Simon suit" );
	else
		m_pClothesButton_guess[9]->setText( "Locked" );
	m_pClothesButton_guess[9]->setContentAlignment( Label::a_center );

	if ( m_iUnlocked_guess & 0x400 )
		m_pClothesButton_guess[10]->setText( "AoM Twitcher suit" );
	else
		m_pClothesButton_guess[10]->setText( "Locked" );
	m_pClothesButton_guess[10]->setContentAlignment( Label::a_center );

	if ( m_iUnlocked_guess & 0x800 )
		m_pClothesButton_guess[11]->setText( "Custom hoodie" );
	else
		m_pClothesButton_guess[11]->setText( "Locked" );
	m_pClothesButton_guess[11]->setContentAlignment( Label::a_center );

}

void UseCustomHoodie_guess()
{
	char szCommand[256];

	if ( gEngfuncs.COM_LoadFile( "models/costumes/custom_hoodie.tga", 5, NULL ) )
	{
		gHUD.m_iClothesState_guess = 12;
		g_bHoodieLoaded_guess = false;
		g_bJeansLoaded_guess = false;
		g_bSleeveLoaded_guess = false;
		g_bFaceLoaded_guess = false;
		sprintf( szCommand, "setclothes 12 %s", "custom_hoodie.tga" );
		gEngfuncs.pfnServerCmd( szCommand );
	}
	else
	{
		strcpy( gViewPort->m_pHUDControl_guess->m_szMessage_guess, "To use custom hoodies place a .tga called custom_hoodie.tga in the models\\costumes folder" );
		gViewPort->m_pHUDControl_guess->m_flMessageAlpha_guess = 255;
		gViewPort->m_pHUDControl_guess->m_bMessageFadeOut_guess = false;
		gViewPort->m_pHUDControl_guess->m_bMessageHold_guess = false;
		gViewPort->m_pHUDControl_guess->m_flMessageHoldTime_guess = 0;
		gViewPort->m_pHUDControl_guess->m_iMessageType_guess = 5;
	}
}

void VGUI_ClothesInit_guess()
{
	gHUD.m_iClothesState_guess = 0;
	gEngfuncs.pfnHookUserMsg( "Clothes", MsgClothes_guess );
}

CClothesMenu::~CClothesMenu( void )
{
}
