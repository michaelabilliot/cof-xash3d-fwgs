// ======================================
// Cry of Fear documents panel header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _DOCUMENTS_H
#define _DOCUMENTS_H
using namespace vgui;

#define MAX_DOCUMENTS_guess	16

// all arrays are indexed by the document number, 1 to MAX_DOCUMENTS_guess
class CDocuments : public Panel
{
public:
	CDocuments();
	~CDocuments();
	void Initialize();
	void ActionSignal( int cmd );
	int KeyInput( int down, int keynum, const char *pszCurrentBinding );

	virtual void setVisible( bool state );

public:
	int		m_iHidden_guess[MAX_DOCUMENTS_guess + 1];				// 0xbc, AddNote message, a hidden document cannot be recalled
	CommandButton	*m_pButton_guess[MAX_DOCUMENTS_guess + 1];	// 0x100
	Image	*m_pDocumentsImage_guess;						// 0x144, gfx/vgui/640_documents.tga
	ImagePanel	*m_pDocuments_guess;						// 0x148, parent of the buttons
	Image	*m_pBackgroundImage_guess;						// 0x14c, gfx/vgui/backgroundtoall.tga
	ImagePanel	*m_pBackground_guess;						// 0x150
	ImagePanel	*m_pCodeDigit1_guess[MAX_DOCUMENTS_guess + 1];	// 0x154, gfx/vgui/notes/code_%i.tga set by the AddNote message
	ImagePanel	*m_pCodeDigit2_guess[MAX_DOCUMENTS_guess + 1];	// 0x198
	ImagePanel	*m_pCodeDigit3_guess[MAX_DOCUMENTS_guess + 1];	// 0x1dc
	ImagePanel	*m_pCodeDigit4_guess[MAX_DOCUMENTS_guess + 1];	// 0x220
	Image	*m_pHiddenNoteImage_guess[MAX_DOCUMENTS_guess + 1];	// 0x264, gfx/vgui/notes/hiddennote1.tga
	ImagePanel	*m_pNote_guess[MAX_DOCUMENTS_guess + 1];			// 0x2a8
};

class CDocumentsHandler_Command : public ActionSignal
{
private:
	CDocuments	*m_pFather;
	int			m_cmd;

public:
	CDocumentsHandler_Command( CDocuments *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

#endif // _DOCUMENTS_H
