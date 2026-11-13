// zone.h -- memory allocation: the zone, the hunk and the cache (zone.c)

#ifndef ZONE_H
#define ZONE_H

void	Memory_Init( void *buf, int size );

void	Z_Free( void *ptr );
void	*Z_Malloc( int size );			// returns 0 filled memory
void	*Z_TagMalloc( int size, int tag );

void	Z_CheckHeap( void );

void	*Hunk_Alloc( int size );		// returns 0 filled memory
void	*Hunk_AllocName( int size, char *name );

void	*Hunk_HighAllocName( int size, char *name );

int		Hunk_LowMark( void );
void	Hunk_FreeToLowMark( int mark );

int		Hunk_HighMark( void );
void	Hunk_FreeToHighMark( int mark );

void	*Hunk_TempAlloc( int size );

void	Hunk_Check( void );

typedef struct cache_user_s
{
	void	*data;
} cache_user_t;

void	Cache_Flush( void );

void	*Cache_Check( cache_user_t *c );
// returns the cached data, and moves to the head of the LRU list
// if present, otherwise returns NULL

void	Cache_Free( cache_user_t *c );

void	*Cache_Alloc( cache_user_t *c, int size, char *name );
// Returns NULL if all purgable data was tossed and there still
// wasn't enough room.

void	Cache_Report( void );

void	Hunk_Print( qboolean all );
void	Cache_FreeLow( int new_low_hunk );
void	Cache_FreeHigh( int new_high_hunk );
void	Cache_Force_Flush( void );
void	Cache_Print( void );
int		ComparePath1( char *path1, char *path2 );
char	*CommatizeNumber( int num, char *pout );
void	Cache_Compact( void );
void	Cache_Init( void );
int		Cache_TotalUsed( void );
void	Cache_Print_Models_And_Totals( void );
void	Cache_Print_Sounds_And_Totals( void );

#endif // ZONE_H
