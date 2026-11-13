//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <string.h>
#include <wchar.h>

#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui/KeyValues.h>
#include <vgui/MouseCode.h>

#include <vgui_controls/Button.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/PropertySheet.h>

using namespace vgui2;

template <class T>
inline T max_guess(T a, T b)
{
	if (a > b)
		return a;
	return b;
}

namespace vgui2
{

//-----------------------------------------------------------------------------
// Purpose: A single tab
//-----------------------------------------------------------------------------
class PageTab : public Button
{
	typedef Button BaseClass;

private:
	bool _active;
	Color _textColor;
	Color _dimTextColor;
	int m_bMaxTabWidth;

public:
	PageTab(Panel *parent, const char *panelName, const char *text, int maxTabWidth) : Button(parent, panelName, text)
	{
		SetCommand(new KeyValues("TabPressed"));
		_active = false;
		m_bMaxTabWidth = maxTabWidth;
	}

	~PageTab()
	{
	}

	virtual void ApplySchemeSettings(IScheme *pScheme)
	{
		BaseClass::ApplySchemeSettings(pScheme);

		_textColor = GetSchemeColor("BrightControlText", GetFgColor(), pScheme);
		_dimTextColor = GetSchemeColor("FgColorDim", GetFgColor(), pScheme);

		int wide, tall;
		int contentWide, contentTall;
		GetSize(wide, tall);
		GetContentSize(contentWide, contentTall);

		wide = max_guess(m_bMaxTabWidth, contentWide + 10); // 10 = 5 pixels margin on each side
		SetSize(wide, tall);
	}

	virtual IBorder *GetBorder(bool depressed, bool armed, bool selected, bool keyfocus)
	{
		IScheme *pScheme = scheme()->GetIScheme(GetScheme());
		if (_active)
		{
			return pScheme->GetBorder("TabActiveBorder");
		}
		return pScheme->GetBorder("TabBorder");
	}

	virtual Color GetButtonFgColor()
	{
		if (_active)
		{
			return _textColor;
		}
		else
		{
			return _dimTextColor;
		}
	}

	virtual void SetActive(bool state)
	{
		_active = state;
		InvalidateLayout();
		Repaint();
	}

	virtual bool CanBeDefaultButton()
	{
		return false;
	}

	// fire the action signal when the mouse is pressed down instead of on release
	virtual void OnMousePressed(MouseCode code)
	{
		// check for context menu open
		if (code == MOUSE_RIGHT)
		{
			PostActionSignal(new KeyValues("OpenContextMenu", "itemID", -1));
		}
		else
		{
			if (!IsEnabled())
				return;

			if (!IsMouseClickEnabled(code))
				return;

			if (IsUseCaptureMouseEnabled())
			{
				RequestFocus();
				FireActionSignal();
				SetSelected(true);
				Repaint();

				// lock mouse input to going to this button
				input()->SetMouseCapture(GetVPanel());
			}
		}
	}

	virtual void OnMouseReleased(MouseCode code)
	{
		// ensure mouse capture gets released
		if (IsUseCaptureMouseEnabled())
		{
			input()->SetMouseCapture(NULL);
		}

		// make sure the button gets unselected
		SetSelected(false);
		Repaint();
	}
};

} // namespace vgui2

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
PropertySheet::PropertySheet(Panel *parent, const char *panelName) : BaseClass(parent, panelName)
{
	_activePage = NULL;
	_activeTab = NULL;
	_tabWidth = 64;
	_activeTabIndex = 0;
	_showTabs = true;
	_combo = NULL;
	_tabFocus = false;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor, associates pages with a combo box instead of tabs
//-----------------------------------------------------------------------------
PropertySheet::PropertySheet(Panel *parent, const char *panelName, ComboBox *combo) : BaseClass(parent, panelName)
{
	_activePage = NULL;
	_activeTab = NULL;
	_tabWidth = 64;
	_activeTabIndex = 0;
	_combo = combo;
	_combo->AddActionSignalTarget(this);
	_showTabs = false;
	_tabFocus = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
PropertySheet::~PropertySheet()
{
}

//-----------------------------------------------------------------------------
// Purpose: adds a page to the sheet
//-----------------------------------------------------------------------------
void PropertySheet::AddPage(Panel *page, const char *title)
{
	if (!page)
		return;

	// don't add the page if we already have it
	if (_pages.HasElement(page))
		return;

	PageTab *tab = new PageTab(this, "tab", title, _tabWidth);
	if (_showTabs)
	{
		tab->AddActionSignalTarget(this);
	}
	else if (_combo)
	{
		_combo->AddItem(title, NULL);
	}
	_pageTabs.AddElement(tab);
	_pages.AddElement(page);

	page->SetParent(this);
	page->AddActionSignalTarget(this);
	PostMessage(page, new KeyValues("ResetData"));

	page->SetVisible(false);
}

//-----------------------------------------------------------------------------
// Purpose: makes the page the active one
//-----------------------------------------------------------------------------
void PropertySheet::SetActivePage(Panel *page)
{
	// walk the list looking for this page
	int index = _pages.FindElement(page);
	if (index < 0)
		return;

	ChangeActiveTab(index);
}

//-----------------------------------------------------------------------------
// Purpose: sets the width of the page tab buttons
//-----------------------------------------------------------------------------
void PropertySheet::SetTabWidth(int pixels)
{
	_tabWidth = pixels;
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: reloads the data in all the property page
//-----------------------------------------------------------------------------
void PropertySheet::ResetAllData()
{
	// iterate all the dialogs resetting them
	for (int i = 0; i < _pages.GetCount(); i++)
	{
		ivgui()->PostMessage(_pages[i]->GetVPanel(), new KeyValues("ResetData"), GetVPanel());
	}
}

//-----------------------------------------------------------------------------
// Purpose: Applies any changes made by the dialog
//-----------------------------------------------------------------------------
void PropertySheet::ApplyChanges()
{
	// iterate all the dialogs resetting them
	for (int i = 0; i < _pages.GetCount(); i++)
	{
		ivgui()->PostMessage(_pages[i]->GetVPanel(), new KeyValues("ApplyChanges"), GetVPanel());
	}
}

//-----------------------------------------------------------------------------
// Purpose: gets a pointer to the currently active page
//-----------------------------------------------------------------------------
Panel *PropertySheet::GetActivePage()
{
	return _activePage;
}

//-----------------------------------------------------------------------------
// Purpose: gets a pointer to the currently active tab
//-----------------------------------------------------------------------------
Panel *PropertySheet::GetActiveTab()
{
	return _activeTab;
}

//-----------------------------------------------------------------------------
// Purpose: returns the number of pages in the sheet
//-----------------------------------------------------------------------------
int PropertySheet::GetNumPages()
{
	return _pages.GetCount();
}

//-----------------------------------------------------------------------------
// Purpose: returns the name contained in the active tab
//-----------------------------------------------------------------------------
void PropertySheet::GetActiveTabTitle(char *textOut, int bufferLen)
{
	if (_activeTab)
		_activeTab->GetText(textOut, bufferLen);
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of the tab "i"
//-----------------------------------------------------------------------------
bool PropertySheet::GetTabTitle(int i, char *textOut, int bufferLen)
{
	if (i < 0 && i > _pageTabs.GetCount())
	{
		return false;
	}

	_pageTabs[i]->GetText(textOut, bufferLen);
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: returns the index of the currently active page
//-----------------------------------------------------------------------------
int PropertySheet::GetActivePageNum()
{
	for (int i = 0; i < _pages.GetCount(); i++)
	{
		if (_pages[i] == _activePage)
		{
			return i;
		}
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Forwards focus requests to current active page
//-----------------------------------------------------------------------------
void PropertySheet::RequestFocus(int direction)
{
	if (direction == -1 || direction == 0)
	{
		if (_activePage)
		{
			_activePage->RequestFocus(direction);
			_tabFocus = false;
		}
	}
	else
	{
		if (_showTabs && _activeTab)
		{
			_activeTab->RequestFocus(direction);
			_tabFocus = true;
		}
		else if (_activePage)
		{
			_activePage->RequestFocus(direction);
			_tabFocus = false;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: moves focus back
//-----------------------------------------------------------------------------
bool PropertySheet::RequestFocusPrev(VPANEL panel)
{
	if (_tabFocus || !_showTabs || !_activeTab)
	{
		_tabFocus = false;
		return BaseClass::RequestFocusPrev(panel);
	}
	else
	{
		if (GetVParent())
		{
			PostMessage(GetVParent(), new KeyValues("FindDefaultButton"));
		}
		_activeTab->RequestFocus(-1);
		_tabFocus = true;
		return true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: moves focus forward
//-----------------------------------------------------------------------------
bool PropertySheet::RequestFocusNext(VPANEL panel)
{
	if (!_tabFocus || !_activePage)
	{
		return BaseClass::RequestFocusNext(panel);
	}
	else
	{
		if (!_activeTab)
		{
			return BaseClass::RequestFocusNext(panel);
		}
		else
		{
			_activePage->RequestFocus(1);
			_tabFocus = false;
			return true;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: lays out the dialog
//-----------------------------------------------------------------------------
void PropertySheet::PerformLayout()
{
	BaseClass::PerformLayout();

	if (!_activePage)
	{
		ChangeActiveTab(0);
		if (_activePage)
		{
			_activePage->RequestFocus(0);
		}
	}

	int x, y, wide, tall;
	GetBounds(x, y, wide, tall);
	if (_activePage)
	{
		if (_showTabs)
		{
			_activePage->SetBounds(0, 28, wide, tall - 28);
		}
		else
		{
			_activePage->SetBounds(0, 0, wide, tall);
		}
		_activePage->InvalidateLayout();
	}

	int limit = _pageTabs.GetCount();
	int xtab = 0;

	// draw the visible tabs
	if (_showTabs)
	{
		for (int i = 0; i < limit; i++)
		{
			int width, tall;
			_pageTabs[i]->GetSize(width, tall);
			if (_pageTabs[i] == _activeTab)
			{
				// active tab is taller
				_activeTab->SetBounds(xtab, 2, width, 27);
			}
			else
			{
				_pageTabs[i]->SetBounds(xtab, 4, width, 25);
			}
			_pageTabs[i]->SetVisible(true);
			xtab += (width + 1);
		}
	}
	else
	{
		for (int i = 0; i < limit; i++)
		{
			_pageTabs[i]->SetVisible(false);
		}
	}

	// ensure draw order (page drawing over all the tabs except one)
	if (_activePage)
	{
		_activePage->MoveToFront();
		_activePage->Repaint();
	}
	if (_activeTab)
	{
		_activeTab->MoveToFront();
		_activeTab->Repaint();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Switches the active panel
//-----------------------------------------------------------------------------
void PropertySheet::OnTabPressed(Panel *panel)
{
	// look for the tab in the list
	for (int i = 0; i < _pageTabs.GetCount(); i++)
	{
		if (_pageTabs[i] == panel)
		{
			// flip to the new tab
			ChangeActiveTab(i);
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the panel associated with index i
//-----------------------------------------------------------------------------
Panel *PropertySheet::GetPage(int i)
{
	if (i < 0 && i > _pages.GetCount())
	{
		return NULL;
	}

	return _pages[i];
}

//-----------------------------------------------------------------------------
// Purpose: disables page by name
//-----------------------------------------------------------------------------
void PropertySheet::DisablePage(const char *title)
{
	EnDisPage(title, false);
}

//-----------------------------------------------------------------------------
// Purpose: enables page by name
//-----------------------------------------------------------------------------
void PropertySheet::EnablePage(const char *title)
{
	EnDisPage(title, true);
}

//-----------------------------------------------------------------------------
// Purpose: enabled or disables page by name
//-----------------------------------------------------------------------------
void PropertySheet::EnDisPage(const char *title, bool state)
{
	for (int i = 0; i < _pageTabs.GetCount(); i++)
	{
		if (_showTabs)
		{
			char tmp[50];
			_pageTabs[i]->GetText(tmp, 50);
			if (!strnicmp(title, tmp, strlen(tmp)))
			{
				_pageTabs[i]->SetEnabled(state);
			}
		}
		else
		{
			_combo->SetItemEnabled(title, state);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: deletes the page associated with panel
//-----------------------------------------------------------------------------
void PropertySheet::DeletePage(Panel *panel)
{
	// look for the panel in the list
	if (!_pages.HasElement(panel))
		return;

	int location = _pages.FindElement(panel);
	if (_activePage == panel)
	{
		ChangeActiveTab(location - 1);
	}

	if (_showTabs)
	{
		_pageTabs[location]->RemoveActionSignalTarget(this);
	}

	PageTab *tab = _pageTabs[location];
	_pageTabs.RemoveElement(_pageTabs[location]);
	tab->MarkForDeletion();

	_pages.RemoveElement(panel);
	panel->MarkForDeletion();

	PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: activates the page at index
//-----------------------------------------------------------------------------
void PropertySheet::ChangeActiveTab(int index)
{
	if (!_pages[index])
		return;

	if (_pages[index] == _activePage)
	{
		_activeTab->RequestFocus();
		_tabFocus = true;
		return;
	}

	if (_activePage)
	{
		// notify old page
		ivgui()->PostMessage(_activePage->GetVPanel(), new KeyValues("PageHide"), GetVPanel());
		KeyValues *msg = new KeyValues("PageTabActivated");
		msg->SetPtr("panel", NULL);
		ivgui()->PostMessage(_activePage->GetVPanel(), msg, GetVPanel());
		_activePage->SetVisible(false);
	}

	if (_activeTab)
	{
		_activeTab->SetActive(false);

		// does the old tab have the focus?
		_tabFocus = _activeTab->HasFocus();
	}
	else
	{
		_tabFocus = false;
	}

	// get the new page
	_activePage = _pages[index];
	_activeTab = _pageTabs[index];
	_activeTabIndex = index;

	_activePage->SetVisible(true);
	_activePage->MoveToFront();

	_activeTab->SetVisible(true);
	_activeTab->MoveToFront();
	_activeTab->SetActive(true);

	if (_tabFocus)
	{
		// if a tab already has focus, give the new tab the focus
		_activeTab->RequestFocus();
	}
	else
	{
		// otherwise, give the focus to the page
		_activePage->RequestFocus();
	}

	if (!_showTabs)
	{
		_combo->ActivateItemByRow(index);
	}

	// notify
	ivgui()->PostMessage(_activePage->GetVPanel(), new KeyValues("PageShow"), GetVPanel());

	KeyValues *msg = new KeyValues("PageTabActivated");
	msg->SetPtr("panel", (Panel *)_activeTab);
	ivgui()->PostMessage(_activePage->GetVPanel(), msg, GetVPanel());

	// tell parent
	PostActionSignal(new KeyValues("PageChanged"));

	// Repaint
	InvalidateLayout();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Gets the panel with the specified hotkey, from the current page
//-----------------------------------------------------------------------------
Panel *PropertySheet::HasHotkey(wchar_t key)
{
	if (!_activePage)
		return NULL;

	for (int i = 0; i < _activePage->GetChildCount(); i++)
	{
		Panel *hot = _activePage->GetChild(i)->HasHotkey(key);
		if (hot)
		{
			return hot;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: catches the opencontextmenu event
//-----------------------------------------------------------------------------
void PropertySheet::OnOpenContextMenu()
{
	PostActionSignal(new KeyValues("OpenContextMenu"));
}

//-----------------------------------------------------------------------------
// Purpose: Handle key presses, through tabs.
//-----------------------------------------------------------------------------
void PropertySheet::OnKeyCodeTyped(KeyCode code)
{
	switch (code)
	{
	case KEY_RIGHT:
		ChangeActiveTab(_activeTabIndex + 1);
		break;

	case KEY_LEFT:
		ChangeActiveTab(_activeTabIndex - 1);
		break;

	default:
		BaseClass::OnKeyCodeTyped(code);
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called by the associated combo box (if in that mode), changes the current panel
//-----------------------------------------------------------------------------
void PropertySheet::OnTextChanged(Panel *panel, const wchar_t *wszText)
{
	if (panel == _combo)
	{
		wchar_t tabText_guess[30];
		for (int i = 0; i < _pageTabs.GetCount(); i++)
		{
			tabText_guess[0] = 0;
			_pageTabs[i]->GetText(tabText_guess, 30);
			if (!wcsicmp(wszText, tabText_guess))
			{
				ChangeActiveTab(i);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void PropertySheet::OnCommand(const char *command)
{
	// propogate the close command to our parent
	if (!stricmp(command, "Close") && GetVParent())
	{
		CallParentFunction(new KeyValues("Command", "command", command));
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void PropertySheet::OnApplyButtonEnable()
{
	// tell parent
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void PropertySheet::OnCurrentDefaultButtonSet(Panel *defaultButton)
{
	// forward the message up
	if (GetVParent())
	{
		KeyValues *msg = new KeyValues("CurrentDefaultButtonSet");
		msg->SetPtr("button", defaultButton);
		PostMessage(GetVParent(), msg);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void PropertySheet::OnDefaultButtonSet(Panel *defaultButton)
{
	// forward the message up
	if (GetVParent())
	{
		KeyValues *msg = new KeyValues("DefaultButtonSet");
		msg->SetPtr("button", defaultButton);
		PostMessage(GetVParent(), msg);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void PropertySheet::OnFindDefaultButton()
{
	if (GetVParent())
	{
		PostMessage(GetVParent(), new KeyValues("FindDefaultButton"));
	}
}

//-----------------------------------------------------------------------------
// Purpose: removes all the pages, marks them for deletion
//-----------------------------------------------------------------------------
void PropertySheet::RemoveAllPages()
{
	int c = _pages.GetCount();
	for (int i = c - 1; i >= 0; --i)
	{
		Panel *page = _pages[i];
		if (_showTabs)
		{
			_pageTabs[i]->RemoveActionSignalTarget(this);
		}

		PageTab *tab = _pageTabs[i];
		if (tab == _activeTab)
		{
			_activeTab = NULL;
		}
		_pageTabs.RemoveElementAt(i);
		tab->MarkForDeletion();

		_pages.RemoveElementAt(i);
		page->SetParent((Panel *)NULL);

		if (_activePage == page)
		{
			_activePage = NULL;
			ChangeActiveTab(max_guess(i - 1, 0));
		}
	}

	PerformLayout();
}
