/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JAYFORMATENCODER_H
#define JAYFORMATENCODER_H

#ifdef JAYPEG_ENCODE_SUPPORT

/** Abstract class representing a format encoder. Currently only JFIF encoder 
 * is implemented */
class JayFormatEncoder
{
public:
	virtual ~JayFormatEncoder(){}
	/** Encode the next scanline of the image.
	 * @param scanline the scanline data to encode in 32bpp format.
	 * @returns OpStatus::OK on success, an error otherwise. */
	virtual OP_STATUS encodeScanline(const UINT32* scanline) = 0;
};

#endif // JAYPEG_ENCODE_SUPPORT

#endif // JAYFORMATENCODER_H

