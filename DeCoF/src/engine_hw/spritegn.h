// spritegn.h: header file for sprite generation program

#ifndef SPRITEGN_H
#define SPRITEGN_H

#define IDSPRITEHEADER	(('P'<<24)+('S'<<16)+('D'<<8)+'I')
														// little-endian "IDSP"
#define SPRITE_VERSION	2

typedef enum
{
	SPR_VP_PARALLEL_UPRIGHT,
	SPR_FACING_UPRIGHT,
	SPR_VP_PARALLEL,
	SPR_ORIENTED,
	SPR_VP_PARALLEL_ORIENTED,
	SPR_LABEL,
	SPR_LABEL_SCALE
} sprtype_t;

typedef enum
{
	SPR_NORMAL,
	SPR_ADDITIVE,
	SPR_INDEXALPHA,
	SPR_ALPHTEST
} sprtexformat_t;

typedef struct {
	int			ident;
	int			version;
	int			type;
	int			texFormat;
	float		boundingradius;
	int			width;
	int			height;
	int			numframes;
	float		beamlength;
	synctype_t	synctype;
} dsprite_t;

typedef struct {
	int			origin[2];
	int			width;
	int			height;
} dspriteframe_t;

typedef struct {
	int			numframes;
} dspritegroup_t;

typedef struct {
	float	interval;
} dspriteinterval_t;

typedef enum { SPR_SINGLE=0, SPR_GROUP } spriteframetype_t;

typedef struct {
	spriteframetype_t	type;
} dspriteframetype_t;

#endif // SPRITEGN_H
