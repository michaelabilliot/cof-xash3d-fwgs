//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: names mouse button inputs
//
// $NoKeywords: $
//=============================================================================

#ifndef MOUSECODE_H
#define MOUSECODE_H

#ifdef _WIN32
#pragma once
#endif

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: names mouse button inputs
//-----------------------------------------------------------------------------
enum MouseCode
{
	MOUSE_LEFT = 0,
	MOUSE_RIGHT,
	MOUSE_MIDDLE,
	MOUSE_4,
	MOUSE_5,
	MOUSE_LAST,
};

} // namespace vgui2

#endif // MOUSECODE_H
