/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JAYENCODEDDATA_H
#define JAYENCODEDDATA_H

#include "modules/jaypeg/jaypeg.h"

#ifdef JAYPEG_ENCODE_SUPPORT

/** This is the callback class of the jaypeg encoder. Inherit this class to be 
 * notified when more data should be written to the encoded stream. */
class JayEncodedData
{
public:
	virtual ~JayEncodedData(){}
	/** More data is available to the encoded stream. If this function returns
	 * an error the encoding will be aborted.
	 * @param data the new data available.
	 * @param datalen the length of the data in bytes.
	 * @return OpStatus::OK on success, an error otherwise. */
	virtual OP_STATUS dataReady(const void* data, unsigned int datalen) = 0;
};

#endif // JAYPEG_ENCODE_SUPPORT

#endif // JAYENCODEDDATA_H

