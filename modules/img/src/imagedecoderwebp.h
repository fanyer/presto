/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

#ifndef IMAGEDECODERWEBP_H
# define IMAGEDECODERWEBP_H

# ifdef WEBP_SUPPORT

#  include "modules/img/image.h"
#  include "modules/webp/webp.h"

class ImageDecoderWebP : public ImageDecoder
{
public:
	ImageDecoderWebP() {}
	~ImageDecoderWebP() {}

	virtual OP_STATUS DecodeData(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes, BOOL load_all = FALSE);
	virtual void SetImageDecoderListener(ImageDecoderListener* listener) { m_dec.SetImageDecoderListener(listener); }

private:
	WebPDecoder m_dec;
};

# endif // WEBP_SUPPORT

#endif // !IMAGEDECODERWEBP_H
