/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(VEGA_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT)

#include "modules/libvega/src/oppainter/vegaoppainter.h"
#include "modules/libvega/src/oppainter/vegaopfont.h"
#include "modules/libvega/src/vegaimage.h"
#include "modules/libvega/vegarenderer.h"
#include "modules/libvega/vegarendertarget.h"
#include "modules/libvega/vegapath.h"
#include "modules/libvega/vegastencil.h"
#include "modules/libvega/src/oppainter/vegaopbitmap.h"
#include "modules/mdefont/processedstring.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"

#ifdef VEGA_OPPAINTER_ANIMATIONS
#include "modules/pi/OpSystemInfo.h"
#include "modules/hardcore/mh/mh.h"
#endif // VEGA_OPPAINTER_ANIMATIONS
#ifdef VEGA_PRINTER_LISTENER_SUPPORT
#include "modules/libvega/src/oppainter/vegaprinterlistener.h"
#endif // VEGA_PRINTER_LISTENER_SUPPORT

#define PRE_MUL(a) if (m_pre_alpha != 255)	(a) = (a) == 255 ? m_pre_alpha : static_cast<UINT8>((static_cast<UINT32>(m_pre_alpha + 1) * static_cast<UINT32>(a)) >> 8)

VEGAOpPainter::VEGAOpPainter() : m_renderer(NULL), m_renderTarget(NULL), 
#ifdef VEGA_OPPAINTER_OPACITY_CACHE
	m_opacityRenderTargetCache(NULL),
#endif // VEGA_OPPAINTER_OPACITY_CACHE
	m_stencilCache(NULL), m_invertFilter(NULL), clipStack(NULL), 
	opacityStack(NULL), stencilStack(NULL), m_modifying_stencil(0),
	translateX(0), translateY(0), m_image_opacity(255),
	m_image_quality(VEGAFill::QUALITY_LINEAR),
	filltransform_is_set(false)
#ifdef CSS_TRANSFORMS
	, current_transform_is_set(false)
#endif // CSS_TRANSFORMS
#ifdef VEGA_OPPAINTER_ANIMATIONS
	, m_animationRenderTarget(NULL), m_animationPreviousRenderTarget(NULL),
	m_isAnimating(FALSE), m_animationStart(0)
#endif // VEGA_OPPAINTER_ANIMATIONS
#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	, m_printerListener(NULL)
#endif // VEGA_PRINTER_LISTENER_SUPPORT
	, m_pre_alpha(255)
{
	fillstate.color = 0;
	fillstate.fill = NULL;

	current_font = NULL;
}

VEGAOpPainter::~VEGAOpPainter()
{
#ifdef VEGA_OPPAINTER_ANIMATIONS
	if (g_main_message_handler)
		g_main_message_handler->UnsetCallBack(this, MSG_VEGA_ANIMATION_UPDATE_SCREEN, (MH_PARAM_1)this);
#endif // VEGA_OPPAINTER_ANIMATIONS
	OP_DELETE(m_invertFilter);
	while (clipStack)
		RemoveClipRect();
	while (opacityStack)
		EndOpacity();
	while (stencilStack)
		EndStencil();
	if (m_renderer)
		m_renderer->setRenderTarget(NULL);
	VEGARenderTarget::Destroy(m_renderTarget);
#ifdef VEGA_OPPAINTER_OPACITY_CACHE
	VEGARenderTarget::Destroy(m_opacityRenderTargetCache);
#endif // VEGA_OPPAINTER_OPACITY_CACHE
	VEGARenderTarget::Destroy(m_stencilCache);
	OP_DELETE(m_renderer);
#ifdef VEGA_OPPAINTER_ANIMATIONS
	VEGARenderTarget::Destroy(m_animationRenderTarget);
	VEGARenderTarget::Destroy(m_animationPreviousRenderTarget);
#endif // VEGA_OPPAINTER_ANIMATIONS
}

OP_STATUS VEGAOpPainter::Construct(unsigned int width, unsigned int height, VEGAWindow* window)
{
#ifdef VEGA_OPPAINTER_ANIMATIONS
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_VEGA_ANIMATION_UPDATE_SCREEN, (MH_PARAM_1)this));
#endif // VEGA_OPPAINTER_ANIMATIONS
	m_renderer = OP_NEW(VEGARenderer, ());
	if (!m_renderer)
		return OpStatus::ERR_NO_MEMORY;

	g_vegaGlobals.mainNativeWindow = window;
	RETURN_IF_ERROR(m_renderer->Init(width, height));

	if (window)
	{
		RETURN_IF_ERROR(m_renderer->createWindowRenderTarget(&m_renderTarget, window));
	}
	else
	{
		RETURN_IF_ERROR(m_renderer->createIntermediateRenderTarget(&m_renderTarget, width, height));
	}

	m_renderer->setRenderTarget(m_renderTarget);
	return OpStatus::OK;
}

OP_STATUS VEGAOpPainter::Resize(unsigned int width, unsigned int height)
{
	RETURN_IF_ERROR(m_renderer->Init(width, height));
	if (m_renderTarget->getWidth() != width || m_renderTarget->getHeight() != height)
	{
		// FIXME: resize the window too
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}
	m_renderer->setRenderTarget(m_renderTarget);

	// FIXME: transparency layers..

#ifdef VEGA_OPPAINTER_ANIMATIONS
	if (m_animationRenderTarget &&
		(m_animationRenderTarget->getWidth() != width || m_animationRenderTarget->getHeight() != height))
	{
		VEGARenderTarget::Destroy(m_animationRenderTarget);
		m_animationRenderTarget = NULL;
		VEGARenderTarget::Destroy(m_animationPreviousRenderTarget);
		m_animationPreviousRenderTarget = NULL;
	}
#endif // VEGA_OPPAINTER_ANIMATIONS
	return OpStatus::OK;
}

void VEGAOpPainter::present(const OpRect* update_rects, unsigned int num_rects)
{
#ifdef VEGA_OPPAINTER_ANIMATIONS
	if (m_isAnimating)
	{
		m_renderer->setRenderTarget(m_renderTarget);

		VEGAFill* img;
		// Non animated part
		if (OpStatus::IsSuccess(m_animationRenderTarget->getImage(&img)))
		{
			VEGATransform trans;
			trans.loadIdentity();
			img->setTransform(trans, trans);
			m_renderer->setFill(img);
			m_renderer->fillRect(0, 0, m_renderTarget->getWidth(), m_renderTarget->getHeight(), GetCurrentStencil());
		}
		m_renderer->setClipRect(animatedArea.x, animatedArea.y, animatedArea.width, animatedArea.height);

		if (m_animationStart)
		{
			double animtime = g_op_time_info->GetWallClockMS();
			animtime -= m_animationStart;
			static const double animation_duration = 200.;

			if (animtime >= animation_duration)
			{
				m_isAnimating = FALSE;
			}
			else
			{
				VEGATransform trans, itrans;
				double t = animtime / animation_duration; // Normalized time

				OP_STATUS gi_stat = m_animationPreviousRenderTarget->getImage(&img);

				// Fade
				if (animationType == VEGA_ANIMATION_FADE)
				{
					if (OpStatus::IsSuccess(gi_stat))
					{
						trans.loadIdentity();
						img->setTransform(trans, trans);
						img->setFillOpacity((unsigned char)(255*(1.0-t)));
						m_renderer->setFill(img);
						m_renderer->fillRect(animatedArea.x, animatedArea.y,
											 animatedArea.width, animatedArea.height,
											 GetCurrentStencil());
					}
				}
				// Slide
				else if (animationType == VEGA_ANIMATION_SLIDE_RIGHT)
				{
					int xpos = (int)(animatedArea.width*(1.0-t));
					if (OpStatus::IsSuccess(gi_stat))
					{
						trans.loadTranslate(VEGA_INTTOFIX((int)animatedArea.width-xpos), 0);
						itrans.loadTranslate(VEGA_INTTOFIX(xpos-(int)animatedArea.width), 0);
						img->setTransform(trans, itrans);
						img->setFillOpacity(255);
						m_renderer->setFill(img);
						m_renderer->fillRect(animatedArea.x+animatedArea.width-xpos, animatedArea.y,
											 animatedArea.width, animatedArea.height,
											 GetCurrentStencil());
					}
					if (OpStatus::IsSuccess(m_animationRenderTarget->getImage(&img)))
					{
						trans.loadTranslate(VEGA_INTTOFIX(-xpos), 0);
						itrans.loadTranslate(VEGA_INTTOFIX(xpos), 0);
						img->setTransform(trans, itrans);
						m_renderer->setFill(img);
						m_renderer->fillRect(animatedArea.x-xpos, animatedArea.y,
											 animatedArea.width, animatedArea.height,
											 GetCurrentStencil());
					}
				}
				else if (animationType == VEGA_ANIMATION_SLIDE_LEFT)
				{
					int xpos = (int)(animatedArea.width*(1.0-t));
					if (OpStatus::IsSuccess(gi_stat))
					{
						trans.loadTranslate(VEGA_INTTOFIX(xpos-(int)animatedArea.width), 0);
						itrans.loadTranslate(VEGA_INTTOFIX((int)animatedArea.width-xpos), 0);
						img->setTransform(trans, itrans);
						img->setFillOpacity(255);
						m_renderer->setFill(img);
						m_renderer->fillRect(animatedArea.x+xpos-animatedArea.width, animatedArea.y,
											 animatedArea.width, animatedArea.height,
											 GetCurrentStencil());
					}
					if (OpStatus::IsSuccess(m_animationRenderTarget->getImage(&img)))
					{
						trans.loadTranslate(VEGA_INTTOFIX(xpos), 0);
						itrans.loadTranslate(VEGA_INTTOFIX(-xpos), 0);
						img->setTransform(trans, itrans);
						m_renderer->setFill(img);
						m_renderer->fillRect(animatedArea.x+xpos, animatedArea.y,
											 animatedArea.width, animatedArea.height,
											 GetCurrentStencil());
					}
				}
				else if (animationType == VEGA_ANIMATION_SLIDE_UP)
				{
					int ypos = (int)(animatedArea.height*(1.0-t));
					if (OpStatus::IsSuccess(gi_stat))
					{
						trans.loadTranslate(0, VEGA_INTTOFIX(ypos-(int)animatedArea.height));
						itrans.loadTranslate(0, VEGA_INTTOFIX((int)animatedArea.height-ypos));
						img->setTransform(trans, itrans);
						img->setFillOpacity(255);
						m_renderer->setFill(img);
						m_renderer->fillRect(animatedArea.x, animatedArea.y+ypos-animatedArea.height,
											 animatedArea.width, animatedArea.height,
											 GetCurrentStencil());
					}
					if (OpStatus::IsSuccess(m_animationRenderTarget->getImage(&img)))
					{
						trans.loadTranslate(0, VEGA_INTTOFIX(ypos));
						itrans.loadTranslate(0, VEGA_INTTOFIX(-ypos));
						img->setTransform(trans, itrans);
						m_renderer->setFill(img);
						m_renderer->fillRect(animatedArea.x, animatedArea.y+ypos,
											 animatedArea.width, animatedArea.height,
											 GetCurrentStencil());
					}
				}
			}
		}

		// Restore clipping
		m_renderer->setClipRect(0, 0, m_renderTarget->getWidth(), m_renderTarget->getHeight());

		//Flush the whole buffer
		update_rects = NULL;
		num_rects = 0;
	}
#endif // VEGA_OPPAINTER_ANIMATIONS

		m_renderer->flush(update_rects, num_rects);

#ifdef VEGA_OPPAINTER_ANIMATIONS
	if (m_isAnimating)
		m_renderer->setRenderTarget(m_animationRenderTarget);
#endif // VEGA_OPPAINTER_ANIMATIONS
}

BOOL VEGAOpPainter::Supports(SUPPORTS supports)
{
	switch (supports)
	{
	case SUPPORTS_TRANSPARENT:
	case SUPPORTS_ALPHA:
	case SUPPORTS_ALPHA_COLOR:
	case SUPPORTS_GETSCREEN:
		return TRUE;
	case SUPPORTS_TILING:
		if (g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_HW3D)
			return TRUE;
		if (g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_HW2D)
			return TRUE;
		return FALSE;
	default:
		return FALSE;
	}
}

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
bool VEGAOpPainter::PrinterNeedsFallback() const
{
	return fillstate.fill != NULL || (fillstate.color >> 24) < 255;
}

void VEGAOpPainter::EmitPrinterFallback(const OpRect& r)
{
	// We could potentially use VEGARenderer::getRender{Min,Max}{X,Y}
	// here to automatically calculate the extents of the changes - it
	// is unsure whether that information is reliable for all backends
	// (so maybe an approximation should be used).
	if (OpBitmap* bmp = CreateBitmapFromBackground(r))
	{
		m_printerListener->DrawBitmapClipped(bmp, OpRect(0, 0, r.width, r.height), r.TopLeft());

		OP_DELETE(bmp);
	}
}
#endif // VEGA_PRINTER_LISTENER_SUPPORT

void VEGAOpPainter::SetColor(UINT8 red, UINT8 green, UINT8 blue, UINT8 alpha)
{
	PRE_MUL(alpha);

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	if (m_printerListener)
		m_printerListener->SetColor(red, green, blue, alpha);
#endif // VEGA_PRINTER_LISTENER_SUPPORT

	fillstate.color = (alpha<<24)|(red<<16)|(green<<8)|blue;
	fillstate.fill = NULL;
}

void VEGAOpPainter::SetFillTransform(const VEGATransform& filltrans)
{
	filltransform.copy(filltrans);
	filltransform_is_set = true;
}

void VEGAOpPainter::ResetFillTransform()
{
	filltransform_is_set = false;
}

UINT32 VEGAOpPainter::GetColor()
{
	UINT32 cur_color = fillstate.color;
	// BGRA -> RGBA
	return (cur_color & 0xff00ff00) | ((cur_color & 0xff) << 16) | ((cur_color & 0xff0000) >> 16);
}

VEGAFill* VEGAOpPainter::CreateLinearGradient(VEGA_FIX x1, VEGA_FIX y1, VEGA_FIX x2, VEGA_FIX y2,
											  unsigned int numStops, VEGA_FIX* stopOffsets, UINT32* stopColors, BOOL premultiplied)
{
	VEGAFill* fill = NULL;
	m_renderer->createLinearGradient(&fill, x1, y1, x2, y2, numStops, stopOffsets, stopColors, !!premultiplied);
	return fill;
}

VEGAFill* VEGAOpPainter::CreateRadialGradient(VEGA_FIX fx, VEGA_FIX fy, VEGA_FIX cx, VEGA_FIX cy,
											  VEGA_FIX radius, unsigned int numStops,
											  VEGA_FIX* stopOffsets, UINT32* stopColors, BOOL premultiplied)
{
	VEGAFill* fill = NULL;
	m_renderer->createRadialGradient(&fill, fx, fy, 0, cx, cy, radius, numStops, stopOffsets, stopColors, !!premultiplied);
	return fill;
}

void VEGAOpPainter::SetFont(OpFont* font)
{
#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	if (m_printerListener)
		m_printerListener->SetFont(font);
#endif // VEGA_PRINTER_LISTENER_SUPPORT
	current_font = font;
}

void VEGAOpPainter::UpdateClipRect()
{
	OpRect clip_rect = GetCurrentClipRect();

	if (opacityStack && m_modifying_stencil == 0 && !IsLayerCached(opacityStack->renderTarget))
	{
		// Fetch the current layer rectangle, intersect with it, adjust offset and set
		clip_rect.IntersectWith(opacityStack->rect);
		clip_rect.OffsetBy(-opacityStack->rect.x, -opacityStack->rect.y);
	}

	m_renderer->setClipRect(clip_rect.x, clip_rect.y, clip_rect.width, clip_rect.height);
}

#ifdef CSS_TRANSFORMS
static inline void TransformPoint(const VEGATransform& t, const OpPoint& p,
								  VEGA_FIX& x, VEGA_FIX& y)
{
	x = VEGA_INTTOFIX(p.x);
	y = VEGA_INTTOFIX(p.y);
	t.apply(x, y);
}

static OpRect ToBBox(const VEGATransform& t, const OpRect& r, bool& isAlmostAligned)
{
	isAlmostAligned = false;

	VEGA_FIX minx, miny;
	TransformPoint(t, r.TopLeft(), minx, miny);
	VEGA_FIX maxx = minx, maxy = miny, x, y;
	TransformPoint(t, r.TopRight(), x, y);
	minx = MIN(minx, x); miny = MIN(miny, y);
	maxx = MAX(maxx, x); maxy = MAX(maxy, y);
	TransformPoint(t, r.BottomRight(), x, y);
	minx = MIN(minx, x); miny = MIN(miny, y);
	maxx = MAX(maxx, x); maxy = MAX(maxy, y);
	TransformPoint(t, r.BottomLeft(), x, y);
	minx = MIN(minx, x); miny = MIN(miny, y);
	maxx = MAX(maxx, x); maxy = MAX(maxy, y);

	OpRect out_r;
	out_r.x = VEGA_FIXTOINT(VEGA_FLOOR(minx));
	out_r.y = VEGA_FIXTOINT(VEGA_FLOOR(miny));
	out_r.width = VEGA_FIXTOINT(VEGA_CEIL(maxx)) - out_r.x;
	out_r.height = VEGA_FIXTOINT(VEGA_CEIL(maxy)) - out_r.y;

	// See tweak TWEAK_LIBVEGA_CLIPRECT_TOLERANCE
	VEGA_FIX sx = VEGA_INTTOFIX(out_r.x);
	VEGA_FIX ex = VEGA_INTTOFIX(out_r.x + out_r.width);
	VEGA_FIX sy = VEGA_INTTOFIX(out_r.y);
	VEGA_FIX ey = VEGA_INTTOFIX(out_r.y + out_r.height);
	if (VEGA_ABS(sx - minx) * VEGA_CLIPRECT_TOLERANCE < VEGA_INTTOFIX(1) &&
		VEGA_ABS(sy - miny) * VEGA_CLIPRECT_TOLERANCE < VEGA_INTTOFIX(1) &&
		VEGA_ABS(ex - maxx) * VEGA_CLIPRECT_TOLERANCE < VEGA_INTTOFIX(1) &&
		VEGA_ABS(ey - maxy) * VEGA_CLIPRECT_TOLERANCE < VEGA_INTTOFIX(1))
		isAlmostAligned = true;

	return out_r;
}

#endif // CSS_TRANSFORMS

OP_STATUS VEGAOpPainter::SetClipRect(const OpRect& rect)
{
	ClipStack* cs = OP_NEW(ClipStack, ());
	if (!cs)
		return OpStatus::ERR_NO_MEMORY;

	cs->next = clipStack;

#ifdef CSS_TRANSFORMS
	cs->has_stencil = false;

	if (HasTransform())
	{
		VEGATransform trans;
		GetClipRectTransform(trans);

		bool isAlmostAligned;
		cs->clip = ToBBox(trans, rect, isAlmostAligned);
		cs->clip.OffsetBy(translateX, translateY);

		// Only create a stenciled clip if the parent clip was created
		// using a stencil, or the current transform is not a simple
		// translation.
		if (!(trans.isScaleAndTranslateOnly() && isAlmostAligned) ||
			(clipStack && clipStack->has_stencil))
		{
			OpRect bounding_rect = GetCurrentClipRect();

			bounding_rect.IntersectWith(cs->clip);

			if (!bounding_rect.IsEmpty())
			{
				// BeginStencil will translate the rect, so undo translation
				bounding_rect.x -= translateX;
				bounding_rect.y -= translateY;

				OP_STATUS err = BeginStencil(bounding_rect);
				if (OpStatus::IsError(err))
				{
					OP_DELETE(cs);
					return err;
				}

				BeginModifyingStencil();
				{
					FillRect(rect);
				}
				EndModifyingStencil();

				cs->has_stencil = true;
			}
		}
	}
	else
#endif // CSS_TRANSFORMS
	{
		cs->clip = rect;
		cs->clip.x += translateX;
		cs->clip.y += translateY;
	}

	if (clipStack)
	{
		cs->clip.IntersectWith(clipStack->clip);

		if (cs->clip.width < 0)
			cs->clip.width = 0;
		if (cs->clip.height < 0)
			cs->clip.height = 0;
	}
	clipStack = cs;

	UpdateClipRect();

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	if (m_printerListener)
		m_printerListener->SetClipRect(clipStack->clip);
#endif // VEGA_PRINTER_LISTENER_SUPPORT
	return OpStatus::OK;
}

void VEGAOpPainter::RemoveClipRect()
{
	OP_ASSERT(clipStack);
	if (!clipStack)
		return;

	ClipStack* cs = clipStack;
	clipStack = clipStack->next;

#ifdef CSS_TRANSFORMS
	if (cs->has_stencil)
		EndStencil();
#endif // CSS_TRANSFORMS

	OP_DELETE(cs);

	UpdateClipRect();

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	if (m_printerListener)
		m_printerListener->SetClipRect(GetCurrentClipRect());
#endif // VEGA_PRINTER_LISTENER_SUPPORT
}

OpRect VEGAOpPainter::GetCurrentClipRect() const
{
	if (clipStack)
		return clipStack->clip;

	return OpRect(0, 0, m_renderTarget->getWidth(), m_renderTarget->getHeight());
}

void VEGAOpPainter::GetClipRect(OpRect* rect)
{
	// NOTE: Transformed clips will return the bbox of the clip
	*rect = GetCurrentClipRect();

	rect->x -= translateX;
	rect->y -= translateY;
}

VEGAStencil *VEGAOpPainter::GetCurrentStencil()
{
	if (!stencilStack)
		return NULL;

	VEGAStencil* stencil = NULL;
	if (m_modifying_stencil > 0)
	{
		if (stencilStack->next)
			stencil = stencilStack->next->stencil;
	}
	else
	{
		stencil = stencilStack->stencil;
	}

	if (stencil)
		stencil->setOffset(0, 0);

	return stencil;
}

OP_STATUS VEGAOpPainter::BeginStencil(const OpRect& rect)
{
	StencilStack* st = OP_NEW(StencilStack, ());
	if (!st)
		return OpStatus::ERR_NO_MEMORY;

	st->rect = rect;
	st->rect.x += translateX;
	st->rect.y += translateY;
	st->stencil = NULL;
	st->next = stencilStack;
	st->renderTarget = opacityStack ? opacityStack->renderTarget : m_renderTarget;

	if (!stencilStack && m_stencilCache && (m_stencilCache->getWidth() != m_renderTarget->getWidth() ||
											m_stencilCache->getHeight() != m_renderTarget->getHeight()))
	{
		VEGARenderTarget::Destroy(m_stencilCache);
		m_stencilCache = NULL;
	}

	if (stencilStack || !m_stencilCache)
	{
		OP_STATUS err = m_renderer->createStencil(&st->stencil, false, m_renderTarget->getWidth(), m_renderTarget->getHeight());
		if (OpStatus::IsError(err))
		{
			OP_DELETE(st);
			return err;
		}
	}
	else
	{
		st->stencil = m_stencilCache;

		// Reinitialize the cached stencil
		int old_clipx, old_clipy, old_clipw, old_cliph;
		m_renderer->getClipRect(old_clipx, old_clipy, old_clipw, old_cliph);
		m_renderer->setRenderTarget(st->stencil);

		m_renderer->setClipRect(st->rect.x, st->rect.y, st->rect.width, st->rect.height);
		m_renderer->clear(st->rect.x, st->rect.y, st->rect.width, st->rect.height, 0);

		m_renderer->setClipRect(old_clipx, old_clipy, old_clipw, old_cliph);
		m_renderer->setRenderTarget(st->renderTarget);

		st->stencil->setOffset(0, 0);
	}

	if (!m_stencilCache)
	{
		m_stencilCache = st->stencil;
	}

	stencilStack = st;
	return OpStatus::OK;
}

void VEGAOpPainter::BeginModifyingStencil()
{
	OP_ASSERT(stencilStack);
	m_renderer->setRenderTarget(stencilStack->stencil);
	m_modifying_stencil++;

	UpdateClipRect();
}

void VEGAOpPainter::EndModifyingStencil()
{
	OP_ASSERT(stencilStack);
	m_modifying_stencil--;
	m_renderer->setRenderTarget(stencilStack->renderTarget);

	UpdateClipRect();
}

void VEGAOpPainter::EndStencil()
{
	OP_ASSERT(stencilStack);
	StencilStack* oldst = stencilStack;
	stencilStack = stencilStack->next;

	// The toplevel stencil is cached, so don't delete it
	if (stencilStack)
	{
		VEGARenderTarget::Destroy(oldst->stencil);
	}
	OP_DELETE(oldst);
}

#ifdef CSS_TRANSFORMS
static OP_STATUS CreatePathForRect(const OpRect& r, VEGAPath& p)
{
	RETURN_IF_ERROR(p.prepare(5));
	RETURN_IF_ERROR(p.moveTo(VEGA_INTTOFIX(r.x), VEGA_INTTOFIX(r.y)));
	RETURN_IF_ERROR(p.lineTo(VEGA_INTTOFIX(r.x+r.width), VEGA_INTTOFIX(r.y)));
	RETURN_IF_ERROR(p.lineTo(VEGA_INTTOFIX(r.x+r.width), VEGA_INTTOFIX(r.y+r.height)));
	RETURN_IF_ERROR(p.lineTo(VEGA_INTTOFIX(r.x), VEGA_INTTOFIX(r.y+r.height)));
	RETURN_IF_ERROR(p.close(true));
#ifdef VEGA_3DDEVICE
	p.forceConvex();
#endif
	return OpStatus::OK;
}

static VEGAPrimitive MakeVEGAPrimitive(const OpRect& r, VEGATransform* trans)
{
	VEGAPrimitive prim;
	prim.type = VEGAPrimitive::RECTANGLE;
	prim.transform = trans;
	prim.data.rect.x = VEGA_INTTOFIX(r.x);
	prim.data.rect.y = VEGA_INTTOFIX(r.y);
	prim.data.rect.width = VEGA_INTTOFIX(r.width);
	prim.data.rect.height = VEGA_INTTOFIX(r.height);
	return prim;
}
#endif // CSS_TRANSFORMS

OP_STATUS VEGAOpPainter::PaintRect(OpRect& r)
{
#ifdef CSS_TRANSFORMS
	if (HasTransform())
	{
		VEGATransform trans;
		GetCTM(trans);
		VEGAPrimitive prim = MakeVEGAPrimitive(r, &trans);
		return m_renderer->fillPrimitive(&prim, GetLayerStencil());
	}
#endif // CSS_TRANSFORMS

	r.OffsetBy(GetLayerOffset());

	if (r.x < 0)
	{
		r.width += r.x;
		r.x = 0;
	}
	if (r.y < 0)
	{
		r.height += r.y;
		r.y = 0;
	}
	if (r.IsEmpty())
		return OpStatus::OK;

	return m_renderer->fillRect(r.x, r.y, r.width, r.height, GetLayerStencil());
}

OP_STATUS VEGAOpPainter::PaintImage(VEGAOpBitmap* vbmp, VEGADrawImageInfo& diinfo)
{
#ifdef VEGA_LIMIT_BITMAP_SIZE
	if (vbmp->isTiled())
		return vbmp->paintTiled(this, diinfo);
#endif // VEGA_LIMIT_BITMAP_SIZE

#if defined(CSS_TRANSFORMS)
	if (HasTransform())
	{
		OpRect dest(diinfo.dstx, diinfo.dsty, diinfo.dstw, diinfo.dsth);

		VEGAFill* img = NULL;
		RETURN_IF_ERROR(vbmp->getFill(m_renderer, &img));

		if (diinfo.type == VEGADrawImageInfo::REPEAT)
			img->setSpread(VEGAFill::SPREAD_REPEAT);
		else
			img->setSpread(VEGAFill::SPREAD_CLAMP);

		img->setFillOpacity(diinfo.opacity);
		img->setQuality(diinfo.quality);

		VEGATransform trans, itrans, ctm;
		GetCTM(ctm);
		VEGAImage::calcTransforms(diinfo, trans, itrans, &ctm);

		img->setTransform(trans, itrans);

		m_renderer->setFill(img);

		VEGAPrimitive prim = MakeVEGAPrimitive(dest, &ctm);

		OP_STATUS status = m_renderer->fillPrimitive(&prim, GetLayerStencil());

		m_renderer->setFill(NULL);

		return status;
	}
#endif

	OpPoint lofs = GetLayerOffset();
	diinfo.dstx += lofs.x;
	diinfo.dsty += lofs.y;

	return m_renderer->drawImage(vbmp, diinfo, GetLayerStencil());
}

OP_STATUS VEGAOpPainter::PaintPath(VEGAPath& path)
{
	VEGATransform trans;
	GetCTM(trans);

	// This might hurt
	path.transform(&trans);

	OP_STATUS status = m_renderer->fillPath(&path, GetLayerStencil());
	m_renderer->setFill(NULL);
	return status;
}

void VEGAOpPainter::SetupComplexFill()
{
	OP_ASSERT(fillstate.fill);

	// Calculate transform for the fill as:
	// Transform = FillTransform * CTM * T(X, Y)
	VEGATransform trans;
	GetCTM(trans);

	if (filltransform_is_set)
		trans.multiply(filltransform);

	VEGATransform itrans;
	itrans.copy(trans);
	itrans.invert();

	fillstate.fill->setTransform(trans, itrans);

	m_renderer->setFill(fillstate.fill);
}

void VEGAOpPainter::DrawRect(const OpRect& rect, UINT32 width)
{
	const VEGA_FIX half = VEGA_INTTOFIX(1)/2;

	VEGAPath p;
	RETURN_VOID_IF_ERROR(p.prepare(5));
	RETURN_VOID_IF_ERROR(p.moveTo(VEGA_INTTOFIX(rect.x)+half, VEGA_INTTOFIX(rect.y)+half));
	RETURN_VOID_IF_ERROR(p.lineTo(VEGA_INTTOFIX(rect.x+rect.width)-half, VEGA_INTTOFIX(rect.y)+half));
	RETURN_VOID_IF_ERROR(p.lineTo(VEGA_INTTOFIX(rect.x+rect.width)-half, VEGA_INTTOFIX(rect.y+rect.height)-half));
	RETURN_VOID_IF_ERROR(p.lineTo(VEGA_INTTOFIX(rect.x)+half, VEGA_INTTOFIX(rect.y+rect.height)-half));
	RETURN_VOID_IF_ERROR(p.close(true));

	VEGAPath stroked_p;
	p.setLineWidth(VEGA_INTTOFIX(width));
	RETURN_VOID_IF_ERROR(p.createOutline(&stroked_p, VEGA_INTTOFIX(1), 0));

	SetupFill();

	OpStatus::Ignore(PaintPath(stroked_p));

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	if (m_printerListener && !m_modifying_stencil && !opacityStack)
	{
		OpRect xlt_rect(rect);

		if (PrinterNeedsFallback())
		{
			xlt_rect.IntersectWith(GetCurrentClipRect());
			EmitPrinterFallback(xlt_rect);
		}
		else
			m_printerListener->DrawRect(xlt_rect, width);
	}
#endif // VEGA_PRINTER_LISTENER_SUPPORT
}

void VEGAOpPainter::FillRect(const OpRect& rect)
{
	SetupFill();

	OpRect r(rect);
	OpStatus::Ignore(PaintRect(r));

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	if (m_printerListener && !m_modifying_stencil && !opacityStack)
	{
		if (PrinterNeedsFallback())
		{
			r.IntersectWith(GetCurrentClipRect());
			EmitPrinterFallback(r);
		}
		else
			m_printerListener->FillRect(r);
	}
#endif // VEGA_PRINTER_LISTENER_SUPPORT
}

void VEGAOpPainter::ClearRect(const OpRect& rect)
{
	OpRect r(rect);
	r.OffsetBy(GetLayerOffset());
	if (r.x < 0)
	{
		r.width += r.x;
		r.x = 0;
	}
	if (r.y < 0)
	{
		r.height += r.y;
		r.y = 0;
	}
	if (r.IsEmpty())
		return;

#ifdef CSS_TRANSFORMS
	// This method should _not_ be called when any transform
	// beyond a simple (positive) scale and translation is
	// set. Anything else could yield incorrect results.
	OP_ASSERT(!HasTransform() ||
			  current_transform.isScaleAndTranslateOnly() &&
			  current_transform[0] > 0 && current_transform[4] > 0);
#endif // CSS_TRANSFORMS

	m_renderer->clear(r.x, r.y, r.width, r.height, GetCurrentColor());
}

void VEGAOpPainter::DrawEllipse(const OpRect& rect, UINT32 width)
{
	OP_ASSERT(!rect.IsEmpty());
	VEGA_FIX yrad = VEGA_INTTOFIX(rect.height - 1) / 2;
	VEGA_FIX xrad = VEGA_INTTOFIX(rect.width - 1) / 2;

	VEGA_FIX x = VEGA_INTTOFIX(rect.x) + VEGA_INTTOFIX(1) / 2;
	VEGA_FIX y = VEGA_INTTOFIX(rect.y) + VEGA_INTTOFIX(1) / 2;

	VEGAPath p;
	RETURN_VOID_IF_ERROR(p.moveTo(x, y + yrad));
	RETURN_VOID_IF_ERROR(p.arcTo(x + 2*xrad, y + yrad, xrad, yrad, 0, true, false, VEGA_INTTOFIX(1)/10));
	RETURN_VOID_IF_ERROR(p.arcTo(x, y + yrad, xrad, yrad, 0, true, false, VEGA_INTTOFIX(1)/10));
	RETURN_VOID_IF_ERROR(p.close(true));

	VEGAPath stroked_p;
	p.setLineWidth(VEGA_INTTOFIX(width));
	RETURN_VOID_IF_ERROR(p.createOutline(&stroked_p, VEGA_INTTOFIX(1)/10, 0));

	SetupFill();

	OpStatus::Ignore(PaintPath(stroked_p));

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	if (m_printerListener && !m_modifying_stencil && !opacityStack)
	{
		OpRect r(rect);

		if (PrinterNeedsFallback())
		{
			r.IntersectWith(GetCurrentClipRect());
			EmitPrinterFallback(r);
		}
		else
			m_printerListener->DrawEllipse(r, width);
	}
#endif // VEGA_PRINTER_LISTENER_SUPPORT
}

void VEGAOpPainter::FillEllipse(const OpRect& rect)
{
	VEGA_FIX xrad = VEGA_INTTOFIX(rect.width) / 2;
	VEGA_FIX yrad = VEGA_INTTOFIX(rect.height) / 2;

	VEGA_FIX x = VEGA_INTTOFIX(rect.x);
	VEGA_FIX y = VEGA_INTTOFIX(rect.y);

	VEGAPath p;
	RETURN_VOID_IF_ERROR(p.moveTo(x, y + yrad));
	RETURN_VOID_IF_ERROR(p.arcTo(x + 2*xrad, y + yrad, xrad, yrad, 0, true, false, VEGA_INTTOFIX(1)/10));
	RETURN_VOID_IF_ERROR(p.arcTo(x, y + yrad, xrad, yrad, 0, true, false, VEGA_INTTOFIX(1)/10));
	RETURN_VOID_IF_ERROR(p.close(true));
#ifdef VEGA_3DDEVICE
	p.forceConvex();
#endif

	SetupFill();

	OpStatus::Ignore(PaintPath(p));

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	if (m_printerListener && !m_modifying_stencil && !opacityStack)
	{
		OpRect r = rect;
		if (PrinterNeedsFallback())
		{
			r.IntersectWith(GetCurrentClipRect());
			EmitPrinterFallback(r);
		}
		else
			m_printerListener->FillEllipse(r);
	}
#endif // VEGA_PRINTER_LISTENER_SUPPORT
}

void VEGAOpPainter::DrawPolygon(const OpPoint* points, int count, UINT32 width)
{
	OP_ASSERT(count > 0);

	VEGAPath p;
	const VEGA_FIX half = VEGA_INTTOFIX(1) / 2;

	RETURN_VOID_IF_ERROR(p.prepare(count+1));

	RETURN_VOID_IF_ERROR(p.moveTo(VEGA_INTTOFIX(points[0].x)+half,
								  VEGA_INTTOFIX(points[0].y)+half));
	for (int i = 1; i < count; ++i)
		RETURN_VOID_IF_ERROR(p.lineTo(VEGA_INTTOFIX(points[i].x)+half,
									  VEGA_INTTOFIX(points[i].y)+half));
	RETURN_VOID_IF_ERROR(p.close(true));

	VEGAPath stroked_p;
	p.setLineWidth(VEGA_INTTOFIX(width));
	RETURN_VOID_IF_ERROR(p.createOutline(&stroked_p, VEGA_INTTOFIX(1), 0));

	SetupFill();

	OpStatus::Ignore(PaintPath(stroked_p));

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	if (m_printerListener && !m_modifying_stencil && !opacityStack)
	{
		int minx = m_renderTarget->getWidth();
		int miny = m_renderTarget->getHeight();
		int maxx = 0;
		int maxy = 0;
		for (int p = 0; p < count; ++p)
		{
			if (points[p].x+translateX < minx)
				minx = points[p].x+translateX;
			if (points[p].x+translateX > maxx)
				maxx = points[p].x+translateX;
			if (points[p].y+translateY < miny)
				miny = points[p].y+translateY;
			if (points[p].y+translateY > maxy)
				maxy = points[p].y+translateY;
		}
		OpRect r(minx, miny, maxx-minx, maxy-miny);

		r.IntersectWith(GetCurrentClipRect());
		EmitPrinterFallback(r);
	}
#endif // VEGA_PRINTER_LISTENER_SUPPORT
}

void VEGAOpPainter::DrawLine(const OpPoint& from, UINT32 length, BOOL horizontal, UINT32 width)
{
	OP_ASSERT(width);

	OpRect r(from.x, from.y, 0, 0);

	if (horizontal)
	{
		r.width = length;
		r.height = width;
	}
	else
	{
		r.width = width;
		r.height = length;
	}

	SetupFill();

	OpStatus::Ignore(PaintRect(r));

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	if (m_printerListener && !m_modifying_stencil && !opacityStack)
	{
		if (PrinterNeedsFallback())
		{
			r.IntersectWith(GetCurrentClipRect());
			EmitPrinterFallback(r);
		}
		else
			m_printerListener->FillRect(r);
	}
#endif // VEGA_PRINTER_LISTENER_SUPPORT
}

void VEGAOpPainter::DrawLine(const OpPoint& from, const OpPoint& to, UINT32 width)
{
	const VEGA_FIX half = VEGA_INTTOFIX(1) / 2;

	VEGAPath p;
	RETURN_VOID_IF_ERROR(p.moveTo(VEGA_INTTOFIX(from.x)+half, VEGA_INTTOFIX(from.y)+half));
	RETURN_VOID_IF_ERROR(p.lineTo(VEGA_INTTOFIX(to.x)+half, VEGA_INTTOFIX(to.y)+half));

	VEGAPath op;
	p.setLineCap(VEGA_LINECAP_BUTT);
	p.setLineWidth(VEGA_INTTOFIX(width));
	RETURN_VOID_IF_ERROR(p.createOutline(&op, VEGA_INTTOFIX(1), 0));

	SetupFill();

	OpStatus::Ignore(PaintPath(op));

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	if (m_printerListener && !m_modifying_stencil && !opacityStack)
	{
		if (PrinterNeedsFallback())
		{
			OpRect r;
			if (to.x < from.x)
			{
				r.x = to.x;
				r.width = from.x-to.x;
			}
			else
			{
				r.x = from.x;
				r.width = to.x-from.x;
			}
			if (to.y < from.y)
			{
				r.y = to.y;
				r.height = from.y-to.y;
			}
			else
			{
				r.y = from.y;
				r.height = to.y-from.y;
			}

			r.IntersectWith(GetCurrentClipRect());
			EmitPrinterFallback(r);
		}
		else
			m_printerListener->DrawLine(from, to, width);
	}
#endif // VEGA_PRINTER_LISTENER_SUPPORT
}

void VEGAOpPainter::DrawString(const OpPoint& pos, const uni_char* text, UINT32 len, INT32 extra_char_space, short word_width)
{
	if (!current_font)
		return;
	int pos_x = pos.x;
	int pos_y = pos.y;
	VEGATransform* trans = NULL;
#ifdef CSS_TRANSFORMS
	VEGATransform ctm;
	if (HasTransform())
	{
		GetCTM(ctm);
		trans = &ctm;
	}
	else
#endif // CSS_TRANSFORMS
	{
		OpPoint lofs = GetLayerOffset();
		pos_x += lofs.x;
		pos_y += lofs.y;
	}

	SetupFill();

	VEGAOpFont* vegaopfont = static_cast<VEGAOpFont*>(current_font);
	VEGAFont* vegafont = vegaopfont->getVegaFont();

#ifdef VEGA_NATIVE_FONT_SUPPORT
	if (!trans && word_width != -1)
	{
		const UINT32 width = vegaopfont->StringWidth(text, len, this, extra_char_space);
		word_width = word_width - width;
	}
#endif // VEGA_NATIVE_FONT_SUPPORT

	m_renderer->drawString(vegafont, pos_x, pos_y, text, len, extra_char_space, word_width,
						   GetLayerStencil(), trans);

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	if (m_printerListener && !m_modifying_stencil && !opacityStack)
	{
		if (PrinterNeedsFallback())
		{
			OpRect r;
			r.x = pos.x;
			r.y = pos.y;
			r.height = current_font->Height();
			r.width = current_font->StringWidth(text, len, this, extra_char_space);

			r.IntersectWith(GetCurrentClipRect());
			EmitPrinterFallback(r);
		}
		else
		{
			m_printerListener->DrawString(pos, text, len, extra_char_space, word_width);
		}
	}
#endif // VEGA_PRINTER_LISTENER_SUPPORT
}

void VEGAOpPainter::InvertRect(const OpRect& rect)
{
#ifdef CSS_TRANSFORMS
	if (HasTransform())
	{
		VEGATransform ctm;
		GetCTM(ctm);

		VEGAPath path;
		RETURN_VOID_IF_ERROR(CreatePathForRect(rect, path));

		path.transform(&ctm);

		OpStatus::Ignore(InvertShape(path));
	}
	else
#endif // CSS_TRANSFORMS
	{
		OpRect r(rect);

		if (clipStack)
		{
			// This kludge is due to the fact that the clip stack stores rects
			// pretranslated by translateX/Y (translating rects right before
			// passing them into VEGARenderer would probably be better)

			r.OffsetBy(translateX, translateY);
			r.IntersectWith(clipStack->clip);
			r.OffsetBy(-translateX, -translateY);
		}

		r.OffsetBy(GetLayerOffset());

		if (r.x < 0)
		{
			r.width += r.x;
			r.x = 0;
		}
		if (r.y < 0)
		{
			r.height += r.y;
			r.y = 0;
		}

		if (r.IsEmpty())
			return;

#ifdef CSS_TRANSFORMS
		OP_ASSERT(!HasTransform());
#endif // CSS_TRANSFORMS

		m_renderer->invertRect(r.x, r.y, r.width, r.height);
	}
}

OP_STATUS VEGAOpPainter::InvertShape(VEGAPath& shape)
{
	return m_renderer->invertPath(&shape);
}

void VEGAOpPainter::InvertBorderRect(const OpRect& rect, int border)
{
	// The border should grow towards the center of the rectangle.
	VEGAPath p;
	OpRect xlt_rect(rect);
	xlt_rect.OffsetBy(GetLayerOffset());
	const VEGA_FIX half_border = VEGA_INTTOFIX(border)/2;

	RETURN_VOID_IF_ERROR(p.prepare(5));
	RETURN_VOID_IF_ERROR(p.moveTo(VEGA_INTTOFIX(xlt_rect.x)+half_border, VEGA_INTTOFIX(xlt_rect.y)+half_border));
	RETURN_VOID_IF_ERROR(p.lineTo(VEGA_INTTOFIX(xlt_rect.x+xlt_rect.width)-half_border, VEGA_INTTOFIX(xlt_rect.y)+half_border));
	RETURN_VOID_IF_ERROR(p.lineTo(VEGA_INTTOFIX(xlt_rect.x+xlt_rect.width)-half_border, VEGA_INTTOFIX(xlt_rect.y+xlt_rect.height)-half_border));
	RETURN_VOID_IF_ERROR(p.lineTo(VEGA_INTTOFIX(xlt_rect.x)+half_border, VEGA_INTTOFIX(xlt_rect.y+xlt_rect.height)-half_border));
	RETURN_VOID_IF_ERROR(p.close(true));

	VEGAPath stroked_p;
	p.setLineWidth(VEGA_INTTOFIX(border));

	// Make sure the corners are crisp and sharp.
	p.setLineJoin(VEGA_LINEJOIN_MITER);
	p.setMiterLimit(VEGA_INTTOFIX(2));

	RETURN_VOID_IF_ERROR(p.createOutline(&stroked_p, VEGA_INTTOFIX(1), 0));

#ifdef CSS_TRANSFORMS
	OP_ASSERT(!HasTransform());
#endif // CSS_TRANSFORMS

	RETURN_VOID_IF_ERROR(InvertShape(stroked_p));
}

void VEGAOpPainter::InvertBorderEllipse(const OpRect& rect, int border)
{
	// The border should grow towards the center of the ellipse.
	VEGAPath p;

	OP_ASSERT(!rect.IsEmpty());
	VEGA_FIX yrad = VEGA_INTTOFIX(rect.height - border) / 2;
	VEGA_FIX xrad = VEGA_INTTOFIX(rect.width - border) / 2;

	OpPoint lofs = GetLayerOffset();
	VEGA_FIX left = VEGA_INTTOFIX(rect.x+lofs.x) + VEGA_INTTOFIX(border) / 2;
	VEGA_FIX top = VEGA_INTTOFIX(rect.y+lofs.y) + VEGA_INTTOFIX(border) / 2;

	RETURN_VOID_IF_ERROR(p.moveTo(left, top + yrad));
	RETURN_VOID_IF_ERROR(p.arcTo(left + 2*xrad, top + yrad, xrad, yrad, 0, true, false, VEGA_INTTOFIX(1)/10));
	RETURN_VOID_IF_ERROR(p.arcTo(left, top + yrad, xrad, yrad, 0, true, false, VEGA_INTTOFIX(1)/10));
	RETURN_VOID_IF_ERROR(p.close(true));

	VEGAPath stroked_p;
	p.setLineWidth(VEGA_INTTOFIX(border));
	RETURN_VOID_IF_ERROR(p.createOutline(&stroked_p, VEGA_INTTOFIX(1)/10, 0));

#ifdef CSS_TRANSFORMS
	OP_ASSERT(!HasTransform());
#endif // CSS_TRANSFORMS

	RETURN_VOID_IF_ERROR(InvertShape(stroked_p));
}

void VEGAOpPainter::InvertBorderPolygon(const OpPoint* point_array, int points, int border)
{
	OP_ASSERT(points > 0);

	// The other invert methods grow towards the center but hard
	// to say what is the center of a generic polygon.
	VEGAPath p;
	const VEGA_FIX half = VEGA_INTTOFIX(1) / 2;
	OpPoint lofs = GetLayerOffset();

	RETURN_VOID_IF_ERROR(p.prepare(points+1));
	RETURN_VOID_IF_ERROR(p.moveTo(VEGA_INTTOFIX(point_array[0].x+lofs.x)+half,
								  VEGA_INTTOFIX(point_array[0].y+lofs.y)+half));
	for (int i = 1; i < points; ++i)
		RETURN_VOID_IF_ERROR(p.lineTo(VEGA_INTTOFIX(point_array[i].x+lofs.x)+half,
									  VEGA_INTTOFIX(point_array[i].y+lofs.y)+half));
	RETURN_VOID_IF_ERROR(p.close(true));

	VEGAPath stroked_p;
	p.setLineWidth(VEGA_INTTOFIX(border));
	RETURN_VOID_IF_ERROR(p.createOutline(&stroked_p, VEGA_INTTOFIX(1), 0));

#ifdef CSS_TRANSFORMS
	OP_ASSERT(!HasTransform());
#endif // CSS_TRANSFORMS

	RETURN_VOID_IF_ERROR(InvertShape(stroked_p));
}

void VEGAOpPainter::GetCTM(VEGATransform& trans)
{
	OpPoint lofs = GetLayerOffset();
	trans.loadTranslate(VEGA_INTTOFIX(lofs.x), VEGA_INTTOFIX(lofs.y));

#ifdef CSS_TRANSFORMS
	if (HasTransform())
		trans.multiply(current_transform);
#endif // CSS_TRANSFORMS
}

// FIXME: support more blit image things
void VEGAOpPainter::DrawBitmapClipped(const OpBitmap* bitmap, const OpRect& source, OpPoint p)
{
	if (source.x < 0 || source.y < 0 ||
		source.x + source.width > (int)bitmap->Width() ||
		source.y + source.height > (int)bitmap->Height())
	{
		OP_ASSERT(!"Your code is buggy. It's sending source rectangles that is outside the bitmap!");
		return;
	}

	VEGAOpBitmap* vbmp = (VEGAOpBitmap*)bitmap;
	VEGADrawImageInfo diinfo;
	diinfo.type = VEGADrawImageInfo::NORMAL;

	diinfo.dstx = p.x;
	diinfo.dsty = p.y;
	diinfo.dstw = source.width;
	diinfo.dsth = source.height;

	diinfo.srcx = source.x;
	diinfo.srcy = source.y;
	diinfo.srcw = source.width;
	diinfo.srch = source.height;

	diinfo.quality = GetImageQuality();

	int image_opacity = m_image_opacity;
	PRE_MUL(image_opacity);
	diinfo.opacity = image_opacity;

	OpStatus::Ignore(PaintImage(vbmp, diinfo));

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	if (m_printerListener && !m_modifying_stencil && !opacityStack)
	{
		if (bitmap->HasAlpha())
		{
			EmitPrinterFallback(OpRect(diinfo.dstx, diinfo.dsty, source.width, source.height));
		}
		else
			m_printerListener->DrawBitmapClipped(bitmap, source, OpPoint(diinfo.dstx, diinfo.dsty));
	}
#endif // VEGA_PRINTER_LISTENER_SUPPORT
}

void VEGAOpPainter::DrawBitmapClippedTransparent(const OpBitmap* bitmap, const OpRect& source, OpPoint p)
{
	DrawBitmapClipped(bitmap, source, p);
}

void VEGAOpPainter::DrawBitmapClippedAlpha(const OpBitmap* bitmap, const OpRect& source, OpPoint p)
{
	DrawBitmapClipped(bitmap, source, p);
}

void VEGAOpPainter::DrawBitmapScaled(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest)
{
	OP_ASSERT(OpRect(0, 0, bitmap->Width(), bitmap->Height()).Contains(source));

	VEGAOpBitmap* vbmp = (VEGAOpBitmap*)bitmap;
	VEGADrawImageInfo diinfo;
	diinfo.type = VEGADrawImageInfo::SCALED;

	diinfo.dstx = dest.x;
	diinfo.dsty = dest.y;
	diinfo.dstw = dest.width;
	diinfo.dsth = dest.height;

	diinfo.srcx = source.x;
	diinfo.srcy = source.y;
	diinfo.srcw = source.width;
	diinfo.srch = source.height;

	diinfo.quality = GetImageQuality();

	int image_opacity = m_image_opacity;
	PRE_MUL(image_opacity);
	diinfo.opacity = image_opacity;

	OpStatus::Ignore(PaintImage(vbmp, diinfo));

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	if (m_printerListener && !m_modifying_stencil && !opacityStack)
		EmitPrinterFallback(OpRect(diinfo.dstx, diinfo.dsty, diinfo.dstw, diinfo.dsth));
#endif // VEGA_PRINTER_LISTENER_SUPPORT
}

void VEGAOpPainter::DrawBitmapScaledTransparent(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest)
{
	DrawBitmapScaled(bitmap, source, dest);
}

void VEGAOpPainter::DrawBitmapScaledAlpha(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest)
{
	DrawBitmapScaled(bitmap, source, dest);
}

OP_STATUS VEGAOpPainter::DrawBitmapTiled(const OpBitmap* bitmap, const OpPoint& offset, const OpRect& dest, INT32 scale)
{
	return DrawBitmapTiled(bitmap, offset, dest, scale, (bitmap->Width()*scale)/100, (bitmap->Width()*scale)/100);
}

OP_STATUS VEGAOpPainter::DrawBitmapTiled(const OpBitmap* bitmap, const OpPoint& offset, const OpRect& const_dest, INT32 scale, UINT32 bitmapWidth, UINT32 bitmapHeight)
{
	OpRect dest = const_dest;

#ifdef VEGA_LIMIT_BITMAP_SIZE
	if (static_cast<const VEGAOpBitmap*>(bitmap)->isTiled())
	{
		OP_ASSERT(!"not allowed, calling code should have checked this");
		return OpStatus::ERR_NOT_SUPPORTED;
	}
#endif // VEGA_LIMIT_BITMAP_SIZE

	int image_opacity = m_image_opacity;
	PRE_MUL(image_opacity);

	// constrain values to avoid overflow
#ifdef CSS_TRANSFORMS
	if (!HasTransform())
#endif // CSS_TRANSFORMS
	{
		OpRect clip;
		GetClipRect(&clip);
		if (dest.x < clip.x)
		{
			const unsigned scaled_w = bitmap->Width() * scale;
			const unsigned times = (clip.x - dest.x) / scaled_w;
			const unsigned size  = times * scaled_w;
			dest.x     += size;
			dest.width -= size;
		}
		if (dest.y < clip.y)
		{
			const unsigned scaled_h = bitmap->Height() * scale;
			const unsigned times = (clip.y - dest.y) / scaled_h;
			const unsigned size  = times * scaled_h;
			dest.y      += size;
			dest.height -= size;
		}
	}

	VEGADrawImageInfo diinfo;
	diinfo.type = VEGADrawImageInfo::REPEAT;

	diinfo.dstx = dest.x;
	diinfo.dsty = dest.y;
	diinfo.dstw = dest.width;
	diinfo.dsth = dest.height;

	diinfo.srcx = 0;
	diinfo.srcy = 0;
	diinfo.srcw = bitmap->Width();
	diinfo.srch = bitmap->Height();

	diinfo.tile_offset_x = offset.x;
	diinfo.tile_offset_y = offset.y;
	diinfo.tile_width = bitmapWidth;
	diinfo.tile_height = bitmapHeight;

	diinfo.quality = GetImageQuality();
	diinfo.opacity = image_opacity;

	const VEGAOpBitmap* vbmp = static_cast<const VEGAOpBitmap*>(bitmap);
	OP_STATUS status = PaintImage(const_cast<VEGAOpBitmap*>(vbmp), diinfo);

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	if (m_printerListener && !m_modifying_stencil && !opacityStack)
	{
		OpRect r(dest);
		r.IntersectWith(GetCurrentClipRect());
		EmitPrinterFallback(r);
	}
#endif // VEGA_PRINTER_LISTENER_SUPPORT
	return status;
}

OP_STATUS VEGAOpPainter::FillPath(class VEGAPath& path)
{
	SetupFill();

	return PaintPath(path);
}

OP_STATUS VEGAOpPainter::FillPathImmutable(const class VEGAPath& path)
{
	VEGAPath mutable_path;
	RETURN_IF_ERROR(mutable_path.copy(&path));

	return FillPath(mutable_path);
}

OpBitmap* VEGAOpPainter::CreateBitmapFromBackground(const OpRect& rect)
{
	OpBitmap* bmp;
	if (OpStatus::IsError(OpBitmap::Create(&bmp, rect.width, rect.height, FALSE, TRUE)))
		return NULL;

	if (OpStatus::IsError(CopyBackgroundToBitmap(bmp, rect)))
	{
		OP_DELETE(bmp);
		return NULL;
	}
	return bmp;
}

OP_STATUS VEGAOpPainter::CopyBackgroundToBitmap(OpBitmap* bmp, const OpRect& rect)
{
	OpRect r(rect);
	r.OffsetBy(GetLayerOffset());

	int dstx = 0;
	int dsty = 0;
	if (r.x < 0)
	{
		dstx -= r.x;
		r.width += r.x;
		r.x = 0;
	}
	if (r.y < 0)
	{
		dsty -= r.y;
		r.height += r.y;
		r.y = 0;
	}
	if (r.x+r.width > (int)m_renderTarget->getWidth())
		r.width = m_renderTarget->getWidth()-r.x;
	if (r.y+r.height > (int)m_renderTarget->getHeight())
		r.height = m_renderTarget->getHeight()-r.y;
	if (r.IsEmpty())
		return OpStatus::OK;


	// Changing render target makes sure all the rendering operations are flushed
	m_renderer->setRenderTarget(NULL);
	m_renderer->setRenderTarget(opacityStack ? opacityStack->renderTarget : m_renderTarget);
	if (OpStatus::IsError(m_renderTarget->copyToBitmap(bmp, r.width, r.height, r.x, r.y, dstx, dsty)))
	{
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

OpRect VEGAOpPainter::GetLayerExtent() const
{
	if (opacityStack && m_modifying_stencil == 0)
		if (!IsLayerCached(opacityStack->renderTarget))
			return opacityStack->rect;

	return OpRect(0, 0, m_renderTarget->getWidth(), m_renderTarget->getHeight());
}

VEGAStencil* VEGAOpPainter::GetLayerStencil()
{
	if (VEGAStencil* stencil = GetCurrentStencil())
	{
		OpPoint ofs = GetLayerExtent().TopLeft();
		stencil->setOffset(-ofs.x, -ofs.y); // Relation between root RT and current RT
		return stencil;
	}
	return NULL;
}

OP_STATUS VEGAOpPainter::GetLayer(VEGARenderTarget*& layer, const OpRect& rect)
{
	OP_ASSERT((unsigned)rect.width  <= m_renderTarget->getWidth() &&
	          (unsigned)rect.height <= m_renderTarget->getHeight());

#ifdef VEGA_OPPAINTER_OPACITY_CACHE
	if (opacityStack)
	{
#endif // VEGA_OPPAINTER_OPACITY_CACHE
		RETURN_IF_ERROR(
		    m_renderer->createIntermediateRenderTarget(&layer,
		                                               rect.width, rect.height));

		OpRect clear_rect(0, 0, rect.width, rect.height);
		ClearLayer(layer, clear_rect);
#ifdef VEGA_OPPAINTER_OPACITY_CACHE
	}
	else // !opacityStack
	{
		// We are creating the first opacity layer

		if (m_opacityRenderTargetCache)
		{
			// Provided the painter has not been resized, we can reuse the
			// cached render target for the opacity layer

			if (m_opacityRenderTargetCache->getWidth()  == m_renderTarget->getWidth() &&
			    m_opacityRenderTargetCache->getHeight() == m_renderTarget->getHeight())
			{
				// Reuse layer!

				layer = m_opacityRenderTargetCache;
				goto clear;
			}

			VEGARenderTarget::Destroy(m_opacityRenderTargetCache);
			m_opacityRenderTargetCache = NULL;
		}

		// We will need to create a new layer. The first opacity layer, which
		// we cache, always has the dimensions of the render target.

		RETURN_IF_ERROR(
		    m_renderer->createIntermediateRenderTarget(&layer,
		                                               m_renderTarget->getWidth(),
		                                               m_renderTarget->getHeight()));
		m_opacityRenderTargetCache = layer;

clear:
		ClearLayer(layer, rect);

	}
#endif // VEGA_OPPAINTER_OPACITY_CACHE

	return OpStatus::OK;
}

void VEGAOpPainter::ClearLayer(VEGARenderTarget* layer, const OpRect& rect)
{
	m_renderer->setRenderTarget(layer);
	m_renderer->setClipRect(rect.x, rect.y, rect.width, rect.height);
	m_renderer->clear(rect.x, rect.y, rect.width, rect.height, 0);
}

void VEGAOpPainter::PutLayer(VEGARenderTarget* layer)
{
#ifdef VEGA_OPPAINTER_OPACITY_CACHE
	// The render target for toplevel opacity is cached, so don't delete it
	if (!opacityStack)
	{
		OP_ASSERT(m_renderTarget->getWidth() == layer->getWidth() &&
				  m_renderTarget->getHeight() == layer->getHeight());
		return;
	}
#endif // VEGA_OPPAINTER_OPACITY_CACHE

	VEGARenderTarget::Destroy(layer);
}

OP_STATUS VEGAOpPainter::BeginOpacity(const OpRect& rect, UINT8 opacity)
{
	OpacityStack* op = OP_NEW(OpacityStack, ());
	if (!op)
		return OpStatus::ERR_NO_MEMORY;

	op->rect = rect;
	op->rect.x += translateX;
	op->rect.y += translateY;
	op->opacity = opacity;

	OP_STATUS status = GetLayer(op->renderTarget, op->rect);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(op);
		return status;
	}

	op->next = opacityStack;
	opacityStack = op;

	m_renderer->setRenderTarget(opacityStack->renderTarget);

	UpdateClipRect();
	return OpStatus::OK;
}

void VEGAOpPainter::EndOpacity()
{
	OP_ASSERT(opacityStack);

	OpRect src_area = GetLayerExtent();

	OpacityStack* oldop = opacityStack;
	opacityStack = opacityStack->next;

	m_renderer->setRenderTarget(opacityStack ? opacityStack->renderTarget : m_renderTarget);

	OpRect dst_area = GetLayerExtent();

	UINT8 opacity = oldop->opacity;
	PRE_MUL(opacity);

	if (VEGAStencil* stencil = GetLayerStencil())
	{
		VEGAFill* f;
		if (OpStatus::IsSuccess(oldop->renderTarget->getImage(&f)))
		{
			f->setFillOpacity(opacity);
			f->setSpread(VEGAFill::SPREAD_CLAMP);

			// Offset of source area relative to destination area
			OpPoint ofs(src_area.x - dst_area.x, src_area.y - dst_area.y);

			if (ofs.x != 0 || ofs.y != 0)
			{
				VEGATransform trans, itrans;
				trans.loadTranslate(VEGA_INTTOFIX(ofs.x), VEGA_INTTOFIX(ofs.y));
				itrans.loadTranslate(VEGA_INTTOFIX(-ofs.x), VEGA_INTTOFIX(-ofs.y));

				f->setTransform(trans, itrans);
			}

			m_renderer->setFill(f);
			m_renderer->fillRect(oldop->rect.x - dst_area.x, oldop->rect.y - dst_area.y,
								 oldop->rect.width, oldop->rect.height, stencil);
			m_renderer->setFill(NULL);
		}
	}
	else
	{
		VEGAFilter* filter;
		if (OpStatus::IsSuccess(m_renderer->createOpacityMergeFilter(&filter, opacity)))
		{
			VEGAFilterRegion region;
			region.dx = oldop->rect.x - dst_area.x;
			region.dy = oldop->rect.y - dst_area.y;
			region.sx = oldop->rect.x - src_area.x;
			region.sy = oldop->rect.y - src_area.y;
			region.width = oldop->rect.width;
			region.height = oldop->rect.height;

			filter->setSource(oldop->renderTarget);

			m_renderer->applyFilter(filter, region);

			OP_DELETE(filter);
		}
	}

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	if (m_printerListener && !m_modifying_stencil && !opacityStack)
		EmitPrinterFallback(oldop->rect);
#endif // VEGA_PRINTER_LISTENER_SUPPORT

	PutLayer(oldop->renderTarget);

	OP_DELETE(oldop);

	UpdateClipRect();
}

UINT8 VEGAOpPainter::GetAccumulatedOpacity()
{
	unsigned int o = 255;
	for (OpacityStack* frame = opacityStack; frame; frame = frame->next)
		o = ((o + 1) * frame->opacity) >> 8;
	OP_ASSERT(o <= 255);
	return static_cast<UINT8>(o);
}

void VEGAOpPainter::MoveRect(int x, int y, unsigned int w, unsigned int h, int dx, int dy)
{
	m_renderer->setClipRect(0, 0, m_renderTarget->getWidth(), m_renderTarget->getHeight());
	m_renderer->moveRect(x, y, w, h, dx, dy);

	UpdateClipRect();
}

void VEGAOpPainter::SetVegaTranslation(int x, int y)
{
	translateX = x;
	translateY = y;
}

#ifdef VEGA_OPPAINTER_ANIMATIONS
OP_STATUS VEGAOpPainter::prepareAnimation(const OpRect& rect, VegaAnimationEvent evt)
{
	if (!m_animationRenderTarget)
	{
		RETURN_IF_ERROR(m_renderer->createIntermediateRenderTarget(&m_animationRenderTarget,
																   m_renderTarget->getWidth(),
																   m_renderTarget->getHeight()));
	}
	if (!m_animationPreviousRenderTarget)
	{
		RETURN_IF_ERROR(m_renderer->createIntermediateRenderTarget(&m_animationPreviousRenderTarget,
																   m_renderTarget->getWidth(),
																   m_renderTarget->getHeight()));
	}
	if (evt == ANIM_SWITCH_TAB)
		animationType = VEGA_ANIMATION_FADE;
	else if (evt == ANIM_SHOW_WINDOW)
		animationType = VEGA_ANIMATION_FADE;
	else if (evt == ANIM_HIDE_WINDOW)
		animationType = VEGA_ANIMATION_FADE;
	else if (evt == ANIM_LOAD_PAGE_BACK)
	{
		// Don't replace a window animation with a new page animation
		if (m_isAnimating && (m_currentEvent == ANIM_SWITCH_TAB ||
							  m_currentEvent == ANIM_SHOW_WINDOW ||
							  m_currentEvent == ANIM_HIDE_WINDOW))
			return OpStatus::OK;

		animationType = VEGA_ANIMATION_SLIDE_RIGHT;
	}
	else // if (evt == ANIM_LOAD_PAGE_FWD)
	{
		// Don't replace a window animation with a new page animation
		if (m_isAnimating && (m_currentEvent == ANIM_SWITCH_TAB ||
							  m_currentEvent == ANIM_SHOW_WINDOW ||
							  m_currentEvent == ANIM_HIDE_WINDOW))
			return OpStatus::OK;

		animationType = VEGA_ANIMATION_SLIDE_LEFT;
	}

	// Copy the current renderTarget to animationRenderTarget and
	// animationPreviousRenderTarget
	OpBitmap* bmp;
	VEGAFill* img = NULL;

	RETURN_IF_ERROR(OpBitmap::Create(&bmp, m_renderTarget->getWidth(), m_renderTarget->getHeight()));

	OP_STATUS err = m_renderTarget->copyToBitmap(bmp);
	if (OpStatus::IsError(err))
	{
		OP_DELETE(bmp);
		return err;
	}
	err = m_renderer->createImage(&img, bmp);
	if (OpStatus::IsError(err))
	{
		OP_DELETE(img);
		OP_DELETE(bmp);
		return err;
	}

	m_renderer->setRenderTarget(m_animationPreviousRenderTarget);
	m_renderer->setFill(img);
	m_renderer->fillRect(0, 0, m_renderTarget->getWidth(), m_renderTarget->getHeight(), GetCurrentStencil());

	m_renderer->setRenderTarget(m_animationRenderTarget);
	m_renderer->setFill(img);
	m_renderer->fillRect(0, 0, m_renderTarget->getWidth(), m_renderTarget->getHeight(), GetCurrentStencil());

	m_isAnimating = TRUE;
	m_animationStart = 0;
	animatedArea = OpRect(0,0,0,0);
	m_currentEvent = evt;

	OP_DELETE(img);
	OP_DELETE(bmp);

	g_main_message_handler->RemoveDelayedMessage(MSG_VEGA_ANIMATION_UPDATE_SCREEN, (MH_PARAM_1)this, 0);
	g_main_message_handler->PostDelayedMessage(MSG_VEGA_ANIMATION_UPDATE_SCREEN, (MH_PARAM_1)this, 0, 0);
	return OpStatus::OK;
}

OP_STATUS VEGAOpPainter::startAnimation(const OpRect& rect)
{
	// Set the animation start time and animating to true
	if (m_isAnimating && m_animationStart == 0)
	{
		m_animationStart = g_op_time_info->GetWallClockMS();
		animatedArea = rect;
	}
	return OpStatus::OK;
}

void VEGAOpPainter::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(msg == MSG_VEGA_ANIMATION_UPDATE_SCREEN);
	present();
	if (m_isAnimating)
		g_main_message_handler->PostDelayedMessage(MSG_VEGA_ANIMATION_UPDATE_SCREEN, (MH_PARAM_1)this, 0, 0);
}
#endif // VEGA_OPPAINTER_ANIMATIONS

class VEGASprite
{
public:
	VEGASprite(unsigned int width, unsigned int height) : x(0), y(0), 
		w(width), h(height), image(NULL), background(NULL)
	{}
	~VEGASprite()
	{
		OP_DELETE(image);
		OP_DELETE(background);
	}

	OP_STATUS Init()
	{
		RETURN_IF_ERROR(OpBitmap::Create(&image, w, h, FALSE, TRUE));
		return OpBitmap::Create(&background, w, h, FALSE, TRUE);
	}

	void SetData(UINT32* data, unsigned int stride)
	{
		if (!data)
		{
#ifdef DEBUG_ENABLE_OPASSERT
			BOOL r =
#endif // DEBUG_ENABLE_OPASSERT
				image->SetColor(NULL, TRUE, NULL);
			OP_ASSERT(r);
		}
		else
			for (unsigned int yp = 0; yp < h; ++yp)
				image->AddLine(data+(stride*yp)/4, yp);
	}

	void SetPosition(int x, int y)
	{
		this->x = x;
		this->y = y;
	}

	int x;
	int y;
	unsigned int w;
	unsigned int h;

	OpBitmap* image;
	OpBitmap* background;
};

VEGASprite* VEGAOpPainter::CreateSprite(unsigned int w, unsigned int h)
{
	VEGASprite* spr = OP_NEW(VEGASprite, (w, h));
	if (!spr || OpStatus::IsError(spr->Init()))
	{
		OP_DELETE(spr);
		spr = NULL;
	}
	return spr;
}

void VEGAOpPainter::DestroySprite(VEGASprite* spr)
{
	OP_DELETE(spr);
}

void VEGAOpPainter::AddSprite(VEGASprite* spr, int x, int y)
{
	SetVegaTranslation(0,0);
	spr->SetPosition(x, y);

	CopyBackgroundToBitmap(spr->background, OpRect(spr->x, spr->y, spr->w, spr->h));
	DrawBitmapClippedAlpha(spr->image, OpRect(0, 0, spr->w, spr->h), OpPoint(spr->x, spr->y));
}

void VEGAOpPainter::RemoveSprite(VEGASprite* spr)
{
	SetVegaTranslation(0,0);
	DrawBitmapClipped(spr->background, OpRect(0, 0, spr->w, spr->h), OpPoint(spr->x, spr->y));
}

void VEGAOpPainter::SetSpriteData(VEGASprite* spr, UINT32* data, unsigned int stride)
{
	spr->SetData(data, stride);
}

void VEGAOpPainter::SetRenderingHint(RenderingHint hint)
{
	m_renderer->setQuality(hint == Quality ? VEGA_DEFAULT_QUALITY : 0);
}

OpPainter::RenderingHint VEGAOpPainter::GetRenderingHint()
{
	return m_renderer->getQuality() > 0 ? Quality : Speed;
}

#endif // VEGA_SUPPORT && VEGA_OPPAINTER_SUPPORT

