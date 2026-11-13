// ======================================
// Cry of Fear game menu header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _GAMEMENU_H
#define _GAMEMENU_H
using namespace vgui;

#define MAX_CAMPAIGNS_guess	256

// a custom campaign, read from maps/*.custom
typedef struct
{
	char	szFirstMap[64];		// "firstmap"
	char	szDescription[64];	// "description", the text of its button
	char	szAuthor[64];		// "author"
	bool	bValid;
} campaign_t_guess;

class CCampaignSlider : public Slider
{
public:
	CCampaignSlider( int x, int y, int wide, int tall, bool vertical ) : Slider( x, y, wide, tall, vertical )
	{
	}

	virtual void paintBackground();
};

class CGameMenu : public Panel
{
public:
	CGameMenu();
	~CGameMenu( void );
	void Initialize();
	void ActionSignal( int cmd );

	void ParseCampaign_guess( char *pszFile, int index );
	void ReadCampaigns_guess();
	void ResetButtonPositions_guess();

	virtual void setVisible( bool state );

protected:
	virtual void solve();

public:
	Label	*m_pVersionLabel_guess;			// 0xbc, "1.6a" in the top left corner
	bool	m_bUnknownC0_guess;				// 0xc0, only cleared
	campaign_t_guess	m_Campaigns_guess[MAX_CAMPAIGNS_guess + 1];	// 0xc1
	ColorButton	*m_pCampaignButton_guess[MAX_CAMPAIGNS_guess];	// 0xc284
	int		m_iUnknownC684_guess;			// 0xc684, never used in 1.6a
	int		m_iUnknownC688_guess;			// 0xc688, never used in 1.6a
	Label	*m_pAuthorLabel_guess;			// 0xc68c, "Author: %s" of the campaign under the cursor
	Font	*m_pCreditsFont_guess;			// 0xc690, scheme "credits"
	int		m_iArmedCampaign_guess;			// 0xc694, campaign button under the cursor, -1 for none
	bool	m_bCampaignArmed_guess;			// 0xc698
	bool	m_bNewGameArmed_guess;		// 0xc699, the newgame_mo.tga picture is shown
	bool	m_bLoadGameArmed_guess;		// 0xc69a, the loadgame_mo.tga picture is shown
	bool	m_bCustomArmed_guess;		// 0xc69b, the customc_mo.tga picture is shown
	bool	m_bJoinServerArmed_guess;		// 0xc69c, the jserver_mo.tga picture is shown
	bool	m_bOptionsArmed_guess;		// 0xc69d, the options_mo.tga picture is shown
	bool	m_bLanguageArmed_guess;		// 0xc69e, the language_mo.tga picture is shown
	bool	m_bUnlockablesArmed_guess;		// 0xc69f, the unlockables_mo.tga picture is shown
	bool	m_bExtrasArmed_guess;		// 0xc6a0, the extras_mo.tga picture is shown
	bool	m_bHostServerArmed_guess;		// 0xc6a1, the hserver_mo.tga picture is shown
	bool	m_bQuitArmed_guess;		// 0xc6a2, the quit_mo.tga picture is shown
	char	*m_pszCampaign_guess;			// 0xc6a4, first map of the campaign picked for the difficulty menu, NULL for a new game
	int		m_iArmedX_guess;				// 0xc6a8, position of the picture under the cursor, it shakes around it
	int		m_iArmedY_guess;				// 0xc6ac
	CTFScrollPanel	*m_pCampaignList_guess;	// 0xc6b0
	ColorButton	*m_pNewGameButton_guess;	// 0xc6b4
	ColorButton	*m_pLoadGameButton_guess;	// 0xc6b8
	ColorButton	*m_pCustomButton_guess;	// 0xc6bc
	ColorButton	*m_pJoinServerButton_guess;	// 0xc6c0
	ColorButton	*m_pOptionsButton_guess;	// 0xc6c4
	ColorButton	*m_pLanguageButton_guess;	// 0xc6c8
	ColorButton	*m_pUnlockablesButton_guess;	// 0xc6cc
	ColorButton	*m_pExtrasButton_guess;	// 0xc6d0
	ColorButton	*m_pHostServerButton_guess;	// 0xc6d4
	ColorButton	*m_pQuitButton_guess;	// 0xc6d8
	ColorButton	*m_pCancelButton_guess;		// 0xc6dc, closes the campaign list
	Image	*m_pLogoImage_guess;			// 0xc6e0, gfx/vgui/menu_logo.tga
	ImagePanel	*m_pLogo_guess;				// 0xc6e4
	Image	*m_pLogoTSImage_guess;			// 0xc6e8, gfx/vgui/menu_logo_ts.tga
	ImagePanel	*m_pLogoTS_guess;			// 0xc6ec
	Image	*m_pNewGameImage_guess;		// 0xc6f0, gfx/vgui/menu/newgame.tga
	ImagePanel	*m_pNewGame_guess;		// 0xc6f4
	Image	*m_pLoadGameImage_guess;		// 0xc6f8, gfx/vgui/menu/loadgame.tga
	ImagePanel	*m_pLoadGame_guess;		// 0xc6fc
	Image	*m_pCustomImage_guess;		// 0xc700, gfx/vgui/menu/customc.tga
	ImagePanel	*m_pCustom_guess;		// 0xc704
	Image	*m_pJoinServerImage_guess;		// 0xc708, gfx/vgui/menu/jserver.tga
	ImagePanel	*m_pJoinServer_guess;		// 0xc70c
	Image	*m_pOptionsImage_guess;		// 0xc710, gfx/vgui/menu/options.tga
	ImagePanel	*m_pOptions_guess;		// 0xc714
	Image	*m_pLanguageImage_guess;		// 0xc718, gfx/vgui/menu/language.tga
	ImagePanel	*m_pLanguage_guess;		// 0xc71c
	Image	*m_pUnlockablesImage_guess;		// 0xc720, gfx/vgui/menu/unlockables.tga
	ImagePanel	*m_pUnlockables_guess;		// 0xc724
	Image	*m_pExtrasImage_guess;		// 0xc728, gfx/vgui/menu/extras.tga
	ImagePanel	*m_pExtras_guess;		// 0xc72c
	Image	*m_pHostServerImage_guess;		// 0xc730, gfx/vgui/menu/hserver.tga
	ImagePanel	*m_pHostServer_guess;		// 0xc734
	Image	*m_pQuitImage_guess;		// 0xc738, gfx/vgui/menu/quit.tga
	ImagePanel	*m_pQuit_guess;		// 0xc73c
	int		m_iUnknownC740_guess;			// 0xc740, never used in 1.6a
	int		m_iUnknownC744_guess;			// 0xc744, never used in 1.6a
};

class CGameMenuHandler_Command : public ActionSignal
{
private:
	CGameMenu	*m_pFather;
	int			m_cmd;

public:
	CGameMenuHandler_Command( CGameMenu *panel, int cmd )
	{
		m_pFather = panel;
		m_cmd = cmd;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pFather->ActionSignal( m_cmd );
	}
};

#endif // _GAMEMENU_H
