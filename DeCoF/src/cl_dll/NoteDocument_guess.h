#include "cofr_legacy.h"
// ======================================
// Cry of Fear note document panel header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _NOTEDOCUMENT_H
#define _NOTEDOCUMENT_H
using namespace vgui;

class CNoteDocument : public CMenuPanel
{
public:
	CNoteDocument( int iTrans, int iRemoveMe, int x, int y, int wide, int tall );
	void Initialize();
	void ActionSignal( int cmd );
	int KeyInput( int down, int keynum, const char *pszCurrentBinding );

	void ClearString_guess( char *str );
	int CountLines_guess();
	void NextLine_guess();
	void NextPage_guess();
	void PrevLine_guess();
	void PrevPage_guess();
	void SetPage_guess( int page );
	void SetText_guess();

	virtual void setVisible( bool state )
	{
		Panel::setVisible( state );
	}

protected:
	virtual void paintBackground()
	{
		bool blurred=CoFR_BlurBackground();
        if(m_pBackground_guess)m_pBackground_guess->setVisible(!blurred);
        if(!blurred)Panel::paintBackground();
	}

	virtual void paint();

public:
	CommandButton	*m_pCloseButton_guess;	// 0xd4, command 3
	CTransparentPanel	*m_pPanel_guess;	// 0xd8, 640x480 window
	CImageLabel	*m_pImageLabel_guess;		// 0xdc, picture of the page
	CImageLabel	*m_pCancelLabel_guess;		// 0xe0, notecancel
	int		m_iLine_guess;					// 0xe4, line of the text file shown in m_szLine_guess
	int		m_iPage_guess;					// 0xe8, page (1 to 4), picks the text file
	char	m_szImage1_guess[16];			// 0xec, picture of page 1, from the NoteDoc message
	char	m_szImage2_guess[16];			// 0xfc
	char	m_szImage3_guess[16];			// 0x10c
	char	m_szImage4_guess[16];			// 0x11c
	char	m_szSound_guess[48];			// 0x12c, played by SetPage_guess
	char	m_szText1_guess[16];			// 0x15c, text file of page 1 (under notes/)
	char	m_szText2_guess[16];			// 0x16c
	char	m_szText3_guess[16];			// 0x17c
	char	m_szText4_guess[16];			// 0x18c
	char	m_szLine_guess[128];			// 0x19c, current line of the text file
	bool	m_bTitle_guess;					// 0x21c, m_szLine_guess is the first line
	Label	*m_pLineLabel_guess;			// 0x220, shows m_szLine_guess
	Image	*m_pBackgroundImage_guess;		// 0x224, gfx/vgui/backgroundtoall.tga
	ImagePanel	*m_pBackground_guess;		// 0x228
	Font	*m_pFont_guess;					// 0x22c, "credits" scheme font
};

class CNoteDocumentHandler_Command : public ActionSignal
{
private:
	CNoteDocument	*m_pFather;
	int				m_cmd;

public:
	CNoteDocumentHandler_Command( CNoteDocument *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

void NoteDocInit_guess();

#endif // _NOTEDOCUMENT_H
