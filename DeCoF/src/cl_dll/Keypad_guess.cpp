// ======================================
// Cry of Fear keypad puzzle panel
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "Keypad_guess.h"
#include "..\game_shared\vgui_loadtga.h"

CKeypad::CKeypad() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int wide, tall;
	int x, y;
	CommandButton *pButton;

	m_pBackgroundPanel_guess = new Panel( 0, 0, ScreenWidth, ScreenHeight );
	m_pBackgroundPanel_guess->setParent( this );
	m_pBackgroundPanel_guess->setBgColor( 0, 0, 0, 0 );

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

	m_pPanelBackImage_guess = vgui_LoadTGA( "gfx/vgui/640_panelback.tga" );
	if ( m_pPanelBackImage_guess )
		m_pPanelBackImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/640_panelback.tga!\n" );

	m_pPanel2BackImage_guess = vgui_LoadTGA( "gfx/vgui/640_panel2back.tga" );
	if ( m_pPanel2BackImage_guess )
		m_pPanel2BackImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/640_panel2back.tga!\n" );

	m_pPanelOffImage_guess = vgui_LoadTGA( "gfx/vgui/640_paneloff.tga" );
	if ( m_pPanelOffImage_guess )
		m_pPanelOffImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/640_paneloff.tga!\n" );

	m_pPanelOnImage_guess = vgui_LoadTGA( "gfx/vgui/640_panelon.tga" );
	if ( m_pPanelOnImage_guess )
		m_pPanelOnImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/640_panelon.tga!\n" );

	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hCreditsScheme = pSchemes->getSchemeHandle( "credits" );
	m_pFont_guess = pSchemes->getFont( hCreditsScheme );

	x = ScreenWidth / 2 - 320;
	y = ScreenHeight / 2 - 240;

	m_pPanelBack_guess = new ImagePanel( m_pPanelBackImage_guess );
	m_pPanelBack_guess->setParent( this );
	m_pPanelBack_guess->setPos( x, y );
	m_pPanelBack_guess->setVisible( true );

	m_pPanel2Back_guess = new ImagePanel( m_pPanel2BackImage_guess );
	m_pPanel2Back_guess->setParent( m_pPanelBack_guess );
	m_pPanel2Back_guess->setPos( 0, 0 );
	m_pPanel2Back_guess->setVisible( false );

	pButton = new CommandButton( "", 3, 3, 20, 20, true, true );
	pButton->setParent( m_pPanelBack_guess );
	pButton->addActionSignal( new CKeypadHandler_Command( this, 1 ) );

	m_pPanelOff_guess = new ImagePanel( m_pPanelOffImage_guess );
	m_pPanelOff_guess->setParent( m_pPanelBack_guess );
	m_pPanelOff_guess->setPos( 520, 0 );
	m_pPanelOff_guess->setVisible( true );

	m_pPanelOn_guess = new ImagePanel( m_pPanelOnImage_guess );
	m_pPanelOn_guess->setParent( m_pPanelBack_guess );
	m_pPanelOn_guess->setPos( 520, 0 );
	m_pPanelOn_guess->setVisible( false );

	m_pKey1_guess = new CommandButton( "", 170, 90, 100, 100, true, true );
	m_pKey1_guess->setParent( m_pPanelBack_guess );
	m_pKey1_guess->addActionSignal( new CKeypadHandler_Command( this, 2 ) );

	m_pKey2_guess = new CommandButton( "", 270, 90, 100, 100, true, true );
	m_pKey2_guess->setParent( m_pPanelBack_guess );
	m_pKey2_guess->addActionSignal( new CKeypadHandler_Command( this, 3 ) );

	m_pKey3_guess = new CommandButton( "", 370, 90, 100, 100, true, true );
	m_pKey3_guess->setParent( m_pPanelBack_guess );
	m_pKey3_guess->addActionSignal( new CKeypadHandler_Command( this, 4 ) );

	m_pKey4_guess = new CommandButton( "", 170, 190, 100, 100, true, true );
	m_pKey4_guess->setParent( m_pPanelBack_guess );
	m_pKey4_guess->addActionSignal( new CKeypadHandler_Command( this, 5 ) );

	m_pKey5_guess = new CommandButton( "", 270, 190, 100, 100, true, true );
	m_pKey5_guess->setParent( m_pPanelBack_guess );
	m_pKey5_guess->addActionSignal( new CKeypadHandler_Command( this, 6 ) );

	m_pKey6_guess = new CommandButton( "", 370, 190, 100, 100, true, true );
	m_pKey6_guess->setParent( m_pPanelBack_guess );
	m_pKey6_guess->addActionSignal( new CKeypadHandler_Command( this, 7 ) );

	m_pKey7_guess = new CommandButton( "", 170, 290, 100, 100, true, true );
	m_pKey7_guess->setParent( m_pPanelBack_guess );
	m_pKey7_guess->addActionSignal( new CKeypadHandler_Command( this, 8 ) );

	m_pKey8_guess = new CommandButton( "", 270, 290, 100, 100, true, true );
	m_pKey8_guess->setParent( m_pPanelBack_guess );
	m_pKey8_guess->addActionSignal( new CKeypadHandler_Command( this, 9 ) );

	m_pKey9_guess = new CommandButton( "", 370, 290, 100, 100, true, true );
	m_pKey9_guess->setParent( m_pPanelBack_guess );
	m_pKey9_guess->addActionSignal( new CKeypadHandler_Command( this, 10 ) );

	m_pCodeLabel_guess = new Label( "", 247, 39, 149, 32 );
	m_pCodeLabel_guess->setParent( m_pPanelBack_guess );
	m_pCodeLabel_guess->setFont( m_pFont_guess );
	m_pCodeLabel_guess->setFgColor( 0, 0, 0, 1 );
	m_pCodeLabel_guess->setBgColor( 0, 0, 0, 255 );
	m_pCodeLabel_guess->setText( "    " );
	m_pCodeLabel_guess->setContentAlignment( Label::a_center );

	setBgColor( 0, 0, 0, 255 );

	Initialize();
}

CKeypad::~CKeypad()
{
	delete m_pPanelBackImage_guess;
	delete m_pPanelOffImage_guess;
	delete m_pPanelOnImage_guess;
}

void CKeypad::ActionSignal( int cmd )
{
	switch ( cmd )
	{
	case 0:
		break;
	case 1:
		setVisible( false );
		gViewPort->UpdateCursorState();
		break;
	case 2:
		EnterKey_guess( 1 );
		break;
	case 3:
		EnterKey_guess( 2 );
		break;
	case 4:
		EnterKey_guess( 3 );
		break;
	case 5:
		EnterKey_guess( 4 );
		break;
	case 6:
		EnterKey_guess( 5 );
		break;
	case 7:
		EnterKey_guess( 6 );
		break;
	case 8:
		EnterKey_guess( 7 );
		break;
	case 9:
		EnterKey_guess( 8 );
		break;
	case 10:
		EnterKey_guess( 9 );
		break;
	default:
		gEngfuncs.Con_DPrintf( "Unknown Keypad ActionSignal %i.\n", cmd );
		break;
	}
}

void CKeypad::Activate_guess()
{
	gEngfuncs.pfnPlaySoundByName( "keypad/keypad_activate.wav", 1 );

	if ( m_iType_guess == 0 )
	{
		m_pPanelOn_guess->setVisible( true );
		m_pPanelOff_guess->setVisible( false );
	}
}

void CKeypad::EnterKey_guess( int key )
{
	char szDigit[4];
	char szStars[8];

	if ( m_iActivated_guess )
	{
		gEngfuncs.pfnPlaySoundByName( "keypad/keypad_press.wav", 1 );
		return;
	}

	if ( strlen( m_szCode_guess ) == 4 )
	{
		if ( m_iType_guess == 2 )
		{
			for ( int i = 1; i <= strlen( m_szCode_guess ); i++ )
			{
				if ( i == 1 )
					strcpy( szStars, "*" );
				else
					strcat( szStars, "*" );
			}

			m_pCodeLabel_guess->setText( szStars );
			m_pCodeLabel_guess->setContentAlignment( Label::a_center );
		}

		Submit_guess();
		return;
	}

	itoa( key, szDigit, 10 );
	strcat( m_szCode_guess, szDigit );
	gEngfuncs.pfnPlaySoundByName( "keypad/keypad_press.wav", 1 );

	if ( m_iType_guess == 2 )
	{
		for ( int i = 1; i <= strlen( m_szCode_guess ); i++ )
		{
			if ( i == 1 )
				strcpy( szStars, "*" );
			else
				strcat( szStars, "*" );
		}

		m_pCodeLabel_guess->setText( szStars );
		m_pCodeLabel_guess->setContentAlignment( Label::a_center );
	}

	if ( strlen( m_szCode_guess ) == 4 )
		Submit_guess();
}

void CKeypad::Initialize()
{
	strncpy( m_szCode_guess, "", 8 );
	setVisible( false );

	m_iType_guess = 0;
	m_iActivated_guess = 0;

	m_pPanelBack_guess->setVisible( true );
	m_pPanel2Back_guess->setVisible( false );

	if ( m_iType_guess == 0 )
	{
		m_pPanelOff_guess->setVisible( true );
		m_pPanelOn_guess->setVisible( false );
	}
	else
	{
		m_pPanelOff_guess->setVisible( false );
		m_pPanelOn_guess->setVisible( false );
	}
}

int CKeypad::KeyInput( int down, int keynum, const char *pszCurrentBinding )
{
	if ( pszCurrentBinding && ( !strcmp( pszCurrentBinding, "toggleconsole" ) || !strcmp( pszCurrentBinding, "keypadnumber" ) ) )
		return 1;

	return 0;
}

int MsgKeypad_guess( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	if ( gViewPort->m_pKeypad_guess )
	{
		gViewPort->m_pKeypad_guess->m_iType_guess = READ_BYTE();
		gViewPort->m_pKeypad_guess->m_iActivated_guess = READ_BYTE();
		gViewPort->m_pKeypad_guess->UpdateLayout_guess();

		int iOn = READ_BYTE();

		switch ( gViewPort->m_pKeypad_guess->m_iType_guess )
		{
		case 0:
			gViewPort->m_pKeypad_guess->m_pPanelBack_guess->setImage( vgui_LoadTGA( "gfx/vgui/640_panelback.tga" ) );
			gViewPort->m_pKeypad_guess->m_pPanelOff_guess->setVisible( true );
			gViewPort->m_pKeypad_guess->m_pPanelOn_guess->setVisible( false );
			break;
		case 1:
			gViewPort->m_pKeypad_guess->m_pPanelOff_guess->setVisible( false );
			gViewPort->m_pKeypad_guess->m_pPanelOn_guess->setVisible( false );
			gViewPort->m_pKeypad_guess->m_pPanelBack_guess->setImage( vgui_LoadTGA( "gfx/vgui/640_panel2back.tga" ) );
			break;
		case 2:
			gViewPort->m_pKeypad_guess->m_pPanelOff_guess->setVisible( false );
			gViewPort->m_pKeypad_guess->m_pPanelOn_guess->setVisible( false );
			gViewPort->m_pKeypad_guess->m_pPanelBack_guess->setImage( vgui_LoadTGA( "gfx/vgui/keypad3.tga" ) );
			break;
		}

		if ( gViewPort->m_pKeypad_guess->m_iActivated_guess )
			gViewPort->m_pKeypad_guess->Activate_guess();

		if ( !gViewPort->m_pKeypad_guess->isVisible() && iOn )
		{
			gViewPort->m_pKeypad_guess->setVisible( true );
			gViewPort->UpdateCursorState();
		}
	}

	return 1;
}

void CKeypad::Submit_guess()
{
	char szCommand[128];

	sprintf( szCommand, "keypadnumber %s", m_szCode_guess );
	gEngfuncs.pfnServerCmd( szCommand );

	strncpy( m_szCode_guess, "", 8 );
}

void CKeypad::UpdateLayout_guess()
{
	if ( m_iType_guess == 0 || m_iType_guess == 1 )
	{
		m_pCodeLabel_guess->setText( "" );
		m_pCodeLabel_guess->setVisible( false );
	}
	else
	{
		m_pCodeLabel_guess->setText( "" );
		m_pCodeLabel_guess->setVisible( true );
	}

	m_pKey1_guess->setSize( 100, 100 );
	m_pKey1_guess->setPos( 170, 90 );
	m_pKey2_guess->setSize( 100, 100 );
	m_pKey2_guess->setPos( 270, 90 );
	m_pKey3_guess->setSize( 100, 100 );
	m_pKey3_guess->setPos( 370, 90 );
	m_pKey4_guess->setSize( 100, 100 );
	m_pKey4_guess->setPos( 170, 190 );
	m_pKey5_guess->setSize( 100, 100 );
	m_pKey5_guess->setPos( 270, 190 );
	m_pKey6_guess->setSize( 100, 100 );
	m_pKey6_guess->setPos( 370, 190 );
	m_pKey7_guess->setSize( 100, 100 );
	m_pKey7_guess->setPos( 170, 290 );
	m_pKey8_guess->setSize( 100, 100 );
	m_pKey8_guess->setPos( 270, 290 );
	m_pKey9_guess->setSize( 100, 100 );
	m_pKey9_guess->setPos( 370, 290 );
}

void VGUI_KeypadInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "Keypad", MsgKeypad_guess );
}
