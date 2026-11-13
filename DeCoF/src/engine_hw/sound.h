// sound.h -- client sound i/o functions (snd_dma.c, snd_mix.c)

#ifndef SOUND_H
#define SOUND_H

// needs mathlib.h (vec3_t), zone.h (cache_user_t) and cvar.h (cvar_t)

#define PAINTBUFFER_SIZE	512

typedef struct
{
	int left;
	int right;
} portable_samplepair_t;

typedef struct sfx_s
{
	char			name[64];
	cache_user_t	cache;
	int				servercount;
} sfx_t;

// !!! if this is changed, it much be changed in asm_i386.h too !!!
typedef struct sfxcache_s
{
	int 	length;
	int 	loopstart;
	int 	speed;
	int 	width;
	int 	stereo;
	byte	data[1];		// variable sized
} sfxcache_t;

typedef struct
{
	qboolean		gamealive;
	qboolean		soundalive;
	qboolean		splitbuffer;
	int				channels;
	int				samples;			// mono samples in buffer
	int				submission_chunk;	// don't mix less than this #
	int				samplepos;			// in mono samples
	int				samplebits;
	int				speed;
	int				dmaspeed;
	unsigned char	*buffer;
} dma_t;

// !!! if this is changed, it much be changed in asm_i386.h too !!!
typedef struct
{
	sfx_t	*sfx;			// sfx number
	int		leftvol;		// 0-255 volume
	int		rightvol;		// 0-255 volume
	int		end;			// end time in global paintsamples
	int 	pos;			// sample position in sfx
	int		looping;		// where to loop, -1 = no looping
	int		entnum;			// to allow overriding a specific sound
	int		entchannel;		//
	vec3_t	origin;			// origin of sound effect
	vec_t	dist_mult;		// distance multiplier (attenuation/clipK)
	int		master_vol;		// 0-255 master volume
	int		isentence;		// true if playing linked sentence
	int		iword;
	int		pitch;
	int		rleftvol_guess;		// 0-255 volume of the rear speakers (four channel output)
	int		rrightvol_guess;
} channel_t;

typedef struct
{
	int		rate;
	int		width;
	int		channels;
	int		loopstart;
	int		samples;
	int		dataofs;		// chunk starts this many bytes from file start
} wavinfo_t;

typedef struct wavstream_s
{
	int			csamplesplayed;
	int			csamplesinmem;
	FileHandle_t	hFile;
	wavinfo_t	info;
	int			lastposloaded;
} wavstream_t;

// sound flags of svc_sound
#define SND_VOLUME			(1<<0)		// a byte
#define SND_ATTENUATION		(1<<1)		// a byte
#define SND_LARGE_INDEX		(1<<2)		// a send sound as short
#define SND_PITCH			(1<<3)		// a byte
#define SND_SENTENCE		(1<<4)		// set if sound num is actually a sentence num
#define SND_STOP			(1<<5)		// stop the sound
#define SND_CHANGE_VOL		(1<<6)		// change sound vol
#define SND_CHANGE_PITCH	(1<<7)		// change sound pitch
#define SND_SPAWNING		(1<<8)		// we're spawing, used in some cases for ambients

#define DEFAULT_SOUND_PACKET_VOLUME			255
#define DEFAULT_SOUND_PACKET_ATTENUATION	1.0
#define DEFAULT_SOUND_PACKET_PITCH			100

#define CVOXFILESENTENCEMAX	1536		// max number of sentences in game. NOTE: this must match CVOXFILESENTENCEMAX in dlls\util.h!!!

void	S_StopAllSounds( qboolean clear );
void	S_StopSound( int entnum, int entchannel );
void	S_StartDynamicSound( int entnum, int entchannel, sfx_t *sfx, vec_t *origin, float fvol, float attenuation, int flags, int pitch );
void	S_StartStaticSound( int entnum, int entchannel, sfx_t *sfxin, vec_t *origin, float fvol, float attenuation, int flags, int pitch );

void	S_BeginPrecaching( void );
void	S_EndPrecaching( void );
struct sfx_s	*S_PrecacheSound( char *name );

// ====================================================================
// User-setable variables
// ====================================================================

#define	MAX_CHANNELS			128
#define	MAX_DYNAMIC_CHANNELS	8

extern	channel_t	channels[MAX_CHANNELS];
// 0 to MAX_DYNAMIC_CHANNELS-1	= normal entity sounds
// MAX_DYNAMIC_CHANNELS to MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS -1 = water, etc
// MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS to total_channels = static sounds

extern	int			total_channels;

extern	int			paintedtime;
extern	vec3_t		listener_origin;
extern	vec3_t		listener_forward;
extern	vec3_t		listener_right;
extern	vec3_t		listener_up;
extern	volatile dma_t	*shm;
extern	volatile dma_t	sn;

extern	cvar_t		loadas8bit;
extern	cvar_t		bgmvolume;
extern	cvar_t		volume;
extern	cvar_t		hisound;

extern	qboolean	snd_initialized;

extern	int			snd_blocked;

#define CHAR_STREAM		'*'		// as one of 1st 2 chars in name, indicates streaming wav data
#define CHAR_SENTENCE	'!'		// as one of 1st 2 chars in name, indicates sentence wav

extern	int			soundtime;
extern	vec_t		sound_nominal_clip_dist;
extern	cvar_t		nosound;

extern	cvar_t		ambient_level;
extern	cvar_t		ambient_fade;
extern	cvar_t		snd_noextraupdate;
extern	cvar_t		snd_show;
extern	cvar_t		_snd_mixahead;
extern	cvar_t		s_distance;
extern	cvar_t		s_verbwet;

extern	qboolean	fakedma;
extern	int			sound_started;

// snd_dma.c
void	S_Init( void );
void	S_Startup( void );
void	S_Shutdown( void );
void	S_Update( vec_t *origin, vec_t *forward, vec_t *right, vec_t *up );
void	S_Update_( void );
void	S_ExtraUpdate( void );
void	S_ClearBuffer( void );
sfx_t	*S_FindName( char *name, int *pfInCache );
void	SND_Spatialize( channel_t *ch );

// snd_mem.c
void	ResampleSfx( sfx_t *sfx, int inrate, int inwidth, byte *data );
sfxcache_t	*S_LoadSound( sfx_t *s, channel_t *ch );
sfxcache_t	*S_LoadStreamSound( sfx_t *s, channel_t *ch );
wavinfo_t	GetWavinfo( char *name, byte *wav, int wavlength );

#define CWAVSTREAMS		MAX_CHANNELS

extern	wavstream_t	wavstreams[CWAVSTREAMS];

#define CVOXWORDMAX		32		// max number of words in a sentence

typedef struct voxword
{
	int		volume;			// increase percent, ie: 125 = 125% increase
	int		pitch;			// pitch shift up percent
	int		start;			// offset start of wave percent
	int		end;			// offset end of wave percent
	int		cbtrim;			// end of wave after being trimmed to 'end'
	int		fKeepCached;	// 1 if this word was already in cache before sentence referenced it
	int		samplefrac;		// if pitch shifting, this is position into wav * 256
	int		timecompress;	// % of wave to skip during playback (causes no pitch shift)
	sfx_t	*sfx;			// name and cache pointer
} voxword_t;

extern	char		*rgpparseword[CVOXWORDMAX];
extern	voxword_t	rgrgvoxword[16][CVOXWORDMAX];

// snd_mix.c: plays a channel on its A3D hardware voice, true when A3D took it (0x01DD0E91)
int		hwPaintChannel_guess( int ch_idx, channel_t *ch, sfxcache_t *sc, int count, int unused, float pitch );
// snd_mix.c: restarts the hardware voice of a stream channel that ended (0x01DD15D3)
void	hwStopChannel_guess( int ch_idx );

// snd_mixa.s
void	Snd_WriteLinearBlastStereo16( void );
// the 8 bit mixers of snd_mixa.s: into paintbuffer (0x01E13578) and into the A3D reverb buffer (0x01E136DC)
void	SND_PaintChannelFrom8Asm_guess( channel_t *ch, sfxcache_t *sc, int count );
void	SND_PaintChannelFrom8A3D_guess( channel_t *ch, sfxcache_t *sc, int count );

// snd_mix.c
void	SetCareerAudioState( int paused );
void	CareerAudio_Command_f( void );
void	S_TransferStereo16( int end );
void	S_TransferPaintBuffer( int endtime );
void	S_FreeChannel( channel_t *ch );
void	S_PaintChannels( int endtime );
void	SND_InitScaletable( void );
void	SX_Init( void );
void	SX_RoomFX( int count, int fFilter, int fTimefx );
qboolean	Wavstream_Init( void );
void	Wavstream_Close( int i );
void	SND_InitMouth( int entnum, int entchannel );
void	SND_CloseMouth( channel_t *ch );
void	VOX_Init( void );
void	VOX_Shutdown( void );
void	VOX_SetChanVol( channel_t *ch );
void	VOX_MakeSingleWordSentence( channel_t *ch, int pitch );
sfxcache_t	*VOX_LoadSound( channel_t *pchan, char *pszin );
void	VOX_TrimStartEndTimes( channel_t *ch, sfxcache_t *sc );
char	*VOX_LookupString( char *pszin, int *psentencenum );
void	VOX_ReadSentenceFile( void );

extern	portable_samplepair_t	paintbuffer[];
extern	int		cszrawsentences;

extern	char	*rgpszrawsentence[CVOXFILESENTENCEMAX];

// voice_sound_engine_interface.cpp
sfxcache_t	*VoiceSE_GetSFXCache( sfx_t *pSFX );
void	VoiceSE_NotifyFreeChannel( int iChannel );

// sequence.cpp
void	Sequence_Init( void );
void	Sequence_OnLevelLoad( const char *mapName );

#endif // SOUND_H
