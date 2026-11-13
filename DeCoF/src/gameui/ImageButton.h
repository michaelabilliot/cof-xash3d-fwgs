//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: A button that draws a texture
//
// $NoKeywords: $
//=============================================================================

#ifndef IMAGEBUTTON_H
#define IMAGEBUTTON_H

#ifdef _WIN32
#pragma once
#endif

#include <Color.h>
#include <vgui_controls/Button.h>

//-----------------------------------------------------------------------------
// Purpose: A button with a texture inside its padding
//-----------------------------------------------------------------------------
class CImageButton : public vgui2::Button
{
public:
	CImageButton( vgui2::Panel *parent, const char *panelName, const char *text );

	virtual void PaintBackground();
	virtual void SetTexture( const char *texture );
	virtual void ForceReload();

	void SetImagePadding( int pad );
	void SetFillPadding( int pad );

	virtual Color GetImageColor();
	virtual void ForceUpload();

private:
	typedef vgui2::Button BaseClass;

	bool m_bUploaded;
	int m_nTextureId;
	char m_szTexture[128];
	int m_padImage;
	int m_padFill;
};

#endif // IMAGEBUTTON_H
