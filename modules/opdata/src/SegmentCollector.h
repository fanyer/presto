/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_OPDATA_SEGMENTCOLLECTOR_H
#define MODULES_OPDATA_SEGMENTCOLLECTOR_H

/** @file
 *
 * @brief SegmentCollector and derived classes - collecting OpData segments.
 *
 * This code is an internal/private part of the opdata module. Please do not
 * \#include this file from outside modules/opdata.
 *
 * This file contains the SegmentCollector base class, used by OpData::Split()
 * to collect the OpData segments resulting from the splitting process. The
 * implementations of SegmentCollector (also found in this file) are
 * instantiated in various Split() and Remove() methods in OpData and UniString.
 *
 * See OpData.h and UniString.h for more extensive design notes on OpData and
 * UniString, respectively.
 */


/**
 * Helper base class for processing OpData segments from a split.
 *
 * Implement #AddSegment() in a subclass, and pass an instance of that subclass
 * to Split().  The instance's #AddSegment() will be invoked once for each
 * segment produced by the splitting process.
 */
class SegmentCollector
{
public:
	/**
	 * Add the given OpData as the next segment produced by Split().
	 *
	 * Called by Split(sepf, coll, ...) for each segment (OpData object)
	 * identified by the splitting process.
	 *
	 * This method can do whatever it wants with the given OpData object.
	 * Typical use cases include appending (some or all of) the OpData
	 * object into some sort of list, or merely counting the number of
	 * segments (by counting the number of calls to #AddSegment()).
	 *
	 * If an error is returned, the split process will be aborted, and the
	 * OP_STATUS propagated.
	 */
	virtual OP_STATUS AddSegment(const OpData& segment) = 0;
};

/// The simple OpData-appending collector used by OpData::Remove().
class Concatenator : public SegmentCollector
{
public:
	Concatenator(OpData& d) : m_data(d) {}

	virtual OP_STATUS AddSegment(const OpData& segment)
	{
		return m_data.Append(segment);
	}

private:
	OpData& m_data; ///< OpData segments are Append()ed here.
};

/** An OpData-appending collector used by OpData::Replace().
 *
 * This collector inserts the replacement block between all segments that are
 * added via AddSegment().
 *
 * If the \c expected_length is specified correctly in the constructor, the
 * resulting OpData will use a single chunk of that length. If the \c
 * expected_length is 0 or too short, the resulting OpData may have several
 * fragments. If the \c expected_length is specified, the caller should call
 * Done() after all segments are added.
 */
class ReplaceConcatenator : public SegmentCollector
{
public:
	ReplaceConcatenator(OpData& d, OpData replacement, size_t expected_length=0)
		: m_data(d), m_replacement(replacement)
		, m_current_ptr(NULL), m_remaining_length(expected_length), m_count(0)
	{}

	virtual OP_STATUS AddSegment(const OpData& segment);

	/** Finalises the collect operation.
	 * If an \c epxected_length greater than 0 was specified in the constructor,
	 * then we need to ensure that the output data is truncated if the actual
	 * length is lower than the expected length. */
	void Done();

	/** Returns the number of replacements. */
	size_t CountReplacement() const { return m_count; }

private:
	OP_STATUS AddData(const OpData& data);

	OpData& m_data; ///< OpData segments are Append()ed here.
	OpData m_replacement;	///< Data to insert between all segments.
	char* m_current_ptr;	///< Pointer to append to, obtained from m_data.
	size_t m_remaining_length;	///< Remaining bytes in m_current_ptr.
	size_t m_count;	///< Number of times the replacement was added.
};

/// The simple OtlCountedList<OpData> collector used by OpData::Split().
class CountedOpDataCollector : public SegmentCollector
{
public:
	CountedOpDataCollector(OtlCountedList<OpData> *list) : m_list(list) {}

	virtual OP_STATUS AddSegment(const OpData& segment)
	{
		return m_list->Append(segment);
	}

private:
	OtlCountedList<OpData> *m_list; ///< OpData segments are appended here.
};

/// The simple OtlCountedList<UniString> collector used by UniString::Split().
class CountedUniStringCollector : public SegmentCollector
{
public:
	CountedUniStringCollector(OtlCountedList<UniString> *list) : m_list(list) {}

	virtual OP_STATUS AddSegment(const OpData& segment);

private:
	OtlCountedList<UniString> *m_list; ///< Segments are appended here.
};

#endif /* MODULES_OPDATA_SEGMENTCOLLECTOR_H */
