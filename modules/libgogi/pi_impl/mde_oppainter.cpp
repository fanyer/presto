/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#include "core/pch.h"

#if !defined(MDE_PLATFORM_IMPLEMENTS_OPPAINTER) && !defined(VEGA_OPPAINTER_SUPPORT)

#include "modules/hardcore/mem/mem_man.h"

#include "modules/pi/OpScreenInfo.h"

#include "modules/libgogi/pi_impl/mde_oppainter.h"
#include "modules/libgogi/pi_impl/mde_opbitmap.h"
#include "modules/libgogi/pi_impl/mde_opfont.h"
#include "modules/libgogi/mde.h"

#ifdef MDEFONT_MODULE
#include "modules/mdefont/mdefont.h"
#endif // MDEFONT_MODULE

#define PRE_MUL(a) if (m_pre_alpha != 255)	(a) = (a) == 255 ? m_pre_alpha : static_cast<UINT8>((static_cast<UINT32>(m_pre_alpha + 1) * static_cast<UINT32>(a)) >> 8)

MDE_OpPainter::MDE_OpPainter()
	: m_pre_alpha(255)
{
	screen = NULL;
	current_font = NULL;
	paint_count=0;
	cliprect_stack = NULL;
}

MDE_OpPainter::~MDE_OpPainter()
{
	OP_ASSERT(cliprect_stack == NULL); // Unbalanced SetClipRect/RemoveClipRect !
	MdeOpClipRect *prevcr;
	for (prevcr = cliprect_stack; prevcr != NULL; cliprect_stack = prevcr)
	{
		prevcr = cliprect_stack->prev;
		OP_DELETE(cliprect_stack);
	}
	cliprect_stack = NULL;
}

void MDE_OpPainter::SetColor(UINT8 red, UINT8 green, UINT8 blue, UINT8 alpha)
{
	PRE_MUL(alpha);

	OP_ASSERT(screen);
	MDE_SetColor(MDE_RGBA(red, green, blue, alpha), screen);
}

UINT32 MDE_OpPainter::GetColor()
{
	OP_ASSERT(screen);
	UINT32 col = screen->col;
	return (MDE_COL_A(col) << 24) | (MDE_COL_B(col) << 16) | (MDE_COL_G(col) << 8) | (MDE_COL_R(col));
}

void MDE_OpPainter::SetFont(OpFont *font)
{
	current_font = font;
}

OP_STATUS MDE_OpPainter::SetClipRect(const OpRect &rect)
{
	MdeOpClipRect *ncr;
	OP_ASSERT(screen);
	ncr = OP_NEW(MdeOpClipRect, ());
	if (!ncr)
		return OpStatus::ERR_NO_MEMORY;
	ncr->prev = cliprect_stack;
	ncr->rect = MDE_MakeRect(rect.x, rect.y, rect.width, rect.height);
	cliprect_stack = ncr;
	if (cliprect_stack->prev)
	{
		cliprect_stack->rect = MDE_RectClip(cliprect_stack->rect, cliprect_stack->prev->rect);
	}
	MDE_SetClipRect(cliprect_stack->rect, screen);

	// Assume MDE_SetClipRect allways suceed
	return OpStatus::OK;
}

void MDE_OpPainter::RemoveClipRect()
{
	MDE_RECT ncr;
	OP_ASSERT(screen);
	if (!cliprect_stack)
		return;
	MdeOpClipRect *pcr = cliprect_stack;
	cliprect_stack = cliprect_stack->prev;
	OP_DELETE(pcr);
	if (cliprect_stack)
	{
		ncr = cliprect_stack->rect;
	}
	else
	{
		ncr = screen->outer_clip;
	}
	MDE_SetClipRect(ncr, screen);
}

void MDE_OpPainter::GetClipRect(OpRect* rect)
{
	OP_ASSERT(rect);
	if (cliprect_stack)
		*rect = OpRect(cliprect_stack->rect.x, cliprect_stack->rect.y, cliprect_stack->rect.w, cliprect_stack->rect.h);
	else
	{
		OP_ASSERT(screen);
		*rect = OpRect(screen->outer_clip.x, screen->outer_clip.y, screen->outer_clip.w, screen->outer_clip.h);
	}
}
	
void MDE_OpPainter::DrawRect(const OpRect &rect, UINT32 width)
{
	OP_ASSERT(screen);
	// top bar
	MDE_DrawRectFill(MDE_MakeRect(rect.x, rect.y, rect.width, width), screen);
	// right bar
	MDE_DrawRectFill(MDE_MakeRect(rect.x + rect.width - width, rect.y + width, width, rect.height - 2 * width), screen);
	// bottom bar
	MDE_DrawRectFill(MDE_MakeRect(rect.x, rect.y + rect.height - width, rect.width, width), screen);
	// left bar
	MDE_DrawRectFill(MDE_MakeRect(rect.x, rect.y + width, width, rect.height - 2 * width), screen);
}

void MDE_OpPainter::FillRect(const OpRect &rect)
{
	OP_ASSERT(screen);
	MDE_DrawRectFill(MDE_MakeRect(rect.x, rect.y, rect.width, rect.height), screen);
}

void MDE_OpPainter::ClearRect(const OpRect &rect)
{
	OP_ASSERT(screen);
	MDE_DrawRectFill(MDE_MakeRect(rect.x, rect.y, rect.width, rect.height), screen, false);
}


void MDE_OpPainter::DrawEllipse(const OpRect &rect, UINT32 width) 
{
	OP_ASSERT(screen);
	MDE_DrawEllipseThick(MDE_MakeRect(rect.x, rect.y, rect.width, rect.height), width, screen);
}

void MDE_OpPainter::FillEllipse(const OpRect &rect)
{
	OP_ASSERT(screen);
	MDE_DrawEllipseFill(MDE_MakeRect(rect.x, rect.y, rect.width, rect.height), screen);
}

void MDE_OpPainter::DrawLine(const OpPoint &from, UINT32 length, BOOL horizontal, UINT32 width)
{
	OP_ASSERT(screen);
	if (horizontal)
	{
		FillRect(OpRect(from.x, from.y, length, width));
	}
	else
	{
		FillRect(OpRect(from.x, from.y, width, length));
	}
}

void MDE_OpPainter::DrawLine(const OpPoint &from, const OpPoint &to, UINT32 width)
{
	OP_ASSERT(screen);
	MDE_DrawLineThick(from.x, from.y, to.x, to.y, width, screen);
}

void MDE_OpPainter::DrawString(const OpPoint &pos, const uni_char* text, UINT32 len, INT32 extra_char_spacing, short word_width)
{
	OP_ASSERT(current_font);
	if (current_font == NULL)
		return;
	if (!len)
		return;

//	MDE_DrawRect(MDE_MakeRect(pos.x, pos.y, current_font->StringWidth(text, len, extra_char_spacing), current_font->Height()),screen);
#ifdef MDEFONT_MODULE
	MDE_FONT* mdefont = ((MDE_OpFont*)current_font)->m_mdefont;

	// FIXME:OOM
	OpStatus::Ignore(MDF_DrawString(mdefont, text, len, pos.x, pos.y, extra_char_spacing, screen, word_width));
#else
	OP_ASSERT(!"MDE_Font module required");
#endif // MDEFONT_MODULE
}

void MDE_OpPainter::DrawPolygon(const OpPoint* point_array, int points, UINT32 width)
{
	OP_ASSERT(screen);
	MDE_DrawLineThick(point_array[points-1].x, point_array[points-1].y, 
					   point_array[0].x, point_array[0].y, width, screen);
	for (int i = 1; i < points; ++i)
		MDE_DrawLineThick(point_array[i-1].x, point_array[i-1].y, 
				point_array[i].x, point_array[i].y, width, screen);
}

void MDE_OpPainter::InvertRect(const OpRect &rect)
{
	OP_ASSERT(screen);
	MDE_DrawRectInvert(MDE_MakeRect(rect.x, rect.y, rect.width, rect.height), screen);
}

void MDE_OpPainter::InvertBorderRect(const OpRect &rect, int border)
{
	OP_ASSERT(screen);
	// top bar
	MDE_DrawRectInvert(MDE_MakeRect(rect.x, rect.y, rect.width, border), screen);
	// right bar
	MDE_DrawRectInvert(MDE_MakeRect(rect.x + rect.width - border, rect.y + border, border, rect.height - 2 * border), screen);
	// bottom bar
	MDE_DrawRectInvert(MDE_MakeRect(rect.x, rect.y + rect.height - border, rect.width, border), screen);
	// left bar
	MDE_DrawRectInvert(MDE_MakeRect(rect.x, rect.y + border, border, rect.height - 2 * border), screen);
}

void MDE_OpPainter::InvertBorderEllipse(const OpRect &rect, int border)
{
	OP_ASSERT(screen);
	MDE_DrawEllipseInvertFill(MDE_MakeRect(rect.x, rect.y, rect.width, rect.height), screen);
	MDE_DrawEllipseInvertFill(MDE_MakeRect(rect.x+border, rect.y+border, rect.width-2*border, rect.height-2*border), screen);
}

void MDE_OpPainter::InvertBorderPolygon(const OpPoint* point_array, int points, int border)
{
	OP_ASSERT(screen);
	MDE_DrawLineInvert(point_array[points-1].x, point_array[points-1].y, 
					   point_array[0].x, point_array[0].y, screen);
	for (int i = 1; i < points; ++i)
		MDE_DrawLineInvert(point_array[i-1].x, point_array[i-1].y, 
				point_array[i].x, point_array[i].y, screen);
}

void MDE_OpPainter::DrawBitmapClippedInternal(MDE_BUFFER* buf, const OpRect &source, OpPoint p)
{
	OP_ASSERT(screen);
	MDE_DrawBuffer(buf, MDE_MakeRect(p.x, p.y, source.width, source.height), source.x, source.y, screen);
}

void MDE_OpPainter::DrawBitmapClipped(const OpBitmap* bitmap, const OpRect &source, OpPoint p)
{
	if (m_pre_alpha != 255 && DrawBitmapClippedOpacity(bitmap, source, p, 255))
		return;

#ifdef MDE_LIMIT_IMAGE_SIZE
	UINT32 rw = ((MDE_OpBitmap*)bitmap)->RealWidth();
	UINT32 rh = ((MDE_OpBitmap*)bitmap)->RealHeight();
	if( rw != bitmap->Width() || rh != bitmap->Height())
	{
		DrawBitmapScaled(bitmap, source, OpRect(p.x, p.y, source.width, source.height));
		return;
	}
#endif // MDE_LIMIT_IMAGE_SIZE
	OP_ASSERT(screen);
	MDE_BUFFER *buf;
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (!((MDE_OpBitmap*)bitmap)->must_support_painter)
	{
		for (SplitBitmapElm* elm = ((MDE_OpBitmap*)bitmap)->GetFirstSplitBitmapElm();
			 elm != NULL;
			 elm = ((MDE_OpBitmap*)bitmap)->GetNextSplitBitmapElm())
		{
			buf = elm->buffer;
			OpRect split_r(0, elm->y_pos, buf->w, buf->h);
			split_r.IntersectWith(source);
			split_r.y -= elm->y_pos;
			if (!split_r.IsEmpty())
			{
				OpPoint split_p(p.x, p.y);
				if (source.y < elm->y_pos)
				{
					split_p.y += elm->y_pos - source.y;
				}
				buf->method = MDE_METHOD_COPY;
#ifdef SUPPORT_INDEXED_OPBITMAP
				if (buf->format == MDE_FORMAT_INDEX8 && bitmap->IsTransparent())
				{
					buf->mask = bitmap->GetTransparentColorIndex();
					buf->method = MDE_METHOD_MASK;
				}
#endif
				DrawBitmapClippedInternal(buf, split_r, split_p);
			}
		}
		return;
	}
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	buf = ((MDE_OpBitmap*)bitmap)->GetMDEBuffer();
	OP_ASSERT(buf);
	
	buf->method = MDE_METHOD_COPY;
	
// Needed to copy INDEX 8 to a format with alpha
#ifdef SUPPORT_INDEXED_OPBITMAP
	if (buf->format == MDE_FORMAT_INDEX8 && bitmap->IsTransparent())
	{
		buf->mask = bitmap->GetTransparentColorIndex();
		buf->method = MDE_METHOD_MASK;
	}
#endif
	DrawBitmapClippedInternal(buf, source, p);
}

void MDE_OpPainter::DrawBitmapClippedTransparentInternal(MDE_BUFFER* buf, const OpRect& source, OpPoint p)
{	
	MDE_DrawBuffer(buf, MDE_MakeRect(p.x, p.y, source.width, source.height), source.x, source.y, screen);
}

void MDE_OpPainter::DrawBitmapClippedTransparent(const OpBitmap* bitmap, const OpRect& source, OpPoint p)
{
	if (m_pre_alpha != 255 && DrawBitmapClippedOpacity(bitmap, source, p, 255))
		return;

#ifdef MDE_LIMIT_IMAGE_SIZE
	UINT32 rw = ((MDE_OpBitmap*)bitmap)->RealWidth();
	UINT32 rh = ((MDE_OpBitmap*)bitmap)->RealHeight();
	if( rw != bitmap->Width() || rh != bitmap->Height())
	{
		DrawBitmapScaledTransparent(bitmap, source, OpRect(p.x, p.y, source.width, source.height));
		return;
	}
#endif // MDE_LIMIT_IMAGE_SIZE
	OP_ASSERT(screen);
	MDE_BUFFER *buf;
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (!((MDE_OpBitmap*)bitmap)->must_support_painter)
	{
		for (SplitBitmapElm* elm = ((MDE_OpBitmap*)bitmap)->GetFirstSplitBitmapElm();
			 elm != NULL;
			 elm = ((MDE_OpBitmap*)bitmap)->GetNextSplitBitmapElm())
		{
			buf = elm->buffer;
			OpRect split_r(0, elm->y_pos, buf->w, buf->h);
			split_r.IntersectWith(source);
			split_r.y -= elm->y_pos;
			if (!split_r.IsEmpty())
			{
				OpPoint split_p(p.x, p.y);
				if (source.y < elm->y_pos)
				{
					split_p.y += elm->y_pos - source.y;
				}
#ifdef SUPPORT_INDEXED_OPBITMAP
				if (buf->format == MDE_FORMAT_INDEX8)
				{
					buf->method = MDE_METHOD_MASK;
					buf->mask = bitmap->GetTransparentColorIndex();
				}
				else
#endif // SUPPORT_INDEXED_OPBITMAP
					buf->method = MDE_METHOD_ALPHA;

				DrawBitmapClippedTransparentInternal(buf, split_r, split_p);
			}
		}
		return;
	}
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	buf = ((MDE_OpBitmap*)bitmap)->GetMDEBuffer();
	if (!buf)
	{
		//temp g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return;
	}
	
	OP_ASSERT(bitmap->IsTransparent());
#ifdef SUPPORT_INDEXED_OPBITMAP
	if (buf->format == MDE_FORMAT_INDEX8)
	{
		buf->method = MDE_METHOD_MASK;
		buf->mask = bitmap->GetTransparentColorIndex();
	}
	else
#endif
	buf->method = MDE_METHOD_ALPHA;

	DrawBitmapClippedTransparentInternal(buf, source, p);
}

void MDE_OpPainter::DrawBitmapClippedAlphaInternal(MDE_BUFFER* buf, const OpRect& source, OpPoint p)
{
	buf->method = MDE_METHOD_ALPHA;
	MDE_DrawBuffer(buf, MDE_MakeRect(p.x, p.y, source.width, source.height), source.x, source.y, screen);
}

BOOL MDE_OpPainter::DrawBitmapClippedOpacity(const OpBitmap* bitmap, const OpRect &source, OpPoint p, int opacity)
{
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (!((MDE_OpBitmap*)bitmap)->must_support_painter)
		return FALSE;
#endif
	MDE_BUFFER *buf = ((MDE_OpBitmap*)bitmap)->GetMDEBuffer();

	if (MDE_GetBlitMethodSupported(MDE_METHOD_OPACITY, buf->format, screen->format))
	{
		PRE_MUL(opacity);
		MDE_COL_A(buf->col) = opacity;
		buf->method = MDE_METHOD_OPACITY;

		MDE_DrawBuffer(buf, MDE_MakeRect(p.x, p.y, source.width, source.height), source.x, source.y, screen);
		return TRUE;
	}
	return FALSE;
}

void MDE_OpPainter::DrawBitmapClippedAlpha(const OpBitmap* bitmap, const OpRect& source, OpPoint p)
{
	if (m_pre_alpha != 255 && DrawBitmapClippedOpacity(bitmap, source, p, 255))
		return;

#ifdef MDE_LIMIT_IMAGE_SIZE
	UINT32 rw = ((MDE_OpBitmap*)bitmap)->RealWidth();
	UINT32 rh = ((MDE_OpBitmap*)bitmap)->RealHeight();
	if( rw != bitmap->Width() || rh != bitmap->Height())
	{
		DrawBitmapScaledAlpha(bitmap, source, OpRect(p.x, p.y, source.width, source.height));
		return;
	}
#endif // MDE_LIMIT_IMAGE_SIZE
	OP_ASSERT(screen);
	MDE_BUFFER *buf;
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (!((MDE_OpBitmap*)bitmap)->must_support_painter)
	{
		for (SplitBitmapElm* elm = ((MDE_OpBitmap*)bitmap)->GetFirstSplitBitmapElm();
			 elm != NULL;
			 elm = ((MDE_OpBitmap*)bitmap)->GetNextSplitBitmapElm())
		{
			buf = elm->buffer;
			OpRect split_r(0, elm->y_pos, buf->w, buf->h);
			split_r.IntersectWith(source);
			split_r.y -= elm->y_pos;
			if (!split_r.IsEmpty())
			{
				OpPoint split_p(p.x, p.y);
				if (source.y < elm->y_pos)
				{
					split_p.y += elm->y_pos - source.y;
				}
				DrawBitmapClippedAlphaInternal(buf, split_r, split_p);
			}
		}
		return;
	}
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	buf = ((MDE_OpBitmap*)bitmap)->GetMDEBuffer();
	OP_ASSERT(buf);
	
	OP_ASSERT(bitmap->HasAlpha());
	
	DrawBitmapClippedAlphaInternal(buf, source, p);
}

void MDE_OpPainter::DrawBitmapScaledInternal(MDE_BUFFER* buf, const OpRect &source, const OpRect& dest)
{
	MDE_RECT dst = MDE_MakeRect(dest.x, dest.y, dest.width, dest.height);
	
	if (!MDE_RectIntersects(dst, screen->clip))
		return;

	MDE_DrawBufferStretch(buf, dst, MDE_MakeRect(source.x, source.y, source.width, source.height), screen);

}

void MDE_OpPainter::DrawBitmapScaled(const OpBitmap* bitmap, const OpRect& _source, const OpRect& _dest)
{
	OP_ASSERT(screen);

	OpRect dest(_dest);
	OpRect source(_source);

#ifdef MDE_LIMIT_IMAGE_SIZE
	UINT32 rw = ((MDE_OpBitmap*)bitmap)->RealWidth();
	UINT32 rh = ((MDE_OpBitmap*)bitmap)->RealHeight();
	if( rw != bitmap->Width() || rh != bitmap->Height())
	{
		//image has been scaled down, stretch it to original dimensions
		int w = bitmap->Width();
		int h = bitmap->Height();
		source.x = (source.x*rw+w-1)/w;
		source.y = (source.y*rh+h-1)/h;
		source.width = (source.width*rw)/w;
		source.height = (source.height*rh)/h;
	}
#endif // MDE_LIMIT_IMAGE_SIZE

	MDE_BUFFER *buf;
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (!((MDE_OpBitmap*)bitmap)->must_support_painter)
	{
		if (source.height == 0 || source.width == 0)
			return;
		MDE_F1616 y_dest_factor = (dest.height << 16) / source.height;
		int dest_bottom;

		for (SplitBitmapElm* elm = ((MDE_OpBitmap*)bitmap)->GetFirstSplitBitmapElm();
			 elm != NULL;
			 elm = ((MDE_OpBitmap*)bitmap)->GetNextSplitBitmapElm())
		{
			buf = elm->buffer;
			OpRect split_r(0, elm->y_pos, buf->w, buf->h);
			split_r.IntersectWith(source);
			split_r.y -= elm->y_pos;
			int y_source_diff = MAX(0, elm->y_pos - source.y);
			dest_bottom = (((y_source_diff + split_r.height) * y_dest_factor + 32768) >> 16) + dest.y;
			if (!split_r.IsEmpty())
			{
				int dest_y = dest.y;
				if (source.y < elm->y_pos)
				{
					dest_y += ((elm->y_pos - source.y) * y_dest_factor + 32768) >> 16;
				}
				int dest_height = dest_bottom - dest_y;
				OpRect split_dest(dest.x, dest_y, dest.width, dest_height);
				buf->method = MDE_METHOD_COPY;
#ifdef SUPPORT_INDEXED_OPBITMAP
				if (buf->format == MDE_FORMAT_INDEX8)
				{
					buf->mask = bitmap->GetTransparentColorIndex();
				}
#endif
				DrawBitmapScaledInternal(buf, split_r, split_dest);
			}
		}
		return;
	}
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	buf = ((MDE_OpBitmap*)bitmap)->GetMDEBuffer();
	OP_ASSERT(buf);
	
	buf->method = MDE_METHOD_COPY;
	
// Needed to copy INDEX 8 to a format with alpha
#ifdef SUPPORT_INDEXED_OPBITMAP
	if (buf->format == MDE_FORMAT_INDEX8)
	{
		buf->mask = bitmap->GetTransparentColorIndex();
	}
#endif
	DrawBitmapScaledInternal(buf, source, dest);
}

void MDE_OpPainter::DrawBitmapScaledTransparentInternal(MDE_BUFFER* buf, const OpRect& source, const OpRect& dest)
{
	MDE_BUFFER* destbuf = screen;
	MDE_RECT dst = MDE_MakeRect(dest.x, dest.y, dest.width, dest.height);	
	
	if (!MDE_RectIntersects(dst, destbuf->clip))
		return;

	MDE_DrawBufferStretch(buf, dst, MDE_MakeRect(source.x, source.y, source.width, source.height), destbuf);
}

void MDE_OpPainter::DrawBitmapScaledTransparent(const OpBitmap* bitmap, const OpRect& _source, const OpRect& _dest)
{
	OP_ASSERT(screen);
	OpRect dest(_dest);
	OpRect source(_source);
#ifdef MDE_LIMIT_IMAGE_SIZE
	UINT32 rw = ((MDE_OpBitmap*)bitmap)->RealWidth();
	UINT32 rh = ((MDE_OpBitmap*)bitmap)->RealHeight();
	if( rw != bitmap->Width() || rh != bitmap->Height())
	{
		//image has been scaled down, stretch it to original dimensions
		int w = bitmap->Width();
		int h = bitmap->Height();
		source.x = (source.x*rw+w-1)/w;
		source.y = (source.y*rh+h-1)/h;
		source.width = (source.width*rw)/w;
		source.height = (source.height*rh)/h;
	}
#endif // MDE_LIMIT_IMAGE_SIZE

	MDE_BUFFER *buf;
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (!((MDE_OpBitmap*)bitmap)->must_support_painter)
	{
		if (source.height == 0 || source.width == 0)
			return;
		MDE_F1616 y_dest_factor = (dest.height << 16) / source.height;
		int dest_bottom;

		for (SplitBitmapElm* elm = ((MDE_OpBitmap*)bitmap)->GetFirstSplitBitmapElm();
			 elm != NULL;
			 elm = ((MDE_OpBitmap*)bitmap)->GetNextSplitBitmapElm())
		{
			buf = elm->buffer;
			OpRect split_r(0, elm->y_pos, buf->w, buf->h);
			split_r.IntersectWith(source);
			split_r.y -= elm->y_pos;
			int y_source_diff = MAX(0, elm->y_pos - source.y);
			dest_bottom = (((y_source_diff + split_r.height) * y_dest_factor + 32768) >> 16) + dest.y;
			if (!split_r.IsEmpty())
			{
				int dest_y = dest.y;
				if (source.y < elm->y_pos)
				{
					dest_y += ((elm->y_pos - source.y) * y_dest_factor + 32768) >> 16;
				}
				int dest_height = dest_bottom - dest_y;
				OpRect split_dest(dest.x, dest_y, dest.width, dest_height);
#ifdef SUPPORT_INDEXED_OPBITMAP
				if (buf->format == MDE_FORMAT_INDEX8)
				{
					buf->method = MDE_METHOD_MASK;
					buf->mask = bitmap->GetTransparentColorIndex();
				}
				else
#endif
					buf->method = MDE_METHOD_ALPHA;

				DrawBitmapScaledTransparentInternal(buf, split_r, split_dest);
			}
		}
		return;
	}
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	buf = ((MDE_OpBitmap*)bitmap)->GetMDEBuffer();
	OP_ASSERT(buf);	
	OP_ASSERT(bitmap->IsTransparent());

	if (buf == NULL)
		return;
	
#ifdef SUPPORT_INDEXED_OPBITMAP
	if (buf->format == MDE_FORMAT_INDEX8)
	{
		buf->method = MDE_METHOD_MASK;
		buf->mask = bitmap->GetTransparentColorIndex();
	}
	else
#endif
		buf->method = MDE_METHOD_ALPHA;
	DrawBitmapScaledTransparentInternal(buf, source, dest);
}


void MDE_OpPainter::DrawBitmapScaledAlphaInternal(MDE_BUFFER* buf, const OpRect& source, const OpRect& dest)
{
	buf->method = MDE_METHOD_ALPHA;
	MDE_BUFFER* destbuf = screen;
	MDE_RECT dst = MDE_MakeRect(dest.x, dest.y, dest.width, dest.height);	
	
	if (!MDE_RectIntersects(dst, destbuf->clip))
		return;

	MDE_DrawBufferStretch(buf, dst, MDE_MakeRect(source.x, source.y, source.width, source.height), destbuf);
}

void MDE_OpPainter::DrawBitmapScaledAlpha(const OpBitmap* bitmap, const OpRect& _source, const OpRect& _dest)
{
	OP_ASSERT(screen);
	OpRect dest(_dest);
	OpRect source(_source);
#ifdef MDE_LIMIT_IMAGE_SIZE
	UINT32 rw = ((MDE_OpBitmap*)bitmap)->RealWidth();
	UINT32 rh = ((MDE_OpBitmap*)bitmap)->RealHeight();
	if( rw != bitmap->Width() || rh != bitmap->Height())
	{
		//image has been scaled down, stretch it to original dimensions
		int w = bitmap->Width();
		int h = bitmap->Height();
		source.x = (source.x*rw+w-1)/w;
		source.y = (source.y*rh+h-1)/h;
		source.width = (source.width*rw)/w;
		source.height = (source.height*rh)/h;
	}
#endif // MDE_LIMIT_IMAGE_SIZE

	MDE_BUFFER *buf;

#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	if (!((MDE_OpBitmap*)bitmap)->must_support_painter)
	{
		if (source.height == 0 || source.width == 0)
			return;
		MDE_F1616 y_dest_factor = (dest.height << 16) / source.height;
		int dest_bottom;

		for (SplitBitmapElm* elm = ((MDE_OpBitmap*)bitmap)->GetFirstSplitBitmapElm();
			 elm != NULL;
			 elm = ((MDE_OpBitmap*)bitmap)->GetNextSplitBitmapElm())
		{
			buf = elm->buffer;
			OpRect split_r(0, elm->y_pos, buf->w, buf->h);
			split_r.IntersectWith(source);
			split_r.y -= elm->y_pos;
			int y_source_diff = MAX(0, elm->y_pos - source.y);
			dest_bottom = (((y_source_diff + split_r.height) * y_dest_factor + 32768) >> 16) + dest.y;
			if (!split_r.IsEmpty())
			{
				int dest_y = dest.y;
				if (source.y < elm->y_pos)
				{
					dest_y += ((elm->y_pos - source.y) * y_dest_factor + 32768) >> 16;
				}
				int dest_height = dest_bottom - dest_y;
				OpRect split_dest(dest.x, dest_y, dest.width, dest_height);
				DrawBitmapScaledAlphaInternal(buf, split_r, split_dest);
			}
		}
		return;
	}
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	buf = ((MDE_OpBitmap*)bitmap)->GetMDEBuffer();
	OP_ASSERT(buf);
	
	OP_ASSERT(bitmap->HasAlpha());
	DrawBitmapScaledAlphaInternal(buf, source, dest);	
}

OpBitmap* MDE_OpPainter::CreateBitmapFromBackground(const OpRect& rect)
{
	OpBitmap* bitmap;
	if (OpStatus::IsSuccess(OpBitmap::Create(&bitmap, rect.width, rect.height, FALSE, TRUE, 0, 0, TRUE)))
	{
		MDE_METHOD old_method = screen->method;
		screen->method = MDE_METHOD_COPY;
		MDE_BUFFER* buf = ((MDE_OpBitmap*)bitmap)->GetMDEBuffer();
		MDE_DrawBuffer(screen, MDE_MakeRect(0, 0, rect.width, rect.height), rect.x, rect.y, buf);
		screen->method = old_method;
		buf->method = old_method;
		return bitmap;
	}
	return NULL;
}

BOOL MDE_OpPainter::Supports(SUPPORTS supports)
{
	switch (supports)
	{
	case SUPPORTS_TRANSPARENT:
		return TRUE;
	case SUPPORTS_ALPHA:
		return TRUE;
	case SUPPORTS_ALPHA_COLOR:
		return TRUE;

	default:
		return FALSE;
	}
}

void MDE_OpPainter::beginPaint(MDE_BUFFER *s)
{
	OP_ASSERT(!screen);
	paint_count++;
	screen = s;
}

void MDE_OpPainter::endPaint()
{
  if( paint_count--==1 )
  {
	  screen = NULL;
	  OP_ASSERT(cliprect_stack == NULL); // Unbalanced SetClipRect/RemoveClipRect !
  }
}

#endif // !MDE_PLATFORM_IMPLEMENTS_OPPAINTER && !VEGA_OPPAINTER_SUPPORT
