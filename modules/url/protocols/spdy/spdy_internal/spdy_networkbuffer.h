/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Kajetan Switalski
**
*/

#ifndef _SPDY_NETWORKBUFFER_H_
#define _SPDY_NETWORKBUFFER_H_

#include "modules/url/protocols/scomm.h"

class SpdyNetworkBufferFragment: public ListElement<SpdyNetworkBufferFragment>
{
	char *data;
	size_t length;
public:
	SpdyNetworkBufferFragment(char *data, size_t length): data(data), length(length) {}

	virtual ~SpdyNetworkBufferFragment()
	{
		OP_DELETEA(data);
	}

	size_t Length()
	{
		return length;
	}

	void SendAndClear(SComm *sink)
	{
		sink->SendData(data, length);
		data = NULL;
		length = 0;
	}
};

/**
 * Faster, simpler and much more SPDY-tailored replacement for OpData which can send its content to the SComm
 * without redundant allocations.
 */
class SpdyNetworkBuffer
{
	char *staticBuffer;
	size_t staticBufferLength;
	size_t staticBufferUsed;
	AutoDeleteList<SpdyNetworkBufferFragment> dynamicBuffers;

public:
	SpdyNetworkBuffer(char *staticBuffer, size_t staticBufferLength)
		: staticBuffer(staticBuffer), staticBufferLength(staticBufferLength), staticBufferUsed(0) {}

	void AppendCopyDataL(const char *data, size_t dataLength)
	{
		size_t fitsToStatic = MIN(dataLength, staticBufferLength - staticBufferUsed);
		if (fitsToStatic > 0)
		{
			op_memcpy(staticBuffer + staticBufferUsed, data, fitsToStatic);
			staticBufferUsed += fitsToStatic;
		}

		if (fitsToStatic != dataLength)
		{
			data += fitsToStatic;
			dataLength -= fitsToStatic;

			char *fragmentData = OP_NEWA_L(char, dataLength);
			op_memcpy(fragmentData, data, dataLength);
			SpdyNetworkBufferFragment *fragment = OP_NEW_L(SpdyNetworkBufferFragment, (fragmentData, dataLength));
			fragment->Into(&dynamicBuffers);
		}
	}

	void AppendCopyDataL(const OpData &data)
	{
		size_t dataLength = data.Length();
		size_t fitsToStatic = MIN(dataLength, staticBufferLength - staticBufferUsed);
		if (fitsToStatic > 0)
		{
			data.CopyInto(staticBuffer + staticBufferUsed, fitsToStatic);
			staticBufferUsed += fitsToStatic;
		}

		if (fitsToStatic != dataLength)
		{
			dataLength -= fitsToStatic;
			char *fragmentData = OP_NEWA_L(char, dataLength);
			data.CopyInto(fragmentData, dataLength, fitsToStatic);
			SpdyNetworkBufferFragment *fragment = OP_NEW_L(SpdyNetworkBufferFragment, (fragmentData, dataLength));
			fragment->Into(&dynamicBuffers);
		}
	}

	size_t Length()
	{
		size_t length = staticBufferUsed;
		for (SpdyNetworkBufferFragment *it = dynamicBuffers.First(); it; it = it->Suc())
			length += it->Length();
		return length;
	}

	void SendAndClearL(SComm *sink)
	{
		if (staticBufferLength)
		{
			char *netBuffer = OP_NEWA_L(char, staticBufferUsed);
			op_memcpy(netBuffer, staticBuffer, staticBufferUsed);
			sink->SendData(netBuffer, staticBufferUsed);
			staticBufferUsed = 0;
		}
		while (!dynamicBuffers.Empty())
		{
			SpdyNetworkBufferFragment *it = dynamicBuffers.First();
			it->SendAndClear(sink);
			it->Out();
			OP_DELETE(it);
		}
	}
};

#endif // _SPDY_NETWORKBUFFER_H_