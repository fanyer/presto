/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef OP_PAGE_LISTENER_H
#define OP_PAGE_LISTENER_H

#include "modules/windowcommander/OpWindowCommander.h"
#include "adjunct/quick_toolkit/widgets/OpPage.h"

/** @brief A listener for events from an OpPage (web page)
  * This is a listener class for events from an OpPage, usually
  * from window commander.
  */
class OpPageListener
{
public:
	OpPageListener() : m_page(0) {}
	virtual ~OpPageListener() { if (m_page) m_page->RemoveListener(this); }

	void SetPage(OpPage* page) { m_page = page; }
	virtual void OnPageDestroyed(OpPage* page) { if (m_page == page) m_page = 0; }

	virtual void OnPageSelectionChanged(OpWindowCommander* commander) {}

	virtual BOOL OnPagePopupMenu(OpWindowCommander* commander, OpDocumentContext& context) { return FALSE; }
	virtual void OnPageSecurityModeChanged(OpWindowCommander* commander, OpDocumentListener::SecurityMode mode, BOOL inline_elt) {}

#ifdef TRUST_RATING
	virtual void OnPageTrustRatingChanged(OpWindowCommander* commander, TrustRating new_rating) {}
#endif // TRUST_RATING

	virtual void OnPageTitleChanged(OpWindowCommander* commander, const uni_char* title) {}
	virtual void OnLinkClicked(OpWindowCommander* commander) {}
	
#ifdef SUPPORT_VISUAL_ADBLOCK
	virtual void OnPageContentBlocked(OpWindowCommander* commander, const uni_char *image_url) {}
	virtual void OnPageContentUnblocked(OpWindowCommander* commander, const uni_char *image_url) {}
#endif // SUPPORT_VISUAL_ADBLOCK
	
	virtual void OnPageAccessKeyUsed(OpWindowCommander* commander) {}
	virtual void OnPageHover(OpWindowCommander* commander, const uni_char* url, const uni_char* link_title, BOOL is_image) {}
	virtual void OnPageNoHover(OpWindowCommander* commander) {}
	virtual void OnTabFocusChanged(OpWindowCommander* commander, OpDocumentContext& context) {}
	virtual void OnPageSearchReset(OpWindowCommander* commander) {}
	virtual void OnPageSearchHit(OpWindowCommander* commander) {}
	virtual void OnPageDocumentIconAdded(OpWindowCommander* commander) {}
	virtual void OnPageScaleChanged(OpWindowCommander* commander, UINT32 newScale) {}
	virtual void OnPageInnerSizeRequest(OpWindowCommander* commander, UINT32 width, UINT32 height) {}
	virtual void OnPageGetInnerSize(OpWindowCommander* commander, UINT32* width, UINT32* height) {}
	virtual void OnPageOuterSizeRequest(OpWindowCommander* commander, UINT32 width, UINT32 height) {}
	virtual void OnPageGetOuterSize(OpWindowCommander* commander, UINT32* width, UINT32* height) {}
	virtual void OnPageMoveRequest(OpWindowCommander* commander, INT32 x, INT32 y) {}
	virtual void OnPageGetPosition(OpWindowCommander* commander, INT32* x, INT32* y) {}
	virtual void OnPageWindowAttachRequest(OpWindowCommander* commander, BOOL attached) {}
	virtual void OnPageGetWindowAttached(OpWindowCommander* commander, BOOL* attached) {}
	virtual void OnPageClose(OpWindowCommander* commander) {}
	virtual void OnPageRaiseRequest(OpWindowCommander* commander) {}
	virtual void OnPageLowerRequest(OpWindowCommander* commander) {}
	
#ifdef GADGET_SUPPORT
	virtual void OnPageGadgetInstall(OpWindowCommander* commander, URLInformation* url_info) {}
#endif // GADGET_SUPPORT
	
# if defined WIC_CAP_JAVASCRIPT_PRINT && defined _PRINT_SUPPORT_
	virtual void OnPageJSPrint(OpWindowCommander * commander, PrintCallback * callback) {}
# endif // WIC_CAP_JAVASCRIPT_PRINT && _PRINT_SUPPORT_
	
#if defined WIC_CAP_JAVASCRIPT_PRINT && defined _PRINT_SUPPORT_
	virtual void OnPageUserPrint(OpWindowCommander * commander, PrintCallback * callback) {}
#endif // WIC_CAP_JAVASCRIPT_PRINT && _PRINT_SUPPORT_
	
#if defined (DOM_FILE_API_SUPPORT) && defined (WIC_CAP_HAS_MOUNTPOINT_API)
	virtual void OnPageMountpointFolderRequest(OpWindowCommander* commander, MountPointFolderCallback* callback, MountPointFolderCallback::FolderType folder_type) {}
	virtual void OnPageMountpointFolderCancel(OpWindowCommander* commander, MountPointFolderCallback* callback) {}
#endif // DOM_FILE_API_SUPPORT && WIC_CAP_HAS_MOUNTPOINT_API

	// OpLoadingListener
	virtual void OnPageLoadingFailed(OpWindowCommander* commander, const uni_char* url) {}
#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	virtual void OnPageSearchSuggestionsRequested(OpWindowCommander* commander, const uni_char* url, OpSearchSuggestionsCallback* callback) {}
	virtual void OnPageSearchSuggestionsUsed(OpWindowCommander* commander, BOOL suggestions) {}
#endif
	virtual void OnPageUrlChanged(OpWindowCommander* commander, const uni_char* url) {}
	virtual void OnPageStartLoading(OpWindowCommander* commander) {}
	virtual void OnPageLoadingProgress(OpWindowCommander* commander, const LoadingInformation* info) {}
	virtual void OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL was_stopped_by_user) {}
	virtual void OnPageStartUploading(OpWindowCommander* commander) {}
	virtual void OnPageUploadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status) {}
	virtual void OnPageOnDemandStateChangeNotify(OpWindowCommander* commander, BOOL has_placeholders) {}

#ifdef EMBROWSER_SUPPORT
	virtual void OnPageUndisplay(OpWindowCommander* commander) {}
	virtual void OnPageLoadingCreated(OpWindowCommander* commander) {}
#endif // EMBROWSER_SUPPORT

	virtual void OnPageCancelDialog(OpWindowCommander* commander, OpDocumentListener::DialogCallback* callback) {}

#ifdef PLUGIN_AUTO_INSTALL
	virtual void OnPageNotifyPluginMissing(OpWindowCommander* commander, const OpStringC& mime_type) {};
	virtual void OnPagePluginDetected(OpWindowCommander* commander, const OpStringC& mime_type) {};
	virtual void OnPageRequestPluginInfo(OpWindowCommander* commander, const OpStringC& mime_type, OpString& plugin_name, BOOL& available) {};
	virtual void OnPageRequestPluginInstallDialog(OpWindowCommander* commander, const PluginInstallInformation& information) {};
#endif // PLUGIN_AUTO_INSTALL

#if defined SUPPORT_DATABASE_INTERNAL
	virtual void OnPageIncreaseQuota(OpWindowCommander* commander, const uni_char* db_name, const uni_char* db_domain, const uni_char* db_type, OpFileLength current_quota_size, OpFileLength quota_hint, OpDocumentListener::QuotaCallback *callback) {}
#endif // SUPPORT_DATABASE_INTERNAL

	virtual void OnGeolocationAccess(OpWindowCommander* commander, const OpVector<uni_char>* hosts) {}
	virtual void OnCameraAccess(OpWindowCommander* commander, const OpVector<uni_char>* hosts) {}
#ifdef DOM_FULLSCREEN_MODE
	virtual OP_STATUS OnJSFullScreenRequest(OpWindowCommander* commander, BOOL enable_fullscreen) { return OpStatus::OK; }
#endif // DOM_FULLSCREEN_MODE

protected:
	friend class OpPageAdvancedListener;
	OpPage* m_page;
};

class OpPageAdvancedListener : public OpPageListener
{
public:
	virtual ~OpPageAdvancedListener() { if (m_page) m_page->RemoveAdvancedListener(this); }
	
	// OpDocumentListener

	virtual void OnPageDrag(OpWindowCommander* commander, const OpRect rect, const uni_char* link_url, const uni_char* link_title, const uni_char* image_url) {}
#ifdef DND_DRAGOBJECT_SUPPORT
	virtual void OnDragLeave(OpWindowCommander* commander, class DesktopDragObject* drag_object) {}
	virtual void OnDragMove(OpWindowCommander* commander, class DesktopDragObject* drag_object, const OpPoint& point) {}
	virtual void OnDragDrop(OpWindowCommander* commander, class DesktopDragObject* drag_object, const OpPoint& point) {}
#endif // DND_DRAGOBJECT_SUPPORT

	virtual void OnPageStatusText(OpWindowCommander* commander, const uni_char* text) {}

	virtual void OnPageConfirm(OpWindowCommander* commander, const uni_char *message, OpDocumentListener::DialogCallback *callback) {}
	virtual void OnPageQueryGoOnline(OpWindowCommander* commander, const uni_char *message, OpDocumentListener::DialogCallback *callback) {}

	virtual void OnPageJSAlert(OpWindowCommander* commander,	const uni_char *hostname, const uni_char *message, OpDocumentListener::JSDialogCallback *callback) {}
	virtual void OnPageJSConfirm(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, OpDocumentListener::JSDialogCallback *callback) {}
	virtual void OnPageJSPrompt(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, const uni_char *default_value, OpDocumentListener::JSDialogCallback *callback) {}
	virtual void OnPageJSPopup(OpWindowCommander* commander,	const uni_char *url, const uni_char *name, int left, int top, int width, int height, BOOL3 scrollbars, BOOL3 location, BOOL refuse, BOOL unrequested, OpDocumentListener::JSPopupCallback *callback) { }

	virtual void OnPageFormSubmit(OpWindowCommander* commander, OpDocumentListener::FormSubmitCallback *callback) {}
	virtual void OnPagePluginPost(OpWindowCommander* commander, OpDocumentListener::PluginPostCallback *callback) {}
	virtual void OnPageDownloadRequest(OpWindowCommander* commander, OpDocumentListener::DownloadCallback * callback) {}
	virtual void OnPageSubscribeFeed(OpWindowCommander* commander, const uni_char* url) {}
	virtual void OnPageAskAboutUrlWithUserName(OpWindowCommander* commander, const uni_char* url, const uni_char* hostname, const uni_char* username, OpDocumentListener::DialogCallback* callback) {}
	virtual void OnPageAskAboutFormRedirect(OpWindowCommander* commander, const uni_char* source_url, const uni_char* target_url, OpDocumentListener::DialogCallback* callback) {}
	virtual void OnPageFormRequestTimeout(OpWindowCommander* commander, const uni_char* url) {}
	virtual BOOL OnPageAnchorSpecial(OpWindowCommander * commander, const OpDocumentListener::AnchorSpecialInfo & info) { return FALSE; }
	virtual void OnPageAskPluginDownload(OpWindowCommander * commander) {}
	virtual void OnPageVisibleRectChanged(OpWindowCommander * commander, const OpRect& visible_rect) {}
	virtual void OnPageMailTo(OpWindowCommander * commander, const OpStringC8& raw_mailto, BOOL mailto_from_dde, BOOL mailto_from_form, const OpStringC8& servername) {}
	virtual void OnDocumentScroll(OpWindowCommander* commander) {}
	virtual void OnPageSetupInstall(OpWindowCommander* commander, const uni_char* url, URLContentType content_type) {}
	virtual void OnFileDropConfirm(OpWindowCommander* commander, const uni_char *server_name, OpDocumentListener::DialogCallback *callback) {}
	virtual void OnUnexpectedComponentGone(OpWindowCommander* commander, OpComponentType type, const OpMessageAddress& address, const OpStringC& information) {}

	// OpLoadingListener
	virtual void OnPageLoadingFailed(OpWindowCommander* commander, const uni_char* url) {}
	virtual void OnPageAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback) {}
};

#endif // OP_PAGE_LISTENER_H
