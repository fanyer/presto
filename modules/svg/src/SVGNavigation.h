/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SVG_NAVIGATION_H
#define SVG_NAVIGATION_H

#include "modules/svg/svg_tree_iterator.h"

class LayoutProperties;
class SVGDocumentContext;

/**
 * Iterate through all elements in a given area
 * with optional constraints
 */
class SVGAreaIterator : public SVGTreeIterator
{
public:
	HTML_Element* Next();
	HTML_Element* Prev();

	OP_STATUS Init(HTML_Element* start_elm, const OpRect* search_area);

protected:
	OP_STATUS TestVisible(HTML_Element* test_elm, HTML_Element* layouted_elm);
	OP_STATUS TestRelevantForDisplay(HTML_Element* layouted_elm);
	virtual OP_STATUS TestElement(HTML_Element* test_elm, HTML_Element* layouted_elm);
	BOOL Step(BOOL forward);

	OpRect m_search_area;

	HTML_Element* m_current_elm;
	SVGDocumentContext* m_doc_ctx;

	BOOL m_prev_found;
	BOOL m_next_found;
};

/**
 * Iterate through focusable elements
 */
class SVGFocusIterator : public SVGAreaIterator
{
protected:
	OP_STATUS TestElement(HTML_Element* test_elm, HTML_Element* layouted_elm);
};

#ifdef SEARCH_MATCHES_ALL
/**
 * Iterate through search highlight updates
 */
class SVGHighlightUpdateIterator : public SVGAreaIterator
{
public:
	OP_STATUS Init(HTML_Element* start_elm, SelectionElm* first_hit);

protected:
	OP_STATUS TestElement(HTML_Element* test_elm, HTML_Element* layouted_elm);

	SelectionElm* m_current_hit;
};
#endif // SEARCH_MATCHES_ALL

#ifdef RESERVED_REGIONS
/**
 * Iterate through elements defining reserved regions
 *
 * @See FEATURE_RESERVED_REGIONS
 */
class SVGReservedRegionIterator : public SVGAreaIterator
{
protected:
	OP_STATUS TestElement(HTML_Element* test_elm, HTML_Element* layouted_elm);
};
#endif // RESERVED_REGIONS

class SVGNavigation
{
public:
	static BOOL FindElementInDirection(HTML_Element* elm, int direction, int nway,
									   HTML_Element*& preferred_next_elm);

	static BOOL NavRefToElement(HTML_Element* elm, Markup::AttrType attr,
								HTML_Element*& preferred_next_elm);
};

#endif // SVG_NAVIGATION_H
