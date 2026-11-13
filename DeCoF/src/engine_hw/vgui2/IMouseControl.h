//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: lets the engine ask the VGUI2 surface whether it owns the mouse
//
// $NoKeywords: $
//=============================================================================

#ifndef IMOUSECONTROL_H
#define IMOUSECONTROL_H

#ifdef _WIN32
#pragma once
#endif

class IMouseControl
{
public:
	virtual bool VGUI2MouseControl( void ) = 0;
	virtual void SetVGUI2MouseControl( bool state ) = 0;
};

extern IMouseControl *mousecontrol;

#endif // IMOUSECONTROL_H
