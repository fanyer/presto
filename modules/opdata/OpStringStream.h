/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULE_OPDATA_OPSTRINGSTREAM_H
#define MODULE_OPDATA_OPSTRINGSTREAM_H

#ifdef OPDATA_STRINGSTREAM
#include "modules/opdata/OpData.h"
#include "modules/opdata/UniString.h"
#include "modules/hardcore/base/opstatus.h"

/** An output stream convertible to a string.
 *
 * Allows streaming simple types and objects of arbitrary types (provided
 * a suitable operator<< exists) to obtain their string descriptions.
 * Internally the data is (likely) stored in a growing buffer, which may result
 * in heap allocations when subsequent elements are streamed.
 * Refrain from using OpStringStream in often executed pieces of code.
 *
 * @tparam Container_t Type of container that stores the growing string.
 * Must supply an Append method with the same overloads as OpStringStream::operator<<.
 * Must support a Data(true) method call returning a '\0'-terminated representation of the content.
 * Must supply a Length() method returning the number of characters in the contained
 * string.
 * Must be assignable, must have default constructor. Examples: OpData, UniString.
 * @tparam Char_t Type of primitive character that is handled by this string stream.
 * Examples: char, uni_char.
 */
template<typename Container_t, typename Char_t>
class OpStringStream
{
public:
	/** Construct an empty OpStringStream.
	 *
	 * An empty OpStringStream contains no data yet and @fn Str() will
	 * return an empty string (""). The object is ready to receive data
	 * through operator<<.
	 */
	OpStringStream() {}

	/** Construct an OpStringStream with initial data.
	 *
	 * OpStringStream will be initialized with @a str. @fn Str() will, at
	 * this point, return a copy of @a str. The object is ready to receive additional data
	 * through operator<<.
	 * @param str Initial '\0'-terminated string.
	 */
	OpStringStream(const Char_t* str)
	{
		OpStatus::Ignore(m_data.AppendFormatted(str));
	}

	/** Get a string representation of contained data.
	 *
	 * Will return a pointer to a string containing all data streamed in up to
	 * this point.
	 * The string will never be NULL, an empty OpStringStream will produce a
	 * '\0'-terminated "".
	 * @note The returned pointer is valid only until the next operator<< is called.
	 * @return '\0'-terminated string.
	 */
	const Char_t* Str() const
	{
		return m_data.Data(true);
	}

	/** Append string.
	 *
	 * @a val must be a '\0'-terminated string. It is deep-copied, so the caller
	 * is not responsible for maintaining the pointer valid through the lifetime
	 * of this OpStringStream.
	 * @param val The value to append.
	 * @return Reference to self, for easy operator chaining.
	 */
	OpStringStream& operator<< (const Char_t* val)
	{
		OpStatus::Ignore(m_data.AppendFormatted(val));
		return *this;
	}

	/** Append a boolean value.
	 *
	 * A true value will be represented as "true", false will be represented
	 * as "false".
	 * @param val The value to append.
	 * @return Reference to self, for easy operator chaining.
	 */
	OpStringStream& operator<< (bool val)
	{
		OpStatus::Ignore(m_data.AppendFormatted(val));
		return *this;
	}

	/** Append integer value.
	 *
	 * Will be formatted as with printf's %%d.
	 * @param val The value to append.
	 * @return Reference to self, for easy operator chaining.
	 */
	OpStringStream& operator<< (int val)
	{
		OpStatus::Ignore(m_data.AppendFormatted(val));
		return *this;
	}

	/** Append unsigned integer value.
	 *
	 * Will be formatted as with printf's %%ud.
	 * @param val The value to append.
	 * @return Reference to self, for easy operator chaining.
	 */
	OpStringStream& operator<< (unsigned int val)
	{
		OpStatus::Ignore(m_data.AppendFormatted(val));
		return *this;
	}

	/** Append long value.
	 *
	 * Will be formatted as with printf's %%ld.
	 * @param val The value to append.
	 * @return Reference to self, for easy operator chaining.
	 */
	OpStringStream& operator<< (long val)
	{
		OpStatus::Ignore(m_data.AppendFormatted(val));
		return *this;
	}

	/** Append unsigned long value.
	 *
	 * Will be formatted as with printf's %%lu.
	 * @param val The value to append.
	 * @return Reference to self, for easy operator chaining.
	 */
	OpStringStream& operator<< (unsigned long val)
	{
		OpStatus::Ignore(m_data.AppendFormatted(val));
		return *this;
	}

	/** Append double precision value.
	 *
	 * Will be formatted as with printf's %%g.
	 * @param val The value to append.
	 * @return Reference to self, for easy operator chaining.
	 */
	OpStringStream& operator<< (double val)
	{
		OpStatus::Ignore(m_data.AppendFormatted(val));
		return *this;
	}

	/** Get the length of the concatenated string.
	 *
	 * @return Length of the string that is returned by Str().
	 */
	size_t Length() const
	{
		return m_data.Length();
	}

	/** Get the size of the concatenated string.
	 *
	 * @return Result of Length() times sizeof(Char_t).
	 */
	size_t Size() const
	{
		return m_data.Length() * sizeof(Char_t);
	}

	/** Get a reference to the container.
	 *
	 * @return Reference to a container with the concatenated string. It is
	 * safe to modify it as long as it remains valid.
	 */
	Container_t& Container()
	{
		return m_data;
	}

private:
	Container_t m_data;
};


typedef OpStringStream<OpData, char> OpASCIIStringStream;
typedef OpStringStream<UniString, uni_char> OpUniStringStream;

#endif // OPDATA_STRINGSTREAM
#endif // MODULE_OPDATA_OPSTRINGSTREAM_H
