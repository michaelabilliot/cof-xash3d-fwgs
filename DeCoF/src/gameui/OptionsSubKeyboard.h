//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef OPTIONS_SUB_KEYBOARD_H
#define OPTIONS_SUB_KEYBOARD_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyPage.h>
#include <UtlVector.h>
#include <utlsymbol.h>

namespace vgui = vgui2;

class VControlsListPanel;

namespace vgui2
{
class Button;
};

//-----------------------------------------------------------------------------
// Purpose: Keyboard Details, Part of OptionsDialog
//-----------------------------------------------------------------------------
class COptionsSubKeyboard : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( COptionsSubKeyboard, vgui::PropertyPage );

public:
	COptionsSubKeyboard( vgui::Panel *parent );
	~COptionsSubKeyboard();

	virtual void	OnResetData();
	virtual void	OnApplyChanges();
	virtual void	OnKeyCodePressed( vgui::KeyCode code );

	virtual void	Finish( int key, int button );

	virtual void	OnKeyCodeTyped( vgui::KeyCode code );
	virtual void	OnKeyTyped( wchar_t unichar );
	virtual void	OnMousePressed( vgui::MouseCode code );
	virtual void	OnMouseDoublePressed( vgui::MouseCode code );
	virtual void	OnMouseWheeled( int delta );

private:
	MESSAGE_FUNC_INT( ItemSelected, "ItemSelected", itemID );

	// Create the key binding list control
	void			CreateKeyBindingList( void );

	virtual void	OnCommand( const char *command );

	// Tell engine to bind/unbind a key
	void			BindKey( const char *key, const char *binding );
	void			UnbindKey( const char *key );

	// Get column 0 action descriptions for all keys
	const char		*GetKeyName( int keynum );
	int				FindKeyForName( char const *keyname );

	// Save/restore/cleanup engine's current bindings ( for handling cancel button )
	void			SaveCurrentBindings( void );
	void			DeleteSavedBindings( void );

	// Get column 0 action descriptions for all keys
	void			ParseActionDescriptions( void );

	// Populate list of actions with current engine keybindings
	void			FillInCurrentBindings( void );
	// Remove all current bindings from list of bindings
	void			ClearBindItems( void );
	// Fill in bindings with mod-specified defaults
	void			FillInDefaultBindings( void );
	// Copy active config to the backup config
	void			ApplyAllBindings( void );

	// Bind a key to the item
	void			AddBinding( KeyValues *item, const char *keyname );

	// Remove all instances of the specified key from bindings
	void			RemoveKeyFromBindItems( const char *key );

	// Find item by binding name
	KeyValues		*GetItemForBinding( const char *binding );

private:
	VControlsListPanel	*m_pKeyBindList;

	vgui::Button		*m_pSetBindingButton;
	vgui::Button		*m_pClearBindingButton;

	typedef struct
	{
		char	*binding;
	} KeyBinding;

	// List of saved bindings for the keys
	KeyBinding		m_Bindings[ 256 ];

	// List of all the keys that need to have their binding removed
	CUtlVector< CUtlSymbol > m_KeysToUnbind;
};

#endif // OPTIONS_SUB_KEYBOARD_H
