//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: microphone input through the waveIn API
//
// $NoKeywords: $
//=============================================================================

#include <windows.h>
#include <mmsystem.h>

extern "C"
{
void	Con_Printf( char *fmt, ... );
void	Con_DPrintf( char *fmt, ... );
}

#include "ivoicerecord.h"
#include "CircularBuffer.h"


// ------------------------------------------------------------------------------
// VoiceRecord_WaveIn
// ------------------------------------------------------------------------------

class VoiceRecord_WaveIn : public IVoiceRecord
{
protected:

	virtual				~VoiceRecord_WaveIn();


// IVoiceRecord.
public:

						VoiceRecord_WaveIn();
	virtual void		Release();

	virtual bool		RecordStart();
	virtual void		RecordStop();

	// Initialize. The format of the data we expect from the provider is
	// 16-bit signed mono at the specified sample rate.
	bool				Init( int sampleRate );

	virtual void		Idle();

	// Get the most recent N samples.
	virtual int			GetRecordedData( short *pOut, int nSamplesWanted );


private:
	void				Term_guess();		// Delete internal objects.

	static void CALLBACK	StaticWaveInCallback_guess( HWAVEIN hwi, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2 );
	void				WaveInCallback_guess( HWAVEIN hwi, UINT uMsg, DWORD dwParam1, DWORD dwParam2 );

	// Hand a buffer back to waveIn.
	BOOL				RecordAddBuffer( int iBuffer );

	// How many buffers waveIn is still filling.
	int					NumOutstandingBuffers_guess();


private:
	enum { NUM_BUFFERS = 15, BUFFER_SIZE = 256 };

	WAVEHDR				m_WaveHdrs_guess[NUM_BUFFERS];
	char				m_Buffers_guess[NUM_BUFFERS][BUFFER_SIZE];

	// The waveIn callback writes into m_Buffer from another thread.
	CRITICAL_SECTION	m_CS_guess;

	HWAVEIN				m_hWaveIn_guess;
	bool				m_bRecording_guess;

	CSizedCircularBuffer<NUM_BUFFERS * BUFFER_SIZE>	m_Buffer_guess;

	DWORD				m_Unused_guess;
};


VoiceRecord_WaveIn::VoiceRecord_WaveIn()
{
	m_hWaveIn_guess = NULL;
	InitializeCriticalSection( &m_CS_guess );
	m_bRecording_guess = false;
}


VoiceRecord_WaveIn::~VoiceRecord_WaveIn()
{
	Term_guess();
	DeleteCriticalSection( &m_CS_guess );
}


void VoiceRecord_WaveIn::Release()
{
	delete this;
}


static void PrintWaveInError_guess( MMRESULT mmr );

bool VoiceRecord_WaveIn::Init( int sampleRate )
{
	WAVEFORMATEX wf_guess =
	{
		WAVE_FORMAT_PCM,		// wFormatTag
		1,						// nChannels
		sampleRate,				// nSamplesPerSec
		sampleRate * 2,			// nAvgBytesPerSec
		2,						// nBlockAlign
		16,						// wBitsPerSample
		sizeof( WAVEFORMATEX )	// cbSize
	};
	MMRESULT	rc_guess;
	int			i_guess;

	rc_guess = waveInOpen( &m_hWaveIn_guess, WAVE_MAPPER, &wf_guess, (DWORD)StaticWaveInCallback_guess, (DWORD)this, CALLBACK_FUNCTION );
	if ( rc_guess != MMSYSERR_NOERROR )
	{
		Con_DPrintf( "ERROR VoiceRecord_WaveIn: waveInOpen\n" );
		PrintWaveInError_guess( rc_guess );
		Term_guess();
		return false;
	}

	for ( i_guess = 0; i_guess < NUM_BUFFERS; i_guess++ )
	{
		memset( &m_WaveHdrs_guess[i_guess], 0, sizeof( WAVEHDR ) );
		m_WaveHdrs_guess[i_guess].lpData = m_Buffers_guess[i_guess];
		m_WaveHdrs_guess[i_guess].dwBufferLength = BUFFER_SIZE;
		m_WaveHdrs_guess[i_guess].dwUser = 1;

		rc_guess = waveInPrepareHeader( m_hWaveIn_guess, &m_WaveHdrs_guess[i_guess], sizeof( WAVEHDR ) );
		if ( rc_guess != MMSYSERR_NOERROR )
		{
			Con_DPrintf( "ERROR VoiceRecord_WaveIn::Init: waveInPrepareHeader\n" );
			PrintWaveInError_guess( rc_guess );
			Term_guess();
			return false;
		}
	}

	rc_guess = waveInStart( m_hWaveIn_guess );
	if ( rc_guess != MMSYSERR_NOERROR )
	{
		Con_DPrintf( "ERROR Voice_RecordStart: waveInStart\n" );
		PrintWaveInError_guess( rc_guess );
		Term_guess();
		return false;
	}

	return true;
}


static void PrintWaveInError_guess( MMRESULT mmr )
{
	char	text_guess[MAX_PATH];

	waveInGetErrorText( mmr, text_guess, sizeof( text_guess ) );
	Con_Printf( "%s\n", text_guess );
}


bool VoiceRecord_WaveIn::RecordStart()
{
	m_bRecording_guess = true;
	Idle();
	return true;
}


void VoiceRecord_WaveIn::RecordStop()
{
	m_bRecording_guess = false;
}


void VoiceRecord_WaveIn::Term_guess()
{
	if ( m_hWaveIn_guess )
	{
		RecordStop();
		waveInReset( m_hWaveIn_guess );
		waveInClose( m_hWaveIn_guess );
		m_hWaveIn_guess = NULL;
	}
}


void VoiceRecord_WaveIn::Idle()
{
	int		i_guess;

	// Give the buffers waveIn is done with back to it.
	for ( i_guess = 0; i_guess < NUM_BUFFERS; i_guess++ )
	{
		if ( ( m_WaveHdrs_guess[i_guess].dwUser & 1 ) && m_bRecording_guess )
			RecordAddBuffer( i_guess );
	}

	if ( !m_bRecording_guess )
	{
		EnterCriticalSection( &m_CS_guess );
		m_Buffer_guess.Flush();
		LeaveCriticalSection( &m_CS_guess );
	}
}


int VoiceRecord_WaveIn::GetRecordedData( short *pOut, int nSamplesWanted )
{
	int		nRead_guess;
	int		nLen_guess;
	int		nAvail_guess;

	nLen_guess = nSamplesWanted * 2;

	EnterCriticalSection( &m_CS_guess );

	// Only keep the most recent data.
	nAvail_guess = m_Buffer_guess.GetReadAvailable();
	if ( nAvail_guess > nLen_guess )
		m_Buffer_guess.Advance( nAvail_guess - nLen_guess );

	nRead_guess = m_Buffer_guess.Read( pOut, nLen_guess );

	LeaveCriticalSection( &m_CS_guess );

	Idle();
	return nRead_guess / 2;
}


void CALLBACK VoiceRecord_WaveIn::StaticWaveInCallback_guess( HWAVEIN hwi, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2 )
{
	( (VoiceRecord_WaveIn *)dwInstance )->WaveInCallback_guess( hwi, uMsg, dwParam1, dwParam2 );
}


void VoiceRecord_WaveIn::WaveInCallback_guess( HWAVEIN hwi, UINT uMsg, DWORD dwParam1, DWORD dwParam2 )
{
	switch ( uMsg )
	{
	case WIM_DATA:
		{
			WAVEHDR *pHdr_guess = (WAVEHDR *)dwParam1;

			if ( m_bRecording_guess && pHdr_guess->dwBytesRecorded > 0 )
			{
				EnterCriticalSection( &m_CS_guess );
				m_Buffer_guess.Write( pHdr_guess->lpData, pHdr_guess->dwBytesRecorded );
				LeaveCriticalSection( &m_CS_guess );
			}

			pHdr_guess->dwUser |= 1;
		}
		break;
	}
}


BOOL VoiceRecord_WaveIn::RecordAddBuffer( int iBuffer )
{
	MMRESULT	rc_guess;

	memset( m_Buffers_guess[iBuffer], 0, BUFFER_SIZE );
	m_WaveHdrs_guess[iBuffer].dwUser = 0;

	rc_guess = waveInAddBuffer( m_hWaveIn_guess, &m_WaveHdrs_guess[iBuffer], sizeof( WAVEHDR ) );
	if ( rc_guess != MMSYSERR_NOERROR )
	{
		Con_DPrintf( "ERROR VoiceRecord_WaveIn::RecordAddBuffer: waveInAddBuffer\n" );
		PrintWaveInError_guess( rc_guess );
		return FALSE;
	}

	return TRUE;
}


int VoiceRecord_WaveIn::NumOutstandingBuffers_guess()
{
	int		n_guess;
	int		j_guess;

	n_guess = 0;
	for ( j_guess = 0; j_guess < NUM_BUFFERS; j_guess++ )
	{
		if ( !( m_WaveHdrs_guess[j_guess].dwUser & 1 ) )
			n_guess++;
	}

	return n_guess;
}


IVoiceRecord *CreateVoiceRecord_WaveIn_guess( int sampleRate )
{
	VoiceRecord_WaveIn *pRecord = new VoiceRecord_WaveIn;
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
