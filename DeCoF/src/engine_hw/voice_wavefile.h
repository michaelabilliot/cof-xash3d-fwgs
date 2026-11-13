//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: reads and writes the .wav files of voice_record_start
//
// $NoKeywords: $
//=============================================================================

#ifndef VOICE_WAVEFILE_H
#define VOICE_WAVEFILE_H
#pragma once

bool	ReadWaveFile( const char *pFilename, char *&pData, int &nDataBytes, int &wBitsPerSample, int &nChannels, int &nSamplesPerSec );
bool	WriteWaveFile( const char *pFilename, const char *pData, int nBytes, int wBitsPerSample, int nChannels, int nSamplesPerSec );

#endif // VOICE_WAVEFILE_H
