/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef IMG_CAPABILITIES_H
#define IMG_CAPABILITIES_H

#define IMG_CAP_DONT_GUESS_ALL_TYPES // ImageManager::AddImageDecoderFactory(factory, type, check_header), decide if an
									 // image format should be checked when guesssing image type or not, in the function
									 // ImageManager::CheckImageType().
#define IMG_CAP_PEEK_IMAGE_DIMENSIONS // Image::PeekImageDimension, so the layoutengine can force peeking dimensions of a image.

#define IMG_CAP_HAS_FAILED_INFO // Image::IsFailed exist, so that the code outside the img module will
								// know that the decoding of an image has failed. (earlier, the only way
								// to know that was through a callback, no direct call existed.

#define IMG_CAP_ASK_PROVIDER_FOR_IMAGE_TYPE // Sometimes, we have special image providers, who can
											// provide us with a check for image type. The normal
											// Url provider will not implement the CheckImageType
											// function, but some special code will do that (like
											// fav icons ico images read from disk).

#define IMG_CAP_BOTTOM_TO_TOP // Some images (bmp) is decoded starting with the bottom line.
							  // 2 functions have been added to the Image interface, IsBottomToTop()
							  // and GetLowestDecodedLine().

#define IMG_CAP_GETEFFECTBITMAP_WITH_LISTENER	// GetBitmapEffect() takes a ImageListener as an argument, like GetBitmap() does

#define IMG_CAP_TGA_SELFTEST_VERIFICATION	// DecoderFactoryTga has a verify function which will compare the given bitmap against a reference tga file

#define IMG_CAP_PNG_SELFTEST_VERIFICATION	// DecoderFactoryPng has a verify function which will compare the given bitmap against a reference png file

#define IMG_CAP_CAN_HANDLE_MINPNG_INDEXED_OUTPUT // imagedecoderpng.cpp can handle output of indexed image data
												 // from minpng_decode in minpng module.

#define IMG_CAP_BITMAPPTR_ACCESS // Ready for non-const GetPointer() and ReleasePointer() methods in OpBitmap that express intended access type

#define IMG_CAP_GETTILE_DESIRED_SIZE // GetTileBitmap has desired_width, desired_height parameters.

#define IMG_CAP_GETINDEXED_DATA // Img module is ready for the new GetPalette and GetIndexedLineData API in OpBitmap.

#define IMG_CAP_REPORT_MEMMAN_CACHE_SIZE // The ImageManager can report cache size and usage

#define IMG_CAP_DECODE_DATA_RESENT_BYTES // DecodeData has a resendBytes parameter instead of returning the bytes to resend as an OP_STATUS

#define IMG_CAP_REPLACE_BITMAP_STATUS // Image::ReplaceBitmap returns OpStatus

#endif // !IMG_CAPABILITIES_H
