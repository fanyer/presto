/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SBCS_ENCODER_H
#define SBCS_ENCODER_H

#ifdef ENCODINGS_HAVE_TABLE_DRIVEN
#include "modules/encodings/encoders/outputconverter.h"
#include "modules/encodings/encoders/iso-8859-1-encoder.h"

/** Convert UTF-16 into a single-byte encoding (table driven)
 */
class UTF16toSBCSConverter : public UTF16toISOLatin1Converter
{
public:
	UTF16toSBCSConverter(const char *charset, const char *tablename);
	virtual OP_STATUS Construct();
	virtual ~UTF16toSBCSConverter();

	virtual const char *GetDestinationCharacterSet() { return m_charset; };

	virtual int ReturnToInitialState(void *) { return 0; };
	virtual int LongestSelfContainedSequenceForCharacter() { return 1; };

protected:
	virtual char lookup(uni_char utf16);

private:
	const unsigned char *m_maptable;
	long m_tablelen;
	char m_charset[128];        ///< name of character set /* ARRAY OK 2009-03-02 johanh */
	char m_tablename[128];      ///< name of charset table /* ARRAY OK 2009-03-02 johanh */
};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN
#endif // SBCS_ENCODER_H
