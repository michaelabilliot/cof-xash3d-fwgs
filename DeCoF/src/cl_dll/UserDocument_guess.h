// ======================================
// Cry of Fear computer user and password document panel header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _USERDOCUMENT_H
#define _USERDOCUMENT_H
using namespace vgui;

class CUserDocument : public CMenuPanel
{
public:
	CUserDocument( int iTrans, int iRemoveMe, int x, int y, int wide, int tall );
	void Initialize();
	void ActionSignal( int cmd );
	int KeyInput( int down, int keynum, const char *pszCurrentBinding );

	const char *GetPassword_guess( int index );
	const char *GetUsername_guess( int index );

	virtual void setVisible( bool state )
	{
		Panel::setVisible( state );
	}

protected:
	virtual void paintBackground()
	{
		Panel::paintBackground();
	}

	virtual void paint();

public:
	CommandButton	*m_pCloseButton_guess;	// 0xd4, command 1
	CImageLabel	*m_pCancelLabel_guess;		// 0xd8, notecancel
	Font	*m_pFont_guess;					// 0xdc, "credits" scheme font
	Image	*m_pDocumentImage_guess;		// 0xe0, gfx/vgui/640_pass1.tga
	ImagePanel	*m_pDocument_guess;			// 0xe4, gets 640_user%i.tga or 640_pass%i.tga
	Image	*m_pBackgroundImage_guess;		// 0xe8, gfx/vgui/backgroundtoall.tga
	ImagePanel	*m_pBackground_guess;		// 0xec
	int		m_iType_guess;					// 0xf0, first byte of the UserDoc message (1 user name, 2 password)
	int		m_iIndex_guess;					// 0xf4, second byte of the UserDoc message (1 to 8)
};

class CUserDocumentHandler_Command : public ActionSignal
{
private:
	CUserDocument	*m_pFather;
	int				m_cmd;

public:
	CUserDocumentHandler_Command( CUserDocument *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

void UserDocInit_guess();

#endif // _USERDOCUMENT_H
