/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef UI_WIN_ICON_H
# define UI_WIN_ICON_H

class OpBitmap;

HBITMAP CreateHBITMAPFromOpBitmap(OpBitmap *bitmap);

struct ICONDIRENTRY
{
	// width, should be 0 if 256 pixels
	UINT8 width;
	// height, should be 0 if 256 pixels
	UINT8 height;
	// color count, should be 0 if more than 256 colors
	UINT8 color_count;
	// reserved, should be 0
	UINT8 reserved;
	// color planes, when .ico should be 0 or 1
	UINT16 color_planes;
	// bits per pixel when .ico format
	UINT16 bits_per_pixel;
	// size in bytes of the bitmap data
	UINT32 size;
	// offset, bitmap data address in file
	UINT32 data_offset;
};

struct ICONDIR
{
	// reserverd
	UINT16 reserved;
	// type, 1 for .ico, 2 for .cur file
	UINT16 type;
	// number of images in file
	UINT16 count;
};

namespace IconUtils
{
	HICON GetAsIcon(const OpBitmap* bitmap);

	/**
	 * Saves an Image as an ICO file.
	 *
	 * @param src_image the source image
	 * @param dest_file_path the target ICO file path
	 * @return status
	 */
	OP_STATUS WriteIcoFile(Image src_image, OpStringC dest_file_path);

	OP_STATUS SaveOpBitmapAsIco(const OpBitmap& bmp, OpStringC outFile);

	OP_STATUS CreateIconBitmap(OpBitmap **new_opbitmap, HICON icn);

	OP_STATUS CreateIconBitmap(OpBitmap **new_opbitmap, const uni_char* file_path, INT32 icon_size);
};

URL GetAttachmentIconURL(const OpStringC& filename, BOOL big_attachment_icon);

#endif // UI_WIN_ICON_H