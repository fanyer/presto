/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "platforms/windows/WindowsGadgetList.h"

#include "adjunct/desktop_util/adt/Finalizer.h"
#include "adjunct/widgetruntime/GadgetConfigFile.h"
#include "adjunct/widgetruntime/pi/PlatformGadgetUtils.h"

#include "modules/gadgets/OpGadgetManager.h"
#include "modules/util/path.h"

#include "platforms/windows/windows_ui/Registry.h"
#include "platforms/windows/windows_ui/RegKeyIterator.h"
#include "platforms/windows/WindowsGadgetUtils.h"

const uni_char WindowsGadgetList::DEFAULT_LIST_REG_KEY_NAME[] =
		UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
const uni_char WindowsGadgetList::REG_KEY_MARKER_VALUE_NAME[] =
		UNI_L("OperaWidget");


WindowsGadgetList& WindowsGadgetList::GetDefaultInstance()
{
	static WindowsGadgetList default_list;
	static const OP_STATUS init_status = default_list.Init();
	OP_ASSERT(OpStatus::IsSuccess(init_status));
	return default_list;
}


OP_STATUS WindowsGadgetList::Init(const OpStringC& list_reg_key_name,
		const OpStringC& gadget_path_prefix)
{
	RETURN_IF_ERROR(m_list_reg_key_name.Set(list_reg_key_name));
	RETURN_IF_ERROR(m_gadget_path_prefix.Set(gadget_path_prefix));
	return OpStatus::OK;
}


const OpStringC& WindowsGadgetList::GetListRegKeyName() const
{
	return m_list_reg_key_name;
}


OpStringC WindowsGadgetList::GetRegKeyMarkerValueName()
{
	return REG_KEY_MARKER_VALUE_NAME;
}


OP_STATUS WindowsGadgetList::Read(
		OpAutoVector<GadgetInstallerContext>& gadget_contexts) const
{
	RegKeyIterator it;
	RETURN_IF_ERROR(it.Init(HKEY_CURRENT_USER, GetListRegKeyName()));

	for ( ; !it.AtEnd(); it.Next())
	{
		OpString app_key_name;
		RETURN_IF_ERROR(it.GetCurrent(app_key_name));

		OpAutoPtr<GadgetInstallerContext> context;
		RETURN_IF_ERROR(CreateGadgetContext(app_key_name,	context));
		if (NULL != context.get())
		{
			RETURN_IF_ERROR(gadget_contexts.Add(context.get()));
			context.release();
		}
	}

	return OpStatus::OK;
}


OP_STATUS WindowsGadgetList::CreateGadgetContext(const OpStringC& app_key_name,
		OpAutoPtr<GadgetInstallerContext>& context) const
{
	OpString display_name;
	OpString install_dir_path;
	if (OpStatus::IsError(
				ReadAppInfo(app_key_name, display_name, install_dir_path)))
	{
		return OpStatus::OK;
	}

	OpString gadget_file_path;
	if (OpStatus::IsError(WindowsGadgetUtils::FindGadgetFile(
					install_dir_path, gadget_file_path)))
	{
		return OpStatus::OK;
	}

	OpAutoPtr<GadgetConfigFile> install_config(
			GadgetConfigFile::Read(gadget_file_path));
	if (NULL == install_config.get())
	{
		return OpStatus::OK;
	}

	OpFile gadget_file;
	RETURN_IF_ERROR(gadget_file.Construct(gadget_file_path));
	OpString gadget_dir_path, error_reason;
	RETURN_IF_ERROR(gadget_file.GetDirectory(gadget_dir_path));
	OpSharedPtr<OpGadgetClass> gadget_class(
			g_gadget_manager->CreateClassWithPath(gadget_dir_path, URL_GADGET_INSTALL_CONTENT, NULL, error_reason));
	if (NULL == gadget_class.get())
	{
		return OpStatus::OK;
	}

	context.reset(OP_NEW(GadgetInstallerContext, ()));
	OP_ASSERT(NULL != context.get());
	if (NULL == context.get())
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	OpString normalized_name;
	RETURN_IF_ERROR(normalized_name.Set(display_name));
	PlatformGadgetUtils::Normalize(normalized_name);

	context->m_gadget_class = gadget_class;
	RETURN_IF_ERROR(context->m_display_name.Set(display_name));
	RETURN_IF_ERROR(context->m_normalized_name.Set(normalized_name));
	RETURN_IF_ERROR(context->m_profile_name.Set(
				install_config->GetProfileName()));
	RETURN_IF_ERROR(context->m_dest_dir_path.Set(install_dir_path));

	return OpStatus::OK;
}


OP_STATUS WindowsGadgetList::ReadAppInfo(const OpStringC& app_key_name,
		OpString& display_name, OpString& install_dir_path) const
{
	DWORD opera_widget = 0;
	if (ERROR_SUCCESS != OpRegReadDWORDValue(
				HKEY_CURRENT_USER, app_key_name, GetRegKeyMarkerValueName(),
				&opera_widget)
			|| 0 == opera_widget)
	{
		return OpStatus::ERR;
	}

	RETURN_IF_ERROR(OpSafeRegQueryValue(HKEY_CURRENT_USER, app_key_name.CStr(),
				display_name, UNI_L("DisplayName")));

	OpString reg_install_dir_path;
	RETURN_IF_ERROR(OpSafeRegQueryValue(HKEY_CURRENT_USER, app_key_name.CStr(),
				reg_install_dir_path, UNI_L("InstallLocation")));
	RETURN_IF_ERROR(OpPathDirFileCombine(install_dir_path,
				m_gadget_path_prefix, reg_install_dir_path));

	return OpStatus::OK;
}


OP_STATUS WindowsGadgetList::GetModificationTime(time_t& mtime) const
{
	HKEY handle = 0;
	if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_CURRENT_USER, GetListRegKeyName(), 0,
				KEY_READ, &handle))
	{
		return OpStatus::ERR;
	}
	OP_ASSERT(0 != handle);
	Finalizer finalizer(BindableFunction(&RegCloseKey, handle));

	FILETIME filetime;
	if (ERROR_SUCCESS != RegQueryInfoKey(handle, NULL, NULL, 0, NULL, NULL,
				NULL, NULL, NULL, NULL, NULL, &filetime))
	{
		return OpStatus::ERR;
	}
	mtime = ToTimeT(filetime);

	return OpStatus::OK;
}


time_t WindowsGadgetList::ToTimeT(const FILETIME& filetime)
{
	// FILETIME -> time_t conversion constants based on
	// http://support.microsoft.com/kb/167296
	static const UINT64 EPOCH_SHIFT = 116444736000000000LL;
	static const UINT32 RESOLUTION_MULT = 10000000;

	ULARGE_INTEGER filetime64;
	filetime64.LowPart = filetime.dwLowDateTime;
	filetime64.HighPart = filetime.dwHighDateTime;

	return (filetime64.QuadPart - EPOCH_SHIFT) / RESOLUTION_MULT;
}

#endif // WIDGET_RUNTIME_SUPPORT
