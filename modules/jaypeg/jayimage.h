/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JAYIMAGE_H
#define JAYIMAGE_H

#include "modules/jaypeg/jaypeg.h"

/** This is the callback class of jaypeg. Inherit this class to be notified
 * when the image changes. */
class JayImage
{
public:
	virtual ~JayImage(){}
	/** Called when the size and number of components is known. 
	 * @param width the width of the image.
	 * @param height the height of the image
	 * @param numComponents the number of components used in the image.
	 * This can only be 1 for grayscale or 3 for color jpegs.
	 * @param progressive TRUE if the image is a progressive jpeg, false 
	 * otherwise. If the image is progressive the same scanline might be 
	 * ready multiple times.
	 * @returns JAYPEG_OK, JAYPEG_ERR or JAYPEG_ERR_NO_MEMORY depending on 
	 * the status of this call. */
	virtual int init(int width, int height, int numComponents, BOOL progressive) = 0;
	/** Called when a scanline is decoded. 
	 * @param scanline the number of the scanline decoded.
	 * @param imagedata an array of r, g and b values for each pixel of 
	 * the scanline.*/
	virtual void scanlineReady(int scanline, const unsigned char *imagedata) = 0;
	/** Called when new exif meta data is found. This function is never called 
	 * unless meta data support is enabled.
	 * @param exif the id of the exif data found.
	 * @param data the value of the exif data. */
	virtual void exifDataFound(unsigned int exif, const char* data){}
	/** Called when an embedded ICC profile has been found (and
	 * reassembled). Only called if support for embedded ICC is enabled.
	 * @param data ICC profile data block.
	 * @param datalen Size of data block. */
#ifdef EMBEDDED_ICC_SUPPORT
	virtual void iccDataFound(const UINT8* data, unsigned datalen){}
#endif // EMBEDDED_ICC_SUPPORT

	/** Called to check if the listener wants more data. If this function
	 * returns false the decoding/flushing will be aborted.
	 * This should be used in the image module to make sure one call to
	 * decode doesn't take too much time.
	 * More data might be sent to the listener after this function is 
	 * returns false to make sure jaypeg doesn't stop in the middle of a 
	 * MCU row.
	 * Keep in mind that if you have progressive jpegs you might need to
	 * call flush multiple times after the decoding is finished to see
	 * the entire image, even if isDone returns true.
	 * @returns true if the listener wants to receive more data, false 
	 * otherwise. */
	virtual BOOL wantMoreData(){return TRUE;}
};

#endif
