/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/jaypeg/jaypeg.h"
#if defined(_JPG_SUPPORT_) && defined(USE_JAYPEG) && defined(JAYPEG_JFIF_SUPPORT)

#include "modules/jaypeg/src/jayjfifdecoder.h"

#include "modules/jaypeg/src/jayjfifmarkers.h"

#include "modules/jaypeg/src/jaystream.h"

#include "modules/jaypeg/jayimage.h"

#include "modules/jaypeg/src/jaycolorlt.h"

// jay_clamp_data is declared in modules/jaypeg/src/jaycolorlt.h and
// used by the macro JAY_CLAMP and g_jay_clamp
// add 256 to the values and look up the value in this table
const unsigned char jay_clamp_data[] =
{ 0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,1,2,3,4,
  5,6,7,8,9,10,11,12,13,14,
  15,16,17,18,19,20,21,22,23,24,
  25,26,27,28,29,30,31,32,33,34,
  35,36,37,38,39,40,41,42,43,44,
  45,46,47,48,49,50,51,52,53,54,
  55,56,57,58,59,60,61,62,63,64,
  65,66,67,68,69,70,71,72,73,74,
  75,76,77,78,79,80,81,82,83,84,
  85,86,87,88,89,90,91,92,93,94,
  95,96,97,98,99,100,101,102,103,104,
  105,106,107,108,109,110,111,112,113,114,
  115,116,117,118,119,120,121,122,123,124,
  125,126,127,128,129,130,131,132,133,134,
  135,136,137,138,139,140,141,142,143,144,
  145,146,147,148,149,150,151,152,153,154,
  155,156,157,158,159,160,161,162,163,164,
  165,166,167,168,169,170,171,172,173,174,
  175,176,177,178,179,180,181,182,183,184,
  185,186,187,188,189,190,191,192,193,194,
  195,196,197,198,199,200,201,202,203,204,
  205,206,207,208,209,210,211,212,213,214,
  215,216,217,218,219,220,221,222,223,224,
  225,226,227,228,229,230,231,232,233,234,
  235,236,237,238,239,240,241,242,243,244,
  245,246,247,248,249,250,251,252,253,254,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255};

static const short jay_cr_to_r[] =
{
-179,
-178,
-176,
-175,
-173,
-172,
-171,
-169,
-168,
-166,
-165,
-164,
-162,
-161,
-159,
-158,
-157,
-155,
-154,
-152,
-151,
-150,
-148,
-147,
-145,
-144,
-143,
-141,
-140,
-138,
-137,
-136,
-134,
-133,
-131,
-130,
-129,
-127,
-126,
-124,
-123,
-122,
-120,
-119,
-117,
-116,
-114,
-113,
-112,
-110,
-109,
-107,
-106,
-105,
-103,
-102,
-100,
-99,
-98,
-96,
-95,
-93,
-92,
-91,
-89,
-88,
-86,
-85,
-84,
-82,
-81,
-79,
-78,
-77,
-75,
-74,
-72,
-71,
-70,
-68,
-67,
-65,
-64,
-63,
-61,
-60,
-58,
-57,
-56,
-54,
-53,
-51,
-50,
-49,
-47,
-46,
-44,
-43,
-42,
-40,
-39,
-37,
-36,
-35,
-33,
-32,
-30,
-29,
-28,
-26,
-25,
-23,
-22,
-21,
-19,
-18,
-16,
-15,
-14,
-12,
-11,
-9,
-8,
-7,
-5,
-4,
-2,
-1,
0,
1,
2,
4,
5,
7,
8,
9,
11,
12,
14,
15,
16,
18,
19,
21,
22,
23,
25,
26,
28,
29,
30,
32,
33,
35,
36,
37,
39,
40,
42,
43,
44,
46,
47,
49,
50,
51,
53,
54,
56,
57,
58,
60,
61,
63,
64,
65,
67,
68,
70,
71,
72,
74,
75,
77,
78,
79,
81,
82,
84,
85,
86,
88,
89,
91,
92,
93,
95,
96,
98,
99,
100,
102,
103,
105,
106,
107,
109,
110,
112,
113,
114,
116,
117,
119,
120,
122,
123,
124,
126,
127,
129,
130,
131,
133,
134,
136,
137,
138,
140,
141,
143,
144,
145,
147,
148,
150,
151,
152,
154,
155,
157,
158,
159,
161,
162,
164,
165,
166,
168,
169,
171,
172,
173,
175,
176,
178
};

static const short jay_cb_to_b[] =
{
-226,
-225,
-223,
-221,
-219,
-217,
-216,
-214,
-212,
-210,
-209,
-207,
-205,
-203,
-202,
-200,
-198,
-196,
-194,
-193,
-191,
-189,
-187,
-186,
-184,
-182,
-180,
-178,
-177,
-175,
-173,
-171,
-170,
-168,
-166,
-164,
-163,
-161,
-159,
-157,
-155,
-154,
-152,
-150,
-148,
-147,
-145,
-143,
-141,
-139,
-138,
-136,
-134,
-132,
-131,
-129,
-127,
-125,
-124,
-122,
-120,
-118,
-116,
-115,
-113,
-111,
-109,
-108,
-106,
-104,
-102,
-101,
-99,
-97,
-95,
-93,
-92,
-90,
-88,
-86,
-85,
-83,
-81,
-79,
-77,
-76,
-74,
-72,
-70,
-69,
-67,
-65,
-63,
-62,
-60,
-58,
-56,
-54,
-53,
-51,
-49,
-47,
-46,
-44,
-42,
-40,
-38,
-37,
-35,
-33,
-31,
-30,
-28,
-26,
-24,
-23,
-21,
-19,
-17,
-15,
-14,
-12,
-10,
-8,
-7,
-5,
-3,
-1,
0,
1,
3,
5,
7,
8,
10,
12,
14,
15,
17,
19,
21,
23,
24,
26,
28,
30,
31,
33,
35,
37,
38,
40,
42,
44,
46,
47,
49,
51,
53,
54,
56,
58,
60,
62,
63,
65,
67,
69,
70,
72,
74,
76,
77,
79,
81,
83,
85,
86,
88,
90,
92,
93,
95,
97,
99,
101,
102,
104,
106,
108,
109,
111,
113,
115,
116,
118,
120,
122,
124,
125,
127,
129,
131,
132,
134,
136,
138,
139,
141,
143,
145,
147,
148,
150,
152,
154,
155,
157,
159,
161,
163,
164,
166,
168,
170,
171,
173,
175,
177,
178,
180,
182,
184,
186,
187,
189,
191,
193,
194,
196,
198,
200,
202,
203,
205,
207,
209,
210,
212,
214,
216,
217,
219,
221,
223,
225
};

JayJFIFDecoder::JayJFIFDecoder(BOOL enable_decoding) :
		decode_data(enable_decoding), markerlength(0), skipCount(0),
		restartInterval(0), restartCount(0), entropyDecoder(NULL), image(NULL),
		curComponent(0), curComponentCount(0), maxVertRes(0), maxHorizRes(0), scanline(NULL),
#ifndef JAYPEG_LOW_QUALITY_SCALE
		scaleCache(NULL),
#endif // !JAYPEG_LOW_QUALITY_SCALE
		width(0), height(0), numComponents(0), sampleSize(0),
		progressive(FALSE), interlaced(TRUE), lastStartedMCURow(0), lastWrittenMCURow(-1),
		wrappedMCU(FALSE), samples_width(0), samples_height(0),
#ifdef EMBEDDED_ICC_SUPPORT
		skipAPP2(FALSE), lastIccChunk(0), totalIccChunks(0),
#endif // EMBEDDED_ICC_SUPPORT
		jfifHeader(FALSE), adobeHeader(FALSE), colorSpace(JAYPEG_COLORSPACE_UNKNOWN), state(JAYPEG_JFIF_STATE_NONE)
{
	for (int i = 0; i < JAYPEG_MAX_COMPONENTS; ++i)
	{
		compVertRes[i] = 0;
		compHorizRes[i] = 0;
		compDataUnitCount[i] = 0;
		progressionSamples[i] = NULL;
		mcuRow[i] = NULL;
	}
}

JayJFIFDecoder::~JayJFIFDecoder()
{
	OP_DELETEA(scanline);
#ifndef JAYPEG_LOW_QUALITY_SCALE
	OP_DELETEA(scaleCache);
#endif // !JAYPEG_LOW_QUALITY_SCALE

	for (int cc = 0; cc < numComponents; ++cc)
	{
		OP_DELETEA(mcuRow[cc]);
		if (progressionSamples[cc])
		{
			int vb = ((samples_height/8)*compVertRes[cc])/maxVertRes;

			for (int i = 0; i < vb; ++i)
			{
				OP_DELETEA(progressionSamples[cc][i]);
			}
			OP_DELETEA(progressionSamples[cc]);
		}
	}
}

#ifdef EMBEDDED_ICC_SUPPORT
OP_STATUS JayJFIFDecoder::ICCDataBuffer::Append(const UINT8* d, unsigned dlen)
{
	if (dlen > m_datasize - m_datalen)
	{
		unsigned new_size = m_datasize + dlen * 2;
		UINT8* new_data = OP_NEWA(UINT8, new_size);
		if (!new_data)
			return OpStatus::ERR_NO_MEMORY;

		if (m_data)
			op_memcpy(new_data, m_data, m_datalen);

		OP_DELETEA(m_data);
		m_data = new_data;
		m_datasize = new_size;
	}

	op_memcpy(m_data + m_datalen, d, dlen);

	m_datalen += dlen;
	return OpStatus::OK;
}
#endif // EMBEDDED_ICC_SUPPORT

BOOL JayJFIFDecoder::init(JayImage *image)
{
	this->image = image;
	return TRUE;
}

BOOL JayJFIFDecoder::isDone()
{
	return state == JAYPEG_JFIF_STATE_DONE;
}

BOOL JayJFIFDecoder::isFlushed()
{
	int lwr = lastWrittenMCURow+1;
	int blockh = 0;
	if (maxVertRes > 0)
	{
		blockh = samples_height/(8*maxVertRes);
	}
	if (lwr >= blockh)
		lwr = 0;
	if (progressive && (wrappedMCU || lastStartedMCURow != lwr))
		return FALSE;
	return TRUE;
}

// FIXME: this does not work for interleaving of some components, it's either all or just one
void JayJFIFDecoder::startMCURow()
{
	lastStartedMCURow = mcuRowNum;
	if (numComponents > 1 && numScanComponents == 1)
		lastStartedMCURow /= compVertRes[scanComponents[curComponent]];
	if (lastStartedMCURow == lastWrittenMCURow)
		wrappedMCU = TRUE;
	// add all the scanlines
	// sum of mcus per block
	if (numScanComponents == 1)
	{
		int cc = scanComponents[curComponent];
		dataUnitsPerMCURow = ((compHorizRes[cc]*width+maxHorizRes-1)/maxHorizRes+7)/8;
		compDataUnitCount[scanComponents[curComponent]] = 0;
		return;
	}
	dataUnitsPerMCURow = 0;
	for (int comp = 0; comp < numScanComponents; ++comp)
	{
		dataUnitsPerMCURow += compHorizRes[scanComponents[comp]]*compVertRes[scanComponents[comp]];
		compDataUnitCount[scanComponents[comp]] = 0;
	}
	// number of blocks per line of blocks
	dataUnitsPerMCURow *= (samples_width/(8*maxHorizRes));
}

#ifndef JAYPEG_LOW_QUALITY_SCALE

void jay_qscale_h1v1(const unsigned char* in, unsigned char* out, int nc, int width)
{
	for (int xp = 0; xp < width; ++xp)
	{
		*out = *in;
		++in;
		out += nc;
	}
}

void jay_qscale_h2v1(const unsigned char* in, unsigned char* out, int nc, int width)
{
	// First pixel
	*out = *in;
	out += nc;

	// reduce width by one since one is already calculated
	--width;
	int w = width/2;
	for (int xp = 0; xp < w; ++xp)
	{
		int o0, o1;
		o0 = ((*in)*3+2)>>2;
		o1 = (*in+2)>>2;
		++in;
		o0 += ((*in)+2)>>2;
		o1 += (((*in)*3)+2)>>2;
		*out = o0;
		out += nc;
		*out = o1;
		out += nc;
	}
	// fill in the last position if needed
	if (width&1)
	{
		*out = *in;
	}
}

/** in1 is the closes line, which will be 3/4 of the output. in2 is the further line which will be 1/4 of the output. */
void jay_qscale_h2v2(const unsigned char* in1, const unsigned char* in2, unsigned char* out, int nc, int width)
{
	// First pixel
	*out = (((*in1)*3)+*in2+2)>>2;
	out += nc;

	// reduce width by one since one is already calculated
	--width;
	int w = width/2;
	for (int xp = 0; xp < w; ++xp)
	{
		int o0, o1;
		o0 = (*in1)*9;
		o1 = (*in1)*3;
		++in1;
		o0 += (*in1)*3;
		o1 += (*in1)*9;

		o0 += (*in2)*3;
		o1 += *in2;
		++in2;
		o0 += *in2;
		o1 += (*in2)*3;

		*out = (o0+8)>>4;
		out += nc;
		*out = (o1+8)>>4;
		out += nc;
	}
	// fill in the last position if needed
	if (width&1)
	{
		*out = (((*in1)*3)+*in2+2)>>2;
	}
}
#endif // !JAYPEG_LOW_QUALITY_SCALE

void JayJFIFDecoder::writeMCURow(int rownum)
{
	lastWrittenMCURow = rownum;

	int mcurh = 8;
	unsigned int ypos[JAYPEG_MAX_COMPONENTS];
	unsigned int yinc[JAYPEG_MAX_COMPONENTS];
	unsigned int xpos[JAYPEG_MAX_COMPONENTS];
	unsigned int xinc[JAYPEG_MAX_COMPONENTS];
	if (numComponents > 1)
	{
		mcurh = maxVertRes*8;
		// 11 bit fixed point
		ypos[0] = 0;
		yinc[0] = (compVertRes[0]<<11)/maxVertRes;
		xinc[0] = (compHorizRes[0]<<11)/maxHorizRes;
		ypos[1] = 0;
		yinc[1] = (compVertRes[1]<<11)/maxVertRes;
		xinc[1] = (compHorizRes[1]<<11)/maxHorizRes;
		ypos[2] = 0;
		yinc[2] = (compVertRes[2]<<11)/maxVertRes;
		xinc[2] = (compHorizRes[2]<<11)/maxHorizRes;
		ypos[3] = 0;
		yinc[3] = (compVertRes[3]<<11)/maxVertRes;
		xinc[3] = (compHorizRes[3]<<11)/maxHorizRes;
	}
	else
	{
		ypos[0] = ypos[1] = ypos[2] = ypos[3] = 0;
		xpos[0] = xpos[1] = xpos[2] = xpos[3] = 0;
		yinc[0] = yinc[1] = yinc[2] = yinc[3] = 0;
		xinc[0] = xinc[1] = xinc[2] = xinc[3] = 0;
	}
#ifndef JAYPEG_LOW_QUALITY_SCALE
	BOOL cache_done = FALSE;
#endif // !JAYPEG_LOW_QUALITY_SCALE
	int rowstart = mcurh*rownum;
	for (int i = 0; i < mcurh; ++i)
	{
		if (rowstart+i < height)
		{
			if ((numComponents == 3 && colorSpace == JAYPEG_COLORSPACE_YCRCB) ||
				(numComponents == 4 && colorSpace == JAYPEG_COLORSPACE_YCCK))
			{
				unsigned char *sl = scanline;
				unsigned char *yrow = mcuRow[0] + (ypos[0]>>11)*compWidthInBlocks[0]*8;
				ypos[0] += yinc[0];
				unsigned char *cbrow = mcuRow[1] + (ypos[1]>>11)*compWidthInBlocks[1]*8;
				ypos[1] += yinc[1];
				unsigned char *crrow = mcuRow[2] + (ypos[2]>>11)*compWidthInBlocks[2]*8;
				ypos[2] += yinc[2];
				unsigned char *krow = mcuRow[3] + (ypos[3]>>11)*compWidthInBlocks[3]*8;
				ypos[3] += yinc[3];
				// The two most common cases are special cases here
				if (compHorizRes[0] == 2 && compHorizRes[1] == 1 && compHorizRes[2] == 1 && numComponents == 3)
				{
#ifndef JAYPEG_LOW_QUALITY_SCALE
					if (compVertRes[0] == 2 && compVertRes[1] == 1 && compVertRes[2] == 1)
					{
						if ((i == 0 && rowstart == 0) || (i == mcurh-1 && rowstart == height-mcurh))
						{
							jay_qscale_h1v1(mcuRow[0] + compWidthInBlocks[0]*8*i, scanline, 3, width);
							jay_qscale_h2v1(mcuRow[1] + compWidthInBlocks[1]*8*(i/2), scanline+1, 3, width);
							jay_qscale_h2v1(mcuRow[2] + compWidthInBlocks[2]*8*(i/2), scanline+2, 3, width);
						}
						else if (i == 0)
						{
							if (!cache_done)
							{
								jay_qscale_h2v2(scaleCache, mcuRow[1], scanline+1, 3, width);
								jay_qscale_h2v2(scaleCache+(width+1)/2, mcuRow[2], scanline+2, 3, width);
								--i;
								cache_done = TRUE;
							}
							else
							{
								jay_qscale_h1v1(mcuRow[0], scanline, 3, width);
								jay_qscale_h2v2(mcuRow[1], scaleCache, scanline+1, 3, width);
								jay_qscale_h2v2(mcuRow[2], scaleCache+(width+1)/2, scanline+2, 3, width);
							}
						}
						else if (i == mcurh-1)
						{
							// add stuff to the cache
							jay_qscale_h1v1(mcuRow[0] + compWidthInBlocks[0]*8*i, scanline, 3, width);
							jay_qscale_h1v1(mcuRow[1] + compWidthInBlocks[1]*8*(i/2), scaleCache, 1, (width+1)/2);
							jay_qscale_h1v1(mcuRow[2] + compWidthInBlocks[2]*8*(i/2), scaleCache+(width+1)/2, 1, (width+1)/2);
							return;
						}
						else
						{
							jay_qscale_h1v1(mcuRow[0] + compWidthInBlocks[0]*8*i, scanline, 3, width);
							if (i&1)
							{
								jay_qscale_h2v2(mcuRow[1] + compWidthInBlocks[1]*8*((i-1)/2), mcuRow[1] + compWidthInBlocks[1]*8*((i-1)/2+1), scanline+1, 3, width);
								jay_qscale_h2v2(mcuRow[2] + compWidthInBlocks[2]*8*((i-1)/2), mcuRow[2] + compWidthInBlocks[2]*8*((i-1)/2+1), scanline+2, 3, width);
							}
							else
							{
								jay_qscale_h2v2(mcuRow[1] + compWidthInBlocks[1]*8*((i-1)/2+1), mcuRow[1] + compWidthInBlocks[1]*8*((i-1)/2), scanline+1, 3, width);
								jay_qscale_h2v2(mcuRow[2] + compWidthInBlocks[2]*8*((i-1)/2+1), mcuRow[2] + compWidthInBlocks[2]*8*((i-1)/2), scanline+2, 3, width);
							}
						}
						unsigned char* sl = scanline;
						for (int xp = 0; xp < width; ++xp)
						{
							int y = sl[0], cb = sl[1], cr = sl[2];

							int crcb_r = jay_cr_to_r[cr];
							int crcb_b = jay_cb_to_b[cb];
							cb -=128;
							cr -=128;
							int crcb_g = (705*cb + 1463*cr + 1024)>>11;

							*sl = g_jay_clamp[y + crcb_b];
							++sl;
							*sl = g_jay_clamp[y - crcb_g];
							++sl;
							*sl = g_jay_clamp[y + crcb_r];
							++sl;
						}
					}
					else if (compVertRes[0] == 1 && compVertRes[1] == 1 && compVertRes[2] == 1)
					{
						jay_qscale_h1v1(mcuRow[0] + compWidthInBlocks[0]*8*i, scanline, 3, width);
						jay_qscale_h2v1(mcuRow[1] + compWidthInBlocks[1]*8*i, scanline+1, 3, width);
						jay_qscale_h2v1(mcuRow[2] + compWidthInBlocks[2]*8*i, scanline+2, 3, width);
						unsigned char* sl = scanline;
						for (int xp = 0; xp < width; ++xp)
						{
							int y = sl[0], cb = sl[1], cr = sl[2];

							int crcb_r = jay_cr_to_r[cr];
							int crcb_b = jay_cb_to_b[cb];
							cb -=128;
							cr -=128;
							int crcb_g = (705*cb + 1463*cr + 1024)>>11;

							*sl = g_jay_clamp[y + crcb_b];
							++sl;
							*sl = g_jay_clamp[y - crcb_g];
							++sl;
							*sl = g_jay_clamp[y + crcb_r];
							++sl;
						}
					}
					else
#endif // !JAYPEG_LOW_QUALITY_SCALE
					{
						int w = width >> 1;
						for (int xp = 0; xp < w; ++xp)
						{
							int y = yrow[xp<<1], cb = cbrow[xp], cr = crrow[xp];

							int crcb_r = jay_cr_to_r[cr];
							int crcb_b = jay_cb_to_b[cb];
							cb -=128;
							cr -=128;
							int crcb_g = (705*cb + 1463*cr + 1024)>>11;

							*sl = g_jay_clamp[y + crcb_b];
							++sl;
							*sl = g_jay_clamp[y - crcb_g];
							++sl;
							*sl = g_jay_clamp[y + crcb_r];
							++sl;

							y = yrow[(xp<<1)+1];
							*sl = g_jay_clamp[y + crcb_b];
							++sl;
							*sl = g_jay_clamp[y - crcb_g];
							++sl;
							*sl = g_jay_clamp[y + crcb_r];
							++sl;
						}
						if (width&1)
						{
							int y = yrow[width-1],
								cb = cbrow[(width-1)>>1],
								cr = crrow[(width-1)>>1];

							*sl = g_jay_clamp[y + jay_cb_to_b[cb]];
							++sl;
							cb -=128;
							cr -=128;
							*sl = g_jay_clamp[y - ((705*cb + 1463*cr + 1024)>>11)];
							++sl;
							*sl = g_jay_clamp[y + jay_cr_to_r[cr+128]];
							++sl;
						}
					}
				}
				else if (compHorizRes[0] == 1 && compHorizRes[1] == 1 && compHorizRes[2] == 1 && (numComponents == 3 || compHorizRes[3] == 1))
				{
					for (int xp = 0; xp < width; ++xp)
					{
						int y = yrow[xp], cb = cbrow[xp], cr = crrow[xp];

						*sl = g_jay_clamp[y + jay_cb_to_b[cb]];
						++sl;
						cb -=128;
						cr -=128;
						*sl = g_jay_clamp[y - ((705*cb + 1463*cr + 1024)>>11)];
						++sl;
						*sl = g_jay_clamp[y + jay_cr_to_r[cr+128]];
						++sl;
					}
					if (numComponents == 4)
					{
						sl = scanline;
						for (int xp = 0; xp < width; ++xp)
						{
							*sl = g_jay_clamp[((255-*sl)*krow[xp])>>8];
							++sl;
							*sl = g_jay_clamp[((255-*sl)*krow[xp])>>8];
							++sl;
							*sl = g_jay_clamp[((255-*sl)*krow[xp])>>8];
							++sl;
						}
					}
				}
				else
				{
					xpos[0] = 0;
					xpos[1] = 0;
					xpos[2] = 0;
					for (int xp = 0; xp < width; ++xp)
					{
						int y, cb, cr;
						int tx = xpos[0]>>11;
						xpos[0] += xinc[0];
						y = yrow[tx];
						tx = xpos[1]>>11;
						xpos[1] += xinc[1];
						cb = cbrow[tx];
						tx = xpos[2]>>11;
						xpos[2] += xinc[2];
						cr = crrow[tx];

						*sl = g_jay_clamp[y + jay_cb_to_b[cb]];
						++sl;
						cb -=128;
						cr -=128;
						*sl = g_jay_clamp[y - ((705*cb + 1463*cr + 1024)>>11)];
						++sl;
						*sl = g_jay_clamp[y + jay_cr_to_r[cr+128]];
						++sl;
					}
					if (numComponents == 4)
					{
						xpos[3] = 0;
						sl = scanline;
						for (int xp = 0; xp < width; ++xp)
						{
							int tx = xpos[3]>>11;
							xpos[3] += xinc[3];
							*sl = g_jay_clamp[((255-*sl)*krow[tx])>>8];
							++sl;
							*sl = g_jay_clamp[((255-*sl)*krow[tx])>>8];
							++sl;
							*sl = g_jay_clamp[((255-*sl)*krow[tx])>>8];
							++sl;
						}
					}
				}
				image->scanlineReady(rowstart+i, scanline);
			}
			else if ((numComponents == 3 && colorSpace == JAYPEG_COLORSPACE_RGB) ||
				(numComponents == 4 && colorSpace == JAYPEG_COLORSPACE_CMYK))
			{
				unsigned char *sl = scanline;
				unsigned char *rrow = mcuRow[0] + (ypos[0]>>11)*compWidthInBlocks[0]*8;
				ypos[0] += yinc[0];
				unsigned char *grow = mcuRow[1] + (ypos[1]>>11)*compWidthInBlocks[1]*8;
				ypos[1] += yinc[1];
				unsigned char *brow = mcuRow[2] + (ypos[2]>>11)*compWidthInBlocks[2]*8;
				ypos[2] += yinc[2];
				unsigned char *krow = mcuRow[3] + (ypos[3]>>11)*compWidthInBlocks[3]*8;
				ypos[3] += yinc[3];
				xpos[0] = 0;
				xpos[1] = 0;
				xpos[2] = 0;
				for (int xp = 0; xp < width; ++xp)
				{
					int tx;
					tx = xpos[2]>>11;
					xpos[2] += xinc[2];
					*sl = brow[tx];
					++sl;
					tx = xpos[1]>>11;
					xpos[1] += xinc[1];
					*sl = grow[tx];
					++sl;
					tx = xpos[0]>>11;
					xpos[0] += xinc[0];
					*sl = rrow[tx];
					++sl;
				}
				if (numComponents == 4)
				{
					xpos[3] = 0;
					sl = scanline;
					// Adobe writes CMYK inverted compared to everyone else.
					// This code assumes that the precense of an adobe header
					// means the cmyk is inverted
					if (adobeHeader)
					{
						for (int xp = 0; xp < width; ++xp)
						{
							int tx = xpos[3]>>11;
							xpos[3] += xinc[3];
							*sl = g_jay_clamp[((*sl)*krow[tx])>>8];
							++sl;
							*sl = g_jay_clamp[((*sl)*krow[tx])>>8];
							++sl;
							*sl = g_jay_clamp[((*sl)*krow[tx])>>8];
							++sl;
						}
					}
					else
					{
						for (int xp = 0; xp < width; ++xp)
						{
							int tx = xpos[3]>>11;
							xpos[3] += xinc[3];
							*sl = g_jay_clamp[((255-*sl)*krow[tx])>>8];
							++sl;
							*sl = g_jay_clamp[((255-*sl)*krow[tx])>>8];
							++sl;
							*sl = g_jay_clamp[((255-*sl)*krow[tx])>>8];
							++sl;
						}
					}
				}
				image->scanlineReady(rowstart+i, scanline);
			}
			else if (numComponents == 1)
			{
				OP_ASSERT(colorSpace == JAYPEG_COLORSPACE_GRAYSCALE);
				image->scanlineReady(rowstart+i, mcuRow[0]+i*compWidthInBlocks[0]*8);
			}
			else
			{
				OP_ASSERT(!"Unsupported colorspace");
			}
		}
	}
}

void JayJFIFDecoder::flushProgressive()
{
	int lwr = lastWrittenMCURow+1;
	int blockh = 0;
	if (maxVertRes > 0)
	{
		blockh = samples_height/(8*maxVertRes);
	}
	if (lwr >= blockh)
		lwr = 0;
	if (progressive && (wrappedMCU || lastStartedMCURow != lwr))
	{
		// If the decoding has wrapped we always want to start from 0
		// If we do not the linear interpolation cache in writeMCU will
		// be used unintialized
		if (wrappedMCU)
			lastStartedMCURow = 0;
		// write the image..
		int row = wrappedMCU?0:lwr;
		int stopval = lastStartedMCURow;
		while (row != stopval || wrappedMCU)
		{
			if (!image->wantMoreData())
			{
				return;
			}
			for (int cc = 0; cc < numComponents; ++cc)
			{
				for (int ducnt = 0;
					ducnt < compVertRes[cc]*compWidthInBlocks[cc];
					++ducnt)
				{
					int yb = ducnt/compWidthInBlocks[cc];
					int xb = ducnt%compWidthInBlocks[cc];
					OP_ASSERT(row*compVertRes[cc]+yb < ((samples_height/8)*compVertRes[cc])/maxVertRes);
					short *samp = progressionSamples[cc][row*compVertRes[cc]+yb];
					samp += xb*64;

					if (xb < compWidthInBlocks[cc])
					{
						unsigned char *decsamples = mcuRow[cc]+yb*compWidthInBlocks[cc]*64+xb*8;
						trans.transform(cc, 64, compWidthInBlocks[cc]*8, samp, decsamples);
					}
				}
			}

			writeMCURow(row);

			++row;
			if (row >= blockh)
			{
				row = 0;
			}
			wrappedMCU = FALSE;
		}
	}
}

#ifdef JAYPEG_EXIF_SUPPORT
static unsigned short readExif16(const unsigned char* exifData, BOOL bigEndian)
{
	if (bigEndian)
	{
		return ((exifData[0]<<8) | exifData[1]);
	}
	return ((exifData[1]<<8) | exifData[0]);
}
static unsigned int readExif32(const unsigned char* exifData, BOOL bigEndian)
{
	if (bigEndian)
	{
		return ((exifData[0]<<24) | (exifData[1]<<16) | (exifData[2]<<8) | exifData[3]);
	}
	return ((exifData[3]<<24) | (exifData[2]<<16) | (exifData[1]<<8) | exifData[0]);
}

#define JAYPEG_EXIF_VALUE_BYTE 1
#define JAYPEG_EXIF_VALUE_ASCII 2
#define JAYPEG_EXIF_VALUE_SHORT 3
#define JAYPEG_EXIF_VALUE_LONG 4
#define JAYPEG_EXIF_VALUE_RATIONAL 5
#define JAYPEG_EXIF_VALUE_UNDEFINED 7
#define JAYPEG_EXIF_VALUE_SLONG 9
#define JAYPEG_EXIF_VALUE_SRATIONAL 10

int JayJFIFDecoder::decodeExif(const unsigned char* exifData, unsigned int exifLen)
{
	unsigned int i;
	char tmpstr[16]; /* ARRAY OK 2009-03-23 wonko */
	unsigned int offset = 0;
	if (exifLen < 8)
	{
		return JAYPEG_ERR;
	}
	BOOL bigEndian = FALSE;
	if (exifData[0] == 'M' && exifData[1] == 'M')
	{
		bigEndian = TRUE;
	}
	else if (exifData[0] != 'I' || exifData[1] != 'I')
	{
		return JAYPEG_ERR;
	}
	if (readExif16(exifData+2, bigEndian) != 42)
	{
		return JAYPEG_ERR;
	}
	offset = readExif32(exifData+4, bigEndian);

	// read number of fields first
	if (offset+2 > exifLen)
		return JAYPEG_ERR;
	unsigned int numFields = readExif16(exifData+offset, bigEndian);
	offset += 2;
	// Detect overflows
	if (offset < 2)
		return JAYPEG_ERR;
	unsigned int exifIFD = 0;
	unsigned int gpsIFD = 0;
	unsigned int interopIFD = 0;
	int curIFD = 0;
	while (offset > 0)
	{
		while (numFields > 0 && offset+12 <= exifLen)
		{
			// Detect overflow
			if (offset+12 < 12)
				return JAYPEG_ERR;
			unsigned short tag = readExif16(exifData+offset, bigEndian);
			unsigned short type;
			unsigned int count;
			unsigned int voffset;
			unsigned int countoffs;
			switch (tag)
			{
			case JAYPEG_EXIF_EXIF_IFD:
				if (curIFD == 0)
				{
					exifIFD = readExif32(exifData+offset+8, bigEndian);
				}
				break;
			case JAYPEG_EXIF_GPS_IFD:
				if (curIFD == 0)
				{
					gpsIFD = readExif32(exifData+offset+8, bigEndian);
				}
				break;
			case JAYPEG_EXIF_INTEROP_IFD:
				if (curIFD == 1)
				{
					interopIFD = readExif32(exifData+offset+8, bigEndian);
				}
				break;
			default:
				// read the value to a string and report it to the image
				type = readExif16(exifData+offset+2, bigEndian);
				count = readExif32(exifData+offset+4, bigEndian);
				switch (type)
				{
				case JAYPEG_EXIF_VALUE_BYTE:
					if (count > 4)
						voffset = readExif32(exifData+offset+8, bigEndian);
					else
						voffset = offset+8;
					// Detect overflow
					if (voffset+count < voffset || voffset+count < count)
						return JAYPEG_ERR;
					if (voffset+count <= exifLen)
					{
						// parse the values to a string..
						// determine the length of the string
						int stringlen = 1;
						for (i = 0; i < count; ++i)
						{
							stringlen += op_snprintf(tmpstr, 16, "%u", exifData[voffset+i]);
							if (i > 0)
								stringlen += 2;
						}
						char* string = OP_NEWA(char, stringlen+1);
						if (string)
						{
							*string = 0;
							int curlen = 0;
							for (i = 0; i < count; ++i)
							{
								curlen += op_snprintf(string+curlen, stringlen+1-curlen, "%s%u", i>0?", ":"", exifData[voffset+i]);
							}
							image->exifDataFound(tag, string);
							OP_DELETEA(string);
						}
					}
					break;
				case JAYPEG_EXIF_VALUE_ASCII:
					if (count > 4)
						voffset = readExif32(exifData+offset+8, bigEndian);
					else
						voffset = offset+8;
					// Detect overflow
					if (voffset+count < voffset || voffset+count < count)
						return JAYPEG_ERR;
					if (voffset+count <= exifLen)
					{
						if (exifData[voffset+count-1]==0)
							image->exifDataFound(tag, (const char*)exifData+voffset);
						else
						{
							// Add null termination if it does not exist already, fixes some broken exif data
							char* tmpstr = OP_NEWA(char, count+1);
							if (tmpstr)
							{
								op_memcpy(tmpstr, exifData+voffset, count);
								tmpstr[count] = 0;
								image->exifDataFound(tag, tmpstr);
								OP_DELETEA(tmpstr);
							}
						}
					}
					break;
				case JAYPEG_EXIF_VALUE_SHORT:
					if (count > 2)
						voffset = readExif32(exifData+offset+8, bigEndian);
					else
						voffset = offset+8;
					// Detect overflow
					if (count&(1u<<31))
						return JAYPEG_ERR;
					countoffs = count*2;
					// Detect overflow
					if (voffset+countoffs < voffset || voffset+countoffs < countoffs)
						return JAYPEG_ERR;
					if (voffset+countoffs <= exifLen)
					{
						// parse the values to a string..
						// determine the length of the string
						int stringlen = 1;
						for (i = 0; i < count; ++i)
						{
							stringlen += op_snprintf(tmpstr, 16, "%u", readExif16(exifData+voffset+i*2, bigEndian));
							if (i > 0)
								stringlen += 2;
						}
						char* string = OP_NEWA(char, stringlen+1);
						if (string)
						{
							*string = 0;
							int curlen = 0;
							for (i = 0; i < count; ++i)
							{
								curlen += op_snprintf(string+curlen, stringlen+1-curlen, "%s%u", i>0?", ":"", readExif16(exifData+voffset+i*2, bigEndian));
							}
							image->exifDataFound(tag, string);
							OP_DELETEA(string);
						}
					}
					break;
				case JAYPEG_EXIF_VALUE_LONG:
					if (count > 1)
						voffset = readExif32(exifData+offset+8, bigEndian);
					else
						voffset = offset+8;
					// Detect overflow
					if (count&(3u<<30))
						return JAYPEG_ERR;
					countoffs = count*4;
					// Detect overflow
					if (voffset+countoffs < voffset || voffset+countoffs < countoffs)
						return JAYPEG_ERR;
					if (voffset+countoffs <= exifLen)
					{
						// parse the values to a string..
						// determine the length of the string
						int stringlen = 1;
						for (i = 0; i < count; ++i)
						{
							stringlen += op_snprintf(tmpstr, 16, "%u", readExif32(exifData+voffset+i*4, bigEndian));
							if (i > 0)
								stringlen += 2;
						}
						char* string = OP_NEWA(char, stringlen+1);
						if (string)
						{
							*string = 0;
							int curlen = 0;
							for (i = 0; i < count; ++i)
							{
								curlen += op_snprintf(string+curlen, stringlen+1-curlen, "%s%u", i>0?", ":"", readExif32(exifData+voffset+i*4, bigEndian));
							}
							image->exifDataFound(tag, string);
							OP_DELETEA(string);
						}
					}
					break;
				case JAYPEG_EXIF_VALUE_RATIONAL:
					voffset = readExif32(exifData+offset+8, bigEndian);
					// Detect overflow
					if (count&(7u<<29))
						return JAYPEG_ERR;
					countoffs = count*8;
					// Detect overflow
					if (voffset+countoffs < voffset || voffset+countoffs < countoffs)
						return JAYPEG_ERR;
					if (voffset+countoffs <= exifLen)
					{
						// parse the values to a string..
						// determine the length of the string
						int stringlen = 1;
						for (i = 0; i < count; ++i)
						{
							if (readExif32(exifData+voffset+i*8+4, bigEndian) != 0)
							{
								stringlen += op_snprintf(tmpstr, 16, "%g", (float)readExif32(exifData+voffset+i*8, bigEndian) / (float)readExif32(exifData+voffset+i*8+4, bigEndian));
								if (i > 0)
									stringlen += 2;
							}
						}
						char* string = OP_NEWA(char, stringlen+1);
						if (string)
						{
							*string = 0;
							int curlen = 0;
							for (i = 0; i < count; ++i)
							{
								if (readExif32(exifData+voffset+i*8+4, bigEndian) != 0)
									curlen += op_snprintf(string+curlen, stringlen+1-curlen, "%s%g", i>0?", ":"", (float)readExif32(exifData+voffset+i*8, bigEndian) / (float)readExif32(exifData+voffset+i*8+4, bigEndian));
							}
							image->exifDataFound(tag, string);
							OP_DELETEA(string);
						}
					}
					break;
				case JAYPEG_EXIF_VALUE_SLONG:
					if (count > 1)
						voffset = readExif32(exifData+offset+8, bigEndian);
					else
						voffset = offset+8;
					// Detect overflow
					if (count&(3u<<30))
						return JAYPEG_ERR;
					countoffs = count*4;
					// Detect overflow
					if (voffset+countoffs < voffset || voffset+countoffs < countoffs)
						return JAYPEG_ERR;
					if (voffset+countoffs <= exifLen)
					{
						// parse the values to a string..
						// determine the length of the string
						int stringlen = 1;
						for (i = 0; i < count; ++i)
						{
							stringlen += op_snprintf(tmpstr, 16, "%d", readExif32(exifData+voffset+i*4, bigEndian));
							if (i > 0)
								stringlen += 2;
						}
						char* string = OP_NEWA(char, stringlen+1);
						if (string)
						{
							*string = 0;
							int curlen = 0;
							for (i = 0; i < count; ++i)
							{
								curlen += op_snprintf(string+curlen, stringlen+1-curlen, "%s%d", i>0?", ":"", readExif32(exifData+voffset+i*4, bigEndian));
							}
							image->exifDataFound(tag, string);
							OP_DELETEA(string);
						}
					}
					break;
				case JAYPEG_EXIF_VALUE_SRATIONAL:
					voffset = readExif32(exifData+offset+8, bigEndian);
					// Detect overflow
					if (count&(7u<<29))
						return JAYPEG_ERR;
					countoffs = count*8;
					// Detect overflow
					if (voffset+countoffs < voffset || voffset+countoffs < countoffs)
						return JAYPEG_ERR;
					if (voffset+countoffs <= exifLen)
					{
						// parse the values to a string..
						// determine the length of the string
						int stringlen = 1;
						for (i = 0; i < count; ++i)
						{
							if (readExif32(exifData+voffset+i*8+4, bigEndian) != 0)
							{
								stringlen += op_snprintf(tmpstr, 16, "%g", (float)((int)readExif32(exifData+voffset+i*8, bigEndian)) / (float)((int)readExif32(exifData+voffset+i*8+4, bigEndian)));
								if (i > 0)
									stringlen += 2;
							}
						}
						char* string = OP_NEWA(char, stringlen+1);
						if (string)
						{
							*string = 0;
							int curlen = 0;
							for (i = 0; i < count; ++i)
							{
								if (readExif32(exifData+voffset+i*8+4, bigEndian) != 0)
									curlen += op_snprintf(string+curlen, stringlen+1-curlen, "%s%g", i>0?", ":"", (float)((int)readExif32(exifData+voffset+i*8, bigEndian)) / (float)((int)readExif32(exifData+voffset+i*8+4, bigEndian)));
							}
							image->exifDataFound(tag, string);
							OP_DELETEA(string);
						}
					}
					break;
				case JAYPEG_EXIF_VALUE_UNDEFINED:
				default:
					// This can be unicode strings or just about anything else
					break;
				}
				break;
			}
			// There is atleast one more value in the header
			offset += 12;
			--numFields;
		}
		if (exifIFD > 0 && exifIFD+2<=exifLen)
		{
			offset = exifIFD;
			exifIFD = 0;
			curIFD = 1;
			numFields = readExif16(exifData+offset, bigEndian);
			offset += 2;
			// Detect overflows
			if (offset < 2)
				return JAYPEG_ERR;
		}
		else if (gpsIFD > 0 && gpsIFD+2<=exifLen)
		{
			offset = gpsIFD;
			gpsIFD = 0;
			curIFD = 2;
			numFields = readExif16(exifData+offset, bigEndian);
			offset += 2;
			// Detect overflows
			if (offset < 2)
				return JAYPEG_ERR;
		}
		else if (interopIFD > 0 && interopIFD+2<=exifLen)
		{
			offset = interopIFD;
			interopIFD = 0;
			curIFD = 3;
			numFields = readExif16(exifData+offset, bigEndian);
			offset += 2;
			// Detect overflows
			if (offset < 2)
				return JAYPEG_ERR;
		}
		else
		{
			offset = 0;
		}
	}

	return JAYPEG_OK;
}
#endif // JAYPEG_EXIF_SUPPORT


int JayJFIFDecoder::decodeMCU(JayStream *stream)
{
	short samples[64];
	unsigned char *decsamples;
	while (1)
	{
		if (!image->wantMoreData())
		{
			return JAYPEG_NOT_ENOUGH_DATA;
		}
		int decresult;
		int cc = scanComponents[curComponent];
		if (progressive || !interlaced)
		{
			short *samp;
			if (numScanComponents > 1)
			{
				int block = compDataUnitCount[cc]/(compHorizRes[cc]*compVertRes[cc]);
				int blockoffs = compDataUnitCount[cc]%(compHorizRes[cc]*compVertRes[cc]);
				OP_ASSERT(mcuRowNum*compVertRes[cc]+blockoffs/compHorizRes[cc] < ((samples_height/8)*compVertRes[cc])/maxVertRes);
				samp = progressionSamples[cc][mcuRowNum*compVertRes[cc]+blockoffs/compHorizRes[cc]];
				OP_ASSERT((block*compHorizRes[cc]+blockoffs%compHorizRes[cc]) < ((samples_width/8)*compHorizRes[cc])/maxHorizRes);
				samp += (block*compHorizRes[cc]+blockoffs%compHorizRes[cc])*64;
			}
			else // not interleaved
			{
				OP_ASSERT(mcuRowNum < ((samples_height/8)*compVertRes[cc])/maxVertRes);
				samp = progressionSamples[cc][mcuRowNum];
				OP_ASSERT(compDataUnitCount[cc] < ((samples_width/8)*compHorizRes[cc])/maxHorizRes);
				samp += compDataUnitCount[cc]*64;
			}
			if (firstProgSample != 0)
				huffEntropyDecoder.noDCSamples(TRUE);
			if (approxHigh)
			{
				OP_ASSERT(approxHigh == approxLow+1);
				int i;
				huffEntropyDecoder.enableRefinement(approxLow);
				for (i = 0; i < numProgSamples; ++i)
				{
					// store all samples in case the huffman fails
					samples[i+firstProgSample] = samp[i+firstProgSample];
				}
				decresult = entropyDecoder->readSamples(stream, cc, numProgSamples, samp+firstProgSample);
				huffEntropyDecoder.enableRefinement(-1);
				huffEntropyDecoder.noDCSamples(FALSE);
				if (decresult != JAYPEG_OK)
				{
					// huffman has failed, restore previous state
					for (i = 0; i < numProgSamples; ++i)
					{
						samp[i+firstProgSample] = samples[i+firstProgSample];
					}
					return decresult;
				}
			}
			else
			{
				int i;
				for (i = 0; i < numProgSamples; ++i)
					samp[firstProgSample+i] = 0;
				decresult = entropyDecoder->readSamples(stream, cc, numProgSamples, samp+firstProgSample);
				huffEntropyDecoder.noDCSamples(FALSE);
				if (decresult != JAYPEG_OK)
					return decresult;
				if (approxLow)
				{
					for (i = 0; i < numProgSamples; ++i)
					{
						samp[firstProgSample+i] <<= approxLow;
					}
				}
			}
		}
		else
		{
			int i;
			for (i = 0; i < 64; ++i)
				samples[i] = 0;
			decresult = entropyDecoder->readSamples(stream, cc, 64, samples);
			if (decresult != JAYPEG_OK)
				return decresult;
			// do the IDCT
			if (numScanComponents > 1)
			{
				int block = compDataUnitCount[cc]/(compHorizRes[cc]*compVertRes[cc]);
				int blockoffs = compDataUnitCount[cc]%(compHorizRes[cc]*compVertRes[cc]);
				i = compHorizRes[cc]*block + blockoffs%compHorizRes[cc];
				if (i < compWidthInBlocks[cc])
				{
					i += (blockoffs/compHorizRes[cc])*compWidthInBlocks[cc]*8;
					decsamples = mcuRow[cc]+i*8;
					trans.transform(cc, 64, compWidthInBlocks[cc]*8, samples, decsamples);
				}
			}
			else
			{
				decsamples = mcuRow[cc]+compDataUnitCount[cc]*8;
				trans.transform(cc, 64, compWidthInBlocks[cc]*8, samples, decsamples);
			}
		}
		++compDataUnitCount[cc];
		--dataUnitsPerMCURow;
		// one MCU row is completed
		if (dataUnitsPerMCURow == 0)
		{
			if (!progressive && interlaced)
			{
				writeMCURow(mcuRowNum);
			}
			++mcuRowNum;
			if ((numScanComponents > 1 && mcuRowNum >= (samples_height/(8*maxVertRes))) ||
				(numScanComponents == 1 && mcuRowNum >= ((compVertRes[cc]*height+maxVertRes-1)/maxVertRes+7)/8))
			{
				lastStartedMCURow = 0;
				if (lastStartedMCURow == lastWrittenMCURow || lastWrittenMCURow < 0)
					wrappedMCU = TRUE;
				state = JAYPEG_JFIF_STATE_FRAME;
				return JAYPEG_OK;
			}
			startMCURow();
		}

		++curComponentCount;
		if (curComponentCount >= compHorizRes[cc]*compVertRes[cc] || numScanComponents == 1)
		{
			// One component is completed, so go to the next one
			curComponentCount = 0;
			++curComponent;
			if (curComponent >= numScanComponents)
			{
				curComponent = 0;
				// now exactly one mcu has been decoded
				++restartCount;
			}
		}

		if (restartCount == restartInterval && restartInterval > 0)
		{
			restartCount = 0;
			entropyDecoder->reset();
			state = JAYPEG_JFIF_STATE_FIND_MARKER;
			return JAYPEG_OK;
		}
	}
	return JAYPEG_OK;
}


int JayJFIFDecoder::decode(JayStream *stream)
{
	int decresult = JAYPEG_OK;
	int cc;

	while (decresult == JAYPEG_OK)
	{
		if (skipCount)
		{
			if (skipCount >= stream->getLength())
			{
				skipCount -= stream->getLength();
				stream->advanceBuffer(stream->getLength());
				return JAYPEG_OK;
			}
			stream->advanceBuffer(skipCount);
			skipCount = 0;
		}

		switch (state)
		{
		case JAYPEG_JFIF_STATE_STARTED:
		case JAYPEG_JFIF_STATE_FRAME:
			if (stream->getLength() < 2)
				return JAYPEG_NOT_ENOUGH_DATA;
			while (stream->getLength() >=2 && decresult == JAYPEG_OK && skipCount == 0 &&
				(state == JAYPEG_JFIF_STATE_STARTED || state == JAYPEG_JFIF_STATE_FRAME))
			{
				if (stream->getBuffer()[0] == 0xff)
				{
					// if frame marker, goto frame decoding
					// else parse the marker
					switch (stream->getBuffer()[1])
					{
					case JAYPEG_JFIF_MARKER_SOF3:
						JAYPEG_PRINTF(("lossless jpg\n"));
						return JAYPEG_ERR;
					case JAYPEG_JFIF_MARKER_SOF9:
						JAYPEG_PRINTF(("arith jpg\n"));
						return JAYPEG_ERR;
					case JAYPEG_JFIF_MARKER_SOFa:
						JAYPEG_PRINTF(("Progressive arith jpg\n"));
						return JAYPEG_ERR;
					case JAYPEG_JFIF_MARKER_SOFb:
						JAYPEG_PRINTF(("lossless arith jpg\n"));
						return JAYPEG_ERR;
					case JAYPEG_JFIF_MARKER_SOF5:
					case JAYPEG_JFIF_MARKER_SOF6:
					case JAYPEG_JFIF_MARKER_SOF7:
					case JAYPEG_JFIF_MARKER_JPG:
					case JAYPEG_JFIF_MARKER_SOFd:
					case JAYPEG_JFIF_MARKER_SOFe:
					case JAYPEG_JFIF_MARKER_SOFf:
						JAYPEG_PRINTF(("Hierarchical jpeg\n"));
						return JAYPEG_ERR;
					case JAYPEG_JFIF_MARKER_SOF2:
					case JAYPEG_JFIF_MARKER_SOF0:
					case JAYPEG_JFIF_MARKER_SOF1:
						if (state != JAYPEG_JFIF_STATE_STARTED)
						{
							return JAYPEG_ERR;
						}
						if (stream->getLength() < 4)
						{
							return JAYPEG_NOT_ENOUGH_DATA;
						}
						progressive = (stream->getBuffer()[1] == JAYPEG_JFIF_MARKER_SOF2);
						interlaced = TRUE;
						lastStartedMCURow = 0;
						lastWrittenMCURow = -1;
						wrappedMCU = FALSE;
						// delete the old sample data, if any
						for (cc = 0; cc < numComponents; ++cc)
						{
							OP_DELETEA(mcuRow[cc]);
							mcuRow[cc] = NULL;
							if (progressionSamples[cc])
							{
								int vb = ((samples_height/8)*compVertRes[cc])/maxVertRes;

								for (int i = 0; i < vb; ++i)
								{
									OP_DELETEA(progressionSamples[cc][i]);
								}
								OP_DELETEA(progressionSamples[cc]);
								progressionSamples[cc] = NULL;
							}
						}

						JAYPEG_PRINTF(("Start of frame\n"));
						markerlength = (stream->getBuffer()[2]<<8) | stream->getBuffer()[3];
						if (markerlength < 8)
						{
							return JAYPEG_ERR;
						}
						if (stream->getLength() < 10)
						{
							return JAYPEG_NOT_ENOUGH_DATA;
						}

						stream->advanceBuffer(4);
						markerlength -= 2;
						sampleSize = stream->getBuffer()[0];
						JAYPEG_PRINTF(("Precision: %d\n", sampleSize));
						if (sampleSize != 8 /*&& sampleSize != 12*/)
						{
							// FIXME: add 12bit support
							OP_ASSERT(!"Unsupported sample size");
							return JAYPEG_ERR;
						}
						width = (stream->getBuffer()[3]<<8)|stream->getBuffer()[4];
						height = (stream->getBuffer()[1]<<8)|stream->getBuffer()[2];
						numComponents = stream->getBuffer()[5];
						if (numComponents != 4 && numComponents != 3 && numComponents != 1)
						{
							if (numComponents > JAYPEG_MAX_COMPONENTS)
								numComponents = JAYPEG_MAX_COMPONENTS;
							return JAYPEG_ERR;
						}
						if (width <= 0 || height <= 0 || numComponents <= 0)
							return JAYPEG_ERR;
						// Abort decoding if the image could not be initialized
						decresult = image->init(width, height, numComponents==1?1:3, progressive);
						if (decresult != JAYPEG_OK)
							return decresult;
						JAYPEG_PRINTF(("size: %d x %d\n", width,height));
						JAYPEG_PRINTF(("Components: %d\n", numComponents));
						decresult = huffEntropyDecoder.init();
						entropyDecoder = &huffEntropyDecoder;
						stream->advanceBuffer(6);
						markerlength -= 6;

						// set the type to continue the scanning
						if (progressive)
						{
							huffEntropyDecoder.enableEOB(TRUE);
						}
						else
						{
							huffEntropyDecoder.enableEOB(FALSE);
						}
						state = JAYPEG_JFIF_STATE_FRAMEHEADER;
						break;
					case JAYPEG_JFIF_MARKER_SOS:
					{
						if (state != JAYPEG_JFIF_STATE_FRAME)
						{
							return JAYPEG_ERR;
						}
						if (stream->getLength() < 4)
						{
							return JAYPEG_NOT_ENOUGH_DATA;
						}
						JAYPEG_PRINTF(("Start of scan\n"));
						markerlength = (stream->getBuffer()[2]<<8) | stream->getBuffer()[3];
						if (stream->getLength() < markerlength+2)
						{
							return JAYPEG_NOT_ENOUGH_DATA;
						}
						numScanComponents = stream->getBuffer()[4];
						JAYPEG_PRINTF(("  image components: %d\n", stream->getBuffer()[4]));
						if (numScanComponents > numComponents)
						{
							return JAYPEG_ERR;
						}
						stream->advanceBuffer(5);
						markerlength -= 3;
						// FIXME: currently only interleaved images are supported for non progressive images
						if (numScanComponents != numComponents && !progressive)
						{
							if (interlaced)
							{
								for (cc = 0; cc < numComponents; ++cc)
								{
									// number of blocks, multiplied with resolution of the block
									int hb = ((samples_width/8)*compHorizRes[cc])/maxHorizRes;
									int vb = ((samples_height/8)*compVertRes[cc])/maxVertRes;
									progressionSamples[cc] = OP_NEWA(short*, vb);
									if (!progressionSamples[cc])
										return JAYPEG_ERR_NO_MEMORY;
									op_memset(progressionSamples[cc], 0, sizeof(short*)*vb);
									for (int i = 0; i < vb; ++i)
									{
										progressionSamples[cc][i] = OP_NEWA(short, 64*hb);
										if (!progressionSamples[cc][i])
											return JAYPEG_ERR_NO_MEMORY;
										op_memset(progressionSamples[cc][i], 0, sizeof(short)*64*hb);
									}
								}
								interlaced = FALSE;
							}
						}
						if (numScanComponents < 1)
							return JAYPEG_ERR;
						for (int i = 0; i < numScanComponents; ++i)
						{
							JAYPEG_PRINTF(("    component: %d\n", stream->getBuffer()[0]));
							JAYPEG_PRINTF(("    comp DC entropy table: %d\n", stream->getBuffer()[1]>>4));
							JAYPEG_PRINTF(("    comp AC entropy table: %d\n", stream->getBuffer()[1]&0xf));
							int cnum = stream->getBuffer()[0];
							scanComponents[i] = i;
							int j;
							for (j = 0; j < numComponents; ++j)
							{
								if (cnum == componentNum[j])
								{
									scanComponents[i] = j;
									j = numComponents;
								}

							}
							for (j = 0; j < i; ++j)
							{
								if (scanComponents[i] == scanComponents[j])
									return JAYPEG_ERR;
							}
							if ((stream->getBuffer()[1]>>4) >= 4 || (stream->getBuffer()[1]&0xf) >= 4)
								return JAYPEG_ERR;
							huffEntropyDecoder.setComponentTableNumber(scanComponents[i], stream->getBuffer()[1]>>4, stream->getBuffer()[1]&0xf);
							stream->advanceBuffer(2);
							markerlength -= 2;
						}
						firstProgSample = stream->getBuffer()[0];
						numProgSamples = 1+stream->getBuffer()[1]-stream->getBuffer()[0];
						if ((progressive || !interlaced) && firstProgSample+numProgSamples > 64)
						{
							return JAYPEG_ERR;
						}
						approxLow = stream->getBuffer()[2]&0xf;
						approxHigh = stream->getBuffer()[2]>>4;
						JAYPEG_PRINTF(("  spectral start: %d\n", firstProgSample));
						JAYPEG_PRINTF(("  spectral end: %d\n", firstProgSample+numProgSamples-1));
						JAYPEG_PRINTF(("  approx high: %d\n", approxHigh));
						JAYPEG_PRINTF(("  approx low: %d\n", approxLow));

						entropyDecoder->reset();

						curComponentCount = 0;
						curComponent = 0;
						restartCount = 0;

						mcuRowNum = 0;
						startMCURow();

						// skip the rest
						stream->advanceBuffer(markerlength);
						// Don't go to restart state if we are not going to decode anything (we will skip to next marker instead)
						if (decode_data)
							state = JAYPEG_JFIF_STATE_RESTART;
						break;
					}
					case JAYPEG_JFIF_MARKER_DHT:
						JAYPEG_PRINTF(("parse huffman\n"));
						decresult = huffEntropyDecoder.readDHT(stream);
						break;
					//  DAC (arithmetic coding)
					//  DQT
					case JAYPEG_JFIF_MARKER_DQT:
						JAYPEG_PRINTF(("parse quant\n"));
						decresult = trans.readDQT(stream);
						break;
					case JAYPEG_JFIF_MARKER_DRI:
						if (stream->getLength() < 4)
						{
							return JAYPEG_NOT_ENOUGH_DATA;
						}
						skipCount = (stream->getBuffer()[2]<<8) | stream->getBuffer()[3];
						if (skipCount < 4)
						{
							return JAYPEG_ERR;
						}
						if (stream->getLength() < 6)
						{
							skipCount = 0;
							return JAYPEG_NOT_ENOUGH_DATA;
						}
						restartInterval = (stream->getBuffer()[4]<<8) | stream->getBuffer()[5];
						restartCount = 0;
						skipCount -= 4;
						stream->advanceBuffer(6);
						break;
					//  APPn
					case JAYPEG_JFIF_MARKER_APP0:
						// APP0 marker is used to identify jfif files
						if (stream->getLength() < 4)
						{
							return JAYPEG_NOT_ENOUGH_DATA;
						}
						skipCount = (stream->getBuffer()[2]<<8) | stream->getBuffer()[3];

						if (skipCount >= 14)
						{
							if (stream->getLength() < 9)
							{
								skipCount = 0;
								return JAYPEG_NOT_ENOUGH_DATA;
							}
							if (stream->getBuffer()[4] == 'J' &&
								stream->getBuffer()[5] == 'F' &&
								stream->getBuffer()[6] == 'I' &&
								stream->getBuffer()[7] == 'F' &&
								stream->getBuffer()[8] == 0)
							{
								if (stream->getLength() < 16)
								{
									// the header must be parsed as one
									skipCount = 0;
									return JAYPEG_NOT_ENOUGH_DATA;
								}
								jfifHeader = TRUE;
								JAYPEG_PRINTF(("Version: %d.%02d\n", stream->getBuffer()[9], stream->getBuffer()[10]));
								if (stream->getBuffer()[11] == 1)
								{
									JAYPEG_PRINTF(("dots per inch\n"));
								}
								else if (stream->getBuffer()[11] == 2)
								{
									JAYPEG_PRINTF(("dots per centimeter\n"));
								}
								else
								{
									JAYPEG_PRINTF(("pixels\n"));
								}
								JAYPEG_PRINTF(("Density: %d x %d\n", (stream->getBuffer()[12]<<8)|stream->getBuffer()[13],
									(stream->getBuffer()[14]<<8)|stream->getBuffer()[15]));
								// Next is thumbnail, but that can be skipped
								stream->advanceBuffer(12);
								skipCount -= 12;
							}
						}

						stream->advanceBuffer(4);
						skipCount -= 2;
						break;
#ifdef JAYPEG_EXIF_SUPPORT
					case JAYPEG_JFIF_MARKER_APP1:
						// APP1 marker is used to identify the exif header
						if (stream->getLength() < 4)
						{
							return JAYPEG_NOT_ENOUGH_DATA;
						}
						skipCount = (stream->getBuffer()[2]<<8) | stream->getBuffer()[3];
						if (decode_data && skipCount >= 14)
						{
							if (stream->getLength() < 10)
							{
								skipCount = 0;
								return JAYPEG_NOT_ENOUGH_DATA;
							}
							if (stream->getBuffer()[4] == 'E' &&
								stream->getBuffer()[5] == 'x' &&
								stream->getBuffer()[6] == 'i' &&
								stream->getBuffer()[7] == 'f' &&
								stream->getBuffer()[8] == 0 &&
								stream->getBuffer()[9] == 0)
							{
								if (stream->getLength() < skipCount-2)
								{
									// the header must be parsed as one
									skipCount = 0;
									return JAYPEG_NOT_ENOUGH_DATA;
								}
								OpStatus::Ignore(decodeExif(stream->getBuffer()+10, skipCount-8));
							}
						}
						stream->advanceBuffer(4);
						skipCount -= 2;
						break;
#endif // JAYPEG_EXIF_SUPPORT
#ifdef EMBEDDED_ICC_SUPPORT
					case JAYPEG_JFIF_MARKER_APP2:
						// APP2 segment can contain embedded ICC profiles
						if (stream->getLength() < 4)
						{
							return JAYPEG_NOT_ENOUGH_DATA;
						}
						skipCount = (stream->getBuffer()[2]<<8) | stream->getBuffer()[3];
						if (decode_data && !skipAPP2 && skipCount >= 16)
						{
							if (stream->getLength() < 18)
							{
								skipCount = 0;
								return JAYPEG_NOT_ENOUGH_DATA;
							}
							const UINT8* d = stream->getBuffer();
							if (op_memcmp((const char*)d+4, "ICC_PROFILE\0", 12) == 0)
							{
								// Don't really need to gulp all the data at once.
								if (stream->getLength() < skipCount+2)
								{
									skipCount = 0;
									return JAYPEG_NOT_ENOUGH_DATA;
								}

								// First and second byte indicates the current and total chunks
								UINT8 chunk_seq = d[4+12];
								UINT8 total_chunks = d[4+12+1];

								BOOL destroy_data = TRUE;

								if (chunk_seq == lastIccChunk+1 &&
									(lastIccChunk == 0 || total_chunks == totalIccChunks))
								{
									lastIccChunk = chunk_seq;
									totalIccChunks = total_chunks;

									// Ignore the 2 bytes chunk indicator
									OP_STATUS status = iccData.Append(d+18, skipCount-16);
									if (OpStatus::IsSuccess(status))
									{
										if (chunk_seq == total_chunks)
										{
											image->iccDataFound(iccData.Data(), iccData.Length());
										}
									}

									destroy_data =
										OpStatus::IsError(status) ||
										chunk_seq == total_chunks;
								}

								if (destroy_data)
								{
									iccData.Destroy();

									skipAPP2 = TRUE;
								}
							}
						}
						stream->advanceBuffer(4);
						skipCount -= 2;
						break;
#endif // EMBEDDED_ICC_SUPPORT
					case JAYPEG_JFIF_MARKER_APPe:
						// APP14 marker is used to identify adobe files
						if (stream->getLength() < 4)
						{
							return JAYPEG_NOT_ENOUGH_DATA;
						}
						skipCount = (stream->getBuffer()[2]<<8) | stream->getBuffer()[3];

						if (skipCount >= 14)
						{
							if (stream->getLength() < 10)
							{
								skipCount = 0;
								return JAYPEG_NOT_ENOUGH_DATA;
							}
							if (stream->getBuffer()[4] == 'A' &&
								stream->getBuffer()[5] == 'd' &&
								stream->getBuffer()[6] == 'o' &&
								stream->getBuffer()[7] == 'b' &&
								stream->getBuffer()[8] == 'e' &&
								stream->getBuffer()[9] == 0)
							{
								if (stream->getLength() < 16)
								{
									// the header must be parsed as one
									skipCount = 0;
									return JAYPEG_NOT_ENOUGH_DATA;
								}
								adobeHeader = TRUE;
								JAYPEG_PRINTF(("Version: %d\n", stream->getBuffer()[10]));
								JAYPEG_PRINTF(("Flags 0: %x\n", (stream->getBuffer()[11]<<8) | stream->getBuffer()[12]));
								JAYPEG_PRINTF(("Flags 1: %x\n", (stream->getBuffer()[13]<<8) | stream->getBuffer()[14]));
								JAYPEG_PRINTF(("Transform: %d\n", stream->getBuffer()[15]));
								adobeTransform = stream->getBuffer()[15];
								stream->advanceBuffer(12);
								skipCount -= 12;
							}
						}

						stream->advanceBuffer(4);
						skipCount -= 2;
						break;
					case JAYPEG_JFIF_MARKER_EOI:
						// This is just to make sure the image is flushed
						if (!interlaced)
							progressive = TRUE;
						flushProgressive();
						stream->advanceBuffer(2);
						state = JAYPEG_JFIF_STATE_DONE;
						break;
					case JAYPEG_JFIF_MARKER_RST0:
					case JAYPEG_JFIF_MARKER_RST1:
					case JAYPEG_JFIF_MARKER_RST2:
					case JAYPEG_JFIF_MARKER_RST3:
					case JAYPEG_JFIF_MARKER_RST4:
					case JAYPEG_JFIF_MARKER_RST5:
					case JAYPEG_JFIF_MARKER_RST6:
					case JAYPEG_JFIF_MARKER_RST7:
						stream->advanceBuffer(2);
						break;
					case 0:
					case 0xff:
						stream->advanceBuffer(1);
						break;
					//  COM
					// other app markers
					default:
						// skip the marker
						if (stream->getLength() < 4)
						{
							return JAYPEG_NOT_ENOUGH_DATA;
						}
						skipCount = (stream->getBuffer()[2]<<8) | stream->getBuffer()[3];
						JAYPEG_PRINTF(("Invalid marker: %x (%d bytes)\n", stream->getBuffer()[1], skipCount));
						stream->advanceBuffer(4);
						skipCount -= 2;
						break;
					}
				}
				else
				{
					stream->advanceBuffer(1);
				}
			}
			break;
		case JAYPEG_JFIF_STATE_FRAMEHEADER:
		{
			if (markerlength < (unsigned int)numComponents*3)
			{
				return JAYPEG_ERR;
			}
			if (stream->getLength() < (unsigned int)numComponents*3)
			{
				return JAYPEG_NOT_ENOUGH_DATA;
			}
			for (int component = 0; component < numComponents; ++component)
			{
				JAYPEG_PRINTF(("  comp: %d\n", stream->getBuffer()[0]));
				JAYPEG_PRINTF(("  hori: %d\n", stream->getBuffer()[1]>>4));
				JAYPEG_PRINTF(("  vert: %d\n", stream->getBuffer()[1]&0xf));
				JAYPEG_PRINTF(("  qtab: %d\n", stream->getBuffer()[2]));

				componentNum[component] = stream->getBuffer()[0];
				int vres = stream->getBuffer()[1]&0xf;
				int hres = stream->getBuffer()[1]>>4;
				if (vres == 0 || hres == 0)
				{
					return JAYPEG_ERR;
				}
				// Force resolution to 1x1 if there is only one component
				if (numComponents == 1)
				{
					vres = 1;
					hres = 1;
				}
				if (vres > maxVertRes)
					maxVertRes = vres;
				if (hres > maxHorizRes)
					maxHorizRes = hres;
				compVertRes[component] = vres;
				compHorizRes[component] = hres;

				if (stream->getBuffer()[2] >= 4)
					return JAYPEG_ERR;
				trans.setComponentTableNum(component, stream->getBuffer()[2]);
				stream->advanceBuffer(3);
				markerlength -= 3;
			}
			if (markerlength)
			{
				// skip the rest of this marker, since it is broken
				skipCount = markerlength;
			}
			for (cc = 0; cc < numComponents; ++cc)
			{
				compWidthInBlocks[cc] = ((width*compHorizRes[cc]+maxHorizRes-1)/maxHorizRes+7)/8;
				mcuRow[cc] = OP_NEWA(unsigned char, 64*compWidthInBlocks[cc]*compVertRes[cc]);
				if (!mcuRow[cc])
					return JAYPEG_ERR_NO_MEMORY;
			}
			mcuRowNum = 0;
			if (numComponents > 1)
			{
				OP_DELETEA(scanline);
				scanline = OP_NEWA(unsigned char, numComponents*width);
				if (!scanline)
					return JAYPEG_ERR_NO_MEMORY;
#ifndef JAYPEG_LOW_QUALITY_SCALE
				OP_DELETEA(scaleCache);
				scaleCache = OP_NEWA(unsigned char, ((width+1)/2)*2);
				if (!scaleCache)
					return JAYPEG_ERR_NO_MEMORY;
#endif // !JAYPEG_LOW_QUALITY_SCALE
			}

			// the sampled width and height..
			samples_width = maxHorizRes*8*((width + 8*maxHorizRes - 1) / (maxHorizRes*8));
			samples_height = maxVertRes*8*((height + 8*maxVertRes - 1) / (maxVertRes*8));
			if (progressive)
			{
				for (cc = 0; cc < numComponents; ++cc)
				{
					// number of blocks, multiplied with resolution of the block
					int hb = ((samples_width/8)*compHorizRes[cc])/maxHorizRes;
					int vb = ((samples_height/8)*compVertRes[cc])/maxVertRes;
					progressionSamples[cc] = OP_NEWA(short*, vb);
					if (!progressionSamples[cc])
						return JAYPEG_ERR_NO_MEMORY;
					op_memset(progressionSamples[cc], 0, sizeof(short*)*vb);
					for (int i = 0; i < vb; ++i)
					{
						progressionSamples[cc][i] = OP_NEWA(short, 64*hb);
						if (!progressionSamples[cc][i])
							return JAYPEG_ERR_NO_MEMORY;
						op_memset(progressionSamples[cc][i], 0, sizeof(short)*64*hb);
					}
				}
			}

			if (numComponents == 1)
			{
				colorSpace = JAYPEG_COLORSPACE_GRAYSCALE;
			}
			else if (numComponents == 3)
			{
				if (jfifHeader)
				{
					colorSpace = JAYPEG_COLORSPACE_YCRCB;
				}
				else if (adobeHeader)
				{
					if (adobeTransform == 0)
						colorSpace = JAYPEG_COLORSPACE_RGB;
					else
						colorSpace = JAYPEG_COLORSPACE_YCRCB;
				}
				else
				{
					if (componentNum[0] == 82 && componentNum[1] == 71 && componentNum[2] == 66)
					{
						colorSpace = JAYPEG_COLORSPACE_RGB;
					}
					else
					{
						colorSpace = JAYPEG_COLORSPACE_YCRCB;
					}
				}
			}
			else if (numComponents == 4)
			{
				if (adobeHeader)
				{
					if (adobeTransform == 0)
						colorSpace = JAYPEG_COLORSPACE_CMYK;
					else
						colorSpace = JAYPEG_COLORSPACE_YCCK;
				}
				else
				{
					colorSpace = JAYPEG_COLORSPACE_CMYK;
				}
			}

			state = JAYPEG_JFIF_STATE_FRAME;
			break;
		}
		case JAYPEG_JFIF_STATE_FIND_MARKER:
			// This state is entered from the restart frame when a restart marker should be found
			while (state == JAYPEG_JFIF_STATE_FIND_MARKER)
			{
				if (stream->getLength() < 2)
					return JAYPEG_NOT_ENOUGH_DATA;
				if (stream->getBuffer()[0] == 0xff && stream->getBuffer()[1] != 0)
				{
					state = JAYPEG_JFIF_STATE_RESTART;
					if (stream->getBuffer()[1] >= JAYPEG_JFIF_MARKER_RST0 && stream->getBuffer()[1] <= JAYPEG_JFIF_MARKER_RST7)
					{
						stream->advanceBuffer(2);
					}
					else
					{
						// FIXME: not sure what should be done here, i need an example which triggers this //timj
						OP_ASSERT(FALSE);
						state = JAYPEG_JFIF_STATE_FRAME;
					}
				}
				else
				{
					stream->advanceBuffer(1);
				}
			}
			break;
		case JAYPEG_JFIF_STATE_RESTART:
			decresult = decodeMCU(stream);
			if (decresult != JAYPEG_OK)
				return decresult;
			break;
		case JAYPEG_JFIF_STATE_DONE:
			// The image is already decoded..
			stream->advanceBuffer(stream->getLength());
			return JAYPEG_OK;
		case JAYPEG_JFIF_STATE_NONE:
			// Decoding is not started..
			if (stream->getLength() < 2)
				return JAYPEG_NOT_ENOUGH_DATA;
			if (stream->getBuffer()[0] != 0xff || stream->getBuffer()[1] != JAYPEG_JFIF_MARKER_SOI)
			{
				return JAYPEG_ERR;
			}
			jfifHeader = FALSE;
			adobeHeader = FALSE;
			stream->advanceBuffer(2);
			state = JAYPEG_JFIF_STATE_STARTED;
			break;
		default:
			OP_ASSERT(FALSE);
			return JAYPEG_ERR;
		};
	}
	return decresult;
}

#endif
