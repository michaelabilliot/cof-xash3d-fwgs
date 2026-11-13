//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef TOOLTIP_H
#define TOOLTIP_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Controls.h>
#include <UtlVector.h>

namespace vgui2
{

class IScheme;
class Panel;

//-----------------------------------------------------------------------------
// Purpose: Tooltip for a panel - shows text when cursor hovers over a panel
//-----------------------------------------------------------------------------
class Tooltip
{
public:
	Tooltip(Panel *parent, const char *text = NULL);
	~Tooltip();

	void SetText(const char *text);
	const char *GetText();

	void ShowTooltip(Panel *currentPanel);
	void HideTooltip();

	void SizeTextWindow();
	void ResetDelay();
	void PerformLayout();
	void InvalidateLayout();

	void SetTooltipFormatToSingleLine();
	void SetTooltipFormatToMultiLine();
	void SetTooltipDelay(int tooltipDelayMilliseconds);
	int GetTooltipDelay();

private:
	virtual void ApplySchemeSettings(IScheme *pScheme);

	CUtlVector<char> m_Text;
	int _delay;			// delay that counts down
	int _tooltipDelay;	// delay before tooltip comes up.
	bool _makeVisible;
	bool _displayOnOneLine;
	bool m_bInvalidLayout;
};

} // namespace vgui2

#endif // TOOLTIP_H
