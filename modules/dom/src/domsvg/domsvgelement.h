/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_SVGELEM_H
#define DOM_SVGELEM_H

#ifdef SVG_DOM

#include "modules/dom/src/domcore/element.h"
#include "modules/dom/src/domsvg/domsvgobject.h"
#include "modules/dom/src/domsvg/domsvgenum.h"
#include "modules/dom/src/domcore/domtokenlist.h"

#include "modules/svg/svg_dominterfaces.h"

/** The DOM element of an SVG element.
 */
class DOM_SVGElement
  : public DOM_Element
{
protected:
	DOM_SVGElement(DOM_SVGElementInterface ifs) : ifs(ifs), object_store(NULL) {}

	DOM_SVGElementInterface ifs;
	DOM_SVGObjectStore* object_store;
	DOM_SVGLocation location;

#ifdef SVG_FULL_11
	ES_GetState GetAnimatedEnumeration(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	ES_GetState GetAnimatedLength(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	ES_GetState GetAnimatedList(SVGDOMItemType listtype, OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	ES_GetState GetAnimatedNumber(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	ES_GetState GetAnimatedString(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	ES_GetState GetAnimatedNumberOptionalNumber(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, UINT32 idx);
	ES_GetState GetAnimatedAngle(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	ES_GetState GetAnimatedPreserveAspectRatio(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	ES_GetState GetAnimatedValue(OpAtom property_name, SVGDOMItemType item_type, ES_Value* value, ES_Runtime* origining_runtime);

	ES_GetState GetPointList(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	ES_GetState GetViewport(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	ES_GetState GetPathSegList(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	ES_GetState GetStringList(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	ES_GetState GetInstanceRoot(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	ES_GetState GetViewportElement(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	ES_GetState GetStringAttribute(OpAtom property_name, ES_Value* value);

	enum FrameEnvironmentType { FRAME_DOCUMENT, FRAME_WINDOW };
	ES_GetState GetFrameEnvironment(ES_Value* value, FrameEnvironmentType type, DOM_Runtime *origining_runtime);
#endif // SVG_FULL_11

#if defined(SVG_TINY_12) || defined(SVG_FULL_11)
	ES_GetState GetCurrentTranslate(OpAtom property_name, ES_Value* valye, ES_Runtime* origining_runtime);
	ES_PutState SetStringAttribute(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
#endif // SVG_TINY_12 || SVG_FULL_11

public:
	/** If you change the number of items in the DOM_SVGElement::element_spec array this value should be changed also. **/
	enum { DOM_SVG_ELEMENT_COUNT =
#if defined(SVG_TINY_12) || defined(SVG_FULL_11)
		35
#endif // SVG_TINY_12 || SVG_FULL_11
#ifdef SVG_FULL_11
		+ 32
#endif
#ifdef SVG_TINY_12
		+ 4
#endif // SVG_TINY_12
		+1 // for sentinel
	};

	virtual ~DOM_SVGElement();
	static OP_STATUS Make(DOM_SVGElement *&element, HTML_Element *html_element, DOM_EnvironmentImpl *environment);
	static OP_STATUS CreateElement(DOM_SVGElement *&element, DOM_Node *reference, const uni_char *name);
	static void ConstructDOMImplementationSVGElementObjectL(ES_Object *element, DOM_SVGElementInterface ifs, DOM_Runtime *runtime);

	virtual OP_STATUS GetBoundingClientRect(DOM_Object *&object);
	virtual OP_STATUS GetClientRects(DOM_ClientRectList *&object);

	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_SVG_ELEMENT || DOM_Element::IsA(type); }
	virtual void GCTraceSpecial(BOOL via_tree);

	static void PutConstructorsL(DOM_Object* target);

	static void InitializeConstructorsTableL(OpString8HashTable<DOM_ConstructorInformation> *table);
	static ES_Object *CreateConstructorL(DOM_Runtime *runtime, DOM_Object *target, unsigned id);

#ifdef SVG_FULL_11
	// Text methods
	DOM_DECLARE_FUNCTION(getNumberOfChars);
	DOM_DECLARE_FUNCTION(getComputedTextLength);
	DOM_DECLARE_FUNCTION(getSubStringLength);
	DOM_DECLARE_FUNCTION(getStartPositionOfChar);
	DOM_DECLARE_FUNCTION(getEndPositionOfChar);
	DOM_DECLARE_FUNCTION(getExtentOfChar);
	DOM_DECLARE_FUNCTION(getRotationOfChar);
	DOM_DECLARE_FUNCTION(getCharNumAtPosition);
	DOM_DECLARE_FUNCTION(selectSubString);

	// Path methods
	DOM_DECLARE_FUNCTION(getTotalLength);
	DOM_DECLARE_FUNCTION(getPointAtLength);
	DOM_DECLARE_FUNCTION(getPathSegAtLength);
	DOM_DECLARE_FUNCTION_WITH_DATA(createSVGPathSeg);

	// getSVGDocument
	DOM_DECLARE_FUNCTION(getSVGDocument);
#endif // SVG_FULL_11

#if defined(SVG_TINY_12) || defined(SVG_FULL_11)
	// Locatable methods
	DOM_DECLARE_FUNCTION_WITH_DATA(getBBox);
	DOM_DECLARE_FUNCTION(getScreenCTM);
#endif // SVG_TINY_12 || SVG_FULL_11

#ifdef SVG_FULL_11
	DOM_DECLARE_FUNCTION(getCTM);
	DOM_DECLARE_FUNCTION(getTransformToElement);

	// Stylable methods
	DOM_DECLARE_FUNCTION(getPresentationAttribute);
#endif // SVG_FULL_11

#if defined(SVG_TINY_12) || defined(SVG_FULL_11)
	// ElementTimeControl
	DOM_DECLARE_FUNCTION_WITH_DATA(beginElement);
	DOM_DECLARE_FUNCTION_WITH_DATA(endElement);
#endif // SVG_TINY_12 || SVG_FULL_11

#ifdef SVG_FULL_11
	DOM_DECLARE_FUNCTION(getStartTime);
	DOM_DECLARE_FUNCTION(getSimpleDuration);

	// Filter methods
	DOM_DECLARE_FUNCTION(setFilterRes);

	// Gaussian Blur methods
	DOM_DECLARE_FUNCTION(setStdDeviation);

	// Marker methods
	DOM_DECLARE_FUNCTION(setOrientToAuto);
	DOM_DECLARE_FUNCTION(setOrientToAngle);

	DOM_DECLARE_FUNCTION(hasExtension);
#endif // SVG_FULL_11

#ifdef SVG_TINY_12
	// SVGT12 uDOM TraitAccess
	DOM_DECLARE_FUNCTION_WITH_DATA(setObjectTrait);
	DOM_DECLARE_FUNCTION_WITH_DATA(getObjectTrait);
#endif // SVG_TINY_12

#if defined(SVG_TINY_12) || defined(SVG_FULL_11)
	DOM_DECLARE_FUNCTION_WITH_DATA(createSVGObject);
	DOM_DECLARE_FUNCTION(pauseAnimations);
	DOM_DECLARE_FUNCTION(unpauseAnimations);
	DOM_DECLARE_FUNCTION(animationsPaused);
	DOM_DECLARE_FUNCTION_WITH_DATA(getCurrentTime);
	DOM_DECLARE_FUNCTION(setCurrentTime);
#endif // SVG_TINY_12 || SVG_FULL_11
#ifdef SVG_FULL_11
	DOM_DECLARE_FUNCTION(suspendRedraw);
	DOM_DECLARE_FUNCTION(unsuspendRedraw);
	DOM_DECLARE_FUNCTION(unsuspendRedrawAll);
	DOM_DECLARE_FUNCTION(forceRedraw);
	DOM_DECLARE_FUNCTION_WITH_DATA(testIntersection);
	DOM_DECLARE_FUNCTION(deselectAll);
	DOM_DECLARE_FUNCTION(getComputedStyle);
#endif // SVG_FULL_11
#ifdef SVG_TINY_12
	// SVGT 1.2 uDOM
	DOM_DECLARE_FUNCTION(setFocus);
	DOM_DECLARE_FUNCTION(moveFocus);
	DOM_DECLARE_FUNCTION(getCurrentFocusedObject);
#endif // SVG_TINY_12
};

class DOM_SVGElement_Prototype
	: public DOM_Object
{
private:
	static void ConstructL(ES_Object *object, DOM_Runtime::SVGElementPrototype prototype, DOM_SVGElementInterface ifs, DOM_Runtime *runtime);

public:
	static OP_STATUS Construct(ES_Object *object, DOM_Runtime::SVGElementPrototype prototype, DOM_SVGElementInterface ifs, DOM_Runtime *runtime);
};

/*
 * Implements
 *
 * Animated objects:
 *  SVGAnimatedString
 *  SVGAnimatedEnumeration
 *  SVGAnimatedNumber
 *  SVGAnimatedLength
 *  SVGAnimatedPreserveAspectRatio
 *  SVGAnimatedRect
 *  SVGAnimatedAngle
 *
 * Animated lists:
 *  SVGAnimatedPointList
 *  SVGAnimatedNumberList
 *  SVGAnimatedLengthList
 *  SVGAnimatedTransformList
 *  SVGAnimatedMatrixList
 *
 */
class DOM_SVGAnimatedValue : public DOM_Object
{
public:
	virtual ~DOM_SVGAnimatedValue();
	static OP_STATUS Make(DOM_SVGAnimatedValue *&anim_val, SVGDOMAnimatedValue* svg_val,
						  const char* name, DOM_SVGLocation location, DOM_EnvironmentImpl *environment);

	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_SVG_ANIMATED_VALUE || DOM_Object::IsA(type); }

	virtual void GCTrace();

	BOOL IsValid() { return location.IsValid(); }

protected:
	DOM_SVGAnimatedValue() : base_object(NULL), anim_object(NULL), svg_value(NULL) {}
	DOM_SVGAnimatedValue(const DOM_SVGAnimatedValue&) : DOM_Object() {}

	DOM_Object* base_object;
	DOM_Object* anim_object;
	DOM_SVGLocation location;

	SVGDOMAnimatedValue* svg_value;
};

#endif // SVG_DOM
#endif // DOM_SVGELEM_H
