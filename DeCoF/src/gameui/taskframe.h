//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Frame that shows up in the GameUI taskbar
//
// $NoKeywords: $
//=============================================================================

#ifndef TASKFRAME_H
#define TASKFRAME_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/ILocalize.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Frame.h>
#include "Taskbar.h"

namespace vgui = vgui2;

//-----------------------------------------------------------------------------
// Purpose: Frame whose title is mirrored in the taskbar
//-----------------------------------------------------------------------------
class CTaskFrame : public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE( CTaskFrame, vgui::Frame );

public:
	CTaskFrame( vgui2::Panel *parent, const char *panelName, bool showTaskbarIcon = true ) : BaseClass( parent, panelName, showTaskbarIcon ) {}
	virtual ~CTaskFrame() {}

	virtual void SetTitle( const char *title, bool surfaceTitle )
	{
		BaseClass::SetTitle( title, surfaceTitle );

		if ( g_pTaskbar )
		{
			wchar_t *wTitle;
			wchar_t w_szTitle[1024];

			wTitle = vgui2::localize()->Find( title );
			if ( !wTitle )
			{
				vgui2::localize()->ConvertANSIToUnicode( title, w_szTitle, sizeof( w_szTitle ) );
				wTitle = w_szTitle;
			}

			g_pTaskbar->SetTitle( GetVPanel(), wTitle );
		}
	}
};

#endif // TASKFRAME_H
