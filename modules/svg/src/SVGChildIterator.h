/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_CHILD_ITERATOR_H
#define SVG_CHILD_ITERATOR_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGTextElementStateContext.h" // SVGTextNodePool

#include "modules/util/adt/opvector.h"

class SVGElementContext;
struct SVGElementInfo;

class SVGChildIterator
{
public:
	virtual SVGElementContext* FirstChild(SVGElementInfo& info) = 0;
	virtual SVGElementContext* NextChild(SVGElementContext* parent_context,
										 SVGElementContext* child_context) = 0;
};

class SVGRenderingTreeChildIterator : public SVGChildIterator
{
public:
	virtual SVGElementContext* FirstChild(SVGElementInfo& info);
	virtual SVGElementContext* NextChild(SVGElementContext* parent_context,
										 SVGElementContext* child_context);
};

class SVGLogicalTreeChildIterator : public SVGChildIterator
{
public:
	virtual SVGElementContext* FirstChild(SVGElementInfo& info);
	virtual SVGElementContext* NextChild(SVGElementContext* parent_context,
										 SVGElementContext* child_context);

protected:
	SVGElementContext* GetNextChildContext(SVGElementContext* parent_context,
										   HTML_Element* candidate_child);
};

class SVGTreePathChildIterator : public SVGChildIterator
{
public:
	SVGTreePathChildIterator(OpVector<HTML_Element>* treepath) : m_treepath(treepath) {}

	virtual SVGElementContext* FirstChild(SVGElementInfo& info);
	virtual SVGElementContext* NextChild(SVGElementContext* parent_context,
										 SVGElementContext* child_context);

protected:
	OpVector<HTML_Element>* m_treepath;
};

#endif // SVG_SUPPORT
#endif // SVG_CHILD_ITERATOR_H
