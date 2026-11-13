// ======================================
// Cry of Fear save and load slots panel header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _SAVELOAD_H
#define _SAVELOAD_H
using namespace vgui;

class CSaveLoad : public Panel
{
public:
	CSaveLoad();
	~CSaveLoad( void );
	void Initialize();
	void ActionSignal( int cmd );

	void ReadAllSlots_guess();
	void ReadSlot_guess( int slot );
	void WriteSaveInfo_guess( const char *pszFile );

protected:
	virtual void solve()
	{
		Panel::solve();
	}

public:
	Label	*m_pSlot1Label_guess;			// 0xbc, text of saveinfo1.cof
	Label	*m_pSlot2Label_guess;			// 0xc0, text of saveinfo2.cof
	Label	*m_pSlot3Label_guess;			// 0xc4, text of saveinfo3.cof
	Label	*m_pSlot4Label_guess;			// 0xc8, text of saveinfo4.cof
	Label	*m_pSlot5Label_guess;			// 0xcc, text of saveinfo5.cof
	ColorButton	*m_pSlot1Button_guess;		// 0xd0
	ColorButton	*m_pSlot2Button_guess;		// 0xd4
	ColorButton	*m_pSlot3Button_guess;		// 0xd8
	ColorButton	*m_pSlot4Button_guess;		// 0xdc
	ColorButton	*m_pSlot5Button_guess;		// 0xe0
	float	m_flSlot1ClickTime_guess;		// 0xe4, a second click before this time picks the slot
	float	m_flSlot2ClickTime_guess;		// 0xe8
	float	m_flSlot3ClickTime_guess;		// 0xec
	float	m_flSlot4ClickTime_guess;		// 0xf0
	float	m_flSlot5ClickTime_guess;		// 0xf4
	char	m_szSaveName_guess[128];		// 0xf8, from the SaveLoad message, written to the save info
	int		m_iMode_guess;					// 0x178, 0 saves, else loads
	Image	*m_pBackgroundImage_guess;		// 0x17c, gfx/vgui/save_load.tga
	ImagePanel	*m_pBackground_guess;		// 0x180
};

class CSaveLoadHandler_Command : public ActionSignal
{
private:
	CSaveLoad	*m_pFather;
	int			m_cmd;

public:
	CSaveLoadHandler_Command( CSaveLoad *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

#endif // _SAVELOAD_H
