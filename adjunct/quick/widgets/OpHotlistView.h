/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 * @author Co-owner: Espen Sand (espen)
 *
 */

#ifndef OP_HOTLIST_VIEW_H
#define OP_HOTLIST_VIEW_H

#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/widgets/OpHotlistTreeView.h"

#include "adjunct/quick_toolkit/widgets/QuickOpWidgetWrapper.h"

/***********************************************************************************
**
**	OpHotlistView
**
***********************************************************************************/

class OpHotlistView : public OpHotlistTreeView
{
	protected:
								OpHotlistView(Type widget_type);

	public:
		static OP_STATUS		Construct(OpHotlistView** obj,Type widget_type,
										PrefsCollectionUI::integerpref splitter_prefs_setting=PrefsCollectionUI::DummyLastIntegerPref,
										PrefsCollectionUI::integerpref viewstyle_prefs_setting=PrefsCollectionUI::DummyLastIntegerPref,
										PrefsCollectionUI::integerpref detailed_splitter_prefs_setting=PrefsCollectionUI::DummyLastIntegerPref,
										PrefsCollectionUI::integerpref detailed_viewstyle_prefs_setting=PrefsCollectionUI::DummyLastIntegerPref);

		// subclassing OpHotlistTreeView
		virtual	void			OnSetDetailed(BOOL detailed);
		virtual BOOL			IsSingleClick() {return !m_detailed && g_pcui->GetIntegerPref(PrefsCollectionUI::HotlistSingleClick);}

		virtual INT32			OnDropItem(HotlistModelItem* hmi_target, DesktopDragObject* drag_object, INT32 i, DropType drop_type, DesktopDragObject::InsertType insert_type, INT32 *first_id, BOOL force_delete);
		virtual BOOL			AllowDropItem(HotlistModelItem* hmi_target, HotlistModelItem* hmi_src, DesktopDragObject::InsertType& insert_type, DropType& drop_type);

		virtual BOOL			OnExternalDragDrop(OpTreeView* tree_view, OpTreeModelItem* item, DesktopDragObject* drag_object, DropType drop_type, DesktopDragObject::InsertType insert_type, BOOL drop, INT32& new_selection_id);
		virtual BOOL 			ShowContextMenu(const OpPoint &point, BOOL center, OpTreeView* view, BOOL use_keyboard_context);
		virtual void			HandleMouseEvent(OpTreeView* widget, HotlistModelItem* item, INT32 pos, MouseButton button, BOOL down, UINT8 nclicks);
		virtual	INT32			GetRootID();

		HotlistModelItem*		GetSelectedItem() {return (HotlistModelItem*) m_hotlist_view->GetSelectedItem();}

		BOOL					IsBookmarks() { return m_widget_type == WIDGET_TYPE_BOOKMARKS_VIEW; }
		BOOL					IsContacts()  { return m_widget_type == WIDGET_TYPE_CONTACTS_VIEW;  }
#ifdef WEBSERVER_SUPPORT
		BOOL					IsUniteServices()   { return m_widget_type == WIDGET_TYPE_UNITE_SERVICES_VIEW;   }
#endif // WEBSERVER_SUPPORT
		BOOL					IsNotes()     { return m_widget_type == WIDGET_TYPE_NOTES_VIEW;     }

		BOOL					ViewSelectedContactMail(BOOL force, BOOL include_folder_content = FALSE, BOOL ignore_modifier_keys = FALSE);
		BOOL					ComposeMailToSelectedContact();


	#ifdef JABBER_SUPPORT
		BOOL                    ComposeMessageToSelectedContact();
		BOOL                    SubscribeToPresenceOfSelectedContact(BOOL subscribe = TRUE);
		BOOL                    AllowPresenceSubscription(BOOL allow = TRUE);
	#endif // JABBER_SUPPORT

		// OpWidgetListener
		virtual void			OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);

		// Implementing the OpTreeModelItem interface

		virtual Type			GetType() {return m_widget_type;}

		// == OpInputContext ======================

		virtual BOOL			OnInputAction(OpInputAction* action);

#ifdef WEBSERVER_SUPPORT
		virtual const char*		GetInputContextName() {return IsBookmarks() ? "Bookmarks Widget" : IsContacts() ? "Contacts Widget" : IsNotes() ? "Notes Widget" : "Unite Services Widget";}
#else
		virtual const char*		GetInputContextName() {return IsBookmarks() ? "Bookmarks Widget" : IsContacts() ? "Contacts Widget" : "Notes Widget";}
#endif // WEBSERVER_SUPPORT

	// void                    GetSortedHotlistItemList( BOOL whole_model, OpVector<HotlistModelItem>& in_items, 	OpVector<HotlistModelItem>& items );


	private:

#ifdef WEBSERVER_SUPPORT
		BOOL					SelectionContainsRootService(OpTreeView * tree_view);
#endif // WEBSERVER_SUPPORT

		void					RemoveTrashItemsFromSelected(OpINT32Vector& id_list);
		// Determines the gadget state pased on the selected items
		GadgetModelItem::GMI_State GetSelectedGadgetsState(OpINT32Vector& id_list, OpTreeView *tree_view);

		Type					m_widget_type;




};

/* QuickWidget wrapper for OpHotlistView */
class QuickHotlistView : public QuickOpWidgetWrapperNoInit<OpHotlistView>
{
public: 
	OP_STATUS Init(OpHotlistView::Type widget_type)
	{
		OpHotlistView* widget;
		RETURN_IF_ERROR(OpHotlistView::Construct(&widget, widget_type));
		SetOpWidget(widget);

		return OpStatus::OK;
	}
};

#endif // OP_HOTLIST_VIEW_H
