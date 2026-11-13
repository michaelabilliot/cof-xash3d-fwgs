// ======================================
// Cry of Fear game menu
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include <string>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "GameMenu_guess.h"
#include "SaveLoad_guess.h"
#include "Unlockables_guess.h"
#include "VGUIExtraMenu_guess.h"
#include "Language_guess.h"
#include "Difficulty_guess.h"
#include "..\game_shared\vgui_loadtga.h"
#include <windows.h>
#include <sys/types.h>
#include <sys/stat.h>

void IrrKlangPlayMP3_guess( const char *pszFile );
void IrrKlangSetLoop_guess( bool bLoop );

const char *GetVersion_guess();

CGameMenu::CGameMenu() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int wide, tall;
	int tsWide, tsTall;
	int logoWide, logoTall;
	int x, y;
	int logoX, logoY;
	int i;

	m_pLogoImage_guess = vgui_LoadTGA( "gfx/vgui/menu_logo.tga" );
	if ( m_pLogoImage_guess )
		m_pLogoImage_guess->setColor( Color( 255, 255, 255, 0 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/menu_logo.tga!\n" );

	m_pNewGameImage_guess = vgui_LoadTGA( "gfx/vgui/menu/newgame.tga" );
	if ( m_pNewGameImage_guess )
		m_pNewGameImage_guess->setColor( Color( 255, 255, 255, 0 ) );

	m_pLoadGameImage_guess = vgui_LoadTGA( "gfx/vgui/menu/loadgame.tga" );
	if ( m_pLoadGameImage_guess )
		m_pNewGameImage_guess->setColor( Color( 255, 255, 255, 0 ) );

	m_pCustomImage_guess = vgui_LoadTGA( "gfx/vgui/menu/customc.tga" );
	if ( m_pCustomImage_guess )
		m_pCustomImage_guess->setColor( Color( 255, 255, 255, 0 ) );

	m_pJoinServerImage_guess = vgui_LoadTGA( "gfx/vgui/menu/jserver.tga" );
	if ( m_pJoinServerImage_guess )
		m_pJoinServerImage_guess->setColor( Color( 255, 255, 255, 0 ) );

	m_pOptionsImage_guess = vgui_LoadTGA( "gfx/vgui/menu/options.tga" );
	if ( m_pOptionsImage_guess )
		m_pOptionsImage_guess->setColor( Color( 255, 255, 255, 0 ) );

	m_pLanguageImage_guess = vgui_LoadTGA( "gfx/vgui/menu/language.tga" );
	if ( m_pLanguageImage_guess )
		m_pLanguageImage_guess->setColor( Color( 255, 255, 255, 0 ) );

	m_pUnlockablesImage_guess = vgui_LoadTGA( "gfx/vgui/menu/unlockables.tga" );
	if ( m_pUnlockablesImage_guess )
		m_pUnlockablesImage_guess->setColor( Color( 255, 255, 255, 0 ) );

	m_pExtrasImage_guess = vgui_LoadTGA( "gfx/vgui/menu/extras.tga" );
	if ( m_pExtrasImage_guess )
		m_pExtrasImage_guess->setColor( Color( 255, 255, 255, 0 ) );

	m_pHostServerImage_guess = vgui_LoadTGA( "gfx/vgui/menu/hserver.tga" );
	if ( m_pHostServerImage_guess )
		m_pHostServerImage_guess->setColor( Color( 255, 255, 255, 0 ) );

	m_pQuitImage_guess = vgui_LoadTGA( "gfx/vgui/menu/quit.tga" );
	if ( m_pQuitImage_guess )
		m_pQuitImage_guess->setColor( Color( 255, 255, 255, 0 ) );

	m_pLogoTSImage_guess = vgui_LoadTGA( "gfx/vgui/menu_logo_ts.tga" );
	if ( m_pLogoTSImage_guess )
		m_pLogoTSImage_guess->setColor( Color( 255, 255, 255, 0 ) );

	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hCreditsScheme = pSchemes->getSchemeHandle( "credits" );
	m_pCreditsFont_guess = pSchemes->getFont( hCreditsScheme );

	m_pVersionLabel_guess = new Label( "", 5, 5, 256, 32 );
	m_pVersionLabel_guess->setFont( m_pCreditsFont_guess );
	m_pVersionLabel_guess->setParent( this );
	m_pVersionLabel_guess->setBgColor( 0, 0, 0, 255 );
	m_pVersionLabel_guess->setFgColor( 255, 255, 255, 0 );
	m_pVersionLabel_guess->setVisible( true );
	m_pVersionLabel_guess->setText( GetVersion_guess() );
	m_pVersionLabel_guess->setContentAlignment( Label::a_west );

	m_pNewGameImage_guess->getSize( wide, tall );
	m_pLogoTSImage_guess->getSize( tsWide, tsTall );
	y = ScreenHeight - tall;
	x = ScreenWidth / 2 - wide / 2;

	m_pLogoImage_guess->getSize( logoWide, logoTall );
	logoX = ScreenWidth / 2 - logoWide / 2;
	logoY = ScreenHeight / 2 - logoTall * 3;

	m_pLogo_guess = new ImagePanel( m_pLogoImage_guess );
	m_pLogo_guess->setParent( this );
	m_pLogo_guess->setPos( logoX, logoY );
	m_pLogo_guess->setVisible( true );

	m_pLogoTS_guess = new ImagePanel( m_pLogoTSImage_guess );
	m_pLogoTS_guess->setParent( this );
	m_pLogoTS_guess->setPos( ScreenWidth - tsWide - 15, ScreenHeight - tsTall - 15 );
	m_pLogoTS_guess->setVisible( true );

	m_pNewGame_guess = new ImagePanel( m_pNewGameImage_guess );
	m_pNewGame_guess->setParent( this );
	m_pNewGame_guess->setPos( x, y - tall * 10 );
	m_pNewGame_guess->setVisible( true );

	m_pLoadGame_guess = new ImagePanel( m_pLoadGameImage_guess );
	m_pLoadGame_guess->setParent( this );
	m_pLoadGame_guess->setPos( x, y - tall * 9 );
	m_pLoadGame_guess->setVisible( true );

	m_pCustom_guess = new ImagePanel( m_pCustomImage_guess );
	m_pCustom_guess->setParent( this );
	m_pCustom_guess->setPos( x, y - tall * 8 );
	m_pCustom_guess->setVisible( true );

	m_pJoinServer_guess = new ImagePanel( m_pJoinServerImage_guess );
	m_pJoinServer_guess->setParent( this );
	m_pJoinServer_guess->setPos( x, y - tall * 7 );
	m_pJoinServer_guess->setVisible( true );

	m_pOptions_guess = new ImagePanel( m_pOptionsImage_guess );
	m_pOptions_guess->setParent( this );
	m_pOptions_guess->setPos( x, y - tall * 2 );
	m_pOptions_guess->setVisible( true );

	m_pLanguage_guess = new ImagePanel( m_pLanguageImage_guess );
	m_pLanguage_guess->setParent( this );
	m_pLanguage_guess->setPos( x, y - tall * 5 );
	m_pLanguage_guess->setVisible( true );

	m_pUnlockables_guess = new ImagePanel( m_pUnlockablesImage_guess );
	m_pUnlockables_guess->setParent( this );
	m_pUnlockables_guess->setPos( x, y - tall * 4 );
	m_pUnlockables_guess->setVisible( true );

	m_pExtras_guess = new ImagePanel( m_pExtrasImage_guess );
	m_pExtras_guess->setParent( this );
	m_pExtras_guess->setPos( x, y - tall * 3 );
	m_pExtras_guess->setVisible( true );

	m_pHostServer_guess = new ImagePanel( m_pHostServerImage_guess );
	m_pHostServer_guess->setParent( this );
	m_pHostServer_guess->setPos( x, y - tall * 6 );
	m_pHostServer_guess->setVisible( true );

	m_pQuit_guess = new ImagePanel( m_pQuitImage_guess );
	m_pQuit_guess->setParent( this );
	m_pQuit_guess->setPos( x, y - tall );
	m_pQuit_guess->setVisible( true );

	m_pNewGameButton_guess = new ColorButton( "", x, y - tall * 10, wide, tall, false, true );
	m_pNewGameButton_guess->setParent( this );
	m_pNewGameButton_guess->addActionSignal( new CGameMenuHandler_Command( this, 1 ) );

	m_pLoadGameButton_guess = new ColorButton( "", x, y - tall * 9, wide, tall, false, true );
	m_pLoadGameButton_guess->setParent( this );
	m_pLoadGameButton_guess->addActionSignal( new CGameMenuHandler_Command( this, 2 ) );

	m_pCustomButton_guess = new ColorButton( "", x, y - tall * 8, wide, tall, false, true );
	m_pCustomButton_guess->setParent( this );
	m_pCustomButton_guess->addActionSignal( new CGameMenuHandler_Command( this, 3 ) );

	m_pJoinServerButton_guess = new ColorButton( "", x, y - tall * 7, wide, tall, false, true );
	m_pJoinServerButton_guess->setParent( this );
	m_pJoinServerButton_guess->addActionSignal( new CGameMenuHandler_Command( this, 4 ) );

	m_pOptionsButton_guess = new ColorButton( "", x, y - tall * 2, wide, tall, false, true );
	m_pOptionsButton_guess->setParent( this );
	m_pOptionsButton_guess->addActionSignal( new CGameMenuHandler_Command( this, 5 ) );

	m_pLanguageButton_guess = new ColorButton( "", x, y - tall * 5, wide, tall, false, true );
	m_pLanguageButton_guess->setParent( this );
	m_pLanguageButton_guess->addActionSignal( new CGameMenuHandler_Command( this, 6 ) );

	m_pUnlockablesButton_guess = new ColorButton( "", x, y - tall * 4, wide, tall, false, true );
	m_pUnlockablesButton_guess->setParent( this );
	m_pUnlockablesButton_guess->addActionSignal( new CGameMenuHandler_Command( this, 7 ) );

	m_pExtrasButton_guess = new ColorButton( "", x, y - tall * 3, wide, tall, false, true );
	m_pExtrasButton_guess->setParent( this );
	m_pExtrasButton_guess->addActionSignal( new CGameMenuHandler_Command( this, 8 ) );

	m_pHostServerButton_guess = new ColorButton( "", x, y - tall * 6, wide, tall, false, true );
	m_pHostServerButton_guess->setParent( this );
	m_pHostServerButton_guess->addActionSignal( new CGameMenuHandler_Command( this, 9 ) );

	m_pQuitButton_guess = new ColorButton( "", x, y - tall, wide, tall, false, true );
	m_pQuitButton_guess->setParent( this );
	m_pQuitButton_guess->addActionSignal( new CGameMenuHandler_Command( this, 10 ) );

	// list of the custom campaigns
	int iCancelWide = ScreenWidth;

	m_pCampaignList_guess = new CTFScrollPanel( ScreenWidth / 2 - 350, ScreenHeight / 2 - 262, 700, 525 );
	m_pCampaignList_guess->setParent( this );
	m_pCampaignList_guess->setScrollBarVisible( false, false );

	ScrollBar *pScrollBar = m_pCampaignList_guess->getVerticalScrollBar();
	pScrollBar->setSlider( new CCampaignSlider( 0, 700 - 1, 700, ( 525 - ( 700 * 2 ) ) + 2, true ) );

	for ( i = 0; i < MAX_CAMPAIGNS_guess; i++ )
	{
		m_pCampaignButton_guess[i] = new ColorButton( "N/A", 25, i * 32, 650, 32, false, true );
		m_pCampaignButton_guess[i]->setParent( m_pCampaignList_guess->getClient() );
		m_pCampaignButton_guess[i]->addActionSignal( new CGameMenuHandler_Command( this, i + 50 ) );
		m_pCampaignButton_guess[i]->setVisible( false );
		m_pCampaignButton_guess[i]->setText( "N/A" );
		m_pCampaignButton_guess[i]->setContentAlignment( Label::a_center );
	}

	m_pAuthorLabel_guess = new Label( " ", 0, 0, ScreenWidth, 64 );
	m_pAuthorLabel_guess->setParent( this );
	m_pAuthorLabel_guess->setFgColor( 255, 255, 255, 0 );
	m_pAuthorLabel_guess->setBgColor( 0, 0, 0, 255 );
	m_pAuthorLabel_guess->setText( " " );
	m_pAuthorLabel_guess->setFont( m_pCreditsFont_guess );
	m_pAuthorLabel_guess->setContentAlignment( Label::a_east );

	m_pCancelButton_guess = new ColorButton( "Cancel", 0, ScreenHeight / 2 + 262, iCancelWide, 32, false, true );
	m_pCancelButton_guess->setParent( this );
	m_pCancelButton_guess->addActionSignal( new CGameMenuHandler_Command( this, 11 ) );
	m_pCancelButton_guess->setVisible( false );
	m_pCancelButton_guess->setContentAlignment( Label::a_center );
	m_pCancelButton_guess->setUnArmedColor( 255, 255, 255, 0 );
	m_pCancelButton_guess->setArmedColor( 255, 0, 0, 0 );

	setBgColor( 0, 0, 0, 255 );

	m_pCampaignList_guess->setScrollValue( 0, 0 );
	m_pCampaignList_guess->validate();

	Initialize();
}

void CGameMenu::ActionSignal( int cmd )
{
	int i;

	if ( !gEngfuncs.GetLocalPlayer() )
		return;

	int index = cmd - 50;

	if ( index >= 0 && index < 30 && m_Campaigns_guess[index].bValid )
	{
		setVisible( false );
		gViewPort->UpdateCursorState();
		gEngfuncs.pfnPlaySoundByName( "ui/3d_click.wav", 0.5 );

		if ( gViewPort->m_pDifficulty_guess )
		{
			m_pszCampaign_guess = m_Campaigns_guess[index].szFirstMap;
			gViewPort->m_pDifficulty_guess->setVisible( true );
			gViewPort->UpdateCursorState();
		}
	}

	switch ( cmd )
	{
	case 1:
		setVisible( false );
		gViewPort->UpdateCursorState();
		gEngfuncs.pfnPlaySoundByName( "ui/3d_click.wav", 0.5 );

		if ( gViewPort->m_pDifficulty_guess )
		{
			m_pszCampaign_guess = NULL;
			gViewPort->m_pDifficulty_guess->setVisible( true );
			gViewPort->UpdateCursorState();
		}
		break;
	case 2:
		setVisible( false );
		gViewPort->UpdateCursorState();
		gEngfuncs.pfnPlaySoundByName( "ui/3d_click.wav", 0.5 );

		if ( gViewPort->m_pSaveLoad_guess )
		{
			gViewPort->m_pSaveLoad_guess->m_pSlot1Button_guess->cursorExited();
			gViewPort->m_pSaveLoad_guess->m_pSlot2Button_guess->cursorExited();
			gViewPort->m_pSaveLoad_guess->m_pSlot3Button_guess->cursorExited();
			gViewPort->m_pSaveLoad_guess->m_pSlot4Button_guess->cursorExited();
			gViewPort->m_pSaveLoad_guess->m_pSlot5Button_guess->cursorExited();

			gViewPort->m_pSaveLoad_guess->m_flSlot1ClickTime_guess = 0;
			gViewPort->m_pSaveLoad_guess->m_flSlot2ClickTime_guess = 0;
			gViewPort->m_pSaveLoad_guess->m_flSlot3ClickTime_guess = 0;
			gViewPort->m_pSaveLoad_guess->m_flSlot4ClickTime_guess = 0;
			gViewPort->m_pSaveLoad_guess->m_flSlot5ClickTime_guess = 0;

			gViewPort->m_pSaveLoad_guess->m_iMode_guess = 1;
			gViewPort->m_pSaveLoad_guess->ReadAllSlots_guess();
			gViewPort->m_pSaveLoad_guess->setVisible( true );
			gViewPort->UpdateCursorState();
			gEngfuncs.pfnServerCmd( "freeze 1024" );
			gEngfuncs.pfnPlaySoundByName( "ui/3d_click.wav", 0.5 );
		}
		break;
	case 3:
		m_pNewGameButton_guess->setVisible( false );
		m_pLoadGameButton_guess->setVisible( false );
		m_pCustomButton_guess->setVisible( false );
		m_pJoinServerButton_guess->setVisible( false );
		m_pOptionsButton_guess->setVisible( false );
		m_pLanguageButton_guess->setVisible( false );
		m_pUnlockablesButton_guess->setVisible( false );
		m_pExtrasButton_guess->setVisible( false );
		m_pHostServerButton_guess->setVisible( false );
		m_pQuitButton_guess->setVisible( false );
		m_pLogo_guess->setVisible( false );
		m_pNewGame_guess->setVisible( false );
		m_pLoadGame_guess->setVisible( false );
		m_pCustom_guess->setVisible( false );
		m_pJoinServer_guess->setVisible( false );
		m_pOptions_guess->setVisible( false );
		m_pLanguage_guess->setVisible( false );
		m_pUnlockables_guess->setVisible( false );
		m_pExtras_guess->setVisible( false );
		m_pHostServer_guess->setVisible( false );
		m_pQuit_guess->setVisible( false );
		m_pCancelButton_guess->setVisible( true );
		m_pCampaignList_guess->setVisible( true );
		gEngfuncs.pfnPlaySoundByName( "ui/3d_click.wav", 0.5 );

		for ( i = 0; i < MAX_CAMPAIGNS_guess; i++ )
			m_pCampaignButton_guess[i]->setVisible( true );

		ReadCampaigns_guess();
		m_pCampaignList_guess->validate();
		break;
	case 4:
		gEngfuncs.pfnServerCmd( "optionshack 0" );
		break;
	case 5:
		gEngfuncs.pfnServerCmd( "optionshack 2" );
		break;
	case 6:
		setVisible( false );
		gViewPort->UpdateCursorState();
		gEngfuncs.pfnPlaySoundByName( "ui/3d_click.wav", 0.5 );

		if ( gViewPort->m_pLanguage_guess )
		{
			gViewPort->m_pLanguage_guess->setVisible( true );
			gViewPort->UpdateCursorState();
		}
		break;
	case 7:
		if ( gViewPort && gViewPort->m_pUnlockables_guess )
		{
			setVisible( false );
			gViewPort->UpdateCursorState();
			gViewPort->m_pUnlockables_guess->setVisible( true );
			gViewPort->UpdateCursorState();
			gEngfuncs.pfnPlaySoundByName( "ui/3d_click.wav", 0.5 );
		}
		break;
	case 8:
		if ( gViewPort->m_pVGUIExtraMenu_guess )
		{
			setVisible( false );
			gViewPort->UpdateCursorState();
			gViewPort->m_pVGUIExtraMenu_guess->setVisible( true );
			gViewPort->UpdateCursorState();
		}
		break;
	case 9:
		gEngfuncs.pfnServerCmd( "optionshack 1" );
		break;
	case 10:
		setVisible( false );
		gEngfuncs.pfnClientCmd( "quit" );
		gViewPort->UpdateCursorState();
		break;
	case 11:
		m_pNewGameButton_guess->setVisible( true );
		m_pLoadGameButton_guess->setVisible( true );
		m_pCustomButton_guess->setVisible( true );
		m_pJoinServerButton_guess->setVisible( true );
		m_pOptionsButton_guess->setVisible( true );
		m_pLanguageButton_guess->setVisible( true );
		m_pUnlockablesButton_guess->setVisible( true );
		m_pExtrasButton_guess->setVisible( true );
		m_pHostServerButton_guess->setVisible( true );
		m_pQuitButton_guess->setVisible( true );
		m_pLogo_guess->setVisible( true );
		m_pNewGame_guess->setVisible( true );
		m_pLoadGame_guess->setVisible( true );
		m_pCustom_guess->setVisible( true );
		m_pJoinServer_guess->setVisible( true );
		m_pOptions_guess->setVisible( true );
		m_pLanguage_guess->setVisible( true );
		m_pUnlockables_guess->setVisible( true );
		m_pExtras_guess->setVisible( true );
		m_pHostServer_guess->setVisible( true );
		m_pQuit_guess->setVisible( true );
		m_pCancelButton_guess->setVisible( false );
		m_pCampaignList_guess->setVisible( false );
		gEngfuncs.pfnPlaySoundByName( "ui/3d_click.wav", 0.5 );

		for ( i = 0; i < MAX_CAMPAIGNS_guess; i++ )
			m_pCampaignButton_guess[i]->setVisible( false );
		break;
	}
}

const char *GetVersion_guess()
{
	return "1.6a";
}

void CGameMenu::Initialize()
{
	m_bUnknownC0_guess = false;
	memset( m_Campaigns_guess, 0, sizeof( m_Campaigns_guess ) );

	m_pCampaignList_guess->setScrollValue( 0, 0 );
	m_pszCampaign_guess = NULL;

	setVisible( false );
}

int MsgGameMenu_guess( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	if ( gViewPort->m_pGameMenu_guess )
	{
		gViewPort->m_pGameMenu_guess->m_bNewGameArmed_guess = false;
		gViewPort->m_pGameMenu_guess->m_bLoadGameArmed_guess = false;
		gViewPort->m_pGameMenu_guess->m_bCustomArmed_guess = false;
		gViewPort->m_pGameMenu_guess->m_bJoinServerArmed_guess = false;
		gViewPort->m_pGameMenu_guess->m_bOptionsArmed_guess = false;
		gViewPort->m_pGameMenu_guess->m_bLanguageArmed_guess = false;
		gViewPort->m_pGameMenu_guess->m_bUnlockablesArmed_guess = false;
		gViewPort->m_pGameMenu_guess->m_bExtrasArmed_guess = false;
		gViewPort->m_pGameMenu_guess->m_bHostServerArmed_guess = false;
		gViewPort->m_pGameMenu_guess->m_bQuitArmed_guess = false;

		gViewPort->m_pGameMenu_guess->m_pNewGame_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/newgame.tga" ) );
		gViewPort->m_pGameMenu_guess->m_pLoadGame_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/loadgame.tga" ) );
		gViewPort->m_pGameMenu_guess->m_pCustom_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/customc.tga" ) );
		gViewPort->m_pGameMenu_guess->m_pJoinServer_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/jserver.tga" ) );
		gViewPort->m_pGameMenu_guess->m_pOptions_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/options.tga" ) );
		gViewPort->m_pGameMenu_guess->m_pLanguage_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/language.tga" ) );
		gViewPort->m_pGameMenu_guess->m_pUnlockables_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/unlockables.tga" ) );
		gViewPort->m_pGameMenu_guess->m_pExtras_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/extras.tga" ) );
		gViewPort->m_pGameMenu_guess->m_pHostServer_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/hserver.tga" ) );
		gViewPort->m_pGameMenu_guess->m_pQuit_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/quit.tga" ) );

		gViewPort->m_pGameMenu_guess->setVisible( true );
		gViewPort->UpdateCursorState();
		gEngfuncs.pfnServerCmd( "freeze 1024" );
	}

	return 1;
}

void CGameMenu::ParseCampaign_guess( char *pszFile, int index )
{
	char token[1024];
	char szPath[256];
	char *pFile;

	gEngfuncs.Con_Printf( "PARSING CUSTOM CAMPAIGN: %s\n", pszFile );

	strcpy( szPath, "maps/" );
	strcat( szPath, pszFile );

	pFile = (char *)gEngfuncs.COM_LoadFile( szPath, 5, NULL );

	if ( !pFile )
	{
		gEngfuncs.Con_Printf( "PARSING CUSTOM CAMPAIGN: Couldn't open campaign file %s\n", pszFile );
		return;
	}

	for ( pFile = gEngfuncs.COM_ParseFile( pFile, token ); pFile; pFile = gEngfuncs.COM_ParseFile( pFile, token ) )
	{
		if ( !stricmp( token, "firstmap" ) )
		{
			pFile = gEngfuncs.COM_ParseFile( pFile, token );
			strncpy( m_Campaigns_guess[index].szFirstMap, token, 64 );
			m_Campaigns_guess[index].bValid = true;
		}
		else if ( !stricmp( token, "author" ) )
		{
			pFile = gEngfuncs.COM_ParseFile( pFile, token );
			strncpy( m_Campaigns_guess[index].szAuthor, token, 64 );
			m_Campaigns_guess[index].bValid = true;
		}
		else if ( !stricmp( token, "description" ) )
		{
			pFile = gEngfuncs.COM_ParseFile( pFile, token );
			strncpy( m_Campaigns_guess[index].szDescription, token, 64 );
			m_Campaigns_guess[index].bValid = true;

			if ( m_pCampaignButton_guess[index] )
			{
				int w, h;
				int x, y;
				int wide, tall;

				m_pCreditsFont_guess->getTextSize( token, w, h );
				m_pCampaignButton_guess[index]->getPos( x, y );
				m_pCampaignButton_guess[index]->setText( token );
				m_pCampaignButton_guess[index]->setArmedColor( 255, 0, 0, 0 );
				m_pCampaignButton_guess[index]->setUnArmedColor( 255, 255, 255, 0 );
				m_pCampaignButton_guess[index]->getSize( wide, tall );
				w = w * 1.2;
				m_pCampaignButton_guess[index]->setContentAlignment( Label::a_center );
			}
		}
	}

	if ( !m_Campaigns_guess[index].bValid )
		gEngfuncs.Con_Printf( "PARSING CUSTOM CAMPAIGN: Couldn't parse data from campaign file %s\n", pszFile );
}

void CGameMenu::ReadCampaigns_guess()
{
	int i;

	for ( i = 0; i < MAX_CAMPAIGNS_guess; i++ )
	{
		m_pCampaignButton_guess[i]->setText( "N/A" );
		m_pCampaignButton_guess[i]->setUnArmedColor( 255, 255, 255, 0 );
		m_pCampaignButton_guess[i]->setArmedColor( 255, 255, 255, 0 );
	}

	m_pszCampaign_guess = NULL;
	m_bUnknownC0_guess = false;
	memset( m_Campaigns_guess, 0, sizeof( m_Campaigns_guess ) );

	char szDir[1024];
	strcpy( szDir, gEngfuncs.pfnGetGameDirectory() );
	strcat( szDir, "\\maps\\" );

	std::string dir = szDir;
	std::string ext = "*.custom";
	std::string search = dir + ext;

	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile( search.c_str(), &fd );

	if ( hFind != INVALID_HANDLE_VALUE )
	{
		do
		{
			std::string file = dir + fd.cFileName;
			struct stat st;

			if ( !stat( file.c_str(), &st ) )
			{
				for ( i = 0; i < MAX_CAMPAIGNS_guess; i++ )
				{
					if ( !m_Campaigns_guess[i].bValid )
					{
						ParseCampaign_guess( fd.cFileName, i );
						break;
					}
				}
			}
		} while ( FindNextFile( hFind, &fd ) > 0 );
	}
}

void CGameMenu::ResetButtonPositions_guess()
{
	int wide, tall;
	int tsWide, tsTall;
	int logoWide, logoTall;
	int x, y;

	m_pNewGameImage_guess->getSize( wide, tall );
	m_pLogoTSImage_guess->getSize( tsWide, tsTall );
	y = ScreenHeight - tall;
	x = ScreenWidth / 2 - wide / 2;

	m_pLogoImage_guess->getSize( logoWide, logoTall );

	m_pNewGame_guess->setPos( x, y - tall * 10 );
	m_pLoadGame_guess->setPos( x, y - tall * 9 );
	m_pCustom_guess->setPos( x, y - tall * 8 );
	m_pJoinServer_guess->setPos( x, y - tall * 7 );
	m_pOptions_guess->setPos( x, y - tall * 2 );
	m_pLanguage_guess->setPos( x, y - tall * 5 );
	m_pUnlockables_guess->setPos( x, y - tall * 4 );
	m_pExtras_guess->setPos( x, y - tall * 3 );
	m_pHostServer_guess->setPos( x, y - tall * 6 );
	m_pQuit_guess->setPos( x, y - tall );
}

void VGUI_GameMenuInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "GameMenu", MsgGameMenu_guess );
}

void CCampaignSlider::paintBackground()
{
	int wide, tall, nobx, noby;

	getPaintSize( wide, tall );
	getNobPos( nobx, noby );

	drawSetColor( 128, 128, 128, 150 );
	drawFilledRect( 0, 0, wide, tall );

	drawSetColor( 200, 200, 200, 0 );
	drawFilledRect( 0, nobx, wide, noby );

	drawSetColor( 0, 0, 0, 150 );
	drawOutlinedRect( 0, nobx, wide - 1, noby - 1 );

	drawSetColor( 255, 255, 255, 150 );
	drawOutlinedRect( 1, nobx + 1, wide, noby );

	drawSetColor( 0, 0, 0, 0 );
	drawOutlinedRect( 0, nobx, wide, noby );
}

void CGameMenu::setVisible( bool state )
{
	if ( state )
	{
		ResetButtonPositions_guess();
		IrrKlangSetLoop_guess( true );
		IrrKlangPlayMP3_guess( "gamestartup.mp3" );

		m_iArmedCampaign_guess = -1;
		m_bCampaignArmed_guess = true;

		m_pNewGameButton_guess->setVisible( true );
		m_pLoadGameButton_guess->setVisible( true );
		m_pCustomButton_guess->setVisible( true );
		m_pJoinServerButton_guess->setVisible( true );
		m_pOptionsButton_guess->setVisible( true );
		m_pLanguageButton_guess->setVisible( true );
		m_pUnlockablesButton_guess->setVisible( true );
		m_pExtrasButton_guess->setVisible( true );
		m_pHostServerButton_guess->setVisible( true );
		m_pQuitButton_guess->setVisible( true );
		m_pLogo_guess->setVisible( true );
		m_pNewGame_guess->setVisible( true );
		m_pLoadGame_guess->setVisible( true );
		m_pCustom_guess->setVisible( true );
		m_pJoinServer_guess->setVisible( true );
		m_pOptions_guess->setVisible( true );
		m_pLanguage_guess->setVisible( true );
		m_pUnlockables_guess->setVisible( true );
		m_pExtras_guess->setVisible( true );
		m_pHostServer_guess->setVisible( true );
		m_pQuit_guess->setVisible( true );
		m_pCancelButton_guess->setVisible( false );
		m_pCampaignList_guess->setVisible( false );
		gEngfuncs.pfnPlaySoundByName( "ui/3d_click.wav", 0.5 );

		for ( int i = 0; i < MAX_CAMPAIGNS_guess; i++ )
			m_pCampaignButton_guess[i]->setVisible( false );
	}

	Panel::setVisible( state );
}

void CGameMenu::solve()
{
	char szText[256];

	if ( !gEngfuncs.GetLocalPlayer() )
		return;

	m_pLogoImage_guess->setColor( Color( 255, 255, 255, gEngfuncs.pfnRandomLong( 0, 50 ) ) );

	if ( m_pNewGameButton_guess->isArmed() )
	{
		if ( !m_bNewGameArmed_guess )
		{
			m_pNewGame_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/newgame_mo.tga" ) );
			gEngfuncs.pfnPlaySoundByName( "ui/3d_rollover.wav", 0.5 );
			m_bNewGameArmed_guess = true;
			m_pNewGame_guess->getPos( m_iArmedX_guess, m_iArmedY_guess );
		}
		else
		{
			m_pNewGame_guess->setPos( m_iArmedX_guess + gEngfuncs.pfnRandomLong( -3, 3 ), m_iArmedY_guess );
		}
	}
	else if ( m_bNewGameArmed_guess )
	{
		m_pNewGame_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/newgame.tga" ) );
		m_bNewGameArmed_guess = false;
		ResetButtonPositions_guess();
	}

	if ( m_pLoadGameButton_guess->isArmed() )
	{
		if ( !m_bLoadGameArmed_guess )
		{
			m_pLoadGame_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/loadgame_mo.tga" ) );
			gEngfuncs.pfnPlaySoundByName( "ui/3d_rollover.wav", 0.5 );
			m_bLoadGameArmed_guess = true;
			m_pLoadGame_guess->getPos( m_iArmedX_guess, m_iArmedY_guess );
		}
		else
		{
			m_pLoadGame_guess->setPos( m_iArmedX_guess + gEngfuncs.pfnRandomLong( -3, 3 ), m_iArmedY_guess );
		}
	}
	else if ( m_bLoadGameArmed_guess )
	{
		m_pLoadGame_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/loadgame.tga" ) );
		m_bLoadGameArmed_guess = false;
		ResetButtonPositions_guess();
	}

	if ( m_pCustomButton_guess->isArmed() )
	{
		if ( !m_bCustomArmed_guess )
		{
			m_pCustom_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/customc_mo.tga" ) );
			gEngfuncs.pfnPlaySoundByName( "ui/3d_rollover.wav", 0.5 );
			m_bCustomArmed_guess = true;
			m_pCustom_guess->getPos( m_iArmedX_guess, m_iArmedY_guess );
		}
		else
		{
			m_pCustom_guess->setPos( m_iArmedX_guess + gEngfuncs.pfnRandomLong( -3, 3 ), m_iArmedY_guess );
		}
	}
	else if ( m_bCustomArmed_guess )
	{
		m_pCustom_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/customc.tga" ) );
		m_bCustomArmed_guess = false;
		ResetButtonPositions_guess();
	}

	if ( m_pJoinServerButton_guess->isArmed() )
	{
		if ( !m_bJoinServerArmed_guess )
		{
			m_pJoinServer_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/jserver_mo.tga" ) );
			gEngfuncs.pfnPlaySoundByName( "ui/3d_rollover.wav", 0.5 );
			m_bJoinServerArmed_guess = true;
			m_pJoinServer_guess->getPos( m_iArmedX_guess, m_iArmedY_guess );
		}
		else
		{
			m_pJoinServer_guess->setPos( m_iArmedX_guess + gEngfuncs.pfnRandomLong( -3, 3 ), m_iArmedY_guess );
		}
	}
	else if ( m_bJoinServerArmed_guess )
	{
		m_pJoinServer_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/jserver.tga" ) );
		m_bJoinServerArmed_guess = false;
		ResetButtonPositions_guess();
	}

	if ( m_pOptionsButton_guess->isArmed() )
	{
		if ( !m_bOptionsArmed_guess )
		{
			m_pOptions_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/options_mo.tga" ) );
			gEngfuncs.pfnPlaySoundByName( "ui/3d_rollover.wav", 0.5 );
			m_bOptionsArmed_guess = true;
			m_pOptions_guess->getPos( m_iArmedX_guess, m_iArmedY_guess );
		}
		else
		{
			m_pOptions_guess->setPos( m_iArmedX_guess + gEngfuncs.pfnRandomLong( -3, 3 ), m_iArmedY_guess );
		}
	}
	else if ( m_bOptionsArmed_guess )
	{
		m_pOptions_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/options.tga" ) );
		m_bOptionsArmed_guess = false;
		ResetButtonPositions_guess();
	}

	if ( m_pLanguageButton_guess->isArmed() )
	{
		if ( !m_bLanguageArmed_guess )
		{
			m_pLanguage_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/language_mo.tga" ) );
			gEngfuncs.pfnPlaySoundByName( "ui/3d_rollover.wav", 0.5 );
			m_bLanguageArmed_guess = true;
			m_pLanguage_guess->getPos( m_iArmedX_guess, m_iArmedY_guess );
		}
		else
		{
			m_pLanguage_guess->setPos( m_iArmedX_guess + gEngfuncs.pfnRandomLong( -3, 3 ), m_iArmedY_guess );
		}
	}
	else if ( m_bLanguageArmed_guess )
	{
		m_pLanguage_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/language.tga" ) );
		m_bLanguageArmed_guess = false;
		ResetButtonPositions_guess();
	}

	if ( m_pUnlockablesButton_guess->isArmed() )
	{
		if ( !m_bUnlockablesArmed_guess )
		{
			m_pUnlockables_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/unlockables_mo.tga" ) );
			gEngfuncs.pfnPlaySoundByName( "ui/3d_rollover.wav", 0.5 );
			m_bUnlockablesArmed_guess = true;
			m_pUnlockables_guess->getPos( m_iArmedX_guess, m_iArmedY_guess );
		}
		else
		{
			m_pUnlockables_guess->setPos( m_iArmedX_guess + gEngfuncs.pfnRandomLong( -3, 3 ), m_iArmedY_guess );
		}
	}
	else if ( m_bUnlockablesArmed_guess )
	{
		m_pUnlockables_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/unlockables.tga" ) );
		m_bUnlockablesArmed_guess = false;
		ResetButtonPositions_guess();
	}

	if ( m_pExtrasButton_guess->isArmed() )
	{
		if ( !m_bExtrasArmed_guess )
		{
			m_pExtras_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/extras_mo.tga" ) );
			gEngfuncs.pfnPlaySoundByName( "ui/3d_rollover.wav", 0.5 );
			m_bExtrasArmed_guess = true;
			m_pExtras_guess->getPos( m_iArmedX_guess, m_iArmedY_guess );
		}
		else
		{
			m_pExtras_guess->setPos( m_iArmedX_guess + gEngfuncs.pfnRandomLong( -3, 3 ), m_iArmedY_guess );
		}
	}
	else if ( m_bExtrasArmed_guess )
	{
		m_pExtras_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/extras.tga" ) );
		m_bExtrasArmed_guess = false;
		ResetButtonPositions_guess();
	}

	if ( m_pHostServerButton_guess->isArmed() )
	{
		if ( !m_bHostServerArmed_guess )
		{
			m_pHostServer_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/hserver_mo.tga" ) );
			gEngfuncs.pfnPlaySoundByName( "ui/3d_rollover.wav", 0.5 );
			m_bHostServerArmed_guess = true;
			m_pHostServer_guess->getPos( m_iArmedX_guess, m_iArmedY_guess );
		}
		else
		{
			m_pHostServer_guess->setPos( m_iArmedX_guess + gEngfuncs.pfnRandomLong( -3, 3 ), m_iArmedY_guess );
		}
	}
	else if ( m_bHostServerArmed_guess )
	{
		m_pHostServer_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/hserver.tga" ) );
		m_bHostServerArmed_guess = false;
		ResetButtonPositions_guess();
	}

	if ( m_pQuitButton_guess->isArmed() )
	{
		if ( !m_bQuitArmed_guess )
		{
			m_pQuit_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/quit_mo.tga" ) );
			gEngfuncs.pfnPlaySoundByName( "ui/3d_rollover.wav", 0.5 );
			m_bQuitArmed_guess = true;
			m_pQuit_guess->getPos( m_iArmedX_guess, m_iArmedY_guess );
		}
		else
		{
			m_pQuit_guess->setPos( m_iArmedX_guess + gEngfuncs.pfnRandomLong( -3, 3 ), m_iArmedY_guess );
		}
	}
	else if ( m_bQuitArmed_guess )
	{
		m_pQuit_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/quit.tga" ) );
		m_bQuitArmed_guess = false;
		ResetButtonPositions_guess();
	}

	bool bArmed = false;
	m_iArmedCampaign_guess = -1;

	for ( int i = 0; i < MAX_CAMPAIGNS_guess; i++ )
	{
		if ( m_pCampaignButton_guess[i]->isArmed() && m_iArmedCampaign_guess != i )
		{
			m_iArmedCampaign_guess = i;

			if ( m_Campaigns_guess[i].szAuthor && m_Campaigns_guess[i].bValid )
			{
				sprintf( szText, "Author: %s", m_Campaigns_guess[i].szAuthor );
				m_pAuthorLabel_guess->setText( szText );
				m_pAuthorLabel_guess->setVisible( true );
			}
			else
			{
				m_pAuthorLabel_guess->setText( "" );
				m_pAuthorLabel_guess->setVisible( false );
			}

			m_bCampaignArmed_guess = true;
		}

		if ( m_pCampaignButton_guess[i]->isArmed() )
		{
			m_iArmedCampaign_guess = i;
			bArmed = true;
		}
	}

	if ( !m_bCampaignArmed_guess && m_iArmedCampaign_guess >= 0 )
		m_iArmedCampaign_guess = -1;

	if ( !bArmed && m_pAuthorLabel_guess->isVisible() )
	{
		m_pAuthorLabel_guess->setText( "" );
		m_pAuthorLabel_guess->setVisible( false );
	}

	Panel::solve();
}

CGameMenu::~CGameMenu( void )
{
	delete m_pLogoImage_guess;
}
