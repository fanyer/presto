/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef UNIX_PROCESS_MANAGER_H
#define UNIX_PROCESS_MANAGER_H

#include "modules/util/adt/opvector.h"
#include "platforms/posix/posix_selector.h"
#include "platforms/posix_ipc/posix_ipc_event_handler.h"

class PosixIpcProcess;
class ProcessConnection;
class IpcServer;

/** @brief Manage components in sub-processes.
 */
class PosixIpcProcessManager : public PosixSelectListener, public PosixIpcEventHandlerListener
{
public:
	PosixIpcProcessManager() : m_timeout(-1), m_nextid(1) {}

	/** Create a new component in a sub-process
	 * @param [out] peer ID given to the component manager in the sub-process
	 * @param requester Address of the requesting component
	 * @param type Type of component to create
	 */
	OP_STATUS CreateComponent(int& peer, OpMessageAddress requester, OpComponentType type);

	/** Send a message to one of the components maintained by this process manager
	 * @param message Message to send
	 */
	OP_STATUS SendMessage(OpTypedMessage* message);

	/** Process events from the components managed by this manager
	  * @param next_timeout Time limit on events in ms, or UINT_MAX for no limit
	  */
	OP_STATUS ProcessEvents(unsigned int next_timeout);

	/** Request a new timeout for ProcessEvents
	  * @param limit new timeout in ms
	  */
	void RequestRunSlice(unsigned int limit);

	// Utilities for PosixIpcProcess

	/** Register a pipe that should be watched for reading data
	 * Sub-processes will be instructed to read data when available
	 * @param fd File descriptor of pipe to watch
	 */
	OP_STATUS RegisterReadPipe(int fd) { return RegisterPipe(fd, false); }

	/** Register a pipe that should be watched for writing data
	 * Sub-processes will be instructed to write data when possible
	 * @param fd File descriptor of pipe to watch
	 */
	OP_STATUS RegisterWritePipe(int fd) { return RegisterPipe(fd, true); }

	/** Notify the process manager we have received word back from a component
	 * that was created
	 * @param address Address of the component that was created
	 */
	void OnComponentCreated(OpMessageAddress address);

	// Implementing PosixSelectListener
	virtual void OnReadReady(int fd);
	virtual void OnWriteReady(int fd);
	virtual void OnError(int fd, int err);
	virtual void OnDetach(int fd);

	// Implementing PosixIpcEventHandlerListener
	virtual void OnSourceError(IpcHandle* source);

private:
	class PipeHolder;

	PosixIpcProcess* GetProcess(int cm_id);
	OP_STATUS GetLogPath(OpString8& log_path);
	OP_STATUS RegisterPipe(int fd, bool write);
	OP_STATUS SetNonBlocking(int fd);
	void KillProcess(PosixIpcProcess* process, bool detach = true);

	AutoDeleteList<PosixIpcProcess> m_processes;
	double m_timeout;
	int m_nextid;
};

#endif // UNIX_PROCESS_MANAGER_H
