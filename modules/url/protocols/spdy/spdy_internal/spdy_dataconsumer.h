/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Kajetan Switalski
**
*/

#ifndef _SPDY_DATACONSUMER_H_
#define _SPDY_DATACONSUMER_H_

/**
 * An interface for all classes that consume SPDY frames data
 */
class SpdyDataConsumer
{
public:
	virtual ~SpdyDataConsumer() {}

	/**
	 * Consumes some portion of the data given in the parameter.
	 * @param data Data to be consumed
	 * @return TRUE if still hungry or FALSE if already sated
	 */
	virtual BOOL ConsumeDataL(OpData &data) = 0;
};

class SpdyDevNullConsumer: public SpdyDataConsumer
{
protected:
	size_t dataToConsume;
public:
	void SetAmountOfDataToConsume(size_t n) { dataToConsume = n; }
	virtual BOOL ConsumeDataL(OpData &data);
};

class SpdyCompressionProvider;

class SpdyDecompressingConsumer: public SpdyDevNullConsumer
{
	SpdyCompressionProvider *compressionProvider;
public:
	void SetCompressionProvider(SpdyCompressionProvider *provider) { compressionProvider = provider; }
	virtual BOOL ConsumeDataL(OpData &data);
};


#endif // _SPDY_DATACONSUMER_H_