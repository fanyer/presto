/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef BROWSER_DESKTOP_WINDOW_H
#define BROWSER_DESKTOP_WINDOW_H

#include "adjunct/quick_toolkit/contexts/DialogContextListener.h"
#include "adjunct/quick_toolkit/widgets/OpBar.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "modules/prefs/prefsmanager/prefstypes.h"

class Window;
class Hotlist;
class HotlistDesktopWindow;
class OpGroup;
class OpToolbar;
class OpPersonalbar;
class OpPagebar;
class OpDragScrollbar;
class OpStatusbar;
class OpSplitter;
class OpWorkspace;
class OpWindowCommander;
class OpBrowserView;
class CyclePagesPopupWindow;
class FindTextManager;
class FullscreenPopupController;
class OpThumbnailPagebar;
class OpStatusbar;

class BlockedPopup
{
	public:

		BlockedPopup(DocumentDesktopWindow* source_window, const uni_char* title, const uni_char* url, OpDocumentListener::JSPopupOpener* opener);

		~BlockedPopup()
		{
			if (m_opener)
				m_opener->Release();
			m_opener = NULL;
		}

		void Open()
		{
			if (m_opener)
				m_opener->Open();
			m_opener = NULL;
		}

		BOOL IsFrom(DocumentDesktopWindow* source_window);

		const uni_char* GetTitle() {return m_title.CStr();}
		const uni_char* GetURL() {return m_url.CStr();}
		INT32			GetID() {return m_id;}

	private:

		DocumentDesktopWindow*				m_source_window;
		URL									m_source_url;
		OpString							m_title;
		OpString							m_url;
		OpDocumentListener::JSPopupOpener*	m_opener;
		INT32								m_id;
};


/***********************************************************************************
**
**	BrowserDesktopWindow
**
***********************************************************************************/

class BrowserDesktopWindow :	public DesktopWindow,
								public OpWorkspace::WorkspaceListener,
								public DocumentWindowListener,
								public OpTimerListener,
								public DialogContextListener
{
	friend class CyclePagesPopupWindow;

	protected:
								BrowserDesktopWindow(
#ifdef DEVTOOLS_INTEGRATED_WINDOW
														BOOL devtools_only = FALSE,
#endif // DEVTOOLS_INTEGRATED_WINDOW
														BOOL privacy_browsing = FALSE,
														BOOL lazy_init  = FALSE
													);

	public:

		static OP_STATUS Construct(BrowserDesktopWindow** obj
#ifdef DEVTOOLS_INTEGRATED_WINDOW
									, BOOL devtools_only = FALSE
#endif // DEVTOOLS_INTEGRATED_WINDOW
									, BOOL privacy_browsing = FALSE
									, BOOL lazy_init  = FALSE
									);

		virtual					~BrowserDesktopWindow();

		void					CyclePages(OpInputAction* action);

		OpSession*				GetUndoSession() {return m_undo_session;}

		BlockedPopup*			GetBlockedPopup(INT32 pos) {return m_blocked_popups.Get(pos);}
		INT32					GetBlockedPopupCount() {return m_blocked_popups.GetCount();}
		void					RemoveBlockedPopup(DocumentDesktopWindow *source);
		void					EmptyClosedTabsAndPopups();

		CyclePagesPopupWindow*	GetCyclePagesPopupWindow() {return m_cycle_pages_popup;}
		DesktopWindow*			GetActiveDesktopWindow() {return m_workspace->GetActiveDesktopWindow();}
		DesktopWindow*			GetActiveNonHotlistDesktopWindow() {return m_workspace->GetActiveNonHotlistDesktopWindow();}
		DocumentDesktopWindow*	GetActiveDocumentDesktopWindow() {return m_workspace->GetActiveDocumentDesktopWindow();}
		MailDesktopWindow*		GetActiveMailDesktopWindow() {return m_workspace->GetActiveMailDesktopWindow();}
#ifdef IRC_SUPPORT
		ChatDesktopWindow*		GetActiveChatDesktopWindow() {return m_workspace->GetActiveChatDesktopWindow();}
#endif
		FindTextManager*		GetFindTextManager() { return m_find_text_manager; }

		Hotlist*				GetHotlist();

		void					UpdateWindowTitle(BOOL delay = FALSE);

	    void                    AddAllPagesToBookmarks(INT32 id);
		BOOL					IsSessionUndoWindow(DesktopWindow* desktop_window);

		OP_STATUS				AddClosedSession();

#ifdef DEVTOOLS_INTEGRATED_WINDOW
		virtual void			AttachDevToolsWindow(BOOL attach);
		virtual BOOL			HasDevToolsWindowAttached() { return m_devtools_workspace && m_devtools_workspace->GetActiveDesktopWindow(); }
		virtual BOOL			IsDevToolsOnly() { return m_devtools_only; }
#endif // DEVTOOLS_INTEGRATED_WINDOW

		// Subclassing DesktopWindow

#ifdef SUPPORT_MAIN_BAR
		OpToolbar*				GetMainBar() {return m_main_bar;}
#endif
		virtual OpWorkspace*	GetWorkspace() {return m_workspace;}
#ifdef DEVTOOLS_INTEGRATED_WINDOW
		virtual const char*		GetWindowName() {return (IsDevToolsOnly()?"Developer Tools Window":"Browser Window");}
#else
		virtual const char*		GetWindowName() {return "Browser Window";}
#endif

		void					CloseAllTabs() { m_workspace->CloseAll(); }
		void					CloseAllTabsExceptActive();

		// hooks subclassed from DesktopWindow

		virtual BOOL			PrivacyMode(){return m_privacy_mode;}
		virtual void			SetPrivacyMode(BOOL mode){m_privacy_mode = mode;}
		virtual void			OnLayout();
		virtual void			OnSettingsChanged(DesktopSettings* settings);
		virtual void			OnActivate(BOOL activate, BOOL first_time);
		virtual void			OnFullscreen(BOOL fullscreen);
		virtual void			OnSessionReadL(const OpSessionWindow* session_window);
		virtual void			OnSessionWriteL(OpSession* session, OpSessionWindow* session_window, BOOL shutdown);

		// OpTimerListener

		virtual void			OnTimeOut(OpTimer* timer) {UpdateWindowTitle();}

		// OpTypedObject

		virtual Type			GetType() {return WINDOW_TYPE_BROWSER;}
		virtual Type			GetOpWindowType() { return m_devtools_only ? WINDOW_TYPE_DEVTOOLS : GetType(); }

		// == OpInputContext ======================

		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual const char*		GetInputContextName() {return "Browser Window";}
		virtual void			SetFocus(FOCUS_REASON reason);

		BOOL 					HandleImageActions(OpInputAction* action, OpWindowCommander* wc );
		BOOL					HandleFrameActions(OpInputAction* action, OpWindowCommander* wc );

		// Implementing the OpWidgetListener interface

		virtual BOOL			OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked);

		// Implementing the OpWorkspace::WorkspaceListener interface

		virtual void			OnDesktopWindowAdded(OpWorkspace* workspace, DesktopWindow* desktop_window);
		virtual void			OnDesktopWindowRemoved(OpWorkspace* workspace, DesktopWindow* desktop_window);
		virtual void			OnDesktopWindowOrderChanged(OpWorkspace* workspace) {}
		virtual void			OnDesktopWindowActivated(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL activate);
		virtual void			OnDesktopWindowAttention(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL attention) {}
		virtual void			OnDesktopWindowChanged(OpWorkspace* workspace, DesktopWindow* desktop_window);
		virtual void			OnDesktopWindowClosing(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL user_initiated);
		virtual void			OnDesktopGroupCreated(OpWorkspace* workspace, DesktopGroupModelItem& group, DesktopWindowCollectionItem& main, DesktopWindowCollectionItem& second) {}
		virtual void			OnDesktopGroupAdded(OpWorkspace* workspace, DesktopGroupModelItem& group) {}
		virtual void			OnDesktopGroupCreated(OpWorkspace* workspace, DesktopGroupModelItem& group) {}
		virtual void			OnDesktopGroupRemoved(OpWorkspace* workspace, DesktopGroupModelItem& group) {}
		virtual void			OnDesktopGroupDestroyed(OpWorkspace* workspace, DesktopGroupModelItem& group) {}
		virtual void			OnDesktopGroupChanged(OpWorkspace* workspace, DesktopGroupModelItem& group) {}
		virtual void			OnWorkspaceEmpty(OpWorkspace* workspace,BOOL has_minimized_window);
		virtual void			OnWorkspaceAdded(OpWorkspace* workspace) {}
		virtual void			OnWorkspaceDeleted(OpWorkspace* workspace) {}

		INT32					GetSelectedHotlistItemId( OpTypedObject::Type type );

		// DocumentWindowListener
		virtual void			OnPopup(DocumentDesktopWindow* document_window, const uni_char *url, const uni_char *name, int left, int top, int width, int height, BOOL3 scrollbars, BOOL3 location);
		virtual void			OnTransparentAreaChanged(DocumentDesktopWindow* document_window, int height);

		BOOL					BannerIsVisible();
#ifdef SUPPORT_VISUAL_ADBLOCK
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
		static void				OpenContentBlockDetails(DesktopWindow *parent, OpInputAction* action, OpVector<ContentFilterItem>& content_to_block);
#else
		static void				OpenContentBlockDetails(DesktopWindow *parent, OpInputAction* action, ContentBlockTreeModel& content_to_block);
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL
#endif // SUPPORT_VISUAL_ADBLOCK

		OpAddressDropDown*		GetAddressDropdown();
		OpSearchDropDown*		GetSearchDropdown();

		virtual void EnableTransparentSkins(BOOL enabled, BOOL top_only, BOOL force_update = FALSE);

		BOOL					HasThumbnailsVisible();
		void					EnsureMenuButton(BOOL enable);
		BOOL					HasTransparentSkins() { return m_transparent_skins_enabled; }


	    BOOL GetMenubarQuickMenuInfoItems(OpScopeDesktopWindowManager_SI::QuickMenuInfo &out);

		/**
		 * Change the fullscreen state of this window and all tabs inside it.
		 *
		 * @param go_fullscreen true to enter fullscreen, false to leave it.
		 * @param is_scripted   true if the mode change is triggered by Javascript, false if triggered by a keyboard shortcut or menu.
		 * @retval              true if the mode was successfully changed, otherwise false.
		 */
		bool SetFullscreenMode(bool go_fullscreen, bool is_scripted);

		/**
		 * Return if the current fullscreen mode was set from a script or not.
		 *
		 * @retval true if fullscreen mode was triggered by a script, false if triggered by the user.
		 */
		bool FullscreenIsScripted() { return m_fullscreen_is_scripted; }

		// DialogContextListener

		virtual void OnDialogClosing(DialogContext* context);

		//
		// DispatchableObject
		//

		/** 
		 * We want browser window to be initialized immediately because it blocks
		 * initialization of other windows.
		 *
		 * @retval -1 
		 */
		virtual int				GetPriority() const { return -1; }
		virtual OP_STATUS		InitHiddenInternals();

	private:

		BOOL					SetMenuBarVisibility(BOOL visible);
		BOOL					IsSmallBanner();
		BOOL					IsTallBanner();
		BOOL					ReopenPageL(OpSession* session, INT32 pos, BOOL check_if_available);
#ifdef NEW_BOOKMARK_MANAGER
		void					OpenCollectionManager();
#endif

	// ======== =========================================================================================

		BOOL					m_reset_hotlist;
#ifdef SUPPORT_MAIN_BAR
		OpToolbar*				m_main_bar;
#endif
		OpThumbnailPagebar*     m_page_bar;
		OpDragScrollbar*		m_drag_scrollbar;
		OpDragScrollbar*		m_side_drag_scrollbar;	// used on the sides
		OpStatusbar*			m_status_bar;
		OpSplitter*				m_splitter;
		OpWorkspace*			m_workspace;
	// OpWidget*				m_header;
		OpTimer*				m_timer;
#ifdef DEVTOOLS_INTEGRATED_WINDOW
		OpSplitter*				m_devtools_splitter;
		OpGroup*				m_top_section;
		OpWidget*				m_bottom_section;
		OpWorkspace*			m_devtools_workspace;
		BOOL					m_devtools_only;
#endif // DEVTOOLS_INTEGRATED_WINDOW

		FindTextManager*		m_find_text_manager;

		Hotlist*				m_hotlist;
		OpButton*				m_hotlist_toggle_button;
		HotlistDesktopWindow*	m_floating_hotlist_window;

		CyclePagesPopupWindow*	m_cycle_pages_popup;
		OpSession*				m_undo_session;
		OpAutoVector<BlockedPopup>	m_blocked_popups;

		FullscreenPopupController* m_fullscreen_popup;

		OpBar::Alignment		m_main_bar_alignment;

		BOOL					m_privacy_mode;

		int						m_transparent_top;
		int						m_transparent_bottom;
		int						m_transparent_left;
		int						m_transparent_right;
		BOOL					m_transparent_skins_enabled;
		BOOL					m_transparent_skins_enabled_top_only;
		BOOL					m_transparent_skin_sections_available;
		BOOL					m_transparent_full_window;		// TRUE of the full window is transparent due to skin option
		bool					m_fullscreen_is_scripted;		// TRUE if fullscreen mode was triggered by a script
};


/***********************************************************************************
 **
 **	CyclePagesPopupWindow
 **
 ***********************************************************************************/

class CyclePagesPopupWindow : public DesktopWindow
{
	public:

								CyclePagesPopupWindow(BrowserDesktopWindow* browser_window);
								~CyclePagesPopupWindow();
	
		OP_STATUS               Init(WindowCycleType type);
		void					AddWindow(DesktopWindow* desktop_window);

		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual void 			OnShow(BOOL show);

		// OpTypedObject

		virtual Type			GetType() {return WINDOW_TYPE_PAGE_CYCLER;}
		virtual const char*		GetWindowName() {return "Cycler Window";}
		virtual const char*     GetInputContextName() {return "Cycler Window";}

	private:

#ifdef SUPPORT_GENERATE_THUMBNAILS
		void SetImages();

		OpButton*				m_image_button;
#endif // SUPPORT_GENERATE_THUMBNAILS

		OpToolbar*				m_pages_list;
		BrowserDesktopWindow*	m_browser_window;
		BOOL 					m_has_activated_window;
};

#endif // BROWSER_DESKTOP_WINDOW_H
