/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef VEGA_SUPPORT
#include "modules/libvega/src/vegagradient.h"
#include "modules/libvega/vegarenderer.h"
#include "modules/libvega/vegapath.h"

#include "modules/libvega/vegatransform.h"
#include "modules/libvega/src/vegacomposite.h"
#include "modules/libvega/src/vegapixelformat.h"
#include "modules/libvega/src/vegaimage.h"

#include "modules/libvega/src/vegabackend_sw.h"
#include "modules/libvega/src/vegabackend_hw3d.h"

VEGAGradient::VEGAGradient() :
	stopOffsets(NULL), stopColors(NULL), stopInvDists(NULL), numStops(0)
#ifdef VEGA_3DDEVICE
	, m_texture(NULL), m_shader(NULL), m_blend_mode(NULL), m_vlayout(NULL)
#endif // VEGA_3DDEVICE
{
	has_calculated_invdists = 0;
	is_simple_radial = 0;
	is_premultiplied = 0;
	is_linear = 1;
}

VEGAGradient::~VEGAGradient()
{
	OP_DELETEA(stopOffsets);
	OP_DELETEA(stopColors);
	OP_DELETEA(stopInvDists);
#ifdef VEGA_3DDEVICE
	VEGARefCount::DecRef(m_texture);
	VEGARefCount::DecRef(m_vlayout);
	VEGARefCount::DecRef(m_shader);
#endif
}

#ifdef VEGA_3DDEVICE
const VEGATransform& VEGAGradient::getTexTransform()
{
	OP_ASSERT(m_texture);
	if (is_linear)
	{
		m_texTransform[0] = linear.x*m_texture->getWidth();
		m_texTransform[1] = linear.y*m_texture->getWidth();
		m_texTransform[2] = linear.dist*m_texture->getWidth();
		m_texTransform[3] = 0;
		m_texTransform[4] = 0;
		m_texTransform[5] = m_texture_line + VEGA_FIXDIV2(VEGA_INTTOFIX(1));
	}
	else
	{
		m_texTransform[0] = VEGA_INTTOFIX(m_texture->getWidth());
		m_texTransform[1] = 0;
		m_texTransform[2] = 0;
		m_texTransform[3] = 0;
		m_texTransform[4] = VEGA_INTTOFIX(m_texture->getHeight());
		m_texTransform[5] = 0;
	}
	m_texTransform.multiply(invPathTransform);
	return m_texTransform;
}

VEGA3dTexture* VEGAGradient::getTexture()
{
	if (!m_texture)
	{
		VEGA3dDevice* dev = g_vegaGlobals.vega3dDevice;
		// TODO: ideally the texture size would not be hardcoded but tweakable and maybe even have a calculated required size
		unsigned int gradientTextureSize = MIN(dev->getMaxTextureSize(), 1024);
		if (!g_vegaGlobals.m_gradient_texture)
		{
			RETURN_VALUE_IF_ERROR(dev->createTexture(&g_vegaGlobals.m_gradient_texture, gradientTextureSize, gradientTextureSize, VEGA3dTexture::FORMAT_RGBA8888, false, false), NULL);
			g_vegaGlobals.m_gradient_texture_next_line = 0;
		}
		m_texture = g_vegaGlobals.m_gradient_texture;
		VEGARefCount::IncRef(m_texture);
		m_texture_line = g_vegaGlobals.m_gradient_texture_next_line;
		g_vegaGlobals.m_gradient_texture_next_line++;
		if (g_vegaGlobals.m_gradient_texture_next_line >= g_vegaGlobals.m_gradient_texture->getHeight())
		{
			VEGARefCount::DecRef(g_vegaGlobals.m_gradient_texture);
			g_vegaGlobals.m_gradient_texture = NULL;
			g_vegaGlobals.m_gradient_texture_next_line = 0;
		}
		VEGASWBuffer buf;
		if (OpStatus::IsError(buf.Create(gradientTextureSize, 1)))
		{
			VEGARefCount::DecRef(m_texture);
			m_texture = NULL;
			return NULL;
		}
		unsigned int stop = 0;
		int ca, cr, cg, cb;
		ca = stopColors[0]>>24;
		cr = (stopColors[0]>>16)&0xff;
		cg = (stopColors[0]>>8)&0xff;
		cb = stopColors[0]&0xff;
		VEGAPixelAccessor grad = buf.GetAccessor(0, 0);
		for (unsigned int i = 0; i < gradientTextureSize; ++i)
		{
			// Last pixel should map to offset 1.0
			VEGA_FIX offset = VEGA_INTTOFIX(i)/(gradientTextureSize-1);
			while (stop < numStops-1 && offset >= stopOffsets[stop+1])
				++stop;

			ca = stopColors[stop]>>24;
			cr = (stopColors[stop]>>16)&0xff;
			cg = (stopColors[stop]>>8)&0xff;
			cb = stopColors[stop]&0xff;

			if (stop+1 < numStops && offset >= stopOffsets[stop])
			{
				// interpolate between the two stops
				int ca2, cr2, cg2, cb2;
				ca2 = stopColors[stop+1]>>24;
				cr2 = (stopColors[stop+1]>>16)&0xff;
				cg2 = (stopColors[stop+1]>>8)&0xff;
				cb2 = stopColors[stop+1]&0xff;

				unsigned int frac = (unsigned int)(VEGA_FIXDIV((offset-stopOffsets[stop]), (stopOffsets[stop+1]-stopOffsets[stop])) * (1<<16));
				ca = ((ca<<16) + (ca2-ca)*frac + (1<<15))>>16;
				cr = ((cr<<16) + (cr2-cr)*frac + (1<<15))>>16;
				cg = ((cg<<16) + (cg2-cg)*frac + (1<<15))>>16;
				cb = ((cb<<16) + (cb2-cb)*frac + (1<<15))>>16;
			}

			grad.StoreARGB(ca, cr, cg, cb);
			++grad;
		}
		m_texture->update(0, m_texture_line, &buf);
		buf.Destroy();
	}
	VEGA3dTexture::WrapMode mode;
	switch (xspread)
	{
	case SPREAD_REPEAT:
		mode = VEGA3dTexture::WRAP_REPEAT;
		break;
	case SPREAD_REFLECT:
		mode = VEGA3dTexture::WRAP_REPEAT_MIRROR;
		break;
	case SPREAD_CLAMP:
	case SPREAD_CLAMP_BORDER:
	default:
		mode = VEGA3dTexture::WRAP_CLAMP_EDGE;
		break;
	}
	m_texture->setWrapMode(mode, mode);
	return m_texture;
}

VEGA3dShaderProgram* VEGAGradient::getCustomShader()
{
	VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;
	if (!is_linear)
	{
		if (!m_shader)
		{
			if (radial.r0 < VEGA_EPSILON)
			{
				RETURN_VALUE_IF_ERROR(device->createShaderProgram(&m_shader, VEGA3dShaderProgram::SHADER_VECTOR2DTEXGENRADIALSIMPLE, VEGA3dShaderProgram::WRAP_CLAMP_CLAMP), NULL);
				if (OpStatus::IsError(device->createVega2dVertexLayout(&m_vlayout, VEGA3dShaderProgram::SHADER_VECTOR2DTEXGENRADIALSIMPLE)))
				{
					VEGARefCount::DecRef(m_shader);
					m_shader = NULL;
					return NULL;
				}
			}
			else
			{
				RETURN_VALUE_IF_ERROR(device->createShaderProgram(&m_shader, VEGA3dShaderProgram::SHADER_VECTOR2DTEXGENRADIAL, VEGA3dShaderProgram::WRAP_CLAMP_CLAMP), NULL);
				if (OpStatus::IsError(device->createVega2dVertexLayout(&m_vlayout, VEGA3dShaderProgram::SHADER_VECTOR2DTEXGENRADIAL)))
				{
					VEGARefCount::DecRef(m_shader);
					m_shader = NULL;
					return NULL;
				}
			}
		}
		if (!m_texture)
		{
			// We need m_texture to set uSrcY correctly.
			getTexture();
			if (!m_texture)
				return NULL;
		}
		device->setShaderProgram(m_shader);
		float foo[3];
		foo[0] = radial.x0;
		foo[1] = radial.y0;
		foo[2] = radial.r0;
		m_shader->setVector3(m_shader->getConstantLocation("uFocusPoint"), foo);
		foo[0] = radial.x1;
		foo[1] = radial.y1;
		foo[2] = radial.r1;
		m_shader->setVector3(m_shader->getConstantLocation("uCenterPoint"), foo);
		m_shader->setScalar(m_shader->getConstantLocation("uSrcY"), (float(m_texture_line) + 0.5f) / m_texture->getHeight());
	}
	return m_shader;
}

VEGA3dVertexLayout* VEGAGradient::getCustomVertexLayout()
{
	return m_vlayout;
}

#endif // VEGA_3DDEVICE

class RadialGradientRampMapper
{
public:
	RadialGradientRampMapper(UINT32* colors, VEGA_FIX* offsets, UINT32* inv_dists,
							 unsigned num_stops, VEGAFill::Spread spread,
							 UINT32 border_color) :
		m_colors(colors),
		m_offsets(offsets),
		m_inv_dists(inv_dists),
		m_num_stops(num_stops),
		m_current_stop(0),
		m_spread(spread),
		m_border_color(border_color) {}

	op_force_inline UINT32 ComputeColor(VEGA_FIX offset)
	{
		// For offsets that are outside the normalized range apply the
		// spread mode.
		if (offset >= VEGA_INTTOFIX(1))
		{
			switch (m_spread)
			{
				// clamp to border || last
				// FIXME: Find where the outer circle is intersected
				// for more optimal clamping.
			case VEGAFill::SPREAD_CLAMP:
				return m_colors[m_num_stops-1];

			case VEGAFill::SPREAD_CLAMP_BORDER:
				return m_border_color;

			default:
				VEGA_FIX offset_int = VEGA_FLOOR(offset);
				offset -= offset_int;

				if (m_spread == VEGAFill::SPREAD_REFLECT)
				{
					int gradnum = VEGA_TRUNCFIXTOINT(offset_int);
					if (gradnum & 1)
						offset = VEGA_INTTOFIX(1) - offset;
				}
			}
		}

		// Find the nearest positive intersection with a stop offset or 0,1 to use as length
		// find which "slot" this is in.
		while (m_current_stop > 0 && m_offsets[m_current_stop-1] > offset)
			--m_current_stop;
		while (m_current_stop < m_num_stops && m_offsets[m_current_stop] < offset)
			++m_current_stop;

		if (m_current_stop == 0)
			return m_colors[0];
		if (m_current_stop >= m_num_stops)
			return m_colors[m_num_stops-1];

		UINT32 fp_invstopdist = m_inv_dists[m_current_stop-1];
		INT32 fp_pos = VEGA_FIXTOSCALEDINT_T(offset - m_offsets[m_current_stop-1], 16);

		// Check which one is intersected in positive direction.
		// [16:16] * [18:14] => [34:30]
		UINT32 local_pos = ((fp_pos * fp_invstopdist) + (1 << 21)) >> 22;
		if (local_pos == 0)
			return m_colors[m_current_stop-1];
		if (local_pos >= 256)
			return m_colors[m_current_stop];

		// Linear interpolation between stop-1 and stop.
		UINT32 c_rb = m_colors[m_current_stop-1]&0xff00ff;
		UINT32 c_ag = (m_colors[m_current_stop-1]>>8)&0xff00ff;
		UINT32 diff_rb = m_colors[m_current_stop]&0xff00ff;
		UINT32 diff_ag = (m_colors[m_current_stop]>>8)&0xff00ff;
		diff_rb -= c_rb;
		diff_ag -= c_ag;
		diff_rb *= local_pos;
		diff_ag *= local_pos;
		diff_rb >>= 8;
		diff_ag >>= 8;
		c_rb += diff_rb;
		c_ag += diff_ag;
		c_rb &= 0xff00ff;
		c_ag &= 0xff00ff;
		return (c_ag << 8) | c_rb;
	}

private:
	UINT32* m_colors;
	VEGA_FIX* m_offsets;
	UINT32* m_inv_dists;
	unsigned m_num_stops;
	unsigned m_current_stop;
	VEGAFill::Spread m_spread;
	UINT32 m_border_color;
};

OP_STATUS VEGAGradient::prepare()
{
	// Compute parameters that are invariant for the entire primitive.
	if (is_linear)
	{
		// Linear gradient
	}
	else
	{
		if (!has_calculated_invdists)
		{
			for (unsigned int s = 0; s < numStops-1; ++s)
			{
				UINT32 dist = VEGA_FIXTOSCALEDINT_T(stopOffsets[s+1] - stopOffsets[s], 16);
				stopInvDists[s] = dist ? (1 << (16+14)) / dist : 0;
			}

			stopInvDists[numStops-1] = 0;
			has_calculated_invdists = true;
		}

		// Radial gradient
		invariant.cdx = radial.x1 - radial.x0;
		invariant.cdy = radial.y1 - radial.y0;
		invariant.cdr = radial.r1 - radial.r0;

		invariant.delta_x = invPathTransform[0];
		invariant.delta_y = invPathTransform[3];

		if (is_simple_radial)
		{
			VEGA_FIX r1_rcp = VEGA_FIXDIV(VEGA_INTTOFIX(1), radial.r1);
			invariant.norm = r1_rcp;

			invariant.delta_x = VEGA_FIXMUL(invariant.delta_x, r1_rcp);
			invariant.delta_y = VEGA_FIXMUL(invariant.delta_y, r1_rcp);

			invariant.circular =
				VEGA_ABS(invariant.cdx) < VEGA_EPSILON &&
				VEGA_ABS(invariant.cdy) < VEGA_EPSILON;

			invariant.focus_at_border = false;

			if (!invariant.circular)
			{
				invariant.cdx = VEGA_FIXMUL(invariant.cdx, r1_rcp);
				invariant.cdy = VEGA_FIXMUL(invariant.cdy, r1_rcp);

				// FIXME: there might be some way to make the focus
				// point on border cases work without this hack.
				// If the focus point is at the outer 10% of the
				// gradient there might be some issues.
				invariant.focus_at_border =
					VEGA_FIXSQR(invariant.cdx) + VEGA_FIXSQR(invariant.cdy) > (VEGA_INTTOFIX(9*9)/(10*10));
			}

			if (invariant.circular)
			{
				invariant.dbq = 0;
				invariant.dpq = 0;
			}
			else
			{
				invariant.dbq =
					VEGA_FIXMUL(invariant.cdx, invariant.delta_x) +
					VEGA_FIXMUL(invariant.cdy, invariant.delta_y);
				invariant.dpq =
					VEGA_FIXMUL(invariant.cdx, invariant.delta_y) -
					VEGA_FIXMUL(invariant.cdy, invariant.delta_x);
			}
		}
		else /* complex radial */
		{
			// Try to keep the values smaller by normalizing based on
			// the larger of the two radii (they should be > 0).
			VEGA_FIX max_r_rcp = VEGA_FIXDIV(VEGA_INTTOFIX(1), MAX(radial.r0, radial.r1));
			invariant.norm = max_r_rcp;

			invariant.cdx = VEGA_FIXMUL(invariant.cdx, max_r_rcp);
			invariant.cdy = VEGA_FIXMUL(invariant.cdy, max_r_rcp);
			invariant.cdr = VEGA_FIXMUL(invariant.cdr, max_r_rcp);

			invariant.delta_x = VEGA_FIXMUL(invariant.delta_x, max_r_rcp);
			invariant.delta_y = VEGA_FIXMUL(invariant.delta_y, max_r_rcp);

			invariant.aq =
				VEGA_FIXSQR(invariant.cdx) + VEGA_FIXSQR(invariant.cdy) -
				VEGA_FIXSQR(invariant.cdr);

			invariant.dbq =
				VEGA_FIXMUL(invariant.cdx, invariant.delta_x) +
				VEGA_FIXMUL(invariant.cdy, invariant.delta_y);
		}
	}
	return OpStatus::OK;
}

class GradientBuffer
{
public:
	void Reset(unsigned max_pixels)
	{
		count = 0;
		remaining = MIN(ARRAY_SIZE(pixels), max_pixels);
		ored = 0;
		anded = ~0u;
	}

	void AddPixel(UINT32 pix)
	{
		pixels[count++] = pix;
		ored |= pix;
		anded &= pix;
		remaining--;
	}

	bool IsFilled() const { return remaining == 0; }
	unsigned Count() const { return count; }

	void Output(VEGAPixelAccessor& color, const UINT8*& alpha, bool is_premultiplied)
	{
		if (alpha)
		{
			OutputMasked(color, alpha, count, is_premultiplied);

			alpha += count;
		}
		else
			OutputOpaque(color, count, is_premultiplied);

		color += count;
	}

private:
	void OutputMasked(VEGAPixelAccessor dst, const UINT8* mask, unsigned cnt, bool is_premultiplied);
	void OutputOpaque(VEGAPixelAccessor dst, unsigned cnt, bool is_premultiplied);

	UINT32 pixels[32]; /* ARRAY OK 2012-03-09 fs */
	UINT32 anded;
	UINT32 ored;
	unsigned count;
	unsigned remaining;
};

void GradientBuffer::OutputMasked(VEGAPixelAccessor dst, const UINT8* mask,
								  unsigned cnt, bool is_premultiplied)
{
	if ((ored >> 24) == 0)
		return;

	const UINT32* b = pixels;
	while (cnt-- > 0)
	{
		UINT32 grad_color = *b++;
		unsigned sa = grad_color >> 24;
		unsigned sr = (grad_color >> 16) & 0xff;
		unsigned sg = (grad_color >> 8) & 0xff;
		unsigned sb = grad_color & 0xff;

		UINT32 m = *mask++;
		sa = (sa * (m+1)) >> 8;

		if (is_premultiplied)
		{
			sr = (sr * (m+1)) >> 8;
			sg = (sg * (m+1)) >> 8;
			sb = (sb * (m+1)) >> 8;
		}

		if (sa == 255)
		{
			dst.StoreRGB(sr, sg, sb);
		}
		else if (sa > 0)
		{
			if (is_premultiplied)
			{
				VEGA_PIXEL cpix = VEGA_PACK_ARGB(sa, sr, sg, sb);
#ifndef USE_PREMULTIPLIED_ALPHA
				cpix = VEGAPixelUnpremultiplyFast(cpix);
#endif // !USE_PREMULTIPLIED_ALPHA

				dst.Store(VEGACompOver(dst.Load(), cpix));
			}
			else
			{
				dst.Store(VEGACompOverUnpacked(dst.Load(), sa, sr, sg, sb));
			}
		}

		dst++;
	}
}

void GradientBuffer::OutputOpaque(VEGAPixelAccessor dst, unsigned cnt,
								  bool is_premultiplied)
{
	if ((ored >> 24) == 0)
		return;

	if ((anded >> 24) == 0xff)
	{
		const UINT32* b = pixels;
		while (cnt-- > 0)
		{
			UINT32 grad_color = *b++;
			unsigned sr = (grad_color >> 16) & 0xff;
			unsigned sg = (grad_color >> 8) & 0xff;
			unsigned sb = grad_color & 0xff;

			dst.StoreRGB(sr, sg, sb);

			dst++;
		}
	}
	else
	{
		const UINT32* b = pixels;
		while (cnt-- > 0)
		{
			UINT32 grad_color = *b++;
			unsigned sa = grad_color >> 24;
			unsigned sr = (grad_color >> 16) & 0xff;
			unsigned sg = (grad_color >> 8) & 0xff;
			unsigned sb = grad_color & 0xff;

			if (sa == 255)
			{
				dst.StoreRGB(sr, sg, sb);
			}
			else if (sa > 0)
			{
				if (is_premultiplied)
				{
					VEGA_PIXEL cpix = VEGA_PACK_ARGB(sa, sr, sg, sb);
#ifndef USE_PREMULTIPLIED_ALPHA
					cpix = VEGAPixelUnpremultiplyFast(cpix);
#endif // !USE_PREMULTIPLIED_ALPHA

					dst.Store(VEGACompOver(dst.Load(), cpix));
				}
				else
				{
					dst.Store(VEGACompOverUnpacked(dst.Load(), sa, sr, sg, sb));
				}
			}

			dst++;
		}
	}
}

/*
 * Base formulas for gradients:
 *
 * Linear {(x1,y1), (x2,y2)}:
 *
 *                dx * (x - x1) + dy * (y - y1)
 *  offset(x,y) = -----------------------------
 *                        dx^2 + dy^2
 *
 *      Where: dx = x2 - x1, dy = y2 - y1
 *      The invariant parts of the expression are precalculated in
 *      the initLinear() method.
 *
 * Radial {(x0,y0,r0), (x1,y1,r1)}:
 *
 *                b +/- sqrt(b^2 - a * c)
 *  offset(x,y) = ----------------------- {a != 0, b^2 - a * c >= 0},
 *                           a
 *
 *           or   c / (2 * b)             {a == 0, b != 0}
 *
 *           remaining cases yield transparent black
 *
 *      Where: a = cdx^2 + cdy^2 - cdr^2 {invariant}
 *             b = pdx * cdx + pdy * cdy + r0 * cdr
 *             c = pdx^2 + pdy^2 - r0^2
 *
 *             cdx = x1 - x0, cdy = y1 - y0, cdr = r1 - r0
 *             pdx = px - x0, pdy = py - y0
 *
 *  With: r0 == 0 (fx<=>x0, fy<=>y0, cx<=>x1, cy<=>y1, r<=>r1)
 *
 *                           pdp
 *  offset(x,y) = -----------------------------
 *                pdc + sqrt(r^2 * pdp - ppc^2)
 *
 *             ppc = pdx * cdy - pdy * cdx
 *             pdc = pdy * cdy + pdx * cdx
 *             pdp = pdx^2 + pdy^2
 *
 *  With: x0 == x1 && y0 == y1 / cdx == cdy == 0
 *
 *  offset(x,y) = sqrt((pdx / r)^2 + (pdy / r)^2)
 *
 */
void VEGAGradient::apply(VEGAPixelAccessor color, struct VEGASpanInfo& span)
{
	int firstPixel = span.pos;
	int lastPixel = span.pos + span.length - 1;

	// Calculate start and stop position in gradient space
	VEGA_FIX scan_start_x = VEGA_INTTOFIX(firstPixel);
	VEGA_FIX scan_start_y = VEGA_INTTOFIX(span.scanline);

	VEGA_FIX pointFive = VEGA_FIXDIV2(VEGA_INTTOFIX(1));
	scan_start_x += pointFive;
	scan_start_y += pointFive;

	invPathTransform.apply(scan_start_x, scan_start_y);

	const UINT8* alpha = span.mask;

	if (is_linear)
	{
		// use a plane equation to determine current distance and delta distance
		VEGA_FIX start_offset = VEGA_FIXMUL(scan_start_y, linear.y) + linear.dist;
		start_offset += VEGA_FIXMUL(scan_start_x, linear.x);

		// FIXME: This is invariant and can be calculated in setTransform
		VEGA_FIX offset_delta =
			VEGA_FIXMUL(invPathTransform[0], linear.x) +
			VEGA_FIXMUL(invPathTransform[3], linear.y);

		unsigned int stop = 0;
		while (firstPixel <= lastPixel)
		{
			int gradnum = 0;
			if (start_offset < 0 || start_offset >= VEGA_INTTOFIX(1))
			{
				VEGA_FIX start_offset_int = VEGA_FLOOR(start_offset);
				start_offset -= start_offset_int;
				gradnum = VEGA_TRUNCFIXTOINT(start_offset_int);
			}
			int length = 1+lastPixel-firstPixel;
			INT32 csx, cdx;
			UINT32* data = stopColors;
			if ((xspread == VEGAFill::SPREAD_CLAMP || xspread == VEGAFill::SPREAD_CLAMP_BORDER) && gradnum!=0)
			{
				// clamp to border || last
				start_offset += VEGA_INTTOFIX(gradnum);
				if (start_offset <= 0 && offset_delta > 0)
				{
					VEGA_FIX fixlength = VEGA_FIXDIV(-start_offset, offset_delta);
					int l = VEGA_FIXTOINT(VEGA_FLOOR(fixlength));
					if (l < length)
						length = l;
				}
				if (start_offset >= VEGA_INTTOFIX(1) && offset_delta < 0)
				{
					VEGA_FIX fixlength = VEGA_FIXDIV(VEGA_INTTOFIX(1)-start_offset, offset_delta);
					int l = VEGA_FIXTOINT(VEGA_FLOOR(fixlength));
					if (l < length)
						length = l;
				}
				OP_ASSERT(length >= 0);
				if (length <= 0)
					length = 1;
				csx = 0;
				cdx = 0;
				if (xspread == VEGAFill::SPREAD_CLAMP_BORDER)
					data = &borderColor;
				else if (start_offset > 0)
					csx = (numStops-1)<<16;
			}
			else
			{
				if (xspread == VEGAFill::SPREAD_REFLECT && (gradnum&1))
				{
					// invert everything
					start_offset = VEGA_INTTOFIX(1)-start_offset;
					offset_delta = -offset_delta;
				}
				// set up all the stuff

				// Find the nearest positive intersection with a stop offset or 0,1 to use as length
				// find which "slot" this is in
				while (stop > 0 && stopOffsets[stop-1] > start_offset)
					--stop;
				while (stop < numStops && stopOffsets[stop] <= start_offset)
					++stop;

				VEGA_FIX check_stop_1 = 0;
				VEGA_FIX check_stop_2 = VEGA_INTTOFIX(1);
				if (stop > 0)
					check_stop_1 = stopOffsets[stop-1];
				if (stop < numStops)
					check_stop_2 = stopOffsets[stop];

				// check which one is intersected in positive direction

				// sx = start_offset-stop1 / stop_2-stop_1
				VEGA_FIX stop_dist = check_stop_2 - check_stop_1;
				VEGA_FIX sx = 0;
				if (!VEGA_EQ(stop_dist, 0))
				{
					sx = VEGA_FIXDIV(start_offset - check_stop_1, stop_dist);

					if (sx >= VEGA_INTTOFIX(1))
						// Guard against numeric errors. We expect
						// values in the range [0, 1) here.
						sx = VEGA_INTTOFIX(1) - VEGA_EPSILON;
				}

				// sx + dx * fixlen = 1 || 0
				// dx = offset_delta + (sx / fixlen)
				VEGA_FIX dx = 0;
				if (offset_delta > 0)
				{
					// stop_2 is intersected in positive direction
					// length * offset_delta + start_offset = stop_2
					// length = (stop_2 - start_offset) / offset_delta
					VEGA_FIX fixlength = VEGA_FIXDIV(check_stop_2-start_offset, offset_delta);
					if (fixlength < VEGA_INTTOFIX(length))
						length = VEGA_FIXTOINT(VEGA_FLOOR(fixlength));
					// calculate sx, dx
					if (length > 1)
						dx = VEGA_FIXDIV(VEGA_INTTOFIX(1)-sx, fixlength);
				}
				else if (offset_delta < 0)
				{
					// stop_1 is intersected in positive direction
					VEGA_FIX fixlength = VEGA_FIXDIV(check_stop_1-start_offset, offset_delta);
					if (fixlength < VEGA_INTTOFIX(length))
						length = VEGA_FIXTOINT(VEGA_FLOOR(fixlength));
					// calculate sx, dx
					if (length > 1)
						dx = VEGA_FIXDIV(-sx, fixlength);
				}
				OP_ASSERT(length >= 0);
				if (length <= 0)
					length = 1;

				if (stop == 0)
				{
					csx = 0;
					cdx = 0;
				}
				else if (stop >= numStops)
				{
					csx = (numStops-1)<<16;
					cdx = 0;
				}
				else
				{
					csx = VEGA_FIXTOSCALEDINT_T(sx, 16);
					// Clamp csx because when clamping sx the comparison may have used too much
					// precision internally causing csx to be >= 1 << 16
					if (csx >= 1 << 16)
						csx = (1 << 16) - 1;
					cdx = VEGA_FIXTOSCALEDINT_T(dx, 16);
					csx += (stop-1)<<16;
				}
			}

// Extract color component as 16:16 fixed point
#define VEGA_FIX16_A(c) (((c)&(0xffu<<24))>>8)
#define VEGA_FIX16_R(c) ((c)&(0xff<<16))
#define VEGA_FIX16_G(c) (((c)&(0xff<<8))<<8)
#define VEGA_FIX16_B(c) (((c)&0xff)<<16)

#define VEGA_FIX16_LRINT(v) (((v)+(1<<15))>>16)

			int pofs = csx >> 16;
			INT32 pfrac = csx & 0xffff;
			UINT32 c1 = data[pofs];
			unsigned cnt = length;

			if (cdx == 0)
 			{
				INT32 ca = c1 >> 24;
				INT32 cr = (c1 >> 16) & 0xff;
				INT32 cg = (c1 >> 8) & 0xff;
				INT32 cb = c1 & 0xff;

				if (pfrac != 0)
				{
					UINT32 c2 = data[pofs+1];

					ca = VEGA_FIX16_LRINT((ca << 16) + pfrac * ((c2>>24) - ca));
					cr = VEGA_FIX16_LRINT((cr << 16) + pfrac * (((c2>>16)&0xff) - cr));
					cg = VEGA_FIX16_LRINT((cg << 16) + pfrac * (((c2>>8)&0xff) - cg));
					cb = VEGA_FIX16_LRINT((cb << 16) + pfrac * ((c2&0xff) - cb));
				}

				if (ca == 0xff)
				{
					VEGA_PIXEL cpix = VEGA_PACK_RGB(cr, cg, cb);

					if (span.mask)
					{
						while (cnt-- > 0)
						{
							int a = *alpha++;

							UINT32 d;
							if (a < 255)
								d = VEGACompOverUnpacked(color.Load(), a, cr, cg, cb);
							else
								d = cpix;

							color.Store(d);

							color++;
						}
					}
					else
					{
						color.Store(cpix, cnt);
						color += cnt;
					}
				}
 				else if (is_premultiplied)
 				{
					VEGA_PIXEL cpix = VEGA_PACK_ARGB(ca, cr, cg, cb);

#ifndef USE_PREMULTIPLIED_ALPHA
					cpix = VEGAPixelUnpremultiplyFast(cpix);
#endif // !USE_PREMULTIPLIED_ALPHA

					if (span.mask)
 					{
						VEGACompOverIn(color.Ptr(), cpix, alpha, cnt);

						alpha += cnt;
					}
					else
					{
						VEGACompOver(color.Ptr(), cpix, cnt);
 					}

					color += cnt;
 				}
 				else
 				{
					if (span.mask)
					{
						while (cnt-- > 0)
						{
							int a = *alpha++;
							if (a)
							{
								unsigned mca = ca;
								if (a < 255) // 0 < a < 255
									mca = (mca * (a+1)) >> 8;

								color.Store(VEGACompOverUnpacked(color.Load(), mca, cr, cg, cb));
							}

							color++;
						}
					}
					else
					{
#ifdef USE_PREMULTIPLIED_ALPHA
						cr = (cr * (ca+1)) >> 8;
						cg = (cg * (ca+1)) >> 8;
						cb = (cb * (ca+1)) >> 8;
#endif // USE_PREMULTIPLIED_ALPHA

						VEGA_PIXEL pix = VEGA_PACK_ARGB(ca, cr, cg, cb);
						VEGACompOver(color.Ptr(), pix, cnt);

						color += cnt;
					}
				}
 			}
 			else
 			{
				UINT32 c2 = data[pofs+1];
				INT32 dca = (c2>>24) - (c1>>24);
				INT32 dcr = ((c2>>16)&0xff) - ((c1>>16)&0xff);
				INT32 dcg = ((c2>>8)&0xff) - ((c1>>8)&0xff);
				INT32 dcb = (c2&0xff) - (c1&0xff);

				INT32 ca = VEGA_FIX16_A(c1) + pfrac * dca;
				INT32 cr = VEGA_FIX16_R(c1) + pfrac * dcr;
				INT32 cg = VEGA_FIX16_G(c1) + pfrac * dcg;
				INT32 cb = VEGA_FIX16_B(c1) + pfrac * dcb;

				INT32 cr_inc = cdx * dcr;
				INT32 cg_inc = cdx * dcg;
				INT32 cb_inc = cdx * dcb;

				if (dca == 0 && ca == (255 << 16))
				{
					if (span.mask)
					{
						while (cnt-- > 0)
						{
							int a = *alpha++;
							if (a)
							{
								INT32 icr = VEGA_FIX16_LRINT(cr);
								INT32 icg = VEGA_FIX16_LRINT(cg);
								INT32 icb = VEGA_FIX16_LRINT(cb);

								if (a < 255)
									color.Store(VEGACompOverUnpacked(color.Load(), a, icr, icg, icb));
								else
									color.StoreRGB(icr, icg, icb);
							}

							color++;

							cr += cr_inc;
							cg += cg_inc;
							cb += cb_inc;
						}
					}
					else
					{
						while (cnt-- > 0)
						{
							INT32 icr = VEGA_FIX16_LRINT(cr);
							INT32 icg = VEGA_FIX16_LRINT(cg);
							INT32 icb = VEGA_FIX16_LRINT(cb);

							color.StoreRGB(icr, icg, icb);

							color++;

							cr += cr_inc;
							cg += cg_inc;
							cb += cb_inc;
						}
					}
				}
				else if (is_premultiplied)
				{
					INT32 ca_inc = cdx * dca;

					if (span.mask)
					{
						while (cnt-- > 0)
						{
							int a = *alpha++;
							if (a)
							{
								INT32 ica = VEGA_FIX16_LRINT(ca);
								INT32 icr = VEGA_FIX16_LRINT(cr);
								INT32 icg = VEGA_FIX16_LRINT(cg);
								INT32 icb = VEGA_FIX16_LRINT(cb);

								if (a < 255)
								{
									ica = (ica * (a+1)) >> 8;
									icr = (icr * (a+1)) >> 8;
									icg = (icg * (a+1)) >> 8;
									icb = (icb * (a+1)) >> 8;
								}

								VEGA_PIXEL cpix = VEGA_PACK_ARGB(ica, icr, icg, icb);
#ifndef USE_PREMULTIPLIED_ALPHA
								cpix = VEGAPixelUnpremultiplyFast(cpix);
#endif // !USE_PREMULTIPLIED_ALPHA

								color.Store(VEGACompOver(color.Load(), cpix));
							}

							color++;

							ca += ca_inc;
							cr += cr_inc;
							cg += cg_inc;
							cb += cb_inc;
						}
					}
					else
					{
						while (cnt-- > 0)
						{
							INT32 ica = VEGA_FIX16_LRINT(ca);
							INT32 icr = VEGA_FIX16_LRINT(cr);
							INT32 icg = VEGA_FIX16_LRINT(cg);
							INT32 icb = VEGA_FIX16_LRINT(cb);

							VEGA_PIXEL cpix = VEGA_PACK_ARGB(ica, icr, icg, icb);
#ifndef USE_PREMULTIPLIED_ALPHA
							cpix = VEGAPixelUnpremultiplyFast(cpix);
#endif // !USE_PREMULTIPLIED_ALPHA

							color.Store(VEGACompOver(color.Load(), cpix));

							color++;

							ca += ca_inc;
							cr += cr_inc;
							cg += cg_inc;
							cb += cb_inc;
						}
					}
				}
				else
				{
					INT32 ca_inc = cdx * dca;

					if (span.mask)
					{
						while (cnt-- > 0)
						{
							int a = *alpha++;
							if (a)
							{
								INT32 ica = VEGA_FIX16_LRINT(ca);
								INT32 icr = VEGA_FIX16_LRINT(cr);
								INT32 icg = VEGA_FIX16_LRINT(cg);
								INT32 icb = VEGA_FIX16_LRINT(cb);

								if (a < 255)
									ica = (ica * (a+1)) >> 8;

								color.Store(VEGACompOverUnpacked(color.Load(), ica, icr, icg, icb));
							}

							color++;

							ca += ca_inc;
							cr += cr_inc;
							cg += cg_inc;
							cb += cb_inc;
						}
					}
					else
					{
						while (cnt-- > 0)
						{
							INT32 ica = VEGA_FIX16_LRINT(ca);
							INT32 icr = VEGA_FIX16_LRINT(cr);
							INT32 icg = VEGA_FIX16_LRINT(cg);
							INT32 icb = VEGA_FIX16_LRINT(cb);

							color.Store(VEGACompOverUnpacked(color.Load(), ica, icr, icg, icb));

							color++;

							ca += ca_inc;
							cr += cr_inc;
							cg += cg_inc;
							cb += cb_inc;
						}
					}
				}
			}

#undef VEGA_FIX16_A
#undef VEGA_FIX16_R
#undef VEGA_FIX16_G
#undef VEGA_FIX16_B

#undef VEGA_FIX16_LRINT

			firstPixel += length;
			start_offset += offset_delta*length;
		}
	}
	else // radial
	{
		scan_start_x -= radial.x0;
		scan_start_y -= radial.y0;

		scan_start_x = VEGA_FIXMUL(scan_start_x, invariant.norm);
		scan_start_y = VEGA_FIXMUL(scan_start_y, invariant.norm);

		RadialGradientRampMapper mapper(stopColors, stopOffsets, stopInvDists,
										numStops, xspread, borderColor);
		GradientBuffer buffer;

		if (is_simple_radial)
		{
			if (radial.r1 < VEGA_EPSILON)
			{
				UINT32 c = stopColors[numStops-1];

				UINT8 r = (c>>16)&0xff;
				UINT8 g = (c>>8)&0xff;
				UINT8 b = c&0xff;
				UINT8 a = c>>24;

				VEGA_PIXEL cpix = VEGA_PACK_ARGB(a, r, g, b);

#ifndef USE_PREMULTIPLIED_ALPHA
				cpix = is_premultiplied ? VEGAPixelUnpremultiplyFast(cpix) : cpix;
#else
				cpix = !is_premultiplied ? VEGAPixelPremultiply(cpix) : cpix;
#endif // USE_PREMULTIPLIED_ALPHA

				if (span.mask)
					VEGACompOverIn(color.Ptr(), cpix, span.mask, span.length);
				else
					VEGACompOver(color.Ptr(), cpix, span.length);

				return;
			}

			VEGA_FIX dist = 0; // Will be set below
			bool need_precise_dist = true;

			VEGA_FIX bq, pq;
			if (invariant.circular)
			{
				bq = 0;
				pq = 0;
			}
			else
			{
				bq = VEGA_FIXMUL(invariant.cdx, scan_start_x) + VEGA_FIXMUL(invariant.cdy, scan_start_y);
				pq = VEGA_FIXMUL(invariant.cdx, scan_start_y) - VEGA_FIXMUL(invariant.cdy, scan_start_x);
			}

			unsigned int cnt = span.length;

			while (cnt)
			{
				buffer.Reset(cnt);

				do
				{
					VEGA_FIX distsq = VEGA_FIXSQR(scan_start_x)+VEGA_FIXSQR(scan_start_y);
					VEGA_FIX start_offset;
					if (invariant.circular)
					{
						if (need_precise_dist)
							dist = VEGA_FIXSQRT(distsq);
						else
							dist = VEGA_FIXDIV2(dist)+VEGA_FIXDIV(distsq, dist*2);
						start_offset = dist;
					}
					else
					{
						VEGA_FIX tmp = VEGA_FIXSQR(pq);

						// tmp must never be negative since sqrt(tmp) is run later.
						if (distsq < tmp)
							tmp = 0;
						else
							tmp = distsq - tmp;

						// If the focus point is at the outer 10% the
						// 10% of the radius closest to the focus
						// point is calculated accurately.
						if (need_precise_dist || (invariant.focus_at_border && tmp < (VEGA_INTTOFIX(1)/(10*10))))
							dist = VEGA_FIXSQRT(tmp);
						else
							dist = VEGA_FIXDIV2(dist)+VEGA_FIXDIV(tmp, dist*2);

						start_offset = dist + bq;

						if (start_offset <= VEGA_EPSILON)
							start_offset = VEGA_INTTOFIX(1);
						else
							start_offset = VEGA_FIXDIV(distsq, start_offset);
					}

					need_precise_dist = (dist <= VEGA_EPSILON);

					OP_ASSERT(start_offset >= 0);

					buffer.AddPixel(mapper.ComputeColor(start_offset));

					scan_start_x += invariant.delta_x;
					scan_start_y += invariant.delta_y;

					bq += invariant.dbq;
					pq += invariant.dpq;

				} while (!buffer.IsFilled());

				buffer.Output(color, alpha, is_premultiplied);

				cnt -= buffer.Count();
			}
		}
		else // complex radial
		{
			VEGA_FIX r0 = VEGA_FIXMUL(radial.r0, invariant.norm);
			VEGA_FIX r0_sqr = VEGA_FIXSQR(r0);

			VEGA_FIX bq =
				VEGA_FIXMUL(invariant.cdx, scan_start_x) + VEGA_FIXMUL(invariant.cdy, scan_start_y) +
				VEGA_FIXMUL(invariant.cdr, r0);

			// t = [-bq +/- sqrt(bq^2 - aq * cq)] / aq {aq != 0}
			// t = -cq / bq {aq == 0}
			bool solve_linear = VEGA_EQ(invariant.aq, 0);

			VEGA_FIX rcp_aq = 0;
			if (!solve_linear)
				rcp_aq = VEGA_FIXDIV(VEGA_INTTOFIX(1), invariant.aq);

			unsigned int cnt = span.length;

			while (cnt)
			{
				buffer.Reset(cnt);

				do
				{
					VEGA_FIX start_offset = 0;
					bool has_solution = false;

					VEGA_FIX cq = VEGA_FIXSQR(scan_start_x) + VEGA_FIXSQR(scan_start_y) - r0_sqr;

					if (solve_linear)
					{
						if (!VEGA_EQ(bq, 0))
						{
							VEGA_FIX offset0 = VEGA_FIXDIV(VEGA_FIXDIV2(cq), bq);
							if (VEGA_FIXMUL(offset0, invariant.cdr) > -r0)
								start_offset = offset0, has_solution = true;
						}
					}
					else
					{
						VEGA_FIX bq_sqr = VEGA_FIXSQR(bq);
						VEGA_FIX aq_cq = VEGA_FIXMUL(invariant.aq, cq);

						// Discriminant < 0 => No real solution - <canvas>
						// spec says this should give transparent black as the
						// result.
						if (bq_sqr >= aq_cq)
						{
							VEGA_FIX sqrt_discr = VEGA_FIXSQRT(bq_sqr - aq_cq);

							VEGA_FIX offset0 = bq + sqrt_discr;
							offset0 = VEGA_FIXMUL(offset0, rcp_aq);

							// r(t) > 0 => dr * t + r0 > 0
							if (VEGA_FIXMUL(offset0, invariant.cdr) > -r0)
								start_offset = offset0, has_solution = true;
							else
							{
								VEGA_FIX offset1 = bq - sqrt_discr;
								offset1 = VEGA_FIXMUL(offset1, rcp_aq);

								if (VEGA_FIXMUL(offset1, invariant.cdr) > -r0)
									start_offset = offset1, has_solution = true;
							}
						}
					}

					UINT32 c = 0;
					if (has_solution)
						c = mapper.ComputeColor(start_offset);

					buffer.AddPixel(c);

					scan_start_x += invariant.delta_x;
					scan_start_y += invariant.delta_y;

					bq += invariant.dbq;

				} while (!buffer.IsFilled());

				buffer.Output(color, alpha, is_premultiplied);

				cnt -= buffer.Count();
			}
		}
	}
}

#ifdef VEGA_USE_GRADIENT_CACHE
CachedGradient::~CachedGradient()
{
	OP_DELETE(fill);
}

unsigned CachedGradient::GetMemUsage() const
{
	if (!fill || !fill->GetBackingStore())
		return 0;

	VEGABackingStore* store = fill->GetBackingStore();
	return store->GetBytesPerLine() * store->GetHeight();
}

VEGAGradientCache::Entry::~Entry()
{
	OP_DELETE(cachedGradient);
}

bool VEGAGradientCache::Entry::match(UINT32* ccols, VEGA_FIX* cofs, VEGA_FIX clx, VEGA_FIX cly, UINT32 cnstops, VEGAFill::Spread cspread,
									 VEGA_FIX sx, VEGA_FIX sy, VEGA_FIX w, VEGA_FIX h) const
{
	if (lx == clx && ly == cly && nstops == cnstops && spread == cspread)
	{
		if (op_memcmp(ofs, cofs, nstops*sizeof(VEGA_FIX)) != 0 ||
			op_memcmp(cols, ccols, nstops*sizeof(UINT32)) != 0)
			return false;

		// Check that the requested rectangle is contained within
		// the cached part of the gradient.
		// VEGA_EQ is used to add some tolerance due to precision errors when
		// calculating sx,sy,ex,ey.
		VEGA_FIX ex = sx+w;
		VEGA_FIX ey = sy+h;
		VEGA_FIX cached_ex = cachedGradient->sx + cachedGradient->w;
		VEGA_FIX cached_ey = cachedGradient->sy + cachedGradient->h;
		if ((sx > cachedGradient->sx || VEGA_EQ(sx, cachedGradient->sx)) &&
			(sy > cachedGradient->sy || VEGA_EQ(sy, cachedGradient->sy)) &&
			(ex < cached_ex || VEGA_EQ(ex, cached_ex)) &&
			(ey < cached_ey || VEGA_EQ(ey, cached_ey)))
		{
			return true;
		}
	}

	return false;
}

void VEGAGradientCache::Entry::reset(UINT32* pcols, VEGA_FIX* pofs, VEGA_FIX plx, VEGA_FIX ply, UINT32 pnstops, VEGAFill::Spread pspread, CachedGradient* cached)
{
	// Don't store more stops than we have room for.
	OP_ASSERT(pnstops<=MAX_STOPS);

	OP_DELETE(cachedGradient);
	cachedGradient = cached;

	lx = plx, ly = ply;
	nstops = pnstops;
	spread = pspread;

	op_memcpy(ofs, pofs, nstops*sizeof(VEGA_FIX));
	op_memcpy(cols, pcols, nstops*sizeof(UINT32));
}

CachedGradient* VEGAGradientCache::lookup(UINT32* cols, VEGA_FIX* ofs, VEGA_FIX lx, VEGA_FIX ly, UINT32 nstops, VEGAFill::Spread spread,
										  VEGA_FIX sx, VEGA_FIX sy, VEGA_FIX w, VEGA_FIX h)
{
	Entry* e = m_lru.First();
	while (e)
	{
		if (e->match(cols, ofs, lx, ly, nstops, spread, sx, sy, w, h))
		{
			e->Out();
			e->IntoStart(&m_lru);
			return e->cachedGradient;
		}

		e = e->Suc();
	}
	return NULL;
}

OP_STATUS VEGAGradientCache::insert(UINT32* cols, VEGA_FIX* ofs, VEGA_FIX lx, VEGA_FIX ly, UINT32 nstops, VEGAFill::Spread spread, CachedGradient* cached)
{
	Entry* e = NULL;
	unsigned mem = cached->GetMemUsage();
	unsigned newMemUsed = m_memUsed + mem;

	// Delete as many cached gradients as needed to get newMemUsed below the memory limit.
	while (newMemUsed > MAX_MEM)
	{
		e = m_lru.Last();
		if (!e)
			return OpStatus::ERR;

		unsigned memFreed = e->cachedGradient->GetMemUsage();
		newMemUsed -= memFreed;

		// Check to see if removing this entry from the cache is enough to get us
		// below the memory limit. If it is enough we don't need to actually delete it
		// since it will be replaced by the new gradient further down in this function.
		if (newMemUsed <= MAX_MEM)
			break;

		e->Out();
		OP_DELETE(e);
		e = NULL;
		m_memUsed -= memFreed;
	}

	if (m_lru.Cardinal() == MAX_SIZE || m_memUsed+mem > MAX_MEM)
	{
		e = m_lru.Last();

		e->Out();

		m_memUsed -= e->cachedGradient->GetMemUsage();
	}
	else
	{
		e = OP_NEW(Entry, ());
	}

	if (!e)
		return OpStatus::ERR_NO_MEMORY;

	e->reset(cols, ofs, lx, ly, nstops, spread, cached);

	e->IntoStart(&m_lru);
	m_memUsed += mem;

	return OpStatus::OK;
}

CachedGradient* VEGAGradient::generateCacheImage(VEGARendererBackend* backend,
												 VEGA_FIX gx, VEGA_FIX gy, VEGA_FIX gw, VEGA_FIX gh)
{
	unsigned cached_w, cached_h;

	bool is_horizontal = VEGA_EQ(linear.y, 0);
	bool is_vertical   = VEGA_EQ(linear.x, 0);
	int gradient_res = 128; // Pixel resolution that we want along the gradient vector.

	// Improve output result if some of the stops are really close to each other.
	bool needs_extra_precision = false;
	for (unsigned i = 1; i < numStops; i++)
	{
		if ((stopOffsets[i] - stopOffsets[i-1]) < VEGA_FLTTOFIX(0.1))
		{
			needs_extra_precision = true;
			break;
		}
	}

	if (needs_extra_precision || xspread == SPREAD_REPEAT || xspread == SPREAD_REFLECT)
	{
		// Horizontal and vertical gradients can use more precision with less memory cost than diagonal.
		gradient_res = (is_horizontal || is_vertical) ? 1024 : 256;
	}

	// TODO: Optimize the simplest case when we only have one or two stop and
	// manually construct a scalable gradient consisting of 1 or 2 pixels.
	if (is_horizontal)
	{
		cached_w = gradient_res;
		cached_h = 1;
	}
	else if (is_vertical)
	{
		cached_w = 1;
		cached_h = gradient_res;
	}
	else
	{
		// Normalize the gradient vector so we can extend it to the wanted pixel resolution.
		VEGA_FIX lin_length = VEGA_VECLENGTH(linear.x, linear.y);
		OP_ASSERT(lin_length != 0);
		VEGA_FIX normalized_lin_x = VEGA_FIXDIV(linear.x, lin_length);
		VEGA_FIX normalized_lin_y = VEGA_FIXDIV(linear.y, lin_length);

		// Calculate the width and height we want to cache and try to maintain the wanted
		// resolution along the gradient vector.
		cached_w = VEGA_FIXTOINT(VEGA_ABS(normalized_lin_x * gradient_res));
		cached_h = VEGA_FIXTOINT(VEGA_ABS(normalized_lin_y * gradient_res));
		if (cached_h==0)
			cached_h=1;
		if (cached_w==0)
			cached_w=1;
	}

	// Find out if the gradient is opaque and store that information in the
	// backing store (to potentially avoid allocating an alpha layer) and image
	// (to potentially avoid blending).
	bool is_opaque = true;
	for (unsigned i = 0; i < numStops; i++)
	{
		if (VEGA_UNPACK_A(stopColors[i]) != 255)
		{
			is_opaque = false;
			break;
		}
	}

	VEGABackingStore* store;
	RETURN_VALUE_IF_ERROR(backend->createBitmapStore(&store, cached_w, cached_h, false, is_opaque), NULL);

	store->SetColor(OpRect(0, 0, cached_w, cached_h), 0);

	VEGASWBuffer* buf = store->BeginTransaction(VEGABackingStore::ACC_READ_WRITE);
	if (!buf)
	{
		VEGARefCount::DecRef(store);
		return NULL;
	}

	// Make a backup of the transforms so that we can restore them later.
	VEGATransform backupPath, backupInv;
	backupPath.copy(pathTransform);
	backupInv.copy(invPathTransform);

	// The code below changes the transform so that the gradient will be scaled
	// to fit within the bitmap and then offset so that gx and gy maps to
	// the top left corner of the bitmap. Effectively caching the contents of
	// the gradient area (gx,gy,gw,gh) and storing it in the bitmap.
	//
	// Since the transform does not contain any rotation, calculations are made
	// directly on the transform to increase precision (fixed point benefits from this).
	// If precision in VEGATransform was not an issue the code could be rewritten as:
	// <code>
	//  if (cached_w>1)
	//    scale_x = VEGA_FIXDIV(VEGA_INTTOFIX(cached_w), gw);
	//  if (cached_h>1)
	//    scale_y = VEGA_FIXDIV(VEGA_INTTOFIX(cached_h), gh);
	//  scaleTrans.loadScale(scale_x, scale_y);
	//  pathTransform.multiply(scaleTrans);
	//  xltTrans.loadTranslate(-gx, -gy);
	//  pathTransform.multiply(xltTrans);
	// </code>
	if (cached_w>1)
	{
		VEGA_FIX cw = VEGA_INTTOFIX(cached_w);
		pathTransform[0] = VEGA_FIXMULDIV(pathTransform[0], cw, gw);
		pathTransform[2] = VEGA_FIXMULDIV(pathTransform[2]-gx, cw, gw);
	}
	if (cached_h>1)
	{
		VEGA_FIX ch = VEGA_INTTOFIX(cached_h);
		pathTransform[4] = VEGA_FIXMULDIV(pathTransform[4], ch, gh);
		pathTransform[5] = VEGA_FIXMULDIV(pathTransform[5]-gy, ch, gh);
	}

	invPathTransform.copy(pathTransform);
	invPathTransform.invert();

	// Prepare the span information
	VEGASpanInfo span;
	span.mask = NULL;
	span.pos = 0;
	span.scanline = 0;
	span.length = cached_w;

	// TODO: Possible optimization, make apply(...) aware that it does
	// not need to do any blending when generating a cached image since
	// that part will be done by the user of the cached image anyway.

	VEGAPixelAccessor dst = buf->GetAccessor(0, 0);
	if (cached_h==1)
	{
		// Horizontal
		apply(dst, span);
	}
	else
	{
		unsigned dststride = buf->GetPixelStride();
		while (span.scanline < cached_h)
		{
			apply(dst, span);

			dst += dststride;
			span.scanline++;
		}
	}

	// Restore transform
	pathTransform.copy(backupPath);
	invPathTransform.copy(backupInv);

	store->EndTransaction(TRUE);
	store->Validate();

	VEGAImage* cache_img = OP_NEW(VEGAImage, ());
	if (!cache_img || OpStatus::IsError(cache_img->init(store)))
	{
		OP_DELETE(cache_img);
		cache_img = NULL;
	}

	CachedGradient* cached_grad = NULL;
	if (cache_img)
	{
		cache_img->setOpaque(is_opaque);

		cached_grad = OP_NEW(CachedGradient, ());
		if (cached_grad)
		{
			// Find out what coordinates (in gradient coordinate system) we have cached.
			// The code below is equal to using the invPathTransform like this:
			// invPathTransform.applyVector(gw, gh);
			// invPathTransform.apply(gx, gy);
			// But the precision is better this way and can be done since there is no rotation involved.
			VEGA_FIX sx = VEGA_FIXDIV(gx-pathTransform[2], pathTransform[0]);
			VEGA_FIX sy = VEGA_FIXDIV(gy-pathTransform[5], pathTransform[4]);
			VEGA_FIX grad_width = VEGA_FIXDIV(gw, pathTransform[0]);
			VEGA_FIX grad_height = VEGA_FIXDIV(gh, pathTransform[4]);

			// Since horizontal and vertical gradients can be repeated using stretchblit
			// we can safely say that the cached gradient area in that direction is
			// from -inf/2 to +inf/2.
			VEGA_FIX half_inf = VEGA_FIXDIV2(VEGA_INFINITY);
			cached_grad->fill = cache_img;
			cached_grad->sx = is_vertical   ? -half_inf : sx;
			cached_grad->sy = is_horizontal ? -half_inf : sy;
			cached_grad->w  = is_vertical   ?  VEGA_INFINITY : grad_width;
			cached_grad->h  = is_horizontal ?  VEGA_INFINITY : grad_height;
		}
		else
		{
			OP_DELETE(cache_img);
		}
	}


	VEGARefCount::DecRef(store);
	return cached_grad;
}

VEGAFill* VEGAGradient::getCache(VEGARendererBackend* backend,
								 VEGA_FIX gx, VEGA_FIX gy, VEGA_FIX gw, VEGA_FIX gh)
{
	// Determine if this gradient can be transformed into a simpler
	// operation

	// Only consider linear gradients.
	if (!is_linear || numStops > VEGAGradientCache::Entry::MAX_STOPS)
		return NULL;

	// The fill-transform must be simple enough - aligned only for now
	unsigned mtx_cls = pathTransform.classify();
	unsigned required_flags = VEGA_XFRMCLS_POS_SCALE | VEGA_XFRMCLS_TRANSLATION;

#ifndef VEGA_USE_BLITTER_FOR_NONPIXELALIGNED_IMAGES
	// Require pixel alignment if the blitter can't handle unaligned images
	required_flags |= VEGA_XFRMCLS_INT_TRANSLATION;
#endif

	if ((mtx_cls & required_flags) != required_flags)
		return NULL;

	mtx_cls &= ~(VEGA_XFRMCLS_TRANSLATION | VEGA_XFRMCLS_INT_TRANSLATION | VEGA_XFRMCLS_POS_SCALE);

	// Don't allow anything except scaling
	if (mtx_cls & ~VEGA_XFRMCLS_SCALE)
		return NULL;

	// Lookup in cache
	VEGAGradientCache* grad_cache = g_opera->libvega_module.gradientCache;
	if (!grad_cache)
	{
		grad_cache = OP_NEW(VEGAGradientCache, ());
		if (!grad_cache)
			return NULL;

		g_opera->libvega_module.gradientCache = grad_cache;
	}

	// Transform gx, gy, gw, gh into coordinates in gradient coordinate space
	// The code below is equal to using the invPathTransform like this:
	// invPathTransform.applyVector(gw, gh);
	// invPathTransform.apply(gx, gy);
	// But the precision is better this way and can be done since there is no rotation involved.
	VEGA_FIX sx = VEGA_FIXDIV(gx-pathTransform[2], pathTransform[0]);
	VEGA_FIX sy = VEGA_FIXDIV(gy-pathTransform[5], pathTransform[4]);
	VEGA_FIX grad_width = VEGA_FIXDIV(gw, pathTransform[0]);
	VEGA_FIX grad_height = VEGA_FIXDIV(gh, pathTransform[4]);

	CachedGradient* cached_grad;
	cached_grad = grad_cache->lookup(stopColors, stopOffsets, linear.x, linear.y, numStops, xspread, sx, sy, grad_width, grad_height);
	if (!cached_grad)
	{
		// No hit, generate new
		cached_grad = generateCacheImage(backend, gx, gy, gw, gh);

		if (cached_grad && OpStatus::IsError(grad_cache->insert(stopColors, stopOffsets, linear.x, linear.y, numStops, xspread, cached_grad)))
		{
			OP_DELETE(cached_grad);
			return NULL;
		}
	}

	if (cached_grad)
	{
		bool is_horizontal = VEGA_EQ(linear.y, 0);
		bool is_vertical   = VEGA_EQ(linear.x, 0);
		bool is_diagonal   = !is_horizontal && !is_vertical;

		VEGA_FIX x_trans = 0;
		VEGA_FIX y_trans = 0;
		VEGA_FIX image_scale_x = VEGA_INTTOFIX(1);
		VEGA_FIX image_scale_y = VEGA_INTTOFIX(1);

		// Transfer transforms - "flattening" on the way
		VEGAImage* cfill = cached_grad->fill;
		const VEGABackingStore* bs = cfill->GetBackingStore();

		// Stretch vertically since its a pure horizontal gradient
		if (is_horizontal)
			image_scale_y = gh / (int)bs->GetHeight();

		// Stretch horizontally since its a pure vertical gradient
		if (is_vertical)
			image_scale_x = gw / (int)bs->GetWidth();

		// Calculate horizontal scale and translation
		if (is_horizontal || is_diagonal)
		{
			image_scale_x = VEGA_FIXDIV(VEGA_FIXMUL(gw, cached_grad->w), grad_width * bs->GetWidth());
			x_trans = sx - cached_grad->sx;
		}

		// Calculate vertical scale and translation
		if (is_vertical || is_diagonal)
		{
			image_scale_y = VEGA_FIXDIV(VEGA_FIXMUL(gh, cached_grad->h), grad_height * bs->GetHeight());
			y_trans = sy - cached_grad->sy;
		}

		// Get the translation in pixel coordinates
		pathTransform.applyVector(x_trans, y_trans);
		x_trans = gx-x_trans;
		y_trans = gy-y_trans;

		// Prepare the new transform
		VEGATransform trans, itrans, scaleTrans;
		trans.loadTranslate(x_trans, y_trans);
		scaleTrans.loadScale(image_scale_x, image_scale_y);
		trans.multiply(scaleTrans);
		itrans.copy(trans);
		itrans.invert();
		cfill->setTransform(trans, itrans);

		// Transfer opacity
		cfill->setFillOpacity(getFillOpacity());

		// Set spread depending on gradient vector (or, just clamp in both directions =))
		cfill->setSpread(SPREAD_CLAMP, SPREAD_CLAMP);
		return cfill;
	}
	return NULL;
}
#endif // VEGA_USE_GRADIENT_CACHE

OP_STATUS VEGAGradient::initLinear(unsigned int numStops,
								   VEGA_FIX x1, VEGA_FIX y1, VEGA_FIX x2, VEGA_FIX y2,
								   bool premultiply_stops)
{
	reset();
	has_calculated_invdists = false;
	is_premultiplied = premultiply_stops;

	// Allow for multiple inits
	OP_DELETEA(stopOffsets);
	OP_DELETEA(stopColors);
	OP_DELETEA(stopInvDists);
	this->numStops = numStops;
	stopOffsets = OP_NEWA(VEGA_FIX, numStops);
	stopColors = OP_NEWA(unsigned int, numStops);
	stopInvDists = NULL;

	if (!stopOffsets || !stopColors)
		return OpStatus::ERR_NO_MEMORY;

	// create the plane equation
	is_linear = true;
	linear.x = x2-x1;
	linear.y = y2-y1;
	VEGA_DBLFIX div = VEGA_DBLFIXADD(VEGA_FIXMUL_DBL(linear.x, linear.x), VEGA_FIXMUL_DBL(linear.y, linear.y));
	if (VEGA_DBLFIXSIGN(div) == 0)
	{
		linear.x = 0;
		linear.y = 0;
	}
	else
	{
		linear.x = VEGA_DBLFIXTOFIX(VEGA_DBLFIXDIV(VEGA_FIXTODBLFIX(linear.x), div));
		linear.y = VEGA_DBLFIXTOFIX(VEGA_DBLFIXDIV(VEGA_FIXTODBLFIX(linear.y), div));
	}
	linear.dist = -(VEGA_FIXMUL(linear.x, x1)+VEGA_FIXMUL(linear.y, y1));

	return OpStatus::OK;
}

OP_STATUS VEGAGradient::initRadial(unsigned int numStops,
								   VEGA_FIX x1, VEGA_FIX y1, VEGA_FIX r1,
								   VEGA_FIX x0, VEGA_FIX y0, VEGA_FIX r0,
								   bool premultiply_stops)
{
	reset();
	has_calculated_invdists = false;
	is_premultiplied = premultiply_stops;

	// Allow for multiple inits
	OP_DELETEA(stopOffsets);
	OP_DELETEA(stopColors);
	OP_DELETEA(stopInvDists);
	this->numStops = numStops;
	stopOffsets = OP_NEWA(VEGA_FIX, numStops);
	stopColors = OP_NEWA(unsigned int, numStops);
	stopInvDists = OP_NEWA(UINT32, numStops);

	if (!stopOffsets || !stopColors || !stopInvDists)
		return OpStatus::ERR_NO_MEMORY;

	is_linear = false;

	radial.x1 = x1;
	radial.y1 = y1;
	radial.r1 = r1;
	radial.x0 = x0;
	radial.y0 = y0;
	radial.r0 = r0;

	is_simple_radial = radial.r0 == 0 &&
		VEGA_FIXSQR(radial.x1 - radial.x0) + VEGA_FIXSQR(radial.y1 - radial.y0) <= VEGA_FIXSQR(radial.r1);
	return OpStatus::OK;
}

void VEGAGradient::setStop(unsigned int stopNum, VEGA_FIX offset, unsigned long color)
{
	if (numStops > MAX_STOPS)
		return;

	if (is_premultiplied)
		color = VEGAPixelPremultiply_BGRA8888((UINT32)color);

	stopOffsets[stopNum] = offset;
	stopColors[stopNum] = (unsigned int)color;
	// FIXME: make sure the list of offsets is sorted
}

#endif // VEGA_SUPPORT
