/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef SVG_CAPABILITIES_H
# define SVG_CAPABILITIES_H

// Has interfaces in module root directory and no external modules
// includes headers from src/.
# define SVG_CAP_HAS_YOCTO_2_API

# define SVG_CAP_USE_LAYOUT_PROPS //< SVGManager::Render takes LayoutProperties* instead of HTMLayoutProperties
# define SVG_CAP_HAS_SVGPATH_API //< Has SVGManager::CreatePath and public SVGPath abstract class

# define SVG_CAP_CONTEXT_IS_COMPLEX //< SVGContext inherits ComplexAttr

# define SVG_CAP_HAS_DOCUMENTDESTROY_API //< Has SVGManager::DocumentDestroy(FramesDocument* frm_doc)

# define SVG_CAP_ATTR_ORDER_CHANGES //< The attributes starts at another number

# define SVG_CAP_HAS_DOM_TIME_CONTROL //< SVGDOM::BeginElement, SVGDOM::EndElement
# define SVG_CAP_HAS_DOM_FILTERS //< SVGDOM::SetFilterRes, SVGDOM::SetStdDeviation, SVGDOM::GetAnimatedNumberOptionalNumber
# define SVG_CAP_HAS_NEW_TRANSFORM_TO_ELEMENT //< Added another argument to SVGDOM::GetTransformToElement
# define SVG_CAP_HAS_DOM_ASPECT_RATIO //< Has SVGPreserveAspectRatio type
# define SVG_CAP_HAS_DOM_GET_VIEWPORT_ELEMENT //< SVGDOM::GetViewportElement

# define SVG_CAP_HAS_ONINPUTACTION_API //< Has SVGManager::OnInputAction(OpInputAction*, HTML_Element*, FramesDocument*)
# define SVG_CAP_HAS_MORE_CSS_PROPS //< Has SVGLayoutProperties stopcolor to enable-background
# define SVG_CAP_HAS_SET_SVG_CSSPROPS_FROM_ATTR //< Has HTMLayoutProperties::SetSVGCSSPropertiesFromAttr(HTML_Element* element, const HTMLayoutProperties& parent_props, BOOL replaced_elm, BOOL is_form_elm, HLDocProfile* hld_profile) in SVG module
# define SVG_CAP_HAS_ON_DOCUMENT_CHANGED //< Has SVGManager::OnDocumentChanged(HTML_Element* node_in_document)
# define SVG_CAP_HAS_GET_EVENTTARGET //< Has SVGManager::GetEventTarget(HTML_ELement* svg_elm)
# define SVG_CAP_NAVIGATE_TO_ELEMENT_HAS_EVENT //< SVGManager::NavigateToElement has an event parameter
# define SVG_CAP_HAS_GET_CURSOR //< SVGManager::GetCursorForElement exists
# define SVG_CAP_HAS_SETFROMCSSDECL //< Has SvgProperties::SetFromCSSDecl(CSS_decl*, LayoutProperties*, HTML_Element*, HLDocProfile*)
# define SVG_CAP_XLINK_TITLE_AND_SHOW //< enums for show and title attributes in xlink
# define SVG_CAP_HAS_PAINT_ITEM_TYPE //< Has SVGDOMPaint and SVGDOMColor methods
# define SVG_CAP_HAS_EXCEPTIONS // Has the SVGDOM::SVGExceptionsCode enum
# define SVG_CAP_HAS_PARSE_ATTRIBUTE_WITH_NS_IDX // SVGManager::ParseAttribute takes and int as attr_type and an additional ns_idx.
# define SVG_CAP_GETCHARNUMATPOSITION_HAS_LONG
# define SVG_CAP_NAVIGATE_TO_ELEMENT_HAS_TARGET //< SVGManager::NavigateToElement has a window_target parameter
# define SVG_CAP_HAS_FULL_ON_DOCUMENT_CHANGED ///< Has SVGManager::OnDocumentChanged with several arguments
# define SVG_CAP_TIMED_INVALIDATION ///< Has SVGManager::OnInvalidationTimerTimeOut(FramesDocument* frm_doc)
# define SVG_CAP_ACCESS_KEY ///< Has SVGManager::HandleAccessKey
# define SVG_CAP_XLINK_HAS_ROOT_URL ///< SVGManager::GetXLinkURL has a root_url argument.
# define SVG_CAP_HAS_SVGFONTMANAGER_API ///< SVGFontManager
# define SVG_CAP_HAS_GETINSTANCEROOT_API ///< Has SVGDOM::GetInstanceRoot
# define SVG_CAP_HAS_HASFEATURE ///< Has SVGDOM::HasFeature
# define SVG_CAP_HAS_ISSAMEATTRVALUE ///< Has SVGManager::IsSameAttributeValue
# define SVG_CAP_HAS_SET_AS_CURRENT_DOC ///< Has SVGManager::SetAsCurrentDoc
# define SVG_CAP_HAS_HANDLE_STYLE_CHANGE ///< Has SVGManager::HandleStyleChange
# define SVG_CAP_HAS_HANDLE_INLINE_CHANGED ///< Has SVGManager::HandleInlineChanged
# define SVG_CAP_HAS_DOM_MARKERS ///< Has SVGDOM::SetOrient, marker enums
# define SVG_CAP_HAS_DOM_ZOOMANDPAN ///< Has SVGDOM::AccessZoomAndPan
# define SVG_CAP_HAS_DOM_STRINGLIST ///< Has SVGDOM::GetStringList and SVGDOMStringList
# define SVG_CAP_HAS_DOM_HASEXTENSION //< Has SVGDOM::HasExtension
# define SVG_CAP_HAS_FONTSIZE ///< Has SvgProperties.fontsize
# define SVG_CAP_HAS_OPFONT_DELETE ///< Has SVGManager::HandleFontDeletion
# define SVG_CAP_WORKPLACE ///< SVGWorkplace and SVGImage exists.
# define SVG_CAP_SVGIMAGE_IS_INPUTCONTEXT ///< SVGImageImpl is OpInputContext
# define SVG_CAP_HAS_DOM_IS_VALID ///< Has SVGDOMPathSeg::IsValid
# define SVG_CAP_HAS_DOM_GET_VALUE_ELEMENT /// SVGDOMLength::GetValue takes a element, attr, ns as arguments
# define SVG_CAP_HAS_DOM_ANIMATED_VALUE_NS /// SVGDOM::GetAnimatedValue also takes a namespace
# define SVG_CAP_HAS_LAST_KNOWN_FPS /// Has SVGImage::GetLastKnownFPS()
# define SVG_CAP_HAS_GLOBAL_SERIAL /// Has SVGDOM::Serial()
# define SVG_CAP_HAS_TEXTSELECTION_API ///< Has SVGManager::HasSelectedText
# define SVG_CAP_HAS_DOM_ANIMATION_ELEMENT ///< Has SVGDOM::GetAnimationSimpleDuration, SVGDOM::GetAnimationCurrentTime, SVGDOM::GetAnimationStartTime, SVGDOM::GetAnimationTargetElement
# define SVG_CAP_DOM_STORE_DOM_OBJECTS ///< DOM_Object <-> SVGObject relation is stored in the svg module.
# define SVG_CAP_TOOLTIP ///< Has SVGManager::GetTooltip
# define SVG_CAP_CONTEXTS_STORE_THEMSELVES ///< SVGElementStateContext::Create and g_svg_manager->CreateDocumentContext store the created context in the element
# define SVG_CAP_SVG_11_COMPATIBLE_DOM ///< The DOM list interfaces keep original strings in order to be compatible with SVG 1.1 DOM
# define SVG_CAP_OWN_PAINTING ///< The SVG module knows how to paint on the VisualDevice by itself, which removes the need for gigantic bitmaps in the external API
# define SVG_CAP_IMAGE_HAS_DOCUMENT_SWITCH ///< We have a SVGImage::SwitchDocument method
# define SVG_CAP_SVGIMAGE_HAS_IS_ECMASCRIPT_ENABLED ///< We have a SVGImage::IsEcmaScriptEnabled method
# define SVG_CAP_TYPE_ATTRIBUTE_AS_STRING ///< Moves parsing of some string attributes that are not animatable to logdoc
# define SVG_CAP_HAS_IS_FOCUSABLE_ELEMENT ///< We have SVGManager::IsFocusableElement
# define SVG_CAP_HAS_SVGIMAGEREF ///< Has SVGImageRef class and SVGManager::GetSVGImageRef
# define SVG_CAP_HANDLE_SVGLOAD_INTERNALLY ///< We take care of propagating SVGLOAD internally. doc only sends the event to the root element.
# define SVG_CAP_HAS_PAINT_TO_BUFFER ///< Has SVGImage::PaintToBuffer
# define SVG_CAP_DOM_HAS_STRING_VERSION ///< SVGDOM::HasFeature uses a string instead of a bitfield
# define SVG_CAP_SAVE_AS_PNG_SUPPORT ///< Has SVGManager::ConvertOpBitmapToPNG and WritePNGToFile
# define SVG_CAP_HAS_HANDLE_EVENT_FINISHED ///< Has SVGManager::HandleEventFinished
# define SVG_CAP_HAS_HANDLER_ELEMENT ///< Has Markup::SVGE_HANDLER
# define SVG_CAP_HAS_NAVIGATION_API ///< Has SVGTreeIterator, SVGManager::GetNavigationIterator, SVGManager::ReleaseIterator, SVGManager::HasNavTargetInDirection, SVGManager::GetNavigationData, SVGManager::UpdateHighlight
# define SVG_CAP_USE_LAYOUTPROPERTIES_GETPROPS ///< Using LayoutProperties::GetProps to access HTMLayoutProperties. For cached props
# define SVG_CAP_HAS_ALLOW_FRAME_FOR_ELEMENT ///< Has SVGManager::AllowFrameForElement
# define SVG_CAP_HAS_ALLOW_INTERACTION_WITH_ELEMENT ///< Has SVGManager::AllowInteractionWithElement
# define SVG_CAP_HAS_ABSTRACTED_SVG_WORKPLACE ///< Has SVGWorkplace::Create and fewer exposed methods than before
# define SVG_CAP_HAS_GET_CLASS ///< Has SVGManager::GetClass
# define SVG_CAP_HAS_INSERT_AND_END_ELEMENT ///< Has SVGManager::InsertElement and SVGManager::EndElement
# define SVG_CAP_HAS_TEXTSELECTION_MODE ///< Has SVGManager::IsInTextSelectionMode
# define SVG_CAP_HAS_GETCSSDECL ///< Has SvgProperties::GetCSSDecl
# define SVG_CAP_HAS_UDOM_METHODS ///< Has SVGDOMPath, SVGDOMRGBColor, SVGDOM::Get/SetTrait
# define SVG_CAP_HAS_SVG_PROPERTIES_RESOLVE ///< Has SvgProperties::Resolve()
# define SVG_CAP_LAYOUT_OVERFLOW_XY ///< Can handle overflow_x and overflow_y instead of overflow from HTMLayoutProperties.
# define SVG_CAP_HAS_CURRENT_ROTATE ///< Has SVGDOM::Get/SetCurrentRotate
# define SVG_CAP_HAS_END_ELEMENT_WITH_PROPS ///< SVGManager::EndElement takes the current cascade as second argument.
# define SVG_CAP_HAS_SET_ANIMATION_TIME ///< Has SVGImage::Set(Virtual)AnimationTime()
# define SVG_CAP_HAS_DOM_PRIM_VALUE ///< Has SVGDOMPrimitiveValue along with associated enums and functions in SVGDOMAnimatedValue.
# define SVG_CAP_HAS_GETBOUNDINGCLIENTRECT ///< Has SVGDOM::GetBoundingClientRect
# define SVG_CAP_HAS_EDITABLE_SUPPORT ///< Has SVGManager::IsEditableElement and SVGManager::BeginEditing
# define SVG_CAP_HAS_SCROLL_TO_RECT ///< Has SVGManager::ScrollToRect
# define SVG_CAP_SVGPROPS_ONDEMAND ///< Understands that SvgProperties may be allocated on demand in the layout module.
# define SVG_CAP_HAS_CONTENT_CHANGED_FLAG ///< SVGManager::HandleInlineChanged has a third BOOL parameter "is_content_changed"

# define SVG_CAP_HTMLDEF_WH_NOT_USED ///< SetHTML_DefinedWidth/SetHTML_DefinedHeight not used in this module.
# define SVG_CAP_INCLUDE_CSSPROPS ///< This module includes layout/cssprops.h and layout/layout_workplace.h where needed, instead of relying on header files in other modules doing it.
# define SVG_CAP_SVGIMAGE_HAS_ISSECURE ///< SVGImage::IsSecure exists
# define SVG_CAP_SVGDOM_HAS_ENCLOSURELIST ///< SVGDOM::GetIntersectionList and SVGDOM::GetEnclosureList exist
# define SVG_CAP_SVGIMAGE_HAS_INTRINSIC_RATIO ///< SVGImage::GetIntrinsicSize exists
# define SVG_CAP_SVGMANAGER_HAS_IS_ECMASCRIPT_ENABLED ///< SVGManager::IsEcmaScriptEnabled exists
# define SVG_CAP_SCROLL_TO_RECT_GLOBAL_ENUM // ScrollToRect uses the global SCROLL_ALIGN enum instead of the one in HTML_Document
# define SVG_CAP_HAS_WEBFONT_API ///< Has Webfont API:s in SVGManager
# define SVG_CAP_HAS_INTRINSIC_OVERRIDE ///< Has SVGImage::GetIntrinsicSize with an allow_override parameter
# define SVG_CAP_HAS_DOM_NOTIFY_CURRENT_TRANSLATE ///< Has SVGDOM::OnCurrentTranslateChange
# define SVG_CAP_HAS_ROLE ///< Has Markup::SVGA_ROLE (the role attribute)
# define SVG_CAP_USE_NEW_ADDSVGWEBFONT ///< Uses WebFontManager::AddSVGWebFont with FramesDocument
# define SVG_CAP_HAS_SET_TARGET_FPS ///< SVGImage::SetTargetFPS exists
#define SVG_CAP_INI_KEYS_ASCII ///< Supports ASCII key strings for actions/menus/dialogs/skin
#define SVG_CAP_NO_FONTNUMBER_ALLOCATION ///< The SVG module does not allocate fontnumbers (depends on WEBFONTS_SUPPORT)
#define SVG_CAP_MULTISTYLE_AWARE ///< use SetScript instead of SetCodepage if present
#define SVG_CAP_HAS_PROGRESS_EVENT_ATTRS ///< Has Markup::SVGA_ONLOADSTART through Markup::SVGA_ONLOADEND
#define SVG_CAP_HAS_ISANIMATIONRUNNING ///< Has SVGImage::IsAnimationRunning
#define SVG_CAP_HAS_SVGIMAGEREF_GETURL ///< SVGImageRef has a GetUrl method
#define SVG_CAP_HAS_LAYOUT_HOOKS ///< Has SVGImage::On{Reflow,ContentSize,Paint,SignalChange}
#define SVG_CAP_SEPARATE_PARSING_AND_LAYOUT ///< Ready to separate HTML parsing from layout. See SEPARATE_PARSING_AND_LAYOUT define in logdoc_module.h
#define SVG_CAP_HAS_HIGHLIGHT_UPDATE_ITERATOR ///< Has SVGManager::GetHighlightUpdateIterator
#define SVG_CAP_HAS_ISVISIBLE ///< Has SVGManager::IsVisible
#define SVG_CAP_WRITINGSYSTEM_ENUM ///< Understands that HTMLayoutProperties::current_script may be an enum
#endif // SVG_CAPABILITIES_H
