/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef ISO_8859_1_DECODER_H
#define ISO_8859_1_DECODER_H

#include "modules/encodings/decoders/inputconverter.h"

/** Convert ISO-8859-1 into UTF-16 */
class ISOLatin1toUTF16Converter : public InputConverter
{
public:
	/** @param is_x_user_defined Report encoding as "x-user-defined" */
	ISOLatin1toUTF16Converter(BOOL is_x_user_defined = FALSE)
		: m_is_x_user_defined(is_x_user_defined) {};

	virtual int Convert(const void *src, int len, void *dest,
                      int maxlen, int *read);
	virtual const char *GetCharacterSet();
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	virtual BOOL IsValidEndState() { return TRUE; }
#endif

private:

	ISOLatin1toUTF16Converter(const ISOLatin1toUTF16Converter&);
	ISOLatin1toUTF16Converter& operator =(const ISOLatin1toUTF16Converter&);

	BOOL m_is_x_user_defined;	///< TRUE if used for "x-user-defined" input
};

#endif // ISO_8859_1_DECODER_H
