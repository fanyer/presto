/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"

#ifdef SVG_SUPPORT_FILTERS

#include "modules/svg/svg_number.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGVector.h"
#include "modules/svg/src/svgfilternode.h"
#include "modules/svg/src/SVGFilter.h"
#include "modules/svg/src/SVGTurbulenceGenerator.h"
#include "modules/svg/src/svgpainter.h"
#include "modules/svg/src/svgpaintnode.h"

#include "modules/libvega/vegapath.h"
#include "modules/libvega/vegafill.h"
#include "modules/libvega/vegarenderer.h"
#include "modules/libvega/vegatransform.h"

#include "modules/libvega/vegafilter.h"

void SVGFilterContext::ClearNodeInfo()
{
	if (m_nodeinfo == NULL)
		return;

	for (unsigned int i = 0; i < m_num_nodes; ++i)
	{
		OP_ASSERT((m_nodeinfo[i].target && m_nodeinfo[i].ref) ||
				  (!m_nodeinfo[i].target && !m_nodeinfo[i].ref));
		VEGARenderTarget::Destroy(m_nodeinfo[i].target);
		m_nodeinfo[i].target = NULL;
		m_nodeinfo[i].ref = 0;
	}
}

OP_STATUS SVGFilterContext::SetStoreSize(unsigned int size)
{
	if (size <= m_num_nodes)
		return OpStatus::OK;

	OP_DELETEA(m_nodeinfo);
	m_num_nodes = 0;

	m_nodeinfo = OP_NEWA(SVGFilterNodeInfo, size);
	if (!m_nodeinfo)
		return OpStatus::ERR_NO_MEMORY;

	m_num_nodes = size;

	for (unsigned int i = 0; i < m_num_nodes; ++i)
	{
		m_nodeinfo[i].target = NULL;
		m_nodeinfo[i].ref = 0;
	}
	return OpStatus::OK;
}

SVGFilterNodeInfo* SVGFilterContext::GetNodeInfo(SVGFilterNode* node)
{
	OP_ASSERT((unsigned)node->GetNodeId() < m_num_nodes);
	return m_nodeinfo + node->GetNodeId();
}

void SVGFilterContext::DecRef(SVGFilterNode* ref)
{
	unsigned int idx = ref->GetNodeId();
	if (--m_nodeinfo[idx].ref == 0)
	{
		VEGARenderTarget::Destroy(m_nodeinfo[idx].target);
		m_nodeinfo[idx].target = NULL;
	}
}

void SVGFilterContext::CalculateClipIntersection(SVGBoundingBox& bbox)
{
	OpRect hcrect = m_painter->GetClipRect();
	const SVGMatrix& ctm = m_painter->GetTransform();
	SVGNumberPair p0 = ctm.ApplyToCoordinate(m_filter_region.x, m_filter_region.y);
	SVGNumberPair p1 = ctm.ApplyToCoordinate(m_filter_region.x+m_filter_region.width, m_filter_region.y);
	SVGNumberPair p2 = ctm.ApplyToCoordinate(m_filter_region.x, m_filter_region.y+m_filter_region.height);

	SVGNumberPair e1(p1.x - p0.x, p1.y - p0.y);
	SVGNumberPair e2(p2.x - p0.x, p2.y - p0.y);

	SVGNumber e1_sqrlen = SVGNumberPair::Dot(e1, e1);
	SVGNumber e2_sqrlen = SVGNumberPair::Dot(e2, e2);

	// Calculate coordinates in the coordinate system defined by the basis {e1, e2}
	SVGNumberPair hcp;
	for (unsigned int i = 0; i < 4; ++i)
	{
		if (i == 0 || i == 3)
		{
			hcp.x = SVGNumber(hcrect.x) - p0.x;
		}
		else
		{
			hcp.x = SVGNumber(hcrect.x+hcrect.width) - p0.x;
		}
		if (i == 0 || i == 1)
		{
			hcp.y = SVGNumber(hcrect.y) - p0.y;
		}
		else
		{
			hcp.y = SVGNumber(hcrect.y+hcrect.height) - p0.y;
		}
		SVGNumber cx = (hcp.x * e1.x + hcp.y * e1.y) / e1_sqrlen;
		SVGNumber cy = (hcp.x * e2.x + hcp.y * e2.y) / e2_sqrlen;

		bbox.minx = SVGNumber::min_of(bbox.minx, cx);
		bbox.maxx = SVGNumber::max_of(bbox.maxx, cx);
		bbox.miny = SVGNumber::min_of(bbox.miny, cy);
		bbox.maxy = SVGNumber::max_of(bbox.maxy, cy);
	}

	// Calculate the intersection of bbox and 'the unit box' (0,0,1,1)
	bbox.minx = SVGNumber::max_of(bbox.minx, 0);
	bbox.miny = SVGNumber::max_of(bbox.miny, 0);
	bbox.maxx = SVGNumber::min_of(bbox.maxx, 1);
	bbox.maxy = SVGNumber::min_of(bbox.maxy, 1);
}

void SVGFilterContext::ComputeTransform(unsigned int buffer_width, unsigned int buffer_height)
{
	m_buffer_xfrm.LoadIdentity();
	m_buffer_xfrm.MultTranslation(-m_filter_region.x, -m_filter_region.y);
	m_buffer_xfrm.MultScale(SVGNumber(buffer_width) / m_filter_region.width,
							SVGNumber(buffer_height) / m_filter_region.height);
}

OP_STATUS SVGFilterContext::NegotiateBufferSize(unsigned int& buffer_width, unsigned int& buffer_height,
												unsigned int max_buf_width, unsigned int max_buf_height)
{
	const SVGMatrix& mat = m_painter->GetTransform();

	if (buffer_height == 0 && buffer_width == 0)
	{
		// Calculate default filter resolution
		SVGNumber expfact = mat.GetExpansionFactor();

		// Attempt to get a buffer resolution that as closely as
		// possible matches the size that the filter result will have
		// when drawn on the canvas

		// If the expansionfactor is smaller than either of the
		// ("linear") scalefactors, then the scaling is probably
		// 'disproportionate' (for example sx >> sy). In that case use
		// the scaled version of the region to determine the
		// buffersize
		if (expfact < mat[0].abs() || expfact < mat[3].abs())
		{
			// canvas transform * (region width x region height)
			buffer_width = (m_filter_region.width * mat[0] +
							m_filter_region.height * mat[2]).abs().ceil().GetIntegerValue();
			buffer_height = (m_filter_region.width * mat[1] +
							 m_filter_region.height * mat[3]).abs().ceil().GetIntegerValue();
		}
		else
		{
			// => current canvas scale * (region width x region height)
			buffer_width = (m_filter_region.width * expfact).ceil().GetIntegerValue();
			buffer_height = (m_filter_region.height * expfact).ceil().GetIntegerValue();
		}
	}

	if (buffer_height == 0 || buffer_width == 0)
		return OpStatus::ERR;

	OP_ASSERT(max_buf_width > 0 && max_buf_height > 0);

	// Bold rescue attempt if too large
	if (buffer_width * buffer_height > max_buf_width * max_buf_height ||
		(buffer_width > max_buf_width || buffer_height > max_buf_height))
	{
		int w = buffer_width;
		int h = buffer_height;
		SVGUtils::ShrinkToFit(w, h, max_buf_width, max_buf_height);
		buffer_width = w;
		buffer_height = h;
	}
	return OpStatus::OK;
}

OP_STATUS SVGFilterContext::Construct(unsigned int buffer_width, unsigned int buffer_height)
{
	OP_ASSERT(m_painter);

	ComputeTransform(buffer_width, buffer_height);

	m_total_area.Set(0, 0, buffer_width, buffer_height);

	SVGBoundingBox bbox;
	CalculateClipIntersection(bbox);

	SVGRect clipped_region(m_filter_region);
	clipped_region.x += bbox.minx * m_filter_region.width;
	clipped_region.y += bbox.miny * m_filter_region.height;
	clipped_region.width = (bbox.maxx - bbox.minx) * m_filter_region.width;
	clipped_region.height = (bbox.maxy - bbox.miny) * m_filter_region.height;

	m_clipped_area = ResolveArea(clipped_region);
	m_clipped_area.IntersectWith(m_total_area); // Paranoia

	if (m_clipped_area.IsEmpty())
		return OpStatus::ERR;

	return m_renderer.Init(buffer_width, buffer_height);
}

void SVGFilterContext::SetupForExtentCalculation()
{
	m_buffer_xfrm.LoadTranslation(-m_filter_region.x, -m_filter_region.y);
}

OpRect SVGFilterContext::ResolveArea(const SVGRect& region)
{
	// Pass the region through the buffer transform
	SVGRect xlat_region = m_buffer_xfrm.ApplyToRect(region);
	OpRect xlat_pix = xlat_region.GetEnclosingRect();
	return xlat_pix;
}

SVGFilterNodeInfo* SVGFilterContext::GetImage(SVGFilterNode* context_node, SVGFilterNode* ref)
{
	SVGFilterNodeInfo* fi = GetNodeInfo(ref);
	if (fi)
		// Make it the correct colorspace
		ConvertSurface(fi, context_node->GetColorSpace());

	return fi;
}

SVGFilterNodeInfo* SVGFilterContext::GetSurfaceDirty(SVGFilterNode* node)
{
	SVGFilterNodeInfo* nodeinfo = GetNodeInfo(node);
	OP_ASSERT(nodeinfo && nodeinfo->target == NULL);

	RETURN_VALUE_IF_ERROR(m_renderer.createIntermediateRenderTarget(&nodeinfo->target,
																	nodeinfo->area.width,
																	nodeinfo->area.height), NULL);

	nodeinfo->colorspace = node->GetColorSpace();

	return nodeinfo;
}

SVGFilterNodeInfo* SVGFilterContext::GetSurface(SVGFilterNode* node)
{
	SVGFilterNodeInfo* nodeinfo = GetSurfaceDirty(node);

	if (nodeinfo)
		ClearSurface(nodeinfo, nodeinfo->area);

	return nodeinfo;
}

OP_STATUS SVGFilterContext::BuildConversionFilters()
{
	if (m_lin_to_srgb && m_srgb_to_lin && m_s2l_tab)
		return OpStatus::OK; // Already built

	OP_DELETE(m_lin_to_srgb);
	m_lin_to_srgb = NULL;
	OP_DELETE(m_srgb_to_lin);
	m_srgb_to_lin = NULL;
	OP_DELETEA(m_s2l_tab);
	m_s2l_tab = NULL;

	m_s2l_tab = OP_NEWA(UINT8, 256);
	if (!m_s2l_tab)
		return OpStatus::ERR_NO_MEMORY;

	// sRGB -> linearRGB table for colors
	for (unsigned int i = 0; i < 256; ++i)
	{
		SVGNumber sval(i);
		sval /= 255;

		SVGNumber lval;
		if (sval > SVGNumber(0.04045))
		{
			lval = ((sval + SVGNumber(0.055)) / SVGNumber(1.055)).pow(SVGNumber(2.4));
		}
		else
		{
			lval = sval / SVGNumber(12.92);
		}

		m_s2l_tab[i] = (unsigned char)(lval*255).GetRoundedIntegerValue();
	}

	OP_STATUS status = m_renderer.createColorSpaceConversionFilter(&m_lin_to_srgb,
																   VEGACOLORSPACE_LINRGB,
																   VEGACOLORSPACE_SRGB);
	RETURN_IF_ERROR(status);
	status = m_renderer.createColorSpaceConversionFilter(&m_srgb_to_lin,
														 VEGACOLORSPACE_SRGB,
														 VEGACOLORSPACE_LINRGB);
	return status;
}

void SVGFilterContext::ConvertSurface(SVGFilterNodeInfo* nodeinfo, ColorSpaceType new_cstype)
{
	// Hmm, maybe allocating the tables statically isn't such a bad idea?
	RETURN_VOID_IF_ERROR(BuildConversionFilters());

	if (new_cstype != nodeinfo->colorspace)
	{
		VEGAFilter* csfilter = new_cstype == COLORSPACE_LINEARRGB ? m_srgb_to_lin : m_lin_to_srgb;

		m_renderer.setRenderTarget(nodeinfo->target);
		csfilter->setSource(nodeinfo->target);

		VEGAFilterRegion fregion;
		fregion.sx = fregion.dx = 0;
		fregion.sy = fregion.dy = 0;
		fregion.width = nodeinfo->area.width;
		fregion.height = nodeinfo->area.height;

		OpStatus::Ignore(m_renderer.applyFilter(csfilter, fregion));

		nodeinfo->colorspace = new_cstype;
	}
}

UINT32 SVGFilterContext::ConvertColor(UINT32 color, ColorSpaceType new_cstype)
{
	// colors assumed to be in sRGB
	if (new_cstype == COLORSPACE_LINEARRGB &&
		OpStatus::IsSuccess(BuildConversionFilters()))
	{
		unsigned int fr = m_s2l_tab[(color >> 16) & 0xff];
		unsigned int fg = m_s2l_tab[(color >> 8) & 0xff];
		unsigned int fb = m_s2l_tab[color & 0xff];

		color = (color & 0xff000000) | (fr << 16) | (fg << 8) | fb;
	}

	return color;
}

void SVGFilterContext::ConvertToAlphaSurface(SVGFilterNodeInfo* src, SVGFilterNodeInfo* dst,
											 const OpRect& conv_area)
{
	OpRect area(conv_area);
	area.IntersectWith(src->area);
	area.IntersectWith(dst->area);

	if (!area.IsEmpty())
	{
		VEGAFilter* alpha_copy = NULL;
		RETURN_VOID_IF_ERROR(m_renderer.createMergeFilter(&alpha_copy, VEGAMERGE_REPLACE));

		alpha_copy->setSource(src->target, true /* copy alpha only */);

		VEGAFilterRegion fregion;
		fregion.sx = area.x - src->area.x;
		fregion.sy = area.y - src->area.y;
		fregion.dx = area.x - dst->area.x;
		fregion.dy = area.y - dst->area.y;
		fregion.width = area.width;
		fregion.height = area.height;

		m_renderer.setRenderTarget(dst->target);

		OpStatus::Ignore(m_renderer.applyFilter(alpha_copy, fregion));

		OP_DELETE(alpha_copy);

		ClearSurfaceInv(dst, area);
	}
	else
	{
		ClearSurface(dst, dst->area);
	}
}

SVGFilterContext::~SVGFilterContext()
{
	ClearNodeInfo();
	OP_DELETEA(m_nodeinfo);

	OP_DELETE(m_lin_to_srgb);
	OP_DELETE(m_srgb_to_lin);
	OP_DELETEA(m_s2l_tab);
}

OP_STATUS SVGFilterContext::CopySurface(SVGFilterNodeInfo* dst, SVGFilterNodeInfo* src,
										const OpRect& src_area, VEGARenderer* renderer /* = NULL */)
{
	OpRect target_rect(src_area);
	if (target_rect.IsEmpty())
		target_rect = src->area;

	target_rect.IntersectWith(dst->area);

	if (target_rect.IsEmpty())
		return OpStatus::OK;

	if (renderer == NULL)
		renderer = &m_renderer;

	VEGAFilter* merge;
	RETURN_IF_ERROR(renderer->createMergeFilter(&merge, VEGAMERGE_REPLACE));

	renderer->setRenderTarget(dst->target);
	merge->setSource(src->target);

	VEGAFilterRegion region;
	region.dx = target_rect.x - dst->area.x;
	region.dy = target_rect.y - dst->area.y;
	region.sx = target_rect.x - src->area.x;
	region.sy = target_rect.y - src->area.y;
	region.width = target_rect.width;
	region.height = target_rect.height;

	OP_STATUS res = renderer->applyFilter(merge, region);

	OP_DELETE(merge);

	return res;
}

void SVGFilterContext::ClearSurface(SVGFilterNodeInfo* nodeinfo, OpRect cl_rect,
									UINT32 color_value /* = 0 */,
									VEGARenderer* renderer /* = NULL */)
{
	cl_rect.IntersectWith(nodeinfo->area);

	if (cl_rect.IsEmpty())
		return;

	cl_rect.OffsetBy(-nodeinfo->area.TopLeft());

	UINT32 clearcolor =
		(GetSVGColorAValue(color_value) << 24) |
		(GetSVGColorRValue(color_value) << 16) |
		(GetSVGColorGValue(color_value) << 8) |
		GetSVGColorBValue(color_value);

	if (renderer == NULL)
		renderer = &m_renderer;

	renderer->setRenderTarget(nodeinfo->target);
	renderer->setClipRect(cl_rect.x, cl_rect.y, cl_rect.width, cl_rect.height);
	renderer->clear(cl_rect.x, cl_rect.y, cl_rect.width, cl_rect.height, clearcolor);
}

void SVGFilterContext::ClearSurfaceInv(SVGFilterNodeInfo* nodeinfo, const OpRect& inv_rect)
{
	if (inv_rect.Equals(nodeinfo->area))
		return;

	if (inv_rect.Top() - nodeinfo->area.Top() > 0)
	{
		OpRect rect(nodeinfo->area.x, nodeinfo->area.y,
					nodeinfo->area.width, inv_rect.Top() - nodeinfo->area.Top());
		ClearSurface(nodeinfo, rect);
	}
	if (nodeinfo->area.Bottom() - inv_rect.Bottom() > 0)
	{
		OpRect rect(nodeinfo->area.x, inv_rect.Bottom(),
					nodeinfo->area.width, nodeinfo->area.Bottom() - inv_rect.Bottom());
		ClearSurface(nodeinfo, rect);
	}
	if (inv_rect.Left() - nodeinfo->area.Left() > 0)
	{
		OpRect rect(nodeinfo->area.x, inv_rect.y,
					inv_rect.Left() - nodeinfo->area.Left(), nodeinfo->area.height);
		ClearSurface(nodeinfo, rect);
	}
	if (nodeinfo->area.Right() - inv_rect.Right() > 0)
	{
		OpRect rect(inv_rect.Right(), inv_rect.y,
					nodeinfo->area.Right() - inv_rect.Right(), inv_rect.height);
		ClearSurface(nodeinfo, rect);
	}
}

OP_STATUS SVGFilterContext::GetResult(SVGFilterNode* node,
									  VEGARenderTarget*& res_rt, OpRect& src, SVGRect& region)
{
	// Get surface/result for node
	SVGFilterNodeInfo* nodeinfo = GetNodeInfo(node);
	if (!nodeinfo->target)
		return OpStatus::ERR;

	// m_visible_area vs. surf->area
	src.x = m_clipped_area.x - nodeinfo->area.x;
	src.y = m_clipped_area.y - nodeinfo->area.y;
	src.width = m_clipped_area.width;
	src.height = m_clipped_area.height;

	region.x = m_filter_region.x + SVGNumber(m_clipped_area.x - m_total_area.x) / m_buffer_xfrm[0];
	region.y = m_filter_region.y + SVGNumber(m_clipped_area.y - m_total_area.y) / m_buffer_xfrm[3];
	region.width = SVGNumber(m_clipped_area.width) / m_buffer_xfrm[0];
	region.height = SVGNumber(m_clipped_area.height) / m_buffer_xfrm[3];

	// Result in sRGB
	ConvertSurface(nodeinfo, COLORSPACE_SRGB);

	res_rt = nodeinfo->target;
	return OpStatus::OK;
}

void SVGFilterNode::ExtentModifier::Apply(OpRect& modarea)
{
	switch (type)
	{
	case IDENTITY:
		break;

	case EXTEND:
		{
			int extend_x = extend.x.GetRoundedIntegerValue();
			int extend_y = extend.y.GetRoundedIntegerValue();

			modarea = modarea.InsetBy(-extend_x, -extend_y);
		}
		break;

	case EXTEND_MIN_1PX:
		{
			int extend_x = SVGNumber::max_of(extend.x, SVGNumber(1)).GetRoundedIntegerValue();
			int extend_y = SVGNumber::max_of(extend.y, SVGNumber(1)).GetRoundedIntegerValue();

			extend_x = MAX(1, extend_x);
			extend_y = MAX(1, extend_y);

			modarea = modarea.InsetBy(-extend_x, -extend_y);
		}
		break;

	case AREA:
		modarea.UnionWith(area.GetEnclosingRect());
		break;
	}
}

void SVGFilterNode::ExtentModifier::Apply(SVGBoundingBox& extents)
{
	switch (type)
	{
	case IDENTITY:
		break;

	case EXTEND:
		{
			int extend_x = extend.x.GetRoundedIntegerValue();
			int extend_y = extend.y.GetRoundedIntegerValue();

			extents.Extend(extend_x, extend_y);
		}
		break;

	case EXTEND_MIN_1PX:
		{
			// Maybe this is a bit overzealous
			SVGNumber extend_x = SVGNumber::max_of(extend.x, SVGNumber(1));
			SVGNumber extend_y = SVGNumber::max_of(extend.y, SVGNumber(1));

			extents.Extend(extend_x, extend_y);
		}
		break;

	case AREA:
		extents.UnionWith(area);
		break;
	}
}

void SVGBinaryFilterNode::ResolveColorSpace(SVGColorInterpolation colorinterp)
{
	if (colorinterp == SVGCOLORINTERPOLATION_AUTO)
	{
		// If both are 'linearRGB' use that - otherwise use 'sRGB'
		if (m_input[0]->GetColorSpace() == COLORSPACE_LINEARRGB &&
			m_input[1]->GetColorSpace() == COLORSPACE_LINEARRGB)
			m_colorspace = COLORSPACE_LINEARRGB;
		else
			m_colorspace = COLORSPACE_SRGB;
	}
	else
	{
		SVGFilterNode::ResolveColorSpace(colorinterp);
	}
}

void SVGBinaryFilterNode::CalculateDefaultSubregion(const SVGRect& filter_region)
{
	m_region.UnionWith(m_input[0]->GetRegion());
	m_region.UnionWith(m_input[1]->GetRegion());

	if (m_region.IsEmpty())
		m_region = filter_region;
}

OP_STATUS SVGFilterContext::Analyse(SVGFilterNode* start_node)
{
	SVGStack<SVGFilterNode> eval_stack;
	RETURN_IF_ERROR(eval_stack.Push(start_node));

	// Calculate area
	SVGFilterNodeInfo* start_nodeinfo = GetNodeInfo(start_node);
	start_nodeinfo->area = m_clipped_area;

	OP_STATUS result = OpStatus::OK;
	while (!eval_stack.IsEmpty())
	{
		SVGFilterNode* node = eval_stack.Pop();
		if (!node)
			break;

		// Increase reference count
		IncRef(node);

		// Calculate additional area for this node
		SVGFilterNodeInfo* nodeinfo = GetNodeInfo(node);
		SVGFilterNode::ExtentModifier extmod;
		node->GetExtentModifier(this, extmod);
		extmod.Apply(nodeinfo->area);
		nodeinfo->area.IntersectWith(m_total_area);

		// Process dependencies
		SVGFilterNode** inputs = node->GetInputNodes();
		unsigned input_count = node->GetInputCount();

		for (unsigned input_num = 0; input_num < input_count; ++input_num)
		{
			SVGFilterNode* inp_node = inputs[input_num];

			// Put the dependency on the stack for further processing
			result = eval_stack.Push(inp_node);
			if (OpStatus::IsError(result))
				break;

			// Propagate area from referring node
			SVGFilterNodeInfo* inp_nodeinfo = GetNodeInfo(inp_node);
			inp_nodeinfo->area.UnionWith(nodeinfo->area);
			inp_nodeinfo->area.IntersectWith(m_total_area);
		}

		if (OpStatus::IsError(result))
			eval_stack.Clear();
	}
	return result;
}

OP_STATUS SVGFilterContext::Evaluate(SVGFilterNode* start_node)
{
	SVGStack<SVGFilterNode> eval_stack;

	RETURN_IF_ERROR(eval_stack.Push(start_node));

	OP_STATUS status = OpStatus::OK;

	while (!eval_stack.IsEmpty())
	{
		SVGFilterNode* node = eval_stack.Top();
		if (!node)
			break;

		BOOL missing_refs = FALSE;

		if (HasResult(node))
		{
			// The primitive has already been evaluated
			eval_stack.Pop();
		}
		else
		{
			SVGFilterNode** inputs = node->GetInputNodes();
			unsigned input_count = node->GetInputCount();

			for (unsigned input_num = 0; input_num < input_count; ++input_num)
			{
				if (HasResult(inputs[input_num]))
					continue;

				// Intermediate result
				status = eval_stack.Push(inputs[input_num]);

				missing_refs = TRUE;
			}
			if (!missing_refs)
			{
				// All sources to this filter are available
				eval_stack.Pop();

				status = node->Apply(this);
			}

			if (OpStatus::IsError(status))
				eval_stack.Clear();
		}
	}
	return status;
}
// --

OP_STATUS SVGScaledUnaryFilterNode::ScaleSurface(VEGARenderer* dst_r,
												 SVGFilterNodeInfo* dst, SVGFilterNodeInfo* src)
{
	if (!dst || !src)
		return OpStatus::ERR;

	dst_r->setRenderTarget(dst->target);

	// Setup source image
	VEGATransform inv_xfrm;
	inv_xfrm.loadScale(VEGA_INTTOFIX(src->area.width) / dst->area.width,
					   VEGA_INTTOFIX(src->area.height) / dst->area.height);
	VEGATransform xfrm;
	xfrm.loadScale(VEGA_INTTOFIX(dst->area.width) / src->area.width,
				   VEGA_INTTOFIX(dst->area.height) / src->area.height);

	VEGAFill* src_img = NULL;
	RETURN_IF_ERROR(src->target->getImage(&src_img));
	src_img->setSpread(VEGAFill::SPREAD_REPEAT);
	src_img->setQuality(VEGAFill::QUALITY_NEAREST);
	src_img->setTransform(xfrm, inv_xfrm);

	dst_r->setFill(src_img);

	OpStatus::Ignore(dst_r->fillRect(0, 0, dst->area.width, dst->area.height));

	dst_r->setFill(NULL);

	return OpStatus::OK;
}

void SVGFilterNode::ResolveColorSpace(SVGColorInterpolation colorinterp)
{
	// Each class inheriting SVGFilterNode needs to decide how to
	// handle 'auto'
	OP_ASSERT(colorinterp != SVGCOLORINTERPOLATION_AUTO);

	if (colorinterp == SVGCOLORINTERPOLATION_LINEARRGB)
		m_colorspace = COLORSPACE_LINEARRGB;
	else
		m_colorspace = COLORSPACE_SRGB;
}

OP_STATUS SVGFilterNode::CommonApply(SVGFilterContext* context,
									 VEGAFilter* filter,
									 SVGFilterNodeInfo* dst, SVGFilterNodeInfo* src,
									 BOOL src_is_alpha_only,
									 BOOL do_inverted_area_clear,
									 int ofs_x, int ofs_y)
{
	OP_ASSERT(dst && src);

	OpRect xlat_pix = context->ResolveArea(m_region);
	xlat_pix.IntersectWith(dst->area);

	if (xlat_pix.IsEmpty())
		return OpStatus::OK;

	if (do_inverted_area_clear)
		context->ClearSurfaceInv(dst, xlat_pix);

	// Filter source
	filter->setSource(src->target, src_is_alpha_only ? true : false);

	VEGAFilterRegion fregion;
	fregion.sx = xlat_pix.x - src->area.x;
	fregion.sy = xlat_pix.y - src->area.y;
	fregion.dx = xlat_pix.x - dst->area.x;
	fregion.dy = xlat_pix.y - dst->area.y;
	fregion.width = xlat_pix.width;
	fregion.height = xlat_pix.height;

	if (ofs_x > 0)
		fregion.dx += ofs_x;
	else if (ofs_x < 0)
		fregion.sx -= ofs_x;

	if (ofs_y > 0)
		fregion.dy += ofs_y;
	else if (ofs_y < 0)
		fregion.sy -= ofs_y;

	VEGARenderer* renderer = context->GetRenderer();

	// Filter destination
	renderer->setRenderTarget(dst->target);

	return renderer->applyFilter(filter, fregion);
}

// Binary
void SVGBinaryFilterNode::GetExtentModifier(SVGFilterContext* context, ExtentModifier& modifier)
{
	if (m_type == DISPLACEMENT)
	{
		// FIXME/OPT: This actually applies to the source only
		const SVGMatrix& buffer_xfrm = context->GetTransform();

		SVGNumber scaled_scale = (m_displacement_scale * buffer_xfrm.GetExpansionFactor()).ceil();
		scaled_scale = scaled_scale.abs();

		modifier = ExtentModifier(ExtentModifier::EXTEND, scaled_scale, scaled_scale);
	}
}

static VEGAMergeType SVGBlendModeToVEGAMergeType(SVGBlendMode svgmode)
{
	switch (svgmode)
	{
	case SVGBLENDMODE_MULTIPLY:
		return VEGAMERGE_MULTIPLY;

	case SVGBLENDMODE_SCREEN:
		return VEGAMERGE_SCREEN;

	case SVGBLENDMODE_DARKEN:
		return VEGAMERGE_DARKEN;

	case SVGBLENDMODE_LIGHTEN:
		return VEGAMERGE_LIGHTEN;

	default:
	case SVGBLENDMODE_NORMAL:
		return VEGAMERGE_NORMAL;
	}
}

static VEGAMergeType SVGCompositeOpToVEGAMergeType(SVGCompositeOperator svgoper)
{
	switch (svgoper)
	{
	case SVGCOMPOSITEOPERATOR_IN:
		return VEGAMERGE_IN;

	case SVGCOMPOSITEOPERATOR_OUT:
		return VEGAMERGE_OUT;

	case SVGCOMPOSITEOPERATOR_ATOP:
		return VEGAMERGE_ATOP;

	case SVGCOMPOSITEOPERATOR_XOR:
		return VEGAMERGE_XOR;

	case SVGCOMPOSITEOPERATOR_ARITHMETIC:
		OP_ASSERT(FALSE);
	case SVGCOMPOSITEOPERATOR_OVER:
	default:
		return VEGAMERGE_OVER;
	}
}

OP_STATUS SVGBinaryFilterNode::Apply(SVGFilterContext* context)
{
	VEGARenderer* renderer = context->GetRenderer();

	VEGAFilter* filter = NULL;
	switch (m_type)
	{
	case BLEND:
		// In-place filtering possible (src -> dst)
		RETURN_IF_ERROR(renderer->createMergeFilter(&filter, SVGBlendModeToVEGAMergeType(m_blend.mode)));
		break;

	case COMPOSITE:
		// In-place filtering possible (src -> dst)
		if (m_composite.oper == SVGCOMPOSITEOPERATOR_ARITHMETIC)
		{
			VEGA_FIX v_k1 = m_composite_k1.GetVegaNumber();
			VEGA_FIX v_k2 = m_composite_k2.GetVegaNumber();
			VEGA_FIX v_k3 = m_composite_k3.GetVegaNumber();
			VEGA_FIX v_k4 = m_composite_k4.GetVegaNumber();

			RETURN_IF_ERROR(renderer->createArithmeticMergeFilter(&filter, v_k1, v_k2, v_k3, v_k4));
		}
		else
		{
			RETURN_IF_ERROR(renderer->createMergeFilter(&filter, SVGCompositeOpToVEGAMergeType(m_composite.oper)));
		}
		break;

	case DISPLACEMENT:
		return ApplyDisplacementMap(context);

	default:
		OP_ASSERT(!"Unknown filter type - memory corruption?");
		return OpStatus::OK;
	}

	OpAutoPtr<VEGAFilter> filter_cleaner(filter);

	// Ok to get a dirty surface, we copy src to it anyway
	SVGFilterNodeInfo* dst = context->GetSurfaceDirty(this);
	if (!dst)
		return OpStatus::ERR_NO_MEMORY;

	SVGFilterNodeInfo* src = context->GetImage(this, m_input[0]);
	SVGFilterNodeInfo* src2 = context->GetImage(this, m_input[1]);
	if (!src || !src2)
		return OpStatus::OK;

	OpRect dst_area = context->ResolveArea(m_region);
	if (m_input[0]->IsAlphaOnly())
	{
		// DST is an alpha surface
		context->ConvertToAlphaSurface(src, dst, dst_area);
	}
	else
	{
		RETURN_IF_ERROR(context->CopySurface(dst, src, dst_area));
	}

	SVGFilterNodeInfo src_alpha;
	src_alpha.target = NULL;

	OP_STATUS status = OpStatus::OK;

	if (m_input[1]->IsAlphaOnly())
	{
		// SRC is an alpha surface
		src_alpha.area = src2->area;

		status = renderer->createIntermediateRenderTarget(&src_alpha.target,
														  src_alpha.area.width,
														  src_alpha.area.height);

		if (OpStatus::IsSuccess(status))
		{
			context->ConvertToAlphaSurface(src2, &src_alpha, src2->area);

			src2 = &src_alpha;
		}
	}

	status = CommonApply(context, filter, dst, src2, FALSE, TRUE);

	context->DecRef(m_input[0]);
	context->DecRef(m_input[1]);

	VEGARenderTarget::Destroy(src_alpha.target);
	return status;
}

static VEGAComponent SVGDispSelToVEGAComp(SVGDisplacementSelector svgdispsel)
{
	switch (svgdispsel)
	{
	default:
	case SVGDISPLACEMENT_A:
		return VEGACOMP_A;

	case SVGDISPLACEMENT_R:
		return VEGACOMP_R;

	case SVGDISPLACEMENT_G:
		return VEGACOMP_G;

	case SVGDISPLACEMENT_B:
		return VEGACOMP_B;
	}
}

OP_STATUS SVGBinaryFilterNode::ApplyDisplacementMap(SVGFilterContext* context)
{
	SVGFilterNodeInfo* dst = context->GetSurfaceDirty(this);
	if (!dst)
		return OpStatus::ERR_NO_MEMORY;

	SVGFilterNodeInfo* src = context->GetImage(this, m_input[0]);

	const SVGMatrix& buffer_xfrm = context->GetTransform();
	SVGNumber scale = m_displacement_scale * buffer_xfrm.GetExpansionFactor();

	OpRect xlat_pix = context->ResolveArea(m_region);
	VEGARenderer* renderer = context->GetRenderer();

	OP_STATUS status = OpStatus::OK;

	if (scale.Equal(0))
	{
		status = context->CopySurface(dst, src, xlat_pix);
	}
	else
	{
		SVGFilterNodeInfo* dmap = context->GetImage(this, m_input[1]);

		VEGAComponent xdp = SVGDispSelToVEGAComp(m_displacement.x_disp);
		VEGAComponent ydp = SVGDispSelToVEGAComp(m_displacement.y_disp);

		VEGAFilter* dispfilter = NULL;
		RETURN_IF_ERROR(renderer->createDisplaceFilter(&dispfilter,
													   dmap->target, scale.GetVegaNumber(),
													   xdp, ydp));

		xlat_pix.IntersectWith(dst->area);

		context->ClearSurfaceInv(dst, xlat_pix);

		dispfilter->setSource(src->target, !!m_input[1]->IsAlphaOnly());

		renderer->setRenderTarget(dst->target);

		VEGAFilterRegion fregion;
		fregion.sx = 0;
		fregion.sy = 0;
		fregion.dx = xlat_pix.x - dst->area.x;
		fregion.dy = xlat_pix.y - dst->area.y;
		fregion.width = xlat_pix.width;
		fregion.height = xlat_pix.height;

		status = renderer->applyFilter(dispfilter, fregion);

		OP_DELETE(dispfilter);
	}

	context->DecRef(m_input[0]);
	context->DecRef(m_input[1]);

	return status;
}

// N-ary
OP_STATUS SVGNaryFilterNode::SetNumberInputs(unsigned num_inputs)
{
	OP_ASSERT(m_inputs == NULL);

	m_inputs = OP_NEWA(SVGFilterNode*, num_inputs);
	if (!m_inputs)
		return OpStatus::ERR_NO_MEMORY;

	for (unsigned i = 0; i < num_inputs; ++i)
		m_inputs[i] = NULL;

	m_num_inputs = num_inputs;
	return OpStatus::OK;
}

void SVGNaryFilterNode::ResolveColorSpace(SVGColorInterpolation colorinterp)
{
	if (colorinterp == SVGCOLORINTERPOLATION_AUTO)
	{
		unsigned int cscount[NUM_COLORSPACES]; /* ARRAY OK 2011-01-25 fs */
		cscount[COLORSPACE_SRGB] = 0;
		cscount[COLORSPACE_LINEARRGB] = 0;

		for (unsigned int i = 0; i < m_num_inputs; i++)
		{
			// Somewhat paranoid
			ColorSpaceType cstype = m_inputs[i] ? m_inputs[i]->GetColorSpace() : COLORSPACE_SRGB;

			switch (cstype)
			{
			default:
			case COLORSPACE_SRGB:
				cscount[COLORSPACE_SRGB]++;
				break;
			case COLORSPACE_LINEARRGB:
				cscount[COLORSPACE_LINEARRGB]++;
				break;
			}
		}

		// If most of the inputs are 'linearRGB' use that - otherwise use 'sRGB'
		if (cscount[COLORSPACE_SRGB] < cscount[COLORSPACE_LINEARRGB])
			m_colorspace = COLORSPACE_LINEARRGB;
		else
			m_colorspace = COLORSPACE_SRGB;
	}
	else
	{
		SVGFilterNode::ResolveColorSpace(colorinterp);
	}
}

void SVGNaryFilterNode::CalculateDefaultSubregion(const SVGRect& filter_region)
{
	// "x, y, width and height default to the union (i.e., tightest fitting
	// bounding box) of the subregions defined for all referenced nodes"
	for (unsigned int i = 0; i < m_num_inputs; i++)
	{
		if (m_inputs[i])
		{
			m_region.UnionWith(m_inputs[i]->GetRegion());
		}
		else
		{
			// "...or one or more of the referenced nodes is a standard input
			// (one of SourceGraphic, SourceAlpha, BackgroundImage, BackgroundAlpha,
			// FillPaint or StrokePaint), ... the default subregion is 0%,0%,100%,100%"

			m_region.UnionWith(filter_region);
			return; // No need to pursue this any longer
		}
	}

	if (m_region.IsEmpty())
	{
		// Handle case when there are no in references and default to (0,0,100%,100%).
		// Happens for instance with a feMerge without feMergeNode's
		m_region = filter_region;
	}
}

OP_STATUS SVGNaryFilterNode::Apply(SVGFilterContext* context)
{
	// In-place filtering possible (first/bottom surface)

	// FIXME: This could be fixed up to do a simple copy for the
	// first feMergeNode, meaning we could request a potentially dirty surface
	SVGFilterNodeInfo* dst = context->GetSurface(this);
	if (!dst)
		return OpStatus::ERR_NO_MEMORY;

	VEGARenderer* renderer = context->GetRenderer();

	VEGAFilter* merge = NULL;
	RETURN_IF_ERROR(renderer->createMergeFilter(&merge, VEGAMERGE_OVER));
	OpAutoPtr<VEGAFilter> merge_clean(merge);

	OpRect xlat_pix = context->ResolveArea(m_region);
	xlat_pix.IntersectWith(dst->area);

	VEGAFilterRegion fregion;
	fregion.dx = xlat_pix.x - dst->area.x;
	fregion.dy = xlat_pix.y - dst->area.y;
	fregion.width = xlat_pix.width;
	fregion.height = xlat_pix.height;

	SVGFilterNodeInfo src_alpha;
	src_alpha.target = NULL; // In case some surface is an alpha surface

	OP_STATUS status = OpStatus::OK;

	for (unsigned int i = 0; i < m_num_inputs && OpStatus::IsSuccess(status); i++)
	{
		SVGFilterNodeInfo* src = context->GetImage(this, m_inputs[i]);
		if (!src)
			continue;

		if (m_inputs[i]->IsAlphaOnly())
		{
			if (!src_alpha.target ||
				src_alpha.area.width < src->area.width ||
				src_alpha.area.height < src->area.height)
			{
				VEGARenderTarget::Destroy(src_alpha.target);

				src_alpha.area = src->area;

				RETURN_IF_ERROR(renderer->createIntermediateRenderTarget(&src_alpha.target,
																		 src_alpha.area.width,
																		 src_alpha.area.height));
			}
			else
			{
				src_alpha.area = src->area;
			}

			context->ConvertToAlphaSurface(src, &src_alpha, src->area);

			src = &src_alpha;
		}

		fregion.sx = xlat_pix.x - src->area.x;
		fregion.sy = xlat_pix.y - src->area.y;

		merge->setSource(src->target);
		renderer->setRenderTarget(dst->target);

		status = renderer->applyFilter(merge, fregion);

		context->DecRef(m_inputs[i]);
	}

	VEGARenderTarget::Destroy(src_alpha.target);
	return OpStatus::OK;
}

// Unary
void SVGUnaryFilterNode::ResolveColorSpace(SVGColorInterpolation colorinterp)
{
	if (colorinterp == SVGCOLORINTERPOLATION_AUTO)
	{
		// Inherit from input
		m_colorspace = m_input->GetColorSpace();
		return;
	}

	SVGFilterNode::ResolveColorSpace(colorinterp);
}

void SVGUnaryFilterNode::CalculateDefaultSubregion(const SVGRect& filter_region)
{
	m_region = m_input->GetRegion();

	if (m_region.IsEmpty())
		m_region = filter_region;
}

static SVGNumber GaussianStdDevToRadius(SVGNumber stddev)
{
	SVGNumber radius;
	if (stddev < 2)
		radius = (stddev * 3).ceil();
	else
		radius = (stddev * ((SVGNumber::pi() * 2).sqrt() * 3 / 4)).floor() + SVGNumber(0.5);

	return radius;
}

void SVGSimpleUnaryFilterNode::CalculateDefaultSubregion(const SVGRect& filter_region)
{
	if (m_type == TILE)
		m_region = filter_region;
	else
		SVGUnaryFilterNode::CalculateDefaultSubregion(filter_region);
}

void SVGSimpleUnaryFilterNode::GetExtentModifier(SVGFilterContext* context, ExtentModifier& modifier)
{
	const SVGMatrix& buffer_xfrm = context->GetTransform();

	switch (m_type)
	{
	case GAUSSIAN:
		{
			// Radius from StdDeviation (x, y)
			if (m_value.x.Equal(0) && m_value.y.Equal(0))
				break;

			SVGNumber scaled_rad_x = GaussianStdDevToRadius(m_value.x * buffer_xfrm[0]);
			SVGNumber scaled_rad_y = GaussianStdDevToRadius(m_value.y * buffer_xfrm[3]);

			modifier = ExtentModifier(ExtentModifier::EXTEND, scaled_rad_x, scaled_rad_y);
		}
		break;

	case OFFSET:
		{
			// Offset (x, y)
			SVGNumber scaled_ofs_x = m_value.x * buffer_xfrm[0];
			SVGNumber scaled_ofs_y = m_value.y * buffer_xfrm[3];

			scaled_ofs_x = scaled_ofs_x.abs();
			scaled_ofs_y = scaled_ofs_y.abs();

			modifier = ExtentModifier(ExtentModifier::EXTEND, scaled_ofs_x, scaled_ofs_y);
		}
		break;

	case MORPHOLOGY:
		{
			// Radius (x,y)
			if (m_value.x.Equal(0) && m_value.y.Equal(0))
				break;

			SVGNumber scaled_rad_x = m_value.x * buffer_xfrm[0];
			SVGNumber scaled_rad_y = m_value.y * buffer_xfrm[3];

			modifier = ExtentModifier(ExtentModifier::EXTEND, scaled_rad_x, scaled_rad_y);
		}
		break;

	case TILE:
		// Include the portion (of the source) that we are going to
		// use as the tile. Preferably this should only affect the
		// area of the source, but there's currently no way to express
		// that. It is also possible that we could get away with less
		// of an area, but then the analysis needs to extended.
		modifier = ExtentModifier(ExtentModifier::AREA, buffer_xfrm.ApplyToRect(m_tile_region));
		break;

	default:
	case COMPONENTTRANSFER:
	case COLORMATRIX:
		break;
	}
}

OP_STATUS SVGSimpleUnaryFilterNode::Apply(SVGFilterContext* context)
{
	VEGAFilter* filter = NULL;
	BOOL do_inverted_clear = TRUE;
	BOOL filter_handles_alpha_only = FALSE;
	int offset_x = 0;
	int offset_y = 0;

	VEGARenderer* renderer = context->GetRenderer();
	const SVGMatrix& buffer_xfrm = context->GetTransform();
	switch (m_type)
	{
	case COMPONENTTRANSFER:
		// In-place filtering possible
		RETURN_IF_ERROR(SetupComponentTransfer(renderer, filter));

		// FIXME/OPT: Does not handle alpha-only explicitly, but the
		// tables could probably be adapted to make it appear that way.
		break;

	case COLORMATRIX:
		// In-place filtering possible
		RETURN_IF_ERROR(SetupColorMatrix(renderer, filter));

		// FIXME/OPT: Does not handle alpha-only explicitly, but the
		// matrix could probably be adapted to make it appear that way.
		break;

	case GAUSSIAN:
		{
			// In-place filtering possible
			if (m_value.x.Equal(0) && m_value.y.Equal(0))
			{
				// Identity/Copy
				RETURN_IF_ERROR(renderer->createMergeFilter(&filter, VEGAMERGE_REPLACE));
			}
			else
			{
				SVGNumber scaled_stddev_x = m_value.x * buffer_xfrm[0];
				SVGNumber scaled_stddev_y = m_value.y * buffer_xfrm[3];

				RETURN_IF_ERROR(renderer->createGaussianFilter(&filter,
															   scaled_stddev_x.GetVegaNumber(),
															   scaled_stddev_y.GetVegaNumber(),
															   false));
			}

			filter_handles_alpha_only = TRUE;
		}
		break;

	case OFFSET:
		{
			// Could probably be done "in-place" (simple pixelwise shift)
			SVGNumber scaled_xofs = m_value.x * buffer_xfrm[0];
			SVGNumber scaled_yofs = m_value.y * buffer_xfrm[3];

			RETURN_IF_ERROR(renderer->createMergeFilter(&filter, VEGAMERGE_REPLACE));

			offset_x = scaled_xofs.GetRoundedIntegerValue();
			offset_y = scaled_yofs.GetRoundedIntegerValue();

			do_inverted_clear = FALSE;
			filter_handles_alpha_only = TRUE;
		}
		break;

	case MORPHOLOGY:
		{
			// In-place filtering possible
			SVGNumber scaled_rad_x = m_value.x * buffer_xfrm[0];
			SVGNumber scaled_rad_y = m_value.y * buffer_xfrm[3];

			int pix_rad_x = scaled_rad_x.GetIntegerValue();
			int pix_rad_y = scaled_rad_y.GetIntegerValue();

			OP_STATUS status;
			if (pix_rad_x <= 0 || pix_rad_y <= 0)
			{
				// Create copy filter
				status = renderer->createMergeFilter(&filter, VEGAMERGE_REPLACE);
			}
			else
			{
				VEGAMorphologyType morph_op;
				switch (m_morph.oper)
				{
				default:
				case SVGMORPHOPERATOR_ERODE:
					morph_op = VEGAMORPH_ERODE;
					break;

				case SVGMORPHOPERATOR_DILATE:
					morph_op = VEGAMORPH_DILATE;
					break;
				}

				status = renderer->createMorphologyFilter(&filter, morph_op,
														  pix_rad_x, pix_rad_y, false);
			}

			RETURN_IF_ERROR(status);

			filter_handles_alpha_only = TRUE;
		}
		break;

	case TILE:
		return CreateTile(context);

	default:
		OP_ASSERT(!"Unknown filter type - memory corruption?");
		return OpStatus::OK;
	}

	OP_ASSERT(filter);

	SVGFilterNodeInfo* src = context->GetImage(this, m_input);
	if (!src)
		return OpStatus::OK; // FIXME: Leak

	OP_STATUS status;

	SVGFilterNodeInfo* dst;
	if (do_inverted_clear)
		dst = context->GetSurfaceDirty(this);
	else
		dst = context->GetSurface(this);

	if (dst)
	{
		status = CommonApply(context, filter, dst, src, m_input->IsAlphaOnly(),
							 do_inverted_clear, offset_x, offset_y);

		if (!filter_handles_alpha_only && m_input->IsAlphaOnly())
		{
			// SRC was alpha-only, make DST be alpha-only
			context->ConvertToAlphaSurface(dst, dst, dst->area);
		}
	}
	else
	{
		status = OpStatus::ERR_NO_MEMORY;
	}

	OP_DELETE(filter);

	context->DecRef(m_input);

	return status;
}

OP_STATUS SVGSimpleUnaryFilterNode::SetupComponentTransfer(VEGARenderer* renderer, VEGAFilter*& filter)
{
	VEGA_FIX stat_tbls[4][3]; // Static tables to avoid allocations
	VEGACompFuncType func_type[4];
	VEGA_FIX* func_tbls_p[4];
	unsigned int func_tbl_size[4];

	unsigned int f;

	for (f = 0; f < 4; ++f)
	{
		func_type[f] = VEGACOMPFUNC_IDENTITY;
		func_tbls_p[f] = NULL;
	}

	OP_STATUS status = OpStatus::OK;

	SVGCompFunc* funcs = m_compxfer.funcs;

	for (f = 0; f < 4 && OpStatus::IsSuccess(status); f++) /* RGBA */
	{
		VEGAComponent comp;

		switch (f)
		{
		default:
		case 0: /* Red */
			comp = VEGACOMP_R;
			break;
		case 1: /* Green */
			comp = VEGACOMP_G;
			break;
		case 2: /* Blue */
			comp = VEGACOMP_B;
			break;
		case 3: /* Alpha */
			comp = VEGACOMP_A;
			break;
		}

		switch (funcs[f].type)
		{
		default:
		case SVGFUNC_IDENTITY:
			break;

		case SVGFUNC_TABLE:
		case SVGFUNC_DISCRETE:
			{
				if (!funcs[f].table || funcs[f].table->GetCount() <= 0)
					/* use 'identity' */
					break;

				unsigned vega_table_size = funcs[f].table->GetCount();

				VEGA_FIX* vega_table = OP_NEWA(VEGA_FIX, vega_table_size);
				if (!vega_table)
				{
					status = OpStatus::ERR_NO_MEMORY;
					break;
				}

				for (UINT32 i = 0; i < vega_table_size; i++)
				{
					SVGObject* obj = funcs[f].table->Get(i);
					if (obj && obj->Type() == SVGOBJECT_NUMBER)
					{
						SVGNumberObject* nval = static_cast<SVGNumberObject*>(obj);

						vega_table[i] = nval->value.GetVegaNumber();
					}
					else
					{
						vega_table[i] = 0;
					}
				}

				if (funcs[f].type == SVGFUNC_TABLE)
					func_type[comp] = VEGACOMPFUNC_TABLE;
				else
					func_type[comp] = VEGACOMPFUNC_DISCRETE;

				func_tbls_p[comp] = vega_table;
				func_tbl_size[comp] = vega_table_size;
			}
			break;

		case SVGFUNC_LINEAR:
			func_type[comp] = VEGACOMPFUNC_LINEAR;

			stat_tbls[comp][0] = funcs[f].slope.GetVegaNumber();
			stat_tbls[comp][1] = funcs[f].intercept.GetVegaNumber();

			func_tbls_p[comp] = stat_tbls[comp];
			func_tbl_size[comp] = 2;
			break;

		case SVGFUNC_GAMMA:
			func_type[comp] = VEGACOMPFUNC_GAMMA;

			stat_tbls[comp][0] = funcs[f].amplitude.GetVegaNumber();
			stat_tbls[comp][1] = funcs[f].exponent.GetVegaNumber();
			stat_tbls[comp][2] = funcs[f].offset.GetVegaNumber();

			func_tbls_p[comp] = stat_tbls[comp];
			func_tbl_size[comp] = 3;
			break;
		}
	}

	if (OpStatus::IsSuccess(status))
		status = renderer->createComponentColorTransformFilter(&filter,
															   func_type[0], func_tbls_p[0],
															   func_tbl_size[0],
															   func_type[1], func_tbls_p[1],
															   func_tbl_size[1],
															   func_type[2], func_tbls_p[2],
															   func_tbl_size[2],
															   func_type[3], func_tbls_p[3],
															   func_tbl_size[3]);

	for (f = 0; f < 4; ++f)
		if (func_type[f] == VEGACOMPFUNC_TABLE ||
			func_type[f] == VEGACOMPFUNC_DISCRETE)
			OP_DELETEA(func_tbls_p[f]);

	return status;
}

OP_STATUS SVGSimpleUnaryFilterNode::SetupColorMatrix(VEGARenderer* renderer, VEGAFilter*& filter)
{
	VEGA_FIX matrix[20];

	// Initialize matrix to identity
	for (unsigned i = 0; i < 20; i++)
		matrix[i] = 0;

	matrix[0] = matrix[6] = matrix[12] = matrix[18] = VEGA_INTTOFIX(1);

	const SVGVector* mat = m_colmatrix.mat;

	switch (m_colmatrix.type)
	{
	case SVGCOLORMATRIX_SATURATE:
		{
			/* Ouch */
			const VEGA_FIX v0213 = SVGNumber(0.213).GetVegaNumber();
			const VEGA_FIX v0787 = SVGNumber(0.787).GetVegaNumber();
			const VEGA_FIX v0715 = SVGNumber(0.715).GetVegaNumber();
			const VEGA_FIX v0072 = SVGNumber(0.072).GetVegaNumber();
			const VEGA_FIX v0928 = SVGNumber(0.928).GetVegaNumber();
			const VEGA_FIX v0285 = SVGNumber(0.285).GetVegaNumber();

			VEGA_FIX s = VEGA_INTTOFIX(1);
			if (mat && mat->GetCount() > 0)
				if (SVGNumberObject* s_val = static_cast<SVGNumberObject*>(mat->Get(0)))
					s = s_val->value.GetVegaNumber();

			matrix[0] = v0213 + VEGA_FIXMUL(v0787, s);
			matrix[1] = v0715 - VEGA_FIXMUL(v0715, s);
			matrix[2] = v0072 - VEGA_FIXMUL(v0072, s);

			matrix[5] = v0213 - VEGA_FIXMUL(v0213, s);
			matrix[6] = v0715 + VEGA_FIXMUL(v0285, s);
			matrix[7] = v0072 - VEGA_FIXMUL(v0072, s);

			matrix[10] = v0213 - VEGA_FIXMUL(v0213, s);
			matrix[11] = v0715 - VEGA_FIXMUL(v0715, s);
			matrix[12] = v0072 + VEGA_FIXMUL(v0928, s);
		}
		break;

	case SVGCOLORMATRIX_HUEROTATE:
		{
			/* Ouch */
			const VEGA_FIX v0213 = SVGNumber(0.213).GetVegaNumber();
			const VEGA_FIX v0787 = SVGNumber(0.787).GetVegaNumber();
			const VEGA_FIX v0715 = SVGNumber(0.715).GetVegaNumber();
			const VEGA_FIX v0072 = SVGNumber(0.072).GetVegaNumber();
			const VEGA_FIX v0928 = SVGNumber(0.928).GetVegaNumber();
			const VEGA_FIX v0143 = SVGNumber(0.143).GetVegaNumber();
			const VEGA_FIX v0140 = SVGNumber(0.140).GetVegaNumber();
			const VEGA_FIX v0283 = SVGNumber(0.283).GetVegaNumber();
			const VEGA_FIX v0285 = SVGNumber(0.285).GetVegaNumber();

			VEGA_FIX vega_angle = 0;
			if (mat && mat->GetCount() > 0)
				if (SVGNumberObject* angle = static_cast<SVGNumberObject*>(mat->Get(0)))
					vega_angle = angle->value.GetVegaNumber();

			VEGA_FIX cos_a = VEGA_FIXCOS(vega_angle);
			VEGA_FIX sin_a = VEGA_FIXSIN(vega_angle);

			/* Row 1 */
			matrix[0] = v0213 + VEGA_FIXMUL(cos_a, v0787) - VEGA_FIXMUL(sin_a, v0213);
			matrix[1] = v0715 - VEGA_FIXMUL(cos_a, v0715) - VEGA_FIXMUL(sin_a, v0715);
			matrix[2] = v0072 - VEGA_FIXMUL(cos_a, v0072) + VEGA_FIXMUL(sin_a, v0928);

			/* Row 2 */
			matrix[5] = v0213 - VEGA_FIXMUL(cos_a, v0213) + VEGA_FIXMUL(sin_a, v0143);
			matrix[6] = v0715 + VEGA_FIXMUL(cos_a, v0285) + VEGA_FIXMUL(sin_a, v0140);
			matrix[7] = v0072 - VEGA_FIXMUL(cos_a, v0072) - VEGA_FIXMUL(sin_a, v0283);

			/* Row 3 */
			matrix[10] = v0213 - VEGA_FIXMUL(cos_a, v0213) - VEGA_FIXMUL(sin_a, v0787);
			matrix[11] = v0715 - VEGA_FIXMUL(cos_a, v0715) + VEGA_FIXMUL(sin_a, v0715);
			matrix[12] = v0072 + VEGA_FIXMUL(cos_a, v0928) + VEGA_FIXMUL(sin_a, v0072);
		}
		break;

	case SVGCOLORMATRIX_LUMINANCETOALPHA:
		break;

	case SVGCOLORMATRIX_MATRIX:
		if (mat)
		{
			for (unsigned i = 0; i < mat->GetCount() && i < 20; i++)
				if (SVGNumberObject* num = static_cast<SVGNumberObject*>(mat->Get(i)))
					matrix[i] = num->value.GetVegaNumber();
		}
		break;

	default:
		break;
	}

	if (m_colmatrix.type == SVGCOLORMATRIX_LUMINANCETOALPHA)
	{
		return renderer->createLuminanceToAlphaFilter(&filter);
	}
	else
	{
		OP_ASSERT(m_colmatrix.type == SVGCOLORMATRIX_MATRIX ||
				  m_colmatrix.type == SVGCOLORMATRIX_HUEROTATE ||
				  m_colmatrix.type == SVGCOLORMATRIX_SATURATE);

		VEGA_FIX* matrix_p = matrix;
		return renderer->createMatrixColorTransformFilter(&filter, matrix_p);
	}
}

OP_STATUS SVGSimpleUnaryFilterNode::CreateTile(SVGFilterContext* context)
{
	SVGFilterNodeInfo* dst = context->GetSurface(this);
	if (!dst)
		return OpStatus::ERR_NO_MEMORY;

	SVGFilterNodeInfo* src = context->GetImage(this, m_input);
	if (!src)
		return OpStatus::OK;

	OpRect xlat_pix = context->ResolveArea(m_region);
	OpRect tile_pix = context->ResolveArea(m_tile_region);

	// Setup tile image
	OpRect clipped_tile(tile_pix);
	clipped_tile.IntersectWith(src->area);
	if (clipped_tile.IsEmpty())
		return OpStatus::OK;

	VEGARenderer* renderer = context->GetRenderer();

	// Create intermediate rendertarget to copy the tile source to
	VEGARenderTarget* im_rt = NULL;
	RETURN_IF_ERROR(renderer->createIntermediateRenderTarget(&im_rt,
															 tile_pix.width,
															 tile_pix.height));
	VEGAAutoRenderTarget im_rt_cleaner(im_rt);

	// Copy appropriate portion
	{
		VEGAFilter* copy = NULL;
		RETURN_IF_ERROR(renderer->createMergeFilter(&copy, VEGAMERGE_REPLACE));

		OpAutoPtr<VEGAFilter> copy_clean(copy);
		copy->setSource(src->target);

		VEGAFilterRegion fregion;
		fregion.sx = fregion.dx = 0;
		fregion.sy = fregion.dy = 0;
		fregion.width = tile_pix.width;
		fregion.height = tile_pix.height;

		int ofs_x = -tile_pix.x;
		int ofs_y = -tile_pix.y;

		if (ofs_x > 0)
			fregion.dx += ofs_x;
		else if (ofs_x < 0)
			fregion.sx -= ofs_x;

		if (ofs_y > 0)
			fregion.dy += ofs_y;
		else if (ofs_y < 0)
			fregion.sy -= ofs_y;

		renderer->setRenderTarget(im_rt);

		RETURN_IF_ERROR(renderer->applyFilter(copy, fregion));
	}

	VEGATransform inv_xfrm;
	inv_xfrm.loadTranslate(-VEGA_INTTOFIX(tile_pix.x), -VEGA_INTTOFIX(tile_pix.y));
	VEGATransform xfrm;
	xfrm.loadTranslate(VEGA_INTTOFIX(tile_pix.x), VEGA_INTTOFIX(tile_pix.y));

	VEGAFill* tile = NULL;
	RETURN_IF_ERROR(im_rt->getImage(&tile));
	tile->setSpread(VEGAFill::SPREAD_REPEAT);
	tile->setQuality(VEGAFill::QUALITY_LINEAR);
	tile->setTransform(xfrm, inv_xfrm);

	renderer->setRenderTarget(dst->target);
	renderer->setFill(tile);

	OpStatus::Ignore(renderer->fillRect(xlat_pix.x, xlat_pix.y,
										xlat_pix.width, xlat_pix.height));

	renderer->setFill(NULL);

	context->DecRef(m_input);

	return OpStatus::OK;
}

void SVGScaledUnaryFilterNode::GetExtentModifier(SVGFilterContext* context, ExtentModifier& modifier)
{
	switch (m_type)
	{
	case CONVOLVE:
		{
			// <Order> kernelUnitLength:s
			if (m_convolve.kernmat == NULL ||
				m_convolve_order.x <= 0 || m_convolve_order.y <= 0)
				break;

			// The setup code should've taken care of this
			OP_ASSERT(!(m_convolve_target.x < 0 || m_convolve_target.x >= m_convolve_order.x));
			OP_ASSERT(!(m_convolve_target.y < 0 || m_convolve_target.y >= m_convolve_order.y));

			SVGNumber dist_x = SVGNumber::max_of(m_convolve_order.x - m_convolve_target.x,
												 m_convolve_target.x);
			SVGNumber dist_y = SVGNumber::max_of(m_convolve_order.y - m_convolve_target.y,
												 m_convolve_target.y);

			if (m_has_unit_len)
			{
				dist_x *= m_unit_len.x;
				dist_y *= m_unit_len.y;
			}

			modifier = ExtentModifier(ExtentModifier::EXTEND_MIN_1PX, dist_x, dist_y);
		}
		break;

	case LIGHTING:
		{
			// One kernelUnitLength extra
			if (m_has_unit_len)
				modifier = ExtentModifier(ExtentModifier::EXTEND_MIN_1PX, m_unit_len.x, m_unit_len.y);
			else
				modifier = ExtentModifier(ExtentModifier::EXTEND_MIN_1PX, 0, 0);
		}
		break;
	}
}

OP_STATUS SVGScaledUnaryFilterNode::Apply(SVGFilterContext* context)
{
	SVGFilterNodeInfo* dst;
	if (!m_has_unit_len)
		dst = context->GetSurfaceDirty(this);
	else
		dst = context->GetSurface(this);

	if (!dst)
		return OpStatus::ERR_NO_MEMORY;

	VEGAFilter* filter = NULL;

	switch (m_type)
	{
	case CONVOLVE:
		// Invalid values, result is 'transparent black'
		if (m_convolve.kernmat == NULL ||
			m_convolve_order.x <= 0 || m_convolve_order.y <= 0)
			break;

		RETURN_IF_ERROR(SetupConvolution(context->GetRenderer(), filter));
		break;

	case LIGHTING:
		// In-place filtering possible (some cases atleast)
		// FIXME: Needs resolved area
		RETURN_IF_ERROR(SetupLighting(context, dst, filter));
		break;

	default:
		OP_ASSERT(!"Unknown filter type - memory corruption?");
		return OpStatus::OK;
	}

	if (m_type == CONVOLVE && !filter)
	{
		// FIXME: A tad ugly - maybe handle this case differently by
		// emitting a different paint node during construction?
		return OpStatus::OK;
	}

	OP_STATUS status = OpStatus::OK;

	SVGFilterNodeInfo* src = context->GetImage(this, m_input);
	if (src)
	{
		if (m_has_unit_len)
		{
			/* Need to rescale the surfaces */
			status = ScaledApply(context, filter, m_unit_len, dst, src, m_input->IsAlphaOnly());
		}
		else
		{
			status = CommonApply(context, filter, dst, src, m_input->IsAlphaOnly(), TRUE);
		}
	}

	OP_DELETE(filter);

	context->DecRef(m_input);

	return status;
}

OP_STATUS SVGScaledUnaryFilterNode::ScaledApply(SVGFilterContext* context,
												VEGAFilter* filter,
												const SVGNumberPair& unit_len,
												SVGFilterNodeInfo* dst, SVGFilterNodeInfo* src,
												BOOL src_is_alpha_only)
{
	SVGNumber scale_x = SVGNumber::min_of(SVGNumber(1) / unit_len.x, 4);
	SVGNumber scale_y = SVGNumber::min_of(SVGNumber(1) / unit_len.y, 4);
	OpRect scaled_src_area;
	scaled_src_area.x = (scale_x * src->area.x).GetIntegerValue();
	scaled_src_area.y = (scale_y * src->area.y).GetIntegerValue();
	scaled_src_area.width = (scale_x * src->area.width).GetRoundedIntegerValue();
	scaled_src_area.height = (scale_y * src->area.height).GetRoundedIntegerValue();

	VEGARenderer scaled_renderer;
	RETURN_IF_ERROR(scaled_renderer.Init(scaled_src_area.width, scaled_src_area.height));

	SVGFilterNodeInfo scl_src;
	scl_src.area = scaled_src_area;

	RETURN_IF_ERROR(scaled_renderer.createIntermediateRenderTarget(&scl_src.target,
																   scl_src.area.width,
																   scl_src.area.height));
	VEGAAutoRenderTarget scaled_src(scl_src.target);
	context->ClearSurface(&scl_src, scl_src.area, 0, &scaled_renderer);

	SVGFilterNodeInfo scl_dst;
	scl_dst.area = scaled_src_area;

	RETURN_IF_ERROR(scaled_renderer.createIntermediateRenderTarget(&scl_dst.target,
																   scl_dst.area.width,
																   scl_dst.area.height));
	VEGAAutoRenderTarget scaled_dst(scl_dst.target);
	// Don't clear destination, will be overwritten

	OP_ASSERT(scaled_src.get() && scaled_dst.get());

	RETURN_IF_ERROR(ScaleSurface(&scaled_renderer, &scl_src, src));

	// FIXME: Adjust source ?

	filter->setSource(scl_src.target, src_is_alpha_only ? true : false);

	scaled_renderer.setRenderTarget(scl_dst.target);

	// Setup a custom transformation for the region
	SVGMatrix unit_region_xfrm = context->GetTransform();
	unit_region_xfrm.MultScale(scale_x, scale_y);

	SVGRect xlat_region = unit_region_xfrm.ApplyToRect(m_region);
	OpRect xlat_pix = xlat_region.GetEnclosingRect();

	xlat_pix.IntersectWith(scl_dst.area);

	VEGAFilterRegion fregion;
	fregion.sx = xlat_pix.x - scl_src.area.x;
	fregion.sy = xlat_pix.y - scl_src.area.y;
	fregion.dx = xlat_pix.x - scl_dst.area.x;
	fregion.dy = xlat_pix.y - scl_dst.area.y;
	fregion.width = xlat_pix.width;
	fregion.height = xlat_pix.height;

	OpStatus::Ignore(scaled_renderer.applyFilter(filter, fregion));

	return ScaleSurface(context->GetRenderer(), dst, &scl_dst);
}

OP_STATUS SVGScaledUnaryFilterNode::SetupConvolution(VEGARenderer* renderer, VEGAFilter*& filter)
{
	VEGAFilterEdgeMode em;
	switch (m_convolve.edge_mode)
	{
	case SVGCONVOLVEEDGEMODE_NONE:
		em = VEGAFILTEREDGE_NONE;
		break;

	case SVGCONVOLVEEDGEMODE_WRAP:
		em = VEGAFILTEREDGE_WRAP;
		break;

	default:
	case SVGCONVOLVEEDGEMODE_DUPLICATE:
		em = VEGAFILTEREDGE_DUPLICATE;
		break;
	}

	int kw = m_convolve_order.x.GetIntegerValue();
	int kh = m_convolve_order.y.GetIntegerValue();

	int ksize = kw * kh;

	OP_ASSERT(kw && kh);
	if (ksize / kw != kh)
		return OpStatus::ERR_NO_MEMORY;

	/* Temp kernel */
	VEGA_FIX* tmp_kern = OP_NEWA(VEGA_FIX, ksize);
	if (!tmp_kern)
		return OpStatus::ERR_NO_MEMORY;

	for (int i = 0; i < ksize; i++)
		if (SVGNumberObject* nval = static_cast<SVGNumberObject*>(m_convolve.kernmat->Get(i)))
			tmp_kern[i] = nval->value.GetVegaNumber();
		else
			tmp_kern[i] = 0;

	OP_STATUS status = renderer->createConvolveFilter(&filter,
													  tmp_kern, kw, kh,
													  m_convolve_target.x.GetIntegerValue(),
													  m_convolve_target.y.GetIntegerValue(),
													  m_convolve_divisor.GetVegaNumber(),
													  m_convolve_bias.GetVegaNumber(),
													  em, m_convolve.preserve_alpha ? true : false);
	OP_DELETEA(tmp_kern);
	return status;
}

OP_STATUS SVGScaledUnaryFilterNode::SetupLighting(SVGFilterContext* context,
												  SVGFilterNodeInfo* nodeinfo, VEGAFilter*& filter)
{
	UINT32 vega_lightcolor = (GetSVGColorRValue(m_lighting.color) << 16) |
		(GetSVGColorGValue(m_lighting.color) << 8) |
		GetSVGColorBValue(m_lighting.color);

	VEGALightParameter lightparam;
	const SVGMatrix& buffer_xfrm = context->GetTransform();

	SVGNumber z_buf_scale = buffer_xfrm[0];

	lightparam.color = context->ConvertColor(vega_lightcolor, m_colorspace);
	lightparam.surfaceScale = (m_lighting_surface_scale * z_buf_scale).GetVegaNumber();
	lightparam.constant = m_lighting_const_fact.GetVegaNumber();

	if (m_lighting.is_specular)
	{
		lightparam.type = VEGALIGHTING_SPECULAR;
		lightparam.exponent = m_lighting_specular_exp.GetVegaNumber();
	}
	else // Diffuse
	{
		lightparam.type = VEGALIGHTING_DIFFUSE;
	}

	SVGLightSource light = m_light;

	// Transform coordinates for the lightsource
	switch (light.type)
	{
	default:
	case SVGLightSource::SVGLIGHTSOURCE_DISTANT:
		break;

	case SVGLightSource::SVGLIGHTSOURCE_SPOT:
		buffer_xfrm.ApplyToCoordinate(&light.pointsAtX, &light.pointsAtY);
		light.pointsAtZ = light.pointsAtZ * z_buf_scale;
		/* Fall through */

	case SVGLightSource::SVGLIGHTSOURCE_POINT:
		buffer_xfrm.ApplyToCoordinate(&light.x, &light.y);
		light.z = light.z * z_buf_scale;
		break;
	}

	if (light.type != SVGLightSource::SVGLIGHTSOURCE_DISTANT)
	{
		// Adjust the position of the light source to account for
		// non-zero origins
		SVGNumber adj_x = nodeinfo->area.x;
		SVGNumber adj_y = nodeinfo->area.y;

		if (m_has_unit_len)
		{
			adj_x /= m_unit_len.x;
			adj_y /= m_unit_len.y;
		}

		light.x -= adj_x;
		light.y -= adj_y;
		light.pointsAtX -= adj_x;
		light.pointsAtY -= adj_y;
	}

	VEGARenderer* renderer = context->GetRenderer();
	switch (light.type)
	{
	case SVGLightSource::SVGLIGHTSOURCE_DISTANT:
		return renderer->createDistantLightFilter(&filter, lightparam,
												  light.x.GetVegaNumber(),
												  light.y.GetVegaNumber(),
												  light.z.GetVegaNumber());

	case SVGLightSource::SVGLIGHTSOURCE_POINT:
		return renderer->createPointLightFilter(&filter, lightparam,
												light.x.GetVegaNumber(),
												light.y.GetVegaNumber(),
												light.z.GetVegaNumber());

	case SVGLightSource::SVGLIGHTSOURCE_SPOT:
		return renderer->createSpotLightFilter(&filter, lightparam,
											   light.x.GetVegaNumber(),
											   light.y.GetVegaNumber(),
											   light.z.GetVegaNumber(),
											   light.pointsAtX.GetVegaNumber(),
											   light.pointsAtY.GetVegaNumber(),
											   light.pointsAtZ.GetVegaNumber(),
											   light.spec_exponent.GetVegaNumber(),
											   light.has_cone_angle ?
											   light.cone_angle.GetVegaNumber() :
											   VEGA_INTTOFIX(360));

	default:
		OP_ASSERT(!"Unrecognized light type - memory corruption?");
		return OpStatus::ERR;
	}
}

// Sources
OP_STATUS SVGSourceImageFilterNode::Apply(SVGFilterContext* context)
{
	SVGFilterNodeInfo* dstinfo = context->GetSurface(this);
	if (!dstinfo)
		return OpStatus::ERR_NO_MEMORY;

	VEGARenderer* renderer = context->GetRenderer();
	switch (m_type)
	{
	case SOURCE:
		{
			SVGPainter painter;
			RETURN_IF_ERROR(painter.BeginPaint(renderer, dstinfo->target, dstinfo->area));
			painter.SetTransform(context->GetTransform());
			painter.SetClipRect(dstinfo->area);
			painter.SetFlatness(context->GetPainter()->GetFlatness());

			context->GetPaintNode()->PaintContent(&painter);

			painter.EndPaint();
		}
		break;

	case BACKGROUND:
		{
			/* Create BackgroundImage */
			VEGARenderTarget* bl_target = NULL;
			OpRect bl_target_area;
			RETURN_IF_ERROR(context->GetPainter()->GetBackgroundLayer(&bl_target, bl_target_area,
																	  context->GetRegion()));

			OP_STATUS status = OpStatus::OK;
			if (bl_target)
				status = RescaleBackgroundImage(context, bl_target, bl_target_area, dstinfo);

			VEGARenderTarget::Destroy(bl_target); // Free the temporary surface

			RETURN_IF_ERROR(status);
		}
		break;

	case FILLPAINT:
	case STROKEPAINT:
		{
			SVGPainter painter;
			RETURN_IF_ERROR(painter.BeginPaint(renderer, dstinfo->target, dstinfo->area));
			painter.SetTransform(context->GetTransform());
			painter.SetClipRect(dstinfo->area);

			SVGObjectProperties obj_props;
			obj_props.aa_quality = VEGA_DEFAULT_QUALITY;
			obj_props.fillrule = SVGFILL_EVEN_ODD;
			obj_props.filled = TRUE;
			obj_props.stroked = FALSE;

			painter.SetObjectProperties(&obj_props);
			painter.SetFillPaint(m_paint);

			SVGPaintDesc dummy_stroke_paint;
			dummy_stroke_paint.Reset();
			painter.SetStrokePaint(dummy_stroke_paint);

			// Setup a dummy paint node, in case paint is a
			// paintserver and needs a bounding box
			SVGPrimitiveNode dummy_rect_node;
			dummy_rect_node.SetRectangle(m_region.x, m_region.y, m_region.width, m_region.height, 0, 0);

			painter.BeginDrawing(&dummy_rect_node);
			painter.DrawRect(m_region.x, m_region.y, m_region.width, m_region.height, 0, 0);
			painter.EndDrawing();

			painter.EndPaint();
		}
		break;

	default:
		OP_ASSERT(0);
	}

	return OpStatus::OK;
}

OP_STATUS SVGSourceImageFilterNode::RescaleBackgroundImage(SVGFilterContext* context,
														   VEGARenderTarget* bl_target,
														   const OpRect& bl_target_area,
														   SVGFilterNodeInfo* nodeinfo)
{
	VEGARenderer* renderer = context->GetRenderer();

	// Needed because fillRect will blend with the destination
	context->ClearSurface(nodeinfo, nodeinfo->area);

	// "Unwind" the surface passed in
	renderer->setRenderTarget(nodeinfo->target);

	VEGATransform inv_xfrm;
	VEGATransform scale_xfrm;
	VEGATransform trans_xfrm;
	VEGATransform skew_xfrm;
	VEGATransform surf_trans_xfrm;

	const SVGRect& filter_region = context->GetRegion();
	const OpRect& filter_pixel_region = context->GetPixelRegion();

	surf_trans_xfrm.loadTranslate(VEGA_INTTOFIX(-bl_target_area.x + nodeinfo->area.x),
								  VEGA_INTTOFIX(-bl_target_area.y + nodeinfo->area.y));
	trans_xfrm.loadTranslate(filter_region.x.GetVegaNumber(), filter_region.y.GetVegaNumber());
	scale_xfrm.loadScale(filter_region.width.GetVegaNumber() / filter_pixel_region.width,
						 filter_region.height.GetVegaNumber() / filter_pixel_region.height);

	const SVGMatrix& transform = context->GetTransform();

	// Setup source image
	skew_xfrm[0] = (transform[0]).GetVegaNumber(); // scale x
	skew_xfrm[1] = (transform[2]).GetVegaNumber(); // skew x
	skew_xfrm[2] = (transform[4]).GetVegaNumber(); // translate x
	skew_xfrm[3] = (transform[1]).GetVegaNumber(); // skew y
	skew_xfrm[4] = (transform[3]).GetVegaNumber(); // scale y
	skew_xfrm[5] = (transform[5]).GetVegaNumber(); // translate y

	inv_xfrm.loadIdentity();
	inv_xfrm.multiply(surf_trans_xfrm);
	inv_xfrm.multiply(skew_xfrm);
	inv_xfrm.multiply(trans_xfrm);
	inv_xfrm.multiply(scale_xfrm);

	VEGATransform xfrm;
	xfrm.copy(inv_xfrm);
	if (!xfrm.invert())
		return OpStatus::ERR; // Or OK?

	VEGAFill* src_img = NULL;
	RETURN_IF_ERROR(bl_target->getImage(&src_img));

	src_img->setSpread(VEGAFill::SPREAD_CLAMP_BORDER);
	src_img->setQuality(VEGAFill::QUALITY_NEAREST);
	src_img->setTransform(xfrm, inv_xfrm);
	src_img->setBorderColor(0); // Transparent black

	renderer->setFill(src_img);

	OpStatus::Ignore(renderer->fillRect(0, 0, nodeinfo->area.width, nodeinfo->area.height));

	renderer->setFill(NULL);

	return OpStatus::OK;
}

// Generators
SVGGeneratorFilterNode::~SVGGeneratorFilterNode()
{
	if (m_type == IMAGE)
		OP_DELETE(m_image.paint_node);
}

OP_STATUS SVGGeneratorFilterNode::Apply(SVGFilterContext* context)
{
	switch (m_type)
	{
	case NOISE:
		return CreateNoise(context);

	case IMAGE:
		{
			SVGFilterNodeInfo* dst = context->GetSurface(this);
			if (!dst)
				return OpStatus::ERR_NO_MEMORY;

			if (!m_image.paint_node)
				// No paint node generally mean that the referenced
				// resource was unavailable
				break;

			VEGARenderer* renderer = context->GetRenderer();

			SVGPainter painter;
			RETURN_IF_ERROR(painter.BeginPaint(renderer, dst->target, dst->area));
			painter.SetTransform(context->GetTransform());
			painter.SetClipRect(dst->area);
			painter.SetFlatness(context->GetPainter()->GetFlatness());

			// Use Paint() instead of PaintContent() here since we
			// want to have the the transform on the the paint node be
			// applied. This is a 'synthetic' paint node, so we fully
			// control what properties is has.
			m_image.paint_node->Paint(&painter);

			painter.EndPaint();

			// Tag the surface as sRGB
			dst->colorspace = COLORSPACE_SRGB;
		}
		break;

	case FLOOD:
		{
			SVGFilterNodeInfo* dst = context->GetSurfaceDirty(this);
			if (!dst)
				return OpStatus::ERR_NO_MEMORY;

			OpRect xlat_pix = context->ResolveArea(m_region);
			UINT32 flood_color = context->ConvertColor(m_flood.color, m_colorspace);

			context->ClearSurfaceInv(dst, xlat_pix);
			context->ClearSurface(dst, xlat_pix, (flood_color & 0x00ffffff) | (m_flood.opacity << 24));
		}
		break;

	default:
		OP_ASSERT(!"Unknown filter type - memory corruption?");
	}

	return OpStatus::OK;
}

void SVGGeneratorFilterNode::ResolveColorSpace(SVGColorInterpolation colorinterp)
{
	if (colorinterp == SVGCOLORINTERPOLATION_AUTO)
		colorinterp = SVGCOLORINTERPOLATION_SRGB;

	SVGFilterNode::ResolveColorSpace(colorinterp);
}

void SVGGeneratorFilterNode::CalculateDefaultSubregion(const SVGRect& filter_region)
{
	// "If there are no referenced nodes ... the default subregion is 0%,0%,100%,100%,
	// where percentages are relative to the dimensions of the filter region"
	m_region = filter_region;
}

OP_STATUS SVGGeneratorFilterNode::CreateNoise(SVGFilterContext* context)
{
	SVGFilterNodeInfo* dst = context->GetSurface(this);
	if (!dst)
		return OpStatus::ERR_NO_MEMORY;

	if (m_noise_base_freq.x < 0 || m_noise_base_freq.y < 0)
		// Invalid parameters, leave result as transparent
		return OpStatus::OK;

	OpAutoPtr<SVGTurbulenceGenerator> generator(OP_NEW(SVGTurbulenceGenerator, ()));
	if (!generator.get())
		return OpStatus::ERR_NO_MEMORY;

	BOOL fract_sum = (m_noise.type == SVGTURBULENCE_FRACTALNOISE) ? TRUE : FALSE;
	BOOL stitch = (m_noise.stitch == SVGSTITCH_STITCH) ? TRUE : FALSE;

	const SVGMatrix& buffer_xfrm = context->GetTransform();
	SVGRect xlat_region = buffer_xfrm.ApplyToRect(m_region);
	OpRect xlat_pix = context->ResolveArea(m_region);

	generator->Init(m_noise_seed.GetIntegerValue());
	RETURN_IF_ERROR(generator->SetParameters(m_noise_base_freq.x, m_noise_base_freq.y,
											 m_noise_num_octaves.GetIntegerValue(),
											 fract_sum, stitch, xlat_region));

	int old_x = xlat_pix.x;
	int old_y = xlat_pix.y;
	xlat_pix.IntersectWith(dst->area);

	OpBitmap* noise_bm = NULL;
	// Allocate new bitmap
	RETURN_IF_ERROR(OpBitmap::Create(&noise_bm, xlat_pix.width, xlat_pix.height,
									 FALSE, TRUE, 0, 0, FALSE));
	OpAutoPtr<OpBitmap> noise_bm_cleaner(noise_bm);

	// Temporary scanline buffer
	UINT32* tmp_buf = OP_NEWA(UINT32, xlat_pix.width);
	if (!tmp_buf)
		return OpStatus::ERR_NO_MEMORY;

	SVGNumber map_step_x = m_region.width / xlat_region.width.GetRoundedIntegerValue();
	SVGNumber map_step_y = m_region.height / xlat_region.height.GetRoundedIntegerValue();

	SVGNumber pt[2]; // FIXME: I think this could just as well be an SVGNumberPair
	pt[1] = m_region.y + map_step_y * (xlat_pix.y - old_y);

	OP_STATUS status = OpStatus::OK;

	for (int y = 0; y < xlat_pix.height; y++, pt[1] += map_step_y)
	{
		pt[0] = m_region.x + map_step_x * (xlat_pix.x - old_x);

		generator->TurbulenceScanline(pt, map_step_x, tmp_buf, xlat_pix.width);
		status = noise_bm->AddLine(tmp_buf, y);
		if (OpStatus::IsError(status))
			break;
	}

	OP_DELETEA(tmp_buf);

	VEGARenderer* renderer = context->GetRenderer();

	VEGAFill* noise_fill = NULL;
	RETURN_IF_ERROR(renderer->createImage(&noise_fill, noise_bm));
	noise_fill->setQuality(VEGAFill::QUALITY_NEAREST);

	renderer->setRenderTarget(dst->target);
	renderer->setFill(noise_fill);

	xlat_pix.x -= dst->area.x;
	xlat_pix.y -= dst->area.y;

	// FIXME/OPT: This is one copy (and clear) too many - should be
	// able to use a bitmap render target, and generate the noise
	// straight into that
	OpStatus::Ignore(renderer->fillRect(xlat_pix.x, xlat_pix.y, xlat_pix.width, xlat_pix.height));

	renderer->setFill(NULL);

	OP_DELETE(noise_fill);

	return status;
}

#endif // SVG_SUPPORT_FILTERS
#endif // SVG_SUPPORT
