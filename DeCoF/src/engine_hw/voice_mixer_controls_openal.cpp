//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: the microphone controls of the sound card mixer (winmm mixer API;
// the Linux build replaced this file with an OpenAL stub of the same name)
//
// Only the class name (RTTI) and the IMixerControls methods are proven; the
// private methods and members are named _guess.
//
// $NoKeywords: $
//=============================================================================

#include <windows.h>
#include <mmsystem.h>
#include <string.h>
#include <malloc.h>

#include "voice_mixer_controls.h"

class CMixerControls : public IMixerControls
{
public:
					CMixerControls();
	virtual			~CMixerControls();

	virtual void	Release();
	virtual bool	GetValue_Float( Control iControl, float &value );
	virtual bool	SetValue_Float( Control iControl, float value );
	virtual bool	SelectMicrophoneForWaveInput();

protected:
	bool			Init_guess();
	void			Term_guess();

	void			Clear_guess();

	bool			GetControlOption_guess( DWORD dwControlID, DWORD cMultipleItems, bool &bValue );
	bool			SetControlOption_guess( DWORD dwControlID, DWORD cMultipleItems, bool bValue );

	bool			GetControlUnsigned_guess( DWORD dwControlID, DWORD cMultipleItems, DWORD &value );
	bool			SetControlUnsigned_guess( DWORD dwControlID, DWORD cMultipleItems, DWORD value );

	bool			GetLineControls_guess( DWORD dwLineID, MIXERCONTROL *controls, DWORD nControls );
	void			FindMicSelectControl_guess( DWORD dwLineID, DWORD nControls );

private:
	HMIXER			m_hMixer_guess;

	// The mixer or mux control of the wave input line that selects the recording source.
	DWORD			m_dwMicSelectControlID_guess;
	DWORD			m_dwMicSelectMultipleItems_guess;
	DWORD			m_dwMicSelectControlType_guess;
	DWORD			m_dwMicSelectIndex_guess;

	// Info about each control we use.
	class ControlInfo_guess
	{
	public:
		DWORD		m_dwControlID_guess;
		DWORD		m_cMultipleItems_guess;
		bool		m_bFound_guess;
	};

	ControlInfo_guess	m_ControlInfos_guess[NumControls];
};

CMixerControls::CMixerControls()
{
	m_dwMicSelectControlID_guess = 0xFFFFFFFF;

	Clear_guess();
	Init_guess();
}

CMixerControls::~CMixerControls()
{
	Term_guess();
}

bool CMixerControls::Init_guess()
{
	Term_guess();

	MMRESULT mmresult_guess;

	// Open the mixer.
	mmresult_guess = mixerOpen( &m_hMixer_guess, 0, 0, 0, 0 );
	if ( mmresult_guess != MMSYSERR_NOERROR )
	{
		Term_guess();
		return false;
	}

	// Get the mixer's capabilities.
	MIXERCAPS caps_guess;
	mmresult_guess = mixerGetDevCaps( (UINT)m_hMixer_guess, &caps_guess, sizeof( caps_guess ) );
	if ( mmresult_guess != MMSYSERR_NOERROR )
	{
		Term_guess();
		return false;
	}

	// Look at each destination line and its source lines for the microphone controls.
	for ( UINT iDestination_guess = 0; iDestination_guess < caps_guess.cDestinations; iDestination_guess++ )
	{
		MIXERLINE destLine_guess;
		destLine_guess.cbStruct = sizeof( destLine_guess );
		destLine_guess.dwDestination = iDestination_guess;
		mmresult_guess = mixerGetLineInfo( (HMIXEROBJ)m_hMixer_guess, &destLine_guess, MIXER_GETLINEINFOF_DESTINATION );
		if ( mmresult_guess != MMSYSERR_NOERROR )
			continue;

		// The wave input line has the control that selects the recording source.
		if ( destLine_guess.dwComponentType == MIXERLINE_COMPONENTTYPE_DST_WAVEIN )
			FindMicSelectControl_guess( destLine_guess.dwLineID, destLine_guess.cControls );

		DWORD nConnections_guess = destLine_guess.cConnections;
		for ( DWORD source_guess = 0; source_guess < nConnections_guess; source_guess++ )
		{
			MIXERLINE srcLine_guess;
			srcLine_guess.cbStruct = sizeof( srcLine_guess );
			srcLine_guess.dwDestination = iDestination_guess;
			srcLine_guess.dwSource = source_guess;
			mmresult_guess = mixerGetLineInfo( (HMIXEROBJ)m_hMixer_guess, &srcLine_guess, MIXER_GETLINEINFOF_SOURCE );
			if ( mmresult_guess != MMSYSERR_NOERROR )
				continue;

			MIXERCONTROL *pControls_guess = (MIXERCONTROL *)_alloca( sizeof( MIXERCONTROL ) * srcLine_guess.cControls );
			if ( !GetLineControls_guess( srcLine_guess.dwLineID, pControls_guess, srcLine_guess.cControls ) )
				continue;

			for ( DWORD iControl_guess = 0; iControl_guess < srcLine_guess.cControls; iControl_guess++ )
			{
				MIXERCONTROL *pControl_guess = &pControls_guess[iControl_guess];

				if ( srcLine_guess.dwComponentType == MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE )
				{
					// The microphone boost (on/off switch).
					if ( pControl_guess->dwControlType == MIXERCONTROL_CONTROLTYPE_ONOFF &&
						( strstr( pControl_guess->szShortName, "Gain" ) || strstr( pControl_guess->szShortName, "Boost" ) || strstr( pControl_guess->szShortName, "+20d" ) ) )
					{
						m_ControlInfos_guess[MicBoost].m_bFound_guess = true;
						m_ControlInfos_guess[MicBoost].m_dwControlID_guess = pControl_guess->dwControlID;
						m_ControlInfos_guess[MicBoost].m_cMultipleItems_guess = pControl_guess->cMultipleItems;
					}

					// The microphone mute on the speakers.
					if ( destLine_guess.dwComponentType == MIXERLINE_COMPONENTTYPE_DST_SPEAKERS && pControl_guess->dwControlType == MIXERCONTROL_CONTROLTYPE_MUTE )
					{
						m_ControlInfos_guess[MicMute].m_bFound_guess = true;
						m_ControlInfos_guess[MicMute].m_dwControlID_guess = pControl_guess->dwControlID;
						m_ControlInfos_guess[MicMute].m_cMultipleItems_guess = pControl_guess->cMultipleItems;
					}

					// The microphone recording volume.
					if ( destLine_guess.dwComponentType == MIXERLINE_COMPONENTTYPE_DST_WAVEIN && pControl_guess->dwControlType == MIXERCONTROL_CONTROLTYPE_VOLUME )
					{
						m_ControlInfos_guess[MicVolume].m_bFound_guess = true;
						m_ControlInfos_guess[MicVolume].m_dwControlID_guess = pControl_guess->dwControlID;
						m_ControlInfos_guess[MicVolume].m_cMultipleItems_guess = pControl_guess->cMultipleItems;
					}
				}
			}
		}
	}

	return true;
}

void CMixerControls::Term_guess()
{
	if ( m_hMixer_guess )
	{
		mixerClose( m_hMixer_guess );
		m_hMixer_guess = 0;
	}

	Clear_guess();
}

bool CMixerControls::GetValue_Float( Control iControl, float &value )
{
	if ( iControl < 0 || iControl >= NumControls || !m_ControlInfos_guess[iControl].m_bFound_guess )
		return false;

	if ( iControl == MicBoost || iControl == MicMute )
	{
		bool value_guess;
		bool ret_guess = GetControlOption_guess( m_ControlInfos_guess[iControl].m_dwControlID_guess, m_ControlInfos_guess[iControl].m_cMultipleItems_guess, value_guess );
		value = value_guess;
		return ret_guess;
	}
	else if ( iControl == MicVolume )
	{
		DWORD dwValue_guess;
		if ( GetControlUnsigned_guess( m_ControlInfos_guess[iControl].m_dwControlID_guess, m_ControlInfos_guess[iControl].m_cMultipleItems_guess, dwValue_guess ) )
		{
			value = dwValue_guess / 65535.0f;
			return true;
		}
	}

	return false;
}

bool CMixerControls::SetValue_Float( Control iControl, float value )
{
	if ( iControl < 0 || iControl >= NumControls || !m_ControlInfos_guess[iControl].m_bFound_guess )
		return false;

	if ( iControl == MicBoost || iControl == MicMute )
	{
		bool bValue_guess = value != 0.0f;
		return SetControlOption_guess( m_ControlInfos_guess[iControl].m_dwControlID_guess, m_ControlInfos_guess[iControl].m_cMultipleItems_guess, bValue_guess );
	}
	else if ( iControl == MicVolume )
	{
		DWORD dwValue_guess = (DWORD)( value * 65535.0f );
		return SetControlUnsigned_guess( m_ControlInfos_guess[iControl].m_dwControlID_guess, m_ControlInfos_guess[iControl].m_cMultipleItems_guess, dwValue_guess );
	}
	else
	{
		return false;
	}
}

bool CMixerControls::SelectMicrophoneForWaveInput()
{
	if ( m_dwMicSelectControlID_guess == 0xFFFFFFFF )
		return false;

	MIXERCONTROLDETAILS_BOOLEAN *pmxcd_b_guess = (MIXERCONTROLDETAILS_BOOLEAN *)_alloca( sizeof( MIXERCONTROLDETAILS_BOOLEAN ) * m_dwMicSelectMultipleItems_guess );

	MIXERCONTROLDETAILS details_guess;
	details_guess.cbStruct = sizeof( details_guess );
	details_guess.dwControlID = m_dwMicSelectControlID_guess;
	details_guess.cChannels = 1;
	details_guess.cMultipleItems = m_dwMicSelectMultipleItems_guess;
	details_guess.cbDetails = sizeof( MIXERCONTROLDETAILS_BOOLEAN );
	details_guess.paDetails = pmxcd_b_guess;
	if ( mixerGetControlDetails( (HMIXEROBJ)m_hMixer_guess, &details_guess, MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_VALUE ) == MMSYSERR_NOERROR )
	{
		// A mux allows only one source at a time.
		if ( m_dwMicSelectControlType_guess == MIXERCONTROL_CONTROLTYPE_MUX )
			memset( pmxcd_b_guess, 0, sizeof( MIXERCONTROLDETAILS_BOOLEAN ) * m_dwMicSelectMultipleItems_guess );

		pmxcd_b_guess[m_dwMicSelectIndex_guess].fValue = 1;

		details_guess.cbStruct = sizeof( details_guess );
		details_guess.dwControlID = m_dwMicSelectControlID_guess;
		details_guess.cChannels = 1;
		details_guess.cMultipleItems = m_dwMicSelectMultipleItems_guess;
		details_guess.cbDetails = sizeof( MIXERCONTROLDETAILS_BOOLEAN );
		details_guess.paDetails = pmxcd_b_guess;
		if ( mixerSetControlDetails( (HMIXEROBJ)m_hMixer_guess, &details_guess, MIXER_OBJECTF_HMIXER | MIXER_SETCONTROLDETAILSF_VALUE ) == MMSYSERR_NOERROR )
			return true;
	}

	return false;
}

void CMixerControls::Release()
{
	delete this;
}

void CMixerControls::Clear_guess()
{
	m_hMixer_guess = 0;
	memset( m_ControlInfos_guess, 0, sizeof( m_ControlInfos_guess ) );
}

bool CMixerControls::GetControlOption_guess( DWORD dwControlID, DWORD cMultipleItems, bool &bValue )
{
	MIXERCONTROLDETAILS cd_guess;
	MIXERCONTROLDETAILS_BOOLEAN mxcdb_guess;

	cd_guess.cbStruct = sizeof( cd_guess );
	cd_guess.dwControlID = dwControlID;
	cd_guess.cChannels = 1;
	cd_guess.cMultipleItems = cMultipleItems;
	cd_guess.cbDetails = sizeof( mxcdb_guess );
	cd_guess.paDetails = &mxcdb_guess;

	MMRESULT mmr_guess = mixerGetControlDetails( (HMIXEROBJ)m_hMixer_guess, &cd_guess, 0 );
	if ( mmr_guess == MMSYSERR_NOERROR )
	{
		bValue = ( mxcdb_guess.fValue != 0 );
		return true;
	}
	else
	{
		return false;
	}
}

bool CMixerControls::SetControlOption_guess( DWORD dwControlID, DWORD cMultipleItems, bool bValue )
{
	MIXERCONTROLDETAILS cd_guess;
	MIXERCONTROLDETAILS_BOOLEAN mxcdb_guess;

	cd_guess.cbStruct = sizeof( cd_guess );
	cd_guess.dwControlID = dwControlID;
	cd_guess.cChannels = 1;
	cd_guess.cMultipleItems = cMultipleItems;
	cd_guess.cbDetails = sizeof( mxcdb_guess );
	cd_guess.paDetails = &mxcdb_guess;

	mxcdb_guess.fValue = bValue;
	MMRESULT mmr_guess = mixerSetControlDetails( (HMIXEROBJ)m_hMixer_guess, &cd_guess, 0 );
	return mmr_guess == MMSYSERR_NOERROR;
}

bool CMixerControls::GetControlUnsigned_guess( DWORD dwControlID, DWORD cMultipleItems, DWORD &value )
{
	MIXERCONTROLDETAILS cd_guess;
	MIXERCONTROLDETAILS_UNSIGNED mxcd_u_guess;

	cd_guess.cbStruct = sizeof( cd_guess );
	cd_guess.dwControlID = dwControlID;
	cd_guess.cChannels = 1;
	cd_guess.cMultipleItems = cMultipleItems;
	cd_guess.cbDetails = sizeof( mxcd_u_guess );
	cd_guess.paDetails = &mxcd_u_guess;

	MMRESULT mmr_guess = mixerGetControlDetails( (HMIXEROBJ)m_hMixer_guess, &cd_guess, 0 );
	if ( mmr_guess == MMSYSERR_NOERROR )
	{
		value = mxcd_u_guess.dwValue;
		return true;
	}
	else
	{
		return false;
	}
}

bool CMixerControls::SetControlUnsigned_guess( DWORD dwControlID, DWORD cMultipleItems, DWORD value )
{
	MIXERCONTROLDETAILS cd_guess;
	MIXERCONTROLDETAILS_UNSIGNED mxcd_u_guess;

	cd_guess.cbStruct = sizeof( cd_guess );
	cd_guess.dwControlID = dwControlID;
	cd_guess.cChannels = 1;
	cd_guess.cMultipleItems = cMultipleItems;
	cd_guess.cbDetails = sizeof( mxcd_u_guess );
	cd_guess.paDetails = &mxcd_u_guess;

	mxcd_u_guess.dwValue = value;
	MMRESULT mmr_guess = mixerSetControlDetails( (HMIXEROBJ)m_hMixer_guess, &cd_guess, 0 );
	return mmr_guess == MMSYSERR_NOERROR;
}

bool CMixerControls::GetLineControls_guess( DWORD dwLineID, MIXERCONTROL *controls, DWORD nControls )
{
	MIXERLINECONTROLS mxlc_guess;

	mxlc_guess.cbStruct = sizeof( mxlc_guess );
	mxlc_guess.dwLineID = dwLineID;
	mxlc_guess.cControls = nControls;
	mxlc_guess.cbmxctrl = sizeof( MIXERCONTROL );
	mxlc_guess.pamxctrl = controls;

	MMRESULT mmr_guess = mixerGetLineControls( (HMIXEROBJ)m_hMixer_guess, &mxlc_guess, MIXER_GETLINECONTROLSF_ALL );
	return mmr_guess == MMSYSERR_NOERROR;
}

void CMixerControls::FindMicSelectControl_guess( DWORD dwLineID, DWORD nControls )
{
	m_dwMicSelectControlID_guess = 0xFFFFFFFF;

	MIXERCONTROL *controls_guess = (MIXERCONTROL *)_alloca( sizeof( MIXERCONTROL ) * nControls );
	if ( !GetLineControls_guess( dwLineID, controls_guess, nControls ) )
		return;

	for ( DWORD iControl_guess = 0; iControl_guess < nControls; iControl_guess++ )
	{
		if ( controls_guess[iControl_guess].dwControlType == MIXERCONTROL_CONTROLTYPE_MIXER ||
			controls_guess[iControl_guess].dwControlType == MIXERCONTROL_CONTROLTYPE_MUX )
		{
			m_dwMicSelectControlID_guess = controls_guess[iControl_guess].dwControlID;
			m_dwMicSelectControlType_guess = controls_guess[iControl_guess].dwControlType;
			m_dwMicSelectMultipleItems_guess = controls_guess[iControl_guess].cMultipleItems;
			m_dwMicSelectIndex_guess = iControl_guess;

			// Find the item of the select control that is the microphone line.
			MIXERCONTROLDETAILS_LISTTEXT *pmxcd_lt_guess = (MIXERCONTROLDETAILS_LISTTEXT *)_alloca( sizeof( MIXERCONTROLDETAILS_LISTTEXT ) * m_dwMicSelectMultipleItems_guess );

			MIXERCONTROLDETAILS details_guess;
			details_guess.cbStruct = sizeof( details_guess );
			details_guess.dwControlID = m_dwMicSelectControlID_guess;
			details_guess.cChannels = 1;
			details_guess.cMultipleItems = m_dwMicSelectMultipleItems_guess;
			details_guess.cbDetails = sizeof( MIXERCONTROLDETAILS_LISTTEXT );
			details_guess.paDetails = pmxcd_lt_guess;
			if ( mixerGetControlDetails( (HMIXEROBJ)m_hMixer_guess, &details_guess, MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_LISTTEXT ) == MMSYSERR_NOERROR )
			{
				for ( DWORD iItem_guess = 0; iItem_guess < m_dwMicSelectMultipleItems_guess; iItem_guess++ )
				{
					MIXERLINE mixerLine_guess;
					mixerLine_guess.cbStruct = sizeof( mixerLine_guess );
					mixerLine_guess.dwLineID = pmxcd_lt_guess[iItem_guess].dwParam1;
					if ( mixerGetLineInfo( (HMIXEROBJ)m_hMixer_guess, &mixerLine_guess, MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_LINEID ) == MMSYSERR_NOERROR &&
						mixerLine_guess.dwComponentType == MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE )
					{
						m_dwMicSelectIndex_guess = iItem_guess;
						break;
					}
				}
			}

			break;
		}
	}
}

IMixerControls *GetMixerControls()
{
	return new CMixerControls;
}
