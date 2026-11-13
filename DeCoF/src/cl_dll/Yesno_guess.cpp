// ======================================
// Cry of Fear yes/no question panel
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "Yesno_guess.h"
#include "..\game_shared\vgui_loadtga.h"

CYesno::CYesno() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int x, y;

	m_pImage_guess = vgui_LoadTGA( "gfx/vgui/640_savegame.tga" );
	if ( m_pImage_guess )
		m_pImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/640_savegame.tga!\n" );

	x = ScreenWidth / 2 - 160;
	y = ScreenHeight / 1.25 - 100;

	m_pImagePanel_guess = new ImagePanel( m_pImage_guess );
	m_pImagePanel_guess->setParent( this );
	m_pImagePanel_guess->setPos( x, y );
	m_pImagePanel_guess->setVisible( true );

	m_pYesButton_guess = new CommandButton( "", 36, 149, 79, 42, true, true );
	m_pYesButton_guess->setParent( m_pImagePanel_guess );
	m_pYesButton_guess->addActionSignal( new CYesnoHandler_Command( this, 2 ) );

	m_pNoButton_guess = new CommandButton( "", 204, 149, 79, 42, true, true );
	m_pNoButton_guess->setParent( m_pImagePanel_guess );
	m_pNoButton_guess->addActionSignal( new CYesnoHandler_Command( this, 3 ) );

	setBgColor( 0, 0, 0, 255 );

	Initialize();
}

void CYesno::ActionSignal( int cmd )
{
	char szCommand[128];

	switch ( cmd )
	{
	case 0:
		break;
	case 1:
		setVisible( false );
		gViewPort->UpdateCursorState();
		break;
	case 2:
		if ( m_iActivate_guess )
		{
			sprintf( szCommand, "yesnoactivate %i", m_iActivate_guess );
			gEngfuncs.pfnServerCmd( szCommand );
			gEngfuncs.pfnPlaySoundByName( "save/save_yes.wav", 1 );
			setVisible( false );
			gViewPort->UpdateCursorState();
			if ( !strcmp( gEngfuncs.pfnGetLevelName(), "maps/video_stress_test.bsp" ) )
				gEngfuncs.pfnServerCmd( "stresscontinue" );
			break;
		}

		gEngfuncs.pfnServerCmd( "unfreeze 1024" );
		gEngfuncs.pfnPlaySoundByName( "save/save_no.wav", 1 );
		setVisible( false );
		gViewPort->UpdateCursorState();
		if ( !strcmp( gEngfuncs.pfnGetLevelName(), "maps/video_stress_test.bsp" ) )
			gEngfuncs.pfnServerCmd( "stresscontinue" );
		break;
	case 3:
		sprintf( szCommand, "yesnono %i", m_iActivate_guess );
		gEngfuncs.pfnServerCmd( szCommand );
		gEngfuncs.pfnPlaySoundByName( "save/save_no.wav", 1 );
		setVisible( false );
		gViewPort->UpdateCursorState();

		if ( !strcmp( gEngfuncs.pfnGetLevelName(), "maps/video_stress_test.bsp" ) )
			gEngfuncs.pfnServerCmd( "stresscontinue" );
		break;
	default:
		gEngfuncs.Con_DPrintf( "Unknown Yesno ActionSignal %i.\n", cmd );
		break;
	}
}

void CYesno::Initialize()
{
	setVisible( false );
	m_iActivate_guess = 0;
}

int MsgYesNo_guess( const char *pszName, int iSize, void *pbuf )
{
	char szImage[128];

	BEGIN_READ( pbuf, iSize );

	if ( gViewPort->m_pYesno_guess )
	{
		sprintf( szImage, "%s", READ_STRING() );
		gViewPort->m_pYesno_guess->m_iActivate_guess = READ_SHORT();

		if ( strlen( szImage ) > 1 )
		{
			if ( vgui_LoadTGA( szImage ) )
			{
				gViewPort->m_pYesno_guess->m_pImagePanel_guess->setImage( vgui_LoadTGA( szImage ) );
				gViewPort->m_pYesno_guess->setVisible( true );
				gViewPort->UpdateCursorState();
			}
			else
				gEngfuncs.Con_Printf( "Cannot load %s!\n", szImage );
		}
		else
			gEngfuncs.Con_Printf( "No message sent\n" );
	}

	return 1;
}

void YesNoInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "YesNo", MsgYesNo_guess );
}

CYesno::~CYesno( void )
{
	delete m_pImage_guess;
}
