/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JAYFORMATDECODER_H
#define JAYFORMATDECODER_H

/** Abstract class representing a format decoder. Currently only JFIF decoder 
 * is implemented but in the future a jpeg 2000 decoder will also be added. */
class JayFormatDecoder
{
public:
	virtual ~JayFormatDecoder(){}
	/** Decode a chunk of data.
	 * @param stream the data stream to decode from.
	 * @returns JAYPEG_ERR_NO_MEMORY if out of memory.
	 * JAYPEG_ERR if an error occured.
	 * JAYPEG_NOT_ENOUGH_DATA if there is not enough data to finish the 
	 * decoding and some data needs to be resent.
	 * JAYPEG_OK if all went well. */
	virtual int decode(class JayStream *stream) = 0;
	/** Flush a progressive image. Causes the updated scanlines to be 
	 * written to the image. */
	virtual void flushProgressive() = 0;
	/** @returns TRUE if the image is completly decoded, FALSE otherwise. */
	virtual BOOL isDone() = 0;
	virtual BOOL isFlushed() = 0;
};

#endif

