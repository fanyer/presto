/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_ADDRESS_DROPDOWN_H
#define OP_ADDRESS_DROPDOWN_H

#include "adjunct/quick_toolkit/widgets/OpTreeViewDropdown.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "modules/util/adt/oplisteners.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/widgets/OpButton.h"
#include "adjunct/desktop_util/visited_search_threaded/VisitedSearchThread.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"
#include "adjunct/desktop_util/search/search_suggest.h"
#include "adjunct/quick/widgets/OpProtocolButton.h"
#include "modules/pi/network/OpHostResolver.h"
#include "modules/regexp/include/regexp_api.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/quick_toolkit/contexts/DialogContextListener.h"

class OpProgressField;
class OpTrustAndSecurityButton;
#ifdef SUPPORT_START_BAR
class PopupWindow;
#endif // SUPPORT_START_BAR
class Bookmark;
class HistoryModelPage;
class HistoryAutocompleteModel;
class HistoryAutocompleteModelItem;
class SimpleAutocompleteItem;
class SearchSuggestionAutocompleteItem;
class OpAddressbarOverlay;
class HistorySeparatorModelItem;
class WordHighlighter;
class AddressbarOverlayController;
class FavoritesOverlayController;
class FavoritesCompositeListener;
class URLFavMenuButton;

/**
 * @brief A autocompleting dropdown for urls
 * @author Patricia Aas
 *
 * Used wherever the user needs to type an address
 */

/***********************************************************************************
**
** The long and complicated story on how the addressfield finds the right tab
** - in baby steps for my convenience
**
** The problem:
** 
** A) The addressfield should only listen to its own tab when placed on a tab
** B) The addressfield should listen to all tabs when placed in the main/status bar
**
** So which tab should the addressfield reflect the state of?
**
** The game is: Find the closest DocumentDesktopWindow and make friends
**
** This is done by using the inputmanager to look for the closest DocumentDesktopWindow,
** if you find one then it is your friend (it basically means you are on a tab, because
** otherwise you would not have found one). If you don't find one, then you are supposed
** to listen to the currently active DocumentDesktopWindow, so that you query for from
** Application. (See DocumentDesktopWindowSpy::UpdateTargetDocumentDesktopWindow for
** how this magic is implemented)
**
** The details are as follows:
**
** 0) The OpAddressDropdown has an OpTabCursor
** 1) The OpAddressDropdown is an OpInputContext
** 2) The OpTabCursor is a DocumentDesktopWindowSpy
** 3) The DocumentDesktopWindowSpy::m_input_context is set to the OpAddressDropdown
** 4) The DocumentDesktopWindowSpy::SpyInputStateListener::m_spy is the OpTabCursor
** 5) If DocumentDesktopWindowSpy::m_input_context is set, then
**    DocumentDesktopWindowSpy::SpyInputStateListener::m_input_state is Enabled
** 6) DocumentDesktopWindowSpy::SpyInputStateListener::m_input_state is an OpInputState
** 7) An OpInputState is a list
** 8) If an OpInputState is Enabled then it is added to the inputmanager
** 9) If an OpInputState is Disabled then it is removed from the inputmanage
** 10) SpyInputStateListener is a OpInputStateListener
** 11) OpInputStateListener has only one callback OnUpdateInputState
** 12) SpyInputStateListener::OnUpdateInputState calls DocumentDesktopWindowSpy::UpdateTargetDocumentDesktopWindow
** 13) DocumentDesktopWindowSpy::UpdateTargetDocumentDesktopWindow will call 
**     OpInputManager::GetTypedObject which will search for all DocumentDesktopWindows
**     "in a given input context path"
**
** Now why we need to revise what is our current tab every time OpInputManager::SyncInputStates()
** is called, is still a mystery to me though.
**
***********************************************************************************/

/***********************************************************************************
**
**	OpTabCursor is a cursor that will follow the tab associated with the InputContext
**  supplied in the HookUpCursor call and notify its listeners about any events that
**  could be relevant for updating the ui.
**
**  The OpAddressDropDown uses it to keep track of the tab/tabs it is interested in.
**  NOTE: When the OpAddressDropDown is on the mainbar it is interested in all tabs.
**        When it is on a page then it is only interested it its own page.
**
***********************************************************************************/

class OpTabCursor
{
public:
	
//---------------------------------------------------------------------------
// Public listener class:
//---------------------------------------------------------------------------

    class Listener
	{
	public:

		/**
		 * The tab has altered in some way - the UI should be updated
		 */
		virtual void OnTabInfoChanged() = 0;

		/**
		 * The current tab is loading
		 */
		virtual void OnTabLoading() = 0;

		/**
		 * The document in the current tab changed
		 */
		virtual void OnTabChanged() = 0;

		/**
		 * The another tab was switched to
		 *
		 * @param old_target_window - the previous target window
		 * @param new_target_window - the current target window
		 */
		virtual void OnTabWindowChanging(DocumentDesktopWindow* old_target_window,
										 DocumentDesktopWindow* new_target_window) = 0;
		virtual void OnPageWindowChanging(OpWindowCommander* old_window_commander,
										  OpWindowCommander* new_window_commander) = 0;

		/**
		 * This tab is closing
		 *
		 * @param desktop_window - the tab that is closing
		 */
		virtual void OnTabClosing(DocumentDesktopWindow* desktop_window) = 0;
		virtual void OnPageClosing(OpWindowCommander* window_commander) = 0;

		/**
		 * A tab had a url change
		 *
		 * @param document_window - that had its url changed
		 * @param url             - that it was changed to
		 */
		virtual void OnTabURLChanged(DocumentDesktopWindow* document_window, const OpStringC & url) = 0;
		virtual void OnPageURLChanged(OpWindowCommander* window_commander, const OpStringC & url) = 0;

	protected:
		virtual ~Listener(){}
	};

	enum CursorType
	{
		OLD_TYPE,
		PAGE_TYPE
	};

	virtual ~OpTabCursor() {}

	virtual CursorType GetCursorType() = 0;

#ifdef QUICK_TOOLKIT_CAP_HAS_PAGE_VIEW
	virtual OP_STATUS SetPage(OpPage& page) { return OpStatus::ERR_NOT_SUPPORTED; }
#endif // QUICK_TOOLKIT_CAP_HAS_PAGE_VIEW

	/**
	 * @param listener that is to be added
	 * @return OpStatus::OK if successful in adding listener
	 */
	virtual OP_STATUS AddListener(OpTabCursor::Listener *listener) = 0;

	/**
	 * @param listener that is to be removed
	 * @return OpStatus::OK if successful in removing listener
	 */
	virtual OP_STATUS RemoveListener(OpTabCursor::Listener *listener) = 0;

	/**
	 * Hook up the cursor to the right tab/tabs by supplying
	 * your input context
	 *
	 * @param input_context - supplied by the user of the OpTabCursor
	 */
	virtual void HookUpCursor(OpInputContext * input_context) = 0;

	/**
	 * UnHook the OpTabCursor
	 *
	 * @param send_change - TRUE if you want a OnTabWindowChanging callback
	 */
	virtual void UnHookCursor(BOOL send_change) = 0;

	virtual OP_STATUS SetWorkspace(OpWorkspace* workspace) = 0;
	virtual BOOL HasWorkspace() = 0;
	virtual BOOL IsTargetWindow(DocumentDesktopWindow* document_window) = 0;
	virtual BOOL IsTargetWindow(OpWindowCommander* windowcommander) = 0;
	virtual OpWindowCommander* GetWindowCommander() = 0;
	virtual OP_STATUS GetSecurityInformation(OpString& text) = 0;
	virtual const OpWidgetImage* GetWidgetImage() = 0;
	virtual const OpWidgetImage* GetWidgetImage(OpWindowCommander* windowcommander) = 0;
};

class OpOldCursor :
	public OpTabCursor,
	private DocumentDesktopWindowSpy,
	private OpWorkspace::WorkspaceListener
{
public:

//  -------------------------
//  Public member functions:
//  -------------------------

	/**
	 * Constructor
	 */
	OpOldCursor();

	/**
	 * Destructor
	 */
	virtual ~OpOldCursor();

	// == Implement OpTabCursor ======================

	virtual CursorType GetCursorType() { return OpTabCursor::OLD_TYPE; }
	virtual OP_STATUS AddListener(OpTabCursor::Listener *listener);
	virtual OP_STATUS RemoveListener(OpTabCursor::Listener *listener);
	virtual void HookUpCursor(OpInputContext * input_context);
	virtual void UnHookCursor(BOOL send_change);
	virtual OP_STATUS SetWorkspace(OpWorkspace* workspace);
	virtual BOOL HasWorkspace() { return m_workspace != NULL; }
	virtual BOOL IsTargetWindow(DocumentDesktopWindow* document_window);
	virtual BOOL IsTargetWindow(OpWindowCommander* windowcommander);
	virtual OpWindowCommander* GetWindowCommander();
	virtual OP_STATUS GetSecurityInformation(OpString& text);
	virtual const OpWidgetImage* GetWidgetImage();
	virtual const OpWidgetImage* GetWidgetImage(OpWindowCommander* windowcommander);

private:

//  -------------------------
//  Private member functions:
//  -------------------------

	// == DocumentDesktopWindowSpy ======================

	virtual void OnStartLoading(DocumentDesktopWindow* document_window);

	virtual void OnLoadingFinished(DocumentDesktopWindow* document_window,
								   OpLoadingListener::LoadingFinishStatus status,
								   BOOL was_stopped_by_user);

	virtual void OnTargetDocumentDesktopWindowChanged(DocumentDesktopWindow* target_window);

	virtual void OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);

	virtual void OnUrlChanged(DocumentDesktopWindow* document_window, const uni_char* url);

	virtual void OnDesktopWindowChanged(DesktopWindow* desktop_window);

	virtual void OnSecurityModeChanged(DocumentDesktopWindow* document_window);

	virtual void OnTrustRatingChanged(DocumentDesktopWindow* document_window);

	virtual void OnOnDemandPluginStateChange(DocumentDesktopWindow* document_window);

	// == OpWorkspace::WorkspaceListener ================

	virtual void OnDesktopWindowAdded(OpWorkspace* workspace, DesktopWindow* desktop_window);
	virtual void OnDesktopWindowRemoved(OpWorkspace* workspace, DesktopWindow* desktop_window);
	virtual void OnDesktopWindowOrderChanged(OpWorkspace* workspace) {}
	virtual void OnDesktopWindowActivated(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL activate);
	virtual void OnDesktopWindowAttention(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL attention) {}
	virtual void OnDesktopWindowChanged(OpWorkspace* workspace, DesktopWindow* desktop_window) {}
	virtual void OnDesktopWindowClosing(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL user_initiated) {}
	virtual void OnDesktopGroupAdded(OpWorkspace* workspace, DesktopGroupModelItem& group) {}
	virtual void OnDesktopGroupCreated(OpWorkspace* workspace, DesktopGroupModelItem& group) {}
	virtual void OnWorkspaceEmpty(OpWorkspace* workspace,BOOL has_minimized_window) {}
	virtual void OnWorkspaceAdded(OpWorkspace* workspace) {}
	virtual void OnWorkspaceDeleted(OpWorkspace* workspace);
	virtual void OnDesktopGroupRemoved(OpWorkspace* workspace, DesktopGroupModelItem& group) {}
	virtual void OnDesktopGroupDestroyed(OpWorkspace* workspace, DesktopGroupModelItem& group) {}
	virtual void OnDesktopGroupChanged(OpWorkspace* workspace, DesktopGroupModelItem& group) {}

	// == Broadcast functions ======================

	void BroadcastTabInfoChanged();
	void BroadcastTabLoading();
	void BroadcastTabChanged();
	void BroadcastTabWindowChanging(DocumentDesktopWindow* old_target_window,
									DocumentDesktopWindow* new_target_window);
	void BroadcastTabClosing(DocumentDesktopWindow* desktop_window);
	void BroadcastTabURLChanged(DocumentDesktopWindow* document_window, const OpStringC & url);

//  -------------------------
//  Private member variables:
//  -------------------------

	// The listeners listening to this OpTabCursor
	OpListeners<OpTabCursor::Listener> m_listeners;

	// The current target window - when the active window is not
	// document window m_current_target is NULL and GetTargetDocumentDesktopWindow()
	// will be the last activated document window
	DocumentDesktopWindow*	m_current_target;

	// The workspace the addressfield is listening to
	// this will only be non-null for global adressfiles (on status or main bar)
	OpWorkspace* m_workspace;
};

/***********************************************************************************
**
**	OpPageCursor
**
***********************************************************************************/

#ifdef QUICK_TOOLKIT_CAP_HAS_PAGE_VIEW
class OpPageCursor : 
	public OpTabCursor,
	private OpPageListener
{
public:

	OpPageCursor();

	// Implement OpPageListener interface
	virtual void OnPageDestroyed(OpPage* page) { m_page = 0; OpPageListener::OnPageDestroyed(page); }
	virtual void OnPageDocumentIconAdded(OpWindowCommander* commander);
	virtual void OnPageStartLoading(OpWindowCommander* commander);
	virtual void OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL was_stopped_by_user);
	virtual void OnPageUrlChanged(OpWindowCommander* commander, const uni_char* url);
	virtual void OnPageTitleChanged(OpWindowCommander* commander, const uni_char* title);
	virtual void OnPageSecurityModeChanged(OpWindowCommander* commander, OpDocumentListener::SecurityMode mode, BOOL inline_elt);

#ifdef TRUST_RATING
	virtual void OnPageTrustRatingChanged(OpWindowCommander* commander, TrustRating new_rating);
#endif // TRUST_RATING

	// == Implement OpTabCursor ======================

	virtual OP_STATUS SetPage(OpPage& page);
	virtual CursorType GetCursorType() { return OpTabCursor::PAGE_TYPE; }
	virtual OP_STATUS AddListener(OpTabCursor::Listener *listener) { m_listener = listener; return OpStatus::OK; }
	virtual OP_STATUS RemoveListener(OpTabCursor::Listener *listener) { m_listener = NULL; return OpStatus::OK; }
	virtual void HookUpCursor(OpInputContext * input_context) {}
	virtual void UnHookCursor(BOOL send_change) {}
	virtual OP_STATUS SetWorkspace(OpWorkspace* workspace) { return OpStatus::ERR; }
	virtual BOOL HasWorkspace() { return FALSE; }
	virtual BOOL IsTargetWindow(DocumentDesktopWindow* document_window);
	virtual BOOL IsTargetWindow(OpWindowCommander* windowcommander);
	virtual OpWindowCommander* GetWindowCommander();
	virtual OP_STATUS GetSecurityInformation(OpString& text);
	virtual const OpWidgetImage* GetWidgetImage();
	virtual const OpWidgetImage* GetWidgetImage(OpWindowCommander* windowcommander);

private:

	OpTabCursor::Listener* m_listener;
	OpPage* m_page;
};
#endif // QUICK_TOOLKIT_CAP_HAS_PAGE_VIEW

/***********************************************************************************
**
**	OpAddressFieldButton
**
***********************************************************************************/

class OpAddressFieldButton : public OpButton
{
public:
	OpAddressFieldButton() : OpButton(OpButton::TYPE_CUSTOM) {}

	virtual BOOL IsInputContextAvailable(FOCUS_REASON reason) { return FALSE; }
};

/***********************************************************************************
**
**	TabTextContent
**
***********************************************************************************/

template<class T>
class TabTextContent
{
public:

	TabTextContent() : m_owner(NULL) {}

	void SetOwner(T* owner) { m_owner = owner; }

	/**
	 * Save the text in the urlfield if it is text entered by the user, assume it is
	 * a url that the user is still editing
	 *
	 */
	OP_STATUS PushTab(OpWindowCommander* windowcommander)
	{
		if(!windowcommander)
			return OpStatus::ERR;

		OpString text;
		RETURN_IF_ERROR(m_owner->GetText(text));

		if(m_owner->TextIsNotUrl(text, windowcommander))
			return AddContent(windowcommander, text);

		return OpStatus::OK;
	}

	/**
	 * Check if we stored some text for this tab - if we did place that text in the
	 * urlfield.
	 * @return OpStatus::OK if text was non-empty and set on the owner
	 */
	OP_STATUS PopTab(OpWindowCommander* windowcommander)
	{
		if(!windowcommander)
			return OpStatus::ERR;
		
		OpString text;
		RETURN_IF_ERROR(RemoveContent(windowcommander, text));
		
		if(text.HasContent())
		{
			RETURN_IF_ERROR(m_owner->SetText(text.CStr()));
		}

		return text.HasContent() ? OpStatus::OK : OpStatus::ERR;
	}

	/**
	 *
	 * UpdateContent
	 *
	 */
	OP_STATUS UpdateContent(OpWindowCommander* windowcommander, const OpStringC & text)
	{
		OpString old_text;
		RETURN_IF_ERROR(RemoveContent(windowcommander, old_text));
		
		return AddContent(windowcommander, text);
	}

	/**
	 *
	 * AddContent
	 *
	 */
	OP_STATUS AddContent(OpWindowCommander* windowcommander, const OpStringC & text)
	{
		OpString *data = OP_NEW(OpString, ());
		
		if (!data)
			return OpStatus::ERR_NO_MEMORY;
		
		OpAutoPtr<OpString> data_ap(data);

		RETURN_IF_ERROR(data->Set(text.CStr()));
		RETURN_IF_ERROR(m_content_per_target.Add(windowcommander, data));
		
		data_ap.release();

		return OpStatus::OK;
	}

	/**
	 *
	 * RemoveContent
	 *
	 */
	OP_STATUS RemoveContent(OpWindowCommander* windowcommander, OpString& text)
	{
		if(!m_content_per_target.Contains(windowcommander))
			return OpStatus::OK;
		
		OpString *data;
		RETURN_IF_ERROR(m_content_per_target.Remove(windowcommander, &data));

		OP_STATUS err = text.TakeOver(*data);
		OP_DELETE(data);
		return err;
	}

private:

	// The content of the url field for each window :
	OpAutoPointerHashTable<OpWindowCommander, OpString>	m_content_per_target;
	T* m_owner;
};

// Containing the logic to decide which protocols should 
class AddressHighlight
{
public:
	AddressHighlight() : m_highlight(FALSE){}

	BOOL HasContent() {return m_original_address.HasContent();}
	BOOL GetHighlight() {return m_highlight;}
	BOOL Compare(OpStringC url);
	OpStringC& GetFullAddress() {return m_original_address;}
	OpStringC& GetProtocol() {return m_protocol;}
	OpStringC& GetQueryField() {return m_query_field;}

	// This is the text that is actually shown then address field is not focused
	void GetShowAddress(OpString& address);
	
	void Clear();
	void SetAddress(OpStringC url);

	OpString m_short_address;	// The address without protocol and query string and trailing '/' . e.g. opera.com

private:
	void SetAddressForExtensions();

	OpString m_protocol;		// e.g. http:// or opera:
	OpString m_query_field;		// e.g. "?a=1"
	OpString m_original_address; // The full address that will be shown when focused. e.g. http://opera.com/
	BOOL m_highlight;			// highlight or not
};


/***********************************************************************************
**
**	OpAddressDropDown
**
***********************************************************************************/

class OpAddressDropDown :
	public OpTreeViewDropdown,
	public OpTabCursor::Listener,
	public OpDelayedTriggerListener,
	public OpHostResolverListener,
	public SearchSuggest::Listener,
	public OpPrefsListener, // Listen to HideURLParameter
	public DesktopHistoryModelListener,
	public DialogContextListener
{
#ifdef SUPPORT_START_BAR
	friend class PopupWindow;
#endif // SUPPORT_START_BAR
	friend class TabTextContent<OpAddressDropDown>;

public:
	struct InitInfo
	{
		InitInfo();

		int m_max_lines;
		bool m_show_dropdown_on_activation;
		bool m_invoke_action_on_click;
		bool m_open_page_in_tab;
		bool m_tab_mode;
		bool m_shall_show_addr_bar_badge;
		bool m_use_display_url;
		bool m_show_search_suggestions;
		bool m_show_speed_dial_pages;
		OpString m_url_string;
	};

	static OP_STATUS Construct(OpAddressDropDown** obj);

#ifdef QUICK_TOOLKIT_CAP_HAS_PAGE_VIEW
	OP_STATUS SetPage(OpPage& page);
#endif // QUICK_TOOLKIT_CAP_HAS_PAGE_VIEW


	/**
	 * Setting adjustment - choose whether you want ctrl + enter to open in tab
	 * or not. 
	 *
	 * @param value - if true then it will open in tab. 
	 */
	void SetOpenInTab(BOOL value) { m_open_page_in_tab = value; }

	/**
	 * An address bar will by default assume it exists in a tabbed enviroment
	 * If that is not the case, use this function. You should only need to use
	 * an argument value of FALSE outside the class
	 *
	 * @param mode Enable tabbed mode if TRUE, otherwise FALSE
	 *
	 * @return OpStatus::OK on success, otherwise an error code
	 */
	OP_STATUS SetTabMode(BOOL mode);
	
	OpWindowCommander* GetWindowCommander() {return m_tab_cursor->GetWindowCommander();}

	BOOL GetMisMatched() { return m_url_page_mismatch; }

	OP_STATUS GetFullAddress(OpString& text);
	
	// Only valid when the query string and protocol field are hidden
	void GetQueryField(OpString& text);
	void GetProtocolField(OpString& text);

	void GetHighlightDomain(OpString& text);

	/**
	 * Emulate a paste action in the edit field without modifying the
	 * edit field content. Incoming text will be inserted into a copy
	 * of the edit field text and then returned.
	 *
	 * If the edit widget does not holds focus then the incoming text is
	 * returned as it is because moving the focus to the edit field would
	 * select all text. The subsequent paste would then remove that
	 * selected text and replace it with the incomming.
	 *
	 * @param text Text to be inserted. On return the result of the insert
	 *        operation
	 *
	 * @return OpStatus::OK on success, otherwise OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS EmulatePaste(OpString& text);

	// DialogContextListener
	virtual void OnDialogClosing(DialogContext* context);

	// == OpTabCursor::Listener ======================

	virtual void OnTabInfoChanged();

	virtual void OnTabLoading();

	virtual void OnTabChanged();

	virtual void OnTabWindowChanging(DocumentDesktopWindow* old_target_window,
									 DocumentDesktopWindow* new_target_window);

	virtual void OnTabClosing(DocumentDesktopWindow* desktop_window);

	virtual void OnTabURLChanged(DocumentDesktopWindow* document_window, const OpStringC & url);

	// == OpPageCursor interface ======================

	virtual void OnPageWindowChanging(OpWindowCommander* old_window_commander,
									  OpWindowCommander* new_window_commander);

	virtual void OnPageClosing(OpWindowCommander* window_commander);

	virtual void OnPageURLChanged(OpWindowCommander* window_commander, const OpStringC & url);

	// == OpTreeViewDropdown ======================

	virtual void OnClear();
	virtual BOOL OnBeforeInvokeAction(BOOL invoked_on_user_typed_string);
	virtual void OnInvokeAction(BOOL invoked_on_user_typed_string);
	virtual void ShowDropdown(BOOL show);
	virtual OP_STATUS GetTextForSelectedItem(OpString& item_text);
	virtual void OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);

	// == OpWidget ======================

	virtual void			GetPadding(INT32* left,
									   INT32* top,
									   INT32* right,
									   INT32* bottom);

	virtual void			OnDeleted();

	virtual void			OnAdded();
	virtual void			OnLayout();
	virtual OpWidget*		GetWidgetByName(const char* name);

	virtual void OnResize(INT32* new_w, INT32* new_h);
	virtual OP_STATUS SetText(const uni_char* text);

   	// == OpInputContext ======================

	virtual void			OnKeyboardInputGained(OpInputContext* new_input_context,
												  OpInputContext* old_input_context,
												  FOCUS_REASON reason);

	virtual void			OnKeyboardInputLost(OpInputContext* new_input_context,
												OpInputContext* old_input_context,
												FOCUS_REASON reason);

	// == OpWidgetListener ======================

	virtual void			OnDropdownMenu(OpWidget *widget,
										BOOL show);

	virtual void			OnDragStart(OpWidget* widget,
										INT32 pos,
										INT32 x,
										INT32 y);

	virtual void			OnChange(OpWidget *widget,
									 BOOL changed_by_mouse);

	virtual void			OnMouseEvent(OpWidget *widget,
							  INT32 pos,
							  INT32 x,
							  INT32 y,
							  MouseButton button,
							  BOOL down,
							  UINT8 nclicks);

	virtual void OnMouseMove(OpWidget *widget, const OpPoint &point);

	virtual void OnMouseLeave(OpWidget *widget);

	// == DesktopHistoryModelListener ======================

	virtual void			OnDesktopHistoryModelItemDeleted(HistoryModelItem* item);

	// == OpTreeModelItem ======================

	virtual Type			GetType() {return WIDGET_TYPE_ADDRESS_DROPDOWN;}

	// == OpInputStateListener ======================

	virtual void			OnUpdateActionState();

	// == OpInputContext ======================

	virtual BOOL			OnInputAction(OpInputAction* action);

	virtual const char*		GetInputContextName() {return "Address Dropdown Widget";}

	// == MessageObject ======================

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	// == OpDelayedTriggerListener ======================

	void	OnTrigger(OpDelayedTrigger*);

	// == SearchSuggest::Listener ======================
	virtual void OnQueryStarted() { m_search_suggest_in_progress = TRUE; };
	virtual void OnQueryComplete(const OpStringC& search_id, OpVector<SearchSuggestEntry>& entries);
	virtual void OnQueryError(const OpStringC& search_id, OP_STATUS error) { ++m_query_error_counter; m_search_suggest_in_progress = FALSE; }

	// == OpPrefsListener =============================
	virtual void PrefChanged(OpPrefsCollection::Collections /* id */, int /* pref */,int /* newvalue */);

	// Get the how much width the current protocol text occupies
	UINT GetProtocolWidth(); 

	OpProtocolButton::ProtocolStatus GetProtocolStatus() { return m_protocol_button ? m_protocol_button->GetStatus() : OpProtocolButton::NotSet;}

	OpProtocolButton* GetProtocolButton() {return m_protocol_button;}

	void GetSecurityInformation(OpString& text) { m_tab_cursor->GetSecurityInformation(text); }

	// == Display Url functions (DSK-336690) =============================
	
	// When selecting a item from the dropdown treeview, use display url if there is any
	void SetUseDisplayUrl(BOOL value) { m_use_display_url = value; }
	BOOL GetUseDisplayUrl() { return m_use_display_url; }
	// If this value is not empty and m_use_display_url is TRUE, the address field is now displaying a *Display Url*
	// but this is the actual url
	OpStringC GetActualUrl() { return m_actual_url; }

	void	EnableFavoritesButton(bool show_fav_btn);

	void UpdateFavoritesFG(bool reflow);

	void InitAddressDropDown(OpAddressDropDown::InitInfo &info);

	// These need to be public for selftest. You should normally not use them outside OpAddressDropDown!
	HistoryAutocompleteModel * GetModel();
	HistoryAutocompleteModelItem * GetSelectedItem(int * position = NULL);

	/**
	 * Close overlay dialog if it is displayed.
	 */
	void CloseOverlayDialog();

	/**
	 * Displays overlay dialog with information about page security and permissions.
	 * @param info vector with page permissions received from core after call of GetUserConsent
	 */
	void DisplayOverlayDialog(const OpVector<OpPermissionListener::ConsentInformation>& info);

	/**
	 * Returns information about address bar's position in relation to the main window
	 * @return true if address bar is located on the bottom of the main window, false otherwise
	 */
	bool IsOnBottom() const;


	// For scope
	OP_STATUS ListPopupWindowWidgets(OpVector<OpWidget> &widgets);
	OP_STATUS ListWidgets(OpWidget* widget, OpVector<OpWidget> &widgets);

	void GetShowAddress(OpString& address) { return m_address.GetShowAddress(address); }
	void EnableAddressBarBadge(BOOL shall_show_addr_bar_badge) { m_shall_show_addr_bar_badge = shall_show_addr_bar_badge;}
	
	// Show the protocol prefix if it was hidden
	void RestoreOriginalAddress();

	/**
	 *	Normally address edit field is not updated from the "outside" (OnPageURLChanged) when it has focus .
	 *	That's especially for the purpose of not overwriting user input, when page executes
	 *	auto-reload. However we should allow to overwrite user input once user presses
	 *	"back", "next" or "reload" button from the addressbar toolbar. (DSK-109894)
	 */
	void SetIgnoreEditFocus() { m_ignore_edit_focus = true; }

	// Overriding OpTimerListener
	virtual void OnTimeOut(OpTimer* timer);

	/**
	 * Block the possibility to interact with edit field for a while
	 * DSK-361401 fix
	 */
	void BlockFocus();

	virtual OP_STATUS GetText(OpString& text);

	/**
	 * Show/Hide notification text after add/remove bookmark/speeddial
	 */
	void AnimateBookmarkNotification(bool show, bool added);

private:
	OpAddressDropDown();

	virtual ~OpAddressDropDown();

	void OnShowFavoritesDialog();

	void OnCloseFavoritesDialog();

	bool ActivateShowAllItem(bool mouse_invoked);

	bool HasSimilarItem(const HistoryAutocompleteModelItem* item, const OpVector<HistoryAutocompleteModelItem>& items);

	/*
	 * Prepares drop down model and displays it
	 * if refresh_history  is true, m_history_item vector is regenerated
	 */
	void PopulateDropdown(bool refresh_history = true);

	/*
	 * Combines history entries from available sources (bookmarks, history module, search engine module)
	 * into m_history_items vector.
	 */
	void PrepareHistory(const OpString& current_text);

	bool AddSearchForItem(const OpStringC& current_text);

	/*
	 * Adds expander ("Show all items") to dropdown. When expander is clicked dropdown should be expanded
	 * to contain all entries matching typed text.
	 */
	void AddExpander();

	void AddPreviousSearches(const OpStringC& search_text);

	/*
	 * Adds opera pages matching typed text to a dropdown.
	 *
	 * @param current_text typed text. Caller is responsible for assuring that given text
	 *        is part of opera page (for now it's either opera:... or o:...).
	 */
	void AddOperaPages(const OpString& current_text);

	/*
	 * If current_text is a number of existing speed dial this method adds new entry
	 * which points to speed dial's address to the dropdown and returns true. If
	 * it isn't false is returned.
	 */
	bool AddSpeedDialAddress(const OpString& current_text);

	//--------------------------------------------------
	// Search functions 
	//--------------------------------------------------

	bool ShouldQuerySearchSuggestions(const OpStringC& current_text);

	/*
	 * Starts search for typed text using given search template. When search is successfuly complete
	 * OnQueryComplete() will be called.
	 *
	 *  @param search_phrase phrase which we should look for (typed text)
	 *  @param search_template search engine which should be used for a search. If NULL default
	 *         search engine will be used
	 */
	void QuerySearchSuggestions(const OpStringC& search_phrase, SearchTemplate* search_template = NULL);

	/*
	 * Starts search for typed text in search_engine module.
	 * Result will arrive via HandleCallback function.
	 */
	void QuerySearchEngineModule(const OpStringC& current_text);

	//--------------------------------------------------
	// Bookmarks :
	//--------------------------------------------------
	void GetBookmarksNicknames(const OpVector<HistoryAutocompleteModelItem>& nicks, OpVector<HistoryAutocompleteModelItem>& result);
	void GetBookmarks(const OpString& current_text, OpVector<HistoryAutocompleteModelItem>& result);

	BOOL MatchAndHighlight(Bookmark* bookmark, HistoryModelPage* page, WordHighlighter& word_highlighter);

	//--------------------------------------------------
	// Search engine module functions :
	//--------------------------------------------------

	void HandleSearchEngineModuleCallback(const OpStringC & search_text, OpAutoVector<VisitedSearch::SearchResult>* result);

	//--------------------------------------------------
	// UI update functions :
	//--------------------------------------------------

	void UpdateButtons(BOOL new_url = FALSE);
	void SetMisMatched() { m_url_page_mismatch = TRUE; }
	BOOL TextIsNotUrl(const OpStringC& text, OpWindowCommander* windowcommander);

	//--------------------------------------------------
	// Query UI placement :
	//--------------------------------------------------

	// Check if the addressfield belongs to a DocumentDesktopWindow, vs being on the status/main bar
	BOOL BelongsToADocumentDesktopWindow();

	// Forward the addressfields workspace to the cursor, so that it can listen to it
	void ForwardWorkspace();

	//--------------------------------------------------
	// Delete functions :
	//--------------------------------------------------

	template<typename T>
	void DeleteItems(OpVector<T>& items_vector)
	{
		while(items_vector.GetCount())
		{
			GenericTreeModelItem* item = items_vector.Remove(0);
			OP_DELETE(item);
		}
	}

	/** Removes HistoryModelItems related to the given URL:
	 *      - item which contains same url as the given,
	 *      - item which contains http or https variant of the given url,
	 *      - item which contains the given url without "www.".
	 *
	 * @param[in] url Url for which related HistoryModelItems will be deleted.
	 */
	void RemovePossibleContentSearchItems(OpStringC url);

	/** Delete the item both from the UI list, and in the source (history in core) */
	void DeleteItem(HistoryAutocompleteModelItem *item, BOOL call_on_change);

	/** Helper for DeleteItem. Deletes items from direct history, which
	 *  reffers to given address. Use DeleteItem instead!
	 *
	 *  @param[in] full_address The address for which all matching records in
	 *                          direct history will be removed.
	 */
	void DeleteFromDirectHistory(OpStringC full_address);

	/** Helper for DeleteItem. Use DeleteItem instead! */
	void DeleteHistoryModelItem(const uni_char *str);

	//--------------------------------------------------
	// Initialization functions :
	//--------------------------------------------------

	OP_STATUS InitializeButton(OpButton ** button,
							   const char * action_string,
							   const uni_char * button_text);

	OP_STATUS InitializeProgressField();

	OP_STATUS InitializeBookmarkMenu();

	OP_STATUS InitializeProtocolButton();

	OP_STATUS InitializeEditField();

	//--------------------------------------------------
	// Start bar specific methods :
	//--------------------------------------------------
#ifdef SUPPORT_START_BAR
	void CreatePopupWindow();
	void ShowPopupWindow(BOOL show = TRUE);
	void PlacePopupWindow();
	void DestroyPopupWindow(BOOL close_immediately = FALSE);
#endif // SUPPORT_START_BAR

	void UpdateProtocolStatus();

	inline void SetModel(HistoryAutocompleteModel * tree_model, BOOL items_deletable);

	/*
	 * Gives access to current text typed by user in address bar's edit field.
	 * Returned value (first parameter) does not include quick completion.
	 */
	OP_STATUS GetTypedText(OpString& typed_text);

	bool IsOperaPage(const OpString& address) const;

	void StartIntranetTest(const uni_char *address);
	uni_char* DeduceProtocolOrSubdomain(const OpString& current_text, uni_char* address);
	virtual void OnHostResolved(OpHostResolver* host_resolver);
	virtual void OnHostResolverError(OpHostResolver* host_resolver, OpHostResolver::Error error);

	void StartSearchSuggest();
	OP_STATUS GetFormattedSuggestedSearch(SearchSuggestionAutocompleteItem* suggest_item, OpString& formated_string);	// get the suggestion formatted and suitable to show in the edit field

	// Update the highlight status of the url when focus change, hovering etc.
	void UpdateAddressFieldHighlight(BOOL mouse_leave = FALSE, BOOL no_delay = FALSE);


	// Show tips and update ini if required conditions are met.
	//void ShowTips(OpVector<HistoryAutocompleteModelItem> & items, const SearchTemplate *search);

	bool IsAddressInBookmarkList();
	bool IsAddressInSpeedDialList();
	bool MaskUrlFromDisplay(const uni_char* url);
	bool ShallMaskUrlFromDisplay(const uni_char* url);


	//--------------------------------------------------
	// Ui widgets :
	//--------------------------------------------------
	
	// Always correspond to the current page, not what in the address box
	AddressHighlight		m_address;

	OpHostResolver*			m_host_resolver;
	OpString				m_host_resolve_address;
	OpProgressField*        m_progress_field;
	URLFavMenuButton*       m_url_fav_menu;
	OpButton*               m_bookmark_status_label;
	OpButton*               m_rss_button;
	OpButton*               m_ondemand_button;
	OpButton*               m_bookmark_button;
	OpButton*               m_dropdown_button;
	OpProtocolButton*		m_protocol_button;
#ifdef SUPPORT_START_BAR
	PopupWindow*			m_popup_window;	// the start bar
#endif // SUPPORT_START_BAR

	//--------------------------------------------------
	// Security :
	//--------------------------------------------------

	// Member used to flag whether the url in the address bar matches the page or not.
	// When it doesen't the security button must not be shown, as it may cause confusion.
	// Bug 226176
	BOOL m_url_page_mismatch;

	//--------------------------------------------------
	// Memory :
	//--------------------------------------------------

	// combined items from bookamrks manager, "history" and "search_engine" module
	OpVector<HistoryAutocompleteModelItem> m_history_items; 
	OpVector<HistoryAutocompleteModelItem> m_more_items;
	OpVector<SearchSuggestionAutocompleteItem> m_search_suggestions_items;

	OpVector<HistoryAutocompleteModelItem> m_search_engine_items; // items provided by "search_engine" module
	OpVector<GenericTreeModelItem> m_items_to_delete;

	SimpleAutocompleteItem* m_show_all_item;

	//--------------------------------------------------
	// Tabs :
	//--------------------------------------------------

	// Cursor the will indicate the current tab :
	OpTabCursor* m_tab_cursor;

	// The content of the url field for each window :
	TabTextContent<OpAddressDropDown> m_text_content;

	//--------------------------------------------------
	// Setting :
	//--------------------------------------------------

	BOOL m_open_page_in_tab;

	// This is used as a real address bar, not in F2 dialog etc. Url from address bar always open in current tab
	// no matter "reuse current tab" is checked or not
	BOOL m_in_addressbar;
	
	//--------------------------------------------------
	// Internal Setting :
	//--------------------------------------------------

	// performance timers used to reduce the number of updates for the progress bar
	OpDelayedTrigger		m_delayed_trigger;
	BOOL					m_delayed_progress_visibility;
	BOOL					m_delayed_layout_in_progress;

	
	AddressbarOverlayController* m_addressbar_overlay_ctl;
	FavoritesOverlayController*  m_favorites_overlay_ctl;

	bool					m_show_search_suggestions;
	bool					m_show_speed_dial_pages;
	BOOL					m_process_search_results; ///< Whether incoming search results should be processed

	int						m_lock_current_text;
	bool					m_is_quick_completed; ///< The current text has been quick completed (inline in the textfield)
	OpString				m_text_before_quick_completion;
	BOOL					m_tab_mode;
	BOOL					m_search_suggest_in_progress;	// Set with a search suggest has been started
	BOOL					m_shall_show_addr_bar_badge;	// Addr-bar badge is an icon (Foreground skin 'Window Document Icon') displayed within addr-bar.
	SearchSuggest*			m_search_suggest;				// pointer to the search suggest support class
	MessageHandler *		m_mh;
	int						m_query_error_counter;				// Number of failed JSON queries for autosuggest.
#if defined(_X11_SELECTION_POLICY_)
	INT32                   m_select_start;
	INT32                   m_select_end;
	INT32                   m_caret_offset;
#endif
	OpDelayedTrigger		m_highlight_delayed_trigger;
	OpString				m_url_proto_www_prefix; ///< Protcol, + www. part if exists, of URL. See DSK-301536.

	BOOL					m_use_display_url;
	GenericTreeModelItem*	m_item_to_select;
	OpString				m_actual_url;
	FavoritesCompositeListener* m_url_fav_composite_listener;
	bool					m_show_favorites_button;
	OpString				m_search_phrase;
	OpRegExp*				m_regex;
	SimpleAutocompleteItem* m_search_for_item;
	HistoryAutocompleteModelItem* m_selected_item; // DSK-358541 when search suggestions arrive late, keep item selected on dd recreation
	bool					m_ignore_edit_focus;
	OpTimer*				m_block_focus_timer;
	OpTimer*				m_notification_timer;
};

#endif // OP_ADDRESS_DROPDOWN_H
