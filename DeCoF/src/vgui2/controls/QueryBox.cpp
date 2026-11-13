//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Creates a Message box with a question in it and yes/no buttons
//
// $NoKeywords: $
//=============================================================================

#include <string.h>

#include <vgui/KeyValues.h>

#include <vgui_controls/QueryBox.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
QueryBox::QueryBox(const char *title, const char *queryText, Panel *parent) : MessageBox(title, queryText, parent)
{
	m_pCancelButton = new Button(this, "CancelButton", "#QueryBox_Cancel");
	m_pCancelButton->SetCommand("Cancel");
	m_pOkButton->SetCommand("OK");
	m_pCancelCommand = NULL;
	m_pOkCommand = NULL;

	m_pOkButton->SetTabPosition(1);
	m_pCancelButton->SetTabPosition(2);
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
QueryBox::QueryBox(const wchar_t *wszTitle, const wchar_t *wszQueryText, Panel *parent) : MessageBox(wszTitle, wszQueryText, parent)
{
	m_pCancelButton = new Button(this, "CancelButton", "#QueryBox_Cancel");
	m_pCancelButton->SetCommand("Cancel");
	m_pOkButton->SetCommand("OK");
	m_pCancelCommand = NULL;
	m_pOkCommand = NULL;

	m_pOkButton->SetTabPosition(1);
	m_pCancelButton->SetTabPosition(2);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
QueryBox::~QueryBox()
{
	delete m_pCancelButton;

	if (m_pOkCommand)
	{
		m_pOkCommand->deleteThis();
	}
	if (m_pCancelCommand)
	{
		m_pCancelCommand->deleteThis();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Layout the window for drawing
//-----------------------------------------------------------------------------
void QueryBox::PerformLayout()
{
	BaseClass::PerformLayout();

	int boxWidth, boxTall;
	GetSize(boxWidth, boxTall);

	int x, y, wide, tall;
	GetClientArea(x, y, wide, tall);
	wide += x;
	tall += y;

	int oldWide, oldTall;
	m_pCancelButton->GetSize(oldWide, oldTall);

	int btnWide, btnTall;
	m_pCancelButton->GetContentSize(btnWide, btnTall);
	btnWide = max(oldWide, btnWide + 10);
	btnTall = max(oldTall, btnTall + 10);
	m_pCancelButton->SetSize(btnWide, btnTall);

	GetSize(boxWidth, boxTall);

	m_pOkButton->SetPos((wide / 2) - (m_pOkButton->GetWide()) - 1 + x, tall - m_pOkButton->GetTall() - 15);
	m_pCancelButton->SetPos((wide / 2) + x + 16, tall - m_pCancelButton->GetTall() - 15);
}

//-----------------------------------------------------------------------------
// Purpose: Handles command text from the buttons
//			Deletes self when closed
//-----------------------------------------------------------------------------
void QueryBox::OnCommand(const char *command)
{
	if (!stricmp(command, "OK"))
	{
		OnCommand("Close");

		if (m_pOkCommand)
		{
			PostActionSignal(m_pOkCommand->MakeCopy());
		}

		MarkForDeletion();
	}
	else if (!stricmp(command, "Cancel"))
	{
		OnCommand("Close");

		if (m_pCancelCommand)
		{
			PostActionSignal(m_pCancelCommand->MakeCopy());
		}

		MarkForDeletion();
	}

	BaseClass::OnCommand(command);
}

//-----------------------------------------------------------------------------
// Purpose: Set the keyvalues to send when ok button is hit
//-----------------------------------------------------------------------------
void QueryBox::SetOKCommand(KeyValues *keyValues)
{
	if (m_pOkCommand)
	{
		m_pOkCommand->deleteThis();
	}

	m_pOkCommand = keyValues;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the value of an integer in the ok command
//-----------------------------------------------------------------------------
void QueryBox::SetOKCommandValue(const char *keyName, int value)
{
	if (!m_pOkCommand)
	{
		m_pOkCommand = new KeyValues("Command");
	}

	m_pOkCommand->SetInt(keyName, value);
}

//-----------------------------------------------------------------------------
// Purpose: Set the keyvalues to send when the cancel button is hit
//-----------------------------------------------------------------------------
void QueryBox::SetCancelCommand(KeyValues *keyValues)
{
	if (m_pCancelCommand)
	{
		m_pCancelCommand->deleteThis();
	}

	m_pCancelCommand = keyValues;
}

//-----------------------------------------------------------------------------
// Purpose: Set the text on the Cancel button
//-----------------------------------------------------------------------------
void QueryBox::SetCancelButtonText(const char *buttonText)
{
	m_pCancelButton->SetText(buttonText);
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Set the text on the Cancel button
//-----------------------------------------------------------------------------
void QueryBox::SetCancelButtonText(const wchar_t *wszButtonText)
{
	m_pCancelButton->SetText(wszButtonText);
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Escape closes the box like the cancel button
//-----------------------------------------------------------------------------
void QueryBox::OnKeyCodeTyped(KeyCode code)
{
	if (code == KEY_ESCAPE)
	{
		OnCommand("Cancel");
	}
	else
	{
		Frame::OnKeyCodeTyped(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void QueryBox::SetKeyFocusOnCancelButton()
{
	m_pCancelButton->RequestFocus();
}
