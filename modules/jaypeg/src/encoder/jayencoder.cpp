/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/jaypeg/jaypeg.h"
#if defined(_JPG_SUPPORT_) && defined(USE_JAYPEG) && defined(JAYPEG_ENCODE_SUPPORT)

#include "modules/jaypeg/jayencoder.h"

#include "modules/jaypeg/src/encoder/jayjfifencoder.h"

JayEncoder::JayEncoder() : encoder(NULL)
{}

JayEncoder::~JayEncoder()
{
	OP_DELETE(encoder);
}

OP_STATUS JayEncoder::init(const char* type, int quality, unsigned int width, unsigned int height, JayEncodedData *strm)
{
	OP_ASSERT(!encoder);
#ifdef JAYPEG_JFIF_SUPPORT
	if (!op_strcmp(type, "jfif"))
	{
		encoder = OP_NEW(JayJFIFEncoder, ());
		if (!encoder)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		OP_STATUS err = ((JayJFIFEncoder*)encoder)->init(quality, width, height, strm);
		if (OpStatus::IsError(err))
		{
			OP_DELETE(encoder);
			encoder = NULL;
			return err;
		}
	}
#endif
	if (!encoder)
	{
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

OP_STATUS JayEncoder::encodeScanline(const UINT32 *scanline)
{
	if (!encoder)
	{
		return OpStatus::ERR;
	}
	return encoder->encodeScanline(scanline);
}

#endif

