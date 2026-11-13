//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: The career screen where the player hires his team
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <vector>

#include "CareerBotFrame.h"
#include "CareerGame.h"
#include "BitmapButton.h"
#include "DottedLabel.h"
#include "ImageButton.h"
#include "Taskbar.h"
#include "../game_shared/bot/bot_profile.h"
#include "steam/steamtypes.h"

#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/BitmapImagePanel.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/PanelHorizListPanel.h>
#include <vgui_controls/ScrollBar.h>
#include <vgui_controls/TextImage.h>

using namespace vgui2;

#define max(a,b)            (((a) > (b)) ? (a) : (b))
#define min(a,b)            (((a) < (b)) ? (a) : (b))

int32 RandomLong( int32 lLow, int32 lHigh );

//-----------------------------------------------------------------------------
// Purpose: A rectangle inside a bot button
//-----------------------------------------------------------------------------
class CRect
{
public:
	CRect();
	CRect( int x, int y, int w, int h );

	int &x() { return m_x; }
	int &y() { return m_y; }
	int &w() { return m_w; }
	int &h() { return m_h; }

protected:
	int m_x, m_y, m_w, m_h;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CRect::CRect()
{
	m_w = 0;
	m_h = 0;
	m_y = 0;
	m_x = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CRect::CRect( int x, int y, int w, int h )
{
	m_x = x;
	m_y = y;
	m_h = h;
	m_w = w;
}

//-----------------------------------------------------------------------------
// Purpose: A bot of the pool or of the team: picture, weapon, cost and skills
//-----------------------------------------------------------------------------
class CBotButton : public CImageButton
{
public:
	void ShowLock( bool show ) { m_showLock = show; }

	enum
	{
		ACTION_LEN = 8,
	};

	enum
	{
		IMAGE_CHAR = 0,
		IMAGE_WEAPON,
		IMAGE_MAX
	};

	enum
	{
		TEXT_POINTS = 0,
		TEXT_NAME,
		TEXT_SKILLLABEL,
		TEXT_TEAMLABEL,
		TEXT_AGGRESSIONLABEL,
		TEXT_MAX_HEIGHT_CALC = TEXT_AGGRESSIONLABEL,
		TEXT_SKILL,
		TEXT_TEAM,
		TEXT_AGGRESSION,
		TEXT_MAX
	};

	CBotButton( Panel *parent, const char *panelName, const char *text );
	virtual ~CBotButton();

	void SetCharacter( const CCareerCharacter *c );
	const CCareerCharacter *GetCharacter();
	void SetAction( const char *action );
	bool HasCharacter( const char *name );

	virtual Color GetImageColor();
	virtual void PaintBackground();
	virtual void SetImgTexture( int num, const char *filename );
	virtual void ForceReload();
	virtual void PerformLayout();
	virtual void Paint();
	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual Color GetBgColor();

	void UpdateBgColor();
	void SetCategoryText( TextImage *imgLabel, TextImage *imgValue, const char *category, float val );
	Color GetLevelColor( int level );
	HFont GetLevelFont( int level, IScheme *pScheme );
	void SetArmedBorder( IBorder *border );

protected:
	virtual IBorder *GetBorder( bool depressed, bool armed, bool selected, bool keyfocus );
	virtual void ForceImgUpload( int num );
	virtual void ForceImgReload( int num );

	typedef CImageButton BaseClass;

	Panel *m_pParent;
	const CCareerCharacter *m_char;
	Color m_normalColor;
	Color m_armedColor;
	Color m_depressedColor;
	Color m_disabledColor;
	Color m_bgColor;
	Color m_disabledBgColor;
	Color m_disabledTextColor;
	char m_action[ACTION_LEN];

	IBorder *m_armedBorder;

	TextImage *m_pText[TEXT_MAX];
	CRect m_textRect[TEXT_MAX];

	bool m_bImgUploaded[IMAGE_MAX];
	int m_nImgTextureId[IMAGE_MAX];
	char m_szImgTexture[IMAGE_MAX][128];
	CRect m_imgRect[IMAGE_MAX];

	bool m_showLock;

	struct bimage_t
	{
		int imageID;
		int width;
		int height;
	};
	bimage_t m_LockImageID;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBotButton::CBotButton( Panel *parent, const char *panelName, const char *text ) : CImageButton( parent, panelName, text )
{
	m_pParent = parent;
	SetSize( 75, 150 );
	m_char = NULL;
	m_action[0] = 0;
	m_showLock = false;

	IScheme *pScheme = scheme()->GetIScheme( GetScheme() );
	m_disabledColor = GetSchemeColor( "BotDisabledBG", GetSchemeColor( "ControlBG", pScheme ), pScheme );
	m_armedColor = GetSchemeColor( "BotArmedBG", GetSchemeColor( "ControlBG", pScheme ), pScheme );
	m_depressedColor = GetSchemeColor( "BotDepressedBG", GetSchemeColor( "ControlBG", pScheme ), pScheme );
	m_normalColor = GetSchemeColor( "BotNormalBG", GetSchemeColor( "ControlBG", pScheme ), pScheme );

	int i;
	for ( i = 0; i < IMAGE_MAX; ++i )
	{
		m_bImgUploaded[i] = false;
		m_nImgTextureId[i] = -1;
		m_szImgTexture[i][0] = 0;
	}

	for ( i = 0; i < TEXT_MAX; ++i )
	{
		m_pText[i] = new TextImage( "" );
	}

	m_armedBorder = NULL;

	m_LockImageID.imageID = surface()->CreateNewTextureID( false );
	surface()->DrawSetTextureFile( m_LockImageID.imageID, "gfx/career/Lock", false, false );
	surface()->DrawGetTextureSize( m_LockImageID.imageID, m_LockImageID.width, m_LockImageID.height );
	m_LockImageID.width = scheme()->GetProportionalScaledValue( m_LockImageID.width );
	m_LockImageID.height = scheme()->GetProportionalScaledValue( m_LockImageID.height );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBotButton::~CBotButton()
{
	for ( int i = 0; i < TEXT_MAX; ++i )
	{
		delete m_pText[i];
	}
}

//-----------------------------------------------------------------------------
// Purpose: Border used while the mouse is over the button
//-----------------------------------------------------------------------------
void CBotButton::SetArmedBorder( IBorder *border )
{
	m_armedBorder = border;
	InvalidateLayout( false );
}

//-----------------------------------------------------------------------------
// Purpose: Uses the armed border while the mouse is over the button
//-----------------------------------------------------------------------------
IBorder *CBotButton::GetBorder( bool depressed, bool armed, bool selected, bool keyfocus )
{
	if ( armed && !depressed && IsEnabled() )
	{
		return m_armedBorder;
	}

	return BaseClass::GetBorder( depressed, armed, selected, keyfocus );
}

//-----------------------------------------------------------------------------
// Purpose: Sounds, colors and borders
//-----------------------------------------------------------------------------
void CBotButton::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetArmedSound( "sound/UI/buttonrollover.wav" );
	SetDepressedSound( "sound/UI/buttonclick.wav" );
	SetReleasedSound( "sound/UI/buttonclickrelease.wav" );

	char bgStr[16];
	if ( m_char )
	{
		sprintf( bgStr, "BotBg%d", m_char->GetPoints() );
	}
	else
	{
		sprintf( bgStr, "BotBg0" );
	}
	m_bgColor = pScheme->GetColor( bgStr, Color( 0, 0, 0, 0 ) );
	m_disabledBgColor = pScheme->GetColor( "BotBgDisabled", Color( 0, 0, 0, 0 ) );
	m_disabledTextColor = pScheme->GetColor( "BotTextDisabled", Color( 0, 0, 0, 0 ) );

	SetDefaultBorder( pScheme->GetBorder( "CareerButtonBorder" ) );
	SetDepressedBorder( pScheme->GetBorder( "CareerButtonDepressedBorder" ) );
	m_armedBorder = pScheme->GetBorder( "CareerButtonArmedBorder" );
}

//-----------------------------------------------------------------------------
// Purpose: Background color by the cost of the bot
//-----------------------------------------------------------------------------
void CBotButton::UpdateBgColor()
{
	IScheme *pScheme = scheme()->GetIScheme( GetScheme() );
	char bgStr[16];
	if ( m_char )
	{
		sprintf( bgStr, "BotBg%d", m_char->GetPoints() );
	}
	else
	{
		sprintf( bgStr, "BotBg0" );
	}
	m_bgColor = pScheme->GetColor( bgStr, Color( 0, 0, 0, 0 ) );
}

//-----------------------------------------------------------------------------
// Purpose: Background color by state
//-----------------------------------------------------------------------------
Color CBotButton::GetBgColor()
{
	if ( !IsEnabled() )
		return m_disabledBgColor;
	else
		return m_bgColor;
}

//-----------------------------------------------------------------------------
// Purpose: Image color by state
//-----------------------------------------------------------------------------
Color CBotButton::GetImageColor()
{
	if ( !IsEnabled() )
		return m_disabledColor;
	else if ( IsDepressed() )
		return m_depressedColor;
	else if ( IsArmed() )
		return m_armedColor;
	else
		return m_normalColor;
}

//-----------------------------------------------------------------------------
// Purpose: Command prefix of the button ("hire" or "fire")
//-----------------------------------------------------------------------------
void CBotButton::SetAction( const char *action )
{
	strncpy( m_action, action, ACTION_LEN );
	m_action[ACTION_LEN - 1] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Label and rating of one skill
//-----------------------------------------------------------------------------
void CBotButton::SetCategoryText( TextImage *imgLabel, TextImage *imgValue, const char *category, float val )
{
	const int BufSize = 128;
	char catBuf[BufSize];
	char levelBuf[BufSize];

	int level = m_char->GetRange( val );
	_snprintf( catBuf, BufSize, "#Career_%sDesc", category );
	_snprintf( levelBuf, BufSize, "#Career_LevelDesc%d", level );

	Color color = GetLevelColor( level );

	imgLabel->SetText( localize()->Find( catBuf ) );
	imgValue->SetText( localize()->Find( levelBuf ) );
	imgValue->SetColor( color );
}

//-----------------------------------------------------------------------------
// Purpose: Color of a skill rating
//-----------------------------------------------------------------------------
Color CBotButton::GetLevelColor( int level )
{
	const int BufSize = 128;
	char levelBuf[BufSize];
	_snprintf( levelBuf, BufSize, "BotRange%d", level );

	IScheme *pScheme = scheme()->GetIScheme( GetScheme() );
	Color color = GetSchemeColor( levelBuf, GetSchemeColor( "ControlBG", pScheme ), pScheme );
	return color;
}

//-----------------------------------------------------------------------------
// Purpose: Font of a skill rating
//-----------------------------------------------------------------------------
HFont CBotButton::GetLevelFont( int level, IScheme *pScheme )
{
	const int BufSize = 128;
	char levelBuf[BufSize];
	_snprintf( levelBuf, BufSize, "BotRange%dFont", level );

	return pScheme->GetFont( pScheme->GetResourceString( levelBuf ), IsProportional() );
}

//-----------------------------------------------------------------------------
// Purpose: The first preferred weapon the player's team can buy
//-----------------------------------------------------------------------------
const char *GetPreferredWeapon( const BotProfile *pProfile )
{
	const char *terroristWeapons[] =
	{
		"galil",
		"ak47",
		"sg552",
		"g3sg1",
		"mac10",
		"elites",
		NULL
	};

	const char *counterTerroristWeapons[] =
	{
		"shield",
		"famas",
		"m4a1",
		"aug",
		"sg550",
		"tmp",
		"fn57",
		NULL
	};

	const char **disallowedWeapons = counterTerroristWeapons;
	if ( TheCareerGame->PlayAsCT() )
	{
		disallowedWeapons = terroristWeapons;
	}

	const char *weapon = NULL;
	for ( int i = 0; i < pProfile->GetWeaponPreferenceCount(); ++i )
	{
		weapon = pProfile->GetWeaponPreferenceAsString( i );

		int j = 0;
		const char *disallowed = disallowedWeapons[j];
		while ( weapon && disallowed && *disallowed )
		{
			if ( !strcmp( weapon, disallowed ) )
			{
				weapon = NULL;
				break;
			}
			++j;
			disallowed = disallowedWeapons[j];
		}

		if ( weapon )
			return weapon;
	}

	return weapon;
}

//-----------------------------------------------------------------------------
// Purpose: Shows a bot (or an empty slot when c is NULL)
//-----------------------------------------------------------------------------
void CBotButton::SetCharacter( const CCareerCharacter *c )
{
	IScheme *pScheme = scheme()->GetIScheme( GetScheme() );
	if ( !GetFont() && pScheme )
	{
		SetFont( pScheme->GetFont( "Default", IsProportional() ) );
	}

	m_char = c;
	if ( !c )
	{
		SetCommand( "EmptyChar" );
		SetText( "" );
		for ( int i = 0; i < TEXT_MAX; ++i )
		{
			m_pText[i]->SetText( "" );
			m_pText[i]->SetFont( GetFont() );
		}
		SetImgTexture( IMAGE_CHAR, "" );
		SetImgTexture( IMAGE_WEAPON, "" );
		InvalidateLayout( false, false );
		return;
	}

	SetImgTexture( IMAGE_CHAR, "" );
	SetImgTexture( IMAGE_WEAPON, "" );

	const int BufLen = 128;
	char buf[BufLen];
	_snprintf( buf, BufLen, "%s %s", m_action, m_char->GetName() );
	SetCommand( buf );
	SetText( "" );

	const BotProfile *pProfile = m_char->GetProfile();
	int i;
	for ( i = 0; i < TEXT_MAX; ++i )
	{
		m_pText[i]->SetFont( GetFont() );
		m_pText[i]->SetDrawWidth( GetWide() - 4 );
		m_pText[i]->SetColor( Color( 255, 255, 255, 255 ) );
	}

	wchar_t wInt[2] = L"0";
	wInt[0] = m_char->GetPoints() % 10 + L'0';
	wchar_t wbuf[128];
	localize()->ConstructString( wbuf, sizeof( wbuf ), localize()->Find( "#Career_Cost" ), 1, wInt );
	m_pText[TEXT_POINTS]->SetText( wbuf );
	m_pText[TEXT_POINTS]->SetColor( GetSchemeColor( "BotPoints", GetSchemeColor( "ControlBG", pScheme ), pScheme ) );
	m_pText[TEXT_POINTS]->SetFont( pScheme->GetFont( pScheme->GetResourceString( "BotPointsFont" ), IsProportional() ) );

	strcpy( buf, m_char->GetName() );
	for ( i = 0; i < strlen( buf ); ++i )
	{
		buf[i] = toupper( buf[i] );
	}
	m_pText[TEXT_NAME]->SetText( buf );
	m_pText[TEXT_NAME]->SetColor( GetSchemeColor( "BotName", GetSchemeColor( "ControlBG", pScheme ), pScheme ) );
	m_pText[TEXT_NAME]->SetFont( pScheme->GetFont( pScheme->GetResourceString( "BotNameFont" ), IsProportional() ) );

	SetCategoryText( m_pText[TEXT_SKILLLABEL], m_pText[TEXT_SKILL], "Skill", pProfile->GetSkill() );
	m_pText[TEXT_SKILLLABEL]->SetColor( GetSchemeColor( "BotDesc", GetSchemeColor( "ControlBG", pScheme ), pScheme ) );
	m_pText[TEXT_SKILL]->SetFont( GetLevelFont( m_char->GetRange( pProfile->GetSkill() ), pScheme ) );
	m_pText[TEXT_SKILLLABEL]->SetFont( pScheme->GetFont( pScheme->GetResourceString( "BotCategoryFont" ), IsProportional() ) );

	SetCategoryText( m_pText[TEXT_TEAMLABEL], m_pText[TEXT_TEAM], "Team", pProfile->GetTeamwork() );
	m_pText[TEXT_TEAMLABEL]->SetColor( GetSchemeColor( "BotDesc", GetSchemeColor( "ControlBG", pScheme ), pScheme ) );
	m_pText[TEXT_TEAM]->SetFont( GetLevelFont( m_char->GetRange( pProfile->GetTeamwork() ), pScheme ) );
	m_pText[TEXT_TEAMLABEL]->SetFont( pScheme->GetFont( pScheme->GetResourceString( "BotCategoryFont" ), IsProportional() ) );

	SetCategoryText( m_pText[TEXT_AGGRESSIONLABEL], m_pText[TEXT_AGGRESSION], "Aggression", pProfile->GetAggression() );
	m_pText[TEXT_AGGRESSIONLABEL]->SetColor( GetSchemeColor( "BotDesc", GetSchemeColor( "ControlBG", pScheme ), pScheme ) );
	m_pText[TEXT_AGGRESSION]->SetFont( GetLevelFont( m_char->GetRange( pProfile->GetAggression() ), pScheme ) );
	m_pText[TEXT_AGGRESSIONLABEL]->SetFont( pScheme->GetFont( pScheme->GetResourceString( "BotCategoryFont" ), IsProportional() ) );

	const char *preferredWeapon = GetPreferredWeapon( pProfile );
	if ( preferredWeapon )
	{
		_snprintf( buf, BufLen, "gfx/career/weapons/640_%s", preferredWeapon );
		SetImgTexture( IMAGE_WEAPON, buf );
	}
	else
	{
		SetImgTexture( IMAGE_WEAPON, "" );
	}

	const char *skinFilename = m_char->GetSkinThumbnailFname();
	if ( skinFilename )
	{
		SetImgTexture( IMAGE_CHAR, skinFilename );
	}

	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Does the button show this bot (any bot when name is NULL)
//-----------------------------------------------------------------------------
bool CBotButton::HasCharacter( const char *name )
{
	if ( !name )
		return ( m_char != NULL );

	if ( !m_char )
		return false;

	return !strcmp( name, m_char->GetName() );
}

//-----------------------------------------------------------------------------
const CCareerCharacter *CBotButton::GetCharacter()
{
	return m_char;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the picture or the weapon image
//-----------------------------------------------------------------------------
void CBotButton::SetImgTexture( int num, const char *filename )
{
	if ( num < 0 || num >= IMAGE_MAX )
		return;

	strcpy( m_szImgTexture[num], filename );
	if ( m_bImgUploaded[num] )
	{
		ForceImgReload( num );
	}
	else
	{
		ForceImgUpload( num );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Creates the texture of an image
//-----------------------------------------------------------------------------
void CBotButton::ForceImgUpload( int num )
{
	if ( num < 0 || num >= IMAGE_MAX )
		return;

	if ( !m_szImgTexture[num][0] )
		return;

	m_bImgUploaded[num] = true;
	m_nImgTextureId[num] = surface()->CreateNewTextureID( false );
	surface()->DrawSetTextureFile( m_nImgTextureId[num], m_szImgTexture[num], false, true );
}

//-----------------------------------------------------------------------------
// Purpose: Reloads the texture of an image
//-----------------------------------------------------------------------------
void CBotButton::ForceImgReload( int num )
{
	if ( num < 0 || num >= IMAGE_MAX )
		return;

	if ( !m_bImgUploaded[num] )
		return;

	surface()->DrawSetTextureFile( m_nImgTextureId[num], m_szImgTexture[num], false, true );
}

//-----------------------------------------------------------------------------
// Purpose: Reloads every texture
//-----------------------------------------------------------------------------
void CBotButton::ForceReload()
{
	BaseClass::ForceReload();

	for ( int i = 0; i < IMAGE_MAX; ++i )
	{
		ForceImgReload( i );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws the images and the lock
//-----------------------------------------------------------------------------
void CBotButton::PaintBackground()
{
	BaseClass::PaintBackground();

	for ( int i = 0; i < IMAGE_MAX; ++i )
	{
		if ( m_szImgTexture[i][0] )
		{
			if ( !m_bImgUploaded[i] )
			{
				ForceImgUpload( i );
			}

			Color col = GetImageColor();
			surface()->DrawSetColor( col );
			surface()->DrawSetTexture( m_nImgTextureId[i] );
			surface()->DrawTexturedRect( m_imgRect[i].x(), m_imgRect[i].y(), m_imgRect[i].x() + m_imgRect[i].w(), m_imgRect[i].y() + m_imgRect[i].h() );

			if ( i == IMAGE_CHAR )
			{
				if ( IsEnabled() )
				{
					surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
				}
				else
				{
					surface()->DrawSetColor( m_disabledTextColor );
				}
				surface()->DrawOutlinedRect( m_imgRect[i].x() - 1, m_imgRect[i].y() - 1, m_imgRect[i].x() + m_imgRect[i].w() + 1, m_imgRect[i].y() + m_imgRect[i].h() + 1 );
			}
		}
	}

	if ( m_showLock )
	{
		int x, y, w, h;
		GetSize( w, h );
		x = w - 3 - m_LockImageID.width;
		y = h - 3 - m_LockImageID.height;
		w = x + m_LockImageID.width;
		h = y + m_LockImageID.height;

		surface()->DrawSetColor( 255, 255, 255, 255 );
		surface()->DrawSetTexture( m_LockImageID.imageID );
		surface()->DrawTexturedRect( x, y, w, h );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Places the texts and the images
//-----------------------------------------------------------------------------
void CBotButton::PerformLayout()
{
	BaseClass::PerformLayout();

	int boxWidth, boxHeight;
	GetSize( boxWidth, boxHeight );

	const int groupSpacing = 5;
	const int itemSpacing = 1;

	int itemWidth, itemHeight;
	int i;

	int height = 24;
	for ( i = 0; i < TEXT_MAX; ++i )
	{
		m_pText[i]->ResizeImageToContent();
		m_pText[i]->GetContentSize( itemWidth, itemHeight );
		m_textRect[i].x() = max( 0, ( boxWidth - itemWidth ) / 2 );
		m_textRect[i].y() = height;
		m_textRect[i].w() = itemWidth;
		m_textRect[i].h() = itemHeight;
		if ( i <= TEXT_MAX_HEIGHT_CALC )
		{
			height += itemHeight;
		}
	}

	int imageHeight = boxHeight - height;
	if ( imageHeight < 10 )
	{
		m_imgRect[IMAGE_CHAR] = CRect( 0, 0, 0, 0 );
		m_imgRect[IMAGE_WEAPON] = CRect( 0, 0, 0, 0 );
	}
	else
	{
		int charHeight = imageHeight * 2 / 3;
		if ( charHeight > boxWidth )
		{
			charHeight = boxWidth;
		}
		m_imgRect[IMAGE_CHAR] = CRect( max( 0, ( boxWidth - charHeight ) / 2 ), 0, charHeight, charHeight );

		int weaponHeight = imageHeight - charHeight;
		if ( !m_szImgTexture[IMAGE_WEAPON][0] )
		{
			m_imgRect[IMAGE_WEAPON] = CRect( 0, 0, 0, 0 );
		}
		else
		{
			int texWidth = 0, texHeight = 0;
			surface()->DrawGetTextureSize( m_nImgTextureId[IMAGE_WEAPON], texWidth, texHeight );
			if ( texHeight && texWidth )
			{
				float ratio1 = (float)weaponHeight / (float)texHeight;
				float ratio2 = (float)boxWidth / (float)texWidth;
				float ratio = min( ratio1, ratio2 );
				texWidth = texWidth * ratio;
				texHeight = texHeight * ratio;
				m_imgRect[IMAGE_WEAPON] = CRect( max( 0, ( boxWidth - texWidth ) / 2 ), 0, texWidth, texHeight );
			}
			else
			{
				m_imgRect[IMAGE_WEAPON] = CRect( 0, 0, 0, 0 );
			}
		}
	}

	height = groupSpacing;
	m_textRect[TEXT_POINTS].y() = height;
	height += groupSpacing + m_textRect[TEXT_POINTS].h();

	if ( m_szImgTexture[IMAGE_CHAR][0] )
	{
		m_imgRect[IMAGE_CHAR].y() = height;
		height += groupSpacing + m_imgRect[IMAGE_CHAR].h();
	}

	m_textRect[TEXT_NAME].y() = height;
	height += itemSpacing + m_textRect[TEXT_NAME].h();

	int labelWidth, labelHeight;
	int textInset = scheme()->GetProportionalScaledValue( 4 );

	m_pText[TEXT_SKILLLABEL]->GetContentSize( labelWidth, labelHeight );
	m_pText[TEXT_SKILL]->GetContentSize( itemWidth, itemHeight );
	m_textRect[TEXT_SKILLLABEL].y() = height;
	m_textRect[TEXT_SKILL].y() = height;
	m_textRect[TEXT_SKILLLABEL].x() = textInset;
	m_textRect[TEXT_SKILL].x() = boxWidth - m_textRect[TEXT_SKILL].w() - textInset;
	height += itemSpacing + m_textRect[TEXT_SKILL].h();

	m_pText[TEXT_TEAMLABEL]->GetContentSize( labelWidth, labelHeight );
	m_pText[TEXT_TEAM]->GetContentSize( itemWidth, itemHeight );
	m_textRect[TEXT_TEAMLABEL].y() = height;
	m_textRect[TEXT_TEAM].y() = height;
	m_textRect[TEXT_TEAMLABEL].x() = textInset;
	m_textRect[TEXT_TEAM].x() = boxWidth - m_textRect[TEXT_TEAM].w() - textInset;
	height += itemSpacing + m_textRect[TEXT_TEAM].h();

	m_pText[TEXT_AGGRESSIONLABEL]->GetContentSize( labelWidth, labelHeight );
	m_pText[TEXT_AGGRESSION]->GetContentSize( itemWidth, itemHeight );
	m_textRect[TEXT_AGGRESSIONLABEL].y() = height;
	m_textRect[TEXT_AGGRESSION].y() = height;
	m_textRect[TEXT_AGGRESSIONLABEL].x() = textInset;
	m_textRect[TEXT_AGGRESSION].x() = boxWidth - m_textRect[TEXT_AGGRESSION].w() - textInset;
	height += itemSpacing + m_textRect[TEXT_SKILL].h();

	if ( m_szImgTexture[IMAGE_WEAPON][0] )
	{
		m_imgRect[IMAGE_WEAPON].y() = height;
		height += itemSpacing + m_imgRect[IMAGE_WEAPON].h();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws the texts
//-----------------------------------------------------------------------------
void CBotButton::Paint()
{
	BaseClass::Paint();

	for ( int i = 0; i < TEXT_MAX; ++i )
	{
		m_pText[i]->SetPos( m_textRect[i].x(), m_textRect[i].y() );
		if ( IsEnabled() )
		{
			m_pText[i]->Paint();
		}
		else
		{
			Color oldColor = m_pText[i]->GetColor();
			m_pText[i]->SetColor( m_disabledTextColor );
			m_pText[i]->Paint();
			m_pText[i]->SetColor( oldColor );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCareerBotFrame::CCareerBotFrame( Panel *parent ) : CCareerBaseFrame( parent, "CareerBotFrame", false )
{
	SetTitle( "#Career_Bot_Frame", true );

	int i;

	m_pPoolBackground = new CDottedBgLabel( this, "PoolBackground", "", 0, 0, 0 );
	m_pPoolBackground->SetTextInset( 2, 2 );
	m_pHiredBackground = new CDottedBgLabel( this, "HiredBackground", "", 0, 0, 0 );
	m_pHiredBackground->SetTextInset( 2, 2 );

	new CBitmapImagePanel( this, "TitlePanel", "gfx/career/Logo_CSCZ_Big" );
	new CBitmapImagePanel( this, "AssembleTeamPanel", "gfx/career/hed_assembleteam" );
	new CBitmapImagePanel( this, "CareerPointsPanel", "resource/career_points" );

	m_pPointsLabel = new Label( this, "LabelPoints", "" );
	m_pListPanel = new PanelHorizListPanel( this, "PanelListPanel" );

	int x = 555;
	int y = 75;
	for ( i = MAX_HIRED - 1; i >= 0; --i )
	{
		char buf[64];
		sprintf( buf, "HireButton%d", i );
		m_pHiredButton[i] = new CBotButton( this, buf, buf );
		m_pHiredButton[i]->SetCommand( buf );
		m_pHiredButton[i]->SetPos( x, y );
		m_pHiredButton[i]->SetAction( "fire" );
		x -= m_pHiredButton[i]->GetWide() + 5;
	}

	m_pAutoRosterButton = new CBitmapButton( this, "ButtonAutoRoster", "", "gfx/career/circled_lightningbolt_away_big", "gfx/career/circled_lightningbolt_over_big", "gfx/career/circled_lightningbolt_clicked_big", NULL );
	m_buttons.PutElement( m_pAutoRosterButton );

	int wide, tall;
	surface()->GetScreenSize( wide, tall );
	bool hiresImages = true;
	if ( tall > 480 || wide > 640 )
	{
		hiresImages = true;
	}

	m_pJumpButtons.AddToTail( NULL );
	char buf[5][64];
	for ( i = 1; i <= 5; ++i )
	{
		sprintf( buf[0], "ButtonJump%d", i );
		sprintf( buf[1], "gfx/career/circled_%d_away%s", i, ( hiresImages ) ? "_big" : "" );
		sprintf( buf[2], "gfx/career/circled_%d_over%s", i, ( hiresImages ) ? "_big" : "" );
		sprintf( buf[3], "gfx/career/circled_%d_clicked%s", i, ( hiresImages ) ? "_big" : "" );
		sprintf( buf[4], "gfx/career/circled_%d_disabled%s", i, ( hiresImages ) ? "_big" : "" );
		m_pJumpButtons.AddToTail( new CBitmapButton( this, buf[0], "", buf[1], buf[2], buf[3], buf[4] ) );
		m_buttons.PutElement( m_pJumpButtons[i] );
	}

	m_buttons.PutElement( m_pStartButton = new CCareerButton( this, "ButtonPlay", "#Career_Start", "gfx/career/icon_forward", true ) );
	m_buttons.PutElement( m_pPrevButton = new CCareerButton( this, "ButtonPrev", "#Career_BotPrev", "gfx/career/icon_backward", false ) );

	m_pPoolPgUpButton = new CBitmapButton( this, "ButtonBotPageUp", "", "gfx/career/LeftArrow_away", "gfx/career/LeftArrow_over", "gfx/career/LeftArrow_clicked", NULL );
	m_buttons.PutElement( m_pPoolPgUpButton );
	m_pPoolPgDownButton = new CBitmapButton( this, "ButtonBotPageDown", "", "gfx/career/RightArrow_away", "gfx/career/RightArrow_over", "gfx/career/RightArrow_clicked", NULL );
	m_buttons.PutElement( m_pPoolPgDownButton );

	m_pPointsLeftLabel = new Label( this, "LabelPointsLeft", "" );

	LoadControlSettings( "Resource/CareerBotFrame.res" );

	for ( i = MAX_HIRED - 1; i >= 0; --i )
	{
		m_pHiredButton[i]->SetCharacter( NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCareerBotFrame::~CCareerBotFrame()
{
}

//-----------------------------------------------------------------------------
// Purpose: Colors of the labels and backgrounds
//-----------------------------------------------------------------------------
void CCareerBotFrame::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pPointsLabel->SetFgColor( pScheme->GetColor( "WindowBG", Color( 0, 0, 0, 0 ) ) );
	m_pListPanel->SetBorder( NULL );

	Color col;
	col = pScheme->GetColor( "ControlDarkBG", col );
	m_pPoolBackground->SetBgColor( col );
	m_pHiredBackground->SetBgColor( col );
	m_pPoolPgUpButton->SetBgColor( Color( 255, 255, 255, 255 ) );
	m_pPoolPgDownButton->SetBgColor( Color( 255, 255, 255, 255 ) );

	m_pPointsLeftLabel->SetFgColor( pScheme->GetColor( "PlayerName", Color( 0, 0, 0, 0 ) ) );
}

//-----------------------------------------------------------------------------
// Purpose: Restores the pool position once the list has its size
//-----------------------------------------------------------------------------
void CCareerBotFrame::OnThink()
{
	BaseClass::OnThink();

	if ( !m_initialLayoutDone )
	{
		int low, high;
		m_pListPanel->GetSliderRange( low, high );
		if ( low || high )
		{
			m_initialLayoutDone = true;
			m_pListPanel->SetSliderPosition( TheCareerGame->GetBotPoolSliderPosition() );
			m_pListPanel->InvalidateLayout( false, false );
		}
	}
}

typedef CCareerCharacter *BotPtr;

//-----------------------------------------------------------------------------
// Purpose: Sorts the pool by cost, skills and name
//-----------------------------------------------------------------------------
int BotSortFunc( const void *p1, const void *p2 )
{
	BotPtr *bot1 = (BotPtr *)p1;
	BotPtr *bot2 = (BotPtr *)p2;

	if ( !bot1 || !bot2 || !*bot1 || !*bot2 )
		return 0;

	bool lt = **bot1 < **bot2;
	bool gt = **bot2 < **bot1;

	return ( lt ) ? -1 : ( ( gt ) ? 1 : 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Sorts a list of bots with qsort
//-----------------------------------------------------------------------------
template < class T >
void SortUtlVector_guess( CUtlVector< T > *vec, int ( *compare )( const void *, const void * ) )
{
	if ( !vec->Count() )
		return;

	T *base = &( *vec )[0];
	qsort( base, vec->Count(), sizeof( T ), compare );
}

//-----------------------------------------------------------------------------
// Purpose: Fills the pool and the team
//-----------------------------------------------------------------------------
void CCareerBotFrame::Activate()
{
	SetPos( 0, 0 );
	BaseClass::Activate();

	int w, h;
	int i;
	m_pHiredButton[0]->GetSize( w, h );
	m_pListPanel->DeleteAllItems();

	CharacterVec allChars = TheCareerGame->GetAvailableCharacters();
	CharacterVec hiredChars = TheCareerGame->GetHiredCharacters();
	while ( !hiredChars.empty() )
	{
		allChars.push_back( hiredChars.back() );
		hiredChars.pop_back();
	}

	CUtlVector< CCareerCharacter * > sortedCharacters;
	for ( i = 0; i < allChars.size(); ++i )
	{
		sortedCharacters.AddToTail( allChars[i] );
	}
	SortUtlVector_guess( &sortedCharacters, BotSortFunc );

	for ( i = 0; i < sortedCharacters.Count(); ++i )
	{
		const CCareerCharacter *c = sortedCharacters[i];
		char buf[64];
		sprintf( buf, "PoolButton%s", c->GetName() );
		CBotButton *pButton = new CBotButton( this, buf, buf );
		pButton->SetCommand( buf );
		pButton->SetSize( w, h );
		pButton->SetAction( "hire" );
		pButton->SetCharacter( c );
		m_pListPanel->AddItem( pButton );
	}

	m_initialLayoutDone = false;

	for ( i = 0; i < MAX_HIRED; ++i )
	{
		m_pHiredButton[i]->SetCharacter( NULL );
	}

	CharacterVecIt it;
	int poolSize = m_pListPanel->GetItemCount();
	CBotButton *pPoolButton;
	for ( i = 0; i < poolSize; ++i )
	{
		pPoolButton = (CBotButton *)m_pListPanel->GetItem( i );
		if ( pPoolButton )
		{
			pPoolButton->SetVisible( true );
		}
	}

	hiredChars = TheCareerGame->GetHiredCharacters();
	for ( it = hiredChars.begin(); it != hiredChars.end(); ++it )
	{
		CCareerCharacter *c = *it;
		CBotButton *pHiredButton = GetHiredButton( NULL );
		CBotButton *pPoolButton = GetPoolButton( c->GetName() );
		if ( pPoolButton && pHiredButton )
		{
			pPoolButton->SetVisible( false );
			pHiredButton->SetCharacter( pPoolButton->GetCharacter() );
		}
	}

	SortHires();
	UpdatePointsRemaining();
	m_pListPanel->SetSliderPosition( TheCareerGame->GetBotPoolSliderPosition() );
	m_pListPanel->InvalidateLayout( false, false );

	int highestFlashed = TheCareerGame->GetHighestCostAvailable( TheCareerGame->GetPointsFlashed() );
	int currentHighest = TheCareerGame->GetHighestCostAvailable( -1 );
	for ( i = 1; i <= currentHighest && i < m_pJumpButtons.Count(); ++i )
	{
		m_pJumpButtons[i]->SetEnabled( true );
		if ( currentHighest > highestFlashed )
		{
			m_pJumpButtons[i]->Flash();
		}
	}
	for ( i = currentHighest + 1; i < m_pJumpButtons.Count(); ++i )
	{
		m_pJumpButtons[i]->SetEnabled( false );
	}
	TheCareerGame->MarkJumpButtonsFlashed();

	m_pStartButton->RequestFocus();
}

//-----------------------------------------------------------------------------
// Purpose: The team button showing a bot (the first empty one when name is NULL)
//-----------------------------------------------------------------------------
CBotButton *CCareerBotFrame::GetHiredButton( const char *name )
{
	for ( int i = 0; i < MAX_HIRED; ++i )
	{
		if ( !name && !m_pHiredButton[i]->HasCharacter( name ) )
			return m_pHiredButton[i];

		if ( name && m_pHiredButton[i]->HasCharacter( name ) )
			return m_pHiredButton[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: The pool button showing a bot
//-----------------------------------------------------------------------------
CBotButton *CCareerBotFrame::GetPoolButton( const char *name )
{
	if ( !name )
		return NULL;

	int i;
	CBotButton *pPoolButton;
	int poolSize = m_pListPanel->GetItemCount();
	for ( i = 0; i < poolSize; ++i )
	{
		pPoolButton = (CBotButton *)m_pListPanel->GetItem( i );
		if ( pPoolButton && pPoolButton->HasCharacter( name ) )
			return pPoolButton;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Points left, and which bots can still be hired
//-----------------------------------------------------------------------------
void CCareerBotFrame::UpdatePointsRemaining()
{
	char tmp[16];
	sprintf( tmp, "%d", TheCareerGame->GetPointsRemaining() );
	m_pPointsLabel->SetText( tmp );

	int i;
	bool openHiredSpot = false;
	for ( i = 0; i < MAX_HIRED; ++i )
	{
		m_pHiredButton[i]->UpdateBgColor();
		if ( !m_pHiredButton[i]->GetCharacter() )
		{
			m_pHiredButton[i]->SetEnabled( false );
			openHiredSpot = true;
		}
		else
		{
			m_pHiredButton[i]->SetEnabled( true );
		}
	}

	CBotButton *pPoolButton;
	int poolSize = m_pListPanel->GetItemCount();
	int maxPoints = TheCareerGame->GetHighestCostAvailable( -1 );
	for ( i = 0; i < poolSize; ++i )
	{
		pPoolButton = (CBotButton *)m_pListPanel->GetItem( i );
		if ( pPoolButton )
		{
			const CCareerCharacter *pChar = pPoolButton->GetCharacter();
			if ( pChar && pChar->GetPoints() <= maxPoints )
			{
				pPoolButton->SetEnabled( openHiredSpot && pChar->GetPoints() <= TheCareerGame->GetPointsRemaining() );
				pPoolButton->ShowLock( false );
			}
			else
			{
				pPoolButton->SetEnabled( false );
				pPoolButton->ShowLock( true );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Buttons of the screen
//-----------------------------------------------------------------------------
void CCareerBotFrame::OnCommand( const char *command )
{
	if ( !strcmp( command, "close" ) )
	{
		OnClose();
	}
	else if ( !strcmp( command, "prev" ) )
	{
		Career_Printf( "Going to map page\n" );
		OnClose();
		g_pTaskbar->OnOpenCareerMapDialog( false );
	}
	else if ( !strcmp( command, "EmptyChar" ) )
	{
	}
	else if ( !strcmp( command, "AutoRoster" ) )
	{
		AutoRoster();
		m_pStartButton->RequestFocus();
	}
	else if ( !strncmp( command, "hire ", 5 ) && strlen( command ) > 5 )
	{
		Hire( command + 5 );
		m_pStartButton->RequestFocus();
	}
	else if ( !strncmp( command, "fire ", 5 ) && strlen( command ) > 5 )
	{
		Fire( command + 5 );
		m_pStartButton->RequestFocus();
	}
	else if ( !strcmp( command, "start" ) )
	{
		TheCareerGame->SetBotPoolSliderPosition( m_pListPanel->GetSliderPosition() );
		TheCareerGame->SendMatchCommand();
		OnClose();
	}
	else if ( !strcmp( command, "PoolPageUp" ) )
	{
		m_pListPanel->SetSliderPosition( m_pListPanel->GetSliderPosition() - m_pListPanel->GetScrollBar()->GetRangeWindow() );
		m_pStartButton->RequestFocus();
	}
	else if ( !strcmp( command, "PoolPageDown" ) )
	{
		m_pListPanel->SetSliderPosition( m_pListPanel->GetSliderPosition() + m_pListPanel->GetScrollBar()->GetRangeWindow() );
		m_pStartButton->RequestFocus();
	}
	else if ( !strncmp( command, "jump ", 5 ) && strlen( command ) == 6 )
	{
		int jumpToPoints = command[5] - '0';
		if ( jumpToPoints > 5 || jumpToPoints < 1 )
			return;

		int i;
		CBotButton *pPoolButton;
		int poolSize = m_pListPanel->GetItemCount();
		for ( i = 0; i < poolSize; ++i )
		{
			pPoolButton = (CBotButton *)m_pListPanel->GetItem( i );
			if ( pPoolButton && pPoolButton->HasCharacter( NULL ) )
			{
				if ( pPoolButton->GetCharacter()->GetPoints() == jumpToPoints && pPoolButton->IsVisible() )
				{
					int x, y;
					pPoolButton->GetPos( x, y );
					Career_Printf( "Jump to %d found someone in index %d at %d, %d\n", jumpToPoints, i, x, y );
					m_pListPanel->SetSliderPosition( x );
					m_pStartButton->RequestFocus();
					return;
				}
			}
		}
		m_pStartButton->RequestFocus();
	}
	else
	{
		Career_Printf( "Unknown command [%s]\n", command );
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: Moves a bot from the pool to the team
//-----------------------------------------------------------------------------
int CCareerBotFrame::Hire( const char *name )
{
	CBotButton *pHiredButton = GetHiredButton( NULL );
	CBotButton *pPoolButton = GetPoolButton( name );
	if ( pPoolButton && pHiredButton && TheCareerGame->Hire( name ) )
	{
		pPoolButton->SetVisible( false );
		pHiredButton->SetCharacter( pPoolButton->GetCharacter() );
		UpdatePointsRemaining();
		SortHires();
		m_pListPanel->InvalidateLayout( false, false );
		pHiredButton->InvalidateLayout( false, false );
		return pPoolButton->GetCharacter()->GetPoints();
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Moves a bot from the team back to the pool
//-----------------------------------------------------------------------------
void CCareerBotFrame::Fire( const char *name )
{
	CBotButton *pHiredButton = GetHiredButton( name );
	CBotButton *pPoolButton = GetPoolButton( name );
	if ( pPoolButton && pHiredButton )
	{
		if ( TheCareerGame->Fire( name ) )
		{
			pPoolButton->SetVisible( true );
			pHiredButton->SetCharacter( NULL );
			UpdatePointsRemaining();
			SortHires();
			m_pListPanel->InvalidateLayout( false, false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Moves the team to the first buttons, in the order of the buttons
//-----------------------------------------------------------------------------
void CCareerBotFrame::SortHires()
{
	std::vector< const CCareerCharacter * > hired;

	int i;
	for ( i = 0; i < MAX_HIRED; ++i )
	{
		if ( m_pHiredButton[i]->HasCharacter( NULL ) )
		{
			hired.push_back( m_pHiredButton[i]->GetCharacter() );
			m_pHiredButton[i]->SetCharacter( NULL );
		}
	}

	i = 0;
	for ( std::vector< const CCareerCharacter * >::iterator it = hired.begin(); it != hired.end(); ++it )
	{
		const CCareerCharacter *pChar = *it;
		m_pHiredButton[i++]->SetCharacter( pChar );
	}

	UpdatePointsRemaining();
}

//-----------------------------------------------------------------------------
// Purpose: The visible pool bots of one cost (the caller deletes the list)
//-----------------------------------------------------------------------------
CUtlVector< const CCareerCharacter * > *CCareerBotFrame::GetPoolCharacters( int points )
{
	CUtlVector< const CCareerCharacter * > *chars = new CUtlVector< const CCareerCharacter * >;

	int i;
	CBotButton *pPoolButton = NULL;
	int poolSize = m_pListPanel->GetItemCount();
	for ( i = poolSize - 1; i >= 0; --i )
	{
		pPoolButton = (CBotButton *)m_pListPanel->GetItem( i );
		if ( !pPoolButton )
			continue;

		const CCareerCharacter *pChar = pPoolButton->GetCharacter();
		if ( pPoolButton->IsVisible() && pChar && pChar->GetPoints() == points )
		{
			chars->AddToTail( pChar );
		}
	}

	return chars;
}

//-----------------------------------------------------------------------------
// Purpose: Hires a random team that spends the points evenly
//-----------------------------------------------------------------------------
void CCareerBotFrame::AutoRoster()
{
	int i;

	while ( m_pHiredButton[0]->GetCharacter() )
	{
		Fire( m_pHiredButton[0]->GetCharacter()->GetName() );
	}

	int numOpen = MAX_HIRED;
	for ( i = 0; i < MAX_HIRED; ++i )
	{
		if ( m_pHiredButton[i]->GetCharacter() )
		{
			--numOpen;
		}
	}

	if ( !numOpen )
		return;

	int pointsRemaining = TheCareerGame->GetPointsRemaining();
	int pointsPerSlot = pointsRemaining / numOpen;
	int remainder = pointsRemaining % numOpen;
	for ( i = 0; i < numOpen; ++i )
	{
		int points = pointsPerSlot;
		if ( remainder )
		{
			++points;
		}

		int j;
		bool done = false;
		for ( j = points; j > 0 && !done; --j )
		{
			CUtlVector< const CCareerCharacter * > *chars = GetPoolCharacters( j );
			while ( !done && chars->Count() )
			{
				int testIndex = RandomLong( 0, chars->Count() - 1 );
				const CCareerCharacter *pChar = (*chars)[testIndex];
				chars->FastRemove( testIndex );
				if ( Hire( pChar->GetName() ) )
				{
					if ( j == points && remainder )
					{
						--remainder;
					}
					done = true;
				}
			}
			delete chars;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Escape goes back to the map screen, Enter starts the match
//-----------------------------------------------------------------------------
void CCareerBotFrame::OnKeyCodeTyped( KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		OnCommand( "prev" );
	}

	if ( code == KEY_ENTER )
	{
		OnCommand( "start" );
	}
	else
	{
		BaseClass::OnKeyCodeTyped( code );
	}
}
