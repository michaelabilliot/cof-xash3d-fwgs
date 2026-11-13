// ======================================
// Cry of Fear padlock puzzle panel header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _PADLOCK_H
#define _PADLOCK_H
using namespace vgui;

class CPadlock : public Panel
{
public:
	CPadlock();
	~CPadlock( void );
	void Initialize();
	void ActionSignal( int cmd );

public:
	Image	*m_pButtonsImage_guess;		// 0xbc, gfx/vgui/puzzle/padlock_buttons.tga
	ImagePanel	*m_pButtons_guess;		// 0xc0, parent of the wheel buttons
};

class CPadlockHandler_Command : public ActionSignal
{
private:
	CPadlock	*m_pFather;
	int			m_cmd;

public:
	CPadlockHandler_Command( CPadlock *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

void PadlockInit_guess();

#endif // _PADLOCK_H
