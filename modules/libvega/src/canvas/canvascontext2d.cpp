/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef CANVAS_SUPPORT

#include "modules/libvega/src/canvas/canvas.h"
#include "modules/libvega/src/canvas/canvascontext2d.h"

#include "modules/libvega/vegafill.h"
#include "modules/libvega/src/vegaimage.h"
#include "modules/libvega/src/vegabackend.h"

#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpScreenInfo.h"

#ifdef PREFS_HAVE_INTERPOLATE_IMAGES
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#endif // PREFS_HAVE_INTERPOLATE_IMAGES

#ifdef VEGA_LIMIT_BITMAP_SIZE
# include "modules/libvega/src/oppainter/vegaopbitmap.h"
#endif // VEGA_LIMIT_BITMAP_SIZE

#define RGB_TO_VEGA(col) ((col&0xff00ff00) | ((col>>16)&0xff) | ((col&0xff)<<16))
#define VEGA_TO_RGB(col) RGB_TO_VEGA(col)

CanvasGradient::CanvasGradient() : m_stopOffsets(NULL), m_stopColors(NULL), m_numStops(0), m_gradfill(NULL)
{}
CanvasGradient::~CanvasGradient()
{
	OP_ASSERT(!InList()); /* DOMCanvasContext2D::GCTrace should remove from list */

	OP_DELETE(m_gradfill);
	OP_DELETEA(m_stopOffsets);
	OP_DELETEA(m_stopColors);
}

OP_STATUS CanvasGradient::addColorStop(double stop, UINT32 color)
{
	VEGA_FIX vstop = DOUBLE_TO_VEGA(stop);
	VEGA_FIX *noffs;
	unsigned int *ncols;
	noffs = OP_NEWA(VEGA_FIX, m_numStops+1);
	ncols = OP_NEWA(unsigned int, m_numStops+1);
	if (!noffs || ! ncols)
	{
		OP_DELETEA(noffs);
		OP_DELETEA(ncols);
		return OpStatus::ERR_NO_MEMORY;
	}
	unsigned int i;
	for( i = 0; i < m_numStops; ++i )
	{
		if (m_stopOffsets[i] > vstop)
			break;
		noffs[i] = m_stopOffsets[i];
		ncols[i] = m_stopColors[i];
	}
	noffs[i] = vstop;
	ncols[i] = RGB_TO_VEGA(color);
	for(; i < m_numStops; ++i )
	{
		noffs[i+1] = m_stopOffsets[i];
		ncols[i+1] = m_stopColors[i];
	}
	OP_DELETEA(m_stopOffsets);
	m_stopOffsets = noffs;
	OP_DELETEA(m_stopColors);
	m_stopColors = ncols;
	++m_numStops;
	OP_DELETE(m_gradfill);
	m_gradfill = NULL;
	return OpStatus::OK;
}

OP_STATUS CanvasGradient::initLinear(double x1, double y1, double x2, double y2)
{
	m_values[0] = DOUBLE_TO_VEGA(x1);
	m_values[1] = DOUBLE_TO_VEGA(y1);
	m_values[2] = DOUBLE_TO_VEGA(x2);
	m_values[3] = DOUBLE_TO_VEGA(y2);
	m_radial = FALSE;
	return OpStatus::OK;
}
OP_STATUS CanvasGradient::initRadial(double x1, double y1, double r1, double x2, double y2, double r2)
{
	m_values[0] = DOUBLE_TO_VEGA(x1);
	m_values[1] = DOUBLE_TO_VEGA(y1);
	m_values[2] = DOUBLE_TO_VEGA(r1);
	m_values[3] = DOUBLE_TO_VEGA(x2);
	m_values[4] = DOUBLE_TO_VEGA(y2);
	m_values[5] = DOUBLE_TO_VEGA(r2);
	m_radial = TRUE;
	return OpStatus::OK;
}

VEGAFill *CanvasGradient::getFill(VEGARenderer* renderer, const VEGATransform &trans)
{
	// If not fill is returned transparent black is used, which is what the spec
	// says should be used for no stops
	if (!m_numStops)
		return NULL;

	if (m_radial)
	{
		// If the inner and outer circles are identical nothing should be rendered
		if (m_values[0] == m_values[3] && m_values[1] == m_values[4] && m_values[2] == m_values[5])
			return NULL;
	}
	else
	{
		// Transparent black if start and stop point are identical
		if (m_values[0] == m_values[2] && m_values[1] == m_values[3])
			return NULL;
	}

	if (!m_gradfill)
	{
		VEGA_FIX* stopOfs = OP_NEWA(VEGA_FIX, m_numStops);
		UINT32* stopCol = OP_NEWA(UINT32, m_numStops);
		if (!stopOfs || !stopCol)
		{
			OP_DELETEA(stopOfs);
			OP_DELETEA(stopCol);
			return NULL;
		}

		for (unsigned int i = 0; i < m_numStops; ++i)
		{
			stopCol[i] = m_stopColors[i];
			stopOfs[i] = m_stopOffsets[i];
		}

		OP_STATUS status;
		if (m_radial)
		{
			status = renderer->createRadialGradient(&m_gradfill,
													m_values[0], m_values[1], m_values[2],
													m_values[3], m_values[4], m_values[5],
													m_numStops, stopOfs, stopCol);
		}
		else
		{
			status = renderer->createLinearGradient(&m_gradfill,
													m_values[0], m_values[1],
													m_values[2], m_values[3],
													m_numStops, stopOfs, stopCol);
		}

		if (OpStatus::IsError(status))
			m_gradfill = NULL;

		OP_DELETEA(stopOfs);
		OP_DELETEA(stopCol);
	}

	if (m_gradfill)
	{
		VEGATransform it;
		it.copy(trans);
		it.invert();

		m_gradfill->setTransform(trans, it);
		m_gradfill->setSpread(VEGAFill::SPREAD_CLAMP);
	}
	return m_gradfill;
}

/* static */ unsigned int
CanvasGradient::allocationCost(CanvasGradient *gradient)
{
	return sizeof(CanvasGradient) + (gradient->m_numStops + 1) * (sizeof(VEGA_FIX) + sizeof(unsigned int));
}

CanvasPattern::CanvasPattern() : m_bitmap(NULL), m_imgfill(NULL)
{}
CanvasPattern::~CanvasPattern()
{
	OP_ASSERT(!InList()); /* DOMCanvasContext2D::GCTrace should remove from list */

	OP_DELETE(m_imgfill);
	OP_DELETE(m_bitmap);
}

OP_STATUS CanvasPattern::init(OpBitmap *bmp, VEGAFill::Spread xrep, VEGAFill::Spread yrep)
{
	m_xrepeat = xrep;
	m_yrepeat = yrep;
	OP_DELETE(m_bitmap);
	RETURN_IF_ERROR(OpBitmap::Create(&m_bitmap, bmp->Width(), bmp->Height(), FALSE, TRUE, 0, 0, FALSE));
	const UINT32 width = bmp->Width();
	UINT32* line = OP_NEWA(UINT32, width);
	if (!line)
		return OpStatus::ERR_NO_MEMORY;
	for (unsigned int y = 0; y < bmp->Height(); ++y)
	{
		bmp->GetLineData(line, y);
		m_bitmap->AddLine(line, y);
	}
	OP_DELETEA(line);
	return OpStatus::OK;
}

static inline void setImageQuality(VEGAFill *fill)
{
	VEGAFill::Quality quality = VEGAFill::QUALITY_NEAREST;

#if defined(PREFS_HAVE_INTERPOLATE_IMAGES) && !defined(VEGA_CANVAS_OPTIMIZE_SPEED)
	if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::InterpolateImages))
		quality = VEGAFill::QUALITY_LINEAR;
#endif

	fill->setQuality(quality);
}

VEGAFill *CanvasPattern::getFill(VEGARenderer* renderer, const VEGATransform &trans)
{
	VEGATransform it;
	it.copy(trans);
	it.invert();
	if (!m_imgfill)
	{
		if (OpStatus::IsError(renderer->createImage(&m_imgfill, m_bitmap)))
			return NULL;
	}
	m_imgfill->setSpread(m_xrepeat, m_yrepeat);
	m_imgfill->setBorderColor(0);
	m_imgfill->setTransform(trans, it);
	setImageQuality(m_imgfill);
	return m_imgfill;
}

/* static */ unsigned int
CanvasPattern::allocationCost(CanvasPattern *pattern)
{
	OpBitmap *bitmap = pattern->m_bitmap;
	return sizeof(CanvasPattern) + g_op_screen_info->GetBitmapAllocationSize(bitmap->Width(), bitmap->Height(), bitmap->IsTransparent(), bitmap->HasAlpha(), 0);
}

CanvasContext2D::CanvasContext2D(Canvas *can, VEGARenderer* rend) :
	m_canvas(can), m_vrenderer(rend), m_state_stack(NULL),
	m_current_path_started(false),
	m_composite_filter(NULL), m_copy_filter(NULL), m_buffer(NULL),
	m_offscreen_buffer(NULL), m_shadow_rt(NULL),
	m_shadow_color_rt(NULL), m_current_shadow_color(0),
	m_shadow_filter_merge(NULL), m_shadow_filter_blur(NULL),
	m_shadow_filter_blur_radius(0), m_shadow_filter_modulate(NULL),
	m_layer(NULL), m_ref_count(1)
{
	// Set these to NULL here, so that setDefaults() can call delete/unref on them.
	m_current_state.clip = NULL;
#ifdef CANVAS_TEXT_SUPPORT
	m_current_state.font = NULL;
#endif // CANVAS_TEXT_SUPPORT

	setDefaults();
}

CanvasContext2D::~CanvasContext2D()
{
	OP_ASSERT(m_ref_count <= 0);
	// Clear all stored states so we do not leak them
	while (m_state_stack)
	{
		if (m_state_stack->clip != m_current_state.clip)
		{
			VEGARenderTarget::Destroy(m_current_state.clip);
			m_current_state.clip = m_state_stack->clip;
		}

#ifdef CANVAS_TEXT_SUPPORT
		if (m_current_state.font)
			m_current_state.font->Unref();

		m_current_state.font = m_state_stack->font;
#endif // CANVAS_TEXT_SUPPORT

		CanvasContext2DState *ns = m_state_stack->next;
		OP_DELETE(m_state_stack);
		m_state_stack = ns;
	}
	OP_DELETE(m_offscreen_buffer);
	VEGARenderTarget::Destroy(m_current_state.clip);
#ifdef CANVAS_TEXT_SUPPORT
	if (m_current_state.font)
		m_current_state.font->Unref();
#endif // CANVAS_TEXT_SUPPORT
	OP_DELETE(m_composite_filter);
	OP_DELETE(m_copy_filter);

	VEGARenderTarget::Destroy(m_shadow_rt);
	VEGARenderTarget::Destroy(m_shadow_color_rt);
	OP_DELETE(m_shadow_filter_merge);
	OP_DELETE(m_shadow_filter_blur);
	OP_DELETE(m_shadow_filter_modulate);
	VEGARenderTarget::Destroy(m_layer);
}

void CanvasContext2D::Reference()
{
	++m_ref_count;
}

void CanvasContext2D::Release()
{
	--m_ref_count;
	if (m_ref_count <= 0)
		OP_DELETE(this);
}

void CanvasContext2D::ClearCanvas()
{
	m_canvas = NULL;
}

void CanvasContext2D::GetReferencedFills(List<CanvasFill>* fills)
{
	CanvasContext2DState *state = &m_current_state;
	while (state)
	{
		if (state->fill.gradient && !state->fill.gradient->InList())
			state->fill.gradient->Into(fills);
		if (state->fill.pattern && !state->fill.pattern->InList())
			state->fill.pattern->Into(fills);

		if (state->stroke.gradient && !state->stroke.gradient->InList())
			state->stroke.gradient->Into(fills);
		if (state->stroke.pattern && !state->stroke.pattern->InList())
			state->stroke.pattern->Into(fills);

		if (state != &m_current_state)
			state = state->next;
		else
			state = m_state_stack;
	}
}

/* Init the canvas with the pointer etc. This will also reset everything. */
OP_STATUS CanvasContext2D::initBuffer(VEGARenderTarget* rt)
{
	OP_DELETE(m_offscreen_buffer);
	m_offscreen_buffer = NULL;
	m_buffer = rt;
	//m_vrenderer.setRenderTarget(m_buffer);
	//m_vrenderer.clear(0, 0, w, h, 0);

	RETURN_IF_ERROR(beginPath());

	return OpStatus::OK;
}

void CanvasContext2D::setDefaults()
{
	m_current_state.transform.loadIdentity();
	if (!m_state_stack || m_state_stack->clip != m_current_state.clip)
		VEGARenderTarget::Destroy(m_current_state.clip);
	m_current_state.clip = NULL;
	m_current_state.alpha = 255;
	//m_current_state.compOperation = CANVAS_COMP_SRC_OVER;
	setGlobalCompositeOperation(CANVAS_COMP_SRC_OVER);
	m_current_state.stroke.gradient = NULL;
	m_current_state.stroke.pattern = NULL;
	m_current_state.stroke.color = 0xff000000;
	m_current_state.fill.gradient = NULL;
	m_current_state.fill.pattern = NULL;
	m_current_state.fill.color = 0xff000000;
	m_current_state.lineWidth = DOUBLE_TO_VEGA(1.0);

	m_current_state.shadowOffsetX = 0;
	m_current_state.shadowOffsetY = 0;
	m_current_state.shadowBlur = 0;
	m_current_state.shadowColor = 0;

#ifdef CANVAS_TEXT_SUPPORT
	if (m_current_state.font)
		m_current_state.font->Unref();

	m_current_state.font = NULL;

	m_current_state.textBaseline = CANVAS_TEXTBASELINE_ALPHABETIC;
	m_current_state.textAlign = CANVAS_TEXTALIGN_START;
#endif // CANVAS_TEXT_SUPPORT

	m_current_state.lineCap = VEGA_LINECAP_BUTT;
	m_current_state.lineJoin = VEGA_LINEJOIN_MITER;
	m_current_state.miterLimit = DOUBLE_TO_VEGA(10.0);
}

void CanvasContext2D::setPaintAttribute(PaintAttribute* pa, const VEGATransform &trans)
{
	if (!m_canvas)
		return;

	if (pa->gradient)
	{
		VEGAFill* fill = pa->gradient->getFill(m_vrenderer, trans);
		if (!fill)
		{
			m_vrenderer->setColor(0);
			return;
		}
		fill->setFillOpacity(m_current_state.alpha);
		m_vrenderer->setFill(fill);
	}
	else if (pa->pattern)
	{
		VEGAFill* fill = pa->pattern->getFill(m_vrenderer, trans);
		if (!fill)
		{
			m_vrenderer->setColor(0);
			return;
		}
		fill->setFillOpacity(m_current_state.alpha);
		m_vrenderer->setFill(fill);
	}
	else
	{
		UINT32 col = pa->color;
		if (m_current_state.alpha != 0xff)
		{
			unsigned int alpha = col >> 24;
			alpha *= m_current_state.alpha;
			alpha >>= 8;
			col = (col&0xffffff) | (alpha<<24);
		}
		m_vrenderer->setColor(col);
	}
}

OP_STATUS CanvasContext2D::save()
{
	CanvasContext2DState *prevstate = OP_NEW(CanvasContext2DState, ());
	if (!prevstate)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	prevstate->transform.copy(m_current_state.transform);
	prevstate->clip = m_current_state.clip;
	prevstate->alpha = m_current_state.alpha;
	prevstate->compOperation = m_current_state.compOperation;
	prevstate->stroke = m_current_state.stroke;
	prevstate->fill = m_current_state.fill;
	prevstate->lineWidth = m_current_state.lineWidth;
	prevstate->lineCap = m_current_state.lineCap;
	prevstate->lineJoin = m_current_state.lineJoin;
	prevstate->miterLimit = m_current_state.miterLimit;
	prevstate->shadowOffsetX = m_current_state.shadowOffsetX;
	prevstate->shadowOffsetY = m_current_state.shadowOffsetY;
	prevstate->shadowBlur = m_current_state.shadowBlur;
	prevstate->shadowColor = m_current_state.shadowColor;

#ifdef CANVAS_TEXT_SUPPORT
	if (m_current_state.font)
		m_current_state.font->Ref();

	prevstate->font = m_current_state.font;

	prevstate->textBaseline = m_current_state.textBaseline;
	prevstate->textAlign = m_current_state.textAlign;
#endif // CANVAS_TEXT_SUPPORT

	prevstate->next = m_state_stack;
	m_state_stack = prevstate;
	return OpStatus::OK;
}

void CanvasContext2D::restore()
{
	if (m_state_stack)
	{
		m_current_state.transform.copy(m_state_stack->transform);
		if (m_state_stack->clip != m_current_state.clip)
		{
			VEGARenderTarget::Destroy(m_current_state.clip);
			m_current_state.clip = m_state_stack->clip;
		}
		m_current_state.alpha = m_state_stack->alpha;
		m_current_state.compOperation = m_state_stack->compOperation;
		m_current_state.stroke = m_state_stack->stroke;
		m_current_state.fill = m_state_stack->fill;
		m_current_state.lineWidth = m_state_stack->lineWidth;
		m_current_state.lineCap = m_state_stack->lineCap;
		m_current_state.lineJoin = m_state_stack->lineJoin;
		m_current_state.miterLimit = m_state_stack->miterLimit;

		m_current_state.shadowOffsetX = m_state_stack->shadowOffsetX;
		m_current_state.shadowOffsetY = m_state_stack->shadowOffsetY;
		m_current_state.shadowBlur = m_state_stack->shadowBlur;
		m_current_state.shadowColor = m_state_stack->shadowColor;

#ifdef CANVAS_TEXT_SUPPORT
		if (m_current_state.font)
			m_current_state.font->Unref();

		m_current_state.font = m_state_stack->font;

		m_current_state.textBaseline = m_state_stack->textBaseline;
		m_current_state.textAlign = m_state_stack->textAlign;
#endif // CANVAS_TEXT_SUPPORT

		CanvasContext2DState *ns = m_state_stack->next;
		OP_DELETE(m_state_stack);
		m_state_stack = ns;
		// reset the actual renderer
		setGlobalCompositeOperation(m_current_state.compOperation);
	}
}

void CanvasContext2D::scale(double x, double y)
{
	VEGATransform t;
	t.loadScale(DOUBLE_TO_VEGA(x), DOUBLE_TO_VEGA(y));
	m_current_state.transform.multiply(t);
}
void CanvasContext2D::rotate(double angle)
{
	VEGATransform t;
	t.loadRotate(DOUBLE_TO_VEGA(angle*180.f/3.14159265f));
	m_current_state.transform.multiply(t);
}
void CanvasContext2D::translate(double x, double y)
{
	VEGATransform t;
	t.loadTranslate(DOUBLE_TO_VEGA(x), DOUBLE_TO_VEGA(y));
	m_current_state.transform.multiply(t);
}

void CanvasContext2D::transform(double m11, double m12, double m21, double m22, double dx, double dy)
{
	VEGATransform t;
	t[0] = DOUBLE_TO_VEGA(m11);
	t[1] = DOUBLE_TO_VEGA(m21);
	t[2] = DOUBLE_TO_VEGA(dx);
	t[3] = DOUBLE_TO_VEGA(m12);
	t[4] = DOUBLE_TO_VEGA(m22);
	t[5] = DOUBLE_TO_VEGA(dy);
	m_current_state.transform.multiply(t);
}

void CanvasContext2D::setTransform(double m11, double m12, double m21, double m22, double dx, double dy)
{
	m_current_state.transform[0] = DOUBLE_TO_VEGA(m11);
	m_current_state.transform[1] = DOUBLE_TO_VEGA(m21);
	m_current_state.transform[2] = DOUBLE_TO_VEGA(dx);
	m_current_state.transform[3] = DOUBLE_TO_VEGA(m12);
	m_current_state.transform[4] = DOUBLE_TO_VEGA(m22);
	m_current_state.transform[5] = DOUBLE_TO_VEGA(dy);
}

void CanvasContext2D::clearClipHelper(VEGARenderTarget* dst, int dx, int dy, int dw, int dh,
									  VEGAStencil* clip)
{
	// We essentially want to do destination-out here, but we lack
	// that operation, so do a 'reverse' source-out instead.

	// Create temporary copy of (the appointed area of) the render target
	VEGARenderTarget* temp_copy;
	RETURN_VOID_IF_ERROR(cloneSurface(temp_copy, dst, dx, dy, dw, dh));

	// Create source-out filter
	VEGAFilter* srcout_filter;
	if (OpStatus::IsSuccess(m_vrenderer->createMergeFilter(&srcout_filter, VEGAMERGE_OUT)))
	{
		// Clear the original buffer
		m_vrenderer->setRenderTarget(dst);
		m_vrenderer->clear(dx, dy, dw, dh, 0);

		// Draw the clip shape into the buffer
		m_vrenderer->setColor(0xffffffff);
		m_vrenderer->fillRect(dx, dy, dw, dh, clip);

		// Apply a source-out filter
		VEGAFilterRegion region;
		region.dx = dx;
		region.dy = dy;
		region.sx = 0;
		region.sy = 0;
		region.width = dw;
		region.height = dh;

		srcout_filter->setSource(temp_copy);
		m_vrenderer->applyFilter(srcout_filter, region);
		OP_DELETE(srcout_filter);
	}

	VEGARenderTarget::Destroy(temp_copy);
}

void CanvasContext2D::clearRectHelper(VEGAPath& path)
{
	// Determine bbox of area to clear.
	VEGA_FIX bx, by, bw, bh;
	path.getBoundingBox(bx, by, bw, bh);

	int ib_minx = VEGA_TRUNCFIXTOINT(VEGA_FLOOR(bx));
	int ib_miny = VEGA_TRUNCFIXTOINT(VEGA_FLOOR(by));
	int ib_maxx = VEGA_TRUNCFIXTOINT(VEGA_CEIL(bx+bw));
	int ib_maxy = VEGA_TRUNCFIXTOINT(VEGA_CEIL(by+bh));

	OpRect clear_rect(ib_minx, ib_miny, ib_maxx - ib_minx, ib_maxy - ib_miny);
	clear_rect.IntersectWith(OpRect(0, 0, m_buffer->getWidth(), m_buffer->getHeight()));

	if (clear_rect.IsEmpty())
		return;

	// We essentially want to do destination-out here, but we lack
	// that operation, so do a 'reverse' source-out instead.

	// Create temporary copy of (the appointed area of) the render target
	VEGARenderTarget* temp_copy;
	RETURN_VOID_IF_ERROR(cloneSurface(temp_copy, m_buffer, clear_rect.x, clear_rect.y,
									  clear_rect.width, clear_rect.height));

	// Create source-out filter
	VEGAFilter* srcout_filter;
	if (OpStatus::IsSuccess(m_vrenderer->createMergeFilter(&srcout_filter, VEGAMERGE_OUT)))
	{
		// Clear the original buffer
		m_vrenderer->setRenderTarget(m_buffer);
		m_vrenderer->clear(clear_rect.x, clear_rect.y, clear_rect.width, clear_rect.height, 0);

		// Draw the clip shape into the buffer
		m_vrenderer->setColor(0xffffffff);
		m_vrenderer->fillPath(&path, m_current_state.clip);

		// Apply a source-out filter
		VEGAFilterRegion region;
		region.dx = clear_rect.x;
		region.dy = clear_rect.y;
		region.sx = 0;
		region.sy = 0;
		region.width = clear_rect.width;
		region.height = clear_rect.height;

		srcout_filter->setSource(temp_copy);
		m_vrenderer->applyFilter(srcout_filter, region);
		OP_DELETE(srcout_filter);
	}

	VEGARenderTarget::Destroy(temp_copy);
}

void CanvasContext2D::clearRect(double x, double y, double w, double h)
{
	if (!m_canvas)
		return;

	VEGA_FIX f_top = DOUBLE_TO_VEGA(y);
	VEGA_FIX f_left = DOUBLE_TO_VEGA(x);
	VEGA_FIX f_bottom = DOUBLE_TO_VEGA(y+h);
	VEGA_FIX f_right = DOUBLE_TO_VEGA(x+w);

	if (!m_current_state.transform.isScaleAndTranslateOnly() || m_current_state.clip)
	{
		VEGAPath p;
		RETURN_VOID_IF_ERROR(p.moveTo(f_left, f_top));
		RETURN_VOID_IF_ERROR(p.lineTo(f_right, f_top));
		RETURN_VOID_IF_ERROR(p.lineTo(f_right, f_bottom));
		RETURN_VOID_IF_ERROR(p.lineTo(f_left, f_bottom));
		RETURN_VOID_IF_ERROR(p.close(true));
#ifdef VEGA_3DDEVICE
		p.forceConvex();
#endif // VEGA_3DDEVICE

		p.transform(&m_current_state.transform);

		clearRectHelper(p);
		return;
	}

	// If the transform is only translation and scale - transform
	// start and stop value, and then ignore the transform.
	m_current_state.transform.apply(f_left, f_top);
	m_current_state.transform.apply(f_right, f_bottom);

	int top = VEGA_FIXTOINT(VEGA_FLOOR(f_top));
	int left = VEGA_FIXTOINT(VEGA_FLOOR(f_left));
	int bottom = VEGA_FIXTOINT(VEGA_CEIL(f_bottom));
	int right = VEGA_FIXTOINT(VEGA_CEIL(f_right));

	// Since w/h can be negative (and the transform can turn the
	// rectangle 'inside-out'), make sure that the corners are what we
	// claim them to be.
	// Note that we only do this unwinding if we already applied the
	// transform (and thus expect to end up in the non-transformed
	// fast-path).
	if (top > bottom)
	{
		int tmp = top;
		top = bottom;
		bottom = tmp;
	}
	if (left > right)
	{
		int tmp = left;
		left = right;
		right = tmp;
	}

	m_vrenderer->setRenderTarget(m_buffer);
	m_vrenderer->clear(left, top, right-left, bottom-top, 0);
}

void CanvasContext2D::fillRect(double x, double y, double w, double h)
{
	if (!m_canvas || !m_buffer)
		return;

	FillParams parms;
	parms.paint = &m_current_state.fill;

	if (w >= 0 && h >= 0 &&
		m_current_state.compOperation == CANVAS_COMP_SRC_OVER &&
		!shouldDrawShadow())
	{
		VEGAPrimitive prim;
		prim.type = VEGAPrimitive::RECTANGLE;
		prim.flatness = 0;
		prim.transform = &m_current_state.transform;
		prim.data.rect.x = DOUBLE_TO_VEGA(x);
		prim.data.rect.y = DOUBLE_TO_VEGA(y);
		prim.data.rect.width = DOUBLE_TO_VEGA(w);
		prim.data.rect.height = DOUBLE_TO_VEGA(h);

		m_vrenderer->setRenderTarget(m_buffer);

		setFillParameters(parms, m_current_state.transform);

		m_vrenderer->fillPrimitive(&prim, m_current_state.clip);
		return;
	}

	VEGAPath p;
	RETURN_VOID_IF_ERROR(p.moveTo(DOUBLE_TO_VEGA(x), DOUBLE_TO_VEGA(y)));
	RETURN_VOID_IF_ERROR(p.lineTo(DOUBLE_TO_VEGA(x+w), DOUBLE_TO_VEGA(y)));
	RETURN_VOID_IF_ERROR(p.lineTo(DOUBLE_TO_VEGA(x+w), DOUBLE_TO_VEGA(y+h)));
	RETURN_VOID_IF_ERROR(p.lineTo(DOUBLE_TO_VEGA(x), DOUBLE_TO_VEGA(y+h)));
	RETURN_VOID_IF_ERROR(p.close(true));
#ifdef VEGA_3DDEVICE
	p.forceConvex();
#endif

	p.transform(&m_current_state.transform);

	fillPath(parms, p);
}
void CanvasContext2D::strokeRect(double x, double y, double w, double h)
{
	if (w == 0 && h == 0)
		return;

	VEGAPath p;
	RETURN_VOID_IF_ERROR(p.moveTo(DOUBLE_TO_VEGA(x), DOUBLE_TO_VEGA(y)));
	if (w == 0)
	{
		RETURN_VOID_IF_ERROR(p.lineTo(DOUBLE_TO_VEGA(x), DOUBLE_TO_VEGA(y+h)));
	}
	else if (h == 0)
	{
		RETURN_VOID_IF_ERROR(p.lineTo(DOUBLE_TO_VEGA(x+w), DOUBLE_TO_VEGA(y)));
	}
	else
	{
		RETURN_VOID_IF_ERROR(p.lineTo(DOUBLE_TO_VEGA(x+w), DOUBLE_TO_VEGA(y)));
		RETURN_VOID_IF_ERROR(p.lineTo(DOUBLE_TO_VEGA(x+w), DOUBLE_TO_VEGA(y+h)));
		RETURN_VOID_IF_ERROR(p.lineTo(DOUBLE_TO_VEGA(x), DOUBLE_TO_VEGA(y+h)));
	}
	RETURN_VOID_IF_ERROR(p.close(true));

	VEGAPath tp;

	p.setLineWidth(m_current_state.lineWidth);
	p.setLineCap(m_current_state.lineCap);
	p.setLineJoin(m_current_state.lineJoin);
	p.setMiterLimit(m_current_state.miterLimit);
	p.createOutline(&tp, CANVAS_FLATNESS, 0); // FIXME:OOM

	tp.transform(&m_current_state.transform);

	FillParams parms;
	parms.paint = &m_current_state.stroke;

	fillPath(parms, tp);
}

OP_STATUS CanvasContext2D::beginPath()
{
	m_current_path_started = false;
	return m_current_path.prepare(128);
}
OP_STATUS CanvasContext2D::closePath()
{
	if (!m_current_path_started)
		return OpStatus::OK;
	return m_current_path.close(true);
}
OP_STATUS CanvasContext2D::moveTo(double x, double y)
{
	m_current_path_started = true;
	VEGA_FIX vx = DOUBLE_TO_VEGA(x);
	VEGA_FIX vy = DOUBLE_TO_VEGA(y);
	m_current_state.transform.apply(vx, vy);
	return m_current_path.moveTo(vx, vy);
}

OP_STATUS CanvasContext2D::lineTo(double x, double y)
{
	if (!m_current_path_started)
		RETURN_IF_ERROR(moveTo(x, y));

	VEGA_FIX vx = DOUBLE_TO_VEGA(x);
	VEGA_FIX vy = DOUBLE_TO_VEGA(y);
	m_current_state.transform.apply(vx, vy);
	return m_current_path.lineTo(vx, vy);
}
OP_STATUS CanvasContext2D::quadraticCurveTo(double cx, double cy, double x, double y)
{
	if (!m_current_path_started)
		RETURN_IF_ERROR(moveTo(cx, cy));

	VEGA_FIX vx = DOUBLE_TO_VEGA(x);
	VEGA_FIX vy = DOUBLE_TO_VEGA(y);
	m_current_state.transform.apply(vx, vy);
	VEGA_FIX vcx = DOUBLE_TO_VEGA(cx);
	VEGA_FIX vcy = DOUBLE_TO_VEGA(cy);
	m_current_state.transform.apply(vcx, vcy);
	return m_current_path.quadraticBezierTo(vx, vy, vcx, vcy, CANVAS_FLATNESS);
}
OP_STATUS CanvasContext2D::bezierCurveTo(double c1x, double c1y, double c2x, double c2y, double x, double y)
{
	if (!m_current_path_started)
		RETURN_IF_ERROR(moveTo(c1x, c1y));

	VEGA_FIX vx = DOUBLE_TO_VEGA(x);
	VEGA_FIX vy = DOUBLE_TO_VEGA(y);
	m_current_state.transform.apply(vx, vy);
	VEGA_FIX vc1x = DOUBLE_TO_VEGA(c1x);
	VEGA_FIX vc1y = DOUBLE_TO_VEGA(c1y);
	m_current_state.transform.apply(vc1x, vc1y);
	VEGA_FIX vc2x = DOUBLE_TO_VEGA(c2x);
	VEGA_FIX vc2y = DOUBLE_TO_VEGA(c2y);
	m_current_state.transform.apply(vc2x, vc2y);
	return m_current_path.cubicBezierTo(vx, vy, vc1x, vc1y, vc2x, vc2y, CANVAS_FLATNESS);
}
OP_STATUS CanvasContext2D::arcTo(double x1, double y1, double x2, double y2, double radius)
{
	if (!m_current_path_started)
		RETURN_IF_ERROR(moveTo(x1, y1));

	VEGA_FIX vx1 = DOUBLE_TO_VEGA(x1);
	VEGA_FIX vy1 = DOUBLE_TO_VEGA(y1);
	m_current_state.transform.apply(vx1, vy1);
	RETURN_IF_ERROR(m_current_path.lineTo(vx1, vy1));

	VEGA_FIX vx2 = DOUBLE_TO_VEGA(x2);
	VEGA_FIX vy2 = DOUBLE_TO_VEGA(y2);
	m_current_state.transform.apply(vx2, vy2);

	VEGA_FIX vradius = DOUBLE_TO_VEGA(radius);
	VEGA_FIX rxx = VEGA_FIXMUL(vradius, m_current_state.transform[0]);
	VEGA_FIX rxy = VEGA_FIXMUL(vradius, m_current_state.transform[3]);
	VEGA_FIX ryx = VEGA_FIXMUL(vradius, m_current_state.transform[1]);
	VEGA_FIX ryy = VEGA_FIXMUL(vradius, m_current_state.transform[4]);
	VEGA_FIX vrx = VEGA_FIXSQRT(VEGA_FIXMUL(rxx, rxx) + VEGA_FIXMUL(rxy, rxy));
	VEGA_FIX vry = VEGA_FIXSQRT(VEGA_FIXMUL(ryx, ryx) + VEGA_FIXMUL(ryy, ryy));

	return m_current_path.arcTo(vx2, vy2, vrx, vry, 0, FALSE, FALSE, CANVAS_FLATNESS);
}
OP_STATUS CanvasContext2D::rect(double x, double y, double w, double h)
{
	VEGA_FIX vx = DOUBLE_TO_VEGA(x);
	VEGA_FIX vy = DOUBLE_TO_VEGA(y);
	m_current_state.transform.apply(vx, vy);
	RETURN_IF_ERROR(m_current_path.moveTo(vx, vy));

	vx = DOUBLE_TO_VEGA(x+w);
	vy = DOUBLE_TO_VEGA(y);
	m_current_state.transform.apply(vx, vy);
	RETURN_IF_ERROR(m_current_path.lineTo(vx, vy));

	vx = DOUBLE_TO_VEGA(x+w);
	vy = DOUBLE_TO_VEGA(y+h);
	m_current_state.transform.apply(vx, vy);
	RETURN_IF_ERROR(m_current_path.lineTo(vx, vy));

	vx = DOUBLE_TO_VEGA(x);
	vy = DOUBLE_TO_VEGA(y+h);
	m_current_state.transform.apply(vx, vy);
	RETURN_IF_ERROR(m_current_path.lineTo(vx, vy));

	RETURN_IF_ERROR(m_current_path.close(true));
	m_current_path_started = true;
	return OpStatus::OK;
}

OP_STATUS CanvasContext2D::arc(double x, double y, double radius, double startang, double endang, BOOL ccw)
{
	// Convert to degrees
	startang = startang * 180 / 3.14159265358979323846;
	endang = endang * 180 / 3.14159265358979323846;

	double deltaang = ccw ? startang - endang : endang - startang;

	if (deltaang >= 360)
	{
		deltaang = 360;
	}
	else
	{
		// Bring the angle into [-360, 360)
		if (deltaang < -360)
			deltaang += op_floor(-deltaang / 360) * 360;

		// Bring the angle into [0, 360)
		if (deltaang <= -360)
			// Special case for -2pi which I gather should transform
			// into the full circumference, and not 0
			deltaang = 360;
		else if (deltaang < 0)
			deltaang += 360;
	}

	if (ccw)
		deltaang = -deltaang;

	VEGA_FIX vstartang = DOUBLE_TO_VEGA(startang);
	VEGA_FIX vdeltaang = DOUBLE_TO_VEGA(deltaang);
	VEGA_FIX vx = DOUBLE_TO_VEGA(x);
	VEGA_FIX vy = DOUBLE_TO_VEGA(y);
	VEGA_FIX vradius = DOUBLE_TO_VEGA(radius);

	BOOL skipMove = FALSE;
	// if the start and end point might be too close, split it in two arcs
	if (vdeltaang > VEGA_INTTOFIX(270) || vdeltaang < VEGA_INTTOFIX(-270))
	{
		vdeltaang = VEGA_FIXDIV2(vdeltaang);

		RETURN_IF_ERROR(partialArc(vx, vy, vradius, vstartang, vdeltaang, FALSE));

		vstartang += vdeltaang;
		skipMove = TRUE;
	}

	return partialArc(vx, vy, vradius, vstartang, vdeltaang, skipMove);
}

OP_STATUS CanvasContext2D::partialArc(VEGA_FIX vx, VEGA_FIX vy, VEGA_FIX vradius,
									  VEGA_FIX vstartang, VEGA_FIX vdeltaang, BOOL skipMove)
{
	VEGA_FIX vx1 = vx + VEGA_FIXMUL(vradius, VEGA_FIXCOS(vstartang));
	VEGA_FIX vy1 = vy + VEGA_FIXMUL(vradius, VEGA_FIXSIN(vstartang));
	VEGA_FIX vx2 = vx + VEGA_FIXMUL(vradius, VEGA_FIXCOS(vstartang+vdeltaang));
	VEGA_FIX vy2 = vy + VEGA_FIXMUL(vradius, VEGA_FIXSIN(vstartang+vdeltaang));
	m_current_state.transform.apply(vx, vy);
	m_current_state.transform.apply(vx1, vy1);
	m_current_state.transform.apply(vx2, vy2);

	VEGA_FIX rxx = VEGA_FIXMUL(vradius, m_current_state.transform[0]);
	VEGA_FIX rxy = VEGA_FIXMUL(vradius, m_current_state.transform[3]);
	VEGA_FIX ryx = VEGA_FIXMUL(vradius, m_current_state.transform[1]);
	VEGA_FIX ryy = VEGA_FIXMUL(vradius, m_current_state.transform[4]);
	VEGA_FIX vrx = VEGA_FIXSQRT(VEGA_FIXMUL(rxx, rxx) + VEGA_FIXMUL(rxy, rxy));
	VEGA_FIX vry = VEGA_FIXSQRT(VEGA_FIXMUL(ryx, ryx) + VEGA_FIXMUL(ryy, ryy));

	if (!skipMove)
	{
		if (m_current_path_started)
		{
			RETURN_IF_ERROR(m_current_path.lineTo(vx1, vy1));
		}
		else
		{
			RETURN_IF_ERROR(m_current_path.moveTo(vx1, vy1));
		}
		m_current_path_started = true;
	}

	return m_current_path.arcTo(vx2, vy2, vrx, vry, 0,
								vdeltaang > VEGA_INTTOFIX(180) || vdeltaang < VEGA_INTTOFIX(-180),
								vdeltaang > 0, CANVAS_FLATNESS);
}

static void removeAddedTail(VEGAPath& p, unsigned int previous_line_count)
{
	bool removed = true;
	while (p.getNumLines() > previous_line_count && removed)
	{
		removed = p.removeLastLine();
		OP_ASSERT(removed);
	}
}
void CanvasContext2D::fill()
{
	unsigned int prevNumLines = m_current_path.getNumLines();

	FillParams parms;
	parms.paint = &m_current_state.fill;

	fillPath(parms, m_current_path);

	removeAddedTail(m_current_path, prevNumLines);
}
void CanvasContext2D::stroke()
{
	VEGATransform inv_ctm;
	inv_ctm.copy(m_current_state.transform);
	if (!inv_ctm.invert())
		return;

	VEGAPath p;
	RETURN_VOID_IF_ERROR(p.copy(&m_current_path)); // FIXME:OOM
	p.transform(&inv_ctm);

	VEGAPath stroked_p;
	p.setLineWidth(m_current_state.lineWidth);
	p.setLineCap(m_current_state.lineCap);
	p.setLineJoin(m_current_state.lineJoin);
	p.setMiterLimit(m_current_state.miterLimit);
	p.createOutline(&stroked_p, CANVAS_FLATNESS, 0); // FIXME:OOM

	stroked_p.transform(&m_current_state.transform);

	FillParams parms;
	parms.paint = &m_current_state.stroke;

	fillPath(parms, stroked_p);
}
void CanvasContext2D::clip()
{
	if (!m_canvas || !m_buffer)
		return;

	// Create a new stencil buffer to set the clipping in
	VEGAStencil* newclip;
	RETURN_VOID_IF_ERROR(m_vrenderer->createStencil(&newclip, false, m_buffer->getWidth(), m_buffer->getHeight()));

	unsigned int prevNumLines = m_current_path.getNumLines();

	m_vrenderer->setColor(0);
	m_vrenderer->setRenderTarget(newclip);
	m_vrenderer->fillPath(&m_current_path, m_current_state.clip); // FIXME:OOM

	removeAddedTail(m_current_path, prevNumLines);

	m_vrenderer->setRenderTarget(NULL);
	// If current stencil exists and is not a copy of a previous states stencil, remove it
	if (m_current_state.clip && (!m_state_stack || m_state_stack->clip != m_current_state.clip))
	{
		VEGARenderTarget::Destroy(m_current_state.clip);
	}
	m_current_state.clip = newclip;
}

static inline VEGA_FIX BlurValueToStdDev(VEGA_FIX blurv)
{
	OP_ASSERT(blurv > 0);

	// "Let sigma be half the value of shadowBlur"
	return blurv / 2;
}

static inline int BlurValueToRadius(VEGA_FIX blurv)
{
	return VEGA_FIXTOINT(VEGA_CEIL(BlurValueToStdDev(blurv) * 3));
}

//
// Shadow generation:
//
// 1) Create surfaces [shadow] and [shcolor]
// 2) Blur / Copy [src] => [shadow]
// 3) Fill [shcolor] with shadowColor
// 4) Colorize using [shcolor] IN [shadow]
//
OP_STATUS CanvasContext2D::createShadow(VEGARenderTarget* src_rt,
										unsigned int src_width, unsigned int src_height,
										VEGARenderTarget** out_shadow)
{
	VEGAFilterRegion region;
	region.dx = region.sx = 0;
	region.dy = region.sy = 0;
	region.width = src_width;
	region.height = src_height;

	// Only grow the cached shadow RT
	if (m_shadow_rt)
	{
		const unsigned shadow_rt_width = m_shadow_rt->getWidth();
		if (shadow_rt_width > src_width)
			src_width = shadow_rt_width;
		const unsigned shadow_rt_height = m_shadow_rt->getHeight();
		if (shadow_rt_height > src_height)
			src_height = shadow_rt_height;
	}
	if (m_shadow_color_rt)
	{
		const unsigned shadow_color_rt_width = m_shadow_color_rt->getWidth();
		if (shadow_color_rt_width > src_width)
			src_width = shadow_color_rt_width;
		const unsigned shadow_color_rt_height = m_shadow_color_rt->getHeight();
		if (shadow_color_rt_height > src_height)
			src_height = shadow_color_rt_height;
	}

	if (m_shadow_rt && (m_shadow_rt->getWidth() < src_width || m_shadow_rt->getHeight() < src_height))
	{
		VEGARenderTarget::Destroy(m_shadow_rt);
		m_shadow_rt = NULL;
	}
	if (!m_shadow_rt)
		RETURN_IF_ERROR(m_vrenderer->createIntermediateRenderTarget(&m_shadow_rt, src_width, src_height));

	if (m_shadow_color_rt && (m_shadow_color_rt->getWidth() < src_width || m_shadow_color_rt->getHeight() < src_height))
	{
		VEGARenderTarget::Destroy(m_shadow_color_rt);
		m_shadow_color_rt = NULL;
	}
	if (!m_shadow_color_rt)
	{
		RETURN_IF_ERROR(m_vrenderer->createIntermediateRenderTarget(&m_shadow_color_rt, src_width, src_height));
		m_current_shadow_color = 0;
	}

	if (m_current_state.shadowColor != m_current_shadow_color)
	{
		m_vrenderer->setRenderTarget(m_shadow_color_rt);

		// globalAlpha should already be applied to the source image
		m_vrenderer->clear(0, 0, m_shadow_color_rt->getWidth(), m_shadow_color_rt->getHeight(), m_current_state.shadowColor);
		m_current_shadow_color = m_current_state.shadowColor;
	}

	m_vrenderer->setRenderTarget(m_shadow_rt);

	VEGAFilter* xferfilter = NULL;
	if (m_current_state.shadowBlur > 0)
	{
		if (m_shadow_filter_blur && m_shadow_filter_blur_radius != m_current_state.shadowBlur)
		{
			OP_DELETE(m_shadow_filter_blur);
			m_shadow_filter_blur = NULL;
		}
		// Create blurred version of source image in shadow buffer
		if (!m_shadow_filter_blur)
		{
			VEGA_FIX stddev = BlurValueToStdDev(m_current_state.shadowBlur);
			RETURN_IF_ERROR(m_vrenderer->createGaussianFilter(&m_shadow_filter_blur, stddev, stddev, false));
			m_shadow_filter_blur_radius = m_current_state.shadowBlur;
		}
		xferfilter = m_shadow_filter_blur;
	}
	else
	{
		// Copy source image to shadow buffer
		if (!m_shadow_filter_merge)
			RETURN_IF_ERROR(m_vrenderer->createMergeFilter(&m_shadow_filter_merge, VEGAMERGE_REPLACE));
		xferfilter = m_shadow_filter_merge;
	}

	xferfilter->setSource(src_rt, true);
	RETURN_IF_ERROR(m_vrenderer->applyFilter(xferfilter, region));

	// Modulate shadow with shadow color
	if (!m_shadow_filter_modulate)
		RETURN_IF_ERROR(m_vrenderer->createMergeFilter(&m_shadow_filter_modulate, VEGAMERGE_IN));
	m_shadow_filter_modulate->setSource(m_shadow_color_rt);
	RETURN_IF_ERROR(m_vrenderer->applyFilter(m_shadow_filter_modulate, region));

	*out_shadow = m_shadow_rt;
	return OpStatus::OK;
}

void CanvasContext2D::calcShadowArea(OpRect& srcrect)
{
	OpRect shadow_rect = srcrect;
	OpRect canvas_rect(0, 0, m_buffer->getWidth(), m_buffer->getHeight());

	// Apply blur radius, if any
	if (m_current_state.shadowBlur > 0)
	{
		int shadow_radius = BlurValueToRadius(m_current_state.shadowBlur);

		shadow_rect = shadow_rect.InsetBy(-shadow_radius);
		canvas_rect = canvas_rect.InsetBy(-shadow_radius);
	}

	int shadow_ox = VEGA_TRUNCFIXTOINT(m_current_state.shadowOffsetX);
	int shadow_oy = VEGA_TRUNCFIXTOINT(m_current_state.shadowOffsetY);

	// Translate the shadow rect to its intended location, and remove
	// the non-visible parts
	shadow_rect.OffsetBy(shadow_ox, shadow_oy);
	shadow_rect.IntersectWith(canvas_rect);

	srcrect = shadow_rect;

	// Translate source rect back to original position
	srcrect.OffsetBy(-shadow_ox, -shadow_oy);
}

bool CanvasContext2D::shouldDrawShadow() const
{
	// "Shadows are only drawn if the opacity component of the alpha
	// component of the color of shadowColor is non-zero and either
	// the shadowBlur is non-zero, or the shadowOffsetX is non-zero,
	// or the shadowOffsetY is non-zero."
	if ((m_current_state.shadowColor >> 24) != 0 &&
		(m_current_state.shadowBlur != 0 ||
		 m_current_state.shadowOffsetX != 0 ||
		 m_current_state.shadowOffsetY != 0))
		return true;

	return false;
}

//
// Compositing:
//
// The following 'classes' exist:
//  * Generic
//  * Zero-preserves [Composite(Src, Dst) == Dst, if Src == 0]
//  * Simple [src-over]
//
// Inputs: Src, Dst, Mask
//
// Generic:
//  Draw() => Src
//  Clone(Dst) => ClDst
//  Composite(Src, ClDst) => (ClDst|Src)
//  Clear(In((ClDst|Src), Mask), Dst)
//
// - Buffering requirements : ClDst + Src
//
// ZeroP(src-mode):
//  Draw(Mask) => Src
//  Composite(Src, Dst) => Dst
//
// - Buffering requirements: Src
//
// ZeroP(dst-mode):
//  Copy(Dst) => Src
//  Draw(Mask) => Dst
//  Composite(Src, Dst) => Dst
//
// - Buffering requirements: Src
//
// Simple:
//  Draw(Mask) => Dst
//
// - Buffering requirements: None
//

OP_STATUS CanvasContext2D::cloneSurface(VEGARenderTarget*& clone, VEGARenderTarget* src,
										int sx, int sy, int sw, int sh)
{
	RETURN_IF_ERROR(m_vrenderer->createIntermediateRenderTarget(&clone, sw, sh));

	OP_STATUS status = copySurface(clone, src, sx, sy, sw, sh, 0, 0);
	if (OpStatus::IsError(status))
		VEGARenderTarget::Destroy(clone);

	return OpStatus::OK;
}

OP_STATUS CanvasContext2D::copySurface(VEGARenderTarget* dst, VEGARenderTarget* src,
									   int sx, int sy, int sw, int sh, int dx, int dy)
{
	if (!m_copy_filter)
		RETURN_IF_ERROR(m_vrenderer->createMergeFilter(&m_copy_filter, VEGAMERGE_REPLACE));

	VEGAFilterRegion copy_region;
	copy_region.sx = sx;
	copy_region.sy = sy;
	copy_region.dx = dx;
	copy_region.dy = dy;
	copy_region.width = sw;
	copy_region.height = sh;

	m_copy_filter->setSource(src);
	m_vrenderer->setRenderTarget(dst);
	m_vrenderer->applyFilter(m_copy_filter, copy_region);

	return OpStatus::OK;
}

enum
{
	COMP_VMERGEOP_MASK	= 0x0ff,
	COMP_DSTMODE		= 0x100,	// Destination mode
	COMP_ZEROP			= 0x200		// Src=0 preserves the destination
};

static const unsigned int VEGACompositeInfo[] =
{
	VEGAMERGE_ATOP	| COMP_ZEROP,					// CANVAS_COMP_SRC_ATOP
	VEGAMERGE_IN,									// CANVAS_COMP_SRC_IN
	VEGAMERGE_OUT,									// CANVAS_COMP_SRC_OUT
	VEGAMERGE_OVER	| COMP_ZEROP,					// CANVAS_COMP_SRC_OVER
	VEGAMERGE_ATOP					| COMP_DSTMODE,	// CANVAS_COMP_DST_ATOP
	VEGAMERGE_IN					| COMP_DSTMODE,	// CANVAS_COMP_DST_IN
	VEGAMERGE_OUT	| COMP_ZEROP	| COMP_DSTMODE,	// CANVAS_COMP_DST_OUT
	VEGAMERGE_OVER	| COMP_ZEROP	| COMP_DSTMODE,	// CANVAS_COMP_DST_OVER
	VEGAMERGE_DARKEN,								// CANVAS_COMP_DARKER
	VEGAMERGE_PLUS	| COMP_ZEROP,					// CANVAS_COMP_LIGHTER
	VEGAMERGE_REPLACE,								// CANVAS_COMP_COPY
	VEGAMERGE_XOR	| COMP_ZEROP					// CANVAS_COMP_XOR
};

static inline bool VEGAZeroPreserves(CanvasContext2D::CanvasCompositeOperation op)
{
	return (VEGACompositeInfo[op] & COMP_ZEROP) != 0;
}

static inline bool VEGADestinationMode(CanvasContext2D::CanvasCompositeOperation op)
{
	return (VEGACompositeInfo[op] & COMP_DSTMODE) != 0;
}

static inline VEGAMergeType VEGATranslateOperation(CanvasContext2D::CanvasCompositeOperation op)
{
	return (VEGAMergeType)(VEGACompositeInfo[op] & COMP_VMERGEOP_MASK);
}

OP_STATUS CanvasContext2D::compositeGeneric(VEGARenderTarget* dst, VEGARenderTarget* src,
											int sx, int sy, int sw, int sh, int dx, int dy)
{
	VEGARenderTarget* dst_clone = NULL;
	RETURN_IF_ERROR(cloneSurface(dst_clone, dst, dx, dy, sw, sh));

	VEGAFilterRegion region;
	region.width = sw;
	region.height = sh;

	VEGARenderTarget* c_src, *c_dst;
	if (VEGADestinationMode(m_current_state.compOperation))
	{
		c_src = dst_clone;
		region.sx = 0;
		region.sy = 0;

		c_dst = src;
		region.dx = sx;
		region.dy = sy;
	}
	else
	{
		c_src = src;
		region.sx = sx;
		region.sy = sy;

		c_dst = dst_clone;
		region.dx = 0;
		region.dy = 0;
	}

	m_composite_filter->setSource(c_src);
	m_vrenderer->setRenderTarget(c_dst);
	m_vrenderer->applyFilter(m_composite_filter, region);

	VEGAFill* img;
	OP_STATUS status = c_dst->getImage(&img);
	if (OpStatus::IsSuccess(status))
	{
		// Clear rect (dst-out)
		clearClipHelper(dst, dx, dy, sw, sh, m_current_state.clip);

		// Draw result on top
		VEGATransform trans, itrans;
		trans.loadTranslate(VEGA_INTTOFIX(dx), VEGA_INTTOFIX(dy));
		itrans.loadTranslate(VEGA_INTTOFIX(-dx), VEGA_INTTOFIX(-dy));

		img->setTransform(trans, itrans);

		m_vrenderer->setFill(img);
		m_vrenderer->setRenderTarget(dst);
		m_vrenderer->fillRect(dx, dy, sw, sh, m_current_state.clip);
		m_vrenderer->setFill(NULL);
	}
	VEGARenderTarget::Destroy(dst_clone);
	return status;
}

OP_STATUS CanvasContext2D::compositeZPSrc(VEGARenderTarget* dst, VEGARenderTarget* src,
										  int sx, int sy, int sw, int sh, int dx, int dy)
{
	VEGAFilterRegion region;
	region.width = sw;
	region.height = sh;

	if (VEGADestinationMode(m_current_state.compOperation))
	{
		region.sx = dx;
		region.sy = dy;
		region.dx = sx;
		region.dy = sy;

		// Perform the operation from the destination surface to the
		// source surface.
		m_composite_filter->setSource(dst);
		m_vrenderer->setRenderTarget(src);
		m_vrenderer->applyFilter(m_composite_filter, region);

		// Copy the result back.
		return copySurface(dst, src, 0, 0, sw, sh, dx, dy);
	}
	else
	{
		region.sx = sx;
		region.sy = sy;
		region.dx = dx;
		region.dy = dy;

		m_composite_filter->setSource(src);
		m_vrenderer->setRenderTarget(dst);
		m_vrenderer->applyFilter(m_composite_filter, region);
	}
	return OpStatus::OK;
}

VEGARenderTarget* CanvasContext2D::getLayer(int width, int height, bool use_stencil)
{
	OP_ASSERT(width > 0 && height > 0);
	if (m_layer)
	{
		int layer_w = m_layer->getWidth();
		int layer_h = m_layer->getHeight();

		if (layer_w >= width && layer_h >= height && !use_stencil)
			return m_layer;

		VEGARenderTarget::Destroy(m_layer);
		m_layer = NULL;

		if (!use_stencil)
		{
			width = MAX(layer_w, width);
			height = MAX(layer_h, height);
		}
	}
	RETURN_VALUE_IF_ERROR(m_vrenderer->createIntermediateRenderTarget(&m_layer, width, height),
						NULL);
	return m_layer;
}

void CanvasContext2D::putLayer(VEGARenderTarget* layer)
{
	OP_ASSERT(layer);
}

void CanvasContext2D::setFillParameters(FillParams& parms, const VEGATransform& ctm)
{
	if (parms.paint)
	{
		setPaintAttribute(parms.paint, ctm);
	}
	else
	{
		VEGATransform trans, itrans;
		trans.copy(ctm);
		trans.multiply(parms.image_transform);

		itrans.copy(trans);
		itrans.invert();

		parms.image->setTransform(trans, itrans);

		m_vrenderer->setFill(parms.image);
	}
}

OP_STATUS CanvasContext2D::fillPath(FillParams& parms, VEGAPath& p)
{
	if (!m_canvas || !m_buffer)
		return OpStatus::OK;

	bool draw_shadow = shouldDrawShadow();

	// If comp-op=src-over && no visible shadow, do normal fillPath to rendertarget
	if (!draw_shadow && m_current_state.compOperation == CANVAS_COMP_SRC_OVER)
	{
		m_vrenderer->setRenderTarget(m_buffer);

		setFillParameters(parms, m_current_state.transform);

		return m_vrenderer->fillPath(&p, m_current_state.clip);
	}

	if (!m_composite_filter)
	{
		VEGAMergeType merge_type = VEGATranslateOperation(m_current_state.compOperation);
		RETURN_IF_ERROR(m_vrenderer->createMergeFilter(&m_composite_filter, merge_type));
	}

	bool zero_preserves = VEGAZeroPreserves(m_current_state.compOperation);

	// Determine geometry extents
	OpRect ar(0, 0, m_buffer->getWidth(), m_buffer->getHeight());
	OpRect shape_bbox = ar;
	if (zero_preserves)
	{
		// If zeros in the source preserve the destination, bound the
		// composite operation to the affected region
		VEGA_FIX bx, by, bw, bh;
		p.getBoundingBox(bx, by, bw, bh);

		int ib_minx = VEGA_TRUNCFIXTOINT(VEGA_FLOOR(bx));
		int ib_miny = VEGA_TRUNCFIXTOINT(VEGA_FLOOR(by));
		int ib_maxx = VEGA_TRUNCFIXTOINT(VEGA_CEIL(bx+bw));
		int ib_maxy = VEGA_TRUNCFIXTOINT(VEGA_CEIL(by+bh));

		shape_bbox.Set(ib_minx, ib_miny, ib_maxx - ib_minx, ib_maxy - ib_miny);

		// The part of the shape that will affect rendering
		ar.IntersectWith(shape_bbox);
	}

	if (draw_shadow)
	{
		// Determine the part of the shape that the shadow generation requires
		calcShadowArea(shape_bbox);

		// The affected rectangle is now the visible part of the shape
		// plus the part that the shadow generation requires.
		ar.UnionWith(shape_bbox);
	}
	else
	{
		// If the affected region is empty we can leave early. Note
		// that this is not necessarily true in general, but can
		// depend on the compositing operation to use and whether we
		// are to paint a shadow or not.
		if (zero_preserves && ar.IsEmpty())
			return OpStatus::OK;
	}

	// Acquire paint surface
	const bool use_stencil = zero_preserves && !draw_shadow && m_current_state.clip;
	VEGARenderTarget* layer = getLayer(ar.width, ar.height, use_stencil);
	if (!layer)
		return OpStatus::ERR_NO_MEMORY;

	// If the affected area (the area/part of the shape that we need
	// to draw) is larger than the render target (and thus probably
	// the renderer), we need to re-initialize the renderer so that it
	// will accept the render target that we are about to use.
	// Since the layer returned by getLayer can be larger than the
	// requested size we need to use the dimensions of the returned
	// layer instead of the dimensions of the affected region.
	unsigned canvas_w = m_buffer->getWidth();
	unsigned canvas_h = m_buffer->getHeight();
	unsigned layer_w = layer->getWidth();
	unsigned layer_h = layer->getHeight();
	if (layer_w > canvas_w || layer_h > canvas_h)
	{
		layer_w = MAX(layer_w, canvas_w);
		layer_h = MAX(layer_h, canvas_h);

		OP_STATUS status = m_vrenderer->Init(layer_w, layer_h);
		if (OpStatus::IsError(status))
		{
			putLayer(layer);
			return status;
		}
	}

	m_vrenderer->setRenderTarget(layer);

	// Clear relevant parts of the offscreen buffer
	m_vrenderer->clear(0, 0, ar.width, ar.height, 0);

	// Adjust path
	VEGATransform xlt;
	xlt.loadTranslate(VEGA_INTTOFIX(-ar.x), VEGA_INTTOFIX(-ar.y));
	p.transform(&xlt);

	xlt.multiply(m_current_state.transform);

	setFillParameters(parms, xlt);

	VEGAStencil* clip = NULL;
	if (use_stencil)
	{
		clip = m_current_state.clip;
		OP_ASSERT(clip);
		clip->setOffset(-ar.x, -ar.y);
	}

	// Paint to offscreen
	OP_STATUS status = m_vrenderer->fillPath(&p, clip);

	if (clip)
		clip->setOffset(0, 0);

	// Undo translation
	xlt.loadTranslate(VEGA_INTTOFIX(ar.x), VEGA_INTTOFIX(ar.y));
	p.transform(&xlt);

	if (OpStatus::IsSuccess(status))
	{
		if (draw_shadow)
		{
			VEGARenderTarget* shadow = NULL;
			if (OpStatus::IsSuccess(createShadow(layer, ar.width, ar.height, &shadow)))
			{
				// Calculate destination
				int shadow_ox = VEGA_TRUNCFIXTOINT(m_current_state.shadowOffsetX);
				int shadow_oy = VEGA_TRUNCFIXTOINT(m_current_state.shadowOffsetY);

				OpRect sh_rect = ar;
				sh_rect.OffsetBy(shadow_ox, shadow_oy);
				OpPoint orig = sh_rect.TopLeft();
				sh_rect.IntersectWith(OpRect(0, 0, m_buffer->getWidth(), m_buffer->getHeight()));

				if (zero_preserves && m_current_state.clip == NULL)
					compositeZPSrc(m_buffer, shadow, sh_rect.x - orig.x, sh_rect.y - orig.y,
								   sh_rect.width, sh_rect.height, sh_rect.x, sh_rect.y);
				else
					compositeGeneric(m_buffer, shadow, sh_rect.x - orig.x, sh_rect.y - orig.y,
									 sh_rect.width, sh_rect.height, sh_rect.x, sh_rect.y);

			}
		}

		OpPoint orig = ar.TopLeft();
		ar.IntersectWith(OpRect(0, 0, m_buffer->getWidth(), m_buffer->getHeight()));

		if (zero_preserves && (!draw_shadow || m_current_state.clip == NULL))
			status = compositeZPSrc(m_buffer, layer, ar.x - orig.x, ar.y - orig.y,
									ar.width, ar.height, ar.x, ar.y);
		else
			status = compositeGeneric(m_buffer, layer, ar.x - orig.x, ar.y - orig.y,
									  ar.width, ar.height, ar.x, ar.y);
	}

	putLayer(layer);

	return status;
}

// Check if a point is inside the current path.
BOOL CanvasContext2D::isPointInPath(double x, double y)
{
	// xor fill is never used
	return m_current_path.isPointInside(DOUBLE_TO_VEGA(x), DOUBLE_TO_VEGA(y), false);
}

void CanvasContext2D::putImageData(int dx, int dy, int sx, int sy, int sw, int sh,
								   UINT8 *data, unsigned datastride)
{
	VEGABackingStore* backingstore = m_buffer->GetBackingStore();

	// Determine the affected region in the backing store
	OpRect dstr(dx + sx, dy + sy, sw, sh);
	dstr.IntersectWith(OpRect(0, 0, backingstore->GetWidth(), backingstore->GetHeight()));

	if (dstr.IsEmpty())
		return;

	VEGASWBuffer* buf = backingstore->BeginTransaction(dstr, VEGABackingStore::ACC_WRITE_ONLY);
	if (!buf)
		return;

	VEGAPixelAccessor dst = buf->GetAccessor(0, 0);
	unsigned dststep = buf->GetPixelStride() - dstr.width;

	// Calculate starting offset in the ImageData buffer
	int src_ofs_x = dstr.x - dx;
	int src_ofs_y = dstr.y - dy;

	const UINT8* src = data + (src_ofs_y * datastride + src_ofs_x) * 4;
	unsigned srcstep = (datastride - dstr.width) * 4;

	for (int yp = 0; yp < dstr.height; ++yp)
	{
		for (int xp = 0; xp < dstr.width; ++xp)
		{
			VEGA_PIXEL ppixel = VEGA_PACK_ARGB(src[3], src[0], src[1], src[2]);
#ifdef USE_PREMULTIPLIED_ALPHA
			ppixel = VEGAPixelPremultiply(ppixel);
#endif // USE_PREMULTIPLIED_ALPHA
			dst.Store(ppixel);

			++dst;
			src += 4;
		}

		src += srcstep;
		dst += dststep;
	}

	backingstore->EndTransaction(TRUE);
}

void CanvasContext2D::getImageData(int src_x, int src_y, int w, int h, UINT8* pixels)
{
	VEGABackingStore* backingstore = m_buffer->GetBackingStore();
	VEGASWBuffer* buf = backingstore->BeginTransaction(VEGABackingStore::ACC_READ_ONLY);
	if (!buf)
		return;

	int left_edge = (src_x < 0) ? MIN(w, -src_x) : 0;
	int top_edge = (src_y < 0) ? MIN(h, -src_y) : 0;
	int right_edge = (src_x + w > (int)buf->width) ? MIN((unsigned)w, (src_x + w) - buf->width) : 0;
	int bottom_edge = (src_y + h > (int)buf->height) ? MIN((unsigned)h, (src_y + h) - buf->height) : 0;
	int bytes_per_line = w * 4;

	if (top_edge)
	{
		op_memset(pixels, 0, top_edge * bytes_per_line);

		pixels += top_edge * bytes_per_line;
		src_y += top_edge;
	}

	int middle_h = MAX(0, h - (top_edge + bottom_edge));
	int middle_w = MAX(0, w - (left_edge + right_edge));

	for (int yp = 0; yp < middle_h; ++yp, ++src_y)
	{
		int curr_x = src_x;

		if (left_edge)
		{
			op_memset(pixels, 0, left_edge * 4);

			pixels += left_edge * 4;
			curr_x += left_edge;
		}

		if (middle_w)
		{
			VEGAConstPixelAccessor pa = buf->GetConstAccessor(curr_x, src_y);

			for (int xp = 0; xp < middle_w; ++xp)
			{
				VEGA_PIXEL ppixel = pa.Load();
#ifdef USE_PREMULTIPLIED_ALPHA
				ppixel = VEGAPixelUnpremultiplyFast(ppixel);
#endif // USE_PREMULTIPLIED_ALPHA
				pixels[0] = VEGA_UNPACK_R(ppixel);
				pixels[1] = VEGA_UNPACK_G(ppixel);
				pixels[2] = VEGA_UNPACK_B(ppixel);
				pixels[3] = VEGA_UNPACK_A(ppixel);

				++pa;
				pixels += 4;
			}
		}

		if (right_edge)
		{
			op_memset(pixels, 0, right_edge * 4);

			pixels += right_edge * 4;
		}
	}

	if (bottom_edge)
		op_memset(pixels, 0, bottom_edge * bytes_per_line);

	backingstore->EndTransaction(FALSE);
}

OP_STATUS CanvasContext2D::drawImage(OpBitmap *bmp, double *src, double *dst)
{
	if (!m_canvas)
		return OpStatus::OK;

#ifdef VEGA_LIMIT_BITMAP_SIZE
	VEGAOpBitmap* vbmp = static_cast<VEGAOpBitmap*>(bmp);
	if (vbmp->isTiled())
		return vbmp->drawCanvasTiled(this, src, dst);
#endif // VEGA_LIMIT_BITMAP_SIZE

	VEGAFill* fill;

	RETURN_IF_ERROR(m_vrenderer->createImage(&fill, bmp));

	OP_STATUS err = drawImage(fill, src, dst);
	OP_DELETE(fill);
	return err;
}

OP_STATUS CanvasContext2D::drawImage(VEGAFill* fill, double *src, double *dst)
{
	if (!m_canvas)
		return OpStatus::OK;

	VEGAPath p;

	RETURN_IF_ERROR(p.moveTo(DOUBLE_TO_VEGA(dst[0]), DOUBLE_TO_VEGA(dst[1])));
	RETURN_IF_ERROR(p.lineTo(DOUBLE_TO_VEGA(dst[0]+dst[2]), DOUBLE_TO_VEGA(dst[1])));
	RETURN_IF_ERROR(p.lineTo(DOUBLE_TO_VEGA(dst[0]+dst[2]), DOUBLE_TO_VEGA(dst[1]+dst[3])));
	RETURN_IF_ERROR(p.lineTo(DOUBLE_TO_VEGA(dst[0]), DOUBLE_TO_VEGA(dst[1]+dst[3])));
	RETURN_IF_ERROR(p.close(true));
#ifdef VEGA_3DDEVICE
	p.forceConvex();
#endif

	p.transform(&m_current_state.transform);

	OP_STATUS err = ((VEGAImage*)fill)->limitArea((int)src[0], (int)src[1], (int)op_ceil(src[0]+src[2])-1, (int)op_ceil(src[1]+src[3])-1);
	if (OpStatus::IsError(err))
	{
		return err;
	}

	FillParams parms;
	parms.paint = NULL;
	parms.image = fill;

	// Setup image transform
	VEGATransform& trans = parms.image_transform;
	VEGATransform tmpt;
	trans.loadTranslate(DOUBLE_TO_VEGA(dst[0]), DOUBLE_TO_VEGA(dst[1]));
	tmpt.loadScale(VEGA_FIXDIV(DOUBLE_TO_VEGA(dst[2]), DOUBLE_TO_VEGA(src[2])),
				   VEGA_FIXDIV(DOUBLE_TO_VEGA(dst[3]), DOUBLE_TO_VEGA(src[3])));
	trans.multiply(tmpt);
	tmpt.loadTranslate(-DOUBLE_TO_VEGA(src[0]), -DOUBLE_TO_VEGA(src[1]));
	trans.multiply(tmpt);

	fill->setSpread(VEGAFill::SPREAD_CLAMP);
	fill->setFillOpacity(m_current_state.alpha);
	setImageQuality(fill);

	fillPath(parms, p);

	m_vrenderer->setFill(NULL);
	return OpStatus::OK;
}
////////////////////////////////////////////////////////////////////////////
// Get and set functions
void CanvasContext2D::setAlpha(double alpha)
{
	if (alpha >= 0. && alpha <= 1.)
		m_current_state.alpha = (int)(255.*alpha+0.5);
}
void CanvasContext2D::setGlobalCompositeOperation(CanvasCompositeOperation comp)
{
	m_current_state.compOperation = comp;
	OP_DELETE(m_composite_filter);
	m_composite_filter = NULL;
}
void CanvasContext2D::setStrokeColor(UINT32 color)
{
	m_current_state.stroke.color = RGB_TO_VEGA(color);
	m_current_state.stroke.pattern = NULL;
	m_current_state.stroke.gradient = NULL;
}
void CanvasContext2D::setStrokeGradient(CanvasGradient *gradient)
{
	m_current_state.stroke.gradient = gradient;
	m_current_state.stroke.pattern = NULL;
}
void CanvasContext2D::setStrokePattern(CanvasPattern *pattern)
{
	m_current_state.stroke.pattern = pattern;
	m_current_state.stroke.gradient = NULL;
}
void CanvasContext2D::setFillColor(UINT32 color)
{
	m_current_state.fill.color = RGB_TO_VEGA(color);
	m_current_state.fill.gradient = NULL;
	m_current_state.fill.pattern = NULL;
}
void CanvasContext2D::setFillGradient(CanvasGradient *gradient)
{
	m_current_state.fill.gradient = gradient;
	m_current_state.fill.pattern = NULL;
}
void CanvasContext2D::setFillPattern(CanvasPattern *pattern)
{
	m_current_state.fill.pattern = pattern;
	m_current_state.fill.gradient = NULL;
}
void CanvasContext2D::setLineWidth(double width)
{
	if (width > 0.)
	{
		m_current_state.lineWidth = DOUBLE_TO_VEGA(width);
	}
}
void CanvasContext2D::setLineCap(CanvasLineCap cap)
{
	switch (cap)
	{
	case CANVAS_LINECAP_ROUND:
		m_current_state.lineCap = VEGA_LINECAP_ROUND;
		break;
	case CANVAS_LINECAP_BUTT:
		m_current_state.lineCap = VEGA_LINECAP_BUTT;
		break;
	case CANVAS_LINECAP_SQUARE:
		m_current_state.lineCap = VEGA_LINECAP_SQUARE;
		break;
	default:
		return;
	}
}
void CanvasContext2D::setLineJoin(CanvasLineJoin join)
{
	switch (join)
	{
	case CANVAS_LINEJOIN_ROUND:
		m_current_state.lineJoin = VEGA_LINEJOIN_ROUND;
		break;
	case CANVAS_LINEJOIN_BEVEL:
		m_current_state.lineJoin = VEGA_LINEJOIN_BEVEL;
		break;
	case CANVAS_LINEJOIN_MITER:
		m_current_state.lineJoin = VEGA_LINEJOIN_MITER;
		break;
	default:
		return;
	}
}
void CanvasContext2D::setMiterLimit(double miter)
{
	if (miter > 0.)
	{
		m_current_state.miterLimit = DOUBLE_TO_VEGA(miter);
	}
}
void CanvasContext2D::setShadowOffsetX(double x)
{
	m_current_state.shadowOffsetX = DOUBLE_TO_VEGA(x);
}
void CanvasContext2D::setShadowOffsetY(double y)
{
	m_current_state.shadowOffsetY = DOUBLE_TO_VEGA(y);
}
void CanvasContext2D::setShadowBlur(double blur)
{
	if (blur >= 0.)
		m_current_state.shadowBlur = DOUBLE_TO_VEGA(blur);
}
void CanvasContext2D::setShadowColor(UINT32 color)
{
	m_current_state.shadowColor = RGB_TO_VEGA(color);
}
#ifdef CANVAS_TEXT_SUPPORT
void CanvasContext2D::setTextBaseline(CanvasTextBaseline baseline)
{
	m_current_state.textBaseline = baseline;
}

void CanvasContext2D::setTextAlign(CanvasTextAlign align)
{
	m_current_state.textAlign = align;
}
#endif // CANVAS_TEXT_SUPPORT

double CanvasContext2D::getAlpha()
{
	return (double)m_current_state.alpha/255.;
}
CanvasContext2D::CanvasCompositeOperation CanvasContext2D::getGlobalCompositeOperation()
{
	return m_current_state.compOperation;
}
UINT32 CanvasContext2D::getStrokeColor()
{
	return VEGA_TO_RGB(m_current_state.stroke.color);
}
CanvasContext2D::CanvasContext2DState *CanvasContext2D::getStateAtDepth(int stackDepth)
{
	CanvasContext2DState *state = stackDepth > 0 ? m_state_stack : &m_current_state;
	for (int i = 1; i < stackDepth; ++i)
	{
		if (!state->next)
			return NULL;
		state = state->next;
	}
	return state;
}
CanvasGradient *CanvasContext2D::getStrokeGradient(int stackDepth)
{
	CanvasContext2DState *state = getStateAtDepth(stackDepth);
	return state ? state->stroke.gradient : NULL;
}
CanvasPattern *CanvasContext2D::getStrokePattern(int stackDepth)
{
	CanvasContext2DState *state = getStateAtDepth(stackDepth);
	return state ? state->stroke.pattern : NULL;
}
UINT32 CanvasContext2D::getFillColor()
{
	return VEGA_TO_RGB(m_current_state.fill.color);
}
CanvasGradient *CanvasContext2D::getFillGradient(int stackDepth)
{
	CanvasContext2DState *state = getStateAtDepth(stackDepth);
	return state ? state->fill.gradient : NULL;
}
CanvasPattern *CanvasContext2D::getFillPattern(int stackDepth)
{
	CanvasContext2DState *state = getStateAtDepth(stackDepth);
	return state ? state->fill.pattern : NULL;
}
double CanvasContext2D::getLineWidth()
{
	return VEGA_TO_DOUBLE(m_current_state.lineWidth);
}
CanvasContext2D::CanvasLineCap CanvasContext2D::getLineCap()
{
	switch (m_current_state.lineCap)
	{
	case VEGA_LINECAP_ROUND:
		return CANVAS_LINECAP_ROUND;
	case VEGA_LINECAP_SQUARE:
		return CANVAS_LINECAP_SQUARE;
	case VEGA_LINECAP_BUTT:
	default:
		return CANVAS_LINECAP_BUTT;
	}
}
CanvasContext2D::CanvasLineJoin CanvasContext2D::getLineJoin()
{
	switch (m_current_state.lineJoin)
	{
	case VEGA_LINEJOIN_ROUND:
		return CANVAS_LINEJOIN_ROUND;
	case VEGA_LINEJOIN_BEVEL:
		return CANVAS_LINEJOIN_BEVEL;
	case VEGA_LINEJOIN_MITER:
	default:
		return CANVAS_LINEJOIN_MITER;
	}
}
double CanvasContext2D::getMiterLimit()
{
	return VEGA_TO_DOUBLE(m_current_state.miterLimit);
}
double CanvasContext2D::getShadowOffsetX()
{
	return VEGA_TO_DOUBLE(m_current_state.shadowOffsetX);
}
double CanvasContext2D::getShadowOffsetY()
{
	return VEGA_TO_DOUBLE(m_current_state.shadowOffsetY);
}
double CanvasContext2D::getShadowBlur()
{
	return VEGA_TO_DOUBLE(m_current_state.shadowBlur);
}
UINT32 CanvasContext2D::getShadowColor()
{
	return VEGA_TO_RGB(m_current_state.shadowColor);
}
#ifdef CANVAS_TEXT_SUPPORT
CSS_property_list* CanvasContext2D::getFont()
{
	return m_current_state.font;
}
CanvasContext2D::CanvasTextBaseline CanvasContext2D::getTextBaseline()
{
	return m_current_state.textBaseline;
}
CanvasContext2D::CanvasTextAlign CanvasContext2D::getTextAlign()
{
	return m_current_state.textAlign;
}
#endif // CANVAS_TEXT_SUPPORT
Canvas *CanvasContext2D::getCanvas()
{
	return m_canvas;
}
int CanvasContext2D::getStateStackDepth()
{
	int depth = 1;
	CanvasContext2DState *s = m_state_stack;
	while (s)
	{
		++depth;
		s = s->next;
	}
	return depth;
}

/* static */ unsigned int
CanvasContext2D::allocationCost(CanvasContext2D *context)
{
	unsigned int cost = sizeof(CanvasContext2D);
	CanvasContext2DState *state = &context->m_current_state;
	while (state)
	{
		cost += sizeof(CanvasContext2DState);
		if (VEGAStencil *clip = state->clip)
			cost += sizeof(VEGAStencil) + clip->getWidth() * clip->getHeight();

		if (state != &context->m_current_state)
			state = state->next;
		else
			state = context->m_state_stack;
	}

	return cost;
}
#endif // CANVAS_SUPPORT

