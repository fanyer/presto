/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JAYHUFFDECODER_H
#define JAYHUFFDECODER_H

#include "modules/jaypeg/jaypeg.h"

#ifdef JAYPEG_JFIF_SUPPORT

#include "modules/jaypeg/src/jayentropydecoder.h"


#define JAY_HUFF_VAL_PTR_BITS 12

/** The huffman decoder used by JayJFIFDecoder. */
class JayHuffDecoder : public JayEntropyDecoder
{
public:
	JayHuffDecoder();
	~JayHuffDecoder();
	/** Initialize the huffman decoder.
	 * @returns JAYPEG_OK. */
	int init();

	void reset();
	/** Set which tables should be used for a component. 
	 * @param component the component to set the tables for.
	 * @param dctn the table for dc samples.
	 * @param actn the table for ac samples. */
	void setComponentTableNumber(unsigned int component, unsigned char dctn, unsigned char actn);
	
	/** Parse a difinition of a huffman table. 
	 * @param stream the data stream to read from.
	 * @returns JAYPEG_ERR_NO_MEMORY on OOM.
	 * JAYPEG_ERR if an error occured.
	 * JAYPEG_NOT_ENOUGH_DATA if there is not enought data in the stream.
	 * JAYPEG_OK if all went well. */
	int readDHT(class JayStream *stream);
	
	/** Tell the huffman decoder that no DC samples should be decoded. 
	 * @param dc set to TRUE to skip decoding of dc samples, FALSE to 
	 * decode the dc samples. */
	void noDCSamples(BOOL dc);
	/** Tell the huffman decoder to enable eob runs longer than one data 
	 * unit. 
	 * @param enabled TRUE to enable EOB, FALSE to disable. */
	void enableEOB(BOOL enabled);
	/** Tell the huffman decoder that we are in a refinement pass. 
	 * @param alow -1 to disable refinement, value >= means the new
	 * bits should be shifted left by that amount. */
	void enableRefinement(int alow);

	int readSamples(JayStream *stream, unsigned int component, unsigned int numSamples, short *samples);
private:
	/** Refine a number of non-zero AC samples using correction bit. */
	int refineNonZeroACSamples(JayStream *stream, int numSamples, int numZero, short *samples, unsigned int &samplecount);
	/** Read a number of bits from the stream without any decoding. */
	int readBits(JayStream *stream, unsigned int bits, unsigned int &value, BOOL extend = TRUE);
	/** Read one bit from the stream. */
	int readBit(JayStream *stream, unsigned int &bit);
	/** decode one huffman encoded number from the stream. */
	int decodeHuffman(JayStream *stream, unsigned char &val);
	/** Slow fallback if the optimized decodeHuffman fails. Will be called 
	 * from the fast decodeHuffman. */
	int slowDecodeHuffman(JayStream *stream, unsigned char &val, int minbits);
	

	struct HuffmanTable
	{
		int HuffMaxCodeAndValPtr[16];
		unsigned short HuffMinCode[16];
	};
   	unsigned char* HuffTable[8];
	unsigned char *curHuffTable;

	// one per component
	short compLastDC[JAYPEG_MAX_COMPONENTS];
	// table number, two per component
	unsigned char compTableNum[JAYPEG_MAX_COMPONENTS*2];

	BOOL skipDC;
	int eob_len;
	int approxLow;

	void fillBitBuffer(JayStream *stream);
	unsigned int bitBuffer;
	int bitBufferLen;
};

#endif // JAYPEG_JFIF_SUPPORT
#endif
