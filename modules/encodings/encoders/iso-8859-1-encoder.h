/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef ISO_8859_1_ENCODER_H
#define ISO_8859_1_ENCODER_H

#include "modules/encodings/encoders/outputconverter.h"

class UTF16toISOLatin1Converter : public OutputConverter
{
public:
	/** @param asciionly Set to TRUE to only allow US-ASCII output
	    @param is_x_user_defined Report encoding as "x-user-defined" */
	UTF16toISOLatin1Converter(BOOL asciionly = FALSE, BOOL is_x_user_defined = FALSE)
		: m_is_x_user_defined(is_x_user_defined)
		{ m_high = asciionly ? 128 : 256; }
	virtual int Convert(const void *src, int len, void *dest, int maxlen,
	                    int *read);

	virtual const char *GetDestinationCharacterSet();
	virtual int ReturnToInitialState(void *) { return 0; };
	virtual int LongestSelfContainedSequenceForCharacter() { return 1; };

protected:
	uni_char m_high; ///< Upper boundary for identity conversion
	BOOL m_is_x_user_defined; ///< TRUE if used for "x-user-defined" output
	virtual char lookup(uni_char utf16);
};

#endif // ISO_8859_1_ENCODER_H
