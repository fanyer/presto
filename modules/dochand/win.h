/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DOCHAND_WIN_H
#define DOCHAND_WIN_H

#include "modules/display/cursor.h"
#include "modules/doc/css_mode.h"
#include "modules/doc/doctypes.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/url/url2.h"
#include "modules/util/simset.h"
#include "modules/dochand/winman_constants.h"
#include "modules/hardcore/cpuusagetracker/cpuusagetracker.h"
#include "modules/windowcommander/OpWindowCommander.h" // For LoadingInformation
#include "modules/util/OpHashTable.h"
#include "modules/dochand/documentreferrer.h"
#ifdef SUPPORT_VISUAL_ADBLOCK
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#endif // SUPPORT_VISUAL_ADBLOCK

#ifdef _AUTO_WIN_RELOAD_SUPPORT_
# include "modules/hardcore/timer/optimer.h"
#endif // _AUTO_WIN_RELOAD_SUPPORT_

#ifdef USE_OP_CLIPBOARD
# include "modules/dragdrop/clipboard_manager.h"
#endif // USE_OP_CLIPBOARD

class CSS;
class DocumentManager;
class HTML_Document;
class Image;
class LinkElement;
class LinkElementDatabase;
class OpSnHandler;
class OpWindow;
class OpWindowCommander;
class PrinterInfo;
class VisualDevice;
class WindowCommander;
class WindowListener;
class ProgressLine;
class DocListElm;
class DocumentReferrer;
class DocumentOrigin;
class OpGadget;
class SearchData;
class CSSCollection;
class ViewportController;
#ifdef SVG_SUPPORT
class SVGImageRef;
#endif // SVG_SUPPORT
#ifdef SCOPE_PROFILER
class OpProfilingSession;
#endif // SCOPE_PROFILER

#ifdef ABSTRACT_CERTIFICATES
# include "modules/pi/network/OpCertificate.h"
#endif

#if defined _SPAT_NAV_SUPPORT_ && defined SN_LEAVE_SUPPORT
class SnLeaveListener
{
public:
	virtual ~SnLeaveListener() {};
	virtual void LeaveInDirection(int direction, int x, int y, class OpView *view) = 0;
	virtual void LeaveInDirection(int direction) = 0;
};
#endif // _SPAT_NAV_SUPPORT_ && SN_LEAVE_SUPPORT

#define MAX_PROGRESS_MESS_LENGTH	200

#ifdef _AUTO_WIN_RELOAD_SUPPORT_

//	user wants to reload the window automatically
class AutoWinReloadParam : public OpTimerListener
{
public:
						AutoWinReloadParam	(Window* win);
						~AutoWinReloadParam ();
	BOOL				OnSecTimer			( );	//	Returns TRUE if we should reload
	void				Enable				( BOOL fEnable);
	void				Toggle				( );
	void				SetSecUserSetting	( int secUserSetting);
	void				SetParams			( BOOL fEnable, int secUserSetting, BOOL fOnlyIfExpired, BOOL fUpdatePrefsManager=TRUE);
	int					GetSecRemaining		( ) const { return m_secRemaining; }
	BOOL				GetOptIsEnabled		( )	const { return m_fEnabled; }
	int					GetOptSecUserSetting( ) const { return m_secUserSetting; }
	BOOL				GetOptOnlyIfExpired	( ) const { return m_fOnlyIfExpired; }

	virtual void OnTimeOut(OpTimer* timer);
private:
	BOOL				m_fEnabled;
	int					m_secUserSetting;			//	sec. between each reload
	int					m_secRemaining;				//	sec remaining to next reload
	BOOL				m_fOnlyIfExpired;			//	only reload if "expired/changed"
	OpTimer* m_timer;
	Window* m_win;

};

#endif // _AUTO_WIN_RELOAD_SUPPORT_

#ifdef DOM_JIL_API_SUPPORT
/**
 * An ScreenPropsChangedListener listens to any change of screen properties
 * that current window is displayed on.
 */
class ScreenPropsChangedListener
{
public:
	/**
	 * Deletes the ScreenPropsChangedListener.
	 */
	virtual ~ScreenPropsChangedListener() {}

	/**
	 * Called when a screen properties have changes
	 */
	virtual void OnScreenPropsChanged() = 0;
};

#endif //DOM_JIL_API_SUPPORT

class OpenUrlInNewWindowInfo
{
public:

	URL					new_url;
	DocumentReferrer	ref_url;
	uni_char*			window_name;
	BOOL				open_in_background_window;
	BOOL				user_initiated;
	unsigned long		opener_id;
	int					opener_sub_win_id;
	BOOL				open_in_page;

						OpenUrlInNewWindowInfo(URL& url,
											   DocumentReferrer refurl,
											   const uni_char* win_name,
											   BOOL open_in_bg_win,
											   BOOL user_init,
											   unsigned long open_win_id,
											   int open_sub_win_id,
											   BOOL open_in_page = FALSE);
						~OpenUrlInNewWindowInfo() { delete [] window_name; }
};


class Window
	: public Link,
	  public MessageObject
#if defined (SHORTCUT_ICON_SUPPORT) && defined (ASYNC_IMAGE_DECODERS)
  , public ImageListener
#endif
  , public OpDocumentListener::DialogCallback
#ifdef USE_OP_CLIPBOARD
  , public ClipboardListener
#endif // USE_OP_CLIPBOARD
{
public:
	void				SetFeatures(Window_Type type);
	BOOL				HasFeature(Window_Feature feature);
	BOOL				HasAllFeatures(Window_Feature feature);
	void				SetCanClose(BOOL bState) { bCanSelfClose = bState; }
	BOOL				CanClose() const { return bCanSelfClose; }
	void				SetCanBeClosedByScript(BOOL flag) { can_be_closed_by_script = flag; }

#ifdef SELFTEST
	/**
	 * Signals to the selftest system that the window is about to close and so
	 * shouldn't be (re)used.
	 */
	void				SetIsAboutToClose(BOOL is_about_to_close)
	{
		m_is_about_to_close = is_about_to_close;
	}

	/**
	 * Returns the state set by SetIsAboutToClose().
	 */
	BOOL				IsAboutToClose() const { return m_is_about_to_close; }
#endif // SELFTEST

	OpWindowCommander::FullScreenState GetFullScreenState() const { return m_fullscreen_state; }

	/**
	 * @returns ERR_NO_MEMORY on OOM, ERR_NOT_SUPPORTED if the fullscreen
	 *			state was not allowed, otherwise OK.
	 */
	OP_STATUS                          SetFullScreenState(OpWindowCommander::FullScreenState state);
	BOOL                               IsFullScreen() const { return m_fullscreen_state != OpWindowCommander::FULLSCREEN_NONE; }

	OpWindow*			GetOpWindow() { return m_opWindow; }

	/**
	 * Returns WindowCommander associated with this window.
	 * This never returns NULL.
	 */
	WindowCommander* GetWindowCommander() const { return m_windowCommander; }

	enum OnlineMode
	{
		ONLINE,
		OFFLINE,
		USER_RESPONDING,
		DENIED
	};
#ifdef SUPPORT_VISUAL_ADBLOCK
	// used when in content blocker mode to send mouse clicks to a listener instead of processing them
	void SetContentBlockEditMode(BOOL edit_mode) { m_content_block_edit_mode = edit_mode; }
	BOOL GetContentBlockEditMode() { return m_content_block_edit_mode; }
	// is the content blocker enabled in prefs?
	BOOL IsContentBlockerEnabled()
	{
		if (m_content_block_server_name)
			return m_content_block_enabled;
		else
			return m_content_block_enabled && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableContentBlocker, GetCurrentShownURL().GetServerName()) != 0;
	}
	// Used when in content blocker mode to be able to load otherwise blocked content
	void SetContentBlockEnabled(BOOL enable) { m_content_block_enabled = enable; }
	void SetContentBlockServerName(ServerName *server_name)
	{
		m_content_block_server_name = server_name;
		m_content_block_enabled = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableContentBlocker, m_content_block_server_name) != 0;
	}
#endif // SUPPORT_VISUAL_ADBLOCK

#ifdef WAND_SUPPORT
	void				IncrementeWandInProgressCounter() { m_wand_in_progress_count++; } // yes, there is a typo in the name
	void				DecrementWandInProgressCounter() { OP_ASSERT(m_wand_in_progress_count > 0); m_wand_in_progress_count--; }
#endif // WAND_SUPPORT

	/* Sets the OOM occurred state.
	 *
	 * When the window is in the OOM occurred state and a new document
	 * is about to be loaded in it, the current document is unloaded
	 * first, before the new load is started. It increases the chances
	 * of a successful load that won't run into OOM immediately, at
	 * the risk of ending up with a blank page if the load fails for
	 * some reason.
	 *
	 * @param oom TRUE if an OOM has occurred, FALSE to clear.
	 */
	void SetOOMOccurred(BOOL oom) { m_oom_occurred = oom; }

	/** Gets the OOM occurred state.
	 *
	 * @return TRUE if OOM has occurred, FALSE otherwise.
	 */
	BOOL GetOOMOccurred() { return m_oom_occurred; }

private:
	void 				LowLevelSetSecurityState(BYTE sec, BOOL is_inline, const uni_char *name, URL *url= NULL);

#ifdef SVG_SUPPORT

	/**
	 * Set this SVGImage to be the favicon of the window.
	 */
	OP_STATUS			SetSVGAsWindowIcon(SVGImageRef* ref);
public:
	/**
	 * Removes this window's favicon if 'ref' is the window icon.
	 * Note: ref can be deleted.
	 */
	void				RemoveSVGWindowIcon(SVGImageRef* ref);
private:
#endif // SVG_SUPPORT

	void				SetFeature(Window_Feature feature);
	void				RemoveFeature(Window_Feature feature);
	void				SetId(unsigned long nid) {this->id = nid;}

	unsigned long		m_features;

	OnlineMode			m_online_mode;

	INT32				m_frames_policy;

#ifdef SUPPORT_VISUAL_ADBLOCK
	BOOL				m_content_block_edit_mode;
	BOOL				m_content_block_enabled;
	ServerName*			m_content_block_server_name;
#endif // SUPPORT_VISUAL_ADBLOCK

	WindowCommander*	m_windowCommander;
	LoadingInformation	m_loading_information;

	VisualDevice*		vis_device;					// visual device of the window
	DocumentManager*	doc_manager;				// windows document manager
	MessageHandler*		msg_handler;				// windows message handler (handles messages between parts of the code)
	uni_char*			current_message;			// message shown in the status bar
	uni_char*			current_default_message;	// message shown in the status bar (see JS window.defaultStatus)
	ProgressState		progress_state;				// message shown in the status bar
	WinState			state;						// busy, clickable or not busy
#ifndef MOUSELESS
  	CursorType			current_cursor;				// current mouse cursor shape
  	CursorType			m_pending_cursor;			// used to track cursor values derived from elements as we traverse the document tree towards the target
	CursorType			cursor_set_by_doc;			// mouse cursor last set by document, only relevant if has_cursor_set_by_doc is TRUE
	BOOL				has_cursor_set_by_doc;		// TRUE if doc wants another cursor than the default. See cursor_set_by_doc.
#endif // !MOUSELESS
#ifdef LIBOPERA
	int                 cursor_content;              // The content types associated with the current cursor.
#endif // LIBOPERA
	URL					active_link_url;			// URL currently under the mouse pointer

	OpWindowCommander::FullScreenState m_fullscreen_state;
	OpWindowCommander::FullScreenState m_previous_fullscreen_state; // To workaround printing problems. See Window::StartPrinting().

	BOOL				m_is_background_transparent;

	BOOL				m_is_explicit_suppress_window, m_is_implicit_suppress_window;
	BOOL				m_is_scriptable_window;
	BOOL				m_is_visible_on_screen; // See setter/getter. Def TRUE

#ifdef _SPAT_NAV_SUPPORT_
	OpSnHandler*          sn_handler;                 // Used by spatial navigation
#ifdef SN_LEAVE_SUPPORT
	SnLeaveListener *   sn_listener;                // Listener called when navigating away from the opera window.
#endif // SN_LEAVE_SUPPORT
#endif /* _SPAT_NAV_SUPPORT */

	BOOL                m_draw_highlight_rects;

	BOOL						bCanSelfClose;

#ifdef SELFTEST
	BOOL						m_is_about_to_close;
#endif // SELFTEST

	BOOL						m_url_came_from_address_field;

#if defined REDIRECT_ON_ERROR || defined AB_ERROR_SEARCH_FORM
	OpString					m_typed_in_url;
#endif // defined REDIRECT_ON_ERROR || defined AB_ERROR_SEARCH_FORM

	BOOL				show_img;
#ifdef FIT_LARGE_IMAGES_TO_WINDOW_WIDTH
	BOOL				fit_img;
#endif // FIT_LARGE_IMAGES_TO_WINDOW_WIDTH
	BOOL				load_img;

	BOOL				is_ssr_mode;
	BOOL				era_mode;

	LayoutMode			layout_mode;

	/* @see SetLimitParagraphWidth */
	BOOL				limit_paragraph_width;

	int					flex_root_max_width;

	WORD				scale;

	int					m_text_scale;

	long				progress_count;
	OpFileLength		doc_progress_count;
	OpFileLength		upload_total_bytes;
	int					upload_file_count;
	int					upload_file_loaded_count;
	BOOL				phase_uploading;
	time_t				start_time;
	uni_char			progress_mess[MAX_PROGRESS_MESS_LENGTH + 1]; /* ARRAY OK 2008-02-28 jl */
	uni_char			progress_mess_low[MAX_PROGRESS_MESS_LENGTH + 1]; /* ARRAY OK 2008-02-28 jl */

	unsigned long		OutputAssociatedWindow;

	BOOL				bShowScrollbars;

	unsigned long		id;

	BOOL				loading;
	BOOL				end_progress_message_sent;
	BOOL				history_cleanup_message_sent;
	BOOL				user_load_from_cache;
	BOOL				pending_unlock_all_painting; /**< Whether we have a message pending that will trigger paint unlock. */

	OpStringS			name;

#ifdef CPUUSAGETRACKING
	CPUUsageTracker		cpu_usage_tracker;
#endif // CPUUSAGETRACKING

	CSSMODE	    		CSSMode;
	BOOL				forceColors;

	BOOL				truezoom;
	UINT32				truezoom_base_scale;

	BOOL				is_something_hovered;
	/**< TRUE if the most recent call to either OpDocumentListener::OnHover
	     or OpDocumentListener::OnNoHover was to OnHover, and FALSE otherwise. */

	BOOL				ecmascript_disabled;

	enum
	{
		NOT_PAUSED = 0,
		PAUSED_PRINTING = 1,
		PAUSED_OTHER = 2
	};
	int ecmascript_paused;
	void ResumeEcmaScriptIfNotPaused();

	ViewportController*	viewportcontroller;

	OpStringS8			forceEncoding;

	BYTE				SecurityState;
	/**< Current security state. */
	BYTE				SecurityState_signalled;
	/**< Security state last signalled to UI. */
	BOOL				SecurityState_changed_by_inline;
	/**< The last time SecurityState was changed it was changed because of an
	     inline element. */
	BOOL				SecurityState_signal_delayed;
	/**< The security state has changed, but we've delayed signalling this to
	     the UI because we've not started loading a document yet. */
	OpStringS			SecurityTextName;

	BOOL				m_privacy_mode;
#ifdef TRUST_RATING
	TrustRating			m_trust_rating;
#endif
#ifdef WEB_TURBO_MODE
	BOOL				m_turbo_mode;
#endif // WEB_TURBO_MODE

	int					currWinHistoryLength;
	int					current_history_number;
	int					min_history_number;
	int					max_history_number;
	BOOL				check_history;

	BOOL				is_canceling_loading;

	BOOL				title_update_title_posted;
	OpString			title;
	BOOL				generated_title;

#ifdef SHORTCUT_ICON_SUPPORT
	class UrlImageContentProvider* shortcut_icon_provider;
	URL					m_shortcut_icon_url;
#endif // SHORTCUT_ICON_SUPPORT

	char*				lastUserName;
	char*				lastProxyUserName;
	OpString			homePage;

	Window_Type			type;

	COLORREF            default_background_color;

#ifdef _PRINT_SUPPORT_
	PrinterInfo*		printer_info;
	PrinterInfo*		preview_printer_info;
	BOOL				preview_mode;
	BOOL				print_mode;
	DM_PrintType		frames_print_type;

	/* For asynchronous printing */
	BOOL is_formatting_print_doc;
	BOOL is_printing;

#endif // _PRINT_SUPPORT_

#ifdef _AUTO_WIN_RELOAD_SUPPORT_
	AutoWinReloadParam	m_userAutoReload;
#endif // _AUTO_WIN_RELOAD_SUPPORT_

	// the window that opened us is specified here
	Window*				opener;
	int					opener_sub_win_id;
	// if it was opened by script this flag will be set
	BOOL				is_available_to_script;
	DocumentOrigin*		opener_origin;
	BOOL				can_be_closed_by_script;

#if defined SEARCH_MATCHES_ALL
	SearchData*			m_search_data;
#endif // SEARCH_MATCHES_ALL

	BOOL always_load_from_cache;

#ifdef LIBOPERA
	// Was the last request initiated by the user?
	//
	// I'm not 100% happy with this solution, but for LIBOPERA we want to
	// emit a noConnection signal if the network is down, except for requests
	// that are not initiated by the user. Maybe this information should be
	// attached to the Comm object somehow instead? (mstensho)
	BOOL                m_lastReqUserInitiated;
#endif // LIBOPERA

	OpWindow* m_opWindow;
	WindowListener *windowlistener;

	BOOL m_bWasInFullscreen;

#ifdef ACCESS_KEYS_SUPPORT
	BOOL in_accesskey_mode;
#endif // ACCESS_KEYS_SUPPORT

	void SetMessageInternal();

	unsigned			window_locked;
	BOOL				window_closed;

	OpINT32Set			already_requested_urls;
	BOOL				use_already_requested_urls;

	BOOL				forced_java_disabled;
	BOOL				forced_plugins_disabled;
	BOOL				document_word_wrap_forced;

#ifdef WAND_SUPPORT
	int					m_wand_in_progress_count;
#endif // WAND_SUPPORT

	AutoDeleteHead		moved_urls;

#ifdef HISTORY_SUPPORT
	BOOL				m_global_history_enabled;
#endif // HISTORY_SUPPORT

#ifdef NEARBY_ELEMENT_DETECTION
	class ElementExpander*
						element_expander;
#endif // NEARBY_ELEMENT_DETECTION


#ifdef GRAB_AND_SCROLL
	BOOL				scroll_is_pan_overridden;
	BOOL				scroll_is_pan;
#endif // GRAB_AND_SCROLL

	/**
	 * If you cancel a keydown event the corresponding keypress event should be
	 * automatically cancelled (preventDefaulted) but since we don't have
	 * enough control of events now we're not able to directly connect
	 * the keydown and keypress events.
	 * Instead, we'll remember a cancelled keydown here and if this
	 * int (keycode) matches that of a keypress, that keypress will
	 * be cancelled as well. Default value here is OP_KEY_INVALID which means
	 * "no currently cancelled keydowns". It should be set to OP_KEY_INVALID
	 * whenever we're reasonably sure there will not come any keypresses
	 * matching any recent keydowns.
	 *
	 * @see SetRecentlyCancelledKeyDown()
	 * @see HasRecentlyCancelledKeyDown()
	 */
	OpKey::Code		recently_cancelled_keydown;

	/**
	 * A top level document has the right to display one download
	 * dialog even if it was unsolicited. This flag keeps track
	 * of when it has done that.
	 */
	BOOL	has_shown_unsolicited_download_dialog;

#ifdef DOM_JIL_API_SUPPORT
	ScreenPropsChangedListener* m_screen_props_listener;
#endif // DOM_JIL_API_SUPPORT

	BOOL m_oom_occurred;

	/** The ID for the next DocListElm to be created. The ID from the
	 *  window's DocumentManager should always be used.
	 */
	int				m_next_doclistelm_id;

#ifdef SCOPE_PROFILER
	/**
	 * When profiling is enabled, this variable will point to the profiling
	 * session used to create OpProbeTimelines. The OpProbeTimelines will in
	 * turn record the profiling data for each document in the Window.
	 *
	 * The Window does *not* own the OpProfilingSession, and must not delete
	 * it.
	 */
	OpProfilingSession *m_profiling_session;
#endif // SCOPE_PROFILER

#ifdef KEYBOARD_SELECTION_SUPPORT
	/**
	 * A flag used for sending keyboard selection mode changed events
	 * to the ui. The mode can change between focusing different
	 * frames and when walking in history or reloading.
	 */
	BOOL m_current_keyboard_selection_mode;
#endif // KEYBOARD_SELECTION_SUPPORT

#ifdef USE_OP_CLIPBOARD
	void OnCopy(OpClipboard*);
	void OnCut(OpClipboard*);
	void OnPaste(OpClipboard*);
	void OnEnd();
#endif // USE_OP_CLIPBOARD

protected:

#if defined(_MACINTOSH_) && defined(_POPUP_MENU_SUPPORT_)
	void				TrackContextMenu_Macintosh		( unsigned idr_menu, POINT ptWhere);
	void				TrackLinkMenu_Macintosh			( );
#endif // defined(_MACINTOSH_) && defined(_POPUP_MENU_SUPPORT_)

	/**
	 * Call UpdateWindow() if ssr status of window and doc were not in sync
	 */
	void UpdateWindowIfLayoutChanged();

	/**
	 * Unlocks the VisualDevice paint lock for all documents in this Window.
	 */
	void UnlockPaintLock();

public:

	void ClearMessage();

						Window(unsigned long nid, OpWindow* opWindow, OpWindowCommander* opWindowCommander);
						~Window();

	void				ConstructL();
	OP_STATUS			Construct() { TRAPD(err,ConstructL()); return err; }

	BOOL				Close(); // return TRUE on success (if a window contains edits, the user can cancel the request to close it)

	void				SetOpener(Window* opened_by, int sub_win_id, BOOL make_available_to_script = TRUE, BOOL from_window_open = FALSE);
	Window*				GetOpenerWindow() { return opener; }
	FramesDocument*		GetOpener(BOOL is_script = TRUE);
	URL					GetOpenerSecurityContext();
	DocumentOrigin*		GetOpenerOrigin() { return opener_origin; }
	BOOL				CanBeClosedByScript() { return can_be_closed_by_script || type != WIN_TYPE_NORMAL; }

#if defined SEARCH_MATCHES_ALL
	void				ResetSearch();
#endif // SEARCH_MATCHES_ALL

	int					GetCurrWinHistoryLength() { return 	currWinHistoryLength; }
	int					GetCurrentHistoryNumber() {return current_history_number; }

	Window*				Suc() const { return (Window*) Link::Suc(); }
	Window*				Pred() const { return (Window*) Link::Pred(); }

	VisualDevice*		VisualDev() const { return vis_device; }
	DocumentManager*	DocManager() const { return doc_manager; }
	MessageHandler*		GetMessageHandler() const { return msg_handler; }

#ifndef MOUSELESS

	void 				SetCursor(CursorType cursor, BOOL set_by_document = FALSE);
	void				UseDefaultCursor();

	void 				SetCurrentCursor();
	CursorType			GetCurrentArrowCursor();
	/**
	 * Calculates the current suitable cursor based on the current state and what doc has asked
	 * for.
	 */
	CursorType GetCurrentCursor();

	/**
	 * Sets a cursor value for tracking, which could probably be used later as the window cursor
	 */
	void SetPendingCursor(const CursorType cur_type) { m_pending_cursor = cur_type; }

	/**
	 * Gets the above cursor value
	 */
	CursorType GetPendingCursor() const { return m_pending_cursor; }

	/**
	 * Sets the pending cursor as the current cursor for the window
	 * If pending cursor was CURSOR_AUTO will set the default cursor
	 */
	void CommitPendingCursor();

#endif // !MOUSELESS

	uni_char*			GetMessage() const { return current_message; }
	uni_char*			GetDefaultMessage() const { return current_default_message; }
	OP_STATUS			SetMessage(const uni_char* msg);
	OP_STATUS			SetDefaultMessage(const uni_char* msg);

	void                DisplayLinkInformation(const URL &link_url, ST_MESSAGETYPE mt = ST_ASTRING, const uni_char *title = NULL);
	void                DisplayLinkInformation(const uni_char* link, ST_MESSAGETYPE mt = ST_ASTRING, const uni_char *title = NULL);
	void                DisplayLinkInformation(const uni_char* hlink, const uni_char* rel_name, ST_MESSAGETYPE mt = ST_ASTRING, const uni_char *title = NULL);

	WinState			GetState() const { return state; }
	void				SetState(WinState s);

	BOOL				IsCancelingLoading() { return is_canceling_loading; }

	BOOL				ShowImages() const { return show_img; }
#ifdef FIT_LARGE_IMAGES_TO_WINDOW_WIDTH
	BOOL				FitImages() const { return fit_img; }
	void				SetFitImages(BOOL flag) { fit_img=flag; }
#endif // FIT_LARGE_IMAGES_TO_WINDOW_WIDTH
	void				SetImagesSetting(SHOWIMAGESTATE setting);
	BOOL				LoadImages() const { return load_img; }

	LayoutMode			GetLayoutMode() const;
	void				SetLayoutMode(LayoutMode mode);

						/**
						 * Is this window currently set to apply text paragraph width limit?
						 *
						 * @return TRUE if we have set this window to apply text paragraph
						 *				width limit.
						 *				Note! This function may return TRUE, although the
						 *				content has decided that width limit should not be applied.
						 *				This function will also always return FALSE if the window is in
						 *				non-normal (MSR/CSSR/ERA/...) layout mode.
						 */
	BOOL				GetLimitParagraphWidth();

						/**
						 * Turn on/off text paragraph width limit.
						 *
						 * @param set TRUE to turn text paragraph width limit on,
						 *			  FALSE to turn it off.
						 *			  If TRUE, the width limit can still be overridden
						 *			  by content or non-normal layout modes. If the
						 *			  setting changed, the line widths will be requested
						 *			  to be recalculated.
						 */
	void				SetLimitParagraphWidth(BOOL set);

	int					GetFlexRootMaxWidth() const { return flex_root_max_width; }
	void				SetFlexRootMaxWidth(int max_width, BOOL reformat = TRUE);

	INT32				GetFramesPolicy() { return m_frames_policy; }
	void				SetFramesPolicy(INT32 frames_policy);

	void				SetForceWordWrap(BOOL value) { document_word_wrap_forced = value; }
	BOOL				ForceWordWrap() { return document_word_wrap_forced; }

	BOOL				GetHandheld() const { return is_ssr_mode; }

	BOOL				GetERA_Mode() const { return era_mode; }
	void				SetERA_Mode(BOOL value);

	void				Sync(); ///< Flushes pending drawoperations so that things wouldn't lag behind, if we do something agressive.
	void				SetWindowSize(int xWin, int yWin);
	void				SetWindowPos(int wWin, int hWin);
	void				GetWindowSize(int& xWin, int& yWin);
	void				GetWindowPos(int& wWin, int& hWin);
	void				SetClientSize(int cx, int cy);
	void				GetClientSize(int &cx, int &cy);

	/**
	 * Get the size of the viewport in document coordinates
	 *
	 * It is possible to set the base scale between physical pixels
	 * and layout pixels using the OpViewportController::SetTrueZoomBaseScale
	 * api.
	 *
	 * In TrueZoom mode, this function will return viewport scaled with the layout
	 * base scale.
	 *
	 * If we are zooming with regular "desktop" zoom, this function will
	 * scale the viewport with the regular zoom factor.
	 *
	 * @param viewport_width (out) The width of the viewport in CSS pixels
	 * @param viewport_height (out) The height of the viewport in CSS pixels
	 *
	 */
	void                GetCSSViewportSize(unsigned int& viewport_width, unsigned int& viewport_height);

	void				HandleNewWindowSize(unsigned int width, unsigned int height);

#ifdef LINK_SUPPORT
	/**
	 * Returns the first of a list of <link> elements in the
	 * current document or NULL if theer are none. See GetLinkDatabase
	 * for a different view of the list.
	 */
	const LinkElement*	GetLinkList() const;
	/**
	 * Returns a database object that allows iterating over sub-
	 * LinkElements. Will never return NULL. See LinkElementDatabase
	 * for more information.
	 */
	const LinkElementDatabase*	GetLinkDatabase() const;
#endif // LINK_SUPPORT

	/** Return the CSSCollection object for the current document. */
	CSSCollection*		GetCSSCollection() const;

#ifdef _SPAT_NAV_SUPPORT_
	OpSnHandler*			GetSnHandler() const { return sn_handler; }
	OpSnHandler*			GetSnHandlerConstructIfNeeded();
#  ifdef SN_LEAVE_SUPPORT
	// FIXME: Make into a proper core API (move to somewhere in windowcommander)
	void                LeaveInDirection(INT32 direction);
	void                SetSnLeaveListener(SnLeaveListener *listener);
	void                SnMarkSelection(); // re-highlights the SN item
#  endif
#endif /* _SPAT_NAV_SUPPORT */

	void				SetCurrentHistoryPos(int pos, BOOL update_doc_man, BOOL is_user_initiated);
	int					GetCurrentHistoryPos() { return current_history_number; }
	void				SetMaxHistoryPos(int pos) {max_history_number = pos; }
	void				SetMinHistoryPos(int pos) { min_history_number = pos; }
	void				SetCurrentHistoryPosOnly(int pos) { current_history_number = pos; }
	void				SetMaxHistory(int len);
	void				SetLastHistoryPos(int pos);
	/** Get the ID to use when creating a new DocListElm within this
	 *  Window, and make a new ID for the next.
	 */
	int					GetNextDocListElmId();
	/**
	 * Set user data for a specific history position.
	 *
	 * The data can later be retrieved with GetHistoryUserData().
	 *
	 * @param history_ID the ID for the history position, previously
	 * sent in OpViewportInfoListener::OnNewPage().
	 * @param user_data a pointer to the user data to set. Callee takes ownership of this object.
	 * @retval OpStatus::OK if operation was successfull
	 * @retval OpStatus::ERR if an error occured, for example if the history_ID could not be found
	 */
	OP_STATUS			SetHistoryUserData(int history_ID, OpHistoryUserData* user_data);
	/**
	 * Returns previously stored user data for a specific history
	 * position.
	 *
	 * @see SetHistoryUserData().
	 *
	 * @param history_ID the ID for the history position
	 * @param[out] user_data where to fill in the pointer to the user
	 * data. If no user data had previously been set, it is filled
	 * with NULL. The returned instance is still owned by the callee.
	 * @retval OpStatus::OK if operation was successfull
	 * @retval OpStatus::ERR if an error occured, for example if the history_ID could not be found
	 */
	OP_STATUS			GetHistoryUserData(int history_ID, OpHistoryUserData** user_data) const;
#ifdef DOCHAND_HISTORY_MANIPULATION_SUPPORT
	void				RemoveElementFromHistory(int pos);
	void				InsertElementInHistory(int pos, DocListElm * elm);
	void				RemoveAllElementsFromHistory(BOOL unlink = FALSE);
#endif // DOCHAND_HISTORY_MANIPULATION_SUPPORT
	/** Always disable java. Ignore java enabled pref, even for host overrides. */
	void				ForceJavaDisabled(BOOL force_off) { forced_java_disabled = force_off; }
	BOOL				GetForceJavaDisabled() { return forced_java_disabled; }
	/** Always disable plugins.*/
	void				ForcePluginsDisabled(BOOL force_off) { forced_plugins_disabled = force_off; }
	BOOL				GetForcePluginsDisabled() { return forced_plugins_disabled; }
	void				RemoveAllElementsExceptCurrent();
	//void				UpdateHistoryItem(const uni_char* title, const char* url_name);
	void				EnsureHistoryLimits();
	BOOL				HasHistoryNext() const { return current_history_number < max_history_number; }
	BOOL				HasHistoryPrev() const { return current_history_number > min_history_number; }
	void				SetHistoryNext(BOOL unused = FALSE);
	void				SetHistoryPrev(BOOL unused = FALSE);
	int					GetHistoryMin() const { return min_history_number; }
	int					GetHistoryMax() const { return max_history_number; }
	int					GetHistoryLen() const { return max_history_number - min_history_number + 1; }
	void				SetCheckHistory(BOOL val) { check_history = val; }
	void				CheckHistory();

	void				HighlightItem(BOOL forward);
	void				HighlightHeading(BOOL forward);
	void				HighlightURL(BOOL forward);

	/**
	 * Enables/disables drawing of highlight. This is enabled by default.
	 * @param enable set to TRUE to enable drawing, FALSE otherwise.
	 */
	void                SetDrawHighlightRects(BOOL enable) { m_draw_highlight_rects = enable; }

	/**
	 * Returns TRUE if spatnav highlight rects shall be drawn in this window.
	 */
	BOOL                DrawHighlightRects() { return  m_draw_highlight_rects; }
#ifdef _SPAT_NAV_SUPPORT_
	BOOL				MarkNextLinkInDirection(INT32 direction, POINT* startingPoint = NULL, BOOL scroll = FALSE);
	BOOL				MarkNextImageInDirection(INT32 direction, POINT* startingPoint = NULL, BOOL scroll = FALSE);
	BOOL                MarkNextItemInDirection(INT32 direction, POINT* startingPoint = NULL, BOOL scroll = FALSE);
#endif /* _SPAT_NAV_SUPPORT */
	OP_STATUS			GetHighlightedURL(uint16 key, BOOL bNewWindow, BOOL bOpenInBackground);

	static uni_char* 	ComposeLinkInformation(const uni_char* hlink, const uni_char* rel_name);

	/**
	 * @param bSubResourcesOnly TRUE if the method was called to
	 * indicate that e.g. (i)frames or additional inlines started
	 * loading, i.e. something which is not the top document.
	 */
	OP_STATUS			StartProgressDisplay(BOOL bReset, BOOL bResetSecurityState= TRUE, BOOL bSubResourcesOnly = FALSE);

	/**
	 * Marks the url as visited. Used to do more, thus the name of the method.
	 *
	 * @param[in] url The url to send to the UI.
	 */
	OP_STATUS			SetProgressText(URL& url, BOOL unused = FALSE);
	void				ResetProgressDisplay();
	void				EndProgressDisplay();

	void				SetProgressCount(long count) { progress_count = count; }
	long				GetProgressCount() const { return progress_count; }
	time_t				GetProgressStartTime() const { return start_time; }

	/**
	 * Set the number of bytes loaded in the Window's main document,
	 * not including inlines.
	 */
	void				SetDocProgressCount(OpFileLength count) { doc_progress_count = count; }
	OpFileLength		GetDocProgressCount() const { return doc_progress_count; }

	const uni_char*		GetProgressMessage();// const { return (progress_mess[0] ? progress_mess : progress_mess_low); }
	void				SetProgressState(ProgressState state, const uni_char *extra_info, MessageHandler *mh, long bytes, URL *url);
	ProgressState       GetProgressState() {return progress_state;};

    void                SetUploadTotal(OpFileLength progress_info1);
	void				SetUploadFileCount(int total, int loaded);

#ifdef PROGRESS_EVENTS_SUPPORT
	/**
	 * Propagate upload progress information to the Window's documents.
	 *
	 * @param[in] mh The handler of the loading URL.
	 * @param[in] loading_url The url being uploaded.
	 * @param[in] bytes The upload delta (in bytes.)
	 * @returns The method has no error conditions.
	 */
	void				UpdateUploadProgress(MessageHandler *mh, const URL &loading_url, unsigned long bytes);
#endif // PROGRESS_EVENTS_SUPPORT

	OP_STATUS			UpdateWindow(BOOL unused = FALSE);
	OP_STATUS			UpdateWindowAllDocuments(BOOL unused = FALSE);
	OP_STATUS			UpdateWindowDefaults(BOOL scroll, BOOL progress, BOOL news, WORD scale, BOOL size);

	INT32				GetScale() const { return scale; }
	void				SetScale(INT32 scale);
	void				SetTextScale(int scale);
	int					GetTextScale() { return m_text_scale; }

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	/** Get the ratio of device pixels to css pixels
	 *
	 * @returns the ratio of device pixels to css pixels, in percentage
	 */
	int					GetPixelScale();
#endif // PIXEL_SCALE_RENDERING_SUPPORT

#ifdef GRAB_AND_SCROLL
	BOOL				GetScrollIsPan() const;
	void				OverrideScrollIsPan();
#endif

	/**
	 * Checks if a key has recently had its keydown event cancelled.
	 *
	 * Background: If you cancel a keydown event then the corresponding
	 * keypress event should be automaticall cancelled. Whether that has
	 * happened is kept track of here in the Window class.
	 *
	 * @param key The key for which a keypress has arrived and we
	 * want to check if its keydown was cancelled.
	 *
	 * @returns TRUE if key was recently cancelled, in which case a
	 * keypress event with the same key should also be cancelled.
	 *
	 * @see SetRecentlyCancelledKeyDown()
	 */
	BOOL HasRecentlyCancelledKeyDown(OpKey::Code key) { return key == recently_cancelled_keydown; }

	/**
	 * Records or clears that a key has had its keydown event cancelled.
	 *
	 * Background: If you cancel a keydown event then the corresponding
	 * keypress event should be automaticall cancelled. Whether that has
	 * happened is kept track of here in the Window class.
	 *
	 * @param key The virtual key to remember a keydown for. Set to OP_KEY_INVALID
	 * to clear all memories of past cancelled keydown events.
	 *
	 * @see HasRecentlyCancelledKeyDown()
	 */
	void SetRecentlyCancelledKeyDown(OpKey::Code key) { recently_cancelled_keydown = key; }

	/** In truezoom the scaling affects only the content, not the layout. (No reflow caused when scaling) */
	void				SetTrueZoom(BOOL status);
	BOOL				GetTrueZoom() { return truezoom; }

	/** Set default scale to base text metrics and window size on in TrueZoom */
	void				SetTrueZoomBaseScale(UINT32 scale);
	UINT32				GetTrueZoomBaseScale() const { return truezoom_base_scale; }

	Window*				GetOutputAssociatedWindow();
	void				SetOutputAssociatedWindow(Window* ass);
	void				SetOutputAssociatedWindowId(unsigned long ass) { OutputAssociatedWindow = ass; }

	BOOL				ShowScrollbars() const { return bShowScrollbars; }
	void				SetShowScrollbars(BOOL show);

	unsigned long		Id() const { return id; }

	void				GotoHistoryPos();			// Only for startup

    void                SetLoading(BOOL set);
	BOOL				IsLoading() const;

	COLORREF            GetDefaultBackgroundColor() { return default_background_color; }
	void                SetDefaultBackgroundColor( const COLORREF &background_color ) { default_background_color = background_color; }

#ifdef ABSTRACT_CERTIFICATES
	const OpCertificate* GetCertificate();
#endif

	const uni_char*		Name() const { return name.CStr(); }
	void				SetName(const uni_char *n);

#ifdef CPUUSAGETRACKING
	/**
	 * Returns the CPUUsageTracker that collects time spent processing
	 * this Window (i.e. tab). Use this when we start working on something
	 * related to this Window after leaving generic code.
	 *
	 * @returns A pointer to a CPUUsageTracker. Never NULL.
	 */
	CPUUsageTracker*	GetCPUUsageTracker() { return &cpu_usage_tracker; }
#endif // CPUUSAGETRACKING

	void				SetForceColors(BOOL f);
	BOOL				ForceColors();

	void				SetCSSMode(CSSMODE f);
	void				SetNextCSSMode();
	CSSMODE				GetCSSMode() const { return CSSMode; }

	/** Set character encoding to force for this window.
	  * @param encoding Name of encoding to force.
	  * @param isnewwin Whether this is a virgin window or not.
	  */
	void				SetForceEncoding(const char *encoding, BOOL isnewwin = FALSE);
	/** Get character encoding forced for this window.
	  * @return Name of character set forced.
	  */
	const char *		GetForceEncoding() const { return forceEncoding.CStr(); }

	void				UpdateVisitedLinks(const URL& url);

#ifdef HISTORY_SUPPORT
	OP_STATUS			AddToGlobalHistory(URL& url, const uni_char *title);
	void				DisableGlobalHistory() { m_global_history_enabled = FALSE; }
	BOOL				IsGlobalHistoryEnabled() { return m_global_history_enabled; }
#endif // HISTORY_SUPPORT
	/**
	 * Recheck the title on the document and update global
	 * history and the UI. If delay is TRUE, then this is
	 * done on a message to avoid getting swamped if a script is
	 * changing title 50000 times per second. This is recommended.
	 *
	 * @param[in] delay Should be TRUE if called externally. Will prevent bad pages from consuming 100% CPU just updating the title.
	 *
	 * @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS			UpdateTitle(BOOL delay = TRUE);
	BOOL				IsWindowTitleGenerated() { return generated_title; }

	/**
	 * Set the title on the Window object and inform listeners about the new title
	 * so that the UI can be updated.
	 *
	 * @param[in] str The (new) title.
	 *
	 * @param[in] force If the UI should be told about the title even if it's the same as we already have.
	 *
	 * @param[in] generated If the title comes from the document (FALSE) or if generated from the url (TRUE).
	 *
	 * @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS			SetWindowTitle(const OpString& str, BOOL force, BOOL generated = FALSE);
	OP_STATUS			GetWindowTitle(OpString& str) const { return str.Set(title); }
	const uni_char*		GetWindowTitle() const { return title.CStr(); }

#ifdef SHORTCUT_ICON_SUPPORT
	OP_STATUS			SetWindowIcon(URL* url);
	Image               GetWindowIcon() const;
	BOOL                HasWindowIcon() const;
	URL					GetWindowIconURL()	{return m_shortcut_icon_url; }
	OP_STATUS			DecodeWindowIcon();
# ifdef ASYNC_IMAGE_DECODERS
	// implements ImageListener
	void				OnPortionDecoded();
	void				OnError(OP_STATUS status) {}
# endif // ASYNC_IMAGE_DECODERS
#endif // SHORTCUT_ICON_SUPPORT

	BYTE				GetSecurityState() const { return SecurityState; }
	void				SignalSecurityState(BOOL changed = FALSE);

	/**
	 * Reset security state for window to unknown. Must only be called when main document url is changed, as
	 * this will open up for setting high security state, risking an inline setting high security
	 * state on main document.
	 */
	void 				ResetSecurityStateToUnknown();

	void				SetSecurityState(BYTE sec, BOOL is_inline, const uni_char *name, URL *url= NULL);
	const uni_char *	GetSecurityStateText() const { return SecurityTextName.CStr();; }
	BOOL				GetPrivacyMode() { return m_privacy_mode; }
	void				SetPrivacyMode(BOOL privacy_mode);
#ifdef TRUST_RATING
	TrustRating			GetTrustRating() const { return m_trust_rating; }
	/**
	 * Sets the trust rating of the window. May be called with different ratings
	 * for the same window. The most strict rating is set as window's final rating.
	 *
	 * @param rating - the trust rating of the window.
	 * @param force - the passed in rating will be set even if it looses the rating.
	 */
	void				SetTrustRating(TrustRating rating, BOOL force = FALSE);
	/**
	 * Check trust rating of this Window.
	 *
	 * @param force if true check regardless of whether the feature is on or off.
	 * @param offline if true do not send check request to sitecheck server, only check against cached and in memory results.
	 * If both force and offline are true, the check will not connect, (offline is respected, force is not.)
	 */
	OP_STATUS			CheckTrustRating(BOOL force = FALSE, BOOL offline = FALSE);
#endif // TRUST_RATING
#ifdef WEB_TURBO_MODE
	BOOL				GetTurboMode() { return m_turbo_mode; }
	void				SetTurboMode(BOOL turbo_mode);
#endif // WEB_TURBO_MODE

	Window_Type			GetType() const { return type; }
	void				SetType(Window_Type new_type) { if (type != WIN_TYPE_BRAND_VIEW &&
                                                            type != WIN_TYPE_DOWNLOAD)
                                                        {
                                                            type = new_type;
                                                            SetFeatures(type);
                                                        }
                                                      }
    void                SetType(DocType doc_type);

    BOOL                IsSuppressWindow() { return type == WIN_TYPE_MAIL_VIEW || type == WIN_TYPE_MAIL_COMPOSE || type == WIN_TYPE_IM_VIEW || m_is_explicit_suppress_window || m_is_implicit_suppress_window; }
	BOOL				IsNormalWindow() {return (type == WIN_TYPE_NORMAL || type == WIN_TYPE_CACHE || type == WIN_TYPE_PLUGINS || type == WIN_TYPE_HISTORY || type == WIN_TYPE_JS_CONSOLE) ? TRUE : FALSE; }
	BOOL				IsThumbnailWindow() { return (type == WIN_TYPE_THUMBNAIL); }

	BOOL				IsMailOrNewsfeedWindow() { return type == WIN_TYPE_MAIL_VIEW || type == WIN_TYPE_MAIL_COMPOSE || type == WIN_TYPE_NEWSFEED_VIEW || type == WIN_TYPE_IM_VIEW; }

	/** If set to TRUE, the content of the document(s) may be transparent. There will be no default backgroundcolor drawn if
		the body of the document is transparent. */
	void				SetBackgroundTransparent(BOOL transparent) { m_is_background_transparent = transparent; }
	BOOL				IsBackgroundTransparent() { return m_is_background_transparent; }

#ifdef GADGET_SUPPORT
	void				SetGadget(OpGadget* gadget);
	OpGadget*			GetGadget() const;
#endif // GADGET_SUPPORT

	BOOL				IsScriptableWindow() {return m_is_scriptable_window && (IsNormalWindow() || IsThumbnailWindow() || type == WIN_TYPE_GADGET || type == WIN_TYPE_BRAND_VIEW || type == WIN_TYPE_DIALOG || type == WIN_TYPE_CONTROLLER || type == WIN_TYPE_DEVTOOLS); }
	BOOL				IsCustomWindow() {return IsNormalWindow() ? FALSE : TRUE;}

	void				SetIsSuppressWindow(BOOL is_suppress_window) { m_is_explicit_suppress_window = is_suppress_window; }
	/**< If is_suppress_window is TRUE, external embeds will be suppressed in
	     this window until this function is called again with a FALSE
	     argument.  If is_suppress_window is FALSE, external embeds will not
	     be suppressed unless the window type and/or contents also require
	     suppression of external embeds.  */
	void				SetIsImplicitSuppressWindow(BOOL is_suppress_window) { m_is_implicit_suppress_window = is_suppress_window; }
	/**< If is_suppress_window is TRUE, external embeds will be suppressed in
	     this window, because its contents require it, until this function is
	     called again with a FALSE argument.  If is_suppress_window is FALSE,
	     external embeds will no longer be suppressed because of the contents
	     of the window, but may be suppressed because of the window type or
	     because suppression has been requested via SetIsSuppressWindow(). */

	void				SetIsScriptableWindow(BOOL is_scriptable_window) { m_is_scriptable_window = is_scriptable_window; }

	void				Reload(EnteredByUser entered_by_user = NotEnteredByUser, BOOL conditionally_request_document = FALSE, BOOL conditionally_request_inlines = TRUE);
	/**< Reload window.

	     @param conditionally_request_document If TRUE, use URL_Reload_Conditional reload policy for document URLs, otherwise use URL_Reload_Full.
	     @param conditionally_request_inlines If TRUE, use URL_Reload_Conditional reload policy for inline URLs, otherwise use URL_Reload_Full. */


#ifdef _AUTO_WIN_RELOAD_SUPPORT_
	AutoWinReloadParam*	GetUserAutoReload() { return &m_userAutoReload; }
#endif // _AUTO_WIN_RELOAD_SUPPORT_

	/** Stop the document loading
	 * @param oom Set to TRUE to signal that the document is stopped due to OOM.
	 *            The loading is then stopped without parsing pending data (needs memory)
	 *            and the current document will be dropped before starting to load another one.
	 */
	BOOL				CancelLoad(BOOL oom = FALSE);

	void				Activate( BOOL fActivate = TRUE);

	void				Raise();
	void				Lower();

	FramesDocument*		GetCurrentDoc() const;

	/**
	 * @return When using frames the active frame document, for non-frames the
	 *         active document itself, if there is no active document at all
	 *         NULL is returned.
	 */
	FramesDocument*		GetActiveFrameDoc();

	BOOL				CloneWindow(Window**return_window=0,BOOL open_in_background=FALSE);

#ifdef SAVE_SUPPORT
	void				DEPRECATED(GetSuggestedSaveFileName( URL *url, uni_char *szName, int maxSize));
	OP_BOOLEAN			DEPRECATED(SaveAs(URL& url, BOOL load_to_file, BOOL save_inline, BOOL frames_only = FALSE));
	OP_BOOLEAN			DEPRECATED(SaveAs(URL& url, BOOL load_to_file, BOOL save_inline, const uni_char* szSaveToFolder, BOOL fCreateDirectory=FALSE, BOOL fExecute=FALSE, BOOL hasfilename=FALSE, const uni_char* handlerapplication=NULL, BOOL frames_only = FALSE));
#endif // SAVE_SUPPORT

	/** Update document loading information and notify OpLoadingListener.
	 *
	 * @param loaded_inlines The number of inlines loaded for the
	 * document so far.
	 * @param total_inlines The total number of inlines found in the
	 * document so far.
	 */
	void				UpdateLoadingInformation(int loaded_inlines, int total_inlines);

	void				HandleDocumentProgress(long bytes_read);
	void				HandleDataProgress(long bytes, BOOL upload_progress, const uni_char* extrainfo, MessageHandler *mh, URL *url);
	BOOL				HandleMessage(UINT message, int id, long user_data);

	void				GetClickedURL(URL url, DocumentReferrer& ref_url, short sub_win_id, BOOL user_initiated, BOOL open_in_background_window = FALSE);


	OP_STATUS			OpenURL(OpenUrlInNewWindowInfo *info, BOOL check_if_expired = TRUE, BOOL reload = FALSE, EnteredByUser entered_by_user = NotEnteredByUser, BOOL called_externally = FALSE);
	OP_STATUS			OpenURL(URL& url, DocumentReferrer ref_url, BOOL check_if_expired, BOOL reload, short sub_win_id, BOOL user_initiated = FALSE, BOOL open_in_background = FALSE, EnteredByUser entered_by_user = NotEnteredByUser, BOOL called_externally = FALSE);
	OP_STATUS			OpenURL(const char* url_name, BOOL check_if_expired = TRUE, BOOL user_initiated = FALSE, BOOL reload = FALSE, BOOL is_hotlist_url = FALSE, BOOL open_in_background = FALSE, EnteredByUser entered_by_user = NotEnteredByUser, BOOL called_externally = FALSE);
    void                OpenURLL(const char* in_url_str, BOOL check_if_expired = TRUE, BOOL user_initiated = FALSE, BOOL reload = FALSE, BOOL is_hotlist_url = FALSE, BOOL open_in_background = FALSE, EnteredByUser entered_by_user = NotEnteredByUser, BOOL called_externally = FALSE)
                            { LEAVE_IF_ERROR(OpenURL(in_url_str, check_if_expired, user_initiated, reload, is_hotlist_url, open_in_background, entered_by_user, called_externally)); }
	OP_STATUS           OpenURL(const uni_char* in_url_str, BOOL check_if_expired = TRUE, BOOL user_initiated = FALSE, BOOL reload = FALSE, BOOL is_hotlist_url = FALSE, BOOL open_in_background = FALSE, EnteredByUser entered_by_user = NotEnteredByUser, BOOL called_externally = FALSE, URL_CONTEXT_ID context_id = -1);
	void                OpenURLL(const uni_char* in_url_str, BOOL check_if_expired = TRUE, BOOL user_initiated = FALSE, BOOL reload = FALSE, BOOL is_hotlist_url = FALSE, BOOL open_in_background = FALSE, EnteredByUser entered_by_user = NotEnteredByUser, BOOL called_externally = FALSE, URL_CONTEXT_ID context_id = -1)
                            { LEAVE_IF_ERROR(OpenURL(in_url_str, check_if_expired, user_initiated, reload, is_hotlist_url, open_in_background, entered_by_user, called_externally, context_id)); }

	void				GenericMailToAction(const URL& url, BOOL called_externally, BOOL mailto_from_form, const OpStringC8& servername);

	DocumentManager*	GetDocManagerById(int sub_win_id);
	void				LoadPendingUrl(URL_ID url_id, int sub_win_id, BOOL user_initiated);

	BOOL                HandleLoadingFailed(Str::LocaleString msg_id, URL url);

	void				AskAboutUnknownDoc(URL& url, int sub_win_id);

#ifndef NO_EXTERNAL_APPLICATIONS
	BOOL				RunApplication(const OpStringC &application, const OpStringC &filename);
	BOOL				ShellExecute(const uni_char* filename);
#endif // !NO_EXTERNAL_APPLICATIONS

	BOOL				GetUserLoadFromCache() const { return user_load_from_cache; }
	void				SetUserLoadFromCache(BOOL val) { user_load_from_cache = val; }

	int					SetNewHistoryNumber();
	void				SetUseHistoryNumber(BOOL value) { OP_ASSERT(!"You need to upgrade the module that called this function!"); }
	void				SetCurrentHistoryNumber(int val) { current_history_number = val; }
	int					GetCurrentHistoryNumber() const { return current_history_number; }

	const char*			GetLastUserName() const { return lastUserName; }
	OP_STATUS			SetLastUserName(const char *_name);

	const char*			GetLastProxyUserName() const { return lastProxyUserName; }
	OP_STATUS			SetLastProxyUserName(const char *_name);

	OP_STATUS			GetHomePage(OpString &target) const { return target.Set(homePage); }
	OP_STATUS			SetHomePage(const OpStringC& url);

	const URL			GetActiveLinkURL() { return active_link_url; }
	void				SetActiveLinkURL(const URL& url, HTML_Document* doc);
	void				ClearActiveLinkURL();

	URL					GetCurrentShownURL();
	URL					GetCurrentLoadingURL();
	URL					GetCurrentURL();

	/**
	 * Returns the url context id for other use cases than network.
	 *
	 * The context id returned regards features like turbo and app cache, which use a context
	 * id internally to organize their data, as transparent, because from a user point
	 * of view, it is the same browsing session as having turbo or app cache disabled.
	 * It depends on features that require absolute data separation, like the main browsing
	 * context (id 0), gadgets, privacy mode, and other future features that might require
	 * their own separate context id.
	 *
	 * This method first checks if base_id is either from turbo or app cache, and if so
	 * returns the non app-cache and non-turbo context id. Else it returns base_id.
	 *
	 * @see Window::GetUrlContextId()
	 */
	URL_CONTEXT_ID		GetMainUrlContextId(URL_CONTEXT_ID base_id = 0);

	/**
	 * Return the url context id that urls opened from this window should use.
	 *
	 * @see Window::GetMainUrlContextId()
	 *
	 * @param url Optional url to help with the determination of the correct context id to use (e.g. in appcache's case).
	 */
	URL_CONTEXT_ID		GetUrlContextId(const uni_char *url = NULL);

	BOOL				GetURLCameFromAddressField() { return m_url_came_from_address_field; }
	void				SetURLCameFromAddressField(BOOL from_address_field) { m_url_came_from_address_field = from_address_field; }

#if defined REDIRECT_ON_ERROR || defined AB_ERROR_SEARCH_FORM
	/**
	 * Returns the exact text that the user entered in the address
	 * bar to be used in the search field for error pages
	 * Note: the string will be truncated at a few hundreds
	 * chars if it's going to be used on the search field on error
	 * pages because more than that would be useless for the user
	 * After loading the document, this string is emptied because
	 * it will not longer be used
	 */
	OpString*			GetTypedInUrl() { return &m_typed_in_url; }
#endif // defined REDIRECT_ON_ERROR || defined AB_ERROR_SEARCH_FORM

#ifndef HAS_NO_SEARCHTEXT
	BOOL				SearchText(const uni_char* key, BOOL forward, BOOL matchcase, BOOL words, BOOL next, BOOL wrap = FALSE, BOOL only_links = FALSE);
# ifdef SEARCH_MATCHES_ALL
	BOOL				HighlightNextMatch(const uni_char* txt,
							BOOL match_case, BOOL match_words,
							BOOL links_only, BOOL forward, BOOL wrap,
							BOOL &wrapped, OpRect& rect);
	/**< Will move the search highlight to the next match found in the document. If no matches have been
	 *   found yet a search will be executed and the next match highlighted.
	 *   @param[IN] txt The text to search for.
	 *   @param[IN] match_case If set to TRUE the search will be case sensitive.
	 *   @param[IN] match_words If set to TRUE only whole words will be matched.
	 *   @param[IN] links_only If set to TRUE only text in links will be matched.
	 *   @param[IN] forward If set to TRUE the next match will be highlighted otherwise the previous.
	 *   @param[IN] wrap If set to TRUE and there is no more matches in the direction specified
	 *                   by forward it will wrap around and start at the other end of the document.
	 *   @param[OUT] wrapped Set to TRUE if wrapped around.
	 *   @param[OUT] rect The coordinate of the search hit if there was any. In document
	 *                    coordinates.
	 *   @returns TRUE if a next match has been found. */

	void				EndSearch(BOOL activate_current_match, int modifiers);
	/**< Must be called from the UI when a search is finished. If activate_current_match is
	 *   TRUE the current matched element will be activated, eg. links navigated to. The
	 *   modifiers parameter specifies which shift or control keys are held down, and only
	 *   needs to be specified if activate_current_match is TRUE. */
# endif // SEARCH_MATCHES_ALL
#endif // !HAS_NO_SEARCHTEXT

#ifdef _PRINT_SUPPORT_
	PrinterInfo*		GetPrinterInfo(BOOL preview) { return !printer_info ? preview_printer_info : printer_info; }
	void				SetPrinterInfo(PrinterInfo* pr_info) { printer_info = pr_info; }
	BOOL				GetPreviewMode() { return preview_mode; }
	void				SetPreviewMode(BOOL val) { preview_mode = val; }
	BOOL				GetPrintMode() { return print_mode; }
	BOOL				TogglePrintMode(BOOL preview);

	BOOL				StartPrinting(PrinterInfo* pinfo, int from_page, BOOL selected_only);
	void				PrintNextPage(PrinterInfo* pinfo);
	void				StopPrinting();
	DM_PrintType		GetFramesPrintType() { return frames_print_type; }
	OP_STATUS			SetFramesPrintType(DM_PrintType fptype, BOOL update); // { frames_print_type = fptype; }

	/* For async printing */

	BOOL IsFormattingPrintDoc() { return is_formatting_print_doc; }
	void SetFormattingPrintDoc(BOOL formatting) { is_formatting_print_doc = formatting; }
	BOOL IsPrinting() { return is_printing; }
	void SetPrinting(BOOL printing) { is_printing = printing;}

#endif // _PRINT_SUPPORT_

	void				TrackLinkMenu			( );

	void				SetAlwaysLoadFromCache(BOOL from_cache);
	BOOL				AlwaysLoadFromCache() { return always_load_from_cache; }

	void				ToggleImageState();

#ifdef LIBOPERA
	/** Will be called from DocumentManager to tell this Window if the request
		(call to OpenURL()) is user initiated or not. */
	void                SetUserInitiated(BOOL user) { m_lastReqUserInitiated = user; }

	/** Was the last request (call to OpenURL()) user initiated? */
	BOOL                WasLastReqUserInitiated() { return m_lastReqUserInitiated; }
#endif // LIBOPERA

	void				RaiseCondition( OP_STATUS type );

#ifdef ACCESS_KEYS_SUPPORT
	BOOL				GetAccesskeyMode() const { return in_accesskey_mode; }
	void				SetAccesskeyMode(BOOL mode) { in_accesskey_mode = mode; }
#endif // ACCESS_KEYS_SUPPORT


	OnlineMode			GetOnlineMode();
	OP_STATUS			QueryGoOnline(URL *url);

	// implementation of the OpDocumentListener::DialogCallback interface
	void				OnDialogReply(OpDocumentListener::DialogCallback::Reply reply);

	void				LockWindow();
	void				UnlockWindow();

	BOOL				IsURLAlreadyRequested(const URL &url);
	void				SetURLAlreadyRequested(const URL &url);

	void				HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	void				ClearMovedUrls() { moved_urls.Clear(); }

	/**
	 * Sets a flag informing the browser core that this window is
	 * visible, or not visible to the user. When a window is set to
	 * not visble certain then unneeded background processing will be
	 * disabled and other processing might prioritize windows that are
	 * visible, thus giving the user a perceived performance increase.
	 *
	 * For the moment this doesn't prevent plugins in background
	 * windows from using high amounts of CPU, and scripts will
	 * continue running.
	 *
	 * If this is set to FALSE when the window is actually showing,
	 * animations might not run (neither gifs, nor SVGs).
	 *
	 * Typically this is set to false whenever the window gets covered
	 * by any other window and then set to true when the operating
	 * system sends a paint event.
	 *
	 * @param is_visible Set to TRUE if the user can see the Window, FALSE
	 * otherwise.
	 */
	void SetVisibleOnScreen(BOOL is_visible) { m_is_visible_on_screen = is_visible; }

	/**
	 * @return TRUE if the window might be visible to the user. FALSE
	 * if it is not possible for the user too see the Window. For
	 * instance if the window is in a background tab or completely
	 * hidden by other opaque windows (not necessarily opera windows).
	 */
	BOOL IsVisibleOnScreen() const { return m_is_visible_on_screen; }

	BOOL IsEcmaScriptDisabled() { return ecmascript_disabled; }
	BOOL IsEcmaScriptEnabled() { return !ecmascript_disabled; }
	void SetEcmaScriptDisabled(BOOL value) { ecmascript_disabled = value; }

	BOOL IsEcmaScriptPaused() { return ecmascript_paused != NOT_PAUSED; }
	void SetEcmaScriptPaused(BOOL value);

	void CancelAllEcmaScriptTimeouts();

#ifdef NEARBY_ELEMENT_DETECTION
	/**
	 * Set new element expander.
	 *
	 * This element expander is currently displaying nearby elements of
	 * interest. If a previously active element expander is set, it will be
	 * removed.
	 *
	 * @param expander New element expander, or NULL to remove the current one
	 */
	void			SetElementExpander(class ElementExpander* expander);

	/**
	 * Get the current element expander, if any.
	 */
	ElementExpander*
					GetElementExpander() { return element_expander; }
#endif // NEARBY_ELEMENT_DETECTION

	ViewportController *GetViewportController() const { return viewportcontroller; }

	/**
	 * A page has the right to show one (1) unsolicited download dialog.
	 * This keeps track of whether it has done that or not. This flag
	 * should only be checked and set on the top document.
	 *
	 * Set to TRUE when the UI has been sent a signal that might result
	 * in a download dialog. For instance AskAboutUnknownDoc or
	 * OnDownloadRequest.
	 *
	 * Set to FALSE when the user has performed an action that should allow
	 * the page to spam one more download dialog. For instance after
	 * walking in history of loading a (user initiated) document, or
	 * clicking in the page.
	 */
	void SetHasShownUnsolicitedDownloadDialog(BOOL new_value) { OP_ASSERT(!new_value || !has_shown_unsolicited_download_dialog); has_shown_unsolicited_download_dialog = new_value; }

	/**
	 * A page has the right to show one (1) unsolicited download dialog.
	 * This keeps track of whether it has done that or not. This flag
	 * should only be checked and set on the top document.
	 *
	 * If this returns TRUE, no calls to listeners that could show
	 * download dialogs should be done unless that action is a direct
	 * result of something the user did. Examples of functions that
	 * should not be called are AskAboutUnknownDoc and OnDownloadRequest.
	 */
	BOOL HasShownUnsolicitedDownloadDialog() { return has_shown_unsolicited_download_dialog; }

	/**
	 * The Screen Properties (ref OpScreenInfo) for the windows screen
	 * has changed, and we need to mark all screen properties caches in
	 * all visual devices as dirty, so that new and fresh properties are
	 * gathered. This will iterate all doc managers under the window and
	 * tell them to update.
	 */
	void ScreenPropertiesHaveChanged();

#ifdef DOM_JIL_API_SUPPORT
	void SetScreenPropsChangeListener(ScreenPropsChangedListener* screen_props_listener) { m_screen_props_listener = screen_props_listener; }
#endif // DOM_JIL_API_SUPPORT

	WindowViewMode GetViewMode();

#ifdef SCOPE_PROFILER

	/**
	 * Start profiling this Window, using the specified OpProfilingSession.
	 *
	 * @param session The OpProfilingSession to record profiling data to.
	 *                The session is not owned by the Window, and must
	 *                therefore outlive the Window.
	 * @return OpStatus::OK if profiling was started; OpStatus::ERR if
	 *         profiling already was started for this Window; or
	 *         OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS StartProfiling(OpProfilingSession *session);

	/**
	 * Stop profiling on this Window. If the Window is not currently being
	 * profiled, nothing happens.
	 */
	void StopProfiling();

	/**
	 * Get the current OpProfilingSession for this Window. This can be used to
	 * check if we currently are profiling this Window.
	 *
	 * @return The current OpProfilingSession. (NULL means the Window is not
	 *         currently being profiled).
	 */
	OpProfilingSession *GetProfilingSession() const { return m_profiling_session; }

#endif // SCOPE_PROFILER

#ifdef GEOLOCATION_SUPPORT
	/** Called whenever any document in this window starts or stops accessing geolocation data.
	    Will result in a call to OpDocumentListener::OnGeolocationAccess. */
	void NotifyGeolocationAccess();
#endif // GEOLOCATION_SUPPORT

#ifdef MEDIA_CAMERA_SUPPORT
	/** Called whenever any document in this window starts or stops accessing camera.
	    Will result a call to OpDocumentListener::OnCameraAccess. */
	void NotifyCameraAccess();
#endif // MEDIA_CAMERA_SUPPORT

#ifdef KEYBOARD_SELECTION_SUPPORT
	/**
	 * Called from frames document when keyboard selection mode has
	 * changed. Will call a callback to through windowcommander if the
	 * mode has changed.
	 */
	void UpdateKeyboardSelectionMode(BOOL enabled);
#endif // KEYBOARD_SELECTION_SUPPORT

#ifdef EXTENSION_SUPPORT
	/**
	 * Perform a screenshot of a visible part of a document in this window and write it to a bitmap.
	 *
	 * @param target_document The document to take a screenshot of. This can be either
	 *                        the top level document or a child document like an iframe. MUST NOT be NULL.
	 * @param source_document The document which requested a screenshot. It is used for determining if it
	 *                        has access to target subdocuments and thus if they should be included in the
	 *                        screenshot. MUST NOT be NULL.
	 * @param[out] bitmap     Set to the bitmap to which the screenshot is be painted. It is created
	 *                        by this method and the caller takes ownership of it if the call succeeded.
	 * 
	 * @return OpStatus::OK, OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS PerformScreenshot(FramesDocument* target_document, FramesDocument* source_document, OpBitmap*& bitmap);
private:
	/**
	 * Helper method for PerformScreenshot. Collects a list of all VisualDevices the source_document isn't allowed to access.
	 *
	 * @param target_document The root document of the subtree to check.
	 * @param source_document The document which requested a screenshot.
	 * @param visdev_to_hide  List of VisualDevices of documents which should not be painted.
	 * @return OpStatus::OK, OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS GetDisallowedScreenshotVisualDevices(FramesDocument* target_document, FramesDocument* source_document, OpVector<VisualDevice>& visdev_to_hide);
public:
#endif // EXTENSION_SUPPORT
};

#endif // DOCHAND_WIN_H
