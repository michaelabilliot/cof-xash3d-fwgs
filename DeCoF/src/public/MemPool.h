//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================

#ifndef MEMPOOL_H
#define MEMPOOL_H

#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Optimized pool memory allocator
//-----------------------------------------------------------------------------
class CMemoryPool
{
public:
	// Initial block size
	CMemoryPool(int blockSize, int numElements);
	~CMemoryPool();

	void		*Alloc(unsigned int amount);
	void		Free(void *memblock);
	int			Count() { return _blocksAllocated; }

private:
	enum
	{
		MAX_BLOBS = 16
	};

	void		AddNewBlob();

	int			_blockSize;
	int			_blocksPerBlob;
	int			_numElements;
	void		*_memBlob[MAX_BLOBS];
	void		*_headOfFreeList;
	int			_numBlobs;
	int			_peakAlloc;
	int			_blocksAllocated;
};

#endif // MEMPOOL_H
