//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: the glue between the voice channels and the sound engine
//
// $NoKeywords: $
//=============================================================================

#ifndef VOICE_SOUND_ENGINE_INTERFACE_H
#define VOICE_SOUND_ENGINE_INTERFACE_H
#pragma once

void	VoiceSE_RegisterCvars();

bool	VoiceSE_Init();
void	VoiceSE_Term();
void	VoiceSE_Idle( float frametime );

// start and stop the sound engine channel of a voice channel
void	VoiceSE_StartChannel( int iChannel );
void	VoiceSE_EndChannel( int iChannel );

// the voice ducks the other sounds while somebody talks
void	VoiceSE_StartOverdrive();
void	VoiceSE_EndOverdrive();

// the mouth movement of a talking player
void	VoiceSE_InitMouth( int entnum );
void	VoiceSE_CloseMouth( int entnum );
void	VoiceSE_MoveMouth( int entnum, short *pSamples, int nSamples );

#endif // VOICE_SOUND_ENGINE_INTERFACE_H
