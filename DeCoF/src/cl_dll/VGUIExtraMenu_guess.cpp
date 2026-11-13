// ======================================
// Cry of Fear extras menu
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "vgui_TeamFortressViewport.h"
#include "VGUIExtraMenu_guess.h"
#include "GameMenu_guess.h"
#include "..\game_shared\vgui_loadtga.h"

//=========================================================
// Declarations this file takes from the Steamworks SDK
// (steam_api.h, isteamfriends.h, steamclientpublic.h)
//=========================================================
typedef unsigned __int64 uint64;
typedef uint64 SteamAPICall_t;

class CSteamID
{
private:
	uint64 m_unAll64Bits;
};

struct FriendGameInfo_t;

class ISteamFriends
{
public:
	virtual const char *GetPersonaName() = 0;
	virtual SteamAPICall_t SetPersonaName( const char *pchPersonaName ) = 0;
	virtual int GetPersonaState() = 0;
	virtual int GetFriendCount( int iFriendFlags ) = 0;
	virtual CSteamID GetFriendByIndex( int iFriend, int iFriendFlags ) = 0;
	virtual int GetFriendRelationship( CSteamID steamIDFriend ) = 0;
	virtual int GetFriendPersonaState( CSteamID steamIDFriend ) = 0;
	virtual const char *GetFriendPersonaName( CSteamID steamIDFriend ) = 0;
	virtual bool GetFriendGamePlayed( CSteamID steamIDFriend, FriendGameInfo_t *pFriendGameInfo ) = 0;
	virtual const char *GetFriendPersonaNameHistory( CSteamID steamIDFriend, int iPersonaName ) = 0;
	virtual bool HasFriend( CSteamID steamIDFriend, int iFriendFlags ) = 0;
	virtual int GetClanCount() = 0;
	virtual CSteamID GetClanByIndex( int iClan ) = 0;
	virtual const char *GetClanName( CSteamID steamIDClan ) = 0;
	virtual const char *GetClanTag( CSteamID steamIDClan ) = 0;
	virtual bool GetClanActivityCounts( CSteamID steamIDClan, int *pnOnline, int *pnInGame, int *pnChatting ) = 0;
	virtual SteamAPICall_t DownloadClanActivityCounts( CSteamID *psteamIDClans, int cClansToRequest ) = 0;
	virtual int GetFriendCountFromSource( CSteamID steamIDSource ) = 0;
	virtual CSteamID GetFriendFromSourceByIndex( CSteamID steamIDSource, int iFriend ) = 0;
	virtual bool IsUserInSource( CSteamID steamIDUser, CSteamID steamIDSource ) = 0;
	virtual void SetInGameVoiceSpeaking( CSteamID steamIDUser, bool bSpeaking ) = 0;
	virtual void ActivateGameOverlay( const char *pchDialog ) = 0;
	virtual void ActivateGameOverlayToUser( const char *pchDialog, CSteamID steamID ) = 0;
	virtual void ActivateGameOverlayToWebPage( const char *pchURL ) = 0;
};

extern "C" __declspec( dllimport ) ISteamFriends * __cdecl SteamFriends();
//=========================================================

CVGUIExtraMenu::CVGUIExtraMenu() : Panel( ScreenWidth / 2 - 400, ScreenHeight / 2 - 300, 800, 600 )
{
	int wide, tall;
	int cancelWide, cancelTall;

	m_pDonateImage_guess = vgui_LoadTGA( "gfx/vgui/menu/donate.tga" );
	m_pDonateImage_guess->setColor( Color( 255, 255, 255, 0 ) );

	m_pSoundtrack1Image_guess = vgui_LoadTGA( "gfx/vgui/menu/soundtrack1.tga" );
	m_pSoundtrack1Image_guess->setColor( Color( 255, 255, 255, 0 ) );

	m_pSoundtrack2Image_guess = vgui_LoadTGA( "gfx/vgui/menu/soundtrack2.tga" );
	m_pSoundtrack2Image_guess->setColor( Color( 255, 255, 255, 0 ) );

	m_pClothesImage_guess = vgui_LoadTGA( "gfx/vgui/menu/clothes.tga" );
	m_pClothesImage_guess->setColor( Color( 255, 255, 255, 0 ) );

	m_pAoMDCImage_guess = vgui_LoadTGA( "gfx/vgui/menu/aomdc.tga" );
	m_pAoMDCImage_guess->setColor( Color( 255, 255, 255, 0 ) );

	m_pFacebookImage_guess = vgui_LoadTGA( "gfx/vgui/menu/facebook.tga" );
	m_pFacebookImage_guess->setColor( Color( 255, 255, 255, 0 ) );

	m_pCancelImage_guess = vgui_LoadTGA( "gfx/vgui/menu/cancel.tga" );
	m_pCancelImage_guess->setColor( Color( 255, 255, 255, 0 ) );

	m_pDonateImage_guess->getSize( wide, tall );
	m_pCancelImage_guess->getSize( cancelWide, cancelTall );

	m_pDonate_guess = new ImagePanel( m_pDonateImage_guess );
	m_pDonate_guess->setParent( this );
	m_pDonate_guess->setPos( 400 - wide / 2, 0 );
	m_pDonate_guess->setVisible( true );

	m_pSoundtrack1_guess = new ImagePanel( m_pSoundtrack1Image_guess );
	m_pSoundtrack1_guess->setParent( this );
	m_pSoundtrack1_guess->setPos( 400 - wide / 2, tall );
	m_pSoundtrack1_guess->setVisible( true );

	m_pSoundtrack2_guess = new ImagePanel( m_pSoundtrack2Image_guess );
	m_pSoundtrack2_guess->setParent( this );
	m_pSoundtrack2_guess->setPos( 400 - wide / 2, tall * 2 );
	m_pSoundtrack2_guess->setVisible( true );

	m_pClothes_guess = new ImagePanel( m_pClothesImage_guess );
	m_pClothes_guess->setParent( this );
	m_pClothes_guess->setPos( 400 - wide / 2, tall * 3 );
	m_pClothes_guess->setVisible( true );

	m_pAoMDC_guess = new ImagePanel( m_pAoMDCImage_guess );
	m_pAoMDC_guess->setParent( this );
	m_pAoMDC_guess->setPos( 400 - wide / 2, tall * 4 );
	m_pAoMDC_guess->setVisible( true );

	m_pFacebook_guess = new ImagePanel( m_pFacebookImage_guess );
	m_pFacebook_guess->setParent( this );
	m_pFacebook_guess->setPos( 400 - wide / 2, tall * 5 );
	m_pFacebook_guess->setVisible( true );

	m_pCancel_guess = new ImagePanel( m_pCancelImage_guess );
	m_pCancel_guess->setParent( this );
	m_pCancel_guess->setPos( 400 - cancelWide / 2, 600 - cancelTall );
	m_pCancel_guess->setVisible( true );

	m_pDonateButton_guess = new ColorButton( "", 400 - wide / 2, 0, wide, tall, false, true );
	m_pDonateButton_guess->setParent( this );
	m_pDonateButton_guess->addActionSignal( new CVGUIExtraMenuHandler_Command( this, 2 ) );

	m_pSoundtrack1Button_guess = new ColorButton( "", 400 - wide / 2, tall, wide, tall, false, true );
	m_pSoundtrack1Button_guess->setParent( this );
	m_pSoundtrack1Button_guess->addActionSignal( new CVGUIExtraMenuHandler_Command( this, 3 ) );

	m_pSoundtrack2Button_guess = new ColorButton( "", 400 - wide / 2, tall * 2, wide, tall, false, true );
	m_pSoundtrack2Button_guess->setParent( this );
	m_pSoundtrack2Button_guess->addActionSignal( new CVGUIExtraMenuHandler_Command( this, 4 ) );

	m_pClothesButton_guess = new ColorButton( "", 400 - wide / 2, tall * 3, wide, tall, false, true );
	m_pClothesButton_guess->setParent( this );
	m_pClothesButton_guess->addActionSignal( new CVGUIExtraMenuHandler_Command( this, 5 ) );

	m_pAoMDCButton_guess = new ColorButton( "", 400 - wide / 2, tall * 4, wide, tall, false, true );
	m_pAoMDCButton_guess->setParent( this );
	m_pAoMDCButton_guess->addActionSignal( new CVGUIExtraMenuHandler_Command( this, 6 ) );

	m_pFacebookButton_guess = new ColorButton( "", 400 - wide / 2, tall * 5, wide, tall, false, true );
	m_pFacebookButton_guess->setParent( this );
	m_pFacebookButton_guess->addActionSignal( new CVGUIExtraMenuHandler_Command( this, 7 ) );

	m_pCancelButton_guess = new ColorButton( "", 400 - cancelWide / 2, 600 - cancelTall, cancelWide, cancelTall, false, true );
	m_pCancelButton_guess->setParent( this );
	m_pCancelButton_guess->addActionSignal( new CVGUIExtraMenuHandler_Command( this, 1 ) );

	setBgColor( 0, 0, 0, 255 );

	Initialize();
}

CVGUIExtraMenu::~CVGUIExtraMenu()
{
	delete m_pDonateImage_guess;
	delete m_pSoundtrack1Image_guess;
	delete m_pSoundtrack2Image_guess;
	delete m_pClothesImage_guess;
	delete m_pAoMDCImage_guess;
	delete m_pFacebookImage_guess;
	delete m_pCancelImage_guess;
}

void CVGUIExtraMenu::ActionSignal( int cmd )
{
	switch ( cmd )
	{
	case 1:
		setVisible( false );
		gViewPort->UpdateCursorState();

		if ( gViewPort && gViewPort->m_pGameMenu_guess )
		{
			gViewPort->m_pGameMenu_guess->setVisible( true );
			gViewPort->UpdateCursorState();
		}
		break;
	case 2:
		if ( SteamFriends() )
			SteamFriends()->ActivateGameOverlayToWebPage( "http://www.cry-of-fear.com/donate.php" );

		gEngfuncs.pfnPlaySoundByName( "ui/3d_click.wav", 1 );
		break;
	case 3:
		if ( SteamFriends() )
			SteamFriends()->ActivateGameOverlayToWebPage( "http://www.cdbaby.com/cd/andreasronnberg" );

		gEngfuncs.pfnPlaySoundByName( "ui/3d_click.wav", 1 );
		break;
	case 4:
		if ( SteamFriends() )
			SteamFriends()->ActivateGameOverlayToWebPage( "http://www.cdbaby.com/cd/andreasronnberg2" );

		gEngfuncs.pfnPlaySoundByName( "ui/3d_click.wav", 1 );
		break;
	case 5:
		if ( SteamFriends() )
			SteamFriends()->ActivateGameOverlayToWebPage( "http://teampsykskallar.spreadshirt.se/" );

		gEngfuncs.pfnPlaySoundByName( "ui/3d_click.wav", 1 );
		break;
	case 6:
		if ( SteamFriends() )
			SteamFriends()->ActivateGameOverlayToWebPage( "http://www.moddb.com/mods/afraid-of-monsters-dc" );

		gEngfuncs.pfnPlaySoundByName( "ui/3d_click.wav", 1 );
		break;
	case 7:
		if ( SteamFriends() )
			SteamFriends()->ActivateGameOverlayToWebPage( "http://www.facebook.com/CryOfFearOfficial" );

		gEngfuncs.pfnPlaySoundByName( "ui/3d_click.wav", 1 );
		break;
	}
}

void CVGUIExtraMenu::Initialize()
{
	setVisible( false );
}

void CVGUIExtraMenu::setVisible( bool state )
{
	if ( state )
	{
		m_bDonateArmed_guess = false;
		m_bSoundtrack1Armed_guess = false;
		m_bSoundtrack2Armed_guess = false;
		m_bClothesArmed_guess = false;
		m_bAoMDCArmed_guess = false;
		m_bFacebookArmed_guess = false;
		m_bCancelArmed_guess = false;
	}

	Panel::setVisible( state );
}

void CVGUIExtraMenu::solve()
{
	if ( m_pDonateButton_guess->isArmed() )
	{
		if ( !m_bDonateArmed_guess )
		{
			m_pDonate_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/donate_mo.tga" ) );
			gEngfuncs.pfnPlaySoundByName( "ui/3d_rollover.wav", 1 );
			m_bDonateArmed_guess = true;
		}
	}
	else if ( m_bDonateArmed_guess )
	{
		m_pDonate_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/donate.tga" ) );
		m_bDonateArmed_guess = false;
	}

	if ( m_pSoundtrack1Button_guess->isArmed() )
	{
		if ( !m_bSoundtrack1Armed_guess )
		{
			m_pSoundtrack1_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/soundtrack1_mo.tga" ) );
			gEngfuncs.pfnPlaySoundByName( "ui/3d_rollover.wav", 1 );
			m_bSoundtrack1Armed_guess = true;
		}
	}
	else if ( m_bSoundtrack1Armed_guess )
	{
		m_pSoundtrack1_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/soundtrack1.tga" ) );
		m_bSoundtrack1Armed_guess = false;
	}

	if ( m_pSoundtrack2Button_guess->isArmed() )
	{
		if ( !m_bSoundtrack2Armed_guess )
		{
			m_pSoundtrack2_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/soundtrack2_mo.tga" ) );
			gEngfuncs.pfnPlaySoundByName( "ui/3d_rollover.wav", 1 );
			m_bSoundtrack2Armed_guess = true;
		}
	}
	else if ( m_bSoundtrack2Armed_guess )
	{
		m_pSoundtrack2_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/soundtrack2.tga" ) );
		m_bSoundtrack2Armed_guess = false;
	}

	if ( m_pClothesButton_guess->isArmed() )
	{
		if ( !m_bClothesArmed_guess )
		{
			m_pClothes_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/clothes_mo.tga" ) );
			gEngfuncs.pfnPlaySoundByName( "ui/3d_rollover.wav", 1 );
			m_bClothesArmed_guess = true;
		}
	}
	else if ( m_bClothesArmed_guess )
	{
		m_pClothes_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/clothes.tga" ) );
		m_bClothesArmed_guess = false;
	}

	if ( m_pAoMDCButton_guess->isArmed() )
	{
		if ( !m_bAoMDCArmed_guess )
		{
			m_pAoMDC_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/aomdc_mo.tga" ) );
			gEngfuncs.pfnPlaySoundByName( "ui/3d_rollover.wav", 1 );
			m_bAoMDCArmed_guess = true;
		}
	}
	else if ( m_bAoMDCArmed_guess )
	{
		m_pAoMDC_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/aomdc.tga" ) );
		m_bAoMDCArmed_guess = false;
	}

	if ( m_pFacebookButton_guess->isArmed() )
	{
		if ( !m_bFacebookArmed_guess )
		{
			m_pFacebook_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/facebook_mo.tga" ) );
			gEngfuncs.pfnPlaySoundByName( "ui/3d_rollover.wav", 1 );
			m_bFacebookArmed_guess = true;
		}
	}
	else if ( m_bFacebookArmed_guess )
	{
		m_pFacebook_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/facebook.tga" ) );
		m_bFacebookArmed_guess = false;
	}

	if ( m_pCancelButton_guess->isArmed() )
	{
		if ( !m_bCancelArmed_guess )
		{
			m_pCancel_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/cancel_mo.tga" ) );
			gEngfuncs.pfnPlaySoundByName( "ui/3d_rollover.wav", 1 );
			m_bCancelArmed_guess = true;
		}
	}
	else if ( m_bCancelArmed_guess )
	{
		m_pCancel_guess->setImage( vgui_LoadTGA( "gfx/vgui/menu/cancel.tga" ) );
		m_bCancelArmed_guess = false;
	}

	Panel::solve();
}
