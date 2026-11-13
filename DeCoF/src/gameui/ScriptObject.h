//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Option scripts (user.scr, settings.scr) and the controls built from them
//
// $NoKeywords: $
//=============================================================================

#ifndef SCRIPTOBJECT_H
#define SCRIPTOBJECT_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include "FileSystem.h"

struct CPanelListPanel;

namespace vgui2
{
class Label;
};

typedef enum
{
	O_BADTYPE,
	O_BOOL,
	O_NUMBER,
	O_LIST,
	O_STRING
} objtype_t;

typedef struct
{
	objtype_t type;
	char szDescription[32];
} objtypedesc_t;

class CScriptListItem
{
public:
	CScriptListItem();
	CScriptListItem( char const *strItem, char const *strValue );

	char szItemText[128];
	char szValue[256];

	CScriptListItem *pNext;
};

class CScriptObject : public vgui2::Panel
{
public:
	void AddItem( CScriptListItem *pItem );
	CScriptObject( void );
	~CScriptObject();

	bool ReadFromBuffer( char **pBuffer );
	void WriteToConfig();
	void WriteToFile( FileHandle_t fp );
	void WriteToScriptFile( FileHandle_t fp );
	void SetCurValue( char const *strValue );

	objtype_t GetType( char *pszType );

	objtype_t type;

	char cvarname[64];
	char prompt[256];

	CScriptListItem *pListItems;

	float fMin, fMax;

	char defValue[128];	// Default value string
	float fdefValue;	// Float version of default value.

	char curValue[128];
	float fcurValue;

	bool bSetInfo;	// Prepend "Setinfo" to keyvalue pair in config?
	// Linked list of default list box items.

	CScriptObject *pNext;
};

class CDescription
{
public:
	CDescription( CPanelListPanel *panel );
	~CDescription();

	bool ReadFromBuffer( char **pBuffer );
	virtual bool InitFromFile( char *pszFileName );
	void TransferCurrentValues( const char *pszConfigFile );

	void AddObject( CScriptObject *pItem );

	void WriteToConfig();
	void WriteToFile( FileHandle_t fp );
	void WriteToScriptFile( FileHandle_t fp );

	virtual void WriteScriptHeader( FileHandle_t fp ) = 0;	// Clients must override
	virtual void WriteFileHeader( FileHandle_t fp ) = 0;	// Clients must override

	void setDescription( const char *pszDesc );
	void setHint( const char *pszHint );

	const char *GetDescription( void ) { return m_pszDescriptionType; };
	const char *getHint( void ) { return m_pszHintText; };

public:
	CScriptObject *pObjList;

private:
	char *m_pszHintText;
	char *m_pszDescriptionType;

	CPanelListPanel *m_pListPanel;
};

class mpcontrol_t : public vgui2::Panel
{
public:
	mpcontrol_t( vgui2::Panel *parent, char const *panelName );

	virtual void OnSizeChanged( int wide, int tall );

	objtype_t type;
	vgui2::Panel *pControl;
	vgui2::Label *pPrompt;
	CScriptObject *pScrObj;

	mpcontrol_t *next;
};

class CInfoDescription : public CDescription
{
public:
	CInfoDescription( CPanelListPanel *panel );

	virtual bool InitFromFile( char *pszFileName );

	virtual void WriteScriptHeader( FileHandle_t fp );
	virtual void WriteFileHeader( FileHandle_t fp );
};

void UTIL_StripInvalidCharacters( char *pszInput );

#endif // SCRIPTOBJECT_H
