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
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domcss/cssrule.h"
#include "modules/dom/src/domcss/cssrulelist.h"
#include "modules/dom/src/domcss/csskeyframesrule.h"
#include "modules/dom/src/domcss/cssstylesheet.h"
#include "modules/dom/src/domstylesheets/medialist.h"
#include "modules/dom/src/domcss/cssmediarule.h"
#include "modules/dom/src/domcss/csssupportsrule.h"
#include "modules/dom/src/domcss/cssstyledeclaration.h"
#include "modules/style/css_dom.h"

static const char* RuleClassFromRuleType(CSS_DOMRule::Type type)
{
	switch (type)
	{
	case CSS_DOMRule::UNKNOWN:
		return "CSSUnknownRule";
	case CSS_DOMRule::STYLE:
		return "CSSStyleRule";
	case CSS_DOMRule::CHARSET:
		return "CSSCharsetRule";
	case CSS_DOMRule::IMPORT:
		return "CSSImportRule";
	case CSS_DOMRule::SUPPORTS:
		return "CSSSupportsRule";
	case CSS_DOMRule::MEDIA:
		return "CSSMediaRule";
	case CSS_DOMRule::FONT_FACE:
		return "CSSFontFaceRule";
	case CSS_DOMRule::PAGE:
		return "CSSPageRule";
	case CSS_DOMRule::KEYFRAMES:
		return "CSSKeyframesRule";
	case CSS_DOMRule::KEYFRAME:
		return "CSSKeyframeRule";
	case CSS_DOMRule::NAMESPACE:
		return "CSSNamespaceRule";
	case CSS_DOMRule::VIEWPORT:
		return "CSSViewportRule";
	default:
		OP_ASSERT(FALSE);
		return NULL;
	}
}

OP_STATUS
DOM_CSSRule::Make(DOM_CSSRule*& rule, DOM_CSSStyleSheet* sheet, CSS_DOMRule* css_rule)
{
	DOM_Runtime* runtime = sheet->GetRuntime();
	return DOMSetObjectRuntime(rule = OP_NEW(DOM_CSSRule, (sheet, css_rule)), runtime, runtime->GetPrototype(DOM_Runtime::CSSRULE_PROTOTYPE), RuleClassFromRuleType(css_rule->GetType()));
}

/* static */ OP_STATUS
DOM_CSSRule::GetRule(DOM_CSSRule*& rule, CSS_DOMRule* css_rule, DOM_Runtime* runtime)
{
	DOM_CSSRule* dom_rule = css_rule->GetDOMRule();

	if (!dom_rule)
	{
		DOM_Object* listable;
		DOM_CSSRuleList* rulelist;
		DOM_CSSStyleSheet* dom_sheet;
		CSS_DOMRule* css_conditional_rule;
		DOM_Object* node_object;

		DOM_Environment* env = runtime->GetEnvironment();

		HTML_Element* sheet_elm = css_rule->GetStyleSheetElm();

		RETURN_IF_ERROR(env->ConstructNode(node_object, sheet_elm));
		DOM_Node* owner_node = (DOM_Node*)node_object;

		DOM_CSSRule *import_rule = NULL;
		CSS_DOMRule *css_import_rule = NULL;
		if (sheet_elm->IsCssImport())
		{
			RETURN_IF_ERROR(CSS_DOMStyleSheet::GetImportRule(css_import_rule, sheet_elm, env));
			RETURN_IF_ERROR(DOM_CSSRule::GetRule(import_rule, css_import_rule, runtime));
		}

		/* Create the stylesheet object */
		ES_Value es_val;
		ES_GetState stat = owner_node->GetStyleSheet(&es_val, import_rule, runtime);
		if (stat == GET_NO_MEMORY)
			return OpStatus::ERR_NO_MEMORY;

		OP_ASSERT(stat == GET_SUCCESS && es_val.type == VALUE_OBJECT);
		dom_sheet = DOM_VALUE2OBJECT(es_val, DOM_CSSStyleSheet);

		/* Check if we're inside a conditional rule. If so, create it. */
		RETURN_IF_ERROR(css_rule->GetParentRule(css_conditional_rule));
		if (css_conditional_rule)
		{
			OP_ASSERT(css_conditional_rule->GetType() == CSS_DOMRule::MEDIA ||
					  css_conditional_rule->GetType() == CSS_DOMRule::SUPPORTS);

			DOM_CSSRule* conditional_rule;
			RETURN_IF_ERROR(DOM_CSSRuleList::GetFromListable(rulelist, dom_sheet));
			RETURN_IF_ERROR(rulelist->GetRule(css_conditional_rule, dom_sheet, conditional_rule));
			listable = conditional_rule;
		}
		else
			listable = dom_sheet;

		/* Create the rule list. */
		RETURN_IF_ERROR(DOM_CSSRuleList::GetFromListable(rulelist, listable));

		/* Finally, create the dom rule. */
		RETURN_IF_ERROR(rulelist->GetRule(css_rule, dom_sheet, dom_rule));
	}

	rule = dom_rule;

	return OpStatus::OK;
}

DOM_CSSRule::DOM_CSSRule(DOM_CSSStyleSheet* sheet, CSS_DOMRule* css_rule) : m_css_rule(css_rule), m_sheet(sheet)
{
	OP_ASSERT(css_rule);
	css_rule->Ref();
}

DOM_CSSRule::~DOM_CSSRule()
{
	m_css_rule->SetDOMRule(NULL);
	m_css_rule->Unref();
}

/* virtual */ void
DOM_CSSRule::GCTrace()
{
	GCMark(m_sheet);
}

/* virtual */ ES_GetState
DOM_CSSRule::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_type:
		{
			if (value)
				DOMSetNumber(value, m_css_rule->GetType());
			return GET_SUCCESS;
		}
		break;
	case OP_ATOM_cssText:
		{
			if (value)
			{
				TempBuffer* buf = GetEmptyTempBuf();
				GET_FAILED_IF_ERROR(m_css_rule->GetText(buf));
				DOMSetString(value, buf);
			}
			return GET_SUCCESS;
		}
		break;
	case OP_ATOM_parentStyleSheet:
		{
			if (value)
				DOMSetObject(value, m_sheet);

			return GET_SUCCESS;
		}
		break;
	case OP_ATOM_parentRule:
		{
			if (value)
			{
				CSS_DOMRule* css_parent;
				GET_FAILED_IF_ERROR(m_css_rule->GetParentRule(css_parent));
				if (css_parent)
				{
#ifdef CSS_ANIMATIONS
					OP_ASSERT(css_parent->GetType() == CSS_DOMRule::MEDIA
					       || css_parent->GetType() == CSS_DOMRule::SUPPORTS
					       || css_parent->GetType() == CSS_DOMRule::KEYFRAMES);
#else
					OP_ASSERT(css_parent->GetType() == CSS_DOMRule::MEDIA
					       || css_parent->GetType() == CSS_DOMRule::SUPPORTS);
#endif // CSS_ANIMATIONS

					DOM_Object* parent = NULL;

					if (css_parent->GetType() == CSS_DOMRule::MEDIA)
						parent = static_cast<DOM_CSSMediaRule*>(css_parent->GetDOMRule());
					else if (css_parent->GetType() == CSS_DOMRule::SUPPORTS)
						parent = static_cast<DOM_CSSSupportsRule*>(css_parent->GetDOMRule());
#ifdef CSS_ANIMATIONS
					else if (css_parent->GetType() == CSS_DOMRule::KEYFRAMES)
						parent = static_cast<DOM_CSSKeyframesRule*>(css_parent->GetDOMRule());
#endif // CSS_ANIMATIONS

					OP_ASSERT(parent);
					DOMSetObject(value, parent);
				}
				else
					DOMSetNull(value);
			}
			return GET_SUCCESS;
		}
		break;
	case OP_ATOM_selectorText:
		{
			if (m_css_rule->GetType() == CSS_DOMRule::STYLE || m_css_rule->GetType() == CSS_DOMRule::PAGE)
			{
				if (value)
				{
					TempBuffer* buf = GetEmptyTempBuf();
					GET_FAILED_IF_ERROR(m_css_rule->GetSelectorText(buf));
					DOMSetString(value, buf);
				}
				return GET_SUCCESS;
			}
		}
		break;
    case OP_ATOM_style:
		{
			if (m_css_rule->GetType() == CSS_DOMRule::STYLE ||
				m_css_rule->GetType() == CSS_DOMRule::PAGE ||
				m_css_rule->GetType() == CSS_DOMRule::FONT_FACE ||
				m_css_rule->GetType() == CSS_DOMRule::VIEWPORT ||
				m_css_rule->GetType() == CSS_DOMRule::KEYFRAME)
			{
				if (value)
				{
					ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_style);
					if (result != GET_FAILED)
						return result;

					DOM_CSSStyleDeclaration *style;

					GET_FAILED_IF_ERROR(DOM_CSSStyleDeclaration::Make(style, this));
					GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_style, *style));

					DOMSetObject(value, style);
				}
				return GET_SUCCESS;
			}
		}
		break;
	case OP_ATOM_href:
		{
			if (m_css_rule->GetType() == CSS_DOMRule::IMPORT)
			{
				if (value)
					DOMSetString(value, m_css_rule->GetHRef());
				return GET_SUCCESS;
			}
		}
		break;
	case OP_ATOM_media:
		{
			if (m_css_rule->GetType() == CSS_DOMRule::IMPORT)
			{
				if (value)
				{
					ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_media);
					if (result != GET_FAILED)
						return result;

					DOM_MediaList *medialist;

					GET_FAILED_IF_ERROR(DOM_MediaList::Make(medialist, this));
					GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_media, *medialist));

					DOMSetObject(value, medialist);
				}
				return GET_SUCCESS;
			}
		}
		break;
	case OP_ATOM_styleSheet:
		{
			if (m_css_rule->GetType() == CSS_DOMRule::IMPORT)
			{
				if (value)
				{
					HTML_Element* import_elm = m_css_rule->GetImportLinkElm();
					if (import_elm)
					{
						DOM_Object* owner_node;
						GET_FAILED_IF_ERROR(GetEnvironment()->ConstructNode(owner_node, import_elm));
						OP_ASSERT(owner_node->IsNode());
						return ((DOM_Node*)owner_node)->GetStyleSheet(value, this, (DOM_Runtime *) origining_runtime);
					}
					else
						DOMSetNull(value);
				}
				return GET_SUCCESS;
			}
		}
		break;
	case OP_ATOM_encoding:
		{
			if (m_css_rule->GetType() == CSS_DOMRule::CHARSET)
			{
				if (value)
					DOMSetString(value, m_css_rule->GetEncoding());
				return GET_SUCCESS;
			}
		}
		break;
	case OP_ATOM_namespaceURI:
		{
			if (m_css_rule->GetType() == CSS_DOMRule::NAMESPACE)
			{
				if (value)
					DOMSetString(value, m_css_rule->GetNamespaceURI());
				return GET_SUCCESS;
			}
		}
		break;
	case OP_ATOM_prefix:
		{
			if (m_css_rule->GetType() == CSS_DOMRule::NAMESPACE)
			{
				if (value)
					DOMSetString(value, m_css_rule->GetNamespacePrefix());
				return GET_SUCCESS;
			}
		}
		break;
#ifdef CSS_ANIMATIONS
	case OP_ATOM_keyText:
		{
			if (m_css_rule->GetType() == CSS_DOMRule::KEYFRAME)
			{
				if (value)
				{
					TempBuffer* buf = GetEmptyTempBuf();
					GET_FAILED_IF_ERROR(m_css_rule->GetKeyframeKeyText(buf));
					DOMSetString(value, buf);
				}
				return GET_SUCCESS;
			}
		}
		break;
#endif
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_CSSRule::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_cssText:
		{
			if (value->type != VALUE_STRING)
				return PUT_NEEDS_STRING;

			CSS_DOMException ex;
			OP_STATUS stat = m_css_rule->SetText(value->value.string, ex);
			if (stat == OpStatus::ERR_NO_MEMORY)
				return PUT_NO_MEMORY;
			else if (stat == OpStatus::ERR)
			{
				if (ex == CSS_DOMEXCEPTION_SYNTAX_ERR)
					return DOM_PUTNAME_DOMEXCEPTION(SYNTAX_ERR);
				else if (ex == CSS_DOMEXCEPTION_HIERARCHY_REQUEST_ERR)
					return DOM_PUTNAME_DOMEXCEPTION(HIERARCHY_REQUEST_ERR);
			}
			else
				return PUT_SUCCESS;
		}
		break;
	case OP_ATOM_selectorText:
		{
			if (m_css_rule->GetType() == CSS_DOMRule::STYLE || m_css_rule->GetType() == CSS_DOMRule::PAGE)
			{
				if (value->type != VALUE_STRING)
					return PUT_NEEDS_STRING;

				CSS_DOMException ex;
				OP_STATUS stat = m_css_rule->SetSelectorText(value->value.string, ex);
				if (stat == OpStatus::ERR_NO_MEMORY)
					return PUT_NO_MEMORY;
				else if (stat == OpStatus::ERR)
				{
					if (ex == CSS_DOMEXCEPTION_SYNTAX_ERR)
						return DOM_PUTNAME_DOMEXCEPTION(SYNTAX_ERR);
				}
				else
					return PUT_SUCCESS;
			}
		}
		break;
	case OP_ATOM_parentStyleSheet:
	case OP_ATOM_parentRule:
	case OP_ATOM_type:
		return PUT_READ_ONLY;
	case OP_ATOM_style:
		if (m_css_rule->GetType() == CSS_DOMRule::STYLE ||
			m_css_rule->GetType() == CSS_DOMRule::PAGE ||
			m_css_rule->GetType() == CSS_DOMRule::FONT_FACE ||
			m_css_rule->GetType() == CSS_DOMRule::VIEWPORT ||
			m_css_rule->GetType() == CSS_DOMRule::KEYFRAME)
			return PUT_READ_ONLY;
		break;
	case OP_ATOM_media:
		if (m_css_rule->GetType() == CSS_DOMRule::MEDIA ||
			m_css_rule->GetType() == CSS_DOMRule::IMPORT)
			return PUT_READ_ONLY;
		break;
	case OP_ATOM_cssRules:
		if (m_css_rule->GetType() == CSS_DOMRule::MEDIA ||
		    m_css_rule->GetType() == CSS_DOMRule::SUPPORTS)
			return PUT_READ_ONLY;
		break;
	case OP_ATOM_href:
	case OP_ATOM_styleSheet:
		if (m_css_rule->GetType() == CSS_DOMRule::IMPORT)
			return PUT_READ_ONLY;
		break;
	case OP_ATOM_encoding:
		if (m_css_rule->GetType() == CSS_DOMRule::CHARSET)
			return PUT_READ_ONLY;
		break;
	case OP_ATOM_prefix:
	case OP_ATOM_namespaceURI:
		if (m_css_rule->GetType() == CSS_DOMRule::NAMESPACE)
			return PUT_READ_ONLY;
		break;
#ifdef CSS_ANIMATIONS
	case OP_ATOM_keyText:
		if (m_css_rule->GetType() == CSS_DOMRule::KEYFRAME)
		{
			if (value->type != VALUE_STRING)
				return PUT_NEEDS_STRING;

			OP_STATUS stat = m_css_rule->SetKeyframeKeyText(value->value.string, value->GetStringLength());
			if (stat == OpStatus::ERR_NO_MEMORY)
				return PUT_NO_MEMORY;
			else
				return PUT_SUCCESS;
		}
		break;
#endif // CSS_ANIMATIONS
	}

	return PUT_FAILED;
}

/* static */ void
DOM_CSSRule::ConstructCSSRuleObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	DOM_Object::PutNumericConstantL(object, "UNKNOWN_RULE", CSS_DOMRule::UNKNOWN, runtime);
	DOM_Object::PutNumericConstantL(object, "STYLE_RULE", CSS_DOMRule::STYLE, runtime);
	DOM_Object::PutNumericConstantL(object, "CHARSET_RULE", CSS_DOMRule::CHARSET, runtime);
	DOM_Object::PutNumericConstantL(object, "IMPORT_RULE", CSS_DOMRule::IMPORT, runtime);
	DOM_Object::PutNumericConstantL(object, "MEDIA_RULE", CSS_DOMRule::MEDIA, runtime);
	DOM_Object::PutNumericConstantL(object, "SUPPORTS_RULE", CSS_DOMRule::SUPPORTS, runtime);
	DOM_Object::PutNumericConstantL(object, "FONT_FACE_RULE", CSS_DOMRule::FONT_FACE, runtime);
	DOM_Object::PutNumericConstantL(object, "PAGE_RULE", CSS_DOMRule::PAGE, runtime);
#ifdef CSS_ANIMATIONS
	DOM_Object::PutNumericConstantL(object, "KEYFRAMES_RULE", CSS_DOMRule::KEYFRAMES, runtime);
	DOM_Object::PutNumericConstantL(object, "KEYFRAME_RULE", CSS_DOMRule::KEYFRAME, runtime);
#endif // CSS_ANIMATIONS
	DOM_Object::PutNumericConstantL(object, "NAMESPACE_RULE", CSS_DOMRule::NAMESPACE, runtime);
	DOM_Object::PutNumericConstantL(object, "VIEWPORT_RULE", CSS_DOMRule::VIEWPORT, runtime);
}
