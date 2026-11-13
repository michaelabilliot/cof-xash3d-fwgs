// ======================================
// Cry of Fear game over panel header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _GAMEOVER_H
#define _GAMEOVER_H
using namespace vgui;

class CGameOver : public Panel
{
public:
	CGameOver();
	~CGameOver();
	void Initialize();
	void ActionSignal( int cmd );

	virtual void setVisible( bool state );

protected:
	virtual void solve();

public:
	Image	*m_pImage_guess;			// 0xbc, gfx/vgui/game_over.tga
	ImagePanel	*m_pPanel_guess;		// 0xc0, parent of the buttons
	Image	*m_pLoadImage_guess;		// 0xc4, gfx/vgui/game_over_load.tga
	ImagePanel	*m_pLoad_guess;			// 0xc8
	Image	*m_pExitImage_guess;		// 0xcc, gfx/vgui/game_over_exit.tga
	ImagePanel	*m_pExit_guess;			// 0xd0
	ColorButton	*m_pLoadButton_guess;	// 0xd4
	ColorButton	*m_pExitButton_guess;	// 0xd8
	char	m_szCustomDoc_guess[256];	// 0xdc, map of the CustomDoc message, restarted in doctor mode
	float	m_flAlpha_guess;			// 0x1dc, fades the pictures in
	bool	m_bLoadArmed_guess;			// 0x1e0
	bool	m_bExitArmed_guess;			// 0x1e1
};

class CGameOverHandler_Command : public ActionSignal
{
private:
	CGameOver	*m_pFather;
	int			m_cmd;

public:
	CGameOverHandler_Command( CGameOver *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

void VGUI_CustomDocInit_guess();

#endif // _GAMEOVER_H
