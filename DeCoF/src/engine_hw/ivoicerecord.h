//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: microphone input (waveIn and DirectSoundCapture)
//
// $NoKeywords: $
//=============================================================================

#ifndef IVOICERECORD_H
#define IVOICERECORD_H
#pragma once

class IVoiceRecord
{
protected:
	virtual			~IVoiceRecord() {}

public:
	// Use this to delete the object.
	virtual void	Release() = 0;

	// Start/stop capturing.
	virtual bool	RecordStart() = 0;
	virtual void	RecordStop() = 0;

	// Idle processing.
	virtual void	Idle() = 0;

	// Get the most recent N samples. Returns the number of samples written.
	virtual int		GetRecordedData( short *pOut, int nSamplesWanted ) = 0;
};

// the two implementations (voice_record_dsound.cpp, voice_record_wavein.cpp)
IVoiceRecord	*CreateVoiceRecord_DSound_guess( int nSamplesPerSec );
IVoiceRecord	*CreateVoiceRecord_WaveIn_guess( int nSamplesPerSec );

#endif // IVOICERECORD_H
