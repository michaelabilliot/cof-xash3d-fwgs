//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: automatic gain control of the incoming voice data
//
// $NoKeywords: $
//=============================================================================

#ifndef VOICE_GAIN_H
#define VOICE_GAIN_H
#pragma once

typedef int AGFixed;

class CAutoGain
{
public:
	CAutoGain();

	// Reset the gain block size, max gain and the average to max gain value.
	void	Reset( int blockSize, float maxGain, float avgToMaxVal, float scale );

	// Process the samples in place.
	void	ProcessSamples( short *pSamples, int nSamples );

private:
	int		m_BlockSize;
	float	m_MaxGain;
	float	m_AvgToMaxVal;

	int		m_CurBlockOffset;
	int		m_CurTotal;
	int		m_CurMax;

	float	m_Scale;

	float	m_CurrentGain;
	float	m_NextGain;

	AGFixed	m_FixedCurrentGain;
	AGFixed	m_GainMultiplier;
};

#endif // VOICE_GAIN_H
