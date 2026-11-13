// ======================================
// Cry of Fear HUD panel header file
// (class name from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _HUDCONTROL_H
#define _HUDCONTROL_H
using namespace vgui;

class CHUDControl : public Panel
{
public:
	CHUDControl();
	~CHUDControl();
	void Initialize();

	void HandleHUDVisibility_guess();
	void LogoUpdate_guess();
	void RenderMessages_guess();
	void ShowSMSIcon_guess( bool bShow );
	void UpdateOnPlayerInfo_guess();

protected:
	virtual void solve();
	virtual void paintBackground();
	virtual void paint();

public:
	int		m_iMessageType_guess;		// 0xbc, text color of the message strip (5 = grey)
	int		m_iMessageTime_guess;		// 0xc0
	int		m_iMessageC4_guess;			// 0xc4
	int		m_iMessageC8_guess;			// 0xc8
	int		m_iMessageCC_guess;			// 0xcc
	int		m_iMessageD0_guess;			// 0xd0
	char	m_szMessage_guess[256];		// 0xd4, "You got the %s" and the ProFont messages
	bool	m_bShowLogo_guess;			// 0x1d4
	char	m_szMessage2_guess[256];	// 0x1d5, second line of the message strip
	int		m_iDevDonator_guess[5];		// 0x2d8, indexed by player
	Label	*m_pTimerPlusLabel_guess[32];	// 0x2ec, "+0" popups of TimerPlus
	float	m_flTimerPlusTime_guess;	// 0x36c
	int		m_iTimerLimit_guess;		// 0x370
	bool	m_bTimerPaused_guess;		// 0x374
	Panel	*m_pLetterboxTop_guess;		// 0x378
	Panel	*m_pLetterboxBottom_guess;	// 0x37c
	Panel	*m_pOverlay_guess;			// 0x380
	Label	*m_pVoteLabel_guess;		// 0x384, "Press Vote button when ready"
	Label	*m_pLobbyLabel_guess[4];	// 0x388
	Label	*m_pVoteKickLabel_guess;	// 0x398, "Type votekick name in chat to votekick a player"
	bool	m_bLobby_guess;				// 0x39c
	bool	m_bPlayerReady_guess[4];	// 0x39d
	Label	*m_pTimerLabel_guess;		// 0x3a4, "00:00:00"
	Label	*m_pMessageLabel_guess;		// 0x3a8
	Label	*m_pMessageLabel2_guess;	// 0x3ac
	Label	*m_pHintLabel_guess;		// 0x3b0
	Label	*m_pSayTextLabel_guess[5];	// 0x3b4
	Label	*m_pUnused3C8_guess;		// 0x3c8, never used in 1.6a
	Label	*m_pLookingAtLabel_guess;	// 0x3cc
	Label	*m_pPlayerLabel_guess[4];	// 0x3d0
	Label	*m_pDeathLabel_guess;		// 0x3e0
	float	m_flLookingAtTime_guess;	// 0x3e4
	ImagePanel	*m_pLobbyBackground_guess;	// 0x3e8
	int		m_iDeathAlpha_guess;		// 0x3ec
	bool	m_bTimer_guess;				// 0x3f0
	float	m_flTimerNextSecond_guess;	// 0x3f4
	int		m_iTimerSeconds_guess;		// 0x3f8
	int		m_iTimerMinutes_guess;		// 0x3fc
	int		m_iTimerHours_guess;		// 0x400
	char	m_szHintLine1_guess[256];	// 0x404, "line1" of the hint file
	char	m_szHintLine2_guess[256];	// 0x504, "line2"
	char	m_szHintLine3_guess[256];	// 0x604, "line3"
	char	m_szHintNoButton_guess[256];	// 0x704, "nobutton"
	char	m_szHintCfgButton_guess[64];	// 0x804, "cfgbutton"
	int		m_iHintLine_guess;			// 0x844
	bool	m_bHint_guess;				// 0x848
	int		m_iHintAlpha_guess;			// 0x84c
	float	m_flHintTime_guess;			// 0x850
	int		m_iUnused854_guess;			// 0x854, never used in 1.6a
	int		m_iUnused858_guess;			// 0x858, never used in 1.6a
	int		m_iSayTextX_guess;			// 0x85c
	int		m_iSayTextY_guess;			// 0x860
	char	m_szUnused864_guess[6][256];	// 0x864, never used in 1.6a
	ImagePanel	*m_pIntroduction_guess;	// 0xe64
	Image	*m_pIntroductionImage_guess;	// 0xe68
	bool	m_bIntroductionFadeIn_guess;	// 0xe6c
	int		m_iIntroductionAlpha_guess;	// 0xe70
	ImagePanel	*m_pHintBar_guess;		// 0xe74
	Image	*m_pHintBarImage_guess;		// 0xe78
	ImagePanel	*m_pVignetteTL_guess;	// 0xe7c
	ImagePanel	*m_pVignetteTR_guess;	// 0xe80
	ImagePanel	*m_pVignetteBL_guess;	// 0xe84
	ImagePanel	*m_pVignetteBR_guess;	// 0xe88
	Panel	*m_pBlackScreen_guess;		// 0xe8c
	Font	*m_pPagerFont_guess;		// 0xe90, scheme "pager"
	Font	*m_pChapterTitleFont_guess;	// 0xe94, scheme "chaptertitle"
	char	m_szChapter_guess[128];		// 0xe98
	float	m_flMessageAlpha_guess;		// 0xf18
	bool	m_bMessageFadeOut_guess;	// 0xf1c
	bool	m_bMessageHold_guess;		// 0xf1d
	bool	m_bChapterF1E_guess;		// 0xf1e
	float	m_flMessageHoldTime_guess;	// 0xf20
	float	m_flMessageF24_guess;		// 0xf24
	int		m_iMessageChars_guess;		// 0xf28
	float	m_flChapterAlpha_guess;		// 0xf2c
	bool	m_bChapterFadeOut_guess;	// 0xf30
	bool	m_bChapterHold_guess;		// 0xf31
	float	m_flChapterHoldTime_guess;	// 0xf34
	float	m_flChapterNextChar_guess;	// 0xf38
	int		m_iChapterChars_guess;		// 0xf3c
	int		m_iChapterType_guess;		// 0xf40
	float	m_flHintSlideInTime_guess;	// 0xf44
	float	m_flHintSlideInLength_guess;	// 0xf48
	float	m_flHintSlideOutTime_guess;	// 0xf4c
	float	m_flHintSlideOutLength_guess;	// 0xf50
	Font	*m_pCreditsFont_guess;		// 0xf54, scheme "credits"
	int		m_iLogoAlpha_guess;			// 0xf58
	bool	m_bLogoF5C_guess;			// 0xf5c
	bool	m_bLogoF5D_guess;			// 0xf5d
	float	m_flLogoTime_guess;			// 0xf60
	int		m_iLogo_guess;				// 0xf64
	Image	*m_pFollowIcon_guess[4];		// 0xf68, gfx/vgui/coop_follow_icon.tga
	ImagePanel	*m_pFollowIconPanel_guess[4];	// 0xf78
	Image	*m_pSyringeIcon_guess[4];		// 0xf88, gfx/vgui/coop_syringe_icon.tga
	ImagePanel	*m_pSyringeIconPanel_guess[4];	// 0xf98
	int		m_iUnusedFA8_guess[5];		// 0xfa8, never used in 1.6a
	float	m_flZoomFOV_guess;			// 0xfbc
	float	m_flZoomLength_guess;		// 0xfc0
	float	m_flZoomTime_guess;			// 0xfc4
	ImagePanel	*m_pHealthBar_guess;		// 0xfc8
	ImagePanel	*m_pHealthBarFull_guess;	// 0xfcc
	ImagePanel	*m_pHealthBarEmpty_guess;	// 0xfd0
	ImagePanel	*m_pLogo_guess;				// 0xfd4
	ImagePanel	*m_pStaminaBar_guess;		// 0xfd8
	ImagePanel	*m_pStaminaBarFull_guess;	// 0xfdc
	ImagePanel	*m_pStaminaBarEmpty_guess;	// 0xfe0
	float	m_flStaminaFlashTime_guess;	// 0xfe4
	Panel	*m_pStaminaFlash_guess;		// 0xfe8
	Image	*m_pLogoImage_guess;		// 0xfec
	ImagePanel	*m_pSMSIcon_guess;		// 0xff0
	bool	m_bSMSIcon_guess;			// 0xff4
	float	m_flSMSIconTime_guess;		// 0xff8
	int		m_iSMSIconAlpha_guess;		// 0xffc
	int		m_iStamina_guess;			// 0x1000
	ImagePanel	*m_pInventoryHint_guess;	// 0x1004
	bool	m_bInventoryHint_guess;		// 0x1008
	float	m_flInventoryHintTime_guess;	// 0x100c
	int		m_iUnused1010_guess;		// 0x1010, never used in 1.6a
	int		m_iInventoryHintY_guess;	// 0x1014
	Image	*m_pSMSIconImage_guess;		// 0x1018, gfx/vgui/640_hudelem_smsicon.tga
	Image	*m_pInventoryHintImage_guess;	// 0x101c, gfx/vgui/hints/hint_inventory.tga
	Image	*m_pCrosshairImage_guess;	// 0x1020, gfx/vgui/crosshair.tga
	Image	*m_pHealthBarImage_guess;	// 0x1024, gfx/vgui/640_healthbar.tga
	Image	*m_pHealthBarFullImage_guess;	// 0x1028, gfx/vgui/640_healthbar_bar.tga
	Image	*m_pHealthBarEmptyImage_guess;	// 0x102c, gfx/vgui/640_healthbar_barempty.tga
	Image	*m_pStaminaBarImage_guess;	// 0x1030, gfx/vgui/640_staminabar.tga
	Image	*m_pStaminaBarFullImage_guess;	// 0x1034, gfx/vgui/640_staminabar_bar.tga
	Image	*m_pStaminaBarEmptyImage_guess;	// 0x1038, gfx/vgui/640_staminabar_barempty.tga
	int		m_iUnused103C_guess[4];		// 0x103c, never used in 1.6a
};

void HUDControlInit_guess();
void ParseHintTokens( char *pszName, char *pszFile );

#endif // _HUDCONTROL_H
