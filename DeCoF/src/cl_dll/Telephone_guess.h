// ======================================
// Cry of Fear telephone puzzle panel header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _TELEPHONE_H
#define _TELEPHONE_H
using namespace vgui;

class CTelephone : public Panel
{
public:
	CTelephone();
	~CTelephone( void );
	void Initialize();
	void ActionSignal( int cmd );

	void AddDigit_guess( int digit );
	void Dial_guess();

	virtual void setVisible( bool state );

protected:
	virtual void paint();

public:
	Image	*m_pBackgroundImage_guess;		// 0xbc, gfx/vgui/backgroundtoall.tga
	ImagePanel	*m_pBackground_guess;		// 0xc0
	bool	m_bLocked_guess;				// 0xc4, set by the Telephone message (3), blocks the buttons
	float	m_flFade_guess;					// 0xc8, alpha of the telephone picture, 255 when shown
	char	m_szNumber_guess[16];			// 0xcc, number to dial, from the Telephone message
	char	m_szDialed_guess[16];			// 0xdc, digits dialed so far
	Image	*m_pTelephoneImage_guess;		// 0xec, gfx/vgui/puzzle/telephone_vgui.tga
	ImagePanel	*m_pTelephone_guess;		// 0xf0, parent of the buttons
};

class CTelephoneHandler_Command : public ActionSignal
{
private:
	CTelephone	*m_pFather;
	int			m_cmd;

public:
	CTelephoneHandler_Command( CTelephone *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

#endif // _TELEPHONE_H
