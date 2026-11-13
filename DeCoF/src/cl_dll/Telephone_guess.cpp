// ======================================
// Cry of Fear telephone puzzle panel
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "Telephone_guess.h"
#include "..\game_shared\vgui_loadtga.h"
#include "event_api.h"

CTelephone::CTelephone() : Panel( 0, 0, ScreenWidth, ScreenHeight )
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

	m_pTelephoneImage_guess = vgui_LoadTGA( "gfx/vgui/puzzle/telephone_vgui.tga" );
	if ( m_pTelephoneImage_guess )
		m_pTelephoneImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/puzzle/telephone_vgui.tga!\n" );

	m_pTelephoneImage_guess->getSize( wide, tall );
	x = ScreenWidth / 2 - wide / 2;
	y = ScreenHeight / 2 - tall / 2;

	m_pTelephone_guess = new ImagePanel( m_pTelephoneImage_guess );
	m_pTelephone_guess->setParent( this );
	m_pTelephone_guess->setPos( x, y );
	m_pTelephone_guess->setVisible( true );

	pButton = new CommandButton( "", 0, 0, 20, 20, true, true );
	pButton->setParent( m_pTelephone_guess );
	pButton->addActionSignal( new CTelephoneHandler_Command( this, 12 ) );

	pButton = new CommandButton( "", 140, 144, 30, 30, true, true );
	pButton->setParent( m_pTelephone_guess );
	pButton->addActionSignal( new CTelephoneHandler_Command( this, 1 ) );

	pButton = new CommandButton( "", 175, 144, 30, 30, true, true );
	pButton->setParent( m_pTelephone_guess );
	pButton->addActionSignal( new CTelephoneHandler_Command( this, 2 ) );

	pButton = new CommandButton( "", 211, 144, 30, 30, true, true );
	pButton->setParent( m_pTelephone_guess );
	pButton->addActionSignal( new CTelephoneHandler_Command( this, 3 ) );

	pButton = new CommandButton( "", 139, 178, 30, 30, true, true );
	pButton->setParent( m_pTelephone_guess );
	pButton->addActionSignal( new CTelephoneHandler_Command( this, 4 ) );

	pButton = new CommandButton( "", 174, 178, 30, 30, true, true );
	pButton->setParent( m_pTelephone_guess );
	pButton->addActionSignal( new CTelephoneHandler_Command( this, 5 ) );

	pButton = new CommandButton( "", 210, 178, 30, 30, true, true );
	pButton->setParent( m_pTelephone_guess );
	pButton->addActionSignal( new CTelephoneHandler_Command( this, 6 ) );

	pButton = new CommandButton( "", 137, 213, 30, 30, true, true );
	pButton->setParent( m_pTelephone_guess );
	pButton->addActionSignal( new CTelephoneHandler_Command( this, 7 ) );

	pButton = new CommandButton( "", 173, 213, 30, 30, true, true );
	pButton->setParent( m_pTelephone_guess );
	pButton->addActionSignal( new CTelephoneHandler_Command( this, 8 ) );

	pButton = new CommandButton( "", 210, 213, 30, 30, true, true );
	pButton->setParent( m_pTelephone_guess );
	pButton->addActionSignal( new CTelephoneHandler_Command( this, 9 ) );

	pButton = new CommandButton( "", 173, 249, 30, 30, true, true );
	pButton->setParent( m_pTelephone_guess );
	pButton->addActionSignal( new CTelephoneHandler_Command( this, 10 ) );

	pButton = new CommandButton( "", 251, 288, 57, 27, true, true );
	pButton->setParent( m_pTelephone_guess );
	pButton->addActionSignal( new CTelephoneHandler_Command( this, 11 ) );

	setBgColor( 0, 0, 0, 0 );

	Initialize();
}

void CTelephone::ActionSignal( int cmd )
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
		Dial_guess();
		break;
	case 12:
		if ( !m_bLocked_guess )
		{
			setVisible( false );
			gViewPort->UpdateCursorState();
			gEngfuncs.pfnPlaySoundByName( "phone/telephone_receiver_down.wav", 1 );
		}
		break;
	}
}

void CTelephone::AddDigit_guess( int digit )
{
	char szDigit[4];

	if ( m_bLocked_guess )
		return;

	if ( strlen( m_szDialed_guess ) < 15 )
	{
		itoa( digit, szDigit, 10 );
		strcat( m_szDialed_guess, szDigit );
		gEngfuncs.pfnPlaySoundByName( "phone/telephone_button_press.wav", 1 );
	}
}

void CTelephone::Dial_guess()
{
	cl_entity_t *pPlayer;

	if ( strlen( m_szDialed_guess ) >= 1 && !m_bLocked_guess )
	{
		pPlayer = gEngfuncs.GetLocalPlayer();

		gEngfuncs.pfnPlaySoundByName( "phone/telephone_button_press.wav", 1 );

		if ( !strcmp( m_szNumber_guess, m_szDialed_guess ) )
			gEngfuncs.pfnServerCmd( "telephoneright" );
		else
			gEngfuncs.pfnServerCmd( "telephonewrong" );

		if ( pPlayer )
			gEngfuncs.pEventAPI->EV_StopSound( -1, CHAN_ITEM, "phone/telephone_dial_tone.wav" );

		strncpy( m_szDialed_guess, "", 16 );
	}
}

void CTelephone::Initialize()
{
	strcpy( m_szNumber_guess, "19357185" );
	strncpy( m_szDialed_guess, "", 16 );
	setVisible( false );
}

int MsgTelephone_guess( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int iOn = READ_BYTE();

	if ( gViewPort->m_pTelephone_guess )
	{
		gViewPort->m_pTelephone_guess->m_bLocked_guess = false;

		if ( iOn > 0 )
		{
			if ( iOn == 1 )
			{
				if ( !gViewPort->m_pTelephone_guess->isVisible() )
				{
					gViewPort->m_pTelephone_guess->setVisible( true );
					gEngfuncs.pfnPlaySoundByName( "phone/telephone_receiver_up.wav", 1 );
					gViewPort->m_pTelephone_guess->m_flFade_guess = 255;
				}

				gViewPort->UpdateCursorState();
				strcpy( gViewPort->m_pTelephone_guess->m_szNumber_guess, READ_STRING() );
				return 1;
			}
			else if ( iOn == 2 )
			{
				cl_entity_t *pPlayer = gEngfuncs.GetLocalPlayer();

				if ( pPlayer )
				{
					gEngfuncs.pEventAPI->EV_StopSound( -1, CHAN_ITEM, "phone/telephone_dial_tone.wav" );
					gEngfuncs.pEventAPI->EV_PlaySound( -1, pPlayer->curstate.origin, CHAN_ITEM, "phone/telephone_dial_tone.wav", 1.0, 0, 0, 100 );
				}
				return 1;
			}
			else if ( iOn == 3 )
			{
				gViewPort->m_pTelephone_guess->m_bLocked_guess = true;
			}
		}
		else
		{
			if ( gViewPort->m_pTelephone_guess->isVisible() == true )
			{
				gViewPort->m_pTelephone_guess->setVisible( false );
				gEngfuncs.pfnPlaySoundByName( "phone/telephone_receiver_down.wav", 1 );
			}

			gViewPort->UpdateCursorState();
			strcpy( gViewPort->m_pTelephone_guess->m_szNumber_guess, READ_STRING() );
			return 1;
		}
	}

	return 1;
}

void TelephoneInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "Telephone", MsgTelephone_guess );
}

void CTelephone::paint()
{
	if ( m_flFade_guess > 0 )
		m_flFade_guess -= gHUD.m_flTimeDelta * 700;

	if ( m_flFade_guess < 0 )
		m_flFade_guess = 0;

	m_pTelephoneImage_guess->setColor( Color( 255, 255, 255, m_flFade_guess ) );

	int alpha = m_flFade_guess;
	if ( alpha < 128 )
		alpha = 128;

	setBgColor( Color( 0, 0, 0, alpha ) );
}

void CTelephone::setVisible( bool state )
{
	cl_entity_t *pPlayer = gEngfuncs.GetLocalPlayer();

	strncpy( m_szDialed_guess, "", 16 );

	if ( state )
	{
		if ( pPlayer )
			gEngfuncs.pEventAPI->EV_PlaySound( -1, pPlayer->curstate.origin, CHAN_ITEM, "phone/telephone_dial_tone.wav", 1.0, 0, 0, 100 );
		gEngfuncs.pfnServerCmd( "freeze 1024" );
	}
	else
	{
		if ( pPlayer )
			gEngfuncs.pEventAPI->EV_StopSound( -1, CHAN_ITEM, "phone/telephone_dial_tone.wav" );
		gEngfuncs.pfnServerCmd( "unfreeze 1024" );
	}

	Panel::setVisible( state );
}

CTelephone::~CTelephone( void )
{
	delete m_pTelephoneImage_guess;
}
