/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

#ifndef DECODERFACTORYWEBP_H
# define DECODERFACTORYWEBP_H

# include "modules/img/imagedecoderfactory.h"

# ifdef WEBP_SUPPORT
class DecoderFactoryWebP : public ImageDecoderFactory
{
public:
	ImageDecoder* CreateImageDecoder(ImageDecoderListener* listener);
	BOOL3 CheckSize(const UCHAR* data, INT32 len, INT32& width, INT32& height);
	BOOL3 CheckType(const UCHAR* data, INT32 len);
};
# endif // WEBP_SUPPORT

#endif // !DECODERFACTORYWEBP_H
