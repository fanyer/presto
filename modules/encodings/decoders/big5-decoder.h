/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef BIG5_DECODER_H
#define BIG5_DECODER_H

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_CHINESE
#include "modules/encodings/decoders/dbcs-decoder.h"

class Big5toUTF16Converter : public DBCStoUTF16Converter
{
public:
	Big5toUTF16Converter();

private:
	virtual int get_map_index(unsigned char first, unsigned char second);
	virtual const char *GetCharacterSet();
};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_CHINESE
#endif // BIG5_DECODER_H
