/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef IMAGEDECODERBMP_H
#define IMAGEDECODERBMP_H

#include "modules/img/image.h"

typedef struct _BMP_FILEHEADER { // bmfh
    UINT16    bfType;
    UINT32   bfSize;
    UINT16    bfReserved1;
    UINT16    bfReserved2;
    UINT32   bfOffBits;
} BMP_FILEHEADER;

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

typedef struct _BMP_COREHEADER { // bmch
    UINT32	bcSize;
    UINT16	bcWidth;
    UINT16	bcHeight;
    UINT16	bcPlanes;
    UINT16	bcBitCount;
} BMP_COREHEADER;

typedef struct _RGBQ { // rgbq
    UINT8	rgbBlue;
    UINT8	rgbGreen;
    UINT8	rgbRed;
#ifdef SUPPORT_ALPHA_IN_32BIT_BMP
    UINT8	rgbAlpha;
#else // !SUPPORT_ALPHA_IN_32BIT_BMP
    UINT8	rgbReserved;
#endif // SUPPORT_ALPHA_IN_32BIT_BMP
} RGBQ;

typedef struct _RGBT { // rgbt
    UINT8	rgbtBlue;
    UINT8	rgbtGreen;
    UINT8	rgbtRed;
} RGBT;

typedef struct ImageBmpDecodeInfo
{
	BMP_FILEHEADER	bmf;
	union {
		BMP_INFOHEADER	bmh;
		BMP_COREHEADER	bmc;
	} bmi;
	BOOL	is_bitmapcoreinfo;
	BOOL	fileheader_loaded;
	BOOL	infoheader_loaded;
	BOOL	colormap_loaded;
}ImageBmpDecodeInfo;

class ImageDecoderBmp : public ImageDecoder
{
public:

	ImageDecoderBmp();
	~ImageDecoderBmp();

	virtual OP_STATUS DecodeData(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes, BOOL load_all = FALSE);
	virtual void SetImageDecoderListener(ImageDecoderListener* imageDecoderListener);

private:
	void CleanDecoder();
	/**
	 * Reads the fileheader from the bmp-data and stores it in internal decode-structure
	 * @param data A pointer to current data
	 * @param num_bytes The number of bytes that is remaining in current data-chunk
	 */
	UINT32 ReadBmpFileHeader(BYTE* data, UINT32 num_bytes);

	/**
	 * Reads the infoheader from the bmp-data and stores it in internal decode-structure.
	 * Also sets pictures width, height and bits_per_pixel.
	 * @param data A pointer to current data
	 * @param num_bytes The number of bytes that is remaining in current data-chunk
	 */
	UINT32 ReadBmpInfoHeader(BYTE* data, UINT32 num_bytes);

	/**
	 * Reads a chunk with rle-data. Returns the amount of byte we wish to have back in next round.
	 * Also signals finished lines to listener.
	 * @param data A pointer to current data
	 * @param num_bytes The number of bytes that is remaining in current data-chunk
	 * @param more True if ImageSource have more data to send
	 */
	INT32 ReadRleData(BYTE* data, UINT32 num_bytes, BOOL more);

	/**
	 * Reads a chunk with non compressed data. Returns the amount of byte we wish to have
	 * back in next round.
	 * Also signals finished lines to listener.
	 * @param data A pointer to current data
	 * @param num_bytes The number of bytes that is remaining in current data-chunk
	 * @param more True if ImageSource have more data to send
	 */
	INT32 ReadData(BYTE* data, UINT32 num_bytes, BOOL more);

	/**
	 * Sets a pixel in the internal line_RGBA to the specified index in palette
	 * @param pixel The pixel on current line that is to be set
	 * @param color_index The index in current col_map that the pixel shall have
	 */
	void SetPixel(UINT32 pixel, UINT16 color_index);

	ImageDecoderListener*	image_decoder_listener;	///> current listener -- whereto information is sent

	int				width,				///> the total width of the picture
					height;				///> the total height of the picture
	int				bits_per_pixel;		///> number of bits/pixel
	UINT32			bitfields_mask[3];	///> used for 16bit BMP_BITFIELDS bmps to store bitmask for RGB
	int				last_decoded_line;	///> the last line we had decoded
	int				start_pixel;		///> pixel to start at on line (if we ran out of data in the middle of a line)
	UINT32*			line_BGRA;			///> the data line where all pixels sent to ImageSource resides
	RGBQ*			col_map;			///> the color map (if any)
	UINT32			col_mapsize;		///> the size of the color map
	BOOL			is_rle_encoded;		///> this is true if we are decoding an rle4 or rle8 bmp
	BOOL			is_upside_down;		///> this image is bottom-up (otherwise top-down)
	ImageBmpDecodeInfo*  decode;		///> here goes all header information
	INT32			total_buf_used;
};

#endif // IMAGEDECODERBMP_H
