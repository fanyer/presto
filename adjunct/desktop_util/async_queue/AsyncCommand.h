/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef ASYNC_COMMAND_H
#define ASYNC_COMMAND_H

#include "adjunct/desktop_util/async_queue/AsyncQueue.h"

/** @brief Implement this abstract class to create a command for use in AsyncQueue
 */
class AsyncCommand : private Link
{
public:
	virtual ~AsyncCommand() { Out(); }

	/** Execute this command - will be called if this command has been added to a queue
	  */
	virtual void Execute() = 0;

	/** @return Queue that this command has been queued in, or 0 if it hasn't been queued yet
	  * This value will always be non-NULL when called from Execute()
	  */
	AsyncQueue* GetQueue() { return static_cast<AsyncQueue*>(GetList()); }

private:
	friend class AsyncQueue;
};

#endif // ASYNC_COMMAND_H
