// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Patricia Aas
//

#include "core/pch.h"

#include "modules/pi/OpSystemInfo.h"
#include "modules/util/filefun.h"
#include "modules/util/opstring.h"
#include "modules/locale/oplanguagemanager.h"
#include "adjunct/desktop_util/handlers/DownloadManager.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "modules/locale/locale-enum.h"


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
DownloadManager* DownloadManager::m_manager = 0;
// static
DownloadManager* DownloadManager::GetManager()
{
	if( !m_manager )
	{
		m_manager = OP_NEW(DownloadManager, ());
	}

	return m_manager;
}

void DownloadManager::DestroyManager()
{
	OP_DELETE(m_manager);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL DownloadManager::HasApplicationHandler( const Viewer& viewer )
{
	OpString tmp;
	tmp.Set(viewer.GetApplicationToOpenWith());
	tmp.Strip();

	return !tmp.IsEmpty();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL DownloadManager::HasPluginHandler( Viewer& viewer )
{
	UINT32 count = viewer.GetPluginViewerCount();
	for( UINT32 i=0; i<count; i++ )
	{
		const PluginViewer* pv = viewer.GetPluginViewer(i);
		if( pv && pv->GetPath() && pv->GetProductName() )
		{
			return TRUE;
		}
	}

	return FALSE;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL DownloadManager::HasSystemRegisteredHandler( const Viewer& viewer,
														 const OpStringC & mime_type )
{
	OpString extentions;
	extentions.Set(viewer.GetExtensions());

	uni_char* e = extentions.CStr();
	if( e )
	{
		while (*e && *(++e) != ',') {};
		*e = 0;

		OpString tmp;
		tmp.AppendFormat(UNI_L("unknown.%s"), extentions.CStr() );

		OpString filename;
		filename.Set(tmp.CStr());

		OpString content_type;
		content_type.Set(mime_type.CStr());

		OpString file_handler;
		OP_STATUS status = g_op_system_info->GetFileHandler(&filename,
															content_type,
															file_handler);

		if( OpStatus::IsSuccess(status) )
		{
			if( !file_handler.IsEmpty() && filename.Compare(file_handler) != 0 )
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/***********************************************************************************
 **
 **
 **
 **
 ** TODO ? check if the file extension differs from the viewers extension,
 **        if so append the viewers first extension ?
 ***********************************************************************************/
OP_STATUS DownloadManager::GetSuggestedExtension(URL & url, OpString & file_extension)
{
	OP_STATUS err;
	// Get suggested extension
	TRAP(err, url.GetAttributeL(URL::KSuggestedFileNameExtension_L, file_extension, TRUE));
	return err;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadManager::GetHostName(URL & url, OpString & host_name)
{
	OP_STATUS err;
	// Get servername if possible
	TRAP(err, url.GetAttributeL(URL::KUniHostName, host_name, TRUE));

	if(host_name.IsEmpty())
	{
		g_languageManager->GetString(Str::SI_IDSTR_DOWNLOAD_DLG_UNKNOWN_SERVER, host_name);
	}

	return OpStatus::OK;
}



/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadManager::GetServerName(URL & url, ServerName *& servername)
{
	servername = (ServerName *) url.GetAttribute(URL::KServerName, NULL);

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadManager::GetFilePathName(URL & url, OpString & file_path)
{
	OP_STATUS err;
	TRAP(err, url.GetAttributeL(URL::KFilePathName_L, file_path, TRUE));
	return err;
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadManager::GetSuggestedFilename(URL & url,
												OpString & file_name,
												const OpStringC & extension)
{
	OP_STATUS err;
	// Get suggested filename
	TRAP(err, url.GetAttributeL(URL::KSuggestedFileName_L, file_name, TRUE));

	if(file_name.IsEmpty())
		MakeDefaultFilename(file_name, extension);

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
//Use the exsisting extension if it exists
void DownloadManager::MakeDefaultFilename(OpString & filename,
										  const OpStringC & extension)
{
	filename.Set("default");

	if (extension.HasContent())
	{
		filename.Append(".");
		filename.Append(extension.CStr());
	}
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadManager::MakeSaveTemporaryFilename(const OpStringC & source_file_name,
													 OpString & file_name)
{
	OpString tmp_storage;
	const uni_char * temp_folder = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_TEMPDOWNLOAD_FOLDER, tmp_storage);

	if(temp_folder)
	   file_name.Set(temp_folder);

	int len = file_name.Length();

	if( len > 0 && file_name[len-1] != PATHSEPCHAR )
	{
		file_name.Append(PATHSEP);
	}

	return file_name.Append(source_file_name.CStr());
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadManager::MakeSavePermanentFilename(const OpStringC & source_file_name,
													 OpString & file_name)
{
	// Never add path in interactive (filedialog) mode
	// This is suggested file name :
	return file_name.Set(source_file_name.CStr());
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL DownloadManager::ExtensionExists(const OpStringC & extension)
{
	return g_viewers->FindViewerByExtension(extension) != NULL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL DownloadManager::AllowExecuteDownloadedContent()
{
	return ((DesktopOpSystemInfo*)g_op_system_info)->AllowExecuteDownloadedContent();
}
