/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/network/src/op_resource_impl.h"
#include "modules/network/src/op_response_impl.h"
#include "modules/network/op_response.h"
#include "modules/locale/locale-enum.h"

OpResourceImpl::OpResourceImpl(URL &url, OpRequest *request)
	: m_url(url)
	, m_request(request)
	, m_multipart_related_root_part(NULL)
#ifdef HAVE_DISK
	, m_save_file_handler(this)
#endif // HAVE_DISK
{
}

OpResourceImpl::~OpResourceImpl()
{
	if (m_multipart_related_root_part)
		OP_DELETE(m_multipart_related_root_part);
}

BOOL OpResourceImpl::QuickLoad(OpResource *&resource, OpURL url, BOOL guess_content_type)
{
	URL tempurl = url.GetURL();
	resource = OP_NEW(OpResourceImpl,(tempurl, NULL));
	return tempurl.QuickLoad(guess_content_type);
}

OP_STATUS OpResourceImpl::GetDataDescriptor(OpDataDescriptor *&data_descriptor,
										 RetrievalMode mode /*= DecompressAndCharsetConvert*/,
										 URLContentType override_content_type /*= URL_UNDETERMINED_CONTENT*/,
										 unsigned short override_charset_id /*= 0*/,
										 BOOL get_original_content /*= FALSE*/)
{
	URL_DataDescriptor *dd = m_url.GetDescriptor(g_main_message_handler, URL::KNoRedirect,
							 mode != DecompressAndCharsetConvert ? TRUE : FALSE,
							 mode != Unprocessed? TRUE : FALSE, NULL,
							 override_content_type, override_charset_id, get_original_content);
	if (!dd)
		return OpStatus::ERR_NO_MEMORY;

	data_descriptor = OP_NEW(OpDataDescriptor,(dd));
	if (!data_descriptor)
	{
		OP_DELETE(dd);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

#ifdef HAVE_DISK

void OpResource::ConnectListener(OpResourceSaveFileListener *listener)
{
	listener->m_resource = this;
}

void OpResource::DisconnectListener(OpResourceSaveFileListener *listener)
{
	listener->m_resource = NULL;
}

OpResourceSaveFileHandler::OpResourceSaveFileHandler(OpResourceImpl *resource)
	: m_resource(resource)
	, m_save_file_listener(NULL)
{
	OP_ASSERT(m_resource);
}

OpResourceSaveFileHandler::~OpResourceSaveFileHandler()
{
	OP_ASSERT(m_save_file_listener == NULL); // Otherwise, NotifyEnd hasn't been received and the save is not finished
	if (m_save_file_listener)
	{
		// TODO: The save in progress must be aborted here, but the current API does not support it.
		m_save_file_listener->OnSaveFileFinished(OpStatus::ERR, 0, 0);
		m_resource->DisconnectListener(m_save_file_listener);
		m_save_file_listener = NULL;
	}
}

OP_STATUS OpResourceSaveFileHandler::SaveAsFile(const OpStringC &file_name, OpResourceSaveFileListener *listener, BOOL delete_if_error)
{
	OP_ASSERT(listener);
	if (m_save_file_listener || !listener)
		return OpStatus::ERR;
	OP_STATUS status = OpStatus::OK;
	if (m_resource->m_url.GetAttribute(URL::KLoadStatus) == URL_LOADING)
	{
		m_save_file_listener = listener;
		m_resource->ConnectListener(m_save_file_listener);

		if (!m_resource->m_request) // m_request should be set (except for QuickLoad resources)
			status = OpStatus::ERR;
		if (OpStatus::IsSuccess(status))
			status = m_resource->m_request->AddListener(this);

		if (OpStatus::IsSuccess(status))
			status = m_resource->m_url.LoadToFile(file_name);

		if (OpStatus::IsError(status))
		{
			if (m_resource->m_request)
				OpStatus::Ignore(m_resource->m_request->RemoveListener(this));
			m_resource->DisconnectListener(m_save_file_listener);
			m_save_file_listener = NULL;
		}
		return SquashStatus(status, OpStatus::ERR_NO_MEMORY);
	}
	else
	{
#ifdef PI_ASYNC_FILE_OP
		m_save_file_listener = listener;
		m_resource->ConnectListener(m_save_file_listener);

		status = m_resource->m_url.ExportAsFileAsync(file_name, this, 0, delete_if_error);

		if (OpStatus::IsError(status))
		{
			m_resource->DisconnectListener(m_save_file_listener);
			m_save_file_listener = NULL;
		}
		return SquashStatus(status, OpStatus::ERR_NO_MEMORY);
#else
		OpFile file;
		OpFileLength length = 0;

		status = m_resource->m_url.ExportAsFile(file_name);

		if (OpStatus::IsSuccess(status) &&
			OpStatus::IsSuccess(status = file.Construct(file_name)))
			status = file.GetFileLength(length);

		status = SquashStatus(status, OpStatus::ERR_NO_MEMORY, OpStatus::ERR_NO_DISK, OpStatus::ERR_NO_ACCESS, OpStatus::ERR_FILE_NOT_FOUND);
		if (OpStatus::IsSuccess(status))
			listener->OnSaveFileProgress(length, length);
		listener->OnSaveFileFinished(status, length, length);
		return OpStatus::OK; // Any error was delivered in the callback
#endif
	}
}

#ifdef _MIME_SUPPORT_

OP_STATUS OpResourceImpl::GetMHTMLRootPart(OpResponse *&root_part)
{
	if (!m_multipart_related_root_part)
	{
		root_part = NULL;
		URL url = m_url.GetMHTMLRootPart();
		OP_ASSERT(url.IsEmpty() || url.GetAttribute(URL::KLoadStatus) == URL_LOADED);
		if (url.IsEmpty() || url.GetAttribute(URL::KLoadStatus) != URL_LOADED)
			return OpStatus::ERR;

		m_multipart_related_root_part = OP_NEW(OpResponseImpl, (OpURL(url)));
		if (!m_multipart_related_root_part)
			return OpStatus::ERR_NO_MEMORY;

		m_multipart_related_root_part->m_resource = OP_NEW(OpResourceImpl, (url, m_request));
		if (!m_multipart_related_root_part->m_resource)
		{
			OP_DELETE(m_multipart_related_root_part);
			m_multipart_related_root_part = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	root_part = m_multipart_related_root_part;
	return OpStatus::OK;
}

#endif // _MIME_SUPPORT_

#ifdef PI_ASYNC_FILE_OP

void OpResourceSaveFileHandler::NotifyProgress(OpFileLength bytes_saved, OpFileLength length, URL_Rep *rep, const OpStringC &name, UINT32 param)
{
	if (m_save_file_listener)
		m_save_file_listener->OnSaveFileProgress(bytes_saved, length);
}

void OpResourceSaveFileHandler::NotifyEnd(OP_STATUS ops, OpFileLength bytes_saved, OpFileLength length, URL_Rep *rep, const OpStringC &name, UINT32 param)
{
	if (m_save_file_listener)
	{
		OpResourceSaveFileListener *tmp_listener = m_save_file_listener;
		m_resource->DisconnectListener(m_save_file_listener);
		m_save_file_listener = NULL;
		ops = SquashStatus(ops, OpStatus::ERR_NO_MEMORY, OpStatus::ERR_NO_DISK, OpStatus::ERR_NO_ACCESS, OpStatus::ERR_FILE_NOT_FOUND);
		tmp_listener->OnSaveFileFinished(ops, bytes_saved, length);
	}
}

#endif // PI_ASYNC_FILE_OP

void OpResourceSaveFileHandler::OnResponseDataLoaded(OpRequest *req, OpResponse *res)
{
	if (m_save_file_listener)
		m_save_file_listener->OnSaveFileProgress(res->GetLoadedContentSize(), res->GetContentSize());
}

void OpResourceSaveFileHandler::OnResponseFinished(OpRequest *req, OpResponse *res)
{
	if (m_save_file_listener)
	{
		OpResourceSaveFileListener *tmp_listener = m_save_file_listener;
		m_resource->DisconnectListener(m_save_file_listener);
		m_save_file_listener = NULL;
		tmp_listener->OnSaveFileFinished(OpStatus::OK, res->GetLoadedContentSize(), res->GetContentSize());
	}
	OpStatus::Ignore(m_resource->m_request->RemoveListener(this));
}

void OpResourceSaveFileHandler::OnRequestFailed(OpRequest *req, OpResponse *res, Str::LocaleString error)
{
	if (m_save_file_listener)
	{
		OpResourceSaveFileListener *tmp_listener = m_save_file_listener;
		m_resource->DisconnectListener(m_save_file_listener);
		m_save_file_listener = NULL;
		tmp_listener->OnSaveFileFinished(OpStatus::ERR_NO_SUCH_RESOURCE, res->GetLoadedContentSize(), res->GetContentSize());
	}
	OpStatus::Ignore(m_resource->m_request->RemoveListener(this));
}

OpResourceSaveFileListener::OpResourceSaveFileListener()
	: m_resource(NULL)
{
}

OpResourceSaveFileListener::~OpResourceSaveFileListener()
{
	if (m_resource)
	{
		static_cast<OpResourceImpl*>(m_resource)->m_save_file_handler.m_save_file_listener = NULL;
		m_resource = NULL;
	}
}

#endif // HAVE_DISK
