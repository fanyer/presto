/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef IMAGEDECODERJPG_H
#define IMAGEDECODERJPG_H

#include "modules/img/image.h"

#ifdef USE_JAYPEG

#include "modules/jaypeg/jayimage.h"
#include "modules/jaypeg/jaydecoder.h"

class ImageDecoderJpg : public ImageDecoder, public JayImage
{
public:
	ImageDecoderJpg();
	~ImageDecoderJpg();

	virtual OP_STATUS DecodeData(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes, BOOL load_all = FALSE);

	virtual void SetImageDecoderListener(ImageDecoderListener* imageDecoderListener);

	int init(int width, int height, int numComponents, BOOL progressive);
	void scanlineReady(int scanline, const unsigned char *imagedata);
#ifdef IMAGE_METADATA_SUPPORT
	void exifDataFound(unsigned int exif, const char* data);
#endif // IMAGE_METADATA_SUPPORT
#ifdef EMBEDDED_ICC_SUPPORT
	void iccDataFound(const UINT8* data, unsigned datalen);
#endif // EMBEDDED_ICC_SUPPORT
private:
	ImageDecoderListener* m_imageDecoderListener;

	JayDecoder *decoder;

	UINT32 width, height;
	UINT8 components;
	BOOL progressive;
	BOOL startFrame;
	UINT32 *linedata;
};

#endif // USE_JAYPEG

#ifdef USE_SYSTEM_JPEG

extern "C" {
#define XMD_H XMD_H
#include <jerror.h>
#include <jpeglib.h>
#undef XMD_H
}

typedef struct {
  struct jpeg_source_mgr pub;

  JOCTET*	buffer;
  int		buf_len;
  BOOL		more;
  long		skip_len;
} jpg_img_src_mgr;

enum JPEG_ERROR {
	JPEGERR_NOERROR, // everything is allright
	JPEGERR_HEADER,  // not a jpeg or a corrupted image
	JPEGERR_OUT_OF_MEMORY, // out of memory
	JPEGERR_DECOMPRESS     // something went wrong during decompress
};

class ImageDecoderJpg : public ImageDecoder
{
public:
	enum DCT_METHOD { DCT_INTEGER, DCT_FAST_INTEGER, DCT_FLOAT};
	enum DITHERING_MODE { DITHERING_NONE, DITHERING_FLOYD_STEINBERG, DITHERING_ORDERED};

	ImageDecoderJpg(BOOL progressive, BOOL smooth, BOOL one_pass,
					DCT_METHOD dct_method, DITHERING_MODE dithering_mode);

	~ImageDecoderJpg();

	virtual OP_STATUS DecodeData(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes, BOOL load_all = FALSE);

	virtual void SetImageDecoderListener(ImageDecoderListener* imageDecoderListener);

public:
	ImageDecoderListener* m_imageDecoderListener;

	//Preferences
	BOOL progressive;
	BOOL smooth;
	BOOL one_pass;
	DCT_METHOD dct_method;
	DITHERING_MODE dithering_mode;

	//Misc stuff, used by the decoder
	BOOL initialized;
	BOOL finished;
	BOOL header_loaded;

	BOOL want_indexed;
	BOOL quantize;
	unsigned char ** colormap;

	OP_STATUS abort_status;

	BOOL is_progressive;
	BOOL decompress_started;
	int bits_per_pixel;
	int start_scan;
	unsigned char* scratch_buffer;
	BOOL reading_data;
	BOOL need_start;

	int current_scanline;

	unsigned char* bitmap_array; //Temporary data for a line used by the jpeglib
	unsigned char* bitmap_lineRGBA; //Temporary data for a row of RGBA

	struct jpeg_decompress_struct	cinfo;
	jpeg_error_mgr jerr_pub;
	jpg_img_src_mgr					jsrc;
	UINT32 width, height;

private:
	int GetNrOfBits(UINT32 nr);
	BOOL IsSizeTooLarge(UINT32 width, UINT32 height);
};

#endif // USE_SYSTEM_JPEG

#endif // IMAGEDECODERJPG_H
