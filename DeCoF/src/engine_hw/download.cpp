//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============
//
// Purpose: downloading the files of a server over HTTP (sv_downloadurl), with a
//			cache of partial downloads in cache/DownloadCache.db
//
// $NoKeywords: $
//=============================================================================

#include <windows.h>
#include <wininet.h>
#include <string.h>
#include <stdio.h>

#include "interface.h"
#include "FileSystem.h"
#include "UtlVector.h"
#include <vgui/KeyValues.h>

extern "C"
{
#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "cvar.h"
#include "cmd.h"
#include "client.h"
#include "net.h"
#include "vgui_int.h"
}

#include "download_internal_guess.h"

extern IFileSystem *g_pFileSystem;

char *SharedVarArgs( char *format, ... );

static const char *CacheDirectory_guess = "cache";
static const char *CacheFilename_guess = "cache/DownloadCache.db";

//--------------------------------------------------------------------------------------------------------------
inline char *CloneString_guess( const char *str )
{
	if ( !str )
	{
		char *cloneStr = new char[1];
		cloneStr[0] = '\0';
		return cloneStr;
	}

	char *cloneStr = new char[ strlen( str ) + 1 ];
	strcpy( cloneStr, str );
	return cloneStr;
}

//--------------------------------------------------------------------------------------------------------------
// keeps partial downloads between sessions
//--------------------------------------------------------------------------------------------------------------
class DownloadCache_guess
{
public:
	DownloadCache_guess();
	~DownloadCache_guess();
	void Init_guess();

	void GetCachedData_guess( RequestContext *rc );			// Loads cached data, if any
	void PersistToDisk_guess( const RequestContext *rc );		// Writes out a completed download to disk
	void PersistToCache_guess( const RequestContext *rc );		// Writes out a partial download to the cache

private:
	KeyValues *m_cache_guess;

	void GetCacheFilename_guess( const RequestContext *rc, char cachePath[_MAX_PATH] );
	void GenerateCacheFilename_guess( const RequestContext *rc, char cachePath[_MAX_PATH] );

	void BuildKeyNames_guess( const char *gamePath );	// builds the keys that index into m_cache
	char m_cachefileKey_guess[256 + 64];
	char m_timestampKey_guess[256 + 64];
};

static DownloadCache_guess *TheDownloadCache_guess = NULL;

//--------------------------------------------------------------------------------------------------------------
DownloadCache_guess::DownloadCache_guess()
{
	m_cache_guess = NULL;
}

//--------------------------------------------------------------------------------------------------------------
DownloadCache_guess::~DownloadCache_guess()
{
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache_guess::BuildKeyNames_guess( const char *gamePath )
{
	if ( !gamePath )
	{
		m_cachefileKey_guess[0] = 0;
		m_timestampKey_guess[0] = 0;
		return;
	}

	char *tmpGamePath = CloneString_guess( gamePath );
	char *tmp = tmpGamePath;
	while ( *tmp )
	{
		if ( *tmp == '/' || *tmp == '\\' )
		{
			*tmp = '_';
		}
		++tmp;
	}
	_snprintf( m_cachefileKey_guess, sizeof( m_cachefileKey_guess ), "cachefile_%s", tmpGamePath );
	_snprintf( m_timestampKey_guess, sizeof( m_timestampKey_guess ), "timestamp_%s", tmpGamePath );

	delete[] tmpGamePath;
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache_guess::Init_guess()
{
	if ( m_cache_guess )
	{
		m_cache_guess->deleteThis();
	}

	m_cache_guess = new KeyValues( "DownloadCache" );
	m_cache_guess->LoadFromFile( g_pFileSystem, CacheFilename_guess, NULL );
	FS_CreateDirHierarchy( CacheDirectory_guess, NULL );
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache_guess::GetCachedData_guess( RequestContext *rc )
{
	if ( !m_cache_guess )
		return;

	char cachePath[_MAX_PATH];
	FileHandle_t fp;
	GetCacheFilename_guess( rc, cachePath );

	if ( *cachePath )
	{
		fp = FS_Open( cachePath, "rb" );
		if ( fp )
		{
			unsigned int size = FS_Size( fp );
			rc->cacheData_guess = new unsigned char[size];
			int status = FS_Read( rc->cacheData_guess, size, 1, fp );
			FS_Close( fp );
			if ( !status )
			{
				delete[] rc->cacheData_guess;
				rc->cacheData_guess = NULL;
			}
			else
			{
				BuildKeyNames_guess( rc->gamePath );
				rc->nBytesCached_guess = size;
				strncpy( rc->cachedTimestamp_guess, m_cache_guess->GetString( m_timestampKey_guess, "" ), 256 );
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache_guess::PersistToDisk_guess( const RequestContext *rc )
{
	if ( !m_cache_guess )
		return;

	if ( rc && rc->data_guess && rc->nBytesTotal_guess && !FS_FileExists( rc->gamePath ) )
	{
		char *gamePath = CloneString_guess( rc->gamePath );
		COM_CreatePath( gamePath );
		delete[] gamePath;

		FileHandle_t fp = FS_Open( rc->gamePath, "wb" );
		if ( fp )
		{
			FS_Write( rc->data_guess, rc->nBytesTotal_guess, 1, fp );
			FS_Close( fp );

			// write succeeded. remove any old data from the cache.
			char cachePath[_MAX_PATH];
			GetCacheFilename_guess( rc, cachePath );
			if ( cachePath[0] )
			{
				FS_RemoveFile( cachePath, NULL );
			}

			BuildKeyNames_guess( rc->gamePath );
			KeyValues *kv = m_cache_guess->FindKey( m_cachefileKey_guess, false );
			if ( kv )
			{
				m_cache_guess->RemoveSubKey( kv );
			}
			kv = m_cache_guess->FindKey( m_timestampKey_guess, false );
			if ( kv )
			{
				m_cache_guess->RemoveSubKey( kv );
			}
		}
	}

	m_cache_guess->SaveToFile( g_pFileSystem, CacheFilename_guess, NULL );
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache_guess::PersistToCache_guess( const RequestContext *rc )
{
	if ( !m_cache_guess || !rc || !rc->data_guess || !rc->nBytesTotal_guess || !rc->nBytesCurrent_guess )
		return;

	char cachePath[_MAX_PATH];
	GenerateCacheFilename_guess( rc, cachePath );

	FileHandle_t fp = FS_Open( cachePath, "wb" );
	if ( fp )
	{
		FS_Write( rc->data_guess, rc->nBytesCurrent_guess, 1, fp );
		FS_Close( fp );

		m_cache_guess->SaveToFile( g_pFileSystem, CacheFilename_guess, NULL );
	}
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache_guess::GetCacheFilename_guess( const RequestContext *rc, char cachePath[_MAX_PATH] )
{
	BuildKeyNames_guess( rc->gamePath );
	const char *path = m_cache_guess->GetString( m_cachefileKey_guess, NULL );
	if ( !path || strncmp( path, CacheDirectory_guess, strlen( CacheDirectory_guess ) ) )
	{
		cachePath[0] = 0;
		return;
	}
	strncpy( cachePath, path, _MAX_PATH );
	cachePath[_MAX_PATH - 1] = 0;
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache_guess::GenerateCacheFilename_guess( const RequestContext *rc, char cachePath[_MAX_PATH] )
{
	GetCacheFilename_guess( rc, cachePath );
	BuildKeyNames_guess( rc->gamePath );

	m_cache_guess->SetString( m_timestampKey_guess, rc->cachedTimestamp_guess );

	if ( !*cachePath )
	{
		const char *lastSlash = strrchr( rc->gamePath, '/' );
		const char *lastBackslash = strrchr( rc->gamePath, '\\' );
		const char *gameFilename = rc->gamePath;
		if ( lastSlash || lastBackslash )
		{
			gameFilename = max( lastSlash, lastBackslash ) + 1;
		}
		for ( int i = 0; i < 1000; ++i )
		{
			_snprintf( cachePath, _MAX_PATH, "%s/%s%4.4d", CacheDirectory_guess, gameFilename, i );
			if ( !FS_FileExists( cachePath ) )
			{
				m_cache_guess->SetString( m_cachefileKey_guess, cachePath );
				return;
			}
		}

		// all 1000 were invalid
		_snprintf( cachePath, _MAX_PATH, "%s/overflow", CacheDirectory_guess );
		m_cache_guess->SetString( m_cachefileKey_guess, cachePath );
	}
}

//--------------------------------------------------------------------------------------------------------------
typedef CUtlVector< char * > StrVector;

class DownloadManager
{
public:
	DownloadManager();
	~DownloadManager();

	void Queue( const char *baseURL, const char *gamePath );
	void Stop() { Reset(); }
	int GetQueueSize() { return m_queuedRequests.Count(); }

	void Update();

	bool HasMapBeenDownloadedFromServer( const char *serverMapName );
	void MarkMapAsDownloadedFromServer( const char *serverMapName );

private:
	void Reset();
	void PruneCompletedRequests_guess();
	void CheckActiveDownload();
	void StartNewDownload();

	typedef CUtlVector< RequestContext * > RequestVector;
	RequestVector m_queuedRequests;			// these are requests waiting to be processed
	RequestContext *m_activeRequest;		// this is the active request being processed in another thread
	RequestVector m_completedRequests_guess;	// these are waiting for the thread to exit

	int m_lastPercent;						// last percent value the progress bar was updated with
	int m_totalRequests;					// Total number of requests (used to set the top progress bar)

	StrVector m_downloadedMaps;				// List of maps for which we have already tried to download assets.
};

static DownloadManager TheDownloadManager;

//--------------------------------------------------------------------------------------------------------------
DownloadManager::DownloadManager()
{
	m_activeRequest = NULL;
	m_lastPercent = 0;
	m_totalRequests = 0;
}

//--------------------------------------------------------------------------------------------------------------
DownloadManager::~DownloadManager()
{
	Reset();

	for ( int i = 0; i < m_downloadedMaps.Count(); ++i )
	{
		delete[] m_downloadedMaps[i];
	}
	m_downloadedMaps.RemoveAll();
}

//--------------------------------------------------------------------------------------------------------------
bool DownloadManager::HasMapBeenDownloadedFromServer( const char *serverMapName )
{
	if ( !serverMapName )
		return false;

	for ( int i = 0; i < m_downloadedMaps.Count(); ++i )
	{
		const char *oldServerMapName = m_downloadedMaps[i];
		if ( oldServerMapName && !stricmp( serverMapName, oldServerMapName ) )
		{
			return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
void DownloadManager::MarkMapAsDownloadedFromServer( const char *serverMapName )
{
	if ( !serverMapName )
		return;

	if ( HasMapBeenDownloadedFromServer( serverMapName ) )
		return;

	m_downloadedMaps.AddToTail( CloneString_guess( serverMapName ) );
}

//--------------------------------------------------------------------------------------------------------------
void DownloadManager::Queue( const char *baseURL, const char *gamePath )
{
	if ( cls.servername && !strcmp( "local", cls.servername ) )
		return;

	++m_totalRequests;

	if ( !TheDownloadCache_guess )
	{
		TheDownloadCache_guess = new DownloadCache_guess;
		TheDownloadCache_guess->Init_guess();
	}

	RequestContext *rc = new RequestContext;
	m_queuedRequests.AddToTail( rc );

	memset( rc, 0, sizeof( RequestContext ) );

	rc->status = HTTP_CONNECTING;

	_snprintf( rc->baseURL, 256, "%s", baseURL );
	_snprintf( rc->gamePath, 256, "%s", gamePath );
	_snprintf( rc->serverURL, 256, "%s", NET_AdrToString( cls.netchan.remote_address ) );
}

//--------------------------------------------------------------------------------------------------------------
void DownloadManager::Reset()
{
	// ask the active request to bail
	if ( m_activeRequest )
	{
		Con_DPrintf( "Aborting download of %s\n", m_activeRequest->gamePath );
		if ( m_activeRequest->nBytesTotal_guess && m_activeRequest->nBytesCurrent_guess )
		{
			// Persist any data we've got to cache
			TheDownloadCache_guess->PersistToCache_guess( m_activeRequest );
		}
		m_activeRequest->shouldStop_guess = true;
		m_completedRequests_guess.AddToTail( m_activeRequest );
		m_activeRequest = NULL;
		StopLoadingProgressBar();
	}

	// clear out any queued requests
	for ( int i = 0; i < m_queuedRequests.Count(); ++i )
	{
		delete m_queuedRequests[i];
	}
	m_queuedRequests.RemoveAll();

	if ( TheDownloadCache_guess )
	{
		delete TheDownloadCache_guess;
		TheDownloadCache_guess = NULL;
	}

	m_lastPercent = 0;
	m_totalRequests = 0;
}

//--------------------------------------------------------------------------------------------------------------
// frees the requests whose worker thread has finished
//--------------------------------------------------------------------------------------------------------------
void DownloadManager::PruneCompletedRequests_guess()
{
	for ( int i = m_completedRequests_guess.Count() - 1; i >= 0; --i )
	{
		if ( m_completedRequests_guess[i]->threadDone_guess )
		{
			if ( m_completedRequests_guess[i]->cacheData_guess )
			{
				delete[] m_completedRequests_guess[i]->cacheData_guess;
			}
			delete m_completedRequests_guess[i];
			m_completedRequests_guess.Remove( i );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void DownloadManager::CheckActiveDownload()
{
	if ( m_activeRequest )
	{
		// check active request for completion / error / progress update
		switch ( m_activeRequest->status )
		{
		case HTTP_DONE:
			if ( m_activeRequest->nBytesTotal_guess )
			{
				SetSecondaryProgressBarText( m_activeRequest->gamePath );
				TheDownloadCache_guess->PersistToDisk_guess( m_activeRequest );
				m_activeRequest->shouldStop_guess = true;
				m_completedRequests_guess.AddToTail( m_activeRequest );
				m_activeRequest = NULL;
				if ( !m_queuedRequests.Count() )
				{
					StopLoadingProgressBar();
					Cbuf_AddText( "retry\n" );
				}
			}
			break;
		case HTTP_ERROR:
			if ( m_activeRequest )
			{
				m_activeRequest->shouldStop_guess = true;
				m_completedRequests_guess.AddToTail( m_activeRequest );
				m_activeRequest = NULL;
				if ( !m_queuedRequests.Count() )
				{
					StopLoadingProgressBar();
					Cbuf_AddText( "retry\n" );
				}
			}
			else
			{
				StopLoadingProgressBar();
				Cbuf_AddText( "retry\n" );
				Stop();
			}
			break;
		case HTTP_FETCH:
			SetSecondaryProgressBarText( m_activeRequest->gamePath );
			if ( m_activeRequest->nBytesTotal_guess )
			{
				int percent = ( m_activeRequest->nBytesCurrent_guess * 100 / m_activeRequest->nBytesTotal_guess );
				if ( percent != m_lastPercent )
				{
					m_lastPercent = percent;
					SetSecondaryProgressBar( m_lastPercent * 0.01f );
				}
			}
			break;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void DownloadManager::StartNewDownload()
{
	if ( m_activeRequest || !m_queuedRequests.Count() )
		return;

	// Remove one request from the queue and make it active
	m_activeRequest = m_queuedRequests[0];
	m_queuedRequests.Remove( 0 );

	// Check the cache
	TheDownloadCache_guess->GetCachedData_guess( m_activeRequest );

	// Update the UI
	ContinueLoadingProgressBar( "Http", m_totalRequests - m_queuedRequests.Count(), 0.0f );
	SetLoadingProgressBarStatusText( "#GameUI_VerifyingAndDownloading" );
	SetSecondaryProgressBarText( m_activeRequest->gamePath );
	SetSecondaryProgressBar( 0.0f );

	// Initiate the download
	Con_DPrintf( "Requesting HTTP download of %s%s.\n", m_activeRequest->baseURL, m_activeRequest->gamePath );

	m_lastPercent = 0;

	DWORD threadID;
	CreateThread( NULL, 0, DownloadThread_guess, m_activeRequest, 0, &threadID );
}

//--------------------------------------------------------------------------------------------------------------
void DownloadManager::Update()
{
	PruneCompletedRequests_guess();
	CheckActiveDownload();
	StartNewDownload();
}

//--------------------------------------------------------------------------------------------------------------
extern "C"
{

void CL_HTTPUpdate( void )
{
	TheDownloadManager.Update();
}

void CL_HTTPStop_f( void )
{
	TheDownloadManager.Stop();
}

void CL_QueueHTTPDownload( const char *filename )
{
	TheDownloadManager.Queue( cl.downloadUrl, filename );
}

int CL_GetDownloadQueueSize( void )
{
	return TheDownloadManager.GetQueueSize();
}

int CL_CanUseHTTPDownload( void )
{
	if ( cl.downloadUrl[0] )
	{
		const char *serverMapName = SharedVarArgs( "%s:%s", cls.servername, cl.levelname );
		return !TheDownloadManager.HasMapBeenDownloadedFromServer( serverMapName );
	}

	return 0;
}

void CL_MarkMapAsUsingHTTPDownload( void )
{
	const char *serverMapName = SharedVarArgs( "%s:%s", cls.servername, cl.levelname );
	TheDownloadManager.MarkMapAsDownloadedFromServer( serverMapName );
}

}
