// ======================================
// Cry of Fear ammo inventory panel header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _AMMOINVENTORY_H
#define _AMMOINVENTORY_H
using namespace vgui;

#define MAX_AMMO_SLOTS_guess	6

// the slot arrays are indexed by the slot number, 1 to MAX_AMMO_SLOTS_guess
// (the viewport never creates this panel in 1.6a, so its size is unknown)
class CAmmoInventory : public Panel
{
public:
	CAmmoInventory();
	~CAmmoInventory( void );
	void Initialize();
	void ActionSignal( int cmd );
	int KeyInput( int down, int keynum, const char *pszCurrentBinding );

	virtual void setVisible( bool state );

protected:
	virtual void solve();

public:
	Image	*m_pBackgroundImage_guess;					// 0xbc, gfx/vgui/backgroundtoall.tga
	ImagePanel	*m_pBackground_guess;					// 0xc0
	int		m_iDropMode_guess;							// 0xc4, set by button 7, the slot buttons then drop their ammo
	int		m_iSelected_guess;							// 0xc8, slot under the cursor
	Label	*m_pCountLabel_guess[MAX_AMMO_SLOTS_guess + 1];	// 0xcc, ammo count of the AmmoInv message
	int		m_iAmmoType_guess[MAX_AMMO_SLOTS_guess + 1];		// 0xe8, 1 glock, 2 m16, 3 p345, 4 shotgun, 5 tmp
	CImageLabel	*m_pSlotImage_guess[MAX_AMMO_SLOTS_guess + 1];	// 0x104, gfx/vgui/ammo/*_slot.tga
	CImageLabel	*m_pSelectedImage_guess;				// 0x120, gfx/vgui/ammo/*_check.tga of the selected slot
	ColorButton	*m_pButton_guess[MAX_AMMO_SLOTS_guess + 1];	// 0x124
	ColorButton	*m_pDropButton_guess;					// 0x140
	Image	*m_pInventoryImage_guess;					// 0x144, gfx/vgui/ammo/ammo_inventory.tga
	ImagePanel	*m_pInventory_guess;					// 0x148, parent of the slots
};

class CAmmoInventoryHandler_Command : public ActionSignal
{
private:
	CAmmoInventory	*m_pFather;
	int				m_cmd;

public:
	CAmmoInventoryHandler_Command( CAmmoInventory *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

void VGUI_AmmoInvInit_guess();

#endif // _AMMOINVENTORY_H
