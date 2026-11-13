//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include "OptionsSubAudio.h"
#include "CvarSlider.h"
#include "CvarToggleCheckButton.h"
#include "LabeledCommandComboBox.h"
#include "EngineInterface.h"
#include "ModInfo.h"

#include <vgui/KeyValues.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
COptionsSubAudio::COptionsSubAudio( vgui::Panel *parent ) : PropertyPage( parent, NULL )
{
	m_pEAXCheckButton_guess = new CCvarToggleCheckButton( this, "EAX", "#GameUI_EnableEAX", "s_eax" );
	m_pA3DCheckButton_guess = new CCvarToggleCheckButton( this, "A3D", "#GameUI_EnableA3D", "s_a3d" );

	m_pSFXSlider = new CCvarSlider( this, "SFX Slider", "#GameUI_SoundEffectVolume", 0.0f, 2.0f, "volume" );
	m_pHEVSlider = new CCvarSlider( this, "Suit Slider", "#GameUI_HEVSuitVolume", 0.0f, 2.0f, "suitvolume" );
	m_pMP3Slider = new CCvarSlider( this, "MP3 Volume", "#GameUI_MP3Volume", 0.0f, 1.0f, "mp3volume" );

	m_pSoundQualityCombo = new CLabeledCommandComboBox( this, "Sound Quality" );
	m_pSoundQualityCombo->AddItem( "#GameUI_High", "hisound 1" );
	m_pSoundQualityCombo->AddItem( "#GameUI_Low", "hisound 0" );
	m_pSoundQualityCombo->SetInitialItem( engine->pfnGetCvarFloat( "hisound" ) != 0.0f ? 0 : 1 );

	LoadControlSettings( "Resource\\OptionsSubAudio.res" );

	// multiplayer only mods have no HEV suit
	if ( ModInfo().IsMultiplayerOnly() )
	{
		Panel *child = FindChildByName( "suit label" );
		if ( child )
		{
			child->SetVisible( false );
		}

		child = FindChildByName( "Suit Slider" );
		if ( child )
		{
			child->SetVisible( false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
COptionsSubAudio::~COptionsSubAudio()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubAudio::OnResetData()
{
	m_pEAXCheckButton_guess->Reset();
	m_pA3DCheckButton_guess->Reset();
	m_pSFXSlider->Reset();
	m_pHEVSlider->Reset();
	m_pMP3Slider->Reset();
	m_pSoundQualityCombo->Reset();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubAudio::OnApplyChanges()
{
	m_pEAXCheckButton_guess->ApplyChanges();
	m_pA3DCheckButton_guess->ApplyChanges();
	m_pSFXSlider->ApplyChanges();
	m_pHEVSlider->ApplyChanges();
	m_pMP3Slider->ApplyChanges();
	m_pSoundQualityCombo->ApplyChanges();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubAudio::OnControlModified()
{
	PostActionSignal( new KeyValues( "ApplyButtonEnable" ) );
}
