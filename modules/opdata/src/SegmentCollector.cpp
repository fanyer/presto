/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#include "modules/opdata/src/SegmentCollector.h"
#include "modules/opdata/UniString.h" // Need UniString(const OpData& d) and IsUniCharAligned()


/* virtual */
OP_STATUS ReplaceConcatenator::AddSegment(const OpData& segment)
{
	if (m_data.IsEmpty())
		return AddData(segment);
	else
	{
		RETURN_IF_ERROR(AddData(m_replacement));
		m_count++;
		return AddData(segment);
	}
}

void ReplaceConcatenator::Done()
{
	if (m_remaining_length)
	{
		OP_ASSERT(m_data.Length() >= m_remaining_length);
		m_data.Trunc(m_data.Length() - m_remaining_length);
	}
}

OP_STATUS ReplaceConcatenator::AddData(const OpData& data)
{
	if (m_data.IsEmpty() && m_remaining_length)
	{
		OP_ASSERT(m_current_ptr == NULL);
		m_current_ptr = m_data.GetAppendPtr(m_remaining_length);
		RETURN_OOM_IF_NULL(m_current_ptr);
	}
	if (m_remaining_length)
	{
		OP_ASSERT(m_current_ptr);
		size_t copied = data.CopyInto(m_current_ptr, m_remaining_length);
		OP_ASSERT(copied <= m_remaining_length);
		m_remaining_length -= copied;
		if (m_remaining_length == 0)
			m_current_ptr = NULL;
		else
			m_current_ptr += copied;
		if (data.Length() > copied)
			return m_data.Append(OpData(data, copied));
		else
			return OpStatus::OK;
	}
	else
		return m_data.Append(data);
}

/* virtual */ OP_STATUS
CountedUniStringCollector::AddSegment(const OpData& segment)
{
	OP_ASSERT(UniString::IsUniCharAligned(segment.Length()));
	return m_list->Append(UniString(segment));
}
