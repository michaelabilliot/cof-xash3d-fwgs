//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: The mission packs and the rules, maps and bots of their difficulties
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CareerDefinitions.h"
#include "CareerGame.h"
#include "CareerGameTasks.h"
#include "EngineInterface.h"
#include "../game_shared/shared_util.h"
#include "../game_shared/bot/bot_profile.h"

#include <FileSystem.h>
#include <vgui/KeyValues.h>
#include <vgui_controls/Controls.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Looks up the bots named in a string of quoted names
//-----------------------------------------------------------------------------
static CharacterVec ParseProfilesFromString( BotProfileManager *m_botManager, const char *str )
{
	char *deletableStr = CloneString( str );
	const char *botNamesStr = deletableStr;
	CharacterVec characters;

	SharedSetQuoteChar( '\'' );
	while ( SharedTokenWaiting( botNamesStr ) )
	{
		botNamesStr = SharedParse( botNamesStr );
		const char *name = SharedGetToken();
		if ( name )
		{
			const BotProfile *pProfile = m_botManager->GetProfile( name, BOT_TEAM_ANY );
			if ( pProfile )
			{
				characters.push_back( new CCareerCharacter( pProfile ) );
			}
		}
	}
	delete[] deletableStr;
	SharedSetQuoteChar( '\"' );

	return characters;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CareerDifficultyDefinition::CareerDifficultyDefinition()
{
	m_botManager = NULL;
	m_vdfFilename = NULL;
	m_botFilename = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CareerDifficultyDefinition::~CareerDifficultyDefinition()
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: Frees the bots and the maps
//-----------------------------------------------------------------------------
void CareerDifficultyDefinition::Reset()
{
	if ( m_botManager )
		delete m_botManager;
	if ( m_vdfFilename )
		delete[] m_vdfFilename;
	if ( m_botFilename )
		delete[] m_botFilename;

	int i;
	for ( i=0; i<m_availableCharacters.size(); ++i )
	{
		delete m_availableCharacters[i];
	}
	m_availableCharacters.clear();

	m_botManager = NULL;
	m_vdfFilename = NULL;
	m_botFilename = NULL;

	if ( m_botManager )
		delete m_botManager;
	m_botManager = NULL;

	for ( MapsVecIt tripletIt = m_maps.begin(); tripletIt != m_maps.end(); ++tripletIt )
	{
		MapVec triplet = *tripletIt;
		for ( MapVecIt it = triplet.begin(); it != triplet.end(); ++it )
		{
			delete *it;
		}
	}
	m_maps.clear();
}

//-----------------------------------------------------------------------------
// Purpose: Reads the bots, costs and map triplets of one difficulty
//-----------------------------------------------------------------------------
void CareerDifficultyDefinition::Init( const char *careerName, const char *vdfFilename, const char *botFilename )
{
	char szFileName[MAX_PATH];

	for ( int i=0; i<m_availableCharacters.size(); ++i )
	{
		delete m_availableCharacters[i];
	}
	m_availableCharacters.clear();

	_snprintf( szFileName, sizeof( szFileName ), "MissionPacks/%s/%s", careerName, botFilename );
	if ( !filesystem()->FileExists( szFileName ) )
	{
		_snprintf( szFileName, sizeof( szFileName ), "BotCampaignProfile.db" );
	}
	m_botManager = new BotProfileManager;
	m_botManager->Init( szFileName );

	KeyValues *data = new KeyValues( "CareerGame" );
	m_botFilename = CloneString( szFileName );

	_snprintf( szFileName, sizeof( szFileName ), "MissionPacks/%s/%s", careerName, vdfFilename );
	if ( !data->LoadFromFile( filesystem(), szFileName ) )
	{
		_snprintf( szFileName, sizeof( szFileName ), "CareerGame%s", vdfFilename );
		if ( !data->LoadFromFile( filesystem(), szFileName ) )
		{
			data->deleteThis();
			data = NULL;
		}
	}

	m_basePoints = 2;
	m_matchNumWins = 3;
	m_matchWinBy = 2;

	if ( data )
	{
		m_basePoints = data->GetInt( "InitialPoints", m_basePoints );
		m_matchNumWins = data->GetInt( "MatchWins", m_matchNumWins );
		m_matchWinBy = data->GetInt( "MatchWinBy", m_matchWinBy );
		m_roundTimeMins = data->GetInt( "RoundTimeInMinutes", 5 );
		m_bombCountdownTimeSecs = data->GetInt( "BombCountdownTimeTimeInSeconds", 45 );
		m_freezeTimeSecs = data->GetInt( "FreezeTimeTimeInSeconds", 5 );

		m_availableCharacters = ParseProfilesFromString( m_botManager, data->GetString( "Characters", "" ) );

		KeyValues *costKey = data->FindKey( "CostAvailability" );
		if ( costKey )
		{
			int i = 1;
			int repNeeded;
			char buf[16];
			sprintf( buf, "%d", i );
			repNeeded = costKey->GetInt( buf, 0 );
			m_costAvailability.push_back( 0 );
			while ( repNeeded )
			{
				m_costAvailability.push_back( repNeeded );
				++i;
				sprintf( buf, "%d", i );
				repNeeded = costKey->GetInt( buf, 0 );
			}
		}

		KeyValues *mapsKey = data->FindKey( "Maps" );
		if ( mapsKey )
		{
			KeyValues *mapKey = mapsKey->GetFirstSubKey();
			while ( 1 )
			{
				MapVec triplet;
				int numMaps;
				for ( numMaps=0; numMaps<3; ++numMaps )
				{
					if ( !mapKey )
						break;

					const char *mapName = mapKey->GetName();
					const char *botNamesStr = mapKey->GetString( "bots", "" );
					CharacterVec bots = ParseProfilesFromString( m_botManager, botNamesStr );
					if ( !bots.size() )
					{
						mapKey = mapKey->GetNextKey();
						break;
					}

					bool friendlyFire = mapKey->GetInt( "FriendlyFire", 0 ) != 0;
					SharedSetQuoteChar( '\'' );
					CCareerMap *pMap = new CCareerMap( mapName, bots, friendlyFire );

					ITaskVec tasks;
					const char *tmp = mapKey->GetString( "tasks", "" );
					while ( tmp && SharedTokenWaiting( tmp ) )
					{
						tmp = SharedParse( tmp );
						const char *task = SharedGetToken();
						if ( task )
						{
							char *tmpTask = CloneString( task );
							tasks.push_back( new CCareerUITask( tmpTask ) );
							delete[] tmpTask;
						}
					}
					pMap->SetTasks( tasks );
					SharedSetQuoteChar( '\"' );

					const char *minEnemiesString = mapKey->GetString( "minEnemies", "3" );
					pMap->SetMinEnemyCount( atoi( minEnemiesString ) );

					const char *thresholdString = mapKey->GetString( "threshold", "9999" );
					pMap->SetThreshold( atoi( thresholdString ) );

					bool teammatesAllowed = mapKey->GetInt( "teammates", 1 ) != 0;
					pMap->SetTeammatesAllowed( teammatesAllowed );

					const char *extraInitString = mapKey->GetString( "commands", NULL );
					if ( extraInitString )
					{
						pMap->SetExtraInitialization( extraInitString );
					}

					triplet.push_back( pMap );
					mapKey = mapKey->GetNextKey();
				}

				if ( numMaps == 3 )
				{
					m_maps.push_back( triplet );
				}
				else
				{
					for ( MapVecIt it = triplet.begin(); it != triplet.end(); ++it )
					{
						delete *it;
					}
				}

				if ( !mapKey )
					break;
			}
		}

		data->deleteThis();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CareerDefinition::CareerDefinition()
{
	m_isCT = true;
	m_author = m_title = m_description = NULL;
	m_url = NULL;
	m_botFilename = m_asciiTitle = NULL;
	m_saveDir = m_careerName = NULL;
	m_asciiTitle = NULL;
	m_versionString = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CareerDefinition::~CareerDefinition()
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: Frees the strings and the difficulties
//-----------------------------------------------------------------------------
void CareerDefinition::Reset()
{
	m_isCT = true;
	if ( m_author )
		delete[] m_author;
	if ( m_title )
		delete[] m_title;
	if ( m_description )
		delete[] m_description;
	if ( m_url )
		delete[] m_url;
	if ( m_careerName )
		delete[] m_careerName;
	if ( m_saveDir )
		delete[] m_saveDir;
	if ( m_asciiTitle )
		delete[] m_asciiTitle;
	if ( m_botFilename )
		delete[] m_botFilename;
	if ( m_versionString )
		delete[] m_versionString;

	m_author = m_title = m_description = NULL;
	m_url = NULL;
	m_botFilename = m_asciiTitle = NULL;
	m_saveDir = m_careerName = NULL;
	m_asciiTitle = NULL;
	m_versionString = NULL;

	m_difficulty[CAREER_DIFFICULTY_EASY].Reset();
	m_difficulty[CAREER_DIFFICULTY_NORMAL].Reset();
	m_difficulty[CAREER_DIFFICULTY_HARD].Reset();
	m_difficulty[CAREER_DIFFICULTY_EXPERT].Reset();
}

//-----------------------------------------------------------------------------
// Purpose: Reads the overview of a mission pack and its difficulties
//-----------------------------------------------------------------------------
bool CareerDefinition::Init( const char *careerName )
{
	m_isCT = true;
	m_careerName = CloneString( careerName );

	char szFileName[MAX_PATH];
	if ( !stricmp( careerName, "TurtleRockCounterTerrorist" ) )
	{
		m_saveDir = CloneString( "Save" );
	}
	else
	{
		_snprintf( szFileName, sizeof( szFileName ), "MissionPacks/%s/Save", careerName );
		m_saveDir = CloneString( szFileName );
	}

	_snprintf( szFileName, sizeof( szFileName ), "MissionPacks/%s/overview.vdf", careerName );
	KeyValues *overview = new KeyValues( "MissionPack" );
	if ( overview->LoadFromFile( filesystem(), szFileName ) )
	{
		m_author = CloneWString( overview->GetWString( "Author", L"<unknown>" ) );
		m_title = CloneWString( overview->GetWString( "Title", L"<unknown>" ) );
		m_asciiTitle = CloneString( overview->GetString( "GameName", overview->GetString( "Title", "<unknown>" ) ) );
		m_description = CloneWString( overview->GetWString( "Description", L"" ) );
		m_url = CloneString( overview->GetString( "URL", "" ) );
		m_isCT = !stricmp( "CT", overview->GetString( "Team", "CT" ) );
		m_versionString = CloneString( overview->GetString( "Version", "1" ) );

		m_bgStartColor = overview->GetColor( "BGColor1" );
		if ( m_bgStartColor == Color( 0, 0, 0, 0 ) )
		{
			m_bgStartColor = Color( 60, 60, 60, 255 );
		}
		m_bgStartColor[3] = 255;

		m_bgEndColor = overview->GetColor( "BGColor2" );
		if ( m_bgEndColor == Color( 0, 0, 0, 0 ) )
		{
			m_bgEndColor = Color( 60, 60, 60, 255 );
		}
		m_bgEndColor[3] = 255;

		m_textColor = overview->GetColor( "TextColor" );
		if ( m_textColor == Color( 0, 0, 0, 0 ) )
		{
			m_textColor = Color( 28, 207, 61, 255 );
		}
		m_textColor[3] = 255;

		const char *botDBString = overview->GetString( "BotProfile", "" );
		if ( filesystem()->FileExists( SharedVarArgs( "MissionPacks/%s/%s", careerName, botDBString ) ) )
		{
			m_botFilename = CloneString( botDBString );
		}

		m_difficulty[CAREER_DIFFICULTY_EASY].Init( careerName, "Easy.vdf", botDBString );
		m_difficulty[CAREER_DIFFICULTY_NORMAL].Init( careerName, "Normal.vdf", botDBString );
		m_difficulty[CAREER_DIFFICULTY_HARD].Init( careerName, "Hard.vdf", botDBString );
		m_difficulty[CAREER_DIFFICULTY_EXPERT].Init( careerName, "Expert.vdf", botDBString );
	}
	overview->deleteThis();
	overview = NULL;

	if ( !m_versionString )
	{
		m_versionString = CloneString( "1" );
	}

	return ( m_title && *m_title && m_asciiTitle && *m_asciiTitle );
}

//-----------------------------------------------------------------------------
// Purpose: The bot profiles of one difficulty
//-----------------------------------------------------------------------------
BotProfileManager *CareerDefinition::GetBotProfileManager( CareerDifficultyType difficulty )
{
	return m_difficulty[difficulty].GetBotProfileManager();
}

//-----------------------------------------------------------------------------
// Purpose: Finds the mission packs and activates the saved one
//-----------------------------------------------------------------------------
CareerDefinitionManager::CareerDefinitionManager()
{
	char szDirectory[MAX_PATH];
	sprintf( szDirectory, "MissionPacks/*" );

	CareerDefinition *careerDef = NULL;
	FileFindHandle_t handle;
	const char *pFileName = filesystem()->FindFirst( szDirectory, &handle );
	while ( pFileName )
	{
		if ( pFileName[0] != '.' )
		{
			if ( filesystem()->FindIsDirectory( handle ) )
			{
				careerDef = new CareerDefinition;
				if ( careerDef->Init( pFileName ) )
				{
					Career_Printf( "Adding MissionPack %s (save dir %s)\n", pFileName, careerDef->GetSaveDirectory() );
					m_careers.AddElement( careerDef );
				}
				else
				{
					delete careerDef;
				}
				careerDef = NULL;
			}
		}
		pFileName = filesystem()->FindNext( handle );
	}
	filesystem()->FindClose( handle );

	m_activeCareer = NULL;
	KeyValues *missionPackInfo = new KeyValues( "MissionPackInfo" );
	if ( missionPackInfo->LoadFromFile( filesystem(), "MissionPackInfo.vdf" ) )
	{
		m_activeCareer = GetCareerDefinition( missionPackInfo->GetString( "ActiveMissionPack", "TurtleRockCounterTerrorist" ) );
	}
	missionPackInfo->deleteThis();

	if ( !m_activeCareer && m_careers.GetCount() )
	{
		SetActiveCareerDefinition( m_careers[0]->GetCareerName() );
	}

	if ( m_activeCareer )
	{
		TheBotProfiles = m_activeCareer->GetBotProfileManager( TheCareerGame->GetDifficulty() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CareerDefinitionManager::~CareerDefinitionManager()
{
	for ( int i=0; i<m_careers.GetCount(); ++i )
	{
		delete m_careers[i];
	}
	m_careers.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Reads a mission pack again, adding or removing it
//-----------------------------------------------------------------------------
void CareerDefinitionManager::ReloadCareerDefinition( const char *careerName )
{
	CareerDefinition *pCareer = GetCareerDefinition( careerName );
	if ( pCareer )
	{
		pCareer->Reset();
		if ( !pCareer->Init( careerName ) )
		{
			Career_Printf( "Removing MissionPack %s\n", careerName );
			m_careers.RemoveElement( pCareer );
			delete pCareer;
		}
	}
	else
	{
		pCareer = new CareerDefinition;
		if ( pCareer->Init( careerName ) )
		{
			Career_Printf( "Adding MissionPack %s (save dir %s)\n", careerName, pCareer->GetSaveDirectory() );
			m_careers.AddElement( pCareer );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Finds a mission pack by name
//-----------------------------------------------------------------------------
CareerDefinition *CareerDefinitionManager::GetCareerDefinition( const char *careerName )
{
	for ( int i=0; i<m_careers.GetCount(); ++i )
	{
		if ( !stricmp( careerName, m_careers[i]->GetCareerName() ) )
			return m_careers[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Activates a mission pack and remembers it
//-----------------------------------------------------------------------------
void CareerDefinitionManager::SetActiveCareerDefinition( const char *careerName )
{
	m_activeCareer = GetCareerDefinition( careerName );

	KeyValues *missionPackInfo = new KeyValues( "MissionPackInfo" );
	missionPackInfo->LoadFromFile( filesystem(), "MissionPackInfo.vdf" );
	if ( m_activeCareer )
	{
		missionPackInfo->SetString( "ActiveMissionPack", m_activeCareer->GetCareerName() );
		TheBotProfiles = m_activeCareer->GetBotProfileManager( TheCareerGame->GetDifficulty() );
	}
	missionPackInfo->SaveToFile( filesystem(), "MissionPackInfo.vdf" );
	missionPackInfo->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: Creates the mission pack list once
//-----------------------------------------------------------------------------
void CareerDefinitionManager::Create()
{
	if ( !TheCareerDefinitions )
	{
		TheCareerDefinitions = new CareerDefinitionManager;
	}
}

//-----------------------------------------------------------------------------
// Purpose: The number of mission packs
//-----------------------------------------------------------------------------
int CareerDefinitionManager::GetNumCareers()
{
	return m_careers.GetCount();
}

//-----------------------------------------------------------------------------
// Purpose: A mission pack by index
//-----------------------------------------------------------------------------
CareerDefinition *CareerDefinitionManager::GetCareer( int index )
{
	if ( index < 0 || index >= m_careers.GetCount() )
		return NULL;

	return m_careers[index];
}

CareerDefinitionManager *TheCareerDefinitions = NULL;
