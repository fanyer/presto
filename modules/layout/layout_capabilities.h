/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _LAYOUT_CAPABILITIES_H_
#define _LAYOUT_CAPABILITIES_H_

/* Use small caches for these object types.  The caches should be
   cleared before Opera exits, and may be cleared eg. on OOM to
   free up memory. */

#define LAYOUT_CAP_POOL_HTMLayoutProperties
#define LAYOUT_CAP_POOL_LayoutProperties
#define LAYOUT_CAP_POOL_ContainerReflowState

/**
 * If we support removal of INPUT borders. This makes us start use the
 * border style value CSS_BORDER_STYLE_value_not_set
 */
#define LAYOUT_CAP_NO_INPUT_BORDERS

#define LAYOUT_CAP_PRESTO_3_FINAL_PATCH_105_MERGE

#define LAYOUT_CAP_HAS_EMBED_CONTENT_TYPE_API

/**
 * GetNextTextFragment() now takes a length parameter.
 */
#define LAYOUT_CAP_GET_NEXT_TEXT_FRAGMENT_LENGTH

/* VerticalBox has GetNegativeOverflow API */
#define LAYOUT_CAP_NEGATIVE_OVERFLOW

/* The img_alt_gen_val and the img_alt_content globals have been moved to layout_module.cpp */
#define LAYOUT_CAP_MOVED_GLOBALS

/** TextSelectionPoint has caret pixeloffset (used by documenteditor) */
#define LAYOUT_CAP_CARETPIXELOFFSET

/** Special attributes in the NS_LAYOUT exist in internal.h */
#define LAYOUT_CAP_HAS_SPECIAL_ATTRS

/** Has SelectContent::RemoveOptionGroup */
#define LAYOUT_CAP_HAS_REMOVEOPTIONGROUP

/** Textselection can set/read with offset in the textnode. Required by DOM and DocumentEdit. */
#define LAYOUT_CAP_SELECTIONPOINT_BY_OFFSET

/* Negative values in the LeadingOffset and TrailingOffset members of the
   LayoutAttr struct will be interpreted as (-x) / 24 em, instead of (x) px. */
#define LAYOUT_CAP_RELATIVE_DEFAULT_MARGINS

/** The bidi calculation class will not be defined in layout with DOCUTIL_CAP_HAS_BIDI_CALCULATION defined */
#define LAYOUT_CAP_HAS_REMOVED_BIDI_CALCULATION

/** BOOL has_clipping has changed to TraverseInfo traverse_info in all the traversing functions */
#define LAYOUT_CAP_HAS_TRAVERSE_INFO

/** InitStylesL() is defined and called in the layout module (used to be done from logdoc) */
#define LAYOUT_CAP_INITSTYLES

/** BoxEdgesObject has include_outline parameter */
#define LAYOUT_CAP_BOXEDGES_HAS_INCLUDE_OUTLINE

/** Has LayoutWorkplace class */
#define LAYOUT_CAP_HAS_LAYOUT_WORKPLACE

/** Gadgets now use control regions for determining where you may drag a gadget by */
#define LAYOUT_CAP_HAS_CONTROLREGION

/** GetNextTextFragment takes codepage argument */
#define LAYOUT_CAP_GNTF_TAKES_CODEPAGE

/** CSS counter code has moved to layout module */
#define LAYOUT_CAP_COUNTERS

/** Moved several small functions from FramesDocument to layout */
#define LAYOUT_CAP_MOVE_TO_LAYOUT_1

/** Sets SvgProperties.fontsize when HTMLayoutProperties.normal_font_size changes */
#define LAYOUT_CAP_USE_SVG_FONTSIZE

/** Has instrumentation of reflow, which can be exported with DOM. */
#define LAYOUT_CAP_HAS_REFLOW_INSTRUMENTATION

/** Support for finding the DHTML offsetParent of an element by searching the cascade.  */
#define LAYOUT_CAP_HAS_FINDOFFSETPARENT

/** ReplacedWeakContent has reflow_requested flag */
#define LAYOUT_CAP_HAS_REPLACEDWEAKCONTENT_REFLOW_REQUESTED

/** IntersectionObject has GetHoverInnerBox */
#define LAYOUT_CAP_INTERSECTION_OBJECT_HOVER_ELEMENT

/** Able to get the URL for the previous image from HEListElm (when an image
	is replaced via CSS or a script) while the new URL is loading */
#define LAYOUT_CAP_OLDIMG_IN_HELISTELM

/** The class ListItemMarker is available. */
#define LAYOUT_CAP_LIST_MARKER_CLASS

/** Has more info in TextSelectionObject for selecting replaced elements */
#define LAYOUT_CAP_REPLACED_ELEMENTS_SELECTION

/** Can handle SVG in CSS background-images */
#define LAYOUT_CAP_HANDLES_SVG_IN_CSSBACKGROUNDS

/** Has traverse_info parameter in EnterLine */
#define LAYOUT_CAP_ENTERLINE_TRAVERSEINFO

/** EmbedContent::Hide is implemented */
#define LAYOUT_CAP_EMBEDCONTENT_HIDE

/** HTMLayoutProperties::PropertyChangeChangesBoxType() is available. */
#define LAYOUT_CAP_BOXCHANGE_DETECTION

/** Text alignment in forms is done in SetCssPropertiesFromHtmlAttr rather than in the forms themselves */
#define LAYOUT_CAP_DEFAULT_FORM_ALIGNMENT

/** Moved reflow and finish layout from logdoc to layout */
#define LAYOUT_CAP_WORKPLACE_REFLOW_INTERNAL

/** Moved the implementation of HTML_Element::GetBoxRect to Box::GetRect */
#define LAYOUT_CAP_BOX_GETRECT

/** Has Box::GetOffsetFromAncestor, which replaces HTML_Element::GetBoxPosition. */
#define LAYOUT_CAP_BOX_GETOFFSETFROMANCESTOR

/** Possibility to override media style in ERA */
#define LAYOUT_CAP_OVERRIDE_MEDIA_STYLE

/** We can yield reflow and resume later */
#define LAYOUT_CAP_YIELD

/** The layout module accepts (but doesn't necessarily handle correctly) ROWSPAN or COLSPAN set to 0. */
#define LAYOUT_CAP_ROWCOLSPAN0

/** LayoutProperties has navup_cp, navdown_cp, navleft_cp, navright_cp for the CSS3 properties:
	nav-up, nav-down, nav-left, nav-right. */
#define LAYOUT_CAP_CSS3_NAVIGATION

/** layout.h declares some CssMap* types, so that logdoc doesn't have to guess. */
#define LAYOUT_CAP_CSSMAP_ARRAYS

/** Has PaintObject::IsHighlightPainted */
#define LAYOUT_CAP_ISHIGHLIGHTPAINTED

/** Has SnapIntersectionObject() */
#define LAYOUT_CAP_HAS_SNAPINTERSECTION_V2

/** LayoutProperties::GetProps */
#define LAYOUT_CAP_LAYOUTPROPERTIES_GETPROPS

/** LayoutWorkplace::IsRtlDocument */
#define LAYOUT_CAP_LAYOUTWORKPLACE_IS_RTL_DOCUMENT

/** LayoutWorkplace controls forced flushing. */
#define LAYOUT_CAP_FORCED_FLUSH

/** Debug flags to keep track if we are reflowing or traversing. Only used with ifdef _DEBUG */
#define LAYOUT_CAP_IS_TRAVERSING_OR_REFLOWING_DEBUG

/** LayoutWorkplace::HandleContentSizedIFrame() */
#define LAYOUT_CAP_HANDLE_CONTENT_SIZED_IFRAME

/** Content has GetPluginWindow method for retrieving OpPluginWindow for ExternalContent. */
#define LAYOUT_CAP_CONTENT_GET_PLUG_WIN

/** GetCalculatedLineHeight(VisualDevice* vd) */
#define LAYOUT_CAP_GETCALCULATEDLINEHEIGHT_TAKES_VISDEV

/** ButtonContainer has OpButton* GetButton(). */
#define LAYOUT_CAP_GET_OPBUTTON

/** CssPropertyItem defined and implemented in the layout module. */
#define LAYOUT_CAP_CSSPROPS

/** LayoutWorkplace has LoadProperties and ApplyPropertyChanges which are replacements for
	CssPropertyItem::LoadCssProperties and ApplyPseudoClasses respectively. */
#define LAYOUT_CAP_LOAD_APPLY_PROPS

/** Handles type decls for system fonts for resetting other font properties than font-family.
	Enabled by STYLE_CAP_SYSFONTS_RESET_PROPS. */
#define LAYOUT_CAP_SYSFONTS_RESET_PROPS

/** LayoutWorkplace provides methods for checking initial containing block
	minimum and maximum width. */
#define LAYOUT_CAP_FLEXROOT_INFO

/** Layout no longer defines the IsCSSPunctuation macro. */
#define LAYOUT_CAP_NO_BREAK_UNI_ISPUNCT

/** HTML_Element::SetCssProperties does not recurse by itself. */
#define LAYOUT_CAP_SETCSSPROPERTIES_DOES_NOT_RECURSE2

/** The layout module can handle CSS_PROPERTY_overflow_x and CSS_PROPERTY_overflow_y separately.
	The functionality is only turned on if the style module defines STYLE_CAP_OVERFLOW_XY. */
#define LAYOUT_CAP_OVERFLOW_XY

/** Moved MarkContentWithPercentageHeightDirty(), MarkBoxesWithAbsoluteWidthDirty(),
	MarkContainersDirty() and ClearUseOldRowHeights() from HTML_Element to Box. */
#define LAYOUT_CAP_MOVED_MISC_DIRTY

/** ApplyPropertyChanges has attribute parameter with namespace to check potential changes caused by
	SetCssPropertiesFromHtmlAttr. */
#define LAYOUT_CAP_APPLYPROPS_ATTR

/** Passes all text through the text shaper before performing font switching, so that the
    font switching is done on the characters that are actually to be displayed. */
#define LAYOUT_CAP_FONTSWITCHING_THROUGH_TEXTSHAPER

/** There is a TextSelection::RemoveLayoutCacheFromSelection method
    as well as TextSelectionPoint::SetLayoutPosition and
	TextSelectionPoint::SetLogicalPosition that basically replaces all the
	previous setters. */
#define LAYOUT_CAP_RESILIANT_TEXT_SELECTION


/** Use the new API in FormObject */
#define LAYOUT_CAP_USE_FORMOBJECT_GETPREFERREDSIZE

/** After properties has been set to the cascade, give SVG a change to
	resolve them. This includes translating currentColor using
	font_color. In practice, this capability means that
	HTMLayoutProperties::GetCssProperties() calls
	SvgProperties::Resolve() if the SVG module has the support. */
#define LAYOUT_CAP_RESOLVES_SVG_PROPS

/** The fileupload has correct handling of borders, size and padding. Was left out when fixing FORMS_CAP_GET_PREFERRED_SIZE. */
#define LAYOUT_CAP_FILEUPLOAD_CSS_FIX

/** A lot of outline responsibility is now moved to VisualDevice
	Will allow things like polygon shaped outlines
*/
#define LAYOUT_CAP_POLYGON_OUTLINE

/** Layout module is ready to handle default stylesheet in style module instead of SetCssPropertiesFromHtmlAttr. */
#define LAYOUT_CAP_DEFAULT_STYLE

/** The layout module has Box::GetRectList that can be used to get
	splitted client rectangles from inline elements. See
	Box::GetRectList for more information. */
#define LAYOUT_CAP_BOX_GETRECTLIST

/** The layout module has a Box::GetRect that takes two extra
	parameters that allow fetching rectangles corresponding to
	substring of text elements. See Box::GetRect() for more
	information. */
#define LAYOUT_CAP_BOX_GETRECT_TAKES_TEXT_POS

/** This module includes layout/cssprops.h and layout/layout_workplace.h where needed,
	instead of relying on header files in other modules doing it. */
#define LAYOUT_CAP_INCLUDE_CSSPROPS

/** Flag to determine if we should heep to original layout on this page - i.e. not apply any layout tricks like flex-root,
	text wrapping or adaptive zoom. */
#define LAYOUT_CAP_KEEP_ORIGINAL_LAYOUT

/** Filtering css declarations based on MDS pref in ERA may be done in style during css selection. */
#define LAYOUT_CAP_ERA_MDS_IN_STYLE

/** BorderEdge resets to NOT_SET values instead of default values. */
#define LAYOUT_CAP_BORDEREDGE_RESET_FIX

/** CssURL class has IsSkinURL method. */
#define LAYOUT_CAP_CSSURL_ISSKIN

/** Markup::LAYOUTA_SKIN_ELM, used for storing skin element names for generated replaced content, exists. */
#define LAYOUT_CAP_ATTR_SKIN_ELM

/** LayoutWorkplace YieldForceLayoutChanged and firends exists. */
#define LAYOUT_CAP_YIELD_FORCE_LAYOUT_CHANGED

/** SetCssPropertiesFromHtmlAttr resets line-height for Markup::HTE_TABLE to -NORMAL_LINE_HEIGHT_FACTOR_I in quirks mode. */
#define LAYOUT_CAP_RESET_TABLE_LINEHEIGHT

/** EmbedContent class has SetRemoved method. */
#define LAYOUT_CAP_EMBEDCONTENT_SETREMOVED

/** Layout will not resize iframes during paint. */
#define LAYOUT_CAP_FRAMES_SIZE_FIX

/** Supports percentage intrinsic size */
#define LAYOUT_CAP_PERCENTAGE_INTRINSIC_SIZE

/** LayoutWorkplace::ApplyPropertyChanges is prepared to let HTML_Document::SetHoverHTMLElement handle which
	elements needs to have ApplyPropertyChanges called when hover element changes. */
#define LAYOUT_CAP_HOVER_TOP_PSEUDO_ELM

/** LayoutWorkplace::ApplyPropertyChanges has skip_top_element parameter. */
#define LAYOUT_CAP_APC_SKIP_TOP_ELEMENT

/** We do our best to reuse the old cascade after yielding. */
#define LAYOUT_CAP_YIELD_KEEP_CASCADE

/** Has LayoutWorkplace::EndElement. */
#define LAYOUT_CAP_LWP_ENDELEMENT

/** Has LayoutWorkplace::HandleDocumentTreeChange. */
#define LAYOUT_CAP_LWP_HANDLEDOCTREECHANGE

/** LayoutWorkplace::GetParagraphList() exists. */
#define LAYOUT_CAP_GETPARAGRAPHLIST

/** Has CSS_BORDER_STYLE_highlight_border */
#define LAYOUT_CAP_HIGHLIGHT_BORDER

/** The layout module can satisfy the requirements of the plug-in event API.
	May be used to toggle import of API_PI_USE_PLUGIN_EVENT_API. */
#define LAYOUT_CAP_PLUGIN_EVENT_API

/** Supports HLDocProfile::SetViewportOverflow() */
#define LAYOUT_CAP_SUPPORTS_VIEWPORT_OVERFLOW_XY

/** Supports ViewportMeta and support methods in LayoutWorkplace. */
#define LAYOUT_CAP_VIEWPORT_META

/** LayoutWorkplace::HandleContentSizedIFrame takes a BOOL signaling
    whether the root has changed size */
#define LAYOUT_CAP_HANDLE_CS_IFRAME_ROOT

/** The layout module can handle 32-bit color values with MSB used for named color bit. */
#define LAYOUT_CAP_RGBA_COLORS

/** LayoutProperties::STOP means the same as LAYOUT_STOP. */
#define LAYOUT_CAP_STOP_CONSISTENT

/** The layout module can recognize and handle the property CSS_PROPERTY_spotlight. */
#define LAYOUT_CAP_SPOTLIGHT

/** WordInfo::is_misspelled exists */
#define LAYOUT_CAP_WORD_INFO_HAS_IS_MISSPELLED

/** Text selection is returned with line feed only as line break, no
	carriage return. */
#define LAYOUT_CAP_TEXT_SELECTION_IS_PLAIN_TEXT

/** LayoutWorkplace::IsTraversable() exists. */
#define LAYOUT_CAP_ISTRAVERSABLE

/** Prior this capability, the layout module would miscalculate the
	right and bottom of inline elements reported to travseral
	objects. The spatial navigation module had workarounds for this
	that they can drop then this capability exists. */
#define LAYOUT_CAP_ENTERINLINEBOX_BOXAREA_IS_CORRECT

/** LayoutWorkplace::SignalHtmlElementRemoved() exists. */
#define LAYOUT_CAP_SIGNAL_ELEMENT_REMOVED

/** LayoutModule owns SharedCssManager. */
#define LAYOUT_CAP_OWNS_SHAREDCSSMANAGER

/** Using the NextWordFS as a member variable in VisualDevice, not extern declared function. */
#define LAYOUT_CAP_USE_MEMBER_NEXTWORDFS

/** VisualDevice::NextWordFS was just a pretty useless wrapper. Layout doesnt use it anymore. */
#define LAYOUT_CAP_DONT_USE_NEXTWORDFS

/** When loading Java Applets, LoadInlineAppletHandle can be used instead of and LoadInlineElm
	pointer. This only actually happens if DOC_CAP_APPLET_LIE_HANDLE and
	JAVA_CAP_CAN_RECEIVE_LIE_APPLET_HANDLE are defined */
#define LAYOUT_CAP_CAN_SEND_LIE_APPLET_HANDLE

/** Has taken care of GetComputedDecl and friends from the logdoc module. */
#define LAYOUT_CAP_HAS_GET_COMPUTED_DECL

/** Supports overflow-wrap (alias word-wrap) property. */
#define LAYOUT_CAP_WORD_WRAP

/** provides GetNextTextFragment taking WritingSystem. uses multistyle version of Get/SetStyle[Ex] */
#define LAYOUT_CAP_MULTISTYLE_AWARE

/** Supports ASCII key strings for actions/menus/dialogs/skin */
#define LAYOUT_CAP_INI_KEYS_ASCII

/** LayoutWorkplace::SetLayoutViewPos() returns an OpRect that specifies the area that has
	was scheduled for repaint. */
#define LAYOUT_CAP_SETLAYOUTVIEWPOS_RETURNS_AREA

/** Ready for default vertical-align values on THEAD, TBODY, TFOOT, TR, TH and TD
	more or less as defined in http://www.w3.org/TR/CSS21/sample.html */
#define LAYOUT_CAP_CSS21_TABLE_VALIGN

/** Layout uses StyleManager::GetGenericFontNumber to fetch generic
	font numbers. */
#define LAYOUT_CAP_GENERIC_FONTS

/** Has LayoutWorkplace::SetIsTraversing() and LayoutWorkplace::IsTraversing() */
#define LAYOUT_CAP_ISTRAVERSING

/** Can pass LayoutProperties to SelectionObject::ApplyProps */
#define LAYOUT_CAP_APPLYPROPS_LAYOUTPROPS

/** SetNewSelection has BOOL highlight. */
#define LAYOUT_CAP_SETNEWSELECTION_SELECT

/** LayoutWorkplace has GetMediaQueryWidth/Height. */
#define LAYOUT_CAP_MQ_WIDTH_HEIGHT

/** Ready to separate HTML parsing from layout. See SEPARATE_PARSING_AND_LAYOUT define in logdoc_module.h */
#define LAYOUT_CAP_SEPARATE_PARSING_AND_LAYOUT

/** SVGContent::Layout exists and is ready to call into the SVG module */
#define LAYOUT_CAP_HAS_SVGCONTENT_LAYOUT

/** Can support svg highlighting */
#define LAYOUT_CAP_SVG_HIGHLIGHTING

/** Uses UnicodePoint for representation of code points */
#define LAYOUT_CAP_UNICODE_POINT_SUPPORT

/** Has stopped using BOXPOS_IGNORE_SCROLL */
#define LAYOUT_CAP_NO_BOXPOS

/** Has LayoutWorkplace::ResetPlugin */
#define LAYOUT_CAP_LAYOUTWORKPLACE_RESET_PLUGIN

#endif // _LAYOUT_CAPABILITIES_H_
