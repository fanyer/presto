/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DAPI_SYSTEM_RESOURCE_SETTER_SUPPORT

#include "modules/device_api/SystemResourceSetter.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/util/opfile/opmemfile.h"
#include "modules/util/filefun.h"
#include "modules/dochand/win.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/img/imagedump.h"

void SystemResourceSetter::SetSystemResource(const uni_char* url_str, ResourceType resource_type, const uni_char* resource_id, SetSystemResourceCallback* callback, FramesDocument* document)
{
	SetSystemResourceHandler* handler;
	OP_STATUS error = SetSystemResourceHandler::Make(handler, url_str, resource_type, resource_id, callback, document);
	if (OpStatus::IsError(error))
		callback->OnFinished(error);
	else
		handler->Start();
}

SystemResourceSetter::SetSystemResourceHandler::SetSystemResourceHandler()
	: m_load_listener(*this)
	, m_query_format_callback(*this)
	, m_set_desktop_image_callback(*this)
	, m_set_ringtone_callback(*this)
{
}

SystemResourceSetter::SetSystemResourceHandler::~SetSystemResourceHandler()
{
	m_load_listener.Out();
}

/* static */ OP_STATUS
SystemResourceSetter::SetSystemResourceHandler::Make(SetSystemResourceHandler*& new_obj, const uni_char* url_str, ResourceType resource_type, const uni_char* resource_id, SetSystemResourceCallback* callback, FramesDocument* document)
{
	new_obj = OP_NEW(SetSystemResourceHandler, ());
	RETURN_OOM_IF_NULL(new_obj);
	OP_ASSERT((resource_type == CONTACT_RINGTONE) == (resource_id != NULL));
	RETURN_IF_ERROR(new_obj->m_resource_id.Set(resource_id));
	new_obj->m_callback = callback;
	new_obj->m_document = document;
	new_obj->m_url = g_url_api->GetURL(document->GetURL(), url_str);
	new_obj->m_resource_type = resource_type;
	OP_ASSERT(new_obj->m_document);
	return OpStatus::OK;
}

void
SystemResourceSetter::SetSystemResourceHandler::Start()
{
	OP_LOAD_INLINE_STATUS error = m_document->LoadInline(m_url, &m_load_listener);
	if (LoadInlineStatus::IsError(error))
		Finish(error);
}

void SystemResourceSetter::SetSystemResourceHandler::Finish(OP_STATUS status)
{
	if (m_callback)
		m_callback->OnFinished(status);
	OP_DELETE(this);
}

/* virtual */ void
SystemResourceSetter::LoadListenerImpl::LoadingStopped(FramesDocument *document, const URL &url)
{
	switch (url.GetAttribute(URL::KLoadStatus))
	{
	case URL_LOADING_FAILURE:
	case URL_LOADING_ABORTED:
		m_handler.Finish(SystemResourceSetter::Status::ERR_RESOURCE_LOADING_FAILED);
		return;
	case URL_LOADED:
		m_handler.OnResourceLoaded();
		return;
	case URL_EMPTY:
	case URL_UNLOADED:
	case URL_LOADING_WAITING:
	case URL_LOADING:
	case URL_LOADING_FROM_CACHE:
		OP_ASSERT(FALSE);
		return;
	default:
		OP_ASSERT(FALSE);// We don't expect any other state after that
		return;
	}
}

#define CALL_CALLBACK_IF_ERROR(expr, callback)        \
do {										          \
	OP_STATUS CALL_CALLBACK_IF_ERROR_TMP = expr;      \
	if (OpStatus::IsError(CALL_CALLBACK_IF_ERROR_TMP))\
	{                                                 \
		callback(CALL_CALLBACK_IF_ERROR_TMP);         \
		return;                                       \
	}                                                 \
} while(FALSE)

#define CALL_CALLBACK_IF_OOM(expr, callback) CALL_CALLBACK_IF_ERROR(expr ? OpStatus::OK : OpStatus::ERR_NO_MEMORY, callback)

#define BREAK_IF_ERROR(expr) {    \
	if (OpStatus::IsError(expr))  \
		break;                    \
} while (FALSE)

void
SystemResourceSetter::SetSystemResourceHandler::OnResourceLoaded()
{
	switch (m_resource_type)
	{
		case DESKTOP_IMAGE:
		{
			if (!m_url.IsImage())
			{
				Finish(SystemResourceSetter::Status::ERR_RESOURCE_FORMAT_UNSUPPORTED);
				return;
			}
			URLContentType content_type = m_url.ContentType();
			OpOSInteractionListener* os_interaction_listener = m_document->GetWindow()->GetWindowCommander()->GetOSInteractionListener();
			os_interaction_listener->OnQueryDesktopBackgroundImageFormat(content_type, &m_query_format_callback);
			return;
		}
		case DEFAULT_RINGTONE:
		case CONTACT_RINGTONE:
		{
			OpString filename;
			TRAPD(error, m_url.GetAttributeL(URL::KSuggestedFileName_L, filename));
			CALL_CALLBACK_IF_ERROR(error, Finish);
			OpString path_str;
			OpString tmp_storage;
			path_str.AppendFormat(UNI_L("%s%s"), g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_TEMP_FOLDER, tmp_storage), filename.CStr());
			m_url.SaveAsFile(path_str);
			g_op_system_info->SetRingtone(path_str.CStr(), &m_set_ringtone_callback, TRUE, m_resource_id.CStr());
			return;
		}
		default:
			OP_ASSERT(!"Not reachable");
	}
}

OP_STATUS SystemResourceSetter::SetSystemResourceHandler::DumpBitmapToFile(OpFileDescriptor& file_descriptor)
{
	OP_ASSERT(file_descriptor.IsOpen());
	OP_ASSERT(file_descriptor.IsWritable());

	UrlImageContentProvider* url_img_content_provider = UrlImageContentProvider::FindImageContentProvider(m_url);
	if (!url_img_content_provider)
		url_img_content_provider = OP_NEW(UrlImageContentProvider, (m_url));
	RETURN_OOM_IF_NULL(url_img_content_provider);
	Image img = url_img_content_provider->GetImage();
	RETURN_IF_ERROR(img.IncVisible(null_image_listener));
	OP_STATUS error = img.OnLoadAll(url_img_content_provider);
	if (OpStatus::IsError(error))
	{
		img.DecVisible(null_image_listener);
		return error;
	}
	OpBitmap* bitmap = img.GetBitmap(NULL);
	OP_ASSERT(bitmap);
	error = DumpOpBitmap(bitmap, file_descriptor, FALSE);
	img.DecVisible(null_image_listener);
	img.ReleaseBitmap();
	return error;
}

OP_STATUS SystemResourceSetter::SetSystemResourceHandler::DumpRawContentToFile(OpFileDescriptor& file_descriptor)
{
	OP_ASSERT(file_descriptor.IsOpen());
	OP_ASSERT(file_descriptor.IsWritable());
	OpAutoPtr<URL_DataDescriptor> desc(m_url.GetDescriptor(g_main_message_handler ,FALSE, TRUE));
	RETURN_OOM_IF_NULL(desc.get());
	BOOL more = TRUE;
	while (more)
	{
		unsigned long buf_size = desc->RetrieveData(more);
		if (buf_size == 0)
			break;
		const char* buf = desc->GetBuffer();
		OP_ASSERT(buf);
		RETURN_IF_ERROR(file_descriptor.Write(buf, buf_size));
		desc->ConsumeData(buf_size);
	}
	return OpStatus::OK;
}
/* virtual */ void
SystemResourceSetter::QueryDesktopBackgroundImageFormatCallbackImpl::OnFinished(OP_STATUS error, URLContentType content_type, const uni_char* dir_path)
{
	CALL_CALLBACK_IF_ERROR(error, m_handler.Finish);
	OP_ASSERT(dir_path && dir_path[0] != 0);
	// dir_path SHOULD always end with path separator
	OP_ASSERT(dir_path[uni_strlen(dir_path)-1] == UNI_L(PATHSEP)[0]);

	OpString suggested_filename;
	TRAP(error,
		m_handler.m_url.GetAttributeL(URL::KSuggestedFileName_L, suggested_filename);
	);
	CALL_CALLBACK_IF_ERROR(error, m_handler.Finish);

	OpString full_file_path;
	OpString file_name;
	OpString extension;
	TRAP(error,
		SplitFilenameIntoComponentsL(suggested_filename, NULL, &file_name, &extension);
	);
	CALL_CALLBACK_IF_ERROR(error, m_handler.Finish);

	if (content_type != m_handler.m_url.ContentType())
	{
		switch(content_type)
		{
			case URL_BMP_CONTENT:
				extension.Set("bmp");
				break;
			default:
				// Only conversion to BMP is handled atm
				m_handler.Finish(SystemResourceSetter::Status::ERR_RESOURCE_FORMAT_UNSUPPORTED);
				return;
		}
	}

	OpMemFile mem_file;
	BOOL mem_file_initialized = FALSE;
	BOOL file_exists = FALSE;
	full_file_path.AppendFormat(UNI_L("%s%s.%s"), dir_path, file_name.CStr(), extension.CStr());
	int i = 0;
	const int MAX_FILENAME_TRIES = 1000;
	do // Try to find most apropriate filename to save:
	{
		OpFile file;
		CALL_CALLBACK_IF_ERROR(file.Construct(full_file_path.CStr()), m_handler.Finish);
		CALL_CALLBACK_IF_ERROR(file.Exists(file_exists), m_handler.Finish);
		if (!file_exists)
			break; // file doesnt exist - go on with this filename
		if (!mem_file_initialized)
		{
			CALL_CALLBACK_IF_ERROR(mem_file.Open(OPFILE_WRITE), m_handler.Finish);
			if (content_type != m_handler.m_url.ContentType())
				CALL_CALLBACK_IF_ERROR(m_handler.DumpBitmapToFile(mem_file), m_handler.Finish);
			else
				CALL_CALLBACK_IF_ERROR(m_handler.DumpRawContentToFile(mem_file), m_handler.Finish);
			mem_file.Close();
			mem_file_initialized = TRUE;
		}
		BOOL files_equal;
		CALL_CALLBACK_IF_ERROR(AreFilesEqual(mem_file, file, files_equal), m_handler.Finish);
		if (files_equal)
			break; // ok the file was already there - nothing to do
		else
		{
			if (i > MAX_FILENAME_TRIES)
				CALL_CALLBACK_IF_ERROR(OpStatus::ERR, m_handler.Finish);
			else // just try next filename
			{
				++i;
				full_file_path.Empty();
				CALL_CALLBACK_IF_ERROR(full_file_path.AppendFormat(UNI_L("%s%s(%d).%s"), dir_path, file_name.CStr(), i, extension.CStr()), m_handler.Finish);
			}
		}
	}
	while (TRUE);

	if (!file_exists)
	{
		if (mem_file_initialized)
		{
			OpFile out_file;
			CALL_CALLBACK_IF_ERROR(out_file.Construct(full_file_path.CStr()), m_handler.Finish);
			CALL_CALLBACK_IF_ERROR(mem_file.Open(OPFILE_READ), m_handler.Finish);
			CALL_CALLBACK_IF_ERROR(out_file.Open(OPFILE_WRITE), m_handler.Finish);
			const unsigned int BUF_SIZE = 1024;
			OpFileLength bytes_read;
			char buf[BUF_SIZE]; /* ARRAY OK msimonides 2010-09-3 */
			do
			{
				CALL_CALLBACK_IF_ERROR(mem_file.Read(buf, BUF_SIZE, &bytes_read), m_handler.Finish);
				CALL_CALLBACK_IF_ERROR(out_file.Write(buf, bytes_read), m_handler.Finish);
			} while (bytes_read == BUF_SIZE);
			out_file.Close();
			mem_file.Close();

		}
		else if (content_type == m_handler.m_url.ContentType())
		{
			if (const_cast<URL&>(m_handler.m_url).SaveAsFile(full_file_path) != 0)
			{
				m_handler.Finish(SystemResourceSetter::Status::ERR_RESOURCE_SAVE_OPERATION_FAILED);
				return;
			}
		}
		else
		{
			OP_ASSERT(content_type == URL_BMP_CONTENT);
			OpFile out_file;
			CALL_CALLBACK_IF_ERROR(out_file.Construct(full_file_path.CStr()), m_handler.Finish);
			CALL_CALLBACK_IF_ERROR(out_file.Open(OPFILE_WRITE), m_handler.Finish);
			CALL_CALLBACK_IF_ERROR(m_handler.DumpBitmapToFile(out_file), m_handler.Finish);
			out_file.Close();
		}
	}
	OpOSInteractionListener* os_interaction_listener = m_handler.m_document->GetWindow()->GetWindowCommander()->GetOSInteractionListener();
	os_interaction_listener->OnSetDesktopBackgroundImage(full_file_path.CStr(), &m_handler.m_set_desktop_image_callback);
}

/* static */ OP_STATUS
SystemResourceSetter::AreFilesEqual(OpFileDescriptor& fd1, OpFileDescriptor& fd2, BOOL& equal)
{
	OP_STATUS error = OpStatus::OK;

	error = fd1.Open(OPFILE_READ);
	error = fd2.Open(OPFILE_READ);
	const unsigned int BUFFER_SIZE = 1024;
	char buf1[BUFFER_SIZE], buf2[BUFFER_SIZE]; /* ARRAY OK msimonides 2010-09-3 */
	OpFileLength bytes_read1, bytes_read2;
	do
	{
		error = fd1.Read(buf1, BUFFER_SIZE, &bytes_read1);
		BREAK_IF_ERROR(error);
		error = fd2.Read(buf2, BUFFER_SIZE, &bytes_read2);
		BREAK_IF_ERROR(error);
		if (bytes_read1 != bytes_read2 || op_memcmp(buf1, buf2, static_cast<size_t>(bytes_read1)) != 0)
		{
			equal = FALSE;
			break;
		}
	} while(bytes_read1 == BUFFER_SIZE);
	fd2.Close();
	fd1.Close();
	if (OpStatus::IsError(error))
		equal = FALSE;
	return error;
}

/* virtual */ void
SystemResourceSetter::SetDesktopBackgroundImageCallbackImpl::OnFinished(OP_STATUS error)
{
	m_handler.Finish(error);
}

/* virtual */ void
SystemResourceSetter::SetRingtoneCallbackImpl::OnFinished(OP_STATUS error, BOOL file_was_moved)
{
	m_handler.Finish(error);
}

#endif // DAPI_SYSTEM_RESOURCE_SETTER_SUPPORT
