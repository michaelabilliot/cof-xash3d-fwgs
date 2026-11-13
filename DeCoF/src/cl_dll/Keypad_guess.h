// ======================================
// Cry of Fear keypad puzzle panel header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _KEYPAD_H
#define _KEYPAD_H
using namespace vgui;

class CKeypad : public Panel
{
public:
	CKeypad();
	~CKeypad();
	void Initialize();
	void ActionSignal( int cmd );
	int KeyInput( int down, int keynum, const char *pszCurrentBinding );

	void Activate_guess();
	void EnterKey_guess( int key );
	void Submit_guess();
	void UpdateLayout_guess();

protected:
	virtual void paintBackground()
	{
		Panel::paintBackground();
	}

public:
	Panel	*m_pBackgroundPanel_guess;		// 0xbc
	Image	*m_pBackgroundImage_guess;		// 0xc0, gfx/vgui/backgroundtoall.tga
	ImagePanel	*m_pBackground_guess;		// 0xc4
	int		m_iActivated_guess;				// 0xc8, second byte of the Keypad message, blocks the keys
	int		m_iType_guess;					// 0xcc, first byte of the Keypad message, 0 to 2 (panel picture)
	char	m_szCode_guess[8];				// 0xd0, digits typed so far, sent with keypadnumber
	Font	*m_pFont_guess;					// 0xd8, "credits" scheme font
	Label	*m_pCodeLabel_guess;			// 0xdc, shows one star per typed digit (type 2)
	CommandButton	*m_pKey1_guess;			// 0xe0
	CommandButton	*m_pKey2_guess;			// 0xe4
	CommandButton	*m_pKey3_guess;			// 0xe8
	CommandButton	*m_pKey4_guess;			// 0xec
	CommandButton	*m_pKey5_guess;			// 0xf0
	CommandButton	*m_pKey6_guess;			// 0xf4
	CommandButton	*m_pKey7_guess;			// 0xf8
	CommandButton	*m_pKey8_guess;			// 0xfc
	CommandButton	*m_pKey9_guess;			// 0x100
	Image	*m_pPanelBackImage_guess;		// 0x104, gfx/vgui/640_panelback.tga
	ImagePanel	*m_pPanelBack_guess;		// 0x108, parent of the keys
	Image	*m_pPanel2BackImage_guess;		// 0x10c, gfx/vgui/640_panel2back.tga
	ImagePanel	*m_pPanel2Back_guess;		// 0x110
	Image	*m_pPanelOffImage_guess;		// 0x114, gfx/vgui/640_paneloff.tga
	ImagePanel	*m_pPanelOff_guess;			// 0x118
	Image	*m_pPanelOnImage_guess;			// 0x11c, gfx/vgui/640_panelon.tga
	ImagePanel	*m_pPanelOn_guess;			// 0x120
};

class CKeypadHandler_Command : public ActionSignal
{
private:
	CKeypad	*m_pFather;
	int		m_cmd;

public:
	CKeypadHandler_Command( CKeypad *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

void VGUI_KeypadInit_guess();

#endif // _KEYPAD_H
