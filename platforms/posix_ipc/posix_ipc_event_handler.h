/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef POSIX_IPC_EVENT_HANDLER_H
#define POSIX_IPC_EVENT_HANDLER_H

class IpcHandle;

class PosixIpcEventHandlerListener
{
public:
	virtual ~PosixIpcEventHandlerListener() {}
	virtual void OnSourceError(IpcHandle* source) = 0;
};

/** @brief Handle events from specified sources
  */
class PosixIpcEventHandler
{
public:
	/** Constructor
	  * @param listener Receiver of error messages from sources, or NULL for no listener
	  */
	explicit PosixIpcEventHandler(PosixIpcEventHandlerListener* listener) : m_listener(listener) {}

	/** Add an event source to be monitored by this handler
	  * @param source Source to add
	  */
	OP_STATUS AddEventSource(IpcHandle* source) { return m_sources.Add(source); }

	/** Handle events from sources added by AddEventSource until timeout ends or instructed to stop
	  * @param timeout If non-NULL, stop handling events after this time has passed or if any event occurred
	  *                Specified in the same format as OpComponentPlatform::GetRuntimeMS()
	  *                If NULL, run indefinitely
	  * @param stop_running Parameter that can be set to true to stop the loop
	  */
	OP_STATUS HandleEvents(const double* timeout, bool& stop_running);

	/** Request a send action on the specified handle
	  * @param source Source that wants to send data
	  */
	OP_STATUS RequestSend(IpcHandle* source) { return OpStatus::OK; }

private:
	inline bool HandleSourceStatus(IpcHandle* source, OP_STATUS& status);

	OpVector<IpcHandle> m_sources;
	PosixIpcEventHandlerListener* m_listener;
};

#endif // POSIX_IPC_EVENT_HANDLER_H
