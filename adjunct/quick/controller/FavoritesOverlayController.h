/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef FAVORITES_OVERLAY_CONTROLLER_H
#define FAVORITES_OVERLAY_CONTROLLER_H

#include "adjunct/quick_toolkit/contexts/PopupDialogContext.h"
#include "adjunct/quick_toolkit/windows/DesktopWindowListener.h"
#include "adjunct/quick/hotlist/HotlistModelItem.h"

class OpWidget;
class OpTreeViewDropdown;
class BookmarkModel;
class DocumentDesktopWindow;
class BrowserDesktopWindow;

class FavoritesOverlayController
		: public PopupDialogContext
		, private OpWidgetListener
		, private OpTimerListener
		, private DesktopWindowListener
{
	typedef PopupDialogContext Base;

public:
	explicit FavoritesOverlayController(OpWidget* anchor_widget);
	virtual ~FavoritesOverlayController();

protected:
	// UiContext
	virtual BOOL			CanHandleAction(OpInputAction* action);
	virtual BOOL			DisablesAction(OpInputAction* action);
	virtual OP_STATUS		HandleAction(OpInputAction* action);
	virtual void			OnUiClosing();

	// OpWidgetListener
	virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);

	// DesktopWindowListener
	virtual void			OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL active);
	virtual void			OnDesktopWindowShown(DesktopWindow* desktop_window, BOOL shown);
	virtual void			OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);

	// OpTimerListener
	virtual void			OnTimeOut(OpTimer* timer);

	//OpInputContext
	virtual void			OnKeyboardInputGained(OpInputContext* new_context, OpInputContext* old_context, FOCUS_REASON reason);

private:
	virtual void			InitL();

	void					ShowBookmarkFolderList();
	OP_STATUS				CreateBookmarkFolderListModel(BookmarkModel **bm_folderlist_model);
	void					ShowActionResultAndTriggerCloseDlgTimer(bool is_action_type_delete);
	int						GetDocPosInSDList(bool include_display_url = true);
	HotlistModelItem*		GetBookmarkItemOfDoc();
	void					UpdateDropDownBtnSkin(bool is_dd_open);
	void					UpdateDropDownCompositeAction(bool is_reset);
	bool					IsThereAnyFolderInBookmarkList();

private:
	OpTimer					*m_auto_hide_timer;
	BrowserDesktopWindow	*m_browser_window;
	DocumentDesktopWindow	*m_document_window;
	OpTreeViewDropdown		*m_dropdown;
	INT32					m_bm_folder_id;
	bool					m_dropdown_needs_closing;
};


class FavoriteFolder
	: public FolderModelItem
{
public:
	FavoriteFolder();

	OP_STATUS				SetRootFolder();
	OP_STATUS				SetFolderTitle(const OpStringC& folder_title, INT32 bookmark_id);
	OpStringC				GetFolderTitle() const {return m_folder_title; }
	INT32					GetFolderID() const { return m_bm_folder_id; }

	// FolderModelItem
	virtual OP_STATUS		GetItemData(ItemData* item_data);

private:
	bool m_show_separator;
	OpString m_folder_title;
	OpString8 m_item_image;
	INT32 m_bm_folder_id;
};

class FavoriteFolderSortListener
	: public OpTreeModel::SortListener
{
	//OpTreeModel::SortListener
	virtual INT32			OnCompareItems(OpTreeModel* tree_model, OpTreeModelItem* item0, OpTreeModelItem* item1);
};

#endif // FAVORITES_OVERLAY_CONTROLLER_H
