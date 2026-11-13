// ======================================
// Cry of Fear note document panel
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include <fstream>
#include <string>
#include <vector>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "NoteDocument_guess.h"
#include "..\game_shared\vgui_loadtga.h"

CNoteDocument::CNoteDocument( int iTrans, int iRemoveMe, int x, int y, int wide, int tall ) : CMenuPanel( iTrans, iRemoveMe, x, y, wide, tall )
{
	int iXPos, iYPos;
	int iWide, iTall;
	int iX, iY;
	Panel *pPanel;
	CommandButton *pButton;

	m_pLineLabel_guess = NULL;
	Initialize();

	iXPos = ScreenWidth / 2 - 320;
	iYPos = ScreenHeight / 2 - 240;

	setBgColor( Color( 0, 0, 0, 0 ) );

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

	pPanel = new Panel( 0, 0, 0, 0 );
	pPanel->setParent( this );
	pPanel->setSize( ScreenWidth, ScreenHeight );
	pPanel->setBgColor( 0, 0, 0, 128 );
	pPanel->setVisible( true );

	m_pPanel_guess = new CTransparentPanel( 0, iXPos, iYPos, 640, 480 );
	m_pPanel_guess->setParent( this );
	m_pPanel_guess->setPaintBorderEnabled( false );
	m_pPanel_guess->setBgColor( Color( 0, 0, 0, 255 ) );

	m_pImageLabel_guess = new CImageLabel( m_szImage1_guess, -5, -5 );
	m_pImageLabel_guess->setVisible( true );
	m_pImageLabel_guess->setParent( m_pPanel_guess );

	m_pCancelLabel_guess = new CImageLabel( "notecancel", -5, -5 );
	m_pCancelLabel_guess->setVisible( true );
	m_pCancelLabel_guess->setParent( m_pPanel_guess );

	m_pCloseButton_guess = new CommandButton( "", 0, 0, 20, 20, true, true );
	m_pCloseButton_guess->setParent( m_pPanel_guess );
	m_pCloseButton_guess->addActionSignal( new CNoteDocumentHandler_Command( this, 3 ) );

	pButton = new CommandButton( "", 38, 411, 33, 33, true, true );
	pButton->setParent( m_pPanel_guess );
	pButton->addActionSignal( new CNoteDocumentHandler_Command( this, 1 ) );

	pButton = new CommandButton( "", 38, 447, 33, 33, true, true );
	pButton->setParent( m_pPanel_guess );
	pButton->addActionSignal( new CNoteDocumentHandler_Command( this, 2 ) );

	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hCreditsScheme = pSchemes->getSchemeHandle( "credits" );
	m_pFont_guess = pSchemes->getFont( hCreditsScheme );

	m_pLineLabel_guess = new Label( "", 0, 0, ScreenWidth, 64 );
	m_pLineLabel_guess->setParent( this );
	m_pLineLabel_guess->setFont( m_pFont_guess );
	m_pLineLabel_guess->setFgColor( 255, 255, 255, 0 );
	m_pLineLabel_guess->setBgColor( 0, 0, 0, 255 );
	m_pLineLabel_guess->setText( "" );
	m_pLineLabel_guess->setContentAlignment( Label::a_center );
}

void CNoteDocument::ActionSignal( int cmd )
{
	switch ( cmd )
	{
	case 1:
		PrevLine_guess();
		break;
	case 2:
		NextLine_guess();
		break;
	case 3:
		gViewPort->HideTopMenu();
		gEngfuncs.pfnServerCmd( "closedocument" );
		break;
	}
}

void CNoteDocument::ClearString_guess( char *str )
{
	for ( int i = 0; i < strlen( str ); i++ )
		str[i] = 0;
}

int CNoteDocument::CountLines_guess()
{
	char szPath[256];

	strcpy( szPath, gEngfuncs.pfnGetGameDirectory() );
	strcat( szPath, "/notes/" );

	switch ( m_iPage_guess )
	{
	case 1:
		strcat( szPath, m_szText1_guess );
		break;
	case 2:
		strcat( szPath, m_szText2_guess );
		break;
	case 3:
		strcat( szPath, m_szText3_guess );
		break;
	case 4:
		strcat( szPath, m_szText4_guess );
		break;
	}

	std::ifstream file( szPath );
	std::string line;
	std::vector<std::string> lines;

	while ( std::getline( file, line ) )
		lines.push_back( line );

	return lines.size();
}

void CNoteDocument::Initialize()
{
	if ( m_pLineLabel_guess )
		m_pLineLabel_guess->setText( "" );

	m_iLine_guess = 0;
	m_iPage_guess = 0;

	strncpy( m_szImage1_guess, "", 16 );
	strncpy( m_szImage2_guess, "", 16 );
	strncpy( m_szImage3_guess, "", 16 );
	strncpy( m_szImage4_guess, "", 16 );
	strncpy( m_szSound_guess, "", 48 );
	strncpy( m_szText1_guess, "", 16 );
	strncpy( m_szText2_guess, "", 16 );
	strncpy( m_szText3_guess, "", 16 );
	strncpy( m_szText4_guess, "", 16 );
	strncpy( m_szLine_guess, "", 128 );

	m_bTitle_guess = false;
}

int CNoteDocument::KeyInput( int down, int keynum, const char *pszCurrentBinding )
{
	if ( pszCurrentBinding && !strcmp( pszCurrentBinding, "toggleconsole" ) )
		return 1;

	return 0;
}

int MsgNoteDoc_guess( const char *pszName, int iSize, void *pbuf )
{
	char szText1[256];
	char szText2[256];
	char szText3[256];
	char szText4[256];

	BEGIN_READ( pbuf, iSize );

	if ( gViewPort->m_pNoteDocument_guess )
	{
		gViewPort->m_pNoteDocument_guess->ClearString_guess( gViewPort->m_pNoteDocument_guess->m_szImage1_guess );
		gViewPort->m_pNoteDocument_guess->ClearString_guess( gViewPort->m_pNoteDocument_guess->m_szImage2_guess );
		gViewPort->m_pNoteDocument_guess->ClearString_guess( gViewPort->m_pNoteDocument_guess->m_szImage3_guess );
		gViewPort->m_pNoteDocument_guess->ClearString_guess( gViewPort->m_pNoteDocument_guess->m_szImage4_guess );
		gViewPort->m_pNoteDocument_guess->ClearString_guess( gViewPort->m_pNoteDocument_guess->m_szText1_guess );
		gViewPort->m_pNoteDocument_guess->ClearString_guess( gViewPort->m_pNoteDocument_guess->m_szSound_guess );

		strcpy( gViewPort->m_pNoteDocument_guess->m_szImage1_guess, READ_STRING() );
		strcpy( gViewPort->m_pNoteDocument_guess->m_szImage2_guess, READ_STRING() );
		strcpy( gViewPort->m_pNoteDocument_guess->m_szImage3_guess, READ_STRING() );
		strcpy( gViewPort->m_pNoteDocument_guess->m_szImage4_guess, READ_STRING() );
		strcpy( szText1, READ_STRING() );
		strcpy( szText2, READ_STRING() );
		strcpy( szText3, READ_STRING() );
		strcpy( szText4, READ_STRING() );
		strcpy( gViewPort->m_pNoteDocument_guess->m_szSound_guess, READ_STRING() );

		int iLanguage = CVAR_GET_FLOAT( "cof_subtitlelanguage" );

		if ( strstr( szText1, "apnote" ) || strstr( szText1, "magazine" ) || strstr( szText1, "purnellnote" ) || strstr( szText1, "sewernote" ) || strstr( szText1, "subnote" ) )
		{
			switch ( iLanguage )
			{
			case 0:
			case 1:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText1_guess, szText1 );
				break;
			case 2:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText1_guess, "languages/dutch/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText1_guess, szText1 );
				break;
			case 3:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText1_guess, "languages/french/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText1_guess, szText1 );
				break;
			case 4:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText1_guess, "languages/german/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText1_guess, szText1 );
				break;
			case 5:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText1_guess, "languages/norwegian/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText1_guess, szText1 );
				break;
			case 6:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText1_guess, "languages/spanish/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText1_guess, szText1 );
				break;
			case 7:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText1_guess, "languages/swedish/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText1_guess, szText1 );
				break;
			}
		}

		if ( strstr( szText2, "apnote" ) || strstr( szText2, "magazine" ) || strstr( szText2, "purnellnote" ) || strstr( szText2, "sewernote" ) || strstr( szText2, "subnote" ) )
		{
			switch ( iLanguage )
			{
			case 0:
			case 1:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText2_guess, szText2 );
				break;
			case 2:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText2_guess, "languages/dutch/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText2_guess, szText2 );
				break;
			case 3:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText2_guess, "languages/french/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText2_guess, szText2 );
				break;
			case 4:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText2_guess, "languages/german/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText2_guess, szText2 );
				break;
			case 5:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText2_guess, "languages/norwegian/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText2_guess, szText2 );
				break;
			case 6:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText2_guess, "languages/spanish/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText2_guess, szText2 );
				break;
			case 7:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText2_guess, "languages/swedish/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText2_guess, szText2 );
				break;
			}
		}

		if ( strstr( szText3, "apnote" ) || strstr( szText3, "magazine" ) || strstr( szText3, "purnellnote" ) || strstr( szText3, "sewernote" ) || strstr( szText3, "subnote" ) )
		{
			switch ( iLanguage )
			{
			case 0:
			case 1:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText3_guess, szText3 );
				break;
			case 2:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText3_guess, "languages/dutch/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText3_guess, szText3 );
				break;
			case 3:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText3_guess, "languages/french/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText3_guess, szText3 );
				break;
			case 4:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText3_guess, "languages/german/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText3_guess, szText3 );
				break;
			case 5:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText3_guess, "languages/norwegian/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText3_guess, szText3 );
				break;
			case 6:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText3_guess, "languages/spanish/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText3_guess, szText3 );
				break;
			case 7:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText3_guess, "languages/swedish/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText3_guess, szText3 );
				break;
			}
		}

		if ( strstr( szText4, "apnote" ) || strstr( szText4, "magazine" ) || strstr( szText4, "purnellnote" ) || strstr( szText4, "sewernote" ) || strstr( szText4, "subnote" ) )
		{
			switch ( iLanguage )
			{
			case 0:
			case 1:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText4_guess, szText4 );
				break;
			case 2:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText4_guess, "languages/dutch/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText4_guess, szText4 );
				break;
			case 3:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText4_guess, "languages/french/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText4_guess, szText4 );
				break;
			case 4:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText4_guess, "languages/german/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText4_guess, szText4 );
				break;
			case 5:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText4_guess, "languages/norwegian/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText4_guess, szText4 );
				break;
			case 6:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText4_guess, "languages/spanish/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText4_guess, szText4 );
				break;
			case 7:
				strcpy( gViewPort->m_pNoteDocument_guess->m_szText4_guess, "languages/swedish/" );
				strcat( gViewPort->m_pNoteDocument_guess->m_szText4_guess, szText4 );
				break;
			}
		}

		gViewPort->m_pNoteDocument_guess->m_iPage_guess = 1;
		gViewPort->m_pNoteDocument_guess->SetPage_guess( 1 );
	}

	gViewPort->ShowVGUIMenu( 31 );
	return 1;
}

void CNoteDocument::NextLine_guess()
{
	char szPath[256];

	strcpy( szPath, gEngfuncs.pfnGetGameDirectory() );
	strcat( szPath, "/notes/" );

	switch ( m_iPage_guess )
	{
	case 1:
		strcat( szPath, m_szText1_guess );
		break;
	case 2:
		strcat( szPath, m_szText2_guess );
		break;
	case 3:
		strcat( szPath, m_szText3_guess );
		break;
	case 4:
		strcat( szPath, m_szText4_guess );
		break;
	}

	std::ifstream file( szPath );
	std::string line;
	std::string strUnused_guess;

	m_iLine_guess++;

	if ( m_iLine_guess > CountLines_guess() )
		m_iLine_guess = 1;

	line = m_iLine_guess;

	int i = 1;

	while ( std::getline( file, line ) )
	{
		if ( i == m_iLine_guess )
		{
			char szLine[256];
			strcpy( szLine, line.c_str() );
			szLine[line.length()] = 0;

			m_bTitle_guess = ( i == 1 );
			strcpy( m_szLine_guess, szLine );
		}

		i++;
	}
}

void CNoteDocument::NextPage_guess()
{
	switch ( m_iPage_guess )
	{
	case 1:
		if ( m_szImage2_guess[1] )
		{
			m_iPage_guess = 2;
			SetPage_guess( 2 );
		}
		break;
	case 2:
		if ( m_szImage3_guess[1] )
		{
			m_iPage_guess = 3;
			SetPage_guess( 3 );
			return;
		}

		m_iPage_guess = 1;
		SetPage_guess( 1 );
		break;
	case 3:
		if ( m_szImage4_guess[1] )
		{
			m_iPage_guess = 4;
			SetPage_guess( 4 );
			return;
		}

		m_iPage_guess = 1;
		SetPage_guess( 1 );
		break;
	case 4:
		m_iPage_guess = 1;
		SetPage_guess( 1 );
		break;
	}
}

void NoteDocInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "NoteDoc", MsgNoteDoc_guess );
}

void CNoteDocument::PrevLine_guess()
{
	char szPath[256];

	strcpy( szPath, gEngfuncs.pfnGetGameDirectory() );
	strcat( szPath, "/notes/" );

	switch ( m_iPage_guess )
	{
	case 1:
		strcat( szPath, m_szText1_guess );
		break;
	case 2:
		strcat( szPath, m_szText2_guess );
		break;
	case 3:
		strcat( szPath, m_szText3_guess );
		break;
	case 4:
		strcat( szPath, m_szText4_guess );
		break;
	}

	std::ifstream file( szPath );
	std::string line;
	std::string strUnused_guess;

	m_iLine_guess--;

	if ( m_iLine_guess <= 0 )
		m_iLine_guess = CountLines_guess();

	line = m_iLine_guess;

	int i = 1;

	while ( std::getline( file, line ) )
	{
		if ( i == m_iLine_guess )
		{
			char szLine[256];
			strcpy( szLine, line.c_str() );
			szLine[line.length()] = 0;

			m_bTitle_guess = ( i == 1 );
			strcpy( m_szLine_guess, szLine );
		}

		i++;
	}
}

void CNoteDocument::PrevPage_guess()
{
	switch ( m_iPage_guess )
	{
	case 1:
		if ( m_szImage4_guess[1] )
		{
			m_iPage_guess = 4;
			SetPage_guess( 4 );
			return;
		}
		if ( m_szImage3_guess[1] )
		{
			m_iPage_guess = 3;
			SetPage_guess( 3 );
			return;
		}
		if ( m_szImage2_guess[1] )
		{
			m_iPage_guess = 2;
			SetPage_guess( 2 );
			return;
		}
	case 2:
		if ( m_szImage1_guess[1] && m_iPage_guess != 1 )
		{
			m_iPage_guess = 1;
			SetPage_guess( 1 );
			return;
		}
		if ( m_szImage4_guess[1] )
		{
			m_iPage_guess = 4;
			SetPage_guess( 4 );
			return;
		}
		if ( m_szImage3_guess[1] )
		{
			m_iPage_guess = 3;
			SetPage_guess( 3 );
			return;
		}
	case 3:
		if ( m_szImage2_guess[1] )
		{
			m_iPage_guess = 2;
			SetPage_guess( 2 );
			return;
		}
		if ( m_szImage1_guess[1] )
		{
			m_iPage_guess = 1;
			SetPage_guess( 1 );
			return;
		}
		if ( m_szImage4_guess[1] )
		{
			m_iPage_guess = 4;
			SetPage_guess( 4 );
			return;
		}
	case 4:
		if ( m_szImage3_guess[1] )
		{
			m_iPage_guess = 3;
			SetPage_guess( 3 );
			return;
		}
		if ( m_szImage2_guess[1] )
		{
			m_iPage_guess = 2;
			SetPage_guess( 2 );
			return;
		}
		if ( m_szImage1_guess[1] )
		{
			m_iPage_guess = 1;
			SetPage_guess( 1 );
			return;
		}
	}
}

void CNoteDocument::SetPage_guess( int page )
{
	BitmapTGA *pImage;

	switch ( page )
	{
	case 1:
		pImage = LoadTGAForRes( m_szImage1_guess );
		break;
	case 2:
		pImage = LoadTGAForRes( m_szImage2_guess );
		break;
	case 3:
		pImage = LoadTGAForRes( m_szImage3_guess );
		break;
	case 4:
		pImage = LoadTGAForRes( m_szImage4_guess );
		break;
	}

	if ( m_szSound_guess[1] )
		gEngfuncs.pfnPlaySoundByName( m_szSound_guess, 1 );

	m_iLine_guess = 1;
	SetText_guess();

	m_pImageLabel_guess->setImage( pImage );
}

void CNoteDocument::SetText_guess()
{
	char szPath[256];

	strcpy( szPath, gEngfuncs.pfnGetGameDirectory() );
	strcat( szPath, "/notes/" );

	switch ( m_iPage_guess )
	{
	case 1:
		strcat( szPath, m_szText1_guess );
		break;
	case 2:
		strcat( szPath, m_szText2_guess );
		break;
	case 3:
		strcat( szPath, m_szText3_guess );
		break;
	case 4:
		strcat( szPath, m_szText4_guess );
		break;
	}

	std::ifstream file( szPath );
	std::string line;
	std::string strUnused_guess;

	line = m_iLine_guess;

	int i = 1;

	while ( std::getline( file, line ) )
	{
		if ( i == m_iLine_guess )
		{
			char szLine[256];
			strcpy( szLine, line.c_str() );
			szLine[line.length()] = 0;

			m_bTitle_guess = ( i == 1 );
			strcpy( m_szLine_guess, szLine );
		}

		i++;
	}
}

void CNoteDocument::paint()
{
	if ( strlen( m_szLine_guess ) > 1 )
	{
		int wide, tall;

		if ( m_szLine_guess[0] == '*' )
			m_pLineLabel_guess->setFgColor( 255, 255, 0, 0 );
		else if ( m_bTitle_guess )
			m_pLineLabel_guess->setFgColor( 255, 255, 0, 0 );
		else
			m_pLineLabel_guess->setFgColor( 200, 200, 200, 0 );

		m_pFont_guess->getTextSize( m_szLine_guess, wide, tall );
		drawSetTextPos( ScreenWidth / 2 - wide / 2, ScreenHeight - tall * 2 );

		m_pLineLabel_guess->setSize( ScreenWidth, 64 );
		m_pLineLabel_guess->setText( m_szLine_guess );
		m_pLineLabel_guess->setPos( 0, ScreenHeight - tall * 3 );
		m_pLineLabel_guess->setContentAlignment( Label::a_center );
	}

	Panel::paint();
}
