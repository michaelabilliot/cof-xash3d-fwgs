// view.h -- player eye positioning and the gamma tables

#ifndef VIEW_H
#define VIEW_H

extern	cvar_t		v_gamma;
extern	cvar_t		v_lightgamma;
extern	cvar_t		v_brightness;

extern	byte		texgammatable[256];	// palette is sent through this to convert to screen gamma
extern	int			lightgammatable[1024];
extern	int			lineargammatable[1024];
extern	int			screengammatable[1024];

extern	float		v_blend[4];
extern	float		v_lambert1;

extern	vec3_t		r_soundOrigin;
extern	vec3_t		r_playerViewportAngles;

void	V_Init( void );
void	V_InitLevel( void );
void	V_RenderView( void );
void	V_UpdatePalette( void );
void	V_CalcShake( void );
void	V_ApplyShake( float *origin, float *angles, float factor );
int		V_ScreenShake( const char *pszName, int iSize, void *pbuf );
int		V_ScreenFade( const char *pszName, int iSize, void *pbuf );
int		V_FadeAlpha( void );

#endif // VIEW_H
