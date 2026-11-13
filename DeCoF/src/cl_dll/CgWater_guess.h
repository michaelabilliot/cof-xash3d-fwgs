// Cry of Fear Cg water: brush entities with the water contents are drawn
// with a refraction shader over a copy of the screen.

#ifndef GUESS_CGWATER_H
#define GUESS_CGWATER_H

#include "CgContext_guess.h"

class CCgWater_guess
{
public:
	CCgWater_guess();

	void DrawBlackQuad_guess( void );
	void EndShaders_guess( void );
	void Init_guess( void );
	void PostDraw_guess( void );
	void PreDraw_guess( void );
	void Render_guess( void );
	void ScreenQuad_guess( void );
	void SetBlackShader_guess( void );
	void UpdateEntities_guess( void );

	PFNGLACTIVETEXTUREARBPROC	glActiveTextureARB;
	PFNGLMULTITEXCOORD2FARBPROC	glMultiTexCoord2fARB;
	bool	m_bAllowTextureShader_guess;
	bool	m_bEnabled_guess;
	bool	m_bDrawn_guess;
	float	m_flTexWidth_guess;
	float	m_flTexHeight_guess;
	GLuint	m_iMaskTexture_guess;
	GLuint	m_iDistortTexture_guess;
	GLuint	m_iNoiseTexture_guess;
	GLuint	m_iSceneTexture_guess;
	GLuint	m_iOffsetTexture_guess;
	cgprogram_t_guess *m_pWaterFP_guess;
	cgprogram_t_guess *m_pWaterVP_guess;
	cgprogram_t_guess *m_pBlackFP_guess;
	cgprogram_t_guess *m_pBlackVP_guess;
	CGparameter	m_pBlackModelViewProj_guess;
	CGparameter	m_pModelViewProj_guess;
	CGparameter	m_pTexOffset_guess;
	CGparameter	m_pScreen_guess;
	GLint	m_iSavedTexture1_guess;
	GLint	m_iSavedTexture2_guess;
};

extern CCgWater_guess g_CgWater_guess;

#endif // GUESS_CGWATER_H
