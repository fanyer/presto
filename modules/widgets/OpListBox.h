/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_LIST_BOX_H
#define OP_LIST_BOX_H

#include "modules/widgets/OpWidget.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/adt/OpFilteredVector.h"
#include "modules/widgets/OpScrollbar.h"

#include "modules/prefs/prefsmanager/collections/pc_display.h"

/** The number of pixels to indent items that are under a group item. */
#define OPTGROUP_EXTRA_MARGIN 10

/** Little helper for the inline search of items */

class OpItemSearch
{
public:
	OpItemSearch() : m_last_keypress_time(0) {}
	~OpItemSearch() {}

	OP_STATUS PressKey(const uni_char *key_value);
	const uni_char* GetSearchString();
	BOOL IsSearching();
#ifdef WIDGETS_IME_ITEM_SEARCH
	void Clear();
#endif // WIDGETS_IME_ITEM_SEARCH
private:
	double m_last_keypress_time;
	OpString string;
};

/** OpStringItem is the item used in ItemHandler for listboxes and dropdowns.
	It basically stores a string, and has some properties like f.ex color and enabled, selected status.
	It can also be a group item or separator item which are handled differently by the ItemHandler. */

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

class AccessibleListListener
{
public:
	virtual BOOL IsListFocused() const = 0;
	virtual BOOL IsListVisible() const = 0;
	virtual OP_STATUS GetAbsolutePositionOfList(OpRect &rect) = 0;
	virtual OpAccessibleItem* GetAccessibleParentOfList() = 0;
	virtual OP_STATUS ScrollItemTo(INT32 item, Accessibility::ScrollTo scroll_to) = 0;
};

class AccessibleListItemListener
{
public:
	virtual OpAccessibleItem* GetAccessibleParentOfItem() = 0;
	virtual AccessibleListListener* GetAccessibleListListener() = 0;
	virtual OP_STATUS GetAbsolutePositionOfItem(OpStringItem* string_item, OpRect &rect) = 0;
};

#endif //ACCESSIBILITY_EXTENSION_SUPPORT

class ItemHandler;

class OpStringItem : public OpFilteredVectorItem<OpStringItem>
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	, public OpAccessibleItem
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
{
	 //Needs access to SetGroupStart and SetGroupStop - see their comment below
	friend class ItemHandler;

public:
	OpStringItem(BOOL filtered = FALSE);
	virtual ~OpStringItem();

	/** Set the text on the item. */
	OP_STATUS SetString(const uni_char* string, OpWidget* widget);

	virtual void SetEnabled(BOOL status);
	virtual void SetSelected(BOOL status);

	/** Set if this item is a separator. A separator will be drawn as a line, so the text doesn't matter. */
	void SetSeperator(BOOL status) { packed.is_seperator = status; }

	/**
	 * Set one or more text decoration properties on the string. Use 0
	 * to clear the property.
	 *
	 * @param text_decoration One or more of WIDGET_TEXT_DECORATION types
	 * @param replace If TRUE, the current value is fully replaced, if FALSE
	 *        the new value is added in addition to the current value
	 */
	void SetTextDecoration(INT32 text_decoration, BOOL replace) { string.SetTextDecoration(text_decoration, replace); }

	void SetFgColor(UINT32 col) { fg_col = col; }
	void SetBgColor(UINT32 col) { bg_col = col; }

	/** Set any kind of data that can be got with GetUserData() */
	void SetUserData(INTPTR data) { user_data = data; }
	INTPTR GetUserData() const { return user_data; }

	BOOL IsEnabled()    { return packed.is_enabled && !packed.is_seperator; }
	BOOL IsSelected()   { return packed.is_selected; }
	BOOL IsGroupStart() { return packed.is_group_start; }
	BOOL IsGroupStop()  { return packed.is_group_stop; }
	BOOL IsSeperator()  { return packed.is_seperator; }
	INT32 GetTextDecoration() { return string.GetTextDecoration(); }

	/** Add item margins to rect */
	static void AddItemMargin(OpWidgetInfo* info, OpWidget *widget, OpRect &rect);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	void SetAccessibleListItemListener(AccessibleListItemListener* list_item_listener) { m_list_item_listener = list_item_listener; }

	virtual BOOL AccessibilityIsReady() const {return m_list_item_listener != NULL;}
	virtual int GetAccessibleChildrenCount() {return 0;}
	virtual OpAccessibleItem* GetAccessibleParent();
	virtual OpAccessibleItem* GetAccessibleChild(int) {return NULL;}
	virtual int GetAccessibleChildIndex(OpAccessibleItem* child) {return Accessibility::NoSuchChild; }
	virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
	virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();
	virtual Accessibility::ElementKind AccessibilityGetRole() const { return Accessibility::kElementKindListRow; }
	virtual Accessibility::State AccessibilityGetState();
	virtual OP_STATUS AccessibilityGetAbsolutePosition(OpRect &rect);
	virtual OP_STATUS AccessibilityGetText(OpString& str);
	virtual OP_STATUS AccessibilityScrollTo(Accessibility::ScrollTo scroll_to);

	virtual OpAccessibleItem* GetNextAccessibleSibling();
	virtual OpAccessibleItem* GetPreviousAccessibleSibling();
	virtual OpAccessibleItem* GetLeftAccessibleObject() { return NULL; }
	virtual OpAccessibleItem* GetRightAccessibleObject() { return NULL; }
	virtual OpAccessibleItem* GetDownAccessibleObject() { return NULL; }
	virtual OpAccessibleItem* GetUpAccessibleObject() { return NULL; }

#endif //ACCESSIBILITY_EXTENSION_SUPPORT

private:
	/* These methods should only be called combined with the SetVisibility  */
	/* method in OpFilteredVector. If called on items in a vector bypassing */
	/* the OpFilteredVector the vector will become inconsistant and bugs    */
	/* _will_ occur.                                                        */
	void SetGroupStart(BOOL status) { packed.is_group_start = status; string.SetForceBold(status); string.SetForceItalic(status); }
	void SetGroupStop(BOOL status)  { packed.is_group_stop = status;  }

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibleListItemListener* m_list_item_listener;
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

public:
	OpWidgetString string;
	OpWidgetString* string2; ///< Temp fix for 2 columns in autocompletionpopup until it uses TreeView
	INTPTR user_data;
	union {
		struct {
			unsigned char is_enabled:1;
			unsigned char is_selected:1;
			unsigned char is_group_start:1;
			unsigned char is_group_stop:1;
			unsigned char is_seperator:1;
		} packed;
		unsigned char packed_init;
	};
	UINT32 bg_col;
	UINT32 fg_col;
#ifdef SKIN_SUPPORT
	OpWidgetImage m_image;
	INT32 m_indent;
#endif
};

/** ItemHandler handles a list of OpStringItem. */

class ItemHandler
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	: public OpAccessibleItem, public AccessibleListItemListener
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
{
public:
	ItemHandler(BOOL multiselectable);
	~ItemHandler();

	void RemoveAll();

	/** Note: The items is not duplicated. The ownership remains the same if not own_items
			  is TRUE. Then they will be owned by this. */
	OP_STATUS DuplicateOf(ItemHandler& ih, BOOL own_items = FALSE, BOOL dup_focused_item = TRUE);

#ifdef WIDGETS_CLONE_SUPPORT
	/** Clone items will create new items with same properties but with widget as owner (that specifies font etc.) */
	OP_STATUS CloneOf(ItemHandler& ih, OpWidget* widget);
#endif

	/** Add item with 2 strings. */
	OP_STATUS AddItem(const uni_char* txt, const uni_char* txt2, OpWidget* widget);

	/** Add item.
	 * @param txt String for the item.
	 * @param nr nr the item should be inserted at (excluding groupitems) or -1 if the items should be added last.
	 * @param got_index Will be set to the index (including groupitems) where it was inserted.
	 * @param widget The widget that owns this ItemHandler.
	 * @param user_data (Optional) Any data that you want to get back from this item with GetUserData()
	 * @param txt2 (Optional) Second string for this item
	 */
	OP_STATUS AddItem(const uni_char* txt, INT32 nr, INT32 *got_index, OpWidget* widget, INTPTR user_data = 0, const uni_char* txt2 = NULL);

	OpStringItem* MakeItem(const uni_char* txt, INT32 nr, OpWidget* widget, INTPTR user_data = 0);

	/** Add item.
	 * @param item The item to add.
	 * @param widget The widget that owns this ItemHandler.
	 * @param got_index Will be set to the index (including groupitems) where it was inserted.
	 * @param nr nr the item should be inserted at (excluding groupitems) or -1 if the items should be added last.
	 */
	OP_STATUS     AddItem(OpStringItem * item, OpWidget* widget, INT32 *got_index = NULL, INT32 nr = -1);

	/** Remove item at nr (excluding groupitems) */
	void RemoveItem(INT32 nr);

	/** Change text in item at nr (excluding groupitems) */
	OP_STATUS ChangeItem(const uni_char* txt, INT32 nr, OpWidget* widget);

	/** Add a group item last in the list. Following items will be added under this group, until EndGroup is called. */
	OP_STATUS BeginGroup(const uni_char* txt, OpWidget* widget);
	/** Add a groupend item last in the list. */
	OP_STATUS EndGroup(OpWidget* widget);

	/** Removes the start and stop item for the given group. */
	void RemoveGroup(int group_nr, OpWidget* widget);
	/** Removes all items! Not only groups. */
	void RemoveAllGroups();

	INTPTR GetItemUserData(INT32 nr);

	/** Select or unselect item at nr (excluding group items). If nr is -1, no item will be selected.
		If the listbox is not multiselectable, it will scroll to selected item. */
	void SelectItem(INT32 nr, BOOL selected);
	void EnableItem(INT32 nr, BOOL enabled);

	/** Set indentation in pixels of item at nr (excluding group items) */
	void SetIndent(INT32 nr, INT32 indent, OpWidget* widget);
	void SetImage(INT32 nr, const char* image, OpWidget* widget);
	void SetImage(INT32 nr, OpWidgetImage* widget_image, OpWidget* widget);

	void SetFgColor(INT32 nr, UINT32 col);
	void SetBgColor(INT32 nr, UINT32 col);
	void SetScript(INT32 nr, WritingSystem::Script script);

	INT32 CountItems() { return m_items.GetCount(VisibleVector); }
	INT32 CountItemsAndGroups() { return m_items.GetCount(CompleteVector); }
	/**
	 * Returns TRUE if the element at the index
	 * (counting all elements, including starts
	 * and stops) is a group stop.
	 */
	BOOL IsElementAtRealIndexGroupStop(UINT32 idx) { return m_items.Get(idx, CompleteVector)->IsGroupStop(); }
	/**
	 * Returns TRUE if the element at the index
	 * (counting all elements, including starts
	 * and stops) is a group start.
	 */
	BOOL IsElementAtRealIndexGroupStart(UINT32 idx) { return m_items.Get(idx, CompleteVector)->IsGroupStart(); }

	BOOL HasGroups() { return has_groups; }

	/** Total height of items and groups. */
	INT32 GetTotalHeight();

	/** Get Y position of item at nr (excluding groups) */
	INT32 GetItemYPos(INT32 nr);

	BOOL IsSelected(INT32 nr);
	BOOL IsEnabled(INT32 nr);

	/** Finds which index in the list item item_nr has. */
	INT32 FindItemIndex(INT32 item_nr);
	/** Get the itemnr of the item at position index. */
	INT32 FindItemNr(INT32 index);
	/** Returns the number of item at the y position.
		if always_get_option is true, will continue search for option if item at position is group start
	 */
	INT32 FindItemNrAtPosition(INT32 y, BOOL always_get_option = FALSE);

	OpStringItem* GetItemAtIndex(INT32 index);
	OpStringItem* GetItemAtNr(INT32 nr);

	/** Move the focus from from_nr to to_nr. If select is TRUE, enabled items will be selected between the items. */
	BOOL MoveFocus(INT32 from_nr, INT32 to_nr, BOOL select);

	/** Select all enabled items, or unselect all items (including disabled). */
	void SetAll(BOOL selected);

	/**
	 * Sets the multiple flag saying if the widget can have multiple selected or only one. Setting it to single selection when there are multiple selected items will not change anything right now but that might change.
	 *
	 * @param[in] multiple TRUE if there should be possible to have multiple selected items.
	 */
	void SetMultiple(BOOL multiple);
	/** Find item by string or -1 if no matching item was found. */
	INT32 FindItem(const uni_char* string);
	/** Find item number, knowing the actual item **/
	INT32 FindItem(OpStringItem* item);
	/** Find enabled item forward or backward from the given number. If no item was found, from_br is returned. */
	INT32 FindEnabled(INT32 from_nr, BOOL forward);

	/** Go through all items, measure their width and height and update widest_item and highest_item. */
	void RecalculateWidestItem(OpWidget* widget);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

	virtual OpAccessibleItem* GetAccessibleParentOfItem() {return this;}
	void SetAccessibleListListener(AccessibleListListener* listener) {m_accessible_listener = listener;}
	virtual AccessibleListListener* GetAccessibleListListener() {return m_accessible_listener;}
	virtual OP_STATUS GetAbsolutePositionOfItem(OpStringItem* string_item, OpRect &rect);

	virtual int GetAccessibleChildrenCount();
	virtual BOOL AccessibilityIsReady() const {return m_accessible_listener != NULL;}
	virtual OpAccessibleItem* GetAccessibleParent();
	virtual OpAccessibleItem* GetAccessibleChild(int);
	virtual int GetAccessibleChildIndex(OpAccessibleItem* child);
	virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
	virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();
	virtual Accessibility::ElementKind AccessibilityGetRole() const {return Accessibility::kElementKindList;}
	virtual Accessibility::State AccessibilityGetState();
	virtual OP_STATUS AccessibilityGetAbsolutePosition(OpRect &rect);
	virtual OP_STATUS AccessibilityGetText(OpString& str) { str.Empty(); return OpStatus::OK; }
	virtual OpAccessibleItem* GetAccessibleLabelForControl();

	virtual OpAccessibleItem* GetNextAccessibleSibling() { return NULL; }
	virtual OpAccessibleItem* GetPreviousAccessibleSibling() { return NULL; }
	virtual OpAccessibleItem* GetLeftAccessibleObject() { return NULL; }
	virtual OpAccessibleItem* GetRightAccessibleObject() { return NULL; }
	virtual OpAccessibleItem* GetDownAccessibleObject() { return NULL; }
	virtual OpAccessibleItem* GetUpAccessibleObject() { return NULL; }

#endif //ACCESSIBILITY_EXTENSION_SUPPORT

public:
	BOOL is_multiselectable;
	INT32 focused_item; ///< Also used as the selected item in single selectable mode.
	INT32 widest_item;
	INT32 highest_item;
private:

	void UpdateMaxSizes(INT32 index, OpWidget* widget);
	INT16 in_group_nr;
	BOOL free_items; ///< false if no items should be freed. (As when dropdownlistbox barrows items from dropdown).
	BOOL has_groups;

	OpFilteredVector<OpStringItem> m_items;

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibleListListener* m_accessible_listener;
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
};

class OpListBox;
class VisualDevice;

/** ListboxListener is a listener internal to OpListBox to listen to the scrollbar. */

class ListboxListener : public OpWidgetListener
{
public:
	ListboxListener(OpListBox* widget)
		: listbox(widget)
	{
	}

	void OnScroll(OpWidget *widget, INT32 old_val, INT32 new_val, BOOL caused_by_input);
private:
	OpListBox* listbox;
};

/** OpListBox is a scrollable list of OpStringItem. It can be in single-select mode or multiselectable.
	If it is multiselectable, several items can be selected (and togglable), otherwise only one item can
	be selected by the user (can be selected by forcing selected status on items from the API though).
	In multiselectable mode, it can show checkboxes in front of each item if
	TWEAK_WIDGETS_CHECKBOX_MULTISELECT is enabled.
*/

class OpListBox : public OpWidget
	, public OpPrefsListener
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	, public AccessibleListListener
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
{
	friend class OpListAccessor;
public:
	// The BorderStyle of an OpListBox determines what its non-CSS-styled
	// border will look like. For popup menus and dropdowns, we usually want a
	// different style (e.g. thin/black) than for the listbox implementing a
	// multiselectable <select> control.
	enum BorderStyle
	{
		BORDER_STYLE_POPUP = 0,
		BORDER_STYLE_FORM
	} border_style;

protected:
	OpListBox(BOOL multiselectable = FALSE, BorderStyle border_style = BORDER_STYLE_FORM);

public:
	static OP_STATUS Construct(OpListBox** obj, BOOL multiselectable = FALSE, BorderStyle border_style = BORDER_STYLE_FORM);

	~OpListBox();

#ifdef WIDGETS_CLONE_SUPPORT
	virtual OP_STATUS CreateClone(OpWidget **obj, OpWidget *parent, INT32 font_size, BOOL expanded);
	virtual BOOL IsCloneable() { return TRUE; }
#endif

	/** Temp fix for 2 columns in autocompletionpopup until it uses TreeView */
	OP_STATUS AddItem(const uni_char* txt, const uni_char* txt2) { return ih.AddItem(txt, txt2, this); }

	OP_STATUS AddItem(const uni_char* txt, INT32 index = -1, INT32 *got_index = NULL, INTPTR user_data = 0, BOOL selected = FALSE, const char* image = NULL);
#ifdef WIDGET_ADD_MULTIPLE_ITEMS
	OP_STATUS AddItems(const OpVector<OpString> *txt, const OpVector<INT32> *index = NULL, const OpVector<void> *user_data = NULL, INT32 selected = -1, const OpVector<char> *image = NULL);
#endif
	/** Shortcut to ItemHandler::RemoveItem, and updates the scrollbar. */
	void RemoveItem(INT32 index);
	/** Shortcut to ItemHandler::ChangeItem. */
	OP_STATUS ChangeItem(const uni_char* txt, INT32 index);

	/** Shortcut for ItemHandler::BeginGroup */
	OP_STATUS BeginGroup(const uni_char* txt);
	/** Shortcut for ItemHandler::EndGroup */
	void EndGroup();
	/** Shortcut for ItemHandler::RemoveGroup */
	void RemoveGroup(int group_nr);
	/** Shortcut for ItemHandler::RemoveAllGroups */
	void RemoveAllGroups();

	/** Remove all items */
	void RemoveAll();

	/**
	 * Locks/unlocks update of this widget.
	 * Note that the widget is not updated automatically when
	 * unlocking. Update() must be explicitly called to do so.
	 */
	void LockUpdate(BOOL val)
	{
		SetLocked(val);
	}

	/* Updates this widget. */
	void Update()
	{
		if (!IsLocked())
		{
			InvalidateAll();
			UpdateScrollbar();
			UpdateScrollbarPosition();
#ifdef WIDGETS_IMS_SUPPORT
			UpdateIMS();
#endif // WIDGETS_IMS_SUPPORT
			ScrollIfNeeded();
		}
	}

	/** Shortcut for ItemHandler::GetItemUserData */
	INTPTR GetItemUserData(INT32 index) { return ih.GetItemUserData(index); }

	/** Select or unselect item at nr (excluding group items). If nr is -1, no item
		will be selected even if the listbox isn't a multiselectable listbox.
		If the listbox is not multiselectable, it will scroll to selected item. */
	void SelectItem(INT32 nr, BOOL selected);

	/** Shortcut for ItemHandler::EnableItem */
	void EnableItem(INT32 nr, BOOL enabled);

	/**
	 * This returns the number of items excluding any "begin group" and "end group"
	 * items that aren't visible in most of the API anyway.
	 */
	INT32 CountItems() { return ih.CountItems(); }
	/**
	 * This returns the number of items including any "begin group" and "end group"
	 * items.
	 */
	UINT32 CountItemsIncludingGroups() { return ih.CountItemsAndGroups(); }

	/**
	 * Returns TRUE if the element at the index
	 * (counting all elements, including starts
	 * and stops) is a group stop.
	 */
	BOOL IsElementAtRealIndexGroupStop(UINT32 idx) { return ih.IsElementAtRealIndexGroupStop(idx); }
	/**
	 * Returns TRUE if the element at the index
	 * (counting all elements, including starts
	 * and stops) is a group start.
	 */
	BOOL IsElementAtRealIndexGroupStart(UINT32 idx) { return ih.IsElementAtRealIndexGroupStart(idx); }

	/**
	 * Sets the multiple flag saying if the widget can have multiple selected or only one. Setting it to single selection when there are multiple selected items will not change anything right now but that might change.
	 *
	 * @param[in] multiple TRUE if there should be possible to have multiple selected items.
	 */
	void SetMultiple(BOOL multiple) { ih.SetMultiple(multiple); }
	BOOL IsMultiple()				{ return ih.is_multiselectable; }

	BOOL IsSelected(INT32 nr) { return ih.IsSelected(nr); }
	BOOL IsEnabled(INT32 nr) { return ih.IsEnabled(nr); }

	/** Get the nr of the focused item (excluding group items) or -1 if no item is selected. */
	INT32 GetSelectedItem();

	/** Get number of visible items */
	INT32 GetVisibleItems();

	/** Set the ScrollbarColors on the scrollbar in this listbox. */
	void SetScrollbarColors(const ScrollbarColors &colors);

	/** Move focused item back (up) or forward (down). If is_rangeaction is TRUE and the listbox
		is multiselectable, the items will be selected */
	BOOL MoveFocusedItem(BOOL forward, BOOL is_rangeaction);

	/** Inform the platform that the highlight has changed to the focused item. */
	void HighlightFocusedItem();

	/** Shortcut for ItemHandler::RecalculateWidestItem */
	void RecalculateWidestItem() { ih.RecalculateWidestItem(this); }

	void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	BorderStyle GetBorderStyle() { return border_style; }

	virtual INT32 GetValue() { return GetSelectedItem(); }

#ifndef MOUSELESS
	/** For a update fix*/
	void ResetMouseOverItem() { mouseover_item = -1; };
#endif // MOUSELESS

	virtual void ScrollIfNeeded(BOOL include_document = FALSE, BOOL smooth_scroll = FALSE);
	virtual BOOL IsScrollable(BOOL vertical);

	// == Hooks ======================
	void OnDirectionChanged();
	void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	void OnResize(INT32* new_w, INT32* new_h);
	void OnFocus(BOOL focus,FOCUS_REASON reason);
	BOOL OnScrollAction(INT32 delta, BOOL vertical, BOOL smooth = TRUE);
#ifndef MOUSELESS
	void OnMouseMove(const OpPoint &point);
	void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks, BOOL send_event);
	void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
	void OnMouseLeave();
	BOOL OnMouseWheel(INT32 delta, BOOL vertical);
#endif // !MOUSELESS
	void OnTimer();

	// Implementing the OpTreeModelItem interface
	virtual Type		GetType() {return WIDGET_TYPE_LISTBOX;}
	virtual BOOL		IsOfType(Type type) { return type == WIDGET_TYPE_LISTBOX || OpWidget::IsOfType(type); }

	// == OpInputContext ======================

	virtual BOOL		OnInputAction(OpInputAction* action);
	virtual const char*	GetInputContextName() {return "List Widget";}
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	virtual void	OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
	virtual void	OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
	virtual void	AccessibilitySendEvent(Accessibility::Event evt);

	//AccessibleListListener implementation
	virtual BOOL IsListFocused() const;
	virtual BOOL IsListVisible() const;
	virtual OP_STATUS GetAbsolutePositionOfList(OpRect &rect);
	virtual OpAccessibleItem* GetAccessibleParentOfList() {return this;}
	virtual OP_STATUS ScrollItemTo(INT32 item, Accessibility::ScrollTo scroll_to);

	// Needed by the accessibility API
	virtual int GetAccessibleChildrenCount();
	virtual OpAccessibleItem* GetAccessibleChild(int);
	virtual int GetAccessibleChildIndex(OpAccessibleItem* child);
	virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
	virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();
	virtual Accessibility::ElementKind AccessibilityGetRole() const { return Accessibility::kElementKindScrollview; }
	virtual Accessibility::State AccessibilityGetState();
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	virtual void PrefChanged (enum OpPrefsCollection::Collections id, int pref, int newvalue);
	/**
	   the number of options ([start, stop[) that need be updated when
	   SelectContent::Paint is called.  this is so that styles are
	   applied to all relevant option elements in a select.  for a
	   list box, only the options overlapped by area need be updated,
	   since drawing always comes from layout. for a closed dropdown,
	   only the selected option need be updated. for an open dropdwon,
	   all options need be updated, since drawing typically doesn't
	   come from layout.
	 */
	void GetRelevantOptionRange(const OpRect& area, UINT32& start, UINT32& stop);

#ifdef WIDGETS_IMS_SUPPORT
    /* OpIMSListener */
public:
	virtual void DoCommitIMS(OpIMSUpdateList *updates);
    virtual void DoCancelIMS();

protected:
	virtual OP_STATUS SetIMSInfo(OpIMSObject& ims_object);
#endif // WIDGETS_IMS_SUPPORT

private:
	enum TIMER_EVENT {
		TIMER_EVENT_SCROLL_UP,
		TIMER_EVENT_SCROLL_DOWN,
		TIMER_EVENT_SCROLL_X
	};
	TIMER_EVENT timer_event; ///< Used in OnTimer to know which direction
	INT32 shiftdown_item;	///< Which item we started on when selecting with keyboard
#ifndef MOUSELESS
	INT32 mousedown_item;
	INT32 mouseover_item;	///< Used to optimize
#endif // !MOUSELESS
	OpScrollbar* scrollbar;
	ListboxListener scrollbar_listener;
	BOOL MoveFocus(INT32 from_nr, INT32 nr, BOOL select);

public:
	INT32 x_scroll;
	ItemHandler ih;
	void AddMargin(OpRect *rect);
	void UpdateScrollbar();
	void UpdateScrollbarPosition();
	BOOL HasScrollbar();
	/**
	 * Returns the index of the item at the specific coordinate
	 * (relative to the upper left corner of the listbox) or -1 if
	 * none hit. If always_get_option is true, will continue search
	 * for option if item at position is group start
	 */
	int FindItemAtPosition(int y, BOOL always_get_option = FALSE);
	BOOL hot_track;		///< change item on mousemove
	BOOL grab_n_scroll;
	int is_grab_n_scrolling;

private:
	INT32 anchor_index;
	BOOL is_dragging;
};

#endif // OP_LIST_BOX_H
