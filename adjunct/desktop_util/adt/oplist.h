/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OPLIST_H
#define OPLIST_H

class OpGenericListItem;

/***********************************************************************************
**
** OpListItemFreeStore
**
***********************************************************************************/

class OpListItemFreeStore
{
public:

	/**

	 */
	OpListItemFreeStore();


	/**

	 */
	OpGenericListItem * Alloc();


	/**

	 */
	void Free(OpGenericListItem * item);

private:

	enum { CHUNKSIZE = 10 };

	OpGenericListItem* m_items;

	OpListItemFreeStore(const OpListItemFreeStore&);
	OpListItemFreeStore& operator =(const OpListItemFreeStore&);

};

/***********************************************************************************
**
** OpGenericList
**
***********************************************************************************/

class OpGenericList
{
protected:

	/**

	 */
	OpGenericList();


	/**

	 */
	virtual ~OpGenericList();


	/**

	 */
	OP_STATUS AddLast(void* object);


	/**

	 */
	void * RemoveFirst();


	/**

	 */
	bool IsEmpty() const;


	/**

	 */
	OpGenericListItem * GetFirst() { return m_first; }


	/**

	 */
	OpGenericListItem * GetLast()  { return m_last;  }

protected:

	OpGenericListItem* m_first;
	OpGenericListItem* m_last;

	static OpListItemFreeStore m_listitems;

private:

	OpGenericList(const OpGenericList&);
	OpGenericList& operator =(const OpGenericList&);

};


/***********************************************************************************
**
** OpGenericIterableList
**
***********************************************************************************/

class OpGenericIterableList
	: private OpGenericList
{
protected:

	/**

	 */
	OpGenericIterableList();


	/**

	 */
	void* Begin();


	/**

	 */
	void* Next();


	/**

	 */
	void* GetCurrentItem();


	/**

	 */
	void* RemoveCurrentItem();


	/**

	 */
	void* RemoveItem(void* item);


	/**

	 */
	bool  IsEmpty() const
		{
			return OpGenericList::IsEmpty();
		}


	/**

	 */
	OP_STATUS AddLast(void* object)
		{
			return OpGenericList::AddLast(object);
		}

	// don't expose RemoveFirst, since it will mess
	// up the m_current pointer

private:

	OpGenericListItem* m_current;
	OpGenericListItem* m_prev;

	OpGenericIterableList(const OpGenericIterableList&);
	OpGenericIterableList& operator =(const OpGenericIterableList&);
};


/***********************************************************************************
**
** OpDesktopList
**
***********************************************************************************/

template<class T>
class OpDesktopList
	: private OpGenericIterableList
{
public:

	/**

	 */
	OpDesktopList() : OpGenericIterableList() {}


	/**

	 */
	OP_STATUS AddLast(T* item)
		{
			return OpGenericIterableList::AddLast(item);
		}


	/**

	 */
	T* Begin()
		{
			return static_cast<T*>(OpGenericIterableList::Begin());
		}


	/**

	 */
	T* Next()
		{
			return static_cast<T*>(OpGenericIterableList::Next());
		}


	/**

	 */
	bool IsEmpty() const
		{
			return OpGenericIterableList::IsEmpty();
		}


	/**

	 */
	T* GetCurrentItem()
		{
			return static_cast<T*>(OpGenericIterableList::GetCurrentItem());
		}


	/**

	 */
	T* RemoveCurrentItem()
		{
			return static_cast<T*>(OpGenericIterableList::RemoveCurrentItem());
		}


	/**

	 */
	T* RemoveItem(T* item)
		{
			return static_cast<T*>(OpGenericIterableList::RemoveItem(item));
		}


	/**

	 */
    OpDesktopList(const OpDesktopList&);


	/**

	 */
	OpDesktopList& operator =(const OpDesktopList&);
};

#endif // OPADT_H
