/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef UNISTRING_UTF8_H_
#define UNISTRING_UTF8_H_

#include "modules/opdata/UniString.h"

/**
 * Convert a UniString to and from a UTF8 encoded string.
 *
 * Note: This class is meant to be used when UniString <--> UTF-8
 *       conversion is needed but it's not possible or desirable
 *       to take the full encodings module as a dependency.
 */
class UniString_UTF8
{
public:
	/**
	 * Set an existing UniString from UTF8-encoded data.
	 *
	 * \param[out] out          Destination UniString.
	 * \param      utf8_string  The string whose content is to be copied.
	 * \param      length       (optional) When not specified, it is assumed that the string is '\0'-terminated.
	 * \return OpStatus::OK on success.
	 */
	static OP_STATUS FromUTF8(UniString& out, const char* utf8_string, int length = -1);

	/**
	 * Create a UTF8-encoded string with the contents of a UniString.
	 *
	 * The returned string must be deallocated with OP_DELETEA when no
	 * longer in use.
	 *
	 * \param[out] utf8_string  Destination UTF8-encoded string.
	 * \param      src          The string whose content is to be copied.
	 * \return OpStatus::OK on success.
	 */
	static OP_STATUS ToUTF8(char** utf8_string, const UniString& src);
};

#endif  // UNISTRING_UTF8_H_
