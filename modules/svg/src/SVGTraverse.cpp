/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/svg/src/svgpch.h"

#if defined(SVG_SUPPORT)

#include "modules/svg/src/SVGTraverse.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGCanvas.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/OpBpath.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGElementStateContext.h"
#include "modules/svg/src/SVGTextElementStateContext.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGGradient.h"
#include "modules/svg/src/SVGPattern.h"
#include "modules/svg/src/SVGFilter.h"
#include "modules/svg/src/SVGTextSelection.h"
#include "modules/svg/src/SVGPoint.h"
#include "modules/svg/src/SVGTextTraverse.h"
#include "modules/svg/src/SVGTextLayout.h"
#include "modules/svg/src/SVGAttributeParser.h"
#include "modules/svg/src/SVGEditable.h"
#include "modules/svg/src/SVGMarker.h"
#include "modules/svg/src/SVGChildIterator.h"
#include "modules/svg/src/svgpaintnode.h"

#include "modules/probetools/probepoints.h"

#include "modules/display/vis_dev.h"

#include "modules/dochand/fdelm.h"

#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpSystemInfo.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/logdoc/src/textdata.h"
#include "modules/logdoc/xmlenum.h" // For XMLA_SPACE

#include "modules/layout/box/box.h"
#include "modules/layout/cascade.h"
#include "modules/layout/content/content.h"
#include "modules/layout/layout_workplace.h"

#include "modules/style/css_all_properties.h"
#include "modules/style/css_media.h"

#ifdef SVG_SUPPORT_MEDIA
#include "modules/media/mediaplayer.h"
#endif // SVG_SUPPORT_MEDIA

// Map from a CSSValue stored in the cascade to a value we can use for
// the SVGCanvas/SVGPainter.
static SVGCanvas::ImageRenderQuality SVGGetImageQualityFromProps(const HTMLayoutProperties& props)
{
	SVGCanvas::ImageRenderQuality quality = SVGCanvas::IMAGE_QUALITY_NORMAL;
	if (props.image_rendering == CSS_VALUE_optimizeSpeed ||
		props.image_rendering == CSS_VALUE__o_crisp_edges)
		quality = SVGCanvas::IMAGE_QUALITY_LOW;

	return quality;
}

#ifdef SVG_SUPPORT_FILTERS
// Map from CSSValue to SVGImageRendering enum value.
static SVGImageRendering SVGGetImageRenderingFromProps(const HTMLayoutProperties& props)
{
	switch (props.image_rendering)
	{
	case CSS_VALUE_optimizeSpeed:
	case CSS_VALUE__o_crisp_edges:
		return SVGIMAGERENDERING_OPTIMIZESPEED;

	case CSS_VALUE_optimizeQuality:
		return SVGIMAGERENDERING_OPTIMIZEQUALITY;

	default:
	case CSS_VALUE_auto:
		break;
	}
	return SVGIMAGERENDERING_AUTO;
}
#endif // SVG_SUPPORT_FILTERS

#ifdef SVG_SUPPORT_TEXTSELECTION
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"

static void GetSelectionColors(SVGDocumentContext* doc_ctx, const HTMLayoutProperties& props,
							   COLORREF& sel_bg_color, COLORREF& sel_fg_color)
{
	sel_bg_color = props.selection_bgcolor;
	sel_fg_color = props.selection_color;

	if (sel_fg_color == CSS_COLOR_transparent)
		sel_fg_color = props.font_color;

	// Don't use the system color for selection color if background color is set and vice versa.
	if (sel_fg_color == USE_DEFAULT_COLOR && sel_bg_color != USE_DEFAULT_COLOR)
		sel_fg_color = props.font_color;
	else if (sel_bg_color == USE_DEFAULT_COLOR && sel_fg_color != USE_DEFAULT_COLOR)
	{
		sel_bg_color = props.bg_color;

		if (sel_bg_color == USE_DEFAULT_COLOR)
			sel_bg_color = COLORREF(CSS_COLOR_transparent);
	}

	if (sel_bg_color == USE_DEFAULT_COLOR || sel_fg_color == USE_DEFAULT_COLOR)
	{
		const ServerName *sn =
			reinterpret_cast<const ServerName *>(doc_ctx->GetURL().GetAttribute(URL::KServerName, NULL));
		if (sel_bg_color == USE_DEFAULT_COLOR)
			sel_bg_color = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED, sn);
		if (sel_fg_color == USE_DEFAULT_COLOR)
			sel_fg_color = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_TEXT_SELECTED, sn);
	}
}
#endif // SVG_SUPPORT_TEXTSELECTION

static BOOL SetupUseOffset(HTML_Element* element, const SVGValueContext& vcxt, SVGNumberPair& use_translation)
{
	OP_ASSERT(element->Type() == Markup::SVGE_USE);

	SVGLengthObject* crdx = NULL;
	AttrValueStore::GetLength(element, Markup::SVGA_X, &crdx);

	SVGLengthObject* crdy = NULL;
	AttrValueStore::GetLength(element, Markup::SVGA_Y, &crdy);

	// "If the (x/y) attribute isn't specified, the effect is as if 0 was specified"
	if (crdx)
		use_translation.x = SVGUtils::ResolveLength(crdx->GetLength(), SVGLength::SVGLENGTH_X, vcxt);

	if (crdy)
		use_translation.y = SVGUtils::ResolveLength(crdy->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

	return use_translation.x.NotEqual(0) || use_translation.y.NotEqual(0);
}

#ifdef SVG_SUPPORT_STENCIL
class SVGContentClipper
{
public:
	SVGContentClipper() : m_canvas(NULL) {}
	~SVGContentClipper() { End(); }

	static BOOL CreateClip(SVGCanvas* canvas, const SVGRect& viewport,
						   const HTMLayoutProperties& props, BOOL always_create = FALSE);

	void Begin(SVGCanvas* canvas, const SVGRect& viewport, const HTMLayoutProperties& props, BOOL always_create = FALSE);
	void End();

protected:
	SVGCanvas* m_canvas;
};

/* static */
BOOL SVGContentClipper::CreateClip(SVGCanvas* canvas, const SVGRect& viewport,
								   const HTMLayoutProperties& props, BOOL always_create)
{
	if (props.overflow_x == CSS_VALUE_hidden ||
		props.overflow_x == CSS_VALUE_scroll)
	{
		SVGRect clip = viewport;
		OpStatus::Ignore(SVGUtils::AdjustCliprect(clip, props));
		if (always_create || !clip.IsEqual(viewport))
			return OpStatus::IsSuccess(canvas->AddClipRect(clip));
	}
	return FALSE;
}

void SVGContentClipper::End()
{
	if (m_canvas)
		m_canvas->RemoveClip();
	m_canvas = NULL;
}

void SVGContentClipper::Begin(SVGCanvas* canvas, const SVGRect& viewport,
							  const HTMLayoutProperties& props, BOOL always_create)
{
	if (CreateClip(canvas, viewport, props, always_create))
		m_canvas = canvas;
}
#endif // SVG_SUPPORT_STENCIL

static OP_STATUS ParseAndSetViewport(const uni_char* svg_view,
									 SVGAspectRatio& ratio, BOOL& has_hardcoded_ratio,
									 SVGRect& viewbox, BOOL& has_hardcoded_viewbox,
									 SVGVector& transform, BOOL& has_hardcoded_transform)
{
	OP_ASSERT(svg_view);
	unsigned strlen = uni_strlen(svg_view);

	if (uni_strncmp(svg_view, "svgView(", 8) != 0 ||
		svg_view[strlen-1] != ')')
	{
		// Broken svgView
		return OpSVGStatus::ATTRIBUTE_ERROR;
	}

	OP_ASSERT(has_hardcoded_ratio == FALSE);
	OP_ASSERT(has_hardcoded_viewbox == FALSE);
	OP_ASSERT(has_hardcoded_transform == FALSE);

	OpString working_copy;
	RETURN_IF_ERROR(working_copy.Set(svg_view+8, strlen-9)); // cut away svgView( and )
	if (working_copy.IsEmpty())
	{
		// Broken svgView, must have at least one attribute
		return OpSVGStatus::ATTRIBUTE_ERROR;
	}

	uni_char* current_attr = working_copy.CStr();
	while (*current_attr)
	{
		uni_char* delim = uni_strchr(current_attr, ';');
		unsigned current_attr_len = delim ? delim - current_attr - 1 : uni_strlen(current_attr);
		if (current_attr_len == 0 || current_attr[current_attr_len-1] != ')')
		{
			// Broken attribute
			return OpSVGStatus::ATTRIBUTE_ERROR;
		}
		if (uni_strncmp(current_attr, "viewBox(", 8) == 0)
		{
			uni_char* attr_val_start = current_attr + 8;
			SVGRectObject* temp_view_box;
			OP_STATUS status = SVGAttributeParser::ParseViewBox(attr_val_start,
				uni_strlen(attr_val_start)-1, &temp_view_box);
			if (OpStatus::IsError(status))
			{
				// Broken attribute
				return OpSVGStatus::ATTRIBUTE_ERROR;
			}

			viewbox.Set(temp_view_box->rect);
			has_hardcoded_viewbox = TRUE;
		}
		else if (uni_strncmp(current_attr, "preserveAspectRatio(", 20) == 0)
		{
			uni_char* attr_val_start = current_attr + 20;
			SVGAspectRatio* aspect_ratio;
			OP_STATUS status = SVGAttributeParser::ParsePreserveAspectRatio(attr_val_start,
				uni_strlen(attr_val_start)-1, aspect_ratio);
			if (OpStatus::IsError(status))
			{
				// Broken attribute
				return OpSVGStatus::ATTRIBUTE_ERROR;
			}

			ratio.Copy(*aspect_ratio);
			has_hardcoded_ratio = TRUE;
			OP_DELETE(aspect_ratio);
		}
		else if (uni_strncmp(current_attr, "transform(", 10) == 0)
		{
			uni_char* attr_val_start = current_attr + 10;
			OP_STATUS status = SVGAttributeParser::ParseVector(attr_val_start,
				uni_strlen(attr_val_start)-1, &transform);
			if (OpStatus::IsError(status))
			{
				// Broken attribute
				return OpSVGStatus::ATTRIBUTE_ERROR;
			}
			has_hardcoded_transform = TRUE;
		}
#ifdef _DEBUG
		else if (uni_strncmp(current_attr, "zoomAndPan(", 11) == 0)
		{
			// DEBUG ONLY CHECK
			uni_char* attr_val_start = current_attr + 11;
			SVGZoomAndPan zoom_and_pan;
			OP_STATUS status = SVGAttributeParser::ParseZoomAndPan(attr_val_start,
				uni_strlen(attr_val_start)-1, zoom_and_pan);
			// DEBUG ONLY CHECK
			if (OpStatus::IsError(status))
			{
				// Broken attribute
				return OpSVGStatus::ATTRIBUTE_ERROR;
			}

			OP_ASSERT(FALSE); // Propagate this outside!
		}
		else if (uni_strncmp(current_attr, "viewTarget", 10) == 0)
		{
			// DEBUG ONLY CHECK
			// Indicates the target object associated with the view.
			// If provided, then the target element(s) will be highlighted.
			OP_ASSERT(FALSE); // Not implemented
		}
		else
		{
			// DEBUG ONLY CHECK
			// Unknown attribute. Unspecified how to handle, but to be
			// somewhat future compatible we just silently ignore them
			OP_ASSERT(FALSE); // Check for missing code to handle this attribute
		}
#endif // _DEBUG

		current_attr += current_attr_len;
		if (delim)
		{
			current_attr++;
		}
	}
	return OpStatus::OK;
}

OP_STATUS SVGVisualTraversalObject::FillViewport(SVGElementInfo& info, const SVGRect& viewport)
{
	const HTMLayoutProperties& props = *info.props->GetProps();
	const SvgProperties *svg_props = props.svg;

	if (svg_props->viewportfill.GetColorType() == SVGColor::SVGCOLOR_NONE)
		return OpStatus::OK;

	RETURN_IF_ERROR(m_canvas->SaveState());

	m_canvas->EnableFill(SVGCanvasState::USE_COLOR);
	m_canvas->SetFillColor(svg_props->viewportfill.GetRGBColor());
	m_canvas->SetFillOpacity(svg_props->viewportfillopacity);
	m_canvas->EnableStroke(SVGCanvasState::USE_NONE);

	m_canvas->DrawRect(viewport.x, viewport.y, viewport.width, viewport.height, 0, 0);

	m_canvas->RestoreState();
	return OpStatus::OK;
}

static OP_BOOLEAN SynthesizeViewBox(HTML_Element *elm, const SVGValueContext &vcxt, SVGRect &rect)
{
	SVGLengthObject *wl = NULL, *hl = NULL;

	RETURN_IF_ERROR(AttrValueStore::GetLength(elm, Markup::SVGA_WIDTH, &wl, NULL));
	RETURN_IF_ERROR(AttrValueStore::GetLength(elm, Markup::SVGA_HEIGHT, &hl, NULL));

	if (wl && hl && wl->GetUnit() != CSS_PERCENTAGE && hl->GetUnit() != CSS_PERCENTAGE)
	{
		SVGNumber w = SVGUtils::ResolveLength(wl->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
		SVGNumber h = SVGUtils::ResolveLength(hl->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

		rect.Set(0, 0, w, h);
		return OpBoolean::IS_TRUE;
	}

	return OpBoolean::IS_FALSE;
}

/** Helps apply object-fit and object-position to SVG. Run FetchObjectFitPositionProps first to get the properties, then run ApplyObjectFitPosition afterward.
  */
struct SVGObjectFitPosition : ObjectFitPosition
{
	SVGObjectFitPosition()
	{
		fit = CSS_VALUE_none;
	}

	void Copy(const ObjectFitPosition& other) {
		fit = other.fit;
		x = other.x; y = other.y;
		x_percent = other.x_percent; y_percent = other.y_percent;
	}

	/** Find the object-fit and object-position properties. The method will first
	  look at the cascaded value for object-* on the embedding HTML element, then
	  the cascade (for example, an SVG element not directly contained in HTML),
	  then give up and use default values.

	  @param cascade		the current cascade (may be NULL).
	  @param layoutbox		the current layout box (may be NULL).
	*/
	void FetchObjectFitPositionProps(LayoutProperties *cascade, Box *layoutbox)
	{
		if (layoutbox && layoutbox->GetSVGContent() && layoutbox->GetSVGContent()->GetObjectFitPosition().fit != CSS_VALUE_UNSPECIFIED)
		{
			// This is in HTML and the value of object-fit has been stored.
			SVGContent *svg_content = layoutbox->GetSVGContent();
			Copy(svg_content->GetObjectFitPosition());
		}
		else if (cascade)
		{
			// We can retrieve values from the cascade.
			const HTMLayoutProperties& props = *cascade->GetProps();
			Copy(props.object_fit_position);
		}
	}

	/** Modify the viewport based on object-fit and object-position properties.

	 @param viewbox					the viewbox.
	 @param viewport[inout]			the viewport to modify.
	 @param ratio[out]				the aspect ratio to modify (only for
									'object-fit: fill').
	 */
	void ApplyObjectFitPosition(SVGRect *viewbox, SVGRect& viewport, SVGAspectRatio *ratio)
	{
		OP_ASSERT(ratio);
		// If 'object-fit: auto', let 'preserveAspectRatio' take effect. Otherwise, let object-.* take effect.
		if (fit != CSS_VALUE_auto)
		{
			if (viewbox)
			{
				SVGRect orig_viewport = viewport;
				CalculateObjectFit(*viewbox, orig_viewport, viewport);
				CalculateObjectPosition(orig_viewport, viewport);
			}

			// 'fill' will override the aspect ratio
			if (fit == CSS_VALUE_fill)
			{
				ratio->align = SVGALIGN_NONE;
				ratio->mos = SVGMOS_MEET;
			}
		}
	}

	/** Resolve percentages to find out where the top and left edges of replaced content go.
	 *
	 *	\warning This code is duplicated in ObjectFitPosition::CalculateObjectPosition (without SVGRects). Any modifications must be consistent.
	 *
	 * @param pos_rect					the rectangle within which to position the image (such as the original viewport).
	 * @param[inout] content			in: the provided dimensions of the content; out: the calculated position of the content.
	 */

	// WARNING: This code is duplicated in layout/layoutprops.h:ObjectFitPosition::CalculateObjectPosition (without SVGRects). Any modifications must be consistent.
	void CalculateObjectPosition(const SVGRect& pos_rect, SVGRect& content)
	{
		// Calculate initial position of content
		content.x = pos_rect.x;
		content.y = pos_rect.y;

		if (x_percent)
			content.x += ((pos_rect.width - content.width) * LayoutFixedToSVGNumber(LayoutFixed(x))) / 100;
		else
			content.x += SVGNumber(x);

		if (y_percent)
			content.y += ((pos_rect.height - content.height) * LayoutFixedToSVGNumber(LayoutFixed(y))) / 100;
		else
			content.y += SVGNumber(y);
	}

	/** Calculate height and width of the painted replaced content (that is,
	 *	not the box, but the actual content). Used with property object-fit.
	 *	Uses the viewbox as the content's intrinsic size.
	 *
	 * \warning This code is duplicated in ObjectFitPosition::CalculateObjectFit
	 *			(without SVGNumbers). Any modifications must be consistent.
	 *			The parameter <code>fit_fallback</code> was removed from the
	 *			SVG version, since 'auto' means "backward-compatible".
	 *
	 * @param intrinsic			an SVG viewBox.
	 * @param inner				size of viewport within which to position the content.
	 * @param[out] dst			resulting size of the painted content.
	 */

	// WARNING: This code is duplicated in layout/layoutprops.h:ObjectFitPosition::CalculateObjectFit
	//			(without SVGNumbers). Any modifications must be consistent.
	void
	CalculateObjectFit(const SVGRect& intrinsic, const SVGRect& inner, SVGRect& dst)
	{
		short object_fit = fit;

		// Fall back to "fill" if an intrinsic dimension is missing.
		if (intrinsic.width == 0 || intrinsic.height == 0)
			object_fit = CSS_VALUE_fill;

		if (object_fit == CSS_VALUE_contain || object_fit == CSS_VALUE_cover)
		{
			// This is a multiplication version of inner_width/inner_height > img.Width()/img.Height(), avoiding floating point
			BOOL inner_is_wider = inner.width*intrinsic.height > intrinsic.width*inner.height;

			if (object_fit == CSS_VALUE_contain && inner_is_wider
				|| object_fit == CSS_VALUE_cover && !inner_is_wider )
			{
				// The content box has a higher width-to-height ratio than the image (i.e. if they were the same height, the content box would be wider).
				// Pillarboxing (if contain): Match image height to content box and scale width
				dst.width = inner.height*intrinsic.width/intrinsic.height;
				dst.height = inner.height;
			}
			else
			{
				// The content box has a lower width-to-height ratio than the image (i.e. if they were the same width, the content box would be taller).
				// Letterboxing (if contain): Match image width to content box and scale height
				dst.width = inner.width;
				dst.height = inner.width*intrinsic.height/intrinsic.width;
			} // If the sides are equal, the latter case works fine: we don't need a special one.
		}
		else if (object_fit == CSS_VALUE_none)
		{
			dst.width = intrinsic.width;
			dst.height = intrinsic.height;
		}
		else
		{
			// CSS_VALUE_fill is initial, traditional value
			// Match image height and width to content box
			dst.height = inner.height;
			dst.width = inner.width;
		}
	}
};

static HTML_Element* GetReferencingUseElement(HTML_Element* ref_target_traversed)
{
	// Find the referencing <use> element
	if (HTML_Element* use_elm = ref_target_traversed->Parent())
		if (HTML_Element* layouted_use_elm = SVGUtils::GetElementToLayout(use_elm))
			if (layouted_use_elm->IsMatchingType(Markup::SVGE_USE, NS_SVG))
				return layouted_use_elm;

	return NULL;
}

/** Calculate the viewport dimensions and viewbox transform to apply when establishing a new viewport. object-fit and object-position are applied here.
 *	@param vpinfo[out]	The ViewportInfo object to fill. When OpStatus::OK is returned, this object is correctly filled.
 */

/* static */
OP_STATUS SVGTraversalObject::CalculateSVGViewport(SVGDocumentContext* doc_ctx, SVGElementInfo& info,
												   const SVGValueContext& vcxt,
												   SVGMatrix& root_transform, ViewportInfo& vpinfo)
{
	BOOL is_topmost_svg_root = (info.layouted == doc_ctx->GetSVGRoot());
	BOOL is_standalone = (is_topmost_svg_root &&
						  doc_ctx->GetSVGRoot()->Parent() &&
						  doc_ctx->GetSVGRoot()->Parent()->Type() == HE_DOC_ROOT);

	// If this is a SVG document (as opposed to an SVG element inline in a
	// document), we can have extra view specifications in the rel part
	// of the URL to the SVG.
	// These should apply to the svgroot
	HTML_Element* rel_element = NULL;
	HTML_Element* view_element = NULL;
	HTML_Element* fallback_view_element = info.layouted;
	BOOL has_svg_view = FALSE;
	if (is_standalone)
	{
		// Topmost SVG, look if the viewport is hardcoded in the url
		const uni_char* rel = doc_ctx->GetURL().UniRelName();
		if (rel)
		{
			rel_element = SVGUtils::FindDocumentRelNode(doc_ctx->GetLogicalDocument(), rel);
			view_element = rel_element;
			if (!view_element)
			{
				// Try to parse it as a viewport specification
				if (uni_strncmp(rel, "svgView(", 8) == 0)
				{
					has_svg_view = TRUE;
					// Handled after we've read the normal values from the svg element
				}
			}
			else
			{
				// The closest SVG element
				fallback_view_element = SVGUtils::GetRootSVGElement(view_element);
				if (!view_element->IsMatchingType(Markup::SVGE_VIEW, NS_SVG))
				{
					view_element = NULL;
				}
			}
		}
	}

	vpinfo.overflow = CSS_VALUE_hidden;
	if (info.props)
	{
		const HTMLayoutProperties& props = *info.props->GetProps();
		vpinfo.overflow = props.overflow_x;
	}

	SVGRect *viewbox = NULL;
	SVGAspectRatio* ratio = NULL;
	SVGRectObject* viewbox_rect_obj = NULL;

	// First fetch values from the fallback element, then override with the view element if any
	for (HTML_Element* current_view_element = fallback_view_element;
		 current_view_element;
		 current_view_element = (current_view_element == view_element ? NULL : view_element))
	{
		if (doc_ctx->IsExternal() && doc_ctx->GetReferencingElement())
		{
			AttrValueStore::GetPreserveAspectRatio(doc_ctx->GetReferencingElement(), ratio);
		}
		else
		{
			AttrValueStore::GetPreserveAspectRatio(current_view_element, ratio);
		}

		if (AttrValueStore::HasObject(current_view_element, Markup::SVGA_OVERFLOW, NS_IDX_SVG))
		{
			SVGEnum* obj;
			if (OpStatus::IsSuccess(AttrValueStore::GetEnumObject(current_view_element, Markup::SVGA_OVERFLOW, SVGENUM_OVERFLOW, &obj)) &&
				obj && obj->Flag(SVGOBJECTFLAG_IS_CSSPROP))
			{
				vpinfo.overflow = obj->EnumValue();
			}
		}

		if (AttrValueStore::HasObject(current_view_element, Markup::SVGA_VIEWBOX, NS_IDX_SVG))
		{
			// We ignore invalid viewBoxes
			if (OpStatus::IsSuccess(AttrValueStore::GetViewBox(current_view_element, &viewbox_rect_obj)) &&
				viewbox_rect_obj)
			{
				viewbox = &viewbox_rect_obj->rect;
			}
		}
	}

	SVGRect viewbox_obj;
	SVGAspectRatio ratio_obj;
	SVGMatrix view_transform;
	if (has_svg_view)
	{
		const uni_char* rel = doc_ctx->GetURL().UniRelName();
		SVGVector transform(SVGOBJECT_TRANSFORM);
		transform.SetSeparator(SVGVECTORSEPARATOR_COMMA_OR_WSP);
		BOOL has_hardcoded_viewbox = FALSE;
		BOOL has_hardcoded_ratio = FALSE;
		BOOL has_hardcoded_transform = FALSE;
		RETURN_IF_MEMORY_ERROR(ParseAndSetViewport(rel,
												   ratio_obj, has_hardcoded_ratio,
												   viewbox_obj, has_hardcoded_viewbox,
												   transform, has_hardcoded_transform));
		if (has_hardcoded_viewbox)
		{
			viewbox = &viewbox_obj;
		}

		if (has_hardcoded_ratio)
		{
			ratio = &ratio_obj;
		}

		if (has_hardcoded_transform)
		{
			transform.GetMatrix(view_transform);
		}
	}

	// Only the topmost svgroot and foreignObjects can have a layoutbox,
	// also note that display=none means no layoutbox for that element.
	Box* layoutbox = info.layouted->GetLayoutBox();

	// fitter handles object-fit and object-position
	SVGObjectFitPosition fitter;
	fitter.FetchObjectFitPositionProps(info.props, layoutbox);

	// To maintain backward compatibility, we do not synthesize viewboxes in <object> and <svg> when 'object-fit: auto'
	if (is_topmost_svg_root && viewbox == NULL && (doc_ctx->GetSVGImage()->IsInImgElement() || fitter.fit != CSS_VALUE_auto))
	{
		/* For svg in html lacking viewBox but with width and
		   height, we synthesize a viewBox corresponding to the width and
		   height to make the image scalable. This makes sense because
		   images should never have scrollbar/be scrollable, so the
		   alternative is to just clip outside content. */

		OP_BOOLEAN has_synthesized_viewbox = SynthesizeViewBox(info.layouted, vcxt, viewbox_obj);
		RETURN_IF_ERROR(has_synthesized_viewbox);

		if (has_synthesized_viewbox == OpBoolean::IS_TRUE)
			viewbox = &viewbox_obj;
	}

	// Note that x and y should have no meaning on the outermost svg element

	if (is_topmost_svg_root && layoutbox)
	{
		// This branch should be executed for the topmost svgroot
		// only, it's for ignoring the x and y attributes.

		if (doc_ctx->HasForcedSize())
		{
			vpinfo.viewport.width = doc_ctx->ForcedWidth();
			vpinfo.viewport.height = doc_ctx->ForcedHeight();
		}
		else
		{
			SVGLengthObject *wl = NULL;
			SVGLengthObject *hl = NULL;

#ifdef SVG_SUPPORT_EXTERNAL_USE
			if (info.traversed != info.layouted)
			{
				// We're inside a shadowtree
				// Get the viewport from the <use> element
				if (HTML_Element* ref_use_elm = GetReferencingUseElement(info.traversed))
				{
					AttrValueStore::GetLength(ref_use_elm, Markup::SVGA_WIDTH, &wl);
					AttrValueStore::GetLength(ref_use_elm, Markup::SVGA_HEIGHT, &hl);
				}
			}
#endif // SVG_SUPPORT_EXTERNAL_USE
			// Get border and padding, if possible
			int border_padding_x = 0, border_padding_y = 0;
			if ((!wl || !hl) && info.props)
			{
				const HTMLayoutProperties& cascade = *info.props->GetProps();
				border_padding_x = cascade.GetNonPercentHorizontalBorderPadding();
				border_padding_y = cascade.GetNonPercentVerticalBorderPadding();
			}

			// Get the viewport from the <use> element if the <svg> element is being <use>d,
			// otherwise from the SVG element.
			if (wl)
				vpinfo.viewport.width = SVGUtils::ResolveLength(wl->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
			else
				vpinfo.viewport.width = layoutbox->GetWidth() - border_padding_x;

			if (hl)
				vpinfo.viewport.height = SVGUtils::ResolveLength(hl->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);
			else
				vpinfo.viewport.height = layoutbox->GetHeight() - border_padding_y;
		}
	}
	else
	{
		SVGLengthObject *wl = NULL;
		SVGLengthObject *hl = NULL;

		if (info.traversed != info.layouted)
		{
			// We're inside a shadowtree

			// Get the viewport parameters from the <use> element
			if (HTML_Element* ref_use_elm = GetReferencingUseElement(info.traversed))
			{
				AttrValueStore::GetLength(ref_use_elm, Markup::SVGA_WIDTH, &wl);
				AttrValueStore::GetLength(ref_use_elm, Markup::SVGA_HEIGHT, &hl);
			}
		}

		SVGLengthObject def_w(SVGNumber(100), CSS_PERCENTAGE);
		SVGLengthObject def_h(SVGNumber(100), CSS_PERCENTAGE);

		HTML_Element* elm = info.layouted;

		if (!wl)
			AttrValueStore::GetLength(elm, Markup::SVGA_WIDTH, &wl, &def_w);
		if (!hl)
			AttrValueStore::GetLength(elm, Markup::SVGA_HEIGHT, &hl, &def_h);

		if (wl)
			vpinfo.viewport.width = SVGUtils::ResolveLength(wl->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
		if (hl)
			vpinfo.viewport.height = SVGUtils::ResolveLength(hl->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

		SVGLengthObject *xc = NULL;
		SVGLengthObject *yc = NULL;
		RETURN_IF_ERROR(AttrValueStore::GetLength(elm, Markup::SVGA_X, &xc));
		RETURN_IF_ERROR(AttrValueStore::GetLength(elm, Markup::SVGA_Y, &yc));

		if (xc)
			vpinfo.viewport.x = SVGUtils::ResolveLength(xc->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
		if (yc)
			vpinfo.viewport.y = SVGUtils::ResolveLength(yc->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

	}

	//
	// Order of application of relevant matrices:
	//
	// M(current{Scale,Translate,...}) * M(viewbox) * M(user)
	//
	// Notes:
	// The above transformations are applied from right to left
	// Viewport clipping should be applied after vd_scale to remain unaffected by pan/zoom

	if (doc_ctx->GetForcedViewbox())
	{
		viewbox = doc_ctx->GetForcedViewbox();
		ratio = NULL; // ignore pAR when viewbox is forced
	}

	// Modify the viewport according to object-fit and -position.
	SVGAspectRatio new_ratio;
	if (ratio)
		new_ratio.Copy(*ratio);
	fitter.ApplyObjectFitPosition(viewbox, vpinfo.viewport, &new_ratio); // Does nothing if 'object-fit: auto'
	// Let ApplyObjectFitPosition override the aspect ratio
	ratio = &new_ratio;

	OP_STATUS result = SVGUtils::GetViewboxTransform(vpinfo.viewport, viewbox, ratio,
													 vpinfo.transform, vpinfo.actual_viewport);

	// vpinfo.transform contains the viewBox transform here and nothing else
	root_transform.Copy(vpinfo.transform);

#if defined(SVG_DOM) || defined(SVG_SUPPORT_PANNING)
	if (layoutbox)
	{
		// Apply currentScale and currentTranslate (also used in panning)
		// before viewBox but after viewport scaling so that we are in
		// user-space coordinates.
		doc_ctx->GetCurrentMatrix(vpinfo.user_transform);
	}
#endif // SVG_DOM || SVG_SUPPORT_PANNING

	if (has_svg_view)
		vpinfo.transform.PostMultiply(view_transform);

	vpinfo.slice = ratio && ratio->mos == SVGMOS_SLICE;

	return result;
}

#ifdef SVG_SUPPORT_STENCIL
void SVGVisualTraversalObject::ClipViewport(SVGElementInfo& info, const ViewportInfo& vpinfo)
{
	if (!m_doc_ctx->IsExternal() &&
		(vpinfo.overflow == CSS_VALUE_hidden ||
		 vpinfo.overflow == CSS_VALUE_scroll ||
		 vpinfo.slice))
	{
		// Clip to viewport before panning and zooming has been
		// applied. Otherwise the clip_rect coordinates will be affected.
		// Clip region is affected by the external scale
		SVGRect clip_rect = vpinfo.viewport;
		if (info.props)
		{
			const HTMLayoutProperties& props = *info.props->GetProps();
			OpStatus::Ignore(SVGUtils::AdjustCliprect(clip_rect, props));
		}

		SVGRect xfrmclip = m_canvas->GetTransform().ApplyToRect(clip_rect);
		OpRect pix_clip_rect = xfrmclip.GetSimilarRect();
		if (!m_canvas->IsContainedBy(pix_clip_rect))
			if (OpStatus::IsSuccess(m_canvas->AddClipRect(clip_rect)))
				info.Set(RESOURCE_VIEWPORT_CLIP);
	}
}

void SVGLayoutObject::ClipViewport(SVGElementInfo& info, const ViewportInfo& vpinfo)
{
	SVGViewportCompositePaintNode* vpc_pn = static_cast<SVGViewportCompositePaintNode*>(info.paint_node);

	if (!m_doc_ctx->IsExternal() &&
		(vpinfo.overflow == CSS_VALUE_hidden ||
		 vpinfo.overflow == CSS_VALUE_scroll ||
		 vpinfo.slice))
	{
		// Clip to viewport before panning and zooming has been
		// applied. Otherwise the clip_rect coordinates will be affected.
		// Clip region is affected by the external scale
		SVGRect clip_rect = vpinfo.viewport;
		if (info.props)
		{
			const HTMLayoutProperties& props = *info.props->GetProps();
			OpStatus::Ignore(SVGUtils::AdjustCliprect(clip_rect, props));
		}

		vpc_pn->SetClipRect(clip_rect);
		vpc_pn->UseClipRect(TRUE);

		if (OpStatus::IsSuccess(m_canvas->AddClipRect(clip_rect)))
			info.Set(RESOURCE_VIEWPORT_CLIP);
	}
	else
	{
		vpc_pn->UseClipRect(FALSE);
	}
}
#endif // SVG_SUPPORT_STENCIL

/** Helper for calculation of (element) bounding boxes */
OP_STATUS GetElementBBox(SVGDocumentContext* doc_ctx, HTML_Element* traversed_elm,
						 const SVGNumberPair& initial_viewport, SVGBoundingBox& bbox)
{
	BOOL do_traverse = TRUE;

	SVGElementContext* elem_ctx = AttrValueStore::GetSVGElementContext(traversed_elm);

	if (elem_ctx && elem_ctx->IsBBoxValid())
	{
		bbox = elem_ctx->GetBBox();
		do_traverse = FALSE;
	}

	OP_STATUS status = OpStatus::OK;
	if (do_traverse)
	{
		HTML_Element* traverseroot = traversed_elm;

		// Make sure the tree state is updated if needed.
		SVGElementInvalidState initial_invalid_state = SVGElementContext::ComputeInvalidState(traversed_elm);

		// Traverse the entire text subtree if pointed to a text-'subelement' (tspan, et.c)
		HTML_Element* layouted_elm = SVGUtils::GetElementToLayout(traversed_elm);
		Markup::Type type = layouted_elm->Type();
		if (SVGUtils::IsTextChildType(type))
		{
			while (traverseroot)
			{
				if (SVGUtils::IsTextRootElement(layouted_elm))
					break;

				traverseroot = traverseroot->Parent();
				layouted_elm = SVGUtils::GetElementToLayout(traverseroot);
			}

			if (!traverseroot)
				return status;
		}

		// Get boundingbox (requires traversing all children of the target element)
		SVGNullCanvas dummy_canvas;
		dummy_canvas.SetDefaults(doc_ctx->GetRenderingQuality());

		SVGLogicalTreeChildIterator ltci;
		SVGBBoxUpdateObject bbu_object(&ltci);
		RETURN_IF_ERROR(bbu_object.SetupResolver());
		bbu_object.SetDocumentContext(doc_ctx);
		bbu_object.SetCurrentViewport(initial_viewport);
		bbu_object.SetCanvas(&dummy_canvas);
		bbu_object.SetInitialInvalidState(initial_invalid_state);

		status = SVGTraverser::Traverse(&bbu_object, traverseroot, NULL);

		// If we didn't have a context before, let's try again. It should be there now.
		if (!elem_ctx)
			elem_ctx = AttrValueStore::GetSVGElementContext(traversed_elm);

		if (elem_ctx && elem_ctx->IsBBoxValid())
		{
			bbox = elem_ctx->GetBBox();
		}
	}

	return status;
}

SVGLayoutObject::~SVGLayoutObject()
{
	CleanContainerStack();
	m_pending_nodes.Clear();
}

/**
 * TODO:
 *  + Handle recursive traverses. (stack of stacks?)
 */

/**
 * These are the states in a traversal of a single
 * element. The order is essential.
 */
enum TraverseElementProgress
{
	ENTER_ELEMENT,	// -> PUSH_CHILD, LEAVE_ELEMENT, POP_ELEMENT
	PUSH_CHILD,		// -> NEXT_CHILD
	NEXT_CHILD,		// -> PUSH_CHILD, LEAVE_ELEMENT
	LEAVE_ELEMENT,	// -> POP_ELEMENT
	POP_ELEMENT
};

struct TraverseStackState
{
	TraverseStackState() {} // Called by the array constructor - don't put logic here. It will only be called once but reused many times with only Init() called in between

	void Init(SVGElementContext* ctx, TraverseStackState* parent_stack_frame)
	{
		// Most of it will be set by someone else later on
		progress = ENTER_ELEMENT;
		parent_frame = parent_stack_frame;
		parent_cascade = NULL;
		child_cascade = NULL;

		layout_info = parent_stack_frame ? parent_stack_frame->layout_info : NULL;
		allocated_layout_info = NULL;

		element_info.context = ctx;
		element_info.paint_node = NULL;
		element_info.traversed = ctx->GetHtmlElement();
		element_info.layouted = ctx->GetLayoutedElement();
		element_info.state = ctx->GetInvalidState();
		element_info.has_transform = 0;
		element_info.has_text_attrs = 0;
		element_info.has_canvasstate = 0;
		element_info.has_container_state = 0;
		element_info.has_resource = 0;

		element_info.css_reload = ctx->IsCSSReloadNeeded() || element_info.layouted->IsPropsDirty();

		if (parent_stack_frame)
		{
			element_info.state = MAX(element_info.state, parent_stack_frame->element_info.state);

			element_info.css_reload |= parent_stack_frame->element_info.css_reload;
		}
	}

	void SetChildCascade(LayoutProperties* cascade) { child_cascade = cascade; }

	SVGElementInfo			element_info;
	/** The variable deciding which state to process in the state
	 * machine when next processing this frame. */
	TraverseElementProgress	progress; // All states. Set before.
	/** Current status. */
	OP_STATUS				result;
	/** Currently traversed child, if traversing children. */
	SVGElementContext*		child_context;
	/** The LayoutInfo structure for the current frame - changes when
	 * crossing document boundaries. */
	LayoutInfo*				layout_info;

private:
	/** Used to look into parent stack frames (for instance to build a
	 * correct cascade lazily if it has not been built yet. NULL for
	 * the root stack frame. */
	TraverseStackState*		parent_frame;
	LayoutProperties*		parent_cascade; // Set in ENTER_ELEMENT, cleared in POP_ELEMENT
	LayoutProperties*		child_cascade; // Set in ENTER_ELEMENT, cleared in POP_ELEMENT
	/** If we entered an external subtree, we will allocate a new
	 * LayoutInfo for that document. This pointer keeps track of that
	 * LayoutInfo so we can free it. */
	LayoutInfo*				allocated_layout_info;

public:
	OP_STATUS SetupProps()
	{
		if (element_info.GetCSSReload())
 		{
			RETURN_IF_ERROR(layout_info->workplace->UnsafeLoadProperties(element_info.layouted));
			if (element_info.context)
				element_info.context->ClearNeedCSSReloadFlag();
		}

		if (!child_cascade)
		{
			if (!parent_frame)
			{
				if (!element_info.props)
				{
					// Here we will crash. Or maybe not, but it isn't
					// right. The one initiating the traverse should
					// have supplied a root props.
					OP_ASSERT(!"Missing root properties");
					return OpStatus::ERR;
				}
				// This is the props for the (traversal) root element
				child_cascade = element_info.props;
			}
			else
			{
				if (!parent_frame->child_cascade)
				{
					RETURN_IF_ERROR(parent_frame->SetupProps());
					OP_ASSERT(parent_frame->child_cascade);
					OP_ASSERT(parent_frame->element_info.props);
					OP_ASSERT(parent_frame->element_info.props == parent_frame->child_cascade);
				}
				parent_cascade = parent_frame->child_cascade;

				// OP_ASSERT(parent_cascade->html_element != element_info.layouted);
				child_cascade = parent_cascade->GetChildCascade(*layout_info, element_info.layouted);

				if (!child_cascade || !child_cascade->GetProps()->svg)
					return OpStatus::ERR_NO_MEMORY;
			}
		}

		element_info.props = child_cascade;
		return OpStatus::OK;
	}

#ifdef SVG_SUPPORT_EXTERNAL_USE
	OP_STATUS SetupLayoutInfoForShadowTree();
#endif // SVG_SUPPORT_EXTERNAL_USE

	OP_STATUS BeforeEnter(SVGTraversalObject* traversal_object)
	{
		OP_ASSERT(element_info.layouted->GetNsType() == NS_SVG || element_info.layouted->IsText());

#ifdef SVG_SUPPORT_EXTERNAL_USE
		if (element_info.traversed->IsMatchingType(Markup::SVGE_ANIMATED_SHADOWROOT, NS_SVG))
			RETURN_IF_ERROR(SetupLayoutInfoForShadowTree());
#endif // SVG_SUPPORT_EXTERNAL_USE

		RETURN_IF_ERROR(SetupProps()); // Compute CSS

		return traversal_object->PrepareElement(element_info);
	}

	void FreeProps(BOOL silent = FALSE)
	{
		OP_DELETE(allocated_layout_info);
		allocated_layout_info = NULL;
		layout_info = NULL;

		if (parent_cascade) // else it is a caller supplied css chunk
		{
			if (child_cascade)
			{
				// This was a CSS chunk we created. If child_cascade
				// is != NULL and parent_cascade == NULL, then this is
				// the supplied CSS and up to the caller to clean
				parent_cascade->CleanSuc();
			}

			parent_cascade = NULL;
		}

		element_info.props = NULL;
		child_cascade = NULL;
	}

#ifdef SVG_INTERRUPTABLE_RENDERING
    void Free()
    {
        FreeProps(TRUE);
    }
#endif // SVG_INTERRUPTABLE_RENDERING

private:
	TraverseStackState(const TraverseStackState& other); /* never copy */
	TraverseStackState& operator=(const TraverseStackState& rhs); /* never assign */
};

#ifdef SVG_SUPPORT_EXTERNAL_USE
OP_STATUS TraverseStackState::SetupLayoutInfoForShadowTree()
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(element_info.layouted);
	if (!doc_ctx)
		return OpStatus::ERR;

	allocated_layout_info = OP_NEW(LayoutInfo, (doc_ctx->GetLayoutWorkplace()));
	if (!allocated_layout_info)
		return OpStatus::ERR_NO_MEMORY;

	layout_info = allocated_layout_info;
	return OpStatus::OK;
}
#endif // SVG_SUPPORT_EXTERNAL_USE

class TraverseStack
{
	// A good value is probably somewhere 20-80. They are right now about 100 bytes each.
#define TRAVERSE_STACK_STATE_ARRAY_SIZE 20
private:
	class TraverseStackStatePool
	{
	public:
		TraverseStackStatePool()
			: m_next_state_array(0), m_next_state_array_index(0) {}

		~TraverseStackStatePool()
		{
			for (UINT32 i = 0; i < m_state_arrays.GetCount(); i++)
			{
				OP_DELETEA(m_state_arrays.Get(i));
			}
		}

		/**
		 * Returns a TraverseStackState or NULL if OOM. Still owned by the pool
		 * so it mustn't be used after the
		 * deletion of the pool.
		 */
		TraverseStackState* GetNew()
		{
			TraverseStackState* array;
			if (m_next_state_array == m_state_arrays.GetCount())
			{
				// New array
				array = AllocateStackBlock();
				if (!array)
					return NULL;
			}
			else
			{
				OP_ASSERT(m_next_state_array < m_state_arrays.GetCount());
				array = m_state_arrays.Get(m_next_state_array);
			}
			OP_ASSERT(array);
			OP_ASSERT(m_next_state_array_index < TRAVERSE_STACK_STATE_ARRAY_SIZE);
			TraverseStackState* return_state = array + m_next_state_array_index;
			m_next_state_array_index++;
			if (m_next_state_array_index == TRAVERSE_STACK_STATE_ARRAY_SIZE)
			{
				m_next_state_array_index = 0;
				m_next_state_array++;
			}
			return return_state;
		}

		/**
		 * Release state. Must be called in the opposite order from GetNew.
		 */
		void ReleaseStackState(TraverseStackState* state)
		{
			OP_ASSERT(state);
			if (m_next_state_array_index == 0)
			{
				OP_ASSERT(m_next_state_array > 0);
				m_next_state_array_index = TRAVERSE_STACK_STATE_ARRAY_SIZE;
				m_next_state_array--;
			}
			m_next_state_array_index--;
#ifdef _DEBUG
			TraverseStackState* debug_array = m_state_arrays.Get(m_next_state_array);
			OP_ASSERT(&debug_array[m_next_state_array_index] == state); // Can only release the one on top of the stack. Not too hard to change but no need anyway right now.
#endif // _DEBUG
		}

	private:
		TraverseStackState* AllocateStackBlock();

		// Cant use the template since that uses the delete operator and we need to use the delete[]
		// operator.
		class TraverseStackStateVector : private OpGenericVector
		{
		public:
			TraverseStackStateVector() : OpGenericVector(30) {}
			UINT32 GetCount() { return OpGenericVector::GetCount(); }
			TraverseStackState* Get(UINT32 idx)
			{
				return static_cast<TraverseStackState*>(OpGenericVector::Get(idx));
			}
			OP_STATUS Add(TraverseStackState* item)
			{
				// We want to do the normal Add but Insert at the end is
				// faster (lower overhead)
				return OpGenericVector::Insert(OpGenericVector::GetCount(),
											   item);
			}
		};

		TraverseStackStateVector m_state_arrays;
		UINT32 m_next_state_array;
		UINT32 m_next_state_array_index;
	};

	class TraverseStackStateStack
	{
	public:
		TraverseStackStateStack() : m_states(NULL), m_count(0), m_size(0) {}
		~TraverseStackStateStack() { OP_DELETEA(m_states); }

		BOOL IsEmpty() const { return m_count == 0; }

		TraverseStackState* Pop() { OP_ASSERT(m_count > 0); return m_states[--m_count]; }
		TraverseStackState* Top() const { OP_ASSERT(m_count > 0); return m_states[m_count - 1]; }

		OP_STATUS Push(TraverseStackState* state)
		{
			if (m_count == m_size)
				RETURN_IF_ERROR(Grow());

			m_states[m_count++] = state;
			return OpStatus::OK;
		}

		TraverseStackState* Get(unsigned idx) const { OP_ASSERT(idx < m_count); return m_states[idx]; }
		unsigned GetCount() const { return m_count; }

	private:
		enum { INITIAL_SIZE = 32 };

		OP_STATUS Grow();

		TraverseStackState** m_states;
		unsigned m_count;
		unsigned m_size;
	};

	TraverseStack(const TraverseStack& other);
	TraverseStack& operator=(const TraverseStack& other);
	TraverseStackStateStack m_stack;
	TraverseStackStatePool m_state_pool;

public:

	TraverseStack() {}

	~TraverseStack()
	{
#ifdef SVG_INTERRUPTABLE_RENDERING
        while (!IsEmpty())
			Pop(TRUE);
#endif // SVG_INTERRUPTABLE_RENDERING

		OP_ASSERT(IsEmpty());
	}

	/**
	 * Returns TRUE if the stack is empty. FALSE otherwise.
	 */
	BOOL IsEmpty() { return m_stack.IsEmpty(); }

	TraverseStackState* Get(unsigned index) { return m_stack.Get(index); }

	unsigned GetCount() const { return m_stack.GetCount(); }

	/**
	 * Returns a reference to the element on the top of
	 * the stack or crashes if the stack is
	 * empty. *hint: use the IsEmpty method.
	 */
	TraverseStackState* Top()
	{
		OP_ASSERT(m_stack.GetCount() > 0);
		return m_stack.Top();
	}

	/**
	 * Returns the m_result of the topmost frame and the deletes that frame.
	 */
	OP_STATUS Pop(BOOL is_cancelled_traverse = FALSE)
	{
		OP_ASSERT(m_stack.GetCount() > 0);
		TraverseStackState* top_of_stack = m_stack.Pop();

#ifdef SVG_INTERRUPTABLE_RENDERING
        if (is_cancelled_traverse)
        {
            top_of_stack->Free();
        }
#endif // SVG_INTERRUPTABLE_RENDERING

		OP_STATUS res = top_of_stack->result;
		m_state_pool.ReleaseStackState(top_of_stack);
		return res;
	}

	/**
	 * Returns OpStatus::ERR_NO_MEMORY if we went OOM.
	 *
	 * @param parent The parent stack frame or NULL if this is the
	 * root element.
	 */
	OP_STATUS PushNewFrame(SVGElementContext* context, TraverseStackState* parent)
	{
		TraverseStackState* new_state = m_state_pool.GetNew();
		if (!new_state)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS status = m_stack.Push(new_state);
		if (OpStatus::IsError(status))
		{
			m_state_pool.ReleaseStackState(new_state);
			return status;
		}

		new_state->Init(context, parent);
		return OpStatus::OK;
	}
};

TraverseStackState* TraverseStack::TraverseStackStatePool::AllocateStackBlock()
{
	TraverseStackState* array = OP_NEWA(TraverseStackState, TRAVERSE_STACK_STATE_ARRAY_SIZE);
	if (!array || OpStatus::IsError(m_state_arrays.Add(array)))
	{
		OP_DELETEA(array);
		array = NULL;
	}
	return array;
}

OP_STATUS TraverseStack::TraverseStackStateStack::Grow()
{
	unsigned new_size = MAX((unsigned)INITIAL_SIZE, 2 * m_size);
	TraverseStackState** new_states = OP_NEWA(TraverseStackState*, new_size);
	if (!new_states)
		return OpStatus::ERR_NO_MEMORY;

	if (m_states)
	{
		op_memcpy(new_states, m_states, m_count * sizeof(*m_states));

		OP_DELETEA(m_states);
	}

	m_states = new_states;
	m_size = new_size;
	return OpStatus::OK;
}

#ifdef SVG_INTERRUPTABLE_RENDERING
void SVGTraversalState::UnhookStackProps()
{
	if (m_traverse_stack->IsEmpty())
		return;

	for (UINT32 i = m_traverse_stack->GetCount(); i > 0; i--)
		if (TraverseStackState* state = m_traverse_stack->Get(i-1))
			state->FreeProps(TRUE);
}

OP_STATUS SVGTraversalState::UpdateStackProps()
{
	if (m_traverse_stack->IsEmpty())
		return OpStatus::OK;

	TraverseStackState* root_state = m_traverse_stack->Get(0);

	unsigned i;
	for (i = 1; i < m_traverse_stack->GetCount(); i++)
	{
		TraverseStackState* state = m_traverse_stack->Get(i);
		// FIXME: It is not necessarily true that all state-frames
		// should share LayoutInfo with the root state-frame.
		state->layout_info = root_state->layout_info;
	}

	for (i = m_traverse_stack->GetCount(); i > 0; i--)
	{
		TraverseStackState* state = m_traverse_stack->Get(i-1);
		if (state->progress != ENTER_ELEMENT)
			state->SetupProps();
	}
	return OpStatus::OK;
}
#endif // SVG_INTERRUPTABLE_RENDERING

/**
 * Traverses the tree with a traversal object with the following semantics:
 *
 * ENTER_ELEMENT:
 *
 *  + Apply traversal filter (AllowElementTraverse), if FALSE is
 *  returned proceed to POP_ELEMENT.
 *
 *  + Setup state for element (cascade and potentially a LayoutInfo),
 *  if error POP_ELEMENT.
 *
 *  + Call the contexts Enter-hook. On errors (or 'signals' such as
 *  ELEMENT_IS_INVISIBLE), proceed to LEAVE_ELEMENT
 *
 *  + Call the contexts HandleContent-hook. If SKIP_CHILDREN is
 *  returned, proceed to LEAVE_ELEMENT
 *
 *  + Fetch the first child of the context (if any). If no children
 *  proceed to LEAVE_ELEMENT, else PUSH_CHILD
 *
 * PUSH_CHILD:
 *
 *  + Push the current child context, and fetch the next. Proceed to
 *  NEXT_CHILD.
 *
 * NEXT_CHILD:
 *
 *  + If no error has occurred, and there is a next child, proceed to
 *  PUSH_CHILD. Else LEAVE_ELEMENT.
 *
 * LEAVE_ELEMENT:
 *
 *  + Call the contexts Leave-hook and propagate the result if we have
 *  encountered an error.
 *
 * POP_ELEMENT:
 *
 *  + Free cascade (and LayoutInfo if appropriate) and pop stackframe.
 *
 */
OP_STATUS SVGTraverser::TraverseElement(SVGTraversalObject* traversal_object,
										TraverseStack* traverse_stack, BOOL allow_timeout)
{
	OP_NEW_DBG("TraverseElement", "svg_traverseelm");
	OP_ASSERT(traversal_object);
	OP_ASSERT(traverse_stack);

	SVGChildIterator* iterator_policy = traversal_object->GetChildPolicy();

	// Anyone inserting more local varables here will be shot.
	//
	// This is designed to be interruptible anywhere and then there
	// must be no state kept between states except for the ones on
	// traverse_stack.
	OP_STATUS full_traverse_result = OpStatus::OK;

	double start_ticks = g_op_time_info->GetRuntimeMS();

	BOOL timed_out = FALSE;
	while (!traverse_stack->IsEmpty() && !timed_out)
	{
		TraverseStackState* state = traverse_stack->Top();

		switch (state->progress)
		{
		case ENTER_ELEMENT:
			{
				PrintElmTypeIndent(state->element_info.traversed, "Enter", traverse_stack->GetCount());

				state->result = OpStatus::OK;

				// FIXME: Should probably move the filtering to make
				// it tighter (filter when walking the child-list -
				// has the added nuisance that the traversal root
				// needs to be 'special' though)
				if (!traversal_object->AllowElementTraverse(state->element_info))
				{
					// Skip this element
					state->progress = POP_ELEMENT;
					break;
				}

				// Sets up LayoutInfo (if crossing into shadow-tree), and LayoutProperties
				state->result = state->BeforeEnter(traversal_object);

				if (OpStatus::IsError(state->result))
				{
					// Skip this and propagate this error upwards
					state->progress = POP_ELEMENT;
					break;
				}

				traversal_object->UpdateValueContext(state->element_info);

				SVGElementContext* context = state->element_info.context;
				state->result = context->Enter(traversal_object, state->element_info);

				if (OpStatus::IsError(state->result))
				{
					// Skip this and propagate this error upwards
					state->progress = LEAVE_ELEMENT;
					break;
				}

				state->result = context->HandleContent(traversal_object, state->element_info);

				if (state->result == OpSVGStatus::SKIP_CHILDREN)
				{
					state->result = OpStatus::OK;
					state->progress = LEAVE_ELEMENT;
					break;
				}

				state->child_context = iterator_policy->FirstChild(state->element_info);
				if (state->child_context == NULL)
				{
					state->progress = LEAVE_ELEMENT;
					break;
				}

				state->progress = PUSH_CHILD;

				OP_ASSERT(state->progress > ENTER_ELEMENT);
			}
			// Fall through

		case PUSH_CHILD:
			{
				state->result = traverse_stack->PushNewFrame(state->child_context, state);

				// Move to next child, to allow removal of the child
				// we just pushed onto the stack
				SVGElementContext* context = state->element_info.context;
				state->child_context = iterator_policy->NextChild(context, state->child_context);
				state->progress = NEXT_CHILD;
			}
			break;

		case NEXT_CHILD:
			{
				if (OpStatus::IsSuccess(state->result))
				{
					if (state->child_context)
					{
						state->progress = PUSH_CHILD;
						break;
					}
				}

				if (state->result == OpSVGStatus::SKIP_SUBTREE)
					state->result = OpStatus::OK;

				state->progress = LEAVE_ELEMENT;
			}
			// Fall through

		case LEAVE_ELEMENT:
			{
				if (state->result == OpSVGStatus::SKIP_ELEMENT)
				{
					state->result = OpStatus::OK;
				}
				else
				{
					// The element was deemed invisible, and could now
					// possilbly have been removed from a possible
					// parent container. We still need to clean up any
					// interactions with state though, so call Leave()
					// FIXME: These interaction seem a bit fragile at
					// best, so we should consider making them less
					// so.
					if (state->result == OpSVGStatus::ELEMENT_IS_INVISIBLE)
						state->result = OpStatus::OK;

					SVGElementContext* context = state->element_info.context;
					OP_STATUS leave_result = context->Leave(traversal_object, state->element_info);

					if (OpStatus::IsSuccess(state->result))
					{
						state->result = leave_result;
					}
					else
					{
						// Things have already gone bad and we have an error code
						// to return in state->result but we've promised
						// to always call the Leave method
						OpStatus::Ignore(leave_result);
					}
				}

				PrintElmTypeIndent(state->element_info.traversed, "Leave", traverse_stack->GetCount());
			}
			// Fall through

		case POP_ELEMENT:
			{
				// Reset CSS
				state->FreeProps();

				OP_STATUS level_result = traverse_stack->Pop();
				if (traverse_stack->IsEmpty())
				{
					full_traverse_result = level_result;
				}
				else
				{
					TraverseStackState* top_state = traverse_stack->Top();
					traversal_object->UpdateValueContext(top_state->element_info);
					top_state->result = level_result;
				}
			}
			break;
		} // end switch

		if (allow_timeout &&
			state->progress == LEAVE_ELEMENT &&
			g_op_time_info->GetRuntimeMS() - start_ticks >= SVG_RENDERING_TIMESLICE_MS)
		{
			timed_out = TRUE;
		}
	} // end while (traverse_stack is not empty)

	if (traverse_stack->IsEmpty())
   		return full_traverse_result;

	OP_ASSERT(allow_timeout);
	return OpSVGStatus::TIMED_OUT;
}

/* static */
OP_STATUS SVGTraverser::CreateCascade(SVGElementInfo& element_info, LayoutProperties*& root_props,
									  Head& prop_list, HLDocProfile*& hld_profile)
{
	// Make sure cascades in referenced external documents use the
	// correct HLDocProfile, e.g see bug 271514.
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(element_info.traversed);
	if (!doc_ctx)
		return OpStatus::ERR;

	hld_profile = doc_ctx->GetHLDocProfile();
	if (!hld_profile)
		return OpStatus::ERR;

	HTML_Element* layouted_elm = element_info.layouted;
	root_props = LayoutProperties::CreateCascade(layouted_elm, prop_list, LAYOUT_WORKPLACE(hld_profile));

	return root_props ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

/* static */
OP_STATUS SVGTraverser::Traverse(SVGTraversalObject* traversal_object, HTML_Element* traversal_root,
								 LayoutProperties* root_props)
{
	TraverseStack traverse_stack;
	RETURN_IF_ERROR(PushTraversalRoot(traverse_stack, traversal_root));

	TraverseStackState* state = traverse_stack.Get(0);

	BOOL props_allocated = FALSE;
	Head prop_list;

	HLDocProfile* hld_profile = traversal_object->GetDocumentContext()->GetHLDocProfile();
	if (!root_props)
	{
		RETURN_IF_ERROR(CreateCascade(state->element_info, root_props, prop_list, hld_profile));

		state->SetChildCascade(root_props);
		props_allocated = TRUE;
	}

	LayoutWorkplace* lwp = hld_profile->GetLayoutWorkplace();
	SVGNumber root_font_size = LayoutFixedToSVGNumber(lwp->GetDocRootProperties().GetRootFontSize());

	traversal_object->UpdateValueContext(root_font_size);

	LayoutInfo info(lwp);
	state->layout_info = &info;
	state->element_info.props = root_props;

	// Potentially upgrade the invalid state (usually if starting within a SVG-fragment).
	SVGElementInvalidState initial_invalid_state = traversal_object->GetInitialInvalidState();
	state->element_info.state = MAX(state->element_info.state, static_cast<unsigned>(initial_invalid_state));

	OP_STATUS result = TraverseElement(traversal_object, &traverse_stack, FALSE /* allow timeout */);

	if (props_allocated)
		prop_list.Clear();

	return result;
}

/* static */
OP_STATUS SVGTraverser::PushTraversalRoot(TraverseStack& traverse_stack, HTML_Element* traversal_root)
{
	SVGElementContext* traversal_context = AttrValueStore::AssertSVGElementContext(traversal_root);
	if (!traversal_context)
		return OpStatus::ERR_NO_MEMORY;

	OP_ASSERT(traverse_stack.IsEmpty());

	return traverse_stack.PushNewFrame(traversal_context, NULL);
}

OP_STATUS SVGTraversalState::Init(HTML_Element* root)
{
	m_continue = FALSE;

	OP_ASSERT(m_traverse_stack == NULL);

	m_traverse_stack = OP_NEW(TraverseStack, ());
	if (!m_traverse_stack)
		return OpStatus::ERR_NO_MEMORY;

	return SVGTraverser::PushTraversalRoot(*m_traverse_stack, root);
}

SVGTraversalState::~SVGTraversalState()
{
	OP_DELETE(m_traversal_object);
	OP_DELETE(m_traverse_stack);
}

OP_STATUS SVGTraversalState::RunSlice(LayoutProperties* root_props)
{
	Head prop_list;
	BOOL props_allocated = FALSE;

	OP_ASSERT(m_traverse_stack && !m_traverse_stack->IsEmpty());

	TraverseStackState* state = m_traverse_stack->Get(0);

	HLDocProfile* hld_profile = m_traversal_object->GetDocumentContext()->GetHLDocProfile();
	if (!root_props)
	{
		RETURN_IF_ERROR(SVGTraverser::CreateCascade(state->element_info, root_props,
													prop_list, hld_profile));

		state->SetChildCascade(root_props);
		props_allocated = TRUE;
	}

	LayoutWorkplace* lwp = hld_profile->GetLayoutWorkplace();
	SVGNumber root_font_size = LayoutFixedToSVGNumber(lwp->GetDocRootProperties().GetRootFontSize());

	m_traversal_object->UpdateValueContext(root_font_size);

	LayoutInfo info(lwp);
	state->layout_info = &info;
	state->element_info.props = root_props;

	// Potentially upgrade the invalid state (usually if starting within a SVG-fragment).
	SVGElementInvalidState initial_invalid_state = m_traversal_object->GetInitialInvalidState();
	state->element_info.state = MAX(state->element_info.state, static_cast<unsigned>(initial_invalid_state));

#ifdef SVG_INTERRUPTABLE_RENDERING
	if (m_continue)
	{
		UpdateStackProps();

		m_continue = FALSE;
	}
#endif // SVG_INTERRUPTABLE_RENDERING

	OP_STATUS result = SVGTraverser::TraverseElement(m_traversal_object, m_traverse_stack, m_allow_timeout);

#ifdef SVG_INTERRUPTABLE_RENDERING
	if (result == OpSVGStatus::TIMED_OUT)
	{
		OP_ASSERT(m_allow_timeout);
		m_continue = TRUE;

		UnhookStackProps();
	}
#endif // SVG_INTERRUPTABLE_RENDERING

	// Clean props
	if (props_allocated)
		prop_list.Clear();

	return result;
}

OP_STATUS
SVGSimpleTraversalObject::CreateResolver()
{
	if (!m_resolver)
	{
		m_resolver = OP_NEW(SVGElementResolver, ());

		if (!m_resolver)
			return OpStatus::ERR_NO_MEMORY;

		m_owns_resolver = TRUE;
	}
	return OpStatus::OK;
}

OP_STATUS
SVGSimpleTraverser::TraverseElement(SVGSimpleTraversalObject* traversal_object, HTML_Element* element)
{
	OP_NEW_DBG("SVGSimpleTraverser::TraverseElement", "svg_traverse");

	BOOL needs_css = FALSE;
	OP_STATUS result = traversal_object->CSSAndEnterCheck(element, needs_css);
	if (OpStatus::IsError(result) || result == OpSVGStatus::SKIP_ELEMENT)
		return result;

	RETURN_IF_ERROR(traversal_object->Enter(element));

	result = traversal_object->DoContent(element);
	if (OpStatus::IsSuccess(result))
	{
		if (traversal_object->AllowChildTraverse(element))
		{
			HTML_Element* child = element->FirstChild();
			while (child)
			{
				if (child->GetNsType() == NS_SVG || child->IsText())
					result = TraverseElement(traversal_object, child);

				child = child->Suc();
			}
		}
	}

	OP_STATUS leave_status = traversal_object->Leave(element);

	if (OpStatus::IsSuccess(result))
		result = leave_status;
	else
		// We already have an error, but we need to call leave
		OpStatus::Ignore(leave_status);

	return result;
}

#ifdef SVG_SUPPORT_STENCIL
OP_STATUS SVGVisualTraversalObject::GenerateClipOrMask(HTML_Element* element)
{
	// Make sure cascades in referenced external documents use the
	// correct HLDocProfile, e.g see bug 271514.
	SVGDocumentContext* element_docctx = AttrValueStore::GetSVGDocumentContext(element);
	if (!element_docctx)
		return OpStatus::ERR;

	SVGElementContext* element_context = AttrValueStore::AssertSVGElementContext(element);
	if (!element_context)
		return OpStatus::ERR_NO_MEMORY;

	// Mark the clipPath/mask context as invalid to have it rebuild everything
	element_context->AddInvalidState(INVALID_ADDED);

	SVGCanvasStateStack cs;
	RETURN_IF_ERROR(cs.Save(m_canvas));

	m_canvas->SetDefaults(m_doc_ctx->GetRenderingQuality());

	// Now traverse and draw the clipping/mask into the stencilbuffer
	SVGDocumentContext* saved_doc_ctx = GetDocumentContext();
	SetDocumentContext(element_docctx);
	BOOL saved_use_traversal_filter = UseTraversalFilter();
	SetTraversalFilter(FALSE);
	BOOL saved_allow_raster_fonts = AllowRasterFonts();
	SetAllowRasterFonts(FALSE);
	BOOL saved_detached_operation = GetDetachedOperation();
	SetDetachedOperation(TRUE);
	List<SVGPaintNode> saved_pending_nodes;
	SavePendingNodes(saved_pending_nodes);

	OP_STATUS result = SVGTraverser::Traverse(this, element, NULL);

	RestorePendingNodes(saved_pending_nodes);
	SetDetachedOperation(saved_detached_operation);
	SetAllowRasterFonts(saved_allow_raster_fonts);
	SetTraversalFilter(saved_use_traversal_filter);
	SetDocumentContext(saved_doc_ctx);

	OP_STATUS restore_result = cs.Restore();
	if (OpStatus::IsSuccess(result))
		result = restore_result;
	else
		OpStatus::Ignore(restore_result);

	return result;
}
#endif // SVG_SUPPORT_STENCIL

OP_STATUS SVGVisualTraversalObject::ValidateUse(SVGElementInfo& info)
{
	// We need to build a shadow tree of shadow nodes for the use tag.
	if (!SVGUtils::HasBuiltNormalShadowTree(info.traversed))
	{
		OP_STATUS status = SVGUtils::BuildShadowTreeForUseTag(m_resolver, info.traversed, info.layouted, m_doc_ctx);
		if (status == OpSVGStatus::GENERAL_DOCUMENT_ERROR ||
			status == OpSVGStatus::DATA_NOT_LOADED_ERROR)
			return InvisibleElement(info);

		RETURN_IF_ERROR(status);

		OP_ASSERT(SVGUtils::HasBuiltNormalShadowTree(info.traversed));
	}
	else
	{
		HTML_Element* target = NULL;
		OP_STATUS status = SVGUtils::LookupAndVerifyUseTarget(m_resolver, m_doc_ctx, info.layouted,
															  info.traversed, TRUE /* animated */,
															  target);
		if (OpStatus::IsSuccess(status))
			m_doc_ctx->RegisterDependency(info.layouted, target);

		// We don't actually expect lookup to fail here do we?
		// It will trigger for cases where the reference is invalid. These cases should be silently ignored.
		//OP_ASSERT(OpStatus::IsSuccess(status) || status == OpSVGStatus::GENERAL_DOCUMENT_ERROR);
	}

	return OpStatus::OK;
}

OP_STATUS SVGTraversalObject::InvisibleElement(SVGElementInfo& info)
{
	const OpRect empty;

	// Remove from parent list
	info.context->Out();

	// We never normally clear flags in these trees since we don't traverse it,
	// but they must be cleared some time so we do it here.
	HTML_Element* it = info.traversed;
	HTML_Element* stop_it = static_cast<HTML_Element*>(info.traversed->NextSibling());
	while (it != stop_it)
	{
		if (it->GetNsType() == NS_SVG)
		{
			// If they are created later they will have their default flags which are wrong.
			SVGElementContext *it_elem_ctx = AttrValueStore::AssertSVGElementContext(it);
			if (!it_elem_ctx)
				return OpStatus::ERR_NO_MEMORY;

			it_elem_ctx->ClearInvalidState();
		}

		it = it->Next();
	}

	return OpSVGStatus::ELEMENT_IS_INVISIBLE;
}

OP_STATUS SVGLayoutObject::InvisibleElement(SVGElementInfo& info)
{
	// Remove from paint tree
	DetachPaintNode(info);

	return SVGTraversalObject::InvisibleElement(info);
}

static OP_STATUS CalculateTextLengthAdjustment(SVGTextArguments& tparams, HTML_Element* layouted_elm,
											   UINT32 numchars, const SVGValueContext& vcxt)
{
	OP_NEW_DBG("CalculateTextLengthAdjustment", "svg_text");
	OP_ASSERT(tparams.chunk_list);

	SVGLengthObject* tlen = NULL;
	AttrValueStore::GetLength(layouted_elm, Markup::SVGA_TEXTLENGTH, &tlen);
	if (!tlen) // Error ?
		return OpStatus::OK;

	SVGLength::LengthOrientation majordir = tparams.IsVertical() ?
		SVGLength::SVGLENGTH_Y : SVGLength::SVGLENGTH_X;

	SVGNumber user_extent = SVGUtils::ResolveLength(tlen->GetLength(), majordir, vcxt);

	// "A negative value is an error"
	if (user_extent < 0)
		return OpStatus::OK;

	SVGNumber diff = user_extent - tparams.total_extent;

	// Simple adjustment
	SVGLengthAdjust lenadj =
		(SVGLengthAdjust)AttrValueStore::GetEnumValue(layouted_elm, Markup::SVGA_LENGTHADJUST,
													  SVGENUM_LENGTHADJUST,
													  SVGLENADJUST_SPACING);
	if (lenadj == SVGLENADJUST_SPACINGANDGLYPHS)
	{
		if (tparams.total_extent > 0)
			tparams.glyph_scale = user_extent / tparams.total_extent;

		tparams.extra_spacing = 0;
	}
	else /* lenadj == SVGLENADJUST_SPACING */
	{
		if (numchars > 1)
			tparams.extra_spacing = diff / (numchars - 1);
	}

	OP_DBG(("Measured extent: %g, textLength: %g, Extra spacing: %g, Glyph Stretch: %g",
			tparams.total_extent.GetFloatValue(),
			user_extent.GetFloatValue(),
			tparams.extra_spacing.GetFloatValue(),
			tparams.glyph_scale.GetFloatValue()));

	// Hmm, this is very crude
	tparams.total_extent = user_extent;

	// Adjust the textchunks
	for (UINT32 i = 0; i < tparams.chunk_list->GetCount(); i++)
	{
		tparams.chunk_list->Get(i)->extent *= tparams.glyph_scale;
	}

#if 0 //def _DEBUG
	// Verify sum of chunk extents
	SVGNumber new_extent(0);
	for (UINT32 i = 0; i < tparams.chunk_list->GetCount(); i++)
	{
		new_extent += *tparams.chunk_list->Get(i);
	}
	OP_ASSERT(new_extent.Close(tparams.total_extent));
#endif // _DEBUG

	return OpStatus::OK;
}

static
void GetLengthOrAuto(HTML_Element* element, Markup::AttrType attr,
					 SVGLength::LengthOrientation lenorient, const SVGValueContext& vcxt,
					 SVGNumber& resolved_len, BOOL& is_auto)
{
	SVGProxyObject* proxy = NULL;
	AttrValueStore::GetProxyObject(element, attr, &proxy);
	if (proxy &&
		proxy->GetRealObject() &&
		proxy->GetRealObject()->Type() == SVGOBJECT_LENGTH)
	{
		SVGLengthObject* len = static_cast<SVGLengthObject*>(proxy->GetRealObject());
		resolved_len =
			SVGUtils::ResolveLength(len->GetLength(), lenorient, vcxt);
		is_auto = FALSE;
	}
	else
	{
		// 'auto'
		resolved_len = 0;
		is_auto = TRUE;
	}
}

static
void FetchTextAreaValues(HTML_Element* layouted_elm, const SVGValueContext& vcxt, SVGTextAreaInfo* area)
{
	// Fetch attributes
	SVGLengthObject* xl = NULL;
	AttrValueStore::GetLength(layouted_elm, Markup::SVGA_X, &xl);
	if (xl)
		area->vis_area.x = SVGUtils::ResolveLength(xl->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
	SVGLengthObject* yl = NULL;
	AttrValueStore::GetLength(layouted_elm, Markup::SVGA_Y, &yl);
	if (yl)
		area->vis_area.y = SVGUtils::ResolveLength(yl->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

	BOOL is_auto = TRUE;
	GetLengthOrAuto(layouted_elm, Markup::SVGA_WIDTH, SVGLength::SVGLENGTH_X, vcxt,
					area->vis_area.width, is_auto);
	area->info.width_auto = is_auto ? 1 : 0;

	GetLengthOrAuto(layouted_elm, Markup::SVGA_HEIGHT, SVGLength::SVGLENGTH_Y, vcxt,
					area->vis_area.height, is_auto);
	area->info.height_auto = is_auto ? 1 : 0;
}

static void FetchAttributeIntoStack(HTML_Element* element, Markup::AttrType attr_type,
									SVGVectorStack& stack, unsigned offset)
{
	SVGVector* vector;
	AttrValueStore::GetVector(element, attr_type, vector);
	// Do we really have to insert NULL vectors? We do it because we used to do it.
	stack.Push(vector, offset);
}

OP_STATUS SVGVisualTraversalObject::PushTextProperties(SVGElementInfo& info)
{
	OP_ASSERT(m_textinfo != NULL);

	const HTMLayoutProperties& props = *info.props->GetProps();

	// Transfer properties that affect the look of text to the drawing state

	// If text-decoration colors changed, mark the current paint
	// as the paint to use when drawing text decorations
	if (m_canvas->DecorationsChanged(props))
		m_canvas->SetTextDecoPaint();

	const SvgProperties *svg_props = props.svg;
	{
		FontAtt new_logfont;
		new_logfont.SetSize(svg_props->fontsize.GetIntegerValue());
		new_logfont.SetFontNumber(props.font_number);
		new_logfont.SetWeight(props.font_weight);
		new_logfont.SetSmallCaps(props.small_caps == CSS_VALUE_small_caps);

		if (props.font_italic == FONT_STYLE_ITALIC)
		{
			new_logfont.SetItalic(TRUE);
		}
		else if (props.font_italic == SVGFONTSTYLE_NORMAL)
		{
			new_logfont.SetItalic(FALSE);
		}

		m_canvas->SetLogFont(new_logfont);
	}

	// Start a new boundingbox
	RETURN_IF_MEMORY_ERROR(m_textinfo->bbox.PushNew());

	// TextContent.attrib:
	//		alignment-baseline, baseline-shift, direction, dominant-baseline,
	//		glyph-orientation-horizontal, glyph-orientation-vertical, kerning,
	//		letter-spacing, text-anchor, text-decoration, unicode-bidi, word-spacing
	//
	// and lengthAdjust, textLength that are common for text elements

	// "If the attribute is not specified, the effect is as if a value of "0" were specified."
	unsigned char_offset = m_textinfo->total_char_sum;
	FetchAttributeIntoStack(info.layouted, Markup::SVGA_X, m_textinfo->xlist, char_offset);
	FetchAttributeIntoStack(info.layouted, Markup::SVGA_Y, m_textinfo->ylist, char_offset);
	FetchAttributeIntoStack(info.layouted, Markup::SVGA_DX, m_textinfo->dxlist, char_offset);
	FetchAttributeIntoStack(info.layouted, Markup::SVGA_DY, m_textinfo->dylist, char_offset);
	FetchAttributeIntoStack(info.layouted, Markup::SVGA_ROTATE, m_textinfo->rotatelist, char_offset);

#ifdef SUPPORT_TEXT_DIRECTION
	// ED: Consider just removing the packed.rtl flag, we could use the props directly instead.
	m_textinfo->packed.rtl = props.direction == CSS_VALUE_rtl ? 1 : 0;
#endif // SUPPORT_TEXT_DIRECTION

	m_textinfo->packed.anchor = svg_props->GetTextAnchor();

	if (info.layouted->HasAttr(XMLA_SPACE, NS_IDX_XML))
		m_textinfo->PushSpace(info.layouted->GetBoolAttr(XMLA_SPACE, NS_IDX_XML));

	if (info.layouted->HasAttr(XMLA_LANG, NS_IDX_XML))
		m_textinfo->PushLang(const_cast<uni_char*>(info.layouted->GetStringAttr(XMLA_LANG, NS_IDX_XML)));

#ifdef SUPPORT_TEXT_DIRECTION
	// For the 'direction' property to have any effect on an element that does not by itself
	// establish a new text chunk (such as a 'tspan' element without absolute position adjustments
	// due to 'x' or 'y' attributes), the 'unicode-bidi' property's value must be embed or bidi-override.
	// FIXME: We still should push here for e.g tspan elements if they are abs.pos'ed.
	if (info.context == m_textinfo->text_root_container || props.unicode_bidi != CSS_VALUE_normal)
		if (!m_textinfo->text_root_container->PushBidiProperties(&props))
			return OpStatus::ERR_NO_MEMORY;
#endif // SUPPORT_TEXT_DIRECTION

	// Setup the fontdescriptor
	m_textinfo->font_desc.SetFont(m_canvas->GetLogFont());
	return OpStatus::OK;
}

OP_STATUS SVGVisualTraversalObject::SetupTextOverflow(SVGElementInfo& info)
{
	const HTMLayoutProperties& props = *info.props->GetProps();

	if (props.text_overflow == CSS_VALUE_ellipsis)
	{
		if (info.layouted->IsMatchingType(Markup::SVGE_TEXTAREA, NS_SVG))
		{
			m_textinfo->packed.use_ellipsis =
				!(m_textarea_info->info.width_auto || m_textarea_info->info.height_auto);

			m_textarea_info->ellipsis_end_offset = 0;
			m_textarea_info->info.has_ellipsis = FALSE;
			m_textarea_info->info.has_ws_ellipsis = FALSE;
		}
		else if (AttrValueStore::HasObject(info.layouted, Markup::SVGA_WIDTH, NS_IDX_SVG))
		{
			m_textinfo->packed.use_ellipsis = 1;

			SVGLengthObject* width;
			AttrValueStore::GetLength(info.layouted, Markup::SVGA_WIDTH, &width);

			const SVGValueContext& vcxt = GetValueContext();
			m_textinfo->max_extent = SVGUtils::ResolveLength(width->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
		}
	}
	return OpStatus::OK;
}

static inline BOOL ElementHasTextLength(HTML_Element* layouted_elm)
{
	// FIXME: Check for value > 0 instead
	return AttrValueStore::HasObject(layouted_elm, Markup::SVGA_TEXTLENGTH, NS_IDX_SVG);
}

OP_STATUS SVGVisualTraversalObject::SetupExtents(SVGElementInfo& info)
{
	Markup::Type type = info.layouted->Type();
	// 'textArea' has no notion of textLength?
	OP_ASSERT(type != Markup::SVGE_TEXTAREA);
	// We might assert the below as well, because textLength on a
	// tspan in a textArea would be 'undefined' as of yet.
	OP_ASSERT(m_textarea_info == NULL);

	SVGTextContainer* text_cont_ctx = static_cast<SVGTextContainer*>(info.context);
	if (info.GetInvalidState() < INVALID_FONT_METRICS)
	{
		m_textinfo->total_extent = text_cont_ctx->GetTextExtent();
		m_textinfo->glyph_scale = text_cont_ctx->GetGlyphScale();
		m_textinfo->extra_spacing = text_cont_ctx->GetExtraSpacing();
		if (type == Markup::SVGE_TEXT)
		{
			SVGTextElement* text_ctx = static_cast<SVGTextElement*>(info.context);
			m_textinfo->chunk_list = &text_ctx->GetChunkList();
		}
	}
	else
	{
		BOOL textlength_calculation_needed = FALSE;
		if (ElementHasTextLength(info.layouted))
			textlength_calculation_needed = TRUE;

		// Extents needed for:
		// textLength,
		// chunk calculations (i.e every 'text' element),
		// text-anchor
		BOOL really_get_extent =
			textlength_calculation_needed ||
			type == Markup::SVGE_TEXT ||
			m_textinfo->NeedsAlignment();

		// FIXME: Where to keep this data?
		if (!really_get_extent)
		{
			text_cont_ctx->ResetCache();
			return OpStatus::OK;
		}

		unsigned int extent_flags = SVGTextData::EXTENT;

		if (textlength_calculation_needed)
		{
			// If textLength is set, we need to count the number of
			// characters/glyphs
			extent_flags |= SVGTextData::NUMCHARS;
		}

		SVGTextData data(extent_flags);

		if (type == Markup::SVGE_TEXT)
		{
			SVGTextElement* text_ctx = static_cast<SVGTextElement*>(info.context);
			OpVector<SVGTextChunk>* chunk_list = &text_ctx->GetChunkList();
			chunk_list->DeleteAll();

			data.chunk_list = chunk_list;
			m_textinfo->chunk_list = chunk_list;
		}

		RETURN_IF_ERROR(SVGUtils::GetTextElementExtent(info.layouted, m_doc_ctx, 0, -1, data,
													   GetCurrentViewport(), m_canvas));

		m_textinfo->total_extent = data.extent;

		if (textlength_calculation_needed && data.numchars > 0)
		{
			// Modifies m_textinfo->total_extent
			RETURN_IF_ERROR(CalculateTextLengthAdjustment(*m_textinfo, info.layouted,
														  data.numchars, GetValueContext()));
		}

		// Store the extent to be used later (saves time)
		text_cont_ctx->SetTextExtent(m_textinfo->total_extent);
		text_cont_ctx->SetGlyphScale(m_textinfo->glyph_scale);
		text_cont_ctx->SetExtraSpacing(m_textinfo->extra_spacing);
	}
	return OpStatus::OK;
}

OP_STATUS SVGLayoutObject::SetupExtents(SVGElementInfo& info)
{
	OP_STATUS status = SVGVisualTraversalObject::SetupExtents(info);

	if (OpStatus::IsSuccess(status))
	{
		SVGTextAttributePaintNode* attr_paint_node = static_cast<SVGTextAttributePaintNode*>(info.paint_node);
		OP_ASSERT(attr_paint_node);

		TextAttributes* text_attrs = attr_paint_node->GetAttributes();

		text_attrs->extra_spacing = m_textinfo->extra_spacing;
		text_attrs->glyph_scale = m_textinfo->glyph_scale;
	}
	return status;
}

OP_STATUS SVGTraversalObject::CreateTextInfo(SVGTextRootContainer* container, SVGTextData* textdata /* = NULL */)
{
	OP_ASSERT(m_textinfo == NULL);

	m_textinfo = OP_NEW(SVGTextArguments, ());
	if (!m_textinfo)
		return OpStatus::ERR_NO_MEMORY;

	m_textinfo->data = textdata;
	m_textinfo->text_root_container = container;

	return OpStatus::OK;
}

void SVGTraversalObject::DestroyTextInfo()
{
	OP_DELETE(m_textinfo);
	m_textinfo = NULL;
}

void SVGTraversalObject::SetupTextRootProperties(SVGElementInfo& info)
{
	m_textinfo->packed.preserve_spaces = SVGUtils::GetPreserveSpaces(info.traversed) ? 1 : 0;

#if defined(SVG_SUPPORT_TEXTSELECTION) && defined(SVG_SUPPORT_EDITABLE)
	SVGTextRootContainer* text_root_cont = static_cast<SVGTextRootContainer*>(info.context);
	if (text_root_cont->IsEditing() || text_root_cont->IsCaretVisible())
		m_textinfo->editable = text_root_cont->GetEditable();
#endif // SVG_SUPPORT_TEXTSELECTION && SVG_SUPPORT_EDITABLE

	// Reset chunk counter
	m_textinfo->current_chunk = 0;

	const SvgProperties* svg_props = info.props->GetProps()->svg;
	m_textinfo->writing_mode = svg_props->GetWritingMode();
}

OP_STATUS SVGVisualTraversalObject::EnterTextRoot(SVGElementInfo& info)
{
	OpRect painter_rect;
	m_canvas->SetPainterRect(painter_rect);

	OP_ASSERT(m_textinfo != NULL);

	// Open the flood-gates so that we traverse all children of the
	// text-root (needed to get CTP and possibly other things right).
	// Note-to-self: Store CTP (and that potential other state) in the
	// text-containers to avoid this.
	m_textinfo->packed.prev_use_traversal_filter = UseTraversalFilter() ? 1 : 0;
	SetTraversalFilter(FALSE);

#ifdef DEBUG_ENABLE_OPASSERT
	Markup::Type type = info.layouted->Type();
#endif // DEBUG_ENABLE_OPASSERT
	OP_ASSERT(type == Markup::SVGE_TEXT || type == Markup::SVGE_TEXTAREA);

	SetupTextRootProperties(info);

	PushTextProperties(info);
	return OpStatus::OK;
}

void SVGLayoutObject::SetTextAttributes(SVGElementInfo& info)
{
	SVGTextAttributePaintNode* attr_paint_node = static_cast<SVGTextAttributePaintNode*>(info.paint_node);
	OP_ASSERT(attr_paint_node);

	TextAttributes* text_attrs = attr_paint_node->GetAttributes();

	m_canvas->GetDecorationPaints(text_attrs->deco_paints);

	const FontAtt& fa = m_textinfo->font_desc.GetFontAttributes();
	const HTMLayoutProperties& props = *info.props->GetProps();
	const SvgProperties *svg_props = props.svg;

	text_attrs->fontsize = svg_props->fontsize;

	text_attrs->letter_spacing = SVGUtils::GetSpacing(info.layouted, Markup::SVGA_LETTER_SPACING, props);
	text_attrs->word_spacing = SVGUtils::GetSpacing(info.layouted, Markup::SVGA_WORD_SPACING, props);

	text_attrs->baseline_shift = m_textinfo->baseline_shift;

	text_attrs->writing_mode = svg_props->GetWritingMode();

#ifdef SVG_SUPPORT_TEXTSELECTION
	GetSelectionColors(m_doc_ctx, props, text_attrs->selection_bg_color, text_attrs->selection_fg_color);
#endif // SVG_SUPPORT_TEXTSELECTION

	if (text_attrs->writing_mode != SVGWRITINGMODE_TB_RL &&
		text_attrs->writing_mode != SVGWRITINGMODE_TB)
	{
		text_attrs->orientation_is_auto = FALSE;
		text_attrs->orientation = svg_props->textinfo.glyph_orientation_horizontal;
	}
	else
	{
		if (svg_props->textinfo.glyph_orientation_vertical_is_auto)
		{
			text_attrs->orientation_is_auto = TRUE;
		}
		else
		{
			text_attrs->orientation_is_auto = FALSE;
			text_attrs->orientation = svg_props->textinfo.glyph_orientation_vertical;
		}
	}

	text_attrs->SetLanguage(m_textinfo->GetCurrentLang());

	text_attrs->decorations = 0;

	if (props.linethrough_color != USE_DEFAULT_COLOR)
		text_attrs->decorations |= TextAttributes::DECO_LINETHROUGH;
	if (props.underline_color != USE_DEFAULT_COLOR)
		text_attrs->decorations |= TextAttributes::DECO_UNDERLINE;
	if (props.overline_color != USE_DEFAULT_COLOR)
		text_attrs->decorations |= TextAttributes::DECO_OVERLINE;

	text_attrs->geometric_precision = (svg_props->textinfo.textrendering == SVGTEXTRENDERING_GEOMETRICPRECISION);

	text_attrs->weight = fa.GetWeight();
	text_attrs->italic = fa.GetItalic();
	text_attrs->smallcaps = fa.GetSmallCaps();
}

OP_STATUS SVGLayoutObject::EnterTextRoot(SVGElementInfo& info)
{
	OP_NEW_DBG("EnterTextRoot", "svg_bidi_trav");
	OP_DBG(("EnterTextRoot"));

	m_textinfo->SetIsLayout(TRUE);

	OP_STATUS status = SVGVisualTraversalObject::EnterTextRoot(info);

	info.context->SetBBoxIsValid(FALSE);

	if (OpStatus::IsSuccess(status) && info.paint_node /* NULL if 'display: none' */)
	{
		SetTextAttributes(info);

		SVGTextAttributePaintNode* attr_paint_node = static_cast<SVGTextAttributePaintNode*>(info.paint_node);

		RETURN_IF_ERROR(m_textinfo->PushAttributeNode(attr_paint_node));

		info.has_text_attrs = 1;

		RETURN_IF_ERROR(PushContainerState(info));

		m_current_container->Insert(attr_paint_node, m_current_pred);

		m_current_container = attr_paint_node;
		m_current_pred = NULL;

		// Reset the child list if we are re-layouting this subtree
		if (info.GetInvalidState() > INVALID_STRUCTURE)
			m_current_container->Clear();
	}
	return status;
}

OP_STATUS SVGVisualTraversalObject::LeaveTextRoot(SVGElementInfo& info)
{
	OP_ASSERT(m_textinfo != NULL);

	if (m_canvas->IsPainterActive())
		m_canvas->ReleasePainter();

	// Restore the traversal filter
	SetTraversalFilter(m_textinfo->packed.prev_use_traversal_filter);

	if (m_textarea_info)
	{
		m_textarea_info->FlushLine();

		OP_DELETE(m_textarea_info);
		m_textarea_info = NULL;

		m_textinfo->area = NULL;
	}

#ifdef SUPPORT_TEXT_DIRECTION
	SVGTextRootContainer* text_root_cont = static_cast<SVGTextRootContainer*>(info.context);
	text_root_cont->ResetBidi(); // This assumes we never traverse into nested textroot elements
#endif // SUPPORT_TEXT_DIRECTION

	return OpStatus::OK;
}

OP_STATUS SVGLayoutObject::LeaveTextRoot(SVGElementInfo& info)
{
	if (info.has_text_attrs)
		m_textinfo->PopAttributeNode();

	PopContainerState(info);

	// If the paint node wasn't detached, set it to be the predecessor
	// of the following node
	if (info.paint_node && info.paint_node->GetParent())
		m_current_pred = info.paint_node;

	return SVGVisualTraversalObject::LeaveTextRoot(info);
}

#ifdef SVG_SUPPORT_EDITABLE
void SVGVisualTraversalObject::FlushCaret(SVGElementInfo& info)
{
	// If the text element is being edited, but the caret hasn't yet
	// been painted, paint it now
#ifdef SVG_SUPPORT_TEXTSELECTION
	SVGTextSelection& sel = m_doc_ctx->GetTextSelection();
	BOOL is_selecting = sel.IsValid() && (sel.IsSelecting() || !sel.IsEmpty());
#else
	BOOL is_selecting = FALSE;
#endif // SVG_SUPPORT_TEXTSELECTION
	if (!is_selecting && m_textinfo->editable && !m_textinfo->packed.has_painted_caret)
	{
		const HTMLayoutProperties& props = *info.props->GetProps();

		SVGEditable* editable = m_textinfo->editable;
		SVGNumber fontheight = props.svg->GetFontSize(info.layouted);
		editable->m_caret.m_pos.x = m_textinfo->ctp.x + 1.5;
		editable->m_caret.m_pos.y = m_textinfo->ctp.y - fontheight;

		editable->m_caret.m_glyph_pos = editable->m_caret.m_pos;

		editable->m_caret.m_glyph_pos.height = -fontheight;
		editable->m_caret.m_pos.height = fontheight;

		editable->Paint(m_canvas);

		m_textinfo->packed.has_painted_caret = 1;

		editable->m_caret.m_line = 0;
	}
}
#endif // SVG_SUPPORT_EDITABLE

void SVGVisualTraversalObject::PopTextProperties(SVGElementInfo& info)
{
	OP_ASSERT(m_textinfo != NULL);

	Markup::Type type = info.layouted->Type();

	if (info.layouted->HasAttr(XMLA_LANG, NS_IDX_XML))
		m_textinfo->PopLang();

	if (info.layouted->HasAttr(XMLA_SPACE, NS_IDX_XML))
		m_textinfo->PopSpace();

	if (type == Markup::SVGE_TSPAN || type == Markup::SVGE_TREF ||
		type == Markup::SVGE_ALTGLYPH)
	{
		m_textinfo->xlist.Pop();
		m_textinfo->ylist.Pop();
		m_textinfo->dxlist.Pop();
		m_textinfo->dylist.Pop();
		m_textinfo->rotatelist.Pop();
	}

	if (type == Markup::SVGE_TSPAN || type == Markup::SVGE_TREF ||
		type == Markup::SVGE_ALTGLYPH || type == Markup::SVGE_TEXTPATH ||
		type == Markup::SVGE_TEXTAREA)
	{
		m_textinfo->bbox.Pop();
	}
}

static SVGNumber ResolveLocalBaseline(SVGElementInfo& info, const SVGValueContext& context)
{
	const SvgProperties *svg_props = info.props->GetProps()->svg;

	SVGNumber baseline; // Initially zero
	const SVGBaselineShift& bls = svg_props->baselineshift;
	if (bls.GetShiftType() != SVGBaselineShift::SVGBASELINESHIFT_BASELINE)
	{
		SVGNumber fontheight = svg_props->GetFontSize(info.layouted);
		switch (bls.GetShiftType())
		{
		case SVGBaselineShift::SVGBASELINESHIFT_SUB:
			baseline = -fontheight / 2;
			break;

		case SVGBaselineShift::SVGBASELINESHIFT_SUPER:
			baseline = fontheight / 2;
			break;

		case SVGBaselineShift::SVGBASELINESHIFT_VALUE:
			{
				const SVGLength& len = bls.GetValue();
				if (len.GetUnit() == CSS_PERCENTAGE)
				{
					baseline = fontheight * len.GetScalar() / 100;
				}
				else
				{
					baseline = SVGUtils::ResolveLength(len, SVGLength::SVGLENGTH_X, context);
				}
			}
			break;
		default:
			OP_ASSERT(0);
			break;
		}
	}
	return baseline;
}

static void ApplyBaselineAdjustment(SVGElementInfo& info, const SVGValueContext& context,
									SVGTextArguments* tparams)
{
	SVGNumber local_baseline_shift = ResolveLocalBaseline(info, context);

	tparams->baseline_shift += local_baseline_shift;
}

static void UnapplyBaselineAdjustment(SVGElementInfo& info, const SVGValueContext& context,
									  SVGTextArguments* tparams)
{
	SVGNumber local_baseline_shift = ResolveLocalBaseline(info, context);

	tparams->baseline_shift -= local_baseline_shift;
}

OP_STATUS SVGVisualTraversalObject::LeaveTextContainer(SVGElementInfo& info)
{
	OP_ASSERT(m_textinfo != NULL);

	UnapplyBaselineAdjustment(info, GetValueContext(), m_textinfo);

	PopTextProperties(info);

	Markup::Type type = info.layouted->Type();

	if (type == Markup::SVGE_TEXTPATH)
	{
		OP_DELETE(m_textinfo->path);
		m_textinfo->path = NULL;

		m_textinfo->AddPendingChunk();
	}

	if (info.has_transform)
		m_textinfo->font_desc.SetScaleChanged(TRUE);

#ifdef SUPPORT_TEXT_DIRECTION
	const HTMLayoutProperties& props = *info.props->GetProps();
	if (info.context == m_textinfo->text_root_container || props.unicode_bidi != CSS_VALUE_normal)
		if (!m_textinfo->text_root_container->PopBidiProperties())
			return OpStatus::ERR_NO_MEMORY;
#endif

	return OpStatus::OK;
}

OP_STATUS SVGLayoutObject::LeaveTextContainer(SVGElementInfo& info)
{
	// m_textinfo could be NULL if we encounter OOM during Enter,
	// otherwise it is not expected to occur
	if (m_textinfo)
	{
		info.context->SetBBox(m_textinfo->bbox.Get());
		info.context->SetBBoxIsValid(TRUE);

		if (info.has_text_attrs)
			m_textinfo->PopAttributeNode();
	}

	PopContainerState(info);

	// If the paint node wasn't detached, set it to be the predecessor
	// of the following node
	if (info.paint_node && info.paint_node->GetParent())
		m_current_pred = info.paint_node;

	return SVGVisualTraversalObject::LeaveTextContainer(info);
}

OP_STATUS SVGIntersectionObject::EnterElement(SVGElementInfo& info, const TransformInfo& tinfo)
{
	SetupGeometricProperties(info);
	SetupPaintProperties(info);

#ifdef SVG_SUPPORT_STENCIL
	const HTMLayoutProperties& props = *info.props->GetProps();
	const SvgProperties* svg_props = props.svg;

	// Push clipPaths (not masks since they're not used during intersection)
	if (!svg_props->clippath.info.is_none)
	{
		m_stencil_target.doc_ctx = m_doc_ctx;
		m_stencil_target.element = info.traversed;
		m_stencil_target.viewport = GetCurrentViewport();

		RETURN_IF_ERROR(SetupClipOrMask(info, svg_props->clippath, Markup::SVGE_CLIPPATH, RESOURCE_CLIPPATH));
	}
#endif // SVG_SUPPORT_STENCIL

	return OpStatus::OK;
}

OP_STATUS SVGIntersectionObject::LeaveElement(SVGElementInfo& info)
{
	return SVGVisualTraversalObject::LeaveContainer(info);
}

OP_STATUS SVGIntersectionObject::EnterContainer(SVGElementInfo& info, const TransformInfo& tinfo)
{
	return SVGIntersectionObject::EnterElement(info, tinfo);
}

#ifndef FOLLOW_THE_SVG_1_1_SPEC_EVEN_WHEN_ITS_UNUSABLE
/* static */ void
SVGTraversalObject::CalculateSurroundingWhiteSpace(HTML_Element* traversed_elm, SVGDocumentContext* doc_ctx,
												   BOOL& keep_trailing_whitespace)
{
	// Set default values
	keep_trailing_whitespace = FALSE;
	OP_ASSERT(SVGUtils::GetElementToLayout(traversed_elm)->IsText() ||
			  SVGUtils::GetElementToLayout(traversed_elm)->Type() == Markup::SVGE_TREF);

	HTML_Element* parent_text_trav = traversed_elm;
	HTML_Element* parent_text_layout;
	do
	{
		parent_text_trav = parent_text_trav->Parent();
		if (!parent_text_trav)
		{
			break;
		}
		parent_text_layout = SVGUtils::GetElementToLayout(parent_text_trav);
	}
	while (parent_text_layout && !SVGUtils::IsTextRootElement(parent_text_layout));

	if (!parent_text_trav)
	{
		return;
	}

	// Now to calculate keep_trailing_whitespace. That will be true if
	// there exist a block after us with any non whitespace in it
	HTML_Element* stop = static_cast<HTML_Element*>(parent_text_trav->NextSibling());
	HTML_Element* next_text_trav = traversed_elm->Next();
	while (next_text_trav != stop && !keep_trailing_whitespace)
	{
		HTML_Element* next_text_layout = SVGUtils::GetElementToLayout(next_text_trav);
		const uni_char* text = NULL;
		unsigned textlen = 0;
		BOOL owns_text = FALSE;
		NS_Type ns = next_text_layout->GetNsType();
		if (next_text_layout->Type() == HE_TEXT)
		{
			TextData* text_data = next_text_layout->GetTextData();
			text = text_data->GetText();
			textlen = text_data->GetTextLen();
		}
		else if (ns == NS_SVG)
		{
			Markup::Type type = next_text_layout->Type();
			if(type == Markup::SVGE_TREF)
			{
				HTML_Element* target = SVGUtils::FindHrefReferredNode(NULL, doc_ctx, next_text_layout);
				if(target)
				{
					doc_ctx->RegisterDependency(next_text_trav, target);
					unsigned text_data_len = target->GetTextContentLength();
					uni_char* text_data = OP_NEWA(uni_char, text_data_len+1);
					if (text_data)
					{
						textlen = target->GetTextContent(text_data, text_data_len+1);
						text = text_data;
						owns_text = TRUE;
					}
				}
			}
			else if (type == Markup::SVGE_HANDLER || type == Markup::SVGE_SCRIPT)
			{
				// don't take text childcontent of script and handler into account
				next_text_trav = static_cast<HTML_Element*>(next_text_trav->NextSibling());
				continue;
			}
		}

		// If it wasn't text, textlen will be 0
		for (unsigned i = 0; i < textlen; i++)
		{
			uni_char following_char = text[i];
			if (following_char == ' ' || following_char == '\n' ||
				following_char == '\r' || following_char == '\t')
			{
				// Whitespace, so we won't keep any of our whitespace
			}
			else
			{
				keep_trailing_whitespace = TRUE;
				break;
			}
		}

		if (owns_text)
		{
			OP_DELETEA((uni_char*)text);
		}

		next_text_trav = next_text_trav->Next();
	}
}
#endif // !FOLLOW_THE_SVG_1_1_SPEC_EVEN_WHEN_ITS_UNUSABLE

OP_STATUS SVGTraversalObject::PrepareTextBlock(SVGTextBlock& block,
											   SVGElementInfo& info, HTML_Element* text_content_elm)
{
	unsigned textlen = text_content_elm->GetTextContentLength();
	if (textlen == 0)
		return OpStatus::OK;

	RETURN_IF_ERROR(m_buffer.Expand(textlen + 1));
	uni_char* text = m_buffer.GetStorage();
	textlen = text_content_elm->GetTextContent(text, textlen+1);

	block.SetText(text, textlen);

	BOOL keep_leading_whitespace = FALSE;
	BOOL keep_trailing_whitespace = FALSE;

#ifndef FOLLOW_THE_SVG_1_1_SPEC_EVEN_WHEN_ITS_UNUSABLE
	if (!m_textinfo->packed.preserve_spaces)
	{
		CalculateSurroundingWhiteSpace(info.traversed, m_doc_ctx, keep_trailing_whitespace);

		keep_leading_whitespace = m_textinfo->packed.keep_leading_whitespace;
	}
#endif // !FOLLOW_THE_SVG_1_1_SPEC_EVEN_WHEN_ITS_UNUSABLE

	block.NormalizeSpaces(m_textinfo->packed.preserve_spaces,
						  keep_leading_whitespace, keep_trailing_whitespace);

	SVGTextCache* text_cache = info.context->GetTextCache();

#ifndef FOLLOW_THE_SVG_1_1_SPEC_EVEN_WHEN_ITS_UNUSABLE
	// If we kept a space, then here we assume that this
	// whitespace will be displayed, so we make sure that any
	// other whitespace before the next real text is collapsed
	// away.
	if (!block.IsEmpty())
		m_textinfo->packed.keep_leading_whitespace = !block.EndsWithWhitespace();
	// else we keep the old value

	text_cache->SetKeepLeadingWS(keep_leading_whitespace);
	text_cache->SetKeepTrailingWS(keep_trailing_whitespace);
#endif // !FOLLOW_THE_SVG_1_1_SPEC_EVEN_WHEN_ITS_UNUSABLE
	text_cache->SetTextContentElement(text_content_elm);
	return OpStatus::OK;
}

OP_STATUS SVGTextMeasurementObject::HandleTextNode(SVGElementInfo& info, HTML_Element* text_content_elm,
												   SVGBoundingBox* bbox)
{
	OP_ASSERT(bbox == NULL); // Only SVGLayoutObject and SVGBBoxUpdateObject calculates bboxes
	OP_ASSERT(m_textinfo);

	if (m_textinfo->data && m_textinfo->data->NeedsRenderMode())
		return SVGVisualTraversalObject::HandleTextNode(info, text_content_elm, bbox);

#ifdef SVG_SUPPORT_TEXTSELECTION
	m_textinfo->SetupTextSelection(m_doc_ctx, info.layouted);
#endif // SVG_SUPPORT_TEXTSELECTION

	SVGTextBlock block;
	RETURN_IF_ERROR(PrepareTextBlock(block, info, text_content_elm));

	OP_STATUS result = OpStatus::OK;
	if (!block.IsEmpty())
	{
		// SVGTextArguments::total_extents is only updated in extent
		// traversers, so should not change in the other (!measurement) case.
		SVGNumber saved_total_extent = m_textinfo->total_extent;
		m_textinfo->total_extent = 0;

		SVGFontDescriptor* fontdesc = &m_textinfo->font_desc;

		SVGDocumentContext* font_doc_ctx = info.context->IsShadowNode() ? AttrValueStore::GetSVGDocumentContext(info.layouted) : m_doc_ctx;
		SVGTextRenderer tr(m_canvas, font_doc_ctx, bbox != NULL);
		tr.SetFontDescriptor(fontdesc);

		const SVGValueContext& vcxt = GetValueContext();

		if (m_textarea_info)
		{
			SVGTextAreaMeasurementTraverser traverser(vcxt, m_doc_ctx, &tr);

			result = traverser.Traverse(block, info, *m_textinfo, AllowRasterFonts());
		}
		else
		{
			SVGTextExtentTraverser traverser(vcxt, m_doc_ctx, &tr);

			result = traverser.Traverse(block, info, *m_textinfo, AllowRasterFonts());
		}

		m_textinfo->total_extent += saved_total_extent;
	}
	return result;
}

OP_STATUS SVGVisualTraversalObject::HandleTextNode(SVGElementInfo& info, HTML_Element* text_content_elm,
												   SVGBoundingBox* bbox)
{
	OP_ASSERT(m_textinfo);

#ifdef SVG_SUPPORT_TEXTSELECTION
	m_textinfo->SetupTextSelection(m_doc_ctx, info.layouted);
#endif // SVG_SUPPORT_TEXTSELECTION

	SVGTextBlock block;
	RETURN_IF_ERROR(PrepareTextBlock(block, info, text_content_elm));

	OP_STATUS result = OpStatus::OK;
	if (!block.IsEmpty())
	{
		SVGFontDescriptor* fontdesc = &m_textinfo->font_desc;

		SVGDocumentContext* font_doc_ctx = info.context->IsShadowNode() ? AttrValueStore::GetSVGDocumentContext(info.layouted) : m_doc_ctx;
		SVGTextRenderer tr(m_canvas, font_doc_ctx, bbox != NULL);
		tr.SetFontDescriptor(fontdesc);

		const SVGValueContext& vcxt = GetValueContext();

		if (m_textarea_info)
		{
			SVGTextAreaPaintTraverser traverser(vcxt, m_doc_ctx, &tr);

			result = traverser.Traverse(block, info, *m_textinfo, AllowRasterFonts());
		}
		else
		{
			SVGTextPaintTraverser traverser(vcxt, m_doc_ctx, &tr);

			result = traverser.Traverse(block, info, *m_textinfo, AllowRasterFonts());
		}

		if (bbox)
			bbox->UnionWith(m_textinfo->bbox.Get());
	}

#ifdef SVG_SUPPORT_EDITABLE
	CheckCaret(info);
#endif // SVG_SUPPORT_EDITABLE

	return result;
}

OP_STATUS SVGLayoutObject::HandleTextNode(SVGElementInfo& info, HTML_Element* text_content_elm,
										  SVGBoundingBox* bbox)
{
	info.paint_node->UpdateState(m_canvas);

	RETURN_IF_ERROR(SVGVisualTraversalObject::HandleTextNode(info, text_content_elm, bbox));

	if (!m_canvas->AllowDraw(IgnorePointerEvents()))
		info.paint_node->SetVisible(FALSE);

	if (info.layouted->IsMatchingType(Markup::SVGE_TREF, NS_SVG))
	{
		OP_ASSERT(info.paint_node == m_current_container);

		SVGTextReferencePaintNode* tref_pn = static_cast<SVGTextReferencePaintNode*>(info.paint_node);
		m_current_container->Insert(tref_pn->GetContentNode(), m_current_pred);
	}
	else
	{
		m_current_container->Insert(info.paint_node, m_current_pred);
		m_current_pred = info.paint_node;
	}
	return OpStatus::OK;
}

#ifdef SVG_SUPPORT_EDITABLE
void SVGVisualTraversalObject::CheckCaret(SVGElementInfo& info)
{
	OP_ASSERT(m_textinfo);

	// If this text is editable, and the editable code claims that
	// the caret should be on this element, but we have not yet
	// painted the caret, then produce a caret now.
	if (!m_textinfo->editable ||
		m_textinfo->SelectionActive() ||
		m_textinfo->packed.has_painted_caret ||
		m_textinfo->editable->m_caret.m_point.elm != info.traversed)
		return;

	const HTMLayoutProperties& props = *info.props->GetProps();

	SVGEditable* editable = m_textinfo->editable;
	SVGNumber fontheight = props.svg->GetFontSize(info.layouted);
	editable->m_caret.m_pos.x = m_textinfo->ctp.x;
	editable->m_caret.m_pos.y = m_textinfo->ctp.y;
	editable->m_caret.m_pos.height = -fontheight;

	BOOL should_draw = TRUE;
	if (m_textarea_info)
	{
		SVGNumber lineheight = fontheight;

		if (SVGLineInfo* li = m_textarea_info->lineinfo->Get(m_textarea_info->current_line))
			lineheight = li->height;

		// If caret is in a textArea, and not visible (out of
		// bound) - don't draw it
		if (m_textinfo->linepos.y > m_textarea_info->vis_area.y + m_textarea_info->vis_area.height ||
			m_textinfo->linepos.y < m_textarea_info->vis_area.y + lineheight)
			should_draw = FALSE;
	}

	editable->m_caret.m_glyph_pos = editable->m_caret.m_pos;

	if (should_draw)
		editable->Paint(m_canvas);

	m_textinfo->packed.has_painted_caret = 1;

	// 		OP_ASSERT(!m_textarea_info ||
	// 				  (m_textarea_info->lineinfo &&
	// 				   m_textarea_info->current_line < m_textarea_info->lineinfo->GetCount()));
	editable->m_caret.m_line = m_textarea_info ? m_textarea_info->current_line : 0;
}
#endif // SVG_SUPPORT_EDITABLE

OP_STATUS SVGTraversalObject::PushSVGViewport(SVGElementInfo& info, const ViewportInfo& vpinfo)
{
	SVGNumberPair vp(vpinfo.actual_viewport.width, vpinfo.actual_viewport.height);

	RETURN_IF_ERROR(PushViewport(vp));
	info.Set(RESOURCE_VIEWPORT);
	return OpStatus::OK;
}

OP_STATUS SVGVisualTraversalObject::PushSVGViewport(SVGElementInfo& info, const ViewportInfo& vpinfo)
{
#ifdef SVG_SUPPORT_STENCIL
	ClipViewport(info, vpinfo);
#endif // SVG_SUPPORT_STENCIL

	return SVGTraversalObject::PushSVGViewport(info, vpinfo);
}

OP_STATUS SVGLayoutObject::PushSVGViewport(SVGElementInfo& info, const ViewportInfo& vpinfo)
{
	SVGViewportCompositePaintNode* viewport_paint_node =
		static_cast<SVGViewportCompositePaintNode*>(info.paint_node);

	SVGNumber vpscale(1);
	BOOL is_topmost_svg_root = (info.layouted == m_doc_ctx->GetSVGRoot());
	// Only apply the base scale to the actual SVG root, and not to
	// inner/sub SVGs and external resources (<image>, <animation>).
	if (is_topmost_svg_root && !m_doc_ctx->IsExternal())
		vpscale = m_canvas->GetBaseScale();

	viewport_paint_node->SetScale(vpscale);
	viewport_paint_node->SetUserTransform(vpinfo.user_transform);
	viewport_paint_node->SetClipRect(vpinfo.viewport);

	UINT32 viewport_fillcolor = 0;
	UINT8 viewport_fillopacity = 0;

	const SvgProperties *svg_props = info.props->GetProps()->svg;
	if (svg_props->viewportfill.GetColorType() != SVGColor::SVGCOLOR_NONE)
	{
		viewport_fillcolor = svg_props->viewportfill.GetRGBColor();
		viewport_fillopacity = svg_props->viewportfillopacity;
	}

	viewport_paint_node->SetFillColor(viewport_fillcolor);
	viewport_paint_node->SetFillOpacity(viewport_fillopacity);

#ifdef SVG_DOM
	if (info.traversed->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
	{
		if (SVGDocumentContext *viewport_doc_ctx = AttrValueStore::GetSVGDocumentContext(info.traversed))
			viewport_doc_ctx->UpdateViewport(vpinfo.viewport);
	}
#endif // SVG_DOM

	// FIXME: Need to figure out a way for a dirty rect potentially
	// stemming from the viewport-fill to make its way to the
	// screenbox for the element (this all happens before
	// EnterContainer, where this usually takes place)
	m_canvas->ResetDirtyRect();

	// FIXME/NOTE: We shouldn't fill the viewport if 'display: none'. (?)
	// Also, this may be an incorrect place to do it for the 'svg'
	// element (although since the 'svg' element can't have opacity
	// in 1.2T it isn't really specified - right? =)).
	// One might also want to consider the effects of the 'clip' property (aren't we already?).
	if (info.GetInvalidState() > INVALID_SUBTREE) // FIXME: Only do this during layout
		OpStatus::Ignore(FillViewport(info, vpinfo.viewport));

	OP_STATUS status = SVGVisualTraversalObject::PushSVGViewport(info, vpinfo);

	// If the viewport was filled, we need to add that area to the
	// elements screen-box
	UpdateElement(info);

	return status;
}

OP_STATUS SVGTraversalObject::PopSVGViewport(SVGElementInfo& info)
{
	if (info.IsSet(RESOURCE_VIEWPORT))
	{
		PopViewport();
		info.Clear(RESOURCE_VIEWPORT);
	}
	return OpStatus::OK;
}

#ifdef SVG_SUPPORT_MEDIA
/* virtual */
OP_STATUS SVGVisualTraversalObject::HandleVideo(SVGElementInfo& info, BOOL draw, SVGBoundingBox* bbox)
{
	HTML_Element* layouted_elm = info.layouted;

	// Get the binding, but do nothing if there isn't one.
	SVGMediaManager* mm = g_svg_manager_impl->GetMediaManager();
	SVGMediaBinding* mbind = mm->GetBinding(layouted_elm);
	if (!mbind && !bbox)
		return OpStatus::OK;

	// aspect ratio-adjusted size of video, which may be
	// different from the bitmap.
	UINT32 video_width = 0, video_height = 0;
	BOOL has_video = FALSE;

	if (mbind)
	{
		mbind->GetPlayer()->GetVideoSize(video_width, video_height);
		has_video = video_width > 0 && video_height > 0;
	}

#if 0
	SVGOverlayType overlay = (SVGOverlayType)AttrValueStore::GetEnumValue(layouted_elm,
																		  Markup::SVGA_OVERLAY,
																		  SVGENUM_OVERLAY,
																		  SVGOVERLAY_NONE);

	// "We don't go upstairs - Not since the accident..."
	if (overlay == SVGOVERLAY_TOP)
	{
		// The drawing should be set aside for later
		// How should this work with 'geometric' videos? Quirky...
	}
#endif

	OP_STATUS status = OpStatus::OK;

	SVGTransformBehavior transform_behavior;
	transform_behavior = (SVGTransformBehavior)AttrValueStore::GetEnumValue(layouted_elm,
																			Markup::SVGA_TRANSFORMBEHAVIOR,
																			SVGENUM_TRANSFORMBEHAVIOR,
																			SVGTRANSFORMBEHAVIOR_GEOMETRIC);

	const SVGValueContext& vcxt = GetValueContext();

	if (transform_behavior == SVGTRANSFORMBEHAVIOR_GEOMETRIC)
	{
		// The video isn't pinned, so do all this viewport-setting-uping.
		// The viewport is defined by (x, y, width, height),
		// but the "viewBox" is defined by the (potential) size of the video

		SVGRect video_vp;
		RETURN_IF_ERROR(SVGUtils::GetImageValues(layouted_elm, vcxt, video_vp));

		if (bbox)
		{
			bbox->minx = video_vp.x;
			bbox->miny = video_vp.y;
			bbox->maxx = video_vp.x + SVGNumber::max_of(video_vp.width, 0);
			bbox->maxy = video_vp.y + SVGNumber::max_of(video_vp.height, 0);
		}

		if (draw && has_video && video_vp.width > 0 && video_vp.height > 0)
		{
			OpStatus::Ignore(FillViewport(info, video_vp));

			SVGAspectRatio* ratio = NULL;
			AttrValueStore::GetPreserveAspectRatio(layouted_elm, ratio);

			status = m_canvas->SaveState();
			if (OpStatus::IsSuccess(status))
			{
				// viewBox
				SVGRect video_vb(0, 0, video_width, video_height);

				SVGMatrix viewboxtransform;
				SVGRect video_rect; // cliprect
				RETURN_IF_ERROR(SVGUtils::GetViewboxTransform(video_vp, &video_vb, ratio, viewboxtransform, video_rect));

#ifdef SVG_SUPPORT_STENCIL
				SVGContentClipper clipper;
				const HTMLayoutProperties& props = *info.props->GetProps();
				clipper.Begin(m_canvas, video_vp, props, TRUE);
#endif // SVG_SUPPORT_STENCIL

				m_canvas->ConcatTransform(viewboxtransform);

				status = DrawVideo(info, mbind, video_rect);

				m_canvas->RestoreState();
			}
		}
	}
	else
	{
		// Pinned basically means that we only care about the position (x, y),
		// and then let the video determine extents, i.e. something like:
		// (x', y') = CTM * (x, y)
		// Extents = (x' - frame_w/2, y' - frame_h/2, frame_w, frame_h)
		SVGNumberPair pinned_pos;

		SVGLengthObject* x;
		AttrValueStore::GetLength(layouted_elm, Markup::SVGA_X, &x);
		if (x)
			pinned_pos.x = SVGUtils::ResolveLength(x->GetLength(), SVGLength::SVGLENGTH_X, vcxt);

		SVGLengthObject* y;
		AttrValueStore::GetLength(layouted_elm, Markup::SVGA_Y, &y);
		if (y)
			pinned_pos.y = SVGUtils::ResolveLength(y->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

		if (bbox)
		{
			bbox->minx = pinned_pos.x;
			bbox->maxx = pinned_pos.x;
			bbox->miny = pinned_pos.y;
			bbox->maxy = pinned_pos.y;
		}

		if (has_video)
		{
			if (bbox)
			{
				SVGNumber us_width = SVGNumber(video_width) / m_canvas->GetTransform()[0];
				SVGNumber us_height = SVGNumber(video_height) / m_canvas->GetTransform()[3];

				bbox->minx -= us_width / 2;
				bbox->miny -= us_height / 2;
				bbox->maxx = bbox->minx + us_width;
				bbox->maxy = bbox->miny + us_height;
			}

			// Draw pinned
			if (draw)
			{
				pinned_pos = m_canvas->GetTransform().ApplyToCoordinate(pinned_pos);
				status = m_canvas->SaveState();
				if (OpStatus::IsSuccess(status))
				{
					m_canvas->GetTransform().LoadIdentity();

					pinned_pos.x = pinned_pos.x.GetIntegerValue();
					pinned_pos.y = pinned_pos.y.GetIntegerValue();

					m_canvas->GetTransform().PostMultTranslation(pinned_pos.x, pinned_pos.y);

					SVGNumber additional_angle;
					switch (transform_behavior)
					{
					case SVGTRANSFORMBEHAVIOR_PINNED90:
						additional_angle = 90;
						break;
					case SVGTRANSFORMBEHAVIOR_PINNED180:
						additional_angle = 180;
						break;
					case SVGTRANSFORMBEHAVIOR_PINNED270:
						additional_angle = 270;
						break;
					}

					if (additional_angle > 0)
					{
						SVGMatrix rot;

						rot.LoadRotation(additional_angle);
						m_canvas->ConcatTransform(rot);
					}

					SVGRect dst_rect(-(int)(video_width / 2),
									 -(int)(video_height / 2),
									 video_width,
									 video_height);

					status = DrawVideo(info, mbind, dst_rect);

					m_canvas->RestoreState();
				}
			}
		}
	}
	return status;
}

/* virtual */
OP_STATUS SVGLayoutObject::HandleVideo(SVGElementInfo& info, BOOL draw, SVGBoundingBox* bbox)
{
	SVGMediaManager* mm = g_svg_manager_impl->GetMediaManager();
	SVGMediaBinding* mbind = mm->GetBinding(info.layouted);
#ifdef PI_VIDEO_LAYER
	if (mbind)
		mbind->DisableOverlay();
#endif // PI_VIDEO_LAYER

	SVGPaintNode* paint_node = info.paint_node;

	// Do nothing if we don't have a binding.
	if (!mbind)
	{
		paint_node->Detach();

		if (!bbox)
			return OpStatus::OK;
	}

	HTML_Element* layouted_elm = info.layouted;

#if 0
	SVGOverlayType overlay = (SVGOverlayType)AttrValueStore::GetEnumValue(layouted_elm,
																		  Markup::SVGA_OVERLAY,
																		  SVGENUM_OVERLAY,
																		  SVGOVERLAY_NONE);

	// "We don't go upstairs - Not since the accident..."
	if (overlay == SVGOVERLAY_TOP)
	{
		// The drawing should be set aside for later
		// How should this work with 'geometric' videos? Quirky...
	}
#endif

	OP_STATUS status = OpStatus::OK;

	SVGTransformBehavior transform_behavior;
	transform_behavior = (SVGTransformBehavior)AttrValueStore::GetEnumValue(layouted_elm,
																			Markup::SVGA_TRANSFORMBEHAVIOR,
																			SVGENUM_TRANSFORMBEHAVIOR,
																			SVGTRANSFORMBEHAVIOR_GEOMETRIC);

	const SVGValueContext& vcxt = GetValueContext();

	SVGVideoNode* video_paint_node = static_cast<SVGVideoNode*>(paint_node);
	video_paint_node->SetBinding(mbind);
#ifdef PI_VIDEO_LAYER
	// Overlay only allowed on non-instanced nodes
	video_paint_node->SetAllowOverlay(info.layouted == info.traversed);
#endif // PI_VIDEO_LAYER

	const HTMLayoutProperties& props = *info.props->GetProps();
	const SvgProperties *svg_props = props.svg;

	SVGPainter::ImageRenderQuality quality =
		(SVGPainter::ImageRenderQuality)SVGGetImageQualityFromProps(props);

	video_paint_node->SetVideoQuality(quality);

	if (transform_behavior == SVGTRANSFORMBEHAVIOR_GEOMETRIC)
	{
		// The video isn't pinned, so do all this viewport-setting-uping.
		// The viewport is defined by (x, y, width, height),
		// but the "viewBox" is defined by the (potential) size of the video.

		SVGRect video_vp;
		RETURN_IF_ERROR(SVGUtils::GetImageValues(layouted_elm, vcxt, video_vp));

		if (bbox)
		{
			bbox->minx = video_vp.x;
			bbox->miny = video_vp.y;
			bbox->maxx = video_vp.x + SVGNumber::max_of(video_vp.width, 0);
			bbox->maxy = video_vp.y + SVGNumber::max_of(video_vp.height, 0);
		}

		UINT32 viewport_fillcolor = 0;
		UINT8 viewport_fillopacity = 0;

		if (svg_props->viewportfill.GetColorType() != SVGColor::SVGCOLOR_NONE)
		{
			viewport_fillcolor = svg_props->viewportfill.GetRGBColor();
			viewport_fillopacity = svg_props->viewportfillopacity;
		}

		video_paint_node->SetBackgroundColor(viewport_fillcolor);
		video_paint_node->SetBackgroundOpacity(viewport_fillopacity);

		SVGAspectRatio* ratio = NULL;
		AttrValueStore::GetPreserveAspectRatio(layouted_elm, ratio);

		video_paint_node->SetVideoAspect(ratio);
		video_paint_node->SetVideoViewport(video_vp);

		video_paint_node->SetIsPinned(FALSE);

		if (draw && video_vp.width > 0 && video_vp.height > 0)
			// FIXME: Temporary cludge for extents
			OpStatus::Ignore(m_canvas->DrawRect(video_vp.x, video_vp.y, video_vp.width, video_vp.height, 0, 0));
	}
	else
	{
		// Aspect ratio-adjusted size of video, which may be
		// different from the bitmap.
		unsigned video_width = 0, video_height = 0;
		BOOL has_video = FALSE;

		if (mbind)
		{
			mbind->GetPlayer()->GetVideoSize(video_width, video_height);
			has_video = video_width > 0 && video_height > 0;
		}

		// Pinned basically means that we only care about the position (x, y),
		// and then let the video determine extents, i.e. something like:
		// (x', y') = CTM * (x, y)
		// Extents = (x' - frame_w/2, y' - frame_h/2, frame_w, frame_h)
		SVGNumberPair pinned_pos;

		SVGLengthObject* x;
		AttrValueStore::GetLength(layouted_elm, Markup::SVGA_X, &x);
		if (x)
			pinned_pos.x = SVGUtils::ResolveLength(x->GetLength(), SVGLength::SVGLENGTH_X, vcxt);

		SVGLengthObject* y;
		AttrValueStore::GetLength(layouted_elm, Markup::SVGA_Y, &y);
		if (y)
			pinned_pos.y = SVGUtils::ResolveLength(y->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

		if (bbox)
		{
			bbox->minx = pinned_pos.x;
			bbox->maxx = pinned_pos.x;
			bbox->miny = pinned_pos.y;
			bbox->maxy = pinned_pos.y;
		}

		if (has_video)
		{
			SVGBoundingBox us_extents;
			SVGNumber us_width = SVGNumber(video_width) / m_canvas->GetTransform()[0];
			SVGNumber us_height = SVGNumber(video_height) / m_canvas->GetTransform()[3];

			// Calculate the extents in userspace
			us_extents.minx = pinned_pos.x - us_width / 2;
			us_extents.miny = pinned_pos.y - us_height / 2;
			us_extents.maxx = us_extents.minx + us_width;
			us_extents.maxy = us_extents.miny + us_height;

			if (bbox)
				*bbox = us_extents;

			// Draw pinned
			if (draw)
			{
				video_paint_node->SetBackgroundColor(0);
				video_paint_node->SetBackgroundOpacity(0);

				video_paint_node->SetPinnedExtents(us_extents);
				video_paint_node->SetVideoAspect(NULL);

				SVGRect pinned_vp(pinned_pos.x, pinned_pos.y, 0, 0);
				video_paint_node->SetVideoViewport(pinned_vp);

				unsigned pinned_rot90 = 0;
				switch (transform_behavior)
				{
				case SVGTRANSFORMBEHAVIOR_PINNED90:
					pinned_rot90 = 1; // 1 * 90
					break;
				case SVGTRANSFORMBEHAVIOR_PINNED180:
					pinned_rot90 = 2; // 2 * 90
					break;
				case SVGTRANSFORMBEHAVIOR_PINNED270:
					pinned_rot90 = 3; // 3 * 90
					break;
				}

				video_paint_node->SetPinnedRotation(pinned_rot90);
				video_paint_node->SetIsPinned(TRUE);

				// FIXME: Temporary cludge for extents
				pinned_pos = m_canvas->GetTransform().ApplyToCoordinate(pinned_pos);
				status = m_canvas->SaveState();
				if (OpStatus::IsSuccess(status))
				{
					m_canvas->GetTransform().LoadIdentity();

					pinned_pos.x = pinned_pos.x.GetIntegerValue();
					pinned_pos.y = pinned_pos.y.GetIntegerValue();

					m_canvas->GetTransform().PostMultTranslation(pinned_pos.x, pinned_pos.y);

					SVGNumber additional_angle;
					if (pinned_rot90 > 0)
					{
						SVGMatrix rot;
						rot.LoadRotation(pinned_rot90 * 90);

						m_canvas->ConcatTransform(rot);
					}

					SVGRect dst_rect(-(int)(video_width / 2),
									 -(int)(video_height / 2),
									 video_width,
									 video_height);

					OpStatus::Ignore(m_canvas->DrawRect(dst_rect.x, dst_rect.y, dst_rect.width, dst_rect.height, 0, 0));

					m_canvas->RestoreState();
				}
			}
		}
	}
	return status;
}

/* virtual */
OP_STATUS SVGVisualTraversalObject::DrawVideo(SVGElementInfo& info, SVGMediaBinding* mbind, const SVGRect& dst_rect)
{
	OpStatus::Ignore(m_canvas->DrawRect(dst_rect.x, dst_rect.y, dst_rect.width, dst_rect.height, 0, 0));
	return OpStatus::OK;
}
#endif // SVG_SUPPORT_MEDIA

#ifdef SVG_SUPPORT_FOREIGNOBJECT
OP_STATUS SVGIntersectionObject::HandleForeignObject(SVGElementInfo& info, const SVGRect& img_vp, int quality)
{
	BOOL has_link = AttrValueStore::HasObject(info.layouted, Markup::XLINKA_HREF, NS_IDX_XLINK);

	const int fx = img_vp.x.GetIntegerValue();
	const int fy = img_vp.y.GetIntegerValue();
	const int fw = img_vp.width.GetIntegerValue();
	const int fh = img_vp.height.GetIntegerValue();
	FramesDocument* frm_doc = m_doc_ctx->GetDocument();
	FramesDocElm* frame = FramesDocElm::GetFrmDocElmByHTML(info.layouted);

	if (has_link)
	{
		// Don't do circular references
		URL* url = NULL;
		URL doc_url = m_doc_ctx->GetURL();
		RETURN_IF_ERROR(AttrValueStore::GetXLinkHREF(doc_url, info.layouted, url));
		if (url && !(doc_url == *url))
		{
			if (!frame)
			{
				BOOL load_frame = TRUE;
				OP_STATUS status = frm_doc->GetNewIFrame(frame, fw, fh, info.layouted, NULL, load_frame, m_canvas->GetVisibility() == SVGVISIBILITY_VISIBLE);
				if (OpStatus::IsError(status))
					frame = NULL;

				if (frame)
					frame->SetNotifyParentOnContentChange(TRUE);
			}
		}
	}

	if (frame && (frame->GetWidth() != fw || frame->GetHeight() != fh))
	{
		VisualDevice* frame_vd = frame->GetVisualDevice();

		// FIXME: too many conversions between screen and document coordinates here
		// FIXME: Plug in the real CTM?
		AffinePos frame_pos(frame_vd->ScaleToDoc(fx), frame_vd->ScaleToDoc(fy));
		frame->SetGeometry(frame_pos, frame_vd->ScaleToDoc(fw), frame_vd->ScaleToDoc(fh));
	}

	if (has_link)
	{
		return m_canvas->DrawImage((OpBitmap*)NULL, OpRect(), img_vp, SVGCanvas::IMAGE_QUALITY_NORMAL);
	}
	else
	{
		HTML_Element* child = info.layouted;
		if (child)
		{
			OpPoint i_pt = m_canvas->GetIntersectionPointInt();

			// This doesn't care about the x and y attributes on the
			// foreignObject element. Has problems with zooming the
			// document.
			g_svg_manager_impl->TransformToElementCoordinates(info.layouted, frm_doc, i_pt.x, i_pt.y);

			// This brings us close to the truth by moving the origin
			// to (x,y) of the foreignObject
			i_pt.x -= fx;
			i_pt.y -= fy;

#if 0
			// This version has problems when zooming and possibly scrolling the document.
			SVGMatrix matrix;
			SVGUtils::GetElementCTM(info.layouted, m_doc_ctx, &matrix, TRUE /* ScreenCTM */);
			matrix.PostMultTranslation(fx, fy);
			matrix.Invert();

			SVGNumberPair tfmed = matrix.ApplyToCoordinate(SVGNumberPair(i_pt.x, i_pt.y));
			i_pt.x = m_doc_ctx->GetVisualDevice()->ScaleToDoc(tfmed.x.GetIntegerValue());
			i_pt.y = m_doc_ctx->GetVisualDevice()->ScaleToDoc(tfmed.y.GetIntegerValue());
#endif

			IntersectionObject intersection(frm_doc, LayoutCoord(i_pt.x), LayoutCoord(i_pt.y));

			Head prop_list;
			HLDocProfile* hld_profile = m_doc_ctx->GetHLDocProfile();
			if (!hld_profile)
				return OpStatus::ERR;

			if (!LayoutProperties::CreateCascade(child, prop_list, LAYOUT_WORKPLACE(hld_profile)))
				return OpStatus::ERR_NO_MEMORY;

			// This is to work around an assert in layout, see bug 289828
			LayoutWorkplace* lwp = hld_profile->GetLayoutWorkplace();
			BOOL old_traversing = lwp->IsTraversing();
			lwp->SetIsTraversing(FALSE);
			intersection.Traverse(child, &prop_list, FALSE);
			lwp->SetIsTraversing(old_traversing);

			if (Box* inner_box = intersection.GetInnerBox())
				if (HTML_Element* intersected_elm = inner_box->GetHtmlElement())
				{
					m_canvas->SetCurrentElement(intersected_elm);
					m_canvas->SetLastIntersectedElement();
				}

			prop_list.Clear();
		}
	}
	return OpStatus::OK;
}

OP_STATUS SVGLayoutObject::HandleForeignObject(SVGElementInfo& info, const SVGRect& img_vp, int quality)
{
	FramesDocument* frm_doc = m_doc_ctx->GetDocument();

	SVGForeignObjectNode* foreign_object_node = static_cast<SVGForeignObjectNode*>(info.paint_node);
	foreign_object_node->SetContext(info.layouted, frm_doc, m_doc_ctx->GetVisualDevice());
	foreign_object_node->SetViewport(img_vp);
	foreign_object_node->SetQuality((SVGPainter::ImageRenderQuality)quality);

	if (AttrValueStore::HasObject(info.layouted, Markup::XLINKA_HREF, NS_IDX_XLINK))
	{
		const int fw = img_vp.width.GetIntegerValue();
		const int fh = img_vp.height.GetIntegerValue();

		FramesDocElm* frame = FramesDocElm::GetFrmDocElmByHTML(info.layouted);

		// Don't do circular references
		URL* url = NULL;
		URL doc_url = m_doc_ctx->GetURL();
		RETURN_IF_ERROR(AttrValueStore::GetXLinkHREF(doc_url, info.layouted, url));
		if (url && !(doc_url == *url))
		{
			if (!frame)
			{
				BOOL load_frame = TRUE;
				OP_STATUS status = frm_doc->GetNewIFrame(frame, fw, fh, info.layouted, NULL, load_frame, m_canvas->GetVisibility() == SVGVISIBILITY_VISIBLE);
				if (OpStatus::IsError(status))
					frame = NULL;

				if (frame)
					frame->SetNotifyParentOnContentChange(TRUE);
			}
		}

		if (frame && (frame->GetWidth() != fw || frame->GetHeight() != fh))
		{
			VisualDevice* frame_vd = frame->GetVisualDevice();

			const int fx = img_vp.x.GetIntegerValue();
			const int fy = img_vp.y.GetIntegerValue();

			// FIXME: too many conversions between screen and document coordinates here
			// FIXME: Plug in the real CTM?
			AffinePos frame_pos(frame_vd->ScaleToDoc(fx), frame_vd->ScaleToDoc(fy));
			frame->SetGeometry(frame_pos, frame_vd->ScaleToDoc(fw), frame_vd->ScaleToDoc(fh));
		}

		SVGForeignObjectHrefNode* fo_href_node = static_cast<SVGForeignObjectHrefNode*>(foreign_object_node);
		fo_href_node->SetFrame(frame);
	}

	return m_canvas->DrawImage((OpBitmap*)NULL, OpRect(), img_vp, SVGCanvas::IMAGE_QUALITY_NORMAL);
}
#endif // SVG_SUPPORT_FOREIGNOBJECT

#ifdef SVG_SUPPORT_SVG_IN_IMAGE
static void UpdateAnimationView(FramesDocument* frm_doc, HTML_Element* layouted_elm, const OpRect& fsize)
{
	FramesDocElm* frame = FramesDocElm::GetFrmDocElmByHTML(layouted_elm);
	if (!frame)
		return;

	VisualDevice* frame_vd = frame->GetVisualDevice();
	CoreView* container_view = frame_vd->GetContainerView();
	if (container_view)
		container_view->SetReference(NULL, NULL);

	if (SVG_DOCUMENT_CLASS* doc = frame->GetCurrentDoc())
		if (LogicalDocument* logdoc = doc->GetLogicalDocument())
			if (SVGWorkplaceImpl* workplace = static_cast<SVGWorkplaceImpl*>(logdoc->GetSVGWorkplace()))
				workplace->QueueReflow();

	frame_vd->SetScrollType(VisualDevice::VD_SCROLLING_NO);

	// FIXME: too many conversions between screen and document coordinates here
// 	if (frame && (frame->GetWidth() != frame_vd->ScaleToDoc(fsize.width) ||
// 				  frame->GetHeight() != frame_vd->ScaleToDoc(fsize.height)))
	// If we are painting the main document the SetGeometry call will trigger an assert in HTML_Element::MarkDirty
	if (!frm_doc->GetVisualDevice()->IsPainting())
	{
		// FIXME: Plug in the real CTM?
		AffinePos frame_pos(frame_vd->ScaleToDoc(fsize.x), frame_vd->ScaleToDoc(fsize.y));
		frame->SetGeometry(frame_pos, frame_vd->ScaleToDoc(fsize.width), frame_vd->ScaleToDoc(fsize.height));
	}
	frame->ShowFrames();

	frame_vd->GetContainerView()->SetReference(frm_doc, layouted_elm);
}

OP_STATUS SVGLayoutObject::HandleVectorImage(SVGElementInfo& info, const SVGRect& img_vp)
{
#ifdef SVG_SUPPORT_STENCIL
	SVGContentClipper clipper;
	const HTMLayoutProperties& props = *info.props->GetProps();
	clipper.Begin(m_canvas, img_vp, props, TRUE);
#endif // SVG_SUPPORT_STENCIL

	HTML_Element* root = SVGUtils::FindHrefReferredNode(m_resolver, m_doc_ctx, info.layouted);
	if (!root || root->GetNsType() != NS_SVG)
	{
		OpStatus::Ignore(m_canvas->DrawRect(img_vp.x, img_vp.y,
											img_vp.width, img_vp.height, 0, 0));
		return OpStatus::OK;
	}

	SVGDocumentContext* other_doc_ctx = AttrValueStore::GetSVGDocumentContext(root);
	if (!other_doc_ctx)
	{
		OpStatus::Ignore(m_canvas->DrawRect(img_vp.x, img_vp.y,
											img_vp.width, img_vp.height, 0, 0));
		return OpStatus::OK;
	}

	SVGVectorImageNode* image_paint_node = static_cast<SVGVectorImageNode*>(info.paint_node);

	RETURN_IF_ERROR(PushContainerState(info));

	if (m_current_container)
		m_current_container->Insert(image_paint_node, m_current_pred);

	m_current_container = image_paint_node;
	m_current_pred = NULL;

	// We'll always regenerate the entire subtree, so no need to clear
	// it.

	image_paint_node->SetImageContext(other_doc_ctx);
	//image_paint_node->SetImageAspect(ar);
	image_paint_node->SetImageViewport(img_vp);

	SVGElementResolverStack resolver(m_resolver);
	RETURN_IF_ERROR(resolver.Push(root));

	SVGCanvasStateStack c;
	RETURN_IF_ERROR(c.Save(m_canvas));

	// This is perhaps not needed in all cases, but fixes bug #275577
	other_doc_ctx->AddInvalidState(INVALID_ADDED);

	m_canvas->SetDefaults(other_doc_ctx->GetRenderingQuality());
	m_canvas->GetTransform().PostMultTranslation(img_vp.x, img_vp.y);

	Markup::Type type = info.layouted->Type();
	if (type == Markup::SVGE_ANIMATION)
	{
		other_doc_ctx->SetTransform(m_canvas->GetTransform());
	}

	other_doc_ctx->ForceSize(img_vp.width.GetIntegerValue(),
							 img_vp.height.GetIntegerValue());

	// It's very helpful to know later on that we were an
	// external resource, fixes
	// http://wwwi/~ed/svg/animtest.svg
	if (!other_doc_ctx->IsExternal())
	{
		if (type == Markup::SVGE_ANIMATION)
			other_doc_ctx->SetIsExternalAnimation(TRUE);
		else
			other_doc_ctx->SetIsExternalResource(TRUE);
	}

	// Remove and free all the old children.
	image_paint_node->FreeChildren();

	// This is just a straight translation of how it used to be
	// (GetLastPaintedRect) - I'd prefer to do it some other way, but
	// I'm not sure how at the moment.
	OpRect last_painted = other_doc_ctx->GetScreenExtents();

	SVGNumberPair vp(img_vp.width, img_vp.height);
	PushViewport(vp);

	SVGDocumentContext* saved_doc_ctx = GetDocumentContext();
	SetDocumentContext(other_doc_ctx);

	BOOL saved_detached_operation = GetDetachedOperation();
	SetDetachedOperation(TRUE);

	List<SVGPaintNode> saved_pending_nodes;
	SavePendingNodes(saved_pending_nodes);

	OP_STATUS trav_result = SVGTraverser::Traverse(this, root, NULL);

	RestorePendingNodes(saved_pending_nodes);

	SetDetachedOperation(saved_detached_operation);

	SetDocumentContext(saved_doc_ctx);

	PopViewport();

	RETURN_IF_ERROR(trav_result);

	const OpRect sub_image_box = other_doc_ctx->GetScreenExtents();
	m_canvas->AddToDirtyRect(sub_image_box);

	if (type == Markup::SVGE_ANIMATION)
	{
		if (!last_painted.Equals(sub_image_box))
			UpdateAnimationView(m_doc_ctx->GetDocument(), info.layouted, sub_image_box);
	}

	c.Restore();

	m_current_container->UpdateState(m_canvas);
	PopContainerState(info);

	return OpStatus::OK;
}

OP_STATUS SVGIntersectionObject::HandleVectorImage(SVGElementInfo& info, const SVGRect& img_vp)
{
#ifdef SVG_SUPPORT_STENCIL
	SVGContentClipper clipper;
	const HTMLayoutProperties& props = *info.props->GetProps();
	clipper.Begin(m_canvas, img_vp, props, TRUE);
#endif // SVG_SUPPORT_STENCIL

	// Never cross the document boundary in intersection mode
	OpStatus::Ignore(m_canvas->DrawRect(img_vp.x, img_vp.y, img_vp.width, img_vp.height, 0, 0));
	return OpStatus::OK;
}
#endif // SVG_SUPPORT_SVG_IN_IMAGE

OP_STATUS SVGVisualTraversalObject::HandleGraphicsElement(SVGElementInfo& info)
{
	BOOL update_bbox = CalculateBBox();

	m_canvas->SetCurrentElement(info.traversed);

	BOOL allow_draw = m_canvas->AllowDraw(IgnorePointerEvents());

	//
	// Here is brief description of the flags used below and what they signify:
	//
	// allow_draw: Determined by 'visibility' and 'pointer-events' -
	//             iff we care about 'pointer-events' (which we don't do
	//             when drawing)
	//
	// update_bbox: The bounding box of this element should be updated.
	//              This flag does _not_ depend on the allow_draw flag,
	//              i.e. even if the geometry is invisible the bbox
	//              should be calculated

	HTML_Element* layouted_elm = info.layouted;
	Markup::Type type = info.layouted->Type();

	const SVGValueContext& vcxt = GetValueContext();

	OP_STATUS result = OpStatus::OK;

	SVGBoundingBox bbox;
	switch (type)
	{
	case Markup::SVGE_ELLIPSE:
		{
			SVGLengthObject* ccx = NULL;
			SVGLengthObject* ccy = NULL;
			SVGLengthObject* rx_length = NULL;
			SVGLengthObject* ry_length = NULL;

			AttrValueStore::GetLength(layouted_elm, Markup::SVGA_RX, &rx_length, NULL);
			AttrValueStore::GetLength(layouted_elm, Markup::SVGA_RY, &ry_length, NULL);
			AttrValueStore::GetLength(layouted_elm, Markup::SVGA_CX, &ccx, NULL);
			AttrValueStore::GetLength(layouted_elm, Markup::SVGA_CY, &ccy, NULL);

			SVGNumber cx = 0;
			if (ccx)
				cx = SVGUtils::ResolveLength(ccx->GetLength(), SVGLength::SVGLENGTH_X, vcxt);

			SVGNumber cy = 0;
			if (ccy)
				cy = SVGUtils::ResolveLength(ccy->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

			SVGNumber rx = 0;
			if (rx_length)
				rx = SVGUtils::ResolveLength(rx_length->GetLength(), SVGLength::SVGLENGTH_X, vcxt);

			SVGNumber ry = 0;
			if (ry_length)
				ry = SVGUtils::ResolveLength(ry_length->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

			if (update_bbox)
			{
				SVGNumber norm_rx = SVGNumber::max_of(rx, 0);
				SVGNumber norm_ry = SVGNumber::max_of(ry, 0);
				bbox.minx = cx - norm_rx;
				bbox.miny = cy - norm_ry;
				bbox.maxx = cx + norm_rx;
				bbox.maxy = cy + norm_ry;
			}

			// If rx or ry equals 0 then don't render the ellipse
			if (rx > 0 && ry > 0)
			{
				if (allow_draw)
				{
					result = m_canvas->DrawEllipse(cx, cy, rx, ry);
				}
			}
		}
		break;

	case Markup::SVGE_LINE:
		{
			SVGLengthObject* crdx1 = NULL;
			SVGLengthObject* crdy1 = NULL;
			SVGLengthObject* crdx2 = NULL;
			SVGLengthObject* crdy2 = NULL;

			RETURN_IF_ERROR(SVGUtils::GetLineValues(layouted_elm, &crdx1, &crdy1,
													&crdx2, &crdy2));

			SVGNumber x1 = 0;
			if (crdx1)
				x1 = SVGUtils::ResolveLength(crdx1->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
			SVGNumber y1 = 0;
			if (crdy1)
				y1 = SVGUtils::ResolveLength(crdy1->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);
			SVGNumber x2 = 0;
			if (crdx2)
				x2 = SVGUtils::ResolveLength(crdx2->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
			SVGNumber y2 = 0;
			if (crdy2)
				y2 = SVGUtils::ResolveLength(crdy2->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

			if (update_bbox)
			{
				bbox.minx = SVGNumber::min_of(x1, x2);
				bbox.miny = SVGNumber::min_of(y1, y2);
				bbox.maxx = SVGNumber::max_of(x1, x2);
				bbox.maxy = SVGNumber::max_of(y1, y2);
			}

			if (allow_draw)
				result = m_canvas->DrawLine(x1, y1, x2, y2);
		}
		break;

	case Markup::SVGE_PATH:
		{
			if (!allow_draw && !update_bbox)
				break;

			SVGNumber plen;
			RETURN_IF_ERROR(AttrValueStore::GetNumber(layouted_elm, Markup::SVGA_PATHLENGTH, plen, -1));

			OpBpath *oppath = NULL;
			result = AttrValueStore::GetPath(layouted_elm, Markup::SVGA_D, &oppath);

			// Render the path until we encountered an error (if any)
			if (!oppath)
				break;

			if (update_bbox)
				bbox = oppath->GetBoundingBox();

			if (allow_draw)
			{
				OP_STATUS draw_res = m_canvas->DrawPath(oppath, plen);

				if (OpStatus::IsError(result))
					break;

				result = draw_res;
			}
		}
		break;

	case Markup::SVGE_POLYGON:
	case Markup::SVGE_POLYLINE:
		{
			if (!allow_draw && !update_bbox)
				break;

			SVGVector* list = NULL;
			if (type == Markup::SVGE_POLYLINE)
				SVGUtils::GetPolylineValues(layouted_elm, list);
			else
				SVGUtils::GetPolygonValues(layouted_elm, list);

			if (!list || list->VectorType() != SVGOBJECT_POINT || list->GetCount() == 0)
				break;

			if (update_bbox)
			{
				for (UINT32 i = 0; i < list->GetCount(); i++)
				{
					SVGPoint* c = static_cast<SVGPoint*>(list->Get(i));
					if (!c)
						return OpStatus::ERR;

					bbox.minx = SVGNumber::min_of(bbox.minx, c->x);
					bbox.miny = SVGNumber::min_of(bbox.miny, c->y);
					bbox.maxx = SVGNumber::max_of(bbox.maxx, c->x);
					bbox.maxy = SVGNumber::max_of(bbox.maxy, c->y);
				}
			}

			// Render the polygon until we encountered an error (if any)
			if (allow_draw)
			{
				OP_STATUS draw_res = m_canvas->DrawPolygon(*list, type == Markup::SVGE_POLYGON);

				if (OpStatus::IsError(result))
				{
					OP_ASSERT(result != OpStatus::ERR);
					break;
				}

				result = draw_res;
			}
		}
		break;

	case Markup::SVGE_CIRCLE:
		{
			SVGLengthObject* crdcx = NULL;
			SVGLengthObject* crdcy = NULL;
			SVGLengthObject* r_length = NULL;

			RETURN_IF_ERROR(SVGUtils::GetCircleValues(layouted_elm, &crdcx, &crdcy, &r_length));

			SVGNumber cx = 0;
			if (crdcx)
				cx = SVGUtils::ResolveLength(crdcx->GetLength(), SVGLength::SVGLENGTH_X, vcxt);

			SVGNumber cy = 0;
			if (crdcy)
				cy = SVGUtils::ResolveLength(crdcy->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

			SVGNumber r = 0;
			if (r_length)
				r = SVGUtils::ResolveLength(r_length->GetLength(), SVGLength::SVGLENGTH_OTHER, vcxt);

			if (update_bbox)
			{
				SVGNumber norm_r = SVGNumber::max_of(r, 0);
				bbox.minx = cx - norm_r;
				bbox.miny = cy - norm_r;
				bbox.maxx = cx + norm_r;
				bbox.maxy = cy + norm_r;
			}

			// If r = 0 then don't render the circle
			if (r > 0)
			{
				if (allow_draw)
					result = m_canvas->DrawEllipse(cx, cy, r, r);
			}
		}
		break;

	case Markup::SVGE_RECT:
		{
			SVGLengthObject *wl = NULL;
			SVGLengthObject *hl = NULL;
			SVGLengthObject *rxl = NULL;
			SVGLengthObject *ryl = NULL;
			SVGLengthObject* crdx = NULL;
			SVGLengthObject *crdy = NULL;
			RETURN_IF_ERROR(SVGUtils::GetRectValues(layouted_elm, &crdx, &crdy, &wl, &hl,
													&rxl, &ryl));

			SVGNumber x = 0;
			if (crdx)
				x = SVGUtils::ResolveLength(crdx->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
			SVGNumber y = 0;
			if (crdy)
				y = SVGUtils::ResolveLength(crdy->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

			SVGNumber w;
			if (wl)
				w = SVGUtils::ResolveLength(wl->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
			SVGNumber h;
			if (hl)
				h = SVGUtils::ResolveLength(hl->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);
			SVGNumber rx;
			if (rxl)
				rx = SVGUtils::ResolveLength(rxl->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
			SVGNumber ry;
			if (ryl)
				ry = SVGUtils::ResolveLength(ryl->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

			if (ryl && !rxl)
			{
				rx = ry;
			}
			else if (rxl && !ryl)
			{
				ry = rx;
			}

			SVGUtils::AdjustRectValues(w, h, rx, ry);

			if (update_bbox)
			{
				bbox.minx = x;
				bbox.miny = y;
				bbox.maxx = x + SVGNumber::max_of(w, 0);
				bbox.maxy = y + SVGNumber::max_of(h, 0);
			}
			
			if (w > 0 && h > 0)
			{
				if (allow_draw)
					result = m_canvas->DrawRect(x, y, w, h, rx, ry);
			}
		}
		break;

	default:
		// Unknown element
		OP_ASSERT(0);
		result = OpStatus::ERR;
		break;
	}

	if (update_bbox)
	{
		info.context->SetBBox(bbox);
		info.context->SetBBoxIsValid(TRUE);
	}

	if (OpStatus::IsMemoryError(result))
		return result;
	else if (result == OpSVGStatus::SKIP_CHILDREN)
		return result;
	else
		return OpStatus::OK;
}

OP_STATUS SVGLayoutObject::HandleGraphicsElement(SVGElementInfo& info)
{
	BOOL update_bbox = CalculateBBox();

	SVGPaintNode* paint_node = info.paint_node;

	BOOL allow_draw = m_canvas->AllowDraw(IgnorePointerEvents());

	//
	// Here is brief description of the flags used below and what they signify:
	//
	// allow_draw: Determined by 'visibility' and 'pointer-events' -
	//             iff we care about 'pointer-events' (which we don't do
	//             when drawing)
	//
	// update_bbox: The bounding box of this element should be updated.
	//              This flag does _not_ depend on the allow_draw flag,
	//              i.e. even if the geometry is invisible the bbox
	//              should be calculated

	HTML_Element* layouted_elm = info.layouted;
	Markup::Type type = info.layouted->Type();

	const SVGValueContext& vcxt = GetValueContext();

	OP_STATUS result = OpStatus::OK;

	SVGBoundingBox bbox;
	switch (type)
	{
	case Markup::SVGE_ELLIPSE:
		{
			SVGLengthObject* ccx = NULL;
			SVGLengthObject* ccy = NULL;
			SVGLengthObject* rx_length = NULL;
			SVGLengthObject* ry_length = NULL;

			AttrValueStore::GetLength(layouted_elm, Markup::SVGA_RX, &rx_length, NULL);
			AttrValueStore::GetLength(layouted_elm, Markup::SVGA_RY, &ry_length, NULL);
			AttrValueStore::GetLength(layouted_elm, Markup::SVGA_CX, &ccx, NULL);
			AttrValueStore::GetLength(layouted_elm, Markup::SVGA_CY, &ccy, NULL);

			SVGNumber cx = 0;
			if (ccx)
				cx = SVGUtils::ResolveLength(ccx->GetLength(), SVGLength::SVGLENGTH_X, vcxt);

			SVGNumber cy = 0;
			if (ccy)
				cy = SVGUtils::ResolveLength(ccy->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

			SVGNumber rx = 0;
			if (rx_length)
				rx = SVGUtils::ResolveLength(rx_length->GetLength(), SVGLength::SVGLENGTH_X, vcxt);

			SVGNumber ry = 0;
			if (ry_length)
				ry = SVGUtils::ResolveLength(ry_length->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

			SVGNumber norm_rx = SVGNumber::max_of(rx, 0);
			SVGNumber norm_ry = SVGNumber::max_of(ry, 0);

			if (update_bbox)
			{
				bbox.minx = cx - norm_rx;
				bbox.miny = cy - norm_ry;
				bbox.maxx = cx + norm_rx;
				bbox.maxy = cy + norm_ry;
			}

			// If rx or ry equals 0 then don't render the ellipse
			SVGPrimitiveNode* prim_paint_node = static_cast<SVGPrimitiveNode*>(paint_node);
			prim_paint_node->SetEllipse(cx, cy, norm_rx, norm_ry);

			if (rx > 0 && ry > 0)
			{
				if (allow_draw)
				{
					// NOTE: Currently here to make sure the paint node gets valid extents
					m_canvas->DrawEllipse(cx, cy, rx, ry);
				}
			}
			else
			{
				paint_node->SetVisible(FALSE);
			}
		}
		break;

	case Markup::SVGE_LINE:
		{
			SVGLengthObject* crdx1 = NULL;
			SVGLengthObject* crdy1 = NULL;
			SVGLengthObject* crdx2 = NULL;
			SVGLengthObject* crdy2 = NULL;

			RETURN_IF_ERROR(SVGUtils::GetLineValues(layouted_elm, &crdx1, &crdy1,
													&crdx2, &crdy2));

			SVGNumber x1 = 0;
			if (crdx1)
				x1 = SVGUtils::ResolveLength(crdx1->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
			SVGNumber y1 = 0;
			if (crdy1)
				y1 = SVGUtils::ResolveLength(crdy1->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);
			SVGNumber x2 = 0;
			if (crdx2)
				x2 = SVGUtils::ResolveLength(crdx2->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
			SVGNumber y2 = 0;
			if (crdy2)
				y2 = SVGUtils::ResolveLength(crdy2->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

			if (update_bbox)
			{
				bbox.minx = SVGNumber::min_of(x1, x2);
				bbox.miny = SVGNumber::min_of(y1, y2);
				bbox.maxx = SVGNumber::max_of(x1, x2);
				bbox.maxy = SVGNumber::max_of(y1, y2);
			}

			SVGPrimitiveNode* prim_paint_node = static_cast<SVGPrimitiveNode*>(paint_node);
			prim_paint_node->SetLine(x1, y1, x2, y2);

			if (allow_draw)
			{
				// NOTE: Currently here to make sure the paint node gets valid extents
				m_canvas->DrawLine(x1, y1, x2, y2);

#ifdef SVG_SUPPORT_MARKERS
				// Draw markers if appropriate
				if (!IsInMarker())
					HandleMarkers(info);
#endif // SVG_SUPPORT_MARKERS
			}
		}
		break;

	case Markup::SVGE_PATH:
		{
			SVGNumber plen;
			RETURN_IF_ERROR(AttrValueStore::GetNumber(layouted_elm, Markup::SVGA_PATHLENGTH, plen, -1));

			OpBpath *oppath = NULL;
			result = AttrValueStore::GetPath(layouted_elm, Markup::SVGA_D, &oppath);

			SVGPathNode* path_node = static_cast<SVGPathNode*>(paint_node);
			path_node->SetPath(oppath);
			path_node->SetPathLength(plen);

			// Render the path until we encountered an error (if any)
			if (!oppath)
				break;

			if (update_bbox)
				bbox = oppath->GetBoundingBox();

			if (allow_draw)
			{
				// NOTE: Currently here to make sure the paint node gets valid extents
				m_canvas->DrawPath(oppath, plen);

				if (OpStatus::IsError(result))
					break;

#ifdef SVG_SUPPORT_MARKERS
				// Draw markers if appropriate
				if (!IsInMarker())
					HandleMarkers(info);
#endif // SVG_SUPPORT_MARKERS
			}
		}
		break;

	case Markup::SVGE_POLYGON:
	case Markup::SVGE_POLYLINE:
		{
			SVGVector* list = NULL;
			if (type == Markup::SVGE_POLYLINE)
				SVGUtils::GetPolylineValues(layouted_elm, list);
			else
				SVGUtils::GetPolygonValues(layouted_elm, list);

			if (list && (list->VectorType() != SVGOBJECT_POINT || list->GetCount() == 0))
				list = NULL;

			SVGPolygonNode* poly_node = static_cast<SVGPolygonNode*>(paint_node);
			poly_node->SetPointList(list);
			poly_node->SetIsClosed(type == Markup::SVGE_POLYGON);

			if (!list)
				break;

			if (update_bbox)
			{
				for (UINT32 i = 0; i < list->GetCount(); i++)
				{
					SVGPoint* c = static_cast<SVGPoint*>(list->Get(i));
					if (!c)
						return OpStatus::ERR;

					bbox.minx = SVGNumber::min_of(bbox.minx, c->x);
					bbox.miny = SVGNumber::min_of(bbox.miny, c->y);
					bbox.maxx = SVGNumber::max_of(bbox.maxx, c->x);
					bbox.maxy = SVGNumber::max_of(bbox.maxy, c->y);
				}
			}

			// Render the polygon until we encountered an error (if any)
			if (allow_draw)
			{
				// NOTE: Currently here to make sure the paint node gets valid extents
				m_canvas->DrawPolygon(*list, type == Markup::SVGE_POLYGON);

				if (OpStatus::IsError(result))
				{
					OP_ASSERT(result != OpStatus::ERR);
					break;
				}

#ifdef SVG_SUPPORT_MARKERS
				// Draw markers if appropriate
				if (!IsInMarker())
					HandleMarkers(info);
#endif // SVG_SUPPORT_MARKERS
			}
		}
		break;

	case Markup::SVGE_CIRCLE:
		{
			SVGLengthObject* crdcx = NULL;
			SVGLengthObject* crdcy = NULL;
			SVGLengthObject* r_length = NULL;

			RETURN_IF_ERROR(SVGUtils::GetCircleValues(layouted_elm, &crdcx, &crdcy, &r_length));

			SVGNumber cx = 0;
			if (crdcx)
				cx = SVGUtils::ResolveLength(crdcx->GetLength(), SVGLength::SVGLENGTH_X, vcxt);

			SVGNumber cy = 0;
			if (crdcy)
				cy = SVGUtils::ResolveLength(crdcy->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

			SVGNumber r = 0;
			if (r_length)
				r = SVGUtils::ResolveLength(r_length->GetLength(), SVGLength::SVGLENGTH_OTHER, vcxt);

			SVGNumber norm_r = SVGNumber::max_of(r, 0);

			if (update_bbox)
			{
				bbox.minx = cx - norm_r;
				bbox.miny = cy - norm_r;
				bbox.maxx = cx + norm_r;
				bbox.maxy = cy + norm_r;
			}

			SVGPrimitiveNode* prim_paint_node = static_cast<SVGPrimitiveNode*>(paint_node);
			prim_paint_node->SetEllipse(cx, cy, norm_r, norm_r);

			// If r = 0 then don't render the circle
			if (r > 0)
			{
				if (allow_draw)
				{
					// NOTE: Currently here to make sure the paint node gets valid extents
					m_canvas->DrawEllipse(cx, cy, r, r);
				}
			}
			else
			{
				paint_node->SetVisible(FALSE);
			}
		}
		break;

	case Markup::SVGE_RECT:
		{
			SVGLengthObject *wl = NULL;
			SVGLengthObject *hl = NULL;
			SVGLengthObject *rxl = NULL;
			SVGLengthObject *ryl = NULL;
			SVGLengthObject* crdx = NULL;
			SVGLengthObject *crdy = NULL;
			RETURN_IF_ERROR(SVGUtils::GetRectValues(layouted_elm, &crdx, &crdy, &wl, &hl,
													&rxl, &ryl));

			SVGNumber x = 0;
			if (crdx)
				x = SVGUtils::ResolveLength(crdx->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
			SVGNumber y = 0;
			if (crdy)
				y = SVGUtils::ResolveLength(crdy->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

			SVGNumber w;
			if (wl)
				w = SVGUtils::ResolveLength(wl->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
			SVGNumber h;
			if (hl)
				h = SVGUtils::ResolveLength(hl->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);
			SVGNumber rx;
			if (rxl)
				rx = SVGUtils::ResolveLength(rxl->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
			SVGNumber ry;
			if (ryl)
				ry = SVGUtils::ResolveLength(ryl->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

			if (ryl && !rxl)
			{
				rx = ry;
			}
			else if (rxl && !ryl)
			{
				ry = rx;
			}

			SVGUtils::AdjustRectValues(w, h, rx, ry);

			SVGNumber norm_w = SVGNumber::max_of(w, 0);
			SVGNumber norm_h = SVGNumber::max_of(h, 0);

			if (update_bbox)
			{
				bbox.minx = x;
				bbox.miny = y;
				bbox.maxx = x + norm_w;
				bbox.maxy = y + norm_h;
			}
			
			SVGPrimitiveNode* prim_paint_node = static_cast<SVGPrimitiveNode*>(paint_node);
			prim_paint_node->SetRectangle(x, y, norm_w, norm_h, rx, ry);

			if (w > 0 && h > 0)
			{
				if (allow_draw)
				{
					// NOTE: Currently here to make sure the paint node gets valid extents
					result = m_canvas->DrawRect(x, y, w, h, rx, ry);
				}
			}
			else
			{
				paint_node->SetVisible(FALSE);
			}
		}
		break;

	default:
		// Unknown element
		OP_ASSERT(0);
		result = OpStatus::ERR;
		break;
	}

	paint_node->UpdateState(m_canvas);

	// FIXME: Disregards geometric conditions (w <= 0 et.c)

	// visibility represented as opacity == 0
	if (!allow_draw)
		paint_node->SetVisible(FALSE);

	m_current_container->Insert(info.paint_node, m_current_pred);
	m_current_pred = info.paint_node;

	if (update_bbox)
	{
		info.context->SetBBox(bbox);
		info.context->SetBBoxIsValid(TRUE);
	}

	UpdateElement(info);

	if (OpStatus::IsMemoryError(result))
		return result;
	else if (result == OpSVGStatus::SKIP_CHILDREN)
		return result;
	else
		return OpStatus::OK;
}

OP_STATUS SVGIntersectionObject::HandleGraphicsElement(SVGElementInfo& info)
{
	if (!m_canvas->AllowPointerEvents())
		return OpStatus::OK;

	OP_STATUS status = SVGVisualTraversalObject::HandleGraphicsElement(info);

	if (m_canvas->GetPointerEvents() == SVGPOINTEREVENTS_BOUNDINGBOX &&
		m_canvas->GetRenderMode() == SVGCanvas::RENDERMODE_INTERSECT_POINT)
	{
		OP_ASSERT(info.context->IsBBoxValid());
		SVGBoundingBox bbox = info.context->GetBBox();

		OpStatus::Ignore(m_canvas->DrawRect(bbox.minx, bbox.miny,
											bbox.maxx - bbox.minx,
											bbox.maxy - bbox.miny, 0, 0));
	}

	return status;
}

OP_STATUS SVGTraversalObject::CalculateSymbolViewport(SVGElementInfo& info, ViewportInfo& vpinfo)
{
	SVGRect* viewbox = NULL;
	SVGRectObject* viewbox_obj;
	RETURN_IF_ERROR(AttrValueStore::GetViewBox(info.layouted, &viewbox_obj));
	if (viewbox_obj)
		viewbox = SVGRectObject::p(viewbox_obj);

	SVGAspectRatio* ratio = NULL;
	RETURN_IF_ERROR(AttrValueStore::GetPreserveAspectRatio(info.layouted, ratio));

	// Fetch width/height from referencing <use>, and if either is
	// missing use 100% instead (much like in the <svg> case)
	SVGLengthObject default_dimension(SVGNumber(100), CSS_PERCENTAGE);
	SVGLengthObject *wl = &default_dimension;
	SVGLengthObject *hl = &default_dimension;

	// Should be inside a shadow tree
	OP_ASSERT(info.traversed != info.layouted);

	// Get the viewport parameters from the <use> element
	if (HTML_Element* ref_use_elm = GetReferencingUseElement(info.traversed))
	{
		AttrValueStore::GetLength(ref_use_elm, Markup::SVGA_WIDTH, &wl, &default_dimension);
		AttrValueStore::GetLength(ref_use_elm, Markup::SVGA_HEIGHT, &hl, &default_dimension);
	}

	const SVGValueContext& vcxt = GetValueContext();

	vpinfo.viewport.width = SVGUtils::ResolveLength(wl->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
	vpinfo.viewport.height = SVGUtils::ResolveLength(hl->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

	RETURN_IF_ERROR(SVGUtils::GetViewboxTransform(vpinfo.viewport, viewbox, ratio,
												  vpinfo.transform, vpinfo.actual_viewport));

	return OpStatus::OK;
}

// FIXME: The handling of <symbol> should be mostly mergeable/analog
// to the handling <svg>, so this pair should probably vanish in the
// future.
OP_STATUS SVGTraversalObject::PushSymbolViewport(SVGElementInfo& info, const ViewportInfo& vpinfo)
{
	SVGNumberPair vp(vpinfo.actual_viewport.width, vpinfo.actual_viewport.height);

	RETURN_IF_ERROR(PushViewport(vp));
	info.Set(RESOURCE_VIEWPORT);

	return OpStatus::OK;
}

OP_STATUS SVGVisualTraversalObject::PushSymbolViewport(SVGElementInfo& info, const ViewportInfo& vpinfo)
{
#ifdef SVG_SUPPORT_STENCIL
	const HTMLayoutProperties& props = *info.props->GetProps();
	if (SVGContentClipper::CreateClip(m_canvas, vpinfo.viewport, props, TRUE))
		info.Set(RESOURCE_VIEWPORT_CLIP);
#endif // SVG_SUPPORT_STENCIL

	return SVGTraversalObject::PushSymbolViewport(info, vpinfo);
}

OP_STATUS SVGTraversalObject::PopSymbolViewport(SVGElementInfo& info)
{
	if (info.IsSet(RESOURCE_VIEWPORT))
	{
		PopViewport();
		info.Clear(RESOURCE_VIEWPORT);
	}
	return OpStatus::OK;
}

#ifdef SVG_SUPPORT_FILTERS
OP_STATUS SVGLayoutObject::SetupFilter(SVGElementInfo& info)
{
	const HTMLayoutProperties& props = *info.props->GetProps();
	const SvgProperties* svg_props = props.svg;
	SVGURL svg_url(svg_props->filter, URL());
	HTML_Element* filter_elm = SVGUtils::FindURLRelReferredNode(m_resolver, m_doc_ctx, info.traversed, svg_url);

	if (!filter_elm)
		m_doc_ctx->RegisterUnresolvedDependency(info.traversed);

	if (!filter_elm || !filter_elm->IsMatchingType(Markup::SVGE_FILTER, NS_SVG))
		return OpSVGStatus::ELEMENT_IS_INVISIBLE;

	RETURN_IF_ERROR(m_resolver->FollowReference(filter_elm));
	m_doc_ctx->RegisterDependency(info.traversed, filter_elm);

	// Quirk for the case where text bboxes has been broken
	// by the bbox-traversal followed by layout of
	// filtered text elements
	Markup::Type type = info.layouted->Type();
	if (type == Markup::SVGE_TEXT || type == Markup::SVGE_TSPAN ||
		type == Markup::SVGE_TREF || type == Markup::SVGE_TEXTPATH)
		info.context->SetBBoxIsValid(FALSE);

	SVGFilter* filter = NULL;
	OP_STATUS result = SVGFilter::Create(filter_elm, info.traversed, GetValueContext(),
										 m_resolver, m_doc_ctx, m_canvas, NULL, &filter);

	if (OpStatus::IsSuccess(result))
	{
		// Save value of image-rendering
		filter->SetImageRendering(SVGGetImageRenderingFromProps(props));

		info.paint_node->SetFilter(filter);
	}
	else
	{
		OP_DELETE(filter);
	}

	m_resolver->LeaveReference(filter_elm);

	RETURN_IF_MEMORY_ERROR(result);

	return OpStatus::OK;
}
#endif // SVG_SUPPORT_FILTERS

#ifdef SVG_SUPPORT_STENCIL
OP_STATUS SVGVisualTraversalObject::SetupClipOrMask(SVGElementInfo& info,
													const SVGURLReference& urlref,
													Markup::Type match_type,
													unsigned resource_tag)
{
	OP_ASSERT(!urlref.info.is_none);

	SVGURL svg_url(urlref, URL());
	HTML_Element* referenced_element = SVGUtils::FindURLRelReferredNode(m_resolver, m_doc_ctx, info.traversed, svg_url);
	if (!referenced_element)
	{
		m_doc_ctx->RegisterUnresolvedDependency(info.traversed);
		return OpStatus::OK;
	}
	else if (!referenced_element->IsMatchingType(match_type, NS_SVG))
	{
		return OpStatus::OK;
	}

	if (OpStatus::IsSuccess(m_resolver->FollowReference(referenced_element)))
	{
		m_doc_ctx->RegisterDependency(info.traversed, referenced_element);

		OP_STATUS result = PushReferringNode(info);
		if (OpStatus::IsSuccess(result))
		{
			result = GenerateClipOrMask(referenced_element);

			PopReferringNode();
		}

		m_resolver->LeaveReference(referenced_element);

		if (OpStatus::IsError(result) || result == OpSVGStatus::ELEMENT_IS_INVISIBLE)
			return result;

		info.Set(resource_tag);
	}
	return OpStatus::OK;
}

void SVGVisualTraversalObject::CleanStencils(SVGElementInfo& info)
{
	if (info.IsSet(RESOURCE_CLIPPATH))
	{
		m_canvas->RemoveClip();

		info.Clear(RESOURCE_CLIPPATH);
	}

	if (info.IsSet(RESOURCE_VIEWPORT_CLIP))
	{
		m_canvas->RemoveClip();

		info.Clear(RESOURCE_VIEWPORT_CLIP);
	}

	if (info.IsSet(RESOURCE_MASK))
	{
		m_canvas->RemoveMask();

		info.Clear(RESOURCE_MASK);
	}
}
#endif // SVG_SUPPORT_STENCIL

OP_BOOLEAN SVGVisualTraversalObject::FetchAltGlyphs(SVGElementInfo& info)
{
	SVGAltGlyphElement* ag_context = static_cast<SVGAltGlyphElement*>(info.context);
	OpVector<GlyphInfo>& glyphs = ag_context->GetGlyphVector();

	if (info.GetInvalidState() >= INVALID_FONT_METRICS)
	{
		glyphs.DeleteAll();

		HTML_Element* target = SVGUtils::FindHrefReferredNode(m_resolver, m_doc_ctx, info.layouted);
		if (!target)
			return OpBoolean::IS_FALSE;

		RETURN_VALUE_IF_ERROR(m_resolver->FollowReference(target), OpBoolean::IS_FALSE);

		SVGAltGlyphMatcher ag_matcher(m_doc_ctx, m_resolver);

		OP_BOOLEAN result = ag_matcher.Match(target);
		if (result == OpBoolean::IS_TRUE)
		{
			const HTMLayoutProperties& props = *info.props->GetProps();
			SVGNumber fontsize = props.svg->GetFontSize(info.layouted);

			result = ag_matcher.FetchGlyphData(!m_textinfo->IsVertical(), fontsize, glyphs);
		}

		m_resolver->LeaveReference(target);
		return result;
	}
	else
	{
		return glyphs.GetCount() > 0 ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
	}
}

OP_STATUS SVGVisualTraversalObject::HandleAltGlyph(SVGElementInfo& info, BOOL have_glyphs)
{
	if (!have_glyphs)
		return OpStatus::OK;

	SVGAltGlyphElement* ag_context = static_cast<SVGAltGlyphElement*>(info.context);
	SVGFontDescriptor* fontdesc = &m_textinfo->font_desc;

	SVGDocumentContext* font_doc_ctx = info.context->IsShadowNode() ? AttrValueStore::GetSVGDocumentContext(info.layouted) : m_doc_ctx;
	SVGTextRenderer tr(m_canvas, font_doc_ctx, CalculateBBox());
	tr.SetFontDescriptor(fontdesc);

	SVGAltGlyphTraverser traverser(GetValueContext(), m_doc_ctx, &tr);
	if (info.paint_node)
		traverser.SetPaintNode(static_cast<SVGAltGlyphNode*>(info.paint_node));

	return traverser.Traverse(ag_context->GetGlyphVector(), info, *m_textinfo);
}

OP_STATUS SVGTextMeasurementObject::HandleAltGlyph(SVGElementInfo& info, BOOL have_glyphs)
{
	if (m_textinfo->data && m_textinfo->data->NeedsRenderMode())
		return SVGVisualTraversalObject::HandleAltGlyph(info, have_glyphs);

	if (!have_glyphs)
		return OpStatus::OK;

	SVGAltGlyphElement* ag_context = static_cast<SVGAltGlyphElement*>(info.context);
	SVGFontDescriptor* fontdesc = &m_textinfo->font_desc;

	SVGDocumentContext* font_doc_ctx = info.context->IsShadowNode() ? AttrValueStore::GetSVGDocumentContext(info.layouted) : m_doc_ctx;
	SVGTextRenderer tr(m_canvas, font_doc_ctx, FALSE);
	tr.SetFontDescriptor(fontdesc);

	SVGAltGlyphExtentTraverser traverser(GetValueContext(), m_doc_ctx, &tr);
	return traverser.Traverse(ag_context->GetGlyphVector(), info, *m_textinfo);
}

OP_STATUS SVGLayoutObject::HandleAltGlyph(SVGElementInfo& info, BOOL have_glyphs)
{
	OP_STATUS status = SVGVisualTraversalObject::HandleAltGlyph(info, have_glyphs);
	if (info.paint_node)
	{
		SVGAltGlyphElement* ag_context = static_cast<SVGAltGlyphElement*>(info.context);
		SVGAltGlyphNode* ag_node = static_cast<SVGAltGlyphNode*>(info.paint_node);

		if (OpStatus::IsSuccess(status) && have_glyphs)
		{
			SVGDocumentContext* font_doc_ctx = info.context->IsShadowNode() ?
				AttrValueStore::GetSVGDocumentContext(info.layouted) : m_doc_ctx;
			ag_node->UpdateState(m_canvas);
			ag_node->SetFontDocument(font_doc_ctx);
			ag_node->SetTextPathAttributes(m_textinfo->GetCurrentTextPathAttributes());
			ag_node->SetGlyphVector(&ag_context->GetGlyphVector());
			ag_node->SetBBox(m_textinfo->bbox.Get());
			ag_node->SetTextSelectionDocument(m_textinfo->doc_ctx);
			ag_node->SetTextSelectionElement(info.layouted);
#ifdef SVG_SUPPORT_EDITABLE
			ag_node->SetEditable(m_textinfo->editable);
#endif // SVG_SUPPORT_EDITABLE
		}
		else
		{
			ag_node->SetGlyphVector(NULL);
		}
	}
	return status;
}

OP_STATUS SVGTextMeasurementObject::HandleForcedLineBreak(SVGElementInfo& info)
{
	if (m_textinfo->data && m_textinfo->data->SetSelectionRectList())
		return SVGVisualTraversalObject::HandleForcedLineBreak(info);
	else
		return m_textarea_info->ForceLineBreak(*info.props->GetProps());
}

OP_STATUS SVGVisualTraversalObject::HandleForcedLineBreak(SVGElementInfo& info)
{
#ifdef SVG_SUPPORT_EDITABLE
	CheckCaret(info);
#endif // SVG_SUPPORT_EDITABLE

	OP_ASSERT(m_textarea_info->remaining_fragments_on_line == 0);

	m_textinfo->AdvanceBlock();
	return OpStatus::OK;
}

void SVGLayoutObject::HandleTextAreaBBox(const SVGBoundingBox& bbox)
{
	if (m_canvas->GetPointerEvents() != SVGPOINTEREVENTS_BOUNDINGBOX)
		return;

	OpStatus::Ignore(m_canvas->DrawRect(bbox.minx, bbox.miny,
										bbox.maxx - bbox.minx,
										bbox.maxy - bbox.miny, 0, 0));
}

OP_STATUS SVGVisualTraversalObject::HandleTextElement(SVGElementInfo& info)
{
	BOOL update_bbox = CalculateBBox();

	m_canvas->SetCurrentElement(info.traversed);

	// BOOL allow_draw = m_canvas->AllowDraw(IgnorePointerEvents());

	//
	// Here is brief description of the flags used below and what they signify:
	//
	// allow_draw: Determined by 'visibility' and 'pointer-events' -
	//             iff we care about 'pointer-events' (which we don't do
	//             when drawing)
	//
	// update_bbox: The bounding box of this element should be updated.
	//              This flag does _not_ depend on the allow_draw flag,
	//              i.e. even if the geometry is invisible the bbox
	//              should be calculated

	HTML_Element* traversed_elm = info.traversed;
	HTML_Element* layouted_elm = info.layouted;
	Markup::Type type = layouted_elm->Type();

	OP_STATUS result = OpStatus::OK;

	SVGBoundingBox bbox;
	switch (type)
	{
	case Markup::HTE_TEXT:
		if (info.GetInvalidState() >= INVALID_FONT_METRICS)
		{
			info.context->GetTextCache()->RemoveCachedInfo();
		}
		result = HandleTextNode(info, layouted_elm, update_bbox ? &bbox : NULL);
		break;

	case Markup::SVGE_ALTGLYPH:
		{
			OP_BOOLEAN ag_result = FetchAltGlyphs(info);
			BOOL has_glyphs = ag_result == OpBoolean::IS_TRUE;

			HandleAltGlyph(info, has_glyphs);

			if (has_glyphs)
			{
				result = OpSVGStatus::SKIP_CHILDREN;
				break;
			}
			RETURN_IF_MEMORY_ERROR(ag_result);
		}
		// Lookup of alternate glyphs failed - treat as tspan, i.e. fall through.

	case Markup::SVGE_TSPAN:
		update_bbox = FALSE;
		break;

	case Markup::SVGE_TREF:
		{
			// And here we slow down character invalidation (do we
			// still need this, won't the dep.graph handle this?)
			m_doc_ctx->SetTRefSeenInSVG();

			/**
			 * "The attributes are applied as if the
			 * 'tref' element was replaced by a 'tspan'
			 * with the referenced character data
			 * (stripped of all supplemental markup)
			 * embedded within the hypothetical 'tspan'
			 * element."
			 */

			HTML_Element* target = SVGUtils::FindHrefReferredNode(m_resolver, m_doc_ctx, layouted_elm);
			if (target)
			{
				m_doc_ctx->RegisterDependency(traversed_elm, target);

				result = HandleTextNode(info, target, update_bbox ? &bbox : NULL);
			}
		}
		break;

	case Markup::SVGE_TEXTAREA:
		if (!update_bbox)
			break;

		bbox.minx = m_textarea_info->vis_area.x;
		bbox.miny = m_textarea_info->vis_area.y;

		bbox.maxx = bbox.minx;
		if (!m_textarea_info->info.width_auto)
			bbox.maxx += m_textarea_info->vis_area.width;

		bbox.maxy = bbox.miny;
		if (!m_textarea_info->info.height_auto)
			bbox.maxy += m_textarea_info->vis_area.height;

		m_textinfo->bbox.Add(bbox);

		HandleTextAreaBBox(bbox);

		// Update the bounding box right away if pointer-events
		// indicate that the bounding box is needed.
		update_bbox = (m_canvas->GetPointerEvents() == SVGPOINTEREVENTS_BOUNDINGBOX);
		break;

	case Markup::SVGE_TBREAK:
		result = HandleForcedLineBreak(info);
		break;

	case Markup::SVGE_TEXT:
		m_textinfo->AddPendingChunk();

		update_bbox = FALSE;
		break;

	case Markup::SVGE_TEXTPATH:
		{
			update_bbox = FALSE;

			// Reset path parameters
			m_textinfo->pathDistance = 0;
			m_textinfo->pathDisplacement = 0;

			// Force a new chunk
			m_textinfo->AddPendingChunk();
			TextPathAttributes* textpath_attrs = info.paint_node ? static_cast<SVGTextPathNode*>(info.paint_node)->GetTextPathAttributes() : NULL;

			BOOL has_xlink_href;
			HTML_Element* target = SVGUtils::FindHrefReferredNode(m_resolver, m_doc_ctx, layouted_elm, &has_xlink_href);
			if (target && target->IsMatchingType(Markup::SVGE_PATH, NS_SVG))
			{
				m_doc_ctx->RegisterDependency(traversed_elm, target);

				OpBpath* path = AttrValueStore::GetPath(target, Markup::SVGA_D);
				if (path)
				{
					SVGMatrix additional_transform;
					AttrValueStore::GetMatrix(target, Markup::SVGA_TRANSFORM, additional_transform);

					m_textinfo->path = OP_NEW(SVGMotionPath, ());
					if (!m_textinfo->path)
						return OpStatus::ERR_NO_MEMORY;

					m_textinfo->path->SetTransform(additional_transform);

					RETURN_IF_ERROR(m_textinfo->path->Set(path, -1));

					if (textpath_attrs)
					{
						textpath_attrs->SetPath(path);
						textpath_attrs->path_transform.Copy(additional_transform);
					}
				}
			}
			else if (has_xlink_href)
			{
				// If xlink:href was present, but didn't point to
				// a valid and loaded element, we treat the
				// textpath as invisible.
				result = OpSVGStatus::ELEMENT_IS_INVISIBLE;
			}

			if (OpStatus::IsSuccess(result))
			{
				SVGLengthObject* start_offset = NULL;

				// If the attribute is not specified, the effect
				// is as if a value of "0" were specified.
				result = AttrValueStore::GetLength(layouted_elm, Markup::SVGA_STARTOFFSET, &start_offset);
				if (OpStatus::IsSuccess(result) && start_offset)
				{
					// Note that 'useroffset' is in "real" path
					// coordinates, not in pathlength coordinates.
					SVGNumber useroffset;

					/**
					 * "An offset from the start of the 'path' for the initial current text
					 * position, calculated using the user agent's distance along the path
					 * algorithm. If a <length> other than a percentage is given, then the
					 * start_offset represents a distance along the path measured in the
					 * current user coordinate system. If a percentage is given, then the
					 * start_offset represents a percentage distance along the entire path.
					 * Thus, start_offset="0%" indicates the start point of the 'path' and
					 * start_offset="100%" indicates the end point of the 'path'."
					 */
					if (start_offset->GetUnit() == CSS_PERCENTAGE)
					{
						useroffset = start_offset->GetScalar() / 100;
						useroffset *= m_textinfo->path->GetLength();
					}
					else
					{
						const SVGValueContext& vcxt = GetValueContext();

						// Should we do ResolveLength or not?
						useroffset = SVGUtils::ResolveLength(start_offset->GetLength(), SVGLength::SVGLENGTH_OTHER, vcxt);

						if (target)
						{
							SVGNumber plen;
							RETURN_IF_ERROR(AttrValueStore::GetNumber(target, Markup::SVGA_PATHLENGTH, plen, -1));
							if (plen > 0 && m_textinfo->path->GetLength().NotEqual(0))
							{
								SVGNumber scalefactor = useroffset / plen;
								useroffset = m_textinfo->path->GetLength() * scalefactor;
							}
						}
					}

#ifndef SVG_ALLOW_NEGATIVE_STARTOFFSET
					if (start_offset->GetScalar() < 0)
					{
						// Note that this is an error according to SVG 1.1
						SVG_NEW_ERROR(layouted_elm);
						SVG_REPORT_ERROR((UNI_L("startOffset has invalid value.")));
						useroffset = 0;
					}
#endif // !SVG_ALLOW_NEGATIVE_STARTOFFSET

					m_textinfo->pathDistance = useroffset;
				}
			}

			SVGSpacing spacing = SVGSPACING_EXACT;
			SVGMethod method = SVGMETHOD_ALIGN;
			if (OpStatus::IsSuccess(result))
			{
				// If the attribute is not specified, the effect is as
				// if a value of align were specified.
				method = (SVGMethod)AttrValueStore::GetEnumValue(layouted_elm, Markup::SVGA_METHOD,
																 SVGENUM_METHOD, SVGMETHOD_ALIGN);

				// If the attribute is not specified, the effect is as
				// if a value of exact were specified.
				spacing = (SVGSpacing)AttrValueStore::GetEnumValue(layouted_elm, Markup::SVGA_SPACING,
																   SVGENUM_SPACING, SVGSPACING_EXACT);
			}

			m_textinfo->packed.spacing_auto = (spacing == SVGSPACING_AUTO);
			m_textinfo->packed.method = method;

			if (textpath_attrs)
				textpath_attrs->path_warp = (method == SVGMETHOD_STRETCH);
		}
		break;

	default:
		// Unknown element
		OP_ASSERT(0);
		result = OpStatus::ERR;
		break;
	}

	if (update_bbox)
	{
		info.context->SetBBox(bbox);
		info.context->SetBBoxIsValid(TRUE);
	}

	if (OpStatus::IsMemoryError(result))
		return result;
	else if (result == OpSVGStatus::SKIP_CHILDREN)
		return result;
	else
		return OpStatus::OK;
}

OP_STATUS SVGLayoutObject::HandleTextElement(SVGElementInfo& info)
{
	OP_STATUS status = SVGVisualTraversalObject::HandleTextElement(info);

	UpdateElement(info);
	return status;
}

OP_STATUS SVGIntersectionObject::HandleTextElement(SVGElementInfo& info)
{
	if (!m_canvas->AllowPointerEvents())
		return OpStatus::OK;

	OP_STATUS status = SVGVisualTraversalObject::HandleTextElement(info);

	if (m_canvas->GetPointerEvents() == SVGPOINTEREVENTS_BOUNDINGBOX &&
		m_canvas->GetRenderMode() == SVGCanvas::RENDERMODE_INTERSECT_POINT &&
		!info.layouted->IsText())
	{
		OP_ASSERT(info.context->IsBBoxValid());
		SVGBoundingBox bbox = info.context->GetBBox();

		OpStatus::Ignore(m_canvas->DrawRect(bbox.minx, bbox.miny,
											bbox.maxx - bbox.minx,
											bbox.maxy - bbox.miny, 0, 0));
	}

	return status;
}

OP_STATUS SVGVisualTraversalObject::EnterAnchor(SVGElementInfo& info)
{
	if (m_textinfo)
		RETURN_IF_MEMORY_ERROR(m_textinfo->bbox.PushNew()); // Link on text

	return OpStatus::OK;
}

OP_STATUS SVGVisualTraversalObject::LeaveAnchor(SVGElementInfo& info)
{
	if (m_textinfo)
		m_textinfo->bbox.Pop(); // Link on text

	return OpStatus::OK;
}

OP_STATUS SVGVisualTraversalObject::EnterTextGroup(SVGElementInfo& info)
{
	if (m_textinfo)
		RETURN_IF_MEMORY_ERROR(m_textinfo->bbox.PushNew());

	return OpStatus::OK;
}

OP_STATUS SVGVisualTraversalObject::LeaveTextGroup(SVGElementInfo& info)
{
	if (m_textinfo)
		m_textinfo->bbox.Pop();

	return OpStatus::OK;
}

OP_STATUS SVGVisualTraversalObject::PrepareTextAreaElement(SVGElementInfo& info)
{
	const HTMLayoutProperties& props = *info.props->GetProps();

	m_textarea_info->text_align = props.text_align;

	m_textinfo->linepos.y = m_textarea_info->vis_area.y;

	const SvgProperties *svg_props = props.svg;
	SVGDisplayAlign display_align = (SVGDisplayAlign)svg_props->textinfo.displayalign;
	if (display_align == SVGDISPLAYALIGN_CENTER ||
		display_align == SVGDISPLAYALIGN_AFTER)
	{
		SVGNumber total_height = m_textarea_info->GetTotalTextHeight();

		SVGNumber adjust = m_textarea_info->vis_area.height - total_height;
		if (display_align == SVGDISPLAYALIGN_CENTER)
			adjust /= 2;

		m_textinfo->linepos.y += adjust;
	}

	SVGLineInfo* li = m_textarea_info->lineinfo->Get(0);
	OP_ASSERT(li || !"PrepareTextAreaElement");

	// Set position
	m_textinfo->linepos.x = m_textarea_info->GetStart(li);

	m_textinfo->ctp = m_textinfo->linepos;
	m_textinfo->ctp.y += li ? li->height - li->descent : svg_props->GetLineIncrement() * 0.8;

	m_textarea_info->current_line = 0;
	m_textarea_info->remaining_fragments_on_line = li ? li->num_fragments : INT_MAX;

	return OpStatus::OK;
}

OP_STATUS SVGTextMeasurementObject::SetupTextAreaElement(SVGElementInfo& info)
{
	if (m_textinfo->data && m_textinfo->data->SetSelectionRectList())
	{
		RETURN_IF_ERROR(CalculateTextAreaExtents(info));
	}

	return SVGVisualTraversalObject::SetupTextAreaElement(info);
}

OP_STATUS SVGTextMeasurementObject::PrepareTextAreaElement(SVGElementInfo& info)
{
	OP_ASSERT(m_textarea_info);

	if (m_textinfo->data && m_textinfo->data->SetSelectionRectList())
		return SVGVisualTraversalObject::PrepareTextAreaElement(info);
	else
		return m_textarea_info->NewBlock(*info.props->GetProps());
}

OP_STATUS SVGVisualTraversalObject::SetupTextAreaElement(SVGElementInfo& info)
{
	OP_ASSERT(m_textinfo != NULL);
	OP_ASSERT(info.layouted->Type() == Markup::SVGE_TEXTAREA);

	m_textarea_info = OP_NEW(SVGTextAreaInfo, ());
	if (!m_textarea_info)
		return OpStatus::ERR_NO_MEMORY;

	m_textinfo->area = m_textarea_info;

	FetchTextAreaValues(info.layouted, GetValueContext(), m_textarea_info);

	SVGTextAreaElement* ta_ctx = static_cast<SVGTextAreaElement*>(info.context);

	m_textarea_info->lineinfo = ta_ctx->GetLineInfo();

	return PrepareTextAreaElement(info);
}

OP_STATUS SVGVisualTraversalObject::CalculateTextAreaExtents(SVGElementInfo& info)
{
	SVGTextAreaElement* ta_ctx = static_cast<SVGTextAreaElement*>(info.context);

	// FIXME: Need to determine if the layout is
	// up-to-date and don't do this step if it is
	unsigned trav_invalid_state = info.GetInvalidState();
	if (trav_invalid_state >= INVALID_FONT_METRICS)
	{
		ta_ctx->ClearLineInfo();

		// Transfer the current invalid state to the current element,
		// so that child lists are rebuilt when needed.
		// A better way would probably be to have invalid state as an
		// input to the traverser.
		ta_ctx->AddInvalidState((SVGElementInvalidState)trav_invalid_state);

		SVGNullCanvas extent_canvas;
		extent_canvas.SetDefaults(75); // Default low quality since it really doesn't matter
		extent_canvas.ConcatTransform(m_canvas->GetTransform());

		SVGTextMeasurementObject extent_object(GetChildPolicy());
		extent_object.SetCanvas(&extent_canvas);
		extent_object.SetCurrentViewport(GetCurrentViewport());
		extent_object.SetDocumentContext(m_doc_ctx);
		extent_object.SetupResolver(m_resolver);

		// ...
		// Further initialization of extent_object
		// ...
		RETURN_IF_ERROR(extent_object.CreateTextInfo(static_cast<SVGTextRootContainer*>(info.context)));

		RETURN_IF_ERROR(SVGTraverser::Traverse(&extent_object, info.traversed, NULL));
	}
	return OpStatus::OK;
}

OP_STATUS SVGLayoutObject::SetupTextAreaElement(SVGElementInfo& info)
{
	RETURN_IF_ERROR(CalculateTextAreaExtents(info));

	return SVGVisualTraversalObject::SetupTextAreaElement(info);
}

OP_STATUS SVGVisualTraversalObject::HandleExternalContent(SVGElementInfo& info)
{
	BOOL update_bbox = CalculateBBox();

	m_canvas->SetCurrentElement(info.traversed);

	BOOL allow_draw = m_canvas->AllowDraw(IgnorePointerEvents());

	//
	// Here is brief description of the flags used below and what they signify:
	//
	// allow_draw: Determined by 'visibility' and 'pointer-events' -
	//             iff we care about 'pointer-events' (which we don't do
	//             when drawing)
	//
	// update_bbox: The bounding box of this element should be updated.
	//              This flag does _not_ depend on the allow_draw flag,
	//              i.e. even if the geometry is invisible the bbox
	//              should be calculated

	OP_STATUS result = OpStatus::OK;
	HTML_Element* traversed_elm = info.traversed;
	HTML_Element* layouted_elm = info.layouted;

	const HTMLayoutProperties& props = *info.props->GetProps();

	SVGBoundingBox bbox;

	Markup::Type type = layouted_elm->Type();
	switch (type)
	{
#ifdef SVG_SUPPORT_FOREIGNOBJECT
	case Markup::SVGE_FOREIGNOBJECT:
#endif // SVG_SUPPORT_FOREIGNOBJECT
	case Markup::SVGE_ANIMATION:
	case Markup::SVGE_IMAGE:
		{
			SVGRect img_vp;
			if (update_bbox || allow_draw)
			{
				RETURN_IF_ERROR(SVGUtils::GetImageValues(layouted_elm, GetValueContext(), img_vp));
			}

			if (update_bbox)
			{
				bbox.minx = img_vp.x;
				bbox.miny = img_vp.y;
				bbox.maxx = img_vp.x + SVGNumber::max_of(img_vp.width,0);
				bbox.maxy = img_vp.y + SVGNumber::max_of(img_vp.height,0);
			}

			if (!allow_draw || img_vp.width < 0 || img_vp.height < 0)
				break;

			SVGCanvas::ImageRenderQuality quality = SVGGetImageQualityFromProps(props);

			OpStatus::Ignore(FillViewport(info, img_vp));

			URL *imageURL = NULL;
			SVGDocumentContext* doc_ctx = SVGUtils::IsShadowNode(traversed_elm) ? AttrValueStore::GetSVGDocumentContext(layouted_elm) : m_doc_ctx;
			RETURN_IF_ERROR(AttrValueStore::GetXLinkHREF(doc_ctx->GetURL(), layouted_elm, imageURL));

			if ((!imageURL || imageURL->IsEmpty()) && (type != Markup::SVGE_FOREIGNOBJECT || AttrValueStore::HasObject(layouted_elm, Markup::XLINKA_HREF, NS_IDX_XLINK)))
			{
				result = OpSVGStatus::ELEMENT_IS_INVISIBLE;
				break;
			}

#ifdef SVG_SUPPORT_SVG_IN_IMAGE
			if (imageURL && type == Markup::SVGE_IMAGE)
			{
				// Note that this will only detect the simplest of
				// loops, however DoImageOrAnimation uses the
				// elementresolver loop detection to catch the
				// trickier cases.
				if (*imageURL == m_doc_ctx->GetURL())
				{
					SVG_NEW_ERROR(layouted_elm);
					SVG_REPORT_ERROR((UNI_L("The 'image' element cannot reference the SVG file that contains it.")));

					result = OpStatus::ERR;
					break;
				}
			}

			if ((type == Markup::SVGE_IMAGE || type == Markup::SVGE_ANIMATION) &&
				imageURL &&
				(imageURL->ContentType() == URL_SVG_CONTENT ||
				 imageURL->ContentType() == URL_XML_CONTENT))
			{
				result = HandleVectorImage(info, img_vp);
			}
			else
#endif // SVG_SUPPORT_SVG_IN_IMAGE
#ifdef SVG_SUPPORT_FOREIGNOBJECT
			if (type == Markup::SVGE_FOREIGNOBJECT)
			{
				result = HandleForeignObject(info, img_vp, quality);
			}
			else
#endif // SVG_SUPPORT_FOREIGNOBJECT
			{
				if (imageURL && type == Markup::SVGE_IMAGE)
				{
					result = HandleRasterImage(info, imageURL, img_vp, quality);
				}
			}
			result = OpStatus::OK; // Needed if no xlink:href exists
		}
		break;

	case Markup::SVGE_VIDEO:
#ifdef SVG_SUPPORT_MEDIA
		result = HandleVideo(info, allow_draw, update_bbox ? &bbox : NULL);
#endif // SVG_SUPPORT_MEDIA
		break;

	default:
		// Unknown element
		OP_ASSERT(0);
		result = OpStatus::ERR;
		break;
	}

	if (update_bbox)
	{
		info.context->SetBBox(bbox);
		info.context->SetBBoxIsValid(TRUE);
	}

	if (OpStatus::IsMemoryError(result))
		return result;
	else if (result == OpSVGStatus::SKIP_CHILDREN)
		return result;
	else
		return OpStatus::OK;
}

OP_STATUS SVGLayoutObject::HandleExternalContent(SVGElementInfo& info)
{
	BOOL update_bbox = CalculateBBox();

	SVGPaintNode* paint_node = info.paint_node;
	if (!paint_node)
		return OpStatus::OK;

	BOOL allow_draw = m_canvas->AllowDraw(IgnorePointerEvents());

	//
	// Here is brief description of the flags used below and what they signify:
	//
	// allow_draw: Determined by 'visibility' and 'pointer-events' -
	//             iff we care about 'pointer-events' (which we don't do
	//             when drawing)
	//
	// update_bbox: The bounding box of this element should be updated.
	//              This flag does _not_ depend on the allow_draw flag,
	//              i.e. even if the geometry is invisible the bbox
	//              should be calculated

	OP_STATUS result = OpStatus::OK;
	HTML_Element* layouted_elm = info.layouted;

	const HTMLayoutProperties& props = *info.props->GetProps();
	const SvgProperties *svg_props = props.svg;

	SVGBoundingBox bbox;

	Markup::Type type = layouted_elm->Type();
	switch (type)
	{
#ifdef SVG_SUPPORT_FOREIGNOBJECT
	case Markup::SVGE_FOREIGNOBJECT:
#endif // SVG_SUPPORT_FOREIGNOBJECT
	case Markup::SVGE_ANIMATION:
	case Markup::SVGE_IMAGE:
		{
			SVGRect img_vp;
			if (update_bbox || allow_draw)
			{
				RETURN_IF_ERROR(SVGUtils::GetImageValues(layouted_elm, GetValueContext(), img_vp));
			}

			if (update_bbox)
			{
				bbox.minx = img_vp.x;
				bbox.miny = img_vp.y;
				bbox.maxx = img_vp.x + SVGNumber::max_of(img_vp.width, 0);
				bbox.maxy = img_vp.y + SVGNumber::max_of(img_vp.height, 0);
			}

			if (!allow_draw || img_vp.width < 0 || img_vp.height < 0)
				break;

			SVGCanvas::ImageRenderQuality quality = SVGGetImageQualityFromProps(props);

			// OpStatus::Ignore(FillViewport(info, img_vp));
			UINT32 viewport_fillcolor = 0;
			UINT8 viewport_fillopacity = 0;

			if (svg_props->viewportfill.GetColorType() != SVGColor::SVGCOLOR_NONE)
			{
				viewport_fillcolor = svg_props->viewportfill.GetRGBColor();
				viewport_fillopacity = svg_props->viewportfillopacity;
			}

			URL *imageURL = NULL;
			SVGDocumentContext* doc_ctx = info.context->IsShadowNode() ? AttrValueStore::GetSVGDocumentContext(layouted_elm) : m_doc_ctx;
			RETURN_IF_ERROR(AttrValueStore::GetXLinkHREF(doc_ctx->GetURL(), layouted_elm, imageURL));

			if ((!imageURL || imageURL->IsEmpty()) && (type != Markup::SVGE_FOREIGNOBJECT || AttrValueStore::HasObject(layouted_elm, Markup::XLINKA_HREF, NS_IDX_XLINK)))
			{
				result = OpSVGStatus::ELEMENT_IS_INVISIBLE;
				break;
			}

#ifdef SVG_SUPPORT_SVG_IN_IMAGE
			if (imageURL && type == Markup::SVGE_IMAGE)
			{
				// Note that this will only detect the simplest of
				// loops, however DoImageOrAnimation uses the
				// elementresolver loop detection to catch the
				// trickier cases.
				if (*imageURL == m_doc_ctx->GetURL())
				{
					SVG_NEW_ERROR(layouted_elm);
					SVG_REPORT_ERROR((UNI_L("The 'image' element cannot reference the SVG file that contains it.")));

					result = OpStatus::ERR;
					break;
				}
			}

			if (type != Markup::SVGE_FOREIGNOBJECT &&
				imageURL &&
				(imageURL->ContentType() == URL_SVG_CONTENT ||
				 imageURL->ContentType() == URL_XML_CONTENT))
			{
				SVGVectorImageNode* image_paint_node = static_cast<SVGVectorImageNode*>(paint_node);
				image_paint_node->SetBackgroundColor(viewport_fillcolor);
				image_paint_node->SetBackgroundOpacity(viewport_fillopacity);
				image_paint_node->SetImageQuality((SVGPainter::ImageRenderQuality)quality);
				result = HandleVectorImage(info, img_vp);
			}
			else
#endif // SVG_SUPPORT_SVG_IN_IMAGE
#ifdef SVG_SUPPORT_FOREIGNOBJECT
			if (type == Markup::SVGE_FOREIGNOBJECT)
			{
				SVGForeignObjectNode* fo_paint_node = static_cast<SVGForeignObjectNode*>(paint_node);
				fo_paint_node->SetBackgroundColor(viewport_fillcolor);
				fo_paint_node->SetBackgroundOpacity(viewport_fillopacity);
				result = HandleForeignObject(info, img_vp, quality);
			}
			else
#endif // SVG_SUPPORT_FOREIGNOBJECT
			{
				if (imageURL && type == Markup::SVGE_IMAGE)
				{
					SVGRasterImageNode* image_paint_node = static_cast<SVGRasterImageNode*>(paint_node);
					image_paint_node->SetBackgroundColor(viewport_fillcolor);
					image_paint_node->SetBackgroundOpacity(viewport_fillopacity);

					result = HandleRasterImage(info, imageURL, img_vp, quality);
				}
			}
			result = OpStatus::OK; // Needed if no xlink:href exists
		}
		break;

	case Markup::SVGE_VIDEO:
#ifdef SVG_SUPPORT_MEDIA
		result = HandleVideo(info, allow_draw, update_bbox ? &bbox : NULL);
#endif // SVG_SUPPORT_MEDIA
		break;

	default:
		// Unknown element
		OP_ASSERT(0);
		result = OpStatus::ERR;
		break;
	}

	// FIXME: Disregards geometric conditions (w <= 0 et.c)

	// visibility represented as opacity == 0
	if (!allow_draw)
		paint_node->SetVisible(FALSE);

	m_current_container->Insert(info.paint_node, m_current_pred);
	m_current_pred = info.paint_node;

	if (update_bbox)
	{
		info.context->SetBBox(bbox);
		info.context->SetBBoxIsValid(TRUE);
	}

	UpdateElement(info);

	if (OpStatus::IsMemoryError(result))
		return result;
	else if (result == OpSVGStatus::SKIP_CHILDREN)
		return result;
	else
		return OpStatus::OK;
}

OP_STATUS SVGIntersectionObject::HandleExternalContent(SVGElementInfo& info)
{
	if (!m_canvas->AllowPointerEvents())
		return OpStatus::OK;

	OP_STATUS status = SVGVisualTraversalObject::HandleExternalContent(info);

	if (m_canvas->GetPointerEvents() == SVGPOINTEREVENTS_BOUNDINGBOX &&
		m_canvas->GetRenderMode() == SVGCanvas::RENDERMODE_INTERSECT_POINT)
	{
		OP_ASSERT(info.context->IsBBoxValid());
		SVGBoundingBox bbox = info.context->GetBBox();

		OpStatus::Ignore(m_canvas->DrawRect(bbox.minx, bbox.miny,
											bbox.maxx - bbox.minx,
											bbox.maxy - bbox.miny, 0, 0));
	}

	return status;
}

OP_STATUS SVGIntersectionObject::HandleRasterImage(SVGElementInfo& info, URL* imageURL, const SVGRect& img_vp, int quality)
{
	if ((URLStatus)imageURL->GetAttribute(URL::KLoadStatus, TRUE) != URL_LOADED)
		return OpStatus::OK;

	SVGAspectRatio* ar = NULL;
	AttrValueStore::GetPreserveAspectRatio(info.layouted, ar);

	OP_STATUS result = SVGUtils::DrawImageFromURL(m_doc_ctx, imageURL, info.layouted, info.props,
												  m_canvas, img_vp, ar, quality);

	if (OpStatus::IsError(result) && result != OpSVGStatus::DATA_NOT_LOADED_ERROR)
	{
		// Broken image. Not much we can do. At least create something
		// to intersect with (modulo clipping).
		OpStatus::Ignore(m_canvas->DrawRect(img_vp.x, img_vp.y, img_vp.width, img_vp.height, 0, 0));
	}
	return result;
}

OP_STATUS SVGLayoutObject::HandleRasterImage(SVGElementInfo& info, URL* imageURL, const SVGRect& img_vp, int quality)
{
	if ((URLStatus)imageURL->GetAttribute(URL::KLoadStatus, TRUE) == URL_LOADED)
	{
		// Just assure that we have allocated a UrlImageContentProvider, skipping that while painting
		UrlImageContentProvider	*provider = UrlImageContentProvider::FindImageContentProvider(*imageURL);
		if (!provider)
		{
			provider = OP_NEW(UrlImageContentProvider, (*imageURL));
			if (!provider)
				return OpStatus::ERR_NO_MEMORY;
		}

		HEListElm* hle = info.layouted->GetHEListElm(FALSE);
		if (!hle)
			return OpStatus::ERR;

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
			hle->Display(m_doc_ctx->GetDocument(), AffinePos(), 20000, 1000000, FALSE, FALSE);

			Image img = provider->GetImage();

			OpStatus::Ignore(img.OnLoadAll(provider));
		}
		provider->DecRef();
	}

	SVGAspectRatio* ar = NULL;
	AttrValueStore::GetPreserveAspectRatio(info.layouted, ar);

	// Allocated in the caller, shouldn't fail here
	SVGRasterImageNode* image_paint_node = static_cast<SVGRasterImageNode*>(info.paint_node);

	image_paint_node->UpdateState(m_canvas);

	// FIXME: Clipping

	image_paint_node->SetImageAspect(ar);
	image_paint_node->SetImageViewport(img_vp);
	image_paint_node->SetImageQuality((SVGPainter::ImageRenderQuality)quality);
	image_paint_node->SetImageContext(*imageURL, info.layouted, m_doc_ctx->GetDocument());

	// This will get us extents
	OpStatus::Ignore(m_canvas->DrawRect(img_vp.x, img_vp.y, img_vp.width, img_vp.height, 0, 0));
	return OpStatus::OK;
}

OP_STATUS SVGLayoutObject::EnterTextNode(SVGElementInfo& info)
{
	// Reset the state of the paint node - text nodes are a bit
	// special in this regard since they get essentially all state
	// from their parent text container, but this should make sure
	// that state is consistent anyway (and if it isn't used then no
	// major harm done).
	info.paint_node->Reset();
	static_cast<SVGTextContentPaintNode*>(info.paint_node)->ResetRuns();

	// Reset the dirty rect
	if (!IsInMarker())
		m_canvas->ResetDirtyRect();

	return SVGVisualTraversalObject::EnterTextNode(info);
}

OP_STATUS SVGLayoutObject::PushContainerState(SVGElementInfo& info)
{
	ContainerState* cs = OP_NEW(ContainerState, (m_current_container, m_current_pred, m_container_stack));
	if (!cs)
		return OpStatus::ERR_NO_MEMORY;

	m_container_stack = cs;
	info.has_container_state = 1;
	return OpStatus::OK;
}

void SVGLayoutObject::PopContainerState(SVGElementInfo& info)
{
	if (!m_container_stack || !info.has_container_state)
		return;

	m_current_container = m_container_stack->container;
	m_current_pred = m_container_stack->pred;

	ContainerState* next = m_container_stack->next;
	OP_DELETE(m_container_stack);
	m_container_stack = next;
}

OP_STATUS SVGLayoutObject::PushReferringNode(SVGElementInfo& info)
{
	if (m_referring_paint_node)
		RETURN_IF_ERROR(m_referrer_stack.Push(m_referring_paint_node));

	m_referring_paint_node = info.paint_node;
	return OpStatus::OK;
}

void SVGLayoutObject::PopReferringNode()
{
	if (!m_referrer_stack.IsEmpty())
		m_referring_paint_node = m_referrer_stack.Pop();
	else
		m_referring_paint_node = NULL;
}

#ifdef SVG_SUPPORT_STENCIL
OP_STATUS SVGVisualTraversalObject::EnterClipPath(SVGElementInfo& info)
{
	const SvgProperties *svg_props = info.props->GetProps()->svg;

	if (!svg_props->clippath.info.is_none)
		RETURN_IF_ERROR(SetupClipOrMask(info, svg_props->clippath, Markup::SVGE_CLIPPATH, RESOURCE_CLIPPATH));

	SVGUnitsType units = SVGUNITS_USERSPACEONUSE; // this is the default
	AttrValueStore::GetUnits(info.traversed, Markup::SVGA_CLIPPATHUNITS, units, SVGUNITS_USERSPACEONUSE);

	if (units == SVGUNITS_OBJECTBBOX)
	{
		SVGBoundingBox bbox;
		RETURN_IF_ERROR(GetElementBBox(m_stencil_target.doc_ctx,
									   m_stencil_target.element,
									   m_stencil_target.viewport, bbox));

		if (!bbox.IsEmpty())
		{
			// [ (maxx-minx) 0 0 (maxy-miny) minx miny ]
			SVGMatrix objbboxTransform;
			objbboxTransform.SetValues(bbox.maxx - bbox.minx, 0, 0,
									   bbox.maxy - bbox.miny, bbox.minx, bbox.miny);
			m_canvas->ConcatTransform(objbboxTransform);
		}
	}

	return m_canvas->BeginClip();
}

OP_STATUS SVGLayoutObject::EnterClipPath(SVGElementInfo& info)
{
	RETURN_IF_ERROR(SVGVisualTraversalObject::EnterClipPath(info));

	RETURN_IF_ERROR(PushContainerState(info));

	// m_referring_paint_node should point to the node we're currently
	// processing, and thus the node we should attach the clippath node to
	OP_ASSERT(m_referring_paint_node);

	SVGClipPathNode* cp_node = static_cast<SVGClipPathNode*>(info.paint_node);
	OP_ASSERT(cp_node == m_pending_nodes.Last());
	cp_node->Out();

	m_referring_paint_node->SetClipPath(cp_node);

	SVGUnitsType units = SVGUNITS_USERSPACEONUSE; // this is the default
	AttrValueStore::GetUnits(info.traversed, Markup::SVGA_CLIPPATHUNITS, units, SVGUNITS_USERSPACEONUSE);

	cp_node->SetBBoxRelative(units == SVGUNITS_OBJECTBBOX);

	m_current_container = cp_node;
	m_current_pred = NULL;

	return OpStatus::OK;
}

OP_STATUS SVGVisualTraversalObject::LeaveClipPath(SVGElementInfo& info)
{
	if (info.IsSet(RESOURCE_CLIPPATH))
		CleanStencils(info);

	m_canvas->EndClip();
	return OpStatus::OK;
}

OP_STATUS SVGLayoutObject::LeaveClipPath(SVGElementInfo& info)
{
	info.context->ClearInvalidState();

	// Try to optimize the paint node
	SVGClipPathNode* cp_node = static_cast<SVGClipPathNode*>(info.paint_node);
	cp_node->Optimize();

	PopContainerState(info);

	return SVGVisualTraversalObject::LeaveClipPath(info);
}

OP_STATUS SVGLayoutObject::EnterMask(SVGElementInfo& info, const TransformInfo& tinfo)
{
	SVGUnitsType maskunits = SVGUNITS_OBJECTBBOX; // this is the default
	SVGUnitsType maskcontentunits = SVGUNITS_USERSPACEONUSE; // default
	HTML_Element* mask_elm = info.layouted;

	// Error handling = fallback to default values
	OpStatus::Ignore(AttrValueStore::GetUnits(mask_elm, Markup::SVGA_MASKUNITS, maskunits, SVGUNITS_OBJECTBBOX));
	OpStatus::Ignore(AttrValueStore::GetUnits(mask_elm, Markup::SVGA_MASKCONTENTUNITS, maskcontentunits, SVGUNITS_USERSPACEONUSE));

	SVGMatrix objbboxTransform;
	if (maskunits == SVGUNITS_OBJECTBBOX || maskcontentunits == SVGUNITS_OBJECTBBOX)
	{
		SVGBoundingBox bbox;
		RETURN_IF_ERROR(GetElementBBox(m_stencil_target.doc_ctx,
									   m_stencil_target.element,
									   m_stencil_target.viewport, bbox));

		// Create transform: [ (maxx-minx) 0 0 (maxy-miny) minx miny ]
		objbboxTransform.SetValues(bbox.maxx - bbox.minx, 0, 0,
								   bbox.maxy - bbox.miny, bbox.minx, bbox.miny);
	}

	const SVGValueContext& vcxt = GetValueContext();

	// Setup default values
	SVGRect mask_rect(-SVGNumber(0.1) /*-10%*/,
					  -SVGNumber(0.1) /*-10%*/,
					  SVGNumber(1.2) /*120%*/,
					  SVGNumber(1.2) /*120%*/);

	if (maskunits == SVGUNITS_USERSPACEONUSE)
	{
		mask_rect.width *= vcxt.viewport_width;
		mask_rect.height *= vcxt.viewport_height;
		mask_rect.x *= vcxt.viewport_width;
		mask_rect.y *= vcxt.viewport_height;
	}

	SVGUtils::GetResolvedLengthWithUnits(mask_elm, Markup::SVGA_WIDTH, SVGLength::SVGLENGTH_X,
										 maskunits, vcxt, mask_rect.width);
	SVGUtils::GetResolvedLengthWithUnits(mask_elm, Markup::SVGA_HEIGHT, SVGLength::SVGLENGTH_Y,
										 maskunits, vcxt, mask_rect.height);
	SVGUtils::GetResolvedLengthWithUnits(mask_elm, Markup::SVGA_X, SVGLength::SVGLENGTH_X,
										 maskunits, vcxt, mask_rect.x);
	SVGUtils::GetResolvedLengthWithUnits(mask_elm, Markup::SVGA_Y, SVGLength::SVGLENGTH_Y,
										 maskunits, vcxt, mask_rect.y);

	if (maskunits == SVGUNITS_OBJECTBBOX)
		mask_rect = objbboxTransform.ApplyToRect(mask_rect);

	/* Skip rendering element if width or height is negative or zero */
	if (mask_rect.height <= 0 || mask_rect.width <= 0)
		return OpSVGStatus::ELEMENT_IS_INVISIBLE;

	// Create a new clipping rectangle with
	// the position (x,y) and the size width x height
	if (OpStatus::IsSuccess(m_canvas->AddClipRect(mask_rect)))
		// Maybe not strictly a viewport, but it seems the overload of
		// the term isn't grave enough to warrant a unique bit.
		info.Set(RESOURCE_VIEWPORT_CLIP);

	// Set coordinate system for mask content (if applicable)
	if (maskcontentunits == SVGUNITS_OBJECTBBOX)
		m_canvas->ConcatTransform(objbboxTransform);

	RETURN_IF_ERROR(m_canvas->BeginMask());

	RETURN_IF_ERROR(PushContainerState(info));

	// m_referring_paint_node should point to the node we're currently
	// processing, and thus the node we should attach the clippath node to
	OP_ASSERT(m_referring_paint_node);

	SVGMaskNode* mask_node = static_cast<SVGMaskNode*>(info.paint_node);
	OP_ASSERT(mask_node == m_pending_nodes.Last());
	mask_node->Out();

	m_referring_paint_node->SetMask(mask_node);

	// Reset the paint-node
	mask_node->Reset();

	mask_node->SetClipRect(mask_rect);
	mask_node->SetBBoxRelative(maskcontentunits == SVGUNITS_OBJECTBBOX);

	// FIXME: Handle tinfo.needs_reset?
	if (info.has_transform)
		mask_node->SetTransform(tinfo.transform);
	else
		mask_node->ResetTransform();

	const HTMLayoutProperties& props = *info.props->GetProps();

	mask_node->SetOpacity(SVGUtils::GetOpacity(info.layouted, props, FALSE));

	m_current_container = mask_node;
	m_current_pred = NULL;

	return OpStatus::OK;
}

OP_STATUS SVGLayoutObject::LeaveMask(SVGElementInfo& info)
{
	m_canvas->EndMask();

	if (info.IsSet(RESOURCE_VIEWPORT_CLIP))
		m_canvas->RemoveClip();

	info.context->ClearInvalidState();

	// Mark the paint nodes extents as invalid - this assumes that
	// children of this element was actual changed (extents-wise), so
	// this should probably be conditionalized on state-flags
	if (info.paint_node)
		info.paint_node->MarkExtentsDirty();

	PopContainerState(info);
	return OpStatus::OK;
}
#endif // SVG_SUPPORT_STENCIL

static BOOL HasMutablePaintNode(int element_type)
{
	return
		element_type == Markup::SVGE_FOREIGNOBJECT ||
		element_type == Markup::SVGE_ANIMATION ||
		element_type == Markup::SVGE_IMAGE;
}

OP_STATUS SVGLayoutObject::PrepareElement(SVGElementInfo& info)
{
	OP_ASSERT(info.paint_node == NULL);

	// info has had a paint node assigned already, nothing to do here
	if (info.paint_node)
		return OpStatus::OK;

	if (!m_detached_operation)
	{
		if (SVGPaintNode* current_paint_node = info.context->GetPaintNode())
		{
			// A paint node exists for this element/context, check if
			// it can be reused. A node is reusable either if the
			// corresponding element/context is not dirty enough, or
			// if the element/context can not mutate it's node.
			// Would like to be a bit more selective here (not
			// recreate nodes unless sufficiently dirty), but it seems
			// there are certain races conditions that can prevent us
			// from doing that (like an update triggered by a font
			// change racing with the completion of a URL load).
			if (/* info.GetInvalidState() < INVALID_ADDED || */
				!HasMutablePaintNode(info.layouted->Type()))
			{
				info.paint_node = current_paint_node;
				return OpStatus::OK;
			}
		}
	}

	// Sufficiently dirty or lacking a paint node - create one

	RETURN_IF_ERROR(CreatePaintNode(info));

	AttachPaintNode(info);
	return OpStatus::OK;
}

OP_STATUS SVGLayoutObject::CreatePaintNode(SVGElementInfo& info)
{
	HTML_Element* real_elm = info.layouted;
	bool oom_check = true;

	switch (real_elm->Type())
	{
		// Text-related elements
	case Markup::SVGE_TEXT:
	case Markup::SVGE_TEXTAREA:
	case Markup::SVGE_TSPAN:
		info.paint_node = OP_NEW(SVGTextAttributePaintNode, ());
		break;

	case Markup::SVGE_TEXTPATH:
		info.paint_node = OP_NEW(SVGTextPathNode, ());
		break;

	case Markup::SVGE_TREF:
		info.paint_node = OP_NEW(SVGTextReferencePaintNode, ());
		break;

	case Markup::SVGE_ALTGLYPH:
		info.paint_node = OP_NEW(SVGAltGlyphNode, ());
		break;

	case Markup::HTE_TEXT:
	case Markup::HTE_TEXTGROUP:
		info.paint_node = OP_NEW(SVGTextContentPaintNode, ());
		break;

		// Basic shapes
	case Markup::SVGE_CIRCLE:
	case Markup::SVGE_ELLIPSE:
	case Markup::SVGE_LINE:
	case Markup::SVGE_RECT:
		info.paint_node = OP_NEW(SVGPrimitiveNode, ());
		break;

	case Markup::SVGE_PATH:
		info.paint_node = OP_NEW(SVGPathNode, ());
		break;

	case Markup::SVGE_POLYGON:
	case Markup::SVGE_POLYLINE:
		info.paint_node = OP_NEW(SVGPolygonNode, ());
		break;

		// 'External' content
	case Markup::SVGE_ANIMATION:
	case Markup::SVGE_IMAGE:
		{
			SVGDocumentContext* doc_ctx = m_doc_ctx;
			if (info.context->IsShadowNode())
				doc_ctx = AttrValueStore::GetSVGDocumentContext(info.layouted);

			URL *image_url = NULL;
			oom_check = false;
			if (OpStatus::IsSuccess(AttrValueStore::GetXLinkHREF(doc_ctx->GetURL(), info.layouted, image_url)))
			{
				if (image_url &&
					(image_url->ContentType() == URL_SVG_CONTENT ||
					 image_url->ContentType() == URL_XML_CONTENT))
				{
					oom_check = true;
					info.paint_node = OP_NEW(SVGVectorImageNode, ());
				}
				else if (info.layouted->Type() == Markup::SVGE_IMAGE)
				{
					oom_check = true;
					info.paint_node = OP_NEW(SVGRasterImageNode, ());
				}
				// FIXME: What about <animation> without href or
				// unhandled content type? Or erroneous href:s at all?
			}
		}
		break;

#ifdef SVG_SUPPORT_MEDIA
	case Markup::SVGE_VIDEO:
		info.paint_node = OP_NEW(SVGVideoNode, ());
		break;
#endif // SVG_SUPPORT_MEDIA

#ifdef SVG_SUPPORT_FOREIGNOBJECT
	case Markup::SVGE_FOREIGNOBJECT:
		if (AttrValueStore::HasObject(real_elm, Markup::XLINKA_HREF, NS_IDX_XLINK))
			info.paint_node = OP_NEW(SVGForeignObjectHrefNode, ());
		else
			info.paint_node = OP_NEW(SVGForeignObjectNode, ());
		break;
#endif // SVG_SUPPORT_FOREIGNOBJECT

#ifdef SVG_SUPPORT_STENCIL
	case Markup::SVGE_CLIPPATH:
		info.paint_node = OP_NEW(SVGClipPathNode, ());
		break;

	case Markup::SVGE_MASK:
		info.paint_node = OP_NEW(SVGMaskNode, ());
		break;
#endif // SVG_SUPPORT_STENCIL

		// Container elements
	case Markup::SVGE_SYMBOL:
		// Is there any case where we could be wanting (or trying) to
		// create paint nodes for non-shadow 'symbol's? They shouldn't
		// be rendered after all...
		OP_ASSERT(info.context->IsShadowNode());
	case Markup::SVGE_G:
	case Markup::SVGE_A:
	case Markup::SVGE_SWITCH:
		info.paint_node = OP_NEW(SVGCompositePaintNode, ());
		break;

	case Markup::SVGE_USE:
		info.paint_node = OP_NEW(SVGOffsetCompositePaintNode, ());
		break;

	case Markup::SVGE_SVG:
		info.paint_node = OP_NEW(SVGViewportCompositePaintNode, ());
		break;

	default:
		oom_check = false;
		break;
	}

	if (info.paint_node)
	{
		OP_ASSERT(!info.paint_node->InList());

		info.paint_node->Into(&m_pending_nodes);
	}
	else if (oom_check)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

void SVGLayoutObject::AttachPaintNode(SVGElementInfo& info)
{
	if (!m_detached_operation)
	{
		SVGPaintNode* current_paint_node = info.context->GetPaintNode();
		if (current_paint_node != info.paint_node)
		{
			OP_ASSERT(m_pending_nodes.Last() == info.paint_node);
			
			if (current_paint_node && current_paint_node->IsBuffered())
				info.paint_node->CopyBufferData(current_paint_node);
			
			OP_DELETE(current_paint_node); // Will detach and clean

			info.context->SetPaintNode(info.paint_node);
		}

		// Paint node is attached and thus no longer pending
		m_pending_nodes.RemoveAll();
	}
}

void SVGLayoutObject::DetachPaintNode(SVGElementInfo& info)
{
	if (info.paint_node)
		info.paint_node->Detach();

	if (m_detached_operation)
	{
		OP_DELETE(info.paint_node);
		info.paint_node = NULL;
	}

	// More than one pending node really ought to be abnormal
	OP_ASSERT(m_pending_nodes.Empty() || m_pending_nodes.Last() == info.paint_node);

	// Remove any pending paint node
	m_pending_nodes.Clear();
}

OP_STATUS SVGTraversalObject::EnterUseElement(SVGElementInfo& info, const TransformInfo& tinfo)
{
	return EnterContainer(info, tinfo);
}

OP_STATUS SVGLayoutObject::EnterUseElement(SVGElementInfo& info, const TransformInfo& tinfo)
{
	SVGOffsetCompositePaintNode* ofs_pn = static_cast<SVGOffsetCompositePaintNode*>(info.paint_node);
	ofs_pn->SetOffset(tinfo.offset);

	return EnterContainer(info, tinfo);
}

OP_STATUS SVGLayoutObject::EnterContainer(SVGElementInfo& info, const TransformInfo& tinfo)
{
	SVGCompositePaintNode* comp_pn = static_cast<SVGCompositePaintNode*>(info.paint_node);

#ifdef SVG_SUPPORT_FILTERS
	const SvgProperties* svg_props = info.props->GetProps()->svg;
	const SVGEnableBackground* eb = svg_props->GetEnableBackground();

	// "enable-background: accumulate" is illegal if no
	// "enable-background: new ..." has been previously seen
	BOOL new_bk = (eb->GetType() == SVGEnableBackground::SVGENABLEBG_NEW);
	if (m_background_count || new_bk)
		m_background_count++;

	comp_pn->SetHasBackgroundLayer(m_background_count > 0, new_bk);
#endif // SVG_SUPPORT_FILTERS

	RETURN_IF_ERROR(PushContainerState(info));

	if (m_current_container)
		m_current_container->Insert(info.paint_node, m_current_pred);

	m_current_container = comp_pn;
	m_current_pred = NULL;

	// Reset the child list if we are re-layouting this subtree
	if (info.GetInvalidState() > INVALID_STRUCTURE)
		m_current_container->Clear();

	return EnterElement(info, tinfo);
}

OP_STATUS SVGLayoutObject::EnterTextGroup(SVGElementInfo& info)
{
	// Reset the dirty rect
	if (!IsInMarker())
		m_canvas->ResetDirtyRect();

	return SVGVisualTraversalObject::EnterTextGroup(info);
}

OP_STATUS SVGLayoutObject::EnterElement(SVGElementInfo& info, const TransformInfo& tinfo)
{
	// Only applies to paint-urls in this case?
	SVGDocumentContext* layouted_elm_doc_ctx = AttrValueStore::GetSVGDocumentContext(info.layouted);
	OP_ASSERT(layouted_elm_doc_ctx);

	// Could we change these to just take m_doc_ctx (instead of
	// fetching it from the element) ?
	SVGUtils::LoadExternalReferences(layouted_elm_doc_ctx, info.layouted);
	SVGUtils::LoadExternalReferencesFromCascade(layouted_elm_doc_ctx, info.layouted, info.props);

	if (SVGUtils::IsDisplayNone(info.layouted, info.props))
		return InvisibleElement(info);

	const HTMLayoutProperties& props = *info.props->GetProps();
	const SvgProperties* svg_props = props.svg;

	// Reset the paint-node
	SVGPaintNode* paint_node = info.paint_node;

	if (paint_node)
	{
		paint_node->Reset();

		if (info.has_transform)
		{
			paint_node->SetTransform(tinfo.transform);
			paint_node->SetRefTransform(tinfo.needs_reset);
		}
		else
		{
			paint_node->ResetTransform();
		}

		paint_node->SetOpacity(SVGUtils::GetOpacity(info.layouted, props,
													m_doc_ctx->GetElement() == info.layouted));
	}

	// Reset the dirty rect
	if (!IsInMarker())
		m_canvas->ResetDirtyRect();

	SetupGeometricProperties(info);
	SetupPaintProperties(info);

#ifdef SVG_SUPPORT_STENCIL
	if (!svg_props->clippath.info.is_none || !svg_props->mask.info.is_none)
	{
		m_stencil_target.doc_ctx = m_doc_ctx;
		m_stencil_target.element = info.traversed;
		m_stencil_target.viewport = GetCurrentViewport();
	}

	if (!svg_props->clippath.info.is_none)
		RETURN_IF_ERROR(SetupClipOrMask(info, svg_props->clippath, Markup::SVGE_CLIPPATH, RESOURCE_CLIPPATH));

	if (!svg_props->mask.info.is_none)
		RETURN_IF_ERROR(SetupClipOrMask(info, svg_props->mask, Markup::SVGE_MASK, RESOURCE_MASK));
#endif // SVG_SUPPORT_STENCIL

	if (!m_current_buffer_element && info.context->ShouldBuffer(svg_props))
	{
		// Tag the paint node as 'buffering'
		if (paint_node)
		{
			paint_node->SetBufferKey(info.context);
			paint_node->ValidateBuffered();

			if (paint_node->IsBuffered())
			{
				if (info.GetInvalidState() >= INVALID_SUBTREE)
				{
					paint_node->SetBufferDirty();
				}
			}
		}

		// Indicate that we have an element that is buffering - to
		// avoid nested buffers
		m_current_buffer_element = info.traversed;
	}
	else if (paint_node)
	{
		// Reset buffering for this node
		paint_node->ResetBuffering();
	}

#ifdef SVG_SUPPORT_FILTERS
	// Need to setup the filter to be able to determine extents
	if (!svg_props->filter.info.is_none)
		RETURN_IF_ERROR(SetupFilter(info));
#endif // SVG_SUPPORT_FILTERS

	return OpStatus::OK;
}

OP_STATUS SVGLayoutObject::LeaveElement(SVGElementInfo& info)
{
	info.context->ClearInvalidState();

	// If we are leaving a buffered subtree, reset the current buffer
	// element marker.
	if (m_current_buffer_element && m_current_buffer_element == info.traversed)
		m_current_buffer_element = NULL;

#ifdef SVG_SUPPORT_STENCIL
	// Remove any clip-extents that we pushed
	if (info.IsSet(RESOURCE_CLIPPATH) ||
		info.IsSet(RESOURCE_MASK) ||
		info.IsSet(RESOURCE_VIEWPORT_CLIP))
		CleanStencils(info);
#endif // SVG_SUPPORT_STENCIL

	return OpStatus::OK;
}

OP_STATUS SVGVisualTraversalObject::LeaveContainer(SVGElementInfo& info)
{
#ifdef SVG_SUPPORT_STENCIL
	// Remove any clip-extents that we pushed
	if (info.IsSet(RESOURCE_CLIPPATH) ||
		info.IsSet(RESOURCE_MASK) ||
		info.IsSet(RESOURCE_VIEWPORT_CLIP))
		CleanStencils(info);
#endif // SVG_SUPPORT_STENCIL

	return OpStatus::OK;
}

OP_STATUS SVGLayoutObject::LeaveContainer(SVGElementInfo& info)
{
	info.context->ClearInvalidState();

	// Mark the paint nodes extents as invalid - this assumes that
	// children of this element were actually changed (extents-wise),
	// so this should probably be conditionalized on state-flags
	SVGCompositePaintNode* paint_node = static_cast<SVGCompositePaintNode*>(info.paint_node);
	if (paint_node)
	{
		paint_node->MarkExtentsDirty();
		paint_node->InvalidateHasNonscalingStrokeCache();
	}
	OP_STATUS status = LeaveElement(info);

#ifdef SVG_SUPPORT_FILTERS
	// Decrement the number of backgrounds started (~the number of
	// containers since the first 'enable-background: new ...').
	if (m_background_count > 0)
		m_background_count--;
#endif // SVG_SUPPORT_FILTERS

	PopContainerState(info);

	// If the paint node wasn't detached, set it to be the predecessor
	// of the following node
	if (paint_node && paint_node->GetParent())
		m_current_pred = paint_node;

	return status;
}

OP_STATUS SVGLayoutObject::LeaveTextNode(SVGElementInfo& info)
{
	// Mark this node
	info.context->ClearInvalidState();
	return OpStatus::OK;
}

OP_STATUS SVGLayoutObject::LeaveTextGroup(SVGElementInfo& info)
{
	// Mark this node
	info.context->ClearInvalidState();
	if (m_textinfo)
	{
		info.context->AddBBox(m_textinfo->bbox.Get());
	}

	info.context->SetBBoxIsValid(TRUE);
	return SVGVisualTraversalObject::LeaveTextGroup(info);
}

OP_STATUS SVGVisualTraversalObject::EnterTextElement(SVGElementInfo& info)
{
	// Create m_textinfo
	if (!m_textinfo)
		RETURN_IF_ERROR(CreateTextInfo(static_cast<SVGTextRootContainer*>(info.context)));

	RETURN_IF_ERROR(EnterTextRoot(info));

	SetupTextOverflow(info);

	// Calculate/get extents
	SetupExtents(info);

	return OpStatus::OK;
}

OP_STATUS SVGVisualTraversalObject::LeaveTextElement(SVGElementInfo& info)
{
	OP_STATUS status = OpStatus::OK;

	if (m_textinfo)
	{
		PopTextProperties(info);

		status = LeaveTextRoot(info);

#ifdef SVG_SUPPORT_EDITABLE
		FlushCaret(info);
#endif // SVG_SUPPORT_EDITABLE
	}

	DestroyTextInfo();
	return status;
}

OP_STATUS SVGVisualTraversalObject::EnterTextArea(SVGElementInfo& info)
{
	if (!m_textinfo)
		RETURN_IF_ERROR(CreateTextInfo(static_cast<SVGTextRootContainer*>(info.context)));

	RETURN_IF_ERROR(EnterTextRoot(info));

	// Fetch values for the text area
	SetupTextAreaElement(info);

	SetupTextOverflow(info);

	return OpStatus::OK;
}

OP_STATUS SVGVisualTraversalObject::LeaveTextArea(SVGElementInfo& info)
{
	OP_STATUS status = OpStatus::OK;

	if (m_textinfo)
	{
		PopTextProperties(info);

		status = LeaveTextRoot(info);

#ifdef SVG_SUPPORT_EDITABLE
		FlushCaret(info);
#endif // SVG_SUPPORT_EDITABLE
	}

	DestroyTextInfo();
	return status;
}

OP_STATUS SVGLayoutObject::LeaveTextArea(SVGElementInfo& info)
{
	if (m_canvas->GetPointerEvents() == SVGPOINTEREVENTS_BOUNDINGBOX)
	{
		SVGTextAttributePaintNode* textattr_node =
			static_cast<SVGTextAttributePaintNode*>(info.paint_node);

		// Forcing the node to have extents so intersection will
		// consider the element.
		textattr_node->ForceExtents(info.context->GetBBox());
	}

	return SVGVisualTraversalObject::LeaveTextArea(info);
}

OP_STATUS SVGVisualTraversalObject::EnterTextContainer(SVGElementInfo& info)
{
	PushTextProperties(info);

	// if we're inside a textArea we don't need to call SetupExtents
	if (!m_textarea_info)
		SetupExtents(info);

	// Apply any baseline adjustments (baseline-shift)
	ApplyBaselineAdjustment(info, GetValueContext(), m_textinfo);

	return OpStatus::OK;
}

OP_STATUS SVGLayoutObject::EnterTextContainer(SVGElementInfo& info)
{
	OP_NEW_DBG("EnterTextContainer", "svg_bidi_trav");
	OP_DBG(("EnterTextContainer"));

	info.context->SetBBoxIsValid(FALSE);

	OP_STATUS status = SVGVisualTraversalObject::EnterTextContainer(info);

	if (OpStatus::IsSuccess(status) && info.paint_node)
	{
		SetTextAttributes(info);

		SVGTextAttributePaintNode* attr_paint_node = static_cast<SVGTextAttributePaintNode*>(info.paint_node);

		RETURN_IF_ERROR(m_textinfo->PushAttributeNode(attr_paint_node));

		info.has_text_attrs = 1;

		RETURN_IF_ERROR(PushContainerState(info));

		m_current_container->Insert(attr_paint_node, m_current_pred);

		m_current_container = attr_paint_node;
		m_current_pred = NULL;

		// Reset the child list if we are re-layouting this subtree
		if (info.GetInvalidState() > INVALID_STRUCTURE)
			m_current_container->Clear();
	}
	return status;
}

OP_STATUS SVGVisualTraversalObject::EnterTextNode(SVGElementInfo& info)
{
	// Setup the fontdescriptor
	// We need to do this here because if we have for instance
	// <text>some text <tspan>foo</tspan> more text</text>, and the
	// tspan changes the font metrics, they won't be restored for the
	// text node following the tspan.
	m_textinfo->font_desc.SetFont(m_canvas->GetLogFont());

	return OpStatus::OK;
}

//
// Element traversal specifications
//

/* static */
BOOL SVGElementContext::FailingRequirements(HTML_Element* element)
{
	/**
	 * "The test attributes do not effect the 'mask', 'clipPath', 'gradient' and 'pattern' elements.
	 *  The test attributes on a referenced element do not affect the rendering of the referencing element.
	 *  The test attributes do not effect the 'defs', and 'cursor' elements as they are not part of the
	 *  rendering tree."
	 */
#ifdef DEBUG_ENABLE_OPASSERT
	Markup::Type element_type = SVGUtils::GetElementToLayout(element)->Type();
#endif // DEBUG_ENABLE_OPASSERT
	OP_ASSERT(element_type != Markup::SVGE_MASK &&
			  element_type != Markup::SVGE_CLIPPATH &&
			  element_type != Markup::SVGE_LINEARGRADIENT &&
			  element_type != Markup::SVGE_RADIALGRADIENT &&
			  element_type != Markup::SVGE_PATTERN &&
			  element_type != Markup::SVGE_DEFS &&
			  element_type != Markup::SVGE_CURSOR);

	SVGElementContext* context = AttrValueStore::GetSVGElementContext_Unsafe(element);
	if (context)
		return context->IsFilteredByRequirements();

	// Maybe create a context here, and tag it, to avoid
	// redoing the entire check?
	return !SVGUtils::ShouldProcessElement(SVGUtils::GetElementToLayout(element));
}

/* static */
BOOL SVGElementContext::NeedsResources(HTML_Element* element)
{
	BOOL ext_required = !!AttrValueStore::GetEnumValue(element, Markup::SVGA_EXTERNALRESOURCESREQUIRED,
													   SVGENUM_BOOLEAN, FALSE);
	if (!ext_required)
		return FALSE;

	return !SVGUtils::HasLoadedRequiredExternalResources(element);
}

// Yes, this is silly, but allows the 'invisible' element to act as a
// container in some cases
// Same code as in SVGContainer (letting it inherit was too big of a risk)
BOOL SVGInvisibleElement::EvaluateChild(HTML_Element* child)
{
	if (child->GetNsType() != NS_SVG)
		return FALSE;

	Markup::Type child_type = child->Type();

	// Could use SVGUtils::IsExternalProxyElement but since we've already checked the namespace...
	if (child->GetInserted() == HE_INSERTED_BY_SVG && child_type == Markup::SVGE_FOREIGNOBJECT)
		return FALSE;

	HTML_Element* real_node = child;
	if (SVGUtils::IsShadowType(child_type))
	{
		real_node = SVGUtils::GetElementToLayout(child);
		child_type = real_node->Type();
	}
	else if (child_type == Markup::SVGE_SYMBOL)
	{
		// Only needs to be evaluated if it's a shadowtree
		return FALSE;
	}

	if (!Markup::IsRealElement(child_type) &&
		child_type != Markup::SVGE_SHADOW &&
		child_type != Markup::SVGE_ANIMATED_SHADOWROOT)
		return FALSE;

	// Non-visual?
	if (SVGUtils::IsAlwaysIrrelevantForDisplay(child_type))
		return FALSE;

	// Text, but not text-root?
	if (SVGUtils::IsTextChildType(child_type))
		return FALSE;

	// Failing requirements?
	if (FailingRequirements(child))
		return FALSE;

	// Waiting for resources?
	if (NeedsResources(child))
		return FALSE;

	return TRUE;
}

BOOL SVGContainer::EvaluateChild(HTML_Element* child)
{
	if (child->GetNsType() != NS_SVG)
		return FALSE;

	Markup::Type child_type = child->Type();

	// Could use SVGUtils::IsExternalProxyElement but since we've already checked the namespace...
	if (child->GetInserted() == HE_INSERTED_BY_SVG && child_type == Markup::SVGE_FOREIGNOBJECT)
		return FALSE;

	HTML_Element* real_node = child;
	if (SVGUtils::IsShadowType(child_type))
	{
		real_node = SVGUtils::GetElementToLayout(child);
		child_type = real_node->Type();
	}
	else if (child_type == Markup::SVGE_SYMBOL)
	{
		// Only needs to be evaluated if it's a shadowtree
		return FALSE;
	}

	if (!Markup::IsRealElement(child_type) &&
		child_type != Markup::SVGE_SHADOW &&
		child_type != Markup::SVGE_ANIMATED_SHADOWROOT)
		return FALSE;

	// Non-visual?
	if (SVGUtils::IsAlwaysIrrelevantForDisplay(child_type))
		return FALSE;

	// Text, but not text-root?
	if (SVGUtils::IsTextChildType(child_type))
		return FALSE;

	// Failing requirements?
	if (FailingRequirements(child))
		return FALSE;

	// Waiting for resources?
	if (NeedsResources(child))
		return FALSE;

	return TRUE;
}

BOOL SVGTransparentContainer::EvaluateChild(HTML_Element* child)
{
	HTML_Element* parent = GetHtmlElement()->Parent();
	if (!parent)
		return FALSE; // Probably detached

	SVGElementContext* parent_context = AttrValueStore::AssertSVGElementContext(parent);
	if (!parent_context)
		return FALSE; // If the parent does not have a context, it probably OOM

	// Evaluate the child in the context of the parent element
	return parent_context->EvaluateChild(child);
}

BOOL SVGSwitchElement::EvaluateChild(HTML_Element* child)
{
	if (child->IsText())
		return FALSE;

	return SVGTransparentContainer::EvaluateChild(child);
}

BOOL SVGTextElement::EvaluateChild(HTML_Element* child)
{
	if (child->GetNsType() != NS_SVG && !child->IsText())
		return FALSE;

	Markup::Type child_type = child->Type();
	if (child_type == Markup::SVGE_BASE_SHADOWROOT)
		return FALSE;

	HTML_Element* real_node = child;
	if (SVGUtils::IsShadowType(child_type))
	{
		real_node = SVGUtils::GetElementToLayout(child);
		child_type = real_node->Type();
	}

	if (real_node->IsText())
		return TRUE;

	if (!(child_type == Markup::SVGE_A ||
		  child_type == Markup::SVGE_TSPAN ||
		  child_type == Markup::SVGE_TREF ||
		  child_type == Markup::SVGE_TEXTPATH ||
		  child_type == Markup::SVGE_ALTGLYPH ||
		  child_type == Markup::SVGE_SWITCH))
		return FALSE;

	if (FailingRequirements(child))
		return FALSE;

	if (NeedsResources(child))
		return FALSE;

	return TRUE;
}

BOOL SVGTextAreaElement::EvaluateChild(HTML_Element* child)
{
	if (child->GetNsType() != NS_SVG && !child->IsText())
		return FALSE;

	Markup::Type child_type = child->Type();
	if (child_type == Markup::SVGE_BASE_SHADOWROOT)
		return FALSE;

	HTML_Element* real_node = child;
	if (SVGUtils::IsShadowType(child_type))
	{
		real_node = SVGUtils::GetElementToLayout(child);
		child_type = real_node->Type();
	}

	if (real_node->IsText())
		return TRUE;

	if (!(child_type == Markup::SVGE_A ||
		  child_type == Markup::SVGE_TSPAN ||
		  child_type == Markup::SVGE_TBREAK ||
		  child_type == Markup::SVGE_SWITCH))
		return FALSE;

	if (FailingRequirements(child))
		return FALSE;

	if (NeedsResources(child))
		return FALSE;

	return TRUE;
}

BOOL SVGTextContainer::EvaluateChild(HTML_Element* child)
{
	if (child->GetNsType() != NS_SVG && !child->IsText())
		return FALSE;

	Markup::Type child_type = child->Type();
	if (child_type == Markup::SVGE_BASE_SHADOWROOT)
		return FALSE;

	HTML_Element* real_node = child;
	if (SVGUtils::IsShadowType(child_type))
	{
		real_node = SVGUtils::GetElementToLayout(child);
		child_type = real_node->Type();
	}

	if (real_node->IsText())
		return TRUE;

	if (!(child_type == Markup::SVGE_A ||
		  child_type == Markup::SVGE_TSPAN ||
		  child_type == Markup::SVGE_TBREAK ||
		  child_type == Markup::SVGE_TREF ||
		  child_type == Markup::SVGE_TEXTPATH ||
		  child_type == Markup::SVGE_ALTGLYPH ||
		  child_type == Markup::SVGE_SWITCH))
		return FALSE;

	if (FailingRequirements(child))
		return FALSE;

	if (NeedsResources(child))
		return FALSE;

	return TRUE;
}

BOOL SVGAltGlyphElement::EvaluateChild(HTML_Element* child)
{
	if (child->GetNsType() != NS_SVG && !child->IsText())
		return FALSE;

	Markup::Type child_type = child->Type();
	if (child_type == Markup::SVGE_BASE_SHADOWROOT)
		return FALSE;

	HTML_Element* real_node = child;
	if (SVGUtils::IsShadowType(child_type))
	{
		real_node = SVGUtils::GetElementToLayout(child);
		child_type = real_node->Type();
	}

	if (real_node->IsText())
		return TRUE;

	return FALSE;
}

BOOL SVGResourceContainer::EvaluateChild(HTML_Element* child)
{
	return SVGContainer::EvaluateChild(child);
}

#ifdef SVG_SUPPORT_STENCIL
BOOL SVGClipPathElement::EvaluateChild(HTML_Element* child)
{
	if (!SVGContainer::EvaluateChild(child))
		return FALSE;

	HTML_Element* real_node = SVGUtils::GetElementToLayout(child);
	if (SVGUtils::IsContainerElement(real_node) &&
		real_node->Type() != Markup::SVGE_USE)
		return FALSE;

	return TRUE;
}
#endif // SVG_SUPPORT_STENCIL

void SVGContainer::AppendChild(HTML_Element* child, List<SVGElementContext>* childlist)
{
	SVGElementContext* context = AttrValueStore::AssertSVGElementContext(child);
	if (!context)
		return;

	// If this child was not previously a child of this container,
	// then "upgrade" its dirtyness so that we layout it.
	if (!m_children.HasLink(context))
	{
		// This will happen when:
		//
		// * a subtree is inserted (parsing/via DOM),
		// * or if it is display: none.
		//
		// In the former case this will cause the invalid state to be
		// explicitly propagated (since at least the root of the
		// inserted subtree should be INVALID_ADDED already).
		//
		// In the later case it is expected to be removed again fairly
		// swiftly via the Enter-chain.
		context->UpgradeInvalidState(INVALID_ADDED);
	}

	context->Out();
	context->Into(childlist);
}

void SVGSwitchElement::AppendChild(HTML_Element* child, List<SVGElementContext>* childlist)
{
	// A more strict test would be 'length == 1', but based on how we
	// build the list this should suffice.
	if (!childlist->Empty())
		return;

	SVGContainer::AppendChild(child, childlist);
}

void SVGContainer::SelectChildren(SVGElementInfo& info)
{
	List<SVGElementContext> childlist;

	HTML_Element* child = info.traversed->FirstChild();
	while (child)
	{
		if (EvaluateChild(child))
			AppendChild(child, &childlist);

		child = child->Suc();
	}

	// Remove any "residual" - now rejected - children.
	m_children.RemoveAll();

	// Update the containers list.
	m_children.Append(&childlist);
}

OP_STATUS SVGContainer::Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	RETURN_IF_ERROR(traversal_object->PushState(info));

	TransformInfo tinfo;
	if (SVGTraversalObject::FetchTransform(info, tinfo))
		traversal_object->ApplyTransform(info, tinfo);

	return traversal_object->EnterContainer(info, tinfo);
}

OP_STATUS SVGContainer::Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	traversal_object->LeaveContainer(info);

	return traversal_object->PopState(info);
}

OP_STATUS SVGSVGElement::Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	ViewportInfo vpinfo;
	traversal_object->CalculateSVGViewport(info, vpinfo);

	RETURN_IF_ERROR(traversal_object->PushState(info));

	traversal_object->PushSVGViewport(info, vpinfo);
	traversal_object->ApplyUserTransform(info, vpinfo);

	TransformInfo tinfo;
	tinfo.transform = vpinfo.transform;

	SVGTraversalObject::FetchTransform(info, tinfo);
	traversal_object->ApplyTransform(info, tinfo);

	return traversal_object->EnterContainer(info, tinfo);
}

OP_STATUS SVGSVGElement::Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	traversal_object->LeaveContainer(info);
	traversal_object->PopSVGViewport(info);

	return traversal_object->PopState(info);
}

OP_STATUS SVGUseElement::Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	if (OpStatus::IsError(traversal_object->ValidateUse(info)))
		return OpSVGStatus::SKIP_ELEMENT;

	RETURN_IF_ERROR(traversal_object->PushState(info));

	TransformInfo tinfo;
	BOOL has_transform = SVGTraversalObject::FetchTransform(info, tinfo);
	BOOL has_offset = SVGTraversalObject::FetchUseOffset(info, traversal_object->GetValueContext(), tinfo);
	if (has_offset || has_transform)
		traversal_object->ApplyTransform(info, tinfo);

	return traversal_object->EnterUseElement(info, tinfo);
}

OP_STATUS SVGUseElement::Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	traversal_object->LeaveContainer(info);

	return traversal_object->PopState(info);
}

#ifdef SVG_SUPPORT_STENCIL
OP_STATUS SVGClipPathElement::Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	RETURN_IF_ERROR(traversal_object->PushState(info));

	TransformInfo tinfo;
	if (SVGTraversalObject::FetchTransform(info, tinfo))
		traversal_object->ApplyTransform(info, tinfo);

	return traversal_object->EnterClipPath(info);
}

OP_STATUS SVGClipPathElement::Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	traversal_object->LeaveClipPath(info);

	return traversal_object->PopState(info);
}

OP_STATUS SVGMaskElement::Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	RETURN_IF_ERROR(traversal_object->PushState(info));

	TransformInfo tinfo;
	if (SVGTraversalObject::FetchTransform(info, tinfo))
		traversal_object->ApplyTransform(info, tinfo);

	return traversal_object->EnterMask(info, tinfo);
}

OP_STATUS SVGMaskElement::Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	traversal_object->LeaveMask(info);

	return traversal_object->PopState(info);
}
#endif // SVG_SUPPORT_STENCIL

OP_STATUS SVGSymbolInstanceElement::Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	// We shouldn't be traversing non-instance symbol elements
	OP_ASSERT(info.traversed != info.layouted);

	RETURN_IF_ERROR(traversal_object->PushState(info));

	ViewportInfo vpinfo;
	traversal_object->CalculateSymbolViewport(info, vpinfo);
	traversal_object->PushSymbolViewport(info, vpinfo);

	TransformInfo tinfo;
	tinfo.transform = vpinfo.transform;

	SVGTraversalObject::FetchTransform(info, tinfo);
	traversal_object->ApplyTransform(info, tinfo);

	return traversal_object->EnterContainer(info, tinfo);
}

OP_STATUS SVGSymbolInstanceElement::Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	traversal_object->LeaveContainer(info);
	traversal_object->PopSymbolViewport(info);

	return traversal_object->PopState(info);
}

OP_STATUS SVGGraphicsElement::Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	RETURN_IF_ERROR(traversal_object->PushState(info));

	TransformInfo tinfo;
	if (SVGTraversalObject::FetchTransform(info, tinfo))
		traversal_object->ApplyTransform(info, tinfo);

	return traversal_object->EnterElement(info, tinfo);
}

OP_STATUS SVGGraphicsElement::HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	return traversal_object->HandleGraphicsElement(info);
}

OP_STATUS SVGGraphicsElement::Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	traversal_object->LeaveElement(info);

	return traversal_object->PopState(info);
}

OP_STATUS SVGExternalContentElement::HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	return traversal_object->HandleExternalContent(info);
}

OP_STATUS SVGTextContainer::Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	OP_NEW_DBG("SVGTextContainer::Enter", "svg_blah");
	PrintElmType(info.layouted);
	RETURN_IF_ERROR(traversal_object->PushState(info));

	TransformInfo tinfo;
	if (SVGTraversalObject::FetchTransform(info, tinfo))
		traversal_object->ApplyTransform(info, tinfo);

	OP_STATUS status = traversal_object->EnterElement(info, tinfo);
	RETURN_IF_ERROR(traversal_object->EnterTextContainer(info));
	return status;
}

OP_STATUS SVGTextContainer::HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	return traversal_object->HandleTextElement(info);
}

OP_STATUS SVGTextNode::Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	return traversal_object->EnterTextNode(info);
}

OP_STATUS SVGTextNode::HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	return traversal_object->HandleTextElement(info);
}

OP_STATUS SVGTextNode::Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	return traversal_object->LeaveTextNode(info);
}

OP_STATUS SVGTextGroupElement::Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	return traversal_object->EnterTextGroup(info);
}

OP_STATUS SVGTextGroupElement::Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	return traversal_object->LeaveTextGroup(info);
}

OP_STATUS SVGTBreakElement::HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	return traversal_object->HandleTextElement(info);
}

OP_STATUS SVGTextContainer::Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	OP_NEW_DBG("SVGTextContainer::Leave", "svg_blah");
	PrintElmType(info.layouted);
	traversal_object->LeaveTextContainer(info);
	traversal_object->LeaveElement(info);

	return traversal_object->PopState(info);
}

OP_STATUS SVGTextElement::Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	RETURN_IF_ERROR(traversal_object->PushState(info));

	TransformInfo tinfo;
	if (SVGTraversalObject::FetchTransform(info, tinfo))
		traversal_object->ApplyTransform(info, tinfo);

	OP_STATUS status = traversal_object->EnterElement(info, tinfo);

	OP_STATUS text_status = traversal_object->EnterTextElement(info);
	if (OpStatus::IsMemoryError(text_status))
		status = text_status;

	return status;
}

OP_STATUS SVGTextElement::Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	traversal_object->LeaveTextElement(info);
	traversal_object->LeaveElement(info);

	return traversal_object->PopState(info);
}

OP_STATUS SVGTextAreaElement::Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	RETURN_IF_ERROR(traversal_object->PushState(info));

	TransformInfo tinfo;
	if (SVGTraversalObject::FetchTransform(info, tinfo))
		traversal_object->ApplyTransform(info, tinfo);

	OP_STATUS status = traversal_object->EnterElement(info, tinfo);

	OP_STATUS text_status = traversal_object->EnterTextArea(info);
	if (OpStatus::IsMemoryError(text_status))
		status = text_status;

	return status;
}

OP_STATUS SVGTextAreaElement::Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	traversal_object->LeaveTextArea(info);
	traversal_object->LeaveElement(info);

	return traversal_object->PopState(info);
}

OP_STATUS SVGAElement::Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	RETURN_IF_ERROR(SVGContainer::Enter(traversal_object, info));

	return traversal_object->EnterAnchor(info);
}

OP_STATUS SVGAElement::Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	traversal_object->LeaveAnchor(info);

	return SVGContainer::Leave(traversal_object, info);
}

OP_STATUS SVGAElement::HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	return SVGContainer::HandleContent(traversal_object, info);
}

OP_STATUS SVGSwitchElement::Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	return SVGContainer::Enter(traversal_object, info);
}

OP_STATUS SVGSwitchElement::Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	return SVGContainer::Leave(traversal_object, info);
}

OP_STATUS SVGSwitchElement::HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info)
{
	return SVGContainer::HandleContent(traversal_object, info);
}

OP_STATUS SVGLayoutObject::HandlePaintserver(SVGElementInfo& info, HTML_Element* paintserver, BOOL is_fill,
											 BOOL& ignore_fallback)
{
	if (paintserver->IsMatchingType(Markup::SVGE_LINEARGRADIENT, NS_SVG) ||
		paintserver->IsMatchingType(Markup::SVGE_RADIALGRADIENT, NS_SVG))
	{
#ifdef SVG_SUPPORT_GRADIENTS
		SVGGradient *g = NULL;
		RETURN_IF_ERROR(SVGGradient::Create(paintserver, m_resolver, m_doc_ctx, GetValueContext(), &g));

		if (g)
		{
# ifdef _DEBUG
			g->Print();
# endif	// _DEBUG

			// It is necessary that at least two stops are defined to have a gradient effect.
			// If no stops are defined, then painting shall occur as if 'none' were specified
			// as the paint style. If one stop is defined, then paint with the solid color fill
			// using the color defined for that gradient stop.
			unsigned num_stops = g->GetNumStops();
			if (num_stops == 0)
			{
				if (is_fill)
				{
					m_canvas->EnableFill(SVGCanvasState::USE_NONE);
				}
				else
				{
					m_canvas->EnableStroke(SVGCanvasState::USE_NONE);
				}

				SVGPaintServer::DecRef(g);
			}
			else if (num_stops == 1)
			{
				const SVGStop* s = g->GetStop(0);
				UINT32 color = s ? s->GetColorRGB() : 0;

				if (is_fill)
				{
					m_canvas->SetFillColor(color);
					m_canvas->EnableFill(SVGCanvasState::USE_COLOR);
				}
				else
				{
					m_canvas->SetStrokeColor(color);
					m_canvas->EnableStroke(SVGCanvasState::USE_COLOR);
				}

				SVGPaintServer::DecRef(g);

				ignore_fallback = TRUE;
			}
			else
			{
				if (is_fill)
				{
					m_canvas->SetFillPaintServer(g);
					m_canvas->EnableFill(SVGCanvasState::USE_PSERVER);
				}
				else
				{
					m_canvas->SetStrokePaintServer(g);
					m_canvas->EnableStroke(SVGCanvasState::USE_PSERVER);
				}
			}
		}
#endif // SVG_SUPPORT_GRADIENTS
	}
	else if (paintserver->IsMatchingType(Markup::SVGE_PATTERN, NS_SVG))
	{
#ifdef SVG_SUPPORT_PATTERNS
		SVGPattern *p = NULL;
		OP_STATUS status = SVGPattern::Create(paintserver, info.traversed, m_resolver,
											  m_doc_ctx, GetValueContext(), &p);

		RETURN_IF_ERROR(status);

		if (p)
		{
			if (is_fill)
			{
				m_canvas->SetFillPaintServer(p);
				m_canvas->EnableFill(SVGCanvasState::USE_PSERVER);
			}
			else
			{
				m_canvas->SetStrokePaintServer(p);
				m_canvas->EnableStroke(SVGCanvasState::USE_PSERVER);
			}
		}

		if (status == OpSVGStatus::COLOR_IS_NONE)
		{
			ignore_fallback = TRUE;
			return status;
		}
#endif // SVG_SUPPORT_PATTERNS
	}
	else if (paintserver->IsMatchingType(Markup::SVGE_SOLIDCOLOR, NS_SVG))
	{
		Head prop_list;

		// Make sure cascades in referenced external documents use the correct HLDocProfile, e.g see bug 271514.
		SVGDocumentContext* ps_elm_docctx = AttrValueStore::GetSVGDocumentContext(paintserver);
		if (!ps_elm_docctx)
			return OpStatus::ERR;

		HLDocProfile* hld_profile = ps_elm_docctx->GetHLDocProfile();
		if (!hld_profile)
			return OpStatus::ERR;

		LayoutProperties* layprops =
			LayoutProperties::CreateCascade(paintserver, prop_list, LAYOUT_WORKPLACE(hld_profile));
		if (!layprops)
			return OpStatus::ERR_NO_MEMORY;

		const HTMLayoutProperties& props = *layprops->GetProps();
		const SvgProperties *svg_props = props.svg;

		UINT32 solidcolor = 0;
		switch (svg_props->solidcolor.GetColorType())
		{
		case SVGColor::SVGCOLOR_RGBCOLOR:
			solidcolor = svg_props->solidcolor.GetRGBColor();
			break;
		case SVGColor::SVGCOLOR_RGBCOLOR_ICCCOLOR:
			solidcolor = svg_props->solidcolor.GetICCColor();
			break;
		case SVGColor::SVGCOLOR_CURRENT_COLOR:
			OP_ASSERT(!"Hah! currentColor not resolved");
		default:
			break;
		}

		UINT8 a = svg_props->solidopacity;

		layprops = NULL;
		prop_list.Clear();

		if (is_fill)
		{
			m_canvas->SetFillColor(solidcolor);
			m_canvas->SetFillOpacity(a);
			m_canvas->EnableFill(SVGCanvasState::USE_COLOR);
		}
		else
		{
			m_canvas->SetStrokeColor(solidcolor);
			m_canvas->SetStrokeOpacity(a);
			m_canvas->EnableStroke(SVGCanvasState::USE_COLOR);
		}

		ignore_fallback = TRUE;
	}

	return OpStatus::OK;
}

OP_STATUS SVGVisualTraversalObject::SetupPaint(SVGElementInfo& info, const SVGPaint* paint, BOOL isFill)
{
	switch (paint->GetPaintType())
	{
	case SVGPaint::UNKNOWN:
	case SVGPaint::INHERIT:
		break;	// Ok, do nothing
#ifndef SVG_SUPPORT_PAINTSERVERS
	case SVGPaint::URI_NONE:
#endif // !SVG_SUPPORT_PAINTSERVERS
	case SVGPaint::NONE:
		if (isFill)
		{
			m_canvas->EnableFill(SVGCanvasState::USE_NONE);
		}
		else
		{
			m_canvas->EnableStroke(SVGCanvasState::USE_NONE);
		}
		break;

	case SVGPaint::CURRENT_COLOR:
	case SVGPaint::URI_CURRENT_COLOR:
		OP_ASSERT(!"Hah! currentColor not resolved");
		/* Fall-through */

#ifndef SVG_SUPPORT_PAINTSERVERS
	case SVGPaint::URI_RGBCOLOR:
	case SVGPaint::URI_RGBCOLOR_ICCCOLOR:
#endif // !SVG_SUPPORT_PAINTSERVERS
	case SVGPaint::RGBCOLOR_ICCCOLOR:
	case SVGPaint::RGBCOLOR:
		{
			UINT32 paint_color = paint->GetRGBColor();
			if (isFill)
			{
				m_canvas->SetFillColor(paint_color);
				m_canvas->EnableFill(SVGCanvasState::USE_COLOR);
			}
			else
			{
				m_canvas->SetStrokeColor(paint_color);
				m_canvas->EnableStroke(SVGCanvasState::USE_COLOR);
			}
		}
		break;

#ifdef SVG_SUPPORT_PAINTSERVERS
	case SVGPaint::URI_NONE:
	case SVGPaint::URI_RGBCOLOR:
	case SVGPaint::URI_RGBCOLOR_ICCCOLOR:
#endif // SVG_SUPPORT_PAINTSERVERS
	case SVGPaint::URI:
		{
#ifdef SVG_SUPPORT_PAINTSERVERS
			SVGURL svg_url(paint->GetURI(), paint->GetURI() ? uni_strlen(paint->GetURI()) : 0, URL());
			HTML_Element *paintserver = SVGUtils::FindURLRelReferredNode(m_resolver, m_doc_ctx, info.traversed, svg_url);

			if (!paintserver)
				m_doc_ctx->RegisterUnresolvedDependency(info.traversed);

			BOOL ignore_fallback = FALSE;
			if (paintserver)
			{
				m_doc_ctx->RegisterDependency(info.traversed, paintserver);

				OP_STATUS status = HandlePaintserver(info, paintserver, isFill, ignore_fallback);
				if (OpStatus::IsError(status))
					// Use fallback if possible, else use what's set
					paintserver = NULL;
				else if (status == OpSVGStatus::COLOR_IS_NONE)
				{
					if (isFill)
					{
						m_canvas->EnableFill(SVGCanvasState::USE_NONE);
					}
					else
					{
						m_canvas->EnableStroke(SVGCanvasState::USE_NONE);
					}
				}
			}
			else if (paint->GetPaintType() == SVGPaint::URI_NONE)
			{
				if (isFill)
				{
					m_canvas->EnableFill(SVGCanvasState::USE_NONE);
				}
				else
				{
					m_canvas->EnableStroke(SVGCanvasState::USE_NONE);
				}
			}
			else if (paint->GetPaintType() == SVGPaint::URI)
			{
				// In SVG 1.2 we should use the default value (inherit
				// from parent?)
				if (isFill)
				{
					m_canvas->SetFillColorRGB(0, 0, 0);
					m_canvas->EnableFill(SVGCanvasState::USE_COLOR);
				}
				else
				{
					m_canvas->EnableStroke(SVGCanvasState::USE_NONE);
				}
			}

			if (!ignore_fallback)
			{
				if (paint->GetPaintType() == SVGPaint::URI_RGBCOLOR ||
					paint->GetPaintType() == SVGPaint::URI_RGBCOLOR_ICCCOLOR)
				{
					// We have a fallback we want to use if
					// resolution/setup of the paintserver
					// fails. Consider only doing this if those
					// particular steps fail - to make this code more
					// obvious.
					UINT32 paint_color = paint->GetRGBColor();
					if (isFill)
					{
						m_canvas->SetFillColor(paint_color);

						if (!paintserver)
							m_canvas->EnableFill(SVGCanvasState::USE_COLOR);
					}
					else
					{
						m_canvas->SetStrokeColor(paint_color);

						if (!paintserver)
							m_canvas->EnableStroke(SVGCanvasState::USE_COLOR);
					}
				}
			}
#else
		return OpSVGStatus::NOT_SUPPORTED_IN_TINY;
#endif // SVG_SUPPORT_PAINTSERVERS
		}
	}

	return OpStatus::OK;
}

void SVGLayoutObject::UpdateElement(SVGElementInfo& info)
{
	if (IsInMarker())
		return;

	OpRect current_screen_box = m_canvas->GetDirtyRect();

	if (!current_screen_box.IsEmpty())
		m_layout_state->AddInvalid(current_screen_box);

	if (info.paint_node)
		info.paint_node->MarkExtentsDirty();

	m_canvas->ResetDirtyRect();
}

SVGElementContext* SVGRenderingTreeChildIterator::FirstChild(SVGElementInfo& info)
{
	SVGElementContext* context = info.context;
	if (info.GetInvalidState() >= INVALID_STRUCTURE)
		context->RebuildChildList(info);

	return context->FirstChild();
}

SVGElementContext* SVGRenderingTreeChildIterator::NextChild(SVGElementContext* parent_context,
															SVGElementContext* child_context)
{
	return child_context->Suc();
}

SVGElementContext* SVGLogicalTreeChildIterator::GetNextChildContext(SVGElementContext* parent_context,
																	HTML_Element* candidate_child)
{
	while (candidate_child)
	{
		if (parent_context->EvaluateChild(candidate_child))
			return AttrValueStore::AssertSVGElementContext(candidate_child);

		candidate_child = candidate_child->Suc();
	}
	return NULL;
}

SVGElementContext* SVGLogicalTreeChildIterator::FirstChild(SVGElementInfo& info)
{
	return GetNextChildContext(info.context, info.traversed->FirstChild());
}

SVGElementContext* SVGLogicalTreeChildIterator::NextChild(SVGElementContext* parent_context,
														  SVGElementContext* child_context)
{
	return GetNextChildContext(parent_context, child_context->GetHtmlElement()->Suc());
}

SVGElementContext* SVGTreePathChildIterator::FirstChild(SVGElementInfo& info)
{
	// If the path vector is empty, there are no more elements to traverse
	if (m_treepath->GetCount() == 0)
		return NULL;

	// The first element in the list is the next child
	HTML_Element* child = m_treepath->Get(0);

	// Assert that it is a reasonable child of the current element
	OP_ASSERT(info.traversed == child->ParentActual());

	// Assert that it isn't a textnode
	OP_ASSERT(!(child->IsText() || SVGUtils::GetElementToLayout(child)->IsText()));

	return AttrValueStore::AssertSVGElementContext(child);
}

SVGElementContext* SVGTreePathChildIterator::NextChild(SVGElementContext* parent_context,
													   SVGElementContext* child_context)
{
	return NULL; // Will we ever need to return non-NULL here?
}

// Transfer properties that affect the geometry of objects to the drawing state
OP_STATUS SVGVisualTraversalObject::SetupGeometricProperties(SVGElementInfo& info)
{
	const HTMLayoutProperties& props = *info.props->GetProps();
	const SvgProperties *svg_props = props.svg;

	// If we had an OOM or some other error when doing the cascade return it here
	RETURN_IF_ERROR(svg_props->GetError());

	if (props.visibility == CSS_VALUE_visible)
		m_canvas->SetVisibility(SVGVISIBILITY_VISIBLE);
	else if (props.visibility == CSS_VALUE_collapse)
		m_canvas->SetVisibility(SVGVISIBILITY_COLLAPSE);
	else if (props.visibility == CSS_VALUE_hidden)
		m_canvas->SetVisibility(SVGVISIBILITY_HIDDEN);

	SVGVisibilityType visibility;
	BOOL css_override, is_inherit;
	if (OpStatus::IsSuccess(AttrValueStore::GetVisibility(info.layouted, visibility,
														  &css_override, &is_inherit)))
	{
		if (css_override && !is_inherit)
			m_canvas->SetVisibility(visibility);
	}

#ifdef SVG_SUPPORT_STENCIL
	if (m_canvas->GetClipMode() != SVGCanvas::STENCIL_CONSTRUCT)
#endif // SVG_SUPPORT_STENCIL
	{
		m_canvas->SetPointerEvents((SVGPointerEvents)svg_props->rendinfo.pointerevents);

		const SVGValueContext& vcxt = GetValueContext();

		if (svg_props->HasDashArray() &&
			svg_props->dasharray_seq > m_canvas->DashArraySeq())
		{
			// Take the SVGLength** vector and transform it into a
			// SVGNumber vector and pass it along to the canvas. Note:
			// "If the sum of the <length>'s is zero, then the stroke
			// is rendered as if a value of none were specified"

			SVGNumber* dash_array = NULL;
			unsigned dash_array_size = 0;

			const SVGDashArray* da = svg_props->GetDashArray();
			if (!da->is_none)
			{
				dash_array_size = da->dash_array_size;
				dash_array = OP_NEWA(SVGNumber, dash_array_size);
				if (!dash_array)
					return OpStatus::ERR_NO_MEMORY;

				SVGNumber dash_len = 0;
				for (unsigned i = 0; i < dash_array_size; i++)
				{
					const SVGLength& len = da->dash_array[i];
					SVGNumber resolved_len = SVGUtils::ResolveLength(len, SVGLength::SVGLENGTH_OTHER, vcxt);
					dash_len += resolved_len;
					dash_array[i] = resolved_len;
				}

				if (dash_len <= 0)
				{
					OP_DELETEA(dash_array);
					dash_array = NULL;
					dash_array_size = 0;
				}
			}

			m_canvas->SetDashes(dash_array, dash_array_size);
			m_canvas->SetDashArraySeq(svg_props->dasharray_seq);
		}

		m_canvas->SetVectorEffect((SVGVectorEffect)svg_props->rendinfo.vector_effect);

		if (svg_props->HasDashOffset())
			m_canvas->SetDashOffset(SVGUtils::ResolveLength(svg_props->dashoffset, SVGLength::SVGLENGTH_OTHER, vcxt));

		m_canvas->SetLineCap((SVGCapType)svg_props->rendinfo.linecap);
		m_canvas->SetLineJoin((SVGJoinType)svg_props->rendinfo.linejoin);
		m_canvas->SetMiterLimit(svg_props->miterlimit);

		if (svg_props->HasLineWidth())
			m_canvas->SetLineWidth(SVGUtils::ResolveLength(svg_props->linewidth, SVGLength::SVGLENGTH_OTHER, vcxt));
	}

	return OpStatus::OK;
}

// Transfer properties that affect the drawing of objects to the drawing state
OP_STATUS SVGVisualTraversalObject::SetupPaintProperties(SVGElementInfo& info)
{
	const HTMLayoutProperties& props = *info.props->GetProps();
	const SvgProperties *svg_props = props.svg;

	m_canvas->SetShapeRendering(svg_props->GetShapeRendering());
	m_canvas->SetAntialiasingQuality(m_canvas->GetShapeRenderingVegaQuality());

#ifdef SVG_SUPPORT_STENCIL
	if (m_canvas->GetClipMode() == SVGCanvas::STENCIL_CONSTRUCT)
	{
		m_canvas->SetFillRule((SVGFillRule)svg_props->rendinfo.cliprule);

		m_canvas->EnableFill(SVGCanvasState::USE_COLOR);
		m_canvas->SetFillOpacity(255);
		m_canvas->SetFillColorRGB(0, 0, 0);
		m_canvas->EnableStroke(SVGCanvasState::USE_NONE);
	}
	else
#endif // SVG_SUPPORT_STENCIL
	{
		m_canvas->SetFillRule((SVGFillRule)svg_props->rendinfo.fillrule);
		m_canvas->SetFillOpacity(svg_props->fillopacity);
		m_canvas->SetPointerEvents((SVGPointerEvents)svg_props->rendinfo.pointerevents);

		m_canvas->SetStrokeOpacity(svg_props->strokeopacity);

		if (svg_props->HasFill() && svg_props->fill_seq > m_canvas->FillSeq())
		{
			RETURN_IF_ERROR(SetupPaint(info, svg_props->GetFill(), TRUE));
			m_canvas->SetFillSeq(svg_props->fill_seq);
		}
		if (svg_props->HasStroke() && svg_props->stroke_seq > m_canvas->StrokeSeq())
		{
			RETURN_IF_ERROR(SetupPaint(info, svg_props->GetStroke(), FALSE));
			m_canvas->SetStrokeSeq(svg_props->stroke_seq);
		}
	}
	return OpStatus::OK;
}

#ifdef SVG_SUPPORT_MARKERS
void SVGVisualTraversalObject::ResolveMarker(SVGElementInfo& info,
											 const SVGURLReference& urlref,
											 HTML_Element*& out_elm)
{
	SVGURL svg_url(urlref, URL());
	HTML_Element* marker_elm = SVGUtils::FindURLRelReferredNode(m_resolver, m_doc_ctx,
																info.traversed, svg_url);

	if (!marker_elm)
	{
		m_doc_ctx->RegisterUnresolvedDependency(info.traversed);
	}
	else if (marker_elm->IsMatchingType(Markup::SVGE_MARKER, NS_SVG))
	{
		m_doc_ctx->RegisterDependency(info.traversed, marker_elm);

		out_elm = marker_elm;
	}
}

OP_STATUS SVGLayoutObject::LayoutMarker(HTML_Element* sel_marker, SVGMarkerNode*& out_marker_node)
{
	AutoDeleteHead marker_prop_list;

	// Create new cascade to ensure inheritance in document order
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(sel_marker);
	if (!doc_ctx)
		return OpStatus::ERR;

	HLDocProfile* hld_profile = doc_ctx->GetHLDocProfile();
	if (!hld_profile)
		return OpStatus::ERR;

	LayoutProperties* marker_props = LayoutProperties::CreateCascade(sel_marker, marker_prop_list,
																	 LAYOUT_WORKPLACE(hld_profile));
	if (!marker_props)
		return OpStatus::ERR_NO_MEMORY;

	SVGElementContext* sel_context = AttrValueStore::AssertSVGElementContext(sel_marker);
	if (!sel_context)
		return OpStatus::ERR_NO_MEMORY;

	BOOL should_clip = FALSE;

#ifdef SVG_SUPPORT_STENCIL
	const HTMLayoutProperties& mprops = *marker_props->GetProps();
	should_clip = (mprops.overflow_x == CSS_VALUE_hidden ||
					mprops.overflow_x == CSS_VALUE_scroll);
#endif // SVG_SUPPORT_STENCIL

	SVGOrientType orient = SVGORIENT_UNKNOWN;
	SVGNumber angle_deg;

	SVGOrient* orientation = NULL;
	AttrValueStore::GetOrientObject(sel_marker, orientation);

	if (orientation)
	{
		orient = orientation->GetOrientType();
		if (orient == SVGORIENT_ANGLE)
		{
			if (SVGAngle* angle = orientation->GetAngle())
				angle_deg = angle->GetAngleInUnits(SVGANGLE_DEG);
		}
	}

	// to rebuild the tree
	sel_context->AddInvalidState(INVALID_ADDED);

	// Calculate the base transform
	SVGMatrix marker_base_xfrm;
	marker_base_xfrm.LoadIdentity();

	SVGMarkerUnitsType units =
		(SVGMarkerUnitsType)AttrValueStore::GetEnumValue(sel_marker, Markup::SVGA_MARKERUNITS,
														SVGENUM_MARKER_UNITS,
														SVGMARKERUNITS_STROKEWIDTH);
	if (units == SVGMARKERUNITS_STROKEWIDTH)
	{
		SVGNumber stroke_width = GetCanvas()->GetStrokeLineWidth();
		marker_base_xfrm.PostMultScale(stroke_width, stroke_width);
	}

	SVGRect marker_vp(0, 0, 3, 3);
	SVGLengthObject* marker_w = NULL;
	AttrValueStore::GetLength(sel_marker, Markup::SVGA_MARKERWIDTH, &marker_w);

	SVGLengthObject* marker_h = NULL;
	AttrValueStore::GetLength(sel_marker, Markup::SVGA_MARKERHEIGHT, &marker_h);

	SVGValueContext vcxt = GetValueContext();
	if (marker_w)
		marker_vp.width = SVGUtils::ResolveLength(marker_w->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
	if (marker_h)
		marker_vp.height = SVGUtils::ResolveLength(marker_h->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

	// Setup viewbox
	SVGAspectRatio* ar = NULL;
	AttrValueStore::GetPreserveAspectRatio(sel_marker, ar);

	SVGRectObject* viewbox = NULL;
	AttrValueStore::GetViewBox(sel_marker, &viewbox);

	SVGMatrix viewbox_xfrm;
	SVGRect cliprect;
	SVGUtils::GetViewboxTransform(marker_vp, viewbox ? &viewbox->rect : NULL,
								ar, viewbox_xfrm, cliprect);

	SVGNumberPair ref_pt(0, 0);

	// Reference point translation
	SVGLengthObject* refx = NULL;
	AttrValueStore::GetLength(sel_marker, Markup::SVGA_REFX, &refx);

	SVGLengthObject* refy = NULL;
	AttrValueStore::GetLength(sel_marker, Markup::SVGA_REFY, &refy);

	if (refx)
		ref_pt.x = SVGUtils::ResolveLength(refx->GetLength(), SVGLength::SVGLENGTH_X, vcxt);
	if (refy)
		ref_pt.y = SVGUtils::ResolveLength(refy->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

	ref_pt = viewbox_xfrm.ApplyToCoordinate(ref_pt);
	marker_base_xfrm.PostMultTranslation(-ref_pt.x, -ref_pt.y);

	marker_base_xfrm.PostMultiply(viewbox_xfrm);

	SVGMarkerNode* marker_content_node = OP_NEW(SVGMarkerNode, ());
	if (!marker_content_node)
		return OpStatus::ERR_NO_MEMORY;

	marker_content_node->Reset();

	SVGNullCanvas canvas;
	canvas.SetDefaults(doc_ctx->GetRenderingQuality());
	canvas.ConcatTransform(marker_base_xfrm);

	SVGRenderingTreeChildIterator rtci;
	SVGNumberPair marker_viewport(marker_vp.width, marker_vp.height);
	SVGPaintTreeBuilder marker_content_builder(&rtci, marker_content_node);
	marker_content_builder.SetCurrentViewport(marker_viewport);
	marker_content_builder.SetDocumentContext(doc_ctx);
	marker_content_builder.SetupResolver(m_resolver);
	marker_content_builder.SetCanvas(&canvas);
	marker_content_builder.SetInMarker(TRUE);

	OP_STATUS status = SVGTraverser::Traverse(&marker_content_builder, sel_marker, marker_props);
	if (OpStatus::IsSuccess(status))
	{
		marker_content_node->SetBaseTransform(marker_base_xfrm);
		marker_content_node->SetOrientIsAuto(orient == SVGORIENT_AUTO);
		marker_content_node->SetAngle(angle_deg);
		marker_content_node->UseClipRect(should_clip);
		marker_content_node->SetClipRect(cliprect);
	}
	else
	{
		OP_DELETE(marker_content_node);
		marker_content_node = NULL;
	}

	out_marker_node = marker_content_node;
	return status;
}

OP_STATUS SVGLayoutObject::HandleMarkers(SVGElementInfo& info)
{
	OP_NEW_DBG("HandlerMarkers", "svg_markers");

	HTML_Element* marker_elms[3];
	marker_elms[0] = NULL;
	marker_elms[1] = NULL;
	marker_elms[2] = NULL;

	const HTMLayoutProperties& props = *info.props->GetProps();
	const SvgProperties *svg_props = props.svg;

	if (!svg_props->marker.info.is_none)
	{
		HTML_Element* marker_elm = NULL;
		ResolveMarker(info, svg_props->marker, marker_elm);
		marker_elms[0] = marker_elms[1] = marker_elms[2] = marker_elm;
	}

	if (!svg_props->markerstart.info.is_none)
		ResolveMarker(info, svg_props->markerstart, marker_elms[0]);
	if (!svg_props->markermid.info.is_none)
		ResolveMarker(info, svg_props->markermid, marker_elms[1]);
	if (!svg_props->markerend.info.is_none)
		ResolveMarker(info, svg_props->markerend, marker_elms[2]);

	if (marker_elms[0] == NULL && marker_elms[1] == NULL && marker_elms[2] == NULL)
	{
		static_cast<SVGGeometryNode*>(info.paint_node)->ResetMarkers();
		return OpStatus::OK;
	}

	SVGMarkerNode* marker_nodes[3];
	marker_nodes[0] = NULL;
	marker_nodes[1] = NULL;
	marker_nodes[2] = NULL;

	OP_STATUS status = OpStatus::OK;
	for (int i = 0; i < 3 && OpStatus::IsSuccess(status); i++)
	{
		HTML_Element* sel_marker = marker_elms[i];
		if (sel_marker)
		{
			if (OpStatus::IsSuccess(m_resolver->FollowReference(sel_marker)))
			{
				status = LayoutMarker(sel_marker, marker_nodes[i]);
				m_resolver->LeaveReference(sel_marker);

				OP_DBG(("marker-%s: elm: %p marker-node: %p",
						i > 0 ? i == 1 ? "mid" : "end" : "start",
						sel_marker, marker_nodes[i]));
			}
		}
	}

	if (OpStatus::IsSuccess(status))
	{
		static_cast<SVGGeometryNode*>(info.paint_node)->SetMarkers(marker_nodes[0], marker_nodes[1], marker_nodes[2]);
	}
	else
	{
		if (marker_nodes[0])
			marker_nodes[0]->Free();
		if (marker_nodes[1])
			marker_nodes[1]->Free();
		if (marker_nodes[2])
			marker_nodes[2]->Free();
	}

	// Invalidate area with the markers included
	SVGBoundingBox bbox;
	info.paint_node->GetLocalExtents(bbox);
	OpStatus::Ignore(m_canvas->DrawRect(bbox.minx, bbox.miny,
										bbox.maxx - bbox.minx,
										bbox.maxy - bbox.miny, 0, 0));
	return status;
}
#endif // SVG_SUPPORT_MARKERS

BOOL SVGLayoutObject::AllowElementTraverse(SVGElementInfo& info)
{
	// This is the magic 'flood-gate' test (maybe this is a candidate
	// for pushing into TraverseElement ?)
	if (!UseTraversalFilter())
		return TRUE;

	// Traverse if the element/subtree/parent is sufficiently dirty
	if (info.context->HasSubTreeChanged() || info.GetInvalidState() > INVALID_STRUCTURE)
		return TRUE;

	// If this element has a node in the paint tree, then set it as
	// the current predecessor
	if (SVGPaintNode* paint_node = info.context->GetPaintNode())
		if (paint_node->InList())
			// The element is clean, so if it has an attached paint
			// node then set it as the current predecessor.
			m_current_pred = paint_node;

	return FALSE;
}

BOOL SVGIntersectionObject::AllowElementTraverse(SVGElementInfo& info)
{
	// This is the magic 'flood-gate' test (maybe this is a candidate
	// for pushing into TraverseElement ?)
	if (!UseTraversalFilter())
		return TRUE;

	SVGPaintNode* paint_node = info.context->GetPaintNode();
	if (!paint_node || !paint_node->GetParent())
		return TRUE;

	// Should consider not doing the following test if extents are
	// dirty to avoid disturbing state unnecessarily
	SVGPaintNode::NodeContext node_context;
	node_context.transform = m_canvas->GetTransform();
	node_context.clip = OpRect(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);

	paint_node->GetNodeContext(node_context);

	OpRect pixel_extents = paint_node->GetPixelExtents(node_context);

	// Allow element if its screenbox is intersected by the point
	// being tested.
	return pixel_extents.Contains(m_canvas->GetIntersectionPointInt());
}

// This is the same as the old SVGPaintingObject
BOOL SVGRectSelectionObject::AllowElementTraverse(SVGElementInfo& info)
{
	// This is the magic 'flood-gate' test (maybe this is a candidate
	// for pushing into TraverseElement ?)
	if (!UseTraversalFilter())
		return TRUE;

	SVGPaintNode* paint_node = info.context->GetPaintNode();
	if (!paint_node || !paint_node->GetParent())
		return TRUE;

	// Should consider not doing the following test if extents are
	// dirty to avoid disturbing state unnecessarily
	SVGPaintNode::NodeContext node_context;
	node_context.transform = m_canvas->GetTransform();
	node_context.clip = OpRect(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);

	paint_node->GetNodeContext(node_context);

	OpRect pixel_extents = paint_node->GetPixelExtents(node_context);

	// Skip elements with screen boxes that are empty, or
	// does not overlap the hard cliprect of the canvas
	if (pixel_extents.IsEmpty())
		return FALSE;

	return m_canvas->IsVisible(pixel_extents);
}

SVGTraversalObject::~SVGTraversalObject()
{
	// Free viewports
	while (m_saved_viewports)
		PopViewport();

	if (m_owns_resolver)
		OP_DELETE(m_resolver);

	OP_DELETE(m_textarea_info);

	DestroyTextInfo();
}

OP_STATUS SVGTraversalObject::SetupResolver(SVGElementResolver* external_resolver /* = NULL */)
{
	m_resolver = external_resolver;
	m_owns_resolver = FALSE;

	if (!m_resolver)
	{
		m_resolver = OP_NEW(SVGElementResolver, ());

		if (!m_resolver)
			return OpStatus::ERR_NO_MEMORY;

		m_owns_resolver = TRUE;
	}
	return OpStatus::OK;
}

/* static */
BOOL SVGTraversalObject::FetchUseOffset(SVGElementInfo& info, const SVGValueContext& vcxt,
										TransformInfo& tinfo)
{
	return SetupUseOffset(info.layouted, vcxt, tinfo.offset);
}

/* static */
BOOL SVGTraversalObject::FetchTransform(SVGElementInfo& info, TransformInfo& tinfo)
{
	SVGTrfmCalcHelper trfm_helper;
	trfm_helper.Setup(info.layouted);

	if (!trfm_helper.IsSet())
		return FALSE;

	tinfo.needs_reset = trfm_helper.HasRefTransform(tinfo.transform);

	SVGMatrix matrix;
	trfm_helper.GetMatrix(matrix);
	tinfo.transform.PostMultiply(matrix);
	return TRUE;
}

void SVGTraversalObject::ApplyTransform(SVGElementInfo& info, const TransformInfo& tinfo)
{
	if (tinfo.needs_reset)
	{
		m_canvas->ResetTransform();
		m_canvas->ConcatTransform(m_root_transform);
	}

	m_canvas->ConcatTransform(tinfo.transform);
	m_canvas->ConcatTranslation(tinfo.offset);

	info.has_transform = 1;

	if (m_textinfo)
		m_textinfo->font_desc.SetScaleChanged(TRUE);
}

void SVGTraversalObject::ApplyUserTransform(SVGElementInfo& info, const ViewportInfo& vpinfo)
{
	m_canvas->ConcatTransform(vpinfo.user_transform);

	info.has_transform = 1;
}

// FIXME: Inline and possibly remove the viewport part
void SVGTraversalObject::UpdateValueContext(SVGElementInfo& info)
{
	const HTMLayoutProperties& props = *info.props->GetProps();
	m_value_context.fontsize = props.svg->GetFontSize(info.layouted);
	m_value_context.ex_height = SVGUtils::GetExHeight(m_doc_ctx->GetVisualDevice(), m_value_context.fontsize, props.font_number);

	m_value_context.viewport_width = m_current_viewport.x;
	m_value_context.viewport_height = m_current_viewport.y;
}

OP_STATUS SVGTraversalObject::PushViewport(const SVGNumberPair& vp)
{
	SavedViewport* svp = OP_NEW(SavedViewport, ());
	if (!svp)
		return OpStatus::ERR_NO_MEMORY;

	svp->viewport = m_current_viewport;
	svp->next = m_saved_viewports;

	m_current_viewport = vp;
	m_value_context.viewport_width = m_current_viewport.x;
	m_value_context.viewport_height = m_current_viewport.y;
	m_saved_viewports = svp;
	return OpStatus::OK;
}

void SVGTraversalObject::PopViewport()
{
	if (!m_saved_viewports)
		return; // For presumed robustness

	SavedViewport* svp = m_saved_viewports;
	m_saved_viewports = svp->next;
	m_current_viewport = svp->viewport;
	m_value_context.viewport_width = m_current_viewport.x;
	m_value_context.viewport_height = m_current_viewport.y;
	OP_DELETE(svp);
}

//
// SVGBBoxUpdateObject
//

SVGElementContext* SVGBBoxUpdateObject::GetParentContext(SVGElementInfo& info)
{
	if (info.traversed == m_doc_ctx->GetSVGRoot())
		return NULL;

	HTML_Element* parent = info.traversed->Parent();

	if (parent->GetNsType() != NS_SVG)
		return NULL;

	return AttrValueStore::GetSVGElementContext_Unsafe(parent);
}

void SVGBBoxUpdateObject::Propagate(SVGElementInfo& info)
{
	// Never propagate bboxes on elements that are display:none
	if (SVGUtils::IsDisplayNone(info.layouted, info.props))
		return;

	SVGBoundingBox bbox = info.context->GetBBox();
	if (bbox.IsEmpty())
		return;

	// Fetch the transforms that affects this element

	if (info.layouted->Type() == Markup::SVGE_USE)
	{
		SVGNumberPair use_translation;
		if (SetupUseOffset(info.layouted, GetValueContext(), use_translation))
		{
			bbox.minx += use_translation.x;
			bbox.miny += use_translation.y;
			bbox.maxx += use_translation.x;
			bbox.maxy += use_translation.y;
			info.context->SetBBox(bbox);
		}
	}

	SVGElementContext* parent_context = GetParentContext(info);
	if (!parent_context)
		return;

	// FIXME: This is lacking for 'special' elements
	// (elements with viewBoxes more or less), and
	// probably for certain configurations of viewports.

	SVGTrfmCalcHelper trfm_helper;
	trfm_helper.Setup(info.layouted);

	if (trfm_helper.IsSet())
	{
		SVGMatrix matrix;
		trfm_helper.GetMatrix(matrix);
		bbox = matrix.ApplyToNonEmptyBoundingBox(bbox);
	}

#if 0
	// How do we handle ref-transform here really
	// Just use root_ctm * matrix and be happy ?
	SVGMatrix ref_matrix;
	if (trfm_helper.HasRefTransform(ref_matrix))
	{
		matrix.Multiply(ref_matrix);
		matrix.Multiply(m_root_transform);
	}
#endif // 0

	parent_context->AddBBox(bbox);
}

OP_STATUS SVGBBoxUpdateObject::SetupTextAreaElement(SVGElementInfo& info)
{
	RETURN_IF_ERROR(CalculateTextAreaExtents(info));

	return SVGVisualTraversalObject::SetupTextAreaElement(info);
}

OP_STATUS SVGBBoxUpdateObject::EnterContainer(SVGElementInfo& info, const TransformInfo& tinfo)
{
	// FIXME/OPT: If the BBox for the container is known to be valid
	// (i.e, we must know that the structure is intact) we could just
	// skip the content-step and move to leave and propagate.
	// Same goes for all Enter*() below.
	info.context->SetBBoxIsValid(FALSE);

	return OpStatus::OK;
}

OP_STATUS SVGBBoxUpdateObject::LeaveContainer(SVGElementInfo& info)
{
	Propagate(info);

	info.context->SetBBoxIsValid(TRUE);

	return OpStatus::OK;
}

OP_STATUS SVGBBoxUpdateObject::EnterElement(SVGElementInfo& info, const TransformInfo& tinfo)
{
	info.context->SetBBoxIsValid(FALSE);

	return OpStatus::OK;
}

OP_STATUS SVGBBoxUpdateObject::LeaveElement(SVGElementInfo& info)
{
	Propagate(info);

	info.context->SetBBoxIsValid(TRUE);

	return OpStatus::OK;
}

OP_STATUS SVGBBoxUpdateObject::EnterTextNode(SVGElementInfo& info)
{
	info.context->SetBBoxIsValid(FALSE);

	return SVGVisualTraversalObject::EnterTextNode(info);
}

OP_STATUS SVGBBoxUpdateObject::LeaveTextNode(SVGElementInfo& info)
{
	// FIXME: Is this overly complex, and could we just do like most
	// other text-stuff (i.e, just use the bbox-stack in tparams) ?
	SVGBoundingBox bbox = info.context->GetBBox();
	if (!bbox.IsEmpty())
		if (SVGElementContext* parent_context = GetParentContext(info))
			// TextNodes can't have transforms of their own, so no need to
			// fetch transforms
			parent_context->AddBBox(bbox);

	return OpStatus::OK;
}

OP_STATUS SVGBBoxUpdateObject::EnterTextGroup(SVGElementInfo& info)
{
	info.context->SetBBoxIsValid(FALSE);
	return SVGVisualTraversalObject::EnterTextGroup(info);
}

OP_STATUS SVGBBoxUpdateObject::LeaveTextGroup(SVGElementInfo& info)
{
	info.context->AddBBox(m_textinfo->bbox.Get());
	info.context->SetBBoxIsValid(TRUE);

	SVGBoundingBox bbox = info.context->GetBBox();
	if (!bbox.IsEmpty())
		if (SVGElementContext* parent_context = GetParentContext(info))
			// TextNodes can't have transforms of their own, so no need to
			// fetch transforms
			parent_context->AddBBox(bbox);

	return SVGVisualTraversalObject::LeaveTextGroup(info);
}

//
// SVGTransformTraversalObject
//

/* virtual */
BOOL SVGTransformTraversalObject::AllowElementTraverse(SVGElementInfo& info)
{
	OP_ASSERT(m_element_filter);

	if (m_element_filter->GetCount() == 0 ||
		m_element_filter->Get(0) != info.traversed)
		return FALSE;

	m_element_filter->Remove(0);
	return TRUE;
}

/* virtual */
OP_STATUS SVGTransformTraversalObject::EnterContainer(SVGElementInfo& info, const TransformInfo& tinfo)
{
	return EnterElement(info, tinfo);
}

/* virtual */
OP_STATUS SVGTransformTraversalObject::EnterElement(SVGElementInfo& info, const TransformInfo& tinfo)
{
	if (m_element_filter->GetCount() == 0)
	{
		m_calculated_ctm = m_canvas->GetTransform();

		// If this is a <use>, then we need to undo any additional
		// translation, since that would only apply if the target was
		// a child of the <use>
		if (info.layouted->Type() == Markup::SVGE_USE)
			m_calculated_ctm.PostMultTranslation(-tinfo.offset.x, -tinfo.offset.y);
	}
	return OpStatus::OK;
}

#endif // SVG_SUPPORT
