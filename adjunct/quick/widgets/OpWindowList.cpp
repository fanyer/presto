/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

#include "adjunct/quick/widgets/OpWindowList.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/Application.h"
#ifdef _UNIX_DESKTOP_
# include "adjunct/quick/managers/DesktopClipboardManager.h"
#endif // _UNIX_DESKTOP_
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/models/DesktopGroupModelItem.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/widgets/OpPagebar.h"
#include "adjunct/quick/widgets/PagebarButton.h"
#include "modules/widgets/WidgetContainer.h"

#include "modules/dragdrop/dragdrop_manager.h"

/***********************************************************************************
**
**	OpWindowList
**
***********************************************************************************/

OpWindowList::OpWindowList()
	: m_detailed(FALSE)
{
	SetTreeModel(g_application->GetWindowList());
	SetShowThreadImage(TRUE);
	SetMultiselectable(TRUE);
	SetListener(this);
	SetMatchType(MATCH_STANDARD);

	SetDetailed(FALSE, TRUE);

	if(g_pcui->GetIntegerPref(PrefsCollectionUI::EnableDrag)&DRAG_BOOKMARK)
	{
		SetDragEnabled(TRUE);
	}

	// Hardcoded - not good but better than nothing.
	SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_ACTIVATE_WINDOW)));

	SetSelectedItem(0);

	GetBorderSkin()->SetImage("Panel Treeview Skin", "Treeview Skin");
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL OpWindowList::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			OpTreeModelItem* item = GetSelectedItem();
			DesktopWindow* dw = item
					? static_cast<DesktopWindowCollectionItem*>(item)
							->GetDesktopWindow()
					: 0;

			switch(child_action->GetAction())
			{
				case OpInputAction::ACTION_MAXIMIZE_PAGE:
				case OpInputAction::ACTION_MINIMIZE_PAGE:
				case OpInputAction::ACTION_RESTORE_PAGE:
				case OpInputAction::ACTION_ACTIVATE_WINDOW:
				case OpInputAction::ACTION_DETACH_PAGE:
					{
						child_action->SetEnabled( dw && dw->GetType() != WINDOW_TYPE_BROWSER && dw->GetWorkspace() );
					}
					return TRUE;

				case OpInputAction::ACTION_RELOAD:
					{
						child_action->SetEnabled( dw && dw->GetType() == WINDOW_TYPE_DOCUMENT && dw->GetWorkspace() );
					}
					return TRUE;

				case OpInputAction::ACTION_LOCK_PAGE:
				case OpInputAction::ACTION_UNLOCK_PAGE:
					{
						UINT32 items = GetSelectedItemCount();
						if(items > 1)
						{
							child_action->SetSelected(FALSE);
						}
						else
						{
							child_action->SetEnabled( dw && dw->GetType() != WINDOW_TYPE_BROWSER && dw->GetWorkspace() );
							if(child_action->GetAction() == OpInputAction::ACTION_LOCK_PAGE)
							{
								child_action->SetSelected( dw && dw->IsLockedByUser());
							}
							else
							{
								child_action->SetSelected( dw && !dw->IsLockedByUser());
							}
						}
					}
					return TRUE;

				case OpInputAction::ACTION_CLOSE_PAGE:
					{
						UINT32 items = GetSelectedItemCount();
						if(items > 1)
						{
							child_action->SetEnabled(TRUE);
						}
						else
						{
							child_action->SetEnabled( dw && dw->IsClosableByUser());
						}
					}
					return TRUE;

				case OpInputAction::ACTION_DISSOLVE_TAB_GROUP:
				case OpInputAction::ACTION_CLOSE_TAB_GROUP:
					child_action->SetEnabled(item && item->GetType() == WINDOW_TYPE_GROUP);
					return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_LOCK_PAGE:
		case OpInputAction::ACTION_UNLOCK_PAGE:
			{
				OpINT32Vector items;
				INT32 idx;
				INT32 count = GetSelectedItems(items, FALSE);

				for(idx = 0; idx < count; idx++)
				{
					INT32 id = items.Get((UINT32)idx);
					OpTreeModelItem* item = GetItemByPosition(id);
					if (item)
					{
						DesktopWindow *window =
								static_cast<DesktopWindowCollectionItem*>(item)
										->GetDesktopWindow();
						if(window != NULL)
						{
							OpPagebar* pagebar = NULL;

							window->SetLockedByUser(window->IsLockedByUser() ? FALSE : TRUE);

							pagebar = (OpPagebar *)window->GetWidgetByType(OpTypedObject::WIDGET_TYPE_PAGEBAR);
							if(pagebar)
							{
								INT32 pos = pagebar->FindWidgetByUserData(window);
								if (pos >= 0)
								{
									OpWidget *button = pagebar->GetWidget(pos);
									if(button && button->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
									{
										pagebar->OnLockedByUser(static_cast<PagebarButton *>(button), window->IsLockedByUser());
									}
									else
									{
										if (button)
											button->Relayout(TRUE, TRUE);
									}
								}
							}
						}
					}
				}
				g_input_manager->UpdateAllInputStates();
			}
			return TRUE;

		// Only the first.
		case OpInputAction::ACTION_ACTIVATE_WINDOW:
		{
			OpTreeModelItem* item = GetSelectedItem();
			if (item)
			{
				DesktopWindow* window =
						static_cast<DesktopWindowCollectionItem*>(item)
								->GetDesktopWindow();
				if (!window)
					break;

				window->Activate();

#if defined(_UNIX_DESKTOP_)
				// We cannot do Raise() in Activate() in unix because focus-follows-mouse
				// focus mode shall activate but not auto raise a window [espen 2006-09-28]
				if( window->GetType() == WINDOW_TYPE_BROWSER )
				{
					window->GetOpWindow()->Raise();
				}
				else if( window->GetParentDesktopWindow() )
				{
					window->GetParentDesktopWindow()->GetOpWindow()->Raise();
					window->GetOpWindow()->Raise();
				}
#endif

			}
			return TRUE;
		}

		case OpInputAction::ACTION_DELETE:
		{
			OpInputAction new_action(OpInputAction::ACTION_CLOSE_PAGE);
			new_action.SetActionData(2);
			return OnInputAction(&new_action);
		}

		case OpInputAction::ACTION_SHOW_CONTEXT_MENU:
		{
			OnContextMenu(GetBounds().Center(), NULL, action->IsKeyboardInvoked());
			return TRUE;
		}
		case OpInputAction::ACTION_CLOSE_PAGE:
		{
			OpINT32Vector items;
			INT32 idx;
			INT32 count = GetSelectedItems(items, FALSE);

			for(idx = 0; idx < count; idx++)
			{
				INT32 id = items.Get((UINT32)idx);
				OpTreeModelItem* item = GetItemByPosition(id);
				if (item)
				{
					if (item->GetType() == WINDOW_TYPE_BROWSER)
					{
						BrowserDesktopWindow *window =
								static_cast<BrowserDesktopWindow*>(
										static_cast<DesktopWindowCollectionItem*>(
												item)->GetDesktopWindow());
						window->Close(FALSE, TRUE, FALSE);
						return TRUE;
					}
					else
					{
						DesktopWindow *window =
								static_cast<DesktopWindowCollectionItem*>(item)
										->GetDesktopWindow();
						if(window != NULL && window->IsClosableByUser())
						{
							window->Close(FALSE, TRUE, FALSE);
						}
					}
				}
			}
		}
		return TRUE;

		case OpInputAction::ACTION_FOCUS_NEXT_WIDGET:
		case OpInputAction::ACTION_FOCUS_PREVIOUS_WIDGET:
			return OpTreeView::OnInputAction(action);

		case OpInputAction::ACTION_DISSOLVE_TAB_GROUP:
		{
			DesktopWindowCollectionItem* item = static_cast<DesktopWindowCollectionItem*>(GetSelectedItem());
			if (item && item->GetType() == WINDOW_TYPE_GROUP)
				item->GetModel()->UnGroup(item);
			return TRUE;
		}
		case OpInputAction::ACTION_CLOSE_TAB_GROUP:
		{
			DesktopWindowCollectionItem* item = static_cast<DesktopWindowCollectionItem*>(GetSelectedItem());
			if (item && item->GetType() == WINDOW_TYPE_GROUP)
				item->GetModel()->DestroyGroup(item);
			return TRUE;
		}

		default:
		{
			// Prevent sending lowlevel events to desktop windows (bug #227626) [espen 2007-02-22]
			if( action->GetAction() < OpInputAction::LAST_ACTION )
			{
				OpINT32Vector items;
				int item_count = GetSelectedItems(items);
				BOOL handled = FALSE;

				for (INT32 i = 0; i < item_count; i++)
				{
					DesktopWindow* window = GetDesktopWindowByPosition(GetItemByID(items.Get(i)));
					if( window && window->OnInputAction(action) )
					{
						handled = TRUE;
					}
				}

				if (handled)
					return TRUE;
			}
		}
	}

	return OpTreeView::OnInputAction(action);
}

/***********************************************************************************
**
**	OnMouseEvent
**
***********************************************************************************/

void OpWindowList::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (!down && nclicks == 1 && button == MOUSE_BUTTON_2)
	{
//		OnContextMenu(OpPoint(x,y), FALSE);
	}
	else if (button == MOUSE_BUTTON_1 && (IsSingleClick() ? !down && nclicks == 1 : down && nclicks == 2))
	{
		if ((GetWidgetContainer()->GetView()->GetShiftKeys() & (SHIFTKEY_SHIFT | SHIFTKEY_CTRL)) == 0)
		{
			// if ctrl or shift are held, the window should not be activated // bug 283906
			DesktopWindow* window = GetDesktopWindowByPosition(pos);
			if (window)
			{
				window->Activate();
			}
		}
	}
#if defined(_UNIX_DESKTOP_)
	else if (down && button == MOUSE_BUTTON_3)
	{
		OpString text;
		g_desktop_clipboard_manager->GetText(text, true);
		OpenNewPage(text);
	}
#else
	else if( down && (button == MOUSE_BUTTON_3 || ( g_pcui->GetIntegerPref(PrefsCollectionUI::DoubleclickToClose) && button == MOUSE_BUTTON_1 && nclicks == 2 )) )
	{
		g_input_manager->InvokeAction(OpInputAction::ACTION_CLOSE_PAGE, 1, NULL, this);
	}
#endif	// defined(_UNIX_DESKTOP_)
}


void OpWindowList::OpenNewPage(const OpString &url)
{
	BOOL open_in_background = g_application->IsOpeningInBackgroundPreferred();

	BOOL saved_open_page_setting = g_pcui->GetIntegerPref(PrefsCollectionUI::OpenPageNextToCurrent);
	g_pcui->WriteIntegerL(PrefsCollectionUI::OpenPageNextToCurrent, FALSE);

	if( url.IsEmpty() )
	{
		DocumentDesktopWindow* page = 0;
		g_application->GetBrowserDesktopWindow( FALSE, open_in_background, TRUE, &page );
	}
	else
	{
		g_application->OpenURL( url, NO, YES, open_in_background ? YES : NO );
	}
	g_pcui->WriteIntegerL(PrefsCollectionUI::OpenPageNextToCurrent, saved_open_page_setting);
}



BOOL OpWindowList::OnContextMenu(const OpPoint& point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	DesktopWindowCollectionItem* item = static_cast<DesktopWindowCollectionItem*>(GetSelectedItem());
	if (!item)
		return FALSE;

	OpPoint p = point + GetScreenRect().TopLeft();
	g_application->GetMenuHandler()->ShowPopupMenu(item->GetContextMenu(), PopupPlacement::AnchorAt(p), 0, keyboard_invoked);
	return TRUE;
}

DesktopWindow* OpWindowList::GetDesktopWindowByPosition(INT32 pos)
{
	DesktopWindowCollectionItem* item =
			static_cast<DesktopWindowCollectionItem*>(GetItemByPosition(pos));
	return item ? item->GetDesktopWindow() : 0;
}


/***********************************************************************************
**
**	OnDragStart
**
***********************************************************************************/

void OpWindowList::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* drag_object = GetDragObject(DRAG_TYPE_WINDOW, x, y);

	if (!drag_object)
		return;

	DesktopWindow* window = GetDesktopWindowByPosition(pos);

	if(window)
	{
		if (window->GetType() == WINDOW_TYPE_DEVTOOLS
#ifdef DEVTOOLS_INTEGRATED_WINDOW
			|| window->IsDevToolsOnly()
#endif // DEVTOOLS_INTEGRATED_WINDOW
			|| window->GetType() == WINDOW_TYPE_BROWSER)
			return; // No dragging dragonfly window

		if (window->GetType() == WINDOW_TYPE_DOCUMENT)
		{
			drag_object->SetURL(((DocumentDesktopWindow*)window)->GetWindowCommander()->GetCurrentURL(FALSE));
			drag_object->SetTitle(((DocumentDesktopWindow*)window)->GetWindowCommander()->GetCurrentTitle());
		}
	}
	g_drag_manager->StartDrag(drag_object, NULL, FALSE);
}

/***********************************************************************************
**
**	OnDragMove
**
***********************************************************************************/

void OpWindowList::OnDragMove(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);

	if (drag_object->GetType() == DRAG_TYPE_WINDOW)
	{
		DesktopWindowCollectionItem* target = static_cast<DesktopWindowCollectionItem*>(GetItemByPosition(pos));
		if (target)
			return target->GetModel()->OnDragWindows(drag_object, target);
	}
	else if (drag_object->GetURL())
	{
		return OnDragURL(drag_object, pos);
	}
}

/***********************************************************************************
**
**	OnDragDrop
**
***********************************************************************************/

void OpWindowList::OnDragDrop(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);

	if (drag_object->GetType() == DRAG_TYPE_WINDOW)
	{
		DesktopWindowCollectionItem* target = static_cast<DesktopWindowCollectionItem*>(GetItemByPosition(pos));
		if (target)
			return target->GetModel()->OnDropWindows(drag_object, target);
	}
	else if (drag_object->GetURL())
	{
		return OnDropURL(drag_object, pos);
	}
}

void OpWindowList::OnDragURL(DesktopDragObject* drag_object, INT32 pos)
{
	drag_object->SetDesktopDropType(DROP_COPY);
	drag_object->SetInsertType(DesktopDragObject::INSERT_INTO);
}

void OpWindowList::OnDropURL(DesktopDragObject* drag_object, INT32 pos)
{
	OpenURLSetting setting;
	setting.m_new_window      = NO;
	setting.m_new_page        = YES;
	setting.m_in_background   = MAYBE;
	setting.m_target_position = pos;

	if( drag_object->GetURLs().GetCount() > 0 )
	{
		for (UINT32 i = 0; i < drag_object->GetURLs().GetCount(); i++)
		{
			setting.m_address.Set( *drag_object->GetURLs().Get(i) );
			g_application->OpenURL(setting);
		}
	}
	else
	{
		setting.m_address.Set( drag_object->GetURL() );
		g_application->OpenURL(setting);
	}
}

void OpWindowList::SetDetailed(BOOL detailed, BOOL force)
{
	if (!force && detailed == m_detailed)
		return;

	m_detailed = detailed;

	SetName(m_detailed ? "Windows View" : "Windows View Small");
	SetColumnWeight(1, 50);
	SetColumnWeight(2, 150);
	SetColumnVisibility(1, m_detailed);
	SetColumnVisibility(2, m_detailed);
}
