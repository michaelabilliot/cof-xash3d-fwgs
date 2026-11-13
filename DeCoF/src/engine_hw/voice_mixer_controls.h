//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: the microphone controls of the sound card mixer
//
// $NoKeywords: $
//=============================================================================

#ifndef VOICE_MIXER_CONTROLS_H
#define VOICE_MIXER_CONTROLS_H
#pragma once

class IMixerControls
{
public:
	enum Control
	{
		MicBoost = 0,
		MicVolume,
		MicMute,
		NumControls
	};

protected:
	virtual			~IMixerControls() {}

public:
	virtual void	Release() = 0;

	// Use this to get and set the control values.
	virtual bool	GetValue_Float( Control iControl, float &value ) = 0;
	virtual bool	SetValue_Float( Control iControl, float value ) = 0;

	// Makes the microphone the recording source of waveIn.
	virtual bool	SelectMicrophoneForWaveInput() = 0;
};

IMixerControls	*GetMixerControls();

#endif // VOICE_MIXER_CONTROLS_H
