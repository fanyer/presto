/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_ADT_BYTEBUFFER_H
#define MODULES_UTIL_ADT_BYTEBUFFER_H

#define BYTEBUFFER_CHUNKSIZE  (1024-8)  // trying to be nice to malloc, but perhaps this is folly

/** A ByteBuffer is an efficient data structure for storing and consuming
  * a sequence of bytes.  Bytes are stored in chunks, which allows large
  * sequences to be stored without allocating any large objects and provides
  * for an amortized O(1) byte insertion cost (if the malloc cooperates).
  */

class ByteBuffer
{
public:
	ByteBuffer();
	~ByteBuffer();

	unsigned int Length() const;
		/**< Return the number of bytes used */

	void Truncate(unsigned int new_length);
		/**< Change the length of the buffer to new_length, which
			 cannot be greater than the current length */

	unsigned int GetChunkCount() const;
		/**< Return the number of chunks used */

	char *GetChunk( unsigned int n, unsigned int *nbytes ) const;
		/**< Return the nth chunk and its length */

	OP_STATUS GetNewChunk( char **bytes, unsigned int *nbytes );
		/**< Return an area of currently unused bytes, either from the
		     of the current chunk or a whole new chunk.  If bytes are
		     written into the area, they need to be reported through a
		     call to AddBytes, or the buffer will not be aware of
		     them. */

	void AddBytes( unsigned int nbytes );
		/**< Report bytes that were written into an area returned by
		     GetNewChunk. */

	OP_STATUS AppendBytes( const char *bytes, unsigned int nbytes );
		/**< Append the first 'nbytes' bytes from 'bytes' to the stream.
	         If this fails due to OOM, then the number of bytes copied
			 before the failure occured is reflected in the value returned
	         by Length(). */

	OP_STATUS AppendString( const char *s, BOOL terminate=FALSE );
		/**< Append all characters of the string 's' to the stream.
		     If 'terminate' is TRUE, then a null byte is also appended.
			 If this fails due to OOM, then the number of bytes copied
			 before the failure occured is reflected in the value returned
	         by Length(). */

	OP_STATUS Append4( UINT32 n );
		/**< Append the four bytes of 'n' in network (big-endian) byte order */

	OP_STATUS Append2( UINT16 n );
		/**< Append the two bytes of 'n' in network (big-endian) byte order */

	OP_STATUS Append1( UINT8 n );
		/**< Append one byte to the buffer */

	void Extract( unsigned int loc, unsigned int n, char *buf ) const;
		/**< Extract n bytes starting at loc into the buffer buf.  If
			 there are not at least loc+n-1 bytes in the buffer the results
			 are undefined. */

	UINT32 Extract4( unsigned int loc ) const;
		/**< Extract four bytes from the buffer at loc; assume they are
		     in network (big-endian) byte order and return them in the
			 native byte order. */

	UINT16 Extract2( unsigned int loc ) const;
		/**< Extract two bytes from the buffer at loc; assume they are
		     in network (big-endian) byte order and return them in the
			 native byte order. */

	UINT8 Extract1( unsigned int loc ) const;
		/**< Return the byte at loc */

	char *Copy( BOOL zero_terminate=FALSE ) const;
		/**< Return a fresh heap-allocated character array containing the
		     contents of the byte buffer.  If zero_terminate is TRUE, then
			 a NUL byte is appended to the result.

			 @return NULL on OOM, otherwise the new array */

    void Consume( unsigned int nbytes );
		/**< Consume the first 'nbytes' bytes from the buffer. */

	void Clear();
		/**< Reset the buffer, but do not deallocate any storage.
			 Executes in constant time. */

	void FreeStorage();
		/**< Reset the buffer and free all storage.  Executes in time
			 that is at least proportional to the number of chunks
	         in the buffer, but can be worse if the malloc starts
			 coalescing or has to search a long free list.
			 */

private:
	char **chunks;			///< Array of byte arrays.  All chunks preceding the current are full, all following are empty
	unsigned int nchunks;	///< Length of the chunks array
	unsigned int current;	///< Current chunk index.  Valid iff nchunks > 0
	unsigned int length;	///< Number of bytes used in total, including consumed bytes.
	unsigned int consumed;	///< Number of bytes consumed from first chunk.
};

inline unsigned int
ByteBuffer::Length() const
{
	return length - consumed;
}

inline unsigned int
ByteBuffer::GetChunkCount() const
{
	return nchunks > 0 ? current+1 : 0;
}

inline char *
ByteBuffer::GetChunk( unsigned int n, unsigned int *nbytes ) const
{
	*nbytes = n < current ? BYTEBUFFER_CHUNKSIZE : length % BYTEBUFFER_CHUNKSIZE;
	char *chunk = chunks[n];
	if (n == 0)
	{
		chunk += consumed;
		*nbytes -= consumed;
	}
	return chunk;
}

inline OP_STATUS
ByteBuffer::AppendString( const char* s, BOOL terminate )
{
	return AppendBytes( s, op_strlen(s) + (terminate ? 1 : 0) );
}

inline OP_STATUS
ByteBuffer::Append1( UINT8 n )
{
	unsigned char c = n;
	return AppendBytes( (char*)&c, 1 );
}

inline UINT8
ByteBuffer::Extract1( unsigned int n ) const
{
	n += consumed;
	return ((unsigned char**)chunks)[n / BYTEBUFFER_CHUNKSIZE][n % BYTEBUFFER_CHUNKSIZE];
}

inline void
ByteBuffer::Clear()
{
	current=0;
	length=0;
	consumed=0;
}

#endif // !MODULES_UTIL_ADT_BYTEBUFFER_H
