/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Golczynski (tgolczynski@opera.com)
 */

#include "core/pch.h"

#if defined(OPERA_MAPI) || defined(M2_MAPI_SUPPORT)
#include "platforms/windows/mapi/OperaMapiUtils.h"

const uni_char* OperaMapiConst::ready_event_name = UNI_L("Global\\OperaMailIsReadyToUse_%d");
const uni_char* OperaMapiConst::request_handled_event_name = UNI_L("Global\\OperaMAPI_%d");

#ifdef OPERA_MAPI
#include "platforms/windows/IPC/SharedMemory.h"
#include "adjunct/m2/src/generated/g_proto_m2_mapimessage.h"
#include "adjunct/m2/src/generated/g_proto_descriptor_m2_mapimessage.h"
#include "adjunct/m2/src/generated/g_message_m2_mapimessage.h"
#include "platforms/windows/windows_ui/registry.h"
#include "platforms/windows/utils/OperaProcessesList.h"

extern HINSTANCE hInst;
HWND g_main_hwnd = NULL;

const uni_char* OperaMapiConst::clients_mail_key = UNI_L("SOFTWARE\\Clients\\Mail");
const uni_char* OperaMapiConst::clients_default_mail_shell_open_command_key = UNI_L("SOFTWARE\\Clients\\Mail\\%s\\Protocols\\mailto\\shell\\open\\command");
const UINT32	OperaMapiConst::time_period_10_seconds = 10000; //If it's not enough for Opera starting, probably something has gone wrong
const UINT32	OperaMapiConst::time_period_30_seconds = 30000;

const OpMessageAddress OperaMapiUtils::s_component_manager_address(0, 0, 0);

OperaMapiUtils::OperaMapiUtils()
	: m_shared_memory(NULL)
	, m_process_id(0)
{
}

OperaMapiUtils::~OperaMapiUtils()
{
	OP_DELETE(m_shared_memory);
}

OP_STATUS OperaMapiUtils::Init()
{
	RETURN_IF_ERROR(FindDefaultMailClient());

	OpINT32Vector process_ids;
	if (!g_opera_processes_list || OpStatus::IsError(g_opera_processes_list->GetOperaProcessIds(m_client_path, process_ids)) || process_ids.GetCount() == 0)
	{
		RETURN_IF_ERROR(StartOperaProcess());
		RETURN_IF_ERROR(process_ids.Add(m_process_id));
	}

	for (UINT32 i = 0; i < process_ids.GetCount(); i++)
	{
		m_process_id = process_ids.Get(i);
		OpString event_name;
		RETURN_IF_ERROR(event_name.AppendFormat(OperaMapiConst::ready_event_name, m_process_id));
		OpAutoHANDLE opera_is_running_event(OpenEvent(READ_CONTROL|SYNCHRONIZE|EVENT_MODIFY_STATE, FALSE, event_name.CStr()));

		if (opera_is_running_event.get() && WaitForSingleObject(opera_is_running_event.get(), 0) == WAIT_OBJECT_0)
		{
			WindowsSharedMemoryManager::SetCM0PID(m_process_id);
			WindowsSharedMemoryManager::OpenSharedMemory(m_shared_memory, s_component_manager_address);
			break;
		}
	}

	return m_shared_memory ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS OperaMapiUtils::FindDefaultMailClient()
{
	if (m_client_path.HasContent())
		return OpStatus::OK;

	OpString client_name;
	RETURN_IF_ERROR(OpSafeRegQueryValue(HKEY_CURRENT_USER, OperaMapiConst::clients_mail_key, client_name, NULL));
	if (client_name.IsEmpty())
	{
		RETURN_IF_ERROR(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, OperaMapiConst::clients_mail_key, client_name, NULL));
		if(client_name.IsEmpty())
			return OpStatus::ERR;
	}

	OpString key;
	RETURN_IF_ERROR(key.AppendFormat(OperaMapiConst::clients_default_mail_shell_open_command_key, client_name.CStr()));

	OpString tmp;
	RETURN_IF_ERROR(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, key.CStr(), tmp, NULL));

	if (tmp[0] != '\"')
		return OpStatus::ERR;
	int end = tmp.FindFirstOf(UNI_L("\""), 1);

	if (end != KNotFound)
	{
		m_client_path.Set(tmp.SubString(1, end - 1));
	}

	return m_client_path.IsEmpty() ? OpStatus::ERR : OpStatus::OK;
}

OP_STATUS OperaMapiUtils::StartOperaProcess()
{
	STARTUPINFO si = {};
	PROCESS_INFORMATION pi;

	si.cb = sizeof(si);

	OpString opera_path;
	RETURN_IF_ERROR(opera_path.AppendFormat("\"%s\"", m_client_path.CStr()));


	BOOL ret = CreateProcess(NULL,opera_path.CStr(),NULL,NULL,FALSE, 0, NULL, NULL, &si, &pi);
	m_process_id = pi.dwProcessId;

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	OpString event_name;
	RETURN_IF_ERROR(event_name.AppendFormat(OperaMapiConst::ready_event_name, m_process_id));
	OpAutoHANDLE opera_is_running_event(CreateEvent(NULL, TRUE, FALSE,event_name.CStr()));
	RETURN_VALUE_IF_NULL(opera_is_running_event.get(), OpStatus::ERR);

	if (!ret || WaitForSingleObject(opera_is_running_event.get(), OperaMapiConst::time_period_10_seconds) != WAIT_OBJECT_0)
		return OpStatus::ERR;
	return OpStatus::OK;
}


OP_STATUS OperaMapiUtils::ConvertToUTF16(LPSTR ansi, OpString& utf)
{
	if (ansi)
	{
		utf.Empty();
		INT32 length = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, ansi, strlen(ansi) + 1, NULL, 0);
		if (length)
		{
			RETURN_OOM_IF_NULL(utf.Reserve(length));
			if (length == MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, ansi, strlen(ansi) + 1, utf.CStr(), length))
				return OpStatus::OK;
		}
	}
	utf.Empty();
	return OpStatus::OK;
}

OP_STATUS OperaMapiUtils::CreateNewMailDescription(OpCreateNewMailMessage **mail_description, LPSTR subject, LPSTR text, LPSTR date)
{
	OpAutoPtr<OpCreateNewMailMessage> message(OpCreateNewMailMessage::Create(s_component_manager_address, s_component_manager_address));
	RETURN_OOM_IF_NULL(message.get());

	OpString tmp;
	RETURN_IF_ERROR(ConvertToUTF16(subject, tmp));
	RETURN_IF_ERROR(message->SetSubject(tmp.CStr() ? tmp.CStr() : UNI_L(""), tmp.CStr() ? tmp.Length() + 1 : 0));
	RETURN_IF_ERROR(ConvertToUTF16(text, tmp));
	RETURN_IF_ERROR(message->SetText(tmp.CStr() ? tmp.CStr() : UNI_L(""), tmp.CStr() ? tmp.Length() + 1 : 0));
	RETURN_IF_ERROR(ConvertToUTF16(date, tmp));
	RETURN_IF_ERROR(message->SetDate(tmp.CStr() ? tmp.CStr() : UNI_L(""), tmp.CStr() ? tmp.Length() + 1 : 0));
	message->SetEventId(static_cast<INT32>(GetCurrentProcessId()));
	*mail_description = message.release();

	return OpStatus::OK;
}

OP_STATUS OperaMapiUtils::AddRecipient(OpCreateNewMailMessage *mail_description, int type, LPSTR name, LPSTR address)
{
	OP_ASSERT(mail_description);
	OpString* str = NULL;

	switch (type)
	{
		case 1: str = &m_to; break;
		case 2: str = &m_cc; break;
		case 3: str = &m_bcc; break;
	}
	RETURN_VALUE_IF_NULL(str, OpStatus::ERR);

	OpString _address, _name;
	RETURN_IF_ERROR(ConvertToUTF16(address, _address));
	RETURN_IF_ERROR(ConvertToUTF16(name, _name));

	if (_address.IsEmpty() && _name.IsEmpty())
		return OpStatus::OK;
	else if (_address.IsEmpty())
		//We have to do this because some applications send an email address as a name(e.g. thunderbird handles this case)
		RETURN_IF_ERROR(str->AppendFormat("%s, ",_name.CStr()));
	else if (_name.IsEmpty())
		RETURN_IF_ERROR(str->AppendFormat("%s, ",_address.CStr()));
	else
		RETURN_IF_ERROR(str->AppendFormat("%s<%s>, ",_name.CStr(), _address.CStr()));

	return OpStatus::OK;
}

bool  OperaMapiUtils::IsValidFileName(const uni_char* name)
{
	if (!name)
		return true;
	const uni_char invalid_characters[] = UNI_L(":\\/? \"<>*|");
	return uni_strpbrk(name, invalid_characters) == NULL;
}

OP_STATUS OperaMapiUtils::AttachFile(OpCreateNewMailMessage *mail_description, LPSTR file, LPSTR name)
{
	OP_ASSERT(mail_description);

	OpString tmp;
	RETURN_IF_ERROR(ConvertToUTF16(file, tmp));
	RETURN_IF_ERROR(m_file_paths.AppendFormat("%s|",tmp.CStr() ? tmp.CStr() : UNI_L("")));

	RETURN_IF_ERROR(ConvertToUTF16(name, tmp));
	if (!IsValidFileName(tmp.CStr()))
		tmp.Empty();
	RETURN_IF_ERROR(m_file_names.AppendFormat("%s|",tmp.CStr() ? tmp.CStr() : UNI_L("")));
	return OpStatus::OK;
}

OP_STATUS OperaMapiUtils::AttachFiles(OpCreateNewMailMessage *mail_description, OpString& files_paths, OpString& files_names, uni_char delimiter)
{
	OP_ASSERT(mail_description);
	uni_char* path_begin = files_paths.CStr();
	uni_char* paths_end = files_paths.CStr() + files_paths.Length();
	uni_char* path_end = NULL;
	uni_char* name_begin = files_names.CStr();
	uni_char* names_end = files_names.CStr() + files_names.Length();
	uni_char* name_end = NULL;

	while (path_begin < paths_end)
	{
		if ((path_end = uni_strchr(path_begin, delimiter)) != NULL)
			*path_end = 0;
		RETURN_IF_ERROR(m_file_paths.AppendFormat("%s|", path_begin));
		path_begin = path_begin + uni_strlen(path_begin) + 1;
		if (name_begin < names_end)
		{
			if ((name_end = uni_strchr(name_begin, delimiter)) != NULL)
				*name_end = 0;
			if (IsValidFileName(name_begin))
				RETURN_IF_ERROR(m_file_names.Append(name_begin));
			name_begin = name_begin + uni_strlen(name_begin) + 1;
		}
		RETURN_IF_ERROR(m_file_names.Append("|"));
	}
	return OpStatus::OK;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
	DWORD process_id;
	GetWindowThreadProcessId(hwnd, &process_id);
	if (static_cast<DWORD>(lParam) == process_id)
	{
		SetForegroundWindow(hwnd);
		return FALSE;
	}
	return TRUE;
}

OP_STATUS OperaMapiUtils::SendMessageToOpera(OpCreateNewMailMessage *mail_description)
{
	OP_ASSERT(mail_description);
	RETURN_IF_ERROR(mail_description->SetFilePaths(m_file_paths.CStr() ? m_file_paths.CStr() : UNI_L(""), m_file_paths.CStr() ? m_file_paths.Length() + 1 : 0));
	RETURN_IF_ERROR(mail_description->SetFileNames(m_file_names.CStr() ? m_file_names.CStr() : UNI_L(""), m_file_names.CStr() ? m_file_names.Length() + 1 : 0));
	RETURN_IF_ERROR(mail_description->SetTo(m_to.CStr() ? m_to.CStr() : UNI_L(""), m_to.CStr() ? m_to.Length() + 1 : 0));
	RETURN_IF_ERROR(mail_description->SetCc(m_cc.CStr() ? m_cc.CStr() : UNI_L(""), m_cc.CStr() ? m_cc.Length() + 1 : 0));
	RETURN_IF_ERROR(mail_description->SetBcc(m_bcc.CStr() ? m_bcc.CStr() : UNI_L(""), m_bcc.CStr() ? m_bcc.Length() + 1 : 0));

	OpString name;
	RETURN_IF_ERROR(name.AppendFormat(OperaMapiConst::request_handled_event_name, GetCurrentProcessId()));
	OpAutoHANDLE handle(CreateEvent(NULL, TRUE, FALSE, name.CStr()));
	RETURN_VALUE_IF_NULL(handle.get(), OpStatus::ERR);

	// Move window to the foreground
	if (AllowSetForegroundWindow(GetCurrentProcessId()))
		EnumWindows(&EnumWindowsProc, static_cast<LPARAM>(m_process_id));
	//This operation can take a long time (depends on attachments size)
	RETURN_IF_ERROR(m_shared_memory->WriteMessage(mail_description));
	//We have to wait longer than 10s because Opera needs more time to load big mail database
	if (WaitForSingleObject(handle.get(), OperaMapiConst::time_period_30_seconds) != WAIT_OBJECT_0)
		return OpStatus::ERR;
	return OpStatus::OK;
}

#endif //OPERA_MAPI
#endif //defined(OPERA_MAPI) || defined(M2_MAPI_SUPPORT)
