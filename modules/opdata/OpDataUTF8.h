/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_UTIL_OPDATA_UTF8_H
#define MODULES_UTIL_OPDATA_UTF8_H

#include "modules/opdata/OpData.h"
#include "modules/opdata/UniString.h"
#include "modules/unicode/utf8.h"

/** Convert a UTF-8 encoded OpData string to or from a UTF-16 encoded UniString.
 *
 * Example:
 * \code
 * UniString foo_uni;
 * RETURN_IF_ERROR(foo_uni.SetConstData(UNI_L("foo")));
 * OpData foo_utf8;
 * RETURN_IF_ERROR(UTF8OpData::FromUniString(foo_uni, foo_utf8));
 * \endcode
 */
class UTF8OpData
{
public:
	/** Converts the specified \c source from UTF-8 encoding to the specified
	 * UniString \c dest.
	 *
	 * @param source is the OpData that contains a UTF-8 encoded string.
	 * @param dest receives the UTF-16 encoded string. Note that this operation
	 *        appends the \c source to \c dest. So it is possible to append
	 *        several UTF-8 encoded OpData strings to one UniString by calling
	 *        this method several times on the same \c dest.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY if there was not enough memory to
	 *         complete the operation. In this case dest may contain a partially
	 *         converted string.
	 * @retval OpStatus::ERR_OUT_OF_RANGE if the specified source string did not
	 *         contain a valid UTF-8 string. This can happen, if the string
	 *         ends in the middle of a multi-byte sequence. */
	static OP_STATUS ToUniString(const OpData& source, UniString& dest);

	/** Converts the specified UniString \c source to a UTF-8 encoded string in
	 * the specified OpData \c dest.
	 *
	 * @param source is the UniString to convert to UTF-8 encoding.
	 * @param dest receives the UTF-8 encoded string. Note that this operation
	 *        appends the \c source to \c dest. So it is possible to append
	 *        several UniString instances to one UTF-8 encoded OpData string by
	 *        calling this method several times on the same \c dest.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY if there was not enough memory to
	 *         complete the operation. In this case dest may contain a partially
	 *         converted string. */
	static OP_STATUS FromUniString(const UniString& source, OpData& dest);

	/** Compares the specified OpData UTF-8 encoded string with the specified
	 * UniString \c other.
	 *
	 * @param utf8 contains a UTF-8 encoded string.
	 * @param other is the UniString to compare \c utf8 with.
	 * @retval OpBoolean::IS_TRUE if both strings are equal.
	 * @retval OpBoolean::IS_FALSE if the strings differ.
	 * @retval OpStatus::ERR_OUT_OF_RANGE if the specified source string did not
	 *         contain a valid UTF-8 string. This can happen, if the string
	 *         ends in the middle of a multi-byte sequence.
	 * @retval OpStatus::ERR_NO_MEMORY if there was not enough memory to convert
	 *         the strings, so we could not decide if both strings are equal.
	 */
	static OP_BOOLEAN CompareWith(OpData utf8, UniString other);
};

#endif // MODULES_UTIL_OPDATA_UTF8_H
