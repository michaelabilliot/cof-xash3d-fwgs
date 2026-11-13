//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: the engine functions GameUI calls (the 2013 version: 9 vtable
//			slots, GetCurrentRenderer has three parameters and there is no
//			Key_KeyStringToKeyNum yet)
//
// $NoKeywords: $
//=============================================================================

#ifndef IGAMEUIFUNCS_H
#define IGAMEUIFUNCS_H
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"
#include "vgui/KeyCode.h"

struct vmode_s;

class IGameUIFuncs : public IBaseInterface
{
public:
	virtual bool		IsKeyDown( const char *keyname, bool &isdown ) = 0;
	virtual const char	*Key_NameForKey( int keynum ) = 0;
	virtual const char	*Key_BindingForKey( int keynum ) = 0;
	virtual vgui2::KeyCode	GetVGUI2KeyCodeForBind( const char *bind ) = 0;
	virtual void		GetVideoModes( struct vmode_s **liststart, int *count ) = 0;
	virtual void		GetCurrentVideoMode( int *wide, int *tall, int *bpp ) = 0;
	virtual void		GetCurrentRenderer( char *name, int namelen, int *windowed ) = 0;
	virtual bool		IsConnectedToVACSecureServer( void ) = 0;
};

#define VENGINE_GAMEUIFUNCS_VERSION "VENGINE_GAMEUIFUNCS_VERSION001"

#endif // IGAMEUIFUNCS_H
