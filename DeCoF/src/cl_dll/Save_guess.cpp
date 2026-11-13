// ======================================
// Cry of Fear save question panel
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "Save_guess.h"
#include "..\game_shared\vgui_loadtga.h"

CSave::CSave() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int x, y;
	CommandButton *pButton;

	m_pImage_guess = vgui_LoadTGA( "gfx/vgui/640_savegame.tga" );
	if ( m_pImage_guess )
		m_pImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/640_panelback.tga!\n" );

	x = ScreenWidth / 2 - 160;
	y = ScreenHeight / 1.25 - 100;

	m_pImagePanel_guess = new ImagePanel( m_pImage_guess );
	m_pImagePanel_guess->setParent( this );
	m_pImagePanel_guess->setPos( x, y );
	m_pImagePanel_guess->setVisible( true );

	pButton = new CommandButton( "", 36, 149, 79, 42, true, true );
	pButton->setParent( m_pImagePanel_guess );
	pButton->addActionSignal( new CSaveHandler_Command( this, 2 ) );

	pButton = new CommandButton( "", 204, 149, 79, 42, true, true );
	pButton->setParent( m_pImagePanel_guess );
	pButton->addActionSignal( new CSaveHandler_Command( this, 3 ) );

	setBgColor( 0, 0, 0, 255 );

	Initialize();
}

void CSave::ActionSignal( int cmd )
{
	switch ( cmd )
	{
	case 0:
		break;
	case 1:
		gEngfuncs.pfnServerCmd( "unfreeze 666" );
		setVisible( false );
		gViewPort->UpdateCursorState();
		break;
	case 2:
		gEngfuncs.pfnServerCmd( "cofsave 19573" );
		gEngfuncs.pfnPlaySoundByName( "save/save_yes.wav", 1 );
		setVisible( false );
		gViewPort->UpdateCursorState();
		break;
	case 3:
		gEngfuncs.pfnServerCmd( "unfreeze 1024" );
		gEngfuncs.pfnPlaySoundByName( "save/save_no.wav", 1 );
		setVisible( false );
		gViewPort->UpdateCursorState();
		break;
	default:
		gEngfuncs.Con_DPrintf( "Unknown Save ActionSignal %i.\n", cmd );
		break;
	}
}

void CSave::Initialize()
{
	setVisible( false );
}

int MsgSave_guess( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	if ( gViewPort->m_pSave_guess )
	{
		if ( READ_BYTE() )
			gViewPort->m_pSave_guess->setVisible( true );
		else
			gViewPort->m_pSave_guess->setVisible( false );

		gViewPort->UpdateCursorState();
	}

	return 1;
}

void SaveInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "Save", MsgSave_guess );
}

CSave::~CSave( void )
{
	delete m_pImage_guess;
}
