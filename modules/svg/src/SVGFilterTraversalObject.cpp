/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"

#ifdef SVG_SUPPORT_FILTERS

#include "modules/svg/src/SVGFilterTraversalObject.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGChildIterator.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGTraverse.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGFilter.h"
#include "modules/svg/src/svgpaintnode.h"
#include "modules/svg/src/svgfilternode.h"
#include "modules/layout/cascade.h"
#include "modules/logdoc/urlimgcontprov.h"

BOOL SVGFilterElement::EvaluateChild(HTML_Element* child)
{
	if (child->GetNsType() == NS_SVG)
	{
		Markup::Type type = child->Type();
		return
			type == Markup::SVGE_FETILE ||
			type == Markup::SVGE_FEBLEND ||
			type == Markup::SVGE_FEFLOOD ||
			type == Markup::SVGE_FEIMAGE ||
			type == Markup::SVGE_FEMERGE ||
			type == Markup::SVGE_FEOFFSET ||
			type == Markup::SVGE_FECOMPOSITE ||
			type == Markup::SVGE_FEMORPHOLOGY ||
			type == Markup::SVGE_FETURBULENCE ||
			type == Markup::SVGE_FECOLORMATRIX ||
			type == Markup::SVGE_FEGAUSSIANBLUR ||
			type == Markup::SVGE_FECONVOLVEMATRIX ||
			type == Markup::SVGE_FEDIFFUSELIGHTING ||
			type == Markup::SVGE_FEDISPLACEMENTMAP ||
			type == Markup::SVGE_FESPECULARLIGHTING ||
			type == Markup::SVGE_FECOMPONENTTRANSFER;
	}
	return FALSE;
}

/**
 * This traverser walks a filter chain
 */

OP_STATUS
SVGFilterTraversalObject::GetFilterPrimitiveRegion(HTML_Element* prim_element, SVGRect& region)
{
	// Handle common attributes for filter primitives

	// Setup an appropriate boundingbox that is used to resolve
	// primitive dimensions to userspace values.
	SVGBoundingBox bbox;
	if (m_filter->GetPrimitiveUnits() == SVGUNITS_OBJECTBBOX)
	{
		bbox = m_filter->GetSourceElementBBox();
	}
	else
	{
		bbox.minx = 0;
		bbox.miny = 0;
		bbox.maxx = 1;
		bbox.maxy = 1;
	}

	const SVGValueContext& vcxt = GetValueContext();
	if (SVGUtils::GetResolvedLengthWithUnits(prim_element, Markup::SVGA_X, SVGLength::SVGLENGTH_X,
											 m_filter->GetPrimitiveUnits(), vcxt, region.x))
	{
		region.x *= bbox.maxx - bbox.minx;
		region.x += bbox.minx;
	}
	if (SVGUtils::GetResolvedLengthWithUnits(prim_element, Markup::SVGA_Y, SVGLength::SVGLENGTH_Y,
											 m_filter->GetPrimitiveUnits(), vcxt, region.y))
	{
		region.y *= bbox.maxy - bbox.miny;
		region.y += bbox.miny;
	}
	if (SVGUtils::GetResolvedLengthWithUnits(prim_element, Markup::SVGA_WIDTH, SVGLength::SVGLENGTH_X,
											 m_filter->GetPrimitiveUnits(), vcxt, region.width))
	{
		region.width *= bbox.maxx - bbox.minx;
	}
	if (SVGUtils::GetResolvedLengthWithUnits(prim_element, Markup::SVGA_HEIGHT, SVGLength::SVGLENGTH_Y,
											 m_filter->GetPrimitiveUnits(), vcxt, region.height))
	{
		region.height *= bbox.maxy - bbox.miny;
	}

	// "A negative value is an error"
	if (region.width < 0 || region.height < 0)
		return OpSVGStatus::INVALID_ARGUMENT;

	return OpStatus::OK;
}

SVGFilterNode* SVGFilterTraversalObject::LookupInput(HTML_Element* elm, Markup::AttrType input_attr)
{
	// Get input
	SVGString* in_ref;
	AttrValueStore::GetString(elm, input_attr, &in_ref);

	if (in_ref)
	{
		unsigned len = in_ref->GetLength();
		if (len >= 9)
		{
			const uni_char* s = in_ref->GetString();

			if (len == 13 && uni_strncmp(s, "SourceGraphic", 13) == 0)
				return m_filter->GetFilterInputNode(INPUTNODE_SOURCEGRAPHIC);
			else if (len == 11 && uni_strncmp(s, "SourceAlpha", 11) == 0)
				return m_filter->GetFilterInputNode(INPUTNODE_SOURCEALPHA);
			else if (len == 15 && uni_strncmp(s, "BackgroundImage", 15) == 0)
				return m_filter->GetFilterInputNode(INPUTNODE_BACKGROUNDIMAGE);
			else if (len == 15 && uni_strncmp(s, "BackgroundAlpha", 15) == 0)
				return m_filter->GetFilterInputNode(INPUTNODE_BACKGROUNDALPHA);
			else if (len == 9 && uni_strncmp(s, "FillPaint", 9) == 0)
				return m_filter->GetFilterInputNode(INPUTNODE_FILLPAINT);
			else if (len == 11 && uni_strncmp(s, "StrokePaint", 11) == 0)
				return m_filter->GetFilterInputNode(INPUTNODE_STROKEPAINT);
		}

		// Resolve named references
		if (m_res_store.GetCount() > 0)
		{
			// Look for matching string
			for (int i = m_res_store.GetCount() - 1; i >= 0; i--)
			{
				SVGString* res = m_res_store.Get(i);
				if (res && res->GetString() && (res->GetLength() == in_ref->GetLength()) &&
					uni_strncmp(res->GetString(), in_ref->GetString(), res->GetLength()) == 0)
					return m_filter->GetFilterNodeAt(i);
			}
		}
	}

	// If no name, or the name lookup fails, the reference is implicit
	// and refers to either the previous primitive or the
	// source(graphic) (if this is the first primitive).

	if (m_res_store.GetCount() > 0)
		// Implicit - previous primitive
		return m_filter->GetFilterNodeAt(m_res_store.GetCount() - 1);

	// Implicit if nothing has been filtered prior to this
	return m_filter->GetFilterInputNode(INPUTNODE_SOURCEGRAPHIC);
}

OP_STATUS SVGFilterTraversalObject::PushNode(SVGString* result_name, SVGFilterNode* filter_node)
{
	// Put result string on lookup stack
	RETURN_IF_ERROR(m_res_store.Add(result_name));

	// Push node
	OP_STATUS status = m_filter->PushFilterNode(filter_node);
	if (OpStatus::IsError(status))
	{
		// Pop the result name from the result stack just in case
		m_res_store.Remove(m_res_store.GetCount() - 1);
	}
	return status;
}

OP_STATUS SVGFEPrimitive::HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	return traversal_object->HandleFilterPrimitive(info);
}

// Create paint nodes that will be used by an 'feImage' element
void SVGFilterTraversalObject::CreateFeImage(HTML_Element* feimage_element,
											 const SVGRect& feimage_region,
											 SVGDocumentContext* doc_ctx,
											 SVGElementResolver* resolver,
											 SVGPaintNode*& paint_node)
{
	URL* url = NULL;
	RETURN_VOID_IF_ERROR(AttrValueStore::GetXLinkHREF(doc_ctx->GetURL(), feimage_element, url));
	if (url == NULL)
		// Failed to get an url to an image.
		// (??? Set error code in order to allocate a surface and pass it along.)
		return;

	SVGAspectRatio* ar = NULL;
	AttrValueStore::GetPreserveAspectRatio(feimage_element, ar);

	URL doc_url = doc_ctx->GetURL();
	URL redirected_to = doc_url.GetAttribute(URL::KMovedToURL, TRUE);
	if (!redirected_to.IsEmpty())
		doc_url = redirected_to;

	BOOL same_doc = (*url == doc_url);

	if (!same_doc && url->GetAttribute(URL::KIsImage, TRUE))
	{
		URLStatus url_stat = (URLStatus)url->GetAttribute(URL::KLoadStatus, TRUE);
		if (url_stat != URL_LOADED)
			// Image url, but not loaded
			return;

		/* Render as if 'image' */
		OpAutoPtr<SVGRasterImageNode> fei_raster_node(OP_NEW(SVGRasterImageNode, ()));
		if (!fei_raster_node.get())
			return;

		// FIXME: This will skip overflow

		// Just assure that we have allocated a UrlImageContentProvider, skipping that while painting
		UrlImageContentProvider	*provider = UrlImageContentProvider::FindImageContentProvider(*url);
		if (!provider)
		{
			provider = OP_NEW(UrlImageContentProvider, (*url));
			if (!provider)
				return;
		}

		FramesDocument* frm_doc = doc_ctx->GetDocument();

#ifdef _PRINT_SUPPORT_
		// Call this magic function (magic to me, at least) that creates a
		// fitting HEListElm for images in print documents that we fetch
		// below.
		if (frm_doc->IsPrintDocument())
			frm_doc->GetHLDocProfile()->AddPrintImageElement(feimage_element, IMAGE_INLINE, url);
#endif // _PRINT_SUPPORT_

		HEListElm* hle = feimage_element->GetHEListElm(FALSE);
		if (!hle)
			return;

		provider->IncRef();
		// Scope so that the Image object is destroyed before the
		// provider.
		{
			// To force loading - we use a big rectangle to make sure that
			// the image isn't undisplayed by accident when the user
			// scrolls The best rect would be the position and size of the
			// svg in the document, but we don't have access to that here.
			// Triggers an IncVisible that the HEListElm
			// owns. Unfortunately gets the wrong coordinates.
			hle->Display(frm_doc, AffinePos(), 20000, 1000000, FALSE, FALSE);

			Image img = provider->GetImage();

			OpStatus::Ignore(img.OnLoadAll(provider));
		}
		provider->DecRef();

		// FIXME: Clipping
		fei_raster_node->Reset();

		fei_raster_node->SetImageAspect(ar);
		fei_raster_node->SetImageViewport(feimage_region);
		fei_raster_node->SetImageQuality(SVGPainter::IMAGE_QUALITY_NORMAL);
		fei_raster_node->SetImageContext(*url, feimage_element, doc_ctx->GetDocument());

		paint_node = fei_raster_node.release();
	}
	else
	{
		// Not a (raster) image
		BOOL has_fragid = !!url->UniRelName();

		if ((!same_doc || !has_fragid)
#ifdef SVG_SUPPORT_SVG_IN_IMAGE
			  && url->ContentType() != URL_SVG_CONTENT
			  && url->ContentType() != URL_XML_CONTENT
#endif // SVG_SUPPORT_SVG_IN_IMAGE
			)
			// Unexpected/unconforming content
			return;

		// No fragment identifier - render as image
		// Has fragment identifier - render as 'use'
		HTML_Element* target = doc_ctx->GetElementByReference(resolver, feimage_element, *url);
		if (!target)
			return;

		OP_ASSERT(target->Type() == Markup::SVGE_SVG || has_fragid);

		if (same_doc)
			doc_ctx->RegisterDependency(feimage_element, target);

		if (!same_doc)
		{
			// NOTE: Overwrites the document context that was passed in
			doc_ctx = AttrValueStore::GetSVGDocumentContext(target);
			if (!doc_ctx)
				return;

			if (!has_fragid)
				// FIXME: This is ugly and most likely broken in certain cases
				doc_ctx->ForceSize(feimage_region.width.GetIntegerValue(),
								   feimage_region.height.GetIntegerValue());
		}

		/* Render as if 'image' */
		OpAutoPtr<SVGVectorImageNode> fei_vector_node(OP_NEW(SVGVectorImageNode, ()));
		if (!fei_vector_node.get())
			return;

		SVGMatrix viewport_xfrm;
		// Setup viewport and coordinate system to draw in
		viewport_xfrm.LoadTranslation(feimage_region.x, feimage_region.y);

		SVGNumberPair image_viewport(feimage_region.width, feimage_region.height);

		if (has_fragid && m_filter->GetPrimitiveUnits() == SVGUNITS_OBJECTBBOX)
		{
			SVGBoundingBox bbox = m_filter->GetSourceElementBBox();

			image_viewport.x /= bbox.maxx - bbox.minx;
			image_viewport.y /= bbox.maxy - bbox.miny;

			viewport_xfrm.PostMultScale(bbox.maxx - bbox.minx, bbox.maxy - bbox.miny);
		}

		fei_vector_node->SetTransform(viewport_xfrm);

		SVGElementResolverStack resolver_mark(resolver);
		RETURN_VOID_IF_ERROR(resolver_mark.Push(target));

		SVGNullCanvas canvas;
		canvas.GetTransform().Copy(viewport_xfrm);

		SVGRenderingTreeChildIterator rtci;
		SVGPaintTreeBuilder tree_builder(&rtci, fei_vector_node.get());
		tree_builder.SetCanvas(&canvas);
		tree_builder.SetupResolver(resolver);
		tree_builder.SetDocumentContext(doc_ctx);
		tree_builder.SetCurrentViewport(image_viewport);

		RETURN_VOID_IF_ERROR(SVGTraverser::Traverse(&tree_builder, target, NULL));

		paint_node = fei_vector_node.release();
	}
}

/**
 * Get a light source element (one of feDistantLight, fePointLight and feSpotLight)
 *
 * @param elm Parent element
 * @param light The light source structure to fill in
 *
 * @return OpStatus::OK if successful
 */
void SVGFilterTraversalObject::GetLightSource(HTML_Element* elm, SVGLightSource& light)
{
	// Traverse child nodes looking for light sources (fe*Light:s)
	HTML_Element *child_elm = elm->FirstChild();
	while (child_elm)
	{
		if (child_elm->GetNsType() != NS_SVG)
		{
			child_elm = child_elm->Suc();
			continue;
		}

		switch (child_elm->Type())
		{
		case Markup::SVGE_FEDISTANTLIGHT:
			{
				light.type = SVGLightSource::SVGLIGHTSOURCE_DISTANT;

				SVGNumber azimuth;
				SVGNumber elevation;

				AttrValueStore::GetNumber(child_elm, Markup::SVGA_AZIMUTH, azimuth);
				AttrValueStore::GetNumber(child_elm, Markup::SVGA_ELEVATION, elevation);

				azimuth = azimuth * SVGNumber::pi() / 180;
				elevation = elevation * SVGNumber::pi() / 180;

				// Calculate the light vector
				//
				// Lx = cos(azimuth)*cos(elevation)
				// Ly = sin(azimuth)*cos(elevation)
				// Lz = sin(elevation)
				//
				light.x = azimuth.cos() * elevation.cos();
				light.y = azimuth.sin() * elevation.cos();
				light.z = elevation.sin();
			}
			return;

		case Markup::SVGE_FEPOINTLIGHT:
			{
				light.type = SVGLightSource::SVGLIGHTSOURCE_POINT;

				// Get the light position
				light.x = m_filter->GetResolvedNumber(child_elm, Markup::SVGA_X, SVGLength::SVGLENGTH_X, TRUE);
				light.y = m_filter->GetResolvedNumber(child_elm, Markup::SVGA_Y, SVGLength::SVGLENGTH_Y, TRUE);
				light.z = m_filter->GetResolvedNumber(child_elm, Markup::SVGA_Z, SVGLength::SVGLENGTH_OTHER, FALSE);
			}
			return;

		case Markup::SVGE_FESPOTLIGHT:
			{
				light.type = SVGLightSource::SVGLIGHTSOURCE_SPOT;

				// Get the light position
				light.x = m_filter->GetResolvedNumber(child_elm, Markup::SVGA_X, SVGLength::SVGLENGTH_X, TRUE);
				light.y = m_filter->GetResolvedNumber(child_elm, Markup::SVGA_Y, SVGLength::SVGLENGTH_Y, TRUE);
				light.z = m_filter->GetResolvedNumber(child_elm, Markup::SVGA_Z, SVGLength::SVGLENGTH_OTHER, FALSE);

				// Get the what the light points at
				light.pointsAtX = m_filter->GetResolvedNumber(child_elm, Markup::SVGA_POINTSATX, SVGLength::SVGLENGTH_X, TRUE);
				light.pointsAtY = m_filter->GetResolvedNumber(child_elm, Markup::SVGA_POINTSATY, SVGLength::SVGLENGTH_Y, TRUE);
				light.pointsAtZ = m_filter->GetResolvedNumber(child_elm, Markup::SVGA_POINTSATZ, SVGLength::SVGLENGTH_OTHER, FALSE);

				// Get light attributes (cone angle/specular exponent)
				SVGNumberObject* cone_ang = NULL;
				AttrValueStore::GetNumberObject(child_elm, Markup::SVGA_LIMITINGCONEANGLE, &cone_ang);
				if (cone_ang != NULL)
				{
					light.has_cone_angle = TRUE;
					light.cone_angle = cone_ang->value.abs();
				}
				else
				{
					light.has_cone_angle = FALSE;
				}

				AttrValueStore::GetNumber(child_elm, Markup::SVGA_SPECULAREXPONENT, light.spec_exponent, 1);
			}
			return;

		default:
			break;
		}

		child_elm = child_elm->Suc();
	}

	// No light source found - using distant with azimuth = elevation = 0
}

static BOOL IsValidStdDev(const SVGNumberPair& stddev)
{
	// Default == 0, 0 disables effect
	return
		!(stddev.x.Equal(0) && stddev.y.Equal(0) ||
		  stddev.x < 0 || stddev.y < 0);
}
static BOOL IsValidMorphRadius(const SVGNumberPair& radius)
{
	// Default == 0, 0 disables effect
	return
		!(radius.x.Equal(0) || radius.y.Equal(0) ||
		  radius.x < 0 || radius.y < 0);
}

OP_STATUS SVGFilterTraversalObject::HandleFilterPrimitive(SVGElementInfo& info)
{
	HTML_Element* layouted_elm = info.layouted;
	Markup::Type type = layouted_elm->Type();

	OpAutoPtr<SVGFilterNode> filter_node(NULL);
	const HTMLayoutProperties& props = *info.props->GetProps();
	const SvgProperties *svg_props = props.svg;

	// Determine filtertype, and prepare as needed
	switch (type)
	{
		// The following filters has no 'in' attribute
	case Markup::SVGE_FETURBULENCE:
		{
			SVGNumberPair base_freq;
			SVGUtils::GetNumberOptionalNumber(layouted_elm, Markup::SVGA_BASEFREQUENCY, base_freq);
			SVGNumber num_octaves;
			AttrValueStore::GetNumber(layouted_elm, Markup::SVGA_NUMOCTAVES, num_octaves, 1);
			SVGNumber seed;
			AttrValueStore::GetNumber(layouted_elm, Markup::SVGA_SEED, seed, 0);

			SVGStitchType stitch_type =
				(SVGStitchType)AttrValueStore::GetEnumValue(layouted_elm, Markup::SVGA_STITCHTILES,
															SVGENUM_STITCHTILES, SVGSTITCH_NOSTITCH);
			SVGTurbulenceType turb_type =
				(SVGTurbulenceType)AttrValueStore::GetEnumValue(layouted_elm, Markup::SVGA_TYPE,
																SVGENUM_TURBULENCETYPE,
																SVGTURBULENCE_TURBULENCE);

			SVGGeneratorFilterNode* gen_filter_node =
				SVGGeneratorFilterNode::CreateNoise(turb_type, base_freq,
													num_octaves, seed, stitch_type);
			if (!gen_filter_node)
				return OpStatus::ERR_NO_MEMORY;

			filter_node.reset(gen_filter_node);
		}
		break;

	case Markup::SVGE_FEIMAGE:
		{
			// This is a bit of hack, but we need the primitive region
			// before creating the filter node, so that we can setup
			// the paint node correctly. Since the default subregion
			// is trivial on 'feImage' this is no biggy.
			SVGRect primitive_region = m_filter->GetFilterRegion();
			RETURN_IF_ERROR(GetFilterPrimitiveRegion(layouted_elm, primitive_region));

			SVGPaintNode* image_paint_node = NULL;
			CreateFeImage(layouted_elm, primitive_region, m_doc_ctx, m_resolver, image_paint_node);

			SVGGeneratorFilterNode* gen_filter_node =
				SVGGeneratorFilterNode::CreateImage(image_paint_node);
			if (!gen_filter_node)
			{
				if (image_paint_node)
					image_paint_node->Free();
				return OpStatus::ERR_NO_MEMORY;
			}

			filter_node.reset(gen_filter_node);
		}
		break;

	case Markup::SVGE_FEFLOOD:
		{
			OP_ASSERT(svg_props->floodcolor.GetColorType() != SVGColor::SVGCOLOR_CURRENT_COLOR &&
					  "Hah! currentColor not resolved");
			UINT32 flood_color = svg_props->floodcolor.GetRGBColor();
			UINT8 flood_opacity = svg_props->floodopacity;

			SVGGeneratorFilterNode* gen_filter_node =
				SVGGeneratorFilterNode::CreateFlood(flood_color, flood_opacity);
			if (!gen_filter_node)
				return OpStatus::ERR_NO_MEMORY;

			filter_node.reset(gen_filter_node);
		}
		break;

	case Markup::SVGE_FEDIFFUSELIGHTING:
	case Markup::SVGE_FESPECULARLIGHTING:
	case Markup::SVGE_FECONVOLVEMATRIX:
		{
			SVGScaledUnaryFilterNode* sclu_filter_node;

			if (type == Markup::SVGE_FECONVOLVEMATRIX)
			{
				SVGVector* kernmat = NULL;

				AttrValueStore::GetVector(layouted_elm, Markup::SVGA_KERNELMATRIX, kernmat);
				if (kernmat == NULL)
				{
					SVG_NEW_ERROR(layouted_elm);
					SVG_REPORT_ERROR((UNI_L("Missing attribute kernelMatrix.")));
				}

				SVGNumberPair order;
				SVGUtils::GetNumberOptionalNumber(layouted_elm, Markup::SVGA_ORDER, order, 3);

				/* Calculate default divisor (SUM(kernel))*/
				SVGNumber def_div = 0;
				if (kernmat)
				{
					for (unsigned int i = 0; i < kernmat->GetCount(); i++)
						if (SVGNumberObject* nval = static_cast<SVGNumberObject*>(kernmat->Get(i)))
							def_div += nval->value;
				}

				SVGNumber divisor;
				AttrValueStore::GetNumber(layouted_elm, Markup::SVGA_DIVISOR, divisor, def_div);
				if (divisor.Equal(0)) /* if the sum is zero, the default divisor is 1 */
					divisor = 1;

				SVGNumber bias;
				AttrValueStore::GetNumber(layouted_elm, Markup::SVGA_BIAS, bias, 0);

				SVGNumberPair tgt;
				// If order is busted, then ignore target
				if (order.x > 0 && order.y > 0)
				{
					AttrValueStore::GetNumber(layouted_elm, Markup::SVGA_TARGETX, tgt.x, order.x / 2);
					AttrValueStore::GetNumber(layouted_elm, Markup::SVGA_TARGETY, tgt.y, order.y / 2);

					if (tgt.x < 0 || tgt.x >= order.x)
						tgt.x = order.x / 2;
					if (tgt.y < 0 || tgt.y >= order.y)
						tgt.y = order.y / 2;
				}

				bool preserve_alpha =
					!!AttrValueStore::GetEnumValue(layouted_elm,
												   Markup::SVGA_PRESERVEALPHA, SVGENUM_BOOLEAN, FALSE);

				SVGConvolveEdgeMode edge_mode =
					(SVGConvolveEdgeMode)AttrValueStore::GetEnumValue(layouted_elm,
																	  Markup::SVGA_EDGEMODE,
																	  SVGENUM_CONVOLVEEDGEMODE,
																	  SVGCONVOLVEEDGEMODE_DUPLICATE);
				sclu_filter_node =
					SVGScaledUnaryFilterNode::CreateConvolve(kernmat, order, tgt, divisor, bias,
															 preserve_alpha, edge_mode);
			}
			else
			{
				OP_ASSERT(type == Markup::SVGE_FEDIFFUSELIGHTING || type == Markup::SVGE_FESPECULARLIGHTING);

				// Fetch common light configuration
				SVGLightSource light;
				GetLightSource(layouted_elm, light);

				SVGNumber surf_scale;
				AttrValueStore::GetNumber(layouted_elm, Markup::SVGA_SURFACESCALE, surf_scale, 1);

				OP_ASSERT(svg_props->lightingcolor.GetColorType() != SVGColor::SVGCOLOR_CURRENT_COLOR &&
						  "Hah! currentColor not resolved");
				UINT32 light_color = svg_props->lightingcolor.GetRGBColor();

				if (type == Markup::SVGE_FESPECULARLIGHTING)
				{
					// Fetch specular configuration
					SVGNumber ks, spec_exp;
					AttrValueStore::GetNumber(layouted_elm, Markup::SVGA_SPECULARCONSTANT, ks, 1);
					if (ks < 0)
						ks = 0;

					AttrValueStore::GetNumber(layouted_elm, Markup::SVGA_SPECULAREXPONENT, spec_exp, 1);
					if (spec_exp < 1)
						spec_exp = 1;
					else if (spec_exp > 128)
						spec_exp = 128;

					sclu_filter_node = SVGScaledUnaryFilterNode::CreateLighting(light, surf_scale,
																				light_color,
																				true /* specular */,
																				ks, spec_exp);
				}
				else
				{
					// Fetch diffuse configuration
					SVGNumber kd;
					AttrValueStore::GetNumber(layouted_elm, Markup::SVGA_DIFFUSECONSTANT, kd, 1);

					sclu_filter_node = SVGScaledUnaryFilterNode::CreateLighting(light, surf_scale,
																				light_color,
																				false /* diffuse */,
																				kd);
				}
			}

			if (!sclu_filter_node)
				return OpStatus::ERR_NO_MEMORY;

			// Fetch possible kernelUnitLength
			if (AttrValueStore::HasObject(layouted_elm, Markup::SVGA_KERNELUNITLENGTH, NS_IDX_SVG))
			{
				SVGNumberPair unit_len = m_filter->GetResolvedNumberOptionalNumber(layouted_elm, Markup::SVGA_KERNELUNITLENGTH, 1);
				if (unit_len.x > 0 && unit_len.y > 0)
					sclu_filter_node->SetUnitLength(unit_len);
			}

			SVGFilterNode* input_node = LookupInput(layouted_elm, Markup::SVGA_IN);
			sclu_filter_node->SetInput(input_node);

			filter_node.reset(sclu_filter_node);
		}
		break;

	case Markup::SVGE_FEGAUSSIANBLUR:
		{
			SVGNumberPair stddev = m_filter->GetResolvedNumberOptionalNumber(layouted_elm, Markup::SVGA_STDDEVIATION, 0);

			if (!IsValidStdDev(stddev))
			{
				stddev.x = 0;
				stddev.y = 0;
			}

			SVGSimpleUnaryFilterNode* unary_filter_node =
				SVGSimpleUnaryFilterNode::CreateGaussian(stddev);
			if (!unary_filter_node)
				return OpStatus::ERR_NO_MEMORY;

			SVGFilterNode* input_node = LookupInput(layouted_elm, Markup::SVGA_IN);
			unary_filter_node->SetInput(input_node);

			filter_node.reset(unary_filter_node);
		}
		break;

	case Markup::SVGE_FEMORPHOLOGY:
		{
			SVGNumberPair radius = m_filter->GetResolvedNumberOptionalNumber(layouted_elm, Markup::SVGA_RADIUS, 0);

			// FIXME: Should < 0 be handled as zero, or as before (i.e, empty result)?
			// The below handles it as zero
			if (!IsValidMorphRadius(radius))
			{
				radius.x = 0;
				radius.y = 0;
			}

			SVGMorphologyOperator morphop =
				(SVGMorphologyOperator)AttrValueStore::GetEnumValue(layouted_elm, Markup::SVGA_OPERATOR,
																	SVGENUM_MORPHOLOGYOPERATOR,
																	SVGMORPHOPERATOR_ERODE);

			SVGSimpleUnaryFilterNode* unary_filter_node =
				SVGSimpleUnaryFilterNode::CreateMorphology(morphop, radius);
			if (!unary_filter_node)
				return OpStatus::ERR_NO_MEMORY;

			SVGFilterNode* input_node = LookupInput(layouted_elm, Markup::SVGA_IN);
			unary_filter_node->SetInput(input_node);

			filter_node.reset(unary_filter_node);
		}
		break;

	case Markup::SVGE_FEOFFSET:
		{
			SVGNumberPair offset;
			offset.x = m_filter->GetResolvedNumber(layouted_elm, Markup::SVGA_DX, SVGLength::SVGLENGTH_X, FALSE);
			offset.y = m_filter->GetResolvedNumber(layouted_elm, Markup::SVGA_DY, SVGLength::SVGLENGTH_Y, FALSE);

			SVGSimpleUnaryFilterNode* unary_filter_node = SVGSimpleUnaryFilterNode::CreateOffset(offset);
			if (!unary_filter_node)
				return OpStatus::ERR_NO_MEMORY;

			SVGFilterNode* input_node = LookupInput(layouted_elm, Markup::SVGA_IN);
			unary_filter_node->SetInput(input_node);

			filter_node.reset(unary_filter_node);
		}
		break;

	case Markup::SVGE_FECOMPONENTTRANSFER:
		{
			/* Red = 0, Green = 1, Blue = 2, Alpha = 3 */
			SVGCompFunc* funcs = OP_NEWA(SVGCompFunc, 4);
			if (!funcs)
				return OpStatus::ERR_NO_MEMORY;

			// Collect child nodes (feFuncX:s)
			for (HTML_Element* child_elm = layouted_elm->FirstChild(); child_elm;
				 child_elm = child_elm->Suc())
			{
				if (child_elm->GetNsType() != NS_SVG)
					continue;

				int func_idx = 0;
				switch (child_elm->Type())
				{
				case Markup::SVGE_FEFUNCR:
					func_idx = 0;
					break;
				case Markup::SVGE_FEFUNCG:
					func_idx = 1;
					break;
				case Markup::SVGE_FEFUNCB:
					func_idx = 2;
					break;
				case Markup::SVGE_FEFUNCA:
					func_idx = 3;
					break;
				default:
					continue;
				}

				funcs[func_idx].type =
					(SVGFuncType)AttrValueStore::GetEnumValue(child_elm, Markup::SVGA_TYPE,
															  SVGENUM_FUNCTYPE, SVGFUNC_IDENTITY);

				AttrValueStore::GetVector(child_elm, Markup::SVGA_TABLEVALUES, funcs[func_idx].table);
				AttrValueStore::GetNumber(child_elm, Markup::SVGA_SLOPE, funcs[func_idx].slope, 1);
				AttrValueStore::GetNumber(child_elm, Markup::SVGA_INTERCEPT, funcs[func_idx].intercept, 0);
				AttrValueStore::GetNumber(child_elm, Markup::SVGA_AMPLITUDE, funcs[func_idx].amplitude, 1);
				AttrValueStore::GetNumber(child_elm, Markup::SVGA_EXPONENT, funcs[func_idx].exponent, 1);
				AttrValueStore::GetNumber(child_elm, Markup::SVGA_OFFSET, funcs[func_idx].offset, 0);

				SVGObject::IncRef(funcs[func_idx].table);
			}

			SVGSimpleUnaryFilterNode* unary_filter_node =
				SVGSimpleUnaryFilterNode::CreateComponentTransfer(funcs);
			if (!unary_filter_node)
				return OpStatus::ERR_NO_MEMORY;

			SVGFilterNode* input_node = LookupInput(layouted_elm, Markup::SVGA_IN);
			unary_filter_node->SetInput(input_node);

			filter_node.reset(unary_filter_node);
		}
		break;

	case Markup::SVGE_FECOLORMATRIX:
		{
			SVGColorMatrixType colmattype =
				(SVGColorMatrixType)AttrValueStore::GetEnumValue(layouted_elm, Markup::SVGA_TYPE,
																 SVGENUM_COLORMATRIXTYPE,
																 SVGCOLORMATRIX_MATRIX);

			SVGVector* values = NULL;
			if (OpStatus::IsError(AttrValueStore::GetVectorWithStatus(layouted_elm, Markup::SVGA_VALUES, values)))
			{
				// These settings should be equal to using an identity matrix,
				// which happens to match the default values for the types where
				// 'values' are applicable
				colmattype = SVGCOLORMATRIX_MATRIX;
				values = NULL;
			}

			SVGSimpleUnaryFilterNode* unary_filter_node =
				SVGSimpleUnaryFilterNode::CreateColorMatrix(colmattype, values);
			if (!unary_filter_node)
				return OpStatus::ERR_NO_MEMORY;

			SVGFilterNode* input_node = LookupInput(layouted_elm, Markup::SVGA_IN);
			unary_filter_node->SetInput(input_node);

			filter_node.reset(unary_filter_node);
		}
		break;

	case Markup::SVGE_FETILE:
		{
			SVGFilterNode* input_node = LookupInput(layouted_elm, Markup::SVGA_IN);
			SVGRect in_region = input_node ? input_node->GetRegion() : m_filter->GetFilterRegion();

			SVGSimpleUnaryFilterNode* unary_filter_node =
				SVGSimpleUnaryFilterNode::CreateTile(in_region);
			if (!unary_filter_node)
				return OpStatus::ERR_NO_MEMORY;

			unary_filter_node->SetInput(input_node);

			filter_node.reset(unary_filter_node);
		}
		break;

	case Markup::SVGE_FEBLEND:
	case Markup::SVGE_FECOMPOSITE:
	case Markup::SVGE_FEDISPLACEMENTMAP:
		{
			// Binary operations
			SVGBinaryFilterNode* binary_filter_node;
			if (type == Markup::SVGE_FEBLEND)
			{
				SVGBlendMode bmode = (SVGBlendMode)AttrValueStore::GetEnumValue(layouted_elm,
																				Markup::SVGA_MODE,
																				SVGENUM_BLENDMODE,
																				SVGBLENDMODE_NORMAL);
				binary_filter_node = SVGBinaryFilterNode::CreateBlend(bmode);
			}
			else if (type == Markup::SVGE_FECOMPOSITE)
			{
				SVGCompositeOperator comp_op =
					(SVGCompositeOperator)AttrValueStore::GetEnumValue(layouted_elm,
																	   Markup::SVGA_OPERATOR,
																	   SVGENUM_COMPOSITEOPERATOR,
																	   SVGCOMPOSITEOPERATOR_OVER);

				/* Parameters for 'arithmetic' */
				SVGNumber k1, k2, k3, k4;
				AttrValueStore::GetNumber(layouted_elm, Markup::SVGA_K1, k1, 0);
				AttrValueStore::GetNumber(layouted_elm, Markup::SVGA_K2, k2, 0);
				AttrValueStore::GetNumber(layouted_elm, Markup::SVGA_K3, k3, 0);
				AttrValueStore::GetNumber(layouted_elm, Markup::SVGA_K4, k4, 0);

				binary_filter_node = SVGBinaryFilterNode::CreateComposite(comp_op, k1, k2, k3, k4);
			}
			else /* Markup::SVGE_FEDISPLACEMENTMAP */
			{
				OP_ASSERT(type == Markup::SVGE_FEDISPLACEMENTMAP);

				SVGNumber scale = m_filter->GetResolvedNumber(layouted_elm, Markup::SVGA_SCALE, SVGLength::SVGLENGTH_OTHER, FALSE);

				SVGDisplacementSelector x_chan_sel =
					(SVGDisplacementSelector)AttrValueStore::GetEnumValue(layouted_elm,
																		  Markup::SVGA_XCHANNELSELECTOR,
																		  SVGENUM_DISPLACEMENTSELECTOR,
																		  SVGDISPLACEMENT_A);
				SVGDisplacementSelector y_chan_sel =
					(SVGDisplacementSelector)AttrValueStore::GetEnumValue(layouted_elm,
																		  Markup::SVGA_YCHANNELSELECTOR,
																		  SVGENUM_DISPLACEMENTSELECTOR,
																		  SVGDISPLACEMENT_A);

				binary_filter_node = SVGBinaryFilterNode::CreateDisplacement(x_chan_sel, y_chan_sel, scale);
			}

			if (!binary_filter_node)
				return OpStatus::ERR_NO_MEMORY;

			// This might look odd, but is intended
			SVGFilterNode* input2 = LookupInput(layouted_elm, Markup::SVGA_IN);
			SVGFilterNode* input1 = LookupInput(layouted_elm, Markup::SVGA_IN2);

			if (type == Markup::SVGE_FEDISPLACEMENTMAP)
			{
				// For the displacement filter, 'in' is the input to
				// displace, and 'in2' is the actual displacement map.
				SVGFilterNode* tmp = input1;
				input1 = input2;
				input2 = tmp;
			}

			binary_filter_node->SetFirstInput(input1);
			binary_filter_node->SetSecondInput(input2);

			filter_node.reset(binary_filter_node);
		}
		break;

	case Markup::SVGE_FEMERGE:
		{
			// This is a potentially N-ary operation
			// Collect all child nodes (feMergeNode:s)
			HTML_Element *child_elm = layouted_elm->FirstChild();
			unsigned int input_count = 0;
			while (child_elm)
			{
				if (child_elm->IsMatchingType(Markup::SVGE_FEMERGENODE, NS_SVG))
					input_count++;

				child_elm = child_elm->Suc();
			}

			SVGNaryFilterNode* merge_filter_node = SVGNaryFilterNode::Create(input_count);
			if (!merge_filter_node)
				return OpStatus::ERR_NO_MEMORY;

			child_elm = layouted_elm->FirstChild();
			input_count = 0;
			while (child_elm)
			{
				if (child_elm->IsMatchingType(Markup::SVGE_FEMERGENODE, NS_SVG))
				{
					SVGFilterNode* input_node = LookupInput(child_elm, Markup::SVGA_IN);
					merge_filter_node->SetInputAt(input_count, input_node);

					input_count++;
				}

				child_elm = child_elm->Suc();
			}

			filter_node.reset(merge_filter_node);
		}
		break;

	default:
		break;
	}

	if (filter_node.get())
	{
		// Resolve the colorspace for this node
		SVGColorInterpolation colorinterp =
			(SVGColorInterpolation)svg_props->rendinfo.color_interpolation_filters;
		filter_node->ResolveColorSpace(colorinterp);

		// Calculate the default subregion
		filter_node->CalculateDefaultSubregion(m_filter->GetFilterRegion());

		// Fetch and setup the primitive region
		SVGRect primitive_region = filter_node->GetRegion();
		RETURN_IF_ERROR(GetFilterPrimitiveRegion(layouted_elm, primitive_region));
		filter_node->SetRegion(primitive_region);

		// Get 'result' attribute
		SVGString* result_ref;
		AttrValueStore::GetString(layouted_elm, Markup::SVGA_RESULT, &result_ref);

		// Add node (and result reference) to filter
		RETURN_IF_ERROR(PushNode(result_ref, filter_node.get()));

		// Filter node is now owned by the SVGFilter - let go of the
		// reference
		filter_node.release();
	}

	return OpStatus::OK;
}

#endif // SVG_SUPPORT_FILTERS
#endif // SVG_SUPPORT
