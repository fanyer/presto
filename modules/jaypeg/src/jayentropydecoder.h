/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JAYENTROPYDECODER_H
#define JAYENTROPYDECODER_H

/** Abstract class representing an entropy decoder. Can be either a huffman or
 * arithmetic decoder. */
class JayEntropyDecoder
{
public:
	virtual ~JayEntropyDecoder(){}

	/** Reset the decoder. Should be done at restart intervalls and new 
	 * scans. */
	virtual void reset() = 0;
	/** Read a number of samples. 
	 * @param stream the data stream to read from.
	 * @param component the component to read the sample for.
	 * @param numSamples the number of sample to read.
	 * @param samples pointer to the array of output samples.
	 * @returns JAYPEG_ERR_NO_MEMORY if out of memory.
	 * JAYPEG_ERR if an error occured.
	 * JAYPEG_NOT_ENOUGH_DATA if there is not enough data to decode the
	 * samples.
	 * JAYPEG_OK if all went well. */
	virtual int readSamples(class JayStream *stream, unsigned int component, unsigned int numSamples, short *samples) = 0;
};

#endif

