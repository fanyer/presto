/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DECODERFACTORYPNG_H
#define DECODERFACTORYPNG_H

#include "modules/img/imagedecoderfactory.h"

#ifdef SELFTEST
class OpBitmap;
#endif 

class DecoderFactoryPng : public ImageDecoderFactory
{
public:

#if defined(INTERNAL_PNG_SUPPORT) || defined(ASYNC_IMAGE_DECODERS_EMULATION)
	ImageDecoder* CreateImageDecoder(ImageDecoderListener* listener);
#endif // INTERNAL_PNG_SUPPORT

	BOOL3 CheckSize(const UCHAR* data, INT32 len, INT32& width, INT32& height);
	BOOL3 CheckType(const UCHAR* data, INT32 len);

#ifdef SELFTEST
	/** Vertify a bitmap against a tga file.
	 * @param refFile the reference file to compare the bitmap to.
	 * @param bitmap the data to check
	 * @param maxPSNR maximum peek signal to noise ratio (http://en.wikipedia.org/wiki/PSNR). The maxPSNR should be 10^(psnr/10) to avoid log10 in the function.
	 * @param maxComponentDiff the maximum difference in a single component of the image.
	 * @param regenerate write the outdata to the file before checking. Should only be true when creating new selftest or updating it.
	 * @returns OpStatus::OK if the images match, OpStatus::ERR it they don't and OpStatus::ERR_NO_MEMory if there was not enough memory to test. */
	static OP_STATUS verify(const char* refFile, const OpBitmap* bitmap, unsigned int linesToTest, unsigned int maxComponentDiff = 1, unsigned int maxPSNR = 0, BOOL regenerate = FALSE);
	static OP_STATUS selftest_verify(const char* refFile, const OpBitmap* bitmap, unsigned int linesToTest, unsigned int maxComponentDiff = 1, unsigned int maxPSNR = 0, BOOL regenerate = FALSE) {	return verify(refFile, bitmap, linesToTest, maxComponentDiff, maxPSNR, regenerate); }


	static OP_STATUS verify(const char* refFile, unsigned int width, unsigned int height, unsigned int stride, UINT32* pixels, unsigned int linesToTest, unsigned int maxComponentDiff = 1, unsigned int maxPSNR = 0, BOOL regenerate = FALSE);
	static OP_STATUS selftest_verify(const char* refFile, unsigned int width, unsigned int height, unsigned int stride, UINT32* pixels, unsigned int linesToTest, unsigned int maxComponentDiff = 1, unsigned int maxPSNR = 0, BOOL regenerate = FALSE) { return verify(refFile, width, height, stride, pixels, linesToTest, maxComponentDiff, maxPSNR, regenerate); }
#endif // SELFTEST
};

#endif // !DECODERFACTORYPNG_H
