// ======================================
// Cry of Fear TV frame panel header file
// (class name from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _TVFRAME_H
#define _TVFRAME_H
using namespace vgui;

class CTVFrame : public Panel
{
public:
	CTVFrame();
	~CTVFrame( void );
	void Initialize();

public:
	int		m_iTVFrame_guess;			// 0xbc
	int		m_iUnusedC0_guess[8];		// 0xc0, never used in 1.6a (sizeof 0xe0 from the new in the viewport)
};

void TVFrameInit_guess();

#endif // _TVFRAME_H
