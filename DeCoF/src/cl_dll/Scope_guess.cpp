// ======================================
// Cry of Fear telescope scope panel
// (class name from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "Scope_guess.h"
#include "..\game_shared\vgui_loadtga.h"

CScope::CScope() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int wide, tall;
	int x, y;
	Panel *pPanel;

	m_pScopeImage_guess = vgui_LoadTGA( "gfx/vgui/telescope_scope.tga" );
	if ( m_pScopeImage_guess )
		m_pScopeImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/telescope_scope.tga!\n" );

	m_pScopeImage_guess->getSize( wide, tall );
	x = ScreenWidth / 2 - wide / 2;
	y = ScreenHeight / 2 - tall / 2;

	// black borders around the scope picture
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

	m_pScopePanel_guess = new ImagePanel( m_pScopeImage_guess );
	m_pScopePanel_guess->setParent( this );
	m_pScopePanel_guess->setPos( x, y );
	m_pScopePanel_guess->setVisible( true );

	setBgColor( 0, 0, 0, 255 );

	Initialize();
}

void CScope::Initialize()
{
	m_iScope_guess = 0;
	setVisible( false );
}

int MsgScope_guess( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	if ( gViewPort->m_pScope_guess )
	{
		int iScope = READ_BYTE();
		gViewPort->m_pScope_guess->m_iScope_guess = iScope;

		if ( iScope )
		{
			switch ( iScope )
			{
			case 1:
				gViewPort->m_pScope_guess->m_pScopePanel_guess->setImage( vgui_LoadTGA( "gfx/vgui/telescope_scope.tga" ) );
				break;
			case 2:
				gViewPort->m_pScope_guess->m_pScopePanel_guess->setImage( vgui_LoadTGA( "gfx/vgui/telescope_scope.tga" ) );
				break;
			default:
				gViewPort->m_pScope_guess->m_pScopePanel_guess->setImage( vgui_LoadTGA( "gfx/vgui/telescope_scope.tga" ) );
				break;
			}

			if ( !gViewPort->m_pScope_guess->isVisible() )
				gViewPort->m_pScope_guess->setVisible( true );
		}
		else
		{
			if ( gViewPort->m_pScope_guess->isVisible() )
				gViewPort->m_pScope_guess->setVisible( false );
		}
	}

	return 1;
}

void ScopeInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "Scope", MsgScope_guess );
}

CScope::~CScope( void )
{
	delete m_pScopeImage_guess;
}
