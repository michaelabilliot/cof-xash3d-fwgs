//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: A list of variable height child panels with a scroll bar
//
// $NoKeywords: $
//=============================================================================

#ifndef PANELLISTPANEL_H
#define PANELLISTPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/Dar.h>
#include <vgui_controls/Panel.h>

namespace vgui2
{
class ScrollBar;
};

namespace vgui = vgui2;

class CPanelListPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CPanelListPanel, vgui::Panel );

public:
	typedef struct dataitem_s
	{
		vgui2::Panel *panel;
	} DATAITEM;

	CPanelListPanel( vgui2::Panel *parent, char const *panelName, bool inverseButtons = false );
	~CPanelListPanel();

	virtual int computeVPixelsNeeded();
	virtual int AddItem( vgui2::Panel *panel );
	virtual int GetItemCount();
	virtual vgui2::Panel *GetItem( int itemIndex );
	virtual void RemoveItem( int itemIndex );
	virtual void DeleteAllItems();
	void SetSliderYOffset( int pixels );
	virtual vgui2::Panel *GetCellRenderer( int row );

	MESSAGE_FUNC_INT( OnSliderMoved, "ScrollBarSliderMoved", position );
	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );

	vgui2::Panel *GetEmbedded()
	{
		return _embedded;
	}

protected:
	DATAITEM *GetDataItem( int itemIndex );

	virtual void PerformLayout();
	virtual void PaintBackground();
	virtual void OnMouseWheeled( int delta );

	vgui2::ScrollBar *GetScrollbar()
	{
		return _vbar;
	}

private:
	vgui2::Dar< DATAITEM * > _dataItems;
	vgui2::ScrollBar *_vbar;
	vgui2::Panel *_embedded;

	int _tableStartX;
	int _tableStartY;
	int _sliderYOffset;
};

#endif // PANELLISTPANEL_H
