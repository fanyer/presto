/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/hardcore/component/Messages.h"

#include "platforms/windows/IPC/SharedPluginBitmap.h"
#include "platforms/windows/IPC/SharedMemory.h"

/*static*/
OpAutoPtr<WindowsSharedBitmapManager> WindowsSharedBitmapManager::s_shared_bitmap_manager;

/*static*/
WindowsSharedBitmapManager* WindowsSharedBitmapManager::GetInstance()
{
	if (WindowsSharedMemoryManager::GetCM0PID() == 0)
		return NULL;

	if (!s_shared_bitmap_manager.get())
		s_shared_bitmap_manager = OP_NEW(WindowsSharedBitmapManager, ());

	return s_shared_bitmap_manager.get();
}

WindowsSharedBitmap* WindowsSharedBitmapManager::CreateMemory(OpPluginImageID identifier, int width, int height)
{
	OP_ASSERT(!m_shared_bitmaps.Contains(identifier));

	OpAutoPtr<WindowsSharedBitmap> wsb = OP_NEW(WindowsSharedBitmap, ());
	RETURN_VALUE_IF_NULL(wsb.get(), NULL);

	if (OpStatus::IsError(wsb->Init(identifier, true))
		|| OpStatus::IsError(wsb->ConstructMemory(width, height))
		|| OpStatus::IsError(m_shared_bitmaps.Add(identifier, wsb.get())))
		return NULL;

	return wsb.release();
}

WindowsSharedBitmap* WindowsSharedBitmapManager::OpenMemory(OpPluginImageID identifier)
{
	if (m_shared_bitmaps.Contains(identifier))
	{
		WindowsSharedBitmap* bitmap = NULL;
		m_shared_bitmaps.GetData(identifier, &bitmap);
		return bitmap;
	}

	OpAutoPtr<WindowsSharedBitmap> wsb = OP_NEW(WindowsSharedBitmap, ());
	RETURN_VALUE_IF_NULL(wsb.get(), NULL);

	if (OpStatus::IsError(wsb->Init(identifier, false)) || OpStatus::IsError(wsb->OpenMemory())
		|| OpStatus::IsError(m_shared_bitmaps.Add(identifier, wsb.get())))
		return NULL;

	return wsb.release();
}

void WindowsSharedBitmapManager::CloseMemory(OpPluginImageID identifier)
{
	if (!m_shared_bitmaps.Contains(identifier))
		return;

	WindowsSharedBitmap* sm;
	if (OpStatus::IsSuccess(m_shared_bitmaps.Remove(identifier, &sm)))
		OP_DELETE(sm);
}

WindowsSharedBitmap::WindowsSharedBitmap()
	: m_close_wait_handle(NULL),
	  m_close_callback_handle(NULL),
	  m_opened_remotely_handle(NULL),
	  m_mapped_memory(NULL),
	  m_mapping_handle(NULL),
	  m_shared_mem(NULL),
	  m_opened(false)
{
}

WindowsSharedBitmap::~WindowsSharedBitmap()
{
	if (m_close_callback_handle)
		UnregisterWait(m_close_callback_handle);

	if (m_opened)
		m_shared_mem->opened = 0;

	if (m_mapped_memory)
		UnmapViewOfFile(m_mapped_memory);

	if (m_mapping_handle)
		CloseHandle(m_mapping_handle);

	if (m_close_wait_handle)
	{
		ResetEvent(m_close_wait_handle);
		CloseHandle(m_close_wait_handle);
	}

	if (m_opened_remotely_handle)
	{
		SetEvent(m_opened_remotely_handle);
		CloseHandle(m_opened_remotely_handle);
	}
}

OP_STATUS WindowsSharedBitmap::Init(OpPluginImageID identifier, bool owner)
{
	UniString name;
	RETURN_IF_ERROR(name.AppendFormat(UNI_L("OperaPluginBitmap_%d_%llu"), WindowsSharedMemoryManager::GetCM0PID(), identifier));
	m_memory_name = OP_NEWA(uni_char, name.Length() + 1);
	RETURN_OOM_IF_NULL(m_memory_name.get());
	name.CopyInto(m_memory_name.get(), name.Length());
	m_memory_name.get()[name.Length()] = 0;

	m_identifier = identifier;
	m_owner = owner;

	return OpStatus::OK;
}

OP_STATUS WindowsSharedBitmap::ConstructMemory(int width, int height)
{
	// Size consists of the header + memory for the bitmap. */
	DWORD size = sizeof(SharedBitmap) + width * height * sizeof(UINT32);

	RETURN_OOM_IF_NULL(m_mapping_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, m_memory_name.get()));
	RETURN_OOM_IF_NULL(m_mapped_memory = MapViewOfFile(m_mapping_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0));

	m_shared_mem = static_cast<SharedBitmap*>(m_mapped_memory);
	m_shared_mem->width = width;
	m_shared_mem->height = height;
	m_shared_mem->owner_pid = GetCurrentProcessId();
	RETURN_OOM_IF_NULL(m_shared_mem->close_wait_event = m_close_wait_handle = CreateEvent(NULL, FALSE, FALSE, NULL));
	/* Create event used to block owner when waiting for remote user. Create in signaled
	   state - won't block owner unless someone opened this memory and changed state. */
	RETURN_OOM_IF_NULL(m_shared_mem->opened_remotely_event = m_opened_remotely_handle = CreateEvent(NULL, FALSE, TRUE, NULL));

	return OpStatus::OK;
}

OP_STATUS WindowsSharedBitmap::OpenMemory()
{
	OP_ASSERT(!m_opened && "Don't call OpenMemory on already open memory!");
	RETURN_OOM_IF_NULL(m_mapping_handle = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, m_memory_name.get()));
	RETURN_OOM_IF_NULL(m_mapped_memory = MapViewOfFile(m_mapping_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0));

	m_shared_mem = static_cast<SharedBitmap*>(m_mapped_memory);

	// Duplicate event handles that owner has created.
	OpAutoHANDLE owner_process(OpenProcess(PROCESS_DUP_HANDLE, FALSE, m_shared_mem->owner_pid));
	RETURN_OOM_IF_NULL(owner_process.get());

	if (!DuplicateHandle(owner_process, m_shared_mem->close_wait_event, GetCurrentProcess(), &m_close_wait_handle, 0, FALSE, DUPLICATE_SAME_ACCESS))
		return OpStatus::ERR;

	if (!DuplicateHandle(owner_process, m_shared_mem->opened_remotely_event, GetCurrentProcess(), &m_opened_remotely_handle, 0, FALSE, DUPLICATE_SAME_ACCESS))
		return OpStatus::ERR;

	// Set to nonsignaled state to let owner know that it should wait when closing memory.
	ResetEvent(m_opened_remotely_handle);

	if (!RegisterWaitForSingleObject(&m_close_callback_handle, m_close_wait_handle, WindowsSharedBitmap::CloseCallback, reinterpret_cast<void*>(m_identifier), INFINITE, WT_EXECUTEDEFAULT))
		return OpStatus::ERR;

	if (InterlockedCompareExchange(&m_shared_mem->opened, TRUE, FALSE))
	{
		//Reaching this means that multiple concurrent accesses to the shared memory for reading are attempted.
		//If this is needed, then some assumptions don't hold anymore and the implementation needs
		//to be changed. F.ex: m_close_wait_handle needs to be implemented using a semaphore instead.
		OP_ASSERT(false);
		return OpStatus::ERR;
	}
	m_opened = true;

	return OpStatus::OK;
}

OP_STATUS WindowsSharedBitmap::ResizeMemory(int width, int height)
{
	OP_ASSERT(m_owner && "Only owner should resize shared memory!");
	// Signal an event and wait for remote users to close this memory.
	SignalObjectAndWait(m_close_wait_handle, m_opened_remotely_handle, INFINITE, FALSE);
	// Reset event to non-signaled state.
	ResetEvent(m_opened_remotely_handle);

	if (m_mapped_memory)
	{
		UnmapViewOfFile(m_mapped_memory);
		m_mapped_memory = NULL;
		m_shared_mem = NULL;
	}

	if (m_mapping_handle)
	{
		CloseHandle(m_mapping_handle);
		m_mapping_handle = NULL;
	}

	// Size consists of the header + memory for the bitmap. */
	DWORD size = sizeof(SharedBitmap) + width * height * sizeof(UINT32);

	RETURN_OOM_IF_NULL(m_mapping_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, m_memory_name.get()));
	RETURN_OOM_IF_NULL(m_mapped_memory = MapViewOfFile(m_mapping_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0));

	m_shared_mem = static_cast<SharedBitmap*>(m_mapped_memory);
	m_shared_mem->width = width;
	m_shared_mem->height = height;
	m_shared_mem->owner_pid = GetCurrentProcessId();
	m_shared_mem->close_wait_event = m_close_wait_handle;
	m_shared_mem->opened_remotely_event = m_opened_remotely_handle;

	return OpStatus::OK;
}

/* static */
VOID WindowsSharedBitmap::CloseCallback(PVOID identifier, BOOLEAN timeout)
{
	if (WindowsSharedBitmapManager* bitmap_manager = WindowsSharedBitmapManager::GetInstance())
		bitmap_manager->CloseMemory(reinterpret_cast<OpPluginImageID>(identifier));
}