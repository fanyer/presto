/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_SIMSET_H
#define MODULES_UTIL_SIMSET_H

#include "modules/util/excepts.h"

class Head;

/** Element in a doubly linked list. */
class Link
{
	friend class Head;
			// function Append need direct access
			// to private members of the Link class
private:

    Link*				suc;
    Link*				pred;

	Head*				parent;

	// dummy copy constructors

	Link(const Link&);
	Link& operator=(const Link&);
	
protected:

	Head*				GetList() const { return parent; }

public:

						Link() : suc(NULL), pred(NULL), parent(NULL) {}
	virtual             ~Link() {}

	void				DEPRECATED(Reset());			///< Clear element (dangerous).

	Link*				Suc() const { return suc; }		///< Get next element.
    Link*				Pred() const { return pred; }	///< Get previous element.

    void				Out();							///< Remove from list.

    void				Into(Head* list);				///< Add to end of list.
    void				IntoStart(Head* list);			///< Add to start of list.

    void				Follow(Link* link);				///< Add to list after link.
    void				Precede(Link* link);			///< Add to list before link.

	/** Check if this element precedes the target element in the list. */
	BOOL				Precedes(const Link* link) const;

	/** Check if this element is part of a list. */
    BOOL				InList() const { return parent != NULL; };

	/** Get identifier for this element, if implemented. */
    virtual	const char* LinkId() { return 0; };
};

inline void Link::Reset() { suc = pred = NULL; parent = NULL; }

/** A doubly-linked list. This class represents the head of a doubly linked
  * list of elements inheriting from the Link class. */
class Head
{
private:

	friend class Link;

    Link*			first;
    Link*			last;

	// dummy copy constructors

					Head(const Head&);
					Head& operator=(const Head&);
	
public:

					Head() : first(NULL), last(NULL) {}
	virtual			~Head(){}

	void			DEPRECATED(Reset());				///< Clear list (dangerous)

    Link*			Last() const { return last; }		///< Get last element.
    Link*			First() const { return first; }		///< Get first element.

	/** Get number of elements. */
    UINT			Cardinal() const;

	/** Check if list is empty. */
    BOOL			Empty() const { return first == NULL; }

	/** Clear list, deallocating all elements. */
    void			Clear();
	/** Clear list, without deallocating elements. */
	void			RemoveAll() { while (first) first->Out(); }

	/** Add all items in list to this list. */
	void			Append(Head *list);

	/** Check if element belongs to this list. */
	BOOL			HasLink(Link* element) const { return element->GetList() == this; }
};

inline void Head::Reset() { first = last = NULL; }

/** A doubly-linked list that deletes its content when destructed. */
class AutoDeleteHead
	: public Head
{
public:
	virtual ~AutoDeleteHead() { Clear(); }
};

/** A link element with an integer hash key. */
class HashedLink : public Link
{
private:
	unsigned int	full_index;

public:

					HashedLink() : full_index(0){}
	
	void			SetIndex(unsigned int index){full_index = index;}
	unsigned int	GetIndex() const{return full_index;};
	HashedLink*		Suc() const { return (HashedLink *) Link::Suc(); }
    HashedLink*		Pred() const { return (HashedLink *) Link::Pred(); }
};

//***************************************************************************

template<class E> class List;

/**
 * Thin template wrapper for Link, eliminating the need for typecasts.
 * When subclassing, use the subclass itself as the element type template
 * parameter E. For instance:
 *
 * <pre>
 * class A: public ListElement<A>
 * {
 *     // ... payload data
 * };
 *
 * List<A> aList;
 * </pre>
 *
 * If you chose private inheritance, where the above uses public, you'll need a
 * friend class List<A>; declaration in the body of class A to avoid errors when
 * you try to use a List<A>.
 */
template<class E>
class ListElement : public Link // Should use private inheritance, but VS6 sucks
{
	friend class List<E>;
protected:
	List<E>* GetList() const				{ return static_cast<List<E>*>(Link::GetList()); }
public:
	E* Suc() const							{ return static_cast<E*>(Link::Suc()); }
	E* Pred() const							{ return static_cast<E*>(Link::Pred()); }
	void Out()								{ Link::Out(); }
	void Into(List<E>* list)				{ Link::Into(list); }
	void IntoStart(List<E>* list)			{ Link::IntoStart(list); }
	void Follow(E* element)					{ Link::Follow(element); }
	void Precede(E* element)				{ Link::Precede(element); }
	BOOL Precedes(const E* element) const	{ return Link::Precedes(element); }
	BOOL InList() const						{ return Link::InList(); }
	virtual	const char* LinkId()			{ return Link::LinkId(); }
};

/**
 * Thin template wrapper for Head, eliminating the need for typecasts.
 */
template<class E>
class List : public Head // Should use private inheritance, but VS6 sucks
{
	friend class ListElement<E>;
public:
	E* First() const						{ return static_cast<E*>(Head::First()); }
	E* Last() const							{ return static_cast<E*>(Head::Last()); }
	UINT Cardinal() const					{ return Head::Cardinal(); }
	BOOL Empty() const						{ return Head::Empty(); }
	void Clear()							{ Head::Clear(); }
	void RemoveAll()						{ Head::RemoveAll(); }
	void Append(List<E>* list)				{ Head::Append(list); }
	BOOL HasLink(E* element) const			{ return Head::HasLink(element); }
};

/**
 * A List that deletes all elements in its destructor
 */
template<class E>
class AutoDeleteList : public List<E>
{
public:
	virtual ~AutoDeleteList()				{ List<E>::Clear(); }
};

//***************************************************************************

template<class E> class CountedList;

/**
 * An element in a counted list, i.e. a list that has a counter for the
 * number of elements added, and can return the length of the list in
 * constant time.
 * When subclassing, use the subclass itself as the element type template
 * parameter E. For instance:
 *
 * <pre>
 * class A: public CountedListElement<A>
 * {
 *     // ... payload data
 * };
 *
 * CountedList<A> aList;
 * </pre>
 */
template<class E>
class CountedListElement : public Link // Should use private inheritance, but VS6 sucks
{
	friend class CountedList<E>;
protected:
	CountedList<E>* GetList() const			{ return static_cast<CountedList<E>*>(Link::GetList()); }
public:
	E* Suc() const							{ return static_cast<E*>(Link::Suc()); }
	E* Pred() const							{ return static_cast<E*>(Link::Pred()); }
	void Out()								{ GetList()->m_count--; Link::Out(); }
	void Into(CountedList<E>* list)			{ Link::Into(list); list->m_count++; }
	void IntoStart(CountedList<E>* list)	{ Link::IntoStart(list); list->m_count++; }
	void Follow(E* element)					{ Link::Follow(element); GetList()->m_count++; }
	void Precede(E* element)				{ Link::Precede(element); GetList()->m_count++; }
	BOOL Precedes(const E* element) const	{ return Link::Precedes(element); }
	BOOL InList() const						{ return Link::InList(); }
	virtual	const char* LinkId()			{ return Link::LinkId(); }
};

/**
 * A counted list, i.e. a list that has a counter for the number of
 * elements added, and can return the length of the list in constant time.
 * Note that CountedList uses 4 bytes more space than a normal List.
 */
template<class E>
class CountedList : public Head // Should use private inheritance, but VS6 sucks
{
	friend class CountedListElement<E>;
private:
	UINT m_count;
public:
	CountedList(): m_count(0)				{ }
	E* First() const						{ return static_cast<E*>(Head::First()); }
	E* Last() const							{ return static_cast<E*>(Head::Last()); }
	UINT Cardinal() const					{ return m_count; }
	BOOL Empty() const						{ return Head::Empty(); }
	void Clear()							{ Head::Clear(); m_count=0; }
	void RemoveAll()						{ Head::RemoveAll(); m_count=0; }
	void Append(CountedList<E>* list)		{ m_count += list->m_count; Head::Append(list); }
	BOOL HasLink(E* element) const			{ return Head::HasLink(element); }
};

/**
 * A CountedList that deletes all elements in its destructor
 */
template<class E>
class CountedAutoDeleteList : public CountedList<E>
{
public:
	virtual ~CountedAutoDeleteList()		{ CountedList<E>::Clear(); }
};

#endif // !MODULES_UTIL_SIMSET_H
