/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef XMLUTILS_XMLSERIALIZER_SUPPORT

#include "modules/xmlutils/src/xmlserializerimpl.h"
#include "modules/xmlutils/xmldocumentinfo.h"
#include "modules/xmlutils/xmllanguageparser.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmlutils.h"
#include "modules/encodings/encoders/outputconverter.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/xmlenum.h"
#include "modules/util/tempbuf.h"
#include "modules/util/str.h"

XMLSerializer::Configuration::Configuration()
	: document_information(0),
	  split_cdata_sections(TRUE),
	  normalize_namespaces(TRUE),
	  discard_default_content(TRUE),
	  format_pretty_print(FALSE),
	  preferred_line_length(80),
	  use_decimal_character_reference(FALSE),
	  add_xml_space_attributes(FALSE),
	  encoding(0)
{
}

/* virtual */
XMLSerializer::~XMLSerializer()
{
}

XMLSerializerBackend::XMLSerializerBackend()
	: attributes(NULL),
	  content(NULL)
{
}

XMLSerializerBackend::~XMLSerializerBackend()
{
}

XMLElementSerializerBackend::XMLElementSerializerBackend(HTML_Element *root, HTML_Element *target)
	: root_element(root),
	  current_element(root),
	  target_element(target),
	  pretend_root(root->Type() != HE_DOC_ROOT)
{
}

static XMLWhitespaceHandling
XMLElementSerializerBackend_GetWhitespaceHandling(HTML_Element *element)
{
	while (element)
		if (element->HasAttr(XMLA_SPACE, NS_IDX_XML))
		{
			if (element->GetBoolAttr(XMLA_SPACE, NS_IDX_XML))
				return XMLWHITESPACEHANDLING_PRESERVE;
			else
				return XMLWHITESPACEHANDLING_DEFAULT;
		}
		else
			element = element->ParentActual();

	return XMLWHITESPACEHANDLING_DEFAULT;
}

/* virtual */ BOOL
XMLElementSerializerBackend::StepL(StepType where, BOOL only_type)
{
	HTML_Element *element = current_element;

 skip:
	switch (where)
	{
	case STEP_FIRST_CHILD:
		if (pretend_root)
			pretend_root = FALSE;
		else
			element = element->FirstChildActual();
		break;

	case STEP_NEXT_SIBLING:
		if (pretend_root || element == root_element)
			return FALSE;
		else
			element = element->SucActual();
		break;

	case STEP_PARENT:
		if (element == root_element)
			if (element->Type() == HE_DOC_ROOT || pretend_root)
				return FALSE;
			else
				pretend_root = TRUE;
		else
			element = element->ParentActual();
	}

	if (!element)
		return FALSE;

	current_element = element;

	HTML_ElementType elementtype = element->Type();
	buffer.Clear();

	if (pretend_root)
		type = CONTENT_ROOT;
	else if (!Markup::IsRealElement(elementtype))
	{
		switch (elementtype)
		{
		case HE_DOC_ROOT:
			type = CONTENT_ROOT;
			break;

		case HE_TEXT:
			type = element->GetIsCDATA() ? CONTENT_CDATA_SECTION : CONTENT_TEXT;
			content = element->Content();
			break;

		case HE_TEXTGROUP:
			type = element->GetIsCDATA() ? CONTENT_CDATA_SECTION : CONTENT_TEXT;
			if (!only_type && where != STEP_REPEAT)
			{
				HTML_Element *stop = element->NextSiblingActual();
				while (element != stop)
				{
					if (element->Type() == HE_TEXT) // Should be, but just to be safe.
						buffer.AppendL(element->Content());

					element = element->NextActual();
				}
				content = buffer.GetStorage();
			}
			break;

		case HE_COMMENT:
			type = CONTENT_COMMENT;
			content = element->GetStringAttr(ATTR_CONTENT);
			break;

		case HE_PROCINST:
			type = CONTENT_PROCESSING_INSTRUCTION;
			content = element->GetStringAttr(ATTR_CONTENT);
			target = element->GetStringAttr(ATTR_TARGET);
			break;

		case HE_DOCTYPE:
			if (!element->ParentActual() || element->ParentActual()->Type() != HE_DOC_ROOT)
			{
				where = STEP_NEXT_SIBLING;
				goto skip;
			}

			type = CONTENT_DOCUMENT_TYPE;
			break;

		default:
			/* Didn't expect that type to occur in the wile. */
			OP_ASSERT(FALSE);
			goto skip;
		}

		if (!content)
			content = UNI_L("");
	}
	else if (Markup::IsRealElement(elementtype))
	{
		type = element == target_element ? CONTENT_ELEMENT_TARGET : CONTENT_ELEMENT;
		name = XMLCompleteName(element);

		attributes.Reset(element);

		if (!only_type)
			wshandling = XMLElementSerializerBackend_GetWhitespaceHandling(element);

		is_html = element->GetNsIdx() == NS_IDX_HTML;
	}
	else
		goto skip;

#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
	before_root_element = FALSE;
	after_root_element = FALSE;

	if (type != CONTENT_ROOT && type != CONTENT_ELEMENT && type != CONTENT_ELEMENT_TARGET)
		if (element && element->Type() != HE_DOC_ROOT && element->ParentActual() && element->ParentActual()->Type() == HE_DOC_ROOT)
			while (TRUE)
				if (HTML_Element *previous = element->PredActual())
					if (Markup::IsRealElement(previous->Type()))
					{
						after_root_element = TRUE;
						break;
					}
					else
						element = previous;
				else
				{
					before_root_element = TRUE;
					break;
				}
#endif // XMLUTILS_CANONICAL_XML_SUPPORT

	return TRUE;
}

/* virtual */ BOOL
XMLElementSerializerBackend::NextAttributeL(XMLCompleteName &name, const uni_char *&value, BOOL &specified, BOOL &id)
{
	const uni_char *localpart;
	int ns_idx;

	if (attributes.GetNext(localpart, value, ns_idx, specified, id))
	{
		name = XMLCompleteName((NS_Element *) (ns_idx > NS_IDX_DEFAULT ? g_ns_manager->GetElementAt(ns_idx) : NULL), localpart);
		if (ns_idx == NS_IDX_XMLNS && uni_str_eq(localpart, UNI_L("xmlns")))
			OpStatus::Ignore(name.SetPrefix(NULL));
		return TRUE;
	}
	else
		return FALSE;
}

#ifdef XMLUTILS_CANONICAL_XML_SUPPORT

/* virtual */ OP_STATUS
XMLElementSerializerBackend::GetNamespacesInScope(XMLNamespaceDeclaration::Reference &nsscope)
{
	return XMLNamespaceDeclaration::PushAllInScope(nsscope, current_element);
}

#endif // XMLUTILS_CANONICAL_XML_SUPPORT
#ifdef XMLUTILS_XMLFRAGMENT_SUPPORT

void
XMLFragmentSerializerBackend::Setup(XMLFragmentData::Element *&element, XMLFragmentData::Content *&current)
{
	element = data->root;

	if (depth == -1)
		current = data->root;
	else
	{
		for (int index = 0; index < depth; ++index)
			element = static_cast<XMLFragmentData::Element *>(element->children[position[index]]);

		current = static_cast<XMLFragmentData::Content *>(element->children[position[depth]]);
	}
}

XMLFragmentSerializerBackend::XMLFragmentSerializerBackend(XMLFragment *fragment)
	: data(fragment->data),
	  position(NULL),
	  attribute(0),
	  depth(-1)
{
	type = CONTENT_ROOT;
}

XMLFragmentSerializerBackend::~XMLFragmentSerializerBackend()
{
	OP_DELETEA(position);
}

OP_STATUS
XMLFragmentSerializerBackend::Construct()
{
	position = OP_NEWA(unsigned, data->max_depth + 1);
	return position ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

/* virtual */ BOOL
XMLFragmentSerializerBackend::StepL(StepType where, BOOL only_type)
{
	XMLFragmentData::Element *element;
	XMLFragmentData::Content *current;

	Setup(element, current);

	if (where == STEP_FIRST_CHILD)
	{
		if (!current || current->type != XMLFragment::CONTENT_ELEMENT || !((XMLFragmentData::Element *) current)->children)
			return FALSE;

		if (data->scope != XMLFragment::GetXMLOptions::SCOPE_WHOLE_FRAGMENT && depth == -1)
		{
			current = data->current;

			int level = 0;

			XMLFragmentData::Element *ancestor = current->parent;
			while (ancestor && ancestor->parent)
			{
				++level;
				ancestor = ancestor->parent;
			}

			depth = level;

			XMLFragmentData::Element *parent = current->parent;
			XMLFragmentData::Content *child = current;

			while (parent)
			{
				int index = 0;
				for (; parent->children[index] != child; ++index) {}
				position[level--] = index;
				child = parent;
				parent = parent->parent;
			}

			if (data->scope == XMLFragment::GetXMLOptions::SCOPE_CURRENT_ELEMENT_EXCLUSIVE)
				goto get_first_child;
		}
		else
		{
		get_first_child:
			current = (XMLFragmentData::Content *) ((XMLFragmentData::Element *) current)->children[0];

			if (current)
				position[++depth] = 0;
		}
	}
	else if (where == STEP_NEXT_SIBLING)
	{
		if (depth == -1 || data->scope != XMLFragment::GetXMLOptions::SCOPE_WHOLE_FRAGMENT && current == data->current)
			return FALSE;

		current = (XMLFragmentData::Content *) element->children[++position[depth]];
	}
	else if (where == STEP_PARENT)
		if (depth == -1)
			return FALSE;
		else if (data->scope == XMLFragment::GetXMLOptions::SCOPE_CURRENT_ELEMENT_INCLUSIVE && current == data->current ||
		         data->scope == XMLFragment::GetXMLOptions::SCOPE_CURRENT_ELEMENT_EXCLUSIVE && element == data->current)
		{
			current = data->root;
			depth = -1;
		}
		else
		{
			current = element;
			--depth;
		}

	if (depth == -1)
		type = CONTENT_ROOT;
	else
	{
		if (!current)
			return FALSE;
		else if (current->type == XMLFragment::CONTENT_ELEMENT)
		{
			type = ((XMLFragmentData::Element *) current)->parent == data->root ? CONTENT_ELEMENT_TARGET : CONTENT_ELEMENT;
			name = ((XMLFragmentData::Element *) current)->name;
			wshandling = ((XMLFragmentData::Element *) current)->wshandling;
			is_html = FALSE;
			attribute = 0;
		}
#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
		else if (current->type == XMLFragment::CONTENT_COMMENT)
		{
			type = CONTENT_COMMENT;
			content = ((XMLFragmentData::Comment *) current)->data.CStr();
			if (!content)
				content = UNI_L("");
		}
		else if (current->type == XMLFragment::CONTENT_PROCESSING_INSTRUCTION)
		{
			type = CONTENT_PROCESSING_INSTRUCTION;
			content = ((XMLFragmentData::ProcessingInstruction *) current)->data.CStr();
			if (!content)
				content = UNI_L("");
			target = ((XMLFragmentData::ProcessingInstruction *) current)->target.CStr();
		}
#endif // XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
		else
		{
			type = CONTENT_TEXT;
			content = ((XMLFragmentData::Text *) current)->data.CStr();
			if (!content)
				content = UNI_L("");
		}
	}

#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
	before_root_element = after_root_element = FALSE;

	if (depth == 0 && current->type != XMLFragment::CONTENT_ELEMENT)
	{
		unsigned index;
		for (index = 0; index < *position; ++index)
			if (static_cast<XMLFragmentData::Content *>(data->root->children[index])->type == XMLFragment::CONTENT_ELEMENT)
			{
				after_root_element = TRUE;
				break;
			}
		if (index == *position)
			before_root_element = TRUE;
	}
#endif // XMLUTILS_CANONICAL_XML_SUPPORT

	return TRUE;
}

/* virtual */ BOOL
XMLFragmentSerializerBackend::NextAttributeL(XMLCompleteName &name, const uni_char *&value, BOOL &specified, BOOL &id)
{
	OP_ASSERT(type == CONTENT_ELEMENT || type == CONTENT_ELEMENT_TARGET);

	XMLFragmentData::Element *element;
	XMLFragmentData::Content *current;

	Setup(element, current);

	if (current)
		if (void **attributes = static_cast<XMLFragmentData::Element *>(current)->attributes)
			if (XMLFragmentData::Element::Attribute *attr = (XMLFragmentData::Element::Attribute *) attributes[attribute])
			{
				name = attr->name;
				value = attr->value.CStr();
				specified = TRUE;
				id = attr->id;

				if (!value)
					value = UNI_L("");

				++attribute;
				return TRUE;
			}

	return FALSE;
}

#ifdef XMLUTILS_CANONICAL_XML_SUPPORT

/* virtual */ OP_STATUS
XMLFragmentSerializerBackend::GetNamespacesInScope(XMLNamespaceDeclaration::Reference &nsscope)
{
	XMLFragmentData::Element *element = data->root;

	for (int index = 0; index <= depth; ++index)
	{
		element = (XMLFragmentData::Element *) element->children[position[index]];

		if (void **attributes = element->attributes)
			if (XMLFragmentData::Element::Attribute *attr = (XMLFragmentData::Element::Attribute *) *attributes)
			{
				const uni_char *value = attr->value.CStr();
				if (!value)
					value = UNI_L("");
				RETURN_IF_ERROR(XMLNamespaceDeclaration::ProcessAttribute(nsscope, attr->name, value, attr->value.Length(), index));

				++attributes;
			}
	}

	return OpStatus::OK;
}

#endif // XMLUTILS_CANONICAL_XML_SUPPORT
#endif // XMLUTILS_XMLFRAGMENT_SUPPORT
#ifdef XMLUTILS_CANONICAL_XML_SUPPORT

/* static */ BOOL
XMLCanonicalizeSerializerBackend::Less(const XMLCompleteName &name1, const XMLCompleteName &name2)
{
	const uni_char *uri1 = name1.GetUri(), *uri2 = name2.GetUri();
	const uni_char *local1 = name1.GetLocalPart(), *local2 = name2.GetLocalPart();

	if (uri1 && uri2)
	{
		int comparison = uni_strcmp(uri1, uri2);
		if (comparison < 0)
			return TRUE;
		else if (comparison > 0)
			return FALSE;
	}
	else if (!uri1 && uri2)
		return TRUE;
	else if (uri1 && !uri2)
		return FALSE;

	int comparison = uni_strcmp(local1, local2);
	if (comparison < 0)
		return TRUE;
	else
		return FALSE;
}

XMLCanonicalizeSerializerBackend::XMLCanonicalizeSerializerBackend(XMLSerializerBackend *backend, BOOL with_comments)
	: backend(backend),
	  with_comments(with_comments),
	  next_attribute(NULL)
{
}

XMLCanonicalizeSerializerBackend::~XMLCanonicalizeSerializerBackend()
{
	attributes.Clear();
}

/* virtual */ BOOL
XMLCanonicalizeSerializerBackend::StepL(StepType where, BOOL only_type)
{
	BOOL found = backend->StepL(where, only_type);

	if (where == STEP_REPEAT)
		next_attribute = static_cast<Attribute *>(attributes.First());
	else
	{
		attributes.Clear();
		next_attribute = NULL;

		if (!with_comments)
			while (found && backend->type == CONTENT_COMMENT)
				found = backend->StepL(STEP_NEXT_SIBLING, only_type);
	}

	if (found)
	{
		type = backend->type;
		name = backend->name;
		wshandling = backend->wshandling;
		content = backend->content;
		target = backend->target;
		is_html = backend->is_html;
		before_root_element = backend->before_root_element;
		after_root_element = backend->after_root_element;

		if (where != STEP_REPEAT && (type == CONTENT_ELEMENT || type == CONTENT_ELEMENT_TARGET))
		{
			XMLCompleteName attrname;
			const uni_char *value;
			BOOL specified, id;

			while (backend->NextAttributeL(attrname, value, specified, id))
				if (!attrname.IsXMLNamespaces())
				{
					Attribute *attr = OP_NEW_L(Attribute, ());
					if (OpStatus::IsMemoryError(attr->name.Set(attrname)) || OpStatus::IsMemoryError(attr->value.Set(value)))
					{
						OP_DELETE(attr);
						LEAVE(OpStatus::ERR_NO_MEMORY);
					}
					attr->specified = specified;
					attr->id = id;

					Attribute *follower = static_cast<Attribute *>(attributes.First());
					while (follower && !Less(attr->name, follower->name))
						follower = static_cast<Attribute *>(follower->Suc());

					if (follower)
						attr->Precede(follower);
					else
						attr->Into(&attributes);
				}

			next_attribute = static_cast<Attribute *>(attributes.First());
		}
	}

	return found;
}

/* virtual */ BOOL
XMLCanonicalizeSerializerBackend::NextAttributeL(XMLCompleteName &name, const uni_char *&value, BOOL &specified, BOOL &id)
{
	if (next_attribute)
	{
		name = next_attribute->name;
		value = next_attribute->value.CStr();
		specified = next_attribute->specified;
		id = next_attribute->id;

		next_attribute = static_cast<Attribute *>(next_attribute->Suc());

		return TRUE;
	}
	else
		return FALSE;
}

/* virtual */ OP_STATUS
XMLCanonicalizeSerializerBackend::GetNamespacesInScope(XMLNamespaceDeclaration::Reference &nsscope)
{
	return backend->GetNamespacesInScope(nsscope);
}

BOOL
XMLCanonicalizeSerializerBackend::IsVisiblyUsing(const XMLNamespaceDeclaration *nsdeclaration)
{
	if (nsdeclaration->GetPrefix())
	{
		if (name.GetPrefix() && uni_strcmp(nsdeclaration->GetPrefix(), name.GetPrefix()) == 0)
			return TRUE;

		Attribute *attribute = static_cast<Attribute *>(attributes.First());

		while (attribute)
		{
			if (attribute->name.GetPrefix() && uni_strcmp(nsdeclaration->GetPrefix(), attribute->name.GetPrefix()) == 0)
				return TRUE;

			attribute = static_cast<Attribute *>(attribute->Suc());
		}
	}
	else if (!name.GetPrefix())
		return TRUE;

	return FALSE;
}

#endif // XMLUTILS_CANONICAL_XML_SUPPORT
#ifdef XMLUTILS_XMLTOLANGUAGEPARSERSERIALIZER_SUPPORT

void
XMLToLanguageParserSerializer::DoSerializeL(BOOL resume_after_continue)
{
	XMLCompleteName attrname;
	BOOL ignore_element, specified, id;

	if (resume_after_continue)
		goto resume_after_continue;

	backend->StepL(XMLSerializerBackend::STEP_REPEAT);

	while (TRUE)
	{
		XMLSerializerBackend::ContentType type;
		XMLLanguageParser::CharacterDataType cdatatype;
		const uni_char *value;
		unsigned content_length;

		ignore_element = FALSE;
		type = backend->type;

		switch (type)
		{
		case XMLSerializerBackend::CONTENT_ROOT:
			LEAVE_IF_ERROR(parser->StartEntity(url, documentinfo, FALSE));
			break;

		case XMLSerializerBackend::CONTENT_TEXT:
		case XMLSerializerBackend::CONTENT_CDATA_SECTION:
			content_length = uni_strlen(backend->content);
			if (backend->type == XMLSerializerBackend::CONTENT_CDATA_SECTION)
				cdatatype = XMLLanguageParser::CHARACTERDATA_CDATA_SECTION;
			else if (XMLUtils::IsWhitespace(backend->content, content_length))
				cdatatype = XMLLanguageParser::CHARACTERDATA_TEXT_WHITESPACE;
			else
				cdatatype = XMLLanguageParser::CHARACTERDATA_TEXT;
			LEAVE_IF_ERROR(parser->AddCharacterData(cdatatype, backend->content, content_length));
			ignore_element = TRUE;
			break;

		case XMLSerializerBackend::CONTENT_PROCESSING_INSTRUCTION:
			LEAVE_IF_ERROR(parser->AddProcessingInstruction(backend->target, uni_strlen(backend->target), backend->content, uni_strlen(backend->content)));
			ignore_element = TRUE;
			break;

		case XMLSerializerBackend::CONTENT_ELEMENT:
		case XMLSerializerBackend::CONTENT_ELEMENT_TARGET:
			LEAVE_IF_ERROR(parser->StartElement(backend->name, backend->type == XMLSerializerBackend::CONTENT_ELEMENT_TARGET, ignore_element));
			if (ignore_element)
				break;
			while (backend->NextAttributeL(attrname, value, specified, id))
				LEAVE_IF_ERROR(parser->AddAttribute(attrname, value, uni_strlen(value), specified, id));
			LEAVE_IF_ERROR(parser->StartContent());
		}

		if (!ignore_element && backend->StepL(XMLSerializerBackend::STEP_FIRST_CHILD))
			continue;
		else
		{
			goto skip_step_parent;

			do
			{
				if (!backend->StepL(XMLSerializerBackend::STEP_PARENT, TRUE))
					return;

			 skip_step_parent:
				switch (backend->type)
				{
				case XMLSerializerBackend::CONTENT_ROOT:
					LEAVE_IF_ERROR(parser->EndEntity());
					break;

				case XMLSerializerBackend::CONTENT_ELEMENT:
				case XMLSerializerBackend::CONTENT_ELEMENT_TARGET:
					block = finished = FALSE;
					LEAVE_IF_ERROR(parser->EndElement(block, finished));
					if (block || finished)
						return;
				}

			 resume_after_continue:;
			}
			while (!backend->StepL(XMLSerializerBackend::STEP_NEXT_SIBLING));
		}
	}
}

OP_STATUS
XMLToLanguageParserSerializer::DoSerialize(BOOL resume_after_continue)
{
	block = finished = FALSE;

	TRAPD(status, DoSerializeL(resume_after_continue));

	if (OpStatus::IsSuccess(status) && block)
	{
		if (!has_set_callback)
		{
			RETURN_IF_ERROR(mh->SetCallBack(this, MSG_XMLUTILS_CONTINUE, (MH_PARAM_1) this));
			has_set_callback = TRUE;
		}
		parser->SetSourceCallback(this);
		return OpStatus::OK;
	}

	if (callback)
	{
		Callback::Status cbstatus;

		if (OpStatus::IsMemoryError(status))
			cbstatus = Callback::STATUS_OOM;
		else if (OpStatus::IsError(status))
			cbstatus = Callback::STATUS_FAILED;
		else
			cbstatus = Callback::STATUS_COMPLETED;

		callback->Stopped(cbstatus);
	}

	OP_DELETE(backend);
	backend = NULL;

	return status;
}

XMLToLanguageParserSerializer::XMLToLanguageParserSerializer(XMLLanguageParser *parser)
	: parser(parser),
	  backend(0),
	  mh(0),
	  has_set_callback(FALSE),
	  callback(0)
{
}

/* virtual */
XMLToLanguageParserSerializer::~XMLToLanguageParserSerializer()
{
	if (mh && has_set_callback)
		mh->UnsetCallBacks(this);

	OP_DELETE(backend);
}

/* virtual */ OP_STATUS
XMLToLanguageParserSerializer::SetConfiguration(const Configuration &configuration)
{
	if (configuration.document_information)
		RETURN_IF_ERROR(documentinfo.Copy(*configuration.document_information));

	return OpStatus::OK;
}

#ifdef XMLUTILS_CANONICAL_XML_SUPPORT

/* virtual */ OP_STATUS
XMLToLanguageParserSerializer::SetCanonicalize(CanonicalizeVersion version, BOOL with_comments)
{
	/* Does not apply to this serializer. */
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
XMLToLanguageParserSerializer::SetInclusiveNamespacesPrefixList(const uni_char *list)
{
	/* Does not apply to this serializer. */
	return OpStatus::OK;
}

#endif // XMLUTILS_CANONICAL_XML_SUPPORT

/* virtual */ OP_STATUS
XMLToLanguageParserSerializer::Serialize(MessageHandler *new_mh, const URL &new_url, HTML_Element *root, HTML_Element *target, Callback *new_callback)
{
	if (backend)
		/* Serializer already started. */
		return OpStatus::ERR;
	else
	{
		backend = OP_NEW(XMLElementSerializerBackend, (root, target));

		if (!backend)
			return OpStatus::ERR_NO_MEMORY;

		mh = new_mh;
		url = new_url;
		callback = new_callback;

		return DoSerialize(FALSE);
	}
}

#ifdef XMLUTILS_XMLFRAGMENT_SUPPORT

/* virtual */ OP_STATUS
XMLToLanguageParserSerializer::Serialize(MessageHandler *new_mh, const URL &new_url, XMLFragment *fragment, Callback *new_callback)
{
	if (backend)
		/* Serializer already started. */
		return OpStatus::ERR;
	else
	{
		backend = OP_NEW(XMLFragmentSerializerBackend, (fragment));

		if (!backend || OpStatus::IsMemoryError(((XMLFragmentSerializerBackend *) backend)->Construct()))
			return OpStatus::ERR_NO_MEMORY;

		mh = new_mh;
		url = new_url;
		callback = new_callback;

		return DoSerialize(FALSE);
	}
}

#endif // XMLUTILS_XMLFRAGMENT_SUPPORT

/* virtual */ XMLSerializer::Error
XMLToLanguageParserSerializer::GetError()
{
	return ERROR_NONE;
}

/* virtual */ void
XMLToLanguageParserSerializer::Continue(Status status)
{
	if (status == STATUS_CONTINUE)
		mh->PostMessage(MSG_XMLUTILS_CONTINUE, (MH_PARAM_1) this, 0);
	else if (callback)
		callback->Stopped(status == STATUS_ABORT_FAILED ? Callback::STATUS_FAILED : Callback::STATUS_OOM);
}

/* virtual */ void
XMLToLanguageParserSerializer::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (par1 == (MH_PARAM_1) this)
		/* The status is typically signalled to a callback, so it
		   isn't completely ignored. */
		OpStatus::Ignore(DoSerialize(TRUE));
}

#endif // XMLUTILS_XMLTOLANGUAGEPARSERSERIALIZER_SUPPORT

#ifdef XMLUTILS_XMLTOSTRINGSERIALIZER_SUPPORT

/* static */ OP_STATUS
XMLSerializer::MakeToStringSerializer(XMLSerializer *&serialize, TempBuffer *buffer)
{
	serialize = OP_NEW(XMLToStringSerializer, (buffer));
	return serialize ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

void
XMLToStringSerializer::AppendL(const char *text, BOOL first, BOOL last)
{
#ifdef XMLUTILS_PRETTY_PRINTING_SUPPORT
	if (format_pretty_print && !format_pretty_print_suspended && first)
	{
		if (linelength != 0)
			AppendL("\n");
		AppendIndentationL();
	}
#endif // XMLUTILS_PRETTY_PRINTING_SUPPORT

	buffer->AppendL(text);

#ifdef XMLUTILS_PRETTY_PRINTING_SUPPORT
	if (format_pretty_print)
		if (last)
		{
			if (!format_pretty_print_suspended)
				AppendL("\n");
		}
		else
		{
			unsigned added = 0;

			for (const char *ptr = text + op_strlen(text); ptr != text; --ptr)
			{
				unsigned ch = *(ptr - 1);
				if (ch == 10 || ch == 13)
				{
					linelength = 0;
					break;
				}
				else
					++added;
			}

			linelength += added;
		}
#endif // XMLUTILS_PRETTY_PRINTING_SUPPORT
}

void
XMLToStringSerializer::AppendL(const uni_char *text, unsigned length)
{
	if (length == ~0u)
		length = uni_strlen(text);

	buffer->AppendL(text, length);

#ifdef XMLUTILS_PRETTY_PRINTING_SUPPORT
	if (format_pretty_print)
	{
		unsigned added = 0;

		for (const uni_char *ptr = text + length; ptr != text; --ptr)
		{
			unsigned ch = *(ptr - 1);
			if (ch == 10 || ch == 13)
			{
				linelength = 0;
				break;
			}
			else
				++added;
		}

		linelength += added;
	}
#endif // XMLUTILS_PRETTY_PRINTING_SUPPORT
}

#ifdef XMLUTILS_PRETTY_PRINTING_SUPPORT

void
XMLToStringSerializer::AppendIndentationL()
{
	if (format_pretty_print && !format_pretty_print_suspended)
	{
		unsigned length = indentlevel * 2 + indentextra;

		if (length)
		{
			linelength += length;

			buffer->ExpandL(buffer->Length() + length + 1);

			while (length--)
				OpStatus::Ignore(buffer->Append(' '));
		}
	}
}

#endif // XMLUTILS_PRETTY_PRINTING_SUPPORT

void
XMLToStringSerializer::AppendAttributeValueL(const uni_char *value)
{
	AppendL("=\"");

	if (value)
	{
		XMLVersion version = documentinfo.GetVersion();

		while (*value)
		{
			const uni_char *value_end = value;

			unsigned ch;
			while ((ch = *value_end) && !(ch == '<' || ch == '>' || ch == '&' || ch == '"' || ch != ' ' && XMLUtils::IsSpaceExtended(version, ch)))
				++value_end;

			AppendCharacterDataL(value, value_end - value, FALSE, FALSE, TRUE);

			if (*value_end)
			{
				const char *entityreference = NULL;

				switch (*value_end)
				{
				case '<':
					entityreference = "&lt;";
					break;

				case '>':
					entityreference = "&gt;";
					break;

				case '&':
					entityreference = "&amp;";
					break;

				case '"':
					entityreference = "&quot;";
					break;

				default:
					AppendEscapedL (*value_end);
				}

				if (entityreference)
					AppendL(entityreference);
				++value_end;
			}

			value = value_end;
		}
	}

	AppendL("\"");
}

void
XMLToStringSerializer::AppendAttributeSpecificationL(const uni_char *prefix, const uni_char *localpart, const uni_char *value)
{
#ifdef XMLUTILS_PRETTY_PRINTING_SUPPORT
	unsigned attribute_length = (prefix ? uni_strlen(prefix) + 1 : 0) + uni_strlen(localpart) + 3 + uni_strlen(value);

	if (linelength > indentlevel * 2 + indentextra && linelength + attribute_length > preferredlinelength)
		AppendL("", TRUE, FALSE);
	else
#endif // XMLUTILS_PRETTY_PRINTING_SUPPORT
		AppendL(" ");

	if (prefix)
	{
		AppendL(prefix);
		AppendL(":");
	}
	AppendL(localpart);
	AppendAttributeValueL(value);
}

void
XMLToStringSerializer::AppendAttributeSpecificationL(XMLNamespaceDeclaration *nsdeclaration)
{
	const uni_char *prefix, *localpart, *uri;
	if (nsdeclaration->GetPrefix())
	{
		prefix = UNI_L("xmlns");
		localpart = nsdeclaration->GetPrefix();
	}
	else
	{
		prefix = NULL;
		localpart = UNI_L("xmlns");
	}
	uri = nsdeclaration->GetUri();

	AppendAttributeSpecificationL(prefix, localpart, uri);

#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
	if (canonicalize)
		XMLNamespaceDeclaration::PushL(nsstack, uri, uni_strlen(uri), nsdeclaration->GetPrefix(), nsdeclaration->GetPrefix() ? uni_strlen(nsdeclaration->GetPrefix()) : 0, level);
#endif // XMLUTILS_CANONICAL_XML_SUPPORT
}

void
XMLToStringSerializer::AppendEncodedL(const char *prefix, const uni_char *&value, unsigned &value_length, const char *suffix)
{
	const uni_char *start = value;
	unsigned appended = 0;

	const uni_char *temporary = value;
	unsigned temporary_length = value_length;

	unsigned length = value_length;

	if (documentinfo.GetVersion() == XMLVERSION_1_0)
		while (temporary_length != 0)
		{
			unsigned ch = XMLUtils::GetNextCharacter(temporary, temporary_length);
			if (!XMLUtils::IsChar10(ch))
			{
				length -= (temporary_length + (ch >= 0x10000 ? 2 : 1));
				break;
			}
		}
	else
		while (temporary_length != 0)
		{
			unsigned ch = XMLUtils::GetNextCharacter(temporary, temporary_length);
			if (!XMLUtils::IsChar11(ch) || XMLUtils::IsRestrictedChar(ch))
			{
				length -= (temporary_length + (ch >= 0x10000 ? 2 : 1));
				break;
			}
		}

	if (converter)
	{
		unsigned length_total = length;

		while (length != 0)
		{
			int read;

			converter->Reset();
			int written = converter->Convert(value, UNICODE_SIZE(length), converter_buffer, converter_buffer_length, &read);

			if (written == -1)
				LEAVE(OpStatus::ERR_NO_MEMORY);

			unsigned consumed = UNICODE_DOWNSIZE(read);

			if (converter->GetNumberOfInvalid() == 0)
			{
				appended += consumed;
				value += consumed;
				length -= consumed;
			}
			else
			{
#ifdef ENCODINGS_HAVE_SPECIFY_INVALID
				const uni_char *invalid = converter->GetInvalidCharacters();
#else // ENCODINGS_HAVE_SPECIFY_INVALID
				/* This is all wrong.  FEATURE_UNCONVERTIBLE_CHARS is really required, but by what? */
				const uni_char *invalid = value;
#endif // ENCODINGS_HAVE_SPECIFY_INVALID

				while (*value != *invalid)
					++appended, ++value, --length;

				break;
			}
		}

		value_length -= length_total - length;
	}
	else
	{
		appended = value_length;
		value_length = 0;
	}

	if (appended != 0)
	{
		if (prefix)
			AppendL(prefix);

		AppendL(start, appended);

		if (suffix)
			AppendL(suffix);
	}
}

void
XMLToStringSerializer::AppendEscapedL(unsigned ch)
{
	char escaped[16]; /* ARRAY OK jl 2008-02-07 */
	op_snprintf(escaped, sizeof escaped, charref_format, ch);
	escaped[sizeof escaped - 1] = 0;

	AppendL(escaped);
}

void
XMLToStringSerializer::AppendCharacterDataL(const uni_char *value, unsigned value_length, BOOL is_cdata_section, BOOL is_comment, BOOL is_attrvalue)
{
	XMLVersion version = documentinfo.GetVersion();
	const char *prefix, *suffix;

	if (is_cdata_section)
	{
		prefix = "<![CDATA[";
		suffix = "]]>";
	}
	else
		prefix = suffix = 0;

	BOOL break_after = FALSE;

#ifdef XMLUTILS_PRETTY_PRINTING_SUPPORT
	if (format_pretty_print)
	{
		BOOL break_before = !is_comment && !is_attrvalue;

		break_after = break_before;

		if (!is_cdata_section && !is_comment && !is_attrvalue)
		{
			const uni_char *ptr = value, *ptr_end = value + value_length;
			while (ptr != ptr_end)
			{
				if (!XMLUtils::IsSpace(*ptr))
					break;
				else if (*ptr == 10 || *ptr == 13)
					break_before = FALSE;
				++ptr;
			}
			while (ptr != ptr_end)
			{
				--ptr_end;
				if (!XMLUtils::IsSpace(*ptr_end))
					break;
				else if (*ptr_end == 10 || *ptr_end == 13)
				{
					break_after = FALSE;
					value_length = ptr_end - value + 1;
				}
			}
		}

		if (break_before)
			AppendL("", TRUE, FALSE);
	}
#endif // XMLUTILS_PRETTY_PRINTING_SUPPORT

	while (value_length != 0)
	{
		unsigned local_value_length;

#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
		if (canonicalize)
			for (local_value_length = 0; local_value_length < value_length && value[local_value_length] != 13; ++local_value_length) {}
		else
#endif // XMLUTILS_CANONICAL_XML_SUPPORT
			local_value_length = value_length;

		value_length -= local_value_length;

		while (local_value_length != 0)
		{
			AppendEncodedL(prefix, value, local_value_length, suffix);

			if (local_value_length != 0)
			{
				unsigned ch = XMLUtils::GetNextCharacter(value, local_value_length);

				if (is_comment)
					AppendL("?");
				else if (is_cdata_section && !split_cdata_sections || !XMLUtils::IsChar(version, ch))
				{
					error = ERROR_INVALID_CHARACTER_IN_CDATA;
					LEAVE(OpStatus::ERR);
				}
				else
					AppendEscapedL(ch);
			}
		}

#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
		if (value_length != 0)
		{
			AppendL("&#xD;");
			++value;
			--value_length;
		}
#endif // XMLUTILS_CANONICAL_XML_SUPPORT
	}

#ifdef XMLUTILS_PRETTY_PRINTING_SUPPORT
	if (break_after)
		AppendL("", FALSE, TRUE);
#endif // XMLUTILS_PRETTY_PRINTING_SUPPORT
}

void
XMLToStringSerializer::AppendNameL(const uni_char *value, BOOL is_qname)
{
	XMLVersion version = documentinfo.GetVersion();
	unsigned value_length = uni_strlen(value);

	if (is_qname ? !XMLUtils::IsValidQName(version, value, value_length) : !XMLUtils::IsValidNCName(version, value, value_length))
	{
		error = ERROR_INVALID_QNAME_OR_NCNAME;
		LEAVE(OpStatus::ERR);
	}

	AppendEncodedL(0, value, value_length, 0);

	if (value_length != 0)
	{
		error = ERROR_INVALID_CHARACTER_IN_NAME;
		LEAVE(OpStatus::ERR);
	}
}

void
XMLToStringSerializer::AppendCorrectlyQuotedL(const uni_char *string)
{
	const char *quote = "\"";

	if (uni_strchr(string, *quote))
		quote = "'";

	AppendL(quote);
	AppendL(string);
	AppendL(quote);
}

void
XMLToStringSerializer::AppendDoctypeDeclarationL()
{
	if (documentinfo.GetDoctypeDeclarationPresent())
	{
		AppendL("<!DOCTYPE ", TRUE, FALSE);
		AppendL(documentinfo.GetDoctypeName());

		if (documentinfo.GetPublicId())
		{
			AppendL(" PUBLIC ");
			AppendCorrectlyQuotedL(documentinfo.GetPublicId());
		}

		if (documentinfo.GetSystemId())
		{
			if (!documentinfo.GetPublicId())
				AppendL(" SYSTEM ");
			else
				AppendL(" ");

			AppendCorrectlyQuotedL(documentinfo.GetSystemId());
		}

		if (documentinfo.GetInternalSubset())
		{
			AppendL(" [");
			AppendL(documentinfo.GetInternalSubset());
			AppendL("]");
		}

		AppendL(">", FALSE, TRUE);
	}
}

static void
XMLInterpretNameL(XMLNamespaceDeclaration *nsdeclaration, XMLCompleteName &name)
{
	/* Names with a URI or prefix have obviously already been interpreted. */
	if (!name.GetUri() && !name.GetPrefix())
	{
		XMLCompleteNameN interpreted(name.GetLocalPart(), uni_strlen(name.GetLocalPart()));
		XMLNamespaceDeclaration::ResolveName(nsdeclaration, interpreted, FALSE);

		if (interpreted.GetUri() || interpreted.GetPrefix())
			name.SetL(interpreted);
	}
}

class XMLNamespaceDeclarationLink
	: public Link
{
public:
	XMLNamespaceDeclarationLink(XMLNamespaceDeclaration *nsdeclaration)
		: nsdeclaration(nsdeclaration)
	{
	}

	XMLNamespaceDeclaration *nsdeclaration;

	static void InsertL(Head &list, XMLNamespaceDeclaration *nsdeclaration)
	{
		XMLNamespaceDeclarationLink *link = OP_NEW(XMLNamespaceDeclarationLink, (nsdeclaration));
		XMLNamespaceDeclarationLink *follower = static_cast<XMLNamespaceDeclarationLink *>(list.First());

		while (follower && uni_strcmp(nsdeclaration->GetPrefix(), follower->nsdeclaration->GetPrefix()) >= 0)
			follower = static_cast<XMLNamespaceDeclarationLink *>(follower->Suc());

		if (follower)
			link->Precede(follower);
		else
			link->Into(&list);
	}
};

void
XMLToStringSerializer::DoSerializeL(XMLSerializerBackend *backend)
{
	XMLSerializerBackend::ContentType type = backend->type;

#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
#define CANONICALIZE canonicalize
#define CANONICALIZE_WITH_COMMENTS canonicalize_with_comments
#define LINEBREAK_AFTER_BEFORE_ROOT_ELEMENT() do { if (canonicalize && backend->before_root_element) AppendL("\n"); } while (0)
#define LINEBREAK_BEFORE_AFTER_ROOT_ELEMENT() do { if (canonicalize && backend->after_root_element) AppendL("\n"); } while (0)
#else // XMLUTILS_CANONICAL_XML_SUPPORT
#define CANONICALIZE FALSE
#define CANONICALIZE_WITH_COMMENTS FALSE
#define LINEBREAK_AFTER_BEFORE_ROOT_ELEMENT()
#define LINEBREAK_BEFORE_AFTER_ROOT_ELEMENT()
#endif // XMLUTILS_CANONICAL_XML_SUPPORT

	if (type == XMLSerializerBackend::CONTENT_TEXT || CANONICALIZE && type == XMLSerializerBackend::CONTENT_CDATA_SECTION)
	{
		const uni_char *content = backend->content;

		while (*content)
		{
			const uni_char *content_end = content;

			while (*content_end && *content_end != '<' && *content_end != '&' && (!CANONICALIZE || *content_end != '>'))
			{
				if (*content_end == ']' && *(content_end + 1) == ']' && *(content_end + 2) == '>')
				{
					content_end += 2;
					break;
				}

				++content_end;
			}

			AppendCharacterDataL(content, content_end - content, FALSE, FALSE, FALSE);

			if (*content_end)
			{
				AppendL(*content_end == '<' ? "&lt;" : *content_end == '&' ? "&amp;" : "&gt;");
				++content_end;
			}

			content = content_end;
		}
	}
	else if (type == XMLSerializerBackend::CONTENT_CDATA_SECTION)
	{
		const uni_char *content = backend->content;

		while (*content)
		{
			const uni_char *content_end = content;

			while (*content_end && uni_strncmp(content_end, UNI_L("]]>"), 3) != 0)
				++content_end;

			AppendCharacterDataL(content, content_end - content, TRUE, FALSE, FALSE);

			if (*content_end)
			{
				AppendL("]", FALSE, TRUE);
				++content_end;
			}

			content = content_end;
		}
	}
	else if (type == XMLSerializerBackend::CONTENT_COMMENT)
	{
#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
		if (!CANONICALIZE || CANONICALIZE_WITH_COMMENTS)
#endif // XMLUTILS_CANONICAL_XML_SUPPORT
		{
			LINEBREAK_BEFORE_AFTER_ROOT_ELEMENT ();

			AppendL("<!--", TRUE, FALSE);

			const uni_char *content = backend->content;

			while (*content)
			{
				const uni_char *content_end = content;

				while (*content_end)
				{
					++content_end;

					if (*content_end == '-' && *(content_end - 1) == '-')
						break;
				}

				AppendCharacterDataL(content, content_end - content, FALSE, TRUE, FALSE);

				if (*content_end || *(content_end - 1) == '-')
				{
					/* Insert space between consecutive dashes. */
					AppendL(" ");
				}

				content = content_end;
			}

			AppendL("-->", FALSE, TRUE);

			LINEBREAK_AFTER_BEFORE_ROOT_ELEMENT ();
		}
	}
	else if (type == XMLSerializerBackend::CONTENT_PROCESSING_INSTRUCTION)
	{
		LINEBREAK_BEFORE_AFTER_ROOT_ELEMENT ();

		AppendL("<?", TRUE, FALSE);
		AppendNameL(backend->target, FALSE);

		if (*backend->content)
		{
			AppendL(" ");
			const uni_char *content = backend->content, *ptr = content;

			while (*ptr && (*ptr != '?' || *(ptr + 1) != '>'))
				++ptr;

			if (*ptr)
			{
				error = ERROR_INVALID_CHARACTER_IN_PROCESSING_INSTRUCTION;
				LEAVE(OpStatus::ERR);
			}

			unsigned content_length = uni_strlen(content);

			AppendEncodedL(NULL, content, content_length, NULL);

			if (content_length != 0)
			{
				error = ERROR_INVALID_CHARACTER_IN_PROCESSING_INSTRUCTION;
				LEAVE(OpStatus::ERR);
			}
		}

		AppendL("?>", FALSE, TRUE);

		LINEBREAK_AFTER_BEFORE_ROOT_ELEMENT ();
	}
	else if (type == XMLSerializerBackend::CONTENT_ROOT)
	{
		if (!CANONICALIZE)
		{
			if (documentinfo.GetXMLDeclarationPresent())
			{
				AppendL("<?xml version=\"1.");

				if (documentinfo.GetVersion() == XMLVERSION_1_0)
					AppendL("0\"");
				else
					AppendL("1\"");

				if (documentinfo.GetEncoding())
				{
					AppendL(" encoding=\"");
					AppendL(documentinfo.GetEncoding());
					AppendL("\"");
				}

				if (documentinfo.GetStandalone() != XMLSTANDALONE_NONE)
				{
					AppendL(" standalone=\"");

					if (documentinfo.GetStandalone() == XMLSTANDALONE_YES)
						AppendL("yes\"");
					else
						AppendL("no\"");
				}

				AppendL("?>", FALSE, TRUE);
			}

			if (documentinfo.GetDoctypeDeclarationPresent() && backend->StepL(XMLSerializerBackend::STEP_FIRST_CHILD, TRUE))
			{
				BOOL has_doctype = FALSE;

				do
					if (backend->type == XMLSerializerBackend::CONTENT_DOCUMENT_TYPE)
					{
						has_doctype = TRUE;
						break;
					}
				while (backend->StepL(XMLSerializerBackend::STEP_NEXT_SIBLING, TRUE));

				if (!has_doctype)
					AppendDoctypeDeclarationL();

				backend->StepL(XMLSerializerBackend::STEP_PARENT);
			}
		}

		if (backend->StepL(XMLSerializerBackend::STEP_FIRST_CHILD))
		{
			do
				DoSerializeL(backend);
			while (backend->StepL(XMLSerializerBackend::STEP_NEXT_SIBLING));

			backend->StepL(XMLSerializerBackend::STEP_PARENT);
		}
	}
	else if (type == XMLSerializerBackend::CONTENT_DOCUMENT_TYPE)
	{
		if (!CANONICALIZE)
			AppendDoctypeDeclarationL();
	}
	else
	{
		++level;

		XMLCompleteName elemname(backend->name); ANCHOR(XMLCompleteName, elemname);
		XMLInterpretNameL(nsnormalizer.GetNamespaceDeclaration(), elemname);

		if (!CANONICALIZE)
		{
			nsnormalizer.SetPerformNormalization(normalize_namespaces && !backend->is_html);
			LEAVE_IF_ERROR(nsnormalizer.StartElement(elemname));

			XMLCompleteName attrname; ANCHOR(XMLCompleteName, attrname);
			const uni_char *value;
			BOOL specified, id;

			while (backend->NextAttributeL(attrname, value, specified, id))
				if (specified || !discard_default_content)
					LEAVE_IF_ERROR(nsnormalizer.AddAttribute(attrname, value, TRUE));

			LEAVE_IF_ERROR(nsnormalizer.FinishAttributes());
		}

		/* The element name is never changed by the normalizer. */
		AppendL("<", TRUE, FALSE);
		if (elemname.GetPrefix())
		{
			AppendL(elemname.GetPrefix());
			AppendL(":");
		}
		AppendL(elemname.GetLocalPart());

#ifdef XMLUTILS_PRETTY_PRINTING_SUPPORT
		indentextra = linelength - indentlevel * 2 + 1;
		BOOL old_format_pretty_print_suspended = format_pretty_print_suspended;
		format_pretty_print_suspended = FALSE;
#endif // XMLUTILS_PRETTY_PRINTING_SUPPORT

		BOOL xmlspace_seen = FALSE;
		XMLWhitespaceHandling previous_wshandling = current_wshandling;

#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
		if (canonicalize)
		{
			XMLNamespaceDeclaration::Reference nsscope; ANCHOR (XMLNamespaceDeclaration::Reference, nsscope);

			LEAVE_IF_ERROR(backend->GetNamespacesInScope(nsscope));

			XMLNamespaceDeclaration *existing_defaultns = XMLNamespaceDeclaration::FindDefaultDeclaration(nsstack, FALSE);
			if (XMLNamespaceDeclaration *defaultns = XMLNamespaceDeclaration::FindDefaultDeclaration(nsscope, FALSE))
			{
				if (IncludeNSDeclaration(static_cast<XMLCanonicalizeSerializerBackend *>(backend), defaultns) && (!existing_defaultns || !uni_str_eq(existing_defaultns->GetUri(), defaultns->GetUri())))
					AppendAttributeSpecificationL(defaultns);
			}
			else if (existing_defaultns)
			{
				AppendAttributeSpecificationL(NULL, UNI_L("xmlns"), UNI_L(""));
				XMLNamespaceDeclaration::PushL(nsstack, NULL, 0, NULL, 0, level);
			}

			AutoDeleteHead nsdeclarations; ANCHOR (AutoDeleteHead, nsdeclarations);

			for (unsigned index = 0, count = XMLNamespaceDeclaration::CountDeclaredPrefixes(nsscope); index < count; ++index)
			{
				XMLNamespaceDeclaration *inscope = XMLNamespaceDeclaration::FindDeclarationByIndex(nsscope, index);
				XMLNamespaceDeclaration *existing = XMLNamespaceDeclaration::FindDeclaration(nsstack, inscope->GetPrefix());

				if (IncludeNSDeclaration(static_cast<XMLCanonicalizeSerializerBackend *>(backend), inscope) && (!existing || !uni_str_eq(existing->GetUri(), inscope->GetUri())))
					XMLNamespaceDeclarationLink::InsertL(nsdeclarations, inscope);
			}

			for (XMLNamespaceDeclarationLink *link = static_cast<XMLNamespaceDeclarationLink *>(nsdeclarations.First()); link; link = static_cast<XMLNamespaceDeclarationLink *>(link->Suc()))
				AppendAttributeSpecificationL(link->nsdeclaration);

			XMLCompleteName attrname; ANCHOR(XMLCompleteName, attrname);
			const uni_char *value;
			BOOL specified, id;

			while (backend->NextAttributeL(attrname, value, specified, id))
				AppendAttributeSpecificationL(attrname.GetPrefix(), attrname.GetLocalPart(), value);
		}
		else
#endif // XMLUTILS_CANONICAL_XML_SUPPORT
		{
			for (unsigned attr_index = 0, attr_count = nsnormalizer.GetAttributesCount(); attr_index < attr_count; ++attr_index)
			{
				XMLCompleteName attrname(nsnormalizer.GetAttributeName(attr_index)); ANCHOR(XMLCompleteName, attrname);

				XMLInterpretNameL(nsnormalizer.GetNamespaceDeclaration(), attrname);

				if (uni_str_eq(attrname.GetLocalPart(), "space") && attrname.IsXML())
				{
					xmlspace_seen = TRUE;
					current_wshandling = uni_str_eq(nsnormalizer.GetAttributeValue(attr_index), "preserve") ? XMLWHITESPACEHANDLING_PRESERVE : XMLWHITESPACEHANDLING_DEFAULT;
				}

				AppendAttributeSpecificationL(attrname.GetPrefix(), attrname.GetLocalPart(), nsnormalizer.GetAttributeValue(attr_index));
			}
		}

		if (!xmlspace_seen && backend->wshandling != current_wshandling && add_xml_space_attributes)
		{
			current_wshandling = backend->wshandling;
			AppendAttributeSpecificationL(UNI_L("xml"), UNI_L("space"), current_wshandling == XMLWHITESPACEHANDLING_PRESERVE ? UNI_L("preserve") : UNI_L("default"));
		}

#ifdef XMLUTILS_PRETTY_PRINTING_SUPPORT
		indentextra = 0;
#endif // XMLUTILS_PRETTY_PRINTING_SUPPORT

		BOOL has_children = backend->StepL(XMLSerializerBackend::STEP_FIRST_CHILD);
		if (has_children || CANONICALIZE)
		{
#ifdef XMLUTILS_PRETTY_PRINTING_SUPPORT
			++indentlevel;
			if (current_wshandling == XMLWHITESPACEHANDLING_PRESERVE)
				format_pretty_print_suspended = TRUE;
#endif // XMLUTILS_PRETTY_PRINTING_SUPPORT

			AppendL(">", FALSE, TRUE);

			if (has_children)
				do
					DoSerializeL(backend);
				while (backend->StepL(XMLSerializerBackend::STEP_NEXT_SIBLING));

#ifdef XMLUTILS_PRETTY_PRINTING_SUPPORT
			--indentlevel;
#endif // XMLUTILS_PRETTY_PRINTING_SUPPORT

			AppendL("</", TRUE, FALSE);

#ifdef XMLUTILS_PRETTY_PRINTING_SUPPORT
			format_pretty_print_suspended = old_format_pretty_print_suspended;
#endif // XMLUTILS_PRETTY_PRINTING_SUPPORT

			if (elemname.GetPrefix())
			{
				AppendL(elemname.GetPrefix());
				AppendL(":");
			}
			AppendL(elemname.GetLocalPart());
			AppendL(">", FALSE, TRUE);

			if (has_children)
				backend->StepL(XMLSerializerBackend::STEP_PARENT);
		}
		else
		{
#ifdef XMLUTILS_PRETTY_PRINTING_SUPPORT
			format_pretty_print_suspended = old_format_pretty_print_suspended;
#endif // XMLUTILS_PRETTY_PRINTING_SUPPORT

			AppendL("/>", FALSE, TRUE);
		}

#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
		if (canonicalize)
			XMLNamespaceDeclaration::Pop(nsstack, level--);
		else
#endif // XMLUTILS_CANONICAL_XML_SUPPORT
			nsnormalizer.EndElement();

		current_wshandling = previous_wshandling;
	}

#undef CANONICALIZE
#undef CANONICALIZE_WITH_COMMENTS
#undef LINEBREAK_AFTER_BEFORE_ROOT_ELEMENT
#undef LINEBREAK_BEFORE_AFTER_ROOT_ELEMENT
}

OP_STATUS
XMLToStringSerializer::DoSerialize(XMLSerializerBackend *backend)
{
	TRAP_AND_RETURN(status, backend->StepL(XMLSerializerBackend::STEP_REPEAT));
	TRAP_AND_RETURN(status, DoSerializeL(backend));

	return OpStatus::OK;
}

XMLToStringSerializer::XMLToStringSerializer(TempBuffer *buffer)
	: buffer(buffer),
	  nsnormalizer(TRUE),
	  split_cdata_sections(TRUE),
	  normalize_namespaces(TRUE),
	  discard_default_content(TRUE),
	  format_pretty_print(FALSE),
	  format_pretty_print_suspended(FALSE),
	  indentlevel(0),
	  indentextra(0),
	  linelength(0),
	  preferredlinelength(80),
	  level(0),
	  charref_format("&#x%x;"),
	  encoding(0),
	  converter(0),
	  converter_buffer(0),
	  error(ERROR_NONE)
#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
	, canonicalize(FALSE)
	, canonicalize_with_comments(FALSE)
#endif // XMLUTILS_CANONICAL_XML_SUPPORT
{
}

/* virtual */
XMLToStringSerializer::~XMLToStringSerializer()
{
	OP_DELETEA(encoding);
	OP_DELETE(converter);
	OP_DELETEA(converter_buffer);
}

/* virtual */ OP_STATUS
XMLToStringSerializer::SetConfiguration(const Configuration &configuration)
{
	if (configuration.document_information)
		RETURN_IF_ERROR(documentinfo.Copy(*configuration.document_information));

	split_cdata_sections = configuration.split_cdata_sections;
	normalize_namespaces = configuration.normalize_namespaces;
	discard_default_content = configuration.discard_default_content;
	format_pretty_print = configuration.format_pretty_print;
	preferredlinelength = configuration.preferred_line_length;
	add_xml_space_attributes = configuration.add_xml_space_attributes;

	if (configuration.use_decimal_character_reference)
		charref_format = "&#%u;";
#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
	else if (canonicalize)
		charref_format = "&#x%X;";
#endif // XMLUTILS_CANONICAL_XML_SUPPORT
	else
		charref_format = "&#x%x;";

	if (configuration.encoding)
	{
		RETURN_IF_ERROR(SetStr(encoding, configuration.encoding));

		OP_STATUS rc =
			OutputConverter::CreateCharConverter(encoding, &converter);
		if (OpStatus::IsError(rc))
			return rc;
		converter_buffer = OP_NEWA(char, 1024);
		converter_buffer_length = 1024;

		if (!converter || !converter_buffer)
			return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

#ifdef XMLUTILS_CANONICAL_XML_SUPPORT

/* virtual */ OP_STATUS
XMLToStringSerializer::SetCanonicalize(CanonicalizeVersion version, BOOL with_comments)
{
	canonicalize = TRUE;
	canonicalize_version = version;
	canonicalize_with_comments = with_comments;

	Configuration configuration;
	configuration.normalize_namespaces = FALSE;
	configuration.discard_default_content = FALSE;
	configuration.add_xml_space_attributes = version != CANONICALIZE_1_0_EXCLUSIVE;
	configuration.encoding = "UTF-8";
	return SetConfiguration(configuration);
}

/* virtual */ OP_STATUS
XMLToStringSerializer::SetInclusiveNamespacesPrefixList(const uni_char *list)
{
	if (!canonicalize || canonicalize_version != CANONICALIZE_1_0_EXCLUSIVE)
		return OpStatus::ERR;

	return inclusive_namespace_prefix_list.Set(list);
}

BOOL
XMLToStringSerializer::IncludeNSDeclaration(XMLCanonicalizeSerializerBackend *backend, XMLNamespaceDeclaration *nsdeclaration)
{
	if (canonicalize_version == CANONICALIZE_1_0)
		return TRUE;
	else if (inclusive_namespace_prefix_list.HasContent())
	{
		const uni_char *list = inclusive_namespace_prefix_list.CStr();
		const uni_char *prefix = nsdeclaration->GetPrefix();

		if (!prefix)
			prefix = UNI_L("#default");

		unsigned prefix_length = uni_strlen(prefix);

		while (*list)
		{
			while (uni_isspace(*list))
				++list;

			if (*list)
			{
				const uni_char *start = list;

				while (*list && !uni_isspace(*list))
					++list;

				if (static_cast<unsigned>(list - start) == prefix_length && uni_strncmp(start, prefix, prefix_length) == 0)
					return TRUE;
			}
		}
	}

	return backend->IsVisiblyUsing(nsdeclaration);
}

#endif // XMLUTILS_CANONICAL_XML_SUPPORT

/* virtual */ OP_STATUS
XMLToStringSerializer::Serialize(MessageHandler *mh, const URL &url, HTML_Element *root, HTML_Element *target, Callback *callback)
{
	TempBuffer::ExpansionPolicy old_expansion_policy = buffer->SetExpansionPolicy(TempBuffer::AGGRESSIVE);
	TempBuffer::CachedLengthPolicy old_cached_length_policy = buffer->SetCachedLengthPolicy(TempBuffer::TRUSTED);

	if (!target)
		target = root;

	current_wshandling = XMLWHITESPACEHANDLING_DEFAULT;

	XMLElementSerializerBackend element_backend(root, target);

#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
	XMLCanonicalizeSerializerBackend canonicalize_backend(&element_backend, canonicalize_with_comments);
	XMLSerializerBackend *backend = canonicalize ? static_cast<XMLSerializerBackend *>(&canonicalize_backend) : &element_backend;

	OP_STATUS status = DoSerialize(backend);
#else // XMLUTILS_CANONICAL_XML_SUPPORT
	OP_STATUS status = DoSerialize(&element_backend);
#endif // XMLUTILS_CANONICAL_XML_SUPPORT

	buffer->SetExpansionPolicy(old_expansion_policy);
	buffer->SetCachedLengthPolicy(old_cached_length_policy);

	if (callback)
		callback->Stopped(status == OpStatus::ERR_NO_MEMORY ? Callback::STATUS_OOM : Callback::STATUS_COMPLETED);

	return status;
}

#ifdef XMLUTILS_XMLFRAGMENT_SUPPORT

/* virtual */ OP_STATUS
XMLToStringSerializer::Serialize(MessageHandler *mh, const URL &url, XMLFragment *fragment, Callback *callback)
{
	TempBuffer::ExpansionPolicy old_expansion_policy = buffer->SetExpansionPolicy(TempBuffer::AGGRESSIVE);
	TempBuffer::CachedLengthPolicy old_cached_length_policy = buffer->SetCachedLengthPolicy(TempBuffer::TRUSTED);

	current_wshandling = XMLWHITESPACEHANDLING_DEFAULT;

	XMLFragmentSerializerBackend fragment_backend(fragment);

	OP_STATUS status = fragment_backend.Construct();

	if (OpStatus::IsSuccess(status))
	{
#ifdef XMLUTILS_CANONICAL_XML_SUPPORT
		XMLCanonicalizeSerializerBackend canonicalize_backend(&fragment_backend, canonicalize_with_comments);
		XMLSerializerBackend *backend = canonicalize ? static_cast<XMLSerializerBackend *>(&canonicalize_backend) : &fragment_backend;

		status = DoSerialize(backend);
#else // XMLUTILS_CANONICAL_XML_SUPPORT
		status = DoSerialize(&fragment_backend);
#endif // XMLUTILS_CANONICAL_XML_SUPPORT
	}

	buffer->SetExpansionPolicy(old_expansion_policy);
	buffer->SetCachedLengthPolicy(old_cached_length_policy);

	if (callback)
		callback->Stopped(status == OpStatus::ERR_NO_MEMORY ? Callback::STATUS_OOM : Callback::STATUS_COMPLETED);

	return status;
}

#endif // XMLUTILS_XMLFRAGMENT_SUPPORT

/* virtual */ XMLSerializer::Error
XMLToStringSerializer::GetError()
{
	return error;
}

#endif // XMLUTILS_XMLTOSTRINGSERIALIZER_SUPPORT
#endif // XMLUTILS_XMLSERIALIZER_SUPPORT
