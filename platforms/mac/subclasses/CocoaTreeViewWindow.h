/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef COCOA_TREEVIEW_WINDOW_H
#define COCOA_TREEVIEW_WINDOW_H

#ifdef QUICK_TOOLKIT_PLATFORM_TREEVIEWDROPDOWN

#include "adjunct/quick_toolkit/widgets/OpTreeViewDropdown.h"
#include "modules/hardcore/timer/optimer.h"
#define BOOL NSBOOL
#import <AppKit/AppKit.h>
#import <Foundation/NSDate.h>
#undef BOOL 

class CocoaTreeViewWindow;
/***********************************************************************************
**
**	TreeViewWindow - implementing the dropdown window
**
***********************************************************************************/
@interface OperaNSPopUpButton : NSPopUpButton
{
}
@end

class CocoaTreeViewWindow : public OpTreeViewWindow, public OpTimerListener
{
private:
	GenericTreeModel *m_tree_model;
	OperaNSPopUpButton *m_button;
	int m_selected;
	id m_delegate;
	DesktopWindowListener* m_listener;
	OpTreeViewDropdown* m_dropdown;
	BOOL m_items_deletable;
	OpTimer* m_timer;
	BOOL m_allow_wrapping_on_scrolling;

public:
	CocoaTreeViewWindow(OpTreeViewDropdown* dropdown);
	virtual ~CocoaTreeViewWindow();

	static OP_STATUS Create(OpTreeViewWindow ** window, OpTreeViewDropdown* dropdown, const char *transp_window_skin = NULL);

	virtual OP_STATUS Clear(BOOL delete_model = FALSE);

	virtual void SetModel(GenericTreeModel *tree_model, BOOL items_deletable);

	virtual GenericTreeModel *GetModel();

	virtual OpTreeModelItem *GetSelectedItem(int * position = NULL);

	virtual void SetSelectedItem(OpTreeModelItem *item, BOOL selected);

	virtual OpRect CalculateRect();

	virtual OpTreeView * GetTreeView();

	virtual OP_STATUS AddDesktopWindowListener(DesktopWindowListener* listener);
	virtual void ClosePopup();
	virtual BOOL IsPopupVisible();
	virtual void SetPopupOuterPos(INT32 x, INT32 y);
	virtual void SetPopupOuterSize(UINT32 width, UINT32 height);
	virtual void SetVisible(BOOL vis, BOOL default_size = FALSE);
	virtual void SetVisible(BOOL vis, OpRect rect);
	virtual BOOL SendInputAction(OpInputAction* action);
	virtual void SetExtraLineHeight(UINT32 extra_height);
	virtual void SetAllowWrappingOnScrolling(BOOL wrapping);
	virtual void SetMaxLines(BOOL max_lines);
	virtual BOOL IsLastLineSelected();
	virtual BOOL IsFirstLineSelected();
	virtual void UnSelectAndScroll();
	virtual void SetTreeViewName(const char* name);

	virtual void OnTimeOut(OpTimer* timer);

	void SetActiveItem(int item);
	void PopupClosed();
};
#endif // QUICK_TOOLKIT_PLATFORM_TREEVIEWDROPDOWN

#endif // COCOA_TREEVIEW_WINDOW_H
