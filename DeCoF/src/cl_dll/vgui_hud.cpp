// ====================================
// Paranoia vgui hud
// written by BUzer.
// ====================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "const.h"
#include "entity_types.h"
#include "cdll_int.h"
#include "vgui_TeamFortressViewport.h"
#include "vgui_hud.h"
#include "vgui_paranoiatext.h" // Wargon
#include "..\game_shared\vgui_loadtga.h"
#include<VGUI_TextImage.h>

#include "ammohistory.h"

#define WEAPON_PAINKILLER	24 // should match server's index


#define HEALTH_RIGHT_OFFSET	(XRES(10))
#define HEALTH_DOWN_OFFSET	(YRES(10))
#define HEALTH_MIN_SPACE	(YRES(3)) // minimum space between bars
#define HEALTH_FLASH_TIME	0.3

#define HEALTH_FADE_TIME	5
#define HEALTH_ZERO_ALPHA	150
#define HEALTH_ALPHA		70

// Wargon: »конка юза.
#define USAGE_FADE_TIME 1
#define USAGE_ALPHA 70

const char* weaponNames[NUM_WEAPON_ICONS] = 
{
	"weapon_aps",
	"weapon_9mmhandgun", // glock
	"weapon_aks",
	"weapon_ak74",
	"weapon_9mmAR",
	"weapon_9mmAR_sec", // ar grenades
	"weapon_shotgun",
	"weapon_val",
	"weapon_groza",
	"weapon_rpk",
	"weapon_handgrenade",
	"weapon_rpg", // Wargon: »конка боеприпасов дл€ RPG.
	"machinegun",
};

// Wargon: »конка юза.
int CanUseStatus;
int __MsgFunc_CanUse( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	CanUseStatus = READ_BYTE();
	return 1;
}

void CanUseInit( void )
{
	HOOK_MESSAGE( CanUse );
}

BitmapTGA* LoadResolutionImage (const char *imgname)
{
	BitmapTGA *pBitmap;
	static int resArray[] =
	{
		320, 400, 512, 640, 800,
		1024, 1152, 1280, 1600
	};

	// try to load image directly
	pBitmap = vgui_LoadTGA(imgname);
	if (!pBitmap)
	{
		//resolution based image. Should contain %d substring
		int resArrayIndex = 0;
		int i = 0;
		while ((resArray[i] <= ScreenWidth) && (i < 9))
		{
			resArrayIndex = i;
			i++;
		}

		while(pBitmap == NULL && resArrayIndex >= 0)
		{
			char imgName[64];
			sprintf(imgName, imgname, resArray[resArrayIndex]);
			pBitmap = vgui_LoadTGA(imgName);
			resArrayIndex--;
		}
	}

	return pBitmap;
}


int ShouldDrawHUD()
{
	if (!gHUD.m_pCvarDraw->value)
		return FALSE;

	if ((gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH) || gEngfuncs.IsSpectateOnly() )
		return FALSE;

	return FALSE;
}


// simple class that owns pointer to a bitmap and draws it
class ImageHolder : public Panel
{
public:
	ImageHolder(const char *imgname, Panel *parent) : Panel(0, 0, 10, 10)
	{
		m_pBitmap = LoadResolutionImage(imgname);
		setParent(parent);
		setPaintBackgroundEnabled(false);
		setVisible(true);
		if (m_pBitmap) m_pBitmap->setPos(0, 0);
	}

	~ImageHolder() {delete m_pBitmap;}
	BitmapTGA *GetBitmap() {return m_pBitmap;}

protected:
	virtual void paint()
	{
		if (ShouldDrawHUD())
		{
			if (m_pBitmap)
				m_pBitmap->doPaint(this);
		}
	}

	BitmapTGA *m_pBitmap;
};


class ShadowLabel : public Label
{
public:
	ShadowLabel(const char* text,int x,int y) : Label(text, x, y) {}

protected:
	virtual void paint()
	{
		int mr, mg, mb, ma;
		int ix, iy;
		getFgColor(mr, mg, mb, ma);
		_textImage->getPos(ix, iy);
		_textImage->setPos(ix+1, iy+1);
		_textImage->setColor( Color(0, 0, 0, ma) );
		_textImage->doPaint(this);
		_textImage->setPos(ix, iy);
		_textImage->setColor( Color(mr, mg, mb, ma) );
		_textImage->doPaint(this);
	}
};


void Hud2Init()
{
//	gEngfuncs.pfnHookUserMsg("RadioIcon", MsgShowRadioIcon);
}

void CHud2::Initialize()
{
	health = -1;
	armor = -1;
	m_fMedkitUpdateTime = 0;
	m_fUsageUpdateTime = 0; // Wargon: »конка юза.
}

CHud2::CHud2() : Panel(0, 0, XRES(640), YRES(480))
{
	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTextScheme = pSchemes->getSchemeHandle( "Default Text" );
	Font *pFont = pSchemes->getFont( hTextScheme );

	hTextScheme = pSchemes->getSchemeHandle( "impact" );
	m_pFontDigits = pSchemes->getFont( hTextScheme );
}

CHud2::~CHud2()
{
/*	delete m_pBitmapHealthFull;
	delete m_pBitmapHealthEmpty;
	delete m_pBitmapHealthFlash;    they are childrens now
	delete m_pBitmapArmorFull;
	delete m_pBitmapArmorEmpty;
	delete m_pBitmapArmorFlash;*/

	for (int i = 0; i < NUM_WEAPON_ICONS; i++)
	{
		delete m_pWeaponIconsArray[i];
	}
}

void CHud2::paintBackground()
{
//	Panel::paintBackground();
}


// buz: I dunno exactly, what solve() function does. I just wanna to find
// some function, who being called each frame, to put panels position setting in there.
// I've tried paintBackground, Chud::Redraw, CHud::Think, and some others, but
// health and armor bars sizes are jumping during interpolation and
// panels motion looks jerky (especially in steam version).
// Putting this in solve() seems to fix first problem.
void CHud2::solve()
{
	float curtime = gEngfuncs.GetClientTime();

	Panel::solve();
}


void CHud2::paint()
{
	//
	// draw ammo counters
	//
	if ( ShouldDrawHUD() ) // Wargon: »нформаци€ о патронах рисуетс€ только если hud_draw = 1.
	{
		WEAPON *pw = gHUD.m_Ammo.m_pWeapon; // shorthand
		if (gHUD.m_SpecTank_on)
		{
			int x = ScreenWidth - HEALTH_RIGHT_OFFSET;
			int y = ScreenHeight - HEALTH_DOWN_OFFSET;

			BitmapTGA* pImg = FindAmmoImageForWeapon("machinegun");
			if (pImg)
			{
				int iw, ih;
				pImg->getSize(iw, ih);
				x -= iw;
				pImg->setColor(Color(255, 255, 255, 0)); // TEST
				pImg->setPos(x, y - ih);
				pImg->doPaint(this);
				x -= XRES(6); // make some space between icon and text
				y -= 10;
			}

			if (gHUD.m_SpecTank_Ammo != -1)
			{
				// draw ammo count string
				int tw, th;
				char buf[256];					
				drawSetTextFont(m_pFontDigits);
				drawSetTextColor(250, 250, 250, 0); // TEST

				sprintf(buf, "%d", gHUD.m_SpecTank_Ammo);
				m_pFontDigits->getTextSize(buf, tw, th);
				x -= tw; y -= th;
				drawSetTextPos(x, y); drawPrintText(buf, strlen(buf));
			}
		}
		else if (pw)
		{
			int x = ScreenWidth - HEALTH_RIGHT_OFFSET;
			int y = ScreenHeight - HEALTH_DOWN_OFFSET;

			// Do we have secondary ammo?
			if ((pw->iAmmo2Type > 0) && (gWR.CountAmmo(pw->iAmmo2Type) > 0))
			{
				// Draw the secondary ammo Icon
				char buf[256];
				sprintf(buf, "%s_sec", pw->szName);
				BitmapTGA* pImg = FindAmmoImageForWeapon(buf);
				if (pImg)
				{
					int iw, ih;
					pImg->getSize(iw, ih);
					x -= iw;
					pImg->setColor(Color(255, 255, 255, 0)); // TEST
					pImg->setPos(x, y - ih);
					pImg->doPaint(this);
					x -= XRES(6); // make some space between icon and text
					y -= 10;
				}

				// draw ammo count string
				int tw, th;				
				sprintf(buf, "%d", gWR.CountAmmo(pw->iAmmo2Type));

				m_pFontDigits->getTextSize(buf, tw, th);
				drawSetTextFont(m_pFontDigits);
				drawSetTextColor(250, 250, 250, 0); // TEST
				drawSetTextPos(x - tw, y - th);
				drawPrintText(buf, strlen(buf));
				x = ScreenWidth - HEALTH_RIGHT_OFFSET - XRES(100);
			}

			if (pw->iAmmoType > 0)
			{
				y = ScreenHeight - HEALTH_DOWN_OFFSET;

				// Draw the ammo Icon
				BitmapTGA* pImg = FindAmmoImageForWeapon(pw->szName);
				if (pImg)
				{
					int iw, ih;
					pImg->getSize(iw, ih);
					x -= iw;
					pImg->setColor(Color(255, 255, 255, 0)); // TEST
					pImg->setPos(x, y - ih);
					pImg->doPaint(this);
					x -= XRES(6); // make some space between icon and text
					y -= 10;
				}

				// draw ammo count string
				int tw, th;
				char buf[256];					
				drawSetTextFont(m_pFontDigits);
				drawSetTextColor(250, 250, 250, 0); // TEST

				sprintf(buf, "%d", gWR.CountAmmo(pw->iAmmoType));
				m_pFontDigits->getTextSize(buf, tw, th);
				x -= tw; y -= th;
				drawSetTextPos(x, y); drawPrintText(buf, strlen(buf));

				if (pw->iClip >= 0) // has clip?
				{
					x -= YRES(12);
					drawSetTextPos(x, y); drawPrintText("/", 1);

					sprintf(buf, "%d", pw->iClip);
					m_pFontDigits->getTextSize(buf, tw, th);
					x = x - tw - YRES(5);
					drawSetTextPos(x, y); drawPrintText(buf, strlen(buf));
				}
			}
		}
	}
	
	Panel::paint();
}


BitmapTGA* CHud2::FindAmmoImageForWeapon(const char *wpn)
{
	return NULL;
}


void CHud2::UpdateHealth( int newhealth )
{
	if (newhealth == health)
		return;

	if (health == -1) // first update, dont do effects
	{
		health = newhealth;
		m_fHealthUpdateTime = -666;
		return;
	}

	oldhealth = health;
	health = newhealth;
	m_fHealthUpdateTime = gEngfuncs.GetClientTime();
}


void CHud2::UpdateArmor( int newarmor )
{
	if (newarmor == armor)
		return;

	if (armor == -1) // first update, dont do effects
	{
		armor = newarmor;
		m_fArmorUpdateTime = -666;
		return;
	}

	oldarmor = armor;
	armor = newarmor;
	m_fArmorUpdateTime = gEngfuncs.GetClientTime();
}
