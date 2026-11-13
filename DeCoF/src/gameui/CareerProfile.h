//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Saved career profile data
//
// $NoKeywords: $
//=============================================================================

#ifndef CAREERPROFILE_H
#define CAREERPROFILE_H

#ifdef _WIN32
#pragma once
#endif

#include "ICareerUI.h"
#include <string>
#include <vector>

typedef std::vector< std::string > StringVector;

class SavedMap
{
public:
	std::string name;
	bool defeated;
};

typedef std::vector< SavedMap > SavedMapVector;

//-----------------------------------------------------------------------------
// Purpose: Career progress at one difficulty level
//-----------------------------------------------------------------------------
class CCareerProfileDifficultyData
{
public:
	void Reset();
	CCareerProfileDifficultyData() { Reset(); }

	bool played;
	int points;
	StringVector hired;
	SavedMapVector maps;
	std::string lastMap;
	int pointsFlashed;
};

//-----------------------------------------------------------------------------
// Purpose: One saved career profile
//-----------------------------------------------------------------------------
class CCareerProfileData
{
public:
	void Reset();

	long filetime;
	std::string name;
	int sliderPos;
	CareerDifficultyType lastDifficulty;
	bool medals[MAX_CAREER_DIFFICULTY];
	CCareerProfileDifficultyData difficulty[MAX_CAREER_DIFFICULTY];
	int tutorData[256];
};

typedef std::vector< CCareerProfileData * > ProfileVector;

#endif // CAREERPROFILE_H
