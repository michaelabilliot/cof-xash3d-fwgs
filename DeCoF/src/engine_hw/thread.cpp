// thread.cpp -- the worker thread that runs the CD audio commands

#include <windows.h>
#include <string.h>

#include "cd_internal.h"
#include "ithread.h"

extern CCDAudio *GetInteralCDAudio( void );

typedef struct
{
	vfunc	pfn;
	int		param1;
	int		param2;
} threadcmds;

class CThread : public IThread
{
public:
	CThread( void );
	virtual			~CThread( void );

	virtual bool	Init( void );
	virtual bool	Shutdown( void );
	virtual bool	AddThreadItem( vfunc pfn, int param1, int param2 );

	HANDLE				m_hEvent_guess;
	CRITICAL_SECTION	m_CriticalSection_guess;

	volatile bool		m_bThreadRunning;
	volatile bool		m_bThreadStopped;

	// commands queued by the main thread and the copy the worker runs
	int					m_nCmds;
	int					_m_nCmds;
	threadcmds			m_rgCmds[ 11 ];
	threadcmds			_m_rgCmds[ 11 ];

	HANDLE				m_hThread_guess;
	DWORD				m_dwThreadID;
};

static CThread g_Thread;

IThread *thread = &g_Thread;

CThread::CThread( void )
{
	m_hEvent_guess = NULL;
	m_hThread_guess = NULL;
	m_dwThreadID = 0;
	m_bThreadRunning = false;
	m_bThreadStopped = false;
}

CThread::~CThread( void )
{
}

static DWORD WINAPI ThreadFunc( LPVOID pv )
{
	CThread *thread = (CThread *)pv;

	while ( thread->m_bThreadRunning )
	{
		WaitForSingleObject( thread->m_hEvent_guess, INFINITE );

		EnterCriticalSection( &thread->m_CriticalSection_guess );

		// take the queued commands
		memcpy( thread->_m_rgCmds, thread->m_rgCmds, thread->m_nCmds * sizeof( threadcmds ) );
		thread->_m_nCmds = thread->m_nCmds;
		thread->m_nCmds = 0;

		ResetEvent( thread->m_hEvent_guess );

		LeaveCriticalSection( &thread->m_CriticalSection_guess );

		for ( int i = 0; i < thread->_m_nCmds; i++ )
		{
			threadcmds *pt = &thread->_m_rgCmds[ i ];

			if ( pt->pfn )
			{
				( GetInteralCDAudio()->*pt->pfn )( pt->param1, pt->param2 );
			}
		}
	}

	thread->m_bThreadStopped = true;
	return 1;
}

bool CThread::Shutdown( void )
{
	if ( !m_hEvent_guess )
		return true;

	m_bThreadStopped = false;
	m_bThreadRunning = false;

	SetEvent( m_hEvent_guess );

	// give the thread a moment to finish
	int safeCount = 100;
	while ( !m_bThreadStopped && --safeCount > 0 )
	{
		Sleep( 1 );
	}

	CloseHandle( m_hEvent_guess );
	DeleteCriticalSection( &m_CriticalSection_guess );

	cdaudio->Shutdown();

	return true;
}

bool CThread::Init( void )
{
	InitializeCriticalSection( &m_CriticalSection_guess );

	m_hEvent_guess = CreateEvent( NULL, TRUE, FALSE, NULL );
	m_bThreadRunning = true;

	cdaudio->Init();

	m_hThread_guess = CreateThread( NULL, 0, ThreadFunc, this, 0, &m_dwThreadID );

	return m_hThread_guess ? true : false;
}

bool CThread::AddThreadItem( vfunc pfn, int param1, int param2 )
{
	bool fret = false;
	bool bFound = false;

	EnterCriticalSection( &m_CriticalSection_guess );

	for ( int i = 0; i < m_nCmds; i++ )
	{
		if ( m_rgCmds[ i ].pfn != pfn )
			continue;

		bFound = true;
		break;
	}

	if ( !bFound && m_nCmds < 10 )
	{
		m_rgCmds[ m_nCmds ].pfn = pfn;
		m_rgCmds[ m_nCmds ].param1 = param1;
		m_rgCmds[ m_nCmds ].param2 = param2;
		m_nCmds++;

		fret = true;
	}

	LeaveCriticalSection( &m_CriticalSection_guess );

	if ( fret )
	{
		SetEvent( m_hEvent_guess );
	}

	return fret;
}
