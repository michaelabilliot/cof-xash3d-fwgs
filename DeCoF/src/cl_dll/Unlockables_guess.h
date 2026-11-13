// ======================================
// Cry of Fear unlockables menu header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _UNLOCKABLES_H
#define _UNLOCKABLES_H
using namespace vgui;

#define MAX_UNLOCKABLES_guess	22	// 21 are used

class CUnlockables : public Panel
{
public:
	CUnlockables();
	~CUnlockables();
	void Initialize();
	void ActionSignal( int cmd );
	int KeyInput_guess( int down, int keynum, const char *pszCurrentBinding );

	virtual void setVisible( bool state );

protected:
	virtual void solve();

public:
	int		m_iUnknownBC_guess;				// 0xbc, never used in 1.6a
	int		m_iArmed_guess;					// 0xc0, 1 + index of the item under the cursor, 0 for none
	float	m_flNextClick_guess;			// 0xc4, ActionSignal ignores clicks before this time; never written in 1.6a
	ColorButton	*m_pItemButton_guess[MAX_UNLOCKABLES_guess];	// 0xc8
	Label	*m_pTextLabel_guess;			// 0x120, description of the item under the cursor
	ColorButton	*m_pBackButton_guess;		// 0x124
	Image	*m_pLockedImage_guess[MAX_UNLOCKABLES_guess];	// 0x128, gfx/vgui/unlockables/unlockables_locked.tga
	ImagePanel	*m_pLockedPanel_guess[MAX_UNLOCKABLES_guess];	// 0x180, hidden once the item is unlocked
	Image	*m_pMenuImage_guess;			// 0x1d8, gfx/vgui/unlockables/unlockables_menu.tga
	ImagePanel	*m_pMenu_guess;				// 0x1dc
};

class CUnlockablesHandler_Command : public ActionSignal
{
private:
	CUnlockables	*m_pFather;
	int			m_cmd;

public:
	CUnlockablesHandler_Command( CUnlockables *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

#endif // _UNLOCKABLES_H
