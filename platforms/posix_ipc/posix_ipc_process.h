/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef UNIX_PROCESS_H
#define UNIX_PROCESS_H

class PosixIpcProcessManager;
class IpcHandle;

/** @brief Represents an Opera sub-process
 */
class PosixIpcProcess : public ListElement<PosixIpcProcess>
{
public:
	/** Constructor
	 * @param type Type of the component being created in the sub-process
	 * @param cm_id ID of the component manager used in this sub-process
	 * @param mgr Manager that maintains this sub-process
	 * @param handle Handle to the read and write pipes of the sub-process
	 */
	PosixIpcProcess(OpComponentType type, int cm_id, PosixIpcProcessManager& mgr, IpcHandle* handle);
	~PosixIpcProcess();

	/** Initialize the process - call before calling other functions below
	 */
	OP_STATUS Init();

	/** Set the Process ID of this sub-process
	 * @param pid Process ID
	 */
	void SetPid(pid_t pid) { m_pid = pid; }

	/** Mark this process as successfully initialized (the process has
	 * established communication with the main Opera process)
	 */
	void MarkInitialized() { m_killonexit = false; }

	/** @return the ID of the component manager used in this process
	 */
	int GetComponentManagerId() const { return m_cm_id; }

	/** Check whether this process uses a certain file descriptor for reading
	 * @param fd File descriptor to check
	 * @return Whether data from this sub-process can be read using @ref fd
	 */
	bool HasReadFd(int fd);

	/** Check whether this process uses a certain file descriptor for writing
	 * @param fd File descriptor to check
	 * @return Whether data can be written to this sub-process @ref fd
	 */
	bool HasWriteFd(int fd);

	/** @return IPC Handle of this sub-process
	 */
	IpcHandle* GetHandle() const { return m_ipc; }

	/** Read data from this process if available
	 * @param fd File descriptor to read from. If this sub-process doesn't
	 *   own @ref fd, do nothing
	 */
	OP_STATUS Read(int fd);

	/** Write data to this process if there is data to send
	 * @param fd File descriptor to write to. If this sub-process doesn't
	 *   own @ref fd, do nothing
	 */
	OP_STATUS Write(int fd);

	/** Send a message to this process
	 * @param msg Message to send
	 */
	OP_STATUS Send(OpTypedMessage* msg);

private:
	void Kill();
	int WaitForExit();
	OP_STATUS HandleRemainingBytes();

	bool m_killonexit;
	pid_t m_pid;
	IpcHandle* m_ipc;
	int m_cm_id;
	OpComponentType m_type;
	PosixIpcProcessManager& m_manager;
};

#endif // UNIX_PROCESS_H
