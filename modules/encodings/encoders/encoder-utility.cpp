/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#ifdef ENCODINGS_HAVE_TABLE_DRIVEN

#include "modules/encodings/encoders/encoder-utility.h"

/**
 * Perform binary lookup in a compressed Unicode-to-DBCS table
 * @param dbcs_table DBCS table to look up
 * @param tablelen   Length of DBCS table in bytes
 * @param character  Unicode character to look up
 * @param out        Result: DBCS characters (nul,nul if none found)
 */
void lookup_dbcs_table(const unsigned char *dbcs_table, long tablelen,
                       uni_char character, char out[2])
{
	// Look up
	const unsigned char *found =
		reinterpret_cast<const unsigned char *>(op_bsearch(&character, dbcs_table, tablelen / 4, 4, unichar_compare));

	// Fill with found value, or zero if none was found
	if (found)
	{
		out[1] = found[2];
		out[0] = found[3];
	}
	else
	{
		out[0] = 0;
		out[1] = 0;
	}
}				   

int unichar_compare(const void *key, const void *entry)
{
#ifdef ENCODINGS_OPPOSITE_ENDIAN
	uni_char skey, sentry;
	reinterpret_cast<char *>(&skey)[0]   = reinterpret_cast<const char *>(key)[0];
	reinterpret_cast<char *>(&skey)[1]   = reinterpret_cast<const char *>(key)[1];
	reinterpret_cast<char *>(&sentry)[1] = reinterpret_cast<const char *>(entry)[0];
	reinterpret_cast<char *>(&sentry)[0] = reinterpret_cast<const char *>(entry)[1];
	return skey - sentry;
#elif defined NEEDS_RISC_ALIGNMENT
	uni_char skey, sentry;
	reinterpret_cast<char *>(&skey)[0]   = reinterpret_cast<const char *>(key)[0];
	reinterpret_cast<char *>(&skey)[1]   = reinterpret_cast<const char *>(key)[1];
	reinterpret_cast<char *>(&sentry)[0] = reinterpret_cast<const char *>(entry)[0];
	reinterpret_cast<char *>(&sentry)[1] = reinterpret_cast<const char *>(entry)[1];
	return skey - sentry;
#else
	return *reinterpret_cast<const uni_char *>(key) -
	       *reinterpret_cast<const uni_char *>(entry);
#endif // NEEDS_RISC_ALIGNMENT
}
#endif // ENCODINGS_HAVE_TABLE_DRIVEN
