//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef IMAGEPANEL_H
#define IMAGEPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

namespace vgui2
{

class IImage;

//-----------------------------------------------------------------------------
// Purpose: Panel that holds a single image
//-----------------------------------------------------------------------------
class ImagePanel : public Panel
{
	DECLARE_CLASS_SIMPLE( ImagePanel, Panel );

public:
	ImagePanel(Panel *parent, const char *name);
	~ImagePanel();

	virtual void SetImage(IImage *image);
	virtual IImage *GetImage();

protected:
	virtual void PaintBackground();
	virtual void GetSettings(KeyValues *outResourceData);
	virtual void ApplySettings(KeyValues *inResourceData);
	virtual const char *GetDescription();
	virtual void OnSizeChanged(int newWide, int newTall);
	virtual void ApplySchemeSettings( IScheme *pScheme );

private:
	IImage *m_pImage;
	char *m_pszImageName;
	char *m_pszColorName;
	bool m_bScaleImage;
	Color m_FillColor;
};

} // namespace vgui2

#endif // IMAGEPANEL_H
