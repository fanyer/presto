/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/logdoc/datasrcelm.h"
#include "modules/util/str.h"

DataSrcElm* DataSrcElm::Create(const uni_char* val, int val_len, BOOL copy)
{
	DataSrcElm* de = OP_NEW(DataSrcElm, (val_len, copy));
	if (!de)
		return NULL;

	if (copy)
	{
		de->m_src = NULL;
		if (val)
		{
			de->m_src = OP_NEWA(uni_char, val_len);
			if (de->m_src)
			{
				op_memcpy(de->m_src, val, val_len * sizeof(uni_char) );  // Must not use uni_strncpy, data can contain NULs

				REPORT_MEMMAN_INC(val_len * sizeof(uni_char) + sizeof(DataSrcElm));
			}
		}
	}
	else
		de->m_src = const_cast<uni_char*>(val);

	if (!de->m_src)
	{
		OP_DELETE(de);
		return NULL;
	}

	return de;
}

DataSrcElm::~DataSrcElm()
{
	if (m_src && m_owns_src)
	{
		REPORT_MEMMAN_DEC(m_src_len * sizeof(uni_char) + sizeof(DataSrcElm));

		OP_DELETEA(m_src);
	}
}

/*static*/ OP_STATUS
DataSrc::AddSrc(const uni_char* src, int src_len, const URL& origin, BOOL copy /* = TRUE */)
{
	if (!First())
		m_origin = origin;
	else
		OP_ASSERT(m_origin == origin); // Or we mix data from different sources

	DataSrcElm *s = DataSrcElm::Create(src, src_len, copy);
	if (!s)
		return OpStatus::ERR_NO_MEMORY;

	s->Into(&m_list);
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DataSrc::CreateCopy(ComplexAttr **copy_to)
{
	// Just create the shell, the clone will have to fill it itself
	DataSrc* clone = OP_NEW(DataSrc, ());
	if (!clone)
		return OpStatus::ERR_NO_MEMORY;
	*copy_to = clone;
	return OpStatus::OK;
}

