/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#ifndef PIXELSCALEPAINTER_H
#define PIXELSCALEPAINTER_H

#if defined(VEGA_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT) && defined(PIXEL_SCALE_RENDERING_SUPPORT)

#include "modules/display/pixelscaler.h"
#include "modules/libvega/vegawindow.h"
#include "modules/libvega/src/oppainter/vegaopbitmap.h"
#include "modules/libvega/src/oppainter/vegaopfont.h"
#include "modules/libvega/src/oppainter/vegaoppainter.h"

#if defined(VEGA_NO_FONTMANAGER_CREATE) && !defined(VEGA_NATIVE_FONT_SUPPORT)
#include "modules/display/pixelscalefont.h"
#endif // VEGA_NO_FONTMANAGER_CREATE && !VEGA_NATIVE_FONT_SUPPORT


/**
 * PixelScalePainter is a wrapper of VEGAOpPainter, used to
 * scale the viewport to match the size of render buffer size
 * 
 * The implementation of PixelScalePainter heavily depends
 * on the VEGAOpPainter.
 */
class PixelScalePainter: public VEGAOpPainter
{
protected:
	PixelScaler scaler;

public:
	PixelScalePainter() : m_max_rects(0), m_update_rects(NULL) {}
	virtual ~PixelScalePainter() { OP_DELETEA(m_update_rects); }

	/**
	 * Initialize the instance
	 * @param scale pixel scale value in percentage.
	 * @see VEGAOpPainter
	 */
	OP_STATUS Construct(unsigned int width, unsigned int height, VEGAWindow* window = NULL, int scale = 100)
	{
		scaler.SetScale(scale);

		return VEGAOpPainter::Construct(scaler.ToDevicePixel(width), scaler.ToDevicePixel(height), window);
	}

	/**
	 * Set scale
	 * @return the old scale value, -1 if the painter has not been initialized.
	 */
	int SetScale(int scale)
	{
		if (scale == scaler.GetScale())
			return scale;

		int oldscale = scaler.SetScale(scale);

		return oldscale;
	}

	/**
	 * Get scale
	 */
	const PixelScaler& GetScaler()
	{
		return scaler;
	}

	/**
	 * Wrapping OpPainter
	 */
	virtual OP_STATUS SetClipRect(const OpRect &rect)
	{
#ifdef CSS_TRANSFORMS
		if (HasTransform())
		{
			return VEGAOpPainter::SetClipRect(rect);
		}
#endif // CSS_TRANSFORMS

		return VEGAOpPainter::SetClipRect(scaler.ToDevicePixel(rect));
	}

	virtual void GetClipRect(OpRect* rect)
	{
		VEGAOpPainter::GetClipRect(rect);
		scaler.FromDevicePixel(rect);
	}

	virtual void ClearRect(const OpRect &rect)
	{
		VEGAOpPainter::ClearRect(scaler.ToDevicePixel(rect));
	}

	virtual void DrawString(const OpPoint &pos, const uni_char* text, UINT32 len, INT32 extra_char_spacing = 0, short word_width = -1)
	{
		if (!current_font)
			return;

#ifdef VEGA_NATIVE_FONT_SUPPORT
		VEGAOpFont* vegaopfont = static_cast<VEGAOpFont*>(current_font);
		int oldscale = vegaopfont->getVegaFont()->SetGlyphScale(scaler.GetScale());
#elif defined(VEGA_NO_FONTMANAGER_CREATE)
		PixelScaleFont* font = static_cast<PixelScaleFont*>(current_font);
		int oldscale =  font->SetGlyphScale(scaler.GetScale());
#endif // VEGA_NATIVE_FONT_SUPPORT

#ifdef CSS_TRANSFORMS
		if (HasTransform())
		{
			VEGAOpPainter::DrawString(pos, text, len,extra_char_spacing, word_width);
		}
		else
#endif // CSS_TRANSFORMS
		{
			VEGAOpPainter::DrawString(scaler.ToDevicePixel(pos), text, len,scaler.ToDevicePixel(extra_char_spacing),
									  word_width > 0 ? scaler.ToDevicePixel(word_width) : -1);
		}

#ifdef VEGA_NATIVE_FONT_SUPPORT
		vegaopfont->getVegaFont()->SetGlyphScale(oldscale);
#elif defined(VEGA_NO_FONTMANAGER_CREATE)
		font->SetGlyphScale(oldscale);
#endif // VEGA_NATIVE_FONT_SUPPORT
	}

	virtual void InvertRect(const OpRect &rect)
	{
#ifdef CSS_TRANSFORMS
		if (HasTransform())
		{
			VEGAOpPainter::InvertRect(rect);
			return;
		}
#endif // CSS_TRANSFORMS

		VEGAOpPainter::InvertRect(scaler.ToDevicePixel(rect));
	}

	virtual void InvertBorderRect(const OpRect &rect, int border)
	{
		VEGAOpPainter::InvertBorderRect(scaler.ToDevicePixel(rect), scaler.ToDevicePixel(border));
	}

	virtual void InvertBorderEllipse(const OpRect &rect, int border)
	{
		VEGAOpPainter::InvertBorderEllipse(scaler.ToDevicePixel(rect), scaler.ToDevicePixel(border));
	}

	virtual void InvertBorderPolygon(const OpPoint* points, int count, int border)
	{
		OP_ASSERT(points > 0);

		// The other invert methods grow towards the center but hard
		// to say what is the center of a generic polygon.
		VEGAPath p;
		const VEGA_FIX half = VEGA_INTTOFIX(1) / 2;
		OpPoint lofs = GetLayerOffset();

		RETURN_VOID_IF_ERROR(p.prepare(count+1));
		RETURN_VOID_IF_ERROR(p.moveTo(VEGA_INTTOFIX(scaler.ToDevicePixel(points[0].x)+lofs.x)+half,
									  VEGA_INTTOFIX(scaler.ToDevicePixel(points[0].y)+lofs.y)+half));
		for (int i = 1; i < count; ++i)
			RETURN_VOID_IF_ERROR(p.lineTo(VEGA_INTTOFIX(scaler.ToDevicePixel(points[i].x)+lofs.x)+half,
										  VEGA_INTTOFIX(scaler.ToDevicePixel(points[i].y)+lofs.y)+half));
		RETURN_VOID_IF_ERROR(p.close(true));

		VEGAPath stroked_p;
		p.setLineWidth(VEGA_INTTOFIX(scaler.ToDevicePixel(border)));
		RETURN_VOID_IF_ERROR(p.createOutline(&stroked_p, VEGA_INTTOFIX(1), 0));

#ifdef CSS_TRANSFORMS
		OP_ASSERT(!HasTransform());
#endif // CSS_TRANSFORMS

		RETURN_VOID_IF_ERROR(InvertShape(stroked_p));
	}

	virtual OP_STATUS DrawBitmapTiled(const OpBitmap* bitmap, const OpPoint& offset, const OpRect& dest, INT32 scale)
	{
		// tilewidth and tileheight should be in CSS pixels.
		int tileWidth = scaler.FromDevicePixel(bitmap->Width() * scale) / 100;
		int tileHeight = scaler.FromDevicePixel(bitmap->Height() * scale) / 100;

		return VEGAOpPainter::DrawBitmapTiled(bitmap, offset, dest, scale, tileWidth, tileHeight);
	}

	virtual OpBitmap* CreateBitmapFromBackground(const OpRect& rect)
	{
		return VEGAOpPainter::CreateBitmapFromBackground(scaler.ToDevicePixel(rect));
	}

	virtual void GetOffscreenBitmapRect(OpRect* rect)
	{
		VEGAOpPainter::GetOffscreenBitmapRect(rect);
		scaler.FromDevicePixel(rect);
	}

	virtual OP_STATUS BeginOpacity(const OpRect& rect, UINT8 opacity)
	{
		return VEGAOpPainter::BeginOpacity(scaler.ToDevicePixel(rect), opacity);
	}

	virtual OpFont* GetFont()
	{
		return VEGAOpPainter::GetFont();
	}

	/**
	 * Wrapping VEGAOpPainter
	 */
	virtual OP_STATUS Resize(unsigned int width, unsigned int height)
	{
		return VEGAOpPainter::Resize(scaler.ToDevicePixel(width), scaler.ToDevicePixel(height));
	}

	virtual void present(const OpRect* update_rects_src = NULL, unsigned int num_rects = 0)
	{
		if (!num_rects)
			return VEGAOpPainter::present(NULL, 0);

		if (num_rects > m_max_rects)
		{
			OP_DELETEA(m_update_rects);
			m_update_rects = OP_NEWA(OpRect, num_rects);

			// In case of OOM, simply do not paint the rects
			if (!m_update_rects)
			{
				m_max_rects = 0;
				return;
			}
			else
			{
				m_max_rects = num_rects;
			}
		}

		for (unsigned int i = 0; i < num_rects; ++i)
			m_update_rects[i] = scaler.ToDevicePixel(update_rects_src[i]);

		return VEGAOpPainter::present(m_update_rects, num_rects);
	}

	virtual OP_STATUS BeginStencil(const OpRect& rect)
	{
		return VEGAOpPainter::BeginStencil(scaler.ToDevicePixel(rect));
	}

	virtual void MoveRect(int x, int y, unsigned int w, unsigned int h, int dx, int dy)
	{
		VEGAOpPainter::MoveRect(scaler.ToDevicePixel(x), scaler.ToDevicePixel(y),
								scaler.ToDevicePixel(w), scaler.ToDevicePixel(h),
								scaler.ToDevicePixel(dx), scaler.ToDevicePixel(dy));
	}

	virtual void SetVegaTranslation(int x, int y)
	{
		VEGAOpPainter::SetVegaTranslation(scaler.ToDevicePixel(x), scaler.ToDevicePixel(y));
	}

protected:
	virtual OP_STATUS PaintRect(OpRect& rect)
	{
#if defined(CSS_TRANSFORMS)
		if (HasTransform())
		{
			return VEGAOpPainter::PaintRect(rect);
		}
#endif // CSS_TRANSFORMS

		return VEGAOpPainter::PaintRect(*(scaler.ToDevicePixel(&rect)));
	}

	virtual OP_STATUS PaintImage(class VEGAOpBitmap* vbmp, struct VEGADrawImageInfo& diinfo)
	{
#ifdef VEGA_LIMIT_BITMAP_SIZE
		if (vbmp->isTiled())
			return VEGAOpPainter::PaintImage(vbmp, diinfo);
#endif // VEGA_LIMIT_BITMAP_SIZE

#if defined(CSS_TRANSFORMS)
		if (HasTransform())
			return VEGAOpPainter::PaintImage(vbmp, diinfo);
#endif // CSS_TRANSFORMS

		int left = scaler.ToDevicePixel(diinfo.dstx);
		int top = scaler.ToDevicePixel(diinfo.dsty);
		int right = scaler.ToDevicePixel(diinfo.dstx + diinfo.dstw);
		int bottom = scaler.ToDevicePixel(diinfo.dsty + diinfo.dsth);
		diinfo.dstx = left;
		diinfo.dsty = top;
		diinfo.dstw = right - left;
		diinfo.dsth = bottom - top;
		diinfo.tile_offset_x = scaler.ToDevicePixel(diinfo.tile_offset_x);
		diinfo.tile_offset_y = scaler.ToDevicePixel(diinfo.tile_offset_y);
		diinfo.tile_width = scaler.ToDevicePixel(diinfo.tile_width);
		diinfo.tile_height = scaler.ToDevicePixel(diinfo.tile_height);

		if ((diinfo.dstw != diinfo.srcw || diinfo.dsth != diinfo.srch) &&
			diinfo.type != VEGADrawImageInfo::REPEAT)
		{
			diinfo.type = diinfo.SCALED;
		}

		return VEGAOpPainter::PaintImage(vbmp, diinfo);
	}

#ifdef CSS_TRANSFORMS
	virtual void GetClipRectTransform(VEGATransform& trans)
	{
		VEGA_FIX factor = VEGA_INTTOFIX(scaler.m_multiplier) / scaler.m_divider;
		trans.loadScale(factor, factor);
		trans.multiply(current_transform);
	}
#endif // CSS_TRANSFORMS

	virtual void GetCTM(VEGATransform& trans)
	{
		VEGATransform scale;
		VEGA_FIX factor = VEGA_INTTOFIX(scaler.m_multiplier) / scaler.m_divider;
		scale.loadScale(factor, factor);

#ifdef CSS_TRANSFORMS
		if (HasTransform())
			scale.multiply(current_transform);
#endif // CSS_TRANSFORMS

		OpPoint lofs = GetLayerOffset();
		trans.loadTranslate(VEGA_INTTOFIX(lofs.x), VEGA_INTTOFIX(lofs.y));
		trans.multiply(scale);
	}

protected:
	unsigned int m_max_rects;
	OpRect* m_update_rects;
};

#endif // VEGA_SUPPORT && VEGA_OPPAINTER_SUPPORT && PIXEL_SCALE_RENDERING_SUPPORT

#endif // PIXELSCALEPAINTER_H
