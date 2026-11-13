//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Holds the enumerated list of default cursors
//
// $NoKeywords: $
//=============================================================================

#ifndef DAR_H
#define DAR_H

#ifdef _WIN32
#pragma once
#endif

#include <stdlib.h>
#include <string.h>
#include <vgui/VGUI.h>

namespace vgui2
{

//-----------------------------------------------------------------------------
// Purpose: Simple lightweight dynamic array implementation
//-----------------------------------------------------------------------------
template<class ELEMTYPE> class Dar
{
public:
	Dar();
	Dar(int initialCapacity);
	~Dar();

public:
	void EnsureCapacity(int wantedCapacity);
	void SetCount(int count);
	int GetCount();
	int AddElement(ELEMTYPE elem);
	void MoveElementToEnd( ELEMTYPE elem );
	int FindElement(ELEMTYPE elem);
	bool HasElement(ELEMTYPE elem);
	int PutElement(ELEMTYPE elem);
	void InsertElementAt(ELEMTYPE elem,int index);
	void SetElementAt(ELEMTYPE elem,int index);
	void RemoveElementAt(int index);
	void RemoveElementsBefore(int index);
	void RemoveElement(ELEMTYPE elem);
	void RemoveAll();
	ELEMTYPE operator[](int index);
	void *GetBaseData();
	void CopyFrom(Dar<ELEMTYPE> &dar);

protected:
	int _count;
	int _capacity;
	ELEMTYPE *_data;
};

//-----------------------------------------------------------------------------
// Purpose: Constructors and destructor
//-----------------------------------------------------------------------------
template<class ELEMTYPE>
Dar<ELEMTYPE>::Dar()
{
	_count = 0;
	_capacity = 0;
	_data = null;
}

template<class ELEMTYPE>
Dar<ELEMTYPE>::Dar(int initialCapacity)
{
	_count = 0;
	_capacity = 0;
	_data = null;
	EnsureCapacity(initialCapacity);
}

template<class ELEMTYPE>
Dar<ELEMTYPE>::~Dar()
{
	delete [] _data;
}

//-----------------------------------------------------------------------------
// Purpose: Makes sure the array can hold wantedCapacity elements
//-----------------------------------------------------------------------------
template<class ELEMTYPE>
void Dar<ELEMTYPE>::EnsureCapacity(int wantedCapacity)
{
	if (wantedCapacity <= _capacity)
	{
		return;
	}

	//double capacity until it is >= wantedCapacity
	//this could be done with math, but iterative is just so much more fun
	int newCapacity = _capacity;
	if (newCapacity == 0)
	{
		newCapacity = 1;
	}
	while (newCapacity < wantedCapacity)
	{
		newCapacity *= 2;
	}

	//allocate and zero newData
	ELEMTYPE *newData = new ELEMTYPE[newCapacity];
	if (newData == null)
	{
		exit(0);
	}
	memset(newData, 0, sizeof(ELEMTYPE) * newCapacity);
	_capacity = newCapacity;

	//copy data into newData
	for (int i = 0; i < _count; i++)
	{
		newData[i] = _data[i];
	}

	delete [] _data;
	_data = newData;
}

template<class ELEMTYPE>
void Dar<ELEMTYPE>::SetCount(int count)
{
	if ((count < 0) || (count > _capacity))
	{
		return;
	}
	_count = count;
}

template<class ELEMTYPE>
int Dar<ELEMTYPE>::GetCount()
{
	return _count;
}

template<class ELEMTYPE>
int Dar<ELEMTYPE>::AddElement(ELEMTYPE elem)
{
	EnsureCapacity(_count + 1);
	_data[_count] = elem;
	_count++;
	return _count - 1;
}

template<class ELEMTYPE>
void Dar<ELEMTYPE>::MoveElementToEnd( ELEMTYPE elem )
{
	// quick check to see if it's already at the end
	if (_data[_count - 1] == elem)
	{
		return;
	}

	// search back from the end
	for (int i = _count - 2; i >= 0; i--)
	{
		if (_data[i] == elem)
		{
			// shift the rest down and put the element at the end
			memmove(&_data[i], &_data[i + 1], (_count - (i + 1)) * sizeof(ELEMTYPE));
			_data[_count - 1] = elem;
			return;
		}
	}
}

template<class ELEMTYPE>
int Dar<ELEMTYPE>::FindElement(ELEMTYPE elem)
{
	for (int i = 0; i < _count; i++)
	{
		if (_data[i] == elem)
		{
			return i;
		}
	}
	return -1;
}

template<class ELEMTYPE>
bool Dar<ELEMTYPE>::HasElement(ELEMTYPE elem)
{
	if (FindElement(elem) >= 0)
	{
		return true;
	}
	return false;
}

template<class ELEMTYPE>
int Dar<ELEMTYPE>::PutElement(ELEMTYPE elem)
{
	int index = FindElement(elem);
	if (index >= 0)
	{
		return index;
	}
	return AddElement(elem);
}

template<class ELEMTYPE>
void Dar<ELEMTYPE>::InsertElementAt(ELEMTYPE elem, int index)
{
	if ((index < 0) || (index > _count))
	{
		return;
	}

	if ((index == _count) || (_count == 0))
	{
		AddElement(elem);
	}
	else
	{
		AddElement(elem); //just to make sure it is big enough
		for (int i = _count - 1; i > index; i--)
		{
			_data[i] = _data[i - 1];
		}
		_data[index] = elem;
	}
}

template<class ELEMTYPE>
void Dar<ELEMTYPE>::SetElementAt(ELEMTYPE elem, int index)
{
	if ((index < 0) || (index >= _count))
	{
		return;
	}

	_data[index] = elem;
}

template<class ELEMTYPE>
void Dar<ELEMTYPE>::RemoveElementAt(int index)
{
	if ((index < 0) || (index >= _count))
	{
		return;
	}

	//slide everything to the right of index, left one.
	for (int i = index; i < (_count - 1); i++)
	{
		_data[i] = _data[i + 1];
	}
	_count--;
}

template<class ELEMTYPE>
void Dar<ELEMTYPE>::RemoveElementsBefore(int index)
{
	if (index <= 0)
	{
		return;
	}

	//slide everything to the right of index, left index places.
	for (int i = index; i < _count; i++)
	{
		_data[i - index] = _data[i];
	}
	_count -= index;
}

template<class ELEMTYPE>
void Dar<ELEMTYPE>::RemoveElement(ELEMTYPE elem)
{
	for (int i = 0; i < _count; i++)
	{
		if (_data[i] == elem)
		{
			RemoveElementAt(i);
			break;
		}
	}
}

template<class ELEMTYPE>
void Dar<ELEMTYPE>::RemoveAll()
{
	_count = 0;
}

template<class ELEMTYPE>
ELEMTYPE Dar<ELEMTYPE>::operator[](int index)
{
	if ((index < 0) || (index >= _count))
	{
		return null;
	}
	return _data[index];
}

template<class ELEMTYPE>
void *Dar<ELEMTYPE>::GetBaseData()
{
	return _data;
}

template<class ELEMTYPE>
void Dar<ELEMTYPE>::CopyFrom(Dar<ELEMTYPE> &dar)
{
	EnsureCapacity(dar._count);
	for (int i = 0; i < dar._count; i++)
	{
		_data[i] = dar._data[i];
	}
	_count = dar._count;
}

} // namespace vgui2

#endif // DAR_H
