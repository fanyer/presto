 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/** @file webforms2support.h
 *
 * Help classes for the web forms 2 support. Dates mostly.
 *
 * @author Daniel Bratell
 */

#ifndef _WEBFORMS2DOM_H_
#define _WEBFORMS2DOM_H_

#include "modules/dom/src/domcore/element.h"
#include "modules/dom/src/domhtml/htmlcoll.h"

class DOM_Object;
class DOM_Element;
class DOM_Collection;
class DOM_Node;
class DOM_CollectionFilter;
class DOM_Environment;
class HTML_Element;
class ES_Value;

// LabelCollectionFilter
class LabelCollectionFilter : public DOM_CollectionFilter
{
public:
	LabelCollectionFilter(HTML_Element* form_element) : m_form_element(form_element) {}
	virtual ~LabelCollectionFilter() {};

	virtual void Visit(HTML_Element *element, BOOL &include, BOOL &visit_children, LogicalDocument *logdoc);

	virtual DOM_CollectionFilter *Clone() const;

	virtual Type GetType() { return TYPE_LABELS; }
	virtual BOOL IsNameSearch(const uni_char *&name) { return FALSE; }
	virtual BOOL CanSkipChildren() { return FALSE; }
	virtual BOOL CanIncludeGrandChildren() { return TRUE; }
	virtual BOOL IsMatched(unsigned collections);
	virtual BOOL IsEqual(DOM_CollectionFilter *other);

private:
	HTML_Element* m_form_element;
};

#endif // _WEBFORMS2DOM_H_
