/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OP_TREE_VIEW_H
#define OP_TREE_VIEW_H

#include "modules/pi/OpDragManager.h"
#include "modules/util/adt/oplisteners.h"
#include "modules/widgets/OpWidget.h"
#include "modules/widgets/AutoCompletePopup.h"

#include "adjunct/desktop_scope/src/scope_widget_info.h"
#include "adjunct/desktop_util/adt/opsortedvector.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/TreeViewModel.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/LineToItemMap.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeViewListener.h"

class TreeViewModelItem;
class ItemDrawArea;
class ColumnListAccessor;
class Edit;

/***********************************************************************************
 **
 **	OpTreeView
 **
 ***********************************************************************************/

class OpTreeView :
	public OpWidget,
	public OpTreeModel::Listener,
	public OpTreeModel::SortListener,
	public OpDelayedTriggerListener
{
public:
	class Column;

	friend class TreeViewModel;
	friend class TreeViewModelItem;
	friend class Column;

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	friend class ColumnListAccessor;
#endif

	//  -------------------------
	//  Construction, destruction and initialization:
	//  -------------------------

	static OP_STATUS Construct(OpTreeView** obj);
	~OpTreeView() { m_view_model.RemoveListener(this); }

	/**
	 * Sets the TreeModel that the OpTreeView should represent
	 *
	 * The sort_by_column and sort_acending choices can be changed
	 * by a call to Sort with the new values
	 *
	 * @param tree_model       - The TreeModel
	 * @param sort_by_column   - Which column it should be sorted by
	 * @param sort_ascending   - Items should be sorted ascending
	 * @param name             - Desired name of the OpTreeView
	 * @param clear_match_text - Whether the match text should be cleared
	 * @param always_use_sortlistener - If this param is true, a tree view always uses sort listener, even if sort column equals -1.
	 * @param grouping		   - Grouping properties to use for this treemodel, or NULL if no grouping is needed
	 */
	void SetTreeModel(OpTreeModel* tree_model,
					  INT32        sort_by_column   = -1,
					  BOOL         sort_ascending   = TRUE,
					  const char*  name             = NULL,
					  BOOL         clear_match_text = FALSE,
					  BOOL		   always_use_sortlistener = FALSE,
					  OpTreeModel::TreeModelGrouping* grouping	= NULL);

	//  ----------------------------------------------------------------------
	//  Listeners:
	//  ----------------------------------------------------------------------

	OP_STATUS AddTreeViewListener(OpTreeViewListener* listener)
		{return m_treeview_listeners.Add(listener);}

	OP_STATUS RemoveTreeViewListener(OpTreeViewListener* listener)
		{return m_treeview_listeners.Remove(listener);}


	//  ----------------------------------------------------------------------
	//  Switches:
	//
	//  The switch methods can be used to alter the behaviour of the treeview.
	//  ----------------------------------------------------------------------

	/**
	 * Switch method: The treeview has column headers
	 * Default      : FALSE
	 *
	 * @param show_column_headers
	 */
	void SetShowColumnHeaders(BOOL show_column_headers);

	/**
	 * Switch method: The treeview is multiselectable
	 * Default      : FALSE
	 *
	 * @param is_multiselectable
	 */
	void SetMultiselectable(BOOL is_multiselectable)
		{m_is_multiselectable = is_multiselectable;}

	/**
	 * Switch method: The treeview allows deselection
	 * Default      : TRUE
	 *
	 * @param is_deselectable
	 */
	void SetDeselectable(BOOL is_deselectable)
		{m_is_deselectable_by_mouse = m_is_deselectable_by_keyboard = is_deselectable;}

	/**
	 * Switch method: The treeview allows deselection using keyboard
	 * Default      : TRUE
	 *
	 * @param is_deselectable
	 */
	void SetDeselectableByKeyboard(BOOL is_deselectable)
		{m_is_deselectable_by_keyboard = is_deselectable;}

	/**
	 * Switch method: The treeview allows deselection using mouse
	 * Default      : TRUE
	 *
	 * @param is_deselectable
	 */
	void SetDeselectableByMouse(BOOL is_deselectable)
		{m_is_deselectable_by_mouse = is_deselectable;}

	/**
	 * Switch method: The treeview allows dragging
	 * Default      : FALSE
	 *
	 * @param is_drag_enabled
	 */
	void SetDragEnabled(BOOL is_drag_enabled)
		{m_is_drag_enabled = is_drag_enabled;}

	/**
	 * Switch method: Shows the threaded image
	 * Default      : FALSE
	 *
	 * @param show_thread_image
	 */
	void SetShowThreadImage(BOOL show_thread_image)
		{m_show_thread_image = show_thread_image;}

	/**
	 * Switch method: Parent folders are bold if they contain bold items
	 * Default      : FALSE
	 *
	 * @param bold_folders
	 */
	void SetBoldFolders(BOOL bold_folders)
		{m_bold_folders = bold_folders;}

	/**
	 * Switch method: Opening a folder will open all subfolders aswell
	 * Default      : FALSE
	 *
	 * @param root_opens_all
	 */
	void SetRootOpensAll(BOOL root_opens_all)
		{m_root_opens_all = root_opens_all;}

	/**
	 * Switch method: The treeview does the matching itself
	 * Default      : FALSE
	 *
	 * @param auto_match
	 */
	void SetAutoMatch(BOOL auto_match)
		{m_auto_match = auto_match;}

	/**
	 * Swith method: The matching is asynchronous - someone else is responsible for calling
	 *               UpdateAfterMatch() when the matching is done
	 * Default     : FALSE
	 *
	 * @param async_match
	 */
	void SetAsyncMatch(BOOL async_match)
		{m_async_match = async_match;}

	BOOL GetAsyncMatch() {return m_async_match;}

	/**
	 * Switch method: The user can sort - by clicking on the headers
	 * Default      : TRUE
	 *
	 * @param user_sortable
	 */
	void SetUserSortable(BOOL user_sortable)
		{ m_user_sortable = user_sortable; }

	/**
	 * Switch method: An empty folder can be opened
	 * Default      : FALSE
	 *
	 * @param allow_open_empty_folder
	 */
	void SetAllowOpenEmptyFolder(BOOL allow_open_empty_folder)
		{ m_allow_open_empty_folder = allow_open_empty_folder; }

	/**
	 * Switch method: Select another item if the selected item is removed
	 * Default      : TRUE
	 *
	 * @param reselect_when_selected_is_removed
	 */
	void SetReselectWhenSelectedIsRemoved(BOOL reselect_when_selected_is_removed)
		{ m_reselect_when_selected_is_removed = reselect_when_selected_is_removed; }

	/**
	 * Switch method: Expand an item on a single click
	 * Default      : TRUE
	 *
	 * @param expand_on_single_click
	 */
	void SetExpandOnSingleClick(BOOL expand_on_single_click)
		{m_expand_on_single_click = expand_on_single_click;}

	/**
	 * Switch method: Make item appear to have focus even when the treeview does not
	 * Default      : FALSE
	 *
	 * @param forced_focus
	 */
	void SetForcedFocus(BOOL forced_focus)
		{m_forced_focus = forced_focus;}

	/**
	 * Switch method: Items can be selected on hover
	 * Default      : FALSE
	 *
	 * @param select_on_hover
	 */
	void SetSelectOnHover(BOOL select_on_hover)
		{m_select_on_hover = select_on_hover;}

	/**
	 * Switch method: Select an item based on its first letter
	 * Default      : TRUE
	 *
	 * @param allow_select_by_letter
	 */
	void SetAllowSelectByLetter(BOOL allow_select_by_letter)
		{ m_allow_select_by_letter = allow_select_by_letter; }

	/**
	 * Switch method: Force the sortorder to be based on the strings
	 * Default      : FALSE
	 *
	 * @param force_sort_by_string
	 */
	void SetForceSortByString(BOOL force_sort_by_string)
		{m_force_sort_by_string = force_sort_by_string;}

	/**
	 * Switch method: Send MATCH_QUERY with empty strings aswell
	 * Default      : FALSE
	 *
	 * @param force_empty_match_query
	 */
	void SetForceEmptyMatchQuery(BOOL force_empty_match_query)
		{m_force_empty_match_query = force_empty_match_query;}

	/**
	 * Switch method: Allow multi line items in the treeview
	 * Default      : FALSE
	 *
	 * @param allow_multi_line_items
	 */
	void SetAllowMultiLineItems(BOOL allow_multi_line_items)
		{m_allow_multi_line_items = allow_multi_line_items;}

	/**
	 * Switch method: Allow multi line icons in the treeview
	 * Default      : FALSE
	 *
	 * @param allow_multi_line_icons
	 */
	void SetAllowMultiLineIcons(BOOL allow_multi_line_icons)
		{m_allow_multi_line_icons = allow_multi_line_icons;}

	/**
	 * Switch method: Display associated text using the "weak" color (gray)
	 * Default      : FALSE
	 *
	 * @param have_weak_associated_text
	 */
	void SetHaveWeakAssociatedText(BOOL have_weak_associated_text)
		{m_have_weak_associated_text = have_weak_associated_text;}

	/**
	 * Switch method: Force treeview to have background lines even if num cols is less than 2
	 * Default      : FALSE
	 *
	 * @param force_background_line
	 */
	void SetForceBackgroundLine(BOOL force_background_line)
		{m_force_background_line = force_background_line;}

#ifdef WIDGET_RUNTIME_SUPPORT
	/**
	 * Should treeview have background lines for items.
	 *
	 * @param paint_background_line Set to TRUE if you want have background lines
	 */
	void SetPaintBackgroundLine(BOOL paint_background_line)
			{ m_paint_background_line = paint_background_line; }
#endif // WIDGET_RUNTIME_SUPPORT

	/**
	 * Switch method: Changes the horizonal scrollbar so the full treeview will be shown
	 *					and not resized to fit the control
	 * Default      : FALSE
	 *
	 * @param show
	 */
	void SetShowHorizontalScrollbar(bool show)
		{ m_show_horizontal_scrollbar = show; }

	/**
	* Switch method: Allows scrolling with the keyboard to wrap to the top when reaching the bottom,
	*				and likewise wrap to the bottom when reaching the top.
	* Default      : FALSE
	*
	* @param wrapping TRUE to allow wrapping, FALSE to stop at the ends of the treeview
	*/
	void SetAllowWrappingOnScrolling(BOOL wrapping)
		{ m_allow_wrapping_on_scrolling = wrapping; }

	/**
	* Switch method: Allows closing all headers during OpenAllItem(FALSE)
	* Default      : FALSE
	*
	* @param wrapping TRUE to allow, FALSE to disallow
	*/
	void SetCloseAllHeaders(BOOL close_all_headers)
		{ m_close_all_headers = close_all_headers;}

	/**
	 * Change the indentation in pixels for thread children
	 * Default      : 16
	 *
	 * @param indentation
	 */
	void SetCustomThreadIndentation(INT32 indentation)
		{ m_thread_indentation = indentation; }

	void SetLinesKeepVisibleOnScroll(INT32 lines_keep_visible)
		{ m_keep_visible_on_scroll = lines_keep_visible;}

	//  ----------------------------------------------------------------------
	//  Column settings:
	//  ----------------------------------------------------------------------

	void SetThreadColumn(INT32 thread_column)
		{m_thread_column = thread_column;}

	void SetCheckableColumn(INT32 checkable_column)
		{m_checkable_column = checkable_column;}

	void SetLinkColumn(INT32 link_column)
		{m_link_column = link_column;}

	INT32 GetLinkColumn()
		{return m_link_column;}

	void SetColumnWeight(INT32 column_index, INT32 weight);

	void SetColumnFixedWidth(INT32 column_index, INT32 fixed_width);
	void SetColumnFixedCharacterWidth(INT32 column_index, INT32 character_count);

	void SetColumnImageFixedDrawArea(INT32 column_index, INT32 fixed_width);

	void SetColumnHasDropdown(INT32 column_index, BOOL has_dropdown);

	BOOL SetColumnVisibility(INT32 column_index, BOOL is_visible);
	BOOL SetColumnUserVisibility(INT32 column_index, BOOL is_visible);

	INT32 GetColumnCount()
		{return m_columns.GetCount();}

	BOOL GetColumnName(INT32 column_index, OpString& name);

	BOOL GetColumnVisibility(INT32 column_index);
	BOOL GetColumnUserVisibility(INT32 column_index);

	BOOL GetColumnMatchability(INT32 column_index);

	BOOL IsFlat();

	//  ----------------------------------------------------------------------
	//  Sorting:
	//  ----------------------------------------------------------------------

	/**
	 * Change the sort column and sort_acending choices
	 *
	 * @param sort_by_column
	 * @param sort_ascending
	 */
	void Sort(INT32 sort_by_column, BOOL sort_ascending);

	void Regroup();

	INT32 GetSortByColumn()
		{ return m_sort_by_column; }

	BOOL GetSortAscending()
		{ return m_sort_ascending; }

	//  ----------------------------------------------------------------------
	//  Drawing and Scrolling:
	//  ----------------------------------------------------------------------

	virtual void ScrollToLine(INT32 line, BOOL force_to_center = FALSE);

	void ScrollToItem(INT32 item, BOOL force_to_center = FALSE);

	void Redraw();

	/**
	 * Set custom color for background line (eg OP_RGB(0xf5, 0xf5, 0xf5))
	 *
	 * @param custom_background_line_color
	 */
	void SetCustomBackgroundLineColor(INT32 custom_background_line_color)
		{ m_custom_background_line_color = custom_background_line_color; }

	//  ----------------------------------------------------------------------
	//  Matching:
	//  ----------------------------------------------------------------------

	void SetMatch(const uni_char* match_text,
	              INT32 match_column,
	              MatchType match_type,
	              INT32 match_parent_id,
	              BOOL select,
	              BOOL force = FALSE);

	void SetMatchText(const uni_char* match_text, BOOL select = TRUE)
		{ SetMatch(match_text, m_match_column, m_match_type, m_match_parent_id, select); }

	void SetMatchColumn(INT32 match_column, BOOL select = FALSE)
		{ SetMatch(m_match_text.CStr(), match_column, m_match_type, m_match_parent_id, select); }

	void SetMatchType(MatchType match_type, BOOL select = FALSE)
		{ SetMatch(m_match_text.CStr(), m_match_column, match_type, m_match_parent_id, select); }

	void SetMatchParentID(INT32 match_parent_id, BOOL select = FALSE)
		{ SetMatch(m_match_text.CStr(), m_match_column, m_match_type, match_parent_id, select); }

	INT32 GetSavedMatchCount()
		{return m_saved_matches.GetCount();}

	BOOL GetSavedMatch(INT32 match_index, OpString& match_text);

	void AddSavedMatch(OpString& match_text);

	/**
	 * Delete all saved quick find searches
	 */
	void DeleteAllSavedMatches()
		{ m_saved_matches.DeleteAll(); WriteSavedMatches(); }

	/**
	 * Update the tree UI after a match has been completed (asks all items if they're matched)
	 *
	 * @param select Whether to select an item and scroll to it after updating the UI
	 */
	void UpdateAfterMatch(BOOL select = TRUE);

	const uni_char * GetMatchText()
		{return m_match_text.CStr();}

	BOOL HasMatch()
		{return HasMatchText() || HasMatchType() || HasMatchParentID();}

	BOOL HasMatchText()
		{return m_match_text.HasContent();}

	BOOL HasMatchType()
		{return m_match_type != MATCH_ALL;}

	BOOL HasMatchParentID()
		{return m_match_parent_id != 0;}

	//  ----------------------------------------------------------------------
	//  Selection:
	//  ----------------------------------------------------------------------

	virtual void SetSelectedItem(INT32 pos,
	                             BOOL changed_by_mouse = FALSE,
	                             BOOL send_onchange = TRUE,
	                             BOOL allow_parent_fallback = FALSE);

	void SetSelectedItemByID(INT32 id,
	                         BOOL changed_by_mouse = FALSE,
	                         BOOL send_onchange = TRUE,
	                         BOOL allow_parent_fallback = FALSE);

	INT32 GetSelectedItemPos()
		{return m_selected_item;}

	INT32 GetSelectedItemModelPos()
		{ return m_selected_item == -1 ? m_selected_item : GetModelPos(m_selected_item); }

	UINT32 GetSelectedItemCount()
		{ return m_selected_item_count; }

	OpTreeModelItem * GetSelectedItem()
		{return GetItemByPosition(m_selected_item);}

	INT32 GetSelectedItems(OpINT32Vector& list, BOOL by_id = TRUE );

	BOOL IsLastLineSelected();
	BOOL IsFirstLineSelected();

	void SelectAllItems(BOOL update = TRUE);

	BOOL SelectNext(BOOL forward = TRUE,
	                BOOL unread_only = FALSE,
	                BOOL skip_group_headers = TRUE);

	BOOL SelectByLetter(INT32 letter, BOOL forward );


	//  ----------------------------------------------------------------------
	//  TreeModel and TreeModelItems:
	//  ----------------------------------------------------------------------

	OpTreeModel * GetTreeModel()
		{ return m_view_model.GetModel(); };

	OpTreeModelItem * GetParentByPosition(INT32 pos);
	OpTreeModelItem * GetItemByPosition(INT32 pos);

	//  ----------------------------------------------------------------------
	//  The Lines :
	//  ----------------------------------------------------------------------

	/*
	* @return TRUE if new items have changed their check state since they where
	*         populated. Otherwise return FALSE.
	*/
	BOOL HasChanged();

	/**
	*
	* @return TRUE if item has changed it's check state since it was
	*         populated. Otherwise return FALSE.
	*/
	BOOL HasItemChanged(INT32 pos);

	INT32 GetChangedItems(INT32*& items);

	INT32 GetItemByID(INT32 id);

	INT32 GetModelIndexByIndex(INT32 index);

	INT32 GetItemByModelItem(OpTreeModelItem* model_item);

	INT32 GetItemByModelPos(INT32 pos)
		{return m_view_model.GetIndexByModelIndex(pos);}

	/**
	 * Convert a line number to item index.
	 *
	 * @param line line number
	 * @return index of the item "owning" the line number @a line,
	 * 		or @c -1 if there's no such item
	 */
	int GetItemByLine(int line) const;

	/**
	 * Get the index of the item containing a point.
	 *
	 * @param point a point relative to widget bounds
	 * @param truncate_to_visible_items whether the function should only look
	 *		for items that are currently visible (i.e., not scrolled out)
	 * @return index of the item whose area contains @a point, or @c -1 if
	 * 		there's no such item
	 */
	INT32 GetItemByPoint(OpPoint point, BOOL truncate_to_visible_items = FALSE);

	/**
	 * @return number of items in the tree view
	 */
	INT32 GetItemCount() const
		{ return m_item_count; }

	/**
	 * Get the index of the line containing a point.
	 *
	 * @param point a point relative to widget bounds
	 * @param truncate_to_visible_items whether the function should only look
	 *		for items that are currently visible (i.e., not scrolled out)
	 * @return index of the line whose area contains @a point, or @c -1 if
	 * 		there's no such line
	 */
	INT32 GetLineByPoint(OpPoint point, BOOL truncate_to_visible_lines = FALSE);

	BOOL IsThreadImage(OpPoint point, INT32 pos);

	int GetLineByItem(int pos);

	INT32 GetLineCount()
		{ return m_line_map.GetCount(); };

	int GetLineHeight()
		{ return m_line_height; }

	INT32 GetModelPos(INT32 pos)
		{return m_view_model.GetModelIndexByIndex(pos);}

	INT32 GetChild(INT32 pos)
		{return m_view_model.GetChildIndex(pos);}

	INT32 GetSibling(INT32 pos)
		{return m_view_model.GetSiblingIndex(pos);}

	INT32 GetParent(INT32 pos)
		{return m_view_model.GetParentIndex(pos);}

	BOOL GetCellRect(INT32 pos,
	                 INT32 column_index,
	                 OpRect& rect,
	                 BOOL text_rect = FALSE);
	//  ----------------------------------------------------------------------
	//  The items :
	//  ----------------------------------------------------------------------

	BOOL IsItemOpen(INT32 pos);
	BOOL IsItemVisible(INT32 pos);
	BOOL IsItemChecked(INT32 pos);
	BOOL IsItemDisabled(INT32 pos);

	BOOL CanItemOpen(INT32 pos);
	void OpenItem(INT32 pos, BOOL open, BOOL scroll_to_fit = TRUE);

	/**
	 * Change the open state of an item an let the listener know about it
	 */
	virtual void ChangeItemOpenState(TreeViewModelItem* item, BOOL open);

	void OpenAllItems(BOOL open);

	void ToggleItem(INT32 pos);

	void SetCheckboxValue(INT32 pos, BOOL checked);

	//  ----------------------------------------------------------------------
	//  The Widgets :
	//  ----------------------------------------------------------------------

	INT32 GetEditColumn()
		{return m_edit ? m_edit_column : -1;}

	INT32 GetEditPos()
		{return m_edit ? m_edit_pos : -1;}

	void EditItem(INT32 pos,
	              INT32 column = 0,
	              BOOL always_use_full_column_width = FALSE,
	              AUTOCOMPL_TYPE autocomplete_type = AUTOCOMPLETION_OFF);

	/**
	 * Removes edit field and ignores any changes
	 *
	 * @param broadcast Notify listener if such exists if TRUE
	 */
	void CancelEdit(BOOL broadcast = FALSE);

	void FinishEdit()
		{if (m_edit) g_input_manager->InvokeAction(OpInputAction::ACTION_EDIT_ITEM, 0, NULL, this);}


	OpRect GetEditRect();

	BOOL IsScrollable(BOOL vertical);

	void StopPendingOnChange()
		{ m_send_onchange_later = FALSE; }

	//  ----------------------------------------------------------------------
	//  Accessibility:
	//  ----------------------------------------------------------------------

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

	int GetItemAt(int x, int y);
	int GetColumnAt(int x, int y);

	Accessibility::State GetUnfilteredState();

	void CreateColumnListAccessorIfNeeded();

	// == OpAccessibilityExtensionListener ======================

	virtual Accessibility::ElementKind AccessibilityGetRole() const
		{return Accessibility::kElementKindScrollview;}

	virtual Accessibility::State AccessibilityGetState();

	virtual void OnKeyboardInputGained(OpInputContext* new_input_context,
	                                   OpInputContext* old_input_context,
	                                   FOCUS_REASON reason);

	virtual void OnKeyboardInputLost(OpInputContext* new_input_context,
	                                 OpInputContext* old_input_context,
	                                 FOCUS_REASON reason);

	virtual int	GetAccessibleChildrenCount();
	virtual int GetAccessibleChildIndex(OpAccessibleItem* child);

	virtual OpAccessibleItem* GetAccessibleChild(int);
	virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
	virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();
#endif

	// == OpTreeModel::Listener ======================

	virtual void   OnItemAdded(OpTreeModel* tree_model, INT32 item);
	virtual void   OnItemChanged(OpTreeModel* tree_model, INT32 item, BOOL sort);
	virtual void   OnItemRemoving(OpTreeModel* tree_model, INT32 index);
	virtual void   OnSubtreeRemoving(OpTreeModel* tree_model, INT32 parent_index, INT32 index, INT32 subtree_size);
	virtual void   OnSubtreeRemoved(OpTreeModel* tree_model, INT32 parent_index, INT32 index, INT32 subtree_size);
	virtual void   OnTreeChanging(OpTreeModel* tree_model);
	virtual void   OnTreeChanged(OpTreeModel* tree_model);
	virtual void   OnTreeDeleted(OpTreeModel* tree_model) {}

	// == OpTreeModel::SortListener ======================

	virtual INT32  OnCompareItems(OpTreeModel* tree_model, OpTreeModelItem* item1, OpTreeModelItem* item2);

	// == OpToolTipListener ======================

	virtual BOOL   HasToolTipText(OpToolTip* tooltip) {return TRUE;}
	virtual void   GetToolTipText(OpToolTip* tooltip, OpInfoText& text);
	virtual void   GetToolTipThumbnailText(OpToolTip* tooltip, OpString& title, OpString& url_string, URL& url);
	virtual INT32  GetToolTipItem(OpToolTip* tooltip);
	virtual INT32  GetToolTipUpdateDelay(OpToolTip* tooltip) {return 100;}
	virtual OpToolTipListener::TOOLTIP_TYPE GetToolTipType(OpToolTip* tooltip); 

	// == WidgetListener ======================

	virtual void   OnScroll(OpWidget *widget, INT32 old_val, INT32 new_val, BOOL caused_by_input);
	virtual void   OnRelayout(OpWidget* widget) {}

	// == OpWidget ======================

	virtual void	SetName(const char* name) {OpWidget::SetName(name); ReadColumnSettings(); ReadSavedMatches();}
	virtual OP_STATUS SetText(const uni_char* text) {SetMatchText(text); return OpStatus::OK;}
	virtual DesktopDragObject* GetDragObject(OpTypedObject::Type type, INT32 x, INT32 y);
	virtual void	OnDeleted();
	virtual void	OnLayout();
	virtual void	OnClick(OpWidget *widget, UINT32 id);
	virtual void	OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	virtual void	OnFocus(BOOL focus,FOCUS_REASON reason);
	virtual void	OnDragStart(const OpPoint& point);
	virtual void	OnDragLeave(OpDragObject* drag_object);
	virtual	void	OnDragCancel(OpDragObject* drag_object);
	virtual void	OnDragMove(OpDragObject* drag_object, const OpPoint& point);
	virtual void	OnDragDrop(OpDragObject* drag_object, const OpPoint& point);
	virtual void	OnTimer();
	virtual void	OnShow(BOOL show);
	virtual void	OnMouseLeave();
	virtual void	OnMouseMove(const OpPoint &point);
	virtual void	OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual void	OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual BOOL	OnMouseWheel(INT32 delta,BOOL vertical);
	virtual void	OnSetCursor(const OpPoint &point);
	virtual void	SetRestrictImageSize(BOOL b) { OpWidget::SetRestrictImageSize(b); m_restrict_image_size = b; }
	virtual void	GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);
	virtual void	OnScaleChanged();
	virtual BOOL	OnScrollAction(INT32 delta, BOOL vertical, BOOL smooth);

	// == OpWidgetImageListener ======================

	virtual void   OnImageChanged(OpWidgetImage* widget_image);

	// == OpDelayedTriggerListener ======================

	virtual void   OnTrigger(OpDelayedTrigger*) {UpdateDelayedItems();}

	// == OpTreeModelItem ======================

	virtual Type   GetType() {return WIDGET_TYPE_TREEVIEW;}
	virtual BOOL   IsOfType(Type type) { return type == WIDGET_TYPE_TREEVIEW || OpWidget::IsOfType(type); }

	// == OpInputContext ======================

	virtual BOOL        OnInputAction(OpInputAction* action);
	virtual const char* GetInputContextName() {return "Tree Widget";}
	BOOL                IsHoveringAssociatedImageItem(INT32 pos) { return pos != -1 && m_hover_associated_image_item == pos; }

#ifdef ANIMATED_SKIN_SUPPORT
	virtual void        OnAnimatedImageChanged(OpWidgetImage* widget_image);
#endif

	void SetExtraLineHeight(UINT32 extra_height) { m_extra_line_height = extra_height; CalculateLineHeight(); }

	/** Set to TRUE if associated images should only be showed on the hovered item. Disabled by default. */
	void SetOnlyShowAssociatedItemOnHover(BOOL status) { m_only_show_associated_item_on_hover = status; }

	// UI Automation (scope)
	virtual OpScopeWidgetInfo* CreateScopeWidgetInfo();
	OP_STATUS   AddQuickWidgetInfoToList(OpScopeDesktopWindowManager_SI::QuickWidgetInfoList &list, TreeViewModelItem* item, INT32 real_col, const OpRect cell_rect, INT32 row, INT32 col);
	OP_STATUS   AddQuickWidgetInfoInvisibleItems(OpScopeDesktopWindowManager_SI::QuickWidgetInfoList &list, INT32 start_pos, INT32 end_pos);

protected:

	OpTreeView();

	void SetDropPos(INT32 drop_pos) { m_drop_pos = drop_pos; }

private:

	/** Called for every custom widget item that is removed from the model. */
	void   OnCustomWidgetItemRemoving(TreeViewModelItem *item, OpWidget* widget = NULL);

	void   SetSelected(INT32 item_index, BOOL selected);

	OpTreeView::Column*	GetColumnByIndex(INT32 column_index);

	/**
	 * @return the visible rect relative to widget bounds
	 */
	OpRect GetVisibleRect(bool include_column_headers = false, bool include_scrollbars = false, bool include_group_header = false)
		{ return AdjustForDirection(GetVisibleRectLTR(include_column_headers, include_scrollbars, include_group_header)); }

	/**
	 * @return the visible rect relative to widget bounds.  The rect is
	 * 		computed for LTR even if the widget is RTL.
	 */
	OpRect GetVisibleRectLTR(bool include_column_headers = false, bool include_scrollbars = false, bool include_group_header = false);

	BOOL GetItemRect(INT32 pos, OpRect& rect, BOOL only_visible = TRUE);
	/**
	 * @rect receives the item rect, in LTR coords even if widget is RTL
	 * @see GetItemRect
	 */
	BOOL GetItemRectLTR(INT32 pos, OpRect& rect, BOOL only_visible = TRUE);

	/**
	 * @rect receives the cell rect, in LTR coords even if widget is RTL
	 * @see GetCellRect
	 */
	BOOL GetCellRectLTR(INT32 pos,
	                    INT32 column_index,
	                    OpRect& rect,
	                    BOOL text_rect = FALSE);

	void   ComputeTotalSize();

	bool   UpdateScrollbars();

	void   SelectItem(INT32 pos,
	                  BOOL force_change = FALSE,
	                  BOOL control = FALSE,
	                  BOOL shift = FALSE,
	                  BOOL changed_by_mouse = FALSE,
	                  BOOL send_onchange = TRUE);

	BOOL   OpenItemRecursively(INT32 pos,
	                           BOOL open,
	                           BOOL recursively, 
	                           BOOL update_lines = TRUE);

	void   PaintTree(OpWidgetPainter* widget_painter, 
	                 const OpRect &paint_rect, 
	                 OpScopeDesktopWindowManager_SI::QuickWidgetInfoList *list = NULL,
	                 BOOL include_invisible = TRUE);

	int    PaintItem(OpWidgetPainter* widget_painter,
		             INT32 pos,
		             OpRect paint_rect,
					 OpRect rect,
		             BOOL background_line,
		             OpScopeDesktopWindowManager_SI::QuickWidgetInfoList *list = NULL);

	void   PaintGroupHeader(OpWidgetPainter* widget_painter,
		             const OpRect &paint_rect,
		             BOOL background_line);

	void   PaintItemBackground(INT32 pos, OpSkinElement* item_skin_element, INT32 skin_state, BOOL background_line, const OpRect& rect);

	void   PaintSelectedItemBackground(const OpRect &paint_rect, BOOL is_selected_item);

	void   PaintCell(OpWidgetPainter* widget_painter,
	                 OpSkinElement *item_skin_elm,
	                 INT32 state,
	                 INT32 pos,
	                 OpTreeView::Column* column,
	                 OpRect& cell_rect,
	                 OpScopeDesktopWindowManager_SI::QuickWidgetInfoList *list = NULL);

	void   PaintText(OpWidgetPainter* widget_painter,
		             OpSkinElement *item_skin_elm,
		             INT32 state,
		             OpRect& cell_rect,
		             TreeViewModelItem* item,
		             ItemDrawArea* cache,
		             BOOL use_weak_style = FALSE,
		             BOOL use_link_style = FALSE,
		             BOOL use_bold_style = FALSE,
		             int right_offset = 0,
		             int *cell_width = NULL);

	/**
	 * @param get_size_only whether to really paint the image or just get its size
	 * @param item the item the thread image is for
	 * @param depth threading depth
	 * @param parent_rect will draw relative to this rect
	 * @return the rect of the thread image
	 */
	OpRect  PaintThreadImage(bool get_size_only,
	                         TreeViewModelItem* item,
	                         int depth,
	                         const OpRect& parent_rect);

	BOOL   HandleGroupHeaderMouseClick(const OpPoint& point, MouseButton button);
	bool SetTopGroupHeader();

	BOOL   MatchItem(INT32 pos, BOOL check = FALSE, BOOL update_line = TRUE);
	BOOL   UpdateSearch(INT32 pos, BOOL check = FALSE, BOOL update_line = TRUE);
	void   Rematch(INT32 pos);

	void   InitItem(INT32 pos);
	void   UpdateItem(INT32 pos, BOOL clear, BOOL delay = FALSE);

	void   SetTreeModelInternal(OpTreeModel* tree_model);
	void   TreeChanged();

	void   UpdateLines(INT32 pos = -1);
	void   UpdateSortParameters(OpTreeModel* model, INT32 sort_by_column, BOOL sort_ascending);
	void   UpdateDelayedItems();

	void   GetInfoText(INT32 pos, OpInfoText& text);

	BOOL   StartColumnSizing(OpTreeView::Column* column, const OpPoint& point, BOOL check_only);
	void   DoColumnSizing(OpTreeView::Column* column, const OpPoint& point);
	void   EndColumnSizing();
	BOOL   IsColumnSizing() {return m_is_sizing_column;}

	void   ReadColumnSettings();
	void   WriteColumnSettings();

	void   ReadSavedMatches();
	void   WriteSavedMatches();

	void   StartScrollTimer();
	void   StopScrollTimer();

	void   UpdateDragSelecting();
	void   CalculateLineHeight();

	// Insert all the lines of this item
	OP_STATUS InsertLines(TreeViewModelItem * item);

	// Insert all the lines of the item with this index
	OP_STATUS InsertLines(int index, int num_lines = -1);

	// Remove all the lines of the item with this index
	OP_STATUS RemoveLines(int index);

	// Get the number of lines this item requires
	int GetNumLines(TreeViewModelItem * item);

	// Get the visible index to the item that owns the line (beware this is slow)
	int GetVisibleItemPos(int line);

	// Calculates the total width need if everything is fixed width. It doesn't add the scrollbar width.
	// Used in combo with horizonal scroll bars
	int GetFixedWidth();

	// calculates the rect needed for the associated image
	void CalculateAssociatedImageSize(ItemDrawArea* associated_image, const OpRect& item_rect, OpRect& associated_image_rect);

	BOOL HasGrouping() const
		{ return m_view_model.GetTreeModelGrouping() ? m_view_model.GetTreeModelGrouping()->HasGrouping() : FALSE; }

//  -------------------------
//  Private member variables:
//  -------------------------

	//  -------------------------
	//  Switches:
	//  -------------------------

	BOOL m_show_column_headers;					// The treeview has column headers
	BOOL m_is_multiselectable;					// The treeview is multiselectable
	BOOL m_is_deselectable_by_keyboard;			// The treeview allows deselection from keyboard action
	BOOL m_is_deselectable_by_mouse;			// The treeview allows deselection from mouse action
	BOOL m_is_drag_enabled;						// The treeview allows dragging
	BOOL m_show_thread_image;					// Shows the threaded image
	BOOL m_bold_folders;						// Parent folder are bold if they contain bold items
	BOOL m_root_opens_all;						// Opening a folder will open all subfolders aswell
	BOOL m_auto_match;							// The treeview does the matching itself
	BOOL m_async_match;							// Matching is done by an asynchronous method
	BOOL m_user_sortable;						// The user can sort - by clicking on the headers
	BOOL m_allow_open_empty_folder;				// An empty folder can be opened
	BOOL m_reselect_when_selected_is_removed;	// Select another item if the selected item is removed
	BOOL m_expand_on_single_click;				// Expand an item on a single click
	BOOL m_forced_focus;						// Make item appear to have focus even when the treeview does not
	BOOL m_select_on_hover;						// Items can be selected on hover
	BOOL m_allow_select_by_letter;				// Select an item based on its first letter
	BOOL m_force_sort_by_string;				// Force the sortorder to be based on the strings
	BOOL m_force_empty_match_query;				// Send MATCH_QUERY with empty strings aswell
	BOOL m_allow_multi_line_items;				// Allow items to span multiple lines
	BOOL m_restrict_image_size;					// If TRUE will restrict image size to 16*16
	BOOL m_allow_multi_line_icons;				// Allow icons to span multiple lines
	BOOL m_have_weak_associated_text;			// Display the associated text as weak (gray)
	BOOL m_force_background_line;				// Force treeview to have background lines even if num cols is less than 2
	BOOL m_allow_wrapping_on_scrolling;			// Allow that scrolling with the keyboard and reaching the bottom will wrap around to the top
	BOOL m_paint_background_line;				// Should treeview contain background lines
	BOOL m_close_all_headers;					// OpenAllItems(FALSE) should close also all headers

	//  -------------------------
	//  Constants:
	//  -------------------------

	INT32 m_column_headers_height;	//(Should maybe not be a member var?)

	//  -------------------------
	//  State:
	//  -------------------------

	BOOL m_is_drag_selecting;	// State - is currently in a drag
	bool m_changed;				// State - detect changes while drawing (Should maybe not be a member var?)

	//  -------------------------
	//  Widgets:
	//  -------------------------

	// Currently active editable field:
	Edit* m_edit;
	INT32 m_edit_pos;		// Line the edit field is on
	INT32 m_edit_column;	// Column the edit field is in

	// Empty, inactive button - filler in right top corner between scrollbar and column headers
	OpButton* m_column_filler;

	// Scrollbars :
	OpScrollbar*      m_horizontal_scrollbar;
	OpScrollbar*      m_vertical_scrollbar;
	OpWidgetListener* m_scrollbar_listener;
	bool              m_show_horizontal_scrollbar;

	//  -------------------------
	//  Drag and drop:
	//  -------------------------

	INT32		m_drop_pos;		// The current drop position : where a drop would occur right now
	DesktopDragObject::InsertType	m_insert_type;	// Type of insert : INSERT_INTO, INSERT_BEFORE, INSERT_AFTER

	//  -------------------------
	//  Painting:
	//  -------------------------

	OpDelayedTrigger            m_redraw_trigger;
	OpVector<TreeViewModelItem> m_items_that_need_redraw;
	OpINTSet   m_item_ids_that_need_string_refresh;
	int        m_line_height;
	BOOL       m_is_centering_needed;
	INT32      m_custom_background_line_color;   // Custom color for background line (eg 0xf5f5f5)
	UINT32     m_extra_line_height;              // Additional line height
	BOOL       m_only_show_associated_item_on_hover;
	bool       m_save_strings_that_need_refresh; // save which strings that need to be refreshed (because the scale changed)

	//  -------------------------
	//  Matching:
	//  -------------------------

	OpString               m_match_text;		// The currently search for text - set by SetMatch
	INT32                  m_match_column;		// - set by SetMatch
	MatchType              m_match_type;		// - set by SetMatch
	INT32                  m_match_parent_id;	// - set by SetMatch
	OpAutoVector<OpString> m_saved_matches;		// - added to by a call to AddSavedMatch

	//  -------------------------
	//  Columns:
	//  -------------------------

	OpWidgetVector<OpTreeView::Column>  m_columns;	// The columns of the treeview
	INT32   m_visible_column_count;		// The columns currently visible
	INT32   m_thread_column;			// Index of the column that is treaded
	INT32   m_checkable_column;			// Index of the column with the checkboxes
	INT32   m_link_column;				// Index of the column with the links
	BOOL    m_column_settings_loaded;	// Flag to indicate that the column settings have been loaded

	//  -------------------------
	//  Custom widgets:
	//  -------------------------

	OpVector<OpWidget>   m_custom_widgets;
	OpVector<OpWidget>   m_custom_painted_widgets;

	//  -------------------------
	//  TreeModel:
	//  -------------------------

	TreeViewModel m_view_model;		// The model this OpTreeView represents

	//  -------------------------
	//  Selection:
	//  -------------------------

	INT32  m_selected_item;			// The index/line of the currently selected item
	UINT32 m_selected_item_count;	// The number of selected items
	INT32  m_shift_item;			// The previously selected item when a new one has been selected with shift down
	INT32  m_hover_item;			// The currently hovered item
	INT32  m_hover_associated_image_item;	// The currently hovered associated image

	INT32  m_reselect_selected_line;// Whether a reselect should occur (Should maybe not be a member var?)
	INT32  m_reselect_selected_id;	// Id of item that should be selected (Should maybe not be a member var?)
	INT32  m_timed_hover_pos;		//
	INT32  m_timer_counter;			//
	BOOL   m_send_onchange_later;	//
	BOOL   m_selected_on_mousedown;	//

	//  -------------------------
	//  Sorting:
	//  -------------------------

	INT32 m_sort_by_column;			// Which column it should be sorted by
	BOOL  m_sort_ascending;			// Items should be sorted ascending
	BOOL  m_sort_by_string_first;	// Set by the GetColumnData request to the model (default FALSE)
	BOOL  m_custom_sort;			// Set by the GetColumnData request to the model (default FALSE)
	BOOL  m_always_use_sortlistener;

	//  -------------------------
	//  Sizing:
	//  -------------------------

	OpAutoArray<OpRect> m_sizing_columns;
	BOOL    m_is_sizing_column;
	INT32   m_sizing_column_left;
	INT32   m_sizing_column_right;
	INT32   m_start_x;
	INT32   m_thread_indentation;     // 16 by default
	INT32   m_keep_visible_on_scroll; // 2 by default

	//  -------------------------
	//  Items:
	//  -------------------------

	INT32 m_item_count; // The number of items

	LineToItemMap m_line_map;

	//  -------------------------
	//  Listeners: List of the listeners currently listening to the OpTreeView
	//  -------------------------

	OpListeners<OpTreeViewListener>	m_treeview_listeners;

	//  -------------------------
	//  Accessibility:
	//  -------------------------

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	ColumnListAccessor* m_column_list_accessor;
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	OpPoint m_anchor; // Prevents mouse to interfere with keyboard navigation

	struct GroupHeaderInfo
	{
		INT32 pos;
		OpRect rect;
		TreeViewModelItem* item;
	};

	OpAutoVector<GroupHeaderInfo> m_group_headers_to_draw;

	class ScopeWidgetInfo;
	friend class ScopeWidgetInfo;
};

#endif // OP_TREE_VIEW_H
