/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef IMAGEDECODERWBMP_H
#define IMAGEDECODERWBMP_H

/** Wireless Bitmap (WBMP) decoder. This class handles the decoding of
  * WBMP images, typically used in WML (WAP) documents.
  */
#include "modules/img/image.h"

class ImageDecoderWbmp : public ImageDecoder
{
public:

	ImageDecoderWbmp();
	~ImageDecoderWbmp();

	virtual OP_STATUS DecodeData(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes, BOOL load_all = FALSE);
	virtual void SetImageDecoderListener(ImageDecoderListener* imageDecoderListener);

private:
	ImageDecoderListener*	image_decoder_listener;	///> Current ImageDecoderListener

	UINT8	bits_per_pixel;	///> Number of bits per pixel in this image
	BOOL	header_loaded;	///> Flag to tell if file header is loaded
	UINT32	last_decoded_line; ///> Line last decoded (when visited DecodeData last time)
	UINT32* line_BGRA;		///> Contains a 32bit/pixel data line that is sent to listener when ready

		/** State variable, needed to be able to handle data in several passes */
	enum	IsDecoding
			{ wType,		///< In image type information
			  wFixHeader,	///< In WBMP fixed header
			  wWidth,		///< In image width information
			  wHeight		///< In image height information
			} wbmp_inField;
	UINT32	wbmp_type,		///< Image type information (unused)
			wbmp_fixheader,	///< Fixed header data (unused)
			wbmp_width,		///< Image width
			wbmp_height;	///< Image height
	BOOL	error;			///< Error flag

	UINT32	start_pixel;	///< Where to start in last_decoded_line
};

#endif // IMAGEDECODERWBMP_H
