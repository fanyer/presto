/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domcss/cssstyledeclaration.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcss/cssrule.h"
#include "modules/dom/src/domcss/csstransformvalue.h"
#include "modules/dom/src/domcore/element.h"
#include "modules/dom/src/domcore/node.h"

#include "modules/layout/layout_workplace.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/style/css.h"
#include "modules/style/css_dom.h"
#include "modules/style/css_style_attribute.h"
#include "modules/doc/frm_doc.h"
#include "modules/stdlib/include/double_format.h"

DOM_CSSStyleDeclaration::DOM_CSSStyleDeclaration(DOM_Element *element, DOMStyleType type)
	: element(element),
	  style(NULL),
	  type(type)
{
}

#ifdef DOM2_MUTATION_EVENTS

DOM_CSSStyleDeclaration::MutationState::MutationState(DOM_Element *element, DOM_Runtime *origining_runtime)
	: element(element),
	  origining_runtime(origining_runtime),
	  send_attrmodified(FALSE)
{
	if (element)
		send_attrmodified = element->HasAttrModifiedHandlers();
}

OP_STATUS
DOM_CSSStyleDeclaration::MutationState::BeforeChange()
{
	if (send_attrmodified)
	{
		StyleAttribute* attr = element->GetThisElement()->GetStyleAttribute();
		prevBuffer.Clear();
		if (attr)
			RETURN_IF_ERROR(attr->ToString(&prevBuffer));
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_CSSStyleDeclaration::MutationState::AfterChange()
{
	if (send_attrmodified)
	{
		TempBuffer newBuffer;
		StyleAttribute* styleattr = element->GetThisElement()->GetStyleAttribute();
		if (styleattr)
			OpStatus::Ignore(styleattr->ToString(&newBuffer));

		const uni_char *prevValue = prevBuffer.GetStorage(), *newValue = newBuffer.GetStorage();
		if (!(prevValue == newValue || prevValue && newValue && uni_strcmp(prevValue, newValue) == 0))
		{
			DOM_Attr* attr;
			PUT_FAILED_IF_ERROR(element->GetAttributeNode(attr, ATTR_XML, UNI_L("style"), NS_IDX_DEFAULT, TRUE, TRUE, TRUE));
			if (attr)
				PUT_FAILED_IF_ERROR(element->SendAttrModified(DOM_Object::GetCurrentThread(origining_runtime), attr, prevValue, newValue));
		}
	}

	return OpStatus::OK;
}

#endif // DOM2_MUTATION_EVENTS

DOM_CSSStyleDeclaration::~DOM_CSSStyleDeclaration()
{
	OP_DELETE(style);
}

/* static */ OP_STATUS
DOM_CSSStyleDeclaration::Make(DOM_CSSStyleDeclaration *&styledeclaration, DOM_Element *element, DOMStyleType type, const uni_char *pseudo_class)
{
	DOM_EnvironmentImpl *environment = element->GetEnvironment();
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	HTML_Element *htmlelement = element->GetThisElement();

	RETURN_IF_ERROR(DOMSetObjectRuntime(styledeclaration = OP_NEW(DOM_CSSStyleDeclaration, (element, type)), runtime, runtime->GetPrototype(DOM_Runtime::CSSSTYLEDECLARATION_PROTOTYPE), "CSSStyleDeclaration"));

	CSS_DOMStyleDeclaration *style;
	OP_STATUS status;

	if (type == DOM_ST_COMPUTED)
		status = htmlelement->DOMGetComputedStyle(style, environment, pseudo_class);
#ifdef CURRENT_STYLE_SUPPORT
	else if (type == DOM_ST_CURRENT)
		status = htmlelement->DOMGetCurrentStyle(style, environment);
#endif // CURRENT_STYLE_SUPPORT
	else
	{
		OP_ASSERT(type == DOM_ST_INLINE);
		status = htmlelement->DOMGetInlineStyle(style, environment);
	}

	RETURN_IF_ERROR(status);

	styledeclaration->style = style;
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_CSSStyleDeclaration::Make(DOM_CSSStyleDeclaration *&styledeclaration, DOM_CSSRule* rule)
{
	DOM_EnvironmentImpl* environment = rule->GetEnvironment();
	DOM_Runtime* runtime = environment->GetDOMRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(styledeclaration = OP_NEW(DOM_CSSStyleDeclaration, (NULL, DOM_ST_RULE)), runtime, runtime->GetPrototype(DOM_Runtime::CSSSTYLEDECLARATION_PROTOTYPE), "CSSStyleDeclaration"));
	CSS_DOMStyleDeclaration *style;
	RETURN_IF_ERROR(rule->GetCSS_DOMRule()->GetStyle(style));
	styledeclaration->style = style;
	return OpStatus::OK;
}

/* virtual */ void
DOM_CSSStyleDeclaration::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	DOM_Object::DOMChangeRuntime(new_runtime);
	style->ChangeEnvironment(new_runtime->GetEnvironment());
}

/* virtual */ ES_GetState
DOM_CSSStyleDeclaration::GetNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	int css_property = DOM_AtomToCssProperty(property_name);

	switch (property_name)
	{
    case OP_ATOM_posLeft:
    case OP_ATOM_posRight:
    case OP_ATOM_posTop:
    case OP_ATOM_posBottom:
    case OP_ATOM_posWidth:
    case OP_ATOM_posHeight:
		OP_ASSERT(css_property != -1);
		if (value)
		{
			double result;
			GET_FAILED_IF_ERROR(style->GetPosValue(result, css_property));
			DOMSetNumber(value, result);
		}
		return GET_SUCCESS;

    case OP_ATOM_pixelHeight:
    case OP_ATOM_pixelLeft:
    case OP_ATOM_pixelTop:
    case OP_ATOM_pixelWidth:
    case OP_ATOM_pixelBottom:
    case OP_ATOM_pixelRight:
		OP_ASSERT(css_property != -1);
		if (value)
		{
			double result;
			GET_FAILED_IF_ERROR(style->GetPixelValue(result, css_property));
			DOMSetNumber(value, result);
		}
		return GET_SUCCESS;

	case OP_ATOM_length:
		DOMSetNumber(value, style->GetLength());
		return GET_SUCCESS;

	case OP_ATOM_cssText:
		if (value)
			if (type == DOM_ST_INLINE)
			{
				TempBuffer* buffer = GetEmptyTempBuf();
				StyleAttribute* attr = element->GetThisElement()->GetStyleAttribute();
				if (attr)
					GET_FAILED_IF_ERROR(attr->ToString(buffer));
				DOMSetString(value, buffer);
			}
			else if (type == DOM_ST_RULE)
			{
				TempBuffer* buffer = GetEmptyTempBuf();
				GET_FAILED_IF_ERROR(style->GetText(buffer));
				DOMSetString(value, buffer);
			}
			else
				DOMSetString(value);
		return GET_SUCCESS;

	default:
		if (css_property != -1)
		{
			if (value)
			{
				TempBuffer *buffer = GetEmptyTempBuf();
				GET_FAILED_IF_ERROR(style->GetPropertyValue(buffer, css_property));
				DOMSetString(value, buffer);
			}
			return GET_SUCCESS;
		}
		else
			return GET_FAILED;
	}
}

/* virtual */ ES_GetState
DOM_CSSStyleDeclaration::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (DOM_Node::ShouldBlockWaitingForStyle(GetFramesDocument(), origining_runtime))
	{
		DOM_DelayedLayoutListener *listener = OP_NEW(DOM_DelayedLayoutListener, (GetFramesDocument(), GetCurrentThread(origining_runtime)));
		if (!listener)
			return GET_NO_MEMORY;

		DOMSetNull(value);
		return GET_SUSPEND;
	}

	return GetNameRestart(property_name, value, origining_runtime, NULL);
}

/* virtual */ ES_PutState
DOM_CSSStyleDeclaration::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
#ifdef NS4P_COMPONENT_PLUGINS
	if (FramesDocument* doc = style->GetEnvironment()->GetFramesDocument())
		if (LayoutWorkplace* wp = doc->GetLayoutWorkplace())
			if (wp->IsTraversing() || wp->IsReflowing())
				return PUT_FAILED;
#endif // NS4P_COMPONENT_PLUGINS

	int css_property = DOM_AtomToCssProperty(property_name);
	BOOL is_pos = FALSE, is_pixel = FALSE;
	CSS_DOMException exception = CSS_DOMEXCEPTION_NONE;

	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	switch (property_name)
	{
    case OP_ATOM_posLeft:
    case OP_ATOM_posRight:
    case OP_ATOM_posTop:
    case OP_ATOM_posBottom:
    case OP_ATOM_posWidth:
    case OP_ATOM_posHeight:
		is_pos = TRUE;
		break;

    case OP_ATOM_pixelHeight:
    case OP_ATOM_pixelLeft:
    case OP_ATOM_pixelTop:
    case OP_ATOM_pixelWidth:
    case OP_ATOM_pixelBottom:
    case OP_ATOM_pixelRight:
		is_pixel = TRUE;
		break;

	case OP_ATOM_length:
		return PUT_READ_ONLY;

	case OP_ATOM_cssText:
		if (value->type == VALUE_NULL)
			DOMSetString(value);
		else if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;

		if (type == DOM_ST_INLINE)
			PUT_FAILED_IF_ERROR(element->SetAttribute(ATTR_XML, UNI_L("style"), NS_IDX_DEFAULT, value->value.string, value->GetStringLength(), TRUE, origining_runtime));
		else if (type == DOM_ST_RULE)
		{
			OP_STATUS stat = style->SetText(value->value.string, exception);
			if (stat == OpStatus::ERR_NO_MEMORY)
				return PUT_NO_MEMORY;
			else if (stat == OpStatus::ERR)
			{
				if (exception == CSS_DOMEXCEPTION_NO_MODIFICATION_ALLOWED_ERR)
					return PUT_READ_ONLY;
				else if (exception == CSS_DOMEXCEPTION_SYNTAX_ERR)
					return DOM_PUTNAME_DOMEXCEPTION(SYNTAX_ERR);
			}
		}
		return PUT_SUCCESS;

	default:
		if (css_property != -1)
		{
			if (value->type == VALUE_NULL)
				DOMSetString(value);
			else if (value->type != VALUE_STRING)
				return PUT_NEEDS_STRING;

#ifdef DOM2_MUTATION_EVENTS
			MutationState mutationstate(element, (DOM_Runtime *) origining_runtime);
			if (element)
			{
				PUT_FAILED_IF_ERROR(mutationstate.BeforeChange());
			}
#endif // DOM2_MUTATION_EVENTS

			status = style->SetProperty(css_property, value->value.string, exception);

#ifdef DOM2_MUTATION_EVENTS
			if (OpStatus::IsSuccess(status) && element)
				PUT_FAILED_IF_ERROR(mutationstate.AfterChange());
#endif // DOM2_MUTATION_EVENTS

			goto handle_status;
		}
		else
			return PUT_FAILED;
	}

	if (is_pos || is_pixel)
	{
		OP_ASSERT(css_property != -1);

		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;

		if (is_pixel)
			value->value.number = op_floor(value->value.number);

		TempBuffer *buffer = GetEmptyTempBuf();
		PUT_FAILED_IF_ERROR(buffer->Expand(33));

		char *number8 = reinterpret_cast<char *>(buffer->GetStorage());
		char *result = OpDoubleFormat::ToString(number8, value->value.number);
		if (!result)
			return PUT_NO_MEMORY;
		make_doublebyte_in_place(buffer->GetStorage(), op_strlen(number8));

		if (is_pos)
			status = style->SetPosValue(css_property, buffer->GetStorage(), exception);
		else
			status = style->SetPixelValue(css_property, buffer->GetStorage(), exception);
	}

handle_status:
	if (OpStatus::IsMemoryError(status))
		return PUT_NO_MEMORY;
	else if (OpStatus::IsError(status))
		if (exception == CSS_DOMEXCEPTION_NO_MODIFICATION_ALLOWED_ERR)
			return PUT_READ_ONLY;
		else if (exception == CSS_DOMEXCEPTION_SYNTAX_ERR)
			return DOM_PUTNAME_DOMEXCEPTION(SYNTAX_ERR);

	return PUT_SUCCESS;
}

/* virtual */ void
DOM_CSSStyleDeclaration::GCTrace()
{
	if (element)
		GCMark(element);
}

/* static */ int
DOM_CSSStyleDeclaration::getPropertyValue(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(styledeclaration, DOM_TYPE_CSS_STYLEDECLARATION, DOM_CSSStyleDeclaration);
	DOM_CHECK_ARGUMENTS("s");

	TempBuffer *buffer = styledeclaration->GetEmptyTempBuf();
	CALL_FAILED_IF_ERROR(styledeclaration->style->GetPropertyValue(buffer, argv[0].value.string));

	DOMSetString(return_value, buffer);
	return ES_VALUE;
}

/* static */ int
DOM_CSSStyleDeclaration::getPropertyCSSValue(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OBJECT);
	return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
}

/* static */ int
DOM_CSSStyleDeclaration::getPropertyPriority(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(styledeclaration, DOM_TYPE_CSS_STYLEDECLARATION, DOM_CSSStyleDeclaration);
	DOM_CHECK_ARGUMENTS("s");

	TempBuffer *buffer = styledeclaration->GetEmptyTempBuf();
	CALL_FAILED_IF_ERROR(styledeclaration->style->GetPropertyPriority(buffer, argv[0].value.string));

	DOMSetString(return_value, buffer);
	return ES_VALUE;
}

/* static */ int
DOM_CSSStyleDeclaration::setProperty(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(styledeclaration, DOM_TYPE_CSS_STYLEDECLARATION, DOM_CSSStyleDeclaration);
	DOM_CHECK_ARGUMENTS("ss|s");

#ifdef DOM2_MUTATION_EVENTS
	MutationState mutationstate(styledeclaration->element, (DOM_Runtime *) origining_runtime);
	if (styledeclaration->element)
		CALL_FAILED_IF_ERROR(mutationstate.BeforeChange());
#endif // DOM2_MUTATION_EVENTS

	CSS_DOMException exception;
	OP_STATUS status = styledeclaration->style->SetProperty(argv[0].value.string, argv[1].value.string, argc == 3 ? argv[2].value.string : NULL, exception);

#ifdef DOM2_MUTATION_EVENTS
	if (OpStatus::IsSuccess(status) && styledeclaration->element)
		CALL_FAILED_IF_ERROR(mutationstate.AfterChange());
#endif // DOM2_MUTATION_EVENTS

	if (OpStatus::IsMemoryError(status))
		return ES_NO_MEMORY;
	else if (OpStatus::IsError(status))
		if (exception == CSS_DOMEXCEPTION_SYNTAX_ERR)
			return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);
		else if (exception == CSS_DOMEXCEPTION_NO_MODIFICATION_ALLOWED_ERR)
			return PUT_READ_ONLY;

	return ES_FAILED;
}

/* static */ int
DOM_CSSStyleDeclaration::removeProperty(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(styledeclaration, DOM_TYPE_CSS_STYLEDECLARATION, DOM_CSSStyleDeclaration);
	DOM_CHECK_ARGUMENTS("s");

#ifdef DOM2_MUTATION_EVENTS
	MutationState mutationstate(styledeclaration->element, (DOM_Runtime *) origining_runtime);
	if (styledeclaration->element)
		CALL_FAILED_IF_ERROR(mutationstate.BeforeChange());
#endif // DOM2_MUTATION_EVENTS

	TempBuffer *buffer = styledeclaration->GetEmptyTempBuf();

	CSS_DOMException exception;
	OP_STATUS status = styledeclaration->style->RemoveProperty(buffer, argv[0].value.string, exception);

#ifdef DOM2_MUTATION_EVENTS
	if (OpStatus::IsSuccess(status) && styledeclaration->element)
		CALL_FAILED_IF_ERROR(mutationstate.AfterChange());
#endif // DOM2_MUTATION_EVENTS

	if (OpStatus::IsMemoryError(status))
		return ES_NO_MEMORY;
	else if (OpStatus::IsError(status))
		if (exception == CSS_DOMEXCEPTION_NO_MODIFICATION_ALLOWED_ERR)
			return DOM_CALL_DOMEXCEPTION(NO_MODIFICATION_ALLOWED_ERR);

	DOMSetString(return_value, buffer);
	return ES_VALUE;
}

/* virtual */ ES_GetState
DOM_CSSStyleDeclaration::GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime)
{
	TempBuffer* buffer = GetEmptyTempBuf();
	GET_FAILED_IF_ERROR(style->GetItem(buffer, property_index));
	DOMSetString(value, buffer);
	return GET_SUCCESS;
}

/* virtual */ ES_GetState
DOM_CSSStyleDeclaration::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = style->GetLength();
	return GET_SUCCESS;
}

/* static */ int
DOM_CSSStyleDeclaration::item(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(styledeclaration, DOM_TYPE_CSS_STYLEDECLARATION, DOM_CSSStyleDeclaration);
	DOM_CHECK_ARGUMENTS("n");

	int index = (int)argv[0].value.number;

	if (argv[0].value.number == (double) index)
		RETURN_GETNAME_AS_CALL(styledeclaration->GetIndex(index, return_value, origining_runtime));
	else
	{
		DOMSetString(return_value, GetEmptyTempBuf());
		return ES_VALUE;
	}
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_CSSStyleDeclaration)
	DOM_FUNCTIONS_FUNCTION(DOM_CSSStyleDeclaration, DOM_CSSStyleDeclaration::getPropertyValue, "getPropertyValue", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_CSSStyleDeclaration, DOM_CSSStyleDeclaration::getPropertyCSSValue, "getPropertyCSSValue", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_CSSStyleDeclaration, DOM_CSSStyleDeclaration::getPropertyPriority, "getPropertyPriority", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_CSSStyleDeclaration, DOM_CSSStyleDeclaration::setProperty, "setProperty", "sss-")
	DOM_FUNCTIONS_FUNCTION(DOM_CSSStyleDeclaration, DOM_CSSStyleDeclaration::removeProperty, "removeProperty", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_CSSStyleDeclaration, DOM_CSSStyleDeclaration::item, "item", "n-")
DOM_FUNCTIONS_END(DOM_CSSStyleDeclaration)

