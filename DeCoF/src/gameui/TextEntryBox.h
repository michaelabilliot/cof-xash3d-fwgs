//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef TEXTENTRYBOX_H
#define TEXTENTRYBOX_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/QueryBox.h>

class CCvarTextEntry;

namespace vgui2
{
class TextEntry;
};

//-----------------------------------------------------------------------------
// Purpose: Popup dialog with a text entry, extending the QueryBox, which extends the MessageBox
//-----------------------------------------------------------------------------
class CTextEntryBox : public vgui2::QueryBox
{
	typedef vgui2::QueryBox BaseClass;

public:
	CTextEntryBox( const char *title, const char *queryText, const char *entryText, bool isCvar, vgui2::Panel *parent = NULL );
	~CTextEntryBox();

	virtual void PerformLayout();
	virtual void ShowWindow( vgui2::Frame *pFrameOver );

	virtual void OnKeyCodeTyped( vgui2::KeyCode code );

protected:
	virtual void OnCommand( const char *command );

	CCvarTextEntry *m_pCvarEntry;
	vgui2::TextEntry *m_pEntry;
};

#endif // TEXTENTRYBOX_H
