/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#pragma once
#include "adjunct/quick_toolkit/widgets/OpSplitter.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

class HotlistModel;
class HotlistModelItem;
class OpHotlistView;
class OpBookmarkView;

class OpHotlistTreeView : public OpSplitter, public OpTreeViewListener
{
public:
	
	enum ViewStyle
	{
		STYLE_TREE,			// Whole tree in one view
		STYLE_SPLIT,		// Folders on top/left, Nonfolders below/right
		STYLE_FLAT,			// Contents of one folder shown flat. (needs a parent button on toolbar)
		STYLE_FOLDERS,		// Only view folders
	};

	OpHotlistTreeView(void);
	virtual ~OpHotlistTreeView(void);

	OP_STATUS Init();

	void SetPrefsmanFlags(PrefsCollectionUI::integerpref splitter_prefs_setting=PrefsCollectionUI::DummyLastIntegerPref, 
						PrefsCollectionUI::integerpref viewstyle_prefs_setting=PrefsCollectionUI::DummyLastIntegerPref, 
						PrefsCollectionUI::integerpref detailed_splitter_prefs_setting=PrefsCollectionUI::DummyLastIntegerPref, 
						PrefsCollectionUI::integerpref detailed_viewstyle_prefs_setting=PrefsCollectionUI::DummyLastIntegerPref);

	BOOL OnInputAction(OpInputAction* action);

	// Multiple view styles support
	void					SetViewStyle(ViewStyle view_style, BOOL force = FALSE);
	ViewStyle				GetViewStyle() {return m_view_style;}
	void					SetDetailed(BOOL detailed, BOOL force = FALSE);

	OpTreeView*				GetFolderView() {return m_folders_view;}
	OpTreeView*				GetItemView() {return m_hotlist_view;}
	OpTreeView*				GetRecentFocusedView() {return m_last_focused_view;}

	PrefsCollectionUI::integerpref GetSplitterPrefsSetting() {return m_detailed ? m_detailed_splitter_prefs_setting : m_splitter_prefs_setting;}
	PrefsCollectionUI::integerpref GetViewStylePrefsSetting() {return m_detailed ? m_detailed_viewstyle_prefs_setting : m_viewstyle_prefs_setting;}

	void					SaveSettings();
	void					LoadSettings();
	
	void					InitModel();

	// folders
	void					SetCurrentFolderByID(INT32 id);
	void					UpdateCurrentFolder();
	INT32					GetCurrentFolderID();
	INT32 					GetSelectedFolderID();
	BOOL					OpenSelectedFolder();

    inline void				SetSelectedItemByID(INT32 id,BOOL changed_by_mouse = FALSE,BOOL send_onchange = TRUE,BOOL allow_parent_fallback = FALSE);

	// == OpInputContext ======================
	virtual void			OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);


	// Drag & Drop 
	virtual void			OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
	virtual void			OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
	BOOL					AddItemAllowed(INT32 id, HotlistModel* model, DesktopDragObject* drag_object, INT32 num_items = 0);
    void                    ShowMaxItemsReachedDialog(HotlistModelItem* hmi_target, DesktopDragObject::InsertType insert_type);


	// OpTreeViewListener
	virtual void			OnTreeViewDeleted(OpTreeView* treeview) {}
	virtual void			OnTreeViewModelChanged(OpTreeView* treeview) {}
	virtual void			OnTreeViewMatchChanged(OpTreeView* treeview) {UpdateCurrentFolder();}

	// subclassing OpWidget
	virtual void			SetAction(OpInputAction* action) {m_hotlist_view->SetAction(action); }
	virtual void			OnDeleted();

	// OpWidgetListener
	virtual void			OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text); // treeview change
	virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);
	virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	virtual BOOL			OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked);

	/////////////////////////////////////////////////////
	// hooks for subclasses
	virtual	INT32			GetRootID() = 0;
	virtual	void			OnSetDetailed(BOOL detailed) = 0;
	virtual BOOL			IsSingleClick() {return !m_detailed && g_pcui->GetIntegerPref(PrefsCollectionUI::HotlistSingleClick);}
	virtual BOOL 			ShowContextMenu(const OpPoint &point, BOOL center, OpTreeView* view, BOOL use_keyboard_context){return TRUE;}
	
	// called when a item within the model is droped
	// @ret -1:abort operation 0:continue 1:skip current one
	virtual INT32			OnDropItem(HotlistModelItem* hmi_target,DesktopDragObject* drag_object, INT32 i, DropType drop_type, DesktopDragObject::InsertType insert_type, INT32 *first_id=0, BOOL force_delete = FALSE ) = 0;
	virtual BOOL			AllowDropItem(HotlistModelItem* hmi_target, HotlistModelItem* hmi_src, DesktopDragObject::InsertType& insert_type,DropType& drop_type);
	
	// called when a external object dragged to treeviews
	// @ret return FALSE in case of failure(e.g. maximum count reached)
	virtual BOOL			OnExternalDragDrop(OpTreeView* tree_view,OpTreeModelItem* item,DesktopDragObject* drag_object,DropType drop_type, DesktopDragObject::InsertType insert_type,BOOL drop,INT32& new_selection_id){return TRUE;}

	virtual void			HandleMouseEvent(OpTreeView* widget, HotlistModelItem* item, INT32 pos, MouseButton button, BOOL down, UINT8 nclicks){}

	/////////////////////////////////////////////////////

    BOOL                    OpenUrls( const OpINT32Vector& index_list, 
									  BOOL3 new_window, 
									  BOOL3 new_page, 
									  BOOL3 in_background, 
									  DesktopWindow* target_window = NULL, 
									  INT32 position = -1, 
									  BOOL ignore_modifier_keys = FALSE);

protected:
	OpTreeView*				GetActiveTreeView() { return m_hotlist_view->GetFocused() == m_hotlist_view ?	m_hotlist_view :	m_folders_view; }
	OpTreeView*             GetTreeViewFromInputContext(OpInputContext* input_context);
    void                    GetSortedItemList( const OpINT32Vector& list, OpINT32Vector& ordered_list);
	void					DragDropMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y, BOOL drop);

private:

protected:
	// View styles
	ViewStyle				m_view_style;
	OpTreeView*				m_hotlist_view;
	OpTreeView*				m_folders_view;
	BOOL					m_detailed;
	 
	// view which is focused recently
	OpTreeView*				m_last_focused_view;


	PrefsCollectionUI::integerpref	m_splitter_prefs_setting;
	PrefsCollectionUI::integerpref	m_viewstyle_prefs_setting;
	PrefsCollectionUI::integerpref	m_detailed_splitter_prefs_setting;
	PrefsCollectionUI::integerpref	m_detailed_viewstyle_prefs_setting;

	// Sort order (they are separate per hotlist view)
	INT32                   m_sort_column;
	BOOL                    m_sort_ascending;

	HotlistModel*	m_model;
};

inline void OpHotlistTreeView::SetSelectedItemByID(INT32 id,BOOL changed_by_mouse,BOOL send_onchange,BOOL allow_parent_fallback)
{
	m_hotlist_view->SetSelectedItemByID(id, changed_by_mouse, send_onchange, allow_parent_fallback);
}
