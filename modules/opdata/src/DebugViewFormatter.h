/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_OPDATA_DEBUGVIEWFORMATTER_H
#define MODULES_OPDATA_DEBUGVIEWFORMATTER_H

/** @file
 *
 * @brief DebugViewFormatter and derived classes - formatting DebugView() output.
 *
 * This file is an internal/private part of the opdata module. Please do not
 * \#include this file from outside modules/opdata.
 *
 * This file contains the DebugViewFormatter base class, used by OpData for
 * formatting the contents of an OpData object into a human-readable debug
 * representation of the object. The implementations of DebugViewFormatter
 * (also found in this file) are instantiated in the public DebugView() methods
 * in OpData and UniString.
 *
 * See OpData.h and UniString.h for more extensive design notes on OpData and
 * UniString, respectively.
 */


#ifdef OPDATA_DEBUG
/**
 * Helper base class for formatting the data presented by OpData::DebugView().
 *
 * This can be used to e.g. format binary data in a way suitable for human
 * consumption.
 *
 * Implement #FormatData() to prepare the data presented by DebugView(), and
 * NeedLength() to estimate the output buffer length needed for a given input
 * buffer length.
 */
class DebugViewFormatter
{
public:
	virtual ~DebugViewFormatter() {}

	/**
	 * Estimate the output size needed for the given input size.
	 *
	 * Before DebugView() calls #FormatData(), it needs to allocate an
	 * output buffer of sufficient length. To figure out this length, it
	 * will call this method to determine the maximum number of output
	 * bytes generated from the given number of input bytes.
	 *
	 * Example: a raw data printer (i.e. generates one output byte per input
	 * byte) should return \c input_length, while a printer that converts
	 * each byte to "\\xhh" notation should return 4 * \c input_length.
	 *
	 * @param input_length Length of input.
	 * @return The maximum output length corresponding to the given
	 *         \c input_length.
	 */
	virtual size_t NeedLength(size_t input_length) = 0;

	/**
	 * Format OpData content for presentation by DebugView().
	 *
	 * Convert fragment data in \c src to a suitable debug representation
	 * in \c dst. Don't read more than \c src_len bytes or write more than
	 * \c dst_len bytes. Return the number of bytes written to \c dst, and
	 * don't bother with '\0'-termination (caller will look after that).
	 *
	 * @param dst Write output bytes to this pointer.
	 * @param dst_len Do not write more than this many bytes to \c dst.
	 * @param src Read OpData content from this pointer.
	 * @param src_len Do not read more than this many bytes from \c src.
	 * @return Actual number of bytes written to \c dst.
	 */
	virtual size_t FormatData(char *dst, size_t dst_len, const char *src, size_t src_len) = 0;
};

/**
 * The trivial raw byte-producing DebugViewFormatter implementation.
 *
 * This is used by OpData::DebugView() when producing debug strings that
 * contain the raw contents of the OpData buffers (no escaping).
 */
class DebugViewRawFormatter : public DebugViewFormatter
{
public:
	virtual ~DebugViewRawFormatter() {}

	virtual size_t NeedLength(size_t input_length)
	{
		return input_length;
	}

	virtual size_t FormatData(char *dst, size_t dst_len, const char *src, size_t src_len)
	{
		OP_ASSERT(dst && dst_len && src && src_len);
		size_t len = MIN(dst_len, src_len);
		op_memcpy(dst, src, len);
		return len;
	}
};

/**
 * The escaping DebugViewFormatter implementation used by OpData::DebugView().
 *
 * Escape non-ASCII and non-printable characters with "\xhh" where "hh" is the
 * byte value in hexadecimal notation.
 */
class DebugViewEscapedByteFormatter : public DebugViewFormatter
{
public:
	virtual ~DebugViewEscapedByteFormatter() {}

	virtual size_t NeedLength(size_t input_length)
	{
		return 4 * input_length;
	}

	virtual size_t FormatData(char *dst, size_t dst_len, const char *src, size_t src_len);

protected:
	/**
	 * Return true if given character is a printable ASCII character.
	 *
	 * Consider 'single quote' and 'backslash' non-printable, since they
	 * are used to delimit output, and escape bytes, respectively.
	 */
	static op_force_inline bool IsPrintableAscii(int c)
	{
		return !(c >> 7) && // within ASCII range (< 0x80)
			c != 0x27 && c != 0x5c && // neither \ nor '
			op_isprint(c); // printable
	}

	/// Output the formatting prefix '\\x'.
	static op_force_inline char *FormatPrefix(char *out)
	{
		*(out++) = '\\';
		*(out++) = 'x';
		return out;
	}

	/// Output the given byte as two hexadecimal digits.
	static op_force_inline char *FormatByte(char *out, int c)
	{
		OP_ASSERT(c <= 0xff);
		*(out++) = "0123456789abcdef"[(c >> 4) & 0x0f];
		*(out++) = "0123456789abcdef"[(c) & 0x0f];
		return out;
	}
};

/**
 * The escaping DebugViewFormatter implementation used by UniString::DebugView().
 *
 * Escape non-ASCII and non-printable characters with "\xhhhh" where "hhhh" is
 * the uni_char value in hexadecimal notation.
 */
class DebugViewEscapedUniCharFormatter : public DebugViewEscapedByteFormatter
{
public:
	virtual ~DebugViewEscapedUniCharFormatter() {}
	virtual size_t NeedLength(size_t input_length);
	virtual size_t FormatData(char *dst, size_t dst_len, const char *src, size_t src_len);
};
#endif // OPDATA_DEBUG

#endif /* MODULES_OPDATA_DEBUGVIEWFORMATTER_H */
