/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 */

#include "core/pch.h"

#include "adjunct/desktop_util/crash/logsender.h"

#include "adjunct/desktop_util/adt/optightvector.h"
#include "adjunct/desktop_util/file_utils/FileUtils.h"
#include "adjunct/desktop_util/file_utils/folder_recurse.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/upload/upload.h"
#include "modules/url/url_man.h"
#include "modules/util/zipload.h"

LogSender::~LogSender()
{
	Reset();
}

OP_STATUS LogSender::Send()
{
	URL upload_url;
	TRAPD(status, upload_url = SetupUploadL());

	if (!upload_url.IsValid())
		return OpStatus::ERR;

	m_uploading_url.SetURL(upload_url);

	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_URL_DATA_LOADED, m_uploading_url->Id()));
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_URL_LOADING_FAILED, m_uploading_url->Id()));
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_URL_MOVED, m_uploading_url->Id()));

	// post request
	if (m_listener)
		m_listener->OnSendingStarted(this);

	upload_url.Reload(g_main_message_handler, URL(), TRUE, FALSE);

	return OpStatus::OK;
}

URL LogSender::SetupUploadL()
{
	// form content
	OpStackAutoPtr<Upload_Multipart> form (OP_NEW_L(Upload_Multipart, ()));
	form->InitL("multipart/form-data");

	OpStackAutoPtr<Upload_URL> log_file (OP_NEW_L(Upload_URL, ()));

	LEAVE_IF_ERROR(ZipLogFile());

	log_file->InitL(m_zip_path.HasContent() ? m_zip_path : m_dmp_path, NULL, "form-data", "file", NULL, ENCODING_NONE);
	form->AddElement(log_file.release());

	const UINT32 paras_num = 5;
	ANCHORD(OpAutoTightVector<Upload_OpString8*>, paras);
	LEAVE_IF_ERROR(paras.Resize(paras_num));

	for(UINT32 i=0; i<paras_num; i++)
	{
		paras[i] = OP_NEW_L(Upload_OpString8,());
	}

	{
		SetFormKeyValueL(paras[0], "email", m_email);
		SetFormKeyValueL(paras[1], "url", m_url);
		SetFormKeyValueL(paras[2], "userComments", m_comments);

		// if this value doesn't exist, the crash happened very early during startup or very late during shutdown
		int current_up = g_pcui->GetIntegerPref(PrefsCollectionUI::StartupTimestamp);

		if (current_up)
			current_up = m_crash_time - current_up;

		int total_up = g_pcui->GetIntegerPref(PrefsCollectionUI::TotalUptime) + current_up;

		ANCHORD(OpString8, total_uptime);
		LEAVE_IF_ERROR(total_uptime.AppendFormat("%d", total_up));
		SetFormKeyValueL(paras[3], "TotalUptime", total_uptime);

		ANCHORD(OpString8, crash_uptime);
		LEAVE_IF_ERROR(crash_uptime.AppendFormat("%d", current_up));
		SetFormKeyValueL(paras[4], "CrashUptime", crash_uptime);

		g_pcui->ResetIntegerL(PrefsCollectionUI::TotalUptime);
		g_prefsManager->CommitL();
	}

	for (UINT32 i = paras_num - 1; (int)i >= 0; i--)
	{
		form->AddElement(paras.Release(i));
	}

	form->PrepareUploadL(UPLOAD_BINARY_NO_CONVERSION);

	URL form_url = urlManager->GetURL("http://crash.opera.com/", "", TRUE);
	form_url.SetHTTP_Method(HTTP_METHOD_POST);
	form_url.SetHTTP_Data(form.release(), TRUE);

	return form_url;
}

OP_STATUS LogSender::ZipLogFile()
{
	if (m_dmp_path.IsEmpty() || m_zip_path.HasContent())
		return OpStatus::ERR_FILE_NOT_FOUND;

	OpString zip_path;
	RETURN_IF_ERROR(zip_path.Set(m_dmp_path));
	RETURN_IF_ERROR(zip_path.Append(".zip"));

	OpZip zip_file;
	RETURN_IF_ERROR(zip_file.Init(&zip_path));

	BOOL is_directory;
	RETURN_IF_ERROR(FileUtils::IsDirectory(m_dmp_path, is_directory));

	// If it is a directory. Zip up all the files, and send them away.
	if (is_directory)
	{
		FolderRecursor recursor(0);
		RETURN_IF_ERROR(recursor.SetRootFolder(m_dmp_path));

		OpFile* file;

		do
		{
			RETURN_IF_ERROR(recursor.GetNextFile(file));

			if (file)
			{
				OP_STATUS status = zip_file.AddFileToArchive(file);
				OP_DELETE(file);
				RETURN_IF_ERROR(status);
			}

		} while (file);

		zip_file.Close();
	}
	else
	{
		// If it is only one file, then zip it on its own.
		OpFile log_file;
		RETURN_IF_ERROR(log_file.Construct(m_dmp_path));
		RETURN_IF_ERROR(zip_file.AddFileToArchive(&log_file));
		zip_file.Close();
	}

	return m_zip_path.TakeOver(zip_path);
}

void LogSender::SetFormKeyValueL(Upload_OpString8* element, OpStringC8 key, OpStringC8 value)
{
	element->InitL(value, NULL, NULL);
	element->SetContentDispositionL("form-data");
	element->SetContentDispositionParameterL("name", key, TRUE);
}

void LogSender::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
		case MSG_URL_MOVED:
			if (m_wants_crashpage)
			{
				const OpStringC url = m_uploading_url->GetAttribute(URL::KMovedToURL).GetAttribute(URL::KUniName_Escaped);
				TRAPD(err, g_pcui->WriteStringL(PrefsCollectionUI::CrashFeedbackPage, url));
				OpStatus::Ignore(err);
			}
			// fall through
		case MSG_URL_DATA_LOADED:
			m_uploading_url->StopLoading(NULL);
			Reset();
			if (m_listener)
				m_listener->OnSendSucceeded(this);
			break;
		case MSG_URL_LOADING_FAILED:
			m_uploading_url->StopLoading(NULL);
			ResetUpload();
			if (m_listener)
				m_listener->OnSendFailed(this);
			break;
	}
}

void LogSender::Reset()
{
	m_email.Empty();
	m_url.Empty();
	m_comments.Empty();

	// If the dump path is to a folder, then delete the whole folder, if not, just delete the file.
	if(m_dmp_path.HasContent())
	{
		OpFile file;
		if (OpStatus::IsSuccess(file.Construct(m_dmp_path.CStr())))
			file.Delete(TRUE);
		m_dmp_path.Empty();
	}

	if(m_zip_path.HasContent())
	{
		OpFile file;
		if (OpStatus::IsSuccess(file.Construct(m_zip_path.CStr())))
			file.Delete(0);
		m_zip_path.Empty();
	}

	m_crash_time = 0;
	ResetUpload();
}

void LogSender::ResetUpload()
{
	m_uploading_url.UnsetURL();

	g_main_message_handler->UnsetCallBack(this, MSG_URL_DATA_LOADED);
	g_main_message_handler->UnsetCallBack(this, MSG_URL_LOADING_FAILED);
	g_main_message_handler->UnsetCallBack(this, MSG_URL_MOVED);
}
