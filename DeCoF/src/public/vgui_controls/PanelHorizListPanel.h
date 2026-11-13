//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef PANELHORIZLISTPANEL_H
#define PANELHORIZLISTPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui/Dar.h>
#include <vgui_controls/Panel.h>

class KeyValues;

namespace vgui2
{

class ScrollBar;

//-----------------------------------------------------------------------------
// Purpose: A horizontal list of child panels with a scroll bar underneath
//-----------------------------------------------------------------------------
class PanelHorizListPanel : public Panel
{
public:
	typedef struct dataitem_s
	{
		// Always store a panel pointer
		Panel *panel;
	} DATAITEM;

	PanelHorizListPanel(vgui2::Panel *parent, char const *panelName);
	~PanelHorizListPanel();

	// DATA & ROW HANDLING
	// The list now owns the panel
	virtual int computeHPixelsNeeded();
	virtual int AddItem(Panel *panel);
	virtual int GetItemCount();
	virtual Panel *GetItem(int itemIndex); // returns pointer to data the row holds
	virtual void RemoveItem(int itemIndex); // removes an item from the table (changing the indices of all following items)
	virtual void DeleteAllItems(); // clears and deletes all the memory used by the data items

	void GetSliderRange(int &low, int &high);
	int GetSliderPosition();
	void SetSliderPosition(int position);

	// painting
	virtual vgui2::Panel *GetCellRenderer(int row);

	// overrides
	virtual void OnSliderMoved(int position);

	DECLARE_PANELMAP();

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);

	vgui2::Panel *GetEmbedded()
	{
		return _embedded;
	}

	vgui2::ScrollBar *GetScrollBar()
	{
		return _hbar;
	}

	DATAITEM *GetDataItem(int itemIndex);

protected:
	virtual void PerformLayout();
	virtual void PaintBackground();
	virtual void OnMouseWheeled(int delta);

private:
	// list of the column headers
	vgui2::Dar<DATAITEM *> _dataItems;
	vgui2::ScrollBar *_hbar;
	vgui2::Panel *_embedded;

	int m_itemPad;

	int _tableStartX;
	int _tableStartY;
};

} // namespace vgui2

#endif // PANELHORIZLISTPANEL_H
