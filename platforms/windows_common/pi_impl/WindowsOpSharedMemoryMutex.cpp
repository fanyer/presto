/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#include "platforms/windows_common/pi_impl/WindowsOpSharedMemoryMutex.h"

WindowsOpSharedMemoryMutex::CurrentHolder::CurrentHolder() :
	m_holdingProcess(0),
	m_holdingThread(0)
{
}

void WindowsOpSharedMemoryMutex::CurrentHolder::reset()
{
	m_holdingProcess = m_holdingThread = 0;
}

void WindowsOpSharedMemoryMutex::CurrentHolder::setToMe()
{
	m_holdingProcess = GetCurrentProcessId();
	m_holdingThread = GetCurrentThreadId();
}

bool WindowsOpSharedMemoryMutex::CurrentHolder::isMe() const
{
	return m_holdingProcess == GetCurrentProcessId() &&
			m_holdingThread == GetCurrentThreadId();
}

WindowsOpSharedMemoryMutex::WindowsOpSharedMemoryMutex() :
	m_originalProcessId(0),
	m_originalSemaphoreHandle(NULL),
	m_currentSemaphoreHandle(NULL)
{
}

OP_STATUS WindowsOpSharedMemoryMutex::Construct()
{
	m_originalProcessId = GetCurrentProcessId();
	m_originalSemaphoreHandle = CreateSemaphore(
		NULL, // security attributes (NULL = default)
		1, // initial count
		1, // max count
		NULL // unnamed object
		);
	return m_originalSemaphoreHandle ? OpStatus::OK : OpStatus::ERR;
}

HANDLE WindowsOpSharedMemoryMutex::GetSemaphoreHandleForThisProcess()
{
	if(GetCurrentProcessId() == m_originalProcessId)
		return m_originalSemaphoreHandle;
	else
	{
		/* If we're not the process that created the original handle,
		we have to duplicate it. */
		HANDLE originalProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, m_originalProcessId);
		if(!originalProcess)
			return NULL;
		HANDLE newSemaphoreHandle = NULL;
		BOOL success = DuplicateHandle(
			originalProcess,
			m_originalSemaphoreHandle,
			GetCurrentProcess(),
			&newSemaphoreHandle,
			NULL, // access flags ignored due to DUPLICATE_SAME_ACCESS
			FALSE, // don't inherit handle in child processes
			DUPLICATE_SAME_ACCESS);
		CloseHandle(originalProcess);
		return success ? newSemaphoreHandle : NULL;
	}
}

WindowsOpSharedMemoryMutex::~WindowsOpSharedMemoryMutex()
{
	if(GetCurrentProcessId() == m_originalProcessId)
		CloseHandle(m_originalSemaphoreHandle);
	else
	{
		/* If we're not the process that created the original handle,
		we have to duplicate it and then close the source handle through
		DuplicateHandle with DUPLICATE_CLOSE_SOURCE parameter. */
		HANDLE ourSemaphoreHandle = GetSemaphoreHandleForThisProcess();
		if(ourSemaphoreHandle)
		{
			// Close source handle
			DuplicateHandle(
				GetCurrentProcess(),
				ourSemaphoreHandle,
				NULL,
				NULL,
				NULL,
				FALSE,
				DUPLICATE_CLOSE_SOURCE);
			CloseHandle(ourSemaphoreHandle);
		}
	}
}

OP_STATUS WindowsOpSharedMemoryMutex::AcquireWithTimeout(DWORD time)
{
	if(m_currentHolder.isMe()) return OpStatus::ERR;
	// Get a handle to the semaphore that's valid in this process:
	HANDLE semaphoreHandle = GetSemaphoreHandleForThisProcess();
	if(!semaphoreHandle) return OpStatus::ERR;
	DWORD waitResult = WaitForSingleObject(
		semaphoreHandle,
		time);
	if(waitResult == WAIT_OBJECT_0)
	{
		/* We have a lock. Now, set m_currentSemaphoreHandle
		to the handle we've waited on. This will be
		needed for Release(). */
		m_currentSemaphoreHandle = semaphoreHandle;
		m_currentHolder.setToMe();
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

OP_STATUS WindowsOpSharedMemoryMutex::Acquire()
{
	return AcquireWithTimeout(INFINITE);
}

bool WindowsOpSharedMemoryMutex::TryAcquire()
{
	return AcquireWithTimeout(0) == OpStatus::OK;
}

OP_STATUS WindowsOpSharedMemoryMutex::Release()
{
	/* m_currentSemaphoreHandle should have been set by Acquire or TryAcquire. */
	if(!m_currentSemaphoreHandle)
		return OpStatus::ERR;

	m_currentHolder.reset();
	HANDLE handle = m_currentSemaphoreHandle;
	m_currentSemaphoreHandle = NULL;
	WINBOOL success = ReleaseSemaphore(handle, 1, NULL);
	if(success && m_originalSemaphoreHandle != handle)
		/* It's time to close this duplicate. */
		success = CloseHandle(handle);
	else if(!success)
	{
		// Roll back, we still hold the lock
		m_currentSemaphoreHandle = handle;
		m_currentHolder.setToMe();
	}
	return success ? OpStatus::OK : OpStatus::ERR;
}
