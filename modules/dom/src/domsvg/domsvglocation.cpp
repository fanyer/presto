/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_DOM

#include "modules/logdoc/logdocenum.h"
#include "modules/dom/src/domsvg/domsvglocation.h"
#include "modules/dom/src/domsvg/domsvgobject.h"
#include "modules/dom/src/domsvg/domsvglist.h"
#include "modules/dom/src/domsvg/domsvgelement.h"
#include "modules/svg/SVGManager.h"
#include "modules/svg/svg_dominterfaces.h"

DOM_SVGLocation::DOM_SVGLocation() :
		elm(NULL), m_location_info_packed_init(0)
{
	m_location_info_packed.attr = Markup::HA_NULL;
	serial = 0;
}

// use ATTR_FIRST_ALLOWED as a special marker for currentTranslate
DOM_SVGLocation::DOM_SVGLocation(DOM_Element* e, BOOL is_current_translate) :
		elm(e), m_location_info_packed_init(0)
{
	serial = 0;
	m_location_info_packed.attr = Markup::HA_NULL;
	m_location_info_packed.is_current_translate = is_current_translate ? 1 : 0;
}

DOM_SVGLocation::DOM_SVGLocation(DOM_Element* e, Markup::AttrType a, NS_Type n) :
		elm(e), m_location_info_packed_init(0)
{
	m_location_info_packed.attr = a;
	m_location_info_packed.ns = n;
	serial = SVGDOM::Serial(elm->GetThisElement(), a, n);
}

DOM_SVGLocation
DOM_SVGLocation::WithAttr(Markup::AttrType a, NS_Type n)
{
	OP_ASSERT(GetAttr() == Markup::HA_NULL && GetNS() == NS_NONE);
	return DOM_SVGLocation(elm, a, n);
}

void
DOM_SVGLocation::GCTrace()
{
	DOM_Object::GCMark(elm);
}

BOOL
DOM_SVGLocation::IsValid()
{
	if (elm != NULL && GetAttr() != ATTR_NULL && GetNS() != NS_NONE)
		return serial == SVGDOM::Serial(elm->GetThisElement(), GetAttr(), GetNS());
	else
		return TRUE;
}

/* static */ BOOL
DOM_SVGLocation::IsValid(DOM_Object* obj)
{
	if (!obj)
		return FALSE;

	BOOL3 is_valid = MAYBE;

	// Check if objects or lists has become invalid

	if (obj->IsA(DOM_TYPE_SVG_OBJECT))
	{
		DOM_SVGObject* dom_svg_obj = static_cast<DOM_SVGObject*>(obj);
		is_valid = dom_svg_obj->IsValid() ? YES : NO;
	}
#ifdef SVG_FULL_11
	else if (obj->IsA(DOM_TYPE_SVG_LIST))
	{
		DOM_SVGList* dom_svg_list = static_cast<DOM_SVGList*>(obj);
		is_valid = dom_svg_list->IsValid() ? YES : NO;
	}
	else if (obj->IsA(DOM_TYPE_SVG_ANIMATED_VALUE))
	{
		DOM_SVGAnimatedValue* dom_svg_animated_value = static_cast<DOM_SVGAnimatedValue*>(obj);
		is_valid = dom_svg_animated_value->IsValid() ? YES : NO;
	}
	else if (obj->IsA(DOM_TYPE_SVG_STRING_LIST))
	{
		DOM_SVGStringList* dom_svg_string_list = static_cast<DOM_SVGStringList*>(obj);
		is_valid = dom_svg_string_list->IsValid() ? YES : NO;
	}
#endif // SVG_FULL_11

	OP_ASSERT(is_valid != MAYBE);
	return is_valid != NO;
}

void
DOM_SVGLocation::Invalidate(BOOL was_removed /* = FALSE */)
{
	if (elm != NULL && GetAttr() != ATTR_NULL && GetNS() != NS_NONE)
		g_svg_manager->HandleSVGAttributeChange(elm->GetFramesDocument(), elm->GetThisElement(),
												GetAttr(), GetNS(), was_removed);
	else if (elm != NULL)
		g_svg_manager->InvalidateCaches(elm->GetThisElement(), elm->GetFramesDocument());
}

#endif // SVG_DOM
