/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/backend/imap/commands/MessageSet.h"

// Max size of a positive INT32 string
#define UINT_STRING_SIZE 10


/***********************************************************************************
 ** Constructor
 **
 ** ImapCommands::MessageSet::MessageSet
 ***********************************************************************************/
ImapCommands::MessageSet::MessageSet(UINT32 start, UINT32 end)
	: m_count(0)
{
	OP_ASSERT(start == 0 || end == 0 || start <= end);

	if (start > 0)
		OpStatus::Ignore(InsertRange(start, end));
};


/***********************************************************************************
 ** Get the sequence as a string that conforms to the IMAP spec
 **
 ** ImapCommands::MessageSet::GetString
 ** @param string Output string
 ***********************************************************************************/
OP_STATUS ImapCommands::MessageSet::GetString(OpString8& string)
{
	// Length of string: a range is max two positive integers and two characters
	size_t string_length = (UINT_STRING_SIZE * 2 + 2) * m_ranges.GetCount();

	// Reserve the space for the string
	char* string_ptr = string.Reserve(string_length + 1);
	RETURN_OOM_IF_NULL(string_ptr);

	// Create the string
	for (unsigned i = 0; i < m_ranges.GetCount(); i++)
	{
		Range* range = m_ranges.GetByIndex(i);

		if (range->end == 0)
			sprintf(string_ptr, "%s%u:*", i != 0 ? "," : "", range->start);
		else if (range->end == range->start)
			sprintf(string_ptr, "%s%u", i != 0 ? "," : "", range->start);
		else
			sprintf(string_ptr, "%s%u:%u", i != 0 ? "," : "", range->start, range->end);

		string_ptr += strlen(string_ptr);

		if (string_ptr - string.CStr() > (ptrdiff_t)string_length)
		{
			OP_ASSERT(!"This can't be happening!");
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Add all the message ids in this sequence to vector, won't add if sequence
 ** contains infinite set
 **
 ** ImapCommands::MessageSet::ToVector
 ** @param vector Vector to add message ids to
 ***********************************************************************************/
OP_STATUS ImapCommands::MessageSet::ToVector(OpINT32Vector & vector) const
{
	for (unsigned i = 0; i < m_ranges.GetCount(); i++)
	{
		Range* range = m_ranges.GetByIndex(i);

		if (range->end == 0)
			return OpStatus::OK;

		for (unsigned j = range->start; j <= range->end; j++)
			RETURN_IF_ERROR(vector.Add(j));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Insert range [start .. end] into this sequence
 **
 ** ImapCommands::MessageSet::InsertRange
 **
 ***********************************************************************************/
OP_STATUS ImapCommands::MessageSet::InsertRange(UINT32 start, UINT32 end)
{
	// Check for invalid range
	if (start == 0 || (end > 0 && start > end))
		return OpStatus::ERR;

	OpAutoPtr<Range> range (OP_NEW(Range, (start, end)));
	if (!range.get())
		return OpStatus::ERR_NO_MEMORY;

	// Find the nearest range
	INT32 nearest = m_ranges.FindInsertPosition(range.get());

	// Only add the range if extending existing ranges fails
	if (!Extend(nearest    , start, end) &&
		!Extend(nearest - 1, start, end))
	{
		// Couldn't extend existing ranges, insert the range
		RETURN_IF_ERROR(m_ranges.Insert(range.get()));
		if (end > 0)
			m_count += end - start + 1;
		range.release();
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Copy
 **
 ** ImapCommands::MessageSet::Copy
 ***********************************************************************************/
OP_STATUS ImapCommands::MessageSet::Copy(const MessageSet& copy_from)
{
	for (unsigned i = 0; i < copy_from.m_ranges.GetCount(); i++)
	{
		OpAutoPtr<Range> range (OP_NEW(Range, (*copy_from.m_ranges.GetByIndex(i))));
		if (!range.get())
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(m_ranges.Insert(range.get()));
		range.release();
	}

	m_count += copy_from.Count();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Merge with another message set
 **
 ** ImapCommands::MessageSet::Merge
 ***********************************************************************************/
OP_STATUS ImapCommands::MessageSet::Merge(const MessageSet& merge_from)
{
	for (unsigned i = 0; i < merge_from.m_ranges.GetCount(); i++)
	{
		Range* range = merge_from.m_ranges.GetByIndex(i);

		RETURN_IF_ERROR(InsertRange(range->start, range->end));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Insert a sequence specified as integer vector
 **
 ** ImapCommands::MessageSet::InsertSequence
 ** @param sequence Sequence to insert into this sequence
 ***********************************************************************************/
OP_STATUS ImapCommands::MessageSet::InsertSequence(const OpINT32Vector& sequence)
{
	for (UINT32 i = 0; i < sequence.GetCount(); i++)
	{
		RETURN_IF_ERROR(InsertRange(sequence.Get(i), sequence.Get(i)));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Checks whether this sequence contains a certain id
 **
 ** ImapCommands::MessageSet::Contains
 ** @param id The ID to check for
 ** @return Whether this sequence contains id
 ***********************************************************************************/
BOOL ImapCommands::MessageSet::Contains(UINT32 id) const
{
	// Try to find the closest range
	Range range(id, id);
	int   index = m_ranges.FindInsertPosition(&range);

	for (unsigned i = max(0, index - 1); (int)i <= index && i < m_ranges.GetCount(); i++)
	{
		Range* found_range = m_ranges.GetByIndex(i);

		if (id >= found_range->start && (id <= found_range->end || found_range->end == 0))
			return TRUE;
	}

	return FALSE;
}


/***********************************************************************************
 ** Checks whether this sequence contains all messages
 **
 ** ImapCommands::MessageSet::ContainsAll
 ***********************************************************************************/
BOOL ImapCommands::MessageSet::ContainsAll() const
{
	// Contains all messages if the only range is 1:0 (1:*)
	return m_ranges.GetCount() > 0 && m_ranges.GetByIndex(0)->start == 1 && m_ranges.GetByIndex(0)->end == 0;
}

/***********************************************************************************
 ** Split off a number of ids in this sequence and give the remainder
 **
 ** ImapCommands::MessageSet::SplitOff
 ** @param to_split_off Number of ids to split off
 ** @param remainder Where to put the remainder after splitting off to_split_off ids
 ** @return Whether any messages were actually split off
 ***********************************************************************************/
OP_STATUS ImapCommands::MessageSet::SplitOff(unsigned to_split_off, ImapCommands::MessageSet& remainder)
{
	unsigned count = 0;
	unsigned index = 0;
	Range*   range = NULL;

	// Find the range where the split should happen
	for (; index < m_ranges.GetCount(); index++)
	{
		range = m_ranges.GetByIndex(index);

		// Can't split infinite ranges
		if (range->end == 0)
			return OpStatus::OK;

		// Check if this is the range that should split
		if (count + 1 + (range->end - range->start) > to_split_off)
			break;

		count += range->end + 1 - range->start;
	}

	// Check if we found a range to split
	if (!range)
		return OpStatus::OK;

	// Check if the range itself should be split
	if (to_split_off - count > 0)
	{
		// Split the range that we found
		RETURN_IF_ERROR(remainder.InsertRange(range->start + (to_split_off - count), range->end));

		range->end = range->start + (to_split_off - count) - 1;

		index++;
	}

	// Add the remaining ranges ( [index .. m_ranges.GetCount()) ) to the remainder
	RETURN_IF_ERROR(SplitOffRanges(index, remainder));
	
	m_count = to_split_off;

	return OpStatus::OK;
}


/***********************************************************************************
 ** Split off a number of ranges in this sequence and give the remainder
 **
 ** ImapCommands::MessageSet::SplitOffRanges
 ** @param to_split_off Number of ranges to split off
 ** @param remainder Remainder after splitting off to_split_off ranges
 ***********************************************************************************/
OP_STATUS ImapCommands::MessageSet::SplitOffRanges(unsigned to_split_off, ImapCommands::MessageSet& remainder)
{
	// Add the remaining ranges ( [to_split_off .. m_ranges.GetCount()) ) to the remainder
	for (unsigned i = to_split_off; i < m_ranges.GetCount(); i++)
	{
		RETURN_IF_ERROR(remainder.InsertRange(m_ranges.GetByIndex(i)->start, m_ranges.GetByIndex(i)->end));
		OP_DELETE(m_ranges.GetByIndex(i));
	}

	// Remove the remaining ranges from this set
	m_ranges.RemoveByIndex(to_split_off, m_ranges.GetCount() - to_split_off);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Expunge an id from this sequence
 **
 ** ImapCommands::MessageSet::Expunge
 ** @param id id to expunge
 **
 ***********************************************************************************/
OP_STATUS ImapCommands::MessageSet::Expunge(UINT32 id)
{
	// Remove from relevant ranges
	Range	 range(id, id);
	int		 bottom = max(0, (int)m_ranges.FindInsertPosition(&range) - 1);

	for (int i = m_ranges.GetCount() - 1; i >= bottom; i--)
	{
		Range* found_range = m_ranges.GetByIndex(i);

		if (found_range->start <= id && found_range->end >= id)
			m_count--;

		// Decrease the start of the range when a server ID is expunged
		// We don't decrease if start == id, since we'd now want the next element as the start
		if (found_range->start > id)
			found_range->start--;

		// We decrease the end even if it's equal to the id, since we don't want that id anymore
		// in the range
		if (found_range->end >= id)
		{
			found_range->end--;

			// If the range is now invalid, remove it
			if (found_range->end < found_range->start)
				m_ranges.Delete(found_range);
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Extend a range
 **
 ** ImapCommands::MessageSet::Extend
 ** @param merge Whether to try to merge the extended range with other ranges
 ** @return Whether the range at index could be extended with [start .. end]
 ***********************************************************************************/
BOOL ImapCommands::MessageSet::Extend(INT32 index, UINT32 start, UINT32 end, BOOL merge)
{
	if (index < 0 || index >= (int)m_ranges.GetCount())
		return FALSE;

	Range* range = m_ranges.GetByIndex(index);

	// 'range' is extendable with values in extension if and only if
	//    (range->start - 1 <= end || end == 0) && (start <= range->end + 1 || range->end == 0)
	if ((range->start - 1 > end && end > 0) || (start > range->end + 1 && range->end > 0))
		return FALSE;

	// We can extend this range. Extend start or end?
	if (start < range->start)
	{
		m_count += range->start - start;
		range->start = start;
		if (merge)
			MergeDown(index);
	}
	if (end > range->end && range->end > 0)
	{
		m_count += end - range->end;
		range->end = end;
		if (merge)
			MergeUp(index);
	}

	return TRUE;
}


/***********************************************************************************
 ** Try to merge ranges down from index
 **
 ** ImapCommands::MessageSet::Merge
 ***********************************************************************************/
void ImapCommands::MessageSet::MergeDown(INT32 index)
{
	for (INT32 merge_index = index - 1; merge_index >= 0; merge_index--)
	{
		// Check if ranges can be merged with the range at 'index'
		Range* range = m_ranges.GetByIndex(merge_index);
		if (!Extend(index, range->start, range->end, FALSE))
			break;

		m_count -= range->end - range->start + 1;
		// Range was merged, remove
		OP_DELETE(m_ranges.RemoveByIndex(merge_index));
		index--;
	}
}


/***********************************************************************************
 ** Try to merge ranges up from index
 **
 ** ImapCommands::MessageSet::MergeUp
 ***********************************************************************************/
void ImapCommands::MessageSet::MergeUp(INT32 index)
{
	while (index + 1 < (int)m_ranges.GetCount())
	{
		// Check if ranges can be merged with the range at 'index'
		Range* range = m_ranges.GetByIndex(index + 1);
		if (!Extend(index, range->start, range->end, FALSE))
			break;

		m_count -= range->end - range->start + 1;
		// Range was merged, remove
		OP_DELETE(m_ranges.RemoveByIndex(index + 1));
	}
}


#endif // M2_SUPPORT
