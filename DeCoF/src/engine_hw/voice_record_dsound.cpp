//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: microphone input through DirectSoundCapture
//
// $NoKeywords: $
//=============================================================================

#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>

extern "C"
{
void	*FS_LoadLibrary( const char *dllName );
}

#include "ivoicerecord.h"

typedef HRESULT (WINAPI *DirectSoundCaptureCreateFn)( const GUID FAR *lpGUID, LPDIRECTSOUNDCAPTURE *lplpDSC, LPUNKNOWN pUnkOuter );


// ------------------------------------------------------------------------------
// VoiceRecord_DSound
// ------------------------------------------------------------------------------

class VoiceRecord_DSound : public IVoiceRecord
{
protected:

	virtual				~VoiceRecord_DSound();


// IVoiceRecord.
public:

						VoiceRecord_DSound();
	virtual void		Release();

	virtual bool		RecordStart();
	virtual void		RecordStop();

	// Initialize. The format of the data we expect from the provider is
	// 16-bit signed mono at the specified sample rate.
	virtual bool		Init( int sampleRate );

	virtual void		Idle();

	// Get the most recent N samples.
	virtual int			GetRecordedData( short *pOut, int nSamplesWanted );


private:
	void				Term_guess();		// Delete internal objects.
	void				Clear_guess();		// Clear members.
	void				UpdateWrapping_guess();

	inline DWORD		NumCaptureBufferBytes_guess() { return m_nCaptureBufferBytes_guess; }


private:
	HINSTANCE					m_hInstDS_guess;
	LPDIRECTSOUNDCAPTURE		m_pCapture_guess;
	LPDIRECTSOUNDCAPTUREBUFFER	m_pCaptureBuffer_guess;

	// How many bytes our capture buffer has.
	DWORD						m_nCaptureBufferBytes_guess;

	// We need to know when the capture buffer loops, so the main thread
	// sets this event and we watch it.
	DWORD						m_WrapOffset_guess;
	HANDLE						m_hWrapEvent_guess;

	// This is our (unwrapped) position that tells how much data we've given to the app.
	DWORD						m_LastReadPos_guess;

	DWORD						m_Unused_guess;
};


VoiceRecord_DSound::VoiceRecord_DSound()
{
	Clear_guess();
}


VoiceRecord_DSound::~VoiceRecord_DSound()
{
	Term_guess();
}


void VoiceRecord_DSound::Release()
{
	delete this;
}


bool VoiceRecord_DSound::RecordStart()
{
	return true;
}


void VoiceRecord_DSound::RecordStop()
{
}


bool VoiceRecord_DSound::Init( int sampleRate )
{
	HRESULT hr;
	DSCBUFFERDESC dscDesc;
	DirectSoundCaptureCreateFn createFn;


	Term_guess();


	WAVEFORMATEX recordFormat =
	{
		WAVE_FORMAT_PCM,		// wFormatTag
		1,						// nChannels
		sampleRate,				// nSamplesPerSec
		sampleRate * 2,			// nAvgBytesPerSec
		2,						// nBlockAlign
		16,						// wBitsPerSample
		sizeof( WAVEFORMATEX )	// cbSize
	};



	// Load the DSound DLL.
	m_hInstDS_guess = (HINSTANCE)FS_LoadLibrary( "dsound.dll" );
	if ( !m_hInstDS_guess )
		goto HandleError;

	createFn = (DirectSoundCaptureCreateFn)GetProcAddress( m_hInstDS_guess, "DirectSoundCaptureCreate" );
	if ( !createFn )
		goto HandleError;

	hr = createFn( NULL, &m_pCapture_guess, NULL );
	if ( FAILED( hr ) )
		goto HandleError;

	// Create the capture buffer.
	memset( &dscDesc, 0, sizeof( dscDesc ) );
	dscDesc.dwSize = sizeof( dscDesc );
	dscDesc.dwFlags = 0;
	dscDesc.dwBufferBytes = recordFormat.nAvgBytesPerSec;
	dscDesc.lpwfxFormat = &recordFormat;

	hr = m_pCapture_guess->CreateCaptureBuffer( &dscDesc, &m_pCaptureBuffer_guess, NULL );
	if ( FAILED( hr ) )
		goto HandleError;


	// Figure out how many bytes we got in our capture buffer.
	DSCBCAPS caps;
	memset( &caps, 0, sizeof( caps ) );
	caps.dwSize = sizeof( caps );

	hr = m_pCaptureBuffer_guess->GetCaps( &caps );
	if ( FAILED( hr ) )
		goto HandleError;

	m_nCaptureBufferBytes_guess = caps.dwBufferBytes;


	// Set it up so we get notification when the buffer wraps.
	m_hWrapEvent_guess = CreateEvent( NULL, FALSE, FALSE, NULL );
	if ( !m_hWrapEvent_guess )
		goto HandleError;

	DSBPOSITIONNOTIFY dsbNotify;
	dsbNotify.dwOffset = dscDesc.dwBufferBytes - 1;
	dsbNotify.hEventNotify = m_hWrapEvent_guess;

	// Get the IDirectSoundNotify interface.
	LPDIRECTSOUNDNOTIFY pNotify;
	hr = m_pCaptureBuffer_guess->QueryInterface( IID_IDirectSoundNotify, (void **)&pNotify );
	if ( FAILED( hr ) )
		goto HandleError;

	hr = pNotify->SetNotificationPositions( 1, &dsbNotify );
	pNotify->Release();
	if ( FAILED( hr ) )
		goto HandleError;

	// Start capturing.
	hr = m_pCaptureBuffer_guess->Start( DSCBSTART_LOOPING );
	if ( FAILED( hr ) )
		return false;

	return true;


HandleError:;
	Term_guess();
	return false;
}


void VoiceRecord_DSound::Term_guess()
{
	if ( m_pCaptureBuffer_guess )
		m_pCaptureBuffer_guess->Release();

	if ( m_pCapture_guess )
		m_pCapture_guess->Release();

	if ( m_hWrapEvent_guess )
		DeleteObject( m_hWrapEvent_guess );

	if ( m_hInstDS_guess )
	{
		FreeLibrary( m_hInstDS_guess );
		m_hInstDS_guess = NULL;
	}

	Clear_guess();
}


void VoiceRecord_DSound::Clear_guess()
{
	m_pCapture_guess = NULL;
	m_pCaptureBuffer_guess = NULL;
	m_WrapOffset_guess = 0;
	m_LastReadPos_guess = 0;
	m_hWrapEvent_guess = NULL;
	m_hInstDS_guess = NULL;
}


void VoiceRecord_DSound::Idle()
{
	UpdateWrapping_guess();
}


int VoiceRecord_DSound::GetRecordedData( short *pOut, int nSamples )
{
	if ( !m_pCaptureBuffer_guess )
		return 0;

	DWORD dwStatus;
	HRESULT hr = m_pCaptureBuffer_guess->GetStatus( &dwStatus );
	if ( FAILED( hr ) || !( dwStatus & DSCBSTATUS_CAPTURING ) )
		return 0;

	Idle(); // Update wrapping..

	DWORD dwReadPos;
	DWORD dwWantedBytes_guess = (DWORD)nSamples;

	DWORD dwCapturePos;
	hr = m_pCaptureBuffer_guess->GetCurrentPosition( &dwCapturePos, &dwReadPos );
	if ( FAILED( hr ) )
		return 0;

	dwCapturePos += m_WrapOffset_guess;
	dwReadPos += m_WrapOffset_guess;

	// Read the range from m_LastReadPos to dwReadPos.
	DWORD dwStartPos;
	if ( dwReadPos > dwWantedBytes_guess )
		dwStartPos = dwReadPos - dwWantedBytes_guess;
	else
		dwStartPos = 0;

	if ( dwStartPos < m_LastReadPos_guess )
		dwStartPos = m_LastReadPos_guess;

	void *pData[2];
	DWORD dataLen[2];

	hr = m_pCaptureBuffer_guess->Lock(
		dwStartPos % NumCaptureBufferBytes_guess(),
		dwReadPos - dwStartPos,
		&pData[0], &dataLen[0],
		&pData[1], &dataLen[1],
		0 );

	if ( FAILED( hr ) )
		return 0;

	// Hopefully we didn't wrap around and get stuck on the same section twice.
	if ( dataLen[0] + dataLen[1] > dwWantedBytes_guess )
	{
		m_pCaptureBuffer_guess->Unlock( pData[0], dataLen[0], pData[1], dataLen[1] );
		return 0;
	}

	memcpy( pOut, pData[0], dataLen[0] );
	memcpy( &pOut[dataLen[0] / 2], pData[1], dataLen[1] );

	m_pCaptureBuffer_guess->Unlock( pData[0], dataLen[0], pData[1], dataLen[1] );

	m_LastReadPos_guess = dwReadPos;
	return ( dataLen[0] + dataLen[1] ) >> 1;
}


void VoiceRecord_DSound::UpdateWrapping_guess()
{
	if ( !m_pCaptureBuffer_guess )
		return;

	// Has the buffer wrapped?
	if ( WaitForSingleObject( m_hWrapEvent_guess, 0 ) == WAIT_OBJECT_0 )
	{
		m_WrapOffset_guess += m_nCaptureBufferBytes_guess;
	}
}


IVoiceRecord *CreateVoiceRecord_DSound_guess( int sampleRate )
{
	VoiceRecord_DSound *pRecord = new VoiceRecord_DSound;
	if ( pRecord && pRecord->Init( sampleRate ) )
	{
		return pRecord;
	}
	else
	{
		if ( pRecord )
			pRecord->Release();

		return NULL;
	}
}
