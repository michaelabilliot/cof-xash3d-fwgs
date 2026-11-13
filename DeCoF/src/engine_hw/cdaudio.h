// cdaudio.h -- the C entry points of the CD and MP3 music player (cd.cpp)

#ifndef CDAUDIO_H
#define CDAUDIO_H

void	CD_Command_f( void );
void	MP3_Command_f( void );
void	PrimeMusicStream( char *filename, int looping );

void	CDAudio_Init( void );
void	CDAudio_Shutdown( void );
void	CDAudio_Play( int track, int looping );
void	CDAudio_Pause( void );
void	CDAudio_Resume( void );

void	MP3_Resume_Audio( void );
void	MP3_Suspend_Audio( void );

#endif // CDAUDIO_H
