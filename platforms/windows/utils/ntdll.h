// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Øyvind Østlund
//

#ifndef NTDLL_H
#define NTDLL_H

namespace NtDll {

	#pragma pack(push, 8)

	struct UNICODE_STRING {
		USHORT					length;
		USHORT					max_len;
		PWSTR					str;
	};

	struct MODULE_INFO {
		DWORD					process_id;
		TCHAR					full_path[MAX_PATH];
		HMODULE					handle;
	};

	struct SHORT_PROCESS_INFO {
		DWORD					process_id;
		DWORD					parent_process_id;
		OpString				process_name;
		OpString				process_path;
		OpString				process_title;
	};

	struct FILE_NAME_INFORMATION {
		ULONG					name_len;
		WCHAR					name[1];
	};

	// (System Info) SystemHandleInformation = 16
	__declspec(align(4))
	struct SYSTEM_HANDLE_INFO { 
		DWORD					process_id;
		byte					handle_type;
		byte					flags;				// 0x01 = PROTECT_FROM_CLOSE, 0x02 = INHERIT
		WORD					handle;
		void * __uptr __ptr32	object_ptr;
		DWORD					granted_access;
	};

	__declspec(align(4))
	struct SYSTEM_HANDLES {
		DWORD					count;
		SYSTEM_HANDLE_INFO		handles[1];
	};

	struct SYSTEM_HANDLE_INFO_EX {
		ULONG_PTR				object_ptr;
		DWORD					process_id;
		DWORD					handle;
		ULONG_PTR				granted_access;
		WORD					creator_back_trace_index;	// or is this flags?
		WORD					handle_type;				// word?
		DWORD					handle_attributes;			// or is this flags?
		DWORD					unknown3;					// Reserved
	};

	struct SYSTEM_HANDLES_EX {
		ULONG_PTR				count;
		ULONG_PTR				reserved;
		SYSTEM_HANDLE_INFO_EX	handles[1];
	};

	struct WINDOW_INFO {
		DWORD					process_id;
		OpString				title;
		HWND					hwnd;
	};

	// (Object info) ObjectTypeInformation = 2
	struct OBJECT_TYPE_INFORMATION {
		UNICODE_STRING			name;
		DWORD					object_count;
		DWORD					handle_count;
		DWORD					reserved1;
		DWORD					reserved2;
		DWORD					reserved3;
		DWORD					reserved4;
		DWORD					peak_obj_count;
		DWORD					peak_handle_count;
		DWORD					reserved5;
		DWORD					reserved6;
		DWORD					reserved7;
		DWORD					reserved8;
		DWORD					invalid_attributes;
		GENERIC_MAPPING			generic_mapping;
		DWORD					valid_access;
		byte					unknown_field;
		byte					maintain_handle_database;
		DWORD					pool_type;
		DWORD					paged_pool_usage;
		DWORD					non_paged_pool_usage;
	};

	struct OBJECT_ALL_TYPES_INFORMATION{
		DWORD					num_types;
		OBJECT_TYPE_INFORMATION type_info;
	};


	//
	// The PEB_LDR_DATA, LDR_DATA_TABLE_ENTRY, RTL_USER_PROCESS_PARAMETERS, PEB
	// and TEB structures are subject to changes between Windows releases; thus,
	// the field offsets and reserved fields may change. The reserved fields are
	// reserved for use only by the Windows operating systems. Do not assume a
	// maximum size for these structures.
	//
	struct PEB_LDR_DATA {
		BYTE					reserved1[8];
		PVOID					reserved2[3];
		LIST_ENTRY				in_memory_order_module_list;
	};

	struct RTL_USER_PROCESS_PARAMETERS {
		BYTE					Reserved1[16];
		PVOID					Reserved2[10];
		UNICODE_STRING			ImagePathName;
		UNICODE_STRING			CommandLine;
	};

	typedef VOID (NTAPI *PPS_POST_PROCESS_INIT_ROUTINE) (VOID);

	struct PEB {
		BYTE					reserved1[2];
		BYTE					being_debugged;
		BYTE					reserved2[1];
		PVOID					reserved3[2];
		PEB_LDR_DATA*			ldr;
		RTL_USER_PROCESS_PARAMETERS* process_parameters;
		BYTE					reserved4[104];
		PVOID					reserved5[52];
		PPS_POST_PROCESS_INIT_ROUTINE post_process_init_routine;
		BYTE					reserved6[128];
		PVOID					reserved7[1];
		ULONG					session_id;
	};

	struct PROCESS_BASIC_INFORMATION {
		PVOID					reserved1;
		PEB*					peb_base_address;
		PVOID					reserved2[2];
		ULONG_PTR				unique_process_id;
		PVOID					reserved3;
	};

	enum SYSTEM_INFORMATION_CLASS_ALT_NAMES {
		SystemFlagsInformation				= SystemGlobalFlag,
		SystemVdmInstemulInformation		= SystemInstemulInformation,
		SystemFileCacheInformation			= SystemCacheInformation,
		SystemInterruptInformation			= SystemProcessorStatistics,
		SystemFullMemoryInformation			= SystemMemoryUsageInformation1,
		SystemLoadGdiDriverInformation		= SystemLoadImage,
		SystemUnloadGdiDriverInformation	= SystemUnloadImage,
		SystemKernelDebuggerInformation		= SystemDebuggerInformation,
		SystemContextSwitchInformation		= SystemThreadSwitchInformation,
		SystemExtendServiceTableInformation	= SystemLoadDriver,
		SystemPrioritySeperation			= SystemPrioritySeparationInformation,
		SystemCurrentTimeZoneInformation	= SystemTimeZoneInformation,
		SystemTimeSlipNotification			= SystemSetTimeSlipEvent,
		SystemSessionCreate					= SystemCreateSession,
		SystemSessionDetach					= SystemDeleteSession,
		SystemVerifierThunkExtend			= SystemAddVerifier,
	};
	
	#pragma pack(pop)

} // namespace NtDll

#endif // NTDLL_H
