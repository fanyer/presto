/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef LOGDOC_CAPABILITIES_H
#define LOGDOC_CAPABILITIES_H

#define LOGDOC_CAP_CSS_LINK_NS_CHANGE
// The special attr ATTR_CSS_LINK has changed namespace to SpecialNs::NS_LOGDOC

#define LOGDOC_CAP_DOMAPI_CHANGES_1
/* DOM API changes between aristotle_1_beta_3 and aristotle_1_beta_4. */

#define LOGDOC_CAP_DOMAPI_CHANGES_2
/* DOM API changes between aristotle_1_beta_9 and aristotle_1_beta_10. */

#define LOGDOC_CAP_MATCHKIND_PUBLIC
/* LinkElement::MatchKind method made public. For common parsing of rel attribute. */

#define LOGDOC_CAP_FILE_SPLITTING
// Splitting the existing files into smaller ones

#define LOGDOC_CAP_USE_SPECIAL_ATTRS
// Some attributes are moved to the special namespace

#define LOGDOC_CAP_DOMAPI_SELECTCONTENTS
/* HTML_Element::DOMSelectContents exists. */

#define LOGDOC_CAP_DOMAPI_RELATIVEFOUND_ARG
// HTML_Element::DOMGetDistanceToRelative has relative_found argument.

#define LOGDOC_CAP_DOMAPI_REMOVEATTR_STATUS
// HTML_Element::DOMRemoveAttribute returns OP_STATUS (not void).

#define LOGDOC_CAP_DOMGETATTR_WITH_TEMPBUF
// Has DOMGetAttributeName with fix for returning lowercase attribute names for xhtml attributes.

#define LOGDOC_CAP_DOMAPI_LOOKUPNS
// HTML_Element::DOMLookupNamespace{Prefix,URI} exists.

#define LOGDOC_CAP_WF2CC
// The preliminary webforms 2 is replaced by the
// call-for-comments version of the specification

#define LOGDOC_CAP_INFERTARGET
// A new target page is opened for each new site

#define LOGDOC_CAP_DOMAPI_REMOVEALLCHILDREN
// Has HTML_Element::DOMRemoveAllChildren function.

#define LOGDOC_CAP_EXT_DATA_HANDLER_URL
// class ExternalHandlerData has additional member m_url

#define LOGDOC_CAP_CORE_1_MERGE_1
// API changes in aristotle_1_beta_22 as part of merge from core-1.

#define LOGDOC_CAP_URL_IS_FOLLOWED
// Has HLDocProfile::IsFollowed function.

#define LOGDOC_CAP_URL_IS_FOLLOWED_WITH_ELEMENT
// Has extra argument to HLDocProfile::IsFollowed function.

#define LOGDOC_CAP_HELISTELM_ONLOAD
// Has HEListElm::OnLoad function.

#define LOGDOC_CAP_URLIMAGECP_CLEANUP
// UrlImageContentProvider has moved and changed interface

#define LOGDOC_CAP_HAS_GETTABLE
// Has HTML_Element::GetTable

#define LOGDOC_CAP_XPATH_HASATTRIBUTE
// Has HTML_Element::XPathHasAttribute function.

#define LOGDOC_CAP_NAMED_ELEMENTS_MAP
// Has LogicalDocument::SearchNamedElements.

#define LOGDOC_CAP_GETAUTOCOMPLETIONOFF
// Has HTML_Element::GetAutocompleteOff

#define LOGDOC_CAP_URLMOVED
// Has UrlImageContentProvider::UrlMoved

#define LOGDOC_CAP_STATIC_URLMOVED
// Has static UrlImageContentProvider::UrlMoved

#define LOGDOC_CAP_HELIST_GETEVENTSENT
// Has HEListElm::GetEventSent method.

#define LOGDOC_CAP_STIGHAL_MERGE_1

#define LOGDOC_CAP_DOMAPI_GETELEMENTNAME
// Has HTML_Element::DOMGetElementName method.

#define LOGDOC_CAP_XMLDOCUMENTINFO
// Has HLDocProfile::GetXMLDocumentInfo method.

#define LOGDOC_CAP_CLEANED_UP_NAMESPACEMANAGER
// NamespaceManager's API has been cleaned up.

#define LOGDOC_CAP_GET_NEXT_TEXT_FRAGMENT_LENGTH
// Use GetNextTextFragment() with length argument

#define LOGDOC_CAP_HAS_SVG_PROPS
// Has CSSPROPS_COMMON_SVG and handles svg properties

#define LOGDOC_CAP_REMOVED_STOP_AFTER
// No longer uses info.stop_after

// Has HTML_Element::{Get,Set}{Selection,CaretPosition} functions.
#define LOGDOC_CAP_DOMAPI_SELECTION_AND_CARET

// HLDocProfile::GetCSS_PageProperties has media type parameter.
#define LOGDOC_CAP_HAS_PAGE_PROPS_MEDIA_PARAM

// Has HTML_Element::ClearCheckForPseudo method.
#define LOGDOC_CAP_CLEAR_CHECK_FOR_PSEUDO

// Has LogicalDocument::XSLTSetHTMLOutput.
#define LOGDOC_CAP_XSLT_HTML_OUTPUT

// Has HTML_Element::XSLTCreateProcessingInstruction.
#define LOGDOC_CAP_XSLT_CREATE_PROCESSINGINSTRUCTION

// Has HTML_Element::XSLTHasSpacePreserve.
#define LOGDOC_CAP_XSLT_HAS_SPACE_PRESERVE

// Has OpElementCallback class.
#define LOGDOC_CAP_OPELEMENTCALLBACK

// HTML_Element::HandleEvent supports an ES_Thread* parameter.
#define LOGDOC_CAP_HANDLEEVENT_ESTHREAD

// HLDocProfile Has SetHasMediaStyle and HasMediaStyle for generic
// respond-to-media-type support.
#define LOGDOC_CAP_HAS_MEDIA_STYLE

// LogicalDocument::ApplyXSLTStylesheets() exists.
#define LOGDOC_CAP_APPLYXSLTSTYLESHEETS

// The font number is stored as a separate CssPropteryItem
#define LOGDOC_CAP_STORE_FONT_NUMBER_SEPARATELY

// HTML_Element::BelongsToForm exists to make it possible for controls
// to belong to several forms at once. Also, HTML_Element::FindFormElm
// and HTML_Element::NextFormElm takes a FramesDocument as argument and
// HTML_Element::NextFormElm uses a pointer to the form tag rather than
// the form number.
#define LOGDOC_CAP_SHARED_FORM_CONTROLS

// HTML_Element::ParentActual() takes a BOOL to indicate whether we
// should take the last_descendant structure into account
#define LOGDOC_CAP_PARENT_ACTUAL_USES_LASTDESCENDANT

// OMF is always supported if M2_SUPPORT is defined (FEATURE_M2).
#define LOGDOC_CAP_ALWAYS_OMF_IN_M2

// HE_INSERTED_BY_SVG exists
#define LOGDOC_CAP_INSERTED_BY_SVG

// HTML_Element::DOMElementLoaded is supported.
#define LOGDOC_CAP_DOMELEMENTLOADED

// Supports experimental -o-text-overflow property with "ellipsis"
// as only supported value.
#define LOGDOC_CAP_HAS_O_TEXT_OVERFLOW

/* The ls and ts members of the StyleCreateInfo struct may provide negative
   values to the corresponding members in LayoutAttr, in which case they should
   be interpreted as (-x) / 24 em, instead of (x) px during layout. */
#define LOGDOC_CAP_RELATIVE_DEFAULT_MARGINS

/* Supports the function LogicalDocument::IsXmlParsingFailed. */
#define LOGDOC_CAP_IS_XML_PARSING_FAILED

// Has HTML_Element::GetElementTitle to get the most suitable title attribute
#define LOGDOC_CAP_GET_ELEMENT_TITLE

// Has ATTR_IME_STYLING
#define LOGDOC_CAP_ATTR_IME_STYLING

// Has ATTR_ALTERNATE (for <?xml-stylesheet alternate="yes"?>)
#define LOGDOC_CAP_ATTR_ALTERNATE

// Has HTML_AttrIterator::GetNext(int& attr, int& ns_idx, void*& obj, ItemType& item_type)
#define LOGDOC_CAP_HAS_OBJECT_ATTRIBUTE_ITERATOR

// Supports HE_TEXTGROUP elements.
#define LOGDOC_CAP_TEXTGROUP

// Supports HTML_Element::DOMHasTextContent function.
#define LOGDOC_CAP_DOMHASTEXTCONTENT

// Supports HTML_Element::{Under,Precede,Follow}Safe functions.
#define LOGDOC_CAP_SAFE_INSERTION

// InitStylesL() has been moved from logdoc to layout
#define LOGDOC_CAP_INITSTYLES_REMOVED

// Supports HTML_Element::SetText with FramesDocument argument.
#define LOGDOC_CAP_SETTEXT_FRAMESDOCUMENT

// Supports HTML_Element::GetOptionText.
#define LOGDOC_CAP_GETOPTIONTEXT

// Supports HTML_Element::ReplaceFormValue.
#define LOGDOC_CAP_REPLACEFORMVALUE

// New transaction based accesskey selection for supporting mix of space and comma separated lists
#define LOGDOC_CAP_NEW_ACCESSKEY_SELECTION

// Has ENCLOSING_BOX type for GetBoxRect
#define LOGDOC_CAP_HAS_ENCLOSING_BOX

// Has HTML_AttrIterator::Reset.
#define LOGDOC_CAP_ATTRITERATOR_RESET

// Has HTML_Element::IsDisabled.
#define LOGDOC_CAP_HAS_ISDISABLED

// HTML attributes with URL values are stored as strings in the logical tree
#define LOGDOC_CAP_URLS_AS_STRINGS

// HTML attributes with URL values are stored as strings in the logical tree
#define LOGDOC_CAP_GET_URL_TAKES_LOGDOC

// HTML_Element::XPathGetAttributeName takes the extra arguments specified and id
#define LOGDOC_CAP_XPATHGETATTRIBUTENAME_SPECIFIED_ID

// The DOMGet*Attribute() methods takes a BOOL parameter that tells whether URL
// values should be resolved to an absolute URL or not
#define LOGDOC_CAP_ATTR_HAS_RESOLVE_PARAM

// opera:config can retreive the full path from a file input box
#define LOGDOC_CAP_OPERACONFIG_FILE_PATH

// HLDocProfile has GetCssErrorCount/IncCssErrorCount methods.
#define LOGDOC_CAP_CSS_ERROR_COUNT

/// The HTML_Element::SetCSSProperties() and AddGeneratedContent() takes a layout flag
#define LOGDOC_CAP_PREVENT_LAYOUT

// has opacity, inherit_opacity and opacity_set in FontProps struct
#define LOGDOC_CAP_HAS_OPACITY_IN_FONTPROPS

// Has the HTML_Element::DOMStepUpDownFormValue method
#define LOGDOC_CAP_STEPUPDOWN

// Uses the counters code in layout after the move
#define LOGDOC_CAP_USES_LAYOUT_COUNTERS

// Form mouse event is fixed so that DOM events are sent by logdoc,
// not by widgets.
#define LOGDOC_CAP_SENDS_ONCLICK_FOR_FORMS

// Moved several smaller functions from FramesDocument to LayoutWorkplace
#define LOGDOC_CAP_MOVE_TO_LAYOUT_1

// LogicalDocument::GetSVGWorkplace exists if SVG_CAP_WORKPLACE is also set.
#define LOGDOC_CAP_HAS_SVGWORKPLACE

// Uses GetNextTextFragment which takes OpFontInfo::CodePage argument
#define LOGDOC_CAP_USES_GNTF_WITH_CODEPAGE

// HTML_Element::GetResolvedObjectType with FramesDocument argument
#define LOGDOC_CAP_GETRESOLVEDOBJECTTYPE_WITH_DOC

// Proper support for getting (and setting, where applicable) the DHTML
// properties scrollLeft, scrollTop, clientWidth, clientHeight, offsetWidth
// and offsetHeight, so that the dom module doesn't need (duplicated) special
// handling for this.
#define LOGDOC_CAP_DHTML_SIZEPOS_PROPS

// HLDocProfile has root_overflow
#define LOGDOC_CAP_HAS_ROOT_OVERFLOW

// HLDocProfile has body_overflow
#define LOGDOC_CAP_HAS_BODY_OVERFLOW

// HEListElm keeps track of the URL for the previous image (when an image
// is replaced via CSS or a script) while the new URL is loading
#define LOGDOC_CAP_HELISTELM_OLDIMG

// New HTML_Element::DOMSetAttribute and DOMSet{Numeric,Boolean}Attribute functions.
#define LOGDOC_CAP_NEW_DOMSETATTRIBUTE

/* Supports delayed event handler registration. */
#define LOGDOC_CAP_DELAYED_SETEVENTHANDLER

/* Supports HTML_Element::XSLTCopyAttribute function. */
#define LOGDOC_CAP_XSLTCOPYATTRIBUTE

/* Supports contentEditable. */
#define LOGDOC_CAP_CONTENTEDITABLE

/* Possibility to save inlines in directory. Turn on with TWEAK_LOGDOC_SAVE_INLINES_IN_DIRECTORY */
#define LOGDOC_CAP_SAVE_INLINES_IN_DIR

// Margin, padding and bgpos properties can be split up to take larger values.
#define LOGDOC_CAP_MORE_SPLIT_CSSPROPS

/* Has PROPS_CHANGED_UPDATE_SIZE */
#define LOGDOC_CAP_CHANGED_UPDATE_SIZE

/* Has GetAllAccessKeys() */
#define LOGDOC_CAP_GETALLACCESSKEYS

/* Handles middle button clicks. */
#define LOGDOC_CAP_HANDLES_MIDCLICK

/* Has MarkContainersDirty, used for limit paragraph width feature */
#define LOGDOC_CAP_LIMIT_PARAGRAPH_WIDTH

/* Has a (second) HTML_Element::HasEventHandler, one with a "local_only" argument. */
#define LOGDOC_CAP_HASEVENTHANDLER

/** Has HTML_Element::HandleContentEditableAttributeChange */
#define LOGDOC_CAP_CONTENTEDITABLE_ATTR_CHANGED

// The LogicalDocument::ESFlush() method takes an ES_Thread parameter
#define LOGDOC_CAP_ESFLUSH_TAKES_THREAD

/* UrlImageContentProvider has methods: SetSVGImageRef, GetSVGImageRef and ResetImage */
#define LOGDOC_CAP_UICP_HAS_SETSVGIMAGEREF

// The contentEditable attribute is a string attribute.
#define LOGDOC_CAP_CONTENTEDITABLE_STRING

/* HTML_Element::GetBoxPos() works for inline boxes, so that positioned inline
   boxes can be used as the containing block for absolutely positioned
   descendants. */
#define LOGDOC_CAP_GETBOXPOS_INLINE

// Has property items for CSSPROPS_BG_SIZE.
#define LOGDOC_CAP_BG_SIZE_PROPS

// The HTML_Element::XSLTCreateElement method takes an additional "internal" argument.
#define LOGDOC_CAP_XSLT_CREATEELEMENT_INTERNAL

// LogicalDocument::Reflow and ::FinishLayout has moved to layout/LayoutWorkplace
#define LOGDOC_CAP_MOVED_REFLOW_AND_FINISHLAYOUT

// Moved HTML_Element::GetBoxRect to layout, Box::GetRect
#define LOGDOC_CAP_MOVED_GETBOXRECT

// Support for the CSS 2.0 property text-overflow
#define LOGDOC_CAP_HAS_TEXT_SHADOW

// 	Has HTML_Element::SetCSS_Style(CSS_property_list* prop_list) and uses StyleAttribute
// for ATTR_STYLE/Markup::SVGA_STYLE
#define LOGDOC_CAP_SET_CSS_STYLE

// The WML_Context class has IncRef() and DecRef() and uses reference counting
#define LOGDOC_CAP_REFCOUNTED_WMLCONTEXT

// Has CSS_COLOR_current_color
#define LOGDOC_CAP_CURRENT_COLOR

// The WML_Context class has the IsParsing() method
#define LOGDOC_CAP_HAS_WML_ISPARSING

// Has methods for setting/getting the pre-focused state of an HTML_Element.
#define LOGDOC_CAP_PREFOCUS

// Has HTML_Element::XSLTCreateTextGroup
#define LOGDOC_CAP_XSLTCREATETEXTGROUP

// Has the attribute code ATTR_WML_SHADOWING_TASK
#define LOGDOC_CAP_HAS_WML_SHADOW_ATTR

// Has HLDocProfile::HasMediaQueryChanged.
#define LOGDOC_CAP_HAS_MEDIA_QUERY_CHANGED

// Has CSSPROPS_NAVUP, CSSPROPS_NAVDOWN, CSSPROPS_NAVLEFT, CSSPROPS_NAVDOWN
#define LOGDOC_CAP_CSS3_NAVIGATION

// Don't declare Css...Map arrays in logdoc, since it is layout that defines them. Declaration will be provided by layout.h
#define LOGDOC_CAP_LAYOUT_CSSMAP_ARRAYS

// HandleEvent has an embedded sequence count in mouse events in
// HTML_Element::HandleEvent
#define LOGDOC_CAP_MOUSE_ACTION_HAS_SEQUENCE_COUNT

// CssPropertiesItemType type for -o-table-baseline exists
#define LOGDOC_CAP_TABLE_BASELINE

// The docutil module code is split up and spread out
#define LOGDOC_CAP_NO_DOCUTIL

// Has the HTML_Element::IsFocusable() method
#define LOGDOC_CAP_HAS_ISFOCUSABLE

// Using LayoutProperties::GetProps to access HTMLayoutProperties. For cached props
#define LOGDOC_CAP_USE_LAYOUTPROPERTIES_GETPROPS

// Mouse event handling has new regressions.
#define LOGDOC_CAP_FOCUS_AFTER_EVENT_HANDLING

// HLDocProfile::SetParseNotLayout takes an optional (full_stop = TRUE) flag
#define LOGDOC_CAP_SETPARSENOTLAYOUT_TAKES_FLAG

// Has LogicalDocument::GetXSLTStylesheet{sCount,URL}.
#define LOGDOC_CAP_GETXSLTSTYLESHEETURL

// Using abstracted SVGWorkplace class
#define LOGDOC_CAP_USE_ABSTRACTED_SVG_WORKPLACE

// Has AppletAttr derived from ComplexAttr to store OpApplet objects in shadowed dom tree for printing.
#define LOGDOC_CAP_HAS_APPLET_ATTR

// Has HTML_Element::DOMMarqueeStartStop(BOOL) and HTML_Element::IsMarqueeStopped(). HTML_Element::GetBGSOUND_Loop may return negative numbers. HTML_Element::GetImg_Loop is removed. ATTR_LOOP may contain negative numbers.
#define LOGDOC_CAP_MARQUEE_START_STOP

// Tell layout to force flushing of pending content before letting scripts trigger layout tree traversal.
#define LOGDOC_CAP_SCRIPTACCESS_FLUSH

// Has code for a DocumentRadioGroups in LogicalDocument and if
// FORMS_CAP_FORMRADIOGROUP is set it actually exists as well.
#define LOGDOC_CAP_RADIO_GROUPS

// HTML_Element::GetAnchorTags() tagkes a document parameter
#define LOGDOC_CAP_GETANCHORTAGS_TAKES_DOC

// AppletAttr stores OpPluginWindow in addition to OpApplet.
#define LOGDOC_CAP_APPLET_ATTR_WINDOW

// Has HTML_Element::PluginInitializedElement(HLDocProfile*) (#if defined(USER_JAVASCRIPT) && defined(_PLUGIN_SUPPORT_))
#define LOGDOC_CAP_PLUGININITIALIZEDELEMENT

// Has HTML_Element::DOM{Get,Set}StylesheetDisabled.
#define LOGDOC_CAP_DOMSTYLESHEETDISABLED

// HTML_Element::HasNumAttr exists which means that GetNumAttr will
// return a preprocessed number for this attribute. Otherwise
// GetNumAttr will return the default value 0.
#define LOGDOC_CAP_HASNUMATTR

// WML_Context::SubstituteVars() takes a OutputConverter parameter
#define LOGDOC_CAP_SUBSTVARS_TAKES_CONVERTER

// Module prepared to use CssPropertyItem and friends from layout module.
#define LOGDOC_CAP_NO_CSSPROPS

// LogicalDocument::Title() has a TempBuffer parameter
#define LOGDOC_CAP_TITLE_TEMPBUFFER

// WML_Context::UpdateWmlInput() returns an OP_STATUS
#define LOGDOC_CAP_UPATEWMLINPUT_OOM

// Quite a few things are shuffeled around to avoid complex globals
#define LOGDOC_CAP_BREW_GLOBALS

// HTML_Element has NextActualStyle and NextSiblingActualStyle.
#define LOGDOC_CAP_NEXT_ACTUAL_STYLE

// HTML_Element::GetTabIndex() exists and WML has ocrrect namespace for attributes
#define LOGDOC_CAP_HAS_GETTABINDEX

// HEListElm::LoadAll takes parameter decode_absolutely_all
#define LOGDOC_CAP_LOAD_ABS_ALL

// Can handle overflow_x and overflow_y instead of overflow from HTMLayoutProperties.
#define LOGDOC_CAP_LAYOUT_OVERFLOW_XY

// HTML_Element::GetLinkedElement takes an extra argument for alt text.
#define LOGDOC_CAP_GETLINKEDELEMENT_TAKES_ALT_TEXT_ARGUMENT

// HTML_Element::GetTabIndex() returns the fully computed value
#define LOGDOC_CAP_GETTABINDEX_IS_GOOD

// HTML_Element::SetCssProperties does not recurse to children by itself.
#define LOGDOC_CAP_SETCSSPROPERTIES_DOES_NOT_RECURSE

// There is a LogicalDocument::MakeParsed() for OBML2D
#define LOGDOC_CAP_MAKE_PARSED_FOR_OBML

// HTML_Element::CleanReferences calls
// HTML_Document::CleanReferencesToElement instead of trying to list
// all pointers by itself.
#define LOGDOC_CAP_CALLS_CLEANREFERENCESTOELEMENT

// Has the WML_Context::RestartParsing() method
#define LOGDOC_CAP_HAS_WML_RESTARTPARSING

// HTML_Element::SetUpdatePseudoElm and GetUpdatePseudoElm exists and
// HTML_Element::EndElement applies property changes if the bit is set.
#define LOGDOC_CAP_UPDATEPSEUDOELM

// Logdoc no longer does calls to MarkSelectionDirty (but has on
// the other hand started calling RemoveLayoutCacheFromSelection if available)
#define LOGDOC_CAP_STOPPED_DIRTYING_SELECTION

// Lots of extra arguments to HTML_Element::DOMSetContents and an
// HTML_Element::ValueModificationType enum
#define LOGDOC_CAP_INTELLIGENT_DOMSETCONTENTS

// HTML_Element::DOMMoveToOtherDocument is supported.
#define LOGDOC_CAP_DOMMOVETOOTHERDOCUMENT

// Has the SearchHelper class and LogicalDocument::FindAllMatches()
#define LOGDOC_CAP_HAS_SEARCH_CODE

// HLDocProfile::SetCharacterSet() checks if a charset that
// gives visual encoding has been set and changes BIDI status
#define LOGDOC_CAP_SETCHARSET_CHECKS_BIDI

// DOM_Environment::ElementCollectionStatusChanged is used (when
// available) to signal changes that affect DOM collections, instead
// of FramesDocument::IncreaseSerialNr.
#define LOGDOC_CAP_USES_ELEMENTCOLLECTIONSTATUSCHANGED

// Ready for lazy property (re-)loading support, through HTML_Element::MarkPropsDirty() and friends.
// The concept of "props preloaded" is gone.
// The layout module enables this functionality by defining LAZY_LOADPROPS when ready
#define LOGDOC_CAP_LAZY_LOADPROPS

// URLImageContentProvider knows the security state of the
// url it loaded the image from. To be used in case the url
// unloads its cached data.
#define LOGDOC_CAP_URLIMGPROVIDER_SECURITY

// Logdoc handles setting width and height of the <canvas>
#define LOGDOC_CAP_CANVAS_RESIZE

// HLDocProfile::AddAccesskeyFromStyle() exists and we no longer need to commit accesskeys
#define LOGDOC_CAP_NEW_ACCESSKEYS

// Has HTML_Element::DOMGetPositionAndSizeList and supports
// HTML_Element::DOM_PS_CONTENT in both DOMGetPositionAndSizeList and
// DOMGetPositionAndSize.
#define LOGDOC_CAP_HAS_DOMGETPSLIST

// HLDocProfile has GetCSSCollection which returns a const ptr to the CSSCollection object.
#define LOGDOC_CAP_GETCSSCOLLECTION

// The behaviour of HTML_Element::{Clean(),Free(),CleanLocal(),OutSafe()}
// has changed to avoid redundant work.
#define LOGDOC_CAP_BETTER_ELEMENT_CLEANUP

// HTML_Element has GetStyleAttribute method for retrieving the StyleAttribute object.
#define LOGDOC_CAP_GETSTYLEATTRIBUTE

// Understands the multi-module-capability SVGPROPS_ONDEMAND
#define LOGDOC_CAP_SVGPROPS_ONDEMAND

// HTML_Element::HandleEvent takes care of some double click actions that
// used to be done in display before scripts had their say
#define LOGDOC_CAP_HANDLES_DBLCLICK

// HTML_Element has GetResolvedImageType()
#define LOGDOC_CAP_HAS_GETRESOLVEDIMAGETYPE

// Defines the attribute ATTR_GENERIC_HE_LIST_ELM
#define LOGDOC_CAP_HAS_GENERIC_HE_LIST_ELM

// The FindAllMatches() functions takes a offset into the last active element
// for setting the next active result of a text search
#define LOGDOC_CAP_SEARCH_HAS_OFFSET

// XML_Events_Registration::MoveToOtherDocument can move the event to
// a NULL document (i.e. removing references to any document)
#define LOGDOC_CAP_XMLEVENTSMOVETONULL

// LogdocXMLTreeAccessor supports new variants of GetAttribute, GetAttributeIndex and
// HasAttribute that takes an XMLTreeAccessor::Attributes argument
#define LOGDOC_CAP_XMLTREEACCESSOR_NEW_ATTRIBUTE_ACCESS

// HTML_Element::SetInnerHTML/HTML_Element::DOMSetInnerHTML has an extra argument for
// specifying the actual parent element of the parsed HTML.
#define LOGDOC_CAP_SETINNERHTML_ACTUAL_PARENT

// HTML_Element::OutSafe
#define LOGDOC_CAP_CALLS_DOCEDIT_ONBEFOREELEMENTOUT

// All support of HTML DTD default attributes are removed
#define LOGDOC_CAP_NO_DEFAULT_ATTRIBUTES

// HE_DOCTYPE elements store XMLDocumentInformation objects as attributes, and those
// objects can be accessed using HTML_Element::GetXMLDocumentInfo().
#define LOGDOC_CAP_XMLDOCUMENTINFO_ATTRIBUTE

// HTML_Element::XSLTStripWhitespace is supported (with morph_2).
#define LOGDOC_CAP_XSLTSTRIPSPACE

// BoxRectType::BORDER_BOX exists
#define LOGDOC_CAP_HAS_BOXRECTTYPE_BORDER_BOX

// URL attributes are store as a class with both string and URL object
#define LOGDOC_CAP_URLS_AS_CLASS

// HTML_Element::GetTabIndex() has a FramesDocument argument.
#define LOGDOC_CAP_TABINDEX_DOC

// HTML_Element::Construct takes a flag telling if attributes need to have their entities decoded.
#define LOGDOC_CAP_ELEMENT_INIT_HAS_ENTITY_FLAG

// logdoc module imports the extract url, so that the code doesn't always have to be
// defined
#define LOGDOC_CAP_IMPORTS_EXTRACT_URL

// the logdoc module uses stored pointers to FramesDocElm in HTML_Element attributes
#define LOGDOC_CAP_USES_FDELMATTR

// Do our best to store the cascade between yields for better performance
#define LOGDOC_CAP_YIELD_KEEP_CASCADE

// HTML_Element handles the svg 'type' attributes (some, not all) as ITEM_TYPE_STRING.
#define LOGDOC_CAP_SVG_TYPE_ATTRIBUTE_AS_STRING

// HTML_Element::SetCssProperties takes an extra flag to create a layout box but not layout.
#define LOGDOC_CAP_SETCSSPROPERTIES_CREATEBOX_BUT_NOT_LAYOUT

/**
 * All HE_TEXTGROUP handling wrapped in 3 new (or modified) methods:
 * static HTML_Element::CreateText()
 * HTML_Element::AppendText()
 * HTML_Element::SetText().
 */
#define LOGDOC_CAP_WRAPPED_TEXT_HANDLING

// HTML_Element::HandleEvent() returns a BOOL to indicate whether the event was handled or not
#define LOGDOC_CAP_HANDLEEVENT_RETURNS_BOOL

// HTML_Element::CreateText now takes an option (unless it's WML) HLDocProfile argument.
#define LOGDOC_CAP_CREATE_TEXT_WITH_PROFILE

// HTML_Element::SetText takes informational data argument to make it
// possible to update ranges and selections better.
#define LOGDOC_CAP_SETTEXT_HAS_INFO

// LogdocXSLTHandler::CancelLoadResource() is not completely broken.
#define LOGDOC_CAP_FIXED_XSLTHANDLER_CANCELLOADRESOURCE

// logdoc stays away from EmbedContentType in layout
#define LOGDOC_CAP_NO_EMBEDCONTENTTYPE

// LogicalDocument::GetTitle exists.
#define LOGDOC_CAP_GET_TITLE_WITH_ROOT

// HTML_Element::DOMGetInputTypeString exists
#define LOGDOC_CAP_DOMGETINPUTTYPESTRING

// Has the attribute NOSCRIPT_ENABLED_ATTR.
#define LOGDOC_CAP_HAS_NOSCRIPT_ENABLED_ATTR

// HTML_Element::GetInnerBox() takes a bool determining whether it should include text boxes.
#define LOGDOC_CAP_GETINNERBOX_TEXT

// HTML_Element is prepared to let ES_LoadManager fire the readystatechange event to external script elements.
#define LOGDOC_CAP_NO_ONREADYSTATECHANGE_TO_SCRIPT

// Has GetCssBackgroundColorFromStyleAttr & similar
#define LOGDOC_CAP_FROMSTYLEATTR

// HTML_Element::CleanLocal calls RemoveFromSelection even if
// FEATURE_TEXT_SELECTION isn't enabled since text-selection still can
// be used for marking the navigated element.
#define LOGDOC_CAP_REMOVE_ELEMENT_FROM_SELECTION

// HLDocProfile has Set/GetHasHandheldDocType
#define LOGDOC_CAP_HLDOCPROF_HAS_HASHANDHELDDOCTYPE

// LogicalDocument::SetNotCompleted() is supported
#define LOGDOC_CAP_SETNOTCOMPLETED

// LogdocModule is prepared to transfer ownership of SharedCssManager to layout
#define LOGDOC_CAP_NO_SHAREDCSSMANAGER

// The Named Map functions are now always available, even when ecmascript is disabled
#define LOGDOC_CAP_ALWAYS_HAS_NAMED_MAP

// logdoc calls CSSCollection::InvalidateClassCache if present.
#define LOGDOC_CAP_INVALIDATE_CLASSCACHE

// search.cpp relied on includes in traverse.h that is moving out
#define LOGDOC_CAP_SEARCH_CPP_HAS_LOGDOC_UTIL_INCLUDE

// Has HTML_Element::IsLayoutFinished()
#define LOGDOC_CAP_FINISHEDLAYOUT

// The GetColourFromStyleAttr methods take a doc parameter
#define LOGDOC_CAP_BAD_COLOR_METHODS_TAKE_DOC

// Logdoc triggers an ONCONTEXTMENU from right clicks and context menus from ONCONTEXTMENU
#define LOGDOC_CAP_ONCONTEXTMENU

// GetPreferredScript is present
#define LOGDOC_CAP_MULTISTYLE_AWARE

// Will call CSSCollection::SetFramesDocument from HLDocProfile::SetFramesDocument if present.
#define LOGDOC_CAP_CSSCOLL_SETDOC

// The logdoc module has a border image inline type and a util function HTML_Element::AttributeFromInlineType
#define LOGDOC_CAP_HAS_BORDER_IMAGE

// Stores the background attribute in a StyleDeclaration as a faked css property.
#define LOGDOC_CAP_BACKGROUND_IN_ATTR

// SetParseNotLayout has been renamed to SuspendIncrementalLayout()
#define LOGDOC_CAP_SUSPEND_INCREMENTAL_LAYOUT

// SaveAsArchiveHelper::GetArchiveL is supported
#define LOGDOC_CAP_GET_INLINED_ARCHIVE

// Has stopped calling ChangeElement with an element if FORMS_CAP_HAS_APPLYPROPS is defined
#define LOGDOC_CAP_CALLS_RIGHT_CHANGE_ELEMENT

// Has HTML_Element::UpdateCursor
#define LOGDOC_CAP_UPDATE_CURSOR

// Has HTML_Element::DOMGetFrameProxyEnvironment (instead of
// DOMGetFrameEnvironment)
#define LOGDOC_CAP_DOMGETFRAMEPROXYENVIRONMENT

// provides GetFirstFontNumber taking script
#define LOGDOC_CAP_GETFIRSTFONTNUMBER_SCRIPT

// The repeat code is gone (attributes still there for some time)
#define LOGDOC_CAP_NO_MORE_REPEAT_TEMPLATES

// HTML_Element::IsDisplayingReplacedContent() exists
#define LOGDOC_CAP_DISPLAYING_REPLACED

// Ready to separate HTML parsing from layout. See SEPARATE_PARSING_AND_LAYOUT define in logdoc_module.h
#define LOGDOC_CAP_SEPARATE_PARSING_AND_LAYOUT

// Search text looks in svg content too
#define LOGDOC_CAP_SVG_TEXTSEARCH

// Supports the spellcheck attribute.
#define LOGDOC_CAP_HAS_SPELLCHECK_ATTR

// Return original size and saved size of a page (SaveAsArchiveHelper::Save)
#define LOGDOC_CAP_RETURN_ORIGINAL_SAVE_SIZE

// DOMLookupNamespacePrefix and DOMLookupNamespaceURI no longer depend on the DOM3_CORE define
#define LOGDOC_CAP_DOM3CORE_METHODS_ALWAYS_THERE

// Has ATTR_AUTOBUFFER
#define LOGDOC_CAP_ATTR_AUTOBUFFER

/* The form seeding code is removed */
#define LOGDOC_CAP_REMOVED_FORM_SEEDING

/* The new form-attributes for HTML5 (code named WebForms2) exists. */
#define LOGDOC_CAP_WF2_FORM_ATTRIBUTES

/* Has HTML_Element::SetObjectIsEmbed(). Used by WEB_TURBO_MODE */
#define LOGDOC_CAP_OBJECT_ELEMENT_IS_EMBED

#endif // !LOGDOC_CAPABILITIES
