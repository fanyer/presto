/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#include "adjunct/quick/dialogs/SetupApplyDialog.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/hotlist/HotlistModel.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/managers/WebServerManager.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/encodings/detector/charsetdetector.h"
#include "modules/locale/src/opprefsfilelanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefsfile/accessors/ini.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/skin/OpSkin.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/url/url2.h"
#include "modules/util/filefun.h"
#include "modules/util/zipload.h"
#include "modules/windowcommander/src/WindowCommander.h"

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/search/searchenginemanager.h"
#endif // DESKTOP_UTIL_SEARCH_ENGINES

#if defined(MSWIN)
# include "platforms/windows/windows_ui/res/#buildnr.rci"
#elif defined(UNIX)
# include "platforms/unix/product/config.h"
#elif defined(_MACINTOSH_)
# include "platforms/mac/Resources/buildnum.h"
#elif defined(WIN_CE)
# include "platforms/win_ce/res/buildnum.h"
#else
# pragma error("include the file where VER_BUILD_NUMBER is defined")
#endif

#if defined (MSWIN)
#include "platforms/windows/win_handy.h"
#endif



/**
 * Allocate and initialize the static class pointer to the last dialog.
 */
SetupApplyDialog* SetupApplyDialog::c_last_started_download = NULL;


/*****************************************************************************
***                                                                        ***
*** SetupApplyDialog                                                       ***
***                                                                        ***
*****************************************************************************/

SetupApplyDialog::SetupApplyDialog()
{
	InitData();
	InsertInQueue();
}

SetupApplyDialog::SetupApplyDialog(URL* url)
{
	InitData();
	InsertInQueue();
	SetURL(url);
}

SetupApplyDialog::SetupApplyDialog(const OpStringC & full_url, URLContentType content_type)
{
	InitData();
	InsertInQueue();
	SetURLInfo(full_url, content_type);
}

void SetupApplyDialog::InitData()
{
	m_transfer = NULL;
	m_oldopfile = NULL;
	m_gadget_treemodelitem_id = -1;
	m_configuration_collection = NULL;
	m_next_download_in_line = NULL;
	m_previous_download_in_line = NULL;
}

void SetupApplyDialog::InsertInQueue()
{
	if (c_last_started_download)
	{
		m_previous_download_in_line = c_last_started_download;
		c_last_started_download->SetNextInLine(this);
		m_queued = TRUE;
	}
	else
	{
		m_queued = FALSE;
	}
	c_last_started_download = this;
}

void SetupApplyDialog::RemoveFromQueue()
{
	if (c_last_started_download == this)
	{
		c_last_started_download = m_previous_download_in_line;
	}
	if (m_next_download_in_line)
	{
		m_next_download_in_line->SetAheadInLine(m_previous_download_in_line);
	}
	if (m_previous_download_in_line)
	{
		m_previous_download_in_line->SetNextInLine(m_next_download_in_line);
	}
	else
	{
		// There is no download ahead of this in line, so this is a running download.
		if (m_next_download_in_line)
		{
			// There is a download waiting in line that needs to be started.
			m_next_download_in_line->StartDownloading(TRUE);
		}
	}
}

OP_STATUS SetupApplyDialog::StartDownloading(BOOL was_queued)
{
	OP_STATUS status = OpStatus::OK;
	BOOL already_transferring = FALSE;

	// Need to store the current state so that we can fallback to it if the setup is rejected by the user.
	if(m_setuptype == OPSKIN_SETUP)
	{
		OP_ASSERT(!"Should not be used for skins, use the toolbar instead");
	}
	else
	{
		switch(m_setuptype)
		{
		case OPTOOLBAR_SETUP:
			{
				m_oldopfile = OP_NEW(OpFile, ());
				if (m_oldopfile)
				{
					TRAPD(err, g_pcfiles->GetFileL(PrefsCollectionFiles::ToolbarConfig, *m_oldopfile));
					OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?
				}
			}
			break;
		case OPMENU_SETUP:
			{
				m_oldopfile = OP_NEW(OpFile, ());
				if (m_oldopfile)
				{
					TRAPD(err, g_pcfiles->GetFileL(PrefsCollectionFiles::MenuConfig, *m_oldopfile));
					OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?
				}
			}
			break;
		case OPMOUSE_SETUP:
			{
				m_oldopfile = OP_NEW(OpFile, ());
				if (m_oldopfile)
				{
					TRAPD(err, g_pcfiles->GetFileL(PrefsCollectionFiles::MouseConfig, *m_oldopfile));
					OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?
				}
			}
			break;
		case OPKEYBOARD_SETUP:
			{
				m_oldopfile = OP_NEW(OpFile, ());
				if (m_oldopfile)
				{
					TRAPD(err, g_pcfiles->GetFileL(PrefsCollectionFiles::KeyboardConfig, *m_oldopfile));
					OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?
				}
			}
			break;
		}
	}

	if(m_transfer)	// redirect causes a new download
	{
		((TransferManager*)g_transferManager)->ReleaseTransferItem((OpTransferItem*)m_transfer);
	}

	RETURN_IF_ERROR(((TransferManager*)g_transferManager)->GetTransferItem(&m_transfer, m_urlstring.CStr(), &already_transferring));

	already_transferring = already_transferring && (m_transfer->GetHaveSize() != 0);

	if(m_transfer && !already_transferring)
	{
		m_url = m_transfer->GetURL();

		MessageHandler *oldHandler = m_url->GetFirstMessageHandler();

		m_url->ChangeMessageHandler(oldHandler ? oldHandler : g_main_message_handler, g_main_message_handler);

//		if(m_url->Status(0) == URL_UNLOADED)	//hack to work around failed second download.
		{
			URL tmp;
			m_url->Reload(g_main_message_handler, tmp, FALSE, TRUE);
		}
		m_transfer->SetTransferListener(this);

	}
	else
	{
		m_transfer = NULL;
	}

	m_patchtype = OpSetupManager::NONE;
	m_tempmergefile = NULL;

	if(was_queued && m_transfer == NULL) // If this download was queued, the download won't have been checked by the OnInit function, as it will not have been running at that time.
	{
		OnProgress(m_transfer, OpTransferListener::TRANSFER_ABORTED);
	}
	return status;
}


OP_STATUS SetupApplyDialog::SetURL(URL * url)
{
	OpString url_string;
	RETURN_IF_ERROR(url->GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI, url_string));
	return SetURLInfo(url_string.CStr(), url->ContentType());
}


OP_STATUS SetupApplyDialog::SetURLInfo(const OpStringC & full_url, URLContentType content_type)
{
	OP_STATUS status = OpStatus::OK;

	RETURN_IF_ERROR(m_urlstring.Set(full_url));
	if (content_type == URL_MULTI_CONFIGURATION_CONTENT)
	{
		m_setup_is_multi_setup = TRUE;
	}
	else
	{
		m_setup_is_multi_setup = FALSE;
		switch(content_type)
		{
		case URL_MENU_CONFIGURATION_CONTENT:
			m_setuptype = OPMENU_SETUP;
			break;
		case URL_TOOLBAR_CONFIGURATION_CONTENT:
			m_setuptype = OPTOOLBAR_SETUP;
			break;
		case URL_MOUSE_CONFIGURATION_CONTENT:
			m_setuptype = OPMOUSE_SETUP;
			break;
		case URL_KEYBOARD_CONFIGURATION_CONTENT:
			m_setuptype = OPKEYBOARD_SETUP;
			break;
		case URL_SKIN_CONFIGURATION_CONTENT:
			OP_ASSERT(!"Should not be used for skins, use the toolbar instead");
			break;
		default:
			//OP_ASSERT(FALSE);			// unsupported filetype, will fallback to the previous set
			break;
		}
	}
	if (!m_queued) // If this is the only running download, start it. If not, it will be started by the previous download when that is done.
	{
		status = StartDownloading(FALSE);
	}
	return status;
}



SetupApplyDialog::~SetupApplyDialog()
{
	if(m_transfer)
	{
		m_transfer->SetTransferListener(NULL);
		((TransferManager*)g_transferManager)->ReleaseTransferItem((OpTransferItem*)m_transfer);
	}
	OP_DELETE(m_configuration_collection);
	OP_DELETE(m_oldopfile);
	RemoveFromQueue();
}

void SetupApplyDialog::OnInit()
{
	ShowWidget("Download_progress_text", FALSE);

	// If there are more than one download dialog, cascade them.
	if (m_previous_download_in_line)
	{
		OpRect rect;
		OpWindow::State state;
		m_previous_download_in_line->GetOpWindow()->GetDesktopPlacement(rect, state);
		if ((INTPTR)m_previous_download_in_line % 2 == 0)
		{
			rect.x += 25;
			rect.y += 25;
			GetOpWindow()->SetDesktopPlacement(rect, state);
		}
	}

	OpString download_opera_setupfile, downloading_setupfile;

	if (m_setup_is_multi_setup)
	{
		const OpStringC language_server(UNI_L("http://xml.opera.com/lang"));
		if (!language_server.Compare(m_urlstring.CStr(), language_server.Length()))
		{
			g_languageManager->GetString(Str::D_SETUP_APPLY_LANGUAGE_TITLE, download_opera_setupfile);
			g_languageManager->GetString(Str::D_SETUP_APPLY_LANGUAGE_CONTENT, downloading_setupfile); // TODO: Adapt to the language downloaded.
		}
		else
		{
			g_languageManager->GetString(Str::D_SETUP_APPLY_MULTI_SETUP_TITLE, download_opera_setupfile);
			g_languageManager->GetString(Str::D_SETUP_APPLY_MULTI_SETUP_CONTENT, downloading_setupfile);
		}
	}
	else
	{
		switch(m_setuptype)
		{
		case OPTOOLBAR_SETUP:
			{
				g_languageManager->GetString(Str::S_DOWNLOAD_OPERA_TOOLBARSETUP, download_opera_setupfile);
				g_languageManager->GetString(Str::S_DOWNLOADING_TOOLBARSETUP, downloading_setupfile);
			}
			break;
		case OPMENU_SETUP:
			{
				g_languageManager->GetString(Str::S_DOWNLOAD_OPERA_MENUSETUP, download_opera_setupfile);
				g_languageManager->GetString(Str::S_DOWNLOADING_MENUSETUP, downloading_setupfile);
			}
			break;
		case OPMOUSE_SETUP:
			{
				g_languageManager->GetString(Str::S_DOWNLOAD_OPERA_MOUSESETUP, download_opera_setupfile);
				g_languageManager->GetString(Str::S_DOWNLOADING_MOUSESETUP, downloading_setupfile);
			}
			break;
		case OPKEYBOARD_SETUP:
			{
				g_languageManager->GetString(Str::S_DOWNLOAD_OPERA_KEYBOARDSETUP, download_opera_setupfile);
				g_languageManager->GetString(Str::S_DOWNLOADING_KEYBOARDSETUP, downloading_setupfile);
			}
			break;
		}
	}
	SetTitle(download_opera_setupfile.CStr());
	SetWidgetText("Question", downloading_setupfile.CStr());

	ShowButton(0, FALSE);

	Dialog::OnInit();

	if(!m_queued && m_transfer == NULL)
	{
		OnProgress(m_transfer, OpTransferListener::TRANSFER_ABORTED);
	}
}


void SetupApplyDialog::OnCancel()
{
	if (!m_previous_download_in_line) // Only unapply the setup if it was actually applied and not waiting in a queue.
	{
		if (m_setup_is_multi_setup)
		{
			// Handle this here
			if (m_configuration_collection)
			{
				RollbackToPreviousSetup();
			}
		}
		else
		{
			if(m_tempmergefile)
			{
				//make sure we don't screw up the fallback(defaults) file
				if(!g_setup_manager->IsFallback(m_oldopfile->GetFullPath(), m_setuptype))
				{
					OpFile* oldfile = static_cast<OpFile *>(m_oldopfile->CreateCopy());

					if (OpStatus::IsError(oldfile->CopyContents(m_tempmergefile, FALSE)))
					{
						OP_ASSERT(0);	// oops
						return;
					}
					OP_DELETE(oldfile);
				}
				m_tempmergefile->Close();
				m_tempmergefile->Delete();
				OP_DELETE(m_tempmergefile);
			}

			if (m_oldopfile)
			{
				if (0 != (m_oldsetupfile = OP_NEW(PrefsFile, (PREFS_STD))))
				{
					TRAPD(err, m_oldsetupfile->ConstructL(); 
							   m_oldsetupfile->SetFileL(m_oldopfile); 
							   m_oldsetupfile->LoadAllL());
					OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?
					g_setup_manager->SelectSetupByFile(m_oldsetupfile, m_setuptype);
				}
			}

			// Remove file from disk
			if( m_setupfilename.CStr() )
			{
				OpFile file;
				file.Construct(m_setupfilename.CStr());
				file.Delete();
			}
		}
	}
}

UINT32 SetupApplyDialog::OnOk()
{
	if (!m_previous_download_in_line && m_tempmergefile) // Nothing to do if the dialog is still waiting in line.
	{
		m_tempmergefile->Close();
		m_tempmergefile->Delete();
		OP_DELETE(m_tempmergefile);
	}
	return 0;
}

void SetupApplyDialog::RegularSetup()
{
	// if it's a regular setup ask user if she wants it
	if (OpStatus::IsError(EnableSetupAndAskUser()))
	{
		OpString statusstring;
		g_languageManager->GetString(Str::S_ERROR_APPLYING_SETUP, statusstring);
		SetWidgetText("Question", statusstring.CStr());
	}

}

void SetupApplyDialog::OnProgress(OpTransferItem* transferItem, TransferStatus status)
{
	//update progressbar

	OpProgressBar* progressbar = (OpProgressBar*) GetWidgetByName("Download_progress_bar");

	if(status == OpTransferListener::TRANSFER_DONE)
	{
		// Raise the dialog to make it stand out from other download dialogs.
		GetOpWindow()->Raise();

		if(transferItem)
		{
			transferItem->SetTransferListener(NULL);
		}

		if(progressbar)
		{
			progressbar->SetProgress(100, 100);
			progressbar->SyncLayout();
			progressbar->Sync();
		}

		if (m_setup_is_multi_setup)
		{
			m_configuration_collection = OP_NEW(ConfigurationCollection, ());
			OP_STATUS result = ExtractMultiConfigurationFromReceivedZip(transferItem);
			if (!OpStatus::IsSuccess(result))
			{
				OP_DELETE(m_configuration_collection);
				m_configuration_collection = NULL;
			}
		}

		// add warning dialog here if:

		// 1. not from opera.com
		// 2. contains "Execute program"

		if(IsAThreat(transferItem))
		{
			INT32 result = 0;

			URL* thisurl = transferItem->GetURL();
			ServerName* server_name = thisurl->GetServerName();
			char* name = NULL;

			if (server_name)
			{
				name = (char*)server_name->Name();
			}

			SetupApplyDialog_ConfirmDialog* dialog = OP_NEW(SetupApplyDialog_ConfirmDialog, ());
			
			if (dialog)
			{
				dialog->Init(this, this, name, &result);
			}
			return;
		}
		RegularSetup();
	}
	else if(status == OpTransferListener::TRANSFER_PROGRESS)
	{
		if(transferItem->GetSize() == 0)
		{					//we don't have any possibility to show progress for this file, just show downloaded bytes
			if (transferItem->GetHaveSize())
			{
				OpString format;
				g_languageManager->GetString(Str::S_SKIN_DOWNLOADED_BYTES, format);

				OpString temp_var;
				temp_var.AppendFormat(format.CStr(), transferItem->GetHaveSize());
				SetWidgetText("Download_progress_text", temp_var.CStr());
				ShowWidget("Download_progress_text", TRUE);
				ShowWidget("Download_progress_bar", FALSE);
			}
		}
		else
		{
			ShowWidget("Download_progress_text", FALSE);
			ShowWidget("Download_progress_bar", TRUE);

			if(progressbar)
			{
				progressbar->SetProgress((UINT)transferItem->GetHaveSize(), (UINT)transferItem->GetSize());
			}
		}
	}
	else if(status == OpTransferListener::TRANSFER_FAILED || status == OpTransferListener::TRANSFER_ABORTED)
	{
		OpString failed;
		g_languageManager->GetString(Str::S_SKIN_DOWNLOAD_FAILED, failed);
		SetWidgetText("Question", failed.CStr());

		if(transferItem)
		{
			transferItem->SetTransferListener(NULL);
		}
	}

}

void SetupApplyDialog::OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to)
{
	// redirect, update everything
	// maybe we should delete the downloaded redirected file, if any
	// we need to have the same Content-Type on the redirected url
	redirect_to->SetAttribute(URL::KForceContentType, redirect_from->ContentType());
}

static int FindStringI( const char* buffer, INT32 buffer_length, const char* string_pattern )
{
	// buffer  wzxyapple0 bl=10
	// pattern apple0     pl=5
	// string test bl-pl=5 first characters in buffer

	if( !string_pattern )
		return -1;
	int string_pattern_length = strlen(string_pattern);
	if( string_pattern_length >= buffer_length || string_pattern_length == 0 )
		return -1;

	for( INT32 i=0; i < buffer_length-string_pattern_length; i++ )
	{
		if( op_strnicmp(buffer+i, string_pattern, string_pattern_length ) == 0 )
		{
			if( buffer[i+string_pattern_length] == 0 ) // Match terminating 0 as well.
				return i;
		}
	}

	return -1;
}

// Check if the content could contain security risks
// 1. Check if the content came from opera.com
// 2. Check if the content contains execute actions
BOOL SetupApplyDialog::IsAThreat(OpTransferItem* transferItem)
{
	// check if the domain is opera.com

	URL* thisurl = transferItem->GetURL();

	//first check if this is from an other server, if yes tell the user that this may be dangerous
	ServerName* server_name = thisurl->GetServerName();
	if (server_name)
	{
		int servername_len = strlen(server_name->Name());
		if(servername_len >= 9)
		{
			const char* t = server_name->Name()+(strlen(server_name->Name())-9);
			//check if the servername _ends_ with opera.com
			return (strcmp(t, "opera.com") == 0) ? FALSE : TRUE;
		}
		else
		{
			return TRUE;
		}
	}

	// else check if the file downloaded contains execute
	if (m_setup_is_multi_setup && m_configuration_collection)
	{
		return ScanMultiSetupForExecuteProgramStatements();
	}
	else
	{
		URL_DataDescriptor* desc = thisurl->GetDescriptor(NULL, TRUE);
		if(!desc)
 		{
 			return TRUE;
 		}

		BOOL more = TRUE;
		while(more && desc)
		{
			OP_MEMORY_VAR unsigned long ret = 0;
			TRAPD(err,ret = desc->RetrieveDataL(more));
			if (OpStatus::IsError(err))
 			{
 				return TRUE;	// an error occurred, to be on the safe side alert
 			}
			else if(ret)
			{
				unsigned buflen = desc->GetBufSize();

				if( FindStringI( desc->GetBuffer(), buflen, "Execute program" ) != -1 )
				{
					OP_DELETE(desc);
					return TRUE;
				}

				desc->ConsumeData(desc->GetBufSize());
			}
		}
		OP_DELETE(desc);
	}
	return FALSE;
}

BOOL SetupApplyDialog::CheckFileHeader(URL* thisurl)
{
	// since zipfiles will be checked elsewhere only check types that should come in clear
	URL_DataDescriptor* desc = thisurl->GetDescriptor(NULL, TRUE);
	if(!desc)
 	{
 		return FALSE;
 	}

	BOOL more = TRUE;
	while(more && desc)
	{
		OP_MEMORY_VAR unsigned long ret = 0;
		TRAPD(err,ret = desc->RetrieveDataL(more));
		if (OpStatus::IsError(err))
 		{
 			return FALSE;	// an error occurred, to be on the safe side alert
 		}
		else if(ret)
		{
			if (IniAccessor::IsRecognizedHeader(desc->GetBuffer(), desc->GetBufSize()) == IniAccessor::HeaderCurrent)
			{
				OP_DELETE(desc);
				return TRUE;
			}
			desc->ConsumeData(desc->GetBufSize());
		}
	}
	OP_DELETE(desc);

	return FALSE;
}

OP_STATUS SetupApplyDialog::SaveDownload(OpStringC directory)
{
	// check fileheader if it's safe to store

	// 2 Retrieve suggested file name
	OpString suggested_filename;

	RETURN_IF_LEAVE(m_url->GetAttributeL(URL::KSuggestedFileName_L, suggested_filename, TRUE));

	if (suggested_filename.IsEmpty())
	{
		RETURN_IF_ERROR(suggested_filename.Set("default"));

		OpString ext;
		TRAPD(op_err, m_url->GetAttributeL(URL::KSuggestedFileNameExtension_L, ext, TRUE));
		RETURN_IF_ERROR(op_err);
		if (ext.HasContent())
		{
			RETURN_IF_ERROR(suggested_filename.Append("."));
			RETURN_IF_ERROR(suggested_filename.Append(ext));
		}
		if (suggested_filename.IsEmpty())
		{
			return OpBoolean::IS_FALSE;
		}
	}

	// 3 Build unique file name
	OpString filename;
	RETURN_IF_ERROR(filename.Set(directory));
	if( filename.HasContent() && filename[filename.Length()-1] != PATHSEPCHAR )
	{
		RETURN_IF_ERROR(filename.Append(PATHSEP));
	}

	RETURN_IF_ERROR(filename.Append(suggested_filename.CStr()));

	BOOL exists = FALSE;

	OpFile file;
	RETURN_IF_ERROR(file.Construct(filename.CStr()));
	RETURN_IF_ERROR(file.Exists(exists));

	if (exists)
	{
		RETURN_IF_ERROR(CreateUniqueFilename(filename));
	}

	// 4 Save file to disk
	URL* url = m_transfer->GetURL();
	if( url )
	{
		url->LoadToFile(filename.CStr());
		m_setupfilename.Set(filename);
	}

	return OpStatus::OK;
}

OP_STATUS SetupApplyDialog::EnableSetupAndAskUser()
{
	OpString apply_downloaded_file_title, keep_this_setup;

	if (m_setup_is_multi_setup)
	{
		if (m_configuration_collection)
		{
			g_languageManager->GetString(Str::D_SETUP_APPLY_ERROR_TITLE, apply_downloaded_file_title);
			g_languageManager->GetString(Str::D_SETUP_APPLY_ERROR_QUESTION, keep_this_setup);
			if (OpStatus::IsSuccess(SaveMultiSetupFiles()) && OpStatus::IsSuccess(InstallSetupFiles()))
			{
				OpStatus::Ignore(InstallOtherSettings());
				g_languageManager->GetString(Str::D_SETUP_APPLY_QUESTION_APPLY_TITLE, apply_downloaded_file_title);
				g_languageManager->GetString(Str::D_SETUP_APPLY_QUESTION_APPLY_TEXT, keep_this_setup);

				ConfigurationCollection::ReadmeFile *readme = m_configuration_collection->readme_file;
				if (readme && readme->mem_file)
				{
					PrefsFile readme_file(PREFS_INI);
					OpString name, author, version, label;
					TRAPD(err, readme_file.ConstructL(); 
							   readme_file.SetFileL(readme->mem_file); 
							   readme_file.LoadAllL(); 
							   readme_file.ReadStringL("INFO", "NAME", name); 
							   readme_file.ReadStringL("INFO", "AUTHOR", author); 
							   readme_file.ReadStringL("INFO", "VERSION", version));

				   OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?

					if (name.HasContent())
					{
						g_languageManager->GetString(Str::D_SETUP_APPLY_SETUP_NAME, label);
						SetWidgetText("Name_label", label.CStr());
						if (version.HasContent())
						{
							OpString temp_var;
							temp_var.AppendFormat(UNI_L("%s (%s)"), name.CStr(), version.CStr());
							SetWidgetText("Skin_name", temp_var.CStr() );
						}
						else
						{
							SetWidgetText("Skin_name", name.CStr());
						}
					}
					if (author.HasContent())
					{
						SetWidgetText("Skin_author", author.CStr());
						g_languageManager->GetString(Str::S_SKIN_AUTHOR, label);
						SetWidgetText("Author_label", label.CStr());
					}
				}
			}
		}
	}
	else
	{
		switch(m_setuptype)
		{
		case OPTOOLBAR_SETUP:
			{
				g_languageManager->GetString(Str::S_APPLY_DOWNLOADED_TOOLBARSETUP, apply_downloaded_file_title);
				g_languageManager->GetString(Str::S_KEEP_THIS_TOOLBARSETUP, keep_this_setup);

				if(!CheckFileHeader(m_url))
				{
					//g_application->ShowDialog(g_application->GetActiveDesktopWindow(), TRANSLAAATE_ME("The toolbar file header was not accepted"), TRANSLAAATE_ME("Setup not applied."), Dialog::TYPE_OK, Dialog::IMAGE_INFO);
					//CloseDialog(TRUE);
					return OpStatus::ERR;
				}
				OpString tmp_storage;
				OpStatus::Ignore(SaveDownload(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_TOOLBARSETUP_FOLDER, tmp_storage)));
			}
			break;
		case OPMENU_SETUP:
			{
				g_languageManager->GetString(Str::S_DOWNLOAD_OPERA_MENUSETUP, apply_downloaded_file_title);
				g_languageManager->GetString(Str::S_KEEP_THIS_MENUSETUP, keep_this_setup);
				if(!CheckFileHeader(m_url))
				{
					//CloseDialog(TRUE);
					return OpStatus::ERR;
				}
				OpString tmp_storage;
				OpStatus::Ignore(SaveDownload(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_MENUSETUP_FOLDER, tmp_storage)));
			}
			break;
		case OPMOUSE_SETUP:
			{
				g_languageManager->GetString(Str::S_DOWNLOAD_OPERA_MOUSESETUP, apply_downloaded_file_title);
				g_languageManager->GetString(Str::S_KEEP_THIS_MOUSESETUP, keep_this_setup);
				if(!CheckFileHeader(m_url))
				{
					//CloseDialog(TRUE);
					return OpStatus::ERR;
				}
				OpString tmp_storage;
				OpStatus::Ignore(SaveDownload(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_MOUSESETUP_FOLDER, tmp_storage)));
				TRAPD(err, g_setup_manager->ScanSetupFoldersL());
			}
			break;
		case OPKEYBOARD_SETUP:
			{
				g_languageManager->GetString(Str::S_DOWNLOAD_OPERA_KEYBOARDSETUP, apply_downloaded_file_title);
				g_languageManager->GetString(Str::S_KEEP_THIS_KEYBOARDSETUP, keep_this_setup);
				if(!CheckFileHeader(m_url))
				{
					//CloseDialog(TRUE);
					return OpStatus::ERR;
				}
				OpString tmp_storage;
				OpStatus::Ignore(SaveDownload(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_KEYBOARDSETUP_FOLDER, tmp_storage)));
				TRAPD(err, g_setup_manager->ScanSetupFoldersL());
			}
			break;
		}
	}

	// multi setup or gadget is applied above
	if(!m_setup_is_multi_setup && m_setupfilename.HasContent())
	{
		OpFile file;
		OP_STATUS err = file.Construct(m_setupfilename.CStr());
		OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?
		PrefsFile* prefsfile = OP_NEW(PrefsFile, (PREFS_STD));
		if (prefsfile)
		{
			//check if the file has patch flag set, if yes the file is merged into the existing file
			TRAP(err, prefsfile->ConstructL(); 
					  prefsfile->SetFileL(&file); 
					  prefsfile->LoadAllL(); 
					  m_patchtype = (OpSetupManager::SetupPatchMode)prefsfile->ReadIntL("INFO", "PATCHMODE", 0) );

			OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?

			if(m_patchtype != OpSetupManager::NONE)
			{
				g_setup_manager->MergeSetupIntoExisting(prefsfile, m_setuptype, m_patchtype, m_tempmergefile);
				err = file.Delete();
				OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?
			}
			else
			{
				err = g_setup_manager->SelectSetupByFile(prefsfile, m_setuptype);
			}

			if (OpStatus::IsError(err))
			{
				//CloseDialog(TRUE);
				return err;
			}
		}
	}

	SetTitle(apply_downloaded_file_title.CStr() ? apply_downloaded_file_title.CStr() : UNI_L(""));
	SetWidgetText("Question2", keep_this_setup.CStr());
	ShowButton(0, TRUE);

	OpString loc_str;
	g_languageManager->GetString(Str::DI_IDYES, loc_str);
	SetButtonText(0, loc_str.CStr());
	g_languageManager->GetString(Str::DI_IDNO, loc_str);
	SetButtonText(1, loc_str.CStr());

	SetCurrentPage(1);
	return OpStatus::OK;
}


// debug

OP_STATUS SetupApplyDialog::ExtractMultiConfigurationFromReceivedZip(OpTransferItem* transfer_item)
{
	if (!m_configuration_collection || !transfer_item)
		return OpStatus::ERR;

	URL *url = transfer_item->GetURL();
	if (!url)
		return OpStatus::ERR;

	OpString cached_zip_file_name;
	RETURN_IF_LEAVE(url->GetAttributeL(URL::KFilePathName_L, cached_zip_file_name));

	if (cached_zip_file_name.IsEmpty())
		return OpStatus::ERR;

	OpFile cached_zip_file;
	RETURN_IF_ERROR(cached_zip_file.Construct(cached_zip_file_name.CStr()));
	if (!OpZip::IsFileZipCompatible(&cached_zip_file))
		return OpStatus::ERR;

	OpZip* multi_setup_zip = OP_NEW(OpZip, ());
	if (!multi_setup_zip)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS err;
	if (OpStatus::IsSuccess(err = multi_setup_zip->Init(&cached_zip_file_name)))
	{
		OpAutoVector<OpString> list;
		if (OpStatus::IsSuccess(err = multi_setup_zip->GetFileNameList(list)))
		{
			for (UINT32 idx = 0; idx < list.GetCount();  idx++)
			{
				OpString* item = list.Get(idx);
				OpString complete_location;
				OpString tmp_storage;
				complete_location.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_HOME_FOLDER, tmp_storage));
				complete_location.Append(*item);
				if (IsInstallableSetupFile(complete_location))
				{
					OpMemFile* mem_file;
					mem_file = multi_setup_zip->CreateOpZipFile(item);
					ConfigurationCollection::InstallableFile* inst_file = OP_NEW(ConfigurationCollection::InstallableFile, ());
					if (inst_file)
					{
						inst_file->mem_file = mem_file;
						inst_file->listed_location.Set(complete_location);
						m_configuration_collection->installable_list.Add(inst_file);
					}
				}
				else if (IsMergeableIniFile(complete_location))
				{
					OpMemFile* mem_file;
					mem_file = multi_setup_zip->CreateOpZipFile(item);
					SetupApplyDialog::ConfigurationCollection::MainIniFile* ini_file = OP_NEW(SetupApplyDialog::ConfigurationCollection::MainIniFile, ());
					if (ini_file)
						ini_file->mem_file = mem_file;
					m_configuration_collection->main_ini_file = ini_file;
				}
				else if (IsReadmeFile(complete_location))
				{
					OpMemFile* mem_file;
					mem_file = multi_setup_zip->CreateOpZipFile(item);
					SetupApplyDialog::ConfigurationCollection::ReadmeFile* readme_file = OP_NEW(SetupApplyDialog::ConfigurationCollection::ReadmeFile, ());
					if (readme_file)
						readme_file->mem_file = mem_file;
					m_configuration_collection->readme_file = readme_file;
				}
			}
		}
	}
	OP_DELETE(multi_setup_zip);
	return err;
}

BOOL SetupApplyDialog::ScanMultiSetupForExecuteProgramStatements()
{
	if (!m_configuration_collection || !m_setup_is_multi_setup)
		return FALSE;

	OpString8 file_buffer;
	for (unsigned int idx = 0; idx < m_configuration_collection->installable_list.GetCount(); idx++)
	{
		ConfigurationCollection::InstallableFile *inst_file = m_configuration_collection->installable_list.Get(idx);
		OpMemFile* current_mem_file = inst_file->mem_file;

		if (current_mem_file == NULL) // || inst_file->listed_location == NULL
		{
			continue;
		}

		int lastperiod = inst_file->listed_location.FindLastOf('.');
		if (lastperiod != KNotFound && !inst_file->listed_location.SubString(lastperiod+1).Compare("ini")) // Only relevant to check .ini-files...
		{
			OpFileLength filesize = current_mem_file->GetFileLength();
			if (!file_buffer.Reserve((int)filesize))
				return TRUE;

			current_mem_file->Open(OPFILE_READ);
			OpFileLength bytes_read;
			current_mem_file->Read(file_buffer.CStr(), filesize, &bytes_read);
			file_buffer[(int)bytes_read] = 0;
			current_mem_file->Close();
			if (file_buffer.FindI("Execute program") != KNotFound)
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

OP_STATUS SetupApplyDialog::SaveMultiSetupFiles()
{
	if (!m_configuration_collection)
		return OpStatus::OK;

	for (unsigned int i = 0; i < m_configuration_collection->installable_list.GetCount(); i++)
	{
		ConfigurationCollection::InstallableFile* cur_inst_file = m_configuration_collection->installable_list.Get(i);
		OP_STATUS error;
		if (OpStatus::IsError(error = SaveMemFile(cur_inst_file->mem_file, cur_inst_file->listed_location, cur_inst_file->real_location)))
		{
			// If something fails, we delete all previously installed files and return an error code.
			for (int j = i; j >= 0; j--)
			{
				ConfigurationCollection::InstallableFile* inst_file = m_configuration_collection->installable_list.Get(i);
				if (inst_file->real_location.HasContent())
				{
					OpFile file;
					file.Construct(inst_file->real_location.CStr());
					file.Delete();
				}
			}
			return error;
		}
	}
	return OpStatus::OK;
}

OP_STATUS SetupApplyDialog::SaveMemFile(OpMemFile* mem_file, const OpStringC intended_location, OpString &real_location)
{
	if (!mem_file)
		return OpStatus::ERR;

	BOOL exists;
	OpFile file;

	RETURN_IF_ERROR(file.Construct(intended_location.CStr()));
	RETURN_IF_ERROR(file.Exists(exists));

	real_location.Set(intended_location);
	if (exists)
	{
		RETURN_IF_ERROR(CreateUniqueFilename(real_location));
	}
	PrefsFile prefsfile(PREFS_STD);
	int patchtype;

	RETURN_IF_LEAVE(prefsfile.ConstructL(); 
					prefsfile.SetFileL(mem_file); 
					prefsfile.LoadAllL(); 
					patchtype = prefsfile.ReadIntL("INFO","PATCHMODE", 0));

	if (patchtype != OpSetupManager::NONE)
	{
		// Merge
		// Make a copy of the file to merge into. Filename decided by the suggested filename in the zip
		// merge the mem-file with that copy
		// store the file of the new, merged, file as real_location.
		// TODO: Complete this!
		OP_ASSERT(FALSE);
	}
	else
	{
		// Save
		OpFile targ_file;
		RETURN_IF_ERROR(targ_file.Construct(real_location.CStr()));
		RETURN_IF_ERROR(targ_file.Open(OPFILE_WRITE));
		int blocksize = 16384;
		OpString8 temp_buffer;
		if (!temp_buffer.Reserve(blocksize))
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(mem_file->Open(OPFILE_READ));
		OpFileLength bytes_read;
		while (!mem_file->Eof())
		{
			mem_file->Read(temp_buffer.CStr(), blocksize, &bytes_read);
			targ_file.Write(temp_buffer.CStr(), bytes_read);
		}
		mem_file->Close();
		targ_file.Close();
	}
	return OpStatus::OK;
}

OP_STATUS SetupApplyDialog::InstallSetupFiles()
{
	if (!m_configuration_collection)
		return OpStatus::ERR;

	ConfigurationCollection::MainIniFile *main_ini_file = m_configuration_collection->main_ini_file;
	if (!main_ini_file)
		return OpStatus::OK;

	PrefsFile opera_ini_file(PREFS_INI);
	RETURN_IF_LEAVE(opera_ini_file.ConstructL(); 
					opera_ini_file.SetFileL(main_ini_file->mem_file); 
					opera_ini_file.LoadAllL());

	for (OP_MEMORY_VAR unsigned int idx = 0; idx < m_configuration_collection->installable_list.GetCount(); idx++)
	{
		OpString location, value;
		ConfigurationCollection::InstallableFile *inst_file = m_configuration_collection->installable_list.Get(idx);

		OpStringC filepath(inst_file->listed_location);
		int last_file_sep_char = filepath.FindLastOf(PATHSEPCHAR);
		if (last_file_sep_char != KNotFound && last_file_sep_char + 1 < filepath.Length())
		{
			location.Set(filepath.CStr(), last_file_sep_char + 1);
		}
		OpString8 key;
		OP_STATUS inner_loop_error;
		if (OpStatus::IsSuccess(inner_loop_error = GetIniKeyCorrespondingToPath(location, key)))
		{
			TRAP(inner_loop_error, opera_ini_file.ReadStringL("User Prefs", key, value));
		}

		if (OpStatus::IsSuccess(inner_loop_error) && value.HasContent())
		{
			// Ensure that the setups work cross platform.
			for (int string_counter = 0; string_counter < value.Length(); string_counter++)
			{
				if (value[string_counter] == '/' || value[string_counter] == '\\')
				{
					value[string_counter] = PATHSEPCHAR;
				}
			}
			OpString complete_location;
			OpString tmp_storage;
			complete_location.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_HOME_FOLDER, tmp_storage));
			complete_location.Append(value);
			if (!complete_location.Compare(filepath))
			{
				ConfigurationCollection::RollbackInformation *rollback_info = &m_configuration_collection->rollback_information;
				if (!key.Compare("Menu Configuration"))
				{
					rollback_info->menu_rollback_needed = TRUE;
					rollback_info->menu_rollback_setup.Set(((OpFile*)g_setup_manager->GetSetupFile(OPMENU_SETUP)->GetFile())->GetFullPath());
					g_setup_manager->SelectSetupByFile(inst_file->real_location.CStr(), OPMENU_SETUP);
				}
				else if (!key.Compare("Toolbar Configuration"))
				{
					rollback_info->toolbar_rollback_needed = TRUE;
					rollback_info->toolbar_rollback_setup.Set(((OpFile*)g_setup_manager->GetSetupFile(OPTOOLBAR_SETUP)->GetFile())->GetFullPath());
					g_setup_manager->SelectSetupByFile(inst_file->real_location.CStr(), OPTOOLBAR_SETUP);
				}
				else if (!key.Compare("Mouse Configuration"))
				{
					rollback_info->mouse_rollback_needed = TRUE;
					rollback_info->mouse_rollback_setup.Set(((OpFile*)g_setup_manager->GetSetupFile(OPMOUSE_SETUP)->GetFile())->GetFullPath());
					g_setup_manager->SelectSetupByFile(inst_file->real_location.CStr(), OPMOUSE_SETUP);
				}
				else if (!key.Compare("Keyboard Configuration"))
				{
					rollback_info->keyboard_rollback_needed = TRUE;
					rollback_info->keyboard_rollback_setup.Set(((OpFile*)g_setup_manager->GetSetupFile(OPKEYBOARD_SETUP)->GetFile())->GetFullPath());
					g_setup_manager->SelectSetupByFile(inst_file->real_location.CStr(), OPKEYBOARD_SETUP);
				}
				else if (!key.Compare("Language File"))
				{
					OpString new_lang, old_lang;
					new_lang.Set(inst_file->real_location);
					old_lang.Empty();
					if (OpStatus::IsSuccess(SetAndActivateNewLanguage(new_lang, old_lang)))
					{
						rollback_info->language_rollback_needed = TRUE;
						rollback_info->language_rollback_setup.Set(old_lang);
					}
				}
				else if (!key.Compare("Windows Storage File"))
				{
					rollback_info->session_rollback_needed = TRUE;

					OpFile winstorfile;
					TRAPD(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::WindowsStorageFile, winstorfile));
					OP_ASSERT(OpStatus::IsSuccess(rc)); // FIXME: do what if rc is bad ?

					rollback_info->session_rollback_setup.Set(winstorfile.GetFullPath());
					rollback_info->session_startuptype_rollback_state = STARTUPTYPE(g_pcui->GetIntegerPref(PrefsCollectionUI::StartupType));;
					OpFile session_file;
					OP_STATUS session_error = session_file.Construct(inst_file->real_location.CStr());
					if (OpStatus::IsSuccess(session_error))
					{
						g_pcfiles->WriteFilePrefL(PrefsCollectionFiles::WindowsStorageFile, &session_file);
					}
					OP_MEMORY_VAR int startuptype;
					TRAP(session_error, startuptype = opera_ini_file.ReadIntL("User Prefs","Startup Type"));

					BOOL permanent_homepage = FALSE;
#ifdef PERMANENT_HOMEPAGE_SUPPORT
					permanent_homepage = (1 == g_pcui->GetIntegerPref(PrefsCollectionUI::PermanentHomepage));
#endif
					if ((!permanent_homepage) &&
						OpStatus::IsSuccess(session_error) &&
						(STARTUPTYPE)startuptype > _STARTUP_FIRST_ENUM &&
						(STARTUPTYPE)startuptype < _STARTUP_LAST_ENUM)
					{
						TRAPD(throwaway_error, g_pcui->WriteIntegerL(PrefsCollectionUI::StartupType, (STARTUPTYPE)startuptype));
						OpStatus::Ignore(throwaway_error);
					}
				}
				else if (!key.Compare("Button Set"))
				{
					rollback_info->skin_rollback_needed = TRUE;
					const OpFile *buttonset = g_pcfiles->GetFile(PrefsCollectionFiles::ButtonSet);
					rollback_info->skin_rollback_setup.Set(buttonset->GetFullPath());

					OpFile skin_file;
					OP_STATUS skin_error = skin_file.Construct(inst_file->real_location.CStr());
					if (OpStatus::IsSuccess(skin_error))
					{
						if (!OpStatus::IsSuccess(g_skin_manager->SelectSkinByFile(&skin_file)))
						{
							// The skin had a bad version number, perhaps? TODO: Deal with this situation...
						}
					}
				}
				else
				{
					OP_ASSERT(FALSE);
					return OpStatus::ERR; // This is an error, we shouldn't get here...
				}
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS SetupApplyDialog::InstallOtherSettings()
{
	if (!m_configuration_collection)
		return OpStatus::ERR;

	ConfigurationCollection::MainIniFile *main_ini_file = m_configuration_collection->main_ini_file;
	if (!main_ini_file)
		return OpStatus::ERR;

	PrefsFile opera_ini_file(PREFS_INI);
	RETURN_IF_LEAVE(opera_ini_file.ConstructL(); 
					opera_ini_file.SetFileL(main_ini_file->mem_file); 
					opera_ini_file.LoadAllL());

	OP_STATUS found_setting;
	OpString string_value;
	OpString8 string8_value;
	ConfigurationCollection::RollbackInformation *rollback_info = &m_configuration_collection->rollback_information;

	TRAP(found_setting, opera_ini_file.ReadStringL("User Prefs", "Home URL", string_value));
	if (OpStatus::IsSuccess(found_setting) && string_value.HasContent())
	{
		rollback_info->home_page_rollback_needed = TRUE;
		rollback_info->home_page_rollback_setup.Set(g_pccore->GetStringPref(PrefsCollectionCore::HomeURL));
		TRAPD(throwaway_error, g_pccore->WriteStringL(PrefsCollectionCore::HomeURL, string_value.CStr()));
		OpStatus::Ignore(throwaway_error);
		string_value.Empty();
	}
	TRAP(found_setting, opera_ini_file.ReadStringL("User Prefs", "Fallback HTML Encoding", string_value));
	if (OpStatus::IsSuccess(found_setting) && string_value.HasContent())
	{
		rollback_info->fallback_encoding_rollback_needed = TRUE;
		rollback_info->fallback_encoding_rollback_setup.Set(g_pcdisplay->GetDefaultEncoding());
		TRAPD(throwaway_error, g_pcdisplay->WriteStringL(PrefsCollectionDisplay::DefaultEncoding, string_value.CStr()));
		OpStatus::Ignore(throwaway_error);
		string8_value.Empty();
	}
	// If you add more setup keys, remember to add those keys to the rollback information and roll back when needed.
	return OpStatus::OK;
}

OP_STATUS SetupApplyDialog::RollbackToPreviousSetup()
{
	if (!m_configuration_collection)
		return OpStatus::ERR;

	ConfigurationCollection::RollbackInformation *rollback_info = &m_configuration_collection->rollback_information;

	if (rollback_info->menu_rollback_needed && rollback_info->menu_rollback_setup.HasContent())
	{
		g_setup_manager->SelectSetupByFile(rollback_info->menu_rollback_setup.CStr(), OPMENU_SETUP);
	}
	if (rollback_info->toolbar_rollback_needed && rollback_info->toolbar_rollback_setup.HasContent())
	{
		g_setup_manager->SelectSetupByFile(rollback_info->toolbar_rollback_setup.CStr(), OPTOOLBAR_SETUP);
	}
	if (rollback_info->keyboard_rollback_needed && rollback_info->keyboard_rollback_setup.HasContent())
	{
		g_setup_manager->SelectSetupByFile(rollback_info->keyboard_rollback_setup.CStr(), OPKEYBOARD_SETUP);
	}
	if (rollback_info->mouse_rollback_needed && rollback_info->mouse_rollback_setup.HasContent())
	{
		g_setup_manager->SelectSetupByFile(rollback_info->mouse_rollback_setup.CStr(), OPMOUSE_SETUP);
	}
	if (rollback_info->language_rollback_needed && rollback_info->language_rollback_setup.HasContent())
	{
		OpString dummy;
		SetAndActivateNewLanguage(rollback_info->language_rollback_setup, dummy);
	}
	if (rollback_info->session_rollback_needed && rollback_info->session_rollback_setup.HasContent())
	{
		OpFile session_file;
		RETURN_IF_ERROR(session_file.Construct(rollback_info->session_rollback_setup.CStr()));
		RETURN_IF_LEAVE(g_pcfiles->WriteFilePrefL(PrefsCollectionFiles::WindowsStorageFile, &session_file));
		RETURN_IF_LEAVE(g_pcui->WriteIntegerL(PrefsCollectionUI::StartupType, rollback_info->session_startuptype_rollback_state));
	}
	if (rollback_info->skin_rollback_needed && rollback_info->skin_rollback_setup.HasContent())
	{
		OpFile skinfile;
		RETURN_IF_ERROR(skinfile.Construct(rollback_info->skin_rollback_setup.CStr()));
		RETURN_IF_ERROR(g_skin_manager->SelectSkinByFile(&skinfile));
	}
	// Home page rollback
	if (rollback_info->home_page_rollback_needed && rollback_info->home_page_rollback_setup.HasContent())
	{
		TRAPD(throwaway_error, g_pccore->WriteStringL(PrefsCollectionCore::HomeURL, rollback_info->home_page_rollback_setup));
		OpStatus::Ignore(throwaway_error);
	}
	// fallback encoding rollback
	if (rollback_info->fallback_encoding_rollback_needed && rollback_info->fallback_encoding_rollback_setup.HasContent())
	{
		OpString defenc;
		defenc.Set(rollback_info->fallback_encoding_rollback_setup);
		TRAPD(throwaway_error, g_pcdisplay->WriteStringL(PrefsCollectionDisplay::DefaultEncoding, defenc));
		OpStatus::Ignore(throwaway_error);
	}
	for (unsigned int i = 0; i < m_configuration_collection->installable_list.GetCount(); i++)
	{
		ConfigurationCollection::InstallableFile* inst_file = m_configuration_collection->installable_list.Get(i);
		if (inst_file->real_location.HasContent())
		{
			// This is a borrow from the original code...
			OpFile file;
			file.Construct(inst_file->real_location.CStr());
			file.Delete();
		}
	}
	return OpStatus::OK;
}

// Helpers from here down...

BOOL SetupApplyDialog::IsLanguagePath(const OpStringC file_path)
{
	int separator_pos = file_path.FindLastOf(PATHSEPCHAR); // slash the filename
	if (separator_pos != KNotFound)
	{
		OpString path;
		path.Set(file_path.CStr(), separator_pos);
		separator_pos = path.FindLastOf(PATHSEPCHAR); // slash the localename
		if (separator_pos != KNotFound)
		{
			OpString locale_folder;
			path.Delete(separator_pos); // Get the folder one level above the requested folder.

			OpString tmp_storage;
			locale_folder.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_HOME_FOLDER, tmp_storage)); // Get the permitted locale folder.
			locale_folder.Append("locale");
			if (!path.Compare(locale_folder))
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

OP_STATUS SetupApplyDialog::GetIniKeyCorrespondingToPath(const OpStringC file_location, OpString8 &key)
{
	int last_path_separator = file_location.FindLastOf(PATHSEPCHAR);
	OpString dir_loc;
	dir_loc.Set(file_location.CStr(), last_path_separator+1);

	/* None of the values returned by GetFolderPath here needs to live
	 * past the if-statement it occurs in, so it should be safe to
	 * reuse the same storage object for all of them.
	 */
	OpString tmp_storage;
	if(!dir_loc.Compare(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_MENUSETUP_FOLDER, tmp_storage)))
	{
		key.Set("Menu Configuration");
		return OpStatus::OK;
	}
	else if (!dir_loc.Compare(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_TOOLBARSETUP_FOLDER, tmp_storage)))
	{
		key.Set("Toolbar Configuration");
		return OpStatus::OK;
	}
	else if (!dir_loc.Compare(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_MOUSESETUP_FOLDER, tmp_storage)))
	{
		key.Set("Mouse Configuration");
		return OpStatus::OK;
	}
	else if (!dir_loc.Compare(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_KEYBOARDSETUP_FOLDER, tmp_storage)))
	{
		key.Set("Keyboard Configuration");
		return OpStatus::OK;
	}
	else if (!dir_loc.Compare(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_SKIN_FOLDER, tmp_storage)))
	{
		key.Set("Button Set");
		return OpStatus::OK;
	}
	else if (!dir_loc.Compare(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_SESSION_FOLDER, tmp_storage)))
	{
		key.Set("Windows Storage File");
		return OpStatus::OK;
	}
	else if (IsLanguagePath(file_location))
	{
		key.Set("Language File");
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

BOOL SetupApplyDialog::IsInstallableSetupFile(const OpStringC filename)
{
	OpFile zip_file;
	RETURN_VALUE_IF_ERROR(zip_file.Construct(filename.CStr()), FALSE);

	OpStringC name(zip_file.GetName());
	int dotpos = name.FindLastOf('.');
	if (dotpos == KNotFound)
		return FALSE;

	OpString extension;
	extension.Set(name.CStr() + dotpos+1);

	// How does this work on MAC ?
	OpString location;
	OpStringC fullpath(zip_file.GetFullPath());
	int last_slash = fullpath.FindLastOf(PATHSEPCHAR);
	if (last_slash != KNotFound)
		location.Set(fullpath.CStr(), last_slash+1);

	if (!name.Compare("search.ini") || !extension.Compare("lng"))
	{
		return IsLanguagePath(filename);
	}

	/* None of the return values of GetFolderPath() here needs to live
	 * beyond its enclosing Compare(), so it should be safe to reuse
	 * the same storage object for all of them.
	 */
	OpString tmp_storage;
	if (   (   !extension.Compare("win")
			&& !location.Compare(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_SESSION_FOLDER, tmp_storage)))
		|| (   !extension.Compare("zip")
			&& !location.Compare(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_SKIN_FOLDER, tmp_storage)))
		||	(  !extension.Compare("ini")
			&& (   !location.Compare(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_MENUSETUP_FOLDER, tmp_storage))
				|| !location.Compare(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_KEYBOARDSETUP_FOLDER, tmp_storage))
				|| !location.Compare(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_MOUSESETUP_FOLDER, tmp_storage))
				|| !location.Compare(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_TOOLBARSETUP_FOLDER, tmp_storage))))
		)
	{
		return TRUE;
	}

	return FALSE;
}

BOOL SetupApplyDialog::IsMergeableIniFile(const OpStringC filename)
{
	int last_slash = filename.FindLastOf(PATHSEPCHAR);
	if (last_slash != KNotFound && (last_slash + 1) < filename.Length())
	{
		OpString path_string, filename_string;
		path_string.Set(filename.CStr(), last_slash + 1);
		filename_string.Set(filename.CStr() + last_slash+1);
		OpString tmp_storage;
		if (!filename_string.Compare("opera.ini") &&
			!path_string.Compare(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_HOME_FOLDER, tmp_storage)))
		{
			return TRUE;
		}
	}
	return FALSE;
}

BOOL SetupApplyDialog::IsReadmeFile(const OpStringC filename)
{
	int last_slash = filename.FindLastOf(PATHSEPCHAR);
	if (last_slash != KNotFound && (last_slash + 1) < filename.Length())
	{
		OpString path_string, filename_string;
		path_string.Set(filename.CStr(), last_slash + 1);
		filename_string.Set(filename.CStr() + last_slash+1);
		OpString tmp_storage;
		if (!filename_string.Compare("readme.txt") &&
			!path_string.Compare(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_HOME_FOLDER, tmp_storage)))
		{
			return TRUE;
		}
	}
	return FALSE;
}

// This function is more or less copy-paste from the preferences dialog
OP_STATUS SetupApplyDialog::SetAndActivateNewLanguage(const OpStringC new_language_file_path, OpString &old_language_file_path)
{
	old_language_file_path.Empty();
	OpFile current_lng_file;
	TRAPD(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::LanguageFile, current_lng_file));
	OP_ASSERT(OpStatus::IsSuccess(rc)); // FIXME: do what if rc is bad ?
	old_language_file_path.Set(current_lng_file.GetFullPath());
	if (old_language_file_path.HasContent() && old_language_file_path.CompareI(new_language_file_path) != 0 )
	{
		OpFile languagefile;
		OP_STATUS err = languagefile.Construct(new_language_file_path.CStr());
		BOOL exists;
		if (OpStatus::IsSuccess(err) && OpStatus::IsSuccess(languagefile.Exists(exists)) && exists)
		{
			TRAP(err, g_pcfiles->WriteFilePrefL(PrefsCollectionFiles::LanguageFile, &languagefile));
			OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
			if (g_searchEngineManager->HasLoadedConfig())
			{
				TRAP(err, g_searchEngineManager->LoadSearchesL());
				OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?
			}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
			g_application->SettingsChanged(SETTINGS_LANGUAGE);
			g_application->SettingsChanged(SETTINGS_MENU_SETUP);
			g_application->SettingsChanged(SETTINGS_TOOLBAR_SETUP);
			return OpStatus::OK;
		}
	}
	return OpStatus::ERR;
}

/*****************************************************************************
***                                                                        ***
*** SetupApplyDialog_ConfirmDialog                                         ***
***                                                                        ***
*****************************************************************************/

SetupApplyDialog_ConfirmDialog::SetupApplyDialog_ConfirmDialog()
:m_listener(NULL)
{

}

OP_STATUS SetupApplyDialog_ConfirmDialog::Init(SetupApplyListener* listener, DesktopWindow *parent_window, char* servername, INT32* result, BOOL executed_by_gadget)
{
	OpString message;
	OpString title;
	// Do what if these fail?
	g_languageManager->GetString(Str::D_SECURITYALERT_SETUPDOWNLOAD_TITLE, title);		
		
#ifdef WIDGET_RUNTIME_SUPPORT
	if (executed_by_gadget)
	{
		g_languageManager->GetString(Str::D_SECURITYALERT_WIDGET_WARNING, message);  
	}
	else
	{	
		g_languageManager->GetString(Str::D_SECURITYALERT_SETUPDOWNLOAD, message);
	}
#else 
	g_languageManager->GetString(Str::D_SECURITYALERT_SETUPDOWNLOAD, message);
#endif // WIDGET_RUNTIME_SUPPORT

	OpString name;
	if (servername)
	{
		name.Set(servername);
		name.Append("\n\n");
		message.Insert(0, name);
	}

	m_listener = listener;
	return SimpleDialog::Init(WINDOW_NAME_SETUP_APPLY_DIALOG_CONFIRM, title, message, parent_window, TYPE_NO_YES, IMAGE_WARNING, FALSE, NULL ,NULL, NULL, 1);
}

UINT32 SetupApplyDialog_ConfirmDialog::OnOk()
{
	SimpleDialog::OnOk();
	if(m_listener)
	{
		m_listener->OnSetupAccepted();
	}
	return 0;
}

void SetupApplyDialog_ConfirmDialog::OnCancel()
{
	SimpleDialog::OnCancel();
	if(m_listener)
	{
		m_listener->OnSetupCancelled();
	}
}

/*****************************************************************************
***                                                                        ***
*** SetupApplyDialog::ConfigurationCollection                              ***
***                                                                        ***
*****************************************************************************/

SetupApplyDialog::ConfigurationCollection::ConfigurationCollection() : rollback_information()
{
	readme_file   = NULL;
	main_ini_file = NULL;
	installable_list.Clear();
}

SetupApplyDialog::ConfigurationCollection::~ConfigurationCollection()
{
	while (installable_list.GetCount() > 0)
	{
		ConfigurationCollection::InstallableFile* inst_file = installable_list.Remove(0);
		if (inst_file)
		{
			if (inst_file->mem_file)
			{
				OP_DELETE(inst_file->mem_file);
			}
			OP_DELETE(inst_file);
		}
	}
	if (main_ini_file)
	{
		if (main_ini_file->mem_file)
		{
			OP_DELETE(main_ini_file->mem_file);
		}
		OP_DELETE(main_ini_file);
	}
	if (readme_file)
	{
		if (readme_file->mem_file)
		{
			OP_DELETE(readme_file->mem_file);
		}
		OP_DELETE(readme_file);
	}
}
