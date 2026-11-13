// ======================================
// Cry of Fear boss health bar
// (class name from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "BossBar_guess.h"
#include "..\game_shared\vgui_loadtga.h"

CBossBar::CBossBar() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int wide, tall;
	int x;

	m_pFullImage_guess = vgui_LoadTGA( "gfx/vgui/boss/doctor_health_full.tga" );
	if ( m_pFullImage_guess )
		m_pFullImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/boss/doctor_health_full.tga!\n" );

	m_pEmptyImage_guess = vgui_LoadTGA( "gfx/vgui/boss/doctor_health_empty.tga" );
	if ( m_pEmptyImage_guess )
		m_pEmptyImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/boss/doctor_health_empty.tga!\n" );

	m_pFull_guess = new ImagePanel( m_pFullImage_guess );
	m_pFull_guess->setParent( this );
	wide = m_pFull_guess->getWide();
	tall = m_pFull_guess->getTall();
	x = ScreenWidth / 2 - wide / 2;
	m_pFull_guess->setPos( x, tall );
	m_pFull_guess->setVisible( true );

	m_pEmpty_guess = new ImagePanel( m_pEmptyImage_guess );
	m_pEmpty_guess->setParent( this );
	m_pEmpty_guess->setPos( x, tall );
	m_pEmpty_guess->setVisible( true );

	m_pBar_guess = new ImagePanel( m_pFullImage_guess );
	m_pBar_guess->setParent( this );
	m_pBar_guess->setPos( x, tall );
	m_pBar_guess->setVisible( true );

	setBgColor( 0, 0, 0, 255 );
	m_iUnused1C8_guess = 0;

	Initialize();
}

CBossBar::~CBossBar()
{
	delete m_pFullImage_guess;
	delete m_pEmptyImage_guess;
}

void CBossBar::Initialize()
{
	m_iUnused1C8_guess = 0;
	setVisible( false );
}

int MsgBossBar_guess( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	if ( gViewPort->m_pBossBar_guess )
	{
		gViewPort->m_pBossBar_guess->m_iUnused1C8_guess = 0;
		gViewPort->m_pBossBar_guess->m_iState_guess = READ_BYTE();
		sprintf( gViewPort->m_pBossBar_guess->m_szEmptyImage_guess, "%s", READ_STRING() );
		sprintf( gViewPort->m_pBossBar_guess->m_szFullImage_guess, "%s", READ_STRING() );

		gViewPort->m_pBossBar_guess->m_pFull_guess->setImage( vgui_LoadTGA( gViewPort->m_pBossBar_guess->m_szFullImage_guess ) );
		gViewPort->m_pBossBar_guess->m_pBar_guess->setImage( vgui_LoadTGA( gViewPort->m_pBossBar_guess->m_szFullImage_guess ) );
		gViewPort->m_pBossBar_guess->m_pEmpty_guess->setImage( vgui_LoadTGA( gViewPort->m_pBossBar_guess->m_szEmptyImage_guess ) );

		int iHealth = READ_SHORT();
		if ( iHealth < 0 )
			iHealth = 0;

		if ( gViewPort->m_pBossBar_guess->m_iState_guess == 1 )
			gViewPort->m_pBossBar_guess->m_iHealth_guess = iHealth;
		else
			gViewPort->m_pBossBar_guess->m_iHealth_guess = 0;

		gViewPort->m_pBossBar_guess->m_iMaxHealth_guess = READ_SHORT();

		if ( gViewPort->m_pBossBar_guess->m_iState_guess == 1 )
			gViewPort->m_pBossBar_guess->setVisible( true );
		else if ( gViewPort->m_pBossBar_guess->m_iState_guess == 0 )
			gViewPort->m_pBossBar_guess->setVisible( false );
	}

	return 1;
}

void VGUI_BossBarInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "BossBar", MsgBossBar_guess );
}

void CBossBar::solve()
{
	int tall = m_pEmpty_guess->getTall();
	int wide = m_pEmpty_guess->getWide();

	if ( m_iState_guess == 2 )
		setVisible( false );

	int width = (int)( (float)m_iHealth_guess / (float)m_iMaxHealth_guess * (float)wide );
	if ( width < 1 )
		width = 0;

	m_pBar_guess->setSize( width, tall );

	Panel::solve();
}
