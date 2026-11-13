// ======================================
// Cry of Fear boss health bar header file
// (class name from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _BOSSBAR_H
#define _BOSSBAR_H
using namespace vgui;

class CBossBar : public Panel
{
public:
	CBossBar();
	~CBossBar();
	void Initialize();

	virtual void solve();

public:
	int		m_iState_guess;				// 0xbc, 0 hidden, 1 shown, 2 hidden on the next solve
	char	m_szEmptyImage_guess[128];	// 0xc0
	char	m_szFullImage_guess[128];	// 0x140
	int		m_iHealth_guess;			// 0x1c0
	int		m_iMaxHealth_guess;			// 0x1c4
	int		m_iUnused1C8_guess;			// 0x1c8, only cleared in 1.6a
	Image	*m_pFullImage_guess;		// 0x1cc, gfx/vgui/boss/doctor_health_full.tga
	Image	*m_pEmptyImage_guess;		// 0x1d0, gfx/vgui/boss/doctor_health_empty.tga
	ImagePanel	*m_pFull_guess;			// 0x1d4
	ImagePanel	*m_pEmpty_guess;		// 0x1d8
	ImagePanel	*m_pBar_guess;			// 0x1dc, cut to the health fraction in solve
};

void VGUI_BossBarInit_guess();

#endif // _BOSSBAR_H
