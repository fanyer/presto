/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef XSLT_SUPPORT
# include "modules/logdoc/src/xsltsupport.h"
# include "modules/logdoc/src/xmlsupport.h"
# include "modules/logdoc/logdoc.h"
# include "modules/logdoc/optreecallback.h"
# include "modules/logdoc/xmlenum.h"
# include "modules/doc/frm_doc.h"
# include "modules/security_manager/include/security_manager.h"
# include "modules/xmlutils/xmldocumentinfo.h"

class LogdocXSLTHandlerTreeCollector
	: public XSLT_Handler::TreeCollector,
	  public XMLTokenHandler,
	  public OpTreeCallback
{
public:
	static OP_STATUS Make(XSLT_Handler::TreeCollector *&tree_collector, LogicalDocument *logdoc, LogdocXSLTHandler *xslt_handler);

	virtual ~LogdocXSLTHandlerTreeCollector();

	/* From XSLT_Handler::TreeCollector: */
	virtual XMLTokenHandler *GetTokenHandler() { return this; }
	virtual void StripWhitespace(XSLT_Stylesheet *stylesheet);
	virtual OP_STATUS GetTreeAccessor(XMLTreeAccessor *&xml_treeaccessor);

	/* From XMLTokenHandler: */
	virtual Result HandleToken(XMLToken &token);

	/* From OpTreeCallback: */
	virtual OP_STATUS ElementFound(HTML_Element *element);
	virtual OP_STATUS ElementNotFound();

private:
	LogdocXSLTHandlerTreeCollector(LogicalDocument *logdoc, LogdocXSLTHandler *xslt_handler)
		: logdoc(logdoc),
		  docinfo(NULL),
		  xslt_handler(xslt_handler),
		  xml_tokenhandler(NULL),
		  root_element(NULL),
		  xml_treeaccessor(NULL)
	{
	}

	LogicalDocument *logdoc;
	URL document_url;
	XMLDocumentInformation *docinfo;
	LogdocXSLTHandler *xslt_handler;
	XMLTokenHandler *xml_tokenhandler;
	HTML_Element *root_element;
	XMLTreeAccessor *xml_treeaccessor;
};

/* static */ OP_STATUS
LogdocXSLTHandlerTreeCollector::Make(XSLT_Handler::TreeCollector *&tree_collector, LogicalDocument *logdoc, LogdocXSLTHandler *xslt_handler)
{
	LogdocXSLTHandlerTreeCollector *tc = OP_NEW(LogdocXSLTHandlerTreeCollector, (logdoc, xslt_handler));

	if (!tc || OpStatus::IsMemoryError(OpTreeCallback::MakeTokenHandler(tc->xml_tokenhandler, logdoc, tc, UNI_L (""))))
	{
		OP_DELETE(tc);
		return OpStatus::ERR_NO_MEMORY;
	}

	tree_collector = tc;
	return OpStatus::OK;
}

/* virtual */
LogdocXSLTHandlerTreeCollector::~LogdocXSLTHandlerTreeCollector()
{
	LogicalDocument::FreeXMLTreeAccessor(xml_treeaccessor);
	OP_DELETE(xml_tokenhandler);
	OP_DELETE(docinfo);
}

static void
LogdocXSLTHandlerTreeCollector_ProcessWhitespace(LogicalDocument *logdoc, XSLT_Stylesheet *stylesheet, HTML_Element *parent, BOOL strip, BOOL preserve)
{
	HTML_Element *iter = parent->Next(), *stop = static_cast<HTML_Element *>(parent->NextSibling());
	while (iter != stop)
	{
		if (Markup::IsRealElement(iter->Type()))
		{
			const uni_char *xmlspace = iter->GetStringAttr(XMLA_SPACE, NS_IDX_XML);
			if (xmlspace)
				if (preserve ? uni_strcmp(xmlspace, "preserve") == 0 : uni_strcmp(xmlspace, "default") == 0)
				{
					LogdocXSLTHandlerTreeCollector_ProcessWhitespace(logdoc, stylesheet, iter, strip, !preserve);
					iter = static_cast<HTML_Element *>(parent->NextSibling());
					continue;
				}

			XMLExpandedName name(iter);

			if (!strip != !stylesheet->ShouldStripWhitespaceIn(name))
			{
				LogdocXSLTHandlerTreeCollector_ProcessWhitespace(logdoc, stylesheet, iter, !strip, preserve);
				iter = static_cast<HTML_Element *>(iter->NextSibling());
				continue;
			}
		}
		else if (iter->IsText() && strip && !preserve)
		{
			HTML_Element *text_iter = iter, *next = NULL;
			BOOL strip_this = TRUE;

			do
			{
				if (!text_iter->HasWhiteSpaceOnly())
					strip_this = FALSE;
				next = text_iter->Next();
				text_iter = text_iter->Suc();
			}
			while (strip_this && text_iter && text_iter->IsText());

			if (strip_this)
			{
				HTML_Element *strip_iter = iter;

				while (strip_iter != text_iter)
				{
					HTML_Element *strip_next = strip_iter->Suc();
					strip_iter->OutSafe(logdoc);
					if (strip_iter->Clean(logdoc))
						strip_iter->Free(logdoc);
					strip_iter = strip_next;
				}
			}

			iter = next;
			continue;
		}

		iter = iter->Next();
	}
}

/* virtual */ void
LogdocXSLTHandlerTreeCollector::StripWhitespace(XSLT_Stylesheet *stylesheet)
{
	LogdocXSLTHandlerTreeCollector_ProcessWhitespace(logdoc, stylesheet, root_element, FALSE, FALSE);
}

/* virtual */ OP_STATUS
LogdocXSLTHandlerTreeCollector::GetTreeAccessor(XMLTreeAccessor *&xml_treeaccessor0)
{
	if (!root_element)
		/* Can only happen if ElementNotFound() fails to create a dummy
		   element. */
		return OpStatus::ERR_NO_MEMORY;

	XMLTreeAccessor::Node *dummy;
	RETURN_IF_ERROR(logdoc->CreateXMLTreeAccessor(xml_treeaccessor, dummy, root_element, NULL, docinfo));

	xml_treeaccessor0 = xml_treeaccessor;
	return OpStatus::OK;
}

/* virtual */ XMLTokenHandler::Result
LogdocXSLTHandlerTreeCollector::HandleToken(XMLToken &token)
{
	if (document_url.IsEmpty() && token.GetParser())
		document_url = token.GetParser()->GetURL();

	if ((token.GetType() == XMLToken::TYPE_XMLDecl || token.GetType() == XMLToken::TYPE_DOCTYPE))
	{
		const XMLDocumentInformation* token_docinfo = token.GetDocumentInformation();
		if (token_docinfo)
		{
			if (!docinfo && !(docinfo = OP_NEW(XMLDocumentInformation, ())))
				return RESULT_OOM;

			OP_STATUS status;
			if (token.GetType() == XMLToken::TYPE_XMLDecl)
				status = docinfo->SetXMLDeclaration(token_docinfo->GetVersion(), token_docinfo->GetStandalone(), token_docinfo->GetEncoding());
			else
			{
				status = docinfo->SetDoctypeDeclaration(token_docinfo->GetDoctypeName(), token_docinfo->GetPublicId(), token_docinfo->GetSystemId(), token_docinfo->GetInternalSubset());

				XMLDoctype* token_doctype = token_docinfo->GetDoctype();
				if (token_doctype)
					docinfo->SetDoctype(token_doctype);
			}

			if (OpStatus::IsMemoryError(status))
				return RESULT_OOM;
		}
	}

	return xml_tokenhandler->HandleToken(token);
}

/* virtual */ OP_STATUS
LogdocXSLTHandlerTreeCollector::ElementFound(HTML_Element *element)
{
	xslt_handler->SetSourceTreeRootElement(root_element = element);

#ifdef RSS_SUPPORT
	HTML_Element *child = element->FirstChildActual();
	while (child && !Markup::IsRealElement(child->Type()))
		child = child->SucActual();

	if (child)
	{
		const uni_char* localpart = child->GetTagName();
		unsigned localpart_length = uni_strlen(localpart);

		if (uni_strnicmp(localpart, "RSS", localpart_length) == 0 ||
		    uni_strnicmp(localpart, "RDF", localpart_length) == 0)
			logdoc->SetRssStatus(LogicalDocument::MAYBE_IS_RSS);
		else if (uni_strnicmp(localpart, "FEED", localpart_length) == 0 &&
		         (child->GetNsType() == NS_ATOM03 || child->GetNsType() == NS_ATOM10))
			logdoc->SetRssStatus(LogicalDocument::IS_RSS);

		if (logdoc->GetRssStatus() == LogicalDocument::MAYBE_IS_RSS)
		{
			child = child->NextActual();
			while (child)
			{
				localpart = child->GetTagName();
				if (localpart && uni_strnicmp(localpart, "CHANNEL", uni_strlen(localpart)) == 0)
				{
					logdoc->SetRssStatus(LogicalDocument::IS_RSS);
					break;
				}
				child = child->NextActual();
			}
		}
	}
#endif // RSS_SUPPORT

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
LogdocXSLTHandlerTreeCollector::ElementNotFound()
{
	root_element = NEW_HTML_Element();
	if (root_element)
		if (OpStatus::IsSuccess(root_element->Construct(logdoc->GetHLDocProfile(), NS_IDX_DEFAULT, HE_DOC_ROOT, NULL, HE_NOT_INSERTED)))
		{
			xslt_handler->SetSourceTreeRootElement(root_element);
			return OpStatus::OK;
		}
		else
			DELETE_HTML_Element(root_element);
	return OpStatus::ERR_NO_MEMORY;
}

LogdocXSLTHandler::~LogdocXSLTHandler()
{
	stylesheet_parser_elms.Clear();
	if (source_tree_root_element && source_tree_root_element->Clean(logdoc))
		source_tree_root_element->Free(logdoc);
	OP_DELETE(error_message);
}

/* virtual */ LogicalDocument *
LogdocXSLTHandler::GetLogicalDocument()
{
	return logdoc;
}

/* virtual */ URL
LogdocXSLTHandler::GetDocumentURL()
{
	return logdoc->GetFramesDocument()->GetURL();
}

/* virtual */ OP_STATUS
LogdocXSLTHandler::LoadResource(ResourceType resource_type, URL resource_url, XMLTokenHandler *token_handler)
{
	if (AllowStylesheetInclusion(resource_type, resource_url))
	{
		StylesheetParserElm *spe = OP_NEW(StylesheetParserElm, (this, resource_url));
		OP_STATUS status = OpStatus::ERR_NO_MEMORY;
		if (!spe || OpStatus::IsError(status = XMLParser::Make(spe->parser, spe, logdoc->GetFramesDocument(), token_handler, resource_url)) || OpStatus::IsError(status = spe->parser->Load(GetDocumentURL())))
		{
			OP_DELETE(spe);
			return status;
		}
		spe->resource_type = resource_type;
		spe->token_handler = token_handler;
		spe->Into(&stylesheet_parser_elms);
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

/* virtual */ void
LogdocXSLTHandler::CancelLoadResource(XMLTokenHandler *token_handler)
{
	for (StylesheetParserElm *spe = static_cast<StylesheetParserElm *>(stylesheet_parser_elms.First()); spe; spe = static_cast<StylesheetParserElm *>(spe->Suc()))
		if (spe->token_handler == token_handler)
		{
			spe->Out();
			OP_DELETE(spe);
			return;
		}
}

/* virtual */ OP_STATUS
LogdocXSLTHandler::StartCollectingSourceTree(XSLT_Handler::TreeCollector *&tree_collector)
{
	return LogdocXSLTHandlerTreeCollector::Make(tree_collector, logdoc, this);
}

/* virtual */ OP_STATUS
LogdocXSLTHandler::OnXMLOutput(XMLTokenHandler *&tokenhandler, BOOL &destroy_when_finished)
{
	tokenhandler = OP_NEW(LogdocXMLTokenHandler, (logdoc));
	if (!tokenhandler)
		return OpStatus::ERR_NO_MEMORY;
	logdoc->SetXMLTokenHandler(static_cast<LogdocXMLTokenHandler *>(tokenhandler));
	destroy_when_finished = FALSE;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
LogdocXSLTHandler::OnHTMLOutput(XSLT_Stylesheet::Transformation::StringDataCollector *&collector, BOOL &destroy_when_finished)
{
	LogdocXSLTStringDataCollector *sdc = OP_NEW(LogdocXSLTStringDataCollector, (logdoc));
	if (!sdc || OpStatus::IsMemoryError(logdoc->PrepareForHTMLOutput(sdc)))
	{
		OP_DELETE(sdc);
		return OpStatus::ERR_NO_MEMORY;
	}
	collector = sdc;
	destroy_when_finished = FALSE;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
LogdocXSLTHandler::OnTextOutput(XSLT_Stylesheet::Transformation::StringDataCollector *&collector, BOOL &destroy_when_finished)
{
	LogdocXSLTStringDataCollector *sdc = OP_NEW(LogdocXSLTStringDataCollector, (logdoc));
	if (!sdc || OpStatus::IsMemoryError(logdoc->PrepareForTextOutput(sdc)))
	{
		OP_DELETE(sdc);
		return OpStatus::ERR_NO_MEMORY;
	}
	collector = sdc;
	destroy_when_finished = FALSE;
	return OpStatus::OK;
}

/* virtual */ void
LogdocXSLTHandler::OnFinished()
{
	logdoc->CleanUpAfterXSLT(FALSE);
}

/* virtual */ void
LogdocXSLTHandler::OnAborted()
{
	logdoc->CleanUpAfterXSLT(TRUE, source_tree_root_element);
}

#ifdef XSLT_ERRORS

/* virtual */ OP_BOOLEAN
LogdocXSLTHandler::HandleMessage(XSLT_Handler::MessageType type, const uni_char *message)
{
	if (logdoc && logdoc->GetFramesDocument()->GetWindow()->GetPrivacyMode())
		return OpBoolean::IS_TRUE;  // claim we handle message, while we actually ignore it

	if (type == XSLT_Handler::MESSAGE_TYPE_ERROR)
	{
		error_message = OP_NEW(OpString, ());
		if (!error_message || OpStatus::IsMemoryError(error_message->Set(message)))
			return OpStatus::ERR_NO_MEMORY;
	}
	return OpBoolean::IS_FALSE;
}

#endif // XSLT_ERRORS

/* virtual */ void
LogdocXSLTHandler::Continue(XMLParser *parser)
{
}

/* virtual */ void
LogdocXSLTHandler::Stopped(XMLParser *parser)
{
}

/* virtual */ BOOL
LogdocXSLTHandler::Redirected(XMLParser *parser)
{
	for (StylesheetParserElm *spe = static_cast<StylesheetParserElm *>(stylesheet_parser_elms.First()); spe; spe = static_cast<StylesheetParserElm *>(spe->Suc()))
		if (spe->parser == parser)
			return AllowStylesheetInclusion(spe->resource_type, parser->GetURL().GetAttribute(URL::KMovedToURL, TRUE));

	OP_ASSERT(!"Ought not happen...");
	return FALSE;
}

/* static */ void
LogdocXSLTHandler::StripWhitespace(LogicalDocument *logdoc, HTML_Element *element, XSLT_Stylesheet *stylesheet)
{
	LogdocXSLTHandlerTreeCollector_ProcessWhitespace(logdoc, stylesheet, element, FALSE, FALSE);
}

BOOL
LogdocXSLTHandler::AllowStylesheetInclusion(ResourceType resource_type, URL resource_url)
{
	OpSecurityManager::Operation op;

	if (resource_type == RESOURCE_LOADED_DOCUMENT)
		op = OpSecurityManager::XSLT_DOCUMENT;
	else
		op = OpSecurityManager::XSLT_IMPORT_OR_INCLUDE;

	BOOL allowed = FALSE;

	return OpStatus::IsSuccess(g_secman_instance->CheckSecurity(op, logdoc->GetFramesDocument(), resource_url, allowed)) && allowed;
}

/* virtual */
LogdocXSLTHandler::StylesheetParserElm::~StylesheetParserElm()
{
	OP_DELETE(parser);
}

/* virtual */ void
LogdocXSLTHandler::StylesheetParserElm::Continue(XMLParser *parser)
{
	/* Won't be used; we use XMLParser::Load(). */
}

/* virtual */ void
LogdocXSLTHandler::StylesheetParserElm::Stopped(XMLParser *parser)
{
	/* We don't care. */
}

/* virtual */ BOOL
LogdocXSLTHandler::StylesheetParserElm::Redirected(XMLParser *parser)
{
	URL target = url.GetAttribute(URL::KMovedToURL, FALSE);

	while (!target.IsEmpty())
		if (!handler->AllowStylesheetInclusion(resource_type, target))
			return FALSE;
		else
		{
			url = target;
			target = url.GetAttribute(URL::KMovedToURL, FALSE);
		}

	return TRUE;
}

OP_BOOLEAN
LogdocXSLTStringDataCollector::ProcessCollectedData()
{
	if (buffer_length != 0 || is_finished)
		RETURN_IF_ERROR(CollectStringData(NULL, 0));

	return buffer_length == 0 && is_finished ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

/* virtual */ OP_STATUS
LogdocXSLTStringDataCollector::CollectStringData(const uni_char *string, unsigned string_length)
{
	const uni_char *data;
	unsigned data_length;

	if (buffer_length != 0)
	{
		if (string_length != 0)
		{
			RETURN_IF_ERROR(buffer.Append(string, string_length));
			buffer_length += string_length;
		}

		data = buffer.GetStorage();
		data_length = buffer_length;
	}
	else
	{
		data = string;
		data_length = string_length;
	}

	if (data_length != 0 || is_finished)
	{
		BOOL need_to_grow;

		is_calling = TRUE;
		unsigned rest = logdoc->Load(logdoc->GetFramesDocument()->GetURL(), const_cast<uni_char *>(data), data_length, need_to_grow, !is_finished, FALSE);
		is_calling = FALSE;
		OP_ASSERT(rest <= data_length);

		if (buffer_length != 0)
		{
			if (rest == 0)
				buffer.Clear();
			else
				buffer.Delete(0, buffer_length - rest);
		}
		else if (rest != 0)
		{
			buffer.Clear();
			buffer.Append(data + (data_length - rest), rest);
		}
		buffer_length = rest;
	}

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
LogdocXSLTStringDataCollector::StringDataFinished()
{
	is_finished = TRUE;
	if (buffer_length != 0)
		RETURN_IF_ERROR(CollectStringData(NULL, 0));
	return OpStatus::OK;
}

#endif // XSLT_SUPPORT
