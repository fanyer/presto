/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#if defined(GADGET_SUPPORT)
#include "modules/util/opfile/opfile.h"

#include "modules/url/url2.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_man.h"
#include "modules/url/url_ds.h"
#include "modules/cache/url_cs.h"
#include "modules/url/loadhandler/url_lh.h"
#include "modules/dochand/win.h" 
#include "modules/dochand/docman.h" 
#include "modules/formats/uri_escape.h"

#include "modules/gadgets/OpGadget.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/gadgets/gadget_utils.h"
#include "modules/url/tools/content_detector.h"


// Url_lhwidget.cpp

// URL Load Handler widget:// protocol

void URL_DataStorage::StartLoadingWidget(MessageHandler* msg_handler, BOOL guess_content_type)
{
	OP_STATUS op_err = OpStatus::OK;

	url->SetAttribute(URL::KIsGeneratedByOpera, FALSE);
	url->Access(FALSE);

	// Clear MIME attributes, since their contents is unknown at this point.
	// The call to OpGadgetManager below may set them, if known.
	SetAttribute(URL::KMIME_CharSet, NULL);

	// Translate into fully qualified path
	OpString path; // result goes here
	URL resource_url(url, static_cast<const char*>(NULL));

	if (OpStatus::IsError(g_gadget_manager->FindGadgetResource(msg_handler, &resource_url, path)) || !path.CStr() || !*path.CStr())
	{
		SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_FILE_CANNOT_OPEN));

		UnsetListCallbacks();
		mh_list->Clean();
		return;
	}

	// Check if the file we got actually exists.
	BOOL exists = FALSE;

	OpFile temp_file;
	temp_file.Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER, OPFILE_FLAGS_ASSUME_ZIP | OPFILE_FLAGS_CASE_SENSITIVE_ZIP);
	temp_file.Exists(exists);

	if(!exists)
	{
		SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_FILE_CANNOT_OPEN));

		UnsetListCallbacks();
		mh_list->Clean();
		return;
	}

	// Start loading.
	SetAttribute(URL::KLoadStatus,URL_LOADING);

	content_size = 0;
	temp_file.GetFileLength(content_size);

	// Create a storage object, pointing to our local file (possibly inside
	// a zip archive).
	SetAttribute(URL::KHeaderLoaded,TRUE);
	OpStackAutoPtr<Cache_Storage> storage_ptr(storage = Local_File_Storage::Create(url, path.CStr(), OPFILE_ABSOLUTE_FOLDER, OPFILE_FLAGS_ASSUME_ZIP | OPFILE_FLAGS_CASE_SENSITIVE_ZIP));
	
	if (storage == NULL)
	{
		SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_FILE_CANNOT_OPEN));
		UnsetListCallbacks();
		mh_list->Clean();
		return;
	}

	OP_DELETE(old_storage);
	old_storage = NULL;

	// Figure out a Content-Type for this resource by looking at the file
	// extension, unless OpGadgetManager::FindGadgetResource() set one
	// for us.
	URLContentType cnt_typ = static_cast<URLContentType>(GetAttribute(URL::KContentType));
	if (URL_UNDETERMINED_CONTENT == cnt_typ)
	{
		const uni_char* fileext = uni_strrchr(path.CStr(), '.');

		if (fileext)
		{
			FindContentType(cnt_typ, NULL, fileext+1, NULL, TRUE);
			SetAttribute(URL::KContentType, cnt_typ);
		}
	}

	// If the type is still unknown, throw an error.
	if ((URLContentType) GetAttribute(URL::KContentType) == URL_UNDETERMINED_CONTENT && guess_content_type)
	{
		ContentDetector content_detector(url, TRUE, TRUE);
		op_err = content_detector.DetectContentType();
		if(OpStatus::IsError(op_err))
		{
			SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_FILE_CANNOT_OPEN));

			UnsetListCallbacks();
			mh_list->Clean();
			return;
		}
	}

	// Everything is fine, now set the MIME type string accordingly.
	op_err = OpStatus::OK;
	switch ((URLContentType) GetAttribute(URL::KContentType))
	{
	case URL_HTML_CONTENT:
		op_err = SetAttribute(URL::KMIME_Type, "text/html");
		break;
	case URL_TEXT_CONTENT:
		op_err = SetAttribute(URL::KMIME_Type, "text/plain");
		break;
	default:
		break;
	}

	if (OpStatus::IsError(op_err))
	{
		SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_FILE_CANNOT_OPEN));
		
		UnsetListCallbacks();
		mh_list->Clean();
		return;
	}

	BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), 0, MH_LIST_ALL);
	BroadcastMessage(MSG_URL_DATA_LOADED, url->GetID(), 0, MH_LIST_ALL);
	SetAttribute(URL::KLoadStatus,URL_LOADED);

	if ((URLContentType) GetAttribute(URL::KContentType) == URL_UNDETERMINED_CONTENT && guess_content_type)
	{
		ContentDetector content_detector(url, TRUE, TRUE);
		OpStatus::Ignore(content_detector.DetectContentType()); 
	}

	time_t time_now = g_timecache->CurrentTime();
	TRAP(op_err, SetAttributeL(URL::KVLocalTimeLoaded, &time_now));
	OpStatus::Ignore(op_err);

	UnsetListCallbacks();
	mh_list->Clean();
	storage_ptr.release();
}
#endif /* GADGET_SUPPORT */ 
