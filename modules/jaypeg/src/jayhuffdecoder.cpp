/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/jaypeg/jaypeg.h"

#if defined(_JPG_SUPPORT_) && defined (USE_JAYPEG) && defined(JAYPEG_JFIF_SUPPORT)


#include "modules/jaypeg/src/jayhuffdecoder.h"
#include "modules/jaypeg/src/jaystream.h"

// Must never be more than 8
#define JAYPEG_HUFF_LOOK_AHEAD_BITS 8

JayHuffDecoder::JayHuffDecoder() : skipDC(FALSE), eob_len(-1), approxLow(-1), bitBuffer(0), bitBufferLen(0)
{
	for (int i = 0; i < 8; ++i)
		HuffTable[i] = NULL;
}

JayHuffDecoder::~JayHuffDecoder()
{
	for (int i = 0; i < 8; ++i)
		OP_DELETEA(HuffTable[i]);
}

int JayHuffDecoder::init()
{
	for (unsigned int i = 0; i < JAYPEG_MAX_COMPONENTS; ++i)
	{
		compTableNum[i*2] = 0;
		compTableNum[i*2+1] = 4;
	}
	reset();

	return JAYPEG_OK;
}

void JayHuffDecoder::reset()
{
	for (unsigned int i = 0; i < JAYPEG_MAX_COMPONENTS; ++i)
	{
		compLastDC[i] = 0;
	}
	if (eob_len > 0)
		eob_len = 0;

	OP_ASSERT(bitBufferLen < 8);
	bitBuffer = 0;
	bitBufferLen = 0;
}

void JayHuffDecoder::setComponentTableNumber(unsigned int component, unsigned char dctn, unsigned char actn)
{
	compTableNum[component*2] = dctn;
	compTableNum[component*2+1] = 4+actn;
}

int JayHuffDecoder::readDHT(JayStream *stream)
{
	unsigned char *HuffSize;
	unsigned short *HuffCode;

	unsigned int len;
	if (stream->getLength() < 4)
	{
		return JAYPEG_NOT_ENOUGH_DATA;
	}
	len = (stream->getBuffer()[2]<<8)|stream->getBuffer()[3];
	if (len < 2)
		return JAYPEG_ERR;
	if (stream->getLength() < len+2)
	{
		return JAYPEG_NOT_ENOUGH_DATA;
	}
	len -= 2;
	stream->advanceBuffer(4);
	while (len > 0)
	{
		if (len < 17)
		{
			// FIXME: error recovery?
			return JAYPEG_ERR;
		}
		unsigned int tabnum;
		tabnum = stream->getBuffer()[0]&0xf;
		if (tabnum >=4 || stream->getBuffer()[0]&0xe0)
		{
			return JAYPEG_ERR;
		}
		tabnum |= (stream->getBuffer()[0]&0x10)>>2;
		stream->advanceBuffer(1);
		len -= 1;

		
		// Read the lengths array
		unsigned int k, i, j, lastk, code;
		k = 0;
		const unsigned char* huff_sizes = stream->getBuffer();
		unsigned int total_huff_size = 0;
		for (i = 0; i < 16; ++i)
		{
			total_huff_size += huff_sizes[i];
		}
		// allocate HuffSize as total_huff_size + 1 and HuffCode as total_huff_size
		HuffSize = OP_NEWA(unsigned char, total_huff_size+1);
		HuffCode = OP_NEWA(unsigned short, total_huff_size);
		if (!HuffSize || !HuffCode)
		{
			OP_DELETEA(HuffSize);
			OP_DELETEA(HuffCode);
			return JAYPEG_ERR_NO_MEMORY;
		}
		for (i = 0; i < 16; ++i)
		{
			j = 1;
			while (j <= huff_sizes[i])
			{
				HuffSize[k] = i+1;
				++j;
				++k;
			}
		}
		HuffSize[k] = 0;
		lastk = k;
		// Generate the codes
		k = 0;
		code = 0;
		unsigned char si = HuffSize[0];
		while (HuffSize[k] != 0)
		{
			while (HuffSize[k] == si)
			{
				// Too long codes will cause a memory overwrite in the lookahead buffer
				if ((code&((-1)<<si)) != 0)
				{
					// Detected a too long code
					OP_DELETEA(HuffSize);
					OP_DELETEA(HuffCode);
					return JAYPEG_ERR;
				}
				HuffCode[k] = code;
				++code;
				++k;
			}
			
			if (HuffSize[k] != 0)
			{
				while (HuffSize[k] != si)
				{
					code <<= 1;
					++si;
				}
			}
		}

		// allocate the table
		OP_DELETEA(HuffTable[tabnum]);
		// FIXME: thismuch memory is not really needed since all codes with <8bits can be stored directly
		const UINT32 size = sizeof(struct HuffmanTable) +
			total_huff_size + (2<<JAYPEG_HUFF_LOOK_AHEAD_BITS);
		HuffTable[tabnum] = OP_NEWA(unsigned char, size);
		if (!HuffTable[tabnum])
		{
			OP_DELETEA(HuffSize);
			OP_DELETEA(HuffCode);
			return JAYPEG_ERR_NO_MEMORY;
		}
		j = 0;
		for (i = 0; i < 16; ++i)
		{
			if (huff_sizes[i] == 0)
			{
				((HuffmanTable*)HuffTable[tabnum])->HuffMaxCodeAndValPtr[i] = (-1)<<JAY_HUFF_VAL_PTR_BITS;
			}
			else
			{
				// Maximum value of val ptr is 16*255 = 4080, which will need 12 bits
				// Max code will need 17 bits so if they are joined together we can save some memory
				((HuffmanTable*)HuffTable[tabnum])->HuffMaxCodeAndValPtr[i] = j;
				((HuffmanTable*)HuffTable[tabnum])->HuffMinCode[i] = HuffCode[j];
				j += huff_sizes[i]-1;
				((HuffmanTable*)HuffTable[tabnum])->HuffMaxCodeAndValPtr[i] |= (HuffCode[j]<<JAY_HUFF_VAL_PTR_BITS);
				++j;
			}
		}
		
		stream->advanceBuffer(16);
		len -= 16;
		// Read the values
		if (len < lastk)
		{
			// free the data used
			OP_DELETEA(HuffSize);
			OP_DELETEA(HuffCode);
			return JAYPEG_ERR;
		}
		unsigned char *HuffVal = HuffTable[tabnum]+sizeof(HuffmanTable)+(2<<JAYPEG_HUFF_LOOK_AHEAD_BITS);
		for (i = 0; i < lastk; ++i)
		{
			HuffVal[i] = stream->getBuffer()[i];
		}
		stream->advanceBuffer(lastk);
		len -= lastk;
		// Generate some internal decoding tables
		unsigned char *HuffLook = HuffTable[tabnum]+sizeof(struct HuffmanTable);
		for (i = 0; i < (1<<JAYPEG_HUFF_LOOK_AHEAD_BITS); ++i)
		{
			// unless we have a code for it, it must be atleast JAYPEG_HUFF_LOOK_AHEAD_BITS bits long
			HuffLook[i] = JAYPEG_HUFF_LOOK_AHEAD_BITS+1;
		}
		j = 0;
		for (i = 1; i <= JAYPEG_HUFF_LOOK_AHEAD_BITS; ++i)
		{
			// FIXME: lookahead table
			for (int ci = 0; ci < huff_sizes[i-1]; ++ci)
			{
				unsigned int code8 = HuffCode[j] << (JAYPEG_HUFF_LOOK_AHEAD_BITS-i);
				for (int lowbits = 0; lowbits < (1<<(JAYPEG_HUFF_LOOK_AHEAD_BITS-i)); ++lowbits)
				{
					HuffLook[code8|lowbits] = i;
					HuffLook[(1<<JAYPEG_HUFF_LOOK_AHEAD_BITS)+(code8|lowbits)] = HuffVal[j];
				}
				++j;
			}

		}
		// free the data used
		OP_DELETEA(HuffSize);
		OP_DELETEA(HuffCode);
	}
	return JAYPEG_OK;
}

void JayHuffDecoder::noDCSamples(BOOL dc)
{
	skipDC = dc;
}

void JayHuffDecoder::enableEOB(BOOL enabled)
{
	if (enabled && eob_len < 0)
		eob_len = 0;
	if (!enabled)
		eob_len = -1;
}

void JayHuffDecoder::enableRefinement(int alow)
{
	approxLow = alow;
}

/** Refine all non-zero AC samples. This can be used to either iterate a number
 * of samples or a number of zero samples. The number of samples iterated will
 * be stored in samplecount. */
int JayHuffDecoder::refineNonZeroACSamples(JayStream *stream, int numSamples, int numZero, short *samples, unsigned int &samplecount)
{
	int result;
	unsigned int samp = 0;
	int i;
	for (i = 0; i < numSamples; ++i)
	{
		if (samples[i] != 0)
		{
			// one bit correction
			result = readBit(stream, samp);
			if (result != JAYPEG_OK)
			{
				return result;
			}
			// FIXME: verify
			if (samp)
			{
				if (samples[i] < 0)
					samples[i] += ((-1)<<approxLow);
				else
					samples[i] += (1<<approxLow);
			}
		}
		else
		{
			samplecount = i;
			--numZero;
			if (numZero < 0)
				return JAYPEG_OK;
		}
	}
	samplecount = i;
	return JAYPEG_OK;
}

int JayHuffDecoder::readSamples(JayStream *stream, unsigned int component, unsigned int numSamples, short *samples)

{
	unsigned int startlen = stream->getLength();
	unsigned int startBitBuffer = bitBuffer;
	int startBitBufferLen = bitBufferLen;

	unsigned int tabnum;
	int result;
	unsigned char t;
	unsigned int samp = 0;
	unsigned int i;

	/* we are in an EOB run. */
	if (eob_len > 0)
	{
		// This should never occur with dc samples
		OP_ASSERT(skipDC);
		if (approxLow >= 0)
		{
			result = refineNonZeroACSamples(stream, numSamples, numSamples, samples, samp);
			if (result != JAYPEG_OK)
			{
				stream->rewindBuffer(startlen-stream->getLength());
				bitBuffer = startBitBuffer;
				bitBufferLen = startBitBufferLen;
				return result;
			}
		}
		--eob_len;
		return JAYPEG_OK;
	}

	if (!skipDC)
	{
		if (approxLow >= 0)
		{
			// append one bit
			result = readBit(stream, samp);
			if (result != JAYPEG_OK)
			{
				stream->rewindBuffer(startlen-stream->getLength());
				bitBuffer = startBitBuffer;
				bitBufferLen = startBitBufferLen;
				return result;
			}
			samples[0] |= (samp<<approxLow);
		}
		else
		{
			tabnum = compTableNum[component*2];
			curHuffTable = HuffTable[tabnum];
			if (!curHuffTable)
			{
				return JAYPEG_ERR;
			}
			result = decodeHuffman(stream, t);
			if (result != JAYPEG_OK)
			{
				stream->rewindBuffer(startlen-stream->getLength());
				bitBuffer = startBitBuffer;
				bitBufferLen = startBitBufferLen;
				return result;
			}
			// t is now the number of bits to copy to dc
			result = readBits(stream, t, samp);
			if (result != JAYPEG_OK)
			{
				stream->rewindBuffer(startlen-stream->getLength());
				bitBuffer = startBitBuffer;
				bitBufferLen = startBitBufferLen;
				return result;
			}
			samples[0] = (short)samp + compLastDC[component];
		}
	}
	tabnum = compTableNum[component*2+1];
	curHuffTable = HuffTable[tabnum];
	if ((skipDC || numSamples>1) && !curHuffTable)
	{
		return JAYPEG_ERR;
	}
	// Now decode the AC:s
	for (i = (skipDC?0:1); i < numSamples; ++i)
	{
		result = decodeHuffman(stream, t);
		if (result != JAYPEG_OK)
		{
			stream->rewindBuffer(startlen-stream->getLength());
			bitBuffer = startBitBuffer;
			bitBufferLen = startBitBufferLen;
			return result;
		}
		unsigned char ssss = t&0xf;
		unsigned char rrrr = t>>4;
		if (ssss == 0)
		{
			if (rrrr!=15)
			{
				if (rrrr>0)
				{
					OP_ASSERT(eob_len == 0);
					if (eob_len < 0)
					{
						return JAYPEG_ERR;
					}
					result = readBits(stream, rrrr, samp, FALSE);
					if (result != JAYPEG_OK)
					{
						stream->rewindBuffer(startlen-stream->getLength());
						bitBuffer = startBitBuffer;
						bitBufferLen = startBitBufferLen;
						return result;
					}
					eob_len = 1<<rrrr;
					eob_len += samp;
					--eob_len;
				}
				if (approxLow >= 0)
				{
					// if we are refining, all non-zero samples must be refined
					result = refineNonZeroACSamples(stream, numSamples-i, numSamples, samples+i, samp);
					if (result != JAYPEG_OK)
					{
						eob_len = 0;
						stream->rewindBuffer(startlen-stream->getLength());
						bitBuffer = startBitBuffer;
						bitBufferLen = startBitBufferLen;
						return result;
					}
				}
				if (!skipDC && approxLow<0)
				{
					compLastDC[component] = samples[0];
				}
				return JAYPEG_OK;
			}
			// this is a zrl (zero run length)
			if (approxLow>=0)
			{
				// read rrrr zeros
				result = refineNonZeroACSamples(stream, numSamples-i, rrrr, samples+i, samp);
				if (result != JAYPEG_OK)
				{
					stream->rewindBuffer(startlen-stream->getLength());
					bitBuffer = startBitBuffer;
					bitBufferLen = startBitBufferLen;
					return result;
				}
				i+=samp;
			}
			else
			{
				i += rrrr;
			}
		}
		else
		{
			short newsamp;
			OP_ASSERT(approxLow<0 || ssss == 1);
			result = readBits(stream, ssss, samp);
			if (result != JAYPEG_OK)
			{
				stream->rewindBuffer(startlen-stream->getLength());
				bitBuffer = startBitBuffer;
				bitBufferLen = startBitBufferLen;
				return result;
			}
			newsamp = (short)samp;

			if (approxLow>=0)
			{
				result = refineNonZeroACSamples(stream, numSamples-i, rrrr, samples+i, samp);
				if (result != JAYPEG_OK)
				{
					stream->rewindBuffer(startlen-stream->getLength());
					bitBuffer = startBitBuffer;
					bitBufferLen = startBitBufferLen;
					return result;
				}
				i+=samp;
				newsamp <<= approxLow;
			}
			else
			{
				i += rrrr;
			}
			if (i >= numSamples)
			{
				// This is an atempt to recover from an error, it is not likely to succeed
				//OP_ASSERT(FALSE);
				i = numSamples-1;
			}

			samples[i] = newsamp;
		}
	}
	if (!skipDC && approxLow<0)
	{
		compLastDC[component] = samples[0];
	}
	return JAYPEG_OK;
}

inline int JayHuffDecoder::readBits(JayStream *stream, unsigned int bits, unsigned int &value, BOOL extend)
{
	if (bits == 0)
	{
		value = 0;
		return JAYPEG_OK;
	}
	OP_ASSERT(bits <= 25); // the buffer allways contains atleast 25 bits when filled
	if (bitBufferLen < (int)bits)
	{
		fillBitBuffer(stream);
		if (bitBufferLen < (int)bits)
			return JAYPEG_NOT_ENOUGH_DATA;
	}
	value = bitBuffer>>(32-bits);
	bitBuffer<<=bits;
	bitBufferLen -= bits;
	// extend
	if (extend && value < (unsigned int)(1<<(bits-1)))
	{
		value += ((-1)<<bits)+1;
	}
	return JAYPEG_OK;
}

inline int JayHuffDecoder::readBit(JayStream *stream, unsigned int &bit)
{
	if (!bitBufferLen)
	{
		fillBitBuffer(stream);
		if (!bitBufferLen)
			return JAYPEG_NOT_ENOUGH_DATA;
	}
	bit = bitBuffer>>31;
	bitBuffer <<= 1;
	--bitBufferLen;
	return JAYPEG_OK;
}

inline int JayHuffDecoder::decodeHuffman(JayStream *stream, unsigned char &val)
{
	//int result;
	int i;
	unsigned int code;

	if (bitBufferLen < JAYPEG_HUFF_LOOK_AHEAD_BITS)
	{
		fillBitBuffer(stream);
		if (bitBufferLen < JAYPEG_HUFF_LOOK_AHEAD_BITS)
		{
			return slowDecodeHuffman(stream, val, 1);
		}
	}

	code = bitBuffer>>(32-JAYPEG_HUFF_LOOK_AHEAD_BITS);
	// set to the number of bits
	i = curHuffTable[sizeof(HuffmanTable)+code];

	if (i <= JAYPEG_HUFF_LOOK_AHEAD_BITS)
	{
		// if we managed to read and can rewind in current byte, do that
		val = curHuffTable[sizeof(HuffmanTable)+(1<<JAYPEG_HUFF_LOOK_AHEAD_BITS)+code];
		bitBuffer <<= i;
		bitBufferLen -= i;
		return JAYPEG_OK;
	}
	return slowDecodeHuffman(stream, val, JAYPEG_HUFF_LOOK_AHEAD_BITS+1);
}
int JayHuffDecoder::slowDecodeHuffman(JayStream *stream, unsigned char &val, int minbits)
{
	int i = minbits-1;
	unsigned int code = 0, bit = 0;
	int result;
	result = readBits(stream, minbits, code, FALSE);
	if (result != JAYPEG_OK)
		return result;
	while (i < 16 && (int)code > (((HuffmanTable*)curHuffTable)->HuffMaxCodeAndValPtr[i]>>JAY_HUFF_VAL_PTR_BITS))
	{
		++i;
		result = readBit(stream, bit);
		if (result != JAYPEG_OK)
			return result;
		code = (code<<1)|bit;
	}
	if (i >= 16)
		return JAYPEG_ERR;
	unsigned int j = (((HuffmanTable*)curHuffTable)->HuffMaxCodeAndValPtr[i]&~((-1)<<JAY_HUFF_VAL_PTR_BITS));
	j += code-((HuffmanTable*)curHuffTable)->HuffMinCode[i];
	val = curHuffTable[sizeof(HuffmanTable)+(2<<JAYPEG_HUFF_LOOK_AHEAD_BITS)+j];

	return JAYPEG_OK;
}

void JayHuffDecoder::fillBitBuffer(JayStream *stream)
{
	// FIXME: if shift left 0 bytes is broken it is probably better to make sure bitBufferLen is less than 24 here
	while (bitBufferLen <= 24 && stream->getLength() > 0)
	{
		unsigned int tmpbits = stream->getBuffer()[0];
		if (tmpbits == 0xff)
		{
			if (stream->getLength() < 2 || stream->getBuffer()[1] != 0)
			{
				return;
			}
			stream->advanceBuffer(1);
		}
		stream->advanceBuffer(1);
		bitBuffer |= (tmpbits << (24-bitBufferLen));
		bitBufferLen += 8;
	}
}
#endif
