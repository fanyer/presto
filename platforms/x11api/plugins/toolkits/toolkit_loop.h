/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
 
#ifndef TOOLKIT_LOOP_H
#define TOOLKIT_LOOP_H

class DescriptorListener
{
public:
	virtual ~DescriptorListener() {}
	virtual OP_STATUS OnReadReady(bool& keep_listening) = 0;
	virtual OP_STATUS OnWriteReady(bool& keep_listening) = 0;
};

class ToolkitLoop
{
public:
	enum Toolkit
	{
		GTK2
	};

	/** Create and initialize a toolkit loop of the specified type
	  * @param toolkit Type of toolkit loop to create
	  * @param read_fd File descriptor used for reading incoming messages
	  * @param write_fd File descriptor used for writing outgoing messages
	  * @return Loop that was created (caller becomes owner), or NULL on error
	  */
	static ToolkitLoop* Create(Toolkit toolkit, int read_fd, int write_fd);

	virtual ~ToolkitLoop() {}

	/** Initialize a toolkit loop
	  * @note Will be called before calling other functions in this class
	  * @param read_fd File descriptor used for reading incoming messages
	  * @param write_fd File descriptor used for writing outgoing messages
	  */
	virtual OP_STATUS Init(int read_fd, int write_fd) = 0;

	/** Notify a listener when the read descriptor passed to Init() is ready
	  * for reading
	  * @param listener Listener to call when ready for reading
	  */
	virtual OP_STATUS WatchReadDescriptor(DescriptorListener* listener) = 0;

	/** Notify a listener when the write descriptor passed to Init() is ready
	  * for writing
	  * @param listener Listener to call when ready for writing
	  */
	virtual OP_STATUS WatchWriteDescriptor(DescriptorListener* listener) = 0;

	/** Run a toolkit loop */
	virtual OP_STATUS Run() = 0;

	/** Exit the toolkit loop */
	virtual void Exit() = 0;

	/** Send a message on the main thread in the toolkit loop
	  * Can be called from threads other than the main thread
	  * @param message Message to send on the main thread
	  */
	virtual OP_STATUS SendMessageOnMainThread(OpTypedMessage* message) = 0;
};

#endif // TOOLKIT_LOOP_H
