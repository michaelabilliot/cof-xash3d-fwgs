// pmove.h -- the engine side of the shared player movement code (pmove.c,
// pmovetst.c)

#ifndef PMOVE_H
#define PMOVE_H

#include "../pm_shared/pm_defs.h"

extern	vec_t			player_mins[4][3];
extern	vec_t			player_maxs[4][3];

extern	playermove_t	*pmove;
extern	playermove_t	g_clmove;
extern	playermove_t	g_svmove;

//
// pmovetst.c
//
float		PM_TraceModel( physent_t *pEnt, vec_t *start, vec_t *end, trace_t *trace );
void		PM_GetModelBounds( struct model_s *mod, vec_t *mins, vec_t *maxs );
int			PM_GetModelType( struct model_s *mod );
void		PM_Init( playermove_t *ppm );
void		PM_InitBoxHull( void );
hull_t		*PM_HullForBox( vec_t *mins, vec_t *maxs );
int			PM_HullPointContents( hull_t *hull, int num, vec_t *p );
int			PM_LinkContents( vec_t *p, int *pIndex );
int			PM_PointContents( vec_t *p, int *truecontents );
int			PM_WaterEntity( vec_t *p );
int			PM_TruePointContents( vec_t *p );
hull_t		*PM_HullForStudioModel( model_t *pModel, vec_t *offset, float frame, int sequence, const vec_t *angles, const vec_t *origin, const byte *pcontroller, const byte *pblending, int *pNumHulls );
hull_t		*PM_HullForBsp( physent_t *pe, vec_t *offset );
int			_PM_TestPlayerPosition( vec_t *pos, pmtrace_t *ptrace, int ( *pfnIgnore )( physent_t *pe ) );
int			PM_TestPlayerPosition( vec_t *pos, pmtrace_t *ptrace );
int			PM_TestPlayerPositionEx( vec_t *pos, pmtrace_t *ptrace, int ( *pfnIgnore )( physent_t *pe ) );
pmtrace_t	_PM_PlayerTrace( vec_t *start, vec_t *end, int traceFlags, int numphysent, physent_t *physents, int ignore_pe, int ( *pfnIgnore )( physent_t *pe ) );
pmtrace_t	PM_PlayerTrace( vec_t *start, vec_t *end, int traceFlags, int ignore_pe );
pmtrace_t	PM_PlayerTraceEx( vec_t *start, vec_t *end, int traceFlags, int ( *pfnIgnore )( physent_t *pe ) );
struct pmtrace_s	*PM_TraceLine( float *start, float *end, int flags, int usehull, int ignore_pe );
struct pmtrace_s	*PM_TraceLineEx( float *start, float *end, int flags, int usehull, int ( *pfnIgnore )( physent_t *pe ) );
qboolean	PM_RecursiveHullCheck( hull_t *hull, int num, float p1f, float p2f, vec_t *p1, vec_t *p2, pmtrace_t *trace );

#endif // PMOVE_H
