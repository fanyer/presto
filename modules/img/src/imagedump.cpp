/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/img/imagedump.h"
#include "modules/util/opfile/opfile.h"
#include "modules/pi/OpBitmap.h"

#ifdef MINPNG_ENCODER
# include "modules/minpng/minpng_encoder.h"
#endif // MINPNG_ENCODER

#ifdef IMG_DUMP_TO_BMP

static OP_STATUS DumpImageLine(OpFileDescriptor& bmpfile, UINT32* linedata, UINT32 line, UINT32 width, BOOL blend_to_grid)
{
	const int TILESIZE = 10;

	for (unsigned int x = 0; x < width; x++)
	{
		UINT32 tmp = linedata[x];
		int r = ((tmp >> 16) & 0xff);
		int g = ((tmp >> 8) & 0xff);
		int b = (tmp & 0xff);
		int alpha = tmp >> 24;
		int gv;
		if (blend_to_grid)// light/dark checkboard background
			gv = 128 + 64 * (((x%(2*TILESIZE))/TILESIZE)^((line%(2*TILESIZE))/TILESIZE));
		else // white background
			gv = 255;
#ifdef USE_PREMULTIPLIED_ALPHA
		int background = (255-alpha)*gv/255;
		r += background;
		g += background;
		b += background;
#else // !USE_PREMULTIPLIED_ALPHA
		r = gv + ((alpha*(r-gv))/255);
		g = gv + ((alpha*(g-gv))/255);
		b = gv + ((alpha*(b-gv))/255);
#endif //USE_PREMULTIPLIED_ALPHA
		r = (r > 255) ? 255 : (r < 0) ? 0 : r;
		g = (g > 255) ? 255 : (g < 0) ? 0 : g;
		b = (b > 255) ? 255 : (b < 0) ? 0 : b;

		RETURN_IF_ERROR(bmpfile.WriteBinByte(b));
		RETURN_IF_ERROR(bmpfile.WriteBinByte(g));
		RETURN_IF_ERROR(bmpfile.WriteBinByte(r));
	}

	// Padding
	for (unsigned int pad = 0; pad < ((4 - ((width*3) & 3)) & 3); pad++)
		RETURN_IF_ERROR(bmpfile.WriteBinByte(0));
	return OpStatus::OK;
}

static OP_STATUS DumpImageData(OpFileDescriptor& bmpfile, UINT32* data, UINT32 width, UINT32 height, UINT32 bytesPerLine, OpBitmap* bitmap, BOOL blend_to_grid)
{
	OP_ASSERT(bitmap);
	OP_ASSERT(bmpfile.IsOpen());
	OP_ASSERT(bmpfile.IsWritable());

	// Write bitmap file header
	// static struct {
	//	uint16 type; // BM
	//	uint32 size; //
	//	uint16 res1, res2; // Set to zero
	//	uint32 off;  // 54
	//};
	RETURN_IF_ERROR(bmpfile.WriteBinByte('B'));
	RETURN_IF_ERROR(bmpfile.WriteBinByte('M'));

	unsigned int imgsize = 14 + 40 + width*height*3;
	RETURN_IF_ERROR(bmpfile.WriteBinByte(imgsize & 0xff));
	RETURN_IF_ERROR(bmpfile.WriteBinByte((imgsize >> 8) & 0xff));
	RETURN_IF_ERROR(bmpfile.WriteBinByte((imgsize >> 16) & 0xff));
	RETURN_IF_ERROR(bmpfile.WriteBinByte((imgsize >> 24) & 0xff));

	RETURN_IF_ERROR(bmpfile.WriteBinShort(0));
	RETURN_IF_ERROR(bmpfile.WriteBinShort(0));
	RETURN_IF_ERROR(bmpfile.WriteBinLong(54 << 24)); // WriteBinLong writes as Big-Endian

	// Write bitmap info header
	//struct {
	//	uint32   biSize;
	//	int32    biWidth;
	//	int32    biHeight;
	//	int16    biPlanes;
	//	int16    biBitCount;
	//	uint32   biCompression;
	//	uint32   biSizeImage;
	//	int32    biXPelsPerMeter;
	//	int32    biYPelsPerMeter;
	//	uint32   biClrUsed;
	//	uint32   biClrImportant;
	//};
	RETURN_IF_ERROR(bmpfile.WriteBinLong(40 << 24));
	unsigned int dim = width;
	RETURN_IF_ERROR(bmpfile.WriteBinByte(dim & 0xff));
	RETURN_IF_ERROR(bmpfile.WriteBinByte((dim >> 8) & 0xff));
	RETURN_IF_ERROR(bmpfile.WriteBinByte((dim >> 16) & 0xff));
	RETURN_IF_ERROR(bmpfile.WriteBinByte((dim >> 24) & 0xff));

	dim = height;
	RETURN_IF_ERROR(bmpfile.WriteBinByte(dim & 0xff));
	RETURN_IF_ERROR(bmpfile.WriteBinByte((dim >> 8) & 0xff));
	RETURN_IF_ERROR(bmpfile.WriteBinByte((dim >> 16) & 0xff));
	RETURN_IF_ERROR(bmpfile.WriteBinByte((dim >> 24) & 0xff));

	RETURN_IF_ERROR(bmpfile.WriteBinShort(1 << 8));
	RETURN_IF_ERROR(bmpfile.WriteBinShort(24 << 8));
	RETURN_IF_ERROR(bmpfile.WriteBinLong(0)); // BI_RGB

	dim = width*height*3;
	RETURN_IF_ERROR(bmpfile.WriteBinByte(dim & 0xff));
	RETURN_IF_ERROR(bmpfile.WriteBinByte((dim >> 8) & 0xff));
	RETURN_IF_ERROR(bmpfile.WriteBinByte((dim >> 16) & 0xff));
	RETURN_IF_ERROR(bmpfile.WriteBinByte((dim >> 24) & 0xff));

	RETURN_IF_ERROR(bmpfile.WriteBinLong(72 << 24));
	RETURN_IF_ERROR(bmpfile.WriteBinLong(72 << 24));
	RETURN_IF_ERROR(bmpfile.WriteBinLong(0));
	RETURN_IF_ERROR(bmpfile.WriteBinLong(0));
	OP_STATUS status = OpStatus::OK;
	if(!data)
	{
		UINT32* linedata = OP_NEWA(UINT32, width);
		RETURN_OOM_IF_NULL(linedata);
		for (int scanline = height - 1; scanline >= 0; scanline--)
		{
			if(bitmap->GetLineData(linedata, scanline))
			{
				status = DumpImageLine(bmpfile, linedata, scanline, width, blend_to_grid);
				if (OpStatus::IsError(status))
					break;
			}
			else
			{
				status = OpStatus::ERR; // no line - corrupted OpBitmap?
				break;
			}
		}
		OP_DELETEA(linedata);
	}
	else
	{
		UINT32* imgptr = data;
		UINT32 stride = bytesPerLine / 4;
		for (int scanline = height - 1; scanline >= 0; scanline--)
		{
			unsigned int scanlineofs = scanline * stride;
			status = DumpImageLine(bmpfile, &imgptr[scanlineofs], scanline, width, blend_to_grid);
			if (OpStatus::IsError(status))
				break;
		}
	}
	return status;
}

OP_STATUS DumpOpBitmap(OpBitmap* bitmap, OpFileDescriptor& bmpfile, BOOL blend_to_grid)
{
	UINT32* ptr = bitmap->Supports(OpBitmap::SUPPORTS_POINTER) && bitmap->GetBpp() == 32 ? (UINT32*)bitmap->GetPointer() : NULL;
	OP_STATUS status = DumpImageData(bmpfile, ptr, bitmap->Width(), bitmap->Height(), bitmap->GetBytesPerLine(), bitmap, blend_to_grid);
	if(ptr)
	{
		bitmap->ReleasePointer();
	}
	return status;
}

OP_STATUS DumpOpBitmap(OpBitmap* bitmap, const uni_char* filestr, BOOL blend_to_grid)
{
	OpFile bmpfile;
	RETURN_IF_ERROR(bmpfile.Construct(filestr, OPFILE_ABSOLUTE_FOLDER));
	BOOL exists;
	RETURN_IF_ERROR(bmpfile.Exists(exists));
	if (exists)
		return OpStatus::ERR; // no overwriting
	RETURN_IF_ERROR(bmpfile.Open(OPFILE_WRITE));

	OP_STATUS status = DumpOpBitmap(bitmap, bmpfile, blend_to_grid);
	if (OpStatus::IsError(status))
	{
		OpStatus::Ignore(bmpfile.Close());
		OpStatus::Ignore(bmpfile.Delete());
	}
	return status;
}
#endif // IMG_DUMP_TO_BMP

#ifdef USE_PREMULTIPLIED_ALPHA
static inline void InvPremultiplyScanline(UINT32* data, int width)
{
	for (int xp = 0; xp < width; ++xp)
	{
		unsigned alpha = data[xp]>>24;
		if (alpha > 0 && alpha < 255)
		{
			unsigned red = (data[xp] >> 16) & 0xff;
			unsigned green = (data[xp] >> 8) & 0xff;
			unsigned blue = data[xp] & 0xff;

			unsigned recip_a = (255u << 24) / alpha;
			red = ((red * recip_a) + (1 << 23)) >> 24;
			green = ((green * recip_a) + (1 << 23)) >> 24;
			blue = ((blue * recip_a) + (1 << 23)) >> 24;

			data[xp] = (alpha<<24)|(red<<16)|(green<<8)|blue;
		}
	}
}
#endif // USE_PREMULTIPLIED_ALPHA

#ifdef MINPNG_ENCODER
OP_STATUS DumpOpBitmapToPNG(OpBitmap* bm, OpFileDescriptor& opened_file)
{
	OP_ASSERT(bm);

	PngEncFeeder feeder;
	PngEncRes res;

	minpng_init_encoder_feeder(&feeder);

	feeder.has_alpha = 1;
	feeder.scanline = 0;
	feeder.xsize = bm->Width();
	feeder.ysize = bm->Height();

	feeder.scanline_data = OP_NEWA(UINT32, bm->Width());
	if (!feeder.scanline_data)
	{
		opened_file.Close();
		return OpStatus::ERR_NO_MEMORY;
	}

	bm->GetLineData(feeder.scanline_data, feeder.scanline);
#ifdef USE_PREMULTIPLIED_ALPHA
	InvPremultiplyScanline(feeder.scanline_data, bm->Width());
#endif // USE_PREMULTIPLIED_ALPHA

	OP_STATUS result = OpStatus::OK;
	BOOL again = TRUE;
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
			if (feeder.scanline >= bm->Height())
			{
				opened_file.Close();
				OP_DELETEA(feeder.scanline_data);
				return OpStatus::ERR;
			}
			bm->GetLineData(feeder.scanline_data, feeder.scanline);
#ifdef USE_PREMULTIPLIED_ALPHA
			InvPremultiplyScanline(feeder.scanline_data, bm->Width());
#endif // USE_PREMULTIPLIED_ALPHA
			break;
		}

		if (res.data_size)
			opened_file.Write(res.data, res.data_size);

		minpng_clear_encoder_result(&res);
	}

	opened_file.Close();
	OP_DELETEA(feeder.scanline_data);

	minpng_clear_encoder_feeder(&feeder);

	return result;
}

OP_STATUS DumpOpBitmapToPNG(OpBitmap* bm, const uni_char* file_name, BOOL allow_overwrite/* = FALSE*/)
{
	OpFile savefile;
	RETURN_IF_ERROR(savefile.Construct(file_name));
	if (!allow_overwrite)
	{
		BOOL exists;
		RETURN_IF_ERROR(savefile.Exists(exists));
		if (exists)
			return OpStatus::ERR; // no overwriting
	}
	RETURN_IF_ERROR(savefile.Open(OPFILE_WRITE));
	const OP_STATUS status = DumpOpBitmapToPNG(bm, savefile);
	OpStatus::Ignore(savefile.Close());
	if (OpStatus::IsError(status))
		OpStatus::Ignore(savefile.Delete());
	return status;
}
#endif // MINPNG_ENCODER

#if defined(IMG_BITMAP_TO_BASE64PNG) || defined(IMG_BITMAP_TO_BASE64JPG)
# include "modules/formats/encoder.h"
# include "modules/libvega/src/vegaswbuffer.h"
static inline OP_STATUS AppendAsBase64(TempBuffer* buffer, const char* data, unsigned datalen)
{
	char* out = NULL;
	int outlen = 0;
	if (MIME_Encode_SetStr(out, outlen, data, datalen, NULL, GEN_BASE64_ONELINE) != MIME_NO_ERROR)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS result = buffer->Append(out, outlen);
	OP_DELETEA(out);
	return result;
}

static inline void CompositeOntoBlack(UINT32* data, int width)
{
	// This essentially means 'premultiply'
	while (width-- > 0)
	{
		UINT32 c = *data;

		unsigned alpha = c >> 24;
		if (alpha == 255)
			;
		else if (alpha == 0)
			*data = 0;
		else if (alpha < 255)
		{
			unsigned red = (c >> 16) & 0xff;
			unsigned green = (c >> 8) & 0xff;
			unsigned blue = c & 0xff;

			red = red * alpha / 255;
			green = green * alpha / 255;
			blue = blue * alpha / 255;

			*data = (red << 16) | (green << 8) | blue;
		}

		data++;
	}
}

class Base64ImageEncoderScanlineProvider
{
public:
	Base64ImageEncoderScanlineProvider(BOOL returnPremultiplied) : width(0), height(0), m_src(NULL), m_bmp(NULL), m_data(NULL), premultiply(returnPremultiplied)
	{}
	~Base64ImageEncoderScanlineProvider()
	{
		OP_DELETEA(m_data);
	}
	/** Initialize the scanline provider with an Opbitmap. The scanlines will be unpremultiplied if required. */
	OP_STATUS Construct(OpBitmap* src)
	{
		m_bmp = src;
		width = m_bmp->Width();
		height = m_bmp->Height();
		m_data = OP_NEWA(UINT32, width);
		if (!m_data)
			return OpStatus::ERR_NO_MEMORY;
		return OpStatus::OK;
	}
	/** Initialize the scanline provider with a buffer of non premultiplied pixels in bgra32 format. */
	OP_STATUS Construct(UINT32* data, unsigned int w, unsigned int h)
	{
		m_src = data;
		width = w;
		height = h;
		if (premultiply)
		{
			m_data = OP_NEWA(UINT32, width);
			if (!m_data)
				return OpStatus::ERR_NO_MEMORY;
		}
		return OpStatus::OK;
	}
	UINT32* GetScanline(unsigned int i)
	{
		if (m_bmp)
		{
			m_bmp->GetLineData(m_data, i);
#ifdef USE_PREMULTIPLIED_ALPHA
			if (!premultiply)
				InvPremultiplyScanline(m_data, width);
#else
			if (premultiply)
				CompositeOntoBlack(m_data, width);
#endif // USE_PREMULTIPLIED_ALPHA
			return m_data;
		}
		else
		{
			if (premultiply)
			{
				op_memcpy(m_data, m_src+width*i, width*4);
				CompositeOntoBlack(m_data, width);
				return m_data;
			}
			return m_src+width*i;
		}
	}
	unsigned int width;
	unsigned int height;
private:
	UINT32* m_src;
	OpBitmap* m_bmp;
	UINT32* m_data;
	BOOL premultiply;
};

#endif // IMG_BITMAP_TO_BASE64PNG || IMG_BITMAP_TO_BASE64JPG

#ifdef IMG_BITMAP_TO_BASE64PNG
# include "modules/minpng/minpng_encoder.h"

static OP_STATUS GetImageAsBase64PNG(Base64ImageEncoderScanlineProvider* prov, TempBuffer* buffer)
{
	OP_ASSERT(prov); // Caller should have made sure

	PngEncFeeder feeder;
	PngEncRes res;
	char remaining[3]; /* ARRAY OK 2009-03-31 fs */
	unsigned int remaining_len = 0;
	
	minpng_init_encoder_feeder(&feeder);
	feeder.has_alpha = 1;
	feeder.scanline = 0;
	feeder.xsize = prov->width;
	feeder.ysize = prov->height;
	BOOL again = TRUE;
	feeder.scanline_data = prov->GetScanline(feeder.scanline);
	OP_STATUS result = OpStatus::OK;

	while (again)
	{
		switch (minpng_encode(&feeder, &res))
		{
		case PngEncRes::ILLEGAL_DATA:
			result = OpStatus::ERR;
			// fall through
		case PngEncRes::OK:
			again = FALSE;
			// fall through
		case PngEncRes::AGAIN:
			break;
		case PngEncRes::OOM_ERROR:
			result = OpStatus::ERR_NO_MEMORY;
			again = FALSE;
			break;
		case PngEncRes::NEED_MORE:
			++feeder.scanline;
			if (feeder.scanline >= prov->height)
			{
				again = FALSE;
				result = OpStatus::ERR;
			}
			else
			{
				feeder.scanline_data = prov->GetScanline(feeder.scanline);
			}
			break;
		}
		if (res.data_size)
		{
			// Feed the data to the base64 encoder
			// Create a new chunk to hold the data
			char* data = (char*)res.data;
			int datalen = res.data_size;
			// Always fill the remaining array with 3 bytes if there
			// is enough data.
			while (datalen && remaining_len < 3)
			{
				remaining[remaining_len++] = *data;
				++data;
				--datalen;
			}
			// If there was enough data to fill the remaining array,
			// flush it. If there was not enough data datalen must 
			// be zero
			if (remaining_len == 3)
			{
				result = AppendAsBase64(buffer, remaining, 3);

				remaining_len = 0;
			}
			int usedlen = datalen - (datalen%3);
			if (usedlen)
			{
				result = AppendAsBase64(buffer, data, usedlen);

				data += usedlen;
				datalen -= usedlen;
			}

			// store the remaining data in the remaining array
			// If there is any data left at this point 
			// remaining_len must be zero. datalen can also never
			// be greater than 2 (since it is datalen%3), so it is 
			// safe
			while (datalen)
			{
				remaining[remaining_len++] = *data;
				++data;
				--datalen;
			}

			if (OpStatus::IsError(result))
				again = FALSE;
		}
		minpng_clear_encoder_result(&res);
	}

	minpng_clear_encoder_feeder(&feeder);

	if (remaining_len && OpStatus::IsSuccess(result))
		result = AppendAsBase64(buffer, remaining, remaining_len);

	return result;
}

OP_STATUS GetOpBitmapAsBase64PNG(OpBitmap* bitmap, TempBuffer* buffer)
{
	OP_ASSERT(bitmap); // Caller should have made sure

	Base64ImageEncoderScanlineProvider prov(FALSE);
	RETURN_IF_ERROR(prov.Construct(bitmap));
	return GetImageAsBase64PNG(&prov, buffer);
}

OP_STATUS GetNonPremultipliedBufferAsBase64PNG(UINT32* pixels, unsigned int width, unsigned int height, TempBuffer* buffer)
{
	OP_ASSERT(pixels); // Caller should have made sure

	Base64ImageEncoderScanlineProvider prov(FALSE);
	RETURN_IF_ERROR(prov.Construct(pixels, width, height));
	return GetImageAsBase64PNG(&prov, buffer);
}

#endif // IMG_BITMAP_TO_BASE64PNG

#ifdef IMG_BITMAP_TO_BASE64JPG
# ifdef JAYPEG_ENCODE_SUPPORT
#  include "modules/jaypeg/jayencoder.h"
#  include "modules/jaypeg/jayencodeddata.h"
#  define JPEG_BUFFER_SIZE 1024
class JpegData : public JayEncodedData
{
public:
	JpegData(TempBuffer* buf) : buffer(buf), tmp(NULL), tmplen(0) {}
	virtual ~JpegData() { OP_DELETEA(tmp); }

	OP_STATUS init()
	{
		tmp = OP_NEWA(char, JPEG_BUFFER_SIZE);
		if (!tmp)
			return OpStatus::ERR_NO_MEMORY;
		return OpStatus::OK;
	}
	virtual OP_STATUS dataReady(const void* data, unsigned int datalen)
	{
		const char* d = (const char*)data;

		while (datalen)
		{
			unsigned tmp_rem = (JPEG_BUFFER_SIZE/3)*3 - tmplen;
			unsigned copylen = MIN(datalen, tmp_rem);

			op_memcpy(tmp+tmplen, d, copylen);

			tmplen += copylen;
			d += copylen;
			datalen -= copylen;

			if (tmplen == (JPEG_BUFFER_SIZE/3)*3)
				RETURN_IF_ERROR(flush());
		}
		return OpStatus::OK;
	}
	OP_STATUS flush()
	{
		if (tmplen == 0)
			return OpStatus::OK;

		OP_STATUS result = AppendAsBase64(buffer, (char*)tmp, tmplen);
		tmplen = 0;
		return result;
	}
	TempBuffer* buffer;
	char* tmp;
	int tmplen;
};
#  undef JPEG_BUFFER_SIZE

static OP_STATUS GetImageAsBase64JPG(Base64ImageEncoderScanlineProvider* prov, TempBuffer* buffer, int quality)
{
	OP_ASSERT(prov); // Caller should have made sure

	JpegData data(buffer);
	JayEncoder enc;

	RETURN_IF_ERROR(data.init());

	RETURN_IF_ERROR(enc.init("jfif", quality, prov->width, prov->height, &data));


	OP_STATUS result = OpStatus::OK;
	for (unsigned int y = 0; y < prov->height && OpStatus::IsSuccess(result); ++y)
	{
		UINT32* scanline = prov->GetScanline(y);

		result = enc.encodeScanline(scanline);
	}

	RETURN_IF_ERROR(result);

	return data.flush();
}
OP_STATUS GetOpBitmapAsBase64JPG(OpBitmap* bitmap, TempBuffer* buffer, int quality/* = 85*/)
{
	OP_ASSERT(bitmap); // Caller should have made sure
	
	Base64ImageEncoderScanlineProvider prov(TRUE);
	RETURN_IF_ERROR(prov.Construct(bitmap));
	return GetImageAsBase64JPG(&prov, buffer, quality);
}
OP_STATUS GetNonPremultipliedBufferAsBase64JPG(UINT32* pixels, unsigned int width, unsigned int height, TempBuffer* buffer, int quality/* = 85*/)
{
	OP_ASSERT(pixels); // Caller should have made sure

	Base64ImageEncoderScanlineProvider prov(TRUE);
	RETURN_IF_ERROR(prov.Construct(pixels, width, height));
	return GetImageAsBase64JPG(&prov, buffer, quality);
}
# endif // JAYPEG_ENCODE_SUPPORT
#endif // IMG_BITMAP_TO_BASE64JPG
