// traceinit.cpp -- tracks the pairing of subsystem init and shutdown calls

#include <string.h>

#include "UtlVector.h"

extern "C"
{
#include "quakedef.h"
#include "sys.h"
#include "traceinit.h"
}

class CInitTracker
{
public:
	enum
	{
		NUM_LISTS = 4,
	};

	class InitFunc
	{
	public:
		const char	*initname;
		const char	*shutdownname;
		int			referencecount;
		int			sequence;
		bool		warningprinted;
		// Profiling data
		double		inittime;
		double		shutdowntime;
	};

	CInitTracker( void );
	~CInitTracker( void );

	void	Init( const char *init, const char *shutdown, int listnum );
	void	Shutdown( const char *shutdown, int listnum );

private:
	int		m_nNumFuncs[ NUM_LISTS ];
	CUtlVector< InitFunc * > m_Funcs[ NUM_LISTS ];
};

static CInitTracker g_InitTracker;

CInitTracker::CInitTracker( void )
{
	for ( int l = 0; l < NUM_LISTS; l++ )
	{
		m_nNumFuncs[ l ] = 0;
	}
}

CInitTracker::~CInitTracker( void )
{
	for ( int l = 0; l < NUM_LISTS; l++ )
	{
		for ( int i = 0; i < m_nNumFuncs[ l ]; i++ )
		{
			InitFunc *f = m_Funcs[ l ][ i ];
			if ( f->referencecount )
			{
				Sys_Printf( "Missing shutdown function for %s : %s\n", f->initname, f->shutdownname );
			}
			delete f;
		}
		m_Funcs[ l ].RemoveAll();
		m_nNumFuncs[ l ] = 0;
	}
}

void CInitTracker::Init( const char *init, const char *shutdown, int listnum )
{
	InitFunc *f = new InitFunc;

	f->initname = init;
	f->shutdownname = shutdown;
	f->sequence = m_nNumFuncs[ listnum ];
	f->referencecount = 1;
	f->warningprinted = false;
	f->inittime = 0.0;
	f->shutdowntime = 0.0;

	m_Funcs[ listnum ].AddToHead( f );
	m_nNumFuncs[ listnum ]++;
}

void CInitTracker::Shutdown( const char *shutdown, int listnum )
{
	if ( !m_nNumFuncs[ listnum ] )
	{
		Sys_Printf( "Mismatched shutdown function %s\n", shutdown );
		return;
	}

	int i = 0;
	InitFunc *f = NULL;
	for ( i = 0; i < m_nNumFuncs[ listnum ]; i++ )
	{
		f = m_Funcs[ listnum ][ i ];
		if ( f->referencecount )
			break;
	}

	if ( f && f->referencecount && _stricmp( f->shutdownname, shutdown ) )
	{
		if ( !f->warningprinted )
		{
			f->warningprinted = true;
		}
	}

	for ( i = 0; i < m_nNumFuncs[ listnum ]; i++ )
	{
		InitFunc *f = m_Funcs[ listnum ][ i ];

		if ( !_stricmp( f->shutdownname, shutdown ) )
		{
			f->referencecount--;
			return;
		}
	}

	Sys_Printf( "Shutdown function %s not in list!!!\n", shutdown );
}

void TraceInit( const char *i, const char *s, int listnum )
{
	g_InitTracker.Init( i, s, listnum );
}

void TraceShutdown( const char *s, int listnum )
{
	g_InitTracker.Shutdown( s, listnum );
}
