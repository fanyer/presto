/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/logdoc/src/xmlsupport.h"
#include "modules/logdoc/src/xml.h"
#include "modules/logdoc/logdoc.h"
#include "modules/logdoc/optreecallback.h"
#include "modules/logdoc/opelementcallback.h"
#include "modules/xmlparser/xmldoctype.h"
#include "modules/doc/frm_doc.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/style/css_media.h"
#include "modules/util/tempbuf.h"
#include "modules/xmlutils/xmldocumentinfo.h"

#ifdef _WML_SUPPORT_
# include "modules/logdoc/wml.h"
#endif // _WML_SUPPORT_

#ifdef OPELEMENTCALLBACK_SUPPORT
# define HAS_ELEMENT_CALLBACK (element_callback != NULL)
#else // OPELEMENTCALLBACK_SUPPORT
# define HAS_ELEMENT_CALLBACK FALSE
#endif // OPELEMENTCALLBACK_SUPPORT

#ifdef OPTREECALLBACK_SUPPORT
# define HAS_TREE_CALLBACK (tree_callback != NULL)
#else // OPTREECALLBACK_SUPPORT
# define HAS_TREE_CALLBACK FALSE
#endif // OPTREECALLBACK_SUPPORT

LogdocXMLTokenHandler::LogdocXMLTokenHandler(LogicalDocument *logdoc)
	: logdoc(logdoc)
	, parser(NULL)
	, source_callback(NULL)
#ifdef OPELEMENTCALLBACK_SUPPORT
	, element_callback(NULL)
#endif // OPELEMENTCALLBACK_SUPPORT
#ifdef OPTREECALLBACK_SUPPORT
	, tree_callback(NULL)
	, id(NULL)
	, subtree_found(FALSE)
	, subtree_finished(FALSE)
	, current_element(NULL)
#endif // OPTREECALLBACK_SUPPORT
	, current_depth(0)
	, m_parse_elm(NULL)
{
}

LogdocXMLTokenHandler::~LogdocXMLTokenHandler()
{
#ifdef OPTREECALLBACK_SUPPORT
	OP_DELETEA(id);

	if (tree_callback && current_element)
	{
		while (HTML_Element *parent = current_element->Parent())
			current_element = parent;

		if (current_element->Clean(logdoc))
			current_element->Free(logdoc);
	}
#endif // OPTREECALLBACK_SUPPORT
}

/* virtual */ XMLTokenHandler::Result
LogdocXMLTokenHandler::HandleToken(XMLToken &token)
{
	if (!parser)
	{
		parser = token.GetParser();

		if (!m_parse_elm)
			m_parse_elm = logdoc->GetRoot();

#ifdef OPTREECALLBACK_SUPPORT
		if (tree_callback)
			if (!id)
			{
				const uni_char *fragment_id = parser->GetURL().UniRelName();

				if (fragment_id && OpStatus::IsMemoryError(UniSetStr(id, fragment_id)))
					return RESULT_OOM;
			}
			else if (!*id)
			{
				OP_DELETEA(id);
				id = NULL;
			}
#endif // OPTREECALLBACK_SUPPORT
	}

#ifdef OPTREECALLBACK_SUPPORT
	if (tree_callback && token.GetType() != XMLToken::TYPE_Finished)
	{
		if (subtree_finished)
			return RESULT_OK;

		if (id && !subtree_found && token.GetType() != XMLToken::TYPE_STag && token.GetType() != XMLToken::TYPE_EmptyElemTag)
			return RESULT_OK;
	}
#endif // OPTREECALLBACK_SUPPORT

	switch (token.GetType())
	{
	case XMLToken::TYPE_PI:
		return HandlePIToken (token);

	case XMLToken::TYPE_Comment:
	case XMLToken::TYPE_Text:
	case XMLToken::TYPE_CDATA:
		return HandleLiteralToken (token);

	case XMLToken::TYPE_DOCTYPE:
		return HandleDoctype();

	case XMLToken::TYPE_STag:
	case XMLToken::TYPE_ETag:
	case XMLToken::TYPE_EmptyElemTag:
		Result result;
		if (token.GetType() != XMLToken::TYPE_ETag)
		{
			current_depth++;
#ifdef MAX_TREE_DEPTH
			if (current_depth > MAX_TREE_DEPTH)
				return RESULT_ERROR; // Protect against running out of stack later on.
#endif //MAX_TREE_DEPTH

			result = HandleStartElementToken(token);
			if (result != RESULT_OK && result != RESULT_BLOCK)
				return result;
		}
		else
			result = RESULT_OK;
		if (token.GetType() != XMLToken::TYPE_STag)
		{
			current_depth--;
			Result result1 = HandleEndElementToken(token);
			if (result1 != RESULT_OK)
				return result1;
		}
		return result;

#ifdef XML_ENTITY_REFERENCE_TOKENS
	case XMLToken::TYPE_EntityReferenceStart:
	case XMLToken::TYPE_EntityReferenceEnd:
		// Ignore except from DOM 3 Load & Save?
		return RESULT_OK;
#endif // XML_ENTITY_REFERENCE_TOKENS

#ifdef OPTREECALLBACK_SUPPORT
	case XMLToken::TYPE_Finished:
		if (tree_callback)
		{
			OP_STATUS status;

			if (id && !subtree_found)
				status = tree_callback->ElementNotFound();
			else
			{
				status = tree_callback->ElementFound(current_element);
				current_element = NULL;
			}

			if (OpStatus::IsMemoryError(status))
				return RESULT_OOM;
		}
#endif // OPTREECALLBACK_SUPPORT
	}

    return RESULT_OK;
}

/* virtual */ void
LogdocXMLTokenHandler::SetSourceCallback(XMLTokenHandler::SourceCallback *callback)
{
	source_callback = callback;
}

void
LogdocXMLTokenHandler::ContinueIfBlocked()
{
	if (source_callback)
		source_callback->Continue(XMLTokenHandler::SourceCallback::STATUS_CONTINUE);
}

HTML_Element *
LogdocXMLTokenHandler::GetParentElement()
{
#ifdef OPELEMENTCALLBACK_SUPPORT
	if (element_callback)
		return NULL;
	else
#endif // OPELEMENTCALLBACK_SUPPORT
#ifdef OPTREECALLBACK_SUPPORT
	if (tree_callback)
		return current_element;
	else
#endif // OPTREECALLBACK_SUPPORT
		return m_parse_elm;
}

void LogdocXMLTokenHandler::RemovingElement(HTML_Element *element)
{
	if (m_parse_elm && element->IsAncestorOf(m_parse_elm))
		m_parse_elm = element->ParentActual();
}

#ifdef OPTREECALLBACK_SUPPORT
OP_STATUS
LogdocXMLTokenHandler::SetTreeCallback(OpTreeCallback *callback, const uni_char *fragment)
{
	tree_callback = callback;
	if (fragment)
		return UniSetStr(id, fragment);
	else
		return OpStatus::OK;
}
#endif // OPTREECALLBACK_SUPPORT

XMLTokenHandler::Result
LogdocXMLTokenHandler::HandleDoctype()
{
	const XMLDocumentInformation &docinfo = parser->GetDocumentInformation();

	if (!HAS_TREE_CALLBACK && !HAS_ELEMENT_CALLBACK)
		if (XMLDocumentInformation *document_info = OP_NEW(XMLDocumentInformation, ()))
			if (OpStatus::IsMemoryError(document_info->Copy(docinfo)))
			{
				OP_DELETE(document_info);
				return RESULT_OOM;
			}
			else
				logdoc->SetXMLDocumentInfo(document_info);
		else
			return RESULT_OOM;

	if (docinfo.GetDoctypeDeclarationPresent()
#ifdef OPTREECALLBACK_SUPPORT
	 	&& !id
#endif
		)
	{
		HLDocProfile *hld_profile = logdoc->GetHLDocProfile ();
		HTML_Element *element, *parent_element = GetParentElement();

		HtmlAttrEntry attributes[1];
		attributes[0].attr = ATTR_NULL;
		XMLDocumentInfoAttr *docinfoattr;

		if (!(element = NEW_HTML_Element()) ||
		    OpStatus::IsMemoryError(element->Construct(hld_profile, NS_IDX_DEFAULT, HE_DOCTYPE, attributes)) ||
		    OpStatus::IsMemoryError(XMLDocumentInfoAttr::Make(docinfoattr, &docinfo)))
		{
			DELETE_HTML_Element(element);
			return RESULT_OOM;
		}

		if (element->SetSpecialAttr(ATTR_XMLDOCUMENTINFO, ITEM_TYPE_COMPLEX, static_cast<ComplexAttr *>(docinfoattr), TRUE, SpecialNs::NS_LOGDOC) == -1)
		{
			OP_DELETE(docinfoattr);
			DELETE_HTML_Element(element);
			return RESULT_OOM;
		}

		if (!HAS_ELEMENT_CALLBACK && InsertElement(hld_profile, parent_element, element))
		{
			DELETE_HTML_Element(element);
			return RESULT_OOM;
		}

#ifdef OPELEMENTCALLBACK_SUPPORT
		if (element_callback && OpStatus::IsMemoryError(element_callback->AddElement(element)))
			return RESULT_OOM;
#endif // OPELEMENTCALLBACK_SUPPORT
	}

	return RESULT_OK;
}

XMLTokenHandler::Result
LogdocXMLTokenHandler::HandlePIToken(XMLToken &token)
{
#ifdef OPTREECALLBACK_SUPPORT
	if (tree_callback && id && !current_element)
		return RESULT_OK;
#endif // OPTREECALLBACK_SUPPORT

	HLDocProfile *hld_profile = logdoc->GetHLDocProfile ();
	HTML_Element *parent_element = GetParentElement();

	HTML_Element *element;
	HtmlAttrEntry attributes[10];

	attributes[0].attr = ATTR_TARGET;
	attributes[0].ns_idx = NS_IDX_DEFAULT;
	attributes[0].value = token.GetName().GetLocalPart();
	attributes[0].value_len = token.GetName().GetLocalPartLength();
	attributes[1].attr = ATTR_CONTENT;
	attributes[1].ns_idx = NS_IDX_DEFAULT;
	attributes[1].value = token.GetData();
	attributes[1].value_len = token.GetDataLength();
	attributes[2].attr = ATTR_NULL;

	const XMLCompleteNameN &name = token.GetName();

	if (name.GetLocalPartLength() == 14 && op_memcmp(name.GetLocalPart(), UNI_L("xml-stylesheet"), UNICODE_SIZE(14)) == 0)
	{
		HtmlAttrEntry *ptr = attributes + 2;

		const uni_char *names[6];
		names[0] = UNI_L("href");
		names[1] = UNI_L("type");
		names[2] = UNI_L("title");
		names[3] = UNI_L("media");
		names[4] = UNI_L("charset");
		names[5] = UNI_L("alternate");

		int codes[] = { ATTR_HREF, ATTR_TYPE, ATTR_TITLE, ATTR_MEDIA, ATTR_CHARSET, -ATTR_ALTERNATE };

		for (unsigned index = 0; index < (sizeof codes / sizeof codes[0]); ++index)
			if (XMLToken::Attribute *attr = token.GetAttribute(names[index]))
			{
				int code = codes[index];

				ptr->attr = code < 0 ? -code : code;
				ptr->ns_idx = code < 0 ? (int)SpecialNs::NS_LOGDOC : (int)NS_IDX_DEFAULT;
				ptr->is_special = code < 0;
				ptr->value = attr->GetValue();
				ptr->value_len = attr->GetValueLength();
				++ptr;
			}

		ptr->attr = ATTR_NULL;
	}

	if (!(element = NEW_HTML_Element()) ||
	    OpStatus::IsMemoryError(element->Construct(hld_profile, NS_IDX_HTML, HE_PROCINST, attributes)) ||
	    !HAS_ELEMENT_CALLBACK && InsertElement(hld_profile, parent_element, element))
	{
		DELETE_HTML_Element(element);
		return RESULT_OOM;
	}

#ifdef OPELEMENTCALLBACK_SUPPORT
	if (element_callback && OpStatus::IsMemoryError(element_callback->AddElement(element)))
		return RESULT_OOM;
#endif // OPELEMENTCALLBACK_SUPPORT

	return RESULT_OK;
}

BOOL
LogdocXMLTokenHandler::InsertElement(HLDocProfile *hld_profile, HTML_Element *parent_element, HTML_Element *element)
{
	BOOL oom = FALSE;

#ifdef OPTREECALLBACK_SUPPORT
	if (tree_callback)
	{
		if (!current_element)
			if (id)
			{
				current_element = element;
				return FALSE;
			}
			else
			{
				parent_element = current_element = NEW_HTML_Element();

				HtmlAttrEntry html_attrs[1];
				html_attrs[0].attr = ATTR_NULL;

				if (!parent_element || OpStatus::IsMemoryError(parent_element->Construct(hld_profile, NS_IDX_DEFAULT, HE_DOC_ROOT, html_attrs)))
				{
					if (parent_element)
						if (parent_element->Clean(hld_profile->GetFramesDocument()))
							parent_element->Free(hld_profile->GetFramesDocument());

					return TRUE;
				}
			}

		element->Under(parent_element);
	}
	else
#endif // OPTREECALLBACK_SUPPORT
	if (parent_element)
	{
		OP_HEP_STATUS status;
		if (element->IsText())
			status = hld_profile->InsertTextElement(parent_element, element);
		else
			status = hld_profile->InsertElement(parent_element, element);

		if (OpStatus::IsMemoryError(status))
			oom = TRUE;
		else if (status != HEParseStatus::STOPPED)
		{
			switch (element->Type())
			{
			case HE_TEXT:
			case HE_TEXTGROUP:
			case HE_DOCTYPE:
			case HE_PROCINST:
			case HE_COMMENT:
				break;

			default:
				m_parse_elm = element;
			}
		}
	}
	else
	{
		// No parent?
		OP_ASSERT(!"No parent_element when inserting an element. Should not be possible in XML.");
		oom = TRUE; // To trigger cleanup code
	}

	if (oom && element->Parent())
		element->OutSafe(hld_profile->GetFramesDocument());

	return oom;
}

XMLTokenHandler::Result
LogdocXMLTokenHandler::HandleStartElementToken(XMLToken &token)
{
	HLDocProfile *hld_profile = logdoc->GetHLDocProfile();
	FramesDocument *frames_doc = hld_profile->GetFramesDocument();
	HTML_Element *parent_element = GetParentElement();

	const XMLCompleteNameN &elemname = token.GetName();
	const uni_char *localpart = elemname.GetLocalPart();
	unsigned localpart_length = elemname.GetLocalPartLength();

	int elem_ns_idx = elemname.GetNsIndex();
	if (elem_ns_idx < 0)
		elem_ns_idx = NS_IDX_DEFAULT;

#ifdef _WML_SUPPORT_
	BOOL is_wml = FALSE;
	if (!HAS_TREE_CALLBACK && !HAS_ELEMENT_CALLBACK && elem_ns_idx == NS_IDX_DEFAULT && frames_doc->GetURL().ContentType() == URL_WML_CONTENT)
	{
		is_wml = TRUE;
		elem_ns_idx = NS_IDX_WML;
	}
#endif // _WML_SUPPORT_

#if defined(SVG_SUPPORT) && defined(SVG_RECOGNITION_MIMETYPE_ALLOWED)
	if (!HAS_TREE_CALLBACK && !HAS_ELEMENT_CALLBACK && elem_ns_idx == NS_IDX_DEFAULT && frames_doc->GetURL().ContentType() == URL_SVG_CONTENT)
		elem_ns_idx = NS_IDX_SVG;
#endif // SVG_SUPPORT &&

	NS_Type elem_ns_type = g_ns_manager->GetNsTypeAt(elem_ns_idx);
	int elem_type = htmLex->GetElementType(localpart, localpart_length, elem_ns_type, TRUE);

#ifdef _WML_SUPPORT_
	// some WML elements (like P and TABLE) are the same as for HTML
	// and will be stored as such
	if (is_wml && WML_Context::IsHtmlElement(static_cast<Markup::Type>(elem_type)))
	{
		elem_ns_type = NS_HTML;
		elem_ns_idx = NS_IDX_HTML;
	}
#endif // _WML_SUPPORT_

	HtmlAttrEntry *hae, *hae_ptr;
	int attributes_count = token.GetAttributesCount() + 2;

	if (attributes_count <= HtmlAttrEntriesMax)
		hae = htmLex->GetAttrArray();
	else
	{
		hae = OP_NEWA(HtmlAttrEntry, attributes_count);
		if (!hae)
			return RESULT_OOM;
	}

	XMLToken::Attribute *attr_ptr = token.GetAttributes(), *attr_ptr_end = attr_ptr + token.GetAttributesCount();
	hae_ptr = hae;

	LogdocXmlName elm_name;
	if (elem_type == HE_UNKNOWN)
	{
		if (OpStatus::IsMemoryError(elm_name.SetName(token.GetName())))
		{
			if (hae != htmLex->GetAttrArray())
				OP_DELETEA(hae);
			return RESULT_OOM;
		}

		hae_ptr->attr = ATTR_XML_NAME;
		hae_ptr->ns_idx = SpecialNs::NS_LOGDOC;
		hae_ptr->is_id = FALSE;
		hae_ptr->is_special = TRUE;
		hae_ptr->is_specified = FALSE;
		// The API transports strings, but they are stored as for instance ComplexAttrs. In this
		// case we want to give away a ComplexAttr through the API so we have to cast it to a string
		// Ugly but works until we've created a more correct way to do it.
		hae_ptr->value = reinterpret_cast<uni_char*>(&elm_name);
		hae_ptr->value_len = 0;
		++hae_ptr;
	}

	while (attr_ptr != attr_ptr_end)
	{
		const XMLCompleteNameN &attrname = attr_ptr->GetName();
		localpart = attrname.GetLocalPart();
		localpart_length = attrname.GetLocalPartLength();

		int attr_ns_idx;
		NS_Type attr_ns_type;

		if (attrname.GetPrefix() || attrname.GetLocalPartLength() == 5 && uni_strncmp(attrname.GetLocalPart(), UNI_L("xmlns"), 5) == 0)
		{
			attr_ns_idx = attrname.GetNsIndex();
			attr_ns_type = g_ns_manager->GetNsTypeAt(attr_ns_idx);
		}
		else
		{
			attr_ns_idx = NS_IDX_DEFAULT;
			attr_ns_type = elem_ns_type;
		}

		int attr_type = htmLex->GetAttrType(localpart, localpart_length, attr_ns_type, TRUE);

#ifdef _WML_SUPPORT_
		// In WML 1.x ...
		if (hld_profile->IsWml())
		{
			// ...some HTML elements can have WML attributes
			if (attr_ns_type == NS_HTML && WML_Context::IsWmlAttribute(static_cast<Markup::AttrType>(attr_type)))
			{
				if (attr_type != ATTR_XML)
					attr_ns_idx = NS_IDX_WML;
			}
			// ...some attributes are supposed to be like in HTML
			else if (attr_ns_type == NS_WML && WML_Context::IsHtmlAttribute(static_cast<Markup::AttrType>(attr_type)))
			{
				if (attr_type != ATTR_XML)
					attr_ns_idx = NS_IDX_HTML;
			}
		}
#endif // _WML_SUPPORT_

		hae_ptr->attr = attr_type;
		hae_ptr->ns_idx = attr_ns_idx;
		hae_ptr->is_id = attr_ptr->GetId();
		hae_ptr->is_specified = attr_ptr->GetSpecified();
		hae_ptr->is_special = FALSE;
		hae_ptr->value = attr_ptr->GetValue();
		hae_ptr->value_len = attr_ptr->GetValueLength();
		hae_ptr->name = localpart;
		hae_ptr->name_len = localpart_length;

#ifdef OPTREECALLBACK_SUPPORT
		if (id && !subtree_found && attr_ptr->GetId() && attr_ptr->GetValueLength() == uni_strlen(id) && uni_strncmp(id, attr_ptr->GetValue(), attr_ptr->GetValueLength()) == 0)
			subtree_found = TRUE;
#endif

		++hae_ptr;
		++attr_ptr;
	}

	hae_ptr->attr = ATTR_NULL;

#ifdef OPTREECALLBACK_SUPPORT
	if (id && !subtree_found)
	{
		if (hae != htmLex->GetAttrArray())
			OP_DELETEA(hae);
		return RESULT_OK;
	}
#endif

	HTML_Element *element;
	BOOL oom = FALSE;

	if (!(element = NEW_HTML_Element()) || OpStatus::IsMemoryError(element->Construct(hld_profile, elem_ns_idx, (HTML_ElementType) elem_type, hae)))
		oom = TRUE;
#ifdef OPELEMENTCALLBACK_SUPPORT
	else if (element_callback)
	{
		element->SetEndTagFound();
		oom = OpStatus::IsMemoryError(element_callback->AddElement(element));
	}
#endif // OPELEMENTCALLBACK_SUPPORT
	else
	{
		oom = InsertElement(hld_profile, parent_element, element);

#ifdef OPTREECALLBACK_SUPPORT
		if (tree_callback)
		{
			if (element->Parent() == current_element)
				current_element = element;
		}
#endif // OPTREECALLBACK_SUPPORT
	}

	if (hae != htmLex->GetAttrArray())
		OP_DELETEA(hae);

	if (element && !element->Parent()
#ifdef OPTREECALLBACK_SUPPORT
		&& element != current_element
		&& !HAS_ELEMENT_CALLBACK
#endif // OPTREECALLBACK_SUPPORT
		)
		if (element->Clean(frames_doc))
			element->Free(frames_doc);
	return oom ? RESULT_OOM : RESULT_OK;
}

XMLTokenHandler::Result
LogdocXMLTokenHandler::HandleEndElementToken(XMLToken &token)
{
	HLDocProfile *hld_profile = logdoc->GetHLDocProfile ();
	HTML_Element *element = GetParentElement();

	BOOL oom = FALSE, block = FALSE;

#ifdef OPELEMENTCALLBACK_SUPPORT
	if (element_callback)
		oom = OpStatus::IsMemoryError(element_callback->EndElement());
	else
#endif // OPELEMENTCALLBACK_SUPPORT
	if (element)
	{
#ifdef OPTREECALLBACK_SUPPORT
		if (tree_callback)
		{
			if (!id || subtree_found)
			{
				element->SetEndTagFound();
				if (element->Parent())
					current_element = element->Parent();
				else
					subtree_finished = TRUE;
			}
		}
		else
#endif // OPTREECALLBACK_SUPPORT
		{
			element = m_parse_elm;

			element->SetEndTagFound();

			if (element->IsMatchingType(HE_FORM, NS_HTML))
			{
				HTML_Element* parent_form = element->ParentActualStyle();
				while (parent_form && !parent_form->IsMatchingType(HE_FORM, NS_HTML))
					parent_form = parent_form->ParentActualStyle();
				hld_profile->SetNewForm(parent_form);
			}

			if (OpStatus::IsMemoryError(hld_profile->EndElement(element)))
				oom = TRUE;

			m_parse_elm = element->Parent();

			while (m_parse_elm && m_parse_elm->GetInserted() == HE_INSERTED_BY_LAYOUT)
				m_parse_elm = m_parse_elm->Parent();

			block = hld_profile->GetESLoadManager()->IsBlocked();
		}
	}

	return oom ? RESULT_OOM : block ? RESULT_BLOCK : RESULT_OK;
}

XMLTokenHandler::Result
LogdocXMLTokenHandler::HandleLiteralToken(XMLToken &token)
{
#ifdef OPTREECALLBACK_SUPPORT
	if (tree_callback && id && !current_element)
		return RESULT_OK;
#endif // OPTREECALLBACK_SUPPORT

	HLDocProfile *hld_profile = logdoc->GetHLDocProfile ();
	HTML_Element *parent_element = GetParentElement();

	BOOL oom = FALSE;

	if (parent_element || HAS_ELEMENT_CALLBACK || HAS_TREE_CALLBACK)
	{
		if (token.GetType() == XMLToken::TYPE_Comment)
		{
			HTML_Element* element = NEW_HTML_Element();

			if (!element)
				return RESULT_OOM;

			const uni_char *simplevalue = token.GetLiteralSimpleValue();
			uni_char *allocatedvalue = NULL;
			unsigned value_length = token.GetLiteralLength();

			if (!simplevalue)
				if (!(allocatedvalue = token.GetLiteralAllocatedValue()))
				{
					DELETE_HTML_Element(element);
					return RESULT_OOM;
				}

			HtmlAttrEntry attributes[2];

			attributes[0].attr = ATTR_CONTENT;
			attributes[0].ns_idx = NS_IDX_DEFAULT;
			attributes[0].value = simplevalue ? simplevalue : allocatedvalue;
			attributes[0].value_len = value_length;
			attributes[1].attr = ATTR_NULL;

			OP_STATUS status = element->Construct(hld_profile, NS_IDX_HTML, HE_COMMENT, attributes);
			OP_DELETEA(allocatedvalue);
			if (OpStatus::IsMemoryError(status))
			{
				DELETE_HTML_Element(element);
				oom = TRUE;
			}
			else
			{
#ifdef OPELEMENTCALLBACK_SUPPORT
				if (element_callback)
					oom = OpStatus::IsMemoryError(element_callback->AddElement(element));
				else
#endif // OPELEMENTCALLBACK_SUPPORT
					oom = InsertElement(hld_profile, parent_element, element);
			}
		}
		else
		{
			XMLToken::Literal literal;
			const uni_char *value = token.GetLiteralSimpleValue();
			unsigned value_length = token.GetLiteralLength(), count;

			if (!value)
			{
				if (OpStatus::IsMemoryError(token.GetLiteral(literal)))
					oom = TRUE;

				count = literal.GetPartsCount();
				if (count > 0)
				{
					value = literal.GetPart(0);
					value_length = literal.GetPartLength(0);
				}
			}
			else
				count = 1;

#ifdef _WML_SUPPORT_
			BOOL expand_wml_vars = !HAS_TREE_CALLBACK && !HAS_ELEMENT_CALLBACK
					&& token.GetType() == XMLToken::TYPE_Text
					&& hld_profile->IsWml();
#else
			const BOOL expand_wml_vars = FALSE;
#endif // _WML_SUPPORT

			HTML_Element* text = HTML_Element::CreateText(hld_profile, value, value_length, FALSE, token.GetType() == XMLToken::TYPE_CDATA, expand_wml_vars);
			if (!text)
				oom = TRUE;
			else
			{
				for (unsigned index = 1; index < count && !oom; ++index)
				{
					value = literal.GetPart(index);
					value_length = literal.GetPartLength(index);

					oom = OpStatus::IsMemoryError(text->AppendText(hld_profile, value, value_length, FALSE, token.GetType() == XMLToken::TYPE_CDATA, expand_wml_vars));
				}

				BOOL handed_over_elm = FALSE;
#ifdef OPELEMENTCALLBACK_SUPPORT
				if (element_callback)
				{
					OP_STATUS status = element_callback->AddElement(text);
					handed_over_elm = OpStatus::IsSuccess(status);
					if (!handed_over_elm)
						text = NULL; // already deleted in DOM_LSLoader::SetElement()
					oom = OpStatus::IsMemoryError(status) || oom;
				}
				else
#endif // OPELEMENTCALLBACK_SUPPORT
				{
					handed_over_elm = !InsertElement(hld_profile, parent_element, text);
					oom = !handed_over_elm || oom;
				}
				if (!handed_over_elm)
					OP_DELETE(text);
			}
		}
	}

	return oom ? RESULT_OOM : RESULT_OK;
}

/* virtual */ void
LogdocXMLParserListener::Continue(XMLParser *parser)
{
	FramesDocument *frames_doc = logdoc->GetFramesDocument();
	frames_doc->GetMessageHandler()->PostMessage(MSG_URL_DATA_LOADED, frames_doc->GetURL().Id(TRUE), 0);
}

/* virtual */ void
LogdocXMLParserListener::Stopped(XMLParser *parser)
{
	/* LogicalDocument::Load checks error conditions. */
	Continue(parser);
}

#ifdef XML_ERRORS

LogdocXMLErrorDataProvider::~LogdocXMLErrorDataProvider()
{
	OP_DELETE(urldd);
}

/* virtual */ BOOL
LogdocXMLErrorDataProvider::IsAvailable()
{
	if (logdoc->GetHLDocProfile()->GetESLoadManager()->GetScriptGeneratingDoc())
		return FALSE;

	if (logdoc->GetFramesDocument()->IsGeneratedDocument())
		return FALSE;

	return TRUE;
}

/* virtual */ OP_STATUS
LogdocXMLErrorDataProvider::RetrieveData(const uni_char *&data, unsigned &data_length)
{
	if (!urldd)
	{
		FramesDocument *doc = logdoc->GetFramesDocument();

		urldd = doc->GetURL().GetDescriptor(doc->GetMessageHandler(), TRUE, FALSE, TRUE, doc->GetWindow());

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
	data_length = previous_length = urldd->GetBufSize() / sizeof data[0];

	if (data_length == 0)
	{
		OP_DELETE(urldd);
		urldd = NULL;
	}

	return OpStatus::OK;
}

#endif // XML_ERRORS
