// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Patricia Aas
//

#include "core/pch.h"

#include "adjunct/desktop_util/handlers/DownloadItem.h"

#include "modules/dochand/win.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/util/filefun.h"
#include "modules/util/gen_str.h"
#include "modules/util/opfile/opfolder.h"

#include "adjunct/quick/dialogs/FileTypeDialog.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/managers/PrivacyManager.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_util/file_utils/filenameutils.h"
#include "adjunct/desktop_util/file_chooser/file_chooser_fun.h"
#include "modules/locale/locale-enum.h"

#ifdef _UNIX_DESKTOP_
# include "platforms/viewix/FileHandlerManager.h"
#endif //_UNIX_DESKTOP_

#ifdef WIDGET_RUNTIME_SUPPORT
# include "modules/regexp/include/regexp_api.h"
#endif // WIDGET_RUNTIME_SUPPORT

//------------------------------------------------------------------------------
//					PUBLIC FUNCTIONS :
//------------------------------------------------------------------------------

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
DownloadItem::DownloadItem(URLInformation * url_information,
						 DesktopFileChooser* chooser,
						 BOOL is_download/* = FALSE*/,
						 BOOL is_privacy_mode/*= FALSE*/)
{
	Init(static_cast<TransferManagerDownloadAPI*>(url_information), is_download, is_privacy_mode, chooser);
}

DownloadItem::DownloadItem(OpDocumentListener::DownloadCallback* download_callback, 
						 DesktopFileChooser* chooser,
						 BOOL is_download/* = FALSE*/,
						 BOOL is_privacy_mode/*= FALSE*/)
{
	Init(static_cast<TransferManagerDownloadAPI*>(download_callback),is_download, is_privacy_mode, chooser);
}

DownloadItem::~DownloadItem()
{
}

void DownloadItem::Init(TransferManagerDownloadAPI* download_callback, BOOL is_download, BOOL is_privacy_mode, DesktopFileChooser* chooser)
{
	ResetVars();
	m_download_callback = static_cast<TransferManagerDownloadAPI *>(download_callback);
	//Do some minor initialization

	m_save = FALSE;
	m_is_download = is_download;
	m_chooser = chooser;
	m_listener_callback = NULL;
	m_file_choosing_action = FILE_CHOOSING_NONE;
	m_download_callback = download_callback;
	m_is_privacy_mode = is_privacy_mode;
	
	// Do some minor initialization.

	GetFile(TRUE);
	GetServer(TRUE);
}


OP_STATUS DownloadItem::Init(UINT file_icon_size,
							 UINT server_icon_size,
							 UINT handler_icon_size,
							 UINT mimetype_icon_size)
{
	m_file_icon_size     = file_icon_size;
	m_server_icon_size   = server_icon_size;
	m_handler_icon_size  = handler_icon_size;
	m_mimetype_icon_size = mimetype_icon_size;

	OpString type;
	return Init(type);
}

OP_STATUS DownloadItem::Init(OpString & type)
{
	m_initialized = TRUE;

	// Make the file object :
	GetFile(TRUE);
	OP_ASSERT(m_file.IsInitialized());
	if(!m_file.IsInitialized())
	{
		m_initialized = FALSE;
		return OpStatus::ERR;
	}

	// Make the server object :
	GetServer(TRUE);
	OP_ASSERT(m_server.IsInitialized());
	if(!m_server.IsInitialized())
	{
		m_initialized = FALSE;
		return OpStatus::ERR;
	}

	// Make the mime type object :
	GetMimeType(type);
	OP_ASSERT(m_mime_type.IsInitialized());
	if(!m_mime_type.IsInitialized())
	{
		m_initialized = FALSE;
		return OpStatus::ERR;
	}

	//If mimetype is untrusted, determine set mimetype on extension :
	if(m_mime_type.IsUntrusted())
	{
		SetMimeTypeOnExtension();
	}

	// Make/Get the viewer :
	GetViewer();

	// Make the handlers :
	if( OpStatus::IsSuccess(GetFileHandlers(m_handlers)) )
	{
		m_handlers_loaded = TRUE;
	}
	else
	{
		m_initialized = FALSE;
		return OpStatus::ERR;
	}

	//Make the plugins :
	if( OpStatus::IsSuccess(GetPlugins(m_plugins)) )
	{
		m_plugins_loaded = TRUE;
	}
	else
	{
		m_initialized = FALSE;
		return OpStatus::ERR;
	}

	//Make the user_handler :
	HandlerContainer * user_handler = 0;
	if( OpStatus::IsSuccess(GetUserHandler(user_handler)) )
	{
		m_user_handler_loaded = TRUE;
	}
	else
	{
		m_initialized = FALSE;
		return OpStatus::ERR;
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
OP_STATUS DownloadItem::ResetVars()
{
	m_mode             = HANDLER_MODE_UNKNOWN;

	m_initialized      = FALSE;

	m_handlers_loaded  = FALSE;
	m_plugins_loaded   = FALSE;
	m_viewer_loaded    = FALSE;

	m_original_viewer  = NULL;
	m_keep_temp_viewer = FALSE;

	m_selected_handler = 0;
	m_selected_plugin  = 0;

	m_user_command.Empty();

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadItem::Invalidate()
{
	ResetVars();

	m_plugins.DeleteAll();
	m_handlers.DeleteAll();

	Viewer * v = OP_NEW(Viewer, ());

	if(!v)
	{
		OP_ASSERT(FALSE);
		return OpStatus::ERR_NO_MEMORY;
	}

	m_viewer.CopyFrom(v);
	OP_DELETE(v);

    m_mime_type.Empty();
	m_server.Empty();
	m_file.Empty();

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadItem::Save(BOOL save, DownloadItemCallback * listener_callback)
{
	m_save = save;
	m_listener_callback = listener_callback;

	RETURN_IF_ERROR(SetFileChoosingAction(FILE_CHOOSING_SAVE));

	OP_ASSERT(m_initialized);
	OP_ASSERT(m_download_callback);
	
	if (!m_initialized || !m_download_callback)
		return OpStatus::ERR;

	OpString saveto_filename;

	BOOL ask_user = TRUE;
	if (m_download_callback->IsSaveDirect())
	{
		OpString savename;
		savename.Set(m_file.GetName());
		m_download_callback->SetSaveDirectFileName(savename, saveto_filename, ask_user);
	}

	if (ask_user)
	{
		// Make the filename :
		DownloadManager::GetManager()->MakeSavePermanentFilename(m_file.GetName(), saveto_filename);
		GetFinalFilename(m_download_callback->GetOpWindow(), &saveto_filename, save, listener_callback);
	}
	else
	{
		SaveDone(save, saveto_filename);

		if(listener_callback)
			listener_callback->OnSaveDone(FALSE);
		else
			OP_DELETE(this);
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
OP_STATUS DownloadItem::SaveDone(BOOL save, OpString& saveto_filename)
{
	m_viewer.SetAction(VIEWER_SAVE);

	// Load data to the file :
	// Initiate the transfer :
	m_download_callback->SetViewer(m_viewer);
#ifdef WIC_CAP_DOWNLOAD_SAVE_NO_SHOW_TRANSFER
	if (m_download_callback->Save(saveto_filename.CStr()))
#else
	if (m_download_callback->Save(saveto_filename.CStr(), TRUE))
#endif
	{
		m_download_callback = NULL;
	}

	// Commit the viewer if the user wants us to remember :
	if(save && m_viewer_loaded)
	{
		CommitViewer();
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
OP_STATUS
DownloadItem::Open(	BOOL save,
					BOOL allow_direct_execute,
					BOOL from_file_type_dialog_box)
{
	OP_ASSERT(m_initialized && m_download_callback);

	if(!m_initialized || !m_download_callback)
		return OpStatus::ERR;

	if(from_file_type_dialog_box)
	{
		/*
		  TODO :
		  The FileTypeDialog should modify m_viewer when opened in conjuction with
		  a DownloadItem. This is not yet implemented.
		*/
		OP_ASSERT(FALSE);
	}

	if ( allow_direct_execute && IsLocalFile() )
	{
		OpenLocalFile();
	}
	else if( m_viewer_loaded ) //TODO : is this test really neccessary?
	{
		switch(m_viewer.GetAction())
		{
		case VIEWER_PLUGIN:
			if (m_download_callback->ReturnToSender(TRUE))
			{
				m_download_callback = NULL;
			}
			break;
		case VIEWER_PASS_URL:
#ifdef WIC_CAP_TRANSFERMANAGERDOWNLOADAPI_PASSURL
			m_download_callback->SetViewer(m_viewer);
			if (m_download_callback->PassURL())
				m_download_callback = NULL;
			break;
#endif // WIC_CAP_TRANSFERMANAGERDOWNLOADAPI_PASSURL
		case VIEWER_OPERA:
		{
			/*
			  Force Opera to use the extension when dealing with this file.
			  So force the mimetype to change.
			*/
			Viewer * test_viewer = g_viewers->FindViewerByExtension(m_file.GetExtension());
			if (test_viewer)
				m_download_callback->ForceMIMEType(test_viewer->GetContentType());

			m_download_callback->SetViewerAction(VIEWER_OPERA);
			if (m_download_callback->ReturnToSender(FALSE))
			{
				m_download_callback = NULL;
			}

			// Do not allow this choice to be remembered.
			// This is a manual override.
			save = FALSE;
			break;
		}

		default:
		{
			OpString saveto_filename;
			DownloadManager::GetManager()->MakeSaveTemporaryFilename(m_file.GetName(), saveto_filename);

			if(!m_viewer.GetFlags().IsSet(ViewActionFlag::SAVE_DIRECTLY))
			{
				m_download_callback->SetWaitForUser();
			}

			// DSK-332652 use previous file name if callback is for file
			// which was previously downloaded and not sucessfully opened
			if(m_download_callback->WasDownloaded())
				RETURN_IF_ERROR(saveto_filename.Set(m_download_callback->FilePathName()));
			else
				CreateUniqueFilename(saveto_filename);

			if (saveto_filename.HasContent())
			{
				m_download_callback->SetViewer(m_viewer);
				// Initiate the transfer :
				if (m_download_callback->Run(saveto_filename.CStr()))
					m_download_callback = NULL;
			}
			else
			{
				if (m_download_callback->Abort())
					m_download_callback = NULL;
			}
		}
		break;
		}
	}

	if( from_file_type_dialog_box || save)
	{
		CommitViewer();
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
OP_STATUS DownloadItem::Cancel()
{
	OP_ASSERT(m_initialized);
	if(!m_initialized)
		return OpStatus::ERR;

	if(!m_download_callback)
		return OpStatus::ERR;

	if (m_download_callback->Abort())
	{
		m_download_callback = NULL;
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
OP_STATUS DownloadItem::Close()
{
	OP_ASSERT(m_initialized);
	if (!m_initialized)
		return OpStatus::ERR;
	if (!m_download_callback)
		return OpStatus::ERR;
	if (m_viewer_loaded)
		if (m_viewer.GetAction() != VIEWER_PLUGIN)
			m_download_callback->Stop();
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadItem::ChooseOtherApplication(DownloadItemCallback * listener_callback)
{
	OP_STATUS status = OpStatus::OK;

	OP_ASSERT(m_chooser);

	if (m_chooser)
	{
		// Aborts if we are already listening to another file dialog.

		RETURN_IF_ERROR(SetFileChoosingAction(FILE_CHOOSING_OTHER_APP));

		m_listener_callback = listener_callback;
#ifdef _MACINTOSH_
		m_request.action = DesktopFileChooserRequest::ACTION_CHOOSE_APPLICATION;
#else
		m_request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN;
#endif

		status = m_chooser->Execute(listener_callback ? listener_callback->GetParentWindow() : NULL, this, m_request);
	}

	return status;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadItem::SetFileChoosingAction(FileChoosingAction action)
{
	if(m_file_choosing_action != FILE_CHOOSING_NONE)
		return OpStatus::ERR;

	m_file_choosing_action = action;
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DownloadItem::OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
{
	BOOL choice_made;
	switch(GetFileChoosingAction())
	{
		case FILE_CHOOSING_SAVE:
		{
			if (result.files.GetCount())
			{
				if (result.active_directory.HasContent())
				{

					if (GetPrivacyMode())
					{
						if (OpStatus::IsError(PrivacyManager::GetInstance()->SetPrivateWindowSaveFolderLocation(result.active_directory)))
						{
							return;
						}
					}
					else
					{
						TRAPD(err, g_pcfiles->WriteDirectoryL(OPFILE_SAVE_FOLDER, result.active_directory.CStr()));
					}
				}

				OpString file_name;
				file_name.Set(result.files.Get(0)->CStr());
				SaveDone(m_save, file_name);

				if (m_listener_callback)
					m_listener_callback->OnSaveDone(FALSE);
				else
				{
					OP_DELETE(this);
					return;
				}
			}
			else
			{
				Cancel();

				if (m_listener_callback)
					m_listener_callback->OnSaveDone(TRUE);
				else
				{
					OP_DELETE(this);
					return;
				}
			}

			break;
		}

		case FILE_CHOOSING_OTHER_APP:
		{
			choice_made = result.files.GetCount();
			if(choice_made)
			{
				if (OpStatus::IsSuccess(m_user_command.Set(*result.files.Get(0))))
				{
					// Set this as the user app in the viewer :
					m_viewer.SetApplicationToOpenWith(m_user_command);
				}
				// Force a reload of m_user_handler in GetUserHandler :
				m_user_handler_loaded = FALSE;
			}

			// Signal that we are done :
			if (m_listener_callback)
				m_listener_callback->OnOtherApplicationChosen(choice_made);
			else
			{
				OP_DELETE(this);
				return;
			}
			break;
		}

		case FILE_CHOOSING_NONE:
		{
			// We recieved a callback from an unknown file dialog.
			OP_ASSERT(FALSE);
			break;
		}
	}

	// Set our file choosing action to none :
	ResetFileChoosingAction();
	ResetDesktopFileChooserRequest(m_request);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadItem::Change(DesktopWindow* parent_window)
{
	OP_ASSERT(m_initialized);
	if(!m_initialized)
		return OpStatus::ERR;

	FiletypeDialog* dialog = OP_NEW(FiletypeDialog, (&m_viewer, FALSE));

	if(!dialog)
	{
		OP_ASSERT(FALSE);
		return OpStatus::ERR_NO_MEMORY;
	}

	dialog->Init(parent_window);
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadItem::Update()
{
	OP_ASSERT(m_initialized);
	if(!m_initialized)
		return OpStatus::ERR;

	if(m_viewer_loaded)
	{
		switch(GetAction())
		{
		case VIEWER_PLUGIN:
			m_mode = HANDLER_MODE_PLUGIN;
			break;
		case VIEWER_OPERA:
			m_mode = HANDLER_MODE_INTERNAL;
			break;
		case VIEWER_PASS_URL:
			// Here we need to be careful, some viewers can handle Pass-url others can't, and the
			// default viewer is not the right to ask - so I don't know it this m_mode makes sense at all
		case VIEWER_APPLICATION:
			m_mode = HANDLER_MODE_EXTERNAL;
			break;
		case VIEWER_SAVE:
			m_mode = HANDLER_MODE_SAVE;
			break;

		default:
			m_mode = HANDLER_MODE_UNKNOWN;
			break;
		}
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
BOOL DownloadItem::HasHandler(UINT selected_handler)
{
	OP_ASSERT(m_initialized);
	if(!m_initialized)
		return FALSE;

	return m_handlers.GetCount() && m_handlers.GetCount() > selected_handler;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL DownloadItem::HasPlugin(UINT selected_plugin)
{
	OP_ASSERT(m_initialized);
	if(!m_initialized)
		return FALSE;

	return m_plugins.GetCount() && m_plugins.GetCount() > selected_plugin;
}

/***********************************************************************************
 **
 **
 **
 **
 ** TODO : This should be improved
 ***********************************************************************************/
BOOL DownloadItem::IsLocalFile()
{
	OP_ASSERT(m_initialized);
	if(!m_initialized)
		return FALSE;

	if (m_download_callback && m_download_callback->GetServerName() == NULL)
		return TRUE;
	else
		return FALSE;
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadItem::GetFileHandlers(OpVector<HandlerContainer>& handlers)
{
	OP_ASSERT(m_initialized);
	if(!m_initialized)
		return OpStatus::ERR;

	if(m_handlers_loaded)
	{
		if(&handlers != &m_handlers)
		{
			for(UINT i = 0; i < m_handlers.GetCount(); i++)
			{
				HandlerContainer * handler = m_handlers.Get(i);
				handlers.Add(handler);
			}
		}
	}
	else
	{
		OpAutoVector<OpString> handler_commands;
		OpAutoVector<OpString> handler_names;
		OpVector<OpBitmap> handler_icons;

		// Protect the objects by copying the info :
		// (GetFileHandlers doesn't take "const OpStringC16&")

		OpString content_type;
		content_type.Set(m_mime_type.GetMimeType().CStr());

		OpString file_name;
		file_name.Set(m_file.GetName().CStr());

		URLType type = GetType();

		//If this is a download the file will be local when we are done :
		if(m_is_download)
			type = URL_FILE;
#ifdef WIC_CAP_TRANSFERMANAGERDOWNLOADAPI_PASSURL
		// Add default from viewers
		OpString appcommand;
		BOOL pass_url = FALSE;
		// DSK-332652 - if file was downloaded then attempt to open it with
		// application from this callback failed, so do not load this handler
		if(m_download_callback && !m_download_callback->WasDownloaded())
			m_download_callback->GetApplicationToOpenWith(appcommand, pass_url);
		if (appcommand.HasContent())
		{
			OpStringC8 appicon("");
			HandlerContainer * handler = OP_NEW(HandlerContainer, (appcommand,
															  appcommand,
															  appicon));

			if(!handler)
			{
				OP_ASSERT(FALSE);
				return OpStatus::ERR_NO_MEMORY;
			}

			if (pass_url)
				handler->SetHandlerMode(HANDLER_MODE_PASS_URL);
			else
				handler->SetHandlerMode(HANDLER_MODE_EXTERNAL);
			handlers.Add(handler);
		}
#endif // WIC_CAP_TRANSFERMANAGERDOWNLOADAPI_PASSURL

		//Get the handlers :
		((DesktopOpSystemInfo*)g_op_system_info)->GetFileHandlers(file_name,
																  content_type,
																  handler_commands,
																  handler_names,
																  handler_icons,
																  type,
																  m_handler_icon_size);

		//The required size of the icon is in : m_handler_icon_size

		for(UINT i = 0; i < handler_commands.GetCount(); i++)
		{
			HandlerContainer * handler = OP_NEW(HandlerContainer, (handler_commands.Get(i),
															  handler_names.Get(i),
															  handler_icons.Get(i)));
			if(!handler)
			{
				OP_ASSERT(FALSE);
				return OpStatus::ERR_NO_MEMORY;
			}

			handlers.Add(handler);
		}

#ifdef WIDGET_RUNTIME_SUPPORT

		if (content_type == UNI_L("application/x-opera-widgets") ||
			content_type == UNI_L("application/widget"))
		{
			for (unsigned i = 0; i < handler_commands.GetCount(); i++)
			{
				if (handler_commands.Get(i)->FindI(UNI_L("opera")) != KNotFound)
				{
					handler_commands.Remove(i);
					handler_names.Remove(i);
					handler_icons.Remove(i);
					handlers.Remove(i);
					i--;
				}
			}

			// skip adding opera as handler
			return OpStatus::OK;
		}
#endif // WIDGET_RUNTIME_SUPPORT

		/* Only offer opera for downloaded files */
		if(m_is_download)
		{
			/*
			  If the viewer for the extension is set to open in opera then offer opera
			  as a viewer and mark it as having HandlerMode::HANDLER_MODE_INTERNAL.
			  If this HandlerContainer is chosen we will override the mimetype in
			  Open and force core to deal with the file anyway.
			*/

			Viewer * test_viewer = g_viewers->FindViewerByExtension(m_file.GetExtension());
			ViewAction action = test_viewer ? test_viewer->GetAction() : VIEWER_NOT_DEFINED;

			if(action == VIEWER_OPERA)
			{
				OpStringC command(UNI_L("opera"));
				OpStringC name(UNI_L("Opera"));
				OpStringC8 icon("Window Browser Icon");

				HandlerContainer * handler = OP_NEW(HandlerContainer, (command,
																  name,
																  icon));

				if(!handler)
				{
					OP_ASSERT(FALSE);
					return OpStatus::ERR_NO_MEMORY;
				}

				handler->SetHandlerMode(HANDLER_MODE_INTERNAL);
				handlers.Insert(0, handler);
			}
		}
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
OP_STATUS DownloadItem::GetUserHandler(HandlerContainer *& user_handler)
{
	if(!m_user_handler_loaded)
	{
		OpString  item_command;
		OpString  item_name;
		OpString8 item_icon;

		RETURN_IF_ERROR(item_command.Set(m_user_command));

		m_user_handler.Init(item_command, item_name, item_icon);
	}

	user_handler = &m_user_handler;

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadItem::GetPlugins(OpVector<PluginContainer>& plugins)
{
	OP_ASSERT(m_initialized);
	if(!m_initialized)
		return OpStatus::ERR;

	if(m_plugins_loaded)
	{
		for(UINT i = 0; i < m_plugins.GetCount(); i++)
		{
			PluginContainer * plugin = m_plugins.Get(i);
			plugins.Add(plugin);
		}
	}
	else
	{
		OpString   plugin_path;
		OpString   plugin_name;
		OpString   plugin_description;
		OpBitmap * plugin_icon = 0;

		UINT32 plugin_count = m_viewer.GetPluginViewerCount();
		UINT selected_plugin = 0;

		for(UINT32 i = 0; i < plugin_count; i++ )
		{
			const PluginViewer* plugin_viewer = m_viewer.GetPluginViewer(i);

			if(plugin_viewer)
			{
				plugin_path.Set(plugin_viewer->GetPath());
				plugin_name.Set(plugin_viewer->GetProductName());
				plugin_description.Set(plugin_viewer->GetDescription());

				PluginContainer * plugin = OP_NEW(PluginContainer, (&plugin_path,
															   &plugin_description,
															   &plugin_name,
															   plugin_icon));

				if(!plugin)
				{
					OP_ASSERT(FALSE);
					return OpStatus::ERR_NO_MEMORY;
				}

				plugins.Add(plugin);

				if(IsViewerSelectedPlugin(plugin))
				{
					selected_plugin = i;
				}
			}
		}

		if(HasPlugin(selected_plugin))
		{
			m_selected_plugin = selected_plugin;
		}
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
BOOL DownloadItem::IsViewerSelectedPlugin(PluginContainer * plugin)
{
	if(!plugin)
		return FALSE;

	if( m_viewer.PluginName() && m_viewer.GetDefaultPluginViewerPath(TRUE) )
	{
		if( uni_strcmp(plugin->GetName().CStr(), m_viewer.PluginName()) == 0 &&
			uni_strcmp(plugin->GetPath().CStr(), m_viewer.GetDefaultPluginViewerPath(TRUE)) == 0 )
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
HandlerContainer * DownloadItem::GetSelectedHandler()
{
	OP_ASSERT(m_initialized);
	if(!m_initialized)
		return 0;

	if(m_selected_handler == -1)
	{
		return &m_user_handler;
	}

	if(HasHandler(m_selected_handler))
	{
		return m_handlers.Get(m_selected_handler);
	}

	return 0;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
PluginContainer * DownloadItem::GetSelectedPlugin()
{
	OP_ASSERT(m_initialized);
	if(!m_initialized)
		return 0;

	if(HasPlugin(m_selected_plugin))
	{
		return m_plugins.Get(m_selected_plugin);
	}

	return 0;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadItem::SetSelected(DownloadManager::Container * container)
{
	switch(container->GetType())
	{
	case CONTAINER_TYPE_HANDLER :
		SetSelectedHandler((HandlerContainer *) container);
		return OpStatus::OK;
		break;

	case CONTAINER_TYPE_PLUGIN :
		SetSelectedPlugin((PluginContainer *) container);
		return OpStatus::OK;
		break;
	}

	return OpStatus::ERR;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
DownloadManager::Container * DownloadItem::GetSelected()
{
	switch(GetMode())
	{
	case HANDLER_MODE_INTERNAL:
	case HANDLER_MODE_EXTERNAL:
		return GetSelectedHandler();
	case HANDLER_MODE_PLUGIN   :
		return GetSelectedPlugin();
	}

	return 0;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadItem::SetSelectedHandler(HandlerContainer * handler)
{
	if(!handler)
		return OpStatus::ERR;

	for(UINT i = 0; i < m_handlers.GetCount(); i++)
	{
		HandlerContainer * h = m_handlers.Get(i);

		if(h == handler)
		{
			return SetSelectedHandlerIndex(i);
		}
	}

	if(&m_user_handler == handler)
	{
		return SetSelectedHandlerIndex(-1);
	}

	return OpStatus::ERR;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadItem::SetSelectedHandlerIndex(int index)
{
	if(index >= (int) m_handlers.GetCount())
		return OpStatus::ERR;

	HandlerContainer * handler = 0;

	if(index >= 0)
		handler = m_handlers.Get(index);
	else if(index == -1)
		handler = &m_user_handler;

	if(handler)
	{
		m_selected_handler = index;

		switch (handler->GetHandlerMode())
		{
		case HANDLER_MODE_EXTERNAL:
		{
			/*
			  If the handler is the file - then don't supply it as a handler.
			  Windows fix for executable files.
			 */
			OpString quoted_filename;
			quoted_filename.AppendFormat(UNI_L("\"%s\""), m_file.GetName().CStr());

			if (	handler->GetCommand().Compare(m_file.GetName()) == 0
				||	handler->GetCommand().Compare(quoted_filename) == 0)
				m_viewer.SetApplicationToOpenWith(UNI_L(""));
			else
				m_viewer.SetApplicationToOpenWith(handler->GetCommand());

			m_viewer.SetAction(VIEWER_APPLICATION);
			break;
		}
		case HANDLER_MODE_INTERNAL:
			m_viewer.SetAction(VIEWER_OPERA);
			break;
		}

		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadItem::SetSelectedPlugin(PluginContainer * plugin)
{
	if(!plugin)
		return OpStatus::ERR;

	for(UINT i = 0; i < m_plugins.GetCount(); i++)
	{
		PluginContainer * p = m_plugins.Get(i);
		if(p == plugin)
		{
			return SetSelectedPluginIndex(i);
		}
	}

	return OpStatus::ERR;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadItem::SetSelectedPluginIndex(UINT index)
{
	if(index >= m_plugins.GetCount())
		return OpStatus::ERR;

	PluginContainer * plugin = m_plugins.Get(index);

	if(plugin)
	{
		m_selected_plugin = index;
		m_viewer.SetDefaultPluginViewer( m_selected_plugin );
		m_viewer.SetAction(VIEWER_PLUGIN);

		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
MimeTypeContainer & DownloadItem::GetMimeType()
{
	OpString type;
	GetMimeType(type);
	return m_mime_type;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadItem::GetMimeType(OpString & type)
{
	OP_ASSERT(m_initialized);
	if(!m_initialized)
		return OpStatus::ERR;

	if(type.IsEmpty())
	{
		if (m_download_callback)
			type.Set(m_download_callback->MIMEType());
	}

	if(type.IsEmpty())
	{
		OpString filename;
		GetFileName(filename);
		UnQuote(filename);

#ifdef _UNIX_DESKTOP_
		if(filename.HasContent())
			FileHandlerManager::GetManager()->GuessMimeType(filename, type);
#endif //_UNIX_DESKTOP_
	}

	if(!m_mime_type.IsInitialized())
	{
		OpString type_name;
		OpBitmap * icon = 0;
		//The required size of the icon is in : m_mimetype_icon_size
		((DesktopOpSystemInfo*)g_op_system_info)->GetFileTypeInfo(m_file.GetName(),
																  type,
																  type_name,
																  icon,
																  m_mimetype_icon_size);

		m_mime_type.Init(&type, &type_name, icon);
	}
	else
	{
		ChangeMimeType(type);
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
OP_STATUS DownloadItem::ChangeMimeType(OpString & mime_type)
{
	// If the mimetype is initialized and is not this mimetype then we are changing
	// the mimetype and everything will have to be invalidated.
	if(m_mime_type.IsInitialized() &&
	   !m_mime_type.HasMimeType(mime_type))
	{
		Invalidate();

		if(!mime_type.HasContent()) // Reset mime_type
		{
			mime_type.Set(UNI_L("temporary")); //dummy
		}

		Init(mime_type);
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
ServerContainer & DownloadItem::GetServer(BOOL force_init)
{
	if(!m_server.IsInitialized() || force_init)
	{
		Image icon;
		OpString name;
		if (m_download_callback)
		{
			OP_ASSERT(g_favicon_manager);
			icon = g_favicon_manager->Get(m_download_callback->URLName());

			//The required size of the icon is in : m_server_icon_size

			OpStatus::Ignore(m_download_callback->GetHostName(name));
		}
		if(name.IsEmpty())
		{
			g_languageManager->GetString(Str::SI_IDSTR_DOWNLOAD_DLG_UNKNOWN_SERVER, name);
		}
		m_server.Init(&name, icon);
	}

	return m_server;
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
FileContainer & DownloadItem::GetFile(BOOL force_init)
{
	if(!m_file.IsInitialized() || force_init)
	{
		OpString file_ext;
		OpString file_name;
		OpString file_path;

		if (m_download_callback)
		{
			file_ext.Set(m_download_callback->SuggestedExtension());
			file_name.Set(m_download_callback->SuggestedFilename());
			file_path.Set(m_download_callback->FilePathName());
		}

		OpBitmap * thumbnail = 0;
		//The required size of the icon is in : m_file_icon_size;

		m_file.Init(&file_name,
					&file_path,
					&file_ext,
					thumbnail);
	}

	return m_file;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpFileLength DownloadItem::GetContentSize()
{
	OP_ASSERT(m_initialized);
	//TEST

	if (m_download_callback)
		return m_download_callback->GetContentSize();
	else
		return 0;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL DownloadItem::GetContentSize(OpString & size_string,
								  UINT32 format_flags)
{
	OpFileLength size = GetContentSize();

	return MakeFormatedSize(size_string,
							size,
							format_flags);
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpFileLength DownloadItem::GetLoadedSize()
{
	OP_ASSERT(m_initialized);
	//TEST
	if (m_download_callback)
		return m_download_callback->GetLoadedSize();
	else
		return 0;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DownloadItem::GetLoadedSize(OpString & size_string,
								 UINT32 format_flags)
{
	OpFileLength size = GetLoadedSize();

	MakeFormatedSize(size_string,
					 size,
					 format_flags);
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HandlerMode DownloadItem::GetMode()
{
	OP_ASSERT(m_initialized);
	//TEST

	Update();
	return m_mode;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadItem::GetFileName(OpString & filename)
{
	//Get the url type:
	//URLType type = GetType();
	if (m_download_callback)
		filename.Set(m_download_callback->FilePathName());

	//Convert the file url to a full path :
	/*if(type == URL_FILE)
	{
		ConvertURLtoFullPath(filename);

		//Quote the filename (there might be spaces) :
		filename.Insert(0, "\"");
		filename.Append("\"");
	}*/

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
#ifdef CONTENT_DISPOSITION_ATTACHMENT_FLAG
BOOL DownloadItem::GetUsedContentDispositionAttachment()
{
	OP_ASSERT(m_initialized);
	//TEST
	if (m_download_callback)
		return m_download_callback->GetUsedContentDispositionAttachment();
	else
		return FALSE;
}
#endif //CONTENT_DISPOSITION_ATTACHMENT_FLAG


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DownloadItem::GetPercentLoaded(OpString & percent_string)
{
	double downloadedsize = (double)(INT64) GetLoadedSize();
	double totalsize      = (double)(INT64) GetContentSize();

	if(totalsize < downloadedsize)
	{
		return;
	}

	// FIXME: comparing double != 0.0 is not robust; chose an error bar !
	OP_MEMORY_VAR double percent = (downloadedsize != 0.0 && totalsize != 0.0) ? (downloadedsize / totalsize) * 100 : 0;

	INT32 percent_int = static_cast<INT32>(percent);
	if (percent_int == 0)
	{
		percent_int = 1;
	}

	percent_string.AppendFormat(UNI_L("%.1f%%"), percent);
}

//------------------------------------------------------------------------------
//					PRIVATE FUNCTIONS :
//------------------------------------------------------------------------------

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadItem::CommitViewer()
{
	OP_ASSERT(m_initialized);
	if(!m_initialized)
		return OpStatus::ERR;

	if(!m_viewer_loaded)
		return OpStatus::ERR;

	if(m_original_viewer)
	{
		g_viewers->DeleteViewer(m_original_viewer);
		m_original_viewer = 0;
	}

	//TODO : if this viewer is already there - remove it

	//Make the viewer to be inserted :
	Viewer * viewer = OP_NEW(Viewer, ());

	if(!viewer)
	{
		OP_ASSERT(FALSE);
		return OpStatus::ERR_NO_MEMORY;
	}

	//Copy from our viewer :
	viewer->CopyFrom(&m_viewer);

	if (OpStatus::IsError(g_viewers->AddViewer(viewer)))
		OP_DELETE(viewer);

	TRAPD(err, g_viewers->WriteViewersL());

	return err;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadItem::OpenLocalFile()
{
	OP_ASSERT(m_initialized);
	if(!m_initialized)
		return OpStatus::ERR;

	OpString filename;
	if (m_download_callback)
		filename.Set(m_download_callback->FilePathName());
	//execute local-files directly
#ifdef MSWIN
	Execute(filename, NULL);
#else // !MSWIN

	HandlerContainer * handler = GetSelectedHandler();
	if (handler)
	{
		// At this point we must ensure the file argument is quoted.
		// PlatformExecuteApplication() specifies that the second argument
		// is "Arguments to the application" and in this case there is only
		// one which may contain spaces. See DSK-319053

		int length = filename.Length();
		if (length > 1 && filename.CStr()[0] != '"' && filename.CStr()[length-1] != '"')
		{
			filename.Insert(0, UNI_L("\""));
			filename.Append(UNI_L("\""));
		}

#ifdef _UNIX_DESKTOP_
		FileHandlerManager::GetManager()->EnableValidation(FALSE);
#endif //_UNIX_DESKTOP_

		// TODO: ExecuteApplication() should take arguments as a vector eliminating the space ambiguity
		g_op_system_info->ExecuteApplication(handler->GetCommand().CStr(), filename.CStr());

#ifdef _UNIX_DESKTOP_
		FileHandlerManager::GetManager()->EnableValidation(TRUE);
#endif //_UNIX_DESKTOP_
	}

#endif //!MSWIN

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL DownloadItem::IsLoading()
{
	OP_ASSERT(m_initialized);
	//TEST
	if (m_download_callback)
		return m_download_callback->IsLoading();
	else
		return FALSE;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL DownloadItem::IsLoaded()
{
	OP_ASSERT(m_initialized);
	//TEST
	if (m_download_callback)
		return m_download_callback->IsLoaded();
	else
		return FALSE;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DownloadItem::SetMimeTypeOnExtension()
{
	OP_ASSERT(m_initialized);
	//TEST

	Viewer* viewer = 0;

	if(m_file.GetExtension().Length())
	{
		viewer = g_viewers->FindViewerByExtension(m_file.GetExtension());
	}

	if (viewer)
	{
		MakeMimeType(viewer->GetContentTypeString());
	}
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
const Viewer & DownloadItem::GetViewer()
{
	OP_ASSERT(m_initialized);
	//TEST

	if(m_viewer_loaded)
		return m_viewer;

	if(!ViewerExists())
		CreateViewer();
	else
		CopyViewer();

	m_viewer_loaded = TRUE;

	return m_viewer;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
ViewAction DownloadItem::GetAction()
{
	OP_ASSERT(m_initialized);
	//TEST

	if(m_viewer_loaded)
		return m_viewer.GetAction();

	return VIEWER_NOT_DEFINED;
}

/***********************************************************************************
 **
 **
 **
 **
 ** We don't have a viewer that matches the content we're downloading...
 ** (create a new viewer that can be added if the user wants)
 ***********************************************************************************/
OP_STATUS DownloadItem::CreateViewer()
{
	OP_ASSERT(m_initialized);
	//TEST

	if(m_viewer_loaded)
	{
		return OpStatus::ERR;
	}

	OP_STATUS err;

	if(m_mime_type.IsInitialized())
	{
		OpString suggestExt;
		OpString OldExt;

		m_viewer.SetContentType(m_mime_type.GetMimeType());

		suggestExt.Set( m_file.GetExtension().CStr() );

		err = OldExt.Set( m_viewer.GetExtensions() );

		if(OpStatus::IsError(err))
		{
			return err;
		}

		if(!suggestExt.IsEmpty())
		{
			if(!OldExt.IsEmpty())
			{
				OldExt.Append(",");
			}
			OldExt.Append(suggestExt);
			m_viewer.SetExtensions(OldExt.CStr());
		}
	}
	else 	//no type available! We are now in the dark
	{
		//this viewer is only used this one time
		err = m_viewer.SetContentType(UNI_L("temporary"));

		if(OpStatus::IsError(err))
		{
			return err;
		}

		err = m_viewer.SetExtensions(m_file.GetExtension().CStr());

		if(OpStatus::IsError(err))
		{
			return err;
		}
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
OP_STATUS DownloadItem::CopyViewer()
{
	OP_ASSERT(m_initialized);
	//TEST

	if(m_viewer_loaded)
	{
		return OpStatus::OK;
	}

	m_original_viewer = FindViewer();

	if (m_original_viewer)
	{
		//take a copy of the viewer
		OP_STATUS err;
		err = m_viewer.CopyFrom(m_original_viewer);

		if(OpStatus::IsError(err))
		{
			return err;
		}
	}
	else
	{
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

Viewer * DownloadItem::FindViewer()
{
	OP_ASSERT(m_initialized);
	//TEST

	Viewer * viewer = NULL;

	if(m_mime_type.IsUntrusted())
	{
		viewer = g_viewers->FindViewerByExtension(m_file.GetExtension());
	}
	else
	{
		viewer = g_viewers->FindViewerByMimeType(m_mime_type.GetMimeType());
	}

	return viewer;
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL DownloadItem::ViewerExists()
{
	OP_ASSERT(m_initialized);
	//TEST

	return FindViewer() != NULL;
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DownloadItem::MakeMimeType(const uni_char * mime_type)
{

}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL DownloadItem::MakeFormatedSize(OpString & size_string,
									OpFileLength size,
									UINT32 format_flags)
{
	size_string.Reserve(256);

	StrFormatByteSize(size_string, size, format_flags);

	if(size == 0)
	{
		// String blatently stolen
		g_languageManager->GetString(Str::SI_IDSTR_UNKNOWN_IMAGE_TYPE, size_string);
		return FALSE;
	}

	return TRUE;
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
URLType DownloadItem::GetType()
{
	OP_ASSERT(m_initialized);
	//TEST

	//Get the url type:
	if (m_download_callback)
		return m_download_callback->GetURLType();
	else
		return URL_UNKNOWN;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DownloadItem::GetFinalFilename(OpWindow *	parent,
										 OpString * finalname,
										 BOOL save,
										 DownloadItemCallback * listener_callback)
{
	OP_STATUS ret = OpStatus::OK;

	OP_ASSERT(m_chooser);

	if (m_chooser)
	{
		// Save operation.
		m_request.action = DesktopFileChooserRequest::ACTION_FILE_SAVE_PROMPT_OVERWRITE;
		g_languageManager->GetString(Str::S_SAVE_AS_CAPTION, m_request.caption);
		OpString tmp_storage;
		m_request.initial_path.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_SAVE_FOLDER, tmp_storage));

		if (GetPrivacyMode())
		{
			RETURN_IF_ERROR(m_request.initial_path.Set(PrivacyManager::GetInstance()->GetPrivateWindowSaveFolderLocation()));
		}
		else
		{
			RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_SAVE_FOLDER, m_request.initial_path));
		}

		if (finalname && finalname->HasContent())
			m_request.initial_path.Append(finalname->CStr());

		OpString filter;
		RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_ALL_FILES_ASTRIX, filter));

		if (filter.HasContent())
		{
			filter.Append(UNI_L("|*.*|"));
			RETURN_IF_ERROR(StringFilterToExtensionFilter(filter.CStr(), &m_request.extension_filters));
		}

		m_request.initial_filter = -1;
		m_request.fixed_filter = FALSE;

		ret = m_chooser->Execute(listener_callback ? listener_callback->GetParentWindow() : parent, this, m_request);
	}

	return ret;
}

BOOL DownloadItem::HasDefaultViewer()
{
	if (!m_original_viewer)
		m_original_viewer = FindViewer();
	return m_original_viewer != NULL;
}
