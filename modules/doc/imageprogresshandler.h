/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef IMAGEPROGRESSHANDLER_H
#define IMAGEPROGRESSHANDLER_H

#include "modules/hardcore/mh/messobj.h"
#include "modules/img/image.h"

/**
 * Something that should move to the img module I think. Only used
 * internally in the img module anyway.
 */
class ImageProgressHandler : public MessageObject, public ImageProgressListener
{
public:
	static ImageProgressHandler* Create();
	~ImageProgressHandler();

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	void OnProgress();
#ifdef ASYNC_IMAGE_DECODERS_EMULATION
	void OnMoreBufferedData();
	void OnMoreDeletedDecoders();
#endif // ASYNC_IMAGE_DECODERS_EMULATION

private:
	ImageProgressHandler();
	BOOL m_pending_progress;
};

#endif // IMAGEPROGRESSHANDLER_H
