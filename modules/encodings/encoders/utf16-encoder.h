/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef UTF16_ENCODER_H
#define UTF16_ENCODER_H

#include "modules/encodings/encoders/outputconverter.h"

/**
 * "Convert" UTF-16 into UTF-16 (native byte order), adding BOM at start.
 * Input string may not contain a BOM.
 */
class UTF16toUTF16OutConverter : public OutputConverter
{
public:
	UTF16toUTF16OutConverter() : m_firstcall(TRUE) {};

	virtual int Convert(const void *src, int len, void *dest, int maxlen,
	                    int *read);

	virtual const char *GetDestinationCharacterSet();

	virtual int ReturnToInitialState(void *dest);
	virtual int LongestSelfContainedSequenceForCharacter() { return 2; };
	virtual void Reset();

private:
	BOOL m_firstcall;
};

#endif // UTF16_ENCODER_H
