/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "modules/pi/OpThreadTools.h"

#ifndef OP_COCOA_MAIN_THREAD_H
#define OP_COCOA_MAIN_THREAD_H

class CocoaOpMainThread : public OpThreadTools
{
public:
	CocoaOpMainThread() : m_main_thread_callback(NULL) {}
	virtual ~CocoaOpMainThread();

	OP_STATUS Init();

	virtual void* Allocate(size_t size);
	virtual void Free(void* memblock);

	OP_STATUS PostMessageToMainThread(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay = 0);
	OP_STATUS SendMessageToMainThread(OpTypedMessage* message);
	
private:
	void *m_main_thread_callback;
};

#endif // OP_COCOA_MAIN_THREAD_H
