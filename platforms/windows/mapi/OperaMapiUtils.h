/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Golczynski (tgolczynski@opera.com)
 */

#ifndef OPERA_MAPI_UTILS_H
#define OPERA_MAPI_UTILS_H

#if defined(OPERA_MAPI) || defined(M2_MAPI_SUPPORT)

namespace OperaMapiConst
{
	extern const uni_char* ready_event_name;
	extern const uni_char* request_handled_event_name;
#ifdef OPERA_MAPI
	extern const UINT32 time_period_10_seconds;
	extern const UINT32 time_period_30_seconds;
	extern const uni_char* clients_mail_key;
	extern const uni_char* clients_default_mail_shell_open_command_key;
#endif
}

#ifdef OPERA_MAPI
class WindowsSharedMemory;
class OpCreateNewMailMessage;

class OperaMapiUtils
{
public:
	OperaMapiUtils();
	~OperaMapiUtils();

	OP_STATUS Init();
	OP_STATUS ConvertToUTF16(LPSTR ansi, OpString& utf);
	
	OP_STATUS CreateNewMailDescription(OpCreateNewMailMessage **mail_description, LPSTR subject = NULL, LPSTR text = NULL, LPSTR date = NULL);
	OP_STATUS AddRecipient(OpCreateNewMailMessage *mail_description, int type, LPSTR name, LPSTR address);
	OP_STATUS AttachFile(OpCreateNewMailMessage *mail_description, LPSTR file, LPSTR name);
	OP_STATUS AttachFiles(OpCreateNewMailMessage *mail_description, OpString& files_paths, OpString& files_names, uni_char delimiter);
	OP_STATUS SendMessageToOpera(OpCreateNewMailMessage *mail_description);
private:
	OP_STATUS StartOperaProcess();
	OP_STATUS AttachFile(const uni_char* name, const uni_char* path);
	OP_STATUS FindDefaultMailClient();
	bool IsValidFileName(const uni_char*);

	static const OpMessageAddress s_component_manager_address;
	OpString m_file_paths;
	OpString m_file_names;
	OpString m_to;
	OpString m_cc;
	OpString m_bcc;
	WindowsSharedMemory* m_shared_memory;
	OpString m_client_path;
	DWORD m_process_id;
};
#endif //OPERA_MAPI
#endif //defined(OPERA_MAPI) || defined(M2_MAPI_SUPPORT)
#endif //OPERA_MAPI_UTILS_H
