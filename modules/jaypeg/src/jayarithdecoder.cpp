/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/jaypeg/jaypeg.h"
#if defined(_JPG_SUPPORT_) && defined (USE_JAYPEG) && defined(JAYPEG_JP2_SUPPORT)

#include "modules/jaypeg/src/jayarithdecoder.h"

#include "modules/jaypeg/src/jaystream.h"

const unsigned short JayArithDecoder::context_qe[] = {
	0x5601,
	0x3401,
	0x1801,
	0x0ac1,
	0x0521,
	0x0221,
	0x5601,
	0x5401,
	0x4801,
	0x3801,
	0x3001,
	0x2401,
	0x1c01,
	0x1601,
	0x5601,
	0x5401,
	0x5101,
	0x4801,
	0x3801,
	0x3401,
	0x3001,
	0x2801,
	0x2401,
	0x2201,
	0x1c01,
	0x1801,
	0x1601,
	0x1401,
	0x1201,
	0x1101,
	0x0ac1,
	0x09c1,
	0x08a1,
	0x0521,
	0x0141,
	0x02a1,
	0x0221,
	0x0141,
	0x0111,
	0x0085,
	0x0049,
	0x0025,
	0x0015,
	0x0009,
	0x0005,
	0x0001,
	0x5601
};

const unsigned char JayArithDecoder::context_nmps[] = {
	1,
	2,
	3,
	4,
	5,
	38,
	7,
	8,
	9,
	10,
	11,
	12,
	13,
	29,
	15,
	16,
	17,
	18,
	19,
	20,
	21,
	22,
	23,
	24,
	25,
	26,
	27,
	28,
	29,
	30,
	31,
	32,
	33,
	34,
	35,
	36,
	37,
	38,
	39,
	40,
	41,
	42,
	43,
	44,
	45,
	45,
	46
};

const unsigned char JayArithDecoder::context_nlps[] = {
	1,
	6,
	9,
	12,
	29,
	33,
	6,
	14,
	14,
	14,
	17,
	18,
	20,
	21,
	14,
	14,
	15,
	16,
	17,
	18,
	19,
	19,
	20,
	21,
	22,
	23,
	24,
	25,
	26,
	27,
	28,
	29,
	30,
	31,
	32,
	33,
	34,
	35,
	36,
	37,
	38,
	39,
	40,
	41,
	42,
	43,
	46
};

const unsigned char JayArithDecoder::context_switch[] = {
	1,
	0,
	0,
	0,
	0,
	0,
	1,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	1,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

const unsigned char JayArithDecoder::context_initial_index[] = {
	46,
	3,
	4,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

JayArithDecoder::JayArithDecoder()
{}

JayArithDecoder::~JayArithDecoder()
{}

int JayArithDecoder::init(JayStream *stream)
{
	if (stream->getLength() < 1)
	{
		return JAYPEG_NOT_ENOUGH_DATA;
	}
	reset();

	creg = stream->getBuffer()[0]<<16;
	int result = bytein(stream);
	if (result != JAYPEG_OK)
	{
		return result;
	}
	creg <<= 7;
	ct -= 7;
	areg = 0x8000;
	return JAYPEG_OK;
}

void JayArithDecoder::reset()
{
	for (unsigned int i = 0; i < JAYPEG_ENTROPY_NUM_CONTEXT; ++i)
	{
		curmps[i] = 0;
		curindex[i] = context_initial_index[i];
	}
}

unsigned int JayArithDecoder::lps_exchange(unsigned int cx, unsigned int d)
{
	if (areg < context_qe[curindex[cx]])
	{
		areg = context_qe[curindex[cx]];
		d = curmps[cx];
		curindex[cx] = context_nmps[curindex[cx]];
	}
	else
	{
		areg = context_qe[curindex[cx]];
		d = 1-curmps[cx];

		if (context_switch[curindex[cx]])
		{
			curmps[cx] = 1-curmps[cx];
		}
		curindex[cx] = context_nlps[curindex[cx]];
	}
	return d;
}

unsigned int JayArithDecoder::mps_exchange(unsigned int cx, unsigned int d)
{
	if (areg < context_qe[curindex[cx]])
	{
		d = 1-curmps[cx];

		if (context_switch[curindex[cx]])
		{
			curmps[cx] = 1-curmps[cx];
		}
		curindex[cx] = context_nlps[curindex[cx]];
	}
	else
	{
		d = curmps[cx];
		curindex[cx] = context_nmps[curindex[cx]];
	}
	return d;
}

int JayArithDecoder::renormd(JayStream *stream)
{
	unsigned int atest = 0;
	unsigned int origlen = stream->getLength();
	unsigned int origct = ct;
	unsigned int origc = creg;
	unsigned short origa = areg;
	
	while (!(atest & 0x8000))
	{
		if (ct == 0)
		{
			int result = bytein(stream);
			if ( result != JAYPEG_OK )
			{
				// rewind since this was no good
				stream->rewindBuffer(origlen-stream->getLength());
				areg = origa;
				creg = origc;
				ct = origct;
				return result;
			}
		}
		areg <<= 1;
		creg <<= 1;
		--ct;
		atest = areg;
	}
	return JAYPEG_OK;
}

int JayArithDecoder::bytein(JayStream *stream)
{
	if (stream->getLength() < 1)
	{
		return JAYPEG_NOT_ENOUGH_DATA;
	}
	if (stream->getBuffer()[0] == 0xff)
	{
		if (stream->getLength() < 2)
		{
			return JAYPEG_NOT_ENOUGH_DATA;
		}
		if (stream->getBuffer()[1]>0x8f)
		{
			creg += 0xff00;
			ct = 8;
		}
		else
		{
			creg += stream->getBuffer()[0]<<9;
			ct = 7;
			stream->advanceBuffer(1);
		}
	}
	else
	{
		creg += stream->getBuffer()[0]<<8;
		ct = 8;
		stream->advanceBuffer(1);
	}
	return JAYPEG_OK;
}

int JayArithDecoder::decode(JayStream *stream, unsigned int cx, unsigned int &d)
{
	areg = areg - context_qe[curindex[cx]];

	if ((creg>>16) < context_qe[curindex[cx]])
	{
		d = lps_exchange(cx, d);
		int result = renormd(stream);
		if (result != JAYPEG_OK)
			return result;
	}
	else
	{
		unsigned short chigh = creg >> 16;
		creg &= 0xffff;
		chigh = chigh - context_qe[curindex[cx]];
		creg |= chigh<<16;
		if (!(areg & 0x8000))
		{
			d = mps_exchange(cx, d);
			int result = renormd(stream);
			if (result != JAYPEG_OK)
				return result;
		}
		else
		{
			d = curmps[cx];
		}
	}
	return JAYPEG_OK;
}

int JayArithDecoder::readStream(JayStream *stream, unsigned short &sample)
{
	unsigned int d = 0;
	if (stream->getLength() < 1)
		return JAYPEG_NOT_ENOUGH_DATA;
	creg &= 0xffff00ff;
	creg |= stream->getBuffer()[0]<<8;
	for (unsigned int cx = 0; cx < JAYPEG_ENTROPY_NUM_CONTEXT; ++cx)
	{
		int result = decode(stream, cx, d);
		if (!result)
		{
			// FIXME: rewind?
			return JAYPEG_OK;
		}
	}
	sample = d;
	return JAYPEG_OK;
}
#endif

