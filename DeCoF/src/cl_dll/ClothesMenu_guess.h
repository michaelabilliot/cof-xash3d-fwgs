// ======================================
// Cry of Fear clothes menu header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _CLOTHESMENU_H
#define _CLOTHESMENU_H
using namespace vgui;

#define MAX_CLOTHES_guess	13	// 12 are used

class CClothesMenu : public Panel
{
public:
	CClothesMenu();
	~CClothesMenu( void );
	void Initialize();
	void ActionSignal( int cmd );
	int KeyInput_guess( int down, int keynum, const char *pszCurrentBinding );

	void UpdateButtons_guess();

public:
	float	m_flNextClick_guess;			// 0xbc, ActionSignal ignores clicks before this time
	ColorButton	*m_pClothesButton_guess[MAX_CLOTHES_guess];	// 0xc0
	int		m_iUnlocked_guess;				// 0xf4, bit field of the unlocked clothes, from the Clothes message
};

class CClothesMenuHandler_Command : public ActionSignal
{
private:
	CClothesMenu	*m_pFather;
	int			m_cmd;

public:
	CClothesMenuHandler_Command( CClothesMenu *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

#endif // _CLOTHESMENU_H
