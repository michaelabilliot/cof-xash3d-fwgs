// ======================================
// Cry of Fear book menu header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _BOOKMENU_H
#define _BOOKMENU_H
using namespace vgui;

class CBookMenu : public Panel
{
public:
	CBookMenu();
	~CBookMenu();
	void Initialize();
	void ActionSignal( int cmd );

	virtual void setVisible( bool state );

public:
	Image	*m_pArrowsImage_guess;		// 0xbc, gfx/vgui/left_right.tga
	ImagePanel	*m_pArrows_guess;		// 0xc0
	Image	*m_pCancelImage_guess;		// 0xc4, gfx/vgui/640_notecancel.tga
	ImagePanel	*m_pCancel_guess;		// 0xc8
};

class CBookMenuHandler_Command : public ActionSignal
{
private:
	CBookMenu	*m_pFather;
	int			m_cmd;

public:
	CBookMenuHandler_Command( CBookMenu *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

#endif // _BOOKMENU_H
