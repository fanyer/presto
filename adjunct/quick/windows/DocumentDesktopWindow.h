/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOCUMENT_DESKTOP_WINDOW_H
#define DOCUMENT_DESKTOP_WINDOW_H

#include "modules/windowcommander/OpWindowCommander.h"

#include "adjunct/desktop_util/search/search_suggest.h"
#include "adjunct/quick/windows/DocumentWindowListener.h"
#include "adjunct/quick/data_types/OpenURLSetting.h"
#include "adjunct/quick/windows/DesktopTab.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "adjunct/quick_toolkit/widgets/OpToolTip.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/application/BrowserApplicationCacheListener.h"
#include "adjunct/quick/permissions/DesktopPermissionCallback.h"
#include "adjunct/quick/widgets/DocumentView.h"
#include "adjunct/desktop_util/history/DocumentHistory.h"

class OpAddressDropDown;
class OpSearchDropDown;
class FindTextManager;
class OpInfobar;
class CycleAccessKeysPopupWindow;
class BrowserDesktopWindow;
class ContentFilterItem;
class DocumentDesktopWindow;
class OpFindTextBar;
class OpWandStoreBar;
class OpGoToIntranetBar;
class OpLocalStorageBar;
class WebhandlerBar;
class WandInfo;
class OpPluginCrashedBar;
class PermissionPopupController;

#ifdef PLUGIN_AUTO_INSTALL
class OpPluginMissingBar;
class OpReloadForPluginBar;
#endif // PLUGIN_AUTO_INSTALL

class DownloadExtensionBar;

namespace DocumentDesktopWindowConsts
{
	// Fix for DSK-360921 - Small windows can be used to trick users with opera:config
	// To prevent small popup windows, window size cannot be below minimum dimensions
	const UINT32 MINIMUM_WINDOW_WIDTH = 125;
	const UINT32 MINIMUM_WINDOW_HEIGHT = 160;
}

class DocumentDesktopWindow : public DesktopTab
							, public OpPageAdvancedListener
							, public OpPermissionListener
#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
							, public SearchSuggest::Listener
							, public OpSearchSuggestionsCallback::Listener
#endif
{
	friend class CycleAccessKeysPopupWindow;

	public:
		enum
		{
			ASK_FOR_PERMISSION_ON_PAGE_LOAD,
			ASK_FOR_PERMISSION_ON_USER_REQUEST
		};

		struct InitInfo
		{
			InitInfo();

			bool m_show_toolbars;     ///< set false to disable all toolbars
			bool m_focus_document;    ///< set true if document should take over focus
			bool m_no_speeddial;      ///< set true to disable speeddial in this window
			bool m_private_browsing;  ///< set true if document should open in private browsing mode
			bool m_staged_init;       ///< set true to init window using DispatchableObject interface
			bool m_delayed_addressbar;///< set true to delay possibility of clicking in addressbar DSK-361401
			OpWindowCommander* m_window_commander;
			OpenURLSetting* m_url_settings; ///< set this to use non-default url opening settings
		};

		DocumentDesktopWindow() {}

		/**
		 * @param parent_workspace Workspace, that will hold this window.
		 * @param parent_window    Window to embed this document in.
		 * @param info             Specific initialization options; leave NULL to use defaults. @see InitInfo
		 */
		DocumentDesktopWindow(OpWorkspace* parent_workspace, OpWindow* parent_window, InitInfo* info = NULL);
		virtual					~DocumentDesktopWindow();
		//
		// DispatchableObject
		//

		virtual int				GetPriority() const;
		virtual OP_STATUS		InitPartial();
		virtual OP_STATUS		InitObject();

		OP_STATUS				AddDocumentWindowListener(DocumentWindowListener* listener) {return m_listeners.Add(listener);}
		void					RemoveDocumentWindowListener(DocumentWindowListener* listener) {m_listeners.Remove(listener);}

		void					LayoutPopupProgress();
		void					LayoutPopupProgress(OpRect visible_rect);
#ifdef SUPPORT_VISUAL_ADBLOCK
		void					LayoutAdblockToolbar();
#endif // SUPPORT_VISUAL_ADBLOCK

		void					SetTitle(const uni_char* new_title = NULL);
		virtual const uni_char*	GetTitle(BOOL no_extra_info = TRUE);
		void					SetDelayedTitle(const uni_char* new_title = NULL);

		void					GoToPage(const uni_char* url, BOOL url_came_from_address_field = FALSE, BOOL reloadOption = FALSE, URL_CONTEXT_ID context_id = -1);
		void 					GoToPage( const OpenURLSetting& setting, BOOL reloadOption = FALSE );
		OP_STATUS				GetURL(OpString &url);
		void					ShowWandStoreBar(WandInfo* info = NULL);
		void					ShowGoToIntranetBar(const uni_char *address);

		OpLocalStorageBar*		GetLocalStorageBar();

		// hide the bar and don't call callback
		void					CancelApplicationCacheBar(UINTPTR id);

		OpWindowCommander*		GetWindowCommander() {return m_document_view ? m_document_view->GetWindowCommander() : NULL;}
		OpBrowserView*			GetBrowserView() {return m_document_view;}

		BOOL					HasShowAddressBarButton();
		void					GetSizeFromBrowserViewSize(INT32& width, INT32& height);

		OpAddressDropDown*		GetAddressDropdown();
		OpSearchDropDown*		GetSearchDropdown();

		CycleAccessKeysPopupWindow* GetCycleAccessKeysPopupWindow() {return m_cycles_accesskeys_popupwindow;}

		BOOL					HasURLChanged() {return !(m_previous_url == m_current_url);}
		BOOL					HasDocument();
		BOOL					HasDocumentInHistory();
		BOOL					IsLoading();

		BOOL 					HandleImageActions(OpInputAction* action, OpWindowCommander* wc);
		BOOL 					HandleFrameActions(OpInputAction* action, OpWindowCommander* wc);

		void					InstallSkin(const uni_char* url);
		void					ShowPluginCrashedBar(bool show);

		// Subclassing DesktopWindow
		virtual const char*		GetWindowName() {return "Document Window";}
		virtual OP_STATUS		GetItemData(ItemData* item_data);
		virtual	Image			GetThumbnailImage() { return GetThumbnailImage(THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT); }
		virtual	Image			GetThumbnailImage(OpPoint &logoStart);
		virtual Image			GetThumbnailImage(UINT32 width, UINT32 height, BOOL high_quality = FALSE, BOOL use_skin_image_if_present = TRUE);
		virtual BOOL			HasFixedThumbnailImage();
		virtual BOOL			IsClosableByUser();
		virtual void			SetHidden(BOOL hidden);
		virtual double			GetWindowScale();
		virtual void			QueryZoomSettings(double &min, double &max, double &step, const OpSlider::TICK_VALUE* &tick_values, UINT8 &num_tick_values);

		//FIXME bug in WindowCommander::GetPrivacyMode
		virtual BOOL			PrivacyMode(){return m_private_browsing;}
		// Hooks

		virtual void			OnLayout();
		virtual void			OnLayoutAfterChildren();
		virtual void			OnActivate(BOOL activate, BOOL first_time);
		virtual void			OnResize(INT32 width, INT32 height);
		virtual void			OnFullscreen(BOOL fullscreen);
		virtual void			OnSessionReadL(const OpSessionWindow* session_window);
		virtual void			OnSessionWriteL(OpSession* session, OpSessionWindow* session_window, BOOL shutdown);

		virtual void			OnSettingsChanged(DesktopSettings* settings);

		// MessageObject::HandleCallback
		virtual void			HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

		// OpTypedObject
		virtual Type			GetType() { return WINDOW_TYPE_DOCUMENT; }

		// OpWidgetListener

	    virtual BOOL			OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked);
#ifdef SUPPORT_SPEED_DIAL
		virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
#endif // SUPPORT_SPEED_DIAL
		virtual void			OnClick(OpWidget *widget, UINT32 id = 0);

		// OpInputContext
		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual const char*		GetInputContextName();
		virtual void			RestoreFocus(FOCUS_REASON reason);

		// OpAdvancedPageListener
		virtual void			OnPageUrlChanged(OpWindowCommander* commander, const uni_char* url);
		virtual void			OnPageStartLoading(OpWindowCommander* commander);
		virtual void			OnPageLoadingProgress(OpWindowCommander* commander, const LoadingInformation* info);
		virtual void			OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL stopped_by_user);
		virtual void			OnPageAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback);

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
		virtual void			OnPageSearchSuggestionsRequested(OpWindowCommander* commander, const uni_char* url, OpSearchSuggestionsCallback* callback);
		virtual void 			OnPageSearchSuggestionsUsed(OpWindowCommander* commander, BOOL suggestions);
#endif
#ifdef EMBROWSER_SUPPORT
		virtual void			OnPageLoadingCreated(OpWindowCommander* commander){};
        virtual void			OnPageStartUploading(OpWindowCommander* commander){};
        virtual void			OnPageUploadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status){};
#endif
		virtual void			OnPageHover(OpWindowCommander* commander, const uni_char* url, const uni_char* link_title, BOOL is_image);
		virtual void			OnPageNoHover(OpWindowCommander* commander);
		virtual void			OnPageTitleChanged(OpWindowCommander* commander, const uni_char* title);
		virtual void			OnPageDocumentIconAdded(OpWindowCommander* commander);
		virtual void			OnPageScaleChanged(OpWindowCommander* commander, UINT32 newScale);
		virtual void			OnPageInnerSizeRequest(OpWindowCommander* commander, UINT32 width, UINT32 height);
		virtual void			OnPageGetInnerSize(OpWindowCommander* commander, UINT32* width, UINT32* height);
		virtual void			OnPageOuterSizeRequest(OpWindowCommander* commander, UINT32 width, UINT32 height);
		virtual void			OnPageGetOuterSize(OpWindowCommander* commander, UINT32* width, UINT32* height);
		virtual void			OnPageMoveRequest(OpWindowCommander* commander, INT32 x, INT32 y);
		virtual void			OnPageGetPosition(OpWindowCommander* commander, INT32* x, INT32* y);
		virtual void			OnPageClose(OpWindowCommander* commander);
	    virtual void            OnPageRaiseRequest(OpWindowCommander* commander);
	    virtual void            OnPageLowerRequest(OpWindowCommander* commander);
		virtual BOOL			OnPagePopupMenu(OpWindowCommander* commander, OpDocumentContext& context) { return FALSE; }
#ifdef SUPPORT_VISUAL_ADBLOCK
		virtual void			OnPageContentBlocked(OpWindowCommander* commander, const uni_char *image_url);
		virtual void			OnPageContentUnblocked(OpWindowCommander* commander, const uni_char *image_url);
#endif // SUPPORT_VISUAL_ADBLOCK
		virtual void			OnPageSecurityModeChanged(OpWindowCommander* commander, OpDocumentListener::SecurityMode mode, BOOL inline_elt);
		virtual void			OnPageTrustRatingChanged(OpWindowCommander* commander, TrustRating new_rating);
		virtual void			OnPageOnDemandStateChangeNotify(OpWindowCommander* commander, BOOL has_placeholders);
		virtual void			OnUnexpectedComponentGone(OpWindowCommander* commander, OpComponentType type, const OpMessageAddress& address, const OpStringC& information);
		virtual void			OnGeolocationAccess(OpWindowCommander* commander, const OpVector<uni_char>* hosts);
		virtual void			OnCameraAccess(OpWindowCommander* commander, const OpVector<uni_char>* hosts);
		virtual OP_STATUS		OnJSFullScreenRequest(OpWindowCommander* commander, BOOL enable_fullscreen);

		virtual void			OnPageAccessKeyUsed(OpWindowCommander* commander);
		OP_STATUS				HandleWebHandlerRequest(const OpStringC& hostname, const OpVector<OpString>& request);

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
		// SearchSuggest::Listener
		virtual void 			OnQueryStarted() {}
		virtual void  			OnQueryComplete(const OpStringC& search_id, OpVector<SearchSuggestEntry>& entries);
		virtual void  			OnQueryError(const OpStringC& search_id, OP_STATUS error) {}

		// OpSearchSuggestionsCallback::Listener
		virtual void 			OnDestroyed(OpSearchSuggestionsCallback* callback);
#endif

#ifdef PLUGIN_AUTO_INSTALL
		virtual void			OnPageNotifyPluginMissing(OpWindowCommander* commander, const OpStringC& a_mime_type);
		virtual void			OnPagePluginDetected(OpWindowCommander* commander, const OpStringC& mime_type);
		virtual void			OnPageRequestPluginInfo(OpWindowCommander* commander, const OpStringC& mime_type, OpString& plugin_name, BOOL& available);
		virtual void			OnPageRequestPluginInstallDialog(OpWindowCommander* commander, const PluginInstallInformation& information);
		virtual void			OnPluginAvailable(const OpStringC& mime_type);
#endif // PLUGIN_AUTO_INSTALL

		virtual void			OnPageJSPopup(OpWindowCommander* commander,	const uni_char *url, const uni_char *name, int left, int top, int width, int height, BOOL3 scrollbars, BOOL3 location, BOOL refuse, BOOL unrequested, OpDocumentListener::JSPopupCallback *callback);

		// OpToolTipListener
		virtual BOOL			HasToolTipText(OpToolTip* tooltip) {return TRUE;}
		virtual void			GetToolTipText(OpToolTip* tooltip, OpInfoText& text);
		virtual void			GetToolTipThumbnailText(OpToolTip* tooltip, OpString& title, OpString& url_string, URL& url);

		// OpPermissionListener
		virtual void			OnAskForPermission(OpWindowCommander *commander, PermissionCallback *callback);
		virtual void			OnAskForPermissionCancelled(OpWindowCommander *wic, PermissionCallback *callback);
		virtual void			OnUserConsentInformation(OpWindowCommander* comm, const OpVector<OpPermissionListener::ConsentInformation>& info, INTPTR user_data);
		virtual void			OnUserConsentError(OpWindowCommander* comm, OP_STATUS status, INTPTR user_data) {}

		void					HandleCurrentPermission(bool allow, OpPermissionListener::PermissionCallback::PersistenceType persistence);
		void 					SetUserConsent(const OpStringC hostname,
								OpPermissionListener::PermissionCallback::PermissionType type,								BOOL3 allowed,
								OpPermissionListener::PermissionCallback::PersistenceType persistence);

		/**
		 * Update or recheck image/icon used for this window
		 * @param force Force a recheck, even if we already had an icon from before
		 * @param notify Notify listeners about possible changes
		 */
		bool					UpdateWindowImage(BOOL force, BOOL notify = FALSE);

		OP_STATUS				SaveActiveDocumentAsFile();
		OP_STATUS				SaveActiveDocumentAtLocation(const uni_char* path);

		void					UpdateAccessKeys();
		OpenURLSetting&			GetOpenURLSetting() { return m_openurl_setting; }

#ifdef WEBSERVER_SUPPORT
		const OpStringC &		GetGadgetIdentifier() { return m_gadget_identifier; }
		OP_STATUS				SetGadgetIdentifier( const OpStringC & gadget_identifier) { return m_gadget_identifier.Set(gadget_identifier.CStr()); }
#endif // WEBSERVER_SUPPORT

		virtual void			EnableTransparentSkin(BOOL enable);

		/**
		 * Returns visibility status of private page.
		 *
		 * @return true if current page points to private page and tab is opened
		 * in private mode, otherwise false.
		 */
		bool IsPrivateIntroPageShown();

		// OpScopeDesktopWindowManager
		virtual OP_STATUS		ListWidgets(OpVector<OpWidget> &widgets);

		OpDocumentListener::TrustMode GetTrustMode() {return m_current_trust_mode;}

		// true if the current page failed and there is no loading url
		BOOL					LoadingFailed() const {return m_loading_failed;}

		bool 					PermissionControllerDisplayed() const { return m_permission_controller != NULL; }

		/**
		 * Gets DownloadExtensionBar which belongs to this DocumentDesktopWindow.
		 *
		 * @param create_if_needed when set to TRUE new DownloadExtensionBar
		 *        will be created if it not exists.
		 */
		DownloadExtensionBar* GetDownloadExtensionBar(BOOL create_if_needed);

		/**
		 * Fetches list of history
		 * Depending on arguments 'forward' and 'fast' list of history is generated.
		 *
		 * @param[out] history_list contains history information.
		 * @param forward identifies type of history list. If true, 'history_list'
		 * comprises of forward history list, otherwise rewind history list.
		 * @param fast identifies rewind or fast-foward history list.
		 */
		OP_STATUS GetHistoryList(OpAutoVector<DocumentHistoryInformation>& history_list, bool forward, bool fast) const;

		/**
		 * @return true if speeddial view is active otherwise false.
		 */
		bool IsSpeedDialActive() const;

		/**
		 * DocumentView factory is used to create various kind of document view.
		 *
		 * @param type identifies what kind of document view to be created.
		 * @param commander can be used by document view to interact with core
		 */
		OP_STATUS CreateDocumentView(DocumentView::DocumentViewTypes type, OpWindowCommander* commander = NULL);

		/**
		 * Returns documen view object for a given type.
		 *
		 * @param type identifies document view to be returned.
		 * @return document view object if one exists, otherwise NULL.
		 */
		DocumentView* GetDocumentViewFromType(DocumentView::DocumentViewTypes type);

		/**
		 * Sets one of the document view active which matches given address.
		 *
		 * @param url_str identifies address associated with a document view.
		 * @param set_focus if 'true', activated document will have focus too.
		 */
		OP_STATUS SetActiveDocumentView(const uni_char* url_str);

		/**
		 * Sets one of the document view active which matches given type.
		 *
		 * @param type identifies document view.
		 * @param set_focus if 'true', activated document will have focus too.
		 */
		OP_STATUS SetActiveDocumentView(DocumentView::DocumentViewTypes type);

		/**
		 * @return active document view if exists, otherwise NULL.
		 */
		DocumentView* GetActiveDocumentView() const;

	private:
		class TrustInfoHistory :
		public OpHistoryUserData
		{
		public:
			TrustInfoHistory(OpDocumentListener::TrustMode trust_mode) : m_current_trust_mode(trust_mode){ }
			OpDocumentListener::TrustMode m_current_trust_mode;
		};

		struct
		{
			OpString hostname;
			BOOL3 allowed;
			OpPermissionListener::PermissionCallback::PersistenceType persistence;
		} m_permission_data;

		OP_STATUS				Init(InitInfo& init_settings);

		/**
		 * In case persona is on and SD is active
		 * sets MDEWidget to use transparency in order to
		 * have fixed SD background //DSK-364182
		 */
		void					UpdateMDETransparency();

		void					BroadcastUrlChanged(const OpStringC& url);
		void					BroadcastDocumentChanging();
		void					BroadcastTransparentAreaChanged(int height);
		OP_STATUS				SetupInitialView(BOOL is_speed_dial);
		OP_STATUS				AddPluginMissingBar(const OpStringC& mime_type);
		OP_STATUS				DeletePluginMissingBars();
		void					EnsureAllowedOuterSize(UINT32& width, UINT32& height) const;
		void					UpdateCollapsedAddressBar();
		UINT32					GetClipboardToken();
		void					LayoutToolbar(OpToolbar* toolbar, OpRect& rect_with_margin, OpRect& rect, BOOL compute_only, BOOL allow_margins = FALSE);
		void					CopySessionWindowL(const OpSessionWindow* in, OpSessionWindow* out) const;
		OP_STATUS				ShowPermissionPopup(DesktopPermissionCallback* callback);
		bool					IsBrowserViewActive() const;

#ifdef SUPPORT_VISUAL_ADBLOCK
		void					ContentBlockChanged(OpWindowCommander* commander, const uni_char* image_url, BOOL remove);
#endif // SUPPORT_VISUAL_ADBLOCK

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
		OP_STATUS  				StartSuggestionsSearch(const uni_char* url_string);
#endif
		// DocumentView methods
		OP_STATUS				InitDocumentView(DocumentView* view);

		/**
		 * Deletes/releases document views.
		 * Document views which are of not type DOCUMENT_TYPE_BROWSER are destroyed
		 * if they are far behind/ahead of current history position. This ensures
		 * more mem and requires recreation of deleted views if they are navigated
		 * back.
		 *
		 * @param force_release if 'true', will delete all views, except
		 * DOCUMENT_TYPE_BROWSER, without respecting history navigation criteria.
		 * Forcefully deletion is required if another tab is activated, provided that
		 * present active tab is of type DOCUMENT_TYPE_BROWSER.
		 * See implementation of OnActivate()
		 */
		void ReleaseDocumentView(bool force_release = false);

		void					ShowGeolocationLicenseDialog(OpPermissionListener::PermissionCallback::PersistenceType persistence);


		OpListeners<DocumentWindowListener>	m_listeners;

		OpToolbar*				m_address_bar;
		OpToolbar*				m_progress_bar;
		OpToolbar*				m_popup_progress_bar;
#ifdef SUPPORT_NAVIGATION_BAR
		OpToolbar*				m_navigation_bar;
#endif // SUPPORT_NAVIGATION_BAR
		OpToolbar*				m_view_bar;
		OpLocalStorageBar*		m_local_storage_bar;
		WebhandlerBar* 			m_webhandler_bar;
		OpWandStoreBar*			m_wandstore_bar;
		OpGoToIntranetBar*		m_intranet_bar;
		OpPluginCrashedBar*		m_plugin_crashed_bar;

#ifdef SUPPORT_VISUAL_ADBLOCK
		OpInfobar*						m_popup_adblock_bar;
		BOOL							m_adblock_edit_mode;	// TRUE when a page is in adblock edit mode

#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
		OpVector<ContentFilterItem>		m_content_to_block;		// model used to keep URLs that the user wants to block
		OpVector<ContentFilterItem>		m_content_to_unblock;	// model used to keep URLs that the user wants to unblock
#else
		ContentBlockTreeModel			m_content_to_block;		// model used to keep URLs that the user wants to block
		ContentBlockTreeModel			m_content_to_unblock;	// model used to keep URLs that the user wants to unblock
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL

#endif // SUPPORT_VISUAL_ADBLOCK

		PermissionPopupController*		m_permission_controller;

#ifdef PLUGIN_AUTO_INSTALL
		OpVector<OpPluginMissingBar>	m_plugin_missing_bars;
		OpVector<OpString>				m_plugin_missing_names;
#endif // PLUGIN_AUTO_INSTALL

		OpBrowserView*					m_document_view; // The DocumentDesktopWindow will be a BrowserViewListener to the m_document_view

		OpButton*						m_show_addressbar_button;
		BOOL							m_show_toolbars;
		bool							m_is_read_document;
		BOOL							m_focus_document;
		BOOL							m_private_browsing; // private browsing mode
		URL								m_current_url;
		URL								m_previous_url;
		DesktopWidgetWindow*			m_popup_progress_window;
		OpString						m_title;

		CycleAccessKeysPopupWindow		*m_cycles_accesskeys_popupwindow;	// used for shift-esc access key mode
		DesktopPermissionCallbackQueue	m_permission_callbacks;

		OpenURLSetting					m_openurl_setting;

		BOOL							m_handled_initial_document_view_activation; // used for setting focus in address field when starting opera with speeddial page

#ifdef WEB_TURBO_MODE
		INT32							m_turboTransferredBytes;
		INT32							m_turboOriginalTransferredBytes;
		INT32							m_prev_turboTransferredBytes;
		INT32							m_prev_turboOriginalTransferredBytes;

		// Usage report
		INT32							m_transferredBytes;
		double							m_transferStart;
#endif //WEB_TURBO_MODE

#ifdef WEBSERVER_SUPPORT
		OpString						m_gadget_identifier; // the identifier of the Unite service that opened this window
#endif // WEBSERVER_SUPPORT

		BOOL							m_loading_failed; // Cache the status so we know if the current page is a error page.
		bool							m_has_OnPageLoadingFinished_been_called;

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
		MessageHandler*					m_mh;
		SearchSuggest*					m_search_suggest;
		OpString	 					m_next_search;
		OpString						m_search_guid;
		OpSearchSuggestionsCallback*	m_search_suggestions_callback;
#endif

		OpDocumentListener::TrustMode	m_current_trust_mode;

#ifdef EXTENSION_SUPPORT
	BOOL3							m_is_extension_document;
	void							CheckExtensionDocument();
#endif // EXTENSION_SUPPORT

	BOOL							m_has_plugin_placeholders;

		// Maximum count of Missing Plugin Toolbars per window. A new toolbar will be created for each missing plugin that is
		// available for download from the autoupdate server.
		static const unsigned int MAX_PLUGIN_MISSING_BAR_COUNT;

		OpString						m_delayed_title;		///< store for title updates, that should not afect UI
		bool							m_delayed_title_change;	///< flag, that indicates if title change should not affect UI
		bool							m_staged_init;			///< true as long as window is being initialized as dispatchable object
		OpWidgetImage*					m_mockup_icon;			///< favicon to use instead of document's icon when initializing as dispatchable object
		DocumentHistory*				m_doc_history;

		/**
		 * Identifies info associated with document view.
		 */
		struct DocumentViewInfo
		{
			DocumentViewInfo(): m_view(NULL), m_history_pos(-1) { }
			DocumentView* m_view;
			int m_history_pos;
		};

		OpAutoVector<DocumentViewInfo> m_document_views;
		INT32 m_active_doc_view_index;
};


/***********************************************************************************
 **
 **	DocumentDesktopWindowSpy
 **
 **	A base helper class to inherit from if one wants "spy"
 **	on the currently active window.
 **
 ***********************************************************************************/

class DocumentDesktopWindowSpy : public DesktopWindowListener, public DocumentWindowListener
{
	public:

								DocumentDesktopWindowSpy();
		virtual					~DocumentDesktopWindowSpy();

		void					UpdateTargetDocumentDesktopWindow();

		void					SetSpyInputContext(OpInputContext* input_context, BOOL send_change = TRUE);
		OpInputContext*			GetSpyInputContext() {return m_input_context;}

		void					SetTargetDocumentDesktopWindow(DocumentDesktopWindow* document_desktop_window, BOOL send_change = TRUE);
		DocumentDesktopWindow*	GetTargetDocumentDesktopWindow() {return m_document_desktop_window;}

		// Overload this hook to know when a new target is set, which may be NULL (= no target)

		virtual void			OnTargetDocumentDesktopWindowChanged(DocumentDesktopWindow* target_window) {}

		// Implementing the DocumentDesktopWindow::DocumentWindowListener ======================

#ifdef EMBROWSER_SUPPORT
		virtual void			OnPageLoadingCreated(OpWindowCommander* commander){};
		virtual void			OnStartUploading(DocumentDesktopWindow* document_window) {}
        virtual void			OnPageUploadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status){};
#endif
		// DesktopWindowListener
		virtual void			OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);

	private:

		class SpyInputStateListener : public OpInputStateListener
		{
			public:

				SpyInputStateListener(DocumentDesktopWindowSpy* spy) : m_spy(spy)
				{
					m_input_state.SetInputStateListener(this);
					m_input_state.SetWantsOnlyFullUpdate(TRUE);
				}
				virtual ~SpyInputStateListener() { }

				void Enable(BOOL enable)
				{
					m_input_state.SetEnabled(enable);
				}

				// OpInputStateListener

				virtual void OnUpdateInputState(BOOL full_update)
				{
					m_spy->UpdateTargetDocumentDesktopWindow();
				}

			private:

				DocumentDesktopWindowSpy*	m_spy;
				OpInputState				m_input_state;
		};

		SpyInputStateListener	m_input_state_listener;
		DocumentDesktopWindow*	m_document_desktop_window;
		OpInputContext*			m_input_context;
};

/***********************************************************************************
 **
 **	CycleAccessKeysPopupWindow - Used for cycling between the access keys defined on a page
 **
 ***********************************************************************************/

class CycleAccessKeysPopupWindow : public DesktopWindow
{
public:

	CycleAccessKeysPopupWindow(DocumentDesktopWindow* browser_window);
	~CycleAccessKeysPopupWindow();

	OP_STATUS 		Init();

	OP_STATUS		AddAccessKey(const uni_char *title, const uni_char *url, const uni_char key);
	void			ClearAccessKeys();
	virtual BOOL	OnInputAction(OpInputAction* action);
	void			UpdateDisplay();

	// OpTypedObject

	virtual Type			GetType() {return WINDOW_TYPE_ACCESSKEY_CYCLER;}
	virtual const char*		GetWindowName() {return "Accesskeys Cycler Window";}
	virtual const char*     GetInputContextName() {return "Accesskeys Cycler Window";}

private:
	class CycleAccessKeyItem
	{
	public:
		CycleAccessKeyItem()
		{
			key = 0;
		}

		OpString url;
		OpString title;
		uni_char key;
		OpString display_name;
	};

	OpToolbar*				m_keys_list;
	DocumentDesktopWindow*	m_document_window;
	BrowserDesktopWindow*	m_browser_window;
};

// Will escape needed chars if needed, if it's a local file and it's not a full url containing ://
OP_STATUS EscapeURLIfNeeded(OpString& address);

#endif // DOCUMENT_DESKTOP_WINDOW_H
