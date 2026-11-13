// ======================================
// Cry of Fear padlock puzzle panel
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "Padlock_guess.h"
#include "..\game_shared\vgui_loadtga.h"

CPadlock::CPadlock() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int wide, tall;
	int x, y;
	CommandButton *pButton;

	m_pButtonsImage_guess = vgui_LoadTGA( "gfx/vgui/puzzle/padlock_buttons.tga" );
	if ( m_pButtonsImage_guess )
		m_pButtonsImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/puzzle/padlock_buttons.tga!\n" );

	if(!m_pButtonsImage_guess){Initialize();return;}
	m_pButtonsImage_guess->getSize( wide, tall );
	x = ScreenWidth / 2 - wide / 2;
	y = ScreenHeight - tall;

	m_pButtons_guess = new ImagePanel( m_pButtonsImage_guess );
	m_pButtons_guess->setParent( this );
	m_pButtons_guess->setPos( x, y );
	m_pButtons_guess->setVisible( true );

	pButton = new CommandButton( "", 18, 20, 66, 66, true, true );
	pButton->setParent( m_pButtons_guess );
	pButton->addActionSignal( new CPadlockHandler_Command( this, 1 ) );

	pButton = new CommandButton( "", 84, 20, 66, 66, true, true );
	pButton->setParent( m_pButtons_guess );
	pButton->addActionSignal( new CPadlockHandler_Command( this, 2 ) );

	pButton = new CommandButton( "", 150, 20, 66, 66, true, true );
	pButton->setParent( m_pButtons_guess );
	pButton->addActionSignal( new CPadlockHandler_Command( this, 3 ) );

	pButton = new CommandButton( "", 216, 20, 66, 66, true, true );
	pButton->setParent( m_pButtons_guess );
	pButton->addActionSignal( new CPadlockHandler_Command( this, 4 ) );

	pButton = new CommandButton( "", 117, 86, 66, 14, true, true );
	pButton->setParent( m_pButtons_guess );
	pButton->addActionSignal( new CPadlockHandler_Command( this, 5 ) );

	setBgColor( 0, 0, 0, 255 );

	Initialize();
}

void CPadlock::ActionSignal( int cmd )
{
	switch ( cmd )
	{
	case 1:
		gEngfuncs.pfnServerCmd( "padlockrotate 1" );
		break;
	case 2:
		gEngfuncs.pfnServerCmd( "padlockrotate 2" );
		break;
	case 3:
		gEngfuncs.pfnServerCmd( "padlockrotate 3" );
		break;
	case 4:
		gEngfuncs.pfnServerCmd( "padlockrotate 4" );
		break;
	case 5:
		gEngfuncs.pfnServerCmd( "padlockcancel" );
		setVisible( false );
		gViewPort->UpdateCursorState();
		break;
	}
}

void CPadlock::Initialize()
{
	setVisible( false );
}

int MsgPadlock_guess( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int iOn = READ_BYTE();

	if ( gViewPort && gViewPort->m_pPadlock_guess )
	{
		if ( iOn )
			gViewPort->m_pPadlock_guess->setVisible( true );
		else
			gViewPort->m_pPadlock_guess->setVisible( false );

		gViewPort->UpdateCursorState();
	}

	return 1;
}

void PadlockInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "Padlock", MsgPadlock_guess );
}

CPadlock::~CPadlock( void )
{
	delete m_pButtonsImage_guess;
}
