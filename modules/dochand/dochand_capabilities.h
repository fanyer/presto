/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DOCHAND_CAPABILITIES_H
#define DOCHAND_CAPABILITIES_H

/* DocListElm class has SetUrl method. */
#define DOCHAND_CAP_DOCLISTELM_SET_URL

/* DocumentManager class has HasJSWindow method. */
#define DOCHAND_CAP_DOCMAN_HASJSWINDOW

/* WindowManager keeps track of the number of documents that have blinking elements. */
#define DOCHAND_CAP_DOCUMENT_WITH_BLINK

/* Window class has additional argument, bResetSecurityState, for StartProgressDisplay method. Fix for bug 77396*/
#define DOCHAND_CAP_SECURITY_STATE

/* DirectHistory class has Set/GetMostRecentTyped methods. */
#define DOCHAND_CAP_HAS_RECENTLY_TYPED_HISTORY

/* Window class has method ComposeLinkInformation */
#define DOCHAND_CAP_COMPOSE_LINK_INFORMATION

/* Has possibility to set frames policy for each window */
#define DOCHAND_CAP_PER_WINDOW_FRAMES_POLICY_SETTINGS

/* Has the Window::MarkNextItemInDirection() method */
#define DOCHAND_CAP_HAS_MARK_NEXT_ITEM

/* Has one more parameter for WMLDeWmlifyHistory() */
#define DOCHAND_CAP_NEW_DEWMLIFY_HISTORY

/* Has the *Active() methods for navigating frames */
#define DOCHAND_CAP_HAS_ACTIVE_FRAMES_METHODS

/* The Window::SetCursor() method takes two parameters */
#define DOCHAND_CAP_SETCURSOR_TAKES_TWO

/* ONLY USED DURING MERGE */
#define DOCHAND_CAP_STIGHAL_MERGE_1

/* The class DocumentTreeIterator is supported. */
#define DOCHAND_CAP_DOCUMENTTREEITERATOR

/* The function DocumentManager::JumpToRelativePosition is supported. */
#define DOCHAND_CAP_JUMPTORELPOS

/* Defines special attribute constants */
#define DOCHAND_CAP_HAS_SPECIAL_ATTRS

/* WindowManager::OpenURLNamedWindow takes ES_Thread* argument (defaults to NULL) */
#define DOCHAND_CAP_OPENURL_ESTHREAD

/* Has Window::IsCancelingLoading */
#define DOCHAND_CAP_ISCANCELINGLOADING

/* Has Window::{Is,Set}URLAlreadyRequested. */
#define DOCHAND_CAP_URLALREADYREQUESTED

/* The window type WIN_TYPE_DIALOG is supported. */
#define DOCHAND_CAP_WIN_TYPE_DIALOG

/* The window type WIN_TYPE_NEWSFEED_VIEW is supported. */
#define DOCHAND_CAP_WIN_TYPE_NEWSFEED

/* The global variables windowManager, globalHistory and directHistory now have
   the required g_ prefix. */
#define DOCHAND_CAP_RENAMED_GLOBALS

/* Window has IsFormattingPrintDoc(), IsPrinting() et al */
#define DOCHAND_CAP_ASYNC_PRINT_INFO

/* DocumentManager has GetDOMEnvironment(). */
#define DOCHAND_CAP_DOCMAN_GETDOMENVIRONMENT

/* Added WIN_TYPE_GADGET */
#define DOCHAND_CAP_WIN_TYPE_GADGET

/* Has Window::ForceJavaDisabled for EMBROWSER_SUPPORT */
#define DOCHAND_CAP_FORCE_JAVA_DISABLED

/* Has FramesDocElm::SetSubWinId. */
#define DOCHAND_CAP_FDELM_SETSUBWINID

/** Has DocListElm::current_page to remember current page in presentation mode. */
#define DOCHAND_CAP_CURRENT_PAGE

/* The global use_history_number flag in Window has finally died. */
#define DOCHAND_CAP_NO_GLOBAL_SETUSEHISTORYNUMBER

/* Cleaned up message handling between Window/WindowManager and MessageHandler. */
#define DOCHAND_CAP_MESSAGE_HANDLING_CLEANUP

/* FramesDocElm has GetPrintDoc */
#define DOCHAND_CAP_FDELM_HAS_GETPRINTDOC

/* the opener of a window is always stored, not just for scripts */
#define DOCHAND_CAP_ALWAYS_STORING_OPENER

/* DocumentManager::JumpToRelativePosition has 'from_onload' argument. */
#define DOCHAND_CAP_JUMPTORELPOS_FORM_ONLOAD

/* DocumentManager::StoreRequestThreadInfo is supported. */
#define DOCHAND_CAP_STORE_REQUEST_THREAD_INFO

/** Has WindowManager::*VisibleOnScreen() */
#define DOCHAND_CAP_VISIBLEONSCREEN

/** Window::IsEcmaScriptDisabled and Window::SetEcmaScriptDisabled is supported. */
#define DOCHAND_CAP_PER_WINDOW_ECMASCRIPT_DISABLE

/* Has extra arguments to Window::Reload and DocumentManager::GetConditionallyRequestInlines. */
#define DOCHAND_CAP_RELOAD_FLAGS

/* DocumentManager::ESGeneratingDocument is supported. */
#define DOCHAND_CAP_GENERATING_DOCUMENT_FINISHED

/* Window::AbortAnimatedGifs() is supported. */
#define DOCHAND_CAP_ABORT_ANIMATED_GIFS

/** Has Window::SetBackgroundTransparent and Window::IsBackgroundTransparent */
#define DOCHAND_CAP_TRANSPARENT_WINDOW

/** Has Get/SetLimitParagraphWidth() */
#define DOCHAND_CAP_LIMIT_PARAGRAPH_WIDTH

/** Has DocumentManager::OpenImageURL. */
#define DOCHAND_CAP_OPENIMAGEURL

/** Has new_page and in_background arguments to DocumentManager::OpenImageURL. */
#define DOCHAND_CAP_OPENIMAGEURL_NEWPAGE

/** Has FramesDocElm::HasExplicitZeroSize. */
#define DOCHAND_CAP_FRAMESTACKING_HAS_EXPLICIT_ZERO_SIZE

/** DocumentManager::SetReloadFlags has extended arguments. */
#define DOCHAND_CAP_EXTENDED_RELOAD_FLAGS

/** The functions FramesDocElm::{Set,}IsDeleted are supported. */
#define DOCHAND_CAP_FRAMESDOCELM_ISDELETED

/** GetCurrentURL() has been split into two new so that people don't get confused about "current" */
#define DOCHAND_CAP_HAS_2_GETCURRENTURLS

/** Window::IsEcmaScriptPaused and Window::SetEcmaScriptPaused is supported. */
#define DOCHAND_CAP_PER_WINDOW_ECMASCRIPT_PAUSE

/** Has Window::SetTrustRating() */
#define DOCHAND_CAP_HAS_SET_TRUSTRATING

/** The Window::IsMCIDisabled() methode exists */
#define DOCHAND_CAP_HAS_MCI_DISABLED

/** Window can handle locally overridden ScrollIsPan */
#define DOCHAND_CAP_SCROLL_IS_PAN_OVERRIDDEN

/** has error_page_shown variable and accessor functions */
#define DOCHAND_CAP_HAS_ERROR_PAGE_SHOWN

/** Window can get and set flex-root state */
#define DOCHAND_CAP_FLEX_ROOT

/* The window type WIN_TYPE_THUMBNAIL is supported. */
#define DOCHAND_CAP_WIN_TYPE_THUMBNAIL

/** FramesDocElm::SetVisibleRect is supported. */
#define DOCHAND_CAP_FRAMESDOCELM_SETVISIBLERECT

/** Fraud checking code lies in dochand now (used to be in doc) */
#define DOCHAND_CAP_HAS_NEW_FRAUD_CHECK_CODE

/** Window::SetIsImplicitSuppressWindow() exists. */
#define DOCHAND_CAP_WINDOW_SETISIMPLICITSUPPRESSWINDOW

/** Window::GetTextScale() and Window::SetTextScale() to zoom text. */
#define DOCHAND_CAP_TEXT_SCALE

/** dochand has MSG_DISPLAY_WEBFEED message, and will display feed if it gets message */
#define DOCHAND_CAP_HAS_DISPLAY_FEED

/** Window::{,Get}ForcePluginsDisabled are supported. */
#define DOCHAND_CAP_FORCE_PLUGINS_DISABLED

/* WindowManager::OpenURLNamedWindow takes plugin_unrequested_popup argument (defaults to FALSE). */
#define DOCHAND_CAP_OPENURL_PLUGINUNREQUESTEDPOPUP

/** Window_Type contains WIN_TYPE_MAIL_COMPOSE. */
#define DOCHAND_CAP_WINMAN_HAS_WIN_COMPOSE

/** WindowManager::OnlineModeChanged() is supported. */
#define DOCHAND_CAP_WINMAN_ONLINEMODECHANGED

// Window::IsContentBlockerEnabled() is available
#define DOCHAND_CAP_HAS_CONTENT_BLOCKER_ENABLED

/** FramesDocument::SetPendingJsRelocation is no longer called. */
#define DOCHAND_CAP_STOPPED_USING_PENDING_RELOCATION

/** Correctly ifdeffed MSG_SELECTION_SCROLL */
#define DOCHAND_CAP_IFDEFFED_MSGSELECTIONSCROLL

/** Window::MarkNext*InDirection functions take an optional third 'BOOL scroll' argument. */
#define DOCHAND_CAP_WINDOW_MARKNEXTINDIRECTION_SCROLL

/** FramesDocElm::GetFrameId exists */
#define DOCHAND_CAP_FRAMES_HAS_FRAMEID

/** Embed can be an iframe element, if the type attribute is set to be handled by opera */
#define DOCHAND_CAP_EMBED_CAN_BE_TREATED_AS_IFRAME

/** Window::SetDrawHighlightRects() is supported. */
#define DOCHAND_CAP_WINMAN_DISABLE_HIGHLIGHTS

/** DocumentManager can use WIC_USE_DOWNLOAD_CALLBACK */
#define DOCHAND_CAP_USE_DOWNLOAD_CALLBACK

/** The security level is not doubled because in-between levels are no longer in use. */
#define DOCHAND_CAP_NON_DOUBLED_SECURITY_LEVEL

/** The image cache is cleared at exit to free all unused images kept in the
 image cache. */
#define DOCHAND_CAP_CLEAR_IMAGE_CACHE_ON_EXIT

/** Wand is informed when the Window goes away. */
#define DOCHAND_CAP_TELL_WAND_WE_HAVE_TO_LEAVE

/** Asynchronous initialization of iframes using FrameReinitData */
#define DOCHAND_CAP_HAS_FRAME_REINIT

// Window::SetContentBlockEnabled() is available
#define DOCHAND_CAP_HAS_SET_CONTENT_BLOCK_ENABLED

/** Has m_adaptive_zoom flag to record the current default state for adaptive zoom on this window. */
#define DOCHAND_CAP_GETADAPTIVEZOOM

/** Window::RemoveAllElementsFromHistory has unlink argument. */
#define DOCHAND_CAP_REMOVEALLELEMENTSFROMHISTORY_UNLINK

/** FramesDocElm::Init doesn't arbitrarily skip allocating a visual device for element types it doesn't know. */
#define DOCHAND_CAP_FLEXIBLE_FRAMESDOCELM_INIT

/** FramesDocElm has nonscaled frames size */
#define DOCHAND_CAP_FRAMES_SIZE_FIX

/** DocumentManager::GetHistoryDirection() is available. */
#define DOCHAND_CAP_DOCMAN_GETHISTORYDIRECTION

/** DocumentManager::OpenURLOptions is supported, and has the ignore_fragment_id option . */
#define DOCHAND_CAP_OPENURLOPTIONS

/** SetPosition has yet another parameter (update_size). */
#define DOCHAND_CAP_SETPOS_NOUPDATESIZE

/** Pointers to the FramesDocElms are stored in attributes in HTML_Element */
#define DOCHAND_CAP_FDELMATTR

/** The DisplayLinkInformation() method takes a URL instead of two strings */
#define DOCHAND_CAP_LINKINFO_TAKES_URL

/** Supports migrating threads that generate new documents. */
#define DOCHAND_CAP_THREAD_MIGRATION

/** FramesDocElm has virtual position (GetVirtualX()/GetVirtualY()). */
#define DOCHAND_CAP_FRAME_VIRTUAL_POS

/** FramesDocElm has a twin HTML_Element for the print clone of the frame */
#define DOCHAND_CAP_FDELM_HAS_PRINTTWIN

/** Window::EndSearch() exists */
#define DOCHAND_CAP_HAS_ENDSEARCH

/** FramesDocElm::SetHtmlElement() exists */
#define DOCHAND_CAP_FDELM_SETHTMLELM

/** No longer calls FramesDocument::PluginSetValue when plugins are disabled. */
#define DOCHAND_CAP_STOPPED_MISUSING_PLUGIN_DEFINES

/** FramesDocElm::ShowFrames() has coreview attribute */
#define DOCHAND_CAP_FDELM_SHOWFRAMES_COREVIEW

/** Window::GetCSSCollection() exists */
#define DOCHAND_CAP_GETCSSCOLLECTION

/** WindowManager::OpenURLNamedWindow will redirect gadgets to another top level window if they try to replace a gadget. */
#define DOCHAND_CAP_GADGETS_REDIRECTED

/** ResetCurrentDownloadRequest takes TransferManagerDownloadCallback pointer argument */
#define DOCHAND_CAP_DOWNLOAD_CALLBACK_RESET_WITH_POINTER

/** Have WIN_TYPE_DEVTOOLS */
#define DOCHAND_CAP_WIN_TYPE_DEVTOOLS

/* Window::SetSecurityState has a URL parameter, and Window has a list of URLs with low security (0-2) */
#define DOCHAND_CAP_SECURITY_STATE_URL

/* Window::SetForceWordWrap and Window::ForceWordWrap are supported. */
#define DOCHAND_CAP_FORCED_WORD_WRAP

/* Window::DisableGlobalHistory and Window::IsGlobalHistoryEnabled are supported. */
#define DOCHAND_CAP_DISABLE_GLOBAL_HISTORY

/* Trust info parser has advisory information. */
#define DOCHAND_CAP_TRUST_INFO_PARSER_ADVISORY

/* DocumentManager has the OBML2DDataManager variable, and GetOBMLDataManager() method */
#define DOCHAND_CAP_HAS_OBML_DATA_MANAGER

/* Window has methods for getting and setting an ElementExpander used by FingerTouch */
#define DOCHAND_CAP_HAS_ELEMENT_EXPANDER

/* FramesDocElm::StopLoading() takes two parameters */
#define DOCHAND_CAP_FDE_STOPLOADING_TAKES_THREE

/* Gadgets url context id != this */
#define DOCHAND_CAP_GADGETS_USES_UNIQUE_CONTEXT_ID

/* Window has GetPrivacyMode and SetPrivacyMode */
#define DOCHAND_CAP_HAS_PRIVACY_MODE

/* NullViewportInfoListener::OnDocumentContentChanged() exists. */
#define DOCHAND_CAP_VC_ONDOCUMENTCONTENTCHANGED

/* ViewportController has SetBufferedMode() and FlushBuffer(). */
#define DOCHAND_CAP_VC_BUFFERED_MODE

/* ViewportController::GetMaxParagraphWidth */
#define DOCHAND_CAP_VPC_GETMAXPARAGRAPHWIDTH

/* DocumentManager::ConstructDOMProxyEnvironment exists */
#define DOCHAND_CAP_CONTRUCTDOMENVIRONMENT

/* DocumentManager::GenerateReferrerURL exists */
#define DOCHAND_CAP_HAS_GENERATE_REFERRER

/* DocumentManager::GetRealWindow will create an about:blank document if requested before there is something else. */
#define DOCHAND_CAP_CREATES_ABOUT_BLANK

/* ViewportController::SelectText() exists. */
#define DOCHAND_CAP_SELECTTEXT

/* ViewportController::SetTrueZoomBaseScale exists*/
#define DOCHAND_CAP_VC_SETTRUEZOOMBASESCALE

/** Window::HighlightNextMatch returns coordinates for the match. */
#define DOCHAND_CAP_HIGHLIGHT_NEXT_MATCH_RETURNS_RECT

/** DocumentManager::GetStorageManager() exists. */
#define DOCHAND_CAP_HAS_STORAGE_MANAGER

/** DocumentManager::GenerateAndShowClickJackingBlock() exists. */
#define DOCHAND_CAP_CLICKJACKING_PAGE

/** Window::HighlightNextMatch takes wrapped argument. */
#define DOCHAND_CAP_HIGHLIGHT_NEXT_MATCH_TAKES_WRAPPED

/** ViewportListener::OnZoomLevelChangeRequest() with arguments
 * priority_rect and reason is implemented */
#define DOCHAND_CAP_HAS_ONZOOMLEVELCHANGEREQUEST_WITH_REASON

/** DocumentManager::GenerateReferrerURL exists */
#define DOCHAND_CAP_HAS_GENERATE_REFERRER

/** Window::SetHasShownUnsolicitedDownloadDialog & co exists */
#define DOCHAND_CAP_HAS_DOWNLOAD_DIALOG_THROTTLING2

/* FramesDocElm::RestoreForms has been renamed to ReactivateDocument */
#define DOCHAND_CAP_HAS_REACTIVATE_DOCUMENT

/* Window has GetTurboMode and SetTurboMode */
#define DOCHAND_CAP_HAS_TURBO_MODE_WINDOW

#endif // DOCHAND_CAPABILITIES_H
