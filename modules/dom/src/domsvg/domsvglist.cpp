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
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domsvg/domsvgobject.h"
#include "modules/dom/src/domsvg/domsvglist.h"
#include "modules/dom/domutils.h"

#include "modules/svg/svg_dominterfaces.h"

#include "modules/doc/frm_doc.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/htm_ldoc.h"

#include "modules/url/url2.h"

#include "modules/svg/svg.h"

/** DOM_SVGList
 *
 * This file is currently in a mess, or a transient state as one could
 * call it.
 */

/* static */ void
DOM_Utils::ReleaseSVGDOMObjectFromLists(DOM_Object* obj)
{
	if (obj && obj->IsA(DOM_TYPE_SVG_OBJECT))
		static_cast<DOM_SVGObject*>(obj)->Release();
}

# ifdef SVG_FULL_11
/* static */ OP_STATUS
DOM_SVGList::Make(DOM_SVGList *&dom_list, SVGDOMList* svg_list, DOM_SVGLocation location, DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	const char* name = svg_list->GetDOMName();

	/* All SVG*List objects except the string list share
	 * prototype. */
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(dom_list = OP_NEW(DOM_SVGList, ()), runtime,
													runtime->GetPrototype(DOM_Runtime::SVGLIST_PROTOTYPE), name));
	OP_ASSERT(svg_list != NULL);
	dom_list->svg_list = svg_list;
	dom_list->location = location;

	return OpStatus::OK;
}

/* virtual */
DOM_SVGList::~DOM_SVGList()
{
	UINT32 count = svg_list->GetCount();
 	for (UINT32 i=0;i<count;++i)
 	{
 		DOM_SVGObject* dom_obj = static_cast<DOM_SVGObject*>(svg_list->GetDOMObject(i));
		if (dom_obj)
			dom_obj->Release();
 	}
	OP_DELETE(svg_list);
}

OP_STATUS
DOM_SVGList::AddObject(DOM_SVGObject* obj, SVGDOMItem* item /* = NULL */)
{
	if (!item)
		item = obj->GetSVGDOMItem();
	obj->SetInList(this);
	if (OpStatus::IsMemoryError(svg_list->SetDOMObject(item, obj)))
		return ES_NO_MEMORY;
	return OpStatus::OK;
}

/* Look through object cache and see if the object exist
 * there. Remove it if it does. */
OP_STATUS
DOM_SVGList::RemoveObject(DOM_SVGObject* obj, UINT32& idx)
{
 	idx = (UINT32)-1;
	UINT32 count = svg_list->GetCount();
 	for (UINT32 i = 0;i<count;++i)
 	{
 		DOM_Object* dom_obj = svg_list->GetDOMObject(i);
 		if (dom_obj == obj)
 		{
 			RETURN_IF_ERROR(svg_list->RemoveItem(i));
 			obj->Release();
 			idx = i;
 			break;
 		}
 	}

	// We this assert triggers, we tried remove an object not member
	// of this list.
	OP_ASSERT(idx != (UINT32)-1);
	return OpStatus::OK;
}

void
DOM_SVGList::ReleaseObject(DOM_SVGObject* obj)
{
	svg_list->ReleaseDOMObject(obj->GetSVGDOMItem());
}

OP_STATUS
DOM_SVGList::InsertObject(DOM_SVGObject* obj, UINT32 index, FramesDocument* frm_doc)
{
	SVGDOMItem* svg_item = obj->GetSVGDOMItem();

	/* Type-check */
	if (!svg_item->IsA(svg_list->ListType()))
		return OpStatus::ERR_OUT_OF_RANGE; // This means type error

	/* Remove item from previous list, and set it this list. We do
	 * this before the range-check because if may affect the length of
	 * this list. */
	DOM_SVGList* in_list = obj->InList();
	if (in_list != NULL)
	{
		UINT32 oldidx;
		CALL_FAILED_IF_ERROR(in_list->RemoveObject(obj, oldidx));

		/* Adjust idx if this list has been affected */
		if (in_list == this && oldidx < index)
			index--;
		in_list->location.Invalidate();
	}

	/* Range-check: If the index is equal to 0, then the new item is
	 * inserted at the front of the list. If the index is greater than
	 * or equal to numberOfItems, then the new item is appended to the
	 * end of the list. */
	if (index > svg_list->GetCount())
		index = svg_list->GetCount();

	OP_BOOLEAN status;
	if (index < svg_list->GetCount())
	{
		status = svg_list->InsertItemBefore(svg_item, index);
	}
	else
	{
		OP_ASSERT(index == svg_list->GetCount());
		status = svg_list->ApplyChange(index, svg_item);
	}

	if (OpBoolean::IS_TRUE == status)
	{
		if (OpStatus::IsMemoryError(AddObject(obj, svg_item)))
			return ES_NO_MEMORY;

		/* Request a repaint of the SVG-region */
		location.Invalidate();

		return OpStatus::OK;
	}
	else if (OpStatus::IsMemoryError(status))
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		// Unknown, unpredicted error
		return OpStatus::ERR;
	}
}

/* virtual */ ES_GetState
DOM_SVGList::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_numberOfItems || property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, svg_list->GetCount());
		return GET_SUCCESS;
	}

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* static */ int
DOM_SVGList::clear(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(list, DOM_TYPE_SVG_LIST, DOM_SVGList);
	list->GetSVGList()->Clear();
	list->location.Invalidate();
	return ES_FAILED;
}

/* static */ int
DOM_SVGList::initialize(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("o");
	DOM_THIS_OBJECT(list, DOM_TYPE_SVG_LIST, DOM_SVGList);
	DOM_ARGUMENT_OBJECT(dom_svg_obj, 0, DOM_TYPE_SVG_OBJECT, DOM_SVGObject);

	/* Type-check */
	if (!dom_svg_obj->GetSVGDOMItem()->IsA(list->svg_list->ListType()))
		return DOM_CALL_SVGEXCEPTION(SVG_WRONG_TYPE_ERR);

	{
		DOM_SVGList* in_list = dom_svg_obj->InList();
		if (in_list != NULL)
		{
			UINT32 dummy;
			CALL_FAILED_IF_ERROR(in_list->RemoveObject(dom_svg_obj, dummy));
			in_list->location.Invalidate();
		}
	}

	list->svg_list->Clear();

	SVGDOMItem* item = dom_svg_obj->GetSVGDOMItem();
	OP_ASSERT(list->svg_list->GetCount() == 0);
	OP_BOOLEAN status = list->svg_list->ApplyChange(0, item);
	if (OpBoolean::IS_TRUE == status)
	{
		if (OpStatus::IsMemoryError(list->AddObject(dom_svg_obj, item)))
			return ES_NO_MEMORY;
		list->location.Invalidate();
		DOMSetObject(return_value, dom_svg_obj);
		return ES_VALUE;
	}
	else if (OpStatus::IsMemoryError(status))
	{
		return ES_NO_MEMORY;
	}
	else
	{
		return ES_FAILED;
	}
}

/* virtual */ void
DOM_SVGList::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	for (UINT32 i=0;i<svg_list->GetCount();++i)
	{
		DOM_SVGObject* obj = static_cast<DOM_SVGObject*>(svg_list->GetDOMObject(i));
		if (obj)
			obj->DOMChangeRuntime(new_runtime);
	}
}

/* virtual */ void
DOM_SVGList::GCTrace()
{
	for (UINT32 i=0;i<svg_list->GetCount();++i)
	{
		DOM_SVGObject* obj = static_cast<DOM_SVGObject*>(svg_list->GetDOMObject(i));
		if (obj && obj->GetIsSignificant())
			GCMark(obj);
	}
	location.GCTrace();
}

/* static */ int
DOM_SVGList::getItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("n");
	DOM_THIS_OBJECT(list, DOM_TYPE_SVG_LIST, DOM_SVGList);

	unsigned idx = static_cast<unsigned>(argv[0].value.number);

	/* Range-check */
	if (idx >= list->svg_list->GetCount())
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	RETURN_GETNAME_AS_CALL(list->GetItemAtIndex(idx, return_value, origining_runtime));
}

ES_GetState DOM_SVGList::GetItemAtIndex(UINT32 idx, ES_Value* value, DOM_Runtime* origining_runtime)
{
	DOM_SVGObject* obj = static_cast<DOM_SVGObject*>(svg_list->GetDOMObject(idx));
 	if (!obj)
 	{
		SVGDOMItem* item;
		OP_STATUS status = svg_list->CreateItem(idx, item);
		if (OpStatus::IsSuccess(status))
		{
			GET_FAILED_IF_ERROR(DOM_SVGObject::Make(obj, item, location,
													origining_runtime->GetEnvironment()));
			if (OpStatus::IsMemoryError(AddObject(obj, item)))
				return GET_NO_MEMORY;
		}
		else if (OpStatus::IsMemoryError(status))
			return GET_NO_MEMORY;
 	}

	if (!obj)
		return GET_FAILED;

	DOMSetObject(value, obj);
	return GET_SUCCESS;
}

/* virtual */ ES_GetState
DOM_SVGList::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	unsigned idx = static_cast<unsigned>(property_index);

	if (idx >= svg_list->GetCount())
		return GET_FAILED;

	if (!value)
		return GET_SUCCESS;

	return GetItemAtIndex(idx, value, static_cast<DOM_Runtime*>(origining_runtime));
}

/* virtual */ ES_GetState
DOM_SVGList::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = svg_list->GetCount();
	return GET_SUCCESS;
}

/* static */ int
DOM_SVGList::insertItemBefore(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("on");
	DOM_THIS_OBJECT(list, DOM_TYPE_SVG_LIST, DOM_SVGList);
	DOM_ARGUMENT_OBJECT(dom_svg_obj, 0, DOM_TYPE_SVG_OBJECT, DOM_SVGObject);

	OP_STATUS status = list->InsertObject(dom_svg_obj,
										  static_cast<unsigned>(argv[1].value.number),
										  origining_runtime->GetEnvironment()->GetFramesDocument());
	if (OpStatus::IsSuccess(status))
	{
		/* Return inserted item */
		DOMSetObject(return_value, dom_svg_obj);
		return ES_VALUE;
	}
	else if (status == OpStatus::ERR_OUT_OF_RANGE)
	{
		return DOM_CALL_SVGEXCEPTION(SVG_WRONG_TYPE_ERR);
	}
	else if (OpStatus::IsMemoryError(status))
	{
		return ES_NO_MEMORY;
	}
	else
	{
		return ES_FAILED;
	}
}

/* static */ int
DOM_SVGList::replaceItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("on");
	DOM_THIS_OBJECT(list, DOM_TYPE_SVG_LIST, DOM_SVGList);
	DOM_ARGUMENT_OBJECT(dom_svg_obj, 0, DOM_TYPE_SVG_OBJECT, DOM_SVGObject);

	unsigned idx = static_cast<unsigned>(argv[1].value.number);

	/* Remove inserted item from previous list */
	DOM_SVGList* in_list = dom_svg_obj->InList();
	if (in_list != NULL)
	{
		UINT32 oldidx;
		CALL_FAILED_IF_ERROR(in_list->RemoveObject(dom_svg_obj, oldidx));

		/* Adjust idx if this list has been affected */
		if (in_list == list && oldidx < idx)
			idx--;
		in_list->location.Invalidate();
	}

	/* Range-check */
	if (idx >= list->svg_list->GetCount())
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	/* Type-check */
	if (!dom_svg_obj->GetSVGDOMItem()->IsA(list->svg_list->ListType()))
		return DOM_CALL_SVGEXCEPTION(SVG_WRONG_TYPE_ERR);

	/* Free replaced item */
	DOM_SVGObject* removed_dom_object = static_cast<DOM_SVGObject*>(list->svg_list->GetDOMObject(idx));
	if (removed_dom_object)
		removed_dom_object->Release();

	SVGDOMItem* new_item = dom_svg_obj->GetSVGDOMItem();
	OP_ASSERT(idx < list->svg_list->GetCount());
	/* Apply change to the SVG side */
	OP_BOOLEAN status = list->svg_list->ApplyChange(idx, new_item);
	if (OpBoolean::IS_TRUE == status)
	{
		if (OpStatus::IsMemoryError(list->AddObject(dom_svg_obj, new_item)))
			return ES_NO_MEMORY;
		list->location.Invalidate();
		DOMSetObject(return_value, dom_svg_obj);
		return ES_VALUE;
	}
	else if (OpStatus::IsMemoryError(status))
	{
		return ES_NO_MEMORY;
	}
	else
	{
		return ES_FAILED;
	}
}

/* static */ int
DOM_SVGList::removeItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("n");
	DOM_THIS_OBJECT(list, DOM_TYPE_SVG_LIST, DOM_SVGList);

	/* Range-check */
	if (argv[0].value.number < 0 ||
		argv[0].value.number >= list->svg_list->GetCount())
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	unsigned idx = static_cast<unsigned>(argv[0].value.number);

	DOM_SVGObject* obj = static_cast<DOM_SVGObject*>(list->svg_list->GetDOMObject(idx));
	if (!obj)
	{
		SVGDOMItem* item = NULL;
		OP_BOOLEAN status = list->svg_list->CreateItem(idx, item);
		if (OpStatus::IsSuccess(status))
		{
			if (OpStatus::IsMemoryError(DOM_SVGObject::Make(obj, item, DOM_SVGLocation(),
															origining_runtime->GetEnvironment())))
				return ES_NO_MEMORY;
		}
		else
		{
			if (OpStatus::IsMemoryError(status))
				return ES_NO_MEMORY;

			OP_ASSERT(!"Did GetItem fail for index in-bounds or why did this happen?");
			return ES_FAILED;
		}
	}
	else
	{
		OP_ASSERT(obj->GetSVGDOMItem() != NULL);
	}

	OP_ASSERT(obj != NULL);
	if (OpStatus::IsMemoryError(list->svg_list->RemoveItem(idx)))
		return ES_NO_MEMORY;

	obj->Release();
	list->location.Invalidate();

	DOMSetObject(return_value, obj);
	return ES_VALUE;
}

/* static */ int
DOM_SVGList::appendItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("o");
	DOM_THIS_OBJECT(list, DOM_TYPE_SVG_LIST, DOM_SVGList);
	DOM_ARGUMENT_OBJECT(dom_svg_obj, 0, DOM_TYPE_SVG_OBJECT, DOM_SVGObject);

	OP_STATUS status = list->InsertObject(dom_svg_obj,
										  list->svg_list->GetCount(),
										  origining_runtime->GetEnvironment()->GetFramesDocument());
	if (OpStatus::IsSuccess(status))
	{
		/* Return inserted item */
		DOMSetObject(return_value, dom_svg_obj);
		return ES_VALUE;
	}
	else if (status == OpStatus::ERR_OUT_OF_RANGE)
	{
		return DOM_CALL_SVGEXCEPTION(SVG_WRONG_TYPE_ERR);
	}
	else if (OpStatus::IsMemoryError(status))
	{
		return ES_NO_MEMORY;
	}
	else
	{
		return ES_FAILED;
	}
}

/* static */ int
DOM_SVGList::createSVGTransformFromMatrix(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("o");
	DOM_THIS_OBJECT(list, DOM_TYPE_SVG_LIST, DOM_SVGList);
	DOM_SVG_ARGUMENT_OBJECT(matrix, 0, SVG_DOM_ITEMTYPE_MATRIX, SVGDOMMatrix);

	/* createSVGTransformFromMatrix() only availible on transform lists */
	if (list->svg_list->ListType() != SVG_DOM_ITEMTYPE_TRANSFORM)
		return ES_FAILED;

	SVGDOMItem* dom_item;
	CALL_FAILED_IF_ERROR(SVGDOM::CreateSVGDOMItem(SVG_DOM_ITEMTYPE_TRANSFORM, dom_item));
	SVGDOMTransform* transform = static_cast<SVGDOMTransform*>(dom_item);

	transform->SetMatrix(matrix);

	DOM_SVGObject *obj;
	CALL_FAILED_IF_ERROR(DOM_SVGObject::Make(obj, transform, DOM_SVGLocation(),
											 origining_runtime->GetEnvironment()));

	DOM_Object::DOMSetObject(return_value, obj);
	return ES_VALUE;
}

/* static */ int
DOM_SVGList::consolidate(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(list, DOM_TYPE_SVG_LIST, DOM_SVGList);

	/* consolidate() only availible on transform lists */
	if (list->svg_list->ListType() != SVG_DOM_ITEMTYPE_TRANSFORM)
		return ES_FAILED;

	UINT32 item_count = list->svg_list->GetCount();
	for (UINT32 i=1;i<item_count;i++)
	{
		DOM_SVGObject* obj = static_cast<DOM_SVGObject*>(list->svg_list->GetDOMObject(i));
		if (obj != NULL)
		{
			obj->Release();
		}
	}

	OP_BOOLEAN status = list->svg_list->Consolidate();

	if (OpStatus::IsMemoryError(status))
		return ES_NO_MEMORY;

	if (OpBoolean::IS_TRUE == status)
	{
		/* Request a repaint of the SVG-region */
		list->location.Invalidate();

		DOM_SVGObject* obj = NULL;
		if (list->svg_list->GetCount() > 0)
		{
			SVGDOMItem* item;
			OP_STATUS status = list->svg_list->CreateItem(0, item);
			if(OpStatus::IsMemoryError(status))
				return ES_NO_MEMORY;
			if (OpStatus::IsSuccess(status))
			{
				CALL_FAILED_IF_ERROR(DOM_SVGObject::Make(obj, item, list->location,
														 origining_runtime->GetEnvironment()));

				if (OpStatus::IsMemoryError(list->AddObject(obj, item)))
					return ES_NO_MEMORY;
			}
		}

		if (obj)
			DOMSetObject(return_value, obj);
		else
			DOMSetNull(return_value);
		return ES_VALUE;
	}

	return ES_FAILED;
}

/* virtual */
DOM_SVGStringList::~DOM_SVGStringList()
{
	OP_DELETE(svg_list);
}

/* static */ OP_STATUS
DOM_SVGStringList::Make(DOM_SVGStringList *&list, SVGDOMStringList* svg_list, DOM_SVGLocation location,
						DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(list = OP_NEW(DOM_SVGStringList, ()),
													runtime,
													runtime->GetPrototype(DOM_Runtime::SVGSTRINGLIST_PROTOTYPE),
													"SVGStringList"));
	OP_ASSERT(svg_list != NULL);
	list->svg_list = svg_list;
	list->location = location;
	return OpStatus::OK;
}

/* virtual */ void
DOM_SVGStringList::GCTrace()
{
	location.GCTrace();
}

/* virtual */ ES_GetState
DOM_SVGStringList::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_numberOfItems || property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, svg_list->GetCount());
		return GET_SUCCESS;
	}
	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_SVGStringList::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	unsigned idx = static_cast<unsigned>(property_index);

	if (idx >= svg_list->GetCount())
		return GET_FAILED;

	if (!value)
		return GET_SUCCESS;

	return GetItemAtIndex(idx, value);
}

/* virtual */ ES_GetState
DOM_SVGStringList::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = svg_list->GetCount();
	return GET_SUCCESS;
}

/* static */ int
DOM_SVGStringList::clear(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(list, DOM_TYPE_SVG_STRING_LIST, DOM_SVGStringList);
	list->svg_list->Clear();
	list->location.Invalidate();
	return ES_FAILED;
}

/* static */ int
DOM_SVGStringList::initialize(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("s");
	DOM_THIS_OBJECT(list, DOM_TYPE_SVG_STRING_LIST, DOM_SVGStringList);
	if (OpStatus::IsMemoryError(list->svg_list->Initialize(argv[0].value.string)))
	{
		return ES_NO_MEMORY;
	}
	else
	{
		list->location.Invalidate();
		DOMSetString(return_value, argv[0].value.string);
		return ES_VALUE;
	}
}

/* static */ int
DOM_SVGStringList::getItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("n");
	DOM_THIS_OBJECT(list, DOM_TYPE_SVG_STRING_LIST, DOM_SVGStringList);

	unsigned idx = static_cast<unsigned>(argv[0].value.number);

	/* Range-check */
	if (idx >= list->svg_list->GetCount())
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	RETURN_GETNAME_AS_CALL(list->GetItemAtIndex(idx, return_value));
}

ES_GetState DOM_SVGStringList::GetItemAtIndex(UINT32 idx, ES_Value* value)
{
	const uni_char* item_str;
	OP_BOOLEAN get_ret = svg_list->GetItem(idx, item_str);
	if (get_ret == OpBoolean::IS_TRUE)
	{
		DOMSetString(value, item_str);
		return GET_SUCCESS;
	}
	else if (OpStatus::IsMemoryError(get_ret))
		return GET_NO_MEMORY;

	return GET_FAILED;
}

/* static */ int
DOM_SVGStringList::insertItemBefore(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("sn"); // FIXME: Should return SVGException
							   // SVG_WRONG_TYPE_ERR: Raised if
							   // parameter newItem is the wrong type
							   // of object for the given list.
	DOM_THIS_OBJECT(list, DOM_TYPE_SVG_STRING_LIST, DOM_SVGStringList);

	unsigned idx = static_cast<unsigned>(argv[1].value.number);

	/* Range-check: If the index is equal to 0, then the new item is
	 * inserted at the front of the list. If the index is greater than
	 * or equal to numberOfItems, then the new item is appended to the
	 * end of the list.*/
	if (idx > list->svg_list->GetCount())
		idx = list->svg_list->GetCount();

	OP_BOOLEAN insert_ret = list->svg_list->InsertItemBefore(argv[0].value.string,
															 idx);
	if (OpStatus::IsMemoryError(insert_ret))
		return ES_NO_MEMORY;
	else
	{
		list->location.Invalidate();
		DOMSetString(return_value, argv[0].value.string);
		return ES_VALUE;
	}
}

/* static */ int
DOM_SVGStringList::replaceItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("sn");
	DOM_THIS_OBJECT(list, DOM_TYPE_SVG_STRING_LIST, DOM_SVGStringList);

	unsigned idx = static_cast<unsigned>(argv[1].value.number);

	/* Range-check */
	if (idx >= list->svg_list->GetCount())
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	OP_BOOLEAN get_ret = list->svg_list->ApplyChange(idx, argv[0].value.string);
	if (get_ret == OpBoolean::IS_TRUE)
	{
		list->location.Invalidate();
		DOMSetString(return_value, argv[0].value.string);
		return ES_VALUE;
	}
	else if (OpStatus::IsMemoryError(get_ret))
		return ES_NO_MEMORY;
	else
		return ES_FAILED;
}

/* static */ int
DOM_SVGStringList::removeItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("n");
	DOM_THIS_OBJECT(list, DOM_TYPE_SVG_STRING_LIST, DOM_SVGStringList);
	unsigned idx = static_cast<unsigned>(argv[0].value.number);

	/* Range-check */
	if (idx >= list->svg_list->GetCount())
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	const uni_char* removed_string = NULL;
	CALL_FAILED_IF_ERROR(list->svg_list->RemoveItem(idx, removed_string));
	list->location.Invalidate();
	DOMSetString(return_value, removed_string);
	return ES_VALUE;
}

/* static */ int
DOM_SVGStringList::appendItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("s");  // FIXME: Should return SVGException
							   // SVG_WRONG_TYPE_ERR: Raised if
							   // parameter newItem is the wrong type
							   // of object for the given list.
	DOM_THIS_OBJECT(list, DOM_TYPE_SVG_STRING_LIST, DOM_SVGStringList);

	UINT32 idx = list->svg_list->GetCount();
	OP_BOOLEAN insert_ret = list->svg_list->InsertItemBefore(argv[0].value.string,
															 idx);
	if (OpStatus::IsMemoryError(insert_ret))
		return ES_NO_MEMORY;

	list->location.Invalidate();
	DOMSetString(return_value, argv[0].value.string);
	return ES_VALUE;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_SVGList)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGList, DOM_SVGList::clear, "clear", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGList, DOM_SVGList::initialize, "initialize", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGList, DOM_SVGList::getItem, "getItem", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGList, DOM_SVGList::getItem, "item", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGList, DOM_SVGList::insertItemBefore, "insertItemBefore", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGList, DOM_SVGList::replaceItem, "replaceItem", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGList, DOM_SVGList::removeItem, "removeItem", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGList, DOM_SVGList::appendItem, "appendItem", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGList, DOM_SVGList::createSVGTransformFromMatrix, "createSVGTransformFromMatrix", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGList, DOM_SVGList::consolidate, "consolidate", 0)
DOM_FUNCTIONS_END(DOM_SVGList)

DOM_FUNCTIONS_START(DOM_SVGStringList)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGStringList, DOM_SVGStringList::clear, "clear", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGStringList, DOM_SVGStringList::initialize, "initialize", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGStringList, DOM_SVGStringList::getItem, "getItem", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGStringList, DOM_SVGStringList::getItem, "item", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGStringList, DOM_SVGStringList::insertItemBefore, "insertItemBefore", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGStringList, DOM_SVGStringList::replaceItem, "replaceItem", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGStringList, DOM_SVGStringList::removeItem, "removeItem", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_SVGStringList, DOM_SVGStringList::appendItem, "appendItem", 0)
DOM_FUNCTIONS_END(DOM_SVGStringList)
# endif // SVG_FULL_11
#endif // SVG_DOM
