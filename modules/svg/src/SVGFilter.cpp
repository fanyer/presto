/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT
#include "modules/svg/src/svgpch.h"

#ifdef SVG_SUPPORT_FILTERS
#include "modules/svg/src/SVGFilter.h"
#include "modules/svg/src/SVGFilterTraversalObject.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGCanvas.h"
#include "modules/svg/src/svgpaintserver.h"
#include "modules/svg/src/SVGTraverse.h"
#include "modules/svg/src/SVGChildIterator.h"
#include "modules/svg/src/AttrValueStore.h"

#include "modules/display/vis_dev.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"

/* static */
OP_STATUS SVGFilter::FetchValues(SVGFilter* filter, HTML_Element* filter_elm,
								 SVGElementResolver* resolver, SVGDocumentContext* doc_ctx,
								 HTML_Element* filter_target_elm,
								 const SVGValueContext& vcxt, SVGBoundingBox* override_targetbbox)
{
	// Handle filter attributes
	// filterUnits, primitiveUnits, x, y, width, height,
	// filterRes, xlink:href<to other filter>
	// this means we should inherit from the referenced element

	HTML_Element* parent_filter_elm = SVGUtils::FindHrefReferredNode(resolver, doc_ctx, filter_elm);
	if (parent_filter_elm && !parent_filter_elm->IsMatchingType(Markup::SVGE_FILTER, NS_SVG))
		parent_filter_elm = NULL;

	if (parent_filter_elm)
	{
		doc_ctx->RegisterDependency(filter_elm, parent_filter_elm);

		SVGElementResolverStack resolver_mark(resolver);
		RETURN_IF_ERROR(resolver_mark.Push(parent_filter_elm));
		RETURN_IF_ERROR(FetchValues(filter, parent_filter_elm, resolver, doc_ctx, filter_target_elm,
									vcxt, override_targetbbox));
	}

	// If this element has no relevant children, and the referenced
	// element does (possibly due to its own href attribute), then
	// this element inherits the children from the referenced element.
	if (parent_filter_elm && !SVGUtils::HasChildren(filter_elm))
	{
		filter->SetChildRoot(filter->GetChildRoot());
	}
	else
	{
		filter->SetChildRoot(filter_elm);
	}

	if (AttrValueStore::HasObject(filter_elm, Markup::SVGA_FILTERUNITS, NS_IDX_SVG))
	{
		SVGUnitsType units;
		OP_STATUS status = AttrValueStore::GetUnits(filter_elm, Markup::SVGA_FILTERUNITS, units, SVGUNITS_OBJECTBBOX);
		if (OpStatus::IsSuccess(status))
			filter->SetFilterUnits(units);
	}

	if (AttrValueStore::HasObject(filter_elm, Markup::SVGA_PRIMITIVEUNITS, NS_IDX_SVG))
	{
		SVGUnitsType units;
		OP_STATUS status = AttrValueStore::GetUnits(filter_elm, Markup::SVGA_PRIMITIVEUNITS, units, SVGUNITS_USERSPACEONUSE);
		if (OpStatus::IsSuccess(status))
			filter->SetPrimitiveUnits(units);
	}

	// Need target bounding box
	SVGBoundingBox bbox;

	if (override_targetbbox)
	{
		bbox = *override_targetbbox;
	}
	else
	{
		SVGNumberPair vp(vcxt.viewport_width, vcxt.viewport_height);
		RETURN_IF_ERROR(GetElementBBox(doc_ctx, filter_target_elm, vp, bbox));
	}

	filter->SetSourceElementBBox(bbox);

	SVGRect filter_region = filter->GetRegionInUnits();
	if (!parent_filter_elm)
	{
		// Set defaults
		filter_region = SVGRect(-0.1, -0.1, 1.2, 1.2);
		if (filter->GetFilterUnits() == SVGUNITS_USERSPACEONUSE)
		{
			filter_region.x *= vcxt.viewport_width;
			filter_region.width *= vcxt.viewport_width;
			filter_region.y *= vcxt.viewport_height;
			filter_region.height *= vcxt.viewport_height;
		}
	}

	SVGUtils::GetResolvedLengthWithUnits(filter_elm, Markup::SVGA_X, SVGLength::SVGLENGTH_X,
										 filter->GetFilterUnits(), vcxt, filter_region.x);
	SVGUtils::GetResolvedLengthWithUnits(filter_elm, Markup::SVGA_Y, SVGLength::SVGLENGTH_Y,
										 filter->GetFilterUnits(), vcxt, filter_region.y);
	SVGUtils::GetResolvedLengthWithUnits(filter_elm, Markup::SVGA_WIDTH, SVGLength::SVGLENGTH_X,
										 filter->GetFilterUnits(), vcxt, filter_region.width);
	SVGUtils::GetResolvedLengthWithUnits(filter_elm, Markup::SVGA_HEIGHT, SVGLength::SVGLENGTH_Y,
										 filter->GetFilterUnits(), vcxt, filter_region.height);

	// "Negative values for width or height are an error"
	if (filter_region.width < 0 || filter_region.height < 0)
		return OpSVGStatus::INVALID_ARGUMENT;

	// "Zero values disable rendering of the element which referenced the filter."
	if (filter_region.width.Equal(0) || filter_region.height.Equal(0))
		return OpSVGStatus::ELEMENT_IS_INVISIBLE;

	filter->SetRegionInUnits(filter_region);
	if (filter->GetFilterUnits() == SVGUNITS_OBJECTBBOX)
	{
		SVGMatrix bbox_xfrm;

		/* bbox set earlier */
		bbox_xfrm.SetValues(bbox.maxx - bbox.minx, 0, 0,
							bbox.maxy - bbox.miny,
							bbox.minx, bbox.miny);

		filter_region = bbox_xfrm.ApplyToRect(filter_region);
	}
	filter->SetFilterRegion(filter_region);

	if (AttrValueStore::HasObject(filter_elm, Markup::SVGA_FILTERRES, NS_IDX_SVG))
	{
		SVGNumberPair filt_res;

		OP_STATUS status = SVGUtils::GetNumberOptionalNumber(filter_elm, Markup::SVGA_FILTERRES, filt_res);
		if (OpStatus::IsSuccess(status))
		{
			// "Negative or zero values disable rendering of the element which referenced the filter."
			if (filt_res.x <= 0 || filt_res.y <= 0)
				return OpSVGStatus::ELEMENT_IS_INVISIBLE;

			filter->SetFilterResolution(filt_res.x, filt_res.y);
		}
	}
	return OpStatus::OK;
}

/* static */
OP_STATUS SVGFilter::Create(HTML_Element* filter_elm, HTML_Element* filter_target_elm,
							const SVGValueContext& vcxt, SVGElementResolver* resolver,
							SVGDocumentContext* doc_ctx,
							SVGCanvas* canvas,
							SVGBoundingBox* override_targetbbox,
							SVGFilter** outfilter)
{
	OP_ASSERT(outfilter);

	SVGFilter* filter = OP_NEW(SVGFilter, (filter_target_elm));
	*outfilter = filter;

	if (!filter)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(FetchValues(filter, filter_elm, resolver, doc_ctx, filter_target_elm,
								vcxt, override_targetbbox));

	// Determine limits (consider doing this based on painting
	// context, rt-size, instead).
	int max_width = INT_MAX;
	int max_height = INT_MAX;
	SVGUtils::LimitCanvasSize(doc_ctx->GetDocument(), doc_ctx->GetVisualDevice(),
							  max_width, max_height);
	filter->m_max_res_width = max_width;
	filter->m_max_res_height = max_height;

	// Need these for <feImage>
	filter->m_doc_ctx = doc_ctx;
	filter->m_resolver = resolver;

	// Setup the input nodes
	filter->SetupInputNodes();

	SVGLogicalTreeChildIterator ltci;
	SVGFilterTraversalObject filter_object(&ltci, filter);
	filter_object.SetupResolver(resolver);
	filter_object.SetDocumentContext(doc_ctx);

	SVGNumberPair vp(vcxt.viewport_width, vcxt.viewport_height);
	filter_object.SetCurrentViewport(vp);

	// This traversal object should be able to do it's job without a canvas
	filter_object.SetCanvas(NULL);

	RETURN_IF_ERROR(SVGTraverser::Traverse(&filter_object, filter->GetChildRoot(), NULL));

	// Transfer state from the canvas if needed
	if (filter->ReferencesInputNode(INPUTNODE_FILLPAINT))
	{
		SVGPaintDesc fillpaint;
		fillpaint.color = canvas->GetActualFillColor();
		fillpaint.opacity = canvas->GetFillOpacity();
		fillpaint.pserver = canvas->GetFillPaintServer();

		filter->m_input_nodes[INPUTNODE_FILLPAINT].SetPaint(fillpaint);
	}
	if (filter->ReferencesInputNode(INPUTNODE_STROKEPAINT))
	{
		SVGPaintDesc strokepaint;
		strokepaint.color = canvas->GetActualStrokeColor();
		strokepaint.opacity = canvas->GetStrokeOpacity();
		strokepaint.pserver = canvas->GetStrokePaintServer();

		filter->m_input_nodes[INPUTNODE_STROKEPAINT].SetPaint(strokepaint);
	}
	if (filter->ReferencesInputNode(INPUTNODE_BACKGROUNDIMAGE) ||
		filter->ReferencesInputNode(INPUTNODE_BACKGROUNDALPHA))
	{
		doc_ctx->SetNeedsBackgroundLayers();
	}

	return OpStatus::OK;
}

SVGNumber SVGFilter::GetResolvedNumber(HTML_Element* elm, Markup::AttrType attr_type, SVGLength::LengthOrientation type, BOOL apply_offset)
{
	SVGNumber result;
	OpStatus::Ignore(AttrValueStore::GetNumber(elm, attr_type, result)); // this is assuming that 0 is always the default
	ResolvePrimitiveUnits(result, type, apply_offset);
	return result;
}

SVGNumberPair SVGFilter::GetResolvedNumberOptionalNumber(HTML_Element* elm, Markup::AttrType attr_type, const SVGNumber& default_value)
{
	SVGNumberPair result;
	OpStatus::Ignore(SVGUtils::GetNumberOptionalNumber(elm, attr_type, result, default_value));
	ResolvePrimitiveUnits(result.x, SVGLength::SVGLENGTH_X, FALSE);
	ResolvePrimitiveUnits(result.y, SVGLength::SVGLENGTH_Y, FALSE);
	return result;
}

void SVGFilter::ResolvePrimitiveUnits(SVGNumber& io_val, SVGLength::LengthOrientation type, BOOL apply_offset)
{
	if(m_primitive_units != SVGUNITS_OBJECTBBOX)
		return;

	SVGNumber multiplier;
	if (type == SVGLength::SVGLENGTH_X)
		multiplier = m_elm_bbox.Width();
	else if (type == SVGLength::SVGLENGTH_Y)
		multiplier = m_elm_bbox.Height();
	else
	{
		SVGNumber w = m_elm_bbox.Width();
		SVGNumber h = m_elm_bbox.Height();
		multiplier = (w*w+h*h).sqrt() / SVGNumber(1.414214); // sqrt(2)
	}

	io_val *= multiplier;

	if (apply_offset)
	{
		if (type == SVGLength::SVGLENGTH_X)
			io_val += m_elm_bbox.minx;
		else if (type == SVGLength::SVGLENGTH_Y)
			io_val += m_elm_bbox.miny;
	}
}

void SVGFilter::SetupInputNodes()
{
	for (unsigned node_num = 0; node_num < NUM_INPUTNODES; ++node_num)
	{
		SVGSourceImageFilterNode* node = m_input_nodes + node_num;
		node->SetRegion(m_region);

		BOOL alpha_only = node_num == INPUTNODE_SOURCEALPHA || node_num == INPUTNODE_BACKGROUNDALPHA;
		node->SetAlphaOnly(alpha_only);

		node->SetNodeId(node_num);

		switch (node_num)
		{
		case INPUTNODE_SOURCEGRAPHIC:
		case INPUTNODE_SOURCEALPHA:
			// Set as 'source'
			node->SetType(SVGSourceImageFilterNode::SOURCE);
			break;

		case INPUTNODE_BACKGROUNDIMAGE:
		case INPUTNODE_BACKGROUNDALPHA:
			// Set as 'background'
			node->SetType(SVGSourceImageFilterNode::BACKGROUND);
			break;

		case INPUTNODE_FILLPAINT:
			// Set as 'paint' (should also set the paint as some point)
			node->SetType(SVGSourceImageFilterNode::FILLPAINT);
			break;

		case INPUTNODE_STROKEPAINT:
			// Set as 'paint' (should also set the paint as some point)
			node->SetType(SVGSourceImageFilterNode::STROKEPAINT);
			break;

		default:
			OP_ASSERT(!"Unknown input node");
		}
	}
}

SVGBoundingBox SVGFilter::GetFilteredExtents(const SVGBoundingBox& extents, SVGPaintNode* context_node)
{
	if (m_nodes.GetCount() == 0)
		return SVGBoundingBox();

	// If the author specified a filterRes with a zero dimension, or
	// the bounding box is empty and filterUnits=objectBoundingBox -
	// there is no work to be performed.
	if ((m_custom_filter_res && (m_filter_res_w <= 0 || m_filter_res_h <= 0)) ||
		(m_elm_bbox.IsEmpty() && m_filter_units == SVGUNITS_OBJECTBBOX))
		return SVGBoundingBox();

	unsigned int num_prims = m_nodes.GetCount();
	SVGBoundingBox* filternode_extents = OP_NEWA(SVGBoundingBox, num_prims + NUM_INPUTNODES);
	if (!filternode_extents)
		return extents;

	OpAutoArray<SVGBoundingBox> extents_cleaner(filternode_extents);

	SVGFilterContext context(context_node);
	context.SetRegion(m_region);
	context.SetupForExtentCalculation();

	SVGFilterNode* start_node = m_nodes.Get(num_prims - 1);

	SVGStack<SVGFilterNode> eval_stack;
	RETURN_VALUE_IF_ERROR(eval_stack.Push(start_node), extents);

	// Starting extents
	filternode_extents[start_node->GetNodeId()] = extents;

	while (!eval_stack.IsEmpty())
	{
		SVGFilterNode* node = eval_stack.Pop();
		if (!node)
			break;

		// Calculate additional area for this node
		SVGBoundingBox& node_extents = filternode_extents[node->GetNodeId()];
		SVGFilterNode::ExtentModifier extmod;
		node->GetExtentModifier(&context, extmod);
		extmod.Apply(node_extents);
		node_extents.IntersectWith(m_region);

		// Process dependencies
		SVGFilterNode** inputs = node->GetInputNodes();
		unsigned input_count = node->GetInputCount();

		OP_STATUS result = OpStatus::OK;

		for (unsigned input_num = 0; input_num < input_count; ++input_num)
		{
			SVGFilterNode* inp_node = inputs[input_num];

			// Put the dependency on the stack for further processing
			result = eval_stack.Push(inp_node);
			if (OpStatus::IsError(result))
				break;

			// Propagate area from referring node
			SVGBoundingBox& inp_extents = filternode_extents[inp_node->GetNodeId()];
			inp_extents.UnionWith(node_extents);
			inp_extents.IntersectWith(m_region);
		}

		if (OpStatus::IsError(result))
			eval_stack.Clear();
	}
	return filternode_extents[start_node->GetNodeId()];
}

OP_STATUS SVGFilter::Apply(SVGPainter* painter, SVGPaintNode* context_node)
{
	SVG_PROBE_START("SVGFilter::Apply");

	if (m_nodes.GetCount() == 0)
		return OpStatus::OK;

	// If the author specified a filterRes with a zero dimension, or
	// the bounding box is empty and filterUnits=objectBoundingBox -
	// there is no work to be performed.
	if ((m_custom_filter_res && (m_filter_res_w <= 0 || m_filter_res_h <= 0)) ||
		(m_elm_bbox.IsEmpty() && m_filter_units == SVGUNITS_OBJECTBBOX))
		return OpStatus::OK;

	OP_STATUS result = OpStatus::OK;

	unsigned int buffer_width = 0;
	unsigned int buffer_height = 0;

	if (m_custom_filter_res)
	{
		buffer_width = m_filter_res_w.GetIntegerValue();
		buffer_height = m_filter_res_h.GetIntegerValue();
	}

	SVGFilterContext context(context_node, painter);
	context.SetRegion(m_region);

	RETURN_IF_ERROR(context.NegotiateBufferSize(buffer_width, buffer_height,
												m_max_res_width, m_max_res_height));
	RETURN_IF_ERROR(context.Construct(buffer_width, buffer_height));

	unsigned int num_prims = m_nodes.GetCount();
	RETURN_IF_ERROR(context.SetStoreSize(num_prims + NUM_INPUTNODES));

	SVGFilterNode* start_node = m_nodes.Get(num_prims - 1);

	RETURN_IF_ERROR(context.Analyse(start_node));
	RETURN_IF_ERROR(context.Evaluate(start_node));

	// Get result and put it on painter
	OpRect src_rect;
	SVGRect result_rect;
	VEGARenderTarget* result_rt = NULL;
	RETURN_IF_ERROR(context.GetResult(start_node, result_rt, src_rect, result_rect));

	if (result_rt)
	{
		SVGPainter::ImageRenderQuality use_quality = SVGPainter::IMAGE_QUALITY_LOW;

		BOOL need_hq =
			(UseInterpolation() && m_image_rendering == SVGIMAGERENDERING_OPTIMIZEQUALITY) ||
			(m_image_rendering != SVGIMAGERENDERING_OPTIMIZESPEED &&
			 !painter->GetTransform().IsAlignedAndNonscaled());

		if (need_hq)
			use_quality = SVGPainter::IMAGE_QUALITY_NORMAL;

		painter->DrawImage(result_rt, src_rect, result_rect, use_quality);
	}

	SVG_PROBE_END();
	return result;
}

#endif // SVG_SUPPORT_FILTERS
#endif // SVG_SUPPORT
