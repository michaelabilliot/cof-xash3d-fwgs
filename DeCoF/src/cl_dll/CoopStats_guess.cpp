// ======================================
// Cry of Fear co-op statistics panel
// (class names from the RTTI of client.dll, file name guessed)
// ======================================

#include <fstream>
#include <string>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "CoopStats_guess.h"
#include "Credits_guess.h"

void IrrKlangPlayMP3_guess( const char *pszFile );
void IrrKlangSetLoop_guess( bool bLoop );

int iUnknown1018F6D0_guess = 256;	// nothing reads it in 1.6a
int g_iCoopStatsTitleTall_guess = 32;
int g_iCoopStatsWide_guess = 220;
int g_iCoopStatsTall_guess = 24;

CCoopStats::CCoopStats() : Panel( 0, 0, ScreenWidth, ScreenHeight )
{
	int i;
	int x, y;

	setBgColor( 0, 0, 0, 255 );

	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();

	m_pTitleLabel_guess = new Label( "STATS - CAMPAIGN WILL AUTORESTART SOON", 0, g_iCoopStatsTitleTall_guess, ScreenWidth, g_iCoopStatsTitleTall_guess );
	m_pTitleLabel_guess->setParent( this );
	m_pTitleLabel_guess->setFont( pSchemes->getFont( pSchemes->getSchemeHandle( "credits" ) ) );
	m_pTitleLabel_guess->setPaintBackgroundEnabled( false );
	m_pTitleLabel_guess->setFgColor( 255, 255, 255, 0 );
	m_pTitleLabel_guess->setText( "STATS - CAMPAIGN WILL AUTORESTART SOON" );
	m_pTitleLabel_guess->setContentAlignment( Label::a_center );

	for ( i = 1; i <= 9; i++ )
	{
		m_pStatLabel_guess[1][i] = new Label( "-", ScreenWidth / 2 - g_iCoopStatsWide_guess, ( i - 10 ) * g_iCoopStatsTall_guess + ScreenHeight / 2, g_iCoopStatsWide_guess, g_iCoopStatsTall_guess );
		m_pStatLabel_guess[1][i]->setParent( this );
		m_pStatLabel_guess[1][i]->setFont( pSchemes->getFont( pSchemes->getSchemeHandle( "pager" ) ) );
		m_pStatLabel_guess[1][i]->setPaintBackgroundEnabled( false );
		m_pStatLabel_guess[1][i]->setFgColor( 255, 255, 255, 0 );
		m_pStatLabel_guess[1][i]->setContentAlignment( Label::a_west );
	}

	for ( i = 1; i <= 9; i++ )
	{
		m_pStatLabel_guess[2][i] = new Label( "-", ScreenWidth / 2 - g_iCoopStatsWide_guess, ( i - 1 ) * g_iCoopStatsTall_guess + ScreenHeight / 2, g_iCoopStatsWide_guess, g_iCoopStatsTall_guess );
		m_pStatLabel_guess[2][i]->setParent( this );
		m_pStatLabel_guess[2][i]->setFont( pSchemes->getFont( pSchemes->getSchemeHandle( "pager" ) ) );
		m_pStatLabel_guess[2][i]->setPaintBackgroundEnabled( false );
		m_pStatLabel_guess[2][i]->setFgColor( 255, 255, 255, 0 );
		m_pStatLabel_guess[2][i]->setContentAlignment( Label::a_west );
	}

	m_pStatLabel_guess[2][9]->getPos( x, y );
	y += g_iCoopStatsTall_guess;

	m_pGradeLabel_guess = new Label( " ", ScreenWidth / 2 - g_iCoopStatsWide_guess / 2, y, g_iCoopStatsWide_guess, g_iCoopStatsTall_guess );
	m_pGradeLabel_guess->setParent( this );
	m_pGradeLabel_guess->setFont( pSchemes->getFont( pSchemes->getSchemeHandle( "pager" ) ) );
	m_pGradeLabel_guess->setPaintBackgroundEnabled( false );
	m_pGradeLabel_guess->setFgColor( 255, 255, 255, 0 );
	m_pGradeLabel_guess->setText( " " );
	m_pGradeLabel_guess->setContentAlignment( Label::a_center );

	for ( i = 1; i <= 9; i++ )
	{
		m_pStatLabel_guess[3][i] = new Label( "-", ScreenWidth / 2, ( i - 10 ) * g_iCoopStatsTall_guess + ScreenHeight / 2, g_iCoopStatsWide_guess, g_iCoopStatsTall_guess );
		m_pStatLabel_guess[3][i]->setParent( this );
		m_pStatLabel_guess[3][i]->setFont( pSchemes->getFont( pSchemes->getSchemeHandle( "pager" ) ) );
		m_pStatLabel_guess[3][i]->setPaintBackgroundEnabled( false );
		m_pStatLabel_guess[3][i]->setFgColor( 255, 255, 255, 0 );
		m_pStatLabel_guess[3][i]->setContentAlignment( Label::a_east );
	}

	for ( i = 1; i <= 9; i++ )
	{
		m_pStatLabel_guess[4][i] = new Label( "-", ScreenWidth / 2, ( i - 1 ) * g_iCoopStatsTall_guess + ScreenHeight / 2, g_iCoopStatsWide_guess, g_iCoopStatsTall_guess );
		m_pStatLabel_guess[4][i]->setParent( this );
		m_pStatLabel_guess[4][i]->setFont( pSchemes->getFont( pSchemes->getSchemeHandle( "pager" ) ) );
		m_pStatLabel_guess[4][i]->setPaintBackgroundEnabled( false );
		m_pStatLabel_guess[4][i]->setFgColor( 255, 255, 255, 0 );
		m_pStatLabel_guess[4][i]->setContentAlignment( Label::a_east );
	}

	Initialize();
}


void SetEndingOrUnlockables( int type, bool hasntgot, bool ending6, bool ending7 )
{
	const char *pszGameDir = gEngfuncs.pfnGetGameDirectory();

	if ( !pszGameDir )
		return;

	char sFileName[256];
	strcpy( sFileName, pszGameDir );
	strcat( sFileName, "/scriptsettings.dat" );

	char sFileNameSecond[256];
	strcpy( sFileNameSecond, pszGameDir );
	strcat( sFileNameSecond, "/alt.dat" );

	std::ifstream file( sFileName );
	std::string line;

	std::ofstream file2( sFileNameSecond );
	std::string line2;

	if ( file.fail() )
		return;

	if ( file2.fail() )
		return;

	int curLine = 0;

	if ( ending6 || ending7 )
		curLine = 666;

	switch ( type )
	{
	case 1:
		curLine = 15;
		break;
	case 2:
		curLine = 7;
		break;
	case 4:
		curLine = 45;
		break;
	case 8:
		curLine = 22;
		break;
	case 16:
		curLine = 3;
		break;
	case 64:
		curLine = 39;
		break;
	case 128:
		curLine = 51;
		break;
	case 0x100:
		curLine = 52;
		break;
	case 0x200:
		curLine = 53;
		break;
	case 0x400:
		curLine = 54;
		break;
	case 0x800:
		curLine = 55;
		break;
	case 0x1000:
		curLine = 56;
		break;
	case 0x2000:
		curLine = 57;
		break;
	case 0x4000:
		curLine = 58;
		break;
	case 0x8000:
		curLine = 59;
		break;
	case 0x10000:
		curLine = 60;
		break;
	case 0x20000:
		curLine = 61;
		break;
	case 0x40000:
		curLine = 62;
		break;
	case 0x80000:
		curLine = 70;
		break;
	case 0x100000:
		curLine = 71;
		break;
	case 0x200000:
		curLine = 72;
		break;
	case 0x400000:
		curLine = 73;
		break;
	case 0x800000:
		curLine = 74;
		break;
	case 0x1000000:
		curLine = 75;
		break;
	case 0x2000000:
		curLine = 76;
		break;
	case 0x4000000:
		curLine = 77;
		break;
	case 0x8000000:
		curLine = 78;
		break;
	case 0x10000000:
		curLine = 79;
		break;
	case 0x20000000:
		curLine = 80;
		break;
	case 0x40000000:
		curLine = 81;
		break;
	case 0x80000000:
		curLine = 82;
		break;
	case 32:
		curLine = 83;
		break;
	}

	if ( curLine == 0 )
		return;

	int i = 1;

	char DestinationText[256];

	while ( std::getline( file, line ) )
	{
		if ( i == curLine )
		{
			strcpy( DestinationText, line.c_str() );
			DestinationText[ line.length() ] = 0;

			switch ( type )
			{
			case 1:
				if ( hasntgot )
					file2 << "v9agjak+p)(,o3qb)aq4\n";
				else
					file2 << "v9agjak+p)(,03qb)aq4\n";
				break;

			case 2:
				if ( hasntgot )
					file2 << "ye&ole1^0(fcanb3v5t4\n";
				else
					file2 << "ye&0le1^0(fcanb3v5t4\n";
				break;

			case 4:
				if ( hasntgot )
					file2 << "1r6fd46ob7u2yj9q83yx\n";
				else
					file2 << "1r6fd46ob7u2yj9r83yx\n";
				break;

			case 8:
				if ( hasntgot )
					file2 << "2quwo6z3y4w\243ld)y!i35\n";
				else
					file2 << "2quvo6z3y4w\243ld)y!i35\n";
				break;

			case 16:
				if ( hasntgot )
					file2 << "z3,4lj)4b!9g7vgi4x5a\n";
				else
					file2 << "z3,4rj)4b!9g7vgi4x5a\n";
				break;

			case 64:
				if ( hasntgot )
					file2 << "r1&,iji4cjtvf,44w919\n";
				else
					file2 << "r1&,iji4cjtvf.44w919\n";
				break;

			case 128:
				if ( hasntgot )
					file2 << "olt0\243s^\243rn(aki!9eqkh\n";
				else
					file2 << "olt0\243s^\243rn(aki!9eqkj\n";
				break;

			case 0x100:
				if ( hasntgot )
					file2 << "h*wchks\243n&%!\243bf%jtew\n";
				else
					file2 << "h*wchks\243n&%!\243bf%jtei\n";
				break;

			case 0x200:
				if ( hasntgot )
					file2 << "%xn%i)jhxmfnbgjsr^if\n";
				else
					file2 << "\243xn%i)jhxmfnbgjsr^if\n";
				break;

			case 0x400:
				if ( hasntgot )
					file2 << "fy^xix&&vkz)vcdn!wtv\n";
				else
					file2 << "fy^xix&&vkz)vcrn!wtv\n";
				break;

			case 0x800:
				if ( hasntgot )
					file2 << "jwsyv**yb&(qtqlg)moj\n";
				else
					file2 << "jwtyv**yb&(qtqlg)moj\n";
				break;

			case 0x1000:
				if ( hasntgot )
					file2 << "henp(cm$at!n%\243f(c$jp\n";
				else
					file2 << "henp(cm$at!m%\243f(c$jp\n";
				break;

			case 0x2000:
				if ( hasntgot )
					file2 << "yef!xkyzw*^vd*tj)zkc\n";
				else
					file2 << "yef!&kyzw*^vd*tj)zkc\n";
				break;

			case 0x4000:
				if ( hasntgot )
					file2 << "z(*mbwy(talqlrqf$lmw\n";
				else
					file2 << "z(*mbwy(tolqlrqf$lmw\n";
				break;

			case 0x8000:
				if ( hasntgot )
					file2 << "zgpwg\243vyqlzensuauwat\n";
				else
					file2 << "zwpwg\243vyqlzensuauwat\n";
				break;

			case 0x10000:
				if ( hasntgot )
					file2 << "dzpy)*iiy$oj!akyfbn%\n";
				else
					file2 << "dzpy)*iiy$0j!akyfbn%\n";
				break;

			case 0x20000:
				if ( hasntgot )
					file2 << "pe%%*\243\243rxa(%(bog)l^j\n";
				else
					file2 << "pe%%*\243\243rxr(%(bog)l^j\n";
				break;

			case 0x40000:
				if ( hasntgot )
					file2 << "efq%)x)mk\243*odxsdocb)\n";
				else
					file2 << "efq%)u)mk\243*odxsdocb)\n";
				break;

			case 0x80000:
				if ( hasntgot )
					file2 << "pzakmx&p^lbk^kw($(fo\n";
				else
					file2 << "pzakhx&p^lbk^kw($(fo\n";
				break;

			case 0x100000:
				if ( hasntgot )
					file2 << "g*q$cceqhz&vcdp(au%!\n";
				else
					file2 << "g*q$cceqhz&vcdp(av%!\n";
				break;

			case 0x200000:
				if ( hasntgot )
					file2 << "aaa^$i)whenzprdh!vus\n";
				else
					file2 << "aba^$i)whenzprdh!vus\n";
				break;

			case 0x400000:
				if ( hasntgot )
					file2 << "mwu\243g$&ztmkfwej(bg^a\n";
				else
					file2 << "mwu\243g$&stmkfwej(bg^a\n";
				break;

			case 0x800000:
				if ( hasntgot )
					file2 << "tzi&aoxfbkb^n&ikaxj%\n";
				else
					file2 << "tzi&a0xfbkb^n&ikaxj%\n";
				break;

			case 0x1000000:
				if ( hasntgot )
					file2 << "wph!uhhgrws)*erdb*rg\n";
				else
					file2 << "wph!uhmgrws)*erdb*rg\n";
				break;

			case 0x2000000:
				if ( hasntgot )
					file2 << "&x&w%dvcr&$yoqs%c%ar\n";
				else
					file2 << "&x&v%dvcr&$yoqs%c%ar\n";
				break;

			case 0x4000000:
				if ( hasntgot )
					file2 << "feyvvuxq^i)brfentm$&\n";
				else
					file2 << "foyvvuxq^i)brfentm$&\n";
				break;

			case 0x8000000:
				if ( hasntgot )
					file2 << "*j%gb*^noyresqjihoez\n";
				else
					file2 << "*j%gb*^noyresqjihoes\n";
				break;

			case 0x10000000:
				if ( hasntgot )
					file2 << "nz(xdxj*w(\243!&uh\243qn*j\n";
				else
					file2 << "nz(xdxj*w)\243!&uh\243qn*j\n";
				break;

			case 0x20000000:
				if ( hasntgot )
					file2 << "lvdw&bhwjqsjm$pfwnp)\n";
				else
					file2 << "rvdw&bhwjqsjm$pfwnp)\n";
				break;

			case 0x40000000:
				if ( hasntgot )
					file2 << "zqv%n(nqquvh)dlmtl!x\n";
				else
					file2 << "zqv%n(noquvh)dlmtl!x\n";
				break;

			case 0x80000000:
				if ( hasntgot )
					file2 << ")cfhc$vle$^hn\243padlvd\n";
				else
					file2 << ")cfhc$vle$^hn\243podlvd\n";
				break;

			case 32:
				if ( hasntgot )
					file2 << "&fx!hd$bw^ayk%dfmcbg\n";
				else
					file2 << "&fx!hd$bw!ayk%dfmcbg\n";
				break;
			}
		}
		else if ( ending6 && i == 47 )
		{
			strcpy( DestinationText, line.c_str() );
			DestinationText[ line.length() ] = 0;

			if ( hasntgot )
				file2 << "g_t\2430j^zrqhli09jb5rc\n";
			else
				file2 << "c_t\2430j^zrqhli09jb5rc\n";
		}
		else if ( ending7 && i == 48 )
		{
			strcpy( DestinationText, line.c_str() );
			DestinationText[ line.length() ] = 0;

			if ( hasntgot )
				file2 << "kk78kewvx9!%n218dkss\n";
			else
				file2 << "ck78kewvx9!%n218dkss\n";
		}
		else
		{
			strcpy( DestinationText, line.c_str() );
			DestinationText[ line.length() ] = 0;

			file2 << DestinationText << "\n";
		}

		i++;
	}

	file.close();
	file2.close();

	if ( remove( sFileName ) == 0 )
		rename( sFileNameSecond, sFileName );
}

void CCoopStats::Initialize()
{
	setVisible( false );
	m_iUnknownBC_guess = 0;
}

int MsgBeatCoop_guess( const char *pszName, int iSize, void *pbuf )
{
	SetEndingOrUnlockables( 0, false, false, true );
	return 1;
}

int MsgCoopStats_guess( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	if ( !gViewPort->m_pCoopStats_guess )
		return 1;

	int iPlayer = READ_BYTE();
	int iShotsFired = READ_SHORT();
	int iShotsHit = READ_SHORT();
	READ_SHORT();
	int iShotsMissed = READ_SHORT();
	float flAccuracy = READ_COORD();
	int iDamageTaken = READ_SHORT();
	int iSeconds = READ_SHORT();
	int iMinutes = READ_SHORT();
	int iHours = READ_SHORT();
	int iMeleeKills = READ_SHORT();
	int iShootingKills = READ_SHORT();
	int iShow = READ_BYTE();
	int iDead = READ_BYTE();
	int iBest = READ_BYTE();
	READ_BYTE();
	READ_BYTE();
	int iTimeAttack = READ_BYTE();
	int iSecondsEarned = READ_SHORT();
	int iVisible = READ_BYTE();
	int iScore = READ_SHORT();

	int iGrade;

	if ( iScore >= 1560 )
		iGrade = 1;
	else if ( iScore >= 1500 )
		iGrade = 2;
	else if ( iScore >= 1440 )
		iGrade = 3;
	else if ( iScore >= 1200 )
		iGrade = 4;
	else if ( iScore >= 1080 )
		iGrade = 5;
	else
		iGrade = ( iScore >= 900 ) ? 6 : 7;

	gViewPort->m_pCoopStats_guess->m_pGradeLabel_guess->setText( "Ending seen: 7" );

	if ( iTimeAttack )
	{
		IrrKlangSetLoop_guess( true );
		IrrKlangPlayMP3_guess( "stats.mp3" );

		switch ( iGrade )
		{
		case 1:
			gViewPort->m_pCoopStats_guess->m_pGradeLabel_guess->setText( "Grade: S" );
			break;
		case 2:
			gViewPort->m_pCoopStats_guess->m_pGradeLabel_guess->setText( "Grade: A" );
			break;
		case 3:
			gViewPort->m_pCoopStats_guess->m_pGradeLabel_guess->setText( "Grade: B" );
			break;
		case 4:
			gViewPort->m_pCoopStats_guess->m_pGradeLabel_guess->setText( "Grade: C" );
			break;
		case 5:
			gViewPort->m_pCoopStats_guess->m_pGradeLabel_guess->setText( "Grade: D" );
			break;
		case 6:
			gViewPort->m_pCoopStats_guess->m_pGradeLabel_guess->setText( "Grade: E" );
			break;
		case 7:
			gViewPort->m_pCoopStats_guess->m_pGradeLabel_guess->setText( "Grade: F" );
			break;
		}
	}

	if ( !iShow )
	{
		gViewPort->m_pCoopStats_guess->setVisible( false );
		return 0;
	}

	if ( iShow == 2 )
		gViewPort->m_pCoopStats_guess->setVisible( true );

	if ( iVisible )
		gViewPort->m_pCoopStats_guess->setVisible( true );

	if ( iPlayer >= 1 && g_PlayerInfoList[iPlayer].name && strlen( g_PlayerInfoList[iPlayer].name ) >= 1 )
	{
		if ( iDead )
		{
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][1]->setFgColor( 117, 0, 0, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][2]->setFgColor( 117, 0, 0, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][3]->setFgColor( 117, 0, 0, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][4]->setFgColor( 117, 0, 0, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][5]->setFgColor( 117, 0, 0, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][6]->setFgColor( 117, 0, 0, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][7]->setFgColor( 117, 0, 0, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][8]->setFgColor( 117, 0, 0, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][9]->setFgColor( 117, 0, 0, 0 );
		}
		else if ( iBest )
		{
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][1]->setFgColor( 255, 211, 81, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][2]->setFgColor( 255, 211, 81, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][3]->setFgColor( 255, 211, 81, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][4]->setFgColor( 255, 211, 81, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][5]->setFgColor( 255, 211, 81, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][6]->setFgColor( 255, 211, 81, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][7]->setFgColor( 255, 211, 81, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][8]->setFgColor( 255, 211, 81, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][9]->setFgColor( 255, 211, 81, 0 );
		}
		else
		{
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][1]->setFgColor( 255, 255, 255, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][2]->setFgColor( 255, 255, 255, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][3]->setFgColor( 255, 255, 255, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][4]->setFgColor( 255, 255, 255, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][5]->setFgColor( 255, 255, 255, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][6]->setFgColor( 255, 255, 255, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][7]->setFgColor( 255, 255, 255, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][8]->setFgColor( 255, 255, 255, 0 );
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][9]->setFgColor( 255, 255, 255, 0 );
		}

		gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][1]->setText( UTIL_VarArgs_guess( "%s - Shots fired: %i", g_PlayerInfoList[iPlayer].name, iShotsFired ) );
		gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][2]->setText( UTIL_VarArgs_guess( "%s - Shots hit: %i", g_PlayerInfoList[iPlayer].name, iShotsHit ) );
		gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][3]->setText( UTIL_VarArgs_guess( "%s - Shots missed: %i", g_PlayerInfoList[iPlayer].name, iShotsMissed ) );
		gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][4]->setText( UTIL_VarArgs_guess( "%s - Accuracy: %i", g_PlayerInfoList[iPlayer].name, (int)flAccuracy ) );
		gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][5]->setText( UTIL_VarArgs_guess( "%s - Damage taken: %i", g_PlayerInfoList[iPlayer].name, iDamageTaken ) );
		gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][6]->setText( UTIL_VarArgs_guess( "%s - Time taken: %i:%i:%i", g_PlayerInfoList[iPlayer].name, iHours, iMinutes, iSeconds ) );
		gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][7]->setText( UTIL_VarArgs_guess( "%s - Killed by melee: %i", g_PlayerInfoList[iPlayer].name, iMeleeKills ) );
		gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][8]->setText( UTIL_VarArgs_guess( "%s - Killed by shooting: %i", g_PlayerInfoList[iPlayer].name, iShootingKills ) );

		if ( iTimeAttack )
			gViewPort->m_pCoopStats_guess->m_pStatLabel_guess[iPlayer][9]->setText( UTIL_VarArgs_guess( "%s - Seconds earned: %i", g_PlayerInfoList[iPlayer].name, iSecondsEarned ) );
	}

	return 1;
}

void VGUI_CoopStatsInit_guess()
{
	gEngfuncs.pfnHookUserMsg( "CoopStats", MsgCoopStats_guess );
	gEngfuncs.pfnHookUserMsg( "BeatCoop", MsgBeatCoop_guess );
}

CCoopStats::~CCoopStats( void )
{
}
