/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#include "modules/opdata/UniString.h"
#include "modules/opdata/src/NeedleFinder.h"
#include "modules/opdata/src/SegmentCollector.h"
#include "modules/opdata/src/DebugViewFormatter.h"
#include "modules/otl/list.h"
#include "modules/opdata/UniStringFragmentIterator.h"

UniString::UniString(const UniString& other, size_t offset /* = 0 */, size_t length /* = OpDataFullLength */)
: m_data(other.m_data)
{
	OP_ASSERT(IsUniCharAligned(m_data.Length()));
	if (offset)
		Consume(offset);
	if (length < Length())
		Trunc(length);
	OP_ASSERT(IsUniCharAligned(m_data.Length()));
}

UniString&
UniString::operator=(const UniString& other)
{
	if (this == &other)
		return *this;

	m_data = other.m_data;
	return *this;
}

OP_STATUS
UniString::AppendConstData(
	const uni_char *data,
	size_t length /* = OpDataUnknownLength */)
{
	OP_ASSERT(IsUniCharAligned(data));
	OP_ASSERT(IsUniCharAligned(m_data.Length()));
	size_t alloc = length;
	if (length == OpDataUnknownLength)
	{
		length = uni_strlen(data);
		alloc = length + 1;
	}
	if (!length)
		return OpStatus::OK; // nothing to do

	return m_data.AppendRawData(
		OPDATA_DEALLOC_NONE,
		reinterpret_cast<char *>(const_cast<uni_char *>(data)),
		UNICODE_SIZE(length),
		UNICODE_SIZE(alloc));
}

OP_STATUS
UniString::AppendRawData(
	OpDataDeallocStrategy ds,
	uni_char *data,
	size_t length /* = OpDataUnknownLength */,
	size_t alloc /* = OpDataUnknownLength */)
{
	OP_ASSERT(IsUniCharAligned(data));
	OP_ASSERT(IsUniCharAligned(m_data.Length()));
	if (length == OpDataUnknownLength)
	{
		length = uni_strlen(data);
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
		return OpStatus::OK; // nothing to do

	return m_data.AppendRawData(
		ds,
		reinterpret_cast<char *>(data),
		UNICODE_SIZE(length),
		UNICODE_SIZE(alloc));
}

OP_STATUS
UniString::AppendCopyData(const uni_char *data, size_t length /* = OpDataUnknownLength */)
{
	OP_ASSERT(IsUniCharAligned(data));
	OP_ASSERT(IsUniCharAligned(m_data.Length()));
	if (length == OpDataUnknownLength)
		length = uni_strlen(data);
	if (!length)
		return OpStatus::OK; // nothing to do

	return m_data.AppendCopyData(
		reinterpret_cast<const char *>(data),
		UNICODE_SIZE(length));
}

OP_STATUS
UniString::AppendFormat(const uni_char *format, ...)
{
	va_list args;
	va_start(args, format);
	OP_STATUS result = AppendVFormat(format, args);
	va_end(args);
	return result;
}

OP_STATUS
UniString::AppendVFormat(const uni_char *format, va_list args)
{
	size_t buf_len = 0;
	uni_char *buf = GetAppendPtr_NoAlloc(buf_len);
	OP_ASSERT(buf_len <= Length()); // GetAppendPtr_NoAlloc() adds buf_len to Length()
	int result;

	// first, try to append without allocation
#ifdef va_copy // must use va_copy() in order to preserve args for reuse below
	va_list args_copy;
	va_copy(args_copy, args);
	result = uni_vsnprintf(buf, buf_len, format, args_copy);
	va_end(args_copy);
#else // va_copy
	result = uni_vsnprintf(buf, buf_len, format, args);
#endif // va_copy

	if (result < 0) // uni_vsnprintf() returned error
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
		result = uni_vsnprintf(buf, buf_len, format, args);
	}
	OP_ASSERT(static_cast<size_t>(result) < buf_len);
	Trunc(Length() - (buf_len - result));
	return OpStatus::OK;
}

uni_char *
UniString::GetAppendPtr(size_t length)
{
	uni_char *ret;

	if (!length)
	{
		ret = reinterpret_cast<uni_char *>(m_data.GetAppendPtr(0));
		OP_ASSERT(IsUniCharAligned(ret));
		return ret;
	}

	// first try to satisfy request without allocation
	ret = GetAppendPtr_NoAlloc(length);
	if (ret)
		return ret;

	// must allocate new chunk to satisfy request.
	ret = reinterpret_cast<uni_char *>(m_data.GetAppendPtr(UNICODE_SIZE(length)));
	OP_ASSERT(IsUniCharAligned(ret));
	return ret;
}

bool
UniString::Equals(const uni_char *buf, size_t length /* = OpDataUnknownLength */) const
{
	if (length == OpDataUnknownLength)
		length = uni_strlen(buf);
	if (length != Length())
		return false;
	return StartsWith(buf, length);
}

bool
UniString::EqualsCase(const uni_char* s, size_t length /* = OpDataUnknownLength */) const
{
	if (length == OpDataUnknownLength)
		length = uni_strlen(s);
	if (length != Length())
		return false;
	return StartsWithCase(s, length);
}

bool
UniString::StartsWith(const UniString& other, size_t length /* = OpDataFullLength */) const
{
	length = MIN(length, other.Length());
	if (length > Length())
		return false;
	if (!length)
		return true;
	return m_data.StartsWith(other.m_data, UNICODE_SIZE(length));
}

bool
UniString::StartsWith(const uni_char *buf, size_t length /* = OpDataUnknownLength */) const
{
	OP_ASSERT(buf || length == 0);
	if (length == OpDataUnknownLength)
		length = uni_strlen(buf);
	if (length > Length())
		return false;
	if (!length)
		return true;
	return m_data.StartsWith(reinterpret_cast<const char *>(buf), UNICODE_SIZE(length));
}

inline size_t uni_strnlen(const uni_char* str, size_t max_len)
{
	const uni_char* end = str ? uni_memchr(str, 0, max_len) : NULL;
	return end ? end-str : max_len;
}

bool
UniString::StartsWithCase(const UniString& other, size_t length /* = OpDataFullLength */) const
{
	length = MIN(length, other.Length());
	if (length > Length())
		return false;
	if (!length)
		return true;

	UniStringFragmentIterator this_itr(*this);
	UniStringFragmentIterator other_itr(other);
	size_t this_length = this_itr.GetLength();
	const uni_char* this_data = this_itr.GetData();
	size_t this_str_len = uni_strnlen(this_data, this_length);
	size_t other_length = other_itr.GetLength();
	const uni_char* other_data = other_itr.GetData();
	size_t other_str_len = uni_strnlen(other_data, other_length);
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
		else if (0 != uni_strnicmp(this_data, other_data, to_compare))
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
					this_str_len = uni_strnlen(this_data, this_length);
				else
					this_str_len -= to_compare;
			}
			else
			{
				++this_itr;
				this_length = this_itr.GetLength();
				this_data = this_itr.GetData();
				this_str_len = uni_strnlen(this_data, this_length);
			}

			// advance data of other
			other_length -= to_compare;
			if (other_length > 0)
			{
				other_data += to_compare;
				if (other_str_len == 0)
					other_str_len = uni_strnlen(other_data, other_length);
				else
					other_str_len -= to_compare;
			}
			else
			{
				++other_itr;
				other_length = other_itr.GetLength();
				other_data = other_itr.GetData();
				other_str_len = uni_strnlen(other_data, other_length);
			}
		}
	}

	return true;
}

bool
UniString::StartsWithCase(const uni_char* buf, size_t length /* = OpDataUnknownLength */) const
{
	OP_ASSERT(buf || length == 0);
	if (length == OpDataUnknownLength)
		length = uni_strlen(buf);
	if (length > Length())
		return false;
	if (!length)
		return true;

	UniStringFragmentIterator this_itr(*this);
	size_t this_length = this_itr.GetLength();
	const uni_char* this_data = this_itr.GetData();
	size_t this_str_len = uni_strnlen(this_data, this_length);
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
		else if (0 != uni_strnicmp(this_data, buf, to_compare))
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
					this_str_len = uni_strnlen(this_data, this_length);
				else
					this_str_len -= to_compare;
			}
			else
			{
				++this_itr;
				this_length = this_itr.GetLength();
				this_data = this_itr.GetData();
				this_str_len = uni_strnlen(this_data, this_length);
			}

			// advance buf
			buf += to_compare;
		}
	}

	return true;
}

bool
UniString::EndsWith(const UniString& other, size_t length /* = OpDataFullLength */) const
{
	length = MIN(length, other.Length());
	if (length > Length())
		return false;
	if (!length)
		return true;
	return m_data.EndsWith(other.m_data, UNICODE_SIZE(length));
}

bool
UniString::EndsWith(const uni_char* buf, size_t length /* = OpDataUnknownLength */) const
{
	OP_ASSERT(buf || length == 0);
	if (length == OpDataUnknownLength)
		length = uni_strlen(buf);
	if (length > Length())
		return false;
	if (!length)
		return true;
	return m_data.EndsWith(reinterpret_cast<const char*>(buf), UNICODE_SIZE(length));
}

const uni_char&
UniString::At(size_t index) const
{
	OP_ASSERT(index < Length());
	return *reinterpret_cast<const uni_char *>(&m_data.At(UNICODE_SIZE(index)));
}

size_t
UniString::FindFirst(uni_char needle, size_t offset /* = 0 */, size_t length /* = OpDataFullLength */) const
{
	if (length == OpDataFullLength || offset + length > Length())
		length = offset < Length() ? Length() - offset : 0;
	UniCharFinder ucf(needle);
	size_t result = m_data.Find(&ucf, UNICODE_SIZE(offset), UNICODE_SIZE(length));
	if (result == OpDataNotFound)
		return OpDataNotFound;
	OP_ASSERT(IsUniCharAligned(result));
	result = UNICODE_DOWNSIZE(result);
	OP_ASSERT(At(result) == needle);
	return result;
}

size_t
UniString::FindFirst(
	const uni_char *needle, size_t needle_len /* = OpDataUnknownLength */,
	size_t offset /* = 0 */, size_t length /* = OpDataFullLength */) const
{
	OP_ASSERT(needle);
	if (needle_len == OpDataUnknownLength)
		needle_len = uni_strlen(needle);
	if (!needle_len)
		return offset <= Length() ? offset : OpDataNotFound;

	if (length == OpDataFullLength || offset + length > Length())
		length = offset < Length() ? Length() - offset : 0;
	UniCharSequenceFinder ucsf(needle, needle_len);
	size_t result = m_data.Find(&ucsf, UNICODE_SIZE(offset), UNICODE_SIZE(length));
	if (result == OpDataNotFound)
		return OpDataNotFound;
	OP_ASSERT(IsUniCharAligned(result));
	result = UNICODE_DOWNSIZE(result);
	OP_ASSERT(UniString(*this, result, needle_len).Equals(needle, needle_len));
	return result;
}

size_t
UniString::FindFirstOf(
	const uni_char *accept, size_t accept_len /* = OpDataUnknownLength */,
	size_t offset /* = 0 */, size_t length /* = OpDataFullLength */) const
{
	OP_ASSERT(accept);
	if (accept_len == OpDataUnknownLength)
		accept_len = uni_strlen(accept);
	if (!accept_len)
		return OpDataNotFound;

	if (length == OpDataFullLength || offset + length > Length())
		length = offset < Length() ? Length() - offset : 0;
	UniCharSetFinder usf(accept, accept_len);
	size_t result = m_data.Find(&usf, UNICODE_SIZE(offset), UNICODE_SIZE(length));
	if (result == OpDataNotFound)
		return OpDataNotFound;
	OP_ASSERT(IsUniCharAligned(result));
	result = UNICODE_DOWNSIZE(result);
	OP_ASSERT(usf.Contains(At(result)));
	return result;
}

size_t
UniString::FindEndOf(
	const uni_char *ignore, size_t ignore_len /* = OpDataUnknownLength */,
	size_t offset /* = 0 */, size_t length /* = OpDataFullLength */) const
{
	OP_ASSERT(ignore);
	if (ignore_len == OpDataUnknownLength)
		ignore_len = uni_strlen(ignore);

	if (length == OpDataFullLength || offset + length > Length())
		length = offset < Length() ? Length() - offset : 0;
	UniCharSetComplementFinder uscf(ignore, ignore_len);
	size_t result = m_data.Find(&uscf, UNICODE_SIZE(offset), UNICODE_SIZE(length));
	if (result == OpDataNotFound)
		return OpDataNotFound;
	OP_ASSERT(IsUniCharAligned(result));
	result = UNICODE_DOWNSIZE(result);
	OP_ASSERT(uscf.Contains(At(result)));
	return result;
}

size_t
UniString::FindLast(uni_char needle, size_t offset /* = 0 */, size_t length /* = OpDataFullLength */) const
{
	if (offset > Length())
		return OpDataNotFound;
	if (length == OpDataFullLength || offset + length > Length())
		length = Length() - offset;
	LastUniCharFinder lucf(needle);
	size_t result = m_data.Find(&lucf, UNICODE_SIZE(offset), UNICODE_SIZE(length));
	if (result == OpDataNotFound)
		return OpDataNotFound;
	OP_ASSERT(IsUniCharAligned(result));
	result = UNICODE_DOWNSIZE(result);
	OP_ASSERT(At(result) == needle);
	return result;
}

size_t
UniString::FindLast(
	const uni_char *needle, size_t needle_len /* = OpDataUnknownLength */,
	size_t offset /* = 0 */, size_t length /* = OpDataFullLength */) const
{
	OP_ASSERT(needle);

	if (offset > Length())
		return OpDataNotFound;
	if (length == OpDataFullLength || offset + length > Length())
		length = Length() - offset;

	if (needle_len == OpDataUnknownLength)
		needle_len = uni_strlen(needle);
	if (!needle_len)
		return offset + length;

	if (length == OpDataFullLength || offset + length > Length())
		length = offset < Length() ? Length() - offset : 0;
	LastUniCharSequenceFinder lucsf(needle, needle_len);
	size_t result = m_data.Find(&lucsf, UNICODE_SIZE(offset), UNICODE_SIZE(length));
	if (result == OpDataNotFound)
		return OpDataNotFound;
	OP_ASSERT(IsUniCharAligned(result));
	result = UNICODE_DOWNSIZE(result);
	OP_ASSERT(UniString(*this, result, needle_len).Equals(needle, needle_len));
	return result;
}

void
UniString::Consume(size_t length)
{
	if (!length)
		return; // nothing to do
	if (length >= Length())
	{
		Clear(); // consume everything
		return;
	}

	m_data.Consume(UNICODE_SIZE(length));
}

void
UniString::Trunc(size_t length)
{
	if (!length)
		return Clear(); // truncate to 0
	else if (length >= Length())
		return; // nothing to do

	m_data.Trunc(UNICODE_SIZE(length));
}

void
UniString::LStrip(void)
{
	size_t i = 0;
	while (i < Length() && uni_isspace(At(i)))
		i++;
	Consume(i);
}

void
UniString::RStrip(void)
{
	size_t i = Length();
	while (i && uni_isspace(At(i - 1)))
		i--;
	Trunc(i);
}

void
UniString::Strip(void)
{
	LStrip();
	RStrip();
}

OP_STATUS
UniString::Remove(uni_char remove, size_t maxremove /* = OpDataFullLength */)
{
	OpData result;
	UniCharFinder sepf(remove);
	Concatenator coll(result);
	RETURN_IF_ERROR(m_data.Split(&sepf, &coll, maxremove));
	m_data = result;
	OP_ASSERT(IsUniCharAligned(m_data.Length()));
	return OpStatus::OK;
}

OP_STATUS
UniString::Remove(const uni_char *remove, size_t length /* = OpDataUnknownLength */, size_t maxremove /* = OpDataFullLength */)
{
	OP_ASSERT(remove);
	if (length == OpDataUnknownLength)
		length = uni_strlen(remove);

	OpData result;
	UniCharSequenceFinder sepf(remove, length);
	Concatenator coll(result);
	RETURN_IF_ERROR(m_data.Split(&sepf, &coll, maxremove));
	m_data = result;
	OP_ASSERT(IsUniCharAligned(m_data.Length()));
	return OpStatus::OK;
}

OP_STATUS
UniString::Replace(uni_char character, uni_char replacement, size_t maxreplace, size_t* replaced)
{
	OpData result;
	UniCharFinder sepf(character);
	OpData replacement_data;
	RETURN_IF_ERROR(replacement_data.SetCopyData(reinterpret_cast<const char *>(&replacement), sizeof(uni_char)));
	ReplaceConcatenator coll(result, replacement_data, m_data.Length());
	RETURN_IF_ERROR(m_data.Split(&sepf, &coll, maxreplace));
	coll.Done();
	m_data = result;
	if (replaced) *replaced = coll.CountReplacement();
	OP_ASSERT(IsUniCharAligned(m_data.Length()));
	return OpStatus::OK;
}

OP_STATUS
UniString::Replace(const uni_char* substring, const uni_char* replacement, size_t maxreplace, size_t* replaced)
{
	if (!substring || !*substring)
		return OpStatus::ERR;

	OpData result;
	size_t substring_length = uni_strlen(substring);
	UniCharSequenceFinder sepf(substring, substring_length);
	size_t replacement_length = replacement ? uni_strlen(replacement) : 0;
	OpData replacement_data;
	RETURN_IF_ERROR(replacement_data.SetCopyData(reinterpret_cast<const char *>(replacement), sizeof(uni_char) * replacement_length));
	ReplaceConcatenator coll(result, replacement_data);
	RETURN_IF_ERROR(m_data.Split(&sepf, &coll, maxreplace));
	m_data = result;
	if (replaced) *replaced = coll.CountReplacement();
	OP_ASSERT(IsUniCharAligned(m_data.Length()));
	return OpStatus::OK;
}

OtlCountedList<UniString> *
UniString::Split(uni_char sep, size_t maxsplit /* = OpDataFullLength */) const
{
	OtlCountedList<UniString> *ret = OP_NEW(OtlCountedList<UniString>, ());
	if (!ret)
		return NULL;
	UniCharFinder sepf(sep);
	CountedUniStringCollector coll(ret);
	if (OpStatus::IsError(m_data.Split(&sepf, &coll, maxsplit)))
	{
		OP_DELETE(ret);
		return NULL;
	}
	return ret;
}

OtlCountedList<UniString> *
UniString::Split(const uni_char *sep, size_t length /* = OpDataUnknownLength */, size_t maxsplit /* = OpDataFullLength */) const
{
	OP_ASSERT(sep);
	if (length == OpDataUnknownLength)
		length = uni_strlen(sep);

	OtlCountedList<UniString> *ret = OP_NEW(OtlCountedList<UniString>, ());
	if (!ret)
		return NULL;
	UniCharSequenceFinder sepf(sep, length);
	CountedUniStringCollector coll(ret);
	if (OpStatus::IsError(m_data.Split(&sepf, &coll, maxsplit)))
	{
		OP_DELETE(ret);
		return NULL;
	}
	return ret;
}

OtlCountedList<UniString>*
UniString::SplitAtAnyOf(const uni_char* sep, size_t length, size_t maxsplit) const
{
	OP_ASSERT(sep);
	if (length == OpDataUnknownLength)
		length = uni_strlen(sep);

	OpAutoPtr< OtlCountedList<UniString> > ret(OP_NEW(OtlCountedList<UniString>, ()));
	RETURN_VALUE_IF_NULL(ret.get(), NULL);
	UniCharSetFinder sepf(sep, length);
	CountedUniStringCollector coll(ret.get());
	RETURN_VALUE_IF_ERROR(m_data.Split(&sepf, &coll, maxsplit), NULL);
	return ret.release();
}

size_t
UniString::CopyInto(uni_char *ptr, size_t length, size_t offset /* = 0 */) const
{
	if (length + offset > Length())
		length = Length() - offset;
	size_t result = m_data.CopyInto(reinterpret_cast<char *>(ptr),
		UNICODE_SIZE(length), UNICODE_SIZE(offset));
	(void) result;
	OP_ASSERT(result == UNICODE_SIZE(length));
	return length;
}

const uni_char *
UniString::Data(bool nul_terminated /* = false */) const
{
	if (nul_terminated && Length() && At(Length() - 1) == '\0')
		nul_terminated = false; // already '\0'-terminated
	const char *ret = m_data.CreatePtr(nul_terminated ? UNICODE_SIZE(1) : 0);
	OP_ASSERT(IsUniCharAligned(ret));
	return reinterpret_cast<const uni_char *>(ret);
}

OP_STATUS
UniString::CreatePtr(const uni_char **ptr, size_t nul_terminators /* = 0 */) const
{
	// First, count the '\0' terminators _within_ the buffer.
	size_t i = Length();
	while (nul_terminators && i && At(i - 1) == '\0')
	{
		i--;
		nul_terminators--;
	}

	const char *ret = m_data.CreatePtr(UNICODE_SIZE(nul_terminators));
	RETURN_OOM_IF_NULL(ret);
	OP_ASSERT(IsUniCharAligned(ret));
	*ptr = reinterpret_cast<const uni_char *>(ret);
	return OpStatus::OK;
}

OP_STATUS
UniString::ToShort(short *ret, size_t *length /* = NULL */, int base /* = 10 */, size_t offset /* = 0 */) const
{
	long l;
	RETURN_IF_ERROR(ToLong(&l, length, base, offset));
	if (l < SHRT_MIN || l > SHRT_MAX)
		return OpStatus::ERR_OUT_OF_RANGE;
	*ret = static_cast<short>(l);
	return OpStatus::OK;
}

OP_STATUS
UniString::ToInt(int *ret, size_t *length /* = NULL */, int base /* = 10 */, size_t offset /* = 0 */) const
{
	long l;
	RETURN_IF_ERROR(ToLong(&l, length, base, offset));
	if (l < INT_MIN || l > INT_MAX)
		return OpStatus::ERR_OUT_OF_RANGE;
	*ret = static_cast<int>(l);
	return OpStatus::OK;
}

OP_STATUS
UniString::ToLong(long *ret, size_t *length /* = NULL */, int base /* = 10 */, size_t offset /* = 0 */) const
{
	OP_ASSERT(ret);
	if (length)
		*length = 0;

	// Need '\0'-terminated buffer
	const uni_char *buf = Data(true);
	RETURN_OOM_IF_NULL(buf);
	const uni_char *start = buf + offset;
	uni_char *end;
#ifdef HAVE_ERRNO
	errno = 0;
#endif // !HAVE_ERRNO
	*ret = uni_strtol(start, &end, base);
	if (end > start)
	{
		if (length)
			*length = end - start;
		else
		{
			const uni_char *stop = buf + Length();
			while (end < stop && uni_isspace(*end))
				end++;
			if (end != stop)
				return OpStatus::ERR;
		}
		while (start < end &&
			   (uni_isspace(*start) || *start == '-' || *start == '+'))
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
UniString::ToUShort(unsigned short *ret, size_t *length /* = NULL */, int base /* = 10 */, size_t offset /* = 0 */) const
{
	unsigned long l;
	RETURN_IF_ERROR(ToULong(&l, length, base, offset));
	if (l > USHRT_MAX)
		return OpStatus::ERR_OUT_OF_RANGE;
	*ret = static_cast<unsigned short>(l);
	return OpStatus::OK;
}

OP_STATUS
UniString::ToUInt(unsigned int *ret, size_t *length /* = NULL */, int base /* = 10 */, size_t offset /* = 0 */) const
{
	unsigned long l;
	RETURN_IF_ERROR(ToULong(&l, length, base, offset));
	if (l > UINT_MAX)
		return OpStatus::ERR_OUT_OF_RANGE;
	*ret = static_cast<unsigned int>(l);
	return OpStatus::OK;
}

OP_STATUS
UniString::ToULong(unsigned long *ret, size_t *length /* = NULL */, int base /* = 10 */, size_t offset /* = 0 */) const
{
	OP_ASSERT(ret);
	if (length)
		*length = 0;

	// Need '\0'-terminated buffer
	const uni_char *buf = Data(true);
	RETURN_OOM_IF_NULL(buf);
	const uni_char *start = buf + offset;
	uni_char *end;
#ifdef HAVE_ERRNO
	errno = 0;
#endif // !HAVE_ERRNO
	*ret = uni_strtoul(start, &end, base);
	if (end > start)
	{
		if (length)
			*length = end - start;
		else
		{
			const uni_char *stop = buf + Length();
			while (end < stop && uni_isspace(*end))
				end++;
			if (end != stop)
				return OpStatus::ERR;
		}
		while (start < end &&
			   (uni_isspace(*start) || *start == '-' || *start == '+'))
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
UniString::ToINT64(INT64 *ret, size_t *length /* = NULL */, int base /* = 10 */, size_t offset /* = 0 */) const
{
	OP_ASSERT(ret);
	if (length)
		*length = 0;

	// Need '\0'-terminated buffer
	const uni_char *buf = Data(true);
	RETURN_OOM_IF_NULL(buf);
	const uni_char *start = buf + offset;
	uni_char *end;
#ifdef HAVE_ERRNO
	errno = 0;
#endif // !HAVE_ERRNO
	*ret = uni_strtoi64(start, &end, base);
	if (end > start)
	{
		if (length)
			*length = end - start;
		else
		{
			const uni_char *stop = buf + Length();
			while (end < stop && uni_isspace(*end))
				end++;
			if (end != stop)
				return OpStatus::ERR;
		}
		while (start < end &&
			   (uni_isspace(*start) || *start == '-' || *start == '+'))
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
UniString::ToUINT64(UINT64 *ret, size_t *length /* = NULL */, int base /* = 10 */, size_t offset /* = 0 */) const
{
	OP_ASSERT(ret);
	if (length)
		*length = 0;

	// Need '\0'-terminated buffer
	const uni_char *buf = Data(true);
	RETURN_OOM_IF_NULL(buf);
	const uni_char *start = buf + offset;
	uni_char *end;
#ifdef HAVE_ERRNO
	errno = 0;
#endif // HAVE_ERRNO
	*ret = uni_strtoui64(start, &end, base);
	if (end > start)
	{
		if (length)
			*length = end - start;
		else
		{
			const uni_char *stop = buf + Length();
			while (end < stop && uni_isspace(*end))
				end++;
			if (end != stop)
				return OpStatus::ERR;
		}
		while (start < end &&
			   (uni_isspace(*start) || *start == '-' || *start == '+'))
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
UniString::ToDouble(double *ret, size_t *length /* = NULL */, size_t offset /* = 0 */) const
{
	OP_ASSERT(ret);
	if (length)
		*length = 0;

	// Need '\0'-terminated buffer
	const uni_char *buf = Data(true);
	RETURN_OOM_IF_NULL(buf);
	const uni_char *start = buf + offset;
	uni_char *end;
#ifdef HAVE_ERRNO
	errno = 0;
#endif // !HAVE_ERRNO
	*ret = uni_strtod(start, &end);
	if (end > start)
	{
		if (length)
			*length = end - start;
		else
		{
			const uni_char *stop = buf + Length();
			while (end < stop && uni_isspace(*end))
				end++;
			if (end != stop)
				return OpStatus::ERR;
		}
		while (start < end &&
			   (uni_isspace(*start) || *start == '-' || *start == '+'))
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

OP_STATUS UniString::AppendFormatted(bool val, bool as_number)
{
	if (as_number) // Append "0" or "1"
		return Append(static_cast<unsigned int>(val));
	return AppendCopyData(val ? UNI_L("true") : UNI_L("false"));
}

OP_STATUS UniString::AppendFormatted(int val)
{
	const size_t length = 32;
	uni_char buffer[length]; /* ARRAY OK 2012-04-16 mpawlowski */
	uni_snprintf(buffer, length, UNI_L("%d"), val);
	return AppendCopyData(buffer);
}

OP_STATUS UniString::AppendFormatted(unsigned int val)
{
	const size_t length = 32;
	uni_char buffer[length]; /* ARRAY OK 2012-04-16 mpawlowski */
	uni_snprintf(buffer, length, UNI_L("%u"), val);
	return AppendCopyData(buffer);
}

OP_STATUS UniString::AppendFormatted(long val)
{
	const size_t length = 32;
	uni_char buffer[length]; /* ARRAY OK 2012-04-16 mpawlowski */
	uni_snprintf(buffer, length, UNI_L("%ld"), val);
	return AppendCopyData(buffer);
}

OP_STATUS UniString::AppendFormatted(unsigned long val)
{
	const size_t length = 32;
	uni_char buffer[length]; /* ARRAY OK 2012-04-16 mpawlowski */
	uni_snprintf(buffer, length, UNI_L("%lu"), val);
	return AppendCopyData(buffer);
}

OP_STATUS UniString::AppendFormatted(double val)
{
	const size_t length = 32;
	uni_char buffer[length]; /* ARRAY OK 2012-04-16 mpawlowski */
	uni_snprintf(buffer, length, UNI_L("%g"), val);
	return AppendCopyData(buffer);
}

OP_STATUS UniString::AppendFormatted(const void* val)
{
	const size_t length = 32;
	uni_char buffer[length]; /* ARRAY OK 2012-04-16 mpawlowski */
	uni_snprintf(buffer, length, UNI_L("%p"), val);
	return AppendCopyData(buffer);
}

#ifdef OPDATA_DEBUG
char *
UniString::DebugView(void) const
{
	DebugViewEscapedUniCharFormatter formatter;
	return m_data.DebugView(&formatter, UNICODE_SIZE(1));
}
#endif // OPDATA_DEBUG

/* private: */

uni_char *
UniString::GetAppendPtr_NoAlloc(size_t& length)
{
	size_t byte_length = UNICODE_SIZE(length);
	char *ret = m_data.GetAppendPtr_NoAlloc(byte_length);
	if (!length)
		length = UNICODE_DOWNSIZE(byte_length);
	OP_ASSERT(IsUniCharAligned(ret));
	return reinterpret_cast<uni_char *>(ret);
}
