/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_TREE_VIEW_DROPDOWN_H
#define OP_TREE_VIEW_DROPDOWN_H

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/desktop_util/treemodel/optreemodelitem.h"

#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

#include "modules/widgets/OpDropDown.h"

class OpTreeViewWindow;

/**
 * @brief A dropdown containing a treeview
 * @author Patricia Aas
 *
 * Currently OpAddressDropdown is an example of use of this dropdown. In that
 * case it subclasses OpTreeViewDropdown. The dropdown displays a treeview,
 * which the OpTreeViewDropdown can create for you in MakeNewModel. When an item
 * in the dropdown is selected, the item->GetUserData() (which is assumed to be
 * a uni_char pointer) text is (as a rule) set in the edit field. If you want an
 * action to be preformed on the string, the action can be set by calling SetAction.
 *
 * Setting up a dropdown :
 * -----------------------
 *
 * InitTreeViewDropdown - will make sure a clean dropdown is ready.
 *
 * MakeNewModel - will make a new SimpleTreeModel with the specified columns and
 *                set it as the model for the dropdown. The model can from then on
 *                be accessed by a call to GetModel()
 *
 * SetModel - an alternative to MakeNewModel if you want to set the model to your
 *            own exsisting model.
 *
 * SetAction - set the action that should be invoked if an item in the dropdown is
 *             selected, and if SetInvokeActionOnClick is set as TRUE then also when
 *             an item is clicked. NOTE: the action is invoked with ActionDataString
 *             set to item->GetUserData() which is assumed to be a uni_char pointer.
 *
 * Important events to handle in the subclass are :
 * ------------------------------------------------
 *
 * OnChange - will called for every letter the user typed, to facilitate an
 *            autocompletion.
 *
 * OnClear  - will called when the dropdown is cleared, and is the time to
 *            clean up any datastructures that are not to persist beyond one
 *            display of the dropdown.
 *
 * Runtime Tweaks:
 * ---------------
 *
 * SetInvokeActionOnClick - whether the action set in SetAction should be invoked
 *                          if an item in the treeview is clicked.
 *
 * SetMaxNumberOfColumns - sets the number of allowed columns, this makes it possible
 *                         for example for one instance of the OpAddressDropdown in
 *                         SpeedDial to only have one column, while another in the
 *                         F2/GoToPage Dialog can have two.
 *
 * Compiletime Tweaks:
 * -------------------
 *
 * WIDGETS_UP_DOWN_MOVES_TO_START_END (mac)
 *
 * This is to make the up arrow actually move up one item in the list rather than
 * move to the start of the edit field which is the normal behaviour on mac. The
 * previous bug mean that after changing to a new url the caret would be placed at
 * the end of the text mean that the next arrow up moved the cursor to the front.
 * By ignoring the "move to line start" action the up arrow will move up one item as
 * expected.
 */
 
class OpTreeViewDropdown :
	public OpDropDown,
	private DesktopWindowListener
{
	friend class TreeViewWindow;

public:

	/**
	 * @see SetEditListener
	 */
	class EditListener
	{
	public:
		virtual ~EditListener() {}

		virtual void OnTreeViewDropdownEditChange(OpTreeViewDropdown* dropdown, const OpStringC& text) = 0;
	};


//  -------------------------
//  Public member functions:
//  -------------------------

	/**
	 * Construct an OpTreeViewDropdown
	 *
	 * @param obj pointer to set to the constructed OpTreeViewDropdown
	 *
	 * @return OpStatus::OK if successful
	 */
	static OP_STATUS Construct(OpTreeViewDropdown ** obj, BOOL edit_field = TRUE);

	/**
	 * Will make sure there is a clear dropdown window
	 *
	 * @return OpStatus::OK if successful
	 */
	OP_STATUS InitTreeViewDropdown(const char *transp_window_skin = NULL);

	/**
	 * Sets the listener to be notified of all edit field text changes.
	 *
	 * You need to set this listener, and use it in addition to
	 * OpWidgetListener::OnChange(), if you want notifications for all edit
	 * field changes.  OpWidgetListener::OnChange() is suppressed when the edit
	 * field changes due to something other than typing.
	 *
	 * @param listener the listener, may be @c NULL
	 */
	void SetEditListener(EditListener* listener) { m_edit_listener = listener; }

	/**
	 * Sets the treemodel of the dropdown, takes over
	 * the ownership of the model.
	 *
	 * @param tree_model               - treemodel that should be used
	 * @param items_owned_by_dropdown  - whether the items should be deleted by the dropdown
	 *
	 * @return OpStatus::OK if successful
	 */
	OP_STATUS SetModel(GenericTreeModel * tree_model,   BOOL items_owned_by_dropdown);

	/**
	 * @return a pointer to the treemodel currently in the dropdown
	 */
	GenericTreeModel * GetModel();

	/**
	 * @return the selected item
	 */
	OpTreeModelItem * GetSelectedItem(int * position = NULL);

	/**
	* Sets the selected item in the treeview
	*
	* @param item  - item to select
	*
	*/
	void SetSelectedItem(OpTreeModelItem *item, BOOL selected);

	/**
	 * @return a pointer to the treeview currently in the dropdown
	 */
	OpTreeView * GetTreeView();

	/**
	* Will resize the dropdown, if it's visible
	*/
	virtual void ResizeDropdown();

	/**
	 * Will display/hide the dropdown
	 *
	 * @param show
	 */
	virtual void ShowDropdown(BOOL show);

	/**
	 * Show dropdown at specific coordinate.
	 * 
	 * @param show Flag used to show and hide dropdown.
	 * @param rect Meaningful when show flag is TRUE.
	   Rect should be in screen-coordinate system
	 */
	virtual void ShowDropdown(BOOL show, OpRect rect);

	/**
	 * Should be called from the destructor of the TreeViewWindow
	 * Useful if the window gets destroyed by the platform
	 */
	void OnWindowDestroyed() {m_tree_view_window = NULL;}

	/**
	 * @return TRUE if the dropdown is currently open
	 */
	BOOL DropdownIsOpen();

	/**
	 * Sets whether the action should be performed if an item is clicked
	 * in the dropdown.
	 *
	 * @param setting TRUE if the action should be performed
	 */
	void SetInvokeActionOnClick(BOOL setting) { m_invoke_action_on_click = setting; }

	/**
	 * Sets the maximum number of columns to be shown ( current max = 3 )
	 *
	 * @param num - the maximum number of columns
	 */
	void SetMaxNumberOfColumns(int num) { m_max_number_of_columns = num; }

	/**
	* Gets the maximum number of columns to be shown ( current max = 3 )
	*
	* @return the maximum number of columns
	*/
	int GetMaxNumberOfColumns() { return m_max_number_of_columns; }

	/**
	 * Save the string containing what the user has actually typed, this
	 * is used when the edit field should be reset after displaying things
	 * while traversing the treeview for example.
	 *
	 * @param user_string - the current user-typed text
	 */
	void SetUserString(const uni_char * user_string) { m_user_string.Set(user_string); }

	/**
	 * @return true if the dropdown has focus - which means that the edit field has focus
	 */
	BOOL HasFocus();

	virtual UINT32 GetColumnWeight(UINT32 column);

	/**
	 * Hook to be notified when the treemodel is being emptied
	 */
	virtual void OnClear() {}

	/**
	 * Hook to be notified when an action is being invoked
	 */
	virtual void OnInvokeAction(BOOL invoked_on_user_typed_string) {}

	/** Get (single-line) text that should be displayed in dropdown edit field for selected item
	  * @param item_text The single-line text associated with the selected item
	  */
	virtual OP_STATUS GetTextForSelectedItem(OpString& item_text);

	/** Called before invoking the item when it is clicked.
	* @return TRUE if the item should be invoked. FALSE will abort invoking the item and the
	*         dropdown will not be closed.
	*/
	virtual BOOL OnBeforeInvokeAction(BOOL invoked_on_user_typed_string) { return TRUE; }

	// == OpWidget ======================

	virtual void SetAction(OpInputAction* action);

	virtual OpInputAction * GetAction();

	virtual void OnResize(INT32* new_w, INT32* new_h);

	void	OnDeleted();

	// == OpDropdown ======================

	virtual OP_STATUS SetText(const uni_char* text);

	virtual Type GetType() { return m_widget_type; }

	// == OpWidgetListener ======================

	virtual void OnMouseEvent(OpWidget *widget,
							  INT32 pos,
							  INT32 x,
							  INT32 y,
							  MouseButton button,
							  BOOL down,
							  UINT8 nclicks);

	virtual void OnDragMove(OpWidget* widget,
							OpDragObject* drag_object,
							INT32 pos,
							INT32 x,
							INT32 y);

	virtual void OnDragDrop(OpWidget* widget,
							OpDragObject* drag_object,
							INT32 pos,
							INT32 x,
							INT32 y);

	virtual void OnDragLeave(OpWidget* widget,
							 OpDragObject* drag_object);

	virtual BOOL OnContextMenu(OpWidget* widget,
							   INT32 child_index, 
							   const OpPoint& menu_point,
							   const OpRect* avoid_rect,
							   BOOL keyboard_invoked);

	virtual void OnInvokeAction(OpWidget *widget,
								BOOL invoked_by_mouse);

	// == OpInputContext ======================

	virtual BOOL OnInputAction(OpInputAction* action);

	virtual const char*	GetInputContextName() { return "TreeView Dropdown Widget"; }

	virtual void OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);

	virtual void OnKeyboardInputLost(OpInputContext* new_input_context,
									 OpInputContext* old_input_context,
									 FOCUS_REASON reason);

	virtual void OnCandidateShow(BOOL visible);
	virtual void OnStopComposing(BOOL /*cancel*/);

	/**
	* Extra pixels to be added to the default line height per item.
	* 
	* @param extra_height - extra height in pixels
	*/
	void SetExtraLineHeight(UINT32 extra_height) { m_extra_line_height = extra_height; }

	/**
	* Control whether the dropdown will be shown when the edit field is activated
	* 
	* @param show_dropdown_on_activation - If TRUE, show the dropdown when the edit field is activated. Default is FALSE.
	*/
	void SetShowDropdownOnActivation(BOOL show_dropdown_on_activation) { m_show_dropdown_on_activation = show_dropdown_on_activation; }

	/**
	* Control whether keyboard scrolling up will activated the edit field and de-select the selected item
	* 
	* @param deselect_on_scroll_end - If FALSE, do not de-select the active item at top or bottom. Default is TRUE.
	*/
	void SetDeselectOnScrollEnd(BOOL deselect_on_scroll_end) { m_deselect_on_scroll_end = deselect_on_scroll_end; }

	/**
	* Switch method: Allows scrolling with the keyboard to wrap to the top when reaching the bottom,
	*				and likewise wrap to the bottom when reaching the top.
	* Default      : FALSE
	*
	* @param wrapping TRUE to allow wrapping, FALSE to stop at the ends of the treeview
	*/
	void SetAllowWrappingOnScrolling(BOOL wrapping)	{ m_allow_wrapping_on_scrolling = wrapping; }

	/**
	 * Disable dropdown window. Needed in certain configurations. 
	 */
	void SetEnableDropdown(BOOL enable_dropdown_window) { m_enable_dropdown_window = enable_dropdown_window; }


	virtual void AdjustSize(OpRect& rect) { };

	/**
	* Override the preferences setting for max lines to show in the drop down. 
	* 
	* @param max_lines Max number of lines to show in the drop down.
	*/
	void SetMaxLines(int max_lines);

	int GetMaxLines() { return m_max_lines; }

	void SetTreeViewName(const char* name);

	OpTreeViewWindow * GetTreeViewWindow() { return m_tree_view_window; }

	void	SetType(Type type) { m_widget_type = type; }

protected:

//  -------------------------
//  Protected member functions:
//  -------------------------

	OpTreeViewDropdown(BOOL edit_field=TRUE);

protected:
//  -------------------------
//  Protected member functions:
//  -------------------------

	// == DesktopWindowListener ======================

	virtual void OnDesktopWindowClosing(DesktopWindow* desktop_window,
										BOOL user_initiated);

private:
//  -------------------------
//  Private member variables:
//  -------------------------

	// The dropdown window :
	OpTreeViewWindow * m_tree_view_window;

	// Settings :
	BOOL		m_invoke_action_on_click;
	OpString	m_user_string;
	int			m_max_number_of_columns;
	UINT32		m_extra_line_height;            // Additional line height
	BOOL		m_show_dropdown_on_activation;	// show dropdown when the edit field is activated
	BOOL		m_deselect_on_scroll_end;		// de-select the dropdown when reaching the top/bottom
	BOOL		m_allow_wrapping_on_scrolling;	// allow wrapping around to top/bottom when scrolling
	BOOL        m_enable_dropdown_window;       // controls visibility of dropdown
	int         m_max_lines;                    // Max lines to show in the drop down
	EditListener* m_edit_listener;
	Type		m_widget_type;
	bool		m_candidate_window_shown;		// Is the ime candidate window shown
};


/***********************************************************************************
**
**	TreeViewWindow - implementing the dropdown window
**
***********************************************************************************/

class OpTreeViewWindow
{
	friend class OpTreeViewDropdown;

public:
	virtual ~OpTreeViewWindow() {}

	/**
	 * Construct a TreeViewWindow
	 *
	 * @param window to be created
	 * @param dropdown that owns the window
	 * @param transp_window_skin Name of transparent skin to use for window (optional). If specified, the window will be transparent.
	 *
	 * @return OpStatus::OK if successful
	 */
	static OP_STATUS Create(OpTreeViewWindow ** window, OpTreeViewDropdown* dropdown, const char *transp_window_skin = NULL);

	/**
	 * The treemodel is emptied, if m_items_deletable is TRUE then all the items are 
	 * deleted.
	 *
	 * @return OpStatus::OK if successful
	 */
	virtual OP_STATUS Clear(BOOL delete_model = FALSE) = 0;

	/**
	 * Sets the treemodel for the window and sets it in the treeview aswell
	 *
	 * @param tree_model      - the treemodel that is to be used
	 * @param items_deletable - if TRUE the items should be deleted by the window, if not they will only be removed.
	 */
	virtual void SetModel(GenericTreeModel * tree_model, BOOL items_deletable) = 0;

	/**
	 * @return pointer to the current treemodel
	 */
	virtual GenericTreeModel * GetModel() = 0;

	/**
	 * @return the selected item
	 */
	virtual OpTreeModelItem * GetSelectedItem(int * position = NULL) = 0;

	/**
	* Sets the selected item in the treeview
	*
	* @param item  - item to select
	*
	*/
	virtual void SetSelectedItem(OpTreeModelItem *item, BOOL selected) = 0;

	/**
	 * @return the rectangle for the dropdown
	 */
	virtual OpRect CalculateRect() = 0;

	/**
	 * @return pointer to the current treeview
	 */
	virtual OpTreeView * GetTreeView() = 0;

	virtual OP_STATUS AddDesktopWindowListener(DesktopWindowListener* listener) = 0;
	virtual void ClosePopup() = 0;
	virtual BOOL IsPopupVisible() = 0;
	virtual void SetPopupOuterPos(INT32 x, INT32 y) = 0;
	virtual void SetPopupOuterSize(UINT32 width, UINT32 height) = 0;
	virtual void SetVisible(BOOL vis, BOOL default_size = FALSE) = 0;
	virtual void SetVisible(BOOL vis, OpRect rect) = 0;
	virtual BOOL SendInputAction(OpInputAction* action) = 0;
	virtual void SetExtraLineHeight(UINT32 extra_height) = 0;
	virtual void SetAllowWrappingOnScrolling(BOOL wrapping) = 0;
	virtual BOOL IsLastLineSelected() = 0;
	virtual BOOL IsFirstLineSelected() = 0;
	virtual void UnSelectAndScroll() = 0;
	virtual void SetTreeViewName(const char* name) = 0;
};


#endif // OP_TREE_VIEW_DROPDOWN_H
