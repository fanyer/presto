/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SVG_FILTER_H
#define SVG_FILTER_H

#if defined(SVG_SUPPORT) && defined(SVG_SUPPORT_FILTERS)

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGRect.h"
#include "modules/svg/src/SVGValue.h"
#include "modules/svg/src/SVGMatrix.h"
#include "modules/svg/src/svgfilternode.h"

enum SVGFilterInputNodeType
{
	INPUTNODE_SOURCEGRAPHIC,
	INPUTNODE_SOURCEALPHA,
	INPUTNODE_BACKGROUNDIMAGE,
	INPUTNODE_BACKGROUNDALPHA,
	INPUTNODE_FILLPAINT,
	INPUTNODE_STROKEPAINT,

	NUM_INPUTNODES
};

class SVGCanvas;
class SVGDocumentContext;
class SVGElementResolver;

class SVGFilter
{
public:
	SVGFilter(HTML_Element* src_elm) :
		m_input_nodes_used(0), m_child_root(NULL), m_source_elm(src_elm),
		m_primitive_units(SVGUNITS_USERSPACEONUSE), m_filter_units(SVGUNITS_OBJECTBBOX),
		m_custom_filter_res(FALSE), m_max_res_width(0), m_max_res_height(0),
		m_image_rendering(SVGIMAGERENDERING_OPTIMIZEQUALITY) {}

	static OP_STATUS Create(HTML_Element* filter_elm, HTML_Element* filter_target_elm,
							const SVGValueContext& vcxt, SVGElementResolver* resolver,
							SVGDocumentContext* doc_ctx,
							SVGCanvas* canvas,
							SVGBoundingBox* override_targetbbox,
							SVGFilter** outfilter);

	OP_STATUS Apply(SVGPainter* painter, SVGPaintNode* context_node);

	SVGBoundingBox GetFilteredExtents(const SVGBoundingBox& extents, SVGPaintNode* context_node);

	void SetRegionInUnits(const SVGRect& region) { m_orig_region = region; }
	SVGRect GetRegionInUnits() { return m_orig_region; }

	void SetFilterRegion(const SVGRect& region) { m_region = region; }
	SVGRect GetFilterRegion() { return m_region; }

	BOOL UseInterpolation() { return m_custom_filter_res; }

	void SetSourceElementBBox(const SVGBoundingBox& bbox) { m_elm_bbox = bbox; }
	SVGBoundingBox GetSourceElementBBox() { return m_elm_bbox; }

	SVGUnitsType GetPrimitiveUnits() const { return m_primitive_units; }
	void SetPrimitiveUnits(SVGUnitsType units) { m_primitive_units = units; }

	SVGUnitsType GetFilterUnits() const { return m_filter_units; }
	void SetFilterUnits(SVGUnitsType units) { m_filter_units = units; }

	void SetImageRendering(SVGImageRendering image_rendering) { m_image_rendering = image_rendering; }

	SVGNumber GetResolvedNumber(HTML_Element* elm, Markup::AttrType attr_type, SVGLength::LengthOrientation type, BOOL apply_offset);
	SVGNumberPair GetResolvedNumberOptionalNumber(HTML_Element* elm, Markup::AttrType attr_type, const SVGNumber& default_value);

	HTML_Element* GetChildRoot() const { return m_child_root; }
	void SetChildRoot(HTML_Element* child_root) { m_child_root = child_root; }

	HTML_Element* GetSourceElement() { return m_source_elm; }

	void SetFilterResolution(SVGNumber pix_width, SVGNumber pix_height)
	{
		m_filter_res_w = pix_width;
		m_filter_res_h = pix_height;
		m_custom_filter_res = TRUE;
	}

	OP_STATUS PushFilterNode(SVGFilterNode* filter_node)
	{
		filter_node->SetNodeId(m_nodes.GetCount() + NUM_INPUTNODES);
		return m_nodes.Add(filter_node);
	}
	SVGFilterNode* GetFilterNodeAt(unsigned idx)
	{
		return m_nodes.Get(idx);
	}
	SVGFilterNode* GetFilterInputNode(SVGFilterInputNodeType node_num)
	{
		OP_ASSERT((unsigned)node_num < NUM_INPUTNODES);
		m_input_nodes_used |= 1 << node_num;
		return m_input_nodes + node_num;
	}

private:
	static OP_STATUS FetchValues(SVGFilter* filter, HTML_Element* filter_elm,
								 SVGElementResolver* resolver, SVGDocumentContext* doc_ctx,
								 HTML_Element* filter_target_elm,
								 const SVGValueContext& vcxt, SVGBoundingBox* override_targetbbox);

	void ResolvePrimitiveUnits(SVGNumber& io_val, SVGLength::LengthOrientation type, BOOL offset);

	OpAutoVector<SVGFilterNode> m_nodes; // Filter nodes
	SVGSourceImageFilterNode m_input_nodes[NUM_INPUTNODES]; // Input filter nodes (source nodes)
	unsigned m_input_nodes_used; ///< Bitfield with a bit set for each input node requested via GetFilterInputNode

	BOOL ReferencesInputNode(SVGFilterInputNodeType node_num) const
	{
		return (m_input_nodes_used & (1 << node_num)) != 0;
	}
	void SetupInputNodes();

	SVGRect m_orig_region;
	SVGRect m_region;

	SVGBoundingBox m_elm_bbox;

	HTML_Element* m_child_root;
	HTML_Element* m_source_elm;

	SVGUnitsType m_primitive_units;
	SVGUnitsType m_filter_units;

	BOOL m_custom_filter_res;
	SVGNumber m_filter_res_w;
	SVGNumber m_filter_res_h;

	unsigned int m_max_res_width;
	unsigned int m_max_res_height;

	SVGImageRendering m_image_rendering;

	void SetMaxResolution(unsigned int max_width, unsigned int max_height)
	{
		m_max_res_width = max_width;
		m_max_res_height = max_height;
	}

	SVGDocumentContext*	m_doc_ctx;
	SVGElementResolver*	m_resolver;
};

#endif // SVG_SUPPORT && SVG_SUPPORT_FILTERS
#endif // SVG_FILTER_H
