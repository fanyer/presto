/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef EUC_TW_ENCODER_H
#define EUC_TW_ENCODER_H

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_CHINESE
#include "modules/encodings/encoders/outputconverter.h"

/** Convert UTF-16 into EUC-TW
 */
class UTF16toEUCTWConverter : public OutputConverter
{
public:
	UTF16toEUCTWConverter();
	virtual OP_STATUS Construct();
	virtual ~UTF16toEUCTWConverter();

	virtual int Convert(const void *src, int len, void *dest, int maxlen,
	                    int *read);

	virtual const char *GetDestinationCharacterSet();
	virtual int ReturnToInitialState(void *) { return 0; };
	virtual int LongestSelfContainedSequenceForCharacter() { return 4; };

private:
	const unsigned char *m_maptable1, *m_maptable2;
	long m_table1top, m_table2len;
};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_CHINESE
#endif // EUC_TW_ENCODER_H
