/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_ADT_UNICHARBUFFER_H
#define MODULES_UTIL_ADT_UNICHARBUFFER_H

#include "modules/util/adt/bytebuffer.h"

class UniCharBuffer : public ByteBuffer
{
public:
	UniCharBuffer() : ByteBuffer() {}
	virtual ~UniCharBuffer() {}

	unsigned	UniLength() const
	{
		return UNICODE_DOWNSIZE(Length());
	}
	/**< Return the number of uni_chars used */

	OP_STATUS	AppendString(const uni_char *s, unsigned length)
	{
		return AppendBytes(reinterpret_cast<const char*>(s), UNICODE_SIZE(length));
	}

	uni_char*	Copy(BOOL zero_terminate = FALSE)
	{
		OP_ASSERT(ByteBuffer::Length() % sizeof(uni_char) == 0);
		unsigned int nbytes = length - consumed;

		char *array = OP_NEWA(char, nbytes+(zero_terminate ? 2 : 0));
		if (array == NULL)
			return NULL;

		char *p = array;
		if (nchunks > 0)
		{
			if (current == 0 && length <= BYTEBUFFER_CHUNKSIZE)
			{
				op_memcpy( p, chunks[0] + consumed, nbytes );
				p += nbytes;
			}
			else
			{
				unsigned int cur = current;
				if ((length % BYTEBUFFER_CHUNKSIZE) == 0)
					++cur;
				op_memcpy( p, chunks[0] + consumed, BYTEBUFFER_CHUNKSIZE - consumed );
				p += BYTEBUFFER_CHUNKSIZE - consumed;
				for ( unsigned int i=1 ; i < cur; i++ )
				{
					op_memcpy( p, chunks[i], BYTEBUFFER_CHUNKSIZE );
					p += BYTEBUFFER_CHUNKSIZE;
				}
				op_memcpy( p, chunks[cur], length % BYTEBUFFER_CHUNKSIZE );
				p += length % BYTEBUFFER_CHUNKSIZE;
			}
		}

		if (zero_terminate)
			*reinterpret_cast<uni_char*>(p) = 0;

		return reinterpret_cast<uni_char*>(array);
	}
};

#endif // MODULES_UTIL_ADT_UNICHARBUFFER_H
