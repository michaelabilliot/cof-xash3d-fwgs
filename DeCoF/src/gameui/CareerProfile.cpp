//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Loading and saving the career profiles
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "CareerGame.h"
#include "CareerProfile.h"
#include "CareerDefinitions.h"
#include "EngineInterface.h"
#include "Taskbar.h"
#include "../common/parsemsg.h"
#include "../game_shared/simple_checksum.h"

#include <FileSystem.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/MessageBox.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Clears the progress at one difficulty
//-----------------------------------------------------------------------------
void CCareerProfileDifficultyData::Reset()
{
	played = false;
	points = 0;
	hired.clear();
	maps.clear();
	lastMap = "";
	pointsFlashed = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Clears the whole profile
//-----------------------------------------------------------------------------
void CCareerProfileData::Reset()
{
	name = "";
	sliderPos = 0;
	for ( int i=0; i<MAX_CAREER_DIFFICULTY; ++i )
	{
		medals[i] = false;
		difficulty[i].Reset();
	}
	memset( tutorData, 0, sizeof( tutorData ) );
}

//-----------------------------------------------------------------------------
// Purpose: Writes a buffer to a file
//-----------------------------------------------------------------------------
static bool WriteFile( const char *pszGameName, unsigned char *buf, int bufLen )
{
	FileHandle_t fp = filesystem()->Open( pszGameName, "wb" );
	if ( !fp )
		return false;

	filesystem()->Write( buf, bufLen, fp );
	filesystem()->Close( fp );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Reads a whole file into a new buffer
//-----------------------------------------------------------------------------
static unsigned char *ReadFile( const char *pszGameName, int &fileLen )
{
	FileHandle_t fp = filesystem()->Open( pszGameName, "rb" );
	if ( !fp )
	{
		fileLen = 0;
		return NULL;
	}

	fileLen = filesystem()->Size( fp );
	unsigned char *buf = new unsigned char[fileLen];
	filesystem()->Read( buf, fileLen, fp );
	filesystem()->Close( fp );
	return buf;
}

//-----------------------------------------------------------------------------
// Purpose: Reads a saved profile, checks its version and checksum
//-----------------------------------------------------------------------------
bool CCareerGame::ParseSavedProfile( const unsigned char *savedData, int dataLength, CCareerProfileData *save )
{
	if ( !savedData || dataLength < 5 || !save )
		return false;

	save->Reset();

	bool isSaveOk = true;
	BEGIN_READ( (void *)savedData, dataLength );

	char *stringVal = READ_STRING();
	if ( !stringVal || strcmp( stringVal, "CZP" ) )
		return false;

	int version = READ_LONG();
	if ( version > 0 && version <= 4 )
	{
		save->name = READ_STRING();
		save->sliderPos = READ_LONG();
		if ( version >= 4 )
		{
			save->lastDifficulty = (CareerDifficultyType)READ_BYTE();
			if ( save->lastDifficulty < 0 || save->lastDifficulty > 3 )
			{
				save->lastDifficulty = CAREER_DIFFICULTY_EASY;
			}
		}
		else
		{
			save->lastDifficulty = CAREER_DIFFICULTY_EASY;
		}

		isSaveOk = READ_OK() != 0;
		if ( isSaveOk )
		{
			isSaveOk = ParseMedalChunk( save, version );
		}

		for ( int i=0; i<MAX_CAREER_DIFFICULTY; ++i )
		{
			if ( isSaveOk )
			{
				isSaveOk = ParseDifficultyChunk( &save->difficulty[i], version );
			}
		}

		if ( isSaveOk )
		{
			isSaveOk = ParseTutorMessageDecayChunk( save, version );
		}

		unsigned int checksum = READ_LONG();
		unsigned int computedChecksum = ComputeSimpleChecksum( savedData, dataLength - 4 );
		if ( isSaveOk )
		{
			isSaveOk = checksum == computedChecksum;
		}
	}
	else
	{
		isSaveOk = false;
	}

	return isSaveOk;
}

//-----------------------------------------------------------------------------
// Purpose: The medals of each difficulty
//-----------------------------------------------------------------------------
bool CCareerGame::ParseMedalChunk( CCareerProfileData *save, int version )
{
	if ( version <= 0 || version > 4 )
		return false;

	for ( int i=0; i<MAX_CAREER_DIFFICULTY; ++i )
	{
		save->medals[i] = READ_BYTE() != 0;
	}

	return READ_OK() != 0;
}

//-----------------------------------------------------------------------------
// Purpose: Hired bots, defeated maps and points of one difficulty
//-----------------------------------------------------------------------------
bool CCareerGame::ParseDifficultyChunk( CCareerProfileDifficultyData *save, int version )
{
	if ( version <= 0 || version > 4 )
		return false;

	save->played = READ_BYTE() != 0;
	if ( !save->played )
		return READ_OK() != 0;

	save->points = READ_BYTE();

	int numHired = READ_BYTE();
	if ( numHired < 0 || numHired > 5 )
		return false;

	int i;
	for ( i=0; i<numHired; ++i )
	{
		save->hired.push_back( READ_STRING() );
	}

	int numMaps = READ_BYTE();
	if ( numMaps < 3 || numMaps % 3 )
		return false;

	for ( i=0; i<numMaps; ++i )
	{
		SavedMap m;
		m.name = READ_STRING();
		m.defeated = READ_BYTE() != 0;
		save->maps.push_back( m );
	}

	save->lastMap = READ_STRING();

	if ( version >= 3 )
	{
		save->pointsFlashed = READ_BYTE();
	}
	else
	{
		save->pointsFlashed = 0;
	}

	return READ_OK() != 0;
}

//-----------------------------------------------------------------------------
// Purpose: The tutor message decay data
//-----------------------------------------------------------------------------
bool CCareerGame::ParseTutorMessageDecayChunk( CCareerProfileData *save, int version )
{
	if ( version <= 0 || version > 4 )
		return false;

	if ( !save )
		return false;

	memset( save->tutorData, 0, sizeof( save->tutorData ) );

	if ( version == 1 )
		return true;

	int numEntries = READ_LONG();
	for ( int i=0; i<numEntries; ++i )
	{
		int val = READ_LONG();
		if ( i < 256 )
		{
			save->tutorData[i] = val;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Makes a loaded profile the current career state
//-----------------------------------------------------------------------------
void CCareerGame::ApplySavedProfile( CCareerProfileData *save, CareerDifficultyType difficulty )
{
	if ( !save )
		return;

	engine->Cvar_Set( "name", (char *)save->name.c_str() );
	m_botPoolSliderPosition = save->sliderPos;
	for ( int i=0; i<MAX_CAREER_DIFFICULTY; ++i )
	{
		m_medals[i] = save->medals[i];
	}

	ApplyDifficultyChunk( &save->difficulty[difficulty] );
	ApplyTutorMessageData( save->tutorData );
}

//-----------------------------------------------------------------------------
// Purpose: Hires the saved bots and marks the defeated maps
//-----------------------------------------------------------------------------
void CCareerGame::ApplyDifficultyChunk( CCareerProfileDifficultyData *save )
{
	if ( !save->played )
		return;

	m_reputationPoints = save->points;

	int i;
	for ( i=0; i<save->hired.size(); ++i )
	{
		Hire( save->hired[i].c_str() );
	}

	for ( i=0; i<save->maps.size(); ++i )
	{
		const char *mapName = save->maps[i].name.c_str();
		CCareerMap *pMap = GetPlayableMap( mapName );
		if ( pMap )
		{
			pMap->SetDefeated( save->maps[i].defeated );
		}
		else
		{
			const int BufLen = 64;
			char buf[BufLen];
			_snprintf( buf, BufLen, "%s_cz", mapName );
			pMap = GetPlayableMap( buf );
			if ( pMap )
			{
				pMap->SetDefeated( save->maps[i].defeated );
			}
		}
	}

	SetLastMap( save->lastMap.c_str() );
	m_reputationPointsFlashed = save->pointsFlashed;
}

//-----------------------------------------------------------------------------
// Purpose: Hands the saved tutor data to the engine
//-----------------------------------------------------------------------------
void CCareerGame::ApplyTutorMessageData( int *tutorData )
{
	engine->pfnProcessTutorMessageDecayBuffer( tutorData, 256 );
}

//-----------------------------------------------------------------------------
// Purpose: Writes a profile into a buffer, returns the bytes used
//-----------------------------------------------------------------------------
int CCareerGame::CreateProfileBuffer( CCareerProfileData *save, unsigned char *data, int dataLength, bool isNewProfile )
{
	unsigned char *tmp = data;
	int remaining = dataLength;

	m_writer->Init( data, dataLength );
	m_writer->WriteString( "CZP" );
	m_writer->WriteLong( 4 );
	m_writer->WriteString( save->name.c_str() );
	m_writer->WriteLong( save->sliderPos );
	m_writer->WriteByte( save->lastDifficulty );

	WriteMedalChunk( save );
	for ( int i=0; i<MAX_CAREER_DIFFICULTY; ++i )
	{
		WriteDifficultyChunk( &save->difficulty[i] );
	}
	WriteTutorMessageDecayChunk( isNewProfile );

	int spaceUsed = m_writer->GetSpaceUsed();
	unsigned int checksum = ComputeSimpleChecksum( data, spaceUsed );
	m_writer->WriteLong( checksum );

	return m_writer->GetSpaceUsed();
}

//-----------------------------------------------------------------------------
// Purpose: The medals of each difficulty
//-----------------------------------------------------------------------------
void CCareerGame::WriteMedalChunk( CCareerProfileData *save )
{
	for ( int i=0; i<MAX_CAREER_DIFFICULTY; ++i )
	{
		m_writer->WriteByte( save->medals[i] );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Hired bots, defeated maps and points of one difficulty
//-----------------------------------------------------------------------------
void CCareerGame::WriteDifficultyChunk( CCareerProfileDifficultyData *save )
{
	m_writer->WriteByte( save->played );
	if ( !save->played )
		return;

	m_writer->WriteByte( save->points );

	int i;
	m_writer->WriteByte( save->hired.size() );
	for ( i=0; i<save->hired.size(); ++i )
	{
		m_writer->WriteString( save->hired[i].c_str() );
	}

	m_writer->WriteByte( save->maps.size() );
	for ( i=0; i<save->maps.size(); ++i )
	{
		m_writer->WriteString( save->maps[i].name.c_str() );
		m_writer->WriteByte( save->maps[i].defeated );
	}

	m_writer->WriteString( save->lastMap.c_str() );
	m_writer->WriteByte( save->pointsFlashed );
}

//-----------------------------------------------------------------------------
// Purpose: The tutor message decay data, empty for a new profile
//-----------------------------------------------------------------------------
void CCareerGame::WriteTutorMessageDecayChunk( bool isNewProfile )
{
	int buffer[256];
	if ( isNewProfile )
	{
		memset( buffer, 0, sizeof( buffer ) );
	}
	else
	{
		engine->pfnConstructTutorMessageDecayBuffer( buffer, 256 );
	}

	m_writer->WriteLong( 256 );
	for ( int i=0; i<256; ++i )
	{
		m_writer->WriteLong( buffer[i] );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Copies the career state into the current profile
//-----------------------------------------------------------------------------
bool CCareerGame::UpdateSavedProfile()
{
	m_currentProfile->name = engine->pfnGetCvarString( "name" );
	m_currentProfile->sliderPos = m_botPoolSliderPosition;
	m_currentProfile->lastDifficulty = m_difficulty;
	UpdateMedalChunk();
	UpdateDifficultyChunk();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Copies the medals into the current profile
//-----------------------------------------------------------------------------
void CCareerGame::UpdateMedalChunk()
{
	for ( int i=0; i<MAX_CAREER_DIFFICULTY; ++i )
	{
		m_currentProfile->medals[i] = m_medals[i];
	}
}

//-----------------------------------------------------------------------------
// Purpose: Copies the roster and the maps into the current profile
//-----------------------------------------------------------------------------
void CCareerGame::UpdateDifficultyChunk()
{
	CCareerProfileDifficultyData *data = &m_currentProfile->difficulty[m_difficulty];
	data->played = true;
	data->points = m_reputationPoints;

	data->hired.clear();
	for ( CharacterVecIt c = m_hiredCharacters.begin(); c != m_hiredCharacters.end(); ++c )
	{
		CCareerCharacter *character = *c;
		if ( character )
		{
			data->hired.push_back( character->GetName() );
		}
	}

	data->maps.clear();
	for ( MapsVecIt triplets = m_maps.begin(); triplets != m_maps.end(); ++triplets )
	{
		for ( MapVecIt maps = triplets->begin(); maps != triplets->end(); ++maps )
		{
			CCareerMap *m = *maps;
			SavedMap sMap;
			sMap.defeated = m->IsDefeated();
			sMap.name = m->GetName();
			data->maps.push_back( sMap );
		}
	}

	data->lastMap = m_lastMapName;
	data->pointsFlashed = m_reputationPointsFlashed;
}

//-----------------------------------------------------------------------------
// Purpose: Loads every profile of the active career
//-----------------------------------------------------------------------------
ProfileVector CCareerGame::BuildProfileList()
{
	char szFileName[MAX_PATH];
	char szDirectory[MAX_PATH];
	sprintf( szDirectory, "%s/CZProfile-*.czp", TheCareerDefinitions->GetActiveCareerDefinition()->GetSaveDirectory() );

	ProfileVector profiles;

	FileFindHandle_t handle;
	const char *pFileName = filesystem()->FindFirst( szDirectory, &handle );
	while ( pFileName )
	{
		sprintf( szFileName, "%s/%s", TheCareerDefinitions->GetActiveCareerDefinition()->GetSaveDirectory(), pFileName );

		int dataLen = 0;
		CCareerProfileData *pProfile = NULL;
		unsigned char *data = ReadFile( szFileName, dataLen );
		if ( data )
		{
			pProfile = new CCareerProfileData;
			if ( !ParseSavedProfile( data, dataLen, pProfile ) )
			{
				delete pProfile;
				pProfile = NULL;
			}
			delete[] data;
		}

		if ( pProfile )
		{
			pProfile->filetime = filesystem()->GetFileTime( szFileName );
			profiles.push_back( pProfile );
		}

		pFileName = filesystem()->FindNext( handle );
	}
	filesystem()->FindClose( handle );

	return profiles;
}

//-----------------------------------------------------------------------------
// Purpose: Loads the profile of one player name
//-----------------------------------------------------------------------------
CCareerProfileData *CCareerGame::GetProfileForName( const char *profileName )
{
	char szFileName[MAX_PATH];
	sprintf( szFileName, "%s/CZProfile-%s.czp", TheCareerDefinitions->GetActiveCareerDefinition()->GetSaveDirectory(), profileName );

	int dataLen = 0;
	CCareerProfileData *pProfile = NULL;
	unsigned char *data = ReadFile( szFileName, dataLen );
	if ( data )
	{
		pProfile = new CCareerProfileData;
		if ( !ParseSavedProfile( data, dataLen, pProfile ) )
		{
			delete pProfile;
			pProfile = NULL;
		}
		delete[] data;
	}

	return pProfile;
}

//-----------------------------------------------------------------------------
// Purpose: Saves an empty profile for the current player name
//-----------------------------------------------------------------------------
void CCareerGame::CreateBlankProfile()
{
	CCareerProfileData profile;
	profile.Reset();
	profile.name = engine->pfnGetCvarString( "name" );
	profile.sliderPos = 0;
	for ( int i=0; i<MAX_CAREER_DIFFICULTY; ++i )
	{
		profile.medals[i] = false;
	}

	filesystem()->CreateDirHierarchy( TheCareerDefinitions->GetActiveCareerDefinition()->GetSaveDirectory(), NULL );

	char tmpGameName[MAX_PATH];
	sprintf( tmpGameName, "%s/CZProfile-%s.czp", TheCareerDefinitions->GetActiveCareerDefinition()->GetSaveDirectory(), GetSaveGamePlayerName( NULL ) );

	const int BufLen = 16384;
	unsigned char *saveBuffer = new unsigned char[BufLen];
	int saveSize = CreateProfileBuffer( &profile, saveBuffer, BufLen, true );
	WriteFile( tmpGameName, saveBuffer, saveSize );
}

//-----------------------------------------------------------------------------
// Purpose: Removes the profile file of a player name
//-----------------------------------------------------------------------------
void CCareerGame::DeleteProfile( const char *name )
{
	char tmpGameName[MAX_PATH];
	sprintf( tmpGameName, "%s/CZProfile-%s.czp", TheCareerDefinitions->GetActiveCareerDefinition()->GetSaveDirectory(), GetSaveGamePlayerName( name ) );
	filesystem()->RemoveFile( tmpGameName, NULL );
}

//-----------------------------------------------------------------------------
// Purpose: The player name with every unsafe character replaced
//-----------------------------------------------------------------------------
const char *CCareerGame::GetSaveGamePlayerName( const char *name )
{
	static char nameBuf[32];
	if ( !name )
	{
		strncpy( nameBuf, engine->pfnGetCvarString( "name" ), 32 );
	}
	else
	{
		strncpy( nameBuf, name, 32 );
	}
	nameBuf[31] = 0;

	char test[2];
	test[1] = 0;
	for ( int i=0; i<31; ++i )
	{
		test[0] = nameBuf[i];
		if ( !strstr( "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-0123456789", test ) )
		{
			nameBuf[i] = '_';
		}
	}

	return nameBuf;
}

//-----------------------------------------------------------------------------
// Purpose: Saves the current profile, optionally with the player config
//-----------------------------------------------------------------------------
bool CCareerGame::SaveProfile( bool updateConfig )
{
	if ( !m_needAutosave )
		return true;

	char tmpGameName[MAX_PATH];
	const int BufLen = 16384;
	unsigned char *saveBuffer = new unsigned char[BufLen];

	filesystem()->CreateDirHierarchy( TheCareerDefinitions->GetActiveCareerDefinition()->GetSaveDirectory(), NULL );
	sprintf( tmpGameName, "%s/CZProfile-%s.czp", TheCareerDefinitions->GetActiveCareerDefinition()->GetSaveDirectory(), GetSaveGamePlayerName( NULL ) );

	UpdateSavedProfile();
	int usedSpace = CreateProfileBuffer( m_currentProfile, saveBuffer, BufLen, false );
	bool ret = WriteFile( tmpGameName, saveBuffer, usedSpace );

	if ( updateConfig )
	{
		_snprintf( tmpGameName, sizeof( tmpGameName ), "writecfg Save/%s\n", GetSaveGamePlayerName( NULL ) );
		engine->pfnClientCmd( tmpGameName );
	}

	delete[] saveBuffer;
	return ret;
}

//-----------------------------------------------------------------------------
// Purpose: Tells the player that the saved profile is broken
//-----------------------------------------------------------------------------
static void ShowLoadError()
{
	MessageBox *box = new MessageBox( "#Career_CorruptSaveTitle", "#Career_CorruptSaveText", g_pTaskbar );
	box->DoModal();
}

//-----------------------------------------------------------------------------
// Purpose: Loads the profile of the current player name
//-----------------------------------------------------------------------------
bool CCareerGame::LoadProfile( CareerDifficultyType difficulty )
{
	char tmpGameName[MAX_PATH];
	sprintf( tmpGameName, "%s/CZProfile-%s.czp", TheCareerDefinitions->GetActiveCareerDefinition()->GetSaveDirectory(), GetSaveGamePlayerName( NULL ) );

	int fileLen;
	unsigned char *buf = ReadFile( tmpGameName, fileLen );
	if ( buf )
	{
		SetDifficulty( difficulty );
		Reset();

		bool isOk = ParseSavedProfile( buf, fileLen, m_currentProfile );
		delete[] buf;
		if ( isOk )
		{
			ApplySavedProfile( m_currentProfile, difficulty );
		}
		else
		{
			ShowLoadError();
			return false;
		}
	}

	return true;
}
