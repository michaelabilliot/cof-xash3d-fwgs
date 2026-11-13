// mss.h -- the parts of the Miles Sound System API that the engine uses (mss32.dll)

#ifndef MSS_H
#define MSS_H

#define AILCALL		__stdcall
#define DXDEC		__declspec( dllimport )

typedef long			S32;
typedef unsigned long	U32;
typedef char			C8;

typedef struct _DIG_DRIVER	*HDIGDRIVER;
typedef struct _STREAM		*HSTREAM;

#ifdef __cplusplus
extern "C"
{
#endif

DXDEC C8		*AILCALL AIL_set_redist_directory( C8 const *dir );
DXDEC S32		AILCALL AIL_startup( void );
DXDEC void		AILCALL AIL_shutdown( void );
DXDEC S32		AILCALL AIL_set_preference( U32 number, S32 value );

DXDEC HDIGDRIVER	AILCALL AIL_open_digital_driver( U32 frequency, S32 bits, S32 channel, U32 flags );
DXDEC void		AILCALL AIL_close_digital_driver( HDIGDRIVER dig );

DXDEC HSTREAM	AILCALL AIL_open_stream( HDIGDRIVER dig, C8 const *filename, S32 stream_mem );
DXDEC void		AILCALL AIL_close_stream( HSTREAM stream );
DXDEC void		AILCALL AIL_start_stream( HSTREAM stream );
DXDEC void		AILCALL AIL_pause_stream( HSTREAM stream, S32 onoff );
DXDEC void		AILCALL AIL_set_stream_volume( HSTREAM stream, S32 volume );
DXDEC void		AILCALL AIL_set_stream_loop_count( HSTREAM stream, S32 count );
DXDEC S32		AILCALL AIL_stream_playback_rate( HSTREAM stream );
DXDEC void		AILCALL AIL_set_stream_user_data( HSTREAM stream, U32 index, S32 value );

// static helper of mss32.lib: registers AIL_shutdown with atexit
S32				MSS_auto_cleanup_guess( void );

#ifdef __cplusplus
}
#endif

#define AIL_startup()	( MSS_auto_cleanup_guess(), AIL_startup() )

#endif // MSS_H
