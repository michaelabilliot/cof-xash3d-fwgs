//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Interface to running the engine from the UI dlls
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "IRunGameEngine.h"
#include "EngineInterface.h"

//-----------------------------------------------------------------------------
// Purpose: Interface to running the engine from the UI dlls
//-----------------------------------------------------------------------------
class CRunGameEngine : public IRunGameEngine
{
public:
	// Returns true if the engine is running, false otherwise.
	virtual bool IsRunning()
	{
		return true;
	}

	// Adds text to the engine command buffer. Only works if IsRunning()
	// returns true on success, false on failure
	virtual bool AddTextCommand( const char *text )
	{
		engine->pfnClientCmd( (char *)text );
		return true;
	}

	// runs the engine with the specified command line parameters.  Only works if !IsRunning()
	// returns true on success, false on failure
	virtual ERunResult RunEngine( int iAppID, const char *gameDir, const char *commandLineParams )
	{
		return k_ERunResultNotInitialized;
	}

	// returns true if the player is currently connected to a game server
	virtual bool IsInGame()
	{
		return engine->pfnGetLevelName() && strlen( engine->pfnGetLevelName() ) > 0;
	}

	// gets information about the server the engine is currently connected to
	// returns true on success, false on failure
	virtual bool GetGameInfo( char *infoBuffer, int bufferSize )
	{
		//!! need to implement
		return false;
	}

	virtual void SetTrackerUserID( int trackerID, const char *trackerName )
	{
		char buf[32];
		sprintf( buf, "%d", trackerID );
		engine->PlayerInfo_SetValueForKey( "*fid", buf );
		engine->PlayerInfo_SetValueForKey( "*fname", (char *)trackerName );

		// update the player's name if necessary
		const char *name = engine->pfnGetCvarString( "name" );
		if ( name && trackerName && *trackerName && !strcmp( name, "Player" ) )
		{
			engine->Cvar_Set( "name", (char *)trackerName );
		}
	}

	// iterates users
	// returns the number of user
	virtual int GetPlayerCount()
	{
		return engine->GetMaxClients();
	}

	// returns a playerID for a player
	// playerIndex is in the range [0, GetPlayerCount)
	virtual unsigned int GetPlayerUserID( int playerIndex )
	{
		int userID = engine->GetTrackerIDForPlayer( playerIndex );
		if ( userID > 0 )
			return userID;
		return 0;
	}

	// gets the in-game name of another user, returns NULL if that user doesn't exists
	virtual const char *GetPlayerName( int trackerID )
	{
		// find the player by their tracker ID
		int playerSlot = engine->GetPlayerForTrackerID( trackerID );
		if ( !playerSlot )
			return NULL;

		hud_player_info_t playerInfo;
		engine->pfnGetPlayerInfo( playerSlot, &playerInfo );
		return playerInfo.name;
	}

	// returns the engine build number and mod version string for server versioning
	virtual unsigned int GetEngineBuildNumber()
	{
		return 0;
	}

	virtual const char *GetProductVersionString()
	{
		return NULL;
	}

	// gets the friends name of a player
	virtual const char *GetPlayerFriendsName( int friendsID )
	{
		return NULL;
	}

	virtual unsigned int GetPlayerFriendsID( int playerIndex )
	{
		return 0;
	}
};

EXPOSE_SINGLE_INTERFACE( CRunGameEngine, IRunGameEngine, RUNGAMEENGINE_INTERFACE_VERSION );
