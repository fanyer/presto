/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "unix_gadgetutils.h"
#include "unix_gadgetuninstaller.h"

#include "adjunct/desktop_util/adt/finalizer.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/widgetruntime/GadgetInstallerContext.h"
#include "adjunct/widgetruntime/pi/PlatformGadgetUtils.h"
#include "adjunct/desktop_util/transactions/OpTransaction.h"

// #include "platforms/utilix/unixprocess.h"
#include "platforms/posix/posix_native_util.h"

#include "modules/pi/OpSystemInfo.h"
#include "modules/util/path.h"
#include "modules/util/zipload.h"

#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"

#if defined(__linux__)
#include <sys/prctl.h>
#endif // __linux__

#include <ctype.h>

OP_STATUS PlatformGadgetUtils::GetOperaExecPath(OpString& exec)
{
#if 0
	return exec->Set(UNI_L("opera"));
#else

	/* (pobara)
	 * returns absolute path to opera binary
	 *
	 * before condemning me to hell for writing this ugly hack
	 * please refer to DSK-252286 and DSK-249882
	 *
	 * this is only temporary solution because launchers
	 * created this way can easily become unusable
	 */
	// return UnixProcess::GetExecPath(&exec);

	// tmp hack, while waiting for patches in utilix
	OpString* exec_ptr = &exec;

	OP_ASSERT(exec_ptr);
	char buf[PATH_MAX];
	int  endpos;

#if defined(__FreeBSD__) || defined(__NetBSD__)
	// *bsd
	char procfs_link[PATH_MAX];
	snprintf(procfs_link, PATH_MAX, "/proc/%i/file", getpid());
    endpos = readlink(procfs_link, buf, sizeof(buf));
#else
	// linux
    endpos = readlink("/proc/self/exe", buf, sizeof(buf));
#endif
	if (endpos < 0)
		return OpStatus::ERR;

	buf[endpos] = '\0';
	exec_ptr->Set(buf); 

#endif // 0
	return OpStatus::OK;
}

OP_STATUS PlatformGadgetUtils::GetImpliedGadgetFilePath(OpString& path) 
{ 
	return OpStatus::ERR; 
}

OP_STATUS PlatformGadgetUtils::GetGadgetProfileName(const OpStringC& gadget_path, 
		OpString& profile_name)
{
	// FIXME: Find a more foolproof solution.  This is just a convoluted way of
	// requiring gadgets to be launched with the "-pd" switch (which happens to
	// be used by the launcher script created upon gadget installation).
	if (NULL == CommandLineManager::GetInstance()->GetArgument(
				CommandLineManager::PersonalDirectory))
	{
		fprintf(stderr, "opera: personal directory (-pd) argument is needed to start widget\n");
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}


// Use PosixFileUtil::RealPath() under #ifdef POSIX_CAP_REALPATH
// Check if this is still used somewhere, waiting for refactoring times. 

OP_STATUS PlatformGadgetUtils::GetAbsolutePath(IN const OpString& file_path,
	OUT OpString& absolute_path)
{
	/* Check how long a path can be. */
	long path_max = pathconf("/", _PC_PATH_MAX);
	if (path_max <= 0) path_max = 4096;

	/* Allocate memory for absolute path and relative path. */
	OpAutoArray<char> abs_path_buf(OP_NEWA(char, path_max));
	if (NULL == abs_path_buf.get())
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	OpAutoArray<char> file_path_buf(OP_NEWA(char, path_max));
	if (NULL == file_path_buf.get())
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	file_path.UTF8(file_path_buf.get(), path_max);

	/* Check if file exists. */
	struct stat buf;
	if ((-1) == stat(file_path_buf.get(), &buf))
	{
		return OpStatus::ERR_FILE_NOT_FOUND;
	}
		
	/* Find absolute path to file. */
	if (NULL == realpath(file_path_buf.get(), abs_path_buf.get()))
	{
		return OpStatus::ERR;
	}
	absolute_path.Set(abs_path_buf.get());

	return OpStatus::OK;
}

/**
 * Normalize name.
 * Replace spaces with minuses, lower all letters and remove unsupported chars.
 * Result should be easy-to-read, easy-to-write gadget name, that can be used
 * to construct paths, process and profile names.
 */
void PlatformGadgetUtils::Normalize(OpString& name)
{
    const uni_char default_char = '-'; 
 
    // If it's empty string we won't do anything. 
    if (name.Length() <= 0) 
    { 
        return; 
    } 
 
    // First phase of normalization. 
    for (int i = 0; i < name.Length(); ++i) 
    { 
        if (!OpPathIsValidFileNameChar(name[i]))
        {
            name[i] = default_char;
        }
    }
 
    // Second phase of normalization. 
    int idx1 = 0, idx2 = 0; 

    // Skip trailing/leading default_char. 
    idx1 = 0; 
    while (name[idx1] == default_char) name[idx1++] = ' '; 
    idx1 = name.Length()-1; 
    while (name[idx1] == default_char) name[idx1--] = ' '; 
    name.Strip(); 
 
    // Replace inner default_char strings to one  
    // default_char character. 
    OpString output; 
    output.Reserve(name.Length()); 

    idx1 = idx2 = 0; 
    while (idx1 < name.Length()) 
    {
		if( name[idx1] == default_char )
		{
            while (name[idx1] == default_char && idx1 < name.Length())  
            {
                idx1++; 
            }
            output[idx2++] = default_char; 
		}
		if ( name[idx1] == '('  || name[idx1] == ')' ||
			 name[idx1] == '\'' || name[idx1] == '\"' )
		{
			idx1++;
		}
        else // name[idx1] != default_char
        { 
			OP_ASSERT( name[idx1] != default_char );
            output[idx2++] = uni_tolower(name[idx1++]); 
        }
    }
    name.Set(output, idx2); 
}

OP_STATUS UnixGadgetUtils::CreateGadgetPackage(const uni_char* wgt_file_path,
		const uni_char* package_type,
		FILE** pipe,
		const uni_char* dst_dir_path,
		const uni_char* output_file_name,
		BOOL blocking_call)
{
	// Get base path where scripts are

	OpString package_folder;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_PACKAGE_FOLDER, package_folder));
	if (NULL == package_folder.CStr())
	{
		return OpStatus::ERR;
	}
	
	// Create path to lib subdirectory

	OpString lib_dir_path;
	RETURN_IF_ERROR(lib_dir_path.Set(package_folder.CStr()));
	RETURN_IF_ERROR(lib_dir_path.Append("/"));
	RETURN_IF_ERROR(lib_dir_path.Append(UNI_L("lib")));

	// The INCDIR points to dir with packaging scripts

	PosixNativeUtil::NativeString lib_dir_native_path(
			lib_dir_path.CStr());
	if (NULL == lib_dir_native_path.get()) 
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	// Set INCDIR to lib path, create finalizer object which will 
	// unset INCDIR variable if error occurs 

	const char* ENV_VAR_NAME = "INCDIR";
	Finalizer unset_env_var(BindableFunction(&unsetenv, ENV_VAR_NAME));
	setenv(ENV_VAR_NAME, lib_dir_native_path.get(), 1);

	// Create path to main script.

	OpString main_script_path;
	RETURN_IF_ERROR(main_script_path.Set(package_folder.CStr()));
	RETURN_IF_ERROR(main_script_path.Append("/"));
	RETURN_IF_ERROR(main_script_path.Append(UNI_L("main.sh")));

	// Prepare command

	OpString command;
	RETURN_IF_ERROR(command.AppendFormat(UNI_L("sh %s \"%s\""), 
		main_script_path.CStr(), wgt_file_path));

	// Append output directory if present

	if (dst_dir_path)
	{
		RETURN_IF_ERROR(command.AppendFormat(UNI_L(" --dest %s "), 
			dst_dir_path));
	}

	// Change output file name

	if (output_file_name)
	{
		RETURN_IF_ERROR(command.AppendFormat(UNI_L(" --override-name %s "), 
			output_file_name));
	}

	// Append package format

	RETURN_IF_ERROR(command.AppendFormat(UNI_L(" --format %s "), package_type));

	// Encode command in local encoding

	PosixNativeUtil::NativeString command_native(command.CStr());
	if (NULL == command_native.get()) 
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	// Execute script and give back pipe for communication

	*pipe = popen(command_native.get(), "r");
	if (NULL == *pipe)
	{
		if (ENOMEM == errno) 
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		else 
		{
			return OpStatus::ERR;
		}
	}

	// If was invoked with blocking set to TRUE wait till the package
	// is created

	if (blocking_call)
	{
		if (pclose(*pipe) == (-1))
		{
			return OpStatus::ERR;
		}
	}

	return OpStatus::OK;
}

OP_STATUS UnixGadgetUtils::NormalizeName(IN const OpStringC& src, OUT OpString& dest)
{
	RETURN_IF_ERROR(dest.Set(src));
	dest.Strip();

	// Cast down to char*

	char* tmp = uni_down_strdup(dest.CStr());
	if (NULL == tmp)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	// Pointer iterator

	char* iter = tmp;

	// For normalized profile name
	// TODO: 512? 

	char output[512];
	memset(output, 0, sizeof(char) * 512);

	UINT32 idx = 0;

	while ((idx < 512) && *iter)
	{
		if (isalnum(*iter))
		{
			output[idx++] = tolower(*iter);
			iter++;
		}
		else
		{
			while (*iter && !isalnum(*iter)) 
			{
				iter++;
			}
			output[idx++] = '-';
		}
	}

	// Strip trailing/leading '-' characters

	idx = 0;
	while (output[idx] == '-') output[idx++] = ' ';
	idx = strlen(output)-1;
	while (output[idx] == '-') output[idx--] = ' ';

	// Strip one more time

	op_free(tmp);
	RETURN_IF_ERROR(dest.Set(output));
	dest.Strip();

	return OpStatus::OK;
}


OP_STATUS UnixGadgetUtils::GetWidgetName(IN const OpString& filename, OUT OpString& name)
{
	OpFile file;
	RETURN_IF_ERROR(file.Construct(filename));
	RETURN_IF_ERROR(file.Open(OPFILE_READ));
	XMLFragment fragment;
	if (OpStatus::IsError(fragment.Parse(&file)))
		return OpStatus::ERR;

	for (int i=0; i<2; i++)
	{
		fragment.RestartFragment();
		// We have to parse with and without xml namespace
		fragment.EnterElement(XMLExpandedName(i==0 ? 0 : UNI_L("http://www.w3.org/ns/widgets"), UNI_L("widget")));
		while (fragment.EnterAnyElement())
		{
			OpStringC element_name(fragment.GetElementName().GetLocalPart());
			if (!element_name.Compare(UNI_L("widgetname")) || !element_name.Compare(UNI_L("name")))
				return name.Set(fragment.GetText());
			fragment.LeaveElement();
		}
	}

	name.Empty();

	return OpStatus::OK; // But no match
}

OP_STATUS PlatformGadgetUtils::InstallGadget(const OpStringC& path)
{
	OpString program, arguments;
	
	RETURN_IF_ERROR(PlatformGadgetUtils::GetOperaExecPath(program));
	RETURN_IF_ERROR(arguments.AppendFormat(UNI_L("-widget \"%s\""),
										   path.CStr()));
	
	g_application->ExecuteProgram(program.CStr(), arguments.CStr());
	
	return OpStatus::OK;	
}

OP_STATUS PlatformGadgetUtils::ExecuteGadget(
		const GadgetInstallerContext& context)
{
	OpString bin_path;
	if (context.m_install_globally)
	{
		// If we install a gadget globally its executable script
		// goes to /usr/bin/.

		bin_path.Set("/usr/bin/");
	}
	else
	{
		OpString default_gadget_folder;
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_DEFAULT_GADGET_FOLDER, default_gadget_folder));
		RETURN_IF_ERROR(OpPathDirFileCombine(bin_path,
				OpStringC(default_gadget_folder.CStr()),
				OpStringC(UNI_L("bin"))));
	}
	OpString wrapper_path;
	RETURN_IF_ERROR(OpPathDirFileCombine(wrapper_path,
		OpStringC(bin_path.CStr()),
		OpStringC(context.m_profile_name.CStr())));
	RETURN_IF_ERROR(g_op_system_info->ExecuteApplication(
		wrapper_path.CStr(), NULL));
	return OpStatus::OK;
}


OP_STATUS PlatformGadgetUtils::UninstallGadget(
		const GadgetInstallerContext& context)
{
	UnixGadgetUninstaller uninstaller;
	RETURN_IF_ERROR( uninstaller.Init(context) );
	return uninstaller.Uninstall();
}
#ifdef WIDGETS_UPDATE_SUPPORT
OP_STATUS PlatformGadgetUtils::UpdateGadget(const GadgetInstallerContext& gctx)
{
	// We need to create a new context with overrided m_dest_dir_path, because
	// m_dest_dir_path in gctx points to widget directory as obtained through
	// PlatformGadgetList implementation on UNIX. We don't need direct widget
	// directory to sort of reinstall widget, we just need directory under which
	// widgets are installed which is by default ~/.opera-widgets. 

	GadgetInstallerContext new_gctx(gctx);
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_DEFAULT_GADGET_FOLDER,
	                                                new_gctx.m_dest_dir_path));
	
	// Update is just reinstall, it will replace all necessary files including:
	// wrapper script, resources located in ~/.opera-widgets/usr/share/<widget-name> 
	// and shortcuts (widget's icon can change after update).

	UnixGadgetInstaller installer;
	RETURN_IF_ERROR(installer.Init(new_gctx));
	OpTransaction tx;
	OpString exec_cmd;
	RETURN_IF_ERROR(installer.Install(tx, exec_cmd));
	RETURN_IF_ERROR(tx.Commit());

	return OpStatus::OK;
}
#endif // WIDGETS_UPDATE_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT
