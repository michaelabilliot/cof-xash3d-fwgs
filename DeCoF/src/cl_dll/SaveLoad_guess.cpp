// ======================================
// Cry of Fear save and load slots panel
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include <fstream>
#include <string>
#include <vector>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "com_weapons.h"
#include "vgui_TeamFortressViewport.h"
#include "SaveLoad_guess.h"
#include "GameMenu_guess.h"
#include "GameOver_guess.h"
#include "..\game_shared\vgui_loadtga.h"
#include "event_api.h"
#include <windows.h>
#include <time.h>

extern int iMenuMode_guess;

void IrrKlangStop_guess( float flFadeTime );
void GetCurrentPath( char *path );

CSaveLoad::CSaveLoad() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int wide, tall;
	int x, y;
	Panel *pPanel;
	CommandButton *pButton;

	m_pBackgroundImage_guess = vgui_LoadTGA( "gfx/vgui/save_load.tga" );
	if ( m_pBackgroundImage_guess )
		m_pBackgroundImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/save_load.tga!\n" );

	m_pBackgroundImage_guess->getSize( wide, tall );
	x = ScreenWidth / 2 - wide / 2;
	y = ScreenHeight / 2 - tall / 2;

	m_pBackground_guess = new ImagePanel( m_pBackgroundImage_guess );
	m_pBackground_guess->setParent( this );
	m_pBackground_guess->setPos( x, y );
	m_pBackground_guess->setVisible( true );

	pButton = new CommandButton( "", 260, 398, 119, 43, true, true );
	pButton->setParent( m_pBackground_guess );
	pButton->addActionSignal( new CSaveLoadHandler_Command( this, 1 ) );

	m_pSlot1Label_guess = new Label( "", 93, 154, 455, 29 );
	m_pSlot1Label_guess->setParent( m_pBackground_guess );
	m_pSlot1Label_guess->setBgColor( 0, 0, 0, 1 );
	m_pSlot1Label_guess->setFgColor( 255, 255, 255, 1 );

	m_pSlot2Label_guess = new Label( "", 93, 190, 455, 29 );
	m_pSlot2Label_guess->setParent( m_pBackground_guess );
	m_pSlot2Label_guess->setBgColor( 0, 0, 0, 1 );
	m_pSlot2Label_guess->setFgColor( 255, 255, 255, 1 );

	m_pSlot3Label_guess = new Label( "", 93, 226, 455, 29 );
	m_pSlot3Label_guess->setParent( m_pBackground_guess );
	m_pSlot3Label_guess->setBgColor( 0, 0, 0, 1 );
	m_pSlot3Label_guess->setFgColor( 255, 255, 255, 1 );

	m_pSlot4Label_guess = new Label( "", 93, 262, 455, 29 );
	m_pSlot4Label_guess->setParent( m_pBackground_guess );
	m_pSlot4Label_guess->setBgColor( 0, 0, 0, 1 );
	m_pSlot4Label_guess->setFgColor( 255, 255, 255, 1 );

	m_pSlot5Label_guess = new Label( "", 93, 296, 455, 29 );
	m_pSlot5Label_guess->setParent( m_pBackground_guess );
	m_pSlot5Label_guess->setBgColor( 0, 0, 0, 1 );
	m_pSlot5Label_guess->setFgColor( 255, 255, 255, 1 );

	m_pSlot1Button_guess = new ColorButton( "", 93, 154, 455, 29, false, false );
	m_pSlot1Button_guess->setParent( m_pBackground_guess );
	m_pSlot1Button_guess->addActionSignal( new CSaveLoadHandler_Command( this, 2 ) );

	m_pSlot2Button_guess = new ColorButton( "", 93, 190, 455, 29, true, true );
	m_pSlot2Button_guess->setParent( m_pBackground_guess );
	m_pSlot2Button_guess->addActionSignal( new CSaveLoadHandler_Command( this, 3 ) );

	m_pSlot3Button_guess = new ColorButton( "", 93, 226, 455, 29, true, true );
	m_pSlot3Button_guess->setParent( m_pBackground_guess );
	m_pSlot3Button_guess->addActionSignal( new CSaveLoadHandler_Command( this, 4 ) );

	m_pSlot4Button_guess = new ColorButton( "", 93, 262, 455, 29, true, true );
	m_pSlot4Button_guess->setParent( m_pBackground_guess );
	m_pSlot4Button_guess->addActionSignal( new CSaveLoadHandler_Command( this, 5 ) );

	m_pSlot5Button_guess = new ColorButton( "", 93, 296, 455, 29, true, true );
	m_pSlot5Button_guess->setParent( m_pBackground_guess );
	m_pSlot5Button_guess->addActionSignal( new CSaveLoadHandler_Command( this, 6 ) );

	// black out the screen around the picture
	pPanel = new Panel( 0, 0, 0, 0 );
	pPanel->setParent( this );
	pPanel->setSize( ScreenWidth / 2 - wide / 2, ScreenHeight );
	pPanel->setBgColor( 0, 0, 0, 0 );
	pPanel->setVisible( true );

	pPanel = new Panel( 0, 0, 0, 0 );
	pPanel->setParent( this );
	pPanel->setSize( ScreenWidth / 2 - wide / 2, ScreenHeight );
	pPanel->setPos( ScreenWidth / 2 + wide / 2, 0 );
	pPanel->setBgColor( 0, 0, 0, 0 );
	pPanel->setVisible( true );

	pPanel = new Panel( 0, 0, 0, 0 );
	pPanel->setParent( this );
	pPanel->setSize( wide, ScreenHeight / 2 - tall / 2 );
	pPanel->setPos( ScreenWidth / 2 - wide / 2, 0 );
	pPanel->setBgColor( 0, 0, 0, 0 );
	pPanel->setVisible( true );

	pPanel = new Panel( 0, 0, 0, 0 );
	pPanel->setParent( this );
	pPanel->setSize( wide, ScreenHeight / 2 - tall / 2 );
	pPanel->setPos( ScreenWidth / 2 - wide / 2, ScreenHeight / 2 + tall / 2 );
	pPanel->setBgColor( 0, 0, 0, 0 );
	pPanel->setVisible( true );

	setBgColor( 0, 0, 0, 255 );

	Initialize();
}

void CSaveLoad::ActionSignal( int cmd )
{
	cl_entity_t *pPlayer = gEngfuncs.GetLocalPlayer();

	switch ( cmd )
	{
	case 1:
		if ( !strcmp( gEngfuncs.pfnGetLevelName(), "maps/c_loadgame.bsp" ) )
		{
			setVisible( false );
			gViewPort->UpdateCursorState();
			gEngfuncs.pfnServerCmd( "closegame" );

			if ( pPlayer )
				gEngfuncs.pEventAPI->EV_PlaySound( -1, pPlayer->curstate.origin, CHAN_AUTO, "save/save_no.wav", 1.0, 0.8, 0, PITCH_NORM );
		}
		else if ( strstr( gEngfuncs.pfnGetLevelName(), "game_menu" ) )
		{
			setVisible( false );
			gViewPort->UpdateCursorState();

			if ( gViewPort && gViewPort->m_pGameMenu_guess )
			{
				gViewPort->m_pGameMenu_guess->setVisible( true );
				gViewPort->UpdateCursorState();
			}
		}
		else if ( strstr( gEngfuncs.pfnGetLevelName(), "difficulty_" ) || strstr( gEngfuncs.pfnGetLevelName(), "server_" ) || strstr( gEngfuncs.pfnGetLevelName(), "unlockables" ) )
		{
			if ( iMenuMode_guess )
			{
				setVisible( false );
				gViewPort->UpdateCursorState();
				gEngfuncs.pfnClientCmd( "map c_game_menu1" );
			}
			else
			{
				setVisible( false );
				gViewPort->UpdateCursorState();
				gEngfuncs.pfnClientCmd( "disconnect" );
			}
		}
		else if ( CL_IsDead() )
		{
			setVisible( false );
			gViewPort->UpdateCursorState();

			if ( gViewPort && gViewPort->m_pGameOver_guess )
			{
				gViewPort->m_pGameOver_guess->setVisible( true );
				gViewPort->UpdateCursorState();
			}
		}
		else
		{
			setVisible( false );
			gViewPort->UpdateCursorState();
			gEngfuncs.pfnServerCmd( "unfreeze 666" );

			if ( pPlayer )
				gEngfuncs.pEventAPI->EV_PlaySound( -1, pPlayer->curstate.origin, CHAN_AUTO, "save/save_no.wav", 1.0, 0.8, 0, PITCH_NORM );
		}
		break;
	case 2:
		if ( gEngfuncs.GetClientTime() < m_flSlot1ClickTime_guess )
		{
			if ( m_iMode_guess )
			{
				byte *pFile = gEngfuncs.COM_LoadFile( "SAVE/saveinfo1.cof", 5, NULL );

				if ( pFile )
				{
					setVisible( false );
					gViewPort->UpdateCursorState();
					gEngfuncs.pfnServerCmd( "unfreeze 1024" );

					if ( strstr( gEngfuncs.pfnGetLevelName(), "game_menu" ) )
						IrrKlangStop_guess( 0 );

					gEngfuncs.COM_FreeFile( pFile );
					gEngfuncs.pfnServerCmd( "cofload 1" );
				}
			}
			else
			{
				setVisible( false );
				gViewPort->UpdateCursorState();
				gEngfuncs.pfnServerCmd( "unfreezesave 1024 1" );

				gHUD.m_flSaveHackTime_guess = gEngfuncs.GetClientTime() + 3.25;
				gHUD.m_iSaveHackSlot_guess = 1;
				gHUD.m_bSaveHack_guess = true;

				WriteSaveInfo_guess( "saveinfo1.cof" );
			}
		}

		m_flSlot1ClickTime_guess = gEngfuncs.GetClientTime() + 0.5;
		break;
	case 3:
		if ( gEngfuncs.GetClientTime() < m_flSlot2ClickTime_guess )
		{
			if ( m_iMode_guess )
			{
				byte *pFile = gEngfuncs.COM_LoadFile( "SAVE/saveinfo2.cof", 5, NULL );

				if ( pFile )
				{
					setVisible( false );
					gViewPort->UpdateCursorState();
					gEngfuncs.pfnServerCmd( "unfreeze 1024" );

					if ( strstr( gEngfuncs.pfnGetLevelName(), "game_menu" ) )
						IrrKlangStop_guess( 0 );

					gEngfuncs.COM_FreeFile( pFile );
					gEngfuncs.pfnServerCmd( "cofload 2" );
				}
			}
			else
			{
				setVisible( false );
				gViewPort->UpdateCursorState();
				gEngfuncs.pfnServerCmd( "unfreezesave 1024 2" );

				gHUD.m_flSaveHackTime_guess = gEngfuncs.GetClientTime() + 3.25;
				gHUD.m_iSaveHackSlot_guess = 2;
				gHUD.m_bSaveHack_guess = true;

				WriteSaveInfo_guess( "saveinfo2.cof" );
			}
		}

		m_flSlot2ClickTime_guess = gEngfuncs.GetClientTime() + 0.5;
		break;
	case 4:
		if ( gEngfuncs.GetClientTime() < m_flSlot3ClickTime_guess )
		{
			if ( m_iMode_guess )
			{
				byte *pFile = gEngfuncs.COM_LoadFile( "SAVE/saveinfo3.cof", 5, NULL );

				if ( pFile )
				{
					setVisible( false );
					gViewPort->UpdateCursorState();
					gEngfuncs.pfnServerCmd( "unfreeze 1024" );

					if ( strstr( gEngfuncs.pfnGetLevelName(), "game_menu" ) )
						IrrKlangStop_guess( 0 );

					gEngfuncs.COM_FreeFile( pFile );
					gEngfuncs.pfnServerCmd( "cofload 3" );
				}
			}
			else
			{
				setVisible( false );
				gViewPort->UpdateCursorState();
				gEngfuncs.pfnServerCmd( "unfreezesave 1024 3" );

				gHUD.m_flSaveHackTime_guess = gEngfuncs.GetClientTime() + 3.25;
				gHUD.m_iSaveHackSlot_guess = 3;
				gHUD.m_bSaveHack_guess = true;

				WriteSaveInfo_guess( "saveinfo3.cof" );
			}
		}

		m_flSlot3ClickTime_guess = gEngfuncs.GetClientTime() + 0.5;
		break;
	case 5:
		if ( gEngfuncs.GetClientTime() < m_flSlot4ClickTime_guess )
		{
			if ( m_iMode_guess )
			{
				byte *pFile = gEngfuncs.COM_LoadFile( "SAVE/saveinfo4.cof", 5, NULL );

				if ( pFile )
				{
					setVisible( false );
					gViewPort->UpdateCursorState();
					gEngfuncs.pfnServerCmd( "unfreeze 1024" );

					if ( strstr( gEngfuncs.pfnGetLevelName(), "game_menu" ) )
						IrrKlangStop_guess( 0 );

					gEngfuncs.COM_FreeFile( pFile );
					gEngfuncs.pfnServerCmd( "cofload 4" );
				}
			}
			else
			{
				setVisible( false );
				gViewPort->UpdateCursorState();
				gEngfuncs.pfnServerCmd( "unfreezesave 1024 4" );

				gHUD.m_flSaveHackTime_guess = gEngfuncs.GetClientTime() + 3.25;
				gHUD.m_iSaveHackSlot_guess = 4;
				gHUD.m_bSaveHack_guess = true;

				WriteSaveInfo_guess( "saveinfo4.cof" );
			}
		}

		m_flSlot4ClickTime_guess = gEngfuncs.GetClientTime() + 0.5;
		break;
	case 6:
		if ( gEngfuncs.GetClientTime() < m_flSlot5ClickTime_guess )
		{
			if ( m_iMode_guess )
			{
				byte *pFile = gEngfuncs.COM_LoadFile( "SAVE/saveinfo5.cof", 5, NULL );

				if ( pFile )
				{
					setVisible( false );
					gViewPort->UpdateCursorState();
					gEngfuncs.pfnServerCmd( "unfreeze 1024" );

					if ( strstr( gEngfuncs.pfnGetLevelName(), "game_menu" ) )
						IrrKlangStop_guess( 0 );

					gEngfuncs.COM_FreeFile( pFile );
					gEngfuncs.pfnServerCmd( "cofload 5" );
				}
			}
			else
			{
				setVisible( false );
				gViewPort->UpdateCursorState();
				gEngfuncs.pfnServerCmd( "unfreezesave 1024 5" );

				gHUD.m_flSaveHackTime_guess = gEngfuncs.GetClientTime() + 3.25;
				gHUD.m_iSaveHackSlot_guess = 5;
				gHUD.m_bSaveHack_guess = true;

				WriteSaveInfo_guess( "saveinfo5.cof" );
			}
		}

		m_flSlot5ClickTime_guess = gEngfuncs.GetClientTime() + 0.5;
		break;
	}
}

void CSaveLoad::Initialize()
{
	setVisible( false );
}

int MsgSaveLoad_guess( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

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

		gViewPort->m_pSaveLoad_guess->m_iMode_guess = READ_BYTE();

		if ( !gViewPort->m_pSaveLoad_guess->m_iMode_guess )
			strcpy( gViewPort->m_pSaveLoad_guess->m_szSaveName_guess, READ_STRING() );

		gViewPort->m_pSaveLoad_guess->ReadAllSlots_guess();
		gViewPort->m_pSaveLoad_guess->setVisible( true );
		gViewPort->UpdateCursorState();
		gEngfuncs.pfnServerCmd( "freeze 1024" );

		cl_entity_t *pPlayer = gEngfuncs.GetLocalPlayer();

		if ( pPlayer )
			gEngfuncs.pEventAPI->EV_PlaySound( -1, pPlayer->curstate.origin, CHAN_AUTO, "save/save_yes.wav", 1.0, 0.8, 0, PITCH_NORM );
	}

	return 1;
}

bool PathExists_guess( const char *pszPath )
{
	return GetFileAttributes( pszPath ) != INVALID_FILE_ATTRIBUTES;
}

void CSaveLoad::ReadAllSlots_guess()
{
	m_pSlot1Label_guess->setText( "" );
	m_pSlot2Label_guess->setText( "" );
	m_pSlot3Label_guess->setText( "" );
	m_pSlot4Label_guess->setText( "" );
	m_pSlot5Label_guess->setText( "" );

	ReadSlot_guess( 1 );
	ReadSlot_guess( 2 );
	ReadSlot_guess( 3 );
	ReadSlot_guess( 4 );
	ReadSlot_guess( 5 );
}

void CSaveLoad::ReadSlot_guess( int slot )
{
	char szPath[256];

	strcpy( szPath, gEngfuncs.pfnGetGameDirectory() );
	strcat( szPath, "/SAVE/" );
	sprintf( szPath, "%ssaveinfo%i.cof", szPath, slot );

	std::ifstream file( szPath );
	std::string line;
	std::vector<std::string> lines;

	if ( file )
	{
		while ( std::getline( file, line ) )
			lines.push_back( line );

		switch ( slot )
		{
		case 1:
			m_pSlot1Label_guess->setText( line.c_str() );
			m_pSlot1Label_guess->setContentAlignment( Label::a_center );
			break;
		case 2:
			m_pSlot2Label_guess->setText( line.c_str() );
			m_pSlot2Label_guess->setContentAlignment( Label::a_center );
			break;
		case 3:
			m_pSlot3Label_guess->setText( line.c_str() );
			m_pSlot3Label_guess->setContentAlignment( Label::a_center );
			break;
		case 4:
			m_pSlot4Label_guess->setText( line.c_str() );
			m_pSlot4Label_guess->setContentAlignment( Label::a_center );
			break;
		case 5:
			m_pSlot5Label_guess->setText( line.c_str() );
			m_pSlot5Label_guess->setContentAlignment( Label::a_center );
			break;
		}
	}
}

void SaveLoadInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "SaveLoad", MsgSaveLoad_guess );
}

void CSaveLoad::WriteSaveInfo_guess( const char *pszFile )
{
	char szDir[MAX_PATH];
	char szPath[256];
	char szTime[256];

	GetCurrentPath( szDir );
	strcat( szDir, "/" );
	strcat( szDir, gEngfuncs.pfnGetGameDirectory() );
	strcat( szDir, "/SAVE/" );

	for ( int i = 0; i < (int)strlen( szDir ); i++ )
	{
		if ( szDir[i] == '/' )
			szDir[i] = '\\';
	}

	if ( !PathExists_guess( szDir ) )
	{
		gEngfuncs.Con_Printf( "Creating save directory %s\n", szDir );

		if ( CreateDirectory( szDir, NULL ) )
			gEngfuncs.Con_Printf( "Made save directory\n" );
	}

	strcpy( szPath, gEngfuncs.pfnGetGameDirectory() );
	strcat( szPath, "/SAVE/" );
	strcat( szPath, pszFile );

	std::ofstream file;
	file.open( szPath );

	if ( file )
	{
		time_t t;
		time( &t );
		strcpy( szTime, ctime( &t ) );

		if ( szTime[strlen( szTime ) - 1] == '\n' )
			szTime[strlen( szTime ) - 1] = 0;

		file << m_szSaveName_guess << " - " << szTime;
		file.close();
	}
}

CSaveLoad::~CSaveLoad( void )
{
	delete m_pBackgroundImage_guess;
}
