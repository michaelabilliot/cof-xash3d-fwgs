//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: A button in the taskbar for one open dialog
//
// $NoKeywords: $
//=============================================================================

#ifndef TASKBUTTON_H
#define TASKBUTTON_H

#ifdef _WIN32
#pragma once
#endif

#include <Color.h>
#include <vgui/MouseCode.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Image.h>

//-----------------------------------------------------------------------------
// Purpose: Draws the focused, out of focus or minimized state of a window
//-----------------------------------------------------------------------------
class CWindowStateImage : public vgui2::Image
{
public:
	CWindowStateImage();

	void SetTall( int tall );
	virtual void Paint();

	void SetFocused();
	void SetOutOfFocus();
	void SetMinimized();

private:
	int m_iState;
};

//-----------------------------------------------------------------------------
// Purpose: Taskbar button for a window
//-----------------------------------------------------------------------------
class CTaskButton : public vgui2::Label
{
public:
	CTaskButton( vgui2::Panel *parent, vgui2::VPANEL task );
	~CTaskButton();

	void SetTitle( const wchar_t *title );
	void SetSelected( bool state );
	bool ShouldDisplay();
	vgui2::VPANEL GetTaskPanel();

protected:
	virtual void OnMousePressed( vgui2::MouseCode mouseCode );
	virtual void OnMouseDoublePressed( vgui2::MouseCode mouseCode );
	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );

private:
	vgui2::HPanel m_hTask;
	bool m_bHasTitle;
	bool m_bSelected;
	Color m_SelectedColor;
	Color m_LabelColor;
	CWindowStateImage m_WindowStateImage;
};

#endif // TASKBUTTON_H
