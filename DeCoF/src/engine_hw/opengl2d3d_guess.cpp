// opengl2d3d_guess.cpp -- the OpenGL to Direct3D wrapper of the Direct3D video mode
//
// The engine draws through the qgl function table; in the Direct3D mode QGL_D3DInit points that
// table at the gl* and wgl* functions below, which emulate the part of OpenGL that the renderer
// uses with DirectDraw 6 and Direct3D 6 (IDirectDraw4, IDirect3DDevice3, vertex buffers). The
// code follows Microsoft's OpenGL to Direct3D wrapper for Quake (its registry key
// "Software\Microsoft\Quake" and its "Wrapper: ..." messages), changed by Valve to use the engine
// allocator, the command line and the launcher's DirectDraw object. No string names the file or
// its functions: the names ending in _guess describe what the code does.

#define DIRECTDRAW_VERSION	0x0600
#define DIRECT3D_VERSION	0x0600
#define D3D_OVERLOADS

#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

extern "C"
{
#include "quakedef.h"
#include "cvar.h"
#include "mem.h"
#include "qgl.h"

extern cvar_t	gl_d3dflip;

// sys_getmodes.cpp
void	VideoMode_GetCurrentVideoMode( int *wide, int *tall, int *bpp );
}

#define GL_TEXTURE0_SGIS_guess		0x835E
#define GL_TEXTURE1_SGIS_guess		0x835F

#define MAXGLTEXHANDLES_guess		4096
#define VBNUMVERTS_guess			0x8000
#define MAXINDICES_guess			0x10000

//-----------------------------------------------------------------------------
// The DirectX manager: a COM object of a static library linked into the engine (it loads
// ddraw.dll, calls DirectDrawEnumerateExA and counts its users in "__DXMgrUsageCount__"). No
// header is known; the methods are named after what the wrapper does with them.
//-----------------------------------------------------------------------------
typedef struct DXMGRMODE_guess
{
	DWORD		dwWidth_guess;
	DWORD		dwHeight_guess;
	DWORD		dwBPP_guess;
	DWORD		dwRefreshRate_guess;
	DWORD		dwUnknown10_guess;
} DXMGRMODE_guess;

typedef struct DXMGRCREATE_guess
{
	DWORD		dwFlags_guess;
	DWORD		dwUnknown04_guess;
	HWND		hWnd_guess;
	DXMGRMODE_guess	mode_guess;
	GUID		guidDevice_guess;
	DWORD		dwUnknown30_guess[ 8 ];
	D3DVALUE	dvNear_guess;
	D3DVALUE	dvFar_guess;
} DXMGRCREATE_guess;

#define DXMGRCREATE_FULLSCREEN_guess	0x00000001
#define DXMGRCREATE_MODE_guess			0x00000002
#define DXMGRCREATE_DEFAULT_guess		0x00000018
#define DXMGRCREATE_DEVICE_guess		0x00000040

#undef INTERFACE
#define INTERFACE IDXEnum_guess
DECLARE_INTERFACE_( IDXEnum_guess, IUnknown )
{
	STDMETHOD( QueryInterface )( THIS_ REFIID riid, LPVOID *ppvObj ) PURE;
	STDMETHOD_( ULONG, AddRef )( THIS ) PURE;
	STDMETHOD_( ULONG, Release )( THIS ) PURE;
	STDMETHOD( Next )( THIS_ ULONG celt, LPVOID rgelt, ULONG *pceltFetched ) PURE;
	STDMETHOD( Skip )( THIS_ ULONG celt ) PURE;
	STDMETHOD( Reset )( THIS ) PURE;
	STDMETHOD( Clone )( THIS_ IDXEnum_guess **ppenum ) PURE;
};

#undef INTERFACE
#define INTERFACE IDXMgr_guess
DECLARE_INTERFACE_( IDXMgr_guess, IUnknown )
{
	STDMETHOD( QueryInterface )( THIS_ REFIID riid, LPVOID *ppvObj ) PURE;
	STDMETHOD_( ULONG, AddRef )( THIS ) PURE;
	STDMETHOD_( ULONG, Release )( THIS ) PURE;
	STDMETHOD( Unknown0C_guess )( THIS ) PURE;
	STDMETHOD( Unknown10_guess )( THIS ) PURE;
	STDMETHOD( EnumDisplayModes_guess )( THIS_ IDXEnum_guess **ppEnum, DWORD dwFlags ) PURE;
	STDMETHOD( EnumTextureFormats_guess )( THIS_ IDXEnum_guess **ppEnum, DWORD dwFlags ) PURE;
	STDMETHOD( Unknown1C_guess )( THIS ) PURE;
	STDMETHOD( Create_guess )( THIS_ DXMGRCREATE_guess *pCreate ) PURE;
	STDMETHOD( Unknown24_guess )( THIS ) PURE;
	STDMETHOD( Unknown28_guess )( THIS ) PURE;
	STDMETHOD( Unknown2C_guess )( THIS ) PURE;
	STDMETHOD( Unknown30_guess )( THIS ) PURE;
	STDMETHOD( Unknown34_guess )( THIS ) PURE;
	STDMETHOD( Unknown38_guess )( THIS ) PURE;
	STDMETHOD( Unknown3C_guess )( THIS ) PURE;
	STDMETHOD( GetDirectDraw_guess )( THIS_ LPDIRECTDRAW4 *ppDD ) PURE;
	STDMETHOD( GetPrimarySurface_guess )( THIS_ LPDIRECTDRAWSURFACE4 *ppDDS ) PURE;
	STDMETHOD( Unknown48_guess )( THIS ) PURE;
	STDMETHOD( Unknown4C_guess )( THIS ) PURE;
	STDMETHOD( Unknown50_guess )( THIS ) PURE;
	STDMETHOD( GetDevice_guess )( THIS_ LPDIRECT3DDEVICE3 *ppDevice ) PURE;
	STDMETHOD( Unknown58_guess )( THIS ) PURE;
	STDMETHOD( SetDirectDraw_guess )( THIS_ LPDIRECTDRAW4 pDD, HWND hWnd, DWORD dwFlags ) PURE;
	STDMETHOD( Unknown60_guess )( THIS ) PURE;
	STDMETHOD( Present_guess )( THIS_ DWORD dwFlags, DWORD dwReserved ) PURE;
};

extern "C" const IID	IID_IDXMgr_guess;
extern "C" HRESULT WINAPI DXMgrCreate_guess( REFIID riid, LPVOID *ppv );

//-----------------------------------------------------------------------------
// The wrapper's state
//-----------------------------------------------------------------------------
struct TexInfo_guess
{
	GLint				m_internalformat_guess;
	int					m_dwWidth_guess;
	int					m_dwHeight_guess;
	int					m_glWidth_guess;
	int					m_glHeight_guess;
	LPDIRECTDRAWSURFACE4	m_ddsurf_guess;
	D3DTEXTUREMINFILTER	m_minmode_guess;
	D3DTEXTUREMAGFILTER	m_magmode_guess;
	D3DTEXTUREMIPFILTER	m_mipmode_guess;
	D3DTEXTUREADDRESS	m_addu_guess;
	D3DTEXTUREADDRESS	m_addv_guess;
	LPDIRECT3DTEXTURE2	m_block_guess;
};

// the vertex format of the untransformed vertex buffer (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2)
struct Vertex_guess
{
	D3DVALUE	x, y, z;
	D3DCOLOR	color_guess;
	D3DVALUE	tu_guess, tv_guess;
	D3DVALUE	tu2_guess, tv2_guess;
};

template <class T> struct StackNode_guess
{
	T					m_data_guess;
	StackNode_guess<T>	*m_next_guess;

	StackNode_guess( StackNode_guess<T> **head, const T &data ) : m_data_guess( data )
	{
		m_next_guess = *head;
		*head = this;
	}
	~StackNode_guess() {}
	T &Data_guess() { return m_data_guess; }
	StackNode_guess<T> *Next_guess() { return m_next_guess; }
};

template <class T> class Stack_guess
{
public:
	int					m_count_guess;
	StackNode_guess<T>	*m_top_guess;

	Stack_guess() { m_count_guess = 0; m_top_guess = NULL; }
	~Stack_guess();
	Stack_guess<T> &Push_guess( const T &data );
};

// works on a stack through pointers to its members
template <class T> class StackLock_guess
{
public:
	int					*m_pCount_guess;
	StackNode_guess<T>	**m_ppTop_guess;

	StackLock_guess( Stack_guess<T> *s )
	{
		m_pCount_guess = &s->m_count_guess;
		m_ppTop_guess = &s->m_top_guess;
	}
	~StackLock_guess() {}
	StackNode_guess<T> *Top_guess() { return *m_ppTop_guess; }
	T &TopData_guess() { return ( *m_ppTop_guess )->Data_guess(); }
	void Pop_guess()
	{
		StackNode_guess<T> *node_guess = *m_ppTop_guess;
		*m_ppTop_guess = ( *m_ppTop_guess )->Next_guess();
		delete node_guess;
		( *m_pCount_guess )--;
	}
	void Push_guess( const T &data )
	{
		new StackNode_guess<T>( m_ppTop_guess, data );
		( *m_pCount_guess )++;
	}
};

template <class T> inline Stack_guess<T>::~Stack_guess()
{
	StackLock_guess<T> lock_guess( this );
	while ( lock_guess.Top_guess() )
		lock_guess.Pop_guess();
}

template <class T> inline Stack_guess<T> &Stack_guess<T>::Push_guess( const T &data )
{
	StackLock_guess<T> lock_guess( this );
	lock_guess.Push_guess( data );
	return *this;
}

struct Globals_guess
{
	int					m_vcnt_guess;
	int					m_pcnt_guess;
	int					m_icnt_guess;
	int					m_nfv_guess;
	int					m_winWidth_guess;
	int					m_winHeight_guess;
	HWND				m_hwnd_guess;
	HDC					m_hdc_guess;
	IDXMgr_guess		*m_pDXMgr_guess;
	DDPIXELFORMAT		m_ddFiveBitAlphaFormat_guess;
	DDPIXELFORMAT		m_ddFourBitAlphaFormat_guess;
	DDPIXELFORMAT		m_ddFiveBitFormat_guess;
	DDPIXELFORMAT		m_ddLuminanceFormat_guess;
	D3DDEVICEDESC		m_dd_guess;
	LPDIRECTDRAW4		m_pDD_guess;
	LPDIRECT3DDEVICE3	m_pD3DDev_guess;
	LPDIRECT3DDEVICE3	m_pD3DDevProcess_guess;
	LPDIRECT3DVIEWPORT3	m_pD3Dview_guess;
	LPDIRECT3DVERTEXBUFFER	m_xyzbuf_guess;
	LPDIRECT3DVERTEXBUFFER	m_tlbuf_guess;
	GLenum				m_cullMode_guess;
	D3DTRANSFORMSTATETYPE	m_matrixMode_guess;
	GLenum				m_prim_guess;
	D3DVALUE			m_zbias_guess;
	BOOL				m_cullEnabled_guess;
	BOOL				m_updatestage_guess;
	BOOL				m_texturing_guess;
	BOOL				m_subsample_guess;
	BOOL				m_mtex_guess;
	BOOL				m_usemtex_guess;
	BOOL				m_usemipmap_guess;
	BOOL				m_doFlip_guess;
	BOOL				m_makeSquare_guess;
	BOOL				m_fullscreen_guess;
	BOOL				m_use4444_guess;
	GLuint				m_curtgt_guess[ 2 ];
	int					m_curstage_guess;
	GLint				m_blendmode_guess[ 2 ];
	D3DCOLOR			m_color_guess;
	D3DCOLOR			m_clearColor_guess;
	D3DVALUE			m_tu_guess, m_tv_guess;
	D3DVALUE			m_tu2_guess, m_tv2_guess;
	const GLfloat		( *m_vertexary_guess )[ 4 ];
	const GLfloat		*m_colorary_guess;
	const GLfloat		*m_texcoordary_guess;
	GLsizei				m_vertexstride_guess;
	GLsizei				m_texcoordstride_guess;
	Vertex_guess		*m_verts_guess;
	Stack_guess<D3DMATRIX>	m_matrixStack_guess[ 2 ];
	DWORD				m_renderStates_guess[ 256 ];
	DWORD				m_textureStageStates_guess[ 2 ][ 24 ];
	LPDIRECT3DTEXTURE2	m_curTexture_guess[ 2 ];
	TexInfo_guess		m_tex_guess[ MAXGLTEXHANDLES_guess ];
	WORD				m_wIndices_guess[ MAXINDICES_guess ];
};

//-----------------------------------------------------------------------------
// Purpose: debug output with a line break
//-----------------------------------------------------------------------------
void DebugPrint_guess( const char *msg )
{
	OutputDebugString( msg );
	OutputDebugString( "\n" );
}

void APIENTRY glAccum (GLenum op, GLfloat value)
{
}

GLboolean APIENTRY glAreTexturesResident (GLsizei n, const GLuint *textures, GLboolean *residences)
{
	GLboolean result_guess = GL_FALSE;
	return result_guess;
}

void APIENTRY glBitmap (GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap)
{
}

void APIENTRY glCallList (GLuint list)
{
}

void APIENTRY glCallLists (GLsizei n, GLenum type, const GLvoid *lists)
{
}

void APIENTRY glClearAccum (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
}

void APIENTRY glClearDepth (GLclampd depth)
{
}

void APIENTRY glClearIndex (GLfloat c)
{
}

void APIENTRY glClearStencil (GLint s)
{
}

void APIENTRY glClipPlane (GLenum plane, const GLdouble *equation)
{
}

void APIENTRY glColor3b (GLbyte red, GLbyte green, GLbyte blue)
{
}

void APIENTRY glColor3bv (const GLbyte *v)
{
}

void APIENTRY glColor3d (GLdouble red, GLdouble green, GLdouble blue)
{
}

void APIENTRY glColor3dv (const GLdouble *v)
{
}

void APIENTRY glColor3fv (const GLfloat *v)
{
}

void APIENTRY glColor3i (GLint red, GLint green, GLint blue)
{
}

void APIENTRY glColor3iv (const GLint *v)
{
}

void APIENTRY glColor3s (GLshort red, GLshort green, GLshort blue)
{
}

void APIENTRY glColor3sv (const GLshort *v)
{
}

void APIENTRY glColor3ui (GLuint red, GLuint green, GLuint blue)
{
}

void APIENTRY glColor3uiv (const GLuint *v)
{
}

void APIENTRY glColor3us (GLushort red, GLushort green, GLushort blue)
{
}

void APIENTRY glColor3usv (const GLushort *v)
{
}

void APIENTRY glColor4b (GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha)
{
}

void APIENTRY glColor4bv (const GLbyte *v)
{
}

void APIENTRY glColor4d (GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha)
{
}

void APIENTRY glColor4dv (const GLdouble *v)
{
}

void APIENTRY glColor4i (GLint red, GLint green, GLint blue, GLint alpha)
{
}

void APIENTRY glColor4iv (const GLint *v)
{
}

void APIENTRY glColor4s (GLshort red, GLshort green, GLshort blue, GLshort alpha)
{
}

void APIENTRY glColor4sv (const GLshort *v)
{
}

void APIENTRY glColor4ui (GLuint red, GLuint green, GLuint blue, GLuint alpha)
{
}

void APIENTRY glColor4uiv (const GLuint *v)
{
}

void APIENTRY glColor4us (GLushort red, GLushort green, GLushort blue, GLushort alpha)
{
}

void APIENTRY glColor4usv (const GLushort *v)
{
}

void APIENTRY glColorMask (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
}

void APIENTRY glColorMaterial (GLenum face, GLenum mode)
{
}

void APIENTRY glCopyPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum type)
{
}

void APIENTRY glCopyTexImage1D (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border)
{
}

void APIENTRY glCopyTexImage2D (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
}

void APIENTRY glCopyTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
}

void APIENTRY glCopyTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
}

void APIENTRY glDeleteLists (GLuint list, GLsizei range)
{
}

void APIENTRY glDrawElements (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
}

void APIENTRY glDrawPixels (GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
}

void APIENTRY glEdgeFlag (GLboolean flag)
{
}

void APIENTRY glEdgeFlagPointer (GLsizei stride, const GLvoid *pointer)
{
}

void APIENTRY glEdgeFlagv (const GLboolean *flag)
{
}

void APIENTRY glEndList (void)
{
}

void APIENTRY glEvalCoord1d (GLdouble u)
{
}

void APIENTRY glEvalCoord1dv (const GLdouble *u)
{
}

void APIENTRY glEvalCoord1f (GLfloat u)
{
}

void APIENTRY glEvalCoord1fv (const GLfloat *u)
{
}

void APIENTRY glEvalCoord2d (GLdouble u, GLdouble v)
{
}

void APIENTRY glEvalCoord2dv (const GLdouble *u)
{
}

void APIENTRY glEvalCoord2f (GLfloat u, GLfloat v)
{
}

void APIENTRY glEvalCoord2fv (const GLfloat *u)
{
}

void APIENTRY glEvalMesh1 (GLenum mode, GLint i1, GLint i2)
{
}

void APIENTRY glEvalMesh2 (GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
}

void APIENTRY glEvalPoint1 (GLint i)
{
}

void APIENTRY glEvalPoint2 (GLint i, GLint j)
{
}

void APIENTRY glFeedbackBuffer (GLsizei size, GLenum type, GLfloat *buffer)
{
}

void APIENTRY glFinish (void)
{
}

void APIENTRY glFlush (void)
{
}

void APIENTRY glFogiv (GLenum pname, const GLint *params)
{
}

void APIENTRY glFrontFace (GLenum mode)
{
}

GLuint APIENTRY glGenLists (GLsizei range)
{
	GLuint result_guess = 0;
	return result_guess;
}

void APIENTRY glGenTextures (GLsizei n, GLuint *textures)
{
}

void APIENTRY glGetBooleanv (GLenum pname, GLboolean *params)
{
}

void APIENTRY glGetClipPlane (GLenum plane, GLdouble *equation)
{
}

void APIENTRY glGetDoublev (GLenum pname, GLdouble *params)
{
}

void APIENTRY glGetIntegerv (GLenum pname, GLint *params)
{
}

void APIENTRY glGetLightfv (GLenum light, GLenum pname, GLfloat *params)
{
}

void APIENTRY glGetLightiv (GLenum light, GLenum pname, GLint *params)
{
}

void APIENTRY glGetMapdv (GLenum target, GLenum query, GLdouble *v)
{
}

void APIENTRY glGetMapfv (GLenum target, GLenum query, GLfloat *v)
{
}

void APIENTRY glGetMapiv (GLenum target, GLenum query, GLint *v)
{
}

void APIENTRY glGetMaterialfv (GLenum face, GLenum pname, GLfloat *params)
{
}

void APIENTRY glGetMaterialiv (GLenum face, GLenum pname, GLint *params)
{
}

void APIENTRY glGetPixelMapfv (GLenum map, GLfloat *values)
{
}

void APIENTRY glGetPixelMapuiv (GLenum map, GLuint *values)
{
}

void APIENTRY glGetPixelMapusv (GLenum map, GLushort *values)
{
}

void APIENTRY glGetPointerv (GLenum pname, GLvoid* *params)
{
}

void APIENTRY glGetPolygonStipple (GLubyte *mask)
{
}

void APIENTRY glGetTexEnvfv (GLenum target, GLenum pname, GLfloat *params)
{
}

void APIENTRY glGetTexEnviv (GLenum target, GLenum pname, GLint *params)
{
}

void APIENTRY glGetTexGendv (GLenum coord, GLenum pname, GLdouble *params)
{
}

void APIENTRY glGetTexGenfv (GLenum coord, GLenum pname, GLfloat *params)
{
}

void APIENTRY glGetTexGeniv (GLenum coord, GLenum pname, GLint *params)
{
}

void APIENTRY glGetTexImage (GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels)
{
}

void APIENTRY glGetTexLevelParameterfv (GLenum target, GLint level, GLenum pname, GLfloat *params)
{
}

void APIENTRY glGetTexLevelParameteriv (GLenum target, GLint level, GLenum pname, GLint *params)
{
}

void APIENTRY glGetTexParameterfv (GLenum target, GLenum pname, GLfloat *params)
{
}

void APIENTRY glGetTexParameteriv (GLenum target, GLenum pname, GLint *params)
{
}

void APIENTRY glHint (GLenum target, GLenum mode)
{
}

void APIENTRY glIndexMask (GLuint mask)
{
}

void APIENTRY glIndexPointer (GLenum type, GLsizei stride, const GLvoid *pointer)
{
}

void APIENTRY glIndexd (GLdouble c)
{
}

void APIENTRY glIndexdv (const GLdouble *c)
{
}

void APIENTRY glIndexf (GLfloat c)
{
}

void APIENTRY glIndexfv (const GLfloat *c)
{
}

void APIENTRY glIndexi (GLint c)
{
}

void APIENTRY glIndexiv (const GLint *c)
{
}

void APIENTRY glIndexs (GLshort c)
{
}

void APIENTRY glIndexsv (const GLshort *c)
{
}

void APIENTRY glIndexub (GLubyte c)
{
}

void APIENTRY glIndexubv (const GLubyte *c)
{
}

void APIENTRY glInitNames (void)
{
}

void APIENTRY glInterleavedArrays (GLenum format, GLsizei stride, const GLvoid *pointer)
{
}

GLboolean APIENTRY glIsEnabled (GLenum cap)
{
	GLboolean result_guess = GL_FALSE;
	return result_guess;
}

GLboolean APIENTRY glIsList (GLuint list)
{
	GLboolean result_guess = GL_FALSE;
	return result_guess;
}

GLboolean APIENTRY glIsTexture (GLuint texture)
{
	GLboolean result_guess = GL_FALSE;
	return result_guess;
}

void APIENTRY glLightModelf (GLenum pname, GLfloat param)
{
}

void APIENTRY glLightModelfv (GLenum pname, const GLfloat *params)
{
}

void APIENTRY glLightModeli (GLenum pname, GLint param)
{
}

void APIENTRY glLightModeliv (GLenum pname, const GLint *params)
{
}

void APIENTRY glLightf (GLenum light, GLenum pname, GLfloat param)
{
}

void APIENTRY glLightfv (GLenum light, GLenum pname, const GLfloat *params)
{
}

void APIENTRY glLighti (GLenum light, GLenum pname, GLint param)
{
}

void APIENTRY glLightiv (GLenum light, GLenum pname, const GLint *params)
{
}

void APIENTRY glLineStipple (GLint factor, GLushort pattern)
{
}

void APIENTRY glLineWidth (GLfloat width)
{
}

void APIENTRY glListBase (GLuint base)
{
}

void APIENTRY glLoadMatrixd (const GLdouble *m)
{
}

void APIENTRY glLoadName (GLuint name)
{
}

void APIENTRY glLogicOp (GLenum opcode)
{
}

void APIENTRY glMap1d (GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points)
{
}

void APIENTRY glMap1f (GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points)
{
}

void APIENTRY glMap2d (GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points)
{
}

void APIENTRY glMap2f (GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points)
{
}

void APIENTRY glMapGrid1d (GLint un, GLdouble u1, GLdouble u2)
{
}

void APIENTRY glMapGrid1f (GLint un, GLfloat u1, GLfloat u2)
{
}

void APIENTRY glMapGrid2d (GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2)
{
}

void APIENTRY glMapGrid2f (GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2)
{
}

void APIENTRY glMaterialf (GLenum face, GLenum pname, GLfloat param)
{
}

void APIENTRY glMaterialfv (GLenum face, GLenum pname, const GLfloat *params)
{
}

void APIENTRY glMateriali (GLenum face, GLenum pname, GLint param)
{
}

void APIENTRY glMaterialiv (GLenum face, GLenum pname, const GLint *params)
{
}

void APIENTRY glMultMatrixd (const GLdouble *m)
{
}

void APIENTRY glMultMatrixf (const GLfloat *m)
{
}

void APIENTRY glNewList (GLuint list, GLenum mode)
{
}

void APIENTRY glNormal3b (GLbyte nx, GLbyte ny, GLbyte nz)
{
}

void APIENTRY glNormal3bv (const GLbyte *v)
{
}

void APIENTRY glNormal3d (GLdouble nx, GLdouble ny, GLdouble nz)
{
}

void APIENTRY glNormal3dv (const GLdouble *v)
{
}

void APIENTRY glNormal3f (GLfloat nx, GLfloat ny, GLfloat nz)
{
}

void APIENTRY glNormal3fv (const GLfloat *v)
{
}

void APIENTRY glNormal3i (GLint nx, GLint ny, GLint nz)
{
}

void APIENTRY glNormal3iv (const GLint *v)
{
}

void APIENTRY glNormal3s (GLshort nx, GLshort ny, GLshort nz)
{
}

void APIENTRY glNormal3sv (const GLshort *v)
{
}

void APIENTRY glNormalPointer (GLenum type, GLsizei stride, const GLvoid *pointer)
{
}

void APIENTRY glPassThrough (GLfloat token)
{
}

void APIENTRY glPixelMapfv (GLenum map, GLsizei mapsize, const GLfloat *values)
{
}

void APIENTRY glPixelMapuiv (GLenum map, GLsizei mapsize, const GLuint *values)
{
}

void APIENTRY glPixelMapusv (GLenum map, GLsizei mapsize, const GLushort *values)
{
}

void APIENTRY glPixelStoref (GLenum pname, GLfloat param)
{
}

void APIENTRY glPixelStorei (GLenum pname, GLint param)
{
}

void APIENTRY glPixelTransferf (GLenum pname, GLfloat param)
{
}

void APIENTRY glPixelTransferi (GLenum pname, GLint param)
{
}

void APIENTRY glPixelZoom (GLfloat xfactor, GLfloat yfactor)
{
}

void APIENTRY glPointSize (GLfloat size)
{
}

void APIENTRY glPolygonStipple (const GLubyte *mask)
{
}

void APIENTRY glPopAttrib (void)
{
}

void APIENTRY glPopClientAttrib (void)
{
}

void APIENTRY glPopName (void)
{
}

void APIENTRY glPrioritizeTextures (GLsizei n, const GLuint *textures, const GLclampf *priorities)
{
}

void APIENTRY glPushAttrib (GLbitfield mask)
{
}

void APIENTRY glPushClientAttrib (GLbitfield mask)
{
}

void APIENTRY glPushName (GLuint name)
{
}

void APIENTRY glRasterPos2d (GLdouble x, GLdouble y)
{
}

void APIENTRY glRasterPos2dv (const GLdouble *v)
{
}

void APIENTRY glRasterPos2f (GLfloat x, GLfloat y)
{
}

void APIENTRY glRasterPos2fv (const GLfloat *v)
{
}

void APIENTRY glRasterPos2i (GLint x, GLint y)
{
}

void APIENTRY glRasterPos2iv (const GLint *v)
{
}

void APIENTRY glRasterPos2s (GLshort x, GLshort y)
{
}

void APIENTRY glRasterPos2sv (const GLshort *v)
{
}

void APIENTRY glRasterPos3d (GLdouble x, GLdouble y, GLdouble z)
{
}

void APIENTRY glRasterPos3dv (const GLdouble *v)
{
}

void APIENTRY glRasterPos3f (GLfloat x, GLfloat y, GLfloat z)
{
}

void APIENTRY glRasterPos3fv (const GLfloat *v)
{
}

void APIENTRY glRasterPos3i (GLint x, GLint y, GLint z)
{
}

void APIENTRY glRasterPos3iv (const GLint *v)
{
}

void APIENTRY glRasterPos3s (GLshort x, GLshort y, GLshort z)
{
}

void APIENTRY glRasterPos3sv (const GLshort *v)
{
}

void APIENTRY glRasterPos4d (GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
}

void APIENTRY glRasterPos4dv (const GLdouble *v)
{
}

void APIENTRY glRasterPos4f (GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
}

void APIENTRY glRasterPos4fv (const GLfloat *v)
{
}

void APIENTRY glRasterPos4i (GLint x, GLint y, GLint z, GLint w)
{
}

void APIENTRY glRasterPos4iv (const GLint *v)
{
}

void APIENTRY glRasterPos4s (GLshort x, GLshort y, GLshort z, GLshort w)
{
}

void APIENTRY glRasterPos4sv (const GLshort *v)
{
}

void APIENTRY glReadBuffer (GLenum mode)
{
}

void APIENTRY glRectd (GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2)
{
}

void APIENTRY glRectdv (const GLdouble *v1, const GLdouble *v2)
{
}

void APIENTRY glRectf (GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
}

void APIENTRY glRectfv (const GLfloat *v1, const GLfloat *v2)
{
}

void APIENTRY glRecti (GLint x1, GLint y1, GLint x2, GLint y2)
{
}

void APIENTRY glRectiv (const GLint *v1, const GLint *v2)
{
}

void APIENTRY glRects (GLshort x1, GLshort y1, GLshort x2, GLshort y2)
{
}

void APIENTRY glRectsv (const GLshort *v1, const GLshort *v2)
{
}

GLint APIENTRY glRenderMode (GLenum mode)
{
	GLint result_guess = 0;
	return result_guess;
}

void APIENTRY glRotated (GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
}

void APIENTRY glScaled (GLdouble x, GLdouble y, GLdouble z)
{
}

void APIENTRY glScissor (GLint x, GLint y, GLsizei width, GLsizei height)
{
}

void APIENTRY glSelectBuffer (GLsizei size, GLuint *buffer)
{
}

void APIENTRY glStencilFunc (GLenum func, GLint ref, GLuint mask)
{
}

void APIENTRY glStencilMask (GLuint mask)
{
}

void APIENTRY glStencilOp (GLenum fail, GLenum zfail, GLenum zpass)
{
}

void APIENTRY glTexCoord1d (GLdouble s)
{
}

void APIENTRY glTexCoord1dv (const GLdouble *v)
{
}

void APIENTRY glTexCoord1f (GLfloat s)
{
}

void APIENTRY glTexCoord1fv (const GLfloat *v)
{
}

void APIENTRY glTexCoord1i (GLint s)
{
}

void APIENTRY glTexCoord1iv (const GLint *v)
{
}

void APIENTRY glTexCoord1s (GLshort s)
{
}

void APIENTRY glTexCoord1sv (const GLshort *v)
{
}

void APIENTRY glTexCoord2d (GLdouble s, GLdouble t)
{
}

void APIENTRY glTexCoord2dv (const GLdouble *v)
{
}

void APIENTRY glTexCoord2fv (const GLfloat *v)
{
}

void APIENTRY glTexCoord2i (GLint s, GLint t)
{
}

void APIENTRY glTexCoord2iv (const GLint *v)
{
}

void APIENTRY glTexCoord2s (GLshort s, GLshort t)
{
}

void APIENTRY glTexCoord2sv (const GLshort *v)
{
}

void APIENTRY glTexCoord3d (GLdouble s, GLdouble t, GLdouble r)
{
}

void APIENTRY glTexCoord3dv (const GLdouble *v)
{
}

void APIENTRY glTexCoord3f (GLfloat s, GLfloat t, GLfloat r)
{
}

void APIENTRY glTexCoord3fv (const GLfloat *v)
{
}

void APIENTRY glTexCoord3i (GLint s, GLint t, GLint r)
{
}

void APIENTRY glTexCoord3iv (const GLint *v)
{
}

void APIENTRY glTexCoord3s (GLshort s, GLshort t, GLshort r)
{
}

void APIENTRY glTexCoord3sv (const GLshort *v)
{
}

void APIENTRY glTexCoord4d (GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
}

void APIENTRY glTexCoord4dv (const GLdouble *v)
{
}

void APIENTRY glTexCoord4f (GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
}

void APIENTRY glTexCoord4fv (const GLfloat *v)
{
}

void APIENTRY glTexCoord4i (GLint s, GLint t, GLint r, GLint q)
{
}

void APIENTRY glTexCoord4iv (const GLint *v)
{
}

void APIENTRY glTexCoord4s (GLshort s, GLshort t, GLshort r, GLshort q)
{
}

void APIENTRY glTexCoord4sv (const GLshort *v)
{
}

void APIENTRY glTexEnvfv (GLenum target, GLenum pname, const GLfloat *params)
{
}

void APIENTRY glTexEnvi (GLenum target, GLenum pname, GLint param)
{
}

void APIENTRY glTexEnviv (GLenum target, GLenum pname, const GLint *params)
{
}

void APIENTRY glTexGend (GLenum coord, GLenum pname, GLdouble param)
{
}

void APIENTRY glTexGendv (GLenum coord, GLenum pname, const GLdouble *params)
{
}

void APIENTRY glTexGenf (GLenum coord, GLenum pname, GLfloat param)
{
}

void APIENTRY glTexGenfv (GLenum coord, GLenum pname, const GLfloat *params)
{
}

void APIENTRY glTexGeni (GLenum coord, GLenum pname, GLint param)
{
}

void APIENTRY glTexGeniv (GLenum coord, GLenum pname, const GLint *params)
{
}

void APIENTRY glTexImage1D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
}

void APIENTRY glTexParameterfv (GLenum target, GLenum pname, const GLfloat *params)
{
}

void APIENTRY glTexParameteri (GLenum target, GLenum pname, GLint param)
{
}

void APIENTRY glTexParameteriv (GLenum target, GLenum pname, const GLint *params)
{
}

void APIENTRY glTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels)
{
}

void APIENTRY glTranslated (GLdouble x, GLdouble y, GLdouble z)
{
}

void APIENTRY glVertex2d (GLdouble x, GLdouble y)
{
}

void APIENTRY glVertex2dv (const GLdouble *v)
{
}

void APIENTRY glVertex2fv (const GLfloat *v)
{
}

void APIENTRY glVertex2i (GLint x, GLint y)
{
}

void APIENTRY glVertex2iv (const GLint *v)
{
}

void APIENTRY glVertex2s (GLshort x, GLshort y)
{
}

void APIENTRY glVertex2sv (const GLshort *v)
{
}

void APIENTRY glVertex3d (GLdouble x, GLdouble y, GLdouble z)
{
}

void APIENTRY glVertex3dv (const GLdouble *v)
{
}

void APIENTRY glVertex3i (GLint x, GLint y, GLint z)
{
}

void APIENTRY glVertex3iv (const GLint *v)
{
}

void APIENTRY glVertex3s (GLshort x, GLshort y, GLshort z)
{
}

void APIENTRY glVertex3sv (const GLshort *v)
{
}

void APIENTRY glVertex4d (GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
}

void APIENTRY glVertex4dv (const GLdouble *v)
{
}

void APIENTRY glVertex4f (GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
}

void APIENTRY glVertex4fv (const GLfloat *v)
{
}

void APIENTRY glVertex4i (GLint x, GLint y, GLint z, GLint w)
{
}

void APIENTRY glVertex4iv (const GLint *v)
{
}

void APIENTRY glVertex4s (GLshort x, GLshort y, GLshort z, GLshort w)
{
}

void APIENTRY glVertex4sv (const GLshort *v)
{
}

BOOL WINAPI wglCopyContext( HGLRC hglrcSrc, HGLRC hglrcDst, UINT mask )
{
	BOOL result_guess = FALSE;
	return result_guess;
}

HGLRC WINAPI wglCreateLayerContext( HDC hdc, int iLayerPlane )
{
	return (HGLRC)1;
}

BOOL WINAPI wglDescribeLayerPlane( HDC hdc, int iPixelFormat, int iLayerPlane, UINT nBytes, LPLAYERPLANEDESCRIPTOR plpd )
{
	BOOL result_guess = FALSE;
	return result_guess;
}

extern "C" PROC WINAPI wglGetDefaultProcAddress_guess( void )
{
	PROC result_guess = NULL;
	return result_guess;
}

int WINAPI wglGetLayerPaletteEntries( HDC hdc, int iLayerPlane, int iStart, int cEntries, COLORREF *pcr )
{
	int result_guess = 0;
	return result_guess;
}

BOOL WINAPI wglRealizeLayerPalette( HDC hdc, int iLayerPlane, BOOL bRealize )
{
	BOOL result_guess = FALSE;
	return result_guess;
}

int WINAPI wglSetLayerPaletteEntries( HDC hdc, int iLayerPlane, int iStart, int cEntries, CONST COLORREF *pcr )
{
	int result_guess = 0;
	return result_guess;
}

BOOL WINAPI wglShareLists( HGLRC hglrc1, HGLRC hglrc2 )
{
	BOOL result_guess = FALSE;
	return result_guess;
}

BOOL WINAPI wglSwapLayerBuffers( HDC hdc, UINT fuPlanes )
{
	BOOL result_guess = FALSE;
	return result_guess;
}

BOOL WINAPI wglUseFontBitmapsA( HDC hdc, DWORD first, DWORD count, DWORD listBase )
{
	BOOL result_guess = FALSE;
	return result_guess;
}

BOOL WINAPI wglUseFontBitmapsW( HDC hdc, DWORD first, DWORD count, DWORD listBase )
{
	BOOL result_guess = FALSE;
	return result_guess;
}

BOOL WINAPI wglUseFontOutlinesA( HDC hdc, DWORD first, DWORD count, DWORD listBase, FLOAT deviation, FLOAT extrusion, int format, LPGLYPHMETRICSFLOAT lpgmf )
{
	BOOL result_guess = FALSE;
	return result_guess;
}

BOOL WINAPI wglUseFontOutlinesW( HDC hdc, DWORD first, DWORD count, DWORD listBase, FLOAT deviation, FLOAT extrusion, int format, LPGLYPHMETRICSFLOAT lpgmf )
{
	BOOL result_guess = FALSE;
	return result_guess;
}

Globals_guess	g_guess;

//-----------------------------------------------------------------------------
// Render state caches: a change flushes the queued primitives first
//-----------------------------------------------------------------------------
inline void ProcessVertices_guess( void )
{
	if ( g_guess.m_pcnt_guess != g_guess.m_vcnt_guess )
	{
		g_guess.m_tlbuf_guess->ProcessVertices( D3DVOP_TRANSFORM | D3DVOP_CLIP, g_guess.m_pcnt_guess, g_guess.m_vcnt_guess - g_guess.m_pcnt_guess, g_guess.m_xyzbuf_guess, g_guess.m_pcnt_guess, g_guess.m_pD3DDevProcess_guess, 0 );
		g_guess.m_pcnt_guess = g_guess.m_vcnt_guess;
	}
}

inline void FlushPrimitives_guess( void )
{
	DWORD size_guess;

	if ( g_guess.m_icnt_guess )
	{
		ProcessVertices_guess();
		g_guess.m_xyzbuf_guess->Unlock();
		g_guess.m_pD3DDev_guess->DrawIndexedPrimitiveVB( D3DPT_TRIANGLELIST, g_guess.m_tlbuf_guess, g_guess.m_wIndices_guess, g_guess.m_icnt_guess, D3DDP_DONOTUPDATEEXTENTS );
		g_guess.m_xyzbuf_guess->Lock( DDLOCK_WAIT | DDLOCK_WRITEONLY | DDLOCK_NOSYSLOCK, (LPVOID *)&g_guess.m_verts_guess, &size_guess );
		g_guess.m_pcnt_guess = 0;
		g_guess.m_vcnt_guess = 0;
		g_guess.m_icnt_guess = 0;
	}
}

inline void SetRenderState_guess( D3DRENDERSTATETYPE state, DWORD value )
{
	if ( g_guess.m_renderStates_guess[ state ] != value )
	{
		FlushPrimitives_guess();
		g_guess.m_renderStates_guess[ state ] = value;
		g_guess.m_pD3DDev_guess->SetRenderState( state, value );
	}
}

inline void SetTextureStageState_guess( DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value )
{
	if ( g_guess.m_textureStageStates_guess[ stage ][ type ] != value )
	{
		FlushPrimitives_guess();
		g_guess.m_textureStageStates_guess[ stage ][ type ] = value;
		g_guess.m_pD3DDev_guess->SetTextureStageState( stage, type, value );
	}
}

inline void SetTexture_guess( DWORD stage, LPDIRECT3DTEXTURE2 texture )
{
	if ( g_guess.m_curTexture_guess[ stage ] != texture )
	{
		FlushPrimitives_guess();
		g_guess.m_curTexture_guess[ stage ] = texture;
		g_guess.m_pD3DDev_guess->SetTexture( stage, texture );
	}
}

//-----------------------------------------------------------------------------
// Texture conversion: the pixels go through a canonical 32 bit RGBA image
//-----------------------------------------------------------------------------
void RawToCanon_guess( DWORD dwFormat, DWORD dwWidth, DWORD dwHeight, const void *lpPixels, DWORD *lpdwCanon )
{
	switch ( dwFormat )
	{
	case 1:
		{
			int count_guess = dwWidth * dwHeight;
			for ( int i = 0; i < count_guess; i++ )
			{
				DWORD t_guess = ( (const BYTE *)lpPixels )[ i ];
				lpdwCanon[ i ] = ( t_guess << 24 ) | ( t_guess << 16 ) | ( t_guess << 8 ) | t_guess;
			}
		}
		break;
	case 3:
	case 4:
	case GL_RGBA:
	case GL_RGB5:
	case GL_RGBA4:
	case GL_RGB5_A1:
	case GL_RGBA8:
		Q_memcpy( lpdwCanon, (void *)lpPixels, dwWidth * dwHeight * sizeof( DWORD ) );
		break;
	}
}

void Resize_guess( DWORD dwWidth, DWORD dwHeight, const DWORD *lpdwCanon, DWORD dwNewWidth, DWORD dwNewHeight, DWORD *lpdwNewCanon )
{
	double xscale_guess = (double)dwWidth / (double)dwNewWidth;
	double yscale_guess = (double)dwHeight / (double)dwNewHeight;

	for ( DWORD i = 0; i < dwNewHeight; i++ )
	{
		for ( DWORD j = 0; j < dwNewWidth; j++ )
		{
			lpdwNewCanon[ i * dwNewWidth + j ] = lpdwCanon[ (int)( i * yscale_guess ) * dwWidth + (int)( j * xscale_guess ) ];
		}
	}
}

void CanonTo8888_guess( const RECT &rect, const DWORD *lpdwCanon, const DDSURFACEDESC2 &ddsd )
{
	int		k_guess, col_guess, row_guess, w_guess;
	LPBYTE	pSurf_guess = (LPBYTE)ddsd.lpSurface;

	for ( row_guess = rect.top, k_guess = 0; row_guess < rect.bottom; row_guess++, pSurf_guess += ddsd.lPitch )
	{
		for ( col_guess = rect.left, w_guess = 0; col_guess < rect.right; col_guess++, k_guess++, w_guess++ )
		{
			( (LPDWORD)pSurf_guess )[ w_guess ] = ( ( lpdwCanon[ k_guess ] & ddsd.ddpfPixelFormat.dwRBitMask ) >> 16 ) | ( lpdwCanon[ k_guess ] & ddsd.ddpfPixelFormat.dwGBitMask ) | ( ( lpdwCanon[ k_guess ] & ddsd.ddpfPixelFormat.dwBBitMask ) << 16 ) | ( lpdwCanon[ k_guess ] & ddsd.ddpfPixelFormat.dwRGBAlphaBitMask );
		}
	}
}

void CanonTo565_guess( const RECT &rect, const DWORD *lpdwCanon, const DDSURFACEDESC2 &ddsd )
{
	int		k_guess, col_guess, row_guess, w_guess;
	LPBYTE	pSurf_guess = (LPBYTE)ddsd.lpSurface;

	for ( row_guess = rect.top, k_guess = 0; row_guess < rect.bottom; row_guess++, pSurf_guess += ddsd.lPitch )
	{
		for ( col_guess = rect.left, w_guess = 0; col_guess < rect.right; col_guess++, k_guess++, w_guess++ )
		{
			( (LPWORD)pSurf_guess )[ w_guess ] = (WORD)( ( ( lpdwCanon[ k_guess ] & 0xF8 ) << 8 ) | ( ( lpdwCanon[ k_guess ] & 0xFC00 ) >> 5 ) | ( ( lpdwCanon[ k_guess ] & 0xF80000 ) >> 19 ) );
		}
	}
}

void CanonTo555_guess( const RECT &rect, const DWORD *lpdwCanon, const DDSURFACEDESC2 &ddsd )
{
	int		k_guess, col_guess, row_guess, w_guess;
	LPBYTE	pSurf_guess = (LPBYTE)ddsd.lpSurface;

	for ( row_guess = rect.top, k_guess = 0; row_guess < rect.bottom; row_guess++, pSurf_guess += ddsd.lPitch )
	{
		for ( col_guess = rect.left, w_guess = 0; col_guess < rect.right; col_guess++, k_guess++, w_guess++ )
		{
			( (LPWORD)pSurf_guess )[ w_guess ] = (WORD)( ( ( lpdwCanon[ k_guess ] & 0xF8 ) << 7 ) | ( ( lpdwCanon[ k_guess ] & 0xF800 ) >> 6 ) | ( ( lpdwCanon[ k_guess ] & 0xF80000 ) >> 19 ) );
		}
	}
}

void CanonTo5551_guess( const RECT &rect, const DWORD *lpdwCanon, const DDSURFACEDESC2 &ddsd )
{
	int		k_guess, col_guess, row_guess, w_guess;
	LPBYTE	pSurf_guess = (LPBYTE)ddsd.lpSurface;

	for ( row_guess = rect.top, k_guess = 0; row_guess < rect.bottom; row_guess++, pSurf_guess += ddsd.lPitch )
	{
		for ( col_guess = rect.left, w_guess = 0; col_guess < rect.right; col_guess++, k_guess++, w_guess++ )
		{
			( (LPWORD)pSurf_guess )[ w_guess ] = (WORD)( ( ( lpdwCanon[ k_guess ] & 0xF8 ) << 7 ) | ( ( lpdwCanon[ k_guess ] & 0xF800 ) >> 6 ) | ( ( lpdwCanon[ k_guess ] & 0xF80000 ) >> 19 ) | ( ( lpdwCanon[ k_guess ] & 0x80000000 ) >> 16 ) );
		}
	}
}

void CanonTo4444_guess( const RECT &rect, const DWORD *lpdwCanon, const DDSURFACEDESC2 &ddsd )
{
	int		k_guess, col_guess, row_guess, w_guess;
	LPBYTE	pSurf_guess = (LPBYTE)ddsd.lpSurface;

	for ( row_guess = rect.top, k_guess = 0; row_guess < rect.bottom; row_guess++, pSurf_guess += ddsd.lPitch )
	{
		for ( col_guess = rect.left, w_guess = 0; col_guess < rect.right; col_guess++, k_guess++, w_guess++ )
		{
			( (LPWORD)pSurf_guess )[ w_guess ] = (WORD)( ( ( lpdwCanon[ k_guess ] & 0xF0 ) << 4 ) | ( ( lpdwCanon[ k_guess ] & 0xF000 ) >> 8 ) | ( ( lpdwCanon[ k_guess ] & 0xF00000 ) >> 20 ) | ( ( lpdwCanon[ k_guess ] & 0xF0000000 ) >> 16 ) );
		}
	}
}

void CanonTo8_guess( const RECT &rect, const DWORD *lpdwCanon, const DDSURFACEDESC2 &ddsd )
{
	int		k_guess, col_guess, row_guess, w_guess;
	LPBYTE	pSurf_guess = (LPBYTE)ddsd.lpSurface;

	for ( row_guess = rect.top, k_guess = 0; row_guess < rect.bottom; row_guess++, pSurf_guess += ddsd.lPitch )
	{
		for ( col_guess = rect.left, w_guess = 0; col_guess < rect.right; col_guess++, k_guess++, w_guess++ )
		{
			pSurf_guess[ w_guess ] = (BYTE)( lpdwCanon[ k_guess ] & 0xFF );
		}
	}
}

void LoadSurface_guess( LPDIRECTDRAWSURFACE4 lpDDS, DWORD dwFormat, DWORD dwWidth, DWORD dwHeight, DWORD dwNewWidth, DWORD dwNewHeight, const void *pixels )
{
	DDSURFACEDESC2	ddsd;
	HRESULT			ddrval;
	DWORD			*lpdwCanon, *lpdwNewCanon;
	RECT			rect;

	lpdwCanon = (DWORD *)Mem_Malloc( dwWidth * dwHeight * sizeof( DWORD ) );
	RawToCanon_guess( dwFormat, dwWidth, dwHeight, pixels, lpdwCanon );
	if ( dwWidth != dwNewWidth || dwHeight != dwNewHeight )
	{
		lpdwNewCanon = (DWORD *)Mem_Malloc( dwNewWidth * dwNewHeight * sizeof( DWORD ) );
		Resize_guess( dwWidth, dwHeight, lpdwCanon, dwNewWidth, dwNewHeight, lpdwNewCanon );
		Mem_Free( lpdwCanon );
	}
	else
	{
		lpdwNewCanon = lpdwCanon;
	}

	Q_memset( &ddsd, 0, sizeof( ddsd ) );
	ddsd.dwSize = sizeof( ddsd );
	ddrval = lpDDS->Lock( NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY | DDLOCK_NOSYSLOCK, NULL );
	if ( ddrval != DD_OK )
	{
		lpDDS->Release();
		OutputDebugString( "Lock failed while loading surface\n" );
		Mem_Free( lpdwNewCanon );
		return;
	}

	SetRect( &rect, 0, 0, ddsd.dwWidth, ddsd.dwHeight );
	if ( ddsd.ddpfPixelFormat.dwRBitMask == 0xFF )
		CanonTo8_guess( rect, lpdwNewCanon, ddsd );
	else if ( ddsd.ddpfPixelFormat.dwRGBAlphaBitMask == 0xF000 )
		CanonTo4444_guess( rect, lpdwNewCanon, ddsd );
	else if ( ddsd.ddpfPixelFormat.dwRGBAlphaBitMask == 0x8000 )
		CanonTo5551_guess( rect, lpdwNewCanon, ddsd );
	else if ( ddsd.ddpfPixelFormat.dwRBitMask == 0xF800 )
		CanonTo565_guess( rect, lpdwNewCanon, ddsd );
	else if ( ddsd.ddpfPixelFormat.dwRBitMask == 0xFF0000 )
		CanonTo8888_guess( rect, lpdwNewCanon, ddsd );
	else
		CanonTo555_guess( rect, lpdwNewCanon, ddsd );
	Mem_Free( lpdwNewCanon );
	lpDDS->Unlock( NULL );
}

HRESULT LoadSubSurface_guess( LPDIRECTDRAWSURFACE4 lpDDS, DWORD dwFormat, DWORD dwWidth, DWORD dwHeight, const void *pixels, RECT *subimage )
{
	DDSURFACEDESC2	ddsd;
	HRESULT			ddrval;
	DWORD			*lpdwCanon, *lpdwNewCanon;
	DWORD			dwNewWidth = subimage->right - subimage->left;
	DWORD			dwNewHeight = subimage->bottom - subimage->top;

	Q_memset( &ddsd, 0, sizeof( ddsd ) );
	ddsd.dwSize = sizeof( ddsd );
	ddrval = lpDDS->Lock( subimage, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY | DDLOCK_NOSYSLOCK, NULL );
	if ( ddrval != DD_OK )
	{
		lpDDS->Release();
		OutputDebugString( "Lock failed while loading surface\n" );
		return ddrval;
	}

	if ( dwFormat == 3 && ddsd.ddpfPixelFormat.dwRBitMask == 0xF800 && dwWidth == dwNewWidth && dwHeight == dwNewHeight )
	{
		CanonTo565_guess( *subimage, (const DWORD *)pixels, ddsd );
	}
	else if ( dwFormat == 3 && ddsd.ddpfPixelFormat.dwRBitMask == 0x7C00 && dwWidth == dwNewWidth && dwHeight == dwNewHeight )
	{
		CanonTo555_guess( *subimage, (const DWORD *)pixels, ddsd );
	}
	else
	{
		lpdwCanon = (DWORD *)Mem_Malloc( dwWidth * dwHeight * sizeof( DWORD ) );
		RawToCanon_guess( dwFormat, dwWidth, dwHeight, pixels, lpdwCanon );
		if ( dwWidth != dwNewWidth || dwHeight != dwNewHeight )
		{
			lpdwNewCanon = (DWORD *)Mem_Malloc( dwNewWidth * dwNewHeight * sizeof( DWORD ) );
			Resize_guess( dwWidth, dwHeight, lpdwCanon, dwNewWidth, dwNewHeight, lpdwNewCanon );
			Mem_Free( lpdwCanon );
		}
		else
		{
			lpdwNewCanon = lpdwCanon;
		}
		if ( ddsd.ddpfPixelFormat.dwRBitMask == 0xFF )
			CanonTo8_guess( *subimage, lpdwNewCanon, ddsd );
		else if ( ddsd.ddpfPixelFormat.dwRGBAlphaBitMask == 0xF000 )
			CanonTo4444_guess( *subimage, lpdwNewCanon, ddsd );
		else if ( ddsd.ddpfPixelFormat.dwRGBAlphaBitMask == 0x8000 )
			CanonTo5551_guess( *subimage, lpdwNewCanon, ddsd );
		else if ( ddsd.ddpfPixelFormat.dwRBitMask == 0xF800 )
			CanonTo565_guess( *subimage, lpdwNewCanon, ddsd );
		else if ( ddsd.ddpfPixelFormat.dwRBitMask == 0xFF0000 )
			CanonTo8888_guess( *subimage, lpdwNewCanon, ddsd );
		else
			CanonTo555_guess( *subimage, lpdwNewCanon, ddsd );
		Mem_Free( lpdwNewCanon );
	}
	lpDDS->Unlock( NULL );
	return DD_OK;
}

//-----------------------------------------------------------------------------
// OpenGL entry points
//-----------------------------------------------------------------------------
void APIENTRY glAlphaFunc (GLenum func, GLclampf ref)
{
	int cmpfunc_guess = -1;

	switch ( func )
	{
	case GL_NEVER:
		cmpfunc_guess = D3DCMP_NEVER;
		break;
	case GL_LESS:
		cmpfunc_guess = D3DCMP_LESS;
		break;
	case GL_EQUAL:
		cmpfunc_guess = D3DCMP_EQUAL;
		break;
	case GL_LEQUAL:
		cmpfunc_guess = D3DCMP_LESSEQUAL;
		break;
	case GL_GREATER:
		cmpfunc_guess = D3DCMP_GREATER;
		break;
	case GL_NOTEQUAL:
		cmpfunc_guess = D3DCMP_NOTEQUAL;
		break;
	case GL_GEQUAL:
		cmpfunc_guess = D3DCMP_GREATEREQUAL;
		break;
	case GL_ALWAYS:
		cmpfunc_guess = D3DCMP_ALWAYS;
		break;
	}
	if ( cmpfunc_guess >= 0 )
	{
		SetRenderState_guess( D3DRENDERSTATE_ALPHAFUNC, cmpfunc_guess );
		SetRenderState_guess( D3DRENDERSTATE_ALPHAREF, (DWORD)( ref * 255.0f ) );
	}
}

void APIENTRY glArrayElement (GLint i)
{
	static float two55_guess = 255.0f;
	Vertex_guess	*vert_guess = &g_guess.m_verts_guess[ g_guess.m_vcnt_guess + g_guess.m_nfv_guess ];
	const GLfloat	*v_guess = g_guess.m_vertexary_guess[ i ];
	const GLfloat	*c_guess;
	int				uRedByte_guess, uGreenByte_guess, uBlueByte_guess;

	vert_guess->x = v_guess[ 0 ];
	vert_guess->y = v_guess[ 1 ];
	vert_guess->z = v_guess[ 2 ];
	c_guess = &g_guess.m_colorary_guess[ i * 3 ];
	__asm
	{
		mov		ebx, c_guess
		fld		dword ptr [ ebx ]
		fld		dword ptr [ ebx + 4 ]
		fld		dword ptr [ ebx + 8 ]
		fld		two55_guess
		fmul	st( 1 ), st( 0 )
		fmul	st( 2 ), st( 0 )
		fmulp	st( 3 ), st( 0 )
		fistp	uBlueByte_guess
		fistp	uGreenByte_guess
		fistp	uRedByte_guess
		mov		eax, uBlueByte_guess
		cmp		eax, 255
		jle		blue_ok
		mov		eax, 255
blue_ok:
		mov		ebx, uGreenByte_guess
		cmp		ebx, 255
		jle		green_ok
		mov		ebx, 255
green_ok:
		mov		ecx, uRedByte_guess
		cmp		ecx, 255
		jle		red_ok
		mov		ecx, 255
red_ok:
		shl		ebx, 8
		shl		ecx, 16
		or		eax, ebx
		or		ecx, 0xFF000000
		or		eax, ecx
		mov		ebx, vert_guess
		mov		dword ptr [ ebx ]Vertex_guess.color_guess, eax
	}
	vert_guess->tu_guess = g_guess.m_tu_guess;
	vert_guess->tv_guess = g_guess.m_tv_guess;
	if ( g_guess.m_mtex_guess )
	{
		vert_guess->tu2_guess = g_guess.m_tu2_guess;
		vert_guess->tv2_guess = g_guess.m_tv2_guess;
	}
	g_guess.m_nfv_guess++;
}


void APIENTRY glBegin (GLenum mode)
{
	g_guess.m_prim_guess = mode;
	g_guess.m_nfv_guess = 0;
	if ( g_guess.m_texturing_guess == TRUE && g_guess.m_updatestage_guess == FALSE )
	{
		TexInfo_guess		&texInfo_guess = g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ 0 ] ];
		LPDIRECT3DTEXTURE2	block_guess = texInfo_guess.m_block_guess;

		SetTextureStageState_guess( 0, D3DTSS_ADDRESSU, texInfo_guess.m_addu_guess );
		SetTextureStageState_guess( 0, D3DTSS_ADDRESSV, texInfo_guess.m_addv_guess );
		SetTextureStageState_guess( 0, D3DTSS_MAGFILTER, texInfo_guess.m_magmode_guess );
		SetTextureStageState_guess( 0, D3DTSS_MINFILTER, texInfo_guess.m_minmode_guess );
		SetTextureStageState_guess( 0, D3DTSS_MIPFILTER, texInfo_guess.m_mipmode_guess );
		SetTexture_guess( 0, block_guess );
		switch ( g_guess.m_blendmode_guess[ 0 ] )
		{
		case GL_REPLACE:
			switch ( texInfo_guess.m_internalformat_guess )
			{
			case 1:
			case 3:
				SetTextureStageState_guess( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
				SetTextureStageState_guess( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
				SetTextureStageState_guess( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 );
				break;
			case 4:
			case GL_RGBA:
			case GL_RGB5:
			case GL_RGBA4:
			case GL_RGB5_A1:
			case GL_RGBA8:
				SetTextureStageState_guess( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
				SetTextureStageState_guess( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
				SetTextureStageState_guess( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
				break;
			}
			break;
		case GL_MODULATE:
			switch ( texInfo_guess.m_internalformat_guess )
			{
			case 1:
			case 3:
				SetTextureStageState_guess( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
				SetTextureStageState_guess( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
				SetTextureStageState_guess( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 );
				break;
			case 4:
			case GL_RGBA:
			case GL_RGB5:
			case GL_RGBA4:
			case GL_RGB5_A1:
			case GL_RGBA8:
				SetTextureStageState_guess( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
				SetTextureStageState_guess( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
				SetTextureStageState_guess( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
				break;
			}
			break;
		case GL_DECAL:
			switch ( texInfo_guess.m_internalformat_guess )
			{
			case 3:
				SetTextureStageState_guess( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
				SetTextureStageState_guess( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
				SetTextureStageState_guess( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 );
				break;
			case 4:
			case GL_RGBA:
			case GL_RGB5:
			case GL_RGBA4:
			case GL_RGB5_A1:
			case GL_RGBA8:
				SetTextureStageState_guess( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
				SetTextureStageState_guess( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
				SetTextureStageState_guess( 0, D3DTSS_COLOROP, D3DTOP_BLENDTEXTUREALPHA );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 );
				break;
			}
			break;
		case GL_BLEND:
			switch ( texInfo_guess.m_internalformat_guess )
			{
			case 1:
			case 3:
				SetTextureStageState_guess( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE | D3DTA_COMPLEMENT );
				SetTextureStageState_guess( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
				SetTextureStageState_guess( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 );
				break;
			case 4:
			case GL_RGBA:
			case GL_RGB5:
			case GL_RGBA4:
			case GL_RGB5_A1:
			case GL_RGBA8:
				SetTextureStageState_guess( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE | D3DTA_COMPLEMENT );
				SetTextureStageState_guess( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
				SetTextureStageState_guess( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
				SetTextureStageState_guess( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
				break;
			}
			break;
		}
		if ( g_guess.m_mtex_guess != FALSE )
		{
			TexInfo_guess		&ti2_guess = g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ 1 ] ];
			LPDIRECT3DTEXTURE2	block2_guess = ti2_guess.m_block_guess;

			SetTextureStageState_guess( 1, D3DTSS_ADDRESSU, ti2_guess.m_addu_guess );
			SetTextureStageState_guess( 1, D3DTSS_ADDRESSV, ti2_guess.m_addv_guess );
			SetTextureStageState_guess( 1, D3DTSS_MAGFILTER, ti2_guess.m_magmode_guess );
			SetTextureStageState_guess( 1, D3DTSS_MINFILTER, ti2_guess.m_minmode_guess );
			SetTextureStageState_guess( 1, D3DTSS_MIPFILTER, ti2_guess.m_mipmode_guess );
			SetTexture_guess( 1, block2_guess );
			switch ( g_guess.m_blendmode_guess[ 1 ] )
			{
			case GL_REPLACE:
				switch ( ti2_guess.m_internalformat_guess )
				{
				case 1:
				case 3:
					SetTextureStageState_guess( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
					SetTextureStageState_guess( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
					SetTextureStageState_guess( 1, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAARG2, D3DTA_CURRENT );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 );
					break;
				case 4:
				case GL_RGBA:
				case GL_RGB5:
				case GL_RGBA4:
				case GL_RGB5_A1:
				case GL_RGBA8:
					SetTextureStageState_guess( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
					SetTextureStageState_guess( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
					SetTextureStageState_guess( 1, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAARG2, D3DTA_CURRENT );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
					break;
				}
				break;
			case GL_MODULATE:
				switch ( ti2_guess.m_internalformat_guess )
				{
				case 1:
				case 3:
					SetTextureStageState_guess( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
					SetTextureStageState_guess( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
					SetTextureStageState_guess( 1, D3DTSS_COLOROP, D3DTOP_MODULATE );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAARG2, D3DTA_CURRENT );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 );
					break;
				case 4:
				case GL_RGBA:
				case GL_RGB5:
				case GL_RGB5_A1:
				case GL_RGBA8:
					SetTextureStageState_guess( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
					SetTextureStageState_guess( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
					SetTextureStageState_guess( 1, D3DTSS_COLOROP, D3DTOP_MODULATE );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAARG2, D3DTA_CURRENT );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
					break;
				}
				break;
			case GL_DECAL:
				switch ( ti2_guess.m_internalformat_guess )
				{
				case 3:
					SetTextureStageState_guess( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
					SetTextureStageState_guess( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
					SetTextureStageState_guess( 1, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAARG2, D3DTA_CURRENT );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 );
					break;
				case 4:
				case GL_RGBA:
				case GL_RGB5:
				case GL_RGBA4:
				case GL_RGB5_A1:
				case GL_RGBA8:
					SetTextureStageState_guess( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
					SetTextureStageState_guess( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
					SetTextureStageState_guess( 1, D3DTSS_COLOROP, D3DTOP_BLENDTEXTUREALPHA );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAARG2, D3DTA_CURRENT );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 );
					break;
				}
				break;
			case GL_BLEND:
				switch ( ti2_guess.m_internalformat_guess )
				{
				case 1:
				case 3:
					SetTextureStageState_guess( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE | D3DTA_COMPLEMENT );
					SetTextureStageState_guess( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
					SetTextureStageState_guess( 1, D3DTSS_COLOROP, D3DTOP_MODULATE );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAARG2, D3DTA_CURRENT );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 );
					break;
				case 4:
				case GL_RGBA:
				case GL_RGB5:
				case GL_RGBA4:
				case GL_RGB5_A1:
				case GL_RGBA8:
					SetTextureStageState_guess( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE | D3DTA_COMPLEMENT );
					SetTextureStageState_guess( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
					SetTextureStageState_guess( 1, D3DTSS_COLOROP, D3DTOP_MODULATE );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAARG2, D3DTA_CURRENT );
					SetTextureStageState_guess( 1, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
					break;
				}
				break;
			}
		}
		g_guess.m_updatestage_guess = TRUE;
	}
}

void APIENTRY glBindTexture (GLenum target, GLuint texture)
{
	g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] = texture;
	g_guess.m_updatestage_guess = FALSE;
}

void APIENTRY glBlendFunc (GLenum sfactor, GLenum dfactor)
{
	int srcmode_guess = -1, dstmode_guess = -1;

	switch ( sfactor )
	{
	case GL_ZERO:
		srcmode_guess = D3DBLEND_ZERO;
		break;
	case GL_ONE:
		srcmode_guess = D3DBLEND_ONE;
		break;
	case GL_DST_COLOR:
		srcmode_guess = D3DBLEND_DESTCOLOR;
		break;
	case GL_ONE_MINUS_DST_COLOR:
		srcmode_guess = D3DBLEND_INVDESTCOLOR;
		break;
	case GL_SRC_ALPHA:
		srcmode_guess = D3DBLEND_SRCALPHA;
		break;
	case GL_ONE_MINUS_SRC_ALPHA:
		srcmode_guess = D3DBLEND_INVSRCALPHA;
		break;
	case GL_DST_ALPHA:
		srcmode_guess = D3DBLEND_DESTALPHA;
		break;
	case GL_ONE_MINUS_DST_ALPHA:
		srcmode_guess = D3DBLEND_INVDESTALPHA;
		break;
	case GL_SRC_ALPHA_SATURATE:
		srcmode_guess = D3DBLEND_SRCALPHASAT;
		break;
	}
	switch ( dfactor )
	{
	case GL_ZERO:
		dstmode_guess = D3DBLEND_ZERO;
		break;
	case GL_ONE:
		dstmode_guess = D3DBLEND_ONE;
		break;
	case GL_SRC_COLOR:
		dstmode_guess = D3DBLEND_SRCCOLOR;
		break;
	case GL_ONE_MINUS_SRC_COLOR:
		dstmode_guess = D3DBLEND_INVSRCCOLOR;
		break;
	case GL_SRC_ALPHA:
		dstmode_guess = D3DBLEND_SRCALPHA;
		break;
	case GL_ONE_MINUS_SRC_ALPHA:
		dstmode_guess = D3DBLEND_INVSRCALPHA;
		break;
	case GL_DST_ALPHA:
		dstmode_guess = D3DBLEND_DESTALPHA;
		break;
	case GL_ONE_MINUS_DST_ALPHA:
		dstmode_guess = D3DBLEND_INVDESTALPHA;
		break;
	}
	if ( srcmode_guess >= 0 )
		SetRenderState_guess( D3DRENDERSTATE_SRCBLEND, srcmode_guess );
	if ( dstmode_guess >= 0 )
		SetRenderState_guess( D3DRENDERSTATE_DESTBLEND, dstmode_guess );
}

void APIENTRY glClear (GLbitfield mask)
{
	DWORD	flags_guess;
	D3DRECT	rect_guess;

	FlushPrimitives_guess();
	flags_guess = 0;
	if ( mask & GL_COLOR_BUFFER_BIT )
		flags_guess |= D3DCLEAR_TARGET;
	else if ( mask & GL_DEPTH_BUFFER_BIT )
		flags_guess |= D3DCLEAR_ZBUFFER;
	else if ( mask & GL_STENCIL_BUFFER_BIT )
		flags_guess |= D3DCLEAR_STENCIL;
	rect_guess.x1 = 0;
	rect_guess.y1 = 0;
	rect_guess.x2 = g_guess.m_winWidth_guess;
	rect_guess.y2 = g_guess.m_winHeight_guess;
	g_guess.m_pD3Dview_guess->Clear2( 1, &rect_guess, flags_guess, g_guess.m_clearColor_guess, 1.0f, 0 );
}

void APIENTRY glClearColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	static float two55_guess = 255.0f;
	int	uRedByte_guess, uGreenByte_guess, uBlueByte_guess, uAlphaByte_guess;

	__asm
	{
		fld		red
		fld		green
		fld		blue
		fld		alpha
		fld		two55_guess
		fmul	st( 1 ), st( 0 )
		fmul	st( 2 ), st( 0 )
		fmul	st( 3 ), st( 0 )
		fmulp	st( 4 ), st( 0 )
		fistp	uAlphaByte_guess
		fistp	uBlueByte_guess
		fistp	uGreenByte_guess
		fistp	uRedByte_guess
		mov		edx, uAlphaByte_guess
		cmp		edx, 255
		jle		alpha_ok
		mov		edx, 255
alpha_ok:
		mov		eax, uBlueByte_guess
		cmp		eax, 255
		jle		blue_ok
		mov		eax, 255
blue_ok:
		mov		ebx, uGreenByte_guess
		cmp		ebx, 255
		jle		green_ok
		mov		ebx, 255
green_ok:
		mov		ecx, uRedByte_guess
		cmp		ecx, 255
		jle		red_ok
		mov		ecx, 255
red_ok:
		shl		ebx, 8
		shl		ecx, 16
		shl		edx, 24
		or		eax, ebx
		or		ecx, edx
		or		eax, ecx
		mov		g_guess.m_clearColor_guess, eax
	}
}

void APIENTRY glColor3f (GLfloat red, GLfloat green, GLfloat blue)
{
	static float two55_guess = 255.0f;
	int	uRedByte_guess, uGreenByte_guess, uBlueByte_guess;

	__asm
	{
		fld		red
		fld		green
		fld		blue
		fld		two55_guess
		fmul	st( 1 ), st( 0 )
		fmul	st( 2 ), st( 0 )
		fmulp	st( 3 ), st( 0 )
		fistp	uBlueByte_guess
		fistp	uGreenByte_guess
		fistp	uRedByte_guess
		mov		eax, uBlueByte_guess
		cmp		eax, 255
		jle		blue_ok
		mov		eax, 255
blue_ok:
		mov		ebx, uGreenByte_guess
		cmp		ebx, 255
		jle		green_ok
		mov		ebx, 255
green_ok:
		mov		ecx, uRedByte_guess
		cmp		ecx, 255
		jle		red_ok
		mov		ecx, 255
red_ok:
		shl		ebx, 8
		shl		ecx, 16
		or		eax, ebx
		or		ecx, 0xFF000000
		or		eax, ecx
		mov		g_guess.m_color_guess, eax
	}
}

void APIENTRY glColor3ub (GLubyte red, GLubyte green, GLubyte blue)
{
	g_guess.m_color_guess = RGBA_MAKE( red, green, blue, 0xFF );
}

void APIENTRY glColor3ubv (const GLubyte *v)
{
	g_guess.m_color_guess = RGBA_MAKE( v[ 0 ], v[ 1 ], v[ 2 ], 0xFF );
}

void APIENTRY glColor4ub (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
	g_guess.m_color_guess = RGBA_MAKE( red, green, blue, alpha );
}

void APIENTRY glColor4ubv (const GLubyte *v)
{
	g_guess.m_color_guess = RGBA_MAKE( v[ 0 ], v[ 1 ], v[ 2 ], v[ 3 ] );
}

void APIENTRY glColor4f (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	static float two55_guess = 255.0f;
	int	uRedByte_guess, uGreenByte_guess, uBlueByte_guess, uAlphaByte_guess;

	__asm
	{
		fld		red
		fld		green
		fld		blue
		fld		alpha
		fld		two55_guess
		fmul	st( 1 ), st( 0 )
		fmul	st( 2 ), st( 0 )
		fmul	st( 3 ), st( 0 )
		fmulp	st( 4 ), st( 0 )
		fistp	uAlphaByte_guess
		fistp	uBlueByte_guess
		fistp	uGreenByte_guess
		fistp	uRedByte_guess
		mov		edx, uAlphaByte_guess
		cmp		edx, 255
		jle		alpha_ok
		mov		edx, 255
alpha_ok:
		mov		eax, uBlueByte_guess
		cmp		eax, 255
		jle		blue_ok
		mov		eax, 255
blue_ok:
		mov		ebx, uGreenByte_guess
		cmp		ebx, 255
		jle		green_ok
		mov		ebx, 255
green_ok:
		mov		ecx, uRedByte_guess
		cmp		ecx, 255
		jle		red_ok
		mov		ecx, 255
red_ok:
		shl		ebx, 8
		shl		ecx, 16
		shl		edx, 24
		or		eax, ebx
		or		ecx, edx
		or		eax, ecx
		mov		g_guess.m_color_guess, eax
	}
}

void APIENTRY glColor4fv (const GLfloat *v)
{
	static float two55_guess = 255.0f;
	int	uRedByte_guess, uGreenByte_guess, uBlueByte_guess, uAlphaByte_guess;

	__asm
	{
		mov		ebx, v
		fld		dword ptr [ ebx ]
		fld		dword ptr [ ebx + 4 ]
		fld		dword ptr [ ebx + 8 ]
		fld		dword ptr [ ebx + 12 ]
		fld		two55_guess
		fmul	st( 1 ), st( 0 )
		fmul	st( 2 ), st( 0 )
		fmul	st( 3 ), st( 0 )
		fmulp	st( 4 ), st( 0 )
		fistp	uAlphaByte_guess
		fistp	uBlueByte_guess
		fistp	uGreenByte_guess
		fistp	uRedByte_guess
		mov		edx, uAlphaByte_guess
		cmp		edx, 255
		jle		alpha_ok
		mov		edx, 255
alpha_ok:
		mov		eax, uBlueByte_guess
		cmp		eax, 255
		jle		blue_ok
		mov		eax, 255
blue_ok:
		mov		ebx, uGreenByte_guess
		cmp		ebx, 255
		jle		green_ok
		mov		ebx, 255
green_ok:
		mov		ecx, uRedByte_guess
		cmp		ecx, 255
		jle		red_ok
		mov		ecx, 255
red_ok:
		shl		ebx, 8
		shl		ecx, 16
		shl		edx, 24
		or		eax, ebx
		or		ecx, edx
		or		eax, ecx
		mov		g_guess.m_color_guess, eax
	}
}

void APIENTRY glColorPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	g_guess.m_colorary_guess = (const GLfloat *)pointer;
	if ( size != 3 && type != GL_FLOAT && stride != 0 )
		OutputDebugString( "Wrapper: unsupported color array\n" );
}

void APIENTRY glCullFace (GLenum mode)
{
	g_guess.m_cullMode_guess = mode;
	if ( g_guess.m_cullEnabled_guess == TRUE )
	{
		DWORD cull_guess;

		if ( mode == GL_BACK )
			cull_guess = D3DCULL_CW;
		else
			cull_guess = D3DCULL_CCW;
		SetRenderState_guess( D3DRENDERSTATE_CULLMODE, cull_guess );
	}
}

void APIENTRY glDepthFunc (GLenum func)
{
	int cmpfunc_guess = -1;

	switch ( func )
	{
	case GL_NEVER:
		cmpfunc_guess = D3DCMP_NEVER;
		break;
	case GL_LESS:
		cmpfunc_guess = D3DCMP_LESS;
		break;
	case GL_EQUAL:
		cmpfunc_guess = D3DCMP_EQUAL;
		break;
	case GL_LEQUAL:
		cmpfunc_guess = D3DCMP_LESSEQUAL;
		break;
	case GL_GREATER:
		cmpfunc_guess = D3DCMP_GREATER;
		break;
	case GL_NOTEQUAL:
		cmpfunc_guess = D3DCMP_NOTEQUAL;
		break;
	case GL_GEQUAL:
		cmpfunc_guess = D3DCMP_GREATEREQUAL;
		break;
	case GL_ALWAYS:
		cmpfunc_guess = D3DCMP_ALWAYS;
		break;
	}
	if ( cmpfunc_guess >= 0 )
		SetRenderState_guess( D3DRENDERSTATE_ZFUNC, cmpfunc_guess );
}

void APIENTRY glDepthMask (GLboolean flag)
{
	if ( flag == GL_FALSE )
		SetRenderState_guess( D3DRENDERSTATE_ZWRITEENABLE, FALSE );
	else
		SetRenderState_guess( D3DRENDERSTATE_ZWRITEENABLE, TRUE );
}

void APIENTRY glDepthRange (GLclampd zNear, GLclampd zFar)
{
	D3DVIEWPORT2 vport_guess;

	FlushPrimitives_guess();
	vport_guess.dwSize = sizeof( D3DVIEWPORT2 );
	g_guess.m_pD3Dview_guess->GetViewport2( &vport_guess );
	vport_guess.dvMinZ = (D3DVALUE)( ( zFar + zNear ) * -1.0 / ( zFar - zNear ) );
	vport_guess.dvMaxZ = (D3DVALUE)( ( zFar + zNear - 2.0 ) * -1.0 / ( zFar - zNear ) );
	g_guess.m_pD3Dview_guess->SetViewport2( &vport_guess );
}

void APIENTRY glDisable (GLenum cap)
{
	D3DVIEWPORT2 vport_guess;

	switch ( cap )
	{
	case GL_DEPTH_TEST:
		SetRenderState_guess( D3DRENDERSTATE_ZENABLE, FALSE );
		break;
	case GL_CULL_FACE:
		g_guess.m_cullEnabled_guess = FALSE;
		SetRenderState_guess( D3DRENDERSTATE_CULLMODE, D3DCULL_NONE );
		break;
	case GL_BLEND:
		SetRenderState_guess( D3DRENDERSTATE_ALPHABLENDENABLE, FALSE );
		break;
	case GL_TEXTURE_2D:
		if ( g_guess.m_curstage_guess == 0 )
		{
			SetTextureStageState_guess( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState_guess( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			g_guess.m_texturing_guess = FALSE;
		}
		else
		{
			SetTextureStageState_guess( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			SetTextureStageState_guess( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			g_guess.m_mtex_guess = FALSE;
		}
		g_guess.m_updatestage_guess = FALSE;
		break;
	case GL_ALPHA_TEST:
		SetRenderState_guess( D3DRENDERSTATE_ALPHATESTENABLE, FALSE );
		break;
	case GL_FOG:
		SetRenderState_guess( D3DRENDERSTATE_FOGENABLE, FALSE );
		SetRenderState_guess( D3DRENDERSTATE_RANGEFOGENABLE, FALSE );
		break;
	case GL_POLYGON_OFFSET_FILL:
		FlushPrimitives_guess();
		vport_guess.dwSize = sizeof( D3DVIEWPORT2 );
		g_guess.m_pD3Dview_guess->GetViewport2( &vport_guess );
		vport_guess.dvMaxZ = g_guess.m_zbias_guess;
		g_guess.m_pD3Dview_guess->SetViewport2( &vport_guess );
		break;
	case GL_SCISSOR_TEST:
		break;
	default:
		OutputDebugString( "Wrapper: glDisable on this cap not supported\n" );
		break;
	}
}

void APIENTRY glDisableClientState (GLenum array)
{
}

void APIENTRY glDrawBuffer (GLenum mode)
{
}

void APIENTRY glEnable (GLenum cap)
{
	D3DVIEWPORT2 vport_guess;

	switch ( cap )
	{
	case GL_DEPTH_TEST:
		SetRenderState_guess( D3DRENDERSTATE_ZENABLE, TRUE );
		break;
	case GL_CULL_FACE:
		g_guess.m_cullEnabled_guess = TRUE;
		if ( g_guess.m_cullMode_guess == GL_BACK )
			SetRenderState_guess( D3DRENDERSTATE_CULLMODE, D3DCULL_CW );
		else
			SetRenderState_guess( D3DRENDERSTATE_CULLMODE, D3DCULL_CCW );
		break;
	case GL_BLEND:
		SetRenderState_guess( D3DRENDERSTATE_ALPHABLENDENABLE, TRUE );
		break;
	case GL_TEXTURE_2D:
		if ( g_guess.m_curstage_guess == 0 )
			g_guess.m_texturing_guess = TRUE;
		else
			g_guess.m_mtex_guess = TRUE;
		g_guess.m_updatestage_guess = FALSE;
		break;
	case GL_ALPHA_TEST:
		SetRenderState_guess( D3DRENDERSTATE_ALPHATESTENABLE, TRUE );
		break;
	case GL_FOG:
		break;
	case GL_POLYGON_OFFSET_FILL:
		vport_guess.dwSize = sizeof( D3DVIEWPORT2 );
		g_guess.m_pD3Dview_guess->GetViewport2( &vport_guess );
		g_guess.m_zbias_guess = vport_guess.dvMaxZ;
		break;
	case GL_SCISSOR_TEST:
		break;
	default:
		OutputDebugString( "Wrapper: glEnable on this cap not supported\n" );
		break;
	}
}

void APIENTRY glEnableClientState (GLenum array)
{
}

void APIENTRY glEnd (void)
{
	int i;

	if ( g_guess.m_nfv_guess == 0 )
		return;

	switch ( g_guess.m_prim_guess )
	{
	case GL_TRIANGLES:
		for ( i = 0; i < g_guess.m_nfv_guess; i++ )
		{
			g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess;
			g_guess.m_vcnt_guess++;
		}
		break;
	case GL_TRIANGLE_STRIP:
		g_guess.m_nfv_guess -= 2;
		for ( i = 0; i < g_guess.m_nfv_guess; i++ )
		{
			g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess;
			g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess + 1;
			g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess + 2;
			g_guess.m_vcnt_guess++;
			if ( ++i < g_guess.m_nfv_guess )
			{
				g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess + 1;
				g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess;
				g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess + 2;
				g_guess.m_vcnt_guess++;
			}
		}
		g_guess.m_vcnt_guess += 2;
		break;
	case GL_TRIANGLE_FAN:
	case GL_POLYGON:
		g_guess.m_nfv_guess--;
		for ( i = 1; i < g_guess.m_nfv_guess; i++ )
		{
			g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess;
			g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess + i;
			g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess + ( i + 1 );
		}
		g_guess.m_vcnt_guess += g_guess.m_nfv_guess + 1;
		break;
	case GL_QUADS:
		for ( i = 0; i < g_guess.m_nfv_guess; i += 4 )
		{
			g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess;
			g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess + 1;
			g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess + 2;
			g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess;
			g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess + 2;
			g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess + 3;
			g_guess.m_vcnt_guess += 4;
		}
		break;
	case GL_QUAD_STRIP:
		for ( i = 2; i < g_guess.m_nfv_guess; i += 2 )
		{
			g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess + ( i - 2 );
			g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess + ( i - 1 );
			g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess + i;
			g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess + ( i - 1 );
			g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess + ( i + 1 );
			g_guess.m_wIndices_guess[ g_guess.m_icnt_guess++ ] = g_guess.m_vcnt_guess + i;
		}
		g_guess.m_vcnt_guess += ( ( g_guess.m_nfv_guess - 2 ) / 2 ) * 6;
		break;
	}
}

void APIENTRY glFogf (GLenum pname, GLfloat param)
{
	switch ( pname )
	{
	case GL_FOG_START:
		SetRenderState_guess( D3DRENDERSTATE_FOGTABLESTART, *(DWORD *)&param );
		break;
	case GL_FOG_END:
		SetRenderState_guess( D3DRENDERSTATE_FOGTABLEEND, *(DWORD *)&param );
		break;
	case GL_FOG_DENSITY:
		SetRenderState_guess( D3DRENDERSTATE_FOGTABLEDENSITY, *(DWORD *)&param );
		break;
	}
}

void APIENTRY glFogfv (GLenum pname, const GLfloat *params)
{
	static D3DCOLOR fogcolor_guess;

	fogcolor_guess = RGBA_MAKE( (DWORD)( params[ 0 ] * 255.0f ), (DWORD)( params[ 1 ] * 255.0f ), (DWORD)( params[ 2 ] * 255.0f ), (DWORD)( params[ 3 ] * 255.0f ) );
	SetRenderState_guess( D3DRENDERSTATE_FOGCOLOR, fogcolor_guess );
}

void APIENTRY glFogi (GLenum pname, GLint param)
{
	if ( pname == GL_FOG_MODE )
	{
		switch ( param )
		{
		case GL_LINEAR:
			SetRenderState_guess( D3DRENDERSTATE_FOGTABLEMODE, D3DFOG_LINEAR );
			break;
		case GL_EXP:
			SetRenderState_guess( D3DRENDERSTATE_FOGTABLEMODE, D3DFOG_EXP );
			break;
		case GL_EXP2:
			SetRenderState_guess( D3DRENDERSTATE_FOGTABLEMODE, D3DFOG_EXP2 );
			break;
		default:
			SetRenderState_guess( D3DRENDERSTATE_FOGTABLEMODE, D3DFOG_NONE );
			break;
		}
	}
}

void APIENTRY glFrustum (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	ProcessVertices_guess();
	D3DMATRIX f;

	f._11 = (D3DVALUE)( 2.0 * zNear / ( right - left ) );
	f._21 = 0.0f;
	f._31 = (D3DVALUE)( ( right + left ) / ( right - left ) );
	f._41 = 0.0f;
	f._12 = 0.0f;
	f._22 = (D3DVALUE)( 2.0 * zNear / ( top - bottom ) );
	f._32 = (D3DVALUE)( ( top + bottom ) / ( top - bottom ) );
	f._42 = 0.0f;
	f._13 = 0.0f;
	f._23 = 0.0f;
	f._33 = (D3DVALUE)( -( zFar + zNear ) / ( zFar - zNear ) );
	f._43 = (D3DVALUE)( -2.0 * zFar * zNear / ( zFar - zNear ) );
	f._14 = 0.0f;
	f._24 = 0.0f;
	f._34 = -1.0f;
	f._44 = 0.0f;
	g_guess.m_pD3DDev_guess->MultiplyTransform( g_guess.m_matrixMode_guess, &f );
}

void APIENTRY glGetFloatv (GLenum pname, GLfloat *params)
{
	switch ( pname )
	{
	case GL_MODELVIEW_MATRIX:
		g_guess.m_pD3DDev_guess->GetTransform( D3DTRANSFORMSTATE_WORLD, (LPD3DMATRIX)params );
		break;
	case GL_PROJECTION_MATRIX:
		g_guess.m_pD3DDev_guess->GetTransform( D3DTRANSFORMSTATE_PROJECTION, (LPD3DMATRIX)params );
		break;
	}
}

DDDEVICEIDENTIFIER	g_ddDeviceId_guess;

const GLubyte * APIENTRY glGetString (GLenum name)
{
	static const char *nvidia_guess = "NVIDIA";

	switch ( name )
	{
	case GL_VENDOR:
		if ( g_ddDeviceId_guess.dwVendorId == 0x12D2 || g_ddDeviceId_guess.dwVendorId == 0x10DE )
			return (const GLubyte *)nvidia_guess;
		return (const GLubyte *)g_ddDeviceId_guess.szDriver;
	case GL_RENDERER:
		return (const GLubyte *)g_ddDeviceId_guess.szDescription;
	case GL_VERSION:
		return (const GLubyte *)"6.0";
	case GL_EXTENSIONS:
		if ( g_guess.m_usemtex_guess )
			return (const GLubyte *)"GL_SGIS_multitexture";
	default:
		return (const GLubyte *)"";
	}
}

void APIENTRY glLoadIdentity (void)
{
	ProcessVertices_guess();
	D3DMATRIX unity_guess;

	unity_guess._11 = 1.0f;
	unity_guess._12 = 0.0f;
	unity_guess._13 = 0.0f;
	unity_guess._14 = 0.0f;
	unity_guess._21 = 0.0f;
	unity_guess._22 = 1.0f;
	unity_guess._23 = 0.0f;
	unity_guess._24 = 0.0f;
	unity_guess._31 = 0.0f;
	unity_guess._32 = 0.0f;
	unity_guess._33 = 1.0f;
	unity_guess._34 = 0.0f;
	unity_guess._41 = 0.0f;
	unity_guess._42 = 0.0f;
	unity_guess._43 = 0.0f;
	unity_guess._44 = 1.0f;
	g_guess.m_pD3DDev_guess->SetTransform( g_guess.m_matrixMode_guess, &unity_guess );
}

void APIENTRY glLoadMatrixf (const GLfloat *m)
{
	ProcessVertices_guess();
	g_guess.m_pD3DDev_guess->SetTransform( g_guess.m_matrixMode_guess, (LPD3DMATRIX)m );
}

void APIENTRY glMatrixMode (GLenum mode)
{
	if ( mode == GL_MODELVIEW )
		g_guess.m_matrixMode_guess = D3DTRANSFORMSTATE_WORLD;
	else
		g_guess.m_matrixMode_guess = D3DTRANSFORMSTATE_PROJECTION;
}

void APIENTRY glOrtho (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	ProcessVertices_guess();
	D3DMATRIX f;

	f._11 = (D3DVALUE)( 2.0 / ( right - left ) );
	f._21 = 0.0f;
	f._31 = 0.0f;
	f._41 = (D3DVALUE)( -( right + left ) / ( right - left ) );
	f._12 = 0.0f;
	f._22 = (D3DVALUE)( 2.0 / ( top - bottom ) );
	f._32 = 0.0f;
	f._42 = (D3DVALUE)( -( top + bottom ) / ( top - bottom ) );
	f._13 = 0.0f;
	f._23 = 0.0f;
	f._33 = (D3DVALUE)( -2.0 / ( zFar - zNear ) );
	f._43 = (D3DVALUE)( -( zFar + zNear ) / ( zFar - zNear ) );
	f._14 = 0.0f;
	f._24 = 0.0f;
	f._34 = 0.0f;
	f._44 = 1.0f;
	g_guess.m_pD3DDev_guess->MultiplyTransform( g_guess.m_matrixMode_guess, &f );
}

void APIENTRY glPolygonMode (GLenum face, GLenum mode)
{
	int fillmode_guess = -1;

	switch ( mode )
	{
	case GL_POINT:
		fillmode_guess = D3DFILL_POINT;
		break;
	case GL_LINE:
		fillmode_guess = D3DFILL_WIREFRAME;
		break;
	case GL_FILL:
		fillmode_guess = D3DFILL_SOLID;
		break;
	}
	if ( fillmode_guess >= 0 )
		SetRenderState_guess( D3DRENDERSTATE_FILLMODE, fillmode_guess );
}

void APIENTRY glPolygonOffset (GLfloat factor, GLfloat units)
{
	static float scale_guess = -0.001f;
	D3DVIEWPORT2 vport_guess;

	FlushPrimitives_guess();
	vport_guess.dwSize = sizeof( D3DVIEWPORT2 );
	g_guess.m_pD3Dview_guess->GetViewport2( &vport_guess );
	if ( units != 0.0f )
		vport_guess.dvMaxZ = factor * scale_guess * g_guess.m_zbias_guess + g_guess.m_zbias_guess;
	else
		vport_guess.dvMaxZ = units * g_guess.m_zbias_guess + g_guess.m_zbias_guess;
	g_guess.m_pD3Dview_guess->SetViewport2( &vport_guess );
}

void APIENTRY glPopMatrix (void)
{
	ProcessVertices_guess();
	if ( g_guess.m_matrixMode_guess == D3DTRANSFORMSTATE_WORLD )
	{
		StackLock_guess<D3DMATRIX> lock_guess( &g_guess.m_matrixStack_guess[ 0 ] );
		g_guess.m_pD3DDev_guess->SetTransform( g_guess.m_matrixMode_guess, &lock_guess.TopData_guess() );
		lock_guess.Pop_guess();
	}
	else
	{
		StackLock_guess<D3DMATRIX> lock_guess( &g_guess.m_matrixStack_guess[ 1 ] );
		g_guess.m_pD3DDev_guess->SetTransform( g_guess.m_matrixMode_guess, &lock_guess.TopData_guess() );
		lock_guess.Pop_guess();
	}
}

void APIENTRY glPushMatrix (void)
{
	D3DMATRIX m;

	g_guess.m_pD3DDev_guess->GetTransform( g_guess.m_matrixMode_guess, &m );
	if ( g_guess.m_matrixMode_guess == D3DTRANSFORMSTATE_WORLD )
		g_guess.m_matrixStack_guess[ 0 ].Push_guess( m );
	else
		g_guess.m_matrixStack_guess[ 1 ].Push_guess( m );
}

void APIENTRY glReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels)
{
	DDSURFACEDESC2			surfdesc_guess;
	DWORD					dwMask_guess;
	int						RRShift_guess, RLShift_guess;
	int						BRShift_guess, BLShift_guess;
	int						GRShift_guess, GLShift_guess;
	LPDIRECTDRAWSURFACE4	lpFront_guess;

	g_guess.m_pDXMgr_guess->GetPrimarySurface_guess( &lpFront_guess );
	Q_memset( &surfdesc_guess, 0, sizeof( DDSURFACEDESC2 ) );
	surfdesc_guess.dwSize = sizeof( DDSURFACEDESC2 );
	if ( g_guess.m_fullscreen_guess )
	{
		HRESULT result_guess = lpFront_guess->Lock( NULL, &surfdesc_guess, DDLOCK_WAIT | DDLOCK_READONLY, NULL );
		if ( result_guess != DD_OK )
		{
			OutputDebugString( "Wrapper: Lock on primary failed" );
			lpFront_guess->Release();
			return;
		}
	}
	else
	{
		RECT	rect_guess;
		POINT	ptTL_guess, ptBR_guess;
		HRESULT	result_guess;

		ptTL_guess.x = 0;
		ptTL_guess.y = 0;
		ClientToScreen( g_guess.m_hwnd_guess, &ptTL_guess );
		ptBR_guess.x = g_guess.m_winWidth_guess;
		ptBR_guess.y = g_guess.m_winHeight_guess;
		ClientToScreen( g_guess.m_hwnd_guess, &ptBR_guess );
		rect_guess.left = ptTL_guess.x;
		rect_guess.right = ptBR_guess.x;
		rect_guess.top = ptTL_guess.y;
		rect_guess.bottom = ptBR_guess.y;
		result_guess = lpFront_guess->Lock( &rect_guess, &surfdesc_guess, DDLOCK_WAIT | DDLOCK_READONLY, NULL );
		if ( result_guess != DD_OK )
		{
			OutputDebugString( "Wrapper: Lock on primary failed" );
			lpFront_guess->Release();
			return;
		}
	}

	// shifts that scale each color mask of the primary surface to 8 bits
	for ( RRShift_guess = 0, dwMask_guess = surfdesc_guess.ddpfPixelFormat.dwRBitMask; !( dwMask_guess & 1 ); dwMask_guess >>= 1, RRShift_guess++ )
		;
	for ( RLShift_guess = 8; dwMask_guess & 1; dwMask_guess >>= 1, RLShift_guess-- )
		;
	for ( GRShift_guess = 0, dwMask_guess = surfdesc_guess.ddpfPixelFormat.dwGBitMask; !( dwMask_guess & 1 ); dwMask_guess >>= 1, GRShift_guess++ )
		;
	for ( GLShift_guess = 8; dwMask_guess & 1; dwMask_guess >>= 1, GLShift_guess-- )
		;
	for ( BRShift_guess = 0, dwMask_guess = surfdesc_guess.ddpfPixelFormat.dwBBitMask; !( dwMask_guess & 1 ); dwMask_guess >>= 1, BRShift_guess++ )
		;
	for ( BLShift_guess = 8; dwMask_guess & 1; dwMask_guess >>= 1, BLShift_guess-- )
		;

	// the window goes into pixels as bottom-up RGB rows
	switch ( surfdesc_guess.ddpfPixelFormat.dwRGBBitCount )
	{
	case 16:
		{
			int		i, j;
			LPBYTE	src_guess, dst_guess;

			src_guess = (LPBYTE)surfdesc_guess.lpSurface;
			dst_guess = (LPBYTE)pixels;
			dst_guess += g_guess.m_winWidth_guess * ( g_guess.m_winHeight_guess - 1 ) * 3;
			for ( i = 0; i < g_guess.m_winHeight_guess; i++ )
			{
				for ( j = 0; j < g_guess.m_winWidth_guess; j++ )
				{
					DWORD	rbyte_guess = ( ( ( (LPWORD)src_guess )[ j ] & surfdesc_guess.ddpfPixelFormat.dwRBitMask ) >> RRShift_guess ) << RLShift_guess;
					DWORD	gbyte_guess = ( ( ( (LPWORD)src_guess )[ j ] & surfdesc_guess.ddpfPixelFormat.dwGBitMask ) >> GRShift_guess ) << GLShift_guess;
					DWORD	bbyte_guess = ( ( ( (LPWORD)src_guess )[ j ] & surfdesc_guess.ddpfPixelFormat.dwBBitMask ) >> BRShift_guess ) << BLShift_guess;

					*dst_guess++ = (BYTE)rbyte_guess;
					*dst_guess++ = (BYTE)gbyte_guess;
					*dst_guess++ = (BYTE)bbyte_guess;
				}
				src_guess += surfdesc_guess.lPitch;
				dst_guess -= g_guess.m_winWidth_guess * 3 * 2;
			}
		}
		break;
	case 24:
		{
			int		i, j;
			LPBYTE	src_guess, dst_guess;

			src_guess = (LPBYTE)surfdesc_guess.lpSurface;
			dst_guess = (LPBYTE)pixels;
			dst_guess += g_guess.m_winWidth_guess * ( g_guess.m_winHeight_guess - 1 ) * 3;
			for ( i = 0; i < g_guess.m_winHeight_guess; i++ )
			{
				for ( j = 0; j < g_guess.m_winWidth_guess; j++ )
				{
					DWORD	rbyte_guess = ( ( *(LPDWORD)( src_guess + j * 3 ) & surfdesc_guess.ddpfPixelFormat.dwRBitMask ) >> RRShift_guess ) << RLShift_guess;
					DWORD	gbyte_guess = ( ( *(LPDWORD)( src_guess + j * 3 ) & surfdesc_guess.ddpfPixelFormat.dwGBitMask ) >> GRShift_guess ) << GLShift_guess;
					DWORD	bbyte_guess = ( ( *(LPDWORD)( src_guess + j * 3 ) & surfdesc_guess.ddpfPixelFormat.dwBBitMask ) >> BRShift_guess ) << BLShift_guess;

					*dst_guess++ = (BYTE)rbyte_guess;
					*dst_guess++ = (BYTE)gbyte_guess;
					*dst_guess++ = (BYTE)bbyte_guess;
				}
				src_guess += surfdesc_guess.lPitch;
				dst_guess -= g_guess.m_winWidth_guess * 3 * 2;
			}
		}
		break;
	case 32:
		{
			int		i, j;
			LPBYTE	src_guess, dst_guess;

			src_guess = (LPBYTE)surfdesc_guess.lpSurface;
			dst_guess = (LPBYTE)pixels;
			dst_guess += g_guess.m_winWidth_guess * ( g_guess.m_winHeight_guess - 1 ) * 3;
			for ( i = 0; i < g_guess.m_winHeight_guess; i++ )
			{
				for ( j = 0; j < g_guess.m_winWidth_guess; j++ )
				{
					DWORD	rbyte_guess = ( ( ( (LPDWORD)src_guess )[ j ] & surfdesc_guess.ddpfPixelFormat.dwRBitMask ) >> RRShift_guess ) << RLShift_guess;
					DWORD	gbyte_guess = ( ( ( (LPDWORD)src_guess )[ j ] & surfdesc_guess.ddpfPixelFormat.dwGBitMask ) >> GRShift_guess ) << GLShift_guess;
					DWORD	bbyte_guess = ( ( ( (LPDWORD)src_guess )[ j ] & surfdesc_guess.ddpfPixelFormat.dwBBitMask ) >> BRShift_guess ) << BLShift_guess;

					*dst_guess++ = (BYTE)rbyte_guess;
					*dst_guess++ = (BYTE)gbyte_guess;
					*dst_guess++ = (BYTE)bbyte_guess;
				}
				src_guess += surfdesc_guess.lPitch;
				dst_guess -= g_guess.m_winWidth_guess * 3 * 2;
			}
		}
		break;
	}
	lpFront_guess->Release();
	lpFront_guess->Unlock( NULL );
}

void APIENTRY glRotatef (GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	ProcessVertices_guess();
	static double pi_guess = atan( 1.0 ) * 4.0;
	D3DMATRIX	rotmat_guess;
	double		a_guess = angle * pi_guess / 180.0;
	float		c_guess = (float)cos( a_guess );
	float		sine_guess = (float)sin( a_guess );

	if ( x == 1.0f )
	{
		rotmat_guess._11 = 1.0f;
		rotmat_guess._21 = 0.0f;
		rotmat_guess._31 = 0.0f;
		rotmat_guess._41 = 0.0f;
		rotmat_guess._12 = 0.0f;
		rotmat_guess._22 = c_guess;
		rotmat_guess._32 = -sine_guess;
		rotmat_guess._42 = 0.0f;
		rotmat_guess._13 = 0.0f;
		rotmat_guess._23 = sine_guess;
		rotmat_guess._33 = c_guess;
		rotmat_guess._43 = 0.0f;
		rotmat_guess._14 = 0.0f;
		rotmat_guess._24 = 0.0f;
		rotmat_guess._34 = 0.0f;
		rotmat_guess._44 = 1.0f;
	}
	else if ( y == 1.0f )
	{
		rotmat_guess._11 = c_guess;
		rotmat_guess._21 = 0.0f;
		rotmat_guess._31 = sine_guess;
		rotmat_guess._41 = 0.0f;
		rotmat_guess._12 = 0.0f;
		rotmat_guess._22 = 1.0f;
		rotmat_guess._32 = 0.0f;
		rotmat_guess._42 = 0.0f;
		rotmat_guess._13 = -sine_guess;
		rotmat_guess._23 = 0.0f;
		rotmat_guess._33 = c_guess;
		rotmat_guess._43 = 0.0f;
		rotmat_guess._14 = 0.0f;
		rotmat_guess._24 = 0.0f;
		rotmat_guess._34 = 0.0f;
		rotmat_guess._44 = 1.0f;
	}
	else if ( z == 1.0f )
	{
		rotmat_guess._11 = c_guess;
		rotmat_guess._21 = -sine_guess;
		rotmat_guess._31 = 0.0f;
		rotmat_guess._41 = 0.0f;
		rotmat_guess._12 = sine_guess;
		rotmat_guess._22 = c_guess;
		rotmat_guess._32 = 0.0f;
		rotmat_guess._42 = 0.0f;
		rotmat_guess._13 = 0.0f;
		rotmat_guess._23 = 0.0f;
		rotmat_guess._33 = 1.0f;
		rotmat_guess._43 = 0.0f;
		rotmat_guess._14 = 0.0f;
		rotmat_guess._24 = 0.0f;
		rotmat_guess._34 = 0.0f;
		rotmat_guess._44 = 1.0f;
	}
	else
		return;
	g_guess.m_pD3DDev_guess->MultiplyTransform( g_guess.m_matrixMode_guess, &rotmat_guess );
}

void APIENTRY glScalef (GLfloat x, GLfloat y, GLfloat z)
{
	ProcessVertices_guess();
	D3DMATRIX f;

	f._11 = x;
	f._21 = 0.0f;
	f._31 = 0.0f;
	f._41 = 0.0f;
	f._12 = 0.0f;
	f._22 = y;
	f._32 = 0.0f;
	f._42 = 0.0f;
	f._13 = 0.0f;
	f._23 = 0.0f;
	f._33 = z;
	f._43 = 0.0f;
	f._14 = 0.0f;
	f._24 = 0.0f;
	f._34 = 0.0f;
	f._44 = 1.0f;
	g_guess.m_pD3DDev_guess->MultiplyTransform( g_guess.m_matrixMode_guess, &f );
}

void APIENTRY glShadeModel (GLenum mode)
{
	if ( mode == GL_SMOOTH )
		SetRenderState_guess( D3DRENDERSTATE_SHADEMODE, D3DSHADE_GOURAUD );
	else
		SetRenderState_guess( D3DRENDERSTATE_SHADEMODE, D3DSHADE_FLAT );
}

void APIENTRY glTexCoord2f (GLfloat s, GLfloat t)
{
	TexInfo_guess *ti_guess = &g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ];

	if ( ti_guess->m_minmode_guess == D3DTFN_POINT )
	{
		g_guess.m_tu_guess = s + 0.5 / g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_dwWidth_guess;
		g_guess.m_tv_guess = t + 0.5 / g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_dwHeight_guess;
	}
	else
	{
		g_guess.m_tu_guess = s;
		g_guess.m_tv_guess = t;
	}
}

void APIENTRY glTexEnvf (GLenum target, GLenum pname, GLfloat param)
{
	if ( pname == GL_TEXTURE_ENV_MODE )
	{
		g_guess.m_blendmode_guess[ g_guess.m_curstage_guess ] = (int)param;
		g_guess.m_updatestage_guess = FALSE;
	}
	else
	{
		OutputDebugString( "Wrapper: GL_TEXTURE_ENV_COLOR not implemented\n" );
	}
}

void APIENTRY glTexParameterf (GLenum target, GLenum pname, GLfloat param)
{
	switch ( pname )
	{
	case GL_TEXTURE_MIN_FILTER:
		switch ( (int)param )
		{
		case GL_NEAREST:
			g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_minmode_guess = D3DTFN_POINT;
			g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_mipmode_guess = D3DTFP_NONE;
			break;
		case GL_LINEAR:
			g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_minmode_guess = D3DTFN_LINEAR;
			g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_mipmode_guess = D3DTFP_NONE;
			break;
		case GL_NEAREST_MIPMAP_NEAREST:
			g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_minmode_guess = D3DTFN_POINT;
			g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_mipmode_guess = D3DTFP_POINT;
			break;
		case GL_NEAREST_MIPMAP_LINEAR:
			g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_minmode_guess = D3DTFN_POINT;
			g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_mipmode_guess = D3DTFP_LINEAR;
			break;
		case GL_LINEAR_MIPMAP_NEAREST:
			g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_minmode_guess = D3DTFN_LINEAR;
			g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_mipmode_guess = D3DTFP_POINT;
			break;
		case GL_LINEAR_MIPMAP_LINEAR:
			g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_minmode_guess = D3DTFN_LINEAR;
			g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_mipmode_guess = D3DTFP_LINEAR;
			break;
		}
		break;
	case GL_TEXTURE_MAG_FILTER:
		if ( param == GL_NEAREST )
			g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_magmode_guess = D3DTFG_POINT;
		else
			g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_magmode_guess = D3DTFG_LINEAR;
		break;
	case GL_TEXTURE_WRAP_S:
		if ( param == GL_CLAMP )
			g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_addu_guess = D3DTADDRESS_CLAMP;
		else
			g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_addu_guess = D3DTADDRESS_WRAP;
		break;
	case GL_TEXTURE_WRAP_T:
		if ( param == GL_CLAMP )
			g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_addv_guess = D3DTADDRESS_CLAMP;
		else
			g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ].m_addv_guess = D3DTADDRESS_WRAP;
		break;
	}
	g_guess.m_updatestage_guess = FALSE;
}

void APIENTRY glTranslatef (GLfloat x, GLfloat y, GLfloat z)
{
	ProcessVertices_guess();
	D3DMATRIX f;

	f._11 = 1.0f;
	f._21 = 0.0f;
	f._31 = 0.0f;
	f._41 = x;
	f._12 = 0.0f;
	f._22 = 1.0f;
	f._32 = 0.0f;
	f._42 = y;
	f._13 = 0.0f;
	f._23 = 0.0f;
	f._33 = 1.0f;
	f._43 = z;
	f._14 = 0.0f;
	f._24 = 0.0f;
	f._34 = 0.0f;
	f._44 = 1.0f;
	g_guess.m_pD3DDev_guess->MultiplyTransform( g_guess.m_matrixMode_guess, &f );
}

void APIENTRY glVertex2f (GLfloat x, GLfloat y)
{
	Vertex_guess *vert_guess = &g_guess.m_verts_guess[ g_guess.m_vcnt_guess + g_guess.m_nfv_guess ];

	vert_guess->x = x;
	vert_guess->y = y;
	vert_guess->z = 0.0f;
	vert_guess->color_guess = g_guess.m_color_guess;
	vert_guess->tu_guess = g_guess.m_tu_guess;
	vert_guess->tv_guess = g_guess.m_tv_guess;
	if ( g_guess.m_mtex_guess )
	{
		vert_guess->tu2_guess = g_guess.m_tu2_guess;
		vert_guess->tv2_guess = g_guess.m_tv2_guess;
	}
	g_guess.m_nfv_guess++;
}

void APIENTRY glVertex3f (GLfloat x, GLfloat y, GLfloat z)
{
	Vertex_guess *vert_guess = &g_guess.m_verts_guess[ g_guess.m_vcnt_guess + g_guess.m_nfv_guess ];

	vert_guess->x = x;
	vert_guess->y = y;
	vert_guess->z = z;
	vert_guess->color_guess = g_guess.m_color_guess;
	vert_guess->tu_guess = g_guess.m_tu_guess;
	vert_guess->tv_guess = g_guess.m_tv_guess;
	if ( g_guess.m_mtex_guess )
	{
		vert_guess->tu2_guess = g_guess.m_tu2_guess;
		vert_guess->tv2_guess = g_guess.m_tv2_guess;
	}
	g_guess.m_nfv_guess++;
}

void APIENTRY glVertex3fv (const GLfloat *v)
{
	Vertex_guess *vert_guess = &g_guess.m_verts_guess[ g_guess.m_vcnt_guess + g_guess.m_nfv_guess ];

	vert_guess->x = v[ 0 ];
	vert_guess->y = v[ 1 ];
	vert_guess->z = v[ 2 ];
	vert_guess->color_guess = g_guess.m_color_guess;
	vert_guess->tu_guess = g_guess.m_tu_guess;
	vert_guess->tv_guess = g_guess.m_tv_guess;
	if ( g_guess.m_mtex_guess )
	{
		vert_guess->tu2_guess = g_guess.m_tu2_guess;
		vert_guess->tv2_guess = g_guess.m_tv2_guess;
	}
	g_guess.m_nfv_guess++;
}

void APIENTRY glVertexPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	g_guess.m_vertexary_guess = (const GLfloat ( * )[ 4 ])pointer;
	g_guess.m_vertexstride_guess = stride;
}

void APIENTRY glTexCoordPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	g_guess.m_texcoordary_guess = (const GLfloat *)pointer;
	g_guess.m_texcoordstride_guess = stride;
}

void APIENTRY glDrawArrays (GLenum mode, GLint first, GLsizei count)
{
	int				offs_guess = first * 4 * ( g_guess.m_texcoordstride_guess / 4 );
	const GLfloat	*srctexcoord_guess = g_guess.m_texcoordary_guess + offs_guess;
	const GLfloat	*srcvertex_guess = (const GLfloat *)g_guess.m_vertexary_guess + offs_guess;
	int				tcstride_guess = g_guess.m_texcoordstride_guess / 4;
	int				vstride_guess = g_guess.m_vertexstride_guess / 4;
	int				i, j;

	count /= 4;
	for ( i = first; i < count; i++ )
	{
		glBegin( GL_QUADS );
		for ( j = 0; j < 4; j++ )
		{
			glTexCoord2f( srctexcoord_guess[ 0 ], srctexcoord_guess[ 1 ] );
			glVertex2f( srcvertex_guess[ 0 ], srcvertex_guess[ 1 ] );
			srctexcoord_guess += tcstride_guess;
			srcvertex_guess += vstride_guess;
		}
		glEnd();
	}
}

void APIENTRY glViewport (GLint x, GLint y, GLsizei width, GLsizei height)
{
	D3DVIEWPORT2 vport_guess;

	FlushPrimitives_guess();
	vport_guess.dwSize = sizeof( D3DVIEWPORT2 );
	g_guess.m_pD3Dview_guess->GetViewport2( &vport_guess );
	vport_guess.dwX = x;
	vport_guess.dwY = g_guess.m_winHeight_guess - ( y + height );
	vport_guess.dwWidth = width;
	vport_guess.dwHeight = height;
	vport_guess.dvClipX = -1.0f;
	vport_guess.dvClipY = 1.0f;
	vport_guess.dvClipWidth = 2.0f;
	vport_guess.dvClipHeight = 2.0f;
	g_guess.m_pD3Dview_guess->SetViewport2( &vport_guess );
}

extern "C" BOOL WINAPI wglChoosePixelFormat( HDC hdc, CONST PIXELFORMATDESCRIPTOR *ppfd )
{
	return TRUE;
}

HGLRC WINAPI wglCreateContext( HDC hdc )
{
	RECT				rcwin_guess;
	DDSURFACEDESC		ddsd;
	D3DDEVICEDESC		hel_guess;
	int					curwide_guess, curbpp_guess;
	LPDIRECT3D3			lpDirect3D_guess;
	DXMGRCREATE_guess	createparams_guess;
	int					curtall_guess;
	int					nParm_guess;
	IDXEnum_guess		*texenum_guess;
	LPDIRECTDRAW		tmpDD_guess;
	int					ix_guess;
	HRESULT				ddrval;
	bool				have_guess;
	D3DDEVICEDESC		haldesc_guess;
	HMODULE				hOverlay_guess;
	D3DVERTEXBUFFERDESC	vbDescr_guess;
	DWORD				vbuflen_guess;
	HKEY				hKey_guess;

	g_guess.m_hdc_guess = hdc;
	g_guess.m_hwnd_guess = WindowFromDC( g_guess.m_hdc_guess );
	GetClientRect( g_guess.m_hwnd_guess, &rcwin_guess );
	g_guess.m_winWidth_guess = LOWORD( rcwin_guess.right );
	g_guess.m_winHeight_guess = LOWORD( rcwin_guess.bottom );

	ddrval = CoInitialize( NULL );
	if ( FAILED( ddrval ) )
		return 0;

	ddrval = DirectDrawCreate( NULL, &tmpDD_guess, NULL );
	if ( FAILED( ddrval ) )
	{
		CoUninitialize();
		return 0;
	}
	ddsd.dwSize = sizeof( ddsd );
	ddrval = tmpDD_guess->GetDisplayMode( &ddsd );
	if ( FAILED( ddrval ) )
	{
		tmpDD_guess->Release();
		CoUninitialize();
		return 0;
	}
	tmpDD_guess->Release();

	ddrval = DXMgrCreate_guess( IID_IDXMgr_guess, (LPVOID *)&g_guess.m_pDXMgr_guess );
	if ( FAILED( ddrval ) )
	{
		CoUninitialize();
		return 0;
	}
	ddrval = g_guess.m_pDXMgr_guess->SetDirectDraw_guess( g_guess.m_pDD_guess, g_guess.m_hwnd_guess, 0 );
	if ( FAILED( ddrval ) )
	{
		g_guess.m_pDXMgr_guess->Release();
		CoUninitialize();
		return 0;
	}

	Q_memset( &createparams_guess, 0, sizeof( createparams_guess ) );
	createparams_guess.dwFlags_guess = DXMGRCREATE_DEFAULT_guess;
	if ( RegOpenKey( HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Quake", &hKey_guess ) == ERROR_SUCCESS )
	{
		DWORD dwSize = 4, dwType, dwValue;

		if ( RegQueryValueEx( hKey_guess, "Emulation", NULL, &dwType, (LPBYTE)&dwValue, &dwSize ) == ERROR_SUCCESS && dwType == REG_DWORD && dwValue != 0 )
		{
			createparams_guess.guidDevice_guess = IID_IDirect3DRGBDevice;
			createparams_guess.dwFlags_guess |= DXMGRCREATE_DEVICE_guess;
		}
		RegCloseKey( hKey_guess );
	}
	createparams_guess.hWnd_guess = g_guess.m_hwnd_guess;
	if ( g_guess.m_fullscreen_guess )
	{
		IDXEnum_guess	*pModeEnum_guess;
		ULONG			fetched_guess;

		createparams_guess.dwFlags_guess |= DXMGRCREATE_FULLSCREEN_guess;
		ddrval = g_guess.m_pDXMgr_guess->EnumDisplayModes_guess( &pModeEnum_guess, 0 );
		if ( FAILED( ddrval ) )
		{
			g_guess.m_pDXMgr_guess->Release();
			CoUninitialize();
			return 0;
		}
		while ( 1 )
		{
			ddrval = pModeEnum_guess->Next( 1, &createparams_guess.mode_guess, &fetched_guess );
			if ( FAILED( ddrval ) )
			{
				pModeEnum_guess->Release();
				g_guess.m_pDXMgr_guess->Release();
				CoUninitialize();
				return 0;
			}
			if ( fetched_guess == 0 )
				break;
			if ( createparams_guess.mode_guess.dwWidth_guess == ddsd.dwWidth && createparams_guess.mode_guess.dwHeight_guess == ddsd.dwHeight && createparams_guess.mode_guess.dwBPP_guess == ddsd.ddpfPixelFormat.dwRGBBitCount )
			{
				createparams_guess.dwFlags_guess |= DXMGRCREATE_MODE_guess;
				break;
			}
		}
		pModeEnum_guess->Release();
	}
	createparams_guess.dvNear_guess = 1.0f;
	createparams_guess.dvFar_guess = 10.0f;
	nParm_guess = COM_CheckParm( "-freq" );
	if ( nParm_guess )
		createparams_guess.mode_guess.dwRefreshRate_guess = atoi( com_argv[ nParm_guess + 1 ] );
	ddrval = g_guess.m_pDXMgr_guess->Create_guess( &createparams_guess );
	if ( FAILED( ddrval ) )
	{
		g_guess.m_pDXMgr_guess->Release();
		CoUninitialize();
		return 0;
	}
	ddrval = g_guess.m_pDXMgr_guess->GetDirectDraw_guess( &g_guess.m_pDD_guess );
	if ( FAILED( ddrval ) )
	{
		g_guess.m_pD3DDev_guess->Release();
		g_guess.m_pDXMgr_guess->Release();
		CoUninitialize();
		return 0;
	}
	ddrval = g_guess.m_pDXMgr_guess->GetDevice_guess( &g_guess.m_pD3DDev_guess );
	if ( FAILED( ddrval ) )
	{
		g_guess.m_pDXMgr_guess->Release();
		CoUninitialize();
		return 0;
	}
	g_guess.m_pD3DDevProcess_guess = g_guess.m_pD3DDev_guess;

	// let the Steam overlay hook the device
	hOverlay_guess = GetModuleHandle( "GameOverlayRenderer.dll" );
	if ( hOverlay_guess )
	{
		void ( *pfnHook_guess )( LPDIRECT3DDEVICE3 *, LPDIRECTDRAW4 * ) = ( void ( * )( LPDIRECT3DDEVICE3 *, LPDIRECTDRAW4 * ) )GetProcAddress( hOverlay_guess, "OverlayHookD3D3" );
		if ( pfnHook_guess )
			pfnHook_guess( &g_guess.m_pD3DDev_guess, &g_guess.m_pDD_guess );
	}

	hel_guess.dwSize = sizeof( D3DDEVICEDESC );
	haldesc_guess.dwSize = sizeof( D3DDEVICEDESC );
	g_guess.m_pD3DDev_guess->GetCaps( &haldesc_guess, &hel_guess );
	g_guess.m_dd_guess = haldesc_guess.dwFlags ? haldesc_guess : hel_guess;

	if ( RegOpenKey( HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Quake", &hKey_guess ) == ERROR_SUCCESS )
	{
		DWORD dwSize = 4, dwType, dwValue;

		if ( RegQueryValueEx( hKey_guess, "DisableMipMap", NULL, &dwType, (LPBYTE)&dwValue, &dwSize ) == ERROR_SUCCESS && dwType == REG_DWORD && dwValue != 0 )
		{
			g_guess.m_usemipmap_guess = FALSE;
			OutputDebugString( "Wrapper: Mipmapping disabled\n" );
		}
		else
		{
			g_guess.m_usemipmap_guess = TRUE;
		}
		RegCloseKey( hKey_guess );
	}
	else
	{
		g_guess.m_usemipmap_guess = TRUE;
	}

	ddrval = g_guess.m_pDXMgr_guess->EnumTextureFormats_guess( &texenum_guess, 0 );
	if ( FAILED( ddrval ) )
	{
		g_guess.m_pD3DDev_guess->Release();
		g_guess.m_pDXMgr_guess->Release();
		CoUninitialize();
		return 0;
	}
	VideoMode_GetCurrentVideoMode( &curwide_guess, &curtall_guess, &curbpp_guess );

	have_guess = false;
	while ( 1 )
	{
		ULONG fetched_guess;

		ddrval = texenum_guess->Next( 1, &g_guess.m_ddFiveBitAlphaFormat_guess, &fetched_guess );
		if ( FAILED( ddrval ) )
		{
			texenum_guess->Release();
			g_guess.m_pD3DDev_guess->Release();
			g_guess.m_pDXMgr_guess->Release();
			CoUninitialize();
			return 0;
		}
		if ( fetched_guess == 0 )
			break;
		if ( curbpp_guess == 16 && ( g_guess.m_ddFiveBitAlphaFormat_guess.dwFlags & DDPF_RGB ) && g_guess.m_ddFiveBitAlphaFormat_guess.dwRGBBitCount == curbpp_guess && ( g_guess.m_ddFiveBitAlphaFormat_guess.dwFlags & DDPF_ALPHAPIXELS ) && g_guess.m_ddFiveBitAlphaFormat_guess.dwRGBAlphaBitMask == 0x8000 && g_guess.m_ddFiveBitAlphaFormat_guess.dwRBitMask == 0x7C00 && g_guess.m_ddFiveBitAlphaFormat_guess.dwGBitMask == 0x3E0 && g_guess.m_ddFiveBitAlphaFormat_guess.dwBBitMask == 0x1F )
		{
			have_guess = true;
			break;
		}
		if ( curbpp_guess == 32 && ( g_guess.m_ddFiveBitAlphaFormat_guess.dwFlags & DDPF_RGB ) && g_guess.m_ddFiveBitAlphaFormat_guess.dwRGBBitCount == curbpp_guess && ( g_guess.m_ddFiveBitAlphaFormat_guess.dwFlags & DDPF_ALPHAPIXELS ) && g_guess.m_ddFiveBitAlphaFormat_guess.dwRGBAlphaBitMask == 0xFF000000 && g_guess.m_ddFiveBitAlphaFormat_guess.dwRBitMask == 0xFF0000 && g_guess.m_ddFiveBitAlphaFormat_guess.dwGBitMask == 0xFF00 && g_guess.m_ddFiveBitAlphaFormat_guess.dwBBitMask == 0xFF )
		{
			have_guess = true;
			break;
		}
	}
	if ( !have_guess )
	{
		OutputDebugString( "Wrapper: Unable to find 5551 texture.\n" );
		texenum_guess->Release();
		g_guess.m_pD3DDev_guess->Release();
		g_guess.m_pDXMgr_guess->Release();
		CoUninitialize();
		return 0;
	}

	ddrval = texenum_guess->Reset();
	if ( FAILED( ddrval ) )
	{
		texenum_guess->Release();
		g_guess.m_pD3DDev_guess->Release();
		g_guess.m_pDXMgr_guess->Release();
		CoUninitialize();
		return 0;
	}
	have_guess = false;
	while ( 1 )
	{
		ULONG fetched_guess;

		ddrval = texenum_guess->Next( 1, &g_guess.m_ddFourBitAlphaFormat_guess, &fetched_guess );
		if ( FAILED( ddrval ) )
		{
			texenum_guess->Release();
			g_guess.m_pD3DDev_guess->Release();
			g_guess.m_pDXMgr_guess->Release();
			CoUninitialize();
			return 0;
		}
		if ( fetched_guess == 0 )
			break;
		if ( ( g_guess.m_ddFourBitAlphaFormat_guess.dwFlags & DDPF_RGB ) && g_guess.m_ddFourBitAlphaFormat_guess.dwRGBBitCount == 16 && ( g_guess.m_ddFourBitAlphaFormat_guess.dwFlags & DDPF_ALPHAPIXELS ) && g_guess.m_ddFourBitAlphaFormat_guess.dwRGBAlphaBitMask == 0xF000 && g_guess.m_ddFourBitAlphaFormat_guess.dwRBitMask == 0xF00 && g_guess.m_ddFourBitAlphaFormat_guess.dwGBitMask == 0xF0 && g_guess.m_ddFourBitAlphaFormat_guess.dwBBitMask == 0xF )
		{
			have_guess = true;
			break;
		}
	}
	if ( !have_guess )
	{
		OutputDebugString( "Wrapper: Not using 4444 texture.\n" );
		g_guess.m_ddFourBitAlphaFormat_guess = g_guess.m_ddFiveBitAlphaFormat_guess;
	}

	ddrval = texenum_guess->Reset();
	if ( FAILED( ddrval ) )
	{
		texenum_guess->Release();
		g_guess.m_pD3DDev_guess->Release();
		g_guess.m_pDXMgr_guess->Release();
		CoUninitialize();
		return 0;
	}
	have_guess = false;
	while ( 1 )
	{
		ULONG fetched_guess;

		ddrval = texenum_guess->Next( 1, &g_guess.m_ddFiveBitFormat_guess, &fetched_guess );
		if ( FAILED( ddrval ) )
		{
			texenum_guess->Release();
			g_guess.m_pD3DDev_guess->Release();
			g_guess.m_pDXMgr_guess->Release();
			CoUninitialize();
			return 0;
		}
		if ( fetched_guess == 0 )
			break;
		if ( ( g_guess.m_ddFiveBitFormat_guess.dwFlags & DDPF_RGB ) && g_guess.m_ddFiveBitFormat_guess.dwRGBBitCount == 16 && !( g_guess.m_ddFiveBitFormat_guess.dwFlags & DDPF_ALPHAPIXELS ) && ( g_guess.m_ddFiveBitFormat_guess.dwRBitMask == 0xF800 || g_guess.m_ddFiveBitFormat_guess.dwRBitMask == 0x7C00 ) && ( g_guess.m_ddFiveBitFormat_guess.dwGBitMask == 0x7E0 || g_guess.m_ddFiveBitFormat_guess.dwGBitMask == 0x3E0 ) && g_guess.m_ddFiveBitFormat_guess.dwBBitMask == 0x1F )
		{
			have_guess = true;
			break;
		}
	}
	if ( !have_guess )
	{
		OutputDebugString( "Wrapper: Unable to find 555 or 565 texture.\n" );
		texenum_guess->Release();
		g_guess.m_pD3DDev_guess->Release();
		g_guess.m_pDXMgr_guess->Release();
		CoUninitialize();
		return 0;
	}

	ddrval = texenum_guess->Reset();
	if ( FAILED( ddrval ) )
	{
		texenum_guess->Release();
		g_guess.m_pD3DDev_guess->Release();
		g_guess.m_pDXMgr_guess->Release();
		CoUninitialize();
		return 0;
	}
	have_guess = false;
	while ( 1 )
	{
		ULONG fetched_guess;

		ddrval = texenum_guess->Next( 1, &g_guess.m_ddLuminanceFormat_guess, &fetched_guess );
		if ( FAILED( ddrval ) )
		{
			texenum_guess->Release();
			g_guess.m_pD3DDev_guess->Release();
			g_guess.m_pDXMgr_guess->Release();
			CoUninitialize();
			return 0;
		}
		if ( fetched_guess == 0 )
			break;
		if ( ( g_guess.m_ddLuminanceFormat_guess.dwFlags & DDPF_LUMINANCE ) && g_guess.m_ddLuminanceFormat_guess.dwLuminanceBitCount == 8 && g_guess.m_ddLuminanceFormat_guess.dwLuminanceBitMask == 0xFF )
		{
			have_guess = true;
			break;
		}
	}
	if ( !have_guess )
	{
		OutputDebugString( "Wrapper: Not using luminance texture\n" );
		g_guess.m_ddLuminanceFormat_guess = g_guess.m_ddFiveBitFormat_guess;
	}
	texenum_guess->Release();

	if ( g_guess.m_dd_guess.dwMaxTextureWidth < 512 || g_guess.m_dd_guess.dwMaxTextureHeight < 512 )
	{
		g_guess.m_subsample_guess = TRUE;
		OutputDebugString( "Wrapper: Subsampling textures to 256 x 256\n" );
	}
	else
	{
		g_guess.m_subsample_guess = FALSE;
	}
	if ( g_guess.m_dd_guess.dpcTriCaps.dwTextureCaps & D3DPTEXTURECAPS_SQUAREONLY )
	{
		g_guess.m_makeSquare_guess = TRUE;
		OutputDebugString( "Wrapper: Forcing all textures to be square\n" );
	}
	else
	{
		g_guess.m_makeSquare_guess = FALSE;
	}
	if ( g_guess.m_dd_guess.wMaxSimultaneousTextures > 1 )
	{
		g_guess.m_usemtex_guess = TRUE;
		OutputDebugString( "Wrapper: Multitexturing enabled\n" );
	}
	else
	{
		g_guess.m_usemtex_guess = FALSE;
		OutputDebugString( "Wrapper: Multitexturing not available with this driver\n" );
	}
	if ( !( g_guess.m_dd_guess.dpcTriCaps.dwTextureFilterCaps & D3DPTFILTERCAPS_LINEARMIPLINEAR ) && !( g_guess.m_dd_guess.dpcTriCaps.dwTextureFilterCaps & D3DPTFILTERCAPS_LINEARMIPNEAREST ) && !( g_guess.m_dd_guess.dpcTriCaps.dwTextureFilterCaps & D3DPTFILTERCAPS_MIPLINEAR ) && !( g_guess.m_dd_guess.dpcTriCaps.dwTextureFilterCaps & D3DPTFILTERCAPS_MIPNEAREST ) )
	{
		g_guess.m_usemipmap_guess = FALSE;
		OutputDebugString( "Wrapper: Mipmapping disabled\n" );
	}

	if ( RegOpenKey( HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Quake", &hKey_guess ) == ERROR_SUCCESS )
	{
		DWORD dwSize = 4, dwType, dwValue;

		if ( RegQueryValueEx( hKey_guess, "DoFlip", NULL, &dwType, (LPBYTE)&dwValue, &dwSize ) == ERROR_SUCCESS && dwType == REG_DWORD && dwValue != 0 )
			g_guess.m_doFlip_guess = TRUE;
		else
			g_guess.m_doFlip_guess = FALSE;
		RegCloseKey( hKey_guess );
	}
	else
	{
		g_guess.m_doFlip_guess = FALSE;
	}
	if ( gl_d3dflip.value )
		g_guess.m_doFlip_guess = TRUE;

	ddrval = g_guess.m_pD3DDev_guess->GetDirect3D( &lpDirect3D_guess );
	if ( FAILED( ddrval ) )
	{
		g_guess.m_pD3DDev_guess->Release();
		g_guess.m_pDXMgr_guess->Release();
		CoUninitialize();
		return 0;
	}
	vbDescr_guess.dwSize = sizeof( vbDescr_guess );
	vbDescr_guess.dwCaps = D3DVBCAPS_WRITEONLY;
	if ( haldesc_guess.dwFlags == 0 )
		vbDescr_guess.dwCaps = D3DVBCAPS_SYSTEMMEMORY;
	vbDescr_guess.dwFVF = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2;
	vbDescr_guess.dwNumVertices = VBNUMVERTS_guess;
	ddrval = lpDirect3D_guess->CreateVertexBuffer( &vbDescr_guess, &g_guess.m_xyzbuf_guess, 0, NULL );
	if ( FAILED( ddrval ) )
	{
		g_guess.m_pD3DDev_guess->Release();
		g_guess.m_pDXMgr_guess->Release();
		CoUninitialize();
		return 0;
	}
	g_guess.m_xyzbuf_guess->Lock( DDLOCK_WAIT | DDLOCK_WRITEONLY | DDLOCK_NOSYSLOCK, (LPVOID *)&g_guess.m_verts_guess, &vbuflen_guess );
	vbDescr_guess.dwFVF = g_guess.m_usemtex_guess ? ( D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX2 ) : D3DFVF_TLVERTEX;
	ddrval = lpDirect3D_guess->CreateVertexBuffer( &vbDescr_guess, &g_guess.m_tlbuf_guess, 0, NULL );
	if ( FAILED( ddrval ) )
	{
		g_guess.m_pD3DDev_guess->Release();
		g_guess.m_pDXMgr_guess->Release();
		CoUninitialize();
		return 0;
	}
	lpDirect3D_guess->Release();

	g_guess.m_cullMode_guess = GL_BACK;
	g_guess.m_cullEnabled_guess = FALSE;
	g_guess.m_updatestage_guess = FALSE;
	g_guess.m_texturing_guess = FALSE;
	g_guess.m_mtex_guess = FALSE;
	g_guess.m_use4444_guess = FALSE;
	g_guess.m_blendmode_guess[ 0 ] = GL_MODULATE;
	g_guess.m_blendmode_guess[ 1 ] = GL_MODULATE;
	g_guess.m_vcnt_guess = 0;
	g_guess.m_pcnt_guess = 0;
	g_guess.m_icnt_guess = 0;
	g_guess.m_curstage_guess = 0;
	for ( ix_guess = 0; ix_guess < MAXGLTEXHANDLES_guess; ix_guess++ )
	{
		g_guess.m_tex_guess[ ix_guess ].m_ddsurf_guess = NULL;
		g_guess.m_tex_guess[ ix_guess ].m_block_guess = NULL;
		g_guess.m_tex_guess[ ix_guess ].m_minmode_guess = D3DTFN_POINT;
		g_guess.m_tex_guess[ ix_guess ].m_magmode_guess = D3DTFG_LINEAR;
		g_guess.m_tex_guess[ ix_guess ].m_mipmode_guess = D3DTFP_LINEAR;
		g_guess.m_tex_guess[ ix_guess ].m_addu_guess = D3DTADDRESS_WRAP;
		g_guess.m_tex_guess[ ix_guess ].m_addv_guess = D3DTADDRESS_WRAP;
	}

	D3DMATRIX identity_guess;

	identity_guess._11 = 1.0f;
	identity_guess._12 = 0.0f;
	identity_guess._13 = 0.0f;
	identity_guess._14 = 0.0f;
	identity_guess._21 = 0.0f;
	identity_guess._22 = 1.0f;
	identity_guess._23 = 0.0f;
	identity_guess._24 = 0.0f;
	identity_guess._31 = 0.0f;
	identity_guess._32 = 0.0f;
	identity_guess._33 = 1.0f;
	identity_guess._34 = 0.0f;
	identity_guess._41 = 0.0f;
	identity_guess._42 = 0.0f;
	identity_guess._43 = 0.0f;
	identity_guess._44 = 1.0f;
	g_guess.m_pD3DDev_guess->SetTransform( D3DTRANSFORMSTATE_VIEW, &identity_guess );
	g_guess.m_pD3DDev_guess->SetTransform( D3DTRANSFORMSTATE_WORLD, &identity_guess );
	g_guess.m_pD3DDev_guess->SetTransform( D3DTRANSFORMSTATE_PROJECTION, &identity_guess );

	for ( ix_guess = 1; ix_guess < 256; ix_guess++ )
		g_guess.m_pD3DDev_guess->GetRenderState( (D3DRENDERSTATETYPE)ix_guess, &g_guess.m_renderStates_guess[ ix_guess ] );
	for ( ix_guess = 1; ix_guess < 24; ix_guess++ )
		g_guess.m_pD3DDev_guess->GetTextureStageState( 0, (D3DTEXTURESTAGESTATETYPE)ix_guess, &g_guess.m_textureStageStates_guess[ 0 ][ ix_guess ] );
	if ( g_guess.m_usemtex_guess )
	{
		for ( ix_guess = 1; ix_guess < 24; ix_guess++ )
			g_guess.m_pD3DDev_guess->GetTextureStageState( 1, (D3DTEXTURESTAGESTATETYPE)ix_guess, &g_guess.m_textureStageStates_guess[ 1 ][ ix_guess ] );
	}
	g_guess.m_curTexture_guess[ 0 ] = NULL;
	g_guess.m_curTexture_guess[ 1 ] = NULL;

	SetRenderState_guess( D3DRENDERSTATE_TEXTUREPERSPECTIVE, TRUE );
	SetRenderState_guess( D3DRENDERSTATE_SPECULARENABLE, FALSE );
	SetRenderState_guess( D3DRENDERSTATE_DITHERENABLE, TRUE );
	SetTextureStageState_guess( 0, D3DTSS_TEXCOORDINDEX, 0 );
	if ( g_guess.m_usemtex_guess == TRUE )
		SetTextureStageState_guess( 1, D3DTSS_TEXCOORDINDEX, 1 );

	ddrval = g_guess.m_pDD_guess->GetDeviceIdentifier( &g_ddDeviceId_guess, DDGDI_GETHOSTIDENTIFIER );
	if ( FAILED( ddrval ) )
	{
		strcpy( g_ddDeviceId_guess.szDriver, "Microsoft Corp." );
		strcpy( g_ddDeviceId_guess.szDescription, "Direct3D" );
	}

	ddrval = g_guess.m_pD3DDev_guess->GetCurrentViewport( &g_guess.m_pD3Dview_guess );
	if ( FAILED( ddrval ) )
	{
		g_guess.m_pD3DDev_guess->Release();
		g_guess.m_pDD_guess->Release();
		g_guess.m_pDXMgr_guess->Release();
		CoUninitialize();
		return 0;
	}
	g_guess.m_pD3DDev_guess->BeginScene();
	return (HGLRC)1;
}

BOOL WINAPI wglDeleteContext( HGLRC hglrc )
{
	int i;

	if ( !g_guess.m_pDXMgr_guess )
		return TRUE;

	g_guess.m_pD3DDev_guess->EndScene();
	for ( i = 0; i < MAXGLTEXHANDLES_guess; i++ )
	{
		if ( g_guess.m_tex_guess[ i ].m_block_guess )
		{
			g_guess.m_tex_guess[ i ].m_block_guess->Release();
			g_guess.m_tex_guess[ i ].m_block_guess = NULL;
		}
		if ( g_guess.m_tex_guess[ i ].m_ddsurf_guess )
		{
			g_guess.m_tex_guess[ i ].m_ddsurf_guess->Release();
			g_guess.m_tex_guess[ i ].m_ddsurf_guess = NULL;
		}
	}
	g_guess.m_pD3Dview_guess->Release();
	g_guess.m_pD3Dview_guess = NULL;
	g_guess.m_xyzbuf_guess->Unlock();
	g_guess.m_xyzbuf_guess->Release();
	g_guess.m_xyzbuf_guess = NULL;
	g_guess.m_tlbuf_guess->Release();
	g_guess.m_tlbuf_guess = NULL;
	g_guess.m_pD3DDev_guess->Release();
	g_guess.m_pD3DDev_guess = NULL;
	g_guess.m_pD3DDevProcess_guess = NULL;
	g_guess.m_pDD_guess->Release();
	g_guess.m_pDD_guess = NULL;
	g_guess.m_pDXMgr_guess->Release();
	g_guess.m_pDXMgr_guess = NULL;
	CoUninitialize();
	return TRUE;
}

extern "C" int WINAPI wglDescribePixelFormat( HDC hdc, int iPixelFormat, UINT nBytes, LPPIXELFORMATDESCRIPTOR ppfd )
{
	ppfd->nSize = sizeof( PIXELFORMATDESCRIPTOR );
	ppfd->nVersion = 1;
	ppfd->dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_GENERIC_ACCELERATED;
	ppfd->iPixelType = PFD_TYPE_RGBA;
	ppfd->cColorBits = 15;
	ppfd->cRedBits = 5;
	ppfd->cRedShift = 0;
	ppfd->cGreenBits = 5;
	ppfd->cGreenShift = 0;
	ppfd->cBlueBits = 5;
	ppfd->cBlueShift = 0;
	ppfd->cAlphaBits = 1;
	ppfd->cAlphaShift = 0;
	ppfd->cAccumBits = 0;
	ppfd->cAccumRedBits = 0;
	ppfd->cAccumGreenBits = 0;
	ppfd->cAccumBlueBits = 0;
	ppfd->cAccumAlphaBits = 0;
	ppfd->cDepthBits = 16;
	ppfd->cStencilBits = 0;
	ppfd->cAuxBuffers = 0;
	ppfd->iLayerType = 0;
	ppfd->bReserved = 0;
	ppfd->dwLayerMask = 0;
	ppfd->dwVisibleMask = 0;
	ppfd->dwDamageMask = 0;
	return 1;
}

HGLRC WINAPI wglGetCurrentContext( VOID )
{
	return (HGLRC)1;
}

HDC WINAPI wglGetCurrentDC( VOID )
{
	return g_guess.m_hdc_guess;
}

extern "C" int WINAPI wglGetPixelFormat( HDC hdc )
{
	return 1;
}

extern "C" void APIENTRY glMTexCoord2fSGIS( GLenum target, GLfloat s, GLfloat t );
extern "C" void APIENTRY glSelectTextureSGIS( GLenum target );

PROC WINAPI wglGetProcAddress( LPCSTR lpszProc )
{
	if ( !strcmp( lpszProc, "glMTexCoord2fSGIS" ) )
		return (PROC)glMTexCoord2fSGIS;
	else if ( !strcmp( lpszProc, "glSelectTextureSGIS" ) )
		return (PROC)glSelectTextureSGIS;
	return NULL;
}

BOOL WINAPI wglMakeCurrent( HDC hdc, HGLRC hglrc )
{
	return TRUE;
}

extern "C" BOOL WINAPI wglSetPixelFormat( HDC hdc, int iPixelFormat, CONST PIXELFORMATDESCRIPTOR *ppfd )
{
	return TRUE;
}

extern "C" BOOL WINAPI wglSwapBuffers( HDC hdc )
{
	FlushPrimitives_guess();
	g_guess.m_pD3DDev_guess->EndScene();
	if ( gl_d3dflip.value )
		g_guess.m_doFlip_guess = TRUE;
	g_guess.m_pDXMgr_guess->Present_guess( g_guess.m_doFlip_guess ? 0 : 2, 0 );
	g_guess.m_pD3DDev_guess->BeginScene();
	return TRUE;
}

GLenum APIENTRY glGetError (void)
{
	return GL_NO_ERROR;
}

void APIENTRY glDeleteTextures (GLsizei n, const GLuint *textures)
{
	for ( int i = 0; i < n; i++ )
	{
		TexInfo_guess &ti_guess = g_guess.m_tex_guess[ textures[ i ] ];

		if ( ti_guess.m_block_guess )
		{
			ti_guess.m_block_guess->Release();
			ti_guess.m_block_guess = NULL;
		}
		if ( ti_guess.m_ddsurf_guess )
		{
			ti_guess.m_ddsurf_guess->Release();
			ti_guess.m_ddsurf_guess = NULL;
		}
		ti_guess.m_minmode_guess = D3DTFN_POINT;
		ti_guess.m_magmode_guess = D3DTFG_LINEAR;
		ti_guess.m_mipmode_guess = D3DTFP_LINEAR;
		ti_guess.m_addu_guess = D3DTADDRESS_WRAP;
		ti_guess.m_addv_guess = D3DTADDRESS_WRAP;
	}
}

void APIENTRY glTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
	TexInfo_guess			*tex_guess = &g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ];
	LPDIRECTDRAWSURFACE4	lpDDSAttached_guess;
	LPDIRECTDRAWSURFACE4	lpDDS = tex_guess->m_ddsurf_guess;
	DDSURFACEDESC2			desc_guess;
	RECT					rc_guess;
	int						i;

	for ( i = 0; i < level; i++ )
	{
		Q_memset( &desc_guess.ddsCaps, 0, sizeof( desc_guess.ddsCaps ) );
		desc_guess.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP;
		HRESULT ddrval = lpDDS->GetAttachedSurface( &desc_guess.ddsCaps, &lpDDSAttached_guess );
		if ( ddrval == DDERR_NOTFOUND )
			return;
		lpDDS = lpDDSAttached_guess;
		lpDDS->Release();
	}
	xoffset = ( xoffset * tex_guess->m_dwWidth_guess ) / tex_guess->m_glWidth_guess;
	yoffset = ( yoffset * tex_guess->m_dwHeight_guess ) / tex_guess->m_glHeight_guess;
	SetRect( &rc_guess, xoffset, yoffset, ( width * tex_guess->m_dwWidth_guess ) / tex_guess->m_glWidth_guess + xoffset, ( height * tex_guess->m_dwHeight_guess ) / tex_guess->m_glHeight_guess + yoffset );
	if ( LoadSubSurface_guess( lpDDS, tex_guess->m_internalformat_guess, width, height, pixels, &rc_guess ) != DD_OK )
		OutputDebugString( "Wrapper: LoadSubSurface Failure.\n" );
}

void APIENTRY glTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	TexInfo_guess	*tex_guess = &g_guess.m_tex_guess[ g_guess.m_curtgt_guess[ g_guess.m_curstage_guess ] ];
	DWORD			nw_guess, nh_guess;

	if ( g_guess.m_subsample_guess )
	{
		if ( width > 256 || height > 256 )
		{
			if ( width > height )
			{
				nw_guess = 256;
				nh_guess = ( height << 8 ) / width;
			}
			else
			{
				nh_guess = 256;
				nw_guess = ( width << 8 ) / height;
			}
		}
		else
		{
			nw_guess = width;
			nh_guess = height;
		}
	}
	else
	{
		nw_guess = width;
		nh_guess = height;
	}
	if ( g_guess.m_makeSquare_guess )
	{
		if ( nh_guess > nw_guess )
			nw_guess = nh_guess;
		else
			nh_guess = nw_guess;
	}

	if ( level == 0 )
	{
		DDSURFACEDESC2			ddsd;
		LPDIRECTDRAWSURFACE4	lpDDS;
		HRESULT					ddrval;

		Q_memset( &ddsd, 0, sizeof( ddsd ) );
		switch ( internalformat )
		{
		case 1:
			ddsd.ddpfPixelFormat = g_guess.m_ddLuminanceFormat_guess;
			break;
		case 3:
			ddsd.ddpfPixelFormat = g_guess.m_ddFiveBitFormat_guess;
			break;
		case 4:
		case GL_RGBA:
		case GL_RGB5:
		case GL_RGBA4:
		case GL_RGB5_A1:
		case GL_RGBA8:
			if ( g_guess.m_use4444_guess )
			{
				ddsd.ddpfPixelFormat = g_guess.m_ddFourBitAlphaFormat_guess;
				g_guess.m_use4444_guess = FALSE;
			}
			else
			{
				ddsd.ddpfPixelFormat = g_guess.m_ddFiveBitAlphaFormat_guess;
			}
			break;
		default:
			OutputDebugString( "Wrapper: texture format not supported\n" );
			break;
		}
		ddsd.dwSize = sizeof( ddsd );
		ddsd.dwHeight = nh_guess;
		ddsd.dwWidth = nw_guess;
		ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_TEXTURESTAGE;
		ddsd.dwTextureStage = g_guess.m_curstage_guess;
		ddsd.ddsCaps.dwCaps = DDSCAPS_TEXTURE;
		ddsd.ddsCaps.dwCaps2 = DDSCAPS2_TEXTUREMANAGE | DDSCAPS2_HINTDYNAMIC;
		ddrval = g_guess.m_pDD_guess->CreateSurface( &ddsd, &lpDDS, NULL );
		if ( ddrval != DD_OK )
		{
			OutputDebugString( "Wrapper: CreateSurface for texture failed\n" );
			return;
		}
		LoadSurface_guess( lpDDS, internalformat, width, height, nw_guess, nh_guess, pixels );
		if ( tex_guess->m_block_guess )
			tex_guess->m_block_guess->Release();
		if ( tex_guess->m_ddsurf_guess )
			tex_guess->m_ddsurf_guess->Release();
		if ( lpDDS->QueryInterface( IID_IDirect3DTexture2, (LPVOID *)&tex_guess->m_block_guess ) != DD_OK )
		{
			OutputDebugString( "Wrapper: QueryInterface for Texture2 failed.\n" );
			lpDDS->Release();
			return;
		}
		tex_guess->m_internalformat_guess = internalformat;
		tex_guess->m_dwWidth_guess = nw_guess;
		tex_guess->m_dwHeight_guess = nh_guess;
		tex_guess->m_ddsurf_guess = lpDDS;
		tex_guess->m_glWidth_guess = width;
		tex_guess->m_glHeight_guess = height;
	}
	else if ( level == 1 && g_guess.m_usemipmap_guess )
	{
		DDSURFACEDESC2			ddsd;
		LPDIRECTDRAWSURFACE4	lpDDS, lpDDSMip_guess;
		HRESULT					ddrval;

		Q_memset( &ddsd, 0, sizeof( ddsd ) );
		switch ( internalformat )
		{
		case 1:
			ddsd.ddpfPixelFormat = g_guess.m_ddLuminanceFormat_guess;
			break;
		case 3:
			ddsd.ddpfPixelFormat = g_guess.m_ddFiveBitFormat_guess;
			break;
		case 4:
		case GL_RGBA:
		case GL_RGB5:
		case GL_RGBA4:
		case GL_RGB5_A1:
		case GL_RGBA8:
			if ( g_guess.m_use4444_guess )
			{
				ddsd.ddpfPixelFormat = g_guess.m_ddFourBitAlphaFormat_guess;
				g_guess.m_use4444_guess = FALSE;
			}
			else
			{
				ddsd.ddpfPixelFormat = g_guess.m_ddFiveBitAlphaFormat_guess;
			}
			break;
		}
		ddsd.dwSize = sizeof( ddsd );
		ddsd.dwHeight = tex_guess->m_dwHeight_guess;
		ddsd.dwWidth = tex_guess->m_dwWidth_guess;
		ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_TEXTURESTAGE;
		ddsd.dwTextureStage = g_guess.m_curstage_guess;
		ddsd.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP | DDSCAPS_COMPLEX;
		ddsd.ddsCaps.dwCaps2 = DDSCAPS2_TEXTUREMANAGE | DDSCAPS2_HINTDYNAMIC;
		ddrval = g_guess.m_pDD_guess->CreateSurface( &ddsd, &lpDDS, NULL );
		if ( ddrval != DD_OK )
		{
			OutputDebugString( "Wrapper: CreateSurface for texture failed\n" );
			return;
		}
		lpDDS->Blt( NULL, tex_guess->m_ddsurf_guess, NULL, DDBLT_WAIT, NULL );
		tex_guess->m_block_guess->Release();
		tex_guess->m_ddsurf_guess->Release();
		tex_guess->m_ddsurf_guess = lpDDS;
		tex_guess->m_ddsurf_guess->QueryInterface( IID_IDirect3DTexture2, (LPVOID *)&tex_guess->m_block_guess );
		Q_memset( &ddsd.ddsCaps, 0, sizeof( ddsd.ddsCaps ) );
		ddsd.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP;
		tex_guess->m_ddsurf_guess->GetAttachedSurface( &ddsd.ddsCaps, &lpDDSMip_guess );
		LoadSurface_guess( lpDDSMip_guess, internalformat, width, height, nw_guess, nh_guess, pixels );
		lpDDSMip_guess->Release();
	}
	else if ( g_guess.m_usemipmap_guess )
	{
		LPDIRECTDRAWSURFACE4	lpDDSAttached_guess;
		LPDIRECTDRAWSURFACE4	lpDDS;
		DDSURFACEDESC2			desc_guess;
		int						i;

		lpDDS = tex_guess->m_ddsurf_guess;
		lpDDS->AddRef();
		for ( i = 0; i < level; i++ )
		{
			Q_memset( &desc_guess.ddsCaps, 0, sizeof( desc_guess.ddsCaps ) );
			desc_guess.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP;
			HRESULT ddrval = lpDDS->GetAttachedSurface( &desc_guess.ddsCaps, &lpDDSAttached_guess );
			lpDDS->Release();
			if ( ddrval == DDERR_NOTFOUND )
				return;
			lpDDS = lpDDSAttached_guess;
		}
		LoadSurface_guess( lpDDS, internalformat, width, height, nw_guess, nh_guess, pixels );
		lpDDS->Release();
	}
	g_guess.m_updatestage_guess = FALSE;
}

void APIENTRY glMTexCoord2fSGIS( GLenum target, GLfloat s, GLfloat t )
{
	if ( target == GL_TEXTURE0_SGIS_guess )
	{
		g_guess.m_tu_guess = s;
		g_guess.m_tv_guess = t;
	}
	else
	{
		g_guess.m_tu2_guess = s;
		g_guess.m_tv2_guess = t;
	}
}

void APIENTRY glSelectTextureSGIS( GLenum target )
{
	g_guess.m_curstage_guess = target != GL_TEXTURE0_SGIS_guess;
}

//-----------------------------------------------------------------------------
// Purpose: the next texture upload uses the 4444 format
//-----------------------------------------------------------------------------
extern "C" __declspec( dllexport ) void Download4444( void )
{
	g_guess.m_use4444_guess = TRUE;
}

typedef struct tagD3DGlobals
{
	HWND			hWnd_guess;
	int				bFullScreen_guess;
	int				bOwnDisplayMode_guess;
	LPDIRECTDRAW4	lpDD_guess;
	int				unknown10_guess;
	int				unknown14_guess;
	int				unknown18_guess;
	int				bStencil_guess;
} D3DGlobals;

//-----------------------------------------------------------------------------
// Purpose: takes the DirectDraw object and the window mode from the video mode code
//-----------------------------------------------------------------------------
extern "C" void QGL_D3DShared( D3DGlobals *d3dGShared )
{
	g_guess.m_pDD_guess = d3dGShared->lpDD_guess;
	g_guess.m_fullscreen_guess = d3dGShared->bFullScreen_guess;
	OutputDebugString( "setting dd ipntr in dll\n" );
}

//-----------------------------------------------------------------------------
// Purpose: points the qgl function table at the wrapper
//-----------------------------------------------------------------------------
extern "C" __declspec( dllexport ) qboolean QGL_D3DInit( void )
{
	qglAccum = dllAccum = glAccum;
	qglAlphaFunc = dllAlphaFunc = glAlphaFunc;
	qglAreTexturesResident = dllAreTexturesResident = glAreTexturesResident;
	qglArrayElement = dllArrayElement = glArrayElement;
	qglBegin = dllBegin = glBegin;
	qglBindTexture = dllBindTexture = glBindTexture;
	qglBitmap = dllBitmap = glBitmap;
	qglBlendFunc = dllBlendFunc = glBlendFunc;
	qglCallList = dllCallList = glCallList;
	qglCallLists = dllCallLists = glCallLists;
	qglClear = dllClear = glClear;
	qglClearAccum = dllClearAccum = glClearAccum;
	qglClearColor = dllClearColor = glClearColor;
	qglClearDepth = dllClearDepth = glClearDepth;
	qglClearIndex = dllClearIndex = glClearIndex;
	qglClearStencil = dllClearStencil = glClearStencil;
	qglClipPlane = dllClipPlane = glClipPlane;
	qglColor3b = dllColor3b = glColor3b;
	qglColor3bv = dllColor3bv = glColor3bv;
	qglColor3d = dllColor3d = glColor3d;
	qglColor3dv = dllColor3dv = glColor3dv;
	qglColor3f = dllColor3f = glColor3f;
	qglColor3fv = dllColor3fv = glColor3fv;
	qglColor3i = dllColor3i = glColor3i;
	qglColor3iv = dllColor3iv = glColor3iv;
	qglColor3s = dllColor3s = glColor3s;
	qglColor3sv = dllColor3sv = glColor3sv;
	qglColor3ub = dllColor3ub = glColor3ub;
	qglColor3ubv = dllColor3ubv = glColor3ubv;
	qglColor3ui = dllColor3ui = glColor3ui;
	qglColor3uiv = dllColor3uiv = glColor3uiv;
	qglColor3us = dllColor3us = glColor3us;
	qglColor3usv = dllColor3usv = glColor3usv;
	qglColor4b = dllColor4b = glColor4b;
	qglColor4bv = dllColor4bv = glColor4bv;
	qglColor4d = dllColor4d = glColor4d;
	qglColor4dv = dllColor4dv = glColor4dv;
	qglColor4f = dllColor4f = glColor4f;
	qglColor4fv = dllColor4fv = glColor4fv;
	qglColor4i = dllColor4i = glColor4i;
	qglColor4iv = dllColor4iv = glColor4iv;
	qglColor4s = dllColor4s = glColor4s;
	qglColor4sv = dllColor4sv = glColor4sv;
	qglColor4ub = dllColor4ub = glColor4ub;
	qglColor4ubv = dllColor4ubv = glColor4ubv;
	qglColor4ui = dllColor4ui = glColor4ui;
	qglColor4uiv = dllColor4uiv = glColor4uiv;
	qglColor4us = dllColor4us = glColor4us;
	qglColor4usv = dllColor4usv = glColor4usv;
	qglColorMask = dllColorMask = glColorMask;
	qglColorMaterial = dllColorMaterial = glColorMaterial;
	qglColorPointer = dllColorPointer = glColorPointer;
	qglCopyPixels = dllCopyPixels = glCopyPixels;
	qglCopyTexImage1D = dllCopyTexImage1D = glCopyTexImage1D;
	qglCopyTexImage2D = dllCopyTexImage2D = glCopyTexImage2D;
	qglCopyTexSubImage1D = dllCopyTexSubImage1D = glCopyTexSubImage1D;
	qglCopyTexSubImage2D = dllCopyTexSubImage2D = glCopyTexSubImage2D;
	qglCullFace = dllCullFace = glCullFace;
	qglDeleteLists = dllDeleteLists = glDeleteLists;
	qglDeleteTextures = dllDeleteTextures = glDeleteTextures;
	qglDepthFunc = dllDepthFunc = glDepthFunc;
	qglDepthMask = dllDepthMask = glDepthMask;
	qglDepthRange = dllDepthRange = glDepthRange;
	qglDisable = dllDisable = glDisable;
	qglDisableClientState = dllDisableClientState = glDisableClientState;
	qglDrawArrays = dllDrawArrays = glDrawArrays;
	qglDrawBuffer = dllDrawBuffer = glDrawBuffer;
	qglDrawElements = dllDrawElements = glDrawElements;
	qglDrawPixels = dllDrawPixels = glDrawPixels;
	qglEdgeFlag = dllEdgeFlag = glEdgeFlag;
	qglEdgeFlagPointer = dllEdgeFlagPointer = glEdgeFlagPointer;
	qglEdgeFlagv = dllEdgeFlagv = glEdgeFlagv;
	qglEnable = dllEnable = glEnable;
	qglEnableClientState = dllEnableClientState = glEnableClientState;
	qglEnd = dllEnd = glEnd;
	qglEndList = dllEndList = glEndList;
	qglEvalCoord1d = dllEvalCoord1d = glEvalCoord1d;
	qglEvalCoord1dv = dllEvalCoord1dv = glEvalCoord1dv;
	qglEvalCoord1f = dllEvalCoord1f = glEvalCoord1f;
	qglEvalCoord1fv = dllEvalCoord1fv = glEvalCoord1fv;
	qglEvalCoord2d = dllEvalCoord2d = glEvalCoord2d;
	qglEvalCoord2dv = dllEvalCoord2dv = glEvalCoord2dv;
	qglEvalCoord2f = dllEvalCoord2f = glEvalCoord2f;
	qglEvalCoord2fv = dllEvalCoord2fv = glEvalCoord2fv;
	qglEvalMesh1 = dllEvalMesh1 = glEvalMesh1;
	qglEvalMesh2 = dllEvalMesh2 = glEvalMesh2;
	qglEvalPoint1 = dllEvalPoint1 = glEvalPoint1;
	qglEvalPoint2 = dllEvalPoint2 = glEvalPoint2;
	qglFeedbackBuffer = dllFeedbackBuffer = glFeedbackBuffer;
	qglFinish = dllFinish = glFinish;
	qglFlush = dllFlush = glFlush;
	qglFogf = dllFogf = glFogf;
	qglFogfv = dllFogfv = glFogfv;
	qglFogi = dllFogi = glFogi;
	qglFogiv = dllFogiv = glFogiv;
	qglFrontFace = dllFrontFace = glFrontFace;
	qglFrustum = dllFrustum = glFrustum;
	qglGenLists = dllGenLists = glGenLists;
	qglGenTextures = dllGenTextures = glGenTextures;
	qglGetBooleanv = dllGetBooleanv = glGetBooleanv;
	qglGetClipPlane = dllGetClipPlane = glGetClipPlane;
	qglGetDoublev = dllGetDoublev = glGetDoublev;
	qglGetError = dllGetError = glGetError;
	qglGetFloatv = dllGetFloatv = glGetFloatv;
	qglGetIntegerv = dllGetIntegerv = glGetIntegerv;
	qglGetLightfv = dllGetLightfv = glGetLightfv;
	qglGetLightiv = dllGetLightiv = glGetLightiv;
	qglGetMapdv = dllGetMapdv = glGetMapdv;
	qglGetMapfv = dllGetMapfv = glGetMapfv;
	qglGetMapiv = dllGetMapiv = glGetMapiv;
	qglGetMaterialfv = dllGetMaterialfv = glGetMaterialfv;
	qglGetMaterialiv = dllGetMaterialiv = glGetMaterialiv;
	qglGetPixelMapfv = dllGetPixelMapfv = glGetPixelMapfv;
	qglGetPixelMapuiv = dllGetPixelMapuiv = glGetPixelMapuiv;
	qglGetPixelMapusv = dllGetPixelMapusv = glGetPixelMapusv;
	qglGetPointerv = dllGetPointerv = glGetPointerv;
	qglGetPolygonStipple = dllGetPolygonStipple = glGetPolygonStipple;
	qglGetString = dllGetString = glGetString;
	qglGetTexEnvfv = dllGetTexEnvfv = glGetTexEnvfv;
	qglGetTexEnviv = dllGetTexEnviv = glGetTexEnviv;
	qglGetTexGendv = dllGetTexGendv = glGetTexGendv;
	qglGetTexGenfv = dllGetTexGenfv = glGetTexGenfv;
	qglGetTexGeniv = dllGetTexGeniv = glGetTexGeniv;
	qglGetTexImage = dllGetTexImage = glGetTexImage;
	qglGetTexLevelParameterfv = dllGetTexLevelParameterfv = NULL;
	qglGetTexLevelParameteriv = dllGetTexLevelParameteriv = NULL;
	qglGetTexParameterfv = dllGetTexParameterfv = glGetTexParameterfv;
	qglGetTexParameteriv = dllGetTexParameteriv = glGetTexParameteriv;
	qglHint = dllHint = glHint;
	qglIndexMask = dllIndexMask = glIndexMask;
	qglIndexPointer = dllIndexPointer = glIndexPointer;
	qglIndexd = dllIndexd = glIndexd;
	qglIndexdv = dllIndexdv = glIndexdv;
	qglIndexf = dllIndexf = glIndexf;
	qglIndexfv = dllIndexfv = glIndexfv;
	qglIndexi = dllIndexi = glIndexi;
	qglIndexiv = dllIndexiv = glIndexiv;
	qglIndexs = dllIndexs = glIndexs;
	qglIndexsv = dllIndexsv = glIndexsv;
	qglIndexub = dllIndexub = glIndexub;
	qglIndexubv = dllIndexubv = glIndexubv;
	qglInitNames = dllInitNames = glInitNames;
	qglInterleavedArrays = dllInterleavedArrays = glInterleavedArrays;
	qglIsEnabled = dllIsEnabled = glIsEnabled;
	qglIsList = dllIsList = glIsList;
	qglIsTexture = dllIsTexture = glIsTexture;
	qglLightModelf = dllLightModelf = glLightModelf;
	qglLightModelfv = dllLightModelfv = glLightModelfv;
	qglLightModeli = dllLightModeli = glLightModeli;
	qglLightModeliv = dllLightModeliv = glLightModeliv;
	qglLightf = dllLightf = glLightf;
	qglLightfv = dllLightfv = glLightfv;
	qglLighti = dllLighti = glLighti;
	qglLightiv = dllLightiv = glLightiv;
	qglLineStipple = dllLineStipple = glLineStipple;
	qglLineWidth = dllLineWidth = glLineWidth;
	qglListBase = dllListBase = glListBase;
	qglLoadIdentity = dllLoadIdentity = glLoadIdentity;
	qglLoadMatrixd = dllLoadMatrixd = glLoadMatrixd;
	qglLoadMatrixf = dllLoadMatrixf = glLoadMatrixf;
	qglLoadName = dllLoadName = glLoadName;
	qglLogicOp = dllLogicOp = glLogicOp;
	qglMap1d = dllMap1d = glMap1d;
	qglMap1f = dllMap1f = glMap1f;
	qglMap2d = dllMap2d = glMap2d;
	qglMap2f = dllMap2f = glMap2f;
	qglMapGrid1d = dllMapGrid1d = glMapGrid1d;
	qglMapGrid1f = dllMapGrid1f = glMapGrid1f;
	qglMapGrid2d = dllMapGrid2d = glMapGrid2d;
	qglMapGrid2f = dllMapGrid2f = glMapGrid2f;
	qglMaterialf = dllMaterialf = glMaterialf;
	qglMaterialfv = dllMaterialfv = glMaterialfv;
	qglMateriali = dllMateriali = glMateriali;
	qglMaterialiv = dllMaterialiv = glMaterialiv;
	qglMatrixMode = dllMatrixMode = glMatrixMode;
	qglMultMatrixd = dllMultMatrixd = glMultMatrixd;
	qglMultMatrixf = dllMultMatrixf = glMultMatrixf;
	qglNewList = dllNewList = glNewList;
	qglNormal3b = dllNormal3b = glNormal3b;
	qglNormal3bv = dllNormal3bv = glNormal3bv;
	qglNormal3d = dllNormal3d = glNormal3d;
	qglNormal3dv = dllNormal3dv = glNormal3dv;
	qglNormal3f = dllNormal3f = glNormal3f;
	qglNormal3fv = dllNormal3fv = glNormal3fv;
	qglNormal3i = dllNormal3i = glNormal3i;
	qglNormal3iv = dllNormal3iv = glNormal3iv;
	qglNormal3s = dllNormal3s = glNormal3s;
	qglNormal3sv = dllNormal3sv = glNormal3sv;
	qglNormalPointer = dllNormalPointer = glNormalPointer;
	qglOrtho = dllOrtho = glOrtho;
	qglPassThrough = dllPassThrough = glPassThrough;
	qglPixelMapfv = dllPixelMapfv = glPixelMapfv;
	qglPixelMapuiv = dllPixelMapuiv = glPixelMapuiv;
	qglPixelMapusv = dllPixelMapusv = glPixelMapusv;
	qglPixelStoref = dllPixelStoref = glPixelStoref;
	qglPixelStorei = dllPixelStorei = glPixelStorei;
	qglPixelTransferf = dllPixelTransferf = glPixelTransferf;
	qglPixelTransferi = dllPixelTransferi = glPixelTransferi;
	qglPixelZoom = dllPixelZoom = glPixelZoom;
	qglPointSize = dllPointSize = glPointSize;
	qglPolygonMode = dllPolygonMode = glPolygonMode;
	qglPolygonOffset = dllPolygonOffset = glPolygonOffset;
	qglPolygonStipple = dllPolygonStipple = glPolygonStipple;
	qglPopAttrib = dllPopAttrib = glPopAttrib;
	qglPopClientAttrib = dllPopClientAttrib = glPopClientAttrib;
	qglPopMatrix = dllPopMatrix = glPopMatrix;
	qglPopName = dllPopName = glPopName;
	qglPrioritizeTextures = dllPrioritizeTextures = glPrioritizeTextures;
	qglPushAttrib = dllPushAttrib = glPushAttrib;
	qglPushClientAttrib = dllPushClientAttrib = glPushClientAttrib;
	qglPushMatrix = dllPushMatrix = glPushMatrix;
	qglPushName = dllPushName = glPushName;
	qglRasterPos2d = dllRasterPos2d = glRasterPos2d;
	qglRasterPos2dv = dllRasterPos2dv = glRasterPos2dv;
	qglRasterPos2f = dllRasterPos2f = glRasterPos2f;
	qglRasterPos2fv = dllRasterPos2fv = glRasterPos2fv;
	qglRasterPos2i = dllRasterPos2i = glRasterPos2i;
	qglRasterPos2iv = dllRasterPos2iv = glRasterPos2iv;
	qglRasterPos2s = dllRasterPos2s = glRasterPos2s;
	qglRasterPos2sv = dllRasterPos2sv = glRasterPos2sv;
	qglRasterPos3d = dllRasterPos3d = glRasterPos3d;
	qglRasterPos3dv = dllRasterPos3dv = glRasterPos3dv;
	qglRasterPos3f = dllRasterPos3f = glRasterPos3f;
	qglRasterPos3fv = dllRasterPos3fv = glRasterPos3fv;
	qglRasterPos3i = dllRasterPos3i = glRasterPos3i;
	qglRasterPos3iv = dllRasterPos3iv = glRasterPos3iv;
	qglRasterPos3s = dllRasterPos3s = glRasterPos3s;
	qglRasterPos3sv = dllRasterPos3sv = glRasterPos3sv;
	qglRasterPos4d = dllRasterPos4d = glRasterPos4d;
	qglRasterPos4dv = dllRasterPos4dv = glRasterPos4dv;
	qglRasterPos4f = dllRasterPos4f = glRasterPos4f;
	qglRasterPos4fv = dllRasterPos4fv = glRasterPos4fv;
	qglRasterPos4i = dllRasterPos4i = glRasterPos4i;
	qglRasterPos4iv = dllRasterPos4iv = glRasterPos4iv;
	qglRasterPos4s = dllRasterPos4s = glRasterPos4s;
	qglRasterPos4sv = dllRasterPos4sv = glRasterPos4sv;
	qglReadBuffer = dllReadBuffer = glReadBuffer;
	qglReadPixels = dllReadPixels = glReadPixels;
	qglRectd = dllRectd = glRectd;
	qglRectdv = dllRectdv = glRectdv;
	qglRectf = dllRectf = glRectf;
	qglRectfv = dllRectfv = glRectfv;
	qglRecti = dllRecti = glRecti;
	qglRectiv = dllRectiv = glRectiv;
	qglRects = dllRects = glRects;
	qglRectsv = dllRectsv = glRectsv;
	qglRenderMode = dllRenderMode = glRenderMode;
	qglRotated = dllRotated = glRotated;
	qglRotatef = dllRotatef = glRotatef;
	qglScaled = dllScaled = glScaled;
	qglScalef = dllScalef = glScalef;
	qglScissor = dllScissor = glScissor;
	qglSelectBuffer = dllSelectBuffer = glSelectBuffer;
	qglShadeModel = dllShadeModel = glShadeModel;
	qglStencilFunc = dllStencilFunc = glStencilFunc;
	qglStencilMask = dllStencilMask = glStencilMask;
	qglStencilOp = dllStencilOp = glStencilOp;
	qglTexCoord1d = dllTexCoord1d = glTexCoord1d;
	qglTexCoord1dv = dllTexCoord1dv = glTexCoord1dv;
	qglTexCoord1f = dllTexCoord1f = glTexCoord1f;
	qglTexCoord1fv = dllTexCoord1fv = glTexCoord1fv;
	qglTexCoord1i = dllTexCoord1i = glTexCoord1i;
	qglTexCoord1iv = dllTexCoord1iv = glTexCoord1iv;
	qglTexCoord1s = dllTexCoord1s = glTexCoord1s;
	qglTexCoord1sv = dllTexCoord1sv = glTexCoord1sv;
	qglTexCoord2d = dllTexCoord2d = glTexCoord2d;
	qglTexCoord2dv = dllTexCoord2dv = glTexCoord2dv;
	qglTexCoord2f = dllTexCoord2f = glTexCoord2f;
	qglTexCoord2fv = dllTexCoord2fv = glTexCoord2fv;
	qglTexCoord2i = dllTexCoord2i = glTexCoord2i;
	qglTexCoord2iv = dllTexCoord2iv = glTexCoord2iv;
	qglTexCoord2s = dllTexCoord2s = glTexCoord2s;
	qglTexCoord2sv = dllTexCoord2sv = glTexCoord2sv;
	qglTexCoord3d = dllTexCoord3d = glTexCoord3d;
	qglTexCoord3dv = dllTexCoord3dv = glTexCoord3dv;
	qglTexCoord3f = dllTexCoord3f = glTexCoord3f;
	qglTexCoord3fv = dllTexCoord3fv = glTexCoord3fv;
	qglTexCoord3i = dllTexCoord3i = glTexCoord3i;
	qglTexCoord3iv = dllTexCoord3iv = glTexCoord3iv;
	qglTexCoord3s = dllTexCoord3s = glTexCoord3s;
	qglTexCoord3sv = dllTexCoord3sv = glTexCoord3sv;
	qglTexCoord4d = dllTexCoord4d = glTexCoord4d;
	qglTexCoord4dv = dllTexCoord4dv = glTexCoord4dv;
	qglTexCoord4f = dllTexCoord4f = glTexCoord4f;
	qglTexCoord4fv = dllTexCoord4fv = glTexCoord4fv;
	qglTexCoord4i = dllTexCoord4i = glTexCoord4i;
	qglTexCoord4iv = dllTexCoord4iv = glTexCoord4iv;
	qglTexCoord4s = dllTexCoord4s = glTexCoord4s;
	qglTexCoord4sv = dllTexCoord4sv = glTexCoord4sv;
	qglTexCoordPointer = dllTexCoordPointer = glTexCoordPointer;
	qglTexEnvf = dllTexEnvf = glTexEnvf;
	qglTexEnvfv = dllTexEnvfv = glTexEnvfv;
	qglTexEnvi = dllTexEnvi = glTexEnvi;
	qglTexEnviv = dllTexEnviv = glTexEnviv;
	qglTexGend = dllTexGend = glTexGend;
	qglTexGendv = dllTexGendv = glTexGendv;
	qglTexGenf = dllTexGenf = glTexGenf;
	qglTexGenfv = dllTexGenfv = glTexGenfv;
	qglTexGeni = dllTexGeni = glTexGeni;
	qglTexGeniv = dllTexGeniv = glTexGeniv;
	qglTexImage1D = dllTexImage1D = glTexImage1D;
	qglTexImage2D = dllTexImage2D = glTexImage2D;
	qglTexParameterf = dllTexParameterf = glTexParameterf;
	qglTexParameterfv = dllTexParameterfv = glTexParameterfv;
	qglTexParameteri = dllTexParameteri = glTexParameteri;
	qglTexParameteriv = dllTexParameteriv = glTexParameteriv;
	qglTexSubImage1D = dllTexSubImage1D = glTexSubImage1D;
	qglTexSubImage2D = dllTexSubImage2D = glTexSubImage2D;
	qglTranslated = dllTranslated = glTranslated;
	qglTranslatef = dllTranslatef = glTranslatef;
	qglVertex2d = dllVertex2d = glVertex2d;
	qglVertex2dv = dllVertex2dv = glVertex2dv;
	qglVertex2f = dllVertex2f = glVertex2f;
	qglVertex2fv = dllVertex2fv = glVertex2fv;
	qglVertex2i = dllVertex2i = glVertex2i;
	qglVertex2iv = dllVertex2iv = glVertex2iv;
	qglVertex2s = dllVertex2s = glVertex2s;
	qglVertex2sv = dllVertex2sv = glVertex2sv;
	qglVertex3d = dllVertex3d = glVertex3d;
	qglVertex3dv = dllVertex3dv = glVertex3dv;
	qglVertex3f = dllVertex3f = glVertex3f;
	qglVertex3fv = dllVertex3fv = glVertex3fv;
	qglVertex3i = dllVertex3i = glVertex3i;
	qglVertex3iv = dllVertex3iv = glVertex3iv;
	qglVertex3s = dllVertex3s = glVertex3s;
	qglVertex3sv = dllVertex3sv = glVertex3sv;
	qglVertex4d = dllVertex4d = glVertex4d;
	qglVertex4dv = dllVertex4dv = glVertex4dv;
	qglVertex4f = dllVertex4f = glVertex4f;
	qglVertex4fv = dllVertex4fv = glVertex4fv;
	qglVertex4i = dllVertex4i = glVertex4i;
	qglVertex4iv = dllVertex4iv = glVertex4iv;
	qglVertex4s = dllVertex4s = glVertex4s;
	qglVertex4sv = dllVertex4sv = glVertex4sv;
	qglVertexPointer = dllVertexPointer = glVertexPointer;
	qglViewport = dllViewport = glViewport;
	qwglCopyContext = wglCopyContext;
	qwglCreateContext = wglCreateContext;
	qwglCreateLayerContext = wglCreateLayerContext;
	qwglDeleteContext = wglDeleteContext;
	qwglDescribeLayerPlane = wglDescribeLayerPlane;
	qwglGetCurrentContext = wglGetCurrentContext;
	qwglGetCurrentDC = wglGetCurrentDC;
	qwglGetLayerPaletteEntries = wglGetLayerPaletteEntries;
	qwglGetProcAddress = wglGetProcAddress;
	qwglMakeCurrent = wglMakeCurrent;
	qwglRealizeLayerPalette = wglRealizeLayerPalette;
	qwglSetLayerPaletteEntries = wglSetLayerPaletteEntries;
	qwglShareLists = wglShareLists;
	qwglSwapLayerBuffers = wglSwapLayerBuffers;
	qwglUseFontBitmaps = wglUseFontBitmapsA;
	qwglUseFontOutlines = wglUseFontOutlinesA;
	qwglChoosePixelFormat = wglChoosePixelFormat;
	qwglDescribePixelFormat = wglDescribePixelFormat;
	qwglGetPixelFormat = wglGetPixelFormat;
	qwglSetPixelFormat = wglSetPixelFormat;
	qwglSwapBuffers = wglSwapBuffers;
	qwglSwapIntervalEXT = NULL;
	qglPointParameterfEXT = NULL;
	qglPointParameterfvEXT = NULL;
	qglColorTableEXT = NULL;
	qglSelectTextureSGIS = NULL;
	qglMTexCoord2fSGIS = NULL;
	return true;
}
