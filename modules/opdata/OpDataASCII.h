/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_UTIL_OPDATA_ASCII_H
#define MODULES_UTIL_OPDATA_ASCII_H

#include "modules/opdata/OpData.h"
#include "modules/opdata/UniString.h"

/** Convert an ASCII encoded OpData string to or from a UTF-16 encoded
 * UniString.
 *
 * @note Here "ASCII" means the Basic Latin subset of Unicode (U+0000-U+007F).
 *  Any character outside this range will not be copied.
 *
 * Example:
 * \code
 * UniString foo_uni;
 * RETURN_IF_ERROR(foo_uni.SetConstData(UNI_L("foo")));
 * OpData foo_ascii;
 * RETURN_IF_ERROR(ASCIIOpData::FromUniString(foo_uni, foo_ascii));
 * \endcode
 */
class ASCIIOpData
{
public:
	/** Converts the specified \c source from ASCII encoding to the specified
	 * UniString \c dest.
	 *
	 * @param source is the OpData that contains an ASCII encoded string.
	 * @param dest receives the UTF-16 encoded string. Note that this operation
	 *        appends the \c source to \c dest. So it is possible to append
	 *        several ASCII encoded OpData strings to one UniString by calling
	 *        this method several times on the same \c dest.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY if there was not enough memory to
	 *         complete the operation. In this case dest may contain a partially
	 *         converted string.
	 * @retval OpStatus::ERR_OUT_OF_RANGE if the specified source string did not
	 *         contain a valid ASCII string. This can happen, if some characters
	 *         of the \c source have the 8th bit set. In this case the \c dest
	 *         contains all real ASCII characters of \c source, any non-ASCII
	 *         characters are skipped. */
	static OP_STATUS ToUniString(const OpData& source, UniString& dest);

	/** Converts the specified UniString \c source to an ASCII encoded string in
	 * the specified OpData \c dest.
	 *
	 * @param source is the UniString to convert to ASCII encoding.
	 * @param dest receives the ASCII encoded string. Note that this operation
	 *        appends the \c source to \c dest. So it is possible to append
	 *        several UniString instances to one ASCII encoded OpData string by
	 *        calling this method several times on the same \c dest.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY if there was not enough memory to
	 *         complete the operation. In this case dest may contain a partially
	 *         converted string.
	 * @retval OpStatus::ERR_OUT_OF_RANGE if the specified source string did not
	 *         contain a valid ASCII string. This can happen, if some characters
	 *         of the \c source have more than the seven ASCII bits set. In this
	 *         case the \c dest contains all real ASCII characters of \c source,
	 *         any non-ASCII characters are skipped. */
	static OP_STATUS FromUniString(const UniString& source, OpData& dest);

	/** Compares the specified OpData ASCII encoded string with the specified
	 * UniString \c other.
	 *
	 * @param ascii contains an ASCII encoded string.
	 * @param other is the UniString to compare \c ascii with.
	 * @retval OpBoolean::IS_TRUE if both strings are equal.
	 * @retval OpBoolean::IS_FALSE if the strings differ.
	 * @retval OpStatus::ERR_OUT_OF_RANGE if the specified source string did not
	 *         contain a valid ASCII string. This can happen, if some characters
	 *         of the \c source have the 8th bit set.
	 * @retval OpStatus::ERR_NO_MEMORY if there was not enough memory to convert
	 *         the strings, so we could not decide if both strings are equal.
	 */
	static OP_BOOLEAN CompareWith(const OpData& ascii, const UniString& other);
};

#endif // MODULES_UTIL_OPDATA_ASCII_H
