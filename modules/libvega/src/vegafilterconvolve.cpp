/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef VEGA_SUPPORT
#include "modules/libvega/src/vegafilterconvolve.h"
#include "modules/libvega/src/vegapixelformat.h"

#define VEGA_CLAMP_U8(v) (((v) <= 255) ? (((v) >= 0) ? (v) : 0) : 255)

VEGAFilterConvolve::VEGAFilterConvolve() :
	kern_w(0), kern_h(0),
	kern_cx(0), kern_cy(0), kernel(NULL),
	divisor(VEGA_INTTOFIX(1)), bias(VEGA_INTTOFIX(0)),
	edge_mode(VEGAFILTEREDGE_NONE), preserve_alpha(TRUE)
{
}

OP_STATUS VEGAFilterConvolve::setKernel(VEGA_FIX* kern_data, int kern_width, int kern_height)
{
	if (!kern_data || kern_width <= 0 || kern_height <= 0)
		return OpStatus::ERR;

	if (kern_w != kern_width || kern_h != kern_height)
	{
		OP_DELETEA(kernel);

		kern_w = kern_width;
		kern_h = kern_height;
		kernel = OP_NEWA_WH(VEGA_FIX, kern_w, kern_h);

		if (!kernel)
			return OpStatus::ERR_NO_MEMORY;
	}

	int kern_size = kern_w * kern_h;
	for (int i = 0; i < kern_size; i++)
	{
		kernel[i] = kern_data[i];
	}

	return OpStatus::OK;
}

#ifdef VEGA_3DDEVICE
#include "modules/libvega/vega3ddevice.h"

static unsigned CountNonZeroCoefficients(VEGA_FIX* kernel, unsigned num_coeffs)
{
	unsigned count = 0;
	for (unsigned i = 0; i < num_coeffs; ++i)
		if (!VEGA_EQ(kernel[i], 0))
			count++;

	return count;
}

OP_STATUS VEGAFilterConvolve::getShader(VEGA3dDevice* device, VEGA3dShaderProgram** out_shader, VEGA3dTexture* srcTex)
{
	// convolve filter needs actual samples
	srcTex->setFilterMode(VEGA3dTexture::FILTER_NEAREST, VEGA3dTexture::FILTER_NEAREST);

	VEGA3dShaderProgram::WrapMode shdmode = edge_mode == VEGAFILTEREDGE_WRAP ? VEGA3dShaderProgram::WRAP_REPEAT_REPEAT : VEGA3dShaderProgram::WRAP_CLAMP_CLAMP;
	// Currently, there are shaders for #elems <= 25
	VEGA3dShaderProgram::ShaderType shdtype;
	unsigned nz_coeff = CountNonZeroCoefficients(kernel, kern_w * kern_h);
	if (nz_coeff <= 16)
	{
		shdtype = VEGA3dShaderProgram::SHADER_CONVOLVE_GEN_16_BIAS;
	}
	else if (nz_coeff <= 25)
	{
		shdtype = VEGA3dShaderProgram::SHADER_CONVOLVE_GEN_25_BIAS;
	}
	else
	{
		return OpStatus::ERR;
	}

	VEGA3dShaderProgram* shader = NULL;
	RETURN_IF_ERROR(device->createShaderProgram(&shader, shdtype, shdmode));

	device->setShaderProgram(shader);

	float coeff[25*4];
	op_memset(coeff, 0, sizeof(coeff));

	unsigned int i = 0, kofs = kern_w * kern_h - 1;
	for (int cy = 0; cy < kern_h; ++cy)
	{
		for (int cx = 0; cx < kern_w; ++cx, --kofs)
		{
			VEGA_FIX kv = kernel[kofs];
			if (!VEGA_EQ(kv, 0))
			{
				coeff[i*4+0] = (float)(cx - kern_cx) / srcTex->getWidth();
				coeff[i*4+1] = (float)(cy - kern_cy) / srcTex->getHeight();

				coeff[i*4+2] = VEGA_FIXTOFLT(kv);

				++i;
			}
		}
	}

	OP_ASSERT(i <= nz_coeff);

	shader->setVector4(shader->getConstantLocation("coeffs"), coeff, nz_coeff <= 16 ? 16 : 25);

	shader->setScalar(shader->getConstantLocation("divisor"), divisor);
	shader->setScalar(shader->getConstantLocation("bias"), bias);
	shader->setScalar(shader->getConstantLocation("preserve_alpha"), !!preserve_alpha);

	device->setTexture(0, srcTex);

	*out_shader = shader;

	return OpStatus::OK;
}

void VEGAFilterConvolve::putShader(VEGA3dDevice* device, VEGA3dShaderProgram* shader)
{
	OP_ASSERT(device && shader);

	VEGARefCount::DecRef(shader);
}

OP_STATUS VEGAFilterConvolve::setupVertexBuffer(VEGA3dDevice* device, VEGA3dBuffer** out_vbuf, VEGA3dVertexLayout** out_vlayout,  unsigned int* out_startIndex, VEGA3dTexture* tex, VEGA3dShaderProgram* sprog,
												const VEGAFilterRegion& region)
{
	srcx = region.sx;
	srcy = region.sy;
	srcw = region.width;
	srch = region.height;

	return createVertexBuffer_Unary(device, out_vbuf, out_vlayout, out_startIndex, tex, sprog, region);
}

OP_STATUS VEGAFilterConvolve::apply(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region, unsigned int frame)
{
	// FIXME: borderWidth is not the entire kernel size, but the
	// maximum deviation from the center, i.e. something like
	// max(abs(-kern_cx), abs(kern_w-kern_cx)) [for the x-axis].
	OP_STATUS status = applyShader(destStore, region, frame, MAX(kern_w, kern_h), edge_mode);
	if (OpStatus::IsError(status))
		status = applyFallback(destStore, region);

	return status;
}
#endif // VEGA_3DDEVICE

static inline void updateSums(VEGA_PIXEL srcval, VEGA_FIX kv,
							  VEGA_FIX& sum_a, VEGA_FIX& sum_r, VEGA_FIX& sum_g, VEGA_FIX& sum_b)
{
	int s_a = VEGA_UNPACK_A(srcval);
	int s_r = VEGA_UNPACK_R(srcval);
	int s_g = VEGA_UNPACK_G(srcval);
	int s_b = VEGA_UNPACK_B(srcval);

#ifndef USE_PREMULTIPLIED_ALPHA
	s_r = ((s_a + 1) * s_r) >> 8;
	s_g = ((s_a + 1) * s_g) >> 8;
	s_b = ((s_a + 1) * s_b) >> 8;
#endif // USE_PREMULTIPLIED_ALPHA

	sum_a += kv * s_a;
	sum_r += kv * s_r;
	sum_g += kv * s_g;
	sum_b += kv * s_b;
}

static inline VEGA_PIXEL reduceAndPack(VEGA_FIX sum_a, VEGA_FIX sum_r, VEGA_FIX sum_g, VEGA_FIX sum_b)
{
	if (sum_a == 0)
		return 0;

#ifndef USE_PREMULTIPLIED_ALPHA
	VEGA_FIX pmf = VEGA_FIXDIV(VEGA_INTTOFIX(255), sum_a);
	sum_r = VEGA_FIXMUL(sum_r, pmf);
	sum_g = VEGA_FIXMUL(sum_g, pmf);
	sum_b = VEGA_FIXMUL(sum_b, pmf);
#endif // USE_PREMULTIPLIED_ALPHA

	int da = VEGA_FIXTOINT(sum_a);
	int dr = VEGA_FIXTOINT(sum_r);
	int dg = VEGA_FIXTOINT(sum_g);
	int db = VEGA_FIXTOINT(sum_b);

	/* Clip */
	da = VEGA_CLAMP_U8(da);
#ifdef USE_PREMULTIPLIED_ALPHA
#define VEGA_CLAMP_TO_ALPHA(v,a) (((v) <= (a)) ? (((v) >= 0) ? (v) : 0) : (a))
	dr = VEGA_CLAMP_TO_ALPHA(dr, da);
	dg = VEGA_CLAMP_TO_ALPHA(dg, da);
	db = VEGA_CLAMP_TO_ALPHA(db, da);
#undef VEGA_CLAMP_TO_ALPHA
#else
	dr = VEGA_CLAMP_U8(dr);
	dg = VEGA_CLAMP_U8(dg);
	db = VEGA_CLAMP_U8(db);
#endif // USE_PREMULTIPLIED_ALPHA

	return VEGA_PACK_ARGB(da, dr, dg, db);
}

static inline void updateSums_PreserveAlpha(VEGA_PIXEL srcval, VEGA_FIX kv,
											VEGA_FIX& sum_r, VEGA_FIX& sum_g, VEGA_FIX& sum_b)
{
	int s_r = VEGA_UNPACK_R(srcval);
	int s_g = VEGA_UNPACK_G(srcval);
	int s_b = VEGA_UNPACK_B(srcval);

#ifdef USE_PREMULTIPLIED_ALPHA
	int s_a = VEGA_UNPACK_A(srcval);
	if (s_a)
	{
		s_r = 255 * s_r / s_a;
		s_g = 255 * s_g / s_a;
		s_b = 255 * s_b / s_a;
	}
	else
		s_r = s_g = s_b = 0;
#endif // USE_PREMULTIPLIED_ALPHA

	sum_r += kv * s_r;
	sum_g += kv * s_g;
	sum_b += kv * s_b;
}

static inline VEGA_PIXEL reduceAndPack_PreserveAlpha(int alpha, VEGA_FIX sum_r, VEGA_FIX sum_g, VEGA_FIX sum_b)
{
	int dr = VEGA_FIXTOINT(sum_r);
	int dg = VEGA_FIXTOINT(sum_g);
	int db = VEGA_FIXTOINT(sum_b);

	/* Clip */
	dr = VEGA_CLAMP_U8(dr);
	dg = VEGA_CLAMP_U8(dg);
	db = VEGA_CLAMP_U8(db);

#ifdef USE_PREMULTIPLIED_ALPHA
	dr = ((alpha + 1) * dr) >> 8;
	dg = ((alpha + 1) * dg) >> 8;
	db = ((alpha + 1) * db) >> 8;
#endif // USE_PREMULTIPLIED_ALPHA

	return VEGA_PACK_ARGB(alpha, dr, dg, db);
}

OP_STATUS VEGAFilterConvolve::apply(const VEGASWBuffer& dest, const VEGAFilterRegion& region)
{
	unsigned int xp, yp;
	VEGAConstPixelAccessor src = source.GetConstAccessor(region.sx, region.sy);
	VEGAPixelAccessor dst = dest.GetAccessor(region.dx, region.dy);

	unsigned int srcPixelStride = source.GetPixelStride();
	unsigned int dstPixelStride = dest.GetPixelStride();

	VEGA_FIX bias_p = bias;
	VEGA_FIX bias_p255 = bias * 255;
	VEGA_FIX invdiv = VEGA_FIXDIV(VEGA_INTTOFIX(1), divisor);

#if 0 // dynamic bias/divisor hack
	VEGA_FIX max_v = 0;
	VEGA_FIX min_v = 0;
	for (int i = 0; i < kern_w * kern_h; i++)
	{
		max_v += (kernel[i] > 0) ? kernel[i] : 0;
		min_v += (kernel[i] < 0) ? kernel[i] : 0;
	}

	VEGA_FIX dyn_fact = max_v - min_v;

	// Adjust the bias and divisor:
	// v = c / divisor + bias
	// => v' = c / (divisor * dyn_fact) + (bias - minv) / dyn_fact
	// => divisor' = divisor * dyn_fact, bias' = (bias - min_v) / dyn_fact
	invdiv = VEGA_FIXDIV(invdiv, dyn_fact);
	bias_p = VEGA_FIXDIV(bias_p - min_v, dyn_fact);
	bias_p255 = VEGA_FIXDIV(bias_p255 - min_v * 255, dyn_fact);
#endif

	switch (edge_mode)
	{
	case VEGAFILTEREDGE_NONE:
		{
			int srcyo = -(kern_cy * (int)srcPixelStride);

			for (yp = 0; yp < region.height; ++yp, srcyo += srcPixelStride)
			{
				int srcx = -kern_cx;

				for (xp = 0; xp < region.width; ++xp, srcx++)
				{
					VEGA_FIX sum_a = 0;
					VEGA_FIX sum_r = 0;
					VEGA_FIX sum_g = 0;
					VEGA_FIX sum_b = 0;

					int kofs = kern_h * kern_w - 1; /* offset into kernel */

					for (int i = 0; i < kern_h; i++, srcyo+=srcPixelStride)
					{
						if (srcyo < 0 || srcyo >= (int)(region.height*srcPixelStride))
						{
							kofs -= kern_w; /* compensate for the inner-loop */
							continue; /* only zeros on this row */
						}

						for (int j = 0; j < kern_w; j++, kofs--, srcx++)
						{
							if (srcx < 0 || srcx >= (int)region.width)
								continue; /* avoid multiplying with zero */

							VEGA_PIXEL srcval = src.LoadRel(srcx + srcyo);
							VEGA_FIX kv = kernel[kofs];

							if (!preserve_alpha)
								updateSums(srcval, kv, sum_a, sum_r, sum_g, sum_b);
							else
								updateSums_PreserveAlpha(srcval, kv, sum_r, sum_g, sum_b);
						}
						srcx -= kern_w;
					}
					srcyo -= kern_h * srcPixelStride;

					sum_r = VEGA_FIXMUL(sum_r, invdiv) + bias_p255;
					sum_g = VEGA_FIXMUL(sum_g, invdiv) + bias_p255;
					sum_b = VEGA_FIXMUL(sum_b, invdiv) + bias_p255;

					VEGA_PIXEL d;
					if (preserve_alpha)
					{
						int src_a = VEGA_UNPACK_A(src.LoadRel(xp + yp * srcPixelStride));
						d = reduceAndPack_PreserveAlpha(src_a, sum_r, sum_g, sum_b);
					}
					else
					{
						sum_a = VEGA_FIXMUL(sum_a, invdiv) + bias_p;
						d = reduceAndPack(sum_a, sum_r, sum_g, sum_b);
					}

					dst.Store(d);

					++dst;
				}

				dst += dstPixelStride - region.width;
			}
		}
		break;

	case VEGAFILTEREDGE_WRAP:
		{
			int srcy = -kern_cy;

			if (srcy < 0)
				srcy = srcy % (int)region.height;

			for (yp = 0; yp < region.height; ++yp, srcy++)
			{
				int srcx = -kern_cx;

				if (srcx < 0)
					srcx = srcx % (int)region.width;

				for (xp = 0; xp < region.width; ++xp, srcx++)
				{
					VEGA_FIX sum_a = 0;
					VEGA_FIX sum_r = 0;
					VEGA_FIX sum_g = 0;
					VEGA_FIX sum_b = 0;

					int kofs = kern_h * kern_w - 1; /* offset into kernel */

					for (int i = 0; i < kern_h; i++, srcy++)
					{
						if (srcy < 0)
							srcy += region.height;
						if (srcy >= (int)region.height)
							srcy -= region.height;

						unsigned int yofs = srcy * srcPixelStride;
						for (int j = 0; j < kern_w; j++, kofs--, srcx++)
						{
							if (srcx < 0)
								srcx += region.width;
							if (srcx >= (int)region.width)
								srcx -= region.width;

							VEGA_PIXEL srcval = src.LoadRel(srcx + yofs);
							VEGA_FIX kv = kernel[kofs];

							if (!preserve_alpha)
								updateSums(srcval, kv, sum_a, sum_r, sum_g, sum_b);
							else
								updateSums_PreserveAlpha(srcval, kv, sum_r, sum_g, sum_b);
						}
						srcx -= kern_w;
					}
					srcy -= kern_h;

					sum_r = VEGA_FIXMUL(sum_r, invdiv) + bias_p255;
					sum_g = VEGA_FIXMUL(sum_g, invdiv) + bias_p255;
					sum_b = VEGA_FIXMUL(sum_b, invdiv) + bias_p255;

					VEGA_PIXEL d;
					if (preserve_alpha)
					{
						int src_a = VEGA_UNPACK_A(src.LoadRel(xp + yp * srcPixelStride));
						d = reduceAndPack_PreserveAlpha(src_a, sum_r, sum_g, sum_b);
					}
					else
					{
						sum_a = VEGA_FIXMUL(sum_a, invdiv) + bias_p;
						d = reduceAndPack(sum_a, sum_r, sum_g, sum_b);
					}

					dst.Store(d);

					++dst;
				}

				dst += dstPixelStride - region.width;
			}
		}
		break;

	default:
	case VEGAFILTEREDGE_DUPLICATE:
		for (yp = 0; yp < region.height; ++yp)
		{
			for (xp = 0; xp < region.width; ++xp)
			{
				VEGA_FIX sum_a = 0;
				VEGA_FIX sum_r = 0;
				VEGA_FIX sum_g = 0;
				VEGA_FIX sum_b = 0;

				int kofs = kern_h * kern_w - 1; /* offset into kernel */

				for (int i = 0; i < kern_h; i++)
				{
					int srcy = yp - kern_cy + i;

					if (srcy < 0)
						srcy = 0;
					else if (srcy >= (int)region.height)
						srcy = region.height - 1;

					unsigned int yofs = srcy * srcPixelStride;
					for (int j = 0; j < kern_w; j++, kofs--)
					{
						int srcx = xp - kern_cx + j;

						if (srcx < 0)
							srcx = 0;
						else if (srcx >= (int)region.width)
							srcx = region.width - 1;

						VEGA_PIXEL srcval = src.LoadRel(srcx + yofs);
						VEGA_FIX kv = kernel[kofs];

						if (!preserve_alpha)
							updateSums(srcval, kv, sum_a, sum_r, sum_g, sum_b);
						else
							updateSums_PreserveAlpha(srcval, kv, sum_r, sum_g, sum_b);
					}
				}

				sum_r = VEGA_FIXMUL(sum_r, invdiv) + bias_p255;
				sum_g = VEGA_FIXMUL(sum_g, invdiv) + bias_p255;
				sum_b = VEGA_FIXMUL(sum_b, invdiv) + bias_p255;

				VEGA_PIXEL d;
				if (preserve_alpha)
				{
					int src_a = VEGA_UNPACK_A(src.LoadRel(xp + yp * srcPixelStride));
					d = reduceAndPack_PreserveAlpha(src_a, sum_r, sum_g, sum_b);
				}
				else
				{
					sum_a = VEGA_FIXMUL(sum_a, invdiv) + bias_p;
					d = reduceAndPack(sum_a, sum_r, sum_g, sum_b);
				}

				dst.Store(d);

				++dst;
			}

			dst += dstPixelStride - region.width;
		}
		break;
	}

	return OpStatus::OK;
}

#undef VEGA_CLAMP_U8

#endif // VEGA_SUPPORT
