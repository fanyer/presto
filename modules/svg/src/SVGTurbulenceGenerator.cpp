/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(SVG_SUPPORT)
# include "modules/svg/src/svgpch.h"
# if defined(SVG_SUPPORT_FILTERS)

#  include "modules/svg/src/SVGTurbulenceGenerator.h"
#  include "modules/svg/src/SVGRect.h"

/* Produces results in the range [1, 2**31 - 2].
 * Algorithm is: r = (a * r) mod m
 * where a = 16807 and m = 2**31 - 1 = 2147483647
 * See [Park & Miller], CACM vol. 31 no. 10 p. 1195, Oct. 1988
 * To test: the algorithm should produce the result 1043618065
 * as the 10,000th generated number if the original seed is 1.
 */

SVGTurbulenceGenerator::TurbRandom::TurbRandom(long in_seed)
{
	setup(in_seed);
}

void SVGTurbulenceGenerator::TurbRandom::setup(long in_seed)
{
	if (in_seed <= 0)
		in_seed = -(in_seed % (RAND_m - 1)) + 1;

	if (in_seed > RAND_m - 1)
		in_seed = RAND_m - 1;

	seed = in_seed;
}

long SVGTurbulenceGenerator::TurbRandom::random()
{
	seed = RAND_a * (seed % RAND_q) - RAND_r * (seed / RAND_q);
	if (seed <= 0)
		seed += RAND_m;

	return seed;
}

SVGTurbulenceGenerator::SVGTurbulenceGenerator() : m_linestate(NULL)
{
}

static inline __FP14 FPMUL_14(__FP14 a, __FP14 b) /* Fixed point mult., 14 bit result */
{
	return (__FP14)((a * b) >> 14);
}
static inline __FP28 FPMUL_28(__FP14 a, __FP14 b) /* Fixed point mult., 28 bit result */
{
	return (__FP28)(a * b);
}
static inline __FP14 INT_FP14(int a)
{
	return (__FP14)(a << 14);
}
static inline int FP14_INT(__FP14 a)
{
	return (int)(a >> 14);
}
static inline __FP14 FP_ABS(__FP14 a)
{
	return a < 0 ? -a : a;
}
static inline int FP14_RND_INT(__FP14 a)
{
	return (int)((a+(1 << 13)) >> 14);
}
static inline __FP14 FP28_FP14(__FP28 a)
{
	return (__FP14)FP14_RND_INT((__FP14)a);
}

static inline __FP14 fp28_isqrt(__FP28 v)
{
	OP_ASSERT(v >= 0);
	if (v == 0)
		return 0;
	if (v == (1 << 28))
		return INT_FP14(1);

	UINT32 root = 0;
	UINT32 rem_hi = 0;
	UINT32 rem_lo = v;
	UINT32 count = 15 + 1; /* number of iterations */

	do {
		rem_hi = (rem_hi << 2) | (rem_lo >> 30);
		rem_lo <<= 2;
		root <<= 1;
		UINT32 test_div = 2*root + 1;
		if (rem_hi >= test_div)
		{
			rem_hi -= test_div;
			root++;
		}
	} while (count-- != 0);

	return (__FP14)((root + 1) >> 1);
}

void SVGTurbulenceGenerator::Init(long seed)
{
	TurbRandom r(seed);

	for (unsigned int k = 0; k < 4; k++)
	{
		for (unsigned int i = 0; i < BSize; i++)
		{
			uLatticeSelector[i] = i;

			__FP28 len28 = 0;

			for (unsigned int j = 0; j < 2; j++)
			{
				__FP14 v = INT_FP14((r.random() % (BSize + BSize)) - BSize) / BSize;
				fGradient[k][i][j] = v;

				len28 += FPMUL_28(v, v);
			}

			__FP14 len14 = fp28_isqrt(len28);

			if (len14)
			{
				fGradient[k][i][0] <<= 14;
				fGradient[k][i][0] /= len14;

				fGradient[k][i][1] <<= 14;
				fGradient[k][i][1] /= len14;
			}
		}
	}

	int i = BSize;
	while (--i)
	{
		unsigned int k = uLatticeSelector[i];
		int j = r.random() % BSize;
		uLatticeSelector[i] = uLatticeSelector[j];
		uLatticeSelector[j] = k;
	}

	for (i = 0; i < BSize + 2; i++)
	{
		uLatticeSelector[BSize + i] = uLatticeSelector[i];

		for (int k = 0; k < 4; k++)
			for (int j = 0; j < 2; j++)
				fGradient[k][BSize + i][j] = fGradient[k][i][j];
	}
}

OP_STATUS SVGTurbulenceGenerator::SetParameters(SVGNumber fBaseFreqX, SVGNumber fBaseFreqY,
												int nNumOctaves, BOOL bFractalSum, BOOL bDoStitching,
												const SVGRect& TileArea)
{
	m_basefreq_x = fBaseFreqX;
	m_basefreq_y = fBaseFreqY;
	m_do_stitch = bDoStitching;
	m_fractal_sum = bFractalSum;
	// Cap the number of octaves at 9, since that should be enough
	m_num_octaves = MIN(nNumOctaves, 9);

	m_linestate = OP_NEWA(LineState, m_num_octaves);
	if (!m_linestate)
		return OpStatus::ERR_NO_MEMORY;

	// Adjust the base frequencies if necessary for stitching.
	if (m_do_stitch)
	{
		// When stitching tiled turbulence, the frequencies must be adjusted
		// so that the tile borders will be continuous.
		if (m_basefreq_x != 0.0)
		{
			SVGNumber fLoFreq = (TileArea.width * m_basefreq_x).floor() / TileArea.width;
			SVGNumber fHiFreq = (TileArea.width * m_basefreq_x).ceil() / TileArea.width;

			if (m_basefreq_x / fLoFreq < fHiFreq / m_basefreq_x)
				m_basefreq_x = fLoFreq;
			else
				m_basefreq_x = fHiFreq;
		}
		if (m_basefreq_y != 0.0)
		{
			SVGNumber fLoFreq = (TileArea.height * m_basefreq_y).floor() / TileArea.height;
			SVGNumber fHiFreq = (TileArea.height * m_basefreq_y).ceil() / TileArea.height;

			if(m_basefreq_y / fLoFreq < fHiFreq / m_basefreq_y)
				m_basefreq_y = fLoFreq;
			else
				m_basefreq_y = fHiFreq;
		}

		// Set up initial stitch values.
		m_initial_stitch.nWidth = (TileArea.width * m_basefreq_x).GetRoundedIntegerValue();
		m_initial_stitch.nWrapX = (TileArea.x * m_basefreq_x).GetIntegerValue() + PerlinN +
			m_initial_stitch.nWidth;

		m_initial_stitch.nHeight = (TileArea.height * m_basefreq_y).GetRoundedIntegerValue();
		m_initial_stitch.nWrapY = (TileArea.y * m_basefreq_y).GetIntegerValue() + PerlinN +
			m_initial_stitch.nHeight;
	}
	return OpStatus::OK;
}

static inline __FP14 s_curve(__FP14 t)
{
	/* s_curve(t) = t * t * (3 - 2 * t) */
	__FP14 t1 = INT_FP14(3) - 2 * t;
	__FP14 t2 = FPMUL_14(t, t);
	return FPMUL_14(t1, t2);
}

static inline __FP14 lerp(__FP14 t, __FP14 a, __FP14 b)
{
	return a + FPMUL_14(t, b - a);
}

void SVGTurbulenceGenerator::TurbulenceScanline(SVGNumber *pt, const SVGNumber& step,
												UINT32* slbuf, UINT32 slbuf_len)
{
	int stitch_wrapy = 0;
	int stitch_height = 0;
	if (m_do_stitch)
	{
		stitch_wrapy = m_initial_stitch.nWrapY;
		stitch_height = m_initial_stitch.nHeight;
	}

	// Calculate common line state for all octaves
	SVGNumber v_y = pt[1] * m_basefreq_y;
	LineState* ls = m_linestate;
	for (int octave = 0; octave < m_num_octaves; octave++, ls++)
	{
		__FP14 t = v_y.GetFixedPointValue(14) + INT_FP14(PerlinN);

		ls->by0 = FP14_INT(t);
		ls->by1 = ls->by0 + 1;

		ls->ry0 = t - INT_FP14(ls->by0); // FP14_FRAC
		ls->ry1 = ls->ry0 - INT_FP14(1);

		// If stitching, adjust lattice points accordingly.
		if (m_do_stitch)
		{
			if (ls->by0 >= stitch_wrapy)
				ls->by0 -= stitch_height;
			if (ls->by1 >= stitch_wrapy)
				ls->by1 -= stitch_height;
		}

		ls->by0 &= BSizeMask;
		ls->by1 &= BSizeMask;

		ls->sy = s_curve(ls->ry0);

		v_y *= 2;

		if (m_do_stitch)
		{
			// Update stitch values. Subtracting PerlinN before the multiplication and
			// adding it afterward simplifies to subtracting it once.
			stitch_height *= 2;
			stitch_wrapy = 2 * stitch_wrapy - PerlinN;
		}
	}

	SVGNumber freq_step = step * m_basefreq_x;
	SVGNumber freq_x = pt[0] * m_basefreq_x;

	for (UINT32 p = 0; p < slbuf_len; ++p, pt[0] += step, freq_x += freq_step)
	{
		__FP14 ft[4];
		int stitch_wrapx = 0;
		int stitch_width = 0;
		if (m_do_stitch)
		{
			stitch_wrapx = m_initial_stitch.nWrapX;
			stitch_width = m_initial_stitch.nWidth;
		}

		ft[0] = 0;
		ft[1] = 0;
		ft[2] = 0;
		ft[3] = 0;

		SVGNumber vec_x = freq_x;

		unsigned int ratio_shift = 0;

		LineState* ls = m_linestate;
		for (int octave = 0; octave < m_num_octaves; octave++, ls++)
		{
			__FP14 t = vec_x.GetFixedPointValue(14) + INT_FP14(PerlinN);

			int bx0 = FP14_INT(t);
			int bx1 = bx0 + 1;

			__FP14 rx0 = t - INT_FP14(bx0); // FP14_FRAC(t)
			__FP14 rx1 = rx0 - INT_FP14(1);

			// If stitching, adjust lattice points accordingly.
			if (m_do_stitch)
			{
				if (bx0 >= stitch_wrapx)
					bx0 -= stitch_width;
				if (bx1 >= stitch_wrapx)
					bx1 -= stitch_width;
			}

			bx0 &= BSizeMask;
			bx1 &= BSizeMask;

			int i = uLatticeSelector[bx0];
			int j = uLatticeSelector[bx1];

			int b00 = uLatticeSelector[i + ls->by0];
			int b10 = uLatticeSelector[j + ls->by0];
			int b01 = uLatticeSelector[i + ls->by1];
			int b11 = uLatticeSelector[j + ls->by1];

			__FP14 sx = s_curve(rx0);

			for (unsigned int cc = 0; cc < 4; cc++)
			{
				__FP14 *q;
				__FP14 u, v;

				q = fGradient[cc][b00];
				u = FP28_FP14(FPMUL_28(rx0, q[0]) + FPMUL_28(ls->ry0, q[1]));

				q = fGradient[cc][b10];
				v = FP28_FP14(FPMUL_28(rx1, q[0]) + FPMUL_28(ls->ry0, q[1]));

				__FP14 a = lerp(sx, u, v);

				q = fGradient[cc][b01];
				u = FP28_FP14(FPMUL_28(rx0, q[0]) + FPMUL_28(ls->ry1, q[1]));

				q = fGradient[cc][b11];
				v = FP28_FP14(FPMUL_28(rx1, q[0]) + FPMUL_28(ls->ry1, q[1]));

				__FP14 b = lerp(sx, u, v);

				__FP14 noise = lerp(ls->sy, a, b);

				if (!m_fractal_sum)
					noise = FP_ABS(noise);

				if (ratio_shift)
				{
					// Compensate if negative
					noise += (noise >> 31) & ((1 << ratio_shift) - 1);

					noise >>= ratio_shift;
				}

				ft[cc] += noise;
			}

			vec_x *= 2;
			ratio_shift++;

			if (m_do_stitch)
			{
				// Update stitch values. Subtracting PerlinN before the multiplication and
				// adding it afterward simplifies to subtracting it once.
				stitch_width *= 2;
				stitch_wrapx = 2 * stitch_wrapx - PerlinN;
			}
		}

		if (m_fractal_sum)
		{
			/* range [-1, 1] */
			ft[3] = (ft[3] + INT_FP14(1)) / 2;
			ft[0] = (ft[0] + INT_FP14(1)) / 2;
			ft[1] = (ft[1] + INT_FP14(1)) / 2;
			ft[2] = (ft[2] + INT_FP14(1)) / 2;
		}
		/* else range [0, 1] */

		/* create color values from turbulence values */
		int ta = FP14_INT(ft[3] * 255);
		int tr = FP14_INT(ft[0] * 255);
		int tg = FP14_INT(ft[1] * 255);
		int tb = FP14_INT(ft[2] * 255);

		/* clamp */
		ta = (ta > 255) ? 255 : ((ta < 0) ? 0 : ta);
		tr = (tr > 255) ? 255 : ((tr < 0) ? 0 : tr);
		tg = (tg > 255) ? 255 : ((tg < 0) ? 0 : tg);
		tb = (tb > 255) ? 255 : ((tb < 0) ? 0 : tb);

#ifdef USE_PREMULTIPLIED_ALPHA
		tr = (tr * (ta+1)) >> 8;
		tg = (tg * (ta+1)) >> 8;
		tb = (tb * (ta+1)) >> 8;
#endif // USE_PREMULTIPLIED_ALPHA

		slbuf[p] = (ta << 24) | (tr << 16) | (tg << 8) | tb;
	}
}

#endif // SVG_FILTER_SUPPORT
#endif // SVG_SUPPORT
