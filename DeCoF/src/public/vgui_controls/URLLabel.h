//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef URLLABEL_H
#define URLLABEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Label.h>

namespace vgui2
{

//-----------------------------------------------------------------------------
// Purpose: Label that opens a web page when clicked
//-----------------------------------------------------------------------------
class URLLabel : public Label
{
	DECLARE_CLASS_SIMPLE( URLLabel, Label );

public:
	URLLabel(Panel *parent, const char *panelName, const char *text, const char *pszURL);
	URLLabel(Panel *parent, const char *panelName, const wchar_t *wszText, const char *pszURL);
	~URLLabel();

	void SetURL(const char *pszURL);

protected:
	virtual void OnMousePressed(MouseCode code);
	virtual void ApplySettings( KeyValues *inResourceData );
	virtual void GetSettings( KeyValues *outResourceData );
	virtual const char *GetDescription( void );
	virtual void ApplySchemeSettings(IScheme *pScheme);

private:
	char	*m_pszURL;
	int		m_iURLSize;
	bool	m_bUnderline;
};

} // namespace vgui2

#endif // URLLABEL_H
