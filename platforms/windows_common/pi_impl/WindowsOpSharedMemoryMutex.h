/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef WINGOGI_SHARED_MEMORY_MUTEX_H
#define WINGOGI_SHARED_MEMORY_MUTEX_H

/** Implementation of WindowsOpSharedMemoryMutex PI.
 *
 * Internally uses a semaphore instead of a mutex because
 * Windows Mutexes are natively recursive. The implementation
 * is relatively complex because there's no easy way of sharing
 * an object representing a semaphore or mutex in Windows.
 * Sync primitives are represented by HANDLEs which are not
 * valid in other processes out-of-the-box.
 */
class WindowsOpSharedMemoryMutex
{
public:
	WindowsOpSharedMemoryMutex();
	~WindowsOpSharedMemoryMutex();
	OP_STATUS Construct();
	OP_STATUS Acquire();
	OP_STATUS Release();
	bool TryAcquire();
private:
	HANDLE GetSemaphoreHandleForThisProcess();
	OP_STATUS AcquireWithTimeout(DWORD time);
	DWORD m_originalProcessId;
	HANDLE m_originalSemaphoreHandle;
	HANDLE m_currentSemaphoreHandle;

	class CurrentHolder
	{
	public:
		CurrentHolder();
		void setToMe();
		void reset();
		bool isMe() const;
	private:
		DWORD m_holdingProcess;
		DWORD m_holdingThread;
	} m_currentHolder;
};

typedef WindowsOpSharedMemoryMutex OpSharedMemoryMutex;

#endif // WINGOGI_SHARED_MEMORY_MUTEX_H
