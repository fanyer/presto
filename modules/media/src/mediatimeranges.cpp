/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/media/mediatimeranges.h"

MediaTimeRanges::MediaTimeRanges()
	: m_ranges(NULL)
	, m_allocated(0)
	, m_used(0)
{
}

/* virtual */
MediaTimeRanges::~MediaTimeRanges()
{
	op_free(m_ranges);
}

/* virtual */ OP_STATUS
MediaTimeRanges::SetRanges(const OpMediaTimeRanges* ranges)
{
	OP_ASSERT(ranges);

	UINT32 length = ranges->Length();

	// Resize the array if required.
	if (m_allocated < length)
	{
		void* mem = op_realloc(m_ranges, sizeof(Range) * length);
		RETURN_OOM_IF_NULL(mem);

		m_ranges = static_cast<Range*>(mem);
		m_allocated = length;
	}

	for (UINT32 i = 0; i < length; i++)
	{
		m_ranges[i].start = ranges->Start(i);
		m_ranges[i].end = ranges->End(i);
	}

	m_used = length;

	return OpStatus::OK;
}

void MediaTimeRanges::Merge()
{
	for (UINT32 i = 0; i + 1 < m_used; i++)
	{
		Range& cur = m_ranges[i];
		Range& next = m_ranges[i + 1];

		if (cur.end >= next.start)
		{
			if (cur.end < next.end)
				cur.end = next.end;

			op_memmove(&m_ranges[i + 1], &m_ranges[i + 2], (m_used - i - 2) * sizeof(Range));
			i--;
			m_used--;
		}
	}
}

bool MediaTimeRanges::IsNormalized()
{
	for (UINT32 i = 0; i < m_used; i++)
	{
		if (m_ranges[i].start >= m_ranges[i].end)
			return false;
		if (i + 1 < m_used && m_ranges[i].end >= m_ranges[i + 1].start)
			return false;
	}

	return true;
}

void MediaTimeRanges::Normalize()
{
	// Throw out empty ranges.
	UINT32 in = 0, out = 0;
	while (in < m_used)
	{
		if (!m_ranges[in].IsEmpty())
			m_ranges[out++] = m_ranges[in];
		in++;
	}
	m_used = out;

	// Insertion sort on range start times.
	for (UINT32 i = 1; i < m_used; i++)
	{
		UINT32 j;
		Range cur = m_ranges[i];

		for (j = i; j > 0 && cur.start < m_ranges[j - 1].start; j--)
			m_ranges[j] = m_ranges[j - 1];

		m_ranges[j] = cur;
	}

	Merge();
}

OP_STATUS MediaTimeRanges::AddRange(double start, double end)
{
	if (start >= end)
		return OpStatus::OK;

	if (!IsNormalized())
		Normalize();

	if (m_used >= m_allocated)
	{
		// Allocate more than one at a time?
		void* mem = op_realloc(m_ranges, sizeof(Range) * (m_used + 1));
		RETURN_OOM_IF_NULL(mem);

		m_ranges = static_cast<Range*>(mem);
		m_allocated++;
	}

	// Insert into an already normalized list.
	UINT32 idx;
	for (idx = 0; idx < m_used; idx++)
		if (start < m_ranges[idx].start)
			break;
	op_memmove(&m_ranges[idx + 1], &m_ranges[idx], (m_used - idx) * sizeof(Range));
	m_ranges[idx].start = start;
	m_ranges[idx].end = end;
	m_used++;

	Merge();

	return OpStatus::OK;
}

/* virtual */ UINT32
MediaTimeRanges::Length() const
{
	return m_used;
}

/* virtual */ double
MediaTimeRanges::Start(UINT32 idx) const
{
	OP_ASSERT(idx < m_used);
	return m_ranges[idx].start;
}

/* virtual */ double
MediaTimeRanges::End(UINT32 idx) const
{
	OP_ASSERT(idx < m_used);
	return m_ranges[idx].end;
}
