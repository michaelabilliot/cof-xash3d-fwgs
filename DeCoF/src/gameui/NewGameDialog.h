//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef NEWGAMEDIALOG_H
#define NEWGAMEDIALOG_H

#ifdef _WIN32
#pragma once
#endif

#include "taskframe.h"

namespace vgui2
{
class RadioButton;
};

//-----------------------------------------------------------------------------
// Purpose: Displays the new game options
//-----------------------------------------------------------------------------
class CNewGameDialog : public CTaskFrame
{
	typedef CTaskFrame BaseClass;

public:
	CNewGameDialog( vgui2::Panel *parent );
	~CNewGameDialog();

protected:
	virtual void OnCommand( const char *command );
	virtual void OnClose();

private:
	int m_nPlayMode;

	vgui2::RadioButton *m_pTraining;
	vgui2::RadioButton *m_pEasy;
	vgui2::RadioButton *m_pMedium;
	vgui2::RadioButton *m_pHard;
};

#endif // NEWGAMEDIALOG_H
