// sys_engine.cpp -- the engine's frame loop (IEngine)

#include <windows.h>

extern "C"
{
#include "quakedef.h"
#include "cvar.h"
#include "host.h"
#include "keys.h"
#include "sys.h"
#include "dll_state.h"

int		Sys_InitGame( char *lpOrgCmdLine, char *pBaseDir, void *pwnd, int bIsDedicated );
void	Sys_ShutdownGame( void );
void	ClientDLL_MouseEvent( int mstate );
}

#include "igame.h"
#include "iengine.h"
#include "cd.h"

class CEngine : public IEngine
{
public:
	CEngine( void );
	virtual			~CEngine( void );

	virtual bool	Load( bool dedicated, char *basedir, char *cmdline );
	virtual void	Unload( void );
	virtual void	SetState( int iState );
	virtual int		GetState( void );
	virtual void	SetSubState( int iSubState );
	virtual int		GetSubState( void );

	virtual int		Frame( void );
	virtual double	GetFrameTime( void );
	virtual double	GetCurTime( void );

	virtual void	TrapKey_Event( int key, bool down );
	virtual void	TrapMouse_Event( int buttons, bool down );

	virtual void	StartTrapMode( void );
	virtual bool	IsTrapping( void );
	virtual bool	CheckDoneTrapping( int &buttons, int &key );

	virtual int		GetQuitting( void );
	virtual void	SetQuitting( int quittype );

private:
	int		m_nQuitting;
	int		m_nDLLState;
	int		m_nSubState;

	double	m_fCurTime;
	double	m_fFrameTime;
	double	m_fOldTime;

	bool	m_bTrapMode;
	bool	m_bDoneTrapping;
	int		m_nTrapKey;
	int		m_nTrapButtons;
};

static CEngine g_Engine;

IEngine *eng = &g_Engine;

CEngine::CEngine( void )
{
	m_nSubState = 0;
	m_nDLLState = DLL_INACTIVE;
	m_fFrameTime = 0.0;
	m_fOldTime = 0.0;
	m_bTrapMode = false;
	m_bDoneTrapping = false;
	m_nTrapKey = 0;
	m_nTrapButtons = 0;
	m_nQuitting = QUIT_NOTQUITTING;
}

CEngine::~CEngine( void )
{
}

void CEngine::Unload( void )
{
	Sys_ShutdownGame();
	m_nDLLState = DLL_INACTIVE;
}

bool CEngine::Load( bool dedicated, char *basedir, char *cmdline )
{
	bool success = false;

	SetState( DLL_ACTIVE );

	bool bSetProcessor_guess = true;
	if ( COM_CheckParm( "-noaff" ) )
		bSetProcessor_guess = false;

	if ( bSetProcessor_guess )
	{
		DWORD dwAffinityMask_guess = 1;
		SetThreadAffinityMask( GetCurrentThread(), dwAffinityMask_guess );
	}

	if ( Sys_InitGame( cmdline, basedir, game->GetMainWindowAddress(), dedicated ) )
		success = true;

	if ( success )
		ForceReloadProfile();

	return success;
}

int CEngine::Frame( void )
{
	cdaudio->Frame();

	if ( !game->IsActiveApp() )
	{
		// sleep longer while minimized than while only out of focus
		game->SleepUntilInput( m_nDLLState != DLL_PAUSED ? 20 : 50 );
	}

	m_fCurTime = Sys_FloatTime();
	m_fFrameTime = m_fCurTime - m_fOldTime;
	m_fOldTime = m_fCurTime;

	if ( m_fFrameTime < 0.0 )
	{
		m_fFrameTime = 0.01f;
	}

	if ( m_nDLLState )
	{
		int dummy;
		int iState = Host_Frame( (float)m_fFrameTime, m_nDLLState, &dummy );

		if ( iState != m_nDLLState )
		{
			SetState( iState );

			switch ( m_nDLLState )
			{
			case DLL_CLOSE:
				SetQuitting( QUIT_TODESKTOP );
				break;
			case 5:		// DLL_RESTART of this engine build
				SetQuitting( QUIT_RESTART );
				break;
			}
		}
	}

	return m_nDLLState;
}

void CEngine::SetSubState( int iSubState )
{
	if ( iSubState != 1 )
	{
		GameSetSubState( iSubState );
	}
}

void CEngine::SetState( int iState )
{
	m_nDLLState = iState;
	GameSetState( iState );
}

int CEngine::GetState( void )
{
	return m_nDLLState;
}

int CEngine::GetSubState( void )
{
	return m_nSubState;
}

double CEngine::GetFrameTime( void )
{
	return m_fFrameTime;
}

double CEngine::GetCurTime( void )
{
	return m_fCurTime;
}

void CEngine::TrapKey_Event( int key, bool down )
{
	// Only key down events are trapped
	if ( m_bTrapMode && down )
	{
		m_bTrapMode = false;
		m_bDoneTrapping = true;

		m_nTrapKey = key;
		m_nTrapButtons = 0;
		return;
	}

	Key_Event( key, down ? 1 : 0 );
}

void CEngine::TrapMouse_Event( int buttons, bool down )
{
	// buttons == 0 for mouse move events
	if ( m_bTrapMode && buttons && !down )
	{
		m_bTrapMode = false;
		m_bDoneTrapping = true;

		m_nTrapKey = 0;
		m_nTrapButtons = buttons;
		return;
	}

	ClientDLL_MouseEvent( buttons );
}

void CEngine::StartTrapMode( void )
{
	if ( m_bTrapMode )
		return;

	m_bDoneTrapping = false;
	m_bTrapMode = true;
}

bool CEngine::IsTrapping( void )
{
	return m_bTrapMode;
}

bool CEngine::CheckDoneTrapping( int &buttons, int &key )
{
	if ( m_bTrapMode )
		return false;

	if ( !m_bDoneTrapping )
		return false;

	key = m_nTrapKey;
	buttons = m_nTrapButtons;
	m_bDoneTrapping = false;
	return true;
}

void CEngine::SetQuitting( int quittype )
{
	m_nQuitting = quittype;
}

int CEngine::GetQuitting( void )
{
	return m_nQuitting;
}
