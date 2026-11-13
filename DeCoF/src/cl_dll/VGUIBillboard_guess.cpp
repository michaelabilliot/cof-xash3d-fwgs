// ======================================
// Cry of Fear billboard panel
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "VGUIBillboard_guess.h"
#include "HUDControl_guess.h"
#include "..\game_shared\vgui_loadtga.h"

CVGUIBillboard::CVGUIBillboard() : Panel( 0, 0, ScreenWidth, ScreenHeight )
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

	if ( !vgui_LoadTGA( "gfx/vgui/640_gustavdahlwelcome.tga" ) )
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/640_gustavdahlwelcome.tga!\n" );

	x = ScreenWidth / 2 - 320;
	y = ScreenHeight / 2 - 240;

	m_pImage_guess = new ImagePanel( vgui_LoadTGA( "gfx/vgui/640_gustavdahlwelcome.tga" ) );
	m_pImage_guess->setParent( this );
	m_pImage_guess->setPos( x, y );
	m_pImage_guess->setVisible( true );

	pButton = new CommandButton( "", x, y, 640, 480, true, true );
	pButton->setParent( this );
	pButton->addActionSignal( new CBillboardHandler_Command( this, 1 ) );

	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	wide = 640;
	tall = 480;
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "credits" );
	SchemeHandle_t hChapterScheme = pSchemes->getSchemeHandle( "chaptertitle" );
	SchemeHandle_t hTextScheme = pSchemes->getSchemeHandle( "credits" );
	m_pFont_guess = pSchemes->getFont( hTitleScheme );

	m_pLabel_guess = new Label( " ", 0, 0, ScreenWidth, 64 );
	m_pLabel_guess->setParent( this );
	m_pLabel_guess->setFont( m_pFont_guess );
	m_pLabel_guess->setFgColor( 0, 0, 0, 0 );
	m_pLabel_guess->setBgColor( 0, 0, 0, 255 );
	m_pLabel_guess->setText( " " );
	m_pLabel_guess->setContentAlignment( Label::a_center );

	setBgColor( 0, 0, 0, 255 );

	Initialize();
}

void CVGUIBillboard::ActionSignal( int cmd )
{
	switch ( cmd )
	{
	case 0:
		break;
	case 1:
		setVisible( false );
		gViewPort->UpdateCursorState();
		break;
	default:
		gEngfuncs.Con_DPrintf( "Unknown Billboard ActionSignal %i.\n", cmd );
		break;
	}
}

void CVGUIBillboard::Initialize()
{
	m_pLabel_guess->setText( "" );
	setVisible( false );
}

int CVGUIBillboard::KeyInput( int down, int keynum, const char *pszCurrentBinding )
{
	if ( pszCurrentBinding && !strcmp( pszCurrentBinding, "toggleconsole" ) )
		return 1;

	return 0;
}

int MsgBoard_guess( const char *pszName, int iSize, void *pbuf )
{
	char szImage[128];

	BEGIN_READ( pbuf, iSize );

	if ( gViewPort->m_pVGUIBillboard_guess )
	{
		sprintf( szImage, "%s", READ_STRING() );

		if ( strlen( szImage ) > 1 )
		{
			if ( vgui_LoadTGA( szImage ) )
				gViewPort->m_pVGUIBillboard_guess->m_pImage_guess->setImage( vgui_LoadTGA( szImage ) );
			else
				gEngfuncs.Con_Printf( "Cannot load %s!\n", szImage );
		}
		else
		{
			gEngfuncs.Con_Printf( "No message sent\n" );
		}

		if ( !gViewPort->m_pVGUIBillboard_guess->isVisible() )
		{
			gViewPort->m_pVGUIBillboard_guess->setVisible( true );
			gViewPort->UpdateCursorState();
		}
	}

	return 1;
}

void VGUI_BoardInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "Board", MsgBoard_guess );
}

void CVGUIBillboard::paint()
{
	int x, y;
	int wide, tall;
	int r, g, b, a;

	if ( gViewPort && gViewPort->m_pHUDControl_guess && gViewPort->m_pHUDControl_guess->m_szMessage_guess[0] )
	{
		if ( strlen( gViewPort->m_pHUDControl_guess->m_szMessage_guess ) > 1 )
		{
			gViewPort->m_pHUDControl_guess->m_pMessageLabel_guess->getPos( x, y );
			gViewPort->m_pHUDControl_guess->m_pMessageLabel_guess->getSize( wide, tall );
			gViewPort->m_pHUDControl_guess->m_pMessageLabel_guess->getFgColor( r, g, b, a );

			m_pLabel_guess->setSize( wide, tall );
			m_pLabel_guess->setText( gViewPort->m_pHUDControl_guess->m_szMessage_guess );
			m_pLabel_guess->setPos( x, y );
			m_pLabel_guess->setContentAlignment( Label::a_center );
			m_pLabel_guess->setFgColor( r, g, b, a );
		}
	}

	Panel::paint();
}

void CVGUIBillboard::paintBackground()
{
	Panel::paintBackground();
}

void CVGUIBillboard::setVisible( bool state )
{
	Panel::setVisible( state );
}

CVGUIBillboard::~CVGUIBillboard( void )
{
	delete m_pBackgroundImage_guess;
}
