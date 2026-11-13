//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: voice communication of the engine (voice.cpp)
//
// $NoKeywords: $
//=============================================================================

#ifndef VOICE_H
#define VOICE_H
#pragma once

#include "ivoicetweak.h"

extern "C" IVoiceTweak g_VoiceTweakAPI;	// cl_enginefuncs.pVoiceTweak (cdll_int.c)

class CCircularBuffer;

extern "C"
{
void		Voice_RegisterCvars( void );
qboolean	Voice_Init( const char *pCodecName, int quality );
void		Voice_Deinit( void );
qboolean	Voice_GetLoopback( void );
void		Voice_LocalPlayerTalkingAck( void );
void		Voice_Idle( float frametime );
qboolean	Voice_IsRecording( void );
qboolean	Voice_RecordStart( const char *pUncompressedFile, const char *pDecompressedFile, const char *pMicInputFile );
qboolean	Voice_RecordStop( void );
int			Voice_GetCompressedData( char *pchDest, int nCount, qboolean bFinal );
int			Voice_AssignChannel( int nEntity );
int			Voice_GetChannel( int nEntity );
int			Voice_AddIncomingData( int nChannel, const char *pchData, int nCount, int iSequenceNumber );
}

int		Voice_GetOutputData( const int iChannel, char *copyBufBytes, const int copyBufSize, const int samplePosition, const int sampleCount );
void	Voice_EndChannel( int iChannel, bool bTellVoiceSE );
void	Voice_NotifyFreeChannel( int iChannel );
void	Voice_EndAllChannels( void );
void	Voice_UpdateVoiceTweakMode( void );
void	Voice_FlushChannel( int nChannel );
double	UpsampleIntoBuffer( const short *pSrc, int nSrcSamples, CCircularBuffer *pBuffer, double startFraction, double rate );

#endif // VOICE_H
