/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_RESOURCE_MANAGER

#include "modules/scope/src/scope_transport.h"
#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_window_manager.h"
#include "modules/scope/src/scope_resource_manager.h"
#include "modules/scope/src/scope_document_manager.h"
#include "modules/dochand/win.h"
#include "modules/dochand/fdelm.h"
#include "modules/url/tools/url_util.h"
#include "modules/url/url_man.h"
#include "modules/encodings/utility/opstring-encodings.h"
#include "modules/formats/hdsplit.h"
#include "modules/cookies/url_cm.h"
#include "modules/formats/encoder.h"
#include "modules/dochand/winman.h"
#include "modules/upload/upload.h"
#include "modules/pi/OpSystemInfo.h"

/* OpScopeResourceManager */

OpScopeResourceManager::OpScopeResourceManager()
	: OpScopeResourceManager_SI()
	, frame_ids(NULL)
	, document_ids(NULL)
	, resource_ids(NULL)
	, reload_policy(OpScopeResourceManager::RELOADPOLICY_DEFAULT)
	, filter_request_mime(&filter_request_default)
	, filter_response_mime(&filter_response_default)
	, filter_request_mime_multipart(&filter_request_mime)
{
}

/* virtual */
OpScopeResourceManager::~OpScopeResourceManager()
{
	if (frame_ids)
		frame_ids->Release();
	if (document_ids)
		document_ids->Release();
	if (resource_ids)
		resource_ids->Release();
}

OP_STATUS
OpScopeResourceManager::Construct(OpScopeIDTable<DocumentManager> *o_frame_ids,
                                  OpScopeIDTable<FramesDocument> *o_document_ids,
                                  OpScopeIDTable<unsigned> *o_resource_ids)
{
	OP_ASSERT(o_frame_ids && o_document_ids && o_resource_ids);
	OP_ASSERT(!frame_ids && !document_ids && !resource_ids);
	frame_ids = o_frame_ids->Retain();
	document_ids = o_document_ids->Retain();
	resource_ids = o_resource_ids->Retain();

	net_buf_len = 4096;
	net_buf.reset(OP_NEWA(char, net_buf_len));
	RETURN_OOM_IF_NULL(net_buf.get());

	TRAP_AND_RETURN(err, xhr_attr = g_url_api->RegisterAttributeL(&xhr_attr_handler));

	return OpStatus::OK;
}

/* virtual */
OP_STATUS
OpScopeResourceManager::OnPostInitialization()
{
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	if (OpScopeServiceManager *manager = GetManager())
		window_manager = OpScopeFindService<OpScopeWindowManager>(manager, "window-manager");
	if (!window_manager)
		return OpStatus::ERR_NULL_POINTER;
#endif // SCOPE_WINDOW_MANAGER_SUPPORT

	return OpStatus::OK;
}

/* virtual */
OP_STATUS
OpScopeResourceManager::OnServiceEnabled()
{
	// Only enable the service if all pointers are initialized
	if (!frame_ids || !document_ids || !resource_ids)
		return OpStatus::ERR_NULL_POINTER;

	return OpStatus::OK;
}

/* virtual */
OP_STATUS
OpScopeResourceManager::OnServiceDisabled()
{
	frame_ids->Reset();
	document_ids->Reset();
	resource_ids->Reset();

	header_overrides.DeleteAll();
	active_resources.DeleteAll();

	reload_policy = OpScopeResourceManager::RELOADPOLICY_DEFAULT;

	filter_request_default.Reset();
	filter_response_default.Reset();
	filter_request_mime.Reset();
	filter_response_mime.Reset();
	filter_request_mime_multipart.Reset();

	return OpStatus::OK;
}

static OpScopeResourceManager::UrlLoad::URLType ToURLType(URLType type)
{
	// Note: Not all types are supported in callbacks so they might not be used (yet).
	switch (type)
	{
	case URL_FILE: return OpScopeResourceManager::UrlLoad::URLTYPE_FILE;
	case URL_DATA: return OpScopeResourceManager::UrlLoad::URLTYPE_DATA;
	case URL_HTTP: return OpScopeResourceManager::UrlLoad::URLTYPE_HTTP;
	case URL_HTTPS: return OpScopeResourceManager::UrlLoad::URLTYPE_HTTPS;

	case URL_FTP: return OpScopeResourceManager::UrlLoad::URLTYPE_FTP;
	case URL_Gopher: return OpScopeResourceManager::UrlLoad::URLTYPE_GOPHER;
	case URL_WAIS: return OpScopeResourceManager::UrlLoad::URLTYPE_WAIS;
	case URL_NEWS: return OpScopeResourceManager::UrlLoad::URLTYPE_NEWS;
	case URL_SNEWS: return OpScopeResourceManager::UrlLoad::URLTYPE_SNEWS;
	case URL_NEWSATTACHMENT: return OpScopeResourceManager::UrlLoad::URLTYPE_NEWSATTACHMENT;
	case URL_SNEWSATTACHMENT: return OpScopeResourceManager::UrlLoad::URLTYPE_SNEWSATTACHMENT;
	case URL_EMAIL: return OpScopeResourceManager::UrlLoad::URLTYPE_EMAIL;
	case URL_ATTACHMENT: return OpScopeResourceManager::UrlLoad::URLTYPE_ATTACHMENT;
	case URL_TELNET: return OpScopeResourceManager::UrlLoad::URLTYPE_TELNET;
	case URL_MAILTO: return OpScopeResourceManager::UrlLoad::URLTYPE_MAILTO;
	case URL_OPERA: return OpScopeResourceManager::UrlLoad::URLTYPE_OPERA;
	case URL_JAVASCRIPT: return OpScopeResourceManager::UrlLoad::URLTYPE_JAVASCRIPT;
	case URL_CONTENT_ID: return OpScopeResourceManager::UrlLoad::URLTYPE_CONTENT_ID;
	case URL_TN3270: return OpScopeResourceManager::UrlLoad::URLTYPE_TN3270;
	case URL_NNTP: return OpScopeResourceManager::UrlLoad::URLTYPE_NNTP;
	case URL_NNTPS: return OpScopeResourceManager::UrlLoad::URLTYPE_NNTPS;
	case URL_TV: return OpScopeResourceManager::UrlLoad::URLTYPE_TV;
	case URL_TEL: return OpScopeResourceManager::UrlLoad::URLTYPE_TEL;
	case URL_MAIL: return OpScopeResourceManager::UrlLoad::URLTYPE_MAIL;
	case URL_IRC: return OpScopeResourceManager::UrlLoad::URLTYPE_IRC;
	case URL_CHAT_TRANSFER: return OpScopeResourceManager::UrlLoad::URLTYPE_CHAT_TRANSFER;
	case URL_MOUNTPOINT: return OpScopeResourceManager::UrlLoad::URLTYPE_MOUNTPOINT;
	case URL_WIDGET: return OpScopeResourceManager::UrlLoad::URLTYPE_WIDGET;
	case URL_UNITE: return OpScopeResourceManager::UrlLoad::URLTYPE_UNITE;

	case URL_NULL_TYPE:
	default: return OpScopeResourceManager::UrlLoad::URLTYPE_UNKNOWN;
	}
}

OP_STATUS
OpScopeResourceManager::OnUrlLoad(URL_Rep *url_rep, DocumentManager *docman, Window *window)
{
	OP_ASSERT(url_rep);
	if (!IsEnabled() || !AcceptContext(docman, window))
		return OpStatus::OK;

	unsigned *url_id_ptr = reinterpret_cast<unsigned *>(static_cast<UINTPTR>(url_rep->GetID()));
	ResourceContext *context = NULL;
	OP_ASSERT(!active_resources.Contains(url_id_ptr));
	if (OpStatus::IsSuccess(active_resources.GetData(url_id_ptr, &context)) && context)
	{
		// We received a double UrlLoad for the same URL, this should not happen.
		RemoveResourceContext(url_rep);
		return OpStatus::ERR;
	}
	RETURN_IF_ERROR(AddResourceContext(url_rep, docman, window));

	UrlLoad msg;

	// Set origin flag?
	if (url_rep->GetAttribute(xhr_attr, TRUE) != 0)
	{
		RETURN_IF_MEMORY_ERROR(url_rep->SetAttribute(xhr_attr, 0)); // Reset flag.
		msg.SetLoadOrigin(LOADORIGIN_XHR);
	}

	unsigned resource_id;
	RETURN_IF_ERROR(GetResourceID(url_rep, resource_id));
	msg.SetResourceID(resource_id);

	msg.SetWindowID(window ? window->Id() : 0); // Fall back on 0.
	if (docman)
	{
		unsigned frame_id;
		RETURN_IF_ERROR(GetFrameID(docman, frame_id));
		msg.SetFrameID(frame_id);

		FramesDocument *frm_doc = docman->GetCurrentDoc();

		// For iframes, which belong to their parent document.
		if (!frm_doc)
			frm_doc = docman->GetParentDoc();

		if (frm_doc)
		{
			unsigned document_id;
			RETURN_IF_ERROR(GetDocumentID(frm_doc, document_id));
			msg.SetDocumentID(document_id);
		}
	}
	RETURN_IF_ERROR(url_rep->GetAttribute(URL::KUniName_Username_Password_Hidden, 0, msg.GetUrlRef(), URL::KNoRedirect));
	msg.SetTime(g_op_time_info->GetTimeUTC());
	msg.SetUrlType(ToURLType((URLType)url_rep->GetAttribute(URL::KType)));

	return SendOnUrlLoad(msg);
}

OP_STATUS
OpScopeResourceManager::OnUrlFinished(URL_Rep *url_rep, URLLoadResult result)
{
	OP_ASSERT(url_rep);
	if (!IsEnabled() || !AcceptResource(url_rep))
		return OpStatus::OK;

	ResourceRemover remover(this, url_rep);

	unsigned resource_id;
	UrlFinished msg;
	RETURN_IF_ERROR(GetResourceID(url_rep, resource_id));
	RETURN_IF_ERROR(SetUrlFinished(msg, resource_id, result));
	RETURN_IF_ERROR(url_rep->GetAttribute(URL::KMIME_Type, msg.GetMimeTypeRef()));
	RETURN_IF_ERROR(url_rep->GetAttribute(URL::KMIME_CharSet, msg.GetCharacterEncodingRef()));

	OpFileLength content_length;
	url_rep->GetAttribute(URL::KContentSize, &content_length);
	msg.SetContentLength(static_cast<UINT32>(content_length));

	return SendOnUrlFinished(msg);
}

OP_STATUS
OpScopeResourceManager::OnUrlRedirect(URL_Rep *orig_url_rep, URL_Rep *new_url_rep)
{
	OP_ASSERT(orig_url_rep && new_url_rep);
	if (!IsEnabled() || !AcceptResource(orig_url_rep))
		return OpStatus::OK;

	RETURN_IF_ERROR(redirected_resources.Add(orig_url_rep));

	UrlRedirect msg;
	unsigned orig_resource_id, new_resource_id;
	RETURN_IF_ERROR(GetResourceID(orig_url_rep, orig_resource_id));
	RETURN_IF_ERROR(GetResourceID(new_url_rep, new_resource_id));
	msg.SetFromResourceID(orig_resource_id);
	msg.SetToResourceID(new_resource_id);
	msg.SetTime(g_op_time_info->GetTimeUTC());

	return SendOnUrlRedirect(msg);
}

OP_STATUS
OpScopeResourceManager::OnUrlUnload(URL_Rep *rep)
{
	if (!IsEnabled())
		return OpStatus::OK;
	OP_ASSERT(rep);

	// Do not wrap the URL_Rep in an URL object at this point. It will cause crashes.
	unsigned *url_id_ptr = reinterpret_cast<unsigned*>(static_cast<UINTPTR>(rep->GetID()));

	// Remove any actively tracked resources and report that they failed
	if (active_resources.Contains(url_id_ptr))
	{
		RemoveResourceContext(rep);

		unsigned resource_id;
		UrlFinished msg;
		RETURN_IF_ERROR(GetResourceID(rep, resource_id));
		RETURN_IF_ERROR(SetUrlFinished(msg, resource_id, URL_LOAD_FAILED));

		RETURN_IF_ERROR(SendOnUrlFinished(msg));
	}

	// Report OnUrlUnload if we have the URL registered with a resource ID
	if (!resource_ids->HasObject(url_id_ptr))
		return OpStatus::OK;

	UrlUnload msg;

	unsigned resource_id;
	RETURN_IF_ERROR(resource_ids->GetID(url_id_ptr, resource_id));
	msg.SetResourceID(resource_id);
	msg.SetTime(g_op_time_info->GetTimeUTC());

	resource_ids->RemoveID(resource_id);

	return SendOnUrlUnload(msg);
}

OP_STATUS
OpScopeResourceManager::OnComposeRequest(URL_Rep *url_rep, int request_id, Upload_EncapsulatedElement *upload, DocumentManager *docman, Window *window)
{
	if (!IsEnabled() || !AcceptResource(url_rep))
		return OpStatus::OK;

	HeaderManager hm(upload);

	OpString8 tmp_name;
	OpString8 tmp_value;

	CustomRequest *custom = GetCustomRequest(url_rep);

	if (custom)
	{
		if (custom->GetHeaderPolicy() == CreateRequestArg::HEADERPOLICY_REPLACE)
			hm.RemoveAll();
		for (unsigned i = 0; i < custom->GetHeaders().GetCount(); ++i)
		{
			RETURN_IF_ERROR(tmp_name.Set(custom->GetHeaders().Get(i)->GetName()));
			RETURN_IF_ERROR(tmp_value.Set(custom->GetHeaders().Get(i)->GetValue()));
			RETURN_IF_ERROR(hm.Override(tmp_name, tmp_value));
		}
	}
	else
	{
		OpAutoPtr<OpHashIterator> iter(header_overrides.GetIterator());
		RETURN_OOM_IF_NULL(iter.get());

		BOOL more = OpStatus::IsSuccess(iter->First());

		while (more)
		{
			const uni_char *key = static_cast<const uni_char*>(iter->GetKey());
			Header *header = static_cast<Header *>(iter->GetData());

			RETURN_IF_ERROR(tmp_name.Set(key));
			RETURN_IF_ERROR(tmp_value.Set(header->GetValue()));
			RETURN_IF_ERROR(hm.Override(tmp_name, tmp_value));

			more = OpStatus::IsSuccess(iter->Next());
		}
	}

	return OpStatus::OK;
}

OP_STATUS
OpScopeResourceManager::OnRequest(URL_Rep *url_rep, int request_id, Upload_EncapsulatedElement *upload, const char* buf, size_t len)
{
	if (!IsEnabled())
		return OpStatus::OK;

	OpStatus::Ignore(http_requests.Add(url_rep, request_id));

	ResourceContext *context;
	GetResourceContext(url_rep, context);

	if (!context)
		return OpStatus::OK;

	DocumentManager *docman = context->GetDocumentManager();
	Window *window = context->GetWindow();

	Request msg;

	msg.SetRequestID(request_id);
	msg.SetTime(g_op_time_info->GetTimeUTC());

	unsigned resource_id;
	RETURN_IF_ERROR(GetResourceID(url_rep, resource_id));
	msg.SetResourceID(resource_id);
	
	msg.SetWindowID(window ? window->Id() : 0); // Fall back on 0.
	if (docman)
	{
		unsigned frame_id;
		RETURN_IF_ERROR(GetFrameID(docman, frame_id));
		msg.SetFrameID(frame_id);

		FramesDocument *frm_doc = docman->GetCurrentDoc();

		// For iframes, which belong to their parent document.
		if (!frm_doc)
			frm_doc = docman->GetParentDoc();

		if (frm_doc)
		{
			unsigned document_id;
			RETURN_IF_ERROR(GetDocumentID(frm_doc, document_id));
			msg.SetDocumentID(document_id);
		}
	}

	OpString url_str;
	RETURN_IF_ERROR(url_rep->GetAttribute(URL::KUniName_Username_Password_Hidden, 0 /* charsetID */ , url_str));
	RETURN_IF_ERROR(msg.SetUrl(url_str.CStr()));

	RETURN_IF_ERROR(SetMethod(url_rep, msg.GetMethodRef()));
	RETURN_IF_ERROR(SendOnRequest(msg));

	// OnRequestHeader

	RequestHeader rh;
	rh.SetRequestID(request_id);
	rh.SetResourceID(resource_id);
	rh.SetTime(g_op_time_info->GetTimeUTC());
	RETURN_IF_ERROR(rh.SetRaw(buf, len));

	HeaderManager hm(upload);

	Header_Item *h = hm.First();

	OpAutoArray<char> hbuf;
	unsigned hbuf_len = 0;

	while (h)
	{
		if (h->HasParameters())
		{
			Header *h_out = rh.GetHeaderListRef().AddNew();
			RETURN_OOM_IF_NULL(h_out);
			RETURN_IF_ERROR(SetHeader(*h_out, h, hbuf, hbuf_len));
		}

		h = hm.Next();
	}

	// Send request headers in a separate message. This is not strictly necessary, but
	// maintains a symmetry between the Reqeust* and Response* messages in the service.
	RETURN_IF_ERROR(SendOnRequestHeader(rh));

	// If there is no payload in this upload, OnRequestFinished will not be called, and
	// the request finishes immediately.
	if (!upload->GetElement())
		RETURN_IF_ERROR(OnRequestFinished(url_rep, request_id, upload));

	return OpStatus::OK;
}

OP_STATUS
OpScopeResourceManager::OnRequestFinished(URL_Rep *url_rep, int request_id, Upload_EncapsulatedElement *upload)
{
	if (!IsEnabled() || !AcceptResource(url_rep))
		return OpStatus::OK;

	RequestFinished msg;
	msg.SetRequestID(request_id);
	msg.SetTime(g_op_time_info->GetTimeUTC());
	Upload_Base *element = upload->GetElement();

	unsigned resource_id;
	RETURN_IF_ERROR(GetResourceID(url_rep, resource_id));
	msg.SetResourceID(resource_id);

	if (element)
	{
		OpAutoPtr<RequestData> rd_auto(OP_NEW(RequestData, ()));
		RETURN_OOM_IF_NULL(rd_auto.get());

		rd_auto->SetResourceID(resource_id);
		rd_auto->SetRequestID(request_id);
		RETURN_IF_ERROR(rd_auto->GetCharacterEncodingRef().Set(element->GetCharset()));
		RETURN_IF_ERROR(rd_auto->GetMimeTypeRef().Set(element->GetContentType()));

		ContentMode *mode;
		RETURN_IF_ERROR(GetRequestMode(url_rep, element, mode));

		OpFileLength payload_len = element->PayloadLength();
		BOOL too_big = (static_cast<unsigned>(payload_len) > mode->GetSizeLimit());
		rd_auto->SetContentLength(static_cast<UINT32>(payload_len));

		if (mode->GetTransport() != ContentMode::TRANSPORT_OFF && !too_big)
		{
			OpAutoPtr<Content> content(OP_NEW(Content, ()));
			RETURN_OOM_IF_NULL(content.get());

			RequestContentReader reader(element, net_buf.get(), net_buf_len);
			RETURN_IF_ERROR(SetContent(*(content.get()), url_rep, reader, *mode, element->GetCharset(), element->GetContentType()));
			rd_auto->SetContent(content.release());
		}

		// Check whether this is a multipart upload.
		Upload_Base *part = element->FirstChild();

		if (part) // Multipart
		{
			Upload_Base *p = part;

			OpAutoArray<char> hbuf;
			unsigned hbuf_len = 0;

			while (p)
			{
				RequestData::Part *part = rd_auto->GetPartListRef().AddNew();
				RETURN_OOM_IF_NULL(part);

				Header_Item *h = p->AccessHeaders().First();

				while (h)
				{
					if (h->HasParameters())
					{
						Header *h_out = part->GetHeaderListRef().AddNew();
						RETURN_OOM_IF_NULL(h_out);
						RETURN_IF_ERROR(SetHeader(*h_out, h, hbuf, hbuf_len));
					}
					h = h->Suc();
				}

				ContentMode *mode;
				RETURN_IF_ERROR(GetMultipartRequestMode(p, mode));

				OpFileLength payload_len = p->PayloadLength();
				BOOL too_big = (static_cast<unsigned>(payload_len) > mode->GetSizeLimit());
				part->SetContentLength(static_cast<UINT32>(payload_len));

				if (mode->GetTransport() != ContentMode::TRANSPORT_OFF && !too_big)
				{
					OpAutoPtr<Content> content(OP_NEW(Content, ()));
					RETURN_OOM_IF_NULL(content.get());

					RequestContentReader reader(p, net_buf.get(), net_buf_len);
					RETURN_IF_ERROR(SetContent(*(content.get()), url_rep, reader, *mode, p->GetCharset(), p->GetContentType()));
					part->SetContent(content.release());
				}

				p = p->Suc();
			}
		}
		
		msg.SetData(rd_auto.release());
	}

	return SendOnRequestFinished(msg);
}

OP_STATUS
OpScopeResourceManager::OnRequestRetry(URL_Rep *url_rep, int orig_request_id, int new_request_id)
{
	if (!IsEnabled() || !AcceptResource(url_rep))
		return OpStatus::OK;

	RETURN_IF_ERROR(http_requests.Remove(url_rep, &orig_request_id));
	RETURN_IF_ERROR(http_requests.Add(url_rep, new_request_id));

	RequestRetry msg;

	unsigned resource_id;
	RETURN_IF_ERROR(GetResourceID(url_rep, resource_id));
	msg.SetResourceID(resource_id);
	msg.SetFromRequestID(orig_request_id);
	msg.SetRequestID(orig_request_id);
	msg.SetToRequestID(new_request_id);
	msg.SetTime(g_op_time_info->GetTimeUTC());

	return SendOnRequestRetry(msg);
}

OP_STATUS
OpScopeResourceManager::OnResponse(URL_Rep *url_rep, int request_id, int response_code)
{
	if (!IsEnabled() || !AcceptResource(url_rep))
		return OpStatus::OK;

	Response msg;
	msg.SetRequestID(request_id);
	msg.SetTime(g_op_time_info->GetTimeUTC());

	unsigned resource_id;
	RETURN_IF_ERROR(GetResourceID(url_rep, resource_id));
	msg.SetResourceID(resource_id);

	if (response_code)
		msg.SetResponseCode(response_code);

	return SendOnResponse(msg);
}

OP_STATUS
OpScopeResourceManager::OnResponseHeader(URL_Rep *url_rep, int request_id, const HeaderList *headerList, const char* buf, size_t len)
{
	if (!IsEnabled() || !AcceptResource(url_rep))
		return OpStatus::OK;

	ResponseHeader msg;
	msg.SetRequestID(request_id);

	unsigned resource_id;
	RETURN_IF_ERROR(GetResourceID(url_rep, resource_id));
	msg.SetResourceID(resource_id);
	msg.SetTime(g_op_time_info->GetTimeUTC());
	msg.SetRaw(buf, len);

	NameValue_Splitter *h = headerList ? headerList->First() : NULL;

	while (h)
	{
		Header *h_out = msg.GetHeaderListRef().AddNew();
		RETURN_OOM_IF_NULL(h_out);
		RETURN_IF_ERROR(h_out->SetName(h->GetName().CStr()));
		RETURN_IF_ERROR(h_out->SetValue(h->GetValue().CStr()));
		h = h->Suc();
	}

	return SendOnResponseHeader(msg);
}

OP_STATUS
OpScopeResourceManager::OnResponseFinished(URL_Rep *url_rep, int request_id)
{
	if (!IsEnabled() || !AcceptResource(url_rep))
		return OpStatus::OK;

	ResponseFinished msg;
	msg.SetRequestID(request_id);

	unsigned resource_id;
	RETURN_IF_ERROR(GetResourceID(url_rep, resource_id));
	msg.SetResourceID(resource_id);

	OpFileLength content_length;
	url_rep->GetAttribute(URL::KContentSize, &content_length);

	if (!content_length)
		url_rep->GetAttribute(URL::KContentLoaded, &content_length);

	msg.SetContentLength(static_cast<UINT32>(content_length));
	msg.SetTime(g_op_time_info->GetTimeUTC());

	ContentMode *mode;
	RETURN_IF_ERROR(GetResponseMode(url_rep, mode));

	OpAutoPtr<ResourceData> rd_auto(OP_NEW(ResourceData, ()));
	RETURN_OOM_IF_NULL(rd_auto.get());
	RETURN_IF_ERROR(SetResourceData(*(rd_auto.get()), url_rep, *mode));
	msg.SetData(rd_auto.release());

	OP_STATUS status = SendOnResponseFinished(msg);

	if (redirected_resources.Contains(url_rep))
		ResourceRemover remover(this, url_rep);

	return status;
}

BOOL
OpScopeResourceManager::ForceFullReload(URL_Rep *url_rep, DocumentManager *docman, Window *window)
{
	if (!IsEnabled() || !AcceptContext(docman, window))
		return FALSE;

	CustomRequest *custom = GetCustomRequest(url_rep);

	if (custom && custom->HasReloadPolicy())
		return (custom->GetReloadPolicy() == RELOADPOLICY_NO_CACHE);

	// Use global policy.
	return (reload_policy == RELOADPOLICY_NO_CACHE);
}

OP_STATUS
OpScopeResourceManager::OnXHRLoad(URL_Rep *url_rep, BOOL cached, DocumentManager *docman, Window *window)
{
	if (!IsEnabled() || !AcceptContext(docman, window))
		return OpStatus::OK;

	RETURN_IF_MEMORY_ERROR(url_rep->SetAttribute(xhr_attr, 1));

	// If this is a cached load, we do not expect any OnUrl* events from the URL, because
	// DOM_LSLoader implements its own caching.
	if (cached)
	{
		RETURN_IF_ERROR(OnUrlLoad(url_rep, docman, window));
		RETURN_IF_ERROR(OnUrlFinished(url_rep, URL_LOAD_FINISHED));
	}

	return OpStatus::OK;
}

OP_STATUS
OpScopeResourceManager::DoGetResource(const GetResourceArg &in, ResourceData &out)
{
	URL_Rep *url_rep;
	RETURN_IF_ERROR(GetResource(in.GetResourceID(), url_rep));

	if (url_rep->GetAttribute(URL::KLoadStatus) != URL_LOADED)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Resource is not loaded"));

	ContentMode *mode;

	if (in.HasContentMode())
		mode = in.GetContentMode();
	else
		RETURN_IF_ERROR(GetResponseMode(url_rep, mode));

	return SetResourceData(out, url_rep, *mode);
}

OP_STATUS
OpScopeResourceManager::DoSetReloadPolicy(const SetReloadPolicyArg &in)
{
	reload_policy = in.GetPolicy();
	return OpStatus::OK;
}

OP_STATUS
OpScopeResourceManager::DoSetResponseMode(const SetResponseModeArg &in)
{
	// Always reset.
	filter_response_default.Reset();
	filter_response_mime.Reset();

	if (in.HasDefaultContentMode())
		filter_response_default.Set(*(in.GetDefaultContentMode()));

	return filter_response_mime.Set(in.GetMimeModeList());
}

OP_STATUS
OpScopeResourceManager::DoSetRequestMode(const SetRequestModeArg &in)
{
	// Always reset.
	filter_request_default.Reset();
	filter_request_mime.Reset();
	filter_request_mime_multipart.Reset();

	if (in.HasDefaultContentMode())
		filter_request_default.Set(*(in.GetDefaultContentMode()));

	RETURN_IF_ERROR(filter_request_mime.Set(in.GetMimeModeList()));
	return filter_request_mime_multipart.Set(in.GetMultipartMimeModeList());
}

OP_STATUS
OpScopeResourceManager::DoAddHeaderOverrides(const AddHeaderOverridesArg &in)
{
	unsigned count = in.GetHeaderList().GetCount();

	for (unsigned i = 0; i < count; ++i)
	{
		Header *header_ptr = in.GetHeaderList().Get(i);
		Header *existing_header;

		// If this override already exists, remove it.
		if (OpStatus::IsSuccess(header_overrides.Remove(header_ptr->GetName().CStr(), &existing_header)))
			OP_DELETE(existing_header);

		OpAutoPtr<Header> header(OP_NEW(Header, ()));
		RETURN_OOM_IF_NULL(header.get());
		RETURN_IF_ERROR(header->SetName(header_ptr->GetName().CStr()));

		if (!header_ptr->GetValue().IsEmpty())
			RETURN_IF_ERROR(header->SetValue(header_ptr->GetValue()));

		RETURN_IF_ERROR(header_overrides.Add(header->GetName().CStr(), header.get()));
		header.release();
	}

	return OpStatus::OK;
}

OP_STATUS
OpScopeResourceManager::DoRemoveHeaderOverrides(const RemoveHeaderOverridesArg &in)
{
	unsigned count = in.GetNameList().GetCount();

	for (unsigned i = 0; i < count; ++i)
	{
		const OpString *str = in.GetNameList().Get(i);

		Header *header_ptr;

		if (OpStatus::IsSuccess(header_overrides.Remove(str->CStr(), &header_ptr)))
		{
			OP_DELETE(header_ptr);
			header_ptr = NULL;
		}
	}

	return OpStatus::OK;
}

OP_STATUS
OpScopeResourceManager::DoClearHeaderOverrides()
{
	header_overrides.DeleteAll();
	return OpStatus::OK;
}

OP_STATUS
OpScopeResourceManager::DoCreateRequest(const CreateRequestArg &in, ResourceID &out)
{
	Window *win = g_windowManager->GetWindow(in.GetWindowID());
	if (!win)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Window not found"));
	URL url = g_url_api->GetURL(in.GetUrl());
	if (!url.IsValid())
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Not a valid URL"));

	// It's currently not possible to request the same URL more than once at the same time.
	if (url.GetAttribute(URL::KLoadStatus) == URL_LOADING)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("URL is already loading"));

	unsigned resource_id;
	RETURN_IF_ERROR(GetResourceID(url.GetRep(), resource_id));
	out.SetResourceID(resource_id);

	URLType scheme = url.Type();
	BOOL is_http = scheme == URL_HTTP || scheme == URL_HTTPS;
	OpString8 method8;
	RETURN_IF_ERROR(method8.SetUTF8FromUTF16(in.GetMethod()));
	RETURN_IF_ERROR(url.SetAttribute(URL::KHTTPSpecialMethodStr, method8));
	URL ref_url;
	OpString8 field;
	CreateRequestArg::HeaderPolicy header_policy = in.GetHeaderPolicy();

	if (in.GetHeaderList().GetCount() && header_policy == CreateRequestArg::HEADERPOLICY_NORMAL)
	{
		const OpProtobufMessageVector<Header> &headers = in.GetHeaderList();
		for (unsigned i = 0; i < headers.GetCount(); ++i)
		{
			Header *header = headers.Get(i);
			// Special handling of certain HTTP headers
			if (is_http)
			{
				if (header->GetName().CompareI(UNI_L("Referer")) == 0)
				{
					ref_url = g_url_api->GetURL(header->GetValue());
					continue;
				}
			}

			URL_Custom_Header header_item;

			RETURN_IF_ERROR(header_item.name.Set(header->GetName()));
			RETURN_IF_ERROR(header_item.value.Set(header->GetValue()));
			RETURN_IF_ERROR(url.SetAttribute(URL::KAddHTTPHeader, &header_item));
		}
	}

	if (in.HasPayload())
	{
		OpAutoPtr<Upload_BinaryBuffer> upload(OP_NEW(Upload_BinaryBuffer, ()));
		RETURN_OOM_IF_NULL(upload.get());
		OpString8 mime_type;
		RETURN_IF_ERROR(mime_type.SetUTF8FromUTF16(in.GetPayload()->GetMimeType()));
		if (in.GetPayload()->HasStringData())
		{
			OpString8 encoded;
			RETURN_IF_ERROR(encoded.SetUTF8FromUTF16(in.GetPayload()->GetStringData()));
			OP_STATUS status;
			TRAP_AND_RETURN(status, upload->InitL(reinterpret_cast<unsigned char *>(encoded.CStr()), encoded.Length(), UPLOAD_COPY_BUFFER, mime_type.CStr(), "utf8"));
			TRAP_AND_RETURN(status, upload->PrepareUploadL(UPLOAD_BINARY_NO_CONVERSION));
			(void)status;
			url.SetHTTP_Data(upload.get(), TRUE);
			upload.release();
		}
		else if (in.GetPayload()->HasByteData())
		{
			OpAutoArray<char> payload(in.GetPayload()->GetByteData().Copy());
			OP_STATUS status;
			TRAP_AND_RETURN(status, upload->InitL(reinterpret_cast<unsigned char *>(payload.get()), in.GetPayload()->GetByteData().Length(), UPLOAD_COPY_BUFFER, mime_type.CStr()));
			TRAP_AND_RETURN(status, upload->PrepareUploadL(UPLOAD_BINARY_NO_CONVERSION));
			(void)status;
			url.SetHTTP_Data(upload.get(), TRUE);
			upload.release();
		}
		else
		{
			return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("No payload was set, need either text or binary field filled in"));
		}
	}

	URL_LoadPolicy policy;
	OP_ASSERT(win->DocManager() && win->DocManager()->GetMessageHandler());
	MessageHandler *mh = win->DocManager()->GetMessageHandler();

	OpAutoPtr<CustomRequest> req(OP_NEW(CustomRequest, (url, header_policy)));
	RETURN_OOM_IF_NULL(req.get());

	if (in.HasReloadPolicy())
		req->SetReloadPolicy(in.GetReloadPolicy());
	if (in.HasRequestContentMode())
		req->SetRequestContentMode(*in.GetRequestContentMode());
	if (in.HasResponseContentMode())
		req->SetResponseContentMode(*in.GetResponseContentMode());

	if (header_policy == CreateRequestArg::HEADERPOLICY_OVERWRITE || header_policy == CreateRequestArg::HEADERPOLICY_REPLACE)
	{
		const OpProtobufMessageVector<Header> &headers = in.GetHeaderList();
		for (unsigned i = 0; i < headers.GetCount(); ++i)
		{
			Header *orig_header = headers.Get(i);
			OpAutoPtr<Header> header(OP_NEW(Header, ()));
			RETURN_OOM_IF_NULL(header.get());
			RETURN_IF_ERROR(header->SetName(orig_header->GetName()));
			RETURN_IF_ERROR(header->SetValue(orig_header->GetValue()));
			RETURN_IF_ERROR(req->GetHeaders().Add(header.get()));
			header.release();
		}
	}

	// If there already is a custom request for this URL, allow this request
	// to overwrite it. (If there is no custom request, the call will have
	// no effect).
	//
	// Note that it is still not valid to create a custom request for a URL
	// that is already loading, but this situation is dealt with earlier in
	// the function, so this should not be the case.
	RemoveCustomRequest(url.GetRep());

	RETURN_IF_ERROR(custom_requests.Add(url.GetRep(), req.get()));
	req.release();

	if (url.LoadDocument(mh, ref_url, policy) == COMM_REQUEST_FAILED)
	{
		RemoveCustomRequest(url.GetRep());
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Failed to load URL"));
	}

	return OpStatus::OK;
}

OP_STATUS
OpScopeResourceManager::DoClearCache()
{
	g_url_api->PurgeData(TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE);
	return OpStatus::OK;
}

OP_STATUS
OpScopeResourceManager::DoGetResourceID(const GetResourceIDArg &in, ResourceID &out)
{
	URL url = g_url_api->GetURL(in.GetUrl());

	if (!url.IsValid())
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Invalid URL"));

	URL_Rep *rep = url.GetRep();

	if (!rep || rep->GetUsedCount() == 0)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("URL not in use"));

	unsigned resource_id;
	RETURN_IF_ERROR(GetResourceID(rep, resource_id));

	out.SetResourceID(resource_id);

	return OpStatus::OK;
}

/** Private classes */

/* OpScopeResourceManager::HeaderManager */

OpScopeResourceManager::HeaderManager::HeaderManager(Upload_EncapsulatedElement *upload)
	: upload(upload)
	, element(upload->GetElement())
	, current_base(NULL)
	, current_item(NULL)
{
	OP_ASSERT(upload);
}

Header_Item *OpScopeResourceManager::HeaderManager::First()
{
	current_base = upload;
	current_item = upload->Headers.First();

	// If outer list is empty, try inner.
	if (!current_item && element)
	{
		current_item = element->Headers.First();
		current_base = element;
	}

	return current_item;
}

Header_Item *OpScopeResourceManager::HeaderManager::Next()
{
	current_item = current_item->Suc();

	// If we're done with outer list, proceed to inner.
	if (!current_item && element && current_base != element)
	{
		current_item = element->Headers.First();
		current_base = element;
	}

	return current_item;
}

OP_STATUS
OpScopeResourceManager::HeaderManager::Override(const OpString8 &name, const OpString8 &value)
{
	// If value is \0, remove all instances of named header.
	if (value.Length() == 0)
	{
		upload->Headers.RemoveHeader(name);
		if (element)
			element->Headers.RemoveHeader(name);
		return OpStatus::OK;
	}

	Header_Item * OP_MEMORY_VAR item = upload->Headers.FindHeader(name, TRUE);

	// If we couldn't find it in outer headers, try inner.
	if (!item && element)
		item = element->Headers.FindHeader(name, TRUE);

	// Still nothing? Insert into outer list.
	if(item == NULL)
		TRAP_AND_RETURN(err, item = upload->Headers.InsertHeaderL(name));

	item->ClearParameters();

	TRAP_AND_RETURN(err, item->AddParameterL(value));

	return OpStatus::OK;
}

void OpScopeResourceManager::HeaderManager::RemoveAll()
{
	Header_Item *item[2]; // ARRAY OK 2010-07-14 andersr
	item[0] = upload->Headers.First();
	item[1] = element ? element->Headers.First() : NULL;

	for (unsigned i = 0; i < 2; ++i)
	{
		while (item[i])
		{
			Header_Item *cur = item[i];
			item[i] = item[i]->Suc();

			if (cur->GetName().Length() > 0)
				OP_DELETE(cur);
		}
	}
}

OpScopeResourceManager::ResponseContentReader::ResponseContentReader(URL_Rep *url_rep, const ContentMode &mode)
	: url_rep(url_rep)
	, mode(mode)
{
}

OP_STATUS
OpScopeResourceManager::ResponseContentReader::Read(ByteBuffer &buf, OpString8 &charset, const uni_char *&err)
{
	BOOL raw = (mode.GetTransport() != OpScopeResourceManager::ContentMode::TRANSPORT_STRING);
	OpAutoPtr<URL_DataDescriptor> dd(url_rep->GetDescriptor(NULL, URL::KNoRedirect, raw, mode.GetDecode(), NULL, URL_TEXT_CONTENT));

	if (!dd.get())
	{
		err = UNI_L("Resource is not loaded");
		return OpStatus::ERR;
	}

	BOOL more = FALSE;
	unsigned ret = 0;

	do
	{
		ret = dd->RetrieveData(more);
		RETURN_IF_ERROR(buf.AppendBytes(dd->GetBuffer(), ret));
		dd->ConsumeData(ret);
	} while(more && ret != 0);

	// If there is a decoder, then it's the character converter to utf-16.
	if (dd->GetFirstDecoder())
		return charset.Set("utf-16");

	return OpStatus::OK;
}

OpScopeResourceManager::RequestContentReader::RequestContentReader(Upload_Base *element, char *net_buf, unsigned net_buf_len)
	: element(element)
	, net_buf(net_buf)
	, net_buf_len(net_buf_len)
{
}

OP_STATUS
OpScopeResourceManager::RequestContentReader::Read(ByteBuffer &buf, OpString8 &charset, const uni_char *&err)
{
	if (!element)
	{
		err = UNI_L("No element to upload");
		return OpStatus::ERR;
	}

	element->ResetContent();

	BOOL more = TRUE;

	while(more)
	{
		unsigned ret = element->GetNextContentBlock(reinterpret_cast<unsigned char *>(net_buf), net_buf_len, more);
		RETURN_IF_ERROR(buf.AppendBytes(net_buf, ret));
	}

	return OpStatus::OK;
}

OpScopeResourceManager::ContentFilter::ContentFilter(ContentFilter *parent)
	: parent(parent)
{
}

OP_STATUS
OpScopeResourceManager::ContentFilter::GetContentMode(const char *mime, ContentMode *&mode)
{
	mode = GetContentMode(mime);

	if (mode)
		return OpStatus::OK;
	else if (parent)
		return parent->GetContentMode(mime, mode);

	return OpStatus::ERR;
}

OpScopeResourceManager::DefaultContentFilter::DefaultContentFilter()
	: ContentFilter(NULL)
{
}

/* virtual */ void
OpScopeResourceManager::DefaultContentFilter::Reset()
{
	defaultMode = ContentMode();
}

/* virtual */ void
OpScopeResourceManager::DefaultContentFilter::Set(const ContentMode &mode)
{
	defaultMode = mode;
}


/* virtual */ OpScopeResourceManager::ContentMode *
OpScopeResourceManager::DefaultContentFilter::GetContentMode(const char * /* mime */)
{
	return &defaultMode;
}

OpScopeResourceManager::MimeContentFilter::MimeContentFilter(ContentFilter *parent)
	: ContentFilter(parent)
{
}

OP_STATUS
OpScopeResourceManager::MimeContentFilter::Add(const char *mime, const ContentMode &mode)
{
	OpAutoPtr<Item> item(OP_NEW(Item, ()));
	RETURN_OOM_IF_NULL(item.get());
	
	item->mode = mode;
	RETURN_IF_ERROR(item->key.Set(mime));

	RETURN_IF_ERROR(modes.Add(item->key.CStr(), item.get()));
	item.release();

	return OpStatus::OK;
}

void
OpScopeResourceManager::MimeContentFilter::Reset()
{
	modes.DeleteAll();
}

OP_STATUS
OpScopeResourceManager::MimeContentFilter::Set(const OpProtobufMessageVector<MimeMode> &mimeList)
{
	unsigned size = mimeList.GetCount();

	for (unsigned i = 0; i < size; ++i)
	{
		const MimeMode *mm = mimeList.Get(i);

		OpString8 type;
		RETURN_IF_ERROR(type.SetUTF8FromUTF16(mm->GetType()));

		RETURN_IF_ERROR(Add(type.CStr(), mm->GetContentMode()));
	}

	return OpStatus::OK;
}

/* virtual */ OpScopeResourceManager::ContentMode *
OpScopeResourceManager::MimeContentFilter::GetContentMode(const char *mime)
{
	Item *item;
	if (OpStatus::IsSuccess(modes.GetData(mime, &item)))
		return &(item->mode);
	return NULL;
}

OpScopeResourceManager::ResourceContext::ResourceContext(OpScopeResourceManager *rm)
	: resource_manager(rm)
	, docman(NULL)
	, window(NULL)
	, frame_id(0)
	, window_id(0)
{
}

OP_STATUS
OpScopeResourceManager::ResourceContext::Construct(DocumentManager *docman, Window *window)
{
	this->docman = docman;
	this->window = window;

	if (!window && docman)
		this->window = docman->GetWindow();

	// Allow missing docman, but do not allow missing window.
	if (!window)
		return OpStatus::ERR;

	if (window)
		window_id = window->Id();
	if (docman)
		RETURN_IF_ERROR(resource_manager->GetFrameID(docman, frame_id));

	return OpStatus::OK;
}

unsigned
OpScopeResourceManager::ResourceContext::GetFrameID() const
{
	return frame_id;
}

unsigned
OpScopeResourceManager::ResourceContext::GetWindowID() const
{
	return window_id;
}

DocumentManager *
OpScopeResourceManager::ResourceContext::GetDocumentManager()
{
	if (frame_id == 0)
		return NULL;

	Window *w = GetWindow();

	if (!w)
		return NULL;

	DocumentTreeIterator iter(w);
	iter.SetIncludeThis();
	iter.SetIncludeEmpty();

	while (iter.Next())
	{
		if (iter.GetDocumentManager() == docman)
			return docman;
	}

	return NULL;
}

Window *
OpScopeResourceManager::ResourceContext::GetWindow()
{
	if (window_id == 0)
		return NULL;

	return g_windowManager->GetWindow(window_id);
}

/* OpScopeResourceManager::CustomRequest */

OpScopeResourceManager::CustomRequest::CustomRequest(URL url, CreateRequestArg::HeaderPolicy header_policy)
	: header_policy(header_policy)
	, has_reload_policy(FALSE)
	, has_request_content_mode(FALSE)
	, has_response_content_mode(FALSE)
	, url(url)
{}

void
OpScopeResourceManager::CustomRequest::SetReloadPolicy(ReloadPolicy reload_policy)
{
	has_reload_policy = TRUE;
	this->reload_policy = reload_policy;
}

void
OpScopeResourceManager::CustomRequest::SetRequestContentMode(ContentMode content_mode)
{
	has_request_content_mode = TRUE;
	this->request_content_mode = content_mode;
}

void
OpScopeResourceManager::CustomRequest::SetResponseContentMode(ContentMode content_mode)
{
	has_response_content_mode = TRUE;
	this->response_content_mode = content_mode;
}

OpScopeResourceManager::CreateRequestArg::HeaderPolicy
OpScopeResourceManager::CustomRequest::GetHeaderPolicy() const
{
	return header_policy;
}

OpScopeResourceManager::ReloadPolicy
OpScopeResourceManager::CustomRequest::GetReloadPolicy() const
{
	return reload_policy;
}

OpScopeResourceManager::ContentMode *
OpScopeResourceManager::CustomRequest::GetRequestContentMode()
{
	return (has_request_content_mode ? &request_content_mode : NULL);
}

OpScopeResourceManager::ContentMode *
OpScopeResourceManager::CustomRequest::GetResponseContentMode()
{
	return (has_response_content_mode ? &response_content_mode : NULL);
}

OpVector<OpScopeResourceManager::Header> &
OpScopeResourceManager::CustomRequest::GetHeaders()
{
	return headers;
}

BOOL
OpScopeResourceManager::CustomRequest::HasReloadPolicy() const
{
	return has_reload_policy;
}

OpScopeResourceManager::ResourceRemover::ResourceRemover(OpScopeResourceManager *resource_manager, URL_Rep *url_rep)
	: resource_manager(resource_manager)
	, url_rep(url_rep)
{
}

OpScopeResourceManager::ResourceRemover::~ResourceRemover()
{
	OP_ASSERT(resource_manager);
	OP_ASSERT(url_rep);

	resource_manager->RemoveCustomRequest(url_rep);
	resource_manager->RemoveResourceContext(url_rep);
	resource_manager->RemoveRedirectedResource(url_rep);
}

/* Private functions */

void
OpScopeResourceManager::RemoveTrailingChar(char *str, char c)
{
	unsigned len = op_strlen(str);

	if (len > 0 && str[len-1] == c)
		str[len-1] = '\0';
}

OP_STATUS
OpScopeResourceManager::SetMethod(const URL_Rep *url_rep, OpString &str)
{
	switch(url_rep->GetAttribute(URL::KHTTP_Method))
	{
	case HTTP_METHOD_GET:
		return str.Set("GET");
	case HTTP_METHOD_POST:
		return str.Set("POST");
	case HTTP_METHOD_HEAD:
		return str.Set("HEAD");
	case HTTP_METHOD_CONNECT:
		return str.Set("CONNECT");
	case HTTP_METHOD_PUT:
		return str.Set("PUT");
	case HTTP_METHOD_OPTIONS:
		return str.Set("OPTIONS");
	case HTTP_METHOD_DELETE:
		return str.Set("DELETE");
	case HTTP_METHOD_TRACE:
		return str.Set("TRACE");
	case HTTP_METHOD_SEARCH:
		return str.Set("SEARCH");
	case HTTP_METHOD_String:
		return url_rep->GetAttribute(URL::KHTTPSpecialMethodStr, str);
	case HTTP_METHOD_Invalid:
		return str.Set("");
	default:
		OP_ASSERT(!"Please add a handler for this method");
		return str.Set("UNKNOWN");
	}
}

OP_STATUS
OpScopeResourceManager::GetResponseMode(URL_Rep *url_rep, ContentMode *&mode)
{
	CustomRequest *custom = GetCustomRequest(url_rep);

	mode = (custom ? custom->GetResponseContentMode() : NULL);

	if (mode)
		return OpStatus::OK;

	OpString8 str;
	RETURN_IF_ERROR(url_rep->GetAttribute(URL::KMIME_Type, str));
	return GetContentMode(filter_response_mime, str.CStr(), mode);
}

OP_STATUS
OpScopeResourceManager::GetRequestMode(URL_Rep *url_rep, Upload_Base *element, ContentMode *&mode)
{
	CustomRequest *custom = GetCustomRequest(url_rep);

	mode = (custom ? custom->GetRequestContentMode() : NULL);

	if (mode)
		return OpStatus::OK;

	return GetContentMode(filter_request_mime, element->GetContentType().CStr(), mode);
}

OP_STATUS
OpScopeResourceManager::GetMultipartRequestMode(Upload_Base *element, ContentMode *&mode)
{
	return GetContentMode(filter_request_mime_multipart, element->GetContentType().CStr(), mode);
}

OP_STATUS
OpScopeResourceManager::GetContentMode(ContentFilter &f, const char *mime, ContentMode *&mode)
{
	// Treat NULL as empty mime type.
	return f.GetContentMode(mime ? mime : "", mode);
}

OP_STATUS
OpScopeResourceManager::SetResourceData(ResourceData &msg, URL_Rep *url_rep, const ContentMode &mode)
{
	unsigned resource_id;
	RETURN_IF_ERROR(GetResourceID(url_rep, resource_id));
	msg.SetResourceID(resource_id);

	RETURN_IF_ERROR(url_rep->GetAttribute(URL::KUniName_Username_Password_Hidden, 0 /* charsetID */ , msg.GetUrlRef()));

	OpString mime, charset;
	RETURN_IF_ERROR(url_rep->GetAttribute(URL::KMIME_Type, msg.GetMimeTypeRef()));
	RETURN_IF_ERROR(url_rep->GetAttribute(URL::KMIME_CharSet,  msg.GetCharacterEncodingRef()));

	OpFileLength content_len = 0;
	url_rep->GetAttribute(URL::KContentSize, &content_len);
	msg.SetContentLength(static_cast<UINT32>(content_len));

	BOOL too_big = (static_cast<unsigned>(content_len) > mode.GetSizeLimit());

	if (mode.GetTransport() == ContentMode::TRANSPORT_OFF || too_big)
		return OpStatus::OK; // Don't include Content.

	OpAutoPtr<Content> content(OP_NEW(Content, ()));
	RETURN_OOM_IF_NULL(content.get());

	OpString8 characterEncoding8, mime8;
	RETURN_IF_ERROR(url_rep->GetAttribute(URL::KMIME_CharSet, characterEncoding8));
	RETURN_IF_ERROR(url_rep->GetAttribute(URL::KMIME_Type, mime8));

	ResponseContentReader reader(url_rep, mode);

	RETURN_IF_ERROR(SetContent(*(content.get()), url_rep, reader, mode, characterEncoding8, mime8));
	msg.SetContent(content.release());

	return OpStatus::OK;
}


OP_STATUS
OpScopeResourceManager::SetContent(Content &msg, URL_Rep *url_rep, ContentReader &reader, const ContentMode &mode, const OpStringC8 &encoding, const OpStringC8 &mime)
{
	if (mode.GetTransport() == ContentMode::TRANSPORT_OFF)
		return OpStatus::OK;

	ByteBuffer tmp;

	// When the content mode is TRANSPORT_BYTES, the data is read directly into
	// the ResourceData's ByteBuffer as an optimization.
	ByteBuffer &buf = (mode.GetTransport() == ContentMode::TRANSPORT_BYTES) ? msg.GetByteDataRef() : tmp;

	OpString &str = msg.GetStringDataRef();

	const uni_char *err = NULL;
	OpString8 charset;

	// Read all data into a ByteBuffer, even when the transport mode is string
	// based. OpString's Append does a lot of strlen, which we don't want.
	if (OpStatus::IsError(reader.Read(buf, charset, err)))
		return SetCommandError(OpScopeTPHeader::InternalError, err);

	// Use byte length as default content.length.
	msg.SetLength(buf.Length());

	if (mode.GetTransport() == ContentMode::TRANSPORT_STRING)
	{
		// Figure out if 'buf' contains the native OpString encoding. If so, we
		// can avoid further character conversion by doing a TakeOver, but that
		// means we must also add the terminating zero manually.
		BOOL native = (charset.CompareI("utf-16") == 0);

		// If native, then +sizeof(uni_char) for terminating zero.
		unsigned len = (native ? buf.Length() + sizeof(uni_char) : buf.Length());

		OpAutoArray<char> copy(OP_NEWA(char, len));
		RETURN_OOM_IF_NULL(copy.get());
		buf.Extract(0, buf.Length(), copy.get());

		if (native)
		{
			uni_char *native_str = reinterpret_cast<uni_char*>(copy.release());
			native_str[(len/sizeof(uni_char))-1] = '\0'; // Add terminating zero.
			str.TakeOver(native_str);
		}
		else if (!charset.IsEmpty())
			RETURN_IF_ERROR(SetFromEncoding(&str, charset.CStr(), copy.get(), buf.Length()));
		else // Assume UTF-8
			RETURN_IF_ERROR(str.SetFromUTF8(copy.get(), buf.Length()));

		// Use string length instead of byte length.
		msg.SetLength(str.Length());

		RETURN_IF_ERROR(url_rep->GetAttribute(URL::KMIME_CharSet, msg.GetCharacterEncodingRef()));
	}
	else if (mode.GetTransport() == ContentMode::TRANSPORT_DATA_URI)
	{
		OpAutoArray<char> copy(buf.Copy());
		RETURN_OOM_IF_NULL(copy.get());

		char *target = NULL;
		int targetlen = 0;
		MIME_Encode_Error err = MIME_Encode_SetStr(target, targetlen, copy.get(), buf.Length(), "UTF-8", GEN_BASE64_ONELINE);
		OpAutoArray<char> target_anchor(target);
		if (err != MIME_NO_ERROR)
			return OpStatus::ERR;

		RETURN_IF_ERROR(str.Append("data:"));
		RETURN_IF_ERROR(str.Append(mime));
		RETURN_IF_ERROR(str.Append(";base64"));
		RETURN_IF_ERROR(str.Append(","));
		RETURN_IF_ERROR(str.Append(target));
	}
	// If mode.GetTransport() == ContentMode::TRANSPORT_BYTES, then ResourceData
	// already contains the bytes we want.

	return OpStatus::OK;
}

OP_STATUS
OpScopeResourceManager::SetHeader(Header &msg, Header_Item *item, OpAutoArray<char> &hbuf, unsigned &hbuf_len)
{
	unsigned length = item->CalculateLength(); // \0 not included

	// Guarantee a big enough buffer.
	if (length >= hbuf_len)
	{
		hbuf_len = 0;
		hbuf.reset(OP_NEWA(char, length+1)); // +1 for \0
		RETURN_OOM_IF_NULL(hbuf.get());
		hbuf_len = length+1;
	}

	item->GetValue(hbuf.get());

	// Header_Itme::GetValue may append \r\n before the \0 (even if you
	// request that it shouldn't).
	RemoveTrailingChar(hbuf.get(), '\n');
	RemoveTrailingChar(hbuf.get(), '\r');

	RETURN_IF_ERROR(msg.SetName(item->GetName().CStr()));
	RETURN_IF_ERROR(msg.SetValue(hbuf.get()));

	return OpStatus::OK;
}

OP_STATUS
OpScopeResourceManager::SetUrlFinished(UrlFinished &msg, unsigned resource_id, URLLoadResult result)
{
	msg.SetResourceID(resource_id);
	UrlFinished::Result finish_result;
	switch (result)
	{
	case URL_LOAD_FINISHED:
		finish_result = UrlFinished::RESULT_FINISHED; break;
	case URL_LOAD_STOPPED:
		finish_result = UrlFinished::RESULT_STOPPED; break;
	default:
		OP_ASSERT(!"Unknown url load result, add new case entry");
	case URL_LOAD_FAILED:
		finish_result = UrlFinished::RESULT_FAILED; break;
	}
	msg.SetResult(finish_result);
	msg.SetTime(g_op_time_info->GetTimeUTC());
	return OpStatus::OK;
}

BOOL
OpScopeResourceManager::AcceptResource(URL_Rep *url_rep)
{
	OP_ASSERT(url_rep);
	unsigned *url_id_ptr = reinterpret_cast<unsigned*>(static_cast<UINTPTR>((url_rep->GetID())));
	return active_resources.Contains(url_id_ptr);
}

BOOL
OpScopeResourceManager::AcceptContext(DocumentManager *docman, Window *window)
{
	if (docman && !window)
		window = docman->GetWindow();
	return window && AcceptWindow(window);
}

BOOL
OpScopeResourceManager::AcceptWindow(Window *window)
{
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	return window_manager && window_manager->AcceptWindow(window);
#else // SCOPE_WINDOW_MANAGER_SUPPORT
	return TRUE;
#endif // SCOPE_WINDOW_MANAGER_SUPPORT

}

OP_STATUS
OpScopeResourceManager::GetDocumentID(FramesDocument *frm_doc, unsigned &id)
{
	return document_ids->GetID(frm_doc, id);
}

OP_STATUS
OpScopeResourceManager::GetFrameID(DocumentManager *docman, unsigned &id)
{
	return frame_ids->GetID(docman, id);
}

OP_STATUS
OpScopeResourceManager::GetResourceID(URL_Rep *url_rep, unsigned &id)
{
	// Let's store the value in the pointer itself.
	return resource_ids->GetID(reinterpret_cast<unsigned*>(static_cast<UINTPTR>(url_rep->GetID())), id);
}

OP_STATUS
OpScopeResourceManager::GetResource(unsigned id, URL_Rep *&url_rep)
{
	unsigned *url_id_ptr;

	if (OpStatus::IsError(resource_ids->GetObject(id, url_id_ptr)))
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Resource does not exist"));

	url_rep = urlManager->LocateURL(reinterpret_cast<URL_ID>(url_id_ptr));

	if (!url_rep)
		return SetCommandError(OpScopeTPHeader::InternalError, UNI_L("Resource no longer exists"));

	return OpStatus::OK;
}

OP_STATUS
OpScopeResourceManager::AddResourceContext(URL_Rep *url_rep, DocumentManager *docman, Window *window)
{
	OP_ASSERT(url_rep);
	unsigned *url_id_ptr = reinterpret_cast<unsigned *>(static_cast<UINTPTR>(url_rep->GetID()));

	OpAutoPtr<ResourceContext> context(OP_NEW(ResourceContext, (this)));
	RETURN_OOM_IF_NULL(context.get());
	RETURN_IF_ERROR(context->Construct(docman, window));
	RETURN_IF_ERROR(active_resources.Add(url_id_ptr, context.get()));
	context.release();

	return OpStatus::OK;
}

void
OpScopeResourceManager::GetResourceContext(URL_Rep *url_rep, ResourceContext *&context)
{
	OP_ASSERT(url_rep);
	unsigned *url_id_ptr = reinterpret_cast<unsigned*>(static_cast<UINTPTR>(url_rep->GetID()));
	OpStatus::Ignore(active_resources.GetData(url_id_ptr, &context));
}

void
OpScopeResourceManager::RemoveResourceContext(URL_Rep *url_rep)
{
	OP_ASSERT(url_rep);
	unsigned *url_id_ptr = reinterpret_cast<unsigned*>(static_cast<UINTPTR>((url_rep->GetID())));
	ResourceContext *context = NULL;
	if (OpStatus::IsSuccess(active_resources.Remove(url_id_ptr, &context)))
	{
		OP_ASSERT(context);
		OP_DELETE(context);
	}
	int idremoved;
	OpStatus::Ignore(http_requests.Remove(url_rep, &idremoved));
}

int
OpScopeResourceManager::GetHTTPRequestID(URL_Rep *url_rep)
{
	int request_id = -1;
	OpStatus::Ignore(http_requests.GetData(url_rep, &request_id));
	return request_id;
}

OpScopeResourceManager::CustomRequest *
OpScopeResourceManager::GetCustomRequest(URL_Rep *url_rep)
{
	CustomRequest *custom = NULL;
	if (OpStatus::IsSuccess(custom_requests.GetData(url_rep, &custom)))
		return custom;

	return NULL;
}

void
OpScopeResourceManager::RemoveCustomRequest(URL_Rep *url_rep)
{
	CustomRequest *custom = NULL;
	RETURN_VOID_IF_ERROR(custom_requests.Remove(url_rep, &custom));
	OP_DELETE(custom);
}

void
OpScopeResourceManager::RemoveRedirectedResource(URL_Rep *url_rep)
{
	OpStatus::Ignore(redirected_resources.Remove(url_rep));
}

OP_STATUS
OpScopeResourceManager::OnDNSStarted(URL_Rep *url_rep, double prefetchtimespent)
{
	OP_ASSERT(url_rep);
	if (!IsEnabled() || !AcceptResource(url_rep))
		return OpStatus::OK;

	unsigned resource_id;
	DnsLookupStarted dnslookup;
	RETURN_IF_ERROR(GetResourceID(url_rep, resource_id));
	dnslookup.SetResourceID(resource_id);

	dnslookup.SetTime(g_op_time_info->GetTimeUTC() - prefetchtimespent);	//Wind back to when started if prefetched.
	return SendOnDnsLookupStarted(dnslookup);
}

/*static*/ OP_STATUS
OpScopeResourceManager::Convert(OpHostResolver::Error status_in, DnsLookupFinished::Status &status_out)
{
	switch(status_in)
	{
	case OpHostResolver::NETWORK_NO_ERROR:
		status_out = DnsLookupFinished::STATUS_NETWORK_NO_ERROR;
		break;
	case OpHostResolver::ERROR_HANDLED:
		status_out = DnsLookupFinished::STATUS_ERROR_HANDLED;
		break;
	case OpHostResolver::HOST_ADDRESS_NOT_FOUND:
		status_out = DnsLookupFinished::STATUS_HOST_ADDRESS_NOT_FOUND;
		break;
	case OpHostResolver::TIMED_OUT:
		status_out = DnsLookupFinished::STATUS_TIMED_OUT;
		break;
	case OpHostResolver::NETWORK_ERROR:
		status_out = DnsLookupFinished::STATUS_NETWORK_ERROR;
		break;
	case OpHostResolver::INTERNET_CONNECTION_CLOSED:
		status_out = DnsLookupFinished::STATUS_INTERNET_CONNECTION_CLOSED;
		break;
	case OpHostResolver::OUT_OF_COVERAGE:
		status_out = DnsLookupFinished::STATUS_OUT_OF_COVERAGE;
		break;
	case OpHostResolver::OUT_OF_MEMORY:
		status_out = DnsLookupFinished::STATUS_OUT_OF_MEMORY;
		break;
	default:
		OP_ASSERT(!"Explicitly ignore unknown error or convert to a proper error!");
		return OpStatus::ERR_NOT_SUPPORTED;
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeResourceManager::OnDNSEnded(URL_Rep *url_rep, OpHostResolver::Error statuscode)
{
	OP_ASSERT(url_rep);
	if (!IsEnabled() || !AcceptResource(url_rep))
		return OpStatus::OK;

	DnsLookupFinished dnsfinished;
	unsigned resource_id;
	RETURN_IF_ERROR(GetResourceID(url_rep, resource_id));

	dnsfinished.SetResourceID(resource_id);

	DnsLookupFinished::Status status;
	RETURN_IF_ERROR(Convert(statuscode, status));

	dnsfinished.SetStatus(status);
	dnsfinished.SetTime(g_op_time_info->GetTimeUTC());

	return SendOnDnsLookupFinished(dnsfinished);
}

#endif // SCOPE_RESOURCE_MANAGER
