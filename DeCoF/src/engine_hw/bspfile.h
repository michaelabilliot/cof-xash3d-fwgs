// bspfile.h -- the on-disk format of a compiled map

#ifndef BSPFILE_H
#define BSPFILE_H

#define Q1BSP_VERSION		29		// quake1 regular version (beta is 28)
#define HLBSP_VERSION		30		// half-life regular version

#define MAX_MAP_LEAFS		8192

#define LUMP_ENTITIES		0
#define LUMP_PLANES			1
#define LUMP_TEXTURES		2
#define LUMP_VERTEXES		3
#define LUMP_VISIBILITY		4
#define LUMP_NODES			5
#define LUMP_TEXINFO		6
#define LUMP_FACES			7
#define LUMP_LIGHTING		8
#define LUMP_CLIPNODES		9
#define LUMP_LEAFS			10
#define LUMP_MARKSURFACES	11
#define LUMP_EDGES			12
#define LUMP_SURFEDGES		13
#define LUMP_MODELS			14

#define HEADER_LUMPS		15

typedef struct lump_s
{
	int		fileofs;
	int		filelen;
} lump_t;

typedef struct dheader_s
{
	int		version;
	lump_t	lumps[HEADER_LUMPS];
} dheader_t;

#define	MAX_MAP_HULLS		4

#define	MAXLIGHTMAPS		4

#define	NUM_AMBIENTS		4		// automatic ambient sounds

// the contents that const.h leaves to this file
#define	CONTENTS_ORIGIN		-7		// removed at csg time
#define	CONTENTS_CLIP		-8		// changed to contents_solid
#define	CONTENTS_CURRENT_0		-9
#define	CONTENTS_CURRENT_90		-10
#define	CONTENTS_CURRENT_180	-11
#define	CONTENTS_CURRENT_270	-12
#define	CONTENTS_CURRENT_UP		-13
#define	CONTENTS_CURRENT_DOWN	-14

#define CONTENTS_TRANSLUCENT	-15

typedef struct
{
	float		mins[3], maxs[3];
	float		origin[3];
	int			headnode[MAX_MAP_HULLS];
	int			visleafs;		// not including the solid leaf 0
	int			firstface, numfaces;
} dmodel_t;

typedef struct
{
	int			planenum;
	short		children[2];	// negative numbers are contents
} dclipnode_t;

#define	MIPLEVELS	4
typedef struct miptex_s
{
	char		name[16];
	unsigned	width, height;
	unsigned	offsets[MIPLEVELS];		// four mip maps stored
} miptex_t;

typedef struct
{
	int			nummiptex;
	int			dataofs[4];		// [nummiptex]
} dmiptexlump_t;

typedef struct
{
	float	point[3];
} dvertex_t;

typedef struct
{
	float	normal[3];
	float	dist;
	int		type;		// PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate
} dplane_t;

typedef struct
{
	int			planenum;
	short		children[2];	// negative numbers are -(leafs+1), not nodes
	short		mins[3];		// for sphere culling
	short		maxs[3];
	unsigned short	firstface;
	unsigned short	numfaces;	// counting both sides
} dnode_t;

typedef struct texinfo_s
{
	float		vecs[2][4];		// [s/t][xyz offset]
	int			miptex;
	int			flags;
} texinfo_t;
#define	TEX_SPECIAL		1		// sky or slime, no lightmap or 256 subdivision

// note that edge 0 is never used, because negative edge nums are used for
// counterclockwise use of the edge in a face
typedef struct
{
	unsigned short	v[2];		// vertex numbers
} dedge_t;

typedef struct
{
	short		planenum;
	short		side;

	int			firstedge;		// we must support > 64k edges
	short		numedges;
	short		texinfo;

// lighting info
	byte		styles[MAXLIGHTMAPS];
	int			lightofs;		// start of [numstyles*surfsize] samples
} dface_t;

// leaf 0 is the generic CONTENTS_SOLID leaf, used for all solid areas
// all other leafs need visibility info
typedef struct
{
	int			contents;
	int			visofs;				// -1 = no visibility info

	short		mins[3];			// for frustum culling
	short		maxs[3];

	unsigned short		firstmarksurface;
	unsigned short		nummarksurfaces;

	byte		ambient_level[NUM_AMBIENTS];
} dleaf_t;

#endif // BSPFILE_H
