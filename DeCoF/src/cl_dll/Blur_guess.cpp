// Cry of Fear motion blur through a rectangle texture
// (no build records the name of this file; its position in the link
// order is between battery.cpp and cdll_int.cpp. Nothing calls this code
// in 1.6a: CHud::DrawBlurTest in hud_redraw.cpp draws the blur instead)

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "glmanager.h"

extern cvar_t *cv_blurtest;

class CBlurTexture_guess
{
public:
	CBlurTexture_guess();

	void Init_guess( int width, int height );
	void BindTexture_guess( int width, int height );
	void DrawQuad_guess( int width, int height, int of );
	void DrawBlended_guess( int width, int height );

	unsigned int	m_uiTexture_guess;
	float			m_flAlpha_guess;
	float			m_flRed_guess;
	float			m_flGreen_guess;
	float			m_flBlue_guess;
};

class CBlur_guess
{
public:
	void Initialize_guess( void );
	void DrawBlur_guess( void );
	bool AnimateNextFrame_guess( int desiredFrameRate );

	int					m_iActive_guess;
	CBlurTexture_guess	m_Output_guess;
};

CBlurTexture_guess::CBlurTexture_guess()
{
}

void CBlurTexture_guess::Init_guess( int width, int height )
{
	// a load of blank pixels to create the texture with
	unsigned char *pBlankTex = new unsigned char[width * height * 3];
	memset( pBlankTex, 0, width * height * 3 );

	glGenTextures( 1, &m_uiTexture_guess );
	glBindTexture( GL_TEXTURE_RECTANGLE_NV, m_uiTexture_guess );
	glTexParameteri( GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexImage2D( GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA8, width, height, 0, GL_RGBA8, GL_UNSIGNED_BYTE, NULL );

	delete[] pBlankTex;
}

void CBlurTexture_guess::BindTexture_guess( int width, int height )
{
	glBindTexture( GL_TEXTURE_RECTANGLE_NV, m_uiTexture_guess );
	glCopyTexImage2D( GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA8, 0, 0, ScreenWidth, ScreenHeight, 0 );
}

void CBlurTexture_guess::DrawQuad_guess( int width, int height, int of )
{
	glTexCoord2f( -of, -of );
	glVertex3f( 0, 1, -1 );
	glTexCoord2f( -of, height + of );
	glVertex3f( 0, 0, -1 );
	glTexCoord2f( width + of, height + of );
	glVertex3f( 1, 0, -1 );
	glTexCoord2f( width + of, -of );
	glVertex3f( 1, 1, -1 );
}

void CBlurTexture_guess::DrawBlended_guess( int width, int height )
{
	glEnable( GL_TEXTURE_RECTANGLE_NV );
	glColor3f( 1, 1, 1 );
	glDisable( GL_DEPTH_TEST );

	glMatrixMode( GL_MODELVIEW );
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode( GL_PROJECTION );
	glPushMatrix();
	glLoadIdentity();
	glOrtho( 0, 1, 1, 0, 0.1, 100 );

	glBindTexture( GL_TEXTURE_RECTANGLE_NV, m_uiTexture_guess );
	glColor4f( m_flRed_guess, m_flGreen_guess, m_flBlue_guess, m_flAlpha_guess );

	if ( cv_blurtest->value )
	{
		glBegin( GL_QUADS );
		DrawQuad_guess( ScreenWidth, ScreenHeight, 0 );
		glEnd();
	}

	glMatrixMode( GL_PROJECTION );
	glPopMatrix();
	glMatrixMode( GL_MODELVIEW );
	glPopMatrix();

	glDisable( GL_TEXTURE_RECTANGLE_NV );
	glEnable( GL_DEPTH_TEST );
}

void CBlur_guess::Initialize_guess( void )
{
	m_iActive_guess = 1;
	m_Output_guess.Init_guess( 320, 240 );
	m_Output_guess.m_flRed_guess = 1.0;
	m_Output_guess.m_flGreen_guess = 1.0;
	m_Output_guess.m_flBlue_guess = 1.0;
}

void CBlur_guess::DrawBlur_guess( void )
{
	if ( cv_blurtest->value )
	{
		glBlendFunc( GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA );
		glEnable( GL_BLEND );

		m_Output_guess.m_flAlpha_guess = 0.4;
		m_Output_guess.m_flRed_guess = 1.0;
		m_Output_guess.m_flGreen_guess = 1.0;
		m_Output_guess.m_flBlue_guess = 1.0;
		m_Output_guess.DrawBlended_guess( ScreenWidth, ScreenHeight );
		m_Output_guess.BindTexture_guess( ScreenWidth, ScreenHeight );

		glDisable( GL_BLEND );
	}
}

bool CBlur_guess::AnimateNextFrame_guess( int desiredFrameRate )
{
	static float lastTime = 0.0f;
	float elapsedTime = 0.0;

	float currentTime = GetTickCount() * 0.001f;
	elapsedTime = currentTime - lastTime;

	if ( elapsedTime > ( 1.0f / desiredFrameRate ) )
	{
		lastTime = currentTime;
		return true;
	}

	return false;
}
