/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/jaypeg/jaypeg.h"

#if defined(_JPG_SUPPORT_) && defined(USE_JAYPEG) && defined(JAYPEG_JFIF_SUPPORT)

#include "modules/jaypeg/src/jayidct.h"
#include "modules/jaypeg/src/jaystream.h"

#include "modules/jaypeg/src/jaycolorlt.h"

JayIDCT::JayIDCT()
{
	int i;
	for (i = 0; i < JAYPEG_MAX_COMPONENTS; ++i)
		compTableNum[i] = 0;
	for (i = 0; i < 4; ++i)
		quantTables[i] = NULL;
}

JayIDCT::~JayIDCT()
{
	for (int i = 0; i < 4; ++i)
		OP_DELETEA(quantTables[i]);
}

const unsigned int jaypeg_zigzag[] = {
	0, 1, 5, 6, 14, 15, 27, 28,
	2, 4, 7, 13, 16, 26, 29, 42,
	3, 8, 12, 17, 25, 30, 41, 43,
	9, 11, 18, 24, 31, 40, 44, 53,
	10, 19, 23, 32, 39, 45, 52, 54,
	20, 22, 33, 38, 46, 51, 55, 60,
	21, 34, 37, 47, 50, 56, 59, 61,
	35, 36, 48, 49, 57, 58, 62, 63
};

/* This table should be:
 [1/8						(sqrt(2)/8) * cos(pi/16)		(sqrt(2)/8) * cos(2*pi/16)		(sqrt(2)/8) * cos(3*pi/16)	...]
 [(sqrt(2)/8) * cos(pi/16)	(1/4) * cos(pi/16) * cos(pi/16)	(1/4) * cos(2*pi/16) * cos(pi/16) ...]*/


// fixedpoint. The values are multiplied with 2048
static const unsigned int jaypeg_idft_const[] = {
	256, 355, 334, 301, 256, 201, 138, 71,
	355, 493, 464, 418, 355, 279, 192, 98,
	334, 464, 437, 393, 334, 263, 181, 92,
	301, 418, 393, 354, 301, 237, 163, 83,
	256, 355, 334, 301, 256, 201, 139, 71,
	201, 279, 263, 237, 201, 158, 109, 55,
	138, 192, 181, 163, 139, 109, 75,  38,
	71,  98,  92,  83,  71,  55,  38,  19
};

#ifdef JAYPEG_FAST_BUT_LOSSY
// Pre-shifted 3 steps
#define JAYPEG_C2 473
#define JAYPEG_C4 362
#define JAYPEG_C6 196
#define JAYPEG_C4C2 669
#define JAYPEG_C4C6 277
#define JAYPEG_Q 277 // C2-C6
#define JAYPEG_R 669 // C2+C6
#define JAYPEG_C4Q 392
#define JAYPEG_C4R 946

#define jayConstMul(sample, c) (((sample)>>8)*(c))
/*static inline int jayConstMul(int sample, unsigned short c)
{
	return ((sample>>8)*c);
}*/

#else
#define JAYPEG_C2 3784
#define JAYPEG_C4 2896
#define JAYPEG_C6 1567
#define JAYPEG_C4C2 5352
#define JAYPEG_C4C6 2217
#define JAYPEG_Q 2217 // C2-C6
#define JAYPEG_R 5352 // C2+C6
#define JAYPEG_C4Q 3135
#define JAYPEG_C4R 7568

static inline int jayConstMul(int sample, unsigned short c)
{
	bool positive = true;
	unsigned long a, b;

	if (sample < 0){
		sample = -sample;
		positive = false;
	}

	a = sample>>16;
	b = sample&0xffff;

	a *= c;
	b *= c;

	a <<= 5;
	b >>= 11;

	if (!positive)
		return -(int)(a+b);
	return a + b;
}
#endif

#define JROW0 0
#define JROW1 8
#define JROW2 16
#define JROW3 24
#define JROW4 32
#define JROW5 40
#define JROW6 48
#define JROW7 56

static const unsigned short noQuantTable[64] = {
	1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1
};

void JayIDCT::transform(unsigned int component, unsigned int numSamples, unsigned int stride, short *samples, unsigned char *oSamples)
{
	int outputSamples[64];

	// this idct is an implementation of the algorithm found at:
	// http://rnvs.informatik.tu-chemnitz.de/~jan/MPEG/HTML/IDCT.html
	// which converts the samples to dft samples and does an inverted fourier transform instead of idct
	// It describes Arai, Agiu and Nakajima and some extensions by Feig

	const unsigned short* qt = quantTables[compTableNum[component]];
	if (!qt)
		qt = noQuantTable;
#if 1
	// This path does not used the extensions by Feig. It is much faster to not use them and
	// instead skip the cases where all ac samples are zero

	// row based...
	int row, col;

	// Apply the idct for all columns
	for (col = 0; col < 8; ++col)
	{
		if (samples[jaypeg_zigzag[col+JROW1]]==0 && samples[jaypeg_zigzag[col+JROW2]]==0 && samples[jaypeg_zigzag[col+JROW3]]==0 && 
			samples[jaypeg_zigzag[col+JROW4]]==0 && samples[jaypeg_zigzag[col+JROW5]]==0 && samples[jaypeg_zigzag[col+JROW6]]==0 && 
			samples[jaypeg_zigzag[col+JROW7]]==0)
		{
			int res = (int)samples[jaypeg_zigzag[col]]*(int)qt[col];
			outputSamples[col] = res;
			outputSamples[col+JROW1] = res;
			outputSamples[col+JROW2] = res;
			outputSamples[col+JROW3] = res;
			outputSamples[col+JROW4] = res;
			outputSamples[col+JROW5] = res;
			outputSamples[col+JROW6] = res;
			outputSamples[col+JROW7] = res;
		}
		else
		{
			int tmp_a0, tmp_a1, tmp_a2, tmp_a3;
			int tmp_m1, tmp_m2, tmp_m3, 
				tmp_m4, tmp_m5, tmp_m6, tmp_m7;
			outputSamples[col] = (int)samples[jaypeg_zigzag[col]]*(int)qt[col];
			outputSamples[col+JROW1] = (int)samples[jaypeg_zigzag[col+JROW1]]*(int)qt[col+JROW1];
			outputSamples[col+JROW2] = (int)samples[jaypeg_zigzag[col+JROW2]]*(int)qt[col+JROW2];
			outputSamples[col+JROW3] = (int)samples[jaypeg_zigzag[col+JROW3]]*(int)qt[col+JROW3];
			outputSamples[col+JROW4] = (int)samples[jaypeg_zigzag[col+JROW4]]*(int)qt[col+JROW4];
			outputSamples[col+JROW5] = (int)samples[jaypeg_zigzag[col+JROW5]]*(int)qt[col+JROW5];
			outputSamples[col+JROW6] = (int)samples[jaypeg_zigzag[col+JROW6]]*(int)qt[col+JROW6];
			outputSamples[col+JROW7] = (int)samples[jaypeg_zigzag[col+JROW7]]*(int)qt[col+JROW7];

			// a0 to a3
			// B1
			tmp_a0 = outputSamples[col];
			tmp_a1 = outputSamples[col+JROW4];
			tmp_a2 = outputSamples[col+JROW2] - outputSamples[col+JROW6];
			tmp_a3 = outputSamples[col+JROW2] + outputSamples[col+JROW6];

			// M + A1 + A2
			// n1
			tmp_m3 = tmp_a0-tmp_a1;
			// n2
			tmp_m4 = jayConstMul(tmp_a2, JAYPEG_C4)-tmp_a3;
			tmp_m5 = tmp_m3-tmp_m4;
			tmp_m3 += tmp_m4;
			// n3
			tmp_m4 = tmp_a0+tmp_a1;
			// a3 is n6
			tmp_m6 = tmp_m4-tmp_a3;
			tmp_m4 += tmp_a3;

			// a4 to a7
			// B1 (and parts of M)
			tmp_a0 = outputSamples[col+JROW5] - outputSamples[col+JROW3];
			tmp_a1 = outputSamples[col+JROW1] + outputSamples[col+JROW7];
			tmp_a2 = outputSamples[col+JROW3] + outputSamples[col+JROW5];
			tmp_a3 = tmp_a1 + tmp_a2;
			tmp_a1 = jayConstMul(tmp_a1-tmp_a2, JAYPEG_C4);
			tmp_a2 = outputSamples[col+JROW1] - outputSamples[col+JROW7];

			// M
			tmp_m1 = jayConstMul(tmp_a0+tmp_a2, JAYPEG_C6);
			tmp_a0 = jayConstMul(-tmp_a0, JAYPEG_Q) - tmp_m1;
			tmp_a2 = jayConstMul(tmp_a2, JAYPEG_R) - tmp_m1;

			// A1 + A2
			tmp_m2 = tmp_a2-tmp_a3;
			tmp_m1 = tmp_m2-tmp_a1;
			//tmp_m0 = tmp_a3;
			tmp_m7 = tmp_a0-tmp_m1;

			outputSamples[col] = tmp_m4 + tmp_a3;
			outputSamples[col+JROW1] = tmp_m3 + tmp_m2;
			outputSamples[col+JROW2] = tmp_m5 - tmp_m1;
			outputSamples[col+JROW3] = tmp_m6 - tmp_m7;
			outputSamples[col+JROW4] = tmp_m6 + tmp_m7;
			outputSamples[col+JROW5] = tmp_m5 + tmp_m1;
			outputSamples[col+JROW6] = tmp_m3 - tmp_m2;
			outputSamples[col+JROW7] = tmp_m4 - tmp_a3;

		}
	}
	unsigned char* sampleRes = oSamples;
	for (row = 0; row < 64; row+=8)
	{
		if (outputSamples[row+1]==0 && outputSamples[row+2]==0 && outputSamples[row+3]==0 && 
			outputSamples[row+4]==0 && outputSamples[row+5]==0 && outputSamples[row+6]==0 && 
			outputSamples[row+7]==0)
		{
			int res = JAY_CLAMP(((outputSamples[row]+1024)>>11)+128);
			sampleRes[0] = res;
			sampleRes[1] = res;
			sampleRes[2] = res;
			sampleRes[3] = res;
			sampleRes[4] = res;
			sampleRes[5] = res;
			sampleRes[6] = res;
			sampleRes[7] = res;
		}
		else
		{
			int tmp_a0, tmp_a1, tmp_a2, tmp_a3;
			int tmp_m1, tmp_m2, tmp_m3, 
				tmp_m4, tmp_m5, tmp_m6, tmp_m7;

			// a0 to a3
			// B1
			tmp_a0 = outputSamples[row];
			tmp_a1 = outputSamples[row+4];
			tmp_a2 = outputSamples[row+2] - outputSamples[row+6];
			tmp_a3 = outputSamples[row+2] + outputSamples[row+6];

			// M + A1 + A2
			// n1
			tmp_m3 = tmp_a0-tmp_a1;
			// n2
			tmp_m4 = jayConstMul(tmp_a2, JAYPEG_C4)-tmp_a3;
			tmp_m5 = tmp_m3-tmp_m4;
			tmp_m3 += tmp_m4;
			// n3
			tmp_m4 = tmp_a0+tmp_a1;
			// a3 is n6
			tmp_m6 = tmp_m4-tmp_a3;
			tmp_m4 += tmp_a3;

			// a4 to a7
			// B1 (and parts of M)
			tmp_a0 = outputSamples[row+5] - outputSamples[row+3];
			tmp_a1 = outputSamples[row+1] + outputSamples[row+7];
			tmp_a2 = outputSamples[row+3] + outputSamples[row+5];
			tmp_a3 = tmp_a1 + tmp_a2;
			tmp_a1 = jayConstMul(tmp_a1-tmp_a2, JAYPEG_C4);
			tmp_a2 = outputSamples[row+1] - outputSamples[row+7];

			// M
			tmp_m1 = jayConstMul(tmp_a0+tmp_a2, JAYPEG_C6);
			tmp_a0 = jayConstMul(-tmp_a0, JAYPEG_Q) - tmp_m1;
			tmp_a2 = jayConstMul(tmp_a2, JAYPEG_R) - tmp_m1;

			// A1 + A2
			tmp_m2 = tmp_a2-tmp_a3;
			tmp_m1 = tmp_m2-tmp_a1;
			//tmp_m0 = tmp_a3;
			tmp_m7 = tmp_a0-tmp_m1;

			sampleRes[0] = JAY_CLAMP(((tmp_m4 + tmp_a3+1024)>>11)+128);
			sampleRes[1] = JAY_CLAMP(((tmp_m3 + tmp_m2+1024)>>11)+128);
			sampleRes[2] = JAY_CLAMP(((tmp_m5 - tmp_m1+1024)>>11)+128);
			sampleRes[3] = JAY_CLAMP(((tmp_m6 - tmp_m7+1024)>>11)+128);
			sampleRes[4] = JAY_CLAMP(((tmp_m6 + tmp_m7+1024)>>11)+128);
			sampleRes[5] = JAY_CLAMP(((tmp_m5 + tmp_m1+1024)>>11)+128);
			sampleRes[6] = JAY_CLAMP(((tmp_m3 - tmp_m2+1024)>>11)+128);
			sampleRes[7] = JAY_CLAMP(((tmp_m4 - tmp_a3+1024)>>11)+128);
		}
		sampleRes += stride;
	}
#else
	// row based...
	int row, col;
	// do the b1 step from the alorithm
	for (row = 0; row < 64; row+=8)
	{
		outputSamples[row] = (int)samples[jaypeg_zigzag[row]]*(int)qt[row];
		outputSamples[row+1] = (int)samples[jaypeg_zigzag[row+1]];
		outputSamples[row+2] = (int)samples[jaypeg_zigzag[row+2]];
		outputSamples[row+3] = (int)samples[jaypeg_zigzag[row+3]];
		outputSamples[row+4] = (int)samples[jaypeg_zigzag[row+4]];
		outputSamples[row+5] = (int)samples[jaypeg_zigzag[row+5]];
		outputSamples[row+6] = (int)samples[jaypeg_zigzag[row+6]];
		outputSamples[row+7] = (int)samples[jaypeg_zigzag[row+7]];
		if (outputSamples[row+1]!=0 || outputSamples[row+2]!=0 || outputSamples[row+3]!=0 || 
			outputSamples[row+4]!=0 || outputSamples[row+5]!=0 || outputSamples[row+6]!=0 || 
			outputSamples[row+7]!=0)
		{
			// Occurs about 50% of the time. The other time there is no need to do the dequantization
			outputSamples[row+1] *= (int)qt[row+1];
			outputSamples[row+2] *= (int)qt[row+2];
			outputSamples[row+3] *= (int)qt[row+3];
			outputSamples[row+4] *= (int)qt[row+4];
			outputSamples[row+5] *= (int)qt[row+5];
			outputSamples[row+6] *= (int)qt[row+6];
			outputSamples[row+7] *= (int)qt[row+7];
			int tmp1 = (outputSamples[row+1]+outputSamples[row+7]);
			int tmp2 = (outputSamples[row+3]+outputSamples[row+5]);
			int tmp3 = (outputSamples[row+1]-outputSamples[row+7]);
			
			outputSamples[row+1] = outputSamples[row+4];
			outputSamples[row+4] = (outputSamples[row+5]-outputSamples[row+3]);

			outputSamples[row+3] = (outputSamples[row+2]+outputSamples[row+6]);
			outputSamples[row+2] = (outputSamples[row+2]-outputSamples[row+6]);


			outputSamples[row+5] = tmp1-tmp2;
			outputSamples[row+6] = tmp3;
			outputSamples[row+7] = tmp1+tmp2;
		}
	}
	for (col = 0; col < 8; ++col)
	{
		if (outputSamples[col+JROW1]!=0 || outputSamples[col+JROW2]!=0 || outputSamples[col+JROW3]!=0 || 
			outputSamples[col+JROW4]!=0 || outputSamples[col+JROW5]!=0 || outputSamples[col+JROW6]!=0 || 
			outputSamples[col+JROW7]!=0)
		{
			// Occurs about 66% of the time
			int tmp1 = (outputSamples[col+JROW1]+outputSamples[col+JROW7]);
			int tmp2 = (outputSamples[col+JROW3]+outputSamples[col+JROW5]);
			int tmp3 = (outputSamples[col+JROW1]-outputSamples[col+JROW7]);

			outputSamples[col+JROW1] = outputSamples[col+JROW4];
			outputSamples[col+JROW4] = (outputSamples[col+JROW5]-outputSamples[col+JROW3]);

			outputSamples[col+JROW3] = (outputSamples[col+JROW2]+outputSamples[col+JROW6]);
			outputSamples[col+JROW2] = (outputSamples[col+JROW2]-outputSamples[col+JROW6]);

			outputSamples[col+JROW5] = tmp1-tmp2;
			outputSamples[col+JROW6] = tmp3;
			outputSamples[col+JROW7] = tmp1+tmp2;
		}
	}
	// do the m step from the alorithm
	// Line 1, 2, 4 and 8
	int tmp4;
	tmp4 = jayConstMul(outputSamples[JROW0+4]+outputSamples[JROW0+6], JAYPEG_C6);
	outputSamples[JROW0+2] = jayConstMul(outputSamples[JROW0+2], JAYPEG_C4);
	outputSamples[JROW0+4] = -jayConstMul(outputSamples[JROW0+4], JAYPEG_Q)-tmp4;
	outputSamples[JROW0+5] = jayConstMul(outputSamples[JROW0+5], JAYPEG_C4);
	outputSamples[JROW0+6] = jayConstMul(outputSamples[JROW0+6], JAYPEG_R)-tmp4;

	tmp4 = jayConstMul(outputSamples[JROW1+4]+outputSamples[JROW1+6], JAYPEG_C6);
	outputSamples[JROW1+2] = jayConstMul(outputSamples[JROW1+2], JAYPEG_C4);
	outputSamples[JROW1+4] = -jayConstMul(outputSamples[JROW1+4], JAYPEG_Q)-tmp4;
	outputSamples[JROW1+5] = jayConstMul(outputSamples[JROW1+5], JAYPEG_C4);
	outputSamples[JROW1+6] = jayConstMul(outputSamples[JROW1+6], JAYPEG_R)-tmp4;

	tmp4 = jayConstMul(outputSamples[JROW3+4]+outputSamples[JROW3+6], JAYPEG_C6);
	outputSamples[JROW3+2] = jayConstMul(outputSamples[JROW3+2], JAYPEG_C4);
	outputSamples[JROW3+4] = -jayConstMul(outputSamples[JROW3+4], JAYPEG_Q)-tmp4;
	outputSamples[JROW3+5] = jayConstMul(outputSamples[JROW3+5], JAYPEG_C4);
	outputSamples[JROW3+6] = jayConstMul(outputSamples[JROW3+6], JAYPEG_R)-tmp4;

	tmp4 = jayConstMul(outputSamples[JROW7+4]+outputSamples[JROW7+6], JAYPEG_C6);
	outputSamples[JROW7+2] = jayConstMul(outputSamples[JROW7+2], JAYPEG_C4);
	outputSamples[JROW7+4] = -jayConstMul(outputSamples[JROW7+4], JAYPEG_Q)-tmp4;
	outputSamples[JROW7+5] = jayConstMul(outputSamples[JROW7+5], JAYPEG_C4);
	outputSamples[JROW7+6] = jayConstMul(outputSamples[JROW7+6], JAYPEG_R)-tmp4;

	// Line 3 and 6
	tmp4 = jayConstMul(outputSamples[JROW2+4]+outputSamples[JROW2+6], JAYPEG_C4C6);
	outputSamples[JROW2+0] = jayConstMul(outputSamples[JROW2+0], JAYPEG_C4);
	outputSamples[JROW2+1] = jayConstMul(outputSamples[JROW2+1], JAYPEG_C4);
	outputSamples[JROW2+2] = outputSamples[JROW2+2]<<1;
	outputSamples[JROW2+3] = jayConstMul(outputSamples[JROW2+3], JAYPEG_C4);
	outputSamples[JROW2+4] = -jayConstMul(outputSamples[JROW2+4], JAYPEG_C4Q)-tmp4;
	outputSamples[JROW2+5] = outputSamples[JROW2+5]<<1;
	outputSamples[JROW2+6] = jayConstMul(outputSamples[JROW2+6], JAYPEG_C4R)-tmp4;
	outputSamples[JROW2+7] = jayConstMul(outputSamples[JROW2+7], JAYPEG_C4);

	tmp4 = jayConstMul(outputSamples[JROW5+4]+outputSamples[JROW5+6], JAYPEG_C4C6);
	outputSamples[JROW5+0] = jayConstMul(outputSamples[JROW5+0], JAYPEG_C4);
	outputSamples[JROW5+1] = jayConstMul(outputSamples[JROW5+1], JAYPEG_C4);
	outputSamples[JROW5+2] = outputSamples[JROW5+2]<<1;
	outputSamples[JROW5+3] = jayConstMul(outputSamples[JROW5+3], JAYPEG_C4);
	outputSamples[JROW5+4] = -jayConstMul(outputSamples[JROW5+4], JAYPEG_C4Q)-tmp4;
	outputSamples[JROW5+5] = outputSamples[JROW5+5]<<1;
	outputSamples[JROW5+6] = jayConstMul(outputSamples[JROW5+6], JAYPEG_C4R)-tmp4;
	outputSamples[JROW5+7] = jayConstMul(outputSamples[JROW5+7], JAYPEG_C4);

	// Line 5 and 7
	// without element 5 and 7
	tmp4 = outputSamples[JROW4 + 0];
	outputSamples[JROW4+0] = -jayConstMul(tmp4, JAYPEG_C2)-jayConstMul(outputSamples[JROW6 + 0], JAYPEG_C6);
	outputSamples[JROW6+0] = -jayConstMul(tmp4, JAYPEG_C6)+jayConstMul(outputSamples[JROW6 + 0], JAYPEG_C2);
	/*if (outputSamples[JROW4+1]==0 && outputSamples[JROW4+2]==0 && 
		outputSamples[JROW4+3]==0 && outputSamples[JROW4+4]==0 &&
		outputSamples[JROW4+5]==0 && outputSamples[JROW4+6]==0 &&
		outputSamples[JROW4+7]==0)
	{
		// This occurs about 50% of the time, so skip some multiplications here
		outputSamples[JROW4 + 1] = -jayConstMul(outputSamples[JROW6 + 1], JAYPEG_C6);
		outputSamples[JROW6 + 1] = jayConstMul(outputSamples[JROW6 + 1], JAYPEG_C2);

		outputSamples[JROW4 + 2] = -jayConstMul(outputSamples[JROW6 + 2], JAYPEG_C4C6);
		outputSamples[JROW6 + 2] = jayConstMul(outputSamples[JROW6 + 2], JAYPEG_C4C2);

		outputSamples[JROW4 + 3] = -jayConstMul(outputSamples[JROW6 + 3], JAYPEG_C6);
		outputSamples[JROW6 + 3] = jayConstMul(outputSamples[JROW6 + 3], JAYPEG_C2);

		outputSamples[JROW4 + 5] = -jayConstMul(outputSamples[JROW6 + 5], JAYPEG_C4C6);
		outputSamples[JROW6 + 5] = jayConstMul(outputSamples[JROW6 + 5], JAYPEG_C4C2);

		outputSamples[JROW4 + 7] = -jayConstMul(outputSamples[JROW6 + 7], JAYPEG_C6);
		outputSamples[JROW6 + 7] = jayConstMul(outputSamples[JROW6 + 7], JAYPEG_C2);

		// element 5 and 7
		{
			int b[4];
			b[0] = jayConstMul(-outputSamples[JROW6 + 6]+outputSamples[JROW6 + 4], JAYPEG_C4);
			b[1] = jayConstMul(-outputSamples[JROW6 + 6]-outputSamples[JROW6 + 4], JAYPEG_C4);
			b[2] = outputSamples[JROW6 + 6]<<1;
			b[3] = (-outputSamples[JROW6 + 4])<<1;
			outputSamples[JROW4 + 4] = b[0]+b[2];
			outputSamples[JROW4 + 6] = b[1]-b[3];
			outputSamples[JROW6 + 4] = b[1]+b[3];
			outputSamples[JROW6 + 6] = -b[0]+b[2];
		}
	}
	else*/
	{
		tmp4 = outputSamples[JROW4 + 1];
		outputSamples[JROW4 + 1] = -jayConstMul(tmp4, JAYPEG_C2)-jayConstMul(outputSamples[JROW6 + 1], JAYPEG_C6);
		outputSamples[JROW6 + 1] = -jayConstMul(tmp4, JAYPEG_C6)+jayConstMul(outputSamples[JROW6 + 1], JAYPEG_C2);

		tmp4 = outputSamples[JROW4 + 2];
		outputSamples[JROW4 + 2] = -jayConstMul(tmp4, JAYPEG_C4C2)-jayConstMul(outputSamples[JROW6 + 2], JAYPEG_C4C6);
		outputSamples[JROW6 + 2] = -jayConstMul(tmp4, JAYPEG_C4C6)+jayConstMul(outputSamples[JROW6 + 2], JAYPEG_C4C2);

		tmp4 = outputSamples[JROW4 + 3];
		outputSamples[JROW4 + 3] = -jayConstMul(tmp4, JAYPEG_C2)-jayConstMul(outputSamples[JROW6 + 3], JAYPEG_C6);
		outputSamples[JROW6 + 3] = -jayConstMul(tmp4, JAYPEG_C6)+jayConstMul(outputSamples[JROW6 + 3], JAYPEG_C2);

		tmp4 = outputSamples[JROW4 + 5];
		outputSamples[JROW4 + 5] = -jayConstMul(tmp4, JAYPEG_C4C2)-jayConstMul(outputSamples[JROW6 + 5], JAYPEG_C4C6);
		outputSamples[JROW6 + 5] = -jayConstMul(tmp4, JAYPEG_C4C6)+jayConstMul(outputSamples[JROW6 + 5], JAYPEG_C4C2);

		tmp4 = outputSamples[JROW4 + 7];
		outputSamples[JROW4 + 7] = -jayConstMul(tmp4, JAYPEG_C2)-jayConstMul(outputSamples[JROW6 + 7], JAYPEG_C6);
		outputSamples[JROW6 + 7] = -jayConstMul(tmp4, JAYPEG_C6)+jayConstMul(outputSamples[JROW6 + 7], JAYPEG_C2);

		// element 5 and 7
		{
			int a[2], b[4];
			a[0] = outputSamples[JROW4 + 4] - outputSamples[JROW6 + 6];
			a[1] = outputSamples[JROW4 + 6] + outputSamples[JROW6 + 4];
			b[0] = jayConstMul(a[0]+a[1], JAYPEG_C4);
			b[1] = jayConstMul(a[0]-a[1], JAYPEG_C4);
			b[2] = (outputSamples[JROW4 + 4] + outputSamples[JROW6 + 6])<<1;
			b[3] = (outputSamples[JROW4 + 6] - outputSamples[JROW6 + 4])<<1;
			outputSamples[JROW4 + 4] = b[0]+b[2];
			outputSamples[JROW4 + 6] = b[1]-b[3];
			outputSamples[JROW6 + 4] = b[1]+b[3];
			outputSamples[JROW6 + 6] = -b[0]+b[2];
		}
	}
	// do the ax step from the algorithm
//	}
	for (col = 0; col < 8; ++col)
	{
		int m[8], tmp;

		m[0] = outputSamples[col+JROW7];
		tmp = outputSamples[col+JROW6]-outputSamples[col+JROW7];
		m[1] = tmp-outputSamples[col+JROW5];
		m[2] = tmp;
		m[3] = outputSamples[col+JROW0]-outputSamples[col+JROW1];
		m[5] = m[3];
		tmp = outputSamples[col+JROW2]-outputSamples[col+JROW3];
		m[3] += tmp;
		m[5] -= tmp;
		m[4] = outputSamples[col+JROW0]+outputSamples[col+JROW1];
		m[6] = m[4];
		m[4] += outputSamples[col+JROW3];
		m[6] -= outputSamples[col+JROW3];
		m[7] = outputSamples[col+JROW4]-m[1];

		outputSamples[col+JROW0] = m[4]+m[0];
		outputSamples[col+JROW1] = m[3]+m[2];
		outputSamples[col+JROW2] = m[5]-m[1];
		outputSamples[col+JROW3] = m[6]-m[7];
		outputSamples[col+JROW4] = m[6]+m[7];
		outputSamples[col+JROW5] = m[5]+m[1];
		outputSamples[col+JROW6] = m[3]-m[2];
		outputSamples[col+JROW7] = m[4]-m[0];
	}

	unsigned int offs = 0;
	for (row = 0; row < 64; row+=8)
	{
		int m[8], tmp;

		m[0] = outputSamples[row+7];
		tmp = outputSamples[row+6]-outputSamples[row+7];
		m[1] = tmp-outputSamples[row+5];
		m[2] = tmp;
		m[3] = outputSamples[row+0]-outputSamples[row+1];
		m[5] = m[3];
		tmp = outputSamples[row+2]-outputSamples[row+3];
		m[3] += tmp;
		m[5] -= tmp;
		m[4] = outputSamples[row+0]+outputSamples[row+1];
		m[6] = m[4];
		m[4] += outputSamples[row+3];
		m[6] -= outputSamples[row+3];
		m[7] = outputSamples[row+4]-m[1];

		/*outputSamples[row+0] = m[4]+m[0];
		outputSamples[row+1] = m[3]+m[2];
		outputSamples[row+2] = m[5]-m[1];
		outputSamples[row+3] = m[6]-m[7];
		outputSamples[row+4] = m[6]+m[7];
		outputSamples[row+5] = m[5]+m[1];
		outputSamples[row+6] = m[3]-m[2];
		outputSamples[row+7] = m[4]-m[0];*/

		oSamples[offs] = JAY_CLAMP((((m[4]+m[0])+1024)>>11)+128);
		oSamples[offs+1] = JAY_CLAMP((((m[3]+m[2])+1024)>>11)+128);
		oSamples[offs+2] = JAY_CLAMP((((m[5]-m[1])+1024)>>11)+128);
		oSamples[offs+3] = JAY_CLAMP((((m[6]-m[7])+1024)>>11)+128);
		oSamples[offs+4] = JAY_CLAMP((((m[6]+m[7])+1024)>>11)+128);
		oSamples[offs+5] = JAY_CLAMP((((m[5]+m[1])+1024)>>11)+128);
		oSamples[offs+6] = JAY_CLAMP((((m[3]-m[2])+1024)>>11)+128);
		oSamples[offs+7] = JAY_CLAMP((((m[4]-m[0])+1024)>>11)+128);
		offs += stride;
	}

	/*unsigned int offs = 0;
	for (unsigned int i = 0; i < 64; i+=8)
	{

		oSamples[offs] = JAY_CLAMP(((outputSamples[i]+1024)>>11)+128);
		oSamples[offs+1] = JAY_CLAMP(((outputSamples[i+1]+1024)>>11)+128);
		oSamples[offs+2] = JAY_CLAMP(((outputSamples[i+2]+1024)>>11)+128);
		oSamples[offs+3] = JAY_CLAMP(((outputSamples[i+3]+1024)>>11)+128);
		oSamples[offs+4] = JAY_CLAMP(((outputSamples[i+4]+1024)>>11)+128);
		oSamples[offs+5] = JAY_CLAMP(((outputSamples[i+5]+1024)>>11)+128);
		oSamples[offs+6] = JAY_CLAMP(((outputSamples[i+6]+1024)>>11)+128);
		oSamples[offs+7] = JAY_CLAMP(((outputSamples[i+7]+1024)>>11)+128);

		offs += stride;
	}*/
#endif
}

// Dequantization
void JayIDCT::setComponentTableNum(unsigned int component, unsigned char tableNum)
{
	compTableNum[component] = tableNum;
}

int JayIDCT::readDQT(JayStream *stream)
{
	if (stream->getLength() < 4)
		return JAYPEG_NOT_ENOUGH_DATA;
	unsigned int len = (stream->getBuffer()[2]<<8) | stream->getBuffer()[3];
	if (stream->getLength() < len+2)
		return JAYPEG_NOT_ENOUGH_DATA;
	stream->advanceBuffer(4);
	len -= 2;
	while (len > 0)
	{
		if (len < 65)
		{
			OP_ASSERT(FALSE);
			return JAYPEG_ERR;
		}
		if ((stream->getBuffer()[0]&0xf0) != 0 && (stream->getBuffer()[0]&0xf0) != 0x10)
		{
			OP_ASSERT(FALSE);
			return JAYPEG_ERR;
		}
		BOOL sample16 = !!(stream->getBuffer()[0]>>4);
		if (sample16 && len < 129)
		{
			OP_ASSERT(FALSE);
			return JAYPEG_ERR;
		}
		unsigned int tabnum = stream->getBuffer()[0]&0xf;
		if (tabnum >= 4)
		{
			OP_ASSERT(FALSE);
			return JAYPEG_ERR;
		}
		stream->advanceBuffer(1);
		len -= 1;

		// allocate the table
		OP_DELETEA(quantTables[tabnum]);
		quantTables[tabnum] = OP_NEWA(unsigned short, 64);
		if (!quantTables[tabnum])
		{
			return JAYPEG_ERR_NO_MEMORY;
		}

		// read the data
		if (sample16)
		{
			for (unsigned int i = 0; i < 64; ++i)
			{
				int sig = jaypeg_zigzag[i];
				quantTables[tabnum][i] = ((stream->getBuffer()[sig*2]<<8) | stream->getBuffer()[sig*2+1]) * jaypeg_idft_const[i];
			}
			stream->advanceBuffer(128);
			len -= 128;
		}
		else
		{
			for (unsigned int i = 0; i < 64; ++i)
			{
				int sig = jaypeg_zigzag[i];
				quantTables[tabnum][i] = stream->getBuffer()[sig] * jaypeg_idft_const[i];
			}
			stream->advanceBuffer(64);
			len -= 64;
		}
	}
	return JAYPEG_OK;
}

#endif

