//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============
//
// Purpose: the WinINet worker thread of an HTTP download (sv_downloadurl)
//
// No build has symbols for this file: every name below the RequestContext
// fields is a guess.
//
// $NoKeywords: $
//=============================================================================

#include <windows.h>
#include <wininet.h>
#include <string.h>
#include <stdio.h>

#include "download_internal_guess.h"

#ifndef min
#define min(a,b)	(((a) < (b)) ? (a) : (b))
#endif

//--------------------------------------------------------------------------------------------------------------
// Debug output of the worker thread (compiled out)
//--------------------------------------------------------------------------------------------------------------
void Thread_DPrintf_guess( char *fmt, ... )
{
}

//--------------------------------------------------------------------------------------------------------------
// Records the WinINet progress of a request
//--------------------------------------------------------------------------------------------------------------
void CALLBACK DownloadStatusCallback_guess( HINTERNET hOpenResource, DWORD dwContext, DWORD dwInternetStatus, LPVOID lpStatusInfo, DWORD dwStatusInfoLength )
{
	RequestContext *rc = (RequestContext *)lpStatusInfo;

	switch ( dwInternetStatus )
	{
	case INTERNET_STATUS_RESOLVING_NAME:
	case INTERNET_STATUS_NAME_RESOLVED:
	case INTERNET_STATUS_CONNECTING_TO_SERVER:
	case INTERNET_STATUS_CONNECTED_TO_SERVER:
	case INTERNET_STATUS_SENDING_REQUEST:
	case INTERNET_STATUS_REQUEST_SENT:
	case INTERNET_STATUS_CLOSING_CONNECTION:
	case INTERNET_STATUS_CONNECTION_CLOSED:
	case INTERNET_STATUS_REQUEST_COMPLETE:
		if ( rc )
		{
			rc->internetStatus_guess = dwInternetStatus;
		}
		break;
	}
}

//--------------------------------------------------------------------------------------------------------------
// Reads the body of the response into rc->data_guess
//--------------------------------------------------------------------------------------------------------------
void ReadData_guess( RequestContext *rc )
{
	const int bufferSize_guess = 2048;
	unsigned char buf_guess[ bufferSize_guess ];
	DWORD dwBytesRead_guess = 0;

	if ( !rc->nBytesTotal_guess )
	{
		rc->status = HTTP_ERROR;
		rc->error = HTTP_ERROR_ZERO_LENGTH_FILE;
		return;
	}

	rc->nBytesCurrent_guess = rc->nBytesCached_guess;
	rc->status = HTTP_FETCH;

	while ( !rc->shouldStop_guess )
	{
		if ( !InternetReadFile( rc->hDataResource_guess, buf_guess, bufferSize_guess, &dwBytesRead_guess ) )
		{
			rc->status = HTTP_ERROR;
			rc->error = HTTP_ERROR_CONNECTION_CLOSED;
			return;
		}

		if ( !dwBytesRead_guess )
		{
			rc->status = HTTP_DONE;
			return;
		}

		DWORD dwBytesToCopy_guess = min( rc->nBytesTotal_guess - rc->nBytesCurrent_guess, dwBytesRead_guess );
		if ( dwBytesToCopy_guess != dwBytesRead_guess )
		{
			Thread_DPrintf_guess( "Warning - read more data than expected!\n" );
		}

		if ( rc->data_guess && dwBytesToCopy_guess > 0 )
		{
			memcpy( rc->data_guess + rc->nBytesCurrent_guess, buf_guess, dwBytesToCopy_guess );
		}

		rc->nBytesCurrent_guess += dwBytesToCopy_guess;
	}

	rc->status = HTTP_ABORTED;
}

//--------------------------------------------------------------------------------------------------------------
// Closes the WinINet handles, waits for the main thread and marks the request done
//--------------------------------------------------------------------------------------------------------------
void CleanUpDownload_guess( RequestContext *rc, HTTPStatus status, HTTPError error )
{
	rc->status = status;
	rc->error = error;

	if ( rc->hDataResource_guess && !InternetCloseHandle( rc->hDataResource_guess ) )
	{
		Thread_DPrintf_guess( "Failed to close data resource for %s%s\n", rc->baseURL, rc->gamePath );
	}
	else if ( rc->hOpenResource && !InternetCloseHandle( rc->hOpenResource ) )
	{
		Thread_DPrintf_guess( "Failed to close open resource for %s%s\n", rc->baseURL, rc->gamePath );
	}

	rc->hDataResource_guess = NULL;
	rc->hOpenResource = NULL;

	// wait until the main thread has taken the result
	while ( !rc->shouldStop_guess )
	{
		Sleep( 100 );
	}

	if ( rc->data_guess )
	{
		delete[] rc->data_guess;
		rc->data_guess = NULL;
	}

	rc->threadDone_guess = true;
}

//--------------------------------------------------------------------------------------------------------------
// (empty in this build, nothing calls it)
//--------------------------------------------------------------------------------------------------------------
void DownloadThreadUnused_guess( void )
{
}

//--------------------------------------------------------------------------------------------------------------
// Fetches rc->baseURL + rc->gamePath over HTTP, resuming a cached partial download
//--------------------------------------------------------------------------------------------------------------
DWORD __stdcall DownloadThread_guess( void *voidPtr )
{
	RequestContext *pRequest_guess = (RequestContext *)voidPtr;

	URL_COMPONENTS urlParts_guess;
	char parts_guess[ 6 ][ 256 ];

	urlParts_guess.dwStructSize = sizeof( urlParts_guess );

	urlParts_guess.dwSchemeLength = 256;
	urlParts_guess.dwHostNameLength = 256;
	urlParts_guess.dwUserNameLength = 256;
	urlParts_guess.dwPasswordLength = 256;
	urlParts_guess.dwUrlPathLength = 256;
	urlParts_guess.dwExtraInfoLength = 256;

	urlParts_guess.lpszScheme = parts_guess[ 0 ];
	urlParts_guess.lpszHostName = parts_guess[ 1 ];
	urlParts_guess.lpszUserName = parts_guess[ 2 ];
	urlParts_guess.lpszPassword = parts_guess[ 3 ];
	urlParts_guess.lpszUrlPath = parts_guess[ 4 ];
	urlParts_guess.lpszExtraInfo = parts_guess[ 5 ];

	DWORD maxUrl_guess = 512;
	char fullURL_guess[ 512 ];
	_snprintf( fullURL_guess, maxUrl_guess, "%s%s", pRequest_guess->baseURL, pRequest_guess->gamePath, fullURL_guess );

	if ( !InternetCrackUrl( fullURL_guess, maxUrl_guess, 0, &urlParts_guess ) )
	{
		CleanUpDownload_guess( pRequest_guess, HTTP_ERROR, HTTP_ERROR_INVALID_URL );
		return pRequest_guess->status;
	}

	if ( urlParts_guess.nScheme != INTERNET_SCHEME_HTTP && urlParts_guess.nScheme != INTERNET_SCHEME_HTTPS )
	{
		CleanUpDownload_guess( pRequest_guess, HTTP_ERROR, HTTP_ERROR_INVALID_PROTOCOL );
		return pRequest_guess->status;
	}

	pRequest_guess->hOpenResource = InternetOpen( "Half-Life", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0 );
	if ( !pRequest_guess->hOpenResource )
	{
		CleanUpDownload_guess( pRequest_guess, HTTP_ERROR, HTTP_ERROR_CANT_BIND_SOCKET );
		return pRequest_guess->status;
	}

	InternetSetStatusCallback( pRequest_guess->hOpenResource, (INTERNET_STATUS_CALLBACK)DownloadStatusCallback_guess );

	if ( pRequest_guess->shouldStop_guess )
	{
		CleanUpDownload_guess( pRequest_guess, HTTP_ABORTED, HTTP_ERROR_NONE );
		return pRequest_guess->status;
	}

	const char *accept_guess[ 2 ] = { "*/*", NULL };

	DWORD flags_guess = 0;
	flags_guess |= INTERNET_FLAG_RELOAD;
	flags_guess |= INTERNET_FLAG_NO_CACHE_WRITE;
	flags_guess |= INTERNET_FLAG_KEEP_CONNECTION;

	if ( urlParts_guess.nScheme == INTERNET_SCHEME_HTTPS )
	{
		flags_guess |= INTERNET_FLAG_SECURE;
		flags_guess |= INTERNET_FLAG_IGNORE_CERT_CN_INVALID;
		flags_guess |= INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
	}

	char hdrs_guess[ 256 ] = "";
	DWORD headersLength_guess = 0;
	char *pszHeaders_guess = NULL;

	if ( *pRequest_guess->cachedTimestamp_guess && pRequest_guess->nBytesCached_guess )
	{
		// resume the cached partial download
		if ( *pRequest_guess->serverURL )
		{
			_snprintf( hdrs_guess, 256, "If-Range: %s\nRange: bytes=%d-\nReferer: hl1://%s\n", pRequest_guess->cachedTimestamp_guess, pRequest_guess->nBytesCached_guess, pRequest_guess->serverURL );
		}
		else
		{
			_snprintf( hdrs_guess, 256, "If-Range: %s\nRange: bytes=%d-\n", pRequest_guess->cachedTimestamp_guess, pRequest_guess->nBytesCached_guess );
		}

		pszHeaders_guess = hdrs_guess;
		headersLength_guess = (DWORD)-1;
	}
	else if ( *pRequest_guess->serverURL )
	{
		_snprintf( hdrs_guess, 256, "Referer: hl1://%s\n", pRequest_guess->serverURL );
	}

	pRequest_guess->hDataResource_guess = InternetOpenUrl( pRequest_guess->hOpenResource, fullURL_guess, pszHeaders_guess, headersLength_guess, flags_guess, (DWORD)pRequest_guess );
	if ( !pRequest_guess->hDataResource_guess )
	{
		CleanUpDownload_guess( pRequest_guess, HTTP_ERROR, HTTP_ERROR_CANT_CONNECT );
		return pRequest_guess->status;
	}

	if ( pRequest_guess->shouldStop_guess )
	{
		CleanUpDownload_guess( pRequest_guess, HTTP_ABORTED, HTTP_ERROR_NONE );
		return pRequest_guess->status;
	}

	DWORD result_guess;
	DWORD dwSize_guess = sizeof( result_guess );
	if ( !HttpQueryInfo( pRequest_guess->hDataResource_guess, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &result_guess, &dwSize_guess, NULL ) )
	{
		CleanUpDownload_guess( pRequest_guess, HTTP_ERROR, HTTP_ERROR_NO_HEADERS );
		return pRequest_guess->status;
	}

	if ( result_guess != HTTP_STATUS_OK && result_guess != HTTP_STATUS_PARTIAL_CONTENT )
	{
		CleanUpDownload_guess( pRequest_guess, HTTP_ERROR, HTTP_ERROR_FILE_NONEXISTENT );
		return pRequest_guess->status;
	}

	dwSize_guess = 256;
	if ( !HttpQueryInfo( pRequest_guess->hDataResource_guess, HTTP_QUERY_LAST_MODIFIED, pRequest_guess->cachedTimestamp_guess, &dwSize_guess, NULL ) )
	{
		pRequest_guess->cachedTimestamp_guess[ 0 ] = '\0';
	}
	pRequest_guess->cachedTimestamp_guess[ 255 ] = '\0';

	if ( pRequest_guess->nBytesCached_guess )
	{
		// resuming a cached partial download (debug output compiled out)
	}

	if ( result_guess != HTTP_STATUS_PARTIAL_CONTENT )
	{
		// the server sends the whole file
		pRequest_guess->nBytesCached_guess = 0;
	}

	dwSize_guess = sizeof( result_guess );
	if ( HttpQueryInfo( pRequest_guess->hDataResource_guess, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &result_guess, &dwSize_guess, NULL ) )
	{
		pRequest_guess->nBytesTotal_guess = result_guess + pRequest_guess->nBytesCached_guess;
		if ( result_guess > 0 )
		{
			pRequest_guess->data_guess = new unsigned char[ pRequest_guess->nBytesTotal_guess ];
		}
	}
	else
	{
		CleanUpDownload_guess( pRequest_guess, HTTP_ERROR, HTTP_ERROR_ZERO_LENGTH_FILE );
		return pRequest_guess->status;
	}

	if ( pRequest_guess->cacheData_guess && pRequest_guess->nBytesCached_guess )
	{
		DWORD amountToCopy_guess = min( pRequest_guess->nBytesCached_guess, pRequest_guess->nBytesTotal_guess );
		memcpy( pRequest_guess->data_guess, pRequest_guess->cacheData_guess, amountToCopy_guess );
	}

	if ( pRequest_guess->shouldStop_guess )
	{
		CleanUpDownload_guess( pRequest_guess, HTTP_ABORTED, HTTP_ERROR_NONE );
		return pRequest_guess->status;
	}

	ReadData_guess( pRequest_guess );

	CleanUpDownload_guess( pRequest_guess, pRequest_guess->status, HTTP_ERROR_NONE );
	return pRequest_guess->status;
}
