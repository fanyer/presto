/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#ifndef OP_SEARCH_DROPDOWN_H
#define OP_SEARCH_DROPDOWN_H

#include "modules/widgets/OpDropDown.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/search/search_net.h"
#endif // DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick_toolkit/widgets/OpTreeViewDropdown.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/desktop_util/search/search_suggest.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpButton.h"

class SearchDropDownModelItem;
class SearchDropDownModel;
class OpLabel;

class OpSearchDropDownListener
{
public:
	virtual ~OpSearchDropDownListener() {}
	virtual void OnSearchChanged( const OpStringC& guid ) = 0;
	virtual void OnSearchDone() = 0;
};

/** @brief Item class for use with SearchDropDownModel
 */
class SearchDropDownModelItem : public TreeModelItem<SearchDropDownModelItem, SearchDropDownModel>
{
public:
	SearchDropDownModelItem() : 
	m_search(NULL), 
	m_separator(FALSE), 
	m_active_search(FALSE), 
	m_suggestion(FALSE), 
	m_suggestion_label(FALSE), 
	m_search_history_header(FALSE)
	{
		
	}
	
	// From TreeModelItem
	virtual OpTypedObject::Type	GetType() { return OpTypedObject::UNKNOWN_TYPE; }
	virtual INT32				GetID() { return 0; }
	
	virtual OP_STATUS			GetItemData(OpTreeModelItem::ItemData *item_data);
	
	void						SetSearchItem(SearchTemplate* search) { m_search = search; }
	SearchTemplate*				GetSearchItem() { return m_search; }
	void						SetSeparator(BOOL separator) { m_separator = separator; }
	void						SetIsSuggestion(BOOL suggestion) { m_suggestion = suggestion; }
	void						SetIsSuggestionLabel(BOOL suggestion) { m_suggestion_label = suggestion; }
	BOOL						IsSeparator() { return m_separator; }
	BOOL						IsSuggestion() { return m_suggestion; }
	BOOL						IsSuggestionLabel() { return m_suggestion_label; }
	void						SetActiveSearch(BOOL active, const uni_char* text);
	BOOL						IsActiveSearch() { return m_active_search; }
	OP_STATUS					GetActiveSearch(OpString& search) { return search.Set(m_active_search_text.CStr()); }
	void						SetIsSearchHistoryHeader(BOOL history_header) { m_search_history_header = history_header; }
	BOOL						IsSearchHistoryHeader() const { return m_search_history_header; }
	virtual BOOL				IsSearchHistoryItem() const { return FALSE; }
	
	// is this item part of the possible selectable items when using keyboard control?
	virtual BOOL				IsSelectable() const { return TRUE; }
	virtual BOOL				IsUserDeletable() const { return FALSE; }
	
protected:
	OpString			m_active_search_text;
	
private:
	SearchTemplate*		m_search;			// reference to the search
	BOOL				m_separator;
	BOOL				m_active_search;	// Is this item the active the user is about to use?
	BOOL				m_suggestion;		// Is this item a suggestion result entry
	BOOL				m_suggestion_label;	// Is this item a suggestion result label entry
	BOOL				m_search_history_header;	// Is this item a search history label entry
};

/** @brief A tree model for the search dropdown with 2 columns
 */
class SearchDropDownModel : public TreeModel<SearchDropDownModelItem>
{
public:
	SearchDropDownModel() {}
	
	// From TreeModel
	virtual INT32 GetColumnCount() { return 3; }
	virtual OP_STATUS GetColumnData(ColumnData* column_data) {	return OpStatus::OK; }
	
private:
};

/** @brief Custom item used for the parent level labels
 */
class SearchParentLabelModelItem : public SearchDropDownModelItem
{
public:
	virtual OP_STATUS			GetItemData(OpTreeModelItem::ItemData *item_data);
	
	virtual OpTypedObject::Type	GetType() { return OpTypedObject::WIDGET_TYPE_SEARCH_PARENT_LABEL_ITEM; }
	
	OP_STATUS					SetText(const uni_char* text) { return m_text.Set(text); }
	const uni_char*				GetText() { return m_text.CStr(); }
	virtual BOOL				IsSelectable() const { return FALSE; }
	OP_STATUS					SetSkinImage(const char* text) { return m_skin_image.Set(text); }
	
private:
	OpString		m_text;
	OpString8		m_skin_image;
};

/** @brief Custom item used for the search field history
 */
class SearchHistoryModelItem : public SearchDropDownModelItem
{
public:
	OP_STATUS SetSearchHistory(const uni_char* search_id, const uni_char* search_term) 
	{ 
		RETURN_IF_ERROR(m_search_id.Set(search_id));
		return m_search_term.Set(search_term);
	}
	const uni_char* GetSearchID() { return m_search_id.CStr(); }
	const uni_char* GetSearchTerm() { return m_search_term.CStr(); }
	
	virtual OP_STATUS			GetItemData(OpTreeModelItem::ItemData *item_data);
	
	virtual OpTypedObject::Type	GetType() { return OpTypedObject::WIDGET_TYPE_SEARCH_HISTORY_ITEM; }
	virtual BOOL				IsUserDeletable() const { return TRUE; }
	virtual BOOL				IsSearchHistoryItem() const { return TRUE; }
	
private:
	OpString	m_search_id;
	OpString	m_search_term;
};

/** @brief Custom item used for the search suggestions
 */
class SearchSuggestionModelItem : public SearchDropDownModelItem
{
public:
	
	OP_STATUS SetSearchSuggestion(const uni_char* text) 
	{
		return m_search_suggestion.Set(text); 
	}
	const uni_char* GetSearchSuggestion() { return m_search_suggestion.CStr(); }
	
	OP_STATUS SetSearchProvider(const uni_char* search_id) 
	{ 
		return m_search_id.Set(search_id);
	}
	const uni_char* GetSearchProvider() { return m_search_id.CStr(); }
	
	virtual OP_STATUS			GetItemData(OpTreeModelItem::ItemData *item_data);
	
	virtual OpTypedObject::Type	GetType() { return OpTypedObject::WIDGET_TYPE_SEARCH_SUGGESTION_ITEM; }
	
private:
	OpString	m_search_suggestion;
	OpString	m_search_id;
};

/** @brief Class to execute an action when selected
 */
class SearchExecuteActionItem : public SearchDropDownModelItem
{
public:
	SearchExecuteActionItem(const uni_char *title, OpInputAction::Action action) : 
	m_action(action)
	{
		m_title.Set(title);
	};
	
	virtual OpTypedObject::Type	GetType() { return OpTypedObject::WIDGET_TYPE_SEARCH_ACTION_ITEM; }
	
	virtual OP_STATUS			GetItemData(OpTreeModelItem::ItemData *item_data);
	
	// is this item part of the possible selectable items when using keyboard control?
	virtual BOOL				IsSelectable() const { return TRUE; }
	
	OpInputAction::Action		GetAction() { return m_action; }
	
protected:
	OpString	m_title;
	OpString8	m_name;
	
private:
	OpInputAction::Action		m_action;			// action to do on click
};

/***********************************************************************************
 **
 **	OpSearchDropDown
 **
 **
 **
 ** Note: OpTypedObject m_type of OpSearchDropDown is the type of the Search chosen 
 **       (that is, one of the types in search_types.h)
 **
 ***********************************************************************************/

class OpSearchDropDown : 
public OpTreeViewDropdown,
public DocumentDesktopWindowSpy, 
public SearchSuggest::Listener
{
protected:
	OpSearchDropDown();
	
public:
	
	static OP_STATUS Construct(OpSearchDropDown** obj);
	
	virtual ~OpSearchDropDown();
	
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
	void					SetSearchFromType(SearchType type);
#endif // DESKTOP_UTIL_SEARCH_ENGINES
	
	const OpStringC&        GetSearchGUID() const { return m_search_guid; }
	
	void 					ValidateSearchType();
	void   	   				SetSearchDropDownListener(OpSearchDropDownListener* listener) {m_search_dropdown_listener = listener;}
	
	virtual void			OnDeleted();
	
	void					GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);
	
	// OpWidgetListener
	
	virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	virtual void			OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
	virtual void			OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y) { static_cast<OpWidgetListener *>(GetParent())->OnDragMove(this, drag_object, pos, x, y);}
	virtual void			OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y) { static_cast<OpWidgetListener *>(GetParent())->OnDragDrop(this, drag_object, pos, x, y);}
	virtual void			OnDragLeave(OpWidget* widget, OpDragObject* drag_object) { static_cast<OpWidgetListener *>(GetParent())->OnDragLeave(this, drag_object);}
	virtual void			OnDropdownMenu(OpWidget *widget, BOOL show);
	virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);
	virtual BOOL            OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
	{
		OpWidgetListener* listener = GetParent() ? GetParent() : this;
		return listener->OnContextMenu(this, child_index, menu_point, avoid_rect, keyboard_invoked);
	}
	
	virtual Type			GetType() {return WIDGET_TYPE_SEARCH_DROPDOWN;}
	
	// OpInputStateListener
	
	virtual void			OnUpdateActionState() {UpdateSearchIcon(); OpDropDown::OnUpdateActionState();}
	
	// == OpInputContext ======================
	
	virtual BOOL			OnInputAction(OpInputAction* action);
	virtual const char*		GetInputContextName() {return "Search Dropdown Widget";}
	
	// DocumentDesktopWindowSpy hooks
	
	virtual void			OnTargetDocumentDesktopWindowChanged(DocumentDesktopWindow* target_window);
	
	void					Search(SearchEngineManager::SearchSetting settings/*const uni_char* override_search_text, SearchTemplate* override_search*/, BOOL is_suggestion);
	
	// == OpTreeViewDropdown ======================
	
	virtual void			OnClear();
	virtual void			OnInvokeAction(BOOL invoked_on_user_typed_string);
	virtual void			ShowDropdown(BOOL show);
	virtual void			OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);

	void SetModel(SearchDropDownModel* tree_model, BOOL items_deletable)
	{
		OpTreeViewDropdown::SetModel(tree_model, items_deletable);
	}
	SearchDropDownModel * GetModel()
	{
		return static_cast<SearchDropDownModel*>(OpTreeViewDropdown::GetModel());
	}
	virtual UINT32 GetColumnWeight(UINT32 column);
	
	virtual void AdjustSize(OpRect& rect);
	
	// MessageObject
	void					HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	
	// SearchSuggest::Listener
	virtual void OnQueryStarted() { m_search_suggest_in_progress = TRUE; };
	virtual void OnQueryComplete(const OpStringC& search_id, OpVector<SearchSuggestEntry>& entries);
	virtual void OnQueryError(const OpStringC& search_id, OP_STATUS error) { ++m_query_error_counter; m_search_suggest_in_progress = FALSE; }

	void ResetQueryErrorCounter() { m_query_error_counter = 0; }

	// really private, but used in selftest. DO NOT USE OUTSIDE SELFTESTS OR THIS CLASS.
	SearchDropDownModelItem *GetActiveSearchEngineItem();
	OP_STATUS				UpdateActiveSearchLine(SearchTemplate* new_search, const OpString& search_text);
	// END OF REALLY PRIVATE
	
	static OP_STATUS		CreateFormattingString(Str::LocaleString message_id, OpString& content_string, OpString& content_string_2, OpString& final_string);
	
	void					SetSearchInSpeedDial(BOOL is_inside_speeddial);
	
	// OpToolTipListener
	virtual BOOL HasToolTipText(OpToolTip* tooltip);
	virtual void GetToolTipText(OpToolTip* tooltip, OpInfoText& text);

	// QuickOpWidgetBase
	virtual BOOL ScrollOnFocus() { return TRUE; }
	virtual void OnSettingsChanged(DesktopSettings* settings);
	
private:
	void					StartSearchSuggest();
	OP_STATUS				ExecuteSearchForItem(SearchDropDownModelItem *item);
	OP_STATUS				AddSearchHistoryItems(OpString& filter);
	OP_STATUS				AddSearchEngines(const OpString& search_text);
	OP_STATUS				AddManageSearchEngines();

	// Adds separator to drop down only, when there is more than one element, except separator
	void					EnsureSeparator();
	virtual void			ResizeDropdown();
	BOOL					HasSearchHistoryItems();
	void					RemoveSearchHistoryHeader();
	void					UpdateSearchField(SearchTemplate *search = NULL);
	void					UpdateSearchIcon();
	void					UpdateSelectedItem();
	
	// used to filter out engines not to be shown in the list
	BOOL					IsValidSearchForList(SearchTemplate* search);

	// updates m_follow_default_search flag
	// should be called after UpdateSearchField when target window changes or
	// drop down is placed in/outside speed dial
	void					UpdateFollowDefaultSearch();

private:
	OpString                m_search_guid;					// GUID if the currently active search engine (in the search field)
	OpString                m_search_text;					// text in the edit box.
	INT32					m_type;							// SearchType of the currently selected search engine
	BOOL                    m_search_changed;
	OpSearchDropDownListener* m_search_dropdown_listener;
	BOOL					m_search_suggest_in_progress;	// Set with a search suggest has been started
	SearchSuggest*			m_search_suggest;				// pointer to the search suggest support class
	BOOL					m_is_inside_speeddial;			// TRUE if inside speed dial. Default is FALSE.
	BOOL					m_follow_default_search;		// whether m_search_guid should be updated when user changes default search engine
	MessageHandler *		m_mh;
	int						m_query_error_counter;				// Number of failed JSON queries for autosuggest.
	SearchDropDownModelItem* m_active_search_item;
	SearchDropDownModelItem* m_web_search_item;				// pointer to search engine item, which will be updated according to editfield content
	OpString				m_ghost_string;					// ghost and tooltip text.
};

/** @brief Subclassed (clear) button for Mac.
 */

// BEGIN FIX FOR DSK-311687
// ismailp - because OpButton grabs focus to itself in its OnMouseDown event,
// we can't set focus to search edit box. We basically let OpButton to do
// whatever it was supposed to do first, and then we set focus to OpEdit of
// the search input.
#ifdef QUICK_SEARCH_DROPDOWN_MAC_STYLE
class MacOpClearButton : public OpButton {
public:
	MacOpClearButton(ButtonType button_type = TYPE_PUSH, ButtonStyle button_style = STYLE_IMAGE_AND_TEXT_ON_RIGHT)
	: OpButton(button_type, button_style) { }
	static OP_STATUS Construct(MacOpClearButton** obj, ButtonType button_type = TYPE_PUSH, ButtonStyle button_style = STYLE_IMAGE_AND_TEXT_ON_RIGHT);
	virtual void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
};
#endif // QUICK_SEARCH_DROPDOWN_MAC_STYLE

/** @brief Special wrapper class that wraps the OpSearchDropDown with a new fancy drop down push button on the left
 */
class OpSearchDropDownSpecial : 
public OpWidget
, public FavIconManager::FavIconListener
{
public:
	static OP_STATUS Construct(OpSearchDropDownSpecial** obj);
	
	OpSearchDropDownSpecial();
	
    virtual void	OnContentSizeChanged();
	
	// OpWidget
	virtual void			OnLayout();
	virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks) { if(GetParent()) ((OpWidgetListener*)GetParent())->OnMouseEvent(widget, pos, x, y, button, down, nclicks); }
	virtual void			OnDeleted();
	void					GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);
	
	// propagate the menu to the parent
	virtual BOOL            OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
	{
		return GetParent() ? ((OpWidgetListener*)GetParent())->OnContextMenu(this, child_index, menu_point, avoid_rect, keyboard_invoked) : FALSE;
	}
	
	virtual OP_STATUS		GetText(OpString& text) { return m_search_drop_down->GetText(text); }
	OpRect					GetTextRect() { return m_search_drop_down->edit->GetTextRect(); }
	INT32					GetStringWidth() { return m_search_drop_down->edit->string.GetWidth(); }
	OpSearchDropDown*		GetSearchDropDown() { return m_search_drop_down; }
	OpEdit*					GetSearchInput() { return GetSearchDropDown()->edit; }
	
	
	BOOL					OnInputAction(OpInputAction* action);
	virtual const char*		GetInputContextName() {return "Search Dropdown Special Widget";}
	
	virtual Type			GetType() {return WIDGET_TYPE_SEARCH_SPECIAL_DROPDOWN;}
	
	// FavIconManager::FavIconListener
	virtual void 			OnFavIconAdded(const uni_char* document_url, const uni_char* image_path);
	virtual void 			OnFavIconsRemoved();	
	
	void					UpdateSearchIcon();
#ifdef QUICK_SEARCH_DROPDOWN_MAC_STYLE
	void					UpdateCloseIcon();
#endif // QUICK_SEARCH_DROPDOWN_MAC_STYLE
	
	OpButton*				GetDropdownButton() { return m_dropdown_button; }
	
	void					SetSearchInSpeedDial(BOOL is_inside_speeddial) 
	{ 
		if(m_search_drop_down) 
			m_search_drop_down->SetSearchInSpeedDial(is_inside_speeddial); 
	}
	
private:
	OpSearchDropDown*				m_search_drop_down;
	OpButton*						m_dropdown_button;
#ifdef QUICK_SEARCH_DROPDOWN_MAC_STYLE
	MacOpClearButton*				m_clear_button;		// button to show a close button to clear the contents, or just a search icon if the field is empty
#else
	OpButton*						m_go_button;		// button for executing the search
#endif // QUICK_SEARCH_DROPDOWN_MAC_STYLE
};

#endif // OP_SEARCH_DROPDOWN_H
