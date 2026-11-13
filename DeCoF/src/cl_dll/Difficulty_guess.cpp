// ======================================
// Cry of Fear difficulty selection panel
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "vgui_TeamFortressViewport.h"
#include "..\game_shared\vgui_checkbutton2.h"
#include "Difficulty_guess.h"
#include "GameMenu_guess.h"
#include "..\game_shared\vgui_loadtga.h"

extern int iMenuMode_guess;

void IrrKlangStop_guess( float flFadeTime );

CDifficulty::CDifficulty() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int wide, tall;
	int x, y;
	ColorButton *pButton;

	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );

	x = ScreenWidth / 2 - 320;
	y = ScreenHeight / 2 - 240;

	m_pImage_guess = vgui_LoadTGA( "gfx/vgui/640_difficulty.tga" );
	if ( m_pImage_guess )
		m_pImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/640_difficulty.tga!\n" );

	m_pImagePanel_guess = new ImagePanel( m_pImage_guess );
	m_pImagePanel_guess->setParent( this );
	m_pImagePanel_guess->setPos( x, y );
	m_pImagePanel_guess->setVisible( true );

	m_CommentaryButton_guess.setParent( this );
	m_CommentaryButton_guess.SetImages( "gfx/vgui/checked.tga", "gfx/vgui/unchecked.tga" );
	m_CommentaryButton_guess.SetText( "Enable developer commentary" );
	m_CommentaryButton_guess.SetCheckboxLeft( true );
	m_CommentaryButton_guess.SetChecked( gEngfuncs.pfnGetCvarFloat( "dev_commentary" ) != 0 );
	m_CommentaryButton_guess.SetHandler( this );
	m_CommentaryButton_guess.getSize( wide, tall );
	m_CommentaryButton_guess.setPos( ScreenWidth / 2 - wide / 2, y + 490 );

	pButton = new ColorButton( "", 120, 90, 400, 75, false, false );
	pButton->setParent( m_pImagePanel_guess );
	pButton->addActionSignal( new CDifficultyHandler_Command( this, 1 ) );
	pButton->setUnArmedBorderColor( 0, 0, 0, 255 );
	pButton->setArmedBorderColor( 255, 255, 255, 192 );
	pButton->setUnArmedColor( 0, 0, 0, 255 );
	pButton->setArmedColor( 0, 0, 0, 255 );

	pButton = new ColorButton( "", 120, 165, 400, 75, false, false );
	pButton->setParent( m_pImagePanel_guess );
	pButton->addActionSignal( new CDifficultyHandler_Command( this, 2 ) );
	pButton->setUnArmedBorderColor( 0, 0, 0, 255 );
	pButton->setArmedBorderColor( 255, 255, 255, 192 );
	pButton->setUnArmedColor( 0, 0, 0, 255 );
	pButton->setArmedColor( 0, 0, 0, 255 );

	pButton = new ColorButton( "", 120, 240, 400, 75, false, false );
	pButton->setParent( m_pImagePanel_guess );
	pButton->addActionSignal( new CDifficultyHandler_Command( this, 3 ) );
	pButton->setUnArmedBorderColor( 0, 0, 0, 255 );
	pButton->setArmedBorderColor( 255, 255, 255, 192 );
	pButton->setUnArmedColor( 0, 0, 0, 255 );
	pButton->setArmedColor( 0, 0, 0, 255 );

	m_pSkill4Button_guess = new ColorButton( "", 120, 315, 400, 75, false, false );
	m_pSkill4Button_guess->setParent( m_pImagePanel_guess );
	m_pSkill4Button_guess->addActionSignal( new CDifficultyHandler_Command( this, 4 ) );
	m_pSkill4Button_guess->setUnArmedBorderColor( 0, 0, 0, 255 );
	m_pSkill4Button_guess->setArmedBorderColor( 255, 255, 255, 192 );
	m_pSkill4Button_guess->setUnArmedColor( 0, 0, 0, 255 );
	m_pSkill4Button_guess->setArmedColor( 0, 0, 0, 255 );

	pButton = new ColorButton( "", 95, 409, 450, 47, false, false );
	pButton->setParent( m_pImagePanel_guess );
	pButton->addActionSignal( new CDifficultyHandler_Command( this, 5 ) );
	pButton->setUnArmedBorderColor( 0, 0, 0, 255 );
	pButton->setArmedBorderColor( 255, 255, 255, 192 );
	pButton->setUnArmedColor( 0, 0, 0, 255 );
	pButton->setArmedColor( 0, 0, 0, 255 );

	setBgColor( 0, 0, 0, 255 );

	Initialize();
}

CDifficulty::~CDifficulty()
{
	delete m_pImage_guess;
}

void CDifficulty::ActionSignal( int cmd )
{
	char szCommand[256];

	switch ( cmd )
	{
	case 1:
		if ( gViewPort && gViewPort->m_pGameMenu_guess && gViewPort->m_pGameMenu_guess->m_pszCampaign_guess )
		{
			sprintf( szCommand, "campaign %s", gViewPort->m_pGameMenu_guess->m_pszCampaign_guess );
			gEngfuncs.pfnServerCmd( szCommand );
		}
		gEngfuncs.pfnServerCmd( "skillset 1" );
		setVisible( false );
		if ( strstr( gEngfuncs.pfnGetLevelName(), "game_menu" ) )
			IrrKlangStop_guess( 1 );
		break;
	case 2:
		if ( gViewPort && gViewPort->m_pGameMenu_guess && gViewPort->m_pGameMenu_guess->m_pszCampaign_guess )
		{
			sprintf( szCommand, "campaign %s", gViewPort->m_pGameMenu_guess->m_pszCampaign_guess );
			gEngfuncs.pfnServerCmd( szCommand );
		}
		gEngfuncs.pfnServerCmd( "skillset 2" );
		setVisible( false );
		if ( strstr( gEngfuncs.pfnGetLevelName(), "game_menu" ) )
			IrrKlangStop_guess( 1 );
		break;
	case 3:
		if ( gViewPort && gViewPort->m_pGameMenu_guess && gViewPort->m_pGameMenu_guess->m_pszCampaign_guess )
		{
			sprintf( szCommand, "campaign %s", gViewPort->m_pGameMenu_guess->m_pszCampaign_guess );
			gEngfuncs.pfnServerCmd( szCommand );
		}
		gEngfuncs.pfnServerCmd( "skillset 3" );
		setVisible( false );
		if ( strstr( gEngfuncs.pfnGetLevelName(), "game_menu" ) )
			IrrKlangStop_guess( 1 );
		break;
	case 4:
		if ( gHUD.m_iUnlocks_guess & 32 )
		{
			if ( gViewPort && gViewPort->m_pGameMenu_guess && gViewPort->m_pGameMenu_guess->m_pszCampaign_guess )
			{
				sprintf( szCommand, "campaign %s", gViewPort->m_pGameMenu_guess->m_pszCampaign_guess );
				gEngfuncs.pfnServerCmd( szCommand );
			}
			gEngfuncs.pfnServerCmd( "skillset 4" );
			setVisible( false );
			if ( strstr( gEngfuncs.pfnGetLevelName(), "game_menu" ) )
				IrrKlangStop_guess( 1 );
		}
		break;
	case 5:
		if ( iMenuMode_guess )
		{
			setVisible( false );
			gViewPort->UpdateCursorState();

			if ( gViewPort && gViewPort->m_pGameMenu_guess )
			{
				gViewPort->m_pGameMenu_guess->setVisible( true );
				gViewPort->UpdateCursorState();
			}
		}
		else
		{
			gEngfuncs.pfnServerCmd( "closegame" );
			setVisible( false );
			gViewPort->UpdateCursorState();
		}
		break;
	}
}

void CDifficulty::Initialize()
{
	setVisible( false );
}

int CDifficulty::KeyInput( int down, int keynum, const char *pszCurrentBinding )
{
	if ( pszCurrentBinding && ( !strcmp( pszCurrentBinding, "toggleconsole" ) || !strcmp( pszCurrentBinding, "skillset" ) ) )
		return 1;

	return 0;
}

void CDifficulty::StateChanged( CCheckButton2 *pButton )
{
	if ( pButton == &m_CommentaryButton_guess )
	{
		if ( pButton->IsChecked() )
			gEngfuncs.pfnClientCmd( "dev_commentary 1" );
		else
			gEngfuncs.pfnClientCmd( "dev_commentary 0" );
	}
}

void CDifficulty::setVisible( bool state )
{
	Panel::setVisible( state );
	gViewPort->UpdateCursorState();

	if ( state )
	{
		if ( gHUD.m_iUnlocks_guess & 0x400000 )
			m_CommentaryButton_guess.setVisible( true );
		else
			m_CommentaryButton_guess.setVisible( false );

		if ( gHUD.m_iUnlocks_guess & 32 )
		{
			m_pSkill4Button_guess->setVisible( true );
			m_pImagePanel_guess->setImage( vgui_LoadTGA( "gfx/vgui/640_difficulty.tga" ) );
		}
		else
		{
			m_pSkill4Button_guess->setVisible( false );
			m_pImagePanel_guess->setImage( vgui_LoadTGA( "gfx/vgui/640_difficultylocked.tga" ) );
		}
	}
}
