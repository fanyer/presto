/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

/**
 * Helper for the feTurbulence filter primitive
 *
 * Based on the algorithm/code presented in the SVG (1.1) spec. (section 15.24)
 */

#include "modules/svg/src/svgpch.h"

#if defined(SVG_SUPPORT) && defined(SVG_SUPPORT_FILTERS)

#ifndef SVG_TURBULENCE_GENERATOR_H
#define SVG_TURBULENCE_GENERATOR_H

class SVGRect;

typedef INT32 __FP14;
typedef INT32 __FP28;

class SVGTurbulenceGenerator
{
public:
	SVGTurbulenceGenerator();
	~SVGTurbulenceGenerator() { OP_DELETEA(m_linestate); }

	void Init(long seed);

	OP_STATUS SetParameters(SVGNumber fBaseFreqX, SVGNumber fBaseFreqY,
							int nNumOctaves, BOOL bFractalSum, BOOL bDoStitching,
							const SVGRect& TileArea);

	void TurbulenceScanline(SVGNumber *pt, const SVGNumber& step,
							UINT32* slbuf, UINT32 slbuf_len);

protected:
	struct StitchInfo
	{
		int nWidth; // How much to subtract to wrap for stitching.
		int nHeight;
		int nWrapX; // Minimum value to wrap.
		int nWrapY;
	};

	/* Pseudo-random number generator for the turbulence generator */
	class TurbRandom
	{
	public:
		TurbRandom(long in_seed);
		
		void setup(long in_seed);
		long random();
	
	private:
		long seed;

		enum { 
			RAND_m = 2147483647,	// = 2147483647; /* 2**31 - 1 */
			RAND_a = 16807,			// = 16807; /* 7**5; primitive root of m */
			RAND_q = 127773,		// = 127773; /* m / a */
			RAND_r = 2836			// = 2836; /* m % a */
		};
	};

	enum
	{
		BSize		= 0x100,
		BSizeMask	= 0xff,   /* BSize - 1 */
		PerlinN		= 0x1000, /* 2^NP */
		NP			= 12,
		NMask		= 0xfff   /* 2^NP-1 */
	};

	unsigned int uLatticeSelector[0x100 + 0x100 /*BSize + BSize*/ + 2];

	__FP14 fGradient[4][0x100 + 0x100 /*BSize + BSize*/ + 2][2];

	StitchInfo m_initial_stitch;

	SVGNumber m_basefreq_x;
	SVGNumber m_basefreq_y;

	BOOL m_do_stitch;
	BOOL m_fractal_sum;

	int m_num_octaves;

	// Precalced line state
	struct LineState
	{
		__FP14 ry0, ry1;
		__FP14 sy;
		int by0, by1;
	};

	LineState* m_linestate;
};

#endif // SVG_TURBULENCE_GENERATOR_H
#endif // SVG_SUPPORT && SVG_SUPPORT_FILTERS
