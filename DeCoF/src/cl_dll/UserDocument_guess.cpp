// ======================================
// Cry of Fear computer user and password document panel
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "UserDocument_guess.h"
#include "..\game_shared\vgui_loadtga.h"

CUserDocument::CUserDocument( int iTrans, int iRemoveMe, int x, int y, int wide, int tall ) : CMenuPanel( iTrans, iRemoveMe, x, y, wide, tall )
{
	int iXPos, iYPos;
	int iWide, iTall;
	int iX, iY;

	m_pBackgroundImage_guess = vgui_LoadTGA( "gfx/vgui/backgroundtoall.tga" );
	if ( m_pBackgroundImage_guess )
		m_pBackgroundImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/backgroundtoall.tga!\n" );

	m_pBackgroundImage_guess->getSize( iWide, iTall );
	iX = ScreenWidth / 2 - iWide / 2;
	iY = ScreenHeight / 2 - iTall / 2;

	m_pBackground_guess = new ImagePanel( m_pBackgroundImage_guess );
	m_pBackground_guess->setParent( this );
	m_pBackground_guess->setPos( iX, iY );
	m_pBackground_guess->setVisible( true );

	iXPos = ScreenWidth / 2 - 320;
	iYPos = ScreenHeight / 2 - 240;

	setBgColor( Color( 0, 0, 0, 0 ) );

	m_pDocumentImage_guess = vgui_LoadTGA( "gfx/vgui/640_pass1.tga" );
	if ( m_pDocumentImage_guess )
		m_pDocumentImage_guess->setColor( Color( 255, 255, 255, 1 ) );
	else
		gEngfuncs.Con_Printf( "Cannot load gfx/vgui/640_pass1.tga!\n" );

	m_pDocument_guess = new ImagePanel( m_pDocumentImage_guess );
	m_pDocument_guess->setParent( this );
	m_pDocument_guess->setPos( iXPos, iYPos );
	m_pDocument_guess->setVisible( true );

	m_pCancelLabel_guess = new CImageLabel( "notecancel", iXPos - 5, iYPos - 5 );
	m_pCancelLabel_guess->setVisible( true );
	m_pCancelLabel_guess->setParent( this );

	m_pCloseButton_guess = new CommandButton( "", iXPos, iYPos, 20, 20, true, true );
	m_pCloseButton_guess->setParent( this );
	m_pCloseButton_guess->addActionSignal( new CUserDocumentHandler_Command( this, 1 ) );

	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hCreditsScheme = pSchemes->getSchemeHandle( "credits" );
	m_pFont_guess = pSchemes->getFont( hCreditsScheme );
}

void CUserDocument::ActionSignal( int cmd )
{
	switch ( cmd )
	{
	case 0:
		break;
	case 1:
		gViewPort->HideTopMenu();
		gEngfuncs.pfnServerCmd( "closedocument" );
		break;
	default:
		gEngfuncs.Con_DPrintf( "Unknown userdoc ActionSignal %i.\n", cmd );
		break;
	}
}

const char *CUserDocument::GetPassword_guess( int index )
{
	switch ( index )
	{
	case 1:
		return "apple45";
	case 2:
		return "cloudyday";
	case 3:
		return "rosesarered";
	case 4:
		return "junesummer";
	case 5:
		return "unforgettable";
	case 6:
		return "thebeatles54";
	case 7:
		return "marriageinapril";
	case 8:
		return "jillanddavid";
	}

	return "?";
}

const char *CUserDocument::GetUsername_guess( int index )
{
	switch ( index )
	{
	case 1:
		return "davidspc";
	case 2:
		return "jillusername";
	case 3:
		return "excelsior";
	case 4:
		return "dorrarpc";
	case 5:
		return "confuseduser";
	case 6:
		return "leatherhoff45";
	case 7:
		return "ea45";
	case 8:
		return "dorrardavid";
	}

	return "?";
}

void CUserDocument::Initialize()
{
	m_iType_guess = 0;
	m_iIndex_guess = 0;
}

int CUserDocument::KeyInput( int down, int keynum, const char *pszCurrentBinding )
{
	if ( pszCurrentBinding && !strcmp( pszCurrentBinding, "toggleconsole" ) )
		return 1;

	return 0;
}

int MsgUserDoc_guess( const char *pszName, int iSize, void *pbuf )
{
	char szImage[128];

	BEGIN_READ( pbuf, iSize );

	if ( gViewPort && gViewPort->m_pUserDocument_guess )
	{
		gViewPort->m_pUserDocument_guess->m_iType_guess = READ_BYTE();
		gViewPort->m_pUserDocument_guess->m_iIndex_guess = READ_BYTE();

		if ( gViewPort->m_pUserDocument_guess->m_iType_guess == 1 )
		{
			sprintf( szImage, "gfx/vgui/640_user%i.tga", gViewPort->m_pUserDocument_guess->m_iIndex_guess );
			gViewPort->m_pUserDocument_guess->m_pDocument_guess->setImage( vgui_LoadTGA( szImage ) );
		}

		if ( gViewPort->m_pUserDocument_guess->m_iType_guess == 2 )
		{
			sprintf( szImage, "gfx/vgui/640_pass%i.tga", gViewPort->m_pUserDocument_guess->m_iIndex_guess );
			gViewPort->m_pUserDocument_guess->m_pDocument_guess->setImage( vgui_LoadTGA( szImage ) );
		}

		gViewPort->ShowVGUIMenu( 34 );
	}

	return 1;
}

void UserDocInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "UserDoc", MsgUserDoc_guess );
}

void CUserDocument::paint()
{
	if ( m_iType_guess == 1 && m_iIndex_guess )
	{
		int wide, tall;
		const char *pszText = GetUsername_guess( m_iIndex_guess );

		drawSetTextFont( m_pFont_guess );
		drawSetTextColor( 200, 200, 200, 1 );
		m_pFont_guess->getTextSize( pszText, wide, tall );
		drawSetTextPos( ScreenWidth / 2 - wide / 2, ScreenHeight - tall * 2 );
		drawPrintText( pszText, strlen( pszText ) );
	}

	if ( m_iType_guess == 2 && m_iIndex_guess )
	{
		int wide, tall;
		const char *pszText = GetPassword_guess( m_iIndex_guess );

		drawSetTextFont( m_pFont_guess );
		drawSetTextColor( 200, 200, 200, 1 );
		m_pFont_guess->getTextSize( pszText, wide, tall );
		drawSetTextPos( ScreenWidth / 2 - wide / 2, ScreenHeight - tall * 2 );
		drawPrintText( pszText, strlen( pszText ) );
	}

	Panel::paint();
}
