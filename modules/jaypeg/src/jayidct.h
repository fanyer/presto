/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JAYIDCT_H
#define JAYIDCT_H

#include "modules/jaypeg/jaypeg.h"
#ifdef JAYPEG_JFIF_SUPPORT

/** The class responible for idct (inverse discrete cosine transform) 
 * transformation and dequantization. It converts the samples to fourier 
 * samples and performs a IDFT. */
class JayIDCT
{
public:
	JayIDCT();
	~JayIDCT();
	/** Apply the idct transfromation. 
	 * @param component the component being transformed.
	 * @param numSamples the number of samples to transform. Should always 
	 * be 64.
	 * @param stride the stide of two rows in the input sample array.
	 * @param samples input sample array.
	 * @param oSamples array which will contain the transformed values. */
	void transform(unsigned int component, unsigned int numSamples, unsigned int stride, short *samples, unsigned char *oSamples);

	/** Set which dequantization table to use for a component.
	 * @param component the component to set table number for.
	 * @param tableNum the table number to use for the specified component.
	 */
	void setComponentTableNum(unsigned int component, unsigned char tableNum);
	/** Read a dequantization table from the stream. 
	 * @param stream the data stream to read from.
	 * @returns JAYPEG_ERR_NO_MEMORY on OOM.
	 * JAYPEG_ERR if an error occured.
	 * JAYPEG_NOT_ENOUGH_DATA if there is not enough data to read the table.
	 * JAYPEG_OK if all went well. */
	int readDQT(class JayStream *stream);
private:
	unsigned short* quantTables[4];
	unsigned char compTableNum[JAYPEG_MAX_COMPONENTS];
};
#endif // JAYPEG_JFIF_SUPPORT

#endif

