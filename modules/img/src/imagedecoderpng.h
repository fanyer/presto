/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef IMAGEDECODERPNG_H
#define IMAGEDECODERPNG_H

#include "modules/img/image.h"

struct PngRes;
struct PngFeeder;

class ImageDecoderPng : public ImageDecoder
{
public:
	ImageDecoderPng();

	~ImageDecoderPng();

	virtual OP_STATUS DecodeData(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes, BOOL load_all = FALSE);

	virtual void SetImageDecoderListener(ImageDecoderListener* imageDecoderListener);

public:
	ImageDecoderListener* m_imageDecoderListener;
#ifndef MINPNG_NO_GAMMA_SUPPORT
	double          screen_gamma;
#endif
	BOOL finished;

	BOOL main_sent;
	PngRes *m_result;
	int frame_index;
	BOOL is_decoding_indexed;
	PngFeeder *m_state;
};

#endif // IMAGEDECODERPNG_H
