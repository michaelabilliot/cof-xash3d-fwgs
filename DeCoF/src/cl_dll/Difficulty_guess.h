// ======================================
// Cry of Fear difficulty selection panel header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _DIFFICULTY_H
#define _DIFFICULTY_H
using namespace vgui;

class CDifficulty : public Panel, public ICheckButton2Handler
{
public:
	CDifficulty();
	~CDifficulty();
	void Initialize();
	void ActionSignal( int cmd );
	int KeyInput( int down, int keynum, const char *pszCurrentBinding );

	virtual void setVisible( bool state );

	// ICheckButton2Handler
	virtual void StateChanged( CCheckButton2 *pButton );

public:
	Image	*m_pImage_guess;			// 0xc0, gfx/vgui/640_difficulty.tga
	ImagePanel	*m_pImagePanel_guess;	// 0xc4, parent of the buttons
	ColorButton	*m_pSkill4Button_guess;	// 0xc8, "skillset 4", shown with unlock 0x20
	CCheckButton2	m_CommentaryButton_guess;	// 0xcc, "Enable developer commentary"
};

class CDifficultyHandler_Command : public ActionSignal
{
private:
	CDifficulty	*m_pFather;
	int			m_cmd;

public:
	CDifficultyHandler_Command( CDifficulty *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

#endif // _DIFFICULTY_H
