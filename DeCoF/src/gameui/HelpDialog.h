//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef HELPDIALOG_H
#define HELPDIALOG_H

#ifdef _WIN32
#pragma once
#endif

#include "taskframe.h"

namespace vgui2
{
class Button;
class CheckButton;
class Label;
};

//-----------------------------------------------------------------------------
// Purpose: Displays the desktop help text
//-----------------------------------------------------------------------------
class CHelpDialog : public CTaskFrame
{
	typedef CTaskFrame BaseClass;

public:
	CHelpDialog();
	~CHelpDialog();

protected:
	virtual void OnClose();
	virtual void OnCommand( const char *command );

private:
	vgui2::Label *m_pInfoText;
	vgui2::Button *m_pClose;
	vgui2::CheckButton *m_pNeverShowAgain;
};

#endif // HELPDIALOG_H
