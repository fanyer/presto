/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef IMAGEDECODERFACTORY_H
#define IMAGEDECODERFACTORY_H

class ImageDecoder;
class ImageDecoderListener;

/**
 * An ImageDecoderFactory can create ImageDecoder objects that can decode images of a specific
 * type. It can also check if it can decode a specific size, and peek the size of an image given
 * a small amount of data.
 */
class ImageDecoderFactory
{
public:
	/**
	 * Deletes the ImageDecoder factory object.
	 */
	virtual ~ImageDecoderFactory() {}

#if defined(INTERNAL_IMG_DECODERS_SUPPORT) || defined(ASYNC_IMAGE_DECODERS_EMULATION)
	/**
	 * Creates an image decoder belonging to this factory.
	 * @return the ImageDecoder created, or NULL if OOM.
	 * @param listener, the listener that will be called by the image decoder.
	 */
	virtual ImageDecoder* CreateImageDecoder(ImageDecoderListener* listener) = 0;
#endif // INTERNAL_IMG_DECODERS_SUPPORT

	/**
	 * Check the size of an image.
	 * @return YES if width and height could be found, NO if we cannot find the size, and MAYBE if we might find the size with more data.
	 * @param data pointer to the data to check the size for, must point to the start of the data stream.
	 * @param len length of the data to check size for.
	 * @param width will be set to the width of the image, if return value is YES.
	 * @param height will be set to the height of the image, if return value is YES.
	 */
	virtual BOOL3 CheckSize(const UCHAR* data, INT32 len, INT32& width, INT32& height) = 0;

	/**
	 * Check if the type of an image is supported by this ImageDecoderFactory.
	 * @return YES if this factory can create decoders for this type of image, NO if it cannot, and MAYBE if it need more data to find out if that is possible.
	 * @param data pointer to the data to check the type for, must point to the start of the data stream.
	 * @param len length of the data to check the type for.
	 */
	virtual BOOL3 CheckType(const UCHAR* data, INT32 len) = 0;
};

#ifdef ASYNC_IMAGE_DECODERS

/**
 * When using asynchronous image decoders, we need an interface that is readding the CreateImageDecoder
 * function to the ImageDecoder interface, since we do not want to be able to create image decoders in
 * the image decoder factories that is only used to check for type and for size.
 */
class AsyncImageDecoderFactory
{
public:
	static OP_STATUS Create(AsyncImageDecoderFactory** new_decoder);

	virtual ~AsyncImageDecoderFactory() {}

	/**
	 * Create an image decoder belonging to this factory. This interface is needed since when using
	 * asynchronous image decoding, the normal image decoder objects will not have this function.
	 * @return the image decoder created, or NULL if OOM.
	 * @param listener, the listener that will be called by the image decoder.
	 * @param type, when using asynchronous image decoding emulation, we also need to give this function the type to create. The emulated asynchronous decoder factory will then create an asynchronous image decoder, which owns a image decoder of the specified type, that is used to actually decode the image.
	 */
	virtual ImageDecoder* CreateImageDecoder(ImageDecoderListener* listener
#ifdef ASYNC_IMAGE_DECODERS_EMULATION
											 , int type
#endif // ASYNC_IMAGE_DECODERS_EMULATION
											 ) = 0;
};

#endif // ASYNC_IMAGE_DECODERS

inline UINT32 MakeUINT32(UCHAR c0, UCHAR c1, UCHAR c2, UCHAR c3)
{
	return
		c0 << 24 |
		c1 << 16 |
		c2 <<  8 |
		c3;
}

#endif // !IMAGEDECODERFACTORY_H
