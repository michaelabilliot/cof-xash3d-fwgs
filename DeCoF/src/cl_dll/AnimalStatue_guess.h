// ======================================
// Cry of Fear animal statue puzzle panel header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _ANIMALSTATUE_H
#define _ANIMALSTATUE_H
using namespace vgui;

class CAnimalStatue : public Panel
{
public:
	CAnimalStatue();
	~CAnimalStatue();
	void Initialize();
	void ActionSignal( int cmd );
	int KeyInput( int down, int keynum, const char *pszCurrentBinding );

public:
	Panel	*m_pBlackPanel_guess;		// 0xbc
	Image	*m_pBackgroundImage_guess;	// 0xc0, gfx/vgui/backgroundtoall.tga
	ImagePanel	*m_pBackground_guess;	// 0xc4
	int		m_iStatue_guess;			// 0xc8, from the Statue message, sent back in movestatue
	Image	*m_pButtonsImage_guess;		// 0xcc, gfx/vgui/640_eagle_button.tga
	ImagePanel	*m_pButtons_guess;		// 0xd0, parent of the buttons
};

class CAnimalStatueHandler_Command : public ActionSignal
{
private:
	CAnimalStatue	*m_pFather;
	int				m_cmd;

public:
	CAnimalStatueHandler_Command( CAnimalStatue *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

void StatueInit_guess();

#endif // _ANIMALSTATUE_H
