/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Alexander Remen (alexr@opera.com)
 *
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/m2_ui/util/AttachmentOpener.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/controller/SimpleDialogController.h"
#include "adjunct/quick/dialogs/DownloadDialog.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/url/url2.h"
#include "modules/viewers/viewers.h"
#include "modules/windowcommander/src/TransferManagerDownload.h"


class ExecuteAttachmentDialog : public SimpleDialogController
{
public:
	ExecuteAttachmentDialog()
	: SimpleDialogController(TYPE_OK_CANCEL, IMAGE_WARNING, WINDOW_NAME_EXECUTE_ATTACHMENT, Str::S_WARNING_RUN_ATTACHMENT,Str::SI_IDSTR_MSG_MAILFREPFS_INVALID_SETTINGS_CAP)
	{
	}

	OP_STATUS Init(const uni_char * a_handler, const uni_char * a_attachment)
	{
		RETURN_IF_ERROR(m_handler.Set(a_handler));
		return m_attachment.Set(a_attachment);
	}

	virtual void OnOk()
	{
		g_desktop_op_system_info->OpenFileInApplication(m_handler.CStr(), m_attachment.CStr());
		SimpleDialogController::OnOk();
	}
private:
	OpString m_handler;
	OpString m_attachment;
};

BOOL AttachmentOpener::OpenAttachment(URL* url, DesktopFileChooser* chooser, DesktopWindow* parent_window)
{
	if (!url)
		return TRUE;

	// [Security Issue] rfz 2006-4-20:
	// If Opera is handling the resource, it would be a security issue
	// to move the attachment from cache to the temporary download folder.

	//Get mimetype so that the viewer can be found:
	const char* mimetype = url->GetAttribute(URL::KMIME_Type).CStr();
	OpString mime_type;
	mime_type.Set(mimetype);

	// The handler that will open the file
	OpString handler;

	// Consult viewer on how the mimetype should be handled:
	Viewer *viewer = g_viewers->FindViewerByMimeType(mime_type);

	if(viewer)
	{
		ViewAction view_action =  viewer->GetAction();

		if(view_action == VIEWER_OPERA || view_action == VIEWER_PLUGIN)
		{
			OpString url_name;
			if (OpStatus::IsSuccess(url->GetAttribute(URL::KUniName_Username_Password_Hidden, url_name)) && url_name.HasContent())
			{
				g_application->GoToPage(url_name.CStr(), TRUE, FALSE, FALSE, 0, url->GetContextId());
			}
			return TRUE;
		}
		else if(view_action == VIEWER_APPLICATION)
		{
			// Use the user configured handler
			RETURN_VALUE_IF_ERROR(handler.Set(viewer->GetApplicationToOpenWith()), TRUE);
		}
	}

	// If opera is not handling the resource:
	// First make sure that we open from temporary download folder
	// not from cache.
	OpString openfrom_filename;
	OpString tmp_storage;
	openfrom_filename.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_TEMPDOWNLOAD_FOLDER, tmp_storage));
	if( openfrom_filename.Length() > 0 && openfrom_filename[openfrom_filename.Length()-1] != PATHSEPCHAR )
		openfrom_filename.Append(PATHSEP);
	OpString fname;
	TRAPD(err, url->GetAttributeL(URL::KSuggestedFileName_L, fname, TRUE));
	openfrom_filename.Append(fname);
	url->LoadToFile(openfrom_filename.CStr());

	// If the user did not configure a handler then use the default handler from the system
	if(handler.IsEmpty())
		g_op_system_info->GetFileHandler(&openfrom_filename, mime_type, handler);

	if(handler.HasContent())
	{
		if(g_op_system_info->GetIsExecutable(&openfrom_filename))
		{
			ExecuteAttachmentDialog * ead = OP_NEW(ExecuteAttachmentDialog, ());
			RETURN_VALUE_IF_NULL(ead, TRUE);
			RETURN_VALUE_IF_ERROR(ead->Init(handler.CStr(), openfrom_filename.CStr()), TRUE);
			RETURN_VALUE_IF_ERROR(ShowDialog(ead, g_global_ui_context, parent_window), TRUE);
		}
		else
		{
			g_desktop_op_system_info->OpenFileInApplication(handler.CStr(), openfrom_filename.CStr());
		}
	}
	else
	{
		ViewActionFlag view_action_flag;
		TransferManagerDownloadCallback * download_callback = OP_NEW(TransferManagerDownloadCallback, (NULL, *url, NULL, view_action_flag));

		if (!chooser)
			RETURN_VALUE_IF_ERROR(DesktopFileChooser::Create(&chooser), TRUE);

		DownloadItem * di = OP_NEW(DownloadItem, (download_callback, chooser, TRUE));
		if (di)
		{
			DownloadDialog* dialog = OP_NEW(DownloadDialog, (di));
			if (dialog)
				dialog->Init(parent_window);
			else
				OP_DELETE(di);
		}
	}
	return TRUE;
}

#endif // M2_SUPPORT
