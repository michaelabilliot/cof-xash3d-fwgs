//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Career (mission pack) definitions read from the career scripts
//
// $NoKeywords: $
//=============================================================================

#ifndef CAREERDEFINITIONS_H
#define CAREERDEFINITIONS_H

#ifdef _WIN32
#pragma once
#endif

#include "CareerGame.h"
#include <Color.h>
#include <vgui/Dar.h>

class BotProfileManager;

//-----------------------------------------------------------------------------
// Purpose: Rules, maps and bots of one career difficulty
//-----------------------------------------------------------------------------
class CareerDifficultyDefinition
{
public:
	CareerDifficultyDefinition();
	~CareerDifficultyDefinition();
	void Init( const char *careerName, const char *vdfFilename, const char *botFilename );
	void Reset();

	BotProfileManager *GetBotProfileManager() { return m_botManager; }

	CharacterVec *GetCharacters() { return &m_availableCharacters; }
	CharCostVector *GetCharacterCostAvailibility() { return &m_costAvailability; }

	int GetBasePoints() const { return m_basePoints; }
	int GetMatchNumWins() const { return m_matchNumWins; }
	int GetMatchWinBy() const { return m_matchWinBy; }
	int GetRoundTimeMins() const { return m_roundTimeMins; }
	int GetBombCountdownTimeSecs() const { return m_bombCountdownTimeSecs; }
	int GetFreezeTimeSecs() const { return m_freezeTimeSecs; }

	MapsVec *GetMaps() { return &m_maps; }

	const char *GetBotProfileDBFilename() const { return m_botFilename; }

private:
	int m_basePoints;
	int m_matchNumWins;
	int m_matchWinBy;
	int m_roundTimeMins;
	int m_bombCountdownTimeSecs;
	int m_freezeTimeSecs;
	MapsVec m_maps;
	CharCostVector m_costAvailability;
	CharacterVec m_availableCharacters;
	BotProfileManager *m_botManager;
	char *m_vdfFilename;
	char *m_botFilename;
};

//-----------------------------------------------------------------------------
// Purpose: One career (mission pack)
//-----------------------------------------------------------------------------
class CareerDefinition
{
public:
	CareerDefinition();
	void Reset();
	bool Init( const char *careerName );
	~CareerDefinition();

	const char *GetCareerName() const { return m_careerName; }
	const char *GetSaveDirectory() const { return m_saveDir; }
	BotProfileManager *GetBotProfileManager( CareerDifficultyType difficulty );

	bool IsCT() const { return m_isCT; }

	const wchar_t *GetAuthor() const { return m_author; }
	const wchar_t *GetTitle() const { return m_title; }
	const wchar_t *GetDescription() const { return m_description; }
	const char *GetURL() const { return m_url; }
	const char *GetAsciiTitle() const { return m_asciiTitle; }
	const char *GetVersion() const { return m_versionString; }

	Color GetBgStartColor() const { return m_bgStartColor; }
	Color GetBgEndColor() const { return m_bgEndColor; }
	Color GetTextColor() const { return m_textColor; }

	CareerDifficultyDefinition *GetDifficulty( CareerDifficultyType difficulty ) { return &m_difficulty[difficulty]; }
	const CareerDifficultyDefinition *GetDifficulty( CareerDifficultyType difficulty ) const { return &m_difficulty[difficulty]; }

	const char *GetBotFilename() const { return m_botFilename; }

private:
	bool m_isCT;
	wchar_t *m_author;
	wchar_t *m_title;
	wchar_t *m_description;
	char *m_url;
	char *m_careerName;
	char *m_saveDir;
	char *m_asciiTitle;
	char *m_botFilename;
	char *m_versionString;
	Color m_bgStartColor;
	Color m_bgEndColor;
	Color m_textColor;
	CareerDifficultyDefinition m_difficulty[MAX_CAREER_DIFFICULTY];
};

//-----------------------------------------------------------------------------
// Purpose: The list of careers and the active one
//-----------------------------------------------------------------------------
class CareerDefinitionManager
{
public:
	CareerDefinitionManager();
	~CareerDefinitionManager();
	static void Create();

	CareerDefinition *GetCareerDefinition( const char *careerName );
	CareerDefinition *GetActiveCareerDefinition() { return m_activeCareer; }
	void SetActiveCareerDefinition( const char *careerName );
	int GetNumCareers();
	CareerDefinition *GetCareer( int index );
	void ReloadCareerDefinition( const char *careerName );

private:
	vgui2::Dar< CareerDefinition * > m_careers;
	CareerDefinition *m_activeCareer;
};

extern CareerDefinitionManager *TheCareerDefinitions;

#endif // CAREERDEFINITIONS_H
