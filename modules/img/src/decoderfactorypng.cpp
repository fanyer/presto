/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef _PNG_SUPPORT_

#include "modules/img/decoderfactorypng.h"

#if defined(INTERNAL_PNG_SUPPORT) || defined(ASYNC_IMAGE_DECODERS_EMULATION)

#include "modules/img/src/imagedecoderpng.h"


ImageDecoder* DecoderFactoryPng::CreateImageDecoder(ImageDecoderListener* listener)
{
	ImageDecoder* image_decoder = OP_NEW(ImageDecoderPng, ());

	if (!image_decoder)
		return NULL;

	if (!((ImageDecoderPng*)image_decoder)->m_state || !((ImageDecoderPng*)image_decoder)->m_result)
	{
		// OOM
		OP_DELETE(image_decoder);
		return NULL;
	}

	image_decoder->SetImageDecoderListener(listener);

	return image_decoder;
}

#endif // INTERNAL_PNG_SUPPORT

BOOL3 DecoderFactoryPng::CheckSize(const UCHAR* data, INT32 len, INT32& width, INT32& height)
{
	if (len < 24)
	{
		return MAYBE;
	}
	
	width = MakeUINT32(data[16], data[17], data[18], data[19]);
	height = MakeUINT32(data[20], data[21], data[22], data[23]);
	
	return (width > 0 && height > 0) ? YES : NO; // Copied from DecoderFactoryWbmp. FIXME: KILSMO
}

BOOL3 DecoderFactoryPng::CheckType(const UCHAR* data, INT32 len)
{
	unsigned char png_signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};

	if (len < 8)
	{
		return MAYBE;
	}

	for (int i = 1; i < 8; i++) // starting at 1 since i have found pngimages that starts with -129,P,N,G
	{							// and works with our pnglib (emil)
		if (data[i] != png_signature[i])
		{
			return NO;
		}
	}

	return YES;
}

#ifdef SELFTEST

#include "modules/pi/OpBitmap.h"
#include "modules/util/opfile/opfile.h"
#include "modules/minpng/minpng.h"
#include "modules/minpng/minpng_encoder.h"

class PNGSelftestDecoderListener : public ImageDecoderListener
{
public:
	PNGSelftestDecoderListener(unsigned int width, unsigned int height, unsigned int stride, UINT32* pixels, unsigned int linesToTest, unsigned int limitDiff, unsigned int limitPSNR) : refBmp(NULL), refWidth(width), refHeight(height), refStride(stride), refPixels(pixels), refLines(linesToTest), validSize(FALSE), MSE(0), maxDiff(0), limitDiff(limitDiff), limitPSNR(limitPSNR)
	{}
	PNGSelftestDecoderListener(const OpBitmap* bmp, unsigned int linesToTest, unsigned int limitDiff, unsigned int limitPSNR) : refBmp(bmp), refPixels(0), refLines(linesToTest), validSize(FALSE), MSE(0), maxDiff(0), limitDiff(limitDiff), limitPSNR(limitPSNR)
	{
		refWidth = bmp->Width();
		refHeight = bmp->Height();
	}
	~PNGSelftestDecoderListener()
	{
		if (refBmp)
			OP_DELETEA(refPixels);
	}
	OP_STATUS Construct()
	{
		if (refBmp)
		{
			refPixels = OP_NEWA(UINT32, refWidth);
			if (!refPixels)
				return OpStatus::ERR_NO_MEMORY;
		}
		return OpStatus::OK;
	}
	void OnLineDecoded(void* data, INT32 line, INT32 lineHeight)
	{
		UINT32* pix;
		if (!validSize || line < 0 || line >= (INT32)refHeight || line >= (INT32)refLines)
			return;
		if (refBmp)
		{
			if (!refBmp->GetLineData(refPixels, line))
				return;
			pix = refPixels;
		}
		else
		{
			pix = refPixels+(refStride/4)*line;
		}
		for (unsigned int i = 0; i < refWidth; ++i)
		{
			int ref_alpha = ((UINT32*)data)[i]>>24;
			int ref_red = ((((((UINT32*)data)[i]>>16)&0xff)*(ref_alpha+1))>>8)&0xff;
			int ref_green = ((((((UINT32*)data)[i]>>8)&0xff)*(ref_alpha+1))>>8)&0xff;
			int ref_blue = ((((((UINT32*)data)[i])&0xff)*(ref_alpha+1))>>8)&0xff;

			int p_alpha = pix[i]>>24;
			int p_red = (pix[i]>>16)&0xff;
			int p_green = (pix[i]>>8)&0xff;
			int p_blue = pix[i]&0xff;
#ifndef USE_PREMULTIPLIED_ALPHA
			p_red = (p_red*(p_alpha+1))>>8;
			p_green = (p_green*(p_alpha+1))>>8;
			p_blue = (p_blue*(p_alpha+1))>>8;
#endif // !USE_PREMULTIPLIED_ALPHA
#if defined(VEGA_OPPAINTER_SUPPORT) && !defined(VEGA_INTERNAL_FORMAT_BGRA8888) && !defined(VEGA_INTERNAL_FORMAT_RGBA8888)

# ifdef VEGA_INTERNAL_FORMAT_RGB565A8
			// This loses more precision for green and alpha than it needs to,
			// but they all need the same base for psnr to work
			ref_red >>= 3;
			ref_green >>= 3;
			ref_blue >>= 3;
			ref_alpha >>= 3;
			p_red >>= 3;
			p_green >>= 3;
			p_blue >>= 3;
			p_alpha >>= 3;
#  define COMPONENT_MAX 0x1f
# else // VEGA_INTERNAL_FORMAT_RGB565A8
#  error "No valid format found"
# endif

#else
# define COMPONENT_MAX 0xff
#endif // VEGA_OPPAINTER_SUPPORT && !VEGA_INTERNAL_FORMAT_BGRA8888 && !VEGA_INTERNAL_FORMAT_RGBA8888
			int compDiff = p_alpha-ref_alpha;
			if (compDiff < 0)
				compDiff = -compDiff;
			MSE += compDiff*compDiff;
			if ((unsigned int)compDiff > maxDiff)
				maxDiff = compDiff;
			compDiff = p_red-ref_red;
			if (compDiff < 0)
				compDiff = -compDiff;
			MSE += compDiff*compDiff;
			if ((unsigned int)compDiff > maxDiff)
				maxDiff = compDiff;
			compDiff = p_green-ref_green;
			if (compDiff < 0)
				compDiff = -compDiff;
			MSE += compDiff*compDiff;
			if ((unsigned int)compDiff > maxDiff)
				maxDiff = compDiff;
			compDiff = p_blue-ref_blue;
			if (compDiff < 0)
				compDiff = -compDiff;
			MSE += compDiff*compDiff;
			if ((unsigned int)compDiff > maxDiff)
				maxDiff = compDiff;
		}
	}
	BOOL OnInitMainFrame(INT32 width, INT32 height)
	{
		if (width >= 0 && (unsigned int)width == refWidth && height >= 0 && (unsigned int)height == refHeight)
			validSize = TRUE;
		return TRUE;
	}
	void OnNewFrame(const ImageFrameData& image_frame_data){}
	void OnAnimationInfo(INT32 nrOfRepeats){}
	void OnDecodingFinished(){}
#ifdef IMAGE_METADATA_SUPPORT
	void OnMetaData(ImageMetaData id, const char* data) {}
#endif // IMAGE_METADATA_SUPPORT
#ifdef EMBEDDED_ICC_SUPPORT
	void OnICCProfileData(const UINT8* data, unsigned datalen) {}
#endif // EMBEDDED_ICC_SUPPORT

	BOOL isValid()
	{
		if (!validSize)
			return FALSE;
		if (maxDiff > limitDiff)
			return FALSE;
		if (MSE > 0)
		{
			unsigned int PSNR = (COMPONENT_MAX*COMPONENT_MAX*refWidth*refLines*4)/MSE;
			if (PSNR < limitPSNR)
				return FALSE;
		}
		return TRUE;
	}
private:
	const OpBitmap* refBmp;
	unsigned int refWidth;
	unsigned int refHeight;
	unsigned int refStride;
	UINT32* refPixels;
	unsigned int refLines;
	BOOL validSize;
	unsigned int MSE;
	unsigned int maxDiff;

	unsigned int limitDiff;
	unsigned int limitPSNR;
};

static OP_STATUS decodePNGImage(const char* fname, ImageDecoderPng* imgDec)
{
	// open file
	OpStringS file_name;
	OpFile imgFile;
	file_name.SetFromUTF8(fname);
	RETURN_IF_ERROR(imgFile.Construct(file_name));
	RETURN_IF_ERROR(imgFile.Open(OPFILE_READ));
	OpFileLength toRead;
	RETURN_IF_ERROR(imgFile.GetFileLength(toRead));
	char* data = OP_NEWA(char, 4*1024);
	if (!data)
		return OpStatus::ERR_NO_MEMORY;
	while( toRead > 0 )
	{
		unsigned int len = (unsigned int)(toRead<4*1024?toRead:4*1024);
		// read data
		OpFileLength bytes_read;
		if (OpStatus::IsError(imgFile.Read(data, len, &bytes_read)) || len != bytes_read)
		{
			OP_DELETEA(data);
			imgFile.Close();
			return OpStatus::ERR;
		}
		// decode data
		int resendBytes;
		OP_STATUS err = imgDec->DecodeData((BYTE*)data, len, toRead>len, resendBytes, TRUE);
		if (OpStatus::IsError(err))
		{
			OP_DELETEA(data);
			imgFile.Close();
			return err;
		}
		if (len <= (unsigned int)resendBytes)
		{
			OP_ASSERT(!"Too small chunk size in tga verification code");
			OP_DELETEA(data);
			imgFile.Close();
			return OpStatus::ERR;
		}
		len -= resendBytes;
		imgFile.SetFilePos(-resendBytes, SEEK_FROM_CURRENT);
		toRead -= len;
	}

	OP_DELETEA(data);
	imgFile.Close();
	return OpStatus::OK;
}

#ifdef USE_PREMULTIPLIED_ALPHA
static inline void unpremultiply_line(UINT32* src, UINT32* dst, unsigned int w)
{
	for (unsigned int xp = 0; xp < w; ++xp)
	{
		if (src[xp]!=0 && (src[xp]>>24)!=255)
		{
			int alpha = src[xp] >> 24;
			if (!alpha)
			{
				dst[xp] = 0;
				continue;
			}
			int red = (src[xp] >> 16)&0xff;
			int green = (src[xp] >> 8)&0xff;
			int blue = src[xp]&0xff;
			red = (red*255)/alpha;
			green = (green*255)/alpha;
			blue = (blue*255)/alpha;
			if (red > 255)
				red = 255;
			if (green > 255)
				green = 255;
			if (blue > 255)
				blue = 255;
			dst[xp] = (alpha<<24) | (red << 16) | (green<<8) | blue;
		}
		else
			dst[xp] = src[xp];
	}
}
#endif // USE_PREMULTIPLIED_ALPHA

OP_STATUS DecoderFactoryPng::verify(const char* refFile, const OpBitmap* bitmap, unsigned int linesToTest, unsigned int maxComponentDiff, unsigned int maxPSNR, BOOL regenerate)
{
	if (regenerate)
	{
#ifdef MINPNG_ENCODER
		OpStringS file_name;
		OpFile imgFile;
		file_name.SetFromUTF8(refFile);
		RETURN_IF_ERROR(imgFile.Construct(file_name));
		RETURN_IF_ERROR(imgFile.Open(OPFILE_WRITE));

		PngEncFeeder feeder;
		PngEncRes res;
		minpng_init_encoder_feeder(&feeder);
		feeder.has_alpha = 1;
		feeder.scanline = 0;
		feeder.xsize = bitmap->Width();
		feeder.ysize = bitmap->Height();
		BOOL again = TRUE;
		const UINT32 width = bitmap->Width();
		feeder.scanline_data = OP_NEWA(UINT32, width);
		if (!feeder.scanline_data)
		{
			imgFile.Close();
			return OpStatus::ERR_NO_MEMORY;
		}
		bitmap->GetLineData(feeder.scanline_data, feeder.scanline);
#ifdef USE_PREMULTIPLIED_ALPHA
		unpremultiply_line(feeder.scanline_data, feeder.scanline_data, bitmap->Width());
#endif // USE_PREMULTIPLIED_ALPHA
		OP_STATUS result = OpStatus::OK;
		while (again)
		{
			switch (minpng_encode(&feeder, &res))
			{
			case PngEncRes::AGAIN:
				break;
			case PngEncRes::ILLEGAL_DATA:
				result = OpStatus::ERR;
				again = FALSE;
				break;
			case PngEncRes::OOM_ERROR:
				result = OpStatus::ERR_NO_MEMORY;
				again = FALSE;
				break;
			case PngEncRes::OK:
				again = FALSE;
				break;
			case PngEncRes::NEED_MORE:
				++feeder.scanline;
				if (feeder.scanline >= bitmap->Height())
				{
					imgFile.Close();
					OP_DELETEA(feeder.scanline_data);
					return OpStatus::ERR;
				}
				bitmap->GetLineData(feeder.scanline_data, feeder.scanline);
#ifdef USE_PREMULTIPLIED_ALPHA
				unpremultiply_line(feeder.scanline_data, feeder.scanline_data, bitmap->Width());
#endif // USE_PREMULTIPLIED_ALPHA
				break;
			}
			if (res.data_size)
				imgFile.Write(res.data, res.data_size);
			minpng_clear_encoder_result(&res);
		}
		OP_DELETEA(feeder.scanline_data);
		minpng_clear_encoder_feeder(&feeder);
		imgFile.Close();
		if (OpStatus::IsError(result))
			return result;
#else // !MINPNG_ENCODER
		return OpStatus::ERR;
#endif // !MINPNG_ENCODER
	}
	ImageDecoderPng decoder;
	PNGSelftestDecoderListener decListener(bitmap, linesToTest, maxComponentDiff, maxPSNR);
	RETURN_IF_ERROR(decListener.Construct());
	decoder.SetImageDecoderListener(&decListener);

	RETURN_IF_ERROR(decodePNGImage(refFile, &decoder));

	if (!decListener.isValid())
		return OpStatus::ERR;
	return OpStatus::OK;
}
OP_STATUS DecoderFactoryPng::verify(const char* refFile, unsigned int width, unsigned int height, unsigned int stride, UINT32* pixels, unsigned int linesToTest, unsigned int maxComponentDiff, unsigned int maxPSNR, BOOL regenerate)
{
	if (!pixels)
		return OpStatus::ERR;
	if (regenerate)
	{
#ifdef MINPNG_ENCODER
		OpStringS file_name;
		OpFile imgFile;
		file_name.SetFromUTF8(refFile);
		RETURN_IF_ERROR(imgFile.Construct(file_name));
		RETURN_IF_ERROR(imgFile.Open(OPFILE_WRITE));

		PngEncFeeder feeder;
		PngEncRes res;
		minpng_init_encoder_feeder(&feeder);
		feeder.has_alpha = 1;
		feeder.scanline = 0;
#ifdef USE_PREMULTIPLIED_ALPHA
		feeder.scanline_data = OP_NEWA(UINT32, width);
		if (!feeder.scanline_data)
		{
			imgFile.Close();
			return OpStatus::ERR_NO_MEMORY;
		}
		UINT32* sl_pix = pixels;
		unpremultiply_line(sl_pix, feeder.scanline_data, width);
#else // !USE_PREMULTIPLIED_ALPHA
		feeder.scanline_data = pixels;
#endif // !USE_PREMULTIPLIED_ALPHA
		feeder.xsize = width;
		feeder.ysize = height;
		BOOL again = TRUE;
		OP_STATUS result = OpStatus::OK;
		while (again)
		{
			switch (minpng_encode(&feeder, &res))
			{
			case PngEncRes::AGAIN:
				break;
			case PngEncRes::ILLEGAL_DATA:
				result = OpStatus::ERR;
				again = FALSE;
				break;
			case PngEncRes::OOM_ERROR:
				result = OpStatus::ERR_NO_MEMORY;
				again = FALSE;
				break;
			case PngEncRes::OK:
				again = FALSE;
				break;
			case PngEncRes::NEED_MORE:
				++feeder.scanline;
				if (feeder.scanline >= height)
				{
#ifdef USE_PREMULTIPLIED_ALPHA
					OP_DELETEA(feeder.scanline_data);
#endif // USE_PREMULTIPLIED_ALPHA
					imgFile.Close();
					return OpStatus::ERR;
				}
#ifdef USE_PREMULTIPLIED_ALPHA
				sl_pix += stride/4;
				unpremultiply_line(sl_pix, feeder.scanline_data, width);
#else // !USE_PREMULTIPLIED_ALPHA
				feeder.scanline_data += stride/4;
#endif // USE_PREMULTIPLIED_ALPHA
				break;
			}
			if (res.data_size)
				imgFile.Write(res.data, res.data_size);
			minpng_clear_encoder_result(&res);
		}
		minpng_clear_encoder_feeder(&feeder);
#ifdef USE_PREMULTIPLIED_ALPHA
		OP_DELETEA(feeder.scanline_data);
#endif // USE_PREMULTIPLIED_ALPHA
		imgFile.Close();
		if (OpStatus::IsError(result))
			return result;
#else // !MINPNG_ENCODER
		return OpStatus::ERR;
#endif // !MINPNG_ENCODER
	}
	ImageDecoderPng decoder;
	PNGSelftestDecoderListener decListener(width, height, stride, pixels, linesToTest, maxComponentDiff, maxPSNR);
	RETURN_IF_ERROR(decListener.Construct());
	decoder.SetImageDecoderListener(&decListener);

	RETURN_IF_ERROR(decodePNGImage(refFile, &decoder));

	if (!decListener.isValid())
		return OpStatus::ERR;
	return OpStatus::OK;
}
#endif // SELFTEST

#endif // _PNG_SUPPORT_
