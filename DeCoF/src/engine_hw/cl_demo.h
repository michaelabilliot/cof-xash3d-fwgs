// cl_demo.h -- demo file format (cl_demo.c)

#ifndef CL_DEMO_H
#define CL_DEMO_H

#define DEMO_PROTOCOL		5

typedef struct demoheader_s
{
	char		szFileStamp[6];
	int			nDemoProtocol;			// should be DEMO_PROTOCOL
	int			nNetProtocolVersion;	// should be PROTOCOL_VERSION
	char		szMapName[MAX_PATH];	// Name of map
	char		szDllDir[MAX_PATH];		// Name of game directory (com_gamedir)
	CRC32_t		mapCRC;
	int			nDirectoryOffset;		// Offset of Entry Directory.
} demoheader_t;

typedef struct demoentry_s
{
	int			nEntryType;				// 0 = startup, 1 = normal
	char		szDescription[64];
	int			nFlags;
	int			nCDTrack;
	float		fTrackTime;				// time of track
	int			nFrames;				// # of frames in track
	int			nOffset;				// File offset of track data
	int			nFileLength;			// length of track
} demoentry_t;

typedef struct demo_info_s
{
	float			timestamp;
	ref_params_t	rp;
	usercmd_t		cmd;
	movevars_t		movevars;
	vec3_t			view;
	int				viewmodel;
} demo_info_t;

typedef struct demodirectory_s
{
	int			nEntries;				// Number of tracks
	demoentry_t	*p_rgEntries;			// Track entry info
} demodirectory_t;

extern	demoentry_t		*pCurrentEntry;
extern	int				nCurEntryIndex;
extern	demodirectory_t	demodir;
extern	demoheader_t	demoheader;

void	CL_SetDemoViewInfo( ref_params_t *rp, vec_t *view, int viewmodel );
void	CL_GetDemoViewInfo( ref_params_t *rp, float *view, int *viewmodel );

#endif // CL_DEMO_H
