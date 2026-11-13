//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: automatic gain control of the incoming voice data
//
// $NoKeywords: $
//=============================================================================

#include <windows.h>
#include "voice_gain.h"

#define AG_FIX_SHIFT		7
#define AG_FLOAT_TO_FIXED(f)	((AGFixed)((f) * (double)(1 << AG_FIX_SHIFT)))
#define AG_FIXED_TO_INT(f)		((int)((f) >> AG_FIX_SHIFT))

CAutoGain::CAutoGain()
{
	Reset( 128, 5.0f, 0.5f, 1 );
}

void CAutoGain::Reset( int blockSize, float maxGain, float avgToMaxVal, float scale )
{
	m_BlockSize = blockSize;
	m_MaxGain = maxGain;
	m_AvgToMaxVal = avgToMaxVal;

	m_CurBlockOffset = 0;
	m_CurTotal = 0;
	m_CurMax = 0;

	m_CurrentGain = 1;
	m_NextGain = 1;
	m_Scale = scale;

	m_GainMultiplier = 0;
	m_FixedCurrentGain = AG_FLOAT_TO_FIXED( 1.0f );
}

void CAutoGain::ProcessSamples( short *pSamples, int nSamples )
{
	short *pCurPos = pSamples;
	int nSamplesLeft = nSamples;

	// Continue until we hit the end of this block.
	while ( nSamplesLeft )
	{
		int nToProcess = min( nSamplesLeft, ( m_BlockSize - m_CurBlockOffset ) );
		for ( int iSample = 0; iSample < nToProcess; iSample++ )
		{
			// Update running totals..
			if ( pCurPos[iSample] < 0 )
			{
				m_CurTotal += -pCurPos[iSample];
				if ( -pCurPos[iSample] > m_CurMax )
					m_CurMax = -pCurPos[iSample];
			}
			else
			{
				m_CurTotal += pCurPos[iSample];
				if ( pCurPos[iSample] > m_CurMax )
					m_CurMax = pCurPos[iSample];
			}

			// Interpolate between the current and next gain values.
			AGFixed gain = m_FixedCurrentGain + iSample * m_GainMultiplier;

			int newval = AG_FIXED_TO_INT( pCurPos[iSample] * gain );
			newval = min( 32767, max( newval, -32768 ) );
			pCurPos[iSample] = (short)newval;
		}
		pCurPos += nToProcess;
		nSamplesLeft -= nToProcess;

		// Did we just finish a block? (m_CurBlockOffset is never advanced,
		// so this is true after every pass)
		if ( ( m_CurBlockOffset % m_BlockSize ) == 0 )
		{
			// Yep.. calculate the next gain
			m_CurrentGain = m_NextGain;
			m_FixedCurrentGain = AG_FLOAT_TO_FIXED( m_CurrentGain );

			int avg = m_CurTotal / m_BlockSize;
			// Average between the average and the max to get the value we'll scale.
			float modifiedMax = avg + ( m_CurMax - avg ) * m_AvgToMaxVal;
			m_NextGain = min( 32767.0f / modifiedMax, m_MaxGain ) * m_Scale;

			// Setup values for fixed-point interpolation
			float fGainMultiplier = ( m_NextGain - m_CurrentGain ) / ( m_BlockSize - 1 );
			m_GainMultiplier = AG_FLOAT_TO_FIXED( fGainMultiplier );

			m_CurTotal = 0;
			m_CurMax = 0;
		}
	}
}
