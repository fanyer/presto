/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef HISTORY_AUTOCOMPLETE_MODEL_H
#define HISTORY_AUTOCOMPLETE_MODEL_H

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#include "adjunct/quick/models/HistoryModelItem.h"
#include "adjunct/quick/models/Bookmark.h"

class FavIconProvider;
class HistoryAutocompleteModelItem;
class OpTreeViewDropdown;
class OpAddressDropDown;
class AddressDropDownResizeButton;
class UniString;

/** @brief A tree model for the history autocomplete dropdown with 4 columns
*/
class HistoryAutocompleteModel : public TreeModel<HistoryAutocompleteModelItem>
{
public:
	// From TreeModel
	virtual INT32 GetColumnCount() { return 2; }
	virtual OP_STATUS GetColumnData(ColumnData* column_data) { return OpStatus::OK; }
};


/** @brief Item class for use with HistoryAutocompleteModel
  */
class HistoryAutocompleteModelItem : public TreeModelItem<HistoryAutocompleteModelItem, HistoryAutocompleteModel>
{
public:
	HistoryAutocompleteModelItem();

	enum SearchType
	{
		PAGE_CONTENT,
		HISTORY,
		BOOKMARK,
		OPERA_PAGE,
		UNKNOWN
	};

	/** @param associated_text Text that should be displayed along with the item
	  */
	virtual OP_STATUS SetAssociatedText(const OpStringC & associated_text) = 0;

	/** Get the address (URL) used to present to user, for speed dial and bookmarks
	  * this address can be different from what GetAddress returns
	  * @param address output URL
	  */
	virtual OP_STATUS GetDisplayAddress(OpString& address) const { return GetAddress(address); }

	/** Does the item has a different display url, when FALSE GetDisplayAddress returns
	  * the normal address
	  */
	virtual BOOL HasDisplayAddress() { return FALSE; }

	/** Get the address (URL) associated with this item
	  * @param address output URL
	  */
	virtual OP_STATUS GetAddress(OpString& address) const = 0;

	/** Get the title associated with this item
	  * @param address output title.
	  */
	virtual OP_STATUS GetTitle(OpString& title){ return OpStatus::ERR_NOT_SUPPORTED; } 

	/** Get the skin image associated with this item
	  * @return Skin key for an image associated with this item
	  */
	virtual const OpStringC8 GetItemImage() = 0;

	/** Set the bookmark associated with this item
	  * @param bookmark A bookmark that represents the same URL as this item
	  */
	virtual void SetBookmark(Bookmark* bookmark) = 0;

	/**
	 * Return TRUE if the item is a show all item.
	 */
	virtual BOOL IsShowAllItem() { return FALSE; }

	/**
	 * Return TRUE if the items address should be added as typed to the history even
	 * if the user choose it from the dropdown.
	 */
	virtual BOOL GetForceAddHistory() { return FALSE; }

	/**
	 * Return TRUE if this item should be possible to remove from within the addressfield dropdown.
	 */
	virtual BOOL IsUserDeletable() { return FALSE; }

	/**
	 * Return TRUE if this item is associated with a item in directHistory, so we know how to delete it.
	 */
	virtual BOOL IsInDirectHistory() { return FALSE; }

	/**
	 * Return TRUE if this item is associated with an item in DesktopHistoryModel.
	 */
	virtual BOOL IsInDesktopHistoryModel() = 0;

	// From TreeModelItem
	virtual OpTypedObject::Type GetType() { return OpTypedObject::UNKNOWN_TYPE; }
	virtual INT32 GetID() { return 0; }

	/* Sets the width of address bar's badge (protocol button). */
	void SetBadgeWidth(int width) { m_badge_width = width; }

	/** 
	 * "Smart" shortening for other than domain part of URL address.
	 *
	 * @param text			input/output value, which will be shortcutted
	 * @param preserve		part of string, when if matched, should not be shortucted
	 * @param max_length	max length of resulting string
	 */
	static		OP_STATUS ShortenUrl(OpString& text, const OpStringC& preserve, int limit);

	/** 
	 * Update item rank value accodring to it's matching with text
	 *
	 * @param text	user input in addressbar
	 */
	virtual		OP_STATUS UpdateRank(const OpStringC& text) = 0;

	/** 
	 * Set fixed item rank value
	 */
	void		SetRank(double rank) { m_rank = rank; }
	
	double		GetRank() const { return m_rank; }

	/**
	 *	Items compare function for QSort
	 */
	int static	Cmp(const HistoryAutocompleteModelItem **a,const HistoryAutocompleteModelItem** b) { return (*b)->GetRank() - (*a)->GetRank() > 0 ? 1 : -1; }
	
	void		SetSearchType(SearchType search_type) { m_search_type = search_type; }
	SearchType	GetSearchType() { return m_search_type; }
	
	// Text which should be highlighted
	OP_STATUS SetHighlightText(const OpStringC& text) { return m_highlight_text.Set(text); }

protected:

	double CalculateRank(const OpStringC& search_pattern, const OpStringC& text, const char* separator, double factor) const;
	double CalculateRankPart(const OpStringC& search_pattern, const OpStringC& text, const char* separator, double factor) const;
	static uni_char* ShortenUrlInternal(const OpStringC& input, const OpStringC& preserve, int max_length, int input_length);
	static OP_STATUS ShortenUrlQuery(UniString& uni_out, const OpStringC& preserve, int limit);

	/**
	 * Join two parts of an autocomplete item, taking UI direction into
	 * account.
	 *
	 * FIXME: Remove when DSK-359759 is fixed properly.
	 *
	 * @param primary the "primary" part, e.g., URL
	 * @param secondary the "secondary" part, e.g., page title
	 * @param result receives the result of the joining
	 */
	static OP_STATUS JoinParts(OpStringC primary, OpStringC secondary, OpString& result);

	OpString	m_highlight_text;

	/** 
	 * Items in the address dropdown might be sorted by rank (relevance) value.
	 */
	double		m_rank;
	
	int			m_badge_width;
	bool		m_use_favicons;
	SearchType	m_search_type;
};


/** @brief History item that refers to a HistoryModelPage for data
  */
class PageBasedAutocompleteItem : 
	public HistoryAutocompleteModelItem,
	public BookmarkListener
{
public:

	/** @param page Non-null pointer to page this item should refer to
	  * NB: pointer should stay alive as long as this item
	  */
	PageBasedAutocompleteItem(HistoryModelPage* page);

	~PageBasedAutocompleteItem();

	/** Set to TRUE if the address should not be unescaped before painting. */
	void SetNoUnescaping(BOOL val) { m_packed.no_unescaping = val; }

	time_t GetAccessed() const { return m_page->GetAccessed(); }
	bool IsBookmark() { return m_page->IsInBookmarks(); }

	// From HistoryAutocompleteModelItem
	virtual OP_STATUS SetAssociatedText(const OpStringC & associated_text) { return m_associated_text.Set(associated_text); }
	virtual OP_STATUS GetAddress(OpString& address) const;
	virtual BOOL HasDisplayAddress();
	virtual OP_STATUS GetDisplayAddress(OpString& address) const;
	virtual OP_STATUS GetTitle(OpString& title);
	virtual const OpStringC8 GetItemImage();
	virtual void SetBookmark(Bookmark* bookmark);
	virtual BOOL IsUserDeletable();
	virtual BOOL IsInDirectHistory() { return TRUE; }
	virtual BOOL IsInDesktopHistoryModel() { return TRUE; }

	// BookmarkListener
	virtual void OnBookmarkDestroyed() {m_bookmark = NULL; }

	// From TreeModelItem
	virtual OP_STATUS GetItemData(OpTreeModelItem::ItemData* item_data);

	virtual OpTypedObject::Type	GetType() { return OpTypedObject::WIDGET_TYPE_PAGE_BASED_AUTOCOMPLETE_ITEM; }

	static void SetFontWidth(unsigned int px) {m_font_width = px;}

	OP_STATUS UpdateRank(const OpStringC& match_text);

private:
	OP_STATUS GetStrippedAddress(OpString & address);
	void SetColumnsWidths(OpTreeModelItem::ItemData* item_data);
	OP_STATUS GetFlags(OpTreeModelItem::ItemData* item_data);
	OP_STATUS GetContents(OpTreeModelItem::ItemData* item_data);
	OP_STATUS GetIcon(OpTreeModelItem::ItemData* item_data);
	OP_STATUS GetTextContent(OpTreeModelItem::ItemData* item_data);

	HistoryModelPage*	m_page;
	Bookmark*			m_bookmark;
	OpString			m_associated_text;

	static unsigned int	m_font_width;
	union
	{
		struct
		{
			unsigned char no_unescaping:1;
		} m_packed;
		unsigned char m_packed_init;
	};
};

/** @brief History item that contains its own data, similar to SimpleTreeModelItem
  */
class SimpleAutocompleteItem : public HistoryAutocompleteModelItem
{
public:
	/** @param tree_model Model that this item will be in
	  */
	SimpleAutocompleteItem(OpTreeModel* tree_model);

	void SetShowAll(BOOL show_all) { m_packed.is_show_all = show_all; }
	void SetUserDeletable(BOOL deletable) { m_packed.is_user_deletable = deletable; }
	void SetForceAddHistory(BOOL force) { m_packed.force_add_history = force; }
	void SetInDirectHistory(BOOL in_direct_history) { m_packed.is_in_direct_history = in_direct_history; }
	void SetIsSearchWith() { m_packed.is_search_with = true; }
	void SetTips(BOOL is_tips) { m_packed.is_tips = is_tips; }
	void SetIsLocalFile() { m_packed.is_local_file = true; }
	void SetIsTypedHistory() { m_packed.is_typed_history = true; }

	/** 
	@param address URL to associate with this item
	*/
	OP_STATUS SetAddress(const OpStringC& address) { return m_address.Set(address); }

	// SimpleTreeModelItem forwarders
	OP_STATUS SetItemData(INT32 column, const uni_char* string, const char* image = NULL, INT32 sort_order = 0) { return m_simpleitem.SetItemData(column, string, image, sort_order); }
	void      SetItemColor(INT32 column, unsigned color) { m_simpleitem.SetItemColor(column, color); }
	OP_STATUS SetFavIconKey(const OpStringC & favicon_key) { return m_simpleitem.SetFavIconKey(favicon_key); }
	OP_STATUS SetItemImage(const char * image) { return m_simpleitem.SetItemImage(0, image); }

	// From HistoryAutocompleteModelItem
	virtual OP_STATUS GetAddress(OpString& address) const { return address.Set(m_address); }
	virtual OP_STATUS SetAssociatedText(const OpStringC & associated_text) { return m_simpleitem.SetAssociatedText(associated_text); }
	virtual const OpStringC8 GetItemImage() { return m_simpleitem.GetItemImage(0); }
	virtual void SetBookmark(Bookmark* bookmark) {}
	virtual BOOL IsShowAllItem() { return m_packed.is_show_all; }
	virtual BOOL GetForceAddHistory() { return m_packed.force_add_history; }
	virtual BOOL IsUserDeletable() { return m_packed.is_user_deletable; }
	virtual BOOL IsInDirectHistory() { return m_packed.is_in_direct_history; }
	virtual BOOL IsInDesktopHistoryModel() { return !IsInDirectHistory(); }

	// From TreeModelItem
	virtual OP_STATUS GetItemData(OpTreeModelItem::ItemData* item_data);
	virtual void OnRemoved() { m_simpleitem.OnRemoved(); }
	virtual BOOL IsSearchWith() { return m_packed.is_search_with; }
	virtual	OP_STATUS UpdateRank(const OpStringC& text) { m_rank = 0; return OpStatus::OK; }

private:
	SimpleTreeModelItem m_simpleitem;
	OpString			m_address;
	union
	{
		struct
		{
			unsigned char is_show_all:1;
			unsigned char is_user_deletable:1;
			unsigned char force_add_history:1;
			unsigned char is_in_direct_history:1;
			unsigned char is_search_with:1;
			unsigned char is_tips:1;
			unsigned char is_local_file:1;
			unsigned char is_typed_history:1;
		} m_packed;
		unsigned char m_packed_init;
	};
};

/** @brief Custom item used for the search suggestions
*/
class SearchSuggestionAutocompleteItem : public SimpleAutocompleteItem
{
public:
	SearchSuggestionAutocompleteItem(OpTreeModel* tree_model) : SimpleAutocompleteItem(tree_model), m_show_search_engine(false) {}

	OP_STATUS SetSearchSuggestion(const uni_char* text) 
	{
		return m_search_suggestion.Set(text); 
	}
	const uni_char* GetSearchSuggestion() const { return m_search_suggestion.CStr(); }

	OP_STATUS SetSearchProvider(const uni_char* search_id) 
	{ 
		return m_search_id.Set(search_id);
	}
	const uni_char* GetSearchProvider() const { return m_search_id.CStr(); }

	virtual OP_STATUS GetItemData(OpTreeModelItem::ItemData *item_data);

	virtual OpTypedObject::Type	GetType() { return OpTypedObject::WIDGET_TYPE_SEARCH_SUGGESTION_ITEM; }

	virtual OP_STATUS GetAddress(OpString& address) const;

	virtual OP_STATUS GetDisplayAddress(OpString& address) const;

	void SetShowSearchEngine() { m_show_search_engine = true; }

	virtual BOOL IsInDesktopHistoryModel() { return !IsInDirectHistory(); }

private:
	OP_STATUS GetColumnData(OpTreeModelItem::ItemData *item_data);

	OpString	m_search_suggestion;
	OpString	m_search_id;
	bool		m_show_search_engine;
};

#endif // HISTORY_AUTOCOMPLETE_MODEL_H
