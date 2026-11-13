//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <string.h>

#include "LabeledCommandComboBox.h"
#include "EngineInterface.h"

#include <vgui/ILocalize.h>
#include <vgui/KeyValues.h>
#include <vgui_controls/Controls.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CLabeledCommandComboBox::CLabeledCommandComboBox( vgui::Panel *parent, const char *panelName ) : vgui::ComboBox( parent, panelName, 6, false )
{
	AddActionSignalTarget( this );
	m_iCurrentSelection = -1;
	m_iStartSelection = -1;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CLabeledCommandComboBox::~CLabeledCommandComboBox()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLabeledCommandComboBox::DeleteAllItems()
{
	BaseClass::DeleteAllItems();
	m_Items.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: adds an item with the console command it runs
//-----------------------------------------------------------------------------
void CLabeledCommandComboBox::AddItem( char const *text, char const *engineCommand )
{
	int idx = m_Items.AddToTail();
	COMMANDITEM *item = &m_Items[ idx ];

	item->comboBoxID = BaseClass::AddItem( text, NULL );

	strcpy( item->name, text );

	if ( text[0] == '#' )
	{
		// need to localize the string
		wchar_t *localized = localize()->Find( text );
		if ( localized )
		{
			localize()->ConvertUnicodeToANSI( localized, item->name, sizeof( item->name ) );
		}
	}

	strcpy( item->command, engineCommand );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLabeledCommandComboBox::ActivateItem( int index )
{
	if ( index < m_Items.Count() )
	{
		int comboBoxID = m_Items[ index ].comboBoxID;
		BaseClass::ActivateItem( comboBoxID );
		m_iCurrentSelection = index;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLabeledCommandComboBox::SetInitialItem( int index )
{
	if ( index < m_Items.Count() )
	{
		m_iStartSelection = index;
		int comboBoxID = m_Items[ index ].comboBoxID;
		ActivateItem( comboBoxID );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLabeledCommandComboBox::OnTextChanged( char const *text )
{
	int i;
	for ( i = 0; i < m_Items.Size(); i++ )
	{
		COMMANDITEM *item = &m_Items[ i ];
		if ( !stricmp( item->name, text ) )
		{
			m_iCurrentSelection = i;
			break;
		}
	}

	if ( HasBeenModified() )
	{
		PostActionSignal( new KeyValues( "ControlModified" ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CLabeledCommandComboBox::GetActiveItemCommand()
{
	if ( m_iCurrentSelection == -1 )
		return NULL;

	COMMANDITEM *item = &m_Items[ m_iCurrentSelection ];
	return item->command;
}

//-----------------------------------------------------------------------------
// Purpose: runs the command of the selected item
//-----------------------------------------------------------------------------
void CLabeledCommandComboBox::ApplyChanges()
{
	if ( m_iCurrentSelection == -1 )
		return;
	if ( m_Items.Size() < 1 )
		return;

	COMMANDITEM *item = &m_Items[ m_iCurrentSelection ];
	engine->pfnClientCmd( item->command );
	m_iStartSelection = m_iCurrentSelection;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CLabeledCommandComboBox::HasBeenModified()
{
	return m_iStartSelection != m_iCurrentSelection;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLabeledCommandComboBox::Reset()
{
	if ( m_iStartSelection != -1 )
	{
		ActivateItem( m_iStartSelection );
	}
}
