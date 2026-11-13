// LauncherDirectDraw_guess.cpp -- the DirectDraw helper that lists the display modes

#include <windows.h>
#include <ddraw.h>

#include "igame.h"
#include "ivideomode.h"
#include "ILauncherDirectDraw.h"

class CLauncherDirectDraw : public ILauncherDirectDraw
{
public:
	CLauncherDirectDraw( void );
	virtual				~CLauncherDirectDraw( void );

	virtual bool		Init_guess( void );
	virtual bool		Shutdown_guess( void );
	virtual void		EnumDisplayModes_guess( int bpp );
	virtual const char	*ErrorString_guess( long hr );

private:
	LPDIRECTDRAW		m_pDirectDraw_guess;
};

static CLauncherDirectDraw g_LauncherDirectDraw_guess;

ILauncherDirectDraw *ddraw_guess = &g_LauncherDirectDraw_guess;

// the depth EnumDisplayModes_guess lists, passed to the callback
static int s_nEnumBpp_guess;

CLauncherDirectDraw::CLauncherDirectDraw( void )
{
	m_pDirectDraw_guess = NULL;
}

CLauncherDirectDraw::~CLauncherDirectDraw( void )
{
}

bool CLauncherDirectDraw::Init_guess( void )
{
	if ( DirectDrawCreate( NULL, &m_pDirectDraw_guess, NULL ) != DD_OK )
		return false;

	m_pDirectDraw_guess->SetCooperativeLevel( NULL, DDSCL_NORMAL );

	if ( !videomode->IsWindowedMode() )
		m_pDirectDraw_guess->SetCooperativeLevel( game->GetMainWindow(), DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT );

	return true;
}

bool CLauncherDirectDraw::Shutdown_guess( void )
{
	if ( !m_pDirectDraw_guess )
		return true;

	m_pDirectDraw_guess->FlipToGDISurface();
	m_pDirectDraw_guess->Release();
	m_pDirectDraw_guess = NULL;

	return true;
}

static HRESULT WINAPI EnumModesCallback_guess( LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext );

void CLauncherDirectDraw::EnumDisplayModes_guess( int bpp )
{
	s_nEnumBpp_guess = bpp;
	m_pDirectDraw_guess->EnumDisplayModes( 0, NULL, &s_nEnumBpp_guess, EnumModesCallback_guess );
}

static HRESULT WINAPI EnumModesCallback_guess( LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext )
{
	int		x_guess = lpDDSurfaceDesc->dwWidth;
	int		y_guess = lpDDSurfaceDesc->dwHeight;
	int		bpp_guess = lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount;
	float	ar_guess = (float)x_guess / y_guess;
	bool	bUse_guess = false;

	// 16:9, 4:3, 5:4 and 16:10 modes only
	if ( x_guess * 9 == y_guess * 16 || x_guess * 3 == y_guess * 4 || x_guess * 4 == y_guess * 5 || x_guess * 5 == y_guess * 8 )
		bUse_guess = true;

	int		*pInfo_guess = (int *)lpContext;
	int		desired_guess = *pInfo_guess;

	if ( bpp_guess == 15 )
		bpp_guess = 16;

	if ( bUse_guess == true && y_guess >= 480 && desired_guess == bpp_guess )
	{
		if ( !videomode->AddMode( x_guess, y_guess, bpp_guess ) )
			return DDENUMRET_OK;
	}

	return DDENUMRET_OK;
}

const char *CLauncherDirectDraw::ErrorString_guess( long hr )
{
	long	err_guess = hr;

	switch ( err_guess )
	{
	case DD_OK:
		return "No error";
	case DDERR_ALREADYINITIALIZED:
		return "Object already initialised";
	case DDERR_CANNOTATTACHSURFACE:
		return "Cannot attack surface";
	case DDERR_CANNOTDETACHSURFACE:
		return "Cannot detach surface";
	case DDERR_CURRENTLYNOTAVAIL:
		return "Currently Not Available";
	case DDERR_EXCEPTION:
		return "Exception has occured";
	case DDERR_GENERIC:
		return "Generic failure";
	case DDERR_HEIGHTALIGN:
		return "Height Alignment";
	case DDERR_INCOMPATIBLEPRIMARY:
		return "Incompatible Primary";
	case DDERR_INVALIDCAPS:
		return "Invalid Caps";
	case DDERR_INVALIDCLIPLIST:
		return "Invlaid Clip List";
	case DDERR_INVALIDMODE:
		return "Invalid Mode";
	case DDERR_INVALIDOBJECT:
		return "Invalid Object";
	case DDERR_INVALIDPARAMS:
		return "Invalid Params";
	case DDERR_INVALIDPIXELFORMAT:
		return "Invalid Pixel Format";
	case DDERR_INVALIDRECT:
		return "Invalid Rectangle";
	case DDERR_LOCKEDSURFACES:
		return "Locked Surfaces";
	case DDERR_NO3D:
		return "No 3d support present";
	case DDERR_NOALPHAHW:
		return "No Alpha Hardware";
	case DDERR_NOCLIPLIST:
		return "No Clip List";
	case DDERR_NOCOLORCONVHW:
		return "No Colour Conversion Hardware";
	case DDERR_NOCOOPERATIVELEVELSET:
		return "No Cooperative Level Set";
	case DDERR_NOCOLORKEY:
		return "Surface Has No Colour Key";
	case DDERR_NOCOLORKEYHW:
		return "No Hardware Colour Key";
	case DDERR_NODIRECTDRAWSUPPORT:
		return "No Direct Draw Support";
	case DDERR_NOEXCLUSIVEMODE:
		return "No Exclusive Mode";
	case DDERR_NOFLIPHW:
		return "No Flip Hardware";
	case DDERR_NOGDI:
		return "GDI is not present";
	case DDERR_NOMIRRORHW:
		return "No Mirror Hardware";
	case DDERR_NOTFOUND:
		return "Requested Item Was No Found";
	case DDERR_NOOVERLAYHW:
		return "No Overlay Hardware";
	case DDERR_NORASTEROPHW:
		return "No Raster Op Hardware";
	case DDERR_NOROTATIONHW:
		return "No Roatation Hardware";
	case DDERR_NOSTRETCHHW:
		return "No Stretch Hardware";
	case DDERR_NOT4BITCOLOR:
		return "Not 4 Bit Colour";
	case DDERR_NOT4BITCOLORINDEX:
		return "Not 4 Bit Colour Index";
	case DDERR_NOT8BITCOLOR:
		return "Not 8 Bit Colour";
	case DDERR_NOTEXTUREHW:
		return "No Texture Hardware";
	case DDERR_NOVSYNCHW:
		return "No VSync Hardware";
	case DDERR_NOZBUFFERHW:
		return "No Z Buffer Hardware";
	case DDERR_NOZOVERLAYHW:
		return "No Z Overlay Hardware";
	case DDERR_OUTOFCAPS:
		return "Hardware Has Already Been Allocated";
	case DDERR_OUTOFMEMORY:
		return "Out Of Memory";
	case DDERR_OUTOFVIDEOMEMORY:
		return "Out Of Video Memory";
	case DDERR_OVERLAYCANTCLIP:
		return "Hardware can't clip overlays";
	case DDERR_OVERLAYCOLORKEYONLYONEACTIVE:
		return "Can only have colour key active at one time for overlays";
	case DDERR_PALETTEBUSY:
		return "Palette Locked by another thread";
	case DDERR_COLORKEYNOTSET:
		return "Colour key is not set";
	case DDERR_SURFACEALREADYATTACHED:
		return "Surface already attached";
	case DDERR_SURFACEALREADYDEPENDENT:
		return "Surface Already Dependent";
	case DDERR_SURFACEBUSY:
		return "Surface Is Busy";
	case DDERR_SURFACEISOBSCURED:
		return "Surface Is Obscured";
	case DDERR_SURFACELOST:
		return "Surface Lost";
	case DDERR_SURFACENOTATTACHED:
		return "Surface Not Attached";
	case DDERR_TOOBIGHEIGHT:
		return "Height Too Big";
	case DDERR_TOOBIGSIZE:
		return "Size Too Big - Height and Width are individually OK";
	case DDERR_TOOBIGWIDTH:
		return "Width Too Big";
	case DDERR_UNSUPPORTED:
		return "Unsupported Operation";
	case DDERR_UNSUPPORTEDFORMAT:
		return "Unsupported Format";
	case DDERR_UNSUPPORTEDMASK:
		return "Unsupported Mask Format";
	case DDERR_VERTICALBLANKINPROGRESS:
		return "Vertical Blank In Progress";
	case DDERR_WASSTILLDRAWING:
		return "Still Drawing";
	case DDERR_XALIGN:
		return "X Align";
	case DDERR_INVALIDDIRECTDRAWGUID:
		return "Invalid Direct Draw GUID";
	case DDERR_DIRECTDRAWALREADYCREATED:
		return "Direct Draw Already Created";
	case DDERR_NODIRECTDRAWHW:
		return "No Direct Draw HardWare";
	case DDERR_PRIMARYSURFACEALREADYEXISTS:
		return "Primary Surface Already Exists";
	case DDERR_NOEMULATION:
		return "No Emulation";
	case DDERR_REGIONTOOSMALL:
		return "Region Too Small";
	case DDERR_CLIPPERISUSINGHWND:
		return "Clipper is using a HWND";
	case DDERR_NOCLIPPERATTACHED:
		return "No Clipper Attached";
	case DDERR_NOHWND:
		return "No HWND set";
	case DDERR_HWNDSUBCLASSED:
		return "HWND Sub-Classed";
	case DDERR_HWNDALREADYSET:
		return "HWND Already Set";
	case DDERR_NOPALETTEATTACHED:
		return "No Palette Attrached";
	case DDERR_NOPALETTEHW:
		return "No Palette Hardware";
	case DDERR_BLTFASTCANTCLIP:
		return "Blit Fast Can't Clip";
	case DDERR_NOBLTHW:
		return "No Blit Hardware";
	case DDERR_NODDROPSHW:
		return "No Direct Draw Raster OP Hardware";
	case DDERR_OVERLAYNOTVISIBLE:
		return "Overlay Not Visible";
	case DDERR_NOOVERLAYDEST:
		return "No Overlay Destination";
	case DDERR_INVALIDPOSITION:
		return "Invlaid Position";
	case DDERR_NOTAOVERLAYSURFACE:
		return "Not An Overlay Surface";
	case DDERR_EXCLUSIVEMODEALREADYSET:
		return "Exclusive Mode Already Set";
	case DDERR_NOTFLIPPABLE:
		return "Not Flippable";
	case DDERR_CANTDUPLICATE:
		return "Can't Duplicate";
	case DDERR_NOTLOCKED:
		return "Not Locked";
	case DDERR_CANTCREATEDC:
		return "Can't Create DC";
	case DDERR_NODC:
		return "No DC has ever Existed";
	case DDERR_WRONGMODE:
		return "Wrong Mode";
	case DDERR_IMPLICITLYCREATED:
		return "Surface Cannot Be Restored";
	case DDERR_NOTPALETTIZED:
		return "Not Palettized";
	case DDERR_UNSUPPORTEDMODE:
		return "Unsupported Mode";
	}

	return "unk";
}
