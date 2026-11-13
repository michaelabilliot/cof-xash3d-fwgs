// zone.c -- memory allocation: the zone, the hunk and the cache

#include "quakedef.h"
#include "cvar.h"
#include "cmd.h"
#include "zone.h"
#include "mathlib.h"
#include "client.h"
#include "delta.h"
#include "server.h"
#include "studio.h"

#define	DYNAMIC_SIZE	0x200000

#define	ZONEID			0x1d4a11
#define MINFRAGMENT		64

typedef struct memblock_s
{
	int		size;		// including the header and possibly tiny fragments
	int		tag;		// a tag of 0 is a free block
	int		id;			// should be ZONEID
	struct memblock_s	*next, *prev;
	int		pad;		// pad to 64 bit boundary
} memblock_t;

typedef struct
{
	int		size;		// total bytes malloced, including header
	memblock_t	blocklist;		// start / end cap for linked list
	memblock_t	*rover;
} memzone_t;

cvar_t	mem_dbgfile = { "mem_dbgfile", ".\\mem.txt" };

/*
==============================================================================

						ZONE MEMORY ALLOCATION

There is never any space between memblocks, and there will never be two
contiguous free memblocks.

The rover can be left pointing at a non-empty block

The zone calls are pretty much only used for small strings and structures,
all big things are allocated on the hunk.
==============================================================================
*/

memzone_t	*mainzone;

/*
========================
Z_ClearZone
========================
*/
void Z_ClearZone( memzone_t *zone, int size )
{
	memblock_t	*block;

	// set the entire zone to one free block

	zone->blocklist.next = zone->blocklist.prev = block =
		(memblock_t *)( (byte *)zone + sizeof( memzone_t ) );
	zone->blocklist.tag = 1;	// in use block
	zone->blocklist.id = 0;
	zone->blocklist.size = 0;
	zone->rover = block;

	block->prev = block->next = &zone->blocklist;
	block->tag = 0;			// free block
	block->id = ZONEID;
	block->size = size - sizeof( memzone_t );
}

/*
========================
Z_Free
========================
*/
void Z_Free( void *ptr )
{
	memblock_t	*block, *other;

	if ( !ptr )
		Sys_Error( "Z_Free: NULL pointer" );

	block = (memblock_t *)( (byte *)ptr - sizeof( memblock_t ) );
	if ( block->id != ZONEID )
		Sys_Error( "Z_Free: freed a pointer without ZONEID" );
	if ( block->tag == 0 )
		Sys_Error( "Z_Free: freed a freed pointer" );

	block->tag = 0;		// mark as free

	other = block->prev;
	if ( !other->tag )
	{	// merge with previous free block
		other->size += block->size;
		other->next = block->next;
		other->next->prev = other;
		if ( block == mainzone->rover )
			mainzone->rover = other;
		block = other;
	}

	other = block->next;
	if ( !other->tag )
	{	// merge the next free block onto the end
		block->size += other->size;
		block->next = other->next;
		block->next->prev = block;
		if ( other == mainzone->rover )
			mainzone->rover = block;
	}
}

/*
========================
Z_Malloc
========================
*/
void *Z_Malloc( int size )
{
	void	*buf;

	Z_CheckHeap();	// DEBUG
	buf = Z_TagMalloc( size, 1 );
	if ( !buf )
		Sys_Error( "Z_Malloc: failed on allocation of %i bytes", size );
	Q_memset( buf, 0, size );

	return buf;
}

void *Z_TagMalloc( int size, int tag )
{
	int			extra;
	memblock_t	*start, *rover, *newmem, *base;

	if ( !tag )
		Sys_Error( "Z_TagMalloc: tried to use a 0 tag" );

	//
	// scan through the block list looking for the first free block
	// of sufficient size
	//
	size += sizeof( memblock_t );	// account for size of block header
	size += 4;					// space for memory trash tester
	size = ( size + 7 ) & ~7;		// align to 8-byte boundary

	base = rover = mainzone->rover;
	start = base->prev;

	do
	{
		if ( rover == start )	// scaned all the way around the list
			return NULL;
		if ( rover->tag )
			base = rover = rover->next;
		else
			rover = rover->next;
	} while ( base->tag || base->size < size );

	//
	// found a block big enough
	//
	extra = base->size - size;
	if ( extra > MINFRAGMENT )
	{	// there will be a free fragment after the allocated block
		newmem = (memblock_t *)( (byte *)base + size );
		newmem->size = extra;
		newmem->tag = 0;			// free block
		newmem->prev = base;
		newmem->id = ZONEID;
		newmem->next = base->next;
		newmem->next->prev = newmem;
		base->next = newmem;
		base->size = size;
	}

	base->tag = tag;				// no longer a free block

	mainzone->rover = base->next;	// next allocation will start looking here

	base->id = ZONEID;

	// marker for memory trash testing
	*(int *)( (byte *)base + base->size - 4 ) = ZONEID;

	return (void *)( (byte *)base + sizeof( memblock_t ) );
}

/*
========================
Z_Print
========================
*/
void Z_Print( memzone_t *zone )
{
	memblock_t	*block;

	Con_Printf( "zone size: %i  location: %p\n", mainzone->size, mainzone );

	for ( block = zone->blocklist.next; ; block = block->next )
	{
		Con_Printf( "block:%p    size:%7i    tag:%3i\n",
			block, block->size, block->tag );

		if ( block->next == &zone->blocklist )
			break;			// all blocks have been hit
		if ( (byte *)block + block->size != (byte *)block->next )
			Con_Printf( "ERROR: block size does not touch the next block\n" );
		if ( block->next->prev != block )
			Con_Printf( "ERROR: next block doesn't have proper back link\n" );
		if ( !block->tag && !block->next->tag )
			Con_Printf( "ERROR: two consecutive free blocks\n" );
	}
}

/*
========================
Z_CheckHeap
========================
*/
void Z_CheckHeap( void )
{
	memblock_t	*block;

	for ( block = mainzone->blocklist.next; ; block = block->next )
	{
		if ( block->next == &mainzone->blocklist )
			break;			// all blocks have been hit
		if ( (byte *)block + block->size != (byte *)block->next )
			Sys_Error( "Z_CheckHeap: block size does not touch the next block\n" );
		if ( block->next->prev != block )
			Sys_Error( "Z_CheckHeap: next block doesn't have proper back link\n" );
		if ( !block->tag && !block->next->tag )
			Sys_Error( "Z_CheckHeap: two consecutive free blocks\n" );
	}
}

//============================================================================

#define	HUNK_SENTINAL	0x1df001ed

typedef struct
{
	int		sentinal;
	int		size;		// including sizeof(hunk_t), -1 = not allocated
	char	name[64];
} hunk_t;

byte		*hunk_base;
int			hunk_size;

int			hunk_low_used;
int			hunk_high_used;

qboolean	hunk_tempactive;
int			hunk_tempmark;

/*
==============
Hunk_Check

Run consistancy and sentinal trahing checks
==============
*/
void Hunk_Check( void )
{
	hunk_t	*h;

	for ( h = (hunk_t *)hunk_base; (byte *)h != hunk_base + hunk_low_used; )
	{
		if ( h->sentinal != HUNK_SENTINAL )
			Sys_Error( "Hunk_Check: trahsed sentinal" );
		if ( h->size < 16 || h->size + (byte *)h - hunk_base > hunk_size )
			Sys_Error( "Hunk_Check: bad size" );
		h = (hunk_t *)( (byte *)h + h->size );
	}
}

/*
==============
Hunk_Print

If "all" is specified, every single allocation is printed.
Otherwise, allocations with the same name will be totaled up before printing.
==============
*/
void Hunk_Print( qboolean all )
{
	hunk_t	*h, *next, *endlow, *starthigh, *endhigh;
	int		count, sum;
	int		totalblocks;
	char	name[65];
	FileHandle_t	file;
	char	commabuf[50];

	name[64] = 0;
	count = 0;
	sum = 0;
	totalblocks = 0;

	file = FS_Open( mem_dbgfile.string, "a" );
	if ( !file )
		return;

	h = (hunk_t *)hunk_base;
	endlow = (hunk_t *)( hunk_base + hunk_low_used );
	starthigh = (hunk_t *)( hunk_base + hunk_size - hunk_high_used );
	endhigh = (hunk_t *)( hunk_base + hunk_size );

	FS_FPrintf( file, "          :%16.16s total hunk size\n", CommatizeNumber( hunk_size, commabuf ) );
	FS_FPrintf( file, "-------------------------\n" );

	while ( 1 )
	{
		//
		// skip to the high hunk if done with low hunk
		//
		if ( h == endlow )
		{
			FS_FPrintf( file, "-------------------------\n" );
			FS_FPrintf( file, "          :%16.16s REMAINING\n", CommatizeNumber( hunk_size - hunk_low_used - hunk_high_used, commabuf ) );
			FS_FPrintf( file, "-------------------------\n" );
			h = starthigh;
		}

		//
		// if totally done, break
		//
		if ( h == endhigh )
			break;

		//
		// run consistancy checks
		//
		if ( h->sentinal != HUNK_SENTINAL )
			Sys_Error( "Hunk_Check: trahsed sentinal" );
		if ( h->size < 16 || h->size + (byte *)h - hunk_base > hunk_size )
			Sys_Error( "Hunk_Check: bad size" );

		next = (hunk_t *)( (byte *)h + h->size );
		count++;
		totalblocks++;
		sum += h->size;

		//
		// print the single block
		//
		Q_memcpy( name, h->name, 64 );
		if ( all )
			FS_FPrintf( file, "%8p :%16.16s %16s\n", h, CommatizeNumber( h->size, commabuf ), name );

		//
		// print the total
		//
		if ( next == endlow || next == endhigh ||
			Q_strncmp( h->name, next->name, 64 ) )
		{
			if ( !all )
				FS_FPrintf( file, "          :%16.16s %16s (TOTAL)\n", CommatizeNumber( sum, commabuf ), name );
			count = 0;
			sum = 0;
		}

		h = next;
	}

	FS_FPrintf( file, "-------------------------\n" );
	FS_FPrintf( file, "%8i total blocks\n", totalblocks );
	FS_Close( file );
}

/*
===================
Hunk_AllocName
===================
*/
void *Hunk_AllocName( int size, char *name )
{
	hunk_t	*h;

	if ( size < 0 )
		Sys_Error( "Hunk_Alloc: bad size: %i", size );

	size = sizeof( hunk_t ) + ( ( size + 15 ) & ~15 );

	if ( hunk_size - hunk_low_used - hunk_high_used < size )
		Sys_Error( "Hunk_Alloc: failed on %i bytes", size );

	h = (hunk_t *)( hunk_base + hunk_low_used );
	hunk_low_used += size;

	Cache_FreeLow( hunk_low_used );

	Q_memset( h, 0, size );

	h->size = size;
	h->sentinal = HUNK_SENTINAL;
	Q_strncpy( h->name, name, 64 );
	h->name[63] = 0;

	return (void *)( h + 1 );
}

/*
===================
Hunk_Alloc
===================
*/
void *Hunk_Alloc( int size )
{
	return Hunk_AllocName( size, "unknown" );
}

int Hunk_LowMark( void )
{
	return hunk_low_used;
}

void Hunk_FreeToLowMark( int mark )
{
	if ( mark < 0 || mark > hunk_low_used )
		Sys_Error( "Hunk_FreeToLowMark: bad mark %i", mark );
	hunk_low_used = mark;
}

int Hunk_HighMark( void )
{
	if ( hunk_tempactive )
	{
		hunk_tempactive = FALSE;
		Hunk_FreeToHighMark( hunk_tempmark );
	}

	return hunk_high_used;
}

void Hunk_FreeToHighMark( int mark )
{
	if ( hunk_tempactive )
	{
		hunk_tempactive = FALSE;
		Hunk_FreeToHighMark( hunk_tempmark );
	}
	if ( mark < 0 || mark > hunk_high_used )
		Sys_Error( "Hunk_FreeToHighMark: bad mark %i", mark );
	hunk_high_used = mark;
}

/*
===================
Hunk_HighAllocName
===================
*/
void *Hunk_HighAllocName( int size, char *name )
{
	hunk_t	*h;

	if ( size < 0 )
		Sys_Error( "Hunk_HighAllocName: bad size: %i", size );

	if ( hunk_tempactive )
	{
		Hunk_FreeToHighMark( hunk_tempmark );
		hunk_tempactive = FALSE;
	}

	size = sizeof( hunk_t ) + ( ( size + 15 ) & ~15 );

	if ( hunk_size - hunk_low_used - hunk_high_used < size )
	{
		Con_Printf( "Hunk_HighAlloc: failed on %i bytes\n", size );
		return NULL;
	}

	hunk_high_used += size;
	Cache_FreeHigh( hunk_high_used );

	h = (hunk_t *)( hunk_base + hunk_size - hunk_high_used );

	Q_memset( h, 0, size );
	h->size = size;
	h->sentinal = HUNK_SENTINAL;
	Q_strncpy( h->name, name, 64 );
	h->name[63] = 0;

	return (void *)( h + 1 );
}

/*
=================
Hunk_TempAlloc

Return space from the top of the hunk
=================
*/
void *Hunk_TempAlloc( int size )
{
	void	*buf;

	size = ( size + 15 ) & ~15;

	if ( hunk_tempactive )
	{
		Hunk_FreeToHighMark( hunk_tempmark );
		hunk_tempactive = FALSE;
	}

	hunk_tempmark = Hunk_HighMark();

	buf = Hunk_HighAllocName( size, "temp" );

	hunk_tempactive = TRUE;

	return buf;
}

/*
===============================================================================

CACHE MEMORY

===============================================================================
*/

typedef struct cache_system_s
{
	int						size;		// including this header
	cache_user_t			*user;
	char					name[64];
	struct cache_system_s	*prev, *next;
	struct cache_system_s	*lru_prev, *lru_next;	// for LRU flushing
} cache_system_t;

cache_system_t *Cache_TryAlloc( int size, qboolean nobottom );

cache_system_t	cache_head;

/*
===========
Cache_Move
===========
*/
void Cache_Move( cache_system_t *c )
{
	cache_system_t	*newmem;

	// we are clearing up space at the bottom, so only allocate it late
	newmem = Cache_TryAlloc( c->size, TRUE );
	if ( newmem )
	{
		Q_memcpy( newmem + 1, c + 1, c->size - sizeof( cache_system_t ) );
		newmem->user = c->user;
		Q_memcpy( newmem->name, c->name, sizeof( newmem->name ) );
		Cache_Free( c->user );
		newmem->user->data = (void *)( newmem + 1 );
	}
	else
	{
		Cache_Free( c->user );		// tough luck...
	}
}

/*
============
Cache_FreeLow

Throw things out until the hunk can be expanded to the given point
============
*/
void Cache_FreeLow( int new_low_hunk )
{
	cache_system_t	*c;

	while ( 1 )
	{
		c = cache_head.next;
		if ( c == &cache_head )
			return;		// nothing in cache at all
		if ( (byte *)c >= hunk_base + new_low_hunk )
			return;		// there is space to grow the hunk
		Cache_Move( c );	// reclaim the space
	}
}

/*
============
Cache_FreeHigh

Throw things out until the hunk can be expanded to the given point
============
*/
void Cache_FreeHigh( int new_high_hunk )
{
	cache_system_t	*c, *prev;

	prev = NULL;
	while ( 1 )
	{
		c = cache_head.prev;
		if ( c == &cache_head )
			return;		// nothing in cache at all
		if ( (byte *)c + c->size <= hunk_base + hunk_size - new_high_hunk )
			return;		// there is space to grow the hunk
		if ( c == prev )
			Cache_Free( c->user );	// didn't move out of the way
		else
		{
			Cache_Move( c );	// try to move it
			prev = c;
		}
	}
}

void Cache_UnlinkLRU( cache_system_t *cs )
{
	if ( !cs->lru_next || !cs->lru_prev )
		Sys_Error( "Cache_UnlinkLRU: NULL link" );

	cs->lru_next->lru_prev = cs->lru_prev;
	cs->lru_prev->lru_next = cs->lru_next;

	cs->lru_prev = cs->lru_next = NULL;
}

void Cache_MakeLRU( cache_system_t *cs )
{
	if ( cs->lru_next || cs->lru_prev )
		Sys_Error( "Cache_MakeLRU: active link" );

	cache_head.lru_next->lru_prev = cs;
	cs->lru_next = cache_head.lru_next;
	cs->lru_prev = &cache_head;
	cache_head.lru_next = cs;
}

/*
============
Cache_TryAlloc

Looks for a free block of memory between the high and low hunk marks
Size should already include the header and padding
============
*/
cache_system_t *Cache_TryAlloc( int size, qboolean nobottom )
{
	cache_system_t	*cs, *newmem;

	// is the cache completely empty?

	if ( !nobottom && cache_head.prev == &cache_head )
	{
		if ( hunk_size - hunk_high_used - hunk_low_used < size )
			Sys_Error( "Cache_TryAlloc: %i is greater then free hunk", size );

		newmem = (cache_system_t *)( hunk_base + hunk_low_used );
		Q_memset( newmem, 0, sizeof( *newmem ) );
		newmem->size = size;

		cache_head.prev = cache_head.next = newmem;
		newmem->prev = newmem->next = &cache_head;

		Cache_MakeLRU( newmem );
		return newmem;
	}

	// search from the bottom up for space

	newmem = (cache_system_t *)( hunk_base + hunk_low_used );
	cs = cache_head.next;

	do
	{
		if ( !nobottom || cs != cache_head.next )
		{
			if ( (byte *)cs - (byte *)newmem >= size )
			{	// found space
				Q_memset( newmem, 0, sizeof( *newmem ) );
				newmem->size = size;

				newmem->next = cs;
				newmem->prev = cs->prev;
				cs->prev->next = newmem;
				cs->prev = newmem;

				Cache_MakeLRU( newmem );

				return newmem;
			}
		}

		// continue looking
		newmem = (cache_system_t *)( (byte *)cs + cs->size );
		cs = cs->next;

	} while ( cs != &cache_head );

	// try to allocate one at the very end
	if ( hunk_base + hunk_size - hunk_high_used - (byte *)newmem >= size )
	{
		Q_memset( newmem, 0, sizeof( *newmem ) );
		newmem->size = size;

		newmem->next = &cache_head;
		newmem->prev = cache_head.prev;
		cache_head.prev->next = newmem;
		cache_head.prev = newmem;

		Cache_MakeLRU( newmem );

		return newmem;
	}

	return NULL;		// couldn't allocate
}

/*
============
Cache_Force_Flush

Throw everything out, so new data will be demand cached
============
*/
void Cache_Force_Flush( void )
{
	while ( cache_head.next != &cache_head )
		Cache_Free( cache_head.next->user );	// reclaim the space
}

static int CacheSystemCompare( const void *ppcs1, const void *ppcs2 );

/*
============
Cache_Flush

Throw everything out, so new data will be demand cached
============
*/
void Cache_Flush( void )
{
	if ( cl.maxclients <= 1 || allow_cheats )
	{
		Cache_Force_Flush();
	}
	else
	{
		Con_Printf( "Server must enable sv_cheats to activate the flush command in multiplayer games.\n" );
	}
}

/*
============
Cache_Print
============
*/
void Cache_Print( void )
{
	cache_system_t	*cd;
	cache_system_t	*sortarray[512];
	int				i, j;
	FileHandle_t	file;
	char			commabuf[50];

	i = 0;
	j = 0;

	file = FS_Open( mem_dbgfile.string, "a" );
	if ( !file )
		return;

	Q_memset( sortarray, sizeof( cache_system_t * ) * 512, 0 );

	FS_FPrintf( file, "\nCACHE:\n" );

	for ( cd = cache_head.next; cd != &cache_head; cd = cd->next )
		sortarray[i++] = cd;

	qsort( sortarray, i, sizeof( cache_system_t * ), CacheSystemCompare );

	for ( j = 0; j < i; j++ )
		FS_FPrintf( file, "%16.16s : %-16s\n", CommatizeNumber( sortarray[j]->size, commabuf ), sortarray[j]->name );

	FS_Close( file );
}

static int CacheSystemCompare( const void *ppcs1, const void *ppcs2 )
{
	cache_system_t	*pcs1 = *(cache_system_t **)ppcs1;
	cache_system_t	*pcs2 = *(cache_system_t **)ppcs2;

	return _stricmp( pcs1->name, pcs2->name );
}

int ComparePath1( char *path1, char *path2 )
{
	while ( *path1 != '/' && *path1 != '\\' && *path1 )
	{
		if ( *path1 != *path2 )
			return 0;
		path1++;
		path2++;
	}

	return 1;
}

char *CommatizeNumber( int num, char *pout )
{
	int		len;
	int		i;
	char	outbuf[50];

	len = 0;
	Q_memset( outbuf, 0, sizeof( outbuf ) );

	while ( num )
	{
		char	tempbuf[50];
		int		temp;

		temp = num % 1000;
		num = num / 1000;
		Q_strncpy( tempbuf, outbuf, sizeof( tempbuf ) - 1 );
		tempbuf[sizeof( tempbuf ) - 1] = 0;

		_snprintf( outbuf, sizeof( outbuf ), ",%03i%s", temp, tempbuf );
	}

	len = Q_strlen( outbuf );

	for ( i = 0; i < len; i++ )
	{
		if ( outbuf[i] != '0' && outbuf[i] != ',' )
			break;
	}

	if ( i == len )
		Q_strcpy( pout, "0" );
	else
		Q_strcpy( pout, &outbuf[i] );

	return pout;
}

/*
============
Cache_Report
============
*/
void Cache_Report( void )
{
	Con_DPrintf( "%4.1f megabyte data cache\n", ( hunk_size - hunk_high_used - hunk_low_used ) / (float)( 1024 * 1024 ) );
}

/*
============
Cache_Compact
============
*/
void Cache_Compact( void )
{
}

/*
============
Cache_Init
============
*/
void Cache_Init( void )
{
	cache_head.next = cache_head.prev = &cache_head;
	cache_head.lru_next = cache_head.lru_prev = &cache_head;

	Cmd_AddCommand( "flush", Cache_Flush );
}

/*
==============
Cache_Free

Frees the memory and removes it from the LRU list
==============
*/
void Cache_Free( cache_user_t *c )
{
	cache_system_t	*cs;

	if ( !c->data )
		Sys_Error( "Cache_Free: not allocated" );

	cs = ( (cache_system_t *)c->data ) - 1;

	cs->prev->next = cs->next;
	cs->next->prev = cs->prev;
	cs->next = cs->prev = NULL;

	c->data = NULL;

	Cache_UnlinkLRU( cs );
}

int Cache_TotalUsed( void )
{
	cache_system_t	*cd;
	int				Total;

	Total = 0;
	for ( cd = cache_head.next; cd != &cache_head; cd = cd->next )
		Total += cd->size;

	return Total;
}

/*
==============
Cache_Check
==============
*/
void *Cache_Check( cache_user_t *c )
{
	cache_system_t	*cs;

	if ( !c->data )
		return NULL;

	cs = ( (cache_system_t *)c->data ) - 1;

	// move to head of LRU
	Cache_UnlinkLRU( cs );
	Cache_MakeLRU( cs );

	return c->data;
}

/*
==============
Cache_Alloc
==============
*/
void *Cache_Alloc( cache_user_t *c, int size, char *name )
{
	cache_system_t	*cs;

	if ( c->data )
		Sys_Error( "Cache_Alloc: already allocated" );

	if ( size <= 0 )
		Sys_Error( "Cache_Alloc: size %i", size );

	size = ( size + sizeof( cache_system_t ) + 15 ) & ~15;

	// find memory for it
	while ( 1 )
	{
		cs = Cache_TryAlloc( size, FALSE );
		if ( cs )
		{
			Q_strncpy( cs->name, name, sizeof( cs->name ) - 1 );
			cs->name[sizeof( cs->name ) - 1] = 0;
			c->data = (void *)( cs + 1 );
			cs->user = c;
			break;
		}

		// free the least recently used cahedat
		if ( cache_head.lru_prev == &cache_head )
			Sys_Error( "Cache_Alloc: out of memory" );
		// not enough memory at all
		Cache_Free( cache_head.lru_prev->user );
	}

	return Cache_Check( c );
}

//============================================================================

/*
========================
Memory_Init
========================
*/
void Memory_Init( void *buf, int size )
{
	int		p;
	int		zonesize;

	zonesize = DYNAMIC_SIZE;

	hunk_base = (byte *)buf;
	hunk_size = size;
	hunk_low_used = 0;
	hunk_high_used = 0;

	Cache_Init();

	p = COM_CheckParm( "-zone" );
	if ( p )
	{
		if ( p < com_argc - 1 )
			zonesize = Q_atoi( com_argv[p + 1] ) * 1024;
		else
			Sys_Error( "Memory_Init: you must specify a size in KB after -zone" );
	}

	mainzone = (memzone_t *)Hunk_AllocName( zonesize, "zone" );
	Z_ClearZone( mainzone, zonesize );
}

void Cache_Print_Models_And_Totals( void )
{
	char			buf[50];
	cache_system_t	*cd;
	cache_system_t	*sortarray[512];
	int32			i, j;
	int32			totalbytes;
	FileHandle_t	file;
	int				subtot;

	i = 0;
	j = 0;
	totalbytes = 0;

	file = FS_Open( mem_dbgfile.string, "a" );
	subtot = 0;
	if ( !file )
		return;

	Q_memset( sortarray, sizeof( cache_system_t * ) * 512, 0 );

	// pull out all the models
	for ( cd = cache_head.next; cd != &cache_head; cd = cd->next )
	{
		if ( Q_strstr( cd->name, ".mdl" ) )
			sortarray[i++] = cd;
	}

	qsort( sortarray, i, sizeof( cache_system_t * ), CacheSystemCompare );

	FS_FPrintf( file, "\nCACHED MODELS:\n" );

	// now process the sorted list
	for ( j = 0; j < i; j++ )
	{
		int					k;
		mstudiotexture_t	*ptexture;
		studiohdr_t			*phdr;

		phdr = (studiohdr_t *)sortarray[j]->user->data;
		ptexture = (mstudiotexture_t *)( (byte *)phdr + phdr->textureindex );

		subtot = 0;
		for ( k = 0; k < phdr->numtextures; k++ )
			subtot += ptexture[k].width * ptexture[k].height + 768;	// 768 = 256 color palette * 3 bytes

		FS_FPrintf( file, "\t%16.16s : %s\n", CommatizeNumber( sortarray[j]->size, buf ), sortarray[j]->name );
		totalbytes += sortarray[j]->size;
	}

	FS_FPrintf( file, "Total bytes in cache used by models:  %s\n", CommatizeNumber( totalbytes, buf ) );
	FS_Close( file );
}

void Cache_Print_Sounds_And_Totals( void )
{
	char			buf[50];
	cache_system_t	*cd;
	cache_system_t	*sortarray[1024];
	int32			i, j;
	int32			totalsndbytes;
	FileHandle_t	file;
	int				subtot;

	i = 0;
	j = 0;
	totalsndbytes = 0;

	file = FS_Open( mem_dbgfile.string, "a" );
	subtot = 0;
	if ( !file )
		return;

	Q_memset( sortarray, sizeof( cache_system_t * ) * 1024, 0 );

	// pull out all the sounds
	for ( cd = cache_head.next; cd != &cache_head; cd = cd->next )
	{
		if ( Q_strstr( cd->name, ".wav" ) )
			sortarray[i++] = cd;
	}

	qsort( sortarray, i, sizeof( cache_system_t * ), CacheSystemCompare );

	if ( !file )
		return;

	FS_FPrintf( file, "\nCACHED SOUNDS:\n" );

	// now process the sorted list
	for ( j = 0; j < i; j++ )
	{
		FS_FPrintf( file, "\t%16.16s : %s\n", CommatizeNumber( sortarray[j]->size, buf ), sortarray[j]->name );
		totalsndbytes += sortarray[j]->size;

		// print the total of each directory
		if ( j + 1 == i || !ComparePath1( sortarray[j]->name, sortarray[j + 1]->name ) )
		{
			char	pathbuf[512];

			Sys_SplitPath( sortarray[j]->name, NULL, pathbuf, NULL, NULL );
			FS_FPrintf( file, "\tTotal Bytes used in \"%s\": %s\n", pathbuf, CommatizeNumber( totalsndbytes - subtot, buf ) );
			subtot = totalsndbytes;
		}
	}

	FS_FPrintf( file, "Total bytes in cache used by sound:  %s\n", CommatizeNumber( totalsndbytes, buf ) );
	FS_Close( file );
}
