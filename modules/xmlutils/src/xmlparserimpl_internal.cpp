/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/xmlutils/src/xmlparserimpl_internal.h"
#include "modules/xmlutils/xmlerrorreport.h"
#include "modules/xmlparser/xmlinternalparser.h"
#include "modules/hardcore/mh/messages.h"
#ifdef OPERA_CONSOLE
# include "modules/console/opconsoleengine.h"
#endif // OPERA_CONSOLE
#include "modules/dochand/win.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opfile/unistream.h"

OP_BOOLEAN
XMLDataSourceHandlerImpl::Load(XMLDataSourceImpl *source, const URL &url)
{
	if (FramesDocument *doc = parser->GetDocument())
	{
		OP_LOAD_INLINE_STATUS status = doc->LoadInline(url, source);

		if (OpStatus::IsMemoryError(status))
			return status;
		else if (status == LoadInlineStatus::LOADING_CANCELLED)
			return OpBoolean::IS_FALSE;
		else
		{
			source->SetLoading();
			return OpBoolean::IS_TRUE;
		}
	}
	else if (MessageHandler *mh = parser->GetMessageHandler())
	{
		RETURN_IF_ERROR(source->SetCallbacks());

		URL_LoadPolicy loadpolicy(FALSE, URL_Reload_If_Expired, FALSE);
		URL local(url);

		local.SetAttribute(URL::KHTTP_Priority, FramesDocument::LOAD_PRIORITY_MAIN_DOCUMENT);

		CommState state = local.LoadDocument(mh, parser->GetURL(), loadpolicy);

		if (state == COMM_LOADING)
		{
			source->SetLoading();
			return OpBoolean::IS_TRUE;
		}
		else
			return OpBoolean::IS_FALSE;
	}
	else
		return OpBoolean::IS_FALSE;
}

XMLDataSourceHandlerImpl::XMLDataSourceHandlerImpl(XMLParserImpl *parser, BOOL load_external_entities)
	: parser(parser),
	  base(0),
	  current(0),
	  load_external_entities(load_external_entities)
{
}

XMLDataSourceHandlerImpl::~XMLDataSourceHandlerImpl()
{
	OP_ASSERT(base == current || !current);
	OP_DELETE(base);
}

/* virtual */ OP_STATUS
XMLDataSourceHandlerImpl::CreateInternalDataSource(XMLDataSource *&source, const uni_char *data, unsigned data_length)
{
	source = 0;

	XMLDataSourceImpl *ds = OP_NEW(XMLDataSourceImpl, (this));

	if (!ds || OpStatus::IsMemoryError(ds->AddData(data, data_length, FALSE, FALSE)))
	{
		OP_DELETE(ds);
		return OpStatus::ERR_NO_MEMORY;
	}

	ds->SetNextSource(current);
	current = ds;

	source = ds;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
XMLDataSourceHandlerImpl::CreateExternalDataSource(XMLDataSource *&source, const uni_char *pubid, const uni_char *system, URL baseurl)
{
	source = 0;
	system = XMLDocumentInformation::GetResolvedSystemId(pubid, system);

	URL url = g_url_api->GetURL(baseurl, system);

	OpString system_string;
	RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI,system_string));

	system = system_string.CStr();

	BOOL found = FALSE;

	const uni_char *filename = NULL;
	BOOL is_default = TRUE;

#ifdef XML_CONFIGURABLE_DOCTYPES
	RETURN_IF_ERROR(g_opera->xmlutils_module.configureddoctypes->Update(is_default));

	found = g_opera->xmlutils_module.configureddoctypes->Find(filename, pubid, system, load_external_entities);
#else // XML_CONFIGURABLE_DOCTYPES
	if (!load_external_entities)
	{
		if (XMLDocumentInformation::IsXHTML(pubid, system))
			filename = UNI_L("html40_entities.dtd");
#ifdef _WML_SUPPORT_
		else if (XMLDocumentInformation::IsWML(pubid, system))
			filename = UNI_L("wml1_entities.dtd");
#endif // _WML_SUPPORT_
	}
#endif // XML_CONFIGURABLE_DOCTYPES

	if (filename)
	{
		OP_STATUS status = OpStatus::OK;
		OpFile *file = OP_NEW(OpFile, ());

		if (!file || OpStatus::IsError(status = file->Construct(filename, uni_strstr(filename, PATHSEP) == NULL ? (is_default ? OPFILE_RESOURCES_FOLDER : OPFILE_USERPREFS_FOLDER) : OPFILE_ABSOLUTE_FOLDER)))
		{
			OP_DELETE(file);
			if (!file || OpStatus::IsMemoryError(status))
				return OpStatus::ERR_NO_MEMORY;
			else
				return OpStatus::OK;
		}

		UnicodeFileInputStream* stream = OP_NEW(UnicodeFileInputStream, ());

		if (!stream || OpStatus::IsError(status = stream->Construct(file, URL_XML_CONTENT, TRUE)))
		{
			OP_DELETE(stream);
			OP_DELETE(file);
			if (!stream || OpStatus::IsMemoryError(status))
				return OpStatus::ERR_NO_MEMORY;
			else
				return OpStatus::OK;
		}

		source = OP_NEW(XMLDataSourceImpl, (this, url, FALSE, file, stream));

		if (!source)
		{
			OP_DELETE(stream);
			OP_DELETE(file);
			return OpStatus::ERR_NO_MEMORY;
		}

		if (OpStatus::IsMemoryError(((XMLDataSourceImpl *) source)->LoadFromStream()))
		{
			OP_DELETE(source);
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	else
	if (load_external_entities)
	{
		source = OP_NEW(XMLDataSourceImpl, (this, url, found));

		if (!source)
			return OpStatus::ERR_NO_MEMORY;
		else if (!((XMLDataSourceImpl *) source)->CheckExternalEntityAccess())
		{
			OP_DELETE(source);
			source = NULL;
		}
		else
		{
			OP_BOOLEAN result = Load((XMLDataSourceImpl *) source, url);

			if (OpStatus::IsMemoryError(result) || result == OpBoolean::IS_FALSE)
			{
				OP_DELETE(source);
				source = NULL;

				if (OpStatus::IsMemoryError(result))
					return OpStatus::ERR_NO_MEMORY;
				else
					return OpStatus::OK;
			}
		}
	}

	if (source)
	{
		source->SetNextSource(current);
		current = (XMLDataSourceImpl *) source;
	}

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
XMLDataSourceHandlerImpl::DestroyDataSource(XMLDataSource *source)
{
	if (source != base)
	{
		current = (XMLDataSourceImpl *) source->GetNextSource();
		XMLDataSourceImpl::Discard((XMLDataSourceImpl *) source);
	}

	if (current == base)
		parser->Continue();

	return OpStatus::OK;
}

OP_STATUS
XMLDataSourceHandlerImpl::Construct(const URL &url)
{
	if (!(base = current = OP_NEW(XMLDataSourceImpl, (this, url, TRUE))))
		return OpStatus::ERR_NO_MEMORY;
	else
		return OpStatus::OK;
}

/* virtual */ void
XMLDataSourceImpl::LoadingProgress(const URL &url)
{
	LoadFromUrl();

	XMLParserImpl *parser = handler->GetParser();

	if (parser->IsPaused())
		if (MessageHandler *mh = parser->GetMessageHandler())
			mh->PostMessage(MSG_XMLUTILS_CONTINUE, (MH_PARAM_1) parser, 0);
		else
			/* Parsing will now hang.  But this ought to be impossible. */
			OP_ASSERT(FALSE);

	if (discarded && !busy)
		OP_DELETE(this);
}

/* virtual */ void
XMLDataSourceImpl::LoadingStopped(const URL &url)
{
	LoadingProgress(url);
}

/* virtual */ void
XMLDataSourceImpl::LoadingRedirected(const URL &from, const URL &to)
{
	CheckExternalEntityAccess();
}

/* virtual */ void
XMLDataSourceImpl::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
	case MSG_URL_DATA_LOADED:
	case MSG_URL_LOADING_FAILED:
		LoadingProgress(url);
		return;

	case MSG_URL_MOVED:
		if (CheckExternalEntityAccess() && OpStatus::IsMemoryError(SetCallbacks()))
			handler->GetParser()->HandleOOM();
		break;
	}

	if (discarded && !busy)
	{
		XMLDataSourceImpl *self = this;
		OP_DELETE(self);
	}
}

void
XMLDataSourceImpl::LoadFromUrl()
{
	OP_ASSERT(!url.IsEmpty());

	XMLParserImpl *parser = handler->GetParser();

	if (!stream && !urldd && !force_empty)
	{
		int http_response;
		if (url.Type() == URL_HTTP || url.Type() == URL_HTTPS)
			http_response = url.GetAttribute(URL::KHTTP_Response_Code, TRUE);
		else
			http_response = HTTP_OK;

		if (http_response == HTTP_OK || http_response == HTTP_NOT_MODIFIED)
		{
			if (url.ContentType() != URL_XML_CONTENT)
			{
				if (OpStatus::IsMemoryError(url.SetAttribute(URL::KMIME_ForceContentType, "text/xml")))
				{
					handler->GetParser()->HandleOOM();
					return;
				}
			}

			urldd = url.GetDescriptor(parser->GetMessageHandler(), TRUE);

			if (!urldd)
				return;
		}
		else
			is_at_end = TRUE;
	}

	++busy;

	if (handler->GetCurrentDataSource() == this)
		if (OpStatus::IsMemoryError(handler->GetParser()->Parse(0, 0, TRUE, 0)))
			handler->GetParser()->HandleOOM();

	--busy;
}

XMLDataSourceImpl::XMLDataSourceImpl(XMLDataSourceHandlerImpl *handler)
	: handler(handler),
	  urldd(NULL),
	  file(NULL),
	  stream(NULL),
	  first_data_element(NULL),
	  last_data_element(NULL),
	  is_at_end(FALSE),
	  discarded(FALSE),
	  force_empty(FALSE),
	  is_loading(FALSE),
	  url_allowed(FALSE),
	  need_more_data(FALSE),
	  busy(0),
	  default_consumed(0)
{
}

XMLDataSourceImpl::XMLDataSourceImpl(XMLDataSourceHandlerImpl *handler, const URL &url, BOOL url_allowed, OpFile *file, UnicodeInputStream *stream)
	: handler(handler),
	  url(url),
	  urldd(NULL),
	  file(file),
	  stream(stream),
	  first_data_element(NULL),
	  last_data_element(NULL),
	  is_at_end(FALSE),
	  discarded(FALSE),
	  force_empty(FALSE),
	  is_loading(FALSE),
	  url_allowed(url_allowed),
	  need_more_data(FALSE),
	  busy(0),
	  default_consumed(0)
{
}

void
XMLDataSourceImpl::Unregister()
{
	if (FramesDocument *doc = handler->GetParser()->GetDocument())
		doc->StopLoadingInline(url, this);
	else if (MessageHandler *mh = handler->GetParser()->GetMessageHandler())
		mh->UnsetCallBacks(this);
}

XMLDataSourceImpl::~XMLDataSourceImpl()
{
	OP_ASSERT(!busy);

	Unregister();

	DataElement *data_element = first_data_element;
	while (data_element)
	{
		DataElement *next_data_element = data_element->next;
		OP_DELETE(data_element);
		data_element = next_data_element;
	}

	OP_DELETE(urldd);

	OP_DELETE(stream);
	OP_DELETE(file);
}

/* virtual */ OP_STATUS
XMLDataSourceImpl::Initialize()
{
	return OpStatus::OK;
}

/* virtual */ const uni_char *
XMLDataSourceImpl::GetName()
{
	return GetURL().GetAttribute(URL::KUniName_With_Fragment).CStr();
}

/* virtual */ URL
XMLDataSourceImpl::GetURL()
{
	if (url.IsEmpty())
		if (XMLDataSource *next = GetNextSource())
			return next->GetURL();

	return url;
}

/* virtual */ const uni_char *
XMLDataSourceImpl::GetData()
{
	return first_data_element ? first_data_element->data + first_data_element->data_offset : NULL;
}

/* virtual */ unsigned
XMLDataSourceImpl::GetDataLength()
{
	return first_data_element ? first_data_element->data_length : 0;
}

/* virtual */ unsigned
XMLDataSourceImpl::Consume(unsigned length)
{
	if (length != 0)
	{
		DataElement *data_element = first_data_element;

		OP_ASSERT(length <= data_element->data_length);

		if ((data_element->data_length -= length) == 0)
		{
			first_data_element = data_element->next;
			OP_DELETE(data_element);

			if (!first_data_element)
				last_data_element = NULL;
		}
		else
			data_element->data_offset += length;
	}

	return length;
}

/* virtual */ OP_BOOLEAN
XMLDataSourceImpl::Grow()
{
	DataElement *data_element = first_data_element;

	if (data_element)
		if (data_element->next)
		{
			/* FIXME: this is one way, just copying data from following
					  data elements into the first data element is another
					  one, that would mean less allocation.  That is
					  potentionally ineffective though, if the first data
					  element is very small. */

			DataElement *next_data_element = data_element->next;

			unsigned combined_length = data_element->data_length + next_data_element->data_length;
			uni_char *combined = OP_NEWA(uni_char, combined_length);

			if (!combined)
				return OpStatus::ERR_NO_MEMORY;

			op_memcpy(combined, data_element->data + data_element->data_offset, data_element->data_length * sizeof combined[0]);
			op_memcpy(combined + data_element->data_length, next_data_element->data, next_data_element->data_length * sizeof combined[0]);

			OP_DELETE(data_element);

			if (next_data_element->ownsdata)
			{
				uni_char *non_const = const_cast<uni_char *>(next_data_element->data);
				OP_DELETEA(non_const);
			}

			next_data_element->data = combined;
			next_data_element->data_length = combined_length;
			next_data_element->ownsdata = TRUE;

			first_data_element = next_data_element;

			return OpBoolean::IS_TRUE;
		}
		else if (data_element->data_offset == 0 && !data_element->ownsdata)
			need_more_data = TRUE;

	return OpBoolean::IS_FALSE;
}

/* virtual */ BOOL
XMLDataSourceImpl::IsAtEnd()
{
	return first_data_element == 0 && is_at_end;
}

/* virtual */ BOOL
XMLDataSourceImpl::IsAllSeen()
{
	return first_data_element == last_data_element && is_at_end;
}

#ifdef XML_ERRORS

/* virtual */ OP_BOOLEAN
XMLDataSourceImpl::Restart ()
{
	BOOL is_base = handler->GetBaseDataSource() == this;

	if (!is_base && !urldd && !stream)
	{
		OP_ASSERT(first_data_element && first_data_element == last_data_element);
		first_data_element->data_offset = 0;
		return OpBoolean::IS_TRUE;
	}

	while (first_data_element)
	{
		DataElement *data_element = first_data_element->next;
		OP_DELETE(first_data_element);
		first_data_element = data_element;
	}

	last_data_element = NULL;

	if (is_base)
	{
		XMLParserImpl *parser = handler->GetParser();

		if (!parser->ErrorDataAvailable())
			return OpBoolean::IS_FALSE;

		const uni_char *data;
		unsigned data_length;

		do
		{
			RETURN_IF_ERROR(parser->RetrieveErrorData(data, data_length));
			RETURN_IF_ERROR(AddData(data, data_length, TRUE, TRUE));
		}
		while (data_length != 0);
	}
	else if (urldd)
	{
		while (first_data_element)
		{
			DataElement *data_element = first_data_element->next;
			OP_DELETE(first_data_element);
			first_data_element = data_element;
		}

		OP_DELETE(urldd);
		urldd = NULL;

		if (url.Status(TRUE) != URL_LOADING && url.Status(TRUE) != URL_LOADED)
			return OpBoolean::IS_FALSE;

		urldd = url.GetDescriptor(handler->GetParser()->GetMessageHandler(), TRUE);

		if (!urldd)
			return OpBoolean::IS_FALSE;

		while (TRUE)
		{
			BOOL more;

			TRAPD(status, urldd->RetrieveDataL(more));
			RETURN_IF_ERROR(status);

			const uni_char *buffer = (const uni_char *) urldd->GetBuffer();
			unsigned buffer_length = (unsigned) urldd->GetBufSize() / sizeof(uni_char);

			RETURN_IF_ERROR(AddData(buffer, buffer_length, more, TRUE));

			urldd->ConsumeData(buffer_length * sizeof(uni_char));

			if (!more || buffer_length == 0)
				break;
		}

		OP_DELETE(urldd);
		urldd = NULL;
	}
	else if (stream)
	{
		OP_DELETE(stream);

		stream = OP_NEW(UnicodeFileInputStream, ());

		if (!stream || OpStatus::IsMemoryError(((UnicodeFileInputStream *) stream)->Construct(file, URL_XML_CONTENT, TRUE)))
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(LoadFromStream());
	}

	is_at_end = TRUE;
	return OpBoolean::IS_TRUE;
}

#endif // XML_ERRORS

OP_STATUS
XMLDataSourceImpl::AddData(const uni_char *data, unsigned data_length, BOOL more, BOOL needcopy)
{
	if (data_length != 0)
	{
		OP_ASSERT(!last_data_element || last_data_element->ownsdata);

		DataElement *previous_data_element = last_data_element;

		if (needcopy && previous_data_element && previous_data_element->ownsdata && previous_data_element->data_offset >= data_length)
		{
			uni_char *source = (uni_char *) (previous_data_element->data + previous_data_element->data_offset - data_length);

			op_memmove(source, source + data_length, previous_data_element->data_length * sizeof data[0]);
			op_memcpy(source + previous_data_element->data_length, data, data_length * sizeof data[0]);

			previous_data_element->data_length += data_length;
			previous_data_element->data_offset -= data_length;
		}
		else
		{
			DataElement *data_element = OP_NEW(DataElement, ());

			if (!data_element)
				return OpStatus::ERR_NO_MEMORY;

			if (needcopy)
			{
				if (!(data_element->data = OP_NEWA(uni_char, data_length)))
				{
					OP_DELETE(data_element);
					return OpStatus::ERR_NO_MEMORY;
				}

				op_memcpy((uni_char *) data_element->data, data, data_length * sizeof data[0]);
				data_element->ownsdata = TRUE;
			}
			else
			{
				data_element->data = data;
				data_element->ownsdata = FALSE;
			}

			data_element->data_length = data_length;
			data_element->next = 0;

			if (previous_data_element)
				previous_data_element->next = data_element;
			else
				first_data_element = data_element;

			last_data_element = data_element;
		}
	}

	if (!more)
		is_at_end = TRUE;

	return OpStatus::OK;
}

OP_STATUS
XMLDataSourceImpl::BeforeParse()
{
	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	if (is_loading && urldd)
	{
		const uni_char *buffer;
		unsigned buffer_length;
		BOOL more;

		if (urldd && !force_empty)
		{
			TRAP(status, urldd->RetrieveDataL(more));

			if (OpStatus::IsSuccess(status))
			{
				buffer = (const uni_char *) urldd->GetBuffer();
				buffer_length = urldd->GetBufSize() / sizeof(uni_char);
			}
		}
		else
		{
			buffer = NULL;
			buffer_length = 0;
			more = FALSE;
		}

		if (OpStatus::IsSuccess(status))
		{
			status = AddData(buffer, buffer_length, more, FALSE);
			default_consumed = buffer_length;
		}
	}

	++busy;

	return status;
}

OP_STATUS
XMLDataSourceImpl::AfterParse(BOOL is_oom)
{
	unsigned consumed = default_consumed;
	default_consumed = 0;

	OP_STATUS status = CleanUp(is_oom, &consumed);

	if (urldd)
		urldd->ConsumeData(consumed * sizeof(uni_char));

	--busy;

	return status;
}

OP_STATUS
XMLDataSourceImpl::LoadFromStream()
{
	while (stream->has_more_data())
	{
		int length;

		uni_char *block = stream->get_block(length);

		if (!block || OpStatus::IsMemoryError(AddData(block, length / sizeof block[0], FALSE)))
			return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

static const OpMessage XMLUtilsDataSource_messages[] = {
	MSG_URL_DATA_LOADED,
	MSG_URL_LOADING_FAILED,
	MSG_URL_MOVED
};

OP_STATUS
XMLDataSourceImpl::SetCallbacks()
{
	OP_ASSERT(!url.IsEmpty());
	return handler->GetParser()->GetMessageHandler()->SetCallBackList(this, url.Id(TRUE), XMLUtilsDataSource_messages, sizeof XMLUtilsDataSource_messages / sizeof XMLUtilsDataSource_messages[0]);
}

OP_STATUS
XMLDataSourceImpl::CleanUp(BOOL is_oom, unsigned *consumed)
{
	DataElement *data_element = first_data_element, *previous_data_element = 0;
	BOOL was_oom = is_oom;

	while (data_element)
		if (is_oom)
		{
			DataElement *next_data_element = data_element->next;
			OP_DELETE(data_element);
			data_element = next_data_element;
		}
		else if (!data_element->ownsdata)
		{
			OP_ASSERT(data_element == last_data_element);

			if (consumed)
			{
				*consumed = data_element->data_offset;
				if (data_element->data_length != 0)
					is_at_end = FALSE;
			}

			if (previous_data_element)
			{
				last_data_element = previous_data_element;
				last_data_element->next = 0;
			}
			else
				first_data_element = last_data_element = 0;

			if (!consumed || need_more_data)
			{
				if (OpStatus::IsMemoryError(AddData(data_element->data + data_element->data_offset, data_element->data_length, !is_at_end, TRUE)))
					is_oom = TRUE;

				if (consumed)
					*consumed += data_element->data_length;

				need_more_data = FALSE;
			}

			OP_DELETE(data_element);

			if (is_oom && !was_oom)
				data_element = first_data_element;
			else
				data_element = 0;
		}
		else
		{
			previous_data_element = data_element;
			data_element = data_element->next;
		}

	if (is_oom)
	{
		first_data_element = last_data_element = 0;

		if (!was_oom)
			return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

BOOL
XMLDataSourceImpl::CheckExternalEntityAccess()
{
	URL parser_url = handler->GetParser()->GetURL().GetAttribute(URL::KMovedToURL, TRUE);
	URL moved_to = url.GetAttribute(URL::KMovedToURL, TRUE);

	if (parser_url.IsEmpty())
		parser_url = handler->GetParser()->GetURL();

	if (moved_to.IsEmpty())
		moved_to = url;

	ServerName *parser_sn = (ServerName *) parser_url.GetAttribute(URL::KServerName, (void *) NULL);
	ServerName *this_sn = (ServerName *) url.GetAttribute(URL::KServerName, (void *) NULL);
	ServerName *moved_to_sn = (ServerName *) moved_to.GetAttribute(URL::KServerName, (void *) NULL);

	if (!url_allowed && parser_sn != this_sn || (this_sn != parser_sn && this_sn != moved_to_sn))
	{
		unsigned window_id = 0;
		XMLParserImpl *parser = handler->GetParser();

		if (FramesDocument *doc = parser->GetDocument())
		{
			if (is_loading)
			{
				doc->StopLoadingInline(url, this);
			}

			window_id = doc->GetWindow()->Id();
		}
		else if (MessageHandler *mh = parser->GetMessageHandler())
		{
			mh->UnsetCallBacks(this);

			if (is_loading)
				url.StopLoading(mh);

			if (Window *window = mh->GetWindow())
				window_id = window->Id();
		}

#ifdef OPERA_CONSOLE
		if (g_console->IsLogging())
		{
			OpConsoleEngine::Message message;

			message.source = OpConsoleEngine::XML;
			message.severity = OpConsoleEngine::Information;
			message.window = window_id;

			if (OpStatus::IsSuccess(parser_url.GetAttribute(URL::KUniName_Username_Password_Hidden,message.url)) &&
			    OpStatus::IsSuccess(message.context.Set("This document is not allowed to load an external entity from: ")) &&
			    OpStatus::IsSuccess(message.context.Append(moved_to.GetAttribute(URL::KName_Username_Password_Hidden))))
			{
				TRAPD(status, g_console->PostMessageL(&message));
				OpStatus::Ignore(status);
			}
		}
#endif // OPERA_CONSOLE

		force_empty = TRUE;

		if (is_loading)
			LoadFromUrl();

		return FALSE;
	}
	else
	{
		if (!moved_to.IsEmpty())
			url = moved_to;

		return TRUE;
	}
}

/* static */ void
XMLDataSourceImpl::Discard(XMLDataSourceImpl *source)
{
	if (source->busy)
	{
		source->discarded = TRUE;
		source->Unregister();
	}
	else
		OP_DELETE(source);
}

/* virtual */ XMLTokenHandler::Result
XMLParserImpl::HandleToken(XMLToken &token)
{
	const XMLDocumentInformation *previous_docinfo = token.GetDocumentInformation();

	if (token.GetType() == XMLToken::TYPE_XMLDecl)
	{
		XMLVersion version;
		XMLStandalone standalone;
		const uni_char *encoding;

		if (previous_docinfo)
		{
			version = previous_docinfo->GetVersion();
			standalone = previous_docinfo->GetStandalone();
			encoding = previous_docinfo->GetEncoding();
		}
		else
		{
			version = parser->GetVersion();
			standalone = parser->GetStandalone();
			encoding = parser->GetEncoding();
		}

		if (OpStatus::IsMemoryError(information.SetXMLDeclaration(version, standalone, encoding)))
			return RESULT_OOM;

		token.SetDocumentInformation(&information);
	}
	else if (token.GetType() == XMLToken::TYPE_DOCTYPE)
	{
		XMLDoctype *doctype = parser->GetDoctype();
		const uni_char *name, *publicid, *systemid, *internalsubset;

		if (previous_docinfo)
		{
			name = previous_docinfo->GetDoctypeName();
			publicid = previous_docinfo->GetPublicId();
			systemid = previous_docinfo->GetSystemId();
			internalsubset = previous_docinfo->GetInternalSubset();
		}
		else
		{
			name = doctype->GetName();
			publicid = doctype->GetPubid();
			systemid = doctype->GetSystem();
			internalsubset = doctype->GetInternalSubsetBuffer()->GetStorage();
		}

		if (OpStatus::IsMemoryError(information.SetDoctypeDeclaration(name, publicid, systemid, internalsubset)))
			return RESULT_OOM;

		information.SetDoctype(doctype);
		token.SetDocumentInformation(&information);
	}

	XMLTokenHandler::Result result = tokenhandler->HandleToken(token);

	token.SetDocumentInformation(previous_docinfo);

	if (token.GetType() == XMLToken::TYPE_STag)
		parser->SetTokenHandler(tokenhandler, FALSE);

	return result;
}

/* Defined in xmlparserimpl.h. */
extern BOOL
XMLUtils_DoLoadExternalEntities(const URL &url, XMLParser::LoadExternalEntities load_external_entities);

OP_STATUS
XMLParserImpl::Construct()
{
 	parser = OP_NEW(XMLInternalParser, (this, &configuration));

	if (!parser)
		return OpStatus::ERR_NO_MEMORY;

	BOOL do_load_external_entities = XMLUtils_DoLoadExternalEntities(url, configuration.load_external_entities);

	parser->SetTokenHandler(this, FALSE);

	/* Normally tell the parser to at least try to load external
	   entities regardless of the setting.  We will load a local file
	   DTD that defines some commonly used entities in XHTML
	   documents, since many pages use them without considering that
	   most XML parsers do not load and read the DTD.  However, since
	   loading external entities requires the caller to implement the
	   listener interface to continue parsing, we don't do this if no
	   listener is registered and XMLParser::Parse was used. */
	parser->SetLoadExternalEntities(listener != 0 || is_standalone);

	datasourcehandler = OP_NEW(XMLDataSourceHandlerImpl, (this, do_load_external_entities));

	if (!datasourcehandler || OpStatus::IsMemoryError(datasourcehandler->Construct(url)))
		return OpStatus::ERR_NO_MEMORY;

	parser->SetDataSourceHandler(datasourcehandler, FALSE);

	return OpStatus::OK;
}

/* virtual */ void
XMLParserImpl::SetConfiguration(const Configuration &new_configuration)
{
	configuration = new_configuration;

	datasourcehandler->SetLoadExternalEntities(XMLUtils_DoLoadExternalEntities(url, configuration.load_external_entities));
}

/* virtual */ OP_STATUS
XMLParserImpl::Parse(const uni_char *data, unsigned data_length, BOOL more, unsigned *consumed)
{
	is_paused = FALSE;

	if (consumed)
		*consumed = 0;

	if (!is_blocked && !is_busy)
	{
		is_parsing = TRUE;

// Like RETURN_IF_ERROR() but sets is_parsing to FALSE before returning.
#define LOCAL_RETURN_IF_ERROR(expression) \
	do { OP_STATUS status = expression; if (OpStatus::IsError(status)) { is_parsing = FALSE; return status; } } while (0)

		BOOL first = TRUE, can_continue = FALSE;
		XMLDataSource *base = datasourcehandler->GetBaseDataSource(), *current;

		if (consumed)
			*consumed = data_length;

		unsigned iterations = 0, max_iterations = 128;

		do
		{
			current = datasourcehandler->GetCurrentDataSource();

			if (first)
			{
				first = FALSE;

				LOCAL_RETURN_IF_ERROR(((XMLDataSourceImpl *) base)->AddData(data, data_length, more, FALSE));

				if (!constructed)
				{
					constructed = TRUE;

					LOCAL_RETURN_IF_ERROR(parser->Initialize(base));
				}
			}

			if (current != base)
				LOCAL_RETURN_IF_ERROR(((XMLDataSourceImpl *) current)->BeforeParse());
			else if (!data)
			{
				is_parsing = FALSE;

				/* This typically means we were called by an external entity
				   data source.  The base data source can only be used when
				   we're called by the real user, or XMLParserImpl::LoadFromUrl
				   if this is a standalone parser. */

				Continue();
				return OpStatus::OK;
			}

			is_busy = TRUE;

			BOOL oom = FALSE;

			switch (parser->Parse(current))
			{
			case XMLInternalParser::PARSE_RESULT_OK:
				if (parser->TokenHandlerBlocked())
				{
					tokenhandler->SetSourceCallback(&sourcecallbackimpl);
					is_blocked = TRUE;
				}
				break;

			case XMLInternalParser::PARSE_RESULT_FINISHED:
				is_finished = TRUE;
				break;

			case XMLInternalParser::PARSE_RESULT_OOM:
				oom = TRUE;
				break;

			default:
				is_failed = TRUE;

#ifdef XML_ERRORS
				if (configuration.generate_error_report)
				{
					errorreport = OP_NEW(XMLErrorReport, ());

					if (!errorreport || OpStatus::IsMemoryError(parser->GenerateErrorReport(current, errorreport, configuration.error_report_context_lines)))
						oom = TRUE;
				}
#endif // XML_ERRORS
			}

			is_busy = FALSE;

			if (current != base)
				LOCAL_RETURN_IF_ERROR(((XMLDataSourceImpl *) current)->AfterParse(is_oom || oom));

			can_continue = current != datasourcehandler->GetCurrentDataSource();

			if (((XMLDataSourceImpl *) current)->IsDiscarded())
				OP_DELETE(current);

			if (oom)
			{
				is_parsing = FALSE;

				HandleOOM();
				return OpStatus::ERR_NO_MEMORY;
			}
		}
		while (can_continue && !is_finished && !is_failed && ++iterations < max_iterations);

		is_parsing = FALSE;

		if (OpStatus::IsMemoryError(((XMLDataSourceImpl *) base)->CleanUp(is_oom, consumed)))
		{
			HandleOOM();
			return OpStatus::ERR_NO_MEMORY;
		}

		if (is_finished || is_failed)
			Stopped();
		else if (!is_oom && !is_blocked)
			is_paused = parser->IsPaused() || iterations == max_iterations;
	}

	return OpStatus::OK;
}

#ifdef XMLUTILS_XMLPARSER_PROCESSTOKEN

/* virtual */ OP_STATUS
XMLParserImpl::ProcessToken(XMLToken &token)
{
	/* Cannot be used with a standalone parser. */
	OP_ASSERT(!is_standalone);

	is_paused = FALSE;

	BOOL can_process = parser->CanProcessToken();
	XMLDataSourceImpl *current = (XMLDataSourceImpl *) datasourcehandler->GetCurrentDataSource();

	if (current && current->IsLoading())
		can_process = FALSE;

	if (token.GetType() == XMLToken::TYPE_Finished || token.GetType() == XMLToken::TYPE_DOCTYPE)
		can_process = FALSE;

	if (token.GetType() == XMLToken::TYPE_PI && token.GetName() == XMLExpandedName(UNI_L("xml-stylesheet")))
		/* Force reparse, to make the XML parser interpret the processing
		   instruction's content as attributes (and possibly complain about it
		   being malformed.) */
		can_process = FALSE;

	if (!constructed)
	{
		constructed = TRUE;

		RETURN_IF_ERROR(parser->Initialize(datasourcehandler->GetBaseDataSource()));
	}

	if (can_process)
	{
		BOOL processed;

		XMLInternalParser::ParseResult result = parser->ProcessToken(current, token, processed);

		switch (result)
		{
		case XMLInternalParser::PARSE_RESULT_OK:
			if (parser->TokenHandlerBlocked())
			{
				OP_ASSERT(processed);
				tokenhandler->SetSourceCallback(&sourcecallbackimpl);
				is_blocked = TRUE;
				break;
			}
			else if (token.GetType() != XMLToken::TYPE_Finished)
			{
				is_paused = TRUE;
				break;
			}

		case XMLInternalParser::PARSE_RESULT_FINISHED:
			OP_ASSERT(processed);
			is_finished = TRUE;
			break;

		case XMLInternalParser::PARSE_RESULT_ERROR:
			is_failed = TRUE;
#ifdef XML_ERRORS
			if (configuration.generate_error_report)
			{
				errorreport = OP_NEW(XMLErrorReport, ());

				if (!errorreport || OpStatus::IsMemoryError(parser->GenerateErrorReport(current, errorreport, configuration.error_report_context_lines)))
					return OpStatus::ERR_NO_MEMORY;
			}
#endif // XML_ERRORS
			break;

		case XMLInternalParser::PARSE_RESULT_OOM:
			return OpStatus::ERR_NO_MEMORY;
		}

		if (processed || is_failed)
			return OpStatus::OK;
	}

	/* Couldn't process a token right now.  Convert the token to text,
	   and parse it/add it to the parse buffer as if Parse() was
	   called by the client code with an equivalent string. */

	TempBuffer buffer;

	ConvertTokenToString(buffer, token);

	const uni_char *data = buffer.GetStorage();

	if (!data)
		data = UNI_L("");

	return Parse(data, buffer.Length(), token.GetType() != XMLToken::TYPE_Finished, NULL);
}

#endif // XMLUTILS_XMLPARSER_PROCESSTOKEN

/* virtual */ OP_STATUS
XMLParserImpl::SignalInvalidEncodingError()
{
	is_failed = TRUE;

	XMLDataSource *current = datasourcehandler->GetCurrentDataSource();

	if (OpStatus::IsMemoryError(parser->SignalInvalidEncodingError(current)))
	{
		HandleOOM();
		return OpStatus::ERR_NO_MEMORY;
	}

#ifdef XML_ERRORS
	if (configuration.generate_error_report)
	{
		errorreport = OP_NEW(XMLErrorReport, ());

		if (!errorreport || OpStatus::IsMemoryError(parser->GenerateErrorReport(current, errorreport, configuration.error_report_context_lines)))
		{
			HandleOOM();
			return OpStatus::ERR_NO_MEMORY;
		}
	}
#endif // XML_ERRORS

	return OpStatus::OK;
}

#ifdef XML_ERRORS

/* virtual */ const XMLRange &
XMLParserImpl::GetErrorPosition()
{
	return parser->GetErrorPosition();
}

/* virtual */ void
XMLParserImpl::GetErrorDescription(const char *&error, const char *&uri, const char *&fragment_id)
{
	if (IsFailed())
	{
		parser->GetErrorDescription(error, uri, fragment_id);
	}
	else
		error = uri = fragment_id = 0;
}

#endif // XML_ERRORS

/* virtual */ URL
XMLParserImpl::GetCurrentEntityUrl()
{
	return datasourcehandler->GetCurrentDataSource()->GetURL();
}

unsigned
XMLParserImpl::GetCurrentEntityDepth()
{
	XMLDataSource *source = datasourcehandler->GetCurrentDataSource();
	unsigned depth = 0;

	while (source)
	{
		++depth;
		source = source->GetNextSource();
	}

	return depth;
}

#ifdef XML_VALIDATING

/* virtual */ OP_STATUS
XMLParserImpl::MakeValidatingTokenHandler(XMLTokenHandler *&tokenhandler, XMLTokenHandler *secondary, XMLValidator::Listener *listener)
{
	return OpStatus::ERR;
}

/* virtual */ OP_STATUS
XMLParserImpl::MakeValidatingSerializer(XMLSerializer *&serializer, XMLValidator::Listener *listener)
{
	return OpStatus::ERR;
}

#endif // XML_VALIDATING
#ifdef XML_CONFIGURABLE_DOCTYPES
# include "modules/util/opstrlst.h"
# include "modules/prefsfile/prefsfile.h"

void
XMLConfiguredDoctypes::UpdateL(BOOL &is_default)
{
	BOOL need_update = FALSE;

	if (defaultlocation)
	{
		OpFile other;
		BOOL exists;

		LEAVE_IF_ERROR(other.Construct(UNI_L("xmlentities.ini"), OPFILE_USERPREFS_FOLDER));
		LEAVE_IF_ERROR(other.Exists(exists));

		if (exists)
		{
			defaultlocation = FALSE;

			OP_DELETE(file);
			file = 0;
		}
	}

	if (!file)
	{
 try_again:
		file = OP_NEW_L(OpFile, ());
		LEAVE_IF_ERROR(file->Construct(UNI_L("xmlentities.ini"), defaultlocation ? OPFILE_INI_FOLDER : OPFILE_USERPREFS_FOLDER));
		need_update = TRUE;
	}

	BOOL exists;

	LEAVE_IF_ERROR(file->Exists(exists));

	if (!exists)
	{
		Clear();

		if (!defaultlocation)
		{
			defaultlocation = TRUE;

			OP_DELETE(file);
			file = 0;

			goto try_again;
		}
		else
			return;
	}

	if (!need_update)
	{
		time_t lastmodified0;

		LEAVE_IF_ERROR(file->GetLastModified(lastmodified0));

		if (lastmodified != lastmodified0)
		{
			lastmodified = lastmodified0;
			need_update = TRUE;
		}
	}

	if (need_update)
	{
		Clear();

		LEAVE_IF_ERROR(file->Open(OPFILE_READ | OPFILE_TEXT));

		PrefsFile prefsfile(PREFS_INI); ANCHOR(PrefsFile, prefsfile);

		prefsfile.ConstructL();
		prefsfile.SetFileL(file);

		LEAVE_IF_ERROR(prefsfile.LoadAllL());

		OpString_list sections; ANCHOR(OpString_list, sections);
		prefsfile.ReadAllSectionsL(sections);

		for (unsigned index = 0; index < sections.Count(); ++index)
		{
			XMLConfiguredDoctype *configureddoctype = OP_NEW_L(XMLConfiguredDoctype, ());
			configureddoctype->Into(this);
			configureddoctype->ConstructL(prefsfile, sections.Item(index).CStr());
		}

		LEAVE_IF_ERROR(file->Close());
	}
}

XMLConfiguredDoctypes::XMLConfiguredDoctypes()
	: defaultlocation(TRUE),
	  file(0),
	  lastmodified(0)
{
}

XMLConfiguredDoctypes::~XMLConfiguredDoctypes()
{
	Clear();
	OP_DELETE(file);
}

OP_STATUS
XMLConfiguredDoctypes::Update(BOOL &is_default)
{
	TRAPD(status, UpdateL(is_default));

	if (OpStatus::IsError(status))
		Clear();

	return status;
}

BOOL
XMLConfiguredDoctypes::Find(const uni_char *&filename, const uni_char *public_id, const uni_char *system_id, BOOL load_external_entities)
{
	XMLConfiguredDoctype *configureddoctype = (XMLConfiguredDoctype *) First();

	filename = NULL;

	while (configureddoctype)
	{
		if (configureddoctype->Match(public_id, system_id))
		{
			if (!load_external_entities || configureddoctype->AlwaysUse())
				filename = configureddoctype->GetFilename();
			return TRUE;
		}

		configureddoctype = (XMLConfiguredDoctype *) configureddoctype->Suc();
	}

	return FALSE;
}

void
XMLConfiguredDoctype::ConstructL(PrefsFile &prefsfile, const uni_char *section)
{
	prefsfile.ReadStringL(section, UNI_L("Public ID"), public_id);
	prefsfile.ReadStringL(section, UNI_L("System ID"), system_id);
	prefsfile.ReadStringL(section, UNI_L("Filename"), filename);
	always_use = prefsfile.ReadBoolL(section, UNI_L("Always Use File"));
}

BOOL
XMLConfiguredDoctype::Match(const uni_char *other_public_id, const uni_char *other_system_id)
{
	return public_id.Compare(other_public_id) == 0 || system_id.Compare(other_system_id) == 0;
}

const uni_char *
XMLConfiguredDoctype::GetFilename()
{
	return filename.CStr();
}

#endif // XML_CONFIGURABLE_DOCTYPES
