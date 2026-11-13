//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef CDKEYENTRYDIALOG_H
#define CDKEYENTRYDIALOG_H

#ifdef _WIN32
#pragma once
#endif

#include "taskframe.h"

namespace vgui2
{
class Button;
class TextEntry;
};

//-----------------------------------------------------------------------------
// Purpose: Asks the user for a CD key
//-----------------------------------------------------------------------------
class CCDKeyEntryDialog : public CTaskFrame
{
	DECLARE_CLASS_SIMPLE( CCDKeyEntryDialog, CTaskFrame );

public:
	CCDKeyEntryDialog( vgui2::Panel *parent, bool inConnect = false );
	~CCDKeyEntryDialog();

	virtual void Activate();

private:
	virtual void OnCommand( const char *command );
	virtual void OnClose();
	virtual void OnThink();

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );

	bool IsEnteredKeyValid();

	vgui2::Button *m_pOK;
	vgui2::Button *m_pQuitGame;
	vgui2::TextEntry *m_pEntry1;
	vgui2::TextEntry *m_pEntry2;
	vgui2::TextEntry *m_pEntry3;
	vgui2::TextEntry *m_pEntry4;
	vgui2::TextEntry *m_pEntry5;

	bool m_bIsValveCDKey;
	bool m_bInConnect;
	int m_iErrCount;
};

#endif // CDKEYENTRYDIALOG_H
