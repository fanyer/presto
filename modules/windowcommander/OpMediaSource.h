/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPMEDIASOURCE_H
#define OPMEDIASOURCE_H

#include "modules/pi/OpMediaPlayer.h"

/** Used for querying which byte ranges are available. */
class OpMediaByteRanges
{
public:
	virtual ~OpMediaByteRanges() {}

	/** @return the number of ranges available */
	virtual UINT32 Length() const = 0;

	/** @return the first byte available in the idx'th range */
	virtual OpFileLength Start(UINT32 idx) const = 0;

	/** @return the first byte not available (exclusive) in the idx'th range */
	virtual OpFileLength End(UINT32 idx) const = 0;
};

/** Media source.
 *
 * Interface for providing OpMediaPlayer with a data source with pull access
 * from core, i.e. where OpMediaPlayer actively request data from the source
 * (and is not notified as new data arrives).
 */
class OpMediaSource
{
public:
	/** Create a new OpMediaSource instance.
	 *
	 * @param[out] source Set to the new OpMediaSource instance.
	 * @param handle Handle to the core media resource.
	 */
	static OP_STATUS Create(OpMediaSource** source, OpMediaHandle handle);

	virtual ~OpMediaSource() {}

	/** @return The (sniffed) Content-Type of the source. */
	virtual const char* GetContentType() = 0;

	/** @return TRUE if the source is seekable (e.g. if it is a file://
	 *  source or a HTTP source that supports byte range requests). */
	virtual BOOL IsSeekable() = 0;

	/** @return the total size of data source in bytes, or 0 if unknown. */
	virtual OpFileLength GetTotalBytes() = 0;

	/** Get the buffered byte ranges.
	 * @param[out] ranges the ranges of bytes available.
	 * @return OK on success or ERR_NULL_POINTER if ranges was NULL.
	 */
	virtual OP_STATUS GetBufferedBytes(const OpMediaByteRanges** ranges) = 0;

	/** Read from the data source into a pre-allocated buffer.
	 *
	 * @param buffer[out] the buffer into which the data is read.
	 * @param offset the offset in bytes from where to read.
	 * @param size the maximum number of bytes to read.
	 *
	 * When the requested data is not yet available, ERR_OUT_OF_RANGE
	 * is returned and loading the requested data will be prioritized
	 * over any preload requests. The only way to cancel such an
	 * implicit request is to call this method again with another
	 * offset/size. For speculative reads, @see Preload().
	 *
	 * Warning: Due to cache size limitations very large reads may be
	 * impossible to ever fulfill. It is recommended to read smaller
	 * chunks of data (less than 1 MB) to avoid this.
	 *
	 * @return OK on success, ERR_NULL_POINTER if buffer is NULL, or
	 * ERR_OUT_OF_RANGE if the requested data is not available.
	 */
	virtual OP_STATUS Read(void* buffer, OpFileLength offset, OpFileLength size) = 0;

	/** Preload data that is expected to later be read using Read.
	 *
	 * @param offset the offset in bytes from where to preload.
	 * @param size the maximum number of bytes to preload, or
	 * FILE_LENGTH_NONE to preload as much as possible from offset.
	 *
	 * If size is 0, a previous preload request will be cancelled.
	 * However, it is not appropriate to start and stop loading
	 * repeatedly using this API in order to throttle bandwidth.
	 *
	 * Initially, all data will be preloaded, starting at offset 0.
	 *
	 * @return OK on success.
	 */
	virtual OP_STATUS Preload(OpFileLength offset, OpFileLength size) = 0;
};

#endif // OPMEDIASOURCE_H
