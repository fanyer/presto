/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef HAS_SET_HTTP_DATA
#include "modules/upload/upload.h"

#include "modules/olddebug/tstdump.h"

#ifdef _URL_EXTERNAL_LOAD_ENABLED_
Upload_AsyncBuffer::Upload_AsyncBuffer()
	: databuf(0),
	  total_datasize(0),
	  head(0),
	  tail(0)
{
}

Upload_AsyncBuffer::~Upload_AsyncBuffer()
{
	op_free(databuf);
}

void Upload_AsyncBuffer::InitL(uint32 buf_len, const char **header_names)
{
	total_datasize = buf_len;
	Upload_Base::InitL(header_names);
}

OP_STATUS Upload_AsyncBuffer::AppendData(const void *buf, uint32 bufsize)
{
	uint32 length = head - tail;

	OP_ASSERT(head < total_datasize);
	if (head >= total_datasize)
		return OpStatus::ERR;

	OP_ASSERT(head + bufsize <= total_datasize);
	if (head + bufsize > total_datasize)
		bufsize = total_datasize - head;

	/* could keep track of the actual allocated size of 'databuf' here, and
	   possibly avoid calling op_realloc() every time. */
	void *result_databuf = op_realloc(databuf, length + bufsize);
	if (!result_databuf)
		return OpStatus::ERR_NO_MEMORY;
	databuf = result_databuf;

	memcpy((UINT8 *)databuf + head - tail, buf, bufsize);
	head += bufsize;

	return OpStatus::OK;
}

OpFileLength Upload_AsyncBuffer::CalculateLength()
{
	return Upload_Base::CalculateLength() + total_datasize;
}

unsigned char *Upload_AsyncBuffer::OutputContent(unsigned char *target, uint32 &remaining_len, BOOL &done)
{
	done = FALSE;
	target = Upload_Base::OutputContent(target, remaining_len, done);
	if(!done)
		return target;

	BOOL more = FALSE;
	uint32 portion_len = GetNextContentBlock(target, remaining_len, more);
	done = !more;
	target += portion_len;
	remaining_len -= portion_len;
	return target;
}

uint32 Upload_AsyncBuffer::GetNextContentBlock(unsigned char *buf, uint32 max_len, BOOL &more)
{
	uint32 bytes = 0;
	OP_ASSERT(head >= tail);
	if (head > tail)
	{
		bytes = MIN(head - tail, max_len);
		memcpy(buf, databuf, bytes);
		uint32 newlength = head - tail - bytes;
		tail += bytes;
		if (newlength > 0)
		{
			memmove(databuf, (UINT8 *)databuf + bytes, newlength);
		}
		else
		{
			op_free(databuf);
			databuf = 0;
		}
	}
	more = tail < total_datasize;
	return bytes;
}

OpFileLength Upload_AsyncBuffer::PayloadLength()
{
	return total_datasize;
}
#endif // _URL_EXTERNAL_LOAD_ENABLED_

#endif // HTTP_data
