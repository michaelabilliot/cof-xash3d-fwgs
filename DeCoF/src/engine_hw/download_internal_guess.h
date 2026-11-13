// download_internal_guess.h -- the state of one HTTP download, shared by download.cpp and
// the worker thread in downloadthread.cpp

#ifndef DOWNLOAD_INTERNAL_GUESS_H
#define DOWNLOAD_INTERNAL_GUESS_H

//--------------------------------------------------------------------------------------------------------------
// the state of one download, shared with the worker thread
//--------------------------------------------------------------------------------------------------------------
enum HTTPStatus
{
	HTTP_CONNECTING = 0,
	HTTP_FETCH,
	HTTP_DONE,
	HTTP_ABORTED,
	HTTP_ERROR,
};

enum HTTPError
{
	HTTP_ERROR_NONE = 0,
	HTTP_ERROR_ZERO_LENGTH_FILE,
	HTTP_ERROR_CONNECTION_CLOSED,
	HTTP_ERROR_INVALID_URL,
	HTTP_ERROR_INVALID_PROTOCOL,
	HTTP_ERROR_CANT_BIND_SOCKET,
	HTTP_ERROR_CANT_CONNECT,
	HTTP_ERROR_NO_HEADERS,
	HTTP_ERROR_FILE_NONEXISTENT,
	HTTP_ERROR_MAX,
};

typedef struct
{
	bool			shouldStop_guess;			// set by the main thread to stop the worker
	bool			threadDone_guess;			// set by the worker when it is finished
	HTTPStatus		status;
	int				internetStatus_guess;		// written by the WinINet status callback
	HTTPError		error;

	char			baseURL[256];
	char			gamePath[256];
	char			serverURL[256];
	char			cachedTimestamp_guess[256];	// Last-Modified of the cached partial download

	DWORD			nBytesTotal_guess;
	DWORD			nBytesCurrent_guess;
	DWORD			nBytesCached_guess;

	unsigned char	*data_guess;
	unsigned char	*cacheData_guess;

	HINTERNET		hOpenResource;				// InternetOpen
	HINTERNET		hDataResource_guess;		// InternetOpenUrl
} RequestContext;

// the WinINet worker of a download (downloadthread.cpp)
DWORD __stdcall DownloadThread_guess( void *voidPtr );

#endif // DOWNLOAD_INTERNAL_GUESS_H
