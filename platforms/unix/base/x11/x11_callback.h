/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef X11_CALLBACK
#define X11_CALLBACK

#include "modules/hardcore/mh/messobj.h"

/**
 * Abstract class to be reimplemented in any class that is interested in
 * a call-back from X11OpMessageLoop.
 */
class X11CbObject : public MessageObject
{
public:
	virtual ~X11CbObject();

	/** Post a callback with a specified event,
	 * @param data Data to receive
	 * @param delay Delay in ms
	 */
	OP_STATUS PostCallback(INTPTR data, unsigned long delay = 0);

	/**
	 * Handle a call-back.
	 * @param data event data as specified in PostCallback
	 */
	virtual void HandleCallBack(INTPTR data) = 0;

	// Implementing MessageObject
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
};

#endif // X11_CALLBACK
