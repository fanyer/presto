/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "unix_gadgetlist.h"

#include "modules/gadgets/OpGadgetManager.h"
#include "modules/pi/system/OpFolderLister.h"
#include "modules/util/path.h"
#include "modules/util/OpHashTable.h"

#include "adjunct/desktop_util/algorithms/opunique.h"
#include "adjunct/widgetruntime/pi/PlatformGadgetUtils.h"

namespace
{
	const char OPERA_WIDGET_PREFIX[] = "opera-widget-";
	const uni_char* GLOBAL_GADGETS_DIR_PATH = UNI_L("/usr/share/");
	const uni_char* GADGET_DIR_PATTERN = UNI_L("opera-widget-*");
};

BOOL UnixGadgetList::HasChangedSince(time_t last_update_time) const
{
	// Create paths to gadgets installation directories, local and global one.

	OpString local_gadgets_dir_path, global_gadgets_dir_path;
	OpString tmp_storage;
	OpPathDirFileCombine(local_gadgets_dir_path,
			OpStringC(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_DEFAULT_GADGET_FOLDER, tmp_storage)),
			OpStringC(UNI_L("usr/share/")));
	global_gadgets_dir_path.Set(GLOBAL_GADGETS_DIR_PATH);

	// Obtain last modification times.

	time_t global_gadgets_modf_time;
	time_t local_gadgets_modf_time;

	OpFile dir;
	dir.Construct(local_gadgets_dir_path);
	dir.GetLastModified(local_gadgets_modf_time);
	dir.Construct(global_gadgets_dir_path);
	dir.GetLastModified(global_gadgets_modf_time);

	return local_gadgets_modf_time >= last_update_time ||
			global_gadgets_modf_time >= last_update_time;
}


OP_STATUS UnixGadgetList::GetAll(
		OpAutoVector<GadgetInstallerContext>& gadget_contexts,
		BOOL list_globally_installed_gadgets) const
{
	// Create paths to gadgets installation directories, local and global one.

	OpString local_gadgets_dir_path, global_gadgets_dir_path;
	OpString default_gadget_folder;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_DEFAULT_GADGET_FOLDER, default_gadget_folder));
	RETURN_IF_ERROR(OpPathDirFileCombine(local_gadgets_dir_path,
			OpStringC(default_gadget_folder.CStr()),
			OpStringC(UNI_L("usr/share/"))));
	RETURN_IF_ERROR(global_gadgets_dir_path.Set(GLOBAL_GADGETS_DIR_PATH));

	// Create directory listers.

	OpFolderLister* dir_lister = NULL;

	RETURN_IF_ERROR(OpFolderLister::Create(&dir_lister));
	OpAutoPtr<OpFolderLister> local_gadgets_dir_lister(dir_lister);
	dir_lister = NULL;
	RETURN_IF_ERROR(OpFolderLister::Create(&dir_lister));
	OpAutoPtr<OpFolderLister> global_gadgets_dir_lister(dir_lister);
	dir_lister = NULL;

	RETURN_IF_ERROR(local_gadgets_dir_lister->Construct(
			local_gadgets_dir_path.CStr(), 
			GADGET_DIR_PATTERN));
	RETURN_IF_ERROR(global_gadgets_dir_lister->Construct(
			global_gadgets_dir_path.CStr(), 
			GADGET_DIR_PATTERN));

	// Create set container to which local gadget names will be added,
	// while listing global gadgets we need to look out to not create 
	// context for global gadget with the same name as already existing 
	// local one. Generally local gadgets overwrite global gadgets.

	OpAutoStringHashTable<OpString> gadgets_names;

	while (local_gadgets_dir_lister->Next())
	{
		if (!local_gadgets_dir_lister->IsFolder())
		{
			continue;
		}

		OpString gadgets_dir_full_path;
		gadgets_dir_full_path.Set(local_gadgets_dir_lister->GetFullPath());

		if (!g_gadget_manager->IsThisAGadgetPath(gadgets_dir_full_path))
		{
			continue;
		}

		OpString* name = OP_NEW(OpString, ());
		RETURN_OOM_IF_NULL(name);
		OP_STATUS ret;
		if (OpStatus::IsError(ret = name->Set(local_gadgets_dir_lister->GetFileName()))
				|| OpStatus::IsError(ret = gadgets_names.Add(name->CStr(), name)))
		{
			OP_DELETE(name);
			return ret;
		}

		OpAutoPtr<GadgetInstallerContext> context(
				OP_NEW(GadgetInstallerContext, ()));

		OP_ASSERT(NULL != context.get());

		RETURN_OOM_IF_NULL(context.get());

		OP_STATUS status = CreateGadgetContext(gadgets_dir_full_path, context);
		switch(status)
		{
			case OpStatus::ERR:
				// fake gadget, carry on
				context.release();
				continue;

			case OpStatus::ERR_NO_MEMORY: 
				return status;

			case OpStatus::OK:
				status = gadget_contexts.Add(context.get());
				context.release();
				RETURN_IF_ERROR(status);
				break;
		}
	}

	if (list_globally_installed_gadgets)
	{
		while (global_gadgets_dir_lister->Next())
		{
			if (!global_gadgets_dir_lister->IsFolder())
			{
				continue;
			}

			OpString gadgets_dir_full_path;
			RETURN_IF_ERROR(gadgets_dir_full_path.Set(
					global_gadgets_dir_lister->GetFullPath()));

			if (!g_gadget_manager->IsThisAGadgetPath(gadgets_dir_full_path))
			{
				continue;
			}

			if (gadgets_names.Contains(global_gadgets_dir_lister->GetFileName()))
			{
				continue;
			}

			OpAutoPtr<GadgetInstallerContext> context(
					OP_NEW(GadgetInstallerContext, ()));

			OP_ASSERT(NULL != context.get());

			RETURN_OOM_IF_NULL(context.get());

			OP_STATUS status = CreateGadgetContext(gadgets_dir_full_path, context);
			switch(status)
			{
				case OpStatus::ERR:
					// fake gadget, carry on
					context.release();
					continue;

				case OpStatus::ERR_NO_MEMORY: 
					return status;

				case OpStatus::OK:
					status = gadget_contexts.Add(context.get());

					// Mark that this gadget is installed globally.

					context->m_install_globally = TRUE;

					context.release();
					RETURN_IF_ERROR(status);
					break;
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS UnixGadgetList::CreateGadgetContext(const OpStringC& gadget_path,
	OpAutoPtr<GadgetInstallerContext>& context) const
{
	OpString error_reason;
	OpSharedPtr<OpGadgetClass> gadget_class(
			g_gadget_manager->CreateClassWithPath(gadget_path, URL_GADGET_INSTALL_CONTENT, NULL, error_reason));
	if (NULL == gadget_class.get())
	{
		return OpStatus::ERR;
	}
	// now we are sure, that this is real gadget now
	
	// gadget class
	context->m_gadget_class = gadget_class;

	// gadget directory path
	RETURN_IF_ERROR(context->m_dest_dir_path.Set(gadget_path));

	// profile name
	OpFile share_widget_dir; 
	RETURN_IF_ERROR(share_widget_dir.Construct(gadget_path.CStr()));
	RETURN_IF_ERROR(context->m_profile_name.Set(share_widget_dir.GetName()));

	// display name
	RETURN_IF_ERROR(context->m_display_name.Set(context->GetDeclaredName()));
	OpString suffix;
	RETURN_IF_ERROR(GetStatusSuffix(context->m_profile_name, suffix));
	RETURN_IF_ERROR(context->m_display_name.Append(suffix));

	// normalized name
	RETURN_IF_ERROR(context->m_normalized_name.Set(context->m_display_name));
	PlatformGadgetUtils::Normalize(context->m_normalized_name);

	return OpStatus::OK;
}

OP_STATUS UnixGadgetList::GetStatusSuffix(const OpStringC& profile_name, OpString& suffix) const
{
	suffix.Empty();

	int num_str_pos = profile_name.FindLastOf('-');
	if (num_str_pos == KNotFound)
		return OpStatus::OK;

	OpString8 num_str;
	RETURN_IF_ERROR(num_str.Set(profile_name.SubString(num_str_pos+1)));

	char* end_ptr = NULL;
	const unsigned long num = op_strtoul(num_str.CStr(), &end_ptr, 10);
	if (*end_ptr == '\0' && num > 0)
		RETURN_IF_ERROR(suffix.AppendFormat(OpUnique::DEFAULT_SUFFIX_FORMAT, num));

	return OpStatus::OK;
}


OP_STATUS PlatformGadgetList::Create(PlatformGadgetList** gadget_list)
{
	OP_ASSERT(NULL != gadget_list);
	if (NULL == gadget_list)
	{
		return OpStatus::ERR;
	}

	*gadget_list = OP_NEW(UnixGadgetList, ());
	RETURN_OOM_IF_NULL(*gadget_list);

	return OpStatus::OK;
}

#endif // WIDGET_RUNTIME_SUPPORT
