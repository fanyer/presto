/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski@opera.com)
 * @author Blazej Kazmierczak (bkazmierczak@opera.com)
 */
 
#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_util/file_utils/folder_recurse.h"
#include "adjunct/desktop_util/transactions/OpTransaction.h"
#include "adjunct/widgetruntime/GadgetConfigFile.h"
#include "adjunct/widgetruntime/GadgetInstallerContext.h"
#include "adjunct/widgetruntime/pi/PlatformGadgetUtils.h"


#include "modules/gadgets/OpGadgetClass.h"
#include "modules/util/path.h"
#include "modules/util/opfile/unistream.h"

#include "platforms/windows/user_fun.h"
#include "platforms/windows/WindowsGadgetUtils.h"
#include "platforms/windows/WindowsLaunch.h"


OP_STATUS PlatformGadgetUtils::GetGadgetProfileName(
		const OpStringC& gadget_path, OpString& profile_name)
{ 
	OpAutoPtr<GadgetConfigFile> config_file(GadgetConfigFile::Read(gadget_path));
	if (NULL == config_file.get())
	{
		return OpStatus::ERR;
	}
	RETURN_IF_ERROR(profile_name.Set(config_file->GetProfileName()));

	return OpStatus::OK; 
}


OP_STATUS PlatformGadgetUtils::GetOperaExecPath(OpString& exec)
{ 
	OP_ASSERT(g_op_system_info);
	return g_op_system_info->GetBinaryPath(&exec);
}


OP_STATUS PlatformGadgetUtils::GetImpliedGadgetFilePath(OpString& path) 
{
	uni_char module_file_path[MAX_PATH];
	const DWORD length = GetModuleFileName(NULL, module_file_path, MAX_PATH);
	if (0 == length || MAX_PATH == length)
	{
		return OpStatus::ERR;
	}

	uni_char* module_file_name = OpPathGetFileName(module_file_path);
	if (0 == OpStringC(module_file_name).CompareI(UNI_L("opera.exe")))
	{
		return OpStatus::ERR;
	}

	uni_char* gadget_dir_path = module_file_path;
	OpPathRemoveFileName(gadget_dir_path);
	
	// Look for GADGET_CONFIGURATION_FILE in the file tree under the
	// installation directory.
	RETURN_IF_ERROR(WindowsGadgetUtils::FindGadgetFile(gadget_dir_path, path));

	return OpStatus::OK;
}


void PlatformGadgetUtils::Normalize(OpString& name)
{
    static const uni_char default_char = '_'; 

    // First phase of normalization 
 
    for (int i = 0; i < name.Length(); ++i) 
    { 
        if (!OpPathIsValidFileNameChar(name[i])) 
        { 
            name[i] = default_char; 
        } 
    } 
 
    // Second phase of normalization 

    // Skip trailing/leading default_char 

	for (int idx = 0; idx < name.Length() &&
			name[idx] == default_char; ++idx)
	{
		name[idx] = ' ';
	}
 
	for (int idx = name.Length() - 1; idx >= 0 && 
			name[idx] == default_char; --idx)
	{
		name[idx] = ' ';
	}

	name.Strip();
 
    // Replace inner default_char strings to one  
    // default_char character. 
 
    OpString output; 
 
    for (int idx = 0; idx < name.Length(); ) 
    { 
        if (name[idx] != default_char) 
        { 
			output.Append(&name[idx++], 1);
        } 
        else 
        { 
            while (name[idx] == default_char && idx < name.Length())  
            { 
                idx++; 
            } 
 
			output.Append(&default_char, 1);
        } 
    } 

	// Because this function is mainly used to create proper widget
	// file/directory names on platforms (windows in this case) we
	// have to remove trailing dots '.' and spaces ' ' from names because
	// windows developer center says "Do not end a file or directory name
	// with a space or period"

	for (int idx = output.Length() - 1; idx >= 0 && 
			(output[idx] == '.' || output[idx] == ' '); --idx)
	{
		output[idx] = '\0';
	}
 
    name.Set(output); 
}


OP_STATUS PlatformGadgetUtils::GetAbsolutePath(const OpString& file_path,
		OpString& absolute_path)
{
	const int abs_pathname_length =
			GetFullPathName(file_path, 0, NULL, NULL) - 1;
	if (0 != abs_pathname_length)
	{
		if (NULL != absolute_path.Reserve(abs_pathname_length))
		{
			const int actual_length =
					GetFullPathName(file_path, abs_pathname_length + 1,
							absolute_path.DataPtr(), NULL);
			if (0 != actual_length)
			{
				OP_ASSERT(abs_pathname_length == actual_length);
				OP_ASSERT(abs_pathname_length == absolute_path.Length());
				return OpStatus::OK;
			}
		}
	}
	return OpStatus::ERR;
}

OP_STATUS PlatformGadgetUtils::InstallGadget(const OpStringC& path)
{
	OpString program;
	RETURN_IF_ERROR(GetOperaExecPath(program));

	OpString8 arguments;
	RETURN_IF_ERROR(arguments.SetUTF8FromUTF16(path));
	const char* argv[] = { "-widget", arguments.CStr() };
	
	WindowsLaunchPI launcher;
	const BOOL result = launcher.Launch(program.CStr(), ARRAY_SIZE(argv),
			argv);
	
	return result ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS PlatformGadgetUtils::ExecuteGadget(
		const GadgetInstallerContext& context)
{
	OpString gadget_starter_path;
	RETURN_IF_ERROR(WindowsGadgetUtils::MakeGadgetStarterPath(
				context, gadget_starter_path));

	WindowsLaunchPI launcher;
	const BOOL result = launcher.Launch(gadget_starter_path.CStr(), 0, NULL);
	
	return result ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS PlatformGadgetUtils::UninstallGadget(
		const GadgetInstallerContext& context)
{
	return WindowsGadgetUtils::Uninstall(context);
}

OP_STATUS WindowsGadgetUtils::Uninstall(const GadgetInstallerContext& context, BOOL uninstall_for_update)
{
	OpString uninstaller_path;
	RETURN_IF_ERROR(WindowsGadgetUtils::MakeGadgetUninstallerPath(
		context, uninstaller_path));

	OpString8 arguments;
	arguments.SetUTF8FromUTF16(uninstaller_path);

	const char* argv[] = { arguments.CStr(), uninstall_for_update ? "/u" : "/f" };

	WindowsLaunchPI launcher;
	BOOL result;
	if (uninstall_for_update)
	{
		result = launcher.LaunchAndWait(UNI_L("wscript"), ARRAY_SIZE(argv),
			argv);
	}
	else
	{
		result = launcher.Launch(UNI_L("wscript"), ARRAY_SIZE(argv),
			argv);
	}

	return result ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS WindowsGadgetUtils::FindGadgetFile(const OpStringC& dir_path,
		OpString& gadget_file_path)
{
	OpString root_folder;
	RETURN_IF_ERROR(root_folder.Set(dir_path));
	FolderRecursor recursor(1);
	RETURN_IF_ERROR(recursor.SetRootFolder(root_folder));
	OpFile* file = NULL;
	do
	{
		file = NULL;
		RETURN_VALUE_IF_ERROR(recursor.GetNextFile(file), FALSE);
		if (NULL != file)
		{
			OpAutoPtr<OpFile> file_holder(file);
			if (OpStringC(GADGET_CONFIGURATION_FILE) == file->GetName())
			{
				RETURN_IF_ERROR(gadget_file_path.Set(file->GetFullPath()));
				return OpStatus::OK;
			}
		}
	} while (NULL != file);

	return OpStatus::ERR;
}


OP_STATUS WindowsGadgetUtils::MakeGadgetStarterPath(
		const GadgetInstallerContext& context, OpString& starter_path)
{
	RETURN_IF_ERROR(OpPathDirFileCombine(starter_path,
				context.m_dest_dir_path, context.m_normalized_name));
	RETURN_IF_ERROR(starter_path.Append(UNI_L(".exe")));
	return OpStatus::OK;
}


OP_STATUS WindowsGadgetUtils::MakeGadgetUninstallerPath(
		const GadgetInstallerContext& context, OpString& uninstaller_path)
{
	RETURN_IF_ERROR(OpPathDirFileCombine(uninstaller_path,
				context.m_dest_dir_path, OpStringC(UNI_L("uninstall.vbs"))));
	return OpStatus::OK;
}
 
#endif // WIDGET_RUNTIME_SUPPORT
