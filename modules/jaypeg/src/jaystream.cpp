/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(_JPG_SUPPORT_) && defined(USE_JAYPEG)

#include "modules/jaypeg/src/jaystream.h"

JayStream::JayStream() : buffer(NULL), bufferlen(0)
{}

void JayStream::initData(const unsigned char *buf, unsigned int len)
{
	buffer = buf;
	bufferlen = len;
}

#endif

