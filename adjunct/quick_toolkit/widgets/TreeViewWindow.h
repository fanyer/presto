/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpTreeViewDropdown.h"

class TreeViewWindow : 
	public DesktopWindow,
	public OpTreeViewWindow
{
	friend class OpTreeViewDropdown;

public :
	TreeViewWindow();
	virtual ~TreeViewWindow();

	/**
	 * Construct a TreeViewWindow
	 *
	 * @param window to be created
	 * @param dropdown that owns the window
	 * @param transp_window_skin Name of transparent skin to use for window (optional). If specified, the window will be transparent.
	 *
	 * @return OpStatus::OK if successful
	 */
	static OP_STATUS Create(TreeViewWindow ** window, OpTreeViewDropdown* dropdown, const char *transp_window_skin = NULL);

	/**
	 * The treemodel is emptied, if m_items_deletable is TRUE then all the items are 
	 * deleted.
	 *
	 * @return OpStatus::OK if successful
	 */
	OP_STATUS Clear(BOOL delete_model = FALSE);

	/**
	 * Sets the treemodel for the window and sets it in the treeview aswell
	 *
	 * @param tree_model      - the treemodel that is to be used
	 * @param items_deletable - if TRUE the items should be deleted by the window, if not they will only be removed.
	 */
	void SetModel(GenericTreeModel * tree_model, BOOL items_deletable);

	/**
	 * @return pointer to the current treemodel
	 */
	GenericTreeModel * GetModel() { return m_tree_model; }

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
	 * @return the rectangle for the dropdown
	 */
	OpRect CalculateRect();

	/**
	 * @return pointer to the current treeview
	 */
	OpTreeView * GetTreeView() { return m_tree_view; }

	// == OpWindow ======================

	virtual BOOL OnNeedPostClose() { return TRUE; }
	
	// == DesktopWindow ======================

	virtual void            OnLayout();

//	virtual Type            GetType() { return WINDOW_TYPE_TREEVIEW; }

	virtual const char*		GetWindowName() { return "Treeview Window"; }

	virtual void			OnShow(BOOL show);
	virtual void			OnClose(BOOL user_initiated);

	// == OpInputContext ======================

	virtual BOOL OnInputAction(OpInputAction* action);

	// == OpTreeViewWindow ======================
	virtual void SetExtraLineHeight(UINT32 extra_height) 
	{ 
		if(m_tree_view) 
			m_tree_view->SetExtraLineHeight(extra_height); 
	}
	virtual void SetAllowWrappingOnScrolling(BOOL wrapping)
	{
		if(m_tree_view) 
			m_tree_view->SetAllowWrappingOnScrolling(wrapping); 
	}
	virtual OP_STATUS AddDesktopWindowListener(DesktopWindowListener* listener);
	virtual void ClosePopup();
	virtual BOOL IsPopupVisible();
	virtual void SetPopupOuterPos(INT32 x, INT32 y);
	virtual void SetPopupOuterSize(UINT32 width, UINT32 height);
	virtual void SetVisible(BOOL vis, BOOL default_size);
	virtual void SetVisible(BOOL vis, OpRect rect);
	virtual BOOL SendInputAction(OpInputAction* action);
	virtual void SetTreeViewName(const char* name);

	OP_STATUS Construct(OpTreeViewDropdown* dropdown, const char *transp_window_skin);
private :


	void UpdateMenu();

	int GetSelectedLine();

	BOOL IsLastLineSelected();

	BOOL IsFirstLineSelected();

	void UnSelectAndScroll();

	// The dropdown that owns me
	OpTreeViewDropdown * m_dropdown;

	// The treeview that is being displayed
	OpTreeView         * m_tree_view;

	// The treemodel in the treeview
	GenericTreeModel    * m_tree_model;

	// Whether the items in the model should be
	// deleted when the model is deleted
	BOOL m_items_deletable;

	// The parent window that is blocked from closing while the TreeViewWindow
	// is shown.
	DesktopWindow* m_blocked_parent;
};
