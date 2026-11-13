//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: A growable memory class.
//
// $NoKeywords: $
//=============================================================================

#ifndef UTLMEMORY_H
#define UTLMEMORY_H

#ifdef _WIN32
#pragma once
#endif

#include <stdlib.h>
#include <new.h>

//-----------------------------------------------------------------------------
// Methods to invoke the constructor, copy constructor, and destructor
//-----------------------------------------------------------------------------
template <class T>
inline void Construct( T* pMemory )
{
	new( pMemory ) T;
}

template <class T>
inline void CopyConstruct( T* pMemory, T const& src )
{
	new( pMemory ) T(src);
}

template <class T>
inline void Destruct( T* pMemory )
{
	pMemory->~T();
}

//-----------------------------------------------------------------------------
// The CUtlMemory class:
// A growable memory class which doubles in size by default.
//-----------------------------------------------------------------------------
template< class T >
class CUtlMemory
{
public:
	// constructor, destructor
	CUtlMemory( int nGrowSize = 0, int nInitSize = 0 );
	CUtlMemory( T* pMemory, int numElements );
	~CUtlMemory();

	// element access
	T& operator[]( int i );
	T const& operator[]( int i ) const;
	T& Element( int i );
	T const& Element( int i ) const;

	// Can we use this index?
	bool IsIdxValid( int i ) const;

	// Gets the base address (can change when adding elements!)
	T* Base();
	T const* Base() const;

	// Attaches the buffer to external memory....
	void SetExternalBuffer( T* pMemory, int numElements );

	// Size
	int NumAllocated() const;
	int Count() const;

	// Grows the memory, so that at least allocated + num elements are allocated
	void Grow( int num = 0 );

	// Makes sure we've got at least this much memory
	void EnsureCapacity( int num );

	// Memory deallocation
	void Purge();

	// is the memory externally allocated?
	bool IsExternallyAllocated() const;

	// Set the size by which the memory grows
	void SetGrowSize( int size );

private:
	enum
	{
		EXTERNAL_BUFFER_MARKER = -1,
	};

	T* m_pMemory;
	int m_NumAllocated;
	int m_GrowSize;
};

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
template< class T >
CUtlMemory<T>::CUtlMemory( int nGrowSize, int nInitSize ) : m_pMemory(0),
	m_NumAllocated( nInitSize ), m_GrowSize( nGrowSize )
{
	if (m_NumAllocated)
	{
		m_pMemory = (T*)malloc( m_NumAllocated * sizeof(T) );
	}
}

template< class T >
CUtlMemory<T>::CUtlMemory( T* pMemory, int numElements ) : m_pMemory(pMemory),
	m_NumAllocated( numElements )
{
	// Special marker indicating externally supplied memory
	m_GrowSize = EXTERNAL_BUFFER_MARKER;
}

//-----------------------------------------------------------------------------
// destructor
//-----------------------------------------------------------------------------
template< class T >
CUtlMemory<T>::~CUtlMemory()
{
	Purge();
}

//-----------------------------------------------------------------------------
// element access
//-----------------------------------------------------------------------------
template< class T >
inline T& CUtlMemory<T>::operator[]( int i )
{
	return m_pMemory[i];
}

//-----------------------------------------------------------------------------
// Gets the base address (can change when adding elements!)
//-----------------------------------------------------------------------------
template< class T >
inline T* CUtlMemory<T>::Base()
{
	return m_pMemory;
}

//-----------------------------------------------------------------------------
// is the memory externally allocated?
//-----------------------------------------------------------------------------
template< class T >
bool CUtlMemory<T>::IsExternallyAllocated() const
{
	return m_GrowSize < 0;
}

//-----------------------------------------------------------------------------
// Memory deallocation
//-----------------------------------------------------------------------------
template< class T >
void CUtlMemory<T>::Purge()
{
	if (!IsExternallyAllocated())
	{
		if (m_pMemory)
		{
			free( (void*)m_pMemory );
			m_pMemory = 0;
		}
		m_NumAllocated = 0;
	}
}

//-----------------------------------------------------------------------------
// element access
//-----------------------------------------------------------------------------
template< class T >
inline T const& CUtlMemory<T>::operator[]( int i ) const
{
	return m_pMemory[i];
}

template< class T >
inline T& CUtlMemory<T>::Element( int i )
{
	return m_pMemory[i];
}

template< class T >
inline T const& CUtlMemory<T>::Element( int i ) const
{
	return m_pMemory[i];
}

//-----------------------------------------------------------------------------
// Can we use this index?
//-----------------------------------------------------------------------------
template< class T >
inline bool CUtlMemory<T>::IsIdxValid( int i ) const
{
	return (i >= 0) && (i < m_NumAllocated);
}

template< class T >
inline T const* CUtlMemory<T>::Base() const
{
	return m_pMemory;
}

//-----------------------------------------------------------------------------
// Attaches the buffer to external memory....
//-----------------------------------------------------------------------------
template< class T >
void CUtlMemory<T>::SetExternalBuffer( T* pMemory, int numElements )
{
	// Blow away any existing allocated memory
	Purge();

	m_pMemory = pMemory;
	m_NumAllocated = numElements;

	// Indicate that we don't own the memory
	m_GrowSize = EXTERNAL_BUFFER_MARKER;
}

//-----------------------------------------------------------------------------
// Size
//-----------------------------------------------------------------------------
template< class T >
inline int CUtlMemory<T>::NumAllocated() const
{
	return m_NumAllocated;
}

template< class T >
inline int CUtlMemory<T>::Count() const
{
	return m_NumAllocated;
}

//-----------------------------------------------------------------------------
// Grows the memory
//-----------------------------------------------------------------------------
template< class T >
void CUtlMemory<T>::Grow( int num )
{
	if ( IsExternallyAllocated() && ( num > 0 ) )
	{
		// Can't grow a buffer whose memory was externally allocated
		return;
	}

	// Make sure we have at least numallocated + num allocations.
	// Use the grow rules specified for this memory (in m_GrowSize)
	int growSize = ( num > 0 ) ? num : m_GrowSize;
	if ( growSize )
	{
		m_NumAllocated += growSize;
	}
	else
	{
		// double the size
		m_NumAllocated += m_NumAllocated ? m_NumAllocated : 1;
	}

	if (m_pMemory)
	{
		m_pMemory = (T*)realloc( m_pMemory, m_NumAllocated * sizeof(T) );
	}
	else
	{
		m_pMemory = (T*)malloc( m_NumAllocated * sizeof(T) );
	}
}

//-----------------------------------------------------------------------------
// Makes sure we've got at least this much memory
//-----------------------------------------------------------------------------
template< class T >
void CUtlMemory<T>::EnsureCapacity( int num )
{
	if (m_NumAllocated < num)
	{
		Grow( num - m_NumAllocated );
	}
}

//-----------------------------------------------------------------------------
// Set the size by which the memory grows
//-----------------------------------------------------------------------------
template< class T >
void CUtlMemory<T>::SetGrowSize( int nSize )
{
	m_GrowSize = nSize;
}


#endif // UTLMEMORY_H
