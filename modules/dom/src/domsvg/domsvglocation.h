/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_SVGLOCATION_H
#define DOM_SVGLOCATION_H

#include "modules/logdoc/namespace.h"
#include "modules/dom/src/domcore/element.h"

class DOM_SVGLocation
{
public:
	DOM_SVGLocation();
	DOM_SVGLocation(DOM_Element* e, BOOL is_current_translate = FALSE);
	DOM_SVGLocation(DOM_Element* e, Markup::AttrType a, NS_Type n);

	void Invalidate(BOOL was_removed = FALSE);
	DOM_SVGLocation WithAttr(Markup::AttrType a, NS_Type n);
	void GCTrace();
	BOOL IsValid();

	static BOOL IsValid(DOM_Object* obj);

	BOOL IsCurrentTranslate() { return (m_location_info_packed.is_current_translate != 0); }

	HTML_Element* GetThisElement() { return elm ? elm->GetThisElement() : NULL; }
	Markup::AttrType GetAttr() { return static_cast<Markup::AttrType>(m_location_info_packed.attr); }
	NS_Type GetNS() { return static_cast<NS_Type>(m_location_info_packed.ns); }

private:
	DOM_Element* elm;
	union {
		struct {
			unsigned int attr:16;	// short attr
			unsigned int ns:8;		// NS_Type ns
			unsigned int is_current_translate:1;
		} m_location_info_packed;
		unsigned int m_location_info_packed_init;
	};
	UINT32 serial;
};

#endif // DOM_SVGLOCATION_H

