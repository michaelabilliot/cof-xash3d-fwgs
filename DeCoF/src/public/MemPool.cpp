//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include "MemPool.h"
#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include <math.h>

// a failed check stops in the debugger
#define Assert( _exp ) if ( !( _exp ) ) { __asm int 3 }

#ifndef max
#define max(a,b)	(((a) > (b)) ? (a) : (b))
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMemoryPool::CMemoryPool(int blockSize, int numElements)
{
	_blockSize = blockSize;
	_numBlobs = 0;
	_numElements = 0;
	_blocksPerBlob = numElements;

	AddNewBlob();

	_peakAlloc = 0;
	_blocksAllocated = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Frees the memory contained in the mempool, and invalidates it for
//			any further use.
//-----------------------------------------------------------------------------
CMemoryPool::~CMemoryPool()
{
	if (_blocksAllocated > 0)
	{
		// blocks still allocated (leak report not compiled in)
	}

	for (int i = 0; i < _numBlobs; i++)
	{
		free(_memBlob[i]);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMemoryPool::AddNewBlob()
{
	void **newBlob;
	int i = _numBlobs;
	int sizeMultiplier = pow( 2, _numBlobs );

	// allocate memory for the blob
	_memBlob[i] = malloc(_blockSize * (_blocksPerBlob * sizeMultiplier));
	Assert(_memBlob[i]);

	// set the head of the free list to the start of the new blob
	_headOfFreeList = _memBlob[i];
	Assert(_headOfFreeList);

	// Setup the free list inside the blob
	newBlob = (void **)_headOfFreeList;
	for (int j = 0; j < (_blocksPerBlob * sizeMultiplier) - 1; j++)
	{
		newBlob[0] = (char *)newBlob + _blockSize;
		newBlob = (void **)newBlob[0];
	}

	// null terminate list
	newBlob[0] = NULL;

	_numBlobs++;
	_numElements += _blocksPerBlob * sizeMultiplier;

	Assert(_numBlobs < (MAX_BLOBS - 1));
}

//-----------------------------------------------------------------------------
// Purpose: Allocs a single block of memory from the pool.  
//-----------------------------------------------------------------------------
void *CMemoryPool::Alloc(unsigned int amount)
{
	void *returnBlock;

	if (amount > (unsigned int)_blockSize)
		return NULL;

	_blocksAllocated++;
	_peakAlloc = max(_peakAlloc, _blocksAllocated);

	if (_blocksAllocated >= _numElements)
	{
		AddNewBlob();
	}

	// check to see if we've run out of space
	Assert(_headOfFreeList);

	// pop the first block off the free list
	returnBlock = _headOfFreeList;

	// make the next block the head of the list
	_headOfFreeList = *((void **)_headOfFreeList);

	return returnBlock;
}

//-----------------------------------------------------------------------------
// Purpose: Frees a block of memory
//-----------------------------------------------------------------------------
void CMemoryPool::Free(void *memBlock)
{
	if (!memBlock)
		return;  // trying to delete NULL pointer, ignore

	--_blocksAllocated;

	// make the block point to the first item in the list
	*((void **)memBlock) = _headOfFreeList;

	// the list head is now the new block
	_headOfFreeList = memBlock;
}
