/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */
#include "core/pch.h"

#ifdef DOM3_LOAD

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domload/lscontenthandler.h"
#include "modules/dom/src/domload/lsloader.h"
#include "modules/dom/src/domload/lsparser.h"
#include "modules/dom/src/opera/domhttp.h"

#include "modules/hardcore/mh/mh.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/doc/frm_doc.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/logdoc_constants.h" // defines SplitTextLen
#include "modules/logdoc/namespace.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/url/protocols/comm.h"

#ifdef DOM_HTTP_SUPPORT
# include "modules/viewers/viewers.h"
# include "modules/encodings/detector/charsetdetector.h"
# include "modules/encodings/utility/charsetnames.h"
#endif

#ifdef DOM_WEBWORKERS_SUPPORT
#include "modules/dom/src/domwebworkers/domwebworkers.h"
#endif // DOM_WEBWORKERS_SUPPORT
#ifdef SCOPE_RESOURCE_MANAGER
# include "modules/scope/scope_network_listener.h"
#endif

OP_STATUS
DOM_LSLoader::AddElement(HTML_Element *element)
{
	return SetElement(element);
}

OP_STATUS
DOM_LSLoader::EndElement()
{
	return SetElement(NULL);
}

BOOL
DOM_LSLoader::ValidXHRXMLType(URLContentType content_type, URL *url)
{
	/* W3C Specifications:
		- If the response entity body is null terminate these steps and return null.
		- If final MIME type is not null, text/xml, application/xml, and does not end in +xml terminate these steps and return null.
		- Let document be a cookie-free Document object that represents the result of parsing the response entity body into a document tree following the rules from the XML specifications. If this fails (unsupported character encoding, namespace well-formedness error et cetera) terminate these steps return null. [XML]
	*/

	// "text/xml" and no MIME type (if no sniffing is performed) will populate responseXML
	if (content_type == URL_XML_CONTENT || content_type == URL_UNDETERMINED_CONTENT)
		return TRUE;

	if (url)
	{
		OpString8 mime_type;

		if (OpStatus::IsSuccess(url->GetAttribute(URL::KMIME_Type, mime_type, URL::KFollowRedirect)))
		{
			int mime_type_len = mime_type.Length();

			// Also no MIME type (pre-sniff value), "application/xml" and any MIME type ending with "+xml"
			// will populate responseXML
			if (url->GetAttribute(URL::KMIME_Type_Was_NULL, URL::KFollowRedirect) ||
				(mime_type_len >= 4 &&
				  (op_strcmp(mime_type.CStr() + (mime_type_len - 4), "+xml") == 0 ||
				   op_strcmp(mime_type.CStr(), "application/xml") == 0
				  )
				))
				return TRUE;
		}
	}

	return FALSE;
}

void
DOM_LSLoader::LoadData()
{
	const unsigned char *buffer;
	int buffer_bytes;
	BOOL more, more_from_url = FALSE, from_dd = FALSE;

#ifndef DOM_WEBWORKERS_SUPPORT
	if (!parser->GetFramesDocument())
		Abort();
#endif // !DOM_WEBWORKERS_SUPPORT

#ifdef DOM_HTTP_SUPPORT
	DOM_XMLHttpRequest *xmlhttprequest = parser->GetXMLHttpRequest();
#endif // DOM_HTTP_SUPPORT

	if (stopped || finished)
		return;
	else if (string)
	{
		buffer = reinterpret_cast<const unsigned char *>(string);
		if (!string_consumed)
		{
			buffer_bytes = UNICODE_SIZE(uni_strlen(string));
			string_consumed = TRUE;
		}
		else
			buffer_bytes = 0;
		more = FALSE;
	}
	else
	{
#ifdef DOM_HTTP_SUPPORT
		if (xmlhttprequest && OpStatus::IsMemoryError(xmlhttprequest->SetStatus(url)))
		{
			HandleOOM();
			return;
		}
#endif // DOM_HTTP_SUPPORT

		if (url.GetAttribute(URL::KIsDirectoryListing, TRUE))
		{
			buffer = reinterpret_cast<const unsigned char *>("");
			buffer_bytes = 0;
			more = FALSE;
		}
		else
		{
			if (!url_dd)
			{
				unsigned short int preferred_charset = 0;
				if (OpStatus::IsMemoryError(g_charsetManager->GetCharsetID("utf-8", &preferred_charset)))
				{
					HandleOOM();
					return;
				}
				g_charsetManager->IncrementCharsetIDReference(preferred_charset);

#ifdef DOM_HTTP_SUPPORT
				// Check if overrideMimeType was called on the XMLHttpRequest
				// object.
				unsigned short int requested_charset = 0;
				URLContentType requested_type = URL_XML_CONTENT;
				if (xmlhttprequest)
				{
					const char *forced_content_type = xmlhttprequest->GetForcedResponseType();

					if (xmlhttprequest->IsExpectingBinaryResponse())
					{
						requested_type = URL_UNKNOWN_CONTENT;
						no_parsing = TRUE;
					}
					else if (forced_content_type)
					{
						char tmp_charset_mimetype[64]; // ARRAY OK 2010-02-24 peter

						// Look up MIME type
						const char *p = op_strchr(forced_content_type, ';');
						if (!p)
							p = forced_content_type + op_strlen(forced_content_type);
						while (p > forced_content_type && ' ' == *(p - 1))
							-- p;
						op_strlcpy(tmp_charset_mimetype, forced_content_type, MIN(static_cast<size_t>(p - forced_content_type + 1), ARRAY_SIZE(tmp_charset_mimetype)));
						if (Viewer *v = g_viewers->FindViewerByMimeType(tmp_charset_mimetype))
						{
							requested_type = v->GetContentType();
							if (requested_type == URL_UNKNOWN_CONTENT)
								requested_type = URL_UNDETERMINED_CONTENT;
						}

						// Look up charset, if specified
						unsigned long int length;
						const char *encoding = CharsetDetector::EncodingFromContentType(forced_content_type, &length);
						if (length > 0)
						{
							op_strlcpy(tmp_charset_mimetype, encoding, MIN(length + 1, ARRAY_SIZE(tmp_charset_mimetype)));
							if (OpStatus::IsMemoryError(g_charsetManager->GetCharsetID(tmp_charset_mimetype, &requested_charset)))
							{
								g_charsetManager->DecrementCharsetIDReference(preferred_charset);
								HandleOOM();
								return;
							}
							g_charsetManager->IncrementCharsetIDReference(requested_charset);

						}
						if (!ValidXHRXMLType(requested_type, NULL))
							no_parsing = TRUE;
					}
					else
					{
						URLContentType content_type = static_cast<URLContentType>(url.GetAttribute(URL::KContentType, URL::KFollowRedirect));
						if (!no_parsing && !ValidXHRXMLType(content_type, &url))
							no_parsing = TRUE;
					}
				}
#else
				const URLContentType requested_type = URL_XML_CONTENT;
				const unsigned short int requested_charset = 0;
#endif // DOM_HTTP_SUPPORT

				url_dd = url.GetDescriptor(mh, TRUE, FALSE, TRUE, NULL, requested_type, requested_charset, FALSE, preferred_charset);
				g_charsetManager->DecrementCharsetIDReference(requested_charset);
				g_charsetManager->DecrementCharsetIDReference(preferred_charset);

				if (!url_dd)
				{
					if (url.Status(TRUE) != URL_LOADING)
					{
#ifdef DOM_HTTP_SUPPORT
						if (xmlhttprequest && OpStatus::IsMemoryError(xmlhttprequest->AdvanceReadyStateToLoading(contenthandler->GetInterruptThread())))
						{
							HandleOOM();
							return;
						}
#endif // DOM_HTTP_SUPPORT
						HandleFinished();
					}

					return;
				}
			}

#ifdef OOM_SAFE_API
			TRAP_AND_RETURN_VALUE_IF_ERROR(status, url_dd->RetrieveDataL(more), status);
#else // OOM_SAFE_API
			url_dd->RetrieveData(more);
#endif // OOM_SAFE_API

			buffer = reinterpret_cast<const unsigned char *>(url_dd->GetBuffer());
			buffer_bytes = url_dd->GetBufSize();

			more_from_url = more;
			from_dd = TRUE;
		}
	}

#ifdef DOM_HTTP_SUPPORT
	if (xmlhttprequest &&
	    (OpStatus::IsMemoryError(xmlhttprequest->SetStatus(url)) ||
	     OpStatus::IsMemoryError(xmlhttprequest->AdvanceReadyStateToLoading(contenthandler->GetInterruptThread())) ||
	     OpStatus::IsMemoryError(xmlhttprequest->AddResponseData(buffer, buffer_bytes))))
	{
		HandleOOM();
		return;
	}
#endif // DOM_HTTP_SUPPORT

	BOOL failed = FALSE;

	if (!no_parsing && !parsing_failed)
	{
		DOM_EnvironmentImpl::CurrentState state(environment);
		state.SetOwnerDocument(parser->GetOwnerDocument());

		if (OpStatus::IsMemoryError(xml_parser->Parse(reinterpret_cast<const uni_char *>(buffer), UNICODE_DOWNSIZE(buffer_bytes), more)))
		{
			HandleOOM();
			return;
		}
		else if (xml_parser->IsFailed())
		{
			HandleError(TRUE);
			failed = TRUE;
		}
		else
		{
			if (queue_used)
			{
				if (!has_doctype_or_root)
				{
					for (unsigned index = 0; index < queue_used; ++index)
						if (HTML_Element *element = queue[index])
							if (element->Type() == HE_DOCTYPE || Markup::IsRealElement(element->Type()))
							{
								has_doctype_or_root = TRUE;
								break;
							}

					if (has_doctype_or_root)
						parser->HandleDocumentInfo(xml_parser->GetDocumentInformation());
				}

				if (OpStatus::IsError(contenthandler->MoreElementsAvailable()))
				{
					HandleError(FALSE);
					failed = TRUE;
				}
			}

			if (!failed && !xml_parser->IsFinished())
				more = TRUE;
		}
	}

	if (no_parsing || !failed || parsing_failed)
	{
		if (no_parsing || parsing_failed || url.GetAttribute(URL::KHTTP_Response_Code, TRUE) == 404)
			contenthandler->DisableParsing();

		if (from_dd && buffer_bytes > 0)
			url_dd->ConsumeData(buffer_bytes);

		if (!more)
			HandleFinished(FALSE);
	}

	if (more && !more_from_url)
		mh->PostMessage(MSG_URL_DATA_LOADED, 0, 0);
}

#ifdef DOM_HTTP_SUPPORT

void
DOM_LSLoader::HandleResponseHeaders()
{
	if (!headers_handled)
	{
		headers_handled = TRUE;

		if (DOM_XMLHttpRequest *xmlhttprequest = parser->GetXMLHttpRequest())
			if (OpStatus::IsMemoryError(xmlhttprequest->SetResponseHeaders(url)))
				HandleOOM();
	}
}

#endif // DOM_HTTP_SUPPORT

OP_STATUS
DOM_LSLoader::SetCallbacks(DOM_LSLoadHandler *handler)
{
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_URL_DATA_LOADED, 0));
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_HEADER_LOADED, 0));
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_NOT_MODIFIED, 0));
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_URL_LOADING_FAILED, 0));
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_URL_MOVED, 0));
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_MULTIPART_RELOAD, 0));
#ifdef PROGRESS_EVENTS_SUPPORT
	if (handler->GetTimeoutMS() > 0)
		RETURN_IF_ERROR(mh->SetCallBack(this, MSG_DOM_LSLOADER_TIMEOUT, 0));
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_DOCHAND_UPLOAD_PROGRESS, 0));
#else
	(void)handler;
#endif // PROGRESS_EVENTS_SUPPORT

	return OpStatus::OK;
}

OP_STATUS
DOM_LSLoader::SetElement(HTML_Element *element)
{
	if (queue_used == queue_size)
	{
		unsigned new_queue_size = queue_size ? queue_size += queue_size : queue_size = 256;
		HTML_Element **new_queue = OP_NEWA(HTML_Element *, new_queue_size);

		if (!new_queue)
		{
			if (element && element->Clean(environment->GetFramesDocument()))
				element->Free(environment->GetFramesDocument());

			return OpStatus::ERR_NO_MEMORY;
		}

		if (queue_used > 0)
			op_memcpy(new_queue, queue, sizeof queue[0] * queue_used);

		OP_DELETEA(queue);
		queue = new_queue;
		queue_size = new_queue_size;
	}

	queue[queue_used++] = element;
	return OpStatus::OK;
}

void
DOM_LSLoader::HandleFinished(BOOL abort)
{
	finished = TRUE;

#ifdef SCOPE_RESOURCE_MANAGER
	if (OpScopeResourceListener::IsEnabled())
	{
		// We already have all events up to OnResponse
		int reqid = OpScopeResourceListener::GetHTTPRequestID(url.GetRep());
		OpStatus::Ignore(OpScopeResourceListener::OnResponseFinished(url.GetRep(), reqid));
		OpStatus::Ignore(OpScopeResourceListener::OnUrlFinished(url.GetRep(), URL_LOAD_FINISHED));
	}
#endif // SCOPE_RESOURCE_MANAGER

	if (mh)
	{
		mh->UnsetCallBacks(this);

		if (!url.IsEmpty())
		{
			if (abort)
				url.StopLoading(mh);

			OP_DELETE(url_dd);
			url_dd = NULL;

			url_in_use.UnsetURL();

			url = URL();
		}
		DeleteLoadRelatedObjects();
	}

#ifdef DOM_HTTP_SUPPORT
	DOM_XMLHttpRequest *xhr = parser->GetXMLHttpRequest();
	if (!xhr || !xhr->HasEventHandlerAndUnsentEvents())
		environment->RemoveLSLoader(this);
#else
	environment->RemoveLSLoader(this);
#endif // DOM_HTTP_SUPPORT

	if (queue_used == 0 || parsing_failed && !contenthandler->IsRunning())
	{
		if (OpStatus::IsMemoryError(contenthandler->HandleFinished()))
			HandleOOM();
	}


}

void
DOM_LSLoader::HandleError(BOOL from_parser)
{
#ifdef DOM_HTTP_SUPPORT
	if (parser->GetXMLHttpRequest())
	{
		/* Don't signal an error (no-one will be listening anyway), and since
		   this is an XMLHttpRequest, the entire (unparsable) result will be
		   available in xmlhttprequest.responseText, which might be what the
		   script is after. */
		parsing_failed = TRUE;
		return;
	}
#endif // DOM_HTTP_SUPPORT

	if (mh)
	{
		mh->UnsetCallBacks(this);

		if (!url.IsEmpty())
		{
			url.StopLoading(mh);

			OP_DELETE(url_dd);
			url_dd = NULL;

			url_in_use.UnsetURL();

			url = URL();
		}
	}

	environment->RemoveLSLoader(this);

	unsigned line = 0, column = 0, byteOffset = 0, charOffset = 0;

#ifdef XML_ERRORS
	if (from_parser)
	{
		XMLRange range = xml_parser->GetErrorPosition();

		if (range.start.IsValid())
		{
			line = range.start.line + 1;
			column = range.start.column;
		}
	}
#endif // XML_ERRORS

	if (mh)
		DeleteLoadRelatedObjects();

	OpStatus::Ignore(parser->SignalError(UNI_L("Parsing failed."), UNI_L("parse-error"), line, column, byteOffset, charOffset));
}

void
DOM_LSLoader::HandleOOM()
{
	if (!oom)
	{
		/* FIXME: report error here, preferably through Window::RaiseCondition if
		   we had a reference to a window. */
		oom = TRUE;

		parser->SignalOOM();
		Abort();
	}
}

/* virtual */ void
DOM_LSLoader::Continue(XMLParser *parser)
{
	mh->PostMessage(MSG_URL_DATA_LOADED, 0, 0);
}

/* virtual */ void
DOM_LSLoader::Stopped(XMLParser *parser)
{
}

void
DOM_LSLoader::SecurityCheckCompleted(BOOL allowed, OP_STATUS error)
{
	blocked_on_security_check = FALSE;
	if (allowed)
	{
		if (mh)
			/* Repost the queued messages. */
			for (unsigned i = 0; i < recorded_messages.GetCount(); i++)
			{
				mh->PostMessage(recorded_messages.Get(i)->msg,
								recorded_messages.Get(i)->par1,
								recorded_messages.Get(i)->par2);
			}
		recorded_messages.DeleteAll();
	}
	else
		HandleFinished();
}

/* virtual */ void
DOM_LSLoader::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (blocked_on_security_check)
	{
		/* Queue the message on security check completion. */
		RecordedMessage* recorded_message = OP_NEW(RecordedMessage, (msg, par1, par2));
		if (!recorded_message || OpStatus::IsError(recorded_messages.Add(recorded_message)))
		{
			/* Soft OOM; abort the load. */
			OP_DELETE(recorded_message);
			msg = MSG_URL_LOADING_FAILED;
			par1 = 0;
			par2 = 0;
		}
		else
			return;
	}

	if (msg == MSG_HEADER_LOADED || msg == MSG_NOT_MODIFIED)
		headers_loaded_received = TRUE;

	switch (msg)
	{
	case MSG_URL_DATA_LOADED:
		if (!headers_loaded_received)
			/* Stray data loaded message; ignore. */
			return;
		/* fall through */

	case MSG_HEADER_LOADED:
	case MSG_NOT_MODIFIED:
	{
		if (!string && url.Status(TRUE) == URL_UNLOADED)
			/* Weird and unexpected URL state; ignore this message. */
			return;

		OP_BOOLEAN result = load_handler->OnHeadersReceived(url);

		if (OpStatus::IsMemoryError(result))
			HandleOOM();
		else if (OpStatus::IsError(result))
			HandleFinished();
		else
		{
			LoadData();
#ifdef DOM_HTTP_SUPPORT
			if (msg == MSG_NOT_MODIFIED)
				if (DOM_XMLHttpRequest *xmlhttprequest = parser->GetXMLHttpRequest())
					if (OpStatus::IsMemoryError(xmlhttprequest->SetNotModifiedStatus(url)))
						HandleOOM();
#endif // DOM_HTTP_SUPPORT

			if (!finished && result == OpBoolean::IS_FALSE)
				HandleFinished();
		}
		return;
	}
	case MSG_URL_LOADING_FAILED:
	{
		BOOL is_network_error = par2 == ERR_REDIRECT_FAILED || !(url.Type() == URL_HTTP || url.Type() == URL_HTTPS);
		if (!is_network_error)
			is_network_error = url.GetAttribute(URL::KHTTP_Response_Code, TRUE) == 0;

		OP_STATUS status = load_handler->OnLoadingFailed(url, is_network_error);
		if (OpStatus::IsMemoryError(status))
			HandleOOM();
		else
		{
			BOOL no_content = par2 == ERR_HTTP_NO_CONTENT || par2 == ERR_COMM_BLOCKED_URL;
			if (no_content)
				/* No data but to get all the status flags set correctly. */
				LoadData();
			else
				HandleError(FALSE);

			HandleFinished();
		}
		return;
	}
	case MSG_MULTIPART_RELOAD:
		HandleFinished();
		return;

	case MSG_URL_MOVED:
	{
		/* Note that we may only get a single MSG_URL_MOVED regardless of the
		   length of the redirect chain because we post such a message to
		   ourselves to trigger security checks. */
		URL moved_to = url.GetAttribute(URL::KMovedToURL, FALSE);
		while (!moved_to.IsEmpty())
		{
			/* There will only be one redirect "in flight" at any one time. */
			OP_BOOLEAN result = load_handler->OnRedirect(url, moved_to);
			if (OpStatus::IsMemoryError(result))
			{
				HandleOOM();
				return;
			}
			else if (OpStatus::IsError(result))
			{
				HandleFinished();
				return;
			}
			else if (result == OpBoolean::IS_FALSE)
			{
				/* "Block" waiting for security check completion. */
				blocked_on_security_check = TRUE;
				return;
			}
			else
				moved_to = moved_to.GetAttribute(URL::KMovedToURL, FALSE);
		}
		break;
	}
#ifdef PROGRESS_EVENTS_SUPPORT
	case MSG_DOM_LSLOADER_TIMEOUT:
	{
		OP_BOOLEAN result = load_handler->OnTimeout();
		if (OpStatus::IsMemoryError(result))
			HandleOOM();
		else if (OpStatus::IsError(result) || result == OpBoolean::IS_TRUE)
			HandleFinished(TRUE);
		return;
	}
	case MSG_DOM_PROGRESS_EVENT_TICK:
		if (OpStatus::IsMemoryError(load_handler->OnProgressTick()))
			HandleOOM();
		return;
	case MSG_DOCHAND_UPLOAD_PROGRESS:
		if (static_cast<URL_ID>(par1) == url.Id())
			if (OpStatus::IsMemoryError(load_handler->OnUploadProgress(static_cast<OpFileLength>(par2))))
				HandleOOM();
		return;
#endif // PROGRESS_EVENTS_SUPPORT
	}
}

DOM_LSLoader::DOM_LSLoader(DOM_LSParser *parser)
	: environment(parser->GetEnvironment())
	, parser(parser)
	, contenthandler(NULL)
	, mh(NULL)
	, url_dd(NULL)
	, string(NULL)
	, string_consumed(FALSE)
	, xml_parser(NULL)
	, xml_tokenhandler(NULL)
	, queue(NULL)
	, queue_used(0)
	, queue_size(0)
	, is_cdata_section(FALSE)
	, finished(FALSE)
	, stopped(FALSE)
	, oom(FALSE)
	, parsing_failed(FALSE)
	, is_parsing_fragment(FALSE)
	, headers_handled(FALSE)
	, has_doctype_or_root(FALSE)
	, headers_loaded_received(FALSE)
	, no_parsing(FALSE)
	, load_handler(NULL)
#ifdef PROGRESS_EVENTS_SUPPORT
	, with_progress_ticks(FALSE)
#endif // PROGRESS_EVENTS_SUPPORT
	, blocked_on_security_check(FALSE)
{
}

DOM_LSLoader::~DOM_LSLoader()
{
	Abort();
}

void
DOM_LSLoader::SetContentHandler(DOM_LSContentHandler *contenthandler_)
{
	contenthandler = contenthandler_;
}

void
DOM_LSLoader::SetLoadHandler(DOM_LSLoadHandler *handler)
{
	load_handler = handler;
}

#ifdef PROGRESS_EVENTS_SUPPORT
OP_STATUS
DOM_LSLoader::PostProgressTick(unsigned milliseconds)
{
	if (mh) // If mh is NULL we have stopped loading already anyway.
	{
		if (!with_progress_ticks)
		{
			with_progress_ticks = TRUE;
			RETURN_IF_ERROR(mh->SetCallBack(this, MSG_DOM_PROGRESS_EVENT_TICK, 0));
		}
		mh->PostMessage(MSG_DOM_PROGRESS_EVENT_TICK, 0, 0, milliseconds);
	}
	return OpStatus::OK;
}
#endif // PROGRESS_EVENTS_SUPPORT

OP_STATUS
DOM_LSLoader::CreateLoadRelatedObjects(URL local_url, BOOL is_fragment)
{
	FramesDocument *doc = parser->GetFramesDocument();
	Window *win = NULL;
	DocumentManager *docman = NULL;
	if (!doc)
	{
#ifdef DOM_WEBWORKERS_SUPPORT
		if (DOM_WebWorker *worker = parser->GetEnvironment()->GetWorkerController()->GetWorkerObject())
		{
			doc = worker->GetWorkerOwnerDocument();
			if (worker->IsA(DOM_TYPE_WEBWORKERS_WORKER_DEDICATED))
			{
				win = doc->GetWindow();
				docman = doc->GetDocManager();
			}
		}
#endif // DOM_WEBWORKERS_SUPPORT
	}
	else
	{
		win = doc->GetWindow();
		docman = doc->GetDocManager();
	}

	if (!doc || !doc->GetLogicalDocument())
		return OpStatus::ERR;

	RETURN_OOM_IF_NULL(mh = OP_NEW(MessageHandler, (win, docman)));


	if (!no_parsing)
	{
		RETURN_IF_ERROR(OpElementCallback::MakeTokenHandler(xml_tokenhandler, doc->GetLogicalDocument(), this));
		RETURN_IF_ERROR(XMLParser::Make(xml_parser, this, mh, xml_tokenhandler, local_url));

		XMLParser::Configuration configuration;

		if (is_fragment)
		{
			configuration.parse_mode = XMLParser::PARSEMODE_FRAGMENT;
			configuration.allow_xml_declaration = FALSE;
			configuration.allow_doctype = FALSE;

			if (HTML_Element *element = parser->GetContext()->GetThisElement())
				RETURN_IF_ERROR(XMLNamespaceDeclaration::PushAllInScope(configuration.nsdeclaration, element));
		}
		configuration.preferred_literal_part_length = SplitTextLen;

		xml_parser->SetConfiguration(configuration);
	}

	return OpStatus::OK;
}

void
DOM_LSLoader::DeleteLoadRelatedObjects()
{
	OP_DELETE(xml_parser);
	xml_parser = NULL;

	OP_DELETE(xml_tokenhandler);
	xml_tokenhandler = NULL;

	OP_DELETE(mh);
	mh = NULL;
}


OP_STATUS
DOM_LSLoader::Construct(URL *url_, URL *ref_url_, const uni_char *string_, BOOL is_fragment, DOM_LSLoadHandler *handler)
{
	OP_ASSERT(handler);
	URL local_url;

	if (url_)
		local_url = *url_;

	RETURN_IF_ERROR(CreateLoadRelatedObjects(local_url, is_fragment));

	BOOL post_message = FALSE;

	RETURN_IF_ERROR(SetCallbacks(handler));

	if (url_)
	{
		url = *url_;

		if (ref_url_)
			ref_url = *ref_url_;

		BOOL load_url = FALSE;

		if (url.Type() == URL_FILE)
			load_url = TRUE;
		else
		{
			CheckExpiryType check_expiry = static_cast<CheckExpiryType>(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CheckExpiryLoad));
			URL next_url = url;
			while (!load_url && !next_url.IsEmpty())
			{
				URLStatus stat = next_url.Status(FALSE);
				if (stat == URL_UNLOADED || stat == URL_LOADING || stat == URL_LOADING_ABORTED || stat == URL_LOADING_FAILURE)
					load_url = TRUE;
				else
					if (check_expiry != CHECK_EXPIRY_NEVER && next_url.Expired(FALSE, check_expiry == CHECK_EXPIRY_USER))
						load_url = TRUE;
					else if (next_url.GetAttribute(URL::KCachePolicy_MustRevalidate))
						load_url = TRUE;
					else
						next_url = next_url.GetAttribute(URL::KMovedToURL, FALSE);
			}
		}

#ifdef DOM_HTTP_SUPPORT
		if (parser->GetXMLHttpRequest() && !load_url)
			if (url.Type() == URL_HTTP || url.Type() == URL_HTTPS)
			{
				OpString8 headers8;

				RETURN_IF_ERROR(url.GetAttribute(URL::KHTTPAllResponseHeadersL, headers8, TRUE));

				if (headers8.IsEmpty())
					load_url = TRUE;
			}
#endif // DOM_HTTP_SUPPORT

		BOOL only_if_modified = TRUE;
		BOOL proxy_no_cache = FALSE;

#ifdef SCOPE_RESOURCE_MANAGER
		if (OpScopeResourceListener::IsEnabled())
		{
			if (OpScopeResourceListener::ForceFullReload(url.GetRep(), mh->GetDocumentManager(), mh->GetWindow()))
			{
				load_url = TRUE;
				only_if_modified = FALSE;
				proxy_no_cache = TRUE;
			}

			OpScopeResourceListener::OnXHRLoad(url.GetRep(), !load_url, mh->GetDocumentManager(), mh->GetWindow());
		}
#endif // SCOPE_RESOURCE_MANAGER

		if (load_url)
		{
#ifdef HTTP_CONTENT_USAGE_INDICATION
			OpStatus::Ignore(url.SetAttribute(URL::KHTTP_ContentUsageIndication,HTTP_UsageIndication_OtherInline));
#endif // HTTP_CONTENT_USAGE_INDICATION

			// Sync xhr is blocking this is why we care about its priority so much.
			if (!parser->IsAsync())
			{
				if (FramesDocument *frm_doc = parser->GetFramesDocument())
					url.SetAttribute(URL::KHTTP_Priority, frm_doc->GetParentDoc() ?
                                     FramesDocument::LOAD_PRIORITY_MAIN_DOCUMENT_SYNC_XHR :
                                     FramesDocument::LOAD_PRIORITY_FRAME_DOCUMENT_SYNC_XHR);
				else
					url.SetAttribute(URL::KHTTP_Priority, URL_LOWEST_PRIORITY);
			}

			/* "Non-credentialled" cross-origin XHR uses must not include cookies nor have
			   them considered in the reponse. In DOM_XMLHttpRequest::SetCredentials(),
			   cookie processing is explicitly disabled if that's the context. To avoid
			   having Reload() revert the setting of KDisableProcessCookies, set
			   thirdparty_determined to TRUE. */
			BOOL thirdparty_determined = TRUE;
			BOOL allow_if_modified = only_if_modified;
			CommState state = url.Reload(mh, ref_url, only_if_modified, proxy_no_cache, FALSE, FALSE, TRUE, FALSE, thirdparty_determined, NotEnteredByUser, &allow_if_modified);
#ifdef DOM_HTTP_SUPPORT
			if (DOM_XMLHttpRequest *xmlhttprequest = parser->GetXMLHttpRequest())
				if (state != COMM_REQUEST_FAILED)
					xmlhttprequest->GetRequest()->SetWasConditionallyIssued(allow_if_modified);
#else
			(void)state;
#endif // DOM_HTTP_SUPPORT

#ifdef PROGRESS_EVENTS_SUPPORT
			if (unsigned millisecs = load_handler->GetTimeoutMS())
				mh->PostMessage(MSG_DOM_LSLOADER_TIMEOUT, 0, 0, millisecs);
			/* Start ticking progress events, for up- and downloads. */
			PostProgressTick(DOM_XHR_PROGRESS_EVENT_INTERVAL);
#endif // PROGRESS_EVENTS_SUPPORT
		}
		else
		{
			if (!url_->GetAttribute(URL::KMovedToURL, FALSE).IsEmpty())
				mh->PostMessage(MSG_URL_MOVED, 0, 0);
			post_message = TRUE;
		}

		url_in_use.SetURL(url);
	}
	else
	{
		RETURN_IF_ERROR(UniSetStr(string, string_));
		mh->PostMessage(MSG_HEADER_LOADED, 0, 0);
		post_message = TRUE;
	}

	if (post_message)
	{
		mh->PostMessage(MSG_HEADER_LOADED, 0, 0);
		mh->PostMessage(MSG_URL_DATA_LOADED, 0, 0);
	}

	environment->AddLSLoader(this);

	return OpStatus::OK;
}

void
DOM_LSLoader::Abort()
{
	stopped = TRUE;

	if (mh)
		mh->UnsetCallBacks(this);

	g_main_message_handler->UnsetCallBacks(this);

	recorded_messages.DeleteAll();

	if (url.Type() != URL_NULL_TYPE)
	{
		OP_DELETE(url_dd);
		url_dd = NULL;

		url_in_use.UnsetURL();

		url.StopLoading(mh);

		url = URL();
	}

	environment->RemoveLSLoader(this);

	OP_DELETEA(string);
	string = NULL;

	if (mh)
		DeleteLoadRelatedObjects();

	for (unsigned index = 0; index < queue_used; ++index)
		if (HTML_Element *element = queue[index])
			if (!element->GetESElement())
				HTML_Element::DOMFreeElement(element, environment);

	OP_DELETEA(queue);
	queue = NULL;
	queue_used = queue_size = 0;

	text.Clear();
}

void
DOM_LSLoader::GetElements(HTML_Element **&elements, unsigned &count)
{
	elements = queue;
	count = queue_used;
}

void
DOM_LSLoader::ConsumeElements(unsigned count)
{
	if (count < queue_used)
		op_memmove(queue, queue + count, sizeof queue[0] * (queue_used - count));

	queue_used -= count;

	if (queue_used == 0 && finished)
		if (OpStatus::IsMemoryError(contenthandler->HandleFinished()))
			HandleOOM();
}

void
DOM_LSLoader::GCTrace()
{
	for (unsigned index = 0; index < queue_used; ++index)
		if (HTML_Element *element = queue[index])
			if (element->GetESElement())
				DOM_Object::GCMark(element->GetESElement());
}
#endif // DOM3_LOAD
