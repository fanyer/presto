/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SBCS_DECODER_H
#define SBCS_DECODER_H

#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
#include "modules/encodings/decoders/inputconverter.h"
#include "modules/encodings/decoders/iso-8859-1-decoder.h"

// ==== Single-byte -> UTF-16   ===========================================

class SingleBytetoUTF16Converter : public ISOLatin1toUTF16Converter
{
public:
	SingleBytetoUTF16Converter(const char *tablename);
	SingleBytetoUTF16Converter(const char *charset, const char *tablename);
	virtual OP_STATUS Construct();
	virtual ~SingleBytetoUTF16Converter();
	virtual int Convert(const void *src, int len, void *dest,
	                    int maxlen, int *read);
	virtual const char *GetCharacterSet() { return m_charset; };
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	virtual BOOL IsValidEndState() { return TRUE; }
#endif

private:
	long m_table_size;          ///< number of map entries
	const UINT16 *m_codepoints; ///< map entries for the 128 upper chars (or all 256)
	char m_charset[128];        ///< name of character set /* ARRAY OK 2009-03-02 johanh */
	char m_tablename[128];      ///< name of charset table /* ARRAY OK 2009-03-02 johanh */

	SingleBytetoUTF16Converter(const SingleBytetoUTF16Converter&);
	SingleBytetoUTF16Converter& operator =(const SingleBytetoUTF16Converter&);
	virtual void Init(const char *charset, const char *tablename);
};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN

#endif // SBCS_DECODER_H
