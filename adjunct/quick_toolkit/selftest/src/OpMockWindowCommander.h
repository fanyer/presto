/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef MOCK_OP_WINDOW_COMMANDER_H
#define MOCK_OP_WINDOW_COMMANDER_H

#include "modules/windowcommander/OpWindowCommander.h"

class OpMockWindowCommander : public OpWindowCommander
{
public:

	virtual OP_STATUS Init() { return OpStatus::OK; }

	BOOL OpenURL(const uni_char* url,
	             const OpWindowCommander::OpenURLOptions & options)
	{
		m_url_context_id = options.context_id;
		return TRUE;
	};
	BOOL FollowURL(const uni_char* url,
	               const uni_char* referrer_url,
	               const OpWindowCommander::OpenURLOptions & options)
	{
         m_url_context_id = options.context_id;
         return TRUE;
	}
	virtual BOOL OpenURL(const uni_char* url, BOOL entered_by_user
						 , URL_CONTEXT_ID context_id = (URL_CONTEXT_ID)-1
						 , BOOL use_prev_entry = FALSE)
	{
		m_url_context_id = context_id;
		return TRUE;
	}

#ifdef EXTENSION_SUPPORT
	virtual void SetExtensionUIListener(OpExtensionUIListener*) { }
#endif

	virtual BOOL FollowURL(const uni_char* url, const uni_char* referrer_url, BOOL entered_by_user
                                                    , URL_CONTEXT_ID context_id = (URL_CONTEXT_ID)-1
                                                   ) 
    {
         m_url_context_id = context_id;
         return TRUE;            
    }

    virtual OP_STATUS GetCurrentContentSize(OpFileLength& size, BOOL include_inlines) { return OpStatus::OK; }
    virtual BOOL IsInlineFeed() { return FALSE; }

#ifdef PLUGIN_AUTO_INSTALL
	virtual OP_STATUS OnPluginAvailable(const uni_char* mime_type) { return OpStatus::OK; }
#endif // PLUGIN_AUTO_INSTALL

 	virtual void Reload(BOOL force_inline_reload = FALSE) { }
	virtual BOOL Next() { return TRUE; }
	virtual BOOL Previous() { return TRUE; }
	virtual void Stop() { }
	virtual BOOL Navigate(UINT16 direction_degrees) { return TRUE; }
	virtual OP_STATUS GetDocumentTypeString(const uni_char** doctype) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual void SetCanBeClosedByScript(BOOL can_close) {}

#ifdef SAVE_SUPPORT
	virtual OP_STATUS GetSuggestedFileName(BOOL focused_frame, OpString* suggested_filename) { return OpStatus::OK; }
	virtual DocumentType GetDocumentType(BOOL focused_frame) { return OpWindowCommander::DOC_HTML; }
	virtual OP_STATUS SaveDocument(const uni_char* file_name, SaveAsMode mode, BOOL focused_frame, unsigned int max_size=0) { return OpStatus::OK; }
		virtual OP_STATUS SaveDocument(const uni_char* file_name, SaveAsMode mode, BOOL focused_frame, unsigned int max_size=0, unsigned int* page_size = NULL, unsigned int* saved_size = NULL) { return OpStatus::OK; }
#endif // SAVE_SUPPORT

#ifdef SELFTEST
	virtual OP_STATUS SaveToPNG(const uni_char* file_name, const OpRect& rect ) { return OpStatus::ERR_NOT_SUPPORTED; } 
#endif // SELFTEST
	virtual void Authenticate(const uni_char* user_name, const uni_char* password, OpWindowCommander* wic, URL_ID authid) { }
	virtual void CancelAuthentication(OpWindowCommander* wic, URL_ID authid) { }
	virtual OP_STATUS InitiateTransfer(OpWindowCommander* wic, uni_char* url, OpTransferItem::TransferAction action, const uni_char* filename) { return OpStatus::OK; }
	virtual void CancelTransfer(OpWindowCommander* wic, const uni_char* url) { }
	virtual void CopyLinkToClipboard(OpDocumentContext& ctx)  { }

	virtual class ThumbnailGenerator* CreateThumbnailGenerator() { return NULL; }

#if defined (_SSL_SUPPORT_) && defined (_NATIVE_SSL_SUPPORT_)
	virtual OpSSLListener::SSLCertificateContext* BrowseCertificates() { return NULL; }
	virtual OP_STATUS InstallCertificate(uni_char* certificate_file, BOOL interactive) { return OpStatus::OK; }
#endif // _SSL_SUPPORT_ && _NATIVE_SSL_SUPPORT_

#ifdef SCOPE_ECMASCRIPT_DEBUGGER
	virtual OP_STATUS DragonflyInspect(OpDocumentContext& context) { return OpStatus::OK; }
#endif // SCOPE_ECMASCRIPT_DEBUGGER
#ifdef INTEGRATED_DEVTOOLS_SUPPORT
	virtual BOOL OpenDevtools(const uni_char *url = NULL) { return TRUE; }
#endif //INTEGRATED_DEVTOOLS_SUPPORT
	virtual OP_STATUS GetElementRect(OpDocumentContext& context, OpRect& rect)  { return OpStatus::OK; }
	virtual OP_STATUS OnUiWindowCreated(OpWindow* opWindow) { m_op_window = opWindow; return OpStatus::OK; }
	virtual void OnUiWindowClosing() { m_called_on_ui_window_closing = TRUE; m_op_window = NULL; }
#ifndef DOXYGEN_DOCUMENTATION
	virtual Window* ViolateCoreEncapsulationAndGetWindow() { return NULL; }
#endif // !DOXYGEN_DOCUMENTATION

	virtual OP_STATUS SetWindowTitle(const OpString&, BOOL, BOOL = FALSE) { return OpStatus::OK; }

	virtual LayoutMode GetLayoutMode() { return m_layout_mode; }
	virtual void SetLayoutMode(LayoutMode mode) { m_layout_mode = mode; }
	virtual void SetSmartFramesMode(BOOL enable) { m_smart_frames_mode = enable; }
	virtual BOOL GetSmartFramesMode() { return m_smart_frames_mode; }
	virtual BOOL HasCurrentElement() { m_called_has_current_element = TRUE; return m_has_current_element; }
	virtual BOOL HasNext() { m_called_has_next = TRUE; return FALSE; }
	virtual BOOL HasPrevious() { m_called_has_previous = TRUE; return FALSE; }
	virtual BOOL IsLoading() { return FALSE; }
	virtual OpDocumentListener::SecurityMode GetSecurityMode() { m_called_get_security_mode = TRUE; return OpDocumentListener::NO_SECURITY; }
	virtual BOOL GetPrivacyMode() { return m_privacy_mode; }
	virtual DocumentURLType GetURLType() { return DocumentURLType_HTTP; }
	virtual const uni_char* GetLoadingURL(BOOL showPassword) { return m_current_url.CStr(); }
	virtual void OpenImage(OpDocumentContext& ctx, BOOL new_tab=FALSE, BOOL in_background=FALSE) {}
	virtual void SetPrivacyMode(BOOL privacy_mode) { m_privacy_mode = privacy_mode; }

#ifdef TRUST_RATING
	virtual OpDocumentListener::TrustMode GetTrustMode() { return OpDocumentListener::TRUSTED_DOMAIN; }
	virtual OP_STATUS CheckDocumentTrust(BOOL force, BOOL offline) { return OpStatus::OK; }
#endif // TRUST_RATING

#ifdef WEB_TURBO_MODE
	virtual BOOL GetTurboMode() { return m_turbo_mode; }
	virtual void SetTurboMode(BOOL turbo_mode) { m_turbo_mode = turbo_mode; }
#endif // WEB_TURBO_MODE

	virtual URL_CONTEXT_ID GetURLContextID() { return m_url_context_id; }

	virtual const uni_char* GetCurrentURL(BOOL showPassword) { return m_current_url.CStr(); }
	virtual const uni_char* GetCurrentTitle() { return UNI_L("MockTitle"); }
	virtual OP_STATUS GetCurrentMIMEType(OpString8 &, BOOL original = FALSE){return OpStatus::ERR ;}
	virtual OP_STATUS GetServerMIMEType(OpString8& mime_type) { return OpStatus::ERR; }
	virtual unsigned int GetCurrentHttpResponseCode() { return 0; }
	virtual BOOL GetHistoryInformation(int index, HistoryInformation* result) { return FALSE; }
	virtual void GetHistoryRange(int* min, int* max) { *min = *max = 0; }
	virtual int GetCurrentHistoryPos() { m_called_get_current_history_pos = TRUE; return HasCurrentElement() ? 1 : 0; }
	virtual int SetCurrentHistoryPos(int pos) { return 0; }
	virtual OP_STATUS SetHistoryUserData(int history_ID, OpHistoryUserData* user_data) { return OpStatus::OK; }
	virtual OP_STATUS GetHistoryUserData(int history_ID, OpHistoryUserData** user_data) const {return OpStatus::ERR ;}

#ifdef HISTORY_SUPPORT
	virtual void DisableGlobalHistory() { }
#endif // HISTORY_SUPPORT

	virtual OpWindow* GetOpWindow() { return m_op_window; }

	virtual unsigned long GetWindowId() { return 0; }

#ifdef HAS_ATVEF_SUPPORT
	virtual OP_STATUS ExecuteES(const uni_char* script) { return OpStatus::OK; }
#endif // HAS_ATVEF_SUPPORT

	virtual void SetTrueZoom(BOOL enable) { m_enable_true_zoom = enable; }
	virtual BOOL GetTrueZoom() { return m_enable_true_zoom; }
	virtual void SetScale(UINT32 scale) { m_scale = scale; }
	virtual UINT32 GetScale() { return m_scale; }
	virtual void SetTextScale(UINT32 scale) { m_text_scale = scale;} 
	virtual UINT32 GetTextScale() { return m_text_scale; }
	virtual void SetImageMode(OpDocumentListener::ImageDisplayMode mode) { m_image_display_mode = mode; }
	virtual OpDocumentListener::ImageDisplayMode GetImageMode() { return m_image_display_mode; }
	virtual void SetCssMode(OpDocumentListener::CssDisplayMode mode) { m_css_display_mode = mode; }
	virtual OpDocumentListener::CssDisplayMode GetCssMode() { return m_css_display_mode; }
	virtual void SetShowScrollbars(BOOL show) { }
	virtual OP_STATUS GetBackgroundColor(OpColor& background_color) { return OpStatus::OK; }
	virtual void SetDefaultBackgroundColor(const OpColor& background_color) { }
	virtual void ForceEncoding(Encoding encoding) { m_encoding = encoding; }
	virtual Encoding GetEncoding() { return m_encoding; }

#ifndef HAS_NO_SEARCHTEXT
	virtual SearchResult Search(const uni_char* search_string, const SearchInfo& search_info) { return OpWindowCommander::SEARCH_NOT_FOUND; }
	DEPRECATED(virtual BOOL Search(const uni_char* string, BOOL forward = TRUE, BOOL matchcase = FALSE, BOOL word = FALSE, BOOL wrap = FALSE, BOOL next = FALSE)) { return FALSE; }
	virtual void ResetSearchPosition() { }
	virtual BOOL GetSearchMatchRectangles(OpVector<OpRect> &all_rects, OpVector<OpRect> &active_rects) { return FALSE; }
#endif // !HAS_NO_SEARCHTEXT

#ifndef HAS_NOTEXTSELECTION
	virtual void Paste() { }
	virtual void Cut() { }
	virtual void Copy() { }

	virtual void SelectTextStart(const OpPoint& p) {}
	virtual void SelectTextEnd(const OpPoint& p) {}
	virtual void SelectTextMoveAnchor(const OpPoint& p) {}
	virtual void SelectWord(const OpPoint& p) {}
	OP_STATUS GetSelectTextStart(OpPoint& p, int &line_height) { return OpStatus::ERR_NOT_SUPPORTED; }
	OP_STATUS GetSelectTextEnd(OpPoint& p, int &line_height) { return OpStatus::ERR_NOT_SUPPORTED; }

#ifdef _PRINT_SUPPORT_	
	virtual void StartPrinting() {}
	virtual OP_STATUS Print(int from, int to, BOOL selected_only) {return OpStatus::ERR_NOT_SUPPORTED;}
	virtual void CancelPrint() {}
	virtual void PrintNextPage() {}
	virtual BOOL IsPrintable() { return FALSE; }
	virtual BOOL IsPrinting()  { return FALSE; }
#endif

	virtual BOOL HasSelectedText() { return FALSE; }
	virtual uni_char *GetSelectedText() { return NULL; }
	virtual void ClearSelection() { }

# ifdef KEYBOARD_SELECTION_SUPPORT
	virtual OP_STATUS SetEditable(BOOL enable) { return OpStatus::OK; }
	virtual BOOL HasSelectableText() { return FALSE; }
# endif // KEYBOARD_SELECTION_SUPPORT
#endif // !HAS_NOTEXTSELECTION

	virtual BOOL CopyImageToClipboard(const uni_char* url) { return FALSE; }

#ifdef SHORTCUT_ICON_SUPPORT
	virtual BOOL HasDocumentIcon() { return FALSE; }
	virtual OP_STATUS GetDocumentIcon(OpBitmap** icon) { return OpStatus::ERR; }
	virtual OP_STATUS GetDocumentIconURL(const uni_char** iconUrl) { return OpStatus::ERR; }

#endif // SHORTCUT_ICON_SUPPORT

#ifdef LINK_SUPPORT
	virtual UINT32 GetLinkElementCount() { return 0; }
	virtual BOOL GetLinkElementInfo(UINT32 index, LinkElementInfo* information) { return FALSE; }
#endif // LINK_SUPPORT

	virtual BOOL GetAlternateCssInformation(UINT32 index, CssInformation* information) { return FALSE; }
	virtual BOOL SetAlternateCss(const uni_char* title) { return FALSE; }
	virtual BOOL ShowCurrentImage() { return FALSE; }
	virtual void Focus() { }
	virtual void SetLoadingListener(OpLoadingListener* listener) { }
	virtual void SetDocumentListener(OpDocumentListener* listener) { }
	virtual void SetMailClientListener(OpMailClientListener* listener) { }

#ifdef _POPUP_MENU_SUPPORT_
	virtual void SetMenuListener(OpMenuListener* listener) { }
#endif // _POPUP_MENU_SUPPORT_

	virtual void SetLinkListener(OpLinkListener* listener) { }

#ifdef ACCESS_KEYS_SUPPORT
	virtual void SetAccessKeyListener(OpAccessKeyListener* listener) { }
#endif // ACCESS_KEYS_SUPPORT

#ifdef _PRINT_SUPPORT_
	virtual void SetPrintingListener(OpPrintingListener* listener) { }
#endif // _PRINT_SUPPORT_

	virtual void SetErrorListener(OpErrorListener* listener) { }


#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
	virtual void SetSSLListener(OpSSLListener* listener) { }
#endif

#ifdef _ASK_COOKIE
	virtual void SetCookieListener(OpCookieListener* listener) { }
#endif // _ASK_COOKIE

#ifdef WIC_FILESELECTION_LISTENER
	virtual void SetFileSelectionListener(OpFileSelectionListener* listener) { }
#endif // WIC_FILESELECTION_LISTENER
#ifdef WIC_COLORSELECTION_LISTENER
	virtual void SetColorSelectionListener(OpColorSelectionListener* listener) {}
#endif // WIC_COLORSELECTION_LISTENER

#ifdef NEARBY_ELEMENT_DETECTION
	virtual void SetFingertouchListener(OpFingertouchListener* listener) { }
#endif

#ifdef WIC_PERMISSION_LISTENER
	/** Set listener for file choosing. */
	virtual void SetPermissionListener(OpPermissionListener* listener) { }

	virtual OP_STATUS SetUserConsent(OpPermissionListener::PermissionCallback::PermissionType type,
			OpPermissionListener::PermissionCallback::PersistenceType persistence,
			const uni_char* host,
			BOOL3 allowed)
	{
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	virtual void GetUserConsent(INTPTR user_data) {}
#endif // WIC_PERMISSION_LISTENER

#ifdef SCOPE_URL_PLAYER
	/** Set listener for url-player callback events. */
	virtual void SetUrlPlayerListener(WindowCommanderUrlPlayerListener* listener) {}
#endif //SCOPE_URL_PLAYER

	virtual OP_STATUS CreateDocumentContext(OpDocumentContext** ctx) { return OpStatus::ERR; }
	virtual OP_STATUS CreateDocumentContextForScreenPos(OpDocumentContext** ctx, const OpPoint& screen_pos) { return OpStatus::ERR; }

#ifdef _POPUP_MENU_SUPPORT_
	virtual void RequestPopupMenu(const OpPoint* point) {}
	virtual void OnMenuItemClicked(OpDocumentContext* context, UINT32 id) {}
	virtual OP_STATUS GetMenuItemIcon(UINT32 icon_id, OpBitmap*& icon_bitmap) { return OpStatus::ERR; }
#endif // _POPUP_MENU_SUPPORT_

	virtual void LoadImage(OpDocumentContext& ctx, BOOL disable_turbo=FALSE) { }

	virtual OP_STATUS SaveImage(OpDocumentContext& ctx, const uni_char* filename) { return OpStatus::ERR; }
#ifdef USE_OP_CLIPBOARD
	virtual BOOL CopyImageToClipboard(OpDocumentContext& ctx) { return FALSE; }
	virtual BOOL CopyBGImageToClipboard(OpDocumentContext& ctx) { return FALSE; }
#endif // USE_OP_CLIPBOARD

	virtual void ClearText(OpDocumentContext& ctx) {}
#ifdef ACCESS_KEYS_SUPPORT
	virtual void SetAccessKeyMode(BOOL enable) { m_access_key_mode = enable; }
	virtual BOOL GetAccessKeyMode() { return m_access_key_mode; }
#endif // ACCESS_KEYS_SUPPORT

#ifdef CONTENT_MAGIC
	virtual void EnableContentMagicIndicator(BOOL enable, INT32 x, INT32 y) { }
#endif

	virtual OP_STATUS GetSelectedLinkInfo(const uni_char** url, const uni_char** title) { return OpStatus::ERR; }
	virtual OP_STATUS GetSelectedLinkInfo(OpString& url, OpString& title) { return OpStatus::ERR; }

	virtual BOOL GetScriptingDisabled() { return m_scripting_disabled; }
	virtual void SetScriptingDisabled(BOOL disabled) { m_scripting_disabled = disabled; }
	virtual BOOL GetScriptingPaused() { return m_scripting_paused; }
	virtual void SetScriptingPaused(BOOL paused) { m_scripting_paused = paused; }

	virtual void SetVisibleOnScreen(BOOL is_visible) { }
	virtual void SetDrawHighlightRects(BOOL enable) { }

#ifdef VIEWPORTS_SUPPORT
	virtual OpViewportController* GetViewportController() const { return NULL; }
#else
# ifdef ADAPTIVE_ZOOM_INTERFACE
	virtual OP_STATUS EnableAdaptiveZoom() { return OpStatus::ERR; }
	virtual OP_STATUS DisableAdaptiveZoom() { return OpStatus::ERR; }
	virtual BOOL IsAdaptiveZoomEnabled() { return FALSE; }
	virtual OP_STATUS AdaptiveZoomResize(INT32 w, INT32 h) { return OpStatus::ERR; }
	virtual void AdaptiveZoomIn() { }
	virtual void AdaptiveZoomOut() { }
	virtual BOOL AdaptiveZoomPan(INT32 dx, INT32 dy) { return FALSE; }
	virtual short AdaptiveZoomGetMaxParagraphWidth() { return 0; }
	virtual void AdaptiveZoomSetDocumentPos(INT32 x, INT32 y) { }
	virtual void AdaptiveZoomGetDocumentPos(UINT32& x, UINT32& y) { }
	virtual OpRect AdaptiveZoomGetVisible()  { return OpRect; }
	virtual INT32 AdaptiveZoomShowRect(const OpRect& rect, BOOL animate = FALSE)  { return 0; }
	virtual AdaptiveZoomState AdaptiveZoomGetState() { return OpWindowCommander::VIEW_ALL; }
# endif // ADAPTIVE_ZOOM_INTERFACE

	virtual void GetTextParagraphRects(Head& paragraph_rects) const { }
#endif // VIEWPORTS_SUPPORT

#ifdef NEARBY_ELEMENT_DETECTION
	virtual void HideFingertouchElements()  { }
#endif // NEARBY_ELEMENT_DETECTION

#ifdef DOM_JSPLUGIN_CUSTOM_EVENT_HOOK
# ifdef DOC_CAP_CUSTOM_WINDOW_EVENTS
	virtual OP_STATUS SendCustomWindowEvent(const uni_char* event_name, int event_param) { return OpStatus::ERR; }
# endif // DOC_CAP_CUSTOM_WINDOW_EVENTS

# ifdef GADGET_SUPPORT
	virtual OP_STATUS SendCustomGadgetEvent(const uni_char* event_name, int event_param) { return OpStatus::ERR; }
# endif // GADGET_SUPPORT
#endif // DOM_JSPLUGIN_CUSTOM_EVENT_HOOK

	//virtual void OnCancelDialog(OpWindowCommander* commander, DialogCallback* callback) {}
#ifdef SVG_SUPPORT
	/** @return TRUE if there was an SVG document fragment at the context position. */
	virtual BOOL HasSVG() {return FALSE;}
# ifdef DOC_CAP_SVG_CONTEXTMENU_ADDITIONS
	virtual BOOL HasSVGAnimation() {return FALSE;}
	virtual BOOL IsSVGAnimationRunning(){return FALSE;}
	virtual BOOL IsSVGZoomAndPanAllowed(){return FALSE;}
# endif // DOC_CAP_SVG_CONTEXTMENU_ADDITIONS
	virtual OP_STATUS SVGZoomBy(OpDocumentContext& context, int zoomdelta){ return OpStatus::OK;}
	virtual OP_STATUS SVGZoomTo(OpDocumentContext& context, int zoomlevel) { return OpStatus::OK;}
	virtual OP_STATUS SVGResetPan(OpDocumentContext& context) { return OpStatus::OK;}
	virtual OP_STATUS SVGPlayAnimation(OpDocumentContext& context) { return OpStatus::OK;}
	virtual OP_STATUS SVGPauseAnimation(OpDocumentContext& context) { return OpStatus::OK;}
	virtual OP_STATUS SVGStopAnimation(OpDocumentContext& context) { return OpStatus::OK;}

#endif // SVG_SUPPORT

	virtual OP_STATUS MediaPlay(OpDocumentContext& context, BOOL play) { return OpStatus::OK; }
	virtual OP_STATUS MediaMute(OpDocumentContext& context, BOOL mute) { return OpStatus::OK; }
	virtual OP_STATUS MediaShowControls(OpDocumentContext& context, BOOL show) { return OpStatus::OK; }

	virtual void ScreenPropertiesHaveChanged() {}

	virtual UINT32 SecurityLowStatusReason() { return 0; }

#ifdef SECURITY_INFORMATION_PARSER
	virtual OP_STATUS CreateSecurityInformationParser(OpSecurityInformationParser** parser) { return OpStatus::ERR; }
	virtual void GetSecurityInformation(OpString& string, BOOL isEV) { string.Empty(); }
#endif // SECURITY_INFORMATION_PARSER
#ifdef TRUST_INFO_RESOLVER
	virtual OP_STATUS CreateTrustInformationResolver(OpTrustInformationResolver** resolver, OpTrustInfoResolverListener * listener) { return OpStatus::ERR; }
#endif // TRUST_INFO_RESOLVER

	virtual const uni_char* ServerUniName() const { return 0; }
	virtual WIC_URLType GetWicURLType() { return WIC_URLType_HTTP; }
	virtual BOOL3 HttpResponseIs200() { return MAYBE; }
	
	virtual OpGadget* GetGadget() const { return m_gadget; }

	virtual WritingSystem::Script GetPreferredScript(BOOL focused_frame) { return WritingSystem::Unknown; }

#ifdef APPLICATION_CACHE_SUPPORT
	virtual void SetApplicationCacheListener(OpApplicationCacheListener* listener){}
#endif // APPLICATION_CACHE_SUPPORT

	virtual BOOL IsIntranet() {return FALSE;}

	virtual OP_STATUS CreateSearchURL(OpDocumentContext& context, OpString8* url, OpString8* query, OpString8* charset, BOOL* is_post) { return OpStatus::ERR; }
	virtual BOOL HasDocumentFocusedFormElement() { return FALSE; }
	virtual BOOL IsDocumentSupportingViewMode(WindowViewMode view_mode) const { return FALSE; }
	virtual BOOL IsDocumentSupportingViewMode(WindowViewMode view_mode) { return FALSE; }

	virtual void ForcePluginsDisabled(BOOL disabled) { m_called_force_disable_plugins = TRUE; }
	virtual BOOL GetForcePluginsDisabled() const { return m_called_force_disable_plugins; }

	virtual FullScreenState GetDocumentFullscreenState() const { return OpWindowCommander::FULLSCREEN_NONE; }
	virtual OP_STATUS SetDocumentFullscreenState(FullScreenState value) { return OpStatus::ERR; }

	virtual void CancelAllScriptingTimeouts() {}

	virtual void OnLinkNavigated(OpWindowCommander* commander, const uni_char* url, const uni_char* link_title, BOOL is_image, const OpRect* link_rect = NULL) { }

#ifdef KEYBOARD_SELECTION_SUPPORT
	virtual void OnKeyboardSelectionModeChanged(OpWindowCommander* commander, BOOL enabled) {}
	virtual OP_STATUS SetKeyboardSelectable(BOOL enable) { return OpStatus::OK; }
	virtual void MoveCaret(CaretMovementDirection direction, BOOL in_selection_mode) {}
	virtual OP_STATUS GetCaretRect(OpRect& rect) { return OpStatus::OK; }
	virtual OP_STATUS SetCaretPosition(const OpPoint& position) { return OpStatus::OK; }
#endif // KEYBOARD_SELECTION_SUPPORT

	// Initilize mock state

	OpMockWindowCommander() :
		m_op_window(NULL),
		m_layout_mode(OpWindowCommander::NORMAL),
		m_smart_frames_mode(FALSE),
		m_privacy_mode(FALSE),
		m_enable_true_zoom(FALSE),
		m_scale(100),
		m_text_scale(100),
		m_image_display_mode(OpDocumentListener::ALL_IMAGES),
		m_css_display_mode(OpDocumentListener::CSS_AUTHOR_MODE),
		m_encoding(OpWindowCommander::ENCODING_AUTOMATIC),
		m_access_key_mode(FALSE),
		m_scripting_disabled(FALSE),
		m_scripting_paused(FALSE),
		m_has_current_element(FALSE),
		m_url_context_id(UINT32_MAX),
#ifdef WEB_TURBO_MODE
		m_turbo_mode(FALSE),
#endif // WEB_TURBO_MODE
		m_called_get_security_mode(FALSE),
		m_called_on_ui_window_closing(FALSE),
		m_called_has_previous(FALSE),
		m_called_has_next(FALSE),
		m_called_has_current_element(FALSE),
		m_called_get_current_history_pos(FALSE),
		m_called_force_disable_plugins(FALSE) {}

public:

	// Query interface for mocking

	OpWindow* m_op_window;
	LayoutMode m_layout_mode;
	BOOL m_smart_frames_mode;
	BOOL m_privacy_mode;
	BOOL m_enable_true_zoom;
	UINT32 m_scale;
	UINT32 m_text_scale;
	OpDocumentListener::ImageDisplayMode m_image_display_mode;
	OpDocumentListener::CssDisplayMode m_css_display_mode;
	Encoding m_encoding;
	BOOL m_access_key_mode;
	BOOL m_scripting_disabled;
	BOOL m_scripting_paused;
	BOOL m_has_current_element;
	OpString m_current_url;
	URL_CONTEXT_ID m_url_context_id;
#ifdef WEB_TURBO_MODE
	BOOL m_turbo_mode;
#endif // WEB_TURBO_MODE
	OpGadget* m_gadget;
	
	// Sensing

	BOOL m_called_get_security_mode;
	BOOL m_called_on_ui_window_closing;
	BOOL m_called_has_previous;
	BOOL m_called_has_next;
	BOOL m_called_has_current_element;
	BOOL m_called_get_current_history_pos;
	BOOL m_called_force_disable_plugins;
};

#endif // MOCK_OP_WINDOW_COMMANDER_H
