/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIA_TIME_RANGES_H
#define MEDIA_TIME_RANGES_H

#include "modules/pi/OpMediaPlayer.h"

/** Contains a list of time ranges.
 *
 * This class is useful if we need to maintain a copy of the time ranges
 * provided by the platform interface.
 */
class MediaTimeRanges
	: public OpMediaTimeRanges
{
public:

	MediaTimeRanges();
	virtual ~MediaTimeRanges();

	/** Copy the specified ranges into this object.
	 *
	 * @param ranges The ranges to copy.
	 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS SetRanges(const OpMediaTimeRanges* ranges);

	/** Check if the collection of time ranges is normalized.
	 *
	 * A normalized MediaTimeRanges is one where no ranges overlap,
	 * start < end for each range, and where the start times are
	 * sorted by increasing order.
	 *
	 * @return true if normalized, false if not.
	 */
	bool IsNormalized();
	void Normalize();

	/** Add a time range, while keeping the ranges normalized.
	 *
	 * If the current ranges are not normalized, they are
	 * normalized before the new range is added.
	 *
	 * @param start Start of time range.
	 * @param end End of time range.
	 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS AddRange(double start, double end);

	// OpMediaTimeRanges
	virtual UINT32 Length() const;
	virtual double Start(UINT32 idx) const;
	virtual double End(UINT32 idx) const;

private:

	/** Represents a time range (in seconds), where @c start <= @c end. */
	struct Range
	{
		double start;
		double end;
		bool IsEmpty() { return start >= end; }
	};

	/** Merge overlapping intervals, assuming start times are already sorted. */
	void Merge();

	Range* m_ranges;

	UINT32 m_allocated;
	UINT32 m_used;
};

#endif // MEDIA_TIME_RANGES_H
