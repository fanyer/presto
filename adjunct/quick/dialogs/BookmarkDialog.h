/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 *
 */

#ifndef BOOKMARK_DIALOG_H
#define BOOKMARK_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"

class BookmarkModel;
class OpBookmarkView;
class HotlistModelItem;
class BookmarkItemData;

class BookmarkDialog : public Dialog,
					   public OpTreeModel::Listener,
					   public DialogListener
{
public:
	BookmarkDialog();
	~BookmarkDialog();
	OP_STATUS Init(DesktopWindow* parent_window, HotlistModelItem* parent);
	virtual BOOL 	SaveSettings() = 0;

	virtual BOOL	OnInputAction(OpInputAction* action);
	virtual void	OnClose(BOOL user_initiated);


	// Empty stubs
	virtual void	OnItemChanged(OpTreeModel* tree_model, INT32 item, BOOL sort) {}
	virtual void	OnItemRemoving(OpTreeModel* tree_model, INT32 item) {}
	virtual void	OnSubtreeRemoving(OpTreeModel* tree_model, INT32 parent, INT32 subtree_root, INT32 subtree_size) {}
	virtual void	OnSubtreeRemoved(OpTreeModel* tree_model, INT32 parent, INT32 subtree_root, INT32 subtree_size) {}
	virtual void	OnTreeChanging(OpTreeModel* tree_model ) {}
	virtual void	OnTreeDeleted(OpTreeModel* tree_model){}
	// The ones actually implemented
	virtual void	OnItemAdded(OpTreeModel* tree_model, INT32 item);
	virtual void	OnTreeChanged(OpTreeModel* tree_model) { DesktopWindow::Close(FALSE, TRUE); }

protected:
	void UpdateFolderDropDown();
	BOOL ValidateNickName(OpString& nick, HotlistModelItem* hmi);

	static INT32	s_last_saved_folder;

	INT32	m_parent_id;
	BookmarkModel*  m_model;
private:
	OpBookmarkView* m_bookmark_view;
};

/***********************************************************************************
 **
 ** EditBookmarkDialog
 **
 **
 ***********************************************************************************/
// TODO: Add model param
class EditBookmarkDialog : public BookmarkDialog
{ 
public:
	EditBookmarkDialog(BOOL attempted_add);
	virtual ~EditBookmarkDialog();
	
	OP_STATUS Init(DesktopWindow* parent_window, 
				   HotlistModelItem* item);
	
	DialogType    GetDialogType() { return TYPE_PROPERTIES; }
	Type		GetType()	{ return DIALOG_TYPE_BOOKMARK_PROPERTIES; }
	const char*	GetWindowName();

	BOOL		GetModality() 	{ return FALSE; }
	const char*	GetHelpAnchor()	{ return "bookmarks.html"; }
	
	virtual void	OnInit();
	virtual void	OnChange(OpWidget* widget, BOOL changed_by_mouse);
	virtual BOOL 	SaveSettings();
	virtual void	OnClose(BOOL user_initiated) { BookmarkDialog::OnClose(user_initiated); }
	
	HotlistModelItem* GetBookmark();
	
	// DialogListener
	virtual void OnOk(Dialog* dialog, UINT32 result);
	
private:	
	
	INT32 m_bookmark_id;
	BOOL	m_is_folder;
	BOOL m_add;
};


/**********************************************************************
 *
 * AddBookmarkDialog
 *
 *
 **********************************************************************/

class AddBookmarkDialog : public BookmarkDialog
{
public:
	
	AddBookmarkDialog() : m_item_data(0) {}
	virtual ~AddBookmarkDialog();
	
	OP_STATUS Init(DesktopWindow* parent_window, 
				   BookmarkItemData* item, 
				   HotlistModelItem* parent = NULL);
	
	DialogType    GetDialogType() { return TYPE_OK_CANCEL; }
	Type		GetType()	{ return DIALOG_TYPE_BOOKMARK_PROPERTIES; }
	const char*	GetWindowName() { return "Add Bookmark Dialog"; }
	BOOL		GetModality() 	{ return FALSE; }
	const char*	GetHelpAnchor()	{ return "bookmarks.html"; }
	
	virtual void	OnInit();
	virtual void	OnChange(OpWidget* widget, BOOL changed_by_mouse);
	virtual BOOL 	SaveSettings();
	virtual void	OnClose(BOOL user_initiated) { BookmarkDialog::OnClose(user_initiated); }
	
	// DialogListener
	virtual void OnOk(Dialog* dialog, UINT32 result);

private:	
	void UpdateIsDuplicate();
	
	BookmarkItemData* m_item_data;
};

#endif // _BOOKMARK_DIALOG_
