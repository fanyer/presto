/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2003-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne
 */
#ifndef __X11_OPTHREADTOOLS_H__
#define __X11_OPTHREADTOOLS_H__

#include "modules/pi/OpThreadTools.h"
#include "adjunct/desktop_util/thread/DesktopMutex.h"

/** @brief OpThreadTools implementation
  * To use this implementation, the main message loop must call
  * GetFirstMessage() in each iteration to see if there are new messages
  * posted via OpThreadTools.
  */
class X11OpThreadTools : public OpThreadTools
{
public:
	/** Call this initialization function before using the functions here
	  * and check error value
	  */
	OP_STATUS Init();

	/** Deliver messages in the message queue (call from main thread)
	 * @return Whether any messages were delivered
	 */
	bool DeliverMessages();

	/** @return singleton instance
	  */
	static X11OpThreadTools* GetInstance() { return s_instance; }

	// From OpThreadTools
	virtual void*	  Allocate(size_t size);
	virtual void	  Free(void *memblock);

	virtual OP_STATUS PostMessageToMainThread(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay);
	virtual OP_STATUS SendMessageToMainThread(OpTypedMessage* message);

private:
	struct MainThreadMessage : public ListElement<MainThreadMessage>
	{
		MainThreadMessage(OpMessage p_msg, MH_PARAM_1 p_par1, MH_PARAM_2 p_par2, unsigned long p_delay)
			: msg(p_msg), par1(p_par1), par2(p_par2), delay(p_delay), typed_msg(NULL) {}
		MainThreadMessage(OpTypedMessage* msg) : typed_msg(msg) {}
		~MainThreadMessage() { OP_DELETE(typed_msg); }

		OpMessage msg;
		MH_PARAM_1 par1;
		MH_PARAM_2 par2;
		unsigned long delay;
		OpTypedMessage* typed_msg;
	};

	static void HandleSIGIO(int);

	DesktopMutex m_mutex;
	AutoDeleteList<MainThreadMessage> m_message_list;
	pthread_t m_main_thread;

	static X11OpThreadTools* s_instance;
};

#endif // __X11_OPTHREADTOOLS_H__
