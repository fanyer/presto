/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MACOPMAINTHREAD_H
#define MACOPMAINTHREAD_H

#include "modules/pi/OpMainThread.h"

class MacOpMainThread : public OpMainThread
{
public:
	enum {
		kOperaMessage = 'OTsM',
		kOperaMessageId = 'OpMs',
		kOperaMessageParam1 = 'OpP1',
		kOperaMessageParam2 = 'OpP2',
		kOperaDelay = 'OpDl'
	};
	MacOpMainThread();
	~MacOpMainThread();

	OP_STATUS		Init();
	void*			Allocate(size_t size);
	void			Free(void* memblock);
	OP_STATUS		PostMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay);
private:
	static OSStatus	HandlePostEvent(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData);
	EventHandlerRef	mEventHandlerRef;
	EventHandlerUPP	mEventHandlerUPP;
};

#endif //!MACOPMAINTHREAD_H
