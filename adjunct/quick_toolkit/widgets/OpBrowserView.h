/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OP_BROWSER_VIEW_H
# define OP_BROWSER_VIEW_H

# include "adjunct/desktop_pi/desktop_file_chooser.h"
# include "adjunct/quick_toolkit/widgets/OpPageListener.h"
# include "adjunct/quick_toolkit/widgets/DialogListener.h"
# include "adjunct/quick/widgets/DocumentView.h"

# include "modules/widgets/OpWidget.h"
# include "modules/windowcommander/OpWindowCommander.h"

# ifdef DESKTOP_UTIL_SEARCH_ENGINES
#  include "adjunct/desktop_util/search/search_net.h"
# endif // DESKTOP_UTIL_SEARCH_ENGINES

class OpPage;
class OpWindow;
class DesktopOpWindow;
class ThumbnailGenerator;
class PrintProgressDialog;

/** @brief A widget that displays a web page
  * This widget can display an OpPage on screen and implements UI hooks needed
  * for pages that are on the screen.
  */
class OpBrowserView 
	:public OpWidget
	,public DocumentView
	,public OpPageAdvancedListener
	,public DialogListener
#ifdef _PRINT_SUPPORT_
# ifdef DESKTOP_PRINTDIALOG
	,public DesktopPrintDialogListener
# endif
	,public OpPrintingListener
#endif // _PRINT_SUPPORT_
#if defined (WIC_CAP_UPLOAD_FILE_CALLBACK) || defined (WIC_FILESELECTION_LISTENER)
	,public DesktopFileChooserListener
#endif // WIC_CAP_UPLOAD_FILE_CALLBACK
#ifdef WIC_FILESELECTION_LISTENER
	,public OpFileSelectionListener
#endif // WIC_FILESELECTION_LISTENER
#ifdef WIC_COLORSELECTION_LISTENER
	,public OpColorSelectionListener
#endif // WIC_COLORSELECTION_LISTENER
{
public:

	static OP_STATUS Construct(OpBrowserView** obj, BOOL border = TRUE, OpWindowCommander* window_commander = NULL);

	OpBrowserView(BOOL border = TRUE, OpWindowCommander* window_commander = NULL);

	virtual ~OpBrowserView();

	virtual OP_STATUS Init();

	/**
	 * Set the page to be displayed.
	 *
	 * The page currently displayed is deleted first.
	 * @note No need to call it if all you need is an OpBrowserView with
	 * a self-configured OpPage.
	 *
	 * @param page the new page.  Ownership is transferred.
	 * @return status
	 */
	OP_STATUS SetPage(OpPage* page);


	/**
	 * @return
	 */
	DEPRECATED(Window* GetWindow());

	OP_STATUS AddListener(OpPageListener* listener);
	OP_STATUS RemoveListener(OpPageListener* listener);
	
	OP_STATUS AddAdvancedListener(OpPageAdvancedListener* listener);
	OP_STATUS RemoveAdvancedListener(OpPageAdvancedListener* listener);
	
	/**
	 * @param charset
	 * @return
	 */
	DEPRECATED(OP_STATUS SetEncoding(const uni_char* charset));

	/**
	 * @param suppress_dialogs
	 */
	void SetSuppressDialogs(BOOL suppress_dialogs)
		{ m_suppress_dialogs = suppress_dialogs; }

	/**
	 * @param suppress
	 */
	void SetSuppressFocusEvents(BOOL suppress)
		{ m_suppress_focus_events = suppress; }

	/**
	 * @param reason
	 */
	void FocusDocument(FOCUS_REASON reason);

	/**
	 * @param embedded
	 */
	void SetEmbeddedInDialog(BOOL embedded)
		{ m_embedded_in_dialog = embedded; }

	/**
	 * Ask to dim the page, or cancel a previous request.
	 *
	 * The page will be dimmed as long as there is at least one uncanceled
	 * dim request.
	 *
	 * @param dimmed whether the request is to dim or undim the page
	 * @return status
	 */
	OP_STATUS RequestDimmed(bool dimmed);

	// Use these helpers for doing direct html generation to this browser view
	// Perform WriteDocumentData on the URL object returned, then call EndWrite when done

	/**
	 * @return
	 */
	URL	BeginWrite();

	/**
	 * @param url
	 */
	void EndWrite(URL& url);

	/**
	 * @return
	 */
	const OpStringC& GetSearchGUID()
		{ return m_search_guid; }

	/**
	 * @param guid
	 */
	void SetSearchGUID(const OpStringC& guid)
		{ m_search_guid.Set(guid); }

	/**
	 * @return
	 */
	DesktopOpWindow* GetOpWindow()
		{ return m_op_window; }

	/**
	 * @param supports
	 */
	void SetSupportsNavigation(BOOL supports);

	/**
	 * @param supports
	 */
	void SetSupportsSmallScreen(BOOL supports);

	/**
	 * @param action
	 * @return
	 */
	BOOL OnInputAction(OpInputAction* action);

	/**
	 * @param hidden
	 */
	void SetHidden(BOOL hidden);

	/** Get the text to put in the security button
	 * @param button_text
	 * @return
	 */
	OP_STATUS GetCachedSecurityButtonText(OpString &button_text);

	/**
	 * @param width
	 * @param height
	 */
	void GetSizeFromBrowserViewSize(INT32& width, INT32& height);

	/**
	 */
	void SaveDocumentIcon();

	/**
	 */
	void UpdateWindowImage(BOOL force);

	/**
	 * @param action
	 * @param opener
	 * @return
	 */
	BOOL ContinuePopup(OpDocumentListener::JSPopupCallback::Action action = OpDocumentListener::JSPopupCallback::POPUP_ACTION_DEFAULT, OpDocumentListener::JSPopupOpener **opener = NULL);

	/** Create a thumbnail of this browser view
	  * @param width Width of the thumbnail to generate
	  * @param height Height of the thumbnail to generate
	  * @param high_quality Whether the thumbnail should be created at high quality
	  * @param logo_start If found, gives the start of a logo in this browserview
	  */
	virtual Image GetThumbnailImage(long width, long height, BOOL high_quality);
	virtual Image GetThumbnailImage(long width, long height, BOOL high_quality, OpPoint& logo_start);
	virtual Image GetSnapshotImage();
	ThumbnailGenerator* CreateThumbnailGenerator(long width, long height, BOOL high_quality);

	virtual void SetPreferedSize(INT32 w, INT32 h) { m_prefered_width = w; m_prefered_height = h; }
	virtual void GetPreferedSize(INT32* w, INT32* h) { *w = m_prefered_width; *h = m_prefered_height; }

#if defined (WIC_CAP_UPLOAD_FILE_CALLBACK)  || defined (WIC_FILESELECTION_LISTENER)
	// DesktopFileChooserListener API
	void OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result);
#endif // WIC_CAP_UPLOAD_FILE_CALLBACK || WIC_FILESELECTION_LISTENER

# if defined _PRINT_SUPPORT_ && defined DESKTOP_PRINTDIALOG
	void DesktopPrintDialogDone() { m_desktop_printdialog = NULL; }
# endif

# if defined QUICK_TOOLKIT_PRINT_PROGRESS_DIALOG
	void PrintProgressDialogDone() { m_print_progress_dialog = NULL; }
# endif

	void SetOwnAuthDialog(BOOL auth) { m_own_auth = auth; }
	void SetConsumeScrolls(BOOL consume) { m_consume_scrolls = consume; }

	OpPage* GetOpPage() { return m_page; }

	// == DialogListener =============
	virtual void OnOk(Dialog*, UINT32) {}
	virtual void OnCancel(Dialog*) {}
	virtual void OnYNCCancel(Dialog*) {}
	virtual void OnClose(Dialog* dialog);

	// DocumentView
	virtual OP_STATUS Create(OpWidget* container);
	virtual void SetFocus(FOCUS_REASON reason) { OpWidget::SetFocus(reason); }
	virtual void Show(bool visible) { SetVisibility(visible); }
	virtual bool IsVisible() { return OpWidget::IsVisible() != FALSE; }
	virtual OpWidget* GetOpWidget() { return this; }
	virtual void SetRect(OpRect rect) { OpWidget::SetRect(rect); }
	virtual void Layout() { }
	virtual OpWindowCommander* GetWindowCommander();
	virtual DocumentViewTypes GetDocumentType() { return DOCUMENT_TYPE_BROWSER; }
	virtual OP_STATUS GetTitle(OpString& title);
	virtual void GetThumbnailImage(Image& image) { image = GetThumbnailImage(THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, TRUE); }
	virtual bool HasFixedThumbnailImage() { return false; }
	virtual OP_STATUS GetTooltipText(OpInfoText& text);
	virtual const OpWidgetImage* GetFavIcon(BOOL force);
	virtual double GetWindowScale();
	virtual void QueryZoomSettings(double &min, double &max, double &step, const OpSlider::TICK_VALUE* &tick_values, UINT8 &num_tick_values) { }

protected:

	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows) { *w = m_prefered_width; *h = m_prefered_height; }

	/** Set current visibility of browser view
	 * @param hidden Whether this browser view is completely hidden from view (ie.
	 *               not visible on the screen)
	 */
	void SelectedTextSearch(const OpStringC& guid);

	// == Hooks ======================

	
	void OnAdded();
	void OnDeleted();
	void OnLayout();
	void OnMove() {Relayout();}
	void OnShow(BOOL show);
	void OnFocus(BOOL focus,FOCUS_REASON reason);
	BOOL OnMouseWheel(INT32 delta,BOOL vertical);

	virtual const char*	GetInputContextName() {return "Browser Widget";}

	virtual void OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
	virtual void OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);

	// Implementing the OpTreeModelItem interface

	virtual Type GetType() {return WIDGET_TYPE_BROWSERVIEW;}

	// == OpToolTipListener ======================

	virtual BOOL HasToolTipText(OpToolTip* tooltip) {return m_hover_url.HasContent() || m_hover_title.HasContent();}
	virtual void GetToolTipText(OpToolTip* tooltip, OpInfoText& text);
	virtual void GetToolTipThumbnailText(OpToolTip* tooltip, OpString& title, OpString& url_string, URL& url);

	// == OpPageListener ======================

	virtual void OnPageDestroyed(OpPage* page);

	// == OpPageListener(OpLoadingListener) ======================

	virtual void OnPageUrlChanged(OpWindowCommander* commander, const uni_char* url);
	virtual void OnPageStartLoading(OpWindowCommander* commander);
	virtual void OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL was_stopped_by_user);
	virtual void OnPageAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback);
#if defined SUPPORT_DATABASE_INTERNAL
	virtual void OnPageIncreaseQuota(OpWindowCommander* commander, const uni_char* db_name, const uni_char* db_domain, const uni_char* db_type, OpFileLength current_quota_size, OpFileLength quota_hint, OpDocumentListener::QuotaCallback *callback);
#endif // SUPPORT_DATABASE_INTERNAL

	// == OpPageListener(OpDocumentListener) ======================

	virtual void OnPageHover(OpWindowCommander* commander, const uni_char* url, const uni_char* link_title, BOOL is_image);
	virtual void OnPageNoHover(OpWindowCommander* commander);
	virtual void OnPageSearchReset(OpWindowCommander* commander);
	virtual void OnPageSearchHit(OpWindowCommander* commander);
	virtual void OnPageDrag(OpWindowCommander* commander, const OpRect rect, const uni_char* link_url, const uni_char* link_title, const uni_char* image_url);
	virtual void OnPageGetInnerSize(OpWindowCommander* commander, UINT32* width, UINT32* height);

#ifdef DND_DRAGOBJECT_SUPPORT
	virtual void OnDragLeave(OpWindowCommander* commander, class DesktopDragObject* drag_object);
	virtual void OnDragMove(OpWindowCommander* commander, class DesktopDragObject* drag_object, const OpPoint& point);
	virtual void OnDragDrop(OpWindowCommander* commander, class DesktopDragObject* drag_object, const OpPoint& point);
#endif // DND_DRAGOBJECT_SUPPORT
	virtual void OnFileDropConfirm(OpWindowCommander* commander, const uni_char *server_name, OpDocumentListener::DialogCallback *callback);

	virtual void OnPageStatusText(OpWindowCommander* commander, const uni_char* text);
	virtual void OnPageConfirm(OpWindowCommander* commander, const uni_char *message, OpDocumentListener::DialogCallback *callback);
	virtual void OnPageQueryGoOnline(OpWindowCommander* commander, const uni_char *message, OpDocumentListener::DialogCallback *callback);

	virtual void OnPageJSAlert(OpWindowCommander* commander,	const uni_char *hostname, const uni_char *message, OpDocumentListener::JSDialogCallback *callback);
	virtual void OnPageJSConfirm(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, OpDocumentListener::JSDialogCallback *callback);
	virtual void OnPageJSPrompt(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, const uni_char *default_value, OpDocumentListener::JSDialogCallback *callback);

# if defined WIC_CAP_JAVASCRIPT_PRINT && defined _PRINT_SUPPORT_
	virtual void OnPageJSPrint(OpWindowCommander * commander, PrintCallback * callback);
# endif // WIC_CAP_JAVASCRIPT_PRINT && _PRINT_SUPPORT_

#if defined WIC_CAP_JAVASCRIPT_PRINT && defined _PRINT_SUPPORT_
	virtual void OnPageUserPrint(OpWindowCommander * commander, PrintCallback * callback);
#endif // WIC_CAP_JAVASCRIPT_PRINT && _PRINT_SUPPORT_

#if defined (DOM_GADGET_FILE_API_SUPPORT) && defined (WIC_CAP_HAS_MOUNTPOINT_API)
	virtual void OnPageMountpointFolderRequest(OpWindowCommander* commander, MountPointFolderCallback* callback, MountPointFolderCallback::FolderType folder_type);
	virtual void OnPageMountpointFolderCancel(OpWindowCommander* commander, MountPointFolderCallback* callback);
#endif // DOM_GADGET_FILE_API_SUPPORT && WIC_CAP_HAS_MOUNTPOINT_API

	virtual void OnPageFormSubmit(OpWindowCommander* commander, OpDocumentListener::FormSubmitCallback *callback);
	virtual void OnPagePluginPost(OpWindowCommander* commander, OpDocumentListener::PluginPostCallback *callback);
	virtual void OnPageDownloadRequest(OpWindowCommander* commander, OpDocumentListener::DownloadCallback * callback);
	virtual void OnPageSubscribeFeed(OpWindowCommander* commander, const uni_char* url);
	virtual void OnPageAskAboutUrlWithUserName(OpWindowCommander* commander, const uni_char* url, const uni_char* hostname, const uni_char* username, OpDocumentListener::DialogCallback* callback);
	virtual void OnPageAskAboutFormRedirect(OpWindowCommander* commander, const uni_char* source_url, const uni_char* target_url, OpDocumentListener::DialogCallback* callback);
	virtual void OnPageFormRequestTimeout(OpWindowCommander* commander, const uni_char* url);
	virtual BOOL OnPageAnchorSpecial(OpWindowCommander * commander, const OpDocumentListener::AnchorSpecialInfo & info);
	virtual void OnPageAskPluginDownload(OpWindowCommander * commander);
	virtual void OnPageVisibleRectChanged(OpWindowCommander * commander, const OpRect& visible_rect);
	virtual void OnPageMailTo(OpWindowCommander * commander, const OpStringC8& raw_mailto, BOOL mailto_from_dde, BOOL mailto_from_form, const OpStringC8& servername);
	virtual void OnPageSetupInstall(OpWindowCommander* commander, const uni_char* url, URLContentType content_type);

#ifdef M2_SUPPORT
	virtual void OnPageExternalEmbedBlocked(OpWindowCommander* commander) {}
#endif // M2_SUPPORT
	// == OpPrintingListener ==================

#ifdef _PRINT_SUPPORT_
	virtual void OnStartPrinting(OpWindowCommander* commander);

	virtual void OnPrintStatus(OpWindowCommander* commander, const PrintInfo* info);
#endif // _PRINT_SUPPORT_

	// == OpFileSelectionListener =============

#ifdef WIC_FILESELECTION_LISTENER
	virtual BOOL OnRequestPermission(OpWindowCommander* commander);
# ifdef _FILE_UPLOAD_SUPPORT_
	virtual void OnUploadFilesRequest(OpWindowCommander* commander, UploadCallback* callback);
	virtual void OnUploadFilesCancel(OpWindowCommander* commander);
# endif // _FILE_UPLOAD_SUPPORT_
# ifdef DOM_GADGET_FILE_API_SUPPORT
	virtual void OnDomFilesystemFilesRequest(OpWindowCommander* commander, DomFilesystemCallback* callback);
	virtual void OnDomFilesystemFilesCancel(OpWindowCommander* commander);
# endif // DOM_GADGET_FILE_API_SUPPORT
#endif // WIC_FILESELECTION_LISTENER

	// == MessageObject =============
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	// == OpColorSelectionListener ============

#ifdef WIC_COLORSELECTION_LISTENER
	virtual void OnColorSelectionRequest(OpWindowCommander* commander, ColorSelectionCallback* callback);
	virtual void OnColorSelectionCancel(OpWindowCommander* commander);
#endif // WIC_COLORSELECTION_LISTENER

	// == OpFileSelectionListener =============

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	virtual int	GetAccessibleChildrenCount();
	virtual OpAccessibleItem* GetAccessibleChild(int);
	virtual int GetAccessibleChildIndex(OpAccessibleItem* child);
	virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
	virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

private:
	class DimmerAnimation;
	class SearchDimmerAnimation;

	OP_STATUS AttachToPage(OpPage& page);
	BOOL HandleHotclickActions(OpInputAction* action);
	BOOL IsActionSupported(OpInputAction* action);
#ifdef _BITTORRENT_SUPPORT_
	BOOL IsBittorrentDownload(OpDocumentListener::DownloadCallback* download_callback);
#endif // _BITTORRENT_SUPPORT_

	// get the clipboard token that should be used for copy operation
	// in case this is a private window
	UINT32 GetClipboardToken();

	OP_STATUS Dim(DimmerAnimation*& dim_window);
	void Undim(DimmerAnimation*& dim_window);
	void ShowConfirmDialog(DesktopWindow* parent_window, const uni_char* message, OpDocumentListener::DialogCallback* callback);

	void TryCloseLastPostDialog();
	void OpenNewOnFormSubmitDialog(OpDocumentListener::FormSubmitCallback * callback);
	void OpenNewOnPluginPostDialog(OpDocumentListener::PluginPostCallback * callback);

private:
	DesktopOpWindow*		m_op_window;
	OpPage*					m_page;
	BOOL					m_border;
	BOOL					m_own_auth;

	DimmerAnimation*		m_dim_window;
	int						m_dim_count;
	DimmerAnimation*		m_search_dim_window;

	OpString				m_hover_title;
	OpString				m_hover_url;

	BOOL					m_suppress_dialogs;			// used when generating thumbnails
	BOOL					m_suppress_focus_events;	// used when generating thumbnails to suppress javascript invoked focus events
	BOOL					m_embedded_in_dialog;		// set to indicate that the OpPageView is in a dialog
	BOOL					m_hidden;					// set to indicate that this OpPageView is completely hidden (not visible on screen)
	BOOL					m_consume_scrolls;
	OpString				m_search_guid;

	INT32					m_prefered_width;
	INT32					m_prefered_height;
	Dialog*					m_last_post_dialog;
# if defined _PRINT_SUPPORT_ && defined DESKTOP_PRINTDIALOG
	DesktopPrintDialog* m_desktop_printdialog;
# endif
# if defined QUICK_TOOLKIT_PRINT_PROGRESS_DIALOG
	class PrintProgressDialog* m_print_progress_dialog;
# endif
#if defined (WIC_CAP_UPLOAD_FILE_CALLBACK) || defined (WIC_FILESELECTION_LISTENER)
	DesktopFileChooserRequest	m_request;
	DesktopFileChooser*			m_chooser;
# if defined (WIC_CAP_UPLOAD_FILE_CALLBACK)
	UploadCallback*				m_upload_callback;
# elif defined (WIC_FILESELECTION_LISTENER)
	FileSelectionCallback*		m_selection_callback;
# endif // WIC_FILESELECTION_LISTENER
#endif // WIC_CAP_UPLOAD_FILE_CALLBACK || WIC_FILESELECTION_LISTENER
#if defined SUPPORT_DATABASE_INTERNAL
	OpDocumentListener::QuotaCallback*	m_quota_callback;
#endif // SUPPORT_DATABASE_INTERNAL
#ifdef WIC_COLORSELECTION_LISTENER
	ColorSelectionCallback* m_color_selection_callback;
#endif
};

#endif // OP_BROWSER_VIEW_H
