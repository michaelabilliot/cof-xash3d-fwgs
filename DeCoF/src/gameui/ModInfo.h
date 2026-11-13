//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef MODINFO_H
#define MODINFO_H

#ifdef _WIN32
#pragma once
#endif

class KeyValues;

//-----------------------------------------------------------------------------
// Purpose: contains all the data entered about a mod in gameinfo.txt
//-----------------------------------------------------------------------------
class CModInfo
{
public:
	CModInfo();
	~CModInfo();
	void FreeModInfo();

	// loads mod info from gameinfo.txt
	void LoadCurrentGameInfo();

	// loads gameinfo from null-terminated string
	void LoadGameInfoFromBuffer( const char *buffer, int bufferSize );

	// data accessors
	const char *GetStartMap();
	const char *GetTrainMap();
	bool IsMultiplayerOnly();
	bool IsSinglePlayerOnly();
	bool BShowSimpleLoadingDialog();
	bool NoModels();
	bool NoHiModel();
	const char *GetGameDescription();
	const char *GetFallbackDir();
	bool GetDetailedTexture();
	bool UseFallbackDirMaps();
	const char *GetMPFilter();

private:
	KeyValues *m_pModData;
};

// singleton accessor
extern CModInfo &ModInfo();

#endif // MODINFO_H
