//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef CONTENTCONTROLDIALOG_H
#define CONTENTCONTROLDIALOG_H

#ifdef _WIN32
#pragma once
#endif

#include "taskframe.h"

namespace vgui2
{
class Button;
class Label;
class TextEntry;
};

//-----------------------------------------------------------------------------
// Purpose: Dialog for setting the content control (gore) password
//-----------------------------------------------------------------------------
class CContentControlDialog : public CTaskFrame
{
	typedef CTaskFrame BaseClass;

public:
	CContentControlDialog( vgui2::Panel *parent );
	~CContentControlDialog();

	virtual void OnCommand( const char *command );
	virtual void OnClose();
	virtual void Activate();

	void ResetPassword();
	void ApplyPassword();
	bool IsPasswordEnabledInDialog();
	bool IsPasswordEnabled() { return ( m_szGorePW[0] != 0 ); }

protected:
	void WriteToken( const char *str );
	void UpdateContentControlStatus( void );

	void Explain( char const *fmt, ... );

	char *BinPrintf( unsigned char *buf, int nLen );
	void HashPassword( const char *newPW, char *hashBuffer );
	bool EnablePassword( const char *newPW );
	bool DisablePassword( const char *oldPW );

	enum
	{
		MAX_GORE_PW = 64,
	};

	char m_szGorePW[ MAX_GORE_PW ];

	bool m_bDefaultPassword;
	vgui2::Label *m_pStatus;
	vgui2::Button *m_pOK;
	vgui2::TextEntry *m_pPassword;
	vgui2::Label *m_pPasswordLabel;
	vgui2::Label *m_pPassword2Label;
	vgui2::TextEntry *m_pPassword2;

	vgui2::Label *m_pExplain;
};

#endif // CONTENTCONTROLDIALOG_H
