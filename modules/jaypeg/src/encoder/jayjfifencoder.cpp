/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/jaypeg/jaypeg.h"
#if defined(JAYPEG_JFIF_SUPPORT) && defined(JAYPEG_ENCODE_SUPPORT)

#include "modules/jaypeg/src/encoder/jayjfifencoder.h"
#include "modules/jaypeg/src/jayjfifmarkers.h"
#include "modules/jaypeg/jayencodeddata.h"

#include "modules/jaypeg/src/jaycolorlt.h"

// "Standard" quant tables from the jpeg specification
static const unsigned char DefaultQuantTableLuminance[64] = {
	16, 11, 10, 16, 24, 40, 51, 61,
	12, 12, 14, 19, 26, 58, 60, 55,
	14, 13, 16, 24, 40, 57, 69, 56,
	14, 17, 22, 29, 51, 87, 80, 62,
	18, 22, 37, 56, 68, 109, 103, 77,
	24, 35, 55, 64, 81, 104, 113, 92,
	49, 64, 78, 87, 103, 121, 120, 101,
	72, 92, 95, 98, 112, 100, 103, 99
};
static const unsigned char DefaultQuantTableChrominance[64] = {
	17, 18, 24, 47, 99, 99, 99, 99,
	18, 21, 26, 66, 99, 99, 99, 99,
	24, 26, 56, 99, 99, 99, 99, 99,
	47, 66, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99
};

// "Standard" huffman tables from the jpeg specification
static const unsigned char HuffLuminanceDCLengths[16] =
{
	0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0
};
static const unsigned char HuffLuminanceDCValues[] =
{
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B
};
static const unsigned char HuffChrominanceDCLengths[16] =
{
	0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0
};
static const unsigned char HuffChrominanceDCValues[] =
{
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B
};
static const unsigned char HuffLuminanceACLengths[16] =
{
	0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125
};
static const unsigned char HuffLuminanceACValues[] =
{
	0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
	0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
	0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
	0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
	0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
	0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
	0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
	0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
	0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
	0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
	0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
	0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
	0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
	0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
	0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
	0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
	0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
	0xf9, 0xfa
};
static const unsigned char HuffChrominanceACLengths[16] =
{
	0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119
};
static const unsigned char HuffChrominanceACValues[] =
{
	0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
	0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
	0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
	0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
	0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
	0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
	0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
	0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
	0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
	0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
	0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
	0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
	0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
	0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
	0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
	0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
	0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
	0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
	0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
	0xf9, 0xfa
};

// This is a lookup table for the calculation (float)op_cos((2.f*x+1)*u*3.1415f/16.f) in fdctZigZagQuant()
static const float cos_zigzag_lookup[] = {
    1.000000f,  1.000000f,  1.000000f,  1.000000f,  1.000000f,  1.000000f,  1.000000f,  1.000000f, 
    0.980786f,  0.831479f,  0.555594f,  0.195130f, -0.195039f, -0.555517f, -0.831428f, -0.980768f, 
    0.923884f,  0.382716f, -0.382630f, -0.923849f, -0.923919f, -0.382801f,  0.382544f,  0.923813f, 
    0.831479f, -0.195039f, -0.980768f, -0.555672f,  0.555440f,  0.980823f,  0.195312f, -0.831325f, 
    0.707123f, -0.707058f, -0.707189f,  0.706992f,  0.707254f, -0.706927f, -0.707320f,  0.706861f, 
    0.555594f, -0.980768f,  0.194948f,  0.831582f, -0.831325f, -0.195403f,  0.980859f, -0.555209f, 
    0.382716f, -0.923919f,  0.923813f, -0.382458f, -0.382972f,  0.924026f, -0.923707f,  0.382201f, 
    0.195130f, -0.555672f,  0.831582f, -0.980841f,  0.980714f, -0.831222f,  0.555132f, -0.194495f, 
};

JayJFIFEncoder::JayJFIFEncoder() : output_stream(NULL), y_blocks(NULL), cr_blocks(NULL), cb_blocks(NULL), 
	bit_buffer(0), bit_buffer_len(0), quant_table_luminance(NULL), quant_table_chrominance(NULL)
{
	pred[0] = pred[1] = pred[2] = 0;
}

JayJFIFEncoder::~JayJFIFEncoder()
{
	OP_DELETEA(y_blocks);
	OP_DELETEA(cr_blocks);
	OP_DELETEA(cb_blocks);
	OP_DELETEA(quant_table_luminance);
	OP_DELETEA(quant_table_chrominance);
}

OP_STATUS JayJFIFEncoder::init(int quality, unsigned int width, unsigned int height, JayEncodedData *strm)
{
	output_stream = strm;
	image_width = width;
	image_height = height;
	encoded_scanlines = 0;

	if (image_width&0xffff0000 || image_height&0xffff0000)
		return OpStatus::ERR;

	block_width = (width+7)/8;
	if (block_width&1)
		++block_width;
	// Allocate the blocks needed for this image
	y_blocks = OP_NEWA(short, (block_width*8)*16);
	cr_blocks = OP_NEWA(short, (block_width*8/2)*8);
	cb_blocks = OP_NEWA(short, (block_width*8/2)*8);

	if (!y_blocks || !cr_blocks || !cb_blocks)
		return OpStatus::ERR_NO_MEMORY;

	// Set up the huffman and quant tables
	// Write all the headers to the stream
	UINT8 marker[2];
	marker[0] = 0xff;
	marker[1] = JAYPEG_JFIF_MARKER_SOI;
	RETURN_IF_ERROR(output_stream->dataReady(marker, 2));

	RETURN_IF_ERROR(writeJFIFHeader());
	RETURN_IF_ERROR(initAndWriteDQTHeader(quality));
	RETURN_IF_ERROR(initAndWriteDHTHeader());
	RETURN_IF_ERROR(writeSOFHeader());
	RETURN_IF_ERROR(writeSOSHeader());

	return OpStatus::OK;
}

OP_STATUS JayJFIFEncoder::writeJFIFHeader()
{
	UINT8 data[18];
	// jfif header
	data[0] = 0xff;
	data[1] = JAYPEG_JFIF_MARKER_APP0;

	// 16 (length - 2 bytes)
	data[2] = 0;
	data[3] = 16;

	// "JFIF\0" (id - 5 bytes)
	data[4] = 'J';
	data[5] = 'F';
	data[6] = 'I';
	data[7] = 'F';
	data[8] = 0;

	// 1 (version major - 1 byte)
	// 2 (version minor - 1 byte)
	data[9] = 1;
	data[10] = 2;

	// 0 (unit - 1 byte)
	data[11] = 0;

	// 1 (x aspect - 2 bytes)
	data[12] = 0;
	data[13] = 1;

	// 1 (y aspect - 2 bytes)
	data[14] = 0;
	data[15] = 1;

	// 0 (thumb width - 1 bytes)
	data[16] = 0;

	// 0 (thumb height - 1 bytes)
	data[17] = 0;

	return output_stream->dataReady(data, 18);
}

// Scale quantization value with quality and clamp to U8
static unsigned char JFIF_ScaledQ(unsigned def, unsigned quality)
{
	unsigned sq = (def * quality + 50) / 100;
	return (unsigned char)(sq > 255 ? 255 : sq);
}

OP_STATUS JayJFIFEncoder::initAndWriteDQTHeader(int quality)
{
	quant_table_luminance = OP_NEWA(unsigned char, 64);
	quant_table_chrominance = OP_NEWA(unsigned char, 64);

	if (!quant_table_luminance || !quant_table_chrominance)
		return OpStatus::ERR_NO_MEMORY;

	// Quality setting used in libjpeg
	// Don't use zero to avoid divide by zero
	if (quality <= 0)
		quality = 1;
	if (quality > 100)
		quality = 100;

	// Quality 50 is no scaling to the standard
	// Quality 100 gives 0, all entries will be 1 -> no quantification at all
	if (quality < 50)
		quality = 5000 / quality;
	else
		quality = 200 - quality*2;

	for (unsigned int i = 0; i < 64; ++i)
	{
		quant_table_luminance[i] = JFIF_ScaledQ(DefaultQuantTableLuminance[i], quality);
		quant_table_chrominance[i] = JFIF_ScaledQ(DefaultQuantTableChrominance[i], quality);
		// Avoid divide by zero
		if (!quant_table_luminance[i])
			quant_table_luminance[i] = 1;
		if (!quant_table_chrominance[i])
			quant_table_chrominance[i] = 1;
	}

	UINT8 data[4];
	// define all tables
	// quant table
	data[0] = 0xff;
	data[1] = JAYPEG_JFIF_MARKER_DQT;
	// 2+65*numtables (length - 2 bytes)
	data[2] = 0;
	data[3] = 2+65*2;
	RETURN_IF_ERROR(output_stream->dataReady(data, 4));

	// table num (quality<<4|tablenum - 1 byte)
	data[0] = 0;
	RETURN_IF_ERROR(output_stream->dataReady(data, 1));
	// table entry 0..63 (64 bytes)
	RETURN_IF_ERROR(output_stream->dataReady(quant_table_luminance, 64));
	// table num (quality<<4|tablenum - 1 byte)
	data[0] = 1;
	RETURN_IF_ERROR(output_stream->dataReady(data, 1));
	// table entry 0..63 (64 bytes)
	RETURN_IF_ERROR(output_stream->dataReady(quant_table_chrominance, 64));
	return OpStatus::OK;
}

OP_STATUS JayJFIFEncoder::initAndWriteDHTHeader()
{
	UINT8 data[4];
	// huff table
	data[0] = 0xff;
	data[1] = JAYPEG_JFIF_MARKER_DHT;
	// (length - 2 bytes)
	int len = 2+(17)*4;
	for (unsigned int i = 0; i < 16; ++i)
	{
		len += HuffLuminanceDCLengths[i];
		len += HuffChrominanceDCLengths[i];
		len += HuffLuminanceACLengths[i];
		len += HuffChrominanceACLengths[i];
	}
	data[2] = len>>8;
	data[3] = len&0xff;
	RETURN_IF_ERROR(output_stream->dataReady(data, 4));

	// Luminance DC huffman table
	// (class<<4 | number - 1 byte)
	data[0] = 0;
	RETURN_IF_ERROR(output_stream->dataReady(data, 1));

	// (number of entries of length 1..16 - 16 bytes)
	RETURN_IF_ERROR(output_stream->dataReady(HuffLuminanceDCLengths, 16));
	// value (num len 1+num len 2+... bytes)
	RETURN_IF_ERROR(output_stream->dataReady(HuffLuminanceDCValues, sizeof(HuffLuminanceDCValues)/sizeof(HuffLuminanceDCValues[0])));

	// Luminance AC huffman table
	// (class<<4 | number - 1 byte)
	data[0] = (1<<4) | 0;
	RETURN_IF_ERROR(output_stream->dataReady(data, 1));

	// (number of entries of length 1..16 - 16 bytes)
	RETURN_IF_ERROR(output_stream->dataReady(HuffLuminanceACLengths, 16));
	// value (num len 1+num len 2+... bytes)
	RETURN_IF_ERROR(output_stream->dataReady(HuffLuminanceACValues, sizeof(HuffLuminanceACValues)/sizeof(HuffLuminanceACValues[0])));

	// Chrominance DC huffman table
	// (class<<4 | number - 1 byte)
	data[0] = 1;
	RETURN_IF_ERROR(output_stream->dataReady(data, 1));

	// (number of entries of length 1..16 - 16 bytes)
	RETURN_IF_ERROR(output_stream->dataReady(HuffChrominanceDCLengths, 16));
	// value (num len 1+num len 2+... bytes)
	RETURN_IF_ERROR(output_stream->dataReady(HuffChrominanceDCValues, sizeof(HuffChrominanceDCValues)/sizeof(HuffChrominanceDCValues[0])));

	// Chrominance AC huffman table
	// (class<<4 | number - 1 byte)
	data[0] = (1<<4) | 1;
	RETURN_IF_ERROR(output_stream->dataReady(data, 1));

	// (number of entries of length 1..16 - 16 bytes)
	RETURN_IF_ERROR(output_stream->dataReady(HuffChrominanceACLengths, 16));
	// value (num len 1+num len 2+... bytes)
	RETURN_IF_ERROR(output_stream->dataReady(HuffChrominanceACValues, sizeof(HuffChrominanceACValues)/sizeof(HuffChrominanceACValues[0])));

	return OpStatus::OK;
}

OP_STATUS JayJFIFEncoder::writeSOFHeader()
{
	UINT8 data[19];

	// Always create a baseline jpeg
	data[0] = 0xff;
	data[1] = JAYPEG_JFIF_MARKER_SOF0;

	// 8+3*numComponents (size - 2 bytes)
	data[2] = 0;
	data[3] = 8+9;

	// 8 (sample size - 1 byte)
	data[4] = 8;

	// height (height - 2 bytes)
	data[5] = (image_height>>8)&0xff;
	data[6] = (image_height&0xff);

	// width (width - 2 bytes)
	data[7] = (image_width>>8)&0xff;
	data[8] = (image_width&0xff);

	// 3 (num components - 1 byte)
	data[9] = 3;

	// per component
	// n (id - 1 byte)
	data[10] = 1;
	// 2<<4|2 (horiz<<4|vert resolution - 1 byte)
	data[11] = (2<<4)|2;
	// qt (quant table num - 1 byte)
	data[12] = 0;

	// n (id - 1 byte)
	data[13] = 2;
	// 1<<4|1 (horiz<<4|vert resolution - 1 byte)
	data[14] = (1<<4)|1;
	// qt (quant table num - 1 byte)
	data[15] = 1;

	// n (id - 1 byte)
	data[16] = 3;
	// 1<<4|1 (horiz<<4|vert resolution - 1 byte)
	data[17] = (1<<4)|1;
	// qt (quant table num - 1 byte)
	data[18] = 1;
	return output_stream->dataReady(data, 19);
}

OP_STATUS JayJFIFEncoder::writeSOSHeader()
{
	UINT8 data[14];
	data[0] = 0xff;
	data[1] = JAYPEG_JFIF_MARKER_SOS;

	// 6+2*numComponents (size - 2 bytes)
	data[2] = 0;
	data[3] = 6+6;

	// 3 (num components - 1 byte)
	data[4] = 3;

	// n (id - 1 byte)
	data[5] = 1;
	// 0 (dchuff<<4|achuff - 1 byte)
	data[6] = 0;

	// n (id - 1 byte)
	data[7] = 2;
	// 0 (dchuff<<4|achuff - 1 byte)
	data[8] = (1<<4)|1;

	// n (id - 1 byte)
	data[9] = 3;
	// 0 (dchuff<<4|achuff - 1 byte)
	data[10] = (1<<4)|1;

	// 0 (first sample - 1 byte)
	// 63 (last sample - 1 byte)
	data[11] = 0;
	data[12] = 63;

	// 0 (high bit << 4 | low bit - 1 byte)
	data[13] = 0;

	return output_stream->dataReady(data, 14);
}

#define ZIGZAG_UNROLLED_LOOP(y, x) (((float)coef[stride*y+x]-128.f) * cos_zigzag_lookup[x+(u<<3)] * cos_zigzag_lookup[y+(v<<3)])


// FIXME: this is a very slow idct implementation
void JayJFIFEncoder::fdctZigZagQuant(short* coef, unsigned int stride, const unsigned char* quant)
{
	// Precomputed (float)op_sqrt(2.f)
	static const float sqrt_2f = 1.4142135f;
	short temp[64];

	for (unsigned int v = 0; v < 8; ++v)
	{
		for (unsigned int u = 0; u < 8; ++u)
		{
			float res = 0.f;

			res += (ZIGZAG_UNROLLED_LOOP(0, 0) + ZIGZAG_UNROLLED_LOOP(0, 1) + ZIGZAG_UNROLLED_LOOP(0, 2) + ZIGZAG_UNROLLED_LOOP(0, 3) + 
				ZIGZAG_UNROLLED_LOOP(0, 4) + ZIGZAG_UNROLLED_LOOP(0, 5) + ZIGZAG_UNROLLED_LOOP(0, 6) + ZIGZAG_UNROLLED_LOOP(0, 7));

			res += (ZIGZAG_UNROLLED_LOOP(1, 0) + ZIGZAG_UNROLLED_LOOP(1, 1) + ZIGZAG_UNROLLED_LOOP(1, 2) + ZIGZAG_UNROLLED_LOOP(1, 3) +
				ZIGZAG_UNROLLED_LOOP(1, 4) + ZIGZAG_UNROLLED_LOOP(1, 5) + ZIGZAG_UNROLLED_LOOP(1, 6) + ZIGZAG_UNROLLED_LOOP(1, 7));

			res += (ZIGZAG_UNROLLED_LOOP(2, 0) + ZIGZAG_UNROLLED_LOOP(2, 1) + ZIGZAG_UNROLLED_LOOP(2, 2) + ZIGZAG_UNROLLED_LOOP(2, 3) +
				ZIGZAG_UNROLLED_LOOP(2, 4) + ZIGZAG_UNROLLED_LOOP(2, 5) + ZIGZAG_UNROLLED_LOOP(2, 6) + ZIGZAG_UNROLLED_LOOP(2, 7));
			
			res += (ZIGZAG_UNROLLED_LOOP(3, 0) + ZIGZAG_UNROLLED_LOOP(3, 1) + ZIGZAG_UNROLLED_LOOP(3, 2) + ZIGZAG_UNROLLED_LOOP(3, 3) +
				ZIGZAG_UNROLLED_LOOP(3, 4) + ZIGZAG_UNROLLED_LOOP(3, 5) + ZIGZAG_UNROLLED_LOOP(3, 6) + ZIGZAG_UNROLLED_LOOP(3, 7));

			res += (ZIGZAG_UNROLLED_LOOP(4, 0) + ZIGZAG_UNROLLED_LOOP(4, 1) + ZIGZAG_UNROLLED_LOOP(4, 2) + ZIGZAG_UNROLLED_LOOP(4, 3) +
				ZIGZAG_UNROLLED_LOOP(4, 4) + ZIGZAG_UNROLLED_LOOP(4, 5) + ZIGZAG_UNROLLED_LOOP(4, 6) + ZIGZAG_UNROLLED_LOOP(4, 7));

			res += (ZIGZAG_UNROLLED_LOOP(5, 0) + ZIGZAG_UNROLLED_LOOP(5, 1) + ZIGZAG_UNROLLED_LOOP(5, 2) + ZIGZAG_UNROLLED_LOOP(5, 3) +
				ZIGZAG_UNROLLED_LOOP(5, 4) + ZIGZAG_UNROLLED_LOOP(5, 5) + ZIGZAG_UNROLLED_LOOP(5, 6) + ZIGZAG_UNROLLED_LOOP(5, 7));

			res += (ZIGZAG_UNROLLED_LOOP(6, 0) + ZIGZAG_UNROLLED_LOOP(6, 1) + ZIGZAG_UNROLLED_LOOP(6, 2) + ZIGZAG_UNROLLED_LOOP(6, 3) +
				ZIGZAG_UNROLLED_LOOP(6, 4) + ZIGZAG_UNROLLED_LOOP(6, 5) + ZIGZAG_UNROLLED_LOOP(6, 6) + ZIGZAG_UNROLLED_LOOP(6, 7));

			res += (ZIGZAG_UNROLLED_LOOP(7, 0) + ZIGZAG_UNROLLED_LOOP(7, 1) + ZIGZAG_UNROLLED_LOOP(7, 2) + ZIGZAG_UNROLLED_LOOP(7, 3) +
				ZIGZAG_UNROLLED_LOOP(7, 4) + ZIGZAG_UNROLLED_LOOP(7, 5) + ZIGZAG_UNROLLED_LOOP(7, 6) + ZIGZAG_UNROLLED_LOOP(7, 7));

			res /= 4.f;
			if (u == 0)
				res /= sqrt_2f;
		 	if (v == 0)
				res /= sqrt_2f;
			temp[v*8+u] = (short)res;
		}
	}
	{
		for (unsigned int v = 0; v < 64; ++v)
		{
			int yp = jaypeg_zigzag[v] / 8;
			int xp = jaypeg_zigzag[v] % 8;

			int temp_quant = quant[jaypeg_zigzag[v]];
			int temp_coef = temp[v];

			temp_coef =(temp_coef >= 0) ? temp_coef + temp_quant/2 : temp_coef - temp_quant/2;
			coef[yp*stride+xp] = temp_coef / temp_quant;
		}
	}
}

// FIXME: this is not the quickest way to get the bit count
static inline int count_bits(short num)
{
	int bits = 0;
	unsigned short mask = num&(1<<15);
	while (bits < 16 && (((unsigned short)num)&(1<<(15-bits))) == mask)
	{
		++bits;
		mask >>= 1;
	}
	return 16-bits;
}

OP_STATUS JayJFIFEncoder::writeBits(int data, unsigned int bits)
{
	// This means flush
	if (bits == 0)
	{
		OP_STATUS status = OpStatus::OK;
		if (bit_buffer_len)
		{
			status = output_stream->dataReady(&bit_buffer, 1);
			bit_buffer = 0;
			bit_buffer_len = 0;
		}
		return status;
	}

	while (bit_buffer_len+bits >= 8)
	{
		bit_buffer |= (data>>(bit_buffer_len+bits-8))&(0xff>>bit_buffer_len);
		RETURN_IF_ERROR(output_stream->dataReady(&bit_buffer, 1));
		if (bit_buffer == 0xff)
		{
			bit_buffer = 0;
			RETURN_IF_ERROR(output_stream->dataReady(&bit_buffer, 1));
		}
		bit_buffer = 0;
		bits -= 8-bit_buffer_len;
		bit_buffer_len = 0;
	}
	if (bits)
	{
		bit_buffer |= (data<<(8-(bits+bit_buffer_len)))&(0xff>>bit_buffer_len);
		bit_buffer_len += bits;
	}
	return OpStatus::OK;
}

// FIXME: this is a very slow huffman encoder
int JayJFIFEncoder::getHuffmanCode(UINT8 val, int component, bool dc, unsigned short& result)
{
	const unsigned char* values;
	const unsigned char* lengths;
	unsigned int valuesLen;
	if (component == 0)
	{
		if (dc)
		{
			values = HuffLuminanceDCValues;
			lengths = HuffLuminanceDCLengths;
			valuesLen = sizeof(HuffLuminanceDCValues)/sizeof(HuffLuminanceDCValues[0]);
		}
		else
		{
			values = HuffLuminanceACValues;
			lengths = HuffLuminanceACLengths;
			valuesLen = sizeof(HuffLuminanceACValues)/sizeof(HuffLuminanceACValues[0]);
		}
	}
	else
	{
		if (dc)
		{
			values = HuffChrominanceDCValues;
			lengths = HuffChrominanceDCLengths;
			valuesLen = sizeof(HuffChrominanceDCValues)/sizeof(HuffChrominanceDCValues[0]);
		}
		else
		{
			values = HuffChrominanceACValues;
			lengths = HuffChrominanceACLengths;
			valuesLen = sizeof(HuffChrominanceACValues)/sizeof(HuffChrominanceACValues[0]);
		}
	}
	unsigned int i;
	for (i = 0; i < valuesLen; ++i)
	{
		if (values[i] == val)
			break;
	}
	if (i >= valuesLen)
	{
		OP_ASSERT(FALSE);
		return 0;
	}
	// Find code i
	result = 0;
	unsigned int cnt = 0;
	for (unsigned int l = 0; l < 16; ++l)
	{
		for (unsigned int e = 0; e < lengths[l]; ++e)
		{
			if (cnt == i)
				return l+1;
			++cnt;
			++result;
		}
		result <<= 1;
	}
	OP_ASSERT(FALSE);
	return 0;
}

OP_STATUS JayJFIFEncoder::encodeMCU(short* mcu, unsigned int stride, int component)
{
	// fdct transform the blocks
	// quantize the blocks
	// zig-zag order the blocks
	fdctZigZagQuant(mcu, stride, component?quant_table_chrominance:quant_table_luminance);
	// huffman encode the result
	// send the result to the encoded stream
	int diff = (int)mcu[0]-(int)pred[component];
	pred[component] = mcu[0];
	mcu[0] = diff;
	int zeros = 0;
	unsigned short code;
	int codeLen;
	for (unsigned int v = 0; v < 8; ++v)
	{
		for (unsigned int u = 0; u < 8; ++u)
		{
			if (mcu[stride*v+u] < 0)
				--mcu[stride*v+u];
			int bits = count_bits(mcu[stride*v+u]);
			OP_ASSERT(bits <= 0xf);
			if (bits)
			{
				while (zeros > 15)
				{
					codeLen = getHuffmanCode(15<<4, component, false, code);
					if (!codeLen)
						return OpStatus::ERR;
					RETURN_IF_ERROR(writeBits(code, codeLen));
					zeros -= 16;
				}
				codeLen = getHuffmanCode((zeros<<4) | bits, component, u==0&&v==0, code);
				if (!codeLen)
					return OpStatus::ERR;
				RETURN_IF_ERROR(writeBits(code, codeLen));
				RETURN_IF_ERROR(writeBits(mcu[stride*v+u], bits));
				zeros = 0;
			}
			else if (u != 0 || v != 0)
			{
				++zeros;
			}
			else
			{
				// The DC sample must always write the number of bits, even if it is zero
				codeLen = getHuffmanCode(bits, component, true, code);
				if (!codeLen)
					return OpStatus::ERR;
				RETURN_IF_ERROR(writeBits(code, codeLen));
			}
		}
	}
	if (zeros)
	{
		// write eob
		codeLen = getHuffmanCode(0, component, false, code);
		if (!codeLen)
			return OpStatus::ERR;
		RETURN_IF_ERROR(writeBits(code, codeLen));
	}
	return OpStatus::OK;
}

OP_STATUS JayJFIFEncoder::encodeScanline(const UINT32* scanline)
{
	UINT32 b_w = (block_width << 2);	// (block_width * 4)
	UINT32 b_w8 = (block_width << 3);	// (block_width * 8)

	if (encoded_scanlines >= image_height)
		return OpStatus::OK;
	// convert to ycc and store in 8x8 blocks (2:1:1 resolution)
	unsigned int yp = encoded_scanlines%16;
	for (unsigned int xp = 0; xp < image_width; ++xp)
	{
		int R, G, B;
		R = (scanline[xp]>>16)&0xff;
		G = (scanline[xp]>>8)&0xff;
		B = scanline[xp]&0xff;
		//fixed point 16.16
		//Y = 0.299 R + 0.587 G + 0.114 B
		//Cb = - 0.1687 R - 0.3313 G + 0.5 B + 128
		//Cr = 0.5 R - 0.4187 G - 0.0813 B + 128
		int Y = 19595*R + 38470*G + 7471*B;
		int Cb = 8388608 - 11056*R - 21712*G + 32768*B;
		int Cr = 8388608 + 32768*R - 27440*G - 5328*B;

		y_blocks[yp*block_width*8+xp] = JAY_CLAMP(Y>>16);

		if (!(xp&1) && !(yp&1))
		{
			cb_blocks[(yp >> 1) * b_w + xp/2] = JAY_CLAMP(Cb>>16);
			cr_blocks[(yp >> 1) * b_w + xp/2] = JAY_CLAMP(Cr>>16);
		}
		else
		{
			cb_blocks[(yp >> 1) * b_w + xp/2] += JAY_CLAMP(Cb>>16);
			cr_blocks[(yp >> 1) * b_w + xp/2] += JAY_CLAMP(Cr>>16);
		}
	}
	if (yp == 15 || encoded_scanlines==image_height-1)
	{
		{
			// everything outside the image is set to 0 (RGB=0)
			for (unsigned int ypc = 0; ypc <= yp; ++ypc)
			{
				// Pad the luma
				for (unsigned int xp = image_width; xp < b_w8; ++xp)
					y_blocks[ypc * b_w8 + xp] = y_blocks[ypc * b_w8 + image_width-1];

				// Pad the chroma
				if (ypc & 1)
					continue;

				// ...but only do it on even lines
				unsigned int start_xp = image_width;
				unsigned int pad_idx = (ypc >> 1) * b_w + (image_width - 1) / 2;

				if (image_width & 1)
				{
					// If the image width is not even the last pixel must be duplicated
					cb_blocks[pad_idx] *= 2;
					cr_blocks[pad_idx] *= 2;

					start_xp++;
				}

				short cb_pad = cb_blocks[pad_idx];
				short cr_pad = cr_blocks[pad_idx];

				OP_ASSERT(start_xp % 2 == 0); // Should now be even

				unsigned int chroma_idx = (ypc >> 1) * b_w + (start_xp >> 1);
				while (start_xp < b_w8)
				{
					cb_blocks[chroma_idx] = cb_pad;
					cr_blocks[chroma_idx] = cr_pad;

					start_xp += 2;
					chroma_idx++;
				}
			}
		}
		if (!(yp&1))
		{
			// The last scanline was only partial for the cr and cb components
			for (unsigned int xpc = 0; xpc < b_w; ++xpc)
			{
				cb_blocks[(yp >> 1) * b_w + xpc] *= 2;
				cr_blocks[(yp >> 1) * b_w + xpc] *= 2;
			}
		}
		{
			// Clear the remaining scanlines
			for (unsigned int ypc = yp+1; ypc < 16; ++ypc)
			{
				// Set the scanlines to the last valid pixel
				op_memcpy(y_blocks+ypc * b_w8, y_blocks+yp * b_w8, b_w8 * sizeof(short));
				if (!(ypc&1))
				{
					op_memcpy(cb_blocks+(ypc>>1) * b_w, cb_blocks+(yp>>1) * b_w, b_w * sizeof(short));
					op_memcpy(cr_blocks+(ypc>>1) * b_w, cr_blocks+(yp>>1) * b_w, b_w * sizeof(short));
				}
			}
		}

		for (unsigned int i = 0; i < (block_width >> 1); ++i)
		{
			for (unsigned int v = 0; v < 8; ++v)
			{
				for (unsigned int u = 0; u < 8; ++u)
				{
					// Divide by 4 to get the average of the 4 samples
					cb_blocks[b_w *v+u+i*8] = (cb_blocks[b_w*v+u+i*8]+2) >> 2;
					cr_blocks[b_w *v+u+i*8] = (cr_blocks[b_w*v+u+i*8]+2) >> 2;
				}
			}
			// Encode the MCUs, apply fdct, quantification and huffman
			RETURN_IF_ERROR(encodeMCU(y_blocks+i*16, b_w8, 0));
			RETURN_IF_ERROR(encodeMCU(y_blocks+i*16+8, b_w8, 0));
			RETURN_IF_ERROR(encodeMCU(y_blocks+b_w8*8+i*16, b_w8, 0));
			RETURN_IF_ERROR(encodeMCU(y_blocks+b_w8*8+i*16+8, b_w8, 0));
			RETURN_IF_ERROR(encodeMCU(cb_blocks+i*8, b_w, 1));
			RETURN_IF_ERROR(encodeMCU(cr_blocks+i*8, b_w, 2));
		}
	}
	++encoded_scanlines;
	if (encoded_scanlines == image_height)
		return finish();
	return OpStatus::OK;
}

OP_STATUS JayJFIFEncoder::finish()
{
	// flush the bit buffer
	RETURN_IF_ERROR(writeBits(0, 0));
	// write the eoi marker
	UINT8 marker[2];
	marker[0] = 0xff;
	marker[1] = JAYPEG_JFIF_MARKER_EOI;
	return output_stream->dataReady(marker, 2);
}

#endif // JAYPEG_JFIF_SUPPORT && JAYPEG_ENCODE_SUPPORT

