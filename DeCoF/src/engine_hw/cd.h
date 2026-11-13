// cd.h -- interface of the CD and MP3 music player

#ifndef CD_H
#define CD_H

class ICDAudio
{
public:
	virtual int		Init( void ) = 0;
	virtual void	Shutdown( void ) = 0;
	virtual void	Play( int track, bool looping ) = 0;
	virtual void	PlayFile( const char *filename, bool looping ) = 0;
	virtual void	Pause( void ) = 0;
	virtual void	Resume( void ) = 0;
	virtual void	Frame( void ) = 0;
	virtual void	MP3_Resume_Audio( void ) = 0;
	virtual void	MP3_Suspend_Audio( void ) = 0;
};

extern ICDAudio *cdaudio;

#endif // CD_H
