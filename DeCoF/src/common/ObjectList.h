// ObjectList.h -- doubly linked list of void pointers

#ifndef OBJECTLIST_H
#define OBJECTLIST_H

#include "IObjectContainer.h"

class ObjectList : public IObjectContainer
{
public:
	void	Init();
	bool	Add( void *newObject );
	void	*GetFirst();
	void	*GetNext();

	ObjectList();
	virtual	~ObjectList();

	void	Clear( bool freeElementsMemory = false );
	int		CountElements();
	void	*RemoveTail();
	void	*RemoveHead();

	bool	AddTail( void *newObject );
	bool	AddHead( void *newObject );
	bool	Remove( void *object );
	bool	Contains( void *object );
	bool	IsEmpty();

	typedef struct element_s
	{
		struct element_s	*prev;	// pointer to the last element or NULL
		struct element_s	*next;	// pointer to the next elemnet or NULL
		void				*object;	// the element's object
	} element_t;

protected:
	element_t	*m_head;	// first element in list
	element_t	*m_tail;	// last element in list
	element_t	*m_current;	// current element in list
	int			m_number;
};

#endif // OBJECTLIST_H
