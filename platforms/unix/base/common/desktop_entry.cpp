/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * author: Patryk Obara
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/desktop_util/shortcuts/DesktopShortcutInfo.h"
#include "adjunct/widgetruntime/pi/PlatformGadgetUtils.h"
#include "modules/util/opfile/opfile.h"
#include "platforms/unix/base/common/desktop_entry.h"

// TODO: 
// - use path.h function (!)
// - check what happens, when directories does not exist

DesktopEntry::DesktopEntry()
	: m_shortcut_info(NULL)
{
}


OP_STATUS DesktopEntry::Init(const DesktopShortcutInfo& shortcut_info)
{
	if (DesktopShortcutInfo::SC_DEST_MENU != shortcut_info.m_destination)
	{
		OP_ASSERT(!"Unsupported shortcut destination");
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	m_shortcut_info = &shortcut_info;
	RETURN_IF_ERROR(PreparePath());

	return OpStatus::OK;
}


OP_STATUS DesktopEntry::Deploy()
{
	//printf("[%i]", info->dest);
	OP_ASSERT(m_shortcut_info);
	OP_ASSERT(m_path.HasContent());

	OpFile file;
	RETURN_IF_ERROR(file.Construct(m_path));
	RETURN_IF_ERROR(file.Open(OPFILE_OVERWRITE));

	// desktop entry content:

	RETURN_IF_ERROR(file.WriteUTF8Line(UNI_L("[Desktop Entry]")));
	RETURN_IF_ERROR(file.WriteUTF8Line(UNI_L("Version=1.0")));

	OpString type;
	RETURN_IF_ERROR(type.SetConcat(UNI_L("Type="), m_shortcut_info->m_type));
	RETURN_IF_ERROR(file.WriteUTF8Line(type));

	OpString generic_name;
	RETURN_IF_ERROR(generic_name.SetConcat(UNI_L("GenericName="),
				m_shortcut_info->m_generic_name));
	RETURN_IF_ERROR(file.WriteUTF8Line(generic_name));

	OpString name;
	RETURN_IF_ERROR(name.SetConcat(UNI_L("Name="), m_shortcut_info->m_name));
	RETURN_IF_ERROR(file.WriteUTF8Line(name));

	OpString exec;
	RETURN_IF_ERROR(exec.SetConcat(UNI_L("Exec="), m_shortcut_info->m_command));
	RETURN_IF_ERROR(file.WriteUTF8Line(exec));

	OpString tmpExec;
	PlatformGadgetUtils::GetOperaExecPath(tmpExec);
	OpString try_exec_string;
	try_exec_string.SetConcat(UNI_L("TryExec="), tmpExec);
	RETURN_IF_ERROR(file.WriteUTF8Line(try_exec_string));

	// setting icon
	OpString icon;
	RETURN_IF_ERROR(icon.SetConcat(UNI_L("Icon="),
				m_shortcut_info->m_icon_name));
	RETURN_IF_ERROR(file.WriteUTF8Line(icon));

	// no support for categories from widget spec means no categories here
	// RETURN_IF_ERROR(file.WriteUTF8Line(UNI_L("Categories=Application;Network")));

	// end of desktop entry file content

	return OpStatus::OK;
}


OP_STATUS DesktopEntry::Remove()
{
	OpFile file;
	RETURN_IF_ERROR(file.Construct(m_path));
	RETURN_IF_ERROR(file.Delete());
	return OpStatus::OK;
}


OP_STATUS DesktopEntry::PreparePath()
{
	OP_ASSERT(m_shortcut_info);

	switch (m_shortcut_info->m_destination)
	{
		case DesktopShortcutInfo::SC_DEST_MENU:
		{
			const char* xdg = getenv("XDG_DATA_HOME");
			if(xdg)
			{
				RETURN_IF_ERROR(m_path.Set(xdg));
			}
			else
			{
				RETURN_IF_ERROR(m_path.Set(getenv("HOME")));
				RETURN_IF_ERROR(m_path.Append(UNI_L("/.local/share")));
			}

			// in data direcory, .desktop files of any kind can appear,
			// but in certain locations
			RETURN_IF_ERROR(m_path.AppendFormat(
						UNI_L("/applications/%s.desktop"),
						m_shortcut_info->m_file_name.CStr()));
/*
	Disabling desktop entry type handling.  We only support Application, anyway.
	-- wdzierzanowski

			if(info->type == "Application")
			{
				path.AppendFormat(UNI_L("/applications/%s.desktop"), normalized_name.CStr());
			}
			else if(info->type == "Directory")
			{
				path.AppendFormat(UNI_L("/desktop-directories/%s.directory"), normalized_name.CStr());
			}
			else if(info->type == "Link")
			{
				path.AppendFormat(UNI_L("/%s.desktop"), normalized_name.CStr());
			}
*/
		}
		break;

		default:
			OP_ASSERT(!"shortcut destination not supported on platform!");
		break;
	}

#ifdef DEBUG
	// printf("desktop entry location: %s\n", uni_down_strdup(m_path.CStr()));
#endif

	return OpStatus::OK;
}


#endif // WIDGET_RUNTIME_SUPPORT
