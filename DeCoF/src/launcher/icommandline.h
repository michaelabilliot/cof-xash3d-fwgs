//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef ICOMMANDLINE_H
#define ICOMMANDLINE_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Interface to engine command line
//-----------------------------------------------------------------------------
class ICommandLine
{
public:
	virtual void		CreateCmdLine( const char *commandline ) = 0;
	virtual const char	*GetCmdLine( void ) = 0;
	virtual char		*CheckParm( const char *psz, char **ppszValue = 0 ) = 0;

	virtual void		RemoveParm( const char *pszParm ) = 0;
	virtual void		AppendParm( const char *pszParm, const char *pszValues ) = 0;

	virtual void		SetParm( const char *pszParm, const char *pszValues ) = 0;
	virtual void		SetParm( const char *pszParm, int iValue ) = 0;
};

extern ICommandLine *cmdline;

#endif // ICOMMANDLINE_H
