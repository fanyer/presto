/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/svg_number.h"

#include "modules/svg/src/svgpainter.h"

#include "modules/svg/src/OpBpath.h"
#include "modules/svg/src/SVGMatrix.h"
#include "modules/svg/src/SVGPoint.h"
#include "modules/svg/src/SVGVector.h"

#include "modules/svg/src/SVGCanvasState.h"
#include "modules/svg/src/svgpaintserver.h"

#if defined(SVG_SUPPORT_FILTERS) || defined(SVG_SUPPORT_OPACITY)
# include "modules/libvega/vegafilter.h"
#endif // SVG_SUPPORT_FILTERS || SVG_SUPPORT_OPACITY
#ifdef SVG_SUPPORT_STENCIL
# include "modules/libvega/vegastencil.h"
#endif // SVG_SUPPORT_STENCIL

#ifdef VEGA_OPPAINTER_SUPPORT
# include "modules/libvega/src/oppainter/vegaopfont.h"
#endif // VEGA_OPPAINTER_SUPPORT

#include "modules/pi/ui/OpUiInfo.h"
#include "modules/pi/OpPainter.h"
#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpFont.h"

SVGPainter::SVGPainter() :
	m_clip_mode(STENCIL_USE),
	m_mask_mode(STENCIL_USE)
#ifdef PI_VIDEO_LAYER
	, m_allow_overlay(TRUE)
#endif // PI_VIDEO_LAYER
{
	m_rendertarget = NULL;
	m_renderer = NULL;

	m_transform_stack = NULL;

#ifndef VEGA_OPPAINTER_SUPPORT
	m_painter = NULL;
	m_painter_bitmap = NULL;
	m_painter_bitmap_is_dirty = FALSE;
#ifdef SVG_BROKEN_PAINTER_ALPHA
	m_last_painter_fillcolor = 0;
#endif // SVG_BROKEN_PAINTER_ALPHA
	m_current_painter_rect.Empty();
	m_painter_rect.Empty();
#endif // !VEGA_OPPAINTER_SUPPORT

#ifdef SVG_SUPPORT_PAINTSERVERS
	m_fills[FILL_PAINT] = NULL;
	m_fills[STROKE_PAINT] = NULL;
#endif // SVG_SUPPORT_PAINTSERVERS

	m_current_quality = ~0u;
	m_imageopacity = 255;

#ifdef SVG_SUPPORT_FILTERS
	m_bg_layers_enabled = TRUE;
#endif // SVG_SUPPORT_FILTERS

#ifdef SVG_SUPPORT_STENCIL
	m_cached_stencil = NULL;
#endif // SVG_SUPPORT_STENCIL
}

SVGPainter::~SVGPainter()
{
#ifdef SVG_SUPPORT_PAINTSERVERS
	// Fills are owned by the paintservers, and should have been
	// released via EndDrawing and PutFill on the paintserver.
	OP_ASSERT(m_fills[FILL_PAINT] == NULL);
	OP_ASSERT(m_fills[STROKE_PAINT] == NULL);
#endif // SVG_SUPPORT_PAINTSERVERS

#ifdef SVG_SUPPORT_STENCIL
	OP_ASSERT(m_cached_stencil == NULL);
#endif // SVG_SUPPORT_STENCIL

#ifndef VEGA_OPPAINTER_SUPPORT
	OP_ASSERT(m_painter == NULL);
	if (m_painter)
		ReleasePainter();
#endif // !VEGA_OPPAINTER_SUPPORT

	CleanTransformStack();

	if (m_renderer)
		m_renderer->setRenderTarget(NULL);

	OP_ASSERT(m_layers.Empty());

#ifdef VALGRIND
	// Make potential leaks show in Valgrind
	while (Layer* layer = m_layers.Last())
		layer->Out();
#endif // VALGRIND

#ifdef SVG_SUPPORT_STENCIL
	OP_ASSERT(m_masks.Empty());
 	OP_ASSERT(m_active_clips.Empty());
	OP_ASSERT(m_partial_clips.Empty());
#endif // SVG_SUPPORT_STENCIL
#ifdef SVG_SUPPORT_FILTERS
	OP_ASSERT(m_bg_layers.Empty());
#endif // SVG_SUPPORT_FILTERS
}

OP_STATUS SVGPainter::SetClipRect(const OpRect& cliprect)
{
	m_clip_extents = cliprect;
	m_renderer->setClipRect(cliprect.x, cliprect.y, cliprect.width, cliprect.height);
	return OpStatus::OK;
}

void SVGPainter::GetCurrentOffset(OpPoint& curr_ofs)
{
	if (m_clip_mode == STENCIL_CONSTRUCT || m_mask_mode == STENCIL_CONSTRUCT)
		curr_ofs = m_area.TopLeft();
	else
		curr_ofs = m_curr_layer_pos;
}

void SVGPainter::Reset()
{
	m_layers.Clear();

#ifdef SVG_SUPPORT_FILTERS
	m_bg_layers.Clear();
#endif // SVG_SUPPORT_FILTERS

#ifdef SVG_SUPPORT_STENCIL
	m_masks.Clear();

 	m_active_clips.Clear();
	m_partial_clips.Clear();
#endif // SVG_SUPPORT_STENCIL
}

OP_STATUS SVGPainter::SetAntialiasingQuality(unsigned int quality)
{
	if (!m_rendertarget || !m_renderer)
		return OpStatus::OK;

	return m_renderer->Init(m_area.width, m_area.height, quality);
}

OP_STATUS SVGPainter::BeginPaint(VEGARenderer* renderer, VEGARenderTarget* rendertarget, const OpRect& area)
{
	m_renderer = renderer;
	m_rendertarget = rendertarget;
	m_area = area;
	m_curr_layer_pos = area.TopLeft();
	m_transform.LoadIdentity();
	m_current_quality = ~0u; // Set 'unknown quality'
	return OpStatus::OK;
}

void SVGPainter::EndPaint()
{
#ifdef SVG_SUPPORT_STENCIL
	ClearStencilCache();
#endif // SVG_SUPPORT_STENCIL

	m_renderer->setRenderTarget(NULL);
	m_renderer = NULL;
	m_rendertarget = NULL;
	m_area.Empty();
}

#ifdef PI_VIDEO_LAYER
void SVGPainter::ClearRect(const OpRect& pixel_rect, UINT32 color)
{
	OpRect clipped_rect(pixel_rect);
	clipped_rect.IntersectWith(m_clip_extents);

	ApplyClipping(clipped_rect);

	if (clipped_rect.IsEmpty())
		return;

	Clear(color, &clipped_rect);
}

void SVGPainter::ApplyClipping(OpRect& rect)
{
#ifdef SVG_SUPPORT_STENCIL
	OpRect clip;
	if (GetClipRect(clip))
		rect.IntersectWith(clip);
#endif // SVG_SUPPORT_STENCIL
}

BOOL SVGPainter::AllowOverlay() const
{
	if (!m_allow_overlay)
		return FALSE;

	// Check if we are rotated/skewed
	if (!m_transform.IsScaledTranslation())
		return FALSE;

#ifdef CSS_TRANSFORMS
	// Check if we are in a transformed context
	if (m_doc_pos.IsTransform())
		return FALSE;
#endif // CSS_TRANSFORMS

#ifdef SVG_SUPPORT_STENCIL
	// Check if we are constructing a stencil
	if (m_clip_mode == STENCIL_CONSTRUCT || m_mask_mode == STENCIL_CONSTRUCT)
		return FALSE;

	// Check if we are using a stencil
	if (m_masks.Last() || GetActualClipStencil())
		return FALSE;
#endif // SVG_SUPPORT_STENCIL

	// Check if an opacity layer is active
	if (IsLayer())
		return FALSE;

#ifdef SVG_SUPPORT_FILTERS
	// Check if a background layer is active
	if (HasBackgroundLayer())
		return FALSE;
#endif // SVG_SUPPORT_FILTERS

	return TRUE;
}
#endif // PI_VIDEO_LAYER

OP_STATUS SVGPainter::PushTransformBlock()
{
	TransformBlock* stack_block = OP_NEW(TransformBlock, (m_transform_stack));
	if (!stack_block)
		return OpStatus::ERR_NO_MEMORY;

	m_transform_stack = stack_block;
	return OpStatus::OK;
}

OP_STATUS SVGPainter::PushTransform(const SVGMatrix& ctm)
{
	if (!m_transform_stack || m_transform_stack->count == TransformBlock::MAX_XFRM_PER_BLOCK)
		RETURN_IF_ERROR(PushTransformBlock());

	m_transform_stack->transform[m_transform_stack->count++] = m_transform;
	m_transform.PostMultiply(ctm);
	return OpStatus::OK;
}

void SVGPainter::PopTransformBlock()
{
	OP_ASSERT(m_transform_stack);

	TransformBlock* next = m_transform_stack->next;
	OP_DELETE(m_transform_stack);
	m_transform_stack = next;
}

void SVGPainter::PopTransform()
{
	if (!m_transform_stack)
	{
		m_transform.LoadIdentity();
		return;
	}

	m_transform = m_transform_stack->transform[--m_transform_stack->count];

	if (m_transform_stack->count == 0)
		PopTransformBlock();
}

void SVGPainter::CleanTransformStack()
{
	while (m_transform_stack)
		PopTransformBlock();
}

#ifdef VEGA_OPPAINTER_SUPPORT
// The code that just renders directly to the current render target...
void SVGPainter::DrawStringPainter(OpFont* font, const OpPoint& pos, const uni_char* text, unsigned len,
								   int extra_char_spacing, int* caret_x)
{
	if (!m_object_props->filled)
		return; // Nothing to draw

	OP_ASSERT(m_paints[FILL_PAINT].pserver == NULL);

	UINT32 fillcolor = m_paints[FILL_PAINT].color;

	UINT8 col_r = GetSVGColorRValue(fillcolor);
	UINT8 col_g = GetSVGColorGValue(fillcolor);
	UINT8 col_b = GetSVGColorBValue(fillcolor);
	UINT8 col_a = GetSVGColorAValue(fillcolor);

	if (m_paints[FILL_PAINT].opacity != 255)
		col_a = ((col_a + 1) * m_paints[FILL_PAINT].opacity) >> 8;

	fillcolor = (col_a << 24) | (col_r << 16) | (col_g << 8) | col_b;

	m_renderer->setColor(fillcolor);

	OpPoint ofs;
	GetCurrentOffset(ofs);

	VEGARenderTarget* target = GetLayer();
	OpRect target_area = GetLayerArea();

	VEGAStencil* stencil = NULL;
	OP_STATUS result = SelectStencil(target_area, stencil);

	if (OpStatus::IsSuccess(result))
	{
		VEGARenderTarget* current_rt = target;
		OpRect current_area = target_area;

		// Currently all text in these case will be rendered using outlines
		OP_ASSERT(m_clip_mode != STENCIL_CONSTRUCT && m_mask_mode != STENCIL_CONSTRUCT);

		OpPoint layer_pos(pos);
		layer_pos -= ofs;

		VEGAOpFont* vegaopfont = static_cast<VEGAOpFont*>(font);
		VEGAFont* vegafont = vegaopfont->getVegaFont();

		if (SetRenderTarget(current_rt, current_area))
			m_renderer->drawString(vegafont, layer_pos.x, layer_pos.y, text, len,
								   extra_char_spacing, -1, stencil);

#ifdef SVG_SUPPORT_FILTERS
		if (HasBackgroundLayer())
		{
			BILayer* bgi = m_bg_layers.Last();

			OpPoint bgimg_ofs(pos);
			bgimg_ofs -= bgi->area.TopLeft();

			m_renderer->setRenderTarget(bgi->target);

			// Set the cliprect to the entire surface
			m_renderer->setClipRect(0, 0, bgi->area.width, bgi->area.height);

			m_renderer->drawString(vegafont, bgimg_ofs.x, bgimg_ofs.y, text, len,
								   extra_char_spacing, -1, stencil);
		}
#endif // SVG_SUPPORT_FILTERS
	}

	if (caret_x)
	{
		m_renderer->setColor(0xff000000 /* black */);
		m_renderer->fillRect(pos.x - ofs.x + (*caret_x), pos.y - ofs.y, 1, font->Height(), stencil);
	}
}

void SVGPainter::DrawSelectedStringPainter(const OpRect& selected,
										   COLORREF sel_bg_color, COLORREF sel_fg_color,
										   OpFont* font,
										   const OpPoint& pos, const uni_char* text,
										   unsigned len, int extra_char_spacing)
{
	// This color is set directly on the VEGARenderer, and should thus be BGRA.
	UINT32 bcol_sel = HTM_Lex::GetColValByIndex(sel_bg_color);
	bcol_sel = 0xFF000000 | (GetRValue(bcol_sel) << 16) | (GetGValue(bcol_sel) << 8) | GetBValue(bcol_sel);

	// This color is set using the external API, so should be RGBA.
	UINT32 fcol_sel = HTM_Lex::GetColValByIndex(sel_fg_color);
	fcol_sel = 0xFF000000 | (GetBValue(fcol_sel) << 16) | (GetGValue(fcol_sel) << 8) | GetRValue(fcol_sel);

	OpPoint ofs;
	GetCurrentOffset(ofs);

	OpRect adjusted_selrect(selected);
	OpPoint adjusted_pos(pos);
	adjusted_pos -= ofs;

	adjusted_selrect.x -= ofs.x;
	adjusted_selrect.y -= ofs.y;

	OpRect old_cliprect;
	m_renderer->getClipRect(old_cliprect.x,old_cliprect.y,old_cliprect.width,old_cliprect.height);
	SetClipRect(adjusted_selrect);

	m_renderer->setColor(bcol_sel);
	m_renderer->fillRect(adjusted_selrect.x, adjusted_selrect.y, adjusted_selrect.width, adjusted_selrect.height);

	UINT32 oldcolor = m_paints[FILL_PAINT].color;
	UINT32 oldopacity = m_paints[FILL_PAINT].opacity;
	m_paints[FILL_PAINT].color = fcol_sel;
	m_paints[FILL_PAINT].opacity = 255;

	DrawStringPainter(font, pos, text, len, extra_char_spacing, NULL);

	m_paints[FILL_PAINT].color = oldcolor;
	m_paints[FILL_PAINT].opacity = oldopacity;
	SetClipRect(old_cliprect);
}
#else
OP_STATUS SVGPainter::GetPainter(unsigned int font_overhang)
{
	OpRect painter_rect_w_overhang(m_painter_rect);
	painter_rect_w_overhang.width += font_overhang;

	return GetPainterInternal(painter_rect_w_overhang, m_painter);
}

OP_STATUS SVGPainter::LowGetPainter(const OpRect& rect, OpPainter*& painter)
{
	// If we have an old bitmap that means GetPainter was called
	// in an unbalanced way. ReleasePainter must be called.
	OP_ASSERT(m_painter_bitmap == NULL);

	if (m_painter_bitmap)
		return OpStatus::OK;

	OP_ASSERT(!rect.IsEmpty()); // the rect must be valid if we do not want to copy
	RETURN_IF_ERROR(OpBitmap::Create(&m_painter_bitmap, rect.width, rect.height,
									 FALSE, TRUE, 0, 0, TRUE));
	if (!rect.IsEmpty()) // Don't know if it can be empty but play it safe
	{
		// Clear the bitmap as fast as possible. Trying different ways.
		UINT8 rgba_empty[] = { 0, 0, 0, 0 };
		if (!m_painter_bitmap->SetColor(rgba_empty, TRUE, NULL))
		{
			// Line by line clearing. Slow....
			UINT32 *lineData = OP_NEWA(UINT32, rect.width);
			if (lineData)
			{
				op_memset(lineData, 0, rect.width*sizeof(UINT32));
				for (int y = 0; y < rect.height; ++y)
				{
					m_painter_bitmap->AddLine(lineData, y);
				}
				OP_DELETEA(lineData);
			}
		}
	}

	OP_STATUS status = OpStatus::OK;
	if (!m_painter_bitmap->Supports(OpBitmap::SUPPORTS_PAINTER)
#ifdef SVG_BROKEN_PAINTER_ALPHA
		&& false // Hack to get some fonts, we wont use these painters for anything but drawing fonts
#endif // SVG_BROKEN_PAINTER_ALPHA
		)
		status = OpStatus::ERR;

	if (OpStatus::IsSuccess(status))
		painter = m_painter_bitmap->GetPainter();

	if (!painter)
		status = OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError(status))
	{
		OP_DELETE(m_painter_bitmap);
		m_painter_bitmap = NULL;
	}

	return status;
}

void SVGPainter::ReleasePainter(const OpRect& damagedRect, OpBitmap** resultbitmap)
{
	if (!m_painter_bitmap)
		return;

	m_painter_bitmap->ReleasePainter();

	OP_ASSERT(resultbitmap);

	*resultbitmap = m_painter_bitmap;
	m_painter_bitmap = NULL;
}

OP_STATUS SVGPainter::GetPainterInternal(const OpRect& rect, OpPainter*& painter)
{
	m_current_painter_rect = rect;

	if (GetLayer())
		m_current_painter_rect.IntersectWith(GetLayerArea());

	// Guard against trying to allocate very large bitmaps
	if (m_current_painter_rect.IsEmpty())
		return OpStatus::ERR;

	return LowGetPainter(m_current_painter_rect, painter);
}

OP_STATUS SVGPainter::GetPainter(const OpRect& rect, OpPainter*& painter)
{
	SetPainterRect(rect);
	RETURN_IF_ERROR(GetPainter(0));
	painter = m_painter;
	return OpStatus::OK;
}

#ifdef SVG_BROKEN_PAINTER_ALPHA
static inline void ColorizeScanline(UINT32* data, UINT32 fillcolor, unsigned width)
{
	UINT8 col_r = GetSVGColorRValue(fillcolor);
	UINT8 col_g = GetSVGColorGValue(fillcolor);
	UINT8 col_b = GetSVGColorBValue(fillcolor);
	UINT8 col_a = GetSVGColorAValue(fillcolor);

	fillcolor = (col_r << 16) | (col_g << 8) | col_b;

	while (width--)
	{
		if (*data == 0)
		{
			data++;
			continue;
		}

		unsigned int alpha = *data & 0xff;
		alpha *= col_a + 1;
		alpha >>= 8;
#ifdef USE_PREMULTIPLIED_ALPHA
		*data++ = (alpha << 24) |
			(((col_r * (alpha + 1)) << 8) & 0xff0000) |
			((col_g * (alpha + 1)) & 0xff00) |
			(((col_b * (alpha + 1)) >> 8) & 0xff);
#else
		*data++ = fillcolor | (alpha << 24);
#endif // USE_PREMULTIPLIED_ALPHA
	}
}

void SVGPainter::FixupPainterAlpha(OpBitmap* painterbm, const OpRect& damagedRect)
{
	UINT32 *pd = NULL;

	if (painterbm->Supports(OpBitmap::SUPPORTS_POINTER) && painterbm->GetBpp() == 32)
	{
		pd = (UINT32*)painterbm->GetPointer();
		OP_ASSERT(pd);
	}

	INT32 in_width = damagedRect.width;
	INT32 in_height = damagedRect.height;

	if (pd)
	{
		for (int y = 0; y < in_height; ++y)
		{
			ColorizeScanline(pd, m_last_painter_fillcolor, in_width);

			pd += painterbm->GetBytesPerLine() / 4;
		}

		painterbm->ReleasePointer();
	}
	else
	{
		pd = OP_NEWA(UINT32, painterbm->Width());
		if (!pd)
		{
			OP_ASSERT(!"FIXME: Handle OOM");
			return;
		}

		for (int line = 0; line < in_height; ++line)
		{
			if (painterbm->GetLineData(pd, line))
			{
				ColorizeScanline(pd, m_last_painter_fillcolor, in_width);

				painterbm->AddLine(pd, line);
			}
		}

		OP_DELETE(pd);
	}
}

void SVGPainter::FillRect(const OpRect& rect, UINT32 color)
{
	VEGARenderTarget* active_rt = GetLayer();
	OpRect active_area = GetLayerArea();

	OpRect dest_area(rect);
#ifdef SVG_SUPPORT_STENCIL
	ApplyClipToRegion(dest_area);
#endif // SVG_SUPPORT_STENCIL
	dest_area.IntersectWith(active_area);

	if (dest_area.IsEmpty())
		return;

	dest_area.OffsetBy(OpPoint(-active_area.x, -active_area.y));

	m_renderer->setRenderTarget(active_rt);
	m_renderer->setColor(color);
	// FIXME: Maybe use the stencil?
	OpStatus::Ignore(m_renderer->fillRect(dest_area.x, dest_area.y,
										  dest_area.width, dest_area.height));
}
#endif // SVG_BROKEN_PAINTER_ALPHA

void SVGPainter::ReleasePainter(OpBitmap** resultbitmap)
{
	OpRect damaged_rect(m_painter_rect);
	damaged_rect.IntersectWith(m_current_painter_rect);

	OpBitmap* painterbm = NULL;
	ReleasePainter(damaged_rect, &painterbm);

#ifdef SVG_BROKEN_PAINTER_ALPHA
	// fix the alpha value of the blitted image
	FixupPainterAlpha(painterbm, damaged_rect);
#endif // SVG_BROKEN_PAINTER_ALPHA

	if (painterbm && m_painter_bitmap_is_dirty)
	{
#ifdef SVG_SUPPORT_STENCIL
		OP_ASSERT(m_clip_mode == STENCIL_USE || !"Shouldn't draw painter text on stencils");
#endif // SVG_SUPPORT_STENCIL

		OpRect damrect(damaged_rect);
		if (m_current_painter_rect.width == (int)painterbm->Width() &&
			m_current_painter_rect.height == (int)painterbm->Height())
		{
			OP_ASSERT(!damrect.IsEmpty());
			// Offset the bitmap
			damrect.OffsetBy(OpPoint(-m_current_painter_rect.x,
									 -m_current_painter_rect.y));
		}

		SVGRect damagedSVGRect(damaged_rect.x, damaged_rect.y, damaged_rect.width, damaged_rect.height);

		m_imageinfo.dest = damagedSVGRect;
		m_imageinfo.source = damrect;

		m_imageinfo.Reset();
		m_imageinfo.quality = SVGPainter::IMAGE_QUALITY_LOW;

		if (OpStatus::IsSuccess(m_imageinfo.Setup(m_renderer, painterbm)))
		{
			m_vtransform.loadIdentity();

			OpStatus::Ignore(ProcessImage(damagedSVGRect));

			m_imageinfo.Reset();
		}
	}

	OP_DELETE(painterbm);

	m_current_painter_rect.Empty();
	m_painter = NULL;

	m_painter_bitmap_is_dirty = FALSE;
}

void SVGPainter::DrawStringPainter(OpFont* font, const OpPoint& pos, const uni_char* text, unsigned len, int extra_char_spacing, int* caret_x)
{
	OP_ASSERT(m_painter);
	OpPoint adjusted_pos(pos);

	if (!m_object_props->filled)
		return; // Nothing to draw

	adjusted_pos.x -= m_current_painter_rect.x;
	adjusted_pos.y -= m_current_painter_rect.y;

	UINT32 fillcolor = m_paints[FILL_PAINT].color;
	if (m_paints[FILL_PAINT].opacity != 255)
	{
		UINT8 a = GetSVGColorAValue(fillcolor);
		a = ((a + 1) * m_paints[FILL_PAINT].opacity) >> 8;
		fillcolor = (a << 24) | (fillcolor & 0x00ffffff);
	}

#ifdef SVG_BROKEN_PAINTER_ALPHA
	if (fillcolor != m_last_painter_fillcolor || m_painter == NULL)
	{
		OP_STATUS status = SyncPainter();
		if (OpStatus::IsError(status))
		{
			// We're in deep shit now...
			return;
		}
	}
	m_last_painter_fillcolor = fillcolor;

	UINT8 r = 0xff;
	UINT8 g = 0xff;
	UINT8 b = 0xff;
	UINT8 a = 0xff;
#else
	UINT8 r = GetSVGColorRValue(fillcolor);
	UINT8 g = GetSVGColorGValue(fillcolor);
	UINT8 b = GetSVGColorBValue(fillcolor);
	UINT8 a = GetSVGColorAValue(fillcolor);
#endif // SVG_BROKEN_PAINTER_ALPHA

	m_painter->SetColor(r, g, b, a);
	m_painter->SetFont(font);

	m_painter->DrawString(adjusted_pos, text, len, extra_char_spacing);

	if (caret_x)
	{
		m_painter->SetColor(0, 0, 0, 255);
		m_painter->DrawRect(OpRect(adjusted_pos.x+(*caret_x),adjusted_pos.y,1,font->Height()));
	}

	m_painter_bitmap_is_dirty = TRUE;
}

void SVGPainter::DrawSelectedStringPainter(const OpRect& selected,
										   COLORREF sel_bg_color, COLORREF sel_fg_color,
										   OpFont* font,
										   const OpPoint& pos, const uni_char* text,
										   unsigned len, int extra_char_spacing)
{
	OP_ASSERT(m_painter);
	UINT32 bcol_sel = HTM_Lex::GetColValByIndex(sel_bg_color);
	UINT32 fcol_sel = HTM_Lex::GetColValByIndex(sel_fg_color);

	OpRect adjusted_selrect(selected);
	OpPoint adjusted_pos(pos);

	adjusted_pos.x -= m_current_painter_rect.x;
	adjusted_pos.y -= m_current_painter_rect.y;

	adjusted_selrect.x -= m_current_painter_rect.x;
	adjusted_selrect.y -= m_current_painter_rect.y;

#ifdef SVG_BROKEN_PAINTER_ALPHA
	UINT32 svg_fcol_sel = (0xff << 24) | (GetRValue(fcol_sel) << 16) |
		(GetGValue(fcol_sel) << 8) | GetBValue(fcol_sel);
	if (svg_fcol_sel != m_last_painter_fillcolor || m_painter == NULL)
	{
		OP_STATUS status = SyncPainter();
		if (OpStatus::IsError(status))
		{
			// We're in deep shit now...
			return;
		}
	}
	m_last_painter_fillcolor = svg_fcol_sel;
#endif // SVG_BROKEN_PAINTER_ALPHA
	m_painter->SetClipRect(adjusted_selrect);
#ifdef SVG_BROKEN_PAINTER_ALPHA
	FillRect(selected, bcol_sel);
#else
	m_painter->SetColor(GetSVGColorRValue(bcol_sel), GetSVGColorGValue(bcol_sel), GetSVGColorBValue(bcol_sel));
	m_painter->FillRect(adjusted_selrect);
#endif // SVG_BROKEN_PAINTER_ALPHA
	m_painter->SetColor(GetSVGColorRValue(fcol_sel), GetSVGColorGValue(fcol_sel), GetSVGColorBValue(fcol_sel));
	m_painter->SetFont(font);
	m_painter->DrawString(adjusted_pos, text, len, extra_char_spacing);
	m_painter->RemoveClipRect();

	m_painter_bitmap_is_dirty = TRUE;
}

OP_STATUS SVGPainter::SyncPainter()
{
	if (m_painter == NULL || !m_painter_bitmap_is_dirty)
		return OpStatus::OK;

	OpRect saved_painter_rect(m_current_painter_rect);
	ReleasePainter();

	OP_STATUS err = GetPainterInternal(saved_painter_rect, m_painter);

	OP_ASSERT(OpStatus::IsError(err) || m_painter);
	OP_ASSERT(!m_current_painter_rect.IsEmpty());

	return err;
}
#endif // VEGA_OPPAINTER_SUPPORT

#ifdef SVG_SUPPORT_STENCIL
VEGAStencil* SVGPainter::GetStencil()
{
	VEGAStencil* newstencil = NULL;

	if (m_cached_stencil)
	{
		m_renderer->setRenderTarget(m_cached_stencil);
		m_renderer->setClipRect(0, 0, m_cached_stencil->getWidth(), m_cached_stencil->getHeight());
		m_renderer->clear(0, 0, m_cached_stencil->getWidth(), m_cached_stencil->getHeight(), 0);

		newstencil = m_cached_stencil;
		m_cached_stencil = NULL;
	}
	else
	{
		RETURN_VALUE_IF_ERROR(m_renderer->createStencil(&newstencil, false,
														m_area.width, m_area.height), NULL);
	}
	return newstencil;
}

BOOL SVGPainter::PutStencil(VEGAStencil* stencil)
{
	OP_ASSERT(stencil);
	OP_ASSERT(!stencil->isComponentBased());

	if (m_cached_stencil)
		return FALSE;

	m_cached_stencil = stencil;
	return TRUE;
}

void SVGPainter::ClearStencilCache()
{
	VEGARenderTarget::Destroy(m_cached_stencil);
	m_cached_stencil = NULL;
}

void SVGPainter::ReleaseStencil(Stencil* stencil)
{
	if (stencil->stencil && PutStencil(stencil->stencil))
		stencil->stencil = NULL;

	OP_DELETE(stencil);
}

SVGPainter::Stencil* SVGPainter::PopStencil(AutoDeleteList<Stencil>& stencil_stack)
{
	if (Stencil* ret_stencil = stencil_stack.Last())
	{
		ret_stencil->Out();
		return ret_stencil;
	}
	return NULL;
}

static inline BOOL IsAlmostAligned(const SVGRect& fr, const OpRect& ir)
{
	// See tweak TWEAK_SVG_CLIPRECT_TOLERANCE
	if ((fr.x - ir.Left()).abs() * SVG_CLIPRECT_TOLERANCE < 1 &&
		(fr.y - ir.Top()).abs() * SVG_CLIPRECT_TOLERANCE < 1 &&
		(fr.x+fr.width - ir.Right()).abs() * SVG_CLIPRECT_TOLERANCE < 1 &&
		(fr.y+fr.height - ir.Bottom()).abs() * SVG_CLIPRECT_TOLERANCE < 1)
		return TRUE;

	return FALSE;
}

OP_STATUS SVGPainter::AddClipRect(const SVGRect& cliprect)
{
	const SVGMatrix& ctm = m_transform;

	// Scaling and translation only
	if (ctm[1].Close(0) && ctm[2].Close(0))
	{
		// Determine if the cliprect is sufficiently aligned to the pixelgrid
		SVGRect xfrm_cr = ctm.ApplyToRect(cliprect);
		OpRect pix_xfrm_cr = xfrm_cr.GetEnclosingRect();
		if (IsAlmostAligned(xfrm_cr, pix_xfrm_cr))
		{
			Stencil* stencil = OP_NEW(Stencil, (NULL, m_clip_mode));
			if (!stencil)
				return OpStatus::ERR_NO_MEMORY;

			// Calculate intersection with parent clip - if any
			if (Stencil* parent_stencil = m_active_clips.Last())
				pix_xfrm_cr.IntersectWith(parent_stencil->area);

			stencil->area = pix_xfrm_cr;
			stencil->Into(&m_active_clips);
			return OpStatus::OK;
		}
	}

	RETURN_IF_ERROR(BeginClip());

	// Save paint state
	SVGPaintDesc saved_fill_desc = m_paints[FILL_PAINT];
	SVGObjectProperties* old_obj_props = m_object_props;

	// Setup clip/stencil painting state
	SVGPaintDesc clip_desc;
	clip_desc.color = 0xffffffff;
	clip_desc.opacity = 255;
	clip_desc.pserver = NULL;

	m_paints[FILL_PAINT] = clip_desc;

	SVGObjectProperties tmp_obj_props;
	tmp_obj_props.aa_quality = m_current_quality;
	tmp_obj_props.fillrule = SVGFILL_NON_ZERO;
	tmp_obj_props.filled = TRUE;
	tmp_obj_props.stroked = FALSE;

	m_object_props = &tmp_obj_props;

	OP_STATUS status = DrawRect(cliprect.x, cliprect.y, cliprect.width, cliprect.height, 0, 0);

	// Restore paint state
	m_object_props = old_obj_props;
	m_paints[FILL_PAINT] = saved_fill_desc;

	EndClip();

	return status;
}

OP_STATUS SVGPainter::BeginClip()
{
	VEGAStencil* newstencil = GetStencil();
	if (!newstencil)
		return OpStatus::ERR_NO_MEMORY;

	Stencil* stencil = OP_NEW(Stencil, (newstencil, m_clip_mode));
	if (!stencil)
	{
		PutStencil(newstencil);
		return OpStatus::ERR_NO_MEMORY;
	}

	stencil->area.Empty();
	stencil->Into(&m_partial_clips);

	m_clip_mode = STENCIL_CONSTRUCT;
	return OpStatus::OK;
}

void SVGPainter::EndClip()
{
	Stencil* current_stencil = PopStencil(m_partial_clips);
	if (!current_stencil)
		// If the stack was empty, there is really nothing to do -
		// we most likely have a bug somewhere which caused an
		// unbalanced pair of Begin/End
		return;

	// This clip will no longer change, so fetch the dirty
	// rect and set area
	int dr_left, dr_right, dr_top, dr_bottom;
	current_stencil->stencil->getDirtyRect(dr_left, dr_right, dr_top, dr_bottom);
	current_stencil->area.Set(dr_left, dr_top, dr_right - dr_left + 1, dr_bottom - dr_top + 1);

	m_clip_mode = current_stencil->prev_mode;

	OP_ASSERT(!current_stencil->InList());
	current_stencil->Into(&m_active_clips);
}

void SVGPainter::RemoveClip()
{
	Stencil* to_delete = PopStencil(m_active_clips);
	ReleaseStencil(to_delete);
}

OP_STATUS SVGPainter::BeginMask()
{
	// Make sure text drawn with the painter makes it to the correct
	// surface - with VEGAOpPainter this should be a no-op.
	OpStatus::Ignore(SyncPainter());

	VEGAStencil* newstencil = NULL;
	RETURN_IF_ERROR(m_renderer->createStencil(&newstencil, true, m_area.width, m_area.height));

	Stencil* stencil = OP_NEW(Stencil, (newstencil, m_mask_mode));
	if (!stencil)
	{
		VEGARenderTarget::Destroy(newstencil);
		return OpStatus::ERR_NO_MEMORY;
	}

	stencil->area.Empty();

	// Create a mask-layer to be added to the layer-stack. This layer
	// should make sure that primitives get renderered to the correct
	// render target.
	Layer* layer = OP_NEW(Layer, ());
	if (!layer)
	{
		OP_DELETE(stencil);
		return OpStatus::ERR_NO_MEMORY;
	}

	layer->area = m_area;
	layer->target = newstencil;
	layer->owns_target = FALSE;

	// Add to mask-stack and layer-stack.
	stencil->Into(&m_masks);
	layer->Into(&m_layers);

	m_mask_mode = STENCIL_CONSTRUCT;
	return OpStatus::OK;
}

void SVGPainter::EndMask()
{
	Stencil* current_mask = m_masks.Last();
	if (!current_mask)
	{
		OP_ASSERT(!"Unbalanced Begin/EndMask");
		return;
	}

	current_mask->Out();

	m_mask_mode = current_mask->prev_mode;

	OP_ASSERT(IsLayer());

	// Make sure text drawn with the painter makes it to the correct
	// surface - with VEGAOpPainter this should be a no-op.
	OpStatus::Ignore(SyncPainter());

	// Pop off the top layer (this mask)
	Layer* srclayer = m_layers.Last();
	srclayer->Out();

	// This layer should not have a mask attached.
	OP_ASSERT(srclayer->mask == NULL);
	// The layer should be the layer that matches the mask render target.
	OP_ASSERT(srclayer->target == current_mask->stencil);

	OP_DELETE(srclayer);

	// Each "mask layer" should have a layer underneath itself.
	OP_ASSERT(IsLayer());

	// Top-most layer adopts the mask stencil
	Layer* layer = m_layers.Last();
	layer->mask = current_mask->stencil;

	// FIXME/TODO: Take current_mask->area and apply it as a clip
	// restriction - should probably be used to limit the size of the
	// layer, or maybe that will be achieveable indirectly?

	current_mask->stencil = NULL;
	OP_DELETE(current_mask);
}
#endif // SVG_SUPPORT_STENCIL

#ifdef SVG_SUPPORT_FILTERS
OP_STATUS SVGPainter::PushBackgroundLayer(const OpRect& pixel_extents, BOOL is_new, UINT8 opacity)
{
	OP_NEW_DBG("SVGPainter::PushBackgroundLayer", "svg_background");

	if (!m_bg_layers_enabled)
		return OpStatus::OK;

	BILayer* bgi = OP_NEW(BILayer, ());
	if (!bgi)
		return OpStatus::ERR_NO_MEMORY;

	OpRect clipped_extents = pixel_extents;
	clipped_extents.IntersectWith(m_clip_extents);

	OP_STATUS status = m_renderer->createIntermediateRenderTarget(&bgi->target,
																  clipped_extents.width,
																  clipped_extents.height);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(bgi);
		return status;
	}

	m_renderer->setRenderTarget(bgi->target);
	m_renderer->setClipRect(0, 0, clipped_extents.width, clipped_extents.height);
	m_renderer->clear(0, 0, clipped_extents.width, clipped_extents.height, 0);

	bgi->area = clipped_extents;
	bgi->is_new = is_new;
	bgi->opacity = opacity;

	bgi->Into(&m_bg_layers);
	return OpStatus::OK;
}

OP_STATUS SVGPainter::PopBackgroundLayer()
{
	OP_NEW_DBG("SVGPainter::PopBackgroundLayer", "svg_background");

	if (!m_bg_layers_enabled)
		return OpStatus::OK;

	if (!HasBackgroundLayer())
	{
		OP_ASSERT(0);
		return OpStatus::OK;
	}

	// Pop off the top background image
	BILayer* srcbgi = m_bg_layers.Last();
	srcbgi->Out();

	if (!HasBackgroundLayer())
	{
		// All background blocks have been ended
		// No need to merge, just return
		OP_DBG(("Dropping background image"));
		m_renderer->setRenderTarget(NULL);
		OP_DELETE(srcbgi);
		return OpStatus::OK;
	}

	BILayer* dstbgi = m_bg_layers.Last();
	OP_ASSERT(dstbgi);

	OpRect src_area = srcbgi->area;
	OpRect dst_area = dstbgi->area;

	OP_ASSERT(!src_area.IsEmpty());
	OP_ASSERT(!dst_area.IsEmpty());

	int dst_x = src_area.x - dst_area.x;
	int dst_y = src_area.y - dst_area.y;

	VEGATransform srctrans, srcitrans;
	srctrans.loadTranslate(VEGA_INTTOFIX(dst_x), VEGA_INTTOFIX(dst_y));
	srcitrans.loadTranslate(-VEGA_INTTOFIX(dst_x), -VEGA_INTTOFIX(dst_y));

	VEGAFill* image_fill;
	if (OpStatus::IsError(srcbgi->target->getImage(&image_fill)))
	{
		OP_DELETE(srcbgi);
		return OpStatus::ERR_NO_MEMORY;
	}
	image_fill->setSpread(VEGAFill::SPREAD_CLAMP);
	image_fill->setQuality(VEGAFill::QUALITY_NEAREST);
	image_fill->setTransform(srctrans, srcitrans);
	image_fill->setFillOpacity(srcbgi->opacity);

	VEGAStencil* stencil = NULL;
	OP_STATUS status;
#ifdef SVG_SUPPORT_STENCIL
	status = SelectStencil(dst_area, stencil);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(srcbgi);
		return status;
	}
#endif // SVG_SUPPORT_STENCIL

	m_renderer->setFill(image_fill);
	m_renderer->setXORFill(false);

	m_renderer->setRenderTarget(dstbgi->target);

	// Set the cliprect to the entire surface
	m_renderer->setClipRect(0, 0, dst_area.width, dst_area.height);

	status = m_renderer->fillRect(dst_x, dst_y, src_area.width, src_area.height, stencil);

	OP_DELETE(srcbgi);

	return status;
}

OP_STATUS SVGPainter::GetBackgroundLayer(VEGARenderTarget** bgi_rt, OpRect& bgi_area,
										 const SVGRect& canvas_region)
{
	OP_ASSERT(m_bg_layers_enabled);

	SVGRect target_rect = m_transform.ApplyToRect(canvas_region);
	OpRect area = target_rect.GetEnclosingRect();

	VEGARenderer tmp_renderer;
	RETURN_IF_ERROR(tmp_renderer.Init(area.width, area.height));

	VEGARenderTarget* res_rt = NULL;
	// Merge from current to last "new"
	RETURN_IF_ERROR(tmp_renderer.createIntermediateRenderTarget(&res_rt, area.width, area.height));

	tmp_renderer.setRenderTarget(res_rt);
	tmp_renderer.setClipRect(0, 0, area.width, area.height);
	tmp_renderer.clear(0, 0, area.width, area.height, 0);

	*bgi_rt = res_rt;

	BILayer* bgi = m_bg_layers.Last();
	while (bgi)
	{
		if (bgi->is_new)
			break;

		bgi = bgi->Pred();
	}

	OP_STATUS status = OpStatus::OK;

	VEGAFilter* merge = NULL;
	VEGAMergeType prev_merge_type = VEGAMERGE_REPLACE;

	while (bgi)
	{
		// Merge surfaces
		OpRect isect(bgi->area);
		isect.IntersectWith(area);

		if (isect.IsEmpty())
			continue;

		VEGAMergeType merge_type = bgi->is_new ? VEGAMERGE_REPLACE : VEGAMERGE_NORMAL;

		if (bgi->is_new || merge_type != prev_merge_type)
		{
			OP_DELETE(merge);

			OP_STATUS status = tmp_renderer.createMergeFilter(&merge, merge_type);
			if (OpStatus::IsError(status))
			{
				VEGARenderTarget::Destroy(res_rt);
				break;
			}

			prev_merge_type = merge_type;
		}

		merge->setSource(bgi->target);

		// Setup regions
		VEGAFilterRegion region;
		region.dx = isect.x - area.x;
		region.dy = isect.y - area.y;
		region.sx = isect.x - bgi->area.x;
		region.sy = isect.y - bgi->area.y;
		region.width = isect.width;
		region.height = isect.height;

		status = tmp_renderer.applyFilter(merge, region);

		if (OpStatus::IsError(status))
			break;

		bgi = bgi->Suc();
	}
	OP_DELETE(merge);

	return status;
}
#endif // SVG_SUPPORT_FILTERS

OP_BOOLEAN SVGPainter::PushLayer(const SVGBoundingBox& extents, UINT8 opacity)
{
	OP_NEW_DBG("SVGPainter::PushLayer", "svg_opacity");

	OpAutoPtr<Layer> layer(OP_NEW(Layer, ()));
	if (!layer.get())
		return OpStatus::ERR_NO_MEMORY;

	OpRect clipped_extents = m_transform.ApplyToBoundingBox(extents).GetEnclosingRect();
	clipped_extents.IntersectWith(m_clip_extents);

	if (clipped_extents.IsEmpty())
		return OpBoolean::IS_FALSE;

	layer->area = clipped_extents;
	layer->opacity = opacity;

	RETURN_IF_ERROR(m_renderer->createIntermediateRenderTarget(&layer->target,
															   clipped_extents.width,
															   clipped_extents.height));

	m_renderer->setRenderTarget(layer->target);
	m_renderer->setClipRect(0, 0, clipped_extents.width, clipped_extents.height);
	m_renderer->clear(0, 0, clipped_extents.width, clipped_extents.height, 0);

	// Make sure text drawn with the painter makes it to the correct
	// surface - with VEGAOpPainter this should be a no-op
	OpStatus::Ignore(SyncPainter());

	layer->Into(&m_layers);

	// Update layer position
	m_curr_layer_pos = clipped_extents.TopLeft();

	layer.release();
	return OpBoolean::IS_TRUE;
}

OP_STATUS SVGPainter::PopLayer()
{
	OP_NEW_DBG("SVGPainter::PopLayer", "svg_opacity");

	if (!IsLayer())
		return OpStatus::OK;

	// Make sure text drawn with the painter makes it to the correct
	// surface - with VEGAOpPainter this should be a no-op
	OpStatus::Ignore(SyncPainter());

	// Pop off the top transparency layer
	Layer* srclayer = m_layers.Last();
	srclayer->Out();

	OpRect src_area = srclayer->area;

	VEGARenderTarget* dst_target = GetLayer();
	OpRect dst_area = GetLayerArea();

	OP_STATUS status = OpStatus::OK;
	if (!src_area.IsEmpty())
	{
		OP_ASSERT(!dst_area.IsEmpty());
		OP_ASSERT(src_area.x >= dst_area.x && src_area.y >= dst_area.y);

#ifdef SVG_SUPPORT_STENCIL
		// Does the layer have a mask attached?
		if (VEGAStencil* srcmask = srclayer->mask)
		{
			VEGAFill* srcfill = NULL;
			status = srclayer->target->getImage(&srcfill);
			if (OpStatus::IsError(status))
			{
				OP_DELETE(srclayer);
				return status;
			}

			// Adjust the offset of the mask
			OP_ASSERT(m_area.x <= dst_area.x && m_area.y <= dst_area.y);
			srcmask->setOffset(m_area.x - dst_area.x, m_area.y - dst_area.y);

			OpPoint ofs(src_area.x - dst_area.x, src_area.y - dst_area.y);

			VEGATransform trans, itrans;
			trans.loadTranslate(VEGA_INTTOFIX(ofs.x), VEGA_INTTOFIX(ofs.y));
			itrans.loadTranslate(VEGA_INTTOFIX(-ofs.x), VEGA_INTTOFIX(-ofs.y));

			srcfill->setTransform(trans, itrans);
			srcfill->setQuality(VEGAFill::QUALITY_NEAREST);
			srcfill->setFillOpacity(srclayer->opacity);

			m_renderer->setRenderTarget(dst_target);
			m_renderer->setFill(srcfill);
			m_renderer->setClipRect(ofs.x, ofs.y, src_area.width, src_area.height);

			status = m_renderer->fillRect(ofs.x, ofs.y, src_area.width, src_area.height, srcmask);

			m_renderer->setFill(NULL);
		}
		else
#endif // SVG_SUPPORT_STENCIL
		{
			// Blend src to dst
			VEGAFilter* filter;

			if (OpStatus::IsError(m_renderer->createOpacityMergeFilter(&filter, srclayer->opacity)))
			{
				OP_DELETE(srclayer);
				return OpStatus::ERR_NO_MEMORY;
			}

			filter->setSource(srclayer->target);

			VEGAFilterRegion region;
			region.sx = region.sy = 0;
			region.dx = src_area.x - dst_area.x;
			region.dy = src_area.y - dst_area.y;
			region.width = src_area.width;
			region.height = src_area.height;

			m_renderer->setRenderTarget(dst_target);

			status = m_renderer->applyFilter(filter, region);

			OP_DELETE(filter);
		}
	}

	OP_DELETE(srclayer);

	// Update layer position
	m_curr_layer_pos = dst_area.TopLeft();

	return status;
}

VEGARenderTarget* SVGPainter::GetLayer()
{
	if (IsLayer())
		return m_layers.Last()->target;

	return m_rendertarget;
}

const OpRect& SVGPainter::GetLayerArea()
{
	if (IsLayer())
		return m_layers.Last()->area;

	return m_area;
}

void SVGPainter::SetVegaTransform()
{
	m_transform.CopyToVEGATransform(m_vtransform);
}

#define VNUM(s) ((s).GetVegaNumber())

OP_STATUS SVGPainter::DrawLine(SVGNumber x1, SVGNumber y1, SVGNumber x2, SVGNumber y2)
{
	if (!m_object_props->stroked)
		return OpStatus::OK;

	VEGAPath vpath;
	RETURN_IF_ERROR(vpath.moveTo(VNUM(x1), VNUM(y1)));
	RETURN_IF_ERROR(vpath.lineTo(VNUM(x2), VNUM(y2)));

	SetVegaTransform();

	VEGAPath vspath;
	RETURN_IF_ERROR(CreateStrokePath(vpath, vspath));

	VPrimitive prim;
	prim.vpath = &vspath;
	prim.vprim = NULL;
	prim.geom_type = VPrimitive::GEOM_STROKE;

	return RenderPrimitive(prim);
}

OP_STATUS SVGPainter::DrawPolygon(SVGVector& list, BOOL closed)
{
	if (list.VectorType() != SVGOBJECT_POINT)
		return OpSVGStatus::BAD_PARAMETER;

	SVGPoint* c = static_cast<SVGPoint*>(list.Get(0));
	if (!c)
		return OpStatus::OK;

	unsigned int prep_count = list.GetCount();
	if (!closed)
		prep_count--;

	VEGAPath vpath;
	RETURN_IF_ERROR(vpath.prepare(prep_count));
	RETURN_IF_ERROR(vpath.moveTo(VNUM(c->x), VNUM(c->y)));

	for (UINT32 i = 1; i < list.GetCount(); i++)
	{
		c = static_cast<SVGPoint*>(list.Get(i));
		RETURN_IF_ERROR(vpath.lineTo(VNUM(c->x), VNUM(c->y)));
	}

	if (closed)
		RETURN_IF_ERROR(vpath.close(true));

	SetVegaTransform();

	return FillStrokePath(vpath, VEGA_INTTOFIX(-1));
}

OP_STATUS SVGPainter::FillStrokePrimitive(VEGAPrimitive& vprimitive)
{
	BOOL filled = m_object_props->filled;

	VPrimitive filled_prim;
	if (filled)
	{
		filled_prim.vpath = NULL;
		filled_prim.vprim = &vprimitive;
		filled_prim.geom_type = VPrimitive::GEOM_FILL;
	}

	OP_STATUS status = OpStatus::OK;

	BOOL stroked = m_object_props->stroked;

	VPrimitive stroked_prim;
	VEGAPath vspath;
	if (stroked)
	{
		VEGAPath path;

		status = path.appendPrimitive(&vprimitive);

		if (OpStatus::IsSuccess(status))
		{
			status = CreateStrokePath(path, vspath);

			stroked_prim.vpath = &vspath;
			stroked_prim.vprim = NULL;
			stroked_prim.geom_type = VPrimitive::GEOM_STROKE;
		}

		if (OpStatus::IsError(status))
			stroked = FALSE;
	}

	if (filled)
		OpStatus::Ignore(RenderPrimitive(filled_prim));
	if (stroked)
		status = RenderPrimitive(stroked_prim);

	return status;
}

OP_STATUS SVGPainter::FillStrokePath(VEGAPath& vpath, VEGA_FIX pathLength)
{
	BOOL filled = m_object_props->filled;
	BOOL stroked = m_object_props->stroked;

	OP_STATUS status = OpStatus::OK;
	VPrimitive filled_prim;
	if (filled)
	{
		// pathLength == 0 means no fill, but might mean that linecaps
		// should be applied
		if (pathLength != 0)
		{
			filled_prim.vpath = &vpath;
			filled_prim.vprim = NULL;
			filled_prim.geom_type = VPrimitive::GEOM_FILL;
		}
		else
		{
			filled = FALSE;
		}
	}

	VPrimitive stroked_prim;
	VEGAPath vspath;
	if (stroked)
	{
		status = CreateStrokePath(vpath, vspath, pathLength);

		stroked_prim.vpath = &vspath;
		stroked_prim.vprim = NULL;
		stroked_prim.geom_type = VPrimitive::GEOM_STROKE;
	}

	if (filled)
		OpStatus::Ignore(RenderPrimitive(filled_prim));
	if (stroked && OpStatus::IsSuccess(status))
		status = RenderPrimitive(stroked_prim);

	return status;
}

static inline bool SVGFillRuleToVEGAFillRule(SVGFillRule svg_fill_rule)
{
	switch (svg_fill_rule)
	{
	case SVGFILL_EVEN_ODD:
		return true;

	case SVGFILL_NON_ZERO:
	case SVGFILL_UNKNOWN:
	default:
		return false;
	}
}

OP_STATUS SVGPainter::RenderPrimitive(VPrimitive& prim)
{
	if (m_current_quality != m_object_props->aa_quality)
	{
		RETURN_IF_ERROR(m_renderer->setQuality(m_object_props->aa_quality));

		m_current_quality = m_object_props->aa_quality;
	}

	OpPoint ofs;
	GetCurrentOffset(ofs);

	VEGARenderTarget* target = GetLayer();
	OpRect target_area = GetLayerArea();

	VEGAStencil* stencil = NULL;
	OP_STATUS result = SelectStencil(target_area, stencil);

	switch (prim.geom_type)
	{
	case VPrimitive::GEOM_FILL:
		m_renderer->setXORFill(SVGFillRuleToVEGAFillRule((SVGFillRule)m_object_props->fillrule));

		SetupPaint(FILL_PAINT);
		break;

	case VPrimitive::GEOM_STROKE:
		// Strokes always use non-zero winding
		m_renderer->setXORFill(false);

		SetupPaint(STROKE_PAINT);
		break;

	case VPrimitive::GEOM_IMAGE:
		SetupImageSource(ofs);
		break;

	default:
		OP_ASSERT(0);
		break;
	}

	if (OpStatus::IsSuccess(result))
	{
		VEGARenderTarget* current_rt = target;
		OpRect current_area = target_area;

#ifdef SVG_SUPPORT_STENCIL
		if (m_clip_mode == STENCIL_CONSTRUCT)
		{
			current_rt = GetStencil(m_partial_clips);
			current_area = m_area;
		}
#endif // SVG_SUPPORT_STENCIL

		// I would like to know if this actually occurs, since I consider it a bug.
		// libvega seems to handle it though, so we should be safe anyway.
		OP_ASSERT(current_rt);

		VEGATransform layer_ctm;
		layer_ctm.loadTranslate(VEGA_INTTOFIX(-ofs.x), VEGA_INTTOFIX(-ofs.y));
		layer_ctm.multiply(m_vtransform);

		if (SetRenderTarget(current_rt, current_area))
		{
			if (prim.vpath)
			{
				prim.vpath->transform(&layer_ctm);

				result = m_renderer->fillPath(prim.vpath, stencil);
			}
			else /* prim.vprim */
			{
				prim.vprim->transform = &layer_ctm;

				result = m_renderer->fillPrimitive(prim.vprim, stencil);
			}
		}

#ifdef SVG_SUPPORT_FILTERS
		if (!(m_clip_mode == STENCIL_CONSTRUCT || m_mask_mode == STENCIL_CONSTRUCT) &&
			HasBackgroundLayer())
		{
			BILayer* bgi = m_bg_layers.Last();

			OpPoint bgimg_ofs(bgi->area.x, bgi->area.y);

			switch (prim.geom_type)
			{
			case VPrimitive::GEOM_FILL:
			case VPrimitive::GEOM_STROKE:
				// FIXME: Needs to re-setup other paint types as well
				break;

			case VPrimitive::GEOM_IMAGE:
				SetupImageSource(bgimg_ofs);
				break;

			default:
				OP_ASSERT(0);
				break;
			}

			result = RenderToBackgroundImage(prim);
		}
#endif // SVG_SUPPORT_FILTERS
	}

	m_renderer->setFill(NULL);

	return result;
}

/** Draw a path consisting of lines and curves (might add more later).
  * The paths can be either open or closed, a path might contain another path. */
OP_STATUS SVGPainter::DrawPath(const OpBpath *path, SVGNumber path_length)
{
	unsigned cmdcount = path->GetCount(TRUE);
	if (cmdcount == 0)
		return OpStatus::OK;

	VEGAPath vpath;
	RETURN_IF_ERROR(vpath.prepare(cmdcount));

	VEGA_FIX vflatness = CalculateFlatness();

	PathSegListIterator* iter = path->GetPathIterator(TRUE); /* normalized */
	if (!iter)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS cmd_status = OpStatus::OK;
	for (unsigned int i = 0; i < cmdcount && OpStatus::IsSuccess(cmd_status); i++)
	{
		const SVGPathSeg* cmd = iter->GetNext();

		if (i == 0 && (cmd->info.type != SVGPathSeg::SVGP_MOVETO_ABS &&
					   cmd->info.type != SVGPathSeg::SVGP_CLOSE))
		{
			OP_DELETE(iter);
			return OpStatus::OK;
		}

		switch(cmd->info.type)
		{
		case SVGPathSeg::SVGP_MOVETO_ABS:
			cmd_status = vpath.moveTo(VNUM(cmd->x), VNUM(cmd->y));
			break;
		case SVGPathSeg::SVGP_LINETO_ABS:
			cmd_status = vpath.lineTo(VNUM(cmd->x), VNUM(cmd->y));
			break;
		case SVGPathSeg::SVGP_CURVETO_CUBIC_ABS:
			cmd_status = vpath.cubicBezierTo(VNUM(cmd->x), VNUM(cmd->y),
											 VNUM(cmd->x1), VNUM(cmd->y1),
											 VNUM(cmd->x2), VNUM(cmd->y2),
											 vflatness);
			break;
		case SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS:
			cmd_status = vpath.quadraticBezierTo(VNUM(cmd->x), VNUM(cmd->y),
												 VNUM(cmd->x1), VNUM(cmd->y1),
												 vflatness);
			break;
		case SVGPathSeg::SVGP_ARC_ABS:
			cmd_status = vpath.arcTo(VNUM(cmd->x), VNUM(cmd->y),
									 VNUM(cmd->x1), VNUM(cmd->y1),
									 VNUM(cmd->x2),
									 cmd->info.large ? true : false,
									 cmd->info.sweep ? true : false,
									 vflatness);
			break;
		case SVGPathSeg::SVGP_CLOSE:
			cmd_status = vpath.close(true);
			break;
		}
	}
	OP_DELETE(iter);

	RETURN_IF_ERROR(cmd_status);

	SetVegaTransform();

	return FillStrokePath(vpath, VNUM(path_length));
}

OP_STATUS SVGPainter::DrawRect(SVGNumber x, SVGNumber y, SVGNumber w, SVGNumber h,
							   SVGNumber rx, SVGNumber ry)
{
	VEGAPrimitive vprim;
	vprim.transform = NULL;

	VEGA_FIX corners[2];

	if (rx.Equal(SVGNumber(0)))
	{
		// Simple rect
		vprim.type = VEGAPrimitive::RECTANGLE;
		vprim.flatness = 0; // This primitive should not need a valid flatness

		vprim.data.rect.x = VNUM(x);
		vprim.data.rect.y = VNUM(y);
		vprim.data.rect.width = VNUM(w);
		vprim.data.rect.height = VNUM(h);
	}
	else
	{
		// Rounded rect
		vprim.type = VEGAPrimitive::ROUNDED_RECT_UNIFORM;
		vprim.flatness = CalculateFlatness();

		corners[0] = VNUM(rx);
		corners[1] = VNUM(ry);

		vprim.data.rrect.x = VNUM(x);
		vprim.data.rrect.y = VNUM(y);
		vprim.data.rrect.width = VNUM(w);
		vprim.data.rrect.height = VNUM(h);
		vprim.data.rrect.corners = corners;
	}

	SetVegaTransform();

	return FillStrokePrimitive(vprim);
}

void SVGPainter::DrawRectWithPaint(const SVGRect& rect, const SVGPaintDesc& fill)
{
	OP_ASSERT(fill.pserver == NULL); // We don't expect paintservers here

	SVGObjectProperties rect_props;
	rect_props.aa_quality = VEGA_DEFAULT_QUALITY;
	rect_props.fillrule = SVGFILL_NON_ZERO;
	rect_props.filled = TRUE;
	rect_props.stroked = FALSE;

	SVGPaintDesc saved_paint0 = m_paints[FILL_PAINT];
	SVGObjectProperties* saved_object_props = m_object_props;

	m_object_props = &rect_props;
	m_paints[FILL_PAINT] = fill;

	OpStatus::Ignore(DrawRect(rect.x, rect.y, rect.width, rect.height, 0, 0));

	m_paints[FILL_PAINT] = saved_paint0;
	m_object_props = saved_object_props;
}

VEGA_FIX SVGPainter::CalculateFlatness()
{
	SVGNumber expansion = m_transform.GetExpansionFactor();
	if (expansion > 0)
		return VNUM(m_flatness / expansion);

	return VEGA_INTTOFIX(1) / 4; /* 0.25 as default */
}

OP_STATUS SVGPainter::DrawEllipse(SVGNumber x, SVGNumber y, SVGNumber rx, SVGNumber ry)
{
	VEGA_FIX vf_flat = CalculateFlatness();

	VEGAPath vpath;
#ifdef SVG_SUPPORT_ELLIPTICAL_ARCS
	RETURN_IF_ERROR(vpath.prepare(1 + 2 + 1));

	RETURN_IF_ERROR(vpath.moveTo(VNUM(x+rx), VNUM(y)));
	// Divided in two arcs because when start and end is the same it's ignored
	RETURN_IF_ERROR(vpath.arcTo(VNUM(x-rx), VNUM(y), VNUM(rx), VNUM(ry),
								0, FALSE, TRUE, vf_flat));
	RETURN_IF_ERROR(vpath.arcTo(VNUM(x+rx), VNUM(y), VNUM(rx), VNUM(ry),
								0, FALSE, TRUE, vf_flat));
	RETURN_IF_ERROR(vpath.close(true));
#else
	RETURN_IF_ERROR(vpath.prepare(1 + 4 + 1));

	RETURN_IF_ERROR(vpath.moveTo(VNUM(x+rx), VNUM(y)));
	// Using same approx. as for rounded rects
	RETURN_IF_ERROR(vpath.cubicBezierTo(VNUM(x), VNUM(y+ry),
										VNUM(x+rx), VNUM(y+ry*0.552285),
										VNUM(x+rx*0.552285), VNUM(y+ry),
										vf_flat));
	RETURN_IF_ERROR(vpath.cubicBezierTo(VNUM(x-rx), VNUM(y),
										VNUM(x-rx*0.552285), VNUM(y+ry),
										VNUM(x-rx), VNUM(y+ry*0.552285),
										vf_flat));
	RETURN_IF_ERROR(vpath.cubicBezierTo(VNUM(x), VNUM(y-ry),
										VNUM(x-rx), VNUM(y-ry*0.552285),
										VNUM(x-rx*0.552285), VNUM(y-ry),
										vf_flat));
	RETURN_IF_ERROR(vpath.cubicBezierTo(VNUM(x+rx), VNUM(y),
										VNUM(x+rx*0.552285), VNUM(y-ry),
										VNUM(x+rx), VNUM(y-ry*0.552285),
										vf_flat));
	RETURN_IF_ERROR(vpath.close(true));
#endif // SVG_SUPPORT_ELLIPTICAL_ARCS

	SetVegaTransform();

	return FillStrokePath(vpath, VEGA_INTTOFIX(-1));
}

#ifdef SVG_SUPPORT_STENCIL
void SVGPainter::ApplyClipToRegion(OpRect& region)
{
	if (Stencil* clip = m_active_clips.Last())
		region.IntersectWith(clip->area);
}
#endif // SVG_SUPPORT_STENCIL

OP_STATUS SVGPainter::SelectStencil(const OpRect& dstrect, VEGAStencil*& stencil)
{
#ifdef SVG_SUPPORT_STENCIL
	VEGAStencil* clip = GetActualClipStencil();
	if (!clip)
		return OpStatus::OK;

	// Reset the stencil offset
	clip->setOffset(0, 0);

	// If a stencil is being used, and the dimensions of the
	// stencils differ from the dimensions on surface we want to
	// render to - adjust the offset of the stencil.
	if (m_clip_mode != STENCIL_CONSTRUCT && m_mask_mode != STENCIL_CONSTRUCT)
	{
		if (((int)clip->getWidth() != dstrect.width ||
			 (int)clip->getHeight() != dstrect.height))
		{
			OP_ASSERT(dstrect.y - m_area.y >= 0 && dstrect.x - m_area.x >= 0);
			clip->setOffset(m_area.x - dstrect.x, m_area.y - dstrect.y);
		}
	}

	stencil = clip;
	return OpStatus::OK;
#else
	return OpStatus::OK;
#endif // SVG_SUPPORT_STENCIL
}

#ifdef SVG_SUPPORT_STENCIL
BOOL SVGPainter::GetClipRect(OpRect& cr)
{
	if (Stencil* vega_stencil = m_active_clips.Last())
		if (vega_stencil->stencil == NULL)
		{
			cr = vega_stencil->area;
			return TRUE;
		}

	return FALSE;
}
#endif // SVG_SUPPORT_STENCIL

BOOL SVGPainter::SetRenderTarget(VEGARenderTarget* rt, const OpRect& area)
{
	m_renderer->setRenderTarget(rt);

	OpRect cliprect = area;

#ifdef SVG_SUPPORT_STENCIL
	OpRect clip;
	if (GetClipRect(clip))
	{
		cliprect.IntersectWith(clip);
		if (cliprect.IsEmpty())
			return FALSE;
	}
#endif // SVG_SUPPORT_STENCIL

	OP_NEW_DBG("SVGPainter::SetupRenderTarget", "svg_opacity");

	cliprect.IntersectWith(m_clip_extents);
	cliprect.OffsetBy(-area.x, -area.y);

	OP_DBG(("Setting clip [%d %d %d %d]", cliprect.x, cliprect.y, cliprect.width, cliprect.height));

	if (cliprect.IsEmpty())
		return FALSE;

	// Restore the clip we just destroyed.
	m_renderer->setClipRect(cliprect.x, cliprect.y, cliprect.width, cliprect.height);
	return TRUE;
}

#ifdef SVG_SUPPORT_FILTERS
OP_STATUS SVGPainter::RenderToBackgroundImage(VPrimitive& prim)
{
	OP_ASSERT(m_clip_mode != STENCIL_CONSTRUCT &&
			  m_mask_mode != STENCIL_CONSTRUCT);
	BILayer* bgi = m_bg_layers.Last();

	m_renderer->setRenderTarget(bgi->target);

	// Set the cliprect to the entire surface
	m_renderer->setClipRect(0, 0, bgi->area.width, bgi->area.height);

	// Adjust layer transform so rendering is relative to the correct
	// surface (the background image)
	// This naturally assumes that the path was previously transformed
	// (by RenderPrimitive).
	int layer_readj_x = m_curr_layer_pos.x - bgi->area.x;
	int layer_readj_y = m_curr_layer_pos.y - bgi->area.y;

	VEGATransform bglayer_ctm;
	bglayer_ctm.loadTranslate(VEGA_INTTOFIX(layer_readj_x), VEGA_INTTOFIX(layer_readj_y));

	if (prim.vpath)
	{
		prim.vpath->transform(&bglayer_ctm);

		return m_renderer->fillPath(prim.vpath, NULL);
	}
	else /* prim.vprim */
	{
		if (prim.vprim->transform)
			bglayer_ctm.multiply(*prim.vprim->transform);

		prim.vprim->transform = &bglayer_ctm;

		return m_renderer->fillPrimitive(prim.vprim, NULL);
	}
}
#endif // SVG_SUPPORT_FILTERS

void SVGPainter::ImageInfo::Reset()
{
	if (need_free)
		OP_DELETE(img);
	img = NULL;
	need_free = FALSE;
}

OP_STATUS SVGPainter::ImageInfo::Setup(VEGARenderer* rend, OpBitmap* bitmap)
{
	RETURN_IF_ERROR(rend->createImage(&img, bitmap));
	need_free = TRUE;
	return OpStatus::OK;
}

OP_STATUS SVGPainter::ImageInfo::Setup(VEGARenderer* rend, VEGARenderTarget* rt)
{
	need_free = FALSE;
	return rt->getImage(&img);
}

void SVGPainter::SetupImageSource(const OpPoint& base_ofs)
{
	OP_NEW_DBG("SVGPainter::SetupImageSource", "svg_image");

	m_renderer->setXORFill(false);

	if (!m_imageinfo.img /* Can this be NULL here ? */)
	{
		m_renderer->setColor(0xFF000000);
		return;
	}

	VEGATransform imagetrans, temptrans;

	// translate
	imagetrans.loadTranslate((SVGNumber(m_imageinfo.source.x).GetVegaNumber()),
							 (SVGNumber(m_imageinfo.source.y).GetVegaNumber()));
	// scale
	VEGA_FIX image_scale_x = (SVGNumber(m_imageinfo.source.width) / m_imageinfo.dest.width).GetVegaNumber();
	VEGA_FIX image_scale_y = (SVGNumber(m_imageinfo.source.height) / m_imageinfo.dest.height).GetVegaNumber();
	temptrans.loadScale(image_scale_x, image_scale_y);
	imagetrans.multiply(temptrans);
	// translate
	temptrans.loadTranslate(VNUM(-m_imageinfo.dest.x), VNUM(-m_imageinfo.dest.y));
	imagetrans.multiply(temptrans);

	// inverted trans
	temptrans.loadTranslate(VEGA_INTTOFIX(-base_ofs.x), VEGA_INTTOFIX(-base_ofs.y));
	temptrans.multiply(m_vtransform);
	temptrans.invert();
	imagetrans.multiply(temptrans);

	VEGAFill::Quality imagequality;
	switch(m_imageinfo.quality)
	{
	case SVGPainter::IMAGE_QUALITY_LOW:
		imagequality = VEGAFill::QUALITY_NEAREST;
		break;

	case SVGPainter::IMAGE_QUALITY_NORMAL:
	case SVGPainter::IMAGE_QUALITY_HIGH:
	default:
		imagequality = VEGAFill::QUALITY_LINEAR;
		break;
	}
	m_imageinfo.img->setQuality(imagequality);
	// FIXME: not optimal
	temptrans.copy(imagetrans);
	temptrans.invert();

	OP_DBG(("baseOfs [%d %d] scale: [%g %g] srcOffset: [%d %d] dstOffset: [%g %g]",
			base_ofs.x, base_ofs.y,
			image_scale_x, image_scale_y,
			m_imageinfo.source.x, m_imageinfo.source.y,
			m_imageinfo.dest.x.GetFloatValue(), m_imageinfo.dest.y.GetFloatValue()));
	OP_DBG(("temptrans [%g %g %g %g %g %g] imagetrans: [%g %g %g %g %g %g]",
			temptrans[0], temptrans[1], temptrans[2],
			temptrans[3], temptrans[4], temptrans[5],
			imagetrans[0], imagetrans[1], imagetrans[2],
			imagetrans[3], imagetrans[4], imagetrans[5]));

	m_imageinfo.img->setTransform(temptrans, imagetrans);
	m_imageinfo.img->setSpread(VEGAFill::SPREAD_CLAMP);

	m_imageinfo.img->setFillOpacity(m_imageopacity);

	m_renderer->setFill(m_imageinfo.img);
}

OP_STATUS SVGPainter::ProcessImage(const SVGRect& dest)
{
	VPrimitive prim;
	prim.geom_type = VPrimitive::GEOM_IMAGE;

	VEGAPrimitive vprim;
	vprim.transform = NULL;

	// Simple rect
	vprim.type = VEGAPrimitive::RECTANGLE;
	vprim.flatness = 0; // This primitive should not need a valid flatness

	vprim.data.rect.x = dest.x.GetVegaNumber();
	vprim.data.rect.y = dest.y.GetVegaNumber();
	vprim.data.rect.width = dest.width.GetVegaNumber();
	vprim.data.rect.height = dest.height.GetVegaNumber();

	prim.vprim = &vprim;
	prim.vpath = NULL;

	SVGObjectProperties* saved_obj_props = m_object_props;
	SVGObjectProperties tmp_obj_props;
	tmp_obj_props.aa_quality = VEGA_DEFAULT_QUALITY;
	tmp_obj_props.fillrule = SVGFILL_NON_ZERO;
	tmp_obj_props.filled = TRUE;
	tmp_obj_props.stroked = FALSE;

	m_object_props = &tmp_obj_props;

	OP_STATUS status = RenderPrimitive(prim);

	m_object_props = saved_obj_props;

	return status;
}

OP_STATUS SVGPainter::DrawImage(OpBitmap* bitmap, const OpRect& source,
								const SVGRect& dest, ImageRenderQuality quality)
{
#ifdef SVG_SUPPORT_STENCIL
	if (m_clip_mode == STENCIL_CONSTRUCT)
		return OpStatus::OK;
#endif // SVG_SUPPORT_STENCIL

	m_imageinfo.dest = dest;
	m_imageinfo.source = source;

	m_imageinfo.Reset();
	m_imageinfo.quality = quality;
	if (bitmap)
		RETURN_IF_ERROR(m_imageinfo.Setup(m_renderer, bitmap));

	SetVegaTransform();

	OP_STATUS result = ProcessImage(dest);

	if (bitmap)
		m_imageinfo.Reset();

	return result;
}

OP_STATUS SVGPainter::DrawImage(VEGARenderTarget* image_rt, const OpRect& in_source,
								const SVGRect& dest, ImageRenderQuality quality)
{
#ifdef SVG_SUPPORT_STENCIL
	OP_ASSERT(m_clip_mode == STENCIL_USE);
#endif // SVG_SUPPORT_STENCIL

	m_imageinfo.dest = dest;
	m_imageinfo.source = in_source;

	m_imageinfo.Reset();
	m_imageinfo.quality = quality;

	if (image_rt)
	{
		RETURN_IF_ERROR(m_imageinfo.Setup(m_renderer, image_rt));

		if (m_imageinfo.source.IsEmpty())
			m_imageinfo.source.Set(0, 0, image_rt->getWidth(), image_rt->getHeight());
	}

	SetVegaTransform();

	OP_STATUS result = ProcessImage(dest);

	if (image_rt)
		m_imageinfo.Reset();

	return result;
}

/** Clear the canvas to the specified color. */
void SVGPainter::Clear(UINT32 color, const OpRect* pixel_clearrect)
{
	OP_ASSERT(m_renderer);
	OP_ASSERT(m_rendertarget);

	UINT8 a = GetSVGColorAValue(color);
	UINT8 r = GetSVGColorRValue(color);
	UINT8 g = GetSVGColorGValue(color);
	UINT8 b = GetSVGColorBValue(color);
	color = (a << 24) | (r << 16) | (g << 8) | b;

	OpRect cl_rect(m_area);

	if (pixel_clearrect)
		cl_rect.IntersectWith(*pixel_clearrect);

	if (cl_rect.IsEmpty())
		return;

	cl_rect.OffsetBy(OpPoint(-m_area.x, -m_area.y));

	m_renderer->setRenderTarget(m_rendertarget);
	m_renderer->setClipRect(cl_rect.x, cl_rect.y, cl_rect.width, cl_rect.height);
	m_renderer->clear(cl_rect.x, cl_rect.y, cl_rect.width, cl_rect.height, color);
}

void SVGPainter::SetupPaint(unsigned paint_index)
{
	const SVGPaintDesc& paint = m_paints[paint_index];

	UINT32 color = paint.color;

	UINT8 a = GetSVGColorAValue(color);
	UINT8 r = GetSVGColorRValue(color);
	UINT8 g = GetSVGColorGValue(color);
	UINT8 b = GetSVGColorBValue(color);

	if (paint.opacity != 255)
		a = (((a + 1) * paint.opacity) >> 8);

	color = (a << 24) | (r << 16) | (g << 8) | b;

	m_renderer->setColor(color);

#ifdef SVG_SUPPORT_PAINTSERVERS
	if (paint.pserver)
		m_renderer->setFill(m_fills[paint_index]);
#endif // SVG_SUPPORT_PAINTSERVERS
}

static inline VEGALineCap SVGCapToVEGACap(SVGCapType svg_cap)
{
	switch (svg_cap)
	{
	case SVGCAP_ROUND:
		return VEGA_LINECAP_ROUND;

	case SVGCAP_SQUARE:
		return VEGA_LINECAP_SQUARE;

	default:
		return VEGA_LINECAP_BUTT;
	}
}

static inline VEGALineJoin SVGJoinToVEGAJoin(SVGJoinType svg_join)
{
	switch (svg_join)
	{
	case SVGJOIN_ROUND:
		return VEGA_LINEJOIN_ROUND;

	case SVGJOIN_MITER:
		return VEGA_LINEJOIN_MITER;

	default:
		return VEGA_LINEJOIN_BEVEL;
	}
}

OP_STATUS SVGPainter::CreateStrokePath(VEGAPath& vpath, VEGAPath& vstrokepath, VEGA_FIX precompPathLength)
{
	OP_NEW_DBG("SVGPainter::CreateStrokePath", "svg_strokepath");
	VEGAPath* stroker = &vpath;

	stroker->setLineWidth(VNUM(m_stroke_props->width));
	stroker->setMiterLimit(VNUM(m_stroke_props->miter_limit));
	stroker->setLineCap(SVGCapToVEGACap((SVGCapType)m_stroke_props->cap));
	stroker->setLineJoin(SVGJoinToVEGAJoin((SVGJoinType)m_stroke_props->join));

	VEGA_FIX vflatness = CalculateFlatness();

	VEGAPath *shape;
	VEGAPath host_vpath;
	VEGATransform inverse_host_xfrm;
	if (m_stroke_props->non_scaling)
	{
		// Transform to the "host" coordinate space
		inverse_host_xfrm.copy(m_vtransform);
		if (inverse_host_xfrm.invert() && OpStatus::IsSuccess(host_vpath.copy(&vpath)))
		{
			host_vpath.transform(&m_vtransform);
			shape = &host_vpath;
		}
		else
		{
			inverse_host_xfrm.loadIdentity();
			shape = &vpath;
		}
	}
	else
	{
		shape = &vpath;
	}

	VEGAPath dash;
	const SVGResolvedDashArray* dash_props = m_stroke_props->dash_array;

	SVGNumber expansion = m_transform.GetExpansionFactor();
	if (dash_props && dash_props->dash_size > 0 && dash_props->dashes)
	{
		const SVGNumber* dashes = dash_props->dashes;
		VEGA_FIX *dasharray = OP_NEWA(VEGA_FIX, dash_props->dash_size);
		if (!dasharray)
			return OpStatus::ERR_NO_MEMORY;

		SVGNumber dashlength = 0;
		for (unsigned int dnum = 0; dnum < dash_props->dash_size; ++dnum)
		{
			dashlength += dashes[dnum];
			dasharray[dnum] = VNUM(dashes[dnum]);

			// This is a workaround for the case where
			// a dasharray is setup with a zero-length dash
			if (!(dnum & 1) && dasharray[dnum] < VEGA_EPSILON)
				dasharray[dnum] = 100*VEGA_EPSILON;
		}

		if (dashlength * expansion > m_flatness)
		{
			if (OpStatus::IsSuccess(shape->createDash(&dash, VNUM(m_stroke_props->dash_offset),
													  dash_props->dash_size, dasharray,
													  precompPathLength)))
				shape = &dash;
		}

		OP_DELETEA(dasharray);
	}

	shape->createOutline(&vstrokepath, vflatness, 0); // FIXME:OOM

	if (m_stroke_props->non_scaling)
	{
		// Transform to the "host" coordinate space
		vstrokepath.transform(&inverse_host_xfrm);

#ifdef _DEBUG
		OP_NEW_DBG("svgpainter", "svg_nsstroke");
		VEGA_FIX x,y,w,h;
		vstrokepath.getBoundingBox(x,y,w,h);
		OP_DBG(("Stroked nonscaling path in host coordinate space: %g %g %g %g",
			x,y,w,h));
#endif // _DEBUG
	}

	return OpStatus::OK;
}

#undef VNUM

void SVGPainter::GetSurfaceTransform(VEGATransform& trans)
{
	// Layer transform
	OpPoint layer_ofs;
	GetCurrentOffset(layer_ofs);
	trans.loadTranslate(VEGA_INTTOFIX(-layer_ofs.x), VEGA_INTTOFIX(-layer_ofs.y));

	// Canvas/Userspace Transform
	VEGATransform tmp;
	m_transform.CopyToVEGATransform(tmp);
	trans.multiply(tmp);
}

#ifdef SVG_SUPPORT_PAINTSERVERS
OP_STATUS SVGPainter::SetupPaintServer(SVGPaintServer* pserver, SVGPaintNode* context_node,
									   VEGAFill* &vfill)
{
	VEGATransform vfilltrans;
	RETURN_IF_ERROR(pserver->GetFill(&vfill, vfilltrans, this, context_node));

	// trans = { FillTransform } * { CTM * T(-layer_ofs) }

	VEGATransform trans;
	GetSurfaceTransform(trans);

	trans.multiply(vfilltrans);

	VEGATransform itrans;
	itrans.copy(trans);
	itrans.invert();

	vfill->setTransform(trans, itrans);
	return OpStatus::OK;
}
#endif // SVG_SUPPORT_PAINTSERVERS

OP_STATUS SVGPainter::BeginDrawing(SVGPaintNode* context_node)
{
#ifdef SVG_SUPPORT_PAINTSERVERS
	if (m_paints[FILL_PAINT].pserver)
	{
		RETURN_IF_ERROR(SetupPaintServer(m_paints[FILL_PAINT].pserver, context_node, m_fills[FILL_PAINT]));
		m_fills[FILL_PAINT]->setFillOpacity(m_paints[FILL_PAINT].opacity);
	}
	if (m_paints[STROKE_PAINT].pserver)
	{
		RETURN_IF_ERROR(SetupPaintServer(m_paints[STROKE_PAINT].pserver, context_node, m_fills[STROKE_PAINT]));
		m_fills[STROKE_PAINT]->setFillOpacity(m_paints[STROKE_PAINT].opacity);
	}
#endif // SVG_SUPPORT_PAINTSERVERS
	return OpStatus::OK;
}

OP_STATUS SVGPainter::EndDrawing()
{
#ifdef SVG_SUPPORT_PAINTSERVERS
	if (m_paints[FILL_PAINT].pserver)
	{
		m_paints[FILL_PAINT].pserver->PutFill(m_fills[FILL_PAINT]);
		m_fills[FILL_PAINT] = NULL;
	}
	if (m_paints[STROKE_PAINT].pserver)
	{
		m_paints[STROKE_PAINT].pserver->PutFill(m_fills[STROKE_PAINT]);
		m_fills[STROKE_PAINT] = NULL;
	}
#endif // SVG_SUPPORT_PAINTSERVERS
	return OpStatus::OK;
}

#endif // SVG_SUPPORT
