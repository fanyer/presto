//
// Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)
//

#ifndef MESSAGE_SET_H
#define MESSAGE_SET_H

#include "adjunct/desktop_util/adt/opsortedvector.h"

namespace ImapCommands
{
	class MessageSet
	{
	public:
		/** Constructor
		  * @param start Start of the first range, start > 0
		  * @param end End of the first range, start < end or end == 0 to specify last message in mailbox ('*')
		  */
		MessageSet(UINT32 start = 0, UINT32 end = 0);

		/** Get the sequence as a string that conforms to the IMAP spec
		  * @param string Output string
		  */
		OP_STATUS GetString(OpString8& string);

		/** Add all the message ids in this sequence to vector, won't add if sequence contains infinite set
		  * @param vector Vector to add message ids to
		  */
		OP_STATUS ToVector(OpINT32Vector& vector) const;

		/** Insert range [start .. end] into this sequence
		  * @param start Start of the range, start > 0
		  * @param end End of the range, start < end or end == 0 to specify last message in mailbox ('*')
		  */
		OP_STATUS InsertRange(UINT32 start, UINT32 end);

		/** Copy another message set
		  * @param copy_from
		  */
		OP_STATUS Copy(const MessageSet& copy_from);

		/** Merge with another message set
		  * @param Message set to merge from
		  */
		OP_STATUS Merge(const MessageSet& merge_from);

		/** Insert a sequence specified as integer vector
		  * @param sequence Sequence to insert into this sequence
		  */
		OP_STATUS InsertSequence(const OpINT32Vector& sequence);

		/** Checks whether this sequence contains a certain id
		  * @param id The ID to check for
		  * @return Whether this sequence contains id
		  */
		BOOL	  Contains(UINT32 id) const;

		/** Checks whether this sequence contains all messages
		  * @return Whether this sequence contains all messages
		  */
		BOOL	  ContainsAll() const;

		/** Count the number of messages in this set
		  */
		unsigned  Count() const { return m_count; }

		/** Check for a specified amount of messages
		  * @param x Number to compare
		  * @return Whether this set contains more than x messages, and returns FALSE if infinite
		  */
		BOOL	  ContainsMoreThan(unsigned x) const { return m_count > x; }

		/** Split off a number of ids in this sequence and give the remainder
		  * @param to_split_off Number of ids to split off
		  * @param remainder Where to put the remainder after splitting off to_split_off ids
		  */
		OP_STATUS SplitOff(unsigned to_split_off, MessageSet& remainder);

		/** Number of ranges in the set
		  */
		unsigned  RangeCount() const { return m_ranges.GetCount(); }

		/** Split off a number of ranges in this sequence and give the remainder
		  * @param to_split_off Number of ranges to split off, needs to be lower than the current count. check with ContainsMoreThan first
		  * @param remainder Remainder after splitting off to_split_off ranges
		  */
		OP_STATUS SplitOffRanges(unsigned to_split_off, MessageSet& remainder);

		/** Expunge an id from this sequence
		  *   if the id is in this sequence, it will be removed. All ids higher than this id will be lowered by one.
		  * @param id id to expunge
		  */
		OP_STATUS Expunge(UINT32 id);

		/** Check whether this is an invalid sequence
		  * @return Whether this is an invalid sequence
		  */
		BOOL	  IsInvalid() const { return m_ranges.GetCount() == 0; }

	private:
		struct Range
		{
			Range(UINT32 p_start, UINT32 p_end) : start(p_start), end(p_end) {}

			UINT32 start; ///< sequence numbers or unique ids are positive, 32-bit numbers
			UINT32 end;

			BOOL operator<(const Range& other_range) const { return start < other_range.start; }
		};

		// No copy constructor
		MessageSet(const MessageSet& copy_from) {}

		BOOL	  Extend(INT32 index, UINT32 start, UINT32 end, BOOL merge = TRUE);
		void	  MergeDown(INT32 index);
		void	  MergeUp(INT32 index);

		OpString8 m_temp_string;

		OpAutoSortedVector<Range> m_ranges;

		unsigned  m_count; // cache of the message count in this messageset
	};
};

#endif // MESSAGE_SET_H
