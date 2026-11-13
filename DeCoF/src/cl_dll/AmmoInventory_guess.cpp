// ======================================
// Cry of Fear ammo inventory panel
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "AmmoInventory_guess.h"
#include "..\game_shared\vgui_loadtga.h"

CAmmoInventory::CAmmoInventory() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int wide, tall;
	int x, y;
	Panel *pPanel;

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

	m_pInventoryImage_guess = vgui_LoadTGA( "gfx/vgui/ammo/ammo_inventory.tga" );
	if ( m_pInventoryImage_guess )
		m_pInventoryImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/ammo/ammo_inventory.tga!\n" );

	m_pInventoryImage_guess->getSize( wide, tall );
	x = ScreenWidth / 2 - wide / 2;
	y = ScreenHeight / 2 - tall / 2;

	m_pInventory_guess = new ImagePanel( m_pInventoryImage_guess );
	m_pInventory_guess->setParent( this );
	m_pInventory_guess->setPos( x, y );
	m_pInventory_guess->setVisible( true );

	m_pSlotImage_guess[1] = new CImageLabel( NULL, 31, 45 );
	m_pSlotImage_guess[1]->setVisible( true );
	m_pSlotImage_guess[1]->setParent( m_pInventory_guess );
	m_pSlotImage_guess[1]->setImage( NULL );

	m_pSlotImage_guess[2] = new CImageLabel( NULL, 133, 45 );
	m_pSlotImage_guess[2]->setVisible( true );
	m_pSlotImage_guess[2]->setParent( m_pInventory_guess );
	m_pSlotImage_guess[2]->setImage( NULL );

	m_pSlotImage_guess[3] = new CImageLabel( NULL, 235, 45 );
	m_pSlotImage_guess[3]->setVisible( true );
	m_pSlotImage_guess[3]->setParent( m_pInventory_guess );
	m_pSlotImage_guess[3]->setImage( NULL );

	m_pSlotImage_guess[4] = new CImageLabel( NULL, 31, 167 );
	m_pSlotImage_guess[4]->setVisible( true );
	m_pSlotImage_guess[4]->setParent( m_pInventory_guess );
	m_pSlotImage_guess[4]->setImage( NULL );

	m_pSlotImage_guess[5] = new CImageLabel( NULL, 133, 167 );
	m_pSlotImage_guess[5]->setVisible( true );
	m_pSlotImage_guess[5]->setParent( m_pInventory_guess );
	m_pSlotImage_guess[5]->setImage( NULL );

	m_pSlotImage_guess[6] = new CImageLabel( NULL, 235, 167 );
	m_pSlotImage_guess[6]->setVisible( true );
	m_pSlotImage_guess[6]->setParent( m_pInventory_guess );
	m_pSlotImage_guess[6]->setImage( NULL );

	m_pCountLabel_guess[1] = new Label( "", 36, 50, 99, 99 );
	m_pCountLabel_guess[1]->setVisible( true );
	m_pCountLabel_guess[1]->setParent( m_pInventory_guess );
	m_pCountLabel_guess[1]->setBgColor( 0, 0, 0, 255 );
	m_pCountLabel_guess[1]->setFgColor( 255, 255, 255, 0 );

	m_pCountLabel_guess[2] = new Label( "", 138, 50, 99, 99 );
	m_pCountLabel_guess[2]->setVisible( true );
	m_pCountLabel_guess[2]->setParent( m_pInventory_guess );
	m_pCountLabel_guess[2]->setBgColor( 0, 0, 0, 255 );
	m_pCountLabel_guess[2]->setFgColor( 255, 255, 255, 0 );

	m_pCountLabel_guess[3] = new Label( "", 240, 50, 99, 99 );
	m_pCountLabel_guess[3]->setVisible( true );
	m_pCountLabel_guess[3]->setParent( m_pInventory_guess );
	m_pCountLabel_guess[3]->setBgColor( 0, 0, 0, 255 );
	m_pCountLabel_guess[3]->setFgColor( 255, 255, 255, 0 );

	m_pCountLabel_guess[4] = new Label( "", 36, 172, 99, 99 );
	m_pCountLabel_guess[4]->setVisible( true );
	m_pCountLabel_guess[4]->setParent( m_pInventory_guess );
	m_pCountLabel_guess[4]->setBgColor( 0, 0, 0, 255 );
	m_pCountLabel_guess[4]->setFgColor( 255, 255, 255, 0 );

	m_pCountLabel_guess[5] = new Label( "", 138, 172, 99, 99 );
	m_pCountLabel_guess[5]->setVisible( true );
	m_pCountLabel_guess[5]->setParent( m_pInventory_guess );
	m_pCountLabel_guess[5]->setBgColor( 0, 0, 0, 255 );
	m_pCountLabel_guess[5]->setFgColor( 255, 255, 255, 0 );

	m_pCountLabel_guess[6] = new Label( "", 240, 172, 99, 99 );
	m_pCountLabel_guess[6]->setVisible( true );
	m_pCountLabel_guess[6]->setParent( m_pInventory_guess );
	m_pCountLabel_guess[6]->setBgColor( 0, 0, 0, 255 );
	m_pCountLabel_guess[6]->setFgColor( 255, 255, 255, 0 );

	m_pSelectedImage_guess = new CImageLabel( NULL, 31, 289 );
	m_pSelectedImage_guess->setVisible( true );
	m_pSelectedImage_guess->setParent( m_pInventory_guess );
	m_pSelectedImage_guess->setImage( NULL );

	m_pButton_guess[1] = new ColorButton( "", 36, 50, 99, 99, false, false );
	m_pButton_guess[1]->setParent( m_pInventory_guess );
	m_pButton_guess[1]->addActionSignal( new CAmmoInventoryHandler_Command( this, 1 ) );

	m_pButton_guess[2] = new ColorButton( "", 138, 50, 99, 99, false, false );
	m_pButton_guess[2]->setParent( m_pInventory_guess );
	m_pButton_guess[2]->addActionSignal( new CAmmoInventoryHandler_Command( this, 2 ) );

	m_pButton_guess[3] = new ColorButton( "", 240, 50, 99, 99, false, false );
	m_pButton_guess[3]->setParent( m_pInventory_guess );
	m_pButton_guess[3]->addActionSignal( new CAmmoInventoryHandler_Command( this, 3 ) );

	m_pButton_guess[4] = new ColorButton( "", 36, 172, 99, 99, false, false );
	m_pButton_guess[4]->setParent( m_pInventory_guess );
	m_pButton_guess[4]->addActionSignal( new CAmmoInventoryHandler_Command( this, 4 ) );

	m_pButton_guess[5] = new ColorButton( "", 138, 172, 99, 99, false, false );
	m_pButton_guess[5]->setParent( m_pInventory_guess );
	m_pButton_guess[5]->addActionSignal( new CAmmoInventoryHandler_Command( this, 5 ) );

	m_pButton_guess[6] = new ColorButton( "", 240, 172, 99, 99, false, false );
	m_pButton_guess[6]->setParent( m_pInventory_guess );
	m_pButton_guess[6]->addActionSignal( new CAmmoInventoryHandler_Command( this, 6 ) );

	m_pDropButton_guess = new ColorButton( "", 7, 415, 360, 50, false, false );
	m_pDropButton_guess->setParent( m_pInventory_guess );
	m_pDropButton_guess->addActionSignal( new CAmmoInventoryHandler_Command( this, 7 ) );

	setBgColor( 0, 0, 0, 0 );

	Initialize();
}

void CAmmoInventory::ActionSignal( int cmd )
{
	switch ( cmd )
	{
	case 1:
		if ( m_iDropMode_guess )
		{
			if ( m_iAmmoType_guess[1] )
			{
				gEngfuncs.pfnServerCmd( "dropammo 1" );
				gEngfuncs.pfnPlaySoundByName( "inventory/inventory_click.wav", 1 );
			}
			else
			{
				gEngfuncs.pfnPlaySoundByName( "inventory/inventory_no.wav", 1 );
			}
		}
		break;
	case 2:
		if ( m_iDropMode_guess )
		{
			if ( m_iAmmoType_guess[2] )
			{
				gEngfuncs.pfnServerCmd( "dropammo 2" );
				gEngfuncs.pfnPlaySoundByName( "inventory/inventory_click.wav", 1 );
			}
			else
			{
				gEngfuncs.pfnPlaySoundByName( "inventory/inventory_no.wav", 1 );
			}
		}
		break;
	case 3:
		if ( m_iDropMode_guess )
		{
			if ( m_iAmmoType_guess[3] )
			{
				gEngfuncs.pfnServerCmd( "dropammo 3" );
				gEngfuncs.pfnPlaySoundByName( "inventory/inventory_click.wav", 1 );
			}
			else
			{
				gEngfuncs.pfnPlaySoundByName( "inventory/inventory_no.wav", 1 );
			}
		}
		break;
	case 4:
		if ( m_iDropMode_guess )
		{
			if ( m_iAmmoType_guess[4] )
			{
				gEngfuncs.pfnServerCmd( "dropammo 4" );
				gEngfuncs.pfnPlaySoundByName( "inventory/inventory_click.wav", 1 );
			}
			else
			{
				gEngfuncs.pfnPlaySoundByName( "inventory/inventory_no.wav", 1 );
			}
		}
		break;
	case 5:
		if ( m_iDropMode_guess )
		{
			if ( m_iAmmoType_guess[5] )
			{
				gEngfuncs.pfnServerCmd( "dropammo 5" );
				gEngfuncs.pfnPlaySoundByName( "inventory/inventory_click.wav", 1 );
			}
			else
			{
				gEngfuncs.pfnPlaySoundByName( "inventory/inventory_no.wav", 1 );
			}
		}
		break;
	case 6:
		if ( m_iDropMode_guess )
		{
			if ( m_iAmmoType_guess[6] )
			{
				gEngfuncs.pfnServerCmd( "dropammo 6" );
				gEngfuncs.pfnPlaySoundByName( "inventory/inventory_click.wav", 1 );
			}
			else
			{
				gEngfuncs.pfnPlaySoundByName( "inventory/inventory_no.wav", 1 );
			}
		}
		break;
	case 7:
		m_iDropMode_guess = 1;
		gEngfuncs.pfnPlaySoundByName( "inventory/inventory_toggle.wav", 1 );
		break;
	}
}

void CAmmoInventory::Initialize()
{
	m_iDropMode_guess = 0;
	m_iSelected_guess = 0;
	setVisible( false );

	m_iAmmoType_guess[1] = 0;
	m_iAmmoType_guess[2] = 0;
	m_iAmmoType_guess[3] = 0;
	m_iAmmoType_guess[4] = 0;
	m_iAmmoType_guess[5] = 0;
	m_iAmmoType_guess[6] = 0;
}

int CAmmoInventory::KeyInput( int down, int keynum, const char *pszCurrentBinding )
{
	if ( pszCurrentBinding )
	{
		if ( !strcmp( pszCurrentBinding, "toggleconsole" ) )
			return 1;

		if ( !strcmp( pszCurrentBinding, "+inventory" ) && isVisible() )
		{
			setVisible( false );
			gViewPort->UpdateCursorState();
			gEngfuncs.pfnPlaySoundByName( "inventory/inventory_close.wav", 1 );
		}
	}

	return 0;
}

int MsgAmmoInv_guess( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	if ( gViewPort->m_pAmmoInventory_guess )
	{
		int iSlot = READ_BYTE();
		int iType = READ_BYTE();
		int iCount = READ_BYTE();
		char szCount[4];

		itoa( iCount, szCount, 10 );

		gViewPort->m_pAmmoInventory_guess->m_iAmmoType_guess[iSlot] = iType;
		gViewPort->m_pAmmoInventory_guess->m_pCountLabel_guess[iSlot]->setText( szCount );
		gViewPort->m_pAmmoInventory_guess->m_pCountLabel_guess[iSlot]->setContentAlignment( Label::a_southwest );

		if ( iType )
		{
			switch ( iType )
			{
			case 1:
				if ( gViewPort->m_pAmmoInventory_guess->m_pSlotImage_guess[iSlot] )
					gViewPort->m_pAmmoInventory_guess->m_pSlotImage_guess[iSlot]->setImage( vgui_LoadTGA( "gfx/vgui/ammo/glock_slot.tga" ) );
				break;
			case 2:
				if ( gViewPort->m_pAmmoInventory_guess->m_pSlotImage_guess[iSlot] )
					gViewPort->m_pAmmoInventory_guess->m_pSlotImage_guess[iSlot]->setImage( vgui_LoadTGA( "gfx/vgui/ammo/m16_slot.tga" ) );
				break;
			case 3:
				if ( gViewPort->m_pAmmoInventory_guess->m_pSlotImage_guess[iSlot] )
					gViewPort->m_pAmmoInventory_guess->m_pSlotImage_guess[iSlot]->setImage( vgui_LoadTGA( "gfx/vgui/ammo/p345_slot.tga" ) );
				break;
			case 4:
				if ( gViewPort->m_pAmmoInventory_guess->m_pSlotImage_guess[iSlot] )
					gViewPort->m_pAmmoInventory_guess->m_pSlotImage_guess[iSlot]->setImage( vgui_LoadTGA( "gfx/vgui/ammo/shotgun_slot.tga" ) );
				break;
			case 5:
				if ( gViewPort->m_pAmmoInventory_guess->m_pSlotImage_guess[iSlot] )
					gViewPort->m_pAmmoInventory_guess->m_pSlotImage_guess[iSlot]->setImage( vgui_LoadTGA( "gfx/vgui/ammo/tmp_slot.tga" ) );
				break;
			}
		}
		else
		{
			if ( gViewPort->m_pAmmoInventory_guess->m_pSlotImage_guess[iSlot] )
			{
				gViewPort->m_pAmmoInventory_guess->m_pSlotImage_guess[iSlot]->setImage( NULL );

				if ( gViewPort->m_pAmmoInventory_guess->m_iSelected_guess == iSlot && gViewPort->m_pAmmoInventory_guess->m_pSelectedImage_guess )
					gViewPort->m_pAmmoInventory_guess->m_pSelectedImage_guess->setImage( NULL );
			}
		}
	}

	return 1;
}

void VGUI_AmmoInvInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "AmmoInv", MsgAmmoInv_guess );
}

void CAmmoInventory::setVisible( bool state )
{
	int i;

	m_iDropMode_guess = 0;
	m_iSelected_guess = 0;

	if ( m_pSelectedImage_guess )
		m_pSelectedImage_guess->setImage( NULL );

	if ( gEngfuncs.pfnGetLevelName() )
	{
		for ( i = 1; i <= MAX_AMMO_SLOTS_guess; i++ )
		{
			if ( m_pButton_guess[i] && m_pButton_guess[i]->isVisible() )
				m_pButton_guess[i]->cursorExited();
		}

		if ( m_pDropButton_guess )
			m_pDropButton_guess->cursorExited();

		if ( state )
		{
			for ( i = 1; i <= MAX_AMMO_SLOTS_guess; i++ )
			{
				switch ( m_iAmmoType_guess[i] )
				{
				case 1:
					if ( m_pSlotImage_guess[i] )
						m_pSlotImage_guess[i]->setImage( vgui_LoadTGA( "gfx/vgui/ammo/glock_slot.tga" ) );
					break;
				case 2:
					if ( m_pSlotImage_guess[i] )
						m_pSlotImage_guess[i]->setImage( vgui_LoadTGA( "gfx/vgui/ammo/m16_slot.tga" ) );
					break;
				case 3:
					if ( m_pSlotImage_guess[i] )
						m_pSlotImage_guess[i]->setImage( vgui_LoadTGA( "gfx/vgui/ammo/p345_slot.tga" ) );
					break;
				case 4:
					if ( m_pSlotImage_guess[i] )
						m_pSlotImage_guess[i]->setImage( vgui_LoadTGA( "gfx/vgui/ammo/shotgun_slot.tga" ) );
					break;
				case 5:
					if ( m_pSlotImage_guess[i] )
						m_pSlotImage_guess[i]->setImage( vgui_LoadTGA( "gfx/vgui/ammo/tmp_slot.tga" ) );
					break;
				}
			}
		}
	}

	Panel::setVisible( state );
}

void CAmmoInventory::solve()
{
	if ( isVisible() )
	{
		if ( m_iSelected_guess && m_pButton_guess[m_iSelected_guess] && !m_pButton_guess[m_iSelected_guess]->isArmed() )
		{
			m_iSelected_guess = 0;
			m_pSelectedImage_guess->setImage( NULL );
		}

		for ( int i = 1; i <= MAX_AMMO_SLOTS_guess; i++ )
		{
			if ( m_pButton_guess[i] && m_pButton_guess[i]->isArmed() && m_iSelected_guess != i && m_iAmmoType_guess[i] )
			{
				m_iSelected_guess = i;

				switch ( m_iAmmoType_guess[i] )
				{
				case 1:
					m_pSelectedImage_guess->setImage( vgui_LoadTGA( "gfx/vgui/ammo/glock_check.tga" ) );
					break;
				case 2:
					m_pSelectedImage_guess->setImage( vgui_LoadTGA( "gfx/vgui/ammo/m16_check.tga" ) );
					break;
				case 3:
					m_pSelectedImage_guess->setImage( vgui_LoadTGA( "gfx/vgui/ammo/p345_check.tga" ) );
					break;
				case 4:
					m_pSelectedImage_guess->setImage( vgui_LoadTGA( "gfx/vgui/ammo/shotgun_check.tga" ) );
					break;
				case 5:
					m_pSelectedImage_guess->setImage( vgui_LoadTGA( "gfx/vgui/ammo/tmp_check.tga" ) );
					break;
				}
			}
		}
	}

	Panel::solve();
}

CAmmoInventory::~CAmmoInventory( void )
{
	delete m_pInventoryImage_guess;
}
