/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"

#include "modules/dom/src/domhtml/htmlcoll.h"
#include "modules/dom/src/domhtml/htmlmicrodata.h"
#include "modules/dom/src/domhtml/htmldoc.h"
#include "modules/dom/src/domhtml/htmlelem.h"
#include "modules/dom/src/js/window.h"
#include "modules/dom/src/domevents/domevent.h"

#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/util/tempbuf.h"
#include "modules/util/adt/unicharbuffer.h"
#include "modules/dochand/docman.h"
#include "modules/dochand/fdelm.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/stringtokenlist_attribute.h"
#include "modules/url/url_man.h"
#include "modules/url/url_pd.h"
#include "modules/spatial_navigation/handler_api.h"
#include "modules/spatial_navigation/navigation_api.h"


#define THROW_IF_DOC_EXCEPTION( doc_exception )                                                             \
	do {																									\
		switch (doc_exception)																				\
		{																									\
			case ES_DOC_EXCEPTION_NONE:																		\
				break;																						\
			case ES_DOC_EXCEPTION_UNSUPPORTED_OPEN:															\
				return DOM_CALL_INTERNALEXCEPTION(UNSUPPORTED_DOCUMENT_OPEN_ERR);							\
			case ES_DOC_EXCEPTION_XML_OPEN:																	\
				return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);											\
			default:																						\
				OP_ASSERT(!"Unknown doc exception");														\
		}																									\
	} while(0)


/* Defined in htmlelem2.cpp. */
extern OP_STATUS DOM_initCollection(DOM_Collection *&collection, DOM_Object *owner, DOM_Node *root, DOM_HTMLElement_Group group, int private_name);

DOM_HTMLDocument::DOM_HTMLDocument(DOM_DOMImplementation *implementation, BOOL is_xml)
	: DOM_Document(implementation, is_xml),
	  documentnodes(NULL)
{
}

static const OpAtom collection_property_names[] =
{
	OP_ATOM_images,
	OP_ATOM_applets,
	OP_ATOM_links,
	OP_ATOM_forms,
	OP_ATOM_anchors,
	OP_ATOM_embeds,
	OP_ATOM_plugins,
	OP_ATOM_scripts,
	OP_ATOM_all
};

static const int collection_private_names[] =
{
	DOM_PRIVATE_images,
	DOM_PRIVATE_applets,
	DOM_PRIVATE_links,
	DOM_PRIVATE_forms,
	DOM_PRIVATE_anchors,
	DOM_PRIVATE_embeds,
	DOM_PRIVATE_plugins,
	DOM_PRIVATE_scripts,
	DOM_PRIVATE_all
};

static const DOM_HTMLElement_Group collection_groups[] =
{
	IMAGES,
	APPLETS,
	LINKS,
	FORMS,
	ANCHORS,
	EMBEDS,
	PLUGINS,
	SCRIPTS,
	ALL
};

ES_GetState
DOM_HTMLDocument::GetCollection(ES_Value *value, OpAtom property_name)
{
	if (value)
	{
		int index, private_name;

		for (index = 0; collection_property_names[index] != property_name; ++index)
		{
			// Loop until we find the right property
		}

		ES_GetState state = DOMSetPrivate(value, private_name = collection_private_names[index]);
		if (state == GET_FAILED)
		{
			DOM_Collection *collection = NULL;
			GET_FAILED_IF_ERROR(DOM_initCollection(collection, this, root, collection_groups[index], private_name));
			DOMSetObject(value, collection);
		}
	}

	return GET_SUCCESS;
}

/* virtual */ OP_STATUS
DOM_HTMLDocument::SetRootElement(DOM_Element *new_root)
{
	OP_STATUS ret = DOM_Document::SetRootElement(new_root);

	unsigned i;
	for (i=0; i < sizeof(collection_private_names)/sizeof(int); i++)
	{
		ES_Value value;
		ES_GetState state = DOMSetPrivate(&value, collection_private_names[i]);
		if (state == GET_SUCCESS)
		{
			DOM_Collection *collection = DOM_VALUE2OBJECT(value, DOM_Collection);
			collection->SetRoot(root, TRUE);
		}
	}

	return ret;
}

/* status */ OP_STATUS
DOM_HTMLDocument::Make(DOM_HTMLDocument *&document, DOM_DOMImplementation *implementation, BOOL create_placeholder, BOOL is_xml)
{
	DOM_Runtime *runtime = implementation->GetRuntime();
	RETURN_IF_ERROR(DOMSetObjectRuntime(document = OP_NEW(DOM_HTMLDocument, (implementation, is_xml)), runtime, runtime->GetPrototype(DOM_Runtime::HTMLDOCUMENT_PROTOTYPE), "HTMLDocument"));

	if (create_placeholder)
		RETURN_IF_ERROR(document->ResetPlaceholderElement());

	return OpStatus::OK;
}

/* virtual */ BOOL
DOM_HTMLDocument::SecurityCheck(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (OriginLoadCheck(GetRuntime(), origining_runtime))
	{
	case YES: return TRUE;
	case MAYBE: return property_name == OP_ATOM_location || property_name == OP_ATOM_document || property_name == OP_ATOM_readyState;
	}

	return FALSE;
}

/* virtual */ ES_GetState
DOM_HTMLDocument::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (!documentnodes && root)
	{
		GET_FAILED_IF_ERROR(DOM_initCollection(documentnodes, this, this, DOCUMENTNODES, DOM_PRIVATE_documentNodes));
		documentnodes->SetPreferWindowObjects();
		documentnodes->SetOwner(this);
	}

	if (documentnodes)
	{
		ES_GetState result = documentnodes->GetName(property_name, property_code, value, origining_runtime);
		if (result != GET_FAILED)
			return result;
	}

	return DOM_Document::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_HTMLDocument::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	DOM_EnvironmentImpl *environment = GetEnvironment();
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	FramesDocument *frames_doc = environment->GetFramesDocument();

	switch(property_name)
	{
	case OP_ATOM_title:
		return GetTitle(value);

	case OP_ATOM_domain:
		if (value)
		{
			DOMSetString(value);
			if (!runtime->GetMutableOrigin()->IsUniqueOrigin())
			{
				const uni_char *domain;
				runtime->GetDomain(&domain, NULL, NULL);
				DOMSetString(value, domain);
			}
		}
		return GET_SUCCESS;

	case OP_ATOM_URL:
		if (value)
		{
			const uni_char *url;

			if (frames_doc)
				url = frames_doc->GetURL().GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden).CStr();
			else
				url = NULL;

			DOMSetString(value, url);
		}
		return GET_SUCCESS;

	case OP_ATOM_head:
		if (value)
		{
			HTML_Element *element = GetPlaceholderElement();
			if (element)
			{
				element = element->FirstChildActual();
				while (element && !element->IsMatchingType(HE_HTML, NS_HTML))
					element = element->SucActual();

				if (element)
				{
					element = element->FirstChildActual();
					while (element && !element->IsMatchingType(HE_HEAD, NS_HTML))
						element = element->SucActual();
				}
			}
			return DOMSetElement(value, element);
		}
		return GET_SUCCESS;

	case OP_ATOM_body:
		if (value)
		{
			DOM_Element *element;

			GET_FAILED_IF_ERROR(GetBodyNode(element));

			DOMSetObject(value, element);
		}
		return GET_SUCCESS;

	case OP_ATOM_bgColor:
	case OP_ATOM_fgColor:
	case OP_ATOM_linkColor:
	case OP_ATOM_alinkColor:
	case OP_ATOM_vlinkColor:
		if (value)
		{
			DOM_Element *element;

			GET_FAILED_IF_ERROR(GetBodyNode(element));

			if (element && element->GetThisElement()->IsMatchingType(HE_BODY, NS_HTML))
			{
				if (property_name == OP_ATOM_fgColor)
					property_name = OP_ATOM_text;
				else if (property_name == OP_ATOM_linkColor)
					property_name = OP_ATOM_link;
				else if (property_name == OP_ATOM_alinkColor)
					property_name = OP_ATOM_aLink;
				else if (property_name == OP_ATOM_vlinkColor)
					property_name = OP_ATOM_vLink;

				return element->GetName(property_name, value, origining_runtime);
			}
			else
				DOMSetString(value);
		}
		return GET_SUCCESS;

	case OP_ATOM_images:
	case OP_ATOM_applets:
	case OP_ATOM_links:
	case OP_ATOM_forms:
	case OP_ATOM_anchors:
	case OP_ATOM_embeds:
	case OP_ATOM_plugins:
	case OP_ATOM_scripts:
	case OP_ATOM_all:
		return GetCollection(value, property_name);

	case OP_ATOM_frames:
		DOMSetObject(value, environment->GetWindow());
		return GET_SUCCESS;

	case OP_ATOM_compatMode:
		if (value)
			if (HLDocProfile *hld_profile = GetHLDocProfile())
			{
				if (hld_profile->IsInStrictMode())
					DOMSetString(value, UNI_L("CSS1Compat"));
				else
					DOMSetString(value, UNI_L("BackCompat"));
			}
		return GET_SUCCESS;

	case OP_ATOM_activeElement:
		if (value)
			if (frames_doc && IsMainDocument())
				return DOMSetElement(value, frames_doc->DOMGetActiveElement());
			else
				DOMSetNull(value);
		return GET_SUCCESS;
	}

	return DOM_Document::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLDocument::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	ES_GetState state;
	ES_Value value2;

	switch(property_name)
	{
	case OP_ATOM_title:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else
			return SetTitle(value, origining_runtime);

	case OP_ATOM_body:
		if (value->type != VALUE_OBJECT || !root)
			return DOM_PUTNAME_DOMEXCEPTION(HIERARCHY_REQUEST_ERR);
		else
			return ((DOM_HTMLElement *) root)->PutChildElement(OP_ATOM_body, value, (DOM_Runtime *) origining_runtime, NULL);

	case OP_ATOM_bgColor:
	case OP_ATOM_fgColor:
	case OP_ATOM_linkColor:
	case OP_ATOM_alinkColor:
	case OP_ATOM_vlinkColor:
		state = GetName(OP_ATOM_body, &value2, origining_runtime);
		if (state == GET_NO_MEMORY)
			return PUT_NO_MEMORY;
		else if (state == GET_SUCCESS && value2.type == VALUE_OBJECT)
		{
			DOM_Node *node = DOM_VALUE2OBJECT(value2, DOM_Node);

			if (property_name == OP_ATOM_fgColor)
				property_name = OP_ATOM_text;
			else if (property_name == OP_ATOM_linkColor)
				property_name = OP_ATOM_link;
			else if (property_name == OP_ATOM_alinkColor)
				property_name = OP_ATOM_aLink;
			else if (property_name == OP_ATOM_vlinkColor)
				property_name = OP_ATOM_vLink;

			if (value->type == VALUE_NULL)
				DOMSetString(value);

			return node->PutName(property_name, value, origining_runtime);
		}
		return PUT_SUCCESS;

	case OP_ATOM_domain:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else
		{
			OP_STATUS status = GetRuntime()->SetDomainChecked(value->value.string);
			if (OpStatus::IsMemoryError(status))
				return PUT_NO_MEMORY;
			else if (OpStatus::IsError(status))
				return PUT_SECURITY_VIOLATION;
			else
				return PUT_SUCCESS;
		}

	case OP_ATOM_head:
		return PUT_SUCCESS;

	case OP_ATOM_URL:
	case OP_ATOM_images:
	case OP_ATOM_applets:
	case OP_ATOM_links:
	case OP_ATOM_embeds:
	case OP_ATOM_forms:
	case OP_ATOM_anchors:
	case OP_ATOM_compatMode:
		return PUT_READ_ONLY;
	}

	return DOM_Document::PutName (property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLDocument::PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object *restart_object)
{
	if (property_name == OP_ATOM_body)
		return ((DOM_HTMLElement *) root)->PutChildElement(OP_ATOM_body, value, (DOM_Runtime *) origining_runtime, restart_object);
	else if (property_name == OP_ATOM_title)
		if (HTML_Element *element = GetElement(HE_TITLE))
		{
			DOM_Node *node;
			PUT_FAILED_IF_ERROR(GetEnvironment()->ConstructNode(node, element, this));
			return node->PutNameRestart(OP_ATOM_text, value, origining_runtime, restart_object);
		}
		else
			return PUT_SUCCESS;
	else
		return PUT_FAILED;
}

/* virtual */ ES_GetState
DOM_HTMLDocument::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	ES_GetState result = DOM_Object::FetchPropertiesL(enumerator, origining_runtime);
	if (result != GET_SUCCESS)
		return result;

	if (root)
	{
		if (!documentnodes)
		{
			GET_FAILED_IF_ERROR(DOM_initCollection(documentnodes, this, this, DOCUMENTNODES, DOM_PRIVATE_documentNodes));
			documentnodes->SetPreferWindowObjects();
			documentnodes->SetOwner(this);
		}

		ES_GetState result = documentnodes->FetchPropertiesL(enumerator, origining_runtime);
		OP_ASSERT(result != GET_SUSPEND);
		if (result != GET_SUCCESS)
			return result;
	}

	DOM_Event::FetchNamedHTMLDocEventPropertiesL(enumerator);
	return GET_SUCCESS;
}

HTML_Element *
DOM_HTMLDocument::GetElement(HTML_ElementType type)
{
	HTML_Element *element = GetPlaceholderElement();

	while (element)
		if (element->GetNsType() == NS_HTML && element->Type() == type)
			break;
		else
			element = element->NextActual();

	return element;
}

OP_STATUS
DOM_HTMLDocument::GetBodyNode(DOM_Element *&element)
{
	HTML_Element *body = NULL;

	if (IsMainDocument())
		if (HLDocProfile *hld_profile = GetHLDocProfile())
			if ((body = hld_profile->GetBodyElm()) != NULL)
				if (!body->IsIncludedActual())
					body = NULL;

	if (!body && placeholder)
	{
		HTML_Element *child = placeholder->FirstChildActual();
		while (child)
			if (child->IsMatchingType(HE_BODY, NS_HTML) || child->IsMatchingType(HE_FRAMESET, NS_HTML))
			{
				body = child;
				break;
			}
			else
				child = child->NextActual();
	}

	if (body)
	{
		DOM_Node *node;

		RETURN_IF_ERROR(GetEnvironment()->ConstructNode(node, body, this));

		element = static_cast<DOM_Element *>(node);
	}
	else
		element = NULL;

	return OpStatus::OK;
}

ES_GetState
DOM_HTMLDocument::GetTitle(ES_Value *value)
{
	if (FramesDocument *frames_doc = GetFramesDocument())
	{
		// Can't use GetLogicalDocument since it will be NULL for
		// something that isn't a MainDocument.
		if (LogicalDocument* logdoc = frames_doc->GetLogicalDocument())
		{
			TempBuffer *buffer = GetEmptyTempBuf();
			const uni_char* title = logdoc->GetTitle(GetPlaceholderElement(), buffer);
			DOMSetString(value, title);
			return GET_SUCCESS;
		}
	}

	DOMSetString(value);
	return GET_SUCCESS;
}

ES_PutState
DOM_HTMLDocument::SetTitle(ES_Value *value, ES_Runtime *origining_runtime)
{
	HTML_Element *title_element = GetElement(HE_TITLE);
	DOM_EnvironmentImpl *environment = GetEnvironment();

	if (!title_element)
	{
		/* Invent <title> on the spot... */
		HTML_Element *head_element = GetElement(HE_HEAD);
		if (!head_element)
		{
			/* ..and its <head> parent, if not there. */

			if (!GetRootElement())
				/* nowhere to add, quietly ignore title update. */
				return PUT_SUCCESS;

			PUT_FAILED_IF_ERROR(HTML_Element::DOMCreateElement(head_element, environment, UNI_L("head"), NS_IDX_HTML));
			PUT_FAILED_IF_ERROR(GetRootElement()->GetThisElement()->DOMInsertChild(environment, head_element, NULL));
			/* the element has been added to the tree, hence no leak - error stems from signalling the insertion. */
		}

		if (head_element)
		{
			PUT_FAILED_IF_ERROR(HTML_Element::DOMCreateElement(title_element, environment, UNI_L("title"), NS_IDX_HTML));
			PUT_FAILED_IF_ERROR(head_element->DOMInsertChild(environment, title_element, NULL));
		}

		/* Note/FIXME: the insertion of a <title> element, and possibly <head>, is synchronous and does not issue any mutation events, like
		   would be done when using e.g., DOM_Node::insertBefore(). Doing so would be preferable, but would turn SetTitle() into an async operation
		   to handle the invocation of mutation event listeners. Leave as is for now, but may (have to) revisit. */
	}

	if (title_element)
	{
		DOM_Node *title_node;
		PUT_FAILED_IF_ERROR(environment->ConstructNode(title_node, title_element, this));
		return title_node->PutName(OP_ATOM_text, value, origining_runtime);
	}

	return PUT_SUCCESS;
}

OP_STATUS
DOM_HTMLDocument::GetAllCollection(DOM_Collection *&all)
{
	ES_Value value;
	ES_GetState state = GetCollection(&value, OP_ATOM_all);

	if (state == GET_NO_MEMORY)
		return OpStatus::ERR_NO_MEMORY;

	all = DOM_VALUE2OBJECT(value, DOM_Collection);
	return OpStatus::OK;
}

class DOM_WriteState : public DOM_Object
{
public:
	~DOM_WriteState() { OP_DELETEA(write_string.string); }

	ES_ValueString	write_string;
};

/* virtual */ int
DOM_HTMLDocument::write(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_DOCUMENT, DOM_Document);

	if (FramesDocument* frames_doc = document->GetFramesDocument())
	{
		OP_BOOLEAN result;

		// data = 0: write  data = 1: writeln
		// writeln() -> writeln("") -> a single newline
		ES_Value empty_string;
		ES_ValueString string_holder;
		OpAutoArray<uni_char> string_anchor(NULL);
		if (argc == 0 && data == 1)
		{
			empty_string.type = VALUE_STRING_WITH_LENGTH;
			string_holder.string = UNI_L("");
			string_holder.length = 0;
			empty_string.value.string_with_length = &string_holder;
			argc = 1;
			argv = &empty_string;
		}

		if (argc > 0)
		{
			UniCharBuffer buffer;
			if (argc > 1)
			{
				for (int i = 0; i < argc; ++i)
					if (argv[i].type == VALUE_STRING_WITH_LENGTH)
						CALL_FAILED_IF_ERROR(buffer.AppendString(argv[i].value.string_with_length->string, argv[i].value.string_with_length->length));

				string_holder.length = buffer.UniLength();
				uni_char* buffer_copy = buffer.Copy(FALSE);
				if (!buffer_copy)
					return ES_NO_MEMORY;
				string_anchor.reset(buffer_copy);
				string_holder.string = buffer_copy;
			}
			else if (argv[0].type == VALUE_STRING_WITH_LENGTH)
				string_holder = *argv[0].value.string_with_length;
			else
				return ES_FAILED;

			// FramesDocument::ESWrite can return OpStatus::ERR_NOT_SUPPORT (if SCRIPTS_IN_XML_HACK is
			// defined and this is an XML document).

			ESDocException doc_exception;
			result = frames_doc->ESWrite(origining_runtime, string_holder.string, string_holder.length, data == 1, &doc_exception);
			THROW_IF_DOC_EXCEPTION(doc_exception);
			CALL_FAILED_IF_ERROR(result);

			if (result == OpBoolean::IS_FALSE)
			{
				DOM_WriteState* write_state;
				CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(write_state = OP_NEW(DOM_WriteState, ()), document->GetRuntime()));

				if (string_anchor.get())
				{
					write_state->write_string = string_holder;
					string_anchor.release();
				}
				else
				{
					write_state->write_string.string = OP_NEWA(uni_char, string_holder.length);
					if (!write_state->write_string.string)
						return ES_NO_MEMORY;

					op_memcpy((void*)write_state->write_string.string, string_holder.string, UNICODE_SIZE(string_holder.length));
					write_state->write_string.length = string_holder.length;
				}

				DOMSetObject(return_value, write_state);

				return ES_SUSPEND | ES_RESTART;
			}
		}
		else if (argc < 0)
		{
			DOM_WriteState* write_state = DOM_VALUE2OBJECT(*return_value, DOM_WriteState);
			ESDocException doc_exception;
			result = frames_doc->ESWrite(origining_runtime, write_state->write_string.string, write_state->write_string.length, data == 1, &doc_exception);
			if (result == OpBoolean::IS_FALSE)
				return ES_SUSPEND | ES_RESTART;
			THROW_IF_DOC_EXCEPTION(doc_exception);
			CALL_FAILED_IF_ERROR(result);

			if (OpStatus::IsError(result))
				return result == OpStatus::ERR_NO_MEMORY ? ES_NO_MEMORY : ES_FAILED;

			OP_ASSERT(result == OpBoolean::IS_TRUE);
		}
    }

	return ES_FAILED;
}

/* virtual */ int
DOM_HTMLDocument::close(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_DOCUMENT, DOM_Document);

	if (FramesDocument *frames_doc = document->GetFramesDocument())
		CALL_FAILED_IF_ERROR(frames_doc->ESClose(origining_runtime));

	return ES_FAILED;
}

/* virtual */ int
DOM_HTMLDocument::open(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_DOCUMENT, DOM_Document);

	if (argc < 0 || argc > 2)
		/* If we're called with more than 2 arguments, the caller
		   probably meant to be calling window.open... */
		return JS_Window::open(document->GetEnvironment()->GetWindow(), argv, argc, return_value, origining_runtime);

	if (FramesDocument* frames_doc = document->GetFramesDocument())
	{
		const uni_char* mime_type = NULL;

		if (argc >= 1)
			mime_type = argv[0].value.string;

		LogicalDocument* logdoc = document->GetLogicalDocument();

		BOOL open_should_open_new_doc = TRUE;

		if (logdoc)
		{
			if (!logdoc->IsParsed())
				open_should_open_new_doc = FALSE;
#ifdef DELAYED_SCRIPT_EXECUTION
			else if (logdoc->GetHLDocProfile()->ESHasDelayedScripts())
				open_should_open_new_doc = FALSE;
#endif // DELAYED_SCRIPT_EXECUTION
		}

		if (open_should_open_new_doc)
		{
			// FramesDocument::ESOpen can return OpStatus::ERR_NOT_SUPPORT if SCRIPTS_IN_XML_HACK is
			// defined and this is an inline script in an XML document.

			ESDocException doc_exception;
			OP_BOOLEAN result = frames_doc->ESOpen(origining_runtime, NULL, FALSE, mime_type, NULL, &doc_exception);
			THROW_IF_DOC_EXCEPTION(doc_exception);
			CALL_FAILED_IF_ERROR(result);

			if (frames_doc->GetURL().IsEmpty() && frames_doc->GetHLDocProfile())
			{
                FramesDocument *orig_frames_doc = origining_runtime->GetFramesDocument();

                const URL *url = orig_frames_doc->GetHLDocProfile()->GetURL();
				if (url == NULL)
					url = &(orig_frames_doc->GetURL());

				frames_doc->GetHLDocProfile()->SetBaseURL(url);
			}
		}

		FramesDocument *new_frames_doc = frames_doc->GetDocManager()->GetCurrentDoc();

		if (new_frames_doc && new_frames_doc != frames_doc)
		{
			OP_STATUS status = new_frames_doc->ConstructDOMEnvironment();
			if (OpStatus::IsMemoryError(status))
				return ES_NO_MEMORY;
			else if (OpStatus::IsError(status))
				DOMSetNull(return_value);
			else
			{
				DOM_EnvironmentImpl *environment = (DOM_EnvironmentImpl *) new_frames_doc->GetDOMEnvironment();

#ifdef SELFTEST
				// HTML 5 specs that anything but text/html should be handled as plain text.
				// That is not how the selftest framework expects it to work though.
				if (mime_type && g_selftest.running)
					if (uni_str_eq(mime_type, "text/xml") || uni_str_eq(mime_type, "application/xml"))
						environment->SetIsXML();
					else if (uni_str_eq(mime_type, "application/xhtml+xml"))
						environment->SetIsXHTML();
#endif // SELFTEST

				DOM_Object *document_obj;
				CALL_FAILED_IF_ERROR(environment->GetProxyDocument(document_obj, origining_runtime));
				DOMSetObject(return_value, document_obj);
			}
		}
		else
		{
			DOM_Object *document_obj;
			CALL_FAILED_IF_ERROR(document->GetEnvironment()->GetProxyDocument(document_obj, origining_runtime));
			DOMSetObject(return_value, document_obj);
		}
	}
	else
		DOMSetObject(return_value, this_object);

	return ES_VALUE;
}

/* virtual */ int
DOM_HTMLDocument::getElementsByName(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_HTML_DOCUMENT, DOM_HTMLDocument);
	DOM_CHECK_ARGUMENTS("s");

	DOM_NameCollectionFilter filter(NULL, argv[0].value.string, TRUE, FALSE);
	DOM_Collection *collection;

	CALL_FAILED_IF_ERROR(DOM_Collection::MakeNodeList(collection, document->GetEnvironment(), document->GetRootElement(), TRUE, TRUE, filter));

	DOMSetObject(return_value, *collection);
	return ES_VALUE;
}

#define WHITESPACE_L UNI_L(" \t\n\f\r")

/* static */ int
DOM_HTMLDocument::getItems(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_HTML_DOCUMENT, DOM_HTMLDocument);
	DOM_CHECK_ARGUMENTS("|s");
	DOM_ToplevelItemCollectionFilter filter;

	if (argc >= 1)
	{
		// Split argument on whitespaces.
		const uni_char* arg_itemtypes_string = argv[0].value.string;
		while (*arg_itemtypes_string)
		{
			// skip whitespace
			arg_itemtypes_string += uni_strspn(arg_itemtypes_string, WHITESPACE_L);
			// find size of next token
			size_t token_len = uni_strcspn(arg_itemtypes_string, WHITESPACE_L);
			if (token_len)
				if (OpStatus::IsMemoryError(filter.AddType(arg_itemtypes_string, token_len)))
					return ES_NO_MEMORY;
			arg_itemtypes_string += token_len;
		}
	}

	DOM_Collection *collection;
	CALL_FAILED_IF_ERROR(DOM_Collection::MakeNodeList(collection, document->GetEnvironment(), document, FALSE, TRUE, filter));
	DOMSetObject(return_value, collection);
	return ES_VALUE;
}

#ifdef _SPAT_NAV_SUPPORT_

/* static */ int
DOM_HTMLDocument::moveFocus(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int direction)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_HTML_DOCUMENT, DOM_HTMLDocument);

	if (FramesDocument* frames_doc = document->GetFramesDocument())
		if (OpSnHandler* handler = frames_doc->GetWindow()->GetSnHandlerConstructIfNeeded())
		{
			DOMSetBoolean(return_value, handler->MarkNextItemInDirection(direction));
			return ES_VALUE;
		}

	return ES_FAILED;
}

#endif // _SPAT_NAV_SUPPORT_

/* Empty function, defined in domobj.cpp. */
extern DOM_FunctionImpl DOM_dummyMethod;

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_HTMLDocument)
	DOM_FUNCTIONS_FUNCTION(DOM_HTMLDocument, DOM_HTMLDocument::open, "open", "ss-")
	DOM_FUNCTIONS_FUNCTION(DOM_HTMLDocument, DOM_dummyMethod, "clear", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_HTMLDocument, DOM_HTMLDocument::close, "close", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_HTMLDocument, DOM_HTMLDocument::getElementsByName, "getElementsByName", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_HTMLDocument, DOM_HTMLDocument::getItems, "getItems", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_HTMLDocument, DOM_dummyMethod, "captureEvents", "n-")
	DOM_FUNCTIONS_FUNCTION(DOM_HTMLDocument, DOM_dummyMethod, "releaseEvents", "n-")
DOM_FUNCTIONS_END(DOM_HTMLDocument)

DOM_FUNCTIONS_WITH_DATA_START(DOM_HTMLDocument)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_HTMLDocument, DOM_HTMLDocument::write, 0, "write", "z")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_HTMLDocument, DOM_HTMLDocument::write, 1, "writeln", "z")
#ifdef _SPAT_NAV_SUPPORT_
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_HTMLDocument, DOM_HTMLDocument::moveFocus, DIR_UP, "moveFocusUp", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_HTMLDocument, DOM_HTMLDocument::moveFocus, DIR_DOWN, "moveFocusDown", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_HTMLDocument, DOM_HTMLDocument::moveFocus, DIR_LEFT, "moveFocusLeft", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_HTMLDocument, DOM_HTMLDocument::moveFocus, DIR_RIGHT, "moveFocusRight", 0)
#endif // _SPAT_NAV_SUPPORT_
DOM_FUNCTIONS_WITH_DATA_END(DOM_HTMLDocument)

