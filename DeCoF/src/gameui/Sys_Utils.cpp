//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: wraps windows system calls so that windows.h doesn't need to be included
//
// $NoKeywords: $
//=============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Sys_Utils.h"

const unsigned int SYS_NO_ERROR = NO_ERROR;
const unsigned int SYS_ERROR_INVALID_HANDLE = ERROR_INVALID_HANDLE;

const unsigned int SYS_WAIT_OBJECT_0 = WAIT_OBJECT_0;
const unsigned int SYS_WAIT_ABANDONED = WAIT_ABANDONED;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Sys_SetLastError( unsigned long error )
{
	::SetLastError( error );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
unsigned long Sys_GetLastError()
{
	return ::GetLastError();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
WHANDLE Sys_CreateMutex( const char *mutexName )
{
	return (WHANDLE)::CreateMutex( NULL, FALSE, mutexName );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Sys_ReleaseMutex( WHANDLE mutexHandle )
{
	::ReleaseMutex( (HANDLE)mutexHandle );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
unsigned int Sys_WaitForSingleObject( WHANDLE mutexHandle, int milliseconds )
{
	return ::WaitForSingleObject( (HANDLE)mutexHandle, milliseconds );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
unsigned int Sys_RegisterWindowMessage( const char *msgName )
{
	return ::RegisterWindowMessage( msgName );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
WHANDLE Sys_FindWindow( const char *className, const char *windowName )
{
	return (WHANDLE)::FindWindow( className, windowName );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Sys_EnumWindows( void *callbackFunction, int lparam )
{
	::EnumWindows( (WNDENUMPROC)callbackFunction, lparam );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Sys_GetWindowText( WHANDLE wnd, char *buffer, int bufferSize )
{
	::GetWindowText( (HWND)wnd, buffer, bufferSize - 1 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Sys_PostMessage( WHANDLE wnd, unsigned int msg, unsigned int wParam, unsigned int lParam )
{
	::PostMessageA( (HWND)wnd, msg, wParam, lParam );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Sys_SetCursorPos( int x, int y )
{
	::SetCursorPos( x, y );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
WHANDLE Sys_CreateWindowEx( const char *windowName )
{
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Sys_DestroyWindow( WHANDLE wnd )
{
}
