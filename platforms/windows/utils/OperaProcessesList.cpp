/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2013 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
*/
#include "core/pch.h"
#include "platforms/windows/utils/OperaProcessesList.h"
#include "platforms/windows/user_fun.h"


#define NAME_OF_PROCESSES_LIST_GUARD UNI_L("OperaProcessesListGuard")
#define NAME_OF_PROCESSES_LIST UNI_L("OperaProcessesList")
#define SIZE_OF_PROCESSES_LIST 4096 /*This buff size allows to write equal or more then 15 instances of opera*/
#define WAIT_ONE_SECOND 1000 /*This should be much more then enough even if slow computer(if not, problably something is wrong)*/
#define RETURN_IF_WAIT_FAILED(val) do { if (WaitForSingleObject(val, WAIT_ONE_SECOND) != WAIT_OBJECT_0) return OpStatus::ERR; }while(0) //

OperaProcessesList* OperaProcessesList::s_instance = NULL;
#ifdef OPERA_MAPI
bool OperaProcessesList::s_read_only = true;
#else
bool OperaProcessesList::s_read_only = false;
#endif

OperaProcessesList* OperaProcessesList::GetInstance()
{
	if (!s_instance)
	{
		s_instance = OP_NEW(OperaProcessesList,());
		if (s_instance && OpStatus::IsError(s_instance->Init()))
			s_instance->Destroy();
	}
	return s_instance;
}

OperaProcessesList::~OperaProcessesList()
{
	if (!s_read_only && m_shared_memory && m_shared_memory_guard)
		OpStatus::Ignore(RemoveThisProcessFromList());
	if (m_shared_memory)
		CloseHandle(m_shared_memory);
	if (m_shared_memory_guard)
		CloseHandle(m_shared_memory_guard);
}

void OperaProcessesList::Destroy()
{
	OP_DELETE(s_instance);
	s_instance = NULL;
}

OP_STATUS OperaProcessesList::Init()
{
	if (!(m_shared_memory = OpenFileMapping(s_read_only ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS, FALSE, NAME_OF_PROCESSES_LIST)) && !s_read_only)
		m_shared_memory = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SIZE_OF_PROCESSES_LIST, NAME_OF_PROCESSES_LIST);
	if (!m_shared_memory)
		return s_read_only ? OpStatus::ERR : OpStatus::ERR_NO_MEMORY;
	m_shared_memory_guard = ::OpenMutex(READ_CONTROL|SYNCHRONIZE|MUTEX_MODIFY_STATE, FALSE, NAME_OF_PROCESSES_LIST_GUARD);
	if (!m_shared_memory_guard && !s_read_only)
		m_shared_memory_guard = ::CreateMutex(NULL, FALSE, NAME_OF_PROCESSES_LIST_GUARD);
	if (!m_shared_memory_guard)
		return s_read_only ? OpStatus::ERR : OpStatus::ERR_NO_MEMORY;
	if (!s_read_only)
		RETURN_IF_ERROR(AddThisProcessToList());
	return OpStatus::OK;
}

OP_STATUS OperaProcessesList::AddThisProcessToList()
{
#if (!defined(PLUGIN_WRAPPER) && !defined(OPERA_MAPI))
	OpString line;
	RETURN_IF_ERROR(GetExePath(line));

	RemoveClosedProcesses();
	DWORD current_process = GetCurrentProcessId();
	RETURN_IF_ERROR(line.AppendFormat("?%d|",current_process));
	RETURN_IF_WAIT_FAILED(m_shared_memory_guard);
	OpAutoReleaseMUTEX release_mutex(m_shared_memory_guard);

	uni_char* view;
	if (!(view = reinterpret_cast<uni_char*>(MapViewOfFile(m_shared_memory, FILE_MAP_ALL_ACCESS, 0, 0, SIZE_OF_PROCESSES_LIST))))
		return OpStatus::ERR;
	OpAutoUnmapViewOfFile unmap_view(view);
	if (sizeof(uni_char)*(uni_strlen(view) + line.Length() + 1) <= SIZE_OF_PROCESSES_LIST)
		op_memcpy(view + uni_strlen(view) , line.CStr(), sizeof(uni_char)*(line.Length() + 1));
#endif //(!defined(PLUGIN_WRAPPER) && !defined(OPERA_MAPI))
	return OpStatus::OK;
}

OP_STATUS OperaProcessesList::RemoveThisProcessFromList()
{
#if (!defined(PLUGIN_WRAPPER) && !defined(OPERA_MAPI))
	OpString path;
	RETURN_IF_ERROR(GetExePath(path));
	RETURN_IF_ERROR(RemoveProcessFromList(path, GetCurrentProcessId()));
#endif //(!defined(PLUGIN_WRAPPER) && !defined(OPERA_MAPI))
	return OpStatus::OK;
}

OP_STATUS OperaProcessesList::RemoveProcessFromList(OpStringC& path, INT32 id)
{
	OpString line;
	RETURN_IF_ERROR(line.Set(path.CStr()));
	RETURN_IF_ERROR(line.AppendFormat("?%d|",id));

	RETURN_IF_WAIT_FAILED(m_shared_memory_guard);
	OpAutoReleaseMUTEX release_mutex(m_shared_memory_guard);

	uni_char* view;
	if (!(view = reinterpret_cast<uni_char*>(MapViewOfFile(m_shared_memory, FILE_MAP_ALL_ACCESS, 0, 0, SIZE_OF_PROCESSES_LIST))))
		return OpStatus::ERR;
	OpAutoUnmapViewOfFile unmap_view(view);

	if (static_cast<int>(uni_strlen(view)) >= line.Length())
	{
		const uni_char* pos = uni_stristr(view, line.CStr());
		if (pos)
			op_memmove(const_cast<uni_char*>(pos), pos + line.Length(), sizeof(uni_char)*(uni_strlen(pos + line.Length()) + 1));
	}
	return OpStatus::OK;
}

OP_STATUS OperaProcessesList::RemoveClosedProcesses()
{
	if (!m_shared_memory || !m_shared_memory_guard)
		return OpStatus::ERR;

	OpString list;
	{
		RETURN_IF_WAIT_FAILED(m_shared_memory_guard);
		OpAutoReleaseMUTEX release_mutex(m_shared_memory_guard);

		uni_char* view;
		if (!(view = reinterpret_cast<uni_char*>(MapViewOfFile(m_shared_memory, FILE_MAP_READ, 0, 0, SIZE_OF_PROCESSES_LIST))))
			return OpStatus::ERR;
		OpAutoUnmapViewOfFile unmap_view(view);
		//view has to be copied because shared memory cannot be mapped
		RETURN_IF_ERROR(list.Set(view));
	}

	int begin_path = 0;
	int end_path = 0;
	int end_proc_id = 0;
	while ((end_path = list.Find(UNI_L("?"), begin_path)) != KNotFound && (end_proc_id = list.Find(UNI_L("|"), end_path)) != KNotFound)
	{
		unsigned long id = uni_strtoul(list.CStr() + end_path + 1, NULL, 10);
		OpAutoHANDLE handle(OpenProcess( PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, id));
		if (!handle.get() || WaitForSingleObject(handle.get(), 0) == WAIT_OBJECT_0)
		{
			OpString path;
			RETURN_IF_ERROR(path.Set(list.CStr() + begin_path, end_path - begin_path));
			RemoveProcessFromList(path, id);
		}
		begin_path = end_proc_id + 1;
	}

	return OpStatus::OK;
}

OP_STATUS OperaProcessesList::GetOperaProcessIds(OpStringC& opera_path, OpINT32Vector& ids)
{
	ids.Clear();
	if (!m_shared_memory || !m_shared_memory_guard)
		return OpStatus::ERR;
	RemoveClosedProcesses();
	RETURN_IF_WAIT_FAILED(m_shared_memory_guard);
	OpAutoReleaseMUTEX release_mutex(m_shared_memory_guard);

	uni_char* view;
	if (!(view = reinterpret_cast<uni_char*>(MapViewOfFile(m_shared_memory, FILE_MAP_READ, 0, 0, SIZE_OF_PROCESSES_LIST))))
		return OpStatus::ERR;
	OpAutoUnmapViewOfFile unmap_view(view);

	const uni_char* begin = view;
	const uni_char* end = 0;
	while ((begin = uni_stristr(begin, opera_path.CStr())) != NULL)
	{
		begin += opera_path.Length();
		if (*begin != UNI_L("?")[0])
			continue;
		begin += 1;
		if ((end = uni_strchr(begin, '|')) == NULL)
			continue;
		unsigned long id = uni_strtoul(begin, NULL, 10);
		RETURN_IF_ERROR(ids.Add(id));
		begin = end + 1;
	}
	return OpStatus::OK;
}
