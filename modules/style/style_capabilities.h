/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef STYLE_CAPABILITIES_H
#define STYLE_CAPABILITIES_H

// Define capabilities supported in current style module

// Has svg css values/properties
#define STYLE_CAP_HAS_SVG_CSS
#define STYLE_CAP_HAS_MORE_SVG_CSS
#define STYLE_CAP_HAS_SVG_VECTOR_EFFECT
#define STYLE_CAP_HAS_SVGT12_PROPS

// CSSCollection::GetPageProperties and CSS::GetPageRuleset
// takes a media type parameter.
#define STYLE_CAP_HAS_PAGE_PROPS_MEDIA_PARAM

// Supports experimental -o-text-overflow property with "ellipsis"
// as only supported value.
#define STYLE_CAP_HAS_O_TEXT_OVERFLOW

// CSSManager class has UpdateActiveUserStyles method.
#define STYLE_CAP_UPDATE_ACTIVE_USER_STYLES

// Has ParseColor function for parsing color strings into COLORREF values.
#define STYLE_CAP_PARSE_COLOR

// CSS_generic_value struct has get/set methods for its member values.
#define STYLE_CAP_GENERIC_VALUE_GETSET_API

// CSS_generic_value and CSS_string_decl has an IsSourceLocal method used
// to check whether a declarations origin is a local stylesheet or not.
#define STYLE_CAP_IS_SOURCE_LOCAL

// CSSManager class has OnInputAction method.
#define STYLE_CAP_HANDLES_INPUTACTIONS

// Has the ExtractURLs method for parsing CSS for URLs which need to be saved
// when saving with images/as archive.
#define STYLE_CAP_EXTRACT_CSS_URLS

// SaveCSSWithInline takes main_page parameter
#define STYLE_CAP_SAVE_INLINES_IN_DIR

// Has the API for farah_2 versions.
#define STYLE_CAP_FARAH_2

// Supports experimental -o-background-size property for a single background image.
#define STYLE_CAP_HAS_O_BACKGROUND_SIZE

// CSS_DOMRule has a member to store the DOM_CSSRule to ensure we don't create more
// than one DOM_CSSRule per CSS_Rule. Methods are SetDOMRule and GetDOMRule and are
// to be used in DOM_CSSRuleList to set and retrieve the DOM_CSSRule for a given rule.
#define STYLE_CAP_STORE_DOM_CSSRULE

// Supports css property text-shadow from CSS2.
#define STYLE_CAP_HAS_TEXT_SHADOW

// Has the StyleAttribute class
#define STYLE_CAP_STYLE_ATTRIBUTE

// Macro for checking whether a media type is in the paged media group (CSS_IS_PAGED_MEDIUM)
#define STYLE_CAP_PAGED_MEDIA_MACRO

// CSS_COLOR_KEYWORD_TYPE_named uses only 1 bit of the 8 most
// significant instead of 2
#define STYLE_CAP_ONE_BIT_COLOR_NAMED

// The CSS parser supports dash-prefixed idents in general. Not only for experimental properties.
// This was introduced in the lexer specification of CSS2.1 at some point. The change in the style
// module includes experimental properties and values in the normal arrays of property strings and
// changes the name of experimental CSS_PROPERTY_* and CSS_VALUE_* constants.
#define STYLE_CAP_IDENT_DASH_PREFIX

// The style module recognizes the :-o-prefocus pseudo class. And the CSS_PSEUDO_CLASS__O_PREFOCUS
// constant exists.
#define STYLE_CAP_O_PREFOCUS

// CSSCollection has HasMediaQueryChanged method, and CSS has MediaAttrChanged for supporting dynamic
// media queries.
#define STYLE_CAP_DYNAMIC_MEDIA_QUERIES

// The style module include @namespace rules in the DOM. Means that CSS_DOMRule has NAMESPACE type,
// and GetNamespacePrefix and GetNamespaceURI methods.
#define STYLE_CAP_NAMESPACE_DOMRULE

// Supports nav-up, nav-down, nav-left, nav-right, nav-index from the CSS3 Basic User Interface Module.
#define STYLE_CAP_CSS3_NAVIGATION

// Supports pre-line as value for white-space property.
#define STYLE_CAP_PRE_LINE_VALUE

// Supports -o-language-string function for using strings from language db in content property.
#define STYLE_CAP_O_LANGUAGE_STRING

// Supports the -o-table-baseline property
#define STYLE_CAP_O_TABLE_BASELINE

// Has ExtractURL which returns OP_STATUS, not just LEAVES.
#define STYLE_CAP_HAS_NON_LEAVE_EXTRACT_URL

// Using LayoutProperties::GetProps to access HTMLayoutProperties. For cached props
#define STYLE_CAP_USE_LAYOUTPROPERTIES_GETPROPS

// Has pseudo group macros: CSS_PSEUDO_CLASS_GROUP_MOUSE and CSS_PSEUDO_CLASS_GROUP_FORM
#define STYLE_CAP_PSEUDO_CLASS_GROUPS

// Don't use the modules/logdoc/src/htmlx.h file
#define STYLE_CAP_DONT_USE_HTMLX

// System fonts set by font shorthand resets all font properties and line-height.
// This is implemented by adding the system font keyword as a type decl to all the
// different properties. Only enabled if layout can handle them (LAYOUT_CAP_SYSFONTS_RESET_PROPS).
#define STYLE_CAP_SYSFONTS_RESET_PROPS

// The style module recognises two separate overflow values for x and y as specified in CSS3
// as well as the CSS2.1 overflow property.
#define STYLE_CAP_OVERFLOW_XY

// CSS_PROPERTY_outline_offset defined and outline-offset property handled by the parser.
#define STYLE_CAP_OUTLINE_OFFSET

// Has CSSCollection::GetDefaultStyle and several internal properties and values which
// are not true css properties/values.
#define STYLE_CAP_DEFAULT_STYLE

// Has -o-highlight-border
#define STYLE_CAP_HIGHLIGHT_BORDER

// Has CSS_VALUE_up and CSS_VALUE_down
#define STYLE_CAP_VALUE_UP_DOWN

// This module includes layout/cssprops.h and layout/layout_workplace.h where needed, instead of relying on header files in other modules doing it.
#define STYLE_CAP_INCLUDE_CSSPROPS

// Filtering css declarations based on MDS pref in ERA can be done in style during css selection.
#define STYLE_CAP_ERA_MDS_IN_STYLE

// Layout module may set BorderEdge values to NOT_SET values instead of default values.
#define STYLE_CAP_BORDEREDGE_RESET_FIX

// CSS_decl_string has separate types for StringDeclUrl/StringDeclSkin/StringDeclString
#define STYLE_CAP_STRINGDECL_TYPES

// Is defaulting radio and checkboxes padding to 1 instead of 0.
#define STYLE_CAP_HAS_CHECKRADIO_PADDING

// CSS_DOMStyleSheet::GetImportRule present.
#define STYLE_CAP_GETIMPORTRULE

// The style module does GetUniNameWithRel instead of UniRelName for the 'fill' and 'stroke' properties
#define STYLE_CAP_ABSOLUTE_FILL_STROKE_URLS

// CSS_DOMRule::GetStyleSheetElm is present.
#define STYLE_CAP_DOMGETSTYLESHEETELM

// Selector matching generates CSS_PROPERTY_selection_color and
// CSS_PROPERTY_selection_background_color for color and background-color
// properties for ::selection pseudo elements.
#define STYLE_CAP_SELECTION_COLORS

// Do not display content in Markup::HTE_NOFRAMES and Markup::HTE_NOEMBED if frames
// resp. plugins are enabled
#define STYLE_CAP_NOFRAMES_NOEMBED_DISPLAY_NONE

// Supports all cursor keywords from the CSS3 Basic User Interface Module.
#define STYLE_CAP_CSS3_CURSOR_KEYWORDS

// The style module can handle 32-bit color values with MSB used for named color bit.
#define STYLE_CAP_RGBA_COLORS

// Has the input-format css property
#define STYLE_CAP_INPUT_FORMAT

// class CSSCollection::Iterator exists
#define STYLE_CAP_CSSCOLL_ITERATOR

// CSSCollection has the capability to add stylesheets to a pending list with
// AddCSS(css, FALSE), and commit them later with CommitAddedCSS().
#define STYLE_CAP_ADD_COMMIT_CSS

// CSSCollection::GetMatchingStyleRules has a new CSS_Properties parameter available.
#define STYLE_CAP_GETMATCHINGSTYLERULES_CSS_PROPERTIES

// CSS_QuerySelector returns the exception to throw in an exception parameter.
#define STYLE_CAP_QUERYSELECTOR_EXCEPTION

// Module has PropertyIsInherited method.
#define STYLE_CAP_PROPERTYISINHERITED

// Uses WebFontManager::AddCSSWebFont with FramesDocument
#define STYLE_CAP_USE_NEW_ADDCSSWEBFONT

// Has CSS_FontfaceRule::GetFontNumber
#define STYLE_CAP_HAS_WEBFONT_GETFONTNUMBER

// Has CSSCollection::AddSVGFont
#define STYLE_CAP_HAS_ADDSVGFONT

// CSS_SvgFont::Contruct allocates fontnumber for SVG font
#define STYLE_CAP_CSS_SVGFONT_ALLOCATES_FONTNUMBER

// CSSCollection has MarkAffectedElementsPropsDirty (moved from logdoc).
#define STYLE_CAP_MARKAFFECTEDELMS

// Has CSS_PROPERTY_buffered_rendering
#define STYLE_CAP_HAS_BUFREND_PROP

// Support for -o-tab-size
#define STYLE_CAP_O_TAB_SIZE

// CSSCollection has SetFramesDocument and stores the doc pointer
// There's no need for the HLDocProfile parameter in the method calls
// anymore and they're therefore removed.
#define STYLE_CAP_CSSCOLL_HAS_DOC

// Has internal value CSS_VALUE_use_lang_def for font-family which will be set by the default stylesheet.
#define STYLE_CAP_MULTISTYLE_AWARE

// Support for word-wrap property and break-word value (CSS3 text)
#define STYLE_CAP_WORD_WRAP

// Has CSS_PROPERTY_box_shadow, properties for border radius
// (shorthand + individual properties) and CSS_PROPERTY__o_border_image.
// Also has support for parsing lists of background properties.
#define STYLE_CAP_HAS_CSS3_BACKGROUND

// Ready to set default vertical-align values on THEAD, TBODY, TFOOT, TR, TH and TD
// more or less as defined in http://www.w3.org/TR/CSS21/sample.html
#define STYLE_CAP_CSS21_TABLE_VALIGN

// CSS_ElementTransitions class has DeleteFinishedTransitions method.
#define STYLE_CAP_DELETE_FINISHED_TRANSITIONS

// TemplateHandler not used anymore
#define STYLE_CAP_NO_MORE_REPEAT_TEMPLATES

// CSS_decl::CreateCopy() exists for everyone (not protected by ifdef)
#define STYLE_CAP_CSS_DECL_COPY

// *:hover will no longer match anything but links in quirks mode so
// the parser can start parsing correctly
#define STYLE_CAP_RESTRICTED_LINK_HOVER

// Ready to separate HTML parsing from layout. See SEPARATE_PARSING_AND_LAYOUT define in logdoc_module.h
#define STYLE_CAP_SEPARATE_PARSING_AND_LAYOUT

// end of capability defines.

#endif // STYLE_CAPABILITIES_H
