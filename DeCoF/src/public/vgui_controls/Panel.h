//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef PANEL_H
#define PANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui/Dar.h>
#include <vgui/MessageMap.h>
#include <vgui/KeyCode.h>
#include <vgui/MouseCode.h>
#include <vgui/IClientPanel.h>
#include <vgui/IScheme.h>
#include <Color.h>

class KeyValues;

namespace vgui2
{

class BuildGroup;
class IBorder;
class Tooltip;

//-----------------------------------------------------------------------------
// Purpose: For hudanimations.txt scripting of vars
//-----------------------------------------------------------------------------
template< class T >
inline T *SETUP_PANEL(T *panel)
{
	panel->MakeReadyForUse();
	return panel;
}

//-----------------------------------------------------------------------------
// Purpose: Base interface to all vgui windows
//			All vgui controls that receive message and/or have a physical presence
//			on screen are be derived from Panel.
//			This is designed as an easy-access to the vgui-functionality; for more
//			low-level access to vgui functions use the IPanel/IClientPanel interfaces directly
//-----------------------------------------------------------------------------
class Panel : public IClientPanel
{
	DECLARE_CLASS_SIMPLE_NOBASE( Panel );

public:
	// pin positions for auto-layout
	enum PinCorner_e
	{
		PIN_TOPLEFT = 0,
		PIN_TOPRIGHT,
		PIN_BOTTOMLEFT,
		PIN_BOTTOMRIGHT,
	};

	// specifies the auto-resize directions for the panel
	enum AutoResize_e
	{
		AUTORESIZE_NO = 0,
		AUTORESIZE_RIGHT,
		AUTORESIZE_DOWN,
		AUTORESIZE_DOWNANDRIGHT,
	};

	// constructors
	Panel();
	Panel( Panel *parent );
	Panel( Panel *parent, const char *panelName );
	Panel( Panel *parent, const char *panelName, HScheme scheme );
	virtual ~Panel();
	virtual VPANEL GetVPanel() { return _vpanel; }
	void SetName( const char *panelName );
	virtual const char *GetName();
	virtual const char *GetClassName();
	void MakeReadyForUse();
	void SetPos( int x, int y );
	void GetPos( int &x, int &y );
	void SetSize( int wide, int tall );
	void GetSize( int &wide, int &tall );
	void SetBounds( int x, int y, int wide, int tall );
	void GetBounds( int &x, int &y, int &wide, int &tall );
	int GetWide();
	void SetWide( int wide );
	int GetTall();
	void SetTall( int tall );
	void SetMinimumSize( int wide, int tall );
	void GetMinimumSize( int &wide, int &tall );
	bool IsBuildModeEditable();
	void SetBuildModeEditable( bool state );
	bool IsBuildModeDeletable();
	void SetBuildModeDeletable( bool state );
	bool IsBuildModeActive();
	void SetZPos( int z );
	virtual void SetVisible( bool state );
	virtual bool IsVisible();
	virtual VPANEL IsWithinTraverse( int x, int y, bool traversePopups );
	MESSAGE_FUNC( Repaint, "Repaint" );
	virtual void PostMessage( VPANEL target, KeyValues *message, float delaySeconds = 0.0f );
	bool IsWithin( int x, int y );
	void LocalToScreen( int &x, int &y );
	void ScreenToLocal( int &x, int &y );
	void ParentLocalToScreen( int &x, int &y );
	void MakePopup( bool showTaskbarIcon = true, bool disabled = false );
	virtual void OnMove();
	virtual Panel *GetParent();
	virtual VPANEL GetVParent();
	virtual void SetParent( Panel *newParent );
	virtual void SetParent( VPANEL newParent );
	virtual bool HasParent( VPANEL potentialParent );
	int GetChildCount();
	Panel *GetChild( int index );
	Panel *FindChildByName( const char *childName, bool recurseDown = false );
	Panel *FindSiblingByName( const char *siblingName );
	void CallParentFunction( KeyValues *message );
	virtual void SetAutoDelete( bool state );
	virtual bool IsAutoDeleteSet();
	virtual void DeletePanel();
	virtual void AddActionSignalTarget( Panel *messageTarget );
	virtual void AddActionSignalTarget( VPANEL messageTarget );
	virtual void RemoveActionSignalTarget( Panel *oldTarget );
	virtual void PostActionSignal( KeyValues *message );
	virtual bool RequestInfoFromChild( const char *childName, KeyValues *outputData );
	virtual void PostMessageToChild( const char *childName, KeyValues *message );
	virtual void PostMessage( Panel *target, KeyValues *message, float delay = 0.0f );
	virtual bool RequestInfo( KeyValues *outputData );
	virtual bool SetInfo( KeyValues *inputData );
	virtual void SetEnabled( bool state );
	virtual bool IsEnabled();
	virtual bool IsPopup();
	virtual void GetClipRect( int &x0, int &y0, int &x1, int &y1 );
	virtual void MoveToFront();
	void SetPinCorner( PinCorner_e pinCorner );
	PinCorner_e GetPinCorner();
	void SetAutoResize( AutoResize_e resizeDir );
	AutoResize_e GetAutoResize();
	virtual void SetBgColor( Color color );
	virtual void SetFgColor( Color color );
	virtual Color GetBgColor();
	virtual Color GetFgColor();
	virtual void SetCursor( HCursor cursor );
	virtual HCursor GetCursor();
	virtual void RequestFocus( int direction = 0 );
	virtual bool HasFocus();
	virtual void InvalidateLayout( bool layoutNow = false, bool reloadScheme = false );
	virtual bool RequestFocusPrev( VPANEL panel = NULL );
	virtual bool RequestFocusNext( VPANEL panel = NULL );
	virtual void SetTabPosition( int position );
	virtual int GetTabPosition();
	virtual void SetBorder( IBorder *border );
	virtual IBorder *GetBorder();
	virtual void SetPaintBorderEnabled( bool state );
	virtual void SetPaintBackgroundEnabled( bool state );
	virtual void SetPaintEnabled( bool state );
	virtual void SetPostChildPaintEnabled( bool state );
	virtual void GetInset( int &left, int &top, int &right, int &bottom );
	virtual void GetPaintSize( int &wide, int &tall );
	virtual void SetBuildGroup( BuildGroup *buildGroup );
	virtual bool IsBuildGroupEnabled();
	virtual bool IsCursorNone();
	virtual bool IsCursorOver();
	virtual void MarkForDeletion();
	virtual bool IsLayoutInvalid();
	virtual Panel *HasHotkey( wchar_t key );
	virtual bool IsOpaque();
	virtual HScheme GetScheme();
	virtual void SetScheme( const char *tag );
	virtual void SetScheme( HScheme scheme );
	virtual Color GetSchemeColor( const char *keyName, IScheme *pScheme );
	virtual Color GetSchemeColor( const char *keyName, Color defaultColor, IScheme *pScheme );
	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void ApplySettings( KeyValues *inResourceData );
	virtual void GetSettings( KeyValues *outResourceData );
	virtual const char *GetDescription();
	virtual const char *GetModuleName();
	virtual void ApplyUserConfigSettings( KeyValues *userConfig );
	virtual void GetUserConfigSettings( KeyValues *userConfig );
	virtual bool HasUserConfigSettings();
	virtual void OnThink();
	virtual void OnChildAdded( VPANEL child );
	virtual void OnSizeChanged( int newWide, int newTall );
	virtual void OnMessage( const KeyValues *params, VPANEL ifromPanel );
	MESSAGE_FUNC_CHARPTR( OnCommand, "Command", command );
	MESSAGE_FUNC( OnMouseCaptureLost, "MouseCaptureLost" );
	MESSAGE_FUNC( OnSetFocus, "SetFocus" );
	MESSAGE_FUNC( OnKillFocus, "KillFocus" );
	MESSAGE_FUNC( OnDelete, "Delete" );
	MESSAGE_FUNC( OnTick, "Tick" );
	MESSAGE_FUNC_INT_INT( OnCursorMoved, "OnCursorMoved", x, y );
	virtual void OnCursorEntered();
	virtual void OnCursorExited();
	virtual void OnMousePressed( MouseCode code );
	virtual void OnMouseDoublePressed( MouseCode code );
	virtual void OnMouseReleased( MouseCode code );
	virtual void OnMouseWheeled( int delta );
	virtual void OnKeyCodePressed( KeyCode code );
	virtual void OnKeyCodeTyped( KeyCode code );
	virtual void OnKeyTyped( wchar_t unichar );
	virtual void OnKeyCodeReleased( KeyCode code );
	virtual void OnKeyFocusTicked();
	MESSAGE_FUNC( OnMouseFocusTicked, "OnMouseFocusTicked" );
	virtual void PaintBackground();
	virtual void Paint();
	virtual void PaintBorder();
	virtual void PaintBuildOverlay();
	virtual void PostChildPaint();
	virtual void PerformLayout();
	DECLARE_PANELMAP();
	virtual VPANEL GetCurrentKeyFocus();
	Tooltip *GetTooltip();
	virtual bool IsProportional() { return m_bProportional; }
	virtual void SetProportional( bool state );
	virtual void SetMouseInputEnabled( bool state );
	virtual void SetKeyBoardInputEnabled( bool state );
	virtual bool IsMouseInputEnabled();
	virtual bool IsKeyBoardInputEnabled();
protected:
	MESSAGE_FUNC_HANDLE_HANDLE( OnRequestFocus, "OnRequestFocus", subFocus, defaultPanel );
	virtual void *QueryInterface( EInterfaceID id );
private:
	virtual Panel *GetPanel() { return this; }
	virtual void Think();
	virtual void PerformApplySchemeSettings();
	void InternalPerformLayout();
	void InternalSetCursor();
	MESSAGE_FUNC_INT_INT( InternalCursorMoved, "CursorMoved", xpos, ypos );
	MESSAGE_FUNC( InternalCursorEntered, "CursorEntered" );
	MESSAGE_FUNC( InternalCursorExited, "CursorExited" );
	MESSAGE_FUNC_INT( InternalMousePressed, "MousePressed", code );
	MESSAGE_FUNC_INT( InternalMouseDoublePressed, "MouseDoublePressed", code );
	MESSAGE_FUNC_INT( InternalMouseReleased, "MouseReleased", code );
	MESSAGE_FUNC_INT( InternalMouseWheeled, "MouseWheeled", delta );
	MESSAGE_FUNC_INT( InternalKeyCodePressed, "KeyCodePressed", code );
	MESSAGE_FUNC_INT( InternalKeyCodeTyped, "KeyCodeTyped", code );
	MESSAGE_FUNC_INT( InternalKeyTyped, "KeyTyped", unichar );
	MESSAGE_FUNC_INT( InternalKeyCodeReleased, "KeyCodeReleased", code );
	MESSAGE_FUNC( InternalKeyFocusTicked, "KeyFocusTicked" );
	MESSAGE_FUNC( InternalMouseFocusTicked, "MouseFocusTicked" );
	MESSAGE_FUNC( InternalInvalidateLayout, "Invalidate" );
	MESSAGE_FUNC( InternalMove, "Move" );
	virtual void InternalFocusChanged( bool lost );
	virtual void PaintTraverse( bool repaint, bool allowForce = true );
	void Init( int x, int y, int wide, int tall );
	void PreparePanelMap( PanelMap_t *panelMap );
	void OnOldMessage( KeyValues *params, VPANEL ifromPanel );
	VPANEL _vpanel;
	HCursor _cursor;
	bool _markedForDeletion;
	IBorder *_border;
	bool _needsRepaint;
	BuildGroup *_buildGroup;
	Color _fgColor;
	Color _bgColor;
	bool _paintBorderEnabled;
	bool _paintBackgroundEnabled;
	bool _paintEnabled;
	bool _postChildPaintEnabled;
	char *_panelName;
	bool _needsLayout;
	bool _needsSchemeUpdate;
	bool _autoDelete;
	int _tabPosition;
	Dar<unsigned long> _actionSignalTargetDar;
	PinCorner_e _pinCorner;
	AutoResize_e _autoResizeDirection;
	unsigned int m_iScheme;
	unsigned char _buildModeFlags;
	bool m_bProportional;
	bool m_bInPerformLayout;
	Tooltip *m_pTooltips;
};

} // namespace vgui2

#endif // PANEL_H
