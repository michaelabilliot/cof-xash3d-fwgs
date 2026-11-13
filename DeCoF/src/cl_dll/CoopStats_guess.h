// ======================================
// Cry of Fear co-op statistics panel header file
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _COOPSTATS_H
#define _COOPSTATS_H
using namespace vgui;

#define MAX_COOP_PLAYERS_guess	4

class CCoopStats : public Panel
{
public:
	CCoopStats();
	~CCoopStats( void );
	void Initialize();

public:
	int		m_iUnknownBC_guess;				// 0xbc, only cleared
	Label	*m_pTitleLabel_guess;			// 0xc0, "STATS - CAMPAIGN WILL AUTORESTART SOON"
	Label	*m_pGradeLabel_guess;			// 0xc4, "Ending seen: 7" and "Grade: %c"
	Label	*m_pStatLabel_guess[MAX_COOP_PLAYERS_guess + 1][10];	// 0xc8, [player 1-4][1-9], ends at 0x190
};

void SetEndingOrUnlockables( int type, bool hasntgot, bool ending6, bool ending7 );

#endif // _COOPSTATS_H
