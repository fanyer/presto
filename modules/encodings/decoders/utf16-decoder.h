/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef UTF16_DECODER_H
#define UTF16_DECODER_H

#include "modules/encodings/decoders/inputconverter.h"
#include "modules/encodings/decoders/identityconverter.h"

// ==== UTF-16 any endian -> UTF-16  =====================================

/**
 * This converter converts from UTF-16 to UTF-16, checking first the
 * byte order mark.
 */
class UTF16toUTF16Converter : public InputConverter
{
public:
	virtual int Convert(const void *src, int len, void *dest, int maxlen,
	                    int *read);

	UTF16toUTF16Converter();
	virtual ~UTF16toUTF16Converter();
	virtual const char *GetCharacterSet();
	virtual void Reset();
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	virtual BOOL IsValidEndState();
#endif

private:
	InputConverter *m_utf16converter;

	UTF16toUTF16Converter(const UTF16toUTF16Converter&);
	UTF16toUTF16Converter& operator =(const UTF16toUTF16Converter&);

};

#if defined(OPERA_BIG_ENDIAN)
# define UTF16LEtoUTF16Converter ByteSwapConverter
# define UTF16BEtoUTF16Converter UTF16NativetoUTF16Converter
#else // defined(OPERA_BIG_ENDIAN)
# define UTF16LEtoUTF16Converter UTF16NativetoUTF16Converter
# define UTF16BEtoUTF16Converter ByteSwapConverter
#endif // defined(OPERA_BIG_ENDIAN)

// ==== UTF-16 machine endian -> UTF-16  =================================

/**
 * This converter takes UTF-16 in the machine endian and "converts" it to
 * the local machine endian.
 */
class UTF16NativetoUTF16Converter : public IdentityConverter
{
public:
	virtual const char *GetCharacterSet();
	virtual const char *GetDestinationCharacterSet();
};

// ==== UTF-16 opposite endian -> UTF-16  ================================

/**
 * This converter takes UTF-16 in the opposite endian and converts it to
 * the local machine endian.
 */

class ByteSwapConverter : public InputConverter
{
public:
	ByteSwapConverter() {};
	
	virtual int Convert(const void *src, int len, void *dest, int maxlen,
		int *read);
	virtual const char *GetCharacterSet();
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	virtual BOOL IsValidEndState() { return TRUE; }
#endif

private:

	ByteSwapConverter(const ByteSwapConverter&);
	ByteSwapConverter& operator =(const ByteSwapConverter&);

};

#endif // UTF16_DECODER_H
