/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

// Needed for JAYPEG_OK etc
#include "modules/jaypeg/jaypeg.h"
#if defined(_JPG_SUPPORT_) && defined(USE_JAYPEG) && defined(JAYPEG_JP2_SUPPORT)

#include "modules/jaypeg/src/jayjp2box.h"
#include "modules/jaypeg/src/jayjp2markers.h"

#include "modules/jaypeg/src/jaystream.h"

JayJP2Box::JayJP2Box() : lolength(0), hilength(0), type(0), parent(NULL)
{}

int JayJP2Box::init(JayJP2Box *prev, JayStream *stream)
{
	if (stream->getLength() < 8)
		return JAYPEG_NOT_ENOUGH_DATA;
	const unsigned char *buffer = stream->getBuffer();
	lolength = buffer[0]<<24 | buffer[1]<<16 | 
		buffer[2] << 8 | buffer[3];
	type = buffer[4]<<24 | buffer[5]<<16 | 
		buffer[6] << 8 | buffer[7];
	if (lolength == 0)
	{
		// FIXME: if this is 0, the box contains all data until eof
		//printf("FIXME: box extends to eof\n");
	}
	hilength = 0;
	if (lolength == 1)
	{
		// FIXME: this means we need to read a 64-bit length
		//printf("FIXME: box has 64-bit length\n");
	}
	//printf("Creating box: %c%c%c%c (%d bytes)\n", buffer[4], buffer[5], buffer[6], buffer[7], lolength);

	parent = prev;
	stream->advanceBuffer(reduceLength(8));
	return JAYPEG_OK;
}

int JayJP2Box::initMarker(JayJP2Box *prev, JayStream *stream)
{
	if (stream->getLength() < 2)
		return JAYPEG_NOT_ENOUGH_DATA;
	if (stream->getBuffer()[0] != 0xff)
		return JAYPEG_ERR;
	type = stream->getBuffer()[1];
	
	hilength = 0;
	lolength = 0;
	
	if ((type < 0x30 || type > 0x3f) &&
			type != JAYPEG_JP2_MARKER_SOC &&
			type != JAYPEG_JP2_MARKER_EOC &&
			type != JAYPEG_JP2_MARKER_SOD)
	{
		if (stream->getLength() < 2)
			return JAYPEG_NOT_ENOUGH_DATA;
		lolength = (stream->getBuffer()[2]<<8)|stream->getBuffer()[3];
		stream->advanceBuffer(reduceLength(2));
	}
	else if (type == JAYPEG_JP2_MARKER_SOD)
	{
		// The parent connot have hilenght set
		lolength = prev->lolength-2;
	}
	
	parent = prev;
	stream->advanceBuffer(2);
	return JAYPEG_OK;
}

void JayJP2Box::setLength(unsigned int len)
{
	hilength = 0;
	lolength = len;
}

unsigned int JayJP2Box::reduceLength(unsigned int len)
{
	if (len > lolength)
	{
		if (hilength == 0)
		{
			// we will only reduce the lenght by lolenght;
			len = lolength;
			lolength = 0;
			if (parent)
				parent->reduceLength(len);
			return len;
		}
		len -= lolength;
		--hilength;
		lolength = 0xffffffff;
	}
	lolength -= len;
	if (parent)
		parent->reduceLength(len);
	return len;
}

bool JayJP2Box::lengthLeft(unsigned int len)
{
	return hilength > 0 || lolength >= len;
}

bool JayJP2Box::isEmpty()
{
	return lolength == 0 && hilength == 0;
}

JayJP2Box *JayJP2Box::getParent()
{
	return parent;
}

unsigned int JayJP2Box::getType()
{
	if (type <= 0xff)
		return JP2_BOX_CODESTREAM;
	return type;
}

unsigned int JayJP2Box::getMarker()
{
	if (type > 0xff)
		return 0;
	return type;
}

void JayJP2Box::makeInvalid()
{
	type = 0;
}
#endif

