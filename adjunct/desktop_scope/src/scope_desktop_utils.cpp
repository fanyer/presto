/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/scope/src/scope_transport.h"
#include "adjunct/desktop_scope/src/scope_desktop_utils.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"


/* OpScopeDesktopUtils */

OpScopeDesktopUtils::OpScopeDesktopUtils()
{
}

/* virtual */
OpScopeDesktopUtils::~OpScopeDesktopUtils()
{
}

OP_STATUS OpScopeDesktopUtils::DoGetString(const DesktopStringID &in, DesktopStringText &out)
{
	// Calculate the enum using the hash
	int enum_value = Str::LocaleString(in.GetEnumText().CStr());
	
	OpString text_string;
	g_languageManager->GetString((Str::LocaleString)enum_value, text_string);
	
	if (text_string.IsEmpty())
		return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("String not found"));
	
	RETURN_IF_ERROR(out.SetText(text_string.CStr()));
	
	return OpStatus::OK;
}

OP_STATUS OpScopeDesktopUtils::DoGetOperaPath(DesktopPath &out)
{
	OpString exec_path;
	if (OpStatus::IsError(g_op_system_info->GetBinaryPath(&exec_path)) || exec_path.IsEmpty())
		return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("Exec path not found"));
	RETURN_IF_ERROR(out.SetPath(exec_path.CStr()));
	
	return OpStatus::OK;
}

OP_STATUS OpScopeDesktopUtils::DoGetLargePreferencesPath(DesktopPath &out)
{
	OpString home_path;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_LOCAL_HOME_FOLDER, home_path));
	return out.SetPath(home_path.CStr());
}

OP_STATUS OpScopeDesktopUtils::DoGetSmallPreferencesPath(DesktopPath &out)
{
	OpString home_path;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_HOME_FOLDER, home_path));
	return out.SetPath(home_path.CStr());
}

OP_STATUS OpScopeDesktopUtils::DoGetCachePreferencesPath(DesktopPath &out)
{
	OpString home_path;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_CACHE_HOME_FOLDER, home_path));
	return out.SetPath(home_path.CStr());
}


OP_STATUS OpScopeDesktopUtils::DoGetCurrentProcessId(DesktopPid &out)
{
	out.SetPid(g_op_system_info->GetCurrentProcessId());
	return OpStatus::OK;
}
