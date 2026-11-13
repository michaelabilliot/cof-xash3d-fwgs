//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: A label with a dotted background
//
// $NoKeywords: $
//=============================================================================

#ifndef DOTTEDLABEL_H
#define DOTTEDLABEL_H

#ifdef _WIN32
#pragma once
#endif

#include <Color.h>
#include <vgui_controls/Label.h>

//-----------------------------------------------------------------------------
// Purpose: A label that draws a dotted border with rounded corners
//-----------------------------------------------------------------------------
class CDottedBgLabel : public vgui2::Label
{
public:
	CDottedBgLabel( vgui2::Panel *parent, const char *panelName, const char *text, int dash, int gap, int corner );

	virtual void PaintBackground();

	void SetColor( Color col ) { m_color = col; }

private:
	int m_dash;
	int m_gap;
	int m_corner;
	Color m_color;
};

#endif // DOTTEDLABEL_H
