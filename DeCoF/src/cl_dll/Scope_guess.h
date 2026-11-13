// ======================================
// Cry of Fear telescope scope panel header file
// (class name from the RTTI of client.dll, file name guessed)
// ======================================

#ifndef _SCOPE_H
#define _SCOPE_H
using namespace vgui;

class CScope : public Panel
{
public:
	CScope();
	~CScope( void );
	void Initialize();

public:
	int		m_iScope_guess;				// 0xbc, Scope message
	Image	*m_pScopeImage_guess;		// 0xc0, gfx/vgui/telescope_scope.tga
	ImagePanel	*m_pScopePanel_guess;	// 0xc4
};

void ScopeInit_guess();

#endif // _SCOPE_H
