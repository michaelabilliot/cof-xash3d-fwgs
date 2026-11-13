//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <wchar.h>

#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui/KeyValues.h>
#include <vgui/MouseCode.h>

#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/PanelHorizListPanel.h>
#include <vgui_controls/ScrollBar.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: scroll bar whose buttons use the dim text color when pressed
//-----------------------------------------------------------------------------
class ScrollBarReversedButtons : public ScrollBar
{
public:
	ScrollBarReversedButtons(Panel *parent, const char *panelName, bool vertical);
	virtual void ApplySchemeSettings(IScheme *pScheme);
};

ScrollBarReversedButtons::ScrollBarReversedButtons(Panel *parent, const char *panelName, bool vertical) : ScrollBar(parent, panelName, vertical)
{
}

void ScrollBarReversedButtons::ApplySchemeSettings(IScheme *pScheme)
{
	ScrollBar::ApplySchemeSettings(pScheme);

	Button *pButton;
	pButton = GetButton(0);
	pButton->SetArmedColor(pButton->GetSchemeColor("DimBaseText", pScheme), pButton->GetBgColor());
	pButton->SetDepressedColor(pButton->GetSchemeColor("DimBaseText", pScheme), pButton->GetBgColor());
	pButton->SetDefaultColor(pButton->GetFgColor(), pButton->GetBgColor());

	pButton = GetButton(1);
	pButton->SetArmedColor(pButton->GetSchemeColor("DimBaseText", pScheme), pButton->GetBgColor());
	pButton->SetDepressedColor(pButton->GetSchemeColor("DimBaseText", pScheme), pButton->GetBgColor());
	pButton->SetDefaultColor(pButton->GetFgColor(), pButton->GetBgColor());
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
PanelHorizListPanel::PanelHorizListPanel(vgui2::Panel *parent, char const *panelName) : Panel(parent, panelName)
{
	SetBounds(0, 0, 100, 100);

	_hbar = new ScrollBarReversedButtons(this, "CPanelListPanelHScroll", false);
	_hbar->SetBounds(0, 0, 20, 20);
	_hbar->SetVisible(false);
	_hbar->AddActionSignalTarget(this);

	_embedded = new Panel(this, "PanelListEmbedded");
	_embedded->SetBounds(0, 0, 20, 20);
	_embedded->SetPaintBackgroundEnabled(false);
	_embedded->SetPaintBorderEnabled(false);

	m_itemPad = 5;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
PanelHorizListPanel::~PanelHorizListPanel()
{
	// free data from table
	DeleteAllItems();
}

//-----------------------------------------------------------------------------
// Purpose: counts the total horizontal pixels
//-----------------------------------------------------------------------------
int PanelHorizListPanel::computeHPixelsNeeded()
{
	int pixels = m_itemPad;
	DATAITEM *item;
	Panel *panel;
	for (int i = 0; i < _dataItems.GetCount(); i++)
	{
		item = _dataItems[i];
		if (!item)
			continue;

		panel = item->panel;
		if (!panel)
			continue;

		if (!panel->IsVisible())
			continue;

		int w = panel->GetWide();
		pixels += w + m_itemPad;
	}

	return pixels;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the panel to use to render a cell
//-----------------------------------------------------------------------------
Panel *PanelHorizListPanel::GetCellRenderer(int row)
{
	DATAITEM *item = _dataItems[row];
	if (item)
	{
		Panel *panel = item->panel;
		return panel;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: adds a panel to the end of the list, the list now owns it
//-----------------------------------------------------------------------------
int PanelHorizListPanel::AddItem(Panel *panel)
{
	InvalidateLayout();

	DATAITEM *newitem = new DATAITEM;
	newitem->panel = panel;
	panel->SetParent(_embedded);
	return _dataItems.PutElement(newitem);
}

//-----------------------------------------------------------------------------
// Purpose: iteration accessor
//-----------------------------------------------------------------------------
int PanelHorizListPanel::GetItemCount()
{
	return _dataItems.GetCount();
}

//-----------------------------------------------------------------------------
// Purpose: returns pointer to data the itemIndex holds
//-----------------------------------------------------------------------------
Panel *PanelHorizListPanel::GetItem(int itemIndex)
{
	if (itemIndex < 0 || itemIndex >= _dataItems.GetCount())
		return NULL;

	return _dataItems[itemIndex]->panel;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
PanelHorizListPanel::DATAITEM *PanelHorizListPanel::GetDataItem(int itemIndex)
{
	if (itemIndex < 0 || itemIndex >= _dataItems.GetCount())
		return NULL;

	return _dataItems[itemIndex];
}

//-----------------------------------------------------------------------------
// Purpose: removes an item from the list
//-----------------------------------------------------------------------------
void PanelHorizListPanel::RemoveItem(int itemIndex)
{
	DATAITEM *item = _dataItems[itemIndex];
	delete item->panel;
	delete item;
	_dataItems.RemoveElementAt(itemIndex);

	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: clears and deletes all the memory used by the data items
//-----------------------------------------------------------------------------
void PanelHorizListPanel::DeleteAllItems()
{
	for (int i = 0; i < _dataItems.GetCount(); i++)
	{
		if (_dataItems[i])
		{
			delete _dataItems[i]->panel;
		}
		delete _dataItems[i];
	}
	_dataItems.RemoveAll();

	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: scrolls the list with the mouse wheel
//-----------------------------------------------------------------------------
void PanelHorizListPanel::OnMouseWheeled(int delta)
{
	int val = _hbar->GetValue();
	val -= (delta * 3 * 5);
	_hbar->SetValue(val);
}

//-----------------------------------------------------------------------------
// Purpose: relayouts out the panel after any internal changes
//-----------------------------------------------------------------------------
void PanelHorizListPanel::PerformLayout()
{
	int wide, tall;
	GetSize(wide, tall);

	int hpixels = computeHPixelsNeeded();
	int range = wide * 3 / 4;

	int oldPos = _hbar->GetValue();
	_hbar->SetVisible(true);
	_hbar->SetEnabled(false);
	_hbar->SetRange(0, hpixels - wide + range);
	_hbar->SetRangeWindow(range);
	_hbar->SetButtonPressedScrollValue(24);
	_hbar->SetPos(0, tall - 20);
	_hbar->SetSize(wide - 2, 18);
	_hbar->InvalidateLayout();
	_hbar->SetValue(oldPos);

	int left = _hbar->GetValue();
	_embedded->SetPos(-left, 0);
	_embedded->SetSize(hpixels, tall - 20);

	// now lay out the controls on the embedded panel
	int x = m_itemPad;
	int w = 0;
	for (int i = 0; i < _dataItems.GetCount(); i++)
	{
		DATAITEM *item = _dataItems[i];
		if (!item || !item->panel)
			continue;

		if (!item->panel->IsVisible())
			continue;

		w = item->panel->GetWide();
		item->panel->SetBounds(x, m_itemPad, w, tall - 20 - m_itemPad * 2);
		w += m_itemPad;
		x += w;
	}
}

//-----------------------------------------------------------------------------
// Purpose: draws background
//-----------------------------------------------------------------------------
void PanelHorizListPanel::PaintBackground()
{
	Panel::PaintBackground();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void PanelHorizListPanel::ApplySchemeSettings(IScheme *pScheme)
{
	Panel::ApplySchemeSettings(pScheme);

	SetBorder(pScheme->GetBorder("ButtonDepressedBorder"));
	SetBgColor(GetSchemeColor("LabelBgColor", GetSchemeColor("WindowDisabledBgColor", pScheme), pScheme));

	_hbar->GetButton(0)->SetArmedSound("sound/UI/buttonrollover.wav");
	_hbar->GetButton(0)->SetDepressedSound("sound/UI/buttonclick.wav");
	_hbar->GetButton(0)->SetReleasedSound("sound/UI/buttonclickrelease.wav");

	_hbar->GetButton(1)->SetArmedSound("sound/UI/buttonrollover.wav");
	_hbar->GetButton(1)->SetDepressedSound("sound/UI/buttonclick.wav");
	_hbar->GetButton(1)->SetReleasedSound("sound/UI/buttonclickrelease.wav");

	_hbar->SetBorder(pScheme->GetBorder("BrowserBorder"));
}

//-----------------------------------------------------------------------------
// Purpose: Called when the scroll bar moves
//-----------------------------------------------------------------------------
void PanelHorizListPanel::OnSliderMoved(int position)
{
	InvalidateLayout();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: gets the range of the scroll bar
//-----------------------------------------------------------------------------
void PanelHorizListPanel::GetSliderRange(int &low, int &high)
{
	_hbar->GetRange(low, high);
}

//-----------------------------------------------------------------------------
// Purpose: gets the position of the scroll bar
//-----------------------------------------------------------------------------
int PanelHorizListPanel::GetSliderPosition()
{
	return _hbar->GetValue();
}

//-----------------------------------------------------------------------------
// Purpose: sets the position of the scroll bar
//-----------------------------------------------------------------------------
void PanelHorizListPanel::SetSliderPosition(int position)
{
	_hbar->SetValue(position);
}

//-----------------------------------------------------------------------------
// Purpose: Message mapping
//-----------------------------------------------------------------------------
MessageMapItem_t PanelHorizListPanel::m_MessageMap[] =
{
	{ "ScrollBarSliderMoved", (MessageFunc_t)(void (PanelHorizListPanel::*)(void))&PanelHorizListPanel::OnSliderMoved, 1, DATATYPE_INT, "position" },
};

IMPLEMENT_PANELMAP( PanelHorizListPanel, Panel );
