/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef OP_PAGE_H
#define OP_PAGE_H

#include "modules/skin/OpWidgetImage.h"
#include "modules/util/adt/oplisteners.h" 
#include "modules/windowcommander/OpWindowCommander.h"

class OpPageListener;
class OpPageAdvancedListener;
class OpWindow;
class OpInputAction;

/** @brief Represents a web page in Opera
  * This class implements everything needed to represent a web page
  * in Opera, and acts as a wrapper between window commander (core)
  * and desktop classes that need to control or get information from
  * a web page.
  *
  * To display a web page, see OpPageView.
  *
  * To listen to events from this page, implement an OpPageListener.
  *
  */
class OpPage
	:public OpMenuListener
	,public OpDocumentListener
	,public OpLoadingListener
{
public:
	
	OpPage(OpWindowCommander* window_commander);
	~OpPage();

	OP_STATUS Init();

	OP_STATUS AddListener(OpPageListener* listener);
	OP_STATUS RemoveListener(OpPageListener* listener);
	OP_STATUS AddAdvancedListener(OpPageAdvancedListener* listener);
	OP_STATUS RemoveAdvancedListener(OpPageAdvancedListener* listener);
	OP_STATUS SetOpWindow(OpWindow* op_window);

	OpWindowCommander* GetWindowCommander();
	OP_STATUS GoToPage(const OpStringC& url);
	OP_STATUS Reload();
	BOOL IsFullScreen();
	BOOL IsHighSecurity();
	OpDocumentListener::SecurityMode GetSecurityMode();
	const uni_char* GetTitle() { return m_title.CStr(); }
	void SelectionChanged();
	void SetSupportsNavigation(BOOL supports) { m_flags.supports_navigation = supports; }
	void SetSupportsSmallScreen(BOOL supports) { m_flags.supports_small_screen = supports; }
	void SetStoppedByUser(BOOL stopped_by_user) {m_flags.stopped_by_user = stopped_by_user;}
	BOOL IsActionSupported(OpInputAction* action);
	OP_STATUS GetSecurityInformation(OpString& text);
	BOOL ContinuePopup(OpDocumentListener::JSPopupCallback::Action action = OpDocumentListener::JSPopupCallback::POPUP_ACTION_DEFAULT, OpDocumentListener::JSPopupOpener **opener = NULL);

	const OpWidgetImage* GetDocumentIcon() const { return &m_document_icon; }

	void SaveDocumentIcon();
	void UpdateWindowImage(BOOL force);
	void SetAuthIsPending(bool is_pending) { m_any_pending_authentication = is_pending; }

	// Implementing OpLoadingListener

	virtual void OnUrlChanged(OpWindowCommander* commander, URL& url);
	virtual void OnUrlChanged(OpWindowCommander* commander, const uni_char* url) ;
	virtual void OnStartLoading(OpWindowCommander* commander);
	virtual void OnLoadingProgress(OpWindowCommander* commander, const LoadingInformation* info);
	virtual void OnLoadingFinished(OpWindowCommander* commander, LoadingFinishStatus status);
	virtual void OnAuthenticationRequired(OpWindowCommander* commander, 
			OpAuthenticationCallback* callback);
	virtual void OnAuthenticationCancelled(OpWindowCommander* commander, URL_ID authid) {}
	virtual void OnStartUploading(OpWindowCommander* commander);
	virtual void OnUploadingFinished(OpWindowCommander* commander, LoadingFinishStatus status);
	virtual BOOL OnLoadingFailed(OpWindowCommander* commander, int msg_id, const uni_char* url);
	virtual void OnCancelDialog(OpWindowCommander* commander, DialogCallback* callback);
	virtual void OnXmlParseFailure(OpWindowCommander*) {}
#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	virtual void OnSearchSuggestionsRequested(OpWindowCommander* commander, const uni_char* url, OpSearchSuggestionsCallback* callback);
#endif

#ifdef EMBROWSER_SUPPORT
	virtual void OnUndisplay(OpWindowCommander* commander);
	virtual void OnLoadingCreated(OpWindowCommander* commander);
#endif // EMBROWSER_SUPPORT

	virtual void OnGeolocationAccess(OpWindowCommander* commander, const OpVector<uni_char>* hosts);
	virtual void OnCameraAccess(OpWindowCommander* commander, const OpVector<uni_char>* hosts);
	virtual OP_STATUS OnJSFullScreenRequest(OpWindowCommander* commander, BOOL enable_fullscreen);

	// Implementing OpMenuListener
	virtual void OnPopupMenu(OpWindowCommander* commander, OpDocumentContext& context, const OpPopupMenuRequestMessage* message);
# ifdef WIC_MIDCLICK_SUPPORT
	virtual void OnMidClick(OpWindowCommander * commander, OpDocumentContext& context,
							BOOL mousedown, ShiftKeyState modifiers);
# endif // WIC_MIDCLICK_SUPPORT
	virtual void OnMenuDispose(OpWindowCommander* commander) { }

	// Implementing OpDocumentListener
	virtual void OnImageModeChanged(OpWindowCommander* commander, ImageDisplayMode mode) {}
	virtual void OnSecurityModeChanged(OpWindowCommander* commander, OpDocumentListener::SecurityMode mode, BOOL inline_elt);
#ifdef TRUST_RATING
	virtual void OnTrustRatingChanged(OpWindowCommander* commander, TrustRating new_rating);
#endif // TRUST_RATING
	virtual void OnCssModeChanged(OpWindowCommander* commander, CssDisplayMode mode)  {}
	virtual void OnHandheldChanged(OpWindowCommander* commander, BOOL handheld) {}
#ifdef _PRINT_SUPPORT_
	virtual void OnPrintPreviewModeChanged(OpWindowCommander* commander, PrintPreviewMode mode) {}
#endif // _PRINT_SUPPORT_
	virtual void OnTitleChanged(OpWindowCommander* commander, const uni_char* title);
	virtual void OnLinkClicked(OpWindowCommander* commander);
	virtual void OnLinkNavigated(OpWindowCommander* commander, const uni_char* url, const uni_char* link_title, BOOL is_image, const OpRect* link_rect = NULL) { }
#ifdef SUPPORT_VISUAL_ADBLOCK
	virtual void OnContentBlocked(OpWindowCommander* commander, const uni_char *image_url);
	virtual void OnContentUnblocked(OpWindowCommander* commander, const uni_char *image_url);
#endif // SUPPORT_VISUAL_ADBLOCK
	virtual void OnAccessKeyUsed(OpWindowCommander* commander);
	virtual void OnNoNavigation(OpWindowCommander* commander) {}
	virtual void OnHover(OpWindowCommander* commander, const uni_char* url, const uni_char* link_title, BOOL is_image);
	virtual void OnNoHover(OpWindowCommander* commander);
	virtual void OnTabFocusChanged(OpWindowCommander* commander, OpDocumentContext& context);
	virtual void OnSearchReset(OpWindowCommander* commander);
	virtual void OnSearchHit(OpWindowCommander* commander);
	virtual void OnDocumentIconAdded(OpWindowCommander* commander);
	virtual void OnDocumentIconRemoved(OpWindowCommander* commander) {}
	virtual void OnScaleChanged(OpWindowCommander* commander, UINT32 newScale);
	virtual void OnInnerSizeRequest(OpWindowCommander* commander, UINT32 width, UINT32 height);
	virtual void OnGetInnerSize(OpWindowCommander* commander, UINT32* width, UINT32* height);
	virtual void OnOuterSizeRequest(OpWindowCommander* commander, UINT32 width, UINT32 height);
	virtual void OnGetOuterSize(OpWindowCommander* commander, UINT32* width, UINT32* height);
	virtual void OnMoveRequest(OpWindowCommander* commander, INT32 x, INT32 y);
	virtual void OnGetPosition(OpWindowCommander* commander, INT32* x, INT32* y);
	virtual void OnWindowAttachRequest(OpWindowCommander* commander, BOOL attached);
	virtual void OnGetWindowAttached(OpWindowCommander* commander, BOOL* attached);
	virtual void OnClose(OpWindowCommander* commander);
	virtual void OnRaiseRequest(OpWindowCommander* commander);
	virtual void OnLowerRequest(OpWindowCommander* commander);
	virtual void OnDrag(OpWindowCommander* commander, const OpRect rect, const uni_char* link_url, const uni_char* link_title, const uni_char* image_url);
#ifdef DND_DRAGOBJECT_SUPPORT
	virtual void OnDragLeave(OpWindowCommander* commander, class DesktopDragObject* drag_object);
	virtual void OnDragMove(OpWindowCommander* commander, class DesktopDragObject* drag_object, const OpPoint& point);
	virtual void OnDragDrop(OpWindowCommander* commander, class DesktopDragObject* drag_object, const OpPoint& point);
#endif // DND_DRAGOBJECT_SUPPORT

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	virtual void OnSearchSuggestionsUsed(OpWindowCommander* commander, BOOL suggestions);
#endif // DOC_SEARCH_SUGGESTIONS_SUPPORT

#ifdef GADGET_SUPPORT
	virtual BOOL HasGadgetReceivedDragRequest(OpWindowCommander* commander) { return FALSE; }
	virtual void OnGadgetDragRequest(OpWindowCommander* commander) {}
	virtual void CancelGadgetDragRequest(OpWindowCommander* commander) {}
	virtual void OnGadgetDragStart(OpWindowCommander* commander, INT32 x, INT32 y) {}
	virtual void OnGadgetDragMove(OpWindowCommander* commander, INT32 x, INT32 y) {}
	virtual void OnGadgetDragStop(OpWindowCommander* commander) {}
	virtual BOOL OnGadgetClick(OpWindowCommander* commander, const OpPoint &point, MouseButton button, UINT8 nclicks) {return FALSE;}
	virtual void OnGadgetPaint(OpWindowCommander* commander, VisualDevice* vis_dev) {}
	virtual void OnGadgetGetAttention(OpWindowCommander* commander) {}
	virtual void OnGadgetShowNotification(OpWindowCommander* commander, const uni_char* message, GadgetShowNotificationCallback* callback) {}
	virtual void OnGadgetShowNotificationCancel(OpWindowCommander* commander) {}
	virtual void OnGadgetInstall(OpWindowCommander* commander, URLInformation* url_info);
#ifdef WIDGET_CONTROL_BUTTONS_SUPPORT	
	virtual void OnMouseGadgetEnter(OpWindowCommander* commander) {};
	virtual void OnMouseGadgetLeave(OpWindowCommander* commander) {};
#endif // WIDGET_CONTROL_BUTTONS_SUPPORT

#endif // GADGET_SUPPORT
	virtual void OnStatusText(OpWindowCommander* commander, const uni_char* text);
	virtual void OnConfirm(OpWindowCommander* commander, const uni_char *message, OpDocumentListener::DialogCallback *callback);
	virtual void OnGenericError(OpWindowCommander* commander, const uni_char *title, const uni_char *message, const uni_char *additional = NULL);
	virtual void OnJSAlert(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, OpDocumentListener::JSDialogCallback *callback);
	virtual void OnJSConfirm(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, OpDocumentListener::JSDialogCallback *callback);
	virtual void OnJSPrompt(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, const uni_char *default_value, OpDocumentListener::JSDialogCallback *callback);
	virtual void OnJSPopup(OpWindowCommander* commander, const uni_char *url, const uni_char *name, int left, int top, int width, int height, BOOL3 scrollbars, BOOL3 location, BOOL refuse, BOOL unrequested, OpDocumentListener::JSPopupCallback *callback);
#if defined WIC_CAP_JAVASCRIPT_PRINT && defined _PRINT_SUPPORT_
	virtual void OnJSPrint(OpWindowCommander * commander, PrintCallback * callback);
#endif // WIC_CAP_JAVASCRIPT_PRINT && _PRINT_SUPPORT_
#if defined WIC_CAP_JAVASCRIPT_PRINT && defined _PRINT_SUPPORT_
	virtual void OnUserPrint(OpWindowCommander * commander, PrintCallback * callback);
#endif // WIC_CAP_JAVASCRIPT_PRINT && _PRINT_SUPPORT_
#if defined (DOM_FILE_API_SUPPORT) && defined (WIC_CAP_HAS_MOUNTPOINT_API)
	virtual void OnMountpointFolderRequest(OpWindowCommander* commander, MountPointFolderCallback* callback, MountPointFolderCallback::FolderType folder_type);
	virtual void OnMountpointFolderCancel(OpWindowCommander* commander, MountPointFolderCallback* callback);
#endif // DOM_FILE_API_SUPPORT && WIC_CAP_HAS_MOUNTPOINT_API
	virtual void OnContentResized(OpWindowCommander* commander, INT32 new_width, INT32 new_height) {}
	virtual void OnContentDisplay(OpWindowCommander* commander) {}

	virtual void OnContentMagicFound(OpWindowCommander* commander, INT32 x, INT32 y) {}
	virtual void OnContentMagicLost(OpWindowCommander* commander) {}
	virtual void OnFormSubmit(OpWindowCommander* commander, OpDocumentListener::FormSubmitCallback *callback);
	virtual void OnPluginPost(OpWindowCommander* commander, OpDocumentListener::PluginPostCallback *callback);

	virtual void OnMailTo(OpWindowCommander* commander, const uni_char* url) {}
	virtual void OnMailTo(OpWindowCommander* commander, const OpStringC8& raw_mailto, BOOL mailto_from_dde, BOOL mailto_from_form, const OpStringC8& servername);

	virtual void OnTel(OpWindowCommander* commander, const uni_char* url) {}
	virtual BOOL OnUnknownProtocol(OpWindowCommander* commander, const uni_char* url) { return FALSE; }
	virtual void OnUnhandledFiletype(OpWindowCommander* commander, const uni_char* url) {}
#ifdef _WML_SUPPORT_
	virtual void OnWmlDenyAccess(OpWindowCommander* commander) {}
#endif // _WML_SUPPORT_
	virtual void OnDownloadRequest(OpWindowCommander* commander, OpDocumentListener::DownloadCallback * callback);
	virtual BOOL OnRefreshUrl(OpWindowCommander* commander, const uni_char* url) { return TRUE; }
	virtual void OnRss(OpWindowCommander* commander, const uni_char* url) {}
	virtual void OnSubscribeFeed(OpWindowCommander* commander, const uni_char* url);
	virtual void OnAskAboutUrlWithUserName(OpWindowCommander* commander, const uni_char* url, const uni_char* hostname, const uni_char* username, OpDocumentListener::DialogCallback* callback);
	virtual void OnAskAboutFormRedirect(OpWindowCommander* commander, const uni_char* source_url, const uni_char* target_url, OpDocumentListener::DialogCallback* callback);
	virtual void OnFormRequestTimeout(OpWindowCommander* commander, const uni_char* url);
	virtual BOOL OnAnchorSpecial(OpWindowCommander * commander, const OpDocumentListener::AnchorSpecialInfo & info);

	virtual void OnAskPluginDownload(OpWindowCommander * commander);

	virtual void OnVisibleRectChanged(OpWindowCommander * commander, const OpRect& visible_rect);

	virtual void OnOnDemandStateChangeNotify(OpWindowCommander* commander, BOOL has_placeholders);

	virtual void OnWebHandler(OpWindowCommander* commander, WebHandlerCallback* cb);
	virtual void OnWebHandlerCancelled(OpWindowCommander* commander, WebHandlerCallback* cb);
	virtual BOOL OnMailToWebHandlerRegister(OpWindowCommander* commander, const uni_char* url, const uni_char* description);
	virtual void OnMailToWebHandlerUnregister(OpWindowCommander* commander, const uni_char* url);
	virtual void IsMailToWebHandlerRegistered(OpWindowCommander* commander, MailtoWebHandlerCheckCallback* cb);
#ifdef PLUGIN_AUTO_INSTALL
	virtual void NotifyPluginDetected(OpWindowCommander* commander, const OpStringC& mime_type);
	virtual void NotifyPluginMissing(OpWindowCommander* commander, const OpStringC& mime_type);
	virtual void RequestPluginInfo(OpWindowCommander* commander, const OpStringC& mime_type, OpString& plugin_name, BOOL& available);
	virtual void RequestPluginInstallDialog(OpWindowCommander* commander, const PluginInstallInformation& information);
#endif // PLUGIN_AUTO_INSTALL

#if defined SUPPORT_DATABASE_INTERNAL
	virtual void OnIncreaseQuota(OpWindowCommander* commander, const uni_char* db_name, const uni_char* db_domain, const uni_char* db_type, OpFileLength current_quota_size, OpFileLength quota_hint, OpDocumentListener::QuotaCallback *callback);
#endif // SUPPORT_DATABASE_INTERNAL

	virtual void OnQueryGoOnline(OpWindowCommander* commander, const uni_char *message, DialogCallback *callback);

#ifdef DRAG_SUPPORT
	virtual void OnFileDropConfirm(OpWindowCommander* commander, const uni_char *server_name, DialogCallback *callback);
#endif // DRAG_SUPPORT

	virtual void OnDocumentScroll(OpWindowCommander* commander);

	virtual void OnSetupInstall(OpWindowCommander* commander, const uni_char* url, URLContentType content_type);
	virtual void OnUnexpectedComponentGone(OpWindowCommander* commander, OpComponentType type, const OpMessageAddress& address, const OpStringC& information);
	virtual void OnKeyboardSelectionModeChanged(OpWindowCommander* commander, BOOL enabled) {}

protected:

	virtual BOOL HasCurrentElement();

private:
	
	BOOL GetSupportsSmallScreen() {return m_flags.supports_small_screen;}
	BOOL GetSupportsNavigation() {return m_flags.supports_navigation;}
	BOOL WasStoppedByUser() {return m_flags.stopped_by_user;}
	void RefreshSecurityInformation(SecurityMode mode);
	void BroadcastPageDestroyed();

	struct {
		unsigned int stopped_by_user:1;
		unsigned int supports_navigation:1;
		unsigned int supports_small_screen:1;
		unsigned int scroll_to_end_if_appropriate:1;
	} m_flags;

	OpListeners<OpPageListener> m_listeners;
	OpListeners<OpPageAdvancedListener> m_advanced_listeners;
	OpWindowCommander* m_window_commander;
	OpDocumentListener::JSPopupCallback* m_current_popup_callback;

	/**
	 * Used for performance enhancing caching of security button text,
	 * as extraction of this information requires some parsing of
	 * xml, which is expensive. It is the OpPageView's
	 * responsibility to make sure this cached string is up to date,
	 * the classes asking for will not be double checking this.
	 */
	OpString m_security_information;

	OpWidgetImage m_document_icon;

	OpString m_title;

	/**
	 * Since the audio support was removed from core, we need to play the "loading finished" sound
	 * on our own now. We need to use the existing core callbacks, however since there doesn't
	 * seem to be any single callback that could be used to reliably determine the "page loaded"
	 * event, we use OnNewPageDisplayed() to set the m_play_loaded_flag and OnLoadingFinished()
	 * to play the sound if the flag is set and reset it.
	 * The reason for that is that we get multiple calls to OnLoadingFinished() during loading
	 * of a single page.
	 */
	bool m_play_loaded_sound;

	bool m_any_pending_authentication;
};

#endif // OP_PAGE_H
