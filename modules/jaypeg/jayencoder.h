/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JAYENCODER_H
#define JAYENCODER_H

#include "modules/jaypeg/jaypeg.h"

#ifdef JAYPEG_ENCODE_SUPPORT

#include "modules/jaypeg/src/jaystream.h"

class JayFormatEncoder;
class JayEncodedData;

/** This is the main encoder class of jaypeg. It is used for all supported
 * streams. This is the class you want to create in order to encode a jpeg
 * image. */
class JayEncoder
{
public:
	JayEncoder();
	~JayEncoder();


	/** Initialize the encoder.
	 * @param type the type of encoder to create. Currently only "jfif" is supported.
	 * @param quality the quality in percent. Should be a number between 1 and 100.
	 * @param width the width of the image to encode.
	 * @param height the height of the image to encoder.
	 * @param strm the stream to send the encoded data to. 
	 * @returns OpStatus::OK on success, an error otherwise. */
	OP_STATUS init(const char* type, int quality, unsigned int width, unsigned int height, JayEncodedData *strm);
	/** Encode the next scanline of the image.
	 * @param scanline the data for the next scanline in 32bpp.
	 * @returns OpStatus::OK on success, an error otherwise. */
	OP_STATUS encodeScanline(const UINT32* scanline);

private:
	JayStream stream;
	
	JayFormatEncoder *encoder;
};

#endif // JAYPEG_ENCODE_SUPPORT

#endif // JAYENCODER_H

