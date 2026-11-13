//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef MESSAGEBOX_H
#define MESSAGEBOX_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Frame.h>

// prevent windows macros from messing with the class
#ifdef MessageBox
#undef MessageBox
#endif

namespace vgui2
{

class Button;
class Label;

//-----------------------------------------------------------------------------
// Purpose: Popup discardable message box
//-----------------------------------------------------------------------------
class MessageBox : public Frame
{
	DECLARE_CLASS_SIMPLE( MessageBox, Frame );

public:
	MessageBox(const char *title, const char *text, Panel *parent = NULL);
	MessageBox(const wchar_t *wszTitle, const wchar_t *wszText, Panel *parent = NULL);
	~MessageBox();

	// Put the message box into a modal state
	// Does not suspend execution - use addActionSignal to get return value
	virtual void DoModal(Frame *pFrameOver = NULL);

	// make the message box appear and in a modeless state
	virtual void ShowWindow(Frame *pFrameOver = NULL);

	// Set a string command to be sent when the OK button is pressed
	// Use AddActionSignalTarget() to mark yourself as a recipient of the command
	virtual void SetCommand(const char *command);
	virtual void SetCommand(KeyValues *command);

	// Set the visibility of the OK button.
	virtual void SetOKButtonVisible(bool state);

	// Set the text on the OK button
	virtual void SetOKButtonText(const char *buttonText);
	virtual void SetOKButtonText(const wchar_t *wszButtonText);

	// Toggle visibility of the close box.
	virtual void DisableCloseButton(bool state);

protected:
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(IScheme *pScheme);
	virtual void OnClose();

	Button *m_pOkButton;
	Label *m_pMessageLabel;

private:
	MESSAGE_FUNC( OnShutdownRequest, "ShutdownRequest" );

	void Init();

	KeyValues *m_OkCommand;
	Frame *m_pFrameOver;
};

} // namespace vgui2

#endif // MESSAGEBOX_H
