/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/xmlutils/src/xmlparserimpl.h"
#include "modules/xmlutils/src/xmlparserimpl_internal.h"
#include "modules/xmlutils/xmltokenhandler.h"
#include "modules/xmlutils/xmlerrorreport.h"
#include "modules/xmlutils/xmlutils.h"
#include "modules/xmlparser/xmlinternalparser.h"

#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"

BOOL
XMLUtils_DoLoadExternalEntities(const URL &url, XMLParser::LoadExternalEntities load_external_entities)
{
	switch (load_external_entities)
	{
	case XMLParser::LOADEXTERNALENTITIES_YES:
		return TRUE;

	case XMLParser::LOADEXTERNALENTITIES_NO:
		return FALSE;

	default:
		return g_pcdoc->GetIntegerPref(PrefsCollectionDoc::XMLLoadExternalEntities, url.GetServerName());
	}
}

XMLParser::Configuration::Configuration()
	: load_external_entities(XMLParser::LOADEXTERNALENTITIES_DEFAULT),
	  parse_mode(XMLParser::PARSEMODE_DOCUMENT),
	  allow_xml_declaration(TRUE),
	  allow_doctype(TRUE),
	  store_internal_subset(FALSE),
#ifdef XML_VALIDATING
	  validation_mode(XMLParser::VALIDATIONMODE_NONE),
#endif // XML_VALIDATING
	  preferred_literal_part_length(0),
#ifdef XML_ERRORS
	  generate_error_report(FALSE),
	  error_report_context_lines(7),
#endif // XML_ERRORS
	  max_tokens_per_call(4096),
	  max_nested_entity_references(XMLUTILS_DEFAULT_MAX_NESTED_ENTITY_REFERENCES)
{
}

XMLParser::Configuration::Configuration(const Configuration &other)
{
	*this = other;
}

void
XMLParser::Configuration::operator= (const Configuration &other)
{
	load_external_entities = other.load_external_entities;
	parse_mode = other.parse_mode;
	allow_xml_declaration = other.allow_xml_declaration;
	allow_doctype = other.allow_doctype;
	store_internal_subset = other.store_internal_subset;
#ifdef XML_VALIDATING
	validation_mode = other.validation_mode;
#endif // XML_VALIDATING
	preferred_literal_part_length = other.preferred_literal_part_length;
#ifdef XML_ERRORS
	generate_error_report = other.generate_error_report;
	error_report_context_lines = other.error_report_context_lines;
#endif // XML_ERRORS
	nsdeclaration = other.nsdeclaration;
	max_tokens_per_call = other.max_tokens_per_call;
}

/* static */ OP_STATUS
XMLParser::Make(XMLParser *&parser, Listener *listener, FramesDocument *document, XMLTokenHandler *tokenhandler, const URL &url)
{
	XMLParserImpl *impl = OP_NEW(XMLParserImpl, (listener, document, document->GetMessageHandler(), tokenhandler, url));

	if (!impl || OpStatus::IsMemoryError(impl->Construct()) || OpStatus::IsMemoryError(impl->GetMessageHandler()->SetCallBack(impl, MSG_XMLUTILS_CONTINUE, (MH_PARAM_1) impl)))
	{
		OP_DELETE(impl);
		return OpStatus::ERR_NO_MEMORY;
	}

	parser = impl;
	return OpStatus::OK;
}

/* static */ OP_STATUS
XMLParser::Make(XMLParser *&parser, Listener *listener, MessageHandler *mh, XMLTokenHandler *tokenhandler, const URL &url)
{
	XMLParserImpl *impl = OP_NEW(XMLParserImpl, (listener, 0, mh, tokenhandler, url));

	if (!impl || OpStatus::IsMemoryError(impl->Construct()) || mh && OpStatus::IsMemoryError(mh->SetCallBack(impl, MSG_XMLUTILS_CONTINUE, (MH_PARAM_1) impl)))
	{
		OP_DELETE(impl);
		return OpStatus::ERR_NO_MEMORY;
	}

	parser = impl;
	return OpStatus::OK;
}

/* virtual */
XMLParser::~XMLParser()
{
}

/* virtual */ void
XMLParserImpl::SourceCallbackImpl::Continue(XMLTokenHandler::SourceCallback::Status status)
{
	parser->Continue(status);
}

static const OpMessage XMLUtils_messages[] = {
	MSG_URL_DATA_LOADED,
	MSG_URL_LOADING_FAILED,
	MSG_URL_MOVED
};

OP_STATUS
XMLParserImpl::SetCallbacks()
{
	return mh->SetCallBackList(this, url.Id(TRUE), XMLUtils_messages, sizeof XMLUtils_messages / sizeof XMLUtils_messages[0]);
}

OP_STATUS
XMLParserImpl::SetLoadingTimeout(unsigned timeout_ms)
{
	OP_ASSERT(!is_waiting_for_timeout);
	if (is_waiting_for_timeout)
		CancelLoadingTimeout();

	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_XMLUTILS_LOAD_TIMEOUT, (MH_PARAM_1)this));
	if (!mh->PostDelayedMessage(MSG_XMLUTILS_LOAD_TIMEOUT, (MH_PARAM_1)this, 0, timeout_ms))
		return OpStatus::ERR_NO_MEMORY;

	is_waiting_for_timeout = TRUE;

	return OpStatus::OK;
}

void
XMLParserImpl::CancelLoadingTimeout()
{
	if (mh)
		mh->RemoveDelayedMessage(MSG_XMLUTILS_LOAD_TIMEOUT, (MH_PARAM_1)this, 0);
	is_waiting_for_timeout = FALSE;
}

void
XMLParserImpl::LoadFromUrl()
{
	if (is_waiting_for_timeout)
		CancelLoadingTimeout();

	if (!is_finished)
	{
		OP_STATUS status = OpStatus::OK;
		BOOL more;

		if (is_calling_loadinline)
			more = TRUE;
		else
		{
			OP_ASSERT(!url.IsEmpty());

			if (url.Type() != URL_DATA && !url.GetAttribute(URL::KHeaderLoaded, TRUE))
				return;

			if (!urldd)
			{
				if (url.Type() == URL_HTTP || url.Type() == URL_HTTPS)
					http_response = url.GetAttribute(URL::KHTTP_Response_Code, TRUE);
				else
					http_response = HTTP_OK;

				if (http_response != HTTP_OK && http_response != HTTP_NOT_MODIFIED)
				{
					is_failed = TRUE;
					Stopped();
					return;
				}

				urldd = url.GetDescriptor(mh, TRUE, FALSE, TRUE, document ? document->GetWindow() : NULL, URL_XML_CONTENT);

				if (!urldd)
				{
					OpFileLength content_size = 0;

					url.GetAttribute(URL::KContentSize, &content_size, TRUE);

					if (url.Status(TRUE) == URL_LOADED && content_size == 0)
					{
						if (OpStatus::IsMemoryError(Parse(UNI_L(""), 0, FALSE, NULL)))
							HandleOOM();

						if (is_finished && !is_failed)
							Stopped();
					}

					return;
				}

				parsed_url = url.GetAttribute(URL::KMovedToURL, TRUE);
			}

			TRAP(status, urldd->RetrieveDataL(more));

			unsigned consumed;

			if (OpStatus::IsMemoryError(Parse((const uni_char *) urldd->GetBuffer(), urldd->GetBufSize() / sizeof(uni_char), more, &consumed)))
			{
				HandleOOM();
				return;
			}
			else if (urldd)
				urldd->ConsumeData(consumed * sizeof(uni_char));

			if (is_finished && !is_failed)
				Stopped();
		}

		if (OpStatus::IsSuccess(status) && (more && is_loading_stopped || is_paused))
			mh->PostMessage(MSG_XMLUTILS_CONTINUE, (MH_PARAM_1) this, 0);
	}
}

/* virtual */ void
XMLParserImpl::LoadingProgress(const URL &url)
{
	LoadFromUrl();
}

/* virtual */ void
XMLParserImpl::LoadingStopped(const URL &url)
{
	is_loading_stopped = TRUE;
	LoadFromUrl();

	if (!document && delete_when_finished)
	{
		XMLParserImpl *self = this;
		OP_DELETE(self);
	}
}

/* virtual */ void
XMLParserImpl::LoadingRedirected(const URL &from, const URL &to)
{
	if (!listener->Redirected(this))
	{
		is_failed = TRUE;
		Stopped();
	}
}

/* virtual */ void
XMLParserImpl::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
	case MSG_XMLUTILS_CONTINUE:
		if (par1 != (MH_PARAM_1) this)
			break;

	case MSG_URL_DATA_LOADED:
		LoadFromUrl();
		break;

	case MSG_XMLUTILS_LOAD_TIMEOUT:
	case MSG_URL_LOADING_FAILED:
		is_failed = TRUE;
		Stopped();
		break;

	case MSG_URL_MOVED:
		if (listener && !listener->Redirected(this))
		{
			is_failed = TRUE;
			Stopped();
		}
		else if (OpStatus::IsMemoryError(SetCallbacks()))
			HandleOOM();
		break;
	}

	if (!mh && delete_when_finished)
	{
		XMLParserImpl *self = this;
		OP_DELETE(self);
	}
}

void
XMLParserImpl::Continue(XMLTokenHandler::SourceCallback::Status status)
{
	is_blocked = FALSE;

	if (status == XMLTokenHandler::SourceCallback::STATUS_CONTINUE)
	{
		if (is_standalone || datasourcehandler->GetBaseDataSource() != datasourcehandler->GetCurrentDataSource())
			mh->PostMessage(MSG_XMLUTILS_CONTINUE, (MH_PARAM_1) this, 0);
		else if (listener)
			listener->Continue(this);
	}
	else
	{
		if (status == XMLTokenHandler::SourceCallback::STATUS_ABORT_FAILED)
			is_failed = TRUE;
		else
			is_oom = TRUE;

		Stopped();
	}
}

#ifdef XMLUTILS_XMLPARSER_PROCESSTOKEN

static void
AppendQNameL(TempBuffer &buffer, const XMLCompleteNameN &name)
{
	unsigned prefix_length = name.GetPrefixLength();

	if (prefix_length != 0)
	{
		buffer.AppendL(name.GetPrefix(), prefix_length);
		buffer.AppendL(":");
	}

	buffer.AppendL(name.GetLocalPart(), name.GetLocalPartLength());
}

void
XMLParserImpl::ConvertTokenToStringL(TempBuffer &buffer, XMLToken &token)
{
	const char *prefix = 0, *suffix = 0;

	switch (token.GetType())
	{
	case XMLToken::TYPE_XMLDecl:
	{
		const XMLDocumentInformation* docinfo = token.GetDocumentInformation();
		if (docinfo)
			if (docinfo->GetXMLDeclarationPresent())
			{
				buffer.AppendL("<?xml version='");
				buffer.AppendL(docinfo->GetVersion() == XMLVERSION_1_1 ? "1.1" : "1.0");

				const uni_char *encoding = docinfo->GetEncoding();
				if (encoding)
				{
					buffer.AppendL("' encoding='");
					buffer.AppendL(encoding);
				}

				XMLStandalone standalone = docinfo->GetStandalone();
				if (standalone != XMLSTANDALONE_NONE)
				{
					buffer.AppendL("' standalone='");
					buffer.AppendL(standalone == XMLSTANDALONE_YES ? "yes" : "no");
				}

				buffer.AppendL("'?>");
				return;
			}

		if (XMLToken::Attribute *version = token.GetAttribute(UNI_L("version")))
		{
			buffer.AppendL("<?xml version='");
			buffer.AppendL(version->GetValue(), version->GetValueLength());

			if (XMLToken::Attribute *encoding = token.GetAttribute(UNI_L("encoding")))
			{
				buffer.AppendL("' encoding='");
				buffer.AppendL(encoding->GetValue(), encoding->GetValueLength());
			}

			if (XMLToken::Attribute *standalone = token.GetAttribute(UNI_L("standalone")))
			{
				buffer.AppendL("' standalone='");
				buffer.AppendL(standalone->GetValue(), standalone->GetValueLength());
			}

			buffer.AppendL("'?>");
			return;
		}

		if (!token.GetData())
			return;

		/* fall through */
	}

	case XMLToken::TYPE_PI:
		buffer.AppendL("<?");
		buffer.AppendL(token.GetName().GetLocalPart(), token.GetName().GetLocalPartLength());
		buffer.AppendL(" ");
		buffer.AppendL(token.GetData(), token.GetDataLength());
		buffer.AppendL("?>");
		return;

	case XMLToken::TYPE_Comment:
		prefix = "<!--";
		suffix = "-->";
		break;

	case XMLToken::TYPE_CDATA:
		prefix = "<![CDATA[";
		suffix = "]]>";
		break;

	case XMLToken::TYPE_DOCTYPE:
	{
		const XMLDocumentInformation* docinfo = token.GetDocumentInformation();
		if (docinfo && docinfo->GetDoctypeDeclarationPresent())
		{
			buffer.AppendL("<!DOCTYPE ");
			buffer.AppendL(docinfo->GetDoctypeName());

			const uni_char *publicid = docinfo->GetPublicId(), *systemid = docinfo->GetSystemId();
			if (publicid && systemid)
			{
				buffer.AppendL(" PUBLIC ");
				const char *publicid_quote = "\"";
				if (uni_strchr(publicid, *publicid_quote) != NULL)
					publicid_quote = "'";
				buffer.AppendL(publicid_quote);
				buffer.AppendL(publicid);
				buffer.AppendL(publicid_quote);
				buffer.AppendL(" ");
				const char *systemid_quote = "\"";
				if (uni_strchr(systemid, *systemid_quote) != NULL)
					systemid_quote = "'";
				buffer.AppendL(systemid_quote);
				buffer.AppendL(systemid);
				buffer.AppendL(systemid_quote);
			}
			else if (systemid)
			{
				buffer.AppendL(" SYSTEM ");
				const char *quote = "\"";
				if (uni_strchr(systemid, *quote) != NULL)
					quote = "'";
				buffer.AppendL(quote);
				buffer.AppendL(systemid);
				buffer.AppendL(quote);
			}

			const uni_char *internalsubset = docinfo->GetInternalSubset();
			if (internalsubset)
			{
				buffer.AppendL(" [");
				buffer.AppendL(internalsubset);
				buffer.AppendL("]");
			}

			buffer.AppendL(">");
		}
		return;
	}

	case XMLToken::TYPE_STag:
	case XMLToken::TYPE_EmptyElemTag:
		buffer.AppendL("<");
		AppendQNameL(buffer, token.GetName());
		if (token.GetAttributesCount() != 0)
		{
			XMLVersion version = information.GetVersion();

			for (unsigned index = 0; index < token.GetAttributesCount(); ++index)
			{
				XMLToken::Attribute &attribute = token.GetAttributes()[index];

				buffer.AppendL(" ");
				AppendQNameL(buffer, attribute.GetName());
				buffer.AppendL("=");

				const uni_char *value = attribute.GetValue();
				unsigned value_length = attribute.GetValueLength(), cindex;

				BOOL includes_quot = FALSE, includes_apos = FALSE;

				for (cindex = 0; cindex < value_length; ++cindex)
					if (value[cindex] == '"')
						includes_quot = TRUE;
					else if (value[cindex] == '\'')
						includes_apos = TRUE;

				unsigned quote;
				if (!includes_quot || includes_apos)
					quote = '\"';
				else
					quote = '\'';

				buffer.AppendL(quote);

				cindex = 0;

				while (cindex < value_length)
				{
					unsigned start = cindex, ch = 0;

					while (cindex < value_length && (ch = value[cindex]) && !(ch == '<' || ch == '&' || ch == '"' || ch != ' ' && XMLUtils::IsSpaceExtended(version, ch)))
						++cindex;

					buffer.AppendL(value + start, cindex - start);

					if (cindex < value_length)
					{
						const char *entityreference;
						char temporary[10]; // ARRAY OK jl 2008-06-19

						switch (value[cindex])
						{
						case '<':
							entityreference = "&lt;";
							break;

						case '&':
							entityreference = "&amp;";
							break;

						case '"':
							entityreference = "&quot;";
							break;

						default:
							op_sprintf(temporary, "&#x%x;", ch);
							entityreference = temporary;
						}

						buffer.AppendL(entityreference);
						++cindex;
					}
				}

				buffer.AppendL(quote);
			}
		}
		if (token.GetType() == XMLToken::TYPE_STag)
			buffer.AppendL(">");
		else
			buffer.AppendL("/>");
		return;

	case XMLToken::TYPE_ETag:
		buffer.AppendL("</");
		AppendQNameL(buffer, token.GetName());
		buffer.AppendL(">");
		return;

	case XMLToken::TYPE_Text:
		break;

	default:
		return;
	}

	if (prefix)
		buffer.AppendL(prefix);

	const uni_char *simplevalue = token.GetLiteralSimpleValue();
	const uni_char *allocatedvalue = 0;
	if (simplevalue)
		buffer.AppendL(simplevalue, token.GetLiteralLength());
	else if ((allocatedvalue = token.GetLiteralAllocatedValue()) != NULL)
	{
		OP_STATUS status = buffer.Append(allocatedvalue, token.GetLiteralLength());
		OP_DELETEA(allocatedvalue);
		LEAVE_IF_ERROR(status);
	}
	else
		LEAVE(OpStatus::ERR_NO_MEMORY);
	if (suffix)
		buffer.AppendL(suffix);
}

OP_STATUS
XMLParserImpl::ConvertTokenToString(TempBuffer &buffer, XMLToken &token)
{
	TRAPD(status, ConvertTokenToStringL(buffer, token));
	return status;
}

#endif // XMLUTILS_XMLPARSER_PROCESSTOKEN

XMLParserImpl::XMLParserImpl(Listener *listener, FramesDocument *document, MessageHandler *mh, XMLTokenHandler *tokenhandler, const URL &url)
	: listener(listener),
	  document(document),
	  mh(mh),
	  tokenhandler(tokenhandler),
	  url(url),
	  parsed_url(url),
	  urldd(0),
	  is_standalone(FALSE),
	  is_loading_stopped(FALSE),
	  is_listener_signalled(FALSE),
	  is_calling_loadinline(FALSE),
	  is_blocked(FALSE),
	  is_busy(FALSE),
	  is_parsing(FALSE),
	  delete_when_finished(FALSE),
	  owns_listener(FALSE),
	  owns_tokenhandler(FALSE),
	  is_waiting_for_timeout(FALSE),
	  is_finished(FALSE),
	  is_failed(FALSE),
	  is_oom(FALSE),
	  is_paused(FALSE),
	  http_response(0),
#ifdef XML_ERRORS
	  errorreport(0),
	  errordataprovider(0),
#endif // XML_ERRORS
	  parser(0),
	  datasourcehandler(0),
	  constructed(FALSE)
{
	sourcecallbackimpl.parser = this;
}

XMLParserImpl::~XMLParserImpl()
{
	if (mh)
		mh->UnsetCallBacks(this);

	CancelLoadingTimeout();

	if (!is_listener_signalled)
		tokenhandler->ParsingStopped(this);

	OP_DELETE(parser);
	OP_DELETE(datasourcehandler);

	if (is_standalone && document)
		document->StopLoadingInline(url, this);

#ifdef XML_ERRORS
	OP_DELETE(errorreport);
#endif // XML_ERRORS

	if (owns_listener)
		OP_DELETE(listener);

	if (owns_tokenhandler)
		OP_DELETE(tokenhandler);

	OP_DELETE(urldd);
}

/* virtual */ void
XMLParserImpl::SetOwnsListener()
{
	owns_listener = TRUE;
}

/* virtual */ void
XMLParserImpl::SetOwnsTokenHandler()
{
	owns_tokenhandler = TRUE;
}

/* virtual */ OP_STATUS
XMLParserImpl::Load(const URL &referrer_url, BOOL new_delete_when_finished, unsigned load_timeout, BOOL bypass_proxy)
{
	if (!document && !mh)
		return OpStatus::ERR;

	if (load_timeout)
		RAISE_IF_MEMORY_ERROR(SetLoadingTimeout(load_timeout));

	is_standalone = TRUE;

	OP_STATUS status = OpStatus::ERR;

	if (document)
	{
		is_calling_loadinline = TRUE;
		OP_LOAD_INLINE_STATUS load_status = document->LoadInline(url, this);
		is_calling_loadinline = FALSE;

		if (load_status == LoadInlineStatus::LOADING_CANCELLED || load_status == LoadInlineStatus::LOADING_REFUSED)
		{
			is_failed = TRUE;
			status = OpStatus::ERR;
			goto cancel_load;
		}

		if (OpStatus::IsError(load_status))
		{
			status = load_status;
			goto cancel_load;
		}

		url_loading.SetURL(url);
	}
	else
	{
		status = SetCallbacks();
		if (OpStatus::IsError(status))
			goto cancel_load;

		URL_LoadPolicy loadpolicy(FALSE, URL_Reload_If_Expired, FALSE, FALSE, bypass_proxy);

		url.SetAttribute(URL::KHTTP_Priority, FramesDocument::LOAD_PRIORITY_MAIN_DOCUMENT);

		CommState state = url.LoadDocument(mh, referrer_url, loadpolicy);

		if (state != COMM_LOADING)
		{
			status = OpStatus::ERR;
			is_failed = TRUE;
			goto cancel_load;
		}

		url_loading.SetURL(url);
	}


	delete_when_finished = new_delete_when_finished;
	return OpStatus::OK;

cancel_load:
	mh->UnsetCallBacks(this);
	CancelLoadingTimeout();
	return status;
}

/* virtual */ BOOL
XMLParserImpl::IsFinished()
{
	return is_finished;
}

/* virtual */ BOOL
XMLParserImpl::IsFailed()
{
	return is_failed;
}

/* virtual */ BOOL
XMLParserImpl::IsOutOfMemory()
{
	return is_oom;
}

/* virtual */ BOOL
XMLParserImpl::IsPaused()
{
	return is_paused;
}

/* virtual */ BOOL
XMLParserImpl::IsBlockedByTokenHandler()
{
	return is_blocked;
}

/* virtual */ URL
XMLParserImpl::GetURL()
{
	return url;
}

/* virtual */ const XMLDocumentInformation &
XMLParserImpl::GetDocumentInformation()
{
	return information;
}

/* virtual */ XMLVersion
XMLParserImpl::GetXMLVersion()
{
	return information.GetVersion();
}

/* virtual */ XMLStandalone
XMLParserImpl::GetXMLStandalone()
{
	return information.GetStandalone();
}

void
XMLParserImpl::Continue()
{
	if (is_standalone)
		mh->PostMessage(MSG_URL_DATA_LOADED, url.Id(TRUE), 0);
	else if (listener && !is_parsing)
		listener->Continue(this);
}

void
XMLParserImpl::Stopped()
{
	if (is_standalone)
	{
		if (document)
		{
			document->StopLoadingInline(url, this);

			document = NULL;
		}

		if (mh)
		{
			mh->UnsetCallBacks(this);
			mh = NULL;
		}

		url_loading.UnsetURL();
	}

	if (!is_listener_signalled)
	{
		if (listener)
			listener->Stopped(this);

		tokenhandler->ParsingStopped(this);

		is_listener_signalled = TRUE;
	}

	OP_DELETE(urldd);
	urldd = NULL;
}

void
XMLParserImpl::HandleOOM()
{
	is_failed = is_oom = TRUE;
	Stopped();
}

#ifdef XML_ERRORS

BOOL
XMLParserImpl::ErrorDataAvailable()
{
	if (is_standalone)
	{
		OP_DELETE(urldd);
		urldd = 0;

		return url.Status(TRUE) == URL_LOADING || url.Status(TRUE) == URL_LOADED;
	}
	else if (errordataprovider)
		return errordataprovider->IsAvailable();
	else
		return FALSE;
}

OP_STATUS
XMLParserImpl::RetrieveErrorData(const uni_char *&data, unsigned &data_length)
{
	if (is_standalone)
	{
		if (!urldd)
		{
			urldd = url.GetDescriptor(mh, TRUE);

			if (!urldd)
			{
				data_length = 0;
				return OpStatus::OK;
			}
		}
		else
			urldd->ConsumeData(previous_length * sizeof data[0]);

		BOOL more;

		TRAPD(status, urldd->RetrieveDataL(more));
		RETURN_IF_ERROR(status);

		data = (const uni_char *) urldd->GetBuffer();
		data_length = previous_length = urldd->GetBufSize() / sizeof(uni_char);

		return OpStatus::OK;
	}
	else
		return errordataprovider->RetrieveData(data, data_length);
}

/* virtual */ const XMLErrorReport *
XMLParserImpl::GetErrorReport()
{
	return errorreport;
}

/* virtual */ XMLErrorReport *
XMLParserImpl::TakeErrorReport()
{
	XMLErrorReport *report = errorreport;
	errorreport = 0;
	return report;
}

/* virtual */ void
XMLParserImpl::SetErrorDataProvider(ErrorDataProvider *provider)
{
	errordataprovider = provider;
}

#endif // XML_ERRORS


#ifdef OPERA_CONSOLE

#ifdef XML_ERRORS
void
XMLParserImpl::AddDetailedReportL(OpString &message)
{
	if (errorreport)
	{
		message.AppendL("\n\n");
		int err_start = -1, info_start = -1;
		OpString current_line;
		for (unsigned item_index = 0; item_index < errorreport->GetItemsCount(); item_index++)
		{
			const XMLErrorReport::Item *item = errorreport->GetItem(item_index);
			if (item->GetType() == XMLErrorReport::Item::TYPE_LINE_END || item->GetType() == XMLErrorReport::Item::TYPE_LINE_FRAGMENT)
				current_line.AppendL(item->GetString());
			if (item->GetType() == XMLErrorReport::Item::TYPE_ERROR_START)
				err_start = current_line.Length();
			else if (item->GetType() == XMLErrorReport::Item::TYPE_INFORMATION_START)
				info_start = current_line.Length();
			if (item->GetType() == XMLErrorReport::Item::TYPE_LINE_END)
			{
				if (err_start > -1 || info_start > -1)
				{
					/* Source line */
					OpString linenr;
					LEAVE_IF_ERROR(linenr.AppendFormat("%3u: ", item->GetLine() + 1));
					message.AppendL(linenr);
					LEAVE_IF_ERROR(message.AppendFormat("%s\n", current_line.CStr()));
					/* Markers */
					int first = MIN(err_start, info_start);
					if (first > -1)
						first += linenr.Length();
					int last = MAX(err_start, info_start) + linenr.Length();
					for (int ex = 0; ex < last + 1; ex++)
						if (ex == first || ex == last)
							message.AppendL("^");
						else
							message.AppendL("-");
					message.AppendL("\n");
				}
				current_line.Empty();
				err_start = info_start = -1;
			}
		}
	}
}

void
XMLParserImpl::AddErrorDescriptionL(OpString &message)
{
	const char *error, *uri, *fragmentid;
	GetErrorDescription(error, uri, fragmentid);

	OpString description, href;
	description.AppendL(error);
	href.AppendL(uri);
	if (fragmentid)
	{
		href.AppendL("#");
		href.AppendL(fragmentid);
	}

	LEAVE_IF_ERROR(message.AppendFormat("\nError: %s\n\nSpecification: %s", description.CStr(), href.CStr()));
}
#endif // XML_ERRORS

void
XMLParserImpl::ReportConsoleErrorL()
{
	if (!g_console->IsLogging())
		return;

	if (!IsFailed())
		return;

	OpConsoleEngine::Message cmessage(OpConsoleEngine::XML, OpConsoleEngine::Error);
	ANCHOR(OpConsoleEngine::Message, cmessage);
	cmessage.window = GetDocument() ? GetDocument()->GetWindow()->Id() : 0;
	URL url = GetURL();
	url.GetAttributeL(URL::KUniName, cmessage.url);

	OpString message;

#ifdef XML_ERRORS
	XMLRange position = GetErrorPosition();
	BOOL hasposition = position.start.IsValid();
	URL entityurl = GetCurrentEntityUrl();

	if (!(url == entityurl) && !entityurl.IsEmpty())
	{
		LEAVE_IF_ERROR(cmessage.context.AppendFormat(UNI_L("External entity: %s; "), entityurl.GetAttribute(URL::KUniName_Username_Password_Hidden).CStr()));
		if (hasposition)
			cmessage.context.AppendL("; ");
	}

	if (hasposition)
	{
		LEAVE_IF_ERROR(cmessage.context.AppendFormat(UNI_L("Tag at line %d, column %d"), position.start.line + 1, position.start.column));

		if (position.end.IsValid() && position.end.line != position.start.line)
			LEAVE_IF_ERROR(cmessage.context.AppendFormat(UNI_L(" to line %d"), position.end.line + 1));
	}

	LEAVE_IF_ERROR(message.AppendFormat("XML parsing failed:"));

	AddDetailedReportL(message);
	AddErrorDescriptionL(message);
#else // XML_ERRORS
	message.SetL("XML parsing failed");
	cmessage.context.SetL("XML parsing");
#endif // XML_ERRORS

	cmessage.message.SetL(message);
	g_console->PostMessageL(&cmessage);
}

/* virtual */ OP_STATUS
XMLParserImpl::ReportConsoleError()
{
	TRAPD(status, ReportConsoleErrorL());
	return status;
}

static void
XMLUtils_ReportConsoleMessageL(OpConsoleEngine::Severity severity, URL url, URL entityurl, int window_id, const XMLRange &position, const uni_char *string)
{
	if (!g_console->IsLogging())
		return;

	OpConsoleEngine::Message message(OpConsoleEngine::XML, severity, window_id); ANCHOR(OpConsoleEngine::Message, message);

	if (!url.IsEmpty())
		url.GetAttributeL(URL::KUniName_Username_Password_Hidden, message.url);

	BOOL hasposition = position.start.IsValid();

	if (!(url == entityurl) && !entityurl.IsEmpty())
	{
		LEAVE_IF_ERROR(message.context.AppendFormat(UNI_L("External entity: %s; "), entityurl.GetAttribute(URL::KUniName_Username_Password_Hidden).CStr()));
		if (hasposition)
			message.context.AppendL("; ");
	}

	if (hasposition)
	{
		LEAVE_IF_ERROR(message.context.AppendFormat(UNI_L("Tag at line %d, column %d"), position.start.line + 1, position.start.column));

		if (position.end.IsValid())
			LEAVE_IF_ERROR(message.context.AppendFormat(UNI_L(" to line %d, column %d"), position.end.line + 1, position.end.column));
	}

	message.message.SetL(string);

	g_console->PostMessageL(&message);
}

/* virtual */ OP_STATUS
XMLParserImpl::ReportConsoleMessage(OpConsoleEngine::Severity severity, const XMLRange &position, const uni_char *message)
{
	TRAPD(status, XMLUtils_ReportConsoleMessageL(severity, url, GetCurrentEntityUrl(), document ? document->GetWindow()->Id() : 0, position, message));
	return status;
}

#endif // OPERA_CONSOLE
