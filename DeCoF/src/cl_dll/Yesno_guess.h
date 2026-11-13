// ======================================
// Cry of Fear yes/no question panel header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _YESNO_H
#define _YESNO_H
using namespace vgui;

class CYesno : public Panel
{
public:
	CYesno();
	~CYesno( void );
	void Initialize();
	void ActionSignal( int cmd );

public:
	int		m_iActivate_guess;			// 0xbc, sent with yesnoactivate and yesnono
	Image	*m_pImage_guess;			// 0xc0, gfx/vgui/640_savegame.tga
	ImagePanel	*m_pImagePanel_guess;	// 0xc4, gets the image of the YesNo message
	CommandButton	*m_pYesButton_guess;	// 0xc8
	CommandButton	*m_pNoButton_guess;		// 0xcc
};

class CYesnoHandler_Command : public ActionSignal
{
private:
	CYesno	*m_pFather;
	int		m_cmd;

public:
	CYesnoHandler_Command( CYesno *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

void YesNoInit_guess();

#endif // _YESNO_H
