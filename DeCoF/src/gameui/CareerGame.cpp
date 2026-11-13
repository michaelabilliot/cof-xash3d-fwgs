//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: The career game state: roster, maps, tasks and the match setup
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>
#include <algorithm>

#include "CareerGame.h"
#include "CareerGameTasks.h"
#include "CareerProfile.h"
#include "CareerDefinitions.h"
#include "CareerBaseFrame.h"
#include "CvarLatch.h"
#include "EngineInterface.h"
#include "MusicManager.h"
#include "Taskbar.h"
#include "../common/parsemsg.h"
#include "../game_shared/shared_util.h"
#include "../game_shared/bot/bot_profile.h"
#include "steam/steamtypes.h"

#include <FileSystem.h>
#include <vgui/IScheme.h>
#include <vgui_controls/Controls.h>

using namespace vgui2;

int32 RandomLong( int32 lLow, int32 lHigh );
char *UTIL_va( char *format, ... );

EXPOSE_SINGLE_INTERFACE( CCareerGame, ICareerUI, CAREERUI_INTERFACE_VERSION );

CCareerGame *TheCareerGame = NULL;
bool CCareerGame::m_checksumsOk = true;

//-----------------------------------------------------------------------------
// Purpose: Debug output of the career code (compiled out)
//-----------------------------------------------------------------------------
void Career_Printf( const char *fmt, ... )
{
}

//-----------------------------------------------------------------------------
// Purpose: The CZCareer message from the game dll
//-----------------------------------------------------------------------------
int __MsgFunc_CZCareer( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	char *pString = READ_STRING();

	if ( !strcmp( pString, "START" ) )
	{
		TheCareerGame->StartMatch();
		TheCareerGame->SetRoundTimeLength( READ_SHORT() );
	}
	else if ( !strcmp( pString, "MATCH" ) )
	{
		int ctWins = READ_LONG();
		int tWins = READ_LONG();
		TheCareerGame->StopMatch( ctWins, tWins );
	}
	else if ( !strcmp( pString, "TASKDONE" ) )
	{
		int index = READ_BYTE();
		CCareerMap *pMap = TheCareerGame->GetPlayableMap( TheCareerGame->GetLastMap() );
		if ( pMap )
		{
			ITaskVec *tasks = pMap->GetTasks();
			for ( ITaskVecIt it = tasks->begin(); it != tasks->end(); ++it )
			{
				--index;
				if ( index <= 0 )
				{
					ICareerTask *task = *it;
					task->Set( true );
					break;
				}
			}
		}
	}
	else if ( !strcmp( pString, "TASKPART" ) )
	{
		int index = READ_BYTE();
		int num = READ_SHORT();
		CCareerMap *pMap = TheCareerGame->GetPlayableMap( TheCareerGame->GetLastMap() );
		if ( pMap )
		{
			ITaskVec *tasks = pMap->GetTasks();
			for ( ITaskVecIt it = tasks->begin(); it != tasks->end(); ++it )
			{
				--index;
				if ( index <= 0 )
				{
					ICareerTask *task = *it;
					task->SetPartial( num );
					break;
				}
			}
		}
	}
	else if ( !strcmp( pString, "TASKUNDONE" ) )
	{
		int index = READ_BYTE();
		CCareerMap *pMap = TheCareerGame->GetPlayableMap( TheCareerGame->GetLastMap() );
		if ( pMap )
		{
			ITaskVec *tasks = pMap->GetTasks();
			for ( ITaskVecIt it = tasks->begin(); it != tasks->end(); ++it )
			{
				--index;
				if ( index <= 0 )
				{
					ICareerTask *task = *it;
					task->Reset();
					break;
				}
			}
		}
	}
	else if ( !strcmp( pString, "GOGOGO" ) )
	{
		CCareerMap *pMap = TheCareerGame->GetPlayableMap( TheCareerGame->GetLastMap() );
		if ( pMap )
		{
			ITaskVec *tasks = pMap->GetTasks();
			for ( ITaskVecIt it = tasks->begin(); it != tasks->end(); ++it )
			{
				ICareerTask *task = *it;
				task->StartRound();
			}
		}
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Round time sent by the game dll
//-----------------------------------------------------------------------------
int CCareerGame::GetRoundTimeLength()
{
	return m_roundTimeLength;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCareerCharacter::CCareerCharacter( const BotProfile *pProfile )
{
	m_pProfile = pProfile;
}

//-----------------------------------------------------------------------------
// Purpose: Debug output
//-----------------------------------------------------------------------------
void CCareerCharacter::Print()
{
	Career_Printf( "[%d] %s: skill(%1.1f) aggression(%1.1f) teamwork(%1.1f) %d\n",
		m_pProfile->GetCost(), m_pProfile->GetName(), m_pProfile->GetSkill() * 100.0f,
		m_pProfile->GetAggression() * 100.0f, m_pProfile->GetTeamwork() * 100.0f, m_pProfile->GetWeaponPreference( 0 ) );
}

//-----------------------------------------------------------------------------
// Purpose: Writes the hire command of a hired bot
//-----------------------------------------------------------------------------
char *CCareerCharacter::Save( char *buf, int &len, int hired )
{
	if ( !hired )
		return buf;

	return BufPrintf( buf, len, "hire \"%s\"\n", m_pProfile->GetName() );
}

//-----------------------------------------------------------------------------
// Purpose: Cost of the bot
//-----------------------------------------------------------------------------
int CCareerCharacter::GetPoints() const
{
	if ( !m_pProfile )
		return 0;

	return m_pProfile->GetCost();
}

//-----------------------------------------------------------------------------
// Purpose: Name of the bot
//-----------------------------------------------------------------------------
const char *CCareerCharacter::GetName() const
{
	if ( !m_pProfile )
		return "";

	return m_pProfile->GetName();
}

//-----------------------------------------------------------------------------
// Purpose: Sorts by cost, skill, teamwork, aggression and name
//-----------------------------------------------------------------------------
bool CCareerCharacter::operator<( const CCareerCharacter &other ) const
{
	if ( !m_pProfile )
		return false;

	if ( GetPoints() != other.GetPoints() )
		return GetPoints() < other.GetPoints();

	if ( GetRange( GetSkill() ) != GetRange( other.GetSkill() ) )
		return GetSkill() < other.GetSkill();

	if ( GetRange( GetTeamwork() ) != GetRange( other.GetTeamwork() ) )
		return GetTeamwork() < other.GetTeamwork();

	if ( GetRange( GetAggression() ) != GetRange( other.GetAggression() ) )
		return GetAggression() < other.GetAggression();

	return strcmp( GetName(), other.GetName() ) < 0;
}

//-----------------------------------------------------------------------------
// Purpose: Rating (0 to 4) of a skill, teamwork or aggression value
//-----------------------------------------------------------------------------
int CCareerCharacter::GetRange( float val ) const
{
	int level = 0;

	if ( val >= 0.9f )
		level = 4;
	else if ( val >= 0.65f )
		level = 3;
	else if ( val >= 0.35f )
		level = 2;
	else if ( val >= 0.1f )
		level = 1;

	return level;
}

//-----------------------------------------------------------------------------
// Purpose: Image of the bot's skin
//-----------------------------------------------------------------------------
const char *CCareerCharacter::GetSkinThumbnailFname() const
{
	static char imageFname[MAX_PATH];

	if ( TheBotProfiles->GetCustomSkinModelname( m_pProfile->GetSkin() ) )
	{
		const char *skinName = TheBotProfiles->GetCustomSkinModelname( m_pProfile->GetSkin() );
		_snprintf( imageFname, sizeof( imageFname ), "gfx/thumbnails/skins/%s.tga", skinName );
		if ( filesystem()->FileExists( imageFname ) )
		{
			imageFname[ strlen( imageFname ) - 4 ] = 0;
			return imageFname;
		}
	}

	if ( TheCareerGame->PlayAsCT() )
	{
		switch ( m_pProfile->GetSkin() )
		{
		case 1:
			return "gfx/thumbnails/skins/urban";
		case 2:
			return "gfx/thumbnails/skins/gsg9";
		case 3:
			return "gfx/thumbnails/skins/sas";
		case 4:
			return "gfx/thumbnails/skins/gign";
		case 5:
			return "gfx/thumbnails/skins/spetsnaz";
		default:
			return "gfx/thumbnails/skins/ct_random";
		}
	}
	else
	{
		switch ( m_pProfile->GetSkin() )
		{
		case 1:
			return "gfx/thumbnails/skins/terror";
		case 2:
			return "gfx/thumbnails/skins/leet";
		case 3:
			return "gfx/thumbnails/skins/arctic";
		case 4:
			return "gfx/thumbnails/skins/guerilla";
		case 5:
			return "gfx/thumbnails/skins/militia";
		default:
			return "gfx/thumbnails/skins/t_random";
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Name of one of the bot's preferred weapons
//-----------------------------------------------------------------------------
const char *CCareerCharacter::GetWeaponPreferenceAsString( int i ) const
{
	return ( m_pProfile ) ? m_pProfile->GetWeaponPreferenceAsString( i ) : NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Number of preferred weapons
//-----------------------------------------------------------------------------
int CCareerCharacter::GetWeaponPreferenceCount() const
{
	return ( m_pProfile ) ? m_pProfile->GetWeaponPreferenceCount() : 0;
}

//-----------------------------------------------------------------------------
float CCareerCharacter::GetSkill() const
{
	return ( m_pProfile ) ? m_pProfile->GetSkill() : 0.0f;
}

//-----------------------------------------------------------------------------
float CCareerCharacter::GetTeamwork() const
{
	return ( m_pProfile ) ? m_pProfile->GetTeamwork() : 0.0f;
}

//-----------------------------------------------------------------------------
float CCareerCharacter::GetAggression() const
{
	return ( m_pProfile ) ? m_pProfile->GetAggression() : 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCareerMap::CCareerMap( const char *name, const CharacterVec &bots, bool friendlyFire )
{
	strncpy( m_name, name, 64 );
	m_name[63] = 0;
	m_bots = bots;
	m_defeated = false;
	m_friendlyFire = friendlyFire;
	m_extra = NULL;
	m_allowTeammates = true;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCareerMap::~CCareerMap()
{
	for ( ITaskVecIt it = m_tasks.begin(); it != m_tasks.end(); ++it )
	{
		delete *it;
	}
	m_tasks.clear();

	if ( m_extra )
		delete[] m_extra;

	for ( int i = 0; i < m_bots.size(); ++i )
	{
		delete m_bots[i];
	}
	m_bots.clear();
}

//-----------------------------------------------------------------------------
int CCareerMap::GetNumBotNames() const
{
	return m_bots.size();
}

//-----------------------------------------------------------------------------
const char *CCareerMap::GetBotName( int index ) const
{
	if ( index >= 0 && index < m_bots.size() )
	{
		return m_bots[index]->GetName();
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Commands to run when the map starts
//-----------------------------------------------------------------------------
void CCareerMap::SetExtraInitialization( const char *extra )
{
	if ( m_extra )
	{
		delete[] m_extra;
		m_extra = NULL;
	}

	if ( extra )
	{
		m_extra = CloneString( extra );
	}
}

//-----------------------------------------------------------------------------
bool CCareerMap::IsFriendlyFireOn() const
{
	return m_friendlyFire;
}

//-----------------------------------------------------------------------------
// Purpose: Number of enemies, more when the player has more points
//-----------------------------------------------------------------------------
int CCareerMap::GetCurrentEnemyCount() const
{
	int enemyCount = GetMinEnemyCount();

	if ( TheCareerGame->GetPoints() > GetThreshold() )
	{
		enemyCount += TheCareerGame->GetPoints() - GetThreshold();
	}

	if ( enemyCount > GetBotList().size() )
	{
		enemyCount = GetBotList().size();
	}

	return enemyCount;
}

//-----------------------------------------------------------------------------
void CCareerMap::ResetTaskCompletion()
{
	for ( ITaskVecIt it = m_tasks.begin(); it != m_tasks.end(); ++it )
	{
		ICareerTask *task = *it;
		task->Reset();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCareerGame::CCareerGame()
{
	m_initDone = false;
	m_pLastMap = NULL;
	m_lastMapName[0] = 0;
	m_reputationPoints = 0;
	m_reputationPointsFlashed = 0;
	m_queuedLoadMenu = false;
	m_queuedName[0] = 0;
	m_hiredCharacters.clear();
	m_availableCharacters.clear();
	m_maps.clear();
	m_isCT = true;
	m_currentProfile = new CCareerProfileData;
	m_writer = new BufferWriter;
	m_difficulty = CAREER_DIFFICULTY_EASY;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCareerGame::~CCareerGame()
{
	delete m_currentProfile;
	delete m_writer;

	delete TheCareerDefinitions;
	TheCareerDefinitions = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Sets up the career game and the career definitions
//-----------------------------------------------------------------------------
void CCareerGame::Create()
{
	if ( !TheCareerGame )
	{
		scheme()->LoadSchemeFromFile( "Resource/CareerScheme.res", "CareerScheme" );
		scheme()->LoadSchemeFromFile( "Resource/CareerBoxScheme.res", "CareerBoxScheme" );
		engine->pfnHookUserMsg( "CZCareer", __MsgFunc_CZCareer );
	}

	TheCareerGame = &__g_CCareerGame_singleton;
	CareerDefinitionManager::Create();

	TheBotProfiles = NULL;
	if ( TheCareerDefinitions->GetActiveCareerDefinition() )
	{
		TheBotProfiles = TheCareerDefinitions->GetActiveCareerDefinition()->GetBotProfileManager( TheCareerGame->GetDifficulty() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reads the rules, bots and maps of the active career
//-----------------------------------------------------------------------------
void CCareerGame::Init()
{
	m_hiredCharacters.clear();
	m_availableCharacters.clear();
	m_maps.clear();
	m_costAvailability.clear();

	m_initDone = true;
	m_basePoints = 0;
	m_matchNumWins = 7;
	m_matchWinBy = 3;
	m_roundTime = 5;
	m_bombCountdownTime = 45;
	m_freezeTime = 5;
	strcpy( m_lastMapName, "de_dust" );
	m_isPlaying = false;

	if ( !TheCareerDefinitions->GetActiveCareerDefinition() )
	{
		Reset();
		return;
	}

	CareerDefinition *pCareer = TheCareerDefinitions->GetActiveCareerDefinition();
	m_isCT = pCareer->IsCT();

	CareerDifficultyDefinition *pCareerData = pCareer->GetDifficulty( m_difficulty );
	m_basePoints = pCareerData->GetBasePoints();
	m_matchNumWins = pCareerData->GetMatchNumWins();
	m_matchWinBy = pCareerData->GetMatchWinBy();
	m_roundTime = pCareerData->GetRoundTimeMins();
	m_bombCountdownTime = pCareerData->GetBombCountdownTimeSecs();
	m_freezeTime = pCareerData->GetFreezeTimeSecs();
	m_availableCharacters = *pCareerData->GetCharacters();
	m_costAvailability = *pCareerData->GetCharacterCostAvailibility();
	m_maps = *pCareerData->GetMaps();

	Reset();
}

//-----------------------------------------------------------------------------
void CCareerGame::SetDifficulty( CareerDifficultyType difficulty )
{
	m_difficulty = difficulty;
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Starts a new career at the current difficulty
//-----------------------------------------------------------------------------
void CCareerGame::Reset()
{
	m_canUnpause = true;
	m_queuedLoadMenu = false;
	m_queuedName[0] = 0;
	m_botPoolSliderPosition = 0;
	m_pLastMap = NULL;
	m_lastMapName[0] = 0;
	m_matchReputationGain = 0;
	m_matchMapsUnlocked = 0;
	m_needAutosave = false;
	m_winfastLength = 0;
	m_ctWins = 0;
	m_tWins = 0;

	for ( int i = 0; i < MAX_CAREER_DIFFICULTY; ++i )
	{
		m_medals[i] = false;
	}

	ClearRoster();

	if ( !m_initDone )
	{
		m_difficulty = CAREER_DIFFICULTY_EASY;
		Init();
	}

	m_reputationPoints = m_basePoints;
	m_wasMapDefeatedAtStart = false;
	m_reputationPointsFlashed = 0;

	bool setLastMap = false;
	for ( MapsVecIt mIt = m_maps.begin(); mIt != m_maps.end(); ++mIt )
	{
		for ( int i = 0; i < 3; ++i )
		{
			CCareerMap *m = (*mIt)[i];
			m->SetDefeated( false );
			m->ResetTaskCompletion();
			if ( !setLastMap )
			{
				SetLastMap( m->GetName() );
				setLastMap = true;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Opens the resume dialog queued by the end of a match
//-----------------------------------------------------------------------------
void CCareerGame::Update()
{
	if ( m_queuedLoadMenu )
	{
		const char *lvl = engine->pfnGetLevelName();
		if ( !lvl || !*lvl )
		{
			engine->Cvar_Set( "name", m_queuedName );
			engine->pfnClientCmd( UTIL_va( "name \"%s\"", m_queuedName ) );

			CCareerBaseFrame *pFrame = (CCareerBaseFrame *)g_pTaskbar->OnOpenResumeCareerGameDialog( GetDifficulty() );
			if ( pFrame )
			{
				pFrame->ShowAutosaveNotice();
			}

			m_queuedLoadMenu = false;
			m_queuedName[0] = 0;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Moves every hired bot back to the available list
//-----------------------------------------------------------------------------
void CCareerGame::ClearRoster()
{
	for ( size_t i = 0; i < GetHiredCharacters().size(); ++i )
	{
		GetAvailableCharacters().push_back( GetHiredCharacters()[i] );
	}
	std::sort( GetAvailableCharacters().begin(), GetAvailableCharacters().end() );
	GetHiredCharacters().clear();
}

//-----------------------------------------------------------------------------
// Purpose: Adds a bot to the team
//-----------------------------------------------------------------------------
bool CCareerGame::Hire( const char *name )
{
	for ( CharacterVecIt it = GetAvailableCharacters().begin(); it != GetAvailableCharacters().end(); ++it )
	{
		CCareerCharacter *character = *it;
		if ( !stricmp( name, character->GetName() ) )
		{
			if ( character->GetPoints() <= GetPointsRemaining() )
			{
				Career_Printf( "%s has joined your team.\n", character->GetName() );
				GetHiredCharacters().push_back( character );
				GetAvailableCharacters().erase( it );
				std::sort( GetHiredCharacters().begin(), GetHiredCharacters().end() );
				return true;
			}
			Career_Printf( "%s refuses to join your team.\n", character->GetName() );
			return false;
		}
	}

	Career_Printf( "%s is not available for hiring!\n", name );
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Removes a bot from the team
//-----------------------------------------------------------------------------
bool CCareerGame::Fire( const char *name )
{
	for ( CharacterVecIt it = GetHiredCharacters().begin(); it != GetHiredCharacters().end(); ++it )
	{
		CCareerCharacter *character = *it;
		if ( !stricmp( name, character->GetName() ) )
		{
			Career_Printf( "%s has left your team.\n", character->GetName() );
			GetAvailableCharacters().push_back( character );
			GetHiredCharacters().erase( it );
			std::sort( GetAvailableCharacters().begin(), GetAvailableCharacters().end() );
			return true;
		}
	}

	Career_Printf( "%s is not on your team!\n", name );
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Points left after paying the hired bots
//-----------------------------------------------------------------------------
int CCareerGame::GetPointsRemaining()
{
	int usedRep = 0;
	for ( CharacterVecCIt it = m_hiredCharacters.begin(); it != m_hiredCharacters.end(); ++it )
	{
		CCareerCharacter *character = *it;
		usedRep += character->GetPoints();
	}
	return m_reputationPoints - usedRep;
}

//-----------------------------------------------------------------------------
// Purpose: The most expensive bot the points can buy
//-----------------------------------------------------------------------------
int CCareerGame::GetHighestCostAvailable( int reputationPoints )
{
	if ( reputationPoints < 0 )
		reputationPoints = m_reputationPoints;

	if ( m_costAvailability.size() < 2 )
		return 1000;

	for ( int i = 1; i < m_costAvailability.size(); ++i )
	{
		if ( m_costAvailability[i] && m_costAvailability[i] > reputationPoints )
		{
			return i - 1;
		}
	}

	return 1000;
}

//-----------------------------------------------------------------------------
// Purpose: Debug output of the map triplets
//-----------------------------------------------------------------------------
void CCareerGame::PrintMaps()
{
	for ( MapsVecIt it = m_maps.begin(); it != m_maps.end(); ++it )
	{
		CCareerMap *m1_guess = (*it)[0];
		CCareerMap *m2_guess = (*it)[1];
		CCareerMap *m3_guess = (*it)[2];
		Career_Printf( "%s%s%s %s%s%s %s%s%s\n",
			m1_guess->IsDefeated() ? "" : "[", m1_guess->GetName(), m1_guess->IsDefeated() ? "" : "]",
			m2_guess->IsDefeated() ? "" : "[", m2_guess->GetName(), m2_guess->IsDefeated() ? "" : "]",
			m3_guess->IsDefeated() ? "" : "[", m3_guess->GetName(), m3_guess->IsDefeated() ? "" : "]" );
	}
}

//-----------------------------------------------------------------------------
void CCareerGame::DefeatMap( const char *mapName )
{
	CCareerMap *pMap = GetPlayableMap( mapName );
	if ( pMap )
	{
		pMap->SetDefeated( true );
	}
}

//-----------------------------------------------------------------------------
const char *CCareerGame::GetLastMap()
{
	return m_lastMapName;
}

//-----------------------------------------------------------------------------
CCareerMap *CCareerGame::GetLastMapPtr()
{
	return m_pLastMap;
}

//-----------------------------------------------------------------------------
// Purpose: Selects the map to play next
//-----------------------------------------------------------------------------
bool CCareerGame::SetLastMap( const char *mapName )
{
	m_lastMapName[0] = 0;
	m_pLastMap = GetPlayableMap( mapName );
	if ( m_pLastMap )
	{
		strncpy( m_lastMapName, mapName, 64 );
		m_lastMapName[63] = 0;
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Finds a map in the unlocked triplets
//-----------------------------------------------------------------------------
CCareerMap *CCareerGame::GetPlayableMap( const char *name )
{
	for ( MapsVecIt mIt = m_maps.begin(); mIt != m_maps.end(); ++mIt )
	{
		bool passedSet = true;
		for ( int i = 0; i < 3; ++i )
		{
			CCareerMap *m = (*mIt)[i];
			if ( !stricmp( name, m->GetName() ) )
			{
				return m;
			}
			if ( !m->IsDefeated() )
			{
				passedSet = false;
			}
		}
		if ( !passedSet )
			return NULL;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Finds a map in any triplet
//-----------------------------------------------------------------------------
CCareerMap *CCareerGame::GetMap( const char *name )
{
	if ( !name )
		return NULL;

	for ( MapsVecIt mIt = m_maps.begin(); mIt != m_maps.end(); ++mIt )
	{
		for ( int i = 0; i < 3; ++i )
		{
			CCareerMap *m = (*mIt)[i];
			if ( !stricmp( name, m->GetName() ) )
			{
				return m;
			}
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Index of the triplet that holds a map
//-----------------------------------------------------------------------------
int CCareerGame::GetMapTripletNum( const char *name )
{
	if ( !name )
		return -1;

	int tripletNum = 0;
	for ( MapsVecIt mIt = m_maps.begin(); mIt != m_maps.end(); ++mIt )
	{
		for ( int i = 0; i < 3; ++i )
		{
			CCareerMap *m = (*mIt)[i];
			if ( !stricmp( name, m->GetName() ) )
			{
				return tripletNum;
			}
		}
		++tripletNum;
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Position of a map inside its triplet
//-----------------------------------------------------------------------------
int CCareerGame::GetMapTripletOffset( const char *name )
{
	if ( !name )
		return -1;

	for ( MapsVecIt mIt = m_maps.begin(); mIt != m_maps.end(); ++mIt )
	{
		for ( int i = 0; i < 3; ++i )
		{
			CCareerMap *m = (*mIt)[i];
			if ( !stricmp( name, m->GetName() ) )
			{
				return i;
			}
		}
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Fills in the maps of the current triplet
//-----------------------------------------------------------------------------
int CCareerGame::GetCurrentMapTriplet( MapInfo *maps )
{
	int tripletNum = GetMapTripletNum( m_lastMapName );
	if ( tripletNum < 0 )
		return tripletNum;

	MapVec mapVec = m_maps[tripletNum];
	for ( int i = 0; i < 3; ++i )
	{
		maps[i].name = mapVec[i]->GetName();
		maps[i].defeated = mapVec[i]->IsDefeated();
	}

	return GetMapTripletOffset( m_lastMapName );
}

//-----------------------------------------------------------------------------
// Purpose: Does winning the current map unlock the next triplet
//-----------------------------------------------------------------------------
bool CCareerGame::DoesCurrentMapUnlockTriplet()
{
	int triplet = 0;
	int numTriplets = m_maps.size();
	for ( MapsVecIt mIt = m_maps.begin(); mIt != m_maps.end(); ++mIt )
	{
		bool passedRestOfTriplet = true;
		bool isCurrentSet = false;
		bool passedCurrent = true;
		for ( int i = 0; i < 3; ++i )
		{
			CCareerMap *m = (*mIt)[i];
			if ( m == m_pLastMap )
			{
				isCurrentSet = true;
				passedCurrent = m->IsDefeated();
			}
			else if ( !m->IsDefeated() )
			{
				passedRestOfTriplet = false;
			}
		}
		if ( isCurrentSet )
		{
			return ( passedRestOfTriplet && !m_wasMapDefeatedAtStart && triplet != numTriplets - 1 );
		}
		++triplet;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Does winning the current map finish the career
//-----------------------------------------------------------------------------
bool CCareerGame::DoesWinUnlockAll()
{
	int triplet = 0;
	int numTriplets = m_maps.size();
	for ( MapsVecIt mIt = m_maps.begin(); mIt != m_maps.end(); ++mIt )
	{
		bool passedRestOfTriplet = true;
		bool isCurrentSet = false;
		bool passedCurrent = true;
		for ( int i = 0; i < 3; ++i )
		{
			CCareerMap *m = (*mIt)[i];
			if ( m == m_pLastMap )
			{
				isCurrentSet = true;
				passedCurrent = m->IsDefeated();
			}
			else if ( !m->IsDefeated() )
			{
				passedRestOfTriplet = false;
			}
		}
		if ( isCurrentSet )
		{
			return ( passedRestOfTriplet && !m_wasMapDefeatedAtStart && triplet == numTriplets - 1 );
		}
		++triplet;
	}
	return false;
}

//-----------------------------------------------------------------------------
CCareerMap *CCareerGame::GetFirstUndefeatedMap()
{
	for ( MapsVecIt mIt = m_maps.begin(); mIt != m_maps.end(); ++mIt )
	{
		for ( int i = 0; i < 3; ++i )
		{
			CCareerMap *m = (*mIt)[i];
			if ( !m->IsDefeated() )
			{
				return m;
			}
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
ITaskVec *CCareerGame::GetTaskVecForMap( const char *mapName )
{
	CCareerMap *pMap = GetPlayableMap( mapName );
	if ( pMap )
		return pMap->GetTasks();

	return NULL;
}

//-----------------------------------------------------------------------------
void CCareerGame::StartMatch()
{
	m_isPlaying = true;
	m_needAutosave = true;
}

//-----------------------------------------------------------------------------
int CCareerGame::GetNumTasksOnCurrentMap()
{
	return m_pLastMap->GetTasks()->size();
}

//-----------------------------------------------------------------------------
ICareerTask *CCareerGame::GetCurrentTask( int index )
{
	int numTasks = 0;
	for ( ITaskVecIt it = m_pLastMap->GetTasks()->begin(); it != m_pLastMap->GetTasks()->end(); ++it )
	{
		ICareerTask *task = *it;
		if ( numTasks == index )
			return task;
		++numTasks;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: The game dll reports the end of a match (or a round)
//-----------------------------------------------------------------------------
void CCareerGame::StopMatch( int ctWins, int tWins )
{
	if ( !m_isPlaying || !m_pLastMap )
		return;

	if ( ctWins || tWins )
	{
		m_ctWins = ctWins;
		m_tWins = tWins;
	}

	bool tasksCompleted = true;
	ITaskVecIt it;
	int numTasksOnMap = GetNumTasksOnCurrentMap();
	for ( int i = 0; i < numTasksOnMap; ++i )
	{
		ICareerTask *task = GetCurrentTask( i );
		if ( !task )
		{
			tasksCompleted = false;
		}
		else if ( !task->IsComplete() )
		{
			tasksCompleted = false;
		}
	}

	if ( tasksCompleted &&
		( ( m_isCT && m_ctWins - m_tWins >= GetMatchWinBy() && m_ctWins >= GetMatchNumWins() ) ||
		  ( !m_isCT && m_tWins - m_ctWins >= GetMatchWinBy() && m_tWins >= GetMatchNumWins() ) ) )
	{
		CCareerMap *pCurrentMap = m_pLastMap;
		CCareerMap *pNextMap = m_pLastMap;

		if ( DoesWinUnlockAll() )
		{
			m_medals[m_difficulty] = true;
		}

		if ( m_pLastMap && !m_pLastMap->IsDefeated() )
		{
			++m_reputationPoints;
			m_pLastMap->SetDefeated( true );
			pNextMap = GetFirstUndefeatedMap();
			if ( !pNextMap )
			{
				pNextMap = m_pLastMap;
			}
		}

		SetLastMap( pNextMap->GetName() );
		pCurrentMap->ResetTaskCompletion();
		SaveProfile( true );

		for ( it = pCurrentMap->GetTasks()->begin(); it != pCurrentMap->GetTasks()->end(); ++it )
		{
			ICareerTask *task = *it;
			task->Set( false );
		}

		SetLastMap( pCurrentMap->GetName() );
	}
	else
	{
		SaveProfile( true );
	}

	if ( ctWins || tWins )
	{
		m_needAutosave = false;
	}
	else
	{
		m_isPlaying = false;
		engine->pfnResetTutorMessageDecayData();

		if ( m_savedCvars.size() )
		{
			for ( int i = 0; i < m_savedCvars.size(); ++i )
			{
				delete m_savedCvars[i];
			}
			m_savedCvars.clear();
		}

		engine->pfnClientCmd( "exec config.cfg\n" );
	}
}

//-----------------------------------------------------------------------------
void CCareerGame::StartLoading()
{
}

//-----------------------------------------------------------------------------
// Purpose: Saves a cvar for the end of the match and sets it
//-----------------------------------------------------------------------------
void CCareerGame::AddSavedCvar( const char *cvarName, const char *cvarVal, bool wait )
{
	const int BufLen = 128;
	char szCommand[BufLen];

	m_savedCvars.push_back( new CvarLatch( cvarName, cvarVal ) );
	_snprintf( szCommand, BufLen, "%s%s \"%s\"\n", wait ? "wait\n" : "", cvarName, cvarVal );
	engine->pfnClientCmd( szCommand );
}

//-----------------------------------------------------------------------------
// Purpose: Starts the local server for the current map
//-----------------------------------------------------------------------------
void CCareerGame::SendMatchCommand()
{
	if ( !m_pLastMap )
		return;

	CareerDefinition *pActiveCareer = TheCareerDefinitions->GetActiveCareerDefinition();
	if ( !pActiveCareer )
		return;

	char szCommand[1024];
	const CharacterVec &mapBots = m_pLastMap->GetBotList();

	char szPassword[9];
	for ( int i = 0; i < 8; ++i )
	{
		szPassword[i] = RandomLong( 0, 9 ) + '0';
	}
	szPassword[8] = 0;
	Career_Printf( "Career Password is [%s]\n", szPassword );

	int numPlayers = m_pLastMap->GetCurrentEnemyCount() + 1;
	if ( m_pLastMap->AreTeammatesAllowed() )
	{
		numPlayers += GetHiredCharacters().size();
	}

	m_savedCvars.push_back( new CvarLatch( "sv_lan", NULL ) );
	m_savedCvars.push_back( new CvarLatch( "maxplayers", NULL ) );
	m_savedCvars.push_back( new CvarLatch( "sv_password", NULL ) );
	m_savedCvars.push_back( new CvarLatch( "hostname", NULL ) );
	sprintf( szCommand, "disconnect\nsv_lan 1\nsetmaster enable\nmaxplayers %i\nsv_password \"%s\"\nhostname \"%s\"\ncd fadeout\ncareer %s\n",
		numPlayers, szPassword, "Counter-Strike: Condition Zero", GetLastMap() );
	engine->pfnClientCmd( szCommand );
	engine->pfnClientCmd( "wait\nwait\n" );

	m_savedCvars.push_back( new CvarLatch( "mp_startmoney", "800" ) );
	m_savedCvars.push_back( new CvarLatch( "mp_limitteams", NULL ) );
	m_savedCvars.push_back( new CvarLatch( "humans_join_team", "any" ) );
	m_savedCvars.push_back( new CvarLatch( "mp_autoteambalance", NULL ) );
	m_savedCvars.push_back( new CvarLatch( "bot_auto_vacate", NULL ) );
	m_savedCvars.push_back( new CvarLatch( "bot_quota", NULL ) );
	m_savedCvars.push_back( new CvarLatch( "bot_join_after_player", "1" ) );
	sprintf( szCommand, "mp_startmoney 10000\nmp_limitteams 0\nhumans_join_team \"%s\"\nwait\nmp_autoteambalance 0\nwait\nbot_auto_vacate 0\nbot_quota 0\nwait\n\nwait\nenable_career_mode\n",
		PlayAsCT() ? "CT" : "T" );
	engine->pfnClientCmd( szCommand );

	m_savedCvars.push_back( new CvarLatch( "bot_defer_to_human", NULL ) );
	sprintf( szCommand, "wait\nbot_defer_to_human 1\nwait\ncareer_matchlimit %d %d\n", GetMatchNumWins(), GetMatchWinBy() );
	engine->pfnClientCmd( szCommand );

	sprintf( szCommand, "wait\nmp_roundtime %d\nmp_c4timer %d\nmp_freezetime %d\n", m_roundTime, m_bombCountdownTime, m_freezeTime );
	engine->pfnClientCmd( szCommand );

	AddSavedCvar( "bot_traceview", "0", true );
	AddSavedCvar( "bot_show_nav", "0", true );
	AddSavedCvar( "bot_show_danger", "0", true );
	AddSavedCvar( "bot_nav_edit", "0", true );
	AddSavedCvar( "bot_walk", "0", true );
	AddSavedCvar( "bot_debug", "0", true );
	AddSavedCvar( "bot_quota_match", "0", true );
	AddSavedCvar( "bot_allow_rogues", "1", true );
	AddSavedCvar( "bot_allow_pistols", "1", true );
	AddSavedCvar( "bot_allow_shotguns", "1", true );
	AddSavedCvar( "bot_allow_sub_machine_guns", "1", true );
	AddSavedCvar( "bot_allow_rifles", "1", true );
	AddSavedCvar( "bot_allow_machine_guns", "1", true );
	AddSavedCvar( "bot_allow_grenades", "1", true );
	AddSavedCvar( "bot_allow_snipers", "1", true );
	AddSavedCvar( "bot_allow_shield", "1", true );

	m_savedCvars.push_back( new CvarLatch( "mp_roundtime", NULL ) );
	m_savedCvars.push_back( new CvarLatch( "mp_c4timer", NULL ) );
	m_savedCvars.push_back( new CvarLatch( "mp_freezetime", NULL ) );
	sprintf( szCommand, "wait\nmp_roundtime %d\nmp_c4timer %d\nmp_freezetime %d\n", m_roundTime, m_bombCountdownTime, m_freezeTime );
	engine->pfnClientCmd( szCommand );

	m_winfastLength = 0;
	for ( ITaskVecIt it = m_pLastMap->GetTasks()->begin(); it != m_pLastMap->GetTasks()->end(); ++it )
	{
		ICareerTask *task = *it;
		sprintf( szCommand, "wait\ncareer_add_task %s \"%s\" %d %d %d %d\n",
			task->GetTaskName(), task->GetWeaponName(), task->GetRepeat(),
			task->MustSurvive(), task->InARow(), task->IsComplete() );
		if ( !stricmp( task->GetTaskName(), "winfast" ) )
		{
			m_winfastLength = task->GetRepeat();
			Career_Printf( "WinFast = %d\n", m_winfastLength );
		}
		engine->pfnClientCmd( szCommand );
	}

	m_savedCvars.push_back( new CvarLatch( "mp_friendlyfire", "0" ) );
	if ( m_pLastMap->IsFriendlyFireOn() )
	{
		engine->pfnClientCmd( "wait\nmp_friendlyfire 1\n" );
	}
	else
	{
		engine->pfnClientCmd( "wait\nmp_friendlyfire 0\n" );
	}

	m_savedCvars.push_back( new CvarLatch( "bot_zombie", "0" ) );
	engine->pfnClientCmd( "wait\nbot_zombie 0\n" );
	m_savedCvars.push_back( new CvarLatch( "bot_stop", "0" ) );
	engine->pfnClientCmd( "wait\nbot_stop 0\n" );
	m_savedCvars.push_back( new CvarLatch( "hostage_stop", "0" ) );
	engine->pfnClientCmd( "wait\nhostage_stop 0\n" );

	if ( m_pLastMap->IsDefeated() )
	{
		m_matchReputationGain = 0;
		m_matchMapsUnlocked = 0;
	}
	else
	{
		m_matchReputationGain = 1;
		m_matchMapsUnlocked = DoesCurrentMapUnlockTriplet() ? 1 : 0;
	}

	m_wasMapDefeatedAtStart = m_pLastMap->IsDefeated();
	StartLoading();
	m_needAutosave = true;

	bool enableTutor = ( engine->pfnGetCvarFloat( "tutor_enable" ) > 0.0f ) ? true : false;
	_snprintf( szCommand, sizeof( szCommand ), "wait\nexec Save/%s.cfg\n", GetSaveGamePlayerName( NULL ) );
	engine->pfnClientCmd( szCommand );
	_snprintf( szCommand, sizeof( szCommand ), "wait\ntutor_enable %d\n", enableTutor );
	engine->pfnClientCmd( szCommand );

	SaveProfile( false );

	filesystem()->CreateDirHierarchy( "MissionPacks", NULL );
	FileHandle_t ofp = filesystem()->Open( "MissionPacks/BotPackList.db", "w", NULL );
	if ( ofp )
	{
		filesystem()->FPrintf( ofp, "\"%s\"\n", TheCareerDefinitions->GetActiveCareerDefinition()->GetDifficulty( m_difficulty )->GetBotProfileDBFilename() );
		filesystem()->Close( ofp );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Adds the bots once the server runs
//-----------------------------------------------------------------------------
void CCareerGame::FinishMatchStart()
{
	const int BufLen = 1024;
	char szCommand[BufLen];
	const CharacterVec &mapBots = m_pLastMap->GetBotList();
	CharacterVecCIt cIt;

	int enemyCount = m_pLastMap->GetCurrentEnemyCount();
	for ( cIt = mapBots.begin(); cIt != mapBots.end(); ++cIt )
	{
		const CCareerCharacter *pChar = *cIt;
		if ( PlayAsCT() )
			sprintf( szCommand, "wait\nwait\nbot_add_t \"%s\"\n", pChar->GetName() );
		else
			sprintf( szCommand, "wait\nwait\nbot_add_ct \"%s\"\n", pChar->GetName() );
		engine->pfnClientCmd( szCommand );

		--enemyCount;
		if ( !enemyCount )
			break;
	}

	if ( m_pLastMap->AreTeammatesAllowed() )
	{
		for ( cIt = GetHiredCharacters().begin(); cIt != GetHiredCharacters().end(); ++cIt )
		{
			const CCareerCharacter *pChar = *cIt;
			if ( PlayAsCT() )
				sprintf( szCommand, "wait\nwait\nbot_add_ct \"%s\"\n", pChar->GetName() );
			else
				sprintf( szCommand, "wait\nwait\nbot_add_t \"%s\"\n", pChar->GetName() );
			engine->pfnClientCmd( szCommand );
		}
	}

	const char *extra = m_pLastMap->GetExtraInitialization();
	if ( extra )
	{
		_snprintf( szCommand, BufLen, "wait\nwait\n%s\n", extra );
		engine->pfnClientCmd( szCommand );
	}
}

//-----------------------------------------------------------------------------
bool CCareerGame::IsPlayingMatch()
{
	return engine->pfnIsCareerMatch() != 0;
}

//-----------------------------------------------------------------------------
ITaskVec *CCareerGame::GetCurrentTaskVec()
{
	if ( m_pLastMap )
		return m_pLastMap->GetTasks();

	return NULL;
}

//-----------------------------------------------------------------------------
void CCareerGame::SetBotPoolSliderPosition( int position )
{
	m_botPoolSliderPosition = position;
}

//-----------------------------------------------------------------------------
int CCareerGame::GetBotPoolSliderPosition()
{
	return m_botPoolSliderPosition;
}

//-----------------------------------------------------------------------------
void CCareerGame::OnRoundEndMenuOpen( bool didWin )
{
	m_canUnpause = false;
	if ( TheMusicManager )
	{
		if ( didWin )
			TheMusicManager->StartTrack( TRACKTYPE_ROUNDWIN, false );
		else
			TheMusicManager->StartTrack( TRACKTYPE_ROUNDLOSE, false );
	}
}

//-----------------------------------------------------------------------------
void CCareerGame::OnMatchEndMenuOpen( bool didWin )
{
	m_canUnpause = false;
	if ( TheMusicManager )
	{
		if ( didWin )
			TheMusicManager->StartTrack( TRACKTYPE_MATCHWIN, false );
		else
			TheMusicManager->StartTrack( TRACKTYPE_MATCHLOSE, false );
	}
}

//-----------------------------------------------------------------------------
void CCareerGame::OnRoundEndMenuClose( bool stillPlaying )
{
	m_canUnpause = true;
	if ( TheMusicManager )
	{
		TheMusicManager->FadeOutTrack();
	}

	if ( !stillPlaying )
	{
		m_queuedLoadMenu = true;
		strncpy( m_queuedName, engine->pfnGetCvarString( "name" ), 32 );
		m_queuedName[31] = 0;
	}
}

//-----------------------------------------------------------------------------
void CCareerGame::OnMatchEndMenuClose( bool stillPlaying )
{
	m_canUnpause = true;
	if ( TheMusicManager )
	{
		TheMusicManager->FadeOutTrack();
	}

	if ( !stillPlaying )
	{
		m_queuedLoadMenu = true;
		strncpy( m_queuedName, engine->pfnGetCvarString( "name" ), 32 );
		m_queuedName[31] = 0;
	}
	else
	{
		m_needAutosave = true;
	}
}
