// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Øyvind Østlund
//

#ifndef HANDLEINFO_H
#define HANDLEINFO_H

#include "platforms/windows/utils/ntdll.h"

class ProcessItem;

//
// Class HandleInfo
//	- Can fill in a list of processes which has an open handle to the file you pass in the path to.
//	- Can find out if Opera is running in a certain folder, and give you info about the running process.
//
class HandleInfo {

public:

	HandleInfo();
	~HandleInfo();

	OP_STATUS Init();

	OP_STATUS			IsFileInUse(const uni_char* file_name, OpVector<ProcessItem>& processes, const BOOL refresh_lists);
	OP_STATUS			IsOperaRunningAt(const uni_char* path, OpVector<ProcessItem>& processes, const BOOL refresh_lists);
	OP_STATUS			GetOpenFileList(DWORD process_id, OpVector<OpString>& file_list);

	OP_STATUS			GetParentProcessItem(DWORD process_id, OpAutoPtr<ProcessItem>& process);
	OP_STATUS			GetChildProcessItems(DWORD process_id, OpVector<ProcessItem>& processes);
	BOOL				IsInstaller(const DWORD process_id);

	static void			GetFileNameThread(PVOID pParam);
	
private:

	typedef LARGE_INTEGER			QWORD;

	// Copy of a kernel system handle, with a duplicate handle this process can use as it's own.
	struct PSEUDO_KERNEL_HANDLE {
		NtDll::SYSTEM_INFORMATION_CLASS	query;
		DWORD							count;
		NtDll::SYSTEM_HANDLE_INFO_EX*	info;
	};

	OP_STATUS						GetProcessList();
	OP_STATUS						GetHandleList(NtDll::SYSTEM_INFORMATION_CLASS query, OpAutoArray<byte>& handles, DWORD& count);
	OP_STATUS						GetWindowList();
	OP_STATUS						GetModuleList(DWORD process_id, OpAutoArray<HMODULE>& modules, DWORD& count);

	BOOL							ProcessListHasID(DWORD pID);

	OP_STATUS						GetDeviceFileName(const uni_char* fs_file_name, OpString& device_file_name);
	OP_STATUS						GetFsFileName(const uni_char* device_file_name, OpString& fs_file_name);

	ProcessItem*					AddProcess(DWORD process_id, BOOL load_icon = TRUE);

	OP_STATUS						GetFileName(PSEUDO_KERNEL_HANDLE& pkh, OpString& str, DWORD process_id);
	OP_STATUS						GetType(PSEUDO_KERNEL_HANDLE& pkh, INT& type);
	OP_STATUS						GetTypeToken(PSEUDO_KERNEL_HANDLE& pkh, OpString& str);
	OP_STATUS						GetName(PSEUDO_KERNEL_HANDLE& pkh, INT type, OpString& str);
	OP_STATUS						GetFileHandleDeviceType(PSEUDO_KERNEL_HANDLE& pkh, DEVICE_TYPE& device_type);

	OP_STATUS						GetProcessId(HANDLE h, DWORD& process_id, DWORD remoteProcess_id);
	OP_STATUS						IsModuleInUse(DWORD processID, const uni_char* file_path, BOOL& match);
	static BOOL CALLBACK			EnumWindowInfo(HWND hwnd, LPARAM lParam);

	OpVector<NtDll::SHORT_PROCESS_INFO>	m_process_list;
	OpVector<NtDll::WINDOW_INFO>		m_window_list;
};

#endif // HANDLEINFO_H
