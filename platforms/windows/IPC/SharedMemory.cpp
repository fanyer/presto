/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/windows/IPC/SharedMemory.h"
#include "modules/hardcore/component/Messages.h"
/*static*/
OpAutoPtr<WindowsSharedMemoryManager> WindowsSharedMemoryManager::s_shared_memory_manager;
DWORD WindowsSharedMemoryManager::cm_0_pid = 0;

/*static*/
WindowsSharedMemoryManager* WindowsSharedMemoryManager::GetInstance()
{
	if (s_shared_memory_manager.get() == NULL)
		s_shared_memory_manager = OP_NEW(WindowsSharedMemoryManager, ());

	return s_shared_memory_manager.get();
}

WindowsSharedMemoryManager::WindowsSharedMemoryManager()
{
	my_sm = OP_NEW(WindowsSharedMemory, ());
	if (OpStatus::IsError(my_sm->Construct(g_component_manager->GetAddress().cm, TRUE)))
	{
		OP_DELETE(my_sm);
		my_sm = NULL;
	}
}

WindowsSharedMemoryManager::~WindowsSharedMemoryManager()
{
	OP_DELETE(my_sm);
	opened_sm.DeleteAll();
}

OP_STATUS WindowsSharedMemoryManager::OpenMemory(WindowsSharedMemory *&sm, OpMessageAddress cm_address)
{
	INT32 cm = cm_address.cm;

	if (opened_sm.Contains(cm))
		return opened_sm.GetData(cm, &sm);

	OpAutoPtr<WindowsSharedMemory> new_sm = OP_NEW(WindowsSharedMemory, ());

	RETURN_IF_ERROR(new_sm->Construct(cm, FALSE));

	RETURN_IF_ERROR(opened_sm.Add(cm, new_sm.get()));

	sm = new_sm.release();

	return OpStatus::OK;
}

OP_STATUS WindowsSharedMemoryManager::OpenSharedMemory(WindowsSharedMemory *&sm, OpMessageAddress cm_address)
{
	INT32 cm = cm_address.cm;

	OpAutoPtr<WindowsSharedMemory> new_sm = OP_NEW(WindowsSharedMemory, ());

	RETURN_IF_ERROR(new_sm->Construct(cm, FALSE));

	sm = new_sm.release();
	return OpStatus::OK;
}

void WindowsSharedMemoryManager::CloseMemory(OpMessageAddress cm_address)
{
	INT32 cm = cm_address.cm;

	if (!opened_sm.Contains(cm))
		return;

	WindowsSharedMemory* sm;

	if (OpStatus::IsSuccess(opened_sm.Remove(cm, &sm)))
		OP_DELETE(sm);
}

WindowsSharedMemory::WindowsSharedMemory()
	: m_file_mapping (NULL)
	, m_shared_mem (NULL)
	, m_not_full_event (NULL)
	, m_not_empty_event (NULL)
	, m_write_mutex (NULL)
	, m_write_mutex_wait (NULL)
	, m_disconnected (FALSE)
	, m_destructing (FALSE)
{
	InitializeCriticalSection(&m_messages_access);
	InitializeCriticalSection(&m_destructor_access);
}

WindowsSharedMemory::~WindowsSharedMemory()
{
	EnterCriticalSection(&m_destructor_access);
	m_destructing = TRUE;
	LeaveCriticalSection(&m_destructor_access);

	if (m_write_mutex_wait)
		UnregisterWaitEx(m_write_mutex_wait, INVALID_HANDLE_VALUE);

	CloseHandle(m_write_mutex);

	if (m_shared_mem)
	{
		if (m_is_owner)
			m_shared_mem->ready = FALSE;
		UnmapViewOfFile(m_shared_mem);
	}

	DeleteCriticalSection(&m_messages_access);
	DeleteCriticalSection(&m_destructor_access);

	CloseHandle(m_not_full_event);
	CloseHandle(m_not_empty_event);

	CloseHandle(m_file_mapping);
}

OP_STATUS WindowsSharedMemory::Construct(INT32 cm, BOOL is_owner)
{
	UniString name;

	if (WindowsSharedMemoryManager::GetCM0PID() == 0)
		return OpStatus::ERR;

	RETURN_IF_ERROR(name.AppendFormat(UNI_L("Opera_%d_%d"), WindowsSharedMemoryManager::GetCM0PID(), cm));

	//julienp:We need to provide our own string pointer here, instead of using the one provided by CreatePtr, otherwise CreateFileMapping fails with ERROR_NOACCESS (998)
	OpAutoArray<uni_char> name_ptr(OP_NEWA(uni_char, name.Length() + 1));
	name.CopyInto(name_ptr.get(), name.Length());
	name_ptr[name.Length()] = 0;

	m_cm = cm;
	m_is_owner = is_owner;

	if (m_is_owner)
	{
		if ((m_file_mapping = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(CircularBuffer), name_ptr.get())) == NULL)
			return OpStatus::ERR;
	}
	else
	{
		if ((m_file_mapping = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE,name_ptr.get())) == NULL)
			return OpStatus::ERR;
	}

	if ((m_shared_mem = (CircularBuffer*)MapViewOfFile(m_file_mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0)) == NULL)
		return OpStatus::ERR;

	if (m_is_owner)
	{
		//See http://msdn.microsoft.com/en-us/library/aa366537(v=vs.85).aspx
		//"The initial contents of the pages in a file mapping object backed by the operating system paging file are 0 (zero)."
		//So, we don't need to initialize the offsets and ready is FALSE until we are ready.

		m_shared_mem->owner_pid = GetCurrentProcessId();
		m_shared_mem->owner_not_full_event = CreateEvent(NULL, FALSE, TRUE, NULL);
		m_shared_mem->owner_not_empty_event = CreateEvent(NULL, FALSE, FALSE, NULL);

		//julienp:	We are going to use an event as mutex here, because mutexes do not play nice with RegisterWaitForSingleObject.
		//			and we do not need the thread-affinity properties of a mutex anyway. We only need something saying:
		//			"No one is writing to the shared mem right now".
		//			The issue with RegisterWaitForSingleObject (not documented in MSDN at this time) is that it creates a thread
		//			For waiting on the HANDLE passed to it, which then acquires ownership of the mutex and spawns another thread
		//			to execute its callback, then returns to waiting. It then never releases the mutex ownership and (since it
		//			owns the mutex), will simply return from the wait immediately and run the callback over and over again.
		m_shared_mem->owner_write_mutex = CreateEvent(NULL, FALSE, TRUE, NULL);

		if (m_shared_mem->owner_not_full_event == NULL || m_shared_mem->owner_not_empty_event == NULL || m_shared_mem->owner_write_mutex == NULL)
		{
			CloseHandle(m_shared_mem->owner_not_full_event);
			CloseHandle(m_shared_mem->owner_not_empty_event);
			CloseHandle(m_shared_mem->owner_write_mutex);
			return OpStatus::ERR;
		}

		m_not_full_event = m_shared_mem->owner_not_full_event;
		m_not_empty_event = m_shared_mem->owner_not_empty_event;
		m_write_mutex = m_shared_mem->owner_write_mutex;

		m_shared_mem->ready = TRUE;
	}
	else
	{
		if (!m_shared_mem->ready)
			return OpStatus::ERR;

		OpAutoHANDLE owner_process(OpenProcess(PROCESS_DUP_HANDLE, FALSE, m_shared_mem->owner_pid));
		if (owner_process.get() == NULL)
			return OpStatus::ERR;

		if (!DuplicateHandle(owner_process, m_shared_mem->owner_not_full_event, GetCurrentProcess(), &m_not_full_event, 0, FALSE, DUPLICATE_SAME_ACCESS))
			return OpStatus::ERR;

		if (!DuplicateHandle(owner_process, m_shared_mem->owner_not_empty_event, GetCurrentProcess(), &m_not_empty_event, 0, FALSE, DUPLICATE_SAME_ACCESS))
			return OpStatus::ERR;

		if (!DuplicateHandle(owner_process, m_shared_mem->owner_write_mutex, GetCurrentProcess(), &m_write_mutex, 0, FALSE, DUPLICATE_SAME_ACCESS))
			return OpStatus::ERR;
	}

	return OpStatus::OK;
}

/*static*/
VOID WindowsSharedMemory::WriteReadyCallback(PVOID shared_mem, BOOLEAN timeout)
{
	WindowsSharedMemory* sm = static_cast<WindowsSharedMemory*>(shared_mem);
	EnterCriticalSection(&sm->m_destructor_access);

	if (!sm->m_destructing)
		sm->WriteReady();
	SetEvent(sm->m_write_mutex);

	LeaveCriticalSection(&sm->m_destructor_access);

}

UINT64 WindowsSharedMemory::AvailableWrite()
{
	UINT64 read_offset = m_shared_mem->read_offset;
	UINT64 write_offset = m_shared_mem->write_offset;
	return (write_offset < read_offset)?(read_offset - write_offset - 1):(sizeof(m_shared_mem->buffer) - write_offset + read_offset - 1);
}

void WindowsSharedMemory::WriteReady()
{
	if (!m_shared_mem->ready)
	{
		m_disconnected = TRUE;
		return;
	}

	EnterCriticalSection(&m_messages_access);

	size_t length = m_messages.Length();
	OpAutoArray<char> messages = OP_NEWA(char, length);
	if (!messages.get())
	{
		LeaveCriticalSection(&m_messages_access);
		return;
	}

	m_messages.CopyInto(messages.get(), length);
	LeaveCriticalSection(&m_messages_access);

	char* messages_offset = messages.get();
	unsigned written = 0;

	while (length)
	{
		UINT64 available = AvailableWrite();

		if (available == 0)
		{
			if (WaitForSingleObject(m_not_full_event, INFINITE) != WAIT_OBJECT_0)
			{
				m_disconnected = TRUE;
				return;
			}

			available = AvailableWrite();
		}

		if (length < available)
			available = length;

		if (m_shared_mem->write_offset + available > sizeof(m_shared_mem->buffer))
		{
			memcpy(m_shared_mem->buffer + m_shared_mem->write_offset, messages_offset, sizeof(m_shared_mem->buffer) - m_shared_mem->write_offset);
			memcpy(m_shared_mem->buffer, messages_offset + sizeof(m_shared_mem->buffer) - m_shared_mem->write_offset, available + m_shared_mem->write_offset - sizeof(m_shared_mem->buffer));
			m_shared_mem->write_offset = available + m_shared_mem->write_offset - sizeof(m_shared_mem->buffer);
		}
		else
		{
			memcpy(m_shared_mem->buffer + m_shared_mem->write_offset, messages_offset, available);
			m_shared_mem->write_offset += available;
		}

		length -= available;
		written += available;
		messages_offset += available;

		SetEvent(m_not_empty_event);
	}

	EnterCriticalSection(&m_messages_access);

	m_messages.Consume(written);

	if (m_messages.Length() == 0)
	{
		UnregisterWait(m_write_mutex_wait);
		m_write_mutex_wait = NULL;
	}

	LeaveCriticalSection(&m_messages_access);
}

OP_STATUS WindowsSharedMemory::WriteMessage(OpTypedMessage* msg)
{
	OpAutoPtr<OpTypedMessage> msg_autoptr (msg);

	if (m_is_owner || m_disconnected)
		return OpStatus::ERR;

	OP_ASSERT(msg);
	OpAutoPtr<OpSerializedMessage> smsg (OpTypedMessage::Serialize(msg));

	if (!smsg.get())
		return OpStatus::ERR_NO_MEMORY;

	UINT64 smsg_len = smsg->data.Length();
	OpData payload;
	RETURN_IF_ERROR(payload.SetCopyData(reinterpret_cast<const char*>(&smsg_len), sizeof(smsg_len)));
	RETURN_IF_ERROR(payload.Append(smsg->data));

//Uncomment this to see the binary dump of what is written to the shared memory (You'll need to hook up a console)
/*#ifdef _DEBUG
	UniString debug_message;
	debug_message.SetCopyData(UNI_L("Writing:"));
	for (UINT i = 0; i < smsg->data.Length(); i++)
		debug_message.AppendFormat(UNI_L(" %.2X"), (unsigned char)smsg->data[i]);
	debug_message.AppendCopyData(UNI_L("\n"));

	const uni_char* debug_message_ptr;
	debug_message.CreatePtr(&debug_message_ptr, true);
	printf("%S", debug_message_ptr);
#endif*/
	EnterCriticalSection(&m_messages_access);

	OP_STATUS err = m_messages.Append(payload);
	if (OpStatus::IsSuccess(err) && m_write_mutex_wait == NULL)
		RegisterWaitForSingleObject(&m_write_mutex_wait, m_write_mutex, WindowsSharedMemory::WriteReadyCallback, this, INFINITE, WT_EXECUTEDEFAULT);

	LeaveCriticalSection(&m_messages_access);
	return err;

}


/** IMPORTANT NOTE **/
// The implementation of this method was made in such a way that OOM errors occuring
// during it won't cause the shared memory to be in an inconsistent state or messages
// to be dropped, so that such errors can be safey ignored. Make sure to keep these
// premisses in mind if you are attempting to modify it.
OP_STATUS WindowsSharedMemory::ReadMessages()
{
	if (!m_is_owner)
		return OpStatus::ERR_NO_ACCESS;

	if (!m_shared_mem->ready)
		return OpStatus::ERR;

	ptrdiff_t write_offset = m_shared_mem->write_offset;
	ptrdiff_t read_offset = m_shared_mem->read_offset;
	unsigned available = (read_offset <= write_offset)?(write_offset - read_offset):(write_offset + sizeof(m_shared_mem->buffer) - read_offset);

	if (available == 0)
		return OpStatus::OK;

	char* buf = m_messages.GetAppendPtr(available);
	if (!buf)
	{
		SetEvent(m_not_empty_event);		//So we retry reading later
		return OpStatus::ERR_NO_MEMORY;
	}

	if (read_offset + available > sizeof(m_shared_mem->buffer))
	{
		op_memcpy(buf, m_shared_mem->buffer + read_offset, sizeof(m_shared_mem->buffer) - read_offset);
		op_memcpy(buf + sizeof(m_shared_mem->buffer) - read_offset, m_shared_mem->buffer, available + read_offset - sizeof(m_shared_mem->buffer));
		read_offset = available + read_offset - sizeof(m_shared_mem->buffer);
	}
	else
	{
		op_memcpy(buf, m_shared_mem->buffer + read_offset, available);
		read_offset += available;
	}
	m_shared_mem->read_offset = read_offset;
	SetEvent(m_not_full_event);


	const char* messages_ptr;
	UINT64 message_size;
	size_t length;

	while ((length = m_messages.Length()) != 0)
	{
		RETURN_IF_ERROR(m_messages.CreatePtr(&messages_ptr));

		op_memcpy(&message_size, messages_ptr, sizeof(message_size));

		if (length - sizeof(message_size) < message_size)
			break;

		OpData payload(m_messages, sizeof(message_size), message_size);

//Uncomment this to see the binary dump of what is read from the shared memory (You'll need to hook up a console)
/*#ifdef _DEBUG
	UniString debug_message;
	debug_message.SetCopyData(UNI_L("Reading:"));
	for (UINT i = 0; i < payload.Length(); i++)
		debug_message.AppendFormat(UNI_L(" %.2X"), (unsigned char)payload[i]);
	debug_message.AppendCopyData(UNI_L("\n"));

	const uni_char* debug_message_ptr;
	debug_message.CreatePtr(&debug_message_ptr, true);
	printf("%S", debug_message_ptr);
#endif*/

		OpSerializedMessage smsg(payload);
		OpTypedMessage* msg = OpSerializedMessage::Deserialize(&smsg);
		RETURN_OOM_IF_NULL(msg);
		//If this somehow fails, we should still finish delivering the messages we have buffered.
		OpStatus::Ignore(g_component_manager->DeliverMessage(msg));

		m_messages.Consume(sizeof(message_size) + message_size);
	}

	return OpStatus::OK;
}
