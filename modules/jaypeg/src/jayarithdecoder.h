/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JAYARITHDECODER_H
#define JAYARITHDECODER_H

#include "modules/jaypeg/jaypeg.h"
#ifdef JAYPEG_JP2_SUPPORT

#include "modules/jaypeg/src/jayentropydecoder.h"

#define JAYPEG_ENTROPY_NUM_CONTEXT 19

class JayArithDecoder : public JayEntropyDecoder
{
public:
	JayArithDecoder();
	~JayArithDecoder();
	int init(class JayStream *stream);

	int readStream(class JayStream *stream, unsigned short &sample);

	void reset();

private:
	static const unsigned short context_qe[];
	static const unsigned char context_nmps[];
	static const unsigned char context_nlps[];
	static const unsigned char context_switch[];

	static const unsigned char context_initial_index[];

	unsigned short curmps[JAYPEG_ENTROPY_NUM_CONTEXT];
	unsigned char curindex[JAYPEG_ENTROPY_NUM_CONTEXT];

	unsigned int lps_exchange(unsigned int cx, unsigned int d);
	unsigned int mps_exchange(unsigned int cx, unsigned int d);
	int renormd(class JayStream *stream);
	int bytein(class JayStream *stream);
	int decode(class JayStream *stream, unsigned int cx, unsigned int &d);

	unsigned int ct;
	
	unsigned int creg;
	unsigned short areg;

};
#endif // JAYPEG_JP2_SUPPORT
#endif

