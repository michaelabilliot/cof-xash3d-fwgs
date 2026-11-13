//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <string.h>
#include <wchar.h>

#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui/KeyValues.h>
#include <vgui/MouseCode.h>
#include <vgui/KeyCode.h>

#include <vgui_controls/Menu.h>
#include <vgui_controls/MenuItem.h>
#include <vgui_controls/ScrollBar.h>
#include <vgui_controls/TextImage.h>
#include <vgui_controls/Controls.h>

#define max(a,b)            (((a) > (b)) ? (a) : (b))

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
Menu::Menu(Panel *parent, const char *panelName) : Panel(parent, panelName)
{
	m_iFixedWidth = 0;
	m_iMinimumWidth = 0;
	m_iNumVisibleLines = 99;	// number of items in menu before scroll bar adds on
	m_pScroller = new ScrollBar(this, "MenuScrollBar", true);
	m_pScroller->SetVisible(false);
	m_pScroller->AddActionSignalTarget(this);
	_sizedForScrollBar = false;
	SetZPos(1);
	SetVisible(false);
	MakePopup(false);
	SetParent(parent);
	_recalculateWidth = true;
	m_iCurrentlySelectedItemID = m_MenuItems.InvalidIndex();
	m_iInputMode = MOUSE;
	m_iCheckImageWidth = 0;
	m_iActivatedItem = 0;
	m_bUseFallbackFont = false;
	m_hFallbackItemFont = INVALID_FONT;

	if (IsProportional())
	{
		int width, height;
		int sw, sh;
		surface()->GetProportionalBase( width, height );
		surface()->GetScreenSize(sw, sh);

		m_iProportionalScrollBarSize = static_cast<int>( static_cast<float>( sw ) / static_cast<float>( width ) * 18.0f );
		m_iMenuItemHeight = static_cast<int>( static_cast<float>( sw ) / static_cast<float>( width ) * static_cast<float>( DEFAULT_MENU_ITEM_HEIGHT ) );
	}
	else
	{
		m_iProportionalScrollBarSize = 18;
		m_iMenuItemHeight = DEFAULT_MENU_ITEM_HEIGHT;
	}

	m_hItemFont = INVALID_FONT;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
Menu::~Menu()
{
	delete m_pScroller;
}

//-----------------------------------------------------------------------------
// Purpose: Remove all menu items from the menu.
//-----------------------------------------------------------------------------
void Menu::DeleteAllItems()
{
	for (int i = m_MenuItems.Head(); i != m_MenuItems.InvalidIndex(); i = m_MenuItems.Next(i))
	{
		m_MenuItems[i]->MarkForDeletion();
	}

	m_MenuItems.RemoveAll();
	m_SortedItems.RemoveAll();
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Add a menu item to the menu.
//-----------------------------------------------------------------------------
int Menu::AddMenuItem( MenuItem *panel )
{
	panel->SetParent( this );
	int itemID = m_MenuItems.AddToTail( panel );
	m_SortedItems.AddToTail(itemID);
	InvalidateLayout(false);
	_recalculateWidth = true;
	if ( m_hItemFont != INVALID_FONT )
	{
		panel->SetFont( m_hItemFont );
	}
	if ( m_bUseFallbackFont && m_hFallbackItemFont != INVALID_FONT )
	{
		Label *l = panel;
		if ( l )
		{
			TextImage *ti = l->GetTextImage();
			if ( ti )
			{
				ti->SetUseFallbackFont( m_bUseFallbackFont, m_hFallbackItemFont );
			}
		}
	}

	return itemID;
}

//-----------------------------------------------------------------------------
// Purpose: Add a menu item to the menu, with a command to send
//-----------------------------------------------------------------------------
int Menu::AddMenuItemCharCommand(MenuItem *item, const char *command, Panel *target, const KeyValues *userData)
{
	item->SetCommand(command);
	item->AddActionSignalTarget( target );
	item->SetUserData(userData);
	return AddMenuItem( item );
}

//-----------------------------------------------------------------------------
// Purpose: Add a menu item to the menu, with a message to send
//-----------------------------------------------------------------------------
int Menu::AddMenuItemKeyValuesCommand(MenuItem *item, KeyValues *message, Panel *target, const KeyValues *userData)
{
	item->SetCommand(message);
	item->AddActionSignalTarget(target);
	item->SetUserData(userData);
	return AddMenuItem(item);
}

//-----------------------------------------------------------------------------
// Purpose: Add a menu item to the menu.
//-----------------------------------------------------------------------------
int Menu::AddMenuItem( const char *itemName, const char *itemText, const char *command, Panel *target, const KeyValues *userData )
{
	MenuItem *item = new MenuItem(this, itemName, itemText );
	return AddMenuItemCharCommand(item, command, target, userData);
}

int Menu::AddMenuItem( const char *itemName, const wchar_t *wszItemText, const char *command, Panel *target, const KeyValues *userData )
{
	MenuItem *item = new MenuItem(this, itemName, wszItemText );
	return AddMenuItemCharCommand(item, command, target, userData);
}

//-----------------------------------------------------------------------------
// Purpose: Add a menu item to the menu. The menu item has the same name as its text
//-----------------------------------------------------------------------------
int Menu::AddMenuItem( const char *itemText, const char *command, Panel *target, const KeyValues *userData )
{
	return AddMenuItem(itemText, itemText, command, target, userData ) ;
}

//-----------------------------------------------------------------------------
// Purpose: Add a menu item to the menu.
//-----------------------------------------------------------------------------
int Menu::AddMenuItem( const char *itemName, const char *itemText, KeyValues *message, Panel *target, const KeyValues *userData )
{
	MenuItem *item = new MenuItem(this, itemName, itemText );
	return AddMenuItemKeyValuesCommand(item, message, target, userData);
}

int Menu::AddMenuItem( const char *itemName, const wchar_t *wszItemText, KeyValues *message, Panel *target, const KeyValues *userData )
{
	MenuItem *item = new MenuItem(this, itemName, wszItemText );
	return AddMenuItemKeyValuesCommand(item, message, target, userData);
}

//-----------------------------------------------------------------------------
// Purpose: Add a menu item to the menu. The menu item has the same name as its text
//-----------------------------------------------------------------------------
int Menu::AddMenuItem( const char *itemText, KeyValues *message, Panel *target, const KeyValues *userData )
{
	return AddMenuItem(itemText, itemText, message, target, userData );
}

//-----------------------------------------------------------------------------
// Purpose: Add a menu item to the menu. The menu item has the same name as its text
//          and sends its text as the command
//-----------------------------------------------------------------------------
int Menu::AddMenuItem( const char *itemText, Panel *target , const KeyValues *userData )
{
	return AddMenuItem(itemText, itemText, target, userData );
}

//-----------------------------------------------------------------------------
// Purpose: Add a checkable menu item to the menu.
//-----------------------------------------------------------------------------
int Menu::AddCheckableMenuItem( const char *itemName, const char *itemText, const char *command, Panel *target, const KeyValues *userData )
{
	MenuItem *item = new MenuItem(this, itemName, itemText, NULL, true);
	return AddMenuItemCharCommand(item, command, target, userData);
}

int Menu::AddCheckableMenuItem( const char *itemName, const wchar_t *wszItemText, const char *command, Panel *target, const KeyValues *userData )
{
	MenuItem *item = new MenuItem(this, itemName, wszItemText, NULL, true);
	return AddMenuItemCharCommand(item, command, target, userData);
}

//-----------------------------------------------------------------------------
// Purpose: Add a checkable menu item to the menu. The menu item has the same name as its text
//-----------------------------------------------------------------------------
int Menu::AddCheckableMenuItem( const char *itemText, const char *command, Panel *target, const KeyValues *userData )
{
	return AddCheckableMenuItem(itemText, itemText, command, target, userData );
}

//-----------------------------------------------------------------------------
// Purpose: Add a checkable menu item to the menu.
//-----------------------------------------------------------------------------
int Menu::AddCheckableMenuItem( const char *itemName, const char *itemText, KeyValues *message, Panel *target, const KeyValues *userData )
{
	MenuItem *item = new MenuItem(this, itemName, itemText, NULL, true);
	return AddMenuItemKeyValuesCommand(item, message, target, userData);
}

int Menu::AddCheckableMenuItem( const char *itemName, const wchar_t *wszItemText, KeyValues *message, Panel *target, const KeyValues *userData )
{
	MenuItem *item = new MenuItem(this, itemName, wszItemText, NULL, true);
	return AddMenuItemKeyValuesCommand(item, message, target, userData);
}

//-----------------------------------------------------------------------------
// Purpose: Add a checkable menu item to the menu. The menu item has the same name as its text
//-----------------------------------------------------------------------------
int Menu::AddCheckableMenuItem( const char *itemText, KeyValues *message, Panel *target, const KeyValues *userData )
{
	return AddCheckableMenuItem(itemText, itemText, message, target, userData );
}

//-----------------------------------------------------------------------------
// Purpose: Add a checkable menu item to the menu. The menu item has the same name as its text
//          and sends its text as the command
//-----------------------------------------------------------------------------
int Menu::AddCheckableMenuItem( const char *itemText, Panel *target, const KeyValues *userData )
{
	return AddCheckableMenuItem(itemText, itemText, target, userData );
}

//-----------------------------------------------------------------------------
// Purpose: Add a Cascading menu item to the menu.
//-----------------------------------------------------------------------------
int Menu::AddCascadingMenuItem( const char *itemName, const char *itemText, const char *command, Panel *target, Menu *cascadeMenu , const KeyValues *userData )
{
	MenuItem *item = new MenuItem(this, itemName, itemText, cascadeMenu );
	return AddMenuItemCharCommand(item, command, target, userData);
}

int Menu::AddCascadingMenuItem( const char *itemName, const wchar_t *wszItemText, const char *command, Panel *target, Menu *cascadeMenu , const KeyValues *userData )
{
	MenuItem *item = new MenuItem(this, itemName, wszItemText, cascadeMenu );
	return AddMenuItemCharCommand(item, command, target, userData);
}

//-----------------------------------------------------------------------------
// Purpose: Add a Cascading menu item to the menu. The menu item has the same name as its text
//-----------------------------------------------------------------------------
int Menu::AddCascadingMenuItem( const char *itemText, const char *command, Panel *target, Menu *cascadeMenu , const KeyValues *userData )
{
	return AddCascadingMenuItem( itemText, itemText, command, target, cascadeMenu, userData );
}

//-----------------------------------------------------------------------------
// Purpose: Add a Cascading menu item to the menu.
//-----------------------------------------------------------------------------
int Menu::AddCascadingMenuItem( const char *itemName, const char *itemText, KeyValues *message, Panel *target, Menu *cascadeMenu , const KeyValues *userData )
{
	MenuItem *item = new MenuItem( this, itemName, itemText, cascadeMenu);
	return AddMenuItemKeyValuesCommand(item, message, target, userData);
}

int Menu::AddCascadingMenuItem( const char *itemName, const wchar_t *wszItemText, KeyValues *message, Panel *target, Menu *cascadeMenu , const KeyValues *userData )
{
	MenuItem *item = new MenuItem( this, itemName, wszItemText, cascadeMenu);
	return AddMenuItemKeyValuesCommand(item, message, target, userData);
}

//-----------------------------------------------------------------------------
// Purpose: Add a Cascading menu item to the menu. The menu item has the same name as its text
//-----------------------------------------------------------------------------
int Menu::AddCascadingMenuItem( const char *itemText, KeyValues *message, Panel *target, Menu *cascadeMenu, const KeyValues *userData )
{
	return AddCascadingMenuItem(itemText, itemText, message, target, cascadeMenu, userData );
}

//-----------------------------------------------------------------------------
// Purpose: Add a Cascading menu item to the menu. The menu item has the same name as its text
//          and sends its text as the command
//-----------------------------------------------------------------------------
int Menu::AddCascadingMenuItem( const char *itemText, Panel *target, Menu *cascadeMenu, const KeyValues *userData )
{
	return AddCascadingMenuItem(itemText, itemText, target, cascadeMenu, userData);
}

//-----------------------------------------------------------------------------
// Purpose: Sets the values of a menu item at the specified index
// Input  : index - the index of this item entry
//			*data - pointer to the data for this item
//-----------------------------------------------------------------------------
void Menu::UpdateMenuItem(int itemID, const char *itemText, KeyValues *message, const KeyValues *userData)
{
	// make sure its enabled since disabled items get highlighted.
	if (m_MenuItems.IsValidIndex(itemID))
	{
		MenuItem *menuItem = dynamic_cast<MenuItem *>(m_MenuItems[itemID]);
		if (menuItem)
		{
			menuItem->SetText(itemText);
			menuItem->SetCommand(message);
			if(userData)
			{
				menuItem->SetUserData(userData);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the values of a menu item at the specified index
//-----------------------------------------------------------------------------
void Menu::UpdateMenuItem(int itemID, const wchar_t *wszItemText, KeyValues *message, const KeyValues *userData)
{
	// make sure its enabled since disabled items get highlighted.
	if (m_MenuItems.IsValidIndex(itemID))
	{
		MenuItem *menuItem = dynamic_cast<MenuItem *>(m_MenuItems[itemID]);
		if (menuItem)
		{
			menuItem->SetText(wszItemText);
			menuItem->SetCommand(message);
			if(userData)
			{
				menuItem->SetUserData(userData);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the content size of the menu
//-----------------------------------------------------------------------------
void Menu::SetFixedWidth(int width)
{
	// the padding makes it so the menu has the label padding on each side of the menu.
	// makes the menu items look centered.
	m_iFixedWidth = width;
	InvalidateLayout(false);
}

//-----------------------------------------------------------------------------
// Purpose: sets the height of each menu item
//-----------------------------------------------------------------------------
void Menu::SetMenuItemHeight(int itemHeight)
{
	m_iMenuItemHeight = itemHeight;
}

//-----------------------------------------------------------------------------
// Purpose: Reformat according to the new layout
//-----------------------------------------------------------------------------
void Menu::PerformLayout()
{
	if (m_SortedItems.Count() > m_iNumVisibleLines)
	{
		AddScrollBar();

		// Make all but visible items invisible
		MakeItemsVisibleInScrollRange();
	}
	else
	{
		RemoveScrollBar();
	}

	// get the appropriate menu border
	LayoutMenuBorder();

	// make sure we factor in insets
	int ileft, iright, itop, ibottom;
	GetInset(ileft, iright, itop, ibottom);

	int menuTall = 0;
	for (int i = 0 ; i < m_SortedItems.Count() ; i++)
	{
		MenuItem *child = m_MenuItems[m_SortedItems[i]];
		if (child && child->IsVisible())
		{
			child->SetPos(0, menuTall);
			if ( m_hItemFont != INVALID_FONT )
			{
				child->SetFont( m_hItemFont );
			}
			menuTall += m_iMenuItemHeight;

			if (!child->IsCheckable() && m_iCheckImageWidth > 0)
			{
				// Non checkable items have to move over
				child->SetTextInset(m_iCheckImageWidth, 0);
			}
			else if (child->IsCheckable())
			{
				child->SetTextInset(0, 0); //TODO: for some reason I can't comment this out.
			}
		}
	}

	// set the width of the menu
	if (!m_iFixedWidth)
	{
		CalculateWidth();
	}
	else if (m_iFixedWidth)
	{
		_menuWide = m_iFixedWidth;
		// fixed width menus include the scroll bar in their width.
		if (_sizedForScrollBar)
		{
			_menuWide -= m_iProportionalScrollBarSize;
		}
	}

	SizeMenuItems();

	int extraWidth = 0;
	if (_sizedForScrollBar)
	{
		extraWidth = m_iProportionalScrollBarSize;
	}

	SetSize(_menuWide + extraWidth, menuTall + itop + ibottom);

	// if the menu has a parent menu item, position the menu relative to the parent
	MenuItem *parent = GetParentMenuItem();
	if (parent)
	{
		PositionCascadingMenu();
	}

	// position the scroll bar
	if (m_pScroller->IsVisible())
	{
		LayoutScrollBar();
	}

	for (int j = m_MenuItems.Head(); j != m_MenuItems.InvalidIndex(); j = m_MenuItems.Next(j))
	{
		m_MenuItems[j]->InvalidateLayout(); // cause each menu item to redo its apply settings now we have sized ourselves
	}

	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Force the menu to work out how wide it should be
//-----------------------------------------------------------------------------
void Menu::ForceCalculateWidth()
{
	_recalculateWidth = true;
	CalculateWidth();
	PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Figure out how wide the menu should be if the menu is not fixed width
//-----------------------------------------------------------------------------
void Menu::CalculateWidth()
{
	if (!_recalculateWidth)
		return;

	_menuWide = 0;
	if (!m_iFixedWidth)
	{
		// find the biggest menu item
		for (int i = m_MenuItems.Head(); i != m_MenuItems.InvalidIndex(); i = m_MenuItems.Next(i))
		{
			int wide, tall;
			m_MenuItems[i]->GetContentSize(wide, tall);
			// leave 8 pixels of padding
			if (wide > _menuWide - 8)
			{
				_menuWide = wide + 8;
			}
		}
	}

	// enfore a minimumWidth
	if (_menuWide < m_iMinimumWidth)
	{
		_menuWide = m_iMinimumWidth;
	}

	_recalculateWidth = false;
}

//-----------------------------------------------------------------------------
// Purpose: Set up the scroll bar attributes,size and location.
//-----------------------------------------------------------------------------
void Menu::LayoutScrollBar()
{
	//!! need to make it recalculate scroll positions
	m_pScroller->SetEnabled(false);
	m_pScroller->SetRangeWindow(m_iNumVisibleLines);
	m_pScroller->SetRange(0, m_MenuItems.Count());
	m_pScroller->SetButtonPressedScrollValue(1);

	int wide, tall;
	GetSize (wide, tall);

	// make sure we factor in insets
	int ileft, iright, itop, ibottom;
	GetInset(ileft, iright, itop, ibottom);

	// with a scroll bar we take off the inset
	wide -= iright;

	m_pScroller->SetPos(wide - m_iProportionalScrollBarSize, 1);

	// scrollbar is inside the menu's borders.
	m_pScroller->SetSize(m_iProportionalScrollBarSize, tall - ibottom - itop);
}

//-----------------------------------------------------------------------------
// Purpose: Figure out where to open menu if it is a cascading menu
//-----------------------------------------------------------------------------
void Menu::PositionCascadingMenu()
{
	Assert(GetVParent());
	int parentX, parentY, parentWide, parentTall;
	// move the menu to the correct place below the menuItem
	ipanel()->GetSize(GetVParent(), parentWide, parentTall);
	ipanel()->GetPos(GetVParent(), parentX, parentY);

	parentX += parentWide, parentY = 0;

	ParentLocalToScreen(parentX, parentY);

	SetPos(parentX, parentY);

	// for cascading menus,
	// make sure we're on the screen
	int workX, workY, workWide, workTall, x, y, wide, tall;
	GetBounds(x, y, wide, tall);
	surface()->GetWorkspaceBounds(workX, workY, workWide, workTall);

	if (x + wide > workX + workWide)
	{
		// we're off the right, move the menu to the left side
		// orignalX - width of the parentmenuitem - width of this menu.
		// add 2 pixels to offset one pixel onto the parent menu.
		x -= (parentWide + wide);
		x -= 2;
	}
	else
	{
		// alignment move it in the amount of the insets.
		x += 1;
	}

	// alignment move it in the amount of the insets.
	y -= 1;

	SetPos(x, y);

	MoveToFront();
}

//-----------------------------------------------------------------------------
// Purpose: Size the menu items so they are the width of the menu.
//          Also size the menu items with cascading menus so the arrow fits in there.
//-----------------------------------------------------------------------------
void Menu::SizeMenuItems()
{
	int ileft, iright, itop, ibottom;
	GetInset(ileft, iright, itop, ibottom);

	// assign the sizes of all the menu item panels
	for (int i = m_MenuItems.Head(); i != m_MenuItems.InvalidIndex(); i = m_MenuItems.Next(i))
	{
		MenuItem *child = m_MenuItems[i];
		if (child)
		{
			// labels do thier own sizing. this will size the label to the width of the menu,
			// this will put the cascading menu arrow on the right side automatically.
			child->SetSize(_menuWide - ileft - iright, m_iMenuItemHeight);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Makes menu items visible in relation to where the scroll bar is
//-----------------------------------------------------------------------------
void Menu::MakeItemsVisibleInScrollRange()
{
	// Detach all items from tree
	int count = 0;
	int startItem = m_pScroller->GetValue();
	do
	{
		int i;
		for (i = 0; i < startItem; i++)
		{
			m_MenuItems[m_SortedItems[i]]->SetVisible(false);
		}

		// Show items in scroll range
		for (i = startItem; count < m_iNumVisibleLines && i < m_SortedItems.Count(); i++)
		{
			m_MenuItems[m_SortedItems[i]]->SetVisible(true);
			count++;
		}

		for ( ; i < m_SortedItems.Count(); i++)
		{
			m_MenuItems[m_SortedItems[i]]->SetVisible(false);
		}

		if (count < m_iNumVisibleLines)
		{
			startItem--;
			count = 0;
		}
	}
	while (count < m_iNumVisibleLines - 1);
}

//-----------------------------------------------------------------------------
// Purpose: Get the approproate menu border
//-----------------------------------------------------------------------------
void Menu::LayoutMenuBorder()
{
	IBorder *menuBorder;
	IScheme *pScheme = scheme()->GetIScheme( GetScheme() );

	menuBorder = pScheme->GetBorder("MenuBorder");

	if ( menuBorder )
	{
		SetBorder(menuBorder);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw a black border on the right side of the menu items
//-----------------------------------------------------------------------------
void Menu::Paint()
{
	if ( m_pScroller->IsVisible() )
	{
		// draw black bar
		int wide, tall;
		GetSize (wide, tall);
		surface()->DrawSetColor(_borderDark);
		if( IsProportional() )
		{
			surface()->DrawFilledRect(wide - m_iProportionalScrollBarSize, -1, wide - m_iProportionalScrollBarSize + 1, tall);
		}
		else
		{
			surface()->DrawFilledRect(wide - m_iProportionalScrollBarSize, -1, wide - m_iProportionalScrollBarSize + 1, tall);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets the max number of items visible (scrollbar appears with more)
// Input  : numItems -
//-----------------------------------------------------------------------------
void Menu::SetNumberOfVisibleItems( int numItems )
{
	m_iNumVisibleLines = numItems;
	InvalidateLayout(false);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
MenuItem *Menu::GetMenuItem(int itemID)
{
	if ( !m_MenuItems.IsValidIndex(itemID) )
		return NULL;

	return m_MenuItems[itemID];
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool Menu::IsValidMenuID(int itemID)
{
	return m_MenuItems.IsValidIndex(itemID);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int Menu::GetInvalidMenuID()
{
	return m_MenuItems.InvalidIndex();
}

//-----------------------------------------------------------------------------
// Purpose: When a menuItem is selected, close cascading menus
//          if the menuItem selected has a cascading menu attached, we
//          want to keep that one open so skip it.
//          Passing NULL will close all cascading menus.
//-----------------------------------------------------------------------------
void Menu::CloseOtherMenus(MenuItem *item)
{
	for (int i = m_MenuItems.Head(); i != m_MenuItems.InvalidIndex(); i = m_MenuItems.Next(i))
	{
		if (m_MenuItems[i] == item)
			continue;

		m_MenuItems[i]->CloseCascadeMenu();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Respond to string commands.
//-----------------------------------------------------------------------------
void Menu::OnCommand( const char *command )
{
	// forward on the message
	PostActionSignal(new KeyValues("Command", "command", command));

	Panel::OnCommand(command);
}

//-----------------------------------------------------------------------------
// Purpose: Handle key presses, Activate shortcuts
//-----------------------------------------------------------------------------
void Menu::OnKeyCodeTyped(KeyCode code)
{
	// Don't allow key inputs when disabled!
	bool alt = (input()->IsKeyDown(KEY_LALT) || input()->IsKeyDown(KEY_RALT));
	if (alt)
	{
		BaseClass::OnKeyCodeTyped( code );
		PostActionSignal(new KeyValues("MenuClose"));
	}

	switch (code)
	{
	case KEY_ESCAPE:
		{
			// hide the menu on ESC
			SetVisible(false);
			break;
		}
	// arrow keys scroll through items on the list.
	// they should also scroll the scroll bar if needed
	case KEY_UP:
		{
			MoveAlongMenuItemList(MENU_UP, 0);
			m_MenuItems[m_iCurrentlySelectedItemID]->ArmItem();
			break;
		}
	case KEY_DOWN:
		{
			MoveAlongMenuItemList(MENU_DOWN, 0);
			m_MenuItems[m_iCurrentlySelectedItemID]->ArmItem();
			break;
		}
	// for menu items with submenus, right arrow opens the submenu
	case KEY_RIGHT:
		{
			// make sure a menuItem is currently selected
			if ( m_MenuItems.IsValidIndex(m_iCurrentlySelectedItemID) )
			{
				if (m_MenuItems[m_iCurrentlySelectedItemID]->HasMenu())
				{
					ActivateItem(m_iCurrentlySelectedItemID);
				}
				else
				{
					BaseClass::OnKeyCodeTyped( code );
				}
			}
			else
			{
				BaseClass::OnKeyCodeTyped( code );
			}
			break;
		}
	// left arrow closes the current menu
	case KEY_LEFT:
		{
			if (GetParentMenuItem())
			{
				SetVisible(false);
			}
			else
			{
				BaseClass::OnKeyCodeTyped( code );
			}
			break;
		}
	// enter key selects the menu item
	case KEY_ENTER:
		{
			// make sure a menuItem is currently selected
			if ( m_MenuItems.IsValidIndex(m_iCurrentlySelectedItemID) )
			{
				ActivateItem(m_iCurrentlySelectedItemID);
			}
			else
			{
				BaseClass::OnKeyCodeTyped( code ); // chain up
			}
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle key presses, Activate shortcuts
// Input  : code -
//-----------------------------------------------------------------------------
void Menu::OnKeyTyped(wchar_t unichar)
{
	// see if we can find a match
	int itemToSelect = m_iCurrentlySelectedItemID;
	if ( itemToSelect < 0 )
	{
		itemToSelect = 0;
	}

	int i;
	wchar_t menuItemName[255];

	i = itemToSelect + 1;
	if ( i >= m_MenuItems.Count() )
	{
		i = 0;
	}

	while ( i != itemToSelect )
	{
		m_MenuItems[i]->GetText(menuItemName, 254);

		if ( towlower(menuItemName[0]) == towlower(unichar) )
		{
			itemToSelect = i;
			break;
		}

		i++;
		if ( i >= m_MenuItems.Count() )
		{
			i = 0;
		}
	}

	if ( itemToSelect >= 0 )
	{
		SetCurrentlyHighlightedItem( itemToSelect );
		InvalidateLayout();
	}

	// don't chain back
}

//-----------------------------------------------------------------------------
// Purpose: Handle the mouse wheel event, scroll the selection
//-----------------------------------------------------------------------------
void Menu::OnMouseWheeled(int delta)
{
	if (!m_pScroller->IsVisible())
		return;

	int val = m_pScroller->GetValue();
	val -= delta;

	m_pScroller->SetValue(val);

	// moving the slider redraws the scrollbar,
	// and so we should redraw the menu since the
	// menu draws the black border to the right of the scrollbar.
	InvalidateLayout();

	// don't chain back
}

//-----------------------------------------------------------------------------
// Purpose: Lose focus, hide menu
//-----------------------------------------------------------------------------
void Menu::OnKillFocus()
{
	// check to see if it's a child taking it
	if (!input()->GetFocus() || !ipanel()->HasParent(input()->GetFocus(), GetVPanel()))
	{
		// if we don't accept keyboard input, then we have to ignore the killfocus if it's not actually being stolen
		if (!IsKeyBoardInputEnabled() && !input()->GetFocus())
			return;

		// get the parent of this menu.
		MenuItem *item = GetParentMenuItem();
		// if the parent is a menu item, this menu is a cascading menu
		// if the panel that is getting focus is the parent menu, don't close this menu.
		if ( (item) && (input()->GetFocus() == item->GetVParent()) )
		{
			// if we are in mouse mode and we clicked on the menuitem that
			// triggers the cascading menu, leave it open.
			if (m_iInputMode == MOUSE)
			{
				// return the focus to the cascading menu.
				MoveToFront();
				return;
			}
		}

		// forward the message to the parent.
		PostActionSignal(new KeyValues("MenuClose"));

		// hide this menu
		SetVisible(false);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set visibility of menu and its children as appropriate.
//-----------------------------------------------------------------------------
void Menu::SetVisible(bool state)
{
	if (state == IsVisible())
		return;

	if (IsVisible() && !state)
	{
		PostActionSignal(new KeyValues("MenuClose"));
		CloseOtherMenus(NULL);

		SetCurrentlySelectedItem(-1);
	}
	else if (state == true)
	{
		MoveToFront();
		RequestFocus();
	}

	BaseClass::SetVisible(state);
	_sizedForScrollBar = false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Menu::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetFgColor(GetSchemeColor("Menu/FgColor", pScheme));
	SetBgColor(GetSchemeColor("Menu/BgColor", pScheme));

	_borderDark = pScheme->GetColor("BorderDark", Color(255, 255, 255, 0));

	for (int i = m_MenuItems.Head(); i != m_MenuItems.InvalidIndex(); i = m_MenuItems.Next(i))
	{
		if( m_MenuItems[i]->IsCheckable() )
		{
			int wide, tall;
			m_MenuItems[i]->GetCheckImageSize( wide, tall );

			m_iCheckImageWidth = max ( m_iCheckImageWidth, wide );
		}
	}
	_recalculateWidth = true;
	CalculateWidth();

	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Menu::SetBgColor( Color newColor )
{
	BaseClass::SetBgColor( newColor );
	for (int i = m_MenuItems.Head(); i != m_MenuItems.InvalidIndex(); i = m_MenuItems.Next(i))
	{
		if( m_MenuItems[i]->HasMenu() )
		{
			m_MenuItems[i]->GetMenu()->SetBgColor( newColor );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Menu::SetFgColor( Color newColor )
{
	BaseClass::SetFgColor( newColor );
	for (int i = m_MenuItems.Head(); i != m_MenuItems.InvalidIndex(); i = m_MenuItems.Next(i))
	{
		if( m_MenuItems[i]->HasMenu() )
		{
			m_MenuItems[i]->GetMenu()->SetFgColor( newColor );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Menu::SetBorder(class IBorder *border)
{
	Panel::SetBorder(border);
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to a MenuItem that is this menus parent, if it has one
//-----------------------------------------------------------------------------
MenuItem *Menu::GetParentMenuItem()
{
	return dynamic_cast<MenuItem *>(GetParent());
}

//-----------------------------------------------------------------------------
// Purpose: Hide the menu when an item has been selected
//-----------------------------------------------------------------------------
void Menu::OnMenuItemSelected(Panel *panel)
{
	SetVisible(false);
	m_pScroller->SetVisible(false);

	// chain this message up through the hierarchy so
	// all the parent menus will close

	// get the parent of this menu.
	MenuItem *item = GetParentMenuItem();
	// if the parent is a menu item, this menu is a cascading menu
	if (item)
	{
		// get the parent of this menuitem, it is also a menu
		Menu *parentMenu = item->GetParentMenu();
		// send the message to this parent menu
		if (parentMenu)
		{
			KeyValues *kv = new KeyValues("MenuItemSelected");
			kv->SetPtr("panel", panel);

			ivgui()->PostMessage(parentMenu->GetVPanel(), kv, GetVPanel());
		}
	}

	bool activeItemSet = false;

	for (int i = m_MenuItems.Head(); i != m_MenuItems.InvalidIndex(); i = m_MenuItems.Next(i))
	{
		if( m_MenuItems[i] == panel )
		{
			activeItemSet = true;
			m_iActivatedItem = i;
			break;
		}
	}
	if( !activeItemSet )
	{
		for (int i = m_MenuItems.Head(); i != m_MenuItems.InvalidIndex(); i = m_MenuItems.Next(i))
		{
			if(m_MenuItems[i]->HasMenu() )
			{
				/*
				// GetActiveItem needs to return -1 or similar if it hasn't been set...
				if( m_MenuItems[i]->GetActiveItem() )
				{
					m_iActivatedItem = m_MenuItems[i]->GetActiveItem();
				}*/
			}
		}
	}

	// also pass it to the parent so they can respond if they like
	if (GetVParent())
	{
		KeyValues *kv = new KeyValues("MenuItemSelected");
		kv->SetPtr("panel", panel);

		ivgui()->PostMessage(GetVParent(), kv, GetVPanel());
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int Menu::GetActiveItem()
{
	return m_iActivatedItem;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
KeyValues *Menu::GetItemUserData(int itemID)
{
	if (m_MenuItems.IsValidIndex(itemID))
	{
		MenuItem *menuItem = dynamic_cast<MenuItem *>(m_MenuItems[itemID]);
		// make sure the menuItem is not disabled
		if (menuItem && menuItem->IsEnabled())
		{
			return menuItem->GetUserData();
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Activate the n'th item in the menu list, as if that menu item had been selected by the user
//-----------------------------------------------------------------------------
void Menu::ActivateItem(int itemID)
{
	if (m_MenuItems.IsValidIndex(itemID))
	{
		MenuItem *menuItem = dynamic_cast<MenuItem *>(m_MenuItems[itemID]);
		// make sure the menuItem is not disabled
		if (menuItem && menuItem->IsEnabled())
		{
			menuItem->FireActionSignal();
			m_iActivatedItem = itemID;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Menu::ActivateItemByRow(int row)
{
	if (m_SortedItems.IsValidIndex(row))
	{
		ActivateItem(m_SortedItems[row]);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return the number of items currently in the menu list
//-----------------------------------------------------------------------------
int Menu::GetItemCount()
{
	return m_MenuItems.Count();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int Menu::GetMenuID(int index)
{
	if ( !m_SortedItems.IsValidIndex(index) )
		return m_MenuItems.InvalidIndex();

	return m_SortedItems[index];
}

//-----------------------------------------------------------------------------
// Purpose: Return the number of items currently visible in the menu list
//-----------------------------------------------------------------------------
int Menu::GetCurrentlyVisibleItemsCount()
{
	if (m_MenuItems.Count() < m_iNumVisibleLines)
	{
		return m_MenuItems.Count();
	}
	return m_iNumVisibleLines;
}

//-----------------------------------------------------------------------------
// Purpose: Enables/disables choices in the list
//          itemName - string name of item in the list
//          state - true enables, false disables
//-----------------------------------------------------------------------------
void Menu::SetItemEnabled(const char *itemName, bool state)
{
	for (int i = m_MenuItems.Head(); i != m_MenuItems.InvalidIndex(); i = m_MenuItems.Next(i))
	{
		if ((stricmp(itemName, m_MenuItems[i]->GetName())) == 0)
		{
			dynamic_cast<MenuItem *>(m_MenuItems[i])->SetEnabled(state);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Enables/disables choices in the list
//-----------------------------------------------------------------------------
void Menu::SetItemEnabled(int itemID, bool state)
{
	if ( !m_MenuItems.IsValidIndex(itemID) )
		return;

	dynamic_cast<MenuItem *>(m_MenuItems[itemID])->SetEnabled(state);
}

//-----------------------------------------------------------------------------
// Purpose: Make the scroll bar visible and narrow the menu
//          also make items visible or invisible in the list as appropriate
//-----------------------------------------------------------------------------
void Menu::AddScrollBar()
{
	m_pScroller->SetVisible(true);
	_sizedForScrollBar = true;
}

//-----------------------------------------------------------------------------
// Purpose: Make the scroll bar invisible and widen the menu
//-----------------------------------------------------------------------------
void Menu::RemoveScrollBar()
{
	m_pScroller->SetVisible(false);
	_sizedForScrollBar = false;
}

//-----------------------------------------------------------------------------
// Purpose: Invalidate layout if the slider is moved so items scroll
//-----------------------------------------------------------------------------
void Menu::OnSliderMoved()
{
	CloseOtherMenus(NULL); // close any cascading menus

	// Invalidate so we redraw the menu!
	InvalidateLayout();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Toggle into mouse mode.
//-----------------------------------------------------------------------------
void Menu::OnCursorMoved(int x, int y)
{
	m_iInputMode = MOUSE;

	// chain up
	CallParentFunction(new KeyValues("OnCursorMoved", "x", x, "y", y));
	RequestFocus();
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Toggle into keyboard mode.
//-----------------------------------------------------------------------------
void Menu::OnKeyCodePressed(KeyCode code)
{
	m_iInputMode = KEYBOARD;
	// send the message to this parent in case this is a cascading menu
	if (GetVParent())
	{
		ivgui()->PostMessage(GetVParent(), new KeyValues("KeyModeSet"), GetVPanel());
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the item currently highlighted in the menu by ptr
//-----------------------------------------------------------------------------
void Menu::SetCurrentlySelectedItem(MenuItem *item)
{
	int itemNum = -1;
	// find it in our list of menuitems
	for (int i = m_MenuItems.Head(); i != m_MenuItems.InvalidIndex(); i = m_MenuItems.Next(i))
	{
		MenuItem *child = m_MenuItems[i];
		if (child == item)
		{
			itemNum = i;
			break;
		}
	}
	Assert( itemNum >= 0 );

	SetCurrentlySelectedItem(itemNum);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Menu::ClearCurrentlyHighlightedItem()
{
	if ( m_MenuItems.IsValidIndex(m_iCurrentlySelectedItemID) )
	{
		m_MenuItems[m_iCurrentlySelectedItemID]->DisarmItem();
	}
	m_iCurrentlySelectedItemID = m_MenuItems.InvalidIndex();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the item currently highlighted in the menu by index
//-----------------------------------------------------------------------------
void Menu::SetCurrentlySelectedItem(int itemID)
{
	// dont deselect if its the same item
	if (itemID == m_iCurrentlySelectedItemID)
		return;

	if ( m_MenuItems.IsValidIndex(m_iCurrentlySelectedItemID) )
	{
		m_MenuItems[m_iCurrentlySelectedItemID]->DisarmItem();
	}

	PostActionSignal(new KeyValues("MenuItemHighlight", "itemID", itemID));
	m_iCurrentlySelectedItemID = itemID;
}

//-----------------------------------------------------------------------------
// Purpose: This will set the item to be currenly selected and highlight it
//          will not open cascading menu. This was added for comboboxes
//          to have the combobox item highlighted in the menu when they open the
//          dropdown.
//-----------------------------------------------------------------------------
void Menu::SetCurrentlyHighlightedItem(int itemID)
{
	SetCurrentlySelectedItem(itemID);
	int row = m_SortedItems.Find(itemID);
	Assert( row != -1 );

	// if there is a scroll bar, and we scroll off lets move it.
	if ( m_pScroller->IsVisible() )
	{
		// now if we are off the scroll bar, it means we moved the scroll bar,
		// hide the menu, unhide the cursor, etc.
		if ( ( row > m_pScroller->GetValue() + m_iNumVisibleLines - 1 ) ||
			 ( row < m_pScroller->GetValue() ) )
		{
			if ( !m_pScroller->IsVisible() )
				return;

			m_pScroller->SetValue(row);
		}
	}

	if ( m_MenuItems.IsValidIndex(m_iCurrentlySelectedItemID) )
	{
		if ( !m_MenuItems[m_iCurrentlySelectedItemID]->IsArmed() )
		{
			m_MenuItems[m_iCurrentlySelectedItemID]->ArmItem();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int Menu::GetCurrentlyHighlightedItem()
{
	return m_iCurrentlySelectedItemID;
}

//-----------------------------------------------------------------------------
// Purpose: Respond to cursor entering a menuItem.
//-----------------------------------------------------------------------------
void Menu::OnCursorEnteredMenuItem(int VPanel)
{
	VPANEL menuItem = (VPANEL)VPanel;
	// if we are in mouse mode
	if (m_iInputMode == MOUSE)
	{
		MenuItem *item = static_cast<MenuItem *>(ipanel()->GetPanel(menuItem, GetModuleName()));
		// arm the menuItem
		item->ArmItem();
		// open the cascading menu
		item->OpenCascadeMenu();
		SetCurrentlySelectedItem(item);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Respond to cursor exiting a menuItem
//-----------------------------------------------------------------------------
void Menu::OnCursorExitedMenuItem(int VPanel)
{
	VPANEL menuItem = (VPANEL)VPanel;
	// only care if we are in mouse mode
	if (m_iInputMode == MOUSE)
	{
		MenuItem *item = static_cast<MenuItem *>(ipanel()->GetPanel(menuItem, GetModuleName()));
		// unhighlight the item.
		// note menuItems with cascading menus will stay lit.
		item->DisarmItem();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Move up or down one in the list of items in the menu
//          Direction is MENU_UP or MENU_DOWN
//-----------------------------------------------------------------------------
void Menu::MoveAlongMenuItemList(int direction, int loopCount)
{
	int itemID = m_iCurrentlySelectedItemID;
	int row = m_SortedItems.Find(itemID);
	row += direction;

	if ( row > m_SortedItems.Count() - 1 )
	{
		if ( m_pScroller->IsVisible() )
		{
			// stop at bottom of scrolled list
			row = m_SortedItems.Count() - 1;
		}
		else
		{
			// if no scroll bar we wrap around
			row = 0;
		}
	}
	else if (row < 0)
	{
		if ( m_pScroller->IsVisible() )
		{
			// stop at top of scrolled list
			row = m_pScroller->GetValue();
		}
		else
		{
			// if no scroll bar we wrap around
			row = m_SortedItems.Count() - 1;
		}
	}

	// if there is a scroll bar, and we scroll off lets move it.
	if ( m_pScroller->IsVisible() )
	{
		if ( row > m_pScroller->GetValue() + m_iNumVisibleLines - 1)
		{
			int val = m_pScroller->GetValue();
			val -= -direction;

			m_pScroller->SetValue(val);

			// moving the slider redraws the scrollbar,
			// and so we should redraw the menu since the
			// menu draws the black border to the right of the scrollbar.
			InvalidateLayout();
		}
		else if ( row < m_pScroller->GetValue() )
		{
			int val = m_pScroller->GetValue();
			val -= -direction;

			m_pScroller->SetValue(val);

			// moving the slider redraws the scrollbar,
			// and so we should redraw the menu since the
			// menu draws the black border to the right of the scrollbar.
			InvalidateLayout();
		}

		// now if we are still off the screen, it means we moved the scroll bar,
		// hide the menu, unhide the cursor, etc.
		if ( ( row > m_pScroller->GetValue() + m_iNumVisibleLines - 1 ) ||
			 ( row < m_pScroller->GetValue() ) )
		{
			// ok, move the scroll bar to match
			m_pScroller->SetValue(row);
		}
	}

	// set the newly selected item
	SetCurrentlySelectedItem(m_SortedItems[row]);

	// make sure the item is visible on the screen
	if ( loopCount < m_MenuItems.Count() )
	{
		// if the next item is a separator or invisible, skip it
		wchar_t text[256];
		m_MenuItems[m_iCurrentlySelectedItemID]->GetText(text, 255);
		if ( !text[0] || !m_MenuItems[m_iCurrentlySelectedItemID]->IsVisible() )
		{
			MoveAlongMenuItemList(direction, loopCount + 1);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return which type of events the menu is currently interested in
//          MenuItems need to know because behaviour is different depending on mode.
//-----------------------------------------------------------------------------
int Menu::GetMenuMode()
{
	return m_iInputMode;
}

//-----------------------------------------------------------------------------
// Purpose: Set the menu to key mode if a child menu goes into keymode
//-----------------------------------------------------------------------------
void Menu::OnKeyModeSet()
{
	m_iInputMode = KEYBOARD;
}

//-----------------------------------------------------------------------------
// Purpose: Set the checked state of a menuItem
//-----------------------------------------------------------------------------
void Menu::SetMenuItemChecked(int itemID, bool state)
{
	m_MenuItems[itemID]->SetChecked(state);
}

//-----------------------------------------------------------------------------
// Purpose: Check if item is checked.
//-----------------------------------------------------------------------------
bool Menu::IsChecked(int itemID)
{
	return m_MenuItems[itemID]->IsChecked();
}

//-----------------------------------------------------------------------------
// Purpose: Set the minmum width the menu has to be. This
//          is useful if you have a menu that is sized to the largest item in it
//          but you don't want the menu to be thinner than the menu button
//-----------------------------------------------------------------------------
void Menu::SetMinimumWidth(int width)
{
	m_iMinimumWidth = width;
}

//-----------------------------------------------------------------------------
// Purpose: Get the minmum width the menu
//-----------------------------------------------------------------------------
int Menu::GetMinimumWidth()
{
	return m_iMinimumWidth;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Menu::SetItemVisible(const char *itemName, bool state)
{
	for (int i = m_MenuItems.Head(); i != m_MenuItems.InvalidIndex(); i = m_MenuItems.Next(i))
	{
		if ((stricmp(itemName, m_MenuItems[i]->GetName())) == 0)
		{
			m_MenuItems[i]->SetVisible(state);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Menu::SetItemVisible(int itemID, bool state)
{
	if ( !m_MenuItems.IsValidIndex(itemID) )
		return;

	m_MenuItems[itemID]->SetVisible(state);
}

//-----------------------------------------------------------------------------
// Purpose: Static method to display a context menu
// Input  : *parent -
//			*menu -
//-----------------------------------------------------------------------------
void Menu::PlaceContextMenu( Panel *parent, Menu *menu )
{
	Assert( parent );
	Assert( menu );
	if ( !menu || !parent )
		return;

	menu->SetVisible(false);
	menu->SetParent( parent );
	menu->AddActionSignalTarget( parent );

	// get cursor position, this is local to this text edit window
	int cursorX, cursorY;
	input()->GetCursorPos(cursorX, cursorY);

	menu->SetVisible(true);

	// relayout the menu immediately so that we know it's size
	menu->InvalidateLayout(true);
	int menuWide, menuTall;
	menu->GetSize(menuWide, menuTall);

	// work out where the cursor is and therefore the best place to put the menu
	int wide, tall;
	surface()->GetScreenSize(wide, tall);

	if (wide - menuWide > cursorX)
	{
		// menu hanging right
		if (tall - menuTall > cursorY)
		{
			// menu hanging down
			menu->SetPos(cursorX, cursorY);
		}
		else
		{
			// menu hanging up
			menu->SetPos(cursorX, cursorY - menuTall);
		}
	}
	else
	{
		// menu hanging left
		if (tall - menuTall > cursorY)
		{
			// menu hanging down
			menu->SetPos(cursorX - menuWide, cursorY);
		}
		else
		{
			// menu hanging up
			menu->SetPos(cursorX - menuWide, cursorY - menuTall);
		}
	}

	menu->RequestFocus();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Menu::SetFont( HFont font )
{
	m_hItemFont = font;
	if ( font )
	{
		m_iMenuItemHeight = surface()->GetFontTall( font ) + 2;
	}
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Menu::SetUseFallbackFont( bool bState, HFont hFallback )
{
	m_hFallbackItemFont = hFallback;
	m_bUseFallbackFont = bState;
}
