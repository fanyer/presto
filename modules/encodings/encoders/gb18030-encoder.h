/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef GB18030_ENCODER_H
#define GB18030_ENCODER_H

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_CHINESE
#include "modules/encodings/encoders/outputconverter.h"

/** Convert UTF-16 into GB-18030
 */
class UTF16toGB18030Converter : public OutputConverter
{
public:
	UTF16toGB18030Converter();
	virtual OP_STATUS Construct();
	virtual ~UTF16toGB18030Converter();

	virtual int Convert(const void *src, int len, void *dest, int maxlen,
	                    int *read);

	virtual const char *GetDestinationCharacterSet();
	virtual int ReturnToInitialState(void *) { return 0; };
	virtual int LongestSelfContainedSequenceForCharacter() { return 4; };
	virtual void Reset();

private:
	const unsigned char *m_gbktable1, *m_gbktable2;
	const UINT16 *m_18030_table;
	long m_table1top, m_table2len, m_18030_length;
	UINT16 m_surrogate; ///< Stored surrogate value
};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_CHINESE
#endif // GB18030_ENCODER_H
