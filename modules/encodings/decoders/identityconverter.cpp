/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file identityconverter.cpp
 *
 * Implementation of the generic character encoding converter.
 */

#include "core/pch.h"

#include "modules/encodings/decoders/identityconverter.h"

int IdentityConverter::Convert(const void *src, int len, void *dest, int maxlen,
                               int *read)
{
	if (len>maxlen) len = maxlen;
	op_memcpy(dest, src, len);  
	*read = len;
	m_num_converted += len / sizeof (uni_char);
	return len;
}

// Return NULL as charset since we aren't converting anything.
const char *IdentityConverter::GetCharacterSet()
{
	return NULL;
}

const char *IdentityConverter::GetDestinationCharacterSet()
{
	return NULL;
}
