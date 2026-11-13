//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Asks for the Steam password again when the login has expired
//
// $NoKeywords: $
//=============================================================================

#ifndef REFRESHSTEAMLOGIN_H
#define REFRESHSTEAMLOGIN_H

#ifdef _WIN32
#pragma once
#endif

#include "taskframe.h"

namespace vgui2
{
class Button;
class CheckButton;
class TextEntry;
};

//-----------------------------------------------------------------------------
// Purpose: Steam login refresh dialog
//-----------------------------------------------------------------------------
class CRefreshSteamLogin : public CTaskFrame
{
	DECLARE_CLASS_SIMPLE( CRefreshSteamLogin, CTaskFrame );

public:
	CRefreshSteamLogin( vgui2::Panel *parent, const char *username );
	~CRefreshSteamLogin();

	virtual void Activate();

protected:
	virtual void OnCommand( const char *command );
	virtual void OnClose();
	virtual void OnThink();

private:
	vgui2::TextEntry *m_pUserName;
	vgui2::TextEntry *m_pPassword;
	vgui2::CheckButton *m_pSavePassword;
	vgui2::Button *m_pLoginButton;
	bool m_bLoggingIn;
	vgui2::VPANEL m_RestrictedPaintPanel;
};

#endif // REFRESHSTEAMLOGIN_H
