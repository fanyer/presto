/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIABYTERANGE_H
#define MEDIABYTERANGE_H

#ifdef MEDIA_PLAYER_SUPPORT

/** Byte range with start and end bytes inclusive.
 *
 * The semantics are similar to HTTP byte ranges.
 */
class MediaByteRange
{
public:
	MediaByteRange(OpFileLength start = FILE_LENGTH_NONE, OpFileLength end = FILE_LENGTH_NONE) :
		start(start), end(end)
	{
		OP_ASSERT(IsEmpty() || !IsFinite() || start <= end);
	}
	BOOL IsEmpty() const { return start == FILE_LENGTH_NONE; }
	BOOL IsFinite() const { return end != FILE_LENGTH_NONE; }
	OpFileLength Length() const { OP_ASSERT(IsFinite()); return end - start + 1; }
	void SetLength(OpFileLength length)
	{
		OP_ASSERT(!IsEmpty());
		if (length == FILE_LENGTH_NONE)
			end = FILE_LENGTH_NONE;
		else if (length > 0)
			end = start + length - 1;
		else // length == 0
			start = end = FILE_LENGTH_NONE;
	}
	void IntersectWith(const MediaByteRange& other)
	{
		if (!IsEmpty() && !other.IsEmpty())
		{
			start = MAX(start, other.start);
			if (!IsFinite())
				end = other.end;
			else if (other.IsFinite())
				end = MIN(end, other.end);
			if (!IsFinite() || start <= end)
				return;
		}
		start = end = FILE_LENGTH_NONE;
	}
	void UnionWith(const MediaByteRange& other)
	{
		if (IsEmpty())
		{
			start = other.start;
			end = other.end;
		}
		else if (!other.IsEmpty())
		{
			start = MIN(start, other.start);
			if (!other.IsFinite())
				end = FILE_LENGTH_NONE;
			else if (IsFinite())
				end = MAX(end, other.end);
		}
	}
	OpFileLength start;
	OpFileLength end;
};

#endif // MEDIA_PLAYER_SUPPORT

#endif // MEDIABYTERANGE_H
