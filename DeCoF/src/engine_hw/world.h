// world.h -- the server's collision world (world.c)

#ifndef WORLD_H
#define WORLD_H

typedef struct areanode_s
{
	int		axis;		// -1 = leaf node
	float	dist;
	struct areanode_s	*children[2];
	link_t	trigger_edicts;
	link_t	solid_edicts;
} areanode_t;

#define	AREA_DEPTH	4
#define	AREA_NODES	32

extern	areanode_t	sv_areanodes[AREA_NODES];
extern	int			sv_numareanodes;

#define	MOVE_NORMAL		0
#define	MOVE_NOMONSTERS	1
#define	MOVE_MISSILE	2

typedef struct
{
	vec3_t		boxmins, boxmaxs;	// enclose the test object along entire move
	const float	*mins, *maxs;		// size of the moving object
	vec3_t		mins2, maxs2;		// size when clipping against mosnters
	const float	*start, *end;
	trace_t		trace;
	short		type;
	short		ignoretrans;
	edict_t		*passedict;
	qboolean	monsterClipBrush;
} moveclip_t;

void		ClearLink( link_t *l );
void		RemoveLink( link_t *l );
void		InsertLinkBefore( link_t *l, link_t *before );
void		InsertLinkAfter( link_t *l, link_t *after );

void		SV_InitBoxHull( void );
hull_t		*SV_HullForBox( const vec_t *mins, const vec_t *maxs );
hull_t		*SV_HullForBeam( const vec_t *start, const vec_t *end, const vec_t *size );
struct hull_s	*SV_HullForBsp( edict_t *ent, const vec_t *mins, const vec_t *maxs, vec_t *offset );
hull_t		*SV_HullForEntity( edict_t *ent, const vec_t *mins, const vec_t *maxs, vec_t *offset );
areanode_t	*SV_CreateAreaNode( int depth, vec_t *mins, vec_t *maxs );

void		SV_ClearWorld( void );
// called after the world model has been loaded, before linking any entities

void		SV_UnlinkEdict( edict_t *ent );
// call before removing an entity, and before trying to move one,
// so it doesn't clip against itself
// flags ent->v.modified

void		SV_TouchLinks( edict_t *ent, areanode_t *node );
void		SV_FindTouchedLeafs( edict_t *ent, mnode_t *node, int *topnode );

void		SV_LinkEdict( edict_t *ent, qboolean touch_triggers );
// Needs to be called any time an entity changes origin, mins, maxs, or solid
// flags ent->v.modified
// sets ent->v.absmin and ent->v.absmax
// if touchtriggers, calls prog functions for the intersected triggers

int			SV_LinkContents( areanode_t *node, const vec_t *pos );
int			SV_PointContents( const vec_t *p );
// returns the CONTENTS_* value from the world at the given point.
// does not check any entities at all
// the non-true version remaps the water current contents to content_water

edict_t		*SV_TestEntityPosition( edict_t *ent );

int			SV_HullPointContents( hull_t *hull, int num, const vec_t *p );

qboolean	SV_RecursiveHullCheck( hull_t *hull, int num, float p1f, float p2f, vec_t *p1, vec_t *p2, trace_t *trace );

void		SV_SingleClipMoveToEntity( edict_t *ent, const vec_t *start, const vec_t *mins, const vec_t *maxs, const vec_t *end, trace_t *trace );
trace_t		SV_ClipMoveToEntity( edict_t *ent, const vec_t *start, const vec_t *mins, const vec_t *maxs, const vec_t *end );
void		SV_ClipToLinks( areanode_t *node, moveclip_t *clip );
void		SV_ClipToWorldbrush( areanode_t *node, moveclip_t *clip );
void		SV_MoveBounds( const vec_t *start, const vec_t *mins, const vec_t *maxs, const vec_t *end, vec_t *boxmins, vec_t *boxmaxs );
trace_t		SV_MoveNoEnts( const vec_t *start, vec_t *mins, vec_t *maxs, const vec_t *end, int type, edict_t *passedict );
trace_t		SV_Move( const vec_t *start, const vec_t *mins, const vec_t *maxs, const vec_t *end, int type, edict_t *passedict, qboolean monsterClipBrush );
// mins and maxs are reletive

// if the entire move stays in a solid volume, trace.allsolid will be set

// if the starting point is in a solid, it will be allowed to move out
// to an open area

// nomonsters is used for line of sight or edge testing, where mosnters
// shouldn't be considered solid objects

// passedict is explicitly excluded from clipping checks (normally NULL)

// pr_cmds.c
void		SV_SetGlobalTrace( trace_t *ptrace );

#endif // WORLD_H
