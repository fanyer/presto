/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcss/cssstylesheet.h"
#include "modules/dom/src/domcss/cssrulelist.h"
#include "modules/dom/src/domcss/cssmediarule.h"
#include "modules/dom/src/domcss/csssupportsrule.h"
#include "modules/dom/src/domcss/csskeyframesrule.h"
#include "modules/dom/src/domcore/element.h"

DOM_CSSRuleList::DOM_CSSRuleList(DOM_Object* listable) : m_list_object(listable)
{
}

/* static */ OP_STATUS
DOM_CSSRuleList::Make(DOM_CSSRuleList*& rulelist, DOM_Object* listable)
{
	DOM_Runtime *runtime = listable->GetRuntime();
	return DOMSetObjectRuntime(rulelist = OP_NEW(DOM_CSSRuleList, (listable)), runtime, runtime->GetPrototype(DOM_Runtime::CSSRULELIST_PROTOTYPE), "CSSRuleList");
}

/* static */ OP_STATUS
DOM_CSSRuleList::GetFromListable(DOM_CSSRuleList*& rulelist, DOM_Object* listable)
{
	ES_Value val;
	OP_BOOLEAN ret = listable->GetPrivate(DOM_PRIVATE_cssRules, &val);
	if (ret == OpBoolean::IS_FALSE)
	{
		RETURN_IF_ERROR(DOM_CSSRuleList::Make(rulelist, listable));
		return listable->PutPrivate(DOM_PRIVATE_cssRules, *rulelist);
	}
	else if (ret == OpBoolean::IS_TRUE)
	{
		rulelist = DOM_VALUE2OBJECT(val, DOM_CSSRuleList);
		return OpStatus::OK;
	}
	else
		return ret;
}

/* virtual */ ES_GetState
DOM_CSSRuleList::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		double length = 0;

		if (m_list_object->IsA(DOM_TYPE_CSS_CONDITIONALRULE))
			length = static_cast<DOM_CSSConditionalRule*>(m_list_object)->m_css_rule->GetRuleCount();
#ifdef CSS_ANIMATIONS
		else if (m_list_object->IsA(DOM_TYPE_CSS_KEYFRAMESRULE))
			length = static_cast<DOM_CSSKeyframesRule*>(m_list_object)->m_css_rule->GetRuleCount();
#endif // CSS_ANIMATIONS
		else
			length = static_cast<DOM_CSSStyleSheet*>(m_list_object)->m_css_sheet->GetRuleCount();

		DOMSetNumber(value, length);
		return GET_SUCCESS;
	}
	else
		return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_CSSRuleList::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
		return PUT_READ_ONLY;
	else
		return PUT_FAILED;
}

/* virtual */ void
DOM_CSSRuleList::GCTrace()
{
	GCMark(m_list_object);
}

/* virtual */ ES_GetState
DOM_CSSRuleList::GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime)
{
	CSS_DOMException ex;
	OP_STATUS stat = OpStatus::OK;
	CSS_DOMRule* css_rule;
	DOM_CSSStyleSheet* sheet;

	DOMSetNull(value);

	if (m_list_object->IsA(DOM_TYPE_CSS_CONDITIONALRULE))
	{
		DOM_CSSConditionalRule* conditional_rule = static_cast<DOM_CSSConditionalRule*>(m_list_object);
		sheet = conditional_rule->m_sheet;
		stat = conditional_rule->m_css_rule->GetRule(css_rule, property_index, ex);
	}
#ifdef CSS_ANIMATIONS
	else if (m_list_object->IsA(DOM_TYPE_CSS_KEYFRAMESRULE))
	{
		DOM_CSSKeyframesRule* keyframes_rule = static_cast<DOM_CSSKeyframesRule*>(m_list_object);
		sheet = keyframes_rule->m_sheet;
		stat = keyframes_rule->m_css_rule->GetRule(css_rule, property_index, ex);
	}
#endif // CSS_ANIMATIONS
	else
	{
		OP_ASSERT(m_list_object->IsA(DOM_TYPE_CSS_STYLESHEET));
		sheet = (DOM_CSSStyleSheet*)m_list_object;
		stat = sheet->m_css_sheet->GetRule(css_rule, property_index, ex);
	}

	if (stat == OpStatus::OK)
	{
		DOM_CSSRule* rule;
		GET_FAILED_IF_ERROR(GetRule(css_rule, sheet, rule));
		DOMSetObject(value, rule);
	}
	else if (stat == OpStatus::ERR_NO_MEMORY)
		return GET_NO_MEMORY;

	return GET_SUCCESS;
}

/* virtual */ ES_GetState
DOM_CSSRuleList::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	if (m_list_object->IsA(DOM_TYPE_CSS_CONDITIONALRULE))
		count = static_cast<DOM_CSSConditionalRule*>(m_list_object)->m_css_rule->GetRuleCount();
#ifdef CSS_ANIMATIONS
	else if (m_list_object->IsA(DOM_TYPE_CSS_KEYFRAMESRULE))
		count = static_cast<DOM_CSSKeyframesRule*>(m_list_object)->m_css_rule->GetRuleCount();
#endif // CSS_ANIMATIONS
	else
		count = static_cast<DOM_CSSStyleSheet*>(m_list_object)->m_css_sheet->GetRuleCount();

	return GET_SUCCESS;
}

OP_STATUS
DOM_CSSRuleList::GetRule(CSS_DOMRule* css_rule, DOM_CSSStyleSheet* sheet, DOM_CSSRule*& dom_rule)
{
	DOM_CSSRule* ret_rule = css_rule->GetDOMRule();
	if (!ret_rule)
	{
		if (css_rule->GetType() == CSS_DOMRule::MEDIA)
		{
			DOM_CSSMediaRule* media_rule;
			RETURN_IF_ERROR(DOM_CSSMediaRule::Make(media_rule, sheet, css_rule));
			ret_rule = media_rule;
		}
		else if (css_rule->GetType() == CSS_DOMRule::SUPPORTS)
		{
			DOM_CSSSupportsRule* supports_rule;
			RETURN_IF_ERROR(DOM_CSSSupportsRule::Make(supports_rule, sheet, css_rule));
			ret_rule = supports_rule;
		}
#ifdef CSS_ANIMATIONS
		else if (css_rule->GetType() == CSS_DOMRule::KEYFRAMES)
		{
			DOM_CSSKeyframesRule* keyframes_rule;
			RETURN_IF_ERROR(DOM_CSSKeyframesRule::Make(keyframes_rule, sheet, css_rule));
			ret_rule = keyframes_rule;
		}
#endif // CSS_ANIMATIONS
		else
		{
			RETURN_IF_ERROR(DOM_CSSRule::Make(ret_rule, sheet, css_rule));
		}

		ES_Value next_rule;

		if (GetPrivate(DOM_PRIVATE_nextRule, &next_rule) == OpBoolean::IS_TRUE)
		{
			RETURN_IF_ERROR(ret_rule->PutPrivate(DOM_PRIVATE_nextRule, next_rule.value.object));
		}
		RETURN_IF_ERROR(PutPrivate(DOM_PRIVATE_nextRule, *ret_rule));
		css_rule->SetDOMRule(ret_rule);
	}

	dom_rule = ret_rule;
	return OpStatus::OK;
}

/* static */ int
DOM_CSSRuleList::item(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(rulelist, DOM_TYPE_CSS_RULELIST, DOM_CSSRuleList);
	DOM_CHECK_ARGUMENTS("n");

	if (!rulelist->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	DOMSetNull(return_value);

	int index = (int)argv[0].value.number;

	if (argv[0].value.number == (double) index)
		RETURN_GETNAME_AS_CALL(rulelist->GetIndex(index, return_value, origining_runtime));
	else
		return ES_VALUE;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_CSSRuleList)
	DOM_FUNCTIONS_FUNCTION(DOM_CSSRuleList, DOM_CSSRuleList::item, "item", "n")
DOM_FUNCTIONS_END(DOM_CSSRuleList)

