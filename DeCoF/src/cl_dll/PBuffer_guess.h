// Cry of Fear pixel buffer (WGL_ARB_pbuffer) that renders into a texture.
// Only the constructor and the destructor are called.

#ifndef GUESS_PBUFFER_H
#define GUESS_PBUFFER_H

DECLARE_HANDLE( HPBUFFERARB );

class CPBuffer_guess
{
public:
	CPBuffer_guess();
	~CPBuffer_guess();

	void BindTexture_guess( void );
	void Create_guess( void );
	void MakeCurrent_guess( void );
	void Prepare_guess( void );
	void ReleaseTexture_guess( void );
	void Shutdown_guess( void );
	void SwitchToWindow_guess( void );

	bool	m_bValid_guess;
	int		m_iHeight_guess;
	int		m_iWidth_guess;
	GLuint	m_iTexture_guess;
	HDC		m_hWindowDC_guess;
	HDC		m_hDC_guess;
	HGLRC	m_hRC_guess;
	HPBUFFERARB m_hPBuffer_guess;
	HGLRC	m_hWindowRC_guess;
};

extern CPBuffer_guess g_PBuffer_guess;

#endif // GUESS_PBUFFER_H
