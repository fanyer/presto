/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DOCHAND_WINMAN_H
#define DOCHAND_WINMAN_H

#include "modules/dochand/winman_constants.h"
#include "modules/dochand/win.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/url/url2.h"
#include "modules/util/simset.h"
#include "modules/prefs/prefsmanager/opprefslistener.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/hardcore/timer/optimer.h"

class OpWindowCommander;
class FramesDocument;
class DocumentManager;
class MessageHandler;
class DocumentWindow;
class Window;
class OpWindow;
class ES_Thread;

class WindowManager
	: public OpPrefsListener,
	  public MessageObject
{
private:

	ST_MESSAGETYPE	messageStringType;
	Head			win_list;

	uni_char		current_popup_message[4096]; /* ARRAY OK 2008-02-28 jl */
	BOOL			current_popup_message_statusline_only;
	Window*			curr_clicked_window;

	URL				curr_clicked_url;
	URL				empty_url;

	unsigned		idcounter;

public:

					WindowManager();

					// Doesn't really need to be virtual, but the class has virtual functions
					// so GCC will warn if the destructor isn't.  And it doesn't really matter
					// anyway.  (jl@opera.com)
					virtual ~WindowManager();

	void			ConstructL();
	void			Clear();

#if defined(_MACINTOSH_)
	// Fix to take care of issues of values being set before window
	// is fully initiallized
	Window*			FirstWindow() const { if(this) return (Window*)win_list.First(); else return 0; }
	Window*			LastWindow() const { if(this) return (Window*)win_list.Last(); else return 0; }
#else // _MACINTOSH_
	Window*			FirstWindow() const { return static_cast<Window*>(win_list.First()); }
	Window*			LastWindow() const { return static_cast<Window*>(win_list.Last()); }
#endif // _MACINTOSH_

	Window*			GetWindow(unsigned long nid);

	void			Insert(Window* window, Window* precede);

	Window*         AddWindow(OpWindow* opWindow, OpWindowCommander* opWindowCommander);

	void			DeleteWindow(Window* window);
	void			RemoveInvalidWindows ();

	const uni_char*	GetPopupMessage(ST_MESSAGETYPE *strType = NULL);
	void			SetPopupMessage(const uni_char *msg, BOOL fStatusLineOnly=FALSE, ST_MESSAGETYPE strType=ST_ASTRING);
	BOOL			IsPopupMessageForStatusLineOnly() const { return current_popup_message_statusline_only; }

	OP_STATUS		UpdateWindows(BOOL unused = FALSE);
	OP_STATUS		UpdateWindowsAllDocuments(BOOL unused = FALSE);
	OP_STATUS		UpdateAllWindowDefaults(BOOL scroll, BOOL progress, BOOL news, WORD scale, BOOL size);

	Window*			SignalNewWindow(Window* opener, int width=0, int height=0, BOOL3 scrollbars=MAYBE, BOOL3 toolbars=MAYBE,
																			   BOOL3 focus_document=MAYBE, BOOL3 open_background=MAYBE,
																			   BOOL opened_by_script = FALSE);
	/**
	 * Request the closing of a Window from the platform (UiWindowListener). Window closing
	 * is asynchronous, and your request may be ignored or denied. For instance, the page
	 * may display a confirmation dialog to the user, and the user may choose to cancel.
	 *
	 * @param[in] window The Window to close. Can not be NULL.
	 */
	void			CloseWindow(Window *window);

public:
	/**
	 * Creates a new Window with the help of the platform.
	 *
	 * @param[in] new_window YES or MAYBE, if MAYBE it might reuse a window instead
	 * of creating a new one, in case there is a suitable window to reuse and the
	 * user preferences are set to not create new windows.
	 *
	 * @todo document and clean up arguments
	 */
	Window*			GetAWindow(BOOL unused1, BOOL3 new_window, BOOL3 background, int width=0, int height=0, BOOL unused2=FALSE, BOOL3 scrollbars=MAYBE, BOOL3 toolbars=MAYBE,
                               const uni_char* unused3 = NULL,const uni_char* unused4 = NULL, BOOL unused5 = FALSE, Window* opener = NULL);
#if defined(MSWIN)
	Window*			GetADDEIdWindow(DWORD id, BOOL &tile_now);
#endif // MSWIN

	void			SetGlobalImagesSetting(SHOWIMAGESTATE set);

#ifdef _AUTO_WIN_RELOAD_SUPPORT_
	void			OnTimer() {}				//	Called 10 times a second
#endif // _AUTO_WIN_RELOAD_SUPPORT_

	void			SetMaxWindowHistory(int len);

	Window*			OpenURLNewWindow(const char* url_name, BOOL check_if_expired = TRUE, BOOL3 open_in_new_window = YES, EnteredByUser entered_by_user = NotEnteredByUser);
	Window*			OpenURLNewWindow(const uni_char* url_name, BOOL check_if_expired = TRUE, BOOL3 open_in_new_window = YES, EnteredByUser entered_by_user = NotEnteredByUser);
	static BOOL		CheckTargetSecurity(FramesDocument *source_doc, FramesDocument *target_doc);

	/**
	 * @param[in] open_in_other_window If the url is requested to be
	 *            loaded in another window than the current. Depending
	 *            on preferences and other settings it might still b
	 *            loaded in the current window. When this is TRUE,
	 *            open_in_background controls whether the new window
	 *            should be brought to front or not.
	 * @param[in] open_in_background ignored if open_in_other_window is FALSE.
	 *            If open_in_other_window and open_in_background are both TRUE
	 *            then the new page will, if possible, load in a background
	 *            window, leaving focus on the current window.
	 */
	OP_STATUS		OpenURLNamedWindow(URL url, Window* load_window, FramesDocument* doc, int sub_win_id, const uni_char* window_name, BOOL user_initiated, BOOL open_in_other_window = FALSE, BOOL open_in_background = FALSE, BOOL delay_open = FALSE, BOOL open_in_page = FALSE, ES_Thread *thread = NULL, BOOL plugin_unrequested_popup = FALSE);

	Window*			GetNamedWindow(Window* from_window, const uni_char* window_name, int &sub_win_id, BOOL open_new_window = TRUE);

	/**
	 * @deprecated Old hack hopefully not used much longer. The action that needs the url also needs to keep track of the url itself. This was prone to race conditions and was magic and undocumented.
	 */
	URL				GetCurrentClickedURL() const { return curr_clicked_url; }
	/**
	 * @deprecated Old hack hopefully not used much longer. The action that needs the url also needs to keep track of the url itself. This was prone to race conditions and was magic and undocumented.
	 */
	void			SetCurrentClickedURL(URL url) { curr_clicked_url = url; }
	Window*			GetCurrentClickedWindow() const { return curr_clicked_window; }
	void			SetCurrentClickedWindow(Window* window) { curr_clicked_window = window; }
	void			ResetCurrentClickedURLAndWindow() { curr_clicked_url = URL(); curr_clicked_window = NULL; }

	void			UpdateVisitedLinks(const URL& url, Window* exclude_window);

	void			ReloadAll();
	void			RefreshAll();

	void			ShowHistory(BOOL bShow);

	//void			HandleNewViewers();

	/// @deprecated, use Window::OpenURL* or DocumentManager::OpenURL() instead
	DEPRECATED(OP_STATUS SetUrlToOpen(const char *url)) { return OpStatus::OK; }

	// OpPrefsListener interface
	void PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue);
	// Ignore changed strings
	void PrefChanged(OpPrefsCollection::Collections, int, const OpStringC &) {}

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/**
	 * This must be called once and exactly once by a document that has
	 * blink tags to ensure that it gets Blink calls. When there are no
	 * document that have called AddDocumentWithBlink then there are no
	 * calls to Blink.
	 */
	void AddDocumentWithBlink();

	/**
	 * A document that has called AddDocumentWithBlink must call this, at the
	 * last when it's about to be destroyed. This will remove it from the
	 * count of objects that are needed Blink calls.
	 */
	void RemoveDocumentWithBlink();

#ifdef _OPERA_DEBUG_DOC_
	/**
	 * Returns the total number of FramesDocuments alive. This includes all Windows and all history.
	 */
	unsigned GetDocumentCount() { return g_opera->doc_module.m_total_document_count; }

	/**
	 * Returns a count of documents currently being active, i.e. current in their tabs, including all
	 * active sub documents. Documents in history are not included. This will do an active
	 * count operation so it's not suitable for performance sensitive operations.
	 */
	unsigned GetActiveDocumentCount();
#endif // _OPERA_DEBUG_DOC_

	OP_STATUS OnlineModeChanged();
	/**< Tells all windows that there has been a change in online mode.  The
	     preference PrefsCollectionNetwork::OfflineMode should already have the
	     value we're updating too (e.g., when this function is called as we go
	     online, it should already the TRUE.)

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY (on failure, all
	             windows will have been told, but any number of them might have
	             failed.) */

#ifdef AB_ERROR_SEARCH_FORM
	/**
	 * Returns the last text the user typed, set by SetLastTypeduserText()
	 */
	const uni_char* GetLastTypedUserText() { return m_last_typed_user_text ? m_last_typed_user_text->CStr() : NULL; }
	/**
	 * Sets the last text the user input on the address bar.
	 * This value will only be held during the Window::OpenURL call
	 * As it's necessary to workaround the nested calls within the
	 * URL APIs. This information will be used in the OpIllegalHost
	 * page to present a neatly built search form with what the user
	 * input
	 */
	void SetLastTypedUserText(OpString* user_text) { m_last_typed_user_text = user_text; }
#endif //AB_ERROR_SEARCH_FORM

	/** 
	 *  Tells window_manager that a window should use the privacy mode
	 *  network context.  This means that the window's cache, cookies etc
	 *  will be separated from regular browsing, so that regular windows
	 *  won't have access to data loaded in the private context.
	 *  
	 *  WindowCommander counts how many windows use the context and deletes
	 *  it when no one are using it anymore.  It's therefor important that
	 *  Windows call this function only once (when entering privacy mode),
	 *  and also calls RemoveWindowFromPrivacyModeContext exactly once (when 
	 *  being deleted or exiting privacy mode).
	 *  
	 *  @return the ID of the context to use.
	 */
	URL_CONTEXT_ID    AddWindowToPrivacyModeContext();
	
	/** 
	 * Call this once when a window exits privacy mode or closes.
	 * @see AddWindowToPrivacyModeContext()
	 */  
	void              RemoveWindowFromPrivacyModeContext();

	/**
	 * Get context id to use for all URLs loaded in windows in privacy 
	 * mode (and only for windows in privacy mode) 
	 */
	URL_CONTEXT_ID	  GetPrivacyModeContextId() { return m_privacy_mode_context_id; }


	/**
	 * Get number of windows/tabs currently in privacy mode.
	 */
	int               GetNumberOfWindowsInPrivacyMode() { return m_number_of_windows_in_privacy_mode; }


private:
	/**
	 * Call this as last window in privacy mode is closed or disables privacy mode.
	 * It will clean up the private data.
	 */
	void 			  ExitPrivacyMode();

#ifdef WEB_TURBO_MODE
public:
	/**
	*  Call this function to make sure the special turbo mode network
	*  context is created before using it. The first time in a browser
	*  session this function is called the network context will be
	*  created. The context will remain intact until the browser session
	*  ends.
	*
	*  The context MUST be used for all URLs loaded from windows with
	*  turbo mode enabled. Doing so will ensure that the window's cache
	*  will be separated from regular browsing, so that regular windows
	*  won't have access to data loaded in the private context.
	*  
	*  @return OpStatus::OK on success. OpStatus::ERR or OpStatus::ERR_NO_MEMORY on failure.
	*/
	OP_STATUS CheckTuboModeContext();

	/**
	*  @return The context ID to use for all URLs loaded in windows using turbo mode 
	*/
	URL_CONTEXT_ID GetTurboModeContextId() { return m_turbo_mode_context_id; }
#endif // WEB_TURBO_MODE

private:
	class BlinkTimerListener : public OpTimerListener
	{
	private:
		void PostNewTimerMessage();
		BOOL m_is_active;
		BOOL m_message_in_queue;
		OpTimer* m_timer;

	public:
		BlinkTimerListener();
		~BlinkTimerListener();
		virtual void OnTimeOut(OpTimer* timer);
		void SetEnabled(BOOL enable);
	};

	BlinkTimerListener m_blink_timer_listener;

	int m_documents_having_blink;

	URL_CONTEXT_ID m_privacy_mode_context_id;             // context id to use for URLs loaded in windows with privacy mode
	int m_number_of_windows_in_privacy_mode;   // how many windows are currently in privacy mode
#ifdef WEB_TURBO_MODE
	URL_CONTEXT_ID m_turbo_mode_context_id;  // context id to use for URLs loaded in turbo mode
#endif //WEB_TURBO_MODE

#ifdef AB_ERROR_SEARCH_FORM
	OpString* m_last_typed_user_text;
#endif //AB_ERROR_SEARCH_FORM
};

#endif // DOCHAND_WINMAN_H
