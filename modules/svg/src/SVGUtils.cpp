/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGUtils.h"

#include "modules/svg/presentation_attrs.h"

#ifdef SVG_SUPPORT_EXTERNAL_USE
# include "modules/dochand/fdelm.h"
#endif // SVG_SUPPORT_EXTERNAL_USE

#include "modules/layout/box/box.h"
#include "modules/layout/cascade.h"
#include "modules/layout/layoutprops.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/src/textdata.h"

#include "modules/display/vis_dev.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/logdoc/xmlenum.h"

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGInternalEnum.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/OpBpath.h"
#include "modules/svg/src/SVGElementStateContext.h"
#include "modules/svg/src/SVGTextElementStateContext.h"
#include "modules/svg/src/SVGTraverse.h"
#include "modules/svg/src/SVGChildIterator.h"
#include "modules/svg/src/SVGTextRenderer.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGTextTraverse.h"
#include "modules/svg/src/SVGMarker.h"
#include "modules/svg/src/SVGFontTraversalObject.h"
#include "modules/svg/src/SVGCanvas.h"

#include "modules/pi/OpScreenInfo.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/security_manager/include/security_manager.h"

#include "modules/viewers/viewers.h"

#include "modules/dochand/win.h"

#define SVG_SUPPORT_FULL_ASPECT_RATIO

static const SVGFeatureType s_svg_supported_features[] = {
#ifdef SVG_FULL_11
	SVGFEATURE_SVG,
	SVGFEATURE_SVGDOM,
	SVGFEATURE_SVGDOMSTATIC,
	SVGFEATURE_SVGDOMANIMATION,
	SVGFEATURE_SVGDOMDYNAMIC,
	SVGFEATURE_SVGANIMATION,
	SVGFEATURE_SVGDYNAMIC, // missing cursor
	SVGFEATURE_SVGSTATIC, // missing colorprofile
	SVGFEATURE_COREATTRIBUTE,
	SVGFEATURE_STRUCTURE,
#ifdef SVG_SUPPORT_FILTERS
	SVGFEATURE_CONTAINERATTRIBUTE, // enable-background
#endif // SVG_SUPPORT_FILTERS
	SVGFEATURE_CONDITIONALPROCESSING,
#ifdef SVG_SUPPORT_STENCIL
	SVGFEATURE_VIEWPORTATTRIBUTE,
#endif // SVG_SUPPORT_STENCIL
	SVGFEATURE_HYPERLINKING,
	SVGFEATURE_XLINKATTRIBUTE, // missing xlink:type, xlink:role, xlink:arcrole, xlink:actuate
	SVGFEATURE_CONDITIONALPROCESSING,
	SVGFEATURE_SHAPE,
	SVGFEATURE_TEXT,
	SVGFEATURE_MARKER,
#ifdef SVG_SUPPORT_GRADIENTS
	SVGFEATURE_GRADIENT,
#endif // SVG_SUPPORT_GRADIENTS
#ifdef SVG_SUPPORT_PATTERNS
	SVGFEATURE_PATTERN,
#endif // SVG_SUPPORT_PATTERNS
#ifdef SVG_SUPPORT_STENCIL
	SVGFEATURE_CLIP,
	SVGFEATURE_MASK,
#endif // SVG_SUPPORT_STENCIL
#ifdef SVG_SUPPORT_FILTERS
	SVGFEATURE_FILTER,
#endif // SVG_SUPPORT_FILTERS
	SVGFEATURE_DOCUMENTEVENTSATTRIBUTE, // missing onabort, onerror, onunload, onscroll(?
	SVGFEATURE_GRAPHICALEVENTSATTRIBUTE, // never sent: onfocusin, onfocusout, onactivate
	SVGFEATURE_ANIMATIONEVENTSATTRIBUTE,
	SVGFEATURE_EXTERNALRESOURCESREQUIRED,
	SVGFEATURE_VIEW,
	//SVGFEATURE_CURSOR, // missing cursor element
	//SVGFEATURE_PAINTATTRIBUTE, // missing color-rendering, color-interpolation
	SVGFEATURE_OPACITYATTRIBUTE,
	SVGFEATURE_SCRIPT,
	SVGFEATURE_STYLE,
	SVGFEATURE_FONT, // many attributes missing still, but enabled anyway
	SVGFEATURE_GRAPHICSATTRIBUTE,
	SVGFEATURE_IMAGE,
	SVGFEATURE_ANIMATION,
	SVGFEATURE_EXTENSIBILITY,
#if		defined(SVG_SUPPORT_GRADIENTS) && \
		defined(SVG_SUPPORT_OPACITY) && \
		defined(SVG_SUPPORT_PATTERNS) && \
		defined(SVG_SUPPORT_ELLIPTICAL_ARCS) && \
		defined(SVG_SUPPORT_STENCIL) && \
		defined(SVG_DOM) && \
		defined(SVG_SUPPORT_FILTERS)
	SVGFEATURE_BASIC_ALL,
#endif
	SVGFEATURE_BASIC_BASE,
#ifdef SVG_SUPPORT_STENCIL
	SVGFEATURE_BASIC_CLIP,
#endif // SVG_SUPPORT_STENCIL
	SVGFEATURE_BASIC_CSS,
#ifdef SVG_DOM
	SVGFEATURE_BASIC_DOMCORE,
	SVGFEATURE_BASIC_DOMEXTENDED,
#endif // SVG_DOM
#ifdef SVG_SUPPORT_FILTERS
	SVGFEATURE_BASIC_FILTER,
#endif // SVG_SUPPORT_FILTERS
	SVGFEATURE_BASIC_FONT,
	SVGFEATURE_BASIC_GRAPHICSATTRIBUTE,
	SVGFEATURE_BASIC_INTERACTIVITY,
	SVGFEATURE_BASIC_PAINTATTRIBUTE, // missing color-rendering
	SVGFEATURE_BASIC_STRUCTURE,
	SVGFEATURE_BASIC_TEXT,
	SVGFEATURE_TINY_ALL,
	SVGFEATURE_TINY_BASE,
	SVGFEATURE_TINY_INTERACTIVITY,
	SVGFEATURE_1_0,
	SVGFEATURE_1_0_STATIC,
	SVGFEATURE_1_0_ANIMATION,
#ifdef SVG_DOM
	SVGFEATURE_1_0_DYNAMIC,
	SVGFEATURE_1_0_ALL,
	SVGFEATURE_1_0_DOM,
	SVGFEATURE_1_0_DOM_STATIC,
	SVGFEATURE_1_0_DOM_ANIMATION,
	SVGFEATURE_1_0_DOM_DYNAMIC,
	SVGFEATURE_1_0_DOM_ALL,
#endif // SVG_DOM
#endif // SVG_FULL_11
#ifdef SVG_TINY_12
	SVGFEATURE_1_2_SVGSTATIC,
	SVGFEATURE_1_2_SVGSTATICDOM,
	SVGFEATURE_1_2_SVGANIMATED,
	SVGFEATURE_1_2_SVGALL,

	SVGFEATURE_1_2_CORE_ATTRIBUTE,
	SVGFEATURE_1_2_NAVIGATION_ATTRIBUTE,
	SVGFEATURE_1_2_STRUCTURE,
	SVGFEATURE_1_2_CONDITIONAL_PROCESSING,
	SVGFEATURE_1_2_CONDITIONAL_PROCESSING_ATTRIBUTE,
	SVGFEATURE_1_2_IMAGE,
	SVGFEATURE_1_2_PREFETCH,
	SVGFEATURE_1_2_DISCARD,
	SVGFEATURE_1_2_SHAPE,
	SVGFEATURE_1_2_TEXT,
	SVGFEATURE_1_2_PAINT_ATTRIBUTE, // missing color-rendering
	SVGFEATURE_1_2_OPACITY_ATTRIBUTE,
	SVGFEATURE_1_2_GRAPHICS_ATTRIBUTE,
	SVGFEATURE_1_2_GRADIENT,
	SVGFEATURE_1_2_SOLID_COLOR,
	SVGFEATURE_1_2_HYPERLINKING,
	SVGFEATURE_1_2_XLINK_ATTRIBUTE, // missing xlink:type, xlink:role, xlink:arcrole, xlink:actuate
	SVGFEATURE_1_2_EXTERNALRESOURCESREQUIRED,
	SVGFEATURE_1_2_SCRIPTING,
	SVGFEATURE_1_2_HANDLER,
	SVGFEATURE_1_2_LISTENER,
	SVGFEATURE_1_2_TIMEDANIMATION,
	SVGFEATURE_1_2_ANIMATION,
#ifdef SVG_SUPPORT_MEDIA
	SVGFEATURE_1_2_AUDIO,
	SVGFEATURE_1_2_VIDEO,
	SVGFEATURE_1_2_TRANSFORMEDVIDEO,
	SVGFEATURE_1_2_COMPOSEDVIDEO,
#endif // SVG_SUPPORT_MEDIA
	SVGFEATURE_1_2_FONT,
	SVGFEATURE_1_2_EXTENSIBILITY,
	SVGFEATURE_1_2_MEDIA_ATTRIBUTE,
	SVGFEATURE_1_2_TEXTFLOW,
#endif // SVG_TINY_12
#ifdef SVG_SUPPORT_EDITABLE
	SVGFEATURE_1_2_EDITABLE_ATTRIBUTE,
	SVGFEATURE_1_2_SVGINTERACTIVE, // editable + navigation
#endif // SVG_SUPPORT_EDITABLE
	SVGFEATURE_UNKNOWN // This should always be the last item in the list
};

HTML_Element* SVGUtils::FindElementById(LogicalDocument* logdoc, const uni_char* id)
{
	if(!logdoc)
		return NULL;

	HTML_Element *result = NULL;

	if(logdoc && id)
	{
#ifdef _PRINT_SUPPORT_
		/**
		* This is necessary to fix the lookup of svg elements in print preview.
		* Note that the LogicalDocument has two trees: one print tree, and one normal tree.
		* The print tree elements are not in the hashtable.
		*/
		if(logdoc->GetFramesDocument() && logdoc->GetFramesDocument()->IsPrintDocument())
		{
			HTML_Element* docroot = logdoc->GetRoot();
			result = docroot->GetElmById(id);
			if(result && result->GetNsType() != NS_SVG)
			{
				result = NULL;
			}
		}
		else
#endif // _PRINT_SUPPORT_
		{
			// Fast (hash table base)
			NamedElementsIterator it;
			logdoc->GetNamedElements(it, id, TRUE, FALSE);
			result = it.GetNextElement();
		}
	}

	return result;
}

HTML_Element* SVGUtils::GetRootSVGElement(HTML_Element *child)
{
	while (child)
	{
		if (child->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
		{
			return child;
		}
		child = child->Parent();
	}

	return NULL;
}

/* static */
HTML_Element* SVGUtils::GetTopmostSVGRoot(HTML_Element *parent)
{
	HTML_Element* root = NULL;
	while (parent)
	{
		NS_Type ns = parent->GetNsType();

		if (ns == NS_SVG)
		{
			if (parent->Type() == Markup::SVGE_SVG)
			{
				root = parent;
			}
		}
		else if(Markup::IsRealElement(parent->Type()))
		{
			// Crossing namespace is not allowed
			return root;
		}

		parent = parent->Parent();
	}

	return root;
}

/* static */
SVGDocumentContext* SVGUtils::GetTopmostSVGDocumentContext(HTML_Element* elm)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	while (doc_ctx)
	{
		SVGDocumentContext* parent_doc_ctx = doc_ctx->GetParentDocumentContext();
		if (!parent_doc_ctx)
			break;

		doc_ctx = parent_doc_ctx;
	}
	return doc_ctx;
}

/* static */
HTML_Element* SVGUtils::GetTextRootElement(HTML_Element* elm)
{
	if(!elm || elm->GetNsType() != NS_SVG || !IsTextClassType(elm->Type()))
		return NULL;

	OP_ASSERT(!IsShadowNode(elm));
	HTML_Element* textroot = elm;
	HTML_Element* closest_svgroot = GetRootSVGElement(elm);
	while(!IsTextRootElement(textroot) && textroot != closest_svgroot)
	{
		textroot = textroot->Parent();
		OP_ASSERT(!textroot || !IsShadowNode(elm));
	}

	return(IsTextRootElement(textroot) ? textroot : NULL);
}

/* static */
BOOL SVGUtils::IsEditable(HTML_Element* elm)
{
	HTML_Element* textroot = GetTextRootElement(elm);
	return textroot != NULL && (AttrValueStore::GetEnumValue(textroot, Markup::SVGA_EDITABLE, SVGENUM_EDITABLE,
															 SVGEDITABLE_NONE) == SVGEDITABLE_SIMPLE);
}

BOOL SVGUtils::IsGraphicElement(HTML_Element *e)
{
	if(e && e->GetNsType() == NS_SVG)
	{
		Markup::Type type = e->Type();
		if(	type == Markup::SVGE_CIRCLE ||
			type == Markup::SVGE_ELLIPSE ||
			type == Markup::SVGE_IMAGE ||
			type == Markup::SVGE_LINE ||
			type == Markup::SVGE_PATH ||
			type == Markup::SVGE_POLYGON ||
			type == Markup::SVGE_POLYLINE ||
			type == Markup::SVGE_RECT ||
			type == Markup::SVGE_TEXT ||
			type == Markup::SVGE_TSPAN ||
			type == Markup::SVGE_TREF ||
			type == Markup::SVGE_TEXTPATH ||
			type == Markup::SVGE_TEXTAREA ||
			type == Markup::SVGE_ALTGLYPH ||
			type == Markup::SVGE_ANIMATION ||
			type == Markup::SVGE_VIDEO
			)
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL SVGUtils::IsContainerElement(HTML_Element *e)
{
	if(e && e->GetNsType() == NS_SVG)
	{
		Markup::Type type = e->Type();
		if(	type == Markup::SVGE_G ||
			type == Markup::SVGE_SVG ||
			type == Markup::SVGE_A ||
			type == Markup::SVGE_USE)
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL SVGUtils::IsAnimationElement(HTML_Element *e)
{
	if(e && e->GetNsType() == NS_SVG)
	{
		Markup::Type type = e->Type();
		if (type == Markup::SVGE_SET ||
			type == Markup::SVGE_ANIMATE ||
			type == Markup::SVGE_ANIMATEMOTION ||
			type == Markup::SVGE_ANIMATECOLOR ||
			type == Markup::SVGE_ANIMATETRANSFORM)
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL SVGUtils::IsTimedElement(HTML_Element *e)
{
	if(e && e->GetNsType() == NS_SVG)
	{
		Markup::Type type = e->Type();
		if (IsAnimationElement(e) ||
			type == Markup::SVGE_ANIMATION ||
			type == Markup::SVGE_VIDEO ||
			type == Markup::SVGE_AUDIO)
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL SVGUtils::IsTextChildType(Markup::Type type)
{
	return type == Markup::SVGE_TREF ||
		type == Markup::SVGE_TSPAN ||
		type == Markup::SVGE_TBREAK ||
		type == Markup::SVGE_TEXTPATH ||
		type == Markup::SVGE_ALTGLYPH;
}

BOOL SVGUtils::IsTextRootElement(HTML_Element* e)
{
	return e && e->GetNsType() == NS_SVG && IsTextRootType(e->Type());
}

BOOL SVGUtils::IsItemTypeTextAttribute(HTML_Element* e, int attr, NS_Type attr_ns)
{
	return e && attr_ns == NS_SVG &&
		   (attr == Markup::SVGA_ID ||
		    attr == Markup::SVGA_CONTENTSCRIPTTYPE ||
			attr == Markup::SVGA_CONTENTSTYLETYPE ||
		    (attr == Markup::SVGA_TYPE &&
			 e->GetNsType() == NS_SVG &&
		     (e->Type() == Markup::SVGE_AUDIO ||
			  e->Type() == Markup::SVGE_VIDEO ||
			  e->Type() == Markup::SVGE_STYLE ||
			  e->Type() == Markup::SVGE_SCRIPT ||
			  e->Type() == Markup::SVGE_HANDLER)));
}

/**
* "A 'clipPath' element can contain 'path' elements, 'text' elements, basic shapes
*  (such as 'circle') or a 'use' element. If a 'use' element is a child of a 'clipPath'
*   element, it must directly reference 'path', 'text' or basic shape elements. Indirect
*   references are an error (see Error processing).
*/
BOOL SVGUtils::IsAllowedClipPathElementType(Markup::Type type)
{
	if (Markup::IsNonElementNode(type) || // This is to allow all the common types for elements in Opera (see logdoc/htm_elm.h)
		type == Markup::SVGE_TEXT ||
		type == Markup::SVGE_PATH ||
		type == Markup::SVGE_CIRCLE ||
		type == Markup::SVGE_ELLIPSE ||
		type == Markup::SVGE_LINE ||
		type == Markup::SVGE_POLYGON ||
		type == Markup::SVGE_POLYLINE ||
		type == Markup::SVGE_RECT)
	{
		return TRUE;
	}

	return FALSE;
}

/**
* "A 'mask' element can contain 'path' elements, 'text' elements, basic shapes
*  (such as 'circle') or container elements.
*/
BOOL SVGUtils::IsAllowedMaskElementType(Markup::Type type)
{
	if (Markup::IsNonElementNode(type) || // This is to allow all the common types for elements in Opera (see logdoc/htm_elm.h)
		type == Markup::SVGE_TEXT ||
		type == Markup::SVGE_PATH ||
		type == Markup::SVGE_CIRCLE ||
		type == Markup::SVGE_ELLIPSE ||
		type == Markup::SVGE_LINE ||
		type == Markup::SVGE_POLYGON ||
		type == Markup::SVGE_POLYLINE ||
		type == Markup::SVGE_RECT ||
		type == Markup::SVGE_IMAGE ||
		type == Markup::SVGE_CLIPPATH ||
		type == Markup::SVGE_MASK ||
		type == Markup::SVGE_A ||
		type == Markup::SVGE_SVG ||
		type == Markup::SVGE_SWITCH ||
		type == Markup::SVGE_G)
	{
		return TRUE;
	}

	return FALSE;
}

BOOL SVGUtils::CanHaveExternalReference(Markup::Type type)
{
	if (type == Markup::SVGE_IMAGE ||
		type == Markup::SVGE_USE ||
		//type == Markup::SVGE_A || // excluded since it's a link
		type == Markup::SVGE_FEIMAGE ||
		type == Markup::SVGE_TREF ||
// 		type == Markup::SVGE_VIDEO || // can have, but not sure if we should handle it this way
//		type == Markup::SVGE_AUDIO || // can have, but not sure if we should handle it this way
		type == Markup::SVGE_ANIMATION ||
		type == Markup::SVGE_FONT_FACE_URI ||
		type == Markup::SVGE_TEXTPATH ||
//		type == Markup::SVGE_LINEARGRADIENT || // only within current document fragment
//		type == Markup::SVGE_RADIALGRADIENT || // only within current document fragment
//		type == Markup::SVGE_PATTERN || // only within current document fragment
//		type == Markup::SVGE_FILTER || // only within current document fragment
		type == Markup::SVGE_MPATH ||
		type == Markup::SVGE_SCRIPT ||
		type == Markup::SVGE_CURSOR ||
		type == Markup::SVGE_ALTGLYPH ||
//		type == Markup::SVGE_GLYPHREF || // not yet implemented, it can have external reference
		type == Markup::SVGE_COLOR_PROFILE ||
		type == Markup::SVGE_DEFINITION_SRC ||
		type == Markup::SVGE_FOREIGNOBJECT ||
		type == Markup::SVGE_PREFETCH)
	{
		return TRUE;
	}

	return FALSE;
}

BOOL SVGUtils::HasFeatures(SVGVector *list)
{
	BOOL result = TRUE;

	if(!list || list->GetCount() == 0)
		return FALSE;

	for(UINT32 i = 0; i < list->GetCount(); i++)
	{
		SVGString* eval = static_cast<SVGString *>(list->Get(i));
		result = HasFeature(eval->GetString(), eval->GetLength());
		if(!result)
			break;
	}
	return result;
}

BOOL SVGUtils::HasFeature(const uni_char* feature, UINT32 str_len)
{
	BOOL res = FALSE;
	SVGFeatureType req_enum = (SVGFeatureType)
		SVGEnumUtils::GetEnumValue(SVGENUM_REQUIREDFEATURES, feature, str_len);
	OP_ASSERT(req_enum < SVGFEATURE_UNKNOWN);

	for(int j = 0; s_svg_supported_features[j] != SVGFEATURE_UNKNOWN; j++)
	{
		if(req_enum == s_svg_supported_features[j])
		{
			res = TRUE;
			break;
		}
	}
	return res;
}

#ifdef SVG_TINY_12
static BOOL HasFormat(const uni_char* format, UINT32 str_len)
{
	OpString mediatype;
	RETURN_VALUE_IF_ERROR(mediatype.Set(format, str_len), FALSE); // FIXME: OOM

#ifdef SVG_SUPPORT_MEDIA
	BOOL3 can_play;
	OP_STATUS status = g_media_module.CanPlayType(&can_play, mediatype);
	if (OpStatus::IsSuccess(status) &&
		can_play != NO)
		return TRUE;
#endif // SVG_SUPPORT_MEDIA

	if (Viewer* viewer = g_viewers->FindViewerByMimeType(mediatype))
		if (viewer->GetAction() == VIEWER_OPERA)
			return TRUE;

	return FALSE;
}

BOOL SVGUtils::HasFormats(SVGVector *list)
{
	BOOL result = TRUE;

	if(!list || list->GetCount() == 0)
		return FALSE;

	for(UINT32 i = 0; i < list->GetCount(); i++)
	{
		SVGString* eval = static_cast<SVGString *>(list->Get(i));
		result = HasFormat(eval->GetString(), eval->GetLength());
		if(!result)
			break;
	}
	return result;
}

BOOL SVGUtils::HasRequiredExtensions(SVGVector *list)
{
	BOOL result = TRUE;

	if(!list || list->GetCount() == 0)
		return FALSE;

	for(UINT32 i = 0; i < list->GetCount(); i++)
	{
		SVGString* eval = static_cast<SVGString *>(list->Get(i));
		result = (g_ns_manager->FindNsType(eval->GetString(), eval->GetLength()) != NS_USER);
		if(!result)
			break;
	}
	return result;
}

BOOL SVGUtils::HasFonts(SVGVector *list)
{
	BOOL result = TRUE;

	if(!list || list->GetCount() == 0)
		return FALSE;

	for(UINT32 i = 0; i < list->GetCount(); i++)
	{
		SVGString* eval = static_cast<SVGString *>(list->Get(i));
		result = (styleManager->GetFontNumber(eval->GetString()) != -1);
		if(!result)
			break;
	}
	return result;
}
#endif // SVG_TINY_12

OP_STATUS SVGUtils::ConvertToUnit(SVGObject* obj, int to_unit, SVGLength::LengthOrientation type,
								  const SVGValueContext& context)
{
	if (obj && obj->Type() == SVGOBJECT_LENGTH)
	{
		SVGLengthObject* length_obj = (SVGLengthObject*)obj;
		SVGNumber in_userunits = ResolveLength(length_obj->GetLength(), type, context);
		SVGNumber new_scalar = ConvertToUnit(in_userunits, to_unit, type, context);
		length_obj->SetScalar(new_scalar);
		length_obj->SetUnit(to_unit);
		return OpStatus::OK;
	}
	return OpSVGStatus::TYPE_ERROR;
}

SVGNumber SVGUtils::ConvertToUnit(SVGNumber number, int unit, SVGLength::LengthOrientation type,
								  const SVGValueContext& context)
{
	switch(unit)
	{
		case CSS_EM:
			return number / context.fontsize;
		case CSS_REM:
			return number / context.root_font_size;
		case CSS_EX:
			return number / context.ex_height;
		case CSS_PX:
			return number; // User coordinates already
		case CSS_PT:
			return (number * 72) / CSS_PX_PER_INCH;
		case CSS_PC:
			return (number * 6) / CSS_PX_PER_INCH;
		case CSS_CM:
			return (number * SVGNumber(2.540000)) / CSS_PX_PER_INCH;
		case CSS_MM:
			return (number * SVGNumber(25.400000)) / CSS_PX_PER_INCH;
		case CSS_IN:
			return number / CSS_PX_PER_INCH;
		case CSS_PERCENTAGE:
		{
			SVGNumber w = context.viewport_width;
			SVGNumber h = context.viewport_height;

			if (type == SVGLength::SVGLENGTH_X)
				return (number * SVGNumber(100)) / w;
			else if (type == SVGLength::SVGLENGTH_Y)
				return (number * SVGNumber(100)) / h;
			else
				// 100 * sqrt(2) => sqrt(2 * 100^2) ~= 141.421356
				// to attempt to preserve precision for fixed point
				return (number * SVGNumber(141.421356)) / (w*w + h*h).sqrt();
		}
		default:
			return number;
	}
}

SVGNumber SVGUtils::GetExHeight(VisualDevice* vd, const SVGNumber& font_size, int font_number)
{
#ifdef OPFONT_GLYPH_PROPS
	if (vd && font_number >= 0)
	{
		HTMLayoutProperties::SetFontAndSize(vd, font_number, font_size.GetRoundedIntegerValue(), FALSE);
		OpFont::GlyphProps glyph_props;
		if (OpStatus::IsSuccess(vd->GetGlyphProps('x', &glyph_props)))
			return SVGNumber(glyph_props.top);
	}
#endif // OPFONT_GLYPH_PROPS

	// Fallback to font_size (em unit base) / 2
	return font_size / SVGNumber(2);
}

SVGNumber SVGUtils::ResolveLength(const SVGLength& len, SVGLength::LengthOrientation type,
								  const SVGValueContext& context)
{
	SVGNumber number = len.GetScalar();
	int unit = len.GetUnit();
	switch(unit)
	{
		case CSS_EM:
			return number * context.fontsize;
		case CSS_REM:
			return number * context.root_font_size;
		case CSS_EX:
			return number * context.ex_height;
		case CSS_PX:
			return number; // User coordinates already
		case CSS_PT:
			return (number * CSS_PX_PER_INCH) / 72; // Taken from old parser
		case CSS_PC:
			return (number * CSS_PX_PER_INCH) / 6; // Taken from old parser
		case CSS_CM:
			return (number * CSS_PX_PER_INCH) / SVGNumber(2.540000); // Taken from old parser
		case CSS_MM:
			return (number * CSS_PX_PER_INCH) / SVGNumber(25.400000); // Taken from old parser
		case CSS_IN:
			return number * CSS_PX_PER_INCH;
		case CSS_PERCENTAGE:
		{
			SVGNumber w = context.viewport_width;
			SVGNumber h = context.viewport_height;

			if (type == SVGLength::SVGLENGTH_X)
				return (number / 100) * w;
			else if (type == SVGLength::SVGLENGTH_Y)
				return (number / 100) * h;
			else
			{
				SVGNumber cw = w/100;
				SVGNumber ch = h/100;
				return number * (cw*cw + ch*ch).sqrt() / SVGNumber(1.414214); // sqrt(2)
			}
		}
		default:
			return number;
	}
}

// Helper function for HasSystemLanguage
static const char*
GetNextLanguageCode(const char* lang_str,
					const char*& lang_code, unsigned& lang_code_len)
{
	// Eat leading whitespace
	while (op_isspace(*lang_str))
		lang_str++;

	const char* p = lang_str;

	// Scan language code
	while (*p && !op_isspace(*p) && *p != ',' && *p != ';')
		p++;

	lang_code_len = p - lang_str;
	lang_code = lang_str;

	// Scan to next comma
	while (*p && *p != ',')
		p++;

	// Consume comma
	if (*p == ',')
		p++;

	return p;
}

BOOL SVGUtils::HasSystemLanguage(SVGVector *list)
{
	if (!list || list->GetCount() == 0)
		return FALSE;

	const char* lang_iter = g_pcnet->GetAcceptLanguage();
	if (!lang_iter || op_strlen(lang_iter) == 0)
	{
		// No language set. Default to 'en'.
		lang_iter = "en";
	}

	BOOL result = FALSE;
	while (*lang_iter)
	{
		const char* lc = NULL;
		unsigned lc_len = 0;

		lang_iter = GetNextLanguageCode(lang_iter, lc, lc_len);

		if (lc_len == 0)
			// Ignore zero-length language codes
			continue;

		for (UINT32 idx = 0; idx < list->GetCount(); idx++)
		{
			SVGString* sval = static_cast<SVGString*>(list->Get(idx));
			if (sval->GetLength() == lc_len &&
				uni_strncmp(sval->GetString(), lc, lc_len) == 0)
			{
				result = TRUE;
				break; // FIXME: Can't we just return the result here?
			}
		}
	}
	return result;
}

OP_STATUS SVGUtils::GetRectValues(HTML_Element *element, SVGLengthObject** x,
								  SVGLengthObject** y, SVGLengthObject **width,
								  SVGLengthObject **height, SVGLengthObject **rx, SVGLengthObject **ry)
{
	// "If the x attribute isn't specified, the effect is as if 0 was specified"
	OpStatus::Ignore(AttrValueStore::GetLength(element, Markup::SVGA_X, x, NULL));

	// "If the y attribute isn't specified, the effect is as if 0 was specified"
	OpStatus::Ignore(AttrValueStore::GetLength(element, Markup::SVGA_Y, y, NULL));

	// A negative value is an error (but only for the svg element), a value of 0 disables rendering of the element
	OpStatus::Ignore(AttrValueStore::GetLength(element, Markup::SVGA_WIDTH, width, NULL));
	OpStatus::Ignore(AttrValueStore::GetLength(element, Markup::SVGA_HEIGHT, height, NULL));

	// Rx and Ry are optional, but if given then a negative value is an error
	OpStatus::Ignore(AttrValueStore::GetLength(element, Markup::SVGA_RX, rx, NULL));
	OpStatus::Ignore(AttrValueStore::GetLength(element, Markup::SVGA_RY, ry, NULL));

	// For 1.2 tiny rx and ry should just be ignored if negative
	if(*rx && (*rx)->GetScalar() < 0) // this attr is optional
	{
		*rx = NULL;
	}
	if(*ry && (*ry)->GetScalar() < 0) // this attr is optional
	{
		*ry = NULL;
	}

	return OpStatus::OK;
}

void SVGUtils::AdjustRectValues(SVGNumber width, SVGNumber height, SVGNumber& rx, SVGNumber& ry)
{
	if(rx.NotEqual(0))
	{
		SVGNumber halfwidth = width/2;
		if(rx > halfwidth)
		{
			rx = halfwidth;
		}
	}
	if(ry.NotEqual(0))
	{
		SVGNumber halfheight = height/2;
		if(ry > halfheight)
		{
			ry = halfheight;
		}
	}
}

OP_STATUS SVGUtils::GetLineValues(HTML_Element *element, SVGLengthObject** x1,
								  SVGLengthObject** y1, SVGLengthObject** x2,
								  SVGLengthObject** y2)
{
	// "If the x1/y1/x2/y2 attribute isn't specified, the effect is as if 0 was specified"
	AttrValueStore::GetLength(element, Markup::SVGA_X1, x1, NULL);
	AttrValueStore::GetLength(element, Markup::SVGA_Y1, y1, NULL);
	AttrValueStore::GetLength(element, Markup::SVGA_X2, x2, NULL);
	AttrValueStore::GetLength(element, Markup::SVGA_Y2, y2, NULL);
	return OpStatus::OK;
}

void SVGUtils::GetPolygonValues(HTML_Element *element, SVGVector*& list)
{
	GetPolylineValues(element, list);
}

void SVGUtils::GetPolylineValues(HTML_Element *element, SVGVector*& list)
{
	// If we got a partial SVGVector (OpSVGStatus::INVALID_ARGUMENT) just
	// render the poly{line,gon} up to the point that was in error.
	OpStatus::Ignore(AttrValueStore::GetVectorWithStatus(element, Markup::SVGA_POINTS, list));
}

OP_STATUS SVGUtils::GetCircleValues(HTML_Element *element, SVGLengthObject** cx,
									SVGLengthObject** cy, SVGLengthObject **r)
{
	// "A negative value of r is an error"
	RETURN_IF_ERROR(AttrValueStore::GetLength(element, Markup::SVGA_R, r));

	// "If the cx/cy attribute isn't specified, the effect is as if 0 was specified"
	RETURN_IF_ERROR(AttrValueStore::GetLength(element, Markup::SVGA_CX, cx, NULL));
	RETURN_IF_ERROR(AttrValueStore::GetLength(element, Markup::SVGA_CY, cy, NULL));
	return OpStatus::OK;
}

OP_STATUS SVGUtils::GetImageValues(HTML_Element *element, const SVGValueContext& vcxt,
								   SVGRect& image_rect)
{
	SVGLengthObject* width = NULL;
	SVGLengthObject* height = NULL;

	// A negative value is an error, a value of 0 disables rendering of the element
	AttrValueStore::GetLength(element, Markup::SVGA_WIDTH, &width);

	// A negative value is an error, a value of 0 disables rendering of the element
	AttrValueStore::GetLength(element, Markup::SVGA_HEIGHT, &height);

	if (width)
		image_rect.width = ResolveLength(width->GetLength(), SVGLength::SVGLENGTH_X, vcxt);

	if (height)
		image_rect.height = ResolveLength(height->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

	SVGLengthObject* x = NULL;
	SVGLengthObject* y = NULL;

	// "If the x/y attribute isn't specified, the effect is as if 0 was specified"
	AttrValueStore::GetLength(element, Markup::SVGA_X, &x);
	AttrValueStore::GetLength(element, Markup::SVGA_Y, &y);

	if (x)
		image_rect.x = ResolveLength(x->GetLength(), SVGLength::SVGLENGTH_X, vcxt);

	if (y)
		image_rect.y = ResolveLength(y->GetLength(), SVGLength::SVGLENGTH_Y, vcxt);

	return OpStatus::OK;
}

/* static */
OP_STATUS SVGUtils::RescaleOutline(OpBpath* outline, SVGNumber scale_factor)
{
	PathSegListIterator* it = outline->GetPathIterator(TRUE);
	if (it)
	{
		while (SVGPathSeg* seg = it->GetNext())
		{
			switch (seg->info.type)
			{
			case SVGPathSeg::SVGP_CURVETO_CUBIC_ABS:
				seg->x2 *= scale_factor;
				seg->y2 *= scale_factor;
				// fall through
#ifdef SVG_KEEP_QUADRATIC_CURVES
			case SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS:
				seg->x1 *= scale_factor;
				seg->y1 *= scale_factor;
				// fall through
#endif // SVG_KEEP_QUADRATIC_CURVES
			case SVGPathSeg::SVGP_MOVETO_ABS:
			case SVGPathSeg::SVGP_LINETO_ABS:
				seg->x *= scale_factor;
				seg->y *= scale_factor;
				break;
#ifdef _DEBUG
			default:
				OP_ASSERT(!"Unexpected segment type in outline");
			case SVGPathSeg::SVGP_CLOSE:
				break;
#endif // _DEBUG
			}
		}
		OP_DELETE(it);
	}
	else
		return OpStatus::ERR_NO_MEMORY;

	outline->RecalculateBoundingBox();
	return OpStatus::OK;
}

OP_STATUS SVGUtils::GetEnumValueStringRepresentation(SVGEnumType enum_type, UINT32 enum_value, TempBuffer* buffer)
{
	const char* enum_str_rep = SVGEnumUtils::GetEnumName(enum_type, enum_value);
	if (enum_str_rep != NULL)
	{
		return buffer->Append(enum_str_rep, op_strlen(enum_str_rep));
	}
	else
		return OpStatus::ERR;
}

BOOL SVGUtils::ShouldProcessElement(HTML_Element* e)
{	// Don't call this directly. Use SVGElementContext::IsFilteredByRequirements of SVGAnimationContext::IsFilteredByRequirements
	SVGVector* requirement_vector = NULL;

	AttrValueStore::GetVector(e, Markup::SVGA_REQUIREDFEATURES, requirement_vector);
	if (requirement_vector && !HasFeatures(requirement_vector))
	{
		return FALSE;
	}

	AttrValueStore::GetVector(e, Markup::SVGA_REQUIREDEXTENSIONS, requirement_vector);
	if(requirement_vector && !HasRequiredExtensions(requirement_vector))
	{
		return FALSE;
	}

	AttrValueStore::GetVector(e, Markup::SVGA_SYSTEMLANGUAGE, requirement_vector);
	if (requirement_vector && !HasSystemLanguage(requirement_vector))
	{
		return FALSE;
	}

#ifdef SVG_TINY_12
	AttrValueStore::GetVector(e, Markup::SVGA_REQUIREDFORMATS, requirement_vector);
	if (requirement_vector && !HasFormats(requirement_vector))
	{
		return FALSE;
	}

	AttrValueStore::GetVector(e, Markup::SVGA_REQUIREDFONTS, requirement_vector);
	if (requirement_vector && !HasFonts(requirement_vector))
	{
		return FALSE;
	}
#endif // SVG_TINY_12

	return TRUE;
}

OP_STATUS SVGUtils::GetViewportForElement(HTML_Element* element, SVGDocumentContext* doc_ctx,
										  SVGNumberPair& viewport, SVGNumberPair* viewport_xy,
										  SVGMatrix* outtfm)
{
	if (!doc_ctx || !element)
		return OpStatus::ERR;

	// Make a list of the parents from the element to the svgroot
	// We need to go through them in reverse order
	OpVector<HTML_Element> elms;

	HTML_Element* svgroot = doc_ctx->GetSVGRoot();
	if (element != svgroot)
	{
		HTML_Element* parent = element->Parent();
		while (parent && parent != svgroot)
		{
			RETURN_IF_ERROR(elms.Add(parent));

			if (parent == svgroot)
				break;

			parent = parent->Parent();
		}
	}

	RETURN_IF_ERROR(elms.Add(svgroot));

	ViewportInfo vpinfo;
	SVGMatrix accumulated_transform;
	SVGValueContext value_cxt; // FIXME: fontsize needs initialization

	// Traverse from root to target element
	for (UINT32 i = elms.GetCount(); i > 0; i--)
	{
		HTML_Element* iterelm = elms.Get(i-1);
		if (iterelm->Type() == Markup::SVGE_SVG)
		{
			// This all feels very strange, and when rewriting it
			// maybe I lost some of the old behavior. I seriously
			// doubt that it was more correct though, because this
			// seems pretty random.

			// There are two (currently disabled) selftests testing
			// the interaction with use and animation. Whether or not
			// this function might be called for shadow nodes is not
			// known at the moment.

			SVGElementInfo element_info;
			element_info.layouted = element_info.traversed = iterelm;
			element_info.context = NULL;
			element_info.props = NULL;
			element_info.has_resource = 0;

			SVGMatrix dummy_root_ctm;
			RETURN_IF_ERROR(SVGTraversalObject::CalculateSVGViewport(doc_ctx, element_info, value_cxt,
																	 dummy_root_ctm, vpinfo));

			accumulated_transform.PostMultiply(vpinfo.user_transform);
			accumulated_transform.PostMultiply(vpinfo.transform);

			value_cxt.viewport_width = vpinfo.actual_viewport.width;
			value_cxt.viewport_height = vpinfo.actual_viewport.height;
		}
	}

	viewport.x = vpinfo.actual_viewport.width;
	viewport.y = vpinfo.actual_viewport.height;

	if (viewport_xy)
		*viewport_xy = accumulated_transform.ApplyToCoordinate(*viewport_xy);

	if (outtfm)
		outtfm->Copy(accumulated_transform);

	return OpStatus::OK;
}

SVGObjectType
SVGUtils::GetObjectType(Markup::Type element_type, int attr_name, NS_Type ns)
{
	SVGObjectType object_type = SVGOBJECT_UNKNOWN;

	if (ns == NS_SPECIAL)
	{
		switch(attr_name)
		{
			case Markup::SVGA_ANIMATE_TRANSFORM:
				object_type = SVGOBJECT_TRANSFORM;
				break;
			case Markup::SVGA_MOTION_TRANSFORM:
				object_type = SVGOBJECT_MATRIX;
				break;
			case Markup::SVGA_ANIMATED_MARKER_PROP:
				object_type = SVGOBJECT_STRING;
				break;
		}
	}
	else if (ns == NS_SVG)
	{
		switch(attr_name)
		{
#ifdef SVG_SUPPORT_STENCIL
			case Markup::SVGA_CLIP_PATH:
			case Markup::SVGA_MASK:
#endif // SVG_SUPPORT_STENCIL
#ifdef SVG_SUPPORT_FILTERS
			case Markup::SVGA_FILTER:
			case Markup::SVGA_IN:
			case Markup::SVGA_IN2:
			case Markup::SVGA_RESULT:
#endif // SVG_SUPPORT_FILTERS
			case Markup::SVGA_MARKER_MID:
			case Markup::SVGA_MARKER_END:
			case Markup::SVGA_MARKER_START:
			case Markup::SVGA_COLOR_RENDERING:
			case Markup::SVGA_UNICODE:
			case Markup::SVGA_FROM:
			case Markup::SVGA_TO:
			case Markup::SVGA_BY:
			case Markup::SVGA_ONCLICK:
			case Markup::SVGA_GLYPH_NAME:
			case Markup::SVGA_TARGET:
			case Markup::SVGA_BASEPROFILE:
			case Markup::SVGA_WIDTHS:
			case Markup::SVGA_BBOX:
//			case Markup::SVGA_GLYPHREF:
			case Markup::SVGA_ATTRIBUTENAME:
			case Markup::SVGA_VERSION:
			case Markup::SVGA_ORIENTATION:
			case Markup::SVGA_NAME:
				object_type = SVGOBJECT_STRING;
				break;
			case Markup::SVGA_COLOR:
			case Markup::SVGA_LIGHTING_COLOR:
			case Markup::SVGA_FLOOD_COLOR:
			case Markup::SVGA_STOP_COLOR:
			case Markup::SVGA_VIEWPORT_FILL:
			case Markup::SVGA_SOLID_COLOR:
				object_type = SVGOBJECT_COLOR;
				break;
			case Markup::SVGA_ROTATE:
				switch(element_type)
				{
					case Markup::SVGE_ANIMATEMOTION:
						object_type = SVGOBJECT_ROTATE;
						break;
					default:
						object_type = SVGOBJECT_VECTOR;
						break;
				}
				break;
			case Markup::SVGA_PRESERVEASPECTRATIO:
				object_type = SVGOBJECT_ASPECT_RATIO;
				break;
			case Markup::SVGA_DUR:
			case Markup::SVGA_REPEATDUR:
			case Markup::SVGA_MIN:
			case Markup::SVGA_MAX:
				object_type = SVGOBJECT_ANIMATION_TIME;
				break;
			case Markup::SVGA_DX:
			case Markup::SVGA_DY:
#ifdef SVG_SUPPORT_FILTERS
			{
				switch (element_type)
				{
					case Markup::SVGE_FEOFFSET:
						object_type = SVGOBJECT_NUMBER;
						break;
					default:
						object_type = SVGOBJECT_VECTOR;
						break;
				}
			}
			break;
			case Markup::SVGA_TABLEVALUES:
			case Markup::SVGA_KERNELMATRIX:
			case Markup::SVGA_FILTERRES: // FIXME: number-optional-number need special handling?
			case Markup::SVGA_ORDER:
			case Markup::SVGA_KERNELUNITLENGTH:
			case Markup::SVGA_STDDEVIATION:
			case Markup::SVGA_RADIUS:
			case Markup::SVGA_BASEFREQUENCY:
#endif // SVG_SUPPORT_FILTERS
			case Markup::SVGA_VALUES:
			case Markup::SVGA_KEYTIMES:
			case Markup::SVGA_KEYPOINTS:
			case Markup::SVGA_KEYSPLINES:
			case Markup::SVGA_BEGIN:
			case Markup::SVGA_END:
			case Markup::SVGA_STROKE_DASHARRAY:
			case Markup::SVGA_POINTS:
			case Markup::SVGA_G1:
			case Markup::SVGA_G2:
			case Markup::SVGA_U1:
			case Markup::SVGA_U2:
			case Markup::SVGA_REQUIREDFEATURES:
			case Markup::SVGA_SYSTEMLANGUAGE:
#ifdef SVG_SUPPORT_GRADIENTS
			case Markup::SVGA_GRADIENTTRANSFORM:
#endif // SVG_SUPPORT_GRADIENTS
#ifdef SVG_SUPPORT_PATTERNS
			case Markup::SVGA_PATTERNTRANSFORM:
#endif // SVG_SUPPORT_PATTERNS
			case Markup::SVGA_TRANSFORM:
			case Markup::SVGA_REQUIREDEXTENSIONS:
			case Markup::SVGA_FONT_STRETCH:
			case Markup::SVGA_UNICODE_RANGE:
			case Markup::SVGA_PANOSE_1:
			case Markup::SVGA_REQUIREDFORMATS:
			case Markup::SVGA_REQUIREDFONTS:
			case Markup::SVGA_CURSOR:
			case Markup::SVGA_LANG:
				object_type = SVGOBJECT_VECTOR;
				break;
			case Markup::SVGA_FONT_SIZE:
				object_type = SVGOBJECT_FONTSIZE;
				break;
			case Markup::SVGA_STROKE:
				object_type = SVGOBJECT_PAINT;
				break;
			case Markup::SVGA_FONT_FAMILY:
			{
				switch(element_type)
				{
					case Markup::SVGE_FONT_FACE:
						object_type = SVGOBJECT_STRING;
						break;
					default:
						object_type = SVGOBJECT_VECTOR;
						break;
				}
			}
			break;
			case Markup::SVGA_X:
			case Markup::SVGA_Y:
			{
				switch(element_type)
				{
					case Markup::SVGE_TSPAN:
					case Markup::SVGE_TREF:
					case Markup::SVGE_TEXT:
					case Markup::SVGE_ALTGLYPH:
						object_type = SVGOBJECT_VECTOR;
						break;
#ifdef SVG_SUPPORT_FILTERS
					case Markup::SVGE_FEDISTANTLIGHT:
					case Markup::SVGE_FEPOINTLIGHT:
					case Markup::SVGE_FESPOTLIGHT:
						object_type = SVGOBJECT_NUMBER;
						break;
#endif // SVG_SUPPORT_FILTERS
					default:
						object_type = SVGOBJECT_LENGTH;
				}
			}
			break;
			case Markup::SVGA_FILL:
			{
				switch(element_type)
				{
					case Markup::SVGE_ANIMATEMOTION:
					case Markup::SVGE_ANIMATECOLOR:
					case Markup::SVGE_ANIMATE:
					case Markup::SVGE_SET:
					case Markup::SVGE_ANIMATETRANSFORM:
					case Markup::SVGE_ANIMATION:
					case Markup::SVGE_VIDEO:
					case Markup::SVGE_AUDIO:
						object_type = SVGOBJECT_ENUM;
						break;
					default:
						object_type = SVGOBJECT_PAINT;
						break;
				}
			}
			break;
			case Markup::SVGA_CX:
			case Markup::SVGA_CY:
			case Markup::SVGA_FX:
			case Markup::SVGA_FY:
			case Markup::SVGA_X1:
			case Markup::SVGA_X2:
			case Markup::SVGA_Y1:
			case Markup::SVGA_Y2:
				object_type = SVGOBJECT_LENGTH;
				break;
			case Markup::SVGA_K:
#ifdef SVG_SUPPORT_FILTERS
			case Markup::SVGA_K1:
			case Markup::SVGA_K2:
			case Markup::SVGA_K3:
			case Markup::SVGA_K4:
			case Markup::SVGA_AZIMUTH:
			case Markup::SVGA_ELEVATION:
			case Markup::SVGA_Z:
			case Markup::SVGA_POINTSATX:
			case Markup::SVGA_POINTSATY:
			case Markup::SVGA_POINTSATZ:
			case Markup::SVGA_SPECULAREXPONENT:
			case Markup::SVGA_SPECULARCONSTANT:
			case Markup::SVGA_DIFFUSECONSTANT:
			case Markup::SVGA_LIMITINGCONEANGLE:
			case Markup::SVGA_INTERCEPT:
			case Markup::SVGA_AMPLITUDE:
			case Markup::SVGA_EXPONENT:
			case Markup::SVGA_DIVISOR:
			case Markup::SVGA_BIAS:
			case Markup::SVGA_TARGETX:
			case Markup::SVGA_TARGETY:
			case Markup::SVGA_SURFACESCALE:
			case Markup::SVGA_SCALE:
			case Markup::SVGA_NUMOCTAVES:
			case Markup::SVGA_SEED:
#endif
			case Markup::SVGA_ACCENT_HEIGHT:
			case Markup::SVGA_ALPHABETIC:
			case Markup::SVGA_ASCENT:
			case Markup::SVGA_CAP_HEIGHT:
			case Markup::SVGA_DESCENT:
			case Markup::SVGA_HANGING:
			case Markup::SVGA_MATHEMATICAL:
			case Markup::SVGA_OVERLINE_POSITION:
			case Markup::SVGA_OVERLINE_THICKNESS:
			case Markup::SVGA_HORIZ_ADV_X:
			case Markup::SVGA_HORIZ_ORIGIN_X:
			case Markup::SVGA_IDEOGRAPHIC:
			case Markup::SVGA_X_HEIGHT:
			case Markup::SVGA_UNITS_PER_EM:
			case Markup::SVGA_UNDERLINE_POSITION:
			case Markup::SVGA_UNDERLINE_THICKNESS:
			case Markup::SVGA_STRIKETHROUGH_POSITION:
			case Markup::SVGA_STRIKETHROUGH_THICKNESS:
			case Markup::SVGA_STEMH:
			case Markup::SVGA_STEMV:
			case Markup::SVGA_SLOPE:
			case Markup::SVGA_PATHLENGTH:
			case Markup::SVGA_OPACITY:
			case Markup::SVGA_STROKE_OPACITY:
			case Markup::SVGA_FILL_OPACITY:
			case Markup::SVGA_STOP_OPACITY:
			case Markup::SVGA_FLOOD_OPACITY:
			case Markup::SVGA_VIEWPORT_FILL_OPACITY:
			case Markup::SVGA_SOLID_OPACITY:
			case Markup::SVGA_STROKE_MITERLIMIT:
			case Markup::SVGA_LETTER_SPACING:
			case Markup::SVGA_V_ALPHABETIC:
			case Markup::SVGA_V_HANGING:
			case Markup::SVGA_V_IDEOGRAPHIC:
			case Markup::SVGA_V_MATHEMATICAL:
			case Markup::SVGA_WORD_SPACING:
#if defined(SVG_SUPPORT_GRADIENTS) || defined(SVG_SUPPORT_FILTERS)
			case Markup::SVGA_OFFSET:
#endif // SVG_SUPPORT_GRADIENTS || SVG_SUPPORT_FILTERS
			case Markup::SVGA_AUDIO_LEVEL:
				object_type = SVGOBJECT_NUMBER;
				break;
			case Markup::SVGA_SNAPSHOTTIME:
			case Markup::SVGA_LINE_INCREMENT:
				object_type = SVGOBJECT_PROXY;
				break;
			case Markup::SVGA_REPEATCOUNT:
				object_type = SVGOBJECT_REPEAT_COUNT;
				break;
			case Markup::SVGA_R:
			case Markup::SVGA_RX:
			case Markup::SVGA_RY:
			case Markup::SVGA_STROKE_WIDTH:
			case Markup::SVGA_STARTOFFSET:
			case Markup::SVGA_TEXTLENGTH:
			case Markup::SVGA_STROKE_DASHOFFSET:
			case Markup::SVGA_REFX:
			case Markup::SVGA_REFY:
			case Markup::SVGA_MARKERWIDTH:
			case Markup::SVGA_MARKERHEIGHT:
				object_type = SVGOBJECT_LENGTH;
				break;
			case Markup::SVGA_WIDTH:
			case Markup::SVGA_HEIGHT:
				object_type = (element_type == Markup::SVGE_TEXTAREA) ?
					SVGOBJECT_PROXY : SVGOBJECT_LENGTH;
				break;
			case Markup::SVGA_D:
			case Markup::SVGA_PATH:
				object_type = SVGOBJECT_PATH;
				break;
#ifdef SVG_SUPPORT_STENCIL
			case Markup::SVGA_CLIPPATHUNITS:
			case Markup::SVGA_CLIP_RULE:
			case Markup::SVGA_MASKUNITS:
			case Markup::SVGA_MASKCONTENTUNITS:
#endif // SVG_SUPPORT_STENCIL
#ifdef SVG_SUPPORT_FILTERS
			case Markup::SVGA_FILTERUNITS:
			case Markup::SVGA_PRIMITIVEUNITS:
			case Markup::SVGA_MODE:
			case Markup::SVGA_OPERATOR:
			case Markup::SVGA_EDGEMODE:
			case Markup::SVGA_XCHANNELSELECTOR:
			case Markup::SVGA_YCHANNELSELECTOR:
			case Markup::SVGA_STITCHTILES:
			case Markup::SVGA_PRESERVEALPHA:
#endif // SVG_SUPPORT_FILTERS
			case Markup::SVGA_SPREADMETHOD:
			case Markup::SVGA_GRADIENTUNITS:
#ifdef SVG_SUPPORT_PATTERNS
			case Markup::SVGA_PATTERNUNITS:
			case Markup::SVGA_PATTERNCONTENTUNITS:
#endif // SVG_SUPPORT_PATTERNS
			case Markup::SVGA_CALCMODE:
			case Markup::SVGA_VISIBILITY:
			case Markup::SVGA_ADDITIVE:
			case Markup::SVGA_ACCUMULATE:
			case Markup::SVGA_ATTRIBUTETYPE:
			case Markup::SVGA_FILL_RULE:
			case Markup::SVGA_STROKE_LINECAP:
			case Markup::SVGA_STROKE_LINEJOIN:
			case Markup::SVGA_DISPLAY:
			case Markup::SVGA_ZOOMANDPAN:
			case Markup::SVGA_TEXT_ANCHOR:
			case Markup::SVGA_RESTART:
			case Markup::SVGA_POINTER_EVENTS:
			case Markup::SVGA_METHOD:
			case Markup::SVGA_SPACING:
			case Markup::SVGA_LENGTHADJUST:
			case Markup::SVGA_TEXT_DECORATION:
			case Markup::SVGA_OVERFLOW:
			case Markup::SVGA_UNICODE_BIDI:
			case Markup::SVGA_WRITING_MODE:
			case Markup::SVGA_DOMINANT_BASELINE:
			case Markup::SVGA_ALIGNMENT_BASELINE:
			case Markup::SVGA_ARABIC_FORM:
			case Markup::SVGA_DIRECTION:
			case Markup::SVGA_IMAGE_RENDERING:
			case Markup::SVGA_TEXT_RENDERING:
			case Markup::SVGA_MARKERUNITS:
			case Markup::SVGA_COLOR_INTERPOLATION:
			case Markup::SVGA_COLOR_INTERPOLATION_FILTERS:
			case Markup::SVGA_FOCUSABLE:
			case Markup::SVGA_VECTOR_EFFECT:
			case Markup::SVGA_FOCUSHIGHLIGHT:
			case Markup::SVGA_EXTERNALRESOURCESREQUIRED:
			case Markup::SVGA_TRANSFORMBEHAVIOR:
			case Markup::SVGA_OVERLAY:
			case Markup::SVGA_DISPLAY_ALIGN:
			case Markup::SVGA_EDITABLE:
			case Markup::SVGA_TEXT_ALIGN:
			case Markup::SVGA_SYNCMASTER:
			case Markup::SVGA_SYNCBEHAVIOR:
			case Markup::SVGA_SYNCBEHAVIORDEFAULT:
			case Markup::SVGA_SHAPE_RENDERING:
			case Markup::SVGA_INITIALVISIBILITY:
			case Markup::SVGA_BUFFERED_RENDERING:
			case Markup::SVGA_TIMELINEBEGIN:
			case Markup::SVGA_TEXT_OVERFLOW:
				object_type = SVGOBJECT_ENUM;
				break;
			case Markup::SVGA_FONT_WEIGHT:
			case Markup::SVGA_FONT_STYLE:
			case Markup::SVGA_FONT_VARIANT:
				if (element_type == Markup::SVGE_FONT_FACE)
				{
					object_type = SVGOBJECT_VECTOR;
				}
				else
				{
					object_type = SVGOBJECT_ENUM;
				}
				break;
			case Markup::SVGA_TYPE:
				switch(element_type)
				{
					case Markup::SVGE_AUDIO:
					case Markup::SVGE_VIDEO:
					case Markup::SVGE_STYLE:
					case Markup::SVGE_SCRIPT:
						object_type = SVGOBJECT_UNKNOWN;
						break;
					default:
						object_type = SVGOBJECT_ENUM;
				}
				break;
			case Markup::SVGA_CLIP:
			case Markup::SVGA_VIEWBOX:
				object_type = SVGOBJECT_RECT;
				break;
#ifdef SVG_SUPPORT_FILTERS
			case Markup::SVGA_ENABLE_BACKGROUND:
				object_type = SVGOBJECT_ENABLE_BACKGROUND;
				break;
#endif // SVG_SUPPORT_FILTERS
			case Markup::SVGA_BASELINE_SHIFT:
				object_type = SVGOBJECT_BASELINE_SHIFT;
				break;
			case Markup::SVGA_GLYPH_ORIENTATION_HORIZONTAL:
			case Markup::SVGA_GLYPH_ORIENTATION_VERTICAL:
			case Markup::SVGA_ORIENT:
				object_type = SVGOBJECT_ORIENT;
				break;
			case Markup::SVGA_NAV_UP:
			case Markup::SVGA_NAV_UP_LEFT:
			case Markup::SVGA_NAV_UP_RIGHT:
			case Markup::SVGA_NAV_DOWN:
			case Markup::SVGA_NAV_DOWN_LEFT:
			case Markup::SVGA_NAV_DOWN_RIGHT:
			case Markup::SVGA_NAV_LEFT:
			case Markup::SVGA_NAV_RIGHT:
			case Markup::SVGA_NAV_PREV:
			case Markup::SVGA_NAV_NEXT:
				object_type = SVGOBJECT_NAVREF;
				break;
			case Markup::SVGA_CLASS:
				object_type = SVGOBJECT_CLASSOBJECT;
				break;
			default:
				object_type = SVGOBJECT_UNKNOWN;
				break;
		}
	}
	else if (ns == NS_XLINK)
	{
		switch(attr_name)
		{
			case Markup::XLINKA_HREF:
				object_type = SVGOBJECT_URL;
				break;
		}
	}
	return object_type;
}

/* static */ SVGVector*
SVGUtils::CreateSVGVector(SVGObjectType vector_object_type,
						  Markup::Type element_type,
						  Markup::AttrType attr_type)
{
	SVGVector* vector = OP_NEW(SVGVector, (vector_object_type));
	if (vector)
		vector->SetSeparator(GetVectorSeparator(element_type, attr_type));
	return vector;
}

/* static */ SVGVectorSeparator
SVGUtils::GetVectorSeparator(Markup::Type element_type, Markup::AttrType attr_type)
{
	// Default vector separator is "comma or whitespace"
	SVGVectorSeparator separator = SVGVECTORSEPARATOR_COMMA_OR_WSP;
	switch(attr_type)
	{
		case Markup::SVGA_VALUES:
		{
			switch (element_type)
			{
				case Markup::SVGE_FECOLORMATRIX:
					separator = SVGVECTORSEPARATOR_COMMA_OR_WSP;
					break;
				default:
					separator = SVGVECTORSEPARATOR_SEMICOLON;
					break;
			}
		}
		break;
		case Markup::SVGA_POINTS:
		case Markup::SVGA_FONT_FAMILY:
			separator = SVGVECTORSEPARATOR_COMMA_OR_WSP;
			break;
		case Markup::SVGA_BEGIN:
		case Markup::SVGA_END:
		case Markup::SVGA_KEYSPLINES:
		case Markup::SVGA_KEYTIMES:
		case Markup::SVGA_KEYPOINTS:
			separator = SVGVECTORSEPARATOR_SEMICOLON;
			break;
		case Markup::SVGA_LANG:
			separator = SVGVECTORSEPARATOR_COMMA;
			break;
		default:
			break;
	}
	return separator;
}

/* static */ SVGObjectType
SVGUtils::GetVectorObjectType(Markup::Type element_type, Markup::AttrType attr_type)
{
	SVGObjectType object_type = SVGOBJECT_UNKNOWN;
	switch(attr_type)
	{
		case Markup::SVGA_POINTS:
			object_type = SVGOBJECT_POINT;
			break;
		case Markup::SVGA_BEGIN:
		case Markup::SVGA_END:
			object_type = SVGOBJECT_TIME;
			break;
		case Markup::SVGA_X:
		case Markup::SVGA_Y:
		case Markup::SVGA_STROKE_DASHARRAY:
		case Markup::SVGA_DX:
		case Markup::SVGA_DY:
			object_type = SVGOBJECT_LENGTH;
			break;
		case Markup::SVGA_FILTERRES:
		case Markup::SVGA_ORDER:
		case Markup::SVGA_KERNELUNITLENGTH:
		case Markup::SVGA_STDDEVIATION:
		case Markup::SVGA_RADIUS:
		case Markup::SVGA_BASEFREQUENCY:
		case Markup::SVGA_TABLEVALUES:
		case Markup::SVGA_KERNELMATRIX:
		case Markup::SVGA_KEYTIMES:
		case Markup::SVGA_KEYPOINTS:
		case Markup::SVGA_ROTATE:
		case Markup::SVGA_PANOSE_1:
			object_type = SVGOBJECT_NUMBER;
			break;
		case Markup::SVGA_VALUES:
		{
			switch (element_type)
			{
				case Markup::SVGE_FECOLORMATRIX:
					object_type = SVGOBJECT_NUMBER;
					break;
				default:
					object_type = SVGOBJECT_STRING;
					break;
			}
		}
		break;
		case Markup::SVGA_FONT_FAMILY:
		case Markup::SVGA_U1:
		case Markup::SVGA_U2:
		case Markup::SVGA_G1:
		case Markup::SVGA_G2:
		case Markup::SVGA_SYSTEMLANGUAGE:
		case Markup::SVGA_REQUIREDEXTENSIONS:
		case Markup::SVGA_REQUIREDFEATURES:
		case Markup::SVGA_REQUIREDFORMATS:
		case Markup::SVGA_REQUIREDFONTS:
		case Markup::SVGA_FONT_WEIGHT:
		case Markup::SVGA_FONT_STYLE:
		case Markup::SVGA_FONT_VARIANT:
		case Markup::SVGA_FONT_STRETCH:
		case Markup::SVGA_UNICODE_RANGE:
		case Markup::SVGA_LANG:
			object_type = SVGOBJECT_STRING;
			break;
		case Markup::SVGA_CURSOR:
			object_type = SVGOBJECT_ENUM;
			break;
		case Markup::SVGA_KEYSPLINES:
			object_type = SVGOBJECT_KEYSPLINE;
			break;
		case Markup::SVGA_GRADIENTTRANSFORM:
		case Markup::SVGA_PATTERNTRANSFORM:
		case Markup::SVGA_TRANSFORM:
			object_type = SVGOBJECT_TRANSFORM;
			break;
	}
	return object_type;
}

/* static */ SVGEnumType
SVGUtils::GetEnumType(Markup::Type element_type, Markup::AttrType attr_type)
{
	SVGEnumType enum_type = SVGENUM_UNKNOWN;
	switch(attr_type)
	{
		case Markup::SVGA_BUFFERED_RENDERING:
			enum_type = SVGENUM_BUFFERED_RENDERING;
			break;
		case Markup::SVGA_TEXT_RENDERING:
			enum_type = SVGENUM_TEXT_RENDERING;
			break;
		case Markup::SVGA_IMAGE_RENDERING:
			enum_type = SVGENUM_IMAGE_RENDERING;
			break;
		case Markup::SVGA_SHAPE_RENDERING:
			enum_type = SVGENUM_SHAPE_RENDERING;
			break;
		case Markup::SVGA_DIRECTION:
			enum_type = SVGENUM_DIRECTION;
			break;
		case Markup::SVGA_ALIGNMENT_BASELINE:
			enum_type = SVGENUM_ALIGNMENT_BASELINE;
			break;
		case Markup::SVGA_DOMINANT_BASELINE:
			enum_type = SVGENUM_DOMINANT_BASELINE;
			break;
		case Markup::SVGA_WRITING_MODE:
			enum_type = SVGENUM_WRITING_MODE;
			break;
		case Markup::SVGA_UNICODE_BIDI:
			enum_type = SVGENUM_UNICODE_BIDI;
			break;
		case Markup::SVGA_OVERFLOW:
			enum_type = SVGENUM_OVERFLOW;
			break;
		case Markup::SVGA_FILTERUNITS:
		case Markup::SVGA_PRIMITIVEUNITS:
		case Markup::SVGA_CLIPPATHUNITS:
        case Markup::SVGA_MASKUNITS:
        case Markup::SVGA_MASKCONTENTUNITS:
		case Markup::SVGA_GRADIENTUNITS:
        case Markup::SVGA_PATTERNUNITS:
        case Markup::SVGA_PATTERNCONTENTUNITS:
			enum_type = SVGENUM_UNITS_TYPE;
			break;
		case Markup::SVGA_SPREADMETHOD:
			enum_type = SVGENUM_SPREAD_METHOD_TYPE;
			break;
		case Markup::SVGA_CURSOR:
			enum_type = SVGENUM_CURSOR;
			break;
		case Markup::SVGA_CALCMODE:
			enum_type = SVGENUM_CALCMODE;
			break;
		case Markup::SVGA_VISIBILITY:
			enum_type = SVGENUM_VISIBILITY;
			break;
		case Markup::SVGA_FILL:
			enum_type = SVGENUM_ANIMATEFILLTYPE;
			break;
		case Markup::SVGA_ADDITIVE:
			enum_type = SVGENUM_ADDITIVE;
			break;
		case Markup::SVGA_ACCUMULATE:
			enum_type = SVGENUM_ACCUMULATE;
			break;
		case Markup::SVGA_ATTRIBUTETYPE:
			enum_type = SVGENUM_ATTRIBUTE_TYPE;
			break;
		case Markup::SVGA_TYPE:
		{
#ifdef SVG_SUPPORT_FILTERS
			switch (element_type)
			{
			case Markup::SVGE_FETURBULENCE:
				enum_type = SVGENUM_TURBULENCETYPE;
				break;
			case Markup::SVGE_FECOLORMATRIX:
				enum_type = SVGENUM_COLORMATRIXTYPE;
				break;
			case Markup::SVGE_FEFUNCR:
			case Markup::SVGE_FEFUNCG:
			case Markup::SVGE_FEFUNCB:
			case Markup::SVGE_FEFUNCA:
				enum_type = SVGENUM_FUNCTYPE;
				break;
			default:
 				enum_type = SVGENUM_TRANSFORM_TYPE;
 				break;
			}
#else // !SVG_SUPPORT_FILTERS
			enum_type = SVGENUM_TRANSFORM_TYPE;
#endif // !SVG_SUPPORT_FILTERS
		}
		break;
#ifdef SVG_SUPPORT_STENCIL
		case Markup::SVGA_CLIP_RULE:
#endif // SVG_SUPPORT_STENCIL
		case Markup::SVGA_FILL_RULE:
			enum_type = SVGENUM_FILL_RULE;
			break;
		case Markup::SVGA_STROKE_LINECAP:
			enum_type = SVGENUM_STROKE_LINECAP;
			break;
		case Markup::SVGA_STROKE_LINEJOIN:
			enum_type = SVGENUM_STROKE_LINEJOIN;
			break;
		case Markup::SVGA_DISPLAY:
			enum_type = SVGENUM_DISPLAY;
			break;
		case Markup::SVGA_ZOOMANDPAN:
			enum_type = SVGENUM_ZOOM_AND_PAN;
			break;
		case Markup::SVGA_FONT_WEIGHT:
			enum_type = SVGENUM_FONT_WEIGHT;
			break;
		case Markup::SVGA_FONT_STYLE:
			enum_type = SVGENUM_FONT_STYLE;
			break;
		case Markup::SVGA_FONT_VARIANT:
			enum_type = SVGENUM_FONT_VARIANT;
			break;
		case Markup::SVGA_TEXT_ANCHOR:
			enum_type = SVGENUM_TEXT_ANCHOR;
			break;
		case Markup::SVGA_RESTART:
			enum_type = SVGENUM_RESTART;
			break;
		case Markup::SVGA_REQUIREDFEATURES:
			enum_type = SVGENUM_REQUIREDFEATURES;
			break;
		case Markup::SVGA_POINTER_EVENTS:
			enum_type = SVGENUM_POINTER_EVENTS;
			break;
        case Markup::SVGA_METHOD:
            enum_type = SVGENUM_METHOD;
			break;
		case Markup::SVGA_SPACING:
			enum_type = SVGENUM_SPACING;
            break;
		case Markup::SVGA_LENGTHADJUST:
			enum_type = SVGENUM_LENGTHADJUST;
			break;
#ifdef SVG_SUPPORT_FILTERS
		case Markup::SVGA_MODE:
			enum_type = SVGENUM_BLENDMODE;
			break;
		case Markup::SVGA_OPERATOR:
		{
			switch (element_type)
			{
				case Markup::SVGE_FECOMPOSITE:
					enum_type = SVGENUM_COMPOSITEOPERATOR;
					break;
				case Markup::SVGE_FEMORPHOLOGY:
					enum_type = SVGENUM_MORPHOLOGYOPERATOR;
					break;
				default:
					break;
			}
		}
		break;
		case Markup::SVGA_EDGEMODE:
			enum_type = SVGENUM_CONVOLVEEDGEMODE;
			break;
		case Markup::SVGA_XCHANNELSELECTOR:
		case Markup::SVGA_YCHANNELSELECTOR:
			enum_type = SVGENUM_DISPLACEMENTSELECTOR;
			break;
		case Markup::SVGA_STITCHTILES:
			enum_type = SVGENUM_STITCHTILES;
			break;
#endif // SVG_SUPPORT_FILTERS
		case Markup::SVGA_TEXT_DECORATION:
			enum_type = SVGENUM_TEXTDECORATION;
			break;
		case Markup::SVGA_SYNCMASTER:
		case Markup::SVGA_PRESERVEALPHA:
		case Markup::SVGA_EXTERNALRESOURCESREQUIRED:
			enum_type = SVGENUM_BOOLEAN;
			break;
		case Markup::SVGA_ARABIC_FORM:
			enum_type = SVGENUM_ARABIC_FORM;
			break;
		case Markup::SVGA_MARKERUNITS:
			enum_type = SVGENUM_MARKER_UNITS;
			break;
		case Markup::SVGA_COLOR_INTERPOLATION:
		case Markup::SVGA_COLOR_INTERPOLATION_FILTERS:
			enum_type = SVGENUM_COLOR_INTERPOLATION;
			break;
		case Markup::SVGA_FOCUSABLE:
			enum_type = SVGENUM_FOCUSABLE;
			break;
		case Markup::SVGA_VECTOR_EFFECT:
			enum_type = SVGENUM_VECTOR_EFFECT;
			break;
		case Markup::SVGA_FOCUSHIGHLIGHT:
			enum_type = SVGENUM_FOCUSHIGHLIGHT;
			break;
		case Markup::SVGA_TRANSFORMBEHAVIOR:
			enum_type = SVGENUM_TRANSFORMBEHAVIOR;
			break;
		case Markup::SVGA_OVERLAY:
			enum_type = SVGENUM_OVERLAY;
			break;
		case Markup::SVGA_DISPLAY_ALIGN:
			enum_type = SVGENUM_DISPLAY_ALIGN;
			break;
		case Markup::SVGA_EDITABLE:
			enum_type = SVGENUM_EDITABLE;
			break;
		case Markup::SVGA_TEXT_ALIGN:
			enum_type = SVGENUM_TEXT_ALIGN;
			break;
		case Markup::SVGA_WIDTH:
		case Markup::SVGA_HEIGHT:
			if (element_type != Markup::SVGE_TEXTAREA)
				break;
			// Fall-through
		case Markup::SVGA_LINE_INCREMENT:
			enum_type = SVGENUM_AUTO;
			break;
		case Markup::SVGA_SNAPSHOTTIME:
			enum_type = SVGENUM_NONE;
			break;
		case Markup::SVGA_SYNCBEHAVIOR:
		case Markup::SVGA_SYNCBEHAVIORDEFAULT:
			enum_type = SVGENUM_SYNCBEHAVIOR;
			break;
		case Markup::SVGA_INITIALVISIBILITY:
			enum_type = SVGENUM_INITIALVISIBILITY;
			break;
		case Markup::SVGA_TIMELINEBEGIN:
			enum_type = SVGENUM_TIMELINEBEGIN;
			break;
		case Markup::SVGA_TEXT_OVERFLOW:
			enum_type = SVGENUM_TEXT_OVERFLOW;
			break;
		default:
			enum_type = SVGENUM_UNKNOWN;
	}
	return enum_type;
}

/* static */ SVGObjectType
SVGUtils::GetProxyObjectType(Markup::Type element_type, Markup::AttrType attr_type)
{
	SVGObjectType obj_type = SVGOBJECT_UNKNOWN;
	switch (attr_type)
	{
	case Markup::SVGA_LINE_INCREMENT:
		obj_type = SVGOBJECT_NUMBER;
		break;
	case Markup::SVGA_WIDTH:
	case Markup::SVGA_HEIGHT:
		obj_type = SVGOBJECT_LENGTH;
		break;
	case Markup::SVGA_SNAPSHOTTIME:
		obj_type = SVGOBJECT_ANIMATION_TIME;
		break;
	}
	OP_ASSERT(obj_type != SVGOBJECT_UNKNOWN);
	return obj_type;
}

OP_STATUS SVGUtils::GetNumberOptionalNumber(HTML_Element* e, Markup::AttrType attr_type,
											SVGNumberPair& numoptnum, SVGNumber def)
{
	SVGVector* vect;
	AttrValueStore::GetVector(e, attr_type, vect);
	if (vect && vect->GetCount() > 0)
	{
		SVGNumberObject* nval = static_cast<SVGNumberObject *>(vect->Get(0));
		if (nval)
		{
			numoptnum.x = nval->value;

			if (vect->GetCount() > 1)
			{
				nval = static_cast<SVGNumberObject *>(vect->Get(1));
				if (nval)
				{
					numoptnum.y = nval->value;
					return OpStatus::OK;
				}
			}

			// Only one number, return that number as both x and y
			numoptnum.y = numoptnum.x;
			return OpStatus::OK;
		}
	}

	// Fallback to the default value
	numoptnum.x = def;
	numoptnum.y = def;
	return OpStatus::OK;
}

/**
 * Gets the viewbox transform, by using viewport, viewbox and preserveAspectRatio parameters.
 *
 * @param The viewport rect (in userunits)
 * @param viewbox The viewbox (or NULL if none)
 * @param ar The aspectratio (or NULL if none)
 * @param tfm The resulting transform
 * @param clipRect The cliprect
 * @return OpStatus::OK if successful
 */
OP_STATUS SVGUtils::GetViewboxTransform(const SVGRect& viewport, const SVGRect* viewbox,
										 const SVGAspectRatio* ar, SVGMatrix& tfm,
										 SVGRect& clipRect)
{
	SVGAspectRatio ratio;
	SVGNumber xscale(1);
	SVGNumber yscale(1);
	SVGNumber x(0);
	SVGNumber y(0);

	// "...preserveAspectRatio only applies when a value has been provided for viewBox
	//  on the same element. For these elements, if attribute viewBox is not provided,
	//  then preserveAspectRatio is ignored."
	if (viewbox)
	{
		if(ar)
			ratio.Copy(*ar);

		// "A negative value for <width> or <height> is an error (see Error processing).
		//  A value of zero disables rendering of the element."
		if(viewbox->width < 0 || viewbox->height < 0)
		{
			// Bad viewbox dimensions
			return OpSVGStatus::INVALID_ARGUMENT;
		}
		if(viewbox->width.Equal(0) || viewbox->height.Equal(0))
		{
			return OpStatus::OK;
		}

		SVGNumber wq = viewport.width / viewbox->width;
		SVGNumber hq = viewport.height / viewbox->height;
		SVGNumber uniform_scalefactor;

		if(ratio.mos == SVGMOS_MEET)
		{
			uniform_scalefactor = SVGNumber::min_of(wq, hq);
		}
		else
		{
			uniform_scalefactor = SVGNumber::max_of(wq, hq);
		}

#ifdef SVG_SUPPORT_FULL_ASPECT_RATIO
		BOOL non_uniform_scaling = (ratio.align == SVGALIGN_NONE);
#else
		BOOL non_uniform_scaling = (ratio.align != SVGALIGN_XMIDYMID);
#endif // #ifdef SVG_SUPPORT_FULL_ASPECT_RATIO

		if (non_uniform_scaling)
		{
			// Non-uniform scaling
			xscale = wq;
			yscale = hq;
		}
		else
		{
			xscale = uniform_scalefactor;
			yscale = uniform_scalefactor;
		}

		switch(ratio.align)
		{
		case SVGALIGN_XMIDYMID:
			{
				// Align the midpoint X value of the element's viewBox with the midpoint X value of the viewport.
				// Align the midpoint Y value of the element's viewBox with the midpoint Y value of the viewport.
				x = (viewport.width - viewbox->width*uniform_scalefactor) / 2;
				y = (viewport.height - viewbox->height*uniform_scalefactor) / 2;
			}
			break;

#ifdef SVG_SUPPORT_FULL_ASPECT_RATIO
		case SVGALIGN_XMINYMIN:
			{
				// Align the <min-x> of the element's viewBox with the smallest X value of the viewport.
				// Align the <min-y> of the element's viewBox with the smallest Y value of the viewport.
				x = 0;
				y = 0;
			}
			break;
		case SVGALIGN_XMINYMID:
			{
				// Align the <min-x> of the element's viewBox with the smallest X value of the viewport.
				// Align the midpoint Y value of the element's viewBox with the midpoint Y value of the viewport.
				x = 0;
				y = (viewport.height - viewbox->height*uniform_scalefactor) / 2;
			}
			break;
		case SVGALIGN_XMINYMAX:
			{
				// Align the <min-x> of the element's viewBox with the smallest X value of the viewport.
				// Align the <min-y>+<height> of the element's viewBox with the maximum Y value of the viewport.
				x = 0;
				y = viewport.height - ((viewbox->height + viewbox->y) * uniform_scalefactor);
			}
			break;
		case SVGALIGN_XMIDYMIN:
			{
				// Align the midpoint X value of the element's viewBox with the midpoint X value of the viewport.
				// Align the <min-y> of the element's viewBox with the smallest Y value of the viewport.
				x = (viewport.width - viewbox->width*uniform_scalefactor) / 2;
				y = 0;
			}
			break;
		case SVGALIGN_XMIDYMAX:
			{
				// Align the midpoint X value of the element's viewBox with the midpoint X value of the viewport.
				// Align the <min-y>+<height> of the element's viewBox with the maximum Y value of the viewport.
				x = (viewport.width - viewbox->width*uniform_scalefactor) / 2;
				y = viewport.height - ((viewbox->height + viewbox->y) * uniform_scalefactor);
			}
			break;
		case SVGALIGN_XMAXYMIN:
			{
				// Align the <min-x>+<width> of the element's viewBox with the maximum X value of the viewport.
				// Align the <min-y> of the element's viewBox with the smallest Y value of the viewport.
				x = viewport.width - ((viewbox->width + viewbox->x) * uniform_scalefactor);
				y = 0;
			}
			break;
		case SVGALIGN_XMAXYMID:
			{
				// Align the <min-x>+<width> of the element's viewBox with the maximum X value of the viewport.
				// Align the midpoint Y value of the element's viewBox with the midpoint Y value of the viewport.
				x = viewport.width - ((viewbox->width + viewbox->x) * uniform_scalefactor);
				y = (viewport.height - viewbox->height*uniform_scalefactor) / 2;
			}
			break;
		case SVGALIGN_XMAXYMAX:
			{
				// Align the <min-x>+<width> of the element's viewBox with the maximum X value of the viewport.
				// Align the <min-y>+<height> of the element's viewBox with the maximum Y value of the viewport.
				x = viewport.width - ((viewbox->width + viewbox->x) * uniform_scalefactor);
				y = viewport.height - ((viewbox->height + viewbox->y) * uniform_scalefactor);
			}
			break;
#endif // SVG_SUPPORT_FULL_ASPECT_RATIO
		}
	}

	//
	// The viewbox transforms for dummies:
	// -----------------------------------
	//
	// Ttot = T(viewport) * T(align pos) * S(align scale) * T(-viewbox pos)
	//
	// Evaluation gives:
	//
	// Ttot = [ asx |  0  | vpx + apx - vbx * asx ]
	//        [  0  | asy | vpy + apy - vby * asy ]
	//        [  0  |  0  |           1           ]
	//
	// NOTE (for eager optimizers):
	// Setting above matrix (Ttot) directly would require 4 adds and 2 muls,
	// while the code below does 6 adds and 6 muls (worst case).
	//

	// Place viewbox (x,y) at origin
	if(viewbox && (viewbox->x.NotEqual(0) || viewbox->y.NotEqual(0)))
	{
		tfm.MultTranslation(-viewbox->x, -viewbox->y);
	}

	// Apply aspect ratio scaling
	if(xscale.NotEqual(SVGNumber(1)) || yscale.NotEqual(SVGNumber(1)))
	{
		tfm.MultScale(xscale, yscale);
	}

	// Translate to aligned position
	if(x.NotEqual(0) || y.NotEqual(0))
	{
		tfm.MultTranslation(x, y);
	}

	// Translate to viewport
	tfm.MultTranslation(viewport.x, viewport.y);

	SVGNumber w = viewport.width;
	SVGNumber h = viewport.height;
	SVGNumber xoff, yoff;
	if (viewbox)
	{
		w = viewbox->width;
		h = viewbox->height;
		xoff = viewbox->x;
		yoff = viewbox->y;
	}

	if(ratio.mos == SVGMOS_SLICE)
	{
		x = xoff - x/xscale;
		y = yoff - y/yscale;

		w = MIN(w, viewport.width/xscale);
		h = MIN(h, viewport.height/yscale);
	}
	else
	{
		x = xoff;
		y = yoff;
	}
	clipRect.Set(x,y,w,h);

	return OpStatus::OK;
}

/* static */
HTML_Element* SVGUtils::GetRealNode(HTML_Element* shadow_elm)
{
	OP_ASSERT(shadow_elm && IsShadowNode(shadow_elm));

	// This pointer is made to cause random crashes (unless we somehow manage to always maintain it correctly)
	HTML_Element* layouted_elm = static_cast<HTML_Element*>(shadow_elm->GetSpecialAttr(Markup::SVGA_REAL_NODE, ITEM_TYPE_NUM, NULL, SpecialNs::NS_SVG));

	OP_ASSERT(layouted_elm);
	OP_ASSERT(!IsShadowNode(layouted_elm));

	return layouted_elm;
}

/* static */ BOOL
SVGUtils::IsPresentationAttribute(Markup::AttrType attr_name)
{
	return (s_presentation_attributes_bitset[attr_name/32] & (1u << (attr_name % 32))) != 0;
}

/* static */ BOOL
SVGUtils::IsPresentationAttribute(Markup::AttrType attr_name, Markup::Type elm_type)
{
	BOOL is_pres_attr = IsPresentationAttribute(attr_name);
	if (is_pres_attr)
		// Filter based on element type
		switch (attr_name)
		{
		case Markup::SVGA_FONT_WEIGHT:
		case Markup::SVGA_FONT_STYLE:
		case Markup::SVGA_FONT_SIZE:
		case Markup::SVGA_FONT_FAMILY:
			is_pres_attr = (elm_type != Markup::SVGE_FONT_FACE);
			break;
		case Markup::SVGA_FILL:
			is_pres_attr = !(elm_type == Markup::SVGE_ANIMATE ||
							 elm_type == Markup::SVGE_ANIMATECOLOR ||
							 elm_type == Markup::SVGE_ANIMATEMOTION ||
							 elm_type == Markup::SVGE_ANIMATETRANSFORM ||
							 elm_type == Markup::SVGE_ANIMATION ||
							 elm_type == Markup::SVGE_AUDIO ||
							 elm_type == Markup::SVGE_SET ||
							 elm_type == Markup::SVGE_VIDEO);
			break;
		}
	return is_pres_attr;
}

/* static */
BOOL SVGUtils::AttributeAffectsFontMetrics(Markup::AttrType attr, Markup::Type elm_type)
{
	return (attr == Markup::SVGA_FONT_FAMILY ||
			attr == Markup::SVGA_FONT ||
			attr == Markup::SVGA_FONT_SIZE ||
			attr == Markup::SVGA_FONT_STYLE ||
			attr == Markup::SVGA_FONT_VARIANT ||
			attr == Markup::SVGA_FONT_WEIGHT ||
			// attr == Markup::SVGA_FONT_SIZE_ADJUST || // we don't support this yet
			attr == Markup::SVGA_TEXT_RENDERING ||
			attr == Markup::SVGA_LINE_INCREMENT ||
			attr == Markup::SVGA_UNICODE_BIDI ||
			attr == Markup::SVGA_WORD_SPACING ||
			attr == Markup::SVGA_WRITING_MODE ||
			attr == Markup::SVGA_LETTER_SPACING ||
			attr == Markup::SVGA_GLYPH_ORIENTATION_VERTICAL || // NOTE: This property is applied only to text written in a vertical 'writing-mode'.
			attr == Markup::SVGA_GLYPH_ORIENTATION_HORIZONTAL || // NOTE: This property is applied only to text written in a horizontal 'writing-mode'.

			// only non-presentation attributes below here:
			attr == Markup::SVGA_LENGTHADJUST ||
			attr == Markup::SVGA_TEXTLENGTH ||
			(elm_type == Markup::SVGE_TEXTAREA &&
				(attr == Markup::SVGA_WIDTH ||
				 attr == Markup::SVGA_HEIGHT ||
				 attr == Markup::SVGA_TEXT_ALIGN ||
				 attr == Markup::SVGA_DISPLAY_ALIGN))

// Uncommented ones here are maybes:
			// attr == Markup::SVGA_PRESERVEASPECTRATIO
			// attr == Markup::SVGA_DOMINANT_BASELINE // (pres.attr)
			// attr == Markup::SVGA_ALIGNMENT_BASELINE // (pres.attr)
			// attr == Markup::SVGA_METHOD ||
			// attr == Markup::SVGA_TRANSFORM ||
			// attr == Markup::SVGA_DIRECTION // (pres.attr)
			// attr == Markup::SVGA_TEXT_ANCHOR // (pres.attr)
			// attr == Markup::SVGA_TEXT_DECORATION // (pres.attr)
			);
}

/* static */
BOOL SVGUtils::GetPreserveSpaces(HTML_Element* element)
{
	while (element)
	{
		// The attributes are on the "real" nodes if this is a
		// shadownode, but the traverse should be done in the
		// shadowtree
		HTML_Element* layouted_elm = GetElementToLayout(element);
		if (layouted_elm->HasAttr(XMLA_SPACE, NS_IDX_XML))
			return layouted_elm->GetBoolAttr(XMLA_SPACE, NS_IDX_XML);

		element = element->ParentActual();
	}
	return FALSE;
}

static OP_STATUS SetupChunks(HTML_Element* elm, SVGDocumentContext* doc_ctx,
							 OpVector<SVGTextChunk>** chunks_out, const SVGNumberPair& initial_viewport)
{
	// Since GetTextElementExtent expects layouted elements, we assume
	// that is the case here as well. This is probably an incorrect assumption.
	if (!elm || !SVGUtils::IsTextClassType(elm->Type()))
		return OpStatus::ERR;

	*chunks_out = NULL;

	SVGElementContext* elem_ctx = AttrValueStore::AssertSVGElementContext(elm);
	if (!elem_ctx)
		return OpStatus::ERR_NO_MEMORY;

	SVGTextElement* text_ctx = static_cast<SVGTextElement*>(elem_ctx);

	OP_STATUS status = OpStatus::OK;
	OpVector<SVGTextChunk>* chunk_list = &text_ctx->GetChunkList();
	if (chunk_list->GetCount() == 0)
	{
		SVGTextData data(SVGTextData::EXTENT);
		data.chunk_list = chunk_list;

		status = SVGUtils::GetTextElementExtent(elm, doc_ctx, 0, -1, data, initial_viewport);
	}

	*chunks_out = chunk_list;
	return status;
}

/* static */
OP_STATUS SVGUtils::GetTextElementExtent(HTML_Element* elm, SVGDocumentContext* doc_ctx,
										 int start_index, int num_chars, SVGTextData& data,
										 const SVGNumberPair& initial_viewport,
										 SVGCanvas* canvas, BOOL needs_chunks)
{
	OP_NEW_DBG("GetTextElementExtent", "svg_textelementextent");
	OP_DBG(("Before"));

	Markup::Type type = elm->Type();
	if (elm->GetNsType() != NS_SVG || !IsTextClassType(type))
	{
		OP_ASSERT(!"Bad element type");
		return OpStatus::ERR;
	}

	if (start_index < 0)
		return OpStatus::ERR_OUT_OF_RANGE;

	HTML_Element* textroot = GetTextRootElement(elm);

	if (!textroot)
		return OpStatus::ERR;

	SVGElementContext* textroot_ctx = AttrValueStore::AssertSVGElementContext(textroot);

	if (!textroot_ctx)
		return OpStatus::ERR_NO_MEMORY;

	SVGTextRootContainer* container = textroot_ctx->GetAsTextRootContainer();
	OP_ASSERT(container);

	// Cascading detached trees can crash.
	OP_ASSERT(doc_ctx && doc_ctx->GetSVGImage()->IsInTree());

	SVGNullCanvas dummy_canvas;
	SVGIntersectionCanvas isect_canvas;

	if (!canvas)
	{
		SVGMatrix ctm;

		if (data.NeedsRenderMode())
		{
			if (VisualDevice* vis_dev = doc_ctx->GetVisualDevice())
				isect_canvas.SetBaseScale(SVGNumber(vis_dev->GetScale()) / 100);

			isect_canvas.ResetTransform();

			SVGNumberPair isect_pt;
			if (data.SetCharNumAtPosition())
			{
				RETURN_IF_ERROR(SVGUtils::GetElementCTM(elm, doc_ctx, &ctm, TRUE));

				isect_pt = ctm.ApplyToCoordinate(data.startpos);
			}

			isect_canvas.SetIntersectionMode(isect_pt);

			canvas = &isect_canvas;
		}
		else
		{
			canvas = &dummy_canvas;
		}

		canvas->SetDefaults(75); // Default low quality since it really doesn't matter
		canvas->ConcatTransform(ctm);
	}

	OP_ASSERT(canvas);

	// We need this policy in some case - maybe we should pass the
	// policy to this function?
	SVGLogicalTreeChildIterator ltci;
	SVGTextMeasurementObject text_meas_object(&ltci);

	RETURN_IF_ERROR(text_meas_object.SetupResolver());

	text_meas_object.SetCurrentViewport(initial_viewport);
	text_meas_object.SetDocumentContext(doc_ctx);
	text_meas_object.SetCanvas(canvas);

	// Transfer tree state from parents.
	text_meas_object.SetInitialInvalidState(SVGElementContext::ComputeInvalidState(elm));

	RETURN_IF_ERROR(text_meas_object.CreateTextInfo(container, &data));

	SVGTextArguments* textinfo = text_meas_object.GetTextParams();

	// make sure the preserve_spaces flag is set correctly, it can affect results
	textinfo->packed.preserve_spaces = GetPreserveSpaces(elm) ? 1 : 0;

	if (needs_chunks && !data.SetExtent())
	{
		RETURN_IF_ERROR(SetupChunks(elm, doc_ctx, &textinfo->chunk_list, initial_viewport));

		textinfo->current_chunk = 0;
	}

	if (num_chars >= 0)
		textinfo->max_glyph_idx = start_index + num_chars;

	OP_STATUS err = SVGTraverser::Traverse(&text_meas_object, elm, NULL);

	OP_DBG(("After"));
	return err;
}

/* static */
OP_STATUS SVGUtils::DrawImage(OpBitmap* bitmap,
							  LayoutProperties* layprops, SVGCanvas* canvas,
							  const SVGRect& dst_rect, SVGAspectRatio* ar, int quality)
{
	SVGRect src(0, 0, bitmap->Width(), bitmap->Height());

	SVGMatrix viewboxtransform;
	SVGRect image_rect;
	RETURN_IF_ERROR(GetViewboxTransform(dst_rect, &src, ar, viewboxtransform, image_rect));

#ifdef SVG_SUPPORT_STENCIL
	BOOL has_clip = FALSE;
	if (layprops)
	{
		const HTMLayoutProperties& props = *layprops->GetProps();
		if (props.overflow_x == CSS_VALUE_hidden || props.overflow_x == CSS_VALUE_scroll)
		{
			SVGRect clip = dst_rect;
			OpStatus::Ignore(AdjustCliprect(clip, props));
			if (!clip.IsEqual(dst_rect))
				has_clip = OpStatus::IsSuccess(canvas->AddClipRect(clip));
		}
		else
		{
			image_rect = src;
		}
	}
#endif // SVG_SUPPORT_STENCIL

	canvas->ConcatTransform(viewboxtransform);

	OpRect bm_rect = image_rect.GetSimilarRect();

	OP_STATUS result = canvas->DrawImage(bitmap, bm_rect, image_rect,
										 (SVGCanvas::ImageRenderQuality)quality);

#ifdef SVG_SUPPORT_STENCIL
	if (has_clip)
		canvas->RemoveClip();
#endif // SVG_SUPPORT_STENCIL
	return result;
}

/* static */
OP_STATUS SVGUtils::DrawImageFromURL(SVGDocumentContext* doc_ctx, URL* imageURL, HTML_Element* layouted_elm,
									 LayoutProperties* layprops, SVGCanvas* canvas,
									 const SVGRect& dst_rect, SVGAspectRatio* ar, int quality)
{
	OP_ASSERT(imageURL);

	OP_STATUS result = OpSVGStatus::DATA_NOT_LOADED_ERROR;
	UrlImageContentProvider	*provider = UrlImageContentProvider::FindImageContentProvider(*imageURL);
	if(!provider)
	{
		provider = OP_NEW(UrlImageContentProvider, (*imageURL));
		if(!provider)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	SVG_DOCUMENT_CLASS* doc = doc_ctx->GetDocument();

#ifdef _PRINT_SUPPORT_
	// Call this magic function (magic to me, at least) that creates a
	// fitting HEListElm for images in print documents that we fetch
	// below.
	if(doc->IsPrintDocument())
	{
		doc->GetHLDocProfile()->AddPrintImageElement(layouted_elm, IMAGE_INLINE, imageURL);
	}
#endif // _PRINT_SUPPORT_

	HEListElm* hle = layouted_elm->GetHEListElm(FALSE);
	if (!hle)
		return OpStatus::ERR;

	provider->IncRef();
	{ // Scope so that the Image object is destroyed before the provider.

		// to force loading - we use a big rectangle to make sure that
		// the image isn't undisplayed by accident when the user
		// scrolls The best rect would be the position and size of the
		// svg in the document, but we don't have access to that here.
		hle->Display(doc, AffinePos(), 20000, 1000000, FALSE, FALSE); // Triggers an IncVisible that the HEListElm owns. Unfortunately gets the wrong coordinates.

		Image img = provider->GetImage();

		RETURN_IF_ERROR(img.OnLoadAll(provider));

		if (img.ImageDecoded())
		{
			OpBitmap* bm = NULL;
			if(img.IsAnimated())
				bm = img.GetBitmap(hle); // won't find proper frame of animation unless using the HEListElm here
			else
				bm = img.GetBitmap(null_image_listener);

			if (bm)
			{
				result = DrawImage(bm, layprops, canvas, dst_rect, ar, quality);
				img.ReleaseBitmap();
			}
		}
		else if (img.IsFailed())
		{
			result = OpStatus::ERR;
		}
	} // Here the img object leaves scope and is destroyed so that we can decref the provider.
	provider->DecRef();

	return result;
}

BOOL SVGUtils::IsViewportElement(Markup::Type type)
{
	/**
	* "The following elements establish new viewports:
	*   - The 'svg' element
	*   - A 'symbol' element define new viewports whenever they are instanced by a 'use' element.
	*   - An 'image' element that references an SVG file will result in the establishment of a
	*     temporary new viewport since the referenced resource by definition will have an 'svg' element.
	*   - A 'foreignObject' element creates a new viewport for rendering the content that is within
	*     the element."
	*/
	switch(type)
	{
	case Markup::SVGE_SVG:
	case Markup::SVGE_IMAGE:
	case Markup::SVGE_SYMBOL:
	case Markup::SVGE_FOREIGNOBJECT:
	case Markup::SVGE_VIDEO:
	case Markup::SVGE_ANIMATION:
		return TRUE;
	}
	return FALSE;
}

/* static */ HTML_Element*
SVGUtils::GetViewportElement(HTML_Element* elm, BOOL find_nearest, BOOL find_svg_only)
{
	HTML_Element* parent, *last_viewport = NULL;
	parent = elm->ParentActual();

	while (parent)
	{
		if ((find_svg_only && parent->IsMatchingType(Markup::SVGE_SVG, NS_SVG)) ||
			(parent->GetNsType() == NS_SVG && IsViewportElement(parent->Type())))
		{
			last_viewport = parent;
			if (find_nearest)
				break;
		}

		parent = parent->ParentActual();
	}

	return last_viewport;
}

/* static */ OP_BOOLEAN
SVGUtils::GetTransformToElement(HTML_Element* from_elm, HTML_Element* to_elm,
								SVGDocumentContext* doc_ctx, SVGMatrix& elm_transform)
{
	SVGMatrix from_xfrm;
	RETURN_IF_ERROR(GetElementCTM(from_elm, doc_ctx, &from_xfrm));
	SVGMatrix to_xfrm;
	RETURN_IF_ERROR(GetElementCTM(to_elm, doc_ctx, &to_xfrm));

	SVGMatrix to_xfrm_inv;
	to_xfrm_inv.Copy(to_xfrm);
	if(!to_xfrm_inv.Invert())
		return OpBoolean::IS_FALSE;

	elm_transform.Copy(from_xfrm);
	elm_transform.Multiply(to_xfrm_inv);

	return OpBoolean::IS_TRUE;
}

/* static */ OP_STATUS
SVGUtils::GetElementCTM(HTML_Element* target, SVGDocumentContext* doc_ctx,
						SVGMatrix* element_ctm, BOOL screen_ctm /* = FALSE */)
{
	OP_NEW_DBG("SVGUtils::GetElementCTM", "svg_transform");

	// Cascading detached trees can crash.
	OP_ASSERT(doc_ctx && doc_ctx->GetSVGImage()->IsInTree());

	HTML_Element* svgroot = GetTopmostSVGRoot(target);
	OpVector<HTML_Element> elms;

	if (target != svgroot)
	{
		HTML_Element* viewport_elm = GetViewportElement(target, !screen_ctm);
		if (!viewport_elm)
			return OpStatus::ERR;

		HTML_Element* parent = target->ParentActual();
		while (parent)
		{
			RETURN_IF_ERROR(elms.Insert(0, parent));
			if (parent == viewport_elm)
				break;

			parent = parent->ParentActual();
		}
	}

	RETURN_IF_ERROR(elms.Add(target));

	SVGTreePathChildIterator tpci(&elms);
	SVGTransformTraversalObject transform_object(&tpci, &elms);

	RETURN_IF_ERROR(transform_object.SetupResolver());

	SVGNullCanvas dummy_canvas;
	transform_object.SetDocumentContext(doc_ctx);
	transform_object.SetCanvas(&dummy_canvas);

	OP_STATUS status = SVGTraverser::Traverse(&transform_object, elms.Get(0), NULL);

	*element_ctm = transform_object.GetCalculatedCTM();

	OP_ASSERT(elms.GetCount() == 0); // Or we haven't visited all nodes we should have visited
	return status;
}

/* static */ OP_STATUS
SVGUtils::AdjustCliprect(SVGRect& cliprect, const HTMLayoutProperties& props)
{
	if (props.clip_left != CLIP_AUTO && props.clip_left != CLIP_NOT_SET)
	{
		cliprect.width -= int(props.clip_left);
		cliprect.x += int(props.clip_left);
	}
	if (props.clip_right != CLIP_AUTO && props.clip_right != CLIP_NOT_SET)
		cliprect.width = SVGNumber(int(props.clip_right)) - cliprect.x;

	if (props.clip_top != CLIP_AUTO && props.clip_top != CLIP_NOT_SET)
	{
		cliprect.height -= int(props.clip_top);
		cliprect.y += int(props.clip_top);
	}
	if (props.clip_bottom != CLIP_AUTO && props.clip_bottom != CLIP_NOT_SET)
		cliprect.height = SVGNumber(int(props.clip_bottom)) - cliprect.y;

	// FIXME: Clip values if < 0 ?
	cliprect.width = (cliprect.width < 0) ? 0 : cliprect.width;
	cliprect.height = (cliprect.height < 0) ? 0 : cliprect.height;

	return OpStatus::OK;
}

/* static */ OP_STATUS
SVGUtils::ShrinkToFit(int& width, int& height, int max_width, int max_height)
{
	if (width < 0)
		width = max_width;
	if (height < 0)
		height = max_height;

	if (height > max_height)
	{
		width = max_height * width / height;
		height = max_height;
	}

	if (width > max_width)
	{
		height = max_width * height / width;
		width = max_width;
	}

	if (width <= 0)
		width = 1;
	if (height <= 0)
		height = 1;
	return OpStatus::OK;
}

/* static */ HTML_Element*
SVGUtils::FindDocumentRelNode(LogicalDocument *logdoc, const uni_char* url_rel)
{
	OP_ASSERT(url_rel != NULL);

	HTML_Element* target = NULL;

	OpString id_str;
	const uni_char* id = url_rel;
	// A very simple xpointer parser
	if (uni_strncmp(url_rel, "xpointer(id(", 12) == 0)
	{
		uni_char quote_char = url_rel[12];
		if (quote_char == '\'' || quote_char == '\"')
		{
			url_rel += 13; // Skip the prefix
			unsigned rest_len = uni_strlen(url_rel);
			uni_char expected_suffix[4] = { quote_char, ')', ')', '\0' };
			if (uni_strstr(url_rel, expected_suffix) == url_rel + rest_len - 3)
			{
				// Any error here will only cause us to return NULL anyway
				OpStatus::Ignore(id_str.Set(url_rel, rest_len - 3));
				id = id_str.CStr();
			}
		}
	}

	if (id != NULL)
	{
		target = FindElementById(logdoc, id);
	}

	return target;
}

/* static */ HTML_Element*
SVGUtils::FindURLRelReferredNode(SVGElementResolver* resolver,
								SVGDocumentContext* doc_ctx, HTML_Element *traversed_elm,
								SVGURL& rel_url)
{
	HTML_Element* layouted_elm = GetElementToLayout(traversed_elm);
	SVGDocumentContext* current_doc_ctx = doc_ctx;

	if(traversed_elm != layouted_elm)
	{
		// In external document
		current_doc_ctx = AttrValueStore::GetSVGDocumentContext(layouted_elm);
		if(!current_doc_ctx)
			return NULL;
	}

	URL url = rel_url.GetURL(current_doc_ctx->GetURL(), traversed_elm); // layouted or traversed elm? xmlbase involved
	OpString str;
	if (OpStatus::IsError(url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI, str)))
		return NULL;

	if (str.IsEmpty())
		return NULL;

	URL doc_url = current_doc_ctx->GetURL();
	URL redirected_to = doc_url.GetAttribute(URL::KMovedToURL,TRUE);
	if (!redirected_to.IsEmpty())
	{
		doc_url = redirected_to;
	}

	URL base_url = ResolveBaseURL(doc_url, layouted_elm);

	HTML_Element* target = NULL;
	if (base_url.Type() == URL_DATA)
	{
		const uni_char* id = uni_strchr(str.CStr(), '#');
		if(!id)
			id = str.CStr();
		else
			id++;

		target = SVGUtils::FindElementById(current_doc_ctx->GetLogicalDocument(), id);
	}
	else
	{
		URL target_url = g_url_api->GetURL(base_url, str.CStr());
		target = current_doc_ctx->GetElementByReference(resolver, layouted_elm, target_url);
	}

	if(target && ((resolver && resolver->IsLoop(target)) || target == traversed_elm))
	{
		target = NULL;
	}

	return target; // FIXME: Report OOM
}

/* static */ HTML_Element *
SVGUtils::FindURLReferredNode(SVGElementResolver* resolver, SVG_DOCUMENT_CLASS* document, HTML_Element* frame_element, const uni_char *url_rel /* = NULL */)
{
	HTML_Element *target = NULL;
	FramesDocElm *frame = FramesDocElm::GetFrmDocElmByHTML(frame_element);

	if (frame != NULL)
	{
		SVG_DOCUMENT_CLASS *frame_doc = frame->GetCurrentDoc();
		if(frame_doc && frame_doc->IsLoaded())
		{
			LogicalDocument *logdoc = frame_doc->GetLogicalDocument();

			if (logdoc != NULL)
			{
				if (url_rel)
					target = FindDocumentRelNode(logdoc, url_rel);
				else
					target = logdoc->GetDocRoot();

				if(target && (target->GetNsType() != NS_SVG || (resolver && resolver->IsLoop(target))))
				{
					//We expected an SVG element
					target = NULL;
				}
			}
		}
	}

	return target;
}

/* static */ HTML_Element *
SVGUtils::FindHrefReferredNode(SVGElementResolver* resolver, SVGDocumentContext* doc_ctx, HTML_Element* elm, BOOL* has_xlink_href_ptr /* = NULL */)
{
	OP_ASSERT(elm);

	URL doc_url = doc_ctx->GetURL();
	URL redirected_to = doc_url.GetAttribute(URL::KMovedToURL,TRUE);
	if (!redirected_to.IsEmpty())
	{
		doc_url = redirected_to;
	}

	URL *url = NULL;
	BOOL has_xlink_href = (OpStatus::IsSuccess(AttrValueStore::GetXLinkHREF(doc_url, elm, url,
																			SVG_ATTRFIELD_DEFAULT)) &&
						   url != NULL);

	if (has_xlink_href_ptr)
	{
		*has_xlink_href_ptr = has_xlink_href;
	}

	HTML_Element* target = NULL;
	if (has_xlink_href)
	{
		target = doc_ctx->GetElementByReference(resolver, elm, *url);

		if (target && resolver && resolver->IsLoop(target))
		{
			target = NULL;
		}
	}

	return target;
}

/* static */ BOOL
SVGUtils::HasBuiltShadowTree(HTML_Element* elm, BOOL animated)
{
	//OP_ASSERT(SVGUtils::GetElementToLayout(elm)->IsMatchingType(Markup::SVGE_USE, NS_SVG));
	const int default_value = 0;
	int built_flags = ATTR_VAL_AS_NUM(elm->GetSpecialAttr(Markup::SVGA_SHADOW_TREE_BUILT, ITEM_TYPE_NUM,
		NUM_AS_ATTR_VAL(default_value), SpecialNs::NS_SVG));
	return (built_flags & (animated ? 0x1 : 0x2 )) != 0;
}

/* static */
BOOL SVGUtils::IsShadowNode(HTML_Element* elm)
{
	return IsShadowType(elm->Type()) && elm->GetNsType() == NS_SVG;
}

/* static */ void
SVGUtils::MarkShadowTreeAsBuilt(HTML_Element* elm, BOOL is_built, BOOL animated)
{
	//OP_ASSERT(SVGUtils::GetElementToLayout(elm)->IsMatchingType(Markup::SVGE_USE, NS_SVG));
	const int default_value = 0;
	int built_flags = ATTR_VAL_AS_NUM(elm->GetSpecialAttr(Markup::SVGA_SHADOW_TREE_BUILT, ITEM_TYPE_NUM,
		NUM_AS_ATTR_VAL(default_value), SpecialNs::NS_SVG));
	if (is_built)
	{
		built_flags |= (animated ? 0x1 : 0x2);
	}
	else
	{
		built_flags &= ~(animated ? 0x1 : 0x2);
	}
	elm->SetSpecialAttr(Markup::SVGA_SHADOW_TREE_BUILT, ITEM_TYPE_NUM, NUM_AS_ATTR_VAL(built_flags), FALSE,  SpecialNs::NS_SVG);
}

/* static */ OP_STATUS
SVGUtils::CloneToShadow(SVGDocumentContext* doc_ctx, HTML_Element* tree_to_clone, HTML_Element* parent_to_be, BOOL is_root_node, BOOL animated_tree)
{
	OP_ASSERT(tree_to_clone);
	OP_ASSERT(parent_to_be);
	OP_ASSERT(!IsShadowNode(tree_to_clone));

	HLDocProfile* hld_profile = doc_ctx->GetHLDocProfile();
	if (!hld_profile)
		return OpStatus::ERR;

	HTML_Element* clone = NEW_HTML_Element();
	if (!clone)
		return OpStatus::ERR_NO_MEMORY;

	Markup::Type elm_type;
	if (is_root_node)
	{
		if (animated_tree)
		{
			elm_type = Markup::SVGE_ANIMATED_SHADOWROOT;
		}
		else
		{
			elm_type = Markup::SVGE_BASE_SHADOWROOT;
		}
	}
	else
	{
		elm_type = Markup::SVGE_SHADOW;
	}
	clone->Construct(hld_profile, NS_IDX_SVG, elm_type, NULL /*attr_list*/, HE_INSERTED_BY_SVG);

	// This pointer is made to cause random crashes (unless we somehow manage to always maintain it correctly)
	// FIXME: Put this in a attr_list and send in the Construct call
	OP_ASSERT(!tree_to_clone->IsMatchingType(Markup::SVGE_SHADOW, NS_SVG));
	clone->SetSpecialAttr(Markup::SVGA_REAL_NODE, ITEM_TYPE_NUM, tree_to_clone, FALSE, SpecialNs::NS_SVG);

	clone->Under(parent_to_be);

	// Using *Actual methods to avoid cloning Markup::SVGE_SHADOW nodes
	HTML_Element* child = tree_to_clone->FirstChildActual();
	while (child)
	{
		OP_ASSERT(!IsShadowNode(child));
		CloneToShadow(doc_ctx, child, clone, FALSE, animated_tree);
		child = child->SucActual();
	}

	return OpStatus::OK;
}

OP_STATUS
SVGUtils::LookupAndVerifyUseTarget(SVGElementResolver* resolver, SVGDocumentContext *use_elm_doc_ctx, HTML_Element* use_elm,
								   HTML_Element* traversed_elm, BOOL animated,
								   HTML_Element*& target)
{
	URL *fullurl = NULL;
	const uni_char* url_rel = NULL;

	if (OpStatus::IsSuccess(AttrValueStore::GetXLinkHREF(use_elm_doc_ctx->GetURL(), use_elm, fullurl,
														 animated ? SVG_ATTRFIELD_DEFAULT :
														 SVG_ATTRFIELD_BASE)) && fullurl)
	{
		url_rel = fullurl->UniRelName();
	}
	if (!url_rel)
		return OpStatus::ERR;

	target = use_elm_doc_ctx->GetElementByReference(resolver, use_elm, *fullurl);
	if (!target)
		return OpStatus::ERR;

	// First see if there is some kind of recursion and abort, since we don't want an infinite loop.
	HTML_Element* parent = traversed_elm;
	while (parent)
	{
		if (GetElementToLayout(parent) == target)
		{
			// recursive
			return OpSVGStatus::GENERAL_DOCUMENT_ERROR;
		}
		parent = parent->Parent();
	}

	return OpStatus::OK;
}

OP_STATUS
SVGUtils::CreateShadowRoot(SVGElementResolver* resolver, SVGDocumentContext* doc_ctx, HTML_Element* use_elm,
						   HTML_Element* traversed_elm, BOOL animated)
{
	HTML_Element* target = NULL;
	SVGDocumentContext *use_elm_doc_ctx = AttrValueStore::GetSVGDocumentContext(use_elm);
	if(!use_elm_doc_ctx)
		return OpStatus::ERR_NULL_POINTER;
	OP_STATUS status = LookupAndVerifyUseTarget(resolver, use_elm_doc_ctx, use_elm, traversed_elm,
												animated, target);

	// Recursive reference discovered
	if (status == OpSVGStatus::GENERAL_DOCUMENT_ERROR)
		return status;

	// Failed to find element possibly because the resources haven't finished loading
	if (OpStatus::IsError(status) &&
		use_elm_doc_ctx->GetDocument() && !use_elm_doc_ctx->GetDocument()->IsLoaded())
		return OpSVGStatus::DATA_NOT_LOADED_ERROR;

	// If the lookup fails (meaning no fragment-id or no element found), we return ok.
	// This is normally not an Opera problem but a problem with the SVG document.
	// OP_ASSERT(OpStatus::IsSuccess(status)); // Use tag with href to something non-existent or Use tag without href
	RETURN_VALUE_IF_ERROR(status, OpStatus::OK);

	doc_ctx->RegisterDependency(use_elm, target);
	return CloneToShadow(doc_ctx, target, traversed_elm, TRUE, animated);
}

OP_STATUS SVGUtils::BuildShadowTreeForUseTag(SVGElementResolver* resolver, HTML_Element* traversed_elm, HTML_Element* layouted_elm, SVGDocumentContext* doc_ctx)
{
	HTML_Element* use_elm = layouted_elm;
	OP_ASSERT(use_elm);
	OP_ASSERT(use_elm->IsMatchingType(Markup::SVGE_USE, NS_SVG));
	OP_ASSERT(!HasBuiltNormalShadowTree(traversed_elm));
	const BOOL animated = TRUE;

	OP_STATUS status = CreateShadowRoot(resolver, doc_ctx, use_elm, traversed_elm, animated);
	if(!OpStatus::IsMemoryError(status) && status != OpSVGStatus::DATA_NOT_LOADED_ERROR)
	{
		MarkShadowTreeAsBuilt(traversed_elm, TRUE, animated);
	}
	return status;
}

UINT8 SVGUtils::GetOpacity(HTML_Element* element, const HTMLayoutProperties& props, BOOL is_topmost_svg_root)
{
	if (is_topmost_svg_root)
		/* Root opacity is handled by html layout. FIXME This will effectively disable
		   SMIL animation of 'opacity' on the topmost svg root, if the css value is
		   overridden. */
		return 255; 

	BOOL override_css = FALSE;
	SVGNumberObject* number_value = NULL;

	OpStatus::Ignore(AttrValueStore::GetNumberObject(element, Markup::SVGA_OPACITY, &number_value));
	if (number_value)
	{
		override_css = number_value->Flag(SVGOBJECTFLAG_IS_CSSPROP);
	}

	UINT8 opacity;

	if (override_css)
	{
		SVGNumber alpha_num = number_value->value;
		if (alpha_num > 1)
			alpha_num = 1;
		else if (alpha_num < 0)
			alpha_num = 0;

		opacity = (UINT8)(alpha_num * 255).GetIntegerValue();
	}
	else
	{
		opacity = props.opacity;
	}
	return opacity;
}

/* static */
SVGNumber SVGUtils::GetSpacing(HTML_Element* element, Markup::AttrType attr, const HTMLayoutProperties& props)
{
	BOOL override_css = FALSE;
	SVGNumberObject* number_value;

	if(OpStatus::IsSuccess(AttrValueStore::GetNumberObject(element, attr, &number_value)) && number_value)
	{
		override_css = number_value->Flag(SVGOBJECTFLAG_IS_CSSPROP);
	}

	if(override_css)
	{
		return number_value->value;
	}

	if (attr == Markup::SVGA_LETTER_SPACING)
		return props.letter_spacing;
	else if (attr == Markup::SVGA_WORD_SPACING)
		return LayoutFixedToSVGNumber(props.word_spacing_i);

	OP_ASSERT(0);
	return SVGNumber(0);
}

/* static */ SVGLength::LengthOrientation
SVGUtils::GetLengthOrientation(Markup::AttrType attr_name, NS_Type ns)
{
	SVGLength::LengthOrientation orientation = SVGLength::SVGLENGTH_OTHER;
	if (ns == NS_SVG)
	{
		Markup::AttrType svg_attr_name = attr_name;
		switch(svg_attr_name)
		{
			case Markup::SVGA_WIDTH:
			case Markup::SVGA_X:
			case Markup::SVGA_X1:
			case Markup::SVGA_X2:
			case Markup::SVGA_RX:
			case Markup::SVGA_CX:
			case Markup::SVGA_DX:
			case Markup::SVGA_FX:
				orientation = SVGLength::SVGLENGTH_X;
				break;
			case Markup::SVGA_X_HEIGHT:
			case Markup::SVGA_HEIGHT:
			case Markup::SVGA_Y:
			case Markup::SVGA_Y1:
			case Markup::SVGA_Y2:
			case Markup::SVGA_RY:
			case Markup::SVGA_CY:
			case Markup::SVGA_DY:
			case Markup::SVGA_FY:
//			case Markup::SVGA_FONT_SIZE: // Unsure about this one / davve
				orientation = SVGLength::SVGLENGTH_Y;
				break;
			default:
				break;
		}
	}

	return orientation;
}

/* static */ void
SVGUtils::LimitCanvasSize(SVG_DOCUMENT_CLASS* doc, VisualDevice* vis_dev, int& w, int& h)
{
	// Limit the width and height to twice the width and
	// height of the visual device to avoid allocating HUGE amounts of memory
	int max_width = 2 * vis_dev->GetRenderingViewWidth();
	int max_height = 2 * vis_dev->GetRenderingViewHeight();

	if(doc)
	{
		if (Window* window = doc->GetWindow())
		{
			// Window size is better since the VisDev might be empty before we create the
			// svg.
			window->GetWindowSize(max_width, max_height);
			max_width *= 2;
			max_height *= 2;
		}
	}

	if (max_width < 800)
	{
		max_width = 800;
	}
	if (max_height < 600)
	{
		max_height = 600;
	}
	if (w > max_width)
		w = max_width;
	if (h > max_height)
		h = max_height;
}

BOOL SVGUtils::IsDisplayNone(HTML_Element* layouted_elm, LayoutProperties* layout_props)
{
	if(!layouted_elm->IsMatchingType(Markup::SVGE_SWITCH, NS_SVG))
	{
		const HTMLayoutProperties& props = *layout_props->GetProps();
		short display = props.display_type;

		SVGDisplayType svgdis;
		BOOL override_css;
		if (OpStatus::IsSuccess(AttrValueStore::GetDisplay(layouted_elm, svgdis, &override_css)))
		{
			if (override_css)
			{
				if (svgdis == SVGDISPLAY_NONE)
					display = CSS_VALUE_none;
				else
					display = CSS_VALUE_inline; // just setting anything but none will do fine...
			}
		}
		return display == CSS_VALUE_none;
	}
	return FALSE;
}

/* static */
BOOL SVGUtils::IsAlwaysIrrelevantForDisplay(Markup::Type type)
{
	switch (type)
	{
	case Markup::SVGE_DEFS:
	case Markup::SVGE_FONT:
	case Markup::SVGE_FONT_FACE:
	case Markup::SVGE_HANDLER:
	case Markup::SVGE_SCRIPT:
	case Markup::SVGE_STYLE:
	case Markup::SVGE_DESC:
	case Markup::SVGE_TITLE:
	case Markup::SVGE_CURSOR:
	case Markup::SVGE_LINEARGRADIENT:
	case Markup::SVGE_RADIALGRADIENT:
	case Markup::SVGE_PATTERN:
	case Markup::SVGE_FILTER:
	case Markup::SVGE_MARKER:
	case Markup::SVGE_CLIPPATH:
	case Markup::SVGE_MASK:
	case Markup::SVGE_SET:
	case Markup::SVGE_DISCARD:
	case Markup::SVGE_ANIMATE:
	case Markup::SVGE_ANIMATEMOTION:
	case Markup::SVGE_ANIMATECOLOR:
	case Markup::SVGE_ANIMATETRANSFORM:
		return TRUE;
	}
	return FALSE; // Might affect display somehow
}

/* static */ BOOL
SVGUtils::IsLoadingExternalFontResources(HTML_Element* elm)
{
	OP_NEW_DBG("IsLoadingExternalFontResources", "svg_font_loading");

	OP_ASSERT(elm->GetNsType() == NS_SVG);
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx)
		return FALSE;

	URL* url = NULL;
	Markup::Type elm_type = elm->Type();
	if (IsFontElementContainer(elm_type))
	{
		//PrintElmType(elm);
		HTML_Element* child = elm->FirstChildActual();
		while (child)
		{
			NS_Type ns = child->GetNsType();
			if (ns == NS_SVG)
			{
				Markup::Type child_type = child->Type();
				if (IsFontElementContainer(child_type))
				{
					if (IsLoadingExternalFontResources(child))
					{
						return TRUE;
					}
				}
				else
				{
					if (CanHaveExternalReference(child_type) &&
						OpStatus::IsSuccess(AttrValueStore::GetXLinkHREF(doc_ctx->GetURL(), child, url)) && url)
					{
						URLStatus url_stat = (URLStatus)url->GetAttribute(URL::KLoadStatus, TRUE);
						if (url_stat == URL_LOADING || url_stat == URL_LOADING_WAITING || url_stat == URL_LOADING_FROM_CACHE)
						{
							return TRUE;
						}
					}
				}
			}

			child = child->SucActual();
		}
	}
	else if (CanHaveExternalReference(elm_type))
	{
		if (OpStatus::IsSuccess(AttrValueStore::GetXLinkHREF(doc_ctx->GetURL(), elm, url)) && url)
		{
			URLStatus url_stat = (URLStatus)url->GetAttribute(URL::KLoadStatus, TRUE);
			if (url_stat == URL_LOADING || url_stat == URL_LOADING_WAITING || url_stat == URL_LOADING_FROM_CACHE)
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/* static */ BOOL
SVGUtils::HasLoadedRequiredExternalResources(HTML_Element* elm)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx)
		return FALSE;

	URL* url = NULL;
	BOOL ext_required = !!AttrValueStore::GetEnumValue(elm, Markup::SVGA_EXTERNALRESOURCESREQUIRED,
													   SVGENUM_BOOLEAN, FALSE);

	if (IsContainerElement(elm))
	{
		HTML_Element* child = elm->FirstChildActual();
		while (child)
		{
			if (IsContainerElement(child) && !child->IsMatchingType(Markup::SVGE_DEFS, NS_SVG))
			{
				if (!HasLoadedRequiredExternalResources(child))
					return FALSE;
			}
			else
			{
				BOOL child_ext_required = !!AttrValueStore::GetEnumValue(elm, Markup::SVGA_EXTERNALRESOURCESREQUIRED, SVGENUM_BOOLEAN, FALSE);
				if (child->GetNsType() == NS_SVG &&
					CanHaveExternalReference(child->Type()) &&
					(ext_required || child_ext_required) &&
					OpStatus::IsSuccess(AttrValueStore::GetXLinkHREF(doc_ctx->GetURL(), child, url)) && url)
				{
					URLStatus url_stat = (URLStatus)url->GetAttribute(URL::KLoadStatus, TRUE);
					if (url_stat != URL_LOADED)
					{
						return FALSE;
					}
				}
			}

			child = child->SucActual();
		}
	}
	else if (CanHaveExternalReference(elm->Type()))
	{
		if (ext_required && OpStatus::IsSuccess(AttrValueStore::GetXLinkHREF(doc_ctx->GetURL(), elm, url)) && url)
		{
			URLStatus url_stat = (URLStatus)url->GetAttribute(URL::KLoadStatus, TRUE);
			if (url_stat != URL_LOADED)
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

/* static */ OP_STATUS
SVGUtils::BuildSVGFontInfo(SVGDocumentContext* doc_ctx, HTML_Element* element)
{
	OpAutoPtr<OpFontInfo> fi(OP_NEW(OpFontInfo, ()));
	if (!fi.get())
		return OpStatus::ERR_NO_MEMORY;

	OP_ASSERT(element->IsMatchingType(Markup::SVGE_FONT, NS_SVG) || element->IsMatchingType(Markup::SVGE_FONT_FACE, NS_SVG));

	// Create a font context so that we can clean up
	SVGFontElement* font_elm_ctx = AttrValueStore::GetSVGFontElement(element);
	if (!font_elm_ctx)
		return OpStatus::ERR_NO_MEMORY; // This indicates OOM

	SVGFontTraversalObject font_traversal_object(doc_ctx, fi.get());
	RETURN_IF_ERROR(font_traversal_object.CreateResolver());

	RETURN_IF_ERROR(SVGSimpleTraverser::TraverseElement(&font_traversal_object, element));

	if (!font_traversal_object.HasGlyphs())
		return OpStatus::ERR;

	fi->SetFontType(OpFontInfo::SVG_WEBFONT);

	RETURN_IF_ERROR(doc_ctx->AddSVGFont(element, fi.get()));

	fi.release();

	return OpStatus::OK;
}

/* static */ SVGNumber
SVGUtils::ResolveLengthWithUnits(const SVGLengthObject* len,
								 SVGLength::LengthOrientation orient,
								 SVGUnitsType units, const SVGValueContext& vcxt)
{
	OP_ASSERT(len);

	if (len->GetUnit() == CSS_PERCENTAGE && units == SVGUNITS_OBJECTBBOX)
		return len->GetScalar() / 100;

	return ResolveLength(len->GetLength(), orient, vcxt);
}

/* static */ BOOL
SVGUtils::GetResolvedLengthWithUnits(HTML_Element* elm, Markup::AttrType attr,
									 SVGLength::LengthOrientation orient,
									 SVGUnitsType units, const SVGValueContext& vcxt,
									 SVGNumber& v)
{
	SVGLengthObject* len = NULL;
	AttrValueStore::GetLength(elm, attr, &len);
	if (!len)
		return FALSE;

	v = ResolveLengthWithUnits(len, orient, units, vcxt);
	return TRUE;
}

/* static */ BOOL
SVGUtils::IsURLEqual(URL &url1, URL &url2)
{
	return url1 == url2 &&
		((!url1.RelName() && !url2.RelName()) ||
		 (url1.RelName() && url2.RelName() &&
		  op_strcmp(url1.RelName(), url2.RelName()) == 0));
}

/* static */ void
SVGUtils::LoadExternalReferences(SVGDocumentContext* doc_ctx, HTML_Element *element)
{
	OP_ASSERT(doc_ctx);
	OP_ASSERT(element->GetNsType() == NS_SVG);

	Markup::Type type = element->Type();
	if (!CanHaveExternalReference(type) || type == Markup::SVGE_SCRIPT)
		return;

	URL doc_url = doc_ctx->GetURL();
	URL redirected_to = doc_url.GetAttribute(URL::KMovedToURL,TRUE);
	if (!redirected_to.IsEmpty())
	{
		doc_url = redirected_to;
	}

	URL* url = NULL;
	if (OpStatus::IsError(AttrValueStore::GetXLinkHREF(doc_url, element, url)) || !url)
		return;

	redirected_to = url->GetAttribute(URL::KMovedToURL, TRUE);
	if (!redirected_to.IsEmpty())
		url = &redirected_to;

	if (!(*url == doc_url))
	{
#ifdef SVG_TRUST_SECURE_FLAG
		if(doc_ctx->GetSVGImage()->IsSecure() && !g_secman_instance->OriginCheck(url, &doc_url))
		{
			doc_ctx->GetSVGImage()->SetSecure(FALSE);
		}
#endif // SVG_TRUST_SECURE_FLAG

		if(type == Markup::SVGE_IMAGE || type == Markup::SVGE_FEIMAGE)
		{
			SVG_DOCUMENT_CLASS* document = doc_ctx->GetDocument();
			if (document->GetLoadImages())
			{
				if(LoadInlineStatus::USE_LOADED == document->LoadInline(url, element, IMAGE_INLINE))
				{
					element->GetHEListElm(FALSE)->UpdateImageUrlIfNeeded(url);
				}
			}
		}
		else
		{
			SVGDocumentContext::SVGFrameElementContext fedata;
			OP_STATUS err = doc_ctx->GetExternalFrameElement(*url, element, &fedata);
			if(OpStatus::IsSuccess(err))
			{
				LoadExternalDocument(*url, fedata.frm_doc, fedata.frame_element);
				if (element != fedata.frame_element)
					doc_ctx->RegisterDependency(element, fedata.frame_element);
			}
		}
	}
}

static inline BOOL SVGURLRefIsValid(const SVGURLReference& reference)
{
	return !(reference.info.is_none || reference.info.url_str_len == 0 || !reference.url_str);
}

/* static */ void
SVGUtils::LoadExternalReferencesFromCascade(SVGDocumentContext* doc_ctx, HTML_Element *element, LayoutProperties* cascade)
{
	OP_ASSERT(cascade);
	OP_ASSERT(doc_ctx);
	OP_ASSERT(element && element->GetNsType() == NS_SVG);

	const HTMLayoutProperties& props = *cascade->GetProps();
	const SvgProperties *svg_props = props.svg;

	if (!svg_props)
		return;

	if (svg_props->HasFill() || svg_props->HasStroke())
	{
		const SVGPaint *fill_paint = svg_props->GetFill();
		const SVGPaint* stroke_paint = svg_props->GetStroke();

		// avoid doing url resolving if we can
		if (fill_paint->GetPaintType() > SVGPaint::CURRENT_COLOR ||
			stroke_paint->GetPaintType() > SVGPaint::CURRENT_COLOR)
		{
			URL doc_url = doc_ctx->GetURL();
			URL redirected_to = doc_url.GetAttribute(URL::KMovedToURL,TRUE);
			if (!redirected_to.IsEmpty())
			{
				doc_url = redirected_to;
			}

			URL base_url = ResolveBaseURL(doc_url, element);

			if (fill_paint)
				LoadExternalPaintServers(doc_ctx, doc_url, base_url, element, fill_paint);

			if (stroke_paint)
				LoadExternalPaintServers(doc_ctx, doc_url, base_url, element, stroke_paint);
		}
	}

	// Put valid references in the array below, and then load
	// associated resources.
	// The size of the array should match the number of
	// SVGURLReference members in SvgProperties.
	unsigned urlref_count = 0;
	const SVGURLReference* urlrefs[7]; /* ARRAY OK 2010-05-05 fs */

#define ADD_SVGURLREF_IF_VALID(ref)				\
	if (SVGURLRefIsValid(ref))					\
		urlrefs[urlref_count++] = &(ref)

	ADD_SVGURLREF_IF_VALID(svg_props->mask);
	ADD_SVGURLREF_IF_VALID(svg_props->filter);
	ADD_SVGURLREF_IF_VALID(svg_props->clippath);
	ADD_SVGURLREF_IF_VALID(svg_props->marker);
	ADD_SVGURLREF_IF_VALID(svg_props->markerstart);
	ADD_SVGURLREF_IF_VALID(svg_props->markermid);
	ADD_SVGURLREF_IF_VALID(svg_props->markerend);

#undef ADD_SVGURLREF_IF_VALID

	if (urlref_count != 0)
	{
		URL doc_url = doc_ctx->GetURL();
		URL redirected_to = doc_url.GetAttribute(URL::KMovedToURL,TRUE);
		if (!redirected_to.IsEmpty())
		{
			doc_url = redirected_to;
		}

		URL base_url = ResolveBaseURL(doc_url, element);

		for (unsigned i = 0; i < urlref_count; ++i)
		{
			const SVGURLReference* reference = urlrefs[i];

			OP_ASSERT(SVGURLRefIsValid(*reference));

			OpString url_str;
			// FIXME: OOM
			OpStatus::Ignore(url_str.Set(reference->url_str, reference->info.url_str_len));

			LoadExternalResource(doc_ctx, doc_url, base_url, element, url_str.CStr());
		}
	}
}

/* static */ void
SVGUtils::LoadExternalResource(SVGDocumentContext *doc_ctx, const URL& doc_url, URL& base_url, HTML_Element *element, const uni_char* resource_uri_str)
{
	URL resource_url = g_url_api->GetURL(base_url, resource_uri_str);
	URL moved_to = resource_url.GetAttribute(URL::KMovedToURL, TRUE);
	if (!moved_to.IsEmpty())
		resource_url = moved_to;

	if (base_url.Type() != URL_DATA && !(resource_url == doc_url))
	{
		SVGDocumentContext::SVGFrameElementContext fedata;
		OP_STATUS err = doc_ctx->GetExternalFrameElement(resource_url, element, &fedata);
		if(OpStatus::IsSuccess(err))
		{
			LoadExternalDocument(resource_url, fedata.frm_doc, fedata.frame_element);
			doc_ctx->RegisterDependency(element, fedata.frame_element);
		}
	}
}

/*
 * This function will try to load resources for all elements where a
 * paint property is active:
 *
 * <g style="fill: url("http://extern");">
 *   <g>
 *    <g>
 *     <...>
 *    </g>
 *   </g>
 * </g>
 *
 * The fill will be active on all those elements and because of this,
 * we try to start loading of the 'http://extern' at all
 * elements. This cause no problems, with the exception that we lookup
 * the proxy element each time and check that the proxy element has
 * started loading each time.
 *
 */
/* static */ void
SVGUtils::LoadExternalPaintServers(SVGDocumentContext *doc_ctx, const URL& doc_url, URL& base_url, HTML_Element *element, const SVGPaint *paint)
{
	OP_ASSERT(paint);

	switch (paint->GetPaintType())
	{
	case SVGPaint::URI_NONE:
	case SVGPaint::URI_RGBCOLOR:
	case SVGPaint::URI_RGBCOLOR_ICCCOLOR:
	case SVGPaint::URI_CURRENT_COLOR:
	case SVGPaint::URI:
	{
		const uni_char* paint_uri_str = paint->GetURI();

		LoadExternalResource(doc_ctx, doc_url, base_url, element, paint_uri_str);
	}
	default:
		break;
	}
}

/* static */ void
SVGUtils::LoadExternalDocument(URL &url, FramesDocument *frm_doc, HTML_Element* frame_element)
{
#ifdef SVG_TRUST_SECURE_FLAG
	URL doc_url = doc_ctx->GetURL();
	if(doc_ctx->GetSVGImage()->IsSecure() && !g_secman_instance->OriginCheck(&url, &doc_url))
	{
		doc_ctx->GetSVGImage()->SetSecure(FALSE);
	}
#endif // SVG_TRUST_SECURE_FLAG

	FramesDocElm* frame = FramesDocElm::GetFrmDocElmByHTML(frame_element);
	if (frame == NULL)
	{
		const BOOL load_frame = TRUE;

		// We should always get something here if it's an external resource.
		// Note that if we use the other doc_ctx for an external resource then we will crash in dochand instead.

		// FIXME: Check that we use the right frm_doc below!
		OP_STATUS status = frm_doc->GetNewIFrame(frame, 1, 1, frame_element, NULL, load_frame, FALSE);
		if (OpStatus::IsError(status))
		{
			frame = NULL;
		}
		OP_ASSERT(OpStatus::IsSuccess(status));

		if (frame != NULL)
		{
			frame->SetAsSVGResourceDocument();

			if (VisualDevice* frame_vd = frame->GetVisualDevice())
				if (CoreView* frame_view = frame_vd->GetContainerView())
					frame_view->SetReference(frm_doc, frame_element);
		}
	}
	else
	{
		DocumentManager* docman = frame->GetDocManager();
		if (docman)
		{
			docman->SetReplace(TRUE); // we don't want a history entry for this

			if (!(url == docman->GetCurrentURL()))
			{
				docman->OpenURL(url, frm_doc, TRUE, FALSE);
			}
			else
			{
				docman->Refresh(url.Id(TRUE));
			}
		}
	}
}

/* static */ URL
SVGUtils::ResolveBaseURL(const URL& root_url, HTML_Element* elm)
{
	if (!elm)
	{
		return root_url;
	}

	URL base_url = ResolveBaseURL(root_url, elm->Parent());
	const uni_char* base_attr = elm->GetStringAttr(XMLA_BASE, NS_IDX_XML);
	if (base_attr)
	{
		// Must not look at where it's redirected. Only interested in
		// the textual contents of the attribute.
		return g_url_api->GetURL(base_url, base_attr);
	}
	return base_url;
}

#if defined(SVG_SUPPORT_EDITABLE) || defined(SVG_TREAT_WHITESPACE_AS_EMPTY_XLINK_HREF)
/* static */
BOOL SVGUtils::IsAllWhitespace(const uni_char* s, unsigned int s_len)
{
	while (s_len)
	{
		if (*s != ' ' && *s != '\n' && *s != '\r' && *s != '\t')
			return FALSE;

		s++;
		s_len--;
	}
	return TRUE;
}
#endif // SVG_SUPPORT_EDITABLE || SVG_TREAT_WHITESPACE_AS_EMPTY_XLINK_HREF


#endif // SVG_SUPPORT
