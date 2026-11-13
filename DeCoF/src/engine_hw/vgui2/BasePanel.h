//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: the engine's root panels: the static panel under everything and
//			the panels the client and GameUI DLLs parent their windows to
//
// $NoKeywords: $
//=============================================================================

#ifndef BASEPANEL_H
#define BASEPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui/Cursor.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/EditablePanel.h>
#include "Color.h"

//-----------------------------------------------------------------------------
// Purpose: a panel the client or GameUI DLL builds its user interface on
//-----------------------------------------------------------------------------
class CEnginePanel : public vgui2::EditablePanel
{
	typedef vgui2::EditablePanel BaseClass;

public:
	CEnginePanel( vgui2::Panel *pParent, const char *pName ) : BaseClass( pParent, pName )
	{
		m_bCanFocus = true;

		Color col = GetBgColor();
		SetBgColor( Color( col[0], col[1], col[2], 255 ) );
	}

	void EnableMouseFocus( bool state )
	{
		m_bCanFocus = state;
	}

	virtual vgui2::VPANEL IsWithinTraverse( int x, int y, bool traversePopups )
	{
		if ( !m_bCanFocus )
			return NULL;

		vgui2::VPANEL retval = BaseClass::IsWithinTraverse( x, y, traversePopups );
		if ( retval == GetVPanel() )
			return NULL;

		return retval;
	}

protected:
	bool m_bCanFocus;
};

extern CEnginePanel *staticClientDLLPanel;
extern CEnginePanel *staticGameUIPanel;

//-----------------------------------------------------------------------------
// Purpose: the panel at the root of the engine's panel tree
//-----------------------------------------------------------------------------
class CStaticPanel : public vgui2::Panel
{
	typedef vgui2::Panel BaseClass;

public:
	CStaticPanel( vgui2::Panel *pParent, const char *pName ) : vgui2::Panel( pParent, pName )
	{
		SetCursor( vgui2::dc_none );

		Color col = GetBgColor();
		SetBgColor( Color( col[0], col[1], col[2], 255 ) );
	}

	virtual vgui2::VPANEL GetCurrentKeyFocus()
	{
		return staticClientDLLPanel->GetCurrentKeyFocus();
	}

	virtual vgui2::VPANEL IsWithinTraverse( int x, int y, bool traversePopups )
	{
		vgui2::VPANEL retval = BaseClass::IsWithinTraverse( x, y, traversePopups );
		if ( retval == GetVPanel() )
			return NULL;

		return retval;
	}
};

#endif // BASEPANEL_H
