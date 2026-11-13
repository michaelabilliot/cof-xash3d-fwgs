// ======================================
// Cry of Fear extras menu header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _VGUIEXTRAMENU_H
#define _VGUIEXTRAMENU_H
using namespace vgui;

class CVGUIExtraMenu : public Panel
{
public:
	CVGUIExtraMenu();
	~CVGUIExtraMenu();
	void Initialize();
	void ActionSignal( int cmd );

	virtual void setVisible( bool state );

protected:
	virtual void solve();

public:
	Image	*m_pDonateImage_guess;		// 0xbc, gfx/vgui/menu/donate.tga
	ImagePanel	*m_pDonate_guess;		// 0xc0
	Image	*m_pSoundtrack1Image_guess;		// 0xc4, gfx/vgui/menu/soundtrack1.tga
	ImagePanel	*m_pSoundtrack1_guess;		// 0xc8
	Image	*m_pSoundtrack2Image_guess;		// 0xcc, gfx/vgui/menu/soundtrack2.tga
	ImagePanel	*m_pSoundtrack2_guess;		// 0xd0
	Image	*m_pClothesImage_guess;		// 0xd4, gfx/vgui/menu/clothes.tga
	ImagePanel	*m_pClothes_guess;		// 0xd8
	Image	*m_pAoMDCImage_guess;		// 0xdc, gfx/vgui/menu/aomdc.tga
	ImagePanel	*m_pAoMDC_guess;		// 0xe0
	Image	*m_pFacebookImage_guess;		// 0xe4, gfx/vgui/menu/facebook.tga
	ImagePanel	*m_pFacebook_guess;		// 0xe8
	Image	*m_pCancelImage_guess;		// 0xec, gfx/vgui/menu/cancel.tga
	ImagePanel	*m_pCancel_guess;		// 0xf0
	ColorButton	*m_pDonateButton_guess;	// 0xf4
	ColorButton	*m_pSoundtrack1Button_guess;	// 0xf8
	ColorButton	*m_pSoundtrack2Button_guess;	// 0xfc
	ColorButton	*m_pClothesButton_guess;	// 0x100
	ColorButton	*m_pAoMDCButton_guess;	// 0x104
	ColorButton	*m_pFacebookButton_guess;	// 0x108
	ColorButton	*m_pCancelButton_guess;	// 0x10c
	bool	m_bDonateArmed_guess;		// 0x110, the donate_mo.tga picture is shown
	bool	m_bSoundtrack1Armed_guess;		// 0x111, the soundtrack1_mo.tga picture is shown
	bool	m_bSoundtrack2Armed_guess;		// 0x112, the soundtrack2_mo.tga picture is shown
	bool	m_bClothesArmed_guess;		// 0x113, the clothes_mo.tga picture is shown
	bool	m_bAoMDCArmed_guess;		// 0x114, the aomdc_mo.tga picture is shown
	bool	m_bFacebookArmed_guess;		// 0x115, the facebook_mo.tga picture is shown
	bool	m_bCancelArmed_guess;		// 0x116, the cancel_mo.tga picture is shown
};

class CVGUIExtraMenuHandler_Command : public ActionSignal
{
private:
	CVGUIExtraMenu	*m_pFather;
	int			m_cmd;

public:
	CVGUIExtraMenuHandler_Command( CVGUIExtraMenu *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

#endif // _VGUIEXTRAMENU_H
