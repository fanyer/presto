/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domhtml/htmlelem.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/element.h"
#include "modules/dom/src/domcore/text.h"
#include "modules/dom/src/domcss/cssstylesheet.h"
#include "modules/dom/src/domevents/domeventthread.h"
#include "modules/dom/src/domhtml/htmlcoll.h"
#include "modules/dom/src/domhtml/htmldoc.h"
#include "modules/dom/src/js/location.h"
#include "modules/dom/src/opatom.h"
#include "modules/dom/src/userjs/userjsmanager.h"
#include "modules/dom/src/userjs/userjsevent.h"
#include "modules/dom/src/js/window.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domfile/domfilelist.h"
#include "modules/dom/src/webforms2/webforms2dom.h"
#include "modules/dom/src/webforms2/validitystate.h"
#include "modules/dom/domutils.h"

#include "modules/doc/frm_doc.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/util/tempbuf.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/forms/datetime.h"
#include "modules/forms/form.h"
#include "modules/forms/formmanager.h"
#include "modules/forms/formvaluelist.h"
#include "modules/forms/webforms2number.h"
#include "modules/forms/webforms2support.h"
#include "modules/formats/uri_escape.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/url/url_api.h"
#include "modules/url/url_man.h"

#ifdef _PLUGIN_SUPPORT_
#include "modules/ns4plugins/opns4plugin.h"
#include "modules/ns4plugins/opns4pluginhandler.h"
#endif // _PLUGIN_SUPPORT_

#ifdef JS_PLUGIN_SUPPORT
# include "modules/jsplugins/src/js_plugin_object.h"
#endif // JS_PLUGIN_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
# include "modules/media/mediaelement.h"
# include "modules/media/mediatrack.h"
# include "modules/dom/src/media/dommediaerror.h"
# include "modules/dom/src/media/domtimeranges.h"
# include "modules/dom/src/media/domtexttrack.h"
#endif // MEDIA_HTML_SUPPORT

#ifdef CANVAS_SUPPORT
# include "modules/libvega/src/canvas/canvas.h"
# include "modules/dom/src/canvas/domcontext2d.h"
# include "modules/dom/src/canvas/domcontextwebgl.h"
#endif // CANVAS_SUPPORT

#ifdef DOM_STREAM_API_SUPPORT
# include "modules/dom/src/media/domstream.h"
#endif // DOM_STREAM_API_SUPPORT

#ifdef CANVAS3D_SUPPORT
#include "modules/ecmascript_utils/esasyncif.h"
#endif // CANVAS3D_SUPPORT

OP_STATUS
DOM_initCollection(DOM_Collection *&collection, DOM_Object *owner, DOM_Node *root, DOM_HTMLElement_Group group, int private_name)
{
	if (!collection)
	{
		DOM_SimpleCollectionFilter filter(group);

		DOM_Collection* new_collection;
		RETURN_IF_ERROR(DOM_Collection::Make(new_collection, owner->GetEnvironment(), "HTMLCollection", root, owner->IsA(DOM_TYPE_DOCUMENT), FALSE, filter));

		if (group != IMAGES)
		{
			new_collection->SetCreateSubcollections();
			if (group == ALL)
			{
				ES_Runtime::MarkObjectAsHidden(*new_collection);
				RETURN_IF_LEAVE(new_collection->AddFunctionL(DOM_HTMLCollection::tags, "tags", "s-"));
			}
		}

		RETURN_IF_ERROR(owner->PutPrivate(private_name, *new_collection));
		collection = new_collection;
	}

	return OpStatus::OK;
}

ES_GetState
DOM_HTMLElement::GetStringAttribute(OpAtom property_name, ES_Value* value)
{
	if (value)
	{
		DOM_EnvironmentImpl *environment = GetEnvironment();

		DOM_EnvironmentImpl::CurrentState state(environment, NULL);
		state.SetTempBuffer();

		DOMSetString(value, this_element->DOMGetAttribute(environment, DOM_AtomToHtmlAttribute(property_name), NULL, NS_IDX_DEFAULT, FALSE, TRUE));
	}

	return GET_SUCCESS;
}

ES_GetState
DOM_HTMLElement::GetNumericAttribute(OpAtom property_name, const unsigned *type, ES_Value* value)
{
	if (value)
	{
		BOOL absent;
		double number = this_element->DOMGetNumericAttribute(GetEnvironment(), DOM_AtomToHtmlAttribute(property_name), NULL, NS_IDX_DEFAULT, absent);

		if (absent || HTML_NUMERIC_PROPERTY_TYPE(type) != TYPE_DOUBLE && (!op_isfinite(number) || number > NUMERIC_ATTRIBUTE_MAX))
			number = HTML_NUMERIC_PROPERTY_HAS_DEFAULT(type) ? HTML_NUMERIC_PROPERTY_DEFAULT(type) : 0;
		else
		{
			switch (HTML_NUMERIC_PROPERTY_TYPE(type))
			{
			case TYPE_LONG_NON_NEGATIVE:
			case TYPE_ULONG:
				if (number < 0)
					number = HTML_NUMERIC_PROPERTY_HAS_DEFAULT(type) ? HTML_NUMERIC_PROPERTY_DEFAULT(type) : 0;
				break;
			case TYPE_ULONG_NON_ZERO:
				if (number <= 0)
					number = HTML_NUMERIC_PROPERTY_HAS_DEFAULT(type) ? HTML_NUMERIC_PROPERTY_DEFAULT(type) : 0;
				break;
			default:
				break;
			}
		}
		DOMSetNumber(value, number);
	}

	return GET_SUCCESS;
}

ES_GetState
DOM_HTMLElement::GetBooleanAttribute(OpAtom property_name, ES_Value *value)
{
	if (value)
		DOMSetBoolean(value, this_element->DOMGetBooleanAttribute(GetEnvironment(), DOM_AtomToHtmlAttribute(property_name), NULL, NS_IDX_DEFAULT));

	return GET_SUCCESS;
}

ES_PutState
DOM_HTMLElement::SetBooleanAttribute(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (!OriginCheck(origining_runtime))
		return PUT_SECURITY_VIOLATION;

	if (value->type != VALUE_BOOLEAN)
		return PUT_NEEDS_BOOLEAN;

	DOM_EnvironmentImpl *environment = GetEnvironment();
	Markup::AttrType attr = DOM_AtomToHtmlAttribute(property_name);

#ifdef DOM2_MUTATION_EVENTS
	BOOL send_attrmodified = HasAttrModifiedHandlers();

	const uni_char *name = HTM_Lex::GetAttrString(attr);
	const uni_char *prevValue = NULL, *newValue;

	if (send_attrmodified)
		prevValue = this_element->DOMGetBooleanAttribute(environment, attr, NULL, NS_IDX_DEFAULT) ? name : NULL;
#endif // DOM2_MUTATION_EVENTS

	DOM_EnvironmentImpl::CurrentState state(environment, (DOM_Runtime *) origining_runtime);

	OP_STATUS status = this_element->DOMSetBooleanAttribute(GetEnvironment(), attr, NULL, NS_IDX_DEFAULT, value->value.boolean);

	PUT_FAILED_IF_ERROR(status);

#ifdef DOM2_MUTATION_EVENTS
	if (send_attrmodified)
	{
		newValue = this_element->DOMGetBooleanAttribute(environment, attr, NULL, NS_IDX_DEFAULT) ? name : NULL;

		if (prevValue != newValue)
			PUT_FAILED_IF_ERROR(SendAttrModified(GetCurrentThread(origining_runtime), property_name, prevValue, newValue));
	}
#endif /* DOM2_MUTATION_EVENTS */

	return PUT_SUCCESS;
}

ES_PutState
DOM_HTMLElement::SetNumericAttribute(OpAtom property_name, const unsigned *type, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (!OriginCheck(origining_runtime))
		return PUT_SECURITY_VIOLATION;

	if (value->type != VALUE_NUMBER)
		return PUT_NEEDS_NUMBER;

	OP_ASSERT(type);
	double number;
	if (HTML_NUMERIC_PROPERTY_TYPE(type) == TYPE_LONG || HTML_NUMERIC_PROPERTY_TYPE(type) == TYPE_LONG_NON_NEGATIVE)
		number = TruncateDoubleToInt(value->value.number);
	else if (HTML_NUMERIC_PROPERTY_TYPE(type) == TYPE_ULONG || HTML_NUMERIC_PROPERTY_TYPE(type) == TYPE_ULONG_NON_ZERO)
		number = TruncateDoubleToUInt(value->value.number);
	else
	{
		OP_ASSERT(HTML_NUMERIC_PROPERTY_TYPE(type) == TYPE_DOUBLE);
		number = value->value.number;
	}

	if (HTML_NUMERIC_PROPERTY_TYPE(type) == TYPE_LONG_NON_NEGATIVE && number < 0 ||
	    HTML_NUMERIC_PROPERTY_TYPE(type) == TYPE_ULONG_NON_ZERO && number == 0)
		return DOM_PUTNAME_DOMEXCEPTION(INDEX_SIZE_ERR);

	DOM_EnvironmentImpl *environment = GetEnvironment();
	Markup::AttrType attr = DOM_AtomToHtmlAttribute(property_name);

#ifdef DOM2_MUTATION_EVENTS
	BOOL send_attrmodified = HasAttrModifiedHandlers();
	uni_char prevValue[DBL_MAX_10_EXP + 50], newValue[DBL_MAX_10_EXP + 50];

	if (send_attrmodified)
	{
		ES_Value prev_value;
#ifdef DEBUG_ENABLE_OPASSERT
		ES_GetState result =
#endif // DEBUG_ENABLE_OPASSERT
		GetNumericAttribute(property_name, type, &prev_value);
		OP_ASSERT(result == GET_SUCCESS && prev_value.type == VALUE_NUMBER);

		double prev_number = prev_value.value.number;
		if (this_element->IsNumericAttributeFloat(attr))
			PUT_FAILED_IF_ERROR(WebForms2Number::DoubleToString(prev_number, prevValue));
		else
			uni_sprintf(prevValue, UNI_L("%d"), static_cast<int>(prev_number));
	}
#endif // DOM2_MUTATION_EVENTS

	DOM_EnvironmentImpl::CurrentState state(environment, (DOM_Runtime *) origining_runtime);

	OP_STATUS status = this_element->DOMSetNumericAttribute(environment, attr, NULL, NS_IDX_DEFAULT, number);

	PUT_FAILED_IF_ERROR(status);

#ifdef DOM2_MUTATION_EVENTS
	if (send_attrmodified)
	{
		BOOL absent;
		double v = this_element->DOMGetNumericAttribute(environment, attr, NULL, NS_IDX_DEFAULT, absent);
		if (this_element->IsNumericAttributeFloat(attr))
			PUT_FAILED_IF_ERROR(WebForms2Number::DoubleToString(v, newValue));
		else
			uni_sprintf(newValue, UNI_L("%d"), static_cast<int>(v));

		PUT_FAILED_IF_ERROR(SendAttrModified(GetCurrentThread(origining_runtime), property_name, prevValue, newValue));
	}
#endif /* DOM2_MUTATION_EVENTS */

	return PUT_SUCCESS;
}

ES_PutState
DOM_HTMLElement::SetStringAttribute(OpAtom property_name, BOOL treat_null_as_empty_string, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (!OriginCheck(origining_runtime))
		return PUT_SECURITY_VIOLATION;

	if (treat_null_as_empty_string && value->type == VALUE_NULL)
		DOMSetString(value);
	else if (value->type != VALUE_STRING)
		return PUT_NEEDS_STRING;

	PUT_FAILED_IF_ERROR(SetAttribute(DOM_AtomToHtmlAttribute(property_name), NULL, NS_IDX_DEFAULT, value->value.string, value->GetStringLength(), HasCaseSensitiveNames(), origining_runtime));
	return PUT_SUCCESS;
}

ES_GetState
DOM_HTMLFormsElement::GetFormValue(ES_Value *value, BOOL with_crlf)
{
	if (value)
	{
		TempBuffer *buffer = GetEmptyTempBuf();

		if (OpStatus::IsSuccess(this_element->DOMGetFormValue(GetEnvironment(), buffer, with_crlf)))
			DOMSetString(value, buffer);
		else
			return GET_NO_MEMORY;
	}

	return GET_SUCCESS;
}

ES_PutState
DOM_HTMLFormsElement::SetFormValue(const uni_char *value)
{
	if (OpStatus::IsSuccess(this_element->DOMSetFormValue(GetEnvironment(), value)))
		return PUT_SUCCESS;
	else
		return PUT_NO_MEMORY;
}

HTML_Element *
DOM_HTMLFormsElement::GetFormElement()
{
	HTML_Element* form_control;
	if (this_element->IsMatchingType(HE_OPTION, NS_HTML))
	{
		HTML_Element *select = static_cast<DOM_HTMLOptionElement *>(this)->GetSelectElement();
		if (!select)
			return NULL;
		form_control = select;
	}
	else
		form_control = this_element;

	// This should be the same as form_control.forms[0] in DOM
	return FormManager::FindFormElm(GetFramesDocument(), form_control);
}

#ifdef DOM_SELECTION_SUPPORT
/** Returns TRUE if the element does have text length and selection start & end.
 */
static BOOL
IsFormElementWithContent(HTML_Element *this_element)
{
	// Exclude HE_SELECT and HE_KEYGEN from the 'form element' set.
	return this_element->IsFormElement() && !this_element->IsMatchingType(HE_SELECT, NS_HTML) && !this_element->IsMatchingType(HE_KEYGEN, NS_HTML);
}

/* static */ BOOL
DOM_HTMLFormsElement::InputElementHasSelectionAttributes(HTML_Element *element)
{
	OP_ASSERT(element->IsMatchingType(HE_INPUT, NS_HTML));

	switch (element->GetInputType())
	{
	case INPUT_TEXT:
	case INPUT_SEARCH:
	case INPUT_TEL:
	case INPUT_URI:
	case INPUT_EMAIL:
	case INPUT_PASSWORD:
	case INPUT_NUMBER:
		return TRUE;
	}

	return FALSE;
}

#endif // DOM_SELECTION_SUPPORT

/* virtual */ ES_GetState
DOM_HTMLFormsElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_form:
		if (value)
			return DOMSetElement(value, GetFormElement());
		return GET_SUCCESS;

	case OP_ATOM_labels:
		if (value)
		{
			GET_FAILED_IF_ERROR(InitLabelsCollection(this, labels));
			DOMSetObject(value, labels);
		}
		return GET_SUCCESS;

	case OP_ATOM_validity:
		if (value)
		{
			GET_FAILED_IF_ERROR(InitValidityState(this, validity));
			DOMSetObject(value, validity);
		}
		return GET_SUCCESS;

	case OP_ATOM_validationMessage:
		if (value)
		{
			DOMSetString(value);
			TempBuffer *buf = GetEmptyTempBuf();
			OP_STATUS status = FormValidator::GetCustomValidationMessage(GetThisElement(), buf);
			if (OpStatus::IsSuccess(status))
				DOMSetString(value, buf);
			else if (OpStatus::IsMemoryError(status))
				return GET_NO_MEMORY;
		}
		return GET_SUCCESS;

	case OP_ATOM_willValidate:
		if (value)
		{
			FormValidator validator(GetFramesDocument());
			BOOL will_validate = validator.WillValidate(GetThisElement());
			DOMSetBoolean(value, will_validate);
		}
		return GET_SUCCESS;

#ifdef DOM_SELECTION_SUPPORT
	case OP_ATOM_selectionStart:
	case OP_ATOM_selectionEnd:
	case OP_ATOM_selectionDirection:
		if (!(this_element->IsMatchingType(HE_INPUT, NS_HTML) || this_element->IsMatchingType(HE_TEXTAREA, NS_HTML)))
			break;

		if (value)
		{
			if (this_element->IsMatchingType(HE_INPUT, NS_HTML) && !InputElementHasSelectionAttributes(this_element))
				return DOM_GETNAME_DOMEXCEPTION(INVALID_STATE_ERR);

			DOM_EnvironmentImpl *environment = GetEnvironment();
			int start, end;
			SELECTION_DIRECTION direction;

			this_element->DOMGetSelection(environment, start, end, direction);

			if (property_name == OP_ATOM_selectionDirection)
			{
				if (direction == SELECTION_DIRECTION_FORWARD)
					DOMSetString(value, UNI_L("forward"));
				else if (direction == SELECTION_DIRECTION_BACKWARD)
					DOMSetString(value, UNI_L("backward"));
				else
					DOMSetString(value, UNI_L("none"));
			}
			else
			{
				if (start == 0 && end == 0)
					start = end = this_element->DOMGetCaretPosition(environment);

				if (property_name == OP_ATOM_selectionEnd)
					start = end;

				DOMSetNumber(value, start);
			}
		}
		return GET_SUCCESS;

	case OP_ATOM_textLength:
		if (!IsFormElementWithContent(this_element))
			break;
		if (value)
		{
			TempBuffer buffer;

			GET_FAILED_IF_ERROR(this_element->DOMGetFormValue(GetEnvironment(), &buffer, TRUE));

			DOMSetNumber(value, buffer.Length());
		}
		return GET_SUCCESS;
#endif // DOM_SELECTION_SUPPORT
	}
	return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLFormsElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch(property_name)
	{
#ifdef DOM_SELECTION_SUPPORT
	case OP_ATOM_textLength:
		if (!IsFormElementWithContent(this_element))
			break;
		/* Fall through */
#endif // DOM_SELECTION_SUPPORT
	case OP_ATOM_form:
	case OP_ATOM_labels:
	case OP_ATOM_validity:
	case OP_ATOM_validationMessage:
	case OP_ATOM_willValidate:
		return PUT_READ_ONLY;

#ifdef DOM_SELECTION_SUPPORT
	case OP_ATOM_selectionStart:
	case OP_ATOM_selectionEnd:
	case OP_ATOM_selectionDirection:
	{
		if (this_element->IsMatchingType(HE_INPUT, NS_HTML))
		{
			if (!InputElementHasSelectionAttributes(this_element))
				return DOM_PUTNAME_DOMEXCEPTION(INVALID_STATE_ERR);
		}
		else if(!this_element->IsMatchingType(HE_TEXTAREA, NS_HTML))
			break;

		if (value)
		{
			DOM_EnvironmentImpl *environment = GetEnvironment();
			int start, end;
			SELECTION_DIRECTION direction;

			this_element->DOMGetSelection(environment, start, end, direction);

			if (start == 0 && end == 0)
				start = end = this_element->DOMGetCaretPosition(environment);

			if (property_name == OP_ATOM_selectionStart)
			{
				if (value->type != VALUE_NUMBER)
					return PUT_NEEDS_NUMBER;
				start = TruncateDoubleToInt(value->value.number);
				if (start > end)
					end = start;
			}
			else if (property_name == OP_ATOM_selectionEnd)
			{
				if (value->type != VALUE_NUMBER)
					return PUT_NEEDS_NUMBER;
				end = TruncateDoubleToInt(value->value.number);
				if (end < start)
					start = end;
			}
			else
			{
				if (value->type != VALUE_STRING)
					return PUT_NEEDS_STRING;
				if (uni_str_eq(value->value.string, "forward"))
					direction = SELECTION_DIRECTION_FORWARD;
				else if (uni_str_eq(value->value.string, "backward"))
					direction = SELECTION_DIRECTION_BACKWARD;
				else
					direction = SELECTION_DIRECTION_NONE;
			}

			PUT_FAILED_IF_ERROR(SetSelectionRange(origining_runtime, start, end, direction));
		}
		return PUT_SUCCESS;
	}
#endif // DOM_SELECTION_SUPPORT
	}

	return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_HTMLFormsElement::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	DOM_HTMLElement::DOMChangeRuntime(new_runtime);

	if (labels)
		labels->DOMChangeRuntime(new_runtime);
	if (validity)
		validity->DOMChangeRuntime(new_runtime);
}

/* static */
OP_STATUS
DOM_HTMLFormsElement::InitLabelsCollection(DOM_HTMLElement *element, DOM_Collection *&labels)
{
	if (!labels)
	{
		DOM_Collection *collection;
		LabelCollectionFilter filter(element->GetThisElement());

		RETURN_IF_ERROR(DOM_Collection::Make(collection, element->GetEnvironment(), "HTMLCollection",
							element->GetOwnerDocument()->GetRootElement(), TRUE, FALSE, filter));

		// Make sure the collection and |this| has the same life span
		RETURN_IF_ERROR(element->PutPrivate(DOM_PRIVATE_labels_collection, collection));

		labels = collection;
	}

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_HTMLFormsElement::InitValidityState(DOM_HTMLElement *element, DOM_ValidityState*& validity)
{
	if (!validity)
	{
		DOM_ValidityState* val_stat;
		RETURN_IF_ERROR(DOM_ValidityState::Make(val_stat, element, element->GetEnvironment()));

		// Make sure the collection and |this| has the same life span
		RETURN_IF_ERROR(element->PutPrivate(DOM_PRIVATE_validityState, val_stat));

		validity = val_stat;
	}
	OP_ASSERT(validity);
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_HTMLTitleElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_text)
		return GetTextContent(value);
	else
		return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLTitleElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_text)
		return PutNameRestart(property_name, value, origining_runtime, NULL);
	else
		return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLTitleElement::PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	if (property_name == OP_ATOM_text)
		return SetTextContent(value, (DOM_Runtime *) origining_runtime, restart_object);
	else
		return DOM_HTMLElement::PutNameRestart(property_name, value, origining_runtime, restart_object);
}

// DOM_HTMLBodyElement

/* virtual */ ES_GetState
DOM_HTMLBodyElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_aLink:
	case OP_ATOM_background:
	case OP_ATOM_bgColor:
	case OP_ATOM_link:
	case OP_ATOM_text:
	case OP_ATOM_vLink:
		return GetStringAttribute(property_name, value);

	default:
		return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_GetState
DOM_HTMLBodyElement::GetName(const uni_char *property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n' && !uni_str_eq(property_name, "onerror"))
	{
#ifdef DOM3_EVENTS
		DOM_EventType type = DOM_Event::GetEventType(NULL, property_name + 2, TRUE);
#else // DOM3_EVENTS
		DOM_EventType type = DOM_Event::GetEventType(property_name + 2, TRUE);
#endif // DOM3_EVENTS

		if (DOM_Event::IsWindowEvent(type))
		{
			/* Redirect this to the window object. */
			ES_GetState state = GetEnvironment()->GetWindow()->GetEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
			if (state != GET_FAILED)
				return state;
		}
	}

	return DOM_Element::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLBodyElement::PutName(const uni_char *property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n' && !uni_str_eq(property_name, "onerror"))
	{
#ifdef DOM3_EVENTS
		DOM_EventType type = DOM_Event::GetEventType(NULL, property_name + 2, TRUE);
#else // DOM3_EVENTS
		DOM_EventType type = DOM_Event::GetEventType(property_name + 2, TRUE);
#endif // DOM3_EVENTS

		if (DOM_Event::IsWindowEvent(type))
		{
			/* Redirect this to the window object. */
			ES_PutState state = GetEnvironment()->GetWindow()->PutEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
			if (state != PUT_FAILED)
				return state;
		}
	}

	return DOM_Element::PutName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_HTMLStylesheetElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_disabled)
	{
		DOMSetBoolean(value, GetThisElement()->DOMGetStylesheetDisabled(GetEnvironment()));
		return GET_SUCCESS;
	}

	return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLStylesheetElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_disabled)
	{
		if (value->type != VALUE_BOOLEAN)
			return PUT_NEEDS_BOOLEAN;
		else
		{
			PUT_FAILED_IF_ERROR(GetThisElement()->DOMSetStylesheetDisabled(GetEnvironment(), value->value.boolean));
			return PUT_SUCCESS;
		}
	}

	return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
}

#ifdef PARTIAL_XMLELEMENT_SUPPORT // See bug 286692
/* virtual */ ES_GetState
DOM_HTMLXmlElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_documentElement)
	{
		// This becomes an "xml document" if, and only if, there is a single root element
		if (value)
		{
			DOM_Node *node;
			DOM_Node *subroot = NULL;
			GET_FAILED_IF_ERROR(GetFirstChild(node));
			while (node)
			{
				if(ELEMENT_NODE == node->GetNodeType())
				{
					if (subroot)
					{
						// Two roots -> not an "xml document" -> no documentElement
						subroot = NULL;
						break;
					}

					subroot = node;
				}

				GET_FAILED_IF_ERROR(node->GetNextSibling(node));
			}

			DOMSetObject(value, subroot);
		}
		return GET_SUCCESS;
	}

	return DOM_HTMLElement::GetName(property_name, value, this_object, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLXmlElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_documentElement)
		return PUT_READ_ONLY;

	return DOM_HTMLElement::PutName(property_name, value, this_object, origining_runtime);
}
#endif // #ifdef PARTIAL_XMLELEMENT_SUPPORT // See bug 286692

DOM_HTMLFormElement::DOM_HTMLFormElement()
	: elements(NULL),
	  images(NULL)
{
}

OP_STATUS
DOM_HTMLFormElement::InitElementsCollection()
{
	if (!elements)
	{
		RETURN_IF_ERROR(DOM_initCollection(elements, this, this, FORMELEMENTS, DOM_PRIVATE_elements));
		elements->GetNodeCollection()->SetUseFormNr();
		elements->SetOwner(this);
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_HTMLFormElement::InitImagesCollection()
{
	if (!images)
	{
		RETURN_IF_ERROR(DOM_initCollection(images, this, this, IMAGES, DOM_PRIVATE_images));
		images->GetNodeCollection()->SetUseFormNr();
		images->SetOwner(this);
	}

	return OpStatus::OK;
}

/* static */ ES_GetState
DOM_HTMLFormElement::GetEnumeratedValue(OpAtom property_name, ES_GetState state, ES_Value* value)
{
	if (value && state == GET_SUCCESS)
		switch (property_name)
		{
		case OP_ATOM_enctype:
		case OP_ATOM_formEnctype:
			DOMSetString(value, Form::GetFormEncTypeEnumeration(value->value.string));
			break;

		case OP_ATOM_method:
		case OP_ATOM_formMethod:
			DOMSetString(value, Form::GetFormMethodEnumeration(value->value.string));
			break;

		default:
			OP_ASSERT(!"Unexpected enumeration attribute");
			break;
		}

	return state;
}

/* virtual */ ES_GetState
DOM_HTMLFormElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_elements:
	case OP_ATOM_length:
		if (value)
		{
			GET_FAILED_IF_ERROR(InitElementsCollection());

			if (property_name == OP_ATOM_elements)
				DOMSetObject(value, elements);
			else
				DOMSetNumber(value, elements->Length());
		}
		return GET_SUCCESS;

	case OP_ATOM_enctype:
	case OP_ATOM_method:
		return GetEnumeratedValue(property_name, GetStringAttribute(property_name, value), value);

	default:
		return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_GetState
DOM_HTMLFormElement::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	GET_FAILED_IF_ERROR(InitElementsCollection());
	ES_GetState result = elements->GetName(property_name, property_code, value, origining_runtime);

	if (result == GET_FAILED)
	{
		GET_FAILED_IF_ERROR(InitImagesCollection());
		result = images->GetName(property_name, property_code, value, origining_runtime);
	}

	/* If a collection found one or more elements, return what it found. */
	if (result == GET_SUCCESS && (!value || value->type == VALUE_OBJECT) || result != GET_FAILED)
		return result;

	if (GetInternalValue(property_name, value))
		return GET_SUCCESS;

	return DOM_Element::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_HTMLFormElement::GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime)
{
	GET_FAILED_IF_ERROR(InitElementsCollection());
	return elements->GetIndex(property_index, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLFormElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_elements:
	case OP_ATOM_length:
		return PUT_READ_ONLY;

	default:
		return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_GetState
DOM_HTMLFormElement::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	GET_FAILED_IF_ERROR(InitElementsCollection());
	return elements->GetIndexedPropertiesLength(count, origining_runtime);
}

/* virtual */ void
DOM_HTMLFormElement::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	DOM_HTMLElement::DOMChangeRuntime(new_runtime);

	if (elements)
		elements->DOMChangeRuntime(new_runtime);
	if (images)
		images->DOMChangeRuntime(new_runtime);
}


DOM_HTMLSelectElement::DOM_HTMLSelectElement()
	: options(NULL)
{
	selected_options = NULL;
}

/* virtual */ ES_GetState
DOM_HTMLSelectElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_length:
	case OP_ATOM_options:
		if (value)
		{
			GET_FAILED_IF_ERROR(InitOptionsCollection());

			if (property_name == OP_ATOM_options)
				DOMSetObject(value, options);
			else
				DOMSetNumber(value, options->Length());
		}
		return GET_SUCCESS;

	case OP_ATOM_selectedOptions:
		if (value)
		{
			GET_FAILED_IF_ERROR(InitSelectedOptionsCollection());
			DOMSetObject(value, selected_options);
		}
		return GET_SUCCESS;

	case OP_ATOM_type:
		if (this_element->DOMGetBooleanAttribute(GetEnvironment(), ATTR_MULTIPLE, NULL, NS_IDX_DEFAULT))
			DOMSetString(value, UNI_L("select-multiple"));
		else
			DOMSetString(value, UNI_L("select-one"));
		return GET_SUCCESS;

	case OP_ATOM_selectedIndex:
	case OP_ATOM_value:
		if (value)
		{
			int selected_index;
			GET_FAILED_IF_ERROR(this_element->DOMGetSelectedIndex(GetEnvironment(), selected_index));

			if (property_name == OP_ATOM_selectedIndex)
				DOMSetNumber(value, selected_index);
			else
			{
				const uni_char* option_value = NULL;

				if (selected_index >= 0)
				{
					GET_FAILED_IF_ERROR(InitOptionsCollection());

					if (HTML_Element *option = options->Item(selected_index))
						if (!(option_value = option->GetValue()))
						{
							TempBuffer *buffer = GetEmptyTempBuf();
							GET_FAILED_IF_ERROR(option->GetOptionText(buffer));
							option_value = buffer->GetStorage();
						}
				}

				DOMSetString(value, option_value);
			}
		}
		return GET_SUCCESS;

	case OP_ATOM_text:
		// remove the MSIE property |text| from select elements since they shouldn't have it
		return GET_FAILED;

	default:
		return DOM_HTMLFormsElement::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_GetState
DOM_HTMLSelectElement::GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime)
{
	GET_FAILED_IF_ERROR(InitOptionsCollection());
	return options->GetIndex(property_index, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLSelectElement::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_length:
		/* DOM2 says readonly, but truncating the list by setting a smaller length
		   is common practice. */
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;
		else
		{
			int new_length = (int) value->value.number;

			if (new_length < 0)
				new_length = 0;

			ES_Object *restart_object;
			int result = ChangeOptions(this, FALSE, FALSE, TRUE, NULL, NULL, new_length, value, (DOM_Runtime *) origining_runtime, restart_object);

			if (result == (ES_SUSPEND | ES_RESTART))
			{
				DOMSetObject(value, restart_object);
				return PUT_SUSPEND;
			}

			if (result == ES_NO_MEMORY)
				return PUT_NO_MEMORY;
		}
		return PUT_SUCCESS;

	case OP_ATOM_form:
	case OP_ATOM_options:
	case OP_ATOM_type:
	case OP_ATOM_selectedOptions:
	case OP_ATOM_labels:
		return PUT_READ_ONLY;

	case OP_ATOM_value:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else
		{
			int new_selectedIndex = -1;

			if (value->value.string)
			{
				PUT_FAILED_IF_ERROR(InitOptionsCollection());

				TempBuffer option_text_buf;
				for (int index = 0, length = options->Length(); index < length; ++index)
					if (HTML_Element *option = options->Item(index))
					{
						const uni_char *option_value = option->GetValue();
						if (!option_value)
						{
							PUT_FAILED_IF_ERROR(option->GetOptionText(&option_text_buf));
							option_value = option_text_buf.GetStorage();
						}
						if (option_value)
							if (uni_str_eq(option_value, value->value.string))
							{
								new_selectedIndex = index;
								break;
							}
					}
			}

			DOMSetNumber(value, new_selectedIndex);
		}
		// fallthrough
	case OP_ATOM_selectedIndex:
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;
		else
		{
			int select_index = TruncateDoubleToInt(value->value.number);
			PUT_FAILED_IF_ERROR(this_element->DOMSetSelectedIndex(GetEnvironment(), select_index));
		}
		return PUT_SUCCESS;

	case OP_ATOM_text:
		// remove the MSIE property |text| from select elements since they shouldn't have it
		return PUT_FAILED;

	default:
		return DOM_HTMLFormsElement::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_HTMLSelectElement::PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	if (property_name == OP_ATOM_length)
	{
		int result = ChangeOptionsRestart(value, restart_object);

		if (result == (ES_SUSPEND | ES_RESTART))
		{
			DOMSetObject(value, restart_object);
			return PUT_SUSPEND;
		}

		if (result == ES_NO_MEMORY)
			return PUT_NO_MEMORY;

		return PUT_SUCCESS;
	}
	else
		return DOM_HTMLElement::PutNameRestart(property_name, value, origining_runtime, restart_object);
}

/* virtual */ ES_PutState
DOM_HTMLSelectElement::PutIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime)
{
	DOM_Object *option;

	if (value->type == VALUE_OBJECT)
	{
		if (!(option = DOM_VALUE2OBJECT(*value, DOM_Object)))
			return PUT_SUCCESS;
	}
	else if (value->type == VALUE_NULL || value->type == VALUE_UNDEFINED)
		option = NULL;
	else
		return PUT_SUCCESS;

	ES_Object *restart_object;

	BOOL modify_length = !options || options->Length() <= property_index;

	int result = ChangeOptions(this, option != NULL, TRUE, modify_length, option, NULL, property_index, value, static_cast<DOM_Runtime *>(origining_runtime), restart_object);

	if (result == (ES_SUSPEND | ES_RESTART))
	{
		DOMSetObject(value, restart_object);
		return PUT_SUSPEND;
	}

	if (result == ES_NO_MEMORY)
		return PUT_NO_MEMORY;

	return PUT_SUCCESS;
}

/* virtual */ ES_PutState
DOM_HTMLSelectElement::PutIndexRestart(int property_index, ES_Value* value, ES_Runtime *origining_runtime, ES_Object* restart_object)
{
	int result = ChangeOptionsRestart(value, restart_object);

	if (result == (ES_SUSPEND | ES_RESTART))
	{
		DOMSetObject(value, restart_object);
		return PUT_SUSPEND;
	}

	if (result == ES_NO_MEMORY)
		return PUT_NO_MEMORY;

	return PUT_SUCCESS;
}

/* virtual */ ES_GetState
DOM_HTMLSelectElement::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	GET_FAILED_IF_ERROR(InitOptionsCollection());
	return options->GetIndexedPropertiesLength(count, origining_runtime);
}

/* virtual */ void
DOM_HTMLSelectElement::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	DOM_HTMLElement::DOMChangeRuntime(new_runtime);

	if (options)
		options->DOMChangeRuntime(new_runtime);
	if (selected_options)
		selected_options->DOMChangeRuntime(new_runtime);
}

/* virtual */ void
DOM_HTMLSelectElement::GCTraceSpecial(BOOL via_tree)
{
	DOM_Element::GCTraceSpecial(via_tree);

	GCMark(options);
	GCMark(selected_options);
}

OP_STATUS
DOM_HTMLSelectElement::InitOptionsCollection()
{
	if (!options)
		RETURN_IF_ERROR(DOM_HTMLOptionsCollection::Make(options, GetEnvironment(), this));

	return OpStatus::OK;
}

OP_STATUS
DOM_HTMLSelectElement::InitSelectedOptionsCollection()
{
	if (!selected_options)
	{
		DOM_SimpleCollectionFilter filter(SELECTED_OPTIONS);

		RETURN_IF_ERROR(DOM_Collection::Make(selected_options, GetEnvironment(), "HTMLCollection", this, FALSE, FALSE, filter));
	}

	return OpStatus::OK;
}

class DOM_ChangeOptionsState
	: public DOM_Object
{
public:
	enum Point
	{
		INITIAL,
		AFTER_REMOVE,
		AFTER_INSERT,
		AFTER_REPLACE
	} point;

	DOM_HTMLSelectElement *select;
	BOOL add, remove, modify_length;
	DOM_Object *option, *reference;
	int index;
	DOM_Runtime *origining_runtime;
	ES_Value return_value;

	DOM_ChangeOptionsState(Point point)
		: point(point)
	{
	}

	virtual void GCTrace()
	{
		GCMark(select);
		GCMark(option);
		GCMark(reference);
		GCMark(return_value);
	}
};

int
DOM_HTMLSelectElement::ChangeOptions(DOM_HTMLSelectElement *select, BOOL add, BOOL remove, BOOL modify_length, DOM_Object *option, DOM_Object *reference, int index, ES_Value *exception_value, DOM_Runtime *origining_runtime, ES_Object *&restart_object)
{
	DOM_ChangeOptionsState *state;
	DOM_ChangeOptionsState::Point point;
	ES_Value return_value;

	if (select)
	{
		state = NULL;
		point = DOM_ChangeOptionsState::INITIAL;

		if (option && !(option->IsA(DOM_TYPE_HTML_OPTIONELEMENT) || option->IsA(DOM_TYPE_HTML_OPTGROUPELEMENT)))
			return ES_FAILED;

		CALL_FAILED_IF_ERROR(select->InitOptionsCollection());
	}
	else
	{
		state = DOM_HOSTOBJECT(restart_object, DOM_ChangeOptionsState);
		point = state->point;

		select = state->select;
		add = state->add;
		remove = state->remove;
		modify_length = state->modify_length;
		option = state->option;
		reference = state->reference;
		index = state->index;
		origining_runtime = state->origining_runtime;

#ifdef DOM2_MUTATION_EVENTS
		if (point != DOM_ChangeOptionsState::INITIAL)
		{
			int result;

			if (point == DOM_ChangeOptionsState::AFTER_REMOVE)
				result = DOM_Node::removeChild(NULL, NULL, -1, &state->return_value, origining_runtime);
			else if (point == DOM_ChangeOptionsState::AFTER_INSERT)
				result = DOM_Node::insertBefore(NULL, NULL, -1, &state->return_value, origining_runtime);
			else // point == DOM_ChangeOptionsState::AFTER_REPLACE
				result = DOM_Node::replaceChild(NULL, NULL, -1, &state->return_value, origining_runtime);

			if (result != ES_VALUE)
			{
				if (result == ES_EXCEPTION)
					*exception_value = state->return_value;

				return result;
			}
		}

		if (!add && !remove && !modify_length)
			return ES_FAILED;
#endif // DOM2_MUTATION_EVENTS
	}

	int length = select->options->Length(), result;

	HTML_Element *reference_element;
	DOM_Node *reference_node = NULL, *parent_node = NULL;

	if (reference)
	{
		if (reference->IsA(DOM_TYPE_HTML_OPTIONELEMENT))
		{
			DOM_HTMLOptionElement *reference_option = static_cast<DOM_HTMLOptionElement *>(reference);

			// Check that the option is inside the select and that there is no other select element between this select and the option
			if (!select->IsAncestorOf(reference_option) || reference_option->GetSelectElement()->GetESElement() != select)
				return select->CallDOMException(NOT_FOUND_ERR, exception_value);

			index = reference_option->GetOptionIndex();
		}
		else if (!reference->IsA(DOM_TYPE_HTML_OPTGROUPELEMENT))
			return ES_FAILED;
	}
	else if (index == -1 || (add && !modify_length && index > length))
		index = length;

	// Don't want to hang the browser just because someone gets the idea to add a million options.
	const int MAX_OPTIONS_IN_SLICE = 100;
	if (modify_length)
	{
		int chunk_end = MIN(index, length + MAX_OPTIONS_IN_SLICE);
		while (length < chunk_end)
		{
			/* Insert empty option. */
			DOM_HTMLElement *empty;
			CALL_FAILED_IF_ERROR(DOM_HTMLElement::CreateElement(empty, select, UNI_L("option")));

			ES_Value arguments[2];
			DOMSetObject(&arguments[0], empty);
			DOMSetNull(&arguments[1]);

			int result = DOM_Node::insertBefore(select, arguments, 2, &return_value, origining_runtime);

#ifdef DOM2_MUTATION_EVENTS
			if (result == (ES_SUSPEND | ES_RESTART))
			{
				point = DOM_ChangeOptionsState::AFTER_INSERT;
				goto suspend;
			}
#endif // DOM2_MUTATION_EVENTS

			if (result != ES_VALUE)
			{
				if (result == ES_EXCEPTION)
					*exception_value = return_value;

				return result;
			}

			++length;
		}

		if (length < index)
		{
			// We ended early to prevent a hang. Schedule a return here later.
			point = DOM_ChangeOptionsState::INITIAL; // Run through the whole function again
			goto suspend;
		}

		// While it's possible that we want to chunk this, it can after all not be
		// heavier than clearing the tree which is an operation that will be
		// limited by the tree size.
		while (index < length)
		{
			HTML_Element *remove_element = select->options->Item(index);
			DOM_Node *remove_node, *parent_node;

			CALL_FAILED_IF_ERROR(select->GetEnvironment()->ConstructNode(remove_node, remove_element, select->GetOwnerDocument()));
			CALL_FAILED_IF_ERROR(remove_node->GetParentNode(parent_node));

			ES_Value arguments[1];
			DOMSetObject(&arguments[0], remove_node);

			int result = DOM_Node::removeChild(parent_node, arguments, 1, &return_value, origining_runtime);

			if (result == ES_EXCEPTION)
				*exception_value = return_value;
#ifdef DOM2_MUTATION_EVENTS
			else if (result == (ES_SUSPEND | ES_RESTART))
			{
				point = DOM_ChangeOptionsState::AFTER_REMOVE;
				goto suspend;
			}
#endif // DOM2_MUTATION_EVENTS

			if (result != ES_VALUE)
				return result;

			--length;
		}

		modify_length = FALSE;
	}

	if (index < length)
	{
		if (reference && reference->IsA(DOM_TYPE_HTML_OPTGROUPELEMENT))
			reference_element = static_cast<DOM_HTMLElement *>(reference)->GetThisElement();
		else
			reference_element = select->options->Item(index);

		if (reference_element)
		{
			CALL_FAILED_IF_ERROR(select->GetEnvironment()->ConstructNode(reference_node, reference_element, select->GetOwnerDocument()));
			CALL_FAILED_IF_ERROR(reference_node->GetParentNode(parent_node));
		}
	}

	if (!parent_node)
	{
		reference_node = NULL;
		parent_node = select;
	}

	if (add && remove && !reference_node && index >= length)
		remove = FALSE;

	if (add && remove)
	{
		add = remove = FALSE;

		if (reference_node)
		{
			ES_Value arguments[2];
			DOMSetObject(&arguments[0], option);
			DOMSetObject(&arguments[1], reference_node);

			/* Lock rebuild (synchronizing with the tree) of a <select> so it's rebuilt only once on unlocking
			 * i.e. it doesn't react on all tree changes triggered by replaceChild(). */
			select->this_element->DOMSelectUpdateLock(TRUE);
			result = DOM_Node::replaceChild(parent_node, arguments, 2, &return_value, origining_runtime);
			select->this_element->DOMSelectUpdateLock(FALSE);

			/* If we've just replaced the collection's element that was cached, try refreshing the cache
			 * with new element. */
			OP_ASSERT(select);
			if (result == ES_VALUE && select->options)
				select->options->AddToIndexCache(index, (static_cast<DOM_Element*>(option))->GetThisElement());

#ifdef DOM2_MUTATION_EVENTS
			point = DOM_ChangeOptionsState::AFTER_REPLACE;
#endif // DOM2_MUTATION_EVENTS
		}
		else
			// FIXME: not certain what this means, check it!
			result = ES_VALUE;
	}
	else if (add)
	{
		add = FALSE;

		if (!option)
		{
			DOM_HTMLElement *empty;
			CALL_FAILED_IF_ERROR(DOM_HTMLElement::CreateElement(empty, select, UNI_L("option")));
			option = empty;
		}

		ES_Value arguments[2];
		DOMSetObject(&arguments[0], option);
		DOMSetObject(&arguments[1], reference_node);

		result = DOM_Node::insertBefore(parent_node, arguments, 2, &return_value, origining_runtime);

#ifdef DOM2_MUTATION_EVENTS
		point = DOM_ChangeOptionsState::AFTER_INSERT;
#endif // DOM2_MUTATION_EVENTS
	}
	else if (remove)
	{
		remove = FALSE;

		if (reference_node)
		{
			ES_Value arguments[1];
			DOMSetObject(&arguments[0], reference_node);

			result = DOM_Node::removeChild(parent_node, arguments, 1, &return_value, origining_runtime);

#ifdef DOM2_MUTATION_EVENTS
			point = DOM_ChangeOptionsState::AFTER_REMOVE;
#endif // DOM2_MUTATION_EVENTS
		}
		else
			// FIXME: not certain what this means, check it!
			result = ES_VALUE;
	}
	else
		return ES_FAILED;

	if (result == ES_EXCEPTION)
		*exception_value = return_value;
#ifdef DOM2_MUTATION_EVENTS
	else if (result == (ES_SUSPEND | ES_RESTART))
		goto suspend;
#endif // DOM2_MUTATION_EVENTS

	if (result != ES_VALUE)
		return result;

	return ES_FAILED;

suspend:
	CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(state = OP_NEW(DOM_ChangeOptionsState, (point)), origining_runtime));

	state->select = select;
	state->add = add;
	state->remove = remove;
	state->modify_length = modify_length;
	state->option = option;
	state->reference = reference;
	state->index = index;
	state->origining_runtime = origining_runtime;
	state->return_value = return_value;

	restart_object = *state;
	return ES_SUSPEND | ES_RESTART;
}

int
DOM_HTMLSelectElement::ChangeOptionsRestart(ES_Value *exception_value, ES_Object *&restart_object)
{
	return ChangeOptions(NULL, FALSE, FALSE, FALSE, NULL, NULL, 0, exception_value, NULL, restart_object);
}

/* static */ int
DOM_HTMLSelectElement::addOrRemove(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	BOOL is_add = data == 0;

	if (argc >= 0)
	{
		DOM_THIS_OBJECT(select, DOM_TYPE_HTML_SELECTELEMENT, DOM_HTMLSelectElement);

		DOM_HTMLElement *option = NULL, *reference = NULL;
		int index = -1;

		if (is_add)
		{
			DOM_CHECK_ARGUMENTS("o");
			DOM_ARGUMENT_OBJECT_EXISTING_OPTIONAL(option, 0, DOM_TYPE_HTML_OPTIONELEMENT, DOM_HTMLOptionElement);
			if (!option)
				DOM_ARGUMENT_OBJECT_EXISTING(option, 0, DOM_TYPE_HTML_OPTGROUPELEMENT, DOM_HTMLOptGroupElement);
			if (argc > 1)
				if (argv[1].type == VALUE_OBJECT)
				{
					DOM_ARGUMENT_OBJECT_EXISTING_OPTIONAL(reference, 1, DOM_TYPE_HTML_OPTIONELEMENT, DOM_HTMLOptionElement);
					if (!reference)
						DOM_ARGUMENT_OBJECT_EXISTING(reference, 1, DOM_TYPE_HTML_OPTGROUPELEMENT, DOM_HTMLOptGroupElement);
				}
				else if (argv[1].type == VALUE_NUMBER)
				{
					index = op_isfinite(argv[1].value.number) ? (int) argv[1].value.number : -2;

					if (index < -1)
						return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
				}
				else if (argv[1].type != VALUE_NULL && argv[1].type != VALUE_UNDEFINED)
					return ES_FAILED;
		}
		else
		{
			// Remove an option; negative indices are flagged as errors - anything else is coerced to 0.
			// Both IE and WebKit performs coercion to 0; WebKit likewise treating an empty argument list.
			// Mirror both these behaviours here.
			// (Mozilla does not implement HTMLSelectElement.remove() in FF3.x, at least)
			if (argc > 0)
			{
				DOM_CHECK_ARGUMENTS("N");
				index = op_isfinite(argv[0].value.number) ? static_cast<int>(argv[0].value.number) : 0;
			}
			else
				index = 0;

			if (index < 0)
				return ES_FAILED;
		}

		return_value->type = VALUE_OBJECT;
		return ChangeOptions(select, is_add, !is_add, FALSE, option, reference, index, return_value, origining_runtime, return_value->value.object);
	}
	else
		return ChangeOptionsRestart(return_value, return_value->value.object);
}

/* static */ int
DOM_HTMLSelectElement::item(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(select, DOM_TYPE_HTML_SELECTELEMENT, DOM_HTMLSelectElement);
	DOM_CHECK_ARGUMENTS("N");

	int index = TruncateDoubleToInt(argv[0].value.number);

	CALL_FAILED_IF_ERROR(select->InitOptionsCollection());
	RETURN_GETNAME_AS_CALL(select->options->GetIndex(index, return_value, origining_runtime));
}

/* static */ int
DOM_HTMLSelectElement::namedItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(select, DOM_TYPE_HTML_SELECTELEMENT, DOM_HTMLSelectElement);
	DOM_CHECK_ARGUMENTS("s");

	CALL_FAILED_IF_ERROR(select->InitOptionsCollection());
	RETURN_GETNAME_AS_CALL(select->options->DOM_Collection::GetName(argv[0].value.string, OP_ATOM_UNASSIGNED, return_value, origining_runtime));
}

HTML_Element *
DOM_HTMLOptionElement::GetSelectElement()
{
	for (HTML_Element *element = this_element; element; element = element->Parent())
		if (element->IsMatchingType(HE_SELECT, NS_HTML))
			return element;

	return NULL;
}

int
DOM_HTMLOptionElement::GetOptionIndex()
{
	HTML_Element *iter = static_cast<HTML_Element *>(this_element->Prev()), *stop = GetSelectElement();

	if (stop)
	{
		int index = 0;

		while (iter != stop)
		{
			if (iter->IsMatchingType(HE_OPTION, NS_HTML))
				++index;

			iter = static_cast<HTML_Element *>(iter->Prev());
		}

		return index;
	}

	return 0;
}

/* virtual */ ES_GetState
DOM_HTMLOptionElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_value:
		if (GetThisElement()->GetValue())
		{
			DOMSetString(value, GetThisElement()->GetValue());
			return GET_SUCCESS;
		}
		// Fall through..
	case OP_ATOM_text:
		if (value)
		{
			TempBuffer *buffer = GetEmptyTempBuf();
			HTML_Element *option = GetThisElement();
			OP_ASSERT(option->IsMatchingType(HE_OPTION, NS_HTML));
			GET_FAILED_IF_ERROR(option->GetOptionText(buffer));
			DOMSetString(value, buffer);
		}
		return GET_SUCCESS;

	case OP_ATOM_index:
		if (value)
			DOMSetNumber(value, GetOptionIndex());
		return GET_SUCCESS;

	case OP_ATOM_selected:
		if (value)
			if (HTML_Element *select = GetSelectElement())
			{
				BOOL is_selected;
				GET_FAILED_IF_ERROR(select->DOMGetOptionSelected(GetEnvironment(), GetOptionIndex(), is_selected));
				DOMSetBoolean(value, is_selected);
			}
			else
				DOMSetBoolean(value, FormValueList::DOMIsFreeOptionSelected(GetThisElement()));

		return GET_SUCCESS;

	default:
		return DOM_HTMLFormsElement::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_HTMLOptionElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	ES_PutState result = PUT_SUCCESS;

	switch (property_name)
	{
	case OP_ATOM_text:
		result = SetTextContent(value, (DOM_Runtime *) origining_runtime, NULL);
		break;

	case OP_ATOM_index:
		return PUT_READ_ONLY;

	case OP_ATOM_selected:
		if (value->type != VALUE_BOOLEAN)
			return PUT_NEEDS_BOOLEAN;
		else if (HTML_Element *select = GetSelectElement())
			PUT_FAILED_IF_ERROR(select->DOMSetOptionSelected(GetEnvironment(), GetOptionIndex(), value->value.boolean));
		else
			FormValueList::DOMSetFreeOptionSelected(GetThisElement(), value->value.boolean);
		break;

	default:
		/* Note: unsafe to attempt to refresh the cache when dealing with properties handled elsewhere. */
		return DOM_HTMLFormsElement::PutName(property_name, value, origining_runtime);
	}
	/* None of the properties invalidate the cache of the options collection, so try to refresh it. */
	if (result == PUT_SUCCESS)
		if (HTML_Element *select = GetSelectElement())
			if (DOM_Object *object = select->GetESElement())
				if (object->IsA(DOM_TYPE_HTML_SELECTELEMENT))
					if (DOM_HTMLOptionsCollection *options = static_cast<DOM_HTMLSelectElement *>(object)->GetOptionsCollection())
						options->RefreshIndexCache(GetThisElement());

	return result;
}

ES_GetState
DOM_HTMLInputElement::GetDataListForInput(ES_Value* value, ES_Runtime* origining_runtime)
{
	const uni_char* datalist = this_element->GetStringAttr(ATTR_LIST);
	if (datalist && *datalist)
	{
		ES_Value arguments[1];
		ES_ValueString string_holder;
		DOMSetStringWithLength(&arguments[0], &string_holder, datalist);

		DOM_EnvironmentImpl *environment = GetEnvironment();
		GET_FAILED_IF_ERROR(environment->ConstructDocumentNode());

		int result = DOM_Document::getElementById(environment->GetDocument(),
												  arguments, 1, value,
												  (DOM_Runtime *) origining_runtime, 0);
		if (result == ES_VALUE && value->type == VALUE_OBJECT)
		{
			DOM_Object *dom_obj = DOM_Utils::GetDOM_Object(value->value.object);
			if (dom_obj->IsA(DOM_TYPE_ELEMENT))
			{
				HTML_Element* element = ((DOM_Element*)dom_obj)->GetThisElement();
				if (element && (element->IsMatchingType(HE_DATALIST, NS_HTML) ||
								element->IsMatchingType(HE_SELECT, NS_HTML)))
					return GET_SUCCESS;
			}
		}
	}

	DOMSetNull(value);
	return GET_SUCCESS;
}

/* Check if a valid date/time string and convert it into a corresponding
   DateTimeSpec if so. */
static BOOL
CheckValidDateTimeString(InputType type, const uni_char *date_string, DateTimeSpec &date_time)
{
	switch (type)
	{
	case INPUT_DATETIME:
	case INPUT_DATETIME_LOCAL:
	{
		if (date_time.SetFromISO8601String(date_string, type == INPUT_DATETIME))
			return TRUE;
		break;
	}
	case INPUT_DATE:
	{
		DaySpec date;
		if (date.SetFromISO8601String(date_string))
		{
			date_time.m_date = date;
			date_time.m_time.Clear();
			return TRUE;
		}
		break;
	}
	case INPUT_MONTH:
	{
		MonthSpec month;
		if (month.SetFromISO8601String(date_string))
		{
			date_time.m_date.SetFromMonth(month);
			date_time.m_time.Clear();
			return TRUE;
		}
		break;
	}
	case INPUT_WEEK:
	{
		WeekSpec week;
		if (week.SetFromISO8601String(date_string))
		{
			date_time.m_date = week.GetFirstDay();
			date_time.m_time.Clear();
			return TRUE;
		}
		break;
	}
	case INPUT_TIME:
	{
		TimeSpec time;
		if (time.SetFromISO8601String(date_string))
		{
			DaySpec date = {1970, 0, 1};
			date_time.m_date = date;
			date_time.m_time = time;
			return TRUE;
		}
		break;
	}
	}
	return FALSE;
}

ES_GetState
DOM_HTMLInputElement::GetNativeValueForInput(OpAtom property_name, ES_Value* value, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(property_name == OP_ATOM_valueAsDate || property_name == OP_ATOM_valueAsNumber);

	if (value)
	{
		ES_GetState result = GetName(OP_ATOM_value, value, origining_runtime);
		if (result != GET_SUCCESS)
			return result;

		if (value->type != VALUE_STRING || !value->value.string || !*value->value.string)
		{
			if (property_name == OP_ATOM_valueAsDate)
				DOMSetNull(value);
			else
				DOMSetNumber(value, op_nan(NULL));
			return GET_SUCCESS;
		}

		InputType type = this_element->GetInputType();
		switch (type)
		{
			case INPUT_DATETIME:
			case INPUT_DATE:
			case INPUT_MONTH:
			case INPUT_WEEK:
			case INPUT_TIME:
			{
				// Now we have a string that will have to be converted to a date.
				DateTimeSpec date_time;
				date_time.Clear();  // Silence compiler

				if (!CheckValidDateTimeString(type, value->value.string, date_time))
				{
					// Something that wasn't a legal value, for instance the empty string.
					if (property_name == OP_ATOM_valueAsDate)
						DOMSetNull(value);
					else
						DOMSetNumber(value, op_nan(NULL)); // Will make us return NaN.
					return GET_SUCCESS;
				}
				// 1000 * frac/10^dig_count = frac * 10^(3-dig_count)
				double milliseconds = date_time.m_time.GetFraction() * op_pow(10, 3-date_time.m_time.GetFractionDigitCount());
				double time_double =
					OpDate::MakeTime(date_time.m_time.m_hour,
									 date_time.m_time.GetMinute(),
									 date_time.m_time.GetSecond(),
									 milliseconds);
				double day_double =
					OpDate::MakeDay(date_time.m_date.m_year,
									date_time.m_date.m_month,
									date_time.m_date.m_day);
				double ms_since_1970 = OpDate::MakeDate(day_double, time_double);

				if (property_name == OP_ATOM_valueAsDate)
				{
					ES_Value date_value;
					DOMSetNumber(&date_value, ms_since_1970);
					ES_Object* date_obj;
					GET_FAILED_IF_ERROR(GetRuntime()->CreateNativeObject(date_value, ENGINE_DATE_PROTOTYPE,	&date_obj));
					DOMSetObject(value, date_obj);
				}
				else
					DOMSetNumber(value, ms_since_1970);

				break;
			}

		case INPUT_DATETIME_LOCAL:
		{
			if (property_name == OP_ATOM_valueAsNumber)
			{
				DateTimeSpec date_time;
				date_time.Clear();
				if (date_time.SetFromISO8601String(value->value.string, FALSE))
					DOMSetNumber(value, date_time.AsDouble());
				else
					DOMSetNumber(value, op_nan(NULL));
			}
			else
				DOMSetNull(value);
			return GET_SUCCESS;
		}
		case INPUT_NUMBER:
		case INPUT_RANGE:
			if (property_name == OP_ATOM_valueAsNumber)
			{
				// Apply the 'number to string' algorithm defined for the above
				// two input types. Luckily that algorithm is the same,
				// converting the string to a floating point value.
				uni_char *end_ptr;
				double val = uni_strtod(value->value.string, &end_ptr);
				if (end_ptr == value->value.string || uni_isspace(*(end_ptr-1)))
					DOMSetNumber(value, op_nan(NULL)); // This wasn't a number
				else
				{
					while (uni_isspace(*end_ptr)) // Trailing whitespace is allowed
						end_ptr++;
					if (*end_ptr)
						DOMSetNumber(value, op_nan(NULL)); // This wasn't a number
					else
						DOMSetNumber(value, val);
				}
				break;
			}
			/* fall through */

		default:
			if (property_name == OP_ATOM_valueAsDate)
				DOMSetNull(value);
			else
				DOMSetNumber(value, op_nan(NULL));
			break;
		}
	}
	return GET_SUCCESS;
}

ES_PutState
DOM_HTMLInputElement::PutNativeValueForInput(OpAtom property_name, ES_Value* value, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(property_name == OP_ATOM_valueAsDate || property_name == OP_ATOM_valueAsNumber);
	InputType type = this_element->GetInputType();
	switch (type)
	{
	case INPUT_DATETIME:
	case INPUT_DATETIME_LOCAL:
	case INPUT_DATE:
	case INPUT_MONTH:
	case INPUT_WEEK:
	case INPUT_TIME:
	{
		double ms_since_1970;
		if (property_name == OP_ATOM_valueAsNumber && value->type == VALUE_NUMBER)
			ms_since_1970 = value->value.number;
		else if (type != INPUT_DATETIME_LOCAL && value->type == VALUE_OBJECT && op_strcmp(ES_Runtime::GetClass(value->value.object), "Date") == 0)
		{
			ES_Value native_value;
			PUT_FAILED_IF_ERROR(GetRuntime()->GetNativeValueOf(value->value.object, &native_value));
			if (native_value.type != VALUE_NUMBER)
				return DOM_PUTNAME_DOMEXCEPTION(INVALID_STATE_ERR);

			ms_since_1970 = native_value.value.number;
		}
		else if (property_name == OP_ATOM_valueAsNumber)
			return PUT_NEEDS_NUMBER;
		else
			return DOM_PUTNAME_DOMEXCEPTION(INVALID_STATE_ERR);

		uni_char result[32]; // ARRAY OK 2011-06-01 sof
		switch (type)
		{
		case INPUT_DATETIME_LOCAL:
			ms_since_1970 = OpDate::LocalTime(ms_since_1970);
			/* fall through */
		case INPUT_DATETIME:
		{
			DateTimeSpec datetime;
			datetime.SetFromTime(ms_since_1970);
			datetime.ToISO8601String(result, type == INPUT_DATETIME);
			break;
		}
		case INPUT_DATE:
		{
			DaySpec date;
			date.SetFromTime(ms_since_1970);
			date.ToISO8601String(result);
			break;
		}
		case INPUT_MONTH:
		{
			MonthSpec month;
			month.SetFromTime(ms_since_1970);
			month.ToISO8601String(result);
			break;
		}
		case INPUT_WEEK:
		{
			WeekSpec week;
			week.SetFromTime(ms_since_1970);
			week.ToISO8601String(result);
			break;
		}
		case INPUT_TIME:
		{
			TimeSpec time_spec;
			time_spec.SetFromTime(ms_since_1970);
			time_spec.ToISO8601String(result);
			break;
		}
		}
		return SetFormValue(result);
	}
	case INPUT_RANGE:
	case INPUT_NUMBER:
		if (value->type == VALUE_STRING)
			return SetFormValue(value->value.string);
		else
			return PUT_NEEDS_STRING;
	default:
		return DOM_PUTNAME_DOMEXCEPTION(INVALID_STATE_ERR);
	}
}

/* virtual */ ES_GetState
DOM_HTMLInputElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	InputType input_type = GetThisElement()->GetInputType();

	switch (property_name)
	{
	case OP_ATOM_type:
		// This is the resolved input type, not the attribute value.
		if (value)
			DOMSetString(value, GetThisElement()->DOMGetInputTypeString());
		return GET_SUCCESS;
	case OP_ATOM_indeterminate:
		if (value)
			DOMSetBoolean(value, this_element->GetIndeterminate());
		return GET_SUCCESS;

	case OP_ATOM_value:
		switch (input_type)
		{
		case INPUT_FILE:
		case INPUT_TEXT:
		case INPUT_PASSWORD:
		case INPUT_URI:
		case INPUT_DATE:
		case INPUT_WEEK:
		case INPUT_TIME:
		case INPUT_EMAIL:
		case INPUT_NUMBER:
		case INPUT_RANGE:
		case INPUT_MONTH:
		case INPUT_DATETIME:
		case INPUT_DATETIME_LOCAL:
		case INPUT_COLOR:
		case INPUT_TEL:
		case INPUT_SEARCH:
			return GetFormValue(value);

		default:
			ES_GetState state = GetStringAttribute(property_name, value);
			if (value && state == GET_SUCCESS && (input_type == INPUT_RADIO || input_type == INPUT_CHECKBOX) &&
				!*value->value.string && !this_element->HasAttr(ATTR_VALUE))
				DOMSetString(value, UNI_L("on"));
			return state;
		}

	case OP_ATOM_files:
		DOMSetNull(value);
		if (value && input_type == INPUT_FILE)
		{
			DOM_FileList *filelist;

			ES_Value previous_filelist_string;
			ES_Value previous_filelist;
			OpString filelist_string;

			/* Previous FileList result is cached on the object, returning
			   a new DOM_FileList object only if the element's value has changed. */

			GET_FAILED_IF_ERROR(GetPrivate(DOM_PRIVATE_filelist_string, &previous_filelist_string));
			GET_FAILED_IF_ERROR(GetPrivate(DOM_PRIVATE_filelist, &previous_filelist));
			OP_BOOLEAN result;
			GET_FAILED_IF_ERROR(result = DOM_FileList::Construct(filelist, this_element, filelist_string, previous_filelist_string, previous_filelist, static_cast<DOM_Runtime *>(origining_runtime)));
			if (result == OpBoolean::IS_TRUE)
			{
				GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_filelist_string, previous_filelist_string));
				GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_filelist, previous_filelist));
			}

			DOMSetObject(value, filelist);
		}
		return GET_SUCCESS;

	case OP_ATOM_checked:
		if (value)
			if (input_type == INPUT_RADIO || input_type == INPUT_CHECKBOX)
			{
				value->value.boolean = !!this_element->DOMGetBoolFormValue(GetEnvironment());
				value->type = VALUE_BOOLEAN;
			}
			else
				GetBooleanAttribute(property_name, value);
		return GET_SUCCESS;
	case OP_ATOM_list:
		if (value)
			return GetDataListForInput(value, origining_runtime);
		return GET_SUCCESS;

	case OP_ATOM_formEnctype:
	case OP_ATOM_formMethod:
		return DOM_HTMLFormElement::GetEnumeratedValue(property_name, GetStringAttribute(property_name, value), value);

	case OP_ATOM_valueAsDate:
	case OP_ATOM_valueAsNumber:
		return GetNativeValueForInput(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));

	default:
		return DOM_HTMLFormsElement::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_HTMLInputElement::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	InputType input_type = this_element->GetInputType();

	switch (property_name)
	{
	case OP_ATOM_indeterminate:
		if (value->type != VALUE_BOOLEAN)
			return PUT_NEEDS_BOOLEAN;
		if (value->value.boolean != !!this_element->GetIndeterminate())
			this_element->SetIndeterminate(GetEnvironment()->GetFramesDocument(), value->value.boolean, TRUE);
		return PUT_SUCCESS;

	case OP_ATOM_name:
		/* Hack to emulate MSIE's behaviour. */
		if (HTML_Element *form_element = GetFormElement())
		{
			const uni_char *old_name = this_element->DOMGetAttribute(GetEnvironment(), ATTR_NAME, NULL, NS_IDX_DEFAULT, FALSE, FALSE);
			if (old_name)
			{
				DOM_Node *node;

				PUT_FAILED_IF_ERROR(GetEnvironment()->ConstructNode(node, form_element, owner_document));

				ES_Value this_value;
				DOMSetObject(&this_value, this);

				PUT_FAILED_IF_ERROR(node->SetInternalValue(old_name, this_value));
			}
		}
		return DOM_HTMLElement::PutName(property_name, value, origining_runtime);

	case OP_ATOM_value:
	case OP_ATOM_innerText: // MSIE-ism, see bug 248191
		if (value->type == VALUE_NULL)
			DOMSetString(value);
		switch (input_type)
		{
		case INPUT_TEXT:
		case INPUT_PASSWORD:
		case INPUT_URI:
		case INPUT_DATE:
		case INPUT_WEEK:
		case INPUT_TIME:
		case INPUT_EMAIL:
		case INPUT_NUMBER:
		case INPUT_RANGE:
		case INPUT_MONTH:
		case INPUT_DATETIME:
		case INPUT_DATETIME_LOCAL:
		case INPUT_COLOR:
		case INPUT_SEARCH:
		case INPUT_TEL:
			if (value->type != VALUE_STRING)
				return PUT_NEEDS_STRING;

			return SetFormValue(value->value.string);

		case INPUT_FILE:
		{
			if (value->type != VALUE_STRING)
				return PUT_NEEDS_STRING;

#ifdef OPERACONFIG_URL
			if (GetRuntime() == origining_runtime && origining_runtime->GetFramesDocument() && origining_runtime->GetFramesDocument()->GetURL().GetAttribute(URL::KName).Compare("opera:config") == 0)
				return SetFormValue(value->value.string);
#endif // OPERACONFIG_URL
			/* Setting a value of a file input to the empty string must clear the input's value.
			 * An attempt to set the input's value to anything else must throw. */
			if (!*value->value.string)
				return SetFormValue(UNI_L(""));
			else
				return DOM_PUTNAME_DOMEXCEPTION(INVALID_STATE_ERR);
		}

		default:
			// [TreatNullAs=EmptyString] handled above before this switch statement
			return SetStringAttribute(OP_ATOM_value, FALSE, value, origining_runtime);
		}

	case OP_ATOM_files:
		// Readonly property.
		return PUT_SUCCESS;

	case OP_ATOM_checked:
		if (input_type == INPUT_RADIO || input_type == INPUT_CHECKBOX)
			if (value->type != VALUE_BOOLEAN)
				return PUT_NEEDS_BOOLEAN;
			else
			{
				this_element->DOMSetBoolFormValue(GetEnvironment(), value->value.boolean);
				return PUT_SUCCESS;
			}
		else
			return SetBooleanAttribute(property_name, value, origining_runtime);

	case OP_ATOM_valueAsDate:
	case OP_ATOM_valueAsNumber:
		return PutNativeValueForInput(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));

	default:
		return DOM_HTMLFormsElement::PutName(property_name, value, origining_runtime);
	}
}

/* static */ int
DOM_HTMLInputElement::stepUpDown(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(element, DOM_TYPE_HTML_INPUTELEMENT, DOM_HTMLInputElement);

	InputType type = element->GetThisElement()->GetInputType();
	switch (type)
	{
	case INPUT_DATETIME:
	case INPUT_DATE:
	case INPUT_MONTH:
	case INPUT_WEEK:
	case INPUT_TIME:
	case INPUT_DATETIME_LOCAL:
	case INPUT_NUMBER:
	case INPUT_RANGE:
		break;
	default:
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);
	}

	DOM_CHECK_ARGUMENTS("|n");

	int step_count;
	if (argc == 0)
		step_count = 1;
	else
		step_count = TruncateDoubleToInt(argv[0].value.number);

	if (data == STEP_DOWN)
		step_count = -step_count;

	OP_STATUS status = element->GetThisElement()->DOMStepUpDownFormValue(this_object->GetEnvironment(), step_count);
	if (status == OpStatus::ERR_NOT_SUPPORTED || status == OpStatus::ERR_OUT_OF_RANGE)
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

	CALL_FAILED_IF_ERROR(status);
	return ES_FAILED;
}

/* virtual */ ES_GetState
DOM_HTMLTextAreaElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_defaultValue:
		return GetTextContent(value);

	case OP_ATOM_type:
		DOMSetString(value, UNI_L("textarea"));
		return GET_SUCCESS;

	case OP_ATOM_value:
		/* LF-normalized rather than CRLF. */
		return GetFormValue(value, FALSE);

	case OP_ATOM_innerText:
		return GetFormValue(value, TRUE);

	default:
		return DOM_HTMLFormsElement::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_HTMLTextAreaElement::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	ES_PutState state;

	switch (property_name)
	{
	case OP_ATOM_defaultValue:
		return DOM_HTMLElement::PutNameRestart(OP_ATOM_text, value, origining_runtime, NULL);

	case OP_ATOM_type:
		return PUT_READ_ONLY;

	case OP_ATOM_value:
	case OP_ATOM_innerText:
		if (value->type == VALUE_NULL)
			DOMSetString(value);
		else if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		state = SetFormValue(value->value.string);
		if (state != PUT_SUCCESS || property_name != OP_ATOM_innerText)
			return state;
		/* Fall through for innerText. */

	default:
		return DOM_HTMLFormsElement::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_HTMLTextAreaElement::PutNameRestart(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime, ES_Object *restart_object)
{
	if (property_name == OP_ATOM_defaultValue)
		return DOM_HTMLElement::PutNameRestart(OP_ATOM_text, value, origining_runtime, restart_object);

	return DOM_HTMLFormsElement::PutNameRestart(property_name, value, origining_runtime, restart_object);
}



// DOM_HTMLButtonElement

/* virtual */ ES_GetState
DOM_HTMLButtonElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_type:
		// This is the resolved input type, not the attribute value.
		if (value)
			DOMSetString(value, GetThisElement()->DOMGetInputTypeString());
		return GET_SUCCESS;

	case OP_ATOM_formEnctype:
	case OP_ATOM_formMethod:
		return DOM_HTMLFormElement::GetEnumeratedValue(property_name, GetStringAttribute(property_name, value), value);

	default:
		return DOM_HTMLFormsElement::GetName(property_name, value, origining_runtime);
	}
}

// DOM_HTMLLabelElement

/* virtual */ ES_GetState
DOM_HTMLLabelElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_control)
		return value ? GetControlForLabel(value, origining_runtime) : GET_SUCCESS;
	else
		return DOM_HTMLFormsElement::GetName(property_name, value, origining_runtime);
}

ES_GetState
DOM_HTMLLabelElement::GetControlForLabel(ES_Value* value, ES_Runtime* origining_runtime)
{
	const uni_char* for_control = this_element->GetStringAttr(ATTR_FOR);
	if (for_control && *for_control)
	{
		ES_Value arguments[1];
		ES_ValueString string_holder;
		DOMSetStringWithLength(&arguments[0], &string_holder, for_control);

		DOM_EnvironmentImpl *environment = GetEnvironment();
		GET_FAILED_IF_ERROR(environment->ConstructDocumentNode());

		int result = DOM_Document::getElementById(environment->GetDocument(), arguments, 1, value, (DOM_Runtime *) origining_runtime, 0);
		return ConvertCallToGetName(result, value);
	}

	return DOMSetElement(value, this_element->FindFirstContainedFormElm());
}


/* virtual */ ES_PutState
DOM_HTMLLabelElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_control)
		return PUT_READ_ONLY;
	else
		return DOM_HTMLFormsElement::PutName(property_name, value, origining_runtime);
}

// DOM_HTMLOutputElement

/* virtual */ ES_GetState
DOM_HTMLOutputElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_defaultValue:
		if (value)
		{
			TempBuffer *buffer = GetEmptyTempBuf();

			if (OpStatus::IsSuccess(this_element->DOMGetDefaultOutputValue(GetEnvironment(), buffer)))
				DOMSetString(value, buffer);
			else
				return GET_NO_MEMORY;
		}
		return GET_SUCCESS;
	case OP_ATOM_value:
		return GetFormValue(value);

	default:
		return DOM_HTMLFormsElement::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_HTMLOutputElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_value:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		PUT_FAILED_IF_ERROR(SetFormValue(value->value.string));
		return PUT_SUCCESS;

	case OP_ATOM_defaultValue:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		PUT_FAILED_IF_ERROR(this_element->DOMSetDefaultOutputValue(GetEnvironment(), value->value.string));
		return PUT_SUCCESS;

	default:
		return DOM_HTMLFormsElement::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_HTMLOutputElement::PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	if (property_name == OP_ATOM_value)
		return SetTextContent(value, (DOM_Runtime *) origining_runtime, restart_object);
	else
		return DOM_HTMLFormsElement::PutNameRestart(property_name, value, origining_runtime, restart_object);
}



/* virtual */ ES_GetState
DOM_HTMLFieldsetElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_elements:
		if (value)
		{
			GET_FAILED_IF_ERROR(InitElementsCollection());
			DOMSetObject(value, elements);
		}
		return GET_SUCCESS;

	default:
		return DOM_HTMLFormsElement::GetName(property_name, value, origining_runtime);
	}
}


/* virtual */ ES_PutState
DOM_HTMLFieldsetElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_elements)
		return PUT_READ_ONLY;

	return DOM_HTMLFormsElement::PutName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_HTMLFieldsetElement::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	DOM_HTMLElement::DOMChangeRuntime(new_runtime);

	if (elements)
		elements->DOMChangeRuntime(new_runtime);
}

OP_STATUS
DOM_HTMLFieldsetElement::InitElementsCollection()
{
	if (!elements)
	{
		RETURN_IF_ERROR(DOM_initCollection(elements, this, this, FORMELEMENTS, DOM_PRIVATE_elements));
		// We don't call elements->SetUseFormNr(); since we want all form controls regardless of form.
	}

	return OpStatus::OK;
}

// DOM_HTMLDataListElement

/* virtual */ ES_GetState
DOM_HTMLDataListElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_options:
		if (value)
		{
			GET_FAILED_IF_ERROR(InitOptionsCollection());
			DOMSetObject(value, options);
		}
		return GET_SUCCESS;
	}
	return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_HTMLDataListElement::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	DOM_HTMLElement::DOMChangeRuntime(new_runtime);

	if (options)
		options->DOMChangeRuntime(new_runtime);
}

OP_STATUS
DOM_HTMLDataListElement::InitOptionsCollection()
{
	if (!options)
	{
		DOM_HTMLOptionsCollection *collection;

		RETURN_IF_ERROR(DOM_HTMLOptionsCollection::Make(collection, GetEnvironment(), this));
		RETURN_IF_ERROR(PutPrivate(DOM_PRIVATE_options, collection));

		options = collection;
	}

	return OpStatus::OK;
}


// DOM_HTMLKeygenElement

ES_GetState DOM_HTMLKeygenElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_type:
		DOMSetString(value, UNI_L("keygen"));
		return GET_SUCCESS;
	case OP_ATOM_keytype:
		{
		const uni_char *keytype = this_element->DOMGetAttribute(GetEnvironment(), ATTR_KEYTYPE, NULL, NS_IDX_DEFAULT, FALSE, FALSE);
		if (keytype && uni_stri_eq(keytype, "rsa"))
			keytype = UNI_L("rsa");
		else
			keytype = NULL;
		DOMSetString(value, keytype);
		return GET_SUCCESS;
		}
	}
	return DOM_HTMLFormsElement::GetName(property_name, value, origining_runtime);
}

ES_PutState DOM_HTMLKeygenElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_type:
		return PUT_READ_ONLY;
	case OP_ATOM_keytype:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else
		{
			if (!*value->value.string)
				this_element->DOMRemoveAttribute(GetEnvironment(), UNI_L("keytype"), NS_IDX_HTML, TRUE);
			else
			{
				if (uni_stri_eq(value->value.string, "rsa"))
					value->value.string = UNI_L("rsa");
				return SetStringAttribute(property_name, FALSE, value, origining_runtime);
			}
			return PUT_SUCCESS;
		}
	}
	return DOM_HTMLFormsElement::PutName(property_name, value, origining_runtime);
}

// DOM_HTMLProgressElement

ES_GetState DOM_HTMLProgressElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_form:
		if (value)
			return DOMSetElement(value, FormManager::FindFormElm(GetFramesDocument(), this_element));
		return GET_SUCCESS;
	case OP_ATOM_labels:
		if (value)
		{
			GET_FAILED_IF_ERROR(DOM_HTMLFormsElement::InitLabelsCollection(this, labels));
			DOMSetObject(value, labels);
		}
		return GET_SUCCESS;
	case OP_ATOM_value:
		if (value)
		{
			BOOL absent;
			double val = this_element->DOMGetNumericAttribute(GetEnvironment(), ATTR_VALUE, NULL, NS_IDX_DEFAULT, absent);
			double max = this_element->DOMGetNumericAttribute(GetEnvironment(), ATTR_MAX, NULL, NS_IDX_DEFAULT, absent);
			if (max <= 0)
				max = 1;
			val = MAX(val, 0);
			val = MIN(val, max);
			DOMSetNumber(value, val);
		}
		return GET_SUCCESS;
	case OP_ATOM_position:
		if (value)
			DOMSetNumber(value, WebForms2Number::GetProgressPosition(GetFramesDocument(), this_element));
		return GET_SUCCESS;
	};
	return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
}

ES_PutState DOM_HTMLProgressElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_form:
	case OP_ATOM_labels:
	case OP_ATOM_position:
		return PUT_READ_ONLY;
	};
	return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
}

void DOM_HTMLProgressElement::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	if (labels)
		labels->DOMChangeRuntime(new_runtime);
}

// DOM_HTMLMeterElement

ES_GetState DOM_HTMLMeterElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_form:
		if (value)
			return DOMSetElement(value, FormManager::FindFormElm(GetFramesDocument(), this_element));
		return GET_SUCCESS;
	case OP_ATOM_labels:
		if (value)
		{
			GET_FAILED_IF_ERROR(DOM_HTMLFormsElement::InitLabelsCollection(this, labels));
			DOMSetObject(value, labels);
		}
		return GET_SUCCESS;
	case OP_ATOM_value:
	case OP_ATOM_optimum:
	case OP_ATOM_min:
	case OP_ATOM_max:
	case OP_ATOM_low:
	case OP_ATOM_high:
		if (value)
		{
			double val, min, max, low, high, optimum;
			WebForms2Number::GetMeterValues(GetFramesDocument(), this_element, val, min, max, low, high, optimum);
			switch (property_name)
			{
			case OP_ATOM_value:
				DOMSetNumber(value, val);
				break;
			case OP_ATOM_optimum:
				DOMSetNumber(value, optimum);
				break;
			case OP_ATOM_min:
				DOMSetNumber(value, min);
				break;
			case OP_ATOM_max:
				DOMSetNumber(value, max);
				break;
			case OP_ATOM_low:
				DOMSetNumber(value, low);
				break;
			case OP_ATOM_high:
				DOMSetNumber(value, high);
				break;
			}
		}
		return GET_SUCCESS;
	};
	return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
}

ES_PutState DOM_HTMLMeterElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_form:
	case OP_ATOM_labels:
		return PUT_READ_ONLY;
	};
	return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
}

void DOM_HTMLMeterElement::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	if (labels)
		labels->DOMChangeRuntime(new_runtime);
}

// DOM_HTMLJSLinkElement

/* virtual */ ES_GetState
DOM_HTMLJSLinkElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	// JavaScript version 1.3 properties, not a part of DOM

	// As for PutName() below, save a teeny bit of work by catching the relevant properties first.
	switch (property_name)
	{
	case OP_ATOM_hash:
	case OP_ATOM_host:
	case OP_ATOM_hostname:
	case OP_ATOM_pathname:
	case OP_ATOM_port:
	case OP_ATOM_protocol:
	case OP_ATOM_search:
		break;
	default:
		return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
	}

	FramesDocument* doc = GetFramesDocument();
	LogicalDocument* logdoc = doc ? doc->GetLogicalDocument() : NULL;
	if (URL *src = GetThisElement()->GetUrlAttr(ATTR_HREF, NS_IDX_HTML, logdoc))
	{
		TempBuffer *buf = GetEmptyTempBuf();
		ServerName *servername = src->GetServerName();

		switch (property_name)
		{
		case OP_ATOM_hash:
			if (value)
			{
				const uni_char *curr_frag = src->GetAttribute(URL::KUniFragment_Name).CStr();
				if (curr_frag && curr_frag[0])
				{
					if(curr_frag[0]!='#')
						GET_FAILED_IF_ERROR(buf->Append('#'));
					GET_FAILED_IF_ERROR(buf->Append(curr_frag));
					DOMSetString(value, buf->GetStorage());
				}
				else
					DOMSetString(value, UNI_L(""));
			}
			return GET_SUCCESS;

		case OP_ATOM_host:
		case OP_ATOM_hostname:
			if (value)
			{
				const uni_char *name = servername ? servername->UniName() : NULL;

				if (property_name == OP_ATOM_host)
				{
					unsigned short port = src->GetAttribute(URL::KServerPort);
					if (port) // Hostname must always use resolved port
					{
						GET_FAILED_IF_ERROR(buf->Append(name));
						GET_FAILED_IF_ERROR(buf->Append(":"));
						GET_FAILED_IF_ERROR(buf->AppendUnsignedLong(port));
						name = buf->GetStorage();
					}
				}

				DOMSetString(value, name);
			}
			return GET_SUCCESS;

		case OP_ATOM_pathname:
			if (value)
			{
				const uni_char *string = src->GetAttribute(URL::KUniPath_Escaped).CStr();

				if (string)
				{
					if (*string == '/')
					{
						// Don't include a leading '/'. for protocols with servername and pathname
						const protocol_selentry* protocol_info = static_cast<const protocol_selentry*>(src->GetAttribute(URL::KProtocolScheme, NULL));
						BOOL might_have_servername = protocol_info == NULL || protocol_info->have_servername;
						if(!might_have_servername)
							string++;
					}

					// In URL, the path includes the query part for some protocols
					// which shouldn't, sofor the DOM property so we
					// need to truncate at the ?
					const uni_char *query_part = uni_strchr(string, '?');
					if (query_part)
					{
						size_t len = query_part - string;
						GET_FAILED_IF_ERROR(buf->Append(string, len));
						string = buf->GetStorage();
					}
				}
				DOMSetString(value, string);
			}
			return GET_SUCCESS;

		case OP_ATOM_port:
			if (value)
			{
				// Port must always use resolved port
				int serverport = src->GetAttribute(URL::KServerPort);

				if (serverport)
					GET_FAILED_IF_ERROR(buf->AppendUnsignedLong(serverport));

				DOMSetString(value, buf);
			}
			return GET_SUCCESS;

		case OP_ATOM_protocol:
			if (value)
			{
				const char *protocol = src->GetAttribute(URL::KProtocolName).CStr();
				if (protocol && *protocol)
				{
					GET_FAILED_IF_ERROR(buf->Append(protocol));
					GET_FAILED_IF_ERROR(buf->Append((uni_char) ':'));
				}

				DOMSetString(value, buf);
			}
			return GET_SUCCESS;

		case OP_ATOM_search:
			if (value)
			{
				const uni_char* search = src->GetAttribute(URL::KUniQuery_Escaped).CStr();
				if (search==NULL)
				{
					const uni_char *name = src->GetAttribute(URL::KUniPath_Escaped).CStr();

					if (name!=NULL)
						search = uni_strchr(name, '?');
				}
				// search[1]!=0 is there to prevent standalone ? like in IE and FF
				DOMSetString(value, search && (search[0]!='?' || search[1]!=0) ? search : NULL);
			}
			return GET_SUCCESS;
		}
	}

	return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLJSLinkElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	// This whole function is a bit expensive, so check
	// first if we're handling one of the supported attributes
	switch(property_name)
	{
	case OP_ATOM_protocol:
	case OP_ATOM_host:
	case OP_ATOM_hostname:
	case OP_ATOM_port:
	case OP_ATOM_pathname:
	case OP_ATOM_search:
	case OP_ATOM_hash:
		break;
	default:
		return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
	}

	if (value->type != VALUE_STRING)
		return PUT_NEEDS_STRING;
	const uni_char *value_string = value->value.string;
	while (uni_isspace(value_string[0]))
		++value_string;

	// Protocol must never be empty!
	if (property_name == OP_ATOM_protocol && !op_isalnum(*value_string))
		return PUT_SUCCESS;

	ES_Value final_value;
	URL *src = GetThisElement()->GetUrlAttr(ATTR_HREF, NS_IDX_HTML, GetLogicalDocument());
	if(!src)
	{
		// If there is no URL then the attribute might be empty
		// if so, initialize with an empty string
		DOMSetString(&final_value);
		ES_PutState ret = DOM_HTMLElement::PutName(OP_ATOM_href, &final_value, origining_runtime);
		if (ret == PUT_FAILED || ret == PUT_NO_MEMORY)
			return ret;

		src = GetThisElement()->GetUrlAttr(ATTR_HREF, NS_IDX_HTML, GetLogicalDocument());
		if (!src) // oom of some kind?
			return PUT_NO_MEMORY;
	}
	// We must have an URL object
	OP_ASSERT(src);

	int value_length = 0;
	const protocol_selentry *protocol_info = NULL;
	//we cannot set components of a url with unkown scheme
	protocol_info = reinterpret_cast<const protocol_selentry*>(src->GetAttribute(URL::KProtocolScheme, NULL));
	BOOL has_servername = protocol_info && protocol_info->have_servername;
	// Because we can't change the different parts of the URL object
	// we need to rebuild the url from scratch and assign it to href, so next time
	// it'll be reparsed into a new URL object, and synchronized with the href attribute

	// Protocol -> need to upcast to uni_char
	OpString curr_protocol_obj;
	PUT_FAILED_IF_ERROR(src->GetAttribute(URL::KProtocolName, curr_protocol_obj));
	const uni_char *curr_protocol = curr_protocol_obj.CStr();

	// Hostname -> nothing much
	OpString curr_hostname_obj;
	PUT_FAILED_IF_ERROR(src->GetAttribute(URL::KUniHostName, curr_hostname_obj));
	const uni_char *curr_hostname = curr_hostname_obj.CStr();

	int curr_port = src->GetAttribute(URL::KServerPort);

	// Path and query might be separated, or might not
	OpString curr_pathname_obj;
	PUT_FAILED_IF_ERROR(src->GetAttribute(URL::KUniPath_Escaped, curr_pathname_obj));
	const uni_char* curr_pathname = curr_pathname_obj.CStr();

	OpString curr_query_obj;
	PUT_FAILED_IF_ERROR(src->GetAttribute(URL::KUniQuery_Escaped, curr_query_obj));
	const uni_char* curr_query = curr_query_obj.CStr();
	const uni_char* curr_frag = src->GetAttribute(URL::KUniFragment_Name).CStr();


	TempBuffer* buf = GetEmptyTempBuf();

	switch (property_name)
	{
	case OP_ATOM_protocol:
	{
		while (IsValidDomainChar(value_string[value_length]))
			value_length++;

		PUT_FAILED_IF_ERROR(buf->Append(value_string, value_length));
		curr_protocol = buf->GetStorage();
		has_servername = !(curr_hostname==NULL || curr_hostname[0]==0);

		// Because this can be an unknown protocol like unknown://.../ we need to treat it like http:
		// for semi compatibility with IE and FF, while we should actually just drop the entire url, but oh well...
		OpStringC scheme_obj(curr_protocol);
		if (URL_NULL_TYPE == g_url_api->RegisterUrlScheme(scheme_obj, has_servername, URL_HTTP, &has_servername))
			return PUT_NO_MEMORY;
		break;
	}
	case OP_ATOM_hostname:
	case OP_ATOM_host:
	{
		if (OP_ATOM_host == property_name)
		{
			const uni_char *port_needle = uni_strrchr(value_string, ':');
			if (port_needle!=NULL)
			{
				curr_port = uni_atoi(port_needle+1);
				if ((curr_port & ~0xffff) || curr_port == 0)
					return PUT_SUCCESS; // was a bad value, just ignore it
			}
			else // Set to zero so we revert to the default scheme port
				curr_port = 0;
		}
		// Parse url until 1st bogus char
		while (IsValidDomainChar(value_string[value_length]))
			value_length++;
		// Remove trailing dots
		while (value_string[value_length]=='.')
			value_length--;
		// Insert valid domain chunk
		PUT_FAILED_IF_ERROR(buf->Append(value_string, value_length));
		curr_hostname = buf->GetStorage();

		break;
	}
	case OP_ATOM_port:
		curr_port = uni_atoi(value_string);
		if ((curr_port & ~0xffff) || curr_port == 0)
			return PUT_SUCCESS; // Illegal port, just ignore the attempt
		break;
	case OP_ATOM_pathname:
		PUT_FAILED_IF_ERROR(buf->Expand(UriEscape::GetEscapedLength(value_string, UriEscape::Hash | UriEscape::QueryUnsafe | UriEscape::UniCtrl) + 1));
		UriEscape::Escape(buf->GetStorage(), value_string, UriEscape::Hash | UriEscape::QueryUnsafe | UriEscape::UniCtrl);
		curr_pathname = buf->GetStorage();
		break;
	case OP_ATOM_search:
		PUT_FAILED_IF_ERROR(buf->Expand(UriEscape::GetEscapedLength(value_string, UriEscape::Hash | UriEscape::UniCtrl) + 1));
		UriEscape::Escape(buf->GetStorage(), value_string, UriEscape::Hash | UriEscape::UniCtrl);
		curr_query = buf->GetStorage();
		break;
	case OP_ATOM_hash:
		curr_frag = value_string;
		break;
	}

	while (curr_pathname && curr_pathname[0] == '/')
		curr_pathname++;
	if (curr_query && curr_query[0] == '?')
		curr_query++;
	if (curr_frag && curr_frag[0] == '#')
		curr_frag++;

	TempBuffer newbuf;
	newbuf.SetExpansionPolicy(TempBuffer::AGGRESSIVE);

	// Now rebuild the href attribute value and set it
	PUT_FAILED_IF_ERROR(newbuf.Append(curr_protocol));

	if (!has_servername)
	{
		PUT_FAILED_IF_ERROR(newbuf.Append(UNI_L(":")));
	}
	else
	{
		PUT_FAILED_IF_ERROR(newbuf.Append(UNI_L("://")));
		PUT_FAILED_IF_ERROR(newbuf.Append(curr_hostname));
		if (curr_hostname && *curr_hostname && curr_port) // Only append port if we have host
		{
			PUT_FAILED_IF_ERROR(newbuf.Append(':'));
			PUT_FAILED_IF_ERROR(newbuf.AppendUnsignedLong(curr_port & 0xffff));
		}
		if (!curr_hostname || uni_strchr(curr_hostname, '/') == NULL)
			//if the hostname has a slash, then we converted from unknown scheme to known one
			PUT_FAILED_IF_ERROR(newbuf.Append('/'));
	}

	PUT_FAILED_IF_ERROR(newbuf.Append(curr_pathname));
	if (curr_query)
	{
		PUT_FAILED_IF_ERROR(newbuf.Append('?'));
		PUT_FAILED_IF_ERROR(newbuf.Append(curr_query));
	}
	if (curr_frag)
	{
		PUT_FAILED_IF_ERROR(newbuf.Append('#'));
		PUT_FAILED_IF_ERROR(newbuf.Append(curr_frag));
	}

	// We end up by setting a fresh new href value
	DOMSetString(&final_value, newbuf.GetStorage());
	return DOM_HTMLElement::PutName(OP_ATOM_href, &final_value, origining_runtime);
}
// DOM_HTMLAnchorElement

/* virtual */ ES_GetState
DOM_HTMLAnchorElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_text)
		return GetTextContent(value);
	else
		return DOM_HTMLJSLinkElement::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLAnchorElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_text)
		return DOM_HTMLElement::PutNameRestart(OP_ATOM_text, value, origining_runtime, NULL);
	else
		return DOM_HTMLJSLinkElement::PutName(property_name, value, origining_runtime);
}

// DOM_HTMLImageElement

/* virtual */ ES_GetState
DOM_HTMLImageElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_lowsrc:
		if (value)
			DOMSetString(value, this_element->DOMGetAttribute(GetEnvironment(), ATTR_XML, UNI_L("lowsrc"), NS_IDX_HTML, TRUE, FALSE));
		return GET_SUCCESS;

	case OP_ATOM_complete:
		if (value)
		{
			BOOL complete = FALSE;
			if (HEListElm *helm = this_element->GetHEListElm(FALSE))
			{
				/* The check wrt 'old url' is needed to determine if
				   the helm's current image isn't about to be replaced
				   (and hence wouldn't be complete.) */
				if (LoadInlineElm *lie = helm->GetLoadInlineElm())
					complete = helm->GetOldUrl().IsEmpty() && lie->GetLoaded() && !helm->GetImage().IsFailed();
			}
			else
			{
				const uni_char *str = this_element->GetStringAttr(ATTR_SRC, NS_IDX_HTML);
				complete = !str || !*str;
			}
			DOMSetBoolean(value, complete);
		}
		return GET_SUCCESS;

	case OP_ATOM_naturalWidth:
	case OP_ATOM_naturalHeight:
		if (value)
		{
			unsigned w, h;
			if (this_element->DOMGetImageSize(GetEnvironment(), w, h))
				DOMSetNumber(value, property_name == OP_ATOM_naturalWidth ? w : h);
			else
				DOMSetNumber(value, 0);
		}
		return GET_SUCCESS;

	case OP_ATOM_x:
	case OP_ATOM_y:
		if (value)
		{
			if (ShouldBlockWaitingForStyle(GetFramesDocument(), origining_runtime))
			{
				DOM_DelayedLayoutListener *listener = OP_NEW(DOM_DelayedLayoutListener, (GetFramesDocument(), GetCurrentThread(origining_runtime)));
				if (!listener)
					return GET_NO_MEMORY;

				DOMSetNull(value);
				return GET_SUSPEND;
			}

			return DOM_HTMLImageElement::GetNameRestart(property_name, value, origining_runtime, NULL);
		}
		return GET_SUCCESS;

	default:
		return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_GetState
DOM_HTMLImageElement::GetNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	switch (property_name)
	{
		case OP_ATOM_x:
		case OP_ATOM_y:
			{
				int x, y;
				GET_FAILED_IF_ERROR(this_element->DOMGetXYPosition(GetEnvironment(), x, y));
				DOMSetNumber(value, property_name == OP_ATOM_x ? x : y);
				return GET_SUCCESS;
			}
		default:
			return DOM_HTMLElement::GetNameRestart(property_name, value, origining_runtime, restart_object);
	}
}

/* virtual */ ES_PutState
DOM_HTMLImageElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_lowsrc:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		PUT_FAILED_IF_ERROR(this_element->DOMSetAttribute(GetEnvironment(), ATTR_XML, UNI_L("lowsrc"), NS_IDX_HTML, value->value.string, value->GetStringLength(), TRUE));
		return PUT_SUCCESS;

	case OP_ATOM_complete:
		return PUT_READ_ONLY;

	default:
		return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
	}
}

// DOM_HTMLMarqueeElement
/* static */ int
DOM_HTMLMarqueeElement::startOrStop(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	OP_ASSERT(data == 0 || data == 1);
	DOM_THIS_OBJECT(domelem, DOM_TYPE_HTML_ELEMENT, DOM_HTMLElement);
	domelem->GetThisElement()->DOMMarqueeStartStop(data == 1);
	return ES_FAILED;
}


// DOM_HTMLPluginElement

#ifdef _PLUGIN_SUPPORT_

DOM_HTMLPluginElement::DOM_HTMLPluginElement()
	: is_connected(FALSE)
{
}

OP_STATUS
DOM_HTMLPluginElement::ConnectToPlugin(ES_Runtime *origining_runtime, BOOL allow_suspend, ES_Object*& suspended_object, BOOL& is_resumed_from_connection_wait, ES_Object *restart_object)
{
	ES_Value value;
	OpNS4Plugin *plugin = NULL;
	FramesDocument* frames_doc = GetFramesDocument();
	OP_STATUS res = OpStatus::OK;

	if (frames_doc && (plugin = this_element->DOMGetNS4Plugin()) != NULL)
	{
		// plugin has been created
		RETURN_IF_ERROR(plugin->GetScriptableObject(origining_runtime, allow_suspend, &value));
		if (allow_suspend)
		{
			if (OpNS4PluginHandler::GetHandler()->IsSuspended(frames_doc, this_element) && value.type == VALUE_OBJECT)
			{
				// the plugin has suspended the script execution and returned a plugin restart object
				if (is_connected)
					is_connected = FALSE; // make sure that an object is not scripted after the embedded element has been removed
				suspended_object = value.value.object;
				return OpStatus::OK;
			}
			else if (restart_object)
			{
				// check if the plugin has been suspended and resumed, that is, if this restart_object is of type PLUGIN_RESTART*
				is_resumed_from_connection_wait = OpNS4PluginHandler::GetHandler()->IsPluginStartupRestartObject(restart_object);
			}
		}
		else
			OP_ASSERT(!OpNS4PluginHandler::GetHandler()->IsSuspended(frames_doc, this_element));
	}
	else if (allow_suspend && frames_doc && frames_doc->GetLogicalDocument())
	{
		if (is_connected)
			is_connected = FALSE; // make sure that an object is not scripted after the embedded element has been removed

		// Default guess is that we need to block waiting for a plugin
		// but try to find as many chances as possible to avoid that to
		// improve script performance and reduce risk of hung scripts
		BOOL plugin_needs_to_be_loaded = TRUE;

		if (restart_object)
			plugin_needs_to_be_loaded = FALSE;
		else if (this_element->IsLayoutFinished() &&
		         !frames_doc->IsLoadingPlugin(this_element))
			plugin_needs_to_be_loaded = FALSE;
		else if (frames_doc->GetWindow()->GetForcePluginsDisabled())
			plugin_needs_to_be_loaded = FALSE;
		else
		{
			OP_ASSERT(this_element->GetNsType() == NS_HTML);
			HTML_Element* doc_root = frames_doc->GetLogicalDocument()->GetRoot();
			if (!doc_root->IsAncestorOf(this_element))
			{
				// Can't wait for plugins when the element isn't even in the document tree, and
				// might never be in which case the layout engine will never see it and
				// the script never resume
				plugin_needs_to_be_loaded = FALSE;
			}
			else if (this_element->Type() == HE_EMBED &&
			         this_element->Parent()->IsMatchingType(HE_OBJECT, NS_HTML) &&
			         this_element->Parent()->IsDisplayingReplacedContent())
			{
				// This is not a plugin, it's an ignored element inside a handled
				// HE_OBJECT
				// FIXME: Must be a way to see this that doesn't involve the layout
				//        engine. HTML_Element::GetResolvedObjectType is tempting
				//        but doesn't seem to work exactly as we need it to work.
				plugin_needs_to_be_loaded = FALSE;
			}
			else if (this_element->Type() == HE_APPLET || !this_element->HasAttr(ATTR_TYPE))
			{
				int required_attribute;
				if (this_element->Type() == HE_EMBED)
					required_attribute = ATTR_SRC;
				else if (this_element->Type() == HE_APPLET)
					required_attribute = ATTR_CODE;
				else
				{
					OP_ASSERT(this_element->Type() == HE_OBJECT);
					required_attribute = ATTR_DATA;
				}
				// Don't block to wait for plugins that can't load anyway
				plugin_needs_to_be_loaded = this_element->HasAttr(required_attribute);

				if (!plugin_needs_to_be_loaded && this_element->Type() == HE_OBJECT)
				{
					// check if there is a <PARAM> with name == filename or movie
					plugin_needs_to_be_loaded = this_element->GetParamURL() ? TRUE : FALSE;
				}
			}
		}

		if (plugin_needs_to_be_loaded)
		{
			// this_element is in the document tree and is ready to be suspended, but
			// neither applet nor plugin has been created and it's too early to decide the type
			this_element->MarkDirty(frames_doc); // make sure the script will be resumed, even if no layout box is created
			res = OpNS4PluginHandler::GetHandler()->Suspend(frames_doc, this_element, origining_runtime, &value, TRUE);
			RETURN_IF_MEMORY_ERROR(res);
			if (OpStatus::IsSuccess(res) && value.type == VALUE_OBJECT)
			{
				// The plugin handler has suspended the script execution and returned a plugin restart object, but
				// not connected yet
				suspended_object = value.value.object;
				return OpStatus::OK;
			}
		}
		else
		{
			// this_element is neither an applet nor a plugin. The script should continue.
			is_resumed_from_connection_wait = TRUE;
		}
	}
	else
		is_connected = FALSE;

	if (OpStatus::IsSuccess(res) && value.type == VALUE_OBJECT)
	{
		RETURN_IF_ERROR(PutPrivate(DOM_PRIVATE_plugin_scriptable, value.value.object));
		is_connected = TRUE;
	}
	return OpStatus::OK;
}

OP_STATUS
DOM_HTMLPluginElement::GetScriptableObject(EcmaScript_Object** scriptable)
{
	ES_Value value;
	OP_BOOLEAN r = GetPrivate(DOM_PRIVATE_plugin_scriptable, &value);
	if (OpStatus::IsError(r))
		return r;
	if (r == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
		*scriptable = ES_Runtime::GetHostObject(value.value.object);
	else
		*scriptable = NULL;
	return OpStatus::OK;
}

#endif // _PLUGIN_SUPPORT_

/* virtual */
ES_GetState	DOM_HTMLPluginElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (GetThisElement()->Type() == HE_EMBED)
	{
		// If this expands, then we should give HE_EMBED a DOM class of its own, like HE_OBJECT.
		switch (property_name)
		{
		case OP_ATOM_src:
		case OP_ATOM_type:
		case OP_ATOM_width:
		case OP_ATOM_height:
		case OP_ATOM_align:
		case OP_ATOM_name:
			return GetStringAttribute(property_name, value);
		}
	}

	return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
}

/* virtual */
ES_PutState DOM_HTMLPluginElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (GetThisElement()->Type() == HE_EMBED)
	{
		// If this expands, then we should give HE_EMBED a DOM class of its own, like HE_OBJECT.
		switch (property_name)
		{
		case OP_ATOM_src:
		case OP_ATOM_type:
		case OP_ATOM_width:
		case OP_ATOM_height:
		case OP_ATOM_align:
		case OP_ATOM_name:
			return SetStringAttribute(property_name, FALSE, value, origining_runtime);
		}
	}

	return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_HTMLPluginElement::GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
#ifdef _PLUGIN_SUPPORT_
	BOOL is_resumed = FALSE;
	ES_Object* suspended_object = NULL;

	// First check if the document's style file has loaded
	FramesDocument *frames_doc = GetFramesDocument();
	if (ShouldBlockWaitingForStyle(frames_doc, origining_runtime))
	{
		DOM_DelayedLayoutListener *listener = OP_NEW(DOM_DelayedLayoutListener, (frames_doc, GetCurrentThread(origining_runtime)));
		if (!listener)
			return GET_NO_MEMORY;
		DOMSetObject(value, this);
		return GET_SUSPEND;
	}

	GET_FAILED_IF_ERROR(ConnectToPlugin(origining_runtime, TRUE, suspended_object, is_resumed));
	if (is_connected)
	{
		EcmaScript_Object* scriptable;
		GET_FAILED_IF_ERROR(GetScriptableObject(&scriptable));
		if (scriptable)
		{
			ES_GetState status = scriptable->GetName(property_name, property_code, value, origining_runtime);
			if (status != GET_FAILED)
				return status;
		}
	}
	else if (suspended_object)
	{
		DOMSetObject(value, suspended_object);
		return GET_SUSPEND;
	}
#endif // _PLUGIN_SUPPORT_

	return DOM_Element::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_HTMLPluginElement::GetNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
#ifdef _PLUGIN_SUPPORT_
	/* Now we could be restarting because we suspended to be connected, because
	   the scriptable needed to suspend or because a base class property (eg innerHTML)
	   suspended. Need to decide which it was and act appropriately. */

	if (restart_object && OpNS4PluginHandler::GetHandler()->IsSupportedRestartObject(restart_object))
	{
		BOOL is_resumed_from_connection_wait = FALSE;
		ES_Object *suspended_object = NULL;

		GET_FAILED_IF_ERROR(ConnectToPlugin(origining_runtime, TRUE, suspended_object, is_resumed_from_connection_wait, restart_object));
		if (is_connected)
		{
			EcmaScript_Object *scriptable;
			GET_FAILED_IF_ERROR(GetScriptableObject(&scriptable));
			if (scriptable)
			{
				if (is_resumed_from_connection_wait)
				{
					ES_GetState status = scriptable->GetName(property_name, property_code, value, origining_runtime);
					if (status != GET_FAILED)
						return status;
				}
				else
				{
					/* It was the scriptable that suspended and now needs to be restarted. */
					ES_GetState status = scriptable->GetNameRestart(property_name, property_code, value, origining_runtime, restart_object);
					if (status != GET_FAILED)
						return status;
				}
			}
		}
		else if (suspended_object)
		{
			/* Restarted but need to block again, hopefully for something else
			   than the first block. */
			DOMSetObject(value, suspended_object);
			return GET_SUSPEND;
		}

		/* The plugin didn't know about the property, or has gone completely
		   missing, so ask the element. */
		return DOM_Element::GetName(property_name, property_code, value, origining_runtime);
	}
	else if (!restart_object && !value)
		/* A restarted [[HasProperty]] call. */
		return GetName(property_name, property_code, value, origining_runtime);
	else if (restart_object)
		/* Check if the restart reason was waiting for layout to complete.
		   Reinitiate its GetName() in that case. */
		if (DOM_Object *object = DOM_GetHostObject(restart_object))
			if (object->IsA(DOM_TYPE_HTML_PLUGINELEMENT))
				return GetName(property_name, property_code, value, origining_runtime);
#endif // _PLUGIN_SUPPORT_

	return DOM_Object::GetNameRestart(property_name, property_code, value, origining_runtime, restart_object);
}

/* virtual */ ES_PutState
DOM_HTMLPluginElement::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
#ifdef _PLUGIN_SUPPORT_
	BOOL is_resumed_from_connection_wait = FALSE;
	ES_Object* suspended_object = NULL;

	// First check if the document's style file has loaded
	FramesDocument *frames_doc = GetFramesDocument();
	if (ShouldBlockWaitingForStyle(frames_doc, origining_runtime))
	{
		DOM_DelayedLayoutListener *listener = OP_NEW(DOM_DelayedLayoutListener, (frames_doc, GetCurrentThread(origining_runtime)));
		if (!listener)
			return PUT_NO_MEMORY;
		DOMSetObject(value, this);
		return PUT_SUSPEND;
	}

	PUT_FAILED_IF_ERROR(ConnectToPlugin(origining_runtime, TRUE, suspended_object, is_resumed_from_connection_wait));
	if (is_connected)
	{
		EcmaScript_Object* scriptable;
		PUT_FAILED_IF_ERROR(GetScriptableObject(&scriptable));
		if (scriptable)
		{
			ES_PutState status = scriptable->PutName(property_name, property_code, value, origining_runtime);
			if (status != PUT_FAILED)
				return status;
		}
	}
	else if (suspended_object)
	{
		DOMSetObject(value, suspended_object);
		return PUT_SUSPEND;
	}
#endif // _PLUGIN_SUPPORT_

	return DOM_Element::PutName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLPluginElement::PutNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
#ifdef _PLUGIN_SUPPORT_
	/* Now we could be restarting because we suspended to be connected, because
	   the scriptable needed to suspend or because a base class property (eg innerHTML)
	   suspended  Need to decide which it was and act appropriately. */

	if (restart_object)
	{
		if (restart_object == *this)
			/* Our own suspended call in PutName() (wait-for-styles) has restarted, resume it. */
			return PutName(property_name, property_code, value, origining_runtime);
		else if (OpNS4PluginHandler::GetHandler()->IsSupportedRestartObject(restart_object))
		{
			BOOL is_resumed_from_connection_wait = FALSE;
			ES_Object* suspended_object = NULL;

			PUT_FAILED_IF_ERROR(ConnectToPlugin(origining_runtime, TRUE, suspended_object, is_resumed_from_connection_wait, restart_object));
			if (is_connected)
			{
				EcmaScript_Object* scriptable;
				PUT_FAILED_IF_ERROR(GetScriptableObject(&scriptable));
				if (scriptable)
				{
					if (is_resumed_from_connection_wait)
					{
						ES_PutState status = scriptable->PutName(property_name, property_code, value, origining_runtime);
						if (status != PUT_FAILED)
							return status;
					}
					else
					{
						// It was the scriptable that suspended and now needs to be restarted.
						ES_PutState status = scriptable->PutNameRestart(property_name, property_code, value, origining_runtime, restart_object);
						if (status != PUT_FAILED)
							return status;
					}
				}
			}
			else if (suspended_object)
			{
				// Restarted but need to block again, hopefully for something else
				// than the first block.
				DOMSetObject(value, suspended_object);
				return PUT_SUSPEND;
			}

			// The plugin wasn't interested. Set it at the element instead.
			return DOM_Element::PutName(property_name, property_code, value, origining_runtime);
		}
	}
#endif // _PLUGIN_SUPPORT_

	return DOM_Object::PutNameRestart(property_name, property_code, value, origining_runtime, restart_object);
}

/* virtual */ ES_GetState
DOM_HTMLPluginElement::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
#ifdef _PLUGIN_SUPPORT_
	ES_Object* dummy1;
	BOOL dummy2;
	GET_FAILED_IF_ERROR(ConnectToPlugin(origining_runtime, FALSE, dummy1, dummy2));
	if (is_connected)
	{
		EcmaScript_Object* scriptable;
		GET_FAILED_IF_ERROR(GetScriptableObject(&scriptable));
		if (scriptable)
		{
			ES_GetState result = scriptable->FetchPropertiesL(enumerator, origining_runtime);
			OP_ASSERT(result != GET_SUSPEND);
			if (result != GET_SUCCESS)
				return result;
		}
	}
#endif // _PLUGIN_SUPPORT_

	return DOM_Element::FetchPropertiesL(enumerator, origining_runtime);
}

#ifdef SVG_DOM

/* static */ int
DOM_HTMLPluginElement::getSVGDocument(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domelem, DOM_TYPE_HTML_ELEMENT, DOM_HTMLElement);
	ES_GetState result = domelem->GetFrameEnvironment(return_value, FRAME_DOCUMENT, (DOM_Runtime *) origining_runtime);
	return ConvertGetNameToCall(result, return_value);
}

#endif // SVG_DOM

// DOM_HTMLObjectElement

/* virtual */ ES_GetState
DOM_HTMLObjectElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_contentDocument:
	case OP_ATOM_contentWindow:
		return GetFrameEnvironment(value,
			property_name == OP_ATOM_contentDocument ? FRAME_DOCUMENT : FRAME_WINDOW,
			static_cast<DOM_Runtime*>(origining_runtime));

	case OP_ATOM_form:
		if (value)
		{
			HTML_Element *iter = this_element->ParentActual();

			while (iter)
				if (iter->IsMatchingType(HE_FORM, NS_HTML))
					break;
				else
					iter = iter->ParentActual();

			return DOMSetElement(value, iter);
		}
		else
			return GET_SUCCESS;

	case OP_ATOM_willValidate:
		DOMSetBoolean(value, FALSE);
		return GET_SUCCESS;

	case OP_ATOM_validity:
		if (value)
		{
			GET_FAILED_IF_ERROR(DOM_HTMLFormsElement::InitValidityState(this, validity));
			DOMSetObject(value, validity);
		}
		return GET_SUCCESS;

	case OP_ATOM_validationMessage:
		if (value)
		{
			DOMSetString(value);
			TempBuffer *buf = GetEmptyTempBuf();
			OP_STATUS status = FormValidator::GetCustomValidationMessage(GetThisElement(), buf);
			if (OpStatus::IsSuccess(status))
				DOMSetString(value, buf);
			else if (OpStatus::IsMemoryError(status))
				return GET_NO_MEMORY;
		}
		return GET_SUCCESS;
	}

	return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLObjectElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_contentDocument:
	case OP_ATOM_contentWindow:
	case OP_ATOM_form:
	case OP_ATOM_willValidate:
	case OP_ATOM_validity:
	case OP_ATOM_validationMessage:
		return PUT_SUCCESS;
	}

	return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
}

DOM_HTMLObjectElement::DOM_HTMLObjectElement()
	:   validity(NULL)
#ifdef JS_PLUGIN_SUPPORT
	  , jsplugin_object(NULL)
#endif // JS_PLUGIN_SUPPORT
{
}

#ifdef JS_PLUGIN_SUPPORT
/* virtual */ ES_GetState
DOM_HTMLObjectElement::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (jsplugin_object)
	{
		EcmaScript_Object *hostobject = ES_Runtime::GetHostObject(jsplugin_object);

		ES_GetState state = hostobject->GetName(property_name, property_code, value, origining_runtime);
		if (state != GET_FAILED)
			return state;
		else
			return DOM_Element::GetName(property_name, property_code, value, origining_runtime);
	}

	return DOM_HTMLPluginElement::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_HTMLObjectElement::GetNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	if (jsplugin_object && restart_object)
	{
		EcmaScript_Object *restartobj = ES_Runtime::GetHostObject(restart_object);
		if (restartobj && restartobj->IsA(ES_HOSTOBJECT_JSPLUGIN))
		{
			EcmaScript_Object *hostobject = ES_Runtime::GetHostObject(jsplugin_object);
			ES_GetState state = hostobject->GetNameRestart(property_name, property_code, value, origining_runtime, restart_object);
			if (state != GET_FAILED)
				return state;
			else
				return DOM_Element::GetName(property_name, property_code, value, origining_runtime);
		}
	}

	return DOM_HTMLPluginElement::GetNameRestart(property_name, property_code, value, origining_runtime, restart_object);
}

/* virtual */ ES_PutState
DOM_HTMLObjectElement::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (jsplugin_object)
	{
		EcmaScript_Object *hostobject = ES_Runtime::GetHostObject(jsplugin_object);

		ES_PutState state = hostobject->PutName(property_name, property_code, value, origining_runtime);
		if (state != PUT_FAILED)
			return state;
		else
			return DOM_Element::PutName(property_name, property_code, value, origining_runtime);
	}

	return DOM_HTMLPluginElement::PutName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_HTMLObjectElement::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	if (jsplugin_object)
		return DOM_HTMLElement::FetchPropertiesL(enumerator, origining_runtime);
	else
		return DOM_HTMLPluginElement::FetchPropertiesL(enumerator, origining_runtime);
}

/* virtual */ void
DOM_HTMLObjectElement::GCTraceSpecial(BOOL via_tree)
{
	DOM_Element::GCTraceSpecial(via_tree);
	GCMark(jsplugin_object);
}

#endif // JS_PLUGIN_SUPPORT

#ifdef MEDIA_HTML_SUPPORT

/* virtual */
DOM_HTMLMediaElement::~DOM_HTMLMediaElement()
{
	Out(); // DOM_EnvironmentImpl::media_elements
}

/* static */ void
DOM_HTMLMediaElement::ConstructHTMLMediaElementObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	// networkState
	DOM_Object::PutNumericConstantL(object, "NETWORK_EMPTY", MediaNetwork::EMPTY, runtime);
	DOM_Object::PutNumericConstantL(object, "NETWORK_IDLE", MediaNetwork::IDLE, runtime);
	DOM_Object::PutNumericConstantL(object, "NETWORK_LOADING", MediaNetwork::LOADING, runtime);
	DOM_Object::PutNumericConstantL(object, "NETWORK_NO_SOURCE", MediaNetwork::NO_SOURCE, runtime);

	// readyState
	DOM_Object::PutNumericConstantL(object, "HAVE_NOTHING", MediaReady::NOTHING, runtime);
	DOM_Object::PutNumericConstantL(object, "HAVE_METADATA", MediaReady::METADATA, runtime);
	DOM_Object::PutNumericConstantL(object, "HAVE_CURRENT_DATA", MediaReady::CURRENT_DATA, runtime);
	DOM_Object::PutNumericConstantL(object, "HAVE_FUTURE_DATA", MediaReady::FUTURE_DATA, runtime);
	DOM_Object::PutNumericConstantL(object, "HAVE_ENOUGH_DATA", MediaReady::ENOUGH_DATA, runtime);
}

OP_STATUS
DOM_HTMLMediaElement::CreateTrackList(MediaElement* media)
{
	MediaTrackList* tracklist = media->GetTrackList();
	if (!tracklist)
		return OpStatus::ERR_NO_MEMORY;

	OP_ASSERT(tracklist->GetDOMObject() == NULL);

	RETURN_IF_ERROR(DOM_TextTrackList::Make(m_tracks, GetEnvironment(), tracklist));

	SetIsSignificant();
	return OpStatus::OK;
}

/*virtual */ ES_GetState
DOM_HTMLMediaElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	HTML_Element *htm_elem = GetThisElement();

	MediaElement *media = htm_elem->GetMediaElement();

	if (media)
	{
		switch (property_name)
		{
		case OP_ATOM_error:
			if (media->GetError() != MEDIA_ERR_NONE && m_error == NULL)
				GET_FAILED_IF_ERROR(DOM_MediaError::Make(m_error, GetEnvironment(), media->GetError()));
			DOMSetObject(value, m_error);
			return GET_SUCCESS;

		case OP_ATOM_currentSrc:
		{
			GET_FAILED_IF_ERROR(media->GetCurrentSrc()->GetAttribute(URL::KUniName_With_Fragment_Escaped, 0, m_current_src));
			DOMSetString(value, m_current_src.CStr());
			return GET_SUCCESS;
		}
		case OP_ATOM_networkState:
			DOMSetNumber(value, media->GetNetworkState().DOMValue());
			return GET_SUCCESS;

		case OP_ATOM_preload:
			DOMSetString(value, media->GetPreloadAttr().DOMValue());
			return GET_SUCCESS;

		case OP_ATOM_readyState:
			DOMSetNumber(value, media->GetReadyState().DOMValue());
			return GET_SUCCESS;

		case OP_ATOM_seeking:
			DOMSetBoolean(value, media->GetSeeking());
			return GET_SUCCESS;

		case OP_ATOM_currentTime:
			DOMSetNumber(value, media->GetPosition());
			return GET_SUCCESS;

		case OP_ATOM_duration:
			DOMSetNumber(value, media->GetDuration());
			return GET_SUCCESS;

		case OP_ATOM_paused:
			DOMSetBoolean(value, media->GetPaused());
			return GET_SUCCESS;

		case OP_ATOM_defaultPlaybackRate:
			DOMSetNumber(value, media->GetDefaultPlaybackRate());
			return GET_SUCCESS;

		case OP_ATOM_playbackRate:
			DOMSetNumber(value, media->GetPlaybackRate());
			return GET_SUCCESS;

		case OP_ATOM_buffered:
		case OP_ATOM_played:
		case OP_ATOM_seekable:
			return GetTimeRanges(property_name, media, value);

		case OP_ATOM_ended:
			DOMSetBoolean(value, media->GetPlaybackEnded());
			return GET_SUCCESS;

		case OP_ATOM_volume:
			DOMSetNumber(value, media->GetVolume());
			return GET_SUCCESS;

		case OP_ATOM_muted:
			DOMSetBoolean(value, media->GetMuted());
			return GET_SUCCESS;

		case OP_ATOM_textTracks:
			if (value)
			{
				if (!m_tracks)
					GET_FAILED_IF_ERROR(CreateTrackList(media));

				DOMSetObject(value, m_tracks);
			}
			return GET_SUCCESS;

		default:
			break;
		}
	}
	return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
}

/*virtual */ ES_PutState
DOM_HTMLMediaElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (value->type == VALUE_NUMBER && (op_isnan(value->value.number) || op_isinf(value->value.number)))
		return PUT_FAILED;

	HTML_Element *htm_elem = GetThisElement();

	MediaElement *media = htm_elem->GetMediaElement();

	if (media)
	{
		switch (property_name)
		{
		case OP_ATOM_currentTime:
		{
			if (value->type != VALUE_NUMBER)
				return PUT_NEEDS_NUMBER;
			if (media->GetReadyState() == MediaReady::NOTHING)
				return DOM_PUTNAME_DOMEXCEPTION(INVALID_STATE_ERR);
			OP_STATUS status = media->SetPosition(value->value.number);
			if (OpStatus::IsMemoryError(status))
				return PUT_NO_MEMORY;
			return PUT_SUCCESS;
		}

		case OP_ATOM_defaultPlaybackRate:
			if (value->type != VALUE_NUMBER)
				return PUT_NEEDS_NUMBER;
			PUT_FAILED_IF_ERROR(media->SetDefaultPlaybackRate(value->value.number));
			return PUT_SUCCESS;

		case OP_ATOM_playbackRate:
		{
			if (value->type != VALUE_NUMBER)
				return PUT_NEEDS_NUMBER;
			OP_STATUS status = media->SetPlaybackRate(value->value.number);
			if (OpStatus::IsMemoryError(status))
				return PUT_NO_MEMORY;
			return PUT_SUCCESS;
		}

		case OP_ATOM_volume:
			if (value->type != VALUE_NUMBER)
				return PUT_NEEDS_NUMBER;
			if (value->value.number < 0.0 || value->value.number > 1.0)
				return DOM_PUTNAME_DOMEXCEPTION(INDEX_SIZE_ERR);
			PUT_FAILED_IF_ERROR(media->SetVolume(value->value.number));
			return PUT_SUCCESS;

		case OP_ATOM_muted:
			if (value->type != VALUE_BOOLEAN)
				return PUT_NEEDS_BOOLEAN;
			PUT_FAILED_IF_ERROR(media->SetMuted(value->value.boolean));
			return PUT_SUCCESS;

		case OP_ATOM_error:
		case OP_ATOM_currentSrc:
		case OP_ATOM_networkState:
		case OP_ATOM_buffered:
		case OP_ATOM_readyState:
		case OP_ATOM_seeking:
		case OP_ATOM_duration:
		case OP_ATOM_paused:
		case OP_ATOM_played:
		case OP_ATOM_seekable:
		case OP_ATOM_ended:
		case OP_ATOM_textTracks:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}
	return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
}

/* static */ int
DOM_HTMLMediaElement::addTextTrack(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(dommedia, DOM_TYPE_HTML_MEDIAELEMENT, DOM_HTMLMediaElement);
	DOM_CHECK_ARGUMENTS("s|ss");

	MediaElement *media = dommedia->GetThisElement()->GetMediaElement();
	if (!media)
		return ES_FAILED;

	const uni_char* kind = argv[0].value.string;
	if (!(uni_str_eq(kind, "subtitles") ||
		  uni_str_eq(kind, "captions") ||
		  uni_str_eq(kind, "descriptions") ||
		  uni_str_eq(kind, "chapters") ||
		  uni_str_eq(kind, "metadata")))
		return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);

	const uni_char* label = argc > 1 ? argv[1].value.string : UNI_L("");
	const uni_char* srclang = argc > 2 ? argv[2].value.string : UNI_L("");

	MediaTrack* track;
	CALL_FAILED_IF_ERROR(MediaTrack::DOMCreate(track, kind, label, srclang));

	DOM_TextTrack* domtrack;
	OP_STATUS status = DOM_TextTrack::Make(domtrack, dommedia->GetEnvironment(), track);
	if (OpStatus::IsError(status))
		OP_DELETE(track);

	CALL_FAILED_IF_ERROR(status);

	CALL_FAILED_IF_ERROR(media->DOMAddTextTrack(track, domtrack));

	if (!dommedia->m_tracks)
		CALL_FAILED_IF_ERROR(dommedia->CreateTrackList(media));

	DOMSetObject(return_value, domtrack);
	return ES_VALUE;
}

/* static */ int
DOM_HTMLMediaElement::canPlayType(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("s");

	DOM_THIS_OBJECT(dommedia, DOM_TYPE_HTML_MEDIAELEMENT, DOM_HTMLMediaElement);
	HTML_Element *htm_elem = dommedia->GetThisElement();

	if (MediaElement *media = htm_elem->GetMediaElement())
	{
		DOMSetString(return_value, media->CanPlayType(argv[0].value.string));
		return ES_VALUE;
	}

	return ES_FAILED;
}

/* static */ int
DOM_HTMLMediaElement::load_play_pause(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(dommedia, DOM_TYPE_HTML_MEDIAELEMENT, DOM_HTMLMediaElement);
	HTML_Element *htm_elem = dommedia->GetThisElement();
	ES_Thread *thread = GetCurrentThread(origining_runtime);

	if (MediaElement *media = htm_elem->GetMediaElement())
	{
		switch (data)
		{
		case 0:
			CALL_FAILED_IF_ERROR(media->Load(thread));
			// the load algorithm set the error attribute to null
			dommedia->m_error = NULL;
			break;
		case 1:
			CALL_FAILED_IF_ERROR(media->Play(thread));
			break;
		case 2:
			CALL_FAILED_IF_ERROR(media->Pause(thread));
			break;
		}
	}

	return ES_FAILED;
}

/* virtual */ void
DOM_HTMLMediaElement::GCTraceSpecial(BOOL via_tree)
{
	DOM_Element::GCTraceSpecial(via_tree);
	GCMark(m_error);
	GCMark(m_tracks);
}

void
DOM_HTMLMediaElement::GCMarkPlaying()
{
	if (MediaElement *media = GetThisElement()->GetMediaElement())
	{
		if (media->GCIsPlaying())
			GCMark(this);
	}
}

ES_GetState
DOM_HTMLMediaElement::GetTimeRanges(OpAtom property_name, MediaElement *media, ES_Value *value)
{
	const OpMediaTimeRanges *ranges = NULL;
	OP_STATUS status = OpStatus::OK;

	switch (property_name)
	{
	case OP_ATOM_buffered:
		status = media->GetBufferedTimeRanges(ranges);
		break;
	case OP_ATOM_played:
		status = media->GetPlayedTimeRanges(ranges);
		break;
	case OP_ATOM_seekable:
		status = media->GetSeekableTimeRanges(ranges);
		break;
	}

	switch (status)
	{
	case OpStatus::OK:
		break;
	case OpStatus::ERR_NO_MEMORY:
		return GET_NO_MEMORY;
	default:
		// Create an empty 'TimeRanges' object for other errors.
		ranges = NULL;
	}

	DOM_TimeRanges *time_ranges;
	GET_FAILED_IF_ERROR(DOM_TimeRanges::Make(time_ranges, GetEnvironment(), ranges));
	DOMSetObject(value, time_ranges);

	return GET_SUCCESS;
}

/* virtual */ int
DOM_HTMLAudioElement_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	DOM_HTMLElement *audio;
	DOM_EnvironmentImpl *environment = GetEnvironment();
	CALL_FAILED_IF_ERROR(environment->ConstructDocumentNode());
	CALL_FAILED_IF_ERROR(DOM_HTMLElement::CreateElement(audio, static_cast<DOM_Node*>(environment->GetDocument()), UNI_L("audio")));
	OP_ASSERT(audio->IsA(DOM_TYPE_HTML_AUDIOELEMENT));

	audio->SetAttribute(ATTR_PRELOAD, NULL, NS_IDX_DEFAULT, UNI_L("auto"), 4, audio->HasCaseSensitiveNames(), origining_runtime);
	if (argc > 0 && argv[0].type == VALUE_STRING)
		audio->SetAttribute(ATTR_SRC, NULL, NS_IDX_DEFAULT, argv[0].value.string, argv[0].GetStringLength(), audio->HasCaseSensitiveNames(), origining_runtime);

	DOMSetObject(return_value, audio);
	return ES_VALUE;
}

/*virtual */ ES_GetState
DOM_HTMLVideoElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	HTML_Element *htm_elem = GetThisElement();

	MediaElement *media = htm_elem->GetMediaElement();

	switch (property_name)
	{
	case OP_ATOM_videoWidth:
		if (!media)
			return GET_FAILED;
		DOMSetNumber(value, media->GetVideoWidth());
		return GET_SUCCESS;

	case OP_ATOM_videoHeight:
		if (!media)
			return GET_FAILED;
		DOMSetNumber(value, media->GetVideoHeight());
		return GET_SUCCESS;

	default:
		break;
	}
	return DOM_HTMLMediaElement::GetName(property_name, value, origining_runtime);
}

#ifdef DOM_STREAM_API_SUPPORT
/* static */ DOM_LocalMediaStream*
DOM_HTMLVideoElement::GetDOMLocalMediaStream(ES_Value* value)
{
	if (value->type == VALUE_OBJECT)
	{
		DOM_HOSTOBJECT_SAFE(stream, value->value.object, DOM_TYPE_LOCALMEDIASTREAM, DOM_LocalMediaStream);
		return stream;
	}

	return NULL;
}
#endif // DOM_STREAM_API_SUPPORT

/* virtual */ ES_PutState
DOM_HTMLVideoElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_videoWidth:
	case OP_ATOM_videoHeight:
		return PUT_READ_ONLY;
#ifdef DOM_STREAM_API_SUPPORT
	case OP_ATOM_src:
		{
			MediaElement* media = this_element->GetMediaElement();
			DOM_LocalMediaStream* stream = GetDOMLocalMediaStream(value);
			if (stream)
			{
				if (stream->IsLive())
				{
					PUT_FAILED_IF_ERROR(stream->AssociateWithMediaElement(media));
					DOMSetString(value, UNI_L("about:streamurl"));
					return DOM_HTMLMediaElement::PutName(property_name, value, origining_runtime);
				}
				else
					return PUT_SUCCESS; // Fail silently, at least until there is a specification that says otherwise.
			}
			else
				media->ClearStreamResource();
		}
		break;
#endif // DOM_STREAM_API_SUPPORT
	default:
		break;
	}
	return DOM_HTMLMediaElement::PutName(property_name, value, origining_runtime);
}

/* static */ void
DOM_HTMLTrackElement::ConstructHTMLTrackElementObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	// readyState enumeration
	PutNumericConstantL(object, "NONE", TRACK_STATE_NONE, runtime);
	PutNumericConstantL(object, "LOADING", TRACK_STATE_LOADING, runtime);
	PutNumericConstantL(object, "LOADED", TRACK_STATE_LOADED, runtime);
	PutNumericConstantL(object, "ERROR", TRACK_STATE_ERROR, runtime);
};

/* virtual */ void
DOM_HTMLTrackElement::GCTraceSpecial(BOOL via_tree)
{
	DOM_Element::GCTraceSpecial(via_tree);
	GCMark(m_track);
}

/*virtual */ ES_GetState
DOM_HTMLTrackElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_kind:
		DOMSetString(value, MediaTrackKind(this_element->GetStringAttr(Markup::HA_KIND)).DOMValue());
		return GET_SUCCESS;

	case OP_ATOM_readyState:
		if (value)
		{
			int ready_state = TRACK_STATE_NONE;
			if (TrackElement* track_element = this_element->GetTrackElement())
				ready_state = track_element->GetReadyState();

			DOMSetNumber(value, ready_state);
		}
		return GET_SUCCESS;

	case OP_ATOM_track:
		if (value)
			if (TrackElement* track_element = this_element->GetTrackElement())
			{
				GET_FAILED_IF_ERROR(track_element->EnsureTrack(this_element));

				MediaTrack* track = track_element->GetTrack();
				if (!track->GetDOMObject())
				{
					OP_ASSERT(m_track == NULL);

					GET_FAILED_IF_ERROR(DOM_TextTrack::Make(m_track, GetEnvironment(), track));
					track->SetDOMObject(m_track);

					SetIsSignificant();
				}
				DOMSetObject(value, m_track);
			}
			else
				DOMSetNull(value);
		return GET_SUCCESS;

	default:
		break;
	}
	return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
}

/*virtual */ ES_PutState
DOM_HTMLTrackElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_track || property_name == OP_ATOM_readyState)
		return PUT_READ_ONLY;

	return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
}
#endif // MEDIA_HTML_SUPPORT

#ifdef CANVAS_SUPPORT

// DOM_HTMLCanvasElement

/* virtual */ ES_PutState
DOM_HTMLCanvasElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_width:
	case OP_ATOM_height:
	{
		ES_PutState result = DOM_HTMLElement::PutName(property_name, value, origining_runtime);
		if (result != PUT_SUCCESS)
			return result;
		RecordAllocationCost();
		return PUT_SUCCESS;
	}
	default:
		return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ void
DOM_HTMLCanvasElement::GCTraceSpecial(BOOL via_tree)
{
	DOM_Element::GCTraceSpecial(via_tree);
	RecordAllocationCost();

	GCMark(m_context2d);
#ifdef CANVAS3D_SUPPORT
	GCMark(m_contextwebgl);
#endif // CANVAS3D_SUPPORT
}

#ifdef CANVAS3D_SUPPORT
/* virtual */ ES_GetState
DOM_HTMLCanvasElement::GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (g_ecmaManager->IsDebugging(origining_runtime) && property_name[0] == 'o' && property_name[1] == 'n')
	{
		ES_GetState res = GetEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
		if (res != GET_FAILED)
			return res;
		// We use this for feature detection in DF. If being debugged onframeend is always defined.
		if (!uni_strcmp(UNI_L("onframeend"), property_name))
		{
			DOMSetNull(value);
			return GET_SUCCESS;
		}
	}
	return DOM_HTMLElement::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLCanvasElement::PutName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (g_ecmaManager->IsDebugging(origining_runtime) && property_name[0] == 'o' && property_name[1] == 'n')
	{
		ES_PutState res = PutEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
		if (res != PUT_FAILED)
			return res;
	}
	return DOM_HTMLElement::PutName(property_name, property_code, value, origining_runtime);
}

#define CANVAS_CONTEXTID_WEBGL UNI_L("experimental-webgl")
#define CANVAS_CONTEXTID_WEBGL_LENGTH 18
#endif // CANVAS3D_SUPPORT

/* static */ int
DOM_HTMLCanvasElement::getContext(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcanvas, DOM_TYPE_HTML_CANVASELEMENT, DOM_HTMLCanvasElement);
	const uni_char *type = NULL;
	unsigned type_length;
#ifdef CANVAS3D_SUPPORT
	DOMWebGLContextAttributes* attributes = NULL;
	if (ES_CONVERSION_COMPLETED(argc))
	{
		argc = ES_CONVERSION_ARGC(argc);
		OP_ASSERT(argc > 1 && argv[1].type == VALUE_OBJECT);
		ES_Object *dict = argv[1].value.object;
		CALL_FAILED_IF_ERROR(DOMWebGLContextAttributes::Make(attributes, origining_runtime));
		attributes->m_alpha = domcanvas->DOMGetDictionaryBoolean(dict, UNI_L("alpha"), attributes->m_alpha);
		attributes->m_depth = domcanvas->DOMGetDictionaryBoolean(dict, UNI_L("depth"), attributes->m_depth);
		attributes->m_stencil = domcanvas->DOMGetDictionaryBoolean(dict, UNI_L("stencil"), attributes->m_stencil);
		attributes->m_antialias = domcanvas->DOMGetDictionaryBoolean(dict, UNI_L("antialias"), attributes->m_antialias);
		attributes->m_premultipliedAlpha = domcanvas->DOMGetDictionaryBoolean(dict, UNI_L("premultipliedAlpha"), attributes->m_premultipliedAlpha);
		attributes->m_preserveDrawingBuffer = domcanvas->DOMGetDictionaryBoolean(dict, UNI_L("preserveDrawingBuffer"), attributes->m_preserveDrawingBuffer);
		type = CANVAS_CONTEXTID_WEBGL;
		type_length = CANVAS_CONTEXTID_WEBGL_LENGTH;
	}
	else
	{
		DOM_CHECK_ARGUMENTS("s");
		type = argv[0].value.string;
		type_length = argv[0].GetStringLength();
	}
#else
	DOM_CHECK_ARGUMENTS("s");
	type = argv[0].value.string;
	type_length = argv[0].GetStringLength();
#endif // CANVAS3D_SUPPORT

	HTML_Element *htm_elem = domcanvas->GetThisElement();

	Canvas* canvas = (Canvas*)htm_elem->GetSpecialAttr(Markup::VEGAA_CANVAS, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_OGP);

	if (!canvas)
		return ES_FAILED;
	if (!canvas->GetOpBitmap())
	{
		// Make sure the canvas has a bitmap
		CALL_FAILED_IF_ERROR(canvas->SetSize(canvas->GetWidth(htm_elem), canvas->GetHeight(htm_elem)));
	}

	if (uni_str_eq(type, "2d") && type_length == 2)
	{
#ifdef CANVAS3D_SUPPORT
		if (domcanvas->m_contextwebgl)
		{
			DOMSetNull(return_value);
			return ES_VALUE;
		}
#endif // CANVAS3D_SUPPORT
		if (!domcanvas->m_context2d)
		{
			CALL_FAILED_IF_ERROR(DOMCanvasContext2D::Make(domcanvas->m_context2d, domcanvas));
		}
		DOMSetObject(return_value, domcanvas->m_context2d);
		return ES_VALUE;
	}
#ifdef CANVAS3D_SUPPORT
	else if (uni_str_eq(type, CANVAS_CONTEXTID_WEBGL) && type_length == CANVAS_CONTEXTID_WEBGL_LENGTH)
	{
		if (domcanvas->m_context2d)
		{
			DOMSetNull(return_value);
			return ES_VALUE;
		}
		if (!domcanvas->m_contextwebgl)
		{
			// The webgl context attributes should be ignored if the webgl context is already created
			if (!attributes && argc > 1 && argv[1].type == VALUE_OBJECT)
			{
				DOM_Object* domo = DOM_Utils::GetDOM_Object(argv[1].value.object);
				if (!domo || !domo->IsA(DOM_TYPE_WEBGLCONTEXTATTRIBUTES))
				{
					ES_CONVERT_ARGUMENTS_AS(return_value, "-{alpha:b,depth:b,stencil:b,antialias:b,premultipliedAlpha:b,preserveDrawingBuffer:b}-");
					return ES_NEEDS_CONVERSION;
				}
				else
					attributes = static_cast<DOMWebGLContextAttributes *>(domo);
			}
			// Status will be OpStatus::ERR if webgl is disabled or if the creation of the WebGL context fails.
			// Return undefined in that case.
			CALL_FAILED_IF_ERROR(DOMCanvasContextWebGL::Make(domcanvas->m_contextwebgl, domcanvas, attributes));
		}
		DOMSetObject(return_value, domcanvas->m_contextwebgl);
		return ES_VALUE;
	}
#endif // CANVAS3D_SUPPORT

	DOMSetNull(return_value);
	return ES_VALUE;
}

#undef CANVAS_CONTEXTID_WEBGL
#undef CANVAS_CONTEXTID_WEBGL_LENGTH

/* static */ int
DOM_HTMLCanvasElement::toDataURL(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domcanvas, DOM_TYPE_HTML_CANVASELEMENT, DOM_HTMLCanvasElement);

#ifdef VEGA_CANVAS_JPG_DATA_URL
	const uni_char* type = NULL;
	int quality = 85;
	if (argc > 0)
	{
		DOM_CHECK_ARGUMENTS("s");
		type = argv[0].value.string;
		if (argc > 1 && argv[1].type == VALUE_NUMBER && argv[1].value.number >= 0. && argv[1].value.number <= 1.)
		{
			quality = (int)(100.*argv[1].value.number+0.5);
		}
	}
#endif // VEGA_CANVAS_JPG_DATA_URL

	HTML_Element* canvas_element = domcanvas->GetThisElement();
	Canvas* canvas = static_cast<Canvas*>(canvas_element->GetSpecialAttr(Markup::VEGAA_CANVAS, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_OGP));
	if (!canvas)
		return ES_FAILED;

	if (!canvas->IsSecure())
	{
		// Raise security exception
		return ES_EXCEPT_SECURITY;
	}

	// Make sure the canvas is up to date before using it's data
	domcanvas->Invalidate();

	// Make sure the canvas is backed.
	CALL_FAILED_IF_ERROR(canvas->Realize(canvas_element));

	TempBuffer *buffer = GetEmptyTempBuf();

	OpBitmap* bmp = canvas->GetOpBitmap();
	if (!bmp || bmp->Width() == 0 || bmp->Height() == 0)
	{
		// No backing bitmap, or zero-size
		CALL_FAILED_IF_ERROR(buffer->Append("data:,"));
	}
	else
	{
		OP_STATUS status;
#ifdef VEGA_CANVAS_JPG_DATA_URL
		if (type && uni_strlen(type) == 10 &&
			uni_strni_eq_lower_ascii(type, UNI_L("image/jpeg"), 10))
			status = canvas->GetJPGDataURL(buffer, quality);
		else
#endif // VEGA_CANVAS_JPG_DATA_URL
			status = canvas->GetPNGDataURL(buffer);

		CALL_FAILED_IF_ERROR(status);
	}

	DOMSetString(return_value, buffer);
	return ES_VALUE;
}

void DOM_HTMLCanvasElement::MakeInsecure()
{
	Canvas* canvas = (Canvas*)GetThisElement()->GetSpecialAttr(Markup::VEGAA_CANVAS, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_OGP);
	if (canvas)
		canvas->MakeInsecure();
}

class DOM_CanvasInvalidator : public ES_ThreadListener
{
public:
	OP_STATUS Init(DOM_HTMLCanvasElement* canvas)
	{
		if (!canvas->GetRuntime()->Protect(*canvas))
		{
			// Didn't manage to protect it. :-(
			return OpStatus::ERR_NO_MEMORY;
		}
		m_canvas = canvas;
		return OpStatus::OK;
	}
	virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal)
	{
		if (!m_canvas)
			return OpStatus::OK;

		switch (signal)
		{
		case ES_SIGNAL_CANCELLED:
		case ES_SIGNAL_FINISHED:
		case ES_SIGNAL_FAILED:
			{
				m_canvas->GetRuntime()->Unprotect(*m_canvas);
				m_canvas->Invalidate(TRUE);
			}
		}

		return OpStatus::OK;
	}
private:
	DOM_HTMLCanvasElement* m_canvas;
};

OP_STATUS DOM_HTMLCanvasElement::ScheduleInvalidation(ES_Runtime* origining_runtime)
{
	m_dirty = TRUE;
	if (m_pending_invalidation)
		return OpStatus::OK;

	ES_Thread* thread = GetCurrentThread(origining_runtime)->GetRunningRootThread();

	DOM_CanvasInvalidator* invalidate = OP_NEW(DOM_CanvasInvalidator, ());
	if (!invalidate || OpStatus::IsError(invalidate->Init(this)))
	{
		OP_DELETE(invalidate);
		return OpStatus::ERR_NO_MEMORY;
	}
	thread->AddListener(invalidate);
	m_pending_invalidation = TRUE;
	return OpStatus::OK;
}
void DOM_HTMLCanvasElement::Invalidate(BOOL threadFinished)
{
	Canvas* canvas = (Canvas*)GetThisElement()->GetSpecialAttr(Markup::VEGAA_CANVAS, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_OGP);
	if (canvas && m_dirty)
		canvas->invalidate(GetFramesDocument(), GetThisElement());
	if (threadFinished)
		m_pending_invalidation = FALSE;
	m_dirty = FALSE;
}

void DOM_HTMLCanvasElement::RecordAllocationCost()
{
	if (Canvas* canvas = this_element ? static_cast<Canvas *>(this_element->GetSpecialAttr(Markup::VEGAA_CANVAS, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_OGP)) : NULL)
		GetRuntime()->RecordExternalAllocation(Canvas::allocationCost(canvas));
}
#endif // CANVAS_SUPPORT

// DOM_HTMLMapElement

OP_STATUS
DOM_HTMLMapElement::InitAreaCollection()
{
	return DOM_initCollection(areas, this, this, AREAS, DOM_PRIVATE_areas);
}

/* virtual */ ES_GetState
DOM_HTMLMapElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_areas)
	{
		GET_FAILED_IF_ERROR(InitAreaCollection());
		DOMSetObject(value, areas);
		return GET_SUCCESS;
	}
	else
		return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLMapElement::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_areas)
		return PUT_READ_ONLY;
	else
		return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_HTMLMapElement::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	DOM_HTMLElement::DOMChangeRuntime(new_runtime);

	if (areas)
		areas->DOMChangeRuntime(new_runtime);
}

// DOM_HTMLScriptElement

/* virtual */ ES_GetState
DOM_HTMLScriptElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_text:
		if (value)
		{
			TempBuffer *buffer = GetEmptyTempBuf();

			if (this_element->GetUrlAttr(ATTR_SRC, NS_IDX_HTML, GetLogicalDocument()))
			{
				// Script element (source) contents aren't exposed to scripts, only exception
				// made is if the element is part of the UserJS events BeforeScript and AfterScript.
#ifdef USER_JAVASCRIPT
				OP_BOOLEAN result = DOM_UserJSEvent::GetScriptSource(origining_runtime, buffer);
				GET_FAILED_IF_ERROR(result);
				if (result == OpBoolean::IS_TRUE)
				{
					DOMSetString(value, buffer);
					return GET_SUCCESS;
				}
#endif // USER_JAVASCRIPT
			}

			GET_FAILED_IF_ERROR(this_element->DOMGetContents(GetEnvironment(), buffer, TRUE));

			DOMSetString(value, buffer);
		}
		return GET_SUCCESS;
	default:
		return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_HTMLScriptElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_text)
	{
#ifdef USER_JAVASCRIPT
		if (DOM_UserJSManager::IsActiveInRuntime(origining_runtime))
			// If user javascript is running, then change script contents
			// else act as if it was just textContent.
			if (!this_element->HasAttr(ATTR_SRC, NS_IDX_HTML))
				return SetTextContent(value, (DOM_Runtime *) origining_runtime, NULL);
			else if (value->type != VALUE_STRING)
				return PUT_NEEDS_STRING;
			else
			{
				DOM_EnvironmentImpl *environment = GetEnvironment();

				DOM_EnvironmentImpl::CurrentState state(environment, (DOM_Runtime *) origining_runtime);
				PUT_FAILED_IF_ERROR(this_element->DOMSetContents(environment, value->value.string));

				return PUT_SUCCESS;
			}
#endif // USER_JAVASCRIPT
		property_name = OP_ATOM_textContent;
	}

	return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
}

// DOM_HTMLTableElement

OP_STATUS
DOM_HTMLTableElement::InitRowsCollection()
{
	return DOM_initCollection(rows, this, this, TABLE_ROWS, DOM_PRIVATE_rows);
}

OP_STATUS
DOM_HTMLTableElement::InitCellsCollection()
{
	return DOM_initCollection(cells, this, this, TABLE_CELLS, DOM_PRIVATE_cells);
}

OP_STATUS
DOM_HTMLTableElement::InitTBodiesCollection()
{
	return DOM_initCollection(tBodies, this, this, TBODIES, DOM_PRIVATE_tbodies);
}

/* virtual */ ES_GetState
DOM_HTMLTableElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_caption:
	case OP_ATOM_tHead:
	case OP_ATOM_tFoot:
		if (value)
		{
			HTML_ElementType type;

			if (property_name == OP_ATOM_caption)
				type = HE_CAPTION;
			else if (property_name == OP_ATOM_tHead)
				type = HE_THEAD;
			else
				type = HE_TFOOT;

			DOM_HTMLElement *element;
			GET_FAILED_IF_ERROR(GetChildElement(element, type));
			DOMSetObject(value, element);
		}
		return GET_SUCCESS;

	case OP_ATOM_rows:
		if (value)
		{
			GET_FAILED_IF_ERROR(InitRowsCollection());
			DOMSetObject(value, GetRowsCollection());
		}
		return GET_SUCCESS;

	case OP_ATOM_cells:
		if (value)
		{
			// This is a MSIEism.
			GET_FAILED_IF_ERROR(InitCellsCollection());
			DOMSetObject(value, GetCellsCollection());
		}
		return GET_SUCCESS;

	case OP_ATOM_tBodies:
		if (value)
		{
			GET_FAILED_IF_ERROR(InitTBodiesCollection());
			DOMSetObject(value, GetTBodiesCollection());
		}
		return GET_SUCCESS;

	default:
		return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_HTMLTableElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
    case OP_ATOM_caption:
    case OP_ATOM_tHead:
    case OP_ATOM_tFoot:
	    if (value->type != VALUE_OBJECT)
			return DOM_PUTNAME_DOMEXCEPTION(HIERARCHY_REQUEST_ERR);
		else
			return PutChildElement(property_name, value, (DOM_Runtime *) origining_runtime, NULL);

	case OP_ATOM_rows:
	case OP_ATOM_tBodies:
		return PUT_READ_ONLY;

	default:
		return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_HTMLTableElement::PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object *restart_object)
{
	switch (property_name)
	{
    case OP_ATOM_caption:
    case OP_ATOM_tHead:
    case OP_ATOM_tFoot:
		return PutChildElement(property_name, value, (DOM_Runtime *) origining_runtime, restart_object);

	default:
		return DOM_HTMLElement::PutNameRestart(property_name, value, origining_runtime, restart_object);
	}
}

/* virtual */ void
DOM_HTMLTableElement::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	DOM_HTMLElement::DOMChangeRuntime(new_runtime);

	if (rows)
		rows->DOMChangeRuntime(new_runtime);
	if (cells)
		cells->DOMChangeRuntime(new_runtime);
	if (tBodies)
		tBodies->DOMChangeRuntime(new_runtime);
}

// DOM_HTMLTableSectionElement

OP_STATUS
DOM_HTMLTableSectionElement::InitRowsCollection()
{
	return DOM_initCollection(rows, this, this, TABLE_SECTION_ROWS, DOM_PRIVATE_rows);
}

/* virtual */ ES_GetState
DOM_HTMLTableSectionElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_rows)
	{
		GET_FAILED_IF_ERROR(InitRowsCollection());
		DOMSetObject(value, *GetRowsCollection());
		return GET_SUCCESS;
	}
	else
		return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
}


/* virtual */ ES_PutState
DOM_HTMLTableSectionElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_rows)
		return PUT_READ_ONLY;
	else
		return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_HTMLTableSectionElement::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	DOM_HTMLElement::DOMChangeRuntime(new_runtime);

	if (rows)
		rows->DOMChangeRuntime(new_runtime);
}

// DOM_HTMLTableRowElement

OP_STATUS
DOM_HTMLTableRowElement::InitCellsCollection()
{
	return DOM_initCollection(cells, this, this, TABLE_ROW_CELLS, DOM_PRIVATE_cells);
}

OP_STATUS
DOM_HTMLTableRowElement::GetTable(DOM_HTMLTableElement *&table)
{
	for (HTML_Element *element = this_element; element; element = element->ParentActual())
		if (element->IsMatchingType(HE_TABLE, NS_HTML))
		{
			DOM_Node *node;
			RETURN_IF_ERROR(GetEnvironment()->ConstructNode(node, element, owner_document));
			table = (DOM_HTMLTableElement *) node;
			return OpStatus::OK;
		}

	table = NULL;
	return OpStatus::OK;
}

OP_STATUS
DOM_HTMLTableRowElement::GetSection(DOM_HTMLTableSectionElement *&section)
{
	for (HTML_Element *element = this_element; element; element = element->ParentActual())
		if (element->IsMatchingType(HE_THEAD, NS_HTML) || element->IsMatchingType(HE_TFOOT, NS_HTML) || element->IsMatchingType(HE_TBODY, NS_HTML))
		{
			DOM_Node *node;
			RETURN_IF_ERROR(GetEnvironment()->ConstructNode(node, element, owner_document));
			section = (DOM_HTMLTableSectionElement *) node;
			return OpStatus::OK;
		}
		else if (element->IsMatchingType(HE_TABLE, NS_HTML))
			break;

	section = NULL;
	return OpStatus::OK;
}

OP_STATUS
DOM_HTMLTableRowElement::GetRowIndex(int &result, BOOL in_section)
{
	DOM_Collection *rows = NULL;

	if (in_section)
	{
		DOM_HTMLTableSectionElement *section;
		RETURN_IF_ERROR(GetSection(section));

		if (section)
		{
			RETURN_IF_ERROR(section->InitRowsCollection());
			rows = section->GetRowsCollection();
		}
	}

	if (!rows)
	{
		DOM_HTMLTableElement *table;
		RETURN_IF_ERROR(GetTable(table));

		if (!table)
		{
			result = -1; // not in a table
			return OpStatus::OK;
		}

		RETURN_IF_ERROR(table->InitRowsCollection());
		rows = table->GetRowsCollection();
	}

	OP_ASSERT(rows);
	for (int index = 0, length = rows->Length(); index < length; ++index)
		if (rows->Item(index) == this_element)
		{
			result = index;
			return OpStatus::OK;
		}

	result = -1;
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_HTMLTableRowElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_rowIndex:
	case OP_ATOM_sectionRowIndex:
		if (value)
		{
			int index;
			GET_FAILED_IF_ERROR(GetRowIndex(index, property_name == OP_ATOM_sectionRowIndex));
			DOMSetNumber(value, index);
		}
		return GET_SUCCESS;

	case OP_ATOM_cells:
		if (value)
		{
			GET_FAILED_IF_ERROR(InitCellsCollection());
			DOMSetObject(value, cells);
		}
		return GET_SUCCESS;

	default:
		return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_HTMLTableRowElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
    switch (property_name)
	{
	case OP_ATOM_cells:
	case OP_ATOM_rowIndex:
	case OP_ATOM_sectionRowIndex:
		return PUT_READ_ONLY;

	default:
		return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ void
DOM_HTMLTableRowElement::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	DOM_HTMLElement::DOMChangeRuntime(new_runtime);

	if (cells)
		cells->DOMChangeRuntime(new_runtime);
}

// DOM_HTMLTableCellElement

OP_STATUS
DOM_HTMLTableCellElement::GetCellIndex(int &result)
{
	/* If the cell is not part of a table row, the value should be -1. */
	result = -1;

	for (HTML_Element *element = this_element; element; element = element->ParentActual())
		if (element->IsMatchingType(HE_TR, NS_HTML))
		{
			DOM_Node *node;
			RETURN_IF_ERROR(GetEnvironment()->ConstructNode(node, element, owner_document));

			DOM_HTMLTableRowElement *row = static_cast<DOM_HTMLTableRowElement *>(node);
			RETURN_IF_ERROR(row->InitCellsCollection());

			DOM_Collection *cells = row->GetCellsCollection();
			for (int index = 0, length = cells->Length(); index < length; ++index)
				if (cells->Item(index) == this_element)
				{
					result = index;
					return OpStatus::OK;
				}
		}

    return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_HTMLTableCellElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_cellIndex)
	{
		if (value)
		{
			int index;
			GET_FAILED_IF_ERROR(GetCellIndex(index));
			DOMSetNumber(value, index);
		}
		return GET_SUCCESS;
	}
	else
		return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLTableCellElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
    if (property_name == OP_ATOM_cellIndex)
		return PUT_READ_ONLY;
	else
		return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
}

// DOM_HTMLFrameElement

/* virtual */ BOOL
DOM_HTMLFrameElement::SecurityCheck(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (DOM_Object::SecurityCheck(property_name, value, origining_runtime))
		return TRUE;

	/* Always allow reading of contentWindow and contentDocument (readable anyway as contentWindow.document). */
	if (value == NULL && (property_name == OP_ATOM_contentWindow || property_name == OP_ATOM_contentDocument))
		return TRUE;

	return FALSE;
}

/* virtual */ ES_GetState
DOM_HTMLFrameElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_contentDocument:
		return GetFrameEnvironment(value, FRAME_DOCUMENT, (DOM_Runtime *) origining_runtime);

	case OP_ATOM_contentWindow:
		return GetFrameEnvironment(value, FRAME_WINDOW, (DOM_Runtime *) origining_runtime);
	}

	return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLFrameElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_contentDocument:
	case OP_ATOM_contentWindow:
		return PUT_READ_ONLY;
	}

	return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
}

// DOM_HTMLTimeElement

/* virtual */ ES_GetState
DOM_HTMLTimeElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_valueAsDate)
	{
		if (value)
		{
			const uni_char* date_time_string = GetThisElement()->GetStringAttr(ATTR_DATETIME);
			BOOL in_content = !date_time_string;
			if (!date_time_string)
			{
				ES_GetState state = GetTextContent(value);
				if (state != GET_SUCCESS)
					return state;
				OP_ASSERT(value->type == VALUE_STRING);
				OP_ASSERT(value->value.string);
				date_time_string = value->value.string;
			}

			// http://www.whatwg.org/specs/web-apps/current-work/multipage/common-microsyntaxes.html#parse-a-date-or-time-string

			while (in_content && uni_isspace(date_time_string[0]))
				date_time_string++;

			DateTimeSpec date_time;
			date_time.Clear();
			unsigned int parse_length = date_time.SetFromISO8601String(date_time_string, TRUE, TRUE);
			if (parse_length)
				date_time_string += parse_length;
			else
			{
				date_time.Clear();

				// Try as date:
				if (date_time.m_date.SetFromISO8601String(date_time_string, TRUE))
					date_time_string += date_time.m_date.GetISO8601StringLength();
				else
				{
					date_time.m_date.m_year = 1970;
					date_time.m_date.m_month = 0;
					date_time.m_date.m_day = 1;

					// Try as time:
					parse_length = date_time.m_time.SetFromISO8601String(date_time_string, TRUE);
					if (parse_length)
						date_time_string += parse_length;
					else
					{
						// Neither datetime, date or time.
						DOMSetNull(value);
						return GET_SUCCESS;
					}
				}
			}

			while (in_content && uni_isspace(date_time_string[0]))
				date_time_string++;

			if (date_time_string[0] != '\0')
			{
				DOMSetNull(value);
				return GET_SUCCESS;
			}

			ES_Value date_value;
			DOMSetNumber(&date_value, date_time.AsDouble());
			ES_Object* date_obj;
			GET_FAILED_IF_ERROR(
					GetRuntime()->CreateNativeObject(date_value,
					ENGINE_DATE_PROTOTYPE,
					&date_obj));
			DOMSetObject(value, date_obj);
		}
		return GET_SUCCESS;
	}
	else
		return DOM_HTMLElement::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLTimeElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_valueAsDate)
		return PUT_SUCCESS; // According to spec, no exception should be thrown; Fail silently instead.
	else
		return DOM_HTMLElement::PutName(property_name, value, origining_runtime);
}

