//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include <vgui_controls/ProgressBar.h>
#include <vgui_controls/Controls.h>

#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/KeyValues.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
ProgressBar::ProgressBar(Panel *parent, const char *panelName) : Panel(parent, panelName)
{
	_progress = 0.0f;
	m_pszDialogVar = NULL;
	SetSegmentInfo( 4, 8 );
	SetBarInset( 4 );

	int screenWidth_guess, screenHeight_guess;
	surface()->GetScreenSize( screenWidth_guess, screenHeight_guess );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
ProgressBar::~ProgressBar()
{
	delete [] m_pszDialogVar;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ProgressBar::SetSegmentInfo( int gap, int width )
{
	_segmentGap = gap;
	_segmentWide = width;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ProgressBar::PaintBackground()
{
	int wide, tall;
	GetSize(wide, tall);

	// gaps
	surface()->DrawSetColor(GetBgColor());
	surface()->DrawFilledRect(0, 0, wide, tall);

	int segmentTotal = wide / (_segmentGap + _segmentWide);
	int segmentsDrawn = (int)(segmentTotal * _progress);

	float flProgress_guess = GetProgress();
	float progressColor_guess = flProgress_guess * 255.0f;
	if (progressColor_guess > 255.0f)
	{
		progressColor_guess = 255.0f;
	}
	if (progressColor_guess < 0.0f)
	{
		progressColor_guess = 0.0f;
	}

	surface()->DrawSetColor(128, 128, 128, 255);
	int gap_guess = 10;
	int x = 0, y = m_iBarInset;
	for (int i = 0; i < segmentsDrawn; i++)
	{
		x += _segmentGap;
		surface()->DrawFilledRect(x, y, x + _segmentWide, y + tall - (y * 2));
		x += _segmentWide;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ProgressBar::SetProgress(float progress)
{
	if (progress != _progress)
	{
		// clamp the progress value within the range
		if (progress < 0.0f)
		{
			progress = 0.0f;
		}
		else if (progress > 1.0f)
		{
			progress = 1.0f;
		}

		_progress = progress;
		Repaint();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float ProgressBar::GetProgress()
{
	return _progress;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ProgressBar::ApplySchemeSettings(IScheme *pScheme)
{
	Panel::ApplySchemeSettings(pScheme);

	SetFgColor(GetSchemeColor("BrightControlText", pScheme));
	SetBgColor(GetSchemeColor("WindowBgColor", pScheme));
	SetBorder(pScheme->GetBorder("ButtonDepressedBorder"));
}

//-----------------------------------------------------------------------------
// Purpose: utility function for calculating a time remaining string
//-----------------------------------------------------------------------------
bool ProgressBar::ConstructTimeRemainingString(wchar_t *output, int outputBufferSizeInBytes, float startTime, float currentTime, float currentProgress, float lastProgressUpdateTime, bool addRemainingSuffix)
{
	output[0] = 0;

	// calculate pre-extrapolation values
	float timeElapsed = lastProgressUpdateTime - startTime;
	float totalTime = timeElapsed / currentProgress;

	// calculate seconds
	int secondsRemaining = (int)(totalTime - timeElapsed);
	if (lastProgressUpdateTime < currentTime)
	{
		// old update, extrapolate
		float progressRate = currentProgress / timeElapsed;
		float extrapolatedProgress = progressRate * (currentTime - startTime);
		float extrapolatedTotalTime = (currentTime - startTime) / extrapolatedProgress;
		secondsRemaining = (int)(extrapolatedTotalTime - timeElapsed);
	}
	// if there's some time, make sure it's at least one second left
	if ( secondsRemaining == 0 && ( ( totalTime - timeElapsed ) > 0 ) )
	{
		secondsRemaining = 1;
	}

	// calculate minutes
	int minutesRemaining = 0;
	while (secondsRemaining >= 60)
	{
		minutesRemaining++;
		secondsRemaining -= 60;
	}

	char minutesBuf[16];
	sprintf(minutesBuf, "%d", minutesRemaining);
	char secondsBuf[16];
	sprintf(secondsBuf, "%d", secondsRemaining);

	if (minutesRemaining > 0)
	{
		wchar_t unicodeMinutes[16];
		localize()->ConvertANSIToUnicode(minutesBuf, unicodeMinutes, sizeof( unicodeMinutes ));
		wchar_t unicodeSeconds[16];
		localize()->ConvertANSIToUnicode(secondsBuf, unicodeSeconds, sizeof( unicodeSeconds ));

		const char *unlocalizedString = "#vgui_TimeLeftMinutesSeconds";
		if (minutesRemaining == 1 && secondsRemaining == 1)
		{
			unlocalizedString = "#vgui_TimeLeftMinuteSecond";
		}
		else if (minutesRemaining == 1)
		{
			unlocalizedString = "#vgui_TimeLeftMinuteSeconds";
		}
		else if (secondsRemaining == 1)
		{
			unlocalizedString = "#vgui_TimeLeftMinutesSecond";
		}

		char unlocString[64];
		strcpy(unlocString, unlocalizedString);
		if (addRemainingSuffix)
		{
			strcat(unlocString, "Remaining");
		}
		localize()->ConstructString(output, outputBufferSizeInBytes, localize()->Find(unlocString), 2, unicodeMinutes, unicodeSeconds);

	}
	else if (secondsRemaining > 0)
	{
		wchar_t unicodeSeconds[16];
		localize()->ConvertANSIToUnicode(secondsBuf, unicodeSeconds, sizeof( unicodeSeconds ));

		const char *unlocalizedString = "#vgui_TimeLeftSeconds";
		if (secondsRemaining == 1)
		{
			unlocalizedString = "#vgui_TimeLeftSecond";
		}
		char unlocString[64];
		strcpy(unlocString, unlocalizedString);
		if (addRemainingSuffix)
		{
			strcat(unlocString, "Remaining");
		}
		localize()->ConstructString(output, outputBufferSizeInBytes, localize()->Find(unlocString), 1, unicodeSeconds);
	}
	else
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ProgressBar::SetBarInset( int pixels )
{
	m_iBarInset = pixels;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int ProgressBar::GetBarInset( void )
{
	return m_iBarInset;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ProgressBar::ApplySettings(KeyValues *inResourceData)
{
	_progress = inResourceData->GetFloat("progress", 0.0f);

	const char *dialogVar = inResourceData->GetString("variable", "");
	if (dialogVar && *dialogVar)
	{
		m_pszDialogVar = new char[strlen(dialogVar) + 1];
		strcpy(m_pszDialogVar, dialogVar);
	}

	BaseClass::ApplySettings(inResourceData);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ProgressBar::GetSettings(KeyValues *outResourceData)
{
	BaseClass::GetSettings(outResourceData);
	outResourceData->SetFloat("progress", _progress );

	if (m_pszDialogVar)
	{
		outResourceData->SetString("variable", m_pszDialogVar);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns a string description of the panel fields for use in the UI
//-----------------------------------------------------------------------------
const char *ProgressBar::GetDescription( void )
{
	static char buf[1024];
	_snprintf(buf, sizeof(buf), "%s, string progress, string variable", BaseClass::GetDescription());
	return buf;
}

//-----------------------------------------------------------------------------
// Purpose: updates progress bar bases on values
//-----------------------------------------------------------------------------
void ProgressBar::OnDialogVariablesChanged(KeyValues *dialogVariables)
{
	if (m_pszDialogVar)
	{
		int val = dialogVariables->GetInt(m_pszDialogVar, -1);
		if (val >= 0.0f)
		{
			SetProgress(val / 100.0f);
		}
	}
}
