/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef RESULTBASE_H
#define RESULTBASE_H

#include "modules/search_engine/TypeDescriptor.h"

/**
 * @brief basic iteration in search results
 * @author Pavel Studeny <pavels@opera.com>
 *
 * This iterator will give you one result at a time (method SearchIterator::Get). You can move
 * to other results until the end/beginning.
 */
class IteratorBase : public NonCopyable
{
public:
	IteratorBase() {}

	virtual ~IteratorBase(void) {}

	/**
	 * go to the next result
	 * @return FALSE if there are no more results or on error
	 */
	virtual BOOL Next(void) = 0;

	/**
	 * go to the previous result
	 * @return FALSE if there are no more results or on error
	 */
	virtual BOOL Prev(void) = 0;

	/**
	 * After performing any operation with Next/Prev, we need to check the Error status.
	 * @return status of the last operation
	 */
	CHECK_RESULT(virtual OP_STATUS Error(void) const) = 0;

	/**
	 * Number of results is often unknown.
	 * @return number of results available, -1 if the number is unknown, but at least one
	 */
	virtual int Count(void) const = 0;

	/**
	 * @return TRUE if Next() finished past the end of data
	 */
	virtual BOOL End(void) const = 0;

	/**
	 * @return TRUE if Prev() finished past the beginning of data
	 */
	virtual BOOL Beginning(void) const = 0;

	/**
	 * @return TRUE if there are no data
	 */
	BOOL Empty(void) const {return Count() == 0;}
};

/**
 * @brief common predecessor of all iterators
 */
template <typename T> class SearchIterator : public IteratorBase
{
public:
	/**
	 * @return the value of the current result
	 */
	virtual const T &Get(void) = 0;
};

/**
 * @brief Random-access iterator
 * Not used currently.
 */
template <typename T> class SearchRndIterator : public SearchIterator<T>
{
public:
	/**
	 * go to the first result
	 */
	virtual BOOL First(void) = 0;

	/**
	 * go to the last result
	 */
	virtual BOOL Last(void) = 0;

	/**
	 * go to result at specified position
	 */
	virtual BOOL Goto(int index) = 0;
};

/**
 * @brief returns sorted results from two other iterators
 * Or is still not implemented anywhere by any search class, but all code is here.
 */
template <typename T> class OrIterator : public SearchIterator<T>
{
public:

	OrIterator(TypeDescriptor::ComparePtr compare = NULL)
	{
		m_a1 = NULL;
		m_a2 = NULL;
		Compare = compare == NULL ? &DefDescriptor<T>::Compare : compare;
	}

	/**
	 * Results must be sorted before applying this.
	 */
	OrIterator(SearchIterator<T> *a1, SearchIterator<T> *a2, TypeDescriptor::ComparePtr compare = NULL)
	{
		OP_ASSERT(a1 != NULL);
		OP_ASSERT(a2 != NULL);
		m_a1 = a1;
		m_a2 = a2;
		Compare = compare == NULL ? &DefDescriptor<T>::Compare : compare;
	}

	void Set(SearchIterator<T> *a1, SearchIterator<T> *a2, TypeDescriptor::ComparePtr compare = NULL)
	{
		OP_ASSERT(a1 != NULL);
		OP_ASSERT(a2 != NULL);

		OP_DELETE(m_a1);
		OP_DELETE(m_a2);

		m_a1 = a1;
		m_a2 = a2;
		if (compare != NULL)
			Compare = compare;
	}

	virtual ~OrIterator(void)
	{
		OP_DELETE(m_a2);
		OP_DELETE(m_a1);
	}

	virtual BOOL Next(void)
	{
		BOOL n1, n2;

		if (End())
			return FALSE;

		if (m_a2 == NULL || m_a2->End())
			return m_a1->Next();
		if (m_a1 == NULL || m_a1->End())
			return m_a2->Next();

		if (Beginning())
		{
			n1 = FALSE;
			n2 = FALSE;

			if (m_a1 != NULL)
				n1 = m_a1->Next();
			if (m_a2 != NULL)
				n2 = m_a2->Next();

			return n1 || n2;
		}

		if (Compare(&m_a1->Get(), &m_a2->Get()))        // m_a1 < m_a2
			return m_a1->Next() || !m_a2->End();
		else if (!Compare(&m_a2->Get(), &m_a1->Get()))  // equal
		{
			n1 = m_a1->Next();
			n2 = m_a2->Next();
			return n1 || n2;
		}
		else                                          // m_a2 < m_a1
			return m_a2->Next() || !m_a1->End();
	}

	virtual BOOL Prev(void)
	{
		BOOL p1, p2;

		if (Beginning())
			return FALSE;

		if (m_a2 == NULL || m_a2->Beginning())
			return m_a1->Prev();
		if (m_a1 == NULL || m_a1->Beginning())
			return m_a2->Prev();

		if (End())
		{
			p1 = FALSE;
			p2 = FALSE;

			if (m_a1 != NULL)
				p1 = m_a1->Prev();
			if (m_a2 != NULL)
				p2 = m_a2->Prev();

			if (p1 && p2)
			{
				if (Compare(&m_a1->Get(), &m_a2->Get()))
					m_a1->Next();
				else
					m_a2->Next();
			}

			return p1 || p2;
		}

		// at least one of m_a1 and m_a2 is valid here
		if (m_a1 != NULL && m_a1->End())
		{
			if (!m_a2->Prev())
				return m_a1->Prev();

			if (!m_a1->Prev())
				return TRUE;

			if (!Compare(&m_a1->Get(), &m_a2->Get()))
			{
				m_a1->Next();
				return TRUE;
			}

			return TRUE;
		}
		if (m_a2 != NULL && m_a2->End())
		{
			if (!m_a1->Prev())
				return m_a2->Prev();

			if (!m_a2->Prev())
				return TRUE;

			if (!Compare(&m_a2->Get(), &m_a1->Get()))
			{
				m_a2->Next();
				return TRUE;
			}

			return TRUE;
		}

		if (Compare(&m_a1->Get(), &m_a2->Get()))        // m_a1 < m_a2
			return m_a2->Prev() || !m_a1->Beginning();
		else if (!Compare(&m_a2->Get(), &m_a1->Get()))  // equal
		{
			p1 = m_a1->Prev();
			p2 = m_a2->Prev();
			return p1 || p2;
		}
		else                                          // m_a2 < m_a1
			return m_a1->Prev() || !m_a2->Beginning();
	}

	virtual const T &Get(void)
	{
		OP_ASSERT(!End());
		OP_ASSERT(!Beginning());

		if (m_a2 == NULL || m_a2->End() || m_a2->Beginning())
			return m_a1->Get();
		if (m_a1 == NULL || m_a1->End() || m_a2->Beginning())
			return m_a2->Get();

		return Compare(&m_a1->Get(), &m_a2->Get()) ? m_a1->Get() : m_a2->Get();
	}

	CHECK_RESULT(virtual OP_STATUS Error(void) const)
	{
		register OP_STATUS err;

		if (m_a1 != NULL && OpStatus::IsError(err = m_a1->Error()))
			return err;

		return m_a2 == NULL ? OpStatus::OK : m_a2->Error();
	}

	virtual int Count(void) const
	{
		int c1, c2;

		if ((c1 = m_a1->Count()) == -1 || (c2 = m_a2->Count()) == -1)
			return -1;
		return c1 + c2;
	}

	virtual BOOL End(void) const {return (m_a1 == NULL || m_a1->End()) && (m_a2 == NULL || m_a2->End());}

	virtual BOOL Beginning(void) const {return (m_a1 == NULL || m_a1->Beginning()) && (m_a2 == NULL || m_a2->Beginning());}

protected:
	SearchIterator<T> *m_a1;
	SearchIterator<T> *m_a2;
	TypeDescriptor::ComparePtr Compare;
};

/**
 * @brief returns sorted results present in both other iterators
 */
template <typename T> class AndIterator : public SearchIterator<T>
{
public:

	AndIterator(TypeDescriptor::ComparePtr compare = NULL)
	{
		m_a1 = NULL;
		m_a2 = NULL;
		Compare = compare == NULL ? &DefDescriptor<T>::Compare : compare;
	}

	AndIterator(SearchIterator<T> *a1, SearchIterator<T> *a2, TypeDescriptor::ComparePtr compare = NULL)
	{
		OP_ASSERT(a1 != NULL);
		OP_ASSERT(a2 != NULL);
		m_a1 = a1;
		m_a2 = a2;
		Compare = compare == NULL ? &DefDescriptor<T>::Compare : compare;

		if (m_a1 != NULL && !m_a1->End() && m_a2 != NULL && !m_a2->End())
		{
			do {
				while (Compare(&m_a1->Get(), &m_a2->Get()))
					if (!m_a1->Next())
						return;

				while (Compare(&m_a2->Get(), &m_a1->Get()))
					if (!m_a2->Next())
						return;
			} while (Compare(&m_a1->Get(), &m_a2->Get()));
		}
	}

	void Set(SearchIterator<T> *a1, SearchIterator<T> *a2, TypeDescriptor::ComparePtr compare = NULL)
	{
		OP_ASSERT(a1 != NULL);
		OP_ASSERT(a2 != NULL);

		OP_DELETE(m_a1);
		OP_DELETE(m_a2);

		m_a1 = a1;
		m_a2 = a2;
		if (compare != NULL)
			Compare = compare;

		if (m_a1 != NULL && !m_a1->End() && m_a2 != NULL && !m_a2->End())
		{
			do {
				while (Compare(&m_a1->Get(), &m_a2->Get()))
					if (!m_a1->Next())
						return;

				while (Compare(&m_a2->Get(), &m_a1->Get()))
					if (!m_a2->Next())
						return;
			} while (Compare(&m_a1->Get(), &m_a2->Get()));
		}
	}

	virtual ~AndIterator(void)
	{
		OP_DELETE(m_a2);
		OP_DELETE(m_a1);
	}

	virtual BOOL Next(void)
	{
		if (m_a2->Beginning())
		{
			m_a2->Next();
			if (m_a2->Beginning())
				m_a2->Next();
		}
		else if (!m_a1->Next())
			return FALSE;

		if (End())
			return FALSE;

		do {
			while (Compare(&m_a1->Get(), &m_a2->Get()))
				if (!m_a1->Next())
					return FALSE;

			while (Compare(&m_a2->Get(), &m_a1->Get()))
				if (!m_a2->Next())
					return FALSE;
		} while (Compare(&m_a1->Get(), &m_a2->Get()));

		return TRUE;
	}

	virtual BOOL Prev(void)
	{
		if (m_a2->End())
		{
			m_a2->Prev();
			if (m_a2->End())
				m_a2->Prev();
		}
		else if (!m_a1->Prev())
			return FALSE;

		if (Beginning())
			return FALSE;

		do {
			while (Compare(&m_a1->Get(), &m_a2->Get()))
				if (!m_a2->Prev())
					return FALSE;

			while (Compare(&m_a2->Get(), &m_a1->Get()))
				if (!m_a1->Prev())
					return FALSE;
		} while (Compare(&m_a1->Get(), &m_a2->Get()));

		return TRUE;
	}

	virtual const T &Get(void)
	{
		OP_ASSERT(!End());
		OP_ASSERT(!Beginning());

		return m_a1->Get();
	}

	CHECK_RESULT(virtual OP_STATUS Error(void) const)
	{
		register OP_STATUS err;

		if (m_a1 != NULL && OpStatus::IsError(err = m_a1->Error()))
			return err;

		return m_a2 == NULL ? OpStatus::OK : m_a2->Error();
	}

	virtual int Count(void) const
	{
		int c1, c2;

		if ((c1 = m_a1->Count()) == -1 || (c2 = m_a2->Count()) == -1)
			return -1;
		return c1;
	}

	virtual BOOL End(void) const {return m_a1 == NULL || m_a1->End() || m_a2 == NULL || m_a2->End();}

	virtual BOOL Beginning(void) const {return m_a1 == NULL || m_a1->Beginning() || m_a2 == NULL || m_a2->Beginning();}

protected:
	SearchIterator<T> *m_a1;
	SearchIterator<T> *m_a2;
	TypeDescriptor::ComparePtr Compare;
};

/**
 * @brief returns sorted results only present in the first of the iterators
 */
template <typename T> class AndNotIterator : public SearchIterator<T>
{
public:

	AndNotIterator(TypeDescriptor::ComparePtr compare = NULL)
	{
		m_a1 = NULL;
		m_a2 = NULL;
		Compare = compare == NULL ? &DefDescriptor<T>::Compare : compare;
	}

	AndNotIterator(SearchIterator<T> *a1, SearchIterator<T> *a2, TypeDescriptor::ComparePtr compare = NULL)
	{
		OP_ASSERT(a1 != NULL);
		OP_ASSERT(a2 != NULL);
		m_a1 = a1;
		m_a2 = a2;
		Compare = compare == NULL ? &DefDescriptor<T>::Compare : compare;

		if (m_a1 == NULL || m_a1->End() || m_a2 == NULL || m_a2->End())
			return;

		for (;;) {
			while (Compare(&m_a2->Get(), &m_a1->Get()))
				if (!m_a2->Next())
					return;

			if (!Compare(&m_a1->Get(), &m_a2->Get()))
			{
				if (!m_a1->Next())
					return;
			}
			else return;
		}
	}

	void Set(SearchIterator<T> *a1, SearchIterator<T> *a2, TypeDescriptor::ComparePtr compare = NULL)
	{
		OP_ASSERT(a1 != NULL);
		OP_ASSERT(a2 != NULL);

		OP_DELETE(m_a1);
		OP_DELETE(m_a2);

		m_a1 = a1;
		m_a2 = a2;
		if (compare != NULL)
			Compare = compare;

		if (m_a1 == NULL || m_a1->End() || m_a2 == NULL || m_a2->End())
			return;

		for (;;) {
			while (Compare(&m_a2->Get(), &m_a1->Get()))
				if (!m_a2->Next())
					return;

			if (!Compare(&m_a1->Get(), &m_a2->Get()))
			{
				if (!m_a1->Next())
					return;
			}
			else return;
		}
	}

	virtual ~AndNotIterator(void)
	{
		OP_DELETE(m_a2);
		OP_DELETE(m_a1);
	}

	virtual BOOL Next(void)
	{
		if (End())
			return FALSE;

		if (m_a2 == NULL || m_a2->End())
			return m_a1->Next();

		if (m_a2->Beginning())
			m_a2->Next();

		do {
			if (!m_a1->Next())
				return FALSE;

			while (Compare(&m_a1->Get(), &m_a2->Get()))
				if (!m_a2->Prev())
					return TRUE;

			while (Compare(&m_a2->Get(), &m_a1->Get()))
				if (!m_a2->Next())
					return TRUE;

		} while (!Compare(&m_a1->Get(), &m_a2->Get()));

		return TRUE;
	}

	virtual BOOL Prev(void)
	{
		if (Beginning())
			return FALSE;

		if (m_a2 == NULL || m_a2->Beginning())
			return m_a1->Prev();

		if (m_a2->End())
			m_a2->Prev();

		do {
			if (!m_a1->Prev())
				return FALSE;

			while (Compare(&m_a2->Get(), &m_a1->Get()))
				if (!m_a2->Next())
					return TRUE;

			while (Compare(&m_a1->Get(), &m_a2->Get()))
				if (!m_a2->Prev())
					return TRUE;
		} while (!Compare(&m_a2->Get(), &m_a1->Get()));

		return TRUE;
	}

	virtual const T &Get(void)
	{
		OP_ASSERT(!End());
		OP_ASSERT(!Beginning());

		return m_a1->Get();
	}

	CHECK_RESULT(virtual OP_STATUS Error(void) const)
	{
		register OP_STATUS err;

		if (m_a1 != NULL && OpStatus::IsError(err = m_a1->Error()))
			return err;

		return m_a2 == NULL ? OpStatus::OK : m_a2->Error();
	}

	virtual int Count(void) const
	{
		return m_a1->Count();
	}

	virtual BOOL End(void) const {return m_a1 == NULL || m_a1->End();}

	virtual BOOL Beginning(void) const {return m_a1 == NULL || m_a1->Beginning();}

protected:
	SearchIterator<T> *m_a1;
	SearchIterator<T> *m_a2;
	TypeDescriptor::ComparePtr Compare;
};


/**
 * @brief A filter to use with FilterIterator
 */
template <typename T> class SearchFilter
{
public:
	virtual ~SearchFilter() {}

	/**
	 * @return TRUE if the filter would filter nothing, i.e. match everything
	 */
	virtual BOOL Empty() const { return FALSE; }

	/**
	 * @param item a search result to be tested
	 * @return TRUE if this SearchFilter matches the item
	 */
	virtual BOOL Matches(const T &item) const = 0;

	/**
	 * @return status of the last Matches operation
	 */
	CHECK_RESULT(virtual OP_STATUS Error(void) const) { return OpStatus::OK; }
};


/**
 * @brief A filtering SearchIterator that filters the output of a source iterator
 * based on a SearchFilter
 */
template <typename T> class FilterIterator : public SearchIterator<T>
{
public:
	/**
	 * Create a new FilterIterator with the given source iterator and search filter
	 * @param source_it The source iterator
	 * @param filter The search filter
	 */
	FilterIterator(SearchIterator<T> *source_it, SearchFilter<T> *filter)
	{
		OP_ASSERT(source_it != NULL);
		OP_ASSERT(filter != NULL);

		m_source_it = source_it;
		m_filter = filter;
		if (m_filter != NULL && m_filter->Empty())
		{
			OP_DELETE(m_filter);
			m_filter = NULL;
		}

		if (m_filter != NULL && !End())
			if (!m_filter->Matches(m_source_it->Get()))
				Next();
		m_empty = End();
	}

	virtual ~FilterIterator(void)
	{
		OP_DELETE(m_source_it);
		OP_DELETE(m_filter);
	}

	virtual BOOL Next(void)
	{
		if (End())
			return FALSE;
		
		while (m_source_it->Next())
			if (m_filter == NULL || m_filter->Matches(m_source_it->Get()))
				return TRUE;

		return FALSE;
	}

	virtual BOOL Prev(void)
	{
		if (Beginning())
			return FALSE;

		while (m_source_it->Prev())
			if (m_filter == NULL || m_filter->Matches(m_source_it->Get()))
				return TRUE;

		return FALSE;
	}

	virtual const T &Get(void)
	{
		OP_ASSERT(!End());
		OP_ASSERT(!Beginning());

		return m_source_it->Get();
	}

	CHECK_RESULT(virtual OP_STATUS Error(void) const)
	{
		if (m_source_it != NULL)
			RETURN_IF_ERROR(m_source_it->Error());
		if (m_filter != NULL)
			RETURN_IF_ERROR(m_filter->Error());
		return OpStatus::OK;
	}

	virtual int Count(void) const
	{
		return m_empty ? 0 : m_source_it->Count();
	}

	virtual BOOL End(void) const {return m_source_it == NULL || m_source_it->End();}

	virtual BOOL Beginning(void) const {return m_source_it == NULL || m_source_it->Beginning();}

protected:
	SearchIterator<T> *m_source_it;
	SearchFilter<T> *m_filter;
	BOOL m_empty;
};


template <typename T> class EmptyIterator : public SearchIterator<T>
{
public:
	virtual BOOL Next(void) {return FALSE;}
	virtual BOOL Prev(void) {return FALSE;}

	virtual const T &Get(void) {OP_ASSERT(0); return *(T *)NULL;}

	CHECK_RESULT(virtual OP_STATUS Error(void) const) {return OpStatus::OK;}

	virtual int Count(void) const {return 0;}
	virtual BOOL End(void) const {return TRUE;}
	virtual BOOL Beginning(void) const {return TRUE;}
};

/**
 * type of searches
 */
enum SearchOperator
{
	operatorLT,  /**< find all results less than the given value */
	operatorLE,  /**< find all results less than or equal to the given value */
	operatorEQ,  /**< find all results equal to the given value */
	operatorGE,  /**< find all results greater than or equal the given value */
	operatorGT   /**< find all results greater than the given value */
};


#endif  // RESULTBASE_H

