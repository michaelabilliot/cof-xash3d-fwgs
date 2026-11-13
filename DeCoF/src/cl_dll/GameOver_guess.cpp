// ======================================
// Cry of Fear game over panel
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "GameOver_guess.h"
#include "SaveLoad_guess.h"
#include "..\game_shared\vgui_loadtga.h"

extern int iMenuMode_guess;
extern int iDoctorMode_guess;
extern int iVGUIMenuNoMusic_guess;

void IrrKlangPlayMP3_guess( const char *pszFile );

CGameOver::CGameOver() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int wide, tall;
	int x, y;
	int loadWide, loadTall;
	int exitWide, exitTall;
	int loadX, loadY;
	int exitX, exitY;
	int exitButtonX;

	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );

	m_pImage_guess = vgui_LoadTGA( "gfx/vgui/game_over.tga" );
	if ( m_pImage_guess )
		m_pImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/game_over.tga!\n" );

	m_pLoadImage_guess = vgui_LoadTGA( "gfx/vgui/game_over_load.tga" );
	if ( m_pLoadImage_guess )
		m_pLoadImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/game_over_load.tga!\n" );

	m_pExitImage_guess = vgui_LoadTGA( "gfx/vgui/game_over_exit.tga" );
	if ( m_pExitImage_guess )
		m_pExitImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/game_over_exit.tga!\n" );

	m_pImage_guess->getSize( wide, tall );
	x = ScreenWidth / 2 - wide / 2;
	y = ScreenHeight / 2 - tall / 2;

	m_pLoadImage_guess->getSize( loadWide, loadTall );
	m_pExitImage_guess->getSize( exitWide, exitTall );

	loadX = wide / 2 - loadWide / 2;
	loadY = tall / 2 + loadTall;
	exitX = wide / 2 - exitWide / 2;
	exitY = loadY + exitTall;
	exitButtonX = wide / 2 - 51;

	m_pPanel_guess = new ImagePanel( m_pImage_guess );
	m_pPanel_guess->setParent( this );
	m_pPanel_guess->setPos( x, y );
	m_pPanel_guess->setVisible( true );

	m_pLoad_guess = new ImagePanel( m_pLoadImage_guess );
	m_pLoad_guess->setParent( m_pPanel_guess );
	m_pLoad_guess->setPos( loadX, loadY );
	m_pLoad_guess->setVisible( true );

	m_pExit_guess = new ImagePanel( m_pExitImage_guess );
	m_pExit_guess->setParent( m_pPanel_guess );
	m_pExit_guess->setPos( exitX, exitY );
	m_pExit_guess->setVisible( true );

	m_pLoadButton_guess = new ColorButton( "", loadX, loadY, 288, 36, false, true );
	m_pLoadButton_guess->setParent( m_pPanel_guess );
	m_pLoadButton_guess->addActionSignal( new CGameOverHandler_Command( this, 1 ) );
	m_pLoadButton_guess->setPos( loadX, loadY );

	m_pExitButton_guess = new ColorButton( "", exitButtonX, exitY, 105, 37, false, true );
	m_pExitButton_guess->setParent( m_pPanel_guess );
	m_pExitButton_guess->addActionSignal( new CGameOverHandler_Command( this, 2 ) );
	m_pExitButton_guess->setPos( exitButtonX, exitY );

	m_flAlpha_guess = 255;

	setBgColor( 0, 0, 0, 255 );

	Initialize();
}

CGameOver::~CGameOver()
{
	delete m_pImage_guess;
	delete m_pLoadImage_guess;
	delete m_pExitImage_guess;
}

void CGameOver::ActionSignal( int cmd )
{
	char szCommand[256];

	switch ( cmd )
	{
	case 1:
		if ( iDoctorMode_guess )
		{
			if ( strlen( m_szCustomDoc_guess ) > 2 )
			{
				strcpy( szCommand, "map " );
				strcat( szCommand, m_szCustomDoc_guess );
				gEngfuncs.pfnClientCmd( szCommand );
			}
			else
			{
				gEngfuncs.pfnClientCmd( "map c_doc_city" );
			}
		}
		else if ( gViewPort->m_pSaveLoad_guess )
		{
			setVisible( false );

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
			gEngfuncs.pfnPlaySoundByName( "ui/3d_click.wav", 1 );
		}
		break;
	case 2:
		setVisible( false );

		if ( iMenuMode_guess )
		{
			gEngfuncs.pfnClientCmd( "map c_game_menu1" );
			gViewPort->UpdateCursorState();
		}
		else
		{
			gEngfuncs.pfnServerCmd( "closegame" );
			gViewPort->UpdateCursorState();
		}
		break;
	}
}

void CGameOver::Initialize()
{
	m_szCustomDoc_guess[0] = 0;
	setVisible( false );
}

int MsgCustomDoc_guess( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	if ( gViewPort->m_pGameOver_guess )
		strcpy( gViewPort->m_pGameOver_guess->m_szCustomDoc_guess, READ_STRING() );

	return 1;
}

void VGUI_CustomDocInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "CustomDoc", MsgCustomDoc_guess );
}

void CGameOver::setVisible( bool state )
{
	if ( state )
	{
		if ( !iVGUIMenuNoMusic_guess )
			IrrKlangPlayMP3_guess( "game_over.mp3" );

		m_bLoadArmed_guess = false;
		m_bExitArmed_guess = false;
		m_flAlpha_guess = 255;

		if ( iDoctorMode_guess )
			m_pLoad_guess->setImage( vgui_LoadTGA( "gfx/vgui/game_over_restart.tga" ) );
		else
			m_pLoad_guess->setImage( vgui_LoadTGA( "gfx/vgui/game_over_load.tga" ) );

		m_pExit_guess->setImage( vgui_LoadTGA( "gfx/vgui/game_over_exit.tga" ) );
	}

	Panel::setVisible( state );
	gViewPort->UpdateCursorState();
}

void CGameOver::solve()
{
	if ( m_flAlpha_guess > 0 )
		m_flAlpha_guess -= gHUD.m_flTimeDelta * 500;

	if ( m_flAlpha_guess < 0 )
		m_flAlpha_guess = 0;

	m_pImage_guess->setColor( Color( 255, 255, 255, m_flAlpha_guess ) );
	m_pLoadImage_guess->setColor( Color( 255, 255, 255, m_flAlpha_guess ) );
	m_pExitImage_guess->setColor( Color( 255, 255, 255, m_flAlpha_guess ) );

	if ( !gEngfuncs.GetLocalPlayer() )
		return;

	if ( m_pLoadButton_guess->isArmed() )
	{
		if ( !m_bLoadArmed_guess )
		{
			if ( iDoctorMode_guess )
				m_pLoad_guess->setImage( vgui_LoadTGA( "gfx/vgui/game_over_restart2.tga" ) );
			else
				m_pLoad_guess->setImage( vgui_LoadTGA( "gfx/vgui/game_over_load2.tga" ) );

			gEngfuncs.pfnPlaySoundByName( "ui/3d_rollover.wav", 1 );
			m_bLoadArmed_guess = true;
		}
	}
	else if ( m_bLoadArmed_guess )
	{
		if ( iDoctorMode_guess )
			m_pLoad_guess->setImage( vgui_LoadTGA( "gfx/vgui/game_over_restart.tga" ) );
		else
			m_pLoad_guess->setImage( vgui_LoadTGA( "gfx/vgui/game_over_load.tga" ) );

		m_bLoadArmed_guess = false;
	}

	if ( m_pExitButton_guess->isArmed() )
	{
		if ( !m_bExitArmed_guess )
		{
			m_pExit_guess->setImage( vgui_LoadTGA( "gfx/vgui/game_over_exit2.tga" ) );
			gEngfuncs.pfnPlaySoundByName( "ui/3d_rollover.wav", 1 );
			m_bExitArmed_guess = true;
		}
	}
	else if ( m_bExitArmed_guess )
	{
		m_pExit_guess->setImage( vgui_LoadTGA( "gfx/vgui/game_over_exit.tga" ) );
		m_bExitArmed_guess = false;
	}

	Panel::solve();
}
