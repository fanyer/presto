/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef IMG_IMAGEDUMP_H
#define IMG_IMAGEDUMP_H

#ifdef IMG_DUMP_TO_BMP

/** Saves a bitmap to disk in .bmp format.
 *
 * @param bitmap - bitmap to save.
 * @param file_path - full absolute path to file where the file will be saved.
 *        If this file already exists it will NOT be overwritten and the function will return OpStatus::ERR.
 * @param blend_to_grid - if TRUE then the background for the transparent parts
 *        of the bitmap will be light/dark checkboard, else it will be white.
 * @return OK if success or any valid error returned by OpFile when writing.
 */
OP_STATUS DumpOpBitmap(OpBitmap* bitmap, const uni_char* file_path, BOOL blend_to_grid);

OP_STATUS DumpOpBitmap(OpBitmap* bitmap, OpFileDescriptor& opened_file, BOOL blend_to_grid);

#endif // IMG_DUMP_TO_BMP

#ifdef MINPNG_ENCODER
/** saves the bitmap to disk in .png format.
 *
 * @param bitmap - bitmap to save.
 * @param file_path - full absolute path to file where the file will be saved.
 * @param allow_overwrite - if FALSE and file_path already exists it
 *        will NOT be overwritten and the function will return
 *        OpStatus::ERR. if TRUE file_path will be overwritten if it
 *        already exists.
 * @return OK if success or any valid error returned by OpFile when writing.
 */
OP_STATUS DumpOpBitmapToPNG(OpBitmap* bitmap, const uni_char* file_path, BOOL allow_overwrite = FALSE);
OP_STATUS DumpOpBitmapToPNG(OpBitmap* bitmap, OpFileDescriptor& opened_file);
#endif // MINPNG_ENCODER

#ifdef IMG_BITMAP_TO_BASE64PNG

/** Get a string representing the content of the canvas as a base-64
 * encoded string with the png representation of the content.
 *
 * @param bitmap - OpBitmap to convert (must be non-NULL)
 * @param buffer a pointer to a TempBuffer where the output is stored.
 * @returns OpStatus::OK on success, an error otherwise.
 */
OP_STATUS GetOpBitmapAsBase64PNG(OpBitmap* bitmap, TempBuffer* buffer);
OP_STATUS GetNonPremultipliedBufferAsBase64PNG(UINT32* pixels, unsigned int width, unsigned int height, TempBuffer* buffer);

#endif // IMG_BITMAP_TO_BASE64PNG

#ifdef IMG_BITMAP_TO_BASE64JPG

/** Get a string representing the content of the canvas as a base-64
 * encoded string with the jpg representation of the content. when
 * passed a bitmap with transparency, this function adheres to
 * <canvas> semantics - ie the bitmap is composited onto a solid black
 * background.
 *
 * @param bitmap - OpBitmap to convert (must be non-NULL)
 * @param buffer a pointer to a TempBuffer where the output is stored.
 * @param quality the quality, in percent (1-100), of the produced jpg image.
 * @returns OpStatus::OK on success, an error otherwise.
 */
OP_STATUS GetOpBitmapAsBase64JPG(OpBitmap* bitmap, TempBuffer* buffer, int quality = 85);
OP_STATUS GetNonPremultipliedBufferAsBase64JPG(UINT32* pixels, unsigned int width, unsigned int height, TempBuffer* buffer, int quality = 85);

#endif // IMG_BITMAP_TO_BASE64JPG

#endif // IMG_IMAGEDUMP_H
