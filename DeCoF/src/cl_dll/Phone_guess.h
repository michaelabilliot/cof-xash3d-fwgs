// ======================================
// Cry of Fear mobile phone panel header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _PHONE_H
#define _PHONE_H
using namespace vgui;

class CPhone : public Panel
{
public:
	CPhone();
	~CPhone( void );
	void Initialize();
	void ActionSignal( int cmd );
	int KeyInput( int down, int keynum, const char *pszCurrentBinding );

	void AddDigit_guess( int digit );
	void Clear_guess();
	void PhoneDial_guess();

	virtual void setVisible( bool state )
	{
		Panel::setVisible( state );
	}

public:
	Image	*m_pPhoneImage_guess;			// 0xbc, gfx/vgui/640_phone.tga
	ImagePanel	*m_pPhone_guess;			// 0xc0, parent of the buttons and the number label
	Image	*m_pBackgroundImage_guess;		// 0xc4, gfx/vgui/backgroundtoall.tga
	ImagePanel	*m_pBackground_guess;		// 0xc8
	char	m_szNumber_guess[16];			// 0xcc, digits dialed so far, sent with phonenumber
	int		m_iUnknownDC_guess;				// 0xdc, not used by the client code
	Label	*m_pNumberLabel_guess;			// 0xe0, shows m_szNumber_guess
};

class CPhoneHandler_Command : public ActionSignal
{
private:
	CPhone	*m_pFather;
	int		m_cmd;

public:
	CPhoneHandler_Command( CPhone *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

void PhoneInit_guess();

#endif // _PHONE_H
