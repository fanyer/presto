/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domselectors/domselector.h"

#ifdef DOM_SELECTORS_API

#include "modules/dom/src/domcore/element.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domcore/domstaticnodelist.h"
#include "modules/dom/src/domcore/docfrag.h"
#include "modules/dom/src/domenvironmentimpl.h"

#include "modules/style/css_dom.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/logdoc/logdoc.h"

#ifdef STYLE_DOM_SELECTORS_API

class DOM_QuerySelectorListener : public CSS_QuerySelectorListener
{
public:
	virtual OP_STATUS MatchElement(HTML_Element* element) { return m_elements.Add(element); }
	OpVector<HTML_Element> m_elements;
};

class DOM_MatchesSelectorListener : public CSS_QuerySelectorListener
{
public:
	DOM_MatchesSelectorListener() : m_match(FALSE) {}
	virtual OP_STATUS MatchElement(HTML_Element* element) { m_match = TRUE; return OpStatus::OK; }
	BOOL Matched() const { return m_match; }
private:
	BOOL m_match;
};

#endif // STYLE_DOM_SELECTORS_API

/* static */ int
DOM_Selector::querySelector(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OBJECT);
#ifdef STYLE_DOM_SELECTORS_API
	if (!this_object->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	DOM_Document* dom_doc;
	DOM_Element* dom_elm;
	HTML_Element* root = NULL;
	DOM_Node* dom_root;

	DOM_CHECK_ARGUMENTS("s");

	const uni_char* selectors = argv[0].value.string;

	if ((data & QUERY_ELEMENT))
	{
		DOM_THIS_OBJECT_EXISTING(dom_elm, DOM_TYPE_ELEMENT, DOM_Element);
		dom_doc = dom_elm->GetOwnerDocument();
		dom_root = dom_elm;
		root = dom_elm->GetThisElement();
	}
	else if ((data & QUERY_DOCFRAG))
	{
		DOM_DocumentFragment* dom_frag;
		DOM_THIS_OBJECT_EXISTING(dom_frag, DOM_TYPE_DOCUMENTFRAGMENT, DOM_DocumentFragment);
		dom_doc = dom_frag->GetOwnerDocument();
		dom_root = dom_frag;
		root = dom_frag->GetPlaceholderElement();
	}
	else
	{
		DOM_THIS_OBJECT_EXISTING(dom_doc, DOM_TYPE_DOCUMENT, DOM_Document);
		DOM_Element* dom_elm = dom_doc->GetRootElement();
		dom_root = dom_doc;
		if (dom_elm)
			root = dom_elm->GetThisElement()->Parent();
	}

	if (root)
	{
		HLDocProfile* hld_profile = dom_doc->GetHLDocProfile();
		if (!hld_profile)
			return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

		BOOL all = (data & QUERY_ALL) != 0;
		BOOL matches = (data & QUERY_MATCHES) != 0;

		if (all)
		{
			DOM_StaticNodeList *collection = NULL;
			if (dom_doc->GetEnvironment()->FindQueryResult(collection, selectors, data, dom_root))
			{
				DOM_Object::DOMSetObject(return_value, collection);
				return ES_VALUE;
			}
		}

		CSS_DOMException ex;
		OP_STATUS stat;

		if (matches)
		{
			DOM_MatchesSelectorListener listener;
			stat = CSS_QuerySelector(hld_profile, selectors, root, CSS_QUERY_MATCHES_SELECTOR, &listener, ex);

			if (OpStatus::IsSuccess(stat))
				DOM_Object::DOMSetBoolean(return_value, listener.Matched());
		}
		else
		{
			DOM_QuerySelectorListener listener;
			stat = CSS_QuerySelector(hld_profile, selectors, root, all ? CSS_QUERY_ALL : 0, &listener, ex);

			if (OpStatus::IsSuccess(stat))
			{
				// Make StaticNodeList.
				if (all)
				{
					DOM_StaticNodeList* nodelist;
					CALL_FAILED_IF_ERROR(DOM_StaticNodeList::Make(nodelist, listener.m_elements, dom_doc));
					CALL_FAILED_IF_ERROR(dom_doc->GetEnvironment()->AddQueryResult(nodelist, selectors, data, dom_root));
					DOM_Object::DOMSetObject(return_value, nodelist);
				}
				else
				{
					OP_ASSERT(listener.m_elements.GetCount() <= 1);
					if (listener.m_elements.GetCount() > 0)
					{
						DOM_Node* node;
						CALL_FAILED_IF_ERROR(dom_doc->GetEnvironment()->ConstructNode(node, listener.m_elements.Get(0), dom_doc));
						DOM_Object::DOMSetObject(return_value, node);
					}
					else
						DOM_Object::DOMSetNull(return_value);
				}
			}
		}

		if (OpStatus::IsError(stat))
		{
			if (OpStatus::IsMemoryError(stat))
				return ES_NO_MEMORY;
			else
			{
				OP_ASSERT(ex == CSS_DOMEXCEPTION_SYNTAX_ERR);
				return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);
			}
		}
		else
			return ES_VALUE;
	}

	return ES_FAILED;

#else // STYLE_DOM_SELECTORS_API

	return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

#endif // STYLE_DOM_SELECTORS_API
}

#endif // DOM_SELECTORS_API
