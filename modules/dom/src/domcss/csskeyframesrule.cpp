/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domcss/csskeyframesrule.h"
#include "modules/dom/src/domcss/cssstylesheet.h"
#include "modules/dom/src/domcss/cssrulelist.h"
#include "modules/dom/src/domstylesheets/medialist.h"

#ifdef CSS_ANIMATIONS

#include "modules/style/css_dom.h"

DOM_CSSKeyframesRule::DOM_CSSKeyframesRule(DOM_CSSStyleSheet* sheet, CSS_DOMRule* css_rule)
	: DOM_CSSRule(sheet, css_rule)
{
}

/* static */ OP_STATUS
DOM_CSSKeyframesRule::Make(DOM_CSSKeyframesRule*& rule, DOM_CSSStyleSheet* sheet, CSS_DOMRule* css_rule)
{
	DOM_Runtime *runtime = sheet->GetRuntime();
	return DOMSetObjectRuntime(rule = OP_NEW(DOM_CSSKeyframesRule, (sheet, css_rule)), runtime, runtime->GetPrototype(DOM_Runtime::CSSKEYFRAMESRULE_PROTOTYPE), "CSSKeyframesRule");
}

/* virtual */ ES_GetState
DOM_CSSKeyframesRule::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_name)
	{
		DOMSetString(value, m_css_rule->GetKeyframesName());
		return GET_SUCCESS;
	}
	else if (property_name == OP_ATOM_cssRules)
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
DOM_CSSKeyframesRule::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_cssRules:
		return PUT_READ_ONLY;

	case OP_ATOM_name:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;

		PUT_FAILED_IF_ERROR(m_css_rule->SetKeyframesName(value->value.string));
		return PUT_SUCCESS;

	default:
		return DOM_CSSRule::PutName(property_name, value, origining_runtime);
	}
}

/*static*/ int
DOM_CSSKeyframesRule::appendRule(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(keyframesrule, DOM_TYPE_CSS_KEYFRAMESRULE, DOM_CSSKeyframesRule);
	DOM_CHECK_ARGUMENTS("z");

	OP_STATUS stat = keyframesrule->m_css_rule->AppendKeyframeRule(argv[0].value.string_with_length->string, argv[0].value.string_with_length->length);

	if (OpStatus::IsMemoryError(stat))
		return ES_NO_MEMORY;
	else
		return ES_FAILED;
}

/*static*/ int
DOM_CSSKeyframesRule::deleteRule(DOM_Object *this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(keyframesrule, DOM_TYPE_CSS_KEYFRAMESRULE, DOM_CSSKeyframesRule);
	DOM_CHECK_ARGUMENTS("z");

	OP_STATUS stat = keyframesrule->m_css_rule->DeleteKeyframeRule(argv[0].value.string_with_length->string, argv[0].value.string_with_length->length);
	CALL_FAILED_IF_ERROR(stat);

	return ES_FAILED;
}

/*static*/ int
DOM_CSSKeyframesRule::findRule(DOM_Object *this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(keyframesrule, DOM_TYPE_CSS_KEYFRAMESRULE, DOM_CSSKeyframesRule);

	DOM_CHECK_ARGUMENTS("z");

	CSS_DOMRule* keyframe_rule;
	CALL_FAILED_IF_ERROR(keyframesrule->m_css_rule->FindKeyframeRule(keyframe_rule,
																	 argv[0].value.string_with_length->string,
																	 argv[0].value.string_with_length->length));

	if (keyframe_rule)
	{
		DOM_CSSRule* ret_rule = keyframe_rule->GetDOMRule();
		if (!ret_rule)
		{
			CALL_FAILED_IF_ERROR(DOM_CSSRule::Make(ret_rule, keyframesrule->m_sheet, keyframe_rule));
			keyframe_rule->SetDOMRule(ret_rule);
		}

		DOMSetObject(return_value, ret_rule);
	}
	else
		DOMSetNull(return_value);

	return ES_VALUE;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_CSSKeyframesRule)
	DOM_FUNCTIONS_FUNCTION(DOM_CSSKeyframesRule, DOM_CSSKeyframesRule::appendRule, "appendRule", "z")
	DOM_FUNCTIONS_FUNCTION(DOM_CSSKeyframesRule, DOM_CSSKeyframesRule::deleteRule, "deleteRule", "z")
	DOM_FUNCTIONS_FUNCTION(DOM_CSSKeyframesRule, DOM_CSSKeyframesRule::findRule, "findRule", "z")
DOM_FUNCTIONS_END(DOM_CSSKeyframesRule)

#endif // CSS_ANIMATIONS
