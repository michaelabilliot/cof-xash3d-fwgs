// ======================================
// Cry of Fear animal statue puzzle panel
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "AnimalStatue_guess.h"
#include "..\game_shared\vgui_loadtga.h"

CAnimalStatue::CAnimalStatue() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int wide, tall;
	int x, y;
	CommandButton *pButton;

	m_pBlackPanel_guess = new Panel( 0, 0, ScreenWidth, ScreenHeight );
	m_pBlackPanel_guess->setParent( this );
	m_pBlackPanel_guess->setBgColor( 0, 0, 0, 0 );

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

	m_pButtonsImage_guess = vgui_LoadTGA( "gfx/vgui/640_eagle_button.tga" );
	if ( m_pButtonsImage_guess )
		m_pButtonsImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/640_eagle_button.tga!\n" );

	x = ScreenWidth / 2 - 320;
	y = ScreenHeight / 2 - 240;

	m_pButtons_guess = new ImagePanel( m_pButtonsImage_guess );
	m_pButtons_guess->setParent( this );
	m_pButtons_guess->setPos( x, y );
	m_pButtons_guess->setVisible( true );

	pButton = new CommandButton( "", 0, 0, 640, 480, true, true );
	pButton->setParent( m_pButtons_guess );
	pButton->addActionSignal( new CAnimalStatueHandler_Command( this, 1 ) );

	pButton = new CommandButton( "", 290, 210, 60, 60, true, true );
	pButton->setParent( m_pButtons_guess );
	pButton->addActionSignal( new CAnimalStatueHandler_Command( this, 2 ) );

	pButton = new CommandButton( "", 290, 330, 60, 60, true, true );
	pButton->setParent( m_pButtons_guess );
	pButton->addActionSignal( new CAnimalStatueHandler_Command( this, 4 ) );

	pButton = new CommandButton( "", 230, 270, 60, 60, true, true );
	pButton->setParent( m_pButtons_guess );
	pButton->addActionSignal( new CAnimalStatueHandler_Command( this, 5 ) );

	pButton = new CommandButton( "", 350, 270, 60, 60, true, true );
	pButton->setParent( m_pButtons_guess );
	pButton->addActionSignal( new CAnimalStatueHandler_Command( this, 3 ) );

	setBgColor( 0, 0, 0, 255 );

	Initialize();
}

CAnimalStatue::~CAnimalStatue()
{
	delete m_pButtonsImage_guess;
	delete m_pBackgroundImage_guess;
}

void CAnimalStatue::ActionSignal( int cmd )
{
	char szCommand[128];

	if ( !m_iStatue_guess )
		return;

	switch ( cmd )
	{
	case 0:
		break;
	case 1:
		setVisible( false );
		gViewPort->UpdateCursorState();
		break;
	case 2:
		sprintf( szCommand, "movestatue 0 %i", m_iStatue_guess );
		gEngfuncs.pfnServerCmd( szCommand );
		gEngfuncs.pfnPlaySoundByName( "statue_puzzle/statue_switch.wav", 1 );
		break;
	case 3:
		sprintf( szCommand, "movestatue 1 %i", m_iStatue_guess );
		gEngfuncs.pfnServerCmd( szCommand );
		gEngfuncs.pfnPlaySoundByName( "statue_puzzle/statue_switch.wav", 1 );
		break;
	case 4:
		sprintf( szCommand, "movestatue 2 %i", m_iStatue_guess );
		gEngfuncs.pfnServerCmd( szCommand );
		gEngfuncs.pfnPlaySoundByName( "statue_puzzle/statue_switch.wav", 1 );
		break;
	case 5:
		sprintf( szCommand, "movestatue 3 %i", m_iStatue_guess );
		gEngfuncs.pfnServerCmd( szCommand );
		gEngfuncs.pfnPlaySoundByName( "statue_puzzle/statue_switch.wav", 1 );
		break;
	default:
		gEngfuncs.Con_DPrintf( "Unknown AnimalStatue ActionSignal %i.\n", cmd );
		break;
	}
}

void CAnimalStatue::Initialize()
{
	setVisible( false );
	m_iStatue_guess = 0;
}

int CAnimalStatue::KeyInput( int down, int keynum, const char *pszCurrentBinding )
{
	if ( pszCurrentBinding && ( !strcmp( pszCurrentBinding, "toggleconsole" ) || !strcmp( pszCurrentBinding, "movestatue" ) ) )
		return 1;

	return 0;
}

int MsgStatue_guess( const char *pszName, int iSize, void *pbuf )
{
	char szImage[128];

	BEGIN_READ( pbuf, iSize );

	if ( gViewPort->m_pAnimalStatue_guess )
	{
		gViewPort->m_pAnimalStatue_guess->m_iStatue_guess = READ_BYTE();

		if ( !gViewPort->m_pAnimalStatue_guess->m_iStatue_guess )
		{
			if ( gViewPort->m_pAnimalStatue_guess->isVisible() )
				gViewPort->m_pAnimalStatue_guess->setVisible( false );

			gViewPort->UpdateCursorState();
			return 1;
		}

		sprintf( szImage, "%s", READ_STRING() );

		if ( strlen( szImage ) > 1 )
		{
			if ( vgui_LoadTGA( szImage ) )
			{
				gViewPort->m_pAnimalStatue_guess->m_pButtons_guess->setImage( vgui_LoadTGA( szImage ) );

				if ( !gViewPort->m_pAnimalStatue_guess->isVisible() )
				{
					gViewPort->m_pAnimalStatue_guess->setVisible( true );
					gViewPort->UpdateCursorState();
				}
			}
			else
			{
				gEngfuncs.Con_Printf( "Cannot load %s!\n", szImage );
			}
		}
		else
		{
			gEngfuncs.Con_Printf( "No message sent\n" );
		}
	}

	return 1;
}

void StatueInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "Statue", MsgStatue_guess );
}
