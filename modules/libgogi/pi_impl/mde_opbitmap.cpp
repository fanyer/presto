/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#if !defined(LIBGOGI_PLATFORM_IMPLEMENTS_OPBITMAP) && !defined(VEGA_OPPAINTER_SUPPORT)
#include "modules/libgogi/pi_impl/mde_opbitmap.h"
#include "modules/libgogi/pi_impl/mde_oppainter.h"

#include "modules/libgogi/mde.h"

/*static*/
OP_STATUS
OpBitmap::Create(OpBitmap **bitmap, UINT32 width, UINT32 height,
								 BOOL transparent,
								 BOOL alpha,
								 UINT32 transpcolor, INT32 indexed,
								 BOOL must_support_painter)
{
	OP_ASSERT(bitmap);

    if (!width  || !height)
        return OpStatus::ERR;

	OpBitmap* new_bitmap = OP_NEW(MDE_OpBitmap, ());

	if (new_bitmap == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	OP_STATUS ret = ((MDE_OpBitmap*)new_bitmap)->Init(width, height, transparent, alpha,
											 transpcolor, indexed, must_support_painter);
	if (OpStatus::IsSuccess(ret))
	{
		*bitmap = new_bitmap;
	}
	else
	{
		OP_DELETE(new_bitmap);
		*bitmap = 0;
	}

	return ret;
}

/*static*/
OP_STATUS
OpBitmap::CreateFromIcon(OpBitmap **bitmap, const uni_char* icon_path)
{
	OP_ASSERT(FALSE);
	return OpStatus::ERR;
}

MDE_OpBitmap::MDE_OpBitmap()
		: painter(NULL)
		, must_support_painter(FALSE)
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
		, current_split_bitmap_elm(NULL)
#endif // MDE_SUPPORT_SPLIT_BITMAPS
		, buffer(NULL)
#ifdef LIBGOGI_SUPPORT_CREATE_TILE
		, current_tile(NULL)
#endif // LIBGOGI_SUPPORT_CREATE_TILE
		, m_initStatus(OpStatus::ERR)
{
}

OP_STATUS MDE_OpBitmap::CreateInternalBuffers(MDE_FORMAT format, int extra)
{
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (!must_support_painter)
		return CreateSplitBuffers(format);
#endif // MDE_SUPPORT_SPLIT_BITMAPS
#ifdef MDE_LIMIT_IMAGE_SIZE
	buffer = MDE_CreateBuffer(real_width, real_height, format, extra);
#else
	buffer = MDE_CreateBuffer(width, height, format, extra);
#endif
	if (!buffer)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	if (hasAlpha)
		buffer->method = MDE_METHOD_ALPHA;
	if (extra)
	{
		// make all palete entries white by default
		op_memset(buffer->pal, 255, extra*3);
	}
	return OpStatus::OK;
}


OP_STATUS MDE_OpBitmap::Init(UINT32 w, UINT32 h, BOOL transparent, BOOL alpha,
							  UINT32 transcol, INT32 indexed, BOOL must_support_painter)
{
	width = w;
	height = h;
#ifdef MDE_LIMIT_IMAGE_SIZE
	scale = 1;
	while ((w*h) / (scale*scale) > MDE_MAX_IMAGE_SIZE)
		++scale;
	// Make sure the real width/height is always big enough
	real_width = (w+scale-1)/scale;
	real_height = (h+scale-1)/scale;
#endif // MDE_LIMIT_IMAGE_SIZE
	hasAlpha = alpha;
	isTransparent = transparent;
	transparentColor = transcol;

	MDE_FORMAT format;
	int extra = 0;
#ifdef SUPPORT_INDEXED_OPBITMAP
	isIndexed = indexed != 0;
	if (indexed)
	{
		// FIXME: it is probably better to allocate palette dynamicly
		format = MDE_FORMAT_INDEX8;
		extra = 256;
	}
	else
#endif // SUPPORT_INDEXED_OPBITMAP
#ifdef USE_16BPP_OPBITMAP

	if (!hasAlpha && !isTransparent)
	{
		format = MDE_FORMAT_RGB16;
	}
 	else
# ifdef USE_16BPP_ALPHA_OPBITMAP
 	{
 		format = MDE_FORMAT_RGBA16;
 	}
# endif // USE_16BPP_ALPHA_OPBITMAP
#endif // USE_16BPP_OPBITMAP

#if !defined(USE_16BPP_ALPHA_OPBITMAP) || !defined(USE_16BPP_OPBITMAP)
	{
#ifdef PLATFORM_COLOR_IS_RGBA
		format = MDE_FORMAT_RGBA32;
#else
		format = MDE_FORMAT_BGRA32;
#endif // PLATFORM_COLOR_IS_RGBA
	}
#endif // !USE_16BPP_ALPHA_OPBITMAP || !USE_16BPP_OPBITMAP

	this->must_support_painter = must_support_painter;

    if (must_support_painter && !Supports(SUPPORTS_PAINTER))
        return OpStatus::ERR_NOT_SUPPORTED;

	m_initStatus = CreateInternalBuffers(format, extra);
	return m_initStatus;
}

/*virtual*/ BOOL
MDE_OpBitmap::Supports(SUPPORTS supports) const
{
	switch (supports)
	{
	case SUPPORTS_INDEXED:
		return TRUE;
#ifndef MDE_SUPPORT_HW_PAINTING
	case SUPPORTS_PAINTER:
#if defined(MDE_SUPPORT_SPLIT_BITMAPS) || defined(MDE_LIMIT_IMAGE_SIZE)
		return must_support_painter;
#endif
		return TRUE;
#endif // MDE_SUPPORT_HW_PAINTING
	case SUPPORTS_POINTER:
		{
# ifdef PLATFORM_COLOR_IS_RGBA
			MDE_FORMAT needed_format = MDE_FORMAT_RGBA32;
# else
			MDE_FORMAT needed_format = MDE_FORMAT_BGRA32;
# endif // PLATFORM_COLOR_IS_RGBA
#if defined(MDE_SUPPORT_SPLIT_BITMAPS) || defined(MDE_LIMIT_IMAGE_SIZE)
			return must_support_painter && buffer->format == needed_format;
#else
			return buffer->format == needed_format;
#endif
		}
#ifdef LIBGOGI_SUPPORT_CREATE_TILE
# ifdef SUPPORT_INDEXED_OPBITMAP
 	case SUPPORTS_CREATETILE:
		return !isIndexed;
# else
 	case SUPPORTS_CREATETILE:
		return TRUE;
# endif // SUPPORT_INDEXED_OPBITMAP
#endif // LIBGOGI_SUPPORT_CREATE_TILE

// 	case SUPPORTS_EFFECT:
	default:
		return FALSE;
	}
}

OpPainter*
MDE_OpBitmap::GetPainter()
{
#ifdef MDE_PLATFORM_IMPLEMENTS_OPPAINTER
	extern OpPainter* get_op_painter();
	painter = get_op_painter();
#else // MDE_PLATFORM_IMPLEMENTS_OPPAINTER
#if defined(MDE_SUPPORT_SPLIT_BITMAPS) || defined(MDE_LIMIT_IMAGE_SIZE)
	if (!must_support_painter)
		return NULL;
#endif
#if !defined(MDE_SUPPORT_HW_PAINTING)
	if (painter == NULL)
		painter = OP_NEW(MDE_OpPainter, ());

	if (painter != NULL)
		((MDE_OpPainter*)painter)->beginPaint(buffer);
#endif
#endif // MDE_PLATFORM_IMPLEMENTS_OPPAINTER

	return painter;
}

/*virtual*/
void
MDE_OpBitmap::ReleasePainter()
{
#ifndef MDE_PLATFORM_IMPLEMENTS_OPPAINTER
	if (painter)
		((MDE_OpPainter*)painter)->endPaint();
#endif // !MDE_PLATFORM_IMPLEMENTS_OPPAINTER
}

/*virtual*/
OP_STATUS
MDE_OpBitmap::InitStatus()
{
	return m_initStatus;
}

MDE_OpBitmap::~MDE_OpBitmap()
{
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	split_bitmap_list.Clear();
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	if (buffer)
		MDE_DeleteBuffer(buffer);

#ifdef LIBGOGI_SUPPORT_CREATE_TILE
	OP_DELETE(current_tile);
#endif // LIBGOGI_SUPPORT_CREATE_TILE
	OP_DELETE(painter);
}

UINT32 MDE_OpBitmap::GetBytesPerLine() const{
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	SplitBitmapElm *bmp = FindSplitBitmapElm(0);
	if (bmp)
	{
#ifdef MDE_LIMIT_IMAGE_SIZE
		return width*MDE_GetBytesPerPixel(bmp->buffer->format);
#else
		return bmp->buffer->stride;
#endif
	}
#endif // MDE_SUPPORT_SPLIT_BITMAPS
#ifdef MDE_LIMIT_IMAGE_SIZE
	return width*MDE_GetBytesPerPixel(buffer->format);
#else
	return buffer->stride;
#endif
}

/*virtual*/
OP_STATUS
MDE_OpBitmap::AddLine(void* data, INT32 line)
{
	MDE_BUFFER* buffer = this->buffer;
	int stride;
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	OP_ASSERT(must_support_painter ? buffer != NULL : !split_bitmap_list.Empty());
#else
	OP_ASSERT(buffer);
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	if (line >= (INT32)height)
		return OpStatus::ERR;
#ifdef MDE_LIMIT_IMAGE_SIZE
	if (line%scale)
		return OpStatus::OK;
	line /= scale;
#endif // MDE_LIMIT_IMAGE_SIZE
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (!must_support_painter)
	{
		SplitBitmapElm* split_bitmap_elm = FindSplitBitmapElm(line);
		OP_ASSERT(split_bitmap_elm != NULL);
		buffer = split_bitmap_elm->buffer;
		line -= split_bitmap_elm->y_pos;
	}
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	void *bufferdata = MDE_LockBuffer(buffer, MDE_MakeRect(0, line, buffer->w, 1), stride, false);
	OP_ASSERT(bufferdata);
	if (!bufferdata)
		return OpStatus::ERR;
#ifdef PLATFORM_COLOR_IS_RGBA
	if (buffer->format == MDE_FORMAT_RGBA32)
	{
#ifdef MDE_LIMIT_IMAGE_SIZE
		for (int pix = 0; pix < real_width; ++pix)
		{
			((UINT32*)bufferdata)[pix] = ((UINT32*)data)[pix*scale];
		}
#else
		op_memcpy(static_cast<char*>(bufferdata),
			   data, width*4);
#endif // MDE_LIMIT_IMAGE_SIZE
	}
# ifdef USE_16BPP_OPBITMAP
	else if (buffer->format == MDE_FORMAT_RGB16)
	{
		UINT16 *bufdata = (UINT16*)bufferdata;
		// copy the line to the buffer
#ifdef MDE_LIMIT_IMAGE_SIZE
		for( unsigned int pixel = 0; pixel < real_width; ++pixel )
#else
		for( unsigned int pixel = 0; pixel < width; ++pixel )
#endif
		{
#ifdef MDE_LIMIT_IMAGE_SIZE
			UINT8* pixel_data = (UINT8*)data+4*pixel*scale;
#else
			UINT8* pixel_data = (UINT8*)data+4*pixel;
#endif
			int r = pixel_data[0];
			int g = pixel_data[1];
			int b = pixel_data[2];
			bufdata[pixel] = (UINT16)MDE_RGB16(r, g, b);
		}
	}
# endif // USE_16BPP_OPBITMAP
#elif defined(USE_16BPP_OPBITMAP)
	if (buffer->format == MDE_FORMAT_BGRA32)
	{
#ifdef MDE_LIMIT_IMAGE_SIZE
		for (int pix = 0; pix < real_width; ++pix)
		{
			((UINT32*)bufferdata)[pix] = ((UINT32*)data)[pix*scale];
		}
#else
		op_memcpy(static_cast<char*>(bufferdata),
			   data, width*4);
#endif // MDE_LIMIT_IMAGE_SIZE
	}
	else if (buffer->format == MDE_FORMAT_RGB16)
	{
		UINT16 *bufdata = (UINT16*)bufferdata;
		// copy the line to the buffer
#ifdef MDE_LIMIT_IMAGE_SIZE
		for( unsigned int pixel = 0; pixel < real_width; ++pixel )
		{
			bufdata[pixel] = (UINT16)MDE_RGB16(((((UINT32*)data)[pixel*scale]&0xff0000)>>16),
						 ((((UINT32*)data)[pixel*scale]&0xff00)>>8),
						 (((UINT32*)data)[pixel*scale]&0xff));
		}
#else
		for( unsigned int pixel = 0; pixel < width; ++pixel )
		{
			bufdata[pixel] = (UINT16)MDE_RGB16(((((UINT32*)data)[pixel]&0xff0000)>>16),
						 ((((UINT32*)data)[pixel]&0xff00)>>8),
						 (((UINT32*)data)[pixel]&0xff));
		}
#endif
	}
	else if (buffer->format == MDE_FORMAT_RGBA16)
	{
		UINT16 *bufdata = (UINT16*)bufferdata;
		// copy the line to the buffer
#ifdef MDE_LIMIT_IMAGE_SIZE
		for( unsigned int pixel = 0; pixel < real_width; ++pixel ){
			bufdata[pixel] = (UINT16)MDE_RGBA16(((((UINT32*)data)[pixel*scale]&0xff0000)>>16),
						 ((((UINT32*)data)[pixel*scale]&0xff00)>>8),
						 (((UINT32*)data)[pixel*scale]&0xff),
						 (((UINT32*)data)[pixel*scale]>>24));
		}
#else
		for( unsigned int pixel = 0; pixel < width; ++pixel ){
			bufdata[pixel] = (UINT16)MDE_RGBA16(((((UINT32*)data)[pixel]&0xff0000)>>16),
						 ((((UINT32*)data)[pixel]&0xff00)>>8),
						 (((UINT32*)data)[pixel]&0xff),
						 (((UINT32*)data)[pixel]>>24));
		}
#endif
	}
#else // BGRA32
	// the rowstride _must_ be a multiple of 4
	if (buffer->format == MDE_FORMAT_BGRA32)
	{
		UINT32 *bufdata = (UINT32*)bufferdata;
		// copy the line to the buffer
#ifdef MDE_LIMIT_IMAGE_SIZE
		for( unsigned int pixel = 0; pixel < real_width; ++pixel ){
			bufdata[pixel] = MDE_RGBA(
					((((UINT32*)data)[pixel*scale]&0xff0000)>>16),
					((((UINT32*)data)[pixel*scale]&0xff00)>>8),
					(((UINT32*)data)[pixel*scale]&0xff),
					(((UINT32*)data)[pixel*scale]>>24));
		}
#else
		for( unsigned int pixel = 0; pixel < width; ++pixel ){
			bufdata[pixel] = MDE_RGBA(
					((((UINT32*)data)[pixel]&0xff0000)>>16),
					((((UINT32*)data)[pixel]&0xff00)>>8),
					(((UINT32*)data)[pixel]&0xff),
					(((UINT32*)data)[pixel]>>24));
		}
#endif
	}
#endif // USE_16BPP_OPBITMAP
// FIXME: indexed opbitmap will fail if this is called, is that correct behaviour?
	else
	{
		OP_ASSERT(FALSE);
		MDE_UnlockBuffer(buffer, false);
		return OpStatus::ERR;
	}
	MDE_UnlockBuffer(buffer, true);
	return OpStatus::OK;
}

#ifdef SUPPORT_INDEXED_OPBITMAP
BOOL MDE_OpBitmap::SetPalette(UINT8* pal,  UINT32 num_colors)
{
	MDE_BUFFER* buffer = this->buffer;
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	OP_ASSERT(must_support_painter ? buffer != NULL : !split_bitmap_list.Empty());
#else
	OP_ASSERT(buffer);
#endif // MDE_SUPPORT_SPLIT_BITMAPS

#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (!must_support_painter)
		buffer = ((SplitBitmapElm*)split_bitmap_list.First())->buffer;
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	if (!isIndexed ||buffer->format != MDE_FORMAT_INDEX8 || !buffer->pal)
		return FALSE;
	if (num_colors > 256)
		return FALSE;
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (!must_support_painter)
	{
		for (SplitBitmapElm* elm = (SplitBitmapElm*)split_bitmap_list.First();
			 elm != NULL;
			 elm = (SplitBitmapElm*)elm->Suc())
		{
			op_memcpy(elm->buffer->pal, pal, num_colors*3);
		}
	}
	else
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	op_memcpy(buffer->pal, pal, num_colors*3);
	return TRUE;
}

BOOL MDE_OpBitmap::GetPalette(UINT8* pal) const
{
	MDE_BUFFER* buffer = this->buffer;
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (!must_support_painter)
		buffer = ((SplitBitmapElm*)split_bitmap_list.First())->buffer;
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	if (!buffer->pal)
		return FALSE;
	op_memcpy(pal, buffer->pal, 256 * 3);
	return TRUE;
}


/*virtual*/
OP_STATUS
MDE_OpBitmap::AddIndexedLine(void* data,  INT32 line)
{
	MDE_BUFFER* buffer = this->buffer;
	OP_ASSERT(isIndexed);
	if (!isIndexed)
		return OpStatus::ERR;
	if (line >= (INT32)height)
		return OpStatus::ERR;
#ifdef MDE_LIMIT_IMAGE_SIZE
	if (line%scale)
		return OpStatus::OK;
	line /= scale;
#endif // MDE_LIMIT_IMAGE_SIZE
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (!must_support_painter)
	{
		SplitBitmapElm* split_bitmap_elm = FindSplitBitmapElm(line);
		OP_ASSERT(split_bitmap_elm);
		line -= split_bitmap_elm->y_pos;
		buffer = split_bitmap_elm->buffer;
	}
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	OP_ASSERT(buffer);
	int stride;
	void *bufferdata = MDE_LockBuffer(buffer, MDE_MakeRect(0, line, buffer->w, 1), stride, false);
	OP_ASSERT(bufferdata);
	if (!bufferdata)
		return OpStatus::ERR;
	// copy the data
#ifdef MDE_LIMIT_IMAGE_SIZE
	for (unsigned int pixel = 0; pixel < real_width; ++pixel)
	{
		((unsigned char*)bufferdata)[pixel] = ((unsigned char*)data)[pixel*scale];
	}
#else
	op_memcpy(((unsigned char*)bufferdata), data, stride);
#endif
	MDE_UnlockBuffer(buffer, true);
	return OpStatus::OK;
}
#endif // SUPPORT_INDEXED_OPBITMAP

BOOL MDE_OpBitmap::SetColorInternal(MDE_BUFFER* buffer, UINT8* color, const OpRect& rect)
{
	unsigned int old_col = buffer->col;
	if (buffer->format == MDE_FORMAT_INDEX8)
		MDE_SetColor(*((UINT32*)color), buffer);
	else
		MDE_SetColor(MDE_RGBA(	(((*(UINT32*)color)&0xff0000)>>16),
								(((*(UINT32*)color)&0xff00)>>8),
								((*(UINT32*)color)&0xff),
								((*(UINT32*)color)>>24)), buffer);
	MDE_DrawRectFill(MDE_MakeRect(rect.x, rect.y, rect.width, rect.height), buffer, false);
	MDE_SetColor(old_col, buffer);
	return TRUE;
}

BOOL
MDE_OpBitmap::SetColor(UINT8* color, BOOL all_transparent, const OpRect *rect)
{
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	OP_ASSERT(!must_support_painter || buffer);
#else
	OP_ASSERT(buffer);
#endif // !MDE_SUPPORT_SPLIT_BITMAPS
	OpRect fs;

	if (color == NULL && all_transparent == FALSE)
		return TRUE;
	if (rect == NULL)
	{
		fs.x = 0;
		fs.y = 0;
		fs.width = width;
		fs.height = height;
		rect = &fs;
	}
#ifdef MDE_LIMIT_IMAGE_SIZE
	// FIXME: should this be rounded?
	fs.x = rect->x/scale;
	fs.y = rect->y/scale;
	fs.width = (rect->x+rect->width)/scale - fs.x;
	fs.height = (rect->y+rect->height)/scale - fs.y;
	rect = &fs;
#endif
	if (all_transparent)
	{
		if (!isTransparent)
			transparentColor = 0;
		color = (UINT8*)&transparentColor;
	}
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (!must_support_painter)
	{
		BOOL ret = FALSE;
		for (SplitBitmapElm* elm = (SplitBitmapElm*)split_bitmap_list.First();
			 elm != NULL;
			 elm = (SplitBitmapElm*)elm->Suc())
		{
			MDE_BUFFER* buffer = elm->buffer;
			// We need to make sure that the rect is correct.
			OpRect r(0, elm->y_pos, buffer->w, buffer->h);
			r.IntersectWith(*rect);
			r.y -= elm->y_pos;

			if (!r.IsEmpty())
			{
				ret = SetColorInternal(buffer, color, r);
			}
		}
		return ret;
	}
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	return SetColorInternal(buffer, color, *rect);
}

/*virtual*/
BOOL
MDE_OpBitmap::GetLineData(void* data, UINT32 line) const
{
	MDE_BUFFER* buffer = this->buffer;
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (must_support_painter)
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	{
		OP_ASSERT(buffer);
		if (!buffer)
		{
			return FALSE;
		}
	}
	if( line >= height )
		return FALSE;
#ifdef MDE_LIMIT_IMAGE_SIZE
	line /= scale;
#endif // MDE_LIMIT_IMAGE_SIZE
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (!must_support_painter)
	{
		SplitBitmapElm* split_bitmap_elm = FindSplitBitmapElm((INT32)line);
		OP_ASSERT(split_bitmap_elm != NULL);
		buffer = split_bitmap_elm->buffer;
		line -= split_bitmap_elm->y_pos;
	}
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	int stride;
	void *bufferdata = MDE_LockBuffer(buffer, MDE_MakeRect(0, line, buffer->w, 1), stride, true);
	OP_ASSERT(bufferdata);
	if (!bufferdata)
		return FALSE;
#ifdef PLATFORM_COLOR_IS_RGBA
	if (buffer->format == MDE_FORMAT_RGBA32)
	{
#ifdef MDE_LIMIT_IMAGE_SIZE
		for (int pixel = 0; pixel < width; ++pixel)
		{
			((UINT32*)data)[pixel] = ((UINT32*)bufferdata)[pixel/scale];
		}
#else
		op_memcpy(data, static_cast<char*>(bufferdata), width*4);
#endif
		MDE_UnlockBuffer(buffer, false);
		return TRUE;
	}
	else if (buffer->format == MDE_FORMAT_RGB16)
	{
		UINT16 *bufdata = (UINT16*)bufferdata;
		for( unsigned int pixel = 0; pixel < width; ++pixel )
		{
#ifdef MDE_LIMIT_IMAGE_SIZE
			bufdata = ((UINT16*)bufferdata)+pixel/scale;
#endif
			UINT8* pixel_data = ((UINT8*)data)+4*pixel;
			pixel_data[0]= MDE_COL_R16(*bufdata);
			pixel_data[1]= MDE_COL_G16(*bufdata);
			pixel_data[2]= MDE_COL_B16(*bufdata);
			pixel_data[3]= 255;
#ifndef MDE_LIMIT_IMAGE_SIZE
			++bufdata;
#endif
		}
	}
# ifdef SUPPORT_INDEXED_OPBITMAP
	else if (buffer->format == MDE_FORMAT_INDEX8)
	{
		UINT8 *bufdata = (UINT8*)bufferdata;
		for( unsigned int pixel = 0; pixel < width; ++pixel )
		{
			UINT8* pixel_data = ((UINT8*)data)+4*pixel;
#ifdef MDE_LIMIT_IMAGE_SIZE
			UINT32 color = bufdata[pixel/scale];
#else
			UINT32 color = bufdata[pixel];
#endif
			pixel_data[0]= buffer->pal[color*3];
			pixel_data[1]= buffer->pal[color*3+1];
			pixel_data[2]= buffer->pal[color*3+2];
			pixel_data[3]= (isTransparent && color == transparentColor) ? 0 : 255;
		}
	}
# endif
#elif defined USE_16BPP_OPBITMAP
	if (buffer->format == MDE_FORMAT_RGB16)
	{
		UINT16 *bufdata = (UINT16*)bufferdata;
		for( unsigned int pixel = 0; pixel < width; ++pixel ){
#ifdef MDE_LIMIT_IMAGE_SIZE
			bufdata = ((UINT16*)bufferdata)+pixel/scale;
#endif
			((UINT32*)data)[pixel] = (255u<<24)|
				(MDE_COL_R16(*bufdata)<<16)|
				(MDE_COL_G16(*bufdata)<<8)|
				MDE_COL_B16(*bufdata);
#ifndef MDE_LIMIT_IMAGE_SIZE
			++bufdata;
#endif
		}
	}
	else if (buffer->format == MDE_FORMAT_RGBA16)
	{
		UINT16 *bufdata = (UINT16*)bufferdata;
		for( unsigned int pixel = 0; pixel < width; ++pixel ){
#ifdef MDE_LIMIT_IMAGE_SIZE
			bufdata = ((UINT16*)bufferdata)+pixel/scale;
#endif
			((UINT32*)data)[pixel] = (MDE_COL_A16A(*bufdata)<<24)|
				(MDE_COL_R16A(*bufdata)<<16)|
				(MDE_COL_G16A(*bufdata)<<8)|
				MDE_COL_B16A(*bufdata);
#ifndef MDE_LIMIT_IMAGE_SIZE
			++bufdata;
#endif
		}
	}
#else
	// the rowstride _must_ be a multiple of 4
	if (buffer->format == MDE_FORMAT_BGRA32)
	{
		UINT32 *bufdata = (UINT32*)bufferdata;
		for( unsigned int pixel = 0; pixel < width; ++pixel ){
#ifdef MDE_LIMIT_IMAGE_SIZE
			((UINT32*)data)[pixel] = (MDE_COL_A(bufdata[pixel/scale])<<24)|
				(MDE_COL_R(bufdata[pixel/scale])<<16)|
				(MDE_COL_G(bufdata[pixel/scale])<<8)|
				MDE_COL_B(bufdata[pixel/scale]);
#else
			((UINT32*)data)[pixel] = (MDE_COL_A(bufdata[pixel])<<24)|
				(MDE_COL_R(bufdata[pixel])<<16)|
				(MDE_COL_G(bufdata[pixel])<<8)|
				MDE_COL_B(bufdata[pixel]);
#endif
		}
	}
#endif // USE_16BPP_OPBITMAP
#ifdef SUPPORT_INDEXED_OPBITMAP
	else if (buffer->format == MDE_FORMAT_INDEX8)
	{
		UINT8 *bufdata = (UINT8*)bufferdata;
		for( unsigned int pixel = 0; pixel < width; ++pixel ){
#ifdef MDE_LIMIT_IMAGE_SIZE
			if (isTransparent && bufdata[pixel/scale]==transparentColor)
				((UINT32*)data)[pixel] = 0;
			else
				((UINT32*)data)[pixel] = (255u<<24)|
					(buffer->pal[bufdata[pixel/scale]*3] << 16) |
					(buffer->pal[bufdata[pixel/scale]*3+1] << 8) |
					buffer->pal[bufdata[pixel/scale]*3+2];
#else
			if (isTransparent && bufdata[pixel]==transparentColor)
				((UINT32*)data)[pixel] = 0;
			else
				((UINT32*)data)[pixel] = (255u<<24)|
					(buffer->pal[bufdata[pixel]*3] << 16) |
					(buffer->pal[bufdata[pixel]*3+1] << 8) |
					buffer->pal[bufdata[pixel]*3+2];
#endif
		}
	}
#endif
	else if (buffer->format == MDE_FORMAT_BGRA32)
	{
		op_memcpy(data, bufferdata, width * 4);
	}
	else
	{
		OP_ASSERT(FALSE);
		MDE_UnlockBuffer(buffer, false);
		return FALSE;
	}
	MDE_UnlockBuffer(buffer, false);
	return TRUE;
}

#ifdef SUPPORT_INDEXED_OPBITMAP

BOOL MDE_OpBitmap::GetIndexedLineData(void* data, UINT32 line) const
{
	MDE_BUFFER* buffer = this->buffer;
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (must_support_painter)
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	{
		OP_ASSERT(buffer);
		if (!buffer)
		{
			return FALSE;
		}
	}
	if( line >= height )
		return FALSE;
#ifdef MDE_LIMIT_IMAGE_SIZE
	line /= scale;
#endif // MDE_LIMIT_IMAGE_SIZE
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (!must_support_painter)
	{
		SplitBitmapElm* split_bitmap_elm = FindSplitBitmapElm((INT32)line);
		OP_ASSERT(split_bitmap_elm != NULL);
		buffer = split_bitmap_elm->buffer;
		line -= split_bitmap_elm->y_pos;
	}
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	int stride;
	void *bufferdata = MDE_LockBuffer(buffer, MDE_MakeRect(0, line, buffer->w, 1), stride, true);
	OP_ASSERT(bufferdata);
	if (!bufferdata)
		return FALSE;

	OP_ASSERT(buffer->format == MDE_FORMAT_INDEX8);
	if (buffer->format == MDE_FORMAT_INDEX8)
	{
		op_memcpy(data, bufferdata, width);
		MDE_UnlockBuffer(buffer, false);
		return TRUE;
	}

	MDE_UnlockBuffer(buffer, false);
	return FALSE;
}

#endif // SUPPORT_INDEXED_OPBITMAP

UINT32 MDE_OpBitmap::Width() const{
	return width;
}
UINT32 MDE_OpBitmap::Height() const{
	return height;
}
UINT8 MDE_OpBitmap::GetBpp() const{
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	MDE_BUFFER* buffer = this->buffer;
	if (!must_support_painter)
		buffer = ((SplitBitmapElm*)split_bitmap_list.First())->buffer;
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	return buffer->ps * 8;
}
BOOL MDE_OpBitmap::HasAlpha() const{
	return hasAlpha;
}
BOOL MDE_OpBitmap::IsTransparent() const{
	return isTransparent;
}

/*virtual*/
UINT32
MDE_OpBitmap::GetTransparentColorIndex() const
{
	return transparentColor;
}

MDE_BUFFER *MDE_OpBitmap::GetMDEBuffer()
{
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	OP_ASSERT(must_support_painter);
#endif
	return buffer;
}

#ifdef OPBITMAP_POINTER_ACCESS
void* MDE_OpBitmap::GetPointer(AccessType access_type)
#else
void* MDE_OpBitmap::GetPointer() const
#endif // OPBITMAP_POINTER_ACCESS
{
#if defined(MDE_SUPPORT_SPLIT_BITMAPS) || defined(MDE_LIMIT_IMAGE_SIZE)
	if (!must_support_painter)
		return NULL;
#endif
	int stride;
	void *bufferdata = MDE_LockBuffer(buffer, MDE_MakeRect(0, 0, buffer->w, buffer->h), stride, true);
	if (bufferdata && stride != buffer->stride)
	{
		MDE_UnlockBuffer(buffer, false);
		return NULL;
	}
	return bufferdata;
}

#ifdef OPBITMAP_POINTER_ACCESS
void MDE_OpBitmap::ReleasePointer(BOOL changed)
#else
void MDE_OpBitmap::ReleasePointer() const
#endif // OPBITMAP_POINTER_ACCESS
{
#if defined(MDE_SUPPORT_SPLIT_BITMAPS) || defined(MDE_LIMIT_IMAGE_SIZE)
	if (!must_support_painter)
		return;
#endif
	MDE_UnlockBuffer(buffer, true);
}

#ifdef MDE_SUPPORT_SPLIT_BITMAPS

SplitBitmapElm::SplitBitmapElm(MDE_BUFFER* buf, int y)
{
	buffer = buf;
	y_pos = y;
}

SplitBitmapElm::~SplitBitmapElm()
{
	MDE_DeleteBuffer(buffer);
}

SplitBitmapElm* MDE_OpBitmap::GetFirstSplitBitmapElm()
{
	current_split_bitmap_elm = (SplitBitmapElm*)split_bitmap_list.First();
	return current_split_bitmap_elm;
}

SplitBitmapElm* MDE_OpBitmap::GetNextSplitBitmapElm()
{
	if (current_split_bitmap_elm == NULL)
	{
		return NULL;
	}
	current_split_bitmap_elm = (SplitBitmapElm*)current_split_bitmap_elm->Suc();
	return current_split_bitmap_elm;
}

OP_STATUS MDE_OpBitmap::CreateSplitBuffers(MDE_FORMAT format)
{
	int bytes_per_pixel = MDE_GetBytesPerPixel(format);
#ifdef MDE_LIMIT_IMAGE_SIZE
	int bytes_per_line = real_width * bytes_per_pixel;
#else
	int bytes_per_line = width * bytes_per_pixel;
#endif
	unsigned int max_lines = MAX(1, bytes_per_line != 0 ? (MDE_SPLIT_BITMAP_SIZE / bytes_per_line) : 0);
	unsigned int y_pos = 0;

	int colors = (format == MDE_FORMAT_INDEX8) ? 256 : 0;

	BOOL failed = FALSE;

#ifdef MDE_LIMIT_IMAGE_SIZE
	while (y_pos < real_height)
	{
		int split_height = MIN(real_height - y_pos, max_lines);
		OP_ASSERT(split_height >= 1);
		MDE_BUFFER* buffer = MDE_CreateBuffer(real_width, split_height, format, colors);
#else
	while (y_pos < height)
	{
		int split_height = MIN(height - y_pos, max_lines);
		OP_ASSERT(split_height >= 1);
		MDE_BUFFER* buffer = MDE_CreateBuffer(width, split_height, format, colors);
#endif
		if (buffer == NULL)
		{
			failed = TRUE;
			break;
		}
		if (format == MDE_FORMAT_INDEX8)
		{
			// make all palete entries white by default
			op_memset(buffer->pal, 255, 256*3);
		}

		SplitBitmapElm* elm = OP_NEW(SplitBitmapElm, (buffer, y_pos));
		if (elm == NULL)
		{
			MDE_DeleteBuffer(buffer);
			failed = TRUE;
			break;
		}
		elm->Into(&split_bitmap_list);
		y_pos += split_height;
	}

	if (failed)
	{
		split_bitmap_list.Clear();
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

SplitBitmapElm* MDE_OpBitmap::FindSplitBitmapElm(int line) const
{
	SplitBitmapElm* last_elm = NULL;
	for (SplitBitmapElm* elm = (SplitBitmapElm*)split_bitmap_list.First();
		 elm != NULL;
		 elm = (SplitBitmapElm*)elm->Suc())
	{
		if (elm->y_pos <= line)
		{
			last_elm = elm;
		}
		else
		{
			return last_elm;
		}
	}
	return last_elm;
}

#endif // MDE_SUPPORT_SPLIT_BITMAPS

#ifdef LIBGOGI_SUPPORT_CREATE_TILE

/* virtual */
OpBitmap* MDE_OpBitmap::GetTile(UINT32 horizontal_count,
								 UINT32 vertical_count)
{
	UINT32 tile_width = horizontal_count * width;
	UINT32 tile_height = vertical_count * height;
	if ( current_tile == NULL ||
		 current_tile->width < tile_width ||
		 current_tile->height < tile_height )
	{
#ifdef _DEBUG
// 		fprintf(stderr, "Had to (re)create a tile of size %dx%d\n",
// 				tile_width, tile_height);
#endif // _DEBUG
		OP_DELETE(current_tile);
		current_tile = (MDE_OpBitmap*) CreateTile(tile_width, tile_height);
	}
	else
	{
#ifdef _DEBUG
// 		fprintf(stderr, "Was able to reuse a tile of size %dx%d\n",
// 				tile_width, tile_height);
#endif // _DEBUG
	}
	return current_tile;
}

/* virtual */
void MDE_OpBitmap::ReleaseTile(OpBitmap* tile)
{
	OP_ASSERT(tile == current_tile);
	if (tile == current_tile)
	{
		OP_DELETE(tile);
		current_tile = NULL;
	}
}

/* virtual */
OpBitmap* MDE_OpBitmap::CreateTile(UINT32 tile_width, UINT32 tile_height)
{
	// The typical tile is made out of a bitmap like (1,2) or (16,1)
	// or something like that so it makes sense to use code that's
	// tuned for such sizes
//	fprintf(stderr, "Creating tile of dimensions (%u, %u) "
//	                "from bitmap of size (%u, %u (%d))\n",
//			tile_width, tile_height, width, height, (int)bits_per_pixel);
	MDE_OpBitmap *tile = OP_NEW(MDE_OpBitmap, ());
	if ( tile == NULL )
	{
		return NULL;
	}

#ifdef SUPPORT_INDEXED_OPBITMAP
	OP_ASSERT(!isIndexed); // Doesn't handle isIndexed
#endif // SUPPORT_INDEXED_OPBITMAP
	OP_STATUS status = tile->Init(tile_width, tile_height, isTransparent,
								  hasAlpha, transparentColor,
								  FALSE, // Not indexed
								  FALSE); // must_support_painter

	if (OpStatus::IsError(status))
	{
		OP_DELETE(tile);
		return NULL;
	}

	int stride;

	MDE_BUFFER* mde_buf = buffer;
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (!must_support_painter)
		mde_buf = GetFirstSplitBitmapElm()->buffer;
#endif // MDE_SUPPORT_SPLIT_BITMAPS

	UINT8* src = (UINT8*)MDE_LockBuffer(mde_buf, MDE_MakeRect(0,0,mde_buf->w, mde_buf->h), stride, true);
	if (!src)
	{
		OP_DELETE(tile);
		return NULL;
	}
	UINT8 *pixeldata_end = src + stride * mde_buf->h;
	int tstride;
	MDE_BUFFER* mde_tile_buf = tile->buffer;
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (!must_support_painter)
		mde_tile_buf = tile->GetFirstSplitBitmapElm()->buffer;

	int split_buffer_lines_left = mde_tile_buf->h;
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	UINT8 *tiledata = (UINT8*)MDE_LockBuffer(mde_tile_buf, MDE_MakeRect(0,0,tile->Width(), tile->Height()), tstride, false);
	UINT8 *linedata_end = src;
	if (!tiledata)
	{
		MDE_UnlockBuffer(mde_buf, false);
		OP_DELETE(tile);
		return NULL;
	}
	for ( UINT32 tile_line = 0; tile_line < tile_height; tile_line++ )
	{
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
		if (0 == split_buffer_lines_left--)
		{
			MDE_UnlockBuffer(mde_tile_buf, true);
			mde_tile_buf = tile->GetNextSplitBitmapElm()->buffer;
			tiledata = (UINT8*)MDE_LockBuffer(mde_tile_buf, MDE_MakeRect(0,0,tile->Width(), tile->Height()), tstride, false);
			if (!tiledata)
			{
				MDE_UnlockBuffer(mde_buf, false);
				OP_DELETE(tile);
				return NULL;
			}
			split_buffer_lines_left = mde_tile_buf->h - 1; // -1 because we are now in first iteration for this buffer
		}
#endif // MDE_SUPPORT_SPLIT_BITMAPS

		UINT8 *linedata_start = linedata_end;
		linedata_end += stride;

		if (stride <= tstride)
		{
			// The tiled bitmap is wider than the original
			int tile_line_left = tstride;
			while(tile_line_left >= stride)
			{
				// Normally a memcpy would be nice here, but rowstride
				// is often 3 or 6 bytes and memcpy is too slow for
				// that kind of size. It _is_ faster when compiling
				// without optimizations, but who cares about that.
#if 0
				op_memcpy(tiledata, linedata_start, stride);
				tiledata += stride;
#else
				// Duff's device. Ugly but fast
				int signed_width = stride;
				UINT8* linedata = linedata_start;
				switch(signed_width & 7)
				{
					while(signed_width>0)
					{
					case 0:
						*(tiledata++) = *(linedata++);
					case 7:
						*(tiledata++) = *(linedata++);
					case 6:
						*(tiledata++) = *(linedata++);
					case 5:
						*(tiledata++) = *(linedata++);
					case 4:
						*(tiledata++) = *(linedata++);
					case 3:
						*(tiledata++) = *(linedata++);
					case 2:
						*(tiledata++) = *(linedata++);
					case 1:
						*(tiledata++) = *(linedata++);
						signed_width -= 8;
					} // end while
				} // end switch
#endif // memcpy or Duff's device
				tile_line_left -= stride;
			}
			if (tile_line_left > 0)
			{
				op_memcpy(tiledata, linedata_start, tile_line_left);
				tiledata += tile_line_left; // wrap to next tile line
			}
		}
		else
		{
			// The original bitmap is wider than the tiled one
			op_memcpy(tiledata, linedata_start, tstride);

			tiledata += tstride;
		}

		if ( linedata_end == pixeldata_end )
		{
			// We're at the end of the original bitmap. Start with line
			// 1 again.
			linedata_end = src;
		}
	}
	MDE_UnlockBuffer(mde_buf, false);
	MDE_UnlockBuffer(mde_tile_buf, true);
	return tile;
}

#endif // LIBGOGI_SUPPORT_CREATE_TILE

#endif // !LIBGOGI_PLATFORM_IMPLEMENTS_OPBITMAP && !VEGA_OPPAINTER_SUPPORT
