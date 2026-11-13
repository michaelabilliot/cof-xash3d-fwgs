// ======================================
// Cry of Fear cutscene skip panel
// (class name from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "vgui_TeamFortressViewport.h"
#include "CutsceneSkip_guess.h"
#include "..\game_shared\vgui_loadtga.h"

CCutsceneSkip::CCutsceneSkip() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int wide, tall;
	int x, y;
	Panel *pPanel;

	m_pDotsImage_guess = vgui_LoadTGA( "gfx/vgui/loading_dots.tga" );
	if ( m_pDotsImage_guess )
		m_pDotsImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/loading_dots.tga!\n" );

	m_pDotsImage_guess->getSize( wide, tall );
	x = ScreenWidth / 2 - wide / 2;
	y = ScreenHeight / 2 - tall / 2;

	// black borders around the picture
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

	m_pDotsPanel_guess = new ImagePanel( m_pDotsImage_guess );
	m_pDotsPanel_guess->setParent( this );
	m_pDotsPanel_guess->setPos( x, y );
	m_pDotsPanel_guess->setVisible( true );

	setBgColor( 0, 0, 0, 255 );

	Initialize();
}

void CCutsceneSkip::Initialize()
{
	setVisible( false );
}

CCutsceneSkip::~CCutsceneSkip( void )
{
	delete m_pDotsImage_guess;
}
