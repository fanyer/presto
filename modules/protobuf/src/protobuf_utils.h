/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#ifndef OP_PROTOBUF_UTILS_H
#define OP_PROTOBUF_UTILS_H

#ifdef PROTOBUF_SUPPORT

#include "modules/protobuf/src/opvaluevector.h"
#include "modules/protobuf/src/protobuf_data.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/adt/bytebuffer.h"
#include "modules/util/opstring.h"

class ByteBuffer;
class TempBuffer;
class OpString8;
class UniString;
template<typename T> class OpValueVector;
template<typename T> class OpProtobufValueVector;

/**
 * ADS 1.2 workaround to avoid problems with using
 * a function pointer argument to a templated function.
 * In particular it seem to be the uni_char** that 
 * cause trouble and leads to:
 * Serious error: C2408E: type deduction fails: function type 0-way resolvable
 */
template<typename T>
class ConvFunction
{
public:
	ConvFunction( T (*f)(const uni_char *, uni_char**, int, BOOL *) ) : strtol_f(f) {}
	T (*strtol_f)(const uni_char *, uni_char**, int, BOOL *);
};

struct OpProtobufUtils
{
	// This is a helper class which binds a uni_char pointer and a length into one object,
	// this allows it to be used as parameter for the Copy() function.
	class UniStringPtr
	{
	public:
		UniStringPtr(const uni_char *str_, unsigned int len_)
			: str(str_)
			, len(len_)
		{}

		const uni_char *CStr() const { return str; }
		unsigned int Length() const { return len; }

	private:
		const uni_char *str;
		unsigned int    len;
	};

	// This is a helper class which binds a char pointer and a length into one object,
	// this allows it to be seen as binary data and not a C string.
	class Bytes
	{
	public:
		Bytes(const char *data, unsigned int len)
			: data(data)
			, len(len)
		{}

		const char *Data() const { return data; }
		unsigned int Length() const { return len; }

	private:
		const char   *data;
		unsigned int  len;
	};

	// TODO: Add max_len to all Copy() funcs
	static inline OP_STATUS Copy(const TempBuffer *from, ByteBuffer *to);
	static OP_STATUS Copy(const TempBuffer &from, ByteBuffer &to);
	template<typename T>
	static OP_STATUS Copy(const UniStringPtr &from, T &to, unsigned int max_len = ~0u);
	static OP_STATUS Copy(OpProtobufStringChunkRange from, TempBuffer &to);

	static OP_STATUS Convert(ByteBuffer &out, const uni_char *in_str, int char_count = -1);
	static OP_STATUS Convert(OpString &out, const char *in_str, int char_count = -1);
	static OP_STATUS ConvertUTF8toUTF16(TempBuffer &out, const ByteBuffer &in, int char_count = -1);
	static OP_STATUS ConvertUTF8toUTF16(ByteBuffer &out, const ByteBuffer &in, int char_count = -1);

	static OP_STATUS ExtractUTF16BE(OpString &out, const ByteBuffer &in, int char_count = -1);

	/**
	 * Append a new string to container @a container containing OpString
	 * objects. @a string is copied to a new OpString object and added to
	 * the container.
	 *
	 * @param container A container with OpString objects. Must have an Add(OpString *) method.
	 * @param string The string to place in container.
	 * @return OK for success, ERR_NO_MEMORY on OOM.
	 */
	template<typename C>
	static OP_STATUS AppendString(C &container, const OpString &string);

	/**
	 * Similar to AppendString(C &, const OpString &) but takes a Unicode string pointer and length.
	 *
	 * @param container A container with OpString objects. Must have an Add(OpString *) method.
	 * @param string The string data to place in container.
	 * @param len The amount of characters to copy from @a string or @c KAll to use the whole string.
	 * @return OK for success, ERR_NO_MEMORY on OOM.
	 */
	template<typename C>
	static OP_STATUS AppendString(C &container, const uni_char *string, int len = KAll);

	/**
	 * Similar to AppendString(C &, const OpString &) but takes a C-string pointer and length.
	 * See OpString::Set() for details on how the C-string is converted.
	 *
	 * @param container A container with OpString objects. Must have an Add(OpString *) method.
	 * @param string The string data to place in container.
	 * @param len The amount of characters to copy from @a string or @c KAll to use the whole string.
	 * @return OK for success, ERR_NO_MEMORY on OOM.
	 */
	template<typename C>
	static OP_STATUS AppendString(C &container, const char *string, int len = KAll);

	/**
	 * Append a new string to container @a container containing TempBuffer
	 * objects. @a string is copied to a new TempBuffer object and added to
	 * the container.
	 *
	 * @param container A container with TempBuffer objects. Must have an Add(TempBuffer *) method.
	 * @param string The string to place in container.
	 * @return OK for success, ERR_NO_MEMORY on OOM.
	 */
	template<typename C>
	static OP_STATUS AppendTempBuffer(C &container, const TempBuffer &string);

	/**
	 * Similar to AppendTempBuffer(C &, const OpString &) but takes a Unicode string pointer and length.
	 *
	 * @param container A container with TempBuffer objects. Must have an Add(TempBuffer *) method.
	 * @param string The string data to place in container.
	 * @param len The amount of characters to copy from @a string or @c KAll to use the whole string.
	 * @return OK for success, ERR_NO_MEMORY on OOM.
	 */
	template<typename C>
	static OP_STATUS AppendTempBuffer(C &container, const uni_char *string, int len = KAll);

	/**
	 * Similar to AppendString(C &, const TempBuffer &) but takes a C-string pointer and length.
	 * See TempBuffer::Append() for details on how the C-string is converted.
	 *
	 * @param container A container with TempBuffer objects. Must have an Add(TempBuffer *) method.
	 * @param string The string data to place in container.
	 * @param len The amount of characters to copy from @a string or @c KAll to use the whole string.
	 * @return OK for success, ERR_NO_MEMORY on OOM.
	 */
	template<typename C>
	static OP_STATUS AppendTempBuffer(C &container, const char *string, int len = KAll);

	/**
	 * Append a new string to UniString container @a container.
	 *
	 * @param container A value vector with UniString objects.
	 * @param string The string to place in container.
	 * @return OK for success, ERR_NO_MEMORY on OOM.
	 */
	static OP_STATUS AppendUniString(OpProtobufValueVector<UniString> &container, const UniString &string);

	/**
	 * Similar to AppendUniString(OpProtobufValueVector<UniString> &, const UniString &) but takes a Unicode string pointer and length.
	 *
	 * @param container A container with UniString objects. Must have an Add(UniString) method.
	 * @param string The string data to place in container.
	 * @param len The amount of characters to copy from @a string or @c KAll to use the whole string.
	 * @return OK for success, ERR_NO_MEMORY on OOM.
	 */
	static OP_STATUS AppendUniString(OpProtobufValueVector<UniString> &container, const uni_char *string, int len = KAll);

	/**
	 * Similar to AppendUniString(OpProtobufValueVector<UniString> &, const UniString &) but takes a C-string pointer and length.
	 * See TempBuffer::Append() for details on how the C-string is converted.
	 *
	 * @param container A container with UniString objects. Must have an Add(UniString) method.
	 * @param string The string data to place in container.
	 * @param len The amount of characters to copy from @a string or @c KAll to use the whole string.
	 * @return OK for success, ERR_NO_MEMORY on OOM.
	 */
	static OP_STATUS AppendUniString(OpProtobufValueVector<UniString> &container, const char *string, int len = KAll);

	/**
	 * Append a new bytebuffer to container @a container containing ByteBuffer
	 * objects. @a bytes is copied to a new ByteBuffer object and added to
	 * the container.
	 *
	 * @param container A container with ByteBuffer objects. Must have an Add(ByteBuffer *) method.
	 * @param bytes The byte data to place in container.
	 * @return OK for success, ERR_NO_MEMORY on OOM.
	 */
	template<typename C>
	static OP_STATUS AppendBytes(C &container, const ByteBuffer &bytes);

	/**
	 * Similar to AppendBytes(C &, const ByteBuffer &) but takes a pointer to
	 * byte data instead.
	 *
	 * @param container A container with ByteBuffer objects. Must have an Add(ByteBuffer *) method.
	 * @param bytes The byte data to place in container.
	 * @param len The amount of bytes to copy from @a bytes.
	 * @return OK for success, ERR_NO_MEMORY on OOM.
	 */
	template<typename C>
	static OP_STATUS AppendBytes(C &container, const char *bytes, int len);

	/**
	 * Append a new OpData to container @a container containing OpData
	 * objects. @a bytes is copied to a new OpData object and added to
	 * the container.
	 *
	 * @param container A container with OpData objects. Must have an Add(OpData *) method.
	 * @param bytes The byte data to place in container.
	 * @return OK for success, ERR_NO_MEMORY on OOM.
	 */
	template<typename C>
	static OP_STATUS AppendOpData(C &container, const OpData &bytes);

	/**
	 * Similar to AppendOpData(C &, const OpData &) but takes a pointer to
	 * byte data instead.
	 *
	 * @param container A container with OpData objects. Must have an Add(OpData *) method.
	 * @param bytes The byte data to place in container.
	 * @param len The amount of bytes to copy from @a bytes.
	 * @return OK for success, ERR_NO_MEMORY on OOM.
	 */
	template<typename C>
	static OP_STATUS AppendOpData(C &container, const char *bytes, int len);

	template<typename T>
	static OP_STATUS Append(T &col, int num);
	template<typename T>
	static inline OP_STATUS Append(T &col, unsigned int num);
	static inline OP_STATUS Append(TempBuffer &col, int num);
	static inline OP_STATUS Append(TempBuffer &col, unsigned int num);
	static inline OP_STATUS Append(TempBuffer &col, const uni_char *in_str, unsigned int max_len = ~0u);
	static inline OP_STATUS Append(OpString8 &col, const char *in_str, unsigned int max_len = ~0u);
	static inline OP_STATUS Append(OpString &col, const char *in_str, unsigned int max_len = ~0u);
	static inline OP_STATUS Append(OpString &col, const Bytes &in_str, unsigned int max_len = ~0u);
	static inline OP_STATUS Append(ByteBuffer &col, const char *in_str, unsigned int max_len = ~0u);
	static inline OP_STATUS Append(ByteBuffer &col, const Bytes &in_bytes, unsigned int max_len = ~0u);

	template<typename T>
	static inline int Length(const OpValueVector<T> &v);
	template<typename T>
	static inline int Length(const OpVector<T> &v);
	static inline int Length(const OpINT32Vector &v);

	static int ParseDelimitedInteger(const uni_char *buffer, unsigned int buffer_len, uni_char delimiter, int &chars_read);

	// missing functions from stdlib
	static char *uitoa( unsigned int value, char *str, int radix);

	/**
	 * Parse a long integer using the specified function (e.g. uni_strtol/uni_strtoul).
	 * 
	 * @param text The text to parse. 
	 * @param strtol_f The function used to parse the number (uni_strtol/uni_strtoul).
	 * @param min If the parsed value is less than 'min', this function fails.
	 * @param min If the parsed value is greater than 'max', this function fails.
	 * @param number The resulting number will be stored in this variable on success.
	 * @param invalid_first If the first character of the text is equal to this value, parsing fails.
	 */
	template<typename T>
	static OP_STATUS ParseLong(const uni_char *text, ConvFunction<T> f, T min, T max, T &number, char invalid_first = '\0');

#ifndef OPERA_BIG_ENDIAN
	static void ByteSwap(uni_char* buf, size_t buf_len);
#endif // OPERA_BIG_ENDIAN
};

/*static*/ inline
OP_STATUS
OpProtobufUtils::Copy(const TempBuffer *from, ByteBuffer *to)
{
	OP_ASSERT(from != NULL || to != NULL);
	if (from == NULL || to == NULL)
		return OpStatus::ERR;
	return Copy(*from, *to);
}

// Note: This method cannot be put as static member of a class, VS6 does not support that.
template<typename T>
OP_STATUS
OpScopeCopy(const ByteBuffer &from, T &to, unsigned int max_len = ~0u)
{
	unsigned int remaining = MIN(max_len, from.Length());
	unsigned int count = from.GetChunkCount();
	for (unsigned int i = 0; i < count; ++i)
	{
		unsigned int nbytes;
		char *chunk = from.GetChunk(i, &nbytes);
		OP_ASSERT(chunk != NULL);
		if (nbytes == 0)
			continue;
		unsigned int copy_size = MIN(nbytes, remaining);
		RETURN_IF_ERROR(OpProtobufUtils::Append(to, OpProtobufUtils::Bytes(chunk, nbytes), copy_size));
		remaining -= copy_size;
		if (remaining == 0)
			break;
	}
	return OpStatus::OK;
}

template<typename T>
/*static*/
OP_STATUS
OpProtobufUtils::Copy(const UniStringPtr &from, T &to, unsigned int max_len)
{
	unsigned int remaining = MIN(max_len, from.Length())*2;
	return Append(to, reinterpret_cast<const char *>(from.CStr()), remaining);
}

template<typename C>
/*static*/
OP_STATUS
OpProtobufUtils::AppendString(C &container, const OpString &string)
{
	OpAutoPtr<OpString> tmp(OP_NEW(OpString, ()));
	RETURN_OOM_IF_NULL(tmp.get());
	RETURN_IF_ERROR(tmp->Set(string));
	RETURN_IF_ERROR(container.Add(tmp.get()));
	tmp.release();
	return OpStatus::OK;
}

template<typename C>
/*static*/
OP_STATUS
OpProtobufUtils::AppendString(C &container, const uni_char *string, int len)
{
	OpAutoPtr<OpString> tmp(OP_NEW(OpString, ()));
	RETURN_OOM_IF_NULL(tmp.get());
	RETURN_IF_ERROR(tmp->Set(string, len));
	RETURN_IF_ERROR(container.Add(tmp.get()));
	tmp.release();
	return OpStatus::OK;
}

template<typename C>
/*static*/
OP_STATUS
OpProtobufUtils::AppendString(C &container, const char *string, int len)
{
	OpAutoPtr<OpString> tmp(OP_NEW(OpString, ()));
	RETURN_OOM_IF_NULL(tmp.get());
	RETURN_IF_ERROR(tmp->Set(string, len));
	RETURN_IF_ERROR(container.Add(tmp.get()));
	tmp.release();
	return OpStatus::OK;
}

// TempBuffer

template<typename C>
/*static*/
OP_STATUS
OpProtobufUtils::AppendTempBuffer(C &container, const TempBuffer &string)
{
	OpAutoPtr<TempBuffer> tmp(OP_NEW(TempBuffer, ()));
	RETURN_OOM_IF_NULL(tmp.get());
	RETURN_IF_ERROR(tmp->Append(string.GetStorage(), string.Length()));
	RETURN_IF_ERROR(container.Add(tmp.get()));
	tmp.release();
	return OpStatus::OK;
}

template<typename C>
/*static*/
OP_STATUS
OpProtobufUtils::AppendTempBuffer(C &container, const uni_char *string, int len)
{
	OpAutoPtr<TempBuffer> tmp(OP_NEW(TempBuffer, ()));
	RETURN_OOM_IF_NULL(tmp.get());
	RETURN_IF_ERROR(tmp->Append(string, len));
	RETURN_IF_ERROR(container.Add(tmp.get()));
	tmp.release();
	return OpStatus::OK;
}

template<typename C>
/*static*/
OP_STATUS
OpProtobufUtils::AppendTempBuffer(C &container, const char *string, int len)
{
	OpAutoPtr<TempBuffer> tmp(OP_NEW(TempBuffer, ()));
	RETURN_OOM_IF_NULL(tmp.get());
	RETURN_IF_ERROR(tmp->Append(string, len));
	RETURN_IF_ERROR(container.Add(tmp.get()));
	tmp.release();
	return OpStatus::OK;
}

// ByteBuffer

template<typename C>
/*static*/
OP_STATUS
OpProtobufUtils::AppendBytes(C &container, const ByteBuffer &bytes)
{
	OpAutoPtr<ByteBuffer> tmp(OP_NEW(ByteBuffer, ()));
	RETURN_OOM_IF_NULL(tmp.get());
	RETURN_IF_ERROR(OpScopeCopy(bytes, *tmp));
	RETURN_IF_ERROR(container.Add(tmp.get()));
	tmp.release();
	return OpStatus::OK;
}

template<typename C>
/*static*/
OP_STATUS
OpProtobufUtils::AppendBytes(C &container, const char *bytes, int len)
{
	OpAutoPtr<ByteBuffer> tmp(OP_NEW(ByteBuffer, ()));
	RETURN_OOM_IF_NULL(tmp.get());
	RETURN_IF_ERROR(tmp->AppendBytes(bytes, len));
	RETURN_IF_ERROR(container.Add(tmp.get()));
	tmp.release();
	return OpStatus::OK;
}

template<typename C>
/*static*/
OP_STATUS
OpProtobufUtils::AppendOpData(C &container, const OpData &bytes)
{
	OpAutoPtr<OpData> tmp(OP_NEW(OpData, ()));
	RETURN_OOM_IF_NULL(tmp.get());
	RETURN_IF_ERROR(tmp->Append(bytes));
	RETURN_IF_ERROR(container.Add(tmp.get()));
	tmp.release();
	return OpStatus::OK;
}

template<typename C>
/*static*/
OP_STATUS
OpProtobufUtils::AppendOpData(C &container, const char *bytes, int len)
{
	OpAutoPtr<OpData> tmp(OP_NEW(OpData, ()));
	RETURN_OOM_IF_NULL(tmp.get());
	RETURN_IF_ERROR(tmp->AppendCopyData(bytes, len));
	RETURN_IF_ERROR(container.Add(tmp.get()));
	tmp.release();
	return OpStatus::OK;
}

template<typename T>
/*static*/
OP_STATUS
OpProtobufUtils::Append(T &col, int num)
{
	char buf[ DECIMALS_FOR_128_BITS + 1 ]; // ARRAY OK 2008-11-14 jborsodi
	op_itoa(num, buf, 10);
	return Append(col, buf);
}

template<typename T>
/*static*/ inline
OP_STATUS
OpProtobufUtils::Append(T &col, unsigned int num)
{
	return Append(col, static_cast<int>(num));
}

/*static*/ inline
OP_STATUS
OpProtobufUtils::Append(TempBuffer &col, int num)
{
	uni_char buf[ DECIMALS_FOR_128_BITS + 1 ]; // ARRAY OK 2008-11-14 jborsodi
	uni_itoa(num, buf, 10);
	return col.Append(buf);
}

/*static*/ inline
OP_STATUS
OpProtobufUtils::Append(TempBuffer &col, unsigned int num)
{
	return col.AppendUnsignedLong(num);
}

/*static*/ inline
OP_STATUS
OpProtobufUtils::Append(TempBuffer &col, const uni_char *in_str, unsigned int max_len)
{
	unsigned int len = MIN(max_len, uni_strlen(in_str));
	return col.Append(in_str, len);
}

/*static*/ inline
OP_STATUS
OpProtobufUtils::Append(ByteBuffer &col, const char *in_str, unsigned int max_len)
{
	unsigned int len = MIN(max_len, op_strlen(in_str));
	return col.AppendBytes(in_str, len);
}

/*static*/ inline
OP_STATUS
OpProtobufUtils::Append(ByteBuffer &col, const Bytes &in_bytes, unsigned int max_len)
{
	unsigned int len = MIN(max_len, in_bytes.Length());
	return col.AppendBytes(in_bytes.Data(), len);
}

/*static*/ inline
OP_STATUS
OpProtobufUtils::Append(OpString8 &col, const char *in_str, unsigned int max_len)
{
	unsigned int len = MIN(max_len, op_strlen(in_str));
	return col.Append(in_str, len);
}

/*static*/ inline
OP_STATUS
OpProtobufUtils::Append(OpString &col, const char *in_str, unsigned int max_len)
{
	unsigned int len = MIN(max_len, op_strlen(in_str));
	return col.Append(in_str, len);
}

/*static*/ inline
OP_STATUS
OpProtobufUtils::Append(OpString &col, const Bytes &in_str, unsigned int max_len)
{
	unsigned int len = MIN(max_len, in_str.Length());
	return col.Append(in_str.Data(), len);
}

template<typename T>
/*static*/ inline
int OpProtobufUtils::Length(const OpValueVector<T> &v)
{
	return v.GetCount();
}

template<typename T>
/*static*/ inline
int OpProtobufUtils::Length(const OpVector<T> &v)
{
	return v.GetCount();
}

/*static*/ inline
int OpProtobufUtils::Length(const OpINT32Vector &v)
{
	return v.GetCount();
}

template<typename T>
/*static*/ OP_STATUS 
OpProtobufUtils::ParseLong(const uni_char *text, ConvFunction<T> f, T min, T max, T &number, char invalid_first)
{
	if(text == NULL)
		return OpStatus::ERR_PARSING_FAILED;
		
	if(text[0] == invalid_first)
		return OpStatus::ERR_PARSING_FAILED;
		
	BOOL overflow;
	uni_char *end = NULL;

	number = f.strtol_f(text, &end, 10, &overflow);
	
	if(*end != '\0')
		return OpStatus::ERR_PARSING_FAILED;
		
	if(overflow)
		return OpStatus::ERR_PARSING_FAILED; // Out of range.

	if(number < min || number > max)
		return OpStatus::ERR_PARSING_FAILED;
	
	return OpStatus::OK;
}

#endif // PROTOBUF_SUPPORT

#endif // OP_PROTOBUF_UTILS_H
