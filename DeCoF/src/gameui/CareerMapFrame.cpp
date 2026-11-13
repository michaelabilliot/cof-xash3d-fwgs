//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: The career map selection screen
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "CareerMapFrame.h"
#include "CareerGame.h"
#include "DottedLabel.h"
#include "EngineInterface.h"
#include "ImageButton.h"
#include "Taskbar.h"
#include "../game_shared/shared_util.h"

#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/BitmapImagePanel.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/RichText.h>
#include <vgui_controls/ScrollBar.h>
#include <vgui_controls/TextImage.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Fills the map description: enemies, tasks and team rules
//-----------------------------------------------------------------------------
void ShowCareerMapDescription( RichText *pText, CCareerMap *pMap, Color normalColor, Color friendlyFireColor )
{
	if ( !pText )
		return;

	if ( !pMap )
	{
		pText->SetText( L"" );
		return;
	}

	const int DescLen = 2048;
	wchar_t str[DescLen] = L"";

	const CharacterVec &bots = pMap->GetBotList();
	int numBots = pMap->GetCurrentEnemyCount();
	CharacterVecCIt cIt;

	char intStr[6] = "";
	_snprintf( intStr, 6, "%d", numBots );
	wchar_t intWStr[6];
	localize()->ConvertANSIToUnicode( intStr, intWStr, sizeof( intWStr ) );

	if ( numBots == 1 )
	{
		wchar_t *singleEnemyString = localize()->Find( "#Career_EnemyListSingle" );
		if ( singleEnemyString )
		{
			wcsncat( str, singleEnemyString, DescLen );
		}
		else
		{
			wchar_t *enemyListStr = localize()->Find( "#Career_EnemyList" );
			localize()->ConstructString( str, DescLen, enemyListStr, 1, intWStr );
		}
	}
	else
	{
		wchar_t *enemyListStr = localize()->Find( "#Career_EnemyList" );
		localize()->ConstructString( str, DescLen, enemyListStr, 1, intWStr );
	}
	wcsncat( str, L"\n", DescLen );

	int botsSeen = 0;
	char botStr[64];
	wchar_t botWStr[64];
	bool needSeparator = false;
	for ( cIt = bots.begin(); cIt != bots.end() && botsSeen++ < numBots; ++cIt )
	{
		if ( needSeparator )
		{
			wcsncat( str, localize()->Find( "#Career_EnemyListSeparator" ), DescLen );
		}
		needSeparator = true;

		const CCareerCharacter *pChar = *cIt;
		_snprintf( botStr, sizeof( botStr ), " %s", pChar->GetName() );
		localize()->ConvertANSIToUnicode( botStr, botWStr, sizeof( botWStr ) );
		wcsncat( str, botWStr, DescLen );
	}
	wcsncat( str, L"\n\n", DescLen );

	wcsncat( str, localize()->Find( "#Career_TasksColon" ), DescLen );
	wcsncat( str, L"\n", DescLen );

	ITaskVec *tv = pMap->GetTasks();
	if ( !tv )
	{
		pText->SetText( L"" );
		return;
	}

	for ( ITaskVecIt it = tv->begin(); it != tv->end(); ++it )
	{
		ICareerTask *task = *it;
		if ( wcslen( task->GetLocalizedTaskNameWithCompletion() ) )
		{
			wcsncat( str, task->GetLocalizedTaskNameWithCompletion(), DescLen );
			wcsncat( str, L"\n", DescLen );
		}
	}

	pText->SetText( L"" );
	pText->InsertColorChange( normalColor );
	pText->InsertString( str );

	if ( pMap->AreTeammatesAllowed() )
	{
		if ( pMap->IsFriendlyFireOn() )
		{
			pText->InsertColorChange( friendlyFireColor );
			pText->InsertString( "#Career_FriendlyFireOn" );
		}
	}
	else
	{
		pText->InsertColorChange( friendlyFireColor );
		pText->InsertString( "#Career_NoTeammates" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: A map button, with its lock and its defeated mark
//-----------------------------------------------------------------------------
class CMapButton : public CImageButton
{
public:
	CMapButton( CCareerMapFrame *parent, const char *panelName, const char *text, const char *mapName );
	virtual ~CMapButton();

	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void PaintBackground();
	virtual void PaintBorder();

	void SetMapName( const char *mapName );
	void SetActive( bool val );
	void SetDefeated( bool val );
	void IgnoreDoubleClicks_guess() { m_ignoreDoubleClicks = true; }

	virtual Color GetBgColor();
	virtual Color GetImageColor();

	virtual void OnMouseDoublePressed( MouseCode mouseCode );
	virtual void OnMousePressed( MouseCode mouseCode );

	virtual void DrawFocusBorder( int tx0, int ty0, int tx1, int ty1 )
	{
	}

private:
	typedef CImageButton BaseClass;

	struct bimage_t
	{
		int imageID;
		int width;
		int height;
	};

	CCareerMapFrame *m_pParent;
	Color m_activeColor;
	Color m_inactiveColor;
	Color m_defeatedColor;
	Color m_armedColor;
	Color m_depressedColor;
	Color m_disabledColor;
	bool m_bActive;
	bool m_bDefeated;
	char *m_pMapName;
	bool m_ignoreDoubleClicks;
	TextImage *m_pDefeatedText;
	bimage_t m_ImageID[2];
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMapButton::CMapButton( CCareerMapFrame *parent, const char *panelName, const char *text, const char *mapName ) : CImageButton( parent, panelName, text )
{
	m_pParent = parent;
	m_bActive = false;

	IScheme *pScheme = scheme()->GetIScheme( GetScheme() );
	m_activeColor = GetSchemeColor( "MapActiveBG", GetSchemeColor( "ControlBG", pScheme ), pScheme );
	m_disabledColor = GetSchemeColor( "MapDisabledBG", GetSchemeColor( "ControlBG", pScheme ), pScheme );
	m_armedColor = GetSchemeColor( "MapArmedBG", GetSchemeColor( "ControlBG", pScheme ), pScheme );
	m_depressedColor = GetSchemeColor( "MapDepressedBG", GetSchemeColor( "ControlBG", pScheme ), pScheme );
	m_inactiveColor = GetSchemeColor( "MapInactiveBG", GetSchemeColor( "ControlBG", pScheme ), pScheme );
	m_defeatedColor = GetSchemeColor( "MapDefeatedBG", GetSchemeColor( "ControlBG", pScheme ), pScheme );

	m_pMapName = CloneString( mapName );
	m_ignoreDoubleClicks = false;

	m_ImageID[0].imageID = surface()->CreateNewTextureID( false );
	surface()->DrawSetTextureFile( m_ImageID[0].imageID, "gfx/career/Checkbox_Unlocked", false, false );
	surface()->DrawGetTextureSize( m_ImageID[0].imageID, m_ImageID[0].width, m_ImageID[0].height );

	m_ImageID[1].imageID = surface()->CreateNewTextureID( false );
	surface()->DrawSetTextureFile( m_ImageID[1].imageID, "gfx/career/Lock", false, false );
	surface()->DrawGetTextureSize( m_ImageID[1].imageID, m_ImageID[1].width, m_ImageID[1].height );
	m_ImageID[1].width = scheme()->GetProportionalScaledValue( m_ImageID[1].width );
	m_ImageID[1].height = scheme()->GetProportionalScaledValue( m_ImageID[1].height );

	m_pDefeatedText = new TextImage( "" );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CMapButton::~CMapButton()
{
	delete[] m_pMapName;
	delete m_pDefeatedText;
}

//-----------------------------------------------------------------------------
// Purpose: Sounds and the defeated mark
//-----------------------------------------------------------------------------
void CMapButton::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetArmedSound( "sound/UI/buttonrollover.wav" );
	SetDepressedSound( "sound/UI/buttonclick.wav" );
	SetReleasedSound( "sound/UI/buttonclickrelease.wav" );

	m_pDefeatedText->SetText( "#Career_Secured" );
	m_pDefeatedText->SetColor( GetSchemeColor( "SelectedMap", GetSchemeColor( "ControlBG", pScheme ), pScheme ) );
	m_pDefeatedText->SetFont( pScheme->GetFont( "MapComplete", IsProportional() ) );
	m_pDefeatedText->ResizeImageToContent();
}

//-----------------------------------------------------------------------------
// Purpose: Draws the lock on locked maps and the mark on defeated ones
//-----------------------------------------------------------------------------
void CMapButton::PaintBackground()
{
	BaseClass::PaintBackground();

	int id = 1;
	if ( !TheCareerGame->GetPlayableMap( m_pMapName ) )
	{
		id = 1;
	}
	else if ( m_bDefeated )
	{
		id = 0;

		int wide, tall;
		GetSize( wide, tall );
		int textWide, textTall;
		m_pDefeatedText->GetSize( textWide, textTall );
		m_pDefeatedText->SetPos( (wide - textWide)/2, (tall - textTall)/2 );
		m_pDefeatedText->Paint();
		return;
	}
	else
	{
		return;
	}

	int x, y, w, h;
	GetSize( w, h );
	x = w - 2 - m_ImageID[id].width;
	y = h - 2 - m_ImageID[id].height;
	if ( m_bActive )
	{
		x -= 4;
		y -= 4;
	}
	w = x + m_ImageID[id].width;
	h = y + m_ImageID[id].height;

	surface()->DrawSetColor( 255, 255, 255, 255 );
	surface()->DrawSetTexture( m_ImageID[id].imageID );
	surface()->DrawTexturedRect( x, y, w, h );
}

//-----------------------------------------------------------------------------
// Purpose: Remembers the map of the button
//-----------------------------------------------------------------------------
void CMapButton::SetMapName( const char *mapName )
{
	delete[] m_pMapName;
	m_pMapName = CloneString( mapName );
}

//-----------------------------------------------------------------------------
// Purpose: A thick border around the active map
//-----------------------------------------------------------------------------
void CMapButton::PaintBorder()
{
	BaseClass::PaintBorder();

	Color col;
	IScheme *pScheme = scheme()->GetIScheme( GetScheme() );
	if ( IsArmed() || m_bActive )
	{
		col = pScheme->GetColor( "SelectedMap", col );
	}
	else
	{
		col = pScheme->GetColor( "BorderBright", col );
	}
	surface()->DrawSetColor( col );

	int w, h, x, y;
	int size = ( m_bActive ) ? 4 : 1;
	GetBounds( x, y, w, h );
	surface()->DrawFilledRect( 0, 0, w, size );
	surface()->DrawFilledRect( 0, 0, size, h );
	surface()->DrawFilledRect( w - size, 0, w, h );
	surface()->DrawFilledRect( 0, h - size, w, h );
}

//-----------------------------------------------------------------------------
// Purpose: Selects the map and shows its description
//-----------------------------------------------------------------------------
void CMapButton::SetActive( bool val )
{
	if ( val )
	{
		SetFillPadding( 1 );
		m_pParent->ShowMapDescription( TheCareerGame->GetMap( m_pMapName ) );
		if ( m_pParent && TheCareerGame->GetPlayableMap( m_pMapName ) )
		{
			TheCareerGame->SetLastMap( m_pMapName );
		}
		m_pParent->GetNextButton()->SetEnabled( TheCareerGame->GetPlayableMap( m_pMapName ) != NULL );
	}
	else
	{
		SetFillPadding( 4 );
	}

	m_bActive = val;
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Shows the defeated mark
//-----------------------------------------------------------------------------
void CMapButton::SetDefeated( bool val )
{
	m_bDefeated = val;
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Background color by state
//-----------------------------------------------------------------------------
Color CMapButton::GetBgColor()
{
	if ( !TheCareerGame->GetPlayableMap( m_pMapName ) )
		return m_disabledColor;
	else if ( !IsEnabled() )
		return m_disabledColor;
	else if ( IsDepressed() )
		return m_depressedColor;
	else if ( IsArmed() )
		return m_armedColor;
	else if ( m_bActive )
		return m_activeColor;
	else if ( m_bDefeated )
		return m_defeatedColor;
	else
		return m_inactiveColor;
}

//-----------------------------------------------------------------------------
// Purpose: Image color by state
//-----------------------------------------------------------------------------
Color CMapButton::GetImageColor()
{
	if ( !TheCareerGame->GetPlayableMap( m_pMapName ) )
		return m_disabledColor;
	else if ( !IsEnabled() )
		return m_disabledColor;
	else if ( IsDepressed() )
		return m_depressedColor;
	else if ( IsArmed() )
		return m_armedColor;
	else if ( m_bDefeated )
		return m_defeatedColor;
	else if ( m_bActive )
		return m_activeColor;
	else
		return m_inactiveColor;
}

//-----------------------------------------------------------------------------
// Purpose: A double click on a playable map starts it
//-----------------------------------------------------------------------------
void CMapButton::OnMouseDoublePressed( MouseCode mouseCode )
{
	if ( !IsEnabled() )
		return;

	if ( mouseCode == MOUSE_LEFT && !m_ignoreDoubleClicks )
	{
		if ( m_pParent && TheCareerGame->GetPlayableMap( m_pMapName ) )
		{
			TheCareerGame->SetLastMap( m_pMapName );
			m_pParent->OnCommand( "next" );
			return;
		}
		m_ignoreDoubleClicks = false;
	}

	BaseClass::OnMouseDoublePressed( mouseCode );
}

//-----------------------------------------------------------------------------
// Purpose: A new click allows double clicks again
//-----------------------------------------------------------------------------
void CMapButton::OnMousePressed( MouseCode mouseCode )
{
	if ( !IsEnabled() )
		return;

	if ( mouseCode == MOUSE_LEFT )
	{
		m_ignoreDoubleClicks = false;
	}

	BaseClass::OnMousePressed( mouseCode );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCareerMapFrame::CCareerMapFrame( Panel *parent ) : CCareerBaseFrame( parent, "CareerMapFrame", false )
{
	SetTitle( "#Career_Map_Frame", true );

	int i;
	for ( i=0; i<3; ++i )
	{
		for ( int j=0; j<3; ++j )
		{
			m_pMapButton[i][j] = NULL;
		}
	}

	m_pMapSelectionBackground = new CDottedBgLabel( this, "MapSelectionBackground", "", 0, 0, 0 );
	m_pMapSelectionBackground->SetTextInset( 2, 2 );
	m_pMapDescriptionBackground = new CDottedBgLabel( this, "MapDescriptionBackground", "", 0, 0, 0 );
	m_pMapDescriptionBackground->SetTextInset( 2, 2 );

	const int spacingX = 10;
	const int spacingY = 10;
	const int startX = 25;
	const int startY = 90;

	const int width = 75;
	const int height = 75;

	const int BufLen = 64;
	char buf[BufLen];

	m_pMapDesc = new RichText( this, "LabelMapDesc", true );
	m_pMapDesc->SetVerticalScrollbar( false );
	m_pMapName = new Label( this, "LabelMapName", "" );

	m_pNextButton = new CCareerButton( this, "ButtonNext", "#Career_MapNext", "gfx/career/icon_forward", true );
	m_pPlayButton = new CCareerButton( this, "ButtonPlay", "#Career_Start", "gfx/career/icon_forward", true );
	m_pCancelButton = new CCareerButton( this, "ButtonCancel", "#Career_Cancel", "gfx/career/icon_cancel", false );
	m_buttons.PutElement( m_pNextButton );
	m_buttons.PutElement( m_pPlayButton );
	m_buttons.PutElement( m_pCancelButton );

	for ( i=0; i<3; ++i )
	{
		_snprintf( buf, BufLen, "TripletBorder%d", i );
		if ( i == 1 )
		{
			CDottedBgLabel *pDottedLabel = new CDottedBgLabel( this, buf, "", 1000, 0, 0 );
			IScheme *pScheme = scheme()->GetIScheme( GetScheme() );
			Color col = pScheme->GetColor( "ActiveTriplet", Color( 0, 0, 0, 255 ) );
			pDottedLabel->SetColor( col );
			m_pTripletBorder[i] = pDottedLabel;
		}
		else
		{
			m_pTripletBorder[i] = new Label( this, buf, "" );
		}

		_snprintf( buf, BufLen, "TripletStatus%d", i );
		m_pTripletStatus[i] = new Label( this, buf, "" );
	}

	for ( int tripletNum=0; tripletNum<3; ++tripletNum )
	{
		for ( int mapNum=0; mapNum<3; ++mapNum )
		{
			_snprintf( buf, BufLen, "Map_%d_%d", tripletNum, mapNum );
			CMapButton *pButton = new CMapButton( this, buf, "", "" );
			pButton->SetContentAlignment( Label::a_northwest );
			pButton->SetEnabled( false );
			pButton->SetTabPosition( 0 );
			pButton->SetImagePadding( 1 );
			pButton->SetFillPadding( 4 );
			m_pMapButton[tripletNum][mapNum] = pButton;
		}
	}

	m_pMapScroll = new ScrollBar( this, "MapScrollbar", false );
	m_buttons.PutElement( m_pMapScroll->GetButton( 0 ) );
	m_buttons.PutElement( m_pMapScroll->GetButton( 1 ) );

	new CBitmapImagePanel( this, "TitlePanel", "gfx/career/Logo_CSCZ_Big" );
	new CBitmapImagePanel( this, "SelectLocationPanel", "gfx/career/hed_selectlocation" );
	m_pMapImage = new CBitmapImagePanel( this, "MapImagePanel", "resource/game_menu" );

	for ( i=0; i<MAX_CAREER_DIFFICULTY; ++i )
	{
		_snprintf( buf, BufLen, "MedalPanel%d", i );
		m_pMedalImage[i] = new CBitmapImagePanel( this, buf, "" );
	}

	LoadControlSettings( "Resource/CareerMapFrame.res" );

	m_numTriplets = TheCareerGame->GetMaps().size();
	m_pMapScroll->SetVisible( true );
	m_pMapScroll->AddActionSignalTarget( this );
	m_pMapScroll->SetRange( 0, m_numTriplets );
	m_pMapScroll->SetRangeWindow( 1 );
	m_pMapScroll->SetButtonPressedScrollValue( 1 );
	m_pMapScroll->SetSize( m_pMapScroll->GetWide(), 18 );

	m_lastTripletActive = -1;
	m_activeMapOffset = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCareerMapFrame::~CCareerMapFrame()
{
}

//-----------------------------------------------------------------------------
// Purpose: Colors and borders of the map list and the description
//-----------------------------------------------------------------------------
void CCareerMapFrame::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pMapImage->SetBorder( pScheme->GetBorder( "ButtonDepressedBorder" ) );

	Color mapColor;
	mapColor = pScheme->GetColor( "MapText", mapColor );
	m_pMapName->SetFgColor( mapColor );
	m_pMapDesc->SetFgColor( mapColor );
	m_pMapDesc->SetBgColor( Color( 0, 0, 0, 0 ) );

	m_pMapSelectionBackground->SetBgColor( pScheme->GetColor( "ControlDarkBG", Color( 0, 0, 0, 0 ) ) );
	m_pMapDescriptionBackground->SetBgColor( pScheme->GetColor( "ControlDarkBG", Color( 0, 0, 0, 0 ) ) );

	Button *pButton = m_pMapScroll->GetButton( 0 );
	pButton->SetArmedColor( pButton->GetSchemeColor( "DimBaseText", pScheme ), pButton->GetBgColor() );
	pButton->SetDepressedColor( pButton->GetSchemeColor( "DimBaseText", pScheme ), pButton->GetBgColor() );
	pButton->SetDefaultColor( pButton->GetFgColor(), pButton->GetBgColor() );

	pButton = m_pMapScroll->GetButton( 1 );
	pButton->SetArmedColor( pButton->GetSchemeColor( "DimBaseText", pScheme ), pButton->GetBgColor() );
	pButton->SetDepressedColor( pButton->GetSchemeColor( "DimBaseText", pScheme ), pButton->GetBgColor() );
	pButton->SetDefaultColor( pButton->GetFgColor(), pButton->GetBgColor() );

	m_pMapDesc->SetBorder( NULL );

	m_friendlyFireColor = pScheme->GetColor( "FriendlyFire", Color( 255, 255, 255, 255 ) );
	m_mapDescriptionColor = pScheme->GetColor( "MapText", Color( 255, 255, 255, 255 ) );

	m_pMapScroll->SetBorder( pScheme->GetBorder( "BrowserBorder" ) );
}

//-----------------------------------------------------------------------------
// Purpose: Message map
//-----------------------------------------------------------------------------
MessageMapItem_t CCareerMapFrame::m_MessageMap[] =
{
	{ "ScrollBarSliderMoved", (MessageFunc_t)(void (CCareerMapFrame::*)(void))&CCareerMapFrame::OnSliderMoved, 1, DATATYPE_INT, "position" },
};

IMPLEMENT_PANELMAP( CCareerMapFrame, BaseClass );

//-----------------------------------------------------------------------------
// Purpose: Shows the triplets around the scroll position
//-----------------------------------------------------------------------------
void CCareerMapFrame::OnSliderMoved( int position )
{
	position = max( 0, min( m_numTriplets - 1, position ) );
	if ( m_lastTripletActive == position )
		return;

	InvalidateLayout();
	Repaint();

	m_lastTripletActive = position;
	if ( !m_numTriplets )
		return;

	if ( m_lastTripletActive == 0 )
	{
		for ( int i=0; i<3; ++i )
		{
			m_pMapButton[0][i]->SetVisible( false );
		}
		m_pTripletBorder[0]->SetVisible( false );
		m_pTripletStatus[0]->SetVisible( false );
	}
	else if ( m_lastTripletActive == m_numTriplets - 1 )
	{
		for ( int i=0; i<3; ++i )
		{
			m_pMapButton[2][i]->SetVisible( false );
		}
		m_pTripletBorder[2]->SetVisible( false );
		m_pTripletStatus[2]->SetVisible( false );
	}

	if ( m_lastTripletActive != 0 )
	{
		for ( int i=0; i<3; ++i )
		{
			m_pMapButton[0][i]->SetVisible( true );
		}
		m_pTripletBorder[0]->SetVisible( true );
		m_pTripletStatus[0]->SetVisible( true );
	}

	if ( m_lastTripletActive != m_numTriplets - 1 )
	{
		for ( int i=0; i<3; ++i )
		{
			m_pMapButton[2][i]->SetVisible( true );
		}
		m_pTripletBorder[2]->SetVisible( true );
		m_pTripletStatus[2]->SetVisible( true );
	}

	const int BufLen = 128;
	char buf[BufLen];
	int col = m_lastTripletActive == 0;
	int triplet = max( 0, m_lastTripletActive - 1 );
	int numTriplets = ( m_lastTripletActive == 0 || m_lastTripletActive == m_numTriplets - 1 ) ? 2 : 3;

	if ( m_numTriplets == 1 )
	{
		col = 1;
		numTriplets = 1;

		int i;
		for ( i=0; i<3; ++i )
		{
			m_pMapButton[0][i]->SetVisible( false );
		}
		m_pTripletBorder[0]->SetVisible( false );
		m_pTripletStatus[0]->SetVisible( false );

		for ( i=0; i<3; ++i )
		{
			m_pMapButton[2][i]->SetVisible( false );
		}
		m_pTripletBorder[2]->SetVisible( false );
		m_pTripletStatus[2]->SetVisible( false );
	}

	for ( int x=col; x<col+numTriplets; ++x, ++triplet )
	{
		MapVec maps = TheCareerGame->GetMaps()[triplet];
		int numDefeated = 0;
		bool tripletUnlocked = TheCareerGame->GetPlayableMap( maps[0]->GetName() ) != NULL;
		for ( int y=0; y<3; ++y )
		{
			CCareerMap *pMap = maps[y];
			m_pMapButton[x][y]->SetEnabled( true );

			_snprintf( buf, BufLen, "#Career_Map_%s", pMap->GetName() );
			if ( !localize()->Find( buf ) )
			{
				_snprintf( buf, BufLen, "%s", pMap->GetName() );
			}
			m_pMapButton[x][y]->SetText( buf );
			m_pMapButton[x][y]->SetTextInset( 8, 8 );
			m_pMapButton[x][y]->SetDefeated( pMap->IsDefeated() );
			if ( pMap->IsDefeated() )
			{
				++numDefeated;
			}

			_snprintf( buf, BufLen, "gfx/thumbnails/maps/%s", pMap->GetName() );
			m_pMapButton[x][y]->SetTexture( buf );

			_snprintf( buf, BufLen, "map %d %s", tripletUnlocked, pMap->GetName() );
			m_pMapButton[x][y]->SetCommand( buf );
			m_pMapButton[x][y]->SetTabPosition( 0 );
			m_pMapButton[x][y]->IgnoreDoubleClicks_guess();
			m_pMapButton[x][y]->SetMapName( pMap->GetName() );
			m_pMapButton[x][y]->SetActive( false );
		}

		IScheme *pScheme = scheme()->GetIScheme( GetScheme() );
		Color col;
		wchar_t wbuf[128];
		wchar_t intbuf[2] = L"0";
		intbuf[0] = triplet + '1';
		localize()->ConstructString( wbuf, sizeof( wbuf ), localize()->Find( "#Career_TripletNum" ), 1, intbuf );
		m_pTripletBorder[x]->SetText( wbuf );
		col = pScheme->GetColor( "TourOfDuty", col );
		m_pTripletBorder[x]->SetFgColor( col );

		if ( tripletUnlocked && numDefeated < 3 )
		{
			m_pTripletStatus[x]->SetText( "#Career_Current" );
			col = pScheme->GetColor( "CurrentTriplet", col );
		}
		else if ( tripletUnlocked )
		{
			m_pTripletStatus[x]->SetText( "#Career_Unlocked" );
			col = pScheme->GetColor( "UnlockedTriplet", col );
		}
		else
		{
			m_pTripletStatus[x]->SetText( "#Career_Locked" );
			col = pScheme->GetColor( "LockedTriplet", col );
		}
		m_pTripletStatus[x]->SetFgColor( col );
	}

	m_pMapButton[1][m_activeMapOffset]->SetActive( true );
}

//-----------------------------------------------------------------------------
// Purpose: Rebuilds the triplets after Activate, closes on bad data
//-----------------------------------------------------------------------------
void CCareerMapFrame::OnThink()
{
	BaseClass::OnThink();

	if ( m_doPostActivate )
	{
		m_doPostActivate = false;
		m_lastTripletActive = -1;
		OnSliderMoved( m_pMapScroll->GetValue() );
	}

	if ( !m_numTriplets || !TheCareerGame->AreChecksumsOk() )
	{
		OnCommand( "Close" );
		MessageBox *box_guess = new MessageBox( "Cannot start game", "WARNING: Game data is corrupt or missing." );
		box_guess->DoModal();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Scrolls to the last map and shows the medals
//-----------------------------------------------------------------------------
void CCareerMapFrame::Activate()
{
	m_numTriplets = TheCareerGame->GetMaps().size();
	m_pMapScroll->SetRange( 0, m_numTriplets );

	SetPos( 0, 0 );
	BaseClass::Activate();

	GetNextButton()->RequestFocus();

	m_lastTripletActive = -1;
	int tripletNum = TheCareerGame->GetMapTripletNum( TheCareerGame->GetLastMap() );
	int tripletOffset = TheCareerGame->GetMapTripletOffset( TheCareerGame->GetLastMap() );
	m_pMapScroll->SetValue( tripletNum );
	OnSliderMoved( tripletNum );
	m_activeMapOffset = ( tripletOffset < 0 ) ? 0 : tripletOffset;
	m_doPostActivate = true;

	int i;
	for ( i=0; i<3; ++i )
	{
		for ( int j=0; j<3; ++j )
		{
			if ( m_pMapButton[i][j] )
			{
				m_pMapButton[i][j]->SetActive( false );
			}
		}
	}
	m_pMapButton[1][m_activeMapOffset]->SetActive( true );

	const int BufLen = 64;
	char buf[BufLen];
	for ( i=0; i<MAX_CAREER_DIFFICULTY; ++i )
	{
		_snprintf( buf, BufLen, "gfx/career/medals/Medal.0%d_%s", i+1, ( TheCareerGame->IsMedalEarned( (CareerDifficultyType)i ) ) ? "Active" : "Inactive" );
		m_pMedalImage[i]->setTexture( buf, true );
		m_pMedalImage[i]->SetVisible( true );
		m_pMedalImage[i]->MoveToFront();
	}
}

//-----------------------------------------------------------------------------
// Purpose: The next button, or the play button when there is no team page
//-----------------------------------------------------------------------------
Button *CCareerMapFrame::GetNextButton()
{
	if ( m_pNextButton && m_pNextButton->IsVisible() )
		return m_pNextButton;

	return m_pPlayButton;
}

//-----------------------------------------------------------------------------
// Purpose: Name, picture and description of the selected map
//-----------------------------------------------------------------------------
void CCareerMapFrame::ShowMapDescription( CCareerMap *pMap )
{
	if ( !pMap )
	{
		m_pMapDesc->SetText( L"" );
	}
	else
	{
		if ( pMap->AreTeammatesAllowed() )
		{
			m_pNextButton->SetVisible( true );
			m_pPlayButton->SetVisible( false );
		}
		else
		{
			m_pNextButton->SetVisible( false );
			m_pPlayButton->SetVisible( true );
		}

		const int BufLen = 128;
		char buf[BufLen];
		_snprintf( buf, BufLen, "#Career_Map_%s_CAPS", pMap->GetName() );
		if ( !localize()->Find( buf ) )
		{
			_snprintf( buf, BufLen, "%s", pMap->GetName() );
		}
		m_pMapName->SetText( buf );

		_snprintf( buf, BufLen, "gfx/thumbnails/maps_wide/%s", pMap->GetName() );
		m_pMapImage->setTexture( buf, true );

		ShowCareerMapDescription( m_pMapDesc, pMap, m_mapDescriptionColor, m_friendlyFireColor );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Map buttons, next and close
//-----------------------------------------------------------------------------
void CCareerMapFrame::OnCommand( const char *command )
{
	const char *remainder = command;
	if ( !SharedTokenWaiting( remainder ) )
		return;

	remainder = SharedParse( remainder );
	const char *cmd = SharedGetToken();
	if ( !stricmp( command, "close" ) )
	{
		OnClose();
		engine->pfnResetTutorMessageDecayData();
		g_pTaskbar->OnOpenNewCareerNameDialog();
	}
	else if ( !strcmp( cmd, "map" ) )
	{
		remainder = SharedParse( remainder );
		const char *tripletStr_guess = SharedGetToken();
		remainder = SharedParse( remainder );
		const char *mapToken_guess = SharedGetToken();
		if ( !tripletStr_guess || !mapToken_guess )
			return;

		int tripletIndex_guess = TheCareerGame->GetMapTripletNum( mapToken_guess );
		if ( tripletIndex_guess < 0 )
			return;

		m_pMapScroll->SetValue( tripletIndex_guess );
		for ( int i_guess=0; i_guess<3; ++i_guess )
		{
			for ( int j_guess=0; j_guess<3; ++j_guess )
			{
				if ( m_pMapButton[i_guess][j_guess] )
				{
					m_pMapButton[i_guess][j_guess]->SetActive( false );
				}
			}
		}

		int offset_guess = max( 0, TheCareerGame->GetMapTripletOffset( mapToken_guess ) );
		m_activeMapOffset = offset_guess;
		m_pMapButton[1][offset_guess]->SetActive( true );
		GetNextButton()->RequestFocus();
	}
	else if ( !strcmp( command, "next" ) )
	{
		Career_Printf( "Going to team page using map [%s]\n", TheCareerGame->GetLastMap() );
		OnClose();
		if ( TheCareerGame->GetLastMapPtr()->AreTeammatesAllowed() )
		{
			g_pTaskbar->OnOpenCareerBotDialog();
		}
		else
		{
			TheCareerGame->SendMatchCommand();
		}
	}
	else
	{
		Career_Printf( "Unknown command [%s] with map %s selected\n", command, TheCareerGame->GetLastMap() );
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: Moves the selection with the arrow keys
//-----------------------------------------------------------------------------
void CCareerMapFrame::ChangeActiveMap( int right, int down )
{
	int triplet = m_pMapScroll->GetValue() + right;
	m_pMapScroll->SetValue( triplet );

	for ( int i=0; i<3; ++i )
	{
		for ( int j=0; j<3; ++j )
		{
			if ( m_pMapButton[i][j] )
			{
				m_pMapButton[i][j]->SetActive( false );
			}
		}
	}

	m_activeMapOffset = max( 0, min( m_activeMapOffset + down, 2 ) );
	m_pMapButton[1][m_activeMapOffset]->SetActive( true );
	GetNextButton()->RequestFocus();
}

//-----------------------------------------------------------------------------
// Purpose: Escape, enter and the arrow keys
//-----------------------------------------------------------------------------
void CCareerMapFrame::OnKeyCodeTyped( KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		OnCommand( "close" );
	}
	else if ( code == KEY_ENTER )
	{
		BaseClass::OnKeyCodeTyped( KEY_SPACE );
	}
	else if ( code == KEY_RIGHT )
	{
		ChangeActiveMap( 1, 0 );
	}
	else if ( code == KEY_LEFT )
	{
		ChangeActiveMap( -1, 0 );
	}
	else if ( code == KEY_UP )
	{
		ChangeActiveMap( 0, -1 );
	}
	else if ( code == KEY_DOWN )
	{
		ChangeActiveMap( 0, 1 );
	}
	else
	{
		BaseClass::OnKeyCodeTyped( code );
	}
}
