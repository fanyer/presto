/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef IMAGEDECODERICO_H
#define IMAGEDECODERICO_H

#include "modules/img/image.h"

typedef struct
{
	UINT8		bWidth;          // Width, in pixels, of the image
    UINT8		bHeight;         // Height, in pixels, of the image
    UINT8		bColorCount;     // Number of colors in image (0 if >=8bpp)
    UINT8		bReserved;       // Reserved ( must be 0)
    UINT16		wPlanes;         // Color Planes
    UINT16		wBitCount;       // Bits per pixel
    UINT32		dwBytesInRes;    // How many bytes in this resource?
    UINT32		dwImageOffset;   // Where in the file is this image?
} ICO_DIRENTRY;

typedef struct
{
    UINT16		idReserved;   // Reserved (must be 0)
    UINT16		idType;       // Resource Type (1 for icons)
    UINT16		idCount;      // How many images?
} ICO_HEADER;

class ImageDecoderIco : public ImageDecoder
{

typedef struct _BMP_INFOHEADER{ // bmih
    UINT32	biSize;
    UINT32	biWidth;
    UINT32	biHeight;
    UINT16	biPlanes;
    UINT16	biBitCount;
    UINT32	biCompression;
    UINT32	biSizeImage;
    UINT32	biXPelsPerMeter;
    UINT32	biYPelsPerMeter;
    UINT32	biClrUsed;
    UINT32	biClrImportant;
} BMP_INFOHEADER;

typedef struct _RGBQ { // rgbq
    UINT8	rgbBlue;
    UINT8	rgbGreen;
    UINT8	rgbRed;
    UINT8	rgbReserved;
} RGBQ;
typedef struct ImageIcoDecodeInfo
{
	ICO_HEADER		header;
	ICO_DIRENTRY*	icEntries;
	BOOL			header_loaded;
	BOOL			entries_loaded;
	BOOL			reached_offset;
} ImageIcoDecodeInfo;

public:

	ImageDecoderIco();
	~ImageDecoderIco();

	virtual OP_STATUS DecodeData(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes, BOOL load_all = FALSE);
	virtual void SetImageDecoderListener(ImageDecoderListener* imageDecoderListener);

private:
	void CleanDecoder();
	/**
	 * Reads the fileheader and stores it in internal decode-structure
	 * @param data A pointer to current data
	 * @param num_bytes The number of bytes that is remaining in current data-chunk
	 */
	UINT32 ReadIcoFileHeader(const BYTE* data, UINT32 num_bytes);

	/**
	 * Read all ICO-entries in current data
	 * @param data A pointer to current data
	 * @param num_bytes The number of bytes that is remaining in current data-chunk
	 */
	UINT32 ReadIcoEntries(const BYTE* data, UINT32 num_bytes);

	ImageDecoderListener*	image_decoder_listener;	///> current listener -- whereto information is sent

	UINT32			width,				///> the total width of the picture
					height;				///> the total height of the picture
	UINT32			bits_per_pixel;		///> number of bits/pixel
	UINT32			col_mapsize;		///> the size of the color map
	ImageIcoDecodeInfo*		decode;		///> here goes all header information

	UINT32			img_to_decode;		///> which image we are supposed to decode

	BOOL			decoding_finished;  ///> Have we decoded everything that we were interested in?
	BOOL			is_alpha;

	OP_STATUS ReadIndexed (const unsigned char* src, UINT32 bytes_in_image);
	OP_STATUS ReadRaw     (const unsigned char* src, UINT32 bytes_in_image);
	OP_STATUS ReadRaw32   (const unsigned char* src, UINT32 bytes_in_image);

};

#endif // IMAGEDECODERICO_H
