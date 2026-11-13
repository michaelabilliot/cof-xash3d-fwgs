//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: A slider with a text entry that shows its value
//
// $NoKeywords: $
//=============================================================================

#ifndef LABELSLIDERCOMBO_H
#define LABELSLIDERCOMBO_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Slider.h>

namespace vgui2
{
class TextEntry;
};

class LabelSliderCombo : public vgui2::Slider
{
public:
	LabelSliderCombo( vgui2::Panel *parent, const char *sliderName, const char *textEntryName );
	~LabelSliderCombo();

	virtual void SetValue( int value, bool bTriggerChangeMessage = true );
	virtual void OnCursorMoved( int x, int y );
	virtual void SetVisible( bool state );

private:
	typedef vgui2::Slider BaseClass;

	void UpdateTextEntry( bool bTriggerChangeMessage );

	vgui2::TextEntry *m_pTextEntry;
	int m_currentValueShown;
};

#endif // LABELSLIDERCOMBO_H
