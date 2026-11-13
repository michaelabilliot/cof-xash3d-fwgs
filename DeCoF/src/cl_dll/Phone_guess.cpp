// ======================================
// Cry of Fear mobile phone panel
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "Phone_guess.h"
#include "..\game_shared\vgui_loadtga.h"

CPhone::CPhone() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int wide, tall;
	int x, y;
	Panel *pPanel;
	CommandButton *pButton;

	pPanel = new Panel( 0, 0, ScreenWidth, ScreenHeight );
	pPanel->setParent( this );
	pPanel->setSize( ScreenWidth, ScreenHeight );
	pPanel->setPos( 0, 0 );
	pPanel->setBgColor( 0, 0, 0, 0 );
	pPanel->setVisible( true );

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

	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );

	x = ScreenWidth / 2 - 131;
	y = ScreenHeight / 2 - 281;

	m_pPhoneImage_guess = vgui_LoadTGA( "gfx/vgui/640_phone.tga" );
	if ( m_pPhoneImage_guess )
		m_pPhoneImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/640_phone.tga!\n" );

	m_pPhone_guess = new ImagePanel( m_pPhoneImage_guess );
	m_pPhone_guess->setParent( this );
	m_pPhone_guess->setPos( x, y );
	m_pPhone_guess->setVisible( true );

	pButton = new CommandButton( "", 17, 306, 64, 26, true, true );
	pButton->setParent( m_pPhone_guess );
	pButton->addActionSignal( new CPhoneHandler_Command( this, 1 ) );

	pButton = new CommandButton( "", 97, 306, 64, 26, true, true );
	pButton->setParent( m_pPhone_guess );
	pButton->addActionSignal( new CPhoneHandler_Command( this, 2 ) );

	pButton = new CommandButton( "", 177, 306, 64, 26, true, true );
	pButton->setParent( m_pPhone_guess );
	pButton->addActionSignal( new CPhoneHandler_Command( this, 3 ) );

	pButton = new CommandButton( "", 17, 344, 64, 26, true, true );
	pButton->setParent( m_pPhone_guess );
	pButton->addActionSignal( new CPhoneHandler_Command( this, 4 ) );

	pButton = new CommandButton( "", 97, 344, 64, 26, true, true );
	pButton->setParent( m_pPhone_guess );
	pButton->addActionSignal( new CPhoneHandler_Command( this, 5 ) );

	pButton = new CommandButton( "", 177, 344, 64, 26, true, true );
	pButton->setParent( m_pPhone_guess );
	pButton->addActionSignal( new CPhoneHandler_Command( this, 6 ) );

	pButton = new CommandButton( "", 17, 382, 64, 26, true, true );
	pButton->setParent( m_pPhone_guess );
	pButton->addActionSignal( new CPhoneHandler_Command( this, 7 ) );

	pButton = new CommandButton( "", 97, 382, 64, 26, true, true );
	pButton->setParent( m_pPhone_guess );
	pButton->addActionSignal( new CPhoneHandler_Command( this, 8 ) );

	pButton = new CommandButton( "", 177, 382, 64, 26, true, true );
	pButton->setParent( m_pPhone_guess );
	pButton->addActionSignal( new CPhoneHandler_Command( this, 9 ) );

	pButton = new CommandButton( "", 98, 420, 64, 26, true, true );
	pButton->setParent( m_pPhone_guess );
	pButton->addActionSignal( new CPhoneHandler_Command( this, 10 ) );

	pButton = new CommandButton( "", 94, 220, 75, 75, true, true );
	pButton->setParent( m_pPhone_guess );
	pButton->addActionSignal( new CPhoneHandler_Command( this, 11 ) );

	m_pNumberLabel_guess = new Label( "", 65, 84, 131, 20 );
	m_pNumberLabel_guess->setVisible( true );
	m_pNumberLabel_guess->setParent( m_pPhone_guess );
	m_pNumberLabel_guess->setFgColor( 255, 255, 255, 0 );
	m_pNumberLabel_guess->setBgColor( 0, 0, 0, 0 );

	setBgColor( 0, 0, 0, 255 );

	Initialize();
}

void CPhone::ActionSignal( int cmd )
{
	switch ( cmd )
	{
	case 1:
		AddDigit_guess( 1 );
		break;
	case 2:
		AddDigit_guess( 2 );
		break;
	case 3:
		AddDigit_guess( 3 );
		break;
	case 4:
		AddDigit_guess( 4 );
		break;
	case 5:
		AddDigit_guess( 5 );
		break;
	case 6:
		AddDigit_guess( 6 );
		break;
	case 7:
		AddDigit_guess( 7 );
		break;
	case 8:
		AddDigit_guess( 8 );
		break;
	case 9:
		AddDigit_guess( 9 );
		break;
	case 10:
		AddDigit_guess( 0 );
		break;
	case 11:
		PhoneDial_guess();
		break;
	}
}

void CPhone::AddDigit_guess( int digit )
{
	char szDigit[4];

	sprintf( szDigit, "%d", digit );

	switch ( digit )
	{
	case 0:
		gEngfuncs.pfnPlaySoundByName( "phone/tone_zero.wav", 1 );
		break;
	case 1:
		gEngfuncs.pfnPlaySoundByName( "phone/tone_one.wav", 1 );
		break;
	case 2:
		gEngfuncs.pfnPlaySoundByName( "phone/tone_two.wav", 1 );
		break;
	case 3:
		gEngfuncs.pfnPlaySoundByName( "phone/tone_three.wav", 1 );
		break;
	case 4:
		gEngfuncs.pfnPlaySoundByName( "phone/tone_four.wav", 1 );
		break;
	case 5:
		gEngfuncs.pfnPlaySoundByName( "phone/tone_five.wav", 1 );
		break;
	case 6:
		gEngfuncs.pfnPlaySoundByName( "phone/tone_six.wav", 1 );
		break;
	case 7:
		gEngfuncs.pfnPlaySoundByName( "phone/tone_seven.wav", 1 );
		break;
	case 8:
		gEngfuncs.pfnPlaySoundByName( "phone/tone_eight.wav", 1 );
		break;
	case 9:
		gEngfuncs.pfnPlaySoundByName( "phone/tone_nine.wav", 1 );
		break;
	}

	strcat( m_szNumber_guess, szDigit );

	if ( strlen( m_szNumber_guess ) >= 8 )
		PhoneDial_guess();

	m_pNumberLabel_guess->setText( m_szNumber_guess, strlen( m_szNumber_guess ) );
	m_pNumberLabel_guess->setContentAlignment( Label::a_center );
}

void CPhone::Clear_guess()
{
	strncpy( m_szNumber_guess, "", 16 );

	m_pNumberLabel_guess->setText( m_szNumber_guess, strlen( m_szNumber_guess ) );
	m_pNumberLabel_guess->setContentAlignment( Label::a_center );
}

void CPhone::Initialize()
{
	setVisible( false );
	Clear_guess();
}

int CPhone::KeyInput( int down, int keynum, const char *pszCurrentBinding )
{
	if ( pszCurrentBinding && ( !strcmp( pszCurrentBinding, "toggleconsole" ) || !strcmp( pszCurrentBinding, "phonenumber" ) ) )
		return 1;

	return 0;
}

int MsgPhone_guess( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	if ( gViewPort->m_pPhone_guess )
	{
		int iOn = READ_BYTE();

		if ( !gViewPort->m_pPhone_guess->isVisible() && iOn )
		{
			gViewPort->m_pPhone_guess->setVisible( true );
			gViewPort->UpdateCursorState();
		}
	}

	return 1;
}

void CPhone::PhoneDial_guess()
{
	char szCommand[128];

	sprintf( szCommand, "phonenumber %s", m_szNumber_guess );
	gEngfuncs.pfnServerCmd( szCommand );

	Clear_guess();
	strncpy( m_szNumber_guess, "", 16 );

	gEngfuncs.pfnPlaySoundByName( "phone/tone_send.wav", 1 );

	if ( isVisible() )
	{
		setVisible( false );
		gViewPort->UpdateCursorState();
	}
}

void PhoneInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "Phone", MsgPhone_guess );
}

CPhone::~CPhone( void )
{
	delete m_pPhoneImage_guess;
}
