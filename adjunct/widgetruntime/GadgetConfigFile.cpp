/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/widgetruntime/GadgetInstallerContext.h"
#include "adjunct/widgetruntime/GadgetConfigFile.h"
#include "modules/util/opautoptr.h"
#include "modules/util/path.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opfile/unistream.h"

#ifdef MSWIN
#include "platforms/windows/user_fun.h"
#endif


namespace
{
	const uni_char* FILE_NAME = UNI_L("install.conf");
}


GadgetConfigFile::GadgetConfigFile()
	: m_installer_context(OP_NEW(GadgetInstallerContext, ()))
{
}

GadgetConfigFile::GadgetConfigFile(
		const GadgetInstallerContext& installer_context)
	: m_installer_context(OP_NEW(GadgetInstallerContext, (installer_context)))
{
	OP_ASSERT(NULL != m_installer_context);
	OpStatus::Ignore(InitFile(*m_installer_context, m_file));
}

GadgetConfigFile::~GadgetConfigFile()
{
	OP_DELETE(m_installer_context);
}


const OpStringC& GadgetConfigFile::GetProfileName() const
{
	return m_installer_context->m_profile_name;
}


const OpFile& GadgetConfigFile::GetFile() const
{
	return m_file;
}


OP_STATUS GadgetConfigFile::InitFile(const GadgetInstallerContext& context,
		OpFile& file)
{
	OpString conf_file_path;
	RETURN_IF_ERROR(conf_file_path.Set(context.m_dest_dir_path));
#ifdef _MACINTOSH_
	// FIXME: avoid magic strings.
	RETURN_IF_ERROR(conf_file_path.Append("/"));
	RETURN_IF_ERROR(conf_file_path.Append(context.m_normalized_name));	
	RETURN_IF_ERROR(conf_file_path.Append(UNI_L(".app/Contents/Resources/widget/")));
	
	// And now - because this is a wrong place - we need to find the one and only - config.xml
	OpFolderLister *wgt_dir;
	BOOL hasConfig = false;
	OpString lastFolder;
	
	RETURN_IF_ERROR(OpFolderLister::Create(&wgt_dir));
	RETURN_IF_ERROR(wgt_dir->Construct(conf_file_path.CStr(), UNI_L("*")));
	while (wgt_dir->Next() && !hasConfig)
	{
		if (wgt_dir->IsFolder())
		{
			lastFolder.Set(wgt_dir->GetFileName());
		}
		else 
		{
			OpString filename;
			filename.Set(wgt_dir->GetFileName());
			if (filename == UNI_L("config.xml"))
			{
				hasConfig = true;
			}
		}
		
	}
	
	if (!hasConfig)
	{
		conf_file_path.Append(lastFolder.CStr());
		conf_file_path.Append("/");
	}
#endif

	// FIXME: avoid magic strings.
	RETURN_IF_ERROR(OpPathDirFileCombine(conf_file_path, conf_file_path,
			OpStringC(FILE_NAME)));

	RETURN_IF_ERROR(file.Construct(conf_file_path));

	return OpStatus::OK;
}


OP_STATUS GadgetConfigFile::InitFile(const OpStringC& gadget_path,
		OpFile& conf_file)
{
	OpString path;
	RETURN_IF_ERROR(path.Set(gadget_path));

	// Go up one directory from `path' at most 2 times to look for the file
	// named `FILE_NAME'.  This is because `GADGET_CONFIGURATION_FILE' pointed
	// to by `gadget_path' may be one directory level deeper than the
	// installation root directory.  (The Widget spec allows two different
	// packaging strategies.)
	for (INT32 i = 0; i < 2; ++i)
	{
		OpFile file;
		RETURN_IF_ERROR(file.Construct(path));

		OpString install_dir_path;
		RETURN_IF_ERROR(file.GetDirectory(install_dir_path));

		// Fix for GetDirectory(), because GetDirectory("config.xml") returns
		// exactly "config.xml", not the true parent directory.
		if (path == install_dir_path)
		{
			path.Empty();
		}

		OpString conf_file_path;
		RETURN_IF_ERROR(OpPathDirFileCombine(conf_file_path,
					install_dir_path, OpStringC(FILE_NAME)));
		RETURN_IF_ERROR(conf_file.Construct(conf_file_path));

		BOOL exists = FALSE;
		RETURN_IF_ERROR(conf_file.Exists(exists));
		if (exists)
		{
			break;
		}
		else
		{
			RETURN_IF_ERROR(path.Set(install_dir_path));
		}
	}

	return OpStatus::OK;
}


OP_STATUS GadgetConfigFile::Write() const
{
	OP_ASSERT(NULL != m_installer_context);

	UnicodeFileOutputStream conf_file;
	RETURN_IF_ERROR(conf_file.Construct(m_file.GetFullPath(), "UTF-16"));

#ifdef MSWIN
	OpString opera_dir_path;
	RETURN_IF_ERROR(g_op_system_info->GetBinaryPath(
				&opera_dir_path));
	OpPathRemoveFileName(opera_dir_path);
	RETURN_IF_LEAVE(
			conf_file.WriteStringL(opera_dir_path.AppendL(UNI_L("\\\n"))));
#endif // MSWIN
	RETURN_IF_LEAVE(
			conf_file.WriteStringL(m_installer_context->m_profile_name));

	return OpStatus::OK;
}


GadgetConfigFile* GadgetConfigFile::Read(const OpStringC& gadget_path)
{
	OpFile conf_file;
	RETURN_VALUE_IF_ERROR(InitFile(gadget_path, conf_file), NULL);

	OpString conf_string;

	{
		UnicodeFileInputStream conf_stream;
		RETURN_VALUE_IF_ERROR(conf_stream.Construct(
					&conf_file, UnicodeFileInputStream::TEXT_FILE, TRUE), NULL);

		// Read the entire contents into one string for easy processing.

		while (conf_stream.has_more_data())
		{
			int block_size = 0;
			const uni_char* conf_data = conf_stream.get_block(block_size);
			RETURN_VALUE_IF_ERROR(conf_string.Append(
						conf_data, block_size / sizeof(uni_char)), NULL);
		}

#ifdef MSWIN
		// The first line is the Opera directory path -- loose it.
		const int line_end_pos = conf_string.FindFirstOf('\n');
		if (KNotFound != line_end_pos)
		{
			conf_string.Delete(0, line_end_pos + 1);
		}
		else
		{
			return NULL;
		}
#endif // MSWIN
	}

	OpAutoPtr<GadgetConfigFile> gadget_config(OP_NEW(GadgetConfigFile, ()));
	OP_ASSERT(NULL != gadget_config.get());
	RETURN_VALUE_IF_NULL(gadget_config.get(), NULL);
	RETURN_VALUE_IF_ERROR(gadget_config->m_file.Copy(&conf_file), NULL);
	RETURN_VALUE_IF_ERROR(
			gadget_config->m_installer_context->m_profile_name.Set(conf_string),
			NULL);

	return gadget_config.release();
}


#endif // WIDGET_RUNTIME_SUPPORT
