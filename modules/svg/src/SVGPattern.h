/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVGPATTERN_H
#define SVGPATTERN_H

#ifdef SVG_SUPPORT
#ifdef SVG_SUPPORT_PATTERNS

#include "modules/svg/src/SVGMatrix.h"
#include "modules/svg/src/SVGRect.h"
#include "modules/svg/src/SVGLength.h"
#include "modules/svg/src/SVGCanvas.h"
#include "modules/svg/src/SVGValue.h"
#include "modules/svg/src/svgpaintserver.h"

class SVGDocumentContext;
class SVGElementResolver;
struct SVGPatternParameters;
class SVGPaintNode;
class VEGARenderTarget;

class SVGPattern : public SVGPaintServer
{
private:
	SVGRect				m_pattern_rect;
	SVGUnitsType		m_units;
	SVGUnitsType		m_content_units;
	SVGMatrix			m_transform;
	SVGRectObject*		m_viewbox;
	SVGAspectRatio*		m_aspectratio;
	unsigned int		m_max_res_width;
	unsigned int		m_max_res_height;
	SVGPaintNode*		m_content_node;
	VEGARenderTarget*	m_target;

	OP_STATUS FetchValues(HTML_Element* pattern_element,
						  SVGElementResolver* resolver, SVGDocumentContext* doc_ctx,
						  SVGPatternParameters* params, HTML_Element** content_root);
	void ResolvePatternParameters(const SVGPatternParameters& params, const SVGValueContext& vcxt);
	OP_STATUS GeneratePatternContent(SVGDocumentContext* doc_ctx, SVGElementResolver* resolver,
									 const SVGValueContext& vcxt, HTML_Element *pattern_element,
									 HTML_Element* content_root, HTML_Element* context_element);

	void SetUnits(SVGUnitsType type) { m_units = type; }
	void SetContentUnits(SVGUnitsType type) { m_content_units = type; }
	void SetTransform(const SVGMatrix& t) { m_transform = t; }

	void SetViewBox(SVGRectObject* vb)
	{
		SVGObject::IncRef(vb);
		SVGObject::DecRef(m_viewbox);
		m_viewbox = vb;
	}
	const SVGRectObject* GetViewBox() const { return m_viewbox; }

	void SetAspectRatio(SVGAspectRatio* ar)
	{
		SVGObject::IncRef(ar);
		SVGObject::DecRef(m_aspectratio);
		m_aspectratio = ar;
	}
	const SVGAspectRatio* GetAspectRatio() const { return m_aspectratio; }

	SVGPattern() :
		m_units(SVGUNITS_OBJECTBBOX), m_content_units(SVGUNITS_USERSPACEONUSE),
		m_viewbox(NULL), m_aspectratio(NULL), m_content_node(NULL), m_target(NULL) {}

public:
	static OP_STATUS Create(HTML_Element *pattern_element,
							HTML_Element* context_element, SVGElementResolver* resolver,
							SVGDocumentContext* doc_ctx, const SVGValueContext& vcxt,
							SVGPattern **outpat);

	virtual ~SVGPattern();

	virtual OP_STATUS GetFill(VEGAFill** vfill, VEGATransform& vfilltrans,
							  SVGPainter* painter, SVGPaintNode* context_node);
	virtual void PutFill(VEGAFill* vfill);
};
#endif // SVG_SUPPORT_PATTERNS
#endif // SVG_SUPPORT
#endif // SVGPATTERN_H
