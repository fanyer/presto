/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWS_OP_MAIN_THREAD_H
#define WINDOWS_OP_MAIN_THREAD_H

#if defined(USE_OP_THREAD_TOOLS)

#include "modules/pi/OpThreadTools.h"
#include "modules/hardcore/mh/mh.h"

#include "platforms/windows/IPC/WindowsOpComponentPlatform.h"
#include "platforms/windows/utils/sync_primitives.h"


struct EncapsulatedMessage
{
	EncapsulatedMessage(OpMessage m, MH_PARAM_1 p1, MH_PARAM_2 p2, unsigned long d = 0)
		: msg(m), par1(p1), par2(p2), delay(d) {}

	OpMessage     msg;
	MH_PARAM_1    par1;
	MH_PARAM_2    par2;
	unsigned long delay;
};

class MainThreadSender: public WindowsOpComponentPlatform::WMListener
{
public:
	static MainThreadSender* Construct();
	static MainThreadSender* GetInstance();
	static void Destruct();

	void Finalize();

	//Implementing WindowsOpComponentPlatform::WMListener
	void HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

	OP_STATUS PostMessageToMainThread(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay);
	OP_STATUS SendMessageToMainThread(OpTypedMessage* message);

private:
	MainThreadSender();
	MainThreadSender(const MainThreadSender& sender) {}
	MainThreadSender& operator = (const MainThreadSender& other) {}
	~MainThreadSender();

	LRESULT	ProcessMessage(EncapsulatedMessage* msg);
	LRESULT ProcessMessage(OpTypedMessage* message);

	OpCriticalSection				m_vector_lock;
	BOOL							m_active;
	HWND							m_message_window;
	OpVector<EncapsulatedMessage>	m_messages;
	OpVector<OpTypedMessage>		m_typed_messages;

	static MainThreadSender*		s_instance;
	static BOOL						s_destroyed;
};

class WindowsOpThreadTools : public OpThreadTools
{
public:
	WindowsOpThreadTools() {}
	virtual ~WindowsOpThreadTools() {}

	/**
	 * Process all pending messages and prepare for shut down.
	 * After Finalize all calls to PostMessage, SendMessage, and ProcessMessage will fail.
	 */
	virtual void* Allocate(size_t size);
	virtual void Free(void* memblock);
	virtual OP_STATUS PostMessageToMainThread(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay);
	virtual OP_STATUS SendMessageToMainThread(OpTypedMessage* message);

};

#endif // USE_OP_THREAD_TOOLS

#endif // WINDOWS_OP_MAIN_THREAD_H
