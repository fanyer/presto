/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 * @author Co-owner: Espen Sand (espen)
 *
 */

#include "core/pch.h"
#include "OpHotlistView.h"

#include "adjunct/desktop_util/mail/mailcompose.h"
#include "adjunct/desktop_util/mail/mailto.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#ifdef M2_SUPPORT
#include "adjunct/m2_ui/windows/ComposeDesktopWindow.h"
#include "adjunct/m2/src/engine/engine.h"
#endif // M2_SUPPORT
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"

#include "modules/inputmanager/inputaction.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/display/vis_dev.h"

#include "modules/pi/OpDragManager.h"
#include "modules/dragdrop/dragdrop_manager.h"
#include "modules/dragdrop/dragdrop_data_utils.h"

#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "modules/widgets/OpMultiEdit.h"

#ifdef WEBSERVER_SUPPORT
# include "adjunct/quick/managers/WebServerManager.h"
# include "modules/skin/OpSkinManager.h"
# include "adjunct/quick/hotlist/Hotlist.h"
# include "adjunct/quick/panels/UniteServicesPanel.h"
#endif

#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/windows/PanelDesktopWindow.h"

/***********************************************************************************
 *
 *	Construct
 *
 ***********************************************************************************/
// static
OP_STATUS OpHotlistView::Construct(OpHotlistView** obj,
								   Type widget_type,
								   PrefsCollectionUI::integerpref splitter_prefs_setting,
								   PrefsCollectionUI::integerpref viewstyle_prefs_setting,
								   PrefsCollectionUI::integerpref detailed_splitter_prefs_setting,
								   PrefsCollectionUI::integerpref detailed_viewstyle_prefs_setting)
{
	if(widget_type == WIDGET_TYPE_CONTACTS_VIEW || widget_type == WIDGET_TYPE_NOTES_VIEW
#ifdef WEBSERVER_SUPPORT
		 || widget_type == WIDGET_TYPE_UNITE_SERVICES_VIEW
#endif
		 )
	{
		*obj = OP_NEW(OpHotlistView,(widget_type));

		if (!*obj)
			return OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsError((*obj)->init_status))
		{
			OP_DELETE(*obj);
			return OpStatus::ERR_NO_MEMORY;
		}

		(*obj)->SetPrefsmanFlags(splitter_prefs_setting,viewstyle_prefs_setting,detailed_splitter_prefs_setting,detailed_viewstyle_prefs_setting);
		(*obj)->Init();

		return OpStatus::OK;
	}
	else
	{
		OP_ASSERT(FALSE);
		*obj = NULL;
		return OpStatus::ERR;
	}
}

/***********************************************************************************
 *
 *	OpHotlistView
 *
 ***********************************************************************************/

OpHotlistView::OpHotlistView(Type widget_type)
{
	m_widget_type = widget_type;

#ifdef WEBSERVER_SUPPORT
	if (IsUniteServices())
	{
		m_folders_view->SetRestrictImageSize(FALSE);
		m_folders_view->SetShowThreadImage(FALSE);

		m_hotlist_view->SetRestrictImageSize(FALSE);
		m_hotlist_view->SetShowThreadImage(FALSE);
	}
#endif // WEBSERVER_SUPPORT

	BOOL sort_ascending = TRUE;

	if (IsContacts())
	{
		m_model = g_hotlist_manager->GetContactsModel();
		m_sort_column = 0;
	}
	else if (IsNotes())
	{
		m_model = g_hotlist_manager->GetNotesModel();
		m_sort_column = -1;
	}
#ifdef WEBSERVER_SUPPORT
	else if (IsUniteServices())
	{
		m_model = g_hotlist_manager->GetUniteServicesModel();
		m_sort_column = -1;
	}
#endif // WEBSERVER_SUPPORT

	m_folders_view->SetTreeModel(m_model, m_sort_column, sort_ascending);
	m_hotlist_view->SetTreeModel(m_model, m_sort_column, sort_ascending);

#ifdef WEBSERVER_SUPPORT
	if (IsUniteServices())
	{
		m_hotlist_view->SetAllowMultiLineIcons(TRUE);
		m_hotlist_view->SetHaveWeakAssociatedText(TRUE);
		m_hotlist_view->SetColumnImageFixedDrawArea(0, 32);
		m_hotlist_view->SetName("Unite Services View");
	}
#endif // WEBSERVER_SUPPORT
}



#ifdef WEBSERVER_SUPPORT
/***********************************************************************************
 *
 *	SelectionContainsRootService
 *
 ***********************************************************************************/
BOOL
OpHotlistView::SelectionContainsRootService(OpTreeView * tree_view)
{
	OpINT32Vector id_list;
	if (IsUniteServices())
	{
		tree_view->GetSelectedItems( id_list );
		
		HotlistModel* model = g_hotlist_manager->GetUniteServicesModel();
		
		if (model)
		{
			for (UINT32 i = 0; i < id_list.GetCount(); i++)
			{
				HotlistModelItem* item = model->GetItemByID(id_list.Get(i));
				if (!item)
					break;

				if (item->IsUniteService())
				{
					if (item->IsRootService())
					{
						return TRUE;
					}
				}
			}
			
		}
	}
	return FALSE;
}

#endif // WEBSERVER_SUPPORT



/***********************************************************************************
 *
 *	GetRootID
 *
 *  @return ID of model root
 *
 ***********************************************************************************/

INT32 OpHotlistView::GetRootID()
{
	if (IsContacts())
		return HotlistModel::ContactRoot;
	else if (IsNotes())
		return HotlistModel::NoteRoot;
#ifdef WEBSERVER_SUPPORT
	else if (IsUniteServices())
		return HotlistModel::UniteServicesRoot;
#endif // WEBSERVER_SUPPORT
	OP_ASSERT(!"Unkown view type");
	return 0;
}


/***********************************************************************************
 *
 *	OnInputAction
 *
 ***********************************************************************************/

BOOL OpHotlistView::OnInputAction(OpInputAction* action)
{
	BOOL trash_selected, special_folder;

	OpTreeView* tree_view = GetTreeViewFromInputContext(action->GetFirstInputContext());

	HotlistModelItem* item;

	item = (HotlistModelItem*) tree_view->GetSelectedItem();

	if (item)
	{
		switch (action->GetAction())
		{
			case OpInputAction::ACTION_GET_ACTION_STATE:
			{
				OpInputAction* child_action = action->GetChildAction();

				switch (child_action->GetAction())
				{
				    case OpInputAction::ACTION_GO_TO_CONTACT_HOMEPAGE:
					{
						if (!item->IsFolder())
						{
							child_action->SetEnabled(item->GetUrl().Length() > 0);
							return TRUE;
						}
						else
						{
							HotlistModel* model = item->GetModel();
							if (model)
							{
								OpINT32Vector index_list;
								model->GetIndexList(item->GetIndex(),index_list, TRUE, 10, OpTypedObject::CONTACT_TYPE);
								for (UINT32 i = 0; i < index_list.GetCount(); i++)
								{
									HotlistModelItem* itm = model->GetItemByIndex(index_list.Get(i));
									if (itm && itm->GetUrl().Length() && !itm->GetIsInsideTrashFolder())
									{
										child_action->SetEnabled(TRUE);
										return TRUE;
									}
								}
							}
							child_action->SetEnabled(FALSE);
							return TRUE;
						}
					}
				}
				break;
			} // ENC case ACTION_GET_ACTION_STATE


			case OpInputAction::ACTION_GO_TO_CONTACT_HOMEPAGE:
			{
				g_hotlist_manager->OpenUrl( item->GetID(), MAYBE, MAYBE, MAYBE, 0, -1, NULL, action->IsKeyboardInvoked());
				return TRUE;
			}

#ifdef M2_SUPPORT
			case OpInputAction::ACTION_VIEW_MESSAGES_FROM_CONTACT:
			{
				return ViewSelectedContactMail(TRUE, TRUE, action->IsKeyboardInvoked());
			}

			case OpInputAction::ACTION_COMPOSE_MAIL:
			case OpInputAction::ACTION_SEND_TEXT_IN_MAIL:
			{
				if (item->IsNote())
				{
					OpString empty, name;
					name.Set(item->GetName());
					MailTo mailto;
					mailto.Init(empty, empty, empty, empty, name); // set mail body  (message) to content of name
					MailCompose::ComposeMail(mailto);
					return TRUE;
				}
				else if (item->IsContact() || item->IsFolder())
				{
					return ComposeMailToSelectedContact();
				}
				return FALSE;
			}

#if defined(JABBER_SUPPORT) && 0
			case OpInputAction::ACTION_COMPOSE_INSTANT_MESSAGE:
			{

				return ComposeMessageToSelectedContact();
			}
			case OpInputAction::ACTION_SUBSCRIBE:
			{
				return SubscribeToPresenceOfSelectedContact();
			}
			case OpInputAction::ACTION_UNSUBSCRIBE:
			{
				return SubscribeToPresenceOfSelectedContact(FALSE);
			}
			case OpInputAction::ACTION_ALLOW_SUBSCRIPTION:
			{
				return AllowPresenceSubscription();
			}
			case OpInputAction::ACTION_DENY_SUBSCRIPTION:
			{
				return AllowPresenceSubscription(FALSE);
			}
#endif // JABBER_SUPPORT

#endif // M2_SUPPORT

#ifdef WEBSERVER_SUPPORT
			case OpInputAction::ACTION_OPERA_UNITE_GOTO_PUBLIC_PAGE:
			{
				if (item->IsUniteService())
				{
					UniteServiceModelItem* unite_item = static_cast<UniteServiceModelItem*>(item);
					unite_item->GoToPublicPage();
				}
				return TRUE;
			}
#endif
		}

		// Flag to know if the only item that is selected is the trash folder
		trash_selected = tree_view->GetSelectedItemCount() == 1 && item->GetIsTrashFolder();

		// Selected item is special folder...
		special_folder = item->GetIsSpecialFolder();

		if (!special_folder)
		{
			// ...or inside special folder
			special_folder = item->GetParentFolder() && item->GetParentFolder()->GetIsSpecialFolder();
		}
	}
	else // !item
	{
		trash_selected = FALSE;
		special_folder = FALSE;
	}

	// these actions don't need a selected item
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_PASTE:
				{
					INT32 item_type = g_hotlist_manager->GetItemTypeFromModelType(m_model->GetModelType());
					child_action->SetEnabled( (!special_folder && g_hotlist_manager->HasClipboardData(item_type))
											  || g_hotlist_manager->HasClipboardItems(item_type));
					return TRUE;
				}

			    case OpInputAction::ACTION_GO_TO_CONTACT_HOMEPAGE:
			    case OpInputAction::ACTION_SAVE_SELECTED_CONTACTS_AS:
				{
					// Disable if no item selected
					BOOL enabled = tree_view->GetSelectedItemCount() > 0;
					child_action->SetEnabled(enabled);
					return TRUE;
				}

#ifdef M2_SUPPORT
				case OpInputAction::ACTION_VIEW_MESSAGES_FROM_CONTACT:
				{
					BOOL enabled = tree_view->GetSelectedItemCount() > 0 && !trash_selected;
					if (item && item->IsFolder())
					{
						enabled = FALSE;
						OpINT32Vector idx_list;
						m_model->GetIndexList( item->GetIndex(), idx_list, FALSE, 1, OpTypedObject::CONTACT_TYPE );
						for (UINT32 i = 0; i < idx_list.GetCount(); i++)
						{
							HotlistModelItem* item = m_model->GetItemByIndex(idx_list.Get(i));
							if (item && !item->IsFolder())
							{
								enabled = TRUE;
								break;
							}
						}
					}
					child_action->SetEnabled(enabled);
					return TRUE;
				}
				case OpInputAction::ACTION_COMPOSE_MAIL:
				{
					child_action->SetEnabled(tree_view->GetSelectedItemCount() > 0 && !trash_selected);
					return TRUE;
				}
#endif // M2_SUPPORT
				case OpInputAction::ACTION_EDIT_PROPERTIES:
				{
					BOOL enabled = (tree_view->GetSelectedItemCount() == 1);

					//really, Opera can show your trashcan with contacts/bookmarks on the personal bar (through the properties dialog)  :D
					if (!IsBookmarks()  && !IsContacts() && item && (item->IsFolder() || trash_selected))
						enabled = FALSE;
					if (item && item->IsSeparator())
						enabled = FALSE;
#ifdef WEBSERVER_SUPPORT
					if (item && IsUniteServices())
					{
						UniteServiceModelItem * unite_item = static_cast<UniteServiceModelItem *>(item);
						if (unite_item->IsRootService() || unite_item->GetIsInsideTrashFolder() || unite_item->NeedsConfiguration())
						{
							enabled = FALSE;
						}
					}
#endif // WEBSERVER_SUPPORT
					child_action->SetEnabled(enabled);
					return TRUE;
				}
#ifdef GADGET_SUPPORT
				case OpInputAction::ACTION_OPEN_WIDGET:
				{
					if (child_action->GetActionData())
					{
						return FALSE;	//The action specifies a gadget, handle this action in Application.cpp
					}

#ifdef WEBSERVER_SUPPORT
					// Commented out as fix for bug DSK-253872, Cannot start root service
					/*
					if (SelectionContainsRootService(tree_view))
					{
						child_action->SetEnabled(FALSE);
						return TRUE;
					}
					*/
#endif // WEBSERVER_SUPPORT			

					OpINT32Vector gdg_list;
					BOOL enable = (GetSelectedGadgetsState(gdg_list, tree_view) == GadgetModelItem::Closed);
					child_action->SetEnabled(enable);//GetSelectedGadgetsState(gdg_list, tree_view) == GadgetModelItem::Closed);

					return TRUE;
				}

				case OpInputAction::ACTION_CLOSE_WIDGET:
				{
					if (child_action->GetActionData())
					{
						return FALSE;	//The action specifies a gadget, handle this action in Application.cpp
					}

#ifdef WEBSERVER_SUPPORT
					// Commented out as fix for bug DSK-253872, Cannot start root service
					/*
					if (SelectionContainsRootService(tree_view))
					{
						child_action->SetEnabled(FALSE);
						return TRUE;
					}
					*/
#endif // WEBSERVER_SUPPORT
					OpINT32Vector gdg_list;
					GadgetModelItem::GMI_State gadget_state = GetSelectedGadgetsState(gdg_list, tree_view);

					child_action->SetEnabled(gadget_state != GadgetModelItem::Closed && gadget_state != GadgetModelItem::NoneSelected);

					return TRUE;
				}

#ifdef WEBSERVER_SUPPORT
				case OpInputAction::ACTION_OPERA_UNITE_GOTO_PUBLIC_PAGE:
				{
					OpINT32Vector id_list;
					tree_view->GetSelectedItems(id_list);
					
					if (id_list.GetCount() == 0)
					{
						child_action->SetEnabled(FALSE);
						return TRUE;
					}

					// don't allow action if any of the selected items is:
					// * in trash
					// * not a unite service
					if (m_model)
					{
						for (UINT32 i = 0; i < id_list.GetCount(); i++)
						{
							HotlistModelItem* item = m_model->GetItemByID(id_list.Get(i));
							if (item && (item->GetIsInsideTrashFolder() || !item->IsUniteService()))
							{
								child_action->SetEnabled(FALSE);
								return TRUE;
							}
						}
					}
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
#endif // WEBSERVER_SUPPORT

      			case OpInputAction::ACTION_SEND_TEXT_IN_MAIL:
				{
					if (item)
					{
						HotlistModel* model = item->GetModel();
						if (item->GetIsTrashFolder())
						{
							child_action->SetEnabled(FALSE);
						}
						else if (model && model->GetCount() == 1 && model->GetTrashFolder())
						{
							child_action->SetEnabled(FALSE);
						}
						else
						{
							child_action->SetEnabled(TRUE);
						}
					}
					else
					{
						child_action->SetEnabled(FALSE);
					}
					return TRUE;
				}
				break;
#endif // GADGET_SUPPORT
			}
			break;

		}// End case ACTION_GET_ACTION_STATE

		case OpInputAction::ACTION_EDIT_PROPERTIES:
		{
			if (!item && m_folders_view)
				item = (HotlistModelItem*)m_folders_view->GetSelectedItem();

			if( tree_view->GetSelectedItemCount() == 1 && item)
			{
				if (IsNotes())
				{
					return g_input_manager->InvokeAction(action, GetParentInputContext());
				}

				g_hotlist_manager->EditItem( item->GetID(), GetParentDesktopWindow() );
			}
			return TRUE;
		}

#ifdef GADGET_SUPPORT
		case OpInputAction::ACTION_OPEN_WIDGET:
		{
			if (action->GetActionData())
				return FALSE;	// handle action in application, it specifies a gadget

			OpINT32Vector id_list;
			// Check if it's already open
			if (GetSelectedGadgetsState(id_list, tree_view) != GadgetModelItem::Closed)
			{
				return FALSE;
			}

			if (!id_list.GetCount())
			{
				return FALSE;
			}

			// Open all the gadgets
#ifdef WEBSERVER_SUPPORT
			g_hotlist_manager->OpenGadgets(id_list, TRUE, HotlistModel::UniteServicesRoot);
#endif // WEBSERVER_SUPPORT
			return TRUE;
		}

		case OpInputAction::ACTION_CLOSE_WIDGET:
		{
			if (action->GetActionData())
				return FALSE;	// handle action in application, it specifies a gadget

			OpINT32Vector id_list;
			// Check if it's already closed
			if (GetSelectedGadgetsState(id_list, tree_view)  == GadgetModelItem::Closed)
				return FALSE;

			if (!id_list.GetCount())
				return FALSE;

			// Close all the gadgets
#ifdef WEBSERVER_SUPPORT
			g_hotlist_manager->OpenGadgets(id_list, FALSE, HotlistModel::UniteServicesRoot);
#endif // WEBSERVER_SUPPORT

			return TRUE;
		}
#endif // GADGET_SUPPORT

		case OpInputAction::ACTION_DELETE:
		case OpInputAction::ACTION_CUT:
		{
			OpINT32Vector id_list;
			tree_view->GetSelectedItems( id_list );

#ifdef WEBSERVER_SUPPORT
			HotlistModel* model = g_hotlist_manager->GetUniteServicesModel();
			OP_ASSERT(model);
			for (UINT32 i = 0; i < id_list.GetCount(); i++)
			{
				UniteServiceModelItem* item = static_cast<UniteServiceModelItem*>(model->GetItemByIndex(id_list.Get(i)));
				if (item && item->IsRootService())
				{
					id_list.Remove(i);
				}
			}
#endif

			if( id_list.GetCount() == 0 )
			{
				return FALSE;
			}
			if( action->GetAction() == OpInputAction::ACTION_DELETE )
			{
				g_hotlist_manager->Delete( id_list, FALSE );
			}
			else if( action->GetAction() == OpInputAction::ACTION_CUT )
			{
				g_hotlist_manager->Delete( id_list, TRUE );
			}
			return TRUE;
		}
		case OpInputAction::ACTION_COPY:
		{
#ifdef WEBSERVER_SUPPORT
			if (IsUniteServices())
			{
				return TRUE;
			}
#endif // WEBSERVER_SUPPORT
		}
		break;

		case OpInputAction::ACTION_SAVE_SELECTED_CONTACTS_AS:
		{
			OpINT32Vector id_list;
			tree_view->GetSelectedItems( id_list, TRUE );
			if( id_list.GetCount() > 0 && item )
			{
				g_hotlist_manager->SaveSelectedAs( item->GetID(), id_list, HotlistManager::SaveAs_Export );
			}
			return TRUE;
		}

		case OpInputAction::ACTION_PASTE:
		{
#ifdef WEBSERVER_SUPPORT
			if (IsUniteServices())
			{
				return TRUE;
			}
#endif // WEBSERVER_SUPPORT
		}
		break;

		case OpInputAction::ACTION_NEW_SEPERATOR:
		{
			int item_id = item ? item->GetID() : GetCurrentFolderID();
			if (item)
			{
				if (item->GetIsTrashFolder() || item ->GetIsInsideTrashFolder())
				{
					item_id = item->GetModel()->GetModelType();
				}
				else
					item_id = item->GetID();
			}

			g_hotlist_manager->NewSeparator( item_id );
			return TRUE;
		}

		case OpInputAction::ACTION_NEW_FOLDER:
		{
			INT32 item_id;
			if (item)
			{
				item_id = item->GetID();
			}
			else
			{
				item_id = GetCurrentFolderID();
			}

			g_hotlist_manager->NewFolder( item_id, GetParentDesktopWindow(), tree_view );
			return TRUE;
		}

		case OpInputAction::ACTION_NEW_CONTACT:
		{
			g_hotlist_manager->NewContact( item ? item->GetID() : GetCurrentFolderID(), GetParentDesktopWindow() );
			return TRUE;
		}

		case OpInputAction::ACTION_NEW_GROUP:
		{
			g_hotlist_manager->NewGroup( item ? item->GetID() : GetCurrentFolderID(), GetParentDesktopWindow() );
			return TRUE;
		}

		case OpInputAction::ACTION_NEW_NOTE:
		{
			HotlistManager::ItemData item_data;

			g_hotlist_manager->NewNote(item_data, item ? item->GetID() : GetCurrentFolderID(), GetParentDesktopWindow(), m_hotlist_view);
			g_input_manager->InvokeAction(OpInputAction::ACTION_EDIT_PROPERTIES, 0, NULL, GetParentInputContext());
			return TRUE;
		}
	}

	return OpHotlistTreeView::OnInputAction(action);
}

///////////////////////////////////////////////////////////////////////////////////////////

void OpHotlistView::RemoveTrashItemsFromSelected(OpINT32Vector& id_list)
{
	if (m_model)
	{
		for (UINT32 i = 0; i < id_list.GetCount(); i++)
		{
			if (m_model->GetItemByID(id_list.Get(i))->GetIsInsideTrashFolder())
			{
				id_list.Remove(i);
				i--;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////

GadgetModelItem::GMI_State OpHotlistView::GetSelectedGadgetsState(OpINT32Vector& id_list, OpTreeView *tree_view)
{
	GadgetModelItem::GMI_State gadget_state = GadgetModelItem::Closed;

	// Retrieve the selected items ignoring ones in trash
	tree_view->GetSelectedItems( id_list );

	// Remove any selected items in the trash
	if (m_model)
	{
		for (UINT32 i = 0; i < id_list.GetCount(); i++)
		{
			HotlistModelItem *item = m_model->GetItemByID(id_list.Get(i));
			if ((!item->IsGadget() && !item->IsUniteService()) || item->GetIsInsideTrashFolder())
			{
				id_list.Remove(i);
				i--;
			}
		}
	}

	// Determine the selected items state
	// Expands the selection to include subitems if folders are selected
#ifdef WEBSERVER_SUPPORT
	if (IsUniteServices())
	{
 		gadget_state = g_hotlist_manager->GetSelectedGadgetsState(id_list, HotlistModel::UniteServicesRoot);
	}
#endif // WEBSERVER_SUPPORT

	return gadget_state;
}

/***********************************************************************************
 *
 *	OnMouseEvent
 *
 ***********************************************************************************/

void OpHotlistView::HandleMouseEvent(OpTreeView* widget, HotlistModelItem* item, INT32 pos, MouseButton button, BOOL down, UINT8 nclicks)
{
	BOOL shift = GetWidgetContainer()->GetView()->GetShiftKeys() & SHIFTKEY_SHIFT;
	BOOL ctrl = GetWidgetContainer()->GetView()->GetShiftKeys() & SHIFTKEY_CTRL;

	// single or double click mode
	BOOL click_state_ok = (IsSingleClick() && !down && nclicks == 1 && !shift) || nclicks == 2;

	if (IsContacts())
	{
		ViewSelectedContactMail(click_state_ok, ctrl);
	}
	else if (IsNotes())
	{
		if (nclicks == 2)
		{
			if (item->GetUrl().HasContent())
			{
				g_application->GoToPage(item->GetUrl().CStr());
			}
			else if (GetWorkspace() && GetWorkspace()->GetActiveDesktopWindow() &&
				GetWorkspace()->GetActiveDesktopWindow()->GetRecentKeyboardChildInputContext())
			{
				if(!item->GetIsTrashFolder())
				{
					OpInputContext* input_context = GetWorkspace()->GetActiveDesktopWindow()->GetRecentKeyboardChildInputContext();
					OpString name;
					name.Set(item->GetName());

					if (strcmp(input_context->GetInputContextName(),"Edit Widget")==0)
					{
						OpInputAction action(OpInputAction::ACTION_INSERT);
						action.SetActionDataString(name.CStr());
						input_context->OnInputAction(&action);

					}
					else if (item->IsNote())
					{
						OpString empty;
						OpString name;
						name.Set(item->GetName());
						MailTo mailto;
						mailto.Init(empty, empty, empty, empty, name); // set mail body  (message) to content of name
						MailCompose::ComposeMail(mailto);
						return;
					}

					input_context->SetFocus(FOCUS_REASON_OTHER);
				}
			}
			else if (item->IsNote())
			{
				OpString empty, name;
				name.Set(item->GetName());
				MailTo mailto;
				mailto.Init(empty, empty, empty, empty, name); // set mail body  (message) to content of name
				MailCompose::ComposeMail(mailto);
			}
		}
	}
#ifdef WEBSERVER_SUPPORT
	else if (IsUniteServices() && item->IsUniteService() && nclicks == 2 )
	{
		static_cast<UniteServiceModelItem*>(item)->GoToPublicPage();
	}
	else if (IsUniteServices() && item->IsUniteService()) // not nclicks == 2
	{
		if(widget->GetType() == WIDGET_TYPE_TREEVIEW 
			&& pos != -1
			&& ((OpTreeView*)widget)->IsHoveringAssociatedImageItem(pos))
		{
			OpInputAction action(OpInputAction::ACTION_EDIT_PROPERTIES);
			INT32 state = g_input_manager->GetActionState(&action, this);
			if (state & OpInputAction::STATE_ENABLED)
			{
				// Show dialog
				g_input_manager->InvokeAction(OpInputAction::ACTION_EDIT_PROPERTIES, 1);
			}
		}
	}
#endif
}



/***********************************************************************************
 *
 *	ShowContextMenu
 *
 ***********************************************************************************/

BOOL OpHotlistView::ShowContextMenu(const OpPoint &point,
									BOOL center,
									OpTreeView* view,
									BOOL use_keyboard_context)
{
	if (!view)
		return FALSE;

	HotlistModelItem* item = (HotlistModelItem*) view->GetSelectedItem();

	const char *menu_name = 0;

	if (IsContacts())
	{
		if( item && item->GetIsTrashFolder() )
			menu_name = "Contact Trash Popup Menu";
		else
			menu_name = "Contact Item Popup Menu";
	}
	else if (IsNotes())
	{
		if( item && item->GetIsTrashFolder() )
			menu_name = "Note Trash Popup Menu";
		else if (item && item->IsFolder())
			menu_name = "Note Folder Popup Menu";
		else
			menu_name = "Note Item Popup Menu";
	}
#ifdef WEBSERVER_SUPPORT
	else if (IsUniteServices()) // TODO: Fix
	{
		if(item)
		{
			if (item->GetIsTrashFolder())
			{
				menu_name = "Unite Trash Popup Menu";
			}
			else if (item->IsSeparator())
			{
				menu_name = "Unite Separator Popup Menu";
			}
			else if (item->IsFolder())
			{
				menu_name = "Unite Folder Popup Menu";
			}
			else if (item->IsRootService())
			{
				if (static_cast<UniteServiceModelItem *>(item)->IsGadgetRunning())
				{
					menu_name = "Webserver Panel Popup Menu";
				}
				else
				{
					menu_name = "Webserver Panel Setup Popup Menu";
				}
			}
			else
			{
				menu_name = "Unite Item Popup Menu";
			}
		}
		else
		{
			menu_name = "Webserver Panel Popup Menu";
		}
	}
#endif // WEBSERVER_SUPPORT

	if (menu_name)
	{
		OpPoint p = point + GetScreenRect().TopLeft();
		g_application->GetMenuHandler()->ShowPopupMenu(menu_name, PopupPlacement::AnchorAt(p, center), 0, use_keyboard_context);
	}

	return TRUE;
}

/***********************************************************************************
 *
 *	ViewSelectedContactMail
 *
 ***********************************************************************************/

BOOL OpHotlistView::ViewSelectedContactMail(BOOL force,
											BOOL include_folder_content,
											BOOL ignore_modifier_keys)
{
#ifdef M2_SUPPORT
	if (m_detailed && !force)
		return FALSE; // Not jump to mail window on single click etc. when detailed
	ContactModelItem* contact = static_cast<ContactModelItem*>(m_hotlist_view->GetSelectedItem());
	if (!g_m2_engine->GetIndexById(contact->GetM2IndexId()))
		contact->SetM2IndexId(0);
	return g_application->GoToMailView(contact->GetM2IndexId(TRUE),0,contact->GetName().CStr())? TRUE : FALSE;
#else
	return FALSE;
#endif
}


/***********************************************************************************
 *
 *	ComposeMailToSelectedContact
 *
 ***********************************************************************************/

BOOL OpHotlistView::ComposeMailToSelectedContact()
{
	OpINT32Vector id_list;
	m_hotlist_view->GetSelectedItems( id_list );

	if (id_list.GetCount() > 0)
	{
		MailCompose::ComposeMail(id_list);
		return TRUE;
	}

	return FALSE;
}

#if defined(JABBER_SUPPORT) && 0

/***********************************************************************************
 *
 *  ComposeMessageToSelectedContact
 *
 ***********************************************************************************/

BOOL OpHotlistView::ComposeMessageToSelectedContact()
{
	OpINT32Vector id_list;
	m_hotlist_view->GetSelectedItems(id_list);

	return g_application->ComposeMessage(id_list) != NULL;
}

/***********************************************************************************
 *
 *  SubscribeToPresenceOfSelectedContact
 *
 ***********************************************************************************/

BOOL OpHotlistView::SubscribeToPresenceOfSelectedContact(BOOL subscribe)
{
	OpINT32Vector id_list;
	m_hotlist_view->GetSelectedItems(id_list);

	return g_application->SubscribeToPresence(id_list, subscribe);
}


/***********************************************************************************
 *
 *  AllowPresenceSubscription
 *
 ***********************************************************************************/

BOOL OpHotlistView::AllowPresenceSubscription(BOOL allow)
{
	OpINT32Vector id_list;
	m_hotlist_view->GetSelectedItems(id_list);

	return g_application->AllowPresenceSubscription(id_list, allow);
}

#endif // JABBER_SUPPORT


/***********************************************************************************
 *
 *	OnDragStart
 *
 ***********************************************************************************/

void OpHotlistView::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	if (widget == m_folders_view || widget == m_hotlist_view)
	{
		HotlistModelItem* item = (HotlistModelItem*) ((OpTreeView*)widget)->GetItemByPosition(pos);

		// Not allowed to drag root service
		if (item->IsRootService())
			return;

		if( item )
		{
			DesktopDragObject* drag_object = 0;

			if( item->GetType() == SEPARATOR_TYPE )
				drag_object = widget->GetDragObject(DRAG_TYPE_SEPARATOR, x, y);
			else if( IsContacts() )
				drag_object = widget->GetDragObject(DRAG_TYPE_CONTACT, x, y);
			else
				drag_object = widget->GetDragObject(DRAG_TYPE_NOTE, x, y);

			if( drag_object )
			{
				if( item->GetType() == SEPARATOR_TYPE )
				{
					// Nothing here
				}
				else if (item->GetType() == NOTE_TYPE)
				{
					DragDrop_Data_Utils::SetText(drag_object, item->GetName().CStr());
					drag_object->SetURL(item->GetUrl().CStr());
				}
				else if (item->GetType() == CONTACT_TYPE)
				{
					OpString full_email;

					full_email.Set(item->GetName());
					if (item->GetMail().HasContent())
					{
						full_email.Append(UNI_L(" <"));
						full_email.Append(item->GetMail());
						full_email.Append(UNI_L(">"));
					}
					DragDrop_Data_Utils::SetText(drag_object, full_email.CStr());
				}
			}

			g_drag_manager->StartDrag(drag_object, NULL, FALSE);
		}
	}
}



INT32 OpHotlistView::OnDropItem(HotlistModelItem* hmi_target,DesktopDragObject* drag_object, INT32 i, DropType drop_type, DesktopDragObject::InsertType insert_type, INT32 *first_id, BOOL force_delete)
{
	HotlistModelItem* to   = hmi_target;
	HotlistModelItem* from = m_model->GetItemByID(drag_object->GetID(i));
	OP_ASSERT(from);

#ifdef WEBSERVER_SUPPORT
	BOOL has_running_server = FALSE;
	BOOL delete_all_webservers = FALSE;
	BOOL no_dragging_separators = FALSE;
	INT32 many_webservers = 0;
	HotlistModelItem* hmi_src    = m_model->GetItemByID(drag_object->GetID(i));
	
	// Check if we have any running services, but only the first time.
	if (i == 0)
	{
		for (INT32 j = 0; j < drag_object->GetIDCount(); j++)
		{
			HotlistModelItem* hmi_check    = m_model->GetItemByID(drag_object->GetID(j));
			if (!hmi_check || !hmi_check->IsUniteService()) 
			{
				continue;
			}
			no_dragging_separators = TRUE;
			if ( static_cast<GadgetModelItem *>(hmi_check)->IsGadgetRunning() ) 
			{
				has_running_server = TRUE;
				many_webservers ++;
			}
			if (has_running_server && many_webservers > 1)
				break;
		}
	}

	if ( has_running_server && !delete_all_webservers && hmi_target && hmi_target->GetIsInsideTrashFolder() )
	{	
		// move to trash, gadget running: show warning
		OpString title, message;
		g_languageManager->GetString(Str::S_DELETE, title);
		g_languageManager->GetString(
			many_webservers > 1 ?
				Str::D_WEBSERVER_MOVE_RUNNING_SERVICES_TO_TRASH_WARNING :
				Str::D_WEBSERVER_MOVE_RUNNING_SERVICE_TO_TRASH_WARNING
			, message
		);

		INT32 result = 0;

		SimpleDialog* dialog = OP_NEW(SimpleDialog, ());
		if (dialog)
		{
			dialog->SetOkTextID(Str::S_DELETE);
			dialog->SetProtectAgainstDoubleClick(SimpleDialog::ForceOff);
			dialog->Init(WINDOW_NAME_DELETE_RUNNING_UNITE_WIDGET, title, message, g_application->GetActiveDesktopWindow(), Dialog::TYPE_OK_CANCEL, Dialog::IMAGE_WARNING, TRUE, &result);
			if (result == Dialog::DIALOG_RESULT_NO)
			{
				return -1;// abort all
			}
			delete_all_webservers  = TRUE;
		}
	}
	if ( no_dragging_separators && hmi_src->IsSeparator() )
		return 1; // abort this one


#endif // WEBSERVER_SUPPORT

	// if USE_MOVE_ITEM on, only way to get here is force_delete is TRUE
	// if force_delte is TRUE, item will be deleted completely, so SetIsMovingItems is not needed

	INT32 flag = (drop_type == DROP_MOVE) ? (HotlistModel::Personalbar|HotlistModel::Panel|HotlistModel::Trash) : 0;

	m_model->SetIsMovingItems(drop_type == DROP_MOVE);

	// Save item (and any children) to move
	HotlistGenericModel tmp(from->GetModel()->GetModelType(), TRUE);

	tmp.SetIsMovingItems(drop_type == DROP_MOVE);

	BOOL from_trash_to_normal = FALSE;

	if (from && from->IsBookmark() && from->GetIsInsideTrashFolder()
		&& (to && !to->GetIsTrashFolder() && !to->GetIsInsideTrashFolder()
			|| to && to->GetIsTrashFolder() && insert_type != DesktopDragObject::INSERT_INTO))
	{
		from_trash_to_normal = TRUE;
	}


	/**
	 * Get a copy of the item from in tmp model
	 * Paste this item from tmp into destination model
	 * Delete the original item from destination model
	 */
	tmp.CopyItem(from, flag);

	// Input saved item at new location
	m_model->SetDirty( TRUE );

	INT32 item_id = 0;

	/**
	 * Changed order of Delete and Paste.
	 * The original Order here -> Make copy, paste in, then delete
	 * is a problem, because we'll get the add event
	 * and then the removing event on the same bookmark
	 * This broke the hash table code because the item would first
	 * be added (adding the copy instead of the original item)
	 * and then removed (removing the copy)
	 *
	 **/
	if (to == NULL || !to->IsGroup())
	{
		if( drop_type == DROP_MOVE)
		{
			// Delete source item
			static_cast<HotlistGenericModel*>(m_model)->DeleteItem( from, FALSE, TRUE /* handle_trash*/, TRUE);
		}
	}
	if (!force_delete)
	{
		item_id = static_cast<HotlistGenericModel*>(m_model)->PasteItem( tmp, to, insert_type, flag);

		if( first_id )
		{
			*first_id = item_id;
		}

	}

	if (from_trash_to_normal)
	{
		HotlistModelItem* item = m_model->GetItemByID(*first_id);
		if (item && item->IsBookmark())
		{
			m_model->ItemUnTrashed(item);
		}
	}


	m_model->SetIsMovingItems(FALSE);

	return 0;
}

BOOL OpHotlistView::OnExternalDragDrop(OpTreeView* tree_view, OpTreeModelItem* item, DesktopDragObject* drag_object, DropType drop_type, DesktopDragObject::InsertType insert_type, BOOL drop, INT32& new_selection_id)
{
	if (IsContacts())
	{
		// Find out what drop and insert type to use

		DropType drop_type = drag_object->GetSuggestedDropType();
		DesktopDragObject::InsertType insert_type = drag_object->GetSuggestedInsertType();

		// if no drop type suggested, use move

		if (drop_type == DROP_NOT_AVAILABLE)
		{
			drop_type = DROP_MOVE;
		}

		if (item && !g_hotlist_manager->IsFolder(item) && insert_type == DesktopDragObject::INSERT_INTO)
		{
			// cannot drop into non folders

			insert_type = DesktopDragObject::INSERT_BEFORE;
		}

		// if sorted in some ways, insert before/after is not available

		if (tree_view->GetSortByColumn() != -1)
		{
			insert_type = DesktopDragObject::INSERT_INTO;
		}

		if (drag_object->GetURL() && *drag_object->GetURL())
		{
			if (drop)
			{
				if( drag_object->GetURLs().GetCount() > 0 )
				{
					for( UINT32 i=0; i<drag_object->GetURLs().GetCount(); i++ )
					{
						HotlistManager::ItemData item_data;
						if( i == 0 )
							item_data.name.Set( drag_object->GetTitle() );
						item_data.url.Set( *drag_object->GetURLs().Get(i) );
						INT32 target_id = item ? item->GetID() : HotlistModel::BookmarkRoot;
						g_hotlist_manager->DropItem( item_data, OpTypedObject::BOOKMARK_TYPE, target_id, insert_type, TRUE, drag_object->GetType(), &new_selection_id );
					}
				}
				else
				{
					HotlistManager::ItemData item_data;
					item_data.name.Set( drag_object->GetTitle() );
					item_data.url.Set( drag_object->GetURL() );
					INT32 target_id = item ? item->GetID() : HotlistModel::ContactRoot;
					g_hotlist_manager->DropItem( item_data, OpTypedObject::BOOKMARK_TYPE, target_id, insert_type, TRUE, drag_object->GetType(), &new_selection_id );
				}
			}
			else
			{
				HotlistManager::ItemData item_data;
				INT32 target_id = item ? item->GetID() : HotlistModel::ContactRoot;
				if( g_hotlist_manager->DropItem( item_data, OpTypedObject::BOOKMARK_TYPE, target_id, insert_type, FALSE, drag_object->GetType()) )
				{
					drag_object->SetInsertType(insert_type);
					drag_object->SetDesktopDropType(DROP_COPY);
				}
			}
		}
	}
	else if (IsNotes())
	{
		const uni_char* name = DragDrop_Data_Utils::GetText(drag_object) ? DragDrop_Data_Utils::GetText(drag_object) : drag_object->GetURL();
		if ( name && *name )
		{
			if( drop )
			{
				if(drag_object->GetURLs().GetCount() > 0 && drag_object->GetType() != DRAG_TYPE_WINDOW)
				{
					for( UINT32 i=0; i<drag_object->GetURLs().GetCount(); i++ )
					{
						HotlistManager::ItemData item_data;
						if( i == 0 )
							item_data.name.Set( name );
						item_data.url.Set( *drag_object->GetURLs().Get(i) );
						INT32 target_id = item ? item->GetID() : HotlistModel::NoteRoot;
						g_hotlist_manager->DropItem( item_data, OpTypedObject::NOTE_TYPE, target_id, insert_type, TRUE, drag_object->GetType(), &new_selection_id );
					}
				}
				else
				{
					HotlistManager::ItemData item_data;
					item_data.name.Set( name );
					item_data.url.Set( drag_object->GetURL() );
					INT32 target_id = item ? item->GetID() : HotlistModel::NoteRoot;
					g_hotlist_manager->DropItem( item_data, OpTypedObject::NOTE_TYPE, target_id, insert_type, TRUE, drag_object->GetType(),&new_selection_id );
				}
			}
			else
			{
				HotlistManager::ItemData item_data;
				INT32 target_id = item ? item->GetID() : HotlistModel::NoteRoot;
				if( g_hotlist_manager->DropItem( item_data, OpTypedObject::NOTE_TYPE, target_id, insert_type, FALSE, drag_object->GetType() ) )
				{
					drag_object->SetInsertType(insert_type);
					drag_object->SetDesktopDropType(DROP_COPY);
				}
			}
		}
	}

	return TRUE;
}

void OpHotlistView::OnSetDetailed(BOOL detailed)
{
	if (IsContacts())
	{
		SetHorizontal(detailed != FALSE);

		m_hotlist_view->SetName(m_detailed ? "Contacts View" : "Contacts View Small");
		m_hotlist_view->SetColumnVisibility(1, m_detailed);
		m_hotlist_view->SetColumnVisibility(2, m_detailed);

		m_hotlist_view->SetLinkColumn(IsSingleClick() ? 0 : -1);
	}
	else if (IsNotes())
	{
		m_hotlist_view->SetName(m_detailed ? "Notes View" : "Notes View Small");
		// m_hotlist_view->SetLinkColumn(IsSingleClick() ? 0 : -1);
	}
}

BOOL OpHotlistView::AllowDropItem(HotlistModelItem* to, HotlistModelItem* from, DesktopDragObject::InsertType& insert_type, DropType& drop_type)
{
#ifdef WEBSERVER_SUPPORT
	if(to)
	{
		// Disallow dragging trash
		if (from)
		{
			if (to->GetModel() && to->GetModel()->GetModelType() == HotlistModel::UniteServicesRoot)
			{
				if (from->GetIsTrashFolder() || from->IsRootService())
				{
					return FALSE;
				}
			}
		}

		// Check if inserting hmi after to is allowed
		if (to->GetModel() && from && to->GetModel()->GetModelType() == HotlistModel::UniteServicesRoot)
		{
			if (!((HotlistGenericModel*)to->GetModel())->IsDroppingGadgetAllowed(from, to, insert_type))
			{
				return FALSE;
			}
			if (drop_type == DROP_COPY)
			{
				return FALSE;
			}
			
		}

		if (to->GetModel() && to->GetModel()->GetModelType() == HotlistModel::UniteServicesRoot)
		{
			if (insert_type == DesktopDragObject::INSERT_AFTER && !to
				|| insert_type == DesktopDragObject::INSERT_BEFORE && (!to->IsFolder() && !to->GetParentFolder()) && !to->GetPreviousItem())
				{
					return FALSE;
				}
		}
	}
#endif
	return OpHotlistTreeView::AllowDropItem(to,from,insert_type,drop_type);
}
