/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"

#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/models/DesktopGroupModelItem.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"

#include "modules/inputmanager/inputmanager.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/display/vis_dev.h"
#if defined(_X11_SELECTION_POLICY_)
# include "adjunct/quick/managers/DesktopClipboardManager.h"
# include "adjunct/quick/windows/BrowserDesktopWindow.h"
#endif

#include "modules/pi/OpWindow.h"

#include "modules/prefs/prefsmanager/collections/pc_ui.h"

#ifdef ENABLE_USAGE_REPORT
# include "adjunct/quick/usagereport/UsageReport.h"
#endif

/***********************************************************************************
**
**	OpWorkspace
**
***********************************************************************************/

OpWorkspace::OpWorkspace(DesktopWindowModelItem* model_item) 
	: m_window(NULL)
#ifdef MOUSE_GESTURE_ON_WORKSPACE
	, m_widget_container(NULL)
#endif
	, m_active_desktop_window(NULL)
	, m_transparent_skins_enabled(FALSE)
	, m_parent_window_id(-1)
	, m_model_item(model_item)
{
	SetOnMoveWanted(TRUE);

	if (m_model_item && m_model_item->GetModel())
		OpStatus::Ignore(m_model_item->GetModel()->AddListener(this));
}

void OpWorkspace::OnDeleted()
{
	if (m_model_item)
		g_application->GetDesktopWindowCollection().RemoveListener(this);

	for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
	{
		m_listeners.Get(i)->OnWorkspaceDeleted(this);
	}

	// Mark them as closing so they don't get activated during closing.
	for (INT32 i = 0; i < GetDesktopWindowCount(); i++)
	{
		GetDesktopWindowFromStack(i)->SetIsClosing(TRUE);
	}

	while (GetDesktopWindowCount())
	{
		GetDesktopWindowFromStack(0)->Close(TRUE, TRUE, TRUE);
	}

#ifdef MOUSE_GESTURE_ON_WORKSPACE
	OP_DELETE(m_widget_container);
#endif
	OP_DELETE(m_window);

	OpWidget::OnDeleted();
}

/***********************************************************************************
**
**	Init
**
***********************************************************************************/

OP_STATUS OpWorkspace::Init()
{
	OP_ASSERT(vis_dev);

	RETURN_IF_ERROR(DesktopOpWindow::Create(&m_window));

	RETURN_IF_ERROR(m_window->Init(OpWindow::STYLE_WORKSPACE, OpTypedObject::WINDOW_TYPE_UNKNOWN, NULL, GetWidgetContainer()->GetOpView()));

#ifdef MOUSE_GESTURE_ON_WORKSPACE
	m_widget_container = OP_NEW(WidgetContainer, (NULL));
	if (m_widget_container)
	{
		m_widget_container->SetParentInputContext(this);
		RETURN_IF_ERROR(m_widget_container->Init(OpRect(0,0,0,0), m_window));
		m_widget_container->GetRoot()->SetBackgroundColor(g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_WORKSPACE));
		m_widget_container->SetEraseBg(TRUE);
		// We want the OnContextMenu message
		m_widget_container->SetWidgetListener(this);
	}
#endif

	return OpStatus::OK;
}

/***********************************************************************************
**
**	OnAdded
**
***********************************************************************************/

void OpWorkspace::OnAdded()
{
	for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
	{
		m_listeners.Get(i)->OnWorkspaceAdded(this);
	}

	if (m_window)
	{
		m_window->SetParent(NULL, GetWidgetContainer()->GetOpView());
	}
	else
	{
		init_status = Init();
	}
}

/***********************************************************************************
**
**	OnShow
**
***********************************************************************************/

void OpWorkspace::OnShow(BOOL show)
{
	if (OpStatus::IsSuccess(init_status))
		m_window->Show(show);
}

void OpWorkspace::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	if (focus && m_active_desktop_window)
		m_active_desktop_window->RestoreFocus(reason);
}

/***********************************************************************************
**
**	OnWindowActivated
**
***********************************************************************************/

void OpWorkspace::OnWindowActivated(BOOL activate)
{
	if (activate && !m_active_desktop_window)
	{
		for (UINT32 i = 0; i < m_desktop_window_stack.GetCount(); i++)
		{
			DesktopWindow* desktop_window = m_desktop_window_stack.Get(i);

			if (!desktop_window->IsMinimized() && IsVisible())
			{
				desktop_window->Activate();
				break;
			}
		}
	}
}

/***********************************************************************************
**
**	OnLayout
**
***********************************************************************************/

void OpWorkspace::OnLayout()
{
	OP_ASSERT(m_window);

	OpRect rect = GetRect(TRUE);

	m_window->SetOuterPos(rect.x, rect.y);
	m_window->SetOuterSize(rect.width, rect.height);

#ifdef MOUSE_GESTURE_ON_WORKSPACE
	m_widget_container->SetSize(rect.width, rect.height);
#endif
}

/***********************************************************************************
**
**	AddDesktopWindow
**
***********************************************************************************/

OP_STATUS OpWorkspace::AddDesktopWindow(DesktopWindow* desktop_window)
{
	desktop_window->SetParentInputContext(this);

	SetTabStop(TRUE);

	RETURN_IF_ERROR(desktop_window->AddListener(this));

	if(g_application->IsBrowserStarted() && desktop_window->GetType() == WINDOW_TYPE_DOCUMENT)
	{
		switch(g_pcui->GetIntegerPref(PrefsCollectionUI::ActivateTabOnClose))
		{
			default:
			{
				RETURN_IF_ERROR(m_desktop_window_stack.Add(desktop_window));
				break;
			}
			// Put tabs opened from a "parent" at the front of the stack
			case 2:
			{
				OpenURLSetting &setting = static_cast<DocumentDesktopWindow *>(desktop_window)->GetOpenURLSetting();
				if(setting.m_src_commander)
				{
					INT32 pos2 = -1;
					UINT32 n;
					OpWindowCommander *source_commander = setting.m_src_commander;

					// we want to insert this window in the stack after other windows
					// opened from the same tab
					for(n = 0; n < m_desktop_window_stack.GetCount(); n++)
					{
						DesktopWindow *win = m_desktop_window_stack.Get(n);
						if(win && win->GetType() == WINDOW_TYPE_DOCUMENT)
						{
							DocumentDesktopWindow *docwin = static_cast<DocumentDesktopWindow *>(win);

							// check if this window in the stack has the same source window as the current
							// one
							if (source_commander == docwin->GetOpenURLSetting().m_src_commander)
							{
								pos2 = n;
							}
							else if (docwin->GetWindowCommander() == source_commander)
							{
								pos2 = n;
							}
						}
					}
					if(pos2 != -1)
					{
						// we found a window with the same source or the source window itself, so insert after it
						RETURN_IF_ERROR(m_desktop_window_stack.Insert(pos2 + 1, desktop_window));
					}
					else
					{
						RETURN_IF_ERROR(m_desktop_window_stack.Add(desktop_window));
					}
				}
				else
				{
					RETURN_IF_ERROR(m_desktop_window_stack.Add(desktop_window));
				}
			}
			break;
		}
	}
	else
	{
		RETURN_IF_ERROR(m_desktop_window_stack.Add(desktop_window));
	}

#ifdef ENABLE_USAGE_REPORT
	if(g_workspace_report)
	{
		desktop_window->AddListener(g_workspace_report);
	}
#endif

	for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
	{
		m_listeners.Get(i)->OnDesktopWindowAdded(this, desktop_window);
	}

	return OpStatus::OK;
}


/***********************************************************************************
**
**	GetActiveNonHotlistDesktopWindow
**
***********************************************************************************/

DesktopWindow* OpWorkspace::GetActiveNonHotlistDesktopWindow()
{
	INT32 count = m_desktop_window_stack.GetCount();

	for (INT32 i = 0; i < count; i++)
	{
		DesktopWindow* window = m_desktop_window_stack.Get(i);

		if (window->GetType() != WINDOW_TYPE_HOTLIST && window->GetType() != WINDOW_TYPE_PANEL && window->IsActive())
		{
			return window;
		}
	}

	return NULL;
}

/***********************************************************************************
**
**	GetActiveDocumentDesktopWindow
**
***********************************************************************************/

DocumentDesktopWindow* OpWorkspace::GetActiveDocumentDesktopWindow()
{
	DesktopWindow* active = GetActiveNonHotlistDesktopWindow();

	if (active && active->GetType() == WINDOW_TYPE_DOCUMENT)
	{
		return (DocumentDesktopWindow*) active;
	}

	return NULL;
}

/***********************************************************************************
**
**	GetActiveMailDesktopWindow
**
***********************************************************************************/

MailDesktopWindow* OpWorkspace::GetActiveMailDesktopWindow()
{
	DesktopWindow* active = GetActiveNonHotlistDesktopWindow();

	if (active && active->GetType() == WINDOW_TYPE_MAIL_VIEW)
	{
		return (MailDesktopWindow*) active;
	}

	return NULL;
}

/***********************************************************************************
**
**	GetActiveChatDesktopWindow
**
***********************************************************************************/
#ifdef IRC_SUPPORT
ChatDesktopWindow* OpWorkspace::GetActiveChatDesktopWindow()
{
	DesktopWindow* active = GetActiveNonHotlistDesktopWindow();
	if (active && active->GetType() == WINDOW_TYPE_CHAT_ROOM)
	{
		return (ChatDesktopWindow*) active;
	}

	return NULL;
}
#endif
/***********************************************************************************
**
**	GetPosition
**
***********************************************************************************/

INT32 OpWorkspace::GetPosition(DesktopWindow* desktop_window)
{
	if (!m_model_item)
		return -1;

	DesktopWindowCollectionItem& window_item = desktop_window->GetModelItem();

	INT32 pos = 0;
	for (DesktopWindowCollectionItem* child = m_model_item->GetChildItem(); child; child = child->GetSiblingItem())
	{
		// check if we have found the window, or the group the window is in
		if (child == &window_item)
			return pos;
		else if (child == window_item.GetParentItem())
			return pos + 1; // We want the window position to be after the group if reopened

		pos++;
	}

	return -1;
}

/***********************************************************************************
 **
 **	GetPositionInGroup
 **
 ***********************************************************************************/

INT32 OpWorkspace::GetPositionInGroup(DesktopWindow* desktop_window)
{
	DesktopGroupModelItem* current_group = desktop_window->GetParentGroup();

	if (current_group != NULL)
	{
		OpVector<DesktopWindow> group_windows;
		RETURN_VALUE_IF_ERROR(current_group->GetDescendants(group_windows), -1);

		for (UINT32 j = 0; j < group_windows.GetCount(); j++)
		{
			if (group_windows.Get(j)->GetID() == desktop_window->GetID())
			{
				return j;
			}
		}
	}

	return -1;
}

/***********************************************************************************
 **
 **	GetGroup
 **
 ***********************************************************************************/

DesktopGroupModelItem* OpWorkspace::GetGroup(INT32 group_no)
{
	if (group_no <= 0)
	{
		return NULL;
	}

	for (DesktopWindowCollectionItem* child = m_model_item->GetChildItem(); child; child = child->GetSiblingItem())
	{
		if (child->IsContainer() && child->GetID() == group_no)
		{
			return static_cast<DesktopGroupModelItem *>(child);
		}
	}

	return NULL;
}

/***********************************************************************************
**
**	OnDesktopWindowChanged
**
***********************************************************************************/

void OpWorkspace::OnDesktopWindowChanged(DesktopWindow* desktop_window)
{
	for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
	{
		m_listeners.Get(i)->OnDesktopWindowChanged(this, desktop_window);
	}
}

DesktopWindow* OpWorkspace::GetNextWindowToActivate(DesktopWindow* closing_desktop_window)
{
	// if it's not active, don't change the focus
	if(closing_desktop_window != m_active_desktop_window)
	{
		if (!m_active_desktop_window || m_active_desktop_window->IsClosing())
			return NULL;
		else
			return m_active_desktop_window;
	}

	if(g_pcui->GetIntegerPref(PrefsCollectionUI::ActivateTabOnClose) == 1)
	{
		if(closing_desktop_window->GetType() == WINDOW_TYPE_SOURCE)
		{
			// special case for source windows where we always want to return back to the stack order
			return NULL;
		}

		// Get the next or previous tab if possible

		DesktopWindowCollectionItem* items[] = {
			closing_desktop_window->GetModelItem().GetNextLeafItem(),
			closing_desktop_window->GetModelItem().GetPreviousLeafItem()
		};
		for (UINT32 i = 0; i < ARRAY_SIZE(items); ++i)
		{
			DesktopWindow* window = items[i] ? items[i]->GetDesktopWindow() : NULL;
			if (window && !window->IsMinimized() && !window->IsClosing() && window->GetWorkspace() == this)
				return window;
		}
	}
	else if(g_pcui->GetIntegerPref(PrefsCollectionUI::ActivateTabOnClose) == 2 &&
		closing_desktop_window->GetType() == WINDOW_TYPE_DOCUMENT)
	{
		DocumentDesktopWindow* desktop_window = static_cast<DocumentDesktopWindow*>(closing_desktop_window);
		OpWindowCommander* src_commander = desktop_window->GetOpenURLSetting().m_src_commander;
		OpWindowCommander* wnd_commander = desktop_window->GetWindowCommander();

		// Look for the first sibling window that can be activated first.
		// If not found look for the first child window that can be activated.

		DesktopWindow* first_sibling = NULL;
		DesktopWindow* first_child = NULL;

		for (UINT32 i = 0; i < m_desktop_window_stack.GetCount(); ++i)
		{
			DesktopWindow* window = m_desktop_window_stack.Get(i);

			if (window != closing_desktop_window && !window->IsMinimized() && !window->IsClosing() &&
				window->GetType() == WINDOW_TYPE_DOCUMENT)
			{
				OpenURLSetting& window_setting = static_cast<DocumentDesktopWindow *>(window)->GetOpenURLSetting();

				if (src_commander && window_setting.m_src_commander == src_commander)
				{
					first_sibling = window;
					break;
				}
				if (!first_child && wnd_commander && window_setting.m_src_commander == wnd_commander)
				{
					first_child = window;
				}
			}
		}

		return first_sibling ? first_sibling : first_child;
	}

	// caller will have to choose window do activate from the stack
	return NULL;
}

OP_STATUS OpWorkspace::GetDesktopWindows(OpVector<DesktopWindow>& windows, Type type)
{
	if (!m_model_item)
		return OpStatus::OK;

	DesktopWindowCollection* model = m_model_item->GetModel();
	INT32 start = m_model_item->GetIndex() + 1;
	INT32 end = m_model_item->GetIndex() + m_model_item->GetSubtreeSize();

	for (INT32 i = start; i <= end; i++)
	{
		DesktopWindowCollectionItem* item = model->GetItemByIndex(i);
		DesktopWindow* window = item->GetDesktopWindow();

		if (window && window->GetParentWorkspace() == this && (type == WINDOW_TYPE_UNKNOWN || window->GetType() == type))
			RETURN_IF_ERROR(windows.Add(window));
	}

	return OpStatus::OK;
}

UINT32 OpWorkspace::GetItemCount()
{
	if (!m_model_item)
		return 0;

	UINT32 count = 0;
	DesktopWindowCollection* model = m_model_item->GetModel();
	INT32 start = m_model_item->GetIndex() + 1;
	INT32 end = m_model_item->GetIndex() + m_model_item->GetSubtreeSize();

	for (INT32 i = start; i <= end; i++)
	{
		DesktopWindowCollectionItem* item = model->GetItemByIndex(i);
		DesktopWindow* window = item->GetDesktopWindow();

		if (window && window->GetParentWorkspace() == this)
			count++;
	}

	return count;
}

void OpWorkspace::OnGroupCreated(DesktopGroupModelItem& group)
{
	group.AddListener(this);
	DesktopWindowCollectionItem* child = group.GetChildItem();
	for (; child; child = child->GetSiblingItem())
	{
		DesktopWindow* dsk_wnd = child->GetDesktopWindow();
		if (dsk_wnd)
			dsk_wnd->SetParentWorkspace(this, FALSE);
	}
	for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
	{
		m_listeners.Get(i)->OnDesktopGroupCreated(this, group);
	}
}

void OpWorkspace::OnGroupAdded(DesktopGroupModelItem& group)
{
	group.AddListener(this);
	DesktopWindowCollectionItem* child = group.GetChildItem();
	for (; child; child = child->GetSiblingItem())
	{
		DesktopWindow* dsk_wnd = child->GetDesktopWindow();
		if (dsk_wnd)
			dsk_wnd->SetParentWorkspace(this, FALSE);
	}
	for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
	{
		m_listeners.Get(i)->OnDesktopGroupAdded(this, group);
	}
}

void OpWorkspace::OnGroupRemoved(DesktopGroupModelItem& group)
{
	group.RemoveListener(this);
	for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
		m_listeners.Get(i)->OnDesktopGroupRemoved(this, group);
}

/* virtual */void
OpWorkspace::OnGroupDestroyed(DesktopGroupModelItem* group)
{
	OP_ASSERT(group);
	group->RemoveListener(this);
	for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
		m_listeners.Get(i)->OnDesktopGroupDestroyed(this, *group);
}

/***********************************************************************************
**
**	OnDesktopWindowClosing
**
***********************************************************************************/

void OpWorkspace::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	UINT32 i;

	for (i = 0; i < m_listeners.GetCount(); i++)
	{
		m_listeners.Get(i)->OnDesktopWindowClosing(this, desktop_window, user_initiated);
	}
	DesktopWindow *window_to_activate = GetNextWindowToActivate(desktop_window);

	if (desktop_window == m_active_desktop_window)
		m_active_desktop_window = NULL;

	desktop_window->RemoveListener(this);

#ifdef ENABLE_USAGE_REPORT
	if(g_workspace_report)
	{
		desktop_window->RemoveListener(g_workspace_report);
	}
#endif

	m_desktop_window_stack.RemoveByItem(desktop_window);

	for (i = 0; i < m_listeners.GetCount(); i++)
	{
		m_listeners.Get(i)->OnDesktopWindowRemoved(this, desktop_window);
	}

	bool more_to_close = false;
	if(!window_to_activate)
	{
		for (i = 0; i < m_desktop_window_stack.GetCount(); i++)
		{
			DesktopWindow* window = m_desktop_window_stack.Get(i);
			if (window->IsClosing())
			{
				more_to_close = true;
			}
			else if (!window->IsMinimized())
			{
				window_to_activate = window;
				break;
			}
		}
	}

	// activate next non-minimized window
	if(window_to_activate)
	{
		window_to_activate->Activate();
		return;
	}

	if (more_to_close)
		return;

	for (i = 0; i < m_listeners.GetCount(); i++)
	{
		m_listeners.Get(i)->OnWorkspaceEmpty(this, m_desktop_window_stack.GetCount() > 0 );
	}

	SetTabStop(FALSE);

	// only activate the window if we don't allow an empty workspace. See bug #364911.
	if (!g_pcui->GetIntegerPref(PrefsCollectionUI::AllowEmptyWorkspace))
	{
		if (GetParentDesktopWindow())
		{
			GetParentDesktopWindow()->SetFocus(FOCUS_REASON_OTHER);
		}
	}
}

/***********************************************************************************
**
**	OnDesktopWindowParentChanged
**
***********************************************************************************/

void OpWorkspace::OnDesktopWindowParentChanged(DesktopWindow* desktop_window)
{
	if (desktop_window->GetParentWorkspace() != this)
	{
		OnDesktopWindowClosing(desktop_window, FALSE);
	}
}

/***********************************************************************************
**
**	OnDesktopWindowActivated
**
***********************************************************************************/

void OpWorkspace::OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL activate)
{
	if (activate)
	{
		m_active_desktop_window = desktop_window;

		m_desktop_window_stack.RemoveByItem(desktop_window);
		m_desktop_window_stack.Insert(0, desktop_window);

		// NULL check to fix bug #159852
		DesktopWindow* parent = GetParentDesktopWindow();
		if (parent)
		{
			for (INT32 i = 0; i < STATUS_TYPE_TOTAL; i++)
			{
				parent->SetStatusText(desktop_window->GetStatusText((DesktopWindowStatusType) i), (DesktopWindowStatusType) i);
			}
		}

		UpdateHiddenStatus();
	}
	else if (m_active_desktop_window == desktop_window)
	{
		m_active_desktop_window = NULL;
	}

	for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
	{
		m_listeners.Get(i)->OnDesktopWindowActivated(this, desktop_window, activate);
	}
}

/***********************************************************************************
**
**	OnDesktopWindowLowered
**
***********************************************************************************/

void OpWorkspace::OnDesktopWindowLowered(DesktopWindow* desktop_window)
{
	m_desktop_window_stack.RemoveByItem(desktop_window);
	m_desktop_window_stack.Add(desktop_window);
}

/***********************************************************************************
**
**	OnDesktopWindowAttention
**
***********************************************************************************/

void OpWorkspace::OnDesktopWindowAttention(DesktopWindow* desktop_window, BOOL attention)
{
	for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
	{
		m_listeners.Get(i)->OnDesktopWindowAttention(this, desktop_window, attention);
	}
}

/***********************************************************************************
**
**	OnDesktopWindowStatusChanged
**
***********************************************************************************/

void OpWorkspace::OnDesktopWindowStatusChanged(DesktopWindow* desktop_window, DesktopWindowStatusType type)
{
	if (desktop_window == m_active_desktop_window && GetParentDesktopWindow() )
	{
		GetParentDesktopWindow()->SetStatusText(desktop_window->GetStatusText(type), type);
	}
}

/***********************************************************************************
**
**	OnDesktopWindowResized
**
***********************************************************************************/

void OpWorkspace::OnDesktopWindowResized(DesktopWindow* desktop_window, INT32 width, INT32 height)
{
	UpdateHiddenStatus();
}

/***********************************************************************************
**
**	CloseAll
**
***********************************************************************************/

void OpWorkspace::CloseAll(BOOL immediately /*=FALSE*/)
{
	if( GetParentDesktopWindow() )
		GetParentDesktopWindow()->LockUpdate(TRUE);

	// 1. We have to compile a list of the open windows
	// before we start to close because the close
	// operation may open a new window.

	// 2. We have to use Id's because one window may
	// close another one by itself (in theory).

	OpINT32Vector window_id_list;
	GetWindowIdList(window_id_list);

	for (UINT32 i = 0; i < window_id_list.GetCount(); i++)
	{
		DesktopWindow* window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID( window_id_list.Get(i) );
		if( window )
			window->Close(immediately, TRUE, FALSE);
	}

	if( GetParentDesktopWindow() )
		GetParentDesktopWindow()->LockUpdate(FALSE);
}

void OpWorkspace::MaximizeAll()
{
	if( GetParentDesktopWindow() )
		GetParentDesktopWindow()->LockUpdate(TRUE);

#if defined(_UNIX_DESKTOP_)
	// Prevents flicker when maximizing background images. LockUpdate() does not work well.
	// [espen 2006-10-13]
	DesktopWindow* active_window = m_active_desktop_window;
#endif

	OpINT32Vector list;
	GetWindowIdList(list);
	for (INT32 i = list.GetCount() - 1; i >= 0; i--)
	{
		DesktopWindow* desktop_window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID( list.Get(i) );
		if( desktop_window )
		{

#if defined(_UNIX_DESKTOP_)
			SetOpenInBackground( desktop_window != active_window );
#endif

			desktop_window->Maximize();

#if defined(_UNIX_DESKTOP_)
			SetOpenInBackground( FALSE );
#endif

		}
	}

	if( !m_active_desktop_window && list.GetCount() > 0 )
	{
		DesktopWindow* window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID( list.Get(0) );
		if( window )
		{
			window->Activate();
		}
	}

	if( GetParentDesktopWindow() )
		GetParentDesktopWindow()->LockUpdate(FALSE);
}

void OpWorkspace::MinimizeAll()
{
	if( GetParentDesktopWindow() )
		GetParentDesktopWindow()->LockUpdate(TRUE);

	OpINT32Vector list;
	GetWindowIdList(list);
	for (INT32 i = list.GetCount() - 1; i >= 0; i--)
	{
		DesktopWindow* desktop_window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID( list.Get(i) );
		if( desktop_window )
		{
			desktop_window->Minimize();
		}
	}

	if( GetParentDesktopWindow() )
		GetParentDesktopWindow()->LockUpdate(FALSE);
}

void OpWorkspace::RestoreAll()
{
	if( GetParentDesktopWindow() )
		GetParentDesktopWindow()->LockUpdate(TRUE);

	OpINT32Vector list;
	GetWindowIdList(list);
	for (INT32 i = list.GetCount() - 1; i >= 0; i--)
	{
		DesktopWindow* desktop_window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID( list.Get(i) );
		if( desktop_window )
		{
			desktop_window->Restore();
		}
	}

	if( !m_active_desktop_window && list.GetCount() > 0 )
	{
		DesktopWindow* window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID( list.Get(0) );
		if( window )
		{
			window->Activate();
		}
	}

	if( GetParentDesktopWindow() )
		GetParentDesktopWindow()->LockUpdate(FALSE);
}

/***********************************************************************************
**
**	CascadeAll
**
***********************************************************************************/

void OpWorkspace::CascadeAll()
{
	if (m_desktop_window_stack.GetCount() == 0)
		return;

	OpINT32Vector list;
	GetWindowIdList(list);

	if( GetParentDesktopWindow() )
		GetParentDesktopWindow()->LockUpdate(TRUE);

	UINT32 outer_width;
	UINT32 outer_height;
	UINT32 inner_width;
	UINT32 inner_height;

	m_desktop_window_stack.Get(0)->GetOuterSize(outer_width, outer_height);
	m_desktop_window_stack.Get(0)->GetInnerSize(inner_width, inner_height);

	INT32 offset = outer_height - inner_height;

	UINT32 workspace_width;
	UINT32 workspace_height;

	m_window->GetInnerSize(&workspace_width, &workspace_height);

	OpRect rect(0, 0, workspace_width * 3 / 4, workspace_height * 3 / 4);

	for (INT32 i = list.GetCount() - 1; i >= 0; i--)
	{
		DesktopWindow* desktop_window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID( list.Get(i) );

		if( !desktop_window )
			break;

		if (desktop_window->IsMinimized())
			continue;

		desktop_window->Restore();
		desktop_window->SetOuterPos(rect.x, rect.y);
		desktop_window->SetOuterSize(rect.width, rect.height);

		rect.x += offset;
		rect.y += offset;

		if (rect.x + rect.width > (INT32) workspace_width || rect.y + rect.height > (INT32) workspace_height)
		{
			rect.x = 0;
			rect.y = 0;
		}
	}

	if( GetParentDesktopWindow() )
		GetParentDesktopWindow()->LockUpdate(FALSE);
}

/***********************************************************************************
**
**	TileAll
**
***********************************************************************************/

void OpWorkspace::TileAll(BOOL vertically)
{
	INT32 i;

	OpINT32Vector list;
	GetWindowIdList(list);

	INT32 total = list.GetCount();

	INT32 count = 0;

	for (i = list.GetCount() - 1; i >= 0; i--)
	{
		DesktopWindow* desktop_window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID( list.Get(i) );
		if( desktop_window && !desktop_window->IsMinimized())
		{
			desktop_window->Restore();
			count++;
		}
	}

	if (count == 0)
		return;

	if( GetParentDesktopWindow() )
		GetParentDesktopWindow()->LockUpdate(TRUE);

	// prefsManager->GetIntegerPrefDirect(PrefsManager::HotlistTileAndCascade);

	INT32 root = 2;

	while (root * root <= count)
	{
		root++;
	}

	INT32 cols = root - 1;

	INT32 rows = count / cols;
	INT32 mul_rows = count % cols;

	UINT32 workspace_width;
	UINT32 workspace_height;

	m_window->GetInnerSize(&workspace_width, &workspace_height);

	INT32 width;
	INT32 height;

	if (vertically)
	{
		width = workspace_width / rows;
		height = workspace_height / cols;
	}
	else
	{
		width = workspace_width / cols;
		height = workspace_height / rows;
	}

	INT32 first_rows = rows - mul_rows;

	INT32 r;
	INT32 c;

	i = 0;

	for (r = 0; r < first_rows; r++)
	{
		for (c = 0; c < cols; c++)
		{
			for (;i < total; i++)
			{
				DesktopWindow* desktop_window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID( list.Get(i) );
				if( desktop_window && !desktop_window->IsMinimized() )
				{
					desktop_window->SetOuterPos((vertically ? r : c) * width, (vertically ? c : r) * height);
					desktop_window->SetOuterSize(width, height);
					i++;
					break;
				}
			}
		}
	}

	cols++;

	if (vertically)
	{
		height = workspace_height / cols;
	}
	else
	{
		width = workspace_width / cols;
	}

	for (r = first_rows; r < rows; r++)
	{
		for (c = 0; c < cols && g_application->GetDesktopWindowCollection().GetDesktopWindowByID(list.Get(i)); c++)
		{
			for (;i < total; i++)
			{
				DesktopWindow* desktop_window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID(list.Get(i));
				if (desktop_window && !desktop_window->IsMinimized())
				{
					desktop_window->SetOuterPos((vertically ? r : c) * width, (vertically ? c : r) * height);
					desktop_window->SetOuterSize(width, height);
					i++;
					break;
				}
			}
		}
	}

	if( GetParentDesktopWindow() )
		GetParentDesktopWindow()->LockUpdate(FALSE);
}

void OpWorkspace::GetWindowIdList(OpINT32Vector& list)
{
	for (UINT32 i = 0; i < m_desktop_window_stack.GetCount(); i++)
	{
		list.Add(m_desktop_window_stack.Get(i)->GetID());
	}
}

void OpWorkspace::UpdateHiddenStatus()
{
	if (m_desktop_window_stack.GetCount() > 0)
	{
		// Check for hidden windows
		int active_x, active_y;
		unsigned active_w, active_h;
	
		m_desktop_window_stack.Get(0)->GetOpWindow()->GetOuterPos(&active_x, &active_y);
		m_desktop_window_stack.Get(0)->GetOpWindow()->GetOuterSize(&active_w, &active_h);
	
		OpRect activated_rect(active_x, active_y, active_w, active_h);
		OpRect workspace_rect = GetBounds();
	
		// Check current window against all other windows; if a window is not on screen or
		// or if it's smaller than the current window, it is hidden
		for (unsigned int i = 1; i < m_desktop_window_stack.GetCount(); i++)
		{
			int x, y;
			unsigned w, h;
	
			m_desktop_window_stack.Get(i)->GetOpWindow()->GetOuterPos(&x, &y);
			m_desktop_window_stack.Get(i)->GetOpWindow()->GetOuterSize(&w, &h);
	
			OpRect window_rect(x, y, w, h);
	
			if (!window_rect.Intersecting(workspace_rect) || activated_rect.Contains(window_rect))
			{
				m_desktop_window_stack.Get(i)->GetWidgetContainer()->GetRoot()->SetPaintingPending();
				m_desktop_window_stack.Get(i)->SetHidden(TRUE);
			}
			else
			{
				m_desktop_window_stack.Get(i)->SetHidden(FALSE);
			}
		}
		m_desktop_window_stack.Get(0)->SetHidden(FALSE);
	}
}


void OpWorkspace::SetOpenInBackground( BOOL state )
{
	m_window->SetOpenInBackground( state );
}

void OpWorkspace::OnTreeChanged(OpTreeModel* tree_model)
{
	for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
		m_listeners.Get(i)->OnDesktopWindowOrderChanged(this);
}

/* virtual */
void OpWorkspace::OnCollapsedChanged(DesktopGroupModelItem* group, bool collapsed)
{
	OP_ASSERT(group);
	for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
		m_listeners.Get(i)->OnDesktopGroupChanged(this, *group);
}

/* virtual */
void OpWorkspace::OnActiveDesktopWindowChanged(DesktopGroupModelItem* group, DesktopWindow* active)
{
	OP_ASSERT(group);
	for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
		m_listeners.Get(i)->OnDesktopGroupChanged(this, *group);

}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

int OpWorkspace::GetAccessibleChildrenCount()
{
	return m_desktop_window_stack.GetCount()+OpWidget::GetAccessibleChildrenCount();
}

OpAccessibleItem* OpWorkspace::GetAccessibleChild(int child_nr)
{
	INT32 widget_children= OpWidget::GetAccessibleChildrenCount();

	if (child_nr < widget_children)
		return OpWidget::GetAccessibleChild(child_nr);
	else
	{
		return m_desktop_window_stack.Get(child_nr-widget_children);
	}
}

int OpWorkspace::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	int index = OpWidget::GetAccessibleChildIndex(child);
	if (index != Accessibility::NoSuchChild)
		return index;

	index = m_desktop_window_stack.Find(static_cast<DesktopWindow*>(child));
	if (index != -1)
		return index + OpWidget::GetAccessibleChildrenCount();

	return Accessibility::NoSuchChild;
}

OpAccessibleItem* OpWorkspace::GetAccessibleChildOrSelfAt(int x, int y)
{
	OpAccessibleItem* result=OpWidget::GetAccessibleChildOrSelfAt(x,y);

	if (!result)
		return NULL;

	if (result!= this)
		return result;

	OpRect widget_rect = GetScreenRect();

	//goes through all windows to find a child at
	for (unsigned int i = 0;i < m_desktop_window_stack.GetCount();i++)
	{
		INT32 x_w,y_w;
		UINT32 w_w,h_w;

		m_desktop_window_stack.Get(i)->GetOpWindow()->GetOuterPos(&x_w, &y_w);
		m_desktop_window_stack.Get(i)->GetOpWindow()->GetOuterSize(&w_w, &h_w);
		if (OpRect(x_w ,y_w, w_w, h_w).Contains(OpPoint(x-widget_rect.x, y-widget_rect.y)))
		{
			return m_desktop_window_stack.Get(i);
		}
	}

	return this;
}

OpAccessibleItem* OpWorkspace::GetAccessibleFocusedChildOrSelf()
{
	OpAccessibleItem* result;

	if ((result=OpWidget::GetAccessibleFocusedChildOrSelf()) != NULL)
		return result;

	if (m_active_desktop_window)
	{
		return m_active_desktop_window;
	}
	return NULL;
}

#endif //ACCESSIBILITY_EXTENSION_SUPPORT

void OpWorkspace::EnableTransparentSkins(BOOL enable)
{
	if (enable == m_transparent_skins_enabled)
		return;

	for (UINT32 i = 0; i < m_desktop_window_stack.GetCount(); i++)
	{
		if (m_desktop_window_stack.Get(i)->GetType() == WINDOW_TYPE_DOCUMENT)
		{
			static_cast<DocumentDesktopWindow*>(m_desktop_window_stack.Get(i))->EnableTransparentSkin(enable);
		}
	}

	m_transparent_skins_enabled = enable;
}

BOOL OpWorkspace::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	g_application->GetMenuHandler()->ShowPopupMenu("Workspace Menu", PopupPlacement::AnchorAtCursor(), 0, keyboard_invoked);
	return true;
}

void OpWorkspace::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (down && button == MOUSE_BUTTON_1 && nclicks == 2)
		g_input_manager->InvokeAction(OpInputAction::ACTION_NEW_PAGE, 1);
#if defined(_X11_SELECTION_POLICY_)
	else if (down && button == MOUSE_BUTTON_3 && nclicks == 1)
	{
		OpString text;
		g_desktop_clipboard_manager->GetText(text, true);
		if (text.HasContent())
			g_application->GoToPage(text.CStr(), TRUE, FALSE, FALSE, NULL, (URL_CONTEXT_ID)-1, FALSE);
	}
#endif

}

#if defined(_X11_SELECTION_POLICY_)
void OpWorkspace::OnDragMove(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);
	if (drag_object->GetURL())
	{
		drag_object->SetDesktopDropType(DROP_COPY);
	}
}

void OpWorkspace::OnDragDrop(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);

	if (drag_object->GetURL())
	{
		// Activate target browser window so that we open in correct 
		OpVector<DesktopWindow> list;
		g_application->GetDesktopWindowCollection().GetDesktopWindows(OpTypedObject::WINDOW_TYPE_BROWSER, list);
		for (UINT32 i=0; i<list.GetCount(); i++)
		{
			if (list.Get(i)->GetWorkspace() == this)
			{
				list.Get(i)->Activate(TRUE);
				break;
			}
		}

		if (drag_object->GetURLs().GetCount() > 0)
		{
			for (UINT32 i=0; i<drag_object->GetURLs().GetCount(); i++)
			{
				OpenURLSetting setting;
				setting.m_address.Set( *drag_object->GetURLs().Get(i) );
				setting.m_new_window      = NO;
				setting.m_new_page        = YES;
				setting.m_in_background   = MAYBE;
				setting.m_target_window   = 0;
				g_application->OpenURL(setting);
			}
		}
		else
		{
			OpenURLSetting setting;
			setting.m_address.Set( drag_object->GetURL() );
			setting.m_new_window      = NO;
			setting.m_new_page        = YES;
			setting.m_in_background   = MAYBE;
			setting.m_target_window   = 0;
			g_application->OpenURL( setting );
		}
	}
}
#endif
