/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT
#include <unistd.h>

#include "desktop_entry.h"
#include "unix_gadgetuninstaller.h"

#include "adjunct/desktop_util/shortcuts/DesktopShortcutInfo.h"
#include "adjunct/widgetruntime/GadgetInstallerContext.h"
#include "adjunct/widgetruntime/pi/PlatformGadgetUtils.h"

#include "modules/util/opfile/opfile.h"
#include "platforms/posix/posix_native_util.h"

UnixGadgetUninstaller::UnixGadgetUninstaller()
	: m_context(NULL)
{
}


OP_STATUS UnixGadgetUninstaller::Init(const GadgetInstallerContext& context)
{
	m_context = &context;
	return OpStatus::OK;
}


const OpStringC& UnixGadgetUninstaller::GetGadgetDir() const
{
	OP_ASSERT(NULL != m_context);
	return m_context->m_dest_dir_path;
}

const OpStringC& UnixGadgetUninstaller::GetInstallationName() const
{
	OP_ASSERT(NULL != m_context);
	return m_context->m_profile_name;
}


OP_STATUS UnixGadgetUninstaller::Uninstall()
{
	RETURN_IF_ERROR(RemoveShareDir());
	// if removing share dir suceeded, then we should return status OK, no matter if
	// removing wrapper script and desktop entry also succeeded
	OpStatus::Ignore(UnlinkWrapperScript());
	OpStatus::Ignore(RemoveWrapperScript());
	OpStatus::Ignore(RemoveDesktopEntries());

	return OpStatus::OK;
}


OP_STATUS UnixGadgetUninstaller::RemoveShareDir()
{
	OpFile share; 
	share.Construct(GetGadgetDir().CStr());
	RETURN_IF_ERROR(share.Delete(TRUE));
	return OpStatus::OK; 
}


OP_STATUS UnixGadgetUninstaller::RemoveWrapperScript()
{
	OpString wrapper_path;
	OpString tmp_storage;
	g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_DEFAULT_GADGET_FOLDER, tmp_storage);
	RETURN_IF_ERROR(wrapper_path.Set(tmp_storage));
	RETURN_IF_ERROR(wrapper_path.Append("bin/"));
	RETURN_IF_ERROR(wrapper_path.Append(GetInstallationName().CStr()));
#ifdef _DEBUG
	OpString8 wrapper_path8;
	RETURN_IF_ERROR(wrapper_path8.Set(wrapper_path));
	printf("wrapper location: %s\n", wrapper_path8.CStr());
#endif	
	OpFile wrapper; 
	RETURN_IF_ERROR(wrapper.Construct(wrapper_path.CStr()));
	RETURN_IF_ERROR(wrapper.Delete());

	return OpStatus::OK; 
}


OP_STATUS UnixGadgetUninstaller::RemoveDesktopEntries()
{
	OP_ASSERT(NULL != m_context);

	DesktopShortcutInfo shortcut_info;
	shortcut_info.m_destination = DesktopShortcutInfo::SC_DEST_MENU;
	RETURN_IF_ERROR(shortcut_info.m_file_name.Set(GetInstallationName()));

	DesktopEntry desktop_entry;
	RETURN_IF_ERROR(desktop_entry.Init(shortcut_info));
	RETURN_IF_ERROR(desktop_entry.Remove());

	return OpStatus::OK;
}


OP_STATUS UnixGadgetUninstaller::UnlinkWrapperScript()
{
	OpString link_path;
	RETURN_IF_ERROR(link_path.Set(op_getenv("HOME")));
	RETURN_IF_ERROR(link_path.Append("/bin/" ));
	RETURN_IF_ERROR(link_path.Append(GetInstallationName().CStr()));

	OpFile link;
	RETURN_IF_ERROR(link.Construct(link_path.CStr()));
	RETURN_IF_ERROR(link.Delete(FALSE));

	return OpStatus::OK;
}


#endif // WIDGET_RUNTIME_SUPPORT
