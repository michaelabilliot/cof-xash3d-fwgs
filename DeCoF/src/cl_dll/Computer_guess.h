// ======================================
// Cry of Fear computer puzzle panel header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _COMPUTER_H
#define _COMPUTER_H
using namespace vgui;

class CComputer : public Panel
{
public:
	CComputer();
	~CComputer( void );
	void Initialize();
	void ActionSignal( int cmd );
	int KeyInput( int down, int keynum, const char *pszCurrentBinding );

	virtual void setVisible( bool state );

protected:
	virtual void solve();

public:
	float	m_flNextKeyTime_guess;		// 0xbc
	int		m_iLoggedOn_guess;			// 0xc0, from the Computer message
	int		m_iUserLength_guess;		// 0xc4, typing sound when the length changes
	int		m_iPasswordLength_guess;	// 0xc8
	Image	*m_pBackgroundImage_guess;	// 0xcc, gfx/vgui/backgroundtoall.tga
	ImagePanel	*m_pBackground_guess;	// 0xd0
	TextEntry	*m_pUser_guess;			// 0xd4, "Enter a username..."
	TextEntry	*m_pPassword_guess;		// 0xd8, "Enter a password..."
	Image	*m_pScreenImage_guess;		// 0xdc, gfx/vgui/puzzle/computer/computer_logon.tga
	ImagePanel	*m_pScreen_guess;		// 0xe0, parent of the buttons and the text entries
	TextPanel	*m_pText_guess;			// 0xe4, "Jill - %s! - David"
	int		m_iUnusedE8_guess[4];		// 0xe8, never used in 1.6a (sizeof 0xf8 from the new in the viewport)
};

class CComputerHandler_Command : public ActionSignal
{
private:
	CComputer	*m_pFather;
	int			m_cmd;

public:
	CComputerHandler_Command( CComputer *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

void VGUI_ComputerInit_guess();

#endif // _COMPUTER_H
