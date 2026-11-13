// ======================================
// Cry of Fear credits, statistics and unlocks panel header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _CREDITS_H
#define _CREDITS_H
using namespace vgui;

#define MAX_CREDIT_LINES_guess	255

class CCredits : public Panel
{
public:
	CCredits();
	~CCredits( void );
	void Initialize();

	int CountLines_guess();
	void SetLine_guess( int iUnused, int iLine );
	void ShowUnlock_guess( int iUnlock, const char *pszText, int iDistance, char *pszModel, int iBody );

protected:
	virtual void solve();

public:
	bool	m_bShowStats_guess;				// 0xbc, the statistics are shown after the credits
	bool	m_bTextOnly_guess;				// 0xbd, a text file without statistics (Credits message byte 1)
	bool	m_bContinue_guess;				// 0xbe, set by a key press (input.cpp)
	int		m_iUnknownC0_guess;				// 0xc0, never used in 1.6a
	Panel	*m_pLetterboxTop_guess;			// 0xc4
	Panel	*m_pLetterboxBottom_guess;		// 0xc8
	char	m_szUnlockText_guess[256];		// 0xcc, "Unlocked: %s" shown by ShowUnlock_guess
	bool	m_bUnlocks_guess;				// 0x1cc, showing the unlocks after the statistics
	int		m_iUnlocks_guess;				// 0x1d0, bit field of the unlocks still to show
	int		m_iScrollSpeed_guess;			// 0x1d4
	int		m_iUnknown1D8_guess[4];			// 0x1d8, never used in 1.6a
	int		m_iShotsFired_guess;			// 0x1e8
	int		m_iShotsHit_guess;				// 0x1ec
	int		m_iSyringesUsed_guess;			// 0x1f0
	int		m_iShotsMissed_guess;			// 0x1f4
	float	m_flAccuracy_guess;				// 0x1f8
	int		m_iTimesSaved_guess;			// 0x1fc
	int		m_iWeaponsFound_guess;			// 0x200
	int		m_iDamageTaken_guess;			// 0x204
	float	m_flDistanceDecimal_guess;		// 0x208
	float	m_flDistance_guess;				// 0x20c, km
	int		m_iSeconds_guess;				// 0x210
	int		m_iMinutes_guess;				// 0x214
	int		m_iHours_guess;					// 0x218
	int		m_iMeleeKills_guess;			// 0x21c
	int		m_iFirearmKills_guess;			// 0x220
	int		m_iDifficulty_guess;			// 0x224, 1 easy to 4 nightmare
	Label	*m_pStatsTitleLabel_guess;		// 0x228, "Statistics and grade"
	Label	*m_pGradeLabel_guess;			// 0x22c
	Label	*m_pGradeShadowLabel_guess;		// 0x230, red copy of the grade
	Label	*m_pStatLabel_guess[15];		// 0x234, [1-14]
	int		m_iUnknown270_guess[17];		// 0x270, never used in 1.6a
	Label	*m_pUnlockLabel_guess;			// 0x2b4, text of the unlock shown
	bool	m_bStatsTimer_guess;			// 0x2b8
	float	m_flStatsTime_guess;			// 0x2bc, the statistics are shown after this time
	char	m_szFile_guess[256];			// 0x2c0, text file of the credits
	Label	*m_pCreditLabel_guess[MAX_CREDIT_LINES_guess + 1];	// 0x3c0, [1-255], one per line of the file
	int		m_iNumLines_guess;				// 0x7c0
	bool	m_bStarted_guess;				// 0x7c4, set by the Credits message, never read in 1.6a
	bool	m_bScrollUp_guess;				// 0x7c5, false when the lines come from the bottom
	int		m_iBrightness_guess;			// 0x7c8, 0-255
	bool	m_bFadeIn_guess;				// 0x7cc
	bool	m_bFadeDone_guess;				// 0x7cd
	Font	*m_pCreditsFont_guess;			// 0x7d0, scheme "credits"
	Font	*m_pCreditsTitleFont_guess;		// 0x7d4, scheme "creditstitle", lines starting with #
	float	m_flStartTime_guess;			// 0x7d8
	float	m_flLastScrollTime = 0;
	float	m_flScrollPixels = 0;
	float	m_flScrollFadeTicks = 0;
};

extern bool g_bUnlockShown_guess;

char *UTIL_VarArgs_guess( const char *format, ... );

#endif // _CREDITS_H
