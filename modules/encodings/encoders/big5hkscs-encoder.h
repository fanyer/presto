/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef BIG5HKSCS_ENCODER_H
#define BIG5HKSCS_ENCODER_H

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_CHINESE
#include "modules/encodings/encoders/outputconverter.h"

/** Convert UTF-16 into Big5-HKSCS
 */
class UTF16toBig5HKSCSConverter : public OutputConverter
{
public:
	UTF16toBig5HKSCSConverter();
	virtual OP_STATUS Construct();
	virtual ~UTF16toBig5HKSCSConverter();

	virtual int Convert(const void *src, int len, void *dest, int maxlen,
	                    int *read);

	virtual const char *GetDestinationCharacterSet();
	virtual int ReturnToInitialState(void *) { return 0; };
	virtual int LongestSelfContainedSequenceForCharacter() { return 2; };
	virtual void Reset();

private:
	const unsigned char *m_big5table1, *m_big5table2,
	                    *m_hkscs_plane0, *m_hkscs_plane2;
	long m_table1top, m_table2len, m_plane0len, m_plane2len;
	UINT16 m_surrogate; ///< Stored surrogate value
};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_CHINESE
#endif // BIG5HKSCS_ENCODER_H
