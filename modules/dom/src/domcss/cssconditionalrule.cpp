/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domcss/cssconditionalrule.h"
#include "modules/dom/src/domcss/cssstylesheet.h"
#include "modules/dom/src/domcss/cssrulelist.h"

#include "modules/style/css_dom.h"

DOM_CSSConditionalRule::DOM_CSSConditionalRule(DOM_CSSStyleSheet* sheet, CSS_DOMRule* css_rule)
	: DOM_CSSRule(sheet, css_rule)
{
}

/* virtual */ ES_GetState
DOM_CSSConditionalRule::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_cssRules)
	{
		if (value)
		{
			DOM_CSSRuleList *rulelist;
			GET_FAILED_IF_ERROR(DOM_CSSRuleList::GetFromListable(rulelist, this));
			DOMSetObject(value, rulelist);
		}
		return GET_SUCCESS;
	}
	else
		return DOM_CSSRule::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_CSSConditionalRule::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_cssRules)
		return PUT_READ_ONLY;
	else
		return DOM_CSSRule::PutName(property_name, value, origining_runtime);
}

/*static*/ int
DOM_CSSConditionalRule::insertRule(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(conditionalrule, DOM_TYPE_CSS_CONDITIONALRULE, DOM_CSSConditionalRule);
	DOM_CHECK_ARGUMENTS("sn");

	if (argv[1].value.number < 0)
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	unsigned int index = TruncateDoubleToUInt(argv[1].value.number);

	CSS_DOMException ex;
	OP_STATUS stat = conditionalrule->m_css_rule->InsertRule(argv[0].value.string, index, ex);
	if (OpStatus::IsMemoryError(stat))
		return ES_NO_MEMORY;
	else if (OpStatus::IsError(stat))
		if (ex == CSS_DOMEXCEPTION_SYNTAX_ERR)
			return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);
		else if (ex == CSS_DOMEXCEPTION_HIERARCHY_REQUEST_ERR)
			return DOM_CALL_DOMEXCEPTION(HIERARCHY_REQUEST_ERR);
		else if (ex == CSS_DOMEXCEPTION_INDEX_SIZE_ERR)
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	DOMSetNumber(return_value, argv[1].value.number);
	return ES_VALUE;
}

/*static*/ int
DOM_CSSConditionalRule::deleteRule(DOM_Object *this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(conditionalrule, DOM_TYPE_CSS_CONDITIONALRULE, DOM_CSSConditionalRule);
	DOM_CHECK_ARGUMENTS("n");

	if (argv[0].value.number < 0)
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	unsigned int index = TruncateDoubleToUInt(argv[0].value.number);

	CSS_DOMException ex;
	OP_STATUS stat = conditionalrule->m_css_rule->DeleteRule(index, ex);
	if (OpStatus::IsError(stat))
		if (ex == CSS_DOMEXCEPTION_INDEX_SIZE_ERR)
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	return ES_FAILED;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_CSSConditionalRule)
	DOM_FUNCTIONS_FUNCTION(DOM_CSSConditionalRule, DOM_CSSConditionalRule::insertRule, "insertRule", "sn")
	DOM_FUNCTIONS_FUNCTION(DOM_CSSConditionalRule, DOM_CSSConditionalRule::deleteRule, "deleteRule", "n")
DOM_FUNCTIONS_END(DOM_CSSConditionalRule)

