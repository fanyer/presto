/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef IDENTITYCONVERTER_H
#define IDENTITYCONVERTER_H

#include "modules/encodings/decoders/inputconverter.h"

/**
 * This converter simply passes data through unchanged.
 */
class IdentityConverter : public InputConverter
{
public:
	IdentityConverter() {};
	
	virtual int Convert(const void *src, int len, void *dest, int maxlen,
	                    int *read);
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	virtual BOOL IsValidEndState() { return TRUE; }
#endif

	virtual const char *GetCharacterSet();
	virtual const char *GetDestinationCharacterSet();

private:
	IdentityConverter(const IdentityConverter&);
	IdentityConverter& operator =(const IdentityConverter&);
};

#endif // IDENTITYCONVERTER_H
