/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef WIC_USE_DOWNLOAD_CALLBACK

# include "modules/windowcommander/src/TransferManagerDownload.h"
# include "modules/doc/frm_doc.h"
# include "modules/dochand/docman.h"
# include "modules/dochand/win.h"
# include "modules/dochand/winman.h"
# include "modules/viewers/viewers.h"
# include "modules/url/url_rep.h"
# include "modules/url/url2.h"
# include "modules/util/filefun.h"
# include "modules/windowcommander/src/TransferManager.h"
# include "modules/windowcommander/src/WindowCommander.h"
# ifdef SECURITY_INFORMATION_PARSER
#  include "modules/windowcommander/src/SecurityInfoParser.h"
# endif //
# ifdef TRUST_INFO_RESOLVER
#  include "modules/windowcommander/src/TrustInfoResolver.h"
# endif // TRUST_INFO_RESOLVER

TransferManagerDownloadCallback::TransferManagerDownloadCallback(
			DocumentManager *	document_manager,
			URL					download_url,
			Viewer * 			default_viewer,
			ViewActionFlag		current_action_flag,
			BOOL                was_downloaded)
	: done_listener(NULL),
	  document_manager(document_manager),
	  download_url(download_url),
	  current_action_flag(current_action_flag),
	  called(FALSE),
	  delayed(FALSE),
	  cancelled(FALSE),
	  is_download_to(FALSE),
	  download_to_started(FALSE),
	  m_privacy_mode(FALSE),
	  download_action_mode(DOWNLOAD_UNDECIDED),
	  download_context(NULL),
	  need_copy_when_downloaded(FALSE),
	  keep_loading(FALSE),
	  doc_man_can_close(TRUE),
	  m_window_commander(NULL),
	  dont_add_to_transfers(FALSE),
	  was_downloaded(was_downloaded)
{
	if (document_manager)
	{
		m_privacy_mode = document_manager->GetWindow()->GetPrivacyMode();
		document_manager->AddCurrentDownloadRequest(this);
	}
	DisableWindowClose();
	if (!default_viewer)
		OpStatus::Ignore(g_viewers->FindViewerByURL(download_url, default_viewer, FALSE));
	if (default_viewer)
		viewer.CopyFrom(default_viewer);
}

TransferManagerDownloadCallback::~TransferManagerDownloadCallback()
{
	if (!doc_man_can_close)
		EnableWindowClose();
	if (document_manager)
		document_manager->RemoveCurrentDownloadRequest(this);
	UnsetCallbacks();
}

OP_STATUS TransferManagerDownloadCallback::Init(const uni_char * file_path)
{
	OP_ASSERT(was_downloaded);
	return file_path_name.Set(file_path);
}

URLInformation::DoneListener * TransferManagerDownloadCallback::SetDoneListener(URLInformation::DoneListener * listener)
{
	DoneListener * old_listener = done_listener;
	done_listener = listener;
	return old_listener;
}

// OpDocumentListener::DownloadCallback methods
const uni_char* TransferManagerDownloadCallback::URLName()
{
	if (url_name.IsEmpty())
	{
		download_url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, url_name, TRUE);
	}
	return url_name.CStr();
}

const uni_char* TransferManagerDownloadCallback::SuggestedFilename()
{
	if (suggested_filename.IsEmpty())
	{
		TRAPD(err, download_url.GetAttributeL(URL::KSuggestedFileName_L, suggested_filename, TRUE)); // ignore err.
	}
	return suggested_filename.CStr();
}

const uni_char* TransferManagerDownloadCallback::SuggestedExtension()
{
	if (suggested_extension.IsEmpty())
	{
		TRAPD(err, download_url.GetAttributeL(URL::KSuggestedFileNameExtension_L, suggested_extension, TRUE));
	}
	return suggested_extension.CStr();
}

const uni_char* TransferManagerDownloadCallback::FilePathName()
{
	// DSK-332652 use file path from previous download
	if (was_downloaded && ! file_path_name.IsEmpty())
		return  file_path_name.CStr();
	TRAPD(err, download_url.GetAttributeL(URL::KFilePathName_L, file_path_name, TRUE));
	return file_path_name.CStr();
}

const char* TransferManagerDownloadCallback::MIMEType()
{
	if (mime_type_override.HasContent())
		return mime_type_override.CStr();
	else
		return download_url.GetAttribute(URL::KMIME_Type, TRUE);
}

URLType TransferManagerDownloadCallback::GetURLType()
{
	return (URLType) download_url.GetAttribute(URL::KType, TRUE);
}

URLContentType TransferManagerDownloadCallback::GetURLContentType()
{
	return (URLContentType) download_url.GetAttribute(URL::KContentType, TRUE);
}

URLInformation::GadgetContentType TransferManagerDownloadCallback::GetGadgetContentType()
{
	GadgetContentType type;
	switch (GetURLContentType())
	{
#ifdef OPERA_SETUP_DOWNLOAD_APPLY_SUPPORT
	case URL_MULTI_CONFIGURATION_CONTENT:
		type = URLINFO_MULTI_CONFIGURATION_CONTENT;
		break;
#endif
	case URL_GADGET_INSTALL_CONTENT:
		type = URLINFO_GADGET_INSTALL_CONTENT;
		break;
		// Please extend this switch and also the URLInformation::ContentType when in need
	default:
		type = URLINFO_OTHER_CONTENT;
		break;
	}
	return type;
}

const ServerName* TransferManagerDownloadCallback::GetServerName()
{
	return (ServerName*) download_url.GetAttribute(URL::KServerName, (void*)NULL, TRUE);
}

const uni_char * TransferManagerDownloadCallback::HostName()
{
	if (host_name.IsEmpty())
	{
		TRAPD(err, download_url.GetAttributeL(URL::KUniHostName, host_name, TRUE));
	}
	return host_name.CStr();
}

OP_STATUS TransferManagerDownloadCallback::GetHostName(OpString& hostname)
{
	TRAPD(err, download_url.GetAttributeL(URL::KUniHostName, hostname, TRUE));
	return err;
}

OpFileLength TransferManagerDownloadCallback::GetContentSize()
{
	OpFileLength size = 0;
	download_url.GetAttribute(URL::KContentSize, &size, TRUE);
	return size;
}

OpFileLength TransferManagerDownloadCallback::GetLoadedSize()
{
	OpFileLength size = 0;
	download_url.GetAttribute(URL::KContentLoaded_Update, &size, TRUE);
	return size;
}

BOOL TransferManagerDownloadCallback::IsLoading()
{
	URLStatus status = download_url.Status(TRUE);

	return status == URL_LOADING;

}

BOOL TransferManagerDownloadCallback::IsLoaded()
{
	URLStatus status = download_url.Status(TRUE);

	return status == URL_LOADED;
}

BOOL TransferManagerDownloadCallback::GetUsedContentDispositionAttachment()
{
	return (BOOL) download_url.GetAttribute(URL::KUsedContentDispositionAttachment);
}

OP_STATUS TransferManagerDownloadCallback::SetMIMETypeOverride(const char *mime_type)
{
	return mime_type_override.Set(mime_type);
}

void TransferManagerDownloadCallback::ForceMIMEType(URLContentType type)
{
	TRAPD(status, download_url.SetAttributeL(URL::KForceContentType, type));
}

long TransferManagerDownloadCallback::Size()
{
	return (long) MIN(LONG_MAX, download_url.GetContentSize());
}

OpDocumentListener::DownloadRequestMode TransferManagerDownloadCallback::Mode()
{
	if (current_action_flag.IsSet(ViewActionFlag::SAVE_AS))
		return OpDocumentListener::SAVE;
	else if (viewer.GetFlags().IsSet(ViewActionFlag::SAVE_DIRECTLY))
		return OpDocumentListener::SAVE;
	else if (viewer.GetAction() == VIEWER_SAVE)
		return OpDocumentListener::SAVE;
	else
		return OpDocumentListener::ASK_USER;
}

const uni_char* TransferManagerDownloadCallback::SuggestedSavePath()
{
	if (viewer.GetFlags().IsSet(ViewActionFlag::SAVE_DIRECTLY))
		return viewer.GetSaveToFolder();
	else
		return NULL;
}

URL_ID TransferManagerDownloadCallback::GetURL_Id()
{
	URL_ID id;
	download_url.GetAttribute(URL::K_ID, &id, TRUE);
	return id;
}

void TransferManagerDownloadCallback::SetDownloadContext(DownloadContext * dc)
{
	download_context = dc;
}

DownloadContext * TransferManagerDownloadCallback::GetDownloadContext()
{
	return download_context;
}

void TransferManagerDownloadCallback::EnableWindowClose()
{
	if (document_manager && document_manager->GetWindow())
	{
		document_manager->GetWindow()->SetCanClose(TRUE);
		doc_man_can_close = TRUE;
	}
}

void TransferManagerDownloadCallback::DisableWindowClose()
{
	if (document_manager && document_manager->GetWindow())
	{
		document_manager->GetWindow()->SetCanClose(FALSE);
		doc_man_can_close = FALSE;
	}
}

TransferManagerDownloadAPI * TransferManagerDownloadCallback::GetDownloadAPI()
{
	return this;
}

OP_STATUS TransferManagerDownloadCallback::LoadToFile(uni_char * filename)
{
	download_filename.Set(filename);
	return download_url.LoadToFile(filename);
}

OP_STATUS TransferManagerDownloadCallback::DoLoadedCheck()
{
	if (document_manager && download_url.Status(TRUE) != URL_LOADING)
		document_manager->HandleAllLoaded(download_url.Id());
	return OpStatus::OK;
}

// TransferManagerDownloadAPI method
const OpStringC8 TransferManagerDownloadCallback::URLFullName()
{
	return download_url.GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, TRUE);
}

const uni_char * TransferManagerDownloadCallback::URLUniFullName()
{
	return download_url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI, TRUE);
}

OpWindow * TransferManagerDownloadCallback::GetOpWindow()
{
	if (document_manager &&
		document_manager->GetWindow())
	{
		OpWindow * ow = (OpWindow*) document_manager->GetWindow()->GetOpWindow();
		return ow;
	}
	else
		return NULL;
}

void TransferManagerDownloadCallback::SetWaitForUser()
{
	if (document_manager)
		document_manager->SetLoadStatus(WAIT_FOR_USER);
}

void TransferManagerDownloadCallback::SetViewer(Viewer& aviewer)
{
	viewer.CopyFrom(&aviewer);
}

void TransferManagerDownloadCallback::SetViewerAction(ViewAction action)
{
	if (document_manager)
		document_manager->SetAction(action);
}

void TransferManagerDownloadCallback::SetViewerApplication(const uni_char *application)
{
	if (document_manager)
		document_manager->SetApplication(application);
}

BOOL TransferManagerDownloadCallback::IsSaveDirect()
{
	return !current_action_flag.IsSet(ViewActionFlag::SAVE_AS)
			&& viewer.GetFlags().IsSet(ViewActionFlag::SAVE_DIRECTLY);
}

OP_STATUS TransferManagerDownloadCallback::SetSaveDirectFileName(OpString& filename, OpString& direct_filename, BOOL& not_success)
{
	not_success = TRUE;

	const uni_char* save_to_folder = viewer.GetSaveToFolder();

	if (save_to_folder)
	{
		if (OpStatus::OK == (direct_filename.Set(save_to_folder)))
		{
			if (	direct_filename.Length() == 0
				|| (	direct_filename.Length() > 0
					&&	direct_filename[direct_filename.Length()-1] == PATHSEPCHAR)
				|| (	direct_filename[direct_filename.Length()-1] != PATHSEPCHAR
					 &&	OpStatus::OK == direct_filename.Append(PATHSEP)))
			{
				if (OpStatus::OK == direct_filename.Append(filename))
				{
					if (OpStatus::OK == CreateUniqueFilename(direct_filename))
					{
						if (direct_filename.HasContent())
							not_success = FALSE;
					}
					else
					{
						return OpStatus::ERR;
					}
				}
				else
				{
					return OpStatus::ERR;
				}
			}
			else
			{
				return OpStatus::ERR;
			}
		}
		else
		{
			return OpStatus::ERR;
		}
	}
	return OpStatus::OK;
}

void TransferManagerDownloadCallback::Stop()
{
	if (document_manager)
		document_manager->StopLoading(FALSE);
}

// OpDocumentListener::DownloadCallback methods that will conclude the
// interaction and ultimately free/delete the resources
BOOL TransferManagerDownloadCallback::Save(uni_char * filename)
{
	DoLoadedCheck();

	if (filename && *filename)
	{
		// If this resource has opera as viewer, then we will do
		// a copy when fully downloaded. If not, then we call LoadToFile..
		ViewActionReply reply;
		OpString mimetype;
		mimetype.Set(MIMEType());
		BOOL load_to_file = TRUE;
		if (OpStatus::IsSuccess(Viewers::GetViewAction(download_url, mimetype, reply, FALSE, FALSE)))
		{
			if (document_manager && document_manager->GetWindow() && document_manager->GetWindow()->GetType() == WIN_TYPE_IM_VIEW)
				keep_loading = TRUE;

			if (reply.action == VIEWER_OPERA)
			{
				download_filename.Set(filename);
				URLStatus download_url_status = (URLStatus) download_url.GetAttribute(URL::KLoadStatus, TRUE);
				if (download_url_status == URL_LOADED)
				{
					download_url.SaveAsFile(filename);
					dont_add_to_transfers = TRUE;
					load_to_file = FALSE;
				}
				else if(IsUrlInlined())
				{
					// Set up for later copy to file (when downloaded)
					need_copy_when_downloaded = TRUE;
					keep_loading = TRUE;
					load_to_file = FALSE;
				}
			}
			else
			{
				if (download_url.GetAttribute(URL::KMultimedia) && download_url.IsExportAllowed())
				{
					download_filename.Set(filename);
					load_to_file = OpStatus::IsError(download_url.ExportAsFile(filename));
					if (load_to_file)
					{
						download_url.SetAttribute(URL::KUnique, TRUE);
					}
				}
			}
		}

		if (load_to_file)
		{
			LoadToFile(filename);
		}
	}

	called = TRUE;
	download_action_mode = DOWNLOAD_SAVE;

	if (!delayed) // Someone will call Execute and then we will go on
		return FALSE;

	Execute();

	return TRUE;
}

BOOL TransferManagerDownloadCallback::PassURL()
{
	g_op_system_info->ExecuteApplication(viewer.GetApplicationToOpenWith(),
		download_url.GetAttribute(URL::KUniName_Username_Password_Escaped_NOT_FOR_UI)); // PASSWORD_SHOW_VERBATIM accepted provisionally

	return Abort();
}

BOOL TransferManagerDownloadCallback::Run(uni_char * filename)
{
	if (filename && *filename)
		LoadToFile(filename);
	called = TRUE;
	download_action_mode = DOWNLOAD_RUN;

	if (!delayed) // Someone will call Execute and then we will go on
		return FALSE;

	Execute();

	return TRUE;
}

BOOL TransferManagerDownloadCallback::ReturnToSender(BOOL usePlugin)
{
	called = TRUE;
	download_action_mode = DOWNLOAD_UNDECIDED;
	keep_loading = TRUE;

	if (document_manager)
	{
		BOOL out_of_memory;
		document_manager->SetAction(VIEWER_OPERA);
		document_manager->HandleByOpera(TRUE, usePlugin, FALSE, &out_of_memory);
		if (out_of_memory)
			return FALSE;
	}

	if (!delayed)
		return FALSE;

	Execute();

	return TRUE;
}

BOOL TransferManagerDownloadCallback::Abort()
{
	called = TRUE;
	download_action_mode = DOWNLOAD_ABORT;

	keep_loading = IsUrlInlined() && download_url.Status(TRUE) == URL_LOADING;

	if (!delayed) // Someone will call Execute and then we will go on
		return FALSE;

	Execute();

	return TRUE;
}

// Methods used by instantiator/creator and/or self
OP_BOOLEAN TransferManagerDownloadCallback::Execute()
{
	OP_STATUS status;
	BOOL done = FALSE;

	if (!cancelled)
	{
		if (called)
		{
			switch (download_action_mode)
			{
			case DOWNLOAD_UNDECIDED:
				// TODO: This should not be possible, need to test......
				// Use this to throw things back at document manager
				done = TRUE;
				break;
			case DOWNLOAD_ABORT:
				if (keep_loading)
				{
					if (document_manager)
						document_manager->SetLoadStatus(WAIT_FOR_LOADING_FINISHED);
				}
				else
				{
					if (document_manager)
						document_manager->StopLoading(FALSE);
					if (!download_url.GetRep()->GetUsedCount()) // These two lines belong in the url module
						download_url.Unload();					// rfz 20071028
				}
				EnableWindowClose();
				done = TRUE;
				break;
			case DOWNLOAD_SAVE:
			case DOWNLOAD_RUN:
				if (!dont_add_to_transfers)
				{
					BOOL show_transfer = TRUE;
					if (download_context)
						show_transfer = download_context->IsShownDownload();

					TransferItem* newitem = 0;
					// Put the download object into TransferManager
					status = ((TransferManager*)g_transferManager)->AddTransferItem(download_url,
							download_filename.CStr(),
							OpTransferItem::ACTION_UNKNOWN,
							FALSE,
							0,
							TransferItem::TRANSFERTYPE_DOWNLOAD,
							NULL,
							NULL,
							FALSE,
							show_transfer,
							m_privacy_mode,
							&newitem);
					if (status == OpStatus::OK)
					{
						if (download_context && download_context->GetTransferListener())
							newitem->SetTransferListener(download_context->GetTransferListener());

						if (download_action_mode == DOWNLOAD_SAVE)
							newitem->SetAction(OpTransferItem::ACTION_SAVE);
						else if (download_action_mode == DOWNLOAD_RUN)
							newitem->SetAction(OpTransferItem::ACTION_RUN_WHEN_FINISHED);

						newitem->SetViewer(viewer);

						if (need_copy_when_downloaded)
							newitem->SetCopyWhenDownloaded(download_filename);

						MessageHandler *oldHandler = download_url.GetFirstMessageHandler();

						// Check if this URL is already being downloaded to some file
						// (the same file or another file - who knows?). The Opera core
						// doesn't handle concurrent downloading of the same URL properly.
						// So just give up.
						//
						// The result will be that the transfer already in progress completes
						// without problems, while the new transfer that was attempted added,
						// will hang (the progress bar in the window that triggered the transfer
						// will stay there until the user aborts it somehow).
						MessageHandler *currentHandler = oldHandler;
						BOOL transfer_in_progress = FALSE;
						while (currentHandler && !transfer_in_progress)
						{
							if (currentHandler == g_main_message_handler)
								transfer_in_progress = TRUE;
							else
								currentHandler = static_cast<MessageHandler *>(currentHandler->Suc());
						}

						if (!transfer_in_progress)
						{
							FramesDocument *doc = document_manager ? document_manager->GetCurrentDoc() : NULL;

							URLStatus ust = download_url.Status(FALSE);
							switch (ust)
							{
							case URL_UNLOADED:
								download_url.Load(g_main_message_handler, doc ? doc->GetURL() : URL());
								currentHandler = g_main_message_handler;
								break;

							case URL_LOADING_ABORTED:
								download_url.ResumeLoad(g_main_message_handler, doc ? doc->GetURL() : URL());
								currentHandler = g_main_message_handler;
								break;

							default:

								if ((ust == URL_LOADED || ust == URL_LOADING_FAILURE) && is_download_to && !download_to_started)
								{
									// Fix for DSK-232055 - the best we can do is to reload
									URL ref = download_url.GetAttribute(URL::KReferrerURL);
									download_url.SetAttribute(URL::KReloadSameTarget, TRUE);
									download_url.Reload(g_main_message_handler, ref, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE);
									currentHandler = g_main_message_handler;
								}
								else
								{
									if (oldHandler && !keep_loading)
									{
										download_url.ChangeMessageHandler(oldHandler, g_main_message_handler);
										currentHandler = g_main_message_handler;
									}
									else
										currentHandler = oldHandler;	
								}
							}
						}
						// currentHandler is NULL when download_url.Status(FALSE) equals URL_LOADING_FAILED, or shall we set it to g_main_message_handler anyway?
						if (currentHandler)
							newitem->SetCallbacks(currentHandler, download_url.Id(TRUE));
					}
				}
				if (download_action_mode == DOWNLOAD_RUN)
				{
					if (	document_manager &&
							download_url.Status(FALSE) != URL_UNLOADED &&
							download_url.Status(TRUE) != URL_LOADING)
					{
						// The loading has already finished. We have to trigger the action
						// manually by using HandleAllLoaded() [espen]

						// InitiateTransfer() above destroys filename
						//				doc_man->SetCurrentURL(saved_url);

						if( viewer.GetAction() == VIEWER_APPLICATION || viewer.GetAction() == VIEWER_PASS_URL)
						{
							document_manager->SetAction(VIEWER_APPLICATION);
							document_manager->SetApplication(viewer.GetApplicationToOpenWith());
						}

						else if(viewer.GetAction() == VIEWER_REG_APPLICATION)
						{
							document_manager->SetAction(VIEWER_REG_APPLICATION);
						}
						// Restore
						//				doc_man->SetCurrentURL(GetUrl());
					}
				}
				if (dont_add_to_transfers)
				{
					if (download_context && download_context->GetTransferListener())
						download_context->GetTransferListener()->OnSavedFromCacheDone(download_filename);
				}
				done = TRUE;
				break;
			}

			// Here we clean up in the context of document manager
			if (document_manager && !keep_loading)
				document_manager->StopLoading(FALSE);	//the window is not handling the loading anymore

			if (document_manager && download_url.Status(TRUE) != URL_LOADING)
				document_manager->HandleAllLoaded(download_url.Id(TRUE));

			EnableWindowClose();
		}
		else
		{
#ifdef WEB_TURBO_MODE
			// Downloads should not use Turbo. NB! This WILL generate a second request for the same resource (this time without using the Turbo proxy)
			if (download_url.GetAttribute(URL::KUsesTurbo) &&
				!download_url.GetAttribute(URL::KTurboBypassed) &&
#ifdef _BITTORRENT_SUPPORT_
				download_url.ContentType() != URL_P2P_BITTORRENT &&
#endif // _BITTORRENT_SUPPORT_
				document_manager)
			{
				URLStatus ustat = (URLStatus)download_url.GetAttribute(URL::KLoadStatus, URL::KFollowRedirect);
				if (ustat == URL_LOADING)
					document_manager->StopLoading(FALSE);

				const OpStringC8 url_str = download_url.GetAttribute(URL::KName_With_Fragment_Username_Password_NOT_FOR_UI, URL::KNoRedirect);
				download_url = g_url_api->GetURL(url_str.CStr());
				document_manager->SetCurrentURL(download_url, TRUE);
			}
#endif // WEB_TURBO_MODE
			delayed = TRUE;
		}
	}
	if (done)
	{
		Window* win = document_manager ? document_manager->GetWindow():NULL;
		TryDelete();

		//if the window is Not intiated by the user and this is the first url the window loads, close it
		//since the data is handled elsewhere.
		if (win && win->GetOpenerWindow() && win->GetHistoryLen() == 0 && win->CanClose())
			win->Close();

		return OpBoolean::IS_TRUE; // Signifying that we have done a transition through a download initiation
	}
	else
	{
		return OpBoolean::IS_FALSE; // Signifying that we will back here before we are done
	}
}

void TransferManagerDownloadCallback::Cancel()
{
	cancelled = TRUE;
	ReleaseDocument();
}

OP_STATUS TransferManagerDownloadCallback::DownloadDefault(DownloadContext * context, const uni_char * downloadpath)
{
	SetDownloadContext(context);

	URL empty;
	OP_STATUS sts = OpStatus::OK;
	WindowCommander * windowcommander = NULL;
	if (document_manager && document_manager->GetWindow())
		windowcommander = document_manager->GetWindow()->GetWindowCommander();
	if (!windowcommander && context)
		windowcommander = static_cast<WindowCommander*>(context->GetWindowCommander());

	viewer.SetAction(VIEWER_SAVE);
	viewer.SetFlags(ViewActionFlag::SAVE_DIRECTLY);
	OpString tmp_storage;
	const uni_char * opfolder_download = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_DOWNLOAD_FOLDER, tmp_storage);
	viewer.SetSaveToFolder(downloadpath ? downloadpath : opfolder_download);

	if (download_url.Status(FALSE) != URL_UNLOADED || !WaitForHeader(windowcommander))
	{
		windowcommander->GetDocumentListener()->OnDownloadRequest(windowcommander, this);
		Execute();
	}

	return sts;
}

OP_STATUS TransferManagerDownloadCallback::DownloadTo(DownloadContext * context, const uni_char * downloadpath)
{
	SetDownloadContext(context);
	is_download_to = TRUE;

	URL empty;
	OP_STATUS sts = OpStatus::OK;
	WindowCommander * windowcommander = NULL;
	if (document_manager && document_manager->GetWindow())
		windowcommander = document_manager->GetWindow()->GetWindowCommander();
	if (!windowcommander && context)
		windowcommander = static_cast<WindowCommander*>(context->GetWindowCommander());

	viewer.SetAction(VIEWER_SAVE);
	viewer.SetFlags(ViewActionFlag::SAVE_AS);
	OpString tmp_storage;
	const uni_char * opfolder_save = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_SAVE_FOLDER, tmp_storage);
	viewer.SetSaveToFolder(downloadpath ? downloadpath : opfolder_save);

	if (download_url.Status(FALSE) != URL_UNLOADED || !WaitForHeader(windowcommander))
	{
		windowcommander->GetDocumentListener()->OnDownloadRequest(windowcommander, this);
		Execute();
	}
	return sts;
}

void TransferManagerDownloadCallback::ReleaseDocument()
{
	EnableWindowClose();
	if (document_manager)
	{
		document_manager->RemoveCurrentDownloadRequest(this);
		document_manager = NULL;
		m_window_commander = NULL;
	}
}

void TransferManagerDownloadCallback::URL_Information_Done()
{
	TryDelete();
}

void TransferManagerDownloadCallback::TryDelete()
{
	if (done_listener && done_listener->OnDone(this) == FALSE)
	{
		/* This object is about to be reused for a potential
		 * ui interaction, we need to make sure that we return
		 * to a sane state.
		 */
		ReleaseDocument();
		download_action_mode = DOWNLOAD_UNDECIDED;
		download_context = NULL;
		need_copy_when_downloaded = FALSE;
		keep_loading = FALSE;
		called = FALSE;
		delayed = FALSE;
		cancelled = FALSE;
		current_action_flag.Reset();
		//viewer.Reset(); ??
		dont_add_to_transfers = FALSE;
	}
	else
		OP_DELETE(this);
}

BOOL TransferManagerDownloadCallback::IsUrlInlined()
{
	if (document_manager)
	{
		DocumentTreeIterator doc_tree(document_manager);

		doc_tree.SetIncludeThis();

		while(doc_tree.Next())
		{
			if(doc_tree.GetFramesDocument())
			{
				if (doc_tree.GetFramesDocument()->IsInline(download_url.Id(), TRUE))
					return TRUE;
			}
		}
	}
	return FALSE;
}

BOOL TransferManagerDownloadCallback::WaitForHeader(WindowCommander * window_commander)
{
	// Initiate Download, make sure this is a listener for events, also ensure that
	// we are not discarded yet..
	m_window_commander = window_commander;
	SetCallbacks(download_url.Id());
	FramesDocument * doc = document_manager ? document_manager->GetCurrentDoc() : NULL;
	download_url.Load(g_main_message_handler, doc ? doc->GetURL() : URL());
	// reset cached values (strings from url)
	url_name.Empty();
	suggested_filename.Empty();
	suggested_extension.Empty();
	download_filename.Empty();
	file_path_name.Empty();
	host_name.Empty();
	download_to_started = TRUE;

	return TRUE;
}

/*static*/
const OpMessage TransferManagerDownloadCallback::download_callback_messages[] =
		{
			MSG_URL_LOADING_FAILED,
			MSG_HEADER_LOADED,
			MSG_URL_MOVED
		};

void TransferManagerDownloadCallback::SetCallbacks(MH_PARAM_1 id)
{
	g_main_message_handler->SetCallBackList(this, id, download_callback_messages, ARRAY_SIZE(download_callback_messages));
}

void TransferManagerDownloadCallback::UnsetCallbacks()
{
	g_main_message_handler->UnsetCallBacks(this);
}

void TransferManagerDownloadCallback::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
	case MSG_URL_LOADING_FAILED:
		// TODO: Maybe a message to the user?
		UnsetCallbacks();
		Abort();
		Execute();
		break;
	case MSG_HEADER_LOADED:
	{
		UnsetCallbacks();
		URL moved_url = download_url.GetAttribute(URL::KMovedToURL, TRUE);
		if (!moved_url.IsEmpty())
		{
			if (!(moved_url == download_url))
			{
				if (download_url.UniRelName())
					download_url = URL(moved_url, download_url.UniRelName());
				else
				{
					download_url = moved_url;
				}
			}
		}
		if (m_window_commander)
			m_window_commander->GetDocumentListener()->OnDownloadRequest(m_window_commander, this);
		Execute();
		break;
	}
	case MSG_URL_MOVED:
	{
		URL moved_url = download_url.GetAttribute(URL::KMovedToURL, TRUE);

		if (!moved_url.IsEmpty())
		{
			// There was a redirect - stop listening to the old URL and wait for the new URL:
			UnsetCallbacks();
			download_url = moved_url;
			WaitForHeader(m_window_commander);
		}

		break;
	}
	default:
		{
			// TODO: analyse why getting here....
		}
		break;
	}
}

OP_STATUS TransferManagerDownloadCallback::GetApplicationToOpenWith(OpString& application, BOOL& pass_url)
{
	pass_url = viewer.GetAction() == VIEWER_PASS_URL;
	if (	viewer.GetAction() == VIEWER_PASS_URL
		||	viewer.GetAction() == VIEWER_APPLICATION)
	{
		return application.Set(viewer.GetApplicationToOpenWith());
	}
	return OpStatus::ERR;
}

#ifdef SECURITY_INFORMATION_PARSER
OP_STATUS TransferManagerDownloadCallback::CreateSecurityInformationParser(OpSecurityInformationParser** parser)
{
	SecurityInformationParser *sip = OP_NEW(SecurityInformationParser, ());
	if (!sip)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS sts = sip->SetSecInfo(download_url.GetAttribute(URL::KSecurityInformationURL));
	if (OpStatus::IsSuccess(sts))
		sts = sip->Parse();
	if (OpStatus::IsSuccess(sts))
		*parser = sip;
	else
		OP_DELETE(sip);
	return sts;
}

int TransferManagerDownloadCallback::GetSecurityMode()
{
	switch (download_url.GetAttribute(URL::KSecurityStatus))
	{
	case SECURITY_STATE_FULL:
		return OpDocumentListener::HIGH_SECURITY;
# ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	case SECURITY_STATE_FULL_EXTENDED:
		return OpDocumentListener::EXTENDED_SECURITY;
#  ifdef SECURITY_STATE_SOME_EXTENDED
	case SECURITY_STATE_SOME_EXTENDED:
		return OpDocumentListener::SOME_EXTENDED_SECURITY;
#  endif // SECURITY_STATE_SOME_EXTENDED
# endif // SSL_CHECK_EXT_VALIDATION_POLICY
	case SECURITY_STATE_HALF:
		return OpDocumentListener::MEDIUM_SECURITY;
	case SECURITY_STATE_LOW:
		return OpDocumentListener::LOW_SECURITY;
	case SECURITY_STATE_NONE:
		return OpDocumentListener::NO_SECURITY;
	}
	return OpDocumentListener::UNKNOWN_SECURITY;
}

int TransferManagerDownloadCallback::GetTrustMode()
{
	return Not_Set;
}

UINT32 TransferManagerDownloadCallback::SecurityLowStatusReason()
{
	return download_url.GetAttribute(URL::KSecurityLowStatusReason);
}

const uni_char*	TransferManagerDownloadCallback::ServerUniName() const
{
	if (download_url.GetServerName())
		return download_url.GetServerName()->UniName();
	else
		return UNI_L("");
}
#endif // SECURITY_INFORMATION_PARSER

#ifdef TRUST_INFO_RESOLVER
OP_STATUS TransferManagerDownloadCallback::CreateTrustInformationResolver(OpTrustInformationResolver** resolver, OpTrustInfoResolverListener * listener)
{
	TrustInformationResolver *tir = OP_NEW(TrustInformationResolver, (listener));
	*resolver = tir;
	if (tir)
	{
		tir->SetServerURL(download_url);
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}
#endif // TRUST_INFO_RESOLVER

#endif // WIC_USE_DOWNLOAD_CALLBACK
