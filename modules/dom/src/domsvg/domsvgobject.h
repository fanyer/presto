/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_SVGOBJECT_H
#define DOM_SVGOBJECT_H

#ifdef SVG_DOM

#include "modules/dom/src/domsvg/domsvglocation.h"
#include "modules/svg/svg_dominterfaces.h"
#include "modules/dom/src/domcore/element.h"

class SVGDOMItem;
class SVGDOMPoint;
class SVGDOMRect;
class SVGDOMMatrix;

#ifdef SVG_FULL_11
class SVGDOMNumber;
class SVGDOMLength;
class SVGDOMAngle;
class SVGDOMTransform;
class SVGDOMAnimatedValue;
class SVGDOMList;
class DOM_SVGList;
#endif // SVG_FULL_11

class DOM_SVGObjectStore;

/** Retrieves the DOM part of the this obejct and checks that it is
    the right type of DOM object.  If it isn't, or if the object is a
    native object, it returns ES_FAILED. It also checks the sync
    between the dom object and svg item. It is missing the type check
    of this_object to DOM_SVGObject, so make sure the macro is used on
    DOM_SVGObjects.
 */
#define DOM_SVG_THIS_ITEM(VARIABLE, TYPE, CLASS)						\
	int svg_this_object_result = DOM_CheckType((DOM_Runtime *) origining_runtime, this_object, DOM_TYPE_SVG_OBJECT, return_value, DOM_Object::WRONG_THIS_ERR); \
	if (svg_this_object_result != ES_VALUE)								\
		return svg_this_object_result;									\
	CLASS *VARIABLE;													\
	SVGDOMItem* VARIABLE ## _tmp_ = ((DOM_SVGObject*)this_object)->GetSVGDOMItem(); \
	if (VARIABLE ## _tmp_->IsA(TYPE))									\
	{																	\
		VARIABLE = (CLASS *)VARIABLE ## _tmp_;							\
	}																	\
	else																\
		return ES_FAILED;												\


/** Retrieves the DOM part of the this obejct and checks that it is
    the right type of DOM object.  If it isn't, or if the object is a
    native object, it returns ES_FAILED. It also checks the sync
    between the dom object and svg item. It is missing the type check
    of this_object to DOM_SVGObject, so make sure the macro is used on
    DOM_SVGObjects.
 */
#define DOM_SVG_THIS_OBJECT_ITEM(VAR1, VAR2, TYPE, CLASS)				\
	int svg_this_object_result = DOM_CheckType((DOM_Runtime *) origining_runtime, this_object, DOM_TYPE_SVG_OBJECT, return_value, DOM_Object::WRONG_THIS_ERR); \
	if (svg_this_object_result != ES_VALUE)								\
		return svg_this_object_result;									\
	DOM_SVGObject *VAR1 = static_cast<DOM_SVGObject*>(this_object);		\
	CLASS *VAR2;														\
	SVGDOMItem* VAR2 ## _tmp_ = static_cast<DOM_SVGObject*>(this_object)->GetSVGDOMItem(); \
	if (VAR2 ## _tmp_->IsA(TYPE))										\
	{																	\
		VAR2 = static_cast<CLASS *>(VAR2 ## _tmp_);						\
	}																	\
	else																\
		return ES_FAILED;												\

/** Retrieves the SVG Item from a DOM_SVGObject from an argument and
    checks that it is the right type of SVG Item.  If it isn't, or if
    the object is a native object, it returns ES_FAILED.
 */
#define DOM_SVG_ARGUMENT_OBJECT(VARIABLE, INDEX, TYPE, CLASS)			\
	CLASS *VARIABLE = NULL;												\
	DOM_ARGUMENT_OBJECT(VARIABLE ## _tmp__1, INDEX, DOM_TYPE_SVG_OBJECT, DOM_SVGObject); \
	if (VARIABLE ## _tmp__1 != NULL)									\
	{																	\
		SVGDOMItem* VARIABLE ## _tmp_ = VARIABLE ## _tmp__1->GetSVGDOMItem(); \
		if (VARIABLE ## _tmp_->IsA(TYPE))								\
		{																\
			VARIABLE = (CLASS *)VARIABLE ## _tmp_;						\
		}																\
		else															\
		{																\
			return ES_FAILED;											\
		}																\
	}																	\
	else																\
	{																	\
		return ES_FAILED;												\
	}																	\

struct DOM_SVGObjectStatic
{
	SVGDOMItemType iface;
	double number;
	char* name;
};

class DOM_SVGObject : public DOM_Object
{
public:
	virtual				~DOM_SVGObject();
	static OP_STATUS	Make(DOM_SVGObject *&obj, SVGDOMItem* svg_item, const DOM_SVGLocation& location, DOM_EnvironmentImpl *environment);
	virtual BOOL		IsA(int type) { return type == DOM_TYPE_SVG_OBJECT || DOM_Object::IsA(type); }
	SVGDOMItem*			GetSVGDOMItem() { return svg_item; }

	void				Release();
#ifdef SVG_FULL_11
	DOM_SVGList*		InList() { return in_list; }
	void				SetInList(DOM_SVGList* list);
#endif // SVG_FULL_11

	HTML_Element*		GetThisElement() { return location.GetThisElement(); }
	void				Invalidate(BOOL was_removed = FALSE) { location.Invalidate(was_removed); }
	void				SetLocation(const DOM_SVGLocation& l) { location = l; }
	BOOL				IsValid() { return location.IsValid(); }

	BOOL				GetIsSignificant() { return (is_significant == 1); }
	void				SetIsSignificant() { is_significant = 1; }

	BOOL				HaveNativeProperty() { return (have_native_property == 1); }
	void				SetHaveNativeProperty() { have_native_property = 1; }

	virtual void		GCTrace();
	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	static void PutConstructorsL(DOM_Object* target);

	static void InitializeConstructorsTableL(OpString8HashTable<DOM_ConstructorInformation> *table);
	static ES_Object *CreateConstructorL(DOM_Runtime *runtime, DOM_Object *target, const char *name, unsigned id);

	static void ConstructDOMImplementationSVGObjectL(ES_Object *element, SVGDOMItemType type, DOM_Runtime *runtime);

#ifdef SVG_FULL_11
	DOM_DECLARE_FUNCTION_WITH_DATA(newValueSpecifiedUnits);
	DOM_DECLARE_FUNCTION_WITH_DATA(convertToSpecifiedUnits);
	DOM_DECLARE_FUNCTION(matrixTransform);
	DOM_DECLARE_FUNCTION_WITH_DATA(transformMethods);
#endif // SVG_FULL_11

	DOM_DECLARE_FUNCTION_WITH_DATA(matrixMethods);

#ifdef SVG_FULL_11
	DOM_DECLARE_FUNCTION(setUri);
	DOM_DECLARE_FUNCTION(setPaint);

	DOM_DECLARE_FUNCTION(setRGBColor);

	DOM_DECLARE_FUNCTION(getFloatValue);
#endif // SVG_FULL_11

#ifdef SVG_TINY_12
	// begin uDOM methods
	DOM_DECLARE_FUNCTION(getComponent);
	DOM_DECLARE_FUNCTION_WITH_DATA(mutableMatrixMethods);
	DOM_DECLARE_FUNCTION_WITH_DATA(svgPathBuilder);
	// end uDOM methods
#endif // SVG_TINY_12

protected:
	DOM_SVGObject() : is_significant(0), have_native_property(0) {}

#ifdef SVG_FULL_11
	ES_GetState GetLengthValue(OpAtom property_name, ES_Value* value, SVGDOMLength* svg_length,
							   ES_Runtime* origining_runtime);
#endif // SVG_FULL_11

	DOM_SVGObjectStore* object_store;

#ifdef SVG_FULL_11
	DOM_SVGList* in_list; ///< List that contains this object, if any
#endif // SVG_FULL_11

	SVGDOMItem* svg_item;
	DOM_SVGLocation location;

	unsigned is_significant:1;
	unsigned have_native_property:1;
private:
	DOM_SVGObject(const DOM_SVGObject&); // Don't implememnt to avoid accidental clones

};

class DOM_SVGObject_Prototype
{
private:
	static void ConstructL(ES_Object *prototype, DOM_Runtime::SVGObjectPrototype type, DOM_Runtime *runtime);

public:
	static OP_STATUS Construct(ES_Object *prototype, DOM_Runtime::SVGObjectPrototype type, DOM_Runtime *runtime);
};

#endif // SVG_DOM
#endif // DOM_SVGOBJECT_H
