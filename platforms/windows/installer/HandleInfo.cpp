// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Øyvind Østlund
//

#include "core/pch.h"

#include "adjunct/desktop_util/file_utils/filenameutils.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "platforms/windows/win_handy.h"
#include "platforms/windows/installer/HandleInfo.h"
#include "platforms/windows/installer/InstallerWizard.h"
#include "platforms/windows/installer/ProcessItem.h"
#include "platforms/windows/windows_ui/winshell.h"

#include <process.h>
#include <Psapi.h>
#include <Tlhelp32.h>

using namespace NtDll;

extern OpString g_spawnPath;

struct GetFileNameThreadParam
{
	NTSTATUS				rc;
	IO_STATUS_BLOCK			iob;
	HANDLE					hFile;
	DWORD					size;
	FILE_NAME_INFORMATION*	file_name_info;
};

enum {
	OB_TYPE_UNKNOWN = 0,
	OB_TYPE_TYPE = 1,
	OB_TYPE_DIRECTORY,
	OB_TYPE_SYMBOLIC_LINK,
	OB_TYPE_TOKEN,
	OB_TYPE_PROCESS,
	OB_TYPE_THREAD,
	OB_TYPE_UNKNOWN_7,
	OB_TYPE_EVENT,
	OB_TYPE_EVENT_PAIR,
	OB_TYPE_MUTANT,
	OB_TYPE_UNKNOWN_11,
	OB_TYPE_SEMAPHORE,
	OB_TYPE_TIMER,
	OB_TYPE_PROFILE,
	OB_TYPE_WINDOW_STATION,
	OB_TYPE_DESKTOP,
	OB_TYPE_SECTION,
	OB_TYPE_KEY,
	OB_TYPE_PORT,
	OB_TYPE_WAITABLE_PORT,
	OB_TYPE_UNKNOWN_21,
	OB_TYPE_UNKNOWN_22,
	OB_TYPE_UNKNOWN_23,
	OB_TYPE_UNKNOWN_24,
	OB_TYPE_IO_COMPLETION,
	OB_TYPE_FILE                        
} SystemHandleType;

static const uni_char* SystemHandleTypeStr[] = 
		{ 
			UNI_L(""), 
			UNI_L(""), 
			UNI_L("Directory"), 
			UNI_L("SymbolicLink"), 
			UNI_L("Token"),
			UNI_L("Process"), 
			UNI_L("Thread"), 
			UNI_L("Unknown7"), 
			UNI_L("Event"), 
			UNI_L("EventPair"), 
			UNI_L("Mutant"), 
			UNI_L("Unknown11"), 
			UNI_L("Semaphore"), 
			UNI_L("Timer"), 
			UNI_L("Profile"), 
			UNI_L("WindowStation"),
			UNI_L("Desktop"), 
			UNI_L("Section"), 
			UNI_L("Key"), 
			UNI_L("Port"), 
			UNI_L("WaitablePort"), 
			UNI_L("Unknown21"), 
			UNI_L("Unknown22"), 
			UNI_L("Unknown23"), 
			UNI_L("Unknown24"),
			UNI_L("IoCompletion"), 
			UNI_L("File")
		};
	
HandleInfo::HandleInfo()
{

}

HandleInfo::~HandleInfo()
{
	m_process_list.DeleteAll();
	m_window_list.DeleteAll();
}

OP_STATUS HandleInfo::Init()
{
	if (!HASNtQuerySystemInformation() || !HASNtQueryObject() || !HASNtQueryInformationFile() || !HASNtQueryInformationProcess())
		return OpStatus::ERR_NOT_SUPPORTED;

	return  OpStatus::OK;
}

// From device file name to DOS filename
OP_STATUS HandleInfo::GetFsFileName(const uni_char* device_file_name, OpString& fsFileName)
{
	OP_ASSERT(device_file_name &&  "device_file_name should never be NULL");

	if (!device_file_name)
		return OpStatus::ERR_NULL_POINTER;

	OP_STATUS rc = OpStatus::ERR;

	uni_char device_name[0x1000];
	uni_char drive[3] = UNI_L("A:");
	drive[2] = 0;

	// Iterating through the drive letters
	for (uni_char actDrive = UNI_L('A'); actDrive <= UNI_L('Z'); actDrive++)
	{
		drive[0] = actDrive;

		// Query the device for the drive letter
		if (QueryDosDevice(drive, device_name, 0x1000) != 0)
		{
			// Network drive?
			if (uni_strnicmp(_T("\\Device\\LanmanRedirector\\"), device_name, 25) == 0)
			{
				//Mapped network drive 
				uni_char drive_letter;
				DWORD dwParam;

				uni_char shared_name[0x1000];

				if (swscanf(device_name, 
							 UNI_L("\\Device\\LanmanRedirector\\;%c:%d\\%s"), 
							 &drive_letter, 
							 &dwParam, 
							 shared_name) != 3)
						continue;

				uni_strcpy(device_name, UNI_L("\\Device\\LanmanRedirector\\"));
				uni_strcat(device_name, shared_name);
			}
			
			// Is this the drive letter we are looking for?
			if (uni_strnicmp(device_name, device_file_name, uni_strlen(device_name)) == 0)
			{
				fsFileName.Set(drive);
				fsFileName.Append((uni_char*)(device_file_name + uni_strlen(device_name)));

				rc = OpStatus::OK;

				break;
			}
		}
	}

	return rc;
}

// From DOS file name to device file name
OP_STATUS HandleInfo::GetDeviceFileName(const uni_char* fs_file_name, OpString& device_file_name)
{
	OP_ASSERT(fs_file_name &&  "fs_file_name should never be NULL");

	if (!fs_file_name || uni_strlen(fs_file_name) < 3)
		return OpStatus::ERR;

	OP_STATUS rc = OpStatus::ERR;
	uni_char drive[3];

	// Get the drive letter 
	// unfortunetaly it works only with DOS file names
	uni_strncpy(drive, fs_file_name, 2);
	drive[2] = 0;

	OpString device_name;
	RETURN_OOM_IF_NULL(device_name.Reserve(0x1000));

	// Query the device for the drive letter
	if (QueryDosDevice(drive, device_name.CStr(), 0x1000) != 0)
	{
		if (device_name.Length() > 4 && device_name.CompareI("\\??\\", 4) == 0)
		{
			// If a drive has been mapped with SUBST,
			// it will have \??\ prepended on the device_name
			device_file_name.Set(device_name + 4);
		}
		else
		// Network drive?
		if (device_name.Length() > 26 &&  device_name.CompareI("\\Device\\LanmanRedirector\\", 25) == 0)
		{
			//Mapped network drive
			INT index = device_name.Find("\\", 26);
			
			if (index == KNotFound)
				return OpStatus::ERR;

			OpString shared_name;
			shared_name.Set(device_name.CStr() + index);
			device_name.Set("\\Device\\LanmanRedirector");
			device_name.Append(shared_name);
		}

		// Set the device name, and add the file name, and the conversion is done.
		device_file_name.Set(device_name);
		device_file_name.Append(fs_file_name + 2);
		rc = OpStatus::OK;
	}

	return rc;
}

ProcessItem* HandleInfo::AddProcess(DWORD process_id, BOOL load_icon /*= TRUE*/)
{

	UINT32 i = 0;
	UINT32 count = m_process_list.GetCount();
	SHORT_PROCESS_INFO* process_info = NULL;

	while (i < count)
	{
		process_info = m_process_list.Get(i);
		if (process_info->process_id == process_id)
		{
			OpAutoPtr<ProcessItem> new_process_item(OP_NEW(ProcessItem, (process_id)));
			
			if (!new_process_item.get())
				return NULL;

			// Path
			OpString path;
			if (OpStatus::IsError(new_process_item->SetProcessPath(process_info->process_path.CStr())))
				return NULL;

			// Icon
			if (load_icon)
			{
				OpStatus::Ignore(new_process_item->LoadProcessIcon());
			}

			// Title
			if (OpStatus::IsError(new_process_item->FindProcessTitle()))
			{
				UINT32 j = 0;
				WINDOW_INFO* window_info = m_window_list.Get(j);

				while (window_info && !new_process_item->GetProcessTitle())
				{
					if (window_info->process_id == new_process_item->GetProcessID())
					{
						if (OpStatus::IsError(new_process_item->SetProcessTitle(window_info->title.CStr())))
							return NULL;
					}
					window_info = m_window_list.Get(++j);
				}
			}

			return new_process_item.release();
		}

		i++;
	}

	return NULL;
		
}

BOOL HandleInfo::ProcessListHasID(DWORD pID)
{
	INT32 i;
	INT32 count = m_process_list.GetCount();

	for (i = 0; i < count; i++)
	{
		if (m_process_list.Get(i)->process_id == pID)
			return TRUE;
	}

	return FALSE;
}

OP_STATUS HandleInfo::IsFileInUse(const uni_char* file_name, OpVector<ProcessItem>& processes, const BOOL refresh_lists)
{

	OP_NEW_DBG("IsFileInUse", "Winstaller");

	if (!file_name)
		return OpStatus::ERR_NULL_POINTER;

	if (!m_process_list.GetCount() || refresh_lists)
		RETURN_IF_ERROR(GetProcessList());

	if (!m_window_list.GetCount() || refresh_lists)
		OpStatus::Ignore(GetWindowList());

	//
	// Remove installer instances (we don't want to terminate the installer)
	//

	SHORT_PROCESS_INFO* process = NULL;
	UINT32 i = 0;

	while ((process = m_process_list.Get(i)) != NULL)
	{
		if (IsInstaller(process->process_id))
			m_process_list.Delete(i);
		else
			++i;
	}

	//
	// See if we can find any module that is loaded into a process
	//

	BOOL match = FALSE;
	for (UINT32 i = 0; i < m_process_list.GetCount(); i++)
	{
		OpStatus::Ignore(IsModuleInUse(m_process_list.Get(i)->process_id, file_name, match));
		
		if (match)
		{
			ProcessItem* process_name = AddProcess(m_process_list.Get(i)->process_id);
			if (process_name && OpStatus::IsError(processes.Add(process_name)))
			{
				OP_DELETE(process_name);
				return OpStatus::ERR_NO_MEMORY;
			}
		}
	}

	//
	// See if we can find any file handle that is locked
	//

	OpString handle_data;
	OpString device_file_name;
	RETURN_IF_ERROR(GetDeviceFileName(file_name, device_file_name));

	PSEUDO_KERNEL_HANDLE pkh;
	SYSTEM_HANDLE_INFO_EX sinfo = {0};
	pkh.query = IsSystemWinXP() ? SystemExtendedHandleInformation : SystemHandleInformation;

	OpAutoArray<BYTE> handles;
	RETURN_IF_ERROR(GetHandleList(pkh.query, handles, pkh.count));

	if (pkh.query == SystemHandleInformation)
		pkh.info = &sinfo;

	for (UINT32 i = 0; i < pkh.count; i++)
	{
		if (pkh.query == SystemExtendedHandleInformation)
			pkh.info = (SYSTEM_HANDLE_INFO_EX*)&((SYSTEM_HANDLES_EX*)handles.get())->handles[i];
		else
		{
			sinfo.process_id = (DWORD)(*(SYSTEM_HANDLE_INFO*)&((SYSTEM_HANDLES*)handles.get())->handles[i]).process_id;
			sinfo.handle = (DWORD)(*(SYSTEM_HANDLE_INFO*)&((SYSTEM_HANDLES*)handles.get())->handles[i]).handle;
			sinfo.handle_type = (DWORD)(*(SYSTEM_HANDLE_INFO*)&((SYSTEM_HANDLES*)handles.get())->handles[i]).handle_type;
		}

#ifdef _DEBUG
		// Add Winstaller as a key in debug.txt to print out what process you are currently checking.
		static DWORD current_process = (DWORD)-1;
		if (current_process != pkh.info->process_id)
		{
			current_process = pkh.info->process_id;
			BOOL found = FALSE;

			for (UINT32 i = 0; i < m_process_list.GetCount(); i++)
			{
				if (current_process == m_process_list.Get(i)->process_id)
				{
					found = TRUE;
					OP_DBG(("Current process %S", m_process_list.Get(i)->process_name.CStr()));
				}

			}
			if (!found)
			{
				OP_DBG(("Current process %u", current_process));
			}
		}
#endif // DEBUG

		// For some reason sometimes the process id does not seem to be a valid process id at all.
		if (!ProcessListHasID(pkh.info->process_id))
			continue;
		
		INT type = OB_TYPE_UNKNOWN;

		// Check if the handle is a file handle.
		if (OpStatus::IsError(GetType(pkh, type)) || type != OB_TYPE_FILE)
			continue;
			
		// Check if the handle contain some info about what file it belongs to.
		if (OpStatus::IsError(GetName(pkh, type, handle_data)) || handle_data.IsEmpty())
			continue;

		// Check if the handle corresponds to the handle we are looking for.
		if (uni_stricmp(handle_data.CStr(), device_file_name.CStr()) != 0)
			continue;

		// We found one that matches. Add it to the list.
		ProcessItem* process_name = AddProcess(pkh.info->process_id);
		if (process_name && OpStatus::IsError(processes.Add(process_name)))
		{
			OP_DELETE(process_name);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

OP_STATUS HandleInfo::GetHandleList(SYSTEM_INFORMATION_CLASS query, OpAutoArray<byte>& handles, DWORD& count)
{
	count = 0;

	// Currently we only support two types of querries.
	OP_ASSERT(query == SystemHandleInformation || query == SystemExtendedHandleInformation);

	DWORD needed = 0;
	NTSTATUS ntstatus = NT_STATUS_INFO_LENGTH_MISMATCH;
	handles.reset();

	if (!NT_SUCCESS(OPNtQuerySystemInformation(query, handles.get(), needed, &needed)))
	{	
		// Just make sure we don't get stuck in an endless loop.
		// Or get an insane random high number as once seen.
		if (needed == 0 || needed > 0x1000000)
			needed = 1;
		
		do
		{
			handles.reset((BYTE*)(ULONG_PTR)OP_NEWA(byte, needed));
			RETURN_OOM_IF_NULL(handles.get());
			ZeroMemory(handles.get(), needed);
			ntstatus = OPNtQuerySystemInformation(query, handles.get(), needed, NULL);
		}
		while ((ntstatus == NT_STATUS_INFO_LENGTH_MISMATCH) && (needed < 0x1000000) && (needed *= 2));
	}

	if (query == SystemExtendedHandleInformation)
		count = ((SYSTEM_HANDLES_EX*)handles.get())->count;
	else
		count = ((SYSTEM_HANDLES*)handles.get())->count;

	return NT_SUCCESS(ntstatus) && handles.get() ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS HandleInfo::GetProcessList()
{
	m_process_list.DeleteAll();

	// Take a snapshot of all processes in the system.
	OpAutoHANDLE process_snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));

	if (process_snapshot.get() == INVALID_HANDLE_VALUE)
		return OpStatus::ERR;

	// Set the size of the structure before using it.
	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32);

	// Retrieve information about the first process,
	// and exit if unsuccessful
	if (!Process32First(process_snapshot, &pe32))
		return OpStatus::ERR;

	OP_STATUS status = OpStatus::OK;

	static const DWORD PATH_LEN = 0x1000;
	uni_char process_path[PATH_LEN];

	// Now walk the snapshot of processes, and
	// display information about each process in turn
	do
	{
		// Retrieve the priority class.
		HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);

		if (!process)
			continue;

		SHORT_PROCESS_INFO* info = OP_NEW(SHORT_PROCESS_INFO, ());

		if (!info)
		{
			CloseHandle(process);
			return OpStatus::ERR_NO_MEMORY;
		}

		ZeroMemory(process_path, PATH_LEN);

		if (GetModuleFileNameEx(process, NULL, process_path, PATH_LEN) != 0)
		{
			info->process_path.Set(process_path);
			info->process_id = pe32.th32ProcessID;
			info->parent_process_id = pe32.th32ParentProcessID;
			info->process_name.Set(pe32.szExeFile);
			status = m_process_list.Add(info);
		}

		CloseHandle(process);
	}
	while(OpStatus::IsSuccess(status) && Process32Next(process_snapshot, &pe32));

	return status;
}


OP_STATUS HandleInfo::GetParentProcessItem(DWORD process_id, OpAutoPtr<ProcessItem>& process)
{
	OP_ASSERT(process.get() == NULL && "The auto pointer should not contain a ProcessItem at this time");

	if (!m_process_list.GetCount())
		RETURN_IF_ERROR(GetProcessList());

	for (UINT32 i = 0; i < m_process_list.GetCount(); ++i)
	{
		if (process_id == m_process_list.Get(i)->process_id)
		{
			process = AddProcess(m_process_list.Get(i)->parent_process_id);
			RETURN_OOM_IF_NULL(process.get());
			break;
		}
	}

	return OpStatus::OK;
}

OP_STATUS HandleInfo::GetChildProcessItems(DWORD process_id, OpVector<ProcessItem>& processes)
{
	OP_ASSERT(processes.GetCount() == 0 && "The array should be empty.");

	if (!m_process_list.GetCount())
		RETURN_IF_ERROR(GetProcessList());

	for (UINT32 i = 0; i < m_process_list.GetCount(); ++i)
	{
		if (process_id == m_process_list.Get(i)->parent_process_id)
		{
			ProcessItem* process = AddProcess(m_process_list.Get(i)->process_id);
			RETURN_OOM_IF_NULL(process);
			RETURN_IF_ERROR(processes.Add(process));
		}
	}

	return OpStatus::OK;

}

BOOL HandleInfo::IsInstaller(const DWORD process_id)
{
	DWORD cur_pid = GetCurrentProcessId();

	// Same process id
	if (cur_pid == process_id)
		return TRUE;

	// Check if this could be an installer, by checking if it has
	// the same path as the current process, which is always an installer/uninstaller.
	OpAutoPtr<ProcessItem> pi = AddProcess(process_id, FALSE);
	if (pi.get() && g_spawnPath.CompareI(pi->GetProcessPath()) != 0)
		return FALSE;

	// Is the parent (which means also installer [elevated/non-elevated])
	OpAutoPtr<ProcessItem> p;
	OP_STATUS status = GetParentProcessItem(process_id, p);
	
	if (OpStatus::IsSuccess(status) && p.get())
	{
		if (cur_pid == p->GetProcessID())
		{
			return TRUE;
		}
	}

	// Is the child (which means also installer [elevated/non-elevated])
	OpAutoVector<ProcessItem>p_list;
	status = GetChildProcessItems(process_id, p_list);
	
	if (OpStatus::IsSuccess(status))
	{
		for (UINT32 i = 0; i < p_list.GetCount(); ++i)
		{
			if (cur_pid == p_list.Get(i)->GetProcessID())
			{
				return TRUE;
			} 
		}
	}

	return FALSE;
}

OP_STATUS HandleInfo::GetWindowList()
{
	m_window_list.DeleteAll();
	
	return EnumWindows(EnumWindowInfo, (LPARAM)this) == TRUE ? OpStatus::OK : OpStatus::ERR;
}

// This is the callback function for the GetWindowList() function.
// It will be called once for every window open at the current time.
//
// 
BOOL CALLBACK HandleInfo::EnumWindowInfo(HWND hwnd, LPARAM lParam)
{
	uni_char title[1024];
	BOOL succeeded = TRUE;
	DWORD length = GetWindowText(hwnd, title , 1024);

	if (length > 0)
	{
		WINDOW_INFO* window_info = OP_NEW(WINDOW_INFO, ());
		RETURN_FALSE_IF(!window_info);

		HandleInfo* handle_info = (HandleInfo*)lParam;

		if(OpStatus::IsSuccess(handle_info->m_window_list.Add(window_info)))
		{
			window_info->hwnd = hwnd;
			GetWindowThreadProcessId(hwnd, &window_info->process_id);
			
			if (OpStatus::IsError(window_info->title.Set(title)))
				succeeded = FALSE;
		}
		else
		{
			succeeded = FALSE;
			OP_DELETE(window_info);
		}
	}

	return succeeded;
}


// File related functions
void HandleInfo::GetFileNameThread(PVOID pParam)
{
	// This thread function for getting the filename
	// if access denied, we hang up in this function, 
	// so if it times out we just kill this thread
	GetFileNameThreadParam* p = (GetFileNameThreadParam*)pParam;

	p->rc = OPNtQueryInformationFile(p->hFile, &p->iob, (PVOID)p->file_name_info, p->size, FileNameInformation);

}

OP_STATUS HandleInfo::GetFileName(PSEUDO_KERNEL_HANDLE& pkh, OpString& str, DWORD process_id)
{
	HANDLE hRemoteProcess = NULL;
	GetFileNameThreadParam tp = {0};

	tp.size = 0x2000;
	tp.file_name_info = (FILE_NAME_INFORMATION*)OP_NEWA(BYTE, tp.size);
	RETURN_OOM_IF_NULL(tp.file_name_info);
	ZeroMemory(tp.file_name_info, tp.size);

	// If this is a remote process, we need to duplicate the handle
	if (process_id != GetCurrentProcessId())
	{
		// Open the process for handle duplication
		hRemoteProcess = ::OpenProcess(PROCESS_DUP_HANDLE, FALSE, process_id);
		RETURN_VALUE_IF_NULL(hRemoteProcess, OpStatus::ERR);

		// Duplicate the remote handle for our process
		if(!::DuplicateHandle(hRemoteProcess, (HANDLE)pkh.info->handle, GetCurrentProcess(), &tp.hFile, 0, FALSE, DUPLICATE_SAME_ACCESS))
		{
			OP_DELETEA(tp.file_name_info);
			CloseHandle(hRemoteProcess);
			return OpStatus::ERR;
		}
	}
	else
		tp.hFile = (HANDLE)pkh.info->handle;

	HANDLE thread = (HANDLE)_beginthread(GetFileNameThread, 0, &tp);

	if (thread)
	{
		// Wait for finishing the thread
		if (WaitForSingleObject(thread, 100) == WAIT_TIMEOUT)
		{
			// Access denied
			// Terminate the thread
			TerminateThread(thread, 0);
			tp.rc = NT_STATUS_ACCESS_DENIED;
		}

		str.Set(tp.file_name_info->name, tp.file_name_info->name_len / sizeof(WCHAR));
		OP_DELETEA(tp.file_name_info);
	}

	// If we opened  a remote process, then close it.
	if (hRemoteProcess)
		CloseHandle(hRemoteProcess);

	// If we created a duplicated handle, then close it.
	if (tp.hFile && tp.hFile != (HANDLE)pkh.info->handle)
		CloseHandle(tp.hFile);

	switch(tp.rc)
	{
		case NT_STATUS_SUCCESS:
			return OpStatus::OK;
		case NT_STATUS_NOT_IMPLEMENTED:
			return OpStatus::ERR_NOT_SUPPORTED;
		case NT_STATUS_INVALID_INFO_CLASS:
			return OpStatus::ERR_BAD_FILE_NUMBER;
		case NT_STATUS_INVALID_DEVICE_REQUEST:
			return OpStatus::ERR;
		case NT_STATUS_INFO_LENGTH_MISMATCH:
			return OpStatus::ERR;
		case NT_STATUS_ACCESS_DENIED:
			return OpStatus::ERR_NO_ACCESS;
		case NT_STATUS_OBJECT_TYPE_MISMATCH:
			return OpStatus::ERR;
		default:
			return OpStatus::ERR;
	}
}

OP_STATUS HandleInfo::GetProcessId(HANDLE h, DWORD& process_id, DWORD remoteProcess_id)
{
	OP_STATUS status = OpStatus::ERR;
	HANDLE handle;
	HANDLE hRemoteProcess = NULL;
	PROCESS_BASIC_INFORMATION pi;
	
	ZeroMemory(&pi, sizeof(pi));
	process_id = 0;

	if (remoteProcess_id != GetCurrentProcessId())
	{
		// Open the process for handle duplication
		hRemoteProcess = ::OpenProcess(PROCESS_DUP_HANDLE, FALSE, remoteProcess_id);
		RETURN_VALUE_IF_NULL(hRemoteProcess, OpStatus::ERR);

		// Duplicate the remote handle for our process
		if (!::DuplicateHandle(hRemoteProcess, h, GetCurrentProcess(), &handle, 0, FALSE, DUPLICATE_SAME_ACCESS))
		{
			CloseHandle(hRemoteProcess);
			return status;
		}
	}
	else
		handle = h;

	// Get the process information
	if (NT_SUCCESS(OPNtQueryInformationProcess(handle, ProcessBasicInformation, &pi, sizeof(pi), NULL)))
	{
		process_id = pi.unique_process_id;
		status = OpStatus::OK;
	}

	if (hRemoteProcess)
		CloseHandle(hRemoteProcess);

	if (handle && handle != h)
		CloseHandle(handle);

	return status;
}

OP_STATUS HandleInfo::GetName(PSEUDO_KERNEL_HANDLE& pkh, INT type, OpString& str)
{
	OP_STATUS status = OpStatus::ERR;

	HANDLE tmp_handle = NULL;
	
	DWORD dwId = 0;

	str.Empty();

	if (pkh.info->process_id != GetCurrentProcessId())
	{
		HANDLE hRemoteProcess = NULL;
		BOOL ret;

		// Open the process for handle duplication
		hRemoteProcess = ::OpenProcess(PROCESS_DUP_HANDLE, FALSE, pkh.info->process_id);
		RETURN_VALUE_IF_NULL(hRemoteProcess, OpStatus::ERR);

		ret = ::DuplicateHandle(hRemoteProcess, (HANDLE)pkh.info->handle, GetCurrentProcess(), &tmp_handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
		
		CloseHandle(hRemoteProcess);
		
		if (!ret)
			return status;
	}
	else
		tmp_handle = (HANDLE)pkh.info->handle;

	// let's be happy, handle is in our process space, so query the infos :)
	switch(type)
	{
		case OB_TYPE_PROCESS:
			GetProcessId(tmp_handle, dwId, GetCurrentProcessId());
			str.AppendFormat(UNI_L("PID: 0x%X"), dwId);
			status = OpStatus::OK;

			break;

		case OB_TYPE_FILE:
			status = GetFileName(pkh, str, pkh.info->process_id);

			break;
	};
	
	ULONG size = 0;
	char* buffer = NULL; // OBJECT_NAME_INFORMATION

	if (str.HasContent())
	{
		OPNtQueryObject(tmp_handle, ObjectNameInformation, NULL, 0, &size);

		// let's try to use the default
		if (size == 0)
			size = 0x2000;

		buffer = OP_NEWA(char, size);

		if (buffer)
		{
			if (NT_SUCCESS(OPNtQueryObject(tmp_handle, ObjectNameInformation, buffer, size, NULL)))
			{
				UNICODE_STRING* uni_string = (UNICODE_STRING*)buffer;
				OP_ASSERT(uni_string->length <= uni_string->max_len && uni_string->length <= size);
				str.Set(uni_string->str, uni_string->length);
				status = OpStatus::OK;
			}
			OP_DELETEA(buffer);
		}
		else
			status = OpStatus::ERR;
	}

	if (tmp_handle && tmp_handle != (HANDLE)pkh.info->handle)
		CloseHandle(tmp_handle);
	
	return status;

}

OP_STATUS HandleInfo::GetTypeToken(PSEUDO_KERNEL_HANDLE& pkh, OpString& str)
{
	HANDLE tmp_handle = NULL;
	OP_STATUS status = OpStatus::ERR;
	char* buffer = NULL;

	if (pkh.info->process_id != GetCurrentProcessId())
	{
		BOOL ret = FALSE;
		HANDLE hRemoteProcess = NULL;

		// Open the process for handle duplication
		hRemoteProcess = ::OpenProcess(PROCESS_DUP_HANDLE, FALSE, pkh.info->process_id);
		RETURN_VALUE_IF_NULL(hRemoteProcess, OpStatus::ERR);

		// Duplicate the remote handle for our process
		ret = ::DuplicateHandle(hRemoteProcess, (HANDLE)pkh.info->handle, GetCurrentProcess(), &tmp_handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
		CloseHandle(hRemoteProcess);
		
		if (!ret)
			return status;
	}
	else
		tmp_handle = (HANDLE)pkh.info->handle;

	// On Xp GetFileType can end up in a race condition which deadlocks the thread. So on XP we have to use the undocumented NtQueryObject() instead.
	// On Vista there were in-depth design changes for the GetFileType() implemenation, so this kind of hang will not be seen in newer Operating Systems.
	// On the other hand NtQueryObject() function seems to end up in deadlocks when it encounters unnamed pipes on Vista/Win7, so there we have to use
	// GetFileType() first to determine if it is a pipe.

	if (IsSystemWinVista() && GetFileType(tmp_handle) == FILE_TYPE_PIPE)
		status = str.Set(UNI_L("Pipe"));
	else
	{
		const ULONG size = 1024;
		buffer = OP_NEWA(char, size);

		if (!buffer)
			status = OpStatus::ERR_NO_MEMORY;
		else
		{
			ZeroMemory(buffer, size);

			// Query the info size (type)
			if (NT_SUCCESS(OPNtQueryObject(tmp_handle, ObjectTypeInformation, buffer, size, NULL)))
			{
				OBJECT_TYPE_INFORMATION* obj_type_info = (OBJECT_TYPE_INFORMATION*)buffer;
				status = str.Set(obj_type_info->name.str);
			}
		}
	}

	if (tmp_handle && tmp_handle != (HANDLE)pkh.info->handle)
		CloseHandle(tmp_handle);
	
	if (buffer)
		OP_DELETEA(buffer);

	return str.HasContent() ? OpStatus::OK : status;
}

OP_STATUS HandleInfo::GetType(PSEUDO_KERNEL_HANDLE& pkh, INT& type)
{
	OpString str_type;
	
	RETURN_IF_ERROR(GetTypeToken(pkh, str_type));

	UINT32 count = ARRAY_SIZE(SystemHandleTypeStr);

	for (WORD i = 1; i < count; i++)
	{
		if (str_type.Compare(SystemHandleTypeStr[i]) == 0)
		{
			type = i;
			break;
		}
	}

	return OpStatus::OK;
}

OP_STATUS HandleInfo::GetModuleList(DWORD process_id, OpAutoArray<HMODULE>& modules, DWORD& count)
{
	// Open process to read to query the module list
	OpAutoHANDLE hProcess(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process_id));
	RETURN_VALUE_IF_NULL(hProcess.get(), OpStatus::ERR);

	DWORD needed = 0;

	// Get the number of modules
	if (EnumProcessModules(hProcess , NULL, 0, &needed) == 0)
		return OpStatus::ERR;

	OP_STATUS status = OpStatus::OK;

	// Make sure the needed memory to store the modules has not changed since we checked.
	do
	{
		count = needed / sizeof(HMODULE);
		modules.reset(OP_NEWA(HMODULE, (count)));
		RETURN_OOM_IF_NULL(modules.get());

		// Get module handles
		if (EnumProcessModules(hProcess, modules.get(), needed, &needed) == 0)
			status = OpStatus::ERR;

	} while ((needed / sizeof(HMODULE)) > count);

	return status;
}

OP_STATUS HandleInfo::IsModuleInUse(DWORD process_id, const uni_char* file_path, BOOL& match)
{
	match = FALSE;

	if (!file_path || !*file_path)
		return OpStatus::ERR_NULL_POINTER;

	// Open process to read the module list
	OpAutoHANDLE hProcess(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process_id));
	RETURN_VALUE_IF_NULL(hProcess.get(), OpStatus::ERR);

	OP_STATUS status = OpStatus::OK;
	OpAutoArray<HMODULE> modules;
	DWORD count = 0;

	RETURN_IF_ERROR(GetModuleList(process_id, modules, count));

	MODULE_INFO module_info;
	module_info.process_id = process_id;

	// Go through all the module handles
	for (DWORD i = 0; i < count; i++)
	{
		module_info.handle = modules[i];

		// Get module full paths
		if (GetModuleFileNameEx(hProcess, modules[i], module_info.full_path, MAX_PATH))
		{
			if (uni_stricmp(module_info.full_path, file_path) == 0)
			{
				match = TRUE;
				break;
			}
		}
	}	
	return status;
}


OP_STATUS HandleInfo::IsOperaRunningAt(const uni_char* path, OpVector<ProcessItem>& processes, const BOOL refresh_lists)
{
    if (!path)
		return OpStatus::ERR_NULL_POINTER;

    if (refresh_lists || !m_process_list.GetCount())
		RETURN_IF_ERROR(GetProcessList());

	//
	// Remove installer instances (we don't want to terminate the installer)
	//

	SHORT_PROCESS_INFO* process = NULL;
	UINT32 i = 0;

	while ((process = m_process_list.Get(i)) != NULL)
	{
		if (IsInstaller(process->process_id))
			m_process_list.Delete(i);
		else
			++i;
	}

	//
	// Go through all the processes and see if opera.exe is in use in that folder
	//

	OpString opera_exe_path;
	RETURN_IF_ERROR(opera_exe_path.Set(path));
	if (opera_exe_path.FindI(UNI_L("\\opera.exe")) == KNotFound)
		RETURN_IF_ERROR(opera_exe_path.Append(UNI_L("\\opera.exe")));
	else if (opera_exe_path.FindI(UNI_L("opera.exe")) == KNotFound)
		RETURN_IF_ERROR(opera_exe_path.Append(UNI_L("opera.exe")));

    i = 0;
    SHORT_PROCESS_INFO* process_info = NULL;

    while (process_info = m_process_list.Get(i++))
    {
		if (process_info->process_path.FindI(opera_exe_path.CStr()) == 0)
		{   
			OpAutoPtr<ProcessItem> process_item = AddProcess(process_info->process_id);
			if (process_item.get())
			{
				RETURN_IF_ERROR(processes.Add(process_item.get()));
				process_item.release();
			}
		}
    }

	//
	// Go through all the modules of each process and see if opera.dll is in use in that folder
	//

	OpString opera_dll_path;
	RETURN_IF_ERROR(opera_dll_path.Set(path));
	if (opera_exe_path.FindI(UNI_L("\\opera.dll")) == KNotFound)
		RETURN_IF_ERROR(opera_dll_path.Append(UNI_L("\\opera.dll")));
	else if (opera_dll_path.FindI(UNI_L("opera.dll")) == KNotFound)
		RETURN_IF_ERROR(opera_dll_path.Append(UNI_L("opera.dll")));

	BOOL match = FALSE;
	for (UINT32 i = 0; i < m_process_list.GetCount(); i++)
	{
		OpStatus::Ignore(IsModuleInUse(m_process_list.Get(i)->process_id, opera_dll_path, match));
		
		if (match)
		{
			OpAutoPtr<ProcessItem> process_name = AddProcess(m_process_list.Get(i)->process_id);
			if (process_name.get())
			{
				RETURN_IF_ERROR(processes.Add(process_name.get()));
				process_name.release();
			}
		}
	}

	return OpStatus::OK;
}


OP_STATUS HandleInfo::GetOpenFileList(DWORD process_id, OpVector<OpString>& file_list)
{
	if (!m_process_list.GetCount())
		RETURN_IF_ERROR(GetProcessList());

	//
	// Open process to read the module list into file_list
	//
	
	OpAutoHANDLE hProcess(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process_id));
	RETURN_VALUE_IF_NULL(hProcess.get(), OpStatus::ERR);

	OpAutoArray<HMODULE> modules;
	DWORD count = 0;

	RETURN_IF_ERROR(GetModuleList(process_id, modules, count));

	MODULE_INFO module_info;
	module_info.process_id = process_id;

	// Go through all the module handles
	for (DWORD i = 0; i < count; i++)
	{
		module_info.handle = modules[i];

		// Get module full paths
		if (GetModuleFileNameEx(hProcess, modules[i], module_info.full_path, MAX_PATH))
		{
			OpAutoPtr<OpString> str(OP_NEW(OpString,));
			RETURN_OOM_IF_NULL(str.get());
			RETURN_IF_ERROR(str->Set(module_info.full_path));
			RETURN_IF_ERROR(file_list.Add(str.get()));
			str.release();
		}
	}

	//
	// See if we can find any file handle that is locked
	//

	OpString handle_data;
	SYSTEM_HANDLE_INFO_EX sinfo = {0};
	PSEUDO_KERNEL_HANDLE pkh;

	if (IsSystemWinXP())
		pkh.query = SystemExtendedHandleInformation;
	else
	{
		pkh.query = SystemHandleInformation;
		pkh.info = &sinfo;
	}

	OpAutoArray<BYTE> handles;
	RETURN_IF_ERROR(GetHandleList(pkh.query, handles, pkh.count));

	for (UINT32 i = 0; i < pkh.count; i++)
	{
		if (pkh.query == SystemExtendedHandleInformation)
			pkh.info = (SYSTEM_HANDLE_INFO_EX*)&((SYSTEM_HANDLES_EX*)handles.get())->handles[i];
		else
		{
			sinfo.process_id = (DWORD)(*(SYSTEM_HANDLE_INFO*)&((SYSTEM_HANDLES*)handles.get())->handles[i]).process_id;
			sinfo.handle = (DWORD)(*(SYSTEM_HANDLE_INFO*)&((SYSTEM_HANDLES*)handles.get())->handles[i]).handle;
			sinfo.handle_type = (DWORD)(*(SYSTEM_HANDLE_INFO*)&((SYSTEM_HANDLES*)handles.get())->handles[i]).handle_type;
		}

		// Only search for thos process IDs that are equal to those sent in.
		if (pkh.info->process_id != process_id)
			continue;
		
		INT type = OB_TYPE_UNKNOWN;

		// Check if the handle is a file handle.
		if (OpStatus::IsError(GetType(pkh, type)) || type != OB_TYPE_FILE)
			continue;
			
		// Check if the handle contain some info about what file it belongs to.
		if (OpStatus::IsError(GetName(pkh, type, handle_data)) || handle_data.IsEmpty())
			continue;

		OpAutoPtr<OpString> str(OP_NEW(OpString,));
		RETURN_OOM_IF_NULL(str.get());
		RETURN_IF_ERROR(str->Set(handle_data.CStr()));
		RETURN_IF_ERROR(GetFsFileName(handle_data.CStr(), *str.get()));
		RETURN_IF_ERROR(file_list.Add(str.get()));
		str.release();
	}
	return OpStatus::OK;
}