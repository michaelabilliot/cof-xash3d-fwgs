// Cry of Fear Cg shader manager (no build records the name of this file)

#include <string>
#include <list>

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "studio_util.h"
#include "r_studioint.h"
#include "glmanager.h"
#include "CgContext_guess.h"

extern engine_studio_api_t IEngineStudio;

CCgContext_guess g_CgContext_guess;

CCgContext_guess::CCgContext_guess()
{
	m_pPrograms_guess = NULL;
	m_context_guess = NULL;
	m_bInitialized_guess = false;
}

CCgContext_guess::~CCgContext_guess()
{
	Shutdown_guess();
	m_context_guess = NULL;
	m_bInitialized_guess = false;
}

cgprogram_t_guess *CCgContext_guess::AddProgram_guess( const char *name, int type )
{
	if ( IEngineStudio.IsHardware() != 1 )
		return NULL;

	cgprogram_t_guess *program = FindProgram_guess( name );

	if ( !m_context_guess )
		Startup_guess();

	if ( !program )
	{
		char path[128];
		sprintf( path, "%s/cgshaders/%s", gEngfuncs.pfnGetGameDirectory(), name );

		if ( IsProgramValid_guess( path, type ) )
		{
			program = new cgprogram_t_guess;
			strcpy( program->name_guess, name );

			if ( type == CGTYPE_FRAGMENT_guess )
				program->program_guess = cgCreateProgramFromFile( m_context_guess, CG_SOURCE, path, m_fragmentProfile_guess, "main", NULL );
			else if ( type == CGTYPE_VERTEX_guess )
				program->program_guess = cgCreateProgramFromFile( m_context_guess, CG_SOURCE, path, m_vertexProfile_guess, "main", NULL );

			program->type_guess = type;
			program->prev_guess = NULL;
			program->next_guess = m_pPrograms_guess;
			if ( m_pPrograms_guess )
				m_pPrograms_guess->prev_guess = program;
			m_pPrograms_guess = program;

			cgGLLoadProgram( program->program_guess );
		}
	}

	return program;
}

cgprogram_t_guess *CCgContext_guess::FindProgram_guess( const char *name )
{
	for ( cgprogram_t_guess *program = m_pPrograms_guess; program; program = program->next_guess )
	{
		if ( !stricmp( name, program->name_guess ) )
			return program;
	}

	return NULL;
}

CGprofile CCgContext_guess::GetFragmentProfile_guess( void )
{
	return m_fragmentProfile_guess;
}

bool CCgContext_guess::IsInitialized_guess( void )
{
	return m_bInitialized_guess;
}

bool CCgContext_guess::IsProgramValid_guess( const char *path, int type )
{
	CGprogram program;

	if ( type == CGTYPE_FRAGMENT_guess )
		program = cgCreateProgramFromFile( m_context_guess, CG_SOURCE, path, m_fragmentProfile_guess, "main", NULL );
	else if ( type == CGTYPE_VERTEX_guess )
		program = cgCreateProgramFromFile( m_context_guess, CG_SOURCE, path, m_vertexProfile_guess, "main", NULL );

	if ( !program )
	{
		gEngfuncs.Con_Printf( "%s\n", cgGetErrorString( cgGetError() ) );
		return false;
	}

	return true;
}

cgprogram_t_guess *CCgContext_guess::RemoveProgram_guess( cgprogram_t_guess *program )
{
	cgprogram_t_guess *next = program->next_guess;

	if ( program == m_pPrograms_guess )
		m_pPrograms_guess = program->next_guess;

	if ( program->next_guess )
		program->next_guess->prev_guess = program->prev_guess;

	if ( program->prev_guess )
		program->prev_guess->next_guess = program->next_guess;

	delete program;

	return next;
}

void CCgContext_guess::Shutdown_guess( void )
{
	cgprogram_t_guess *program = m_pPrograms_guess;

	while ( program )
		program = RemoveProgram_guess( program );

	m_pPrograms_guess = NULL;
	m_bInitialized_guess = false;
}

bool CCgContext_guess::Startup_guess( void )
{
	bool result;

	m_context_guess = cgCreateContext();

	if ( m_context_guess )
	{
		m_vertexProfile_guess = cgGLGetLatestProfile( CG_GL_VERTEX );

		if ( m_vertexProfile_guess != CG_PROFILE_UNKNOWN )
		{
			cgGLSetOptimalOptions( m_vertexProfile_guess );
			m_fragmentProfile_guess = cgGLGetLatestProfile( CG_GL_FRAGMENT );

			if ( m_fragmentProfile_guess != CG_PROFILE_UNKNOWN )
			{
				cgGLSetOptimalOptions( m_fragmentProfile_guess );
				result = true;
			}
			else
				result = false;
		}
		else
			result = false;
	}
	else
		result = false;

	m_bInitialized_guess = result;
	return result;
}
