//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef DEMOEDITDIALOG_H
#define DEMOEDITDIALOG_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include "../HLTV/common/DirectorCmd.h"

namespace vgui = vgui2;

class IEngineWrapper;
class IDemoPlayer;
class CLabeledCommandComboBox;

namespace vgui2
{
class Button;
class Label;
class TextEntry;
};

#define MAX_DATA_FIELDS	8

//-----------------------------------------------------------------------------
// Purpose: edits one director command of the demo being played
//-----------------------------------------------------------------------------
class CDemoEditDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CDemoEditDialog, vgui::Frame );

public:
	CDemoEditDialog( vgui::Panel *parent, const char *name, IEngineWrapper *engine, IDemoPlayer *demoPlayer );
	~CDemoEditDialog();

	void SetCommand( DirectorCmd *cmd );

	virtual void OnCommand( const char *command );

protected:
	void OnOK();
	void OnGetView();
	void OnGetTime();
	void OnUpdate();

	MESSAGE_FUNC( OnTypeChanged, "ControlModified" );

	void UpdateDrcCmd();
	void ClearDataFields();

	void SetDataField( int index, char *name, char *value );
	void SetDataFieldf( int index, char *name, float value );
	void SetDataFieldi( int index, char *name, int value );

	char *GetDataFields( int index );
	int GetDataFieldx( int index );
	int GetDataFieldi( int index );
	float GetDataFieldf( int index );
	void GetDataField3f( int index, float *f1, float *f2, float *f3 );
	void GetDataField2f( int index, float *f1, float *f2 );

	IEngineWrapper				*m_Engine;
	IDemoPlayer					*m_DemoPlayer;
	DirectorCmd					m_DrcCmd;
	bool						m_ChangingData;

	CLabeledCommandComboBox		*m_TypeBox;
	vgui::TextEntry				*m_TimeStamp;
	vgui::Button				*m_GetViewButton;
	vgui::TextEntry				*m_DataEntries[ MAX_DATA_FIELDS ];
	vgui::Label					*m_DataLabels[ MAX_DATA_FIELDS ];
};

#endif // DEMOEDITDIALOG_H
