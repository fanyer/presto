/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcss/cssstylesheet.h"
#include "modules/dom/src/domcss/cssrulelist.h"
#include "modules/dom/src/domcss/cssrule.h"
#include "modules/dom/src/domcore/element.h"
#include "modules/doc/frm_doc.h"

#include "modules/logdoc/htm_elm.h"

DOM_CSSStyleSheet::DOM_CSSStyleSheet(DOM_Node *owner_node, DOM_CSSRule *import_rule)
	: DOM_StyleSheet(owner_node), m_css_sheet(NULL), m_import_rule(import_rule)
{
}

/* static */ OP_STATUS
DOM_CSSStyleSheet::Make(DOM_CSSStyleSheet *&stylesheet, DOM_Node *owner_node, DOM_CSSRule *import_rule)
{
	DOM_Runtime* runtime = owner_node->GetRuntime();
	RETURN_IF_ERROR(DOMSetObjectRuntime(stylesheet = OP_NEW(DOM_CSSStyleSheet, (owner_node, import_rule)), runtime, runtime->GetPrototype(DOM_Runtime::CSSSTYLESHEET_PROTOTYPE), "CSSStyleSheet"));
	stylesheet->m_css_sheet = OP_NEW(CSS_DOMStyleSheet, (runtime->GetEnvironment(), owner_node->GetThisElement()));
	if (!stylesheet->m_css_sheet)
		return OpStatus::ERR_NO_MEMORY;
	else
		return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_CSSStyleSheet::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	HTML_Element *element = ((DOM_Element *) owner_node)->GetThisElement();

	if (property_name == OP_ATOM_ownerRule)
	{
		if (value)
		{
			if (m_import_rule)
				DOMSetObject(value, m_import_rule);
			else
				DOMSetNull(value);
		}
		return GET_SUCCESS;
	}
	else if (property_name == OP_ATOM_cssRules)
	{
		// Origin check. Not allowed to see contents of linked in stylesheet from other server.
		if (element->IsMatchingType(HE_LINK, NS_HTML))
		{
			URL origin_url(element->GetLinkOriginURL());
			if (!origin_url.IsEmpty() && !OriginCheck(origin_url, origining_runtime))
				return GET_SECURITY_VIOLATION;
		}

		if (value)
		{
			DOM_CSSRuleList *rulelist;
			GET_FAILED_IF_ERROR(DOM_CSSRuleList::GetFromListable(rulelist, this));
			DOMSetObject(value, rulelist);
		}
		return GET_SUCCESS;
	}
	else if (property_name == OP_ATOM_disabled)
	{
		if (value)
			DOMSetBoolean(value, element->DOMGetStylesheetDisabled(GetEnvironment()));
		return GET_SUCCESS;
	}
	else
		return DOM_StyleSheet::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_CSSStyleSheet::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_ownerRule || property_name == OP_ATOM_cssRules)
		return PUT_READ_ONLY;
	else
		return DOM_StyleSheet::PutName(property_name, value, origining_runtime);
}

/* static */ int
DOM_CSSStyleSheet::deleteRule(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(sheet, DOM_TYPE_CSS_STYLESHEET, DOM_CSSStyleSheet);
	DOM_CHECK_ARGUMENTS("N");

	unsigned index = TruncateDoubleToUInt(argv[0].value.number);

	CSS_DOMException ex;
	OP_STATUS stat = sheet->m_css_sheet->DeleteRule(index, ex);
	if (OpStatus::IsError(stat))
		if (ex == CSS_DOMEXCEPTION_INDEX_SIZE_ERR)
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	return ES_FAILED;
}

/* static */ int
DOM_CSSStyleSheet::insertRule(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(sheet, DOM_TYPE_CSS_STYLESHEET, DOM_CSSStyleSheet);
	DOM_CHECK_ARGUMENTS("sN");

	unsigned index = TruncateDoubleToUInt(argv[1].value.number);

	CSS_DOMException ex;
	OP_STATUS stat = sheet->m_css_sheet->InsertRule(argv[0].value.string, index, ex);
	if (OpStatus::IsMemoryError(stat))
		return ES_NO_MEMORY;
	else if (OpStatus::IsError(stat))
		if (ex == CSS_DOMEXCEPTION_SYNTAX_ERR)
			return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);
		else if (ex == CSS_DOMEXCEPTION_HIERARCHY_REQUEST_ERR)
			return DOM_CALL_DOMEXCEPTION(HIERARCHY_REQUEST_ERR);
		else if (ex == CSS_DOMEXCEPTION_INDEX_SIZE_ERR)
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
		else if (ex == CSS_DOMEXCEPTION_NO_MODIFICATION_ALLOWED_ERR)
			return DOM_CALL_DOMEXCEPTION(NO_MODIFICATION_ALLOWED_ERR);

	DOMSetNumber(return_value, argv[1].value.number);
	return ES_VALUE;
}

/* virtual */ void
DOM_CSSStyleSheet::GCTrace()
{
	if (m_import_rule)
		GCMark(m_import_rule);

	DOM_StyleSheet::GCTrace();
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_CSSStyleSheet)
	DOM_FUNCTIONS_FUNCTION(DOM_CSSStyleSheet, DOM_CSSStyleSheet::deleteRule, "deleteRule", "n-")
	DOM_FUNCTIONS_FUNCTION(DOM_CSSStyleSheet, DOM_CSSStyleSheet::insertRule, "insertRule", "sn-")
DOM_FUNCTIONS_END(DOM_CSSStyleSheet)

