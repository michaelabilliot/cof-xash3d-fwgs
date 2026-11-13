//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef DEMOPLAYERDIALOG_H
#define DEMOPLAYERDIALOG_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/PHandle.h>
#include "BaseSystemModule.h"

class IEngineWrapper;
class IDemoPlayer;
class IBaseSystem;
class IWorld;

namespace vgui2
{
class Button;
class Label;
class Slider;
class ToggleButton;
};

//-----------------------------------------------------------------------------
// Purpose: the HLTV demo player controls
//-----------------------------------------------------------------------------
class CDemoPlayerDialog : public vgui2::Frame, public BaseSystemModule
{
	typedef vgui2::Frame BaseClass;

public:
	CDemoPlayerDialog( vgui2::Panel *parent );
	~CDemoPlayerDialog();

	virtual void OnThink();
	virtual void OnClose();
	virtual void OnCommand( const char *command );
	virtual void OnMessage( const KeyValues *params, vgui2::VPANEL fromPanel );

	// ISystemModule
	void ReceiveSignal( ISystemModule *module, unsigned int signal, void *data );

protected:
	void Update();
	bool LoadModules();

	void OnPause();
	void OnPlay();
	void OnStart();
	void OnEnd();
	void OnSlower();
	void OnFaster();
	void OnLoad();
	void OnStop();
	void OnNextFrame( int direction );
	void OnEvents();
	void OnSave();

	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );

	vgui2::Label		*m_pLableTimeCode;
	vgui2::Slider		*m_pTimeSlider;
	vgui2::ToggleButton	*m_MasterButton;

	vgui2::Button		*m_pButtonPlay;
	vgui2::Button		*m_pButtonStepF;
	vgui2::Button		*m_pButtonFaster;
	vgui2::Button		*m_pButtonSlower;
	vgui2::Button		*m_pButtonStepB;
	vgui2::Button		*m_pButtonPause;
	vgui2::Button		*m_pButtonStart;
	vgui2::Button		*m_pButtonEnd;
	vgui2::Button		*m_pButtonLoad;
	vgui2::Button		*m_pButtonStop;

	vgui2::DHANDLE< vgui2::Frame > m_hDemoPlayerFileDialog;
	vgui2::DHANDLE< vgui2::Frame > m_hDemoEventsDialog;

	IEngineWrapper		*m_Engine;
	IDemoPlayer			*m_DemoPlayer;
	IBaseSystem			*m_System;
	IWorld				*m_World;

	float				m_NextTimeScale;
	int					m_lastSliderTime;
};

#endif // DEMOPLAYERDIALOG_H
