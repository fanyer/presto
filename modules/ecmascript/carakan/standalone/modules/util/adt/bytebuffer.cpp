/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/*
 * Byte buffer implementation
 * Lars T Hansen
 * Jens Lindström
 */

#include "core/pch.h"
#include "modules/util/adt/bytebuffer.h"

ByteBuffer::ByteBuffer()
	: chunks(NULL)
	, nchunks(0)
	, current(0)
	, length(0)
	, consumed(0)
{
}

ByteBuffer::~ByteBuffer()
{
	FreeStorage();
}

OP_STATUS
ByteBuffer::GetNewChunk( char **bytes, unsigned int *nbytes )
{
	/* This is perhaps not pretty, but it does ensure that there is a
	   valid chunk numbered 'current' and that there are non-zero
	   bytes available in it. */
	RETURN_IF_ERROR( AppendBytes( "", 0 ) );

	*nbytes = BYTEBUFFER_CHUNKSIZE - length % BYTEBUFFER_CHUNKSIZE;
	*bytes = chunks[current];
	if (*nbytes == 0)
		*nbytes = BYTEBUFFER_CHUNKSIZE;
	else
		*bytes += BYTEBUFFER_CHUNKSIZE - *nbytes;

	return OpStatus::OK;
}

void
ByteBuffer::AddBytes( unsigned int nbytes )
{
	length += nbytes;
}

OP_STATUS
ByteBuffer::AppendBytes( const char *bytes, unsigned int nbytes )
{
	/* See GetNewChunk for its fairly odd use of this function. */

	if (nchunks == 0)
	{
		chunks = new char*[4];
		if (chunks == NULL)
			return OpStatus::ERR_NO_MEMORY;
		nchunks = 4;
		for ( unsigned int i=0 ; i < 4 ; i++ )
			chunks[i] = NULL;
		chunks[0] = new char[BYTEBUFFER_CHUNKSIZE];
		if (chunks[0] == NULL)
			return OpStatus::ERR_NO_MEMORY;
	}

	for (;;)
	{
		unsigned int offs = length % BYTEBUFFER_CHUNKSIZE;
		BOOL is_full = FALSE;

		if (offs == 0)
			is_full = current < length/BYTEBUFFER_CHUNKSIZE;

		if (!is_full)
		{
			unsigned int k = MIN(BYTEBUFFER_CHUNKSIZE - offs, nbytes);
			op_memcpy( chunks[current] + offs, bytes, k );
			nbytes -= k;
			length += k;
			bytes += k;
			if (nbytes == 0)
				break;
		}

		if (current+1 == nchunks)
		{
			char **new_chunks = new char*[2*nchunks];
			if (new_chunks == NULL)
				return OpStatus::ERR_NO_MEMORY;
			for ( unsigned int i=nchunks ; i < 2*nchunks ; i++ )
				new_chunks[i] = NULL;
			op_memcpy( new_chunks, chunks, nchunks*sizeof(char*) );
			delete [] chunks;
			chunks = new_chunks;
			nchunks *= 2;
		}

		if (chunks[current+1] == NULL)
		{
			chunks[current+1] = new char[BYTEBUFFER_CHUNKSIZE];
			if (chunks[current] == NULL)
				return OpStatus::ERR_NO_MEMORY;
		}
		++current;
	}

	return OpStatus::OK;
}

OP_STATUS
ByteBuffer::Append4( UINT32 n )
{
	union
	{
		UINT32 n;
		char ns[4];
	} u;
	u.n = n;
#ifndef OPERA_BIG_ENDIAN
	char tmp;
	tmp = u.ns[0]; u.ns[0] = u.ns[3]; u.ns[3] = tmp;
	tmp = u.ns[1]; u.ns[1] = u.ns[2]; u.ns[2] = tmp;
#endif
	return AppendBytes( u.ns, 4 );
}

OP_STATUS
ByteBuffer::Append2( UINT16 n )
{
	union
	{
		UINT16 n;
		char ns[2];
	} u;
	u.n = n;
#ifndef OPERA_BIG_ENDIAN
	char tmp;
	tmp = u.ns[0]; u.ns[0] = u.ns[1]; u.ns[1] = tmp;
#endif
	return AppendBytes( u.ns, 2 );
}

void
ByteBuffer::Extract( unsigned int loc, unsigned int n, char *buf ) const
{
	loc += consumed;

	unsigned int chunkidx = loc / BYTEBUFFER_CHUNKSIZE;
	unsigned int idx = loc % BYTEBUFFER_CHUNKSIZE;

	while (n > 0)
	{
		unsigned int k = MIN(BYTEBUFFER_CHUNKSIZE - idx, n);
		op_memcpy( buf, chunks[chunkidx]+idx, k );
		n -= k;
		buf += k;
		idx = 0;
		chunkidx++;
	}
}

UINT32
ByteBuffer::Extract4( unsigned int loc ) const
{
	loc += consumed;

	if (BYTEBUFFER_CHUNKSIZE - loc % BYTEBUFFER_CHUNKSIZE >= 4)
	{
		// Common case
		unsigned char *chunk = (unsigned char*)chunks[loc / BYTEBUFFER_CHUNKSIZE];
		unsigned int idx = loc % BYTEBUFFER_CHUNKSIZE;
		return (chunk[idx] << 24) | (chunk[idx+1] << 16) | (chunk[idx+2] << 8) | chunk[idx+3];
	}
	else
		return (Extract1(loc) << 24) | (Extract1(loc+1) << 16) | (Extract1(loc+2) << 8) | Extract1(loc+3);
}

UINT16
ByteBuffer::Extract2( unsigned int loc ) const
{
	loc += consumed;
	return (Extract1(loc) << 8) | Extract1(loc+1);
}

void
ByteBuffer::Consume( unsigned int nbytes )
{
	consumed += nbytes;

	if (consumed >= BYTEBUFFER_CHUNKSIZE)
	{
		unsigned int discard = consumed / BYTEBUFFER_CHUNKSIZE;
		for ( unsigned int i=0 ; i < discard ; i++ )
		{
			// We could just rotate the chunks array and reuse the
			// chunks instead.
			delete [] chunks[i];
		}
		current -= discard;

		op_memmove( chunks, chunks+discard, (current+1) * sizeof chunks[0] );
		op_memset( chunks+current+1, 0, discard * sizeof chunks[0] );

		unsigned int delta = discard * BYTEBUFFER_CHUNKSIZE;
		length -= delta;
		consumed -= delta;
	}
}

char *
ByteBuffer::Copy( BOOL zero_terminate ) const
{
	unsigned int nbytes = length - consumed;

	char *array = new char[nbytes+(zero_terminate ? 1 : 0)];
	if (array == NULL)
		return NULL;

	char *p = array;
	if (nchunks > 0)
	{
		if (0 < current)
		{
			op_memcpy( p, chunks[0] + consumed, BYTEBUFFER_CHUNKSIZE - consumed );
			p += BYTEBUFFER_CHUNKSIZE - consumed;
		}
		for ( unsigned int i=1 ; i < current ; i++ )
		{
			op_memcpy( p, chunks[i], BYTEBUFFER_CHUNKSIZE );
			p += BYTEBUFFER_CHUNKSIZE;
		}
		op_memcpy( p, chunks[current], length % BYTEBUFFER_CHUNKSIZE );
		p += length % BYTEBUFFER_CHUNKSIZE;
	}

	if (zero_terminate)
		*p = 0;

	return array;
}

void
ByteBuffer::Truncate(unsigned int new_length)
{
	new_length += consumed;
	current = new_length / BYTEBUFFER_CHUNKSIZE - (new_length % BYTEBUFFER_CHUNKSIZE == 0);
	length = new_length;
}

void
ByteBuffer::FreeStorage()
{
	for ( unsigned int i=0 ; i < nchunks ; i++ )
		delete [] chunks[i];

	delete [] chunks;
	chunks = NULL;
	nchunks = 0;
	current = 0;
	length = 0;
	consumed = 0;
}
