/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef DOC_CAPABILITIES_H
#define DOC_CAPABILITIES_H

#define DOC_CAP_LAYOUT_MODE
// Layout engine takes different layout modes (LAYOUT_NORMAL, LAYOUT_SSR, LAYOUT_MSR)

#define DOC_CAP_LAYOUT_MODE_EXT
// Layout engine takes different layout modes (LAYOUT_NORMAL, LAYOUT_SSR, LAYOUT_CSSR, LAYOUT_AMSR, LAYOUT_MSR)

#define DOC_CAP_ESOPEN_SUPPORTS_CONTENT_TYPE
// FramesDocument::ESOpen supports an argument specifying a content type.  Used to implement
// support for the mimeType argument to document.write in DOM.

#define DOC_CAP_SCROLLTORECT_ALIGNMENT
// HTML_Document::ScrollToRect takes a new parameter.

#define DOC_CAP_GET_MEDIA_TYPE
// FramesDocument::GetMediaType exists and should be used to get the
// media type to send to LoadCssProperties

#define DOC_CAP_WF2CC
// New InputTypes from the Call-for-Comments version of WebForms2 is
// included

#define DOC_CAP_WF2_EXTERNALFORMLOAD
// The WebForms2 method LoadExternalData has an extra attribute, the URL to
// load from. Previously we always loaded from the URL in the data attribute.

#define DOC_CAP_REMOVE_ELEM_FROM_COUNTERS
// Class FramesDocument has method RemoveElementFromCounters.

#define DOC_CAP_HAS_HIGHLIGHT_FUNCTIONS
// Highlight functionality moved to doc from dochand/win

// Has ScrollToRect and GetCurrentArrowCursor
#define DOC_CAP_SETCURSOR_AND_SCROLLTORECT_FIX

#define DOC_CAP_LOWAPPLYPSEUDOCLASSES_NO_UPDATE
// FramesDocument::LowApplyPseudoClasses takes a 'no_update' argument.

#define DOC_CAP_SET_NEW_URL
// Has FramesDocument::SetNewUrl method.

#define DOC_CAP_ESNEEDTERMINATION
// Has FramesDocument::ESNeedTermination method.

#define DOC_CAP_ISGENERATEDDOCUMENT
// Has FramesDocument::IsGeneratedDocument method.

#define DOC_CAP_CREATEESENVIRONMENT_FORCE
// FramesDocument::CreateESEnvironment has force argument.

#define DOC_CAP_HAS_LOADING_REFUSED
// Has the LOADING_REFUSED value in LoadInlineStatus

#define DOC_CAP_STIGHAL_MERGE_1
// self explanatory

#define DOC_CAP_SUPPRESS_SUPPORTS_URL
// Document::GetSuppress supports a URL parameter

#define DOC_CAP_CLEARSELECTION_SETFOCUSED_ELEMENT
// Has clear_selection in SetFocusedElement

#define DOC_CAP_NEGATIVE_OVERFLOW
// The document may overflow to the left (Document::NegativeOverflow API)

#define DOC_CAP_MOVED_GLOBALS
// the selection_scroll_active and m_selection_scroll_document globals have been moved to the doc_module object

#define DOC_CAP_STOPLOADINGINLINE
// has FramesDocument::StopLoadingInline(const URL &, ExternalInlineListener *)

#define DOC_CAP_HAS_SPECIAL_ATTRS
// has special attribute constants in doctypes.h

#define DOC_CAP_MOUSEOVERURL_ESTHREAD
// Document::MouseOverURL supports an ES_Thread* parameter

#define DOC_CAP_ASYNC_TEMPLATE_REMOVAL
// Has FramesDocument::AddPendingTemplateRemoval

#define DOC_CAP_SETSELECTION_PLACE_CARET
// Has BOOL place_caret_at_endpoint in SetSelection

#define DOC_CAP_GETELEMENTWITHSELECTION
// Has HTML_Document::GetElementWithSelection()

#define DOC_CAP_FOCUS_ORIGIN
// Has HTML_Document::HighlightElement variant with 'origin' argument

#define DOC_CAP_AGGRESSIVE_WORD_BREAKING
// Has FramesDocument::GetAggressiveWordBreaking method

#define DOC_CAP_USER_AUTO_RELOAD
// has FramesDocument::ESOpenURL(..., BOOL user_auto_reload) needed for #182400

#define DOC_CAP_FRM_DOC_HAS_IS_INLINE_FRAME
// Has FramesDocument::IsInlineFrame (convenience function)

#define DOC_CAP_CREATE_PRINT_LAYOUT_TAKES_PREVIEW_FLAG
// FramesDocument::CreatePrintLayout takes an extra 'preview' flag (for iframe printing)

#define DOC_CAP_DOM_SIGNAL_DOCUMENT_FINISHED
// FramesDocument::DOMSignalDocumentFinished is supported.

#define DOC_CAP_FORM_VALUE_SERIAL_NR
// FramesDocument::GetNewFormValueSerialNr is supported.

#define DOC_CAP_RESTORE_FORM_STATE
// FramesDocument supports the form state restoration related functions.

#define DOC_CAP_HISTORY_NAVIGATION_MODE
// FramesDocument supports different history navigation modes.

#define DOC_CAP_SIGNAL_ES_RESTING
// FramesDocument::SignalESResting is supported.

#define DOC_CAP_DOC_HANDLES_INPUTACTIONS
// FramesDocument::OnInputAction() is available

#define DOC_CAP_AUTOFOCUS
// FramesDocument has flags for autofocus (from webforms2) handling

#define DOC_CAP_ADBLOCK_SUPPORT_METHODS
// Has the support methods used by the content blocker in edit mode

#define DOC_CAP_MOVED_COUNTERS_TO_LAYOUT
// CSS Counters move to layout module

#define DOC_CAP_MOVE_TO_LAYOUT_1
// Moved several small functions to layout

#define DOC_CAP_FORM_EVENTS_SENT_BY_LOGDOC
// DOM events for form controls are sent by logdoc, not by widgets

// there is an optional argument to the FocusElement(..., BOOL allow_scroll)
// to make it possible to surpress scrolling when focusing elements
#define DOC_CAP_FOCUS_SCROLL

// Document::MouseWheel is supported.
#define DOC_CAP_DOCUMENT_MOUSEWHEEL

// If this and FORMS_CAP_FORMMANAGER is defined,
// then the ExternalFormLoad class has moved to the forms module
#define DOC_CAP_MOVED_EXTERNALFORMLOAD

// API changes to support pagebreaking inside iframes
#define DOC_CAP_IFRAME_PAGEBREAKING

// FramesDocument::IsESGeneratingDocument is supported.
#define DOC_CAP_IS_GENERATING_DOCUMENT

// New version of FramesDocument::LoadInline with less unused arguments and a load_silent argument.
#define DOC_CAP_NEW_LOADINLINE

// Searching in all frames is enabled, The SearchData class used for that exists.
#define DOC_CAP_MATCH_IN_ALL_FRAMES

// Willing to store the old URL for an image in HEListElm (when an image is
// replaced via CSS or a script) while the new URL is loading
#define DOC_CAP_OLDIMG_IN_HELISTELM

// FramesDocument::AskedAboutFlashDownload() and FramesDocument::SetAskedAboutFlashDownload() is supported.
#define DOC_CAP_ASK_ABOUT_FLASH_DOWNLOAD

// FramesDocument::GetMaxParagraphWidth()
#define DOC_CAP_LIMIT_PARAGRAPH_WIDTH

// FramesDocument::LocalLoadInline() returns LOADING_STARTED in one case where it used to return LOADING_CANCELLED.
#define DOC_CAP_LOADINLINE_CORRECT_RET

// Document::GetURLForViewSource is supported, and should be used for "View source" functionality.
#define DOC_CAP_GETURLFORVIEWSOURCE

// Document::GetDesignMode is supported
#define DOC_CAP_GETDESIGNMODE

// Possibility to override media style in ERA
#define DOC_CAP_OVERRIDE_MEDIA_STYLE

// LoadInlineElm knows its UrlImageContentProvider (and keeps it referenced)
#define DOC_CAP_LOADINLINEELM_KNOWS_CONTENTPROVIDER

// The Visisted page search handler code is present
#define DOC_CAP_HAS_VISITED_PAGE_SEARCH

// MouseAction includes a sequence_count in the button parameter and
// MouseUp and MouseDown has a sequence_count parameter.
#define DOC_CAP_MOUSE_ACTION_HAS_SEQUENCE_COUNT

// FramesDocument::HandleEventFinished has svg hook
#define DOC_CAP_USE_SVG_HANDLE_EVENT_FINISHED

// HTML_Document has a hash for mapping elements to search results
#define DOC_CAP_HAS_SEARCH_HASHMAP

// doc has code for checking trust rating of documents
#define DOC_CAP_HAS_TRUST_RATING

// FramesDocument::MarkContainersDirty
#define DOC_CAP_FRAMES_DOCUMENT_MARK_CONTAINERS_DIRTY

// has CheckTrustRating with force_check and offline parameters
#define DOC_CAP_HAS_CHECK_TRUST_RATING_FORCE

// Using LayoutProperties::GetProps to access HTMLayoutProperties. For cached props
#define DOC_CAP_USE_LAYOUTPROPERTIES_GETPROPS

#define DOC_CAP_FOCUS_AFTER_EVENT_HANDLING

// searchinfo.h exists
#define DOC_CAP_SEARCHINFO

// FramesDocument::SetVisibleRect is supported.
#define DOC_CAP_FRAMESDOCUMENT_SETVISIBLERECT

// Document::MouseOverURL takes a HTML_Element parameter
#define DOC_CAP_MOUSEOVERURL_TAKES_ELEMENT

// FramesDocument::SetIsGeneratedDoc() exists
#define DOC_CAP_HAS_SETISGENERATEDDOC

// fraud checking code removed from doc (moved to dochand)
#define DOC_CAP_FRAUD_CHECK_CODE_MOVED

// The loading aborted feature implemented, *Doc::StopLoading() takes two params
#define DOC_CAP_LOADING_ABORTED

// FramesDocument::SetTextScale for zooming text only
#define DOC_CAP_TEXT_SCALE

// [Frames]Document::Title() has a TempBuffer parameter
#define DOC_CAP_TITLE_TEMPBUFFER

// FramesDocument::LoadInline() has from_user_css parameter.
#define DOC_CAP_LOADINLINE_FROMUSERCSS

// FramesDocument::ImagesDecoded() is supported.
#define DOC_CAP_HAS_IMAGES_DECODED

// has FramesDocument::WasAboutBlank()
#define DOC_CAP_ABOUTBLANKFLAGS

// Doesn't call LogicalDocument::SetTitle anymore and
// FramesDocument::SetTitle doesn't exist
#define DOC_CAP_REMOVED_SET_TITLE

// FramesDocument::ERA_ModeChanged exists
#define DOC_CAP_FRAMESDOCUMENT_ERA_MODE_CHANGED

// HTML_Document::CleanReferencesToElement exists
#define DOC_CAP_CLEANREFERENCESTOELEMENT

// FramesDocument::RemoveLayoutCacheFromSelection exists and doc has stopped calling TextSelection::MarkDirty
#define DOC_CAP_RESILIANT_SELECTION

// Embed can be an iframe element, if the type attribute is set to be handled by opera
#define DOC_CAP_EMBED_CAN_BE_TREATED_AS_IFRAME

// FramesDocument::DOMGetActiveElement exists
#define DOC_CAP_DOMGETACTIVEELEMENT

// Ready for lazy property (re-)loading, through HTML_Element::MarkPropsDirty().
// Aware that MarkExtraDirty() doesn't cause property reload.
// The layout module enables this functionality by defining LAZY_LOADPROPS when ready
#define DOC_CAP_LAZY_LOADPROPS

// FramesDocument::BubbleHoverToParent exists
#define DOC_CAP_BUBBLE_HOVER

// HTML_Document::[GS]etNavigationElement should be used in preference to assorted - now deprecated - older forms.
#define DOC_CAP_USE_NAVIGATION

// HEListElm and FramesDocument::FlushInlineElement handles images with generic inlines
#define DOC_CAP_HANDLES_GENERIC_INLINE

// FramesDocument::GetOBML2D_Data() exists - added on core-2, october 2007
#define DOC_CAP_HAS_GET_OBML2D_DATA

// HTML_Document::SetHoverHTMLElement has parameter for not changing element to css-styling
#define DOC_CAP_HAS_SET_HOVER_ELM_WITHOUT_STYLING

// FramesDocument::CheckERA_LayoutMode handles setting frame policy
// for flex root instead of assuming that the frame policy of flex
// root is handled in the default frame policy for the document
#define DOC_CAP_CHECKERA_HANDLES_FLEX_FRAMES

// doc module imports the extract url, so that the code doesn't always have to be
// defined
#define DOC_CAP_IMPORTS_EXTRACT_URL

// the doc module uses stored pointers to FramesDocElm in HTML_Element attributes
#define DOC_CAP_USES_FDELM_POINTERS

// Supports migrating threads that generate new documents.
#define DOC_CAP_THREAD_MIGRATION

// HTML_Document::GetURLTargetElement()
#define DOC_CAP_GETURLTARGETELM

// GetNewScrollPos moved from HTML_Document to FramesDocument
#define DOC_CAP_FRMDOC_HAS_GETNEWSCROLLPOS

// HTML_Document::SetHoverHTMLElement is ready to handle which elements should have ApplyPropertyChanges called
// when the hover element changes.
#define DOC_CAP_HOVER_TOP_PSEUDO_ELM

// The enum that GetNewScrollPos and some other methods use has been made global.
#define DOC_CAP_FRMDOC_HAS_GETNEWSCROLLPOS2

// A HandleKeyEvent() method that tells whether the event was handled or not exists
#define DOC_CAP_HANDLEEVENT_RETURNS_BOOL

// The matched document in SearchData is a FramesDocument
#define DOC_CAP_SEARCH_DOC_IS_FRAMESDOC

// FramesDocument::GetFirstInvalidCharacterOffset()
#define DOC_CAP_FIRST_INVALID_CHARACTER_OFFSET

// FramesDocument::SetAsCurrentDoc() has the visible_if_current parameter
#define DOC_CAP_SETASCURRENT_HAS_VISIBLE

// ExternalInlineListener callback functions take a FramesDocument* argument
#define DOC_CAP_EXTERNALINLINELISTENER_DOCUMENTc

// FramesDocument::ShouldCommitNewStyle() exists
#define DOC_CAP_COMMIT_NEW_STYLE

// FramesDocument is prepared to let ES_LoadManager fire the load event to external script elements.
#define DOC_CAP_NO_ONLOAD_TO_SCRIPT

// FramesDocument::CommitAddedCSS exists
#define DOC_CAP_COMMIT_ADDED_CSS

// SCROLL_ALIGN_SPOTLIGHT exists
#define DOC_CAP_HAS_SPOTLIGHT_SCROLL

// DisplayedDoc and DocDestructed is called with either HTML_Document or FramesDocument pointers. Used to be Document pointers.
#define DOC_CAP_TYPED_DOC_DESTRUCTION

// FramesDocument::FinishDocument() calls FinishDocument() on parent documents
#define DOC_CAP_FINISHDOCUMENT_PROPAGATES

// FramesDocument::GetCompatibleHistoryNavigationNeeded() is supported.
#define DOC_CAP_GETCOMPATIBLEHISTORYNAVIGATIONNEEDED

// LogicalDocument::Title(NULL) is not called anymore.
#define DOC_CAP_NOT_CALLING_TITLE_SANS_TEMPBUFFER

// Prepared for WindowCommander::GetSecurityModeFromState() becoming non-static.
#define DOC_CAP_USE_NONSTATIC_WIC_GETSECMODE

// HTML_Document::GetHTML_Element()  takes a bool determining whether it should include text nodes.
#define DOC_CAP_GETHTML_ELEMENT_TEXT

// Improvements in printing of iframes. Like not crashing or leaking whole document trees.
#define DOC_CAP_IMPROVED_IFRAME_PRINTING

// Document::GetSelectedText(OpString) has moved to FramesDocument
#define DOC_CAP_FRM_DOC_HAS_GETSELECTEDTEXT

// *Document::*SelectedText takes selections in form elements into accound. So it's
// now possible to use GetSelectedText() to get the selection from a text field.
#define DOC_CAP_SELECTIONS_IN_FORMS

// A number of FramesDocument methods now iterate over their child
// documents themselves instead of recursing through
// dochand/FramesDocElm
#define DOC_CAP_ITERATE_DOCUMENT

// DocumentFormSubmitCallback implements GetTargetWindowCommander()
#define DOC_CAP_GET_TARGET_WIC

// Calls the OBML2DData-constructor with frm_doc parameter
#define DOC_CAP_CREATE_OBML_WITH_FRMDOC

// FocusElement also sets the navigation element even if do_highlight is FALSE
#define DOC_CAP_FOCUS_ELEMENT_NAV_FIX

// FramesDocument::IsAborted() exists
#define DOC_CAP_HAS_ISABORTED

// The classes LoadInlineApplet and LoadInlineAppletHandle have been defined
#define DOC_CAP_APPLET_LIE_HANDLE

// FramesDocument::SetHasXMLEvents and FramesDocument::HasXMLEvents exist.
#define DOC_CAP_HAS_XML_EVENTS

// There is a FramesDocument::ScrollToRect method
#define DOC_CAP_FRMDOC_HAS_SCROLLTORECT

// The function FramesDocument::SetAlwaysCreateDOMEnvironment() is supported.
#define DOC_CAP_SET_ALWAYS_CREATE_DOM_ENVIRONMENT

// HTML_Document::UpdateHover now dows nothing and the internal functionality moved to an internal function
#define DOC_CAP_REMOVED_UPDATE_HOVER

// Doc has code to initiate a context menu
#define DOC_CAP_INITIATE_CONTEXT_MENU

// FramesDocument::CleanReferencesToElement exists
#define DOC_CAP_CLEAN_ELEMENT_REFERENCES

// FramesDocument::ContainsOnlyOperaInternalScripts exists
#define DOC_CAP_CONTAINS_ONLY_OPERA_INTERNAL_SCRIPTS

// FramesDocument handles loading of feeds by itself if WEBFEEDS_DISPLAY_SUPPORT is available
#define DOC_CAP_LOADS_FEEDS

// fetches PresentationAttr's font via PresentationFont using WritingSystem
#define DOC_CAP_MULTISTYLE_AWARE

// The doc module understand the border image inline type
#define DOC_CAP_HAS_BORDER_IMAGE

// FramesDocument::DragonflyInspect exists
#define DOC_CAP_DRAGONFLY_INSPECT

// FramesDocument::LoadInline(URL, listener, BOOL) has a reload flag
#define DOC_CAP_LOADINLINE_HAS_RELOAD

// New MAKE_SEQUENCE_COUNT_CLICK_INDICATOR_AND_BUTTON and EXTRACT_CLICK_INDICATOR macros
#define DOC_CAP_HAVE_CLICK_INDICATOR

// Has HTML_Document::ScrollToSavedElement, to scroll to a specific element after reflow
#define DOC_CAP_SCROLL_TO_SAVED_ELEMENT

// FramesDocument::HandleWindowEvent() has additional parameters which allow to handle custom DOM events targeted to window
#define DOC_CAP_CUSTOM_WINDOW_EVENTS

// FramesDocument::ESOpen takes care of document.domain migration
#define DOC_CAP_ESOPEN_DOMAIN_MIGRATING

// FramesDocument methods for svg contextmenus
#define DOC_CAP_SVG_CONTEXTMENU_ADDITIONS

// Cleared out use of the repeat template code
#define DOC_CAP_NO_MORE_REPEAT_TEMPLATES

// Can do svg highlighting
#define DOC_CAP_SVG_HIGHLIGHTING

// Ready to separate HTML parsing from layout. See SEPARATE_PARSING_AND_LAYOUT define in logdoc_module.h
#define DOC_CAP_SEPARATE_PARSING_AND_LAYOUT

// FramesDocument::SetLayoutViewPos() returns an OpRect that specifies the area that has
// was scheduled for repaint.
#define DOC_CAP_SETLAYOUTVIEWPOS_RETURNS_AREA

// FramesDocument::HighlightNextMatch will return coordinates for each hit
#define DOC_CAP_HIGHLIGHTNEXTMATCH_RETURNS_RECT

// FramesDocument::HandleMidClick exists if WIC_MIDCLICK_SUPPORT is enabled
#define DOC_CAP_HAS_HANDLEMIDDLECLICK

// FramesDocument:: and HTML_Document::RestoreForms has been renamed to ReactivateDocument
#define DOC_CAP_HAS_REACTIVATE_DOCUMENT

// FramesDocument has Add/RemoveMediaElement
#define DOC_CAP_HAS_ADD_REMOVE_MEDIA_ELEMENT

// FramesDocument::ESWrite and ESOpen can handle interrupted pointers between documents
#define DOC_CAP_CROSSDOCUMENTINTERRUPT

// FramesDocument methods for media context menus
#define DOC_CAP_MEDIA_CONTEXT_MENU

// Doc doesn't use the old SecurityManager API for Unite access
#define DOC_CAP_STOPPED_USING_IS_SAME_UNITE

/* The form seeding code is removed */
#define DOC_CAP_REMOVED_FORM_SEEDING

#endif // DOC_CAPABILITIES_H
