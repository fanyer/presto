/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_DOM

/* Temporary: SVG_TINY_12 might be defined by dominternaltypes.h. */
#include "modules/dom/src/dominternaltypes.h"

#ifdef SVG_TINY_12
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/domsvg/domsvgenum.h"
#include "modules/dom/src/domsvg/domsvgelement.h"
#include "modules/dom/src/domsvg/domsvgobjectstore.h"
#include "modules/dom/src/domsvg/domsvgelementinstance.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/js/window.h"

#include "modules/logdoc/htm_lex.h"
#include "modules/logdoc/htm_elm.h"

#include "modules/svg/SVGManager.h"
#include "modules/svg/svg.h"

/* static */ int
DOM_SVGElement::getObjectTrait(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);
	HTML_Element *element = elm->GetThisElement();
	DOM_Environment* environment = elm->GetEnvironment();

	int ns_idx = NS_IDX_DEFAULT;
	Markup::AttrType attr = ATTR_XML;
	const uni_char* name = NULL;

	if(data == 1 || data == 8)
	{
		DOM_CHECK_ARGUMENTS("Ss");
		name = argv[1].value.string;
		ns_idx = argv[0].type == VALUE_STRING ? element->DOMGetNamespaceIndex(environment, argv[0].value.string) : NS_IDX_DEFAULT;
		attr = HTM_Lex::GetAttrType(name, g_ns_manager->GetNsTypeAt(element->ResolveNsIdx(ns_idx)));
	}
	else
	{
		DOM_CHECK_ARGUMENTS("s");
		name = argv[0].value.string;
		attr = HTM_Lex::GetAttrType(name, g_ns_manager->GetNsTypeAt(element->ResolveNsIdx(ns_idx)));
	}

	OP_STATUS err = OpStatus::OK;
	SVGDOMItemType type = SVG_DOM_ITEMTYPE_UNKNOWN;
	switch(data)
	{
		case 0: // getTrait
		case 1: // getTraitNS
		case 7:	// getPresentationTrait
		case 8: // getPresentationTraitNS
			{
				if(uni_str_eq(name, "#text"))
				{
					return elm->GetTextContent(return_value);
				}
				else
				{
					TempBuffer *buffer = DOM_Object::GetEmptyTempBuf();
					err = SVGDOM::GetTrait(environment, element, attr, name, ns_idx, (data == 7 || data == 8),
										   type, NULL, buffer);

					if(OpStatus::IsSuccess(err))
					{
						DOMSetString(return_value, buffer);
						return ES_VALUE;
					}
				}
			}
			break;
		case 2: // getFloatTrait
		case 9: // getFloatPresentationTrait
			{
				double val;
				err = SVGDOM::GetTrait(environment, element, attr, name, ns_idx, (data == 9), type, NULL, NULL, &val);
				if(OpStatus::IsSuccess(err))
				{
					DOMSetNumber(return_value, val);
					return ES_VALUE;
				}
			}
			break;
		case 3: // getMatrixTrait
		case 10: // getMatrixPresentationTrait
			type = SVG_DOM_ITEMTYPE_MATRIX;
			break;
		case 4: // getRectTrait
		case 11: // getRectPresentationTrait
			type = SVG_DOM_ITEMTYPE_RECT;
			break;
		case 5: // getPathTrait
		case 12: // getPathPresentationTrait
			type = SVG_DOM_ITEMTYPE_PATH;
			break;
		case 6: // getRGBColorTrait
		case 13: // getRGBColorPresentationTrait
			type = SVG_DOM_ITEMTYPE_RGBCOLOR;
			break;
	}

	if(type != SVG_DOM_ITEMTYPE_UNKNOWN)
	{
		SVGDOMItem* svgobj;
		err = SVGDOM::GetTrait(environment, element, attr, name, ns_idx, (data > 9), type, &svgobj);
		if(OpStatus::IsSuccess(err))
		{
			DOM_SVGObject* domobj;
			err = DOM_SVGObject::Make(domobj, svgobj, DOM_SVGLocation(),
									origining_runtime->GetEnvironment());

			if(OpStatus::IsSuccess(err))
			{
				DOMSetObject(return_value, domobj);
				return ES_VALUE;
			}
			else
			{
				OP_DELETE(svgobj);

				if(OpStatus::IsMemoryError(err))
					return ES_NO_MEMORY;
			}
		}
	}

	if(OpStatus::ERR_NOT_SUPPORTED == err)
	{
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
	}
	else if(OpStatus::ERR == err)
	{
		return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
	}
	else if(OpStatus::ERR_NULL_POINTER == err)
	{
		return DOM_CALL_DOMEXCEPTION(INVALID_ACCESS_ERR);
	}

	return ES_FAILED;
}

#ifdef DOM2_MUTATION_EVENTS
class DOM_SVGElement_setObjectTrait_State
	: public DOM_Object
{
public:
	OpString value;
	ES_Value restart_object;

	virtual void GCTrace()
	{
		GCMark(restart_object);
	}
};
#endif // DOM2_MUTATION_EVENTS

/* static */ int
DOM_SVGElement::setObjectTrait(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);
	HTML_Element *element = elm->GetThisElement();
	DOM_Environment* environment = elm->GetEnvironment();

#ifdef DOM2_MUTATION_EVENTS
	if (argc < 0)
	{
		OP_ASSERT(return_value->type == VALUE_OBJECT);

		DOM_SVGElement_setObjectTrait_State *state = DOM_VALUE2OBJECT(*return_value, DOM_SVGElement_setObjectTrait_State);

		DOMSetString(return_value, state->value.CStr());

		ES_PutState result = elm->SetTextContent(return_value, origining_runtime, state->restart_object.value.object);

		if (result == PUT_SUSPEND)
		{
			state->restart_object = *return_value;

			DOMSetObject(return_value, state);
			return ES_SUSPEND | ES_RESTART;
		}

		return ConvertPutNameToCall(result, return_value);
	}
#endif // DOM2_MUTATION_EVENTS

	Markup::AttrType attr = ATTR_XML;
	int ns_idx = NS_IDX_DEFAULT;
	SVGDOMItemType type = SVG_DOM_ITEMTYPE_UNKNOWN;
	const uni_char* name = NULL;

	OP_STATUS err = OpStatus::OK;
	switch(data)
	{
		case 0: // setTrait
		case 1: // setTraitNS
			{
				const uni_char* value;
				if(data == 0)
				{
					DOM_CHECK_ARGUMENTS("ss");
					name = argv[0].value.string;
					attr = HTM_Lex::GetAttrType(name, g_ns_manager->GetNsTypeAt(element->ResolveNsIdx(ns_idx)));
					value = argv[1].value.string;
				}
				else
				{
					DOM_CHECK_ARGUMENTS("Sss");
					name = argv[1].value.string;
					value = argv[2].value.string;
					ns_idx = argv[0].type == VALUE_STRING ? element->DOMGetNamespaceIndex(environment, argv[0].value.string) : NS_IDX_DEFAULT;
					attr = HTM_Lex::GetAttrType(name, g_ns_manager->GetNsTypeAt(element->ResolveNsIdx(ns_idx)));
				}

				if(uni_str_eq(name, "#text"))
				{
					ES_Value *value = &argv[(data == 0 ? 1 : 2)];
					*return_value = *value;

					ES_PutState result = elm->SetTextContent(return_value, origining_runtime);

#ifdef DOM2_MUTATION_EVENTS
					if (result == PUT_SUSPEND)
					{
						DOM_SVGElement_setObjectTrait_State *state;

						CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(state = OP_NEW(DOM_SVGElement_setObjectTrait_State, ()), elm->GetRuntime()));
						CALL_FAILED_IF_ERROR(state->value.Set(value->value.string));
						state->restart_object = *return_value;

						DOMSetObject(return_value, state);
						return ES_SUSPEND | ES_RESTART;
					}
#else // DOM2_MUTATION_EVENTS
					OP_ASSERT(result != PUT_SUSPEND);
#endif // DOM2_MUTATION_EVENTS

					return ConvertPutNameToCall(result, return_value);
				}
				else
				{
					err = SVGDOM::SetTrait(environment, element, attr, name, ns_idx, value);
				}
			}
			break;

		case 2: // setFloatTrait
			{
				DOM_CHECK_ARGUMENTS("sN");
				attr = HTM_Lex::GetAttrType(argv[0].value.string, g_ns_manager->GetNsTypeAt(element->ResolveNsIdx(ns_idx)));

				if(attr == Markup::HA_XML)
					return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
				if(op_isnan(argv[1].value.number) || op_isinf(argv[1].value.number))
					return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);

				err = SVGDOM::SetTrait(environment, element, attr, name, ns_idx, NULL, NULL, &argv[1].value.number);
			}
			break;

		case 3: // setMatrixTrait
			{
				type = SVG_DOM_ITEMTYPE_MATRIX;
			}
			break;
		case 4: // setRectTrait
			{
				type = SVG_DOM_ITEMTYPE_RECT;
			}
			break;
		case 5: // setPathTrait
			{
				type = SVG_DOM_ITEMTYPE_PATH;
			}
			break;
		case 6: // setRGBColorTrait
			{
				type = SVG_DOM_ITEMTYPE_RGBCOLOR;
			}
			break;
	}

	if(type != SVG_DOM_ITEMTYPE_UNKNOWN)
	{
		DOM_CHECK_ARGUMENTS("s-");

		if(argv[1].type == VALUE_OBJECT)
		{
			name = argv[0].value.string;
			attr = HTM_Lex::GetAttrType(name, g_ns_manager->GetNsTypeAt(element->ResolveNsIdx(ns_idx)));
			DOM_ARGUMENT_OBJECT(domobj, 1, DOM_TYPE_SVG_OBJECT, DOM_SVGObject);
			if(domobj)
			{
				SVGDOMItem* svgdomitem = domobj->GetSVGDOMItem();
				if(svgdomitem->IsA(type))
					err = SVGDOM::SetTrait(environment, element, attr, name, ns_idx, NULL, svgdomitem);
				else
					err = OpStatus::ERR; // type mismatch err
			}
		}
		else if(argv[1].type == VALUE_NULL)
		{
			err = OpStatus::ERR_NULL_POINTER;
		}
		else
		{
			err = OpStatus::ERR;
		}
	}

	if(OpStatus::ERR_NOT_SUPPORTED == err)
	{
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
	}
	else if(OpStatus::ERR == err)
	{
		return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);
	}
	else if(OpStatus::ERR_NULL_POINTER == err)
	{
		return DOM_CALL_DOMEXCEPTION(INVALID_ACCESS_ERR);
	}

	return ES_FAILED;
}
#endif // SVG_TINY_12
#endif // SVG_DOM
