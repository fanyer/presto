/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef ASYNC_QUEUE_H
#define ASYNC_QUEUE_H

class AsyncCommand;

/** @brief Queue for AsyncCommands
  * Use this class to easily set yourself up for executing asynchronous commands.
  * Example:
  *   AsyncQueue queue(100);
  *   class HelloWorldCommand : public AsyncCommand {
  *       virtual void Execute() { printf("hello world!"); }
  *   };
  *
  *   queue.AddCommand(OP_NEW(HelloWorldCommand, ()));
  */

class AsyncQueue : protected MessageObject, private AutoDeleteHead
{
public:
	/** Constructor
	  * @param delay (time in ms) Commands added to this queue will be executed after at least this much time
	  */
	AsyncQueue(unsigned long delay = 0);
	virtual ~AsyncQueue() {}

	/** Add a command to the queue, will be executed asynchronously
	  * @param command Command to add, ownership is transferred to this queue
	  */
	virtual OP_STATUS AddCommand(AsyncCommand* command);

	/** @return Whether the queue is currently empty (no unfinished commands)
	  */
	BOOL Empty() { return AutoDeleteHead::Empty(); }


protected:
	inline MessageHandler& GetMessageHandler() { return m_message_handler; }
	virtual BOOL PostMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay) { return m_message_handler.PostMessage(msg, par1, par2, delay); }

	// From MessageObject
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	friend class AsyncCommand;

	OP_STATUS SetupCallBack();

	MessageHandler m_message_handler;
	unsigned long m_delay;
	BOOL m_callback_initialized;
};

#endif // ASYNC_QUEUE_H
