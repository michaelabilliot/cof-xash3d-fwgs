// ObjectList.cpp -- doubly linked list of void pointers

#include <stdlib.h>

#include "ObjectList.h"

extern "C"
{
#include "mem.h"
}

ObjectList::ObjectList()
{
	m_head = m_tail = m_current = NULL;
	m_number = 0;
}

ObjectList::~ObjectList()
{
	Clear( false );
}

bool ObjectList::AddHead( void *newObject )
{
	// create new element
	element_t *newElement = (element_t *)Mem_ZeroMalloc( sizeof( element_t ) );

	// out of memory
	if ( !newElement )
		return false;

	// insert element
	newElement->object = newObject;

	if ( m_head )
	{
		newElement->next = m_head;
		m_head->prev = newElement;
	}

	m_head = newElement;

	// if list was empty set new m_tail
	if ( !m_tail )
		m_tail = m_head;

	m_number++;

	return true;
}

void *ObjectList::RemoveHead()
{
	void *retObj;

	// check m_head is present
	if ( m_head )
	{
		retObj = m_head->object;
		element_t *newHead = m_head->next;
		if ( newHead )
			newHead->prev = NULL;

		// if only one element is in list also update m_tail
		// if we remove this prev element
		if ( m_tail == m_head )
			m_tail = NULL;

		Mem_Free( m_head );
		m_head = newHead;

		m_number--;
	}
	else
		retObj = NULL;

	return retObj;
}

bool ObjectList::AddTail( void *newObject )
{
	// create new element
	element_t *newElement = (element_t *)Mem_ZeroMalloc( sizeof( element_t ) );

	// out of memory
	if ( !newElement )
		return false;

	// insert element
	newElement->object = newObject;

	if ( m_tail )
	{
		newElement->prev = m_tail;
		m_tail->next = newElement;
	}

	m_tail = newElement;

	// if list was empty set new m_tail
	if ( !m_head )
		m_head = m_tail;

	m_number++;

	return true;
}

void *ObjectList::RemoveTail()
{
	void *retObj;

	// check m_tail is present
	if ( m_tail )
	{
		retObj = m_tail->object;
		element_t *newTail = m_tail->prev;
		if ( newTail )
			newTail->next = NULL;

		// if only one element is in list also update m_tail
		// if we remove this prev element
		if ( m_head == m_tail )
			m_head = NULL;

		Mem_Free( m_tail );
		m_tail = newTail;

		m_number--;
	}
	else
		retObj = NULL;

	return retObj;
}

bool ObjectList::IsEmpty()
{
	return ( m_head == NULL );
}

int ObjectList::CountElements()
{
	return m_number;
}

bool ObjectList::Contains( void *object )
{
	element_t *e = m_head;

	while ( e && e->object != object ) { e = e->next; }

	if ( e )
	{
		m_current = e;
		return true;
	}
	else
	{
		return false;
	}
}

void ObjectList::Clear( bool freeElementsMemory )
{
	element_t *ne;
	element_t *e = m_head;

	while ( e )
	{
		ne = e->next;

		if ( freeElementsMemory && e->object )
			Mem_Free( e->object );

		Mem_Free( e );
		e = ne;
	}

	m_head = m_tail = m_current = NULL;
	m_number = 0;
}

bool ObjectList::Remove( void *object )
{
	element_t *e = m_head;

	while ( e && e->object != object ) { e = e->next; }

	if ( e )
	{
		if ( e->prev ) e->prev->next = e->next;
		if ( e->next ) e->next->prev = e->prev;

		if ( m_head == e ) m_head = e->next;
		if ( m_tail == e ) m_tail = e->prev;
		if ( m_current == e ) m_current = e->next;

		Mem_Free( e );
		m_number--;
	}

	return ( e != NULL );
}

void ObjectList::Init()
{
	m_head = m_tail = m_current = NULL;
	m_number = 0;
}

void *ObjectList::GetFirst()
{
	if ( m_head )
	{
		m_current = m_head->next;
		return m_head->object;
	}
	else
	{
		m_current = NULL;
		return NULL;
	}
}

void *ObjectList::GetNext()
{
	void *retObj = NULL;

	if ( m_current )
	{
		retObj = m_current->object;
		m_current = m_current->next;
	}

	return retObj;
}

bool ObjectList::Add( void *newObject )
{
	return AddTail( newObject );
}
