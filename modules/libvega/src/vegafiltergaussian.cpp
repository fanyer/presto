/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef VEGA_SUPPORT
#include "modules/libvega/src/vegafiltergaussian.h"
#include "modules/libvega/src/vegapixelformat.h"
#ifdef VEGA_3DDEVICE
#include "modules/libvega/src/vegabackend_hw3d.h"
#endif // VEGA_3DDEVICE

VEGAFilterGaussian::VEGAFilterGaussian() :
	wrap(false), kernel_w(0), kernel_h(0), kernel_x(NULL), kernel_y(NULL)
{}

VEGAFilterGaussian::~VEGAFilterGaussian()
{
	if (kernel_x)
		OP_DELETEA(kernel_x);
	if (kernel_y)
		OP_DELETEA(kernel_y);
}

/*static */
void VEGAFilterGaussian::initKernel(VEGA_FIX stdDev, VEGA_FIX* kernel, int kernel_width, int kernel_size)
{
	OP_ASSERT(kernel_size == 2*kernel_width+1);
	/* g[n] = exp(-n^2 / 2*stdDev^2) / (sqrt(2*pi) * stdDev) [-kern_width ... kern_width] */
	VEGA_FIX div_fact = VEGA_FIXMUL(stdDev, VEGA_FIXSQRT(VEGA_FIX_2PI));
	VEGA_FIX stdDev2 = VEGA_FIXMUL(stdDev, stdDev);
	VEGA_FIX sum = 0;
	int i, n;
	for (i = 0, n = -kernel_width; i <= kernel_width; i++, n++)
	{
		VEGA_FIX n2 = VEGA_INTTOFIX(n * n);
		VEGA_FIX expon = VEGA_FIXEXP(-VEGA_FIXDIV(n2, VEGA_FIXMUL2(stdDev2)));
		VEGA_FIX v = VEGA_FIXDIV(expon, div_fact);

		kernel[i] = kernel[(kernel_size - 1) - i] = v;
		sum += (n ? 2 : 1) * v;
	}

	/* Normalize (generally only for small stddev:s) */
	for (i = 0; i < kernel_size; ++i)
		kernel[i] = VEGA_FIXDIV(kernel[i], sum);
}

OP_STATUS VEGAFilterGaussian::createKernel(VEGA_FIX stdDev, VEGA_FIX** kernel, int& kernel_size)
{
	if (kernel == NULL)
		return OpStatus::ERR;

	/* Possibly use 5 instead of 3 */
	int kern_width = VEGA_FIXTOINT(VEGA_CEIL(3 * stdDev));
	int kern_size = 1 + 2*kern_width;
	VEGA_FIX* kern = OP_NEWA(VEGA_FIX, kern_size);
	if (kern == NULL)
		return OpStatus::ERR_NO_MEMORY;

	initKernel(stdDev, kern, kern_width, kern_size);

	kernel_size = kern_size;
	*kernel = kern;

	return OpStatus::OK;
}

OP_STATUS VEGAFilterGaussian::setStdDeviation(VEGA_FIX x, VEGA_FIX y)
{
	if (kernel_x)
	{
		OP_DELETEA(kernel_x);
		kernel_x = NULL;
	}
	if (x < VEGA_INTTOFIX(275)/1000)
	{
		kernel_w = 0;
	}
	else if (x >= VEGA_INTTOFIX(2))
	{
		kernel_w = VEGA_FIXTOINT(VEGA_FLOOR(VEGA_FIXMUL(x, VEGA_FIXDIV(3*VEGA_FIXSQRT(VEGA_FIX_2PI),
																	   VEGA_INTTOFIX(4))) +
											VEGA_FIXDIV2(1)));
	}
	else
	{
		RETURN_IF_ERROR(createKernel(x, &kernel_x, kernel_w));
	}

	if (kernel_y)
	{
		OP_DELETEA(kernel_y);
		kernel_y = NULL;
	}
	if (y < VEGA_INTTOFIX(275)/1000)
	{
		kernel_h = 0;
	}
	else if (y >= VEGA_INTTOFIX(2))
	{
		kernel_h = VEGA_FIXTOINT(VEGA_FLOOR(VEGA_FIXMUL(y, VEGA_FIXDIV(3*VEGA_FIXSQRT(VEGA_FIX_2PI),
																	   VEGA_INTTOFIX(4))) +
											VEGA_FIXDIV2(1)));
	}
	else
	{
		RETURN_IF_ERROR(createKernel(y, &kernel_y, kernel_h));
	}

	return OpStatus::OK;
}

static inline UINT32 inc_sum_argb(VEGA_PIXEL v,
								  unsigned int& acc_a, unsigned int& acc_r,
								  unsigned int& acc_g, unsigned int& acc_b)
{
	unsigned int sa = VEGA_UNPACK_A(v);
	if (sa == 0)
		return 0;

	unsigned int sr = VEGA_UNPACK_R(v);
	unsigned int sg = VEGA_UNPACK_G(v);
	unsigned int sb = VEGA_UNPACK_B(v);

#ifndef USE_PREMULTIPLIED_ALPHA
	unsigned int sap1 = sa+1;
	sr = (sr * sap1) >> 8;
	sg = (sg * sap1) >> 8;
	sb = (sb * sap1) >> 8;
#endif // !USE_PREMULTIPLIED_ALPHA

	acc_a += sa;
	acc_r += sr;
	acc_g += sg;
	acc_b += sb;

	return (sa<<24)|(sr<<16)|(sg<<8)|sb;
}

static inline void dec_sum_argb(UINT32 v,
								unsigned int& acc_a, unsigned int& acc_r,
								unsigned int& acc_g, unsigned int& acc_b)
{
	unsigned int va = v >> 24;
	if (va == 0)
		return;

	acc_a -= va;
	acc_r -= (v >> 16) & 0xff;
	acc_g -= (v >> 8) & 0xff;
	acc_b -= v & 0xff;
}

static inline VEGA_PIXEL applyKernel(const UINT32* cbuf, unsigned int cbuf_pos,
									 unsigned int cbuf_mask,
									 const VEGA_FIX* kernel, int kernel_size)
{
	VEGA_FIX vda, vdr, vdg, vdb;

	vda = vdr = vdg = vdb = 0;

	while (kernel_size--)
	{
		UINT32 c = cbuf[cbuf_pos++];
		VEGA_FIX kv = *kernel++;

		unsigned int sa = c >> 24;

		if (sa != 0)
		{
			unsigned int sr = (c >> 16) & 0xff;
			unsigned int sg = (c >> 8) & 0xff;
			unsigned int sb = c & 0xff;
#ifdef USE_PREMULTIPLIED_ALPHA
			vda += sa * kv;
			vdr += sr * kv;
			vdg += sg * kv;
			vdb += sb * kv;
#else
			VEGA_FIX pmf = VEGA_INTTOFIX(sa) / 255;

			vda += sa * kv;
			vdr += VEGA_FIXMUL(sr * pmf, kv);
			vdg += VEGA_FIXMUL(sg * pmf, kv);
			vdb += VEGA_FIXMUL(sb * pmf, kv);
#endif // !USE_PREMULTIPLIED_ALPHA
		}

		cbuf_pos &= cbuf_mask;
	}

	unsigned int da = VEGA_FIXTOINT(vda);
	da = (da > 255) ? 255 : da;

	if (da == 0)
		return 0;

#ifdef USE_PREMULTIPLIED_ALPHA
	unsigned int dr = VEGA_FIXTOINT(vdr);
	unsigned int dg = VEGA_FIXTOINT(vdg);
	unsigned int db = VEGA_FIXTOINT(vdb);

	dr = (dr > da) ? da : dr;
	dg = (dg > da) ? da : dg;
	db = (db > da) ? da : db;

#else
	VEGA_FIX pmf = VEGA_FIXDIV(VEGA_INTTOFIX(255), vda);
	unsigned int dr = VEGA_FIXTOINT(VEGA_FIXMUL(vdr, pmf));
	unsigned int dg = VEGA_FIXTOINT(VEGA_FIXMUL(vdg, pmf));
	unsigned int db = VEGA_FIXTOINT(VEGA_FIXMUL(vdb, pmf));

	dr = (dr > 255) ? 255 : dr;
	dg = (dg > 255) ? 255 : dg;
	db = (db > 255) ? 255 : db;

#endif // !USE_PREMULTIPLIED_ALPHA

	return VEGA_PACK_ARGB(da, dr, dg, db);
}

static inline VEGA_PIXEL applyKernel_A(const unsigned char* cbuf_a, unsigned int cbuf_pos,
									   unsigned int cbuf_mask,
									   const VEGA_FIX* kernel, int kernel_size)
{
	VEGA_FIX vda = 0;

	while (kernel_size--)
	{
		unsigned int c = cbuf_a[cbuf_pos++];
		VEGA_FIX kv = *kernel++;

		vda += c * kv;

		cbuf_pos &= cbuf_mask;
	}

	unsigned int da = VEGA_FIXTOINT(vda);
	da = (da > 255) ? 255 : da;

	return VEGA_PACK_ARGB(da, 0, 0, 0);
}

static inline VEGA_PIXEL pack_pixel(unsigned int acc_a, unsigned int acc_r,
									unsigned int acc_g, unsigned int acc_b,
									unsigned int div)
{
#ifdef USE_PREMULTIPLIED_ALPHA
	unsigned int da = ((acc_a * div) + (1 << 23)) >> 24;
	da = (da > 255) ? 255 : da;

	if (da == 0)
		return 0;

	// d<c> = acc_<c> / div
	unsigned int dr = ((acc_r * div) + (1 << 23)) >> 24;
	unsigned int dg = ((acc_g * div) + (1 << 23)) >> 24;
	unsigned int db = ((acc_b * div) + (1 << 23)) >> 24;

	dr = (dr > da) ? da : dr;
	dg = (dg > da) ? da : dg;
	db = (db > da) ? da : db;

#else
	unsigned int da = acc_a / div;
	da = (da > 255) ? 255 : da;

	if (da == 0)
		return 0;

	// d<c> = (acc_<c> / div) / (acc_a / div)
	unsigned int dr = (acc_r << 8) / acc_a;
	unsigned int dg = (acc_g << 8) / acc_a;
	unsigned int db = (acc_b << 8) / acc_a;

	dr = (dr > 255) ? 255 : dr;
	dg = (dg > 255) ? 255 : dg;
	db = (db > 255) ? 255 : db;

#endif // USE_PREMULTIPLIED_ALPHA

	return VEGA_PACK_ARGB(da, dr, dg, db);
}

void VEGAFilterGaussian::boxBlurRow(VEGAPixelPtr dstp, unsigned int dststride,
									VEGAPixelPtr srcp, unsigned int srcstride,
									unsigned int count,
									unsigned int left_count, unsigned int right_count)
{
	unsigned int acc_a;
	unsigned int acc_r;
	unsigned int acc_g;
	unsigned int acc_b;

	VEGAConstPixelAccessor src(srcp);
	VEGAPixelPtr src_end = srcp + count * srcstride;

	unsigned int rd_pos, wr_pos;
	rd_pos = wr_pos = 0;

	acc_a = acc_r = acc_g = acc_b = 0;

	// Prefill with zeros before left edge
	int w = left_count;
	while (w--)
		cbuf[wr_pos++] = 0;

	// Get source pixels for a half kernel
	w = MIN(count, right_count);
	while (w--)
	{
		// Queue right edge pixel and update accumulator
		cbuf[wr_pos++] = inc_sum_argb(src.Load(), acc_a, acc_r, acc_g, acc_b);

		src += srcstride;
	}

	unsigned kernel_len = 1 + left_count + right_count;
#ifdef USE_PREMULTIPLIED_ALPHA
	unsigned kernel_div = (1 << 24) / kernel_len;
#else
	unsigned kernel_div = kernel_len;
#endif // USE_PREMULTIPLIED_ALPHA

	// Process remaining source pixels
	VEGAPixelAccessor dst(dstp);
	while (src.Ptr() < src_end)
	{
		// Queue right edge pixel and update accumulator
		cbuf[wr_pos++] = inc_sum_argb(src.Load(), acc_a, acc_r, acc_g, acc_b);
		wr_pos &= cbuf_mask;
		src += srcstride;

		dst.Store(pack_pixel(acc_a, acc_r, acc_g, acc_b, kernel_div));

		dst += dststride;

		count--;

		// Dequeue and remove left edge pixel from accumulator
		dec_sum_argb(cbuf[rd_pos++], acc_a, acc_r, acc_g, acc_b);
		rd_pos &= cbuf_mask;
	}

	while (count)
	{
		// Right edge consists of zeros => no need to accumulate

		dst.Store(pack_pixel(acc_a, acc_r, acc_g, acc_b, kernel_div));

		dst += dststride;

		count--;

		// Dequeue and remove left edge pixel from accumulator
		dec_sum_argb(cbuf[rd_pos++], acc_a, acc_r, acc_g, acc_b);
		rd_pos &= cbuf_mask;
	}
}

void VEGAFilterGaussian::boxBlurRow_A(VEGAPixelPtr dstp, unsigned int dststride,
									  VEGAPixelPtr srcp, unsigned int srcstride,
									  unsigned int count,
									  unsigned int left_count, unsigned int right_count)
{
	VEGAConstPixelAccessor src(srcp);
	VEGAPixelPtr src_end = srcp + count * srcstride;

	unsigned int rd_pos, wr_pos, xp;
	rd_pos = wr_pos = xp = 0;

	unsigned int acc_a = 0;

	// Prefill with zeros before left edge
	int w = left_count;
	while (w--)
		cbuf_a[wr_pos++] = 0;

	// Get source pixels for a half kernel
	w = MIN(count, right_count);
	while (w--)
	{
		unsigned char right_edge_a = VEGA_UNPACK_A(src.Load());
		cbuf_a[wr_pos++] = right_edge_a;
		acc_a += right_edge_a;

		src += srcstride;
	}

	int kernel_len = 1 + left_count + right_count;

	// Process remaining source pixels
	VEGAPixelAccessor dst(dstp);
	while (src.Ptr() < src_end)
	{
		// Add right edge to running average
		unsigned char right_edge_a = VEGA_UNPACK_A(src.Load());
		cbuf_a[wr_pos++] = right_edge_a;
		acc_a += right_edge_a;
		src += srcstride;

		unsigned int da = acc_a / kernel_len;
		da = (da > 255) ? 255 : da;
		dst.StoreARGB(da, 0, 0, 0);

		dst += dststride;

		count--;

		// Remove left edge from running average
		acc_a -= cbuf_a[rd_pos++];

		rd_pos &= cbuf_mask;
		wr_pos &= cbuf_mask;
	}

	while (count)
	{
		// Right edge consists of zeros => no need to accumulate

		unsigned int da = acc_a / kernel_len;
		da = (da > 255) ? 255 : da;
		dst.StoreARGB(da, 0, 0, 0);

		dst += dststride;

		count--;

		// Remove left edge from running average
		acc_a -= cbuf_a[rd_pos++];

		rd_pos &= cbuf_mask;
	}
}

void VEGAFilterGaussian::realGaussRow(VEGAPixelPtr dstp, unsigned int dststride,
									  VEGAPixelPtr srcp, unsigned int srcstride,
									  unsigned int count, const VEGA_FIX* kernel,
									  unsigned int half_kl)
{
	VEGAConstPixelAccessor src(srcp);
	int pixels_left = count;

	unsigned int sa, sr, sg, sb;
	unsigned int wr_pos, rd_pos, pos;
	rd_pos = wr_pos = pos = 0;

	// Prefill with zeros before left edge
	// FIXME: Just advance the (read?)pointer appropriately
	int w = half_kl;
	while (w--)
		cbuf[wr_pos++] = 0;

	// Get source pixels for the other part
	w = MIN(count, half_kl);
	while (w--)
	{
		src.LoadUnpack(sa, sr, sg, sb);
		cbuf[wr_pos++] = (sa<<24)|(sr<<16)|(sg<<8)|sb;

		src += srcstride;
		pos++;
	}

	unsigned int kernel_len = 1 + 2*half_kl;

	// Process remaining source pixels
	VEGAPixelAccessor dst(dstp);
	while (pos < count)
	{
		src.LoadUnpack(sa, sr, sg, sb);
		cbuf[wr_pos++] = (sa<<24)|(sr<<16)|(sg<<8)|sb;

		src += srcstride;
		pos++;

		dst.Store(applyKernel(cbuf, rd_pos++, cbuf_mask, kernel, kernel_len));

		dst += dststride;

		pixels_left--;

		rd_pos &= cbuf_mask;
		wr_pos &= cbuf_mask;
	}

	// cbuf has less than kernel_w items
	// (for cases when half_kl > count)
	while (pos < half_kl+1)
	{
		cbuf[wr_pos++] = 0;
		pos++;
		wr_pos &= cbuf_mask;
	}

	// Process remaining output
	while (pixels_left)
	{
		// FIXME: No need to advance this pointer?
		// Just decrement count instead
		cbuf[wr_pos++] = 0;

		dst.Store(applyKernel(cbuf, rd_pos++, cbuf_mask, kernel, kernel_len));

		dst += dststride;

		pixels_left--;

		rd_pos &= cbuf_mask;
		wr_pos &= cbuf_mask;
	}
}

void VEGAFilterGaussian::realGaussRow_A(VEGAPixelPtr dstp, unsigned int dststride,
										VEGAPixelPtr srcp, unsigned int srcstride,
										unsigned int count, const VEGA_FIX* kernel,
										unsigned int half_kl)
{
	VEGAConstPixelAccessor src(srcp);
	int row_pixels_left = count;

	unsigned int rd_pos, wr_pos, pos;
	rd_pos = wr_pos = pos = 0;

	// Prefill with zeros before left edge
	int w = half_kl;
	while (w--)
		cbuf_a[wr_pos++] = 0;

	// Get source pixels for the other part
	w = MIN(count, half_kl);
	while (w--)
	{
		cbuf_a[wr_pos++] = VEGA_UNPACK_A(src.Load());

		src += srcstride;
		pos++;
	}

	unsigned int kernel_len = 1 + 2*half_kl;

	// Process remaining source pixels
	VEGAPixelAccessor dst(dstp);
	while (pos < count)
	{
		cbuf_a[wr_pos++] = VEGA_UNPACK_A(src.Load());

		src += srcstride;
		pos++;

		dst.Store(applyKernel_A(cbuf_a, rd_pos++, cbuf_mask, kernel, kernel_len));

		dst += dststride;

		row_pixels_left--;

		rd_pos &= cbuf_mask;
		wr_pos &= cbuf_mask;
	}

	// cbuf has less than kernel_w items
	// (for cases when half_kl > count)
	while (pos < half_kl+1)
	{
		cbuf_a[wr_pos++] = 0;
		pos++;
		wr_pos &= cbuf_mask;
	}

	// Process remaining output
	while (row_pixels_left)
	{
		cbuf_a[wr_pos++] = 0;

		dst.Store(applyKernel_A(cbuf_a, rd_pos++, cbuf_mask, kernel, kernel_len));

		dst += dststride;

		row_pixels_left--;

		rd_pos &= cbuf_mask;
		wr_pos &= cbuf_mask;
	}
}

void VEGAFilterGaussian::blur(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf)
{
	unsigned int srcstride = srcbuf.GetPixelStride();
	unsigned int dststride = dstbuf.GetPixelStride();

	VEGAPixelAccessor dst = dstbuf.GetAccessor(0, 0);
	VEGAConstPixelAccessor src = srcbuf.GetConstAccessor(0, 0);

	unsigned width = dstbuf.width;
	unsigned height = dstbuf.height;

	unsigned int half_kw = kernel_w / 2;

	if (kernel_w == 0)
	{
		for (unsigned int yp = 0; yp < height; ++yp)
		{
			dst.MoveFrom(src.Ptr(), width);

			src += srcstride;
			dst += dststride;
		}
	}
	else if (kernel_x)
	{
		// Small stddevs
		for (unsigned int yp = 0; yp < height; ++yp)
		{
			realGaussRow(dst.Ptr(), 1, src.Ptr(), 1,
						 width, kernel_x, half_kw);

			src += srcstride;
			dst += dststride;
		}
	}
	else
	{
		// Large stddevs
		if (kernel_w & 1)
		{
			// Odd
			for (unsigned int yp = 0; yp < height; ++yp)
			{
				boxBlurRow(dst.Ptr(), 1, src.Ptr(), 1,
						   width, half_kw, half_kw);
				boxBlurRow(dst.Ptr(), 1, dst.Ptr(), 1,
						   width, half_kw, half_kw);
				boxBlurRow(dst.Ptr(), 1, dst.Ptr(), 1,
						   width, half_kw, half_kw);

				dst += dststride;
				src += srcstride;
			}
		}
		else
		{
			// Even
			for (unsigned int yp = 0; yp < height; ++yp)
			{
				boxBlurRow(dst.Ptr(), 1, src.Ptr(), 1,
						   width, half_kw, half_kw-1); // d shift left
				boxBlurRow(dst.Ptr(), 1, dst.Ptr(), 1,
						   width, half_kw-1, half_kw); // d shift right
				boxBlurRow(dst.Ptr(), 1, dst.Ptr(), 1,
						   width, half_kw, half_kw); // d+1

				dst += dststride;
				src += srcstride;
			}
		}
	}

	dst = dstbuf.GetAccessor(0, 0);

	unsigned int half_kh = kernel_h / 2;

	if (kernel_h == 0)
	{
		return;
	}
	else if (kernel_y)
	{
		// Small stddevs
		for (unsigned int xp = 0; xp < width; ++xp)
		{
			realGaussRow(dst.Ptr(), dststride, dst.Ptr(), dststride,
						 height, kernel_y, half_kh);

			dst++;
		}
	}
	else
	{
		// Large stddevs
		if (kernel_h & 1)
		{
			// Odd
			for (unsigned int xp = 0; xp < width; ++xp)
			{
				boxBlurRow(dst.Ptr(), dststride, dst.Ptr(), dststride,
						   height, half_kh, half_kh);
				boxBlurRow(dst.Ptr(), dststride, dst.Ptr(), dststride,
						   height, half_kh, half_kh);
				boxBlurRow(dst.Ptr(), dststride, dst.Ptr(), dststride,
						   height, half_kh, half_kh);

				dst++;
			}
		}
		else
		{
			// Even
			for (unsigned int xp = 0; xp < width; ++xp)
			{
				boxBlurRow(dst.Ptr(), dststride, dst.Ptr(), dststride,
						   height, half_kh, half_kh-1); // d shift left
				boxBlurRow(dst.Ptr(), dststride, dst.Ptr(), dststride,
						   height, half_kh-1, half_kh); // d shift right
				boxBlurRow(dst.Ptr(), dststride, dst.Ptr(), dststride,
						   height, half_kh, half_kh); // d+1

				dst++;
			}
		}
	}
}

void VEGAFilterGaussian::blur_A(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf)
{
	unsigned int srcstride = srcbuf.GetPixelStride();
	unsigned int dststride = dstbuf.GetPixelStride();

	VEGAPixelAccessor dst = dstbuf.GetAccessor(0, 0);
	VEGAConstPixelAccessor src = srcbuf.GetConstAccessor(0, 0);

	unsigned width = dstbuf.width;
	unsigned height = dstbuf.height;

	unsigned int half_kw = kernel_w / 2;

	if (kernel_w == 0)
	{
		if (kernel_h == 0)
		{
			// Copy only alpha channel
			for (unsigned int yp = 0; yp < height; ++yp)
			{
				for (unsigned int xp = 0; xp < width; ++xp)
				{
					dst.StoreARGB(VEGA_UNPACK_A(src.Load()), 0, 0, 0);

					++src;
					++dst;
				}

				src += srcstride - width;
				dst += dststride - width;
			}
		}
		else
		{
			for (unsigned int yp = 0; yp < height; ++yp)
			{
				dst.MoveFrom(src.Ptr(), width);

				src += srcstride;
				dst += dststride;
			}
		}
	}
	else if (kernel_x)
	{
		// Small stddevs
		for (unsigned int yp = 0; yp < height; ++yp)
		{
			realGaussRow_A(dst.Ptr(), 1, src.Ptr(), 1,
						   width, kernel_x, half_kw);

			src += srcstride;
			dst += dststride;
		}
	}
	else
	{
		// Large stddevs
		if (kernel_w & 1)
		{
			// Odd
			for (unsigned int yp = 0; yp < height; ++yp)
			{
				boxBlurRow_A(dst.Ptr(), 1, src.Ptr(), 1,
							 width, half_kw, half_kw);
				boxBlurRow_A(dst.Ptr(), 1, dst.Ptr(), 1,
							 width, half_kw, half_kw);
				boxBlurRow_A(dst.Ptr(), 1, dst.Ptr(), 1,
							 width, half_kw, half_kw);

				dst += dststride;
				src += srcstride;
			}
		}
		else
		{
			// Even
			for (unsigned int yp = 0; yp < height; ++yp)
			{
				boxBlurRow_A(dst.Ptr(), 1, src.Ptr(), 1,
							 width, half_kw, half_kw-1); // d shift left
				boxBlurRow_A(dst.Ptr(), 1, dst.Ptr(), 1,
							 width, half_kw-1, half_kw); // d shift right
				boxBlurRow_A(dst.Ptr(), 1, dst.Ptr(), 1,
							 width, half_kw, half_kw); // d+1

				dst += dststride;
				src += srcstride;
			}
		}
	}

	dst = dstbuf.GetAccessor(0, 0);

	unsigned int half_kh = kernel_h / 2;

	if (kernel_h == 0)
	{
		return;
	}
	else if (kernel_y)
	{
		// Small stddevs
		for (unsigned int xp = 0; xp < width; ++xp)
		{
			realGaussRow_A(dst.Ptr(), dststride,
						   dst.Ptr(), dststride,
						   height, kernel_y, half_kh);

			dst++;
		}
	}
	else
	{
		// Large stddevs
		if (kernel_h & 1)
		{
			// Odd
			for (unsigned int xp = 0; xp < width; ++xp)
			{
				boxBlurRow_A(dst.Ptr(), dststride, dst.Ptr(), dststride,
							 height, half_kh, half_kh);
				boxBlurRow_A(dst.Ptr(), dststride, dst.Ptr(), dststride,
							 height, half_kh, half_kh);
				boxBlurRow_A(dst.Ptr(), dststride, dst.Ptr(), dststride,
							 height, half_kh, half_kh);

				dst++;
			}
		}
		else
		{
			// Even
			for (unsigned int xp = 0; xp < width; ++xp)
			{
				boxBlurRow_A(dst.Ptr(), dststride, dst.Ptr(), dststride,
							 height, half_kh, half_kh-1); // d shift left
				boxBlurRow_A(dst.Ptr(), dststride, dst.Ptr(), dststride,
							 height, half_kh-1, half_kh); // d shift right
				boxBlurRow_A(dst.Ptr(), dststride, dst.Ptr(), dststride,
							 height, half_kh, half_kh); // d+1

				dst++;
			}
		}
	}
}

// Calculate the startoffset for wrap
static inline unsigned int calc_startoffset(unsigned int run_len, unsigned int kern_ofs)
{
	return (run_len - 1) - ((kern_ofs - 1) % run_len);
}

void VEGAFilterGaussian::boxBlurRow_W(VEGAPixelPtr dstp, unsigned int dststride,
									  VEGAPixelPtr srcp, unsigned int srcstride,
									  unsigned int count,
									  unsigned int left_count, unsigned int kernel_len)
{
	VEGAConstPixelAccessor src(srcp);
	// Calculate row end
	VEGAPixelPtr src_end = srcp + count * srcstride;

	unsigned int acc_a, acc_r, acc_g, acc_b;
	unsigned int rd_pos, wr_pos;
	rd_pos = wr_pos = 0;

	acc_a = acc_r = acc_g = acc_b = 0;

	src += calc_startoffset(count, left_count) * srcstride;

	// Get source pixels for the first destination pixel
	int w = kernel_len - 1;
	while (w--)
	{
		// Queue right edge pixel and update accumulator
		cbuf[wr_pos++] = inc_sum_argb(src.Load(), acc_a, acc_r, acc_g, acc_b);

		src += srcstride;
		if (src.Ptr() >= src_end)
			src.Reset(srcp);
	}

#ifdef USE_PREMULTIPLIED_ALPHA
	unsigned kernel_div = (1 << 24) / kernel_len;
#else
	unsigned kernel_div = kernel_len;
#endif // USE_PREMULTIPLIED_ALPHA

	// Produce output
	VEGAPixelAccessor dst(dstp);
	while (count)
	{
		// Queue right edge pixel and update accumulator
		cbuf[wr_pos++] = inc_sum_argb(src.Load(), acc_a, acc_r, acc_g, acc_b);

		src += srcstride;
		if (src.Ptr() >= src_end)
			src.Reset(srcp);

		dst.Store(pack_pixel(acc_a, acc_r, acc_g, acc_b, kernel_div));

		dst += dststride;

		count--;

		// Dequeue and remove left edge pixel from accumulator
		dec_sum_argb(cbuf[rd_pos++], acc_a, acc_r, acc_g, acc_b);

		rd_pos &= cbuf_mask;
		wr_pos &= cbuf_mask;
	}
}

void VEGAFilterGaussian::boxBlurRow_AW(VEGAPixelPtr dstp, unsigned int dststride,
									   VEGAPixelPtr srcp, unsigned int srcstride,
									   unsigned int count,
									   unsigned int left_count, unsigned int kernel_len)
{
	VEGAConstPixelAccessor src(srcp);
	// Calculate row end
	VEGAPixelPtr src_end = srcp + count * srcstride;

	unsigned int rd_pos, wr_pos;
	rd_pos = wr_pos = 0;

	unsigned int acc_a = 0;

	src += calc_startoffset(count, left_count) * srcstride;

	// Get source pixels for first destination pixel
	int w = kernel_len - 1;
	while (w--)
	{
		unsigned char right_edge_a = VEGA_UNPACK_A(src.Load());
		cbuf_a[wr_pos++] = right_edge_a;
		acc_a += right_edge_a;

		src += srcstride;
		if (src.Ptr() >= src_end)
			src.Reset(srcp);
	}

	// Process output pixels
	VEGAPixelAccessor dst(dstp);
	while (count)
	{
		// Add right edge to running average
		unsigned char right_edge_a = VEGA_UNPACK_A(src.Load());
		cbuf_a[wr_pos++] = right_edge_a;
		acc_a += right_edge_a;

		src += srcstride;
		if (src.Ptr() >= src_end)
			src.Reset(srcp);

		unsigned int da = acc_a / kernel_len;
		da = (da > 255) ? 255 : da;
		dst.StoreARGB(da, 0, 0, 0);

		dst += dststride;

		count--;

		// Remove left edge from running average
		acc_a -= cbuf_a[rd_pos++];

		rd_pos &= cbuf_mask;
		wr_pos &= cbuf_mask;
	}
}

void VEGAFilterGaussian::realGaussRow_W(VEGAPixelPtr dstp, unsigned int dststride,
										VEGAPixelPtr srcp, unsigned int srcstride,
										unsigned int count, const VEGA_FIX* kernel,
										unsigned int half_kl)
{
	VEGAConstPixelAccessor src(srcp);
	// Calculate row end
	VEGAPixelPtr src_end = srcp + count * srcstride;

	unsigned int sa, sr, sg, sb;
	unsigned int wr_pos, rd_pos;
	rd_pos = wr_pos = 0;

	src += calc_startoffset(count, half_kl) * srcstride;

	// Prefill with appropriate pixels based on wrapping
	int w = half_kl*2;
	while (w--)
	{
		src.LoadUnpack(sa, sr, sg, sb);
		cbuf[wr_pos++] = (sa<<24)|(sr<<16)|(sg<<8)|sb;

		src += srcstride;
		if (src.Ptr() >= src_end)
			src.Reset(srcp);
	}

	unsigned int kernel_len = 1 + 2*half_kl;

	// Produce output
	VEGAPixelAccessor dst(dstp);
	while (count)
	{
		src.LoadUnpack(sa, sr, sg, sb);
		cbuf[wr_pos++] = (sa<<24)|(sr<<16)|(sg<<8)|sb;

		src += srcstride;
		if (src.Ptr() >= src_end)
			src.Reset(srcp);

		dst.Store(applyKernel(cbuf, rd_pos++, cbuf_mask, kernel, kernel_len));

		dst += dststride;

		count--;

		rd_pos &= cbuf_mask;
		wr_pos &= cbuf_mask;
	}
}

void VEGAFilterGaussian::realGaussRow_AW(VEGAPixelPtr dstp, unsigned int dststride,
										 VEGAPixelPtr srcp, unsigned int srcstride,
										 unsigned int count, const VEGA_FIX* kernel,
										 unsigned int half_kl)
{
	VEGAConstPixelAccessor src(srcp);
	// Calculate row end
	VEGAPixelPtr src_end = srcp + count * srcstride;

	unsigned int rd_pos, wr_pos;
	rd_pos = wr_pos = 0;

	src += calc_startoffset(count, half_kl) * srcstride;

	// Prefill with zeros before left edge
	int w = half_kl*2;
	while (w--)
	{
		cbuf_a[wr_pos++] = VEGA_UNPACK_A(src.Load());

		src += srcstride;
		if (src.Ptr() >= src_end)
			src.Reset(srcp);
	}

	unsigned int kernel_len = 1 + 2*half_kl;

	// Process output
	VEGAPixelAccessor dst(dstp);
	while (count)
	{
		cbuf_a[wr_pos++] = VEGA_UNPACK_A(src.Load());

		src += srcstride;
		if (src.Ptr() >= src_end)
			src.Reset(srcp);

		dst.Store(applyKernel_A(cbuf_a, rd_pos++, cbuf_mask, kernel, kernel_len));

		dst += dststride;

		count--;

		rd_pos &= cbuf_mask;
		wr_pos &= cbuf_mask;
	}
}

void VEGAFilterGaussian::blur_W(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf)
{
	unsigned int srcstride = srcbuf.GetPixelStride();
	unsigned int dststride = dstbuf.GetPixelStride();

	VEGAPixelAccessor dst = dstbuf.GetAccessor(0, 0);
	VEGAConstPixelAccessor src = srcbuf.GetConstAccessor(0, 0);

	unsigned width = dstbuf.width;
	unsigned height = dstbuf.height;

	unsigned int half_kw = kernel_w / 2; // floor(kernel_w / 2)

	if (kernel_w == 0)
	{
		for (unsigned int yp = 0; yp < height; ++yp)
		{
			dst.MoveFrom(src.Ptr(), width);

			src += srcstride;
			dst += dststride;
		}
	}
	else if (kernel_x)
	{
		// Small stddevs
		for (unsigned int yp = 0; yp < height; ++yp)
		{
			realGaussRow_W(dst.Ptr(), 1, src.Ptr(), 1,
						   width, kernel_x, half_kw);

			src += srcstride;
			dst += dststride;
		}
	}
	else
	{
		// Large stddevs
		if (kernel_w & 1)
		{
			// Odd
			for (unsigned int yp = 0; yp < height; ++yp)
			{
				boxBlurRow_W(dst.Ptr(), 1, src.Ptr(), 1,
							 width, half_kw, kernel_w);
				boxBlurRow_W(dst.Ptr(), 1, dst.Ptr(), 1,
							 width, half_kw, kernel_w);
				boxBlurRow_W(dst.Ptr(), 1, dst.Ptr(), 1,
							 width, half_kw, kernel_w);

				dst += dststride;
				src += srcstride;
			}
		}
		else
		{
			// Even
			for (unsigned int yp = 0; yp < height; ++yp)
			{
				boxBlurRow_W(dst.Ptr(), 1, src.Ptr(), 1,
							 width, half_kw, kernel_w); // d shift left
				boxBlurRow_W(dst.Ptr(), 1, dst.Ptr(), 1,
							 width, half_kw-1, kernel_w); // d shift right
				boxBlurRow_W(dst.Ptr(), 1, dst.Ptr(), 1,
							 width, half_kw, kernel_w+1); // d+1

				dst += dststride;
				src += srcstride;
			}
		}
	}

	unsigned int half_kh = kernel_h / 2;

	dst = dstbuf.GetAccessor(0, 0);

	if (kernel_h == 0)
	{
		return;
	}
	else if (kernel_y)
	{
		// Small stddevs
		for (unsigned int xp = 0; xp < width; ++xp)
		{
			realGaussRow_W(dst.Ptr(), dststride, dst.Ptr(), dststride,
						   height, kernel_y, half_kh);

			dst++;
		}
	}
	else
	{
		// Large stddevs
		if (kernel_h & 1)
		{
			// Odd
			for (unsigned int xp = 0; xp < width; ++xp)
			{
				boxBlurRow_W(dst.Ptr(), dststride, dst.Ptr(), dststride,
							 height, half_kh, kernel_h);
				boxBlurRow_W(dst.Ptr(), dststride, dst.Ptr(), dststride,
							 height, half_kh, kernel_h);
				boxBlurRow_W(dst.Ptr(), dststride, dst.Ptr(), dststride,
							 height, half_kh, kernel_h);

				dst++;
			}
		}
		else
		{
			// Even
			for (unsigned int xp = 0; xp < width; ++xp)
			{
				boxBlurRow_W(dst.Ptr(), dststride, dst.Ptr(), dststride,
							 height, half_kh, kernel_h); // d shift left
				boxBlurRow_W(dst.Ptr(), dststride, dst.Ptr(), dststride,
							 height, half_kh-1, kernel_h); // d shift right
				boxBlurRow_W(dst.Ptr(), dststride, dst.Ptr(), dststride,
							 height, half_kh, kernel_h+1); // d+1

				dst++;
			}
		}
	}
}

void VEGAFilterGaussian::blur_AW(const VEGASWBuffer& dstbuf, const VEGASWBuffer& srcbuf)
{
	unsigned int srcstride = srcbuf.GetPixelStride();
	unsigned int dststride = dstbuf.GetPixelStride();

	VEGAPixelAccessor dst = dstbuf.GetAccessor(0, 0);
	VEGAConstPixelAccessor src = srcbuf.GetConstAccessor(0, 0);

	unsigned width = dstbuf.width;
	unsigned height = dstbuf.height;

	unsigned int half_kw = kernel_w / 2;

	if (kernel_w == 0)
	{
		if (kernel_h == 0)
		{
			// Copy only alpha channel
			for (unsigned int yp = 0; yp < height; ++yp)
			{
				for (unsigned int xp = 0; xp < width; ++xp)
				{
					dst.StoreARGB(VEGA_UNPACK_A(src.Load()), 0, 0, 0);

					++src;
					++dst;
				}

				src += srcstride - width;
				dst += dststride - width;
			}
		}
		else
		{
			for (unsigned int yp = 0; yp < height; ++yp)
			{
				dst.MoveFrom(src.Ptr(), width);

				src += srcstride;
				dst += dststride;
			}
		}
	}
	else if (kernel_x)
	{
		// Small stddevs
		for (unsigned int yp = 0; yp < height; ++yp)
		{
			realGaussRow_AW(dst.Ptr(), 1, src.Ptr(), 1,
							width, kernel_x, half_kw);

			src += srcstride;
			dst += dststride;
		}
	}
	else
	{
		// Large stddevs
		if (kernel_w & 1)
		{
			// Odd
			for (unsigned int yp = 0; yp < height; ++yp)
			{
				boxBlurRow_AW(dst.Ptr(), 1, src.Ptr(), 1,
							  width, half_kw, kernel_w);
				boxBlurRow_AW(dst.Ptr(), 1, dst.Ptr(), 1,
							  width, half_kw, kernel_w);
				boxBlurRow_AW(dst.Ptr(), 1, dst.Ptr(), 1,
							  width, half_kw, kernel_w);

				dst += dststride;
				src += srcstride;
			}
		}
		else
		{
			// Even
			for (unsigned int yp = 0; yp < height; ++yp)
			{
				boxBlurRow_AW(dst.Ptr(), 1, src.Ptr(), 1,
							  width, half_kw, kernel_w); // d shift left
				boxBlurRow_AW(dst.Ptr(), 1, dst.Ptr(), 1,
							  width, half_kw-1, kernel_w); // d shift right
				boxBlurRow_AW(dst.Ptr(), 1, dst.Ptr(), 1,
							  width, half_kw, kernel_w+1); // d+1

				dst += dststride;
				src += srcstride;
			}
		}
	}

	dst = dstbuf.GetAccessor(0, 0);

	unsigned int half_kh = kernel_h / 2;

	if (kernel_h == 0)
	{
		return;
	}
	else if (kernel_y)
	{
		// Small stddevs
		for (unsigned int xp = 0; xp < width; ++xp)
		{
			realGaussRow_AW(dst.Ptr(), dststride, dst.Ptr(), dststride,
							height, kernel_y, half_kh);

			dst++;
		}
	}
	else
	{
		// Large stddevs
		if (kernel_h & 1)
		{
			// Odd
			for (unsigned int xp = 0; xp < width; ++xp)
			{
				boxBlurRow_AW(dst.Ptr(), dststride, dst.Ptr(), dststride,
							  height, half_kh, kernel_h);
				boxBlurRow_AW(dst.Ptr(), dststride, dst.Ptr(), dststride,
							  height, half_kh, kernel_h);
				boxBlurRow_AW(dst.Ptr(), dststride, dst.Ptr(), dststride,
							  height, half_kh, kernel_h);

				dst++;
			}
		}
		else
		{
			// Even
			for (unsigned int xp = 0; xp < width; ++xp)
			{
				boxBlurRow_AW(dst.Ptr(), dststride, dst.Ptr(), dststride,
							  height, half_kh, kernel_h); // d shift left
				boxBlurRow_AW(dst.Ptr(), dststride, dst.Ptr(), dststride,
							  height, half_kh-1, kernel_h); // d shift right
				boxBlurRow_AW(dst.Ptr(), dststride, dst.Ptr(), dststride,
							  height, half_kh, kernel_h+1); // d+1

				dst++;
			}
		}
	}
}

#ifdef VEGA_3DDEVICE
#include "modules/libvega/vega3ddevice.h"

struct VEGAMultiPassParams
{
	VEGA3dDevice* device;
	VEGA3dShaderProgram* shader;
	VEGA3dBuffer* vtxbuf;
	unsigned int startIndex;
	VEGA3dVertexLayout* vtxlayout;

	VEGA3dTexture* temporary1;
	VEGA3dFramebufferObject* temporary1RT;
	VEGA3dTexture* temporary2;
	VEGA3dFramebufferObject* temporary2RT;
	VEGA3dRenderTarget* destination;

	VEGA3dTexture* source;
	VEGA3dRenderTarget* target;

	VEGA3dDevice::Vega2dVertex* verts;

	VEGAFilterRegion region;
	unsigned int num_passes;
};

void VEGAFilterGaussian::setupSourceRect(unsigned int sx, unsigned int sy,
										 unsigned int width, unsigned int height,
										 VEGA3dTexture* tex, VEGA3dDevice::Vega2dVertex* verts)
{
	verts[0].s = (float)sx / tex->getWidth();
	verts[0].t = (float)sy / tex->getHeight();
	verts[1].s = (float)sx / tex->getWidth();
	verts[1].t = (float)(sy+height) / tex->getHeight();
	verts[2].s = (float)(sx+width) / tex->getWidth();
	verts[2].t = (float)sy / tex->getHeight();
	verts[3].s = (float)(sx+width) / tex->getWidth();
	verts[3].t = (float)(sy+height) / tex->getHeight();
}

void VEGAFilterGaussian::setupDestinationRect(unsigned int dx, unsigned int dy,
											  unsigned int width, unsigned int height,
											  VEGA3dDevice::Vega2dVertex* verts)
{
	verts[0].x = (float)dx;
	verts[0].y = (float)dy;
	verts[0].color = (sourceAlphaOnly?0xff000000:0xffffffff);
	verts[1].x = (float)dx;
	verts[1].y = (float)(dy+height);
	verts[1].color = (sourceAlphaOnly?0xff000000:0xffffffff);
	verts[2].x = (float)(dx+width);
	verts[2].y = (float)dy;
	verts[2].color = (sourceAlphaOnly?0xff000000:0xffffffff);
	verts[3].x = (float)(dx+width);
	verts[3].y = (float)(dy+height);
	verts[3].color = (sourceAlphaOnly?0xff000000:0xffffffff);
}

OP_STATUS VEGAFilterGaussian::preparePassRealGauss(unsigned int passno, VEGAMultiPassParams& params)
{
	float coeff[16*4];

	op_memset(coeff, 0, sizeof (coeff));

	VEGA3dDevice::Vega2dVertex* verts = params.verts;
	if (passno == 0)
	{
		VEGA3dTexture* srcTex = params.temporary2;
		params.device->setShaderProgram(params.shader);

		setupSourceRect(params.region.sx, params.region.sy, params.region.width, params.region.height,
						srcTex, verts);
		setupDestinationRect(1, 1, params.region.width, params.region.height, verts);

		for (int i = 0; i < kernel_w; ++i)
		{
			coeff[i*4+0] = (float)(i - kernel_w / 2) / srcTex->getWidth();
			coeff[i*4+2] = VEGA_FIXTOFLT(kernel_x[i]);
		}

		params.source = srcTex;
		params.target = params.temporary1RT;
	}
	else if (passno == params.num_passes - 1) // AKA pass "1"
	{
		VEGA3dTexture* tmpTex = params.temporary1;
		params.region.sx = params.region.sy = 1;
		params.device->setShaderProgram(params.shader);

		setupSourceRect(params.region.sx, params.region.sy, params.region.width, params.region.height, tmpTex, verts);
		setupDestinationRect(params.region.dx, params.region.dy, params.region.width, params.region.height,
							 verts);

		for (int i = 0; i < kernel_h; ++i)
		{
			coeff[i*4+1] = (float)(i - kernel_h / 2) / tmpTex->getHeight();
			coeff[i*4+2] = VEGA_FIXTOFLT(kernel_y[i]);
		}

		params.source = tmpTex;
		params.target = params.destination;
	}

	params.shader->setVector4(params.shader->getConstantLocation("coeffs"), coeff, 16);


	return params.vtxbuf->writeAnywhere(4, sizeof(VEGA3dDevice::Vega2dVertex), verts, params.startIndex);
}

static void setBoxBlurCoeffs(VEGA3dShaderProgram* shader, int kernsize, int d_idx, float pixel_size,
							 BOOL right_adjust = FALSE)
{
	OP_ASSERT(kernsize <= 32);

	float coeff[16*4];
	op_memset(coeff, 0, sizeof(coeff));

#define SET_COEFF(IDX, D, WEIGHT) coeff[(IDX)*4+d_idx] = D, coeff[(IDX)*4+2] = WEIGHT

	const float two_texels = 2.0f * pixel_size;

	// Setup initial sampling point
	float d = (0.5f - kernsize / 2) * pixel_size;

	// Setup coefficients
	if ((kernsize & 1) == 0 && right_adjust)
		d += pixel_size; // Shift starting point one sample right

	for (int i = 0; i < kernsize / 2; ++i)
	{
		SET_COEFF(i, d, 2.0f / kernsize);

		d += two_texels;
	}

	if (kernsize & 1)
		SET_COEFF(kernsize / 2, d - 0.5f * pixel_size, 1.0f / kernsize);

	shader->setVector4(shader->getConstantLocation("coeffs"), coeff, 16);

#undef SET_COEFF
}

OP_STATUS VEGAFilterGaussian::preparePassBoxBlur(unsigned int passno, VEGAMultiPassParams& params)
{
	VEGA3dDevice::Vega2dVertex* verts = params.verts;
	if (passno == 0)
	{
		VEGA3dTexture* srcTex = params.temporary2;
		params.device->setShaderProgram(params.shader);

		setupSourceRect(params.region.sx, params.region.sy, params.region.width, params.region.height,
						srcTex, verts);
		setupDestinationRect(1, 1, params.region.width, params.region.height, verts);

		setBoxBlurCoeffs(params.shader, kernel_w, 0 /* X */, 1.0f / srcTex->getWidth());

		params.source = srcTex;
		params.target = params.temporary1RT;
	}
	else if (passno == params.num_passes - 1) // Final pass
	{
		VEGA3dTexture* tmpTex = (passno&1)?params.temporary1:params.temporary2;
		params.region.sx = params.region.sy = 1;
		params.device->setShaderProgram(params.shader);

		setupSourceRect(params.region.sx, params.region.sy, params.region.width, params.region.height, tmpTex, verts);
		setupDestinationRect(params.region.dx, params.region.dy, params.region.width, params.region.height,
							 verts);

		int kernsize = kernel_h;
		if ((kernel_h & 1) == 0)
			kernsize++; // Add one if even

		setBoxBlurCoeffs(params.shader, kernsize, 1 /* Y */, 1.0f / tmpTex->getHeight());

		params.source = tmpTex;
		params.target = params.destination;
	}
	else
	{
		params.region.sx = params.region.sy = 1;
		if (passno&1)
		{
			params.source = params.temporary1;
			params.target = params.temporary2RT;
		}
		else
		{
			params.source = params.temporary2;
			params.target = params.temporary1RT;
		}
		params.device->setShaderProgram(params.shader);

		int kernsize, dir;
		BOOL adjust_right = FALSE;
		float pixel_size = 0.f;
		if (passno >= 3)
		{
			// Vertical - pass 3 and 4
			kernsize = kernel_h;
			dir = 1;

			// If the kernel size is even we need to jitter it a bit
			if ((kernsize & 1) == 0)
			{
				if (passno == 4)
					adjust_right = TRUE;
			}
			pixel_size = 1.0f / params.source->getHeight();
		}
		else
		{
			// Horizontal - pass 1 and 2
			kernsize = kernel_w;
			dir = 0;

			// If the kernel size is even we need to jitter it a bit
			if ((kernsize & 1) == 0)
			{
				if (passno == 1)
					adjust_right = TRUE;
				if (passno == 2)
					kernsize++;
			}
			pixel_size = 1.0f / params.source->getWidth();
		}

		setBoxBlurCoeffs(params.shader, kernsize, dir,
						 pixel_size, adjust_right);

		setupSourceRect(params.region.sx, params.region.sy, params.region.width, params.region.height, params.source, verts);
	}

	params.source->setFilterMode(VEGA3dTexture::FILTER_LINEAR, VEGA3dTexture::FILTER_LINEAR);

	return params.vtxbuf->writeAnywhere(4, sizeof(VEGA3dDevice::Vega2dVertex), verts, params.startIndex);
}

OP_STATUS VEGAFilterGaussian::preparePass(unsigned int passno, VEGAMultiPassParams& params)
{
	OP_ASSERT(params.num_passes == 2 || params.num_passes == 6);

	if (params.num_passes == 2)
		return preparePassRealGauss(passno, params);

	return preparePassBoxBlur(passno, params);
}

OP_STATUS VEGAFilterGaussian::applyRecursive(VEGAMultiPassParams& params)
{
	OP_STATUS err = OpStatus::OK;
	VEGA3dDevice* device = params.device;

	device->setShaderProgram(params.shader);

	for (unsigned i = 0; i < params.num_passes; ++i)
	{
		err = preparePass(i, params);
		if (OpStatus::IsError(err))
			break;

		device->setTexture(0, params.source);
		device->setRenderTarget(params.target);
		params.shader->setOrthogonalProjection();

		err = device->drawPrimitives(VEGA3dDevice::PRIMITIVE_TRIANGLE_STRIP, params.vtxlayout, params.startIndex, 4);
	}

	return err;
}

OP_STATUS VEGAFilterGaussian::apply(VEGABackingStore_FBO* destStore, const VEGAFilterRegion& region, unsigned int frame)
{
	// FIXME: Needs refining
	bool real_gauss = true;
	if (!kernel_x || !kernel_y)
	{
		if (!(!kernel_x && !kernel_y && kernel_w && kernel_h && kernel_w <= 31 && kernel_h <= 31))
			return applyFallback(destStore, region);

		real_gauss = false;
	}

	VEGAMultiPassParams params;
	params.device = g_vegaGlobals.vega3dDevice;

	if (sourceRT->getType() != VEGA3dRenderTarget::VEGA3D_RT_TEXTURE || !static_cast<VEGA3dFramebufferObject*>(sourceRT)->getAttachedColorTexture())
		return OpStatus::ERR;

	params.region = region;
	params.num_passes = real_gauss ? 1+1 : 3+3;

	params.shader = NULL;
	params.vtxbuf = NULL;
	params.vtxlayout = NULL;
	params.temporary1 = NULL;
	params.temporary1RT = NULL;
	params.temporary2 = NULL;
	params.temporary2RT = NULL;
	params.destination = destStore->GetWriteRenderTarget(frame);

	params.temporary1 = params.device->getTempTexture(region.width+2, region.height+2);
	params.temporary1RT = params.device->getTempTextureRenderTarget();
	if (!params.temporary1 || !params.temporary1RT)
		return OpStatus::ERR;
	VEGARefCount::IncRef(params.temporary1);
	VEGARefCount::IncRef(params.temporary1RT);

	params.temporary2 = params.device->createTempTexture2(region.width+2, region.height+2);
	params.temporary2RT = params.device->createTempTexture2RenderTarget();
	OP_STATUS err = OpStatus::OK;
	if (!params.temporary2 || !params.temporary2RT)
		err = OpStatus::ERR;
	else
		err = updateClampTexture(params.device, static_cast<VEGA3dFramebufferObject*>(sourceRT)->getAttachedColorTexture(), 
								params.region.sx, params.region.sy, params.region.width, params.region.height, 0,
								VEGAFILTEREDGE_NONE, params.temporary2RT, ((kernel_w>kernel_h)?kernel_w:kernel_h)/2);

	if (OpStatus::IsSuccess(err))
	{
		err = params.device->createShaderProgram(&params.shader, VEGA3dShaderProgram::SHADER_CONVOLVE_GEN_16, wrap ? VEGA3dShaderProgram::WRAP_REPEAT_REPEAT : VEGA3dShaderProgram::WRAP_CLAMP_CLAMP);
		if (OpStatus::IsSuccess(err))
		{
			params.vtxbuf = params.device->getTempBuffer(4*sizeof(VEGA3dDevice::Vega2dVertex));
			if (params.vtxbuf)
				VEGARefCount::IncRef(params.vtxbuf);
			else
				err = OpStatus::ERR;
			if (OpStatus::IsSuccess(err))
				err = params.device->createVega2dVertexLayout(&params.vtxlayout, params.shader->getShaderType());
			if (OpStatus::IsSuccess(err))
			{
				params.device->setRenderState(params.device->getDefault2dNoBlendRenderState());
				params.device->setScissor(0, 0, region.width+kernel_w/2+2, region.height+kernel_h/2+2);
				params.device->setRenderTarget(params.temporary1RT);
				params.device->clear(true, false, false, 0, 1.f, 0);
				params.device->setRenderState(params.device->getDefault2dNoBlendNoScissorRenderState());

				VEGA3dDevice::Vega2dVertex verts[4];
				params.verts = verts;

				err = applyRecursive(params);
			}
		}
	}

	// Free allocated resources
	VEGARefCount::DecRef(params.vtxlayout);
	VEGARefCount::DecRef(params.vtxbuf);

	params.device->setTexture(0, NULL);

	VEGARefCount::DecRef(params.temporary1RT);
	VEGARefCount::DecRef(params.temporary1);
	VEGARefCount::DecRef(params.temporary2RT);
	VEGARefCount::DecRef(params.temporary2);

	VEGARefCount::DecRef(params.shader);

	return err;
}
#endif // VEGA_3DDEVICE

OP_STATUS VEGAFilterGaussian::apply(const VEGASWBuffer& dest, const VEGAFilterRegion& region)
{
	VEGASWBuffer sub_src = source.CreateSubset(region.sx, region.sy, region.width, region.height);
	VEGASWBuffer sub_dst = dest.CreateSubset(region.dx, region.dy, region.width, region.height);

	// If the required kernel is even (width or height) we need an extra slot
	unsigned int cbuf_size = VEGAround_pot(MAX(kernel_h+1-(kernel_h & 1),
											   kernel_w+1-(kernel_w & 1)));
	cbuf_mask = cbuf_size - 1;

	if (!sourceAlphaOnly)
	{
		cbuf = OP_NEWA(UINT32, cbuf_size);
		if (!cbuf)
			return OpStatus::ERR_NO_MEMORY;

		if (wrap)
		{
			blur_W(sub_dst, sub_src);
		}
		else
		{
			blur(sub_dst, sub_src);
		}

		OP_DELETEA(cbuf);
	}
	else
	{
		/* Alpha Only case */
		cbuf_a = OP_NEWA(UINT8, cbuf_size);
		if (!cbuf_a)
			return OpStatus::ERR_NO_MEMORY;

		if (wrap)
		{
			blur_AW(sub_dst, sub_src);
		}
		else
		{
			blur_A(sub_dst, sub_src);
		}

		OP_DELETEA(cbuf_a);
	}

	return OpStatus::OK;
}

#endif // VEGA_SUPPORT
