// dinput_guess.cpp -- DirectInput mouse and keyboard (after Quake's in_win.c)

#define DIRECTINPUT_VERSION		0x0700

extern "C"
{
#include "quakedef.h"
#include "keys.h"
#include "APIProxy.h"
}

#include <dinput.h>

#ifndef SPI_GETMOUSESPEED
#define SPI_GETMOUSESPEED		112
#endif

extern "C"
{
extern	HWND	*pmainwindow;
extern	int		window_center_x, window_center_y;
}

extern "C"
{
void	ClientDLL_MouseEvent( int mstate );
void	VGui_CallEngineSurfaceAppHandler( void *hwnd, unsigned int msg, unsigned int wparam, long lparam );
}

// DirectInput scan code to engine key
static unsigned char s_DIKToKey_guess[256] =
{
	  0,  27,  49,  50,  51,  52,  53,  54,  55,  56,  57,  48,  45,  61, 127,   9,	// 0x00
	113, 119, 101, 114, 116, 121, 117, 105, 111, 112,  91,  93,  13, 133,  97, 115,	// 0x10
	100, 102, 103, 104, 106, 107, 108,  59,  39,  96, 134,  92, 122, 120,  99, 118,	// 0x20
	 98, 110, 109,  44,  46,  47, 134,  42, 132,  32, 175, 135, 136, 137, 138, 139,	// 0x30
	140, 141, 142, 143, 144, 255,   0, 160, 161, 162, 173, 163, 164, 165, 174, 166,	// 0x40
	167, 168, 170, 171,   0,   0,   0, 145, 146,   0,   0,   0,   0,   0,   0,   0,	// 0x50
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,	// 0x60
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,	// 0x70
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,	// 0x80
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 169, 133,   0,   0,	// 0x90
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,	// 0xA0
	  0,   0,   0,   0,   0, 172,   0,   0, 132,   0,   0,   0,   0,   0,   0,   0,	// 0xB0
	  0,   0,   0,   0,   0,   0,   0, 151, 128, 150,   0, 130,   0, 131,   0, 152,	// 0xC0
	129, 149, 147, 148,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,	// 0xD0
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,	// 0xE0
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,	// 0xF0
};

// DirectInput scan code to Windows virtual key, for the VGUI key messages
static unsigned char s_DIKToVK_guess[256] =
{
	  0,  27,  49,  50,  51,  52,  53,  54,  55,  56,  57,  48, 189, 187,   8,   9,	// 0x00
	 81,  87,  69,  82,  84,  89,  85,  73,  79,  80, 219, 221,  13,  17,  65,  83,	// 0x10
	 68,  70,  71,  72,  74,  75,  76, 186, 222, 192,  16, 220,  90,  88,  67,  86,	// 0x20
	 66,  78,  77, 188, 190, 191,  16, 106,  18,  32,  20, 112, 113, 114, 115, 116,	// 0x30
	117, 118, 119, 120, 121,  19,   0,  36,  38,  33, 109,  37, 101,  39, 107,  35,	// 0x40
	 40,  34,  96, 110,   0,   0,   0, 122, 123,   0,   0,   0,   0,   0,   0,   0,	// 0x50
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,	// 0x60
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,	// 0x70
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,	// 0x80
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  13,  17,   0,   0,	// 0x90
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,	// 0xA0
	  0,   0,   0,   0,   0, 111,   0,   0,  18,   0,   0,   0,   0,   0,   0,   0,	// 0xB0
	  0,   0,   0,   0,   0,   0,   0,  36,  38,  33,   0,  37,   0,  39,   0,  35,	// 0xC0
	 40,  34,  45,  46,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,	// 0xD0
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,	// 0xE0
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,	// 0xF0
};

extern "C"
{

qboolean	g_fUseDInput = false;

static LPDIRECTINPUT		g_pdi;
static LPDIRECTINPUTDEVICE	g_pMouse;
static LPDIRECTINPUTDEVICE	g_pKeyboard_guess;
static DWORD				s_dwDIVersion_guess;
static qboolean				s_bDIInitialized_guess;
static qboolean				s_bMouseActive_guess;

static int		mstate_di;
static int		s_mouseX_guess, s_mouseY_guess;
static int		s_mouseAccel_guess;
static int		s_mouseThreshold1_guess, s_mouseThreshold2_guess;
static int		s_mouseSpeed_guess;

static int		s_keyRepeatDelay_guess;
static int		s_keyRepeatRate_guess;
static int		s_repeatKey_guess;
static int		s_keyDownTime_guess;
static int		s_lastRepeatTime_guess;

int DInput_AcquireMouse_guess( void );
int DInput_AcquireKeyboard_guess( void );
void DInput_ReadSystemParms_guess( void );

void DInput_Init_guess( void )
{
	HINSTANCE	hInst;
	HRESULT		hr;

	g_fUseDInput = true;
	hInst = GetModuleHandle( NULL );

	s_dwDIVersion_guess = 0x0700;
	hr = DirectInputCreate( hInst, s_dwDIVersion_guess, &g_pdi, NULL );
	if ( FAILED( hr ) )
	{
		// try the DirectX 5 interface
		s_dwDIVersion_guess = 0x0500;
		hr = DirectInputCreate( hInst, s_dwDIVersion_guess, &g_pdi, NULL );
		if ( FAILED( hr ) )
			return;
	}

	if ( !DInput_AcquireMouse_guess() )
		return;

	if ( !DInput_AcquireKeyboard_guess() )
		return;

	DInput_ReadSystemParms_guess();
	s_bMouseActive_guess = true;
	s_bDIInitialized_guess = true;
}

void DInput_ReadSystemParms_guess( void )
{
	int		parms_guess[3];
	int		value_guess;

	if ( !SystemParametersInfo( SPI_GETMOUSE, 0, parms_guess, 0 ) )
	{
		parms_guess[0] = 7;
		parms_guess[1] = 0;
		parms_guess[2] = 1;
	}

	if ( COM_CheckParm( "-noforcemparms" ) || COM_CheckParm( "-noforcemspd" ) )
		s_mouseAccel_guess = parms_guess[2];
	else
		s_mouseAccel_guess = 1;

	if ( COM_CheckParm( "-noforcemparms" ) || COM_CheckParm( "-noforcemaccel" ) )
	{
		s_mouseThreshold1_guess = parms_guess[0];
		s_mouseThreshold2_guess = parms_guess[1];
	}
	else
	{
		s_mouseThreshold1_guess = 0;
		s_mouseThreshold2_guess = 0;
	}

	if ( SystemParametersInfo( SPI_GETMOUSESPEED, 0, &s_mouseSpeed_guess, 0 ) )
	{
		if ( s_mouseSpeed_guess < 1 || s_mouseSpeed_guess > 20 )
			s_mouseSpeed_guess = 10;
	}
	else
	{
		s_mouseSpeed_guess = 10;
	}

	if ( SystemParametersInfo( SPI_GETKEYBOARDDELAY, 0, &value_guess, 0 ) )
	{
		switch ( value_guess )
		{
		case 0:
			s_keyRepeatDelay_guess = 250;
			break;
		case 1:
			s_keyRepeatDelay_guess = 500;
			break;
		case 2:
			s_keyRepeatDelay_guess = 750;
			break;
		default:
			s_keyRepeatDelay_guess = 1000;
			break;
		}
	}
	else
	{
		s_keyRepeatDelay_guess = 1000;
	}

	if ( SystemParametersInfo( SPI_GETKEYBOARDSPEED, 0, &value_guess, 0 ) )
	{
		if ( value_guess < 0 )
			value_guess = 0;
		if ( value_guess > 31 )
			value_guess = 31;

		// 0 is about 2.5 repeats a second, 31 about 30
		s_keyRepeatRate_guess = value_guess * -367 / 31 + 400;
	}
	else
	{
		s_keyRepeatRate_guess = 400;
	}
}

int DInput_AcquireMouse_guess( void )
{
	HRESULT		hr;
	DIPROPDWORD	dipdw;

	// obtain an interface to the system mouse device.
	hr = IDirectInput_CreateDevice( g_pdi, GUID_SysMouse, &g_pMouse, NULL );
	if ( FAILED( hr ) )
		return 0;

	// set the data format to "mouse format".
	if ( s_dwDIVersion_guess == 0x0700 )
		hr = IDirectInputDevice_SetDataFormat( g_pMouse, &c_dfDIMouse2 );
	else
		hr = IDirectInputDevice_SetDataFormat( g_pMouse, &c_dfDIMouse );

	if ( FAILED( hr ) )
		return 0;

	// set the cooperativity level.
	hr = IDirectInputDevice_SetCooperativeLevel( g_pMouse, *pmainwindow, DISCL_EXCLUSIVE | DISCL_FOREGROUND );
	if ( FAILED( hr ) )
		return 0;

	// set the buffer size to DINPUT_BUFFERSIZE elements.
	// the buffer size is a DWORD property associated with the device
	dipdw.diph.dwSize = sizeof( DIPROPDWORD );
	dipdw.diph.dwHeaderSize = sizeof( DIPROPHEADER );
	dipdw.diph.dwObj = 0;
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.dwData = 32;

	hr = IDirectInputDevice_SetProperty( g_pMouse, DIPROP_BUFFERSIZE, &dipdw.diph );
	if ( FAILED( hr ) )
		return 0;

	IDirectInputDevice_Acquire( g_pMouse );

	s_mouseX_guess = window_center_x;
	s_mouseY_guess = window_center_y;
	mstate_di = 0;
	return 1;
}

void DInput_ReleaseMouse_guess( void )
{
	if ( g_fUseDInput && s_bDIInitialized_guess && s_bMouseActive_guess && g_pMouse )
	{
		IDirectInputDevice_Unacquire( g_pMouse );
		IDirectInputDevice_Release( g_pMouse );
		g_pMouse = NULL;
	}
}

int DInput_AcquireKeyboard_guess( void )
{
	HRESULT		hr;
	DIPROPDWORD	dipdw;

	hr = IDirectInput_CreateDevice( g_pdi, GUID_SysKeyboard, &g_pKeyboard_guess, NULL );
	if ( FAILED( hr ) )
		return 0;

	hr = IDirectInputDevice_SetDataFormat( g_pKeyboard_guess, &c_dfDIKeyboard );
	if ( FAILED( hr ) )
		return 0;

	hr = IDirectInputDevice_SetCooperativeLevel( g_pKeyboard_guess, *pmainwindow, DISCL_EXCLUSIVE | DISCL_FOREGROUND );
	if ( FAILED( hr ) )
		return 0;

	dipdw.diph.dwSize = sizeof( DIPROPDWORD );
	dipdw.diph.dwHeaderSize = sizeof( DIPROPHEADER );
	dipdw.diph.dwObj = 0;
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.dwData = 32;

	hr = IDirectInputDevice_SetProperty( g_pKeyboard_guess, DIPROP_BUFFERSIZE, &dipdw.diph );
	if ( FAILED( hr ) )
		return 0;

	IDirectInputDevice_Acquire( g_pKeyboard_guess );
	return 1;
}

// the Windows pointer speed and acceleration
int DInput_AccelerateMouse_guess( int delta )
{
	int		ret_guess;
	int		dist_guess;

	ret_guess = delta;
	dist_guess = ( delta > 0 ) ? delta : -delta;

	if ( s_mouseAccel_guess >= 1 && dist_guess > s_mouseThreshold1_guess )
		ret_guess *= 2;

	if ( s_mouseAccel_guess >= 2 && dist_guess > s_mouseThreshold2_guess )
		ret_guess *= 2;

	ret_guess = ret_guess * s_mouseSpeed_guess / 10;
	return ret_guess;
}

void DInput_ReadMouse_guess( void )
{
	DIDEVICEOBJECTDATA	od;
	DWORD				dwElements;
	HRESULT				hr;

	if ( !g_fUseDInput || !s_bDIInitialized_guess || !s_bMouseActive_guess )
		return;

	for ( ;; )
	{
		dwElements = 1;

		hr = IDirectInputDevice_GetDeviceData( g_pMouse, sizeof( DIDEVICEOBJECTDATA ), &od, &dwElements, 0 );
		if ( FAILED( hr ) )
		{
			IDirectInputDevice_Acquire( g_pMouse );
			hr = IDirectInputDevice_GetDeviceData( g_pMouse, sizeof( DIDEVICEOBJECTDATA ), &od, &dwElements, 0 );
		}

		/* Unable to read data or no data available */
		if ( FAILED( hr ) || dwElements == 0 )
			break;

		/* Look at the element to see what happened */
		switch ( od.dwOfs )
		{
		case DIMOFS_X:
			s_mouseX_guess += DInput_AccelerateMouse_guess( od.dwData );
			break;

		case DIMOFS_Y:
			s_mouseY_guess += DInput_AccelerateMouse_guess( od.dwData );
			break;

		case DIMOFS_Z:
			if ( (int)od.dwData > 0 )
			{
				Key_Event( K_MWHEELUP, true );
				Key_Event( K_MWHEELUP, false );
			}
			else
			{
				Key_Event( K_MWHEELDOWN, true );
				Key_Event( K_MWHEELDOWN, false );
			}
			break;

		case DIMOFS_BUTTON0:
			if ( od.dwData )
				mstate_di |= 1;
			else
				mstate_di &= ~1;
			break;

		case DIMOFS_BUTTON1:
			if ( od.dwData )
				mstate_di |= 2;
			else
				mstate_di &= ~2;
			break;

		case DIMOFS_BUTTON2:
			if ( od.dwData )
				mstate_di |= 4;
			else
				mstate_di &= ~4;
			break;

		case DIMOFS_BUTTON3:
			if ( od.dwData )
				mstate_di |= 8;
			else
				mstate_di &= ~8;
			break;

		case DIMOFS_BUTTON4:
			if ( od.dwData )
				mstate_di |= 16;
			else
				mstate_di &= ~16;
			break;
		}
	}
}

void GetMousePos_guess( POINT *ppt )
{
	g_engdstAddrs.pfnGetMousePos( &ppt );

	if ( g_fUseDInput )
	{
		DInput_ReadMouse_guess();
		ppt->x = s_mouseX_guess;
		ppt->y = s_mouseY_guess;
	}
	else
	{
		GetCursorPos( ppt );
	}
}

void SetMousePos_guess( int x, int y )
{
	g_engdstAddrs.pfnSetMousePos( &x, &y );

	if ( g_fUseDInput )
	{
		DInput_ReadMouse_guess();
		s_mouseX_guess = x;
		s_mouseY_guess = y;
	}
	else
	{
		SetCursorPos( x, y );
	}
}

void DInput_KeyEvent_guess( int scancode, int key, int down, int repeat );

void DInput_ReadKeyboard_guess( void )
{
	DIDEVICEOBJECTDATA	od;
	DWORD				dwElements;
	HRESULT				hr;
	int					msec_guess;

	msec_guess = GetTickCount();

	if ( !g_fUseDInput || !s_bDIInitialized_guess )
		return;

	for ( ;; )
	{
		dwElements = 1;

		hr = IDirectInputDevice_GetDeviceData( g_pKeyboard_guess, sizeof( DIDEVICEOBJECTDATA ), &od, &dwElements, 0 );
		if ( FAILED( hr ) )
		{
			IDirectInputDevice_Acquire( g_pKeyboard_guess );
			hr = IDirectInputDevice_GetDeviceData( g_pKeyboard_guess, sizeof( DIDEVICEOBJECTDATA ), &od, &dwElements, 0 );
		}

		if ( FAILED( hr ) || dwElements == 0 )
			break;

		if ( s_DIKToKey_guess[od.dwOfs] )
		{
			DInput_KeyEvent_guess( od.dwOfs, s_DIKToKey_guess[od.dwOfs], od.dwData & 0x80, 0 );

			if ( od.dwData & 0x80 )
			{
				s_repeatKey_guess = od.dwOfs;
				s_keyDownTime_guess = msec_guess;
				s_lastRepeatTime_guess = msec_guess;
			}
			else if ( od.dwOfs == s_repeatKey_guess )
			{
				s_repeatKey_guess = 0;
			}
		}
	}

	// auto repeat of the last key held down
	if ( s_repeatKey_guess )
	{
		if ( s_lastRepeatTime_guess == s_keyDownTime_guess )
		{
			if ( msec_guess >= s_keyDownTime_guess + s_keyRepeatDelay_guess )
			{
				DInput_KeyEvent_guess( s_repeatKey_guess, s_DIKToKey_guess[s_repeatKey_guess], 1, 1 );
				s_lastRepeatTime_guess = msec_guess;
			}
		}
		else if ( msec_guess >= s_lastRepeatTime_guess + s_keyRepeatRate_guess )
		{
			DInput_KeyEvent_guess( s_repeatKey_guess, s_DIKToKey_guess[s_repeatKey_guess], 1, 1 );
			s_lastRepeatTime_guess = msec_guess;
		}
	}
}

void DInput_KeyEvent_guess( int scancode, int key, int down, int repeat )
{
	int		lParam_guess;

	if ( repeat )
		lParam_guess = ( scancode << 16 ) | 0x40000000 | 1;
	else
		lParam_guess = scancode << 16;

	if ( down )
		VGui_CallEngineSurfaceAppHandler( NULL, WM_KEYDOWN, s_DIKToVK_guess[scancode], lParam_guess );
	else
		VGui_CallEngineSurfaceAppHandler( NULL, WM_KEYUP, s_DIKToVK_guess[scancode], lParam_guess );

	Key_Event( key, down );
}

void DInput_Shutdown_guess( void )
{
	DInput_ReleaseMouse_guess();

	if ( g_fUseDInput && s_bDIInitialized_guess )
	{
		IDirectInput_Release( g_pdi );
		g_pdi = NULL;
	}

	if ( g_fUseDInput && s_bDIInitialized_guess && g_pKeyboard_guess )
	{
		IDirectInputDevice_Unacquire( g_pKeyboard_guess );
		IDirectInputDevice_Release( g_pKeyboard_guess );
		g_pKeyboard_guess = NULL;
	}

	g_fUseDInput = false;
	s_bDIInitialized_guess = false;
	s_bMouseActive_guess = false;
}

void IN_UpdateMouse_guess( void )
{
	if ( !g_fUseDInput )
		return;

	DInput_ReadMouse_guess();
	ClientDLL_MouseEvent( mstate_di );
	DInput_ReadKeyboard_guess();
}

void SetMouseEnable( qboolean fEnable )
{
	g_engdstAddrs.pfnSetMouseEnable( &fEnable );

	if ( s_bMouseActive_guess == fEnable )
		return;

	if ( !g_fUseDInput || !s_bDIInitialized_guess )
		return;

	if ( fEnable )
	{
		if ( !DInput_AcquireMouse_guess() )
			s_bDIInitialized_guess = false;
	}
	else
	{
		DInput_ReleaseMouse_guess();
	}

	s_bMouseActive_guess = fEnable;
}

}
