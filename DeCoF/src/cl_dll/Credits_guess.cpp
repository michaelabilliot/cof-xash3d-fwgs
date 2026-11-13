// ======================================
// Cry of Fear credits, statistics and unlocks panel
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include <fstream>
#include <cmath>
#include <string>
#include <vector>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "Credits_guess.h"
#include "CoopStats_guess.h"
#include <stdarg.h>

extern int iMenuMode_guess;
extern int g_iHeadShieldType_guess;
extern cl_entity_t g_HeadShieldEnt;
extern int g_HeadShieldState;

#define SHIELD_ON	0
extern cvar_t *cl_fastcredits;
extern int iUnknown101902FC_guess;	// StudioModelRenderer.cpp

// the custom costume textures are loaded again when these are cleared (studio renderer)
extern bool g_bHoodieLoaded_guess;
extern bool g_bJeansLoaded_guess;
extern bool g_bFaceLoaded_guess;

void IrrKlangPlayMP3_guess( const char *pszFile );
void IrrKlangSetLoop_guess( bool bLoop );
void IrrKlangStop_guess( float flFadeTime );

bool g_bUnlockShown_guess;			// one unlock is on the screen, cleared by a key press (input.cpp)
int g_iUnlockClothes_guess;			// clothes drawn on the unfolded clothes model of the unlock
int g_iCreditsLetterboxHeight_guess;
int g_iCoopCredits_guess;			// Credits message, the co-op statistics follow
int iUnknown101B27C8_guess;			// pitch of the unlock model

CCredits::CCredits() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int i;
	int y;

	setBgColor( 255, 255, 255, 0 );

	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hCreditsScheme = pSchemes->getSchemeHandle( "credits" );
	SchemeHandle_t hCreditsTitleScheme = pSchemes->getSchemeHandle( "creditstitle" );
	m_pCreditsFont_guess = pSchemes->getFont( hCreditsScheme );
	m_pCreditsTitleFont_guess = pSchemes->getFont( hCreditsTitleScheme );

	m_bScrollUp_guess = true;

	for ( i = 1; i <= MAX_CREDIT_LINES_guess; i++ )
	{
		y = ScreenHeight;
		m_pCreditLabel_guess[i] = new Label( " ", 0, y + 64 + i * 32, ScreenWidth, 32 );
		m_pCreditLabel_guess[i]->setParent( this );
		m_pCreditLabel_guess[i]->setFont( m_pCreditsFont_guess );
		m_pCreditLabel_guess[i]->setFgColor( 0, 0, 0, 255 );
		m_pCreditLabel_guess[i]->setBgColor( 0, 0, 0, 255 );
		m_pCreditLabel_guess[i]->setText( " " );
		m_pCreditLabel_guess[i]->setContentAlignment( Label::a_center );
	}

	y = ScreenHeight / 2;

	for ( i = 1; i <= 14; i++ )
	{
		m_pStatLabel_guess[i] = new Label( " ", 0, y - 256 + i * 32, ScreenWidth, 32 );
		m_pStatLabel_guess[i]->setParent( this );
		m_pStatLabel_guess[i]->setFont( m_pCreditsFont_guess );
		m_pStatLabel_guess[i]->setFgColor( 0, 0, 0, 255 );
		m_pStatLabel_guess[i]->setBgColor( 0, 0, 0, 255 );
		m_pStatLabel_guess[i]->setText( " " );
		m_pStatLabel_guess[i]->setContentAlignment( Label::a_center );
		m_pStatLabel_guess[i]->setVisible( false );
	}

	m_pGradeLabel_guess = new Label( " ", 0, y + 256, ScreenWidth, 42 );
	m_pGradeLabel_guess->setParent( this );
	m_pGradeLabel_guess->setFont( pSchemes->getFont( pSchemes->getSchemeHandle( "chaptertitle" ) ) );
	m_pGradeLabel_guess->setFgColor( 0, 0, 0, 255 );
	m_pGradeLabel_guess->setBgColor( 0, 0, 0, 255 );
	m_pGradeLabel_guess->setText( " " );
	m_pGradeLabel_guess->setContentAlignment( Label::a_center );
	m_pGradeLabel_guess->setVisible( false );

	m_pGradeShadowLabel_guess = new Label( " ", 0, y + 256, ScreenWidth, 42 );
	m_pGradeShadowLabel_guess->setParent( this );
	m_pGradeShadowLabel_guess->setFont( pSchemes->getFont( pSchemes->getSchemeHandle( "chaptertitle" ) ) );
	m_pGradeShadowLabel_guess->setFgColor( 0, 0, 0, 255 );
	m_pGradeShadowLabel_guess->setBgColor( 0, 0, 0, 255 );
	m_pGradeShadowLabel_guess->setText( " " );
	m_pGradeShadowLabel_guess->setContentAlignment( Label::a_center );
	m_pGradeShadowLabel_guess->setVisible( false );

	m_pStatsTitleLabel_guess = new Label( "Statistics and grade", 0, y - 266, ScreenWidth, 42 );
	m_pStatsTitleLabel_guess->setParent( this );
	m_pStatsTitleLabel_guess->setFont( m_pCreditsTitleFont_guess );
	m_pStatsTitleLabel_guess->setFgColor( 0, 0, 0, 255 );
	m_pStatsTitleLabel_guess->setBgColor( 0, 0, 0, 255 );
	m_pStatsTitleLabel_guess->setText( "Statistics and grade" );
	m_pStatsTitleLabel_guess->setContentAlignment( Label::a_center );
	m_pStatsTitleLabel_guess->setVisible( false );

	m_pUnlockLabel_guess = new Label( "Unlocks", 0, y - 266, ScreenWidth, 42 );
	m_pUnlockLabel_guess->setParent( this );
	m_pUnlockLabel_guess->setFont( m_pCreditsTitleFont_guess );
	m_pUnlockLabel_guess->setFgColor( 0, 0, 0, 255 );
	m_pUnlockLabel_guess->setBgColor( 0, 0, 0, 255 );
	m_pUnlockLabel_guess->setText( "Unlocks" );
	m_pUnlockLabel_guess->setContentAlignment( Label::a_center );
	m_pUnlockLabel_guess->setVisible( false );

	g_iCreditsLetterboxHeight_guess = ScreenHeight / 480.0f * 25.0f + 0.5f;

	m_pLetterboxTop_guess = new Panel( 0, 0, ScreenWidth, g_iCreditsLetterboxHeight_guess );
	m_pLetterboxTop_guess->setParent( this );
	m_pLetterboxTop_guess->setSize( ScreenWidth, g_iCreditsLetterboxHeight_guess );
	m_pLetterboxTop_guess->setBgColor( 0, 0, 0, 0 );
	m_pLetterboxTop_guess->setVisible( true );

	m_pLetterboxBottom_guess = new Panel( 0, ScreenHeight - g_iCreditsLetterboxHeight_guess, ScreenWidth, g_iCreditsLetterboxHeight_guess );
	m_pLetterboxBottom_guess->setParent( this );
	m_pLetterboxBottom_guess->setSize( ScreenWidth, g_iCreditsLetterboxHeight_guess );
	m_pLetterboxBottom_guess->setBgColor( 0, 0, 0, 0 );
	m_pLetterboxBottom_guess->setVisible( true );

	Initialize();
}

int CCredits::CountLines_guess()
{
	std::ifstream file( m_szFile_guess );
	std::string line;
	std::vector<std::string> lines;

	while ( std::getline( file, line ) )
		lines.push_back( line );

	return lines.size();
}

void CCredits::Initialize()
{
	g_iUnlockClothes_guess = 0;
	setVisible( false );
	m_bShowStats_guess = false;
	m_bUnlocks_guess = false;
	m_iUnlocks_guess = 0;
}

int MsgCredits_guess( const char *pszName, int iSize, void *pbuf )
{
	char szPath[256];
	char szFile[256];
	char szEnding[256];

	BEGIN_READ( pbuf, iSize );

	if ( !gViewPort->m_pCredits_guess )
		return 1;

	gViewPort->m_pCredits_guess->m_szUnlockText_guess[0] = '\0';
	g_iUnlockClothes_guess = 0;
	gViewPort->m_pCredits_guess->m_bUnlocks_guess = false;
	gViewPort->m_pCredits_guess->m_iUnlocks_guess = 0;
	gViewPort->m_pCredits_guess->m_pStatsTitleLabel_guess->setVisible( false );
	gViewPort->m_pCredits_guess->m_pGradeLabel_guess->setVisible( false );
	gViewPort->m_pCredits_guess->m_pGradeShadowLabel_guess->setVisible( false );
	gViewPort->m_pCredits_guess->setVisible( true );

	int iTextOnly = READ_BYTE();
	bool bConclusion = false;
	int iLanguage;

	if ( iTextOnly )
	{
		strcpy( szFile, READ_STRING() );

		if ( strstr( szFile, "coconc.txt" ) || strstr( szFile, "conclusion1.txt" ) || strstr( szFile, "conclusion2.txt" ) ||
			strstr( szFile, "conclusion3.txt" ) || strstr( szFile, "conclusion4.txt" ) || strstr( szFile, "docconc.txt" ) )
		{
			bConclusion = true;
			strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, szFile );
		}
		else if ( !strstr( szFile, gEngfuncs.pfnGetGameDirectory() ) )
		{
			strcpy( szPath, gEngfuncs.pfnGetGameDirectory() );
			strcat( szPath, szFile );
			strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, szPath );
		}
		else
		{
			strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, szFile );
		}
	}
	else
	{
		strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, READ_STRING() );
	}

	iLanguage = gEngfuncs.pfnGetCvarFloat( "cof_subtitlelanguage" );

	if ( !iTextOnly || bConclusion == true )
	{
		// the conclusion texts are translated (coconc.txt gets the docconc.txt of the language)
		switch ( iLanguage )
		{
		case 2:
			if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "coconc.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/dutch/docconc.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion1.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/dutch/conclusion1.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion2.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/dutch/conclusion2.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion3.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/dutch/conclusion3.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion4.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/dutch/conclusion4.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "docconc.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/dutch/docconc.txt" );
			}
			break;
		case 3:
			if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "coconc.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/french/docconc.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion1.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/french/conclusion1.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion2.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/french/conclusion2.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion3.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/french/conclusion3.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion4.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/french/conclusion4.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "docconc.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/french/docconc.txt" );
			}
			break;
		case 4:
			if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "coconc.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/german/docconc.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion1.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/german/conclusion1.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion2.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/german/conclusion2.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion3.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/german/conclusion3.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion4.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/german/conclusion4.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "docconc.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/german/docconc.txt" );
			}
			break;
		case 5:
			if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "coconc.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/norwegian/docconc.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion1.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/norwegian/conclusion1.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion2.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/norwegian/conclusion2.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion3.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/norwegian/conclusion3.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion4.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/norwegian/conclusion4.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "docconc.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/norwegian/docconc.txt" );
			}
			break;
		case 6:
			if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "coconc.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/spanish/docconc.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion1.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/spanish/conclusion1.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion2.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/spanish/conclusion2.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion3.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/spanish/conclusion3.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion4.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/spanish/conclusion4.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "docconc.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/spanish/docconc.txt" );
			}
			break;
		case 7:
			if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "coconc.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/swedish/docconc.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion1.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/swedish/conclusion1.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion2.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/swedish/conclusion2.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion3.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/swedish/conclusion3.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "conclusion4.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/swedish/conclusion4.txt" );
			}
			else if ( strstr( gViewPort->m_pCredits_guess->m_szFile_guess, "docconc.txt" ) )
			{
				strcpy( gViewPort->m_pCredits_guess->m_szFile_guess, gEngfuncs.pfnGetGameDirectory() );
				strcat( gViewPort->m_pCredits_guess->m_szFile_guess, "/txtfiles/languages/swedish/docconc.txt" );
			}
			break;
		}
	}

	gViewPort->m_pCredits_guess->m_iNumLines_guess = gViewPort->m_pCredits_guess->CountLines_guess();
	gViewPort->m_pCredits_guess->m_bStarted_guess = true;

	int iStartY = 0;
	if ( !gViewPort->m_pCredits_guess->m_bScrollUp_guess )
		iStartY = ScreenHeight;

	gViewPort->m_pCredits_guess->m_iBrightness_guess = 0;
	gViewPort->m_pCredits_guess->m_bFadeIn_guess = false;
	gViewPort->m_pCredits_guess->m_bFadeDone_guess = false;
	gViewPort->m_pCredits_guess->m_bStatsTimer_guess = false;
	gViewPort->m_pCredits_guess->m_flStatsTime_guess = 0;

	int i;

	for ( i = 1; i <= gViewPort->m_pCredits_guess->m_iNumLines_guess; i++ )
	{
		gViewPort->m_pCredits_guess->SetLine_guess( i, i );

		int iOffset = i * 32;
		if ( gViewPort->m_pCredits_guess->m_bScrollUp_guess )
			iOffset = -iOffset;

		gViewPort->m_pCredits_guess->m_pCreditLabel_guess[i]->setPos( 0, iStartY + iOffset );
		gViewPort->m_pCredits_guess->m_pCreditLabel_guess[i]->setVisible( true );
	}

	gViewPort->m_pCredits_guess->m_iShotsFired_guess = READ_SHORT();
	gViewPort->m_pCredits_guess->m_iShotsHit_guess = READ_SHORT();
	gViewPort->m_pCredits_guess->m_iSyringesUsed_guess = READ_SHORT();
	gViewPort->m_pCredits_guess->m_iShotsMissed_guess = READ_SHORT();
	gViewPort->m_pCredits_guess->m_flAccuracy_guess = READ_COORD();
	gViewPort->m_pCredits_guess->m_iTimesSaved_guess = READ_SHORT();
	gViewPort->m_pCredits_guess->m_iWeaponsFound_guess = READ_SHORT();
	gViewPort->m_pCredits_guess->m_iDamageTaken_guess = READ_SHORT();
	gViewPort->m_pCredits_guess->m_flDistanceDecimal_guess = READ_SHORT();
	gViewPort->m_pCredits_guess->m_flDistance_guess = READ_SHORT();
	gViewPort->m_pCredits_guess->m_iSeconds_guess = READ_SHORT();
	gViewPort->m_pCredits_guess->m_iMinutes_guess = READ_SHORT();
	gViewPort->m_pCredits_guess->m_iHours_guess = READ_SHORT();
	gViewPort->m_pCredits_guess->m_iMeleeKills_guess = READ_SHORT();
	gViewPort->m_pCredits_guess->m_iFirearmKills_guess = READ_SHORT();
	READ_BYTE();
	gViewPort->m_pCredits_guess->m_iScrollSpeed_guess = READ_BYTE();
	int iHide = READ_BYTE();
	gViewPort->m_pCredits_guess->m_iDifficulty_guess = READ_BYTE();
	gViewPort->m_pCredits_guess->m_iUnlocks_guess = READ_LONG();
	int iEnding = READ_BYTE();
	int iGrade = READ_BYTE();
	int iCoop = READ_BYTE();
	int iStatsTimer = READ_BYTE();
	g_iCoopCredits_guess = READ_BYTE();
	int iEnding6 = READ_BYTE();
	int iEnding7 = READ_BYTE();
	READ_BYTE();
	READ_BYTE();
	int iShowGrade = READ_BYTE();

	if ( gViewPort->m_pCredits_guess->m_iDifficulty_guess < 1 || gViewPort->m_pCredits_guess->m_iDifficulty_guess > 4 )
		gViewPort->m_pCredits_guess->m_iDifficulty_guess = 1;

	if ( iHide )
		gViewPort->m_pCredits_guess->setVisible( false );

	if ( iTextOnly )
	{
		gViewPort->m_pCredits_guess->m_bTextOnly_guess = true;
		gViewPort->m_pCredits_guess->m_bScrollUp_guess = false;

		for ( i = 1; i <= MAX_CREDIT_LINES_guess; i++ )
			gViewPort->m_pCredits_guess->m_pCreditLabel_guess[i]->setPos( 0, ScreenHeight + i * 32 );
	}
	else
	{
		gViewPort->m_pCredits_guess->m_bScrollUp_guess = true;
		gViewPort->m_pCredits_guess->m_bTextOnly_guess = false;
	}

	gViewPort->m_pCredits_guess->m_bShowStats_guess = false;
	gViewPort->m_pCredits_guess->m_flStartTime_guess = gEngfuncs.GetClientTime();
	gViewPort->m_pCredits_guess->m_flLastScrollTime = gViewPort->m_pCredits_guess->m_flStartTime_guess;
	gViewPort->m_pCredits_guess->m_flScrollPixels = 0;
	gViewPort->m_pCredits_guess->m_flScrollFadeTicks = 0;

	gViewPort->m_pCredits_guess->m_pStatLabel_guess[1]->setText( UTIL_VarArgs_guess( "Shots fired: %i", gViewPort->m_pCredits_guess->m_iShotsFired_guess ) );
	gViewPort->m_pCredits_guess->m_pStatLabel_guess[2]->setText( UTIL_VarArgs_guess( "Shots hit: %i", gViewPort->m_pCredits_guess->m_iShotsHit_guess ) );
	gViewPort->m_pCredits_guess->m_pStatLabel_guess[3]->setText( UTIL_VarArgs_guess( "Shots missed: %i", gViewPort->m_pCredits_guess->m_iShotsMissed_guess ) );
	gViewPort->m_pCredits_guess->m_pStatLabel_guess[4]->setText( UTIL_VarArgs_guess( "Overall accuracy: %i percent", (int)gViewPort->m_pCredits_guess->m_flAccuracy_guess ) );

	if ( iCoop )
	{
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[5]->setText( UTIL_VarArgs_guess( "Overall damage taken: %i", gViewPort->m_pCredits_guess->m_iDamageTaken_guess ) );
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[6]->setText( UTIL_VarArgs_guess( "Distance travelled on foot: %i.%i km", (int)gViewPort->m_pCredits_guess->m_flDistance_guess, (int)gViewPort->m_pCredits_guess->m_flDistanceDecimal_guess ) );
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[7]->setText( UTIL_VarArgs_guess( "Time taken: %i:%i:%i", gViewPort->m_pCredits_guess->m_iHours_guess, gViewPort->m_pCredits_guess->m_iMinutes_guess, gViewPort->m_pCredits_guess->m_iSeconds_guess ) );
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[8]->setText( UTIL_VarArgs_guess( "Enemies killed by melee: %i", gViewPort->m_pCredits_guess->m_iMeleeKills_guess ) );
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[9]->setText( UTIL_VarArgs_guess( "Enemies killed by firearms: %i", gViewPort->m_pCredits_guess->m_iFirearmKills_guess ) );
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[10]->setText( "" );
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[11]->setText( "" );
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[12]->setText( "" );
	}
	else
	{
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[5]->setText( UTIL_VarArgs_guess( "Syringes used: %i", gViewPort->m_pCredits_guess->m_iSyringesUsed_guess ) );
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[6]->setText( UTIL_VarArgs_guess( "Times saved: %i", gViewPort->m_pCredits_guess->m_iTimesSaved_guess ) );
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[7]->setText( UTIL_VarArgs_guess( "Weapons found: %i", gViewPort->m_pCredits_guess->m_iWeaponsFound_guess ) );
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[8]->setText( UTIL_VarArgs_guess( "Overall damage taken: %i", gViewPort->m_pCredits_guess->m_iDamageTaken_guess ) );
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[9]->setText( UTIL_VarArgs_guess( "Distance travelled on foot: %i.%i km", (int)gViewPort->m_pCredits_guess->m_flDistance_guess, (int)gViewPort->m_pCredits_guess->m_flDistanceDecimal_guess ) );
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[10]->setText( UTIL_VarArgs_guess( "Time taken: %i:%i:%i", gViewPort->m_pCredits_guess->m_iHours_guess, gViewPort->m_pCredits_guess->m_iMinutes_guess, gViewPort->m_pCredits_guess->m_iSeconds_guess ) );
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[11]->setText( UTIL_VarArgs_guess( "Enemies killed by melee: %i", gViewPort->m_pCredits_guess->m_iMeleeKills_guess ) );
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[12]->setText( UTIL_VarArgs_guess( "Enemies killed by firearms: %i", gViewPort->m_pCredits_guess->m_iFirearmKills_guess ) );
	}

	switch ( gViewPort->m_pCredits_guess->m_iDifficulty_guess )
	{
	case 1:
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[13]->setText( UTIL_VarArgs_guess( "Difficulty chosen: Easy" ) );
		break;
	case 2:
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[13]->setText( UTIL_VarArgs_guess( "Difficulty chosen: Normal" ) );
		break;
	case 3:
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[13]->setText( UTIL_VarArgs_guess( "Difficulty chosen: Hard" ) );
		break;
	case 4:
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[13]->setText( UTIL_VarArgs_guess( "Difficulty chosen: Nightmare" ) );
		break;
	default:
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[13]->setText( UTIL_VarArgs_guess( "Difficulty chosen: Easy" ) );
		break;
	}

	if ( iEnding6 && iCoop )
		iEnding = 6;

	if ( iEnding7 && g_iCoopCredits_guess )
	{
		strcpy( szEnding, "Ending seen: " );
		strcat( szEnding, "7" );
	}
	else
	{
		strcpy( szEnding, "Ending seen: " );

		switch ( iEnding )
		{
		case 1:
			strcat( szEnding, "1" );
			break;
		case 2:
			strcat( szEnding, "2" );
			break;
		case 3:
			strcat( szEnding, "3" );
			break;
		case 4:
			strcat( szEnding, "4" );
			break;
		case 5:
			strcat( szEnding, "5" );
			break;
		case 6:
			strcat( szEnding, "6" );
			break;
		case 7:
			strcat( szEnding, "7" );
			break;
		}
	}

	gViewPort->m_pCredits_guess->m_pStatLabel_guess[14]->setText( szEnding );

	for ( i = 1; i <= 14; i++ )
	{
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[i]->setContentAlignment( Label::a_center );
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[i]->setVisible( false );
	}

	switch ( iGrade )
	{
	case 1:
		gViewPort->m_pCredits_guess->m_pGradeLabel_guess->setText( "S" );
		gViewPort->m_pCredits_guess->m_pGradeShadowLabel_guess->setText( "S" );
		break;
	case 2:
		gViewPort->m_pCredits_guess->m_pGradeLabel_guess->setText( "A" );
		gViewPort->m_pCredits_guess->m_pGradeShadowLabel_guess->setText( "A" );
		break;
	case 3:
		gViewPort->m_pCredits_guess->m_pGradeLabel_guess->setText( "B" );
		gViewPort->m_pCredits_guess->m_pGradeShadowLabel_guess->setText( "B" );
		break;
	case 4:
		gViewPort->m_pCredits_guess->m_pGradeLabel_guess->setText( "C" );
		gViewPort->m_pCredits_guess->m_pGradeShadowLabel_guess->setText( "C" );
		break;
	case 5:
		gViewPort->m_pCredits_guess->m_pGradeLabel_guess->setText( "D" );
		gViewPort->m_pCredits_guess->m_pGradeShadowLabel_guess->setText( "D" );
		break;
	case 6:
		gViewPort->m_pCredits_guess->m_pGradeLabel_guess->setText( "E" );
		gViewPort->m_pCredits_guess->m_pGradeShadowLabel_guess->setText( "E" );
		break;
	default:
		gViewPort->m_pCredits_guess->m_pGradeLabel_guess->setText( "F" );
		gViewPort->m_pCredits_guess->m_pGradeShadowLabel_guess->setText( "F" );
		break;
	}

	if ( !iShowGrade )
	{
		gViewPort->m_pCredits_guess->m_pGradeLabel_guess->setText( " " );
		gViewPort->m_pCredits_guess->m_pGradeShadowLabel_guess->setText( " " );
		gViewPort->m_pCredits_guess->m_pStatLabel_guess[14]->setText( " " );
	}

	if ( iStatsTimer )
	{
		gViewPort->m_pCredits_guess->m_bStatsTimer_guess = true;
		gViewPort->m_pCredits_guess->m_flStatsTime_guess = gEngfuncs.GetClientTime() + 0.2;
	}

	return 1;
}

void CCredits::SetLine_guess( int iUnused, int iLine )
{
	std::ifstream file( m_szFile_guess );
	std::string line;
	std::string line2;
	line = iLine;
	int i = 1;
	char szLine[256];

	while ( std::getline( file, line ) )
	{
		if ( i == iLine )
		{
			strcpy( szLine, line.c_str() );
			szLine[line.length()] = 0;

			char *pszText = szLine;

			if ( szLine[0] == '#' )
			{
				pszText = &szLine[1];
				m_pCreditLabel_guess[i]->setFont( m_pCreditsTitleFont_guess );
			}
			else
			{
				m_pCreditLabel_guess[i]->setFont( m_pCreditsFont_guess );
			}

			m_pCreditLabel_guess[i]->setText( pszText );
			m_pCreditLabel_guess[i]->setContentAlignment( Label::a_center );
		}

		i++;
	}
}

void CCredits::ShowUnlock_guess( int iUnlock, const char *pszText, int iDistance, char *pszModel, int iBody )
{
	int i;

	iUnknown101902FC_guess = iDistance;
	m_iUnlocks_guess &= ~iUnlock;
	m_bContinue_guess = false;

	for ( i = 1; i <= 14; i++ )
	{
		if ( m_pStatLabel_guess[i]->isVisible() )
			m_pStatLabel_guess[i]->setVisible( false );
	}

	for ( i = 1; i <= m_iNumLines_guess; i++ )
	{
		if ( m_pCreditLabel_guess[i]->isVisible() )
			m_pCreditLabel_guess[i]->setVisible( false );
	}

	if ( m_pGradeLabel_guess->isVisible() )
		m_pGradeLabel_guess->setVisible( false );
	if ( m_pGradeShadowLabel_guess->isVisible() )
		m_pGradeShadowLabel_guess->setVisible( false );
	if ( m_pStatsTitleLabel_guess->isVisible() )
		m_pStatsTitleLabel_guess->setVisible( false );

	strcpy( m_szUnlockText_guess, pszText );

	if ( g_iHeadShieldType_guess != 6 )
		g_iHeadShieldType_guess = 6;

	iUnknown101902FC_guess = 50;

	if ( !strcmp( pszModel, "models/items/generic_note.mdl" ) || !strcmp( pszModel, "models/weapons/axe/w_axe.mdl" ) ||
		!strcmp( pszModel, "models/weapons/famas/w_famas.mdl" ) || !strcmp( pszModel, "models/items/package.mdl" ) )
	{
		iUnknown101B27C8_guess = 90;
	}
	else if ( !strcmp( pszModel, "models/items/cassette_tape.mdl" ) )
	{
		iUnknown101902FC_guess = 20;
		iUnknown101B27C8_guess = 90;
	}
	else
	{
		if ( !strcmp( pszModel, "models/doctor_unlock.mdl" ) )
			iUnknown101902FC_guess = 80;

		iUnknown101B27C8_guess = 0;
	}

	gHUD.m_pszHeadShieldSound_guess = pszModel;
	gHUD.m_iHeadShieldSoundFlags_guess = iBody;
	g_HeadShieldState = SHIELD_ON;
}

char *UTIL_VarArgs_guess( const char *format, ... )
{
	va_list argptr;
	static char string[1024];

	va_start( argptr, format );
	vsprintf( string, format, argptr );
	va_end( argptr );

	return string;
}

void VGUI_CreditsInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "Credits", MsgCredits_guess );
}

void CCredits::solve()
{
	static bool s_bLetterboxFrame = false;
	static bool s_bFadeFrame = true;
	int i;

	int iSpeed = g_iCreditsLetterboxHeight_guess / 15;
	if ( iSpeed <= 0 )
		iSpeed = 1;

	if ( gHUD.m_BlackBar_guess.m_iOn_guess )
	{
		s_bLetterboxFrame = !s_bLetterboxFrame;
		if ( s_bLetterboxFrame )
		{
			int x, y, x2, y2;

			m_pLetterboxTop_guess->getPos( x, y );
			m_pLetterboxBottom_guess->getPos( x2, y2 );

			if ( y < 0 )
				y += iSpeed;
			if ( y > 0 )
				y = 0;
			if ( y2 > ScreenHeight - g_iCreditsLetterboxHeight_guess )
				y2 -= iSpeed;
			if ( y2 < ScreenHeight - g_iCreditsLetterboxHeight_guess )
				y2 = ScreenHeight - g_iCreditsLetterboxHeight_guess;

			m_pLetterboxTop_guess->setPos( x, y );
			m_pLetterboxBottom_guess->setPos( x2, y2 );
		}
	}
	else
	{
		s_bLetterboxFrame = !s_bLetterboxFrame;
		if ( s_bLetterboxFrame )
		{
			int x, y, x2, y2;

			m_pLetterboxTop_guess->getPos( x, y );
			m_pLetterboxBottom_guess->getPos( x2, y2 );

			if ( y > -g_iCreditsLetterboxHeight_guess )
				y -= iSpeed;
			if ( y < -g_iCreditsLetterboxHeight_guess )
				y = -g_iCreditsLetterboxHeight_guess;
			if ( y2 < ScreenHeight )
				y2 += iSpeed;
			if ( y2 > ScreenHeight )
				y2 = ScreenHeight;

			m_pLetterboxTop_guess->setPos( x, y );
			m_pLetterboxBottom_guess->setPos( x2, y2 );
		}
	}

	if ( m_bStatsTimer_guess == true && !m_bTextOnly_guess )
	{
		float flTime = gEngfuncs.GetClientTime();

		if ( flTime > m_flStatsTime_guess && !m_bShowStats_guess )
		{
			IrrKlangSetLoop_guess( true );
			IrrKlangPlayMP3_guess( "stats.mp3" );
			m_bShowStats_guess = true;
			m_pLetterboxBottom_guess->setVisible( false );
			m_pLetterboxTop_guess->setVisible( false );

			if ( g_iCoopCredits_guess && gViewPort->m_pCoopStats_guess )
			{
				setVisible( false );
				gViewPort->m_pCoopStats_guess->setVisible( true );
			}
		}
	}

	s_bFadeFrame = !s_bFadeFrame;
	if ( !s_bFadeFrame )
	{
		Panel::solve();
		return;
	}

	if ( strlen( m_szUnlockText_guess ) > 2 )
	{
		int r, g, b, a;

		m_pUnlockLabel_guess->getFgColor( r, g, b, a );
		if ( a > 0 )
			a -= 15;
		if ( a < 0 )
			a = 0;

		m_pUnlockLabel_guess->setText( m_szUnlockText_guess );
		m_pUnlockLabel_guess->setVisible( true );
		m_pUnlockLabel_guess->setPos( 0, ScreenWidth / 2 );
		m_pUnlockLabel_guess->setFgColor( 255, 255, 255, a );
	}

	if ( m_bUnlocks_guess )
	{
		// show the unlocks one by one
		if ( m_iUnlocks_guess & 0x40 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x40, "Unlocked: FAMAS with infinite ammo", 50, "models/weapons/famas/w_famas.mdl", 0 );
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x80 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x80, "Unlocked: David Leatherhoff suit", 50, "models/costumes/unfolded_clothes.mdl", 1 );
				g_iUnlockClothes_guess = 1;
				g_bHoodieLoaded_guess = false;
				g_bJeansLoaded_guess = false;
				g_bFaceLoaded_guess = false;
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x100 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x100, "Unlocked: ModDB hoodie", 50, "models/costumes/unfolded_clothes.mdl", 0 );
				g_iUnlockClothes_guess = 2;
				g_bHoodieLoaded_guess = false;
				g_bJeansLoaded_guess = false;
				g_bFaceLoaded_guess = false;
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x200 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x200, "Unlocked: Hello Kitty suit", 50, "models/costumes/unfolded_clothes.mdl", 1 );
				g_iUnlockClothes_guess = 3;
				g_bHoodieLoaded_guess = false;
				g_bJeansLoaded_guess = false;
				g_bFaceLoaded_guess = false;
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x400 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x400, "Unlocked: Afraid of Monsters suit", 50, "models/costumes/unfolded_clothes.mdl", 1 );
				g_iUnlockClothes_guess = 4;
				g_bHoodieLoaded_guess = false;
				g_bJeansLoaded_guess = false;
				g_bFaceLoaded_guess = false;
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x800 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x800, "Unlocked: Camouflage hoodie", 50, "models/costumes/unfolded_clothes.mdl", 0 );
				g_iUnlockClothes_guess = 5;
				g_bHoodieLoaded_guess = false;
				g_bJeansLoaded_guess = false;
				g_bFaceLoaded_guess = false;
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x1000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x1000, "Unlocked: Half Life Creations hoodie", 50, "models/costumes/unfolded_clothes.mdl", 0 );
				g_iUnlockClothes_guess = 6;
				g_bHoodieLoaded_guess = false;
				g_bJeansLoaded_guess = false;
				g_bFaceLoaded_guess = false;
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x2000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x2000, "Unlocked: Black Metal suit", 50, "models/costumes/unfolded_clothes.mdl", 1 );
				g_iUnlockClothes_guess = 7;
				g_bHoodieLoaded_guess = false;
				g_bJeansLoaded_guess = false;
				g_bFaceLoaded_guess = false;
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x4000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x4000, "Unlocked: Team Psykskallar hoodie", 50, "models/costumes/unfolded_clothes.mdl", 0 );
				g_iUnlockClothes_guess = 8;
				g_bHoodieLoaded_guess = false;
				g_bJeansLoaded_guess = false;
				g_bFaceLoaded_guess = false;
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x8000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x8000, "Unlocked: Fuck Anime suit", 50, "models/costumes/unfolded_clothes.mdl", 1 );
				g_iUnlockClothes_guess = 9;
				g_bHoodieLoaded_guess = false;
				g_bJeansLoaded_guess = false;
				g_bFaceLoaded_guess = false;
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x10000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x10000, "Unlocked: Sick Simon suit", 50, "models/costumes/unfolded_clothes.mdl", 1 );
				g_iUnlockClothes_guess = 10;
				g_bHoodieLoaded_guess = false;
				g_bJeansLoaded_guess = false;
				g_bFaceLoaded_guess = false;
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x20000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x20000, "Unlocked: AoM Twitcher suit", 50, "models/costumes/unfolded_clothes.mdl", 1 );
				g_iUnlockClothes_guess = 11;
				g_bHoodieLoaded_guess = false;
				g_bJeansLoaded_guess = false;
				g_bFaceLoaded_guess = false;
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x40000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x40000, "Unlocked: Custom hoodies", 50, "models/costumes/unfolded_clothes.mdl", 0 );
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x80000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x80000, "Unlocked: AoMDC axe", 50, "models/weapons/axe/w_axe.mdl", 0 );
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x100000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x100000, "Unlocked: Camera of Death", 50, "models/weapons/camera/w_camera.mdl", 0 );
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x200000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x200000, "Unlocked: Simon\'s Book", 50, "models/weapons/booklaser/w_booklaser.mdl", 0 );
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x400000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x400000, "Unlocked: Developer commentaries", 50, "models/developer_commentary.mdl", 0 );
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x800000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x800000, "Unlocked: Secret package", 50, "models/items/package.mdl", 0 );
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x20 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x20, "Unlocked: Nightmare mode", 50, "models/items/cassette_tape.mdl", 0 );
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x1000000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x1000000, "Unlocked: Nightvision", 50, "models/items/w_nightvision.mdl", 0 );
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x2000000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x2000000, "Unlocked: Book page 1", 50, "models/items/generic_note.mdl", 0 );
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x4000000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x4000000, "Unlocked: Book page 2", 50, "models/items/generic_note.mdl", 0 );
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x8000000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x8000000, "Unlocked: Book page 3", 50, "models/items/generic_note.mdl", 0 );
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x10000000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x10000000, "Unlocked: Book page 4", 50, "models/items/generic_note.mdl", 0 );
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x20000000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x20000000, "Unlocked: Book page 5", 50, "models/items/generic_note.mdl", 0 );
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x40000000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x40000000, "Unlocked: Book page 6", 50, "models/items/generic_note.mdl", 0 );
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_iUnlocks_guess & 0x80000000 )
		{
			if ( !g_bUnlockShown_guess )
			{
				ShowUnlock_guess( 0x80000000, "Unlocked: Doctor mode", 50, "models/doctor_unlock.mdl", 0 );
				g_bUnlockShown_guess = true;
			}
		}
		else if ( m_bContinue_guess )
		{
			m_bContinue_guess = false;
			setVisible( false );
			gViewPort->UpdateCursorState();
			g_iUnlockClothes_guess = 0;

			if ( iMenuMode_guess )
			{
				gEngfuncs.pfnClientCmd( "map c_game_menu1" );
				gViewPort->UpdateCursorState();
			}
			else
			{
				gEngfuncs.pfnServerCmd( "closegame" );
				gViewPort->UpdateCursorState();
			}
		}
	}
	else
	{
		if ( m_pUnlockLabel_guess->isVisible() )
			m_pUnlockLabel_guess->setVisible( false );

		if ( !m_bShowStats_guess || m_bTextOnly_guess || g_iCoopCredits_guess )
		{
			// Position and opacity use the client clock so repaint rate has no effect.
			const float now = gEngfuncs.GetClientTime();
			float elapsed = now - m_flLastScrollTime;
			m_flLastScrollTime = now;
			if ( !std::isfinite(elapsed) || elapsed < 0.0f ) elapsed = 0.0f;
			if ( elapsed > 0.1f ) elapsed = 0.1f;
			const int speed = m_iScrollSpeed_guess > 0 ? m_iScrollSpeed_guess : 1;
			const float pixelsPerSecond = cl_fastcredits->value ? 1200.0f :
				ScreenHeight / (5.0f + 2.0f * speed);
			m_flScrollPixels += elapsed * pixelsPerSecond;
			const int iStep = static_cast<int>(m_flScrollPixels);
			m_flScrollPixels -= iStep;
			m_flScrollFadeTicks += elapsed * 30.0f;
			const int fadeTicks = static_cast<int>(m_flScrollFadeTicks);
			m_flScrollFadeTicks -= fadeTicks;
			if ( m_bFadeIn_guess )
			{
				if ( m_iBrightness_guess < 255 )
					m_iBrightness_guess += 15 * fadeTicks;
			}
			else if ( m_iBrightness_guess > 0 )
			{
				m_iBrightness_guess -= 15 * fadeTicks;
			}

			if ( m_iBrightness_guess > 255 )
				m_iBrightness_guess = 255;
			else if ( m_iBrightness_guess < 0 )
				m_iBrightness_guess = 0;

			if ( m_bTextOnly_guess )
				m_iBrightness_guess = 255;

			for ( i = 1; i <= m_iNumLines_guess; i++ )
			{
				int x, y;
				int r, g, b, a;

				m_pCreditLabel_guess[i]->getPos( x, y );

				if ( m_bScrollUp_guess )
					y += iStep;
				else
					y -= iStep;

				m_pCreditLabel_guess[i]->getFgColor( r, g, b, a );

				bool bScrollUp = m_bScrollUp_guess;

				if ( bScrollUp )
				{
					if ( y > 0 && y < ScreenHeight - ScreenHeight * 0.25 )
						a -= 5 * fadeTicks;
					if ( y > ScreenHeight - ScreenHeight * 0.25 )
						a += 5 * fadeTicks;
				}
				else
				{
					if ( y > ScreenHeight - ScreenHeight * 0.25 )
						a -= 5 * fadeTicks;
					if ( y < ScreenHeight * 0.25 )
						a += 5 * fadeTicks;
				}

				if ( i == m_iNumLines_guess / 2 && y > ScreenHeight / 2 - 16 && !m_bFadeIn_guess && !m_bFadeDone_guess && !m_bTextOnly_guess )
					m_bFadeIn_guess = true;

				if ( a < 0 )
					a = 0;
				else if ( a > 255 )
					a = 255;

				if ( y > ScreenHeight )
					a = 255;

				if ( bScrollUp )
				{
					if ( i == m_iNumLines_guess && y > ScreenHeight / 2 - 16 && !m_bTextOnly_guess )
					{
						m_bFadeIn_guess = false;
						m_bFadeDone_guess = true;
						y = ScreenHeight / 2 - 16;

						if ( !m_bStatsTimer_guess )
						{
							m_bStatsTimer_guess = true;
							m_flStatsTime_guess = gEngfuncs.GetClientTime() + 3.0;
							IrrKlangStop_guess( 2.0 );
						}
					}
				}
				else if ( i == m_iNumLines_guess && y < ScreenHeight / 2 - 16 && !m_bTextOnly_guess )
				{
					m_bFadeIn_guess = false;
					m_bFadeDone_guess = true;
					y = ScreenHeight / 2 - 16;

					if ( !m_bStatsTimer_guess )
					{
						m_bStatsTimer_guess = true;
						m_flStatsTime_guess = gEngfuncs.GetClientTime() + 3.0;
					}
				}

				m_pCreditLabel_guess[i]->setFgColor( m_iBrightness_guess, m_iBrightness_guess, m_iBrightness_guess, a );
				m_pCreditLabel_guess[i]->setPos( x, y );
			}
		}
		else
		{
			// show the statistics
			int x, y;
			int r, g, b, a;

			if ( m_bContinue_guess )
			{
				m_bContinue_guess = false;
				m_bUnlocks_guess = true;
			}

			setBgColor( 255, 255, 255, 255 );

			for ( i = 1; i <= m_iNumLines_guess; i++ )
			{
				m_pCreditLabel_guess[i]->getFgColor( r, g, b, a );
				a += 15;
				if ( a > 255 )
					a = 255;
				m_pCreditLabel_guess[i]->setFgColor( m_iBrightness_guess, m_iBrightness_guess, m_iBrightness_guess, a );
			}

			m_bFadeIn_guess = true;

			if ( m_iBrightness_guess < 255 )
				m_iBrightness_guess += 15;

			if ( m_iBrightness_guess > 255 )
				m_iBrightness_guess = 255;
			else if ( m_iBrightness_guess < 0 )
				m_iBrightness_guess = 0;

			if ( !m_pGradeLabel_guess->isVisible() )
				m_pGradeLabel_guess->setVisible( true );
			m_pGradeLabel_guess->getFgColor( r, g, b, a );
			if ( a > 0 )
				a -= 5;
			if ( a < 0 )
				a = 0;
			m_pGradeLabel_guess->setFgColor( m_iBrightness_guess, m_iBrightness_guess, m_iBrightness_guess, a );

			if ( !m_pGradeShadowLabel_guess->isVisible() )
				m_pGradeShadowLabel_guess->setVisible( true );
			m_pGradeLabel_guess->getFgColor( r, g, b, a );
			if ( a > 0 )
				a -= 1;
			if ( a < 0 )
				a = 0;
			m_pGradeShadowLabel_guess->setFgColor( 255, 0, 0, a );

			if ( !m_pStatsTitleLabel_guess->isVisible() )
				m_pStatsTitleLabel_guess->setVisible( true );
			m_pStatsTitleLabel_guess->getFgColor( r, g, b, a );
			if ( a > 0 )
				a -= 5;
			if ( a < 0 )
				a = 0;
			m_pStatsTitleLabel_guess->setFgColor( m_iBrightness_guess, m_iBrightness_guess, m_iBrightness_guess, a );

			for ( i = 1; i <= 14; i++ )
			{
				if ( !m_pStatLabel_guess[i]->isVisible() )
					m_pStatLabel_guess[i]->setVisible( true );
				m_pStatLabel_guess[i]->getFgColor( r, g, b, a );
				if ( a > 0 )
					a -= 5;
				if ( a < 0 )
					a = 0;
				m_pStatLabel_guess[i]->setVisible( true );
				m_pStatLabel_guess[i]->getPos( x, y );
				m_pStatLabel_guess[i]->setFgColor( m_iBrightness_guess, m_iBrightness_guess, m_iBrightness_guess, a );
				m_pStatLabel_guess[i]->setPos( x, y );
			}
		}

		if ( m_bShowStats_guess && !m_bTextOnly_guess && !g_iCoopCredits_guess )
		{
			int r, g, b, a;

			getBgColor( r, g, b, a );
			if ( a < 255 )
				a += 5;
			if ( a > 255 )
				a = 255;
			setBgColor( 255 - m_iBrightness_guess, 255 - m_iBrightness_guess, 255 - m_iBrightness_guess, a );
		}
		else if ( !m_bTextOnly_guess )
		{
			setBgColor( 255 - m_iBrightness_guess, 255 - m_iBrightness_guess, 255 - m_iBrightness_guess, 0 );
		}

		if ( !m_bTextOnly_guess )
		{
			Panel::solve();
			return;
		}
	}

	setBgColor( 255, 255, 255, 255 );
	Panel::solve();
}

CCredits::~CCredits( void )
{
}
