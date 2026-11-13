// IDemoPlayer.h -- interface of the HLTV demo player module (demoplayer.dll)

#ifndef IDEMOPLAYER_H
#define IDEMOPLAYER_H
#pragma once

struct ref_params_s;
typedef struct ref_params_s ref_params_t;

class IWorld;
class DirectorCmd;

class IDemoPlayer
{
public:
	virtual void			SetWorldTime( double time, bool relative ) = 0;
	virtual void			SetTimeScale( float scale ) = 0;
	virtual void			SetPaused( bool state ) = 0;
	virtual void			SetEditMode( bool state ) = 0;
	virtual void			SetMasterMode( bool state ) = 0;
	virtual bool			IsPaused( void ) = 0;
	virtual bool			IsLoading( void ) = 0;
	virtual bool			IsActive( void ) = 0;
	virtual bool			IsEditMode( void ) = 0;
	virtual bool			IsMasterMode( void ) = 0;
	virtual void			RemoveFrames( double starttime, double endtime ) = 0;
	virtual void			ExecuteDirectorCmd( DirectorCmd *cmd ) = 0;
	virtual double			GetWorldTime( void ) = 0;
	virtual double			GetStartTime( void ) = 0;
	virtual double			GetEndTime( void ) = 0;
	virtual float			GetTimeScale( void ) = 0;
	virtual IWorld			*GetWorld( void ) = 0;
	virtual char			*GetFileName( void ) = 0;
	virtual bool			SaveGame( char *filename ) = 0;
	virtual bool			LoadGame( char *filename ) = 0;
	virtual void			Stop( void ) = 0;
	virtual void			ForceHLTV( bool state ) = 0;
	virtual void			GetDemoViewInfo( ref_params_t *rp, float *view, int *viewmodel ) = 0;
	virtual int				ReadDemoMessage( unsigned char *buffer, int size ) = 0;
	virtual void			ReadNetchanState( int *incoming_sequence, int *incoming_acknowledged, int *incoming_reliable_acknowledged, int *incoming_reliable_sequence, int *outgoing_sequence, int *reliable_sequence, int *last_reliable_sequence ) = 0;
};

#define DEMOPLAYER_INTERFACE_VERSION	"demoplayer001"

#endif // IDEMOPLAYER_H
