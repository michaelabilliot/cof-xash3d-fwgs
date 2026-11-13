// draw.h -- these are the only functions outside the refresh allowed
// to touch the vid buffer (gl_draw.c)

#ifndef DRAW_H
#define DRAW_H

extern	struct qpic_s	*draw_disc;	// also used on sbar

typedef struct
{
	char			name[64];
	cache_user_t	cache;
} cacheentry_t;

typedef struct cachewad_s cachewad_t;

typedef void (*PFNCACHE)( cachewad_t *, byte * );

struct cachewad_s
{
	char			*name;
	cacheentry_t	*cache;
	int				cacheCount;
	int				cacheMax;
	struct lumpinfo_s	*lumps;
	int				lumpCount;
	int				cacheExtra;
	PFNCACHE		pfnCacheBuild;
	int				numpaths;
	char			**basedirs;
	int				*lumppathindices;
	int				tempWad;
};

int		Draw_Character( int x, int y, int num, unsigned int font );
int		Draw_String( int x, int y, char *str );
int		Draw_StringLen( const char *psz, unsigned int font );
void	Draw_SetTextColor( float r, float g, float b );
void	Draw_ResetTextColor( void );
void	Draw_GetDefaultColor( void );
void	Draw_Pic( int x, int y, struct qpic_s *pic );
void	Draw_TileClear( int x, int y, int w, int h );
void	Draw_BeginDisc( void );
struct qpic_s	*Draw_PicFromWad( char *name );

// decals.c
void	Draw_DecalSetName( int decal, char *name );
void	Draw_Shutdown( void );
void	Draw_DecalShutdown( void );
void	Draw_CacheWadInit( char *name, int cacheMax, cachewad_t *wad );
qboolean	Draw_CustomCacheWadInit( int cacheMax, cachewad_t *wad, void *raw, int nFileSize );
void	Draw_CacheWadHandler( cachewad_t *wad, PFNCACHE fn, int extraDataSize );
void	Draw_FreeWad( cachewad_t *pWad );
int		Draw_DecalIndex( int id );
int		Draw_DecalCount( void );
const char	*Draw_DecalName( int number );
struct texture_s	*Draw_DecalTexture( int index );
int		Draw_CacheByIndex( cachewad_t *wad, int nIndex, int playernum );
int		Draw_DecalIndexFromName( char *name );
void	Decal_Init( void );
qboolean	CustomDecal_Validate( void *raw, int nFileSize );
qboolean	CustomDecal_Init( struct cachewad_s *wad, void *raw, int nFileSize, int playernum );
void	*Draw_CacheGet( cachewad_t *wad, int index );
void	*Draw_CustomCacheGet( cachewad_t *wad, void *raw, int rawsize, int index );
qboolean	Draw_CacheReload( cachewad_t *wad, int i, struct lumpinfo_s *pLump, cacheentry_t *pic, char *clean, char *path );
qboolean	Draw_ValidateCustomLogo( cachewad_t *wad, unsigned char *data, struct lumpinfo_s *lump );
qboolean	Draw_CacheLoadFromCustom( char *clean, cachewad_t *wad, void *raw, int rawsize, cacheentry_t *pic );
int		Draw_CacheIndex( cachewad_t *wad, char *path );
int		Draw_CacheFindIndex( cachewad_t *wad, char *path );

// gl_draw.c
void	Draw_MiptexTexture( cachewad_t *wad, byte *data );

// cl_draw.c
void	SPR_Shutdown_NoModelFree( void );

// gl_draw.c
void	Draw_FillRGBA( int x, int y, int w, int h, int r, int g, int b, int a );

#endif // DRAW_H
