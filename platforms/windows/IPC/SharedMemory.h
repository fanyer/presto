/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWS_SHARED_MEMORY_H
#define WINDOWS_SHARED_MEMORY_H

class WindowsSharedMemory;

class WindowsSharedMemoryManager
{
public:
	~WindowsSharedMemoryManager();

	static WindowsSharedMemoryManager* GetInstance();

	WindowsSharedMemory* GetMemory() {return my_sm;};
	OP_STATUS OpenMemory(WindowsSharedMemory *&sm, OpMessageAddress cm_address);
	void CloseMemory(OpMessageAddress cm_address);

	static void SetCM0PID(DWORD cm_0_pid) {WindowsSharedMemoryManager::cm_0_pid = cm_0_pid;}
	static DWORD GetCM0PID() { return cm_0_pid; }
	//Use this method only when you comunicate with an opera process from a dll in another process.
	static OP_STATUS OpenSharedMemory(WindowsSharedMemory *&sm, OpMessageAddress cm_address);

private:

	WindowsSharedMemoryManager();

	OpINT32HashTable<WindowsSharedMemory> opened_sm;

	WindowsSharedMemory* my_sm;

	static OpAutoPtr<WindowsSharedMemoryManager> s_shared_memory_manager;
	static DWORD cm_0_pid;
};

class WindowsSharedMemory
{
public:

	OP_STATUS ReadMessages();
	OP_STATUS WriteMessage(OpTypedMessage* msg);

	HANDLE GetNotEmptyEvent() {return m_not_empty_event;}

	~WindowsSharedMemory();

private:
	friend class WindowsSharedMemoryManager;

	static VOID CALLBACK WriteReadyCallback(PVOID shared_mem, BOOLEAN timeout);
	void WriteReady();
	UINT64 AvailableWrite();

	WindowsSharedMemory();
	OP_STATUS Construct(INT32 cm, BOOL is_owner);

	/* Expanding members to UINT64 is necessary because structure is shared
	   between processes which can run on different architectures. */
	struct CircularBuffer
	{
		volatile UINT64 read_offset;
		volatile UINT64 write_offset;

		union
		{
			HANDLE owner_not_full_event;
			UINT64 padding;
		};
		union
		{
			HANDLE owner_not_empty_event;
			UINT64 padding;
		};
		union
		{
			HANDLE owner_write_mutex;
			UINT64 padding;
		};

		DWORD owner_pid;
		volatile BOOL ready;

		char buffer[8192];
	};

	HANDLE m_not_full_event;
	HANDLE m_not_empty_event;
	HANDLE m_write_mutex;

	HANDLE m_write_mutex_wait;

	HANDLE m_file_mapping;
	CircularBuffer* m_shared_mem;

	OpData m_messages;
	CRITICAL_SECTION m_messages_access;

	BOOL m_destructing;
	CRITICAL_SECTION m_destructor_access;

	INT32 m_cm;
	BOOL m_is_owner;
	BOOL m_disconnected;
};

#endif // WINDOWS_SHARED_MEMORY_H
