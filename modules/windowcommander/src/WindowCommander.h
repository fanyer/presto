/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef WINDOW_COMMANDER_H
#define WINDOW_COMMANDER_H

#include "modules/windowcommander/OpExtensionUIListener.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/OpTransferManager.h"
#include "modules/windowcommander/src/CookieManager.h"

class URL;
class CSS;
class DocumentManager;
class HTML_Element;
class JS_Window;

class NullLoadingListener : public OpLoadingListener
{
public:
	~NullLoadingListener() {};

	void OnUrlChanged(OpWindowCommander* commander, const uni_char* url) {}
	void OnStartLoading(OpWindowCommander* commander) {}
	void OnLoadingProgress(OpWindowCommander* commander, const LoadingInformation* info) {}
	void OnRedirect(OpWindowCommander* commander) {}
	void OnLoadingFinished(OpWindowCommander* commander, LoadingFinishStatus status) {}
	void OnAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback) {}
	void OnStartUploading(OpWindowCommander* commander) {}
	void OnUploadingFinished(OpWindowCommander* commander, LoadingFinishStatus status) {}
	BOOL OnLoadingFailed(OpWindowCommander* commander, int msg_id, const uni_char* url) { return FALSE; }
#ifdef EMBROWSER_SUPPORT
	void OnUndisplay(OpWindowCommander* commander) {}
	void OnLoadingCreated(OpWindowCommander* commander) {}
#endif // EMBROWSER_SUPPORT
#ifdef URL_MOVED_EVENT
	void OnUrlMoved(OpWindowCommander* commander, const uni_char* url) {}
#endif // URL_MOVED_EVENT
	void OnXmlParseFailure(OpWindowCommander* commander){}
#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	void OnSearchSuggestionsRequested(OpWindowCommander* commander, const uni_char* url, OpSearchSuggestionsCallback* callback) {}
#endif
#ifdef XML_AIT_SUPPORT
	OP_STATUS OnAITDocument(OpWindowCommander* commander, AITData* ait_data) { return OpStatus::OK; }
#endif // XML_AIT_SUPPORT
};

class NullDocumentListener : public OpDocumentListener
{
public:
	~NullDocumentListener() {}
	void OnImageModeChanged(OpWindowCommander* commander, ImageDisplayMode mode) {}
	void OnSecurityModeChanged(OpWindowCommander* commander, SecurityMode mode, BOOL inline_elt) {}
#ifdef TRUST_RATING
	void OnTrustRatingChanged(OpWindowCommander* commander, TrustRating new_rating) {}
#endif // TRUST_RATING
	void OnCssModeChanged(OpWindowCommander* commander, CssDisplayMode mode) {}
	void OnTabFocusChanged(OpWindowCommander* commander, OpDocumentContext& context) {}
	void OnHandheldChanged(OpWindowCommander* commander, BOOL handheld) {}
#ifdef _PRINT_SUPPORT_
	void OnPrintPreviewModeChanged(OpWindowCommander* commander, PrintPreviewMode mode) {}
#endif
	void OnTitleChanged(OpWindowCommander* commander, const uni_char* title) {}
	void OnLinkClicked(OpWindowCommander* commander) {}
	void OnLinkNavigated(OpWindowCommander* commander, const uni_char* url, const uni_char* link_title, BOOL is_image, const OpRect* link_rect) {}
	void OnNoNavigation(OpWindowCommander* commander) {}
	void OnHover(OpWindowCommander* commander, const uni_char* url, const uni_char* link_title, BOOL is_image) {}
	void OnNoHover(OpWindowCommander* commander) {}
#ifdef SUPPORT_VISUAL_ADBLOCK
	void OnContentBlocked(OpWindowCommander* commander, const uni_char *image_url) {}
	void OnContentUnblocked(OpWindowCommander* commander, const uni_char *image_url) {}
#endif // SUPPORT_VISUAL_ADBLOCK
#ifdef ACCESS_KEYS_SUPPORT
	void OnAccessKeyUsed(OpWindowCommander* commander) {}
#endif // ACCESS_KEYS_SUPPORT
#ifndef HAS_NO_SEARCHTEXT
	void OnSearchReset(OpWindowCommander* commander) {}
# ifdef WIC_SEARCH_MATCHES
	void OnSearchHit(OpWindowCommander* commander) {}
# endif // WIC_SEARCH_MATCHES
#endif // HAS_NO_SEARCHTEXT
#ifdef SHORTCUT_ICON_SUPPORT
	void OnDocumentIconAdded(OpWindowCommander* commander) {}
	void OnDocumentIconRemoved(OpWindowCommander* commander) {}
#endif // SHORTCUT_ICON_SUPPORT
	void OnScaleChanged(OpWindowCommander* commander, UINT32 newScale) {}

#if LAYOUT_MAX_REFLOWS > 0
	void OnReflowStuck(OpWindowCommander* commander) {}
#endif
#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	void OnSearchSuggestionsUsed(OpWindowCommander* commander, BOOL suggestions) {}
#endif // DOC_SEARCH_SUGGESTIONS_SUPPORT

#ifdef GADGET_SUPPORT
    BOOL HasGadgetReceivedDragRequest(OpWindowCommander* commander) { return FALSE; }
    void OnGadgetDragRequest(OpWindowCommander* commander) {}
	void CancelGadgetDragRequest(OpWindowCommander* commander) {}
    void OnGadgetDragStart(OpWindowCommander* commander, INT32 x, INT32 y) {}
    void OnGadgetDragMove(OpWindowCommander* commander, INT32 x, INT32 y) {}
	void OnGadgetDragStop(OpWindowCommander* commander) {}
    BOOL OnGadgetClick(OpWindowCommander* commander, const OpPoint &point, MouseButton button, UINT8 nclicks) {return FALSE;}
    void OnGadgetPaint(OpWindowCommander* commander, VisualDevice* vis_dev) {}
	void OnGadgetGetAttention(OpWindowCommander* commander) {}
	void OnGadgetShowNotification(OpWindowCommander* commander, const uni_char* message, OpDocumentListener::GadgetShowNotificationCallback* callback) { callback->OnShowNotificationFinished(GadgetShowNotificationCallback::REPLY_IGNORED); }
	void OnGadgetShowNotificationCancel(OpWindowCommander* commander) {}
# ifdef WIC_USE_DOWNLOAD_CALLBACK
	void OnGadgetInstall(OpWindowCommander* commander, URLInformation* url_info) {url_info->URL_Information_Done();}
# endif // WIC_USE_DOWNLOAD_CALLBACK
# ifdef DRAG_SUPPORT
	void OnMouseGadgetEnter(OpWindowCommander* commander) {};
	void OnMouseGadgetLeave(OpWindowCommander* commander) {};
# endif // DRAG_SUPPORT
#endif // GADGET_SUPPORT

	void OnInnerSizeRequest(OpWindowCommander* commander, UINT32 width, UINT32 height) {}
	void OnGetInnerSize(OpWindowCommander* commander, UINT32* width, UINT32* height) {}
	void OnOuterSizeRequest(OpWindowCommander* commander, UINT32 width, UINT32 height) {}
	void OnGetOuterSize(OpWindowCommander* commander, UINT32* width, UINT32* height) {}
	void OnMoveRequest(OpWindowCommander* commander, INT32 x, INT32 y) {}
	void OnGetPosition(OpWindowCommander* commander, INT32* x, INT32* y) {}
#ifdef WIC_USE_WINDOW_ATTACHMENT
	void OnWindowAttachRequest(OpWindowCommander* commander, BOOL attached) {}
	void OnGetWindowAttached(OpWindowCommander* commander, BOOL* attached) { *attached = FALSE; }
#endif // WIC_USE_WINDOW_ATTACHMENT
	void OnClose(OpWindowCommander* commander) {}
	void OnRaiseRequest(OpWindowCommander* commander) {}
	void OnLowerRequest(OpWindowCommander* commander) {}
	void OnStatusText(OpWindowCommander* commander, const uni_char* text) {}
	void OnMailTo(OpWindowCommander* commander, const OpStringC8& raw_url, BOOL called_externally, BOOL mailto_from_form, const OpStringC8& servername) {}
	void OnTel(OpWindowCommander* commander, const uni_char* url) {}
	BOOL OnUnknownProtocol(OpWindowCommander* commander, const uni_char* url) { return FALSE; }
	void OnUnhandledFiletype(OpWindowCommander* commander, const uni_char* url) {}
	void OnWmlDenyAccess(OpWindowCommander* commander) {}
#ifdef XHR_SECURITY_OVERRIDE
	XHRPermission OnXHR( OpWindowCommander* commander, const uni_char* source_url, const uni_char* target_url) { return AUTO; }
#endif // XHR_SECURITY_OVERRIDE
	BOOL OnRefreshUrl(OpWindowCommander* commander, const uni_char* url) { return TRUE; }
#ifdef WIC_USE_DOWNLOAD_CALLBACK
	void OnDownloadRequest(OpWindowCommander* commander, DownloadCallback * callback) {}
#else // !WIC_USE_DOWNLOAD_CALLBACK
	void OnDownloadRequest(OpWindowCommander* commander, const uni_char* url, const uni_char* suggested_filename, const char* mimetype, OpFileLength size, DownloadRequestMode mode, const uni_char* suggested_savepath = NULL) { commander->CancelTransfer(commander, url); }
#endif // !WIC_USE_DOWNLOAD_CALLBACK
#ifdef WEB_HANDLERS_SUPPORT
	void OnWebHandler(OpWindowCommander* commander, WebHandlerCallback* cb) { cb->OnCancel(); }
	void OnWebHandlerCancelled(OpWindowCommander* commander, WebHandlerCallback* cb) { }
	BOOL OnMailToWebHandlerRegister(OpWindowCommander* commander, const uni_char* url, const uni_char* description) { return FALSE; }
	void OnMailToWebHandlerUnregister(OpWindowCommander* commander, const uni_char* url) {}
	void IsMailToWebHandlerRegistered(OpWindowCommander* commander, MailtoWebHandlerCheckCallback* cb) { cb->OnCompleted(FALSE); }
#endif // WEB_HANDLERS_SUPPORT
#ifdef RSS_SUPPORT
	void OnRss(OpWindowCommander* commander, const uni_char* url) {};
# ifdef WIC_SUBSCRIBE_FEED
	void OnSubscribeFeed(OpWindowCommander* commander, const uni_char* url) {}
# endif // WIC_SUBSCRIBE_FEED
#endif // RSS_SUPPORT

	virtual void OnJSAlert(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, JSDialogCallback *callback) { callback->OnAlertDismissed(); }
	virtual void OnJSConfirm(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, JSDialogCallback *callback) { callback->OnConfirmDismissed(FALSE); }
	virtual void OnJSPrompt(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, const uni_char *default_value, JSDialogCallback *callback) { callback->OnPromptDismissed(FALSE, UNI_L("")); }

	virtual void OnJSPopup(OpWindowCommander* commander, const uni_char *url, const uni_char *name, int left, int top, int width, int height, BOOL3 scrollbars, BOOL3 location, BOOL refuse, BOOL unrequested, JSPopupCallback *callback) { callback->Continue(); }

#if defined(USER_JAVASCRIPT) && defined(REVIEW_USER_SCRIPTS)
	virtual BOOL OnReviewUserScripts(OpWindowCommander *wc, const uni_char *hostname, const uni_char *preference_value, OpString &reviewed_value) { return FALSE; }
#endif // USER_JAVASCRIPT && USER_JAVASCRIPT
	virtual void OnFormSubmit(OpWindowCommander* commander, FormSubmitCallback *callback) { callback->Continue(TRUE); }
#ifdef _PLUGIN_SUPPORT_
    virtual void OnPluginPost(OpWindowCommander* commander, PluginPostCallback *callback) { callback->Continue(TRUE); }
#endif // _PLUGIN_SUPPORT_

	virtual void OnCancelDialog(OpWindowCommander* commander, DialogCallback* callback) { }
	virtual void OnConfirm(OpWindowCommander* commander, const uni_char *message, DialogCallback *callback) { callback->OnDialogReply(DialogCallback::REPLY_YES); }
	virtual void OnGenericError(OpWindowCommander* commander, const uni_char *title, const uni_char *message, const uni_char *additional = NULL) {}
	virtual void OnQueryGoOnline(OpWindowCommander* commander, const uni_char *message, DialogCallback *callback) { callback->OnDialogReply(DialogCallback::REPLY_CANCEL); }
#ifdef DRAG_SUPPORT
	virtual void OnFileDropConfirm(OpWindowCommander* commander, const uni_char *server, DialogCallback *callback) { callback->OnDialogReply(DialogCallback::REPLY_CANCEL); }
#endif // DRAG_SUPPORT
	virtual void OnAskAboutUrlWithUserName(OpWindowCommander* commander, const uni_char* url, const uni_char* hostname, const uni_char* username, DialogCallback* callback) { callback->OnDialogReply(DialogCallback::REPLY_YES); }
	virtual void OnAskAboutFormRedirect(OpWindowCommander* commander, const uni_char* source_url, const uni_char* target_url, DialogCallback* callback)  { callback->OnDialogReply(DialogCallback::REPLY_YES); }
	virtual void OnFormRequestTimeout(OpWindowCommander* commander, const uni_char* url) {}

#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	virtual void OnIncreaseQuota(OpWindowCommander* commander, const uni_char* db_name, const uni_char* db_domain, const uni_char* db_type, OpFileLength current_quota_size, OpFileLength quota_hint, QuotaCallback *callback) { callback->OnQuotaReply(FALSE, 0); }
#endif // DATABASE_MODULE_MANAGER_SUPPORT
#ifdef WIC_USE_ANCHOR_SPECIAL
	virtual BOOL OnAnchorSpecial(OpWindowCommander*, const AnchorSpecialInfo&) {return FALSE;}
#endif // WIC_USE_ANCHOR_SPECIAL
#ifdef WIC_USE_ASK_PLUGIN_DOWNLOAD
	virtual void OnAskPluginDownload(OpWindowCommander* commander) {}
#endif // WIC_USE_ASK_PLUGIN_DOWNLOAD
#ifdef WIC_USE_VIS_RECT_CHANGE
	void OnVisibleRectChanged(OpWindowCommander* commander, const OpRect& visible_rect) {}
#endif // WIC_USE_VIS_RECT_CHANGE
	virtual void OnOnDemandStateChangeNotify(OpWindowCommander* commander, BOOL has_placeholders) { }
#ifdef WIC_ORIENTATION_CHANGE
	void OnOrientationChangeRequest(OpWindowCommander* commander, Orientation orientation) {}
#endif //WIC_ORIENTATION_CHANGE

#ifdef WIDGETS_IMS_SUPPORT
	virtual OP_STATUS OnIMSRequested(OpWindowCommander* commander, IMSCallback* callback) { return OpStatus::ERR; }
	virtual void OnIMSCancelled(OpWindowCommander* commander, IMSCallback* callback) {}
	virtual void OnIMSUpdated(OpWindowCommander * commander, IMSCallback* callback) {}
#endif // WIDGETS_IMS_SUPPORT

#ifdef COOKIE_WARNING_SUPPORT
	virtual void OnCookieWriteDenied(OpWindowCommander* commander) {}
#endif // COOKIE_WARNING_SUPPORT

#ifdef DOC_RETURN_TOUCH_EVENT_TO_SENDER
	BOOL OnTouchEventProcessed(OpWindowCommander* commander, void* user_data, BOOL prevented) { return FALSE; }
#endif // DOC_RETURN_TOUCH_EVENT_TO_SENDER

	virtual void OnDocumentScroll(OpWindowCommander* commander) { }
	virtual void OnUnexpectedComponentGone(OpWindowCommander* commander, OpComponentType type, const OpMessageAddress& address, const OpStringC& information) {}

#ifdef WIC_SETUP_INSTALL_CALLBACK
	virtual void OnSetupInstall(OpWindowCommander* commander, const uni_char* url, URLContentType content_type) {}
#endif // WIC_SETUP_INSTALL_CALLBACK

#ifdef PLUGIN_AUTO_INSTALL
	virtual void NotifyPluginDetected(OpWindowCommander* commander, const OpStringC& mime_type) {}
	virtual void NotifyPluginMissing(OpWindowCommander* commander, const OpStringC& mime_type) {}
	virtual void RequestPluginInfo(OpWindowCommander* commander, const OpStringC& mime_type, OpString& plugin_name, BOOL& available) {}
	virtual void RequestPluginInstallDialog(OpWindowCommander* commander, const PluginInstallInformation& information) {}
#endif // PLUGIN_AUTO_INSTALL


#ifdef REFRESH_DELAY_OVERRIDE
	virtual void OnReviewRefreshDelay(OpWindowCommander* wc, OpDocumentListener::ReviewRefreshDelayCallback *cb) { cb->Default(); };
#endif // REFRESH_DELAY_OVERRIDE

#ifdef KEYBOARD_SELECTION_SUPPORT
	virtual void OnKeyboardSelectionModeChanged(OpWindowCommander* wc, BOOL enabled) {}
#endif // KEYBOARD_SELECTION_SUPPORT

#ifdef GEOLOCATION_SUPPORT
	virtual void OnGeolocationAccess(OpWindowCommander* wc, const OpVector<uni_char>* hostnames) {}
#endif // GEOLOCATION_SUPPORT

#ifdef MEDIA_CAMERA_SUPPORT
	virtual void OnCameraAccess(OpWindowCommander* wc, const OpVector<uni_char>* hostnames) { }
#endif // MEDIA_CAMERA_SUPPORT

#ifdef DOM_FULLSCREEN_MODE
	virtual OP_STATUS OnJSFullScreenRequest(OpWindowCommander* commander, BOOL enable_fullscreen){ return OpStatus::ERR; }
#endif // DOM_FULLSCREEN_MODE
};

#ifdef _POPUP_MENU_SUPPORT_
class NullMenuListener : public OpMenuListener
{
public:
	~NullMenuListener() {}

	void OnPopupMenu(OpWindowCommander* commander, OpDocumentContext& context, const OpPopupMenuRequestMessage* message) {}
# ifdef WIC_MIDCLICK_SUPPORT
	void OnMidClick(OpWindowCommander* commander, OpDocumentContext& ctx, BOOL mousedown, ShiftKeyState modifiers) {}
# endif // WIC_MIDCLICK_SUPPORT

	void OnMenuDispose(OpWindowCommander* commander) {}
};
#endif // _POPUP_MENU_SUPPORT_

class NullLinkListener : public OpLinkListener
{
public:
	~NullLinkListener() {}
	void OnLinkElementAvailable(OpWindowCommander* commander) {}
	void OnAlternateCssAvailable(OpWindowCommander* commander);
};

#ifdef ACCESS_KEYS_SUPPORT
class NullAccessKeyListener : public OpAccessKeyListener
{
public:
	~NullAccessKeyListener() {}
	void OnAccessKeyAdded(OpWindowCommander* commander, OpKey::Code key, const uni_char* caption, const uni_char* url) {}
	void OnAccessKeyRemoved(OpWindowCommander* commander, OpKey::Code key) {}
};
#endif // ACCESS_KEYS_SUPPORT

#ifdef _PRINT_SUPPORT_
class NullPrintingListener : public OpPrintingListener
{
public:
	~NullPrintingListener() {}	
	void OnStartPrinting(OpWindowCommander* commander) {}
	void OnPrintStatus(OpWindowCommander* commander, const PrintInfo* info) {}
};
#endif // _PRINT_SUPPORT_

class NullErrorListener : public OpErrorListener
{
public:
	~NullErrorListener() {}
	void OnOperaDocumentError(OpWindowCommander* commander, OperaDocumentError error) {};
	void OnFileError(OpWindowCommander* commander, FileError error) {};
	void OnHttpError(OpWindowCommander* commander, HttpError error) {};
	void OnDsmccError(OpWindowCommander* commander, DsmccError error) {};
#ifdef _SSL_SUPPORT_
	void OnTlsError(OpWindowCommander* commander, TlsError error) {};
#endif // _SSL_SUPPORT_
	void OnNetworkError(OpWindowCommander* commander, NetworkError error) {};
	void OnBoundaryError(OpWindowCommander* commander, BoundaryError error) {};
	void OnRedirectionRegexpError(OpWindowCommander* commander) {};
	void OnOomError(OpWindowCommander* commander) {};
	void OnSoftOomError(OpWindowCommander* commander) {};
};


#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
class NullSSLListener : public OpSSLListener
{
public:
	~NullSSLListener() {}

	void OnCertificateBrowsingNeeded(OpWindowCommander* wic, SSLCertificateContext* context, SSLCertificateReason reason, SSLCertificateOption options)
		{ context->OnCertificateBrowsingDone(FALSE, SSL_CERT_OPTION_NONE); }

	void OnCertificateBrowsingCancel(OpWindowCommander* wic, SSLCertificateContext* context)
		{ }

	void OnSecurityPasswordNeeded(OpWindowCommander* wic, SSLSecurityPasswordCallback* callback)
		{ callback->OnSecurityPasswordDone(FALSE, NULL, NULL); }

	void OnSecurityPasswordCancel(OpWindowCommander* wic, SSLSecurityPasswordCallback* callback)
		{ }
};
#endif // _SSL_SUPPORT_ && _NATIVE_SSL_SUPPORT_ || WIC_USE_SSL_LISTENER


#ifdef WIC_FILESELECTION_LISTENER
class NullFileSelectionListener : public OpFileSelectionListener
{
public:
	BOOL OnRequestPermission(OpWindowCommander* commander) { return FALSE; }

# ifdef _FILE_UPLOAD_SUPPORT_
	void OnUploadFilesRequest(OpWindowCommander* commander, UploadCallback* callback) { callback->OnFilesSelected(0); }
	void OnUploadFilesCancel(OpWindowCommander* commander) { }
# endif // _FILE_UPLOAD_SUPPORT_

# ifdef DOM_GADGET_FILE_API_SUPPORT
	void OnDomFilesystemFilesRequest(OpWindowCommander* commander, DomFilesystemCallback* callback) { callback->OnFilesSelected(0); }
	void OnDomFilesystemFilesCancel(OpWindowCommander* commander) { }
# endif // DOM_GADGET_FILE_API_SUPPORT
};
#endif // WIC_FILESELECTION_LISTENER

#ifdef WIC_COLORSELECTION_LISTENER
class NullColorSelectionListener : public OpColorSelectionListener
{
public:
	virtual void OnColorSelectionRequest(OpWindowCommander* commander, ColorSelectionCallback* callback) { callback->OnColorSelected(callback->GetInitialColor()); }
	virtual void OnColorSelectionCancel(OpWindowCommander* commander) {}
};
#endif // WIC_COLORSELECTION_LISTENER

#ifdef NEARBY_ELEMENT_DETECTION
class NullFingertouchListener : public OpFingertouchListener
{
public:
	void OnElementsExpanding(OpWindowCommander* wic) {}
	void OnElementsExpanded(OpWindowCommander* wic) {}
	void OnElementsHiding(OpWindowCommander* wic) {}
	void OnElementsHidden(OpWindowCommander* wic) {}
};
#endif // NEARBY_ELEMENT_DETECTION

#ifdef WIC_PERMISSION_LISTENER
class NullPermissionListener : public OpPermissionListener
{
public:
	void OnAskForPermission(OpWindowCommander *commander, PermissionCallback *callback);
	void OnAskForPermissionCancelled(OpWindowCommander *commander, PermissionCallback *callback) {}
# ifdef WIC_SET_PERMISSION
	void OnUserConsentInformation(OpWindowCommander *commander, const OpVector<OpPermissionListener::ConsentInformation> &consent_information, INTPTR user_data) {}
	void OnUserConsentError(OpWindowCommander *wic, OP_STATUS error, INTPTR user_data) {}
# endif // WIC_SET_PERMISSION
};
#endif // WIC_PERMISSION_LISTENER

#ifdef WIC_EXTERNAL_APPLICATIONS_ENUMERATION
class NullApplicationListener : public OpApplicationListener
{
public:
	void GetInstalledApplications(GetInstalledApplicationsCallback * callback) { callback->OnFinished(); }
	OP_STATUS ExecuteApplication(ApplicationType type, unsigned int argc, const uni_char * const * argv) { return OpStatus::ERR_NO_SUCH_RESOURCE; }
};
#endif // WIC_EXTERNAL_APPLICATIONS_ENUMERATION


#ifdef DOM_TO_PLATFORM_MESSAGES
class NullPlatformMessageListener: public OpPlatformMessageListener
{
public:
	void OnMessage(OpWindowCommander* wic, PlatformMessageContext *context, const uni_char *message, const uni_char *url) {}
};
#endif // DOM_TO_PLATFORM_MESSAGES

#ifdef APPLICATION_CACHE_SUPPORT
class NullApplicationCacheListener : public OpApplicationCacheListener
{
public:
	/* events that must be answered. We trigger OP_ASSERTs if null listener is set when getting callbacks. */
	virtual void OnDownloadAppCache(OpWindowCommander* commander, UINTPTR id, InstallAppCacheCallback* callback){ OP_ASSERT(!"Error: listener not set"); callback->OnDownloadAppCacheReply(TRUE); }
	virtual void CancelDownloadAppCache(OpWindowCommander* commander, UINTPTR id) { }
	virtual void OnCheckForNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id, InstallAppCacheCallback* callback) { OP_ASSERT(!"Error: listener not set"); callback->OnCheckForNewAppCacheVersionReply(TRUE); }
	virtual void CancelCheckForNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id){ }
	virtual void OnDownloadNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id, InstallAppCacheCallback* callback){ OP_ASSERT(!"Error: listener not set"); callback->OnDownloadNewAppCacheVersionReply(TRUE); }
	virtual void CancelDownloadNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id){ }
	virtual void OnIncreaseAppCacheQuota(OpWindowCommander* commander, UINTPTR id, const uni_char* cache_domain, OpFileLength current_quota_size, QuotaCallback *callback) { OP_ASSERT(!"Error: listener not set");callback->OnQuotaReply(TRUE, current_quota_size + 512*1024); }
	virtual void CancelIncreaseAppCacheQuota(OpWindowCommander* commander, UINTPTR id) {  }

	/* notification events */
	virtual void OnAppCacheChecking(OpWindowCommander* commander){}
	virtual void OnAppCacheError(OpWindowCommander* commander){}
	virtual void OnAppCacheNoUpdate(OpWindowCommander* commander){}
	virtual void OnAppCacheDownloading(OpWindowCommander* commander){}
	virtual void OnAppCacheProgress(OpWindowCommander* commander){}
	virtual void OnAppCacheUpdateReady(OpWindowCommander* commander){}
	virtual void OnAppCacheCached(OpWindowCommander* commander){}
	virtual void OnAppCacheObsolete(OpWindowCommander* commander){}

};
#endif // APPLICATION_CACHE_SUPPORT

#ifdef WIC_OS_INTERACTION
class NullOSInteractionListener : public OpOSInteractionListener
{
public:
	virtual void OnQueryDesktopBackgroundImageFormat(URLContentType content_type, QueryDesktopBackgroundImageFormatCallback* callback) { OP_ASSERT(callback); callback->OnFinished(OpStatus::ERR, static_cast<URLContentType>(0), NULL); }
	virtual void OnSetDesktopBackgroundImage(const uni_char* file_path, SetDesktopBackgroundImageCallback* callback) { if (callback) callback->OnFinished(OpStatus::ERR_NOT_SUPPORTED); }
};
#endif // WIC_OS_INTERACTION

#ifdef EXTENSION_SUPPORT
class NullExtensionUIListener : public OpExtensionUIListener
{
public:
	/* Events that must be answered. We trigger OP_ASSERTs if null listener is set when getting callbacks. */
	virtual void OnExtensionUIItemAddRequest(OpWindowCommander* commander, ExtensionUIItem *i) { OP_ASSERT(!"Error: listener not set"); if (i->callbacks) i->callbacks->ItemAdded(i->id, OpExtensionUIListener::ItemAddedFailed); }
	virtual void OnExtensionUIItemRemoveRequest(OpWindowCommander* commander, ExtensionUIItem *i) { if (i->callbacks) i->callbacks->ItemRemoved(i->id, OpExtensionUIListener::ItemRemovedFailed); }

	virtual void OnExtensionSpeedDialInfoUpdated(OpWindowCommander* commander) {};
};
#endif // EXTENSION_SUPPORT

#ifdef GOGI_SELFTEST_FINISHED_NOTIFICATION
class NullSelftestListener : public OpSelftestListener
{
public:
	void OnSelftestFinished() {}
};
#endif // GOGI_SELFTEST_FINISHED_NOTIFICATION

#ifdef WIC_LIMIT_PROGRESS_FREQUENCY
# include "modules/hardcore/timer/optimer.h"

/** @short Listener proxy for OpLoadingListener.
 *
 * This proxy is used to impose a maximum frequency of progress updates to the
 * product / UI (for performance reasons). It is achieved by inserting an
 * instance of this class between core and the OpLoadingListener implementation
 * on the product / UI side.
 *
 * (will be moved to a dedicated header file later, but right now that is
 * tricky, because it would involve circular header file dependencies)
 */
class LoadingListenerProxy :
	public OpLoadingListener,
	public OpTimerListener
{
public:
	LoadingListenerProxy(OpWindowCommander* commander);

	/** @short Set the actual OpLoadingListener defined by product / UI. */
	void SetLoadingListener(OpLoadingListener* listener);

	/** @short Get the actual OpLoadingListener defined by product / UI. */
	OpLoadingListener* GetLoadingListener() { return product_loadinglistener; }

private:
	class DelayedLoadingInformation : public LoadingInformation
	{
	public:
		OpString stored_loading_message;
	};

	OpString url;
	DelayedLoadingInformation loading_information;
	LoadingFinishStatus loading_finish_status;

	union
	{
		struct
		{
			unsigned int url_changed:1; ///< Pending call to OnUrlChanged()
			unsigned int start_stop_loading:1; ///< Pending call to OnStartLoading() or OnLoadingFinished()
			unsigned int loading_information:1; ///< Pending call to OnLoadingProgress()
		} pending_progress;
		UINT32 has_pending_progress;
	};

	OpWindowCommander* commander;

	/** @short Sent the start event to the product if we have not already done so */
	BOOL m_send_start;
	/** @short Have we already sent the start event ? */
	BOOL m_start_sent;
	/** @short Sent the stop event to the product */
	BOOL m_send_stop;

	/** @short The actual OpLoadingListener defined by product / UI. */
	OpLoadingListener* product_loadinglistener;

	OpTimer progress_timer;
	BOOL progress_timer_running;
	NullLoadingListener null_loading_listener;

	/** @short Called when pending has been enqueued.
	 *
	 * This will make sure that the progress is eventually reported to
	 * product_listener (by calling FlushPendingProgress()).
	 */
	void HandleProgressEnqueued();

	/** @short Flush pending progress.
	 *
	 * Reports enqueued progress to product_listener.
	 */
	void FlushPendingProgress();

	// OpTimerListener
	void OnTimeOut(OpTimer* timer);

	// OpLoadingListener
	void OnUrlChanged(OpWindowCommander* commander, const uni_char* url);
	void OnStartLoading(OpWindowCommander* commander);
	void OnLoadingProgress(OpWindowCommander* commander, const LoadingInformation* info);
	void OnLoadingFinished(OpWindowCommander* commander, LoadingFinishStatus status);
# ifdef URL_MOVED_EVENT
	void OnUrlMoved(OpWindowCommander* commander, const uni_char* url);
# endif // URL_MOVED_EVENT
	void OnAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback);
	void OnStartUploading(OpWindowCommander* commander);
	void OnUploadingFinished(OpWindowCommander* commander, LoadingFinishStatus status);
	BOOL OnLoadingFailed(OpWindowCommander* commander, int msg_id, const uni_char* url);
# ifdef EMBROWSER_SUPPORT
	void OnUndisplay(OpWindowCommander* commander);
	void OnLoadingCreated(OpWindowCommander* commander);
# endif // EMBROWSER_SUPPORT
	BOOL OnAboutToLoadUrl(OpWindowCommander* commander, const char* url, BOOL inline_document);
	void OnXmlParseFailure(OpWindowCommander* commander);
# ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	void OnSearchSuggestionsRequested(OpWindowCommander* commander, const uni_char* url, OpSearchSuggestionsCallback* callback);
# endif
# ifdef XML_AIT_SUPPORT
	virtual OP_STATUS OnAITDocument(OpWindowCommander* commander, AITData* ait_data);
# endif // XML_AIT_SUPPORT
};
#endif // WIC_LIMIT_PROGRESS_FREQUENCY

class NullMailClientListener : public OpMailClientListener
{
public:
	virtual void GetExternalEmbedPermission(OpWindowCommander* commander, BOOL& has_permission) { has_permission = FALSE; }
	virtual void OnExternalEmbendBlocked(OpWindowCommander* commander) {};
};

#ifdef PLUGIN_AUTO_INSTALL
class PluginInstallInformationImpl : public PluginInstallInformation
{
public:
	/**
	 * Set the mime-type of the plugin.
	 *
	 * @param mime_type - the mime-type to be set for this plugin information struct.
	 * @return - OP_STATUS determining the success of the operation.
	 */
	virtual OP_STATUS SetMimeType(const OpStringC& mime_type);

	/**
	 * Set the plugin content URL for this plugin information struct.
	 * This is meant to hold the data URL for the given EMBED/OBJECT/APPLET that is determined by layout.
	 * Also updates the internal string representation of the URL.
	 *
	 * @param url - the plugin content data URL.
	 */
	void SetURL(const URL& url);

	/**
	 * Get the plugin content URL stored in this plugin information struct.
	 *
	 * @return - The URL for the plugin content.
	 */
	virtual URL GetURL() const;

	/**
	 * Implementation of PluginInstallInformation, see OpWindowCommander.h
	 */
	virtual OP_STATUS GetMimeType(OpString& mime_type) const;
	virtual OP_STATUS GetURLString(OpString& url_string, BOOL for_ui = TRUE) const;
	virtual BOOL IsURLValidForDialog() const;

protected:
	/**
	 * The mime-type of the plugin that this struct represents.
	 */
	OpString m_mime_type;

	/**
	 * The core URL object for the plugin content URL of the plugin represented by this struct.
	 */
	URL m_plugin_url;
};
#endif // PLUGIN_AUTO_INSTALL

#ifdef DOM_LOAD_TV_APP
class NullTVAppMessageListener : public OpTVAppMessageListener
{
public:
	virtual ~NullTVAppMessageListener() {}
	virtual OP_STATUS OnLoadTVApp(OpWindowCommander* wic, const uni_char *, const uni_char *, const uni_char *, OpVector<OpStringC>*) {return OpStatus::ERR;}
	virtual OP_STATUS OnOpenForbiddenUrl(OpWindowCommander* wic, const uni_char* forbidden_url) {return OpStatus::OK;}
};
#endif //DOM_LOAD_TV_APP

/** @short Core implementation of OpWindowCommander.
 *
 * This class is only to be used on the core side, NOT on the product / UI
 * side.
 *
 * This class implements all methods in the OpWindowCommander API. In addition
 * it provides getters of pointers to the various listeners associated with
 * OpWindowCommander. These pointers will never be NULL; there are
 * Null*Listenener objects used when the product / UI has not set a
 * listener. This class also provides a few convenience methods for core to
 * use, such as reporting changes in security state.
 */
class WindowCommander : public OpWindowCommander
{
public:
	WindowCommander();
	~WindowCommander();

	OP_STATUS Init();

	// === Browsing stuff ===

	BOOL OpenURL(const uni_char* url,
				 const OpWindowCommander::OpenURLOptions & options);
#ifdef DOM_LOAD_TV_APP
	BOOL LoadTvApp(const uni_char* url,
				   const OpenURLOptions & options,
				   OpAutoVector<OpString>* whitelist);
	void CleanupTvApp();
#endif
	BOOL FollowURL(const uni_char* url,
				   const uni_char* referrer_url,
				   const OpWindowCommander::OpenURLOptions & options);
	BOOL OpenURL(const uni_char* url, BOOL entered_by_user,
				 URL_CONTEXT_ID context_id = (URL_CONTEXT_ID)-1,
				 BOOL use_prev_entry = FALSE);
	BOOL FollowURL(const uni_char* url, const uni_char* referrer_url,
				   BOOL entered_by_user,
				   URL_CONTEXT_ID context_id = (URL_CONTEXT_ID)-1); // -1 == "No Context id."
	void Reload(BOOL force_inline_reload = FALSE);
	BOOL Next();
	BOOL Previous();
	void GetHistoryRange(int* min, int* max);
	int GetCurrentHistoryPos();
	int SetCurrentHistoryPos(int pos);
	OP_STATUS SetHistoryUserData(int history_ID, OpHistoryUserData* user_data);
	OP_STATUS GetHistoryUserData(int history_ID, OpHistoryUserData** user_data) const;
	void Stop();
	BOOL Navigate(UINT16 direction);
	OP_STATUS GetDocumentTypeString(const uni_char** doctype);
	void SetCanBeClosedByScript(BOOL can_close);
#ifdef SAVE_SUPPORT
# ifdef SELFTEST
	OP_STATUS SaveToPNG(const uni_char* file_name, const OpRect& rect );
# endif // SELFTEST
	OP_STATUS GetSuggestedFileName(BOOL focused_frame, OpString* suggested_filename);
	DocumentType GetDocumentType(BOOL focused_frame);
	OP_STATUS SaveDocument(const uni_char* file_name, SaveAsMode mode, BOOL focused_frame, unsigned int max_size = 0, unsigned int* page_size = NULL, unsigned int* saved_size = NULL);
# ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
	OP_STATUS GetOriginalURLForMHTML(OpString& url);
# endif // MHTML_ARCHIVE_REDIRECT_SUPPORT
#endif // SAVE_SUPPORT
	void Authenticate(const uni_char* user_name, const uni_char* password, OpWindowCommander* wic, URL_ID authid);
	void CancelAuthentication(OpWindowCommander* wic, URL_ID authid);
	OP_STATUS InitiateTransfer(OpWindowCommander* wic, uni_char* url, OpTransferItem::TransferAction action, const uni_char* filename);
	void CancelTransfer(OpWindowCommander* wic, const uni_char* url);

#ifdef SUPPORT_GENERATE_THUMBNAILS
	class ThumbnailGenerator* CreateThumbnailGenerator();
#endif // SUPPORT_GENERATE_THUMBNAILS


#ifdef SCOPE_ECMASCRIPT_DEBUGGER
	OP_STATUS DragonflyInspect(OpDocumentContext& context);
#endif //SCOPE_ECMASCRIPT_DEBUGGER
#ifdef INTEGRATED_DEVTOOLS_SUPPORT
	BOOL OpenDevtools(const uni_char *url = NULL);
#endif // #ifdef INTEGRATED_DEVTOOLS_SUPPORT

	/** @short Return the rect describing the position and size element captured by the context.
	 *
	 * @param context The context describing the element
	 * @param[out] rect The rectangle containing the element in screen coordinates
	 * @return
	 *      OpStatus::OK if the rect was successfully fetched and is available in #rect
	 *      OpStatus::ERR if the rect was not accessible. The values in #rect does not
	 *      represent the elements rectangle.
	 */
	virtual OP_STATUS GetElementRect(OpDocumentContext& context, OpRect& rect);

#ifdef SVG_SUPPORT
	OP_STATUS SVGZoomBy(OpDocumentContext& context, int zoomdelta);
	OP_STATUS SVGZoomTo(OpDocumentContext& context, int zoomlevel);
	OP_STATUS SVGResetPan(OpDocumentContext& context);
	OP_STATUS SVGPlayAnimation(OpDocumentContext& context);
	OP_STATUS SVGPauseAnimation(OpDocumentContext& context);
	OP_STATUS SVGStopAnimation(OpDocumentContext& context);
#endif // SVG_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
	OP_STATUS MediaPlay(OpDocumentContext& context, BOOL play);
	OP_STATUS MediaMute(OpDocumentContext& context, BOOL mute);
	OP_STATUS MediaShowControls(OpDocumentContext& context, BOOL show);
#endif // MEDIA_HTML_SUPPORT

	// === notifications we have to get from the ui window ===
	OP_STATUS OnUiWindowCreated(OpWindow* opWindow);
	void OnUiWindowClosing();

	// === State information dpt ===

	BOOL HasNext();
	BOOL HasPrevious();
	BOOL IsLoading();
	OpDocumentListener::SecurityMode GetSecurityMode();

	BOOL GetPrivacyMode();
	void SetPrivacyMode(BOOL privacy_mode);

	DocumentURLType GetURLType();
	UINT32 SecurityLowStatusReason();
	const uni_char * ServerUniName() const;
#ifdef SECURITY_INFORMATION_PARSER
	OP_STATUS CreateSecurityInformationParser(OpSecurityInformationParser ** parser);
	void GetSecurityInformation(OpString &button_text, BOOL isEV);
#endif // SECURITY_INFORMATION_PARSER
#ifdef TRUST_INFO_RESOLVER
	OP_STATUS CreateTrustInformationResolver(OpTrustInformationResolver ** resolver, OpTrustInfoResolverListener * listener);
#endif // TRUST_INFO_RESOLVER
	WIC_URLType GetWicURLType();
#ifdef TRUST_RATING
	BOOL3 HttpResponseIs200();
	OpDocumentListener::TrustMode GetTrustMode();
	OP_STATUS CheckDocumentTrust(BOOL force, BOOL offline);
#endif // TRUST_RATING
#ifdef WEB_TURBO_MODE
	BOOL GetTurboMode();
	void SetTurboMode(BOOL turbo_mode);
#endif // WEB_TURBO_MODE
	URL_CONTEXT_ID GetURLContextID();

	const uni_char* GetCurrentURL(BOOL showPassword);
	const uni_char* GetLoadingURL(BOOL showPassword);
	const uni_char* GetCurrentTitle();
	OP_STATUS GetCurrentMIMEType(OpString8& mime_type, BOOL original = FALSE);
	OP_STATUS GetServerMIMEType(OpString8& mime_type);
	unsigned int GetCurrentHttpResponseCode();
	BOOL GetHistoryInformation(int index, HistoryInformation* result);

#ifdef HISTORY_SUPPORT
	void DisableGlobalHistory();
#endif //HISTORY_SUPPORT

	OpWindow* GetOpWindow() { return m_opWindow; }
	virtual unsigned long GetWindowId();

#ifdef HAS_ATVEF_SUPPORT
	OP_STATUS ExecuteES(const uni_char* script);
#endif // HAS_ATVEF_SUPPORT

	void SetScale(UINT32 scale);
	UINT32 GetScale();

	void SetTrueZoom(BOOL enable);
	BOOL GetTrueZoom();

	void SetTextScale(UINT32 scale);
	UINT32 GetTextScale();

	void SetImageMode(OpDocumentListener::ImageDisplayMode mode);
	OpDocumentListener::ImageDisplayMode GetImageMode();
	void SetCssMode(OpDocumentListener::CssDisplayMode mode);
	OpDocumentListener::CssDisplayMode GetCssMode();

	void ForceEncoding(Encoding encoding);
	Encoding GetEncoding();
#ifdef FONTSWITCHING
	WritingSystem::Script GetPreferredScript(BOOL focused_frame);
#endif

	OP_STATUS GetBackgroundColor(OpColor& background_color);
	void SetDefaultBackgroundColor(const OpColor& background_color);

	void SetShowScrollbars(BOOL show);

#ifndef HAS_NO_SEARCHTEXT
	SearchResult Search(const uni_char* search_string, const SearchInfo& search_info);
	void ResetSearchPosition();
	BOOL GetSearchMatchRectangles(OpVector<OpRect> &all_rects, OpVector<OpRect> &active_rects);
#endif // !HAS_NO_SEARCHTEXT
#ifndef HAS_NOTEXTSELECTION
# ifdef USE_OP_CLIPBOARD
	void Paste();
	void Cut();
	void Copy();
# endif // USE_OP_CLIPBOARD
	void SelectTextStart(const OpPoint& p);
	void SelectTextMoveAnchor(const OpPoint& p);
	void SelectTextEnd(const OpPoint& p);
	void SelectWord(const OpPoint& p);
# ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
	OP_STATUS SelectNearbyLink(const OpPoint& p);
# endif // NEARBY_INTERACTIVE_ITEM_DETECTION
	OP_STATUS GetSelectTextStart(OpPoint& p, int &line_height);
	OP_STATUS GetSelectTextEnd(OpPoint& p, int &line_height);

#ifdef _PRINT_SUPPORT_
	OP_STATUS Print(int from, int to, BOOL selected_only);
	void CancelPrint();
	void PrintNextPage();
	BOOL IsPrintable();
	BOOL IsPrinting();
#endif

	BOOL HasSelectedText();
	uni_char *GetSelectedText();
	void ClearSelection();
# ifdef KEYBOARD_SELECTION_SUPPORT
	OP_STATUS SetKeyboardSelectable(BOOL enable);
	void MoveCaret(CaretMovementDirection direction, BOOL in_selection_mode);
	OP_STATUS GetCaretRect(OpRect& rect);
	OP_STATUS SetCaretPosition(const OpPoint& postion);
# endif // KEYBOARD_SELECTION_SUPPORT
#endif // !HAS_NOTEXTSELECTION
#ifdef USE_OP_CLIPBOARD
	BOOL CopyImageToClipboard(const uni_char* url);
	void CopyLinkToClipboard(OpDocumentContext& ctx);
#endif // USE_OP_CLIPBOARD
#ifdef SHORTCUT_ICON_SUPPORT
	BOOL HasDocumentIcon();
	OP_STATUS GetDocumentIcon(OpBitmap** icon);
	OP_STATUS GetDocumentIconURL(const uni_char** iconUrl);
#endif // SHORTCUT_ICON_SUPPORT
#ifdef LINK_SUPPORT
	UINT32 GetLinkElementCount();
	BOOL GetLinkElementInfo(UINT32 index, LinkElementInfo* information);
#endif // LINK_SUPPORT
	BOOL GetAlternateCssInformation(UINT32 index, CssInformation* information);
	BOOL SetAlternateCss(const uni_char* title);
	BOOL ShowCurrentImage();
#ifdef WINDOW_COMMANDER_TRANSFER
	OP_STATUS InitiateTransfer(DocumentManager* docManager, URL& url, OpTransferItem::TransferAction action, const uni_char* filename);
	void	CancelTransfer(DocumentManager* docManager, URL& url);
#endif // WINDOW_COMMANDER_TRANSFER

	// === Listener dpt ===
	void SetLoadingListener(OpLoadingListener* listener);
	void SetDocumentListener(OpDocumentListener* listener);
	void SetMailClientListener(OpMailClientListener* listener);
#ifdef _POPUP_MENU_SUPPORT_
	void SetMenuListener(OpMenuListener* listener);
#endif // _POPUP_MENU_SUPPORT_
	void SetLinkListener(OpLinkListener* listener);
#ifdef ACCESS_KEYS_SUPPORT
	void SetAccessKeyListener(OpAccessKeyListener* listener);
#endif // ACCESS_KEYS_SUPPORT
#ifdef _PRINT_SUPPORT_
	void SetPrintingListener(OpPrintingListener* listener);
	void StartPrinting();
#endif // _PRINT_SUPPORT_
	void SetErrorListener(OpErrorListener* listener);
#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
	void SetSSLListener(OpSSLListener* listener);
#endif // _SSL_SUPPORT_ && _NATIVE_SSL_SUPPORT_ || WIC_USE_SSL_LISTENER
#ifdef _ASK_COOKIE
	void SetCookieListener(OpCookieListener* listener);
#endif // _ASK_COOKIE
#ifdef WIC_FILESELECTION_LISTENER
	void SetFileSelectionListener(OpFileSelectionListener* listener);
#endif // WIC_FILESELECTION_LISTENER
#ifdef WIC_COLORSELECTION_LISTENER
	void SetColorSelectionListener(OpColorSelectionListener* listener);
#endif // WIC_COLORSELECTION_LISTENER
#ifdef NEARBY_ELEMENT_DETECTION
	void SetFingertouchListener(OpFingertouchListener* listener);
#endif // NEARBY_ELEMENT_DETECTION
#ifdef WIC_PERMISSION_LISTENER
	void SetPermissionListener(OpPermissionListener* listener);
#endif // WIC_PERMISSION_LISTENER
#ifdef SCOPE_URL_PLAYER
	void SetUrlPlayerListener(WindowCommanderUrlPlayerListener* listener);
#endif //SCOPE_URL_PLAYER
#ifdef WIC_EXTERNAL_APPLICATIONS_ENUMERATION
	void SetApplicationListener(OpApplicationListener *listener);
#endif // WIC_EXTERNAL_APPLICATIONS_ENUMERATION
#ifdef WIC_OS_INTERACTION
	void SetOSInteractionListener(OpOSInteractionListener *listener);
#endif // WIC_OS_INTERACTION

#ifdef APPLICATION_CACHE_SUPPORT
	virtual void SetApplicationCacheListener(OpApplicationCacheListener* listener);
#endif // APPLICATION_CACHE_SUPPORT

#ifdef EXTENSION_SUPPORT
	virtual void SetExtensionUIListener(OpExtensionUIListener* listener);
#endif // EXTENSION_SUPPORT

#ifdef GOGI_SELFTEST_FINISHED_NOTIFICATION
	void SetSelftestListener(OpSelftestListener *listener);
#endif // GOGI_SELFTEST_FINISHED_NOTIFICATION

#ifdef ACCESS_KEYS_SUPPORT
	void SetAccessKeyMode(BOOL enable);
	BOOL GetAccessKeyMode();
#endif // ACCESS_KEYS_SUPPORT

#ifdef DOM_TO_PLATFORM_MESSAGES
	void SetPlatformMessageListener(OpPlatformMessageListener *listener);
#endif // DOM_TO_PLATFORM_MESSAGES

#ifdef CONTENT_MAGIC
	void EnableContentMagicIndicator(BOOL enable, INT32 x, INT32 y);
#endif // CONTENT_MAGIC

	OP_STATUS GetSelectedLinkInfo(OpString& url, OpString& title);
	void SetVisibleOnScreen(BOOL is_visible);
	void SetDrawHighlightRects(BOOL enable);

	BOOL GetScriptingDisabled();
	void SetScriptingDisabled(BOOL disabled);
	BOOL GetScriptingPaused();
	void SetScriptingPaused(BOOL paused);
	void CancelAllScriptingTimeouts();

	// === Getters for listeners. These are not in the OpWindowCommander interface ===
	/** @short Returns the OpLoadingListener to use for call-out from core.
	 *
	 * Also @see SelftestHelper
	 */
	OpLoadingListener*   GetLoadingListener() { return m_loadingListener; }
	OpDocumentListener*  GetDocumentListener() { return m_documentListener; }
	OpMailClientListener* GetMailClientListener() { return m_mailClientListener; }
#ifdef _POPUP_MENU_SUPPORT_
	OpMenuListener*      GetMenuListener() { return m_menuListener; }
#endif // _POPUP_MENU_SUPPORT_
	OpLinkListener*      GetLinkListener() { return m_linkListener; }
#ifdef ACCESS_KEYS_SUPPORT
	OpAccessKeyListener* GetAccessKeyListener() { return m_accessKeyListener; }
#endif // ACCESS_KEYS_SUPPORT
#ifdef _PRINT_SUPPORT_
	OpPrintingListener*  GetPrintingListener() { return m_printingListener; }
#endif // _PRINT_SUPPORT_
	OpErrorListener*     GetErrorListener() { return m_errorListener; }
#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
	OpSSLListener*		 GetSSLListener() { return m_sslListener; }
#endif // _SSL_SUPPORT_ && _NATIVE_SSL_SUPPORT_ || WIC_USE_SSL_LISTENER
#ifdef _ASK_COOKIE
	OpCookieListener*    GetCookieListener() { return m_cookieListener; }
#endif // _ASK_COOKIE
#ifdef WIC_FILESELECTION_LISTENER
	OpFileSelectionListener* GetFileSelectionListener() { return m_fileSelectionListener; }
#endif // WIC_FILESELECTION_LISTENER
#ifdef WIC_COLORSELECTION_LISTENER
	OpColorSelectionListener* GetColorSelectionListener() { return m_colorSelectionListener; }
#endif // WIC_COLORSELECTION_LISTENER
#ifdef NEARBY_ELEMENT_DETECTION
	OpFingertouchListener* GetFingertouchListener() { return m_fingertouchListener; }
#endif // NEARBY_ELEMENT_DETECTION
#ifdef WIC_PERMISSION_LISTENER
	OpPermissionListener* GetPermissionListener() { return m_permissionListener; }
#endif // WIC_PERMISSION_LISTENER
#ifdef SCOPE_URL_PLAYER
	WindowCommanderUrlPlayerListener* GetUrlPlayerListener() { return m_urlPlayerListener; }
#endif //SCOPE_URL_PLAYER

#ifdef WIC_EXTERNAL_APPLICATIONS_ENUMERATION
	OpApplicationListener* GetApplicationListener() { return m_applicationListener; }
#endif // WIC_EXTERNAL_APPLICATIONS_ENUMERATION

#ifdef DOM_TO_PLATFORM_MESSAGES
	OpPlatformMessageListener* GetPlatformMessageListener() { return m_platformMessageListener; }
#endif // DOM_TO_PLATFORM_MESSAGES

#ifdef APPLICATION_CACHE_SUPPORT
	OpApplicationCacheListener* GetApplicationCacheListener() { return m_applicationCachelistener; }
#endif // APPLICATION_CACHE_SUPPORT

#ifdef WIC_OS_INTERACTION
	OpOSInteractionListener* GetOSInteractionListener() { return m_osInteractionListener; }
#endif // WIC_OS_INTERACTION

#ifdef EXTENSION_SUPPORT
	OpExtensionUIListener*      GetExtensionUIListener() { return m_extensionUIListener; }
#endif // EXTENSION_SUPPORT

#ifdef GOGI_SELFTEST_FINISHED_NOTIFICATION
	OpSelftestListener* GetSelftestListener() { return m_selftestListener; }
#endif // GOGI_SELFTEST_FINISHED_NOTIFICATION

#ifdef DOM_LOAD_TV_APP
	void SetTVAppMessageListener (OpTVAppMessageListener*);
	OpTVAppMessageListener* GetTVAppMessageListener() { return m_tvAppMessageListener; }
#endif //DOM_LOAD_TV_APP

#ifdef _POPUP_MENU_SUPPORT_
	virtual void RequestPopupMenu(const OpPoint* point);
	virtual void OnMenuItemClicked(OpDocumentContext* context, UINT32 id);
	virtual OP_STATUS GetMenuItemIcon(UINT32 id, OpBitmap*& icon_bitmap);
#endif // _POPUP_MENU_SUPPORT_

	virtual OP_STATUS CreateDocumentContext(OpDocumentContext** ctx);
	virtual OP_STATUS CreateDocumentContextForScreenPos(OpDocumentContext** ctx, const OpPoint& screen_pos);
	virtual void LoadImage(OpDocumentContext& ctx, BOOL disable_turbo=FALSE);
#ifdef USE_OP_CLIPBOARD
    virtual BOOL CopyImageToClipboard(OpDocumentContext& ctx);
    virtual BOOL CopyBGImageToClipboard(OpDocumentContext& ctx);
#endif // USE_OP_CLIPBOARD
	virtual OP_STATUS SaveImage(OpDocumentContext& ctx, const uni_char* filename);
	virtual void OpenImage(OpDocumentContext& ctx, BOOL new_tab=FALSE, BOOL in_background=FALSE);

#ifdef WIC_TEXTFORM_CLIPBOARD_CONTEXT
	virtual void CopyTextToClipboard(OpDocumentContext& ctx);
	virtual void CutTextToClipboard(OpDocumentContext& ctx);
	virtual void PasteTextFromClipboard(OpDocumentContext& ctx);
#endif // WIC_TEXTFORM_CLIPBOARD_CONTEXT
	virtual void ClearText(OpDocumentContext& ctx);

	LayoutMode GetLayoutMode();
	void SetLayoutMode(OpWindowCommander::LayoutMode);
	void SetSmartFramesMode(BOOL enable);
	BOOL GetSmartFramesMode();

	BOOL HasCurrentElement();
	void SetCurrentElement(HTML_Element* elm) { currentElement = elm; }

	void Focus();

	/** THIS IS BAD, but I really need the Window* pointer in a transition phase (Trond) */
	Window* ViolateCoreEncapsulationAndGetWindow() { return m_window; }

	OP_STATUS SetWindowTitle(const OpString& str, BOOL force, BOOL generated = FALSE);

	/** Helper function translates between OpDocumentListener::SecurityMode and core's BYTE securityState */
	OpDocumentListener::SecurityMode GetSecurityModeFromState(BYTE securityState);

	OpViewportController* GetViewportController() const;

#ifdef NEARBY_ELEMENT_DETECTION
	void HideFingertouchElements();
#endif // NEARBY_ELEMENT_DETECTION

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
	OP_STATUS FindNearbyInteractiveItems(const OpRect& rect, List<InteractiveItemInfo>& list, BOOL bbox_rectangles = FALSE);
#endif // NEARBY_INTERACTIVE_ITEM_DETECTION

#ifdef DOM_JSPLUGIN_CUSTOM_EVENT_HOOK
	OP_STATUS SendCustomWindowEvent(const uni_char* event_name, int event_param);
# ifdef GADGET_SUPPORT
	OP_STATUS SendCustomGadgetEvent(const uni_char* event_name, int event_param);
# endif // GADGET_SUPPORT
#endif // DOM_JSPLUGIN_CUSTOM_EVENT_HOOK

	/** tell window that screen has changed, could be that window is moved
	 * from a screen to another, OR that the properties of the current
	 * screen has changed
	 */
	void ScreenPropertiesHaveChanged();

#ifdef GADGET_SUPPORT
	void SetGadget(OpGadget* gadget) { m_gadget = gadget; }
	OpGadget* GetGadget() const { return m_gadget; }
#endif // GADGET_SUPPORT

	OP_STATUS GetCurrentContentSize(OpFileLength& size, BOOL include_inlines);

#ifdef WEBFEEDS_DISPLAY_SUPPORT
	BOOL IsInlineFeed();
#endif // WEBFEEDS_DISPLAY_SUPPORT

	BOOL IsIntranet();
	BOOL HasDocumentFocusedFormElement();

	OP_STATUS CreateSearchURL(OpDocumentContext& context, OpString8* url, OpString8* query, OpString8* charset, BOOL* is_post);

#ifdef PLUGIN_AUTO_INSTALL
	OP_STATUS OnPluginAvailable(const uni_char* mime_type);
#endif // PLUGIN_AUTO_INSTALL

#ifdef _PLUGIN_SUPPORT_
	/**
	 * Called when a new plug-in has been detected.
	 *
	 * Asynchronous plug-in detection can notify documents that are missing
	 * plug-ins about newly detected ones. This is to let documents refresh
	 * their embed content without having to reload the whole page.
	 *
	 * @see PluginProber.
	 *
	 * @param mime_type  The mime-type that became available.
	 * @return See OpStatus.
	 */
	OP_STATUS OnPluginDetected(const uni_char* mime_type);
#endif // _PLUGIN_SUPPORT_

#ifdef SELFTEST
	/**
	 * A helper class for selftests.
	 *
	 * Allows selftests to access internal structures of the
	 * WindowCommander class for testing purposes only.
	 */
    class SelftestHelper {
    WindowCommander* m_wic;
    public:
        SelftestHelper(WindowCommander* wic) : m_wic(wic) {}
        /**
		 * Returns the loading listener set by the platform.
		 *
		 * Some selftests insert their own loading listener between core
		 * and the platform, by first getting the platform's listener,
		 * with GetLoadingListener(), to use as the fallback, and then
		 * setting their own with SetLoadingListener(). The old listener
		 * is called from the selftest's listener.
		 *
		 * If WIC_LIMIT_PROGRESS_FREQUENCY is used, this will not work.
		 * WindowCommander::GetLoadingListener() will not necessarily
		 * return what was set by the platform calling
		 * SetLoadingListener(); it might return a proxy loading listener
		 * used by core. Furthermore, calling SetLoadingListener() will
		 * not replace that proxy listener. Use this helper method to get
		 * the platform's listener for the fallback.
		 */
        OpLoadingListener* GetPlatformLoadingListener() {
# ifdef WIC_LIMIT_PROGRESS_FREQUENCY
            return ((LoadingListenerProxy*)(m_wic->GetLoadingListener()))->GetLoadingListener();
# else
            return m_wic->GetLoadingListener();
# endif // WIC_LIMIT_PROGRESS_FREQUENCY
        }
		/**
		 * Override any loading listener proxy used.
		 *
		 * When WIC_LIMIT_PROGRESS_FREQUENCY is defined,
		 * SetLoadingListener() cannot override the
		 * LoadingListenerProxy which will always be called
		 * anyway. Some selftests (like dochand.lockdisplay) rely on
		 * correct timing for loading listener events, and will have
		 * problems with the LoadingListenerProxy. Use this helper
		 * method to make sure that your loading listener will be
		 * called instead of the LoadingListenerProxy.
		 *
		 * @param loading_listener the loading listener to use, or
		 *        NULL if the original LoadingListenerProxy should be
		 *        used
		 */
		void OverrideLoadingListenerProxy(OpLoadingListener* loading_listener)
		{
#ifdef WIC_LIMIT_PROGRESS_FREQUENCY
			if (loading_listener)
				m_wic->m_loadingListener = loading_listener;
			else
				m_wic->m_loadingListener = &(m_wic->m_loadingListenerProxy);
#endif // WIC_LIMIT_PROGRESS_FREQUENCY
		}
    };
#endif // SELFTEST

	BOOL IsDocumentSupportingViewMode(WindowViewMode view_mode) const;

	static DocumentURLType URLType2DocumentURLType(URLType type);

	void ForcePluginsDisabled(BOOL disabled);

	BOOL GetForcePluginsDisabled() const;

#ifdef WIC_SET_PERMISSION
	virtual OP_STATUS SetUserConsent(OpPermissionListener::PermissionCallback::PermissionType permission_type, OpPermissionListener::PermissionCallback::PersistenceType persistence_type, const uni_char *hostname, BOOL3 allowed);
	virtual void GetUserConsent(INTPTR user_data);
#endif // WIC_SET_PERMISSION

#ifdef GOGI_APPEND_HISTORY_LIST
	virtual OP_STATUS AppendHistoryList(const OpVector<HistoryInformation>& urls, unsigned int activate_url_index);
#endif // GOGI_APPEND_HISTORY_LIST

	virtual FullScreenState GetDocumentFullscreenState() const;
	virtual OP_STATUS SetDocumentFullscreenState(FullScreenState value);

private:

	/** helper method */
	const CSS* GetCSS(UINT32 index) const;

	// data:

	/** the window that we operate on */
	Window* m_window;
	OpWindow* m_opWindow;

#ifdef GADGET_SUPPORT
	OpGadget* m_gadget;
#endif // GADGET_SUPPORT

	/** The listeners */
	OpLoadingListener*    m_loadingListener;
	OpDocumentListener*   m_documentListener;
	OpMailClientListener* m_mailClientListener;
#ifdef _POPUP_MENU_SUPPORT_
	OpMenuListener*       m_menuListener;
#endif // _POPUP_MENU_SUPPORT_
	OpLinkListener*       m_linkListener;
#ifdef ACCESS_KEYS_SUPPORT
	OpAccessKeyListener*  m_accessKeyListener;
#endif // ACCESS_KEYS_SUPPORT
#ifdef _PRINT_SUPPORT_
	OpPrintingListener*   m_printingListener;
#endif // _PRINT_SUPPORT_
	OpErrorListener*      m_errorListener;
#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
	OpSSLListener*		  m_sslListener;
#endif // _SSL_SUPPORT_ && _NATIVE_SSL_SUPPORT_ || WIC_USE_SSL_LISTENER
#ifdef _ASK_COOKIE
	OpCookieListener*	  m_cookieListener;
#endif // _ASK_COOKIE
#ifdef WIC_FILESELECTION_LISTENER
	OpFileSelectionListener* m_fileSelectionListener;
#endif // WIC_FILESELECTION_LISTENER
#ifdef WIC_COLORSELECTION_LISTENER
	OpColorSelectionListener* m_colorSelectionListener;
#endif // WIC_COLORSELECTION_LISTENER
#ifdef DOM_TO_PLATFORM_MESSAGES
	OpPlatformMessageListener* m_platformMessageListener;
#endif // DOM_TO_PLATFORM_MESSAGES
#ifdef NEARBY_ELEMENT_DETECTION
	OpFingertouchListener* m_fingertouchListener;
#endif // NEARBY_ELEMENT_DETECTION
#ifdef WIC_PERMISSION_LISTENER
	OpPermissionListener* m_permissionListener;
#endif // WIC_PERMISSION_LISTENER
#ifdef SCOPE_URL_PLAYER
	/** Should be removed when urlplayer is removed!
	 *  This listener is a code-around so that closing a
	 *  window dont crash opera while url-playing.
	 *  Since urlPlayer going to die so should this.
	 *  Since URLPlayer will be removed, WindowCommanderUrlPlayerListener
	 *  is made as a separate listener so that it's realy
	 *  simple to throw it out
	 */
	WindowCommanderUrlPlayerListener* m_urlPlayerListener;
#endif //SCOPE_URL_PLAYER

#ifdef WIC_EXTERNAL_APPLICATIONS_ENUMERATION
	OpApplicationListener* m_applicationListener;
#endif // WIC_EXTERNAL_APPLICATIONS_ENUMERATION

#ifdef APPLICATION_CACHE_SUPPORT
	OpApplicationCacheListener* m_applicationCachelistener;
#endif // APPLICATION_CACHE_SUPPORT

#ifdef WIC_OS_INTERACTION
	OpOSInteractionListener* m_osInteractionListener;
#endif // WIC_OS_INTERACTION

#ifdef EXTENSION_SUPPORT
	OpExtensionUIListener* m_extensionUIListener;
#endif // EXTENSION_SUPPORT

#ifdef WIC_LIMIT_PROGRESS_FREQUENCY
	LoadingListenerProxy  m_loadingListenerProxy;
#else
	NullLoadingListener   m_nullLoadingListener;
#endif // WIC_LIMIT_PROGRESS_FREQUENCY

#ifdef GOGI_SELFTEST_FINISHED_NOTIFICATION
	OpSelftestListener* m_selftestListener;
#endif // GOGI_SELFTEST_FINISHED_NOTIFICATION

	NullDocumentListener  m_nullDocumentListener;
	NullMailClientListener m_nullMailClientListener;
#ifdef _POPUP_MENU_SUPPORT_
	NullMenuListener      m_nullMenuListener;
#endif // _POPUP_MENU_SUPPORT_
	NullLinkListener      m_nullLinkListener;
#ifdef ACCESS_KEYS_SUPPORT
	NullAccessKeyListener m_nullAccessKeyListener;
#endif // ACCESS_KEYS_SUPPORT
#ifdef _PRINT_SUPPORT_
	NullPrintingListener  m_nullPrintingListener;
#endif // _PRINT_SUPPORT_
	NullErrorListener     m_nullErrorListener;
#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
	NullSSLListener	  m_nullSSLListener;
#endif // _SSL_SUPPORT_ && _NATIVE_SSL_SUPPORT_ || WIC_USE_SSL_LISTENER
#ifdef _ASK_COOKIE
	NullCookieListener m_nullCookieListener;
#endif // _ASK_COOKIE
#ifdef WIC_FILESELECTION_LISTENER
	NullFileSelectionListener m_nullFileSelectionListener;
#endif // WIC_FILESELECTION_LISTENER
#ifdef WIC_COLORSELECTION_LISTENER
	NullColorSelectionListener m_nullColorSelectionListener;
#endif // WIC_COLORSELECTION_LISTENER
#ifdef NEARBY_ELEMENT_DETECTION
	NullFingertouchListener m_nullFingertouchListener;
#endif // NEARBY_ELEMENT_DETECTION
#ifdef WIC_PERMISSION_LISTENER
	NullPermissionListener m_nullPermissionListener;
#endif // WIC_FILESELECTION_LISTENER

#ifdef WIC_EXTERNAL_APPLICATIONS_ENUMERATION
	NullApplicationListener m_nullApplicationListener;
#endif // WIC_EXTERNAL_APPLICATIONS_ENUMERATION
#ifdef DOM_TO_PLATFORM_MESSAGES
	NullPlatformMessageListener m_nullPlatformMessageListener;
#endif // DOM_TO_PLATFORM_MESSAGES
#ifdef WIC_OS_INTERACTION
	NullOSInteractionListener m_nullOSInteractionListener;
#endif // WIC_OS_INTERACTION

#ifdef APPLICATION_CACHE_SUPPORT
	NullApplicationCacheListener m_null_application_cache_listener;
#endif // APPLICATION_CACHE_SUPPORT

#ifdef EXTENSION_SUPPORT
	NullExtensionUIListener m_null_extension_ui_listener;
#endif // EXTENSION_SUPPORT

#ifdef GOGI_SELFTEST_FINISHED_NOTIFICATION
	NullSelftestListener m_nullSelftestListener;
#endif // GOGI_SELFTEST_FINISHED_NOTIFICATION

#ifdef DOM_LOAD_TV_APP
	OpTVAppMessageListener* m_tvAppMessageListener;
	NullTVAppMessageListener m_nullTvAppMessageListener;
#endif //DOM_LOAD_TV_APP

	HTML_Element* currentElement;

#ifndef HAS_NO_SEARCHTEXT
	OpString m_searchtext;
#endif // !HAS_NO_SEARCHTEXT

	FramesDocument* selection_start_document;

	// private methods:

	void NullAllListeners();
};

#ifdef _DEBUG
/**
 * This operator can be used in an OP_DBG() statement to print the
 * OpLoadingListener::LoadingFinishStatus in a human readable form:
 * @code
 * enum OpLoadingListener::LoadingFinishStatus status = ...;
 * OP_DBG(("") << "loading finish status: " << status);
 * @endcode
 */
inline Debug& operator<<(Debug& d, enum OpLoadingListener::LoadingFinishStatus s)
{
	switch (s) {
# define DEBUG_CASE_PRINT(e) case OpLoadingListener::e: return d << "OpLoadingListener::" # e
		DEBUG_CASE_PRINT(LOADING_SUCCESS);
		DEBUG_CASE_PRINT(LOADING_REDIRECT);
		DEBUG_CASE_PRINT(LOADING_COULDNT_CONNECT);
		DEBUG_CASE_PRINT(LOADING_UNKNOWN);
	default: return d << "OpLoadingListener::LoadingFinishStatus(unknown:" << static_cast<int>(s) << ")";
	}
# undef DEBUG_CASE_PRINT
}
#endif // _DEBUG

#endif // WINDOW_COMMANDER_H
