// ======================================
// Cry of Fear inventory panel header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _RECBINVENTORY_H
#define _RECBINVENTORY_H
using namespace vgui;

class CRECBInventory : public CMenuPanel
{
public:
	CRECBInventory( int iTrans, int iRemoveMe, int x, int y, int wide, int tall );
	void Initialize();
	void ActionSignal( int cmd );
	int KeyInput( int down, int keynum, const char *pszCurrentBinding );

	void ClearItems_guess();
	void DoAction_guess( int slot );
	bool MouseOver_guess( int x, int y, int wide, int tall );
	void SelectItem_guess( int slot );
	void SetAction_guess( int action );
	void Update_guess();

	virtual void setVisible( bool state );

protected:
	virtual void solve();

public:
	Image	*m_pBackgroundImage_guess;		// 0xd4, gfx/vgui/backgroundtoall.tga
	ImagePanel	*m_pBackground_guess;		// 0xd8
	CImageLabel	*m_pMenuImage_guess;		// 0xdc, inventory_weapons.tga or inventory_items.tga behind the item menu
	bool	m_bUnknownE0_guess;				// 0xe0, cleared by setVisible
	int		m_iUnknownE4_guess;				// 0xe4, cleared by setVisible
	int		m_iDualWield_guess;				// 0xe8, slot picked for inventorydualwield (menu command 53)
	int		m_iCombine_guess;				// 0xec, slot picked for inventorycombine (menu command 51)
	int		m_iSelected_guess;				// 0xf0, slot of the item menu (1 to 6)
	int		m_iSelectionAlpha_guess;		// 0xf4, alpha of the red border of m_pSelection_guess
	Panel	*m_pSelection_guess;			// 0xf8, red border around the slot picked for combine or dual wield
	int		m_iUnusedFC_guess;				// 0xfc, not referenced by any code
	ColorButton	*m_pMenuButtons_guess[7];	// 0x100, item menu, commands 50 to 56
	int		m_iUnused11C_guess;				// 0x11c, not referenced by any code
	CImageLabel	*m_pItemImages_guess[10];	// 0x120, pictures of the 6 items, the 3 quick slots ([6], [7], [9]) and the hovered item ([8])
	Panel	*m_pBackgroundPanel_guess;		// 0x148
	Image	*m_pInventoryImage_guess;		// 0x14c, gfx/vgui/640_inventory.tga
	ImagePanel	*m_pInventory_guess;		// 0x150, parent of the buttons and pictures
	CImageLabel	*m_pPopupImage_guess;		// 0x154
	int		m_iUnused158_guess;				// 0x158, not referenced by any code
	ColorButton	*m_pItemButtons_guess[6];	// 0x15c, item slots, commands 1 to 6
	int		m_iUnused174_guess[3];			// 0x174, not referenced by any code
	ColorButton	*m_pOptionButtons_guess[5];	// 0x180, commands 7, 8, 10, 15 and 16
	ColorButton	*m_pQuickSlot1Button_guess;	// 0x194, command 12
	ColorButton	*m_pQuickSlot2Button_guess;	// 0x198, command 13
	ColorButton	*m_pQuickSlot3Button_guess;	// 0x19c, command 14
	Label	*m_pObjectiveLabel_guess;		// 0x1a0, gHUD.m_szObjectiveText_guess
	int		m_iThree_guess;					// 0x1a4, picks gfx/vgui/640_inventory_three.tga
	int		m_iUnused1A8_guess[2];			// 0x1a8, not referenced by any code
	int		m_iAction_guess;				// 0x1b0, set by SetAction_guess, run by DoAction_guess
	int		m_iUnused1B4_guess;				// 0x1b4, not referenced by any code
	int		m_iActionItem1_guess;			// 0x1b8, first slot of the combine and dual wield actions
	int		m_iActionItem2_guess;			// 0x1bc, second slot
	int		m_iHoverItem_guess;				// 0x1c0, slot shown in m_pItemImages_guess[8] (1 to 6, 7 to 9 quick slots)
};

class CRECBInventoryHandler_Command : public ActionSignal
{
private:
	CRECBInventory	*m_pFather;
	int				m_cmd;

public:
	CRECBInventoryHandler_Command( CRECBInventory *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

void VGUI_QSlotInit_guess();

#endif // _RECBINVENTORY_H
