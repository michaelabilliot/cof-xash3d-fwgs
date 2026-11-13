//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Handles the master server's version check reply: an out of date
//			client is told to update and quits. The class names come from the
//			RTTI; the file name is not recorded anywhere.
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "Socket_guess.h"
#include "GameUI_Interface.h"
#include "EngineInterface.h"

#undef PostMessage	// windows.h, from cdll_int.h

#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>

#include <vgui_controls/Controls.h>
#include <vgui_controls/MessageBox.h>

//-----------------------------------------------------------------------------
// Purpose: Message box that opens the update page and quits when closed
//-----------------------------------------------------------------------------
class QuitMessageBox : public vgui2::MessageBox
{
public:
	QuitMessageBox( const char *title, const char *text, vgui2::Panel *parent = NULL ) : MessageBox( title, text, parent )
	{
	}

	virtual void OnClose()
	{
		wchar_t *str = vgui2::localize()->Find( "#GameUI_OutOfDate_URL" );
		char url[128];
		vgui2::localize()->ConvertUnicodeToANSI( str, url, sizeof( url ) );
		vgui2::system()->ShellExecute( "open", url );
		engine->pfnClientCmd( "quit\n" );
	}
};

//-----------------------------------------------------------------------------
// Purpose: Master server version check reply
//-----------------------------------------------------------------------------
class CMasterVersionChkMsgHandler : public CMsgHandler
{
public:
	CMasterVersionChkMsgHandler( HANDLERTYPE type, void *typeinfo = NULL );

	virtual bool Process_guess( netadr_t *from, CMsgBuffer *msg );
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMasterVersionChkMsgHandler::CMasterVersionChkMsgHandler( HANDLERTYPE type, void *typeinfo ) : CMsgHandler( type, typeinfo )
{
}

//-----------------------------------------------------------------------------
// Purpose: Process the version check reply
//-----------------------------------------------------------------------------
bool CMasterVersionChkMsgHandler::Process_guess( netadr_t *from, CMsgBuffer *msg )
{
	// Skip the control character
	msg->ReadByte_guess();

	// Get the version status
	char *pVersionStatus = msg->ReadString_guess();
	pVersionStatus++;

	if ( pVersionStatus && !strnicmp( pVersionStatus, "outofdate", strlen( "outofdate" ) ) )
	{
		QuitMessageBox *box = new QuitMessageBox( "#GameUI_OutOfDate_Title", "#GameUI_OutOfDate_Msg" );
		GameUI().ActivateGameUI();
		vgui2::surface()->RestrictPaintToSinglePanel( box->GetVPanel() );
		box->DoModal();
	}

	return true;
}
