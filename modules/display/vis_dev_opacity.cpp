/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "modules/display/vis_dev.h"
#include "modules/dochand/win.h"

#ifdef OPERA_BIG_ENDIAN
#define ALPHA_OFS 0
#else
#define ALPHA_OFS 3
#endif

#ifdef VEGA_SUPPORT
#include "modules/libvega/vegarenderer.h"
#include "modules/libvega/vegafilter.h"
#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#endif
#endif

#ifdef SVG_SUPPORT
#include "modules/svg/SVGManager.h"
#endif // SVG_SUPPORT

#ifdef DISPLAY_SUPPORTS_BROKEN_ALPHA_PAINTER
#include "modules/display/alphabitmap_painter.h"
#endif // DISPLAY_SUPPORTS_BROKEN_ALPHA_PAINTER
// == VisualDeviceBackBuffer =======================================================

VisualDeviceBackBuffer::VisualDeviceBackBuffer()
	: bitmap(NULL)
	, use_painter_opacity(FALSE)
	, display_effect(DisplayEffect::EFFECT_TYPE_NONE, 0)
	, oom_fallback(FALSE)
{
}

VisualDeviceBackBuffer::~VisualDeviceBackBuffer()
{
	OP_DELETE(bitmap);
}

static inline OP_STATUS ClearBitmap(OpBitmap* bmp, UINT32 color, const OpRect& rect)
{
	OP_ASSERT(rect.x+rect.width <= (INT32)bmp->Width());
	OP_ASSERT(rect.y+rect.height <= (INT32)bmp->Height());

	if (bmp->Supports(OpBitmap::SUPPORTS_POINTER) && bmp->GetBpp()==32)
	{
		UINT32* data = (UINT32*)bmp->GetPointer();
		if (data)
		{
			UINT32 stride = bmp->GetBytesPerLine()/4;
			data += rect.y * stride + rect.x;
			for (INT32 y = 0; y < rect.height; ++y)
			{
				for (INT32 x = 0; x < rect.width; ++x)
					data[x] = color;
				data += stride;
			}
			bmp->ReleasePointer();
			return OpStatus::OK;
		}
	}
	UINT32* linedata = OP_NEWA(UINT32, bmp->Width());
	if (!linedata)
		return OpStatus::ERR_NO_MEMORY;

	if ((UINT32)rect.width == bmp->Width() && (UINT32)rect.height == bmp->Height())
	{
		for (UINT32 x = 0; x < bmp->Width(); ++x)
			linedata[x] = color;
		for (UINT32 y = 0; y < bmp->Height(); ++y)
			bmp->AddLine(linedata, y);
	}
	else
	{
		for (INT32 y = 0; y < rect.height; ++y)
		{
			if (bmp->GetLineData(linedata, rect.y + y))
			{
				for (INT32 x = 0; x < rect.width; ++x)
					linedata[rect.x + x] = color;
				bmp->AddLine(linedata, rect.y + y);
			}
		}
	}
	OP_DELETEA(linedata);
	return OpStatus::OK;
}

OP_STATUS VisualDeviceBackBuffer::InitBitmap(const OpRect& rect, OpPainter* painter, int opacity, BOOL replace_color, BOOL copy_background)
{
	INT32 current_width = 0;
	INT32 current_height = 0;

	if (bitmap)
	{
		current_width = bitmap->Width();
		current_height = bitmap->Height();

		if (current_width < rect.width || current_height < rect.height
			|| (!replace_color && copy_background)) // TODO: special case that currently is not supported in a cached back buffer
		{
			OP_DELETE(bitmap);
			bitmap = NULL;
		}
	}

	has_background = FALSE;
	if (!bitmap)
	{
		if (!replace_color && copy_background)
			bitmap = painter->CreateBitmapFromBackground(rect);
		has_background = !!bitmap;

		INT32 new_width = MAX(current_width, rect.width);
		INT32 new_height = MAX(current_height, rect.height);

		OpAutoPtr<OpBitmap> bg_bitmap;
		if (bitmap && !bitmap->Supports(OpBitmap::SUPPORTS_PAINTER))
		{
			bg_bitmap.reset(bitmap);
			bitmap = NULL;
		}
		if (!bitmap)
			RETURN_IF_ERROR(OpBitmap::Create(&bitmap, new_width, new_height, FALSE, TRUE, 0, 0, TRUE));
		if (bg_bitmap.get())
		{
			OpRect source(rect);
			source.x = 0;
			source.y = 0;
			/* According to the documentation of SetColor(), color
			 * must be non-NULL when all_transparent is TRUE.  I
			 * suspect that is a bug in the documentation, but I'll do
			 * as it says anyway.
			 */
			UINT8 dummy_color[4] = {0, 0, 0, 0};
			if (!bitmap->SetColor(dummy_color, TRUE, NULL))
				return OpStatus::ERR;
			OpPainter * painter = bitmap->GetPainter();
			if (painter->Supports(OpPainter::SUPPORTS_ALPHA))
				painter->DrawBitmapClippedAlpha(bg_bitmap.get(), source, OpPoint(0, 0));
			else
				painter->DrawBitmapClipped(bg_bitmap.get(), source, OpPoint(0, 0));
		}
	}

	bitmap_rect.x = 0;
	bitmap_rect.y = 0;
	bitmap_rect.width = rect.width;
	bitmap_rect.height = rect.height;

	if (OpPainter *bitmap_painter = bitmap->GetPainter())
	{
		bitmap_painter->SetRenderingHint(painter->GetRenderingHint());
		bitmap->ReleasePainter();
	}

	// When using vega oppainter the filters and the painter is always in the same color format 
#ifndef VEGA_OPPAINTER_SUPPORT
	// We must have a 32 bit bitmap since we assume it's 32 bit in ApplyEffectInternal and EndBackbuffer. Anything else will crash!
	// Ideally, we should have a BOOL must_be_32bit in OpBitmap::Create (or a prettier API design)
	OP_ASSERT(bitmap->GetBpp() == 32);
	if (bitmap->GetBpp() != 32)
		return OpStatus::ERR;
#endif // !VEGA_OPPAINTER_SUPPORT

	if (replace_color)
	{
		UINT32 color = REALLY_UGLY_COLOR;
		if (!bitmap->SetColor((UINT8*)&color, TRUE, &bitmap_rect))
			RETURN_IF_ERROR(ClearBitmap(bitmap, color, bitmap_rect));
		bitmap->ForceAlpha();
	}
	else if (!has_background)
	{
		if (!bitmap->SetColor(NULL, TRUE, &bitmap_rect))
			RETURN_IF_ERROR(ClearBitmap(bitmap, 0, bitmap_rect));
	}

	this->opacity = opacity;
	this->replace_color = replace_color;

	return OpStatus::OK;
}

// == VisualDevice =======================================================

OP_STATUS VisualDevice::BeginOpacity(const OpRect& rect, UINT8 opacity)
{
	VisualDeviceBackBuffer* bb;
	return BeginBackbuffer(rect, opacity, TRUE, bb);
}

void VisualDevice::EndOpacity()
{
	EndBackbuffer(TRUE);
}

OP_STATUS VisualDevice::BeginBackbuffer(const OpRect& rect, UINT8 opacity, BOOL clip, BOOL copy_background, VisualDeviceBackBuffer*& bb, int clip_rect_inset)
{
	if (!painter)
		return OpStatus::ERR;

#ifdef VEGA_SUPPORT
	if (g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_HW3D)
	{
		if (!m_cachedBB)
			m_cachedBB = OP_NEW(VisualDeviceBackBuffer, ());

		if (backbuffers.HasLink(m_cachedBB))
			bb = OP_NEW(VisualDeviceBackBuffer, ());
		else
		{
			bb = m_cachedBB;
			bb->oom_fallback = FALSE;
		}
	}
	else
#endif // VEGA_SUPPORT
		bb = OP_NEW(VisualDeviceBackBuffer, ());

	if (!bb)
		return OpStatus::ERR;

	bb->opacity = opacity; // set directly - needed for OOM fallback

	// Cover area but keep the hole! We need to flush it immediately.
	//Outside parts of the background might be flushed from FlushLast, so we can't do this. (Bug 249498)
	//CoverBackground(rect, TRUE, TRUE);
	FlushBackgrounds(rect);
	bb->doc_rect = rect;

	OpRect doc_rect = ToBBox(rect);

	// Get rid of "infinite" values from the input. 'clip_rect_inset'
	// compensates for blurring at the edge of the visible area.
	if (clip)
		doc_rect.SafeIntersectWith(doc_display_rect.InsetBy(clip_rect_inset));

	OpRect screen_rect;

#ifdef CSS_TRANSFORMS
	if (offset_transform_is_set)
	{
		screen_rect = offset_transform.GetTransformedBBox(doc_rect);
	}
	else
#endif // CSS_TRANSFORMS
	{
		screen_rect = OffsetToContainerAndScroll(ScaleToScreen(doc_rect));
	}

#ifdef CSS_TRANSFORMS
	BOOL use_clipping = !HasTransform();
#else
	const BOOL use_clipping = TRUE;
#endif // CSS_TRANSFORMS

	if (use_clipping && OpStatus::IsError(bg_cs.PushClipping(doc_rect)))
	{
		if (bb != m_cachedBB)
			OP_DELETE(bb);
		return OpStatus::ERR_NO_MEMORY;
	}

	if (clip)
	{
		// Clip screen_rect to current clipping so we don't allocate unnecessary data.
		OpRect cliprect;
		painter->GetClipRect(&cliprect);
		cliprect = cliprect.InsetBy(clip_rect_inset);
		screen_rect.SafeIntersectWith(cliprect);
	}

	if (screen_rect.IsEmpty())
	{
		if (bb != m_cachedBB)
			OP_DELETE(bb);
		if (use_clipping)
			bg_cs.PopClipping();
		return OpStatus::ERR;
	}

	// ASSUMPTION: sides of screen_rect will never exceed maximum
	// allowed bitmap dimensions (eg
	// VEGARendererBackend::maxBitmapSide() when using VEGAOpPainter).
	// VisualDevice::ConstrainClipRects can help conform to this
	// restriction, and is used from most places. Note that it is
	// currently not possible to fix calls from traverse.

	if (opacity != 255 && OpStatus::IsSuccess(painter->BeginOpacity(screen_rect, opacity)))
	{
		bb->use_painter_opacity = TRUE;

		// reusing old painter - reset pre-alpha
		painter->SetPreAlpha(255);

		// Painter supports opacity.
		bb->Into(&backbuffers);
		return OpStatus::OK;
	}

	OpStatus::Ignore(CheckFont());

	BOOL replace_color = FALSE;
#ifdef GADGET_SUPPORT
#ifdef REALLY_UGLY_COLOR_HACK
	if (GetWindow() && GetWindow()->GetType() == WIN_TYPE_GADGET)
	{
		if (backbuffers.First())
		{
			if (bb != m_cachedBB)
				OP_DELETE(bb);
			bg_cs.PopClipping();
			return OpStatus::ERR;
		}
		replace_color = TRUE;		
	}
#endif
#endif // GADGET_SUPPORT

	if (OpStatus::IsError(bb->InitBitmap(screen_rect, painter, opacity, replace_color, copy_background)))
	{
		// HACK fix for when creating bitmap (or fetching its painter
		// - see below) fails. set pre-alpha value in painter and let
		// it deal as best it can. not perfect, but works reasonably
		// well.
		if (opacity != 255)
		{
			bb->Into(&backbuffers);
			bb->oom_fallback = TRUE;
			painter->SetPreAlpha(PreMulBBAlpha());
			return OpStatus::OK;
		}

		if (bb != m_cachedBB)
			OP_DELETE(bb);
		bg_cs.PopClipping();
		return OpStatus::ERR;
	}

	bb->rect = OpRect(screen_rect.x - offset_x, screen_rect.y - offset_y, screen_rect.width, screen_rect.height);
	bb->old_painter = painter;

#ifdef DISPLAY_SUPPORTS_BROKEN_ALPHA_PAINTER
	// Only use the broken alpha painter when not using opacity.
	// When opacity is used the platform will have to handle it, 
	// or SetOpacity is called on the bitmap which removes all 
	// alpha data anyway
	if (bb->opacity == 255)
	{
		AlphaBitmapPainter* bmppainter = OP_NEW(AlphaBitmapPainter, ());
		OP_STATUS status = OpStatus::ERR_NO_MEMORY;
		if (bmppainter)
			status = bmppainter->Construct(bb->bitmap, copy_background);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(bmppainter);
			if (bb != m_cachedBB)
				OP_DELETE(bb);
			bg_cs.PopClipping();
			return status;
		}
		painter = bmppainter;
	}
	else
#endif // !DISPLAY_SUPPORTS_BROKEN_ALPHA_PAINTER
	{
		OpPainter *bitmap_painter = bb->bitmap->GetPainter();
		if (!bitmap_painter)
		{
			// HACK fix for when fetching painter fails (see
			// above). set pre-alpha value in painter and let it deal
			// as best it can. not perfect, but works reasonably well.
			OP_DELETE(bb->bitmap);
			bb->bitmap = 0;
			bb->rect.Set(0,0,0,0);
			bb->old_painter = 0;

			bb->Into(&backbuffers);
			bb->oom_fallback = TRUE;
			painter->SetPreAlpha(PreMulBBAlpha());
			return OpStatus::OK;
		}
		painter = bitmap_painter;
	}

	painter->SetColor(bb->old_painter->GetColor());
	painter->SetFont(currentFont);

	// Translate offset so painting is correct relative to this buffer.
	bb->ofs_diff_x = -bb->rect.x - offset_x;
	bb->ofs_diff_y = -bb->rect.y - offset_y;
	offset_x_ex += bb->ofs_diff_x;
	offset_y_ex += bb->ofs_diff_y;
	offset_x += bb->ofs_diff_x;
	offset_y += bb->ofs_diff_y;

#ifdef CSS_TRANSFORMS
	if (HasTransform())
	{
		if (offset_transform_is_set)
		{
			AffineTransform ofs_diff;
			ofs_diff.LoadTranslate(float(bb->ofs_diff_x), float(bb->ofs_diff_y));
			ofs_diff.PostMultiply(offset_transform);

			offset_transform = ofs_diff;
		}

		// If we are in transformed mode, the transform has to be
		// 'inherited' by the backbuffer, we also need to calculate a
		// new base transform (due to the offset change).
		vd_screen_ctm = GetBaseTransform();

		UpdatePainterTransform(*GetCurrentTransform());
	}
#endif // CSS_TRANSFORMS

	bb->Into(&backbuffers);

	return OpStatus::OK;
}

void VisualDevice::EndBackbuffer(BOOL paint)
{
	VisualDeviceBackBuffer* bb = (VisualDeviceBackBuffer*) backbuffers.Last();
	OP_ASSERT(bb);
	if (!bb)
		return;
	bb->Out();

	painter->SetPreAlpha(PreMulBBAlpha());

	FlushBackgrounds(bb->doc_rect);

	if (bb->oom_fallback)
	{
		// OP_ASSERT(painter->GetPreAlpha() != 255);
		OP_ASSERT(!bb->bitmap);
		if (bb != m_cachedBB)
			OP_DELETE(bb);
		bg_cs.PopClipping();
		return;
	}

	if (bb->use_painter_opacity)
	{
		// Painter support for opacity was used.
		painter->EndOpacity();
		bb->use_painter_opacity = FALSE;
		if (bb != m_cachedBB)
			OP_DELETE(bb);
		bg_cs.PopClipping();
		return;
	}

#ifdef VEGA_SUPPORT
	// Apply effect on bitmap
	ApplyEffectInternal(bb);
#endif

#ifdef CSS_TRANSFORMS
	if (HasTransform())
	{
		// As the bitmap can be reused in m_cachedBB we need to clear
		// the current transform on the bitmap painter
		VEGAOpPainter* vpainter = static_cast<VEGAOpPainter*>(bb->bitmap->GetPainter());
		vpainter->ClearTransform();
		bb->bitmap->ReleasePainter();
	}
#endif // CSS_TRANSFORMS

	bb->old_painter->SetFont(currentFont);
	bb->old_painter->SetColor(painter->GetColor());
#ifdef DISPLAY_SUPPORTS_BROKEN_ALPHA_PAINTER
	if (bb->opacity == 255)
	{
		OP_DELETE(painter);
	}
	else
#endif // DISPLAY_SUPPORTS_BROKEN_ALPHA_PAINTER
	{
		bb->bitmap->ReleasePainter();
	}

	painter = bb->old_painter;

	offset_x -= bb->ofs_diff_x;
	offset_y -= bb->ofs_diff_y;
	offset_x_ex -= bb->ofs_diff_x;
	offset_y_ex -= bb->ofs_diff_y;

#ifdef CSS_TRANSFORMS
	if (HasTransform())
	{
		if (offset_transform_is_set)
		{
			AffineTransform ofs_diff;
			ofs_diff.LoadTranslate(float(-bb->ofs_diff_x), float(-bb->ofs_diff_y));
			ofs_diff.PostMultiply(offset_transform);

			offset_transform = ofs_diff;
		}

		// Restore base transform
		vd_screen_ctm = GetBaseTransform();
	}
#endif // CSS_TRANSFORMS

	// Apply opacity on bitmap
	if (bb->opacity != 255)
	{
		OpRect dst = OffsetToContainer(bb->rect);
		if (painter->DrawBitmapClippedOpacity(bb->bitmap, bb->bitmap_rect, OpPoint(dst.x, dst.y), bb->opacity))
			paint = FALSE; // Painting done
		else if (!bb->bitmap->SetOpacity(bb->opacity))
		{
			int opacity = bb->opacity;
#ifdef GADGET_SUPPORT
	        if (GetWindow() && GetWindow()->GetType() == WIN_TYPE_GADGET)
			{
		        bb->has_background = FALSE;
#ifdef REALLY_UGLY_COLOR_HACK
				// Nested opacitybuffers does not work with uglycolorhack. We could calculate the correct opacity here
				// but since parent opacitybuffer also use uglycolorhack it will be ignored later anyway.
//				VisualDeviceBackBuffer* parent_bb = (VisualDeviceBackBuffer*) backbuffers.Last();
//				if (parent_bb && bb->replace_color && parent_bb->replace_color)
//				{
//					opacity = opacity * parent_bb->opacity / 255;
//				}
#endif // REALLY_UGLY_COLOR_HACK
			}
#endif // GADGET_SUPPORT

			BOOL supportsPointer = 
				bb->bitmap->Supports(OpBitmap::SUPPORTS_POINTER);
			
			OpRect *effect = &bb->bitmap_rect;
			UINT32* data = NULL;
			if(supportsPointer)
				data = (UINT32*) bb->bitmap->GetPointer();

#ifdef REALLY_UGLY_COLOR_HACK
			if (bb->replace_color)
			{
				if(supportsPointer && data)
				{
					UINT32 stride = bb->bitmap->GetBytesPerLine()/4;
					data += effect->y * stride + effect->x;
					for (INT32 y = 0; y < effect->height; ++y)
					{
						for (INT32 x = 0; x < effect->width; ++x)
						{
							UINT32 color = data[x];
							if (color == REALLY_UGLY_COLOR)
								color = 0;
							else
							{
								/* If we get into this part of the
								 * code, then the back buffer was
								 * originally a copy of the
								 * background.  And at that point, it
								 * was fully opaque.  No rendering
								 * since then can have made any pixels
								 * less opaque.  So we are guaranteed
								 * that all pixels have alpha==255.
								 */
#ifdef USE_PREMULTIPLIED_ALPHA
#ifndef OPERA_BIG_ENDIAN
								((UINT8*)&color)[0] = (((UINT8*)&color)[0]*opacity)/255;
#endif
								((UINT8*)&color)[1] = (((UINT8*)&color)[1]*opacity)/255;
								((UINT8*)&color)[2] = (((UINT8*)&color)[2]*opacity)/255;
#ifdef OPERA_BIG_ENDIAN
								((UINT8*)&color)[3] = (((UINT8*)&color)[3]*opacity)/255;
#endif
#endif
								((UINT8*)&color)[ALPHA_OFS] = opacity;
							}

							data[x] = color;
						}
						data += stride;
					}
				}
				else
				{
					UINT32* line = OP_NEWA(UINT32, bb->bitmap->Width());
					if(line)
					{
						for(int i = effect->y; i < effect->y + effect->height; i++)
						{
							if(bb->bitmap->GetLineData(line, i))
							{
								for(int j = effect->x; j < effect->x + effect->width; j++)
								{
									if(line[j] == REALLY_UGLY_COLOR)
									{
#ifdef USE_PREMULTIPLIED_ALPHA
										line[j] = 0;
#else
										((UINT8*)&line[j])[ALPHA_OFS] = 0;
#endif
									}
									else
									{
										/* If we get into this part of
										 * the code, then the back
										 * buffer was originally a
										 * copy of the background.
										 * And at that point, it was
										 * fully opaque.  No rendering
										 * since then can have made
										 * any pixels less opaque.  So
										 * we are guaranteed that all
										 * pixels have alpha==255.
										 */
#ifdef USE_PREMULTIPLIED_ALPHA
#ifndef OPERA_BIG_ENDIAN
										((UINT8*)&line[j])[0] = (((UINT8*)&line[j])[0]*opacity)/255;
#endif
										((UINT8*)&line[j])[1] = (((UINT8*)&line[j])[1]*opacity)/255;
										((UINT8*)&line[j])[2] = (((UINT8*)&line[j])[2]*opacity)/255;
#ifdef OPERA_BIG_ENDIAN
										((UINT8*)&line[j])[3] = (((UINT8*)&line[j])[3]*opacity)/255;
#endif
#endif
										((UINT8*)&line[j])[ALPHA_OFS] = opacity;
									}
								}

								if(OpStatus::IsError(bb->bitmap->AddLine(line, i)))
								{
									break;
								}
							}
						}
						OP_DELETEA(line);
					}
				}
			}
			else
#endif // REALLY_UGLY_COLOR_HACK
			{
				if(supportsPointer && data)
				{
					UINT32 stride = bb->bitmap->GetBytesPerLine()/4;
					data += effect->y * stride + effect->x;
					for (INT32 y = 0; y < effect->height; ++y)
					{
						for (INT32 x = 0; x < effect->width; ++x)
						{
							UINT32 color = data[x];
							if (((UINT8*)&color)[ALPHA_OFS] || bb->has_background)
							{
								/* If we get into this part of the
								 * code, then the back buffer was
								 * originally a copy of the
								 * background.  And at that point, it
								 * was fully opaque.  No rendering
								 * since then can have made any pixels
								 * less opaque.  So we are guaranteed
								 * that all pixels have alpha==255.
								 */
#ifdef USE_PREMULTIPLIED_ALPHA
#ifndef OPERA_BIG_ENDIAN
								((UINT8*)&color)[0] = (((UINT8*)&color)[0]*opacity)/255;
#endif
								((UINT8*)&color)[1] = (((UINT8*)&color)[1]*opacity)/255;
								((UINT8*)&color)[2] = (((UINT8*)&color)[2]*opacity)/255;
#ifdef OPERA_BIG_ENDIAN
								((UINT8*)&color)[3] = (((UINT8*)&color)[3]*opacity)/255;
#endif
#endif
								((UINT8*)&color)[ALPHA_OFS] = opacity;

								data[x] = color;
							}
						}
						data += stride;
					}
				}
				else
				{
					UINT32* line = OP_NEWA(UINT32, bb->bitmap->Width());
					if(line)
					{
						for(int i = effect->y; i < effect->y + effect->height; i++)
						{
							if(bb->bitmap->GetLineData(line, i))
							{
								for(int j = effect->x; j < effect->x + effect->width; j++)
								{
									if (((UINT8*)&line[j])[ALPHA_OFS] || bb->has_background)
									{
										/* If we get into this part of
										 * the code, then the back
										 * buffer was originally a
										 * copy of the background.
										 * And at that point, it was
										 * fully opaque.  No rendering
										 * since then can have made
										 * any pixels less opaque.  So
										 * we are guaranteed that all
										 * pixels have alpha==255.
										 */
#ifdef USE_PREMULTIPLIED_ALPHA
#ifndef OPERA_BIG_ENDIAN
										((UINT8*)&line[j])[0] = (((UINT8*)&line[j])[0]*opacity)/255;
#endif
										((UINT8*)&line[j])[1] = (((UINT8*)&line[j])[1]*opacity)/255;
										((UINT8*)&line[j])[2] = (((UINT8*)&line[j])[2]*opacity)/255;
#ifdef OPERA_BIG_ENDIAN
										((UINT8*)&line[j])[3] = (((UINT8*)&line[j])[3]*opacity)/255;
#endif
#endif
										((UINT8*)&line[j])[ALPHA_OFS] = opacity;
									}
								}

								if(OpStatus::IsError(bb->bitmap->AddLine(line, i)))
								{
									break;
								}
							}
						}
						OP_DELETEA(line);
					}
				}
			}

			if(supportsPointer)
				bb->bitmap->ReleasePointer();
		}
	}

	if (paint)
	{
		if (bb->has_background)
			bb->bitmap->ForceAlpha();

#ifdef CSS_TRANSFORMS
		// Temporarily set and reset the painter transform - if any -
		// since bb->rect is in screen-coordinates, and should not be
		// affected by the transform.
		VDCTMState ctm_state = SaveCTM();
#endif // CSS_TRANSFORMS

		BlitImage(bb->bitmap, bb->bitmap_rect, bb->rect);

#ifdef CSS_TRANSFORMS
		RestoreCTM(ctm_state);
#endif // CSS_TRANSFORMS
	}

	// Debug
	//SetColor(OP_RGB(255, 0, 255));
	//painter->DrawRect(OffsetToContainer(bb->rect));

	if (bb != m_cachedBB)
		OP_DELETE(bb);
	bg_cs.PopClipping();
}

OP_STATUS VisualDevice::BeginEffect(const OpRect& rect, const DisplayEffect& display_effect)
{
#ifdef VEGA_SUPPORT
	if (display_effect.type != DisplayEffect::EFFECT_TYPE_BLUR && display_effect.type != DisplayEffect::EFFECT_TYPE_SVG_FILTER)
		return OpStatus::ERR;

	OpRect extended_rect(rect);
	DisplayEffect local_de = display_effect;
	if(display_effect.type == DisplayEffect::EFFECT_TYPE_BLUR)
	{
		OP_ASSERT(local_de.radius > 0);
		// Limit radius
		local_de.radius = LimitBlurRadius(local_de.radius);

		// Extend the rectangle with blur radius so it isn't cut of in the edges.
		extended_rect = extended_rect.InsetBy(-local_de.radius, -local_de.radius);

		// Scale the radius
		local_de.radius = ScaleToScreen(local_de.radius);
	}

	VisualDeviceBackBuffer* bb;
	RETURN_IF_ERROR(BeginBackbuffer(extended_rect, 255, TRUE, FALSE, bb, -local_de.radius));

	bb->display_effect = local_de;

	return OpStatus::OK;
#else
	return OpStatus::ERR;
#endif
}

void VisualDevice::ApplyEffectInternal(VisualDeviceBackBuffer *bb)
{
#ifdef VEGA_SUPPORT
	if (bb->display_effect.type != DisplayEffect::EFFECT_TYPE_BLUR && bb->display_effect.type != DisplayEffect::EFFECT_TYPE_SVG_FILTER)
		return;

	OpBitmap *bitmap = bb->GetBitmap();

	if(bb->display_effect.type == DisplayEffect::EFFECT_TYPE_BLUR)
	{
		VEGA_FIX blur = VEGA_INTTOFIX(bb->display_effect.radius);
		blur /= 2; ///< Radius to gauss parameter

		VEGARenderer *renderer;
		VEGARenderTarget* rt;
#ifdef VEGA_OPPAINTER_SUPPORT
		VEGAOpPainter *painter = (VEGAOpPainter *) bitmap->GetPainter();
		renderer = painter->GetRenderer();
		rt = painter->GetRenderTarget();
#else
		VEGARenderer rend;
		RETURN_VOID_IF_ERROR(rend.Init(bitmap->Width(), bitmap->Height()));
		RETURN_VOID_IF_ERROR(rend.createBitmapRenderTarget(&rt, bitmap));
		rend.setRenderTarget(rt);
		renderer = &rend;
#endif

		VEGAFilter* filter = NULL;
		if (OpStatus::IsSuccess(renderer->createGaussianFilter(&filter, blur, blur, false)))
		{
			filter->setSource(rt);

			VEGAFilterRegion region;
			region.sx = region.dx = bb->bitmap_rect.x;
			region.sy = region.dy = bb->bitmap_rect.y;
			region.width = bb->bitmap_rect.width;
			region.height = bb->bitmap_rect.height;

			renderer->applyFilter(filter, region);
		}

#ifdef VEGA_OPPAINTER_SUPPORT
		bitmap->ReleasePainter();
#else
		// FIXME: do I need to flush the bitmap? (API is not yet final)
		rend.setRenderTarget(NULL);
		VEGARenderTarget::Destroy(rt);
#endif
		OP_DELETE(filter);
	}
#endif
}

void VisualDevice::EndEffect()
{
#ifdef VEGA_SUPPORT
	EndBackbuffer(TRUE);
#endif
}

#ifdef VEGA_OPPAINTER_SUPPORT

OP_STATUS VisualDevice::BeginStencil(const OpRect& rect)
{
	// Must flush before BeginStencil
	FlushBackgrounds(rect);

	OpRect doc_rect = ToBBox(rect);
	OpRect screen_rect = OffsetToContainerAndScroll(ScaleToScreen(doc_rect));

	if (OpStatus::IsError(bg_cs.PushClipping(doc_rect)))
		return OpStatus::ERR_NO_MEMORY;

	// Clip screen_rect to current clipping so we don't allocate unnecessary data.
	OpRect cliprect;
	painter->GetClipRect(&cliprect);
	screen_rect.SafeIntersectWith(cliprect);

	if (screen_rect.IsEmpty())
	{
		bg_cs.PopClipping();
		return OpStatus::ERR;
	}

	VEGAOpPainter *vp = (VEGAOpPainter *) painter;
	OP_STATUS status = vp->BeginStencil(screen_rect);
	if (OpStatus::IsError(status))
		bg_cs.PopClipping();

	return status;
}

void VisualDevice::BeginModifyingStencil()
{
	VEGAOpPainter *vp = (VEGAOpPainter *) painter;
	vp->BeginModifyingStencil();
}

void VisualDevice::EndModifyingStencil()
{
	VEGAOpPainter *vp = (VEGAOpPainter *) painter;
	vp->EndModifyingStencil();
}

void VisualDevice::EndStencil()
{
	VEGAOpPainter *vp = (VEGAOpPainter *) painter;
	vp->EndStencil();
	bg_cs.PopClipping();
}

#endif // VEGA_OPPAINTER_SUPPORT
