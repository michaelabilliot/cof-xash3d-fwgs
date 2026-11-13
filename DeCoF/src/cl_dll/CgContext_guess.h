// Cry of Fear Cg shader manager: the Cg context, the latest vertex and
// fragment profiles and a list of the programs loaded from cgshaders/.

#ifndef GUESS_CGCONTEXT_H
#define GUESS_CGCONTEXT_H

#include "TextureManager_guess.h"

//=========================================================
// Declarations this file takes from the NVIDIA Cg toolkit
// (Cg/cg.h, Cg/cgGL.h)
//=========================================================
typedef struct _CGcontext *CGcontext;
typedef struct _CGprogram *CGprogram;
typedef struct _CGparameter *CGparameter;

typedef enum
{
	CG_PROFILE_START = 6144,
	CG_PROFILE_UNKNOWN,
	CG_PROFILE_VP20,
	CG_PROFILE_FP20,
	CG_PROFILE_VP30,
	CG_PROFILE_FP30,
	CG_PROFILE_ARBVP1,
	CG_PROFILE_FP40,
	CG_PROFILE_ARBFP1 = 7000,
	CG_PROFILE_VP40 = 7001,
	CG_PROFILE_MAX = 7100,
} CGprofile;

typedef enum
{
	CG_UNKNOWN = 4096,
	CG_IN,
	CG_OUT,
	CG_INOUT,
	CG_MIXED,
	CG_VARYING,
	CG_UNIFORM,
	CG_CONSTANT,
	CG_PROGRAM_SOURCE,
	CG_PROGRAM_ENTRY,
	CG_COMPILED_PROGRAM,
	CG_PROGRAM_PROFILE,
	CG_GLOBAL,
	CG_PROGRAM,
	CG_DEFAULT,
	CG_ERROR,
	CG_SOURCE,
	CG_OBJECT,
} CGenum;

typedef int CGerror;

typedef enum
{
	CG_GL_MATRIX_IDENTITY = 0,
	CG_GL_MATRIX_TRANSPOSE = 1,
	CG_GL_MATRIX_INVERSE = 2,
	CG_GL_MATRIX_INVERSE_TRANSPOSE = 3,

	CG_GL_MODELVIEW_MATRIX,
	CG_GL_PROJECTION_MATRIX,
	CG_GL_TEXTURE_MATRIX,
	CG_GL_MODELVIEW_PROJECTION_MATRIX,

	CG_GL_VERTEX,
	CG_GL_FRAGMENT,
} CGGLenum;

extern "C"
{
CGcontext __cdecl cgCreateContext( void );
CGprogram __cdecl cgCreateProgramFromFile( CGcontext ctx, CGenum program_type, const char *program_file, CGprofile profile, const char *entry, const char **args );
CGerror __cdecl cgGetError( void );
const char * __cdecl cgGetErrorString( CGerror error );
CGparameter __cdecl cgGetNamedParameter( CGprogram prog, const char *name );

CGprofile __cdecl cgGLGetLatestProfile( CGGLenum profile_type );
void __cdecl cgGLSetOptimalOptions( CGprofile profile );
void __cdecl cgGLLoadProgram( CGprogram program );
void __cdecl cgGLEnableProfile( CGprofile profile );
void __cdecl cgGLDisableProfile( CGprofile profile );
void __cdecl cgGLBindProgram( CGprogram program );
void __cdecl cgGLSetParameter1f( CGparameter param, float x );
void __cdecl cgGLSetParameter4fv( CGparameter param, const float *v );
void __cdecl cgGLSetStateMatrixParameter( CGparameter param, CGGLenum matrix, CGGLenum transform );
}

// program types
#define CGTYPE_FRAGMENT_guess	1
#define CGTYPE_VERTEX_guess		2

typedef struct cgprogram_s_guess
{
	char		name_guess[32];
	int			type_guess;
	CGprogram	program_guess;
	struct cgprogram_s_guess *next_guess;
	struct cgprogram_s_guess *prev_guess;
} cgprogram_t_guess;

class CCgContext_guess
{
public:
	CCgContext_guess();
	~CCgContext_guess();

	cgprogram_t_guess *AddProgram_guess( const char *name, int type );
	cgprogram_t_guess *FindProgram_guess( const char *name );
	CGprofile GetFragmentProfile_guess( void );
	bool IsInitialized_guess( void );
	bool IsProgramValid_guess( const char *path, int type );
	cgprogram_t_guess *RemoveProgram_guess( cgprogram_t_guess *program );
	void Shutdown_guess( void );
	bool Startup_guess( void );

	void EnableVertexProfile_guess( void )
	{
		if ( g_TextureManager_guess.IsHardware_guess() )
			cgGLEnableProfile( m_vertexProfile_guess );
	}
	void EnableFragmentProfile_guess( void )
	{
		if ( g_TextureManager_guess.IsHardware_guess() )
			cgGLEnableProfile( m_fragmentProfile_guess );
	}
	void DisableVertexProfile_guess( void )
	{
		if ( g_TextureManager_guess.IsHardware_guess() )
			cgGLDisableProfile( m_vertexProfile_guess );
	}
	void DisableFragmentProfile_guess( void )
	{
		if ( g_TextureManager_guess.IsHardware_guess() )
			cgGLDisableProfile( m_fragmentProfile_guess );
	}
	void BindProgram_guess( cgprogram_t_guess *program )
	{
		if ( g_TextureManager_guess.IsHardware_guess() )
			cgGLBindProgram( program->program_guess );
	}

	CGcontext	m_context_guess;
	CGprofile	m_vertexProfile_guess;
	CGprofile	m_fragmentProfile_guess;
	cgprogram_t_guess *m_pPrograms_guess;
	bool		m_bInitialized_guess;
};

extern CCgContext_guess g_CgContext_guess;

#endif // GUESS_CGCONTEXT_H
