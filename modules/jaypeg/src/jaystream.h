/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JAYSTREAM_H
#define JAYSTREAM_H

/** The main data stream class which handles consuming and un-consuming of data.
 */
class JayStream
{
public:
	JayStream();
	/** Initialize the stream with a new data buffer.
	 * @param buf the data buffer.
	 * @param len the length of the buffer. */
	void initData(const unsigned char *buf, unsigned int len);
	/** @returns the number of bytes left in the buffer. */
	unsigned int getLength(){return bufferlen;}
	/** @return a pointer to the buffer. */
	const unsigned char *getBuffer(){return buffer;}
	/** Consumes some data in the buffer.
	 * @param len the number of bytes to advance. */
	void advanceBuffer(unsigned int len){
		buffer += len;
		bufferlen -= len;
	}
	/** Undo a consume.
	 * @param len the number of bytes to rewind the buffer. */
	void rewindBuffer(unsigned int len){
		buffer -= len;
		bufferlen += len;
	}

private:
	const unsigned char *buffer;
	unsigned int bufferlen;
};

#endif

