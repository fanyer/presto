/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef M2_MAPI_SUPPORT

#include "platforms/windows/mapi/WindowsDesktopMailClientUtils.h"
#include "platforms/windows/mapi/OperaMapiUtils.h"
#include "platforms/windows/utils/authorization.h"

OP_STATUS DesktopMailClientUtils::Create(DesktopMailClientUtils** desktop_mail_client_utils)
{
	*desktop_mail_client_utils = OP_NEW(WindowsDesktopMailClientUtils,());
	if (!*desktop_mail_client_utils)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

void WindowsDesktopMailClientUtils::SignalMailClientIsInitialized(bool initialized)
{
	if (IsOperaDefaultMailClient())
	{
		if (!m_event.get())
		{
			OpString event_name;
			RETURN_VALUE_IF_ERROR(event_name.AppendFormat(OperaMapiConst::ready_event_name, GetCurrentProcessId()), );
			m_event.reset(::CreateEvent(NULL, TRUE, FALSE, event_name.CStr()));
		}
		if (initialized)
			SetEvent(m_event);
		else
			ResetEvent(m_event);
	}
}

void WindowsDesktopMailClientUtils::SignalMailClientHandledMessage(INT32 id)
{
	OpString name;
	name.AppendFormat(OperaMapiConst::request_handled_event_name, id);
	HANDLE handle = OpenEvent(READ_CONTROL|SYNCHRONIZE|EVENT_MODIFY_STATE, FALSE, name.CStr());
	if (!handle)
		return;
	SetEvent(handle);
	CloseHandle(handle);
}

OP_STATUS WindowsDesktopMailClientUtils::SetOperaAsDefaultMailClient()
{
	OperaInstaller installer;
	OP_STATUS status = installer.SetDefaultMailerAndAssociations();
	if (OpStatus::IsSuccess(status) && !IsSystemWinVista() && WindowsUtils::IsPrivilegedUser(FALSE))
	{
		status = installer.SetDefaultMailerAndAssociations(TRUE);
	}
	return status;
}

#endif //M2_MAPI_SUPPORT