/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef ICONUTILS_H
#define ICONUTILS_H

#include "adjunct/desktop_util/datastructures/StreamBuffer.h"

namespace IconUtils
{
	/**
	 * Load a bitmap from file. All formats supported by image module
	 * will be read. The returned bitmap must be released by the caller
	 * when no longer needed.
	 *
	 * @param filename File with valid image data
	 * @param width Width of returned bitmap, native width is used if
	 *        zero or negative
	 * @param height Height of returned bitmap, native height is used if
	 *        zero or negative
	 *
	 * @return The bitmap or 0 on error.
	 */
	OpBitmap* GetBitmap(const OpStringC& filename, int width, int height);

	/**
	 * Load a bitmap from buffer. All formats supported by image module
	 * will be read. The returned bitmap must be released by the caller
	 * when no longer needed.
	 *
	 * @param buffer Buffer with valid image data
	 * @param width Width of returned bitmap, native width is used if
	 *        zero or negative
	 * @param height Height of returned bitmap, native height is used if
	 *        zero or negative
	 *
	 * @return The bitmap or 0 on error.
	 */
	OpBitmap* GetBitmap(const StreamBuffer<UINT8>& buffer, int width, int height);

	/**
	 * Load a bitmap from buffer. All formats supported by image module
	 * will be read. The returned bitmap must be released by the caller
	 * when no longer needed.
	 *
	 * @param buffer Buffer with valid image data
	 * @opara buffer_size Size og buffer in bytes
	 * @param width Width of returned bitmap, native width is used if
	 *        zero or negative
	 * @param height Height of returned bitmap, native height is used if
	 *        zero or negative
	 *
	 * @return The bitmap or 0 on error.
	 */
	OpBitmap* GetBitmap(const UINT8* buffer, UINT32 buffer_size, int width, int height);

	/**
	 * Write bitmap content into buffer in PNG format. The buffer can 
	 * be saved to disk as a valid PNG file.
	 *
	 * @param bitmap Bitmap to write
	 * @param buffer buffer with PNG data on return
	 *
	 * @return OpStatus::OK on success, otherwise ERR_NO_MEMORY on too little memory
	 *         available or OpStatus::ERR on encoding error
	 */
	OP_STATUS WriteBitmapToPNGBuffer(OpBitmap* bitmap, StreamBuffer<UINT8>& buffer);
};

#endif //ICONUTILS_H
