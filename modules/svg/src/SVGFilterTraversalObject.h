/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_FILTER_TRAVERSAL_OBJECT_H
#define SVG_FILTER_TRAVERSAL_OBJECT_H

#if defined(SVG_SUPPORT) && defined(SVG_SUPPORT_FILTERS)

#include "modules/svg/src/SVGTraverse.h"

class SVGFilterNode;
class SVGFilter;
struct SVGLightSource;

class SVGFilterTraversalObject : public SVGTraversalObject
{
public:
	SVGFilterTraversalObject(SVGChildIterator* child_iterator, SVGFilter* filter) :
		SVGTraversalObject(child_iterator), m_filter(filter) {}
	virtual ~SVGFilterTraversalObject() {}

	virtual OP_STATUS HandleFilterPrimitive(SVGElementInfo& info);

private:
	void CreateFeImage(HTML_Element* feimage_element, const SVGRect& feimage_region,
					   SVGDocumentContext* doc_ctx, SVGElementResolver* resolver,
					   SVGPaintNode*& paint_node);
	void GetLightSource(HTML_Element* elm, SVGLightSource& light);

	OP_STATUS GetFilterPrimitiveRegion(HTML_Element* prim_element, SVGRect& region);
	SVGFilterNode* LookupInput(HTML_Element* elm, Markup::AttrType input_attr);
	OP_STATUS PushNode(SVGString* result_name, SVGFilterNode* filter_node);

	// Result destinations
	OpVector<SVGString> m_res_store;

	SVGFilter* m_filter;
};

#endif // SVG_SUPPORT && SVG_SUPPORT_FILTERS
#endif // SVG_FILTER_TRAVERSAL_OBJECT_H
