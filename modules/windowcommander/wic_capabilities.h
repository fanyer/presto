/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file wic_capabilities.h
 *
 * This file contains the capabilities exported by the WindowCommander module.
 *
 * @author rikard@opera.com
*/

#ifndef WIC_CAPABILITIES_H
#define WIC_CAPABILITIES_H

// this capability is used to determine if the new api extensions are available
#define WIC_CAP_ALCHEMILLA_2
#define WIC_CAP_ALCHEMILLA_3

/* LoadingInformation has a BOOL that says whether we're uploading or not */
#define WIC_CAP_UPLOAD_STATUS_IN_LI

/* LinkElementInfo has the 'type' field */
#define WIC_CAP_TYPE_IN_LEI

/* Capability: OnJSAlert, OnJSConfirm and OnJSPrompt in OpDocumentListener.
   Used by the DOM module from version dumdum_1_final_patch_22 if defined. */
#define WIC_CAP_JAVASCRIPT_DIALOGS

#define WIC_CAP_JAVASCRIPT_POPUP
#define WIC_CAP_JAVASCRIPT_POPUP_ADVANCED

#define WIC_CAP_JAVA_LISTENERS

/* Capability: OnLoadingCreated in OpLoadingListener.
   Used by the DOC module from version presto_2_final_patch_4 if defined. */
#define WIC_CAP_LOADING_CREATED_CALLBACK

#define WIC_CAP_CONTENTMAGIC_CALLBACK

#define WIC_CAP_HAS_SEL_LINK_INFO

// defines that the authentication api passes around windowcommander and userdata
#define WIC_CAP_MULTI_AUTH

// defines that the API for a asynchronous confirm dialog is present.
#define WIC_CAP_HAS_CONFIRM_DIALOG

// LoadingInformation has Reset() method
#define WIC_CAP_LI_RESET

// security information passes on inline info
#define WIC_CAP_SECINFO_INLINES

// Interfaces for signalling oom
#define WIC_CAP_SIGNAL_OOM

// OpDocumentListener has OnContentResized
#define WIC_CAP_HAS_ONCONTENTRESIZE

#define WIC_CAP_HAS_ON_UNKNOWN_PROTOCOL

/** Capability: OpDocumentListener::OnHandheldChanged */
#define WIC_CAP_HANDHELD_CHANGE_CALLBACK

// we have OnAuthenticationCancelled and BroadcastAuthenticationCancelled is available from within the core code.
#define WIC_CAP_BROADCAST_AUTH

/* OnJSAlert, OnJSConfirm and OnJSPrompt in OpDocumentListener takes an extra
   argument specifying the hostname of the script that requested the dialog. */
#define WIC_CAP_JS_DIALOGS_HOSTNAME

// AbortScript and SetScriptingDisabled with friends
#define WIC_CAP_SETSCRIPTINGDISABLED

/** Has OpWindowCommander::*VisibleOnScreen() */
#define WIC_CAP_VISIBLEONSCREEN

// Gadgets now use control regions (CSS) to facilitate gadget dragging.
#define WIC_CAP_GADGET_CONTROL_REGIONS

// has the WindowCommander::CancelTransfer() method
#define WIC_CAP_HAS_CANCEL_TRANSFER

// OnFormSubmit replaces OnInsecureFormsSubmit
#define WIC_CAP_ONFORMSUBMIT

// OpDocumentListener::OnUserJSonHTTPSConfirmationNeeded is supported.
#define WIC_CAP_USERJSONHTTPSCONFIRMATION

// OpWindowCommander::Set/GetTrueZoom exist
#define WIC_CAP_TRUEZOOM

// has content blocker methods (taking uni_char* instead of URL*)
#define WIC_CAP_CONTENTBLOCK_METHODS_URLSTRING

// has OpDocumentListener::CancelGadgetDragRequest()
#define WIC_CAP_CANCEL_GADGET_REQUEST

#define WIC_INITIATE_TRANSFER_TAKES_TXACTION

#define WIC_CAP_ONDOWNLOADREQ_TAKES_7

// documentListener OnRefreshUrl
#define WIC_CAP_HAS_ONREFRESHURL

// OpDocumentListener has OnTrustRatingChanged()
#define WIC_CAP_DOCUMENTLISTENER_TRUST_RATING

/** support for handling of RSS */
#define WIC_CAP_RSS

// Has OpWindowCommander::GetEncoding
#define WIC_CAP_GETENCODING

// Has OpLoadingListener::OnLoadingFailed
#define WIC_CAP_HAS_ONLOADINGFAILED

#define WIC_CAP_HAS_ONACCESSKEY_USED

// Has OpWindowCommander::{Get,Set}ScriptingPaused.
#define WIC_CAP_SETSCRIPTINGPAUSED

// has OpDocumentListener::OnPluginPost()
#define WIC_CAP_ON_PLUGIN_POST

// Has OpDocumentListener::OnContentDisplay
#define WIC_CAP_ONCONTENTDISPLAY

// The AuthenticationInformation class holds a URL-object that is used
// to get the correct information about the ongoing authentication
#define WIC_CAP_AUTHINFO_HAS_URL

// OpTransferManagerListener has 2 new pure virtual methods related to deletion of files
#define WIC_CAP_TRANSFERMANAGER_METHODS_DELETEFILES

// OpUiWindowListener::CreateUiWindow has a boolean flag to be able to tell
// the platform that a window with per-pixel transparency is needed
#define WIC_CAP_CREATEWINDOW_TAKES_TRANSPARENCY_FLAG

// OpWindowCommander::OpenURL takes a bool argument 'use_prev_entry' that will
// reuse the same history item, so no new history item will be created for the opened url
#define WIC_CAP_OPENURL_TAKES_USE_PREV_ENTRY

// Has OpSSLListener
#define WIC_CAP_SSL_LISTENER

// Handles non-doubled security levels if DOCHAND_CAP_NON_DOUBLED_SECURITY_LEVEL is defined.
#define WIC_CAP_NON_DOUBLED_SECURITY_LEVEL

// Supports the Extended Validation security mode
#define WIC_CAP_EXTENDED_VALIDATION_SEC_MODE

// Has WindowCommander::Enable/DisableAdaptiveZoomInternal
#define WIC_CAP_ADAPTIVE_ZOOM_INTERNAL

// Have functions to get trust level and initiate trust checks
#define WIC_CAP_TRUST_GET_AND_CHECK

// Has SSLSSLCertificate::GetValidFrom/GetValidTo
#define WIC_CAP_CERT_VALIDTO

// Has OpDocumentListener::OnWmlDenyAccess()
#define WIC_CAP_WMLDENYACCESS

// Has OpDocumentListener::OnAskAboutUrlWithUserName
//#define WIC_CAP_ASK_ABOUT_URL_WITH_USERNAME -- retired, replaced by *_2

// Has OpDocumentListener::OnAskAboutFormRedirect
//#define WIC_CAP_ASK_ABOUT_FORM_REDIRECT -- retired, replaced by *_2

// Has OpDocumentListener::OnFormRequestTimeout
#define WIC_CAP_FORM_REQUEST_TIMEOUT

// Has EC_NET_NO_NET NetworkError
#define WIC_CAP_HAS_EC_NET_NO_NET

// Has EC_NET_NO_FREE_SOCKETS NetworkError
#define WIC_CAP_HAS_EC_NET_NO_FREE_SOCKETS

// TransferManagerDownloadAPI supports a forced change of mimetype
#define WIC_CAP_TRANSFERMANAGER_SUPPORTS_FORCE_MIME_TYPE

// Using URL_ID in AuthenticationInformation
#define WIC_CAP_AUTHINFO_HAS_URL_ID

// due to a compiler issue, the AuthenticationInformation::ServerName is renamed to ServerNameC
#define WIC_CAP_AUTHINFO_HAS_SERVERNAMEC

// OpSSLListener::SSLCertificateContext offers GetNumberOfComments/GetCertificateComment
#define WIC_CAP_CERT_COMMENTS

// OpWindowCommander offers Set/GetAccessKeyMode()
#define WIC_CAP_HAS_SETGET_ACCESSKEYMODE

// Has OpCookieListener
#define WIC_CAP_COOKIE_LISTENER

// Has OpDocumentListener::OnAskAboutUrlWithUserName that takes username/hostname
#define WIC_CAP_ASK_ABOUT_URL_WITH_USERNAME_2

// Has OpDocumentListener::OnAskAboutFormRedirect that takes source_ur/target_url
#define WIC_CAP_ASK_ABOUT_FORM_REDIRECT_2

// OpDocumentListener has OnGadgetGetAttention() and OnGadgetShowNotification()
#define WIC_CAP_GADGET_GETATTENTION

// Has OpFileSelectionListener
#define WIC_CAP_FILESELECTION

// OpWindowCommander has GetTextParagraphRects()
#define WIC_CAP_GETTEXTPARAGRAPHRECTS

// Has URLInformation API with methods
#define WIC_CAP_URL_INFO_METHODS

// Has BOOL OpTransferItem::GetShowTransfer()
#define WIC_CAP_SHOWTRANSFER

// PopupMenuInfo::spell_session_id and OpSpellUiSession exists
#define WIC_CAP_SPELLCHECKING

// OpDocumentListener::SecurityMode has FLAWED_SECURITY value
#define WIC_CAP_FLAWED_SECURITY

// OpCookieListener has a cookie action dedicated to removing all session cookies on exit
#define WIC_CAP_COOKIE_ACTION_DOMAINCOOKIES_DELETE_ON_EXIT

// The AccesskeyListener::OnAccessKeyAdded() method takes 4 parameters
#define WIC_CAP_ACCESSKEYADDED_TAKES_4

// AskCookieContext provides GetServerName()
#define WIC_CAP_COOKIE_CONTEXT_SERVERNAME

// OpOomListener has OnOodError().
#define WIC_CAP_OOD_ERROR

// Has OpDataSyncListener
#define WIC_CAP_DATA_SYNC_LISTENER

// OpMenuListener::MenuTypeFlag defines the value POPUP_TEXTEDIT
#define WIC_CAP_POPUPMENU_TEXTEDIT

// Support the reduced Extended Validation security mode
#define WIC_CAP_SOME_EXTENDED_SECURITY

// URLInformation object has a way to survive a complete transistion
#define WIC_CAP_URLINFO_REUSE

// DownloadCallback::Save lost its show_transfer flag
#define WIC_CAP_DOWNLOAD_SAVE_NO_SHOW_TRANSFER

// The PopupMenuInfo has doc_url_info
#define WIC_CAP_POPUPMENUINFO_DOCURLINFO2

// TransferManagerDownloadAPI has PassURL method
#define WIC_CAP_TRANSFERMANAGERDOWNLOADAPI_PASSURL

// URLInformation has method SetMIMETypeOverride
#define WIC_CAP_URLINFO_MIMETYPE_OVERRIDE

// OpWindowCommander has GetSuggestedFileName(BOOL, OpString*) and GetDocumentType().
#define WIC_CAP_SAVEDOCUMENT3

// OpWindowCommander::SearchInfo exists, along with an improved Search() method.
#define WIC_CAP_SEARCHINFO

/* OpDocumentListener::FormSubmitCallback::GetTargetWindowCommander() exists,
   if DOC_CAP_GET_TARGET_WIC is defined as well. */
#define WIC_CAP_GET_TARGET_WIC

// OpErrorListener has OnMissingInlineContentError()
#define WIC_CAP_MISSING_INLINE_CONTENT

// OpDocumentListener has OnGadgetInstall()
#define WIC_CAP_GADGET_INSTALL

// OpWindowCommander::GetWindow() is deprecated. ViolateCoreEncapsulationAndGetWindow() exists.
#define WIC_CAP_GETWINDOW_DEPRECATED

// Has OpGadgetListener
#define WIC_CAP_HAS_GADGET_LISTENER

// Has enum TRANSFERTYPE_WEEBFEED_DOWNLOAD in TransferItem
#define WIC_CAP_TRANSFER_WEBFEED

// Has enum DOC_WEBFEED in OpWindowCommander::DocumentType
#define WIC_CAP_DOCUMENTTYPE_WEBFEED

// OpDocumentContext used for menus rather than the old PopupMenuInfo
#define WIC_CAP_POPUP_DOCUMENT_CONTEXT

// OpWindowCommander has GetPrivacyMode() and SetPrivacyMode()
#define WIC_CAP_HAS_PRIVACY_MODE

// OpWindowCommander has SetSmartFramesMode() and GetSmartFramesMode().
#define WIC_CAP_SMARTFRAMES_TOGGLE

// OpTransferManager::RemovePrivacyModeTransfers() exists, and
// TransferManager::AddTransferItem() takes started_in_privacy_mode parameter
#define WIC_CAP_TRANSFERMANAGER_PRIVACY_MODE

// Has OpMediaSource
#define WIC_CAP_HAS_MEDIA_SOURCE

// OpViewportController has SetBufferedMode() and FlushBuffer().
#define WIC_CAP_VC_BUFFERED_MODE

// OpViewportInfoListener::OnDocumentContentChanged() exists.
#define WIC_CAP_VC_ONDOCUMENTCONTENTCHANGED

// OpSpellUiSession::GetInstalledLanguageVersionAt() exists if
// SPELLCHECKER_CAP_GET_DICTIONARY_VERSION is defined as well
#define WIC_CAP_GET_DICTIONARY_VERSION

// OpWindowCommander::CancelTransfer() exists.
#define WIC_CAP_CANCEL_TRANSFER

// OpWindowCommander has SendCustomWindowEvent() and SendCustomGadgetEvent().
#define WIC_CAP_SEND_CUSTOM_EVENT

// Has OpFingertouchListener
#define WIC_CAP_FINGERTOUCH_LISTENER

// OpWindowCommander::SaveDocument() has a max_size parameter
#define WIC_CAP_SAVE_HAS_MAXSIZE

// Has OpSSLListener::SSLCertificateContext::GetServerName(OpString*) if LIBSSL_CAP_WIC_GETSERVERNAME is defined.
#define WIC_CAP_SSLCONTEXT_GETSERVERNAME

// Has OpViewportController::SelectTextStart() and OpViewportController::SelectTextEnd() if DOCHAND_CAP_SELECTTEXT && !HAS_NOTEXTSELECTION
#define WIC_CAP_SELECTTEXT

// OpWindowCommander::DocumentType has DOC_PLUGIN
#define WIC_CAP_TYPE_DOC_PLUGIN

// OpDocumentListener::OnCancelDialog() exists.
#define WIC_CAP_CANCEL_DIALOG

// Has OpWindowCommander and OpDocumentContext methods for SVG context menus
#define WIC_CAP_HAS_SVG_CONTEXTMENU

// Has OpViewportController::SetTrueZoomBaseScale() if DOCHAND_CAP_VC_SETTRUEZOOMBASESCALE is defined.
#define WIC_CAP_SETTRUEZOOMBASESCALE

// Has OpGadgetListener extension for notifications about downloaded widgets (through opera.widgetManager)
#define WIC_CAP_HAS_GADGET_LISTENER_DOWNLOAD_NOTIFICATIONS

// OpWindowCommander::SetTextScale and ::GetTextScale exist
#define WIC_CAP_TEXTSCALE

// Has GetSearchMatchRectangles and OnSearchHit
#define WIC_CAP_HAS_SEARCH_MATCHES

// OpWindowCommander has GetTurboMode() and SetTurboMode()
#define WIC_CAP_HAS_TURBO_MODE

// One can ask the windowcommander for a special url context if applicable (TURBO mode only user for now)
#define WIC_CAP_URL_CONTEXT

// OpWindowCommander::SaveDocument has extra arguments that return the original byte size written to disk in case of partial a write (disk full).
#define WIC_CAP_SAVE_RETURN_SAVED_SIZE

// MidClick functionality and some additions to OpDocumentContext
#define WIC_CAP_HAS_LINKDATATYPE

// OpWindowCommander::GetHistoryRange(), OpWindowCommander::SetCurrentHistoryPos() are added.
#define WIC_CAP_WINDOW_HISTORY_POSITIONS

// OpWindowCommander has OnIMSRequested, OnIMSCancelled, OnIMSUpdated
#define WIC_CAP_WIDGETS_IMS_SUPPORT

// The method OpViewportRequestListener::OnZoomLevelChangeRequest()
// has two new arguments: priority_rect and reason
#define WIC_CAP_HAS_ONZOOMLEVELCHANGEREQUEST_WITH_REASON

// class LoadingInformation contains loadedBytesRootDocument and totalBytesRootDocument
#define WIC_CAP_HAS_ROOT_DOC_LOAD_INFO

//if OpDocumentListener::OnConfirmQuota exists
#define WIC_CAP_DOCLISTENER_HAS_INCREASE_QUOTA

// Has OpWebTurboUsageListener
#define WIC_CAP_WEB_TURBO_USAGE_LISTENER

// Adds OnGadget{Installed,Removed,Upgraded,Started,Stopped}()
#define WIC_CAP_HAS_GADGET_LISTENER_EXTRA_EVENTS

// enum DocumentURLType, OpWindowCommander::GetURLType() and
// AuthenticationInformation::GetURLType() are available:
#define WIC_CAP_DOCUMENT_URL_TYPE

// OpWindowCommander::CreateThumbnailGenerator exists
#define WIC_CAP_HAS_CREATE_THUMBNAIL_GENERATOR

// OpSpellUiSession is extended with a function to get the license text from a dictionary.
#define WIC_CAP_DICTIONARY_LICENSE

// OpLoadingListener can be notified about moved URLs (redirects)
#define WIC_CAP_URL_MOVED_EVENT

#endif // WIC_CAPABILITIES_H
