/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 *
 */

#ifndef OP_BOOKMARKS_VIEW_H
#define OP_BOOKMARKS_VIEW_H

#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "adjunct/quick/models/BookmarkModel.h"
#include "adjunct/quick/widgets/OpHotlistTreeView.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"

class OpBookmarkView
	: public OpHotlistTreeView
	, public DesktopBookmarkListener
{
 protected:
	OpBookmarkView();
  
 public:	
	static OP_STATUS		Construct(OpBookmarkView** obj,
									PrefsCollectionUI::integerpref splitter_prefs_setting=PrefsCollectionUI::DummyLastIntegerPref,
									PrefsCollectionUI::integerpref viewstyle_prefs_setting=PrefsCollectionUI::DummyLastIntegerPref,
									PrefsCollectionUI::integerpref detailed_splitter_prefs_setting=PrefsCollectionUI::DummyLastIntegerPref,
									PrefsCollectionUI::integerpref detailed_viewstyle_prefs_setting=PrefsCollectionUI::DummyLastIntegerPref);

	
	BOOL OnInputAction(OpInputAction* action);

	void SetModel(BookmarkModel* model);

	// subclassing OpHostlistTreeView
	virtual	INT32			GetRootID()	{return HotlistModel::BookmarkRoot;}
	virtual	void			OnSetDetailed(BOOL detailed);
	virtual BOOL			OnExternalDragDrop(OpTreeView* tree_view,OpTreeModelItem* item,DesktopDragObject* drag_object,DropType drop_type, DesktopDragObject::InsertType insert_type,BOOL drop,INT32& new_selection_id);
	virtual void			HandleMouseEvent(OpTreeView* widget, HotlistModelItem* item, INT32 pos, MouseButton button, BOOL down, UINT8 nclicks);
	virtual INT32			OnDropItem(HotlistModelItem* hmi_target, DesktopDragObject* drag_object, INT32 i, DropType drop_type, DesktopDragObject::InsertType insert_type, INT32 *first_id, BOOL force_delete);
	
	virtual BOOL			ShowContextMenu(const OpPoint &point, BOOL center, OpTreeView* view, BOOL use_keyboard_context);
			
	// OpWidgetListener	
	virtual void 			OnSortingChanged(OpWidget *widget);
	virtual void			OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
	
	virtual Type			GetType() {return WIDGET_TYPE_BOOKMARKS_VIEW;}
	
	// == OpInputContext ======================
	
	virtual const char* GetInputContextName() { return "Bookmarks Widget"; }

	HotlistModelItem* GetItemByPosition(INT32 pos)   { return static_cast<HotlistModelItem*>(m_hotlist_view->GetItemByPosition(pos)); }
	HotlistModelItem* GetSelectedItem()              { return static_cast<HotlistModelItem*>(m_hotlist_view->GetSelectedItem()); }
	HotlistModelItem* GetParentByPosition(INT32 pos) { return static_cast<HotlistModelItem*>(m_hotlist_view->GetParentByPosition(pos)); }

	void DropHistoryItem(HotlistModelItem* hmi_target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type);
	void DropWindowItem(HotlistModelItem* target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type);
	void DropURLItem(HotlistModelItem* hmi_target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type);

	void UpdateHistoryDragObject(HotlistModelItem* target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type);
	void UpdateDragObject(HotlistModelItem* target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type);


	virtual void OnDeleted();

	// DesktopBookmarkListener
	virtual void OnBookmarkModelLoaded();

};

#endif // OP_BOOKMARK_VIEW_H
