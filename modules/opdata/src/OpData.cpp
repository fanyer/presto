/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#include "modules/opdata/OpData.h"
#include "modules/opdata/OpDataFragmentIterator.h"
#include "modules/opdata/src/OpDataFragment.h"
#include "modules/opdata/src/OpDataChunk.h"
#include "modules/opdata/src/NeedleFinder.h"
#include "modules/opdata/src/SegmentCollector.h"
#include "modules/opdata/src/DebugViewFormatter.h"
#include "modules/otl/list.h"


OpData::OpData(const OpData& other, size_t offset /* = 0 */, size_t length /* = OpDataFullLength */)
{
	op_memcpy(this, &other, sizeof(*this));
	if (!IsEmbedded())
		n.first->Traverse(NULL, n.last,
			OpDataFragment::TRAVERSE_ACTION_USE |
			OpDataFragment::TRAVERSE_INCLUDE_LAST);
	if (offset)
		Consume(offset);
	if (length < Length())
		Trunc(length);
}

OpData&
OpData::operator=(const OpData& other)
{
	if (this == &other)
		return *this;

	Clear();
	op_memcpy(this, &other, sizeof(*this));
	if (!IsEmbedded())
		n.first->Traverse(NULL, n.last,
			OpDataFragment::TRAVERSE_ACTION_USE |
			OpDataFragment::TRAVERSE_INCLUDE_LAST);
	return *this;
}

void
OpData::Clear(void)
{
	if (!IsEmbedded())
		n.first->Traverse(NULL, n.last,
			OpDataFragment::TRAVERSE_ACTION_RELEASE |
			OpDataFragment::TRAVERSE_INCLUDE_LAST);
	e.meta = EmptyMeta();
}

OP_STATUS
OpData::Append(const OpData& other)
{
	const OpData *src = &other;
	OpData copy;

	if (other.IsEmpty())
		return OpStatus::OK; // nothing to append
	else if (IsEmpty())
	{
		*this = other;
		return OpStatus::OK;
	}
	else if (this == &other) // self-append
	{
		copy = other;
		src = &copy;
	}

	if (src->IsEmbedded())
	{
		char *buf = GetAppendPtr(src->Length());
		RETURN_OOM_IF_NULL(buf);
		op_memcpy(buf, src->e.data, src->Length());
		return OpStatus::OK;
	}

	return AppendFragments(src->n.first, src->n.first_offset,
		src->n.last, src->n.last_length, src->n.length);
}

OP_STATUS
OpData::Append(char c)
{
	char *buf = GetAppendPtr(1);
	RETURN_OOM_IF_NULL(buf);
	*buf = c;
	return OpStatus::OK;
}

OP_STATUS
OpData::AppendConstData(
	const char *data,
	size_t length /* = OpDataUnknownLength */)
{
	size_t alloc = length;
	if (length == OpDataUnknownLength)
	{
		length = op_strlen(data);
		alloc = length + 1;
	}
	if (!length)
		return OpStatus::OK;
	else if (IsEmbedded() && CanEmbed(Length() + length))
	{
		char *buf = GetAppendPtr_NoAlloc(length);
		OP_ASSERT(buf);
		op_memcpy(buf, data, length);
		return OpStatus::OK;
	}

	// Prepare chunk and fragment
	OpDataChunk *chunk = OpDataChunk::Create(data, alloc);
	RETURN_OOM_IF_NULL(chunk);
	OpDataFragment *fragment = OpDataFragment::Create(chunk, 0, length);
	if (!fragment)
	{
		chunk->Release();
		return OpStatus::ERR_NO_MEMORY;
	}
	OP_STATUS s = AppendFragments(fragment, 0, fragment, length, length);
	// AppendFragments() calls fragment->Use() on success. On error, we want
	// to release the fragment anyway.
	fragment->Release();
	return s;
}

OP_STATUS
OpData::AppendRawData(
	OpDataDeallocStrategy ds,
	char *data,
	size_t length /* = OpDataUnknownLength */,
	size_t alloc /* = OpDataUnknownLength */)
{
	if (length == OpDataUnknownLength)
	{
		length = op_strlen(data);
		if (alloc == OpDataUnknownLength)
			alloc = length + 1;
		else if (length > alloc)
		{
			OP_ASSERT(!"Bad caller! length should be <= alloc");
			length = alloc;
		}
	}
	else if (alloc == OpDataUnknownLength)
		alloc = length;
	if (!length)
	{
		OpDataDealloc(data, ds);
		return OpStatus::OK;
	}
	else if (IsEmbedded() && CanEmbed(Length() + length))
	{
		char *buf = GetAppendPtr_NoAlloc(length);
		OP_ASSERT(buf);
		op_memcpy(buf, data, length);
		OpDataDealloc(data, ds);
		return OpStatus::OK;
	}

	// Prepare chunk and fragment
	OpDataChunk *chunk = OpDataChunk::Create(ds, data, alloc);
	RETURN_OOM_IF_NULL(chunk);
	OpDataFragment *fragment = OpDataFragment::Create(chunk, 0, length);
	if (!fragment)
	{
		// Must destroy chunk without deallocating data
		chunk->AbdicateOwnership();
		chunk->Release();
		return OpStatus::ERR_NO_MEMORY;
	}
	OP_STATUS s = AppendFragments(fragment, 0, fragment, length, length);
	if (OpStatus::IsError(s))
	{
		// Must destroy fragment & chunk without deallocating data
		chunk->AbdicateOwnership();
	}
	// AppendFragments() calls fragment->Use() on success. On error, we want
	// to release the fragment anyway.
	fragment->Release();
	return s;
}

OP_STATUS
OpData::AppendCopyData(const char *data, size_t length /* = OpDataUnknownLength */)
{
	if (length == OpDataUnknownLength)
		length = op_strlen(data);
	if (!length)
		return OpStatus::OK; // nothing to do

	char *buf = GetAppendPtr(length);
	RETURN_OOM_IF_NULL(buf);
	op_memcpy(buf, data, length);
	return OpStatus::OK;
}

OP_STATUS
OpData::AppendFormat(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	OP_STATUS result = AppendVFormat(format, args);
	va_end(args);
	return result;
}

OP_STATUS
OpData::AppendVFormat(const char *format, va_list args)
{
	size_t buf_len = 0;
	char *buf = GetAppendPtr_NoAlloc(buf_len);
	OP_ASSERT(buf_len <= Length()); // GetAppendPtr_NoAlloc() adds buf_len to Length()
	int result;

	// first, try to append without allocation
#ifdef va_copy // must use va_copy() in order to preserve args for reuse below
	va_list args_copy;
	va_copy(args_copy, args);
	result = op_vsnprintf(buf, buf_len, format, args_copy);
	va_end(args_copy);
#else // va_copy
	result = op_vsnprintf(buf, buf_len, format, args);
#endif // va_copy

	if (result < 0) // op_vsnprintf() returned error
	{
		Trunc(Length() - buf_len);
		return OpStatus::ERR;
	}
	else if (static_cast<size_t>(result) >= buf_len)
	{
		// buf was too short, allocate to make enough room
		Trunc(Length() - buf_len);
		buf_len = result + 1;
		buf = GetAppendPtr(buf_len);
		RETURN_OOM_IF_NULL(buf);
		result = op_vsnprintf(buf, buf_len, format, args);
	}
	OP_ASSERT(static_cast<size_t>(result) < buf_len);
	Trunc(Length() - (buf_len - result));
	return OpStatus::OK;
}

char *
OpData::GetAppendPtr(size_t length)
{
	if (!length)
		return IsEmbedded()
			? e.data + Length()
			: const_cast<char *>(n.last->Data() + n.last_length);

	// first try to satisfy request without allocation
	char *ret = GetAppendPtr_NoAlloc(length);
	if (ret)
		return ret;

	// must allocate new chunk to satisfy request.

	if (IsEmbedded())
		length += Length(); // also copy existing data into chunk

	// allocate appropriately sized OpDataChunk (w/ '\0'-terminator)
	OpDataChunk *chunk = OpDataChunk::Allocate(length + 1);
	if (!chunk)
		return NULL;

	// wrap chunk in fragment
	OpDataFragment *fragment = OpDataFragment::Create(chunk, 0, length);
	if (!fragment) {
		chunk->Release();
		return NULL;
	}

	OP_ASSERT(chunk->IsMutable());
	OP_ASSERT(chunk->Size() >= length);
	ret = chunk->MutableData();

	if (IsEmbedded())
	{
		if (!IsEmpty())
		{
			// copy existing embedded data into chunk
			op_memcpy(ret, e.data, Length());
			ret += Length();
		}

		// convert from embedded to normal mode pointing at fragment
		n.first = fragment;
		n.first_offset = 0;
		n.last = fragment;
		n.last_length = length;
		n.length = length; // already added Length() above
		return ret;
	}
	else if (OpStatus::IsError(AppendFragments(
		fragment, 0, fragment, length, length)))
	{
		fragment->Release();
		chunk->Release();
		return NULL;
	}
	fragment->Release(); // AppendFragments() called fragment->Use().
	return ret;
}

bool
OpData::Equals(const char *buf, size_t length /* = OpDataUnknownLength */) const
{
	if (length == OpDataUnknownLength)
		length = op_strlen(buf);
	if (length != Length())
		return false;
	return StartsWith(buf, length);
}

bool
OpData::EqualsCase(const char* s, size_t length /* = OpDataUnknownLength */) const
{
	if (length == OpDataUnknownLength)
		length = op_strlen(s);
	if (length != Length())
		return false;
	return StartsWithCase(s, length);
}

class CompareHelper : public OpDataFragment::TraverseHelper
{
public:
	CompareHelper(const char *cmp_buf, size_t cmp_buf_len, size_t cmp_total_len, const OpDataFragment *cmp_next, size_t offset)
		: m_cmp_buf(cmp_buf)
		, m_cmp_buf_len(cmp_buf_len)
		, m_cmp_total_len(cmp_total_len)
		, m_cmp_next(cmp_next)
		, m_offset(offset)
		, m_result(0)
	{}

	virtual bool ProcessFragment(const OpDataFragment *frag)
	{
		OP_ASSERT(!m_result);

		if (m_offset >= frag->Length())
		{
			m_offset -= frag->Length();
			return false; // continue with next fragment
		}

		// Compare to m_cmp_buf, starting from frag->Data()[m_offset]
		const char *p = frag->Data() + m_offset;
		size_t cmp_len = MIN(frag->Length() - m_offset, m_cmp_buf_len);
		m_result = p == m_cmp_buf ? 0 : op_memcmp(p, m_cmp_buf, cmp_len);
		if (m_result)
			return true;

		// Not found, prepare for next search
		m_cmp_buf += cmp_len;
		m_cmp_buf_len -= cmp_len;
		m_cmp_total_len -= cmp_len;
		m_offset += cmp_len;
		if (m_cmp_buf_len == 0 && m_cmp_total_len > 0)
		{
			// Proceed to m_cmp_next.
			OP_ASSERT(m_cmp_next);
			m_cmp_buf = m_cmp_next->Data();
			m_cmp_buf_len = MIN(m_cmp_total_len, m_cmp_next->Length());
			m_cmp_next = m_cmp_next->Next();
			// There may still be data left in 'frag' that we must
			// continue comparing against.
			if (m_cmp_total_len && frag->Length() > m_offset)
				return ProcessFragment(frag);
		}
		m_offset = 0;
		return !m_cmp_total_len;
	}

	int Result(void) const { return m_result; }

	size_t Remaining(void) const { return m_cmp_total_len; }

private:
	/**
	 * When comparing object A to object (or buffer) B, we need to do a
	 * piecewise comparison between subsets of fragments from A and subsets
	 * of fragments from B. The following members keep track of B:
	 * @{
	 */

	/// Current buffer to compare against.
	const char *m_cmp_buf;

	/// Number of bytes to compare in current buffer, starting from \c m_cmp_buf.
	size_t m_cmp_buf_len;

	/// Number of bytes to compare in total from current buffer and \c next.
	size_t m_cmp_total_len;

	/// The fragment from which to get the next comparison buffer.
	const OpDataFragment *m_cmp_next;

	/**
	 * @}
	 * ...and the following members, plus the fragment passed to
	 * ProcessFragment(), enable us to keep track of where we are in A:
	 * @{
	 */

	/// Offset where to start compare, relative to start of current fragment.
	size_t m_offset;

	/** @} */

	/// Stored result from comparison.
	int m_result;
};

bool
OpData::StartsWith(const OpData& other, size_t length /* = OpDataFullLength */) const
{
	length = MIN(length, other.Length());
	if (length > Length())
		return false;
	if (!length)
		return true;
	if (IsEmbedded())
	{
		OP_ASSERT(length < EmbeddedDataLimit);
		char cmp_buf[EmbeddedDataLimit]; // ARRAY OK 2011-08-23 johanh
		size_t cmp_len = other.CopyInto(cmp_buf, length);
		OP_ASSERT(length == cmp_len);
		return op_memcmp(e.data, cmp_buf, cmp_len) == 0;
	}

	// Prepare CompareHelper object and perform traversal
	const char *buf; // first buffer to compare against
	size_t buf_len; // length of first buffer
	const OpDataFragment *next_buf; // where to get next buffer
	if (other.IsEmbedded())
	{
		buf = other.e.data;
		buf_len = other.Length();
		next_buf = NULL;
	}
	else
	{
		buf = other.n.first->Data() + other.n.first_offset;
		buf_len = (other.n.first == other.n.last)
			? other.n.last_length
			: other.n.first->Length();
		buf_len -= other.n.first_offset;
		next_buf = other.n.first->Next();
	}
	CompareHelper helper(buf, MIN(buf_len, length), length, next_buf, n.first_offset);
	n.first->Traverse(&helper, n.last);
	OP_ASSERT(helper.Result() || helper.Remaining() == 0);
	return !helper.Result();
}

bool
OpData::StartsWith(const char *buf, size_t length /* = OpDataUnknownLength */) const
{
	OP_ASSERT(buf || length == 0);
	if (length == OpDataUnknownLength)
		length = op_strlen(buf);
	if (length > Length())
		return false;
	if (!length)
		return true;
	if (IsEmbedded())
		return op_memcmp(e.data, buf, length) == 0;

	// Prepare CompareHelper object and perform traversal
	CompareHelper helper(buf, length, length, NULL, n.first_offset);
	n.first->Traverse(&helper, n.last);
	OP_ASSERT(helper.Result() || helper.Remaining() == 0);
	return !helper.Result();
}

inline size_t op_strnlen(const char* str, size_t max_len)
{
	const char* end = str ? static_cast<const char*>(op_memchr(str, 0, max_len)) : NULL;
	return end ? end-str : max_len;
}

bool
OpData::StartsWithCase(const OpData& other, size_t length /* = OpDataFullLength */) const
{
	length = MIN(length, other.Length());
	if (length > Length())
		return false;
	if (!length)
		return true;

	OpDataFragmentIterator this_itr(*this);
	OpDataFragmentIterator other_itr(other);
	size_t this_length = this_itr.GetLength();
	const char* this_data = this_itr.GetData();
	size_t this_str_len = op_strnlen(this_data, this_length);
	size_t other_length = other_itr.GetLength();
	const char* other_data = other_itr.GetData();
	size_t other_str_len = op_strnlen(other_data, other_length);
	while (length)
	{
		OP_ASSERT(this_itr && this_data && this_length);
		OP_ASSERT(other_itr && other_data && other_length);

		size_t to_compare = MIN(this_length, length);
		if (to_compare > other_length) to_compare = other_length;
		if (to_compare > this_str_len)
		{
			OP_ASSERT(this_str_len != this_length);
			to_compare = this_str_len;
		}
		if (to_compare > other_str_len)
		{
			OP_ASSERT(other_str_len != other_length);
			to_compare = other_str_len;
		}

		// compare the first part of both fragments
		if (to_compare == 0)
		{
			OP_ASSERT(other_length > 0 && this_length > 0 &&
					  other_str_len != other_length && this_str_len != this_length);
			if (this_str_len != 0 && other_str_len != 0)
				return false;
			OP_ASSERT(this_data[0] == '\0' && other_data[0] == '\0');
			to_compare++;
		}
		else if (0 != op_strnicmp(this_data, other_data, to_compare))
			return false;

		// advance to the next part to compare:
		length -= to_compare;

		if (length > 0)
		{
			// advance data of this
			this_length -= to_compare;
			if (this_length > 0)
			{
				this_data += to_compare;
				if (this_str_len == 0)
					this_str_len = op_strnlen(this_data, this_length);
				else
					this_str_len -= to_compare;
			}
			else
			{
				++this_itr;
				this_length = this_itr.GetLength();
				this_data = this_itr.GetData();
				this_str_len = op_strnlen(this_data, this_length);
			}

			// advance data of other
			other_length -= to_compare;
			if (other_length > 0)
			{
				other_data += to_compare;
				if (other_str_len == 0)
					other_str_len = op_strnlen(other_data, other_length);
				else
					other_str_len -= to_compare;
			}
			else
			{
				++other_itr;
				other_length = other_itr.GetLength();
				other_data = other_itr.GetData();
				other_str_len = op_strnlen(other_data, other_length);
			}
		}
	}

	return true;
}

bool
OpData::StartsWithCase(const char* buf, size_t length /* = OpDataUnknownLength */) const
{
	OP_ASSERT(buf || length == 0);
	if (length == OpDataUnknownLength)
		length = op_strlen(buf);
	if (length > Length())
		return false;
	if (!length)
		return true;

	OpDataFragmentIterator this_itr(*this);
	size_t this_length = this_itr.GetLength();
	const char* this_data = this_itr.GetData();
	size_t this_str_len = op_strnlen(this_data, this_length);
	while (length)
	{
		OP_ASSERT(this_itr && this_data && this_length);
		size_t to_compare = MIN(this_length, length);
		if (to_compare > this_str_len)
		{
			OP_ASSERT(this_str_len != this_length);
			to_compare = this_str_len;
		}

		if (to_compare == 0)
		{
			OP_ASSERT(this_length > 0 && this_str_len == 0);
			if (buf[0] != '\0')
				return false;
			to_compare++;
		}
		else if (0 != op_strnicmp(this_data, buf, to_compare))
			return false;

		length -= to_compare;
		if (length > 0)
		{
			// advance data of this
			this_length -= to_compare;
			if (this_length > 0)
			{
				this_data += to_compare;
				if (this_str_len == 0)
					this_str_len = op_strnlen(this_data, this_length);
				else
					this_str_len -= to_compare;
			}
			else
			{
				++this_itr;
				this_length = this_itr.GetLength();
				this_data = this_itr.GetData();
				this_str_len = op_strnlen(this_data, this_length);
			}

			// advance buf
			buf += to_compare;
		}
	}

	return true;
}

bool
OpData::EndsWith(const OpData& other, size_t length /* = OpDataFullLength */) const
{
	length = MIN(length, other.Length());
	if (length > Length())
		return false;
	if (!length)
		return true;
	OpData endof_this(*this, Length()-length, length);
	return endof_this.Equals(OpData(other, 0, length));
}

bool
OpData::EndsWith(const char* buf, size_t length /* = OpDataUnknownLength */) const
{
	OP_ASSERT(buf || length == 0);
	if (length == OpDataUnknownLength)
		length = op_strlen(buf);
	if (length > Length())
		return false;
	if (!length)
		return true;
	OpData endof_this(*this, Length()-length, length);
	return endof_this.Equals(buf, length);
}

const char&
OpData::At(size_t index) const
{
	OP_ASSERT(index < Length());
	if (IsEmbedded())
		return e.data[index];

	// shortcut to last fragment
	if (n.last != n.first && index >= n.length - n.last_length)
	{
		index -= n.length - n.last_length;
		return n.last->At(index);
	}

	index += n.first_offset;
	OpDataFragment *frag = n.first->Lookup(&index);
	OP_ASSERT(frag && index < frag->Length());
	return frag->At(index);
}

size_t
OpData::FindFirst(char needle, size_t offset /* = 0 */, size_t length /* = OpDataFullLength */) const
{
	ByteFinder bf(needle);
	size_t result = Find(&bf, offset, length);
	if (result == OpDataNotFound)
		return OpDataNotFound;
	OP_ASSERT(At(result) == needle);
	return result;
}

size_t
OpData::FindFirst(
	const char *needle, size_t needle_len /* = OpDataUnknownLength */,
	size_t offset /* = 0 */, size_t length /* = OpDataFullLength */) const
{
	OP_ASSERT(needle);
	if (needle_len == OpDataUnknownLength)
		needle_len = op_strlen(needle);
	if (!needle_len)
		return offset <= Length() ? offset : OpDataNotFound;

	ByteSequenceFinder bsf(needle, needle_len);
	size_t result = Find(&bsf, offset, length);
	if (result == OpDataNotFound)
		return OpDataNotFound;
	OP_ASSERT(OpData(*this, result, needle_len).Equals(needle, needle_len));
	return result;
}

size_t
OpData::FindFirstOf(
	const char *accept, size_t accept_len /* = OpDataUnknownLength */,
	size_t offset /* = 0 */, size_t length /* = OpDataFullLength */) const
{
	OP_ASSERT(accept);
	if (accept_len == OpDataUnknownLength)
		accept_len = op_strlen(accept);
	if (!accept_len)
		return OpDataNotFound;

	ByteSetFinder bsf(accept, accept_len);
	size_t result = Find(&bsf, offset, length);
	if (result == OpDataNotFound)
		return OpDataNotFound;
	OP_ASSERT(bsf.Contains(At(result)));
	return result;
}

size_t
OpData::FindEndOf(
	const char *ignore, size_t ignore_len /* = OpDataUnknownLength */,
	size_t offset /* = 0 */, size_t length /* = OpDataFullLength */) const
{
	OP_ASSERT(ignore);
	if (ignore_len == OpDataUnknownLength)
		ignore_len = op_strlen(ignore);

	ByteSetComplementFinder bscf(ignore, ignore_len);
	size_t result = Find(&bscf, offset, length);
	if (result == OpDataNotFound)
		return OpDataNotFound;
	OP_ASSERT(bscf.Contains(At(result)));
	return result;
}

size_t
OpData::FindLast(char needle, size_t offset /* = 0 */, size_t length /* = OpDataFullLength */) const
{
	LastByteFinder lbf(needle);
	size_t result = Find(&lbf, offset, length);
	if (result == OpDataNotFound)
		return OpDataNotFound;
	OP_ASSERT(At(result) == needle);
	return result;
}

size_t
OpData::FindLast(
	const char *needle, size_t needle_len /* = OpDataUnknownLength */,
	size_t offset /* = 0 */, size_t length /* = OpDataFullLength */) const
{
	OP_ASSERT(needle);

	if (offset > Length())
		return OpDataNotFound;
	if (length == OpDataFullLength || offset + length > Length())
		length = Length() - offset;

	if (needle_len == OpDataUnknownLength)
		needle_len = op_strlen(needle);
	if (!needle_len)
		return offset + length;

	LastByteSequenceFinder lbsf(needle, needle_len);
	size_t result = Find(&lbsf, offset, length);
	if (result == OpDataNotFound)
		return OpDataNotFound;
	OP_ASSERT(OpData(*this, result, needle_len).Equals(needle, needle_len));
	return result;
}

void
OpData::Consume(size_t length)
{
	if (!length)
		return; // nothing to do
	if (length >= Length())
	{
		Clear(); // consume everything
		return;
	}
	if (IsEmbedded())
	{
		op_memmove(e.data, e.data + length, Length() - length);
		SetMeta(Length() - length);
		return;
	}

	// Traverse fragment list, releasing consumed fragments
	n.first_offset += length;
	n.first = n.first->Lookup(&n.first_offset,
		OpDataFragment::TRAVERSE_ACTION_RELEASE);
	OP_ASSERT(n.first && n.first_offset < n.first->Length());
	n.length -= length;
}

void
OpData::Trunc(size_t new_length)
{
	if (!new_length)
	{
		Clear(); // truncate to 0
		return;
	}
	else if (new_length >= Length())
		return; // nothing to do
	else if (IsEmbedded())
	{
		SetMeta(new_length); // truncate embedded data
		return;
	}
	else if (Length() - new_length < n.last_length)
	{
		// truncate within last fragment, no need to traverse fragments
		n.last_length -= Length() - new_length;
		n.length = new_length;
		return;
	}
	OP_ASSERT(n.first != n.last); // single fragment should be handled above

	// Traverse fragment list to find new n.last and n.last_length
	OpDataFragment *old_last = n.last;
	n.last_length = new_length + n.first_offset;
	n.last = n.first->Lookup(&n.last_length, OpDataFragment::LOOKUP_PREFER_LENGTH);
	OP_ASSERT(n.last && n.last_length && n.last_length <= n.last->Length() && old_last != n.last);
	n.length = new_length;

	// Release the remainder of the fragment list
	n.last->Traverse(NULL, old_last,
		OpDataFragment::TRAVERSE_ACTION_RELEASE |
		OpDataFragment::TRAVERSE_EXCLUDE_FIRST |
		OpDataFragment::TRAVERSE_INCLUDE_LAST );
}

void
OpData::LStrip(void)
{
	OpDataCharIterator it(*this);
	size_t i = 0;
	while (i < Length() && op_isspace(*it)) { ++i; ++it; }
	Consume(i);
}

void
OpData::RStrip(void)
{
	size_t i = Length();
	OpDataCharIterator it(*this, i);
	while (--it && op_isspace(*it)) i--;
	Trunc(i);
}

void
OpData::Strip(void)
{
	LStrip();
	RStrip();
}

class FindHelper : public OpDataFragment::TraverseHelper
{
public:
	FindHelper(NeedleFinder *nf, size_t fragment_offset, size_t global_offset, size_t remaining)
		: m_nf(nf)
		, m_fragment_offset(fragment_offset)
		, m_global_offset(global_offset)
		, m_remaining(remaining)
		, m_found(OpDataNotFound)
		, m_needle_len(OpDataUnknownLength)
	{}

	virtual bool ProcessFragment(const OpDataFragment *frag)
	{
		if (m_fragment_offset >= frag->Length())
		{
			m_fragment_offset -= frag->Length();
			return false; // continue with next fragment
		}

		// Invoke needle finder from frag->Data()[m_fragment_offset]
		const char * const start = frag->Data() + m_fragment_offset;
		size_t length = MIN(frag->Length() - m_fragment_offset, m_remaining);
		size_t found = m_nf->FindNeedle(
			m_global_offset, start, length, &m_needle_len, length == m_remaining);
		if (found != OpDataNotFound)
		{
			OP_ASSERT(found + m_needle_len <= m_global_offset + length);
			m_found = found;
			return true; // stop traversing
		}

		// not found
		m_fragment_offset = 0; // start at beginning of next fragment
		m_global_offset += length;
		m_remaining -= length;
		return !m_remaining;
	}

	/**
	 * Call these after a search to get the result.
	 * @{
	 */
	size_t Found(void) const { return m_found; }
	size_t NeedleLength(void) const { return m_needle_len; }
	/** @} */

private:
	/// The #NeedleFinder instance used to find out result.
	NeedleFinder *m_nf;

	/// Where to start searching, relative to current fragment.
	size_t m_fragment_offset;

	/// Current search position relative to the OpData object's start.
	size_t m_global_offset;

	/// The number of bytes left to examine.
	size_t m_remaining;

	/**
	 * When the needle has been found, the global offset of the start of the
	 * needle is stored here.
	 */
	size_t m_found;

	/// The length of the found needle is stored here.
	size_t m_needle_len;
};

size_t
OpData::Find(NeedleFinder *nf, size_t offset, size_t length) const
{
	OP_ASSERT(nf);

	if (offset > Length())
		return OpDataNotFound;
	if (length == OpDataFullLength || length > Length() - offset)
		length = Length() - offset;
	if (!length)
		return OpDataNotFound; // no haystack

	if (IsEmbedded())
	{
		size_t needle_len = 0;
		size_t found = nf->FindNeedle(
			offset, e.data + offset, length, &needle_len, true);
		if (found == OpDataNotFound)
			return OpDataNotFound;
		OP_ASSERT(found >= offset);
		OP_ASSERT(found + needle_len <= Length());
		return found;
	}

	FindHelper helper(nf, n.first_offset + offset, offset, length);
	n.first->Traverse(&helper, n.last);
	if (helper.Found() == OpDataNotFound)
		return OpDataNotFound;
	OP_ASSERT(helper.Found() >= offset);
	OP_ASSERT(helper.Found() + helper.NeedleLength() <= offset + length);
	return helper.Found();
}

class SplitHelper : public OpDataFragment::TraverseHelper
{
public:
	SplitHelper(size_t maxsplit, size_t offset, size_t length,
		NeedleFinder *sepf, SegmentCollector *coll)
		: m_segment()
		, m_splits_left(maxsplit)
		, m_remaining(length)
		, m_global_offset(0)
		, m_offset(offset)
		, m_sepf(sepf)
		, m_coll(coll)
		, m_result(OpStatus::OK)
	{
		OpStatus::Ignore(m_result);
		OP_ASSERT(m_splits_left);
	}

	virtual bool ProcessFragment(OpDataFragment *frag)
	{
		OP_ASSERT(m_remaining);
		OP_ASSERT(m_last);

		// skip past fragment if not part of search
		if (m_offset >= frag->Length())
		{
			OP_ASSERT(m_segment.IsEmpty()); // we are between segments
			m_offset -= frag->Length();
			return false;
		}

		// start searching from m_offset within frag.
		const char * const start = frag->Data() + m_offset;
		// Number of bytes consumed in this iteration; initialize to upper bound:
		size_t length = MIN(m_remaining, frag->Length() - m_offset);
		OP_ASSERT(length);
		size_t sep_len = 0;
		size_t found = m_splits_left
			? m_sepf->FindNeedle(m_global_offset, start, length, &sep_len, frag == m_last)
			: OpDataNotFound;

		if (found != OpDataNotFound) // found separator
		{
			/*
			 * We have found a separator starting at global index
			 * 'found', and ending at 'found + sep_len'.
			 */
			OpData new_segment;

			// First, find the end of the current segment.
			if (found > m_global_offset)
			{
				// Initial portion of current fragment belongs
				// in the last segment.
				frag->Use();
				m_segment.ExtendIntoFragment(frag, m_offset, found - m_global_offset);
			}
			else if (found < m_global_offset)
			{
				/*
				 * Some needle finders may return a separator
				 * that is wholly contained in a previous
				 * fragment. In this case we must transfer the
				 * bytes between that separator and the start
				 * of this fragment from the current segment
				 * into what will become the next segment.
				 */
				if (found + sep_len < m_global_offset)
				{
					size_t transfer = m_global_offset - (found + sep_len);
					OP_ASSERT(transfer <= m_segment.Length());
					new_segment = OpData(m_segment, m_segment.Length() - transfer);
				}

				/*
				 * Separator starts in a previous fragment, so
				 * we must truncate the current segment
				 * accordingly.
				 */
				size_t remove = m_global_offset - found;
				OP_ASSERT(remove <= m_segment.Length());
				m_segment.Trunc(m_segment.Length() - remove);
			}
			// else m_segment is exactly what we want, already ;-)


			// Finish up the current segment.
			if (m_splits_left != OpDataFullLength)
				m_splits_left--;
			m_result = m_coll->AddSegment(m_segment);
			if (OpStatus::IsError(m_result))
				return true;

			// Finally, prepare the next segment.
			m_segment = new_segment;
			found += sep_len; // reset found to after the separator
			length = found > m_global_offset ? found - m_global_offset : 0;
			OP_ASSERT(length <= m_remaining);
			m_remaining -= length;
			m_global_offset += length;
			m_offset += length;
			if (m_remaining) // continue search within this fragment
				return ProcessFragment(frag);
		}
		else // no more separators within this fragment.
		{
			OP_ASSERT(length);
			frag->Use();
			m_segment.ExtendIntoFragment(frag, m_offset, length);
			m_remaining -= length;
			m_global_offset += length;
			m_offset = 0;
		}
		return !m_remaining;
	}

	virtual void SetTraverseProperties(const OpDataFragment *, const OpDataFragment *last, unsigned int)
	{
		m_last = last;
	}

	OP_STATUS SegmentCollectorResult(void) const { return m_result; }

	OpData& CurrentSegment(void) { return m_segment; }

private:
	OpData m_segment; ///< Current OpData segment.
	size_t m_splits_left; ///< Number of remaining splits.
	size_t m_remaining; ///< Number of remaining bytes to be traversed.
	size_t m_global_offset; ///< Current global offset within the haystack.
	size_t m_offset; ///< Number of bytes to skip in the current fragment.
	NeedleFinder *m_sepf; ///< Separator finder.
	SegmentCollector *m_coll; ///< Segment collector.
	OP_STATUS m_result; ///< Return value from \c m_coll->AddSegment().
	const OpDataFragment *m_last; ///< Last fragment to be processed.
};

OP_STATUS
OpData::Split(NeedleFinder *sepf, SegmentCollector *coll,
	size_t maxsplit /* = OpDataFullLength */) const
{
	OP_ASSERT(sepf && coll);

	if (!maxsplit)
		return coll->AddSegment(*this);

	if (IsEmbedded())
	{
		size_t numsplits = 0;
		size_t start = 0;
		while (numsplits++ < maxsplit && start < Length())
		{
			size_t sep_len = 0;
			size_t found = sepf->FindNeedle(
				start, e.data + start, Length() - start, &sep_len, true);
			if (found == OpDataNotFound)
				break;
			OP_ASSERT(start <= found);
			OP_ASSERT(found + sep_len <= Length());
			RETURN_IF_ERROR(coll->AddSegment(OpData(*this, start, found - start)));
			start = found + sep_len;
		}
		return coll->AddSegment(OpData(*this, start));
	}

	SplitHelper helper(maxsplit, n.first_offset, Length(), sepf, coll);
	n.first->Traverse(&helper, n.last);
	RETURN_IF_ERROR(helper.SegmentCollectorResult());
	return coll->AddSegment(helper.CurrentSegment());
}

OP_STATUS
OpData::Remove(char remove, size_t maxremove /* = OpDataFullLength */)
{
	OpData result;
	ByteFinder bf(remove);
	Concatenator coll(result);
	RETURN_IF_ERROR(Split(&bf, &coll, maxremove));
	*this = result;
	return OpStatus::OK;
}

OP_STATUS
OpData::Remove(const char *remove, size_t length /* = OpDataUnknownLength */, size_t maxremove /* = OpDataFullLength */)
{
	OP_ASSERT(remove);
	if (length == OpDataUnknownLength)
		length = op_strlen(remove);

	OpData result;
	ByteSequenceFinder bsf(remove, length);
	Concatenator coll(result);
	RETURN_IF_ERROR(Split(&bsf, &coll, maxremove));
	*this = result;
	return OpStatus::OK;
}

OP_STATUS
OpData::Replace(char character, char replacement, size_t maxreplace, size_t* replaced)
{
	OpData result;
	ByteFinder sepf(character);
	OpData replacement_data;
	RETURN_IF_ERROR(replacement_data.SetCopyData(&replacement, 1));
	ReplaceConcatenator coll(result, replacement_data, Length());
	RETURN_IF_ERROR(Split(&sepf, &coll, maxreplace));
	coll.Done();
	*this = result;
	if (replaced) *replaced = coll.CountReplacement();
	return OpStatus::OK;
}

OP_STATUS
OpData::Replace(const char* substring, const char* replacement, size_t maxreplace, size_t* replaced)
{
	if (!substring || !*substring)
		return OpStatus::ERR;

	OpData result;
	size_t substring_length = op_strlen(substring);
	ByteSequenceFinder sepf(substring, substring_length);
	size_t replacement_length = replacement ? op_strlen(replacement) : 0;
	OpData replacement_data;
	RETURN_IF_ERROR(replacement_data.SetCopyData(replacement, replacement_length));
	ReplaceConcatenator coll(result, replacement_data);
	RETURN_IF_ERROR(Split(&sepf, &coll, maxreplace));
	*this = result;
	if (replaced) *replaced = coll.CountReplacement();
	return OpStatus::OK;
}

OtlCountedList<OpData> *
OpData::Split(char sep, size_t maxsplit /* = OpDataFullLength */) const
{
	OtlCountedList<OpData> *ret = OP_NEW(OtlCountedList<OpData>, ());
	if (!ret)
		return NULL;
	ByteFinder bf(sep);
	CountedOpDataCollector coll(ret);
	if (OpStatus::IsError(Split(&bf, &coll, maxsplit)))
	{
		OP_DELETE(ret);
		return NULL;
	}
	return ret;
}

OtlCountedList<OpData> *
OpData::Split(const char *sep, size_t length /* = OpDataUnknownLength */, size_t maxsplit /* = OpDataFullLength */) const
{
	OP_ASSERT(sep);
	if (length == OpDataUnknownLength)
		length = op_strlen(sep);

	OtlCountedList<OpData> *ret = OP_NEW(OtlCountedList<OpData>, ());
	if (!ret)
		return NULL;
	ByteSequenceFinder bsf(sep, length);
	CountedOpDataCollector coll(ret);
	if (OpStatus::IsError(Split(&bsf, &coll, maxsplit)))
	{
		OP_DELETE(ret);
		return NULL;
	}
	return ret;
}

OtlCountedList<OpData>*
OpData::SplitAtAnyOf(const char* sep, size_t length, size_t maxsplit) const
{
	OP_ASSERT(sep);
	if (length == OpDataUnknownLength)
		length = op_strlen(sep);

	OpAutoPtr< OtlCountedList<OpData> > ret(OP_NEW(OtlCountedList<OpData>, ()));
	RETURN_VALUE_IF_NULL(ret.get(), NULL);
	ByteSetFinder bsf(sep, length);
	CountedOpDataCollector coll(ret.get());
	RETURN_VALUE_IF_ERROR(Split(&bsf, &coll, maxsplit), NULL);
	return ret.release();
}

class CopyIntoHelper : public OpDataFragment::TraverseHelper
{
public:
	CopyIntoHelper(char *buf, size_t length, size_t offset)
		: m_buf(buf), m_written(0), m_length(length), m_offset(offset)
	{}

	virtual bool ProcessFragment(const OpDataFragment *frag)
	{
		OP_ASSERT(m_buf);
		if (m_offset >= frag->Length())
		{
			m_offset -= frag->Length();
			return false;
		}
		const char *from = frag->Data() + m_offset;
		size_t copy_len = MIN(m_length - m_written, frag->Length() - m_offset);
		op_memcpy(m_buf + m_written, from, copy_len);
		m_written += copy_len;
		m_offset = 0;
		return m_written == m_length;
	}

	void SetBuffer(char *buf) { m_buf = buf; }

	char *Buffer(void) const { return m_buf; }

	size_t Written(void) const { return m_written; }

	size_t Length(void) const { return m_length; }

private:
	char *m_buf; ///< Buffer to copy data into
	size_t m_written; ///< Number of bytes written into \c buf so far
	size_t m_length; ///< Total number of bytes to copy
	size_t m_offset; ///< Byte offset into fragment list where copying starts
};

size_t
OpData::CopyInto(char *ptr, size_t length, size_t offset /* = 0 */) const
{
	if (length + offset > Length())
		length = Length() - offset;

	if (IsEmbedded())
		op_memcpy(ptr, e.data + offset, length);
	else
	{
		CopyIntoHelper helper(ptr, length, n.first_offset + offset);
		n.first->Traverse(&helper, n.last);
		OP_ASSERT(helper.Written() == length);
	}
	return length;
}

const char *
OpData::Data(bool nul_terminated /* = false */) const
{
	if (nul_terminated && Length() && At(Length() - 1) == '\0')
		nul_terminated = false; // already '\0'-terminated
	return CreatePtr(nul_terminated ? 1 : 0);
}

OP_STATUS
OpData::CreatePtr(const char **ptr, size_t nul_terminators /* = 0 */) const
{
	// First, count the '\0' terminators _within_ the buffer.
	size_t i = Length();
	while (nul_terminators && i && At(i - 1) == '\0')
	{
		i--;
		nul_terminators--;
	}

	const char *ret = CreatePtr(nul_terminators);
	RETURN_OOM_IF_NULL(ret);
	*ptr = ret;
	return OpStatus::OK;
}

OP_STATUS
OpData::ToShort(short *ret, size_t *length /* = NULL */, int base /* = 10 */, size_t offset /* = 0 */) const
{
	long l;
	RETURN_IF_ERROR(ToLong(&l, length, base, offset));
	if (l < SHRT_MIN || l > SHRT_MAX)
		return OpStatus::ERR_OUT_OF_RANGE;
	*ret = static_cast<short>(l);
	return OpStatus::OK;
}

OP_STATUS
OpData::ToInt(int *ret, size_t *length /* = NULL */, int base /* = 10 */, size_t offset /* = 0 */) const
{
	long l;
	RETURN_IF_ERROR(ToLong(&l, length, base, offset));
	if (l < INT_MIN || l > INT_MAX)
		return OpStatus::ERR_OUT_OF_RANGE;
	*ret = static_cast<int>(l);
	return OpStatus::OK;
}

OP_STATUS
OpData::ToLong(long *ret, size_t *length /* = NULL */, int base /* = 10 */, size_t offset /* = 0 */) const
{
	OP_ASSERT(ret);
	if (length)
		*length = 0;

	// Need '\0'-terminated buffer
	const char *buf = Data(true);
	RETURN_OOM_IF_NULL(buf);
	const char *start = buf + offset;
#if defined(TYPE_SYSTEMATIC) && defined(HAVE_STRTOL)
	const
#endif
	char *end;
#ifdef HAVE_ERRNO
	errno = 0;
#endif // HAVE_ERRNO
	*ret = op_strtol(start, &end, base);
	if (end > start)
	{
		if (length)
			*length = end - start;
		else
		{
			const char *stop = buf + Length();
			while (end < stop && op_isspace(*end))
				end++;
			if (end != stop)
				return OpStatus::ERR;
		}
		while (start < end &&
			   (op_isspace(*start) || *start == '-' || *start == '+'))
			++start;
	}
#ifdef HAVE_ERRNO
	if (errno == ERANGE)
		return OpStatus::ERR_OUT_OF_RANGE;
	if (errno)
		return OpStatus::ERR;
#endif // HAVE_ERRNO
	return end <= start ? OpStatus::ERR : OpStatus::OK;
}

OP_STATUS
OpData::ToUShort(unsigned short *ret, size_t *length /* = NULL */, int base /* = 10 */, size_t offset /* = 0 */) const
{
	unsigned long l;
	RETURN_IF_ERROR(ToULong(&l, length, base, offset));
	if (l > USHRT_MAX)
		return OpStatus::ERR_OUT_OF_RANGE;
	*ret = static_cast<unsigned short>(l);
	return OpStatus::OK;
}

OP_STATUS
OpData::ToUInt(unsigned int *ret, size_t *length /* = NULL */, int base /* = 10 */, size_t offset /* = 0 */) const
{
	unsigned long l;
	RETURN_IF_ERROR(ToULong(&l, length, base, offset));
	if (l > UINT_MAX)
		return OpStatus::ERR_OUT_OF_RANGE;
	*ret = static_cast<unsigned int>(l);
	return OpStatus::OK;
}

OP_STATUS
OpData::ToULong(unsigned long *ret, size_t *length /* = NULL */, int base /* = 10 */, size_t offset /* = 0 */) const
{
	OP_ASSERT(ret);
	if (length)
		*length = 0;

	// Need '\0'-terminated buffer
	const char *buf = Data(true);
	RETURN_OOM_IF_NULL(buf);
	const char *start = buf + offset;
#if defined(TYPE_SYSTEMATIC) && defined(HAVE_STRTOUL)
	const
#endif
	char *end;
#ifdef HAVE_ERRNO
	errno = 0;
#endif // HAVE_ERRNO
	*ret = op_strtoul(start, &end, base);
	if (end > start)
	{
		if (length)
			*length = end - start;
		else
		{
			const char *stop = buf + Length();
			while (end < stop && op_isspace(*end))
				end++;
			if (end != stop)
				return OpStatus::ERR;
		}
		while (start < end &&
			   (op_isspace(*start) || *start == '-' || *start == '+'))
			++start;
	}
#ifdef HAVE_ERRNO
	if (errno == ERANGE)
		return OpStatus::ERR_OUT_OF_RANGE;
	if (errno)
		return OpStatus::ERR;
#endif // HAVE_ERRNO
	return end <= start ? OpStatus::ERR : OpStatus::OK;
}

#ifdef STDLIB_64BIT_STRING_CONVERSIONS
OP_STATUS
OpData::ToINT64(INT64 *ret, size_t *length /* = NULL */, int base /* = 10 */, size_t offset /* = 0 */) const
{
	OP_ASSERT(ret);
	if (length)
		*length = 0;

	// Need '\0'-terminated buffer
	const char *buf = Data(true);
	RETURN_OOM_IF_NULL(buf);
	const char *start = buf + offset;
	char *end;
#ifdef HAVE_ERRNO
	errno = 0;
#endif // HAVE_ERRNO
	*ret = op_strtoi64(start, &end, base);
	if (end > start)
	{
		if (length)
			*length = end - start;
		else
		{
			const char *stop = buf + Length();
			while (end < stop && op_isspace(*end))
				end++;
			if (end != stop)
				return OpStatus::ERR;
		}
		while (start < end &&
			   (op_isspace(*start) || *start == '-' || *start == '+'))
			++start;
	}
#ifdef HAVE_ERRNO
	if (errno == ERANGE)
		return OpStatus::ERR_OUT_OF_RANGE;
	if (errno)
		return OpStatus::ERR;
#endif // HAVE_ERRNO
	return end <= start ? OpStatus::ERR : OpStatus::OK;
}

OP_STATUS
OpData::ToUINT64(UINT64 *ret, size_t *length /* = NULL */, int base /* = 10 */, size_t offset /* = 0 */) const
{
	OP_ASSERT(ret);
	if (length)
		*length = 0;

	// Need '\0'-terminated buffer
	const char *buf = Data(true);
	RETURN_OOM_IF_NULL(buf);
	const char *start = buf + offset;
	char *end;
#ifdef HAVE_ERRNO
	errno = 0;
#endif // HAVE_ERRNO
	*ret = op_strtoui64(start, &end, base);
	if (end > start)
	{
		if (length)
			*length = end - start;
		else
		{
			const char *stop = buf + Length();
			while (end < stop && op_isspace(*end))
				end++;
			if (end != stop)
				return OpStatus::ERR;
		}
		while (start < end &&
			   (op_isspace(*start) || *start == '-' || *start == '+'))
			++start;
	}
#ifdef HAVE_ERRNO
	if (errno == ERANGE)
		return OpStatus::ERR_OUT_OF_RANGE;
	if (errno)
		return OpStatus::ERR;
#endif // HAVE_ERRNO
	return end <= start ? OpStatus::ERR : OpStatus::OK;
}
#endif // STDLIB_64BIT_STRING_CONVERSIONS

OP_STATUS
OpData::ToDouble(double *ret, size_t *length /* = NULL */, size_t offset /* = 0 */) const
{
	OP_ASSERT(ret);
	if (length)
		*length = 0;

	// Need '\0'-terminated buffer
	const char *buf = Data(true);
	RETURN_OOM_IF_NULL(buf);
	const char *start = buf + offset;
	char *end;
#ifdef HAVE_ERRNO
	errno = 0;
#endif // HAVE_ERRNO
	*ret = op_strtod(start, &end);
	if (end > start)
	{
		if (length)
			*length = end - start;
		else
		{
			const char *stop = buf + Length();
			while (end < stop && op_isspace(*end))
				end++;
			if (end != stop)
				return OpStatus::ERR;
		}
		while (start < end &&
			   (op_isspace(*start) || *start == '-' || *start == '+'))
			++start;
	}
#ifdef HAVE_ERRNO
	if (errno == ERANGE)
		return OpStatus::ERR_OUT_OF_RANGE;
	if (errno)
		return OpStatus::ERR;
#endif // HAVE_ERRNO
	return end <= start ? OpStatus::ERR : OpStatus::OK;
}

OP_STATUS OpData::AppendFormatted(bool val, bool as_number)
{
	if (as_number) // Append "0" or "1"
		return Append(static_cast<unsigned int>(val));
	return AppendCopyData(val ? "true" : "false");
}

OP_STATUS OpData::AppendFormatted(int val)
{
	char buffer[32]; /* ARRAY OK 2012-04-16 mpawlowski */
	op_snprintf(buffer, sizeof(buffer), "%d", val);
	return AppendCopyData(buffer);
}

OP_STATUS OpData::AppendFormatted(unsigned int val)
{
	char buffer[32]; /* ARRAY OK 2012-04-16 mpawlowski */
	op_snprintf(buffer, sizeof(buffer), "%u", val);
	return AppendCopyData(buffer);
}

OP_STATUS OpData::AppendFormatted(long val)
{
	char buffer[32]; /* ARRAY OK 2012-04-16 mpawlowski */
	op_snprintf(buffer, sizeof(buffer), "%ld", val);
	return AppendCopyData(buffer);
}

OP_STATUS OpData::AppendFormatted(unsigned long val)
{
	char buffer[32]; /* ARRAY OK 2012-04-16 mpawlowski */
	op_snprintf(buffer, sizeof(buffer), "%lu", val);
	return AppendCopyData(buffer);
}

OP_STATUS OpData::AppendFormatted(double val)
{
	char buffer[32]; /* ARRAY OK 2012-04-16 mpawlowski */
	op_snprintf(buffer, sizeof(buffer), "%g", val);
	return AppendCopyData(buffer);
}

OP_STATUS OpData::AppendFormatted(const void* val)
{
	char buffer[32]; /* ARRAY OK 2012-04-16 mpawlowski */
	op_snprintf(buffer, sizeof(buffer), "%p", val);
	return AppendCopyData(buffer);
}

#ifdef OPDATA_DEBUG
class DebugViewHelper : public OpDataFragment::TraverseHelper
{
public:
	DebugViewHelper(char *output, size_t output_length,
		size_t first_offset, size_t last_length,
		DebugViewFormatter *formatter, size_t unit_size)
		: m_output(output)
		, m_output_length(output_length)
		, m_written(0)
		, m_first_offset(first_offset)
		, m_last(NULL)
		, m_last_length(last_length)
		, m_formatter(formatter)
		, m_unit_size(unit_size)
	{}

	/**
	 * Produce the following string from the given fragment:
	 *
	 *   [123+'DATA'+456] ->
	 *
	 * where "123" is m_first_offset (skip this part if m_first_offset is 0;
	 * for any subsequent fragment m_first_offset is implicitly 0),
	 * and "456" is frag->Length() - m_last_length if frag == m_last (i.e.
	 * the number of bytes in the fragment that follow the end of the
	 * OpData object).
	 * The trailing " -> " is skipped for the last fragment.
	 */
	virtual bool ProcessFragment(const OpDataFragment *frag)
	{
		OP_ASSERT(m_last);
		OP_ASSERT(m_formatter);

		// buf must be able to hold size_t in decimal plus '+' and '\0'.
		char buf[sizeof(size_t) * 3 + 2]; // ARRAY OK 2011-08-17 johanh
		int written;

		// determine '123+' prefix
		if (m_first_offset)
		{
#ifdef DEBUG_ENABLE_OPASSERT
			int buf_len =
#endif
			op_snprintf(buf, sizeof(buf), "%lu+",
				static_cast<unsigned long>(m_first_offset / m_unit_size));
			OP_ASSERT(buf_len > 0 && static_cast<size_t>(buf_len) < sizeof(buf));
		}
		else
		{
			buf[0] = '\0';
		}

		// determine data length
		OP_ASSERT(frag->Length() > m_first_offset);
		size_t length = frag->Length() - m_first_offset;
		if (frag == m_last)
		{
			OP_ASSERT(m_last_length - m_first_offset <= length);
			length = m_last_length - m_first_offset;
		}

		// produce initial part of data
		written = op_snprintf(
			m_output + m_written,
			m_output_length - m_written,
			"[%s'", buf);
		OP_ASSERT(0 < written && m_written + written < m_output_length);
		m_written = MIN(m_written + written, m_output_length);

		// write out fragment data
		m_written += m_formatter->FormatData(
			m_output + m_written,
			m_output_length - m_written,
			frag->Data() + m_first_offset,
			length);
		OP_ASSERT(m_written < m_output_length);

		// determine '+456' suffix
		if (frag == m_last && length < frag->Length() - m_first_offset)
		{
#ifdef DEBUG_ENABLE_OPASSERT
			int buf_len =
#endif
			op_snprintf(buf, sizeof(buf), "+%lu",
				static_cast<unsigned long>(((frag->Length() - m_first_offset) - length) / m_unit_size));
			OP_ASSERT(buf_len > 0 && static_cast<size_t>(buf_len) < sizeof(buf));
		}
		else
		{
			buf[0] = '\0';
		}

		// produce final part of fragment
		written = op_snprintf(
			m_output + m_written,
			m_output_length - m_written,
			"'%s]", buf);
		OP_ASSERT(0 < written && m_written + written < m_output_length);
		m_written = MIN(m_written + written, m_output_length);

		if (frag != m_last)
		{
			// produce arrow to next fragment
			written = op_strlcat(m_output + m_written, " -> ",
				m_output_length - m_written);
			OP_ASSERT(0 < written && m_written + written < m_output_length);
			m_written = MIN(m_written + written, m_output_length);
		}

		m_first_offset = 0;
		OP_ASSERT(m_written < m_output_length);
		return frag == m_last;
	}

	virtual void SetTraverseProperties(const OpDataFragment *, const OpDataFragment *last, unsigned int)
	{
		m_last = last;
	}

	char *Output(void) const { return m_output; }

	size_t OutputLength(void) const { return m_output_length; }

	size_t Written(void) const { return m_written; }

private:
	char *m_output; ///< Output string.
	size_t m_output_length; ///< Max number of bytes to write into \c m_output.
	size_t m_written; ///< Number of bytes written into \c m_output.
	size_t m_first_offset; ///< Offset into first fragment.
	const OpDataFragment *m_last; ///< Last fragment.
	size_t m_last_length; ///< Number of bytes used in last fragment.
	DebugViewFormatter *m_formatter; ///< Formatter for data contents.
	size_t m_unit_size; ///< Divide printed sizes by this factor.
};

class FragmentCounter : public OpDataFragment::TraverseHelper
{
public:
	FragmentCounter() : m_num_fragments(0) {}

	virtual bool ProcessFragment(const OpDataFragment *)
	{
		m_num_fragments++;
		return false;
	}

	size_t NumFragments(void) const { return m_num_fragments; }

private:
	size_t m_num_fragments; ///< Counts the number of fragments traversed.
};

char *
OpData::DebugView(DebugViewFormatter *formatter, size_t unit_size /* = 1 */) const
{
	OP_ASSERT(formatter);
	char *ret;
	size_t ret_length;
	size_t data_length = formatter->NeedLength(Length());

	if (IsEmbedded())
	{
		ret_length = 2 + data_length + 2 + 1;
		ret = static_cast<char *>(op_malloc(ret_length));
		if (!ret)
			return NULL;
		size_t i = 0;
		ret[i++] = '{';
		ret[i++] = '\'';
		if (Length())
			i += formatter->FormatData(ret + 2, ret_length - 2, e.data, Length());
		OP_ASSERT(i + 2 + 1 <= ret_length);
		ret[i++] = '\'';
		ret[i++] = '}';
		ret[i++] = '\0';
		OP_ASSERT(i <= ret_length);
		return ret;
	}

	// Count #fragments in object
	FragmentCounter c;
	n.first->Traverse(&c, n.last);

	// Calculate length of output buffer.
	ret_length = data_length +
		c.NumFragments() * 8 + // "[''] -> " per fragment
		2 * 21 + // "12345-" and "-12345" leading/trailing numbers
		1; // '\0'-terminator
	ret = static_cast<char *>(op_malloc(ret_length));
	if (!ret)
		return NULL;

	DebugViewHelper helper(ret, ret_length, n.first_offset, n.last_length,
		formatter, unit_size);
	n.first->Traverse(&helper, n.last);
	OP_ASSERT(helper.Output() == ret && helper.OutputLength() == ret_length);
	OP_ASSERT(helper.Written() < ret_length);
	ret[helper.Written()] = '\0';
	return ret;
}

char *
OpData::DebugView(bool escape_nonprintables /* = true */) const
{
	if (escape_nonprintables)
	{
		DebugViewEscapedByteFormatter formatter;
		return DebugView(&formatter);
	}
	else
	{
		DebugViewRawFormatter formatter;
		return DebugView(&formatter);
	}
}
#endif // OPDATA_DEBUG


/* private: */

OpData::OpData(
	OpDataFragment *first,
	size_t first_offset,
	OpDataFragment *last,
	size_t last_length,
	size_t length)
{
	n.first = first;
	n.first_offset = first_offset;
	n.last = last;
	n.last_length = last_length;
	n.length = length;

	OP_ASSERT(n.first);
	OP_ASSERT(n.last);
	OP_ASSERT(n.first_offset < n.first->Length());
	OP_ASSERT(n.last_length > 0 && n.last_length <= n.last->Length());
	OP_ASSERT(n.length);
	OP_ASSERT(length--); // assignment intended
	OP_ASSERT(n.first->Lookup(&length) == n.last);
}

char *
OpData::GetAppendPtr_NoAlloc(size_t& length)
{
	char *ret;
	size_t avail;
	if (IsEmbedded())
	{
		ret = e.data + Length();
		avail = EmbeddedDataLimit - Length();
		if (!length)
			length = avail;
		else if (avail < length)
			return NULL;
		SetMeta(Length() + length);
		return ret;
	}

	/*
	 * Question: Under which conditions can we append bytes to an OpData
	 * instance in normal mode, without actually allocating any memory?
	 *
	 * In other words: under which conditions can we append bytes into the
	 * last fragment in our fragment list?
	 *
	 * 1. Obviously, to be able to append any bytes at all, there must be
	 *    some free space in the underlying chunk, and the chunk cannot be
	 *    referring to const data.
	 *
	 * 2. Also, the chunk cannot be shared with any other fragments
	 *    (that may already point into that "free space").
	 *
	 * 3. The fragment must be unshared.
	 *
	 * 4. Even if the fragment is shared, there is still some hope: If the
	 *    fragment does not have a following fragment (fragment->Next() ==
	 *    NULL), then we know that this must be the last fragment for _all_
	 *    OpData objects that refer to it. Hence, the other OpData objects
	 *    already have their own n.last_length members limiting how many
	 *    bytes they are using in this last fragment. Therefore, as long as
	 *    we already refer to the entire fragment (our n.last_length ==
	 *    fragment->Length()), we can freely _extend_ the fragment into the
	 *    free space of the underlying chunk, without corrupting the other
	 *    OpData instances pointing to this fragment.
	 *
	 * OpDataFragment::GetAppendPtr_NoAlloc() takes care of the fragment-
	 * and chunk-specific details of this logic. All we need to do, is make
	 * sure that n.last_length refers to the end of the fragment.
	 */

	if (!n.last->IsShared())
		n.last->Trunc(n.last_length);

	if (n.last_length != n.last->Length())
		return NULL;

	ret = n.last->GetAppendPtr_NoAlloc(length);
	if (ret)
	{
		n.last_length += length;
		n.length += length;
	}
	return ret;
}

const char *
OpData::CreatePtr(size_t nuls) const
{
	if (IsEmbedded())
		return nuls ? Consolidate(nuls) : e.data;

	// Need all data in a single fragment.
	if (n.first != n.last)
		return Consolidate(nuls);

	// Can reuse existing fragment if there are enough '\0' bytes.

	// Count '\0' bytes immediately following Length().
	size_t nul_offset = n.last_length;
	size_t nul = 0; // #'\0' bytes found so far.
	while (nul < nuls && n.last->TrySetByte(nul_offset, '\0'))
	{
		nul++;
		nul_offset++;
	}

	// Consolidate if we need more '\0' bytes. Otherwise, use buffer.
	return nul < nuls ? Consolidate(nuls) : n.first->Data() + n.first_offset;
}

char *
OpData::Consolidate(size_t nuls) const
{
	/*
	 * There are 4 different cases of consolidation:
	 * 1. From embedded mode to embedded mode
	 * 2. From embedded mode to normal mode (single chunk)
	 * 3. From normal mode to embedded mode
	 * 4. From normal mode to normal mode (single chunk)
	 *
	 * Case #1 is a no-op (except for the '\0'-termination)
	 * In cases #3 and #4, we copy data from the fragment list in old_frag;
	 * in case #2, old_frag is NULL.
	 * In cases #2 and #4, we copy data into a single chunk, wrapped by the
	 * new_frag fragment. In case #3, we copy into the embedded area, and
	 * new_frag is NULL.
	 */

	// Get case #1 (embedded -> embedded) out of the way
	if (IsEmbedded() && CanEmbed(Length() + nuls))
	{
		size_t nul = 0;
		while (nul < nuls)
			e.data[Length() + nul++] = '\0';
		return e.data;
	}

	OpDataFragment *old_frag = IsEmbedded() ? NULL : n.first;
	OpDataFragment *new_frag = NULL;
	CopyIntoHelper helper(NULL, Length(), IsEmbedded() ? 0 : n.first_offset);

	// Case #3 (normal -> embedded): Set up embedded area as destination
	if (CanEmbed(Length() + nuls))
		helper.SetBuffer(e.data);

	// Cases #2 and #4 (? -> normal): Set up chunk/new_frag as destination
	else
	{
		OpDataChunk *chunk = OpDataChunk::Allocate(Length() + nuls);
		if (!chunk)
			return NULL;
		OP_ASSERT(chunk->IsMutable());
		OP_ASSERT(chunk->Size() >= Length() + nuls);
		new_frag = OpDataFragment::Create(chunk, 0, Length());
		if (!new_frag)
		{
			chunk->Release();
			return NULL;
		}
		helper.SetBuffer(chunk->MutableData());
	}
	OP_ASSERT(helper.Buffer());

	if (old_frag) // Cases #3 and #4: copy from normal mode into helper.Buffer()
	{
		old_frag->Traverse(&helper, NULL,
			OpDataFragment::TRAVERSE_ACTION_RELEASE |
			OpDataFragment::TRAVERSE_INCLUDE_LAST);
		OP_ASSERT(helper.Written() == helper.Length());
	}
	else // Case #2: copying from embedded area into helper.Buffer()
		op_memcpy(helper.Buffer(), e.data, helper.Length());

	// '\0'-terminate
	size_t nul = 0;
	while (nul < nuls)
		helper.Buffer()[helper.Length() + nul++] = '\0';

	if (new_frag) // Cases #2 and #4 (? -> normal): setup normal mode object
	{
		n.first = new_frag;
		n.first_offset = 0;
		n.last = new_frag;
		n.last_length = helper.Length();
		n.length = helper.Length();
	}
	else // Cases #3 (normal -> embedded): setup embedded mode object
		SetMeta(helper.Length());

	return helper.Buffer();
}

void
OpData::ExtendIntoFragment(OpDataFragment *frag, size_t offset, size_t length)
{
	OP_ASSERT(frag);
	OP_ASSERT(offset < frag->Length());
	OP_ASSERT(length);
	OP_ASSERT(offset + length <= frag->Length());

	// See header file declaration for more details on the 3 conditions.

	if (IsEmpty()) // Meets condition #3.
	{
		n.first = frag;
		n.first_offset = offset;
		n.last = frag;
		n.last_length = offset + length;
		n.length = length;
		return;
	}
	if (IsEmbedded())
	{
		// Cannot meet conditions #1 or #2 in embedded mode.
		OP_ASSERT(!"Cannot extend embedded object without allocation");
		return;
	}
	if (frag == n.last && offset == n.last_length)
	{
		// Condition #1: extend into same fragment.
		n.last_length += length;
		n.length += length;
		return;
	}
	if (n.last_length == n.last->Length() && offset == 0)
	{
		if (n.last->Next() == NULL)
			n.last->SetNext(frag);
		if (n.last->Next() == frag)
		{
			// Condition #2: extend into next fragment
			n.last = frag;
			n.last_length = length;
			n.length += length;
			return;
		}
	}
	OP_ASSERT(!"Failed prerequisites for extending this object");
}

OP_STATUS
OpData::AppendFragments(
	OpDataFragment *first,
	size_t first_offset,
	OpDataFragment *last,
	size_t last_length,
	size_t length)
{
	if (IsEmpty())
	{
		// Adopt given fragments directly
		n.first = first;
		n.first_offset = first_offset;
		n.last = last;
		n.last_length = last_length;
		n.length = length;
		first->Traverse(NULL, last,
			OpDataFragment::TRAVERSE_ACTION_USE |
			OpDataFragment::TRAVERSE_INCLUDE_LAST);
		return OpStatus::OK;
	}

	if (IsEmbedded())
	{
		// Place embedded data into its own chunk
		OpDataChunk *chunk = OpDataChunk::Allocate(Length());
		RETURN_OOM_IF_NULL(chunk);
		OP_ASSERT(chunk->IsMutable());
		OP_ASSERT(chunk->Size() >= Length());
		op_memcpy(chunk->MutableData(), e.data, Length());
		OpDataFragment *f = OpDataFragment::Create(chunk, 0, Length());
		if (!f)
		{
			chunk->Release();
			return OpStatus::ERR_NO_MEMORY;
		}

		// Convert from embedded to normal mode pointing at fragment
		n.length = Length();
		n.first = f;
		n.first_offset = 0;
		n.last = f;
		n.last_length = n.length;
	}

	/*
	 * Question: Can we append the given fragments to the existing fragment
	 * list? Or do we need to copy the existing fragment list first?
	 *
	 * In order to append to the existing fragment list, we need to meet
	 * two conditions:
	 *
	 * 1. The fragment list that n.first..n.last is part of, must end at
	 *    n.last. (i.e. n.last->Next() == NULL). If not, we would overwrite
	 *    n.last->Next() with the given fragments, thereby corrupting some
	 *    other OpData instance's fragment list.
	 *    This is however not a problem unless n.last->IsShared(), since we
	 *    can simply force n.last->Next() to NULL in the unshared case.
	 *
	 * 2. We must be using all the data in our last fragment (n.last_length
	 *    == n.last->Length()). If this is not the case, we would silently
	 *    include the rest of this fragment when moving n.last (and
	 *    n.last_length) to point at the new 'last' fragment. Again, if our
	 *    current last fragment is not shared with other OpData instances, this
	 *    is not an issue, since we are free to truncate it to match
	 *    n.last_length.
	 */
	if (!n.last->IsShared())
	{
		n.last->Trunc(n.last_length);
		n.last->SetNext(NULL);
	}

	if (n.last_length != n.last->Length() ||
	    n.last->Next() ||
	    OpDataFragment::ListsOverlap(n.first, n.last, first, last))
	{
		// Cannot append to n.last => must copy fragment list.
		OpDataFragment *new_first, *new_last;
		RETURN_IF_ERROR(OpDataFragment::CopyList(n.first, n.last,
			n.first_offset, n.last_length, &new_first, &new_last));

		// The new fragment list has reset first_offset to 0.
		if (n.first == n.last)
			n.last_length -= n.first_offset;
		OP_ASSERT(n.last_length <= n.last->Length());
		n.first_offset = 0;

		// Release old fragment list.
		n.first->Traverse(NULL, n.last,
			OpDataFragment::TRAVERSE_ACTION_RELEASE |
			OpDataFragment::TRAVERSE_INCLUDE_LAST);

		n.first = new_first;
		n.last = new_last;
	}

	/*
	 * If the given fragment list starts with an offset into the first
	 * fragment, we need to reset that offset somehow, either by rewriting
	 * the first fragment directly (if unshared), or by creating a new
	 * first fragment without an offset that points into the remainder of
	 * the given fragment list.
	 */
	OpDataFragment *replace_first = NULL;
	if (first_offset && !first->IsShared())
	{
		// Can reset offset without reallocating 'first'.
		first->Consume(first_offset);
		// Reset last_length, if necessary.
		if (first == last)
			last_length -= first_offset;
		OP_ASSERT(last_length <= last->Length());
		OP_ASSERT(first->Length());
	}
	else if (first_offset)
	{
		// Must reallocate 'first' fragment.
		size_t new_length = (first == last) ? last_length : first->Length();
		new_length -= first_offset;
		replace_first = OpDataFragment::Create(first->Chunk(),
			first->Offset() + first_offset, new_length);
		RETURN_OOM_IF_NULL(replace_first);
		replace_first->Chunk()->Use();

		// replace_first has reset first_offset to 0.
		// Also replace last if first == last
		if (first == last)
		{
			last_length -= first_offset;
			last = replace_first;
		}

		// Replace 'first' by 'replace_first'.
		replace_first->SetNext(first->Next());
		first = replace_first;
	}

	// Append first..last to n.last.
	OP_ASSERT(n.last_length == n.last->Length());
	OP_ASSERT(!n.last->Next());
	first->Traverse(NULL, last,
		OpDataFragment::TRAVERSE_ACTION_USE |
		OpDataFragment::TRAVERSE_INCLUDE_LAST);
	if (replace_first) // replace_first now has refcount == 2; decrement.
		replace_first->Release();
	n.last->SetNext(first);
	n.last = last;
	n.last_length = last_length;
	n.length += length;
	return OpStatus::OK;
}
