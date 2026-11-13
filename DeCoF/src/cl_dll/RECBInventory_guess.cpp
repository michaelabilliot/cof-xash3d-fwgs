// ======================================
// Cry of Fear inventory panel
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "RECBInventory_guess.h"
#include "Documents_guess.h"
#include "HUDControl_guess.h"
#include "..\game_shared\vgui_loadtga.h"

extern int iWheelchair_guess;

bool g_bInventoryFadeOut_guess;

// plays a sound by name; only __CmdFunc_Unlockables_guess (hud.cpp) calls it
void InventorySound_guess( char *pszSound, float flVolume )
{
	gEngfuncs.pfnPlaySoundByName( pszSound, flVolume );
}

CRECBInventory::CRECBInventory( int iTrans, int iRemoveMe, int x, int y, int wide, int tall ) : CMenuPanel( iTrans, iRemoveMe, x, y, wide, tall )
{
	int iWide, iTall;
	int iX, iY;
	int i;

	m_pBackgroundPanel_guess = new Panel( 0, 0, ScreenWidth, ScreenHeight );
	m_pBackgroundPanel_guess->setParent( this );
	m_pBackgroundPanel_guess->setBgColor( 0, 0, 0, 0 );

	m_iThree_guess = 0;

	m_pBackgroundImage_guess = vgui_LoadTGA( "gfx/vgui/backgroundtoall.tga" );
	if ( m_pBackgroundImage_guess )
		m_pBackgroundImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/backgroundtoall.tga!\n" );

	m_pBackgroundImage_guess->getSize( iWide, iTall );
	iX = ScreenWidth / 2 - iWide / 2;
	iY = ScreenHeight / 2 - iTall / 2;

	m_pBackground_guess = new ImagePanel( m_pBackgroundImage_guess );
	m_pBackground_guess->setParent( this );
	m_pBackground_guess->setPos( iX, iY );
	m_pBackground_guess->setVisible( true );

	iX = ScreenWidth / 2 - 374;
	iY = ScreenHeight / 2 - 240;

	m_pInventoryImage_guess = vgui_LoadTGA( "gfx/vgui/640_inventory.tga" );
	if ( m_pInventoryImage_guess )
		m_pInventoryImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/640_inventory.tga!\n" );

	m_pInventory_guess = new ImagePanel( m_pInventoryImage_guess );
	m_pInventory_guess->setParent( this );
	m_pInventory_guess->setPos( iX, iY );
	m_pInventory_guess->setVisible( true );

	m_pItemImages_guess[0] = new CImageLabel( NULL, 31, 68 );
	m_pItemImages_guess[0]->setVisible( true );
	m_pItemImages_guess[0]->setParent( m_pInventory_guess );
	m_pItemImages_guess[0]->setImage( NULL );

	m_pItemImages_guess[1] = new CImageLabel( NULL, 133, 68 );
	m_pItemImages_guess[1]->setVisible( true );
	m_pItemImages_guess[1]->setParent( m_pInventory_guess );
	m_pItemImages_guess[1]->setImage( NULL );

	m_pItemImages_guess[2] = new CImageLabel( NULL, 235, 68 );
	m_pItemImages_guess[2]->setVisible( true );
	m_pItemImages_guess[2]->setParent( m_pInventory_guess );
	m_pItemImages_guess[2]->setImage( NULL );

	m_pItemImages_guess[3] = new CImageLabel( NULL, 31, 227 );
	m_pItemImages_guess[3]->setVisible( true );
	m_pItemImages_guess[3]->setParent( m_pInventory_guess );
	m_pItemImages_guess[3]->setImage( NULL );

	m_pItemImages_guess[4] = new CImageLabel( NULL, 133, 227 );
	m_pItemImages_guess[4]->setVisible( true );
	m_pItemImages_guess[4]->setParent( m_pInventory_guess );
	m_pItemImages_guess[4]->setImage( NULL );

	m_pItemImages_guess[5] = new CImageLabel( NULL, 235, 227 );
	m_pItemImages_guess[5]->setVisible( true );
	m_pItemImages_guess[5]->setParent( m_pInventory_guess );
	m_pItemImages_guess[5]->setImage( NULL );

	m_pItemImages_guess[6] = new CImageLabel( NULL, 398, 68 );
	m_pItemImages_guess[6]->setVisible( true );
	m_pItemImages_guess[6]->setParent( m_pInventory_guess );
	m_pItemImages_guess[6]->setImage( NULL );

	m_pItemImages_guess[7] = new CImageLabel( NULL, 500, 68 );
	m_pItemImages_guess[7]->setVisible( true );
	m_pItemImages_guess[7]->setParent( m_pInventory_guess );
	m_pItemImages_guess[7]->setImage( NULL );

	m_pItemImages_guess[8] = new CImageLabel( NULL, 31, 348 );
	m_pItemImages_guess[8]->setVisible( true );
	m_pItemImages_guess[8]->setParent( m_pInventory_guess );
	m_pItemImages_guess[8]->setImage( NULL );

	m_pItemImages_guess[9] = new CImageLabel( NULL, 602, 68 );
	m_pItemImages_guess[9]->setVisible( true );
	m_pItemImages_guess[9]->setParent( m_pInventory_guess );
	m_pItemImages_guess[9]->setImage( NULL );

	m_pItemButtons_guess[0] = new ColorButton( "", 36, 73, 99, 99, false, false );
	m_pItemButtons_guess[0]->setParent( m_pInventory_guess );
	m_pItemButtons_guess[0]->addActionSignal( new CRECBInventoryHandler_Command( this, 1 ) );

	m_pItemButtons_guess[1] = new ColorButton( "", 138, 73, 99, 99, false, false );
	m_pItemButtons_guess[1]->setParent( m_pInventory_guess );
	m_pItemButtons_guess[1]->addActionSignal( new CRECBInventoryHandler_Command( this, 2 ) );

	m_pItemButtons_guess[2] = new ColorButton( "", 240, 73, 99, 99, false, false );
	m_pItemButtons_guess[2]->setParent( m_pInventory_guess );
	m_pItemButtons_guess[2]->addActionSignal( new CRECBInventoryHandler_Command( this, 3 ) );

	m_pItemButtons_guess[3] = new ColorButton( "", 36, 232, 99, 99, false, false );
	m_pItemButtons_guess[3]->setParent( m_pInventory_guess );
	m_pItemButtons_guess[3]->addActionSignal( new CRECBInventoryHandler_Command( this, 4 ) );

	m_pItemButtons_guess[4] = new ColorButton( "", 138, 232, 99, 99, false, false );
	m_pItemButtons_guess[4]->setParent( m_pInventory_guess );
	m_pItemButtons_guess[4]->addActionSignal( new CRECBInventoryHandler_Command( this, 5 ) );

	m_pItemButtons_guess[5] = new ColorButton( "", 240, 232, 99, 99, false, false );
	m_pItemButtons_guess[5]->setParent( m_pInventory_guess );
	m_pItemButtons_guess[5]->addActionSignal( new CRECBInventoryHandler_Command( this, 6 ) );

	m_pOptionButtons_guess[0] = new ColorButton( "", 373, 226, 363, 47, false, false );
	m_pOptionButtons_guess[0]->setParent( m_pInventory_guess );
	m_pOptionButtons_guess[0]->addActionSignal( new CRECBInventoryHandler_Command( this, 7 ) );
	m_pOptionButtons_guess[0]->setVisible( false );

	m_pOptionButtons_guess[1] = new ColorButton( "", 373, 276, 363, 47, false, false );
	m_pOptionButtons_guess[1]->setParent( m_pInventory_guess );
	m_pOptionButtons_guess[1]->addActionSignal( new CRECBInventoryHandler_Command( this, 8 ) );
	m_pOptionButtons_guess[1]->setVisible( false );

	m_pOptionButtons_guess[2] = new ColorButton( "", 373, 325, 363, 47, false, false );
	m_pOptionButtons_guess[2]->setParent( m_pInventory_guess );
	m_pOptionButtons_guess[2]->addActionSignal( new CRECBInventoryHandler_Command( this, 10 ) );
	m_pOptionButtons_guess[2]->setVisible( false );

	m_pOptionButtons_guess[3] = new ColorButton( "", 373, 376, 363, 47, false, false );
	m_pOptionButtons_guess[3]->setParent( m_pInventory_guess );
	m_pOptionButtons_guess[3]->addActionSignal( new CRECBInventoryHandler_Command( this, 15 ) );
	m_pOptionButtons_guess[3]->setVisible( false );

	m_pOptionButtons_guess[4] = new ColorButton( "", 373, 226, 363, 97, false, false );
	m_pOptionButtons_guess[4]->setParent( m_pInventory_guess );
	m_pOptionButtons_guess[4]->addActionSignal( new CRECBInventoryHandler_Command( this, 16 ) );

	for ( i = 0; i < 6; i++ )
	{
		m_pItemButtons_guess[i]->setUnArmedBorderColor( 0, 0, 0, 255 );
		m_pItemButtons_guess[i]->setArmedBorderColor( 255, 255, 255, 128 );
		m_pItemButtons_guess[i]->setUnArmedColor( 0, 0, 0, 255 );
		m_pItemButtons_guess[i]->setArmedColor( 0, 0, 0, 255 );
	}

	for ( i = 0; i < 5; i++ )
	{
		m_pOptionButtons_guess[i]->setUnArmedBorderColor( 0, 0, 0, 255 );
		m_pOptionButtons_guess[i]->setArmedBorderColor( 255, 255, 255, 128 );
		m_pOptionButtons_guess[i]->setUnArmedColor( 0, 0, 0, 255 );
		m_pOptionButtons_guess[i]->setArmedColor( 0, 0, 0, 255 );
	}

	m_pQuickSlot1Button_guess = new ColorButton( "", 403, 73, 99, 99, false, false );
	m_pQuickSlot1Button_guess->setParent( m_pInventory_guess );
	m_pQuickSlot1Button_guess->addActionSignal( new CRECBInventoryHandler_Command( this, 12 ) );
	m_pQuickSlot1Button_guess->setUnArmedBorderColor( 0, 0, 0, 255 );
	m_pQuickSlot1Button_guess->setArmedBorderColor( 255, 255, 255, 255 );
	m_pQuickSlot1Button_guess->setUnArmedColor( 0, 0, 0, 255 );
	m_pQuickSlot1Button_guess->setArmedColor( 0, 0, 0, 255 );

	m_pQuickSlot2Button_guess = new ColorButton( "", 505, 73, 99, 99, false, false );
	m_pQuickSlot2Button_guess->setParent( m_pInventory_guess );
	m_pQuickSlot2Button_guess->addActionSignal( new CRECBInventoryHandler_Command( this, 13 ) );
	m_pQuickSlot2Button_guess->setUnArmedBorderColor( 0, 0, 0, 255 );
	m_pQuickSlot2Button_guess->setArmedBorderColor( 255, 255, 255, 255 );
	m_pQuickSlot2Button_guess->setUnArmedColor( 0, 0, 0, 255 );
	m_pQuickSlot2Button_guess->setArmedColor( 0, 0, 0, 255 );

	m_pQuickSlot3Button_guess = new ColorButton( "", 607, 73, 99, 99, false, false );
	m_pQuickSlot3Button_guess->setParent( m_pInventory_guess );
	m_pQuickSlot3Button_guess->addActionSignal( new CRECBInventoryHandler_Command( this, 14 ) );
	m_pQuickSlot3Button_guess->setUnArmedBorderColor( 0, 0, 0, 255 );
	m_pQuickSlot3Button_guess->setArmedBorderColor( 255, 255, 255, 255 );
	m_pQuickSlot3Button_guess->setUnArmedColor( 0, 0, 0, 255 );
	m_pQuickSlot3Button_guess->setArmedColor( 0, 0, 0, 255 );

	m_pPopupImage_guess = new CImageLabel( NULL, 0, 0 );
	m_pPopupImage_guess->setVisible( false );
	m_pPopupImage_guess->setParent( this );
	m_pPopupImage_guess->setImage( NULL );

	m_pMenuImage_guess = new CImageLabel( NULL, -5, -5 );
	m_pMenuImage_guess->setVisible( false );
	m_pMenuImage_guess->setParent( m_pInventory_guess );
	m_pMenuImage_guess->setImage( vgui_LoadTGA( "gfx/vgui/inventory_weapons.tga" ) );

	m_pMenuButtons_guess[0] = new ColorButton( "", 0, 0, 126, 42, false, false );
	m_pMenuButtons_guess[0]->setParent( m_pInventory_guess );
	m_pMenuButtons_guess[0]->addActionSignal( new CRECBInventoryHandler_Command( this, 50 ) );
	m_pMenuButtons_guess[0]->setVisible( false );
	m_pMenuButtons_guess[0]->setContentAlignment( Label::a_center );

	m_pMenuButtons_guess[1] = new ColorButton( "", 0, 42, 126, 42, false, false );
	m_pMenuButtons_guess[1]->setParent( m_pInventory_guess );
	m_pMenuButtons_guess[1]->addActionSignal( new CRECBInventoryHandler_Command( this, 51 ) );
	m_pMenuButtons_guess[1]->setVisible( false );
	m_pMenuButtons_guess[1]->setContentAlignment( Label::a_center );

	m_pMenuButtons_guess[2] = new ColorButton( "", 0, 84, 126, 42, false, false );
	m_pMenuButtons_guess[2]->setParent( m_pInventory_guess );
	m_pMenuButtons_guess[2]->addActionSignal( new CRECBInventoryHandler_Command( this, 52 ) );
	m_pMenuButtons_guess[2]->setVisible( false );
	m_pMenuButtons_guess[2]->setContentAlignment( Label::a_center );

	m_pMenuButtons_guess[3] = new ColorButton( "", 0, 126, 126, 42, false, false );
	m_pMenuButtons_guess[3]->setParent( m_pInventory_guess );
	m_pMenuButtons_guess[3]->addActionSignal( new CRECBInventoryHandler_Command( this, 53 ) );
	m_pMenuButtons_guess[3]->setVisible( false );
	m_pMenuButtons_guess[3]->setContentAlignment( Label::a_center );

	m_pMenuButtons_guess[4] = new ColorButton( "", 0, 126, 42, 42, false, false );
	m_pMenuButtons_guess[4]->setParent( m_pInventory_guess );
	m_pMenuButtons_guess[4]->addActionSignal( new CRECBInventoryHandler_Command( this, 54 ) );
	m_pMenuButtons_guess[4]->setVisible( false );
	m_pMenuButtons_guess[4]->setContentAlignment( Label::a_center );

	m_pMenuButtons_guess[5] = new ColorButton( "", 42, 126, 42, 42, false, false );
	m_pMenuButtons_guess[5]->setParent( m_pInventory_guess );
	m_pMenuButtons_guess[5]->addActionSignal( new CRECBInventoryHandler_Command( this, 55 ) );
	m_pMenuButtons_guess[5]->setVisible( false );
	m_pMenuButtons_guess[5]->setContentAlignment( Label::a_center );

	m_pMenuButtons_guess[6] = new ColorButton( "", 84, 126, 42, 42, false, false );
	m_pMenuButtons_guess[6]->setParent( m_pInventory_guess );
	m_pMenuButtons_guess[6]->addActionSignal( new CRECBInventoryHandler_Command( this, 56 ) );
	m_pMenuButtons_guess[6]->setVisible( false );
	m_pMenuButtons_guess[6]->setContentAlignment( Label::a_center );

	for ( i = 0; i < 7; i++ )
	{
		m_pMenuButtons_guess[i]->setUnArmedBorderColor( 255, 255, 255, 128 );
		m_pMenuButtons_guess[i]->setArmedBorderColor( 255, 255, 255, 0 );
		m_pMenuButtons_guess[i]->setUnArmedColor( 255, 255, 255, 128 );
		m_pMenuButtons_guess[i]->setArmedColor( 255, 255, 255, 0 );
	}

	m_pSelection_guess = new Panel( 36, 73, 99, 99 );
	m_pSelection_guess->setParent( m_pInventory_guess );
	m_pSelection_guess->setBgColor( 0, 0, 0, 255 );
	m_pSelection_guess->setVisible( false );
	m_pSelection_guess->setBorder( new LineBorder( Color( 255, 0, 0, 255 ) ) );
	m_pSelection_guess->setPaintBorderEnabled( true );

	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hInventoryScheme = pSchemes->getSchemeHandle( "inventory" );
	Font *pInventoryFont = pSchemes->getFont( hInventoryScheme );

	m_pObjectiveLabel_guess = new Label( "", 404, 354, 301, 97 );
	m_pObjectiveLabel_guess->setParent( m_pInventory_guess );
	m_pObjectiveLabel_guess->setBgColor( 0, 0, 0, 255 );
	m_pObjectiveLabel_guess->setFgColor( 255, 255, 255, 0 );
	m_pObjectiveLabel_guess->setFont( pInventoryFont );
	m_pObjectiveLabel_guess->setContentAlignment( Label::a_center );
	m_pObjectiveLabel_guess->setVisible( true );
}

void CRECBInventory::ActionSignal( int cmd )
{
	char szCommand[256];
	char szCmd[32];
	int i;

	const char *pszLevel = gEngfuncs.pfnGetLevelName();

	switch ( cmd )
	{
	case 0:
		break;
	case 1:
		SelectItem_guess( 1 );
		break;
	case 2:
		SelectItem_guess( 2 );
		break;
	case 3:
		SelectItem_guess( 3 );
		break;
	case 4:
		SelectItem_guess( 4 );
		break;
	case 5:
		SelectItem_guess( 5 );
		break;
	case 6:
		SelectItem_guess( 6 );
		break;
	case 7:
		SetAction_guess( 1 );
		break;
	case 8:
		SetAction_guess( 2 );
		break;
	case 9:
		SetAction_guess( 3 );
		break;
	case 10:
		SetAction_guess( 4 );
		break;
	case 11:
		gViewPort->HideTopMenu();
		break;
	case 12:
		break;
	case 13:
		break;
	case 14:
		break;
	case 15:
		if ( !strstr( pszLevel, "cof_" ) )
			SetAction_guess( 7 );
		break;
	case 16:
		if ( !strstr( pszLevel, "cof_" ) && gViewPort && gViewPort->m_pDocuments_guess )
		{
			gViewPort->HideTopMenu();
			gViewPort->m_pDocuments_guess->setVisible( true );
			gViewPort->UpdateCursorState();
		}
		break;
	case 50:
		sprintf( szCmd, "inventoryequip %i\n", m_iSelected_guess );
		gEngfuncs.pfnServerCmd( szCmd );
		m_pMenuImage_guess->setVisible( false );
		for ( i = 0; i < 7; i++ )
			m_pMenuButtons_guess[i]->setVisible( false );
		break;
	case 51:
		{
			int x, y;

			m_iCombine_guess = m_iSelected_guess;
			m_pItemButtons_guess[m_iSelected_guess - 1]->getPos( x, y );
			m_iSelectionAlpha_guess = 255;
			g_bInventoryFadeOut_guess = true;
			m_pMenuImage_guess->setVisible( false );
			m_pSelection_guess->setPos( x, y );
			m_pSelection_guess->setVisible( true );
			for ( i = 0; i < 7; i++ )
				m_pMenuButtons_guess[i]->setVisible( false );
		}
		break;
	case 52:
		sprintf( szCmd, "inventorydrop %i\n", m_iSelected_guess );
		gEngfuncs.pfnServerCmd( szCmd );
		m_pMenuImage_guess->setVisible( false );
		for ( i = 0; i < 7; i++ )
			m_pMenuButtons_guess[i]->setVisible( false );
		break;
	case 53:
		{
			int x, y;

			m_iDualWield_guess = m_iSelected_guess;
			m_pItemButtons_guess[m_iSelected_guess - 1]->getPos( x, y );
			m_iSelectionAlpha_guess = 255;
			g_bInventoryFadeOut_guess = true;
			m_pSelection_guess->setPos( x, y );
			m_pSelection_guess->setVisible( true );
			m_pMenuImage_guess->setVisible( false );
			for ( i = 0; i < 7; i++ )
				m_pMenuButtons_guess[i]->setVisible( false );
		}
		break;
	case 54:
		if ( strlen( gHUD.GetSlotItem_guess( m_iSelected_guess ) ) && strstr( gHUD.GetSlotItem_guess( m_iSelected_guess ), "weapon_" ) )
		{
			m_pItemImages_guess[6]->setImage( vgui_LoadTGA( gHUD.CheckItem( "inventory_picture", gHUD.GetSlotItem_guess( m_iSelected_guess ) ) ) );
			strncpy( gHUD.m_szInventoryWeapon1_guess, gHUD.GetSlotItem_guess( m_iSelected_guess ), 128 );
			gEngfuncs.pfnPlaySoundByName( "inventory/inventory_click.wav", 1 );
			sprintf( szCommand, "quickselset1 %i\n", m_iSelected_guess );
			gEngfuncs.pfnServerCmd( szCommand );
			m_pMenuImage_guess->setVisible( false );
			for ( i = 0; i < 7; i++ )
				m_pMenuButtons_guess[i]->setVisible( false );
		}
		else
			gEngfuncs.pfnPlaySoundByName( "inventory/inventory_no.wav", 1 );

		m_iAction_guess = 0;
		break;
	case 55:
		if ( strlen( gHUD.GetSlotItem_guess( m_iSelected_guess ) ) && strstr( gHUD.GetSlotItem_guess( m_iSelected_guess ), "weapon_" ) )
		{
			m_pItemImages_guess[7]->setImage( vgui_LoadTGA( gHUD.CheckItem( "inventory_picture", gHUD.GetSlotItem_guess( m_iSelected_guess ) ) ) );
			strncpy( gHUD.m_szInventoryWeapon2_guess, gHUD.GetSlotItem_guess( m_iSelected_guess ), 128 );
			gEngfuncs.pfnPlaySoundByName( "inventory/inventory_click.wav", 1 );
			sprintf( szCommand, "quickselset2 %i\n", m_iSelected_guess );
			gEngfuncs.pfnServerCmd( szCommand );
			m_pMenuImage_guess->setVisible( false );
			for ( i = 0; i < 7; i++ )
				m_pMenuButtons_guess[i]->setVisible( false );
		}
		else
			gEngfuncs.pfnPlaySoundByName( "inventory/inventory_no.wav", 1 );

		m_iAction_guess = 0;
		break;
	case 56:
		if ( strlen( gHUD.GetSlotItem_guess( m_iSelected_guess ) ) && strstr( gHUD.GetSlotItem_guess( m_iSelected_guess ), "weapon_" ) )
		{
			m_pItemImages_guess[9]->setImage( vgui_LoadTGA( gHUD.CheckItem( "inventory_picture", gHUD.GetSlotItem_guess( m_iSelected_guess ) ) ) );
			strncpy( gHUD.m_szInventoryWeapon3_guess, gHUD.GetSlotItem_guess( m_iSelected_guess ), 128 );
			gEngfuncs.pfnPlaySoundByName( "inventory/inventory_click.wav", 1 );
			sprintf( szCommand, "quickselset3 %i\n", m_iSelected_guess );
			gEngfuncs.pfnServerCmd( szCommand );
			m_pMenuImage_guess->setVisible( false );
			for ( i = 0; i < 7; i++ )
				m_pMenuButtons_guess[i]->setVisible( false );
		}
		else
			gEngfuncs.pfnPlaySoundByName( "inventory/inventory_no.wav", 1 );

		m_iAction_guess = 0;
		break;
	default:
		gEngfuncs.Con_DPrintf( "Unknown Inventory ActionSignal %i.\n", cmd );
		break;
	}
}

void CRECBInventory::ClearItems_guess()
{
	for ( int i = 0; i < 10; i++ )
		m_pItemImages_guess[i]->setImage( NULL );

	m_iAction_guess = 0;
	m_iActionItem2_guess = 0;
	m_iActionItem1_guess = 0;
	m_iHoverItem_guess = 0;
}

void CRECBInventory::DoAction_guess( int slot )
{
	char szCommand[256];
	char szCmd[32];

	if ( m_iAction_guess <= 0 )
		return;

	const char *pszLevel = gEngfuncs.pfnGetLevelName();

	switch ( m_iAction_guess )
	{
	case 1:
		sprintf( szCmd, "inventoryequip %i\n", slot );
		gEngfuncs.pfnServerCmd( szCmd );
		break;
	case 2:
		if ( !m_iActionItem1_guess || !m_iActionItem2_guess )
			return;

		if ( m_iActionItem1_guess == m_iActionItem2_guess )
		{
			gEngfuncs.pfnPlaySoundByName( "inventory/inventory_no.wav", 1 );
		}
		else
		{
			if ( strlen( gHUD.CheckItem( "combined_result", gHUD.GetSlotItem_guess( m_iActionItem1_guess ) ) ) <= 1 )
				break;

			sprintf( szCommand, "inventorycombine %i %i %s\n", m_iActionItem1_guess, m_iActionItem2_guess, gHUD.CheckItem( "combined_result", gHUD.GetSlotItem_guess( m_iActionItem1_guess ) ) );
			gEngfuncs.pfnServerCmd( szCommand );
		}

		m_iActionItem1_guess = 0;
		m_iActionItem2_guess = 0;
		break;
	case 3:
		sprintf( szCmd, "inventoryuse %i\n", slot );
		gEngfuncs.pfnServerCmd( szCmd );
		break;
	case 4:
		sprintf( szCmd, "inventorydrop %i\n", slot );
		gEngfuncs.pfnServerCmd( szCmd );
		break;
	case 5:
		if ( strlen( gHUD.GetSlotItem_guess( slot ) ) && strstr( gHUD.GetSlotItem_guess( slot ), "weapon_" ) )
		{
			m_pItemImages_guess[6]->setImage( vgui_LoadTGA( gHUD.CheckItem( "inventory_picture", gHUD.GetSlotItem_guess( slot ) ) ) );
			strncpy( gHUD.m_szInventoryWeapon1_guess, gHUD.GetSlotItem_guess( slot ), 128 );
			gEngfuncs.pfnPlaySoundByName( "inventory/inventory_click.wav", 1 );
			sprintf( szCmd, "quickselset1 %i\n", slot );
			gEngfuncs.pfnServerCmd( szCmd );
		}
		else
			gEngfuncs.pfnPlaySoundByName( "inventory/inventory_no.wav", 1 );
		break;
	case 6:
		if ( strlen( gHUD.GetSlotItem_guess( slot ) ) && strstr( gHUD.GetSlotItem_guess( slot ), "weapon_" ) )
		{
			m_pItemImages_guess[7]->setImage( vgui_LoadTGA( gHUD.CheckItem( "inventory_picture", gHUD.GetSlotItem_guess( slot ) ) ) );
			strncpy( gHUD.m_szInventoryWeapon2_guess, gHUD.GetSlotItem_guess( slot ), 128 );
			gEngfuncs.pfnPlaySoundByName( "inventory/inventory_click.wav", 1 );
			sprintf( szCmd, "quickselset2 %i\n", slot );
			gEngfuncs.pfnServerCmd( szCmd );
		}
		else
			gEngfuncs.pfnPlaySoundByName( "inventory/inventory_no.wav", 1 );
		break;
	case 7:
		if ( !strstr( pszLevel, "cof_" ) && m_iActionItem1_guess && m_iActionItem2_guess )
		{
			if ( m_iActionItem1_guess == m_iActionItem2_guess )
			{
				gEngfuncs.pfnPlaySoundByName( "inventory/inventory_no.wav", 1 );
				m_iActionItem2_guess = 0;
				m_iActionItem1_guess = 0;
				return;
			}

			if ( strlen( gHUD.CheckItem( "classname", gHUD.GetSlotItem_guess( m_iActionItem1_guess ) ) ) > 1 )
			{
				sprintf( szCommand, "inventorydualwield %i %i\n", m_iActionItem1_guess, m_iActionItem2_guess );
				gEngfuncs.pfnServerCmd( szCommand );
				m_iActionItem2_guess = 0;
				m_iActionItem1_guess = 0;
				return;
			}
		}
		return;
	case 8:
		if ( strlen( gHUD.GetSlotItem_guess( slot ) ) && strstr( gHUD.GetSlotItem_guess( slot ), "weapon_" ) )
		{
			m_pItemImages_guess[9]->setImage( vgui_LoadTGA( gHUD.CheckItem( "inventory_picture", gHUD.GetSlotItem_guess( slot ) ) ) );
			strncpy( gHUD.m_szInventoryWeapon3_guess, gHUD.GetSlotItem_guess( slot ), 128 );
			gEngfuncs.pfnPlaySoundByName( "inventory/inventory_click.wav", 1 );
			sprintf( szCmd, "quickselset3 %i\n", slot );
			gEngfuncs.pfnServerCmd( szCmd );
		}
		else
			gEngfuncs.pfnPlaySoundByName( "inventory/inventory_no.wav", 1 );
		break;
	default:
		return;
	}

	m_iAction_guess = 0;
}

void CRECBInventory::Initialize()
{
	if ( gViewPort && gViewPort->m_pRECBInventory_guess )
		gViewPort->m_pRECBInventory_guess->Update_guess();

	m_pItemImages_guess[6]->setImage( NULL );
	gHUD.m_szInventoryWeapon1_guess[0] = 0;
	if ( strlen( gHUD.CheckItem( "inventory_picture", gHUD.m_szInventoryWeapon1_guess ) ) > 2 )
		m_pItemImages_guess[6]->setImage( vgui_LoadTGA( gHUD.CheckItem( "inventory_picture", gHUD.m_szInventoryWeapon1_guess ) ) );

	m_pItemImages_guess[7]->setImage( NULL );
	gHUD.m_szInventoryWeapon2_guess[0] = 0;
	if ( strlen( gHUD.CheckItem( "inventory_picture", gHUD.m_szInventoryWeapon2_guess ) ) > 2 )
		m_pItemImages_guess[7]->setImage( vgui_LoadTGA( gHUD.CheckItem( "inventory_picture", gHUD.m_szInventoryWeapon2_guess ) ) );

	m_pItemImages_guess[9]->setImage( NULL );
	gHUD.m_szInventoryWeapon3_guess[0] = 0;
	if ( strlen( gHUD.CheckItem( "inventory_picture", gHUD.m_szInventoryWeapon3_guess ) ) > 2 )
		m_pItemImages_guess[9]->setImage( vgui_LoadTGA( gHUD.CheckItem( "inventory_picture", gHUD.m_szInventoryWeapon3_guess ) ) );
}

int CRECBInventory::KeyInput( int down, int keynum, const char *pszCurrentBinding )
{
	if ( pszCurrentBinding )
	{
		if ( !strcmp( pszCurrentBinding, "toggleconsole" ) )
			return 1;

		if ( !strcmp( pszCurrentBinding, "+inventory" ) && isVisible() )
		{
			gViewPort->HideTopMenu();
			gEngfuncs.pfnPlaySoundByName( "inventory/inventory_close.wav", 1 );
		}
	}

	return 0;
}

bool CRECBInventory::MouseOver_guess( int x, int y, int wide, int tall )
{
	int cx, cy;

	if ( !gViewPort )
		return false;

	getApp()->getCursorPos( cx, cy );

	if ( cx >= x && cx <= x + wide && cy >= y && cy <= y + wide )
		return true;

	return false;
}

int MsgInventChck_guess( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	if ( gViewPort && gViewPort->m_pRECBInventory_guess )
		gViewPort->m_pRECBInventory_guess->Update_guess();

	return 1;
}

int MsgQSlotClear_guess( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int iSlot = READ_BYTE();
	int iOn = READ_BYTE();

	if ( gViewPort->m_pRECBInventory_guess )
	{
		switch ( iSlot )
		{
		case 1:
			if ( !iOn )
			{
				gViewPort->m_pRECBInventory_guess->m_pItemImages_guess[6]->setImage( NULL );
				gHUD.m_szInventoryWeapon1_guess[0] = 0;
			}
			else
			{
				if ( strlen( gHUD.CheckItem( "inventory_picture", gHUD.m_szInventoryWeapon1_guess ) ) > 2 )
					gViewPort->m_pRECBInventory_guess->m_pItemImages_guess[6]->setImage( vgui_LoadTGA( gHUD.CheckItem( "inventory_picture", gHUD.m_szInventoryWeapon1_guess ) ) );
			}
			break;
		case 2:
			if ( !iOn )
			{
				gViewPort->m_pRECBInventory_guess->m_pItemImages_guess[7]->setImage( NULL );
				gHUD.m_szInventoryWeapon2_guess[0] = 0;
			}
			else
			{
				if ( strlen( gHUD.CheckItem( "inventory_picture", gHUD.m_szInventoryWeapon2_guess ) ) > 2 )
					gViewPort->m_pRECBInventory_guess->m_pItemImages_guess[7]->setImage( vgui_LoadTGA( gHUD.CheckItem( "inventory_picture", gHUD.m_szInventoryWeapon2_guess ) ) );
			}
			break;
		case 3:
			if ( !iOn )
			{
				gViewPort->m_pRECBInventory_guess->m_pItemImages_guess[9]->setImage( NULL );
				gHUD.m_szInventoryWeapon3_guess[0] = 0;
			}
			else
			{
				if ( strlen( gHUD.CheckItem( "inventory_picture", gHUD.m_szInventoryWeapon3_guess ) ) > 2 )
					gViewPort->m_pRECBInventory_guess->m_pItemImages_guess[9]->setImage( vgui_LoadTGA( gHUD.CheckItem( "inventory_picture", gHUD.m_szInventoryWeapon3_guess ) ) );
			}
			break;
		}
	}

	return 1;
}

int MsgQSlotUpdate_guess( const char *pszName, int iSize, void *pbuf )
{
	char szCommand[128];

	BEGIN_READ( pbuf, iSize );
	int iSlot = READ_BYTE();
	int iItem = READ_BYTE();

	if ( gViewPort->m_pRECBInventory_guess )
	{
		switch ( iSlot )
		{
		case 1:
			if ( strlen( gHUD.GetSlotItem_guess( iItem ) ) && strstr( gHUD.GetSlotItem_guess( iItem ), "weapon_" ) )
			{
				gViewPort->m_pRECBInventory_guess->m_pItemImages_guess[6]->setImage( vgui_LoadTGA( gHUD.CheckItem( "inventory_picture", gHUD.GetSlotItem_guess( iItem ) ) ) );
				strncpy( gHUD.m_szInventoryWeapon1_guess, gHUD.GetSlotItem_guess( iItem ), 128 );
				sprintf( szCommand, "quickselset1 %i\n", iItem );
				gEngfuncs.pfnServerCmd( szCommand );
			}
			break;
		case 2:
			if ( strlen( gHUD.GetSlotItem_guess( iItem ) ) && strstr( gHUD.GetSlotItem_guess( iItem ), "weapon_" ) )
			{
				gViewPort->m_pRECBInventory_guess->m_pItemImages_guess[7]->setImage( vgui_LoadTGA( gHUD.CheckItem( "inventory_picture", gHUD.GetSlotItem_guess( iItem ) ) ) );
				strncpy( gHUD.m_szInventoryWeapon2_guess, gHUD.GetSlotItem_guess( iItem ), 128 );
				sprintf( szCommand, "quickselset2 %i\n", iItem );
				gEngfuncs.pfnServerCmd( szCommand );
			}
			break;
		case 3:
			if ( strlen( gHUD.GetSlotItem_guess( iItem ) ) && strstr( gHUD.GetSlotItem_guess( iItem ), "weapon_" ) )
			{
				gViewPort->m_pRECBInventory_guess->m_pItemImages_guess[9]->setImage( vgui_LoadTGA( gHUD.CheckItem( "inventory_picture", gHUD.GetSlotItem_guess( iItem ) ) ) );
				strncpy( gHUD.m_szInventoryWeapon3_guess, gHUD.GetSlotItem_guess( iItem ), 128 );
				sprintf( szCommand, "quickselset3 %i\n", iItem );
				gEngfuncs.pfnServerCmd( szCommand );
			}
			break;
		}
	}

	return 1;
}

void CRECBInventory::SelectItem_guess( int slot )
{
	char szCommand[256];
	char szCmd[32];

	if ( m_iDualWield_guess )
	{
		if ( m_iDualWield_guess != slot )
		{
			if ( strlen( gHUD.CheckItem( "classname", gHUD.GetSlotItem_guess( slot ) ) ) > 1 )
			{
				sprintf( szCmd, "inventorydualwield %i %i\n", m_iDualWield_guess, slot );
				gEngfuncs.pfnServerCmd( szCmd );
				m_iDualWield_guess = 0;
				g_bInventoryFadeOut_guess = false;
				m_pSelection_guess->setVisible( false );
				return;
			}

			m_iDualWield_guess = 0;
			g_bInventoryFadeOut_guess = false;
			m_pSelection_guess->setVisible( false );
			gEngfuncs.pfnPlaySoundByName( "inventory/inventory_no.wav", 1 );
			return;
		}
	}
	else
	{
		if ( m_iCombine_guess )
		{
			if ( m_iCombine_guess != slot )
			{
				if ( strlen( gHUD.CheckItem( "combined_result", gHUD.GetSlotItem_guess( slot ) ) ) > 1 )
				{
					sprintf( szCommand, "inventorycombine %i %i %s\n", m_iCombine_guess, slot, gHUD.CheckItem( "combined_result", gHUD.GetSlotItem_guess( m_iCombine_guess ) ) );
					gEngfuncs.pfnServerCmd( szCommand );
					m_iDualWield_guess = 0;
					g_bInventoryFadeOut_guess = false;
					m_pSelection_guess->setVisible( false );
					return;
				}

				m_iCombine_guess = 0;
				g_bInventoryFadeOut_guess = false;
				m_pSelection_guess->setVisible( false );
				gEngfuncs.pfnPlaySoundByName( "inventory/inventory_no.wav", 1 );
				return;
			}
		}
		else
		{
			if ( strlen( gHUD.GetSlotItem_guess( slot ) ) > 1 )
			{
				int x, y, wide, tall;
				bool bItem = false;

				m_iSelected_guess = slot;
				gEngfuncs.pfnPlaySoundByName( "inventory/inventory_toggle.wav", 1 );

				if ( !strstr( gHUD.GetSlotItem_guess( m_iSelected_guess ), "weapon_" ) )
					bItem = true;

				m_pItemButtons_guess[m_iSelected_guess - 1]->getPos( x, y );
				m_pItemButtons_guess[m_iSelected_guess - 1]->getSize( wide, tall );
				x += wide;
				m_pMenuButtons_guess[0]->getSize( wide, tall );

				m_pMenuImage_guess->setVisible( true );
				m_pMenuImage_guess->setPos( x - 5, y - 5 );

				if ( bItem )
					m_pMenuImage_guess->setImage( vgui_LoadTGA( "gfx/vgui/inventory_items.tga" ) );
				else
					m_pMenuImage_guess->setImage( vgui_LoadTGA( "gfx/vgui/inventory_weapons.tga" ) );

				for ( int i = 1; i <= 7; i++ )
				{
					m_pMenuButtons_guess[i - 1]->setPos( x, y );

					if ( i >= 4 )
					{
						if ( !bItem )
							m_pMenuButtons_guess[i - 1]->setVisible( true );
						else
							m_pMenuButtons_guess[i - 1]->setVisible( false );
					}
					else
					{
						m_pMenuButtons_guess[i - 1]->setVisible( true );
					}

					if ( i <= 4 )
						y += tall;

					if ( i >= 5 )
						x += 42;
				}
			}
			return;
		}
	}

	gEngfuncs.pfnPlaySoundByName( "inventory/inventory_no.wav", 1 );
}

void CRECBInventory::SetAction_guess( int action )
{
	gEngfuncs.pfnPlaySoundByName( "inventory/inventory_toggle.wav", 1 );

	m_iAction_guess = action;
	m_iActionItem2_guess = 0;
	m_iActionItem1_guess = 0;
}

void CRECBInventory::Update_guess()
{
	const char *pszLevel = gEngfuncs.pfnGetLevelName();

	if ( strstr( pszLevel, "cof_" ) )
	{
		m_pBackground_guess->setVisible( true );
		m_pInventory_guess->setImage( vgui_LoadTGA( "gfx/vgui/640_inventory_p.tga" ) );

		m_pItemButtons_guess[0]->setPos( 36, 91 );
		m_pItemImages_guess[0]->setPos( 31, 86 );
		m_pItemButtons_guess[1]->setPos( 138, 91 );
		m_pItemImages_guess[1]->setPos( 133, 86 );
		m_pItemButtons_guess[2]->setPos( 240, 91 );
		m_pItemImages_guess[2]->setPos( 235, 86 );
		m_pItemButtons_guess[3]->setPos( 36, 213 );
		m_pItemImages_guess[3]->setPos( 31, 208 );
		m_pItemButtons_guess[4]->setPos( 138, 213 );
		m_pItemImages_guess[4]->setPos( 133, 208 );
		m_pItemButtons_guess[5]->setPos( 240, 213 );
		m_pItemImages_guess[5]->setPos( 235, 208 );
		m_pItemImages_guess[8]->setPos( 31, 329 );
		m_pQuickSlot1Button_guess->setPos( 403, 90 );
		m_pQuickSlot2Button_guess->setPos( 505, 90 );
		m_pQuickSlot3Button_guess->setPos( 607, 90 );
		m_pItemImages_guess[6]->setPos( 398, 85 );
		m_pItemImages_guess[7]->setPos( 500, 85 );
		m_pItemImages_guess[9]->setPos( 602, 85 );

		m_pOptionButtons_guess[0]->setSize( 363, 42 );
		m_pOptionButtons_guess[0]->setPos( 373, 243 );
		m_pOptionButtons_guess[1]->setSize( 363, 42 );
		m_pOptionButtons_guess[1]->setPos( 373, 285 );
		m_pOptionButtons_guess[2]->setSize( 363, 42 );
		m_pOptionButtons_guess[2]->setPos( 373, 327 );
		m_pOptionButtons_guess[3]->setVisible( false );
		m_pOptionButtons_guess[4]->setVisible( false );

		setBgColor( 255, 0, 0, 255 );
	}
	else
	{
		m_pBackground_guess->setVisible( true );

		if ( m_iThree_guess )
			m_pInventory_guess->setImage( vgui_LoadTGA( "gfx/vgui/640_inventory_three.tga" ) );
		else
			m_pInventory_guess->setImage( vgui_LoadTGA( "gfx/vgui/640_inventory.tga" ) );


		m_pItemButtons_guess[0]->setPos( 36, 73 );
		m_pItemImages_guess[0]->setPos( 31, 68 );
		m_pItemButtons_guess[1]->setPos( 138, 73 );
		m_pItemImages_guess[1]->setPos( 133, 68 );
		m_pItemButtons_guess[2]->setPos( 240, 73 );
		m_pItemImages_guess[2]->setPos( 235, 68 );
		m_pItemButtons_guess[3]->setPos( 36, 232 );
		m_pItemImages_guess[3]->setPos( 31, 227 );
		m_pItemButtons_guess[4]->setPos( 138, 232 );
		m_pItemImages_guess[4]->setPos( 133, 227 );
		m_pItemButtons_guess[5]->setPos( 240, 232 );
		m_pItemImages_guess[5]->setPos( 235, 227 );
		m_pItemImages_guess[8]->setPos( 31, 348 );
		m_pQuickSlot1Button_guess->setPos( 403, 73 );
		m_pQuickSlot2Button_guess->setPos( 505, 73 );
		m_pQuickSlot3Button_guess->setPos( 607, 73 );
		m_pItemImages_guess[6]->setPos( 398, 68 );
		m_pItemImages_guess[7]->setPos( 500, 68 );
		m_pItemImages_guess[9]->setPos( 602, 68 );

		m_pOptionButtons_guess[0]->setSize( 363, 47 );
		m_pOptionButtons_guess[0]->setPos( 373, 226 );
		m_pOptionButtons_guess[1]->setSize( 363, 47 );
		m_pOptionButtons_guess[1]->setPos( 373, 276 );
		m_pOptionButtons_guess[2]->setSize( 363, 47 );
		m_pOptionButtons_guess[2]->setPos( 373, 325 );
		m_pOptionButtons_guess[3]->setVisible( false );
		m_pOptionButtons_guess[4]->setVisible( true );

		setBgColor( 0, 0, 0, 255 );
	}

	m_pBackgroundPanel_guess->setBgColor( 0, 0, 0, 0 );
}

void VGUI_QSlotInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "QSlotClear", MsgQSlotClear_guess );
	gEngfuncs.pfnHookUserMsg( "QSlotUpdate", MsgQSlotUpdate_guess );
	gEngfuncs.pfnHookUserMsg( "InventChck", MsgInventChck_guess );
}

void CRECBInventory::setVisible( bool state )
{
	int i;

	for ( i = 0; i < 7; i++ )
		m_pMenuButtons_guess[i]->setVisible( false );

	m_pMenuImage_guess->setVisible( false );
	m_pSelection_guess->setVisible( false );

	m_iSelectionAlpha_guess = 0;
	m_iSelected_guess = 0;
	m_iCombine_guess = 0;
	m_iDualWield_guess = 0;
	m_iAction_guess = 0;
	m_iActionItem2_guess = 0;
	m_iActionItem1_guess = 0;
	m_iHoverItem_guess = 0;

	if ( state )
	{
		if ( m_pItemImages_guess[8] )
			m_pItemImages_guess[8]->setImage( NULL );

		if ( m_pPopupImage_guess )
			m_pPopupImage_guess->setImage( NULL );

		m_bUnknownE0_guess = false;
		m_iUnknownE4_guess = 0;

		if ( strlen( gHUD.m_szObjectiveTitle_guess ) > 2 && strlen( gHUD.m_szObjectiveText_guess ) > 2 )
		{
			ParseHintTokens( gHUD.m_szObjectiveTitle_guess, gHUD.m_szObjectiveText_guess );

			if ( strlen( gViewPort->m_pHUDControl_guess->m_szHintLine1_guess ) > 2 )
			{
				char szText[256];
				unsigned int j;

				for ( i = 0, j = 0; j < strlen( gViewPort->m_pHUDControl_guess->m_szHintLine1_guess ); j++ )
				{
					i++;

					if ( gViewPort->m_pHUDControl_guess->m_szHintLine1_guess[j] == '@' )
						szText[j] = '\n';
					else
						szText[j] = gViewPort->m_pHUDControl_guess->m_szHintLine1_guess[j];
				}

				szText[i] = 0;
				m_pObjectiveLabel_guess->setText( szText );
			}

			m_pObjectiveLabel_guess->setVisible( true );
		}
		else
			m_pObjectiveLabel_guess->setVisible( false );
	}
	else
	{
		if ( m_pPopupImage_guess )
			m_pPopupImage_guess->setImage( NULL );

		m_bUnknownE0_guess = false;
		m_iUnknownE4_guess = 0;

		if ( gEngfuncs.pfnGetLevelName() )
		{
			for ( i = 0; i < 5; i++ )
			{
				if ( m_pOptionButtons_guess[i] && m_pOptionButtons_guess[i]->isVisible() )
					m_pOptionButtons_guess[i]->cursorExited();
			}

			for ( i = 0; i < 6; i++ )
			{
				if ( m_pItemButtons_guess[i] && m_pItemButtons_guess[i]->isVisible() )
					m_pItemButtons_guess[i]->cursorExited();
			}

			m_pQuickSlot1Button_guess->cursorExited();
			m_pQuickSlot2Button_guess->cursorExited();
			m_pQuickSlot3Button_guess->cursorExited();
		}

		m_pItemImages_guess[8]->setImage( NULL );
	}

	Panel::setVisible( state );
}

void CRECBInventory::solve()
{
	int i;

	if ( isVisible() )
	{
		if ( m_pSelection_guess->isVisible() )
		{
			if ( g_bInventoryFadeOut_guess )
			{
				if ( m_iSelectionAlpha_guess > 0 )
					m_iSelectionAlpha_guess -= 15;

				if ( m_iSelectionAlpha_guess <= 0 )
				{
					g_bInventoryFadeOut_guess = false;
					m_iSelectionAlpha_guess = 0;
				}
			}
			else
			{
				if ( m_iSelectionAlpha_guess < 255 )
					m_iSelectionAlpha_guess += 15;

				if ( m_iSelectionAlpha_guess >= 255 )
				{
					g_bInventoryFadeOut_guess = true;
					m_iSelectionAlpha_guess = 255;
				}
			}

			m_pSelection_guess->setBorder( new LineBorder( Color( 255, 0, 0, m_iSelectionAlpha_guess ) ) );
		}

		if ( iWheelchair_guess && isVisible() )
		{
			gViewPort->HideTopMenu();
			return;
		}

		if ( getApp()->isMouseDown( MOUSE_LEFT, NULL ) )
		{
			bool bArmed = false;

			for ( i = 0; i < 7; i++ )
			{
				if ( m_pMenuButtons_guess[i]->isArmed() )
					bArmed = true;
			}

			if ( !bArmed )
			{
				for ( i = 0; i < 7; i++ )
					m_pMenuButtons_guess[i]->setVisible( false );

				m_pMenuImage_guess->setVisible( false );
			}
		}

		if ( m_pPopupImage_guess->isVisible() )
			m_pPopupImage_guess->setVisible( false );

		for ( i = 1; i <= 6; i++ )
		{
			if ( m_pItemButtons_guess[i - 1]->isArmed() && strlen( gHUD.GetSlotItem_guess( i ) ) > 1 )
			{
				m_pItemImages_guess[8]->setImage( vgui_LoadTGA( gHUD.CheckItem( "check_picture", gHUD.GetSlotItem_guess( i ) ) ) );
				m_iHoverItem_guess = i;
			}
		}

		if ( m_pQuickSlot1Button_guess->isArmed() && m_iHoverItem_guess != 7 && strlen( gHUD.m_szInventoryWeapon1_guess ) > 1 )
		{
			m_iHoverItem_guess = 7;
			m_pItemImages_guess[8]->setImage( vgui_LoadTGA( gHUD.CheckItem( "check_picture", gHUD.m_szInventoryWeapon1_guess ) ) );
		}

		if ( m_pQuickSlot2Button_guess->isArmed() && m_iHoverItem_guess != 8 && strlen( gHUD.m_szInventoryWeapon2_guess ) > 1 )
		{
			m_iHoverItem_guess = 8;
			m_pItemImages_guess[8]->setImage( vgui_LoadTGA( gHUD.CheckItem( "check_picture", gHUD.m_szInventoryWeapon2_guess ) ) );
		}

		if ( m_pQuickSlot3Button_guess->isArmed() && m_iHoverItem_guess != 9 && strlen( gHUD.m_szInventoryWeapon3_guess ) > 1 )
		{
			m_iHoverItem_guess = 9;
			m_pItemImages_guess[8]->setImage( vgui_LoadTGA( gHUD.CheckItem( "check_picture", gHUD.m_szInventoryWeapon3_guess ) ) );
		}

		if ( m_iHoverItem_guess >= 1 && m_iHoverItem_guess <= 6 && !m_pItemButtons_guess[m_iHoverItem_guess - 1]->isArmed() )
		{
			m_pItemImages_guess[8]->setImage( NULL );
			m_iHoverItem_guess = 0;
		}

		if ( m_iHoverItem_guess == 7 && !m_pQuickSlot1Button_guess->isArmed() )
		{
			m_pItemImages_guess[8]->setImage( NULL );
			m_iHoverItem_guess = 0;
		}

		if ( m_iHoverItem_guess == 8 && !m_pQuickSlot2Button_guess->isArmed() )
		{
			m_pItemImages_guess[8]->setImage( NULL );
			m_iHoverItem_guess = 0;
		}

		if ( m_iHoverItem_guess == 9 && !m_pQuickSlot3Button_guess->isArmed() )
		{
			m_pItemImages_guess[8]->setImage( NULL );
			m_iHoverItem_guess = 0;
		}

		if ( m_pQuickSlot1Button_guess->isArmed() && m_iHoverItem_guess != 7 )
			m_iHoverItem_guess = 7;

		if ( m_pQuickSlot2Button_guess->isArmed() && m_iHoverItem_guess != 8 )
			m_iHoverItem_guess = 8;

		if ( m_pQuickSlot3Button_guess->isArmed() && m_iHoverItem_guess != 9 )
			m_iHoverItem_guess = 9;
	}

	Panel::solve();
}
