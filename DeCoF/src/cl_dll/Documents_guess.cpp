// ======================================
// Cry of Fear documents panel
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "event_api.h"
#include "vgui_TeamFortressViewport.h"
#include "Documents_guess.h"
#include "..\game_shared\vgui_loadtga.h"

CDocuments::CDocuments() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int wide, tall;
	int x, y;
	int i;
	Panel *pPanel;
	CommandButton *pButton;

	pPanel = new Panel( 0, 0, ScreenWidth, ScreenHeight );
	pPanel->setParent( this );
	pPanel->setSize( ScreenWidth, ScreenHeight );
	pPanel->setPos( 0, 0 );
	pPanel->setBgColor( 0, 0, 0, 0 );
	pPanel->setVisible( true );

	m_pDocumentsImage_guess = vgui_LoadTGA( "gfx/vgui/640_documents.tga" );
	if ( m_pDocumentsImage_guess )
		m_pDocumentsImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/640_documents.tga!\n" );

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

	m_pDocumentsImage_guess->getSize( wide, tall );
	x = ScreenWidth / 2 - wide / 2;
	y = ScreenHeight / 2 - tall / 2;

	m_pDocuments_guess = new ImagePanel( m_pDocumentsImage_guess );
	m_pDocuments_guess->setParent( this );
	m_pDocuments_guess->setPos( x, y );
	m_pDocuments_guess->setVisible( true );

	y = 76;

	for ( i = 1; i <= MAX_DOCUMENTS_guess; i++ )
	{
		m_pHiddenNoteImage_guess[i] = vgui_LoadTGA( "gfx/vgui/notes/hiddennote1.tga" );
		if ( m_pHiddenNoteImage_guess[i] )
			m_pHiddenNoteImage_guess[i]->setColor( Color( 255, 255, 255, 1 ) );
		else
			gEngfuncs.Con_Printf( "Cannot load gfx/vgui/notes/hiddennote1.tga!\n" );

		m_pNote_guess[i] = new ImagePanel( m_pHiddenNoteImage_guess[i] );
		m_pNote_guess[i]->setParent( m_pDocuments_guess );
		m_pNote_guess[i]->setPos( 230, y );
		m_pNote_guess[i]->setVisible( false );

		m_pCodeDigit1_guess[i] = new ImagePanel( NULL );
		m_pCodeDigit1_guess[i]->setParent( m_pDocuments_guess );
		m_pCodeDigit1_guess[i]->setPos( 230, y );
		m_pCodeDigit1_guess[i]->setVisible( false );

		m_pCodeDigit2_guess[i] = new ImagePanel( NULL );
		m_pCodeDigit2_guess[i]->setParent( m_pDocuments_guess );
		m_pCodeDigit2_guess[i]->setPos( 230, y );
		m_pCodeDigit2_guess[i]->setVisible( false );

		m_pCodeDigit3_guess[i] = new ImagePanel( NULL );
		m_pCodeDigit3_guess[i]->setParent( m_pDocuments_guess );
		m_pCodeDigit3_guess[i]->setPos( 230, y );
		m_pCodeDigit3_guess[i]->setVisible( false );

		m_pCodeDigit4_guess[i] = new ImagePanel( NULL );
		m_pCodeDigit4_guess[i]->setParent( m_pDocuments_guess );
		m_pCodeDigit4_guess[i]->setPos( 230, y );
		m_pCodeDigit4_guess[i]->setVisible( false );

		m_pButton_guess[i] = new CommandButton( "", 230, y, 422, 30, true, true );
		m_pButton_guess[i]->setParent( m_pDocuments_guess );
		m_pButton_guess[i]->addActionSignal( new CDocumentsHandler_Command( this, i ) );

		y += 30;
	}

	pButton = new CommandButton( "", 0, 0, 20, 20, true, true );
	pButton->setParent( m_pDocuments_guess );
	pButton->addActionSignal( new CDocumentsHandler_Command( this, 20 ) );

	pButton = new CommandButton( "", 0, 34, 33, 33, true, true );
	pButton->setParent( m_pDocuments_guess );
	pButton->addActionSignal( new CDocumentsHandler_Command( this, 25 ) );

	setBgColor( 0, 0, 0, 255 );

	Initialize();
}

CDocuments::~CDocuments()
{
	delete m_pDocumentsImage_guess;
	delete m_pBackgroundImage_guess;

	for ( int i = 1; i <= MAX_DOCUMENTS_guess; i++ )
		delete m_pHiddenNoteImage_guess[i];
}

void CDocuments::ActionSignal( int cmd )
{
	char szCommand[64];

	if ( cmd != 20 )
	{
		if ( cmd == 25 )
		{
			setVisible( false );
			gViewPort->UpdateCursorState();
			gViewPort->ShowVGUIMenu( 30 );
		}
		else if ( cmd >= 1 && cmd <= MAX_DOCUMENTS_guess )
		{
			if ( strlen( gHUD.m_Notes_guess[cmd].szLine_guess[4] ) > 1 && !m_iHidden_guess[cmd] )
			{
				setVisible( false );
				gViewPort->UpdateCursorState();
				sprintf( szCommand, "recalldocument %i", cmd );
				gEngfuncs.pfnServerCmd( szCommand );
			}
			else
			{
				gEngfuncs.pfnPlaySoundByName( "inventory/inventory_no.wav", 1 );
			}
		}
	}
	else
	{
		setVisible( false );
		gViewPort->UpdateCursorState();
	}
}

void CDocuments::Initialize()
{
	setVisible( false );
}

int CDocuments::KeyInput( int down, int keynum, const char *pszCurrentBinding )
{
	if ( pszCurrentBinding && !strcmp( pszCurrentBinding, "toggleconsole" ) )
		return 1;

	return 0;
}

void CDocuments::setVisible( bool state )
{
	if ( state )
	{
		for ( int i = 1; i <= MAX_DOCUMENTS_guess; i++ )
		{
			if ( strlen( gHUD.m_Notes_guess[i].szLine_guess[4] ) > 1 )
				m_pNote_guess[i]->setVisible( true );
			else
				m_pNote_guess[i]->setVisible( false );
		}

		cl_entity_t *pPlayer = gEngfuncs.GetLocalPlayer();
		if ( pPlayer )
			gEngfuncs.pEventAPI->EV_PlaySound( 0, pPlayer->origin, CHAN_AUTO, "notes/note_turn1.wav", 1.0, 0.8, 0, 80 );
	}

	Panel::setVisible( state );
}
