/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
**
*/

#ifndef PROTOBUF_DATA_H
#define PROTOBUF_DATA_H

#ifdef PROTOBUF_SUPPORT

#include "modules/util/adt/bytebuffer.h"
#include "modules/util/tempbuf.h"
#include "modules/unicode/utf8.h"
#include "modules/util/opstring.h"

#include "modules/opdata/OpData.h"
#include "modules/opdata/OpDataFragmentIterator.h"
#include "modules/opdata/UniString.h"
#include "modules/opdata/UniStringFragmentIterator.h"

#include "modules/protobuf/src/protobuf_vector.h"

/**
 * Represent a single chunk in a data buffer.
 */
class OpProtobufDataChunk
{
public:
	OpProtobufDataChunk()
		: data(NULL)
		, length(0)
	{}

	OpProtobufDataChunk(const char *data, size_t len)
		: data(data)
		, length(len)
	{}

	const char *GetData() const { return data; }
	size_t GetLength() const { return length; }

	void SetData(const char *buf) { data = buf; }
	void SetLength(size_t len) { length = len; }

private:
	const char *data;
	size_t length;
};

class ContiguousDataChunkRangeBase
{
public:
	void Construct()
	{
		data = NULL;
		length = 0;
	}

	void Construct(const char *cdata, size_t len)
	{
		data = cdata;
		length = len;
	}

	size_t Length() const { return length; }
	BOOL IsEmpty() const { return Length() == 0; }

	OpProtobufDataChunk Front() const
	{
		OP_ASSERT(!IsEmpty());
		return OpProtobufDataChunk(data, length);
	}

	void PopFront()
	{
		OP_ASSERT(!IsEmpty());
		length = 0;
	}

private:
	const char *data;
	size_t length;
};

class ByteBufferRangeBase;

class ByteBufferChunkRangeBase
{
public:
	void Construct()
	{
		buffer = NULL;
		idx = 0;
		length = 0;
	}

	void Construct(const ByteBuffer *buf)
	{
		OP_ASSERT(buf);
		Construct(*buf);
	}

	void Construct(const ByteBuffer &buf)
	{
		buffer = &buf;
		idx = 0;
		length = buffer->Length();
		Update();
	}

	void Construct(const ByteBuffer *buf, size_t slice_len, size_t offset)
	{
		OP_ASSERT(buf);
		Construct(*buf, slice_len , offset);
	}

	void Construct(const ByteBuffer &buf, size_t slice_len, size_t offset)
	{
		buffer = &buf;
		idx = 0;
		length = buffer->Length();
		Init(slice_len, offset);
	}

	void Construct(const ByteBufferChunkRangeBase &base, size_t slice_len, size_t offset)
	{
		buffer = base.buffer;
		idx = base.idx;
		length = base.length;
		Init(slice_len, offset);
	}

	size_t Length() const { return length; }

	BOOL IsEmpty() const { return length == 0; }

	OpProtobufDataChunk Front() const
	{
		OP_ASSERT(!IsEmpty());
		return OpProtobufDataChunk(cur, MIN(cur_len, length));
	}

	void PopFront()
	{
		OP_ASSERT(!IsEmpty());
		if (cur_len < length)
		{
			length -= cur_len;
			++idx;
			Update();
		}
		else
		{
			length = 0;
		}
	}

private:
	friend class ByteBufferRangeBase;

	void Init(size_t slice_len, size_t offset)
	{
		if (offset > length)
			offset = length;
		if (offset + slice_len > length)
			slice_len = length - offset;
		length = slice_len;
		Advance(offset);
	}
	
	void Advance(size_t offset)
	{
		Update();
		while (cur_len <= offset && !IsEmpty())
		{
			offset -= cur_len;
			++idx;
			Update();
		}
		cur += offset;
		cur_len -= offset;
	}

	void Update()
	{
		if (idx < buffer->GetChunkCount())
		{
			cur = buffer->GetChunk(idx, &cur_len);
			OP_ASSERT(cur);
		}
		else
		{
			length = 0;
		}
	}

	const ByteBuffer *buffer;
	unsigned idx; ///< Index of current chunk
	char *cur; ///< Current chunk
	unsigned cur_len; ///< Length of current chunk
	unsigned length; ///< Current length
};

class OpDataChunkRange
{
public:
	OpDataChunkRange()
		: remaining(0)
	{
	}

	OpDataChunkRange(const OpData &opdata)
		: iterator(opdata)
		, remaining(opdata.Length())
	{
	}

	OpDataChunkRange(const OpData &opdata, size_t slice_len, size_t offset)
	{
		OpData tmp(opdata);
		tmp.Consume(offset);
		tmp.Trunc(slice_len);
		iterator = OpDataFragmentIterator(tmp);
		remaining = tmp.Length();
	}

	size_t Length() const { return remaining; }
	BOOL IsEmpty() const { return remaining == 0; }
	OpProtobufDataChunk Front() const
	{
		OP_ASSERT(!IsEmpty());
		return OpProtobufDataChunk(iterator.GetData(), iterator.GetLength());
	}
	void PopFront()
	{
		OP_ASSERT(!IsEmpty());
		OP_ASSERT(iterator.GetLength() <= remaining);
		remaining -= iterator.GetLength();
		iterator.Next();
	}

private:
	OpDataFragmentIterator iterator;
	size_t remaining;
};

class ByteBufferChunkRange
{
public:
	ByteBufferChunkRange(const ByteBuffer &buf)
	{
		base_range.Construct(buf);
	}

	ByteBufferChunkRange(const ByteBuffer *buf)
	{
		base_range.Construct(buf);
	}

	ByteBufferChunkRange(const ByteBuffer &buf, size_t slice_len, size_t offset)
	{
		base_range.Construct(buf, slice_len, offset);
	}

	ByteBufferChunkRange(const ByteBuffer *buf, size_t slice_len, size_t offset)
	{
		base_range.Construct(buf, slice_len, offset);
	}

	BOOL IsEmpty() const { return base_range.IsEmpty(); }
	OpProtobufDataChunk Front() const { return base_range.Front(); }
	void PopFront() { base_range.PopFront(); }

private:
	ByteBufferChunkRangeBase base_range;
};

class ByteBufferRangeBase
{
public:
	void Construct(const ByteBuffer &buf)
	{
		chunk_range.Construct(buf);
		length = chunk_range.buffer->Length();
		Update();
	}

	void Construct(const ByteBuffer *buf)
	{
		chunk_range.Construct(buf);
		length = chunk_range.buffer->Length();
		Update();
	}

	size_t Length() const { return length; }

	ByteBufferChunkRangeBase ChunkRange() const
	{
		return chunk_range;
	}

	BOOL IsEmpty() const { return chunk_range.IsEmpty(); }

	char Front() const
	{
		OP_ASSERT(!IsEmpty());
		return *cur;
	}

	void PopFront()
	{
		OP_ASSERT(!IsEmpty());
		++cur;
		--length;
		if (cur >= end)
		{
			chunk_range.PopFront();
			Update();
		}
	}

private:
	void Update()
	{
		if (!chunk_range.IsEmpty())
		{
			OpProtobufDataChunk data_chunk = chunk_range.Front();
			chunk = cur = data_chunk.GetData();
			end = cur + data_chunk.GetLength();
		}
	}

	ByteBufferChunkRangeBase chunk_range;
	const char *chunk; ///< Start of current chunk
	const char *cur; ///< Position in current chunk
	const char *end; ///< End of current chunk
	size_t length; ///< Current length
};

class ByteBufferRange
{
public:
	ByteBufferRange(const ByteBuffer &buf)
	{
		base_range.Construct(buf);
	}

	ByteBufferRange(const ByteBuffer *buf)
	{
		base_range.Construct(buf);
	}

	size_t Length() const { return base_range.Length(); }

	BOOL IsEmpty() const { return base_range.IsEmpty(); }
	char Front() const { return base_range.Front(); }
	void PopFront() { return base_range.PopFront(); }

private:
	ByteBufferRangeBase base_range;
};

/**
 * Represents a range that covers all possible chunks in a data buffer object.
 * Certain data objects (such as char *) may have just one chunk.
 *
 */
class OpProtobufDataChunkRange
{
	enum Type
	{
		TYPE_CONTIGUOUS_DATA, // All byte data in one chunk, char *
		TYPE_OPDATA, // OpData
		TYPE_BYTEBUFFER // ByteBuffer
	};

public:
	OpProtobufDataChunkRange(const char *data, size_t length)
		: type(TYPE_CONTIGUOUS_DATA)
	{
		c_data.Construct(data, length);
	}

	OpProtobufDataChunkRange(const OpData &data)
		: type(TYPE_OPDATA)
	{
		opdata = OpDataChunkRange(data);
	}

	OpProtobufDataChunkRange(const OpData &data, size_t slice_len, size_t offset)
		: type(TYPE_OPDATA)
	{
		opdata = OpDataChunkRange(data, slice_len, offset);
	}

	OpProtobufDataChunkRange(const ByteBuffer &buf)
		: type(TYPE_BYTEBUFFER)
	{
		bytebuffer.Construct(buf);
	}

	OpProtobufDataChunkRange(const ByteBuffer &buf, size_t slice_len, size_t offset)
		: type(TYPE_BYTEBUFFER)
	{
		bytebuffer.Construct(buf, slice_len, offset);
	}

	OpProtobufDataChunkRange()
		: type(TYPE_CONTIGUOUS_DATA)
	{
		c_data.Construct();
	}

	size_t Length() const
	{
		switch (type)
		{
		case TYPE_CONTIGUOUS_DATA:
			return c_data.Length();
		case TYPE_OPDATA:
			return opdata.Length();
		case TYPE_BYTEBUFFER:
			return bytebuffer.Length();
		default:
			OP_ASSERT(!"Unknown type, should not happen");
			return 0;
		}
	}

	/**
	 * @return @c TRUE if there are no more chunks in this range.
	 */
	BOOL IsEmpty() const
	{
		switch (type)
		{
		case TYPE_CONTIGUOUS_DATA:
			return c_data.IsEmpty();
		case TYPE_OPDATA:
			return opdata.IsEmpty();
		case TYPE_BYTEBUFFER:
			return bytebuffer.IsEmpty();
		default:
			OP_ASSERT(!"Unknown type, should not happen");
			return TRUE;
		}
	}

	/**
	 * @return The front-most data chunk.
	 * @note Only call this when IsEmpty() returns @c FALSE.
	 */
	OpProtobufDataChunk Front() const
	{
		OP_ASSERT(!IsEmpty());
		switch (type)
		{
		case TYPE_CONTIGUOUS_DATA:
			return c_data.Front();
		case TYPE_OPDATA:
			return opdata.Front();
		case TYPE_BYTEBUFFER:
			return bytebuffer.Front();
		default:
			OP_ASSERT(!"Unknown type, should not happen");
			return OpProtobufDataChunk(NULL, 0);
		}
	}

	/**
	 * Move to the next chunk in the range.
	 * @note Only call this when IsEmpty() returns @c FALSE.
	 */
	void PopFront()
	{
		OP_ASSERT(!IsEmpty());
		switch (type)
		{
		case TYPE_CONTIGUOUS_DATA:
			c_data.PopFront();
			break;
		case TYPE_OPDATA:
			opdata.PopFront();
			break;
		case TYPE_BYTEBUFFER:
			bytebuffer.PopFront();
			break;
		default:
			OP_ASSERT(!"Unknown type, should not happen");
		}
	}

private:
	Type type;
	union
	{
		ContiguousDataChunkRangeBase c_data;
		ByteBufferChunkRangeBase bytebuffer;
	};
	OpDataChunkRange opdata;
};

class ContiguousDataRangeBase
{
public:
	void Construct()
	{
		data = NULL;
		back = NULL;
		front = NULL;
	}

	void Construct(const ContiguousDataRangeBase &helper, size_t len)
	{
		data = helper.data;
		back = helper.front + MIN(len, helper.Length());
		front = helper.front;
	}

	void Construct(const char *d, size_t len)
	{
		data = d;
		back = d + len;
		front = d;
	}

	OpProtobufDataChunkRange ChunkRange() const
	{
		return OpProtobufDataChunkRange(front, Length());
	}

	size_t Length() const
	{
		return back - front;
	}

	int DistanceTo(const ContiguousDataRangeBase &helper) const
	{
		OP_ASSERT(data == helper.data);
		return helper.front - front;
	}

	size_t CopyInto(char *out, size_t len)
	{
		size_t copied = MIN(len, Length());
		op_memcpy(out, front, copied);
		return copied;
	}

	BOOL IsEmpty() const
	{
		return front >= back;
	}

	char Front() const
	{
		OP_ASSERT(!IsEmpty());
		return *front;
	}

	char Back() const
	{
		OP_ASSERT(!IsEmpty());
		return back[-1];
	}

	void PopFront(size_t num)
	{
		if (num > 0)
		{
			OP_ASSERT(num <= Length() && !IsEmpty());
			front += num;
		}
	}

	void PopBack(size_t num)
	{
		if (num > 0)
		{
			OP_ASSERT(num <= Length() && !IsEmpty());
			back -= num;
		}
	}

private:
	const char *data;
	const char *front;
	const char *back;
};

class OpDataRangeBase
{
public:
	void Construct(const OpDataRangeBase &helper, size_t len)
	{
		data = helper.data;
		cur = helper.cur;
		length = MIN(len, helper.length) + cur;
	}

	void Construct(const OpData &d)
	{
		data = &d;
		cur = 0;
		length = d.Length();
	}

	OpProtobufDataChunkRange ChunkRange() const
	{
		return OpProtobufDataChunkRange(*data, Length(), cur);
	}

	size_t Length() const
	{
		return length - cur;
	}

	int DistanceTo(const OpDataRangeBase &helper) const
	{
		OP_ASSERT(data == helper.data);
		return helper.cur - cur;
	}

	size_t CopyInto(char *out, size_t len) const
	{
		return data->CopyInto(out, len, cur);
	}

	BOOL IsEmpty() const
	{
		return cur >= length;
	}

	char Front() const
	{
		OP_ASSERT(!IsEmpty());
		return data->At(cur);
	}

	char Back() const
	{
		OP_ASSERT(!IsEmpty());
		return data->At(length - 1);
	}

	void PopFront(size_t num)
	{
		if (num > 0)
		{
			OP_ASSERT(num <= length && !IsEmpty());
			cur += num;
		}
	}

	void PopBack(size_t num)
	{
		if (num > 0)
		{
			OP_ASSERT(num <= length && !IsEmpty());
			length -= num;
		}
	}

private:
	const OpData *data;
	size_t cur; // Current offset
	size_t length; // Current length
};

/**
 * Represent a single chunk in a string.
 */
class OpProtobufStringChunk
{
public:
	OpProtobufStringChunk(const uni_char *str, size_t len)
		: string(str)
		, length(len)
	{}

	const uni_char *GetString() const { return string; }
	size_t GetLength() const { return length; }

private:
	const uni_char *string;
	size_t length;
};

/**
 * Defines a range of string chunk data which covers a single contiguous
 * string. Used for many string types which use a single contiguous chunk
 * of memory for storing the string data, such uni_char *, OpString and
 * TempBuffer.
 */
class OpUniCharChunkRangeBase
{
public:
	void Construct(const uni_char *string, size_t length)
	{
		front = string;
		back = string + length;
	}

	void Construct()
	{
		front = back = NULL;
	}

	/**
	 * @return @c TRUE if there are no more chunks in this range.
	 */
	BOOL IsEmpty() const
	{
		return front >= back;
	}

	/**
	 * @return The front-most string chunk.
	 * @note Only call this when IsEmpty() returns @c FALSE.
	 */
	OpProtobufStringChunk Front() const
	{
		return OpProtobufStringChunk(front, back - front);
	}

	/**
	 * Move to the next chunk in the range.
	 * @note Only call this when IsEmpty() returns @c FALSE.
	 */
	void PopFront()
	{
		OP_ASSERT(!IsEmpty());
		front = back;
	}

private:
	const uni_char *front, *back;
};

/**
 * Defines a range of string chunk data which a UniString.
 */
class UniStringChunkRange
{
public:
	UniStringChunkRange(const UniString &string)
		: iterator(string)
		, remaining(string.Length())
	{
	}

	UniStringChunkRange()
		: remaining(0)
	{
	}

	/**
	 * @return @c TRUE if there are no more chunks in this range.
	 */
	BOOL IsEmpty() const
	{
		return remaining == 0;
	}

	/**
	 * @return The front-most string chunk.
	 * @note Only call this when IsEmpty() returns @c FALSE.
	 */
	OpProtobufStringChunk Front() const
	{
		OP_ASSERT(!IsEmpty());
		return OpProtobufStringChunk(iterator.GetData(), iterator.GetLength());
	}

	/**
	 * Move to the next chunk in the range.
	 * @note Only call this when IsEmpty() returns @c FALSE.
	 */
	void PopFront()
	{
		OP_ASSERT(!IsEmpty());
		OP_ASSERT(iterator.GetLength() <= remaining);
		remaining -= iterator.GetLength();
		iterator.Next();
		OP_ASSERT(remaining == 0 || iterator.IsValid());
	}

private:
	UniStringFragmentIterator iterator;
	size_t remaining;
};

/**
 * Represents a range that covers all possible chunks in a string object.
 * Certain string objects (such as OpString) may have just one chunk.
 *
 */
class OpProtobufStringChunkRange
{
	enum Type
	{
		TYPE_CONTIGUOUS_STRING16, // All (Unicode) string data in one chunk, uni_string *, OpString etc.
		TYPE_UNISTRING // UniString
	};

public:
	OpProtobufStringChunkRange(const uni_char *string, size_t length)
		: type(TYPE_CONTIGUOUS_STRING16)
	{
		unichar16.Construct(string, length);
	}

	OpProtobufStringChunkRange(const OpString &string)
		: type(TYPE_CONTIGUOUS_STRING16)
	{
		unichar16.Construct(string.CStr(), string.Length());
	}

	OpProtobufStringChunkRange(const TempBuffer &string)
		: type(TYPE_CONTIGUOUS_STRING16)
	{
		unichar16.Construct(string.GetStorage(), string.Length());
	}

	OpProtobufStringChunkRange()
		: type(TYPE_CONTIGUOUS_STRING16)
	{
		unichar16.Construct();
	}

	OpProtobufStringChunkRange(const UniString &string)
		: type(TYPE_UNISTRING)
		, unistring(string)
	{
	}

	/**
	 * @return @c TRUE if there are no more chunks in this range.
	 */
	BOOL IsEmpty() const
	{
		switch (type)
		{
		case TYPE_CONTIGUOUS_STRING16:
			return unichar16.IsEmpty();
		case TYPE_UNISTRING:
			return unistring.IsEmpty();
		default:
			OP_ASSERT(!"Unknown type, should not happen");
			return TRUE;
		}
	}

	/**
	 * @return The front-most string chunk.
	 * @note Only call this when IsEmpty() returns @c FALSE.
	 */
	OpProtobufStringChunk Front() const
	{
		OP_ASSERT(!IsEmpty());
		switch (type)
		{
		case TYPE_CONTIGUOUS_STRING16:
			return unichar16.Front();
		case TYPE_UNISTRING:
			return unistring.Front();
		default:
			OP_ASSERT(!"Unknown type, should not happen");
			return OpProtobufStringChunk(NULL, 0);
		}
	}

	/**
	 * Move to the next chunk in the range.
	 * @note Only call this when IsEmpty() returns @c FALSE.
	 */
	void PopFront()
	{
		OP_ASSERT(!IsEmpty());
		switch (type)
		{
		case TYPE_CONTIGUOUS_STRING16:
			unichar16.PopFront();
			break;
		case TYPE_UNISTRING:
			unistring.PopFront();
			break;
		default:
			OP_ASSERT(!"Unknown type, should not happen");
		}
	}

private:
	Type type;
	union
	{
		OpUniCharChunkRangeBase unichar16;
	};
	UniStringChunkRange unistring;
};

class OpProtobufDataRange
{
	enum Type
	{
		TYPE_CHAR_PTR, // const char */const unsigned char *
		TYPE_OPDATA // const OpData *
	};
public:
	OpProtobufDataRange()
		: type(TYPE_CHAR_PTR)
	{
		u.data_ptr.Construct(NULL, 0);
	}

	OpProtobufDataRange(const OpProtobufDataRange &r, size_t len)
		: type(r.type)
	{
		switch (type)
		{
		case TYPE_CHAR_PTR:
			u.data_ptr.Construct(r.u.data_ptr, len);
			break;
		case TYPE_OPDATA:
			u.opdata.Construct(r.u.opdata, len);
			break;
		default:
			OP_ASSERT(!"Invalid type, should not happen");
		}
	}

	OpProtobufDataRange(const char *data, size_t len)
		: type(TYPE_CHAR_PTR)
	{
		u.data_ptr.Construct(data, len);
	}

	OpProtobufDataRange(const OpData &data)
		: type(TYPE_OPDATA)
	{
		u.opdata.Construct(data);
	}

	OpProtobufDataChunkRange ChunkRange() const
	{
		switch (type)
		{
		case TYPE_CHAR_PTR:
			return u.data_ptr.ChunkRange();
		case TYPE_OPDATA:
			return u.opdata.ChunkRange();
		default:
			OP_ASSERT(!"Invalid type, should not happen");
			return OpProtobufDataChunkRange();
		}
	}

	int DistanceTo(const OpProtobufDataRange &range) const
	{
		OP_ASSERT(type == range.type);
		if (type != range.type)
			return 0;
		switch (type)
		{
		case TYPE_CHAR_PTR:
			return u.data_ptr.DistanceTo(range.u.data_ptr);
		case TYPE_OPDATA:
			return u.opdata.DistanceTo(range.u.opdata);
		default:
			OP_ASSERT(!"Invalid type, should not happen");
			return 0;
		}
	}

	size_t CopyInto(char *out, size_t length)
	{
		OP_ASSERT(out);
		switch (type)
		{
		case TYPE_CHAR_PTR:
			return u.data_ptr.CopyInto(out, length);
		case TYPE_OPDATA:
			return u.opdata.CopyInto(out, length);
		default:
			OP_ASSERT(!"Invalid type, should not happen");
			return 0;
		}
	}

	// Range API
	size_t Length() const
	{
		switch (type)
		{
		case TYPE_CHAR_PTR:
			return u.data_ptr.Length();
		case TYPE_OPDATA:
			return u.opdata.Length();
		default:
			OP_ASSERT(!"Invalid type, should not happen");
			return 0;
		}
	}

	BOOL IsEmpty() const
	{
		switch (type)
		{
		case TYPE_CHAR_PTR:
			return u.data_ptr.IsEmpty();
		case TYPE_OPDATA:
			return u.opdata.IsEmpty();
		default:
			OP_ASSERT(!"Invalid type, should not happen");
			return TRUE;
		}
	}

	char Front() const
	{
		OP_ASSERT(!IsEmpty());
		switch (type)
		{
		case TYPE_CHAR_PTR:
			return u.data_ptr.Front();
		case TYPE_OPDATA:
			return u.opdata.Front();
		default:
			OP_ASSERT(!"Invalid type, should not happen");
			return '\0';
		}
	}

	char Back() const
	{
		OP_ASSERT(!IsEmpty());
		switch (type)
		{
		case TYPE_CHAR_PTR:
			return u.data_ptr.Back();
		case TYPE_OPDATA:
			return u.opdata.Back();
		default:
			OP_ASSERT(!"Invalid type, should not happen");
			return '\0';
		}
	}

	void PopFront(size_t num = 1)
	{
		switch (type)
		{
		case TYPE_CHAR_PTR:
			u.data_ptr.PopFront(num);
			break;
		case TYPE_OPDATA:
			u.opdata.PopFront(num);
			break;
		default:
			OP_ASSERT(!"Invalid type, should not happen");
		}
	}

	void PopBack(size_t num = 1)
	{
		switch (type)
		{
		case TYPE_CHAR_PTR:
			u.data_ptr.PopBack(num);
			break;
		case TYPE_OPDATA:
			u.opdata.PopBack(num);
			break;
		default:
			OP_ASSERT(!"Invalid type, should not happen");
		}
	}

private:
	Type type;

	union
	{
		ContiguousDataRangeBase data_ptr;
		OpDataRangeBase opdata;
	} u;
};

class OpStringOutputRange
{
public:
	OpStringOutputRange(OpString &string) : string(string) {}

	size_t Length() const { return string.Length(); }
	OP_STATUS Put(const OpProtobufStringChunk &chunk) { return string.Append(chunk.GetString(), chunk.GetLength()); }

private:
	OpString &string;
};

class UniStringOutputRange
{
public:
	UniStringOutputRange(UniString &string) : string(string) {}

	size_t Length() const { return string.Length(); }
	OP_STATUS Put(const OpProtobufStringChunk &chunk) { return string.AppendCopyData(chunk.GetString(), chunk.GetLength()); }

private:
	UniString &string;
};

class TempBufferOutputRange
{
public:
	TempBufferOutputRange(TempBuffer &string) : string(string) {}

	size_t Length() const { return string.Length(); }
	OP_STATUS Put(const OpProtobufStringChunk &chunk) { return string.Append(chunk.GetString(), chunk.GetLength()); }

private:
	TempBuffer &string;
};

class OpProtobufStringOutputRange
{
public:
	virtual ~OpProtobufStringOutputRange() {}

	virtual size_t Length() const = 0;
	virtual OP_STATUS Put(const OpProtobufStringChunk &chunk) = 0;
};

class OpProtobufOpStringOutputRange
	: public OpProtobufStringOutputRange
{
public:
	OpProtobufOpStringOutputRange(OpString &string) : range(string) {}

	virtual size_t Length() const {return range.Length();}
	virtual OP_STATUS Put(const OpProtobufStringChunk &chunk) { return range.Put(chunk); }

private:
	OpStringOutputRange range;
};

class OpProtobufUniStringOutputRange
	: public OpProtobufStringOutputRange
{
public:
	OpProtobufUniStringOutputRange(UniString &string) : range(string) {}

	virtual size_t Length() const {return range.Length();}
	virtual OP_STATUS Put(const OpProtobufStringChunk &chunk) { return range.Put(chunk); }

private:
	UniStringOutputRange range;
};

class OpProtobufTempBufferOutputRange
	: public OpProtobufStringOutputRange
{
public:
	OpProtobufTempBufferOutputRange(TempBuffer &string) : range(string) {}

	virtual size_t Length() const {return range.Length();}
	virtual OP_STATUS Put(const OpProtobufStringChunk &chunk) { return range.Put(chunk); }

private:
	TempBufferOutputRange range;
};

class ContigousDataOutputRange
{
public:
	ContigousDataOutputRange(char *data, size_t max_len)
		: data(data)
		, cur(data)
		, end(data + max_len)
	{
	}

	size_t Length() const
	{
		return cur - data;
	}

	size_t Remaining() const { return end - cur; }

	OP_STATUS Put(const OpProtobufDataChunk &chunk)
	{
		OP_ASSERT(chunk.GetLength() <= Remaining());
		if (chunk.GetLength() > Remaining())
			return OpStatus::ERR_OUT_OF_RANGE;
		op_memcpy(cur, chunk.GetData(), chunk.GetLength());
		cur += chunk.GetLength();
		return OpStatus::OK;
	}

private:
	char *data;
	char *cur;
	char *end;
};

class ByteBufferOutputRange
{
public:
	ByteBufferOutputRange(ByteBuffer &buffer)
		: buffer(buffer)
	{
	}

	size_t Length() const
	{
		return buffer.Length();
	}

	OP_STATUS Put(const OpProtobufDataChunk &chunk)
	{
		return buffer.AppendBytes(chunk.GetData(), chunk.GetLength());
	}

private:
	ByteBuffer &buffer;
};

class OpDataOutputRange
{
public:
	OpDataOutputRange(OpData &data)
		: data(data)
	{
	}

	size_t Length() const
	{
		return data.Length();
	}

	OP_STATUS Put(const OpProtobufDataChunk &chunk)
	{
		OpData tmp;
		RETURN_IF_ERROR(tmp.SetCopyData(chunk.GetData(), chunk.GetLength()));
		return data.Append(tmp);
	}

private:
	OpData &data;
};

class OpProtobufDataOutputRange
{
public:
	virtual ~OpProtobufDataOutputRange() {}

	virtual size_t Length() const = 0;
	virtual OP_STATUS Put(const OpProtobufDataChunk &chunk) = 0;
};

class OpProtobufContigousDataOutputRange : public OpProtobufDataOutputRange
{
public:
	OpProtobufContigousDataOutputRange(char *data, size_t max_length) : range(data, max_length) {}

	virtual size_t Length() const { return range.Length(); }
	virtual OP_STATUS Put(const OpProtobufDataChunk &chunk) { return range.Put(chunk); }

private:
	ContigousDataOutputRange range;
};

class OpProtobufByteBufferOutputRange : public OpProtobufDataOutputRange
{
public:
	OpProtobufByteBufferOutputRange(ByteBuffer &buffer)	: range(buffer) {}

	virtual size_t Length() const { return range.Length(); }
	virtual OP_STATUS Put(const OpProtobufDataChunk &chunk) { return range.Put(chunk); }

private:
	ByteBufferOutputRange range;
};

class OpProtobufOpDataOutputRange : public OpProtobufDataOutputRange
{
public:
	OpProtobufOpDataOutputRange(OpData &data) : range(data) {}

	virtual size_t Length() const { return range.Length(); }
	virtual OP_STATUS Put(const OpProtobufDataChunk &chunk) { return range.Put(chunk); }

private:
	OpDataOutputRange range;
};

class OpProtobufStringOutputAdapter
{
public:
	OpProtobufStringOutputAdapter(OpProtobufStringOutputRange *range)
		: output(range)
	{
		OP_ASSERT(range);
	}

	OP_STATUS Put(OpProtobufDataRange range)
	{
		return Put(range.ChunkRange());
	}
	OP_STATUS Put(OpProtobufDataChunkRange range)
	{
		if (range.IsEmpty())
			return OpStatus::OK;

		for (; !range.IsEmpty(); range.PopFront())
		{
			RETURN_IF_ERROR(Put(range.Front()));
		}
		return OpStatus::OK;
	}
	OP_STATUS Put(const OpProtobufDataChunk &chunk);

private:
	OpProtobufStringOutputRange *output;
	UTF8Decoder converter;
	uni_char buf[1024]; // ARRAY OK 2009-05-05 jhoff
};

class OpProtobufDataOutputAdapter
{
public:
	OpProtobufDataOutputAdapter(OpProtobufDataOutputRange *range)
		: output(range)
	{
		OP_ASSERT(range);
	}

	OP_STATUS Put(OpProtobufDataRange range)
	{
		return Put(range.ChunkRange());
	}
	OP_STATUS Put(OpProtobufDataChunkRange range)
	{
		if (range.IsEmpty())
			return OpStatus::OK;

		for (; !range.IsEmpty(); range.PopFront())
		{
			RETURN_IF_ERROR(Put(range.Front()));
		}
		return OpStatus::OK;
	}
	OP_STATUS Put(const OpProtobufDataChunk &chunk)
	{
		return output->Put(chunk);
	}

private:
	OpProtobufDataOutputRange *output;
};

// Array ranges

class OpProtobufStringArrayRange
{
	enum Type
	{
		TYPE_OPSTRING, // OpAutoVector<OpString>
		TYPE_TEMPBUFFER, // OpAutoVector<TempBuffer>
		TYPE_UNISTRING // OpProtobufValueVector<UniString>
	};
public:
	OpProtobufStringArrayRange(const OpAutoVector<OpString> &v)
		: type(TYPE_OPSTRING)
		, idx(0)
	{
		opstring_vector = &v;
	}

	OpProtobufStringArrayRange(const OpAutoVector<TempBuffer> &v)
		: type(TYPE_TEMPBUFFER)
		, idx(0)
	{
		tempbuf_vector = &v;
	}

	OpProtobufStringArrayRange(const OpProtobufValueVector<UniString> &v)
		: type(TYPE_UNISTRING)
		, idx(0)
	{
		unistring_vector = &v;
	}

	size_t GetCount() const
	{
		switch (type)
		{
		case TYPE_OPSTRING: return opstring_vector->GetCount();
		case TYPE_TEMPBUFFER: return tempbuf_vector->GetCount();
		case TYPE_UNISTRING: return unistring_vector->GetCount();
		default:
			OP_ASSERT(!"Invalid type, should not happen");
			return 0;
		}
	}

	// Range API

	BOOL IsEmpty() const
	{
		switch (type)
		{
		case TYPE_OPSTRING: return idx >= opstring_vector->GetCount();
		case TYPE_TEMPBUFFER: return idx >= tempbuf_vector->GetCount();;
		case TYPE_UNISTRING: return idx >= unistring_vector->GetCount();
		default:
			OP_ASSERT(!"Invalid type, should not happen");
			return TRUE;
		}
	}

	OpProtobufStringChunkRange Front()
	{
		OP_ASSERT(!IsEmpty());
		switch (type)
		{
		case TYPE_OPSTRING: return OpProtobufStringChunkRange(*opstring_vector->Get(idx));
		case TYPE_TEMPBUFFER: return OpProtobufStringChunkRange(*tempbuf_vector->Get(idx));
		case TYPE_UNISTRING: return OpProtobufStringChunkRange(unistring_vector->Get(idx));
		default:
			OP_ASSERT(!"Invalid type, should not happen");
			return OpProtobufStringChunkRange();
		}
	}

	void PopFront()
	{
		OP_ASSERT(!IsEmpty());
		++idx;
	}

private:
	Type type;
	unsigned idx;
	union
	{
		const OpAutoVector<OpString> *opstring_vector;
		const OpAutoVector<TempBuffer> *tempbuf_vector;
		const OpProtobufValueVector<UniString> *unistring_vector;
	};
};

class OpProtobufDataArrayRange
{
	enum Type
	{
		TYPE_BYTEBUFFER, // ByteBuffer
		TYPE_OPDATA // OpData
	};
public:
	OpProtobufDataArrayRange(const OpAutoVector<ByteBuffer> &v)
		: type(TYPE_BYTEBUFFER)
		, idx(0)
		, bytebuffer_vector(&v)
	{
	}

	OpProtobufDataArrayRange(const OpProtobufValueVector<OpData> &v)
		: type(TYPE_OPDATA)
		, idx(0)
		, opdata_vector(&v)
	{
	}

	size_t GetCount() const
	{
		switch (type)
		{
		case TYPE_BYTEBUFFER:
			return bytebuffer_vector->GetCount();
		case TYPE_OPDATA:
			return opdata_vector->GetCount();
		default:
			OP_ASSERT(!"Invalid type, should not happen");
			return 0;
		}
	}

	// Range API

	BOOL IsEmpty() const
	{
		switch (type)
		{
		case TYPE_BYTEBUFFER:
			return idx >= bytebuffer_vector->GetCount();
		case TYPE_OPDATA:
			return idx >= opdata_vector->GetCount();
		default:
			OP_ASSERT(!"Invalid type, should not happen");
			return TRUE;
		}
	}

	OpProtobufDataChunkRange Front()
	{
		OP_ASSERT(!IsEmpty());
		switch (type)
		{
		case TYPE_BYTEBUFFER:
			return OpProtobufDataChunkRange(*bytebuffer_vector->Get(idx));
		case TYPE_OPDATA:
			return OpProtobufDataChunkRange(opdata_vector->Get(idx));
		default:
			OP_ASSERT(!"Invalid type, should not happen");
			return OpProtobufDataChunkRange();
		}
	}

	void PopFront()
	{
		OP_ASSERT(!IsEmpty());
		++idx;
	}

private:
	Type type;
	unsigned idx;
	union
	{
		const OpAutoVector<ByteBuffer> *bytebuffer_vector;
		const OpProtobufValueVector<OpData> *opdata_vector;
	};
};
#endif // PROTOBUF_SUPPORT

#endif // OP_PROTOBUF_DATA_H
