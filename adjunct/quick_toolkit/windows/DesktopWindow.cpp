/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"
#include "adjunct/quick/dialogs/CustomizeToolbarDialog.h"
#include "adjunct/quick/widgets/OpPagebar.h"
#include "adjunct/quick/widgets/PagebarButton.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/quick/models/DesktopGroupModelItem.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/managers/SleepManager.h"
#include "adjunct/quick/managers/ToolbarManager.h"

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_util/rtl/uidirection.h"
#include "adjunct/desktop_util/sessions/opsession.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/desktop_util/widget_utils/WidgetThumbnailGenerator.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"

#include "modules/libgogi/pi_impl/mde_opview.h"

#include "modules/locale/oplanguagemanager.h"

#include "modules/inputmanager/inputmanager.h"
#include "modules/thumbnails/src/thumbnail.h"
#include "modules/widgets/WidgetContainer.h"

#include "modules/pi/OpWindow.h"
#include "modules/pi/OpView.h"
#include "modules/pi/OpScreenInfo.h"

#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/thumbnails/thumbnailmanager.h"
#include "modules/display/VisDevListeners.h"
#include "modules/dragdrop/dragdrop_manager.h"

#include "modules/scope/src/scope_manager.h"
#include "adjunct/desktop_scope/src/scope_desktop_window_manager.h"

#if defined(_WINDOWS_)
#include "platforms/windows/pi/WindowsOpWindow.h"
#include "platforms/windows/windows_ui/menubar.h"
#endif
#if defined(_MACINTOSH_)
#include "platforms/mac/util/AppleEventUtils.h"
#endif

#ifdef SUPPORT_DATA_SYNC
#include "adjunct/quick/managers/SyncManager.h"
#endif // SUPPORT_DATA_SYNC

/***********************************************************************************
**
**	InternalDesktopWindowListener
**
***********************************************************************************/

class InternalDesktopWindowListener : public OpWindowListener
{
	public:

		friend class DesktopWindow;

		InternalDesktopWindowListener(DesktopWindow* desktopwindow)
			: m_desktop_window(desktopwindow)
			, m_has_been_activated(FALSE)
		{
		}

		void OnResize(UINT32 width, UINT32 height)
		{
			m_desktop_window->m_lock_count++;

			//TODO: m_desktop_window->GetWidgetContainer()->SetPos(0, 0);
			m_desktop_window->GetWidgetContainer()->SetSize(width, height);
			m_desktop_window->OnResize(width, height);

			m_desktop_window->m_lock_count--;

			for (OpListenersIterator iterator(m_desktop_window->m_listeners); m_desktop_window->m_listeners.HasNext(iterator);)
			{
				m_desktop_window->m_listeners.GetNext(iterator)->OnDesktopWindowResized(m_desktop_window, width, height);
			}
		}

		void OnMove()
		{
			m_desktop_window->OnMove();
			m_desktop_window->GetWidgetContainer()->GetRoot()->GenerateOnWindowMoved();
			for (OpListenersIterator iterator(m_desktop_window->m_listeners); m_desktop_window->m_listeners.HasNext(iterator);)
			{
				m_desktop_window->m_listeners.GetNext(iterator)->OnDesktopWindowMoved(m_desktop_window);
			}
		}

		void OnActivate(BOOL active)
		{
			m_desktop_window->m_active = active;

			if (!active)
			{
				g_application->SetToolTipListener(NULL);
			}

			for (OpListenersIterator iterator(m_desktop_window->m_listeners); m_desktop_window->m_listeners.HasNext(iterator);)
			{
				m_desktop_window->m_listeners.GetNext(iterator)->OnDesktopWindowActivated(m_desktop_window, active);
			}
			m_desktop_window->GetModelItem().Change();

			if (!IsNonBlockingDialogWithInactiveParent())
			{
				if (m_desktop_window->RestoreFocusWhenActivated())
				{
					if (active)
					{
						m_desktop_window->RestoreFocus(FOCUS_REASON_ACTIVATE);
					}
					else
					{
						m_desktop_window->ReleaseFocus();
					}
				}

				m_desktop_window->OnActivate(active, active && !m_has_been_activated);
			}

			if (active)
			{
				m_has_been_activated = TRUE;
				DesktopGroupModelItem* group = m_desktop_window->GetParentGroup();
				if (group)
					group->SetActiveDesktopWindow(m_desktop_window);
			}

			m_desktop_window->GetWidgetContainer()->GetRoot()->GenerateOnWindowActivated(active);

			if (active)
			{
				g_input_manager->UpdateAllInputStates(TRUE);
			}

			m_desktop_window->GetWidgetContainer()->UpdateDefaultPushButton();

			m_desktop_window->SetAttention(FALSE);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
			m_desktop_window->AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityStateFocused | Accessibility::kAccessibilityStateInvisible));
#endif
		}

		BOOL OnIsCloseAllowed()
		{
			for (OpListenersIterator iterator(m_desktop_window->m_listeners); m_desktop_window->m_listeners.HasNext(iterator);)
			{
				if (!m_desktop_window->m_listeners.GetNext(iterator)->OnDesktopWindowIsCloseAllowed(m_desktop_window))
					return FALSE;
			}

			return TRUE;
		}

		void OnClose(BOOL user_initiated);

		void OnFocus(BOOL focus)
		{
		}

		void OnShow( BOOL show )
		{
			m_desktop_window->m_showed = show;
			m_desktop_window->OnShow(show);
			if (show)
				m_desktop_window->SyncLayout();
			m_desktop_window->BroadcastDesktopWindowShown(show);
		}

		void OnDropFiles(const OpVector<OpString>& file_names)
		{
			m_desktop_window->OnDropFiles(file_names);
		}

		void OnVisibilityChanged(BOOL vis)
		{
			if (!vis)
				m_desktop_window->GetWidgetContainer()->GetRoot()->SetPaintingPending();
			m_desktop_window->m_visible = vis;
		}

	private:
		void CloseDescendants();
		bool IsNonBlockingDialogWithInactiveParent()
		{
			return m_desktop_window->GetType() == OpTypedObject::DIALOG_TYPE &&
			       !m_desktop_window->IsModalDialog() &&
			       m_desktop_window->GetParentDesktopWindow() &&
			       !m_desktop_window->GetParentDesktopWindow()->IsActive();
		}

		DesktopWindow*	m_desktop_window;
		BOOL			m_has_been_activated;
};


void InternalDesktopWindowListener::CloseDescendants()
{
	OpVector<DesktopWindow> descendants;
	RETURN_VOID_IF_ERROR(m_desktop_window->GetModelItem().GetDescendants(descendants));

	// Mark all descendants as closing in advance.  This flag may have impact
	// on the way a DesktopWindow performs its clean-up.
	// Even if a descendant is not closed immediately below -- because it's
	// VisibleInWindowList() -- it will have been closed by the time
	// InternalDesktopWindowListener::OnClose() returns.
	for (unsigned i = descendants.GetCount(); i > 0; --i)
		descendants.Get(i - 1)->m_is_closing = TRUE;

	for (unsigned i = descendants.GetCount(); i > 0; --i)
	{
		DesktopWindow& descendant = *descendants.Get(i - 1);

		// Close children that are not associated with the window list (eg. those
		// that aren't part of a session, so that session sections will not be
		// overwritten with empty data)
		if (descendant.VisibleInWindowList())
			continue;

		descendant.Close(TRUE);
	}
}

void InternalDesktopWindowListener::OnClose(BOOL user_initiated)
{
    if(m_desktop_window->PreClose())
    {
       return;
	}

	m_desktop_window->m_active = FALSE;

	// Close all child DesktopWindows first.
	//
	// Failure to do so may lead to stale pointer usage: as part of destroying
	// the parent DesktopWindow, the underlying platform widget tree is brought
	// down as well, and this includes child windows (dialogs, drop-down
	// pop-ups, etc.).
	CloseDescendants();

	const char* window_name = m_desktop_window->GetWindowName();

	if (window_name && m_desktop_window->m_save_placement_on_close)
	{
		OpRect rect;
		OpWindow::State state;
		WinSizeState old_type_state = NORMAL;

		m_desktop_window->GetOpWindow()->GetDesktopPlacement(rect, state);

		switch (state)
		{
			case OpWindow::RESTORED: old_type_state = NORMAL; break;
			case OpWindow::MINIMIZED: old_type_state = ICONIC; break;
			case OpWindow::MAXIMIZED: old_type_state = MAXIMIZED; break;
			case OpWindow::FULLSCREEN: old_type_state = MAXIMIZED; break;
		}

		if (old_type_state != ICONIC)
		{
			OpString win_name;
			win_name.Set(window_name);
			TRAPD(err, g_pcui->WriteWindowInfoL(win_name, rect, old_type_state));
		}
	}

	m_desktop_window->BroadcastDesktopWindowClosing(user_initiated);
	m_desktop_window->OnClose(user_initiated);

	OpWorkspace* workspace = m_desktop_window->GetParentWorkspace();
	if (g_drag_manager->IsDragging())
	{
		DesktopDragObject* drag_object = (DesktopDragObject*)g_drag_manager->GetDragObject();
		if (drag_object && (drag_object->GetType() == OpTypedObject::DRAG_TYPE_WINDOW || drag_object->GetType() == OpTypedObject::DRAG_TYPE_THUMBNAIL))
		{
			for (INT32 i = 0; i < drag_object->GetIDCount(); i++)
			{
				if (drag_object->GetID(i) == m_desktop_window->GetID())
				{
					g_drag_manager->StopDrag();
					break;
				}
			}
		}
		else if (workspace)
		{
			// Testing for workspace prevents a drag from being canceled when Ctrl+TAB'ing
			// using the page cycler window. We should probably restrict calling StopDrag()
			// even more.
		 	g_drag_manager->StopDrag();
		}
	}

	OP_DELETE(m_desktop_window);
	// Dont reference members after this delete, the object is destroyed.

	if (user_initiated && workspace)
	{
		if (workspace->GetWidgetContainer() && workspace->GetDesktopWindowCount() == 0)
			g_input_manager->UpdateAllInputStates(TRUE);

		if (g_pcui->GetIntegerPref(PrefsCollectionUI::MaximizeNewWindowsWhenAppropriate) == 4)
			workspace->TileAll(TRUE);
	}
}
/***********************************************************************************
**
**	DesktopWindow
**
***********************************************************************************/

DesktopWindow::DesktopWindow()
	: init_status(OpStatus::OK)
	, m_window(NULL)
	, m_window_listener(NULL)
	, m_parent_workspace(NULL)
	, m_old_state(OpWindow::RESTORED)
	, m_fullscreen(FALSE)
	, m_save_placement_on_close(TRUE)
	, m_active(FALSE)
	, m_visible(TRUE)	///< OnVisibilityChanged is guaranteed to be called for child windows at least once, so the value
						///< doesn't matter for those. But top level windows won't get it so make those always visible.
	, m_showed(FALSE)
	, m_attention(FALSE)
	, m_locked_by_user(FALSE)
	, m_windowclosingblocked(0)
	, m_is_closing(FALSE)
	, m_id(GetUniqueID())
	, m_disabled_count(0)
	, m_lock_count(0)
	, m_widget_container(NULL)
	, m_delayed_update_flags(0)
	, m_uses_fixed_thumbnail_image(FALSE)
	, m_model_item(this)
{
	SetParentInputContext(g_application->GetInputContext());
	m_image.SetListener(this);

	// make sure we don't update the icon too often as it has a significant performance hit on ibench - pettern 25.08.2008
	m_delayed_trigger.SetDelayedTriggerListener(this);
	m_delayed_trigger.SetTriggerDelay(0, 300);
	m_delayed_propagate_to_native_window = FALSE;
}

/***********************************************************************************
**
**	Init
**
***********************************************************************************/

OP_STATUS DesktopWindow::Init(OpWindow::Style style, UINT32 effects)
{
	return Init(style, NULL, NULL, NULL, effects);
}

OP_STATUS DesktopWindow::Init(OpWindow::Style style, DesktopWindow* parent_window, UINT32 effects)
{
	return Init(style, NULL, parent_window, NULL, effects);
}

OP_STATUS DesktopWindow::Init(OpWindow::Style style, OpWorkspace* parent_workspace, UINT32 effects)
{
	return Init(style, NULL, NULL, parent_workspace, effects);
}

OP_STATUS DesktopWindow::Init(OpWindow::Style style, OpWindow* parent_opwindow, UINT32 effects)
{
	return Init(style, parent_opwindow, NULL, NULL, effects);
}

OP_STATUS DesktopWindow::Init(OpWindow::Style style, DesktopWindow* parent_window, OpWindow* parent_opwindow, UINT32 effects)
{
	return Init(style, parent_opwindow, parent_window, NULL, effects);
}

OP_STATUS DesktopWindow::Init(OpWindow::Style style, OpWindow* parent_opwindow, DesktopWindow* parent_window, OpWorkspace* parent_workspace, UINT32 effects)
{
	RETURN_IF_ERROR(DesktopOpWindow::Create(&m_window));

	m_window->SetDesktopWindow(this);

	if (parent_opwindow == NULL && parent_window != NULL)
		parent_opwindow = parent_window->GetOpWindow();
	if (parent_opwindow == NULL && parent_workspace != NULL)
		parent_opwindow = parent_workspace->GetOpWindow();
	RETURN_IF_ERROR(m_window->Init(style, GetOpWindowType(), parent_opwindow, NULL, NULL, effects));
	if (style == OpWindow::STYLE_NOTIFIER)
	{
		g_application->SetPopupDesktopWindow(NULL); //Close existing one
		g_application->SetPopupDesktopWindow(this);
	}

	m_style = style;
	m_parent_workspace = parent_workspace;

	m_window_listener = OP_NEW(InternalDesktopWindowListener, (this));
	if (m_window_listener == NULL)
		return OpStatus::ERR_NO_MEMORY;
	m_window->SetWindowListener(m_window_listener);

	RETURN_IF_ERROR(g_application->AddSettingsListener(this));

	// Init WidgetContainer
	m_widget_container = OP_NEW(WidgetContainer, (this));
	if (m_widget_container == NULL)
		return OpStatus::ERR_NO_MEMORY;

	m_widget_container->SetParentInputContext(this);

	RETURN_IF_ERROR(m_widget_container->Init(OpRect(0,0,0,0), m_window));

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	m_widget_container->GetRoot()->SetAccessibleParent(this);
#endif

	m_widget_container->SetWidgetListener(this);
	m_widget_container->SetFocusable(TRUE);

	m_widget_container->GetRoot()->SetRTL(UiDirection::Get() == UiDirection::RTL);

	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_CLOSE_AUTOCOMPL_POPUP, (MH_PARAM_1)this));

	m_skin.Set(GetWindowName());
	m_skin.Append(" Skin");
	SetSkin(m_skin.CStr(), GetFallbackSkin());

	RETURN_IF_ERROR(g_application->AddDesktopWindow(this, parent_window));

	if (m_parent_workspace)
		RETURN_IF_ERROR(m_parent_workspace->AddDesktopWindow(this));

	Relayout();

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	m_window->SetAccessibleItem(this);
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
#if defined(_MACINTOSH_)
	InformDesktopWindowCreated(this);
#endif
	
	// Add the scope listener for each window
	if (g_scope_manager->desktop_window_manager)
		AddListener(g_scope_manager->desktop_window_manager);
	
	return OpStatus::OK;
}

OP_STATUS DesktopWindow::InitObject()
{
	// null check, because none of OnSessionReadL implementations does it
	RETURN_VALUE_IF_NULL(GetSessionWindow(), OpStatus::ERR);
	TRAPD(err, OnSessionReadL(GetSessionWindow()));
	return err;
}

void DesktopWindow::ResetWindowListener()
{
    m_window->SetWindowListener(m_window_listener);
}

/***********************************************************************************
**
**	~DesktopWindow
**
***********************************************************************************/

DesktopWindow::~DesktopWindow()
{
	if (IsModalDialog())
	{
		DesktopWindowCollectionItem* parent = GetModelItem().GetParentItem();
		if (parent != NULL && parent->GetDesktopWindow() != NULL)
			parent->GetDesktopWindow()->Enable(TRUE);
	}

	if (g_application)
	{
		OpStatus::Ignore(g_application->RemoveDesktopWindow(this));
		if (g_application->GetPopupDesktopWindow() == this)
		{
			g_application->ResetPopupDesktopWindow();
		}
	}

	OP_ASSERT(m_dialogs.GetCount() == 0);

	m_model_item.Remove();

	g_main_message_handler->UnsetCallBacks(this);
	if (m_window)
		m_window->SetWindowListener(NULL);

#if defined(_MACINTOSH_)
	InformDesktopWindowRemoved(this);
#endif

	OP_DELETE(m_widget_container);
	OP_DELETE(m_window);
	OP_DELETE(m_window_listener);

}

/***********************************************************************************
**
**	AddDialog
**
***********************************************************************************/

void DesktopWindow::AddDialog(Dialog* dialog)
{
	m_dialogs.Add(dialog);
}

/***********************************************************************************
**
**	RemoveDialog
**
***********************************************************************************/

void DesktopWindow::RemoveDialog(Dialog* dialog)
{
	m_dialogs.RemoveByItem(dialog);
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL DesktopWindow::OnInputAction(OpInputAction* action)
{
	// Window actions should always only apply to toplevel
	// Page actions should always only apply to workspace windows, unless action data is 1 or 2

	if(ToolbarManager::GetInstance()->HandleAction(GetWidgetContainer()->GetRoot(), action))
		return TRUE;

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_CLOSE_PAGE:
				case OpInputAction::ACTION_CLOSE_CLICKED_PAGE:
				{
					child_action->SetEnabled((GetParentWorkspace() || child_action->GetActionData()) && IsClosableByUser());
					return TRUE;
				}
				case OpInputAction::ACTION_MAXIMIZE_PAGE:
				{
					child_action->SetSelected(IsActive());
					return TRUE;
				}
				case OpInputAction::ACTION_LOCK_PAGE:
				{
					child_action->SetSelected(IsLockedByUser() ? TRUE : FALSE);
					return TRUE;
				}
				case OpInputAction::ACTION_UNLOCK_PAGE:
				{
					child_action->SetSelected(IsLockedByUser() ? FALSE : TRUE);
					return TRUE;
				}

				case OpInputAction::ACTION_TOGGLE_TAB_THUMBNAILS:
				case OpInputAction::ACTION_SET_ALIGNMENT:
				case OpInputAction::ACTION_SET_AUTO_ALIGNMENT:
				case OpInputAction::ACTION_SET_COLLAPSE:
				case OpInputAction::ACTION_SET_WRAPPING:
				case OpInputAction::ACTION_SET_BUTTON_STYLE:
				case OpInputAction::ACTION_ENABLE_LARGE_IMAGES:
				case OpInputAction::ACTION_DISABLE_LARGE_IMAGES:
				case OpInputAction::ACTION_RESTORE_TO_DEFAULTS:
				case OpInputAction::ACTION_CUSTOMIZE_TOOLBARS:
				{
					OpString8 name;
					name.Set(child_action->GetActionDataString());

					OpWidget* widget = GetWidgetByName(name.CStr());

					if (!widget)
						return FALSE;

					return widget->OnInputAction(action);
				}

				case OpInputAction::ACTION_MINIMIZE_WINDOW:
				{
					if (!GetParentWorkspace())
					{
						OpRect rect;
						OpWindow::State state;

						m_window->GetDesktopPlacement(rect, state);
						child_action->SetEnabled(state != OpWindow::MINIMIZED);
						return TRUE;
					}
					return FALSE;
				}

				case OpInputAction::ACTION_DETACH_PAGE:
				{
					child_action->SetEnabled(GetType() != WINDOW_TYPE_BROWSER && GetType() != WINDOW_TYPE_HOTLIST);
					return TRUE;
				}
				case OpInputAction::ACTION_CREATE_SEARCH:
				{
					child_action->SetEnabled(g_searchEngineManager->HasLoadedConfig() && WindowCommanderProxy::HasSearchForm(GetWindowCommander()));
					return TRUE;
				}
				case OpInputAction::ACTION_INSPECT_ELEMENT:
				{
					child_action->SetEnabled(GetType() == WINDOW_TYPE_DOCUMENT);
					return TRUE;
				}
			}
			return FALSE;
		}

		case OpInputAction::ACTION_TOGGLE_TAB_THUMBNAILS:
		case OpInputAction::ACTION_SET_ALIGNMENT:
		case OpInputAction::ACTION_SET_AUTO_ALIGNMENT:
		case OpInputAction::ACTION_SET_COLLAPSE:
		case OpInputAction::ACTION_SET_WRAPPING:
		case OpInputAction::ACTION_SET_BUTTON_STYLE:
		case OpInputAction::ACTION_ENABLE_LARGE_IMAGES:
		case OpInputAction::ACTION_DISABLE_LARGE_IMAGES:
		case OpInputAction::ACTION_CUSTOMIZE_TOOLBARS:
		{
			OpString8 name;
			name.Set(action->GetActionDataString());

			OpWidget* widget = GetWidgetByName(name.CStr());

			if (!widget)
				return FALSE;

			return widget->OnInputAction(action);
		}
		case OpInputAction::ACTION_RESTORE_TO_DEFAULTS:
		{
			OpString8 name;
			
			name.Set(action->GetActionDataString());
			
			OpWidget* widget = GetWidgetByName(name.CStr());
			
			if (widget)
			{
				return widget->OnInputAction(action);
			}
			else
			{
				if(action->IsActionDataStringEqualTo(UNI_L("selected toolbar")))
				{
					OpInputContext *context = action->GetFirstInputContext();
					
					if(!context)
					{
						return FALSE;
					}
					if(context->GetType() == WIDGET_TYPE_BUTTON)
					{
						OpButton *button = static_cast<OpButton *>(context);
						
						if(button->GetParent() && button->GetParent()->GetType() == WIDGET_TYPE_TOOLBAR)
						{
							widget = button->GetParent();
						}
						else
						{
							return FALSE;
						}
					}
					else if(context->GetType() == WIDGET_TYPE_TOOLBAR)
					{
						widget = static_cast<OpWidget *>(context);
					}
				}
				else
				{
					return FALSE;
				}
			}
			break;
		}
		case OpInputAction::ACTION_MAXIMIZE_WINDOW:
		case OpInputAction::ACTION_MINIMIZE_WINDOW:
		case OpInputAction::ACTION_RESTORE_WINDOW:
		{
			if (GetParentWorkspace())
				return FALSE;
			break;
		}

		case OpInputAction::ACTION_CLOSE_WINDOW:
		{
			if (GetParentWorkspace() || action->GetActionData())
				return FALSE;
			break;
		}

		case OpInputAction::ACTION_LOCK_PAGE:
		case OpInputAction::ACTION_UNLOCK_PAGE:
		{
			OpPagebar* pagebar = NULL;

			SetLockedByUser(IsLockedByUser() ? FALSE : TRUE);
			g_input_manager->UpdateAllInputStates();

			pagebar = (OpPagebar *)GetWidgetByType(OpTypedObject::WIDGET_TYPE_PAGEBAR);
			if(pagebar)
			{
				INT32 pos = pagebar->FindWidgetByUserData(this);
				if (pos >= 0)
				{
					OpWidget *button = pagebar->GetWidget(pos);
					if(button->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
					{
						pagebar->OnLockedByUser(static_cast<PagebarButton *>(button), IsLockedByUser());
					}
					else
					{
						button->Relayout(TRUE, TRUE);
					}
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_MAXIMIZE_PAGE:
		case OpInputAction::ACTION_MINIMIZE_PAGE:
		case OpInputAction::ACTION_RESTORE_PAGE:
		case OpInputAction::ACTION_CLOSE_PAGE:
		case OpInputAction::ACTION_CLOSE_CLICKED_PAGE:
		{
			OpWorkspace* workspace = GetParentWorkspace();

			if (action->GetActionData() == 1)
			{
				if (g_application->IsSDI())
				{
					// if in SDI and this window has a parent workspace,
					// ignore action, unless the action is Close and
					// there are more than one left in workspace

					if (workspace && (action->GetAction() != OpInputAction::ACTION_CLOSE_PAGE || workspace->GetDesktopWindowCount() == 1))
					{
						return FALSE;
					}
					else
					{
						// we need to check if the tab is locked, in which case we cannot close the window
						if(action->GetAction() == OpInputAction::ACTION_CLOSE_PAGE && GetWorkspace())
						{
							INT32 cnt;

							for(cnt = 0; cnt < GetWorkspace()->GetDesktopWindowCount(); cnt++)
							{
								DesktopWindow *window = GetWorkspace()->GetDesktopWindowFromStack(cnt);

								if(window && !window->IsClosableByUser())
								{
									return FALSE;
								}
							}
						}
					}
				}
				else
				{
					// if in MDI and this window is toplevel and has a workspace, ignore

					if (!workspace && GetWorkspace())
					{
						return FALSE;
					}
				}
			}
			else if (action->GetActionData() == 2)
			{
				// when action data is 2, always affect page or window, even if action is page
				break;
			}
			else
			{
				// when action data is 0, only affect pages

				if (!workspace)
					return FALSE;
			}
			break;
		}
	}

	// Don't allow mouse gesture over dialogs
	if (IsDialog() && action->GetActionMethod() == OpInputAction::METHOD_MOUSE)
	{
		switch (action->GetAction())
		{
			case OpInputAction::ACTION_MAXIMIZE_PAGE:
			case OpInputAction::ACTION_MAXIMIZE_WINDOW:
			case OpInputAction::ACTION_MINIMIZE_PAGE:
			case OpInputAction::ACTION_MINIMIZE_WINDOW:
			case OpInputAction::ACTION_RESTORE_PAGE:
			case OpInputAction::ACTION_RESTORE_WINDOW:
			case OpInputAction::ACTION_DETACH_PAGE:
			{
				return TRUE;
			}
		}
	}

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_MAXIMIZE_PAGE:
		case OpInputAction::ACTION_MAXIMIZE_WINDOW:
		{
			OpRect rect;
			OpWindow::State state;

			m_window->GetDesktopPlacement(rect, state);

			if (state != OpWindow::MAXIMIZED)
			{
				Maximize();
				return TRUE;
			}
			return FALSE;
		}
		case OpInputAction::ACTION_MINIMIZE_PAGE:
		case OpInputAction::ACTION_MINIMIZE_WINDOW:
		{
			OpRect rect;
			OpWindow::State state;

			m_window->GetDesktopPlacement(rect, state);

			if (state != OpWindow::MINIMIZED)
			{
				Minimize();
				return TRUE;
			}
			return FALSE;
		}
		case OpInputAction::ACTION_RESTORE_PAGE:
		case OpInputAction::ACTION_RESTORE_WINDOW:
		{
			OpRect rect;
			OpWindow::State state;

			m_window->GetDesktopPlacement(rect, state);

			if (state != OpWindow::RESTORED)
			{
				Restore();
				return TRUE;
			}
			return FALSE;
		}


		case OpInputAction::ACTION_DETACH_PAGE:
		{
			SetParentWorkspace(NULL);
			return TRUE;
		}

		case OpInputAction::ACTION_CLOSE_PAGE:
		case OpInputAction::ACTION_CLOSE_CLICKED_PAGE:
		case OpInputAction::ACTION_CLOSE_WINDOW:
		{
			// Work around for DSK-337037
			static_cast<DesktopOpSystemInfo*>(g_op_system_info)->CancelNestedMessageLoop();

			Close(FALSE, TRUE, FALSE);
			return TRUE;
		}

		case OpInputAction::ACTION_GET_TYPED_OBJECT:
		{
			if ((action->GetActionData() == WINDOW_TYPE_DESKTOP_WITH_BROWSER_VIEW && GetBrowserView())
				|| action->GetActionData() == WINDOW_TYPE_DESKTOP)
			{
				action->SetActionObject(this);
				return TRUE;
			}
			return FALSE;
		}
	}

	return FALSE;
}

PagebarButton* DesktopWindow::GetPagebarButton()
{
	PagebarButton* pagebar_button = NULL;
	OpPagebar* pagebar = static_cast<OpPagebar*>(GetWidgetByType(OpTypedObject::WIDGET_TYPE_PAGEBAR));
	if (pagebar)
	{
		INT32 pos = pagebar->FindWidgetByUserData(this);
		if (pos >= 0)
		{
			OpWidget* button = pagebar->GetWidget(pos);
			OP_ASSERT(button->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON));
			pagebar_button = static_cast<PagebarButton*>(button);
		}
	}
	return pagebar_button;
}

/***********************************************************************************
**
**	OnSettingsChanged
**
***********************************************************************************/

void DesktopWindow::OnSettingsChanged(DesktopSettings* settings)
{
	if (settings->IsChanged(SETTINGS_SKIN))
	{
		UpdateSkin();
	}
	if (settings->IsChanged(SETTINGS_LANGUAGE))
	{
		UpdateLanguage();
		GetOpWindow()->UpdateLanguage();
	}
	if (settings->IsChanged(SETTINGS_UI_FONTS))
	{
		GetOpWindow()->UpdateFont();
	}
	if (settings->IsChanged(SETTINGS_MENU_SETUP))
	{
		GetOpWindow()->UpdateMenuBar();
	}

	GetOpWindow()->Redraw();
	GetWidgetContainer()->GetRoot()->GenerateOnSettingsChanged(settings);
}

void DesktopWindow::Close(BOOL immediately, BOOL user_initiated, BOOL force)
{
	if (!force && !IsClosableByUser() && user_initiated)
		return;

	m_is_closing = TRUE;

	if (immediately)
	{
		if (m_window)	// may be NULL due to OOM
		{
			m_window->Close(user_initiated);
		}
	}
	else
	{
		// Post a message that we should close the window very soon.
		g_main_message_handler->RemoveDelayedMessage(MSG_CLOSE_AUTOCOMPL_POPUP, (MH_PARAM_1)this, (MH_PARAM_2)user_initiated);
		g_main_message_handler->PostMessage(MSG_CLOSE_AUTOCOMPL_POPUP, (MH_PARAM_1)this, (MH_PARAM_2)user_initiated);
	}
}

void DesktopWindow::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_CLOSE_AUTOCOMPL_POPUP)
		Close(TRUE, (BOOL) par2);
}

void DesktopWindow::Relayout()
{
	//Relayout all widgets but don't invalidate the whole window. Widgets will invalidate themselves
	//if needed.
    if (m_widget_container)
    	m_widget_container->GetRoot()->Relayout(TRUE, FALSE);
}

void DesktopWindow::SyncLayout()
{
    if (m_widget_container)
    	m_widget_container->GetRoot()->SyncLayout();
}

void DesktopWindow::OnRelayout(OpWidget* widget)
{
	if (widget == m_widget_container->GetRoot())
	{
		OnRelayout();
	}
	else
	{
		Relayout();
	}
}

void DesktopWindow::OnLayout(OpWidget* widget)
{
	if (widget == m_widget_container->GetRoot())
	{
		OnLayout();
	}
}

void DesktopWindow::OnLayoutAfterChildren(OpWidget* widget)
{
	if (widget == m_widget_container->GetRoot())
    {
		OnLayoutAfterChildren();
	}
}

void DesktopWindow::Show(BOOL show, const OpRect* preferred_rect, OpWindow::State preferred_state, BOOL behind, BOOL force_preferred_rect, BOOL inner)
{
	BOOL is_dialog = ((GetType() >= DIALOG_TYPE) && (GetType() < DIALOG_TYPE_LAST));

	if (show)
	{
		g_application->ShowOpera(TRUE);
	}

	if (!show)
	{
		m_window->Show(FALSE);
		return;
	}
	else if (!is_dialog && m_style != OpWindow::STYLE_DESKTOP)
	{
		//generic window: just show it at the desired position and size
		if (preferred_rect)
		{
			SetOuterSize(preferred_rect->width, preferred_rect->height);
			SetOuterPos(preferred_rect->x, preferred_rect->y);
		}

		m_window->Show(TRUE);
		return;
	}

	OpRect rect(0, 0, 0, 0);
	WinSizeState old_state_type = NORMAL;
	const char* window_name = GetWindowName();

	if (window_name)
	{
		OpString win_name;
		win_name.Set(window_name);
		g_pcui->GetWindowInfo(win_name, rect, old_state_type);
	}

	// policy enum soon?
	// 0 remeber size
	// 1 always maximize
	// 2 always maximize, including popups
	// 3 always restore
	// 4 tile all

	int policy = g_pcui->GetIntegerPref(PrefsCollectionUI::MaximizeNewWindowsWhenAppropriate);

	// if policy is 1 and rect is bigger than available workspace, then bump policy to 2

	if (policy == 1 && m_parent_workspace && preferred_rect && (m_parent_workspace->GetWidth() < preferred_rect->width || m_parent_workspace->GetHeight() < preferred_rect->height))
	{
		policy = 2;
	}

	BOOL tile = FALSE;

	if (m_parent_workspace && policy == 4)
	{
		tile = TRUE;
		preferred_state = OpWindow::RESTORED;
		rect.x = 0;
		rect.y = 0;
		rect.width = 1;
		rect.height = 1;
		force_preferred_rect = FALSE;
	}
	else if (!force_preferred_rect || (GetType() == WINDOW_TYPE_DOCUMENT && policy == 2 && m_parent_workspace))
	{
		int sdi = g_application->IsSDI();

		if ((GetType() == WINDOW_TYPE_DOCUMENT || GetType() == WINDOW_TYPE_SOURCE) && m_parent_workspace && (policy || sdi))
		{
			if (policy == 1 || policy == 2 || sdi)
			{
				preferred_state = OpWindow::MAXIMIZED;
			}
			else
			{
				preferred_state = OpWindow::RESTORED;
				rect.x = 16;
				rect.y = 16;
				rect.width = m_parent_workspace->GetWidth() * 3 / 4;
				rect.height = m_parent_workspace->GetHeight() * 3 / 4;
			}
		}
		else
		{
			switch (old_state_type)
			{
				case NORMAL: preferred_state = OpWindow::RESTORED; break;
				case ICONIC: preferred_state = OpWindow::MINIMIZED; break;
				case MAXIMIZED: preferred_state = OpWindow::MAXIMIZED; break;
#ifdef RESTORE_MAC_FULLSCREEN_ON_STARTUP
				case FULLSCREEN: preferred_state = OpWindow::FULLSCREEN; break;
#endif // RESTORE_MAC_FULLSCREEN_ON_STARTUP
			}
			if (m_style == OpWindow::STYLE_DESKTOP)
			{
				OpWorkspace* workspace = GetParentWorkspace();
				DesktopWindow* active_window = workspace ? workspace->GetActiveDesktopWindow() : g_application->GetActiveDesktopWindow();

				// if desktop style window is of same type as current active one, place restored pos relative to it (cascade)

				if (active_window && active_window->GetType() == GetType()
#ifdef DEVTOOLS_INTEGRATED_WINDOW
					// for devtools we don't want cascade, just use saved position
					&& !IsDevToolsOnly()
#endif
					)
				{
					OpRect r;
					OpWindow::State state;
					active_window->GetOpWindow()->GetDesktopPlacement(r, state);

					rect.x = r.x + 20;
					rect.y = r.y + 20;
				}
			}
		}
	}

	BOOL has_saved_pos = rect.width != 0;

	if (preferred_rect && (force_preferred_rect || !has_saved_pos))
	{
		rect.x = preferred_rect->x != DEFAULT_SIZEPOS ? preferred_rect->x : 200;
		rect.y = preferred_rect->y != DEFAULT_SIZEPOS ? preferred_rect->y : 200;
		rect.width = preferred_rect->width != DEFAULT_SIZEPOS ? preferred_rect->width : 400;
		rect.height = preferred_rect->height != DEFAULT_SIZEPOS ? preferred_rect->height : 300;
	}
	else if (!has_saved_pos)
	{
		if (GetType() == WINDOW_TYPE_PANEL && !m_parent_workspace)
		{
			rect.Set(100,100,700,500);
		}
		else
		{
			OpScreenProperties properties;
			g_op_screen_info->GetProperties(&properties, GetOpWindow());
#if defined(_MACINTOSH_)
			rect.Set(10,30,1000, properties.height - 40);
#elif defined(_UNIX_DESKTOP_)
			rect = properties.workspace_rect.InsetBy(50);
#elif defined(WIN32)
			rect.Set(20, 20, MIN(950, GetSystemMetrics(SM_CXMAXIMIZED) - 40), GetSystemMetrics(SM_CYMAXIMIZED) - 40);
#else
			rect.Set(100,100,700,500);
#endif

			if (m_style == OpWindow::STYLE_DESKTOP
#ifdef WIN32 // FIXME: We should probably not maximize on any platform! Needs to be modified above, and tested though!
				&& GetType() != WINDOW_TYPE_BROWSER
#endif
				)
			{
				preferred_state = OpWindow::MAXIMIZED;
			}
		}
	}
	else
	{
		inner = FALSE;
	}

	// let the has_saved_pos be valid only if the opening rect intersect with screen of parent window

	if (has_saved_pos && is_dialog)
	{
		DesktopWindow* dw = GetParentDesktopWindow();

		if (dw)
		{
			OpRect screen_rect;
			dw->GetScreenRect(screen_rect);
			has_saved_pos = screen_rect.Intersecting(rect);
		}
	}

	BOOL center = FALSE; // do not force centering of dialogs on default

	// center dialogs with no saved pos;
	// modal dialogs with a parent should always center to the parent
	center = (!has_saved_pos && !force_preferred_rect && is_dialog) || (GetParentWindow() && IsModalDialog());

	// If this a modal dialog we want to cascade from the parent then adjust it now
	if (GetParentWindow() && IsModalDialog() && IsCascaded())
	{
		// Get the parent's placement
		OpRect r;
		OpWindow::State state;
		GetParentWindow()->GetDesktopPlacement(r, state);

		rect.x = r.x + 20;
		rect.y = r.y + 20;
		center = FALSE;
	}

	if (tile)
	{
		m_parent_workspace->GetParentDesktopWindow()->LockUpdate(TRUE);
	}

	m_old_state = preferred_state;
	if (IsModalDialog())
	{
		DesktopWindowCollectionItem* parent = GetModelItem().GetParentItem();
		if (parent != NULL && parent->GetDesktopWindow() != NULL)
			parent->GetDesktopWindow()->Enable(FALSE);
	}

	m_window->SetDesktopPlacement(rect, preferred_state, inner, behind, center);

	// IMPORTANT: No code for dialogs after this
	//!!!Below lines could potentially causes opera to crash on Windows!!!
	if (tile)
	{
		m_parent_workspace->TileAll(TRUE);
		m_parent_workspace->GetParentDesktopWindow()->LockUpdate(FALSE);
	}
}

void DesktopWindow::SetStatusText( const uni_char* text, DesktopWindowStatusType type )
{

	if (type < 0 || type >= STATUS_TYPE_TOTAL)
		return;

	if (m_status_text[type].Compare(text) == 0)
		return;

	m_status_text[type].Set( text );
	BroadcastDesktopWindowStatusChanged(type);
}

const uni_char*	 DesktopWindow::GetStatusText( DesktopWindowStatusType type  )
{
	if (type < 0 || type >= STATUS_TYPE_TOTAL)
		return NULL;

	return m_status_text[type].CStr();
}

void DesktopWindow::SetUnderlyingStatusText( const uni_char* text, DesktopWindowStatusType type )
{
	if (type < 0 || type >= STATUS_TYPE_TOTAL)
		return;

	if (m_underlying_status_text[type].Compare(text) == 0)
		return;

	m_underlying_status_text[type].Set( text );
	BroadcastDesktopWindowStatusChanged(type);
}

const uni_char*	 DesktopWindow::GetUnderlyingStatusText( DesktopWindowStatusType type  )
{
	if (type < 0 || type >= STATUS_TYPE_TOTAL)
		return NULL;

	return m_underlying_status_text[type].CStr();
}

void DesktopWindow::BroadcastDesktopWindowChanged()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDesktopWindowChanged(this);
	}
	GetModelItem().Change();
	DesktopGroupModelItem* group = GetParentGroup();
	if (group)
		group->Change();
}


void DesktopWindow::BroadcastDesktopWindowStatusChanged(DesktopWindowStatusType type )
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDesktopWindowStatusChanged(this, type);
	}
}


void DesktopWindow::BroadcastDesktopWindowClosing(BOOL user_initiated)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDesktopWindowClosing(this, user_initiated);
	}
}

void DesktopWindow::BroadcastDesktopWindowContentChanged()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDesktopWindowContentChanged(this);
	}
}

void DesktopWindow::BroadcastDesktopWindowBrowserViewChanged()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDesktopWindowBrowserViewChanged(this);
	}
}

void DesktopWindow::BroadcastDesktopWindowShown(BOOL shown)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDesktopWindowShown(this, shown);
	}
}

void DesktopWindow::BroadcastDesktopWindowPageChanged()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDesktopWindowPageChanged(this);
	}
}

void DesktopWindow::BroadcastDesktopWindowAnimationRectChanged(const OpRect &source, const OpRect &target)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDesktopWindowAnimationRectChanged(this,source, target);
	}
}

void DesktopWindow::GetOuterSize(UINT32& width, UINT32& height)
{
	m_window->GetOuterSize(&width, &height);
}

void DesktopWindow::SetOuterSize(UINT32 width, UINT32 height)
{
	m_window->SetOuterSize(width, height);
}

void DesktopWindow::GetInnerSize(UINT32& width, UINT32& height)
{
	m_window->GetInnerSize(&width, &height);
}

void DesktopWindow::SetInnerSize(UINT32 width, UINT32 height)
{
	m_window->SetInnerSize(width, height);
}

void DesktopWindow::GetOuterPos(INT32& x, INT32& y)
{
	m_window->GetOuterPos(&x, &y);
}

void DesktopWindow::SetOuterPos(INT32 x, INT32 y)
{
	m_window->SetOuterPos(x, y);
}

void DesktopWindow::GetInnerPos(INT32& x, INT32& y)
{
	m_window->GetInnerPos(&x, &y);
}

void DesktopWindow::SetInnerPos(INT32 x, INT32 y)
{
	m_window->SetInnerPos(x, y);
}

void DesktopWindow::SetMinimumInnerSize(UINT32 width, UINT32 height)
{
	m_window->SetMinimumInnerSize(width, height);
}

void DesktopWindow::SetMaximumInnerSize(UINT32 width, UINT32 height)
{
	m_window->SetMaximumInnerSize(width, height);
}

INT32 DesktopWindow::GetOuterWidth()
{
	UINT32 width, height;
	GetOuterSize(width, height);
	return width;
}

INT32 DesktopWindow::GetOuterHeight()
{
	UINT32 width, height;
	GetOuterSize(width, height);
	return height;
}

INT32 DesktopWindow::GetInnerWidth()
{
	UINT32 width, height;
	GetInnerSize(width, height);
	return width;
}

INT32 DesktopWindow::GetInnerHeight()
{
	UINT32 width, height;
	GetInnerSize(width, height);
	return height;
}

void DesktopWindow::SetWidgetImage(const OpWidgetImage* widget_image)
{
	m_image.SetWidgetImage(widget_image);
	SetWindowIconFromIcon();
}

void DesktopWindow::SetIcon(Image& image, BOOL propagate_to_native_window)
{
	m_delayed_propagate_to_native_window = m_delayed_propagate_to_native_window ? m_delayed_propagate_to_native_window : propagate_to_native_window;
	m_delayed_has_image_data = TRUE;
 
	m_image.SetBitmapImage(image, FALSE);
 
	m_delayed_update_flags |= UPDATE_TYPE_IMAGE;

	// update the image immediately if it has been cleared or a page is no longer loading
	if(!m_image.GetImage() || (GetWindowCommander() && !GetWindowCommander()->IsLoading()))
	{
		m_delayed_trigger.InvokeTrigger(OpDelayedTrigger::INVOKE_NOW);
	}
	else
	{
		m_delayed_trigger.InvokeTrigger(OpDelayedTrigger::INVOKE_DELAY);
	}
}

void DesktopWindow::SetIcon(const OpStringC8& image, BOOL propagate_to_native_window)
{
	m_delayed_has_image_data = FALSE;
	m_delayed_icon_image.Set(image);
	m_delayed_propagate_to_native_window = m_delayed_propagate_to_native_window ? m_delayed_propagate_to_native_window : propagate_to_native_window;
	m_delayed_update_flags |= UPDATE_TYPE_IMAGE;

    // update the image immediately if it has been cleared or a page is no longer loading
	if(!m_image.GetImage() || (GetWindowCommander() && !GetWindowCommander()->IsLoading()))
	{
		m_delayed_trigger.InvokeTrigger(OpDelayedTrigger::INVOKE_NOW);
	}
	else
	{
		m_delayed_trigger.InvokeTrigger(OpDelayedTrigger::INVOKE_DELAY);
	}
}

void DesktopWindow::SetTitle(const uni_char* title)
{
	if(m_title.Compare(title))
    {
		m_title.Set(title);
        m_window->SetTitle(m_title.CStr());
		BroadcastDesktopWindowChanged();
        //STATUS_TYPE_TITLE is only used in ChatWindow and seems useless for others
        //So don't update it here,or "- Opera" will be added to the title of header toolbar of ChatWindow
        //SetStatusText(m_title.CStr(), STATUS_TYPE_TITLE);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityEventTextChanged));
#endif
	}
}

void DesktopWindow::OnTrigger(OpDelayedTrigger*)
{
	if(m_delayed_update_flags & UPDATE_TYPE_IMAGE)
    {
		// only set it if the image has changed
		if(!m_image.GetImage() || m_delayed_has_image_data || (m_image.GetImage() && m_delayed_icon_image.Compare(m_image.GetImage())))
		{
			if(!m_delayed_has_image_data)
			{
				m_image.SetImage(m_delayed_icon_image.CStr());
            }
			if (m_delayed_propagate_to_native_window)
			{
				SetWindowIconFromIcon();
				m_delayed_propagate_to_native_window = FALSE;
			}
		}
		m_delayed_has_image_data = FALSE;
	}
	m_delayed_update_flags = 0;
}

void DesktopWindow::GetBounds(OpRect& rect)
{
	rect.x = 0;
	rect.y = 0;
	GetInnerSize((UINT32&)rect.width, (UINT32&)rect.height);
}

void DesktopWindow::GetRect(OpRect& rect)
{
	GetOuterPos(rect.x, rect.y);
	GetOuterSize((UINT32&)rect.width, (UINT32&)rect.height);
}

void DesktopWindow::GetAbsoluteRect(OpRect& rect)
{
	rect = m_widget_container->GetRoot()->GetScreenRect();
}

void DesktopWindow::GetScreenRect(OpRect& rect, BOOL workspace)
{
	OpRect window_rect;

	GetRect(window_rect);

	OpScreenProperties screen_props;
	OpPoint point(window_rect.x + window_rect.width / 2, window_rect.y + window_rect.height / 2);
	g_op_screen_info->GetProperties(&screen_props, GetOpWindow(), &point);

	if (workspace)
		rect.Set(screen_props.workspace_rect);
	else
		rect.Set(screen_props.screen_rect);
}

void DesktopWindow::FitRectToScreen(OpRect& rect, BOOL fully_visible, BOOL full_screen)
{
	OpRect screen;
	OpScreenProperties screen_props;
	OpPoint point(rect.x + rect.width/2, rect.y + rect.height/2);
	g_op_screen_info->GetProperties(&screen_props, GetOpWindow(), &point);
	if (full_screen)
		screen.Set(screen_props.screen_rect);
	else
		screen.Set(screen_props.workspace_rect);

	//make sure the window is not to big at all
	if (fully_visible)
	{
		//make sure the window is not to big at all
		rect.width = rect.width < screen.width ? rect.width : screen.width;
		rect.height = rect.height < screen.height ? rect.height : screen.height;
	}
	else
	{
		//check if a substantial amount of the window is shown (10%)
		OpRect intersect(screen);
		intersect.IntersectWith(rect);
		if ((intersect.width > (rect.width / 10)) && (intersect.height > (rect.height / 10)))
			return;	//don't correct the possibly hidden part of the window, enough is visible
	}
	//calculate the offset needed to make the rect entirely visible again
	OpRect overlap(screen);
	overlap.UnionWith(rect);
	//apply the offset
	rect.x += rect.x < screen.x ? overlap.width - screen.width : screen.width - overlap.width;
	rect.y += rect.y < screen.y ? overlap.height - screen.height : screen.height - overlap.height;
}

void DesktopWindow::SetAttention(BOOL attention)
{
	if (m_attention == attention || (attention && IsActive()))
		return;

	m_attention = attention;

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDesktopWindowAttention(this, attention);
	}

	if (attention)
		m_window->Attention();

	g_input_manager->UpdateAllInputStates();
}

BOOL DesktopWindow::Activate(BOOL activate, BOOL parent_too)
{
	if(activate && GetType() == WINDOW_TYPE_BROWSER
		&& this != g_application->GetActiveBrowserDesktopWindow()
	    && g_application->LockActiveWindow())
	{
		return TRUE;
	}

	// if the active window was opened in the background, clear the OpenURLSetting.m_in_background member so
	// the activation is right
	if(g_pcui->GetIntegerPref(PrefsCollectionUI::ActivateTabOnClose) == 1 && !activate)
	{
		// if the closing window was opened in the foreground, then don't use the regular configuration
		// to activate the next to the right
		if (GetType() == WINDOW_TYPE_DOCUMENT)
		{
			OpenURLSetting &setting = static_cast<DocumentDesktopWindow *>(this)->GetOpenURLSetting();

			if (setting.m_in_background == NO)
				setting.m_in_background = YES;
		}
	}
	g_application->ShowOpera(TRUE);

	if (activate && parent_too)
	{
		DesktopWindow* dw = GetParentDesktopWindow();

		if(dw)
			dw->Activate();
	}

	if (m_active == activate)
		return FALSE;

	if (activate)
	{
		// Put a tab into fullscreen when activated if the parent is in fullscreen -- now enabled for all platforms
		DesktopWindow* pdw  = GetParentDesktopWindow();
		if( pdw && pdw->GetType() == WINDOW_TYPE_BROWSER && pdw->IsFullscreen() && !IsFullscreen())
		{
			if (Fullscreen(TRUE))
			{
				// Tell core about it
				BrowserDesktopWindow* bdw = static_cast<BrowserDesktopWindow*>(pdw);
				if (GetWindowCommander())
					GetWindowCommander()->SetDocumentFullscreenState(bdw->FullscreenIsScripted() ? OpWindowCommander::FULLSCREEN_NORMAL : OpWindowCommander::FULLSCREEN_PROJECTION);
			}
		}
		m_window->Activate();
	}
	else
		m_window->Deactivate();

	return TRUE;
}

void DesktopWindow::Restore()
{
	if( GetType() == WINDOW_TYPE_BROWSER )
	{
		// Prevent attempt to hide banner in non-registered mode
		if( Fullscreen(FALSE) )
			return;
	}

	m_window->Restore();
}

void DesktopWindow::Minimize()
{
	if( GetType() == WINDOW_TYPE_BROWSER )
	{
		// Prevent attempt to hide banner in non-registered mode
		if( Fullscreen(FALSE) )
			return;
	}

	m_window->Minimize();
}

void DesktopWindow::Maximize()
{
	if( GetType() == WINDOW_TYPE_BROWSER )
	{
		// Prevent attempt to hide banner in non-registered mode
		if( Fullscreen(FALSE) )
			return;
	}

	m_window->Maximize();
}

void DesktopWindow::Raise()
{
	m_window->Raise();
}

void DesktopWindow::Lower()
{
	m_window->Lower();

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDesktopWindowLowered(this);
	}
}

void DesktopWindow::Enable(BOOL enable)
{
	if (enable && m_disabled_count > 0)
	{
		m_disabled_count--;
	}
	else if (!enable)
	{
		m_disabled_count++;
	}
}

BOOL DesktopWindow::Fullscreen(BOOL fullscreen)
{
	if (KioskManager::GetInstance()->GetNoChangeFullScreen())
		return FALSE;

	if (fullscreen && !IsFullscreen())
	{
		m_fullscreen = TRUE;
		OnFullscreen(TRUE);
		if (!KioskManager::GetInstance()->GetEnabled())
			g_sleep_manager->StartStayAwakeSession();
		OpRect rect;
		m_window->GetDesktopPlacement(rect, m_old_state);
		m_window->Fullscreen();
		return TRUE;
	}
	else if (!fullscreen && IsFullscreen())
	{
		m_fullscreen = FALSE;
		OnFullscreen(FALSE);
		if (!KioskManager::GetInstance()->GetEnabled())
			g_sleep_manager->EndStayAwakeSession();
		switch (m_old_state)
		{
			case OpWindow::RESTORED:	Restore(); break;
			case OpWindow::MINIMIZED:	Minimize(); break;
			case OpWindow::MAXIMIZED:	Maximize(); break;
		}
		return TRUE;
	}
	return FALSE;
}

DesktopGroupModelItem* DesktopWindow::GetParentGroup()
{
	DesktopWindowCollectionItem* parent = m_model_item.GetParentItem();

	return parent && parent->GetType() == WINDOW_TYPE_GROUP
		? static_cast<DesktopGroupModelItem*>(parent) : NULL;
}

BOOL DesktopWindow::IsFullscreen()
{
	return m_fullscreen;
}

BOOL DesktopWindow::IsRestored()
{
	OpRect rect;
	OpWindow::State state;
	m_window->GetDesktopPlacement(rect, state);

	return state == OpWindow::RESTORED;
}

BOOL DesktopWindow::IsMinimized()
{
	OpRect rect;
	OpWindow::State state;
	m_window->GetDesktopPlacement(rect, state);

	return state == OpWindow::MINIMIZED;
}

BOOL DesktopWindow::IsMaximized()
{
	OpRect rect;
	OpWindow::State state;
	m_window->GetDesktopPlacement(rect, state);

	return state == OpWindow::MAXIMIZED;
}


BOOL DesktopWindow::SupportsExternalCompositing()
{
	return m_window && m_window->SupportsExternalCompositing();
}


void DesktopWindow::LockUpdate(BOOL lock)
{
	if (!lock)
		m_lock_count--;

	if (!m_lock_count)
	{
		OpWorkspace* workspace = GetParentWorkspace();

		if (!workspace || !workspace->GetParentDesktopWindow() || !workspace->GetParentDesktopWindow()->IsLocked())
		{
			m_window->LockUpdate(lock);
		}
	}

	if (lock)
		m_lock_count++;
}

void DesktopWindow::SetSkin(const char* skin, const char* fallback_skin)
{
	GetSkinImage()->SetImage(skin, fallback_skin);
	UpdateSkin();
}

OpWidgetImage* DesktopWindow::GetSkinImage()
{
	return GetWidgetContainer()->GetRoot()->GetBorderSkin();
}

void DesktopWindow::UpdateSkin()
{
	m_window->SetSkin(GetSkin());
}

void DesktopWindow::SetWindowIconFromIcon()
{
	GetOpWindow()->SetIcon(&m_image);
}

void DesktopWindow::UpdateLanguage()
{
	if (GetWidgetContainer() != NULL)
		GetWidgetContainer()->GetRoot()->SetRTL(UiDirection::Get() == UiDirection::RTL);
}

/***********************************************************************************
**
**	GetWidgetByName
**
***********************************************************************************/

OpWidget* DesktopWindow::GetWidgetByName(const OpStringC8 & name)
{
	if (name.IsEmpty())
		return NULL;

	if (g_application->GetCustomizeToolbarsDialog() && op_stricmp(name.CStr(), "target toolbar") == 0)
		return g_application->GetCustomizeToolbarsDialog()->GetTargetToolbar();

	OpWidget* root_widget = GetWidgetContainer()->GetRoot();

	if (root_widget)
	{
		return root_widget->GetWidgetByName(name.CStr());
	}

	return NULL;
}

/***********************************************************************************
**
**	GetWidgetByTypeAndId
**
***********************************************************************************/

OpWidget* DesktopWindow::GetWidgetByTypeAndId(OpTypedObject::Type type, INT32 id)
{
	OpWidget* widget = GetWidgetContainer()->GetRoot()->GetWidgetByTypeAndId(type, id);

	if (widget)
		return widget;

	DesktopWindow* dw = GetParentDesktopWindow();
	return dw ? dw->GetWidgetByTypeAndId(type, id) : NULL;
}

/***********************************************************************************
**
**	GetParentDesktopWindow
**
***********************************************************************************/

DesktopWindow* DesktopWindow::GetParentDesktopWindow()
{
	return m_parent_workspace ? m_parent_workspace->GetParentDesktopWindow() : NULL;
}

/***********************************************************************************
**
**	GetToplevelDesktopWindow
**
***********************************************************************************/

DesktopWindow* DesktopWindow::GetToplevelDesktopWindow()
{
	DesktopWindow* top_level = this;
	DesktopWindow* desktop_window = this->GetParentDesktopWindow();

	while (desktop_window)
	{
		top_level = desktop_window;
		desktop_window = top_level->GetParentDesktopWindow();
	}

	return top_level;
}


/***********************************************************************************
**
**	SetParentWorkspace
**
***********************************************************************************/

void DesktopWindow::SetParentWorkspace(OpWorkspace* workspace, BOOL activate)
{
	if (workspace == GetParentWorkspace())
	{
		return;
	}

	switch (GetType())
	{
		case WINDOW_TYPE_BROWSER:
		case WINDOW_TYPE_HOTLIST:
			return;
	}


	if (workspace)
		m_parent_workspace = workspace;
	else
	{
		// We used to call Application::GetBrowserDesktopWindow() but it is not
		// suitable in this case as no new page shall be opened in the new browser
		// window (bug #263155) [espen 2007-05-04]

#ifdef DEVTOOLS_INTEGRATED_WINDOW
		BOOL devtools_only = FALSE;

		// Turn on the devtools only flag if putting the devtools in a window on it own
		if (GetType() == WINDOW_TYPE_DEVTOOLS)
			devtools_only = TRUE;
#endif // DEVTOOLS_INTEGRATED_WINDOW

		BrowserDesktopWindow* browser;
		OP_STATUS err = BrowserDesktopWindow::Construct(&browser
#ifdef DEVTOOLS_INTEGRATED_WINDOW
															, devtools_only
#endif // DEVTOOLS_INTEGRATED_WINDOW
															);
		RETURN_VOID_IF_ERROR(err);

		OpRect rect(0, 0, 0, 0);
		WinSizeState state = NORMAL;
		OpString win_name;
		win_name.Set(browser->GetWindowName());
		g_pcui->GetWindowInfo(win_name, rect, state);
		browser->SetOuterSize(rect.width, rect.height);

		BOOL behind = g_application->IsOpeningInBackgroundPreferred(FALSE);
		browser->Show(TRUE, NULL, OpWindow::RESTORED, behind, FALSE);

		m_parent_workspace = browser->GetWorkspace();
	}

	if (m_window)
		m_window->SetParent(m_parent_workspace->GetOpWindow(), NULL, !activate);
	m_parent_workspace->AddDesktopWindow(this);
	SetParentInputContext(m_parent_workspace);

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDesktopWindowParentChanged(this);
	}

	if (activate || (m_active && m_parent_workspace->GetActiveDesktopWindow() != this))
	{
		// We cannot activate an already active window. The state is cached
		// in the object and in platform code which results in that the workspace
		// object is not informed. Must deactivate first [espen 2007-11-27]

		if( m_active )
			Activate(FALSE);
		Activate(TRUE);
	}

}

/***********************************************************************************
**
**	ResetParentWorkspace
**
***********************************************************************************/

void DesktopWindow::ResetParentWorkspace()
{
	if (m_parent_workspace == NULL)
		return;

	OpWorkspace* workspace = NULL;
	for (DesktopWindowCollectionItem* parent = GetModelItem().GetParentItem(); parent && !workspace; parent = parent->GetParentItem())
	{
		if (parent->GetDesktopWindow())
			workspace = parent->GetDesktopWindow()->GetWorkspace();
	}

	SetParentWorkspace(workspace, FALSE);
}

/***********************************************************************************
**
**	GetItemData
**
***********************************************************************************/

OP_STATUS DesktopWindow::GetItemData(ItemData* item_data)
{
	if (item_data->query_type == INFO_QUERY)
	{
//		item_data->info_query_data.info_text->SetStatusText(GetTitle());
		GetToolTipText(NULL, *item_data->info_query_data.info_text);
		return OpStatus::OK;
	}
	else if (item_data->query_type == COLUMN_QUERY)
	{
		if(item_data->column_query_data.column == 0)
		{
			item_data->column_query_data.column_text->Set(GetTitle());
			item_data->column_query_data.column_image = GetWidgetImage()->GetImage();
			item_data->column_bitmap = GetWidgetImage()->GetBitmapImage();

			if (this == g_application->GetActiveDesktopWindow(FALSE))
			{
				item_data->flags |= FLAG_BOLD;
			}
		}
		else if(item_data->column_query_data.column == 1)
		{
			if(GetType() == WINDOW_TYPE_DOCUMENT)
			{
				DocumentDesktopWindow *win = (DocumentDesktopWindow *)this;
				URL url = WindowCommanderProxy::GetMovedToURL(win->GetWindowCommander());
				url.GetAttribute(URL::KUniName_Username_Password_Hidden, *item_data->column_query_data.column_text);
			}
		}
	}
	else if (item_data->query_type == INIT_QUERY)
	{
		item_data->flags |= FLAG_INITIALLY_OPEN;
	}
	else if (item_data->query_type == MATCH_QUERY)
    {
		BOOL matched = VisibleInWindowList();

		if (item_data->match_query_data.match_text)
			matched = matched && MatchText(item_data->match_query_data.match_text->CStr());

		if(matched)
			item_data->flags |= FLAG_MATCHED;
	}
	else if (item_data->query_type == URL_QUERY)
	{
		RETURN_IF_ERROR(item_data->url_query_data.thumbnail_text->title.Set(GetTitle()));
		item_data->url_query_data.thumbnail_text->thumbnail = GetThumbnailImage();
		item_data->url_query_data.thumbnail_text->fixed_image = HasFixedThumbnailImage();
	}
	return OpStatus::OK;
}

BOOL DesktopWindow::MatchText(const uni_char* string)
{
	OpString word;
	while ((string = StringUtils::SplitWord(string, ' ', word)))
	{
		if (GetTitle() && uni_stristr(GetTitle(), word.CStr()))
			continue;

		if (GetType() != WINDOW_TYPE_DOCUMENT)
			return FALSE;

		OpString url;
		DocumentDesktopWindow *win = static_cast<DocumentDesktopWindow*>(this);
		OpStatus::Ignore(win->GetURL(url));
		if (url.FindI(word) == KNotFound)
			return FALSE;
	}

	return TRUE;
}

void DesktopWindow::GetToolTipThumbnailText(OpToolTip* tooltip, OpString& title, OpString& url_string, URL& url)
{
	title.Set(GetTitle());
}

void DesktopWindow::GetToolTipText(OpToolTip* tooltip, OpInfoText& text)
{
	OpString title;

	g_languageManager->GetString(Str::SI_IDSTR_HL_TREE_TITLE, title);

	text.AddTooltipText(title.CStr(), GetTitle());

#ifdef SUPPORT_GENERATE_THUMBNAILS

	if(tooltip && g_pcui->GetIntegerPref(PrefsCollectionUI::UseThumbnailsInTabTooltips))
	{
		Image image = GetThumbnailImage();

		if(!image.IsEmpty())
		{
			tooltip->SetImage(image, HasFixedThumbnailImage());
		}
	}
#endif // SUPPORT_GENERATE_THUMBNAILS
}

BOOL DesktopWindow::RespectsGlobalVisibilityChange(BOOL visible)
{
	return !visible || m_window->GetStyle() != OpWindow::STYLE_POPUP && m_window->GetStyle() != OpWindow::STYLE_EXTENSION_POPUP;
}

/***********************************************************************************
**
**	OnDropFiles
**
***********************************************************************************/

void DesktopWindow::OnDropFiles(const OpVector<OpString>& file_names)
{
	for (UINT32 i = 0; i < file_names.GetCount(); i++)
	{
		g_application->GoToPage(file_names.Get(i)->CStr(), i > 0);
	}
}

/***********************************************************************************
**
**	OnDragDrop
**
***********************************************************************************/
void DesktopWindow::OnDragMove(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);
	BOOL force_new_window = widget->GetType() == WIDGET_TYPE_WORKSPACE
		|| GetWidgetContainer()->GetView()->GetShiftKeys() & (SHIFTKEY_CTRL | SHIFTKEY_SHIFT);

	if (drag_object->GetType() == DRAG_TYPE_WINDOW && widget->GetType() == WIDGET_TYPE_WORKSPACE)
	{
		drag_object->SetDesktopDropType(DROP_MOVE);
	}
	else if (drag_object->GetType() == DRAG_TYPE_BOOKMARK)
	{
		drag_object->SetDesktopDropType(force_new_window ? DROP_COPY : DROP_MOVE);
	}
	else if (drag_object->GetType() == DRAG_TYPE_CONTACT)
	{
		drag_object->SetDesktopDropType(force_new_window ? DROP_COPY : DROP_MOVE);
	}
	else if (drag_object->GetType() == DRAG_TYPE_HISTORY)
	{
		drag_object->SetDesktopDropType(force_new_window ? DROP_COPY : DROP_MOVE);
	}
	else if (drag_object->GetType() == DRAG_TYPE_SEPARATOR)
	{
		drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);
	}
	else if (drag_object->GetURL() )
	{
		// if the source and target is the same window.
		BOOL same_window = FALSE;

		if (drag_object->GetSource() && drag_object->GetSource()->GetType() >= WIDGET_TYPE &&
			drag_object->GetSource()->GetType() < WIDGET_TYPE_LAST)
		{
			//dragged object is a widget, we can get the window
			OpWidget* dragged_widget = (OpWidget*)drag_object->GetSource();
			if (widget->GetParentDesktopWindow() == dragged_widget->GetParentDesktopWindow())
			{
				same_window = TRUE;
				force_new_window = TRUE; // dragging link in the same page should always open a new tab
			}
		}		

		//drag of a link to the same window is not forced
		//Let's test if the link is dragged to the same window
		INT32 state = g_op_system_info->GetShiftKeyState();
		if (((state & (SHIFTKEY_CTRL|SHIFTKEY_SHIFT)) == 0) &&
			!(g_pcui->GetIntegerPref(PrefsCollectionUI::EnableDrag)&DRAG_SAMEWINDOW) &&
			same_window)
		{
			//dragged widget and target widget have are in the same window
			//this is disabled on default to prevent unintended drops
			drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);
			return;
		}

		if( !*drag_object->GetURL() )
		{
			drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);
			return;
		}
		drag_object->SetDesktopDropType(force_new_window ? DROP_COPY : DROP_MOVE);
	}
}
/***********************************************************************************
**
**	OnDragDrop
**
***********************************************************************************/
void DesktopWindow::OnDragDrop(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);
	BOOL force_new_window = widget->GetType() == WIDGET_TYPE_WORKSPACE
		|| GetWidgetContainer()->GetView()->GetShiftKeys() & (SHIFTKEY_CTRL | SHIFTKEY_SHIFT);

	if (drag_object->GetType() == DRAG_TYPE_WINDOW && widget->GetType() == WIDGET_TYPE_WORKSPACE)
	{
		for (INT32 i = 0; i < drag_object->GetIDCount(); i++)
		{
			DesktopWindow* desktop_window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID(drag_object->GetID(i));

			if (desktop_window)
			{
				desktop_window->SetParentWorkspace((OpWorkspace*) widget);
			}
		}
	}
	else if (drag_object->GetType() == DRAG_TYPE_BOOKMARK)
	{
		if( drag_object->GetIDCount() > 0 )
		{
			// When dropping bookmark in tab, always open in tab it is dropped to
			g_hotlist_manager->OpenUrls( drag_object->GetIDs(), NO /*new_window*/, MAYBE /*new_page*/, MAYBE /* in_background*/, this );
		}
	}
	else if (drag_object->GetType() == DRAG_TYPE_CONTACT)
	{
		if( drag_object->GetIDCount() > 0 )
		{
			g_hotlist_manager->OpenContacts( drag_object->GetIDs(), NO, YES, MAYBE, this );
		}
	}
	else if (drag_object->GetType() == DRAG_TYPE_HISTORY)
	{
		DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();

		for (INT32 i = 0; i < drag_object->GetIDCount(); i++)
		{
			HistoryModelItem* history_item = history_model->FindItem(drag_object->GetID(i));

			if (history_item)
			{
				OpString address;
				history_item->GetAddress(address);

				g_application->GoToPage( address.CStr(), i > 0 || force_new_window, FALSE, FALSE, this );
			}
		}
	}
	else if (drag_object->GetURL() )
	{
		// if the source and target is the same window.
		BOOL same_window = FALSE;
		BOOL open_in_background = g_pcui->GetIntegerPref(PrefsCollectionUI::OpenDraggedLinkInBackground);

		if (drag_object->GetSource() && drag_object->GetSource()->GetType() >= WIDGET_TYPE &&
			drag_object->GetSource()->GetType() < WIDGET_TYPE_LAST)
		{
			//dragged object is a widget, we can get the window
			OpWidget* dragged_widget = (OpWidget*)drag_object->GetSource();
			if (widget->GetParentDesktopWindow() == dragged_widget->GetParentDesktopWindow())
			{
				same_window = TRUE;
				force_new_window = TRUE; // dragging link in the same page should always open a new tab
			}
		}		

		//drag of a link to the same window is not forced
		//Let's test if the link is dragged to the same window
		INT32 state = g_op_system_info->GetShiftKeyState();
		if (((state & (SHIFTKEY_CTRL|SHIFTKEY_SHIFT)) == 0) &&
			!(g_pcui->GetIntegerPref(PrefsCollectionUI::EnableDrag)&DRAG_SAMEWINDOW) &&
			same_window)
		{
			//dragged widget and target widget have are in the same window
			//this is disabled on default to prevent unintended drops
			return;
		}

		if( !*drag_object->GetURL() )
		{
			return;
		}

		drag_object->SetDropCaptureLocked(true);

		if( drag_object->GetURLs().GetCount() > 0 )
		{
			const OpVector<OpString>& url_list = drag_object->GetURLs();
			for (UINT32 i = 0; i < url_list.GetCount(); i++)
			{
				g_application->GoToPage(url_list.Get(i)->CStr(), force_new_window || i > 0, open_in_background, FALSE, this );
			}
		}
		else
		{
			g_application->GoToPage(drag_object->GetURL(), force_new_window, open_in_background, FALSE, this );
		}
		drag_object->SetDropCaptureLocked(false);
	}
}

/***********************************************************************************
**
**	SetFocus
**
***********************************************************************************/

void DesktopWindow::SetFocus(FOCUS_REASON reason)
{
	OpWidget* widget = GetWidgetContainer()->GetNextFocusableWidget(GetWidgetContainer()->GetRoot(), TRUE);

	if (widget)
	{
		widget->SetFocus(FOCUS_REASON_OTHER);
	}
	else
	{
		OpInputContext::SetFocus(reason);
	}
}

/***********************************************************************************
**
**	RestoreFocus
**
***********************************************************************************/

void DesktopWindow::RestoreFocus(FOCUS_REASON reason)
{
	if (GetDialogCount() && GetDialog(GetDialogCount() - 1)->GetOverlayed())
	{
		// Restore focus on the overlay dialog
		OpInputContext* context = GetDialog(GetDialogCount() - 1);
		g_input_manager->RestoreKeyboardInputContext(context, this, reason);
		g_input_manager->SetMouseInputContext(context ? context : this);
	}
	else
	{
		g_input_manager->RestoreKeyboardInputContext(this, GetWidgetContainer()->GetNextFocusableWidget(GetWidgetContainer()->GetRoot(), TRUE), reason);
		g_input_manager->SetMouseInputContext(this);
	}
}

void DesktopWindow::SetLockedByUser(BOOL locked)
{
	m_locked_by_user = locked;

	m_window->SetLockedByUser(locked);
}

void DesktopWindow::UpdateUserLockOnPagebar()
{
	PagebarButton* button = GetPagebarButton();
	OpPagebar* bar = button ? button->GetPagebar() : NULL;
	if (bar)
		bar->OnLockedByUser(button, IsLockedByUser());
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

/***********************************************************************************
**
**  Acessibility methods
**
***********************************************************************************/

int DesktopWindow::GetAccessibleChildrenCount()
{
	int count = m_widget_container->GetRoot()->GetAccessibleChildrenCount();
#ifdef _WINDOWS_
	if (((WindowsOpWindow*)m_window)->m_menubar)
	{
		count++;
	}
#endif //_WINDOWS_
	return count;
}

OpAccessibleItem* DesktopWindow::GetAccessibleParent()
{
	return GetParentWorkspace();
}

OpAccessibleItem* DesktopWindow::GetAccessibleChild(int child)
{
#ifdef _WINDOWS_
	if (((WindowsOpWindow*)m_window)->m_menubar)
	{
		if (child == 0)
			return ((WindowsOpWindow*)m_window)->m_menubar;
		child--;
	}
#endif //_WINDOWS_
	return m_widget_container->GetRoot()->GetAccessibleChild(child);
}

int DesktopWindow::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	int index = m_widget_container->GetRoot()->GetAccessibleChildIndex(child);
#ifdef _WINDOWS_
	if (index != Accessibility::NoSuchChild && ((WindowsOpWindow*)m_window)->m_menubar)
		index++; 
	else if (child == ((WindowsOpWindow*)m_window)->m_menubar)
		index = 0;
#endif //_WINDOWS_
	return index;
}

OpAccessibleItem* DesktopWindow::GetAccessibleChildOrSelfAt(int x, int y)
{

#ifdef _WINDOWS_
	if (((WindowsOpWindow*)m_window)->m_menubar)
	{
		OpAccessibleItem* menubar_acc = ((WindowsOpWindow*)m_window)->m_menubar->GetAccessibleChildOrSelfAt(x,y);
		if (menubar_acc)
			return menubar_acc;
	}
#endif //_WINDOWS_
	return m_widget_container->GetRoot()->GetAccessibleChildOrSelfAt(x,y);
}

OpAccessibleItem* DesktopWindow::GetAccessibleFocusedChildOrSelf()
{
#ifdef _WINDOWS_
	if (((WindowsOpWindow*)m_window)->m_menubar)
	{
		OpAccessibleItem* menubar_acc = ((WindowsOpWindow*)m_window)->m_menubar->GetAccessibleFocusedChildOrSelf();
		if (menubar_acc)
			return menubar_acc;
	}
#endif //_WINDOWS_
	OpAccessibleItem* focused_widget = m_widget_container->GetRoot()->GetAccessibleFocusedChildOrSelf();
	if (focused_widget)
		return focused_widget;
	else if (IsActive())
		return this;
	else
		return NULL;
}

OpAccessibleItem* DesktopWindow::GetNextAccessibleSibling()
{
	return NULL;
}

OpAccessibleItem* DesktopWindow::GetPreviousAccessibleSibling()
{
	return NULL;
}

OP_STATUS DesktopWindow::AccessibilityGetAbsolutePosition(OpRect & rect)
{
	m_widget_container->GetRoot()->AccessibilityGetAbsolutePosition(rect);
	return OpStatus::OK;
}

OP_STATUS DesktopWindow::AccessibilityClicked()
{
	return OpStatus::OK;
}

OP_STATUS DesktopWindow::AccessibilitySetText(const uni_char* text)
{
	return OpStatus::OK;
}

OP_STATUS DesktopWindow::AccessibilitySetValue(int value)
{
	return OpStatus::OK;
}

OP_STATUS DesktopWindow::AccessibilityGetValue(int &value)
{
	return OpStatus::ERR;
}

OP_STATUS DesktopWindow::AccessibilityGetText(OpString& str)
{
	str.Set(GetTitle());

	return OpStatus::OK;
}

OP_STATUS DesktopWindow::AccessibilityGetDescription(OpString& str)
{
	str.Set(GetWindowDescription());

	return OpStatus::OK;
}

OP_STATUS DesktopWindow::AccessibilityGetKeyboardShortcut(ShiftKeyState* shifts, uni_char* kbdShortcut)
{
	return OpStatus::ERR;
}

Accessibility::State DesktopWindow::AccessibilityGetState()
{
	Accessibility::State state = m_widget_container->GetRoot()->AccessibilityGetState();
	// The screen reader doesn't need to worry about background windows, even if they ARE partially visible.
	if (GetParentWorkspace() && (GetParentWorkspace()->GetActiveDesktopWindow() != this))
	{
		state |= Accessibility::kAccessibilityStateInvisible;
	}
	return state;
}

Accessibility::ElementKind DesktopWindow::AccessibilityGetRole() const
{
	return Accessibility::kElementKindWindow;
}

OpAccessibleItem* DesktopWindow::GetLeftAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* DesktopWindow::GetRightAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* DesktopWindow::GetUpAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* DesktopWindow::GetDownAccessibleObject()
{
	return NULL;
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

/***********************************************************************************
**
**	DesktopWindowSpy
**
***********************************************************************************/

DesktopWindowSpy::DesktopWindowSpy(BOOL prefer_windows_with_browser_view) :
	m_input_state_listener(this),
	m_desktop_window(NULL),
	m_input_context(NULL),
	m_prefer_windows_with_browser_view(prefer_windows_with_browser_view)
{
}

DesktopWindowSpy::~DesktopWindowSpy()
{
	SetSpyInputContext(NULL, FALSE);
}

/***********************************************************************************
**
**	SetSpyInputContext
**
***********************************************************************************/

void DesktopWindowSpy::SetSpyInputContext(OpInputContext* input_context, BOOL send_change)
{
	if (input_context == m_input_context)
		return;

	m_input_context = input_context;

	if (m_input_context)
	{
		m_input_state_listener.Enable(TRUE);
		if (send_change)
			UpdateTargetDesktopWindow();
	}
	else
	{
		m_input_state_listener.Enable(FALSE);
		SetTargetDesktopWindow(NULL, send_change);
	}
}

/***********************************************************************************
**
**	SetTargetDesktopWindow
**
***********************************************************************************/

void DesktopWindowSpy::SetTargetDesktopWindow(DesktopWindow* desktop_window, BOOL send_change)
{
	if (desktop_window == m_desktop_window)
		return;

	if (m_desktop_window)
	{
		if (GetTargetBrowserView())
			GetTargetBrowserView()->RemoveListener(this);

		m_desktop_window->RemoveListener(this);
	}

	m_desktop_window = desktop_window;

	if (m_desktop_window)
	{
		if (GetTargetBrowserView())
			GetTargetBrowserView()->AddListener(this);

		m_desktop_window->AddListener(this);
	}

	if (send_change)
		OnTargetDesktopWindowChanged(m_desktop_window);
}

/***********************************************************************************
**
**	UpdateTargetDesktopWindow
**
***********************************************************************************/

void DesktopWindowSpy::UpdateTargetDesktopWindow()
{
	OP_ASSERT(m_input_context);

	DesktopWindow* desktop_window = NULL;

	if (m_prefer_windows_with_browser_view)
	{
		desktop_window = (DesktopWindow*) g_input_manager->GetTypedObject(OpTypedObject::WINDOW_TYPE_DESKTOP_WITH_BROWSER_VIEW, m_input_context);
	}

	if (!desktop_window)
	{
		desktop_window = (DesktopWindow*) g_input_manager->GetTypedObject(OpTypedObject::WINDOW_TYPE_DESKTOP, m_input_context);

		if (!desktop_window)
		{
			desktop_window = g_application->GetActiveDesktopWindow();
		}
	}

	SetTargetDesktopWindow(desktop_window);
}

/***********************************************************************************
**
**	OnDesktopWindowClosing
**
***********************************************************************************/

void DesktopWindowSpy::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	UpdateTargetDesktopWindow();

	if (desktop_window == m_desktop_window)
	{
		if (GetTargetBrowserView())
			GetTargetBrowserView()->RemoveListener(this);

		m_desktop_window->RemoveListener(this);
		m_desktop_window = NULL;
	}

}

void DesktopWindowSpy::OnDesktopWindowContentChanged(DesktopWindow* desktop_window)
{
	OP_ASSERT(desktop_window == m_desktop_window);
	OnTargetDesktopWindowChanged(desktop_window);
}

void DesktopWindowSpy::OnDesktopWindowBrowserViewChanged(DesktopWindow* desktop_window)
{
	m_desktop_window = NULL;
	SetTargetDesktopWindow(desktop_window);
}

Image DesktopWindow::GetThumbnailImage(UINT32 width, UINT32 height, BOOL high_quality, BOOL use_skin_image_if_present)
{
	if(GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
	{
		// do nothing for top level windows (for now)
		return Image();
	}
	OpRect rect = GetWidgetContainer()->GetRoot()->GetRect();
	if(rect.IsEmpty())
	{
		// for the off-chance possibility that the root widget has not been laid out yet (as layout is async, it's possible)
		return Image();
	}
	// try to see if we have a skin image we can use that will override the thumbnail. Some windows such as chat, transfer etc.
	// might want to have overrides, but we won't allow overrides for the document window
	if(GetType() != WINDOW_TYPE_DOCUMENT && use_skin_image_if_present)
	{
		const char* name = GetWindowName();
		if(name)
		{
			OpString8 skin_name;

			RETURN_VALUE_IF_ERROR(skin_name.Set(name), Image());

			RETURN_VALUE_IF_ERROR(skin_name.Append(" Thumbnail Icon"), Image());

			OpSkinElement *element = g_skin_manager->GetSkinElement(skin_name.CStr());

			if(element)
			{
				m_uses_fixed_thumbnail_image = TRUE;
				Image image = element->GetImage(0);
				return image;
			}
		}
	}
	m_uses_fixed_thumbnail_image = FALSE;

	WidgetThumbnailGenerator thumbnail_generator(*GetWidgetContainer()->GetRoot());
	return thumbnail_generator.GenerateThumbnail(width, height, high_quality);
}

Image DesktopWindow::GetThumbnailImage(OpPoint &logo)
{
	return GetThumbnailImage();
}

void DesktopWindow::UpdateUseFixedImage()
{
	if(GetType() != WINDOW_TYPE_DOCUMENT)
	{
		const char* name = GetWindowName();
		if(name)
		{
			OpString8 skin_name;

			RETURN_VOID_IF_ERROR(skin_name.Set(name));

			RETURN_VOID_IF_ERROR(skin_name.Append(" Thumbnail Icon"));

			OpSkinElement *element = g_skin_manager->GetSkinElement(skin_name.CStr());

			if(element)
			{
				m_uses_fixed_thumbnail_image = TRUE;
			}
		}
	}
}
Image DesktopWindow::GetSnapshotImage()
{
	if(GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
	{
		// do nothing for top level windows (for now)
		return Image();
	}
	OpRect rect = GetWidgetContainer()->GetRoot()->GetRect();
	if(rect.IsEmpty())
	{
		// for the off-chance possibility that the root widget has not been laid out yet (as layout is async, it's possible)
		return Image();
	}
	WidgetThumbnailGenerator thumbnail_generator(*GetWidgetContainer()->GetRoot());
	return thumbnail_generator.GenerateSnapshot();
}

OP_STATUS DesktopWindow::AddToSession(OpSession* session, INT32 parent_id, BOOL shutdown, BOOL extra_info)
{
	if(!session)
		return OpStatus::ERR_NULL_POINTER;

	OP_ASSERT(!PrivacyMode() && VisibleInWindowList());
	if (PrivacyMode() || !VisibleInWindowList())
		return OpStatus::ERR;

	// We don't want to store anything related to extension-opened documents
	if (GetWindowCommander() && GetWindowCommander()->GetGadget() &&
		GetWindowCommander()->GetGadget()->IsExtension())
		return OpStatus::ERR;

	OpRect windowrect;
	OpWindow::State windowstate;
	if (GetOpWindow())
		GetOpWindow()->GetDesktopPlacement(windowrect, windowstate);

#ifndef RESTORE_MAC_FULLSCREEN_ON_STARTUP
	windowstate = (windowstate == OpWindow::FULLSCREEN) ? OpWindow::MAXIMIZED : windowstate;
#endif

	TRAPD(ret,
		OpSessionWindow* sessionwindow = session->AddWindowL();

		OpWindow::State restore_window_state = GetOpWindow()->GetRestoreState();
		OpWorkspace* workspace = GetParentWorkspace();

		sessionwindow->SetValueL("x", windowrect.x);
		sessionwindow->SetValueL("y", windowrect.y);
		sessionwindow->SetValueL("w", windowrect.width);
		sessionwindow->SetValueL("h", windowrect.height);
		sessionwindow->SetValueL("state", windowstate);
		sessionwindow->SetValueL("restore to state", restore_window_state);
		sessionwindow->SetValueL("id", GetID());
		sessionwindow->SetValueL("parent", parent_id);
		sessionwindow->SetValueL("saveonclose", GetSavePlacementOnClose());
		sessionwindow->SetValueL("position", workspace ? workspace->GetPosition(this) : 0);
		sessionwindow->SetValueL("stack position", workspace ? workspace->GetStackPosition(this) : 0);

		// if extra info is set, we also store title so that the "closed pages" menu
		// can display that information

		if (extra_info)
		{
			sessionwindow->SetStringL("title", GetTitle());
			sessionwindow->GetWidgetImage()->SetWidgetImage(GetWidgetImage());
		}

		INT32 active = 0;
		if(workspace && windowstate != OpWindow::MINIMIZED )
		{
			active = workspace->GetActiveDesktopWindow() == this;
		}

		sessionwindow->SetValueL("active", active);
		sessionwindow->SetValueL("locked", IsLockedByUser() ? 1 : 0);

		PagebarButton *button = GetPagebarButton();
		if (button)
		{
			sessionwindow->SetValueL("group", button->GetGroupNumber());
			sessionwindow->SetValueL("group-active", button->IsActiveTabForGroup());
			sessionwindow->SetValueL("hidden", button->IsHidden());
		}

		if (GetParentGroup())
		{
			sessionwindow->SetValueL("group_position", workspace ? workspace->GetPositionInGroup(this) : -1);
		}

		OnSessionWriteL(session, sessionwindow, shutdown);
	);

	return ret;
}

OP_STATUS DesktopWindow::ListWidgets(OpVector<OpWidget> &widgets) 
{
	if (!GetWidgetContainer())
	{
		OP_ASSERT(!"Missing subclass implementation\n");
		return OpStatus::ERR;
	}

	OpWidget* widget = GetWidgetContainer()->GetRoot(); 
	if (!widget)
		return OpStatus::ERR;

	OpWidget* child = widget->GetFirstChild();
	if (!child)
		return OpStatus::ERR;
	do {
		widgets.Add(child);
		ListWidgets(child, widgets);
	} while (child = child->GetNextSibling());

	return OpStatus::OK;
}

OP_STATUS DesktopWindow::ListWidgets(OpWidget* widget, OpVector<OpWidget> &widgets)
{
	if (!widget)
		return OpStatus::ERR;
	OpWidget* child = widget->GetFirstChild();
	if (!child)
		return OpStatus::ERR;
	do {
		widgets.Add(child);
		ListWidgets(child, widgets);
	} while (child = child->GetNextSibling());

	return OpStatus::OK;
}

