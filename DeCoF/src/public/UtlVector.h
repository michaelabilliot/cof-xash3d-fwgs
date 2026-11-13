//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//
// A growable array class that maintains a free list and keeps elements
// in the same location
//=============================================================================

#ifndef UTLVECTOR_H
#define UTLVECTOR_H

#ifdef _WIN32
#pragma once
#endif

#include <string.h>
#include "UtlMemory.h"

//-----------------------------------------------------------------------------
// The CUtlVector class:
// A growable array class which doubles in size by default.
// It will always keep all elements consecutive in memory, and may move the
// elements around in memory (via a realloc) when elements are inserted or
// removed. Clients should therefore refer to the elements of the vector
// by index (they should *never* maintain pointers to elements in the vector).
//-----------------------------------------------------------------------------
template< class T >
class CUtlVector
{
public:
	typedef T ElemType_t;

	// constructor, destructor
	CUtlVector( int growSize = 0, int initSize = 0 );
	CUtlVector( T* pMemory, int numElements );
	~CUtlVector();

	// element access
	T& operator[]( int i );
	T const& operator[]( int i ) const;
	T& Element( int i );
	T const& Element( int i ) const;

	// Gets the base address (can change when adding elements!)
	T* Base();
	T const* Base() const;

	// Returns the number of elements in the vector
	// SIZE IS DEPRECATED!
	int Count() const;
	int Size() const;	// don't use me!

	// Is element index valid?
	bool IsValidIndex( int i ) const;

	// Adds an element, uses default constructor
	int AddToHead();
	int AddToTail();
	int InsertBefore( int elem );
	int InsertAfter( int elem );

	// Adds an element, uses copy constructor
	int AddToHead( T const& src );
	int AddToTail( T const& src );
	int InsertBefore( int elem, T const& src );
	int InsertAfter( int elem, T const& src );

	// Adds multiple elements, uses default constructor
	int AddMultipleToHead( int num );
	int AddMultipleToTail( int num );
	int InsertMultipleBefore( int elem, int num );
	int InsertMultipleAfter( int elem, int num );

	// Calls RemoveAll() then AddMultipleToTail.
	void SetSize( int size );

	// Calls SetSize and copies each element.
	void CopyArray( T const *pArray, int size );

	// Add the specified array to the tail.
	int AddVectorToTail( CUtlVector<T> const &src );

	// Finds an element (element needs operator== defined)
	int Find( T const& src );

	bool HasElement( T const& src );

	// Makes sure we have enough memory allocated to store a requested # of elements
	void EnsureCapacity( int num );

	// Element removal
	void FastRemove( int elem );	// doesn't preserve order
	void Remove( int elem );		// preserves order, shifts elements
	void FindAndRemove( T const& src );	// removes first occurrence of src, preserves order, shifts elements
	void RemoveMultiple( int elem, int num );	// preserves order, shifts elements
	void RemoveAll();				// doesn't deallocate memory

	// Memory deallocation
	void Purge();

	// Set the size by which it grows when it needs to allocate more memory.
	void SetGrowSize( int size );

protected:
	// Can't copy this unless we explicitly do it!
	CUtlVector( CUtlVector const& vec );

	// Grows the vector
	void GrowVector( int num = 1 );

	// Shifts elements....
	void ShiftElementsRight( int elem, int num = 1 );
	void ShiftElementsLeft( int elem, int num = 1 );

	// For easier access to the elements through the debugger
	void ResetDbgInfo();

	CUtlMemory<T> m_Memory;
	int m_Size;

	// For easier access to the elements through the debugger
	// it's in release builds so this can be used in libraries correctly
	T *m_pElements;
};

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
template< class T >
inline CUtlVector<T>::CUtlVector( int growSize, int initSize )	:
	m_Memory(growSize, initSize), m_Size(0)
{
	ResetDbgInfo();
}

template< class T >
inline CUtlVector<T>::CUtlVector( T* pMemory, int numElements )	:
	m_Memory(pMemory, numElements), m_Size(0)
{
	ResetDbgInfo();
}

template< class T >
inline CUtlVector<T>::~CUtlVector()
{
	Purge();
}

//-----------------------------------------------------------------------------
// element access
//-----------------------------------------------------------------------------
template< class T >
inline T& CUtlVector<T>::operator[]( int i )
{
	return m_Memory[i];
}

template< class T >
inline T const& CUtlVector<T>::operator[]( int i ) const
{
	return m_Memory[i];
}

template< class T >
inline T& CUtlVector<T>::Element( int i )
{
	return m_Memory[i];
}

template< class T >
inline T const& CUtlVector<T>::Element( int i ) const
{
	return m_Memory[i];
}

//-----------------------------------------------------------------------------
// Gets the base address (can change when adding elements!)
//-----------------------------------------------------------------------------
template< class T >
inline T* CUtlVector<T>::Base()
{
	return m_Memory.Base();
}

template< class T >
inline T const* CUtlVector<T>::Base() const
{
	return m_Memory.Base();
}

//-----------------------------------------------------------------------------
// Count
//-----------------------------------------------------------------------------
template< class T >
inline int CUtlVector<T>::Count() const
{
	return m_Size;
}

template< class T >
inline int CUtlVector<T>::Size() const
{
	return m_Size;
}

//-----------------------------------------------------------------------------
// Is element index valid?
//-----------------------------------------------------------------------------
template< class T >
inline bool CUtlVector<T>::IsValidIndex( int i ) const
{
	return (i >= 0) && (i < m_Size);
}

//-----------------------------------------------------------------------------
// Adds an element, uses default constructor
//-----------------------------------------------------------------------------
template< class T >
inline int CUtlVector<T>::AddToHead()
{
	return InsertBefore(0);
}

template< class T >
inline int CUtlVector<T>::AddToTail()
{
	return InsertBefore( m_Size );
}

template< class T >
int CUtlVector<T>::InsertBefore( int elem )
{
	GrowVector();
	ShiftElementsRight(elem);
	Construct( &Element(elem) );
	return elem;
}

template< class T >
inline int CUtlVector<T>::InsertAfter( int elem )
{
	return InsertBefore( elem + 1 );
}

//-----------------------------------------------------------------------------
// Adds an element, uses copy constructor
//-----------------------------------------------------------------------------
template< class T >
inline int CUtlVector<T>::AddToHead( T const& src )
{
	return InsertBefore( 0, src );
}

template< class T >
inline int CUtlVector<T>::AddToTail( T const& src )
{
	return InsertBefore( m_Size, src );
}

template< class T >
int CUtlVector<T>::InsertBefore( int elem, T const& src )
{
	GrowVector();
	ShiftElementsRight(elem);
	CopyConstruct( &Element(elem), src );
	return elem;
}

template< class T >
inline int CUtlVector<T>::InsertAfter( int elem, T const& src )
{
	return InsertBefore( elem + 1, src );
}

//-----------------------------------------------------------------------------
// Adds multiple elements, uses default constructor
//-----------------------------------------------------------------------------
template< class T >
inline int CUtlVector<T>::AddMultipleToHead( int num )
{
	return InsertMultipleBefore( 0, num );
}

template< class T >
inline int CUtlVector<T>::AddMultipleToTail( int num )
{
	return InsertMultipleBefore( m_Size, num );
}

template< class T >
int CUtlVector<T>::InsertMultipleBefore( int elem, int num )
{
	if( num == 0 )
		return elem;

	GrowVector(num);
	ShiftElementsRight(elem, num);

	// Invoke default constructors
	for (int i = 0; i < num; ++i)
		Construct( &Element(elem+i) );

	return elem;
}

template< class T >
inline int CUtlVector<T>::InsertMultipleAfter( int elem, int num )
{
	return InsertMultipleBefore( elem + 1, num );
}

//-----------------------------------------------------------------------------
// Calls RemoveAll() then AddMultipleToTail.
//-----------------------------------------------------------------------------
template< class T >
void CUtlVector<T>::SetSize( int size )
{
	RemoveAll();
	AddMultipleToTail( size );
}

//-----------------------------------------------------------------------------
// Calls SetSize and copies each element.
//-----------------------------------------------------------------------------
template< class T >
void CUtlVector<T>::CopyArray( T const *pArray, int size )
{
	SetSize( size );
	for( int i=0; i < size; i++ )
		(*this)[i] = pArray[i];
}

//-----------------------------------------------------------------------------
// Add the specified array to the tail.
//-----------------------------------------------------------------------------
template< class T >
int CUtlVector<T>::AddVectorToTail( CUtlVector<T> const &src )
{
	int base = Count();

	// Make space.
	AddMultipleToTail( src.Count() );

	// Copy the elements.
	for ( int i=0; i < src.Count(); i++ )
		(*this)[base + i] = src[i];

	return base;
}

//-----------------------------------------------------------------------------
// Finds an element (element needs operator== defined)
//-----------------------------------------------------------------------------
template< class T >
int CUtlVector<T>::Find( T const& src )
{
	for ( int i = 0; i < Size(); ++i )
	{
		if (Element(i) == src)
			return i;
	}
	return -1;
}

template< class T >
bool CUtlVector<T>::HasElement( T const& src )
{
	return ( Find(src) >= 0 );
}

//-----------------------------------------------------------------------------
// Makes sure we have enough memory allocated to store a requested # of elements
//-----------------------------------------------------------------------------
template< class T >
void CUtlVector<T>::EnsureCapacity( int num )
{
	m_Memory.EnsureCapacity(num);
	ResetDbgInfo();
}

//-----------------------------------------------------------------------------
// Element removal
//-----------------------------------------------------------------------------
template< class T >
void CUtlVector<T>::FastRemove( int elem )
{
	Destruct( &Element(elem) );
	if (m_Size > 0)
	{
		memcpy( &Element(elem), &Element(m_Size-1), sizeof(T) );
		--m_Size;
	}
}

template< class T >
void CUtlVector<T>::Remove( int elem )
{
	Destruct( &Element(elem) );
	ShiftElementsLeft(elem);
	--m_Size;
}

template< class T >
void CUtlVector<T>::FindAndRemove( T const& src )
{
	int elem = Find( src );
	if ( elem != -1 )
	{
		Remove( elem );
	}
}

template< class T >
void CUtlVector<T>::RemoveMultiple( int elem, int num )
{
	for (int i = elem + num; --i >= elem; )
		Destruct(&Element(i));

	ShiftElementsLeft(elem, num);
	m_Size -= num;
}

template< class T >
void CUtlVector<T>::RemoveAll()
{
	for (int i = m_Size; --i >= 0; )
		Destruct(&Element(i));

	m_Size = 0;
}

//-----------------------------------------------------------------------------
// Memory deallocation
//-----------------------------------------------------------------------------
template< class T >
void CUtlVector<T>::Purge()
{
	RemoveAll();
	m_Memory.Purge( );
	ResetDbgInfo();
}

template< class T >
void CUtlVector<T>::SetGrowSize( int size )
{
	m_Memory.SetGrowSize( size );
}

//-----------------------------------------------------------------------------
// Grows the vector
//-----------------------------------------------------------------------------
template< class T >
void CUtlVector<T>::GrowVector( int num )
{
	if (m_Size + num > m_Memory.NumAllocated())
	{
		m_Memory.Grow( m_Size + num - m_Memory.NumAllocated() );
	}

	m_Size += num;
	ResetDbgInfo();
}

//-----------------------------------------------------------------------------
// Shifts elements
//-----------------------------------------------------------------------------
template< class T >
void CUtlVector<T>::ShiftElementsRight( int elem, int num )
{
	int numToMove = m_Size - elem - num;
	if ((numToMove > 0) && (num > 0))
		memmove( &Element(elem+num), &Element(elem), numToMove * sizeof(T) );
}

template< class T >
void CUtlVector<T>::ShiftElementsLeft( int elem, int num )
{
	int numToMove = m_Size - elem - num;
	if ((numToMove > 0) && (num > 0))
	{
		memmove( &Element(elem), &Element(elem+num), numToMove * sizeof(T) );
	}
}

//-----------------------------------------------------------------------------
// For easier access to the elements through the debugger
//-----------------------------------------------------------------------------
template< class T >
inline void CUtlVector<T>::ResetDbgInfo()
{
#ifdef _DEBUG
	m_pElements = m_Memory.Base();
#endif
}

#endif // UTLVECTOR_H
