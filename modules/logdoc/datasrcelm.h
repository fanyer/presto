/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DATA_SRC_ELM_H
#define DATA_SRC_ELM_H

#include "modules/util/simset.h"
#include "modules/logdoc/complex_attr.h"
#include "modules/hardcore/mem/mem_man.h"

/// Macro used to allocate DataSrcElms.
#define NEW_DataSrcElm(x)   (DataSrcElm::Create x ) // FIXME:REPORT_MEMMAN-KILSMO
/// Macro used to deallocate DataSrcElms.
#define DELETE_DataSrcElm(e)    OP_DELETE(e) // FIXME:REPORT_MEMMAN-KILSMO

class DataSrc;
class DataSrcElm;

/// Used to store CDATA sections for SCRIPT, STYLE and the like.
class DataSrcElm
  : public ListElement<DataSrcElm>
{
private:

	/// Pointer to the data buffer. Need not be null terminated.
	uni_char*		m_src;
	/// Length of the data in the buffer.
	int				m_src_len;
	/// TRUE if m_src is owned by us.
	BOOL			m_owns_src;

	DataSrcElm(int val_len, BOOL owns_src) : m_src(NULL), m_src_len(val_len), m_owns_src(owns_src) {}
	virtual ~DataSrcElm();

	static DataSrcElm* Create(const uni_char* val, int val_len, BOOL copy = TRUE);
public:

	uni_char*		GetSrc() { return m_src; };
	int				GetSrcLen() { return m_src_len; };

	friend class DataSrc;
};

class DataSrc : public ComplexAttr
{
private:
	URL m_origin;
	List<DataSrcElm> m_list;
public:
	DataSrc() {}
	virtual ~DataSrc() { DeleteAll(); }

	/**
	 * Adds DataSrcElms to store the new source.
	 */
	OP_STATUS AddSrc(const uni_char* src, int src_len, const URL& origin, BOOL copy = TRUE);

	/**
	 * Sets the origin url for an empty resource.
	 */
	void SetOriginURL(const URL& origin) { m_origin = origin; }

	/**
	 * Deletes all content, leaving a completely empty DataSrc
	 */
	void DeleteAll() { m_list.Clear(); m_origin = URL(); }

	/**
	 * The origin of the data. Empty URL if it's from the current document.
	 */
	const URL& GetOrigin() { return m_origin; }

	/**
	 * The first chunk of data in the source. Use Suc() to get more of it.
	 */
	DataSrcElm* First() { return m_list.First(); }

	/**
	 * Checks if the list of DataSrcElms is empty.
	 */
	BOOL IsEmpty() { return m_list.First() == NULL; }

	/**
	 * Moves the contents of this DataSrc to |other|.
	 * @param[inout] other The object that will receive all of this contents.
	 */
	void MoveTo(DataSrc* other) { OP_ASSERT(other != this || !"Hang"); while (DataSrcElm* elm = m_list.First()) { elm->Out(); elm->Into(&other->m_list);} other->m_origin = m_origin; }

	/**
	 * Counts the number of DataSrcElm chunks this DataSrc contains.
	 */
	unsigned GetDataSrcElmCount() { return m_list.Cardinal(); }

	// From ComplexAttr interface
	virtual OP_STATUS	CreateCopy(ComplexAttr **copy_to);
};

#endif // DATA_SRC_ELM_H
