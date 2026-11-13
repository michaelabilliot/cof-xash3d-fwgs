// ======================================
// Cry of Fear unlockables menu
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "vgui_TeamFortressViewport.h"
#include "Unlockables_guess.h"
#include "GameMenu_guess.h"
#include "BookMenu_guess.h"
#include "..\game_shared\vgui_loadtga.h"

extern int iMenuMode_guess;

CUnlockables::CUnlockables() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int wide, tall;
	int x, y;
	int i;

	m_pMenuImage_guess = vgui_LoadTGA( "gfx/vgui/unlockables/unlockables_menu.tga" );
	if ( m_pMenuImage_guess )
		m_pMenuImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/unlockables/unlockables_menu.tga!\n" );

	m_pMenuImage_guess->getSize( wide, tall );
	x = ScreenWidth / 2 - wide / 2;
	y = ScreenHeight / 2 - tall / 2;

	m_pMenu_guess = new ImagePanel( m_pMenuImage_guess );
	m_pMenu_guess->setParent( this );
	m_pMenu_guess->setPos( x, y );
	m_pMenu_guess->setVisible( true );

	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hCreditsScheme = pSchemes->getSchemeHandle( "credits" );
	Font *pFont = pSchemes->getFont( hCreditsScheme );

	m_pTextLabel_guess = new Label( "", 126, 428, 549, 47 );
	m_pTextLabel_guess->setParent( m_pMenu_guess );
	m_pTextLabel_guess->setBgColor( 0, 0, 0, 255 );
	m_pTextLabel_guess->setFgColor( 255, 255, 255, 0 );
	m_pTextLabel_guess->setFont( pFont );
	m_pTextLabel_guess->setContentAlignment( Label::a_center );

	m_pBackButton_guess = new ColorButton( "", 304, 503, 192, 54, false, false );
	m_pBackButton_guess->setParent( m_pMenu_guess );
	m_pBackButton_guess->addActionSignal( new CUnlockablesHandler_Command( this, 50 ) );

	for ( i = 0; i < 21; i++ )
	{
		m_pLockedImage_guess[i] = vgui_LoadTGA( "gfx/vgui/unlockables/unlockables_locked.tga" );
		if ( m_pLockedImage_guess[i] )
			m_pLockedImage_guess[i]->setColor( Color( 255, 255, 255, 1 ) );
		else
			gEngfuncs.Con_Printf( "Cannot load gfx/vgui/unlockables/unlockables_locked.tga!\n" );
	}

	m_pLockedPanel_guess[0] = new ImagePanel( m_pLockedImage_guess[0] );
	m_pLockedPanel_guess[0]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[0]->setPos( 85, 139 );
	m_pLockedPanel_guess[0]->setVisible( true );

	m_pItemButton_guess[0] = new ColorButton( "", 85, 139, 90, 90, false, false );
	m_pItemButton_guess[0]->setParent( m_pMenu_guess );
	m_pItemButton_guess[0]->addActionSignal( new CUnlockablesHandler_Command( this, 1 ) );

	m_pLockedPanel_guess[1] = new ImagePanel( m_pLockedImage_guess[1] );
	m_pLockedPanel_guess[1]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[1]->setPos( 175, 139 );
	m_pLockedPanel_guess[1]->setVisible( true );

	m_pItemButton_guess[1] = new ColorButton( "", 175, 139, 90, 90, false, false );
	m_pItemButton_guess[1]->setParent( m_pMenu_guess );
	m_pItemButton_guess[1]->addActionSignal( new CUnlockablesHandler_Command( this, 2 ) );

	m_pLockedPanel_guess[2] = new ImagePanel( m_pLockedImage_guess[2] );
	m_pLockedPanel_guess[2]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[2]->setPos( 265, 139 );
	m_pLockedPanel_guess[2]->setVisible( true );

	m_pItemButton_guess[2] = new ColorButton( "", 265, 139, 90, 90, false, false );
	m_pItemButton_guess[2]->setParent( m_pMenu_guess );
	m_pItemButton_guess[2]->addActionSignal( new CUnlockablesHandler_Command( this, 3 ) );

	m_pLockedPanel_guess[3] = new ImagePanel( m_pLockedImage_guess[3] );
	m_pLockedPanel_guess[3]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[3]->setPos( 355, 139 );
	m_pLockedPanel_guess[3]->setVisible( true );

	m_pItemButton_guess[3] = new ColorButton( "", 355, 139, 90, 90, false, false );
	m_pItemButton_guess[3]->setParent( m_pMenu_guess );
	m_pItemButton_guess[3]->addActionSignal( new CUnlockablesHandler_Command( this, 4 ) );

	m_pLockedPanel_guess[4] = new ImagePanel( m_pLockedImage_guess[4] );
	m_pLockedPanel_guess[4]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[4]->setPos( 445, 139 );
	m_pLockedPanel_guess[4]->setVisible( true );

	m_pItemButton_guess[4] = new ColorButton( "", 445, 139, 90, 90, false, false );
	m_pItemButton_guess[4]->setParent( m_pMenu_guess );
	m_pItemButton_guess[4]->addActionSignal( new CUnlockablesHandler_Command( this, 5 ) );

	m_pLockedPanel_guess[5] = new ImagePanel( m_pLockedImage_guess[5] );
	m_pLockedPanel_guess[5]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[5]->setPos( 535, 139 );
	m_pLockedPanel_guess[5]->setVisible( true );

	m_pItemButton_guess[5] = new ColorButton( "", 535, 139, 90, 90, false, false );
	m_pItemButton_guess[5]->setParent( m_pMenu_guess );
	m_pItemButton_guess[5]->addActionSignal( new CUnlockablesHandler_Command( this, 6 ) );

	m_pLockedPanel_guess[6] = new ImagePanel( m_pLockedImage_guess[6] );
	m_pLockedPanel_guess[6]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[6]->setPos( 625, 139 );
	m_pLockedPanel_guess[6]->setVisible( true );

	m_pItemButton_guess[6] = new ColorButton( "", 625, 139, 90, 90, false, false );
	m_pItemButton_guess[6]->setParent( m_pMenu_guess );
	m_pItemButton_guess[6]->addActionSignal( new CUnlockablesHandler_Command( this, 7 ) );

	m_pLockedPanel_guess[7] = new ImagePanel( m_pLockedImage_guess[7] );
	m_pLockedPanel_guess[7]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[7]->setPos( 85, 229 );
	m_pLockedPanel_guess[7]->setVisible( true );

	m_pItemButton_guess[7] = new ColorButton( "", 85, 229, 90, 90, false, false );
	m_pItemButton_guess[7]->setParent( m_pMenu_guess );
	m_pItemButton_guess[7]->addActionSignal( new CUnlockablesHandler_Command( this, 8 ) );

	m_pLockedPanel_guess[8] = new ImagePanel( m_pLockedImage_guess[8] );
	m_pLockedPanel_guess[8]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[8]->setPos( 175, 229 );
	m_pLockedPanel_guess[8]->setVisible( true );

	m_pItemButton_guess[8] = new ColorButton( "", 175, 229, 90, 90, false, false );
	m_pItemButton_guess[8]->setParent( m_pMenu_guess );
	m_pItemButton_guess[8]->addActionSignal( new CUnlockablesHandler_Command( this, 9 ) );

	m_pLockedPanel_guess[9] = new ImagePanel( m_pLockedImage_guess[9] );
	m_pLockedPanel_guess[9]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[9]->setPos( 265, 229 );
	m_pLockedPanel_guess[9]->setVisible( true );

	m_pItemButton_guess[9] = new ColorButton( "", 265, 229, 90, 90, false, false );
	m_pItemButton_guess[9]->setParent( m_pMenu_guess );
	m_pItemButton_guess[9]->addActionSignal( new CUnlockablesHandler_Command( this, 10 ) );

	m_pLockedPanel_guess[10] = new ImagePanel( m_pLockedImage_guess[10] );
	m_pLockedPanel_guess[10]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[10]->setPos( 355, 229 );
	m_pLockedPanel_guess[10]->setVisible( true );

	m_pItemButton_guess[10] = new ColorButton( "", 355, 229, 90, 90, false, false );
	m_pItemButton_guess[10]->setParent( m_pMenu_guess );
	m_pItemButton_guess[10]->addActionSignal( new CUnlockablesHandler_Command( this, 11 ) );

	m_pLockedPanel_guess[11] = new ImagePanel( m_pLockedImage_guess[11] );
	m_pLockedPanel_guess[11]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[11]->setPos( 445, 229 );
	m_pLockedPanel_guess[11]->setVisible( true );

	m_pItemButton_guess[11] = new ColorButton( "", 445, 229, 90, 90, false, false );
	m_pItemButton_guess[11]->setParent( m_pMenu_guess );
	m_pItemButton_guess[11]->addActionSignal( new CUnlockablesHandler_Command( this, 12 ) );

	m_pLockedPanel_guess[12] = new ImagePanel( m_pLockedImage_guess[12] );
	m_pLockedPanel_guess[12]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[12]->setPos( 535, 229 );
	m_pLockedPanel_guess[12]->setVisible( true );

	m_pItemButton_guess[12] = new ColorButton( "", 535, 229, 90, 90, false, false );
	m_pItemButton_guess[12]->setParent( m_pMenu_guess );
	m_pItemButton_guess[12]->addActionSignal( new CUnlockablesHandler_Command( this, 13 ) );

	m_pLockedPanel_guess[13] = new ImagePanel( m_pLockedImage_guess[13] );
	m_pLockedPanel_guess[13]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[13]->setPos( 625, 229 );
	m_pLockedPanel_guess[13]->setVisible( true );

	m_pItemButton_guess[13] = new ColorButton( "", 625, 229, 90, 90, false, false );
	m_pItemButton_guess[13]->setParent( m_pMenu_guess );
	m_pItemButton_guess[13]->addActionSignal( new CUnlockablesHandler_Command( this, 14 ) );

	m_pLockedPanel_guess[14] = new ImagePanel( m_pLockedImage_guess[14] );
	m_pLockedPanel_guess[14]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[14]->setPos( 85, 319 );
	m_pLockedPanel_guess[14]->setVisible( true );

	m_pItemButton_guess[14] = new ColorButton( "", 85, 319, 90, 90, false, false );
	m_pItemButton_guess[14]->setParent( m_pMenu_guess );
	m_pItemButton_guess[14]->addActionSignal( new CUnlockablesHandler_Command( this, 15 ) );

	m_pLockedPanel_guess[15] = new ImagePanel( m_pLockedImage_guess[15] );
	m_pLockedPanel_guess[15]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[15]->setPos( 175, 319 );
	m_pLockedPanel_guess[15]->setVisible( true );

	m_pItemButton_guess[15] = new ColorButton( "", 175, 319, 90, 90, false, false );
	m_pItemButton_guess[15]->setParent( m_pMenu_guess );
	m_pItemButton_guess[15]->addActionSignal( new CUnlockablesHandler_Command( this, 16 ) );

	m_pLockedPanel_guess[16] = new ImagePanel( m_pLockedImage_guess[16] );
	m_pLockedPanel_guess[16]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[16]->setPos( 265, 319 );
	m_pLockedPanel_guess[16]->setVisible( true );

	m_pItemButton_guess[16] = new ColorButton( "", 265, 319, 90, 90, false, false );
	m_pItemButton_guess[16]->setParent( m_pMenu_guess );
	m_pItemButton_guess[16]->addActionSignal( new CUnlockablesHandler_Command( this, 17 ) );

	m_pLockedPanel_guess[17] = new ImagePanel( m_pLockedImage_guess[17] );
	m_pLockedPanel_guess[17]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[17]->setPos( 355, 319 );
	m_pLockedPanel_guess[17]->setVisible( true );

	m_pItemButton_guess[17] = new ColorButton( "", 355, 319, 90, 90, false, false );
	m_pItemButton_guess[17]->setParent( m_pMenu_guess );
	m_pItemButton_guess[17]->addActionSignal( new CUnlockablesHandler_Command( this, 18 ) );

	m_pLockedPanel_guess[18] = new ImagePanel( m_pLockedImage_guess[18] );
	m_pLockedPanel_guess[18]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[18]->setPos( 445, 319 );
	m_pLockedPanel_guess[18]->setVisible( true );

	m_pItemButton_guess[18] = new ColorButton( "", 445, 319, 90, 90, false, false );
	m_pItemButton_guess[18]->setParent( m_pMenu_guess );
	m_pItemButton_guess[18]->addActionSignal( new CUnlockablesHandler_Command( this, 19 ) );

	m_pLockedPanel_guess[19] = new ImagePanel( m_pLockedImage_guess[19] );
	m_pLockedPanel_guess[19]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[19]->setPos( 535, 319 );
	m_pLockedPanel_guess[19]->setVisible( true );

	m_pItemButton_guess[19] = new ColorButton( "", 535, 319, 90, 90, false, false );
	m_pItemButton_guess[19]->setParent( m_pMenu_guess );
	m_pItemButton_guess[19]->addActionSignal( new CUnlockablesHandler_Command( this, 20 ) );

	m_pLockedPanel_guess[20] = new ImagePanel( m_pLockedImage_guess[20] );
	m_pLockedPanel_guess[20]->setParent( m_pMenu_guess );
	m_pLockedPanel_guess[20]->setPos( 625, 319 );
	m_pLockedPanel_guess[20]->setVisible( true );

	m_pItemButton_guess[20] = new ColorButton( "", 625, 319, 90, 90, false, false );
	m_pItemButton_guess[20]->setParent( m_pMenu_guess );
	m_pItemButton_guess[20]->addActionSignal( new CUnlockablesHandler_Command( this, 21 ) );

	for ( i = 0; i < 21; i++ )
	{
		m_pItemButton_guess[i]->setUnArmedBorderColor( 0, 0, 0, 255 );
		m_pItemButton_guess[i]->setArmedBorderColor( 255, 0, 0, 192 );
		m_pItemButton_guess[i]->setUnArmedColor( 0, 0, 0, 255 );
		m_pItemButton_guess[i]->setArmedColor( 0, 0, 0, 255 );
	}

	setBgColor( 0, 0, 0, 0 );

	Initialize();
}

CUnlockables::~CUnlockables()
{
	for ( int i = 0; i < 21; i++ )
		delete m_pLockedImage_guess[i];

	delete m_pMenuImage_guess;
}

void CUnlockables::ActionSignal( int cmd )
{
	if ( cmd != 50 && gEngfuncs.GetClientTime() < m_flNextClick_guess )
		return;

	switch ( cmd )
	{
	case 1:
		if ( gHUD.m_iUnlocks_guess & 1 || gHUD.m_iUnlocks_guess & 2 || gHUD.m_iUnlocks_guess & 4 || gHUD.m_iUnlocks_guess & 8 || gHUD.m_iUnlocks_guess & 16 )
		{
			if ( gViewPort && gViewPort->m_pBookMenu_guess )
			{
				setVisible( false );
				gViewPort->UpdateCursorState();
				gViewPort->m_pBookMenu_guess->setVisible( true );
				gViewPort->UpdateCursorState();
			}
		}
		break;
	case 4:
		if ( gHUD.m_iUnlocks_guess & 0x80000000 )
		{
			setVisible( false );
			gViewPort->UpdateCursorState();
			gEngfuncs.pfnClientCmd( "map c_doc_city" );
		}
		break;
	case 50:
		if ( iMenuMode_guess )
		{
			if ( gViewPort && gViewPort->m_pGameMenu_guess )
			{
				setVisible( false );
				gViewPort->UpdateCursorState();
				gViewPort->m_pGameMenu_guess->setVisible( true );
				gViewPort->UpdateCursorState();
				gEngfuncs.pfnPlaySoundByName( "ui/3d_click.wav", 1 );
			}
		}
		else if ( gEngfuncs.pfnGetLevelName() && strstr( gEngfuncs.pfnGetLevelName(), "unlockable" ) )
		{
			gEngfuncs.pfnClientCmd( "disconnect" );
		}
		break;
	}
}

void CUnlockables::Initialize()
{
	m_iArmed_guess = 0;
	setVisible( false );
}

int CUnlockables::KeyInput_guess( int down, int keynum, const char *pszCurrentBinding )
{
	if ( pszCurrentBinding && !strcmp( pszCurrentBinding, "toggleconsole" ) )
		return 1;

	return 0;
}

void UnlockablesInit_guess()
{
	return;
}

void CUnlockables::setVisible( bool state )
{
	if ( state )
	{
		if ( gHUD.m_iUnlocks_guess & 1 || gHUD.m_iUnlocks_guess & 2 || gHUD.m_iUnlocks_guess & 4 || gHUD.m_iUnlocks_guess & 8 || gHUD.m_iUnlocks_guess & 16 )
			m_pLockedPanel_guess[0]->setVisible( false );
		else
			m_pLockedPanel_guess[0]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x20 )
			m_pLockedPanel_guess[1]->setVisible( false );
		else
			m_pLockedPanel_guess[1]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x400000 )
			m_pLockedPanel_guess[2]->setVisible( false );
		else
			m_pLockedPanel_guess[2]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x80000000 )
			m_pLockedPanel_guess[3]->setVisible( false );
		else
			m_pLockedPanel_guess[3]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x1000000 )
			m_pLockedPanel_guess[4]->setVisible( false );
		else
			m_pLockedPanel_guess[4]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x800000 )
			m_pLockedPanel_guess[5]->setVisible( false );
		else
			m_pLockedPanel_guess[5]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x40 )
			m_pLockedPanel_guess[6]->setVisible( false );
		else
			m_pLockedPanel_guess[6]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x80000 )
			m_pLockedPanel_guess[7]->setVisible( false );
		else
			m_pLockedPanel_guess[7]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x100000 )
			m_pLockedPanel_guess[8]->setVisible( false );
		else
			m_pLockedPanel_guess[8]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x200000 )
			m_pLockedPanel_guess[9]->setVisible( false );
		else
			m_pLockedPanel_guess[9]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x80 )
			m_pLockedPanel_guess[10]->setVisible( false );
		else
			m_pLockedPanel_guess[10]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x100 )
			m_pLockedPanel_guess[11]->setVisible( false );
		else
			m_pLockedPanel_guess[11]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x200 )
			m_pLockedPanel_guess[12]->setVisible( false );
		else
			m_pLockedPanel_guess[12]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x400 )
			m_pLockedPanel_guess[13]->setVisible( false );
		else
			m_pLockedPanel_guess[13]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x800 )
			m_pLockedPanel_guess[14]->setVisible( false );
		else
			m_pLockedPanel_guess[14]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x1000 )
			m_pLockedPanel_guess[15]->setVisible( false );
		else
			m_pLockedPanel_guess[15]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x2000 )
			m_pLockedPanel_guess[16]->setVisible( false );
		else
			m_pLockedPanel_guess[16]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x4000 )
			m_pLockedPanel_guess[17]->setVisible( false );
		else
			m_pLockedPanel_guess[17]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x8000 )
			m_pLockedPanel_guess[18]->setVisible( false );
		else
			m_pLockedPanel_guess[18]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x10000 )
			m_pLockedPanel_guess[19]->setVisible( false );
		else
			m_pLockedPanel_guess[19]->setVisible( true );
		if ( gHUD.m_iUnlocks_guess & 0x20000 )
			m_pLockedPanel_guess[20]->setVisible( false );
		else
			m_pLockedPanel_guess[20]->setVisible( true );
	}

	Panel::setVisible( state );
}

void CUnlockables::solve()
{
	char szText[128];

	if ( isVisible() )
	{
		for ( int i = 0; i < 21; i++ )
		{
			if ( m_pItemButton_guess[i] && m_pTextLabel_guess && m_pItemButton_guess[i]->isArmed() && m_iArmed_guess != i + 1 )
			{
				m_iArmed_guess = i + 1;

				switch ( i )
				{
				case 0:
					if ( gHUD.m_iUnlocks_guess & 1 || gHUD.m_iUnlocks_guess & 2 || gHUD.m_iUnlocks_guess & 4 || gHUD.m_iUnlocks_guess & 8 || gHUD.m_iUnlocks_guess & 16 )
						m_pTextLabel_guess->setText( "Click to view all unlocked hint pages." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 1:
					if ( gHUD.m_iUnlocks_guess & 0x20 )
						m_pTextLabel_guess->setText( "Nightmare difficulty mode." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 2:
					if ( gHUD.m_iUnlocks_guess & 0x400000 )
						m_pTextLabel_guess->setText( "Developer commentary - Enable in difficulty menu." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 3:
					if ( gHUD.m_iUnlocks_guess & 0x80000000 )
						m_pTextLabel_guess->setText( "Doctor\'s Story. Click to play." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 4:
					if ( gHUD.GetKeyForCommand_guess( "flashlight" ) )
						sprintf( szText, "Nightvision - press %s to enable.", gHUD.GetKeyForCommand_guess( "flashlight" ) );
					else
						strcpy( szText, "Nightvision - Press flashlight button to enable." );

					if ( gHUD.m_iUnlocks_guess & 0x1000000 )
						m_pTextLabel_guess->setText( szText );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 5:
					if ( gHUD.m_iUnlocks_guess & 0x800000 )
						m_pTextLabel_guess->setText( "Hidden package." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 6:
					if ( gHUD.m_iUnlocks_guess & 0x40 )
						m_pTextLabel_guess->setText( "FAMAS assault rifle with infinite ammo." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 7:
					if ( gHUD.m_iUnlocks_guess & 0x80000 )
						m_pTextLabel_guess->setText( "AoMDC axe." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 8:
					if ( gHUD.m_iUnlocks_guess & 0x100000 )
						m_pTextLabel_guess->setText( "Camera of death." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 9:
					if ( gHUD.m_iUnlocks_guess & 0x200000 )
						m_pTextLabel_guess->setText( "Simon\'s book." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 10:
					if ( gHUD.m_iUnlocks_guess & 0x80 )
						m_pTextLabel_guess->setText( "David Leatherhoff suit - Equip in secret room." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 11:
					if ( gHUD.m_iUnlocks_guess & 0x100 )
						m_pTextLabel_guess->setText( "ModDB hoodie - Equip in secret room." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 12:
					if ( gHUD.m_iUnlocks_guess & 0x200 )
						m_pTextLabel_guess->setText( "Hello Kitty suit - Equip in secret room." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 13:
					if ( gHUD.m_iUnlocks_guess & 0x400 )
						m_pTextLabel_guess->setText( "Afraid of Monsters suit - Equip in secret room." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 14:
					if ( gHUD.m_iUnlocks_guess & 0x800 )
						m_pTextLabel_guess->setText( "Camouflage hoodie - Equip in secret room." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 15:
					if ( gHUD.m_iUnlocks_guess & 0x1000 )
						m_pTextLabel_guess->setText( "Half-Life Creations hoodie - Equip in secret room." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 16:
					if ( gHUD.m_iUnlocks_guess & 0x2000 )
						m_pTextLabel_guess->setText( "Black Metal suit - Equip in secret room." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 17:
					if ( gHUD.m_iUnlocks_guess & 0x4000 )
						m_pTextLabel_guess->setText( "Team Psykskallar hoodie - Equip in secret room." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 18:
					if ( gHUD.m_iUnlocks_guess & 0x8000 )
						m_pTextLabel_guess->setText( "Awesome suit - Equip in secret room." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 19:
					if ( gHUD.m_iUnlocks_guess & 0x10000 )
						m_pTextLabel_guess->setText( "Sick Simon suit - Equip in secret room." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				case 20:
					if ( gHUD.m_iUnlocks_guess & 0x20000 )
						m_pTextLabel_guess->setText( "AoM Twitcher suit - Equip in secret room." );
					else
						m_pTextLabel_guess->setText( "This item is currently locked." );

					m_pTextLabel_guess->setContentAlignment( Label::a_center );
					break;
				}
			}
		}

		if ( m_pTextLabel_guess && m_iArmed_guess && !m_pItemButton_guess[m_iArmed_guess - 1]->isArmed() )
		{
			m_iArmed_guess = 0;
			m_pTextLabel_guess->setText( "" );
			m_pTextLabel_guess->setContentAlignment( Label::a_center );
		}
	}

	Panel::solve();
}
