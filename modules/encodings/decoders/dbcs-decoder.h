/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DBCS_DECODER_H
#define DBCS_DECODER_H

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && (defined ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_KOREAN)
#include "modules/encodings/decoders/inputconverter.h"

/**
 * Superclass for generic DBCS to UTF-16 converters.
 */

class DBCStoUTF16Converter : public InputConverter
{
public:
	virtual int Convert(const void *src, int len, void *dest, int maxlen, int *read);
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	virtual BOOL IsValidEndState() { return !m_prev_byte; }
#endif
	virtual ~DBCStoUTF16Converter();

	virtual const char *GetCharacterSet() = 0;
	virtual void Reset();

protected:
	/** Create DBCS decoder
	  * @param table_name Name of conversion table to load.
	  * @param first_low Low boundary (inclusive) of first byte of DBCS data.
	  * @param first_high High boundary (inclusive) of first byte of DBCS data.
	  * @param second_low Low boundary (inclusive) of second byte of DBCS data.
	  * @param second_high High boundary (inclusive) of second byte of DBCS data.
	  * @param second_low2 Low boundary (inclusive) of second byte of DBCS data (2nd range if any).
	  * @param second_high2 High boundary (inclusive) of second byte of DBCS data (2nd range if any).
	  */
	DBCStoUTF16Converter(const char *table_name,
	                     unsigned char first_low, unsigned char first_high,
						 unsigned char second_low, unsigned char second_high,
						 unsigned char second_low2 = 255, unsigned char second_high2 = 0);

	/** Second-phase constructor for allocating internal tables
	  * @return OpStatus::OK on success, OpStatus::ERR on failure.
	  */
	virtual OP_STATUS Construct();

	/** Convert DBCS data to index to mapping table
	  * @param first First byte of DBCS data
	  * @param second Second byte of DBCS data
	  * @return Index into mapping table. If input is invalid, the return
	  *         value is undefined. Return value -1 may be used to indicate
	  *         that no mapping exists.
	  */
	virtual int get_map_index(unsigned char first, unsigned char second) = 0;

private:
	char m_table_name[128]; ///< name of table to load /* ARRAY OK 2009-03-02 johanh */
	const UINT16 *m_map_table;
	long m_map_length;
	unsigned char m_prev_byte;
	unsigned char m_first_low, m_first_high, m_second_low, m_second_high,
	              m_second_low2, m_second_high2;
};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && (ENCODINGS_HAVE_CHINESE || ENCODINGS_HAVE_KOREAN)
#endif // DBCS_DECODER_H
