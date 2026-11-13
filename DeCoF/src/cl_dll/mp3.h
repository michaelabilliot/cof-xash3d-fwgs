// mp3 support added by Killar

#ifndef MP3_H
#define MP3_H

// Cry of Fear plays the music and the 2D sound effects with irrKlang (cl_dll/mp3.cpp)
// instead of FMOD. The class keeps the Paranoia interface (Initialize, Shutdown,
// PlayMP3, StopMP3, Frame); the object, the sound engine and the sounds are static
// globals of mp3.cpp.
class CMP3
{
public:
	CMP3();

	void	Disabled_guess( void );	// disabled, only the return is left
	void	Frame( void );
	int		Initialize( void );
	int		PlayMP3( const char *pszSong );
	int		PlayMP3Path_guess( const char *pszPath );
	void	PlaySFX_guess( const char *pszSample, int iType, bool bFlag, Vector vecOrigin );
	int		SetPaused_guess( bool bPaused );
	int		Shutdown( void );
	int		StopMP3( float fade );

	bool	m_bInitialized_guess;		// Initialize() was called
	cl_entity_t	*m_pLocalPlayer_guess;
	Vector	m_vecUnknown08_guess;		// only cleared by the constructor
	Vector	m_vecUnknown14_guess;
	Vector	m_vecUnknown20_guess;
	int		m_iUnknown2C_guess;			// only cleared by Initialize()
	int		m_iUnknown30_guess;
	bool	m_bUnknown34_guess;
	bool	m_bUnknown35_guess;

	// buz: mp3 file name:
	char	m_szMP3File[128];
	int		m_iIsPlaying;
	int		m_iIsPaused_guess;

	int		m_iVolume_guess;			// music volume (0-255) forced while m_bHold_guess is set
	bool	m_bHold_guess;				// never set: PlayMP3 only stores the song while it is set
	const char	*m_pszPending_guess;

	// buz: for fadeout:
	float	m_flFadeoutStart;
	float	m_flFadeoutDuration;
	float	m_flFadeinStart_guess;		// set by the PlayMP3 message, never read
	float	m_flFadeinDuration_guess;

	bool	m_bLoop_guess;
	bool	m_bStopping_guess;
};

void IrrKlangCreate_guess( void );
void IrrKlangDestroy_guess( void );
void IrrKlangFrame_guess( void );
void IrrKlangInitialize_guess( void );
bool IrrKlangIsPaused_guess( void );
void IrrKlangPlayMP3_guess( const char *pszFile );
void IrrKlangPlaySFX_guess( const char *pszFile, int iType, bool bFlag, vec3_t vecOrigin );
void IrrKlangSetFade_guess( float flParm1, float flParm2, float flParm3, float flParm4 );
void IrrKlangSetLoop_guess( bool bLoop );
void IrrKlangSetPaused_guess( bool bPaused );
void IrrKlangSetStopping_guess( bool bStopping );
void IrrKlangStop_guess( float flFadeTime );

// not in 1.6a: cdll_int.cpp still defines it and hud_redraw.cpp still uses it
#endif
