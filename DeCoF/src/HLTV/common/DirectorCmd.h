//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: HLTV director command, stored as a type and a bit buffer
//
// $NoKeywords: $
//=============================================================================

#ifndef DIRECTORCMD_H
#define DIRECTORCMD_H

#ifdef _WIN32
#pragma once
#endif

#include "types.h"
#include "Bitbuffer.h"

extern const char *DRC_CMD_NAME[];

class DirectorCmd
{
public:
	DirectorCmd();
	virtual ~DirectorCmd();

	void	Clear();
	void	Copy( DirectorCmd *cmd );
	void	Resize( int size );
	void	FromString( char *string );
	char	*ToString();
	void	WriteToStream( BitBuffer *stream );
	bool	ReadFromStream( BitBuffer *stream );

	int		GetType();
	char	*GetName();
	float	GetTime();

	void	SetTime( float time );
	void	SetStartData();
	void	SetEventData( int entity1, int entity2, int flags );
	void	SetChaseData( int entity1, int entity2, float distance, int flags );
	void	SetModeData( int mode );
	void	SetInEyeData( int entity );
	void	SetMapData( int entity, float angle, float distance );
	void	SetCameraData( vec_t *position, vec_t *angles, float fov, int entity );
	void	SetCamPathData( vec_t *position, vec_t *angles, float fov, int flags );
	void	SetTimeScaleData( float factor );
	void	SetMessageData( int effect, unsigned int color, vec_t *position, float fadein, float fadeout, float holdtime, float fxtime, char *text );
	void	SetSoundData( char *name, float volume );
	void	SetStatusData( int slots, int spectators, int proxies );
	void	SetBannerData( char *filename );
	void	SetStuffTextData( char *commands );
	void	SetWayPoints( int number );

	bool	GetEventData( int &entity1, int &entity2, int &flags );
	bool	GetChaseData( int &entity1, int &entity2, float &distance, int &flags );
	bool	GetInEyeData( int &player );
	bool	GetMapData( int &entity, float &angle, float &distance );
	bool	GetCameraData( vec_t *position, vec_t *angles, float &fov, int &entity );
	bool	GetCamPathData( vec_t *position, vec_t *angles, float &fov, int &flags );
	bool	GetTimeScaleData( float &factor );
	bool	GetMessageData( int &effect, int &color, vec_t *position, float &fadein, float &fadeout, float &holdtime, float &fxtime, char *text );
	bool	GetSoundData( char *name, float &volume );
	bool	GetModeData( int &mode );
	bool	GetStatusData( int &slots, int &spectators, int &proxies );
	bool	GetBannerData( char *filename );
	bool	GetStuffTextData( char *commands );
	bool	GetWayPointsData( int &number );

	float		m_Time;
	int			m_Type;
	int			m_Size;
	BitBuffer	m_Data;
	int			m_Index;
};

#endif // DIRECTORCMD_H
