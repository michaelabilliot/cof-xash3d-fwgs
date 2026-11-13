//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef MESSAGEMAP_H
#define MESSAGEMAP_H

#ifdef _WIN32
#pragma once
#endif

#include <UtlVector.h>

namespace vgui2
{

//-----------------------------------------------------------------------------
// Purpose: parameter data type enumeration
//			used internal but the shortcut macros require this to be exposed
//-----------------------------------------------------------------------------
enum DataType_t
{
	DATATYPE_VOID,
	DATATYPE_CONSTCHARPTR,
	DATATYPE_INT,
	DATATYPE_FLOAT,
	DATATYPE_PTR,
	DATATYPE_BOOL,
	DATATYPE_KEYVALUES,
	DATATYPE_CONSTWCHARPTR,
	DATATYPE_UINT64,
};

class Panel;

typedef void (Panel::*MessageFunc_t)(void);

//-----------------------------------------------------------------------------
// Purpose: Single item in a message map
//			Contains the information to map a string message name with parameters
//			to a function call
//-----------------------------------------------------------------------------
#pragma warning(disable:4121)
struct MessageMapItem_t
{
	const char *name;
	MessageFunc_t func;

	int numParams;

	DataType_t firstParamType;
	const char *firstParamName;

	DataType_t secondParamType;
	const char *secondParamName;

	int nameSymbol;
	int firstParamSymbol;
	int secondParamSymbol;
};

#define DECLARE_PANELMESSAGEMAP( className )																\
	static void AddToMap( char const *scriptname, vgui2::MessageFunc_t function, int paramCount, int p1type, const char *p1name, int p2type, const char *p2name ) \
	{																										\
		vgui2::PanelMessageMap *map = vgui2::FindOrAddPanelMessageMap( GetPanelClassName() );				\
																											\
		vgui2::MessageMapItem_t entry;																		\
		entry.name = scriptname;																			\
		entry.func = function;																				\
		entry.numParams = paramCount;																		\
		entry.firstParamType = (vgui2::DataType_t)p1type;													\
		entry.firstParamName = p1name;																		\
		entry.secondParamType = (vgui2::DataType_t)p2type;													\
		entry.secondParamName = p2name;																		\
		entry.nameSymbol = 0;																				\
		entry.firstParamSymbol = 0;																			\
		entry.secondParamSymbol = 0;																		\
																											\
		map->entries.AddToTail( entry );																	\
	}																										\
																											\
	static void ChainToMap( void )																			\
	{																										\
		static bool chained = false;																		\
		if ( chained )																						\
			return;																							\
		chained = true;																						\
		vgui2::PanelMessageMap *map = vgui2::FindOrAddPanelMessageMap( GetPanelClassName() );				\
		map->pfnClassName = &GetPanelClassName;																\
		if ( map && GetPanelBaseClassName() && GetPanelBaseClassName()[0] )								\
		{																									\
			map->baseMap = vgui2::FindOrAddPanelMessageMap( GetPanelBaseClassName() );						\
		}																									\
	}																										\
																											\
	class className##_RegisterMap;																			\
	friend class className##_RegisterMap;																	\
	class className##_RegisterMap																			\
	{																										\
	public:																									\
		className##_RegisterMap()																			\
		{																									\
			className::ChainToMap();																		\
		}																									\
	};																										\
	className##_RegisterMap m_RegisterClass;																\
																											\
	virtual vgui2::PanelMessageMap *GetMessageMap()														\
	{																										\
		static vgui2::PanelMessageMap *s_pMap = vgui2::FindOrAddPanelMessageMap( GetPanelClassName() );	\
		return s_pMap;																						\
	}

#define VGUI_USEKEYBINDINGMAPS 0

#define DECLARE_CLASS_SIMPLE( className, baseClassName ) \
	typedef baseClassName BaseClass; \
	typedef className ThisClass;	\
public:	\
	DECLARE_PANELMESSAGEMAP( className ); \
	static char const *GetPanelClassName() { return #className; } \
	static char const *GetPanelBaseClassName() { return #baseClassName; }

#define DECLARE_CLASS_SIMPLE_NOBASE( className ) \
	typedef className ThisClass;	\
public:	\
	DECLARE_PANELMESSAGEMAP( className ); \
	static char const *GetPanelClassName() { return #className; } \
	static char const *GetPanelBaseClassName() { return NULL; }

#define _MessageFuncCommon( name, scriptname, paramCount, p1type, p1name, p2type, p2name )	\
	class PanelMessageFunc_##name; \
	friend class PanelMessageFunc_##name; \
	class PanelMessageFunc_##name \
	{ \
	public: \
		static void InitVar() \
		{ \
			static bool bAdded = false; \
			if ( !bAdded ) \
			{ \
				bAdded = true; \
				AddToMap( scriptname, (vgui2::MessageFunc_t)(void (ThisClass::*)(void))&ThisClass::name, paramCount, p1type, p1name, p2type, p2name ); \
			} \
		} \
		PanelMessageFunc_##name() \
		{ \
			PanelMessageFunc_##name::InitVar(); \
		} \
	}; \
	PanelMessageFunc_##name m_##name##_register; \

// Use this macro to define a message mapped function
// must end with a semicolon ';', or with a function
// no parameter
#define MESSAGE_FUNC( name, scriptname )			_MessageFuncCommon( name, scriptname, 0, 0, 0, 0, 0 );	virtual void name( void )

// one parameter
#define MESSAGE_FUNC_INT( name, scriptname, p1 )	_MessageFuncCommon( name, scriptname, 1, vgui2::DATATYPE_INT, #p1, 0, 0 );	virtual void name( int p1 )
#define MESSAGE_FUNC_PTR( name, scriptname, p1 )	_MessageFuncCommon( name, scriptname, 1, vgui2::DATATYPE_PTR, #p1, 0, 0 );	virtual void name( vgui2::Panel *p1 )
#define MESSAGE_FUNC_ENUM( name, scriptname, t1, p1 )	_MessageFuncCommon( name, scriptname, 1, vgui2::DATATYPE_INT, #p1, 0, 0 );	virtual void name( t1 p1 )
#define MESSAGE_FUNC_FLOAT( name, scriptname, p1 )	_MessageFuncCommon( name, scriptname, 1, vgui2::DATATYPE_FLOAT, #p1, 0, 0 );	virtual void name( float p1 )
#define MESSAGE_FUNC_CHARPTR( name, scriptname, p1 )	_MessageFuncCommon( name, scriptname, 1, vgui2::DATATYPE_CONSTCHARPTR, #p1, 0, 0 );	virtual void name( const char *p1 )
#define MESSAGE_FUNC_WCHARPTR( name, scriptname, p1 )	_MessageFuncCommon( name, scriptname, 1, vgui2::DATATYPE_CONSTWCHARPTR, #p1, 0, 0 );	virtual void name( const wchar_t *p1 )

// two parameters
#define MESSAGE_FUNC_INT_INT( name, scriptname, p1, p2 )	_MessageFuncCommon( name, scriptname, 2, vgui2::DATATYPE_INT, #p1, vgui2::DATATYPE_INT, #p2 );	virtual void name( int p1, int p2 )
#define MESSAGE_FUNC_PTR_INT( name, scriptname, p1, p2 )	_MessageFuncCommon( name, scriptname, 2, vgui2::DATATYPE_PTR, #p1, vgui2::DATATYPE_INT, #p2 );	virtual void name( vgui2::Panel *p1, int p2 )
#define MESSAGE_FUNC_PTR_CHARPTR( name, scriptname, p1, p2 )	_MessageFuncCommon( name, scriptname, 2, vgui2::DATATYPE_PTR, #p1, vgui2::DATATYPE_CONSTCHARPTR, #p2 );	virtual void name( vgui2::Panel *p1, const char *p2 )
#define MESSAGE_FUNC_PTR_WCHARPTR( name, scriptname, p1, p2 )	_MessageFuncCommon( name, scriptname, 2, vgui2::DATATYPE_PTR, #p1, vgui2::DATATYPE_CONSTWCHARPTR, #p2 );	virtual void name( vgui2::Panel *p1, const wchar_t *p2 )
#define MESSAGE_FUNC_HANDLE_HANDLE( name, scriptname, p1, p2 )	_MessageFuncCommon( name, scriptname, 2, vgui2::DATATYPE_INT, #p1, vgui2::DATATYPE_INT, #p2 );	virtual void name( vgui2::VPANEL p1, vgui2::VPANEL p2 )

// unlimited parameters (passed in the whole KeyValues)
#define MESSAGE_FUNC_PARAMS( name, scriptname, p1 )	_MessageFuncCommon( name, scriptname, 1, vgui2::DATATYPE_KEYVALUES, NULL, 0, 0 );	virtual void name( KeyValues *p1 )

// mapping, one per class
struct PanelMessageMap
{
	PanelMessageMap()
	{
		baseMap = NULL;
		pfnClassName = NULL;
		processed = false;
	}

	CUtlVector< MessageMapItem_t > entries;
	bool processed;
	PanelMessageMap *baseMap;
	char const *(*pfnClassName)( void );
};

PanelMessageMap *FindPanelMessageMap( char const *className );
PanelMessageMap *FindOrAddPanelMessageMap( char const *className );

//-----------------------------------------------------------------------------
// Purpose: stores the list of messages a panel handles (old style message maps)
//-----------------------------------------------------------------------------
struct PanelMap_t
{
	MessageMapItem_t *dataDesc;
	int dataNumFields;
	const char *dataClassName;
	PanelMap_t *baseMap;
	int processed;
};

// for use in class declarations
// declares the static variables and functions needed for the data description iteration
#define DECLARE_PANELMAP() \
	static vgui2::PanelMap_t m_PanelMap; \
	static vgui2::MessageMapItem_t m_MessageMap[]; \
	virtual vgui2::PanelMap_t *GetPanelMap( void );

#ifndef ARRAYSIZE
#define ARRAYSIZE(p)		(sizeof(p)/sizeof(p[0]))
#endif

// could embed typeid() into here as well?
#define IMPLEMENT_PANELMAP( derivedClass, baseClass ) \
	vgui2::PanelMap_t derivedClass::m_PanelMap = { derivedClass::m_MessageMap, ARRAYSIZE(derivedClass::m_MessageMap), #derivedClass, &baseClass::m_PanelMap }; \
	vgui2::PanelMap_t *derivedClass::GetPanelMap( void ) { return &m_PanelMap; }

// no parameter
#define MAP_MESSAGE( type, name, func )						{ name, (vgui2::MessageFunc_t)(&type::func), 0 }

// one parameter
#define MAP_MESSAGE_PARAMS( type, name, func )				{ name, (vgui2::MessageFunc_t)(&type::func), 1, vgui2::DATATYPE_KEYVALUES, NULL }
#define MAP_MESSAGE_PTR( type, name, func, param1 )			{ name, (vgui2::MessageFunc_t)(&type::func), 1, vgui2::DATATYPE_PTR, param1 }
#define MAP_MESSAGE_INT( type, name, func, param1 )			{ name, (vgui2::MessageFunc_t)(&type::func), 1, vgui2::DATATYPE_INT, param1 }
#define MAP_MESSAGE_BOOL( type, name, func, param1 )		{ name, (vgui2::MessageFunc_t)(&type::func), 1, vgui2::DATATYPE_BOOL, param1 }
#define MAP_MESSAGE_FLOAT( type, name, func, param1 )		{ name, (vgui2::MessageFunc_t)(&type::func), 1, vgui2::DATATYPE_FLOAT, param1 }
#define MAP_MESSAGE_CONSTCHARPTR( type, name, func, param1)	{ name, (vgui2::MessageFunc_t)(&type::func), 1, vgui2::DATATYPE_CONSTCHARPTR, param1 }
#define MAP_MESSAGE_CONSTWCHARPTR( type, name, func, param1)	{ name, (vgui2::MessageFunc_t)(&type::func), 1, vgui2::DATATYPE_CONSTWCHARPTR, param1 }

// two parameters
#define MAP_MESSAGE_INT_INT( type, name, func, param1, param2 ) { name, (vgui2::MessageFunc_t)&type::func, 2, vgui2::DATATYPE_INT, param1, vgui2::DATATYPE_INT, param2 }
#define MAP_MESSAGE_PTR_INT( type, name, func, param1, param2 ) { name, (vgui2::MessageFunc_t)&type::func, 2, vgui2::DATATYPE_PTR, param1, vgui2::DATATYPE_INT, param2 }
#define MAP_MESSAGE_INT_CONSTCHARPTR( type, name, func, param1, param2 ) { name, (vgui2::MessageFunc_t)&type::func, 2, vgui2::DATATYPE_INT, param1, vgui2::DATATYPE_CONSTCHARPTR, param2 }
#define MAP_MESSAGE_PTR_CONSTCHARPTR( type, name, func, param1, param2 ) { name, (vgui2::MessageFunc_t)&type::func, 2, vgui2::DATATYPE_PTR, param1, vgui2::DATATYPE_CONSTCHARPTR, param2 }
#define MAP_MESSAGE_PTR_CONSTWCHARPTR( type, name, func, param1, param2 ) { name, (vgui2::MessageFunc_t)&type::func, 2, vgui2::DATATYPE_PTR, param1, vgui2::DATATYPE_CONSTWCHARPTR, param2 }
#define MAP_MESSAGE_CONSTCHARPTR_CONSTCHARPTR( type, name, func, param1, param2 ) { name, (vgui2::MessageFunc_t)&type::func, 2, vgui2::DATATYPE_CONSTCHARPTR, param1, vgui2::DATATYPE_CONSTCHARPTR, param2 }

} // namespace vgui2

#endif // MESSAGEMAP_H
