/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef IMAGEDECODERXBM_H
#define IMAGEDECODERXBM_H

#include "modules/img/image.h"

#define WidthString "_width"
#define HeightString "_height"

class ImageDecoderXbm : public ImageDecoder
{
public:

	ImageDecoderXbm();
	~ImageDecoderXbm();

	virtual OP_STATUS DecodeData(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes, BOOL load_all = FALSE);
	virtual void SetImageDecoderListener(ImageDecoderListener* imageDecoderListener);

private:
	ImageDecoderListener*	image_decoder_listener;

	UINT32	width;
	UINT32	height;
	BOOL	header_loaded;
	UINT32	last_decoded_line;
	UINT32	start_pixel;
	UINT32*	row_BGRA;
	int 	GetNextLine(const unsigned char* buf, int buf_len);
	BOOL	GetNextInt(const unsigned char* buf, int buf_len, int& buf_used, BYTE& next_byte);
};

#endif // IMAGEDECODERXBM_H
