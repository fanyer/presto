/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_DOM
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/domsvg/domsvgenum.h"
#include "modules/dom/src/domsvg/domsvgelement.h"
#include "modules/dom/src/domsvg/domsvglocation.h"
#include "modules/dom/src/domsvg/domsvglist.h"
#include "modules/dom/src/domsvg/domsvgobjectstore.h"
#include "modules/dom/src/domsvg/domsvgelementinstance.h"
#include "modules/dom/src/domcss/cssstyledeclaration.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domcore/domstaticnodelist.h"
#include "modules/dom/src/js/window.h"
#ifdef USER_JAVASCRIPT
#include "modules/dom/src/userjs/userjsevent.h"
#endif // USER_JAVASCRIPT

#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/spatial_navigation/sn_handler.h"
#include "modules/spatial_navigation/navigation_api.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/logdoc/xmlenum.h"

#include "modules/url/url2.h"

#include "modules/svg/SVGManager.h"
#include "modules/svg/svg_image.h"
#include "modules/svg/svg.h"

#ifdef DOM_NO_COMPLEX_GLOBALS
# define CONST_SIMPLE_ARRAY(name, type) void DOM_##name##_Init (DOM_GlobalData *global_data) { type *local = global_data->name; int i=0;
# define ELEMENT_SPEC_ENTRY(a, b, c, d) local[i].type = a, local[i].svgelm_interface = b, local[i].classname = c, local[i++].prototype = d
# define CONST_SIMPLE_END(name) ; };
#else
# define CONST_SIMPLE_ARRAY(name, type) const type g_DOM_##name[] = {
# define ELEMENT_SPEC_ENTRY(a, b, c, d) { a, b, c, d }
# define CONST_SIMPLE_END(name) };
#endif // !_NO_GLOBALS_

// Note: if you change the number of elements in the table below you must also update DOM_SVG_ELEMENT_COUNT.
CONST_SIMPLE_ARRAY(svg_element_spec, SVGElementSpec)
	ELEMENT_SPEC_ENTRY(Markup::SVGE_ANIMATE, DOM_SVGElementInterface::SVG_INTERFACE_ANIMATION_ELEMENT_BASE, "SVGAnimateElement", DOM_Runtime::SVG_ANIMATE_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_ANIMATECOLOR, DOM_SVGElementInterface::SVG_INTERFACE_ANIMATION_ELEMENT_BASE, "SVGAnimateColorElement", DOM_Runtime::SVG_ANIMATE_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_ANIMATETRANSFORM, DOM_SVGElementInterface::SVG_INTERFACE_ANIMATION_ELEMENT_BASE, "SVGAnimateTransformElement", DOM_Runtime::SVG_ANIMATE_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_SET, DOM_SVGElementInterface::SVG_INTERFACE_ANIMATION_ELEMENT_BASE, "SVGSetElement", DOM_Runtime::SVG_ANIMATE_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_ANIMATEMOTION, DOM_SVGElementInterface::SVG_INTERFACE_ANIMATION_ELEMENT_BASE, "SVGAnimateMotionElement", DOM_Runtime::SVG_ANIMATE_PROTOTYPE),

	ELEMENT_SPEC_ENTRY(Markup::SVGE_CIRCLE, DOM_SVGElementInterface::SVG_INTERFACE_CIRCLE_ELEMENT, "SVGCircleElement", DOM_Runtime::SVG_CIRCLE_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_DEFS, DOM_SVGElementInterface::SVG_INTERFACE_DEFS_ELEMENT, "SVGDefsElement", DOM_Runtime::SVG_DEFS_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_DESC, DOM_SVGElementInterface::SVG_INTERFACE_DESC_ELEMENT, "SVGDescElement", DOM_Runtime::SVG_DESC_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_ELLIPSE, DOM_SVGElementInterface::SVG_INTERFACE_ELLIPSE_ELEMENT, "SVGEllipseElement", DOM_Runtime::SVG_ELLIPSE_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_G, DOM_SVGElementInterface::SVG_INTERFACE_G_ELEMENT, "SVGGElement", DOM_Runtime::SVG_G_PROTOTYPE),

	ELEMENT_SPEC_ENTRY(Markup::SVGE_IMAGE, DOM_SVGElementInterface::SVG_INTERFACE_IMAGE_ELEMENT, "SVGImageElement", DOM_Runtime::SVG_IMAGE_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_LINE, DOM_SVGElementInterface::SVG_INTERFACE_LINE_ELEMENT, "SVGLineElement", DOM_Runtime::SVG_LINE_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_LINEARGRADIENT, DOM_SVGElementInterface::SVG_INTERFACE_LINEARGRADIENT_ELEMENT, "SVGLinearGradientElement",	DOM_Runtime::SVG_LINEARGRADIENT_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_PATH, DOM_SVGElementInterface::SVG_INTERFACE_PATH_ELEMENT, "SVGPathElement", DOM_Runtime::SVG_PATH_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_POLYGON, DOM_SVGElementInterface::SVG_INTERFACE_POLYGON_ELEMENT, "SVGPolygonElement", DOM_Runtime::SVG_POLYGON_PROTOTYPE),

	ELEMENT_SPEC_ENTRY(Markup::SVGE_POLYLINE, DOM_SVGElementInterface::SVG_INTERFACE_POLYLINE_ELEMENT, "SVGPolylineElement", DOM_Runtime::SVG_POLYLINE_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_RADIALGRADIENT, DOM_SVGElementInterface::SVG_INTERFACE_RADIALGRADIENT_ELEMENT, "SVGRadialGradientElement",	DOM_Runtime::SVG_RADIALGRADIENT_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_RECT, DOM_SVGElementInterface::SVG_INTERFACE_RECT_ELEMENT, "SVGRectElement", DOM_Runtime::SVG_RECT_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_STOP, DOM_SVGElementInterface::SVG_INTERFACE_STOP_ELEMENT, "SVGStopElement", DOM_Runtime::SVG_STOP_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_SYMBOL, DOM_SVGElementInterface::SVG_INTERFACE_SYMBOL_ELEMENT, "SVGSymbolElement", DOM_Runtime::SVG_SYMBOL_PROTOTYPE),

	ELEMENT_SPEC_ENTRY(Markup::SVGE_SWITCH, DOM_SVGElementInterface::SVG_INTERFACE_SWITCH_ELEMENT, "SVGSwitchElement", DOM_Runtime::SVG_SWITCH_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_TEXT, DOM_SVGElementInterface::SVG_INTERFACE_TEXT_ELEMENT, "SVGTextElement", DOM_Runtime::SVG_TEXT_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::HTE_TEXT, DOM_SVGElementInterface::SVG_INTERFACE_TEXTCONTENT_ELEMENT, "SVGTextContentElement", DOM_Runtime::SVG_TEXTCONTENT_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_TITLE, DOM_SVGElementInterface::SVG_INTERFACE_TITLE_ELEMENT, "SVGTitleElement", DOM_Runtime::SVG_TITLE_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_TSPAN, DOM_SVGElementInterface::SVG_INTERFACE_TSPAN_ELEMENT, "SVGTSpanElement", DOM_Runtime::SVG_TSPAN_PROTOTYPE),

	ELEMENT_SPEC_ENTRY(Markup::SVGE_TREF, DOM_SVGElementInterface::SVG_INTERFACE_TREF_ELEMENT, "SVGTRefElement", DOM_Runtime::SVG_TREF_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_USE, DOM_SVGElementInterface::SVG_INTERFACE_USE_ELEMENT, "SVGUseElement", DOM_Runtime::SVG_USE_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_A, DOM_SVGElementInterface::SVG_INTERFACE_A_ELEMENT, "SVGAElement", DOM_Runtime::SVG_A_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_SCRIPT, DOM_SVGElementInterface::SVG_INTERFACE_SCRIPT_ELEMENT, "SVGScriptElement", DOM_Runtime::SVG_SCRIPT_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_MPATH, DOM_SVGElementInterface::SVG_INTERFACE_MPATH_ELEMENT, "SVGMPathElement", DOM_Runtime::SVG_MPATH_PROTOTYPE),

	ELEMENT_SPEC_ENTRY(Markup::SVGE_FONT, DOM_SVGElementInterface::SVG_INTERFACE_FONT_ELEMENT, "SVGFontElement", DOM_Runtime::SVG_FONT_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_GLYPH, DOM_SVGElementInterface::SVG_INTERFACE_GLYPH_ELEMENT, "SVGGlyphElement", DOM_Runtime::SVG_GLYPH_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_MISSING_GLYPH, DOM_SVGElementInterface::SVG_INTERFACE_GLYPH_ELEMENT, "SVGMissingGlyphElement", DOM_Runtime::SVG_MISSINGGLYPH_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FOREIGNOBJECT, DOM_SVGElementInterface::SVG_INTERFACE_FOREIGNOBJECT_ELEMENT, "SVGForeignObjectElement", DOM_Runtime::SVG_FOREIGN_OBJECT_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_SVG, DOM_SVGElementInterface::SVG_INTERFACE_SVG_ELEMENT, "SVGSVGElement", DOM_Runtime::SVG_SVG_PROTOTYPE),
	// 35 elements in this ifdef section
#ifdef SVG_FULL_11
	ELEMENT_SPEC_ENTRY(Markup::SVGE_CLIPPATH, DOM_SVGElementInterface::SVG_INTERFACE_CLIPPATH_ELEMENT, "SVGClipPathElement",	DOM_Runtime::SVG_CLIPPATH_PROTOTYPE),
	// Filter primitives
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FEBLEND, DOM_SVGElementInterface::SVG_INTERFACE_FEBLEND_ELEMENT, "SVGFEBlendElement", DOM_Runtime::SVG_FEBLEND_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FECOLORMATRIX, DOM_SVGElementInterface::SVG_INTERFACE_FECOLORMATRIX_ELEMENT, "SVGFEColorMatrixElement", DOM_Runtime::SVG_FECOLORMATRIX_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FECOMPONENTTRANSFER, DOM_SVGElementInterface::SVG_INTERFACE_FECOMPONENTTRANSFER_ELEMENT, "SVGFEComponentTransferElement", DOM_Runtime::SVG_FECOMPONENTTRANSFER_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FEFUNCR, DOM_SVGElementInterface::SVG_INTERFACE_COMPONENT_TRANSFER_FUNCTION_ELEMENT, "SVGFEFuncRElement", DOM_Runtime::SVG_FEFUNCR_PROTOTYPE),

	ELEMENT_SPEC_ENTRY(Markup::SVGE_FEFUNCG, DOM_SVGElementInterface::SVG_INTERFACE_COMPONENT_TRANSFER_FUNCTION_ELEMENT, "SVGFEFuncGElement", DOM_Runtime::SVG_FEFUNCG_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FEFUNCB, DOM_SVGElementInterface::SVG_INTERFACE_COMPONENT_TRANSFER_FUNCTION_ELEMENT, "SVGFEFuncBElement", DOM_Runtime::SVG_FEFUNCB_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FEFUNCA, DOM_SVGElementInterface::SVG_INTERFACE_COMPONENT_TRANSFER_FUNCTION_ELEMENT, "SVGFEFuncAElement", DOM_Runtime::SVG_FEFUNCA_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FECOMPOSITE, DOM_SVGElementInterface::SVG_INTERFACE_FECOMPOSITE_ELEMENT, "SVGFECompositeElement", DOM_Runtime::SVG_FECOMPOSITE_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FECONVOLVEMATRIX, DOM_SVGElementInterface::SVG_INTERFACE_FECONVOLVEMATRIX_ELEMENT, "SVGFEConvolveMatrixElement",	DOM_Runtime::SVG_FECONVOLVEMATRIX_PROTOTYPE),

	ELEMENT_SPEC_ENTRY(Markup::SVGE_FEDIFFUSELIGHTING,	DOM_SVGElementInterface::SVG_INTERFACE_FEDIFFUSELIGHTING_ELEMENT, "SVGFEDiffuseLightingElement", DOM_Runtime::SVG_FEDIFFUSELIGHTNING_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FEDISTANTLIGHT, DOM_SVGElementInterface::SVG_INTERFACE_FEDISTANTLIGHT_ELEMENT, "SVGFEDistantLightElement",	DOM_Runtime::SVG_FEDISTANTLIGHT_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FEPOINTLIGHT, DOM_SVGElementInterface::SVG_INTERFACE_FEPOINTLIGHT_ELEMENT, "SVGFEPointLightElement", DOM_Runtime::SVG_FEPOINTLIGHT_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FESPOTLIGHT, DOM_SVGElementInterface::SVG_INTERFACE_FESPOTLIGHT_ELEMENT, "SVGFESpotLightElement", DOM_Runtime::SVG_FESPOTLIGHT_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FEDISPLACEMENTMAP,	DOM_SVGElementInterface::SVG_INTERFACE_FEDISPLACEMENTMAP_ELEMENT, "SVGFEDisplacementMapElement", DOM_Runtime::SVG_FEDISPLACEMENTMAP_PROTOTYPE),

	ELEMENT_SPEC_ENTRY(Markup::SVGE_FEFLOOD, DOM_SVGElementInterface::SVG_INTERFACE_FEFLOOD_ELEMENT, "SVGFEFloodElement", DOM_Runtime::SVG_FEFLOOD_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FEGAUSSIANBLUR, DOM_SVGElementInterface::SVG_INTERFACE_FEGAUSSIANBLUR_ELEMENT, "SVGFEGaussianBlurElement",	DOM_Runtime::SVG_FEGAUSSIANBLUR_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FEIMAGE, DOM_SVGElementInterface::SVG_INTERFACE_FEIMAGE_ELEMENT, "SVGFEImageElement", DOM_Runtime::SVG_FEIMAGE_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FEMERGE, DOM_SVGElementInterface::SVG_INTERFACE_FEMERGE_ELEMENT, "SVGFEMergeElement", DOM_Runtime::SVG_FEMERGE_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FEMERGENODE, DOM_SVGElementInterface::SVG_INTERFACE_FEMERGENODE_ELEMENT, "SVGFEMergeNodeElement", DOM_Runtime::SVG_FEMERGENODE_PROTOTYPE),

	ELEMENT_SPEC_ENTRY(Markup::SVGE_FEMORPHOLOGY, DOM_SVGElementInterface::SVG_INTERFACE_FEMORPHOLOGY_ELEMENT, "SVGFEMorphologyElement", DOM_Runtime::SVG_FEMORPHOLOGY_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FEOFFSET, DOM_SVGElementInterface::SVG_INTERFACE_FEOFFSET_ELEMENT, "SVGFEOffsetElement", DOM_Runtime::SVG_FEOFFSET_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FESPECULARLIGHTING, DOM_SVGElementInterface::SVG_INTERFACE_FESPECULARLIGHTING_ELEMENT, "SVGFESpecularLightingElement",	DOM_Runtime::SVG_FESPECULARLIGHTNING_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FETILE, DOM_SVGElementInterface::SVG_INTERFACE_FETILE_ELEMENT, "SVGFETileElement",	DOM_Runtime::SVG_FETILE_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_FETURBULENCE, DOM_SVGElementInterface::SVG_INTERFACE_FETURBULENCE_ELEMENT, "SVGFETurbulenceElement", DOM_Runtime::SVG_FETURBULENCE_PROTOTYPE),

	ELEMENT_SPEC_ENTRY(Markup::SVGE_FILTER, DOM_SVGElementInterface::SVG_INTERFACE_FILTER_ELEMENT, "SVGFilterElement",	DOM_Runtime::SVG_FILTER_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_MARKER, DOM_SVGElementInterface::SVG_INTERFACE_MARKER_ELEMENT, "SVGMarkerElement", DOM_Runtime::SVG_MARKER_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_MASK, DOM_SVGElementInterface::SVG_INTERFACE_MASK_ELEMENT, "SVGMaskElement", DOM_Runtime::SVG_MASK_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_PATTERN, DOM_SVGElementInterface::SVG_INTERFACE_PATTERN_ELEMENT, "SVGPatternElement", DOM_Runtime::SVG_PATTERN_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_STYLE, DOM_SVGElementInterface::SVG_INTERFACE_STYLE_ELEMENT, "SVGStyleElement", DOM_Runtime::SVG_STYLE_PROTOTYPE),

	ELEMENT_SPEC_ENTRY(Markup::SVGE_TEXTPATH, DOM_SVGElementInterface::SVG_INTERFACE_TEXTPATH_ELEMENT, "SVGTextPathElement", DOM_Runtime::SVG_TEXTPATH_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_VIEW, DOM_SVGElementInterface::SVG_INTERFACE_VIEW_ELEMENT, "SVGViewElement", DOM_Runtime::SVG_VIEW_PROTOTYPE),
	// 32 elements in this ifdef section
#endif // SVG_FULL_11
#ifdef SVG_TINY_12
	ELEMENT_SPEC_ENTRY(Markup::SVGE_AUDIO, DOM_SVGElementInterface::SVG_INTERFACE_AUDIO_ELEMENT, "SVGAudioElement", DOM_Runtime::SVG_AUDIO_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_VIDEO, DOM_SVGElementInterface::SVG_INTERFACE_VISUAL_MEDIA_ELEMENT, "SVGVideoElement", DOM_Runtime::SVG_VIDEO_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_ANIMATION, DOM_SVGElementInterface::SVG_INTERFACE_ANIMATION_ELEMENT, "SVGAnimationElement", DOM_Runtime::SVG_ANIMATION_PROTOTYPE),
	ELEMENT_SPEC_ENTRY(Markup::SVGE_TEXTAREA, DOM_SVGElementInterface::SVG_INTERFACE_TEXTAREA_ELEMENT, "SVGTextAreaElement", DOM_Runtime::SVG_TEXTAREA_PROTOTYPE),
	// 4 elements in this ifdef section
#endif // SVG_TINY_12
	ELEMENT_SPEC_ENTRY(Markup::HTE_ANY, DOM_SVGElementInterface::SVG_INTERFACE_ELEMENT, /* Sentinel */ "SVGElement", DOM_Runtime::SVG_PROTOTYPE)
CONST_SIMPLE_END(svg_element_spec)


#ifdef DOM_NO_COMPLEX_GLOBALS
# define DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_START() void DOM_svgElementPrototypeClassNames_Init(DOM_GlobalData *global_data) { const char **classnames = global_data->svgElementPrototypeClassNames;
# define DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM(name) *classnames = "SVG" name "ElementPrototype"; ++classnames;
# define DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM_LAST() *classnames = "";
# define DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_END() }
#else // DOM_NO_COMPLEX_GLOBALS
# define DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_START() const char *const g_DOM_svgElementPrototypeClassNames[] = {
# define DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM(name) "SVG" name "ElementPrototype",
# define DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM_LAST() ""
# define DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_END() };
#endif // DOM_NO_COMPLEX_GLOBALS

/* Important: this array must be kept in sync with the enumeration
   DOM_Runtime::SVGElementPrototype.  See its definition for what
   else (if anything) it needs to be in sync with. */
DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_START()
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("") // SVGElementPrototype
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Circle")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Ellipse")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Line")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Path")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Polyline")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Polygon")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Rect")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("SVG")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Text")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("TextContent")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("TSpan")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("TRef")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Animate")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("G")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("A")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Script")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("MPath")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Font")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Glyph")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("MissingGlyph")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("ForeignObject")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Defs")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Desc")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Title")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Symbol")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Use")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Image")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Switch")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("LinearGradient")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("RadialGradient")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Stop")
#ifdef SVG_FULL_11
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Pattern")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("TextPath")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Marker")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("View")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Mask")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEFlood")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEBlend")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEColorMatrix")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEComponentTransfer")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEComposite")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEConvolveMatrix")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEDiffuseLightning")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEDisplacementMap")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEImage")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEMerge")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEMorphology")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEOffset")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FESpecularLightning")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FETile")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FETurbulence")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Style")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("ClipPath")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEFuncR")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEFuncG")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEFuncB")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEFuncA")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEDistantLight")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FESpotLight")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEPointLight")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEMergeNode")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("FEGaussianBlur")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Filter")
#endif // SVG_FULL_11
#ifdef SVG_TINY_12
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Audio")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Video")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("Animation")
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM("TextArea")
#endif // SVG_TINY_12
	DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM_LAST()
DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_END()

#undef DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_START
#undef DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM
#undef DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_ITEM_LAST
#undef DOM_SVG_ELEMENT_PROTOTYPE_CLASSNAMES_END

DOM_SVGElementInterface::DOM_SVGElementInterface(unsigned int type)
: DOM_SVGInterfaceSpec(type, g_DOM_svg_if_inheritance_table)
{
}

/* static */ void
DOM_SVGElement::PutConstructorsL(DOM_Object* target)
{
	int i = 0;
	while(TRUE)
	{
		if (g_DOM_svg_element_spec[i].type == Markup::HTE_ANY /* Sentinel */)
			break;
		DOM_Object* obj = target->PutConstructorL(g_DOM_svg_element_spec[i].classname,
			static_cast<DOM_Runtime::SVGElementPrototype>(g_DOM_svg_element_spec[i].prototype),
			g_DOM_svg_element_spec[i].svgelm_interface);
		ConstructDOMImplementationSVGElementObjectL(*obj, g_DOM_svg_element_spec[i].svgelm_interface, target->GetRuntime());
		++i;
	}
}

/* static */ void
DOM_SVGElement::InitializeConstructorsTableL(OpString8HashTable<DOM_ConstructorInformation> *table)
{
#define ADD_CONSTRUCTOR(name, id) table->AddL(name, OP_NEW_L(DOM_ConstructorInformation, (name, DOM_Runtime::CTN_SVGELEMENT, id)))

	const SVGElementSpec *spec = g_DOM_svg_element_spec;

	for (unsigned index = 0; spec[index].type != Markup::HTE_ANY; ++index)
		ADD_CONSTRUCTOR(spec[index].classname, index);
#undef ADD_CONSTRUCTOR
}

/* static */ ES_Object *
DOM_SVGElement::CreateConstructorL(DOM_Runtime *runtime, DOM_Object *target, unsigned id)
{
	const SVGElementSpec *spec = g_DOM_svg_element_spec;

	DOM_Object *object = target->PutConstructorL(spec[id].classname, static_cast<DOM_Runtime::SVGElementPrototype>(spec[id].prototype), spec[id].svgelm_interface);

	ConstructDOMImplementationSVGElementObjectL(*object, spec[id].svgelm_interface, runtime);

	return *object;
}

/* static */ OP_STATUS
DOM_SVGElement::Make(DOM_SVGElement *&element, HTML_Element *html_element, DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	const char *classname = NULL;
	ES_Object *prototype = NULL;
	int i = 0;
	while (TRUE)
	{
		if (g_DOM_svg_element_spec[i].type == Markup::HTE_ANY /* Sentinel */ ||
			g_DOM_svg_element_spec[i].type == html_element->Type())
		{
			element = OP_NEW(DOM_SVGElement, (g_DOM_svg_element_spec[i].svgelm_interface));
			classname = g_DOM_svg_element_spec[i].classname;
			prototype = runtime->GetSVGElementPrototype(static_cast<DOM_Runtime::SVGElementPrototype>(g_DOM_svg_element_spec[i].prototype),
														g_DOM_svg_element_spec[i].svgelm_interface);
			break;
		}
		i++;
	}
	RETURN_IF_ERROR(DOMSetObjectRuntime(element, runtime, prototype, classname));
	RETURN_IF_ERROR(DOM_SVGObjectStore::Make(element->object_store, g_DOM_svg_element_entries, element->ifs));

	element->location = DOM_SVGLocation(element);
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_SVGElement::GetBoundingClientRect(DOM_Object *&object)
{
	RECT rect;

	// root svg element cannot be fetched via GetBoundingClientRect - need to use DOMGetPositionAndSize
	HTML_Element* element = GetThisElement();
	if (element && element->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
	{
		int left, top, width, height;
		RETURN_IF_ERROR(element->DOMGetPositionAndSize(GetEnvironment(), HTML_Element::DOM_PS_BORDER, left, top, width, height));
		rect.left = left;
		rect.top = top;
		rect.bottom = top + height;
		rect.right = left + width;
	}
	else
	{
		OpRect svg_rect;
		RETURN_IF_ERROR(SVGDOM::GetBoundingClientRect(GetThisElement(), svg_rect));
		rect.left = svg_rect.x;
		rect.top = svg_rect.y;
		rect.right = svg_rect.x + svg_rect.width;
		rect.bottom = svg_rect.y + svg_rect.height;
	}

	return MakeClientRect(object, rect, GetRuntime());
}

/* virtual */ OP_STATUS
DOM_SVGElement::GetClientRects(DOM_ClientRectList *&object)
{
	RETURN_IF_ERROR(DOM_ClientRectList::Make(object, GetRuntime()));

	RECT rect;
	OpRect svg_rect;
	RETURN_IF_ERROR(SVGDOM::GetBoundingClientRect(GetThisElement(), svg_rect));
	rect.left = svg_rect.x;
	rect.top = svg_rect.y;
	rect.right = svg_rect.x + svg_rect.width;
	rect.bottom = svg_rect.y + svg_rect.height;

	RECT *cpy = OP_NEW(RECT, (rect));
	if (OpStatus::IsMemoryError(object->GetList().Add(cpy)))
	{
		OP_DELETE(cpy);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

/* virtual */
DOM_SVGElement::~DOM_SVGElement()
{
	OP_DELETE(object_store);
}

/* virtual */ void
DOM_SVGElement::GCTraceSpecial(BOOL via_tree)
{
	DOM_Element::GCTraceSpecial(via_tree);
	object_store->GCTrace(GetRuntime());
}

/* static */ OP_STATUS
DOM_SVGElement::CreateElement(DOM_SVGElement *&element, DOM_Node *reference, const uni_char *name)
{
	DOM_Document *document;
	HTML_Element *htmlelement;
	DOM_Node *node;

	if (reference->IsA(DOM_TYPE_DOCUMENT))
		document = (DOM_Document *) reference;
	else
		document = reference->GetOwnerDocument();

	RETURN_IF_ERROR(HTML_Element::DOMCreateElement(htmlelement, document->GetEnvironment(), name, NS_IDX_SVG, TRUE));
	RETURN_IF_ERROR(document->GetEnvironment()->ConstructNode(node, htmlelement, document));

	element = (DOM_SVGElement *) node;
	return OpStatus::OK;
}

#ifdef SVG_FULL_11
ES_GetState
DOM_SVGElement::GetStringAttribute(OpAtom property_name, ES_Value* value)
{
	if (value)
	{
		DOM_EnvironmentImpl *environment = GetEnvironment();
		Markup::AttrType attr = DOM_AtomToSvgAttribute(property_name);

		DOM_EnvironmentImpl::CurrentState state(environment);
		state.SetTempBuffer();

		DOMSetString(value, this_element->DOMGetAttribute(environment, attr, NULL, NS_IDX_DEFAULT, TRUE));
	}

	return GET_SUCCESS;
}

ES_GetState
DOM_SVGElement::GetAnimatedList(SVGDOMItemType listtype, OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (value)
	{
 		DOM_Object* obj = object_store->GetObject(property_name);

 		if (!DOM_SVGLocation::IsValid(obj))
 		{
			Markup::AttrType attr = DOM_AtomToSvgAttribute(property_name);

			if (Markup::HA_XML != attr)
			{
				SVGDOMAnimatedValue* anim_list;
				GET_FAILED_IF_ERROR(SVGDOM::GetAnimatedList(GetThisElement(), GetEnvironment()->GetFramesDocument(), listtype, anim_list, attr));

				DOM_SVGAnimatedValue* dom_anim_list;
				const char* name = anim_list->GetDOMName();
				if (OpStatus::IsMemoryError(DOM_SVGAnimatedValue::Make(dom_anim_list, anim_list, name, location.WithAttr(attr, NS_SVG), GetEnvironment())))
				{
					OP_DELETE(anim_list);
					return GET_NO_MEMORY;
				}

				object_store->SetObject(property_name, dom_anim_list);
				DOMSetObject(value, dom_anim_list);
			}
			else
			{
				return GET_FAILED;
			}
 		}
 		else
 		{
 			DOMSetObject(value, obj);
 		}
	}
	return GET_SUCCESS;
}

ES_GetState
DOM_SVGElement::GetAnimatedValue(OpAtom property_name, SVGDOMItemType item_type, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (value)
	{
		DOM_Object* obj = object_store->GetObject(property_name);

		if (!DOM_SVGLocation::IsValid(obj))
		{
			SVGDOMAnimatedValue* anim_value = NULL;
			NS_Type ns;
			Markup::AttrType attr;
			if (property_name == OP_ATOM_href)
			{
				attr = Markup::XLINKA_HREF;
				ns = NS_XLINK;
			}
			else
			{
				ns = NS_SVG;
				attr = DOM_AtomToSvgAttribute(property_name);
				if (Markup::HA_XML == attr)
				{
					return GET_FAILED;
				}
			}

			GET_FAILED_IF_ERROR(SVGDOM::GetAnimatedValue(GetThisElement(), GetEnvironment()->GetFramesDocument(),
														item_type, anim_value, attr, ns));

			const char* name = anim_value->GetDOMName();
			DOM_SVGAnimatedValue* dom_anim_value;
			if (OpStatus::IsMemoryError(DOM_SVGAnimatedValue::Make(dom_anim_value, anim_value, name, location.WithAttr(attr, ns), GetEnvironment())))
			{
				OP_DELETE(anim_value);
				return GET_NO_MEMORY;
			}

			object_store->SetObject(property_name, dom_anim_value);
			DOMSetObject(value, dom_anim_value);
		}
		else
		{
			DOMSetObject(value, obj);
		}
	}
	return GET_SUCCESS;
}

ES_GetState
DOM_SVGElement::GetAnimatedNumberOptionalNumber(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, UINT32 idx)
{
	if (value)
	{
		DOM_Object* obj = object_store->GetObject(property_name);

		if (!DOM_SVGLocation::IsValid(obj))
		{
			Markup::AttrType attr = DOM_AtomToSvgAttribute(property_name);

			if (Markup::HA_XML != attr)
			{
				SVGDOMAnimatedValue* anim_value;
				GET_FAILED_IF_ERROR(SVGDOM::GetAnimatedNumberOptionalNumber(GetThisElement(), GetEnvironment()->GetFramesDocument(),
																		   anim_value, attr, idx));

				const char* name = anim_value->GetDOMName() ;
				DOM_SVGAnimatedValue* dom_anim_value;
				if (OpStatus::IsMemoryError(DOM_SVGAnimatedValue::Make(dom_anim_value, anim_value, name, location.WithAttr(attr, NS_SVG), GetEnvironment())))
				{
					OP_DELETE(anim_value);
					return GET_NO_MEMORY;
				}

				object_store->SetObject(property_name, dom_anim_value);
				DOMSetObject(value, dom_anim_value);
			}
		}
		else
		{
			DOMSetObject(value, obj);
		}
	}
	return GET_SUCCESS;
}

ES_GetState
DOM_SVGElement::GetAnimatedString(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	return GetAnimatedValue(property_name, SVG_DOM_ITEMTYPE_STRING, value, origining_runtime);
}

ES_GetState
DOM_SVGElement::GetAnimatedPreserveAspectRatio(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	return GetAnimatedValue(property_name, SVG_DOM_ITEMTYPE_PRESERVE_ASPECT_RATIO, value, origining_runtime);
}

ES_GetState
DOM_SVGElement::GetAnimatedAngle(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	return GetAnimatedValue(property_name, SVG_DOM_ITEMTYPE_ANGLE, value, origining_runtime);
}

ES_GetState
DOM_SVGElement::GetAnimatedEnumeration(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	return GetAnimatedValue(property_name, SVG_DOM_ITEMTYPE_ENUM, value, origining_runtime);
}

ES_GetState
DOM_SVGElement::GetAnimatedLength(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	return GetAnimatedValue(property_name, SVG_DOM_ITEMTYPE_LENGTH, value, origining_runtime);
}

ES_GetState
DOM_SVGElement::GetAnimatedNumber(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	return GetAnimatedValue(property_name, SVG_DOM_ITEMTYPE_NUMBER, value, origining_runtime);
}

ES_GetState
DOM_SVGElement::GetInstanceRoot(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (!value)
	{
		return GET_SUCCESS;
	}

	HTML_Element* root;
	DOM_Environment* environment = GetEnvironment();
	OP_STATUS status = SVGDOM::GetInstanceRoot(GetThisElement(), environment->GetFramesDocument(),
							property_name == OP_ATOM_animatedInstanceRoot, root);

	GET_FAILED_IF_ERROR(status);
	OP_ASSERT(root);

	DOM_Object* node;
	GET_FAILED_IF_ERROR(environment->ConstructNode(node, root));
	OP_ASSERT(node->IsA(DOM_TYPE_SVG_ELEMENT_INSTANCE));
	DOMSetObject(value, node);
	return GET_SUCCESS;
}

ES_GetState
DOM_SVGElement::GetViewport(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (value)
	{
		DOM_Object* obj = object_store->GetObject(property_name);

		if (!DOM_SVGLocation::IsValid(obj))
		{
			SVGDOMRect* rect;
			GET_FAILED_IF_ERROR(SVGDOM::GetViewPort(GetThisElement(), GetEnvironment()->GetFramesDocument(), rect));

			DOM_SVGObject* dom_rect;
			if (OpStatus::IsMemoryError(DOM_SVGObject::Make(dom_rect, rect, DOM_SVGLocation(this),
											GetEnvironment())))
			{
				OP_DELETE(rect);
				return GET_NO_MEMORY;
			}

			DOMSetObject(value, dom_rect);
			object_store->SetObject(property_name, dom_rect);
		}
		else
		{
			DOMSetObject(value, obj);
		}
	}

	return GET_SUCCESS;
}
#endif // SVG_FULL_11

ES_GetState
DOM_SVGElement::GetCurrentTranslate(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (value)
	{
		DOM_Object* obj = object_store->GetObject(property_name);

		if (!DOM_SVGLocation::IsValid(obj))
		{
			SVGDOMPoint* point = NULL;
			GET_FAILED_IF_ERROR(SVGDOM::GetCurrentTranslate(GetThisElement(), GetEnvironment()->GetFramesDocument(), point));

			DOM_SVGObject* dom_point = NULL;
			if (OpStatus::IsMemoryError(DOM_SVGObject::Make(dom_point, point, DOM_SVGLocation(this,TRUE),
												GetEnvironment())))
			{
				OP_DELETE(point);
				return GET_NO_MEMORY;
			}

			DOMSetObject(value, dom_point);
			object_store->SetObject(property_name, dom_point);
		}
		else
		{
			DOMSetObject(value, obj);
		}
	}

	return GET_SUCCESS;
}

#ifdef SVG_FULL_11
ES_GetState
DOM_SVGElement::GetPointList(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (value)
	{
		DOM_Object* obj = object_store->GetObject(property_name);
		if (!DOM_SVGLocation::IsValid(obj))
		{
			HTML_Element* elm = GetThisElement();
			FramesDocument* frm_doc = GetEnvironment()->GetFramesDocument();
			SVGDOMItem* svg_item = NULL;

			OP_ASSERT(property_name == OP_ATOM_points || property_name == OP_ATOM_animatedPoints);
			GET_FAILED_IF_ERROR(SVGDOM::GetPointList(elm, frm_doc, svg_item, Markup::SVGA_POINTS, property_name == OP_ATOM_points));

			if (!svg_item->IsA(SVG_DOM_ITEMTYPE_LIST))
			{
				OP_DELETE(svg_item);
				return GET_FAILED;
			}

			DOM_SVGList* dom_obj;
			if (OpStatus::IsMemoryError(DOM_SVGList::Make(dom_obj, (SVGDOMList*)svg_item,
													  DOM_SVGLocation(this, Markup::SVGA_POINTS, NS_SVG),
													  GetEnvironment())))
			{
				OP_DELETE(svg_item);
				return GET_NO_MEMORY;
			}

			DOMSetObject(value, dom_obj);
			object_store->SetObject(property_name, dom_obj);
		}
		else
		{
			DOMSetObject(value, obj);
		}
	}

	return GET_SUCCESS;
}

ES_GetState
DOM_SVGElement::GetPathSegList(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (value)
	{
		DOM_Object* obj = object_store->GetObject(property_name);

		if (!DOM_SVGLocation::IsValid(obj))
		{
			BOOL is_base = (property_name == OP_ATOM_pathSegList ||
							property_name == OP_ATOM_normalizedPathSegList);
			BOOL is_normalized = (property_name == OP_ATOM_normalizedPathSegList ||
								  property_name == OP_ATOM_animatedNormalizedPathSegList);
			SVGDOMItem* svg_item = NULL;
			HTML_Element* elm = GetThisElement();
			FramesDocument* frm_doc = GetEnvironment()->GetFramesDocument();

			GET_FAILED_IF_ERROR(SVGDOM::GetPathSegList(elm, frm_doc, svg_item, Markup::SVGA_D, is_base, is_normalized));

			SVGDOMList* svg_list = static_cast<SVGDOMList*>(svg_item);
			DOM_SVGList* dom_obj;
			if (OpStatus::IsMemoryError(DOM_SVGList::Make(dom_obj, svg_list,
													  DOM_SVGLocation(this, Markup::SVGA_D, NS_SVG),
													  GetEnvironment())))
			{
				OP_DELETE(svg_item);
				return GET_NO_MEMORY;
			}

			DOMSetObject(value, dom_obj);
			object_store->SetObject(property_name, dom_obj);
		}
		else
		{
			DOMSetObject(value, obj);
		}
	}
	return GET_SUCCESS;
}

ES_GetState
DOM_SVGElement::GetStringList(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (value)
	{
		DOM_Object* obj = object_store->GetObject(property_name);
		if (!DOM_SVGLocation::IsValid(obj))
		{
			FramesDocument* frm_doc = GetEnvironment()->GetFramesDocument();
			SVGDOMStringList* string_list = NULL;
			Markup::AttrType attr = DOM_AtomToSvgAttribute(property_name);
			GET_FAILED_IF_ERROR(SVGDOM::GetStringList(this_element, frm_doc,
													  attr, string_list));
			OP_ASSERT(string_list != NULL);
			DOM_SVGStringList* dom_obj;
			if (OpStatus::IsMemoryError(DOM_SVGStringList::Make(dom_obj, string_list,
										DOM_SVGLocation(this, attr, NS_SVG),
										GetEnvironment())))
			{
				OP_DELETE(string_list);
				return GET_NO_MEMORY;
			}

			DOMSetObject(value, dom_obj);
			object_store->SetObject(property_name, dom_obj);
		}
		else
		{
			DOMSetObject(value, obj);
		}
	}
	return GET_SUCCESS;
}

ES_GetState
DOM_SVGElement::GetViewportElement(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (value)
	{
		BOOL find_nearest = (property_name != OP_ATOM_farthestViewportElement);
		BOOL find_svg_only = (property_name == OP_ATOM_ownerSVGElement) ||
			(property_name == OP_ATOM_farthestViewportElement);

		HTML_Element* viewport_elm = NULL;
		GET_FAILED_IF_ERROR(SVGDOM::GetViewportElement(GetThisElement(), find_nearest, find_svg_only, viewport_elm));
		DOMSetElement(value, viewport_elm);
	}
	return GET_SUCCESS;
}
#endif // SVG_FULL_11

/* virtual */ ES_GetState
DOM_SVGElement::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
#ifdef SVG_FULL_11
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_A_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_target:
			return GetAnimatedString(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_ANIMATED_PATH_DATA))
	{
		switch (property_name)
		{
		case OP_ATOM_pathSegList:
		case OP_ATOM_normalizedPathSegList:
		case OP_ATOM_animatedPathSegList:
		case OP_ATOM_animatedNormalizedPathSegList:
			return GetPathSegList(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_ANIMATED_POINTS))
	{
		switch(property_name)
		{
		case OP_ATOM_points:
		case OP_ATOM_animatedPoints:
			return GetPointList(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_CIRCLE_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_cx:
		case OP_ATOM_cy:
		case OP_ATOM_r:
			return GetAnimatedLength(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_CLIPPATH_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_clipPathUnits:
			return GetAnimatedEnumeration(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_ELLIPSE_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_cx:
		case OP_ATOM_cy:
		case OP_ATOM_rx:
		case OP_ATOM_ry:
			return GetAnimatedLength(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FIT_TO_VIEW_BOX))
	{
		switch (property_name)
		{
		case OP_ATOM_viewBox:
			return GetAnimatedValue(property_name, SVG_DOM_ITEMTYPE_RECT, value, origining_runtime);
		case OP_ATOM_preserveAspectRatio:
			return GetAnimatedPreserveAspectRatio(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FOREIGNOBJECT_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_x:
		case OP_ATOM_y:
		case OP_ATOM_width:
		case OP_ATOM_height:
			return GetAnimatedLength(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_GRADIENT_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_gradientUnits:
			return GetAnimatedEnumeration(property_name, value, origining_runtime);
		case OP_ATOM_gradientTransform:
			return GetAnimatedList(SVG_DOM_ITEMTYPE_TRANSFORM, property_name, value, origining_runtime);
		case OP_ATOM_spreadMethod:
			return GetAnimatedEnumeration(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_IMAGE_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_x:
		case OP_ATOM_y:
		case OP_ATOM_width:
		case OP_ATOM_height:
			return GetAnimatedLength(property_name, value, origining_runtime);
		case OP_ATOM_preserveAspectRatio:
			return GetAnimatedPreserveAspectRatio(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_LINEARGRADIENT_ELEMENT) ||
		ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_LINE_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_x1:
		case OP_ATOM_x2:
		case OP_ATOM_y1:
		case OP_ATOM_y2:
			return GetAnimatedLength(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_LOCATABLE))
	{
		switch(property_name)
		{
		case OP_ATOM_nearestViewportElement:
		case OP_ATOM_farthestViewportElement:
			return GetViewportElement(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_MARKER_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_refX:
		case OP_ATOM_refY:
		case OP_ATOM_markerWidth:
		case OP_ATOM_markerHeight:
			return GetAnimatedLength(property_name, value, origining_runtime);
		case OP_ATOM_markerUnits:
		case OP_ATOM_orientType:
			return GetAnimatedEnumeration(property_name, value, origining_runtime);
		case OP_ATOM_orientAngle:
			return GetAnimatedAngle(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_MASK_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_maskUnits:
			return GetAnimatedEnumeration(property_name, value, origining_runtime);
		case OP_ATOM_maskContentUnits:
			return GetAnimatedEnumeration(property_name, value, origining_runtime);
		case OP_ATOM_width:
		case OP_ATOM_height:
		case OP_ATOM_x:
		case OP_ATOM_y:
			return GetAnimatedLength(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_PATH_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_pathLength:
			return GetAnimatedNumber(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_PATTERN_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_patternUnits:
			return GetAnimatedEnumeration(property_name, value, origining_runtime);
		case OP_ATOM_patternContentUnits:
			return GetAnimatedEnumeration(property_name, value, origining_runtime);
		case OP_ATOM_patternTransform:
			return GetAnimatedList(SVG_DOM_ITEMTYPE_TRANSFORM, property_name, value, origining_runtime);
		case OP_ATOM_width:
		case OP_ATOM_height:
		case OP_ATOM_x:
		case OP_ATOM_y:
			return GetAnimatedLength(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_RADIALGRADIENT_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_cx:
		case OP_ATOM_cy:
		case OP_ATOM_fx:
		case OP_ATOM_fy:
		case OP_ATOM_r:
			return GetAnimatedLength(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_RECT_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_width:
		case OP_ATOM_height:
		case OP_ATOM_x:
		case OP_ATOM_y:
		case OP_ATOM_rx:
		case OP_ATOM_ry:
			return GetAnimatedLength(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_STOP_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_offset:
			return GetAnimatedNumber(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_STYLABLE))
	{
		switch(property_name)
		{
		case OP_ATOM_className:
			return GetAnimatedString(property_name, value, origining_runtime);
		case OP_ATOM_style:
			if (value)
			{
				ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_style);
				if (result != GET_FAILED)
					return result;

				DOM_CSSStyleDeclaration *style;
				GET_FAILED_IF_ERROR(DOM_CSSStyleDeclaration::Make(style, this, DOM_CSSStyleDeclaration::DOM_ST_INLINE));
				GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_style, style));
				DOMSetObject(value, style);
			}
			return GET_SUCCESS;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_STYLE_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_xmlspace:
		{
			DOMSetString(value, this_element->DOMGetAttribute(GetEnvironment(), XMLA_SPACE, NULL, NS_IDX_XML, TRUE));
			return GET_SUCCESS;
		}
		case OP_ATOM_type:
		case OP_ATOM_media:
		case OP_ATOM_title:
			return GetStringAttribute(property_name, value);
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_SCRIPT_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_type:
			return GetStringAttribute(property_name, value);

#ifdef USER_JAVASCRIPT
		// Like for ScriptElement.text, the script source
		// is exposed for select UserJS events
		// (cf. DOM_HTMLScriptElement::GetName().)
		case OP_ATOM_text:
		{
			TempBuffer *buffer = GetEmptyTempBuf();
			OP_BOOLEAN result = DOM_UserJSEvent::GetScriptSource(origining_runtime, buffer);
			GET_FAILED_IF_ERROR(result);
			if (result == OpBoolean::IS_TRUE)
			{
				DOMSetString(value, buffer);
				return GET_SUCCESS;
			}
			break;
		}
#endif // USER_JAVASCRIPT
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_TEXTCONTENT_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_textLength:
			return GetAnimatedLength(property_name, value, origining_runtime);
		case OP_ATOM_lengthAdjust:
			return GetAnimatedEnumeration(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_TEXTPATH_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_startOffset:
			return GetAnimatedLength(property_name, value, origining_runtime);
		case OP_ATOM_method:
		case OP_ATOM_spacing:
			return GetAnimatedEnumeration(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_TEXTPOSITIONING_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_x:
		case OP_ATOM_y:
		case OP_ATOM_dx:
		case OP_ATOM_dy:
			return GetAnimatedList(SVG_DOM_ITEMTYPE_LENGTH, property_name, value, origining_runtime);
		case OP_ATOM_rotate:
			return GetAnimatedList(SVG_DOM_ITEMTYPE_NUMBER, property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_TRANSFORMABLE))
	{
		switch(property_name)
		{
		case OP_ATOM_transform:
			return GetAnimatedList(SVG_DOM_ITEMTYPE_TRANSFORM, property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_URI_REFERENCE))
	{
		switch(property_name)
		{
		case OP_ATOM_href:
			return GetAnimatedString(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FILTER_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_x:
		case OP_ATOM_y:
		case OP_ATOM_width:
		case OP_ATOM_height:
			return GetAnimatedLength(property_name, value, origining_runtime);
		case OP_ATOM_filterUnits:
		case OP_ATOM_primitiveUnits:
			return GetAnimatedEnumeration(property_name, value, origining_runtime);
		case OP_ATOM_filterResX:
			return GetAnimatedNumberOptionalNumber(property_name, value, origining_runtime, 0);
		case OP_ATOM_filterResY:
			return GetAnimatedNumberOptionalNumber(property_name, value, origining_runtime, 1);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FILTER_PRIMITIVE_STANDARD_ATTRIBUTES))
	{
		switch (property_name)
		{
		case OP_ATOM_x:
		case OP_ATOM_y:
		case OP_ATOM_width:
		case OP_ATOM_height:
			return GetAnimatedLength(property_name, value, origining_runtime);
		case OP_ATOM_result:
			return GetAnimatedString(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEFLOOD_ELEMENT) ||
		ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FECOMPONENTTRANSFER_ELEMENT) ||
		ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEMERGENODE_ELEMENT) ||
		ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FETILE_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
			return GetAnimatedString(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEBLEND_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
		case OP_ATOM_in2:
			return GetAnimatedString(property_name, value, origining_runtime);
		case OP_ATOM_mode:
			return GetAnimatedEnumeration(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FECOLORMATRIX_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
			return GetAnimatedString(property_name, value, origining_runtime);
		case OP_ATOM_type:
			return GetAnimatedEnumeration(property_name, value, origining_runtime);
		case OP_ATOM_values:
			return GetAnimatedList(SVG_DOM_ITEMTYPE_NUMBER, property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_COMPONENT_TRANSFER_FUNCTION_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_type:
			return GetAnimatedEnumeration(property_name, value, origining_runtime);
		case OP_ATOM_tableValues:
			return GetAnimatedList(SVG_DOM_ITEMTYPE_NUMBER, property_name, value, origining_runtime);
		case OP_ATOM_slope:
		case OP_ATOM_intercept:
		case OP_ATOM_amplitude:
		case OP_ATOM_exponent:
		case OP_ATOM_offset:
			return GetAnimatedNumber(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FECOMPOSITE_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
		case OP_ATOM_in2:
			return GetAnimatedString(property_name, value, origining_runtime);
		case OP_ATOM_operator:
			return GetAnimatedEnumeration(property_name, value, origining_runtime);
		case OP_ATOM_k1:
		case OP_ATOM_k2:
		case OP_ATOM_k3:
		case OP_ATOM_k4:
			return GetAnimatedNumber(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FECONVOLVEMATRIX_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
			return GetAnimatedString(property_name, value, origining_runtime);
		case OP_ATOM_orderX:
			return GetAnimatedNumberOptionalNumber(property_name, value, origining_runtime, 0);
		case OP_ATOM_orderY:
			return GetAnimatedNumberOptionalNumber(property_name, value, origining_runtime, 1);
		case OP_ATOM_targetX:
		case OP_ATOM_targetY: // FIXME: Really is SVGAnimatedInteger's
			return GetAnimatedNumber(property_name, value, origining_runtime);
		case OP_ATOM_kernelMatrix:
			return GetAnimatedList(SVG_DOM_ITEMTYPE_NUMBER, property_name, value, origining_runtime);
		case OP_ATOM_divisor:
		case OP_ATOM_bias:
			return GetAnimatedNumber(property_name, value, origining_runtime);
		case OP_ATOM_edgeMode:
			return GetAnimatedEnumeration(property_name, value, origining_runtime);
		case OP_ATOM_kernelUnitLengthX:
			return GetAnimatedNumberOptionalNumber(property_name, value, origining_runtime, 0);
		case OP_ATOM_kernelUnitLengthY:
			return GetAnimatedNumberOptionalNumber(property_name, value, origining_runtime, 1);
		case OP_ATOM_preserveAlpha:
			return GetAnimatedValue(property_name, SVG_DOM_ITEMTYPE_BOOLEAN, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEDIFFUSELIGHTING_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
			return GetAnimatedString(property_name, value, origining_runtime);
		case OP_ATOM_surfaceScale:
		case OP_ATOM_diffuseConstant:
			return GetAnimatedNumber(property_name, value, origining_runtime);
		case OP_ATOM_kernelUnitLengthX:
			return GetAnimatedNumberOptionalNumber(property_name, value, origining_runtime, 0);
		case OP_ATOM_kernelUnitLengthY:
			return GetAnimatedNumberOptionalNumber(property_name, value, origining_runtime, 1);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEDISTANTLIGHT_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_azimuth:
		case OP_ATOM_elevation:
			return GetAnimatedNumber(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEPOINTLIGHT_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_x:
		case OP_ATOM_y:
		case OP_ATOM_z:
			return GetAnimatedNumber(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FESPOTLIGHT_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_x:
		case OP_ATOM_y:
		case OP_ATOM_z:
		case OP_ATOM_pointsAtX:
		case OP_ATOM_pointsAtY:
		case OP_ATOM_pointsAtZ:
		case OP_ATOM_specularExponent:
		case OP_ATOM_limitingConeAngle:
			return GetAnimatedNumber(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEDISPLACEMENTMAP_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
		case OP_ATOM_in2:
			return GetAnimatedString(property_name, value, origining_runtime);
		case OP_ATOM_scale:
			return GetAnimatedNumber(property_name, value, origining_runtime);
		case OP_ATOM_xChannelSelector:
		case OP_ATOM_yChannelSelector:
			return GetAnimatedEnumeration(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEGAUSSIANBLUR_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
			return GetAnimatedString(property_name, value, origining_runtime);
		case OP_ATOM_stdDeviationX:
			return GetAnimatedNumberOptionalNumber(property_name, value, origining_runtime, 0);
		case OP_ATOM_stdDeviationY:
			return GetAnimatedNumberOptionalNumber(property_name, value, origining_runtime, 1);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEIMAGE_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_preserveAspectRatio:
			return GetAnimatedPreserveAspectRatio(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEMORPHOLOGY_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
			return GetAnimatedString(property_name, value, origining_runtime);
		case OP_ATOM_operator:
			return GetAnimatedEnumeration(property_name, value, origining_runtime);
		case OP_ATOM_radiusX:
			return GetAnimatedNumberOptionalNumber(property_name, value, origining_runtime, 0);
		case OP_ATOM_radiusY:
			return GetAnimatedNumberOptionalNumber(property_name, value, origining_runtime, 1);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEOFFSET_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
			return GetAnimatedString(property_name, value, origining_runtime);
		case OP_ATOM_dx:
		case OP_ATOM_dy:
			return GetAnimatedNumber(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FESPECULARLIGHTING_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
			return GetAnimatedString(property_name, value, origining_runtime);
		case OP_ATOM_surfaceScale:
		case OP_ATOM_specularConstant:
		case OP_ATOM_specularExponent:
			return GetAnimatedNumber(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FETURBULENCE_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_baseFrequencyX:
			return GetAnimatedNumberOptionalNumber(property_name, value, origining_runtime, 0);
		case OP_ATOM_baseFrequencyY:
			return GetAnimatedNumberOptionalNumber(property_name, value, origining_runtime, 1);
		case OP_ATOM_numOctaves: // FIXME: Really SVGAnimatedInteger
		case OP_ATOM_seed:
			return GetAnimatedNumber(property_name, value, origining_runtime);
		case OP_ATOM_stitchTiles:
		case OP_ATOM_type:
			return GetAnimatedEnumeration(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_USE_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_x:
		case OP_ATOM_y:
		case OP_ATOM_width:
		case OP_ATOM_height:
			return GetAnimatedLength(property_name, value, origining_runtime);
		case OP_ATOM_instanceRoot:
		case OP_ATOM_animatedInstanceRoot:
			return GetInstanceRoot(property_name, value, origining_runtime);
			break;
		default:
			break;
		}
	}

//	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_VIEW_ELEMENT))
//	{
//		switch (property_name)
//		{
//		case OP_ATOM_viewTarget:
//			return GetStringList(property_name, value, origining_runtime);
//		default:
//			break;
//		}
//	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED))
	{
		switch (property_name)
		{
		case OP_ATOM_externalResourcesRequired:
			return GetAnimatedValue(property_name, SVG_DOM_ITEMTYPE_BOOLEAN, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_TESTS))
	{
		switch(property_name)
		{
		case OP_ATOM_requiredFeatures:
		case OP_ATOM_requiredExtensions:
		case OP_ATOM_systemLanguage:
			return GetStringList(property_name, value, origining_runtime);
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_ANIMATION_ELEMENT_BASE))
	{
		switch(property_name)
		{
		case OP_ATOM_targetElement:
		{
			if (value)
			{
				HTML_Element* target;
				GET_FAILED_IF_ERROR(SVGDOM::GetAnimationTargetElement(GetThisElement(),
																	  GetEnvironment()->GetFramesDocument(),
																	  target));
				DOMSetElement(value, target);
			}
			return GET_SUCCESS;
		}
		break;
		default:
			break;
		}
	}

	if(ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_CAN_REFERENCE_DOCUMENT))
	{
		if(property_name == OP_ATOM_contentDocument)
		{
			return GetFrameEnvironment(value, FRAME_DOCUMENT, static_cast<DOM_Runtime *>(origining_runtime));
		}
		else if(property_name == OP_ATOM_contentWindow)
		{
			return GetFrameEnvironment(value, FRAME_WINDOW, static_cast<DOM_Runtime *>(origining_runtime));
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_ZOOM_AND_PAN))
	{
		switch (property_name)
		{
		case OP_ATOM_zoomAndPan:
		{
			SVGDOM::SVGZoomAndPanType zap = SVGDOM::SVG_DOM_SVG_ZOOMANDPAN_UNKNOWN;
			OpStatus::Ignore(SVGDOM::AccessZoomAndPan(this_element, GetEnvironment()->GetFramesDocument(), zap, FALSE /* read */));
			DOMSetNumber(value, zap);
			return GET_SUCCESS;
		}
		default:
			break;
		}
	}
#endif // SVG_FULL_11

#ifdef SVG_TINY_12
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_TIMED_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_isPaused:
		{
			BOOL paused;
			GET_FAILED_IF_ERROR(SVGDOM::AnimationsPaused(this_element, GetEnvironment()->GetFramesDocument(), paused));
			DOMSetBoolean(value, paused);
			return GET_SUCCESS;
		}
		default:
			break;
		}
	}
#endif // SVG_TINY_12

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_SVG_ELEMENT))
	{
		switch (property_name)
		{
			case OP_ATOM_targetFps:
				{
					if (FramesDocument *frames_doc = GetEnvironment()->GetFramesDocument())
					{
					    SVGImage* svg_img = g_svg_manager->GetSVGImage(frames_doc->GetLogicalDocument(), GetThisElement());
					    UINT32 fps = 0;
					    if(svg_img)
					    {
						    fps = svg_img->GetTargetFPS();
					    }

					    DOMSetNumber(value, fps);
					    return GET_SUCCESS;
					}
					else
					{
						DOMSetUndefined(value);
						return GET_SUCCESS;
					}
				}
				break;
			case OP_ATOM_currentFps:
				{
					if (FramesDocument *frames_doc = GetEnvironment()->GetFramesDocument())
					{
					    SVGImage* svg_img = g_svg_manager->GetSVGImage(frames_doc->GetLogicalDocument(), GetThisElement());
					    UINT32 fpsFixed = 0;
					    if(svg_img)
					    {
						    fpsFixed = svg_img->GetLastKnownFPS();
					    }

					    DOMSetNumber(value, fpsFixed / (1<<16));
					    return GET_SUCCESS;
					}
					else
					{
						DOMSetUndefined(value);
						return GET_SUCCESS;
					}
				}
				break;
#ifdef SVG_FULL_11
			case OP_ATOM_x:
			case OP_ATOM_y:
			case OP_ATOM_width:
			case OP_ATOM_height:
				return GetAnimatedLength(property_name, value, origining_runtime);

 			case OP_ATOM_contentScriptType:
				DOMSetString(value, UNI_L("text/ecmascript"));
				return GET_SUCCESS;
 			case OP_ATOM_contentStyleType:
				DOMSetString(value, UNI_L("text/css"));
				return GET_SUCCESS;
			case OP_ATOM_viewport:
				return GetViewport(property_name, value, origining_runtime);

			case OP_ATOM_pixelUnitToMillimeterX:
			{
				double lambda = SVGDOM::PixelUnitToMillimeterX(GetThisElement(), GetEnvironment()->GetFramesDocument());
				DOMSetNumber(value, lambda);
				return GET_SUCCESS;
			}
			break;
			case OP_ATOM_pixelUnitToMillimeterY:
			{
				double lambda = SVGDOM::PixelUnitToMillimeterY(GetThisElement(), GetEnvironment()->GetFramesDocument());
				DOMSetNumber(value, lambda);
				return GET_SUCCESS;
			}
			break;
			case OP_ATOM_screenPixelToMillimeterX:
			{
				double lambda = SVGDOM::ScreenPixelToMillimeterX(GetThisElement(), GetEnvironment()->GetFramesDocument());
				DOMSetNumber(value, lambda);
				return GET_SUCCESS;
			}
			break;
			case OP_ATOM_screenPixelToMillimeterY:
			{
				double lambda = SVGDOM::ScreenPixelToMillimeterY(GetThisElement(), GetEnvironment()->GetFramesDocument());
				DOMSetNumber(value, lambda);
				return GET_SUCCESS;
			}
			break;
			case OP_ATOM_useCurrentView:
			{
				DOMSetBoolean(value, SVGDOM::UseCurrentView(GetThisElement(), GetEnvironment()->GetFramesDocument()));
				return GET_SUCCESS;
			}
			break;
#endif // SVG_FULL_11
			case OP_ATOM_currentScale:
			{
				double scale;
				if (OpStatus::IsMemoryError(SVGDOM::GetCurrentScale(GetThisElement(), GetEnvironment()->GetFramesDocument(), scale)))
					return GET_NO_MEMORY;

				DOMSetNumber(value, scale);
				return GET_SUCCESS;
			}
			break;
			case OP_ATOM_currentTranslate:
				return GetCurrentTranslate(property_name, value, origining_runtime);
#ifdef SVG_TINY_12
			case OP_ATOM_currentRotate:
			{
				double rotation;
				if (OpStatus::IsMemoryError(SVGDOM::GetCurrentRotate(GetThisElement(), GetEnvironment()->GetFramesDocument(), rotation)))
					return GET_NO_MEMORY;

				DOMSetNumber(value, rotation);
				return GET_SUCCESS;
			}
			break;
#endif // SVG_TINY_12
		}
	}

    switch (property_name)
    {
	case OP_ATOM_id:
	{
		DOMSetString(value, this_element->GetId());
		return GET_SUCCESS;
	}
#ifdef SVG_FULL_11
	case OP_ATOM_xmlbase:
	{
		DOMSetString(value, this_element->DOMGetAttribute(GetEnvironment(), XMLA_BASE, NULL, NS_IDX_XML, TRUE));
		return GET_SUCCESS;
	}
	case OP_ATOM_viewportElement:
	case OP_ATOM_ownerSVGElement:
		return GetViewportElement(property_name, value, origining_runtime);
#endif // SVG_FULL_11
	case OP_ATOM_classList:
		if (value)
		{
			ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_classList);
			if (result != GET_FAILED)
				return result;

			DOM_DOMTokenList *classlist_token;
			GET_FAILED_IF_ERROR(DOM_DOMTokenList::Make(classlist_token, this));
			GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_classList, classlist_token));
			DOMSetObject(value, classlist_token);
		}
		return GET_SUCCESS;
#ifdef DRAG_SUPPORT
	case OP_ATOM_dropzone:
		DOMSetString(value, this_element->GetDropzone());
		return GET_SUCCESS;

	case OP_ATOM_draggable:
		DOMSetBoolean(value, this_element->IsDraggable(GetFramesDocument()));
		return GET_SUCCESS;
#endif // DRAG_SUPPORT

	default:
		return DOM_Element::GetName(property_name, value, origining_runtime);
    }
}

ES_PutState
DOM_SVGElement::SetStringAttribute(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (!OriginCheck(origining_runtime))
		return PUT_SECURITY_VIOLATION;

	if (value->type != VALUE_STRING)
		return PUT_NEEDS_STRING;

	if(property_name == OP_ATOM_xmlbase)
	{
		PUT_FAILED_IF_ERROR(SetAttribute(XMLA_BASE, NULL, NS_IDX_XML, value->value.string, value->GetStringLength(), TRUE, origining_runtime));
	}
	else
	{
		PUT_FAILED_IF_ERROR(SetAttribute(DOM_AtomToSvgAttribute(property_name), NULL, NS_IDX_DEFAULT, value->value.string, value->GetStringLength(), TRUE, origining_runtime));
	}
	return PUT_SUCCESS;
}

/* virtual */ ES_PutState
DOM_SVGElement::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
#ifdef SVG_FULL_11
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_A_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_target:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_ANIMATED_PATH_DATA))
	{
		switch (property_name)
		{
		case OP_ATOM_pathSegList:
		case OP_ATOM_normalizedPathSegList:
		case OP_ATOM_animatedPathSegList:
		case OP_ATOM_animatedNormalizedPathSegList:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_ANIMATED_POINTS))
	{
		switch(property_name)
		{
		case OP_ATOM_points:
		case OP_ATOM_animatedPoints:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_CIRCLE_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_cx:
		case OP_ATOM_cy:
		case OP_ATOM_r:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_COMPONENT_TRANSFER_FUNCTION_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_type:
		case OP_ATOM_tableValues:
		case OP_ATOM_slope:
		case OP_ATOM_intercept:
		case OP_ATOM_amplitude:
		case OP_ATOM_exponent:
		case OP_ATOM_offset:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_ELLIPSE_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_cx:
		case OP_ATOM_cy:
		case OP_ATOM_rx:
		case OP_ATOM_ry:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FOREIGNOBJECT_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_x:
		case OP_ATOM_y:
		case OP_ATOM_width:
		case OP_ATOM_height:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FECONVOLVEMATRIX_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1: // NOTE: Not in interface for this element
		case OP_ATOM_orderX:
		case OP_ATOM_orderY:
		case OP_ATOM_targetX:
		case OP_ATOM_targetY:
		case OP_ATOM_kernelMatrix:
		case OP_ATOM_divisor:
		case OP_ATOM_bias:
		case OP_ATOM_edgeMode:
		case OP_ATOM_kernelUnitLengthX:
		case OP_ATOM_kernelUnitLengthY:
		case OP_ATOM_preserveAlpha:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FECOMPOSITE_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
		case OP_ATOM_in2:
		case OP_ATOM_operator:
		case OP_ATOM_k1:
		case OP_ATOM_k2:
		case OP_ATOM_k3:
		case OP_ATOM_k4:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEDIFFUSELIGHTING_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
		case OP_ATOM_surfaceScale:
		case OP_ATOM_diffuseConstant:
		case OP_ATOM_kernelUnitLengthX:
		case OP_ATOM_kernelUnitLengthY:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEDISPLACEMENTMAP_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
		case OP_ATOM_in2:
		case OP_ATOM_scale:
		case OP_ATOM_xChannelSelector:
		case OP_ATOM_yChannelSelector:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEDISTANTLIGHT_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_azimuth:
		case OP_ATOM_elevation:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEGAUSSIANBLUR_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
		case OP_ATOM_stdDeviationX:
		case OP_ATOM_stdDeviationY:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEIMAGE_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_preserveAspectRatio:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEMORPHOLOGY_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
		case OP_ATOM_operator:
		case OP_ATOM_radiusX:
		case OP_ATOM_radiusY:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEPOINTLIGHT_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_x:
		case OP_ATOM_y:
		case OP_ATOM_z:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FESPOTLIGHT_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_x:
		case OP_ATOM_y:
		case OP_ATOM_z:
		case OP_ATOM_pointsAtX:
		case OP_ATOM_pointsAtY:
		case OP_ATOM_pointsAtZ:
		case OP_ATOM_specularExponent:
		case OP_ATOM_limitingConeAngle:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEFLOOD_ELEMENT) ||
		ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FECOMPONENTTRANSFER_ELEMENT) ||
		ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEMERGENODE_ELEMENT) ||
		ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FETILE_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEBLEND_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
		case OP_ATOM_in2:
		case OP_ATOM_mode:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FECOLORMATRIX_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
		case OP_ATOM_type:
		case OP_ATOM_values:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEOFFSET_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
		case OP_ATOM_dx:
		case OP_ATOM_dy:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FESPECULARLIGHTING_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_in1:
		case OP_ATOM_surfaceScale:
		case OP_ATOM_specularConstant:
		case OP_ATOM_specularExponent:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FETURBULENCE_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_baseFrequencyX:
		case OP_ATOM_baseFrequencyY:
		case OP_ATOM_numOctaves:
		case OP_ATOM_seed:
		case OP_ATOM_stitchTiles:
		case OP_ATOM_type:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FILTER_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_x:
		case OP_ATOM_y:
		case OP_ATOM_width:
		case OP_ATOM_height:
		case OP_ATOM_filterUnits:
		case OP_ATOM_primitiveUnits:
		case OP_ATOM_filterResX:
		case OP_ATOM_filterResY:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FILTER_PRIMITIVE_STANDARD_ATTRIBUTES))
	{
		switch (property_name)
		{
		case OP_ATOM_x:
		case OP_ATOM_y:
		case OP_ATOM_width:
		case OP_ATOM_height:
		case OP_ATOM_result:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FIT_TO_VIEW_BOX))
	{
		switch (property_name)
		{
		case OP_ATOM_viewBox:
		case OP_ATOM_preserveAspectRatio:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_GRADIENT_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_gradientUnits:
		case OP_ATOM_gradientTransform:
		case OP_ATOM_spreadMethod:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_IMAGE_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_x:
		case OP_ATOM_y:
		case OP_ATOM_width:
		case OP_ATOM_height:
		case OP_ATOM_preserveAspectRatio:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_LOCATABLE))
	{
		switch(property_name)
		{
		case OP_ATOM_nearestViewportElement:
		case OP_ATOM_farthestViewportElement:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_MASK_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_maskUnits:
		case OP_ATOM_maskContentUnits:
		case OP_ATOM_width:
		case OP_ATOM_height:
		case OP_ATOM_x:
		case OP_ATOM_y:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_MARKER_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_refX:
		case OP_ATOM_refY:
		case OP_ATOM_markerWidth:
		case OP_ATOM_markerHeight:
		case OP_ATOM_markerUnits:
		case OP_ATOM_orientType:
		case OP_ATOM_orientAngle:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_PATH_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_pathLength:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_RECT_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_width:
		case OP_ATOM_height:
		case OP_ATOM_x:
		case OP_ATOM_y:
		case OP_ATOM_rx:
		case OP_ATOM_ry:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_LINEARGRADIENT_ELEMENT) ||
		ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_LINE_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_x1:
		case OP_ATOM_x2:
		case OP_ATOM_y1:
		case OP_ATOM_y2:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_PATTERN_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_patternUnits:
		case OP_ATOM_patternContentUnits:
		case OP_ATOM_patternTransform:
		case OP_ATOM_width:
		case OP_ATOM_height:
		case OP_ATOM_x:
		case OP_ATOM_y:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_RADIALGRADIENT_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_cx:
		case OP_ATOM_cy:
		case OP_ATOM_fx:
		case OP_ATOM_fy:
		case OP_ATOM_r:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_SCRIPT_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_type:
		{
// 			 DOMGetString(value, this_element->DOMGetAttribute(GetEnvironment(),
// 																	 UNI_L("type") /* XMLA_BASE */,
// 																	 NS_IDX_SVG));
		}
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_STOP_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_offset:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_STYLABLE))
	{
		switch(property_name)
		{
		case OP_ATOM_className:
		case OP_ATOM_style:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_STYLE_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_xmlspace:
		case OP_ATOM_type:
		case OP_ATOM_media:
		case OP_ATOM_title:
			return PUT_READ_ONLY;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_TESTS))
	{
		switch(property_name)
		{
		case OP_ATOM_requiredFeatures:
		case OP_ATOM_requiredExtensions:
		case OP_ATOM_systemLanguage:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_TEXTCONTENT_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_textLength:
		case OP_ATOM_lengthAdjust:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_TEXTPOSITIONING_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_x:
		case OP_ATOM_y:
		case OP_ATOM_dx:
		case OP_ATOM_dy:
		case OP_ATOM_rotate:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_TEXTPATH_ELEMENT))
	{
		switch(property_name)
		{
		case OP_ATOM_startOffset:
		case OP_ATOM_method:
		case OP_ATOM_spacing:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_TRANSFORMABLE))
	{
		switch(property_name)
		{
		case OP_ATOM_transform:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_URI_REFERENCE))
	{
		switch(property_name)
		{
		case OP_ATOM_href:
			return PUT_READ_ONLY;
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_ZOOM_AND_PAN))
	{
		switch (property_name)
		{
		case OP_ATOM_zoomAndPan:
		{
			OP_ASSERT(value != NULL);
			SVGDOM::SVGZoomAndPanType zap = (SVGDOM::SVGZoomAndPanType)(int)value->value.number;
			OpStatus::Ignore(SVGDOM::AccessZoomAndPan(this_element,
													  GetEnvironment()->GetFramesDocument(),
													  zap, TRUE /* write */));
			return PUT_SUCCESS;
		}
		default:
			break;
		}
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_USE_ELEMENT))
	{
		switch (property_name)
		{
		case OP_ATOM_x:
		case OP_ATOM_y:
		case OP_ATOM_width:
		case OP_ATOM_height:
		case OP_ATOM_instanceRoot:
		case OP_ATOM_animatedInstanceRoot:
			return PUT_READ_ONLY;
			break;
		default:
			break;
		}
	}
#endif // SVG_FULL_11
#ifdef SVG_TINY_12
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_TIMED_ELEMENT))
	{
		switch (property_name)
		{
			case OP_ATOM_isPaused:
				return PUT_READ_ONLY;
		}
	}
#endif // SVG_TINY_12

#ifdef SVG_FULL_11
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED))
	{
		switch (property_name)
		{
			case OP_ATOM_externalResourcesRequired:
				return PUT_READ_ONLY;
		}
	}
#endif // SVG_FULL_11

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_SVG_ELEMENT))
	{
		switch(property_name)
		{
			case OP_ATOM_currentFps:
				return PUT_READ_ONLY;

			case OP_ATOM_targetFps:
				{
					if (value->type == VALUE_NUMBER)
					{
						if(op_isnan(value->value.number) || op_isinf(value->value.number))
							return DOM_PUTNAME_DOMEXCEPTION(NOT_SUPPORTED_ERR);

						if (FramesDocument *frames_doc = GetEnvironment()->GetFramesDocument())
						{
							SVGImage* svg_img = g_svg_manager->GetSVGImage(frames_doc->GetLogicalDocument(), GetThisElement());
							if(svg_img)
							{
								svg_img->SetTargetFPS(static_cast<UINT32>(value->value.number));
								return PUT_SUCCESS;
							}
						}
						return PUT_FAILED;
					}
					return PUT_NEEDS_NUMBER;
				}
				break;

#ifdef SVG_FULL_11
			case OP_ATOM_x:
			case OP_ATOM_y:
			case OP_ATOM_width:
			case OP_ATOM_height:
			case OP_ATOM_pixelUnitToMillimeterX:
			case OP_ATOM_pixelUnitToMillimeterY:
			case OP_ATOM_screenPixelToMillimeterX:
			case OP_ATOM_screenPixelToMillimeterY:
			case OP_ATOM_viewport:
#endif // SVG_FULL_11
#if defined(SVG_FULL_11) || defined(SVG_TINY_12)
			case OP_ATOM_currentTranslate:
				return PUT_READ_ONLY;
#endif // SVG_FULL_11 || SVG_TINY_12

#ifdef SVG_TINY_12
			case OP_ATOM_currentRotate:
			{
				if (value->type == VALUE_NUMBER)
				{
					if (OpStatus::IsMemoryError(SVGDOM::SetCurrentRotate(GetThisElement(), GetEnvironment()->GetFramesDocument(), value->value.number)))
						return PUT_NO_MEMORY;
					return PUT_SUCCESS;
				}
				return PUT_NEEDS_NUMBER;
			}
			break;
#endif // SVG_TINY_12
#if defined(SVG_FULL_11) || defined(SVG_TINY_12)
			case OP_ATOM_currentScale:
			{
				if (value->type == VALUE_NUMBER)
				{
					if (OpStatus::IsMemoryError(SVGDOM::SetCurrentScale(GetThisElement(), GetEnvironment()->GetFramesDocument(), value->value.number)))
						return PUT_NO_MEMORY;
					return PUT_SUCCESS;
				}
				return PUT_NEEDS_NUMBER;
			}
			break;
#endif // SVG_FULL_11 || SVG_TINY_12
		}
	}

	switch (property_name)
	{
	case OP_ATOM_id:
	case OP_ATOM_xmlbase:
	{
		return SetStringAttribute(property_name, value, origining_runtime);
	}
	break;
#ifdef SVG_FULL_11
	case OP_ATOM_ownerSVGElement:
	case OP_ATOM_viewportElement:
	{
		return PUT_READ_ONLY;
	}
#endif // SVG_FULL_11

	case OP_ATOM_classList:
		return PUT_SUCCESS;
#ifdef DRAG_SUPPORT
	case OP_ATOM_dropzone:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		PUT_FAILED_IF_ERROR(this_element->DOMSetAttribute(GetEnvironment(), DOM_AtomToSvgAttribute(property_name), NULL, NS_IDX_DEFAULT, value->value.string, value->string_length, FALSE));
		return PUT_SUCCESS;

	case OP_ATOM_draggable:
	{
		if (value->type != VALUE_BOOLEAN)
			return PUT_NEEDS_BOOLEAN;
		const uni_char* attr_val = value->value.boolean ? UNI_L("true") : UNI_L("false");
		PUT_FAILED_IF_ERROR(this_element->DOMSetAttribute(GetEnvironment(), DOM_AtomToSvgAttribute(property_name), NULL, NS_IDX_DEFAULT, attr_val, uni_strlen(attr_val), FALSE));
		return PUT_SUCCESS;
	}
#endif // DRAG_SUPPORT
	default:
		break;
	}

	return DOM_Element::PutName(property_name, value, origining_runtime);
}


/* virtual */ void
DOM_SVGElement::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	DOM_Element::DOMChangeRuntime(new_runtime);

	for (int i=0; i<object_store->GetCount(); i++)
	{
		DOM_Object* obj = object_store->Get(i);
		if (obj)
			obj->DOMChangeRuntime(new_runtime);
	}
}

/* static */ void
DOM_SVGElement::ConstructDOMImplementationSVGElementObjectL(ES_Object *element, DOM_SVGElementInterface ifs, DOM_Runtime *runtime)
{
#ifdef SVG_FULL_11
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_TEXTCONTENT_ELEMENT))
	{
		DOM_Object::PutNumericConstantL(element, "LENGTHADJUST_UNKNOWN", SVGDOM::SVG_DOM_TEXTCONTENT_LENGTHADJUST_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(element, "LENGTHADJUST_SPACING", SVGDOM::SVG_DOM_TEXTCONTENT_LENGTHADJUST_SPACING, runtime);
		DOM_Object::PutNumericConstantL(element, "LENGTHADJUST_SPACINGANDGLYPHS", SVGDOM::SVG_DOM_TEXTCONTENT_LENGTHADJUST_SPACINGANDGLYPHS, runtime);
	}
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_TEXTPATH_ELEMENT))
	{
		DOM_Object::PutNumericConstantL(element, "TEXTPATH_METHODTYPE_UNKNOWN", SVGDOM::SVG_DOM_TEXTPATH_METHODTYPE_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(element, "TEXTPATH_METHODTYPE_ALIGN", SVGDOM::SVG_DOM_TEXTPATH_METHODTYPE_ALIGN, runtime);
		DOM_Object::PutNumericConstantL(element, "TEXTPATH_METHODTYPE_STRETCH", SVGDOM::SVG_DOM_TEXTPATH_METHODTYPE_STRETCH, runtime);
		DOM_Object::PutNumericConstantL(element, "TEXTPATH_SPACINGTYPE_UNKNOWN", SVGDOM::SVG_DOM_TEXTPATH_SPACINGTYPE_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(element, "TEXTPATH_SPACINGTYPE_AUTO", SVGDOM::SVG_DOM_TEXTPATH_SPACINGTYPE_AUTO, runtime);
		DOM_Object::PutNumericConstantL(element, "TEXTPATH_SPACINGTYPE_EXACT", SVGDOM::SVG_DOM_TEXTPATH_SPACINGTYPE_EXACT, runtime);
	}
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_UNIT_TYPES))
	{
		DOM_Object::PutNumericConstantL(element, "SVG_UNIT_TYPE_UNKNOWN", SVGDOM::SVG_DOM_SVG_UNIT_TYPE_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_UNIT_TYPE_USERSPACEONUSE", SVGDOM::SVG_DOM_SVG_UNIT_TYPE_USERSPACEONUSE, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_UNIT_TYPE_OBJECTBOUNDINGBOX", SVGDOM::SVG_DOM_SVG_UNIT_TYPE_OBJECTBOUNDINGBOX, runtime);
	}
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_GRADIENT_ELEMENT))
	{
		DOM_Object::PutNumericConstantL(element, "SVG_SPREADMETHOD_UNKNOWN", SVGDOM::SVG_DOM_SVG_SPREADMETHOD_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_SPREADMETHOD_PAD", SVGDOM::SVG_DOM_SVG_SPREADMETHOD_PAD, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_SPREADMETHOD_REFLECT", SVGDOM::SVG_DOM_SVG_SPREADMETHOD_REFLECT, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_SPREADMETHOD_REPEAT", SVGDOM::SVG_DOM_SVG_SPREADMETHOD_REPEAT, runtime);
	}
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEBLEND_ELEMENT))
	{
		DOM_Object::PutNumericConstantL(element, "SVG_FEBLEND_MODE_UNKNOWN", SVGDOM::SVG_DOM_SVG_FEBLEND_MODE_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FEBLEND_MODE_NORMAL", SVGDOM::SVG_DOM_SVG_FEBLEND_MODE_NORMAL, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FEBLEND_MODE_MULTIPLY", SVGDOM::SVG_DOM_SVG_FEBLEND_MODE_MULTIPLY, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FEBLEND_MODE_SCREEN", SVGDOM::SVG_DOM_SVG_FEBLEND_MODE_SCREEN, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FEBLEND_MODE_DARKEN", SVGDOM::SVG_DOM_SVG_FEBLEND_MODE_DARKEN, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FEBLEND_MODE_LIGHTEN", SVGDOM::SVG_DOM_SVG_FEBLEND_MODE_LIGHTEN, runtime);
	}
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FECOLORMATRIX_ELEMENT))
	{
		DOM_Object::PutNumericConstantL(element, "SVG_FECOLORMATRIX_TYPE_UNKNOWN", SVGDOM::SVG_DOM_SVG_FECOLORMATRIX_TYPE_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FECOLORMATRIX_TYPE_MATRIX", SVGDOM::SVG_DOM_SVG_FECOLORMATRIX_TYPE_MATRIX, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FECOLORMATRIX_TYPE_SATURATE", SVGDOM::SVG_DOM_SVG_FECOLORMATRIX_TYPE_SATURATE, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FECOLORMATRIX_TYPE_HUEROTATE", SVGDOM::SVG_DOM_SVG_FECOLORMATRIX_TYPE_HUEROTATE, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FECOLORMATRIX_TYPE_LUMINANCETOALPHA", SVGDOM::SVG_DOM_SVG_FECOLORMATRIX_TYPE_LUMINANCETOALPHA, runtime);
	}
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_COMPONENT_TRANSFER_FUNCTION_ELEMENT))
	{
		DOM_Object::PutNumericConstantL(element, "SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN", SVGDOM::SVG_DOM_SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY", SVGDOM::SVG_DOM_SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FECOMPONENTTRANSFER_TYPE_TABLE", SVGDOM::SVG_DOM_SVG_FECOMPONENTTRANSFER_TYPE_TABLE, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE", SVGDOM::SVG_DOM_SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FECOMPONENTTRANSFER_TYPE_LINEAR", SVGDOM::SVG_DOM_SVG_FECOMPONENTTRANSFER_TYPE_LINEAR, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FECOMPONENTTRANSFER_TYPE_GAMMA", SVGDOM::SVG_DOM_SVG_FECOMPONENTTRANSFER_TYPE_GAMMA, runtime);
	}
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FECOMPOSITE_ELEMENT))
	{
		DOM_Object::PutNumericConstantL(element, "SVG_FECOMPOSITE_OPERATOR_UNKNOWN", SVGDOM::SVG_DOM_SVG_FECOMPOSITE_OPERATOR_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FECOMPOSITE_OPERATOR_OVER", SVGDOM::SVG_DOM_SVG_FECOMPOSITE_OPERATOR_OVER, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FECOMPOSITE_OPERATOR_IN", SVGDOM::SVG_DOM_SVG_FECOMPOSITE_OPERATOR_IN, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FECOMPOSITE_OPERATOR_OUT", SVGDOM::SVG_DOM_SVG_FECOMPOSITE_OPERATOR_OUT, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FECOMPOSITE_OPERATOR_ATOP", SVGDOM::SVG_DOM_SVG_FECOMPOSITE_OPERATOR_ATOP, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FECOMPOSITE_OPERATOR_XOR", SVGDOM::SVG_DOM_SVG_FECOMPOSITE_OPERATOR_XOR, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_FECOMPOSITE_OPERATOR_ARITHMETIC", SVGDOM::SVG_DOM_SVG_FECOMPOSITE_OPERATOR_ARITHMETIC, runtime);
	}
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FECONVOLVEMATRIX_ELEMENT))
	{
		DOM_Object::PutNumericConstantL(element, "SVG_EDGEMODE_UNKNOWN", SVGDOM::SVG_DOM_SVG_EDGEMODE_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_EDGEMODE_DUPLICATE", SVGDOM::SVG_DOM_SVG_EDGEMODE_DUPLICATE, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_EDGEMODE_WRAP", SVGDOM::SVG_DOM_SVG_EDGEMODE_WRAP, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_EDGEMODE_NONE", SVGDOM::SVG_DOM_SVG_EDGEMODE_NONE, runtime);
	}
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEDISPLACEMENTMAP_ELEMENT))
	{
		DOM_Object::PutNumericConstantL(element, "SVG_CHANNEL_UNKNOWN", SVGDOM::SVG_DOM_SVG_CHANNEL_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_CHANNEL_R", SVGDOM::SVG_DOM_SVG_CHANNEL_R, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_CHANNEL_G", SVGDOM::SVG_DOM_SVG_CHANNEL_G, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_CHANNEL_B", SVGDOM::SVG_DOM_SVG_CHANNEL_B, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_CHANNEL_A", SVGDOM::SVG_DOM_SVG_CHANNEL_A, runtime);
	}
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEMORPHOLOGY_ELEMENT))
	{
		DOM_Object::PutNumericConstantL(element, "SVG_MORPHOLOGY_OPERATOR_UNKNOWN", SVGDOM::SVG_DOM_SVG_MORPHOLOGY_OPERATOR_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_MORPHOLOGY_OPERATOR_ERODE", SVGDOM::SVG_DOM_SVG_MORPHOLOGY_OPERATOR_ERODE, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_MORPHOLOGY_OPERATOR_DILATE", SVGDOM::SVG_DOM_SVG_MORPHOLOGY_OPERATOR_DILATE, runtime);
	}
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FETURBULENCE_ELEMENT))
	{
		DOM_Object::PutNumericConstantL(element, "SVG_TURBULENCE_TYPE_UNKNOWN", SVGDOM::SVG_DOM_SVG_TURBULENCE_TYPE_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_TURBULENCE_TYPE_FRACTALNOISE", SVGDOM::SVG_DOM_SVG_TURBULENCE_TYPE_FRACTALNOISE, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_TURBULENCE_TYPE_TURBULENCE", SVGDOM::SVG_DOM_SVG_TURBULENCE_TYPE_TURBULENCE, runtime);

		DOM_Object::PutNumericConstantL(element, "SVG_STITCHTYPE_UNKNOWN", SVGDOM::SVG_DOM_SVG_STITCHTYPE_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_STITCHTYPE_STITCH", SVGDOM::SVG_DOM_SVG_STITCHTYPE_STITCH, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_STITCHTYPE_NOSTITCH", SVGDOM::SVG_DOM_SVG_STITCHTYPE_NOSTITCH, runtime);
	}
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_ZOOM_AND_PAN))
	{
		DOM_Object::PutNumericConstantL(element, "SVG_ZOOMANDPAN_UNKNOWN", SVGDOM::SVG_DOM_SVG_ZOOMANDPAN_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_ZOOMANDPAN_DISABLE", SVGDOM::SVG_DOM_SVG_ZOOMANDPAN_DISABLE, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_ZOOMANDPAN_MAGNIFY", SVGDOM::SVG_DOM_SVG_ZOOMANDPAN_MAGNIFY, runtime);
	}
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_MARKER_ELEMENT))
	{
		DOM_Object::PutNumericConstantL(element, "SVG_MARKERUNITS_UNKNOWN", SVGDOM::SVG_DOM_SVG_MARKERUNITS_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_MARKERUNITS_USERSPACEONUSE", SVGDOM::SVG_DOM_SVG_MARKERUNITS_USERSPACEONUSE, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_MARKERUNITS_STROKEWIDTH", SVGDOM::SVG_DOM_SVG_MARKERUNITS_STROKEWIDTH, runtime);

		DOM_Object::PutNumericConstantL(element, "SVG_MARKER_ORIENT_UNKNOWN", SVGDOM::SVG_DOM_SVG_MARKER_ORIENT_UNKNOWN, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_MARKER_ORIENT_AUTO", SVGDOM::SVG_DOM_SVG_MARKER_ORIENT_AUTO, runtime);
		DOM_Object::PutNumericConstantL(element, "SVG_MARKER_ORIENT_ANGLE", SVGDOM::SVG_DOM_SVG_MARKER_ORIENT_ANGLE, runtime);
	}
#endif // SVG_FULL_11
#ifdef SVG_TINY_12
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_SVG_ELEMENT)) // SVGT12 uDOM
	{
		DOM_Object::PutNumericConstantL(element, "NAV_AUTO", SVGDOM::SVG_DOM_NAV_AUTO, runtime);
		DOM_Object::PutNumericConstantL(element, "NAV_NEXT", SVGDOM::SVG_DOM_NAV_NEXT, runtime);
		DOM_Object::PutNumericConstantL(element, "NAV_PREV", SVGDOM::SVG_DOM_NAV_PREV, runtime);
		DOM_Object::PutNumericConstantL(element, "NAV_UP", SVGDOM::SVG_DOM_NAV_UP, runtime);
		DOM_Object::PutNumericConstantL(element, "NAV_UP_RIGHT", SVGDOM::SVG_DOM_NAV_UP_RIGHT, runtime);
		DOM_Object::PutNumericConstantL(element, "NAV_RIGHT", SVGDOM::SVG_DOM_NAV_RIGHT, runtime);
		DOM_Object::PutNumericConstantL(element, "NAV_DOWN_RIGHT", SVGDOM::SVG_DOM_NAV_DOWN_RIGHT, runtime);
		DOM_Object::PutNumericConstantL(element, "NAV_DOWN", SVGDOM::SVG_DOM_NAV_DOWN, runtime);
		DOM_Object::PutNumericConstantL(element, "NAV_DOWN_LEFT", SVGDOM::SVG_DOM_NAV_DOWN_LEFT, runtime);
		DOM_Object::PutNumericConstantL(element, "NAV_LEFT", SVGDOM::SVG_DOM_NAV_LEFT, runtime);
		DOM_Object::PutNumericConstantL(element, "NAV_UP_LEFT", SVGDOM::SVG_DOM_NAV_UP_LEFT, runtime);
	}
#endif // SVG_TINY_12
}

#ifdef SVG_FULL_11
/* static */ int
DOM_SVGElement::getNumberOfChars(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	UINT32 numChars;
	OP_STATUS status = SVGDOM::GetNumberOfChars(elm->GetThisElement(), elm->GetEnvironment()->GetFramesDocument(), numChars);

	if (OpStatus::IsSuccess(status))
	{
		DOMSetNumber(return_value, numChars);
		return ES_VALUE;
	}
	else if (OpStatus::IsMemoryError(status))
	{
		return ES_NO_MEMORY;
	}

	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::getComputedTextLength(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	double len;
	OP_STATUS status = SVGDOM::GetComputedTextLength(elm->GetThisElement(), elm->GetEnvironment()->GetFramesDocument(), len);

	if (OpStatus::IsSuccess(status))
	{
		DOMSetNumber(return_value, len);
		return ES_VALUE;
	}
	else if (OpStatus::IsMemoryError(status))
	{
		return ES_NO_MEMORY;
	}
	else if(OpStatus::IsError(status))
	{
		if(status == OpStatus::ERR_OUT_OF_RANGE)
		{
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
		}
	}

	DOMSetNumber(return_value, 0);
	return ES_VALUE;
}

/* static */ int
DOM_SVGElement::getSubStringLength(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("nn");
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	if(argv[0].value.number < 0 || argv[1].value.number < 0)
	{
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
	}

	double len;
	OP_STATUS status = SVGDOM::GetSubStringLength(elm->GetThisElement(), elm->GetEnvironment()->GetFramesDocument(), static_cast<UINT32>(argv[0].value.number), static_cast<UINT32>(argv[1].value.number), len);

	if (OpStatus::IsSuccess(status))
	{
		DOMSetNumber(return_value, len);
		return ES_VALUE;
	}
	else if (OpStatus::IsMemoryError(status))
	{
		return ES_NO_MEMORY;
	}
	else if(OpStatus::IsError(status))
	{
		if(status == OpStatus::ERR_OUT_OF_RANGE)
		{
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
		}
	}

	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::getStartPositionOfChar(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("n");
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	if(argv[0].value.number < 0)
	{
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
	}

	SVGDOMPoint* pos;
	OP_STATUS status = SVGDOM::GetStartPositionOfChar(elm->GetThisElement(), elm->GetEnvironment()->GetFramesDocument(), static_cast<UINT32>(argv[0].value.number), pos);

	if (OpStatus::IsSuccess(status))
	{
		DOM_SVGObject* dom_point;
		if (OpStatus::IsMemoryError(DOM_SVGObject::Make(dom_point, pos, DOM_SVGLocation(),
											 elm->GetEnvironment())))
		{
			OP_DELETE(pos);
			return ES_NO_MEMORY;
		}
		DOMSetObject(return_value, dom_point);
		return ES_VALUE;
	}
	else if (OpStatus::IsMemoryError(status))
	{
		return ES_NO_MEMORY;
	}
	else if(OpStatus::IsError(status))
	{
		if(status == OpStatus::ERR_OUT_OF_RANGE)
		{
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
		}
	}

	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::getEndPositionOfChar(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("n");
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	if(argv[0].value.number < 0)
	{
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
	}

	SVGDOMPoint* pos;
	OP_STATUS status = SVGDOM::GetEndPositionOfChar(elm->GetThisElement(), elm->GetEnvironment()->GetFramesDocument(), static_cast<UINT32>(argv[0].value.number), pos);

	if (OpStatus::IsSuccess(status))
	{
		DOM_SVGObject* dom_point;
		if (OpStatus::IsMemoryError(DOM_SVGObject::Make(dom_point, pos, DOM_SVGLocation(),
											 elm->GetEnvironment())))
		{
			OP_DELETE(pos);
			return ES_NO_MEMORY;
		}
		DOMSetObject(return_value, dom_point);
		return ES_VALUE;
	}
	else if (OpStatus::IsMemoryError(status))
	{
		return ES_NO_MEMORY;
	}
	else if(OpStatus::IsError(status))
	{
		if(status == OpStatus::ERR_OUT_OF_RANGE)
		{
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
		}
	}

	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::getExtentOfChar(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("n");
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	if(argv[0].value.number < 0)
	{
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
	}

	SVGDOMRect* extent;
	OP_STATUS status = SVGDOM::GetExtentOfChar(elm->GetThisElement(), elm->GetEnvironment()->GetFramesDocument(), static_cast<UINT32>(argv[0].value.number), extent);

	if (OpStatus::IsSuccess(status))
	{
		DOM_SVGObject* dom_rect;
		if (OpStatus::IsMemoryError(DOM_SVGObject::Make(dom_rect, extent, DOM_SVGLocation(),
											  elm->GetEnvironment())))
		{
			OP_DELETE(extent);
			return ES_NO_MEMORY;
		}
		DOMSetObject(return_value, dom_rect);
		return ES_VALUE;
	}
	else if (OpStatus::IsMemoryError(status))
	{
		return ES_NO_MEMORY;
	}
	else if(OpStatus::IsError(status))
	{
		if(status == OpStatus::ERR_OUT_OF_RANGE)
		{
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
		}
	}

	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::getRotationOfChar(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("n");
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	if(argv[0].value.number < 0)
	{
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
	}

	double rotation;
	OP_STATUS status = SVGDOM::GetRotationOfChar(elm->GetThisElement(), elm->GetEnvironment()->GetFramesDocument(), static_cast<UINT32>(argv[0].value.number), rotation);

	if (OpStatus::IsSuccess(status))
	{
		DOMSetNumber(return_value, rotation);
		return ES_VALUE;
	}
	else if (OpStatus::IsMemoryError(status))
	{
		return ES_NO_MEMORY;
	}
	else if(OpStatus::IsError(status))
	{
		if(status == OpStatus::ERR_OUT_OF_RANGE)
		{
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
		}
	}

	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::getCharNumAtPosition(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("o");
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);
	DOM_ARGUMENT_OBJECT(pos, 0, DOM_TYPE_SVG_OBJECT, DOM_SVGObject);

	SVGDOMItem* svg_item = pos->GetSVGDOMItem();
	if (!svg_item->IsA(SVG_DOM_ITEMTYPE_POINT))
		return ES_FAILED;

	long index;
	OP_STATUS status = SVGDOM::GetCharNumAtPosition(elm->GetThisElement(), elm->GetEnvironment()->GetFramesDocument(), (SVGDOMPoint*)svg_item, index);

	if (OpStatus::IsSuccess(status))
	{
		DOMSetNumber(return_value, index);
		return ES_VALUE;
	}
	else if (OpStatus::IsMemoryError(status))
	{
		return ES_NO_MEMORY;
	}

	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::selectSubString(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("nn");
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	if(argv[0].value.number < 0 || argv[1].value.number < 0)
	{
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
	}

	OP_STATUS status = SVGDOM::SelectSubString(elm->GetThisElement(), elm->GetEnvironment()->GetFramesDocument(), static_cast<UINT32>(argv[0].value.number), static_cast<UINT32>(argv[1].value.number));

	if (OpStatus::IsMemoryError(status))
	{
		return ES_NO_MEMORY;
	}
	else if(OpStatus::IsError(status))
	{
		if(status == OpStatus::ERR_OUT_OF_RANGE)
		{
			return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);
		}
	}

	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::getTotalLength(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	double length;
	OP_STATUS status = SVGDOM::GetTotalLength(elm->GetThisElement(), elm->GetEnvironment()->GetFramesDocument(), length);

	if (OpStatus::IsSuccess(status))
	{
		DOMSetNumber(return_value, length);
		return ES_VALUE;
	}
	else if (OpStatus::IsMemoryError(status))
	{
		return ES_NO_MEMORY;
	}

	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::getPointAtLength(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("n");
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	SVGDOMPoint* point = NULL;
	OP_STATUS status = SVGDOM::GetPointAtLength(elm->GetThisElement(), elm->GetEnvironment()->GetFramesDocument(), argv[0].value.number, point);

	if (OpStatus::IsSuccess(status))
	{
		DOM_SVGObject* dom_point;
		if (OpStatus::IsMemoryError(DOM_SVGObject::Make(dom_point, point, DOM_SVGLocation(),
											 elm->GetEnvironment())))
		{
			OP_DELETE(point);
			return ES_NO_MEMORY;
		}

		DOMSetObject(return_value, dom_point);
		return ES_VALUE;
	}
	else if (OpStatus::IsMemoryError(status))
	{
		return ES_NO_MEMORY;
	}

	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::getPathSegAtLength(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("n");
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	UINT32 pathSegIndex;
	CALL_FAILED_IF_ERROR(SVGDOM::GetPathSegAtLength(elm->GetThisElement(), elm->GetEnvironment()->GetFramesDocument(), argv[0].value.number, pathSegIndex));

	DOMSetNumber(return_value, pathSegIndex);
	return ES_VALUE;
}

/* static */ int
DOM_SVGElement::createSVGPathSeg(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_SVGObject* obj = NULL;
	SVGDOMPathSeg* pathseg = NULL;

	if (data >= 0 && data <= 18)
	{
		// Crude mapping to the enum
		// See below for data -> method mapping
		pathseg = SVGDOM::CreateSVGDOMPathSeg(data + 1);
	}
	else
	{
		OP_ASSERT(!"Internal error: Missing data parameter handler");
		return ES_FAILED;
	}

	if (pathseg == NULL)
		return ES_NO_MEMORY;

	OpAutoPtr<SVGDOMItem> item(pathseg);

	// Setup object with correct values
	switch (data)
	{
	default:
	case 0: // createSVGPathSegClosePath()
		break;
	case 1: // createSVGPathSegMovetoAbs(x, y)
	case 2: // createSVGPathSegMovetoRel(x, y)
	case 3: // createSVGPathSegLinetoAbs(x, y)
	case 4: // createSVGPathSegLinetoRel(x, y)
	case 17: // createSVGPathSegCurvetoQuadraticSmoothAbs(x, y)
	case 18: // createSVGPathSegCurvetoQuadraticSmoothRel(x, y)
		DOM_CHECK_ARGUMENTS("nn");
		pathseg->SetX(argv[0].value.number);
		pathseg->SetY(argv[1].value.number);
		break;

	case 5: // createSVGPathSegCurvetoCubicAbs(x, y, x1, y1, x2, y2)
	case 6: // createSVGPathSegCurvetoCubicRel(x, y, x1, y1, x2, y2)
		DOM_CHECK_ARGUMENTS("nnnnnn");
		pathseg->SetX(argv[0].value.number);
		pathseg->SetY(argv[1].value.number);
		pathseg->SetX1(argv[2].value.number);
		pathseg->SetY1(argv[3].value.number);
		pathseg->SetX2(argv[4].value.number);
		pathseg->SetY2(argv[5].value.number);
		break;

	case 7: // createSVGPathSegCurvetoQuadraticAbs(x, y, x1, y1)
	case 8: // createSVGPathSegCurvetoQuadraticRel(x, y, x1, y1)
		DOM_CHECK_ARGUMENTS("nnnn");
		pathseg->SetX(argv[0].value.number);
		pathseg->SetY(argv[1].value.number);
		pathseg->SetX1(argv[2].value.number);
		pathseg->SetY1(argv[3].value.number);
		break;

	case 9: // createSVGPathSegArcAbs(x, y, r1, r2, angle, largeArcFlag, sweepFlag)
	case 10: // createSVGPathSegArcRel(x, y, r1, r2, angle, largeArcFlag, sweepFlag)
		DOM_CHECK_ARGUMENTS("nnnnnbb");
		pathseg->SetX(argv[0].value.number);
		pathseg->SetY(argv[1].value.number);
		pathseg->SetR1(argv[2].value.number);
		pathseg->SetR2(argv[3].value.number);
		pathseg->SetAngle(argv[4].value.number);
		pathseg->SetLargeArcFlag(argv[5].value.boolean);
		pathseg->SetSweepFlag(argv[6].value.boolean);
		break;

	case 11: // createSVGPathSegLinetoHorizontalAbs(x)
	case 12: // createSVGPathSegLinetoHorizontalRel(x)
		DOM_CHECK_ARGUMENTS("n");
		pathseg->SetX(argv[0].value.number);
		break;

	case 13: // createSVGPathSegLinetoVerticalAbs(y)
	case 14: // createSVGPathSegLinetoVerticalRel(y)
		DOM_CHECK_ARGUMENTS("n");
		pathseg->SetY(argv[0].value.number);
		break;

	case 15: // createSVGPathSegCurvetoCubicSmoothAbs(x, y, x2, y2)
	case 16: // createSVGPathSegCurvetoCubicSmoothRel(x, y, x2, y2)
		DOM_CHECK_ARGUMENTS("nnnn");
		pathseg->SetX(argv[0].value.number);
		pathseg->SetY(argv[1].value.number);
		pathseg->SetX2(argv[2].value.number);
		pathseg->SetY2(argv[3].value.number);
		break;
	}

	CALL_FAILED_IF_ERROR(DOM_SVGObject::Make(obj, pathseg, DOM_SVGLocation(),
											 origining_runtime->GetEnvironment()));

	item.release();

	DOM_Object::DOMSetObject(return_value, obj);
	return ES_VALUE;
}
#endif // SVG_FULL_11

/* static */ int
DOM_SVGElement::getBBox(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	SVGDOMRect* rect;
	OP_BOOLEAN status = SVGDOM::GetBoundingBox(elm->GetThisElement(), origining_runtime->GetFramesDocument(),
											   rect, data == 0 ? SVGDOM::SVG_BBOX_NORMAL : SVGDOM::SVG_BBOX_SCREEN);
	if (status == OpBoolean::IS_TRUE)
	{
		DOM_SVGObject *dom_rect;
		OP_STATUS status = DOM_SVGObject::Make(dom_rect, rect, DOM_SVGLocation(),
											  origining_runtime->GetEnvironment());
		if (OpStatus::IsSuccess(status))
		{
			DOMSetObject(return_value, dom_rect);
			return ES_VALUE;
		}
		else if (OpStatus::IsMemoryError(status))
		{
			OP_DELETE(rect);
			return ES_NO_MEMORY;
		}
	}
	else if (OpStatus::IsMemoryError(status))
	{
		return ES_NO_MEMORY;
	}

	return ES_FAILED;
}

#ifdef SVG_FULL_11
/* static */ int
DOM_SVGElement::getCTM(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	SVGDOMMatrix* matrix;
	OP_BOOLEAN status = SVGDOM::GetCTM(elm->GetThisElement(), origining_runtime->GetFramesDocument(),
									   matrix);
	if (status == OpBoolean::IS_TRUE)
	{
		DOM_SVGObject *dom_matrix;
		OP_STATUS status = DOM_SVGObject::Make(dom_matrix, matrix, DOM_SVGLocation(),
											 origining_runtime->GetEnvironment());
		if (OpStatus::IsSuccess(status))
		{
			DOMSetObject(return_value, dom_matrix);
			return ES_VALUE;
		}
		else if (OpStatus::IsMemoryError(status))
		{
			OP_DELETE(matrix);
			return ES_NO_MEMORY;
		}
	}
	else if (OpStatus::IsMemoryError(status))
	{
		return ES_NO_MEMORY;
	}

	return ES_FAILED;
}
#endif // SVG_FULL_11

/* static */ int
DOM_SVGElement::getScreenCTM(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	SVGDOMMatrix* matrix;
	OP_BOOLEAN status = SVGDOM::GetScreenCTM(elm->GetThisElement(), origining_runtime->GetFramesDocument(),
											 matrix);
	if (status == OpBoolean::IS_TRUE)
	{
		DOM_SVGObject *dom_matrix;
		OP_STATUS status = DOM_SVGObject::Make(dom_matrix, matrix, DOM_SVGLocation(),
											  origining_runtime->GetEnvironment());
		if (OpStatus::IsSuccess(status))
		{
			DOMSetObject(return_value, dom_matrix);
			return ES_VALUE;
		}
		else if (OpStatus::IsMemoryError(status))
		{
			OP_DELETE(matrix);
			return ES_NO_MEMORY;
		}
	}
	else if (OpStatus::IsMemoryError(status))
	{
		return ES_NO_MEMORY;
	}

	return ES_FAILED;
}

#ifdef SVG_FULL_11
/* static */ int
DOM_SVGElement::getTransformToElement(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("o");
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);
	DOM_ARGUMENT_OBJECT(target_elem, 0, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	SVGDOMMatrix* matrix;

	OP_BOOLEAN status = SVGDOM::GetTransformToElement(elm->GetThisElement(), origining_runtime->GetFramesDocument(),
													  target_elem->GetThisElement(), matrix);
	if (status == OpBoolean::IS_TRUE)
	{
		DOM_SVGObject *dom_matrix;
		OP_STATUS status = DOM_SVGObject::Make(dom_matrix, matrix, DOM_SVGLocation(),
											 origining_runtime->GetEnvironment());
		if (OpStatus::IsSuccess(status))
		{
			DOMSetObject(return_value, dom_matrix);
			return ES_VALUE;
		}
		else if (OpStatus::IsMemoryError(status))
		{
			OP_DELETE(matrix);
			return ES_NO_MEMORY;
		}
	}
	else if (status == OpBoolean::IS_FALSE) /* Not invertable */
	{
		return DOM_CALL_SVGEXCEPTION(SVG_MATRIX_NOT_INVERTABLE);
	}
	else if (OpStatus::IsMemoryError(status))
	{
		// Throw exception
		return ES_NO_MEMORY;
	}

	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::getPresentationAttribute(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("s");
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	Markup::AttrType attr_name = SVG_Lex::GetAttrType(argv[0].value.string);

	if (attr_name != Markup::HA_XML)
	{
		SVGDOMItem* item;
		OP_BOOLEAN svg_status = SVGDOM::GetPresentationAttribute(elm->GetThisElement(),
																 origining_runtime->GetFramesDocument(),
																 attr_name,
																 item);
		if (svg_status == OpBoolean::IS_TRUE)
		{
			DOM_SVGObject* dom_object;
			OP_STATUS dom_status = DOM_SVGObject::Make(dom_object, item,
													   elm->location.WithAttr(attr_name, NS_SVG),
													   origining_runtime->GetEnvironment());
			if (OpStatus::IsMemoryError(dom_status))
				return ES_NO_MEMORY;

			DOMSetObject(return_value, dom_object);
			return ES_VALUE;
		}
	}
	return ES_FAILED;
}
#endif // SVG_FULL_11

/* static */ int
DOM_SVGElement::beginElement(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	if (data == 0)
	{
		if (OpStatus::IsMemoryError(SVGDOM::BeginElement(elm->GetThisElement(), origining_runtime->GetFramesDocument(), 0)))
			return ES_NO_MEMORY;
	}
	else
	{
		DOM_CHECK_ARGUMENTS("n");
		if (OpStatus::IsMemoryError(SVGDOM::BeginElement(elm->GetThisElement(), origining_runtime->GetFramesDocument(), argv[0].value.number)))
			return ES_NO_MEMORY;
	}

	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::endElement(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	if (data == 0)
	{
		if (OpStatus::IsMemoryError(SVGDOM::EndElement(elm->GetThisElement(), origining_runtime->GetFramesDocument(), 0)))
			return ES_NO_MEMORY;
	}
	else
	{
		DOM_CHECK_ARGUMENTS("n");
		if (OpStatus::IsMemoryError(SVGDOM::EndElement(elm->GetThisElement(), origining_runtime->GetFramesDocument(), argv[0].value.number)))
			return ES_NO_MEMORY;
	}

	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::getCurrentTime(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	if (data == 0)
	{
		DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

		double current_time;
		CALL_FAILED_IF_ERROR(SVGDOM::GetDocumentCurrentTime(elm->GetThisElement(), elm->GetEnvironment()->GetFramesDocument(), current_time));

		DOMSetNumber(return_value, current_time);
		return ES_VALUE;
	}
	else if (data == 1)
	{
		DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

		double current_time;
		if (OpStatus::IsSuccess(SVGDOM::GetAnimationCurrentTime(elm->GetThisElement(),
													   origining_runtime->GetFramesDocument(),
													   current_time)))
		{
			DOMSetNumber(return_value, current_time);
			return ES_VALUE;
		}
	}

	return ES_FAILED;
}

#ifdef SVG_FULL_11
/* static */ int
DOM_SVGElement::getStartTime(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	double start_time;
	if (OpStatus::IsSuccess(SVGDOM::GetAnimationStartTime(elm->GetThisElement(),
												 origining_runtime->GetFramesDocument(),
												 start_time)))
	{
		DOMSetNumber(return_value, start_time);
		return ES_VALUE;
	}
	else
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);
}

/* static */ int
DOM_SVGElement::getSimpleDuration(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	double duration;
	OP_STATUS status = SVGDOM::GetAnimationSimpleDuration(elm->GetThisElement(),
														  origining_runtime->GetFramesDocument(),
														  duration);
	if (status == OpStatus::ERR_NOT_SUPPORTED)
	{
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
	}
	else if (OpStatus::IsSuccess(status))
	{
		DOMSetNumber(return_value, duration);
		return ES_VALUE;
	}
	else
		return ES_FAILED;
}

/* static */ int
DOM_SVGElement::setFilterRes(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("nn");
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	if (OpStatus::IsMemoryError(SVGDOM::SetFilterRes(elm->GetThisElement(), origining_runtime->GetFramesDocument(), argv[0].value.number, argv[1].value.number)))
		return ES_NO_MEMORY;

	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::setStdDeviation(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("nn");
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	if (OpStatus::IsMemoryError(SVGDOM::SetStdDeviation(elm->GetThisElement(), origining_runtime->GetFramesDocument(), argv[0].value.number, argv[1].value.number)))
		return ES_NO_MEMORY;

	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::hasExtension(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("s");
	OP_BOOLEAN has_ext = SVGDOM::HasExtension(argv[0].value.string);
	DOMSetBoolean(return_value, has_ext == OpBoolean::IS_TRUE);
	return ES_VALUE;
}

/* static */ int
DOM_SVGElement::setOrientToAuto(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	if (OpStatus::IsMemoryError(SVGDOM::SetOrient(elm->GetThisElement(), origining_runtime->GetFramesDocument(), NULL)))
		return ES_NO_MEMORY;

	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::setOrientToAngle(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("o");
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);
	DOM_SVG_ARGUMENT_OBJECT(angle, 0, SVG_DOM_ITEMTYPE_ANGLE, SVGDOMAngle);

	if (OpStatus::IsMemoryError(SVGDOM::SetOrient(elm->GetThisElement(), origining_runtime->GetFramesDocument(), angle)))
		return ES_NO_MEMORY;

	return ES_FAILED;
}
#endif // SVG_FULL_11

/************
* DOM_SVGElement
*/

/* static */ int
DOM_SVGElement::createSVGObject(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	OpAutoPtr<SVGDOMItem> item;
	SVGDOMItemType type = SVG_DOM_ITEMTYPE_UNKNOWN;
	switch(data)
	{
#ifdef SVG_FULL_11
	case 0:
		type = SVG_DOM_ITEMTYPE_NUMBER;
		break;
	case 1:
		type = SVG_DOM_ITEMTYPE_LENGTH;
		break;
	case 2:
		type = SVG_DOM_ITEMTYPE_ANGLE;
		break;
#endif // SVG_FULL_11
	case 3:
		type = SVG_DOM_ITEMTYPE_POINT;
		break;
#ifdef SVG_FULL_11
	case 4:
		type = SVG_DOM_ITEMTYPE_MATRIX;
		break;
#endif // SVG_FULL_11
	case 5:
		type = SVG_DOM_ITEMTYPE_RECT;
		break;
#ifdef SVG_FULL_11
	case 6:
		type = SVG_DOM_ITEMTYPE_TRANSFORM;
		break;
#endif // SVG_FULL_11
#ifdef SVG_TINY_12
	case 7:
		type = SVG_DOM_ITEMTYPE_PATH;
		break;

	/** create-methods that have arguments below here **/
	case 8:
		DOM_CHECK_ARGUMENTS("nnnnnn");
		{
			SVGDOMItem* dom_item;
			CALL_FAILED_IF_ERROR(SVGDOM::CreateSVGDOMItem(SVG_DOM_ITEMTYPE_MATRIX, dom_item));
			SVGDOMMatrix* m = static_cast<SVGDOMMatrix*>(dom_item);
			for(int i = 0; i < 6; i++)
			{
				m->SetValue(i, argv[i].value.number);
			}
			item.reset(m);
		}
		break;
	case 9:
		DOM_CHECK_ARGUMENTS("nnn");
		{
			SVGDOMItem* dom_item;
			CALL_FAILED_IF_ERROR(SVGDOM::CreateSVGDOMItem(SVG_DOM_ITEMTYPE_RGBCOLOR, dom_item));
			SVGDOMRGBColor* c = static_cast<SVGDOMRGBColor*>(dom_item);
			c->SetComponent(0, argv[0].value.number);
			c->SetComponent(1, argv[1].value.number);
			c->SetComponent(2, argv[2].value.number);
			item.reset(c);
		}
		break;
#endif // SVG_TINY_12
#ifdef SVG_FULL_11
	case 10:
		DOM_CHECK_ARGUMENTS("o");
		{
			DOM_SVG_ARGUMENT_OBJECT(matrix, 0, SVG_DOM_ITEMTYPE_MATRIX, SVGDOMMatrix);

			SVGDOMItem* dom_item;
			CALL_FAILED_IF_ERROR(SVGDOM::CreateSVGDOMItem(SVG_DOM_ITEMTYPE_TRANSFORM, dom_item));
			SVGDOMTransform* t = static_cast<SVGDOMTransform*>(dom_item);

			t->SetMatrix(matrix);

			item.reset(t);
		}
		break;
#endif // SVG_FULL_11
	default:
		OP_ASSERT(!"Internal error: Missing data parameter handler");
		return ES_FAILED;
	}

	// For all calls that didn't have arguments, create the item here
	if(data < 8)
	{
		SVGDOMItem* dom_item;
		CALL_FAILED_IF_ERROR(SVGDOM::CreateSVGDOMItem(type, dom_item));
		item.reset(dom_item);
	}

	DOM_SVGObject* obj;
	CALL_FAILED_IF_ERROR(DOM_SVGObject::Make(obj, item.get(), DOM_SVGLocation(),
											 origining_runtime->GetEnvironment()));

	item.release();

	DOM_Object::DOMSetObject(return_value, obj);
	return ES_VALUE;
}

#ifdef SVG_FULL_11
/* static */ int
DOM_SVGElement::suspendRedraw(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("n");
	DOMSetNumber(return_value, 1);
	return ES_VALUE;
}

/* static */ int
DOM_SVGElement::unsuspendRedraw(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("n");
	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::unsuspendRedrawAll(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::forceRedraw(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	CALL_FAILED_IF_ERROR(SVGDOM::ForceRedraw(elm->GetThisElement(), elm->GetEnvironment()->GetFramesDocument()));

	return ES_FAILED;
}
#endif // SVG_FULL_11

/* static */ int
DOM_SVGElement::pauseAnimations(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	CALL_FAILED_IF_ERROR(SVGDOM::PauseAnimations(elm->GetThisElement(), elm->GetEnvironment()->GetFramesDocument()));

	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::unpauseAnimations(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	CALL_FAILED_IF_ERROR(SVGDOM::UnpauseAnimations(elm->GetThisElement(), elm->GetEnvironment()->GetFramesDocument()));

	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::animationsPaused(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	BOOL paused;
	CALL_FAILED_IF_ERROR(SVGDOM::AnimationsPaused(elm->GetThisElement(), elm->GetEnvironment()->GetFramesDocument(), paused));

	DOMSetBoolean(return_value, paused);
	return ES_VALUE;
}

/* static */ int
DOM_SVGElement::setCurrentTime(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("n");
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	CALL_FAILED_IF_ERROR(SVGDOM::SetDocumentCurrentTime(elm->GetThisElement(), elm->GetEnvironment()->GetFramesDocument(), argv[0].value.number));

	return ES_FAILED;
}

#ifdef SVG_FULL_11
/* static */ int
DOM_SVGElement::testIntersection(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

	if (data < 2)
	{
		// Element test versions
		DOM_CHECK_ARGUMENTS("oo");
		DOM_ARGUMENT_OBJECT(element, 0, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);
		DOM_SVG_ARGUMENT_OBJECT(rect, 1, SVG_DOM_ITEMTYPE_RECT, SVGDOMRect);

		BOOL element_match = FALSE;

		OP_STATUS status;
		if (data == 0) // checkIntersection
		{
			status = SVGDOM::CheckIntersection(elm->GetThisElement(),
											   elm->GetEnvironment()->GetFramesDocument(),
											   rect, element->GetThisElement(), element_match);
		}
		else // checkEnclosure
		{
			status = SVGDOM::CheckEnclosure(elm->GetThisElement(),
											elm->GetEnvironment()->GetFramesDocument(),
											rect, element->GetThisElement(), element_match);
		}

		CALL_FAILED_IF_ERROR(status);

		DOMSetBoolean(return_value, element_match);
		return ES_VALUE;
	}
	else
	{
		DOM_CHECK_ARGUMENTS("oO");
		OP_ASSERT(data == 2 || data == 3);
		DOM_SVG_ARGUMENT_OBJECT(rect, 0, SVG_DOM_ITEMTYPE_RECT, SVGDOMRect);
		DOM_ARGUMENT_OBJECT(element, 1, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);

		OP_STATUS status;
		OpVector<HTML_Element> selected;

		if (data == 2) // getIntersectionList
		{
			status = SVGDOM::GetIntersectionList(elm->GetThisElement(),
											   elm->GetEnvironment()->GetFramesDocument(),
											   rect, element ? element->GetThisElement() : NULL, selected);
		}
		else // getEnclosureList
		{
			status = SVGDOM::GetEnclosureList(elm->GetThisElement(),
										   elm->GetEnvironment()->GetFramesDocument(),
										   rect, element ? element->GetThisElement() : NULL, selected);
		}

		CALL_FAILED_IF_ERROR(status);

		DOM_StaticNodeList* nodelist;
		CALL_FAILED_IF_ERROR(DOM_StaticNodeList::Make(nodelist, selected, elm->GetOwnerDocument()));

		DOMSetObject(return_value, nodelist);

		return ES_VALUE;
	}
}

/* static */ int
DOM_SVGElement::deselectAll(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	/* TODO: Implement */
	return ES_FAILED;
}
#endif // SVG_FULL_11

#ifdef SVG_TINY_12
/* static */ int
DOM_SVGElement::setFocus(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(svgelm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);
	DOM_CHECK_ARGUMENTS("o");

	HTML_Element* element_to_focus = NULL;

	DOM_HOSTOBJECT_SAFE(domnode, argv[0].value.object, DOM_TYPE_NODE, DOM_Node);
	if (domnode)
	{
		element_to_focus = domnode->GetThisElement();
	}
#ifdef SVG_SUPPORT
	else
	{
		DOM_HOSTOBJECT_SAFE(elminstance, argv[0].value.object, DOM_TYPE_SVG_ELEMENT_INSTANCE, DOM_SVGElementInstance);
		if (elminstance)
		{
			element_to_focus = elminstance->GetThisElement();
		}
	}
#endif // SVG_SUPPORT

	if(!element_to_focus)
	{
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
	}

	DOM_EnvironmentImpl *environment = svgelm->GetEnvironment();

	if (FramesDocument *frames_doc = environment->GetFramesDocument())
		if (HTML_Document* html_doc = frames_doc->GetHtmlDocument())
		{
			HTML_Element *current_element = html_doc->GetFocusedElement();

			if (!frames_doc->GetDocRoot() || !frames_doc->GetDocRoot()->IsAncestorOf(element_to_focus))
				return ES_FAILED;

			if (current_element != element_to_focus)
			{
				CALL_FAILED_IF_ERROR(frames_doc->Reflow(FALSE));

				current_element = current_element ? current_element : html_doc->GetNavigationElement();

				if (element_to_focus->IsFocusable(frames_doc))
				{
					BOOL highlight = html_doc->GetVisualDevice()->GetDrawFocusRect();
					BOOL focused = html_doc->FocusElement(element_to_focus, HTML_Document::FOCUS_ORIGIN_DOM, TRUE, TRUE, highlight);

					/* Defined in domhtml/htmlelem.cpp. */
					extern OP_STATUS SendEvent(DOM_EnvironmentImpl *environment, DOM_EventType event, HTML_Element *target, ES_Thread* interrupt_thread);

					ES_Thread* interrupt_thread = environment->GetDOMRuntime() == origining_runtime ?
						GetCurrentThread(origining_runtime) : NULL;

					if (current_element)
						CALL_FAILED_IF_ERROR(SendEvent(environment, ONBLUR, current_element, interrupt_thread));

					if (focused)
						CALL_FAILED_IF_ERROR(SendEvent(environment, ONFOCUS, element_to_focus, interrupt_thread));
				}
				else
				{
					return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
				}
			}
		}

	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::moveFocus(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	// This is not that far from what is spec:ed, but it doesn't seem to send focusevents properly
	DOM_THIS_OBJECT(svgelm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);
	DOM_CHECK_ARGUMENTS("n");
	DOM_EnvironmentImpl *environment = svgelm->GetEnvironment();
	FramesDocument *frames_doc = environment->GetFramesDocument();

	if (frames_doc)
	{
		int dir = DIR_DOWN;
		int argdir = (int) argv[0].value.number;
		switch(argdir)
		{
			case SVGDOM::SVG_DOM_NAV_DOWN:
			case SVGDOM::SVG_DOM_NAV_AUTO:
				break;
			case SVGDOM::SVG_DOM_NAV_UP:
				dir = DIR_UP;
				break;
			case SVGDOM::SVG_DOM_NAV_RIGHT:
				dir = DIR_RIGHT;
				break;
			case SVGDOM::SVG_DOM_NAV_LEFT:
				dir = DIR_LEFT;
				break;
			case SVGDOM::SVG_DOM_NAV_NEXT:
			case SVGDOM::SVG_DOM_NAV_PREV:
			case SVGDOM::SVG_DOM_NAV_UP_LEFT:
			case SVGDOM::SVG_DOM_NAV_UP_RIGHT:
			case SVGDOM::SVG_DOM_NAV_DOWN_LEFT:
			case SVGDOM::SVG_DOM_NAV_DOWN_RIGHT:
			default:
				return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
		}

#ifdef _SPAT_NAV_SUPPORT_
		OpSnHandler* sn_handler = frames_doc->GetWindow()->GetSnHandlerConstructIfNeeded();
		if (sn_handler)
		{
			BOOL res = sn_handler->MarkNextItemInDirection(dir);
			if (!res)
				return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);
		}
#else
		(void)dir;
#endif // _SPAT_NAV_SUPPORT_
	}
	return ES_FAILED;
}

/* static */ int
DOM_SVGElement::getCurrentFocusedObject(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(svgelm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);
	DOM_EnvironmentImpl *environment = svgelm->GetEnvironment();

	if (FramesDocument *frames_doc = environment->GetFramesDocument())
		if (HTML_Document* html_doc = frames_doc->GetHtmlDocument())
		{
			if (html_doc->GetFocusedElement())
				return svgelm->DOMSetElement(return_value, html_doc->GetFocusedElement());
			else
			{
				CALL_FAILED_IF_ERROR(environment->ConstructDocumentNode());
				DOMSetObject(return_value, environment->GetDocument());
				return ES_VALUE;
			}
		}

	return ES_FAILED;
}
#endif // SVG_TINY_12

#ifdef SVG_FULL_11
ES_GetState
DOM_SVGElement::GetFrameEnvironment(ES_Value* value, FrameEnvironmentType type, DOM_Runtime *origining_runtime)
{
	if (!g_svg_manager->AllowFrameForElement(this_element))
		return GET_FAILED;

	DOM_ProxyEnvironment* frame_environment;
	FramesDocument* frame_frames_doc;
	GET_FAILED_IF_ERROR(this_element->DOMGetFrameProxyEnvironment(frame_environment, frame_frames_doc, GetEnvironment()));

	DOM_Object *object = NULL;

	if (frame_environment)
	{
		OP_STATUS status;

		if (type == FRAME_DOCUMENT)
			status = static_cast<DOM_ProxyEnvironmentImpl *>(frame_environment)->GetProxyDocument(object, origining_runtime);
		else
			status = static_cast<DOM_ProxyEnvironmentImpl *>(frame_environment)->GetProxyWindow(object, origining_runtime);

		GET_FAILED_IF_ERROR(status);

		if (frame_frames_doc)
			origining_runtime->GetEnvironment()->AccessedOtherEnvironment(frame_frames_doc);
	}

	DOMSetObject(value, object);
	return GET_SUCCESS;
}

/* static */ int
DOM_SVGElement::getSVGDocument(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domelem, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);
	ES_GetState result = domelem->GetFrameEnvironment(return_value, FRAME_DOCUMENT, (DOM_Runtime *) origining_runtime);
	return ConvertGetNameToCall(result, return_value);
}
#endif // SVG_FULL_11

#ifdef SVG_FULL_11
/* static */ int
DOM_SVGElement::getComputedStyle(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(elm, DOM_TYPE_SVG_ELEMENT, DOM_SVGElement);
	return JS_Window::getComputedStyle(elm->GetEnvironment()->GetWindow(), argv, argc, return_value, origining_runtime);
}
#endif // SVG_FULL_11

/* static */ void
DOM_SVGElement_Prototype::ConstructL(ES_Object *object, DOM_Runtime::SVGElementPrototype prototype, DOM_SVGElementInterface ifs, DOM_Runtime *runtime)
{
	DOM_SVGElement::ConstructDOMImplementationSVGElementObjectL(object, ifs, runtime);

#ifdef SVG_FULL_11
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_TEXTCONTENT_ELEMENT))
	{
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getNumberOfChars, "getNumberOfChars", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getComputedTextLength, "getComputedTextLength", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getSubStringLength, "getSubStringLength", "nn");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getStartPositionOfChar, "getStartPositionOfChar", "n");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getEndPositionOfChar, "getEndPositionOfChar", "n");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getExtentOfChar, "getExtentOfChar", "n");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getRotationOfChar, "getRotationOfChar", "n");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getCharNumAtPosition, "getCharNumAtPosition", "o");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::selectSubString, "selectSubString", "nn");
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_PATH_ELEMENT))
	{
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getTotalLength, "getTotalLength", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getPointAtLength, "getPointAtLength", "n-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getPathSegAtLength, "getPathSegAtLength", "n-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGPathSeg, 0, "createSVGPathSegClosePath", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGPathSeg, 1, "createSVGPathSegMovetoAbs", "nn-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGPathSeg, 2, "createSVGPathSegMovetoRel", "nn-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGPathSeg, 3, "createSVGPathSegLinetoAbs", "nn-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGPathSeg, 4, "createSVGPathSegLinetoRel", "nn-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGPathSeg, 5, "createSVGPathSegCurvetoCubicAbs", "nnnnnn-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGPathSeg, 6, "createSVGPathSegCurvetoCubicRel", "nnnnnn-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGPathSeg, 7, "createSVGPathSegCurvetoQuadraticAbs", "nnnn-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGPathSeg, 8, "createSVGPathSegCurvetoQuadraticRel", "nnnn-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGPathSeg, 9, "createSVGPathSegArcAbs", "nnnnnbb-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGPathSeg, 10, "createSVGPathSegArcRel", "nnnnnbb-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGPathSeg, 11, "createSVGPathSegLinetoHorizontalAbs", "n-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGPathSeg, 12, "createSVGPathSegLinetoHorizontalRel", "n-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGPathSeg, 13, "createSVGPathSegLinetoVerticalAbs", "n-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGPathSeg, 14, "createSVGPathSegLinetoVerticalRel", "n-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGPathSeg, 15, "createSVGPathSegCurvetoCubicSmoothAbs", "nnnn-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGPathSeg, 16, "createSVGPathSegCurvetoCubicSmoothRel", "nnnn-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGPathSeg, 17, "createSVGPathSegCurvetoQuadraticSmoothAbs", "nn-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGPathSeg, 18, "createSVGPathSegCurvetoQuadraticSmoothRel", "nn-");
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_CAN_REFERENCE_DOCUMENT))
	{
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getSVGDocument, "getSVGDocument", NULL);
	}
#endif // SVG_FULL_11

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_LOCATABLE))
	{
#ifdef SVG_FULL_11
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getCTM, "getCTM", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getTransformToElement, "getTransformToElement", "o");
#endif // SVG_FULL_11
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getBBox, 0, "getBBox", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getBBox, 1, "getScreenBBox", NULL); // SVGT12 uDOM
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getScreenCTM, "getScreenCTM", NULL);
	}

#ifdef SVG_FULL_11
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_STYLABLE))
	{
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getPresentationAttribute, "getPresentationAttribute", NULL);
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_ANIMATION_ELEMENT_BASE))
	{
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getStartTime, "getStartTime", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getCurrentTime, 1, "getCurrentTime", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getSimpleDuration, "getSimpleDuration", NULL);
	}
#endif // SVG_FULL_11

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_SMIL_ELEMENT_TIME_CONTROL))
	{
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::beginElement, 0, "beginElement", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::beginElement, 1, "beginElementAt", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::endElement, 0, "endElement", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::endElement, 1, "endElementAt", NULL);
	}

#ifdef SVG_FULL_11
	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FILTER_ELEMENT))
	{
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::setFilterRes, "setFilterRes", NULL);
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_FEGAUSSIANBLUR_ELEMENT))
	{
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::setStdDeviation, "setStdDeviation", NULL);
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_TESTS))
	{
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::hasExtension, "hasExtension", NULL);
	}

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_MARKER_ELEMENT))
	{
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::setOrientToAuto, "setOrientToAuto", NULL);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::setOrientToAngle, "setOrientToAngle", NULL);
	}
#endif // SVG_FULL_11

	if (ifs.HasInterface(DOM_SVGElementInterface::SVG_INTERFACE_SVG_ELEMENT))
	{
#ifdef SVG_FULL_11
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGObject, 0, "createSVGNumber", 0);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGObject, 1, "createSVGLength", 0);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGObject, 2, "createSVGAngle", 0);
#endif // SVG_FULL_11
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGObject, 3, "createSVGPoint", 0);
#ifdef SVG_FULL_11
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGObject, 4, "createSVGMatrix", 0);
#endif // SVG_FULL_11
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGObject, 5, "createSVGRect", 0);
#ifdef SVG_FULL_11
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGObject, 6, "createSVGTransform", 0);
#endif // SVG_FULL_11
#ifdef SVG_TINY_12
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGObject, 7, "createSVGPath", 0); // This is SVGT1.2 uDOM
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGObject, 8, "createSVGMatrixComponents", 0); // This is SVGT1.2 uDOM
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGObject, 9, "createSVGRGBColor", 0);
#endif // SVG_TINY_12
#ifdef SVG_FULL_11
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::createSVGObject, 10, "createSVGTransformFromMatrix", 0);
#endif // SVG_FULL_11
#ifdef SVG_FULL_11
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::suspendRedraw, "suspendRedraw", 0);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::unsuspendRedraw, "unsuspendRedraw", 0);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::unsuspendRedrawAll, "unsuspendRedrawAll", 0);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::forceRedraw, "forceRedraw", 0);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::pauseAnimations, "pauseAnimations", 0);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::unpauseAnimations, "unpauseAnimations", 0);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::animationsPaused, "animationsPaused", 0);
#endif // SVG_FULL_11
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getCurrentTime, 0, "getCurrentTime", 0);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::setCurrentTime, "setCurrentTime", "n");
#ifdef SVG_FULL_11
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::testIntersection, 0, "checkIntersection", 0);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::testIntersection, 1, "checkEnclosure", 0);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::testIntersection, 2, "getIntersectionList", 0);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::testIntersection, 3, "getEnclosureList", 0);
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::deselectAll, "deselectAll", 0);
		DOM_Object::AddFunctionL(object, runtime, DOM_Document::getElementById, 1, "getElementById", "z-");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getComputedStyle, "getComputedStyle", "-s-");
#endif // SVG_FULL_11
#ifdef SVG_TINY_12
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::setFocus, "setFocus", "o");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::moveFocus, "moveFocus", "n");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getCurrentFocusedObject, "getCurrentFocusedObject", "n");
#endif // SVG_TINY_12
	}

#ifdef SVG_TINY_12
	// This is SVG Tiny 1.2 uDOM TraitAccess
	{
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getObjectTrait, 0, "getTrait", "s");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getObjectTrait, 1, "getTraitNS", "Ss");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getObjectTrait, 2, "getFloatTrait", "s");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getObjectTrait, 3, "getMatrixTrait", "s");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getObjectTrait, 4, "getRectTrait", "s");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getObjectTrait, 5, "getPathTrait", "s");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getObjectTrait, 6, "getRGBColorTrait", "s");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getObjectTrait, 7, "getPresentationTrait", "s");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getObjectTrait, 8, "getPresentationTraitNS", "Ss");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getObjectTrait, 9, "getFloatPresentationTrait", "s");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getObjectTrait, 10, "getMatrixPresentationTrait", "s");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getObjectTrait, 11, "getRectPresentationTrait", "s");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getObjectTrait, 12, "getPathPresentationTrait", "s");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::getObjectTrait, 13, "getRGBColorPresentationTrait", "s");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::setObjectTrait, 0, "setTrait", "ss");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::setObjectTrait, 1, "setTraitNS", "Sss");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::setObjectTrait, 2, "setFloatTrait", "sn");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::setObjectTrait, 3, "setMatrixTrait", "so");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::setObjectTrait, 4, "setRectTrait", "so");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::setObjectTrait, 5, "setPathTrait", "so");
		DOM_Object::AddFunctionL(object, runtime, DOM_SVGElement::setObjectTrait, 6, "setRGBColorTrait", "so");
	}
#endif // SVG_TINY_12
}

/* static */ OP_STATUS
DOM_SVGElement_Prototype::Construct(ES_Object *object, DOM_Runtime::SVGElementPrototype prototype, DOM_SVGElementInterface ifs, DOM_Runtime *runtime)
{
	TRAPD(status, ConstructL(object, prototype, ifs, runtime));
	return status;
}

/** DOM_SVGAnimatedValue
 */

#ifdef SVG_FULL_11
/* static */ OP_STATUS
DOM_SVGAnimatedValue::Make(DOM_SVGAnimatedValue *&anim_val,
						   SVGDOMAnimatedValue* svg_val,
						   const char* name,
						   DOM_SVGLocation location,
						   DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(anim_val = OP_NEW(DOM_SVGAnimatedValue, ()), runtime,
													runtime->GetObjectPrototype(), name));
	anim_val->svg_value = svg_val;
	anim_val->location = location;
	return OpStatus::OK;
}

/* virtual */
DOM_SVGAnimatedValue::~DOM_SVGAnimatedValue()
{
	OP_DELETE(svg_value);
}

/* virtual */ void
DOM_SVGAnimatedValue::GCTrace()
{
	GCMark(base_object);
	GCMark(anim_object);
	location.GCTrace();
}

// Variants of GetName/PutName of that use the SVGDOMAnimatedValue
// interface extended with primitive values. See below for the older
// versions of these functions.

/* virtual */ ES_GetState
DOM_SVGAnimatedValue::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_baseVal || property_name == OP_ATOM_animVal)
	{
		if (value)
		{
			BOOL base = (property_name == OP_ATOM_baseVal);

			SVGDOMPrimitiveValue prim;

			DOM_Object*& source = base ? base_object : anim_object;
			if (DOM_SVGLocation::IsValid(source))
			{
				if (source->IsA(DOM_TYPE_SVG_LIST) || source->IsA(DOM_TYPE_SVG_OBJECT))
					prim.type = SVGDOMPrimitiveValue::VALUE_ITEM;
			}
			else
			{
				OP_STATUS status = OpStatus::OK;
				SVGDOMAnimatedValue::Field field = base ? SVGDOMAnimatedValue::FIELD_BASE : SVGDOMAnimatedValue::FIELD_ANIM;
				GET_FAILED_IF_ERROR(svg_value->GetPrimitiveValue(prim, field));

				if (prim.type == SVGDOMPrimitiveValue::VALUE_ITEM)
				{
					SVGDOMItem *item = prim.value.item;
					if (item->IsA(SVG_DOM_ITEMTYPE_LIST))
					{
						DOM_SVGList* list;
						status = DOM_SVGList::Make(list, (SVGDOMList *)item,
												location,
												GetEnvironment());
						source = list;
					}
					else
					{
						DOM_SVGObject* obj;
						status = DOM_SVGObject::Make(obj, item,
													location,
													GetEnvironment());
						source = obj;
					}

					if (OpStatus::IsMemoryError(status))
					{
						OP_DELETE(item);
						return GET_NO_MEMORY;
					}
				}
			}

			if (prim.type == SVGDOMPrimitiveValue::VALUE_NONE)
			{
				DOMSetNull(value);
			}
			else if (prim.type == SVGDOMPrimitiveValue::VALUE_NUMBER)
			{
				DOMSetNumber(value, prim.value.number);
			}
			else if (prim.type == SVGDOMPrimitiveValue::VALUE_BOOLEAN)
			{
				DOMSetBoolean(value, prim.value.boolean);
			}
			else if (prim.type == SVGDOMPrimitiveValue::VALUE_STRING)
			{
				DOMSetString(value, prim.value.str);
			}
			else
			{
				OP_ASSERT(source != NULL);
				DOMSetObject(value, source);
			}
		}
		return GET_SUCCESS;
	}
	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_SVGAnimatedValue::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_baseVal)
	{
		OP_BOOLEAN ret;
		if (svg_value->IsSetByNumber())
		{
			if (value->type != VALUE_NUMBER)
				return PUT_NEEDS_NUMBER;

			ret = svg_value->SetNumber(value->value.number);
		}
		else if (svg_value->IsSetByString())
		{
			if (value->type != VALUE_STRING)
				return PUT_NEEDS_STRING;

			ret = svg_value->SetString(value->value.string, GetLogicalDocument());
		}
		else
		{
			return PUT_READ_ONLY;
		}

		PUT_FAILED_IF_ERROR(ret);
		if (ret == OpBoolean::IS_TRUE)
			location.Invalidate();
		return PUT_SUCCESS;
	}
	else if (property_name == OP_ATOM_animVal)
		return PUT_READ_ONLY;

	return DOM_Object::PutName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_SVGAnimatedValue::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	DOM_Object::DOMChangeRuntime(new_runtime);
	if (base_object)
		base_object->DOMChangeRuntime(new_runtime);
	if (anim_object)
		anim_object->DOMChangeRuntime(new_runtime);
}
#endif // SVG_FULL_11

// Inheritance hierarchy for elements
#define INHERITS(sym) (1U << (unsigned int)(DOM_SVGElementInterface::sym))
#define IS_BASE 0

#define GRAPHICAL_ELEMENT 	\
INHERITS(SVG_INTERFACE_ELEMENT) | \
INHERITS(SVG_INTERFACE_TESTS) | \
INHERITS(SVG_INTERFACE_LANG_SPACE) | \
INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | \
INHERITS(SVG_INTERFACE_STYLABLE) | \
INHERITS(SVG_INTERFACE_TRANSFORMABLE)

#define FILTER_PRIM_COMMON \
INHERITS(SVG_INTERFACE_ELEMENT) | \
INHERITS(SVG_INTERFACE_FILTER_PRIMITIVE_STANDARD_ATTRIBUTES)

/* Retain the same order as in the enumeration in domsvgenum.h::DOM_SVGElementInterface */
CONST_SIMPLE_ARRAY(svg_if_inheritance_table, unsigned int)
	/* Inherited interfaces */
	// SVG_INTERFACE_ANIMATED_PATH_DATA
	CONST_ENTRY(IS_BASE),
	// SVG_INTERFACE_ANIMATED_POINTS
	CONST_ENTRY(IS_BASE),
	// SVG_INTERFACE_COMPONENT_TRANSFER_FUNCTION_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT)),
	/*SVG_INTERFACE_CSS_RULE, */ // css::CSSRule
	// SVG_INTERFACE_ELEMENT
	CONST_ENTRY(IS_BASE), // Element
	// SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED
	CONST_ENTRY(IS_BASE),
	// SVG_INTERFACE_FILTER_PRIMITIVE_STANDARD_ATTRIBUTES
	CONST_ENTRY(INHERITS(SVG_INTERFACE_STYLABLE)),
	// SVG_INTERFACE_FIT_TO_VIEW_BOX
	CONST_ENTRY(IS_BASE),
	// SVG_INTERFACE_GRADIENT_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_URI_REFERENCE) | \
				INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | INHERITS(SVG_INTERFACE_STYLABLE) | \
				INHERITS(SVG_INTERFACE_UNIT_TYPES)),

	// SVG_INTERFACE_LANG_SPACE
	CONST_ENTRY(IS_BASE),
	// SVG_INTERFACE_LOCATABLE
	CONST_ENTRY(IS_BASE),
	/*SVG_INTERFACE_RENDERING_INTENT,*/
	// SVG_INTERFACE_STYLABLE
	CONST_ENTRY(IS_BASE),
	// SVG_INTERFACE_TESTS
	CONST_ENTRY(IS_BASE),
	// SVG_INTERFACE_TEXTCONTENT_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_TESTS) | \
					 INHERITS(SVG_INTERFACE_LANG_SPACE) | \
					 INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | \
					 INHERITS(SVG_INTERFACE_STYLABLE)), //| events::EventTarget

	// SVG_INTERFACE_TEXTPOSITIONING_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_TEXTCONTENT_ELEMENT)),
	// SVG_INTERFACE_TRANSFORMABLE
	CONST_ENTRY(INHERITS(SVG_INTERFACE_LOCATABLE)),
	// SVG_INTERFACE_UNIT_TYPES
	CONST_ENTRY(IS_BASE),
	// SVG_INTERFACE_URI_REFERENCE
	CONST_ENTRY(IS_BASE),
	// SVG_INTERFACE_ZOOM_AND_PAN
	CONST_ENTRY(IS_BASE),
	// SVG_INTERFACE_SMIL_ELEMENT_TIME_CONTROL
	CONST_ENTRY(IS_BASE),
	// SVG_INTERFACE_TIMED_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_SMIL_ELEMENT_TIME_CONTROL)),
	// SVG_INTERFACE_CAN_REFERENCE_DOCUMENT
	CONST_ENTRY(IS_BASE),
	// SVG_INTERFACE_VISUAL_MEDIA_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_TIMED_ELEMENT) | INHERITS(SVG_INTERFACE_URI_REFERENCE) | \
					 INHERITS(SVG_INTERFACE_TESTS) | INHERITS(SVG_INTERFACE_LANG_SPACE) | \
					 INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | \
					 INHERITS(SVG_INTERFACE_STYLABLE) | INHERITS(SVG_INTERFACE_TRANSFORMABLE)), // | events::EventTarget /* 22 */

	/* Unused up to 32 */
	/* 23 */
	CONST_ENTRY(IS_BASE),CONST_ENTRY(IS_BASE),CONST_ENTRY(IS_BASE),
	CONST_ENTRY(IS_BASE), /* 26 */
	CONST_ENTRY(IS_BASE),CONST_ENTRY(IS_BASE),CONST_ENTRY(IS_BASE),
	CONST_ENTRY(IS_BASE), /* 30 */
	CONST_ENTRY(IS_BASE),CONST_ENTRY(IS_BASE), /* 32 */

	// SVG_INTERFACE_ANIMATION_ELEMENT_BASE
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_TESTS) | \
					 INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | \
					 INHERITS(SVG_INTERFACE_SMIL_ELEMENT_TIME_CONTROL) | \
					 INHERITS(SVG_INTERFACE_TIMED_ELEMENT)),

	// SVG_INTERFACE_CIRCLE_ELEMENT
	CONST_ENTRY(GRAPHICAL_ELEMENT), //| events::EventTarget

	// SVG_INTERFACE_CLIP_PATH_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_TESTS) | \
					 INHERITS(SVG_INTERFACE_LANG_SPACE) | \
					 INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | \
					 INHERITS(SVG_INTERFACE_STYLABLE) | INHERITS(SVG_INTERFACE_TRANSFORMABLE) | \
					 INHERITS(SVG_INTERFACE_UNIT_TYPES)),

	// SVG_INTERFACE_DEFS_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_TESTS) | \
					 INHERITS(SVG_INTERFACE_LANG_SPACE) | INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | \
					 INHERITS(SVG_INTERFACE_STYLABLE) | INHERITS(SVG_INTERFACE_TRANSFORMABLE)), //| events::EventTarget

	// SVG_INTERFACE_DESC_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_LANG_SPACE) | \
					 INHERITS(SVG_INTERFACE_STYLABLE)),

	// SVG_INTERFACE_ELLIPSE_ELEMENT
	CONST_ENTRY(GRAPHICAL_ELEMENT), //| events::EventTarget

	// SVG_INTERFACE_FILTER_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_URI_REFERENCE) | \
					 INHERITS(SVG_INTERFACE_LANG_SPACE) | \
					 INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | \
					 INHERITS(SVG_INTERFACE_STYLABLE) | INHERITS(SVG_INTERFACE_UNIT_TYPES)),

	// SVG_INTERFACE_FEBLEND_ELEMENT
	CONST_ENTRY(FILTER_PRIM_COMMON),
	// SVG_INTERFACE_FECOLORMATRIX_ELEMENT
	CONST_ENTRY(FILTER_PRIM_COMMON),
	// SVG_INTERFACE_FECOMPONENTTRANSFER_ELEMENT
	CONST_ENTRY(FILTER_PRIM_COMMON),

	// SVG_INTERFACE_FEFUNCR_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_COMPONENT_TRANSFER_FUNCTION_ELEMENT)),
	// SVG_INTERFACE_FEFUNCG_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_COMPONENT_TRANSFER_FUNCTION_ELEMENT)),
	// SVG_INTERFACE_FEFUNCB_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_COMPONENT_TRANSFER_FUNCTION_ELEMENT)),
	// SVG_INTERFACE_FEFUNCA_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_COMPONENT_TRANSFER_FUNCTION_ELEMENT)),

	// SVG_INTERFACE_FECOMPOSITE_ELEMENT
	CONST_ENTRY(FILTER_PRIM_COMMON),
	// SVG_INTERFACE_FECONVOLVEMATRIX_ELEMENT
	CONST_ENTRY(FILTER_PRIM_COMMON),
	// SVG_INTERFACE_FEDIFFUSELIGHTING_ELEMENT
	CONST_ENTRY(FILTER_PRIM_COMMON),

	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT)), // SVG_INTERFACE_FEDISTANTLIGHT_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT)), // SVG_INTERFACE_FEPOINTLIGHT_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT)), // SVG_INTERFACE_FESPOTLIGHT_ELEMENT

	// SVG_INTERFACE_FEDISPLACEMENTMAP_ELEMENT
	CONST_ENTRY(FILTER_PRIM_COMMON),
	// SVG_INTERFACE_FEFLOOD_ELEMENT
	CONST_ENTRY(FILTER_PRIM_COMMON),
	// SVG_INTERFACE_FEGAUSSIANBLUR_ELEMENT
	CONST_ENTRY(FILTER_PRIM_COMMON),

	// SVG_INTERFACE_FEIMAGE_ELEMENT
	CONST_ENTRY(FILTER_PRIM_COMMON | INHERITS(SVG_INTERFACE_URI_REFERENCE) | \
					 INHERITS(SVG_INTERFACE_LANG_SPACE) | \
					 INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED)),

	// SVG_INTERFACE_FEMERGE_ELEMENT
	CONST_ENTRY(FILTER_PRIM_COMMON),
	// SVG_INTERFACE_FEMERGENODE_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT)),

	// SVG_INTERFACE_FEMORPHOLOGY_ELEMENT
	CONST_ENTRY(FILTER_PRIM_COMMON),
	// SVG_INTERFACE_FEOFFSET
	CONST_ENTRY(FILTER_PRIM_COMMON),
	// SVG_INTERFACE_FESPECULARLIGHTING_ELEMENT
	CONST_ENTRY(FILTER_PRIM_COMMON),
	// SVG_INTERFACE_FETILE_ELEMENT
	CONST_ENTRY(FILTER_PRIM_COMMON),
	// SVG_INTERFACE_FETURBULENCE_ELEMENT
	CONST_ENTRY(FILTER_PRIM_COMMON),

	// SVG_INTERFACE_G_ELEMENT,
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_TESTS) | \
					 INHERITS(SVG_INTERFACE_LANG_SPACE) | INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | \
					 INHERITS(SVG_INTERFACE_STYLABLE) | INHERITS(SVG_INTERFACE_TRANSFORMABLE)), // | events::EventTarget

	// SVG_INTERFACE_IMAGE_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_URI_REFERENCE) | \
					 INHERITS(SVG_INTERFACE_TESTS) | INHERITS(SVG_INTERFACE_LANG_SPACE) | \
					 INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | INHERITS(SVG_INTERFACE_STYLABLE) | \
					 INHERITS(SVG_INTERFACE_TRANSFORMABLE)), //| events::EventTarget

	// SVG_INTERFACE_LINE_ELEMENT
	CONST_ENTRY(GRAPHICAL_ELEMENT), //| events::EventTarget

	// SVG_INTERFACE_LINEARGRADIENT_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_GRADIENT_ELEMENT)),

	// SVG_INTERFACE_MARKER_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_LANG_SPACE) | \
					 INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | \
					 INHERITS(SVG_INTERFACE_STYLABLE) | INHERITS(SVG_INTERFACE_FIT_TO_VIEW_BOX)),

	// SVG_INTERFACE_MASK_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_TESTS) | \
					 INHERITS(SVG_INTERFACE_LANG_SPACE) | \
					 INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | \
					 INHERITS(SVG_INTERFACE_STYLABLE) | INHERITS(SVG_INTERFACE_UNIT_TYPES)),

	// SVG_INTERFACE_RADIALGRADIENT_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_GRADIENT_ELEMENT)),

	// SVG_INTERFACE_RECT_ELEMENT
	CONST_ENTRY(GRAPHICAL_ELEMENT), //| events::EventTarget

	// SVG_INTERFACE_PATH_ELEMENT
	CONST_ENTRY(GRAPHICAL_ELEMENT | INHERITS(SVG_INTERFACE_ANIMATED_PATH_DATA)), //| events::EventTarget

	// SVG_INTERFACE_PATTERN_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_URI_REFERENCE) | \
					 INHERITS(SVG_INTERFACE_TESTS) |	INHERITS(SVG_INTERFACE_LANG_SPACE) | \
					 INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | \
					 INHERITS(SVG_INTERFACE_STYLABLE) | INHERITS(SVG_INTERFACE_FIT_TO_VIEW_BOX) | \
					 INHERITS(SVG_INTERFACE_UNIT_TYPES)),

	// SVG_INTERFACE_POLYGON_ELEMENT
	CONST_ENTRY(GRAPHICAL_ELEMENT | INHERITS(SVG_INTERFACE_ANIMATED_POINTS)),

	// SVG_INTERFACE_POLYLINE_ELEMENT
	CONST_ENTRY(GRAPHICAL_ELEMENT | INHERITS(SVG_INTERFACE_ANIMATED_POINTS)),

	// SVG_INTERFACE_STOP_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_STYLABLE)),

	// SVG_INTERFACE_STYLE_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT)),

	// SVG_INTERFACE_SVG_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_TESTS) | \
					 INHERITS(SVG_INTERFACE_LANG_SPACE) | \
					 INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | \
					 INHERITS(SVG_INTERFACE_STYLABLE) | \
					 INHERITS(SVG_INTERFACE_LOCATABLE) | INHERITS(SVG_INTERFACE_FIT_TO_VIEW_BOX) | \
					 INHERITS(SVG_INTERFACE_ZOOM_AND_PAN) | INHERITS(SVG_INTERFACE_TIMED_ELEMENT)),

	// SVG_INTERFACE_SYMBOL_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_LANG_SPACE) | \
					 INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | INHERITS(SVG_INTERFACE_STYLABLE)| \
					 INHERITS(SVG_INTERFACE_FIT_TO_VIEW_BOX)), // | events::EventTarget

	// SVG_INTERFACE_SWITCH_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_TESTS) | \
					 INHERITS(SVG_INTERFACE_LANG_SPACE) | INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | \
					 INHERITS(SVG_INTERFACE_STYLABLE) | INHERITS(SVG_INTERFACE_TRANSFORMABLE)), //| events::EventTarget

	// SVG_INTERFACE_TEXTPATH_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_TEXTCONTENT_ELEMENT) | INHERITS(SVG_INTERFACE_LOCATABLE) | INHERITS(SVG_INTERFACE_URI_REFERENCE)),
	// SVG_INTERFACE_TEXT_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_TEXTPOSITIONING_ELEMENT) | \
					 INHERITS(SVG_INTERFACE_TRANSFORMABLE)),
	// SVG_INTERFACE_TITLE_ELEMENT, // SVG_IF_ELEMENT | SVG_IF_LANG_SPACE | SVG_IF_STYLABLE
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_LANG_SPACE) | \
					 INHERITS(SVG_INTERFACE_STYLABLE)),
	// SVG_INTERFACE_TREF_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_TEXTPOSITIONING_ELEMENT) | \
					 INHERITS(SVG_INTERFACE_LOCATABLE) | \
					 INHERITS(SVG_INTERFACE_URI_REFERENCE)),
	// SVG_INTERFACE_TSPAN_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_TEXTPOSITIONING_ELEMENT) | INHERITS(SVG_INTERFACE_LOCATABLE)),

	// SVG_INTERFACE_USE_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_URI_REFERENCE) | \
					 INHERITS(SVG_INTERFACE_TESTS) | INHERITS(SVG_INTERFACE_LANG_SPACE) | \
					 INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | INHERITS(SVG_INTERFACE_STYLABLE) | \
					 INHERITS(SVG_INTERFACE_TRANSFORMABLE)), //| events::EventTarget

	// SVG_INTERFACE_VIEW_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | \
					 INHERITS(SVG_INTERFACE_FIT_TO_VIEW_BOX) | INHERITS(SVG_INTERFACE_ZOOM_AND_PAN)),

	// SVG_INTERFACE_A_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_URI_REFERENCE) | \
					 INHERITS(SVG_INTERFACE_TESTS) | INHERITS(SVG_INTERFACE_LANG_SPACE) | \
					 INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | INHERITS(SVG_INTERFACE_STYLABLE) | \
					 INHERITS(SVG_INTERFACE_TRANSFORMABLE)), // | events::EventTarget

	// SVG_INTERFACE_SCRIPT_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_URI_REFERENCE) | \
					 INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED)), // | events::EventTarget

	// SVG_INTERFACE_MPATH_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_URI_REFERENCE) | \
					 INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED)),

	// SVG_INTERFACE_FONT_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_STYLABLE) | \
					 INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED)),

	// SVG_INTERFACE_GLYPH_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_STYLABLE)),

	// SVG_INTERFACE_MISSING_GLYPH_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_STYLABLE)),

	// SVG_INTERFACE_FOREIGNOBJECT_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_TESTS) | INHERITS(SVG_INTERFACE_URI_REFERENCE) | \
					 INHERITS(SVG_INTERFACE_LANG_SPACE) | INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | \
					 INHERITS(SVG_INTERFACE_STYLABLE) | INHERITS(SVG_INTERFACE_TRANSFORMABLE) | INHERITS(SVG_INTERFACE_CAN_REFERENCE_DOCUMENT)), // | events::EventTarget

	// SVG_INTERFACE_AUDIO_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_TIMED_ELEMENT) | INHERITS(SVG_INTERFACE_URI_REFERENCE) | \
					 INHERITS(SVG_INTERFACE_TESTS) | INHERITS(SVG_INTERFACE_LANG_SPACE) | \
					 INHERITS(SVG_INTERFACE_EXTERNAL_RESOURCES_REQUIRED) | \
					 INHERITS(SVG_INTERFACE_STYLABLE)), // | events::EventTarget

	// SVG_INTERFACE_TEXTAREA_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_ELEMENT) | INHERITS(SVG_INTERFACE_TRANSFORMABLE)), // | events::EventTarget

	// SVG_INTERFACE_ANIMATION_ELEMENT
	CONST_ENTRY(INHERITS(SVG_INTERFACE_VISUAL_MEDIA_ELEMENT) | INHERITS(SVG_INTERFACE_CAN_REFERENCE_DOCUMENT))

CONST_SIMPLE_END(svg_if_inheritance_table)

#endif // SVG_DOM
