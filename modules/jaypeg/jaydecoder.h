/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JAYDECODER_H
#define JAYDECODER_H

#include "modules/jaypeg/src/jaystream.h"

class JayFormatDecoder;
class JayImage;

/** This is the main decoder class of jaypeg. It is used for all supported
 * streams. This is the class you want to create in order to decode a jpeg
 * image. */
class JayDecoder
{
public:
	JayDecoder();
	~JayDecoder();

	/** Init the decoder. This will try to parse part of the buffer and 
	 * create the apropriate decoder.
	 * Set decode_data to FALSE if you just want to parse the file without
	 * actually decoding the image. This is usefull for determining the 
	 * size of the image without decoding it.
	 * If image is NULL no decoder will be created, only the valid check 
	 * will be performed.
	 * The data passed to init must also be passed as the first decode 
	 * (more data may be included when calling decode).
	 * @param buffer the first bytes of the file. This is used to determine 
	 * which type of jpeg it is. 
	 * @param bufferlen the length of the buffer.
	 * @param img the image listener.
	 * @param decode_data set to TRUE to decode the data, FALSE to only 
	 * parse the file and determine size.
	 * @returns JAYPEG_NOT_ENOUGH_DATA if the buffer doesn not contain 
	 * enough data to determine if this is a valid jpeg. 
	 * JAYPEG_ERR_NO_MEMORY if the operation runs out of memory. 
	 * JAYPEG_ERR if the image is not a valid jpeg. 
	 * JAYPEG_OK otherwise.*/
	int init(const unsigned char *buffer, unsigned int bufferlen, JayImage *img, BOOL decode_data);
	/** This is the main decoding function. It should be called over and 
	 * over again until all data has been passed to it or the image is 
	 * ready. If a poitive number is returned, that amount of data must be 
	 * passed to decode next time. 
	 * @param buffer the data to parse.
	 * @param bufferlen the length of the data.
	 * @returns JAYPEG_ERR_NO_MEMORY if out of memory.
	 * JAYPEG_ERR if there is an error in the jpeg stream.
	 * The number of bytes to resend next time if no error occured. */
	int decode(const unsigned char *buffer, unsigned int bufferlen);

	/** Used for progressive images (but it doesn't hurt to call it for 
	 * non-progressive). This function will decode the image data
	 * from the samples it has gotten so far. It should be called whenever
	 * the decoder has sent all the data in one block, since it might take
	 * time before more data is available. If this is not called the 
	 * progressive image will not be updated until it is completed. */
	void flushProgressive();

	/** @returns TRUE if the decoding is finished, FALSE otherwise. */
	BOOL isDone();
	/** @returns TRUE if the image has been completly flused to the
	 * listener, FALSE otherwise. */
	BOOL isFlushed();
private:
	JayStream stream;
	
	JayFormatDecoder *decoder;
};

#endif

