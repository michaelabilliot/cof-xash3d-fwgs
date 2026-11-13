// ======================================
// Cry of Fear cutscene skip panel header file
// (class name from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _CUTSCENESKIP_H
#define _CUTSCENESKIP_H
using namespace vgui;

class CCutsceneSkip : public Panel
{
public:
	CCutsceneSkip();
	~CCutsceneSkip( void );
	void Initialize();

public:
	Image	*m_pDotsImage_guess;		// 0xbc, gfx/vgui/loading_dots.tga
	ImagePanel	*m_pDotsPanel_guess;	// 0xc0
};

#endif // _CUTSCENESKIP_H
