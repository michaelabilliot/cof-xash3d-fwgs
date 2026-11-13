// ======================================
// Cry of Fear billboard panel header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _VGUIBILLBOARD_H
#define _VGUIBILLBOARD_H
using namespace vgui;

class CVGUIBillboard : public Panel
{
public:
	CVGUIBillboard();
	~CVGUIBillboard( void );
	void Initialize();
	void ActionSignal( int cmd );
	int KeyInput( int down, int keynum, const char *pszCurrentBinding );

	virtual void setVisible( bool state );

protected:
	virtual void paintBackground();
	virtual void paint();

public:
	Image	*m_pBackgroundImage_guess;	// 0xbc, gfx/vgui/backgroundtoall.tga
	ImagePanel	*m_pBackground_guess;	// 0xc0
	ImagePanel	*m_pImage_guess;		// 0xc4, the picture of the Board message
	Label	*m_pLabel_guess;			// 0xc8, repeats the message line of the HUD panel
	Font	*m_pFont_guess;				// 0xcc, scheme "credits"
};

class CBillboardHandler_Command : public ActionSignal
{
private:
	CVGUIBillboard	*m_pFather;
	int				m_cmd;

public:
	CBillboardHandler_Command( CVGUIBillboard *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

void VGUI_BoardInit_guess();

#endif // _VGUIBILLBOARD_H
